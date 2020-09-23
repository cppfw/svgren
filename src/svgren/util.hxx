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

// convert degrees to radians
inline real deg_to_rad(real deg){
	return deg * utki::pi<real>() / real(180);
}

// Return angle between x axis and point knowing given center.
inline real point_angle(const r4::vector2<real>& c, const r4::vector2<real>& p){
	using std::atan2;
    return atan2(p.y() - c.y(), p.x() - c.x());
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
	class Renderer& r;
	DeviceSpaceBoundingBox oldBb;
public:
	DeviceSpaceBoundingBoxPush(Renderer& r);
	~DeviceSpaceBoundingBoxPush()noexcept;
};

class ViewportPush{
	class Renderer& r;
	r4::vector2<real> oldViewport;
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
