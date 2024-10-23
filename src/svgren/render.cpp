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

	auto im = rasterize(svg, p);

	ret.dims = im.dims();

	ret.pixels.resize(im.pixels().size());

	// This deprecated render() function uses the rasterize() and then just reinterprets the result,
	// so it is OK to use reinterpret_cast here.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	auto span = utki::make_span(reinterpret_cast<const uint32_t*>(im.pixels().data()), im.pixels().size());

	std::copy(span.begin(), span.end(), ret.pixels.begin());

	return ret;
}

image_type svgren::rasterize(const svgdom::svg_element& svg, const parameters& params)
{
	auto svg_dims = svg.get_dimensions(svgdom::real(params.dpi));

	if (!svg_dims.is_positive()) {
		return {};
	}

	image_type::dimensions_type raster_dims;

	if (params.dims_request.is_zero()) {
		using std::ceil;
		raster_dims = ceil(svg_dims).to<unsigned>();
	} else {
		real aspect_ratio = svg.aspect_ratio(svgdom::real(params.dpi));
		if (aspect_ratio == 0) {
			return {};
		}
		ASSERT(aspect_ratio > 0)
		using std::round;
		using std::max;
		if (params.dims_request.x() == 0 && params.dims_request.y() != 0) {
			raster_dims.x() = unsigned(round(aspect_ratio * real(params.dims_request.y())));
			raster_dims.x() = max(raster_dims.x(), unsigned(1)); // we don't want zero width
			raster_dims.y() = params.dims_request.y();
		} else if (params.dims_request.x() != 0 && params.dims_request.y() == 0) {
			raster_dims.y() = unsigned(round(real(params.dims_request.x()) / aspect_ratio));
			raster_dims.y() = max(raster_dims.y(), unsigned(1)); // we don't want zero height
			raster_dims.x() = params.dims_request.x();
		} else {
			ASSERT(params.dims_request.is_positive())
			raster_dims = params.dims_request;
		}
	}

	ASSERT(raster_dims.is_positive())
	ASSERT(svg_dims.is_positive())

	veg::canvas canvas(raster_dims);

	canvas.scale(raster_dims.to<real>().comp_div(svg_dims.to<real>()));

	renderer r(canvas, params.dpi, svg_dims, svg);

	svg.accept(r);

	return canvas.release();
}
