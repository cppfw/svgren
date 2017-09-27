#pragma once

namespace svgren{

struct SubSurface{
	std::uint8_t* data; //RGBA premultiplied alpha
	unsigned stride;
	unsigned width;
	unsigned height;
	unsigned posx;
	unsigned posy;
};

}
