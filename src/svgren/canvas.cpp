#include "canvas.hxx"

#include <algorithm>

#include <utki/debug.hpp>

using namespace svgren;

canvas::canvas(unsigned width, unsigned height) :
		data(
				width * height,
#ifdef M_SVGREN_BACKGROUND
				M_SVGREN_BACKGROUND
#else
				0
#endif
			)
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		, surface(width, height, this->data.data())
		, cr(cairo_create(this->surface.surface))
#endif
{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(!this->cr){
		throw std::runtime_error("svgren::canvas::canvas(): could not create cairo context");
	}
#endif
}

canvas::~canvas(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_destroy(this->cr);
#endif
}

void canvas::scale(real x, real y){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_scale(this->cr, x, y);
	ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
#endif
}

std::vector<uint32_t> canvas::release(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	for(auto &c : this->data){
		// swap red and blue channels, as cairo works in BGRA format, while we need to return RGBA
		c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);

		// unpremultiply alpha
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
#endif

	return std::move(this->data);
}