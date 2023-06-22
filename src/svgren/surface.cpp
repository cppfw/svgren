/*
The MIT License (MIT)

Copyright (c) 2015-2023 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* ================ LICENSE END ================ */

#include "surface.hxx"

#include <utki/debug.hpp>

#include "canvas.hxx"

using namespace svgren;

surface surface::intersection(const r4::rectangle<unsigned>& r) const
{
	surface ret = *this;

	ASSERT(ret.span.data() == this->span.data())
	ASSERT(ret.p == this->p)
	ASSERT(ret.d == this->d)

	// TRACE(<< "ret = " << ret << " r = " << r << std::endl)
	ret.intersect(r);
	// TRACE(<< "ret = " << ret << std::endl)
	ASSERT(ret.d.y() <= r.d.y(), [&](auto& o) {
		o << "ret = " << ret << " this = " << (*this) << " r = " << r;
	})

	ASSERT(ret.p.x() >= this->p.x())
	ASSERT(ret.p.y() >= this->p.y())

	auto delta = (ret.p.y() - this->p.y()) * ret.stride + (ret.p.x() - this->p.x());

	ret.span = utki::make_span(ret.span.data() + delta, ret.span.size() - delta);

	ASSERT(ret.span.end() == this->span.end())
	ASSERT(ret.d.y() <= this->d.y(), [&](auto& o) {
		o << "ret = " << ret << " this = " << *this << " r = " << r;
	})

	return ret;
}

void surface::append_luminance_to_alpha()
{
	// Luminance is calculated using formula L = 0.2126 * R + 0.7152 * G + 0.0722 * B

	constexpr auto red_coeff = 0.2126;
	constexpr auto green_coeff = 0.7152;
	constexpr auto blue_coeff = 0.0722;

	// TODO: take stride into account, do not append luminance to alpha for data out of the surface width
	for (auto& px : this->span) {
		px.set(
			image_type::value(1),
			image_type::value(1),
			image_type::value(1),
			// we use premultiplied alpha format, so no need to multiply alpha by liminance
			rasterimage::multiply(px.r(), image_type::value(red_coeff))
				+ rasterimage::multiply(px.g(), image_type::value(green_coeff))
				+ rasterimage::multiply(px.b(), image_type::value(blue_coeff))
		);
	}
}
