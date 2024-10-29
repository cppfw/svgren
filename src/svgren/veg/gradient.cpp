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

#include "gradient.hpp"

#include "util.hxx"

using namespace veg;

#if VEG_BACKEND == VEG_BACKEND_AGG
namespace {
agg::rgba to_agg_rgba(const r4::vector4<real>& rgba)
{
	return {
		agg::rgba::value_type(rgba.r()),
		agg::rgba::value_type(rgba.g()),
		agg::rgba::value_type(rgba.b()),
		agg::rgba::value_type(rgba.a())
	};
}
} // namespace
#endif

// NOLINTNEXTLINE(modernize-use-equals-default, "destructor is not trivial in some build configs")
gradient::~gradient()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	ASSERT(this->pattern)
	cairo_pattern_destroy(this->pattern);
#elif VEG_BACKEND == VEG_BACKEND_AGG
#endif
}

linear_gradient::linear_gradient(const r4::vector2<real>& p0, const r4::vector2<real>& p1)
#if VEG_BACKEND == VEG_BACKEND_AGG
	:
	gradient(this->linear_pad, this->linear_reflect, this->linear_repeat)
#endif
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	this->pattern = cairo_pattern_create_linear(
		backend_real(p0.x()),
		backend_real(p0.y()),
		backend_real(p1.x()),
		backend_real(p1.y())
	);
	if (!this->pattern) {
		throw std::runtime_error("cairo_pattern_create_linear() failed");
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	auto v = p1 - p0;
	auto len = v.norm();

	this->local_matrix.set_identity();

	// gradient needs inverse matrix, i.e. matrix which transforms screen coordinates to gradient coordiantes

	// we need to transform the [p0, p1] line segment to [0, 1] segment on X-axis
	// and then stretch it to color_lut_size length

	this->local_matrix.scale(decltype(gradient::lut)::color_lut_size, 1);
	this->local_matrix.scale(real(1) / len, 1);
	this->local_matrix.rotate(-get_angle(v));
	this->local_matrix.translate(-p0);
#endif
}

radial_gradient::radial_gradient(const r4::vector2<real>& f, const r4::vector2<real>& c, real r)
#if VEG_BACKEND == VEG_BACKEND_AGG
	:
	gradient(this->radial_pad, this->radial_reflect, this->radial_repeat)
#endif
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	this->pattern = cairo_pattern_create_radial(
		backend_real(f.x()),
		backend_real(f.y()),
		backend_real(0),
		backend_real(c.x()),
		backend_real(c.y()),
		backend_real(r)
	);
	if (!this->pattern) {
		throw std::runtime_error("cairo_pattern_create_radial() failed");
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	// gradient parameters are stretched so that the gradient radius becomes color_lut_size,
	// this is for optimal color picking from the color_lut
	real lut_r = decltype(gradient::lut)::color_lut_size;
	auto rel_f = f - c;
	auto lut_f = rel_f / r * lut_r;
	this->radial_pad.g.init(lut_r, lut_f.x(), lut_f.y());
	this->radial_reflect.g.init(lut_r, lut_f.x(), lut_f.y());
	this->radial_repeat.g.init(lut_r, lut_f.x(), lut_f.y());

	this->local_matrix.set_identity();

	// gradient needs inverse matrix, i.e. matrix which transforms screen coordinates to gradient coordiantes

	// we need to transform the gradient center to 0 point and scale it to radius of 1
	// and then stretch it to color_lut_size

	this->local_matrix.scale(lut_r);
	this->local_matrix.scale(real(1) / r);
	this->local_matrix.translate(-c);
#endif
}

void gradient::set_spread_method(gradient_spread_method spread_method)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_extend_t extend = [&spread_method]() {
		switch (spread_method) {
			default:
				ASSERT(false)
			case gradient_spread_method::pad:
				return CAIRO_EXTEND_PAD;
			case gradient_spread_method::reflect:
				return CAIRO_EXTEND_REFLECT;
			case gradient_spread_method::repeat:
				return CAIRO_EXTEND_REPEAT;
		}
	}();

	cairo_pattern_set_extend(this->pattern, extend);
	ASSERT(cairo_pattern_status(this->pattern) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	switch (spread_method) {
		default:
			ASSERT(false)
		case gradient_spread_method::pad:
			this->cur_grad = &this->pad;
			break;
		case gradient_spread_method::reflect:
			this->cur_grad = &this->reflect;
			break;
		case gradient_spread_method::repeat:
			this->cur_grad = &this->repeat;
			break;
	}
#endif
}

void gradient::set_stops(utki::span<const stop> stops)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	for (auto& s : stops) {
		cairo_pattern_add_color_stop_rgba(this->pattern, s.offset, s.rgba.r(), s.rgba.g(), s.rgba.b(), s.rgba.a());
		ASSERT(cairo_pattern_status(this->pattern) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
			o << "status = " << cairo_status_to_string(cairo_pattern_status(this->pattern)) << " offset = " << s.offset
			  << " rgba = " << s.rgba;
		})
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	if (stops.size() == 1) {
		auto color = to_agg_rgba(stops.front().rgba);
		this->lut.add_color(backend_real(0), color);
		this->lut.add_color(backend_real(1), color);
	} else {
		for (auto& s : stops) {
			this->lut.add_color(backend_real(s.offset), to_agg_rgba(s.rgba));
		}
	}

	this->lut.build_lut();

	// premultiply alpha since we use premultiplied pixel format
	this->lut.premultiply();
#endif
}
