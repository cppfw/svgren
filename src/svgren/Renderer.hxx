#pragma once

#include <vector>

#include <utki/config.hpp>

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

#include <svgdom/Visitor.hpp>

#include "config.hpp"

namespace svgren{


class Renderer : public svgdom::Visitor{
	cairo_t* cr;
	
	const real dpi;
	
	std::vector<std::array<real, 2>> viewportStack;//stack of width, height
	
	std::array<real, 2> curBoundingBoxPos = {{0, 0}};
	std::array<real, 2> curBoundingBoxDim = {{0, 0}};
	
	
	std::vector<const svgdom::Styleable*> styleStack;
		
	const svgdom::StylePropertyValue* getStyleProperty(svgdom::StyleProperty_e p);
	
	class PushStyles{
		Renderer& r;
	public:
		PushStyles(Renderer& r, const svgdom::Styleable& s);
		~PushStyles()noexcept;
	};
	
	class SetTempCairoContext{
		cairo_t* oldCr = nullptr;
		cairo_surface_t* surface = nullptr;
		Renderer& renderer;

		real opacity;
	public:
		SetTempCairoContext(Renderer& renderer);
		~SetTempCairoContext()noexcept;
	};
	
	real lengthToPx(const svgdom::Length& l, unsigned coordIndex = 0)const noexcept;
	
	void applyCairoTransformation(const svgdom::Transformable::Transformation& t);
	
	void applyCairoTransformations(const decltype(svgdom::Transformable::transformations)& transformations);
	
	void setCairoPatternSource(cairo_pattern_t* pat, const svgdom::Gradient& g);
	
	void setGradient(const svgdom::Element* gradientElement);
	
	void updateCurBoundingBox();
	
	void renderCurrentShape();
	
	void applyViewBox(const svgdom::ViewBoxed& e);
	
	void renderSvgElement(
			const svgdom::SvgElement& e,
			const svgdom::Length& width,
			const svgdom::Length& height
		);
	
public:
	Renderer(cairo_t* cr, real dpi, std::array<real, 2> canvasSize);
	
	void visit(const svgdom::GElement& e)override;
	void visit(const svgdom::UseElement& e)override;
	void visit(const svgdom::SvgElement& e)override;
	void visit(const svgdom::PathElement& e)override;
	void visit(const svgdom::CircleElement& e) override;
	void visit(const svgdom::PolylineElement& e) override;
	void visit(const svgdom::PolygonElement& e) override;
	void visit(const svgdom::LineElement& e) override;
	void visit(const svgdom::EllipseElement& e) override;
	void visit(const svgdom::RectElement& e) override;
};



}
