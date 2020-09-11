#include "Renderer.hxx"

#include <utki/math.hpp>

#include <svgdom/elements/coordinate_units.hpp>

#include "util.hxx"
#include "config.hxx"

#include "FilterApplier.hxx"

using namespace svgren;

real Renderer::length_to_px(const svgdom::length& l, unsigned coordIndex) const noexcept{
	if (l.is_percent()) {
		ASSERT(coordIndex < this->viewport.size())
		return this->viewport[coordIndex] * (l.value / 100);
	}
	return l.to_px(this->dpi);
}

void Renderer::applyCairoTransformation(const svgdom::transformable::transformation& t) {
//	TRACE(<< "Renderer::applyCairoTransformation(): applying transformation " << unsigned(t.type) << std::endl)
	switch (t.type_) {
		case svgdom::transformable::transformation::type::translate:
//			TRACE(<< "translate x,y = (" << t.x << ", " << t.y << ")" << std::endl)
			this->canvas.translate(t.x, t.y);
			break;
		case svgdom::transformable::transformation::type::matrix:
			this->canvas.transform({{
					{{t.a, t.c, t.e}},
					{{t.b, t.d, t.f}}
				}});
			break;
		case svgdom::transformable::transformation::type::scale:
//			TRACE(<< "scale transformation factors = (" << t.x << ", " << t.y << ")" << std::endl)
			this->canvas.scale(t.x, t.y);
			break;
		case svgdom::transformable::transformation::type::rotate:
			this->canvas.translate(t.x, t.y);
			this->canvas.rotate(deg_to_rad(t.angle));
			this->canvas.translate(-t.x, -t.y);
			break;
		case svgdom::transformable::transformation::type::skewx:
			{
				using std::tan;
				this->canvas.transform({{
						{{ 1, tan(deg_to_rad(t.angle)), 0 }},
						{{ 0, 1,                        0 }}
					}});
			}
			break;
		case svgdom::transformable::transformation::type::skewy:
			{
				using std::tan;
				this->canvas.transform({{
						{{ 1,                        0, 0 }},
						{{ tan(deg_to_rad(t.angle)), 1, 0}}
					}});
			}
			break;
		default:
			ASSERT(false)
			break;
	}

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	// WORKAROUND: Due to cairo/pixman bug https://bugs.freedesktop.org/show_bug.cgi?id=102966
	//             we have to limit the maximum value of matrix element by 16 bit integer (+-0x7fff).
	using std::min;
	using std::max;
	const double maxValue = double(0x7fff);

	cairo_matrix_t matrix;
	cairo_get_matrix(this->cr, &matrix);
	matrix.xx = max(-maxValue, min(matrix.xx, maxValue));
	matrix.yx = max(-maxValue, min(matrix.yx, maxValue));
	matrix.xy = max(-maxValue, min(matrix.xy, maxValue));
	matrix.yy = max(-maxValue, min(matrix.yy, maxValue));
	matrix.x0 = max(-maxValue, min(matrix.x0, maxValue));
	matrix.y0 = max(-maxValue, min(matrix.y0, maxValue));
	cairo_set_matrix(this->cr, &matrix);
#endif
}

void Renderer::applyTransformations(const decltype(svgdom::transformable::transformations)& transformations) {
	for (auto& t : transformations) {
		this->applyCairoTransformation(t);
	}
}

void Renderer::applyViewBox(const svgdom::view_boxed& e, const svgdom::aspect_ratioed& ar) {
	if (!e.is_view_box_specified()) {
		return;
	}

	if (ar.preserve_aspect_ratio.preserve != svgdom::aspect_ratioed::aspect_ratio_preservation::none) {
		if (e.view_box[3] >= 0 && this->viewport[1] >= 0) { // if viewBox width and viewport width are not 0
			real scaleFactor, dx, dy;

			real viewBoxAspect = e.view_box[2] / e.view_box[3];
			real viewportAspect = this->viewport[0] / this->viewport[1];

			if ((viewBoxAspect >= viewportAspect && ar.preserve_aspect_ratio.slice) || (viewBoxAspect < viewportAspect && !ar.preserve_aspect_ratio.slice)) {
				// fit by Y
				scaleFactor = this->viewport[1] / e.view_box[3];
				dx = e.view_box[2] - this->viewport[0];
				dy = 0;
			} else { // viewBoxAspect < viewportAspect
				// fit by X
				scaleFactor = this->viewport[0] / e.view_box[2];
				dx = 0;
				dy = e.view_box[3] - this->viewport[1];
			}
			switch (ar.preserve_aspect_ratio.preserve) {
				case svgdom::aspect_ratioed::aspect_ratio_preservation::none:
					ASSERT(false)
				default:
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_min_y_max:
					this->canvas.translate(0, dy);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_min_y_mid:
					this->canvas.translate(0, dy / 2);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_min_y_min:
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_mid_y_max:
					this->canvas.translate(dx / 2, dy);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_mid_y_mid:
					this->canvas.translate(dx / 2, dy / 2);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_mid_y_min:
					this->canvas.translate(dx / 2, 0);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_max_y_max:
					this->canvas.translate(dx, dy);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_max_y_mid:
					this->canvas.translate(dx, dy / 2);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_max_y_min:
					this->canvas.translate(dx, 0);
					break;
			}

			this->canvas.scale(scaleFactor, scaleFactor);
		}
	} else { // if no preserveAspectRatio enforced
		if (e.view_box[2] != 0 && e.view_box[3] != 0) { // if viewBox width and height are not 0
			this->canvas.scale(
					this->viewport[0] / e.view_box[2],
					this->viewport[1] / e.view_box[3]
				);
		}
	}
	this->canvas.translate(-e.view_box[0], -e.view_box[1]);
}

void Renderer::setCairoPatternSource(cairo_pattern_t& pat, const svgdom::gradient& g, const svgdom::style_stack& ss){
	// Gradient inherits all attributes from other gradients it refers via href.
	// Here we need to make sure that the gradient inherits 'styles', 'class' and all possible presentation attributes.
	// For that we need to replace the gradient element in the style stack with the one which has those inherited
	// attributes substituted. The currently handled gradient element is at the top of the style stack.

	svgdom::style_stack gradient_ss(ss); // copy style stack, so that we are able to modify it

	struct dummy_styleable : public svgdom::styleable{
		const svgdom::gradient& g;

		dummy_styleable(const svgdom::gradient& g) : g(g) {}

		const std::string& get_id()const override{
			return this->g.get_id();
		}

		const std::string& get_tag()const override{
			return this->g.get_tag();
		}
	} effective_gradient_styleable(g);

	effective_gradient_styleable.styles = this->gradient_get_styles(g);
	effective_gradient_styleable.classes = this->gradient_get_classes(g);
	effective_gradient_styleable.presentation_attributes = this->gradient_get_presentation_attributes(g);

	ASSERT(!gradient_ss.stack.empty())
	gradient_ss.stack.pop_back();
	gradient_ss.stack.push_back(effective_gradient_styleable);

	struct ColorStopAdder : public svgdom::const_visitor{
		cairo_pattern_t& pat;
		svgdom::style_stack& ss;

		ColorStopAdder(cairo_pattern_t& pat, svgdom::style_stack& ss) : pat(pat), ss(ss) {}

		void visit(const svgdom::gradient::stop_element& s)override{
			svgdom::style_stack::push stylePush(this->ss, s);

			svgdom::rgb rgb;
			if(auto p = this->ss.get_style_property(svgdom::style_property::stop_color)){
				rgb = p->get_rgb();
			}else{
				rgb.r = 0;
				rgb.g = 0;
				rgb.b = 0;
			}

			svgdom::real opacity;
			if(auto p = this->ss.get_style_property(svgdom::style_property::stop_opacity)){
				opacity = p->opacity;
			}else{
				opacity = 1;
			}
			cairo_pattern_add_color_stop_rgba(&this->pat, s.offset, rgb.r, rgb.g, rgb.b, opacity);
			ASSERT(cairo_pattern_status(&this->pat) == CAIRO_STATUS_SUCCESS)
		}
	} visitor(pat, gradient_ss);

	for(auto& stop : this->gradientGetStops(g)){
		stop->accept(visitor);
	}

	switch(this->gradientGetSpreadMethod(g)){
		default:
		case svgdom::gradient::spread_method::default_:
			ASSERT(false)
			break;
		case svgdom::gradient::spread_method::pad:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_PAD);
			ASSERT(cairo_pattern_status(&pat) == CAIRO_STATUS_SUCCESS)
			break;
		case svgdom::gradient::spread_method::reflect:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_REFLECT);
			ASSERT(cairo_pattern_status(&pat) == CAIRO_STATUS_SUCCESS)
			break;
		case svgdom::gradient::spread_method::repeat:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_REPEAT);
			ASSERT(cairo_pattern_status(&pat) == CAIRO_STATUS_SUCCESS)
			break;
	}
	cairo_set_source(this->cr, &pat);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
}

void Renderer::applyFilter() {
	if(auto filter = this->styleStack.get_style_property(svgdom::style_property::filter)){
		this->applyFilter(filter->get_local_id_from_iri());
	}
}

void Renderer::applyFilter(const std::string& id) {
	auto f = this->finder.find_by_id(id);
	if(!f){
		return;
	}

	FilterApplier visitor(*this);

	ASSERT(f)
	f->e.accept(visitor);

	this->blit(visitor.getLastResult());
}

void Renderer::setGradient(const std::string& id) {
	auto g = this->finder.find_by_id(id);
	if(!g){
		cairo_set_source_rgba(this->cr, 0, 0, 0, 0);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		return;
	}

	struct CommonGradientPush{
		CairoMatrixSaveRestore cairoMatrixPush; // here we need to save/restore only matrix!

		std::unique_ptr<ViewportPush> viewportPush;

		CommonGradientPush(Renderer& r, const svgdom::gradient& gradient) :
				cairoMatrixPush(r.cr)
		{
			if (r.gradientGetUnits(gradient) == svgdom::coordinate_units::object_bounding_box) {
				r.canvas.translate(r.userSpaceShapeBoundingBoxPos[0], r.userSpaceShapeBoundingBoxPos[1]);
				r.canvas.scale(r.userSpaceShapeBoundingBoxDim[0], r.userSpaceShapeBoundingBoxDim[1]);
				this->viewportPush = std::unique_ptr<ViewportPush>(new ViewportPush(r, {{1, 1}}));
			}

			r.applyTransformations(r.gradientGetTransformations(gradient));
		}

		~CommonGradientPush()noexcept{}
	};

	struct GradientSetter : public svgdom::const_visitor{
		Renderer& r;

		const svgdom::style_stack& ss;

		GradientSetter(Renderer& r, const svgdom::style_stack& ss) : r(r), ss(ss) {}

		void visit(const svgdom::linear_gradient_element& gradient)override{
			CommonGradientPush commonPush(this->r, gradient); // TODO: move out of visitor?

			if(auto pat = cairo_pattern_create_linear(
					this->r.length_to_px(this->r.gradientGetX1(gradient), 0),
					this->r.length_to_px(this->r.gradientGetY1(gradient), 1),
					this->r.length_to_px(this->r.gradientGetX2(gradient), 0),
					this->r.length_to_px(this->r.gradientGetY2(gradient), 1)
				))
			{
				utki::scope_exit pat_scope_exit([&pat]() {cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}

		void visit(const svgdom::radial_gradient_element& gradient)override{
			CommonGradientPush commonPush(this->r, gradient); // TODO: move out of visitor?

			auto cx = this->r.gradientGetCx(gradient);
			auto cy = this->r.gradientGetCy(gradient);
			auto radius = this->r.gradientGetR(gradient);
			auto fx = this->r.gradientGetFx(gradient);
			auto fy = this->r.gradientGetFy(gradient);

			if(!fx.is_valid()){
				fx = cx;
			}
			if(!fy.is_valid()){
				fy = cy;
			}

			if(auto pat = cairo_pattern_create_radial(
					this->r.length_to_px(fx, 0),
					this->r.length_to_px(fy, 1),
					0,
					this->r.length_to_px(cx, 0),
					this->r.length_to_px(cy, 1),
					this->r.length_to_px(radius)
				))
			{
				utki::scope_exit pat_scope_exit([&pat]() {cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}

		void default_visit(const svgdom::element&)override{
			cairo_set_source_rgba(this->r.cr, 0, 0, 0, 0);
			ASSERT(cairo_status(this->r.cr) == CAIRO_STATUS_SUCCESS)
		}
	} visitor(*this, g->ss);

	g->e.accept(visitor);
}

void Renderer::updateCurBoundingBox() {
	double x1, y1, x2, y2;

	// According to SVG spec https://www.w3.org/TR/SVG/coords.html#ObjectBoundingBox
	// "The bounding box is computed exclusive of any values for clipping, masking, filter effects, opacity and stroke-width"
	cairo_path_extents(
			this->cr,
			&x1,
			&y1,
			&x2,
			&y2
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->userSpaceShapeBoundingBoxPos[0] = svgren::real(x1);
	this->userSpaceShapeBoundingBoxPos[1] = svgren::real(y1);
	this->userSpaceShapeBoundingBoxDim[0] = svgren::real(x2 - x1);
	this->userSpaceShapeBoundingBoxDim[1] = svgren::real(y2 - y1);

	if(this->userSpaceShapeBoundingBoxDim[0] == 0){
		// empty path
		return;
	}

	// set device space bounding box
	std::array<std::array<double, 2>, 4> rectVertices = {{
		{{x1 ,y1}},
		{{x2, y2}},
		{{x1, y2}},
		{{x2, y1}}
	}};

	for(auto& vertex : rectVertices){
		cairo_user_to_device(this->cr, &vertex[0], &vertex[1]);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		DeviceSpaceBoundingBox bb;
		bb.left = decltype(bb.left)(vertex[0]);
		bb.right = decltype(bb.right)(vertex[0]);
		bb.top = decltype(bb.top)(vertex[1]);
		bb.bottom = decltype(bb.bottom)(vertex[1]);

		this->deviceSpaceBoundingBox.merge(bb);
	}
}

void Renderer::renderCurrentShape(bool isCairoGroupPushed) {
	this->updateCurBoundingBox();

	if (auto p = this->styleStack.get_style_property(svgdom::style_property::fill_rule)) {
		switch (p->fill_rule) {
			default:
				ASSERT(false)
				break;
			case svgdom::fill_rule::evenodd:
				cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_EVEN_ODD);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::fill_rule::nonzero:
				cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_WINDING);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
		}
	} else {
		cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_WINDING);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	svgdom::style_value blackFill;

	auto fill = this->styleStack.get_style_property(svgdom::style_property::fill);
	if (!fill) {
		blackFill = svgdom::style_value::parse_paint("black");
		fill = &blackFill;
	}

	auto stroke = this->styleStack.get_style_property(svgdom::style_property::stroke);

	// OPTIMIZATION: in case there is 'opacity' style property and only one of
	//               'stroke' or 'fill' is not none and is a solid color (not pattern/gradient),
	//               then there is no need to push cairo group, but just multiply
	//               the 'stroke-opacity' or 'fill-opacity' by 'opacity' value.
	auto opacity = svgdom::real(1);
	if(!isCairoGroupPushed){
		if(auto p = this->styleStack.get_style_property(svgdom::style_property::opacity)){
			opacity = p->opacity;
		}
	}

	ASSERT(fill)
	if (!fill->is_none()) {
		if (fill->is_url()) {
			this->setGradient(fill->get_local_id_from_iri());
		} else {
			svgdom::real fillOpacity = 1;
			if (auto p = this->styleStack.get_style_property(svgdom::style_property::fill_opacity)){
				fillOpacity = p->opacity;
			}

			auto fillRgb = fill->get_rgb();
			cairo_set_source_rgba(this->cr, fillRgb.r, fillRgb.g, fillRgb.b, fillOpacity * opacity);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_fill_preserve(this->cr);
		ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
	}

	if (stroke && !stroke->is_none()) {
		if (auto p = this->styleStack.get_style_property(svgdom::style_property::stroke_width)){
			cairo_set_line_width(this->cr, this->length_to_px(p->stroke_width));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		} else {
			cairo_set_line_width(this->cr, 1);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		if (auto p = this->styleStack.get_style_property(svgdom::style_property::stroke_linecap)) {
			switch(p->stroke_line_cap){
				default:
					ASSERT(false)
					break;
				case svgdom::stroke_line_cap::butt:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_BUTT);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::stroke_line_cap::round:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_ROUND);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::stroke_line_cap::square:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_SQUARE);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
			}
		}else{
			cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_BUTT);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		if(auto p = this->styleStack.get_style_property(svgdom::style_property::stroke_linejoin)){
			switch(p->stroke_line_join){
				default:
					ASSERT(false)
					break;
				case svgdom::stroke_line_join::miter:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_MITER);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::stroke_line_join::round:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_ROUND);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::stroke_line_join::bevel:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_BEVEL);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
			}
		}else{
			cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_MITER);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		if(stroke->is_url()){
			this->setGradient(stroke->get_local_id_from_iri());
		}else{
			svgdom::real strokeOpacity = 1;
			if(auto p = this->styleStack.get_style_property(svgdom::style_property::stroke_opacity)){
				strokeOpacity = p->opacity;
			}

			auto rgb = stroke->get_rgb();
			cairo_set_source_rgba(this->cr, rgb.r, rgb.g, rgb.b, strokeOpacity * opacity);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_stroke_preserve(this->cr);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	// clear path if any left
	cairo_new_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->applyFilter();
}

void Renderer::renderSvgElement(
		const svgdom::container& e,
		const svgdom::styleable& s,
		const svgdom::view_boxed& v,
		const svgdom::aspect_ratioed& a,
		const svgdom::length& x,
		const svgdom::length& y,
		const svgdom::length& width,
		const svgdom::length& height
	)
{
	svgdom::style_stack::push pushStyles(this->styleStack, s);

	if(this->isGroupInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoTempContext(*this, true);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	if (this->isOutermostElement) {
		this->canvas.translate(
				this->length_to_px(x, 0),
				this->length_to_px(y, 1)
			);
	}

	ViewportPush viewportPush(
			*this,
			{{
				this->length_to_px(width, 0),
				this->length_to_px(height, 1)
			}}
		);

	this->applyViewBox(v, a);

	{
		bool oldOutermostElementFlag = this->isOutermostElement;
		this->isOutermostElement = false;
		utki::scope_exit scope_exit([oldOutermostElementFlag, this](){
			this->isOutermostElement = oldOutermostElementFlag;
		});

		this->relay_accept(e);
	}

	this->applyFilter();
}

Renderer::Renderer(
		svgren::canvas& canvas,
		unsigned dpi,
		std::array<real, 2> canvasSize,
		const svgdom::svg_element& root
	) :
		canvas(canvas),
		cr(canvas.cr),
		finder(root),
		dpi(real(dpi)),
		viewport(canvasSize)
{
	this->deviceSpaceBoundingBox.setEmpty();
	this->background = getSubSurface(this->cr);
}

void Renderer::visit(const svgdom::g_element& e){
//	TRACE(<< "rendering GElement: id = " << e.id << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isGroupInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoTempContext(*this, true);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	this->relayAccept(e);

	this->applyFilter();
}

void Renderer::visit(const svgdom::use_element& e){
//	TRACE(<< "rendering UseElement" << std::endl)
	auto ref = this->finder.find_by_id(e.get_local_id_from_iri());
	if(!ref){
		return;
	}

	struct RefRenderer : public svgdom::const_visitor{
		Renderer& r;
		const svgdom::use_element& ue;
		svgdom::g_element fakeGElement;

		RefRenderer(Renderer& r, const svgdom::use_element& e) :
				r(r), ue(e)
		{
			this->fakeGElement.styles = e.styles;
			this->fakeGElement.presentation_attributes = e.presentation_attributes;
			this->fakeGElement.transformations = e.transformations;

			// add x and y transformation
			{
				svgdom::transformable::transformation t;
				t.type_ = svgdom::transformable::transformation::type::translate;
				t.x = this->r.length_to_px(e.x, 0);
				t.y = this->r.length_to_px(e.y, 1);

				this->fakeGElement.transformations.push_back(t);
			}
		}

		void visit(const svgdom::symbol_element& symbol)override{
			struct FakeSvgElement : public svgdom::element{
				Renderer& r;
				const svgdom::use_element& ue;
				const svgdom::symbol_element& se;

				FakeSvgElement(Renderer& r, const svgdom::use_element& ue, const svgdom::symbol_element& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					const auto hundredPercent = svgdom::length(100, svgdom::length_unit::percent);

					this->r.renderSvgElement(
							this->se,
							this->se,
							this->se,
							this->se,
							svgdom::length(0),
							svgdom::length(0),
							this->ue.width.is_valid() ? this->ue.width : hundredPercent,
							this->ue.height.is_valid() ? this->ue.height : hundredPercent
						);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, this->ue, symbol));
			this->fakeGElement.accept(this->r);
		}

		void visit(const svgdom::svg_element& svg)override{
			struct FakeSvgElement : public svgdom::element{
				Renderer& r;
				const svgdom::use_element& ue;
				const svgdom::svg_element& se;

				FakeSvgElement(Renderer& r, const svgdom::use_element& ue, const svgdom::svg_element& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					// width and height of <use> element override those of <svg> element.
					this->r.renderSvgElement(
							this->se,
							this->se,
							this->se,
							this->se,
							this->se.x,
							this->se.y,
							this->ue.width.is_valid() ? this->ue.width : this->se.width,
							this->ue.height.is_valid() ? this->ue.height : this->se.height
						);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, this->ue, svg));
			this->fakeGElement.accept(this->r);
		}

		void default_visit(const svgdom::element& element)override{
			struct FakeSvgElement : public svgdom::element{
				Renderer& r;
				const svgdom::element& e;

				FakeSvgElement(Renderer& r, const svgdom::element& e) :
						r(r), e(e)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					this->e.accept(this->r);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, element));
			this->fakeGElement.accept(this->r);
		}

		void default_visit(const svgdom::element& element, const svgdom::container& c)override{
			this->default_visit(element);
		}
	} visitor(*this, e);

	ASSERT(ref)

	ref->e.accept(visitor);
}

void Renderer::visit(const svgdom::svg_element& e) {
//	TRACE(<< "rendering SvgElement" << std::endl)
	renderSvgElement(e, e, e, e, e.x, e.y, e.width, e.height);
}

bool Renderer::isInvisible() {
	if(auto p = this->styleStack.get_style_property(svgdom::style_property::visibility)){
		if(p->visibility != svgdom::visibility::visible){
			return true;
		}
	}
	return this->isGroupInvisible();
}

bool Renderer::isGroupInvisible() {
	if(auto p = this->styleStack.get_style_property(svgdom::style_property::display)){
		if(p->display == svgdom::display::none){
			return true;
		}
	}
	return false;
}

void Renderer::visit(const svgdom::path_element& e) {
//	TRACE(<< "rendering PathElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	double prevQuadraticX1 = 0;
	double prevQuadraticY1 = 0;

	const svgdom::path_element::step* prevStep = nullptr;

	for (auto& s : e.path) {
		switch (s.type_) {
			case svgdom::path_element::step::type::move_abs:
				cairo_move_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::move_rel:
				if (!cairo_has_current_point(this->cr)) {
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					cairo_move_to(this->cr, 0, 0);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				cairo_rel_move_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::line_abs:
				cairo_line_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::line_rel:
				cairo_rel_line_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::horizontal_line_abs:
				{
					double x, y;
					if (cairo_has_current_point(this->cr)) {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x, &y);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						y = 0;
					}
					cairo_line_to(this->cr, s.x, y);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}
				break;
			case svgdom::path_element::step::type::horizontal_line_rel:
				cairo_rel_line_to(this->cr, s.x, 0);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::vertical_line_abs:
				{
					double x, y;
					if (cairo_has_current_point(this->cr)) {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x, &y);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x = 0;
					}
					cairo_line_to(this->cr, x, s.y);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}
				break;
			case svgdom::path_element::step::type::vertical_line_rel:
				cairo_rel_line_to(this->cr, 0, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::close:
				cairo_close_path(this->cr);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::quadratic_abs:
				cairoQuadraticCurveTo(this->cr, s.x1, s.y1, s.x, s.y);
				break;
			case svgdom::path_element::step::type::quadratic_rel:
				cairoRelQuadraticCurveTo(this->cr, s.x1, s.y1, s.x, s.y);
				break;
			case svgdom::path_element::step::type::quadratic_smooth_abs:
				{
					double x0, y0; // current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x0, &y0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_move_to(this->cr, 0, 0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; // control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::quadratic_abs:
							x1 = -(prevStep->x1 - x0) + x0;
							y1 = -(prevStep->y1 - y0) + y0;
							break;
						case svgdom::path_element::step::type::quadratic_smooth_abs:
							x1 = -(prevQuadraticX1 - x0) + x0;
							y1 = -(prevQuadraticY1 - y0) + y0;
							break;
						case svgdom::path_element::step::type::quadratic_rel:
							x1 = -(prevStep->x1 - prevStep->x) + x0;
							y1 = -(prevStep->y1 - prevStep->y) + y0;
							break;
						case svgdom::path_element::step::type::quadratic_smooth_rel:
							x1 = -(prevQuadraticX1 - prevStep->x) + x0;
							y1 = -(prevQuadraticY1 - prevStep->y) + y0;
							break;
						default:
							// No previous step or previous step is not a quadratic Bezier curve.
							// Set first control point equal to current point
							x1 = x0;
							y1 = y0;
							break;
					}
					prevQuadraticX1 = x1;
					prevQuadraticY1 = y1;
					cairoQuadraticCurveTo(this->cr, x1, y1, s.x, s.y);
				}
				break;
			case svgdom::path_element::step::type::quadratic_smooth_rel:
				{
					double x0, y0; // current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x0, &y0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_move_to(this->cr, 0, 0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; // control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::quadratic_smooth_abs:
							x1 = -(prevQuadraticX1 - x0);
							y1 = -(prevQuadraticY1 - y0);
							break;
						case svgdom::path_element::step::type::quadratic_abs:
							x1 = -(prevStep->x1 - x0);
							y1 = -(prevStep->y1 - y0);
							break;
						case svgdom::path_element::step::type::quadratic_smooth_rel:
							x1 = -(prevQuadraticX1 - prevStep->x);
							y1 = -(prevQuadraticY1 - prevStep->y);
							break;
						case svgdom::path_element::step::type::quadratic_rel:
							x1 = -(prevStep->x1 - prevStep->x);
							y1 = -(prevStep->y1 - prevStep->y);
							break;
						default:
							// No previous step or previous step is not a quadratic Bezier curve.
							// Set first control point equal to current point, i.e. 0 because this is Relative step.
							x1 = 0;
							y1 = 0;
							break;
					}
					prevQuadraticX1 = x1;
					prevQuadraticY1 = y1;
					cairoRelQuadraticCurveTo(this->cr, x1, y1, s.x, s.y);
				}
				break;
			case svgdom::path_element::step::type::cubic_abs:
				cairo_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::cubic_rel:
				cairo_rel_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::path_element::step::type::cubic_smooth_abs:
				{
					double x0, y0; // current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x0, &y0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_move_to(this->cr, 0, 0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; // first control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::cubic_smooth_abs:
						case svgdom::path_element::step::type::cubic_abs:
							x1 = -(prevStep->x2 - x0) + x0;
							y1 = -(prevStep->y2 - y0) + y0;
							break;
						case svgdom::path_element::step::type::cubic_smooth_rel:
						case svgdom::path_element::step::type::cubic_rel:
							x1 = -(prevStep->x2 - prevStep->x) + x0;
							y1 = -(prevStep->y2 - prevStep->y) + y0;
							break;
						default:
							// No previous step or previous step is not a cubic Bezier curve.
							// Set first control point equal to current point
							x1 = x0;
							y1 = y0;
							break;
					}
					cairo_curve_to(this->cr, x1, y1, s.x2, s.y2, s.x, s.y);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}
				break;
			case svgdom::path_element::step::type::cubic_smooth_rel:
				{
					double x0, y0; // current point, absolute coordinates
					if(cairo_has_current_point(this->cr)){
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x0, &y0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					}else{
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_move_to(this->cr, 0, 0);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; // first control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::cubic_smooth_abs:
						case svgdom::path_element::step::type::cubic_abs:
							x1 = -(prevStep->x2 - x0);
							y1 = -(prevStep->y2 - y0);
							break;
						case svgdom::path_element::step::type::cubic_smooth_rel:
						case svgdom::path_element::step::type::cubic_rel:
							x1 = -(prevStep->x2 - prevStep->x);
							y1 = -(prevStep->y2 - prevStep->y);
							break;
						default:
							// No previous step or previous step is not a cubic Bezier curve.
							// Set first control point equal to current point, i.e. 0 because this is Relative step.
							x1 = 0;
							y1 = 0;
							break;
					}
					cairo_rel_curve_to(this->cr, x1, y1, s.x2, s.y2, s.x, s.y);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}
				break;
			case svgdom::path_element::step::type::arc_abs:
			case svgdom::path_element::step::type::arc_rel:
				{
					real x, y;
					if (cairo_has_current_point(this->cr)) {
						double xx, yy;
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &xx, &yy);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x = real(xx);
						y = real(yy);
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x = 0;
						y = 0;
					}

					if (s.rx <= 0) {
						break;
					}
					ASSERT(s.rx > 0)
					real radiiRatio = s.ry / s.rx;

					if (radiiRatio <= 0) {
						break;
					}

					// cancel rotation of end point
					real xe, ye;
					{
						real xx;
						real yy;
						if(s.type_ == svgdom::path_element::step::type::arc_abs){
							xx = s.x - x;
							yy = s.y - y;
						}else{
							xx = s.x;
							yy = s.y;
						}

						auto res = rotate_vector(xx, yy, deg_to_rad(-real(s.x_axis_rotation)));
						xe = res[0];
						ye = res[1];
					}
					ASSERT(radiiRatio > 0)
					ye /= radiiRatio;

					// find the angle between the end point and the x axis
					auto angle = pointAngle(real(0), real(0), xe, ye);

					using std::sqrt;

					// put the end point onto the x axis
					xe = sqrt(xe * xe + ye * ye);
					ye = 0;

					using std::max;

					// update the x radius if it is too small
					auto rx = max(s.rx, xe / 2);

					// find one circle center
					auto xc = xe / 2;
					auto yc = sqrt(rx * rx - xc * xc);

					// choose between the two circles according to flags
					if (!(s.flags.large_arc ^ s.flags.sweep)) {
						yc = -yc;
					}

					// put the second point and the center back to their positions
					{
						auto res = rotate_vector(xe, real(0), angle);
						xe = res[0];
						ye = res[1];
					}
					{
						auto res = rotate_vector(xc, yc, angle);
						xc = res[0];
						yc = res[1];
					}

					auto angle1 = pointAngle(xc, yc, real(0), real(0));
					auto angle2 = pointAngle(xc, yc, xe, ye);

					CairoContextSaveRestore cairoMatrixPush1(this->cr);

					this->canvas.translate(x, y);
					this->canvas.rotate(deg_to_rad(s.x_axis_rotation));
					this->canvas.scale(1, radiiRatio);
					if (s.flags.sweep) {
						cairo_arc(this->cr, xc, yc, rx, angle1, angle2);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						cairo_arc_negative(this->cr, xc, yc, rx, angle1, angle2);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					}
				}
				break;
			default:
				ASSERT_INFO(false, "unknown path step type: " << unsigned(s.type_))
				break;
		}
		prevStep = &s;
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::circle_element& e){
//	TRACE(<< "rendering CircleElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	cairo_arc(
			this->cr,
			this->length_to_px(e.cx, 0),
			this->length_to_px(e.cy, 1),
			this->length_to_px(e.r),
			0,
			2 * utki::pi<double>()
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::polyline_element& e){
//	TRACE(<< "rendering PolylineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	// TODO: make a common push class for shapes

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	if (e.points.size() == 0) {
		return;
	}

	auto i = e.points.begin();
	cairo_move_to(this->cr, (*i)[0], (*i)[1]);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	++i;

	for (; i != e.points.end(); ++i) {
		cairo_line_to(this->cr, (*i)[0], (*i)[1]);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::polygon_element& e){
//	TRACE(<< "rendering PolygonElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	if (e.points.size() == 0) {
		return;
	}

	auto i = e.points.begin();
	cairo_move_to(this->cr, (*i)[0], (*i)[1]);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	++i;

	for (; i != e.points.end(); ++i) {
		cairo_line_to(this->cr, (*i)[0], (*i)[1]);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	cairo_close_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::line_element& e){
//	TRACE(<< "rendering LineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	cairo_move_to(this->cr, this->length_to_px(e.x1, 0), this->length_to_px(e.y1, 1));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	cairo_line_to(this->cr, this->length_to_px(e.x2, 0), this->length_to_px(e.y2, 1));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::ellipse_element& e){
//	TRACE(<< "rendering EllipseElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	{
		CairoContextSaveRestore saveRestore(this->cr);

		this->canvas.translate(
				this->length_to_px(e.cx, 0),
				this->length_to_px(e.cy, 1)
			);

		this->canvas.scale(
				this->length_to_px(e.rx, 0),
				this->length_to_px(e.ry, 1)
			);
		cairo_arc(this->cr, 0, 0, 1, 0, 2 * utki::pi<double>());
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_close_path(this->cr);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::style_element& e){
	this->styleStack.add_css(e.css);
}

void Renderer::visit(const svgdom::rect_element& e){
//	TRACE(<< "rendering RectElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	auto width = this->length_to_px(e.width, 0);
	auto height = this->length_to_px(e.height, 1);

	// NOTE: see SVG sect: https://www.w3.org/TR/SVG/shapes.html#RectElementWidthAttribute
	//       Zero values disable rendering of the element.
	if(width == real(0) || height == real(0)){
		return;
	}

	if ((e.rx.value == 0 || !e.rx.is_valid())
			&& (e.ry.value == 0 || !e.ry.is_valid())) {
		cairo_rectangle(this->cr, this->length_to_px(e.x, 0), this->length_to_px(e.y, 1), width, height);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	} else {
		// compute real rx and ry
		auto rx = e.rx;
		auto ry = e.ry;

		if(!ry.is_valid() && rx.is_valid()){
			ry = rx;
		} else if (!rx.is_valid() && ry.is_valid()){
			rx = ry;
		}
		ASSERT(rx.is_valid() && ry.is_valid())

		if(this->length_to_px(rx, 0) > width / 2){
			rx = e.width;
			rx.value /= 2;
		}
		if(this->length_to_px(ry, 1) > height / 2){
			ry = e.height;
			ry.value /= 2;
		}

		cairo_move_to(this->cr, this->length_to_px(e.x, 0) + this->length_to_px(rx, 0), this->length_to_px(e.y, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_line_to(this->cr, this->length_to_px(e.x, 0) + width - this->length_to_px(rx, 0), this->length_to_px(e.y, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(
					this->length_to_px(e.x, 0) + width - this->length_to_px(rx, 0),
					this->length_to_px(e.y, 1) + this->length_to_px(ry, 1)
				);
			this->canvas.scale(
					this->length_to_px(rx, 0),
					this->length_to_px(ry, 1)
				);
			cairo_arc(this->cr, 0, 0, 1, -utki::pi<double>() / 2, 0);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_line_to(this->cr, this->length_to_px(e.x, 0) + width, this->length_to_px(e.y, 1) + height - this->length_to_px(ry, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(
					this->length_to_px(e.x, 0) + width - this->length_to_px(rx, 0),
					this->length_to_px(e.y, 1) + height - this->length_to_px(ry, 1)
				);
			this->canvas.scale(
					this->length_to_px(rx, 0),
					this->length_to_px(ry, 1)
				);
			cairo_arc(this->cr, 0, 0, 1, 0, utki::pi<double>() / 2);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_line_to(this->cr, this->length_to_px(e.x, 0) + this->length_to_px(rx, 0), this->length_to_px(e.y, 1) + height);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(
					this->length_to_px(e.x, 0) + this->length_to_px(rx, 0),
					this->length_to_px(e.y, 1) + height - this->length_to_px(ry, 1)
				);
			this->canvas.scale(
					this->length_to_px(rx, 0),
					this->length_to_px(ry, 1)
				);
			cairo_arc(this->cr, 0, 0, 1, utki::pi<double>() / 2, utki::pi<double>());
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_line_to(this->cr, this->length_to_px(e.x, 0), this->length_to_px(e.y, 1) + this->length_to_px(ry, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(
					this->length_to_px(e.x, 0) + this->length_to_px(rx, 0),
					this->length_to_px(e.y, 1) + this->length_to_px(ry, 1)
				);
			this->canvas.scale(
					this->length_to_px(rx, 0),
					this->length_to_px(ry, 1)
				);
			cairo_arc(this->cr, 0, 0, 1, utki::pi<double>(), utki::pi<double>() * 3 / 2);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_close_path(this->cr);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

namespace{
struct GradientCaster : public svgdom::const_visitor{
	const svgdom::linear_gradient_element* linear = nullptr;
	const svgdom::radial_gradient_element* radial = nullptr;
	const svgdom::gradient* gradient = nullptr;

	void visit(const svgdom::linear_gradient_element& e)override{
		this->gradient = &e;
		this->linear = &e;
	}

	void visit(const svgdom::radial_gradient_element& e)override{
		this->gradient = &e;
		this->radial = &e;
	}
};
}

const decltype(svgdom::transformable::transformations)& Renderer::gradientGetTransformations(const svgdom::gradient& g){
	if(g.transformations.size() != 0){
		return g.transformations;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetTransformations(*caster.gradient);
			}
		}
	}
	return g.transformations;
}

svgdom::coordinate_units Renderer::gradientGetUnits(const svgdom::gradient& g){
	if(g.units != svgdom::coordinate_units::unknown){
		return g.units;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetUnits(*caster.gradient);
			}
		}
	}
	return svgdom::coordinate_units::object_bounding_box; // bounding box is default
}

svgdom::length Renderer::gradientGetX1(const svgdom::linear_gradient_element& g){
	if(g.x1.is_valid()){
		return g.x1;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetX1(*caster.linear);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetY1(const svgdom::linear_gradient_element& g){
	if(g.y1.is_valid()){
		return g.y1;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetY1(*caster.linear);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetX2(const svgdom::linear_gradient_element& g) {
	if(g.x2.is_valid()){
		return g.x2;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetX2(*caster.linear);
			}
		}
	}
	return svgdom::length(100, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetY2(const svgdom::linear_gradient_element& g) {
	if(g.y2.is_valid()){
		return g.y2;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetY2(*caster.linear);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetCx(const svgdom::radial_gradient_element& g) {
	if(g.cx.is_valid()){
		return g.cx;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetCx(*caster.radial);
			}
		}
	}
	return svgdom::length(50, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetCy(const svgdom::radial_gradient_element& g) {
	if(g.cy.is_valid()){
		return g.cy;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetCy(*caster.radial);
			}
		}
	}
	return svgdom::length(50, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetR(const svgdom::radial_gradient_element& g) {
	if(g.r.is_valid()){
		return g.r;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetR(*caster.radial);
			}
		}
	}
	return svgdom::length(50, svgdom::length_unit::percent);
}

svgdom::length Renderer::gradientGetFx(const svgdom::radial_gradient_element& g) {
	if(g.fx.is_valid()){
		return g.fx;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetFx(*caster.radial);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::unknown);
}

svgdom::length Renderer::gradientGetFy(const svgdom::radial_gradient_element& g) {
	if(g.fy.is_valid()){
		return g.fy;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetFy(*caster.radial);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::unknown);
}

const decltype(svgdom::container::children)& Renderer::gradientGetStops(const svgdom::gradient& g) {
	if(g.children.size() != 0){
		return g.children;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetStops(*caster.gradient);
			}
		}
	}

	return g.children;
}

const decltype(svgdom::styleable::styles)& Renderer::gradient_get_styles(const svgdom::gradient& g){
	if(g.styles.size() != 0){
		return g.styles;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradient_get_styles(*caster.gradient);
			}
		}
	}

	return g.styles;
}

const decltype(svgdom::styleable::classes)& Renderer::gradient_get_classes(const svgdom::gradient& g){
	if(!g.classes.empty()){
		return g.classes;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradient_get_classes(*caster.gradient);
			}
		}
	}

	return g.classes;
}

svgdom::gradient::spread_method Renderer::gradientGetSpreadMethod(const svgdom::gradient& g){
	if(g.spread_method_ != svgdom::gradient::spread_method::default_){
		return g.spread_method_;
	}

	ASSERT(g.spread_method_ == svgdom::gradient::spread_method::default_)

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetSpreadMethod(*caster.gradient);
			}
		}
	}

	return svgdom::gradient::spread_method::pad;
}

decltype(svgdom::styleable::presentation_attributes) Renderer::gradient_get_presentation_attributes(const svgdom::gradient& g){
	decltype(svgdom::styleable::presentation_attributes) ret = g.presentation_attributes; // copy

	decltype(svgdom::styleable::presentation_attributes) ref_attrs;

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				ref_attrs = this->gradient_get_presentation_attributes(*caster.gradient);
			}
		}
	}

	for(auto& ra : ref_attrs){
		if(ret.find(ra.first) == ret.end()){
			ret.insert(std::make_pair(ra.first, std::move(ra.second)));
		}
	}

	return ret;
}

void Renderer::blit(const Surface& s) {
	if(!s.data || s.width == 0 || s.height == 0){
		TRACE(<< "Renderer::blit(): source image is empty" << std::endl)
		return;
	}
	ASSERT(s.data && s.width != 0 && s.height != 0)

	auto dst = getSubSurface(this->cr);

	if(s.x >= dst.width || s.y >= dst.height){
		TRACE(<< "Renderer::blit(): source image is out of canvas" << std::endl)
		return;
	}

	auto dstp = reinterpret_cast<std::uint32_t*>(dst.data) + s.y * dst.stride + s.x;
	auto srcp = reinterpret_cast<std::uint32_t*>(s.data);
	unsigned dx = std::min(s.width, dst.width - s.x);
	unsigned dy = std::min(s.height, dst.height - s.y);

	for(unsigned y = 0; y != dy; ++y){
		auto p = dstp + y * dst.stride;
		auto sp = srcp + y * s.stride;
		for(unsigned x = 0; x != dx; ++x, ++p, ++sp){
			*p = *sp;
		}
	}
}
