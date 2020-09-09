#pragma once

#include <vector>
#include <cstdint>

#include <utki/config.hpp>

#include "config.hpp"

#define SVGREN_BACKEND_CAIRO 1
#define SVGREN_BACKEND_SKIA 2

#define SVGREN_BACKEND SVGREN_BACKEND_CAIRO

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#	if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#		include <cairo.h>
#	else
#		include <cairo/cairo.h>
#	endif
#endif

namespace svgren{

class canvas{

public:
	std::vector<uint32_t> data;

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	struct cairo_surface_wrapper{
		cairo_surface_t* surface;

		cairo_surface_wrapper(unsigned width, unsigned height){
			int stride = ret.width * sizeof(uint32_t);
			this->surface = cairo_image_surface_create_for_data(
				reinterpret_cast<unsigned char*>(ret.pixels.data()),
				CAIRO_FORMAT_ARGB32,
				ret.width,
				ret.height,
				stride
			);
		}
	}

	cairo_t* cr;
#endif

	canvas(unsigned width, unsigned height);
	~canvas();

};

}
