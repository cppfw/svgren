#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>

#include <utki/config.hpp>

#include <r4/matrix2.hpp>
#include <r4/rectangle.hpp>

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

	void transform(const r4::matrix2<real>& matrix);
	void translate(real x, real y);
	void translate(const r4::vector2<real>& v){
		this->translate(v.x(), v.y());
	}
	void rotate(real radians);
	void scale(real x, real y);
	void scale(const r4::vector2<real>& v){
		this->scale(v.x(), v.y());
	}

	enum class fill_rule{
		even_odd,
		winding
	};

	void set_fill_rule(fill_rule fr);

	void set_source(real r, real g, real b, real a);

	r4::vector2<real> matrix_mul(const r4::vector2<real>& v);

	// multiply by current matrix without translation part
	r4::vector2<real> matrix_mul_distance(const r4::vector2<real>& v);

	r4::rectangle<real> get_shape_bounding_box()const;

	bool has_current_point()const;
	r4::vector2<real> get_current_point()const;

	std::vector<uint32_t> release();
};

}
