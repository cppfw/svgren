#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>

#include <utki/config.hpp>

#include <r4/matrix2.hpp>
#include <r4/rectangle.hpp>

#include <svgdom/elements/styleable.hpp>
#include <svgdom/elements/gradients.hpp>

#include "config.hxx"
#include "surface.hxx"

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#	if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#		include <cairo.h>
#	else
#		include <cairo/cairo.h>
#	endif
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
#	include <agg/agg_rendering_buffer.h>
#	include <agg/agg_pixfmt_rgba.h>
#	include <agg/agg_renderer_base.h>
#	include <agg/agg_renderer_scanline.h>
#	include <agg/agg_path_storage.h>
#	include <agg/agg_scanline_u.h>
#	include <agg/agg_rasterizer_scanline_aa.h>
#	include <agg/agg_curves.h>
#	include <agg/agg_conv_stroke.h>
#	include <agg/agg_gradient_lut.h>
#	include <agg/agg_span_gradient.h>
#	include <agg/agg_span_allocator.h>
#	include <agg/agg_span_interpolator_linear.h>
#endif

namespace svgren{

inline r4::vector4<unsigned> to_rgba(pixel c){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	// cairo uses BGRA format
	return {
		unsigned((c >> 16) & 0xff),
		unsigned((c >> 8) & 0xff),
		unsigned((c >> 0) & 0xff),
		unsigned((c >> 24) & 0xff)
	};
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// RGBA format
	return {
		unsigned((c >> 0) & 0xff),
		unsigned((c >> 8) & 0xff),
		unsigned((c >> 16) & 0xff),
		unsigned((c >> 24) & 0xff)
	};
#endif
}

inline pixel to_pixel(const r4::vector4<unsigned>& rgba){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	// cairo uses BGRA format
	return rgba.b() | (rgba.g() << 8) | (rgba.r() << 16) | (rgba.a() << 24);
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// RGBA format
	return rgba.r() | (rgba.g() << 8) | (rgba.b() << 16) | (rgba.a() << 24);
#endif
}

class canvas{
public:
	const r4::vector2<unsigned> dims;

	class gradient{
		friend class canvas;

	protected:
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		cairo_pattern_t* pattern;
		
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
		struct gradient_wrapper_base{
			virtual int calculate(int x, int y, int) const = 0;
			virtual ~gradient_wrapper_base(){}
		};
		template <class T> struct gradient_wrapper : public gradient_wrapper_base{
			T g;
			int calculate(int x, int y, int d)const override{
				return this->g.calculate(x, y, d);
			}
		};

		template <class T, template <class> class Spread> struct spread_gradient_wrapper : public gradient_wrapper_base{
			T g;
			Spread<T> sg{this->g};
			int calculate(int x, int y, int d)const override{
				return this->sg.calculate(x, y, d);
			}
		};

		const gradient_wrapper_base& pad;
		const gradient_wrapper_base& reflect;
		const gradient_wrapper_base& repeat;

		const gradient_wrapper_base* cur_grad;

		const gradient_wrapper_base& get_agg_gradient()const{
			ASSERT(this->cur_grad)
			return *this->cur_grad;
		}

		agg::gradient_lut<agg::color_interpolator<agg::rgba8>, 0x2ff> lut;

		r4::matrix2<real> local_matrix;

		gradient(
				const gradient_wrapper_base& pad,
				const gradient_wrapper_base& reflect,
				const gradient_wrapper_base& repeat
			) :
				pad(pad),
				reflect(reflect),
				repeat(repeat),
				cur_grad(&this->pad)
		{}
#endif
	public:

		struct stop{
			r4::vector4<real> rgba;
			real offset;
		};

		void set_spread_method(svgdom::gradient::spread_method spread_method);
		void set_stops(utki::span<const stop> stops);

		virtual ~gradient();
	};

	class linear_gradient : public gradient{
#if SVGREN_BACKEND == SVGREN_BACKEND_AGG
		gradient_wrapper<agg::gradient_x> linear_pad;
		spread_gradient_wrapper<agg::gradient_x, agg::gradient_reflect_adaptor> linear_reflect;
		spread_gradient_wrapper<agg::gradient_x, agg::gradient_repeat_adaptor> linear_repeat;
#endif
	public:
		linear_gradient(
				const r4::vector2<real>& p0,
				const r4::vector2<real>& p1
			);
	};

	class radial_gradient : public gradient{
#if SVGREN_BACKEND == SVGREN_BACKEND_AGG
		gradient_wrapper<agg::gradient_radial_focus> radial_pad;
		spread_gradient_wrapper<agg::gradient_radial_focus, agg::gradient_reflect_adaptor> radial_reflect;
		spread_gradient_wrapper<agg::gradient_radial_focus, agg::gradient_repeat_adaptor> radial_repeat;
#endif
	public:
		radial_gradient(
				const r4::vector2<real>& f,
				const r4::vector2<real>& c,
				real r
			);
	};

private:
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	typedef double backend_real;

	std::vector<pixel> pixels;

	struct cairo_surface_wrapper{
		cairo_surface_t* surface;

		cairo_surface_wrapper(unsigned width, unsigned height, pixel* buffer){
			if(width == 0 || height == 0){
				throw std::invalid_argument("svgren::canvas::canvas(): width or height argument is zero");
			}
			int stride = width * sizeof(pixel);
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
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	typedef agg::path_storage::container_type::value_type backend_real;

	struct group{
		std::vector<pixel> pixels;
		agg::rendering_buffer rendering_buffer;
		agg::pixfmt_rgba32_pre pixel_format; // use premultiplied pixel format for faster blending
		agg::renderer_base<decltype(pixel_format)> renderer_base;

		group(const r4::vector2<unsigned>& dims) :
				pixels(dims.x() * dims.y(), 0),
				rendering_buffer(
						reinterpret_cast<agg::int8u*>(this->pixels.data()),
						dims.x(),
						dims.y(),
						dims.x() * sizeof(decltype(this->pixels)::value_type)
					),
				pixel_format(this->rendering_buffer),
				renderer_base(this->pixel_format)
		{}
	};

	// NOTE: the groups stack has to be std::list because agg's structures are non-moveable and non-copyable
	std::list<group> group_stack;

	const real approximation_scale = 10;

	void agg_render(agg::rasterizer_scanline_aa<>& rasterizer);

	agg::path_storage path; // this path stores path commands, including bezier curve commands
	r4::vector2<real> subpath_start_point{0};

	mutable agg::path_storage polyline_path; // this path stores only move_to and line_to path commands
	void agg_path_to_polyline()const;
	void agg_invalidate_polyline(){
		this->polyline_path.remove_all();
	}

	agg::trans_affine matrix;

#endif

	struct context_type{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
		agg::rgba color = agg::rgba(0);
		real line_width = 1;
		agg::filling_rule_e fill_rule = agg::filling_rule_e::fill_even_odd;
		agg::line_cap_e line_cap = agg::line_cap_e::butt_cap;
		agg::line_join_e line_join = agg::line_join_e::miter_join;
		agg::trans_affine gradient_matrix;
#endif
		std::shared_ptr<const gradient> grad; // this is needed at least to save shared pointer to gradient object to prevent its deletion
	} context;

	bool has_current_point()const;
public:

	canvas(const r4::vector2<unsigned>& dims);
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

	void set_fill_rule(svgdom::fill_rule fr);

	void set_source(const r4::vector4<real>& rgba);
	void set_source(std::shared_ptr<const gradient> g);

	r4::vector2<real> matrix_mul(const r4::vector2<real>& v)const;

	// multiply by current matrix without translation part
	r4::vector2<real> matrix_mul_distance(const r4::vector2<real>& v)const;

	r4::rectangle<real> get_shape_bounding_box()const;

	r4::vector2<real> get_current_point()const;

	void move_to_abs(const r4::vector2<real>& p);
	void move_to_rel(const r4::vector2<real>& p);

	void line_to_abs(const r4::vector2<real>& p);
	void line_to_rel(const r4::vector2<real>& p);

	void quadratic_curve_to_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& ep);
	void quadratic_curve_to_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& ep);

	void cubic_curve_to_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep);
	void cubic_curve_to_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep);

	void arc_abs(const r4::vector2<real>& center, const r4::vector2<real>& radius, real start_angle, real sweep_angle);

	void arc_abs(const r4::vector2<real>& end_point, const r4::vector2<real>& radius, real x_axis_rotation, bool large_arc, bool sweep);
	void arc_rel(const r4::vector2<real>& end_point, const r4::vector2<real>& radius, real x_axis_rotation, bool large_arc, bool sweep);

	void close_path();

	void clear_path();

	void fill();
	void stroke();

	void set_line_width(real width);
	void set_line_cap(svgdom::stroke_line_cap lc);
	void set_line_join(svgdom::stroke_line_join lj);

	void rectangle(const r4::rectangle<real>& rect, const r4::vector2<real>& corner_radius = {0});

	r4::matrix2<real> get_matrix()const;
	void set_matrix(const r4::matrix2<real>& m);

	void push_group();
	void pop_group(real opacity);
	void pop_mask_and_group();

	svgren::surface get_sub_surface(const r4::rectangle<unsigned>& region = {0, std::numeric_limits<unsigned>::max()});

	std::vector<pixel> release();
};

}
