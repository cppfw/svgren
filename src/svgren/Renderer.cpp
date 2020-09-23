#include "Renderer.hxx"

#include <utki/math.hpp>

#include <svgdom/elements/coordinate_units.hpp>

#include "util.hxx"
#include "config.hxx"

#include "FilterApplier.hxx"

using namespace svgren;

real Renderer::length_to_px(const svgdom::length& l, unsigned coordIndex)const noexcept{
	if(l.is_percent()){
		ASSERT(coordIndex < this->viewport.size())
		return this->viewport[coordIndex] * (l.value / 100);
	}
	return real(l.to_px(this->dpi));
}

r4::vector2<real> Renderer::length_to_px(const svgdom::length& x, const svgdom::length& y)const noexcept{
	return r4::vector2<real>{
			this->length_to_px(x, 0),
			this->length_to_px(y, 1)
		};
}

void Renderer::apply_transformation(const svgdom::transformable::transformation& t){
//	TRACE(<< "Renderer::applyCairoTransformation(): applying transformation " << unsigned(t.type) << std::endl)
	switch (t.type_) {
		case svgdom::transformable::transformation::type::translate:
//			TRACE(<< "translate x,y = (" << t.x << ", " << t.y << ")" << std::endl)
			this->canvas.translate(t.x, t.y);
			break;
		case svgdom::transformable::transformation::type::matrix:
			this->canvas.transform({
					{t.a, t.c, t.e},
					{t.b, t.d, t.f}
				});
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
				this->canvas.transform({
						{ 1, tan(deg_to_rad(t.angle)), 0 },
						{ 0, 1,                        0 }
					});
			}
			break;
		case svgdom::transformable::transformation::type::skewy:
			{
				using std::tan;
				this->canvas.transform({
						{ 1,                        0, 0 },
						{ tan(deg_to_rad(t.angle)), 1, 0 }
					});
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
	cairo_get_matrix(this->canvas.cr, &matrix);
	matrix.xx = max(-maxValue, min(matrix.xx, maxValue));
	matrix.yx = max(-maxValue, min(matrix.yx, maxValue));
	matrix.xy = max(-maxValue, min(matrix.xy, maxValue));
	matrix.yy = max(-maxValue, min(matrix.yy, maxValue));
	matrix.x0 = max(-maxValue, min(matrix.x0, maxValue));
	matrix.y0 = max(-maxValue, min(matrix.y0, maxValue));
	cairo_set_matrix(this->canvas.cr, &matrix);
#endif
}

void Renderer::apply_transformations(const decltype(svgdom::transformable::transformations)& transformations){
	for (auto& t : transformations) {
		this->apply_transformation(t);
	}
}

void Renderer::applyViewBox(const svgdom::view_boxed& e, const svgdom::aspect_ratioed& ar){
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
	}else{ // if no preserveAspectRatio enforced
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

void Renderer::applyFilter(const std::string& id){
	auto f = this->finder.find_by_id(id);
	if(!f){
		return;
	}

	FilterApplier visitor(*this);

	ASSERT(f)
	f->e.accept(visitor);

	this->blit(visitor.getLastResult());
}

void Renderer::set_gradient(const std::string& id){
	auto g = this->finder.find_by_id(id);
	if(!g){
		this->canvas.set_source(0, 0, 0, 0);
		return;
	}

	struct CommonGradientPush{
		CairoMatrixSaveRestore cairoMatrixPush; // here we need to save/restore only matrix!

		std::unique_ptr<ViewportPush> viewportPush;

		CommonGradientPush(Renderer& r, const svgdom::gradient& gradient) :
				cairoMatrixPush(r.cr)
		{
			if (r.gradientGetUnits(gradient) == svgdom::coordinate_units::object_bounding_box) {
				r.canvas.translate(r.userSpaceShapeBoundingBox.p);
				r.canvas.scale(r.userSpaceShapeBoundingBox.d);
				this->viewportPush = std::unique_ptr<ViewportPush>(new ViewportPush(r, {{1, 1}}));
			}

			r.apply_transformations(r.gradient_get_transformations(gradient));
		}

		~CommonGradientPush()noexcept{}
	};

	struct GradientSetter : public svgdom::const_visitor{
		Renderer& r;

		const svgdom::style_stack& ss;

		GradientSetter(Renderer& r, const svgdom::style_stack& ss) : r(r), ss(ss) {}

		void visit(const svgdom::linear_gradient_element& gradient)override{
			CommonGradientPush commonPush(this->r, gradient);

			if(auto pat = cairo_pattern_create_linear(
					this->r.length_to_px(this->r.gradientGetX1(gradient), 0),
					this->r.length_to_px(this->r.gradientGetY1(gradient), 1),
					this->r.length_to_px(this->r.gradientGetX2(gradient), 0),
					this->r.length_to_px(this->r.gradientGetY2(gradient), 1)
				))
			{
				utki::scope_exit pat_scope_exit([&pat](){cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}

		void visit(const svgdom::radial_gradient_element& gradient)override{
			CommonGradientPush commonPush(this->r, gradient);

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
					this->r.length_to_px(radius, 0)
				))
			{
				utki::scope_exit pat_scope_exit([&pat](){cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}

		void default_visit(const svgdom::element&)override{
			this->r.canvas.set_source(0, 0, 0, 0);
		}
	} visitor(*this, g->ss);

	g->e.accept(visitor);
}

void Renderer::updateCurBoundingBox(){
	this->userSpaceShapeBoundingBox = this->canvas.get_shape_bounding_box();

	if(this->userSpaceShapeBoundingBox.d[0] == 0){
		// empty path
		return;
	}

	// set device space bounding box
	std::array<r4::vector2<real>, 4> rectVertices = {{
		this->userSpaceShapeBoundingBox.p,
		this->userSpaceShapeBoundingBox.pdx_pdy(),
		this->userSpaceShapeBoundingBox.x_pdy(),
		this->userSpaceShapeBoundingBox.pdx_y()
	}};

	for(auto& vertex : rectVertices){
		vertex = this->canvas.matrix_mul(vertex);

		DeviceSpaceBoundingBox bb;
		bb.left = decltype(bb.left)(vertex[0]);
		bb.right = decltype(bb.right)(vertex[0]);
		bb.top = decltype(bb.top)(vertex[1]);
		bb.bottom = decltype(bb.bottom)(vertex[1]);

		this->deviceSpaceBoundingBox.unite(bb);
	}
}

void Renderer::renderCurrentShape(bool isCairoGroupPushed){
	this->updateCurBoundingBox();

	if(auto p = this->styleStack.get_style_property(svgdom::style_property::fill_rule)){
		switch (p->fill_rule) {
			default:
				ASSERT(false)
				break;
			case svgdom::fill_rule::evenodd:
				this->canvas.set_fill_rule(canvas::fill_rule::even_odd);
				break;
			case svgdom::fill_rule::nonzero:
				this->canvas.set_fill_rule(canvas::fill_rule::winding);
				break;
		}
	}else{
		this->canvas.set_fill_rule(canvas::fill_rule::winding);
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
			this->set_gradient(fill->get_local_id_from_iri());
		} else {
			svgdom::real fillOpacity = 1;
			if (auto p = this->styleStack.get_style_property(svgdom::style_property::fill_opacity)){
				fillOpacity = p->opacity;
			}

			auto fillRgb = fill->get_rgb();
			this->canvas.set_source(fillRgb.r, fillRgb.g, fillRgb.b, fillOpacity * opacity);
		}

		cairo_fill_preserve(this->cr);
		ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
	}

	if (stroke && !stroke->is_none()) {
		if (auto p = this->styleStack.get_style_property(svgdom::style_property::stroke_width)){
			cairo_set_line_width(this->cr, this->length_to_px(p->stroke_width, 0));
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
			this->set_gradient(stroke->get_local_id_from_iri());
		}else{
			svgdom::real strokeOpacity = 1;
			if(auto p = this->styleStack.get_style_property(svgdom::style_property::stroke_opacity)){
				strokeOpacity = p->opacity;
			}

			auto rgb = stroke->get_rgb();
			this->canvas.set_source(rgb.r, rgb.g, rgb.b, strokeOpacity * opacity);
		}

		cairo_stroke_preserve(this->cr);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	// clear path if any left
	this->canvas.clear_path();
	
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
		this->canvas.translate(this->length_to_px(x, y));
	}

	ViewportPush viewportPush(*this, this->length_to_px(width, height));

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
		r4::vector2<real> canvasSize,
		const svgdom::svg_element& root
	) :
		canvas(canvas),
		cr(canvas.cr),
		finder(root),
		dpi(real(dpi)),
		viewport(canvasSize)
{
	this->deviceSpaceBoundingBox.set_empty();
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

	this->apply_transformations(e.transformations);

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
				auto p = this->r.length_to_px(e.x, e.y);
				t.x = p.x();
				t.y = p.y();

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

void Renderer::visit(const svgdom::svg_element& e){
//	TRACE(<< "rendering SvgElement" << std::endl)
	renderSvgElement(e, e, e, e, e.x, e.y, e.width, e.height);
}

bool Renderer::is_invisible(){
	if(auto p = this->styleStack.get_style_property(svgdom::style_property::visibility)){
		if(p->visibility != svgdom::visibility::visible){
			return true;
		}
	}
	return this->isGroupInvisible();
}

bool Renderer::isGroupInvisible(){
	if(auto p = this->styleStack.get_style_property(svgdom::style_property::display)){
		if(p->display == svgdom::display::none){
			return true;
		}
	}
	return false;
}

void Renderer::visit(const svgdom::path_element& e){
//	TRACE(<< "rendering PathElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->is_invisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	r4::vector2<real> prev_quadratic_p{0};

	const svgdom::path_element::step* prevStep = nullptr;

	for(auto& s : e.path){
		switch(s.type_){
			case svgdom::path_element::step::type::move_abs:
				this->canvas.move_to_abs({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::move_rel:
				if(!this->canvas.has_current_point()){
					this->canvas.move_to_abs(0);
				}
				this->canvas.move_to_rel({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::line_abs:
				this->canvas.line_to_abs({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::line_rel:
				this->canvas.line_to_rel({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::horizontal_line_abs:
				this->canvas.line_to_abs({
						real(s.x),
						this->canvas.get_current_point().y()
					});
				break;
			case svgdom::path_element::step::type::horizontal_line_rel:
				this->canvas.line_to_rel({real(s.x), 0});
				break;
			case svgdom::path_element::step::type::vertical_line_abs:
				this->canvas.line_to_abs({
						this->canvas.get_current_point().x(),
						real(s.y)
					});
				break;
			case svgdom::path_element::step::type::vertical_line_rel:
				this->canvas.line_to_rel({0, real(s.y)});
				break;
			case svgdom::path_element::step::type::close:
				this->canvas.close_path();
				break;
			case svgdom::path_element::step::type::quadratic_abs:
				this->canvas.quadratic_curve_to_abs({real(s.x1), real(s.y1)}, {real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::quadratic_rel:
				this->canvas.quadratic_curve_to_rel({real(s.x1), real(s.y1)}, {real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::quadratic_smooth_abs:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p1 = r4::vector2<real>{real(prevStep->x1), real(prevStep->y1)};

					r4::vector2<real> cp1; // control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::quadratic_abs:
							cp1 = -(p1 - cur_p) + cur_p;
							break;
						case svgdom::path_element::step::type::quadratic_smooth_abs:
							cp1 = -(prev_quadratic_p - cur_p) + cur_p;
							break;
						case svgdom::path_element::step::type::quadratic_rel:
							cp1 = -(p1 - p) + cur_p;
							break;
						case svgdom::path_element::step::type::quadratic_smooth_rel:
							cp1 = -(prev_quadratic_p - p) + cur_p;
							break;
						default:
							// No previous step or previous step is not a quadratic Bezier curve.
							// Set first control point equal to current point
							cp1 = cur_p;
							break;
					}
					prev_quadratic_p = cp1;
					this->canvas.quadratic_curve_to_abs(cp1, {real(s.x), real(s.y)});
				}
				break;
			case svgdom::path_element::step::type::quadratic_smooth_rel:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p1 = r4::vector2<real>{real(prevStep->x1), real(prevStep->y1)};

					r4::vector2<real> cp1; // control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::quadratic_smooth_abs:
							cp1 = -(prev_quadratic_p - cur_p);
							break;
						case svgdom::path_element::step::type::quadratic_abs:
							cp1 = -(p1 - cur_p);
							break;
						case svgdom::path_element::step::type::quadratic_smooth_rel:
							cp1 = -(prev_quadratic_p - p);
							break;
						case svgdom::path_element::step::type::quadratic_rel:
							cp1 = -(p1 - p);
							break;
						default:
							// No previous step or previous step is not a quadratic Bezier curve.
							// Set first control point equal to current point, i.e. 0 because this is Relative step.
							cp1.set(0);
							break;
					}
					prev_quadratic_p = cp1;
					this->canvas.quadratic_curve_to_rel(cp1, {real(s.x), real(s.y)});
				}
				break;
			case svgdom::path_element::step::type::cubic_abs:
				this->canvas.cubic_curve_to_abs(
						{real(s.x1), real(s.y1)},
						{real(s.x2), real(s.y2)},
						{real(s.x), real(s.y)}
					);
				break;
			case svgdom::path_element::step::type::cubic_rel:
				this->canvas.cubic_curve_to_rel(
						{real(s.x1), real(s.y1)},
						{real(s.x2), real(s.y2)},
						{real(s.x), real(s.y)}
					);
				break;
			case svgdom::path_element::step::type::cubic_smooth_abs:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p2 = r4::vector2<real>{real(prevStep->x2), real(prevStep->y2)};

					r4::vector2<real> cp1; // first control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::cubic_smooth_abs:
						case svgdom::path_element::step::type::cubic_abs:
							cp1 = -(p2 - cur_p) + cur_p;
							break;
						case svgdom::path_element::step::type::cubic_smooth_rel:
						case svgdom::path_element::step::type::cubic_rel:
							cp1 = -(p2 - p) + cur_p;
							break;
						default:
							// No previous step or previous step is not a cubic Bezier curve.
							// Set first control point equal to current point
							cp1 = cur_p;
							break;
					}
					this->canvas.cubic_curve_to_abs(
							cp1,
							{real(s.x2), real(s.y2)},
							{real(s.x), real(s.y)}
						);
				}
				break;
			case svgdom::path_element::step::type::cubic_smooth_rel:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p2 = r4::vector2<real>{real(prevStep->x2), real(prevStep->y2)};

					r4::vector2<real> cp1; // first control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::cubic_smooth_abs:
						case svgdom::path_element::step::type::cubic_abs:
							cp1 = -(p2 - cur_p);
							break;
						case svgdom::path_element::step::type::cubic_smooth_rel:
						case svgdom::path_element::step::type::cubic_rel:
							cp1 = -(p2 - p);
							break;
						default:
							// No previous step or previous step is not a cubic Bezier curve.
							// Set first control point equal to current point, i.e. 0 because this is Relative step.
							cp1.set(0);
							break;
					}
					this->canvas.cubic_curve_to_rel(
							cp1,
							{real(s.x2), real(s.y2)},
							{real(s.x), real(s.y)}
						);
				}
				break;
			case svgdom::path_element::step::type::arc_abs:
			case svgdom::path_element::step::type::arc_rel:
				{
					auto cur_p = this->canvas.get_current_point();
					auto r = r4::vector2<real>{real(s.rx), real(s.ry)};
					auto p = r4::vector2<real>{real(s.x), real(s.y)};

					if(r.x() <= 0){
						break;
					}
					ASSERT(r.x() > 0)
					auto radii_ratio = r.y() / r.x();

					if(radii_ratio <= 0){
						break;
					}

					r4::vector2<real> end_p; // end point
					
					if(s.type_ == svgdom::path_element::step::type::arc_abs){
						end_p = p - cur_p;
					}else{
						end_p = p;
					}

					// cancel rotation of end point
					end_p.rotate(deg_to_rad(-real(s.x_axis_rotation)));
				
					ASSERT(radii_ratio > 0)
					end_p.y() /= radii_ratio;

					// find the angle between the end point and the x axis
					auto angle = point_angle(real(0), end_p);

					using std::sqrt;

					// put the end point onto the x axis
					end_p.x() = end_p.norm();
					end_p.y() = 0;

					using std::max;

					// update the x radius if it is too small
					r.x() = max(r.x(), end_p.x() / real(2));

					// find one circle center
					r4::vector2<real> center = {
						end_p.x() / real(2),
						sqrt(utki::pow2(r.x()) - utki::pow2(end_p.x() / real(2)))
					};

					// choose between the two circles according to flags
					if(!(s.flags.large_arc ^ s.flags.sweep)){
						center.y() = -center.y();
					}

					// put the second point and the center back to their positions
					end_p = r4::vector2<real>{end_p.x(), real(0)}.rot(angle);
					center.rotate(angle);

					auto angle1 = point_angle(center, real(0));
					auto angle2 = point_angle(center, end_p);

					CairoContextSaveRestore cairoMatrixPush1(this->cr);

					this->canvas.translate(cur_p);
					this->canvas.rotate(deg_to_rad(real(s.x_axis_rotation)));
					this->canvas.scale(1, radii_ratio);

					if(s.flags.sweep){
						// make sure angle1 is smaller than angle2
						if(angle1 > angle2){
							angle1 -= 2 * utki::pi<real>();
						}
						this->canvas.arc_abs(center, r.x(), angle1, angle2);
					}else{
						// make sure angle2 is smaller than angle1
						if(angle2 > angle1){
							angle2 -= 2 * utki::pi<real>();
						}
						this->canvas.arc_abs(center, r.x(), angle1, angle2);
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

	if(this->is_invisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	this->canvas.arc_abs(
			this->length_to_px(e.cx, e.cy),
			this->length_to_px(e.r, 0),
			0,
			2 * utki::pi<real>()
		);

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::polyline_element& e){
//	TRACE(<< "rendering PolylineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->is_invisible()){
		return;
	}

	// TODO: make a common push class for shapes

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	if(e.points.empty()){
		return;
	}

	auto i = e.points.begin();
	this->canvas.move_to_abs(i->to<real>());
	++i;

	for(; i != e.points.end(); ++i){
		this->canvas.line_to_abs(i->to<real>());
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::polygon_element& e){
//	TRACE(<< "rendering PolygonElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->is_invisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	if (e.points.size() == 0) {
		return;
	}

	auto i = e.points.begin();
	this->canvas.move_to_abs(i->to<real>());
	++i;

	for (; i != e.points.end(); ++i) {
		this->canvas.line_to_abs(i->to<real>());
	}

	this->canvas.close_path();

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::line_element& e){
//	TRACE(<< "rendering LineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->is_invisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	this->canvas.move_to_abs(this->length_to_px(e.x1, e.y1));
	this->canvas.line_to_abs(this->length_to_px(e.x2, e.y2));

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::ellipse_element& e){
//	TRACE(<< "rendering EllipseElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->is_invisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	{
		CairoContextSaveRestore saveRestore(this->cr);

		this->canvas.translate(this->length_to_px(e.cx, e.cy));
		this->canvas.scale(this->length_to_px(e.rx, e.ry));

		this->canvas.arc_abs(0, 1, 0, real(2) * utki::pi<real>());
		this->canvas.close_path();
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::style_element& e){
	this->styleStack.add_css(e.css);
}

void Renderer::visit(const svgdom::rect_element& e){
//	TRACE(<< "rendering RectElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->is_invisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->apply_transformations(e.transformations);

	auto dims = this->length_to_px(e.width, e.height);

	// NOTE: see SVG sect: https://www.w3.org/TR/SVG/shapes.html#RectElementWidthAttribute
	//       Zero values disable rendering of the element.
	if(dims.x() == real(0) || dims.y() == real(0)){
		return;
	}

	if((e.rx.value == 0 || !e.rx.is_valid()) & (e.ry.value == 0 || !e.ry.is_valid())){
		this->canvas.rectangle({this->length_to_px(e.x, e.y), dims});
	}else{
		// compute real rx and ry
		auto rx = e.rx;
		auto ry = e.ry;

		if(!ry.is_valid() && rx.is_valid()){
			ry = rx;
		}else if(!rx.is_valid() && ry.is_valid()){
			rx = ry;
		}
		ASSERT(rx.is_valid() && ry.is_valid())

		auto r = this->length_to_px(rx, ry);

		if(r.x() > dims.x() / 2){
			rx = e.width;
			rx.value /= 2;
		}
		if(r.y() > dims.y() / 2){
			ry = e.height;
			ry.value /= 2;
		}

		r = this->length_to_px(rx, ry);

		auto p = this->length_to_px(e.x, e.y);

		this->canvas.move_to_abs(p + r4::vector2<real>{r.x(), 0});
		this->canvas.line_to_abs(p + r4::vector2<real>{dims.x() - r.x(), 0});

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(p + r4::vector2<real>{dims.x() - r.x(), r.y()});
			this->canvas.scale(r);
			this->canvas.arc_abs(0, 1, -utki::pi<real>() / 2, 0);
		}

		this->canvas.line_to_abs(p + dims - r4::vector2<real>{0, r.y()});

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(p + dims - r);
			this->canvas.scale(r);
			this->canvas.arc_abs(0, 1, 0, utki::pi<real>() / 2);
		}

		this->canvas.line_to_abs(p + r4::vector2<real>{r.x(), dims.y()});

		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(p + r4::vector2<real>{r.x(), dims.y() - r.y()});
			this->canvas.scale(r);
			this->canvas.arc_abs(0, 1, utki::pi<real>() / 2, utki::pi<real>());
		}

		this->canvas.line_to_abs(p + r4::vector2<real>{0, r.y()});
		
		{
			CairoContextSaveRestore saveRestore(this->cr);
			this->canvas.translate(p + r);
			this->canvas.scale(r);
			this->canvas.arc_abs(0, 1, utki::pi<real>(), utki::pi<real>() * 3 / 2);
		}

		this->canvas.close_path();
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

const decltype(svgdom::transformable::transformations)& Renderer::gradient_get_transformations(const svgdom::gradient& g){
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
				return this->gradient_get_transformations(*caster.gradient);
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

svgdom::length Renderer::gradientGetCx(const svgdom::radial_gradient_element& g){
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

svgdom::length Renderer::gradientGetCy(const svgdom::radial_gradient_element& g){
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

void Renderer::blit(const Surface& s){
	if(!s.data || s.d.x() == 0 || s.d.y() == 0){
		TRACE(<< "Renderer::blit(): source image is empty" << std::endl)
		return;
	}
	ASSERT(s.data && s.d.x() != 0 && s.d.y() != 0)

	auto dst = getSubSurface(this->cr);

	if(s.p.x() >= dst.d.x() || s.p.y() >= dst.d.y()){
		TRACE(<< "Renderer::blit(): source image is out of canvas" << std::endl)
		return;
	}

	auto dstp = reinterpret_cast<uint32_t*>(dst.data) + s.p.y() * dst.stride + s.p.x();
	auto srcp = reinterpret_cast<uint32_t*>(s.data);
	using std::min;
	auto dp = min(s.d, dst.d - s.p);

	for(unsigned y = 0; y != dp.y(); ++y){
		auto p = dstp + y * dst.stride;
		auto sp = srcp + y * s.stride;
		for(unsigned x = 0; x != dp.x(); ++x, ++p, ++sp){
			*p = *sp;
		}
	}
}
