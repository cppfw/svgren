#include "surface.hxx"

#include <utki/debug.hpp>

#include "canvas.hxx"

using namespace svgren;

surface surface::intersection(const r4::rectangle<unsigned>& r)const{
	surface ret = *this;

	ASSERT(ret.span.data() == this->span.data())
	ASSERT(ret.p == this->p)
	ASSERT(ret.d == this->d)
	
	// TRACE(<< "ret = " << ret << " r = " << r << std::endl)
	ret.intersect(r);
	// TRACE(<< "ret = " << ret << std::endl)
	ASSERT_INFO(ret.d.y() <= r.d.y(), "ret = " << ret << " this = " << (*this) << " r = " << r)
	
	ASSERT(ret.p.x() >= this->p.x())
	ASSERT(ret.p.y() >= this->p.y())
	
	auto delta = (ret.p.y() - this->p.y()) * ret.stride + (ret.p.x() - this->p.x());

	ret.span = utki::make_span(
			ret.span.data() + delta,
			ret.span.size() - delta
		);
	
	ASSERT(ret.span.end() == this->span.end())
	ASSERT_INFO(ret.d.y() <= this->d.y(), "ret = " << ret << " this = " << *this << " r = " << r)
	
	return ret;
}

void surface::append_luminance_to_alpha(){
	// Luminance is calculated using formula L = 0.2126 * R + 0.7152 * G + 0.0722 * B
	// For faster calculation it can be simplified to L = (2 * R + 3 * G + B) / 6
	
	// TODO: take stride into account, do not append luminance to alpha for data out of the surface width
	for(auto p = this->span.begin(); p != this->span.end(); ++p){
		auto c = get_rgba(*p).to<uint32_t>();

		uint32_t l = (2 * c.r() + 3 * c.g() + c.b()) / 6;
		ASSERT(l <= 255)
		
		// Cairo uses premultiplied alpha, so no need to multiply alpha by liminance.
		*p &= 0xffffff;
		*p |= (l << 24);
	}
}
