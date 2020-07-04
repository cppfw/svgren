#include "Renderer.hxx"

#include <utki/math.hpp>

#include <svgdom/elements/coordinate_units.hpp>

#include "util.hxx"

#include "FilterApplier.hxx"

using namespace svgren;


real Renderer::lengthToPx(const svgdom::Length& l, unsigned coordIndex) const noexcept{
	if (l.isPercent()) {
		ASSERT(coordIndex < this->viewport.size())
		return this->viewport[coordIndex] * (l.value / 100);
	}
	return l.toPx(this->dpi);
}

void Renderer::applyCairoTransformation(const svgdom::Transformable::Transformation& t) {
//	TRACE(<< "Renderer::applyCairoTransformation(): applying transformation " << unsigned(t.type) << std::endl)
	switch (t.type_) {
		case svgdom::Transformable::Transformation::Type_e::TRANSLATE:
//			TRACE(<< "translate x,y = (" << t.x << ", " << t.y << ")" << std::endl)
			cairo_translate(this->cr, t.x, t.y);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			break;
		case svgdom::Transformable::Transformation::Type_e::MATRIX:
			{
				cairo_matrix_t matrix;
				matrix.xx = t.a;
				matrix.yx = t.b;
				matrix.xy = t.c;
				matrix.yy = t.d;
				matrix.x0 = t.e;
				matrix.y0 = t.f;
				cairo_transform(this->cr, &matrix);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			}
			break;
		case svgdom::Transformable::Transformation::Type_e::SCALE:
//			TRACE(<< "scale transformation factors = (" << t.x << ", " << t.y << ")" << std::endl)
			if(t.x * t.y != 0){ //cairo does not allow non-invertible scaling
				cairo_scale(this->cr, t.x, t.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			}else{
				TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
			}
			break;
		case svgdom::Transformable::Transformation::Type_e::ROTATE:
			cairo_translate(this->cr, t.x, t.y);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			cairo_rotate(this->cr, degToRad(t.angle));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			cairo_translate(this->cr, -t.x, -t.y);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			break;
		case svgdom::Transformable::Transformation::Type_e::SKEWX:
			{
				cairo_matrix_t matrix;
				matrix.xx = 1;
				matrix.yx = 0;
				matrix.xy = std::tan(degToRad(t.angle));
				matrix.yy = 1;
				matrix.x0 = 0;
				matrix.y0 = 0;
				cairo_transform(this->cr, &matrix);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			}
			break;
		case svgdom::Transformable::Transformation::Type_e::SKEWY:
			{
				cairo_matrix_t matrix;
				matrix.xx = 1;
				matrix.yx = std::tan(degToRad(t.angle));
				matrix.xy = 0;
				matrix.yy = 1;
				matrix.x0 = 0;
				matrix.y0 = 0;
				cairo_transform(this->cr, &matrix);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			}
			break;
		default:
			ASSERT(false)
			break;
	}

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
}

void Renderer::applyTransformations(const decltype(svgdom::Transformable::transformations)& transformations) {
	for (auto& t : transformations) {
		this->applyCairoTransformation(t);
	}
}

void Renderer::applyViewBox(const svgdom::ViewBoxed& e, const svgdom::AspectRatioed& ar) {
	if (!e.isViewBoxSpecified()) {
		return;
	}

	if (ar.preserveAspectRatio.preserve != svgdom::AspectRatioed::PreserveAspectRatio_e::NONE) {
		if (e.viewBox[3] >= 0 && this->viewport[1] >= 0) { // if viewBox width and viewport width are not 0
			real scaleFactor, dx, dy;

			real viewBoxAspect = e.viewBox[2] / e.viewBox[3];
			real viewportAspect = this->viewport[0] / this->viewport[1];

			if ((viewBoxAspect >= viewportAspect && ar.preserveAspectRatio.slice) || (viewBoxAspect < viewportAspect && !ar.preserveAspectRatio.slice)) {
				// fit by Y
				scaleFactor = this->viewport[1] / e.viewBox[3];
				dx = e.viewBox[2] - this->viewport[0];
				dy = 0;
			} else {// viewBoxAspect < viewportAspect
				// fit by X
				scaleFactor = this->viewport[0] / e.viewBox[2];
				dx = 0;
				dy = e.viewBox[3] - this->viewport[1];
			}
			switch (ar.preserveAspectRatio.preserve) {
				case svgdom::AspectRatioed::PreserveAspectRatio_e::NONE:
					ASSERT(false)
				default:
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MIN_Y_MAX:
					cairo_translate(this->cr, 0, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MIN_Y_MID:
					cairo_translate(this->cr, 0, dy / 2);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MIN_Y_MIN:
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MID_Y_MAX:
					cairo_translate(this->cr, dx / 2, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MID_Y_MID:
					cairo_translate(this->cr, dx / 2, dy / 2);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MID_Y_MIN:
					cairo_translate(this->cr, dx / 2, 0);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MAX_Y_MAX:
					cairo_translate(this->cr, dx, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MAX_Y_MID:
					cairo_translate(this->cr, dx, dy / 2);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MAX_Y_MIN:
					cairo_translate(this->cr, dx, 0);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
			}

			if(scaleFactor != 0){ // cairo does not allow non-invertible scaling
				cairo_scale(this->cr, scaleFactor, scaleFactor);
			}else{
				TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
			}
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}
	} else { // if no preserveAspectRatio enforced
		if (e.viewBox[2] != 0 && e.viewBox[3] != 0) { // if viewBox width and height are not 0
			if(this->viewport[0] * this->viewport[1] != 0){ // cairo does not allow non-invertible scaling
				cairo_scale(
						this->cr,
						this->viewport[0] / e.viewBox[2],
						this->viewport[1] / e.viewBox[3]
					);
			}else{
				TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
			}
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}
	}
	cairo_translate(this->cr, -e.viewBox[0], -e.viewBox[1]);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
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

		void visit(const svgdom::Gradient::StopElement& s)override{
			svgdom::style_stack::push stylePush(this->ss, s);

			svgdom::rgb rgb;
			if(auto p = this->ss.get_style_property(svgdom::StyleProperty_e::stop_color)){
				rgb = p->get_rgb();
			}else{
				rgb.r = 0;
				rgb.g = 0;
				rgb.b = 0;
			}

			svgdom::real opacity;
			if(auto p = this->ss.get_style_property(svgdom::StyleProperty_e::stop_opacity)){
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
		case svgdom::Gradient::SpreadMethod_e::DEFAULT:
			ASSERT(false)
			break;
		case svgdom::Gradient::SpreadMethod_e::PAD:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_PAD);
			ASSERT(cairo_pattern_status(&pat) == CAIRO_STATUS_SUCCESS)
			break;
		case svgdom::Gradient::SpreadMethod_e::REFLECT:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_REFLECT);
			ASSERT(cairo_pattern_status(&pat) == CAIRO_STATUS_SUCCESS)
			break;
		case svgdom::Gradient::SpreadMethod_e::REPEAT:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_REPEAT);
			ASSERT(cairo_pattern_status(&pat) == CAIRO_STATUS_SUCCESS)
			break;
	}
	cairo_set_source(this->cr, &pat);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
}

void Renderer::applyFilter() {
	if(auto filter = this->styleStack.get_style_property(svgdom::StyleProperty_e::filter)){
		this->applyFilter(filter->getLocalIdFromIri());
	}
}

void Renderer::applyFilter(const std::string& id) {
	auto f = this->finder.findById(id);
	if(!f){
		return;
	}

	FilterApplier visitor(*this);

	ASSERT(f)
	f->e.accept(visitor);

	this->blit(visitor.getLastResult());
}

void Renderer::setGradient(const std::string& id) {
	auto g = this->finder.findById(id);
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
			if (r.gradientGetUnits(gradient) == svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX) {
				cairo_translate(r.cr, r.userSpaceShapeBoundingBoxPos[0], r.userSpaceShapeBoundingBoxPos[1]);
				ASSERT(cairo_status(r.cr) == CAIRO_STATUS_SUCCESS)
				if(r.userSpaceShapeBoundingBoxDim[0] * r.userSpaceShapeBoundingBoxDim[1] != 0){ // cairo does not allow non-invertible scaling
					cairo_scale(r.cr, r.userSpaceShapeBoundingBoxDim[0], r.userSpaceShapeBoundingBoxDim[1]);
				}else{
					TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
				}
				ASSERT_INFO(cairo_status(r.cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(r.cr)))
				this->viewportPush = std::unique_ptr<ViewportPush>(new ViewportPush(r, {{1, 1}}));
			}

			r.applyTransformations(r.gradientGetTransformations(gradient));
		}

		~CommonGradientPush()noexcept{}
	};

	struct GradientSetter : public svgdom::ConstVisitor{
		Renderer& r;

		const svgdom::style_stack& ss;

		GradientSetter(Renderer& r, const svgdom::style_stack& ss) : r(r), ss(ss) {}

		void visit(const svgdom::LinearGradientElement& gradient)override{
			CommonGradientPush commonPush(this->r, gradient); // TODO: move out of visitor?

			if(auto pat = cairo_pattern_create_linear(
					this->r.lengthToPx(this->r.gradientGetX1(gradient), 0),
					this->r.lengthToPx(this->r.gradientGetY1(gradient), 1),
					this->r.lengthToPx(this->r.gradientGetX2(gradient), 0),
					this->r.lengthToPx(this->r.gradientGetY2(gradient), 1)
				))
			{
				utki::scope_exit pat_scope_exit([&pat]() {cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}

		void visit(const svgdom::RadialGradientElement& gradient)override{
			CommonGradientPush commonPush(this->r, gradient); // TODO: move out of visitor?

			auto cx = this->r.gradientGetCx(gradient);
			auto cy = this->r.gradientGetCy(gradient);
			auto radius = this->r.gradientGetR(gradient);
			auto fx = this->r.gradientGetFx(gradient);
			auto fy = this->r.gradientGetFy(gradient);

			if (!fx.isValid()) {
				fx = cx;
			}
			if (!fy.isValid()) {
				fy = cy;
			}

			if(auto pat = cairo_pattern_create_radial(
					this->r.lengthToPx(fx, 0),
					this->r.lengthToPx(fy, 1),
					0,
					this->r.lengthToPx(cx, 0),
					this->r.lengthToPx(cy, 1),
					this->r.lengthToPx(radius)
				))
			{
				utki::scope_exit pat_scope_exit([&pat]() {cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}

		void default_visit(const svgdom::Element&)override{
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

	if (auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::FILL_RULE)) {
		switch (p->fillRule) {
			default:
				ASSERT(false)
				break;
			case svgdom::FillRule_e::EVENODD:
				cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_EVEN_ODD);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::FillRule_e::NONZERO:
				cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_WINDING);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
		}
	} else {
		cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_WINDING);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	svgdom::StyleValue blackFill;

	auto fill = this->styleStack.get_style_property(svgdom::StyleProperty_e::FILL);
	if (!fill) {
		blackFill = svgdom::StyleValue::parsePaint("black");
		fill = &blackFill;
	}

	auto stroke = this->styleStack.get_style_property(svgdom::StyleProperty_e::STROKE);

	// OPTIMIZATION: in case there is 'opacity' style property and only one of
	//               'stroke' or 'fill' is not none and is a solid color (not pattern/gradient),
	//               then there is no need to push cairo group, but just multiply
	//               the 'stroke-opacity' or 'fill-opacity' by 'opacity' value.
	auto opacity = svgdom::real(1);
	if(!isCairoGroupPushed){
		if(auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::OPACITY)){
			opacity = p->opacity;
		}
	}

	ASSERT(fill)
	if (!fill->isNone()) {
		if (fill->isUrl()) {
			this->setGradient(fill->getLocalIdFromIri());
		} else {
			svgdom::real fillOpacity = 1;
			if (auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::FILL_OPACITY)) {
				fillOpacity = p->opacity;
			}

			auto fillRgb = fill->getRgb();
			cairo_set_source_rgba(this->cr, fillRgb.r, fillRgb.g, fillRgb.b, fillOpacity * opacity);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_fill_preserve(this->cr);
		ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
	}

	if (stroke && !stroke->isNone()) {
		if (auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::STROKE_WIDTH)) {
			cairo_set_line_width(this->cr, this->lengthToPx(p->strokeWidth));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		} else {
			cairo_set_line_width(this->cr, 1);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		if (auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::STROKE_LINECAP)) {
			switch (p->strokeLineCap) {
				default:
					ASSERT(false)
					break;
				case svgdom::StrokeLineCap_e::BUTT:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_BUTT);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::StrokeLineCap_e::ROUND:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_ROUND);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::StrokeLineCap_e::SQUARE:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_SQUARE);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
			}
		} else {
			cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_BUTT);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		if (auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::STROKE_LINEJOIN)) {
			switch (p->strokeLineJoin) {
				default:
					ASSERT(false)
					break;
				case svgdom::StrokeLineJoin_e::MITER:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_MITER);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::StrokeLineJoin_e::ROUND:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_ROUND);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
				case svgdom::StrokeLineJoin_e::BEVEL:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_BEVEL);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					break;
			}
		} else {
			cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_MITER);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		if (stroke->isUrl()) {
			this->setGradient(stroke->getLocalIdFromIri());
		} else {
			svgdom::real strokeOpacity = 1;
			if (auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::STROKE_OPACITY)) {
				strokeOpacity = p->opacity;
			}

			auto rgb = stroke->getRgb();
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
		const svgdom::Container& e,
		const svgdom::Styleable& s,
		const svgdom::ViewBoxed& v,
		const svgdom::AspectRatioed& a,
		const svgdom::Length& x,
		const svgdom::Length& y,
		const svgdom::Length& width,
		const svgdom::Length& height
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
		cairo_translate(
				this->cr,
				this->lengthToPx(x, 0),
				this->lengthToPx(y, 1)
			);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	ViewportPush viewportPush(
			*this,
			{{
				this->lengthToPx(width, 0),
				this->lengthToPx(height, 1)
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
		cairo_t* cr,
		real dpi,
		std::array<real, 2> canvasSize,
		const svgdom::SvgElement& root
	) :
		cr(cr),
		finder(root),
		dpi(dpi),
		viewport(canvasSize)
{
	this->deviceSpaceBoundingBox.setEmpty();
	this->background = getSubSurface(this->cr);
}


void Renderer::visit(const svgdom::GElement& e) {
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
	auto ref = this->finder.findById(e.get_local_id_from_iri());
	if(!ref){
		return;
	}

	struct RefRenderer : public svgdom::ConstVisitor{
		Renderer& r;
		const svgdom::use_element& ue;
		svgdom::g_element fakeGElement;

		RefRenderer(Renderer& r, const svgdom::UseElement& e) :
				r(r), ue(e)
		{
			this->fakeGElement.styles = e.styles;
			this->fakeGElement.presentation_attributes = e.presentation_attributes;
			this->fakeGElement.transformations = e.transformations;

			// add x and y transformation
			{
				svgdom::Transformable::Transformation t;
				t.type_ = svgdom::Transformable::Transformation::Type_e::TRANSLATE;
				t.x = this->r.lengthToPx(e.x, 0);
				t.y = this->r.lengthToPx(e.y, 1);

				this->fakeGElement.transformations.push_back(t);
			}
		}

		void visit(const svgdom::SymbolElement& symbol)override{
			struct FakeSvgElement : public svgdom::Element{
				Renderer& r;
				const svgdom::UseElement& ue;
				const svgdom::SymbolElement& se;

				FakeSvgElement(Renderer& r, const svgdom::UseElement& ue, const svgdom::SymbolElement& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::Visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::ConstVisitor& visitor) const override{
					const auto hundredPercent = svgdom::Length::make(100, svgdom::Length::Unit_e::PERCENT);

					this->r.renderSvgElement(
							this->se,
							this->se,
							this->se,
							this->se,
							svgdom::Length::make(0),
							svgdom::Length::make(0),
							this->ue.width.isValid() ? this->ue.width : hundredPercent,
							this->ue.height.isValid() ? this->ue.height : hundredPercent
						);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, this->ue, symbol));
			this->fakeGElement.accept(this->r);
		}

		void visit(const svgdom::SvgElement& svg)override{
			struct FakeSvgElement : public svgdom::Element{
				Renderer& r;
				const svgdom::UseElement& ue;
				const svgdom::SvgElement& se;

				FakeSvgElement(Renderer& r, const svgdom::UseElement& ue, const svgdom::SvgElement& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::Visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::ConstVisitor& visitor) const override{
					// width and height of <use> element override those of <svg> element.
					this->r.renderSvgElement(
							this->se,
							this->se,
							this->se,
							this->se,
							this->se.x,
							this->se.y,
							this->ue.width.isValid() ? this->ue.width : this->se.width,
							this->ue.height.isValid() ? this->ue.height : this->se.height
						);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, this->ue, svg));
			this->fakeGElement.accept(this->r);
		}

		void default_visit(const svgdom::Element& element)override{
			struct FakeSvgElement : public svgdom::Element{
				Renderer& r;
				const svgdom::Element& e;

				FakeSvgElement(Renderer& r, const svgdom::Element& e) :
						r(r), e(e)
				{}

				void accept(svgdom::Visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::ConstVisitor& visitor) const override{
					this->e.accept(this->r);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, element));
			this->fakeGElement.accept(this->r);
		}

		void default_visit(const svgdom::Element& element, const svgdom::Container& c)override{
			this->default_visit(element);
		}
	} visitor(*this, e);

	ASSERT(ref)

	ref->e.accept(visitor);
}

void Renderer::visit(const svgdom::SvgElement& e) {
//	TRACE(<< "rendering SvgElement" << std::endl)
	renderSvgElement(e, e, e, e, e.x, e.y, e.width, e.height);
}

bool Renderer::isInvisible() {
	if(auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::VISIBILITY)){
		if(p->visibility != svgdom::Visibility_e::VISIBLE){
			return true;
		}
	}
	return this->isGroupInvisible();
}

bool Renderer::isGroupInvisible() {
	if(auto p = this->styleStack.get_style_property(svgdom::StyleProperty_e::DISPLAY)){
		if(p->display == svgdom::Display_e::NONE){
			return true;
		}
	}
	return false;
}

void Renderer::visit(const svgdom::PathElement& e) {
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

	const svgdom::PathElement::Step* prevStep = nullptr;

	for (auto& s : e.path) {
		switch (s.type_) {
			case svgdom::PathElement::Step::Type_e::MOVE_ABS:
				cairo_move_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::MOVE_REL:
				if (!cairo_has_current_point(this->cr)) {
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					cairo_move_to(this->cr, 0, 0);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				cairo_rel_move_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::LINE_ABS:
				cairo_line_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::LINE_REL:
				cairo_rel_line_to(this->cr, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::HORIZONTAL_LINE_ABS:
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
			case svgdom::PathElement::Step::Type_e::HORIZONTAL_LINE_REL:
				cairo_rel_line_to(this->cr, s.x, 0);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::VERTICAL_LINE_ABS:
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
			case svgdom::PathElement::Step::Type_e::VERTICAL_LINE_REL:
				cairo_rel_line_to(this->cr, 0, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::CLOSE:
				cairo_close_path(this->cr);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::QUADRATIC_ABS:
				cairoQuadraticCurveTo(this->cr, s.x1, s.y1, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::QUADRATIC_REL:
				cairoRelQuadraticCurveTo(this->cr, s.x1, s.y1, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_ABS:
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
					switch (prevStep ? prevStep->type_ : svgdom::PathElement::Step::Type_e::UNKNOWN) {
						case svgdom::PathElement::Step::Type_e::QUADRATIC_ABS:
							x1 = -(prevStep->x1 - x0) + x0;
							y1 = -(prevStep->y1 - y0) + y0;
							break;
						case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_ABS:
							x1 = -(prevQuadraticX1 - x0) + x0;
							y1 = -(prevQuadraticY1 - y0) + y0;
							break;
						case svgdom::PathElement::Step::Type_e::QUADRATIC_REL:
							x1 = -(prevStep->x1 - prevStep->x) + x0;
							y1 = -(prevStep->y1 - prevStep->y) + y0;
							break;
						case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_REL:
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
			case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_REL:
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
					switch (prevStep ? prevStep->type_ : svgdom::PathElement::Step::Type_e::UNKNOWN) {
						case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_ABS:
							x1 = -(prevQuadraticX1 - x0);
							y1 = -(prevQuadraticY1 - y0);
							break;
						case svgdom::PathElement::Step::Type_e::QUADRATIC_ABS:
							x1 = -(prevStep->x1 - x0);
							y1 = -(prevStep->y1 - y0);
							break;
						case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_REL:
							x1 = -(prevQuadraticX1 - prevStep->x);
							y1 = -(prevQuadraticY1 - prevStep->y);
							break;
						case svgdom::PathElement::Step::Type_e::QUADRATIC_REL:
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
			case svgdom::PathElement::Step::Type_e::CUBIC_ABS:
				cairo_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::CUBIC_REL:
				cairo_rel_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				break;
			case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_ABS:
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
					switch (prevStep ? prevStep->type_ : svgdom::PathElement::Step::Type_e::UNKNOWN) {
						case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_ABS:
						case svgdom::PathElement::Step::Type_e::CUBIC_ABS:
							x1 = -(prevStep->x2 - x0) + x0;
							y1 = -(prevStep->y2 - y0) + y0;
							break;
						case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_REL:
						case svgdom::PathElement::Step::Type_e::CUBIC_REL:
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
			case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_REL:
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
					switch (prevStep ? prevStep->type_ : svgdom::PathElement::Step::Type_e::UNKNOWN) {
						case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_ABS:
						case svgdom::PathElement::Step::Type_e::CUBIC_ABS:
							x1 = -(prevStep->x2 - x0);
							y1 = -(prevStep->y2 - y0);
							break;
						case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_REL:
						case svgdom::PathElement::Step::Type_e::CUBIC_REL:
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
			case svgdom::PathElement::Step::Type_e::ARC_ABS:
			case svgdom::PathElement::Step::Type_e::ARC_REL:
				{
					double x, y;
					if (cairo_has_current_point(this->cr)) {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						cairo_get_current_point(this->cr, &x, &y);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					} else {
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
						x = 0;
						y = 0;
					}

					if (s.rx <= 0) {
						break;
					}
					ASSERT(s.rx > 0)
					double radiiRatio = s.ry / s.rx;

					if (radiiRatio <= 0) {
						break;
					}

					// cancel rotation of end point
					double xe, ye;
					{
						double xx;
						double yy;
						if (s.type_ == svgdom::PathElement::Step::Type_e::ARC_ABS) {
							xx = s.x - x;
							yy = s.y - y;
						} else {
							xx = s.x;
							yy = s.y;
						}

						auto res = rotate(xx, yy, degToRad(-double(s.xAxisRotation)));
						xe = res[0];
						ye = res[1];
					}
					ASSERT(radiiRatio > 0)
					ye /= radiiRatio;

					// find the angle between the end point and the x axis
					auto angle = pointAngle(double(0), double(0), xe, ye);

					using std::sqrt;

					// put the end point onto the x axis
					xe = sqrt(xe * xe + ye * ye);
					ye = 0;

					using std::max;

					// update the x radius if it is too small
					auto rx = max(double(s.rx), xe / 2);

					// find one circle center
					auto xc = xe / 2;
					auto yc = sqrt(rx * rx - xc * xc);

					// choose between the two circles according to flags
					if (!(s.flags.large_arc ^ s.flags.sweep)) {
						yc = -yc;
					}

					// put the second point and the center back to their positions
					{
						auto res = rotate(xe, double(0), angle);
						xe = res[0];
						ye = res[1];
					}
					{
						auto res = rotate(xc, yc, angle);
						xc = res[0];
						yc = res[1];
					}

					auto angle1 = pointAngle(xc, yc, double(0), double(0));
					auto angle2 = pointAngle(xc, yc, xe, ye);

					CairoContextSaveRestore cairoMatrixPush1(this->cr);

					cairo_translate(this->cr, x, y);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					cairo_rotate(this->cr, degToRad(s.xAxisRotation));
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					if(radiiRatio != 0){ // cairo does not allow non-invertible scaling
						cairo_scale(this->cr, 1, radiiRatio);
						ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
					}else{
						TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
					}
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

void Renderer::visit(const svgdom::CircleElement& e) {
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
			this->lengthToPx(e.cx, 0),
			this->lengthToPx(e.cy, 1),
			this->lengthToPx(e.r),
			0,
			2 * utki::pi<double>()
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::PolylineElement& e) {
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

void Renderer::visit(const svgdom::PolygonElement& e) {
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

void Renderer::visit(const svgdom::LineElement& e) {
//	TRACE(<< "rendering LineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}

	PushCairoGroupIfNeeded cairoGroupPush(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyTransformations(e.transformations);

	cairo_move_to(this->cr, this->lengthToPx(e.x1, 0), this->lengthToPx(e.y1, 1));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	cairo_line_to(this->cr, this->lengthToPx(e.x2, 0), this->lengthToPx(e.y2, 1));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

void Renderer::visit(const svgdom::EllipseElement& e) {
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

		cairo_translate(this->cr, this->lengthToPx(e.cx, 0), this->lengthToPx(e.cy, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		{
			auto dx = this->lengthToPx(e.rx, 0);
			auto dy = this->lengthToPx(e.ry, 1);
			if(dx * dy != 0){ // cairo doesn't allow non-invertible scaling
				cairo_scale(this->cr, dx, dy);
				ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			}else{
				TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
			}
		}
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

	auto width = this->lengthToPx(e.width, 0);
	auto height = this->lengthToPx(e.height, 1);

	// NOTE: see SVG sect: https://www.w3.org/TR/SVG/shapes.html#RectElementWidthAttribute
	//       Zero values disable rendering of the element.
	if(width == real(0) || height == real(0)){
		return;
	}

	if ((e.rx.value == 0 || !e.rx.isValid())
			&& (e.ry.value == 0 || !e.ry.isValid())) {
		cairo_rectangle(this->cr, this->lengthToPx(e.x, 0), this->lengthToPx(e.y, 1), width, height);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	} else {
		// compute real rx and ry
		auto rx = e.rx;
		auto ry = e.ry;

		if (!ry.isValid() && rx.isValid()) {
			ry = rx;
		} else if (!rx.isValid() && ry.isValid()) {
			rx = ry;
		}
		ASSERT(rx.isValid() && ry.isValid())

		if (this->lengthToPx(rx, 0) > width / 2) {
			rx = e.width;
			rx.value /= 2;
		}
		if (this->lengthToPx(ry, 1) > height / 2) {
			ry = e.height;
			ry.value /= 2;
		}

		cairo_move_to(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_line_to(this->cr, this->lengthToPx(e.x, 0) + width - this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + width - this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(ry, 1));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			{
				auto dx = this->lengthToPx(rx, 0);
				auto dy = this->lengthToPx(ry, 1);
				if(dx * dy != 0){ // cairo doesn't allow non-invertible scaling
					cairo_scale(this->cr, dx, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}else{
					TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
				}
			}
			cairo_arc(this->cr, 0, 0, 1, -utki::pi<double>() / 2, 0);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_line_to(this->cr, this->lengthToPx(e.x, 0) + width, this->lengthToPx(e.y, 1) + height - this->lengthToPx(ry, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + width - this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + height - this->lengthToPx(ry, 1));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			{
				auto dx = this->lengthToPx(rx, 0);
				auto dy = this->lengthToPx(ry, 1);
				if(dx * dy != 0){ // cairo doesn't allow non-invertible scaling
					cairo_scale(this->cr, dx, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}else{
					TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
				}
			}
			cairo_arc(this->cr, 0, 0, 1, 0, utki::pi<double>() / 2);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_line_to(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + height);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + height - this->lengthToPx(ry, 1));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			{
				auto dx = this->lengthToPx(rx, 0);
				auto dy = this->lengthToPx(ry, 1);
				if(dx * dy != 0){ // cairo doesn't allow non-invertible scaling
					cairo_scale(this->cr, dx, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}else{
					TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
				}
			}
			cairo_arc(this->cr, 0, 0, 1, utki::pi<double>() / 2, utki::pi<double>());
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_line_to(this->cr, this->lengthToPx(e.x, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(ry, 1));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(ry, 1));
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
			{
				auto dx = this->lengthToPx(rx, 0);
				auto dy = this->lengthToPx(ry, 1);
				if(dx * dy != 0){ // cairo doesn't allow non-invertible scaling
					cairo_scale(this->cr, dx, dy);
					ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
				}else{
					TRACE(<< "WARNING: non-invertible scaling encountered at " << __FILE__ << ":" << __LINE__ << std::endl)
				}
			}
			cairo_arc(this->cr, 0, 0, 1, utki::pi<double>(), utki::pi<double>() * 3 / 2);
			ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		}

		cairo_close_path(this->cr);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}

	this->renderCurrentShape(cairoGroupPush.isPushed());
}

namespace{
struct GradientCaster : public svgdom::ConstVisitor{
	const svgdom::LinearGradientElement* linear = nullptr;
	const svgdom::RadialGradientElement* radial = nullptr;
	const svgdom::Gradient* gradient = nullptr;

	void visit(const svgdom::LinearGradientElement& e) override{
		this->gradient = &e;
		this->linear = &e;
	}

	void visit(const svgdom::RadialGradientElement& e) override{
		this->gradient = &e;
		this->radial = &e;
	}
};
}

const decltype(svgdom::Transformable::transformations)& Renderer::gradientGetTransformations(const svgdom::Gradient& g) {
	if(g.transformations.size() != 0){
		return g.transformations;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

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

svgdom::CoordinateUnits_e Renderer::gradientGetUnits(const svgdom::Gradient& g) {
	if(g.units != svgdom::CoordinateUnits_e::UNKNOWN){
		return g.units;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetUnits(*caster.gradient);
			}
		}
	}
	return svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX; // bounding box is default
}

svgdom::Length Renderer::gradientGetX1(const svgdom::LinearGradientElement& g){
	if(g.x1.isValid()){
		return g.x1;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetX1(*caster.linear);
			}
		}
	}
	return svgdom::Length::make(0, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetY1(const svgdom::LinearGradientElement& g) {
	if(g.y1.isValid()){
		return g.y1;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetY1(*caster.linear);
			}
		}
	}
	return svgdom::Length::make(0, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetX2(const svgdom::LinearGradientElement& g) {
	if(g.x2.isValid()){
		return g.x2;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetX2(*caster.linear);
			}
		}
	}
	return svgdom::Length::make(100, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetY2(const svgdom::LinearGradientElement& g) {
	if(g.y2.isValid()){
		return g.y2;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.linear){
				return this->gradientGetY2(*caster.linear);
			}
		}
	}
	return svgdom::Length::make(0, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetCx(const svgdom::RadialGradientElement& g) {
	if(g.cx.isValid()){
		return g.cx;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetCx(*caster.radial);
			}
		}
	}
	return svgdom::Length::make(50, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetCy(const svgdom::RadialGradientElement& g) {
	if(g.cy.isValid()){
		return g.cy;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetCy(*caster.radial);
			}
		}
	}
	return svgdom::Length::make(50, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetR(const svgdom::RadialGradientElement& g) {
	if(g.r.isValid()){
		return g.r;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetR(*caster.radial);
			}
		}
	}
	return svgdom::Length::make(50, svgdom::Length::Unit_e::PERCENT);
}

svgdom::Length Renderer::gradientGetFx(const svgdom::RadialGradientElement& g) {
	if(g.fx.isValid()){
		return g.fx;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetFx(*caster.radial);
			}
		}
	}
	return svgdom::Length::make(0, svgdom::Length::Unit_e::UNKNOWN);
}

svgdom::Length Renderer::gradientGetFy(const svgdom::RadialGradientElement& g) {
	if(g.fy.isValid()){
		return g.fy;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.radial){
				return this->gradientGetFy(*caster.radial);
			}
		}
	}
	return svgdom::Length::make(0, svgdom::Length::Unit_e::UNKNOWN);
}

const decltype(svgdom::Container::children)&  Renderer::gradientGetStops(const svgdom::Gradient& g) {
	if(g.children.size() != 0){
		return g.children;
	}

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

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

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

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
	if(g.spreadMethod != svgdom::Gradient::SpreadMethod_e::DEFAULT){
		return g.spreadMethod;
	}

	ASSERT(g.spreadMethod == svgdom::Gradient::SpreadMethod_e::DEFAULT)

	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetSpreadMethod(*caster.gradient);
			}
		}
	}

	return svgdom::Gradient::SpreadMethod_e::PAD;
}

decltype(svgdom::styleable::presentation_attributes) Renderer::gradient_get_presentation_attributes(const svgdom::gradient& g){
	decltype(svgdom::styleable::presentation_attributes) ret = g.presentation_attributes; // copy

	decltype(svgdom::styleable::presentation_attributes) ref_attrs;

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);

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
