/*
The MIT License (MIT)

Copyright (c) 2015-2024 Ivan Gagis <igagis@gmail.com>

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

#include "canvas.hpp"

#include <algorithm>
#include <iomanip>

#include <rasterimage/operations.hpp>
#include <utki/debug.hpp>

#include "util.hpp"
#include "util.hxx"

#if VEG_BACKEND == VEG_BACKEND_CAIRO
#elif VEG_BACKEND == VEG_BACKEND_AGG
#	include <agg/agg_conv_curve.h>
#	include <agg/agg_bounding_rect.h>
#	include <agg/agg_conv_dash.h>

using agg_curve3_type = agg::curve3_div;
using agg_curve4_type = agg::curve4_div;
#endif

using namespace veg;

namespace {
// approximate 90 degree arc with bezier curve which matches the arc at 45 degree point
// and has the same tangent as an arc at 45 degree point
using std::sqrt;
const real arc_bezier_param = real(4 * (sqrt(2) - 1) / 3);
} // namespace

canvas::canvas(const r4::vector2<unsigned>& dims) :
	dims(dims)
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	,
	pixels(size_t(dims.x()) * size_t(dims.y()), {0, 0, 0, 0}),
	surface(dims.x(), dims.y(), this->pixels.data()),
	cr(cairo_create(this->surface.surface))
#elif VEG_BACKEND == VEG_BACKEND_AGG
#endif
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	if (!this->cr) {
		throw std::runtime_error("svgren::canvas::canvas(): could not create cairo context");
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	// push main drawing surface to group stack
	this->push_group();
#endif
}

// NOLINTNEXTLINE(modernize-use-equals-default, "destructor is not trivial in some build configs")
canvas::~canvas()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_destroy(this->cr);
#endif
}

rasterimage::image<uint8_t, 4> canvas::release_image()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	auto ret = rasterimage::image<uint8_t, 4>(this->dims, std::move(this->pixels));
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto ret = std::move(this->group_stack.front().image);
#endif

#if VEG_BACKEND == VEG_BACKEND_CAIRO
	ret.span().swap_red_blue();
#endif

	ret.span().unpremultiply_alpha();

	return ret;
}

#if VEG_BACKEND == VEG_BACKEND_CAIRO
#elif VEG_BACKEND == VEG_BACKEND_AGG
namespace {
agg::trans_affine to_agg_matrix(const r4::matrix2<real>& matrix)
{
	agg::trans_affine m;
	m.sx = matrix[0][0];
	m.shx = matrix[0][1];
	m.tx = matrix[0][2];
	m.shy = matrix[1][0];
	m.sy = matrix[1][1];
	m.ty = matrix[1][2];
	return m;
}
} // namespace

namespace {
r4::matrix2<real> to_r4_matrix(const agg::trans_affine& matrix)
{
	return {
		{ real(matrix.sx), real(matrix.shx), real(matrix.tx)},
		{real(matrix.shy),  real(matrix.sy), real(matrix.ty)}
	};
}
} // namespace

void canvas::agg_path_to_polyline() const
{
	if (this->polyline_path.total_vertices() != 0) {
		return;
	}

	agg::conv_curve<decltype(this->path), agg_curve3_type, agg_curve4_type> curve(
		// AGG should have had const argument for path, because path does not change,
		// but it doesn't, so we have to use const_cast
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
		const_cast<decltype(this->path) &>(this->path)
	);
	curve.approximation_scale(this->approximation_scale);

	this->polyline_path.concat_path(curve);
}

#endif

void canvas::transform(const r4::matrix2<real>& matrix)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_matrix_t m;
	m.xx = matrix[0][0];
	m.yx = matrix[1][0];
	m.xy = matrix[0][1];
	m.yy = matrix[1][1];
	m.x0 = matrix[0][2];
	m.y0 = matrix[1][2];
	cairo_transform(this->cr, &m);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo status = " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(to_agg_matrix(matrix));
#endif
}

void canvas::translate(real x, real y)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_translate(this->cr, x, y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo status = " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(agg::trans_affine_translation(x, y));
#endif
}

void canvas::rotate(real radians)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_rotate(this->cr, radians);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo status = " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(agg::trans_affine_rotation(radians));
#endif
}

void canvas::scale(real x, real y)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	if (x * y != 0) { // cairo does not allow non-invertible scaling
		cairo_scale(this->cr, x, y);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
			o << "cairo status = " << cairo_status_to_string(cairo_status(this->cr));
		})
	} else {
		LOG([&](auto& o) {
			o << "WARNING: non-invertible scaling encountered" << std::endl;
		})
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(agg::trans_affine_scaling(x, y));
#endif
}

void canvas::set_fill_rule(svgdom::fill_rule fr)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_fill_rule_t cfr = [&fr]() {
		switch (fr) {
			default:
				ASSERT(false);
			case svgdom::fill_rule::evenodd:
				return CAIRO_FILL_RULE_EVEN_ODD;
			case svgdom::fill_rule::nonzero:
				return CAIRO_FILL_RULE_WINDING;
		}
	}();
	cairo_set_fill_rule(this->cr, cfr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	switch (fr) {
		default:
			ASSERT(false)
		case svgdom::fill_rule::evenodd:
			this->context.fill_rule = agg::filling_rule_e::fill_even_odd;
			break;
		case svgdom::fill_rule::nonzero:
			this->context.fill_rule = agg::filling_rule_e::fill_non_zero;
			break;
	}
#endif
}

void canvas::set_source(const r4::vector4<real>& rgba)
{
	this->context.grad.reset();
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_set_source_rgba(
		this->cr,
		backend_real(rgba.r()),
		backend_real(rgba.g()),
		backend_real(rgba.b()),
		backend_real(rgba.a())
	);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->context.color.r = rgba.r();
	this->context.color.g = rgba.g();
	this->context.color.b = rgba.b();
	this->context.color.a = rgba.a();
	this->context.color.premultiply(); // since we use premultiplied pixel format
#endif
}

void canvas::set_source(std::shared_ptr<const gradient> g)
{
	ASSERT(g)
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_set_source(this->cr, g->pattern);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	// switch from screen coordinates to gradeint local coordinates
	ASSERT(this->get_matrix().det() != 0, [this](auto& o) {
		o << "matrix =\n" << this->get_matrix();
	}) // make sure the matrix is invertible
	auto gm = g->local_matrix * this->get_matrix().inv();

	this->context.gradient_matrix = to_agg_matrix(gm);
#endif
	this->context.grad = std::move(g);
}

r4::vector2<real> canvas::matrix_mul(const r4::vector2<real>& v) const
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	backend_real x = v.x();
	backend_real y = v.y();
	cairo_user_to_device(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto vec = v.to<decltype(this->context.matrix.sx)>();
	this->context.matrix.transform(&vec.x(), &vec.y());
	return vec.to<real>();
#endif
}

r4::vector2<real> canvas::matrix_mul_distance(const r4::vector2<real>& v) const
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	backend_real x = v.x();
	backend_real y = v.y();
	cairo_user_to_device_distance(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto vec = v.to<decltype(this->context.matrix.sx)>();
	this->context.matrix.transform_2x2(&vec.x(), &vec.y());
	return vec.to<real>();
#endif
}

r4::rectangle<real> canvas::get_shape_bounding_box() const
{
	// According to SVG spec https://www.w3.org/TR/SVG/coords.html#ObjectBoundingBox
	// "The bounding box is computed exclusive of any values for clipping, masking, filter effects, opacity and
	// stroke-width"

#if VEG_BACKEND == VEG_BACKEND_CAIRO
	backend_real x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	cairo_path_extents(this->cr, &x1, &y1, &x2, &y2);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	return r4::rectangle<real>{
		{     real(x1),      real(y1)},
		{real(x2 - x1), real(y2 - y1)}
	};
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->agg_path_to_polyline();
	real x1 = 0;
	real x2 = 0;
	real y1 = 0;
	real y2 = 0;
	agg::bounding_rect_single(this->polyline_path, 0, &x1, &y1, &x2, &y2);
	return {
		{     x1,      y1},
		{x2 - x1, y2 - y1}
	};
#endif
}

bool canvas::has_current_point() const
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	auto cp = cairo_has_current_point(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return bool(cp != 0);
#elif VEG_BACKEND == VEG_BACKEND_AGG
	return this->path.total_vertices() != 0;
#endif
}

r4::vector2<real> canvas::get_current_point() const
{
	if (!this->has_current_point()) {
		return 0;
	}
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	backend_real xx = 0, yy = 0;
	cairo_get_current_point(this->cr, &xx, &yy);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return {real(xx), real(yy)};
#elif VEG_BACKEND == VEG_BACKEND_AGG
	return {real(this->path.last_x()), real(this->path.last_y())};
#endif
}

void canvas::move_abs(const r4::vector2<real>& p)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_move_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.move_to(p.x(), p.y());
	this->subpath_start_point = p;
	this->agg_invalidate_polyline();
#endif
}

void canvas::move_rel(const r4::vector2<real>& p)
{
	if (!this->has_current_point()) {
		this->move_abs(p);
		return;
	}
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_rel_move_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->move_abs(this->get_current_point() + p);
#endif
}

void canvas::line_abs(const r4::vector2<real>& p)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_line_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.line_to(p.x(), p.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::line_rel(const r4::vector2<real>& p)
{
	if (!this->has_current_point()) {
		this->line_abs(p);
		return;
	}
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_rel_line_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.line_rel(p.x(), p.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::quadratic_curve_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& ep)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	backend_real x0 = 0, y0 = 0; // current point, absolute coordinates
	if (cairo_has_current_point(this->cr)) {
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_get_current_point(this->cr, &x0, &y0);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	} else {
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_move_to(this->cr, 0, 0);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		x0 = 0;
		y0 = 0;
	}

	constexpr auto one_third = 1.0 / 3.0;
	constexpr auto two_thirds = one_third * 2;

	cairo_curve_to(
		this->cr,
		two_thirds * backend_real(cp1.x()) + one_third * x0,
		two_thirds * backend_real(cp1.y()) + one_third * y0,
		two_thirds * backend_real(cp1.x()) + one_third * backend_real(ep.x()),
		two_thirds * backend_real(cp1.y()) + one_third * backend_real(ep.y()),
		backend_real(ep.x()),
		backend_real(ep.y())
	);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.curve3(cp1.x(), cp1.y(), ep.x(), ep.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::quadratic_curve_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& ep)
{
	if (!this->has_current_point()) {
		this->quadratic_curve_abs(cp1, ep);
		return;
	}
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	constexpr auto one_third = 1.0 / 3.0;
	constexpr auto two_thirds = one_third * 2;

	cairo_rel_curve_to(
		this->cr,
		two_thirds * backend_real(cp1.x()),
		two_thirds * backend_real(cp1.y()),
		two_thirds * backend_real(cp1.x()) + one_third * backend_real(ep.x()),
		two_thirds * backend_real(cp1.y()) + one_third * backend_real(ep.y()),
		backend_real(ep.x()),
		backend_real(ep.y())
	);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	auto d = this->get_current_point();
	this->quadratic_curve_abs(cp1 + d, ep + d);
#endif
}

void canvas::cubic_curve_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_curve_to(
		this->cr,
		backend_real(cp1.x()),
		backend_real(cp1.y()),
		backend_real(cp2.x()),
		backend_real(cp2.y()),
		backend_real(ep.x()),
		backend_real(ep.y())
	);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.curve4(cp1.x(), cp1.y(), cp2.x(), cp2.y(), ep.x(), ep.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::cubic_curve_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep)
{
	if (!this->has_current_point()) {
		this->cubic_curve_abs(cp1, cp2, ep);
		return;
	}
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_rel_curve_to(
		this->cr,
		backend_real(cp1.x()),
		backend_real(cp1.y()),
		backend_real(cp2.x()),
		backend_real(cp2.y()),
		backend_real(ep.x()),
		backend_real(ep.y())
	);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	auto d = this->get_current_point();
	this->cubic_curve_abs(cp1 + d, cp2 + d, ep + d);
#endif
}

void canvas::close_path()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_close_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.close_polygon();
	this->move_abs(this->subpath_start_point);
	this->agg_invalidate_polyline();
#endif
}

void canvas::clear_path()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_new_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.remove_all();
	this->subpath_start_point.set(0);
	this->agg_invalidate_polyline();
#endif
}

void canvas::arc_abs(
	const r4::vector2<real>& center,
	const r4::vector2<real>& radius,
	real start_angle,
	real sweep_angle
)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	canvas_matrix_push matrix_push(*this);

	this->translate(center);
	this->scale(radius);

	if (sweep_angle >= 0) {
		cairo_arc(this->cr, 0, 0, 1, backend_real(start_angle), backend_real(start_angle + sweep_angle));
	} else {
		cairo_arc_negative(this->cr, 0, 0, 1, backend_real(start_angle), backend_real(start_angle + sweep_angle));
	}
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	agg::bezier_arc shape(center.x(), center.y(), radius.x(), radius.y(), start_angle, sweep_angle);

	this->path.join_path(shape);
	this->agg_invalidate_polyline();
#endif
}

void canvas::arc_abs(
	const r4::vector2<real>& end_point,
	const r4::vector2<real>& radius,
	real x_axis_rotation,
	bool large_arc,
	bool sweep
)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	auto cur_p = this->get_current_point();

	if (radius.x() <= 0) {
		return;
	}
	ASSERT(radius.x() > 0)
	auto radii_ratio = radius.y() / radius.x();

	if (radii_ratio <= 0) {
		return;
	}

	r4::vector2<real> end_p = end_point - cur_p; // relative end point

	// cancel rotation of end point
	end_p.rotate(-x_axis_rotation);

	ASSERT(radii_ratio > 0)
	end_p.y() /= radii_ratio;

	// find the angle between the end point and the x axis
	auto angle = get_angle(end_p);

	using std::sqrt;

	// put the end point onto the x axis
	end_p.x() = end_p.norm();
	end_p.y() = 0;

	using std::max;

	// update the x radius if it is too small
	auto rx = max(radius.x(), end_p.x() / real(2));

	// find one circle center
	r4::vector2<real> center = {end_p.x() / real(2), sqrt(utki::pow2(rx) - utki::pow2(end_p.x() / real(2)))};

	// choose between the two circles according to flags
	if (!(large_arc ^ sweep)) {
		center.y() = -center.y();
	}

	// put the second point and the center back to their positions
	end_p = r4::vector2<real>{end_p.x(), real(0)}.rot(angle);
	center.rotate(angle);

	auto angle1 = get_angle(-center);
	auto angle2 = get_angle(end_p - center);

	if (sweep) {
		// make sure angle1 is smaller than angle2
		if (angle1 > angle2) {
			angle1 -= 2 * real(utki::pi);
		}
	} else {
		// make sure angle2 is smaller than angle1
		if (angle2 > angle1) {
			angle2 -= 2 * real(utki::pi);
		}
	}

	canvas_matrix_push matrix_push(*this);
	this->translate(cur_p);
	this->rotate(x_axis_rotation);
	this->scale(1, radii_ratio);

	this->arc_abs(center, rx, angle1, angle2 - angle1);

#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->path.arc_to(radius.x(), radius.y(), x_axis_rotation, large_arc, sweep, end_point.x(), end_point.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::arc_rel(
	const r4::vector2<real>& end_point,
	const r4::vector2<real>& radius,
	real x_axis_rotation,
	bool large_arc,
	bool sweep
)
{
	this->arc_abs(end_point + this->get_current_point(), radius, x_axis_rotation, large_arc, sweep);
}

#if VEG_BACKEND == VEG_BACKEND_AGG
void canvas::agg_render(agg::rasterizer_scanline_aa<>& rasterizer)
{
	agg::scanline_u8 scanline;
	if (!this->context.grad) {
		agg::renderer_scanline_aa_solid<decltype(group::renderer_base)> renderer(this->group_stack.back().renderer_base
		);
		renderer.color(this->context.color);
		agg::render_scanlines(rasterizer, scanline, renderer);
	} else {
		agg::span_interpolator_linear<const decltype(this->context.gradient_matrix)> span_interpolator{
			this->context.gradient_matrix
		};

		agg::span_gradient<
			decltype(group::pixel_format)::color_type,
			decltype(span_interpolator),
			const gradient::gradient_wrapper_base,
			const decltype(this->context.grad->lut)>
			span_gradient(
				span_interpolator,
				this->context.grad->get_agg_gradient(),
				this->context.grad->lut,
				0,
				decltype(gradient::lut)::color_lut_size
			);

		agg::span_allocator<decltype(group::pixel_format)::color_type> span_allocator;

		ASSERT(!this->group_stack.empty())
		agg::render_scanlines_aa(
			rasterizer,
			scanline,
			this->group_stack.back().renderer_base,
			span_allocator,
			span_gradient
		);
	}
}
#endif

void canvas::fill()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_fill_preserve(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())

	this->agg_path_to_polyline();

	agg::conv_transform<decltype(this->polyline_path), decltype(this->context.matrix)> transformed_path(
		this->polyline_path,
		this->context.matrix
	);

	agg::rasterizer_scanline_aa<> rasterizer;
	rasterizer.filling_rule(this->context.fill_rule);
	rasterizer.add_path(transformed_path);

	this->agg_render(rasterizer);
#endif
}

#if VEG_BACKEND == VEG_BACKEND_AGG
template <class path_type>
void canvas::agg_stroke(path_type& vertex_source)
{
	agg::conv_stroke<path_type> stroke_path(vertex_source);

	stroke_path.width(this->context.line_width);
	stroke_path.line_cap(this->context.line_cap);
	stroke_path.line_join(this->context.line_join);
	stroke_path.approximation_scale(this->approximation_scale);

	ASSERT(!this->group_stack.empty())
	agg::conv_transform<decltype(stroke_path), decltype(this->context.matrix)> transformed_path(
		stroke_path,
		this->context.matrix
	);

	agg::rasterizer_scanline_aa<> rasterizer;
	rasterizer.add_path(transformed_path);

	this->agg_render(rasterizer);
}
#endif

void canvas::stroke()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_stroke_preserve(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->agg_path_to_polyline();

	if (!this->context.dash_array.empty()) {
		// draw dashed line

		agg::conv_dash<decltype(this->polyline_path)> dashed_path(this->polyline_path);

		// TRACE(<< "dasharray.size() = " << this->context.dash_array.size() << std::endl)
		real dash_length = 0;
		for (auto& d : this->context.dash_array) {
			// TRACE(<< "dash = (" << d.first << ", " << d.second << ")" << std::endl)
			dashed_path.add_dash(d.first, d.second);
			dash_length += d.first;
			dash_length += d.second;
		}

		// TRACE(<< "dash offset = " << this->context.dash_offset << std::endl)

		// in case no dashes are added to the dashed_path and dash offset is non-zero the dash_start() function will
		// hang, so call the dash_start() only after all the add_dash() calls are done
		if (this->context.dash_offset >= 0) {
			dashed_path.dash_start(backend_real(this->context.dash_offset));
		} else {
			dashed_path.dash_start(backend_real(dash_length + this->context.dash_offset));
		}

		this->agg_stroke(dashed_path);
	} else {
		// no dashing

		this->agg_stroke(this->polyline_path);
	}
#endif
}

void canvas::rectangle(const r4::rectangle<real>& rect, const r4::vector2<real>& corner_radius)
{
	if (corner_radius.is_zero()) {
#if VEG_BACKEND == VEG_BACKEND_CAIRO
		cairo_rectangle(
			this->cr,
			backend_real(rect.p.x()),
			backend_real(rect.p.y()),
			backend_real(rect.d.x()),
			backend_real(rect.d.y())
		);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#else
		this->move_abs(rect.p);
		this->line_abs(rect.x2_y1());
		this->line_abs(rect.x2_y2());
		this->line_abs(rect.x1_y2());
		this->close_path();
#endif
	} else {
		this->move_abs(rect.p + r4::vector2<real>{corner_radius.x(), 0});
		this->line_abs(rect.p + r4::vector2<real>{rect.d.x() - corner_radius.x(), 0});

		this->cubic_curve_rel(
			{arc_bezier_param * corner_radius.x(), 0},
			{corner_radius.x(), corner_radius.y() * (1 - arc_bezier_param)},
			corner_radius
		);

		this->line_abs(rect.p + rect.d - r4::vector2<real>{0, corner_radius.y()});

		this->cubic_curve_rel(
			{0, arc_bezier_param * corner_radius.y()},
			{-corner_radius.x() * (1 - arc_bezier_param), corner_radius.y()},
			{-corner_radius.x(), corner_radius.y()}
		);

		this->line_abs(rect.p + r4::vector2<real>{corner_radius.x(), rect.d.y()});

		this->cubic_curve_rel(
			{-arc_bezier_param * corner_radius.x(), 0},
			{-corner_radius.x(), -(1 - arc_bezier_param) * corner_radius.y()},
			-corner_radius
		);

		this->line_abs(rect.p + r4::vector2<real>{0, corner_radius.y()});

		this->cubic_curve_rel(
			{0, -arc_bezier_param * corner_radius.y()},
			{(1 - arc_bezier_param) * corner_radius.x(), -corner_radius.y()},
			{corner_radius.x(), -corner_radius.y()}
		);

		this->close_path();
	}
}

void canvas::circle(const r4::vector2<real>& center, real radius)
{
	this->move_abs(center + r4::vector2<real>{radius, 0});

	this->cubic_curve_rel(
		{0, arc_bezier_param * radius},
		{-radius * (1 - arc_bezier_param), radius},
		{-radius, radius}
	);

	this->cubic_curve_rel(
		{-arc_bezier_param * radius, 0},
		{-radius, -radius * (1 - arc_bezier_param)},
		{-radius, -radius}
	);

	this->cubic_curve_rel(
		{0, -arc_bezier_param * radius},
		{radius * (1 - arc_bezier_param), -radius},
		{radius, -radius}
	);

	this->cubic_curve_rel({arc_bezier_param * radius, 0}, {radius, radius * (1 - arc_bezier_param)}, {radius, radius});

	this->close_path();
}

void canvas::set_line_width(real width)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_set_line_width(this->cr, backend_real(width));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->context.line_width = width;
#endif
}

void canvas::set_line_cap(svgdom::stroke_line_cap lc)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_line_cap_t clc = [&lc]() {
		switch (lc) {
			default:
			case svgdom::stroke_line_cap::butt:
				return CAIRO_LINE_CAP_BUTT;
			case svgdom::stroke_line_cap::round:
				return CAIRO_LINE_CAP_ROUND;
			case svgdom::stroke_line_cap::square:
				return CAIRO_LINE_CAP_SQUARE;
		}
	}();
	cairo_set_line_cap(this->cr, clc);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	switch (lc) {
		default:
			ASSERT(false)
		case svgdom::stroke_line_cap::butt:
			this->context.line_cap = agg::line_cap_e::butt_cap;
			break;
		case svgdom::stroke_line_cap::round:
			this->context.line_cap = agg::line_cap_e::round_cap;
			break;
		case svgdom::stroke_line_cap::square:
			this->context.line_cap = agg::line_cap_e::square_cap;
			break;
	}
#endif
}

void canvas::set_line_join(svgdom::stroke_line_join lj)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_line_join_t clj = [&lj]() {
		switch (lj) {
			default:
			case svgdom::stroke_line_join::miter:
				return CAIRO_LINE_JOIN_MITER;
			case svgdom::stroke_line_join::round:
				return CAIRO_LINE_JOIN_ROUND;
			case svgdom::stroke_line_join::bevel:
				return CAIRO_LINE_JOIN_BEVEL;
		}
	}();
	cairo_set_line_join(this->cr, clj);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif VEG_BACKEND == VEG_BACKEND_AGG
	switch (lj) {
		default:
			ASSERT(false)
		case svgdom::stroke_line_join::miter:
			this->context.line_join = agg::line_join_e::miter_join;
			break;
		case svgdom::stroke_line_join::bevel:
			this->context.line_join = agg::line_join_e::bevel_join;
			break;
		case svgdom::stroke_line_join::round:
			this->context.line_join = agg::line_join_e::round_join;
			break;
	}
#endif
}

r4::matrix2<real> canvas::get_matrix() const
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_matrix_t cm;
	cairo_get_matrix(this->cr, &cm);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return {
		{real(cm.xx), real(cm.xy), real(cm.x0)},
		{real(cm.yx), real(cm.yy), real(cm.y0)}
	};
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	return to_r4_matrix(this->context.matrix);
#endif
}

void canvas::set_matrix(const r4::matrix2<real>& m)
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_matrix_t cm;
	cm.xx = backend_real(m[0][0]);
	cm.xy = backend_real(m[0][1]);
	cm.x0 = backend_real(m[0][2]);
	cm.yx = backend_real(m[1][0]);
	cm.yy = backend_real(m[1][1]);
	cm.y0 = backend_real(m[1][2]);
	cairo_set_matrix(this->cr, &cm);
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix = to_agg_matrix(m);
#endif
}

veg::image_span_type canvas::get_image_span()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	auto s = cairo_get_group_target(cr);
	ASSERT(s)

	auto stride_bytes = cairo_image_surface_get_stride(s);

	ASSERT(stride_bytes % sizeof(image_type::pixel_type) == 0)

	auto dims = r4::vector2<unsigned>(
		unsigned(cairo_image_surface_get_width(s)), //
		unsigned(cairo_image_surface_get_height(s))
	);

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
	auto buffer = reinterpret_cast<image_type::pixel_type*>(cairo_image_surface_get_data(s));

	return image_type::image_span_type(
		dims, //
		stride_bytes / sizeof(image_type::pixel_type),
		buffer
	);
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto& cur_group = this->group_stack.back();

	return cur_group.image.span();
#endif
}

void canvas::push_group()
{
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_push_group(this->cr);
	if (cairo_status(this->cr) != CAIRO_STATUS_SUCCESS) {
		throw std::runtime_error("cairo_push_group() failed");
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->group_stack.emplace_back(this->dims);
#endif
}

void canvas::pop_group(real opacity)
{
	ASSERT(0 <= opacity)
	ASSERT(opacity <= 1)
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_pop_group_to_source(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})

	if (opacity < 1) {
		cairo_paint_with_alpha(this->cr, opacity);
	} else {
		cairo_paint(this->cr);
	}
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(this->group_stack.size() >= 2)

	auto& sg = this->group_stack.back();
	auto& dg = *std::next(this->group_stack.rbegin());

	if (opacity < 1) {
		dg.renderer_base.blend_from(
			sg.pixel_format,
			nullptr,
			0, // x
			0, // y
			agg::cover_type(opacity * agg::cover_full)
		);
	} else {
		dg.renderer_base.blend_from(
			sg.pixel_format,
			nullptr,
			0, // x
			0 // y
		);
	}

	this->group_stack.pop_back();
#endif
}

namespace {
void move_luminance_to_alpha(image_span_type img)
{
	for (auto line : img) {
		for (auto& px : line) {
			px.set(
				image_type::value(1),
				image_type::value(1),
				image_type::value(1),
				// we use premultiplied alpha format, so no need to multiply alpha by liminance
				rasterimage::luminance(px)
			);
		}
	}
}
} // namespace

void canvas::pop_mask_and_group()
{
	move_luminance_to_alpha(this->get_image_span());

#if VEG_BACKEND == VEG_BACKEND_CAIRO
	cairo_pattern_t* mask = cairo_pop_group(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})

	utki::scope_exit scope_exit([mask]() {
		cairo_pattern_destroy(mask);
	});

	cairo_pop_group_to_source(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})

	cairo_mask(this->cr, mask);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, [&](auto& o) {
		o << "cairo error: " << cairo_status_to_string(cairo_status(this->cr));
	})
#elif VEG_BACKEND == VEG_BACKEND_AGG
	ASSERT(this->group_stack.size() >= 2)

	auto mask = this->group_stack.back().image.span();
	auto grp = std::next(this->group_stack.rbegin())->image.span();

	// apply mask to the group by multiplying group pixels by mask alpha
	ASSERT(mask.dims() == grp.dims())

	// TODO: use std::views::zip from C++23 (invent drop-in replacement)
	for (auto mask_line_iter = mask.begin(), group_line_iter = grp.begin(); mask_line_iter != mask.end();
		 ++mask_line_iter, ++group_line_iter)
	{
		ASSERT(mask_line_iter->size() == group_line_iter->size())
		for (auto mi = mask_line_iter->begin(), gi = group_line_iter->begin(); mi != mask_line_iter->end(); ++mi, ++gi)
		{
			// extract group color and mask alpha
			auto& gc = *gi;
			auto ma = mi->a();

			// multiply group color (since we use pre-multiplied pixel format) by mask alpha
			gc.comp_operation([&ma](const auto& a) {
				return rasterimage::multiply(a, ma);
			});
		}
	}

	this->group_stack.pop_back(); // pop out mask

	this->pop_group(1); // pop masked group
#endif
}

void canvas::set_dash_pattern(utki::span<const real> dashes, real offset)
{
	const backend_real epsilon_dash = 1e-8;

#if VEG_BACKEND == VEG_BACKEND_CAIRO
	if (dashes.empty()) {
		cairo_set_dash(this->cr, nullptr, 0, 0); // disable dashing
	} else if (dashes.size() == 1) { // dashes and gaps are of equal length
		auto dash = backend_real(dashes[0]);
		cairo_set_dash(this->cr, &dash, 1, backend_real(offset));
	} else {
		unsigned num_repeats = 1 + unsigned(dashes.size() % 2); // if number of values is odd, then repeat twice
		std::vector<backend_real> dasharray(dashes.size() * num_repeats);
		auto dst = dasharray.begin();
		for (unsigned r = 0; r != num_repeats; ++r) {
			for (auto src = dashes.begin(); src != dashes.end(); ++src, ++dst) {
				ASSERT(dst != dasharray.end())
				if (*src == 0) {
					*dst = epsilon_dash;
				} else {
					*dst = decltype(dasharray)::value_type(*src);
				}
			}
		}
		ASSERT(dst == dasharray.end())
		cairo_set_dash(this->cr, dasharray.data(), int(dasharray.size()), backend_real(offset));
		if (cairo_status(this->cr) != CAIRO_STATUS_SUCCESS) {
			throw std::runtime_error("cairo_set_dash() failed. Check if there was no negative values in dashes array.");
		}
	}
#elif VEG_BACKEND == VEG_BACKEND_AGG
	this->context.dash_offset = offset;

	// TRACE(<< "dashes.size() = " << dashes.size() << std::endl)

	unsigned num_repeats =
		1 + unsigned(dashes.size() % 2); // if number of values is odd, then repeat dashes array twice
	// TRACE(<< "num_repeats = " << num_repeats << std::endl)
	this->context.dash_array.resize(dashes.size() / (3 - num_repeats));

	// TRACE(<< "dash_array.size() = " << this->context.dash_array.size() << std::endl)

	auto src = dashes.begin();
	for (auto& dst : this->context.dash_array) {
		std::array<real, 2> pair{};
		for (auto& v : pair) {
			ASSERT(src != dashes.end())
			if (*src == 0) {
				v = decltype(pair)::value_type(epsilon_dash);
			} else {
				v = *src;
			}
			++src;
			if (src == dashes.end()) {
				if (num_repeats == 2) {
					--num_repeats;
					src = dashes.begin();
				}
			}
		}
		dst = std::make_pair(pair[0], pair[1]);
	}
#endif
}
