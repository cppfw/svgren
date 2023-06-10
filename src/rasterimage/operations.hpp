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

#include <r4/vector.hpp>
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

template <typename value_type>
value_type value(float f)
{
	if constexpr (std::is_floating_point_v<value_type>) {
		return value_type(f);
	} else {
		static_assert(std::is_integral_v<value_type>, "unexpected non-integral value_type");
		static_assert(std::is_unsigned_v<value_type>, "unexpected signed integral value_type");

		const auto val_max = std::numeric_limits<value_type>::max();

		return value_type(f * val_max);
	}
}

template <typename value_type>
r4::vector4<value_type> unpremultiply_alpha(const r4::vector4<value_type>& c)
{
	// optimization
	if (c.a() == value<value_type>(1) || c.a() == value<value_type>(0)) {
		return c;
	}

	return {//
			divide(c.r(), c.a()),
			divide(c.g(), c.a()),
			divide(c.b(), c.a()),
			c.a()};
}

template <typename to_value_type = float, typename from_value_type, size_t num_channels>
r4::vector<to_value_type, num_channels> to_float(const r4::vector<from_value_type, num_channels>& integral)
{
	static_assert(std::is_floating_point_v<to_value_type>, "unexpected non-floating point destination type");
	static_assert(std::is_integral_v<from_value_type>, "unexpceted non-integral source type");
	static_assert(std::is_unsigned_v<from_value_type>, "unexpected signed source type");

	static const auto val_max = std::numeric_limits<from_value_type>::max();

	return integral.template to<to_value_type>() / to_value_type(val_max);
}

template <typename to_value_type, typename from_value_type, size_t num_channels>
r4::vector<to_value_type, num_channels> to_integral(const r4::vector<from_value_type, num_channels>& floating_point)
{
	static_assert(std::is_floating_point_v<from_value_type>, "unexpected non-floating point source type");
	static_assert(std::is_integral_v<to_value_type>, "unexpceted non-integral destination type");
	static_assert(std::is_unsigned_v<to_value_type>, "unexpected signed destination type");

	static const auto val_max = std::numeric_limits<to_value_type>::max();

	return (floating_point * val_max).template to<to_value_type>();
}

} // namespace rasterimage
