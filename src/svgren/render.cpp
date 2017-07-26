#include <utki/config.hpp>
#include <utki/util.hpp>
#include <utki/math.hpp>

#include "Renderer.hxx"

#include <cmath>

#include "render.hpp"

#include "config.hpp"

using namespace svgren;



std::vector<std::uint32_t> svgren::render(const svgdom::SvgElement& svg, unsigned& width, unsigned& height, real dpi, bool bgra){
	auto w = unsigned(svg.width.toPx(dpi));
	auto h = unsigned(svg.height.toPx(dpi));

	if(w == 0 && svg.viewBox[2] > 0){
		w = unsigned(svg.viewBox[2]);
	}
	
	if(h == 0 && svg.viewBox[3] > 0){
		h = unsigned(svg.viewBox[3]);
	}
	
	if(w == 0 || h == 0){
		return std::vector<std::uint32_t>();
	}
	
	if(width == 0 && height == 0){
		width = w;
		height = h;
	}else{
		real aspectRatio = svg.aspectRatio(dpi);
		if (aspectRatio == 0){
			return std::vector<std::uint32_t>();
		}
		ASSERT(aspectRatio > 0)
		if(width == 0 && height != 0){
			width = unsigned(aspectRatio * real(height));
		}else if(width != 0 && height == 0){
			height = unsigned(real(width) / aspectRatio);
		}
	}
	
	ASSERT(width != 0)
	ASSERT(height != 0)
	ASSERT(w != 0)
	ASSERT(h != 0)
	
	int stride = width * sizeof(std::uint32_t);
	
	TRACE(<< "width = " << width << " stride = " << stride / 4 << std::endl)
	
	std::vector<std::uint32_t> ret((stride / sizeof(std::uint32_t)) * height);
	
	for(auto& c : ret){
#ifdef M_SVGREN_WHITE_BACKGROUND
		c = 0xffffffff;
#else
		c = 0;
#endif
	}
	
	utki::ScopeExit scopeExitCairoReset([]{
		cairo_debug_reset_static_data();
	});
	
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
			reinterpret_cast<unsigned char*>(&*ret.begin()),
			CAIRO_FORMAT_ARGB32,
			width,
			height,
			stride
		);
	if(!surface){
		ret.clear();
		return ret;
	}
	utki::ScopeExit scopeExitSurface([&surface](){
		cairo_surface_destroy(surface);
	});
	
	cairo_t* cr = cairo_create(surface);
	if(!cr){
		ret.clear();
		return ret;
	}
	utki::ScopeExit scopeExitContext([&cr](){
		cairo_destroy(cr);
	});
	
	cairo_scale(cr, real(width) / real(w), real(height) / real(h));
	
	Renderer r(cr, dpi, {{real(w), real(h)}});
	
	svg.accept(r);
	
	//swap Red and Blue
	if(!bgra){
		for(auto& c : ret){
			c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
		}
	}
	
	//unpremultiply alpha
	for(auto &c : ret){
		std::uint32_t a = (c >> 24);
		if(a == 0xff){
			continue;
		}
		if(a != 0){
			std::uint32_t r = (c & 0xff) * 0xff / a;
			utki::clampTop(r, std::uint32_t(0xff));
			std::uint32_t g = ((c >> 8) & 0xff) * 0xff / a;
			utki::clampTop(g, std::uint32_t(0xff));
			std::uint32_t b = ((c >> 16) & 0xff) * 0xff / a;
			utki::clampTop(b, std::uint32_t(0xff));
			c = ((a << 24) | (b << 16) | (g << 8) | r);
		}else{
			c = 0;
		}
	}
	
	return ret;
}
