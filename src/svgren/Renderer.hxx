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
#include "SubSurface.hxx"


namespace svgren{


class Renderer : public svgdom::ConstVisitor{
public:
	cairo_t* cr;
	
	svgdom::Finder finder;
	
	const real dpi;
	
	//TODO: refactor, get rid of stack and save old viewport on program stack
	std::vector<std::array<real, 2>> viewportStack;//stack of {width, height} pairs
	
	//this bounding box is used for gradients
	std::array<real, 2> curUserSpaceShapeBoundingBoxPos = {{0, 0}};
	std::array<real, 2> curUserSpaceShapeBoundingBoxDim = {{0, 0}};
	
	struct DeviceSpaceBoundingBox{
		int x, y;
		unsigned width, height;
	};
	
	//this bounding box is used for filter region calculation.
	DeviceSpaceBoundingBox boundingBoxStack;
	
	const std::array<real, 2>& getDeviceSpaceBoundingBoxDim()const noexcept{
		//TODO:
		return this->curUserSpaceShapeBoundingBoxDim;
	}
	
	svgdom::StyleStack styleStack;
	
	std::vector<SubSurface> backgroundStack;
	
	class PushBackgroundIfNeeded{
		bool backgroundPushed;
		Renderer& r;
		
	public:
		PushBackgroundIfNeeded(Renderer& r);
		~PushBackgroundIfNeeded()noexcept;
		
		bool isPushed()const noexcept{
			return this->backgroundPushed;
		}
	};
	
	class PushCairoGroupIfNeeded{
		bool groupPushed;
		Renderer& renderer;

		real opacity = real(1);
	public:
		PushCairoGroupIfNeeded(Renderer& renderer, bool forcePush = false);
		~PushCairoGroupIfNeeded()noexcept;
	};
	
	real lengthToPx(const svgdom::Length& l, unsigned coordIndex = 0)const noexcept;
	
	void applyCairoTransformation(const svgdom::Transformable::Transformation& t);
	
	void applyCairoTransformations(const decltype(svgdom::Transformable::transformations)& transformations);
	
	void setCairoPatternSource(cairo_pattern_t& pat, const svgdom::Gradient& g, const svgdom::StyleStack& ss);
	
	void setGradient(const std::string& id);
	
	void applyFilter(const std::string& id);
	void applyFilter();
	
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
	
	bool isInvisible();
	
public:
	Renderer(
			cairo_t* cr,
			real dpi,
			std::array<real, 2> canvasSize,
			const svgdom::SvgElement& root
		);

	//WORKAROUND: MSVS compiler complains about cannot access protected member,
	//            Declare public method which calls protected one.
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

	void defaultVisit(const svgdom::Element& e, const svgdom::Container& c) override{
		//do nothing by default
	}
};



}
