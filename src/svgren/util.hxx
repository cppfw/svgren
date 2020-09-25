#pragma once

#include <array>
#include <cmath>

#include <utki/config.hpp>
#include <utki/math.hpp>

#include <svgdom/length.hpp>
#include <svgdom/elements/element.hpp>
#include <svgdom/visitor.hpp>

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

#include "config.hxx"
#include "surface.hxx"
#include "canvas.hxx"

namespace svgren{

// convert degrees to radians
inline real deg_to_rad(real deg){
	return deg * utki::pi<real>() / real(180);
}

// Return angle between x axis and point knowing given center.
inline real point_angle(const r4::vector2<real>& c, const r4::vector2<real>& p){
	using std::atan2;
    return atan2(p.y() - c.y(), p.x() - c.x());
}

class canvas_matrix_push{
	r4::matrix2<real> m;
	canvas& c;
public:
	canvas_matrix_push(canvas& c);
	~canvas_matrix_push()noexcept;
};

class canvas_context_push{
	canvas& c;
public:
	canvas_context_push(canvas& c);
	~canvas_context_push()noexcept;
};

real percentLengthToFraction(const svgdom::length& l);

void appendLuminanceToAlpha(surface s);

struct DeviceSpaceBoundingBox{
	real left, top, right, bottom;
	
	void set_empty();
	
	void unite(const DeviceSpaceBoundingBox& bb);
	
	real width()const noexcept;
	real height()const noexcept;

	r4::vector2<real> pos()const noexcept{
		return r4::vector2<real>{
			this->left,
			this->top
		};
	}

	r4::vector2<real> dims()const noexcept{
		return r4::vector2<real>{
			this->width(),
			this->height()
		};
	}
};

class DeviceSpaceBoundingBoxPush{
	class renderer& r;
	DeviceSpaceBoundingBox oldBb;
public:
	DeviceSpaceBoundingBoxPush(renderer& r);
	~DeviceSpaceBoundingBoxPush()noexcept;
};

class viewport_push{
	class renderer& r;
	r4::vector2<real> oldViewport;
public:
	viewport_push(renderer& r, const decltype(oldViewport)& viewport);
	~viewport_push()noexcept;
};

class PushCairoGroupIfNeeded{
	bool groupPushed;
	surface oldBackground;
	class svgren::renderer& renderer;

	real opacity = real(1);
	
	const svgdom::element* maskElement = nullptr;
public:
	PushCairoGroupIfNeeded(svgren::renderer& renderer, bool isContainer);
	~PushCairoGroupIfNeeded()noexcept;
	
	bool isPushed()const noexcept{
		return this->groupPushed;
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
