#include "canvas.hxx"

#include <algorithm>

#include <utki/debug.hpp>

#include "util.hxx"

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
#	include <agg2/agg_conv_curve.h>
#	include <agg2/agg_bounding_rect.h>
#endif

using namespace svgren;

canvas::canvas(unsigned width, unsigned height) :
		pixels(
				width * height,
#ifdef M_SVGREN_BACKGROUND
				M_SVGREN_BACKGROUND
#else
				0
#endif
			)
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		, surface(width, height, this->pixels.data())
		, cr(cairo_create(this->surface.surface))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
		, rendering_buffer(
				reinterpret_cast<agg::int8u*>(pixels.data()),
				width,
				height,
				width * sizeof(decltype(this->pixels)::value_type)
			)
		, pixel_format(this->rendering_buffer)
		, renderer_base(this->pixel_format)
		, renderer(renderer_base)
#endif
{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(!this->cr){
		throw std::runtime_error("svgren::canvas::canvas(): could not create cairo context");
	}
#endif
	this->clear_path();
}

canvas::~canvas(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_destroy(this->cr);
#endif
}

std::vector<uint32_t> canvas::release(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	for(auto &c : this->pixels){
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

	return std::move(this->pixels);
}

#if SVGREN_BACKEND == SVGREN_BACKEND_AGG
namespace{
agg::trans_affine to_agg_matrix(const r4::matrix2<real>& matrix){
	agg::trans_affine m;
	m.sx = matrix[0][0];  m.shx = matrix[0][1]; m.tx = matrix[0][2];
	m.shy = matrix[1][0]; m.sy = matrix[1][1];  m.ty = matrix[1][2];
	return m;
}
}

namespace{
r4::matrix2<real> to_r4_matrix(const agg::trans_affine& matrix){
	return {
			{real(matrix.sx),  real(matrix.shx), real(matrix.tx)},
			{real(matrix.shy), real(matrix.sy),  real(matrix.ty)}
		};
}
}
#endif

void canvas::transform(const r4::matrix2<real>& matrix){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_matrix_t m;
	m.xx = matrix[0][0];
	m.yx = matrix[1][0];
	m.xy = matrix[0][1];
	m.yy = matrix[1][1];
	m.x0 = matrix[0][2];
	m.y0 = matrix[1][2];
	cairo_transform(this->cr, &m);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.matrix *= to_agg_matrix(matrix);
#endif
}

void canvas::translate(real x, real y){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_translate(this->cr, x, y);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.matrix *= agg::trans_affine_translation(x, y);
#endif
}

void canvas::rotate(real radians){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rotate(this->cr, radians);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.matrix *= agg::trans_affine_rotation(radians);
#endif
}

void canvas::scale(real x, real y){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(x * y != 0){ // cairo does not allow non-invertible scaling
		cairo_scale(this->cr, x, y);
		ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
	}else{
		TRACE(<< "WARNING: non-invertible scaling encountered" << std::endl)
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.matrix *= agg::trans_affine_scaling(x, y);
#endif
}

void canvas::set_fill_rule(svgdom::fill_rule fr){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_fill_rule_t cfr;
	switch (fr){
		default:
			ASSERT(false);
		case svgdom::fill_rule::evenodd:
			cfr = CAIRO_FILL_RULE_EVEN_ODD;
			break;
		case svgdom::fill_rule::nonzero:
			cfr = CAIRO_FILL_RULE_WINDING;
			break;
	}
	cairo_set_fill_rule(this->cr, cfr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	switch(fr){
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

void canvas::set_source(const r4::vector4<real>& rgba){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_source_rgba(this->cr, double(rgba.r()), double(rgba.g()), double(rgba.b()), double(rgba.a()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.color.r = rgba.r();
	this->context.color.g = rgba.g();
	this->context.color.b = rgba.b();
	this->context.color.a = rgba.a();
#endif
}

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
namespace{
void set_cairo_pattern_properties(cairo_pattern_t* pat, const gradient& g){
	for(auto& s : g.stops){
		cairo_pattern_add_color_stop_rgba(pat, s.offset, s.rgba.r(), s.rgba.g(), s.rgba.b(), s.rgba.a());
		ASSERT(cairo_pattern_status(pat) == CAIRO_STATUS_SUCCESS)
	}

	cairo_extend_t extend;

	switch(g.spread_method){
		default:
		case svgdom::gradient::spread_method::default_:
			ASSERT(false)
		case svgdom::gradient::spread_method::pad:
			extend = CAIRO_EXTEND_PAD;
			break;
		case svgdom::gradient::spread_method::reflect:
			extend = CAIRO_EXTEND_REFLECT;
			break;
		case svgdom::gradient::spread_method::repeat:
			extend = CAIRO_EXTEND_REPEAT;
			break;
	}

	cairo_pattern_set_extend(pat, extend);
	ASSERT(cairo_pattern_status(pat) == CAIRO_STATUS_SUCCESS)
}
}
#endif

void canvas::set_source(const linear_gradient& g){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto pat = cairo_pattern_create_linear(
			double(g.p0.x()),
			double(g.p0.y()),
			double(g.p1.x()),
			double(g.p1.y())
		);
	if(!pat){
		throw std::runtime_error("cairo_pattern_create_linear() failed");
	}
	utki::scope_exit pat_scope_exit([&pat](){cairo_pattern_destroy(pat);});

	set_cairo_pattern_properties(pat, g);

	cairo_set_source(this->cr, pat);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// TODO:
#endif
}

void canvas::set_source(const radial_gradient& g){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto pat = cairo_pattern_create_radial(
			double(g.c0.x()),
			double(g.c0.y()),
			double(g.r0),
			double(g.c1.x()),
			double(g.c1.y()),
			double(g.r1)
		);
	if(!pat){
		throw std::runtime_error("cairo_pattern_create_radial() failed");
	}
	utki::scope_exit pat_scope_exit([&pat](){cairo_pattern_destroy(pat);});

	set_cairo_pattern_properties(pat, g);

	cairo_set_source(this->cr, pat);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// TODO:
#endif
}

r4::vector2<real> canvas::matrix_mul(const r4::vector2<real>& v){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x = v.x();
	double y = v.y();
	cairo_user_to_device(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	auto vec = v.to<decltype(this->context.matrix.sx)>();
	this->context.matrix.transform(&vec.x(), &vec.y());
	return vec.to<real>();
#endif
}

r4::vector2<real> canvas::matrix_mul_distance(const r4::vector2<real>& v){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x = v.x();
	double y = v.y();
	cairo_user_to_device_distance(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	auto vec = v.to<decltype(this->context.matrix.sx)>();
	this->context.matrix.transform_2x2(&vec.x(), &vec.y());
	return vec.to<real>();
#endif
}

r4::rectangle<real> canvas::get_shape_bounding_box()const{
	// According to SVG spec https://www.w3.org/TR/SVG/coords.html#ObjectBoundingBox
	// "The bounding box is computed exclusive of any values for clipping, masking, filter effects, opacity and stroke-width"

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x1, y1, x2, y2;

	cairo_path_extents(
			this->cr,
			&x1,
			&y1,
			&x2,
			&y2
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)

	return r4::rectangle<real>{
			{real(x1), real(y1)},
			{real(x2 - x1), real(y2 - y1)}
		};
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	real x1, x2, y1, y2;
	agg::bounding_rect_single(
			const_cast<std::remove_const<std::remove_pointer<decltype(this)>::type>::type*>(this)->path,
			1, // 0th vertex is always 0 point, ignore it, start from 1st vertex
			&x1,
			&y1,
			&x2,
			&y2
		);
	return {{x1, y1}, {x2 - x1, y2 - y1}};
#endif
}

r4::vector2<real> canvas::get_current_point()const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double xx, yy;
	cairo_get_current_point(this->cr, &xx, &yy);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return {real(xx), real(yy)};
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	return {
			real(this->path.last_x()),
			real(this->path.last_y())
		};
#endif
}

void canvas::move_to_abs(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_move_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.move_to(p.x(), p.y());
#endif
}

void canvas::move_to_rel(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_move_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.move_rel(p.x(), p.y());
#endif
}

void canvas::line_to_abs(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_line_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.line_to(p.x(), p.y());
#endif
}

void canvas::line_to_rel(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_line_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.line_rel(p.x(), p.y());
#endif
}

void canvas::quadratic_curve_to_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& ep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x0, y0; // current point, absolute coordinates
	if (cairo_has_current_point(this->cr)) {
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_get_current_point(this->cr, &x0, &y0);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	}
	else {
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		cairo_move_to(this->cr, 0, 0);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		x0 = 0;
		y0 = 0;
	}
	cairo_curve_to(
			this->cr,
			2.0 / 3.0 * double(cp1.x()) + 1.0 / 3.0 * x0,
			2.0 / 3.0 * double(cp1.y()) + 1.0 / 3.0 * y0,
			2.0 / 3.0 * double(cp1.x()) + 1.0 / 3.0 * double(ep.x()),
			2.0 / 3.0 * double(cp1.y()) + 1.0 / 3.0 * double(ep.y()),
			double(ep.x()),
			double(ep.y())
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg_curve3_type curve;
	curve.approximation_scale(this->approximation_scale);
	curve.angle_tolerance(agg::deg2rad(22));
	curve.cusp_limit(agg::deg2rad(0));
	curve.init(
			this->path.last_x(), this->path.last_y(),
			cp1.x(), cp1.y(),
			ep.x(), ep.y()
		);

	this->path.join_path(curve);
#endif
}

void canvas::quadratic_curve_to_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& ep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_curve_to(
			this->cr,
			2.0 / 3.0 * double(cp1.x()),
			2.0 / 3.0 * double(cp1.y()),
			2.0 / 3.0 * double(cp1.x()) + 1.0 / 3.0 * double(ep.x()),
			2.0 / 3.0 * double(cp1.y()) + 1.0 / 3.0 * double(ep.y()),
			double(ep.x()),
			double(ep.y())
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	r4::vector2<real> d{
			real(this->path.last_x()),
			real(this->path.last_y())
		};
	this->quadratic_curve_to_abs(cp1 + d, ep + d);
#endif
}

void canvas::cubic_curve_to_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_curve_to(
			this->cr,
			double(cp1.x()),
			double(cp1.y()),
			double(cp2.x()),
			double(cp2.y()),
			double(ep.x()),
			double(ep.y())
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg_curve4_type curve;
	curve.approximation_scale(this->approximation_scale);
	curve.angle_tolerance(agg::deg2rad(22));
	curve.cusp_limit(agg::deg2rad(0));
	curve.init(
			this->path.last_x(), this->path.last_y(),
			cp1.x(), cp1.y(),
			cp2.x(), cp2.y(),
			ep.x(), ep.y()
		);

	this->path.join_path(curve);
#endif
}

void canvas::cubic_curve_to_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_curve_to(
			this->cr,
			double(cp1.x()),
			double(cp1.y()),
			double(cp2.x()),
			double(cp2.y()),
			double(ep.x()),
			double(ep.y())
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	r4::vector2<real> d{
			real(this->path.last_x()),
			real(this->path.last_y())
		};

	this->cubic_curve_to_abs(cp1 + d, cp2 + d, ep + d);
#endif
}

void canvas::close_path(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_close_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	auto cp = this->get_current_point();
	this->path.close_polygon();
	this->move_to_abs(cp);
#endif
}

void canvas::clear_path(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_new_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.remove_all();
#endif
	this->move_to_abs(0); // make sure there is current point
}

void canvas::arc_abs(const r4::vector2<real>& center, const r4::vector2<real>& radius, real start_angle, real sweep_angle){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	canvas_matrix_push matrix_push(*this);

	this->translate(center);
	this->scale(radius);

	if(sweep_angle >= 0){
		cairo_arc(this->cr, 0, 0, 1, double(start_angle), double(start_angle + sweep_angle));
	}else{
		cairo_arc_negative(this->cr, 0, 0, 1, double(start_angle), double(start_angle + sweep_angle));
	}
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg::bezier_arc shape(center.x(), center.y(), radius.x(), radius.y(), start_angle, sweep_angle);
	agg::conv_curve<decltype(shape), agg_curve3_type, agg_curve4_type> curve(shape);
	curve.approximation_scale(this->approximation_scale);
	this->path.join_path(curve);
#endif
}

void canvas::arc_abs(const r4::vector2<real>& end_point, const r4::vector2<real>& radius, real x_axis_rotation, bool large_arc, bool sweep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto cur_p = this->get_current_point();

	if(radius.x() <= 0){
		return;
	}
	ASSERT(radius.x() > 0)
	auto radii_ratio = radius.y() / radius.x();

	if(radii_ratio <= 0){
		return;
	}

	r4::vector2<real> end_p = end_point - cur_p; // relative end point
	
	// cancel rotation of end point
	end_p.rotate(-x_axis_rotation);

	ASSERT(radii_ratio > 0)
	end_p.y() /= radii_ratio;

	// find the angle between the end point and the x axis
	auto angle = point_angle(real(0), end_p);

	using std::sqrt;

	// put the end point onto the x axis
	end_p.x() = end_p.norm();
	end_p.y() = 0;

	using std::max;

	// update the x radius if it is too small
	auto rx = max(radius.x(), end_p.x() / real(2));

	// find one circle center
	r4::vector2<real> center = {
		end_p.x() / real(2),
		sqrt(utki::pow2(rx) - utki::pow2(end_p.x() / real(2)))
	};

	// choose between the two circles according to flags
	if(!(large_arc ^ sweep)){
		center.y() = -center.y();
	}

	// put the second point and the center back to their positions
	end_p = r4::vector2<real>{end_p.x(), real(0)}.rot(angle);
	center.rotate(angle);

	auto angle1 = point_angle(center, real(0));
	auto angle2 = point_angle(center, end_p);

	if(sweep){
		// make sure angle1 is smaller than angle2
		if(angle1 > angle2){
			angle1 -= 2 * utki::pi<real>();
		}
	}else{
		// make sure angle2 is smaller than angle1
		if(angle2 > angle1){
			angle2 -= 2 * utki::pi<real>();
		}
	}

	canvas_matrix_push matrix_push(*this);
	this->translate(cur_p);
	this->rotate(x_axis_rotation);
	this->scale(1, radii_ratio);

	this->arc_abs(center, rx, angle1, angle2 - angle1);

#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg::bezier_arc_svg shape(
			this->path.last_x(),
			this->path.last_y(),
			radius.x(),
			radius.y(),
			x_axis_rotation,
			large_arc,
			sweep,
			end_point.x(),
			end_point.y()
		);
	agg::conv_curve<decltype(shape), agg_curve3_type, agg_curve4_type> curve(shape);
	curve.approximation_scale(this->approximation_scale);
	this->path.join_path(curve);
#endif
}

void canvas::arc_rel(const r4::vector2<real>& end_point, const r4::vector2<real>& radius, real x_axis_rotation, bool large_arc, bool sweep){
	this->arc_abs(end_point + this->get_current_point(), radius, x_axis_rotation, large_arc, sweep);
}

void canvas::fill(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_fill_preserve(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg::conv_transform<agg::path_storage, agg::trans_affine> transformed_path(this->path, this->context.matrix);

	this->rasterizer.filling_rule(this->context.fill_rule);
	this->rasterizer.add_path(transformed_path);

    this->renderer.color(this->context.color);
    agg::render_scanlines(this->rasterizer, this->scanline, this->renderer);
#endif
}

void canvas::stroke(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_stroke_preserve(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg::conv_stroke<decltype(this->path)> stroke_path(this->path);
	stroke_path.width(this->context.line_width);
	stroke_path.line_join(this->context.line_join);
	stroke_path.line_cap(this->context.line_cap);

	agg::conv_transform<decltype(stroke_path), agg::trans_affine> transformed_path(stroke_path, this->context.matrix);

	this->rasterizer.add_path(transformed_path);

    this->renderer.color(this->context.color);
    agg::render_scanlines(this->rasterizer, this->scanline, this->renderer);
#endif
}

void canvas::rectangle(const r4::rectangle<real>& rect){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rectangle(this->cr, double(rect.p.x()), double(rect.p.y()), double(rect.d.x()), double(rect.d.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->move_to_abs(rect.p);
	this->line_to_abs(rect.x2_y1());
	this->line_to_abs(rect.x2_y2());
	this->line_to_abs(rect.x1_y2());
	this->close_path();
#endif
}

void canvas::set_line_width(real width){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_line_width(this->cr, double(width));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.line_width = width;
#endif
}

void canvas::set_line_cap(svgdom::stroke_line_cap lc){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_line_cap_t clc;
	switch(lc){
		default:
		case svgdom::stroke_line_cap::butt:
			clc = CAIRO_LINE_CAP_BUTT;
			break;
		case svgdom::stroke_line_cap::round:
			clc = CAIRO_LINE_CAP_ROUND;
			break;
		case svgdom::stroke_line_cap::square:
			clc = CAIRO_LINE_CAP_SQUARE;
			break;
	}
	cairo_set_line_cap(this->cr, clc);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	switch(lc){
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

void canvas::set_line_join(svgdom::stroke_line_join lj){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_line_join_t clj;
	switch(lj){
		default:
		case svgdom::stroke_line_join::miter:
			clj = CAIRO_LINE_JOIN_MITER;
			break;
		case svgdom::stroke_line_join::round:
			clj = CAIRO_LINE_JOIN_ROUND;
			break;
		case svgdom::stroke_line_join::bevel:
			clj = CAIRO_LINE_JOIN_BEVEL;
			break;
	}
	cairo_set_line_join(this->cr, clj);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	switch(lj){
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

void canvas::push_context(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_save(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context_stack.push_back(this->context);
#endif
}

void canvas::pop_context(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_restore(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	if(this->context_stack.empty()){
		throw std::logic_error("canvas::pop_contex(): context stack is empty");
	}
	this->context = this->context_stack.back();
	this->context_stack.pop_back();
#endif
}

r4::matrix2<real> canvas::get_matrix(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_matrix_t cm;
	cairo_get_matrix(this->cr, &cm);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return {
		{real(cm.xx), real(cm.xy), real(cm.x0)},
		{real(cm.yx), real(cm.yy), real(cm.y0)}
	};
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	return to_r4_matrix(this->context.matrix);
#endif
}

void canvas::set_matrix(const r4::matrix2<real>& m){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_matrix_t cm;
	cm.xx = double(m[0][0]); cm.xy = double(m[0][1]); cm.x0 = double(m[0][2]);
	cm.yx = double(m[1][0]); cm.yy = double(m[1][1]); cm.y0 = double(m[1][2]);
	cairo_set_matrix(this->cr, &cm);
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.matrix = to_agg_matrix(m);
#endif
}

svgren::surface canvas::get_sub_surface(const r4::rectangle<unsigned>& region){
	r4::vector2<unsigned> dims;
	uint32_t* buffer;
	unsigned stride;

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	{
		auto s = cairo_get_group_target(cr);
		ASSERT(s)
		
		stride = cairo_image_surface_get_stride(s); // stride is returned in bytes
		
		dims = decltype(dims){
			unsigned(cairo_image_surface_get_width(s)),
			unsigned(cairo_image_surface_get_height(s))
		};

		buffer = reinterpret_cast<uint32_t*>(cairo_image_surface_get_data(s));
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	dims = decltype(dims){this->rendering_buffer.width(), this->rendering_buffer.height()};
	stride = this->rendering_buffer.stride_abs();
	buffer = reinterpret_cast<uint32_t*>(this->rendering_buffer.buf());
#endif

	ASSERT(buffer)
	ASSERT(stride % sizeof(uint32_t) == 0)

	svgren::surface ret;
	ret.stride = stride / sizeof(uint32_t);

	using std::min;
	ret.d = min(region.d, dims - region.p);
	ret.span = utki::make_span(
			buffer + region.p.y() * ret.stride + region.p.x(),
			ret.stride * ret.d.y() - (ret.stride - ret.d.x()) // subtract 'tail' from last pixels row
		);
	ret.p = region.p;
	
	ASSERT(ret.d.y() <= dims.y())
	ASSERT(ret.d.y() == 0 || std::next(ret.span.begin(), ret.stride * (ret.d.y() - 1)) < ret.span.end())

	return ret;
}

void canvas::push_group(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_push_group(this->cr);
	if(cairo_status(this->cr) != CAIRO_STATUS_SUCCESS){
		throw std::runtime_error("cairo_push_group() failed");
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// TODO:
#endif
}

void canvas::pop_group(real opacity){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_pop_group_to_source(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))

	if(opacity < 1){
		cairo_paint_with_alpha(this->cr, opacity);
	}else{
		cairo_paint(this->cr);
	}
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// TODO:
#endif
}

void canvas::pop_mask_and_group(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	this->get_sub_surface().append_luminance_to_alpha();

	cairo_pattern_t* mask = cairo_pop_group(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))

	utki::scope_exit scope_exit([mask](){
		cairo_pattern_destroy(mask);
	});

	cairo_pop_group_to_source(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))

	cairo_mask(this->cr, mask);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// TODO:
#endif
}
