#include "Surface.hxx"

#include <utki/debug.hpp>

using namespace svgren;

Surface Surface::intersectionSurface(const CanvasRegion& r)const{
	Surface ret = *this;
	
	ret.intersect(r);
	
	ASSERT(ret.x >= this->x)
	ASSERT(ret.y >= this->y)
	
	ret.data += ((ret.y - this->y) * ret.stride + (ret.x - this->x)) * sizeof(std::uint32_t);
	
	return ret;
}
