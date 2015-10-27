/* 
 * File:   render.hpp
 * Author: ivan
 *
 * Created on October 12, 2015, 11:04 PM
 */

#pragma once

#include <svgdom/dom.hpp>


namespace svgren{

/**
 * @brief Render SVG to memory surface.
 * @param svg - SVG document root.
 * @param dpi - dots per inch to use for units conversion to pixels.
 * @return An array of RGBA values representing the resulting raster image.
 */
std::vector<std::uint32_t> render(const svgdom::SvgElement& svg, unsigned dpi = 96);

}//~namespace
