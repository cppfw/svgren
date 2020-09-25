#pragma once

#include <vector>

#include <r4/vector4.hpp>

#include <svgdom/elements/gradients.hpp>

#include "config.hxx"

namespace svgren{

struct gradient{
	struct stop{
		r4::vector4<real> rgba;
		real offset;
	};
	std::vector<stop> stops;

	svgdom::gradient::spread_method spread_method;
};

struct linear_gradient : public gradient{
	r4::vector2<real> p0;
	r4::vector2<real> p1;
};

struct radial_gradient : public gradient{
	r4::vector2<real> c0;
	r4::vector2<real> c1;
	real r0;
	real r1;
};

}
