#pragma once

namespace svgren{

struct SubSurface{
	std::uint8_t* data; //RGBA premultiplied alpha
	unsigned stride;
	
	unsigned width;
	unsigned height;
	
	//position of subsurface on the canvas
	unsigned posx;
	unsigned posy;
};

}
