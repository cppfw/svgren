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

#include "render.hpp"

#include <cmath>

#include <utki/config.hpp>
#include <utki/util.hpp>

#include "canvas.hxx"
#include "config.hxx"
#include "renderer.hxx"

using namespace svgren;

result svgren::render(const svgdom::svg_element& svg, const parameters& p)
{
	result ret;

	auto svg_dims = svg.get_dimensions(svgdom::real(p.dpi));

	if (!svg_dims.is_positive()) {
		return ret;
	}

	if (p.dims_request.is_zero()) {
		using std::ceil;
		ret.dims = ceil(svg_dims).to<unsigned>();
	} else {
		real aspect_ratio = svg.aspect_ratio(svgdom::real(p.dpi));
		if (aspect_ratio == 0) {
			return ret;
		}
		ASSERT(aspect_ratio > 0)
		using std::round;
		using std::max;
		if (p.dims_request.x() == 0 && p.dims_request.y() != 0) {
			ret.dims.x() = unsigned(round(aspect_ratio * real(p.dims_request.y())));
			ret.dims.x() = max(ret.dims.x(), unsigned(1)); // we don't want zero width
			ret.dims.y() = p.dims_request.y();
		} else if (p.dims_request.x() != 0 && p.dims_request.y() == 0) {
			ret.dims.y() = unsigned(round(real(p.dims_request.x()) / aspect_ratio));
			ret.dims.y() = max(ret.dims.y(), unsigned(1)); // we don't want zero height
			ret.dims.x() = p.dims_request.x();
		} else {
			ASSERT(p.dims_request.is_positive())
			ret.dims = p.dims_request;
		}
	}

	ASSERT(ret.dims.is_positive())
	ASSERT(svg_dims.is_positive())

	svgren::canvas canvas(ret.dims);

	canvas.scale(ret.dims.to<real>().comp_div(svg_dims.to<real>()));

	renderer r(canvas, p.dpi, svg_dims, svg);

	svg.accept(r);

	auto pixels = canvas.release();

	// TODO:

	ret.pixels = std::move(pixels);

	return ret;
}
