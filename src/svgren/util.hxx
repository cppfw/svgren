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

#include <array>
#include <cmath>

#include <r4/segment2.hpp>
#include <svgdom/elements/element.hpp>
#include <svgdom/length.hpp>
#include <svgdom/visitor.hpp>
#include <utki/config.hpp>
#include <utki/math.hpp>

#include "canvas.hxx"
#include "config.hxx"
#include "surface.hxx"

namespace svgren {

class canvas_matrix_push
{
	r4::matrix2<real> m;
	veg::canvas& c;

public:
	canvas_matrix_push(veg::canvas& c);

	canvas_matrix_push(const canvas_matrix_push&) = delete;
	canvas_matrix_push& operator=(const canvas_matrix_push&) = delete;

	canvas_matrix_push(canvas_matrix_push&&) = delete;
	canvas_matrix_push& operator=(canvas_matrix_push&&) = delete;

	~canvas_matrix_push() noexcept;
};

real percent_to_fraction(const svgdom::length& l);

class renderer_viewport_push
{
	class renderer& r;
	r4::vector2<real> old_viewport;

public:
	renderer_viewport_push(renderer& r, const decltype(old_viewport) & viewport);

	renderer_viewport_push(const renderer_viewport_push&) = delete;
	renderer_viewport_push& operator=(const renderer_viewport_push&) = delete;

	renderer_viewport_push(renderer_viewport_push&&) = delete;
	renderer_viewport_push& operator=(renderer_viewport_push&&) = delete;

	~renderer_viewport_push() noexcept;
};

class common_element_push
{
	bool group_pushed;
	surface old_background;
	class svgren::renderer& renderer;

	real opacity = real(1);

	const svgdom::element* mask_element = nullptr;

	canvas_matrix_push matrix_push;

	r4::segment2<real> old_device_space_bounding_box;

public:
	common_element_push(svgren::renderer& renderer, bool is_container);

	common_element_push(const common_element_push&) = delete;
	common_element_push& operator=(const common_element_push&) = delete;

	common_element_push(common_element_push&&) = delete;
	common_element_push& operator=(common_element_push&&) = delete;

	~common_element_push() noexcept;

	bool is_group_pushed() const noexcept
	{
		return this->group_pushed;
	}
};

struct gradient_caster : public svgdom::const_visitor {
	const svgdom::linear_gradient_element* linear = nullptr;
	const svgdom::radial_gradient_element* radial = nullptr;
	const svgdom::gradient* gradient = nullptr;

	void visit(const svgdom::linear_gradient_element& e) override
	{
		this->gradient = &e;
		this->linear = &e;
	}

	void visit(const svgdom::radial_gradient_element& e) override
	{
		this->gradient = &e;
		this->radial = &e;
	}
};

} // namespace svgren
