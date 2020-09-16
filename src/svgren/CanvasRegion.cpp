#include "CanvasRegion.hxx"

#include <utki/debug.hpp>

using namespace svgren;

// void CanvasRegion::intersect1(const CanvasRegion& r){
// #ifdef DEBUG
// 	auto oldWidth = this->width;
// #endif
// 	if(this->x >= r.right() || this->right() <= r.x){
// 		this->width = 0;
// 	}else if(this->x <= r.x){
// 		if(this->right() >= r.right()){
// 			this->width -= this->right() - r.right();
// 		}
// 		this->width -= r.x - this->x;
// 		this->x = r.x;
// 	}else{
// 		if(r.right() >= this->right()){
// 			this->width -= r.right() - this->right();
// 		}
// 		this->width = r.width - (this->x - r.x);
// 	}
// 	ASSERT(this->width <= r.width)
// 	ASSERT(this->width <= oldWidth)
			
// #ifdef DEBUG
// 	auto oldHeight = this->height;
// #endif

// 	if(this->y >= r.bottom() || this->bottom() <= r.y){
// 		this->height = 0;
// 	}else if(this->y <= r.y){
// 		if(this->bottom() >= r.bottom()){
// 			this->height -= this->bottom() - r.bottom();
// 		}
// 		this->height -= r.y - this->y;
// 		this->y = r.y;
// 	}else{
// 		if(r.bottom() >= this->bottom()){
// 			this->height -= r.bottom() - this->bottom();
// 		}
// 		this->height = r.height - (this->y - r.y);
// 	}
// 	ASSERT(this->height <= r.height)
// 	ASSERT(this->height <= oldHeight)
// }
