#pragma once

#include <svgdom/Visitor.hpp>

#include "Renderer.hxx"

namespace svgren{



class FilterApplyer : public svgdom::ConstVisitor{
	Renderer& r;

	decltype(svgdom::FilterElement::primitiveUnits) primitiveUnits;
	
	CanvasRegion filterRegion;
	
	struct FilterResult{
		std::vector<std::uint8_t> data;
		Surface surface;
	};
	
	std::map<std::string, FilterResult> results;
	
	//TODO:
//	Surface getSource(const std::string& in, );
//	void setResult
	
public:
	
	FilterApplyer(Renderer& r) : r(r) {}
	
	void visit(const svgdom::FilterElement& e)override;
	void visit(const svgdom::FeGaussianBlurElement& e)override;
};

}
