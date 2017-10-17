#pragma once

#include <svgdom/Visitor.hpp>

#include "Renderer.hxx"

namespace svgren{

struct FilterResult{
	std::vector<std::uint8_t> data;
	Surface surface;
};

class FilterApplyer : public svgdom::ConstVisitor{
	Renderer& r;

	decltype(svgdom::FilterElement::primitiveUnits) primitiveUnits;
	
	CanvasRegion filterRegion;
	
	std::map<std::string, FilterResult> results;
	
	FilterResult* lastResult = nullptr;
	
	Surface getSource(const std::string& in);
	void setResult(const std::string& name, FilterResult&& result);
	
	Surface getSourceGraphic();
	
public:
	
	Surface getLastResult();
	
	FilterApplyer(Renderer& r) : r(r) {}
	
	void visit(const svgdom::FilterElement& e)override;
	void visit(const svgdom::FeGaussianBlurElement& e)override;
};

}
