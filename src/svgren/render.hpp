#pragma once

#include <svgdom/dom.hpp>

#include <r4/vector2.hpp>

namespace svgren{

/**
 * @brief SVG render parameters.
 */
struct parameters{
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
struct result{
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
result render(const svgdom::svg_element& svg, const parameters& p = parameters());

}
