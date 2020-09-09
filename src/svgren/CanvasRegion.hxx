#pragma once

#include <limits>

namespace svgren{

/**
 * @brief rectangular region on canvas.
 */
struct CanvasRegion{
	unsigned x = 0, y = 0, width = 0, height = 0; // in pixels
	
	CanvasRegion(
			unsigned x = 0,
			unsigned y = 0,
			unsigned width = std::numeric_limits<unsigned>::max(),
			unsigned height = std::numeric_limits<unsigned>::max()
		) :
			x(x),
			y(y),
			width(width),
			height(height)
	{}
	
	void intersect(const CanvasRegion& r);
	
	unsigned right()const{
		return this->x + this->width;
	}
	
	unsigned bottom()const{
		return this->y + this->height;
	}
};

}
