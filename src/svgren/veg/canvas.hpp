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

#pragma once

#include <array>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <vector>

#include <r4/matrix.hpp>
#include <r4/rectangle.hpp>
#include <rasterimage/image.hpp>
#include <svgdom/elements/gradients.hpp>
#include <svgdom/elements/styleable.hpp>
#include <utki/config.hpp>

#include "config.hpp"
#include "gradient.hpp"

#if VEG_BACKEND == VEG_BACKEND_CAIRO
#	if CFG_OS == CFG_OS_WINDOWS || CFG_OS_NAME == CFG_OS_NAME_IOS
#		include <cairo.h>
#	else
#		include <cairo/cairo.h>
#	endif
#elif VEG_BACKEND == VEG_BACKEND_AGG
#	include <agg/agg_rendering_buffer.h>
#	include <agg/agg_pixfmt_rgba.h>
#	include <agg/agg_renderer_base.h>
#	include <agg/agg_renderer_scanline.h>
#	include <agg/agg_scanline_u.h>
#	include <agg/agg_rasterizer_scanline_aa.h>
#	include <agg/agg_curves.h>
#	include <agg/agg_conv_stroke.h>
#	include <agg/agg_span_allocator.h>
#	include <agg/agg_span_interpolator_linear.h>
#endif

namespace veg {

/**
 * @brief Path fill rule.
 */
enum class fill_rule {
	nonzero,
	evenodd
};

/**
 * @brief Stroke line cap.
 */
enum class line_cap {
	butt,
	round,
	square
};

/**
 * @brief Stroke line join.
 */
enum class line_join {
	miter,
	round,
	bevel
};

/**
 * @brief Canvas for drawing vector graphics.
 * The canvas rasterizes and draws vector graphics to an underlying raster image (the drawing surface).
 * Coordinate system is two dimensional, x-axis right, y-axis down.
 */
class canvas
{
public:
	const r4::vector2<unsigned> dims;

private:
#if VEG_BACKEND == VEG_BACKEND_CAIRO
	std::vector<rasterimage::image<uint8_t, 4>::pixel_type> pixels;

	struct cairo_surface_wrapper {
		cairo_surface_t* surface;

		cairo_surface_wrapper(unsigned width, unsigned height, decltype(pixels)::value_type* buffer)
		{
			if (width == 0 || height == 0) {
				throw std::invalid_argument("svgren::canvas::canvas(): width or height argument is zero");
			}
			int stride = int(width) * int(sizeof(uint32_t)); // 32 bits per pixel
			this->surface = cairo_image_surface_create_for_data(
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				reinterpret_cast<unsigned char*>(buffer),
				CAIRO_FORMAT_ARGB32,
				int(width),
				int(height),
				stride
			);
			if (!this->surface) {
				throw std::runtime_error("svgren::canvas::canvas(): could not create cairo surface");
			}
		}

		cairo_surface_wrapper(const cairo_surface_wrapper&) = delete;
		cairo_surface_wrapper& operator=(const cairo_surface_wrapper&) = delete;

		cairo_surface_wrapper(cairo_surface_wrapper&&) = delete;
		cairo_surface_wrapper& operator=(cairo_surface_wrapper&&) = delete;

		~cairo_surface_wrapper()
		{
			cairo_surface_destroy(this->surface);
		}
	} surface;

	cairo_t* cr;
#elif VEG_BACKEND == VEG_BACKEND_AGG

	struct group {
		image_type image;
		agg::rendering_buffer rendering_buffer;
		agg::pixfmt_rgba32_pre pixel_format; // use premultiplied pixel format for faster blending
		agg::renderer_base<decltype(pixel_format)> renderer_base;

		group(const r4::vector2<unsigned>& dims) :
			image(dims, 0),
			rendering_buffer(
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				reinterpret_cast<agg::int8u*>(this->image.span().data()),
				this->image.dims().x(),
				this->image.dims().y(),
				int(this->image.span().stride_bytes())
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
	r4::vector2<real> subpath_start_point = 0;

	mutable agg::path_storage polyline_path; // this path stores only move_to and line_to path commands
	void agg_path_to_polyline() const;

	void agg_invalidate_polyline()
	{
		this->polyline_path.remove_all();
	}

#endif

	struct context_type {
#if VEG_BACKEND == VEG_BACKEND_CAIRO
#elif VEG_BACKEND == VEG_BACKEND_AGG
		agg::rgba color = agg::rgba(0);
		real line_width = 1;
		agg::filling_rule_e fill_rule = agg::filling_rule_e::fill_even_odd;
		agg::line_cap_e line_cap = agg::line_cap_e::butt_cap;
		agg::line_join_e line_join = agg::line_join_e::miter_join;
		agg::trans_affine gradient_matrix;
		agg::trans_affine matrix;
		real dash_offset = 0;
		std::vector<std::pair<real, real>> dash_array;
#endif
		std::shared_ptr<const gradient>
			grad; // this is needed at least to save shared pointer to gradient object to prevent its deletion
	} context;

	bool has_current_point() const;

public:
	/**
	 * @brief Construct a canvas with the underlying raster image of the specified dimensions.
	 * @param dims - dimensions of the underlying raster image in pixels.
	 */
	canvas(const r4::vector2<unsigned>& dims);

	canvas(const canvas&) = delete;
	canvas& operator=(const canvas&) = delete;

	canvas(canvas&&) = delete;
	canvas& operator=(canvas&&) = delete;

	~canvas();

	/**
	 * @brief Apply matrix transformation.
	 * Multiply current matrix by given matrix from the left.
	 * M = matrix * M
	 * @param matrix - matrixy to multiply by from the left.
	 */
	void transform(const r4::matrix2<real>& matrix);

	/**
	 * @brief Apply translation.
	 * Multiply current matrix by translation matrix from the left.
	 * M = T * M
	 * @param x - translation along x-axis.
	 * @param y - translation along y-axis.
	 */
	void translate(real x, real y);

	/**
	 * @brief Apply translation.
	 * Multiply current matrix by translation matrix from the left.
	 * M = T * M
	 * @param v - translation vector.
	 */
	void translate(const r4::vector2<real>& v)
	{
		this->translate(v.x(), v.y());
	}

	/**
	 * @brief Apply rotation.
	 * Multiply current matrix by rotation matrix from the left.
	 * M = R * M
	 * @param radians - rotation angle in radians.
	 */
	void rotate(real radians);

	/**
	 * @brief Apply scale.
	 * Multiply current matrix by scale matrix from the left.
	 * M = S * M
	 * @param x - scale along x-axis.
	 * @param y - scale along y-axis.
	 */
	void scale(real x, real y);

	/**
	 * @brief Apply scale.
	 * Multiply current matrix by scale matrix from the left.
	 * M = S * M
	 * @param v - scale vector.
	 */
	void scale(const r4::vector2<real>& v)
	{
		this->scale(v.x(), v.y());
	}

	/**
	 * @brief Set fill rule.
	 * @param fr - fill rule to use.
	 */
	void set_fill_rule(fill_rule fr);

	/**
	 * @brief Set paint source as single color.
	 * @brief rgba - color to use as paint source.
	 */
	void set_source(const r4::vector4<real>& rgba);

	/**
	 * @brief Set paint source as gradient.
	 * @param g - gradient to use as paint source.
	 */
	void set_source(std::shared_ptr<const gradient> g);

	/**
	 * @brief Muiltiply given vector by current matrix.
	 * V' = M * V
	 * @param v - vector to multiply.
	 * @return result of the multiplication.
	 */
	r4::vector2<real> matrix_mul(const r4::vector2<real>& v) const;

	/**
	 * @brief Multiply given vector by current matrix without translation part.
	 * V' = M * V
	 * @param v - vector to multiply.
	 * @return result of the multiplication.
	 */
	r4::vector2<real> matrix_mul_distance(const r4::vector2<real>& v) const;

	/**
	 * @brief Get bounding box of current shape.
	 * @return rectangle representing bounding box of the current shape.
	 */
	r4::rectangle<real> get_shape_bounding_box() const;

	/**
	 * @brief Get current point.
	 * @return current point.
	 */
	r4::vector2<real> get_current_point() const;

	/**
	 * @brief Move current point to absolute coordinates.
	 * @param p - new coordinates of the current point.
	 */
	void move_abs(const r4::vector2<real>& p);

	/**
	 * @brief Move current point relatively to current point.
	 * @param p - vector to move the current point by.
	 */
	void move_rel(const r4::vector2<real>& p);

	/**
	 * @brief Add a line to the current shape.
	 * Add a straight line segment to the current shape starting at current point and ending at specified absolute
	 * coordinates. The current point is updated to the other end of the line segment.
	 * @param p - absolute coordinates of the line segment end.
	 */
	void line_abs(const r4::vector2<real>& p);

	/**
	 * @brief Add a line to the current shape.
	 * Add a straight line segment to the current shape starting at current point and ending at specified coordinates
	 * relative to the current point. The current point is updated to the other end of the line segment.
	 * @param p - relative coordinates of the line segment end.
	 */
	void line_rel(const r4::vector2<real>& p);

	/**
	 * @brief Add a quadratic curve to the current shape.
	 * Add a quadratic curve to the current shape. The quadratic curve is specified by the current point as start
	 * point, one control point and an end point. Control point and an end point are specified in absolute coordinates.
	 * The current point is updated to the end point of the curve.
	 * @param cp1 - control point of the quadratic curve.
	 * @param ep - end point of the quadratic curve.
	 */
	void quadratic_curve_abs(
		const r4::vector2<real>& cp1, //
		const r4::vector2<real>& ep
	);

	/**
	 * @brief Add a quadratic curve to the current shape.
	 * Add a quadratic curve to the current shape. The quadratic curve is specified by the current point as start
	 * point, one control point and an end point. Control point and an end point are specified in coordinates relative
	 * to the current point. The current point is updated to the end point of the curve.
	 * @param cp1 - control point of the quadratic curve.
	 * @param ep - end point of the quadratic curve.
	 */
	void quadratic_curve_rel(
		const r4::vector2<real>& cp1, //
		const r4::vector2<real>& ep
	);

	/**
	 * @brief Add a cubic curve to the current shape.
	 * Add a cubic curve to the current shape. The cubic curve is specified by the current point as start
	 * point, two control points and an end point. Control points and an end point are specified in absolute
	 * coordinates. The current point is updated to the end point of the curve.
	 * @param cp1 - first control point of the cubic curve.
	 * @param cp2 - second control point of the cubic curve.
	 * @param ep - end point of the cubic curve.
	 */
	void cubic_curve_abs(
		const r4::vector2<real>& cp1, //
		const r4::vector2<real>& cp2,
		const r4::vector2<real>& ep
	);

	/**
	 * @brief Add a cubic curve to the current shape.
	 * Add a cubic curve to the current shape. The cubic curve is specified by the current point as start
	 * point, two control points and an end point. Control points and an end point are specified in coordinates relative
	 * to the current point. The current point is updated to the end point of the curve.
	 * @param cp1 - first control point of the cubic curve.
	 * @param cp2 - second control point of the cubic curve.
	 * @param ep - end point of the cubic curve.
	 */
	void cubic_curve_rel(
		const r4::vector2<real>& cp1, //
		const r4::vector2<real>& cp2,
		const r4::vector2<real>& ep
	);

	/**
	 * @brief Add a circle arc to the current shape.
	 * Add a circle arc to the current shape. The arc is specified by center point, radius, start angle and sweep angle.
	 * All coordinates are absolute.
	 * The current point is updated to the end point of the arc.
	 * @param center - coordinates of the arc center point.
	 * @param radius - radius of the arc.
	 * @param start_angle - start angle of the arc in radians.
	 * @param sweep_angle - sweep angle of the arc in radians.
	 */
	void arc_abs(
		const r4::vector2<real>& center, //
		const r4::vector2<real>& radius,
		real start_angle,
		real sweep_angle
	);

	/**
	 * @brief Add an elliptic arc to the current shape.
	 * Add an elliptic arc to the current shape. The arc is specified by end point, ellipse's x-axis radius, ellipse's
	 * y-axis radius, angle of the ellipse's x-axis rotation, large arc flag, sweep direction flag. All coordinates are
	 * absolute. The current point is updated to the end point of the arc.
	 * @param end_point - end point of the arc.
	 * @param radius - x-axis and y-axis radiuses of the ellipse.
	 * @param x_axis_rotation - rotation of the ellipse's x-axis in radians.
	 * @param large_arc - large arc flag: true = draw large arc, flase = draw small arc.
	 * @param sweep - sweep direction: true = clockwise, false = counter-clockwise.
	 */
	void arc_abs(
		const r4::vector2<real>& end_point,
		const r4::vector2<real>& radius,
		real x_axis_rotation,
		bool large_arc,
		bool sweep
	);

	/**
	 * @brief Add an elliptic arc to the current shape.
	 * Add an elliptic arc to the current shape. The arc is specified by end point, ellipse's x-axis radius, ellipse's
	 * y-axis radius, angle of the ellipse's x-axis rotation, large arc flag, sweep direction flag. All coordinates are
	 * relative to the current point. The current point is updated to the end point of the arc.
	 * @param end_point - end point of the arc.
	 * @param radius - x-axis and y-axis radiuses of the ellipse.
	 * @param x_axis_rotation - rotation of the ellipse's x-axis in radians.
	 * @param large_arc - large arc flag: true = draw large arc, flase = draw small arc.
	 * @param sweep - sweep direction: true = clockwise, false = counter-clockwise.
	 */
	void arc_rel(
		const r4::vector2<real>& end_point,
		const r4::vector2<real>& radius,
		real x_axis_rotation,
		bool large_arc,
		bool sweep
	);

	/**
	 * @brief Close the path of the current shape.
	 * Add a straight line segment to the current shape which connects the current point with the start point of the
	 * current shape. The current point is updated to the start point of the current shape.
	 */
	void close_path();

	/**
	 * @brief Clears current shape.
	 * Make the current shape empty.
	 */
	void clear_path();

	/**
	 * @brief Add a rectangle to the current shape.
	 * All the coordinates are absolute.
	 * @param rect - rectangle position and dimensions.
	 * @param corner_radius - x-axis and y-axis radiuses of the rectangle's rounded corners.
	 */
	void rectangle(
		const r4::rectangle<real>& rect, //
		const r4::vector2<real>& corner_radius = 0
	);

	/**
	 * @brief Add a circle to the current shape.
	 * All the coordinates are absolute.
	 * @param center - center point of the circle.
	 * @param radius - radius of the circle.
	 */
	void circle(
		const r4::vector2<real>& center, //
		real radius
	);

	/**
	 * @brief Fill the current shape.
	 * Actually performs rasterization of the current shape by filling it with the current paint source using the
	 * current fill rule. The current shape is preserved.
	 */
	void fill();

#if VEG_BACKEND == VEG_BACKEND_AGG

private:
	template <class path_type>
	void agg_stroke(path_type& vertex_source);

public:
#endif

	/**
	 * @brief Stroke the current shape.
	 * Actually performs rasterization of the current shape by stroking along the shape's path with the current paint
	 * source using the current line cap, line join, line width and dash pattern. The current shape is preserved.
	 */
	void stroke();

	/**
	 * @brief Set line width.
	 * @param width - the line width to use for stroke operation.
	 */
	void set_line_width(real width);

	/**
	 * @brief Set line cap.
	 * @param lc - the line cap to use for stroke operation.
	 */
	void set_line_cap(line_cap lc);

	/**
	 * @brief Set line join.
	 * @param lj - the line join to use for stroke operation.
	 */
	void set_line_join(line_join lj);

	/**
	 * @brief Set dash pattern.
	 * The dash pattern to use for stroke operation.
	 * @param dashes - array of dash and gap lengths. If number of values is odd,
	 *                 then the array conents is effectively repeated twice. Negative values are an error.
	 *                 Empty list means no dashing, the stroke line will be solid.
	 * @param offset - initial dash pattern offset. Can be negative.
	 */
	void set_dash_pattern(
		utki::span<const real> dashes, //
		real offset
	);

	/**
	 * @brief Get the current transformation matrix.
	 * @return current transformation matrix.
	 */
	r4::matrix2<real> get_matrix() const;

	/**
	 * @brief Set current transformation matrix.
	 * @param m - new transformation matrix.
	 */
	void set_matrix(const r4::matrix2<real>& m);

	/**
	 * @brief Push drawing group.
	 * Creates a new drawing surface (raster image) of the same dimensions as the canvas and sets it as current drawing
	 * surface. I.e. pushes a new group to the group stack.
	 */
	void push_group();

	/**
	 * @brief Merge current drawing surface with previous one.
	 * Blends contents of the current drawing surface with the previous drawing surface.
	 * The current drawing surface is discarded. The previous drawing surface is set as current.
	 * I.e. pops a group from the groups stack.
	 * @param opacity - blending factor from [0:1]: 1 = completely replace contents of previous drawing surface,
	 *                  0 = do not change the previous drawing surface.
	 */
	void pop_group(real opacity);

	/**
	 * @brief Pop group with mask.
	 * Blend the previous drawing surface with the previous-previous drawing surface using current drawing surface as a
	 * mask. The current drawing surface contents are turned into luminance values and used as per-pixel blending
	 * factors (alpha-mask). Effectively, removes two groups from the top of the group stack.
	 */
	void pop_mask_and_group();

	/**
	 * @brief Get image span of the current drawing surface.
	 * @return image_span of the current drawing surface.
	 */
	image_span_type get_image_span();

	/**
	 * @brief Release the current drawing surface.
	 * After this call the canvas remains in invalid state and cannot be used further.
	 * @return raster image of the current drawing surface.
	 */
	image_type release();
};

} // namespace veg
