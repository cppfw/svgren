#include "render.hpp"

#include <cmath>

#include <utki/config.hpp>
#include <utki/util.hpp>

#include "Renderer.hxx"
#include "config.hpp"
#include "canvas.hxx"

using namespace svgren;
	
result svgren::render(const svgdom::svg_element& svg, const parameters& p){
	result ret;
	
	auto svg_width = svg.get_dimensions(p.dpi);
	
	if(svg_width[0] <= 0 || svg_width[1] <= 0){
		return ret;
	}
	
	if(p.width_request == 0 && p.height_request == 0){
		using std::ceil;
		ret.width = unsigned(ceil(svg_width[0]));
		ret.height = unsigned(ceil(svg_width[1]));
	}else{
		real aspectRatio = svg.aspect_ratio(p.dpi);
		if (aspectRatio == 0){
			return ret;
		}
		ASSERT(aspectRatio > 0)
		if(p.width_request == 0 && p.height_request != 0){
			ret.width = unsigned(std::round(aspectRatio * real(p.height_request)));
			ret.width = std::max(ret.width, unsigned(1)); // we don't want zero width
			ret.height = p.height_request;
		}else if(p.width_request != 0 && p.height_request == 0){
			ret.height = unsigned(std::round(real(p.width_request) / aspectRatio));
			ret.height = std::max(ret.height, unsigned(1)); // we don't want zero height
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
	ASSERT(svg_width[0] > 0)
	ASSERT(svg_width[1] > 0)
	
	canvas cvs(ret.width, ret.height);

	int stride = ret.width * sizeof(uint32_t);
	
//	TRACE(<< "width = " << ret.width << " height = " << ret.height << " stride = " << stride / 4 << std::endl)
	
	ret.pixels.resize((stride / sizeof(uint32_t)) * ret.height);
	
	for(auto& c : ret.pixels){
#ifdef M_SVGREN_BACKGROUND
		c = M_SVGREN_BACKGROUND;
#else
		c = 0;
#endif
	}
	
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
			reinterpret_cast<unsigned char*>(ret.pixels.data()),
			CAIRO_FORMAT_ARGB32,
			ret.width,
			ret.height,
			stride
		);
	if(!surface){
		ret.pixels.clear();
		return ret;
	}
	utki::scope_exit scope_exit_surface([&surface](){
		cairo_surface_destroy(surface);
	});
	
	cairo_t* cr = cairo_create(surface);
	if(!cr){
		ret.pixels.clear();
		return ret;
	}
	utki::scope_exit scope_exit_context([&cr](){
		cairo_destroy(cr);
	});
	
	cairo_scale(cr, real(ret.width) / svg_width[0], real(ret.height) / svg_width[1]);
	ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
	
	Renderer r(cr, p.dpi, {{svg_width[0], svg_width[1]}}, svg);
	
	svg.accept(r);
	
	// swap Red and Blue
	if(!p.bgra){
		for(auto& c : ret.pixels){
			c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
		}
	}
	
	// unpremultiply alpha
	for(auto &c : ret.pixels){
		uint32_t a = (c >> 24);
		if(a == 0xff){
			continue;
		}
		if(a != 0){
			using std::min;
			uint32_t r = (c & 0xff) * 0xff / a;
			r = min(r, uint32_t(0xff)); // clamp top
			uint32_t g = ((c >> 8) & 0xff) * 0xff / a;
			g = min(g, uint32_t(0xff)); // clamp top
			uint32_t b = ((c >> 16) & 0xff) * 0xff / a;
			b = min(b, uint32_t(0xff)); // clamp top
			c = ((a << 24) | (b << 16) | (g << 8) | r);
		}else{
			c = 0;
		}
	}
	
	return ret;
}
