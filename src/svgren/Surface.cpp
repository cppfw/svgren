#include "Surface.hxx"

#include <utki/debug.hpp>

using namespace svgren;

Surface Surface::intersectionSurface(const CanvasRegion& r)const{
	Surface ret = *this;

	ASSERT(ret.data == this->data)
	
	ret.intersect(r);
	ASSERT(ret.d.y() <= r.d.y())
	
	ASSERT(ret.p.x() >= this->p.x())
	ASSERT(ret.p.y() >= this->p.y())
	
	ret.data += ((ret.p.y() - this->p.y()) * ret.stride + (ret.p.x() - this->p.x())) * sizeof(uint32_t);
	
	ASSERT(ret.end == this->end)
	ASSERT_INFO(ret.d.y() <= this->d.y(), "ret = " << ret << " this = " << *this << " r = " << r)
	
	return ret;
}
