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

#include <r4/vector.hpp>
#include <utki/debug.hpp>
#include <utki/span.hpp>

#include "operations.hpp"

// TODO: doxygen
namespace rasterimage {

class dimensioned
{
public:
	using dimensions_type = r4::vector2<uint32_t>;

protected:
	dimensions_type dimensions;

public:
	dimensioned(dimensions_type dimensions) :
		dimensions(dimensions)
	{}

	const dimensions_type& dims() const noexcept
	{
		return this->dimensions;
	}
};

template <typename channel_type, size_t number_of_channels>
class image : public dimensioned
{
public:
	static const size_t num_channels = number_of_channels;

	using pixel_type = r4::vector<channel_type, num_channels>;
	using value_type = typename pixel_type::value_type;

	static_assert(sizeof(pixel_type) == sizeof(channel_type) * number_of_channels, "pixel_type has padding");

	static_assert(
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		sizeof(pixel_type[2]) == sizeof(pixel_type) * 2,
		"pixel_type array has gaps"
	);

private:
	std::vector<pixel_type> buffer;

	template <bool is_const>
	class iterator_internal
	{
		friend class image;

	public:
		using const_value_type = utki::span<const pixel_type>;

	private:
		using non_const_value_type = utki::span<pixel_type>;

		std::conditional_t<is_const, const_value_type, non_const_value_type> line;

		iterator_internal(decltype(line) line) :
			line(line)
		{}

	public:
		// The iterator cannot have stronger tag than std::input_iterator_tag
		// because it's reference type is value_type. Otherwise, the iterator is
		// actually a random access iterator.
		using iterator_category = std::input_iterator_tag;

		using difference_type = std::ptrdiff_t;
		using value_type = decltype(line);
		using reference = value_type;
		using pointer = void;

		iterator_internal() = default;

		bool operator!=(const iterator_internal& i) const noexcept
		{
			return this->line.data() != i.line.data();
		}

		bool operator==(const iterator_internal& i) const noexcept
		{
			return this->line.data() == i.line.data();
		}

		value_type operator*() noexcept
		{
			return this->line;
		}

		const_value_type operator*() const noexcept
		{
			return this->line;
		}

		const value_type* operator->() noexcept
		{
			return &this->line;
		}

		const const_value_type* operator->() const noexcept
		{
			return &this->line;
		}

		iterator_internal& operator++() noexcept
		{
			this->line = utki::make_span(this->line.data() + this->line.size(), this->line.size());
			return *this;
		}

		iterator_internal& operator--() noexcept
		{
			this->line = utki::make_span(this->line.data() - this->line.size(), this->line.size());
			return *this;
		}

		// postfix increment
		iterator_internal operator++(int) noexcept
		{
			iterator_internal ret(*this);
			this->operator++();
			return ret;
		}

		// postfix decrement
		iterator_internal operator--(int) noexcept
		{
			iterator_internal ret(*this);
			this->operator--();
			return ret;
		}

		iterator_internal& operator+=(difference_type d) noexcept
		{
			this->line = utki::make_span(this->line.data() + d * this->line.size(), this->line.size());

			return *this;
		}

		iterator_internal& operator-=(difference_type d) noexcept
		{
			return this->operator+=(-d);
		}

		iterator_internal operator+(difference_type d) const noexcept
		{
			iterator_internal ret = *this;
			ret += d;
			return ret;
		}

		friend iterator_internal operator+(difference_type d, const iterator_internal& i) noexcept
		{
			return i + d;
		}

		iterator_internal operator-(difference_type d) const noexcept
		{
			iterator_internal ret = *this;
			ret -= d;
			return ret;
		}

		difference_type operator-(const iterator_internal& i) const noexcept
		{
			ASSERT(!this->line.empty())
			if (this->line.data() >= i.line.data()) {
				return (this->line.data() - i.line.data()) / this->line.size();
			} else {
				return -((i.line.data() - this->line.data()) / this->line.size());
			}
		}

		value_type operator[](difference_type d) noexcept
		{
			return *(*this + d);
		}

		const_value_type operator[](difference_type d) const noexcept
		{
			return *(*this + d);
		}

		bool operator<(const iterator_internal& i) const noexcept
		{
			return this->line.data() < i.line.data();
		}

		bool operator>(const iterator_internal& i) const noexcept
		{
			return this->line.data() > i.line.data();
		}

		bool operator>=(const iterator_internal& i) const noexcept
		{
			return this->line.data() >= i.line.data();
		}

		bool operator<=(const iterator_internal& i) const noexcept
		{
			return this->line.data() <= i.line.data();
		}
	};

public:
	using iterator = iterator_internal<false>;
	using const_iterator = iterator_internal<true>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	image(dimensions_type dimensions = {0, 0}) :
		dimensioned(dimensions),
		buffer(this->dimensions.x() * this->dimensions.y()){
			ASSERT(!this->buffer.empty() || (this->dimensions.x() == 0 && !this->buffer.data()))}

		image(dimensions_type dimensions, pixel_type fill) :
		image(dimensions)
	{
		std::fill(this->pixels().begin(), this->pixels().end(), fill);
	}

	image(dimensions_type dimensions, decltype(buffer) buffer) :
		dimensioned(dimensions),
		buffer(std::move(buffer)){ASSERT(
			this->dims().x() * this->dims().y() == this->pixels().size(),
			[this](auto& o) {
				o << "rasterimage::image::image(dims, buffer): dimensions do not match with pixels array size"
				  << "\n";
				o << "\t"
				  << "dims = " << this->dims() << ", pixels().size() = " << this->pixels().size();
			}
		)}

		iterator begin() noexcept
	{
		return iterator(utki::make_span(this->buffer.data(), this->dimensions.x()));
	}

	bool empty() const noexcept
	{
		return this->buffer.empty();
	}

	iterator end() noexcept
	{
		return iterator(utki::make_span(this->buffer.data() + this->dimensions.x() * this->dimensions.y(), 0));
	}

	const_iterator cbegin() const noexcept
	{
		return const_iterator(utki::make_span(this->buffer.data(), this->dimensions.x()));
	}

	const_iterator cend() const noexcept
	{
		return const_iterator(utki::make_span(this->buffer.data() + this->dimensions.x() * this->dimensions.y(), 0));
	}

	const_reverse_iterator crbegin() const
	{
		return const_reverse_iterator(this->cend());
	}

	const_reverse_iterator crend() const
	{
		return const_reverse_iterator(this->cbegin());
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator(this->end());
	}

	reverse_iterator rend()
	{
		return reverse_iterator(this->begin());
	}

	utki::span<pixel_type> pixels() noexcept
	{
		return this->buffer;
	}

	utki::span<const pixel_type> pixels() const noexcept
	{
		return this->buffer;
	}

	utki::span<pixel_type> operator[](uint32_t line_index) noexcept
	{
		return *utki::next(this->begin(), line_index);
	}

	utki::span<const pixel_type> operator[](uint32_t line_index) const noexcept
	{
		return *utki::next(this->begin(), line_index);
	}

	void clear(pixel_type val)
	{
		for (auto l : *this) {
			for (auto& p : l) {
				p = val;
			}
		}
	}

	void swap_red_blue() noexcept
	{
		using std::swap;
		for (auto& p : this->pixels()) {
			swap(p.r(), p.b());
		}
	}

	void unpremultiply_alpha() noexcept
	{
		for (auto& p : this->pixels()) {
			p = rasterimage::unpremultiply_alpha(p);
		}
	}

	static image make(dimensions_type dims, const value_type* data, size_t stride_in_values = 0)
	{
		if (stride_in_values == 0) {
			stride_in_values = dims.x() * num_channels;
		}

		image im(dims);

		auto src_row = data;

		for (auto row : im) {
			auto num_values_per_row = im.dims().x() * num_channels;
			ASSERT(row.size() * num_channels == num_values_per_row)
			std::copy( //
				src_row,
				src_row + num_values_per_row,
				reinterpret_cast<value_type*>(row.data())
			);
			src_row += stride_in_values;
		}

		ASSERT(src_row == data + stride_in_values * im.dims().y())

		return im;
	}

	constexpr static value_type value(float f)
	{
		return rasterimage::value<value_type>(f);
	}
};

} // namespace rasterimage
