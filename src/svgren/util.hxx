#pragma once

#include <array>

#include <utki/config.hpp>

#include <svgdom/Length.hpp>

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
real degToRad(real deg);

//Rotate a point by an angle around the origin point.
std::array<real, 2> rotate(real x, real y, real angle);

//Return angle between x axis and point knowing given center.
real pointAngle(real cx, real cy, real px, real py);

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
public:
	PushCairoGroupIfNeeded(Renderer& renderer);
	~PushCairoGroupIfNeeded()noexcept;
};

}
