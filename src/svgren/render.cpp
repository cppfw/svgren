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
	
	if(svg_dims.x() <= 0 || svg_dims.y() <= 0){
		return ret;
	}
	
	if(p.dims_request.is_zero()){
		using std::ceil;
		ret.dims = ceil(svg_dims).to<unsigned>();
	}else{
		real aspect_ratio = svg.aspect_ratio(svgdom::real(p.dpi));
		if (aspect_ratio == 0){
			return ret;
		}
		ASSERT(aspect_ratio > 0)
		using std::round;
		using std::max;
		if(p.dims_request.x() == 0 && p.dims_request.y() != 0){
			ret.dims.x() = unsigned(round(aspect_ratio * real(p.dims_request.y())));
			ret.dims.x() = max(ret.dims.x(), unsigned(1)); // we don't want zero width
			ret.dims.y() = p.dims_request.y();
		}else if(p.dims_request.x() != 0 && p.dims_request.y() == 0){
			ret.dims.y() = unsigned(round(real(p.dims_request.x()) / aspect_ratio));
			ret.dims.y() = max(ret.dims.y(), unsigned(1)); // we don't want zero height
			ret.dims.x() = p.dims_request.x();
		}else{
			ASSERT(p.dims_request.is_positive())
			ret.dims = p.dims_request;
		}
	}
	
	ASSERT(ret.dims.is_positive())
	ASSERT(svg_dims.is_positive())
	
	svgren::canvas canvas(ret.dims);
	
	canvas.scale(ret.dims.to<real>().comp_div(svg_dims.to<real>()));
	
	renderer r(canvas, p.dpi, svg_dims, svg);
	
	svg.accept(r);
	
	ret.pixels = canvas.release();

	return ret;
}
