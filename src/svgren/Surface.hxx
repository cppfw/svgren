#pragma once

#include "CanvasRegion.hxx"

namespace svgren{

struct Surface : public CanvasRegion{
	std::uint8_t* data; //RGBA premultiplied alpha
	unsigned stride;
};

}
