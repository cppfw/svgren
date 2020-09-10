#pragma once

#include <svgdom/dom.hpp>

namespace svgren{

/**
 * @brief SVG render parameters.
 */
struct parameters{
	/**
	 * @brief Width request for the resulting raster image.
	 * If width request is set to 0 then the width will be adjusted to preserve
	 * aspect ratio of the SVG image or determined from the SVG root element if
	 * height request is also set to zero.
	 */
	unsigned width_request = 0;
	
	/**
	 * @brief Height request for the resulting raster image.
	 * If height request is set to 0 then the height will be adjusted to preserve
	 * aspect ratio of the SVG image or determined from the SVG root element if
	 * width request is also set to zero.
	 */
	unsigned height_request = 0;
	
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
	 * @brief Resulting width of the raster image.
	 */
	unsigned width = 0;
	
	/**
	 * @brief Resulting height of the raster image.
	 */
	unsigned height = 0;
};

/**
 * @brief Render SVG image to raster image.
 * @param svg - the SVG document object model to render.
 * @param p - render parameters.
 * @return Rendering result.
 */
result render(const svgdom::svg_element& svg, const parameters& p = parameters());

}
