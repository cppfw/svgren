/*
The MIT License (MIT)

Copyright (c) 2015-2023 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* ================ LICENSE END ================ */

#pragma once

#include <svgdom/visitor.hpp>

#include "renderer.hxx"

namespace svgren {

struct filter_result {
	// std::vector<image_type::pixel_type> data;
	image_type image;
	svgren::surface surface;

	filter_result(r4::rectangle<unsigned> surface_rect) :
		image(surface_rect.d),
		surface(
			surface_rect.p, //
			this->image.span()
		)
	{}
};

class filter_applier : public svgdom::const_visitor
{
	renderer& r;

	// TODO: naming convention
	decltype(svgdom::filter_element::primitive_units) primitiveUnits = svgdom::coordinate_units::user_space_on_use;

	r4::rectangle<unsigned> filterRegion = {0, std::numeric_limits<unsigned>::max()};

	std::map<std::string, filter_result> results;

	filter_result* lastResult = nullptr;

	surface get_source(const std::string& in);
	void set_result(const std::string& name, filter_result&& result);

	surface get_source_graphic();

public:
	surface get_last_result();

	filter_applier(renderer& r) :
		r(r)
	{}

	void visit(const svgdom::filter_element& e) override;

	void visit(const svgdom::fe_gaussian_blur_element& e) override;
	void visit(const svgdom::fe_color_matrix_element& e) override;
	void visit(const svgdom::fe_blend_element& e) override;
	void visit(const svgdom::fe_composite_element& e) override;
};

} // namespace svgren
