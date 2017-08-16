#pragma once

#include <vector>

#include <utki/config.hpp>

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

#include <svgdom/Visitor.hpp>
#include <svgdom/Finder.hpp>
#include <svgdom/StyleStack.hpp>
#include <svgdom/elements/AspectRatioed.hpp>


#include "config.hpp"


namespace svgren{


class Renderer : public svgdom::ConstVisitor{
	cairo_t* cr;
	
	svgdom::Finder finder;
	
	const real dpi;
	
	std::vector<std::array<real, 2>> viewportStack;//stack of width, height
	
	std::array<real, 2> curBoundingBoxPos = {{0, 0}};
	std::array<real, 2> curBoundingBoxDim = {{0, 0}};
	
	svgdom::StyleStack styleStack;
	
	
	class SetTempCairoContext{
		cairo_t* oldCr = nullptr;
		cairo_surface_t* surface = nullptr;
		Renderer& renderer;

		real opacity = 1;
	public:
		SetTempCairoContext(Renderer& renderer);
		~SetTempCairoContext()noexcept;
	};
	
	real lengthToPx(const svgdom::Length& l, unsigned coordIndex = 0)const noexcept;
	
	void applyCairoTransformation(const svgdom::Transformable::Transformation& t);
	
	void applyCairoTransformations(const decltype(svgdom::Transformable::transformations)& transformations);
	
	void setCairoPatternSource(cairo_pattern_t& pat, const svgdom::Gradient& g, const svgdom::StyleStack& ss);
	
	void setGradient(const std::string& id);
	
	void applyFilter(const std::string& id);
	
	void updateCurBoundingBox();
	
	void renderCurrentShape();
	
	void applyViewBox(const svgdom::ViewBoxed& e, const svgdom::AspectRatioed& ar);
	
	void renderSvgElement(
			const svgdom::SvgElement& e,
			const svgdom::Length& width,
			const svgdom::Length& height
		);
	
	svgdom::Length gradientGetX1(const svgdom::LinearGradientElement& g);
	svgdom::Length gradientGetY1(const svgdom::LinearGradientElement& g);
	svgdom::Length gradientGetX2(const svgdom::LinearGradientElement& g);
	svgdom::Length gradientGetY2(const svgdom::LinearGradientElement& g);
	
	svgdom::Length gradientGetCx(const svgdom::RadialGradientElement& g);
	svgdom::Length gradientGetCy(const svgdom::RadialGradientElement& g);
	svgdom::Length gradientGetR(const svgdom::RadialGradientElement& g);
	svgdom::Length gradientGetFx(const svgdom::RadialGradientElement& g);
	svgdom::Length gradientGetFy(const svgdom::RadialGradientElement& g);
	
	const decltype(svgdom::Container::children)& gradientGetStops(const svgdom::Gradient& g);
	const svgdom::Styleable& gradientGetStyle(const svgdom::Gradient& g);
	svgdom::Gradient::SpreadMethod_e gradientGetSpreadMethod(const svgdom::Gradient& g);
	
public:
	Renderer(
			cairo_t* cr,
			real dpi,
			std::array<real, 2> canvasSize,
			const svgdom::SvgElement& root
		);
	
	void relayAccept(const svgdom::Container& e){
		this->ConstVisitor::relayAccept(e);
	}
	
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
