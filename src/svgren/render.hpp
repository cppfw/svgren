#pragma once

#include <svgdom/dom.hpp>

#include "config.hpp"

namespace svgren{

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
std::vector<std::uint32_t> render(const svgdom::SvgElement& svg, unsigned& width, unsigned& height, real dpi = 96, bool bgra = false);

}
