#pragma once

#include "CanvasRegion.hxx"

namespace svgren{

struct Surface : public CanvasRegion{
	std::uint8_t* data = nullptr; //RGBA data
	unsigned stride;
};

}
