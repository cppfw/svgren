/*
The MIT License (MIT)

Copyright (c) 2015-2024 Ivan Gagis <igagis@gmail.com>

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

#include "veg/canvas.hpp"

using namespace svgren;

surface surface::intersection(const r4::rectangle<unsigned>& r)
{
	auto ret_rect = r4::rectangle<unsigned>(this->rect()).intersect(r);

	ASSERT(ret_rect.d.y() <= r.d.y(), [&](auto& o) {
		o << "ret_rect = " << ret_rect << " this->rect() = " << this->rect() << " r = " << r;
	})

	if (ret_rect.d.is_any_zero()) {
		// resulting surface is of zero size
		return {};
	}

	ASSERT(this->rect().contains(ret_rect))

	return {
		ret_rect.p, //
		this->image_span.subspan({ret_rect.p - this->position, ret_rect.d})
	};
}
