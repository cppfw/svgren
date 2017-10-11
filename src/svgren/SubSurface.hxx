#pragma once

#include "CanvasRegion.hxx"

namespace svgren{

struct SubSurface : public CanvasRegion{
	std::uint8_t* data; //RGBA premultiplied alpha
	unsigned stride;
};

}
