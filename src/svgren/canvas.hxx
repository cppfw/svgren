#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>

#include <utki/config.hpp>

#include "config.hxx"

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#	if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#		include <cairo.h>
#	else
#		include <cairo/cairo.h>
#	endif
#endif

namespace svgren{

class canvas{
	std::vector<uint32_t> data;
public:

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	struct cairo_surface_wrapper{
		cairo_surface_t* surface;

		cairo_surface_wrapper(unsigned width, unsigned height, uint32_t* buffer){
			if(width == 0 || height == 0){
				throw std::logic_error("svgren::canvas::canvas(): width or height argument is zero");
			}
			int stride = width * sizeof(uint32_t);
			this->surface = cairo_image_surface_create_for_data(
				reinterpret_cast<unsigned char*>(buffer),
				CAIRO_FORMAT_ARGB32,
				width,
				height,
				stride
			);
			if(!this->surface){
				throw std::runtime_error("svgren::canvas::canvas(): could not create cairo surface");
			}
		}
		~cairo_surface_wrapper(){
			cairo_surface_destroy(this->surface);
		}
	} surface;

	cairo_t* cr;
#endif

	canvas(unsigned width, unsigned height);
	~canvas();

	void transform(const std::array<std::array<real, 3>, 2>& matrix);
	void translate(real x, real y);
	void rotate(real radians);
	void scale(real x, real y);

	enum class fill_rule{
		even_odd,
		winding
	};

	void set_fill_rule(fill_rule fr);

	std::vector<uint32_t> release();
};

}
