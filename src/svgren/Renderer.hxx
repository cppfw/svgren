#pragma once

#include <vector>

#include <utki/config.hpp>

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

#include <svgdom/visitor.hpp>
#include <svgdom/finder.hpp>
#include <svgdom/style_stack.hpp>
#include <svgdom/elements/aspect_ratioed.hpp>

#include "config.hpp"
#include "Surface.hxx"
#include "util.hxx"

namespace svgren{

class Renderer : public svgdom::const_visitor{
public:
	cairo_t* cr;
	
	svgdom::finder finder;
	
	const real dpi;
	
	bool isOutermostElement = true;
	
	std::array<real, 2> viewport; // width, height
	
	// this bounding box is used for gradients
	std::array<real, 2> userSpaceShapeBoundingBoxPos = {{0, 0}};
	std::array<real, 2> userSpaceShapeBoundingBoxDim = {{0, 0}};
	
	// this bounding box is used for filter region calculation.
	DeviceSpaceBoundingBox deviceSpaceBoundingBox;
	
	svgdom::style_stack styleStack;
	
	Surface background; // for accessing background image from filter effects
	
	// blit surface to current cairo surface
	void blit(const Surface& s);
	
	real lengthToPx(const svgdom::length& l, unsigned coordIndex = 0)const noexcept;
	
	void applyCairoTransformation(const svgdom::transformable::transformation& t);
	
	void applyTransformations(const decltype(svgdom::transformable::transformations)& transformations);
	
	void setCairoPatternSource(cairo_pattern_t& pat, const svgdom::gradient& g, const svgdom::style_stack& ss);
	
	void setGradient(const std::string& id);
	
	void applyFilter(const std::string& id);
	void applyFilter();
	
	void updateCurBoundingBox();
	
	void renderCurrentShape(bool isCairoGroupPushed);
	
	void applyViewBox(const svgdom::view_boxed& e, const svgdom::aspect_ratioed& ar);
	
	void renderSvgElement(
			const svgdom::container& c,
			const svgdom::styleable& s,
			const svgdom::view_boxed& v,
			const svgdom::aspect_ratioed& a,
			const svgdom::length& x,
			const svgdom::length& y,
			const svgdom::length& width,
			const svgdom::length& height
		);
	
	const decltype(svgdom::transformable::transformations)& gradientGetTransformations(const svgdom::gradient& g);
	svgdom::coordinate_units gradientGetUnits(const svgdom::gradient& g);
	
	svgdom::length gradientGetX1(const svgdom::linear_gradient_element& g);
	svgdom::length gradientGetY1(const svgdom::linear_gradient_element& g);
	svgdom::length gradientGetX2(const svgdom::linear_gradient_element& g);
	svgdom::length gradientGetY2(const svgdom::linear_gradient_element& g);
	
	svgdom::length gradientGetCx(const svgdom::radial_gradient_element& g);
	svgdom::length gradientGetCy(const svgdom::radial_gradient_element& g);
	svgdom::length gradientGetR(const svgdom::radial_gradient_element& g);
	svgdom::length gradientGetFx(const svgdom::radial_gradient_element& g);
	svgdom::length gradientGetFy(const svgdom::radial_gradient_element& g);
	
	const decltype(svgdom::container::children)& gradientGetStops(const svgdom::gradient& g);
	const decltype(svgdom::styleable::styles)& gradient_get_styles(const svgdom::gradient& g);
	const decltype(svgdom::styleable::classes)& gradient_get_classes(const svgdom::gradient& g);
	svgdom::gradient::spread_method gradientGetSpreadMethod(const svgdom::gradient& g);
	decltype(svgdom::styleable::presentation_attributes) gradient_get_presentation_attributes(const svgdom::gradient& g);
	
	bool isInvisible();
	bool isGroupInvisible();
	
public:
	Renderer(
			cairo_t* cr,
			real dpi,
			std::array<real, 2> canvasSize,
			const svgdom::svg_element& root
		);

	// WORKAROUND: MSVS compiler complains about cannot access protected member,
	//             Declare public method which calls protected one.
	void relayAccept(const svgdom::container& e){
		this->const_visitor::relay_accept(e);
	}

	void visit(const svgdom::g_element& e)override;
	void visit(const svgdom::use_element& e)override;
	void visit(const svgdom::svg_element& e)override;
	void visit(const svgdom::path_element& e)override;
	void visit(const svgdom::circle_element& e)override;
	void visit(const svgdom::polyline_element& e)override;
	void visit(const svgdom::polygon_element& e)override;
	void visit(const svgdom::line_element& e)override;
	void visit(const svgdom::ellipse_element& e)override;
	void visit(const svgdom::rect_element& e)override;
	void visit(const svgdom::style_element& e)override;

	void default_visit(const svgdom::element& e, const svgdom::container& c)override{
		// do nothing by default
	}
};

}
