/* 
 * File:   render.hpp
 * Author: ivan
 *
 * Created on October 12, 2015, 11:04 PM
 */

#pragma once

#include <svgdom/dom.hpp>


namespace svgren{

std::vector<std::uint32_t> render(const svgdom::SvgElement& svg, unsigned width, unsigned height);

}//~namespace
