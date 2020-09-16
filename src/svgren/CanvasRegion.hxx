#pragma once

#include <limits>

#include <r4/rectangle.hpp>

namespace svgren{

/**
 * @brief rectangular region on canvas.
 */
struct CanvasRegion : public r4::rectangle<unsigned>{
	// unsigned x = 0, y = 0, width = 0, height = 0; // in pixels
	
	CanvasRegion(
			unsigned x = 0,
			unsigned y = 0,
			unsigned width = std::numeric_limits<unsigned>::max(),
			unsigned height = std::numeric_limits<unsigned>::max()
		) :
			r4::rectangle<unsigned>{x, y, width, height}
	{}
	
	// void intersect1(const CanvasRegion& r);
	
	unsigned right()const{
		return this->pdx();
		// return this->x + this->width;
	}
	
	unsigned bottom()const{
		return this->pdy();
		// return this->y + this->height;
	}
};

}
