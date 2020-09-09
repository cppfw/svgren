#pragma once

#include <cstdint>

#include "CanvasRegion.hxx"

namespace svgren{

struct Surface : public CanvasRegion{
	uint8_t* data = nullptr; // RGBA data
	uint8_t* end = nullptr;
	unsigned stride = 0;
	
	Surface intersectionSurface(const CanvasRegion& r)const;
};

}
