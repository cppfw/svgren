#include "Surface.hxx"

#include <utki/debug.hpp>

using namespace svgren;

Surface Surface::intersectionSurface(const CanvasRegion& r)const{
	Surface ret = *this;

	ASSERT(ret.data == this->data)
	
	ret.intersect(r);
	ASSERT(ret.height <= r.height)
	
	ASSERT(ret.x >= this->x)
	ASSERT(ret.y >= this->y)
	
	ret.data += ((ret.y - this->y) * ret.stride + (ret.x - this->x)) * sizeof(std::uint32_t);
	
	ASSERT(ret.end == this->end)
	ASSERT_INFO(ret.height <= this->height, "ret.height = " << ret.height << " this->height = " << this->height << " r.height = " << r.height)
	
	return ret;
}
