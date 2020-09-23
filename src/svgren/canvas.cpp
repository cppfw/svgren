#include "canvas.hxx"

#include <algorithm>

#include <utki/debug.hpp>

using namespace svgren;

canvas::canvas(unsigned width, unsigned height) :
		data(
				width * height,
#ifdef M_SVGREN_BACKGROUND
				M_SVGREN_BACKGROUND
#else
				0
#endif
			)
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
		, surface(width, height, this->data.data())
		, cr(cairo_create(this->surface.surface))
#endif
{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(!this->cr){
		throw std::runtime_error("svgren::canvas::canvas(): could not create cairo context");
	}
#endif
}

canvas::~canvas(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_destroy(this->cr);
#endif
}

std::vector<uint32_t> canvas::release(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	for(auto &c : this->data){
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

	return std::move(this->data);
}

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
#endif
}

void canvas::translate(real x, real y){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_translate(this->cr, x, y);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
#endif
}

void canvas::rotate(real radians){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rotate(this->cr, radians);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo status = " << cairo_status_to_string(cairo_status(this->cr)))
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
#endif
}

void canvas::set_fill_rule(canvas::fill_rule fr){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_fill_rule_t cfr;
	switch (fr){
		default:
			ASSERT(false);
		case fill_rule::even_odd:
			cfr = CAIRO_FILL_RULE_EVEN_ODD;
			break;
		case fill_rule::winding:
			cfr = CAIRO_FILL_RULE_WINDING;
			break;
	}
	cairo_set_fill_rule(this->cr, cfr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::set_source(real r, real g, real b, real a){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_source_rgba(this->cr, double(r), double(g), double(b), double(a));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

r4::vector2<real> canvas::matrix_mul(const r4::vector2<real>& v){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x = v.x();
	double y = v.y();
	cairo_user_to_device(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#endif
}

r4::vector2<real> canvas::matrix_mul_distance(const r4::vector2<real>& v){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x = v.x();
	double y = v.y();
	cairo_user_to_device_distance(this->cr, &x, &y);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return r4::vector2<real>(real(x), real(y));
#endif
}

r4::rectangle<real> canvas::get_shape_bounding_box()const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	double x1, y1, x2, y2;

	// According to SVG spec https://www.w3.org/TR/SVG/coords.html#ObjectBoundingBox
	// "The bounding box is computed exclusive of any values for clipping, masking, filter effects, opacity and stroke-width"
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
#endif
}

bool canvas::has_current_point()const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto cp = cairo_has_current_point(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return bool(cp);
#endif
}

r4::vector2<real> canvas::get_current_point()const{
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	auto has_cur_p = cairo_has_current_point(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	if(has_cur_p){
		double xx, yy;
		cairo_get_current_point(this->cr, &xx, &yy);
		ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
		return {real(xx), real(yy)};
	}
	cairo_move_to(this->cr, 0, 0);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
	return real(0);
#endif
}

void canvas::move_to_abs(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_move_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::move_to_rel(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_move_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::line_to_abs(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_line_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::line_to_rel(const r4::vector2<real>& p){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rel_line_to(this->cr, double(p.x()), double(p.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
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
#endif
}

void canvas::close_path(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_close_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::clear_path(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_new_path(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::arc_abs(const r4::vector2<real>& center, real radius, real angle1, real angle2){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	if(angle2 > angle1){
		cairo_arc(this->cr, double(center.x()), double(center.y()), double(radius), double(angle1), double(angle2));
	}else{
		cairo_arc_negative(this->cr, double(center.x()), double(center.y()), double(radius), double(angle1), double(angle2));
	}
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::fill(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_fill_preserve(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#endif
}

void canvas::stroke(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_stroke_preserve(this->cr);
	ASSERT_INFO(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS, "cairo error: " << cairo_status_to_string(cairo_status(this->cr)))
#endif
}

void canvas::rectangle(const r4::rectangle<real>& rect){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_rectangle(this->cr, double(rect.p.x()), double(rect.p.y()), double(rect.d.x()), double(rect.d.y()));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::set_line_width(real width){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_set_line_width(this->cr, double(width));
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
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
#endif
}

void canvas::push_context(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_save(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
#endif
}

void canvas::pop_context(){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_restore(this->cr);
	ASSERT(cairo_status(this->cr) == CAIRO_STATUS_SUCCESS)
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
#endif
}

void canvas::set_matrix(const r4::matrix2<real>& m){
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	cairo_matrix_t cm;
	cm.xx = double(m[0][0]); cm.xy = double(m[0][1]); cm.x0 = double(m[0][2]);
	cm.yx = double(m[1][0]); cm.yy = double(m[1][1]); cm.y0 = double(m[1][2]);
	cairo_set_matrix(this->cr, &cm);
#endif
}
