#include "Surface.hxx"


using namespace svgren;

Surface Surface::intersectionSurface(const CanvasRegion& r)const{
	Surface ret = *this;
	
	ret.intersect(r);
	
	ret.data += ((ret.y - r.y) * ret.stride + (ret.x - r.x)) * sizeof(std::uint32_t);
	
	return ret;
}
