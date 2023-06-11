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

#include <variant>

#include <papki/file.hpp>

#include "image.hpp"

namespace rasterimage {

enum class depth {
	uint_8_bit,
	uint_16_bit,
	float_32_bit,

	enum_size
};

template <depth depth_enum>
using depth_type_t = std::conditional_t<
	depth_enum == depth::uint_8_bit,
	uint8_t,
	std::conditional_t<
		depth_enum == depth::uint_16_bit,
		uint16_t,
		std::conditional_t<depth_enum == depth::float_32_bit, float, void>>>;

enum class format {
	grey,
	greya,
	rgb,
	rgba,

	enum_size
};

inline constexpr size_t to_num_channels(format f)
{
	return size_t(f) + 1;
}

inline constexpr format to_format(unsigned num_channels)
{
#ifdef DEBUG
	if (num_channels < 1 || 4 < num_channels) {
		throw std::logic_error("num_channels out of range");
	}
#endif
	return format(num_channels - 1);
}

// TODO: doxygen
class image_variant
{
public:
	using variant_type = std::variant<
		image<uint8_t, 1>,
		image<uint8_t, 2>,
		image<uint8_t, 3>,
		image<uint8_t, 4>,
		image<uint16_t, 1>,
		image<uint16_t, 2>,
		image<uint16_t, 3>,
		image<uint16_t, 4>,
		image<float, 1>,
		image<float, 2>,
		image<float, 3>,
		image<float, 4>>;

	variant_type variant;

private:
	static size_t to_variant_index(format pixel_format, depth channel_depth);

public:
	image_variant(
		const r4::vector2<uint32_t>& dimensions = {0, 0},
		format pixel_format = format::rgba,
		depth channel_depth = depth::uint_8_bit
	);

	template <typename channel_type, size_t num_channels>
	image_variant(image<channel_type, num_channels>&& im) :
		variant(std::move(im))
	{}

	size_t num_channels() const noexcept
	{
		auto ret = size_t(this->get_format()) + 1;
		ASSERT(
			std::visit(
				[](const auto& sfi) {
					return sfi.num_channels;
				},
				this->variant
			)
			== ret
		)
		return ret;
	}

	format get_format() const noexcept
	{
		auto ret = format(this->variant.index() % size_t(format::enum_size));
		ASSERT(
			format(std::visit(
				[](const auto& sfi) {
					return sfi.num_channels - 1;
				},
				this->variant
			))
			== ret
		)
		return ret;
	}

	depth get_depth() const noexcept
	{
		return depth(this->variant.index() / size_t(format::enum_size));
	}

	const dimensioned::dimensions_type& dims() const noexcept;

	bool empty() const noexcept;

	/**
	 * @brief Get buffer size.
	 * @return Size of the underlying image buffer, in pixels.
	 */
	size_t buffer_size() const noexcept;

	template <format components_enum, depth depth_enum = depth::uint_8_bit>
	image<depth_type_t<depth_enum>, to_num_channels(components_enum)>& get()
	{
		return std::get<image<depth_type_t<depth_enum>, to_num_channels(components_enum)>>(this->variant);
	}

	template <format components_enum, depth depth_enum = depth::uint_8_bit>
	const image<depth_type_t<depth_enum>, to_num_channels(components_enum)>& get() const
	{
		return std::get<image<depth_type_t<depth_enum>, to_num_channels(components_enum)>>(this->variant);
	}

	/**
	 * @brief Write image to PNG file.
	 *
	 * @param fi - file interface for writing the file. Must not be opened.
	 *             Exisitng file will be overwritten.
	 */
	void write_png(const papki::file& fi) const;
};

/**
 * @brief Read PNG image from file.
 * @param fi - file to read the image from. File must not be opened.
 * @return Image read from the file.
 */
image_variant read_png(const papki::file& fi);

/**
 * @brief Read JPEG image from file.
 * @param fi - file to read the image from. File must not be opened.
 * @return Image read from the file.
 */
image_variant read_jpeg(const papki::file& fi);

/**
 * @brief Read image from file.
 * Automatically detects the image file format by filename suffix.
 * @param fi - file to read the image from. File must not be opened.
 * @return Image read from file.
 */
image_variant read(const papki::file& fi);

} // namespace rasterimage
