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

#include "image_variant.hpp"

#include <string>

// JPEG lib does not have 'extern "C"{}' :-(, so we put it outside of their .h
// or will have linking problems otherwise because
// of "_" symbol in front of C-function names
extern "C" {
#include <jpeglib.h>
}

#include <png.h>
#include <utki/config.hpp>

using namespace std::string_literals;

using namespace rasterimage;

using factory_type = std::add_pointer_t<image_variant::variant_type(const r4::vector2<uint32_t>& dimensions)>;

// creates std::array of factory functions which construct image_variant::variant_type
// initialized to alternative index same as factory's index in the array
template <size_t... index>
std::array<factory_type, sizeof...(index)> make_factories_array(std::index_sequence<index...>)
{
	return {[](const r4::vector2<uint32_t>& dimensions) {
		return image_variant::variant_type(std::in_place_index<index>, dimensions);
	}...};
}

size_t image_variant::to_variant_index(format pixel_format, depth channel_depth)
{
	auto ret = size_t(channel_depth) * size_t(format::enum_size) + size_t(pixel_format);
	ASSERT(ret < std::variant_size_v<image_variant::variant_type>)
	return ret;
}

image_variant::image_variant(const r4::vector2<uint32_t>& dimensions, format pixel_format, depth channel_depth) :
	variant([&]() {
		const static auto factories_array =
			make_factories_array(std::make_index_sequence<std::variant_size_v<image_variant::variant_type>>());

		auto i = to_variant_index(pixel_format, channel_depth);

		return factories_array[i](dimensions);
	}())
{}

const dimensioned::dimensions_type& image_variant::dims() const noexcept
{
	try {
		ASSERT(!this->variant.valueless_by_exception())
		return std::visit(
			[&](const dimensioned& im) -> const auto& {
				return im.dims();
			},
			this->variant
		);
	} catch (std::bad_variant_access& e) {
		// this->variant must never be valueless_by_exeception,
		// so should never reach here
		ASSERT(false)
		abort();
	}
}

bool image_variant::empty() const noexcept
{
	try {
		ASSERT(!this->variant.valueless_by_exception())
		return std::visit(
			[&](const auto& im) {
				return im.empty();
			},
			this->variant
		);
	} catch (std::bad_variant_access& e) {
		// this->variant must never be valueless_by_exeception,
		// so should never reach here
		ASSERT(false)
		abort();
	}
}

size_t image_variant::buffer_size() const noexcept
{
	try {
		ASSERT(!this->variant.valueless_by_exception())
		return std::visit(
			[&](const auto& im) {
				return im.pixels().size();
			},
			this->variant
		);
	} catch (std::bad_variant_access& e) {
		// this->variant must never be valueless_by_exeception,
		// so should never reach here
		ASSERT(false)
		abort();
	}
}

namespace {
void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	auto fi = reinterpret_cast<papki::file*>(png_get_io_ptr(png_ptr));
	ASSERT(fi)

	ASSERT(fi->is_open())

	// TODO: check return value
	fi->write(utki::make_span(data, length));
}

void png_flush_callback(png_structp png_ptr)
{
	// do nothing
}
} // namespace

void image_variant::write_png(const papki::file& fi) const
{
	if (this->get_depth() != rasterimage::depth::uint_8_bit) {
		// TODO: add support for writing 16 bit images
		throw std::logic_error("writing of only 8 bit images is currently supported");
	}

	if (this->get_format() != rasterimage::format::rgba) {
		// TODO: support writing of non-RGBA images
		throw std::logic_error("writing of non RGBA iamges is currently not supported");
	}

	papki::file::guard file_guard(fi, papki::file::mode::create);

	png_structp png_ptr = nullptr;
	png_infop info_ptr = nullptr;

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (png_ptr == nullptr) {
		throw std::runtime_error("Could not allocate PNG write struct");
	}
	utki::scope_exit png_scope_exit([&png_ptr]() {
		png_destroy_write_struct(&png_ptr, nullptr);
	});

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) {
		throw std::runtime_error("Could not allocate PNG info struct");
	}
	utki::scope_exit info_scope_exit([&png_ptr, &info_ptr]() {
		png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	});

	auto dims = this->dims();

	png_set_write_fn(
		png_ptr,
		const_cast<papki::file*>(&fi), // TODO: why const_cast?
		&png_write_callback,
		&png_flush_callback
	);

	// write header (8 bit color depth)
	png_set_IHDR(
		png_ptr,
		info_ptr,
		dims.x(),
		dims.y(),
		8,
		PNG_COLOR_TYPE_RGBA, // TODO: support other color formats
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE
	);

	png_write_info(png_ptr, info_ptr);

	// write image data
	auto p = std::visit(
		[](const auto& im) {
			return reinterpret_cast<png_const_bytep>(im.pixels().data());
		},
		this->variant
	);
	auto sizeof_pixel = std::visit(
		[](const auto& im) {
			return sizeof(typename std::remove_reference_t<decltype(im)>::pixel_type);
		},
		this->variant
	);
	for (uint32_t y = 0; y != dims.y(); ++y, p += dims.x() * sizeof_pixel) {
		png_write_row(png_ptr, p);
	}

	png_write_end(png_ptr, nullptr);
}

image_variant rasterimage::read(const papki::file& fi)
{
	auto suffix = fi.suffix();

	if (suffix == "png") {
		return rasterimage::read_png(fi);
	} else if (suffix == "jpg") {
		return rasterimage::read_jpeg(fi);
	} else {
		throw std::invalid_argument("rasterimage::read(): unknown image file format, suffix = "s + suffix);
	}
}

namespace {
void png_read_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	auto fi = reinterpret_cast<papki::file*>(png_get_io_ptr(png_ptr));
	ASSERT(fi)

	// TODO: get number of bytes read and check for EOF, rise error if needed
	fi->read(utki::make_span(data, length));
}
} // namespace

image_variant rasterimage::read_png(const papki::file& fi)
{
	ASSERT(!fi.is_open())

	// open file
	papki::file::guard file_guard(fi);

	static const unsigned png_sig_size = 8; // the size of PNG signature

	{
		std::array<png_byte, png_sig_size> sig;
		auto span = utki::make_span(sig);

		std::fill(span.begin(), span.end(), 0);

		auto num_bytes_read = fi.read(span);
		if (num_bytes_read != span.size_bytes()) {
			throw std::invalid_argument("rasterimage::read_png(): could not read file signature");
		}

		// check that it is a PNG file
		if (png_sig_cmp(span.data(), 0, span.size_bytes()) != 0) {
			throw std::invalid_argument("rasterimage::read_png(): not a PNG file");
		}
	}

	// create internal PNG-structure to work with PNG file
	// (no warning and error callbacks)
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	utki::scope_exit png_scope_exit([&png_ptr]() {
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
	});

	// NOTE: the memory freed by png_destroy_read_struct()
	png_infop info_ptr = png_create_info_struct(png_ptr);
	utki::scope_exit info_scope_exit([&png_ptr, &info_ptr] {
		png_destroy_info_struct(png_ptr, &info_ptr);
	});

	png_set_sig_bytes(png_ptr, png_sig_size); // we've already read png_sig_size bytes

	// set custom "ReadFromFile" function
	png_set_read_fn(
		png_ptr,
		const_cast<papki::file*>(&fi), // TODO: why const_cast?
		png_read_callback
	);

	// read in all information about file
	png_read_info(png_ptr, info_ptr);

	// get information from info_ptr
	png_uint_32 width = 0;
	png_uint_32 height = 0;
	int bit_depth = 0;
	int color_format = 0;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_format, nullptr, nullptr, nullptr);

	// we want to convert tRNS transparency (e.g. single color treated as transparent) to proper alpha channel
	png_set_tRNS_to_alpha(png_ptr);

	// convert paletted PNG to rgb image
	if (color_format == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}

	// convert grayscale PNG to 8bit greyscale PNG
	if (color_format == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}

#if CFG_ENDIANNESS == CFG_ENDIANNESS_LITTLE
	// PNG stores 16 bit images in network order (big endian),
	// so we ask libpng to convert it to little endian
	png_set_swap(png_ptr);
#endif

	// set gamma information
	double gamma = 0.0f;

	// if there's gamma info in the file, set it to 2.2
	if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
		png_set_gamma(png_ptr, 2.2, gamma);
	} else {
		png_set_gamma(png_ptr, 2.2, 0.45455); // set to 0.45455 otherwise (good guess for GIF images on PCs)
	}

	// update info after all transformations
	png_read_update_info(png_ptr, info_ptr);

	// get all dimensions and color info again
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_format, nullptr, nullptr, nullptr);

	depth image_depth;

	// strip 16-bit png to 8-bit
	if (bit_depth == 16) {
		image_depth = depth::uint_16_bit;
	} else {
		image_depth = depth::uint_8_bit;
	}

	// set image type
	format image_format;
	switch (color_format) {
		case PNG_COLOR_TYPE_GRAY:
			image_format = format::grey;
			break;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			image_format = format::greya;
			break;
		case PNG_COLOR_TYPE_RGB:
			image_format = format::rgb;
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			image_format = format::rgba;
			break;
		default:
			throw std::invalid_argument("rasterimage::read_png(): unknown color_format");
	}

	image_variant im({width, height}, image_format, image_depth);

	// get PNG bytes per row
	png_size_t num_bytes_per_row = png_get_rowbytes(png_ptr, info_ptr);

	// check that our expectations are correct
	if (num_bytes_per_row != png_size_t(im.dims().x()) * png_size_t(im.num_channels())) {
		throw std::runtime_error("rasterimage::read_png(): number of bytes per row does not match expected value");
	}

	std::visit(
		[&](auto&& image) {
			using image_type = std::remove_reference_t<decltype(image)>;
			using depth_type = typename image_type::pixel_type::value_type;
			if constexpr ( //
				!std::is_same_v<depth_type, uint8_t> && //
				!std::is_same_v<depth_type, uint16_t> //
			)
			{
				// only 8 or 16 bit images are supported
				throw std::invalid_argument("rasterimage::read_png(): unsupported image depth");
			} else {
				ASSERT((num_bytes_per_row * height) == image.pixels().size_bytes())
				ASSERT(image.dims().y() != 0 && image.pixels().size() != 0)

				// make an array of row pointers
				std::vector<png_bytep> rows(image.dims().y());
				std::generate( //
					rows.begin(),
					rows.end(),
					[
#ifdef DEBUG
						&image,
#endif
						i = image.begin()]() mutable {
						ASSERT(i != image.end())
						auto ret = i->data();
						++i;
						return reinterpret_cast<decltype(rows)::value_type>(ret);
					}
				);

				// read in image data
				png_read_image(png_ptr, rows.data());
			}
		},
		im.variant
	);

	return im;
}

namespace {
const size_t jpeg_input_buffer_size = 4096;

struct data_manager_jpeg_source {
	jpeg_source_mgr pub;
	papki::file* fi;
	JOCTET* buffer;
	bool sof; // true if the file was just opened
};

void jpeg_init_source_callback(j_decompress_ptr cinfo)
{
	ASSERT(cinfo)
	auto src = reinterpret_cast<data_manager_jpeg_source*>(cinfo->src);
	ASSERT(src)
	src->sof = true;
}

// This function is calld when variable "bytes_in_buffer" reaches 0 and
// the necessarity in new portion of information appears.
// RETURNS: TRUE if the buffer is successfuly filled.
//          FALSE if i/o error occured
boolean jpeg_callback_fill_input_buffer(j_decompress_ptr cinfo)
{
	ASSERT(cinfo)
	auto src = reinterpret_cast<data_manager_jpeg_source*>(cinfo->src);
	ASSERT(src)

	// read in JPEGINPUTBUFFERSIZE JOCTET's
	size_t nbytes;

	try {
		auto buf_wrapper = utki::make_span(src->buffer, sizeof(JOCTET) * jpeg_input_buffer_size);
		ASSERT(src->fi)
		nbytes = src->fi->read(buf_wrapper);
	} catch (std::runtime_error&) {
		if (src->sof) {
			return FALSE; // the specified file is empty
		}
		// we read the data before. Insert End Of File info into the buffer
		src->buffer[0] = (JOCTET)(0xFF);
		src->buffer[1] = (JOCTET)(JPEG_EOI);
		nbytes = 2;
	} catch (...) {
		return FALSE; // error
	}

	// Set next input byte for JPEG and number of bytes read
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->sof = false; // the file is not empty since we read some data
	return TRUE; // operation successful
}

// skip num_bytes (seek forward)
void jpeg_callback_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	ASSERT(cinfo)
	auto src = reinterpret_cast<data_manager_jpeg_source*>(cinfo->src);
	ASSERT(src)
	if (num_bytes <= 0) {
		// nothing to skip
		return;
	}

	// read "num_bytes" bytes and waste them away
	while (num_bytes > long(src->pub.bytes_in_buffer)) {
		num_bytes -= long(src->pub.bytes_in_buffer);
		jpeg_callback_fill_input_buffer(cinfo);
	}

	// update current JPEG read position
	src->pub.next_input_byte += size_t(num_bytes);
	src->pub.bytes_in_buffer -= size_t(num_bytes);
}

// terminate source when decompress is finished
// (nothing to do in this function in our case)
void jpeg_callback_term_source(j_decompress_ptr cinfo) {}

} // namespace

image_variant rasterimage::read_jpeg(const papki::file& fi)
{
	ASSERT(!fi.is_open())

	papki::file::guard file_guard(fi);

	jpeg_decompress_struct cinfo; // decompression object
	jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&cinfo); // creat decompress object

	utki::scope_exit cinfo_scope_exit([&cinfo]() {
		jpeg_destroy_decompress(&cinfo); // clean decompression object
	});

	data_manager_jpeg_source* src = nullptr;

	// check if memory for JPEG-decompressor manager is allocated,
	// it is possible that several libraries accessing the source
	if (cinfo.src == nullptr) {
		// Allocate memory for our manager and set a pointer of global library
		// structure to it. We use JPEG library memory manager, this means that
		// the library will take care of memory freeing for us.
		// JPOOL_PERMANENT means that the memory is allocated for a whole
		// time  of working with the library.
		cinfo.src = reinterpret_cast<jpeg_source_mgr*>(
			(cinfo.mem->alloc_small)(j_common_ptr(&cinfo), JPOOL_PERMANENT, sizeof(data_manager_jpeg_source))
		);
		src = reinterpret_cast<data_manager_jpeg_source*>(cinfo.src);
		if (!src) {
			throw std::bad_alloc();
		}

		// allocate memory for read data
		src->buffer = reinterpret_cast<JOCTET*>(
			(cinfo.mem->alloc_small)(j_common_ptr(&cinfo), JPOOL_PERMANENT, jpeg_input_buffer_size * sizeof(JOCTET))
		);

		if (!src->buffer) {
			throw std::bad_alloc();
		}
	} else {
		src = reinterpret_cast<data_manager_jpeg_source*>(cinfo.src);
	}

	// set handler functions
	src->pub.init_source = &jpeg_init_source_callback;
	src->pub.fill_input_buffer = &jpeg_callback_fill_input_buffer;
	src->pub.skip_input_data = &jpeg_callback_skip_input_data;
	src->pub.resync_to_restart = &jpeg_resync_to_restart; // use default func
	src->pub.term_source = &jpeg_callback_term_source;
	// set the fields of our structure
	src->fi = const_cast<papki::file*>(&fi);
	// set pointers to the buffers
	src->pub.bytes_in_buffer = 0; // forces fill_input_buffer on first read
	src->pub.next_input_byte = nullptr; // until buffer loaded

	jpeg_read_header(&cinfo, TRUE); // read parametrs of a JPEG file

	jpeg_start_decompress(&cinfo); // start decompression

	utki::scope_exit decompress_scope_exit([&cinfo]() {
		jpeg_finish_decompress(&cinfo); // finish file decompression
	});

	format image_format = to_format(cinfo.output_components);

	image_variant im({cinfo.output_width, cinfo.output_height}, image_format, depth::uint_8_bit);

	// calculate the size of a row in bytes
	auto num_bytes_in_row = size_t(im.dims().x() * im.num_channels());

	// Allocate memory for one row. It is an array of rows which
	// contains only one row. JPOOL_IMAGE means that the memory is allocated
	// only for time of this image reading. So, no need to free the memory explicitly.
	JSAMPARRAY buffer = (cinfo.mem->alloc_sarray)(j_common_ptr(&cinfo), JPOOL_IMAGE, num_bytes_in_row, 1);

	std::visit(
		[&cinfo, &buffer, num_bytes_in_row](auto&& image) {
#ifdef DEBUG
			using image_type = std::remove_reference_t<decltype(image)>;
			using depth_type = typename image_type::pixel_type::value_type;
			ASSERT(std::is_same_v<depth_type, uint8_t>)
#endif
			auto i = image.begin();
			for (int y = 0; cinfo.output_scanline < image.dims().y(); ++y, ++i) {
				// read the string into buffer
				jpeg_read_scanlines(&cinfo, buffer, 1);

				ASSERT(num_bytes_in_row == i->size_bytes())

				std::copy(*buffer, *buffer + num_bytes_in_row, reinterpret_cast<uint8_t*>(i->data()));
			}
		},
		im.variant
	);

	return im;
}
