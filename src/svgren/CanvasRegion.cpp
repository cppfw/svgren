#include "CanvasRegion.hxx"


using namespace svgren;

void CanvasRegion::intersect(const CanvasRegion& r) {
	if(this->x >= r.right() || this->right() <= r.x){
		this->width = 0;
	}else if(this->x <= r.x){
		this->width -= r.x - this->x;
		this->x = r.x;
	}else{
		this->width = r.width - (this->x - r.x);
	}
	
	if(this->y >= r.bottom() || this->bottom() <= r.y){
		this->height = 0;
	}else if(this->y <= r.y){
		this->height -= r.y - this->y;
		this->y = r.y;
	}else{
		this->height = r.height - (this->y - r.y);
	}
}
