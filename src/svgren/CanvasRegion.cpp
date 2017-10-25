#include "CanvasRegion.hxx"

#include <utki/debug.hpp>

using namespace svgren;

void CanvasRegion::intersect(const CanvasRegion& r) {
	if(this->x >= r.right() || this->right() <= r.x){
		this->width = 0;
	}else if(this->x <= r.x){
		this->width = this->width - (r.x - this->x) - (this->right() - r.right());
		this->x = r.x;
	}else{
		this->width = r.width - (this->x - r.x) - (r.right() - this->right());
	}
	ASSERT(this->width <= r.width)
	
	
	if(this->y >= r.bottom() || this->bottom() <= r.y){
		this->height = 0;
	}else if(this->y <= r.y){
		this->height = this->height - (r.y - this->y) - (this->bottom() - r.bottom());
		this->y = r.y;
	}else{
		this->height = r.height - (this->y - r.y) - (r.bottom() - this->bottom());
	}
	ASSERT(this->height <= r.height)
}
