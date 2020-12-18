#include "canvas.hxx"

#include <algorithm>
#include <iomanip>

#include <utki/debug.hpp>

#include "util.hxx"

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
#	include <agg/agg_conv_curve.h>
#	include <agg/agg_bounding_rect.h>
#	include <agg/agg_conv_dash.h>

typedef agg::curve3_div agg_curve3_type;
typedef agg::curve4_div agg_curve4_type;
#endif

using namespace svgren;

canvas::canvas(const r4::vector2<unsigned>& dims) :
		dims(dims)
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		, pixels(dims.x() * dims.y(), 0)
		, surface(dims.x(), dims.y(), this->pixels.data())
		, cr(cairo_create(this->surface.surface))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
#endif
{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(!this->cr){
		throw std::runtime_error("svgren::canvas::canvas(): could not create cairo context");
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// push main drawing surface to group stack
	this->push_group();
#endif
}

canvas::~canvas(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_destroy(this->cr);
#endif
}

std::vector<pixel> canvas::release(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto& ret = this->pixels;
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto& ret = this->group_stack.front().pixels;
#endif

	for(auto &c : ret){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		// swap red and blue channels, as cairo works in BGRA format, while we need to return RGBA
		c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
#endif

		// unpremultiply alpha
		pixel a = (c >> 24);
		if(a == 0xff){
			continue;
		}
		if(a != 0){
			using std::min;
			pixel r = (c & 0xff) * 0xff / a;
			r = min(r, pixel(0xff)); // clamp top
			pixel g = ((c >> 8) & 0xff) * 0xff / a;
			g = min(g, pixel(0xff)); // clamp top
			pixel b = ((c >> 16) & 0xff) * 0xff / a;
			b = min(b, pixel(0xff)); // clamp top
			c = ((a << 24) | (b << 16) | (g << 8) | r);
		}else{
			c = 0;
		}
	}
	return std::move(ret);
}

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
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

namespace{
agg::rgba to_agg_rgba(const r4::vector4<real>& rgba){
	return agg::rgba(
			agg::rgba::value_type(rgba.r()),
			agg::rgba::value_type(rgba.g()),
			agg::rgba::value_type(rgba.b()),
			agg::rgba::value_type(rgba.a())
		);
}
}

void canvas::agg_path_to_polyline()const{
	if(this->polyline_path.total_vertices() != 0){
		return;
	}

	agg::conv_curve<decltype(this->path), agg_curve3_type, agg_curve4_type> curve(
			const_cast<decltype(this->path)&>(this->path)
		);
	curve.approximation_scale(this->approximation_scale);

	this->polyline_path.concat_path(curve);
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
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(to_agg_matrix(matrix));
#endif
}

void canvas::translate(real x, real y){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_translate(this->cr, x, y);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(agg::trans_affine_translation(x, y));
#endif
}

void canvas::rotate(real radians){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rotate(this->cr, radians);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(agg::trans_affine_rotation(radians));
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
	ASSERT(!this->group_stack.empty())
	this->context.matrix.premultiply(agg::trans_affine_scaling(x, y));
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
	this->context.grad.reset();
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_source_rgba(this->cr, backend_real(rgba.r()), backend_real(rgba.g()), backend_real(rgba.b()), backend_real(rgba.a()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.color.r = rgba.r();
	this->context.color.g = rgba.g();
	this->context.color.b = rgba.b();
	this->context.color.a = rgba.a();
	this->context.color.premultiply(); // since we use premultiplied pixel format
#endif
}

canvas::gradient::~gradient(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO	
	ASSERT(this->pattern)
	cairo_pattern_destroy(this->pattern);
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
#endif
}

canvas::linear_gradient::linear_gradient(const r4::vector2<real>& p0, const r4::vector2<real>& p1)
#if SVGREN_BACKEND == SVGREN_BACKEND_AGG
	:
		gradient(
				this->linear_pad,
				this->linear_reflect,
				this->linear_repeat
			)
#endif
{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	this->pattern = cairo_pattern_create_linear(
			backend_real(p0.x()),
			backend_real(p0.y()),
			backend_real(p1.x()),
			backend_real(p1.y())
		);
	if(!this->pattern){
		throw std::runtime_error("cairo_pattern_create_linear() failed");
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
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

canvas::radial_gradient::radial_gradient(
		const r4::vector2<real>& f,
		const r4::vector2<real>& c,
		real r
	)
#if SVGREN_BACKEND == SVGREN_BACKEND_AGG
	:
		gradient(
				this->radial_pad,
				this->radial_reflect,
				this->radial_repeat
			)
#endif
{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	this->pattern = cairo_pattern_create_radial(
			backend_real(f.x()),
			backend_real(f.y()),
			backend_real(0),
			backend_real(c.x()),
			backend_real(c.y()),
			backend_real(r)
		);
	if(!this->pattern){
		throw std::runtime_error("cairo_pattern_create_radial() failed");
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
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

void canvas::gradient::set_spread_method(svgdom::gradient::spread_method spread_method){
	ASSERT(spread_method != svgdom::gradient::spread_method::default_)
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_extend_t extend;

	switch(spread_method){
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

	cairo_pattern_set_extend(this->pattern, extend);
	ASSERT(cairo_pattern_status(this->pattern) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	switch(spread_method){
		default:
		case svgdom::gradient::spread_method::default_:
			ASSERT(false)
		case svgdom::gradient::spread_method::pad:
			this->cur_grad = &this->pad;
			break;
		case svgdom::gradient::spread_method::reflect:
			this->cur_grad = &this->reflect;
			break;
		case svgdom::gradient::spread_method::repeat:
			this->cur_grad = &this->repeat;
			break;
	}
#endif
}

void canvas::gradient::set_stops(utki::span<const stop> stops){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	for(auto& s : stops){
		cairo_pattern_add_color_stop_rgba(
				this->pattern,
				s.offset,
				s.rgba.r(),
				s.rgba.g(),
				s.rgba.b(),
				s.rgba.a()
			);
		ASSERT_INFO(cairo_pattern_status(this->pattern) == CAIRO_STATUS_SUCCESS,
				"status = " << cairo_status_to_string(cairo_pattern_status(this->pattern)) << " offset = " << s.offset << " rgba = " << s.rgba
			)
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	if(stops.size() == 1){
		this->lut.add_color(backend_real(0), to_agg_rgba(stops.rbegin()->rgba));
		this->lut.add_color(backend_real(1), to_agg_rgba(stops.rbegin()->rgba));
	}else{
		for(auto& s : stops){
			this->lut.add_color(backend_real(s.offset), to_agg_rgba(s.rgba));
		}
	}
	
	this->lut.build_lut();

	// premultiply alpha since we use premultiplied pixel format
	this->lut.premultiply();
#endif
}

void canvas::set_source(std::shared_ptr<const gradient> g){
	ASSERT(g)
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_source(this->cr, g->pattern);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	// switch from screen coordinates to gradeint local coordinates
	auto gm = g->local_matrix * this->get_matrix().inv();

	this->context.gradient_matrix = to_agg_matrix(gm);
#endif
	this->context.grad = std::move(g);
}

r4::vector2<real> canvas::matrix_mul(const r4::vector2<real>& v)const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	backend_real x = v.x();
	backend_real y = v.y();
	cairo_user_to_device(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto vec = v.to<decltype(this->context.matrix.sx)>();
	this->context.matrix.transform(&vec.x(), &vec.y());
	return vec.to<real>();
#endif
}

r4::vector2<real> canvas::matrix_mul_distance(const r4::vector2<real>& v)const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	backend_real x = v.x();
	backend_real y = v.y();
	cairo_user_to_device_distance(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	auto vec = v.to<decltype(this->context.matrix.sx)>();
	this->context.matrix.transform_2x2(&vec.x(), &vec.y());
	return vec.to<real>();
#endif
}

r4::rectangle<real> canvas::get_shape_bounding_box()const{
	// According to SVG spec https://www.w3.org/TR/SVG/coords.html#ObjectBoundingBox
	// "The bounding box is computed exclusive of any values for clipping, masking, filter effects, opacity and stroke-width"

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	backend_real x1, y1, x2, y2;

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
	this->agg_path_to_polyline();
	real x1, x2, y1, y2;
	agg::bounding_rect_single(
			this->polyline_path,
			0,
			&x1,
			&y1,
			&x2,
			&y2
		);
	return {{x1, y1}, {x2 - x1, y2 - y1}};
#endif
}

bool canvas::has_current_point()const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto cp = cairo_has_current_point(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return bool(cp != 0);
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	return this->path.total_vertices() != 0;
#endif
}

r4::vector2<real> canvas::get_current_point()const{
	if(!this->has_current_point()){
		return 0;
	}
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	backend_real xx, yy;
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
	cairo_move_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.move_to(p.x(), p.y());
	this->subpath_start_point = p;
	this->agg_invalidate_polyline();
#endif
}

void canvas::move_to_rel(const r4::vector2<real>& p){
	if(!this->has_current_point()){
		this->move_to_abs(p);
		return;
	}
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_move_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->move_to_abs(this->get_current_point() + p);
#endif
}

void canvas::line_to_abs(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_line_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.line_to(p.x(), p.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::line_to_rel(const r4::vector2<real>& p){
	if(!this->has_current_point()){
		this->line_to_abs(p);
		return;
	}
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_line_to(this->cr, backend_real(p.x()), backend_real(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.line_rel(p.x(), p.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::quadratic_curve_to_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& ep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	backend_real x0, y0; // current point, absolute coordinates
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
			2.0 / 3.0 * backend_real(cp1.x()) + 1.0 / 3.0 * x0,
			2.0 / 3.0 * backend_real(cp1.y()) + 1.0 / 3.0 * y0,
			2.0 / 3.0 * backend_real(cp1.x()) + 1.0 / 3.0 * backend_real(ep.x()),
			2.0 / 3.0 * backend_real(cp1.y()) + 1.0 / 3.0 * backend_real(ep.y()),
			backend_real(ep.x()),
			backend_real(ep.y())
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.curve3(cp1.x(), cp1.y(), ep.x(), ep.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::quadratic_curve_to_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& ep){
	if(!this->has_current_point()){
		this->quadratic_curve_to_abs(cp1, ep);
		return;
	}
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_curve_to(
			this->cr,
			2.0 / 3.0 * backend_real(cp1.x()),
			2.0 / 3.0 * backend_real(cp1.y()),
			2.0 / 3.0 * backend_real(cp1.x()) + 1.0 / 3.0 * backend_real(ep.x()),
			2.0 / 3.0 * backend_real(cp1.y()) + 1.0 / 3.0 * backend_real(ep.y()),
			backend_real(ep.x()),
			backend_real(ep.y())
		);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	auto d = this->get_current_point();
	this->quadratic_curve_to_abs(cp1 + d, ep + d);
#endif
}

void canvas::cubic_curve_to_abs(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
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
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.curve4(cp1.x(), cp1.y(), cp2.x(), cp2.y(), ep.x(), ep.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::cubic_curve_to_rel(const r4::vector2<real>& cp1, const r4::vector2<real>& cp2, const r4::vector2<real>& ep){
	if(!this->has_current_point()){
		this->cubic_curve_to_abs(cp1, cp2, ep);
		return;
	}
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
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
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	auto d = this->get_current_point();
	this->cubic_curve_to_abs(cp1 + d, cp2 + d, ep + d);
#endif
}

void canvas::close_path(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_close_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.close_polygon();
	this->move_to_abs(this->subpath_start_point);
	this->agg_invalidate_polyline();
#endif
}

void canvas::clear_path(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_new_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->path.remove_all();
	this->subpath_start_point.set(0);
	this->agg_invalidate_polyline();
#endif
}

void canvas::arc_abs(const r4::vector2<real>& center, const r4::vector2<real>& radius, real start_angle, real sweep_angle){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	canvas_matrix_push matrix_push(*this);

	this->translate(center);
	this->scale(radius);

	if(sweep_angle >= 0){
		cairo_arc(this->cr, 0, 0, 1, backend_real(start_angle), backend_real(start_angle + sweep_angle));
	}else{
		cairo_arc_negative(this->cr, 0, 0, 1, backend_real(start_angle), backend_real(start_angle + sweep_angle));
	}
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	agg::bezier_arc shape(center.x(), center.y(), radius.x(), radius.y(), start_angle, sweep_angle);

	this->path.join_path(shape);
	this->agg_invalidate_polyline();
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
	auto angle = get_angle(end_p);

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

	auto angle1 = get_angle(-center);
	auto angle2 = get_angle(end_p - center);

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
	this->path.arc_to(radius.x(), radius.y(), x_axis_rotation, large_arc, sweep, end_point.x(), end_point.y());
	this->agg_invalidate_polyline();
#endif
}

void canvas::arc_rel(const r4::vector2<real>& end_point, const r4::vector2<real>& radius, real x_axis_rotation, bool large_arc, bool sweep){
	this->arc_abs(end_point + this->get_current_point(), radius, x_axis_rotation, large_arc, sweep);
}

#if SVGREN_BACKEND == SVGREN_BACKEND_AGG
void canvas::agg_render(agg::rasterizer_scanline_aa<>& rasterizer){
	agg::scanline_u8 scanline;
	if(!this->context.grad){
		agg::renderer_scanline_aa_solid<decltype(group::renderer_base)> renderer(this->group_stack.back().renderer_base);
		renderer.color(this->context.color);
		agg::render_scanlines(
				rasterizer,
				scanline,
				renderer
			);
	}else{
		agg::span_interpolator_linear<
				const decltype(this->context.gradient_matrix)
			> span_interpolator{this->context.gradient_matrix};

		agg::span_gradient<
				decltype(group::pixel_format)::color_type,
                decltype(span_interpolator),
				const gradient::gradient_wrapper_base,
				const decltype(this->context.grad->lut)
			> span_gradient(
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

void canvas::fill(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_fill_preserve(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())

	this->agg_path_to_polyline();

	agg::conv_transform<
			decltype(this->polyline_path),
			decltype(this->context.matrix)
		> transformed_path(
			this->polyline_path,
			this->context.matrix
		);

	agg::rasterizer_scanline_aa<> rasterizer;
	rasterizer.filling_rule(this->context.fill_rule);
	rasterizer.add_path(transformed_path);

	this->agg_render(rasterizer);
#endif
}

void canvas::stroke(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_stroke_preserve(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->agg_path_to_polyline();

	agg::conv_dash<decltype(this->polyline_path)> dashed_path(this->polyline_path);

	if(!this->context.dash_array.empty()){
		// TRACE(<< "dasharray.size() = " << this->context.dash_array.size() << std::endl)
		real dash_length = 0;
		for(auto& d : this->context.dash_array){
			// TRACE(<< "dash = (" << d.first << ", " << d.second << ")" << std::endl)
			dashed_path.add_dash(d.first, d.second);
			dash_length += d.first;
			dash_length += d.second;
		}

		// TRACE(<< "dash offset = " << this->context.dash_offset << std::endl)

		// in case no dashes are added to the dashed_path and dash offset is non-zero the dash_start() function will hang,
		// so call the dash_start() only after all the add_dash() calls are done
		if(this->context.dash_offset >= 0){
			dashed_path.dash_start(backend_real(this->context.dash_offset));
		}else{
			dashed_path.dash_start(backend_real(dash_length + this->context.dash_offset));
		}
	}else{
		const backend_real no_dash_len = 1e8;
		// no dashing
		dashed_path.add_dash(no_dash_len, 0);
		dashed_path.dash_start(no_dash_len / 10);
	}

	agg::conv_stroke<decltype(dashed_path)> stroke_path(dashed_path);

	stroke_path.width(this->context.line_width);
	stroke_path.line_cap(this->context.line_cap);
	stroke_path.line_join(this->context.line_join);
	stroke_path.approximation_scale(this->approximation_scale);

	ASSERT(!this->group_stack.empty())
	agg::conv_transform<
			decltype(stroke_path),
			decltype(this->context.matrix)
		> transformed_path(
			stroke_path,
			this->context.matrix
		);

	agg::rasterizer_scanline_aa<> rasterizer;
	rasterizer.add_path(transformed_path);

    this->agg_render(rasterizer);
#endif
}

void canvas::rectangle(const r4::rectangle<real>& rect, const r4::vector2<real>& corner_radius){
	if(corner_radius.is_zero()){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		cairo_rectangle(this->cr, backend_real(rect.p.x()), backend_real(rect.p.y()), backend_real(rect.d.x()), backend_real(rect.d.y()));
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#else
		this->move_to_abs(rect.p);
		this->line_to_abs(rect.x2_y1());
		this->line_to_abs(rect.x2_y2());
		this->line_to_abs(rect.x1_y2());
		this->close_path();
#endif
	}else{
		// approximate 90 degree arc with bezier curve which matches the arc at 45 degree point
		// and has the same tangent as an arc at 45 degree point
		using std::sqrt;
		const real arc_bezier_param = real(4 * (sqrt(2) - 1) / 3);

		this->move_to_abs(rect.p + r4::vector2<real>{corner_radius.x(), 0});
		this->line_to_abs(rect.p + r4::vector2<real>{rect.d.x() - corner_radius.x(), 0});

		this->cubic_curve_to_rel(
				{arc_bezier_param * corner_radius.x(), 0},
				{corner_radius.x(), corner_radius.y() * (1 - arc_bezier_param)},
				corner_radius
			);

		this->line_to_abs(rect.p + rect.d - r4::vector2<real>{0, corner_radius.y()});

		this->cubic_curve_to_rel(
				{0, arc_bezier_param * corner_radius.y()},
				{-corner_radius.x() * (1 - arc_bezier_param), corner_radius.y()},
				{-corner_radius.x(), corner_radius.y()}
			);

		this->line_to_abs(rect.p + r4::vector2<real>{corner_radius.x(), rect.d.y()});

		this->cubic_curve_to_rel(
				{-arc_bezier_param * corner_radius.x(), 0},
				{-corner_radius.x(), -(1 - arc_bezier_param) * corner_radius.y()},
				-corner_radius
			);

		this->line_to_abs(rect.p + r4::vector2<real>{0, corner_radius.y()});

		this->cubic_curve_to_rel(
				{0, -arc_bezier_param * corner_radius.y()},
				{(1 - arc_bezier_param) * corner_radius.x(), -corner_radius.y()},
				{corner_radius.x(), -corner_radius.y()}
			);

		this->close_path();
	}
}

void canvas::set_line_width(real width){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_line_width(this->cr, backend_real(width));
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

r4::matrix2<real> canvas::get_matrix()const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_matrix_t cm;
	cairo_get_matrix(this->cr, &cm);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return {
		{real(cm.xx), real(cm.xy), real(cm.x0)},
		{real(cm.yx), real(cm.yy), real(cm.y0)}
	};
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	return to_r4_matrix(this->context.matrix);
#endif
}

void canvas::set_matrix(const r4::matrix2<real>& m){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_matrix_t cm;
	cm.xx = backend_real(m[0][0]); cm.xy = backend_real(m[0][1]); cm.x0 = backend_real(m[0][2]);
	cm.yx = backend_real(m[1][0]); cm.yy = backend_real(m[1][1]); cm.y0 = backend_real(m[1][2]);
	cairo_set_matrix(this->cr, &cm);
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	ASSERT(!this->group_stack.empty())
	this->context.matrix = to_agg_matrix(m);
#endif
}

svgren::surface canvas::get_sub_surface(const r4::rectangle<unsigned>& region){
	r4::vector2<unsigned> dims;
	pixel* buffer;
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

		buffer = reinterpret_cast<pixel*>(cairo_image_surface_get_data(s));
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	{
		ASSERT(!this->group_stack.empty())
		dims = this->dims;
		auto& cur_group = this->group_stack.back();
		stride = cur_group.rendering_buffer.stride_abs();
		buffer = cur_group.pixels.data();
	}
#endif

	ASSERT(buffer)
	ASSERT(stride % sizeof(pixel) == 0)

	svgren::surface ret;
	ret.stride = stride / sizeof(pixel);

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
	this->group_stack.emplace_back(this->dims);
#endif
}

void canvas::pop_group(real opacity){
	ASSERT(0 <= opacity) ASSERT(opacity <= 1)
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
	ASSERT(this->group_stack.size() >= 2)

	auto& sg = this->group_stack.back();
	auto& dg = *std::next(this->group_stack.rbegin());

	if(opacity < 1){
		dg.renderer_base.blend_from(
				sg.pixel_format,
				nullptr,
				0, // x
				0, // y
				agg::cover_type(opacity * agg::cover_full)
			);
	}else{
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

void canvas::pop_mask_and_group(){
	this->get_sub_surface().append_luminance_to_alpha();

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
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
	ASSERT(this->group_stack.size() >= 2)

	auto& mask = this->group_stack.back().pixels;
	auto& grp = std::next(this->group_stack.rbegin())->pixels;

	// apply mask to the group by multiplying group pixels by mask alpha
	ASSERT(mask.size() == grp.size())
	auto mi = mask.begin();
	auto gi = grp.begin();
	for(; mi != mask.end(); ++mi, ++gi){
		// extract group color and mask alpha
		auto gc = to_rgba(*gi);
		auto ma = ((*mi) >> 24);

		// multiply group color (since we use pre-multiplied pixel format) by mask alpha
		gc *= ma;
		gc /= 0xff;

		// store back the masked group color
		*gi = to_pixel(gc);
	}

	this->group_stack.pop_back(); // pop out mask

	this->pop_group(1); // pop masked group
#endif
}

void canvas::set_dash_pattern(utki::span<const real> dashes, real offset){
	const backend_real epsilon_dash = 1e-8;

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(dashes.empty()){
		cairo_set_dash(this->cr, nullptr, 0, 0); // disable dashing
	}else if(dashes.size() == 1){ // dashes and gaps are of equal length
		auto dash = backend_real(dashes[0]);
		cairo_set_dash(this->cr, &dash, 1, backend_real(offset));
	}else{
		unsigned num_repeats = 1 + unsigned(dashes.size() % 2); // if number of values is odd, then repeat twice
		std::vector<backend_real> dasharray(dashes.size() * num_repeats);
		auto dst = dasharray.begin();
		for(unsigned r = 0; r != num_repeats; ++r){
			for(auto src = dashes.begin(); src != dashes.end(); ++src, ++dst){
				ASSERT(dst != dasharray.end())
				if(*src == 0){
					*dst = epsilon_dash;
				}else{
					*dst = decltype(dasharray)::value_type(*src);
				}
			}
		}
		ASSERT(dst == dasharray.end())
		cairo_set_dash(this->cr, dasharray.data(), dasharray.size(), backend_real(offset));
		if(cairo_status(this->cr) != CAIRO_STATUS_SUCCESS){
			throw std::runtime_error("cairo_set_dash() failed. Check if there was no negative values in dashes array.");
		}
	}
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
	this->context.dash_offset = offset;

	// TRACE(<< "dashes.size() = " << dashes.size() << std::endl)

	unsigned num_repeats = 1 + unsigned(dashes.size() % 2); // if number of values is odd, then repeat dashes array twice
	// TRACE(<< "num_repeats = " << num_repeats << std::endl)
	this->context.dash_array.resize(dashes.size() / (3 - num_repeats));

	// TRACE(<< "dash_array.size() = " << this->context.dash_array.size() << std::endl)

	auto src = dashes.begin();
	for(auto dst = this->context.dash_array.begin(); dst != this->context.dash_array.end(); ++dst){
		std::array<real, 2> pair;
		for(unsigned i = 0; i != 2; ++i){
			ASSERT(src != dashes.end())
			if(*src == 0){
				pair[i] = epsilon_dash;
			}else{
				pair[i] = *src;
			}
			++src;
			if(src == dashes.end()){
				if(num_repeats == 2){
					--num_repeats;
					src = dashes.begin();
				}else{
					ASSERT(i == 1)
					ASSERT(dst == --this->context.dash_array.end())
				}
			}
		}
		*dst = std::make_pair(pair[0], pair[1]);
	}
#endif
}
