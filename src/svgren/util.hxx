#pragma once

#include <array>
#include <cmath>

#include <utki/config.hpp>
#include <utki/math.hpp>

#include <svgdom/length.hpp>
#include <svgdom/elements/element.hpp>
#include <svgdom/visitor.hpp>

#include <r4/segment2.hpp>

#include "config.hxx"
#include "surface.hxx"
#include "canvas.hxx"

namespace svgren{

// convert degrees to radians
inline real deg_to_rad(real deg){
	return deg * utki::pi<real>() / real(180);
}

// return angle between x axis and vector
inline real get_angle(const r4::vector2<real>& v){
	using std::atan2;
    return atan2(v.y(), v.x());
}

class canvas_matrix_push{
	r4::matrix2<real> m;
	canvas& c;
public:
	canvas_matrix_push(canvas& c);
	~canvas_matrix_push()noexcept;
};

real percent_to_fraction(const svgdom::length& l);

class renderer_viewport_push{
	class renderer& r;
	r4::vector2<real> old_viewport;
public:
	renderer_viewport_push(renderer& r, const decltype(old_viewport)& viewport);
	~renderer_viewport_push()noexcept;
};

class common_element_push{
	bool group_pushed;
	surface old_background;
	class svgren::renderer& renderer;

	real opacity = real(1);
	
	const svgdom::element* mask_element = nullptr;

	canvas_matrix_push matrix_push;

	r4::segment2<real> old_device_space_bounding_box;
public:
	common_element_push(svgren::renderer& renderer, bool is_container);
	~common_element_push()noexcept;
	
	bool is_group_pushed()const noexcept{
		return this->group_pushed;
	}
};

struct gradient_caster : public svgdom::const_visitor{
	const svgdom::linear_gradient_element* linear = nullptr;
	const svgdom::radial_gradient_element* radial = nullptr;
	const svgdom::gradient* gradient = nullptr;

	void visit(const svgdom::linear_gradient_element& e)override{
		this->gradient = &e;
		this->linear = &e;
	}

	void visit(const svgdom::radial_gradient_element& e)override{
		this->gradient = &e;
		this->radial = &e;
	}
};

}
