/*
The MIT License (MIT)

Copyright (c) 2015-2024 Ivan Gagis <igagis@gmail.com>

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

#include <vector>

#include <svgdom/elements/aspect_ratioed.hpp>
#include <svgdom/util/finder_by_id.hpp>
#include <svgdom/util/style_stack.hpp>
#include <svgdom/util/style_stack_cache.hpp>
#include <svgdom/visitor.hpp>
#include <utki/config.hpp>
#include <veg/canvas.hpp>

#include "config.hxx"
#include "surface.hxx"
#include "util.hxx"

namespace svgren {

class renderer : public svgdom::const_visitor
{
public:
	veg::canvas& canvas;

	svgdom::finder_by_id finder_by_id;
	svgdom::style_stack_cache style_stack_cache;

	const real dpi;

	bool is_outermost_element = true;

	r4::vector2<real> viewport;

	// this bounding box is used for gradients
	r4::rectangle<real> user_space_bounding_box{};

	// this bounding box is used for filter region calculation.
	r4::segment2<real> device_space_bounding_box{};

	svgdom::style_stack style_stack;

	surface background; // for accessing background image from filter effects

	void blit(const surface& s);

	real length_to_px(const svgdom::length& l) const noexcept;
	r4::vector2<real> length_to_px(const svgdom::length& x, const svgdom::length& y) const noexcept;

	void apply_transformation(const svgdom::transformable::transformation& t);

	void apply_transformations(const decltype(svgdom::transformable::transformations) & transformations);

	void set_gradient_properties(
		veg::gradient& gradient, //
		const svgdom::gradient& g,
		const svgdom::style_stack& ss
	);

	void set_gradient(const std::string& id);

	void apply_filter(const std::string& id);
	void apply_filter();

	void update_bounding_box();

	void render_shape(bool is_group_pushed);

	void apply_viewbox(const svgdom::view_boxed& e, const svgdom::aspect_ratioed& ar);

	void render_svg_element(
		const svgdom::container& c,
		const svgdom::styleable& s,
		const svgdom::view_boxed& v,
		const svgdom::aspect_ratioed& a,
		const svgdom::length& x,
		const svgdom::length& y,
		const svgdom::length& width,
		const svgdom::length& height
	);

	const decltype(svgdom::transformable::transformations) & gradient_get_transformations(const svgdom::gradient& g);
	svgdom::coordinate_units gradient_get_units(const svgdom::gradient& g);

	svgdom::length gradient_get_x1(const svgdom::linear_gradient_element& g);
	svgdom::length gradient_get_y1(const svgdom::linear_gradient_element& g);
	svgdom::length gradient_get_x2(const svgdom::linear_gradient_element& g);
	svgdom::length gradient_get_y2(const svgdom::linear_gradient_element& g);

	svgdom::length gradient_get_cx(const svgdom::radial_gradient_element& g);
	svgdom::length gradient_get_cy(const svgdom::radial_gradient_element& g);
	svgdom::length gradient_get_r(const svgdom::radial_gradient_element& g);
	svgdom::length gradient_get_fx(const svgdom::radial_gradient_element& g);
	svgdom::length gradient_get_fy(const svgdom::radial_gradient_element& g);

	const decltype(svgdom::container::children) & gradient_get_stops(const svgdom::gradient& g);
	const decltype(svgdom::styleable::styles) & gradient_get_styles(const svgdom::gradient& g);
	const decltype(svgdom::styleable::classes) & gradient_get_classes(const svgdom::gradient& g);
	svgdom::gradient::spread_method gradient_get_spread_method(const svgdom::gradient& g);

	decltype(svgdom::styleable::presentation_attributes) //
	gradient_get_presentation_attributes(const svgdom::gradient& g);

	bool is_invisible();
	bool is_group_invisible();

public:
	renderer(
		veg::canvas& canvas, //
		unsigned dpi,
		r4::vector2<real> viewport,
		const svgdom::svg_element& root
	);

	// declare public method which calls protected one.
	void relay_accept(const svgdom::container& e)
	{
		this->const_visitor::relay_accept(e);
	}

	void visit(const svgdom::g_element& e) override;
	void visit(const svgdom::use_element& e) override;
	void visit(const svgdom::svg_element& e) override;
	void visit(const svgdom::path_element& e) override;
	void visit(const svgdom::circle_element& e) override;
	void visit(const svgdom::polyline_element& e) override;
	void visit(const svgdom::polygon_element& e) override;
	void visit(const svgdom::line_element& e) override;
	void visit(const svgdom::ellipse_element& e) override;
	void visit(const svgdom::rect_element& e) override;
	void visit(const svgdom::style_element& e) override;
	void visit(const svgdom::defs_element& e) override;

	void default_visit(const svgdom::element& e, const svgdom::container& c) override
	{
		// do nothing by default
	}
};

} // namespace svgren
