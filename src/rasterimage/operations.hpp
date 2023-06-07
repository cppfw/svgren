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

#include <type_traits>

#include <utki/debug.hpp>
#include <utki/types.hpp>

// TODO: doxygen
namespace rasterimage {

template <typename value_type>
value_type multiply(value_type a, value_type b)
{
#ifdef DEBUG
	static const auto val_zero = value_type(0);
	static const auto val_one = value_type(1);
#endif
	static const auto val_max = std::numeric_limits<value_type>::max();

	if constexpr (std::is_integral_v<value_type>) {
		static_assert(std::is_unsigned_v<value_type>, "unexpected signed integral type");
		static_assert(sizeof(value_type) <= sizeof(uint32_t), "unexpected too large integral type");

		using calc_type = typename utki::uint_size<sizeof(value_type) * 2>::type;

		return value_type(calc_type(a) * calc_type(b) / calc_type(val_max));
	} else {
		static_assert(
			std::is_floating_point_v<value_type>,
			"unexpected value type, expected either integral or floating point"
		);
		ASSERT(val_zero <= a && a <= val_one)
		ASSERT(val_zero <= b && b <= val_one)

		return a * b;
	}
}

template <typename value_type>
value_type divide(value_type a, value_type b)
{
#ifdef DEBUG
	static const auto val_zero = value_type(0);
#endif
	static const auto val_one = value_type(1);
	static const auto val_max = std::numeric_limits<value_type>::max();

	using std::min;
	if constexpr (std::is_integral_v<value_type>) {
		static_assert(std::is_unsigned_v<value_type>, "unexpected signed integral type");
		static_assert(sizeof(value_type) <= sizeof(uint32_t), "unexpected too large integral type");

		using calc_type = typename utki::uint_size<sizeof(value_type) * 2>::type;

		return value_type(min(calc_type(calc_type(a) * calc_type(val_max) / calc_type(b)), calc_type(val_max)));
	} else {
		static_assert(
			std::is_floating_point_v<value_type>,
			"unexpected value type, expected either integral or floating point"
		);
		ASSERT(val_zero <= a && a <= val_one)
		ASSERT(val_zero < b && b <= val_one)

		auto res = min(a / b, val_one); // clamp top

		return a;
	}
}

} // namespace rasterimage
