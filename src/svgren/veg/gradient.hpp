/*
The MIT License (MIT)

Copyright (c) 2015-2023 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* ================ LICENSE END ================ */

#pragma once

#include "config.hpp"

#if VEG_BACKEND == VEG_BACKEND_CAIRO
#	if CFG_OS == CFG_OS_WINDOWS || CFG_OS_NAME == CFG_OS_NAME_IOS
#		include <cairo.h>
#	else
#		include <cairo/cairo.h>
#	endif
#elif VEG_BACKEND == VEG_BACKEND_AGG
// #	include <agg/agg_rendering_buffer.h>
// #	include <agg/agg_pixfmt_rgba.h>
// #	include <agg/agg_renderer_base.h>
// #	include <agg/agg_renderer_scanline.h>
// #	include <agg/agg_path_storage.h>
// #	include <agg/agg_scanline_u.h>
// #	include <agg/agg_rasterizer_scanline_aa.h>
// #	include <agg/agg_curves.h>
// #	include <agg/agg_conv_stroke.h>
#	include <agg/agg_gradient_lut.h>
#	include <agg/agg_span_gradient.h>
// #	include <agg/agg_span_allocator.h>
// #	include <agg/agg_span_interpolator_linear.h>
#endif

namespace veg {

enum class gradient_spread_method {
	pad,
	reflect,
	repeat
};

class gradient
{
	friend class canvas;

protected:
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_pattern_t* pattern = nullptr;

	gradient() = default;

#elif VEG_BACKEND == VEG_BACKEND_AGG
	struct gradient_wrapper_base {
		virtual int calculate(int x, int y, int) const = 0;

		gradient_wrapper_base() = default;

		gradient_wrapper_base(const gradient_wrapper_base&) = delete;
		gradient_wrapper_base& operator=(const gradient_wrapper_base&) = delete;

		gradient_wrapper_base(gradient_wrapper_base&&) = delete;
		gradient_wrapper_base& operator=(gradient_wrapper_base&&) = delete;

		virtual ~gradient_wrapper_base() = default;
	};

	template <class gradient_type>
	struct gradient_wrapper : public gradient_wrapper_base {
		gradient_type g;

		int calculate(int x, int y, int d) const override
		{
			return this->g.calculate(x, y, d);
		}
	};

	template <class gradient_type, template <class> class spread_templ>
	struct spread_gradient_wrapper : public gradient_wrapper_base {
		gradient_type g;
		spread_templ<gradient_type> sg{this->g};

		int calculate(int x, int y, int d) const override
		{
			return this->sg.calculate(x, y, d);
		}
	};

	const gradient_wrapper_base& pad;
	const gradient_wrapper_base& reflect;
	const gradient_wrapper_base& repeat;

	const gradient_wrapper_base* cur_grad;

	const gradient_wrapper_base& get_agg_gradient() const
	{
		ASSERT(this->cur_grad)
		return *this->cur_grad;
	}

	constexpr static auto gradient_lut_size = 0x2ff;
	agg::gradient_lut<agg::color_interpolator<agg::rgba8>, gradient_lut_size> lut;

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
	struct stop {
		r4::vector4<real> rgba;
		real offset;
	};

	void set_spread_method(gradient_spread_method spread_method);
	void set_stops(utki::span<const stop> stops);

	gradient(const gradient&) = delete;
	gradient& operator=(const gradient&) = delete;

	gradient(gradient&&) = delete;
	gradient& operator=(gradient&&) = delete;

	virtual ~gradient();
};

class linear_gradient : public gradient
{
#if VEG_BACKEND == VEG_BACKEND_AGG
	gradient_wrapper<agg::gradient_x> linear_pad;
	spread_gradient_wrapper<agg::gradient_x, agg::gradient_reflect_adaptor> linear_reflect;
	spread_gradient_wrapper<agg::gradient_x, agg::gradient_repeat_adaptor> linear_repeat;
#endif

public:
	linear_gradient(
		const r4::vector2<real>& p0, //
		const r4::vector2<real>& p1
	);
};

class radial_gradient : public gradient
{
#if VEG_BACKEND == VEG_BACKEND_AGG
	gradient_wrapper<agg::gradient_radial_focus> radial_pad;
	spread_gradient_wrapper<agg::gradient_radial_focus, agg::gradient_reflect_adaptor> radial_reflect;
	spread_gradient_wrapper<agg::gradient_radial_focus, agg::gradient_repeat_adaptor> radial_repeat;
#endif

public:
	radial_gradient(
		const r4::vector2<real>& f, //
		const r4::vector2<real>& c,
		real r
	);
};

} // namespace veg
