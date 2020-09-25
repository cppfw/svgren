#pragma once

#include <svgdom/visitor.hpp>

#include "renderer.hxx"

namespace svgren{

struct filter_result{
	std::vector<uint32_t> data;
	svgren::surface surface;
};

class filter_applier : public svgdom::const_visitor{
	renderer& r;

	decltype(svgdom::filter_element::primitive_units) primitiveUnits;
	
	r4::rectangle<unsigned> filterRegion = {0, std::numeric_limits<unsigned>::max()};
	
	std::map<std::string, filter_result> results;
	
	filter_result* lastResult = nullptr;
	
	surface get_source(const std::string& in);
	void set_result(const std::string& name, filter_result&& result);
	
	surface get_source_graphic();
	
public:
	
	surface get_last_result();
	
	filter_applier(renderer& r) : r(r) {}
	
	void visit(const svgdom::filter_element& e)override;
	
	void visit(const svgdom::fe_gaussian_blur_element& e)override;
	void visit(const svgdom::fe_color_matrix_element& e)override;
	void visit(const svgdom::fe_blend_element& e)override;
	void visit(const svgdom::fe_composite_element& e)override;

};

}
