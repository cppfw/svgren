#pragma once

#include <array>
#include <cmath>

#include <utki/config.hpp>
#include <utki/math.hpp>

#include <svgdom/length.hpp>
#include <svgdom/elements/element.hpp>

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

#include "config.hxx"
#include "Surface.hxx"

namespace svgren{

void cairoRelQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y);

void cairoQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y);

// convert degrees to radians
template <class T> T deg_to_rad(T deg){
	return deg * utki::pi<T>() / T(180);
}

// Rotate a vector by an angle around the origin point.
template <class T> std::array<T, 2> rotate_vector(T x, T y, T angle){
	using std::cos;
	using std::sin;
    return {{x * cos(angle) - y * sin(angle), y * cos(angle) + x * sin(angle)}};
}

// Return angle between x axis and point knowing given center.
template <class T> T pointAngle(T cx, T cy, T px, T py){
	using std::atan2;
    return atan2(py - cy, px - cx);
}

class CairoMatrixSaveRestore{
	cairo_matrix_t m;
	cairo_t* cr;
public:
	CairoMatrixSaveRestore(cairo_t* cr);
	~CairoMatrixSaveRestore()noexcept;
};

class CairoContextSaveRestore{
	cairo_t* cr;
public:
	CairoContextSaveRestore(cairo_t* cr);
	~CairoContextSaveRestore()noexcept;
};

Surface getSubSurface(cairo_t* cr, const r4::rectangle<unsigned>& region = {0, std::numeric_limits<unsigned>::max()});

real percentLengthToFraction(const svgdom::length& l);

void appendLuminanceToAlpha(Surface s);

struct DeviceSpaceBoundingBox{
	real left, top, right, bottom;
	
	void set_empty();
	bool is_empty()const noexcept;
	
	void merge(const DeviceSpaceBoundingBox& bb);
	
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
	class Renderer& r;
	DeviceSpaceBoundingBox oldBb;
public:
	DeviceSpaceBoundingBoxPush(Renderer& r);
	~DeviceSpaceBoundingBoxPush()noexcept;
};

class ViewportPush{
	class Renderer& r;
	std::array<real, 2> oldViewport;
public:
	ViewportPush(Renderer& r, const decltype(oldViewport)& viewport);
	~ViewportPush()noexcept;
};

class PushCairoGroupIfNeeded{
	bool groupPushed;
	Surface oldBackground;
	class Renderer& renderer;

	real opacity = real(1);
	
	const svgdom::element* maskElement = nullptr;
public:
	PushCairoGroupIfNeeded(Renderer& renderer, bool isContainer);
	~PushCairoGroupIfNeeded()noexcept;
	
	bool isPushed()const noexcept{
		return this->groupPushed;
	}
};

}
