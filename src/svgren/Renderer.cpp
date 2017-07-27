#include "Renderer.hxx"

#include <utki/math.hpp>

using namespace svgren;

namespace{
class CairoMatrixSaveRestore{
	cairo_matrix_t m;
	cairo_t* cr;
public:
	CairoMatrixSaveRestore(cairo_t* cr) :
			cr(cr)
	{
		ASSERT(this->cr)
		cairo_get_matrix(this->cr, &this->m);
	}
	~CairoMatrixSaveRestore()noexcept{
		cairo_set_matrix(this->cr, &this->m);
	}
};
}

namespace{
class CairoContextSaveRestore{
	cairo_t* cr;
	
public:
	CairoContextSaveRestore(cairo_t* cr) :
			cr(cr)
	{
		ASSERT(this->cr)
		cairo_save(this->cr);
	}
	
	~CairoContextSaveRestore()noexcept{
		cairo_restore(this->cr);
	}
};
}

namespace{
void cairoRelQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y){
	cairo_rel_curve_to(cr,
			2.0 / 3.0 * x1,
			2.0 / 3.0 * y1,
			2.0 / 3.0 * x1 + 1.0 / 3.0 * x,
			2.0 / 3.0 * y1 + 1.0 / 3.0 * y,
			x,
			y
		);
}
}

namespace{
void cairoQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y){
	double x0, y0; //current point, absolute coordinates
	if (cairo_has_current_point(cr)) {
		cairo_get_current_point(cr, &x0, &y0);
	}
	else {
		cairo_move_to(cr, 0, 0);
		x0 = 0;
		y0 = 0;
	}
	cairo_curve_to(cr,
			2.0 / 3.0 * x1 + 1.0 / 3.0 * x0,
			2.0 / 3.0 * y1 + 1.0 / 3.0 * y0,
			2.0 / 3.0 * x1 + 1.0 / 3.0 * x,
			2.0 / 3.0 * y1 + 1.0 / 3.0 * y,
			x,
			y
		);
}
}

namespace{
//convert degrees to radians
real degToRad(real deg){
	return deg * utki::pi<real>() / real(180);
}
}

namespace{
//Rotate a point by an angle around the origin point.
std::tuple<real, real> rotate(real x, real y, real angle){
    return std::make_tuple(x * std::cos(angle) - y * std::sin(angle), y * std::cos(angle) + x * std::sin(angle));
}
}

namespace{
//Return angle between x axis and point knowing given center.
real pointAngle(real cx, real cy, real px, real py){
    return std::atan2(py - cy, px - cx);
}
}



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

void Renderer::applyViewBox(const svgdom::ViewBoxed& e) {
	if (!e.isViewBoxSpecified()) {
		return;
	}

	if (e.preserveAspectRatio.preserve != svgdom::PreserveAspectRatio_e::NONE) {
		if (e.viewBox[3] >= 0 && this->viewportStack.back()[1] >= 0) { //if viewBox width and viewport width are not 0
			real scaleFactor, dx, dy;

			real viewBoxAspect = e.viewBox[2] / e.viewBox[3];
			real viewportAspect = this->viewportStack.back()[0] / this->viewportStack.back()[1];

			if ((viewBoxAspect >= viewportAspect && e.preserveAspectRatio.slice) || (viewBoxAspect < viewportAspect && !e.preserveAspectRatio.slice)) {
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
			switch (e.preserveAspectRatio.preserve) {
				case svgdom::PreserveAspectRatio_e::NONE:
					ASSERT(false)
				default:
					break;
				case svgdom::PreserveAspectRatio_e::X_MIN_Y_MAX:
					cairo_translate(this->cr, 0, dy);
					break;
				case svgdom::PreserveAspectRatio_e::X_MIN_Y_MID:
					cairo_translate(this->cr, 0, dy / 2);
					break;
				case svgdom::PreserveAspectRatio_e::X_MIN_Y_MIN:
					break;
				case svgdom::PreserveAspectRatio_e::X_MID_Y_MAX:
					cairo_translate(this->cr, dx / 2, dy);
					break;
				case svgdom::PreserveAspectRatio_e::X_MID_Y_MID:
					cairo_translate(this->cr, dx / 2, dy / 2);
					break;
				case svgdom::PreserveAspectRatio_e::X_MID_Y_MIN:
					cairo_translate(this->cr, dx / 2, 0);
					break;
				case svgdom::PreserveAspectRatio_e::X_MAX_Y_MAX:
					cairo_translate(this->cr, dx, dy);
					break;
				case svgdom::PreserveAspectRatio_e::X_MAX_Y_MID:
					cairo_translate(this->cr, dx, dy / 2);
					break;
				case svgdom::PreserveAspectRatio_e::X_MAX_Y_MIN:
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

void Renderer::setCairoPatternSource(cairo_pattern_t& pat, const svgdom::Gradient& g) {
	class ColorStopAdder : public svgdom::Visitor{
		
	};
	
	for (auto& stop : g.getStops()) {
		
		if (auto s = dynamic_cast<svgdom::Gradient::StopElement*> (stop.get())) {
			svgdom::Rgb rgb;
			if (auto p = s->getStyleProperty(svgdom::StyleProperty_e::STOP_COLOR)) {
				rgb = p->getRgb();
			} else {
				rgb.r = 0;
				rgb.g = 0;
				rgb.b = 0;
			}

			svgdom::real opacity;
			if (auto p = s->getStyleProperty(svgdom::StyleProperty_e::STOP_OPACITY)) {
				opacity = p->opacity;
			} else {
				opacity = 1;
			}
			cairo_pattern_add_color_stop_rgba(&pat, s->offset, rgb.r, rgb.g, rgb.b, opacity);
		}
	}
	switch (g.getSpreadMethod()) {
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

void Renderer::setGradient(const std::string& id) {
	auto g = this->finder.findById(id);
	if(!g){
		cairo_set_source_rgba(this->cr, 0, 0, 0, 0);
		return;
	}
	
	struct GradientSetter : public svgdom::Visitor{
		Renderer& r;
		
		GradientSetter(Renderer& r) : r(r) {}
		
		void visit(const svgdom::LinearGradientElement& gradient)override{
			//TODO:
		}
		
		void visit(const svgdom::RadialGradientElement& gradient)override{
			//TODO:
		}
		
		void defaultVisit(const svgdom::Element&)override{
			cairo_set_source_rgba(this->r.cr, 0, 0, 0, 0);
		}
	} visitor(*this);
	
	ASSERT(g.e)
	g.e->accept(visitor);
	
	
	
	
	const svgdom::Element* gradientElement = g.e;
	
	if (auto gradient = dynamic_cast<const svgdom::Gradient*> (gradientElement)) {
		//here we need to save/restore only matrix
		CairoMatrixSaveRestore cairoMatrixPush(this->cr);

		if (gradient->isBoundingBoxUnits()) {
			cairo_translate(this->cr, this->curBoundingBoxPos[0], this->curBoundingBoxPos[1]);
			cairo_scale(this->cr, this->curBoundingBoxDim[0], this->curBoundingBoxDim[1]);
			this->viewportStack.push_back({{1, 1}});
		}
		utki::ScopeExit gradScopeExit([this, gradient]() {
			if (gradient->isBoundingBoxUnits()) {
				this->viewportStack.pop_back();
			}
		});

		this->applyCairoTransformations(gradient->transformations);

		if (auto g = dynamic_cast<const svgdom::LinearGradientElement*> (gradient)) {
			if(auto pat = cairo_pattern_create_linear(
					this->lengthToPx(g->getX1(), 0),
					this->lengthToPx(g->getY1(), 1),
					this->lengthToPx(g->getX2(), 0),
					this->lengthToPx(g->getY2(), 1)
				))
			{
				utki::ScopeExit patScopeExit([&pat]() {cairo_pattern_destroy(pat);});
				this->setCairoPatternSource(*pat, *g);
			}
		} else if (auto g = dynamic_cast<const svgdom::RadialGradientElement*> (gradient)) {
			auto cx = g->getCx();
			auto cy = g->getCy();
			auto r = g->getR();
			auto fx = g->getFx();
			auto fy = g->getFy();

			if (!fx.isValid()) {
				fx = cx;
			}
			if (!fy.isValid()) {
				fy = cy;
			}

			if(auto pat = cairo_pattern_create_radial(
					this->lengthToPx(fx, 0),
					this->lengthToPx(fy, 1),
					0,
					this->lengthToPx(cx, 0),
					this->lengthToPx(cy, 1),
					this->lengthToPx(r)
				))
			{
				utki::ScopeExit patScopeExit([&pat]() {cairo_pattern_destroy(pat);});
				this->setCairoPatternSource(*pat, *g);
			}
		}
	}
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

	svgdom::StylePropertyValue blackFill;

	auto fill = this->styleStack.getStyleProperty(svgdom::StyleProperty_e::FILL);
	if (!fill) {
		blackFill = svgdom::StylePropertyValue::parsePaint("black");
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
			cairo_set_line_width(this->cr, this->lengthToPx(p->length));
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
}

void Renderer::renderSvgElement(const svgdom::SvgElement& e, const svgdom::Length& width, const svgdom::Length& height) {
	StyleStack::Push pushStyles(this->styleStack, e);

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

	this->applyViewBox(e);

	e.relayAccept(*this);
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
	StyleStack::Push pushStyles(this->styleStack, e);

	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	e.relayAccept(*this);
}

void Renderer::visit(const svgdom::UseElement& e) {
	auto ref = this->finder.findById(e.getLocalIdFromIri());
	if(!ref){
		return;
	}
	
	StyleStack::Push pushStyles(this->styleStack, e);

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

	struct RefRenderer : public svgdom::Visitor{
		Renderer& r;
		const svgdom::UseElement& e;
		
		RefRenderer(Renderer& r, const svgdom::UseElement& e) : r(r), e(e){}
		
		void visit(const svgdom::SymbolElement& symbol)override{
			StyleStack::Push pushSymbolStyles(this->r.styleStack, symbol);

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

			this->r.applyViewBox(symbol);

			symbol.relayAccept(this->r);
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
	} visitor(*this, e);
	
	ASSERT(ref.e)
	
	ref.e->accept(visitor);
}

void Renderer::visit(const svgdom::SvgElement& e) {
	renderSvgElement(e, e.width, e.height);
}

void Renderer::visit(const svgdom::PathElement& e) {
	StyleStack::Push pushStyles(this->styleStack, e);

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
						xe = std::get<0>(res);
						ye = std::get<1>(res);
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
						xe = std::get<0>(res);
						ye = std::get<1>(res);
					}
					{
						auto res = rotate(xc, yc, angle);
						xc = std::get<0>(res);
						yc = std::get<1>(res);
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
	StyleStack::Push pushStyles(this->styleStack, e);

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
	StyleStack::Push pushStyles(this->styleStack, e);

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
	StyleStack::Push pushStyles(this->styleStack, e);

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
	StyleStack::Push pushStyles(this->styleStack, e);

	SetTempCairoContext cairoTempContext(*this);

	CairoContextSaveRestore cairoMatrixPush(this->cr);

	this->applyCairoTransformations(e.transformations);

	cairo_move_to(this->cr, this->lengthToPx(e.x1, 0), this->lengthToPx(e.y1, 1));
	cairo_line_to(this->cr, this->lengthToPx(e.x2, 0), this->lengthToPx(e.y2, 1));

	this->renderCurrentShape();
}

void Renderer::visit(const svgdom::EllipseElement& e) {
	StyleStack::Push pushStyles(this->styleStack, e);

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
	StyleStack::Push pushStyles(this->styleStack, e);

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
	if(auto p = this->renderer.styleStack.getStyleProperty(svgdom::StyleProperty_e::OPACITY)){
		if(p->opacity < 1){
			this->opacity = p->opacity;
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
