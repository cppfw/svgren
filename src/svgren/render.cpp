#include "render.hpp"

#include <cmath>

#include <utki/config.hpp>
#include <utki/util.hpp>

#include "Renderer.hxx"
#include "config.hpp"

using namespace svgren;



std::vector<std::uint32_t> svgren::render(const svgdom::SvgElement& svg, unsigned& width, unsigned& height, real dpi, bool bgra){
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
	
Result svgren::render(const svgdom::SvgElement& svg, const Parameters& p){
	Result ret;
	
	real w = svg.width.toPx(p.dpi);
	real h = svg.height.toPx(p.dpi);

	if(w <= 0 && svg.viewBox[2] > 0){
		w = svg.viewBox[2];
	}
	
	if(h <= 0 && svg.viewBox[3] > 0){
		h = svg.viewBox[3];
	}
	
	if(w <= 0 || h <= 0){
		return ret;
	}
	
	if(p.widthRequest == 0 && p.heightRequest == 0){
		ret.width = unsigned(std::ceil(w));
		ret.height = unsigned(std::ceil(h));
	}else{
		real aspectRatio = svg.aspectRatio(p.dpi);
		if (aspectRatio == 0){
			return ret;
		}
		ASSERT(aspectRatio > 0)
		if(p.widthRequest == 0 && p.heightRequest != 0){
			ret.width = unsigned(std::round(aspectRatio * real(p.heightRequest)));
			ret.width = std::max(ret.width, unsigned(1));//we don't want zero width
			ret.height = p.heightRequest;
		}else if(p.widthRequest != 0 && p.heightRequest == 0){
			ret.height = unsigned(std::round(real(p.widthRequest) / aspectRatio));
			ret.height = std::max(ret.height, unsigned(1));//we don't want zero height
			ret.width = p.widthRequest;
		}else{
			ASSERT(p.widthRequest != 0)
			ASSERT(p.heightRequest != 0)
			ret.width = p.widthRequest;
			ret.height = p.heightRequest;
		}
	}
	
	ASSERT(ret.width != 0)
	ASSERT(ret.height != 0)
	ASSERT(w > 0)
	ASSERT(h > 0)
	
	int stride = ret.width * sizeof(std::uint32_t);
	
//	TRACE(<< "width = " << ret.width << " height = " << ret.height << " stride = " << stride / 4 << std::endl)
	
	ret.pixels.resize((stride / sizeof(std::uint32_t)) * ret.height);
	
	for(auto& c : ret.pixels){
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
			reinterpret_cast<unsigned char*>(&*ret.pixels.begin()),
			CAIRO_FORMAT_ARGB32,
			ret.width,
			ret.height,
			stride
		);
	if(!surface){
		ret.pixels.clear();
		return ret;
	}
	utki::ScopeExit scopeExitSurface([&surface](){
		cairo_surface_destroy(surface);
	});
	
	cairo_t* cr = cairo_create(surface);
	if(!cr){
		ret.pixels.clear();
		return ret;
	}
	utki::ScopeExit scopeExitContext([&cr](){
		cairo_destroy(cr);
	});
	
	cairo_scale(cr, real(ret.width) / w, real(ret.height) / h);
	
	Renderer r(cr, p.dpi, {{w, h}}, svg);
	
	svg.accept(r);
	
	//swap Red and Blue
	if(!p.bgra){
		for(auto& c : ret.pixels){
			c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
		}
	}
	
	//unpremultiply alpha
	for(auto &c : ret.pixels){
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
