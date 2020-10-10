#include "render.hpp"

#include <cmath>

#include <utki/config.hpp>
#include <utki/util.hpp>

#include "renderer.hxx"
#include "config.hxx"
#include "canvas.hxx"

using namespace svgren;
	
result svgren::render(const svgdom::svg_element& svg, const parameters& p){
	result ret;
	
	auto svg_dims = svg.get_dimensions(svgdom::real(p.dpi));
	
	if(svg_dims[0] <= 0 || svg_dims[0] <= 0){
		return ret;
	}
	
	if(p.width_request == 0 && p.height_request == 0){
		using std::ceil;
		ret.width = unsigned(ceil(svg_dims[0]));
		ret.height = unsigned(ceil(svg_dims[1]));
	}else{
		real aspectRatio = svg.aspect_ratio(svgdom::real(p.dpi));
		if (aspectRatio == 0){
			return ret;
		}
		ASSERT(aspectRatio > 0)
		using std::round;
		using std::max;
		if(p.width_request == 0 && p.height_request != 0){
			ret.width = unsigned(round(aspectRatio * real(p.height_request)));
			ret.width = max(ret.width, unsigned(1)); // we don't want zero width
			ret.height = p.height_request;
		}else if(p.width_request != 0 && p.height_request == 0){
			ret.height = unsigned(round(real(p.width_request) / aspectRatio));
			ret.height = max(ret.height, unsigned(1)); // we don't want zero height
			ret.width = p.width_request;
		}else{
			ASSERT(p.width_request != 0)
			ASSERT(p.height_request != 0)
			ret.width = p.width_request;
			ret.height = p.height_request;
		}
	}
	
	ASSERT(ret.width != 0)
	ASSERT(ret.height != 0)
	ASSERT(svg_dims[0] > 0)
	ASSERT(svg_dims[1] > 0)
	
	svgren::canvas canvas(ret.width, ret.height);
	
	canvas.scale(real(ret.width) / svg_dims[0], real(ret.height) / svg_dims[1]);
	
	renderer r(canvas, p.dpi, {svg_dims[0], svg_dims[1]}, svg);
	
	svg.accept(r);
	
	ret.pixels = canvas.release();

	return ret;
}
