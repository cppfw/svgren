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

#pragma once

#include <cstdint>

#include <r4/rectangle.hpp>
#include <rasterimage/image.hpp>
#include <utki/span.hpp>

#include "config.hxx"

namespace svgren {

struct surface {
	r4::vector2<unsigned> position = 0;
	image_span_type image_span;

	surface() = default;

	surface(r4::vector2<unsigned> position, image_span_type image_span) :
		position(position),
		image_span(image_span)
	{}

	r4::rectangle<unsigned> rect() const noexcept
	{
		return {this->position, this->image_span.dims()};
	}

	surface intersection(const r4::rectangle<unsigned>& r);

	void append_luminance_to_alpha();
};

} // namespace svgren
