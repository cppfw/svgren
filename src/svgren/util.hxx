#pragma once

#include <array>

#include <utki/config.hpp>

#include "config.hpp"

#if M_OS == M_OS_WINDOWS || M_OS_NAME == M_OS_NAME_IOS
#	include <cairo.h>
#else
#	include <cairo/cairo.h>
#endif

namespace svgren{

void cairoImageSurfaceBlur(cairo_surface_t* surface, std::array<real, 2> stdDeviation);

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

}