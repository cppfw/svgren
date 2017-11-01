#pragma once

#include <cstdint>

#include "CanvasRegion.hxx"

namespace svgren{

struct Surface : public CanvasRegion{
	std::uint8_t* data = nullptr; //RGBA data
	std::uint8_t* end = nullptr;
	unsigned stride = 0;
	
	Surface intersectionSurface(const CanvasRegion& r)const;
};

}
