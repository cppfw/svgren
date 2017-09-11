#include "Renderer.hxx"

#include <utki/math.hpp>

#include <svgdom/CoordinateUnits.hpp>

#include "util.hxx"

using namespace svgren;


real Renderer::lengthToPx(const svgdom::Length& l, unsigned coordIndex) const noexcept{
	if (l.isPercent()) {
		if (this->viewportStack.size() == 0) {
			return 0;
		}
		ASSERT(coordIndex < this->viewportStack.back().size())
		return this->viewportStack.back()[coordIndex] * (l.value / 100);
	}
	return l.toPx(this->dpi);
}

void Renderer::applyCairoTransformation(const svgdom::Transformable::Transformation& t) {
	switch (t.type) {
		case svgdom::Transformable::Transformation::Type_e::TRANSLATE:
			cairo_translate(this->cr, t.x, t.y);
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
		}
			break;
		case svgdom::Transformable::Transformation::Type_e::SCALE:
			cairo_scale(this->cr, t.x, t.y);
			break;
		case svgdom::Transformable::Transformation::Type_e::ROTATE:
			cairo_translate(this->cr, t.x, t.y);
			cairo_rotate(this->cr, degToRad(t.angle));
			cairo_translate(this->cr, -t.x, -t.y);
			break;
		case svgdom::Transformable::Transformation::Type_e::SKEWX:
		{
			cairo_matrix_t matrix;
			matrix.xx = 1;
			matrix.yx = 0;
			matrix.xy = std::tan(t.angle);
			matrix.yy = 1;
			matrix.x0 = 0;
			matrix.y0 = 0;
			cairo_transform(this->cr, &matrix);
		}
			break;
		case svgdom::Transformable::Transformation::Type_e::SKEWY:
		{
			cairo_matrix_t matrix;
			matrix.xx = 1;
			matrix.yx = std::tan(t.angle);
			matrix.xy = 0;
			matrix.yy = 1;
			matrix.x0 = 0;
			matrix.y0 = 0;
			cairo_transform(this->cr, &matrix);
		}
			break;
		default:
			ASSERT(false)
			break;
	}
}

void Renderer::applyCairoTransformations(const decltype(svgdom::Transformable::transformations)& transformations) {
	for (auto& t : transformations) {
		this->applyCairoTransformation(t);
	}
}

void Renderer::applyViewBox(const svgdom::ViewBoxed& e, const svgdom::AspectRatioed& ar) {
	if (!e.isViewBoxSpecified()) {
		return;
	}

	if (ar.preserveAspectRatio.preserve != svgdom::AspectRatioed::PreserveAspectRatio_e::NONE) {
		if (e.viewBox[3] >= 0 && this->viewportStack.back()[1] >= 0) { //if viewBox width and viewport width are not 0
			real scaleFactor, dx, dy;

			real viewBoxAspect = e.viewBox[2] / e.viewBox[3];
			real viewportAspect = this->viewportStack.back()[0] / this->viewportStack.back()[1];

			if ((viewBoxAspect >= viewportAspect && ar.preserveAspectRatio.slice) || (viewBoxAspect < viewportAspect && !ar.preserveAspectRatio.slice)) {
				//fit by Y
				scaleFactor = this->viewportStack.back()[1] / e.viewBox[3];
				dx = e.viewBox[2] - this->viewportStack.back()[0];
				dy = 0;
			} else {//viewBoxAspect < viewportAspect
				//fit by X
				scaleFactor = this->viewportStack.back()[0] / e.viewBox[2];
				dx = 0;
				dy = e.viewBox[3] - this->viewportStack.back()[1];
			}
			switch (ar.preserveAspectRatio.preserve) {
				case svgdom::AspectRatioed::PreserveAspectRatio_e::NONE:
					ASSERT(false)
				default:
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MIN_Y_MAX:
					cairo_translate(this->cr, 0, dy);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MIN_Y_MID:
					cairo_translate(this->cr, 0, dy / 2);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MIN_Y_MIN:
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MID_Y_MAX:
					cairo_translate(this->cr, dx / 2, dy);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MID_Y_MID:
					cairo_translate(this->cr, dx / 2, dy / 2);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MID_Y_MIN:
					cairo_translate(this->cr, dx / 2, 0);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MAX_Y_MAX:
					cairo_translate(this->cr, dx, dy);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MAX_Y_MID:
					cairo_translate(this->cr, dx, dy / 2);
					break;
				case svgdom::AspectRatioed::PreserveAspectRatio_e::X_MAX_Y_MIN:
					cairo_translate(this->cr, dx, 0);
					break;
			}

			cairo_scale(this->cr, scaleFactor, scaleFactor);
		}
	} else {//if no preserveAspectRatio enforced
		if (e.viewBox[2] != 0 && e.viewBox[3] != 0) { //if viewBox width and height are not 0
			cairo_scale(
					this->cr,
					this->viewportStack.back()[0] / e.viewBox[2],
					this->viewportStack.back()[1] / e.viewBox[3]
					);
		}
	}
	cairo_translate(this->cr, -e.viewBox[0], -e.viewBox[1]);
}

void Renderer::setCairoPatternSource(cairo_pattern_t& pat, const svgdom::Gradient& g, const svgdom::StyleStack& ss) {
	svgdom::StyleStack gradientSs(ss);
	if(gradientSs.stack.back()->styles.size() == 0){
		//gradient does not have styles attribute declared, need to inherit it from referenced
		gradientSs.stack.push_back(&this->gradientGetStyle(g));
	}
	
	struct ColorStopAdder : public svgdom::ConstVisitor{
		cairo_pattern_t& pat;
		svgdom::StyleStack& ss;
		
		ColorStopAdder(cairo_pattern_t& pat, svgdom::StyleStack& ss) : pat(pat), ss(ss) {}
		
		void visit(const svgdom::Gradient::StopElement& s)override{
			svgdom::StyleStack::Push stylePush(this->ss, s);
			
			svgdom::Rgb rgb;
			if (auto p = this->ss.getStyleProperty(svgdom::StyleProperty_e::STOP_COLOR)) {
				rgb = p->getRgb();
			} else {
				rgb.r = 0;
				rgb.g = 0;
				rgb.b = 0;
			}

			svgdom::real opacity;
			if (auto p = this->ss.getStyleProperty(svgdom::StyleProperty_e::STOP_OPACITY)) {
				opacity = p->opacity;
			} else {
				opacity = 1;
			}
			cairo_pattern_add_color_stop_rgba(&this->pat, s.offset, rgb.r, rgb.g, rgb.b, opacity);
		}
	} visitor(pat, gradientSs);
	
	for (auto& stop : this->gradientGetStops(g)) {
		stop->accept(visitor);
	}

	switch (this->gradientGetSpreadMethod(g)) {
		default:
		case svgdom::Gradient::SpreadMethod_e::DEFAULT:
			ASSERT(false)
			break;
		case svgdom::Gradient::SpreadMethod_e::PAD:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_PAD);
			break;
		case svgdom::Gradient::SpreadMethod_e::REFLECT:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_REFLECT);
			break;
		case svgdom::Gradient::SpreadMethod_e::REPEAT:
			cairo_pattern_set_extend(&pat, CAIRO_EXTEND_REPEAT);
			break;
	}
	cairo_set_source(this->cr, &pat);
}

void Renderer::applyFilter(const std::string& id) {
	auto f = this->finder.findById(id);
	if(!f){
		return;
	}
	
	struct FilterApplyer : public svgdom::ConstVisitor{
		Renderer& r;
		
		FilterApplyer(Renderer& r) : r(r) {}
		
		svgdom::CoordinateUnits_e primitiveUnits;
		
		void visit(const svgdom::FilterElement& e)override{
			this->primitiveUnits = e.primitiveUnits;
			this->relayAccept(e);
		}
		
		void visit(const svgdom::FeGaussianBlurElement& e)override{
			if(!e.isStdDeviationSpecified()){
				return;
			}
			auto sd = e.getStdDeviation();
			if(this->primitiveUnits == svgdom::CoordinateUnits_e::USER_SPACE_ON_USE){
				double x = double(sd[0]);
				double y = double(sd[1]);
				cairo_user_to_device_distance(this->r.cr, &x, &y);
				sd[0] = real(x);
				sd[1] = real(y);
			}else if(this->primitiveUnits == svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX){
				sd[0] = this->r.curBoundingBoxDim[0] * sd[0];
				sd[1] = this->r.curBoundingBoxDim[1] * sd[1];
			}else{
				return;
			}
			cairoImageSurfaceBlur(cairo_get_target(r.cr), sd);
		}
	} visitor(*this);
	
	ASSERT(f)
	f->e.accept(visitor);
}

void Renderer::setGradient(const std::string& id) {
	auto g = this->finder.findById(id);
	if(!g){
		cairo_set_source_rgba(this->cr, 0, 0, 0, 0);
		return;
	}
	
	struct CommonGradientPush{
		Renderer& r;
		
		const svgdom::Gradient& gradient;
		
		CairoMatrixSaveRestore cairoMatrixPush; //here we need to save/restore only matrix
		
		CommonGradientPush(Renderer& r, const svgdom::Gradient& gradient) :
				r(r),
				gradient(gradient),
				cairoMatrixPush(r.cr)
		{
			if (this->gradient.isBoundingBoxUnits()) {
				cairo_translate(this->r.cr, this->r.curBoundingBoxPos[0], this->r.curBoundingBoxPos[1]);
				cairo_scale(this->r.cr, this->r.curBoundingBoxDim[0], this->r.curBoundingBoxDim[1]);
				this->r.viewportStack.push_back({{1, 1}});
			}

			this->r.applyCairoTransformations(this->gradient.transformations);
		}
		
		~CommonGradientPush()noexcept{
			if(this->gradient.isBoundingBoxUnits()) {
				this->r.viewportStack.pop_back();
			}
		}
	};
	
	struct GradientSetter : public svgdom::ConstVisitor{
		Renderer& r;
		
		const svgdom::StyleStack& ss;
		
		GradientSetter(Renderer& r, const svgdom::StyleStack& ss) : r(r), ss(ss) {}
		
		void visit(const svgdom::LinearGradientElement& gradient)override{
			CommonGradientPush commonPush(this->r, gradient);
			
			if(auto pat = cairo_pattern_create_linear(
					this->r.lengthToPx(this->r.gradientGetX1(gradient), 0),
					this->r.lengthToPx(this->r.gradientGetY1(gradient), 1),
					this->r.lengthToPx(this->r.gradientGetX2(gradient), 0),
					this->r.lengthToPx(this->r.gradientGetY2(gradient), 1)
				))
			{
				utki::ScopeExit patScopeExit([&pat]() {cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}
		
		void visit(const svgdom::RadialGradientElement& gradient)override{
			CommonGradientPush commonPush(this->r, gradient);
			
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
				utki::ScopeExit patScopeExit([&pat]() {cairo_pattern_destroy(pat);});
				this->r.setCairoPatternSource(*pat, gradient, this->ss);
			}
		}
		
		void defaultVisit(const svgdom::Element&)override{
			cairo_set_source_rgba(this->r.cr, 0, 0, 0, 0);
		}
	} visitor(*this, g->ss);
	
	g->e.accept(visitor);
}

void Renderer::updateCurBoundingBox() {
	double x1, y1, x2, y2;

	cairo_path_extents(this->cr,
			&x1,
			&y1,
			&x2,
			&y2
			);

	this->curBoundingBoxPos[0] = svgren::real(x1);
	this->curBoundingBoxPos[1] = svgren::real(y1);
	this->curBoundingBoxDim[0] = svgren::real(x2 - x1);
	this->curBoundingBoxDim[1] = svgren::real(y2 - y1);
}

void Renderer::renderCurrentShape() {
	if (auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::FILL_RULE)) {
		switch (p->fillRule) {
			default:
				ASSERT(false)
				break;
			case svgdom::FillRule_e::EVENODD:
				cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_EVEN_ODD);
				break;
			case svgdom::FillRule_e::NONZERO:
				cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_WINDING);
				break;
		}
	} else {
		cairo_set_fill_rule(this->cr, CAIRO_FILL_RULE_WINDING);
	}

	svgdom::StyleValue blackFill;

	auto fill = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::FILL);
	if (!fill) {
		blackFill = svgdom::StyleValue::parsePaint("black");
		fill = &blackFill;
	}

	auto stroke = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::STROKE);

	this->updateCurBoundingBox();

	ASSERT(fill)
	if (!fill->isNone()) {
		if (fill->isUrl()) {
			this->setGradient(fill->getLocalIdFromIri());
		} else {
			svgdom::real opacity;
			if (auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::FILL_OPACITY)) {
				opacity = p->opacity;
			} else {
				opacity = 1;
			}

			auto fillRgb = fill->getRgb();
			cairo_set_source_rgba(this->cr, fillRgb.r, fillRgb.g, fillRgb.b, opacity);
		}

		if (stroke && !stroke->isNone()) {
			cairo_fill_preserve(this->cr);
		} else {
			cairo_fill(this->cr);
		}
	}

	if (stroke && !stroke->isNone()) {
		if (auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::STROKE_WIDTH)) {
			cairo_set_line_width(this->cr, this->lengthToPx(p->strokeWidth));
		} else {
			cairo_set_line_width(this->cr, 1);
		}

		if (auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::STROKE_LINECAP)) {
			switch (p->strokeLineCap) {
				default:
					ASSERT(false)
					break;
				case svgdom::StrokeLineCap_e::BUTT:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_BUTT);
					break;
				case svgdom::StrokeLineCap_e::ROUND:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_ROUND);
					break;
				case svgdom::StrokeLineCap_e::SQUARE:
					cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_SQUARE);
					break;
			}
		} else {
			cairo_set_line_cap(this->cr, CAIRO_LINE_CAP_BUTT);
		}

		if (auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::STROKE_LINEJOIN)) {
			switch (p->strokeLineJoin) {
				default:
					ASSERT(false)
					break;
				case svgdom::StrokeLineJoin_e::MITER:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_MITER);
					break;
				case svgdom::StrokeLineJoin_e::ROUND:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_ROUND);
					break;
				case svgdom::StrokeLineJoin_e::BEVEL:
					cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_BEVEL);
					break;
			}
		} else {
			cairo_set_line_join(this->cr, CAIRO_LINE_JOIN_MITER);
		}

		if (stroke->isUrl()) {
			this->setGradient(stroke->getLocalIdFromIri());
		} else {
			svgdom::real opacity;
			if (auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::STROKE_OPACITY)) {
				opacity = p->opacity;
			} else {
				opacity = 1;
			}

			auto rgb = stroke->getRgb();
			cairo_set_source_rgba(this->cr, rgb.r, rgb.g, rgb.b, opacity);
		}

		cairo_stroke(this->cr);
	}

	//clear path if any left
	cairo_new_path(this->cr);
	
	if(auto filter = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::FILTER)){
		this->applyFilter(filter->getLocalIdFromIri());
	}
}

void Renderer::renderSvgElement(const svgdom::SvgElement& e, const svgdom::Length& width, const svgdom::Length& height) {
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	if (this->viewportStack.size() > 1) { //if not the outermost 'svg' element
		cairo_translate(
				this->cr,
				this->lengthToPx(e.x, 0),
				this->lengthToPx(e.y, 1)
				);
	}

	this->viewportStack.push_back({
		{
			this->lengthToPx(width, 0),
			this->lengthToPx(height, 1)
		}});
	utki::ScopeExit scopeExit([this]() {
		this->viewportStack.pop_back();
	});

	this->applyViewBox(e, e);

	this->relayAccept(e);
}

Renderer::Renderer(
		cairo_t* cr,
		real dpi,
		std::array<real, 2> canvasSize,
		const svgdom::SvgElement& root
	) :
		cr(cr),
		finder(root),
		dpi(dpi)
{
	this->viewportStack.push_back(canvasSize);
}

void Renderer::visit(const svgdom::GElement& e) {
//	TRACE(<< "rendering GElement: id = " << e.id << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	this->relayAccept(e);
}

void Renderer::visit(const svgdom::UseElement& e) {
//	TRACE(<< "rendering UseElement" << std::endl)
	auto ref = this->finder.findById(e.getLocalIdFromIri());
	if(!ref){
		return;
	}
	
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	//Apply x and y transformation
	{
		svgdom::Transformable::Transformation t;
		t.type = svgdom::Transformable::Transformation::Type_e::TRANSLATE;
		t.x = this->lengthToPx(e.x, 0);
		t.y = this->lengthToPx(e.y, 1);

		this->applyCairoTransformation(t);
	}

	struct RefRenderer : public svgdom::ConstVisitor{
		Renderer& r;
		const svgdom::UseElement& e;
		
		RefRenderer(Renderer& r, const svgdom::UseElement& e) : r(r), e(e){}
		
		void visit(const svgdom::SymbolElement& symbol)override{
			svgdom::StyleStack::Push pushSymbolStyles(this->r.styleStack, symbol);

			SetTempCairoContext symbolCairoTempContext(this->r);

			CairoContextSaveRestore symbolCairoMatrixPush(this->r.cr);

			const auto hundredPercent = svgdom::Length::make(100, svgdom::Length::Unit_e::PERCENT);
			this->r.viewportStack.push_back({
				{
					this->r.lengthToPx(this->e.width.isValid() ? this->e.width : hundredPercent, 0),
					this->r.lengthToPx(this->e.height.isValid() ? this->e.height : hundredPercent, 1)
				}});
			utki::ScopeExit symbolScopeExit([this]() {
				this->r.viewportStack.pop_back();
			});

			this->r.applyViewBox(symbol, symbol);

			this->r.relayAccept(symbol);
		}
		
		void visit(const svgdom::SvgElement& svg)override{
			//width and height of <use> element override those of <svg> element.
			this->r.renderSvgElement(
					svg,
					this->e.width.isValid() ? this->e.width : svg.width,
					this->e.height.isValid() ? this->e.height : svg.height
				);
		}
		
		void defaultVisit(const svgdom::Element& element)override{
			element.accept(this->r);
		}
		
		void defaultVisit(const svgdom::Element& element, const svgdom::Container& c)override{
			this->defaultVisit(element);
		}
	} visitor(*this, e);
	
	ASSERT(ref)
	
	ref->e.accept(visitor);
}

void Renderer::visit(const svgdom::SvgElement& e) {
//	TRACE(<< "rendering SvgElement" << std::endl)
	renderSvgElement(e, e.width, e.height);
}

bool Renderer::isInvisible() {
	if(auto p = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::DISPLAY)){
		if(p->display == svgdom::Display_e::NONE){
			return true;
		}
	}
	return false;
}


void Renderer::visit(const svgdom::PathElement& e) {
//	TRACE(<< "rendering PathElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	double prevQuadraticX1 = 0;
	double prevQuadraticY1 = 0;

	const svgdom::PathElement::Step* prevStep = nullptr;

	for (auto& s : e.path) {
		switch (s.type) {
			case svgdom::PathElement::Step::Type_e::MOVE_ABS:
				cairo_move_to(this->cr, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::MOVE_REL:
				if (!cairo_has_current_point(this->cr)) {
					cairo_move_to(this->cr, 0, 0);
				}
				cairo_rel_move_to(this->cr, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::LINE_ABS:
				cairo_line_to(this->cr, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::LINE_REL:
				cairo_rel_line_to(this->cr, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::HORIZONTAL_LINE_ABS:
				{
					double x, y;
					if (cairo_has_current_point(this->cr)) {
						cairo_get_current_point(this->cr, &x, &y);
					} else {
						y = 0;
					}
					cairo_line_to(this->cr, s.x, y);
				}
				break;
			case svgdom::PathElement::Step::Type_e::HORIZONTAL_LINE_REL:
				cairo_rel_line_to(this->cr, s.x, 0);
				break;
			case svgdom::PathElement::Step::Type_e::VERTICAL_LINE_ABS:
				{
					double x, y;
					if (cairo_has_current_point(this->cr)) {
						cairo_get_current_point(this->cr, &x, &y);
					} else {
						x = 0;
					}
					cairo_line_to(this->cr, x, s.y);
				}
				break;
			case svgdom::PathElement::Step::Type_e::VERTICAL_LINE_REL:
				cairo_rel_line_to(this->cr, 0, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::CLOSE:
				cairo_close_path(this->cr);
				break;
			case svgdom::PathElement::Step::Type_e::QUADRATIC_ABS:
				cairoQuadraticCurveTo(this->cr, s.x1, s.y1, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::QUADRATIC_REL:
				cairoRelQuadraticCurveTo(this->cr, s.x1, s.y1, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::QUADRATIC_SMOOTH_ABS:
				{
					double x0, y0; //current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						cairo_get_current_point(this->cr, &x0, &y0);
					} else {
						cairo_move_to(this->cr, 0, 0);
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; //control point
					switch (prevStep ? prevStep->type : svgdom::PathElement::Step::Type_e::UNKNOWN) {
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
							//No previous step or previous step is not a quadratic Bezier curve.
							//Set first control point equal to current point
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
					double x0, y0; //current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						cairo_get_current_point(this->cr, &x0, &y0);
					} else {
						cairo_move_to(this->cr, 0, 0);
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; //control point
					switch (prevStep ? prevStep->type : svgdom::PathElement::Step::Type_e::UNKNOWN) {
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
							//No previous step or previous step is not a quadratic Bezier curve.
							//Set first control point equal to current point, i.e. 0 because this is Relative step.
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
				break;
			case svgdom::PathElement::Step::Type_e::CUBIC_REL:
				cairo_rel_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
				break;
			case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_ABS:
				{
					double x0, y0; //current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						cairo_get_current_point(this->cr, &x0, &y0);
					} else {
						cairo_move_to(this->cr, 0, 0);
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; //first control point
					switch (prevStep ? prevStep->type : svgdom::PathElement::Step::Type_e::UNKNOWN) {
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
							//No previous step or previous step is not a cubic Bezier curve.
							//Set first control point equal to current point
							x1 = x0;
							y1 = y0;
							break;
					}
					cairo_curve_to(this->cr, x1, y1, s.x2, s.y2, s.x, s.y);
				}
				break;
			case svgdom::PathElement::Step::Type_e::CUBIC_SMOOTH_REL:
				{
					double x0, y0; //current point, absolute coordinates
					if (cairo_has_current_point(this->cr)) {
						cairo_get_current_point(this->cr, &x0, &y0);
					} else {
						cairo_move_to(this->cr, 0, 0);
						x0 = 0;
						y0 = 0;
					}

					double x1, y1; //first control point
					switch (prevStep ? prevStep->type : svgdom::PathElement::Step::Type_e::UNKNOWN) {
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
							//No previous step or previous step is not a cubic Bezier curve.
							//Set first control point equal to current point, i.e. 0 because this is Relative step.
							x1 = 0;
							y1 = 0;
							break;
					}
					cairo_rel_curve_to(this->cr, x1, y1, s.x2, s.y2, s.x, s.y);
				}
				break;
			case svgdom::PathElement::Step::Type_e::ARC_ABS:
			case svgdom::PathElement::Step::Type_e::ARC_REL:
				{
					real x, y;
					if (cairo_has_current_point(this->cr)) {
						double xx, yy;
						cairo_get_current_point(this->cr, &xx, &yy);
						x = real(xx);
						y = real(yy);
					} else {
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

					//cancel rotation of end point
					real xe, ye;
					{
						real xx;
						real yy;
						if (s.type == svgdom::PathElement::Step::Type_e::ARC_ABS) {
							xx = s.x - x;
							yy = s.y - y;
						} else {
							xx = s.x;
							yy = s.y;
						}

						auto res = rotate(xx, yy, degToRad(-s.xAxisRotation));
						xe = res[0];
						ye = res[1];
					}
					ASSERT(radiiRatio > 0)
					ye /= radiiRatio;

					//Find the angle between the end point and the x axis
					real angle = pointAngle(0, 0, xe, ye);

					//Put the end point onto the x axis
					xe = std::sqrt(xe * xe + ye * ye);
					ye = 0;

					//Update the x radius if it is too small
					real rx = std::max(s.rx, xe / 2);

					//Find one circle center
					real xc = xe / 2;
					real yc = std::sqrt(rx * rx - xc * xc);

					//Choose between the two circles according to flags
					if (!(s.flags.largeArc ^ s.flags.sweep)) {
						yc = -yc;
					}

					//Put the second point and the center back to their positions
					{
						auto res = rotate(xe, 0, angle);
						xe = res[0];
						ye = res[1];
					}
					{
						auto res = rotate(xc, yc, angle);
						xc = res[0];
						yc = res[1];
					}

					real angle1 = pointAngle(xc, yc, 0, 0);
					real angle2 = pointAngle(xc, yc, xe, ye);

					CairoContextSaveRestore cairoMatrixPush1(this->cr);

					cairo_translate(this->cr, x, y);
					cairo_rotate(this->cr, degToRad(s.xAxisRotation));
					cairo_scale(this->cr, 1, radiiRatio);
					if (s.flags.sweep) {
						cairo_arc(this->cr, xc, yc, rx, angle1, angle2);
					} else {
						cairo_arc_negative(this->cr, xc, yc, rx, angle1, angle2);
					}
				}
				break;
			default:
				ASSERT_INFO(false, "unknown path step type: " << unsigned(s.type))
				break;
		}
		prevStep = &s;
	}

	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::CircleElement& e) {
//	TRACE(<< "rendering CircleElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	cairo_arc(
			this->cr,
			this->lengthToPx(e.cx, 0),
			this->lengthToPx(e.cy, 1),
			this->lengthToPx(e.r),
			0,
			2 * utki::pi<double>()
			);

	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::PolylineElement& e) {
//	TRACE(<< "rendering PolylineElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	if (e.points.size() == 0) {
		return;
	}

	auto i = e.points.begin();
	cairo_move_to(this->cr, (*i)[0], (*i)[1]);
	++i;

	for (; i != e.points.end(); ++i) {
		cairo_line_to(this->cr, (*i)[0], (*i)[1]);
	}

	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::PolygonElement& e) {
//	TRACE(<< "rendering PolygonElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	if (e.points.size() == 0) {
		return;
	}

	auto i = e.points.begin();
	cairo_move_to(this->cr, (*i)[0], (*i)[1]);
	++i;

	for (; i != e.points.end(); ++i) {
		cairo_line_to(this->cr, (*i)[0], (*i)[1]);
	}

	cairo_close_path(this->cr);

	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::LineElement& e) {
//	TRACE(<< "rendering LineElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	cairo_move_to(this->cr, this->lengthToPx(e.x1, 0), this->lengthToPx(e.y1, 1));
	cairo_line_to(this->cr, this->lengthToPx(e.x2, 0), this->lengthToPx(e.y2, 1));

	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::EllipseElement& e) {
//	TRACE(<< "rendering EllipseElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	{
		CairoContextSaveRestore saveRestore(this->cr);

		cairo_translate(this->cr, this->lengthToPx(e.cx, 0), this->lengthToPx(e.cy, 1));
		cairo_scale(this->cr, this->lengthToPx(e.rx, 0), this->lengthToPx(e.ry, 1));
		cairo_arc(this->cr, 0, 0, 1, 0, 2 * utki::pi<double>());
		cairo_close_path(this->cr);
	}


	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::RectElement& e) {
//	TRACE(<< "rendering RectElement" << std::endl)
	svgdom::StyleStack::Push pushStyles(this->styleStack, e);

	if(this->isInvisible()){
		return;
	}
	
	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	if ((e.rx.value == 0 || !e.rx.isValid())
			&& (e.ry.value == 0 || !e.ry.isValid())) {
		cairo_rectangle(this->cr, this->lengthToPx(e.x, 0), this->lengthToPx(e.y, 1), this->lengthToPx(e.width, 0), this->lengthToPx(e.height, 1));
	} else {
		//compute real rx and ry
		auto rx = e.rx;
		auto ry = e.ry;

		if (!ry.isValid() && rx.isValid()) {
			ry = rx;
		} else if (!rx.isValid() && ry.isValid()) {
			rx = ry;
		}
		ASSERT(rx.isValid() && ry.isValid())

		if (this->lengthToPx(rx, 0) > this->lengthToPx(e.width, 0) / 2) {
			rx = e.width;
			rx.value /= 2;
		}
		if (this->lengthToPx(ry, 1) > this->lengthToPx(e.height, 1) / 2) {
			ry = e.height;
			ry.value /= 2;
		}

		cairo_move_to(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1));
		cairo_line_to(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(e.width, 0) - this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1));

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(e.width, 0) - this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(ry, 1));
			cairo_scale(this->cr, this->lengthToPx(rx, 0), this->lengthToPx(ry, 1));
			cairo_arc(this->cr, 0, 0, 1, -utki::pi<double>() / 2, 0);
		}

		cairo_line_to(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(e.width, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(e.height, 1) - this->lengthToPx(ry, 1));

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(e.width, 0) - this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(e.height, 1) - this->lengthToPx(ry, 1));
			cairo_scale(this->cr, this->lengthToPx(rx, 0), this->lengthToPx(ry, 1));
			cairo_arc(this->cr, 0, 0, 1, 0, utki::pi<double>() / 2);
		}

		cairo_line_to(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(e.height, 1));

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(e.height, 1) - this->lengthToPx(ry, 1));
			cairo_scale(this->cr, this->lengthToPx(rx, 0), this->lengthToPx(ry, 1));
			cairo_arc(this->cr, 0, 0, 1, utki::pi<double>() / 2, utki::pi<double>());
		}

		cairo_line_to(this->cr, this->lengthToPx(e.x, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(ry, 1));

		{
			CairoContextSaveRestore saveRestore(this->cr);
			cairo_translate(this->cr, this->lengthToPx(e.x, 0) + this->lengthToPx(rx, 0), this->lengthToPx(e.y, 1) + this->lengthToPx(ry, 1));
			cairo_scale(this->cr, this->lengthToPx(rx, 0), this->lengthToPx(ry, 1));
			cairo_arc(this->cr, 0, 0, 1, utki::pi<double>(), utki::pi<double>() * 3 / 2);
		}

		cairo_close_path(this->cr);
	}

	this->renderCurrentShape();
}



Renderer::SetTempCairoContext::SetTempCairoContext(Renderer& renderer) :
		renderer(renderer)
{
	auto opacityP = this->renderer.styleStack.getStyleProperty(svgdom::StyleProperty_e::OPACITY);
	auto filterP = this->renderer.styleStack.getStyleProperty(svgdom::StyleProperty_e::FILTER);
	if((opacityP && opacityP->opacity < 1) || filterP){
//		TRACE(<< "setting temp context" << std::endl)
		this->surface = cairo_surface_create_similar_image(
				cairo_get_target(renderer.cr),
				cairo_image_surface_get_format(cairo_get_target(renderer.cr)),
				cairo_image_surface_get_width(cairo_get_target(renderer.cr)),
				cairo_image_surface_get_height(cairo_get_target(renderer.cr))
			);
		this->oldCr = renderer.cr;
		renderer.cr = cairo_create(this->surface);

		cairo_matrix_t m;
		cairo_get_matrix(this->oldCr, &m);

		cairo_set_matrix(renderer.cr, &m);
		
		
		if(opacityP){
			this->opacity = opacityP->opacity;
		}
	}
}

Renderer::SetTempCairoContext::~SetTempCairoContext()noexcept{
	if(this->oldCr){
		//blit surface
		{
			CairoContextSaveRestore saveRestore(this->oldCr);
			cairo_identity_matrix(this->oldCr);
			cairo_set_source_surface(this->oldCr, this->surface, 0, 0);
			cairo_paint_with_alpha(this->oldCr, this->opacity);
		}
		
		ASSERT(this->surface)
		cairo_destroy(this->renderer.cr);
		cairo_surface_destroy(this->surface);
		this->renderer.cr = this->oldCr;
	}else{
		ASSERT(!this->surface)
	}
}

namespace{
struct GradientCaster : public svgdom::ConstVisitor{
	const svgdom::LinearGradientElement* linear;
	const svgdom::RadialGradientElement* radial;
	const svgdom::Gradient* gradient;
	
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

const svgdom::Styleable& Renderer::gradientGetStyle(const svgdom::Gradient& g) {
	if(g.styles.size() != 0){
		return g;
	}
	
	auto refId = g.getLocalIdFromIri();
	if(refId.length() != 0){
		auto ref = this->finder.findById(refId);
		
		if(ref){
			GradientCaster caster;
			ref->e.accept(caster);
			if(caster.gradient){
				return this->gradientGetStyle(*caster.gradient);
			}
		}
	}
	
	return g;
}

svgdom::Gradient::SpreadMethod_e Renderer::gradientGetSpreadMethod(const svgdom::Gradient& g) {
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
