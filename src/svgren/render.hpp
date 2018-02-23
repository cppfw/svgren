#pragma once

#include <svgdom/dom.hpp>

#include "config.hpp"

namespace svgren{

/**
 * @brief SVG render parameters.
 */
struct Parameters{
	/**
	 * @brief Width request for the resulting raster image.
	 * If width request is set to 0 then the width will be adjusted to preserve
	 * aspect ratio of the SVG image or determined from the SVG root element if
	 * height request is also set to zero.
	 */
	unsigned widthRequest = 0;
	
	/**
	 * @brief Height request for the resulting raster image.
	 * If height request is set to 0 then the height will be adjusted to preserve
	 * aspect ratio of the SVG image or determined from the SVG root element if
	 * width request is also set to zero.
	 */
	unsigned heightRequest = 0;
	
	/**
	 * @brief Dots per inch to use for unit conversion to pixels.
	 */
	real dpi = 96;
	
	/**
	 * @brief Request output format as BRGA.
	 * By default the output format is RGBA.
	 */
	bool bgra = false;
};

/**
 * @brief SVG image rendering result.
 */
struct Result{
	/**
	 * @brief Array of pixels.
	 */
	std::vector<std::uint32_t> pixels;
	
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
Result render(const svgdom::SvgElement& svg, const Parameters& p = Parameters());


/**
 * @brief Render SVG to memory surface.
 * @param svg - SVG document root.
 * @param width - width request of the resulting raster image.
 *                If 0 then width is taken from SVG document trying to preserve aspect ratio.
 *                Resulting image width is returned via this reference argument.
 * @param height - height request of the resulting raster image.
 *                 If 0 then height is taken from SVG document trying to preserve aspect ratio.
 *                 Resulting image height is returned via this reference argument.
 * @param dpi - dots per inch to use for units conversion to pixels.
 * @param bgra - indicates if the output format is BGRA or RGBA. Default is RGBA.
 * @return An array of RGBA values representing the resulting raster image.
 */
//TODO: remove deprecated function
inline std::vector<std::uint32_t> render(const svgdom::SvgElement& svg, unsigned& width, unsigned& height, real dpi = 96, bool bgra = false){
	TRACE_ALWAYS(<< "svgren::render(svg, width, height, dpi, bgra): !!!DEPRECATED!!! Use svgren::render(svg, p)" << std::endl)
	Parameters p;
	p.widthRequest = width;
	p.heightRequest = height;
	p.dpi = dpi;
	p.bgra = bgra;
	auto r = render(svg, p);
	width = r.width;
	height = r.height;
	return std::move(r.pixels);
}

}
