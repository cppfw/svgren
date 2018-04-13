#pragma once

#include <array>
#include <cmath>

#include <utki/config.hpp>
#include <utki/math.hpp>

#include <svgdom/Length.hpp>
#include <svgdom/elements/Element.hpp>

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

#include "config.hpp"
#include "Surface.hxx"
#include "CanvasRegion.hxx"

namespace svgren{

void cairoRelQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y);

void cairoQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y);

//convert degrees to radians
template <class T> T degToRad(T deg){
	return deg * utki::pi<T>() / T(180);
}

//Rotate a point by an angle around the origin point.
template <class T> std::array<T, 2> rotate(T x, T y, T angle){
	using std::cos;
	using std::sin;
    return {{x * cos(angle) - y * sin(angle), y * cos(angle) + x * sin(angle)}};
}

//Return angle between x axis and point knowing given center.
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


Surface getSubSurface(cairo_t* cr, const CanvasRegion& region = CanvasRegion());

real percentLengthToFraction(const svgdom::Length& l);

void appendLuminanceToAlpha(Surface s);

struct DeviceSpaceBoundingBox{
	real left, top, right, bottom;
	
	void setEmpty();
	bool isEmpty()const noexcept;
	
	void merge(const DeviceSpaceBoundingBox& bb);
	
	real width()const noexcept;
	real height()const noexcept;
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
	
	const svgdom::Element* maskElement = nullptr;
public:
	PushCairoGroupIfNeeded(Renderer& renderer, bool isContainer);
	~PushCairoGroupIfNeeded()noexcept;
	
	bool isPushed()const noexcept{
		return this->groupPushed;
	}
};

}
