#pragma once

#include <svgdom/visitor.hpp>

#include "Renderer.hxx"

namespace svgren{

struct FilterResult{
	std::vector<uint8_t> data;
	Surface surface;
};

class FilterApplier : public svgdom::const_visitor{
	Renderer& r;

	decltype(svgdom::filter_element::primitive_units) primitiveUnits;
	
	CanvasRegion filterRegion;
	
	std::map<std::string, FilterResult> results;
	
	FilterResult* lastResult = nullptr;
	
	Surface getSource(const std::string& in);
	void setResult(const std::string& name, FilterResult&& result);
	
	Surface getSourceGraphic();
	
public:
	
	Surface getLastResult();
	
	FilterApplier(Renderer& r) : r(r) {}
	
	void visit(const svgdom::filter_element& e)override;
	
	void visit(const svgdom::fe_gaussian_blur_element& e)override;
	void visit(const svgdom::fe_color_matrix_element& e)override;
	void visit(const svgdom::fe_blend_element& e)override;
	void visit(const svgdom::fe_composite_element& e)override;

};

}
