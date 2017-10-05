#pragma once

namespace svgren{

struct SubSurface{
	std::uint8_t* data; //RGBA premultiplied alpha
	unsigned stride;
	unsigned width;
	unsigned height;
	
	//TODO: remove these?
	unsigned posx;
	unsigned posy;
};

}
