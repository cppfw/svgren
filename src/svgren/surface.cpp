#include "surface.hxx"

#include <utki/debug.hpp>

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
	
	auto delta = ((ret.p.y() - this->p.y()) * ret.stride + (ret.p.x() - this->p.x())) * sizeof(uint32_t);

	ret.span = utki::make_span(
			ret.span.data() + delta,
			ret.span.size() - delta
		);
	
	ASSERT(ret.span.end() == this->span.end())
	ASSERT_INFO(ret.d.y() <= this->d.y(), "ret = " << ret << " this = " << *this << " r = " << r)
	
	return ret;
}
