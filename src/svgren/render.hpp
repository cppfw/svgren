/* 
 * File:   render.hpp
 * Author: ivan
 *
 * Created on October 12, 2015, 11:04 PM
 */

#pragma once

#include <svgdom/dom.hpp>

#include "config.hpp"

namespace svgren{

/**
 * @brief Render SVG to memory surface.
 * @param svg - SVG document root.
 * @param dpi - dots per inch to use for units conversion to pixels.
 * @param width - width of the resulting raster image. If 0 then width is taken from SVG document trying to preserve aspect ratio.
 * @param height height of the resulting raster image. If 0 then height is taken from SVG document trying to preserve aspect ratio.
 * @return An array of RGBA values representing the resulting raster image.
 */
std::vector<std::uint32_t> render(const svgdom::SvgElement& svg, real dpi = 96, unsigned width = 0, unsigned height = 0);

}//~namespace
