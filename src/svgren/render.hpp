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

#include <r4/vector.hpp>
#include <rasterimage/image.hpp>
#include <svgdom/dom.hpp>

namespace svgren {

using image_type = rasterimage::image<uint8_t, 4>;

/**
 * @brief SVG render parameters.
 */
struct parameters {
	/**
	 * @brief Width and height request for the resulting raster image.
	 * If width request is set to 0 then the width will be adjusted to preserve
	 * aspect ratio of the SVG image or determined from the SVG root element if
	 * height request is also set to zero. Same works for height request.
	 */
	r4::vector2<unsigned> dims_request = 0;

	/**
	 * @brief Dots per inch to use for unit conversion to pixels.
	 */
	unsigned dpi = 96;
};

/**
 * @brief SVG image rendering result.
 */
struct result {
	/**
	 * @brief Array of pixels.
	 */
	std::vector<uint32_t> pixels;

	/**
	 * @brief Resulting width and height of the raster image.
	 */
	r4::vector2<unsigned> dims = 0;
};

/**
 * @brief Render SVG image to raster image.
 * @param svg - the SVG document object model to render.
 * @param p - render parameters.
 * @return Rendering result.
 */
[[deprecated("use rasterize()")]] result render(const svgdom::svg_element& svg, const parameters& p = parameters());

image_type rasterize(const svgdom::svg_element& svg, const parameters& params = parameters());

} // namespace svgren
