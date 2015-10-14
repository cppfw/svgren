/* 
 * File:   render.cpp
 * Author: ivan
 * 
 * Created on October 12, 2015, 11:04 PM
 */

#include "render.hpp"


using namespace svgren;




std::vector<std::uint32_t> svgren::render(const svgdom::SvgElement& svg, unsigned width, unsigned height){
	std::vector<std::uint32_t> ret(width * height);
	
	for(auto& c : ret){
		c = 0xffff00;
	}
	//TODO:
	
	return ret;
}
