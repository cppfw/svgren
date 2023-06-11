#include <tst/set.hpp>
#include <tst/check.hpp>

#include <regex>

#include <png.h>

#include <utki/config.hpp>
#include <utki/span.hpp>
#include <r4/vector.hpp>
#include <papki/fs_file.hpp>
#include <svgdom/dom.hpp>

#include <rasterimage/image_variant.hpp>

#include "../../src/svgren/config.hxx"
#include "../../src/svgren/render.hpp"

namespace{
const unsigned tolerance = 10;

const std::string data_dir = "samples_data/";

const std::string render_backend_name =
#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
    "cairo"
#elif SVGREN_BACKEND == SVGREN_BACKEND_AGG
    "agg"
#else
#   error "Unknown rendering backend"
#endif
;
}

class raster_image final{
public:
	/**
	 * @brief raster_image color depth.
	 */
	enum class color_depth{
		unknown = 0,
		grey  = 1, //1 channel. Only Grey channel
		greya = 2, //2 channels. Grey with Alpha channel
		rgb   = 3, //3 channels. Red Green Blue channels
		rgba  = 4  //4 channels. rgba format (4 channels)
	};

private:
	color_depth color_depth_v{color_depth::unknown};
	r4::vector2<unsigned> dim_v = 0;
	std::vector<uint8_t> buf_v; // image pixels data

public:
	/**
	 * @brief Default constructor.
	 * Creates uninitialized raster_image object.
	 */
	raster_image() = default;

	raster_image(const raster_image& im) = default;

	/**
	 * @brief Get image dimensions.
	 * @return raster_image dimensions.
	 */
	const r4::vector2<unsigned>& dims()const noexcept{
		return this->dim_v;
	}

	/**
	 * @brief Get color depth.
	 * @return Bits per pixel.
	 */
	unsigned bits_per_pixel()const{
		return this->num_channels() * 8;
	}

	/**
	 * @brief Get color depth.
	 * @return Number of color channels.
	 */
	unsigned num_channels()const{
		return unsigned(this->color_depth_v);
	}

	/**
	 * @brief Get color depth.
	 * @return Color depth type.
	 */
	color_depth depth()const{
		return this->color_depth_v;
	}

	/**
	 * @brief Get pixel data.
	 * @return Pixel data of the image.
	 */
	utki::span<uint8_t> buf(){
		return utki::make_span(this->buf_v);
	}

	/**
	 * @brief Get pixel data.
	 * @return Pixel data of the image.
	 */
	utki::span<const uint8_t> buf()const{
		return utki::make_span(this->buf_v);
	}

public:
	/**
	 * @brief Initialize this image object with given parameters.
	 * Pixel data remains uninitialized.
	 * @param dimensions - image dimensions.
	 * @param depth - color depth.
	 */
	void init(r4::vector2<unsigned> dimensions, color_depth depth){
		this->dim_v = dimensions;
		this->color_depth_v = depth;
		this->buf_v.resize(size_t(this->dims().x()) * size_t(this->dims().y()) * size_t(this->num_channels()));
	}

	/**
	 * @brief Flip image vertically.
	 */
	void flip_vertical();
	
private:
	static void png_custom_read_function(png_structp png_ptr, png_bytep data, png_size_t length){
		auto fi = reinterpret_cast<papki::file*>(png_get_io_ptr(png_ptr));
		tst::check(fi, SL);
	//	TRACE(<< "png_custom_read_function: fi = " << fi << " png_ptr = " << png_ptr << " data = " << std::hex << data << " length = " << length << std::endl)
		try{
			utki::span<png_byte> buf_wrapper(data, size_t(length));
			fi->read(buf_wrapper);
	//		TRACE(<< "png_custom_read_function: fi->Read() finished" << std::endl)
		}catch(...){
			// do not let any exception get out of this function
	//		TRACE(<< "png_custom_read_function: fi->Read() failed" << std::endl)
		}
	}
public:

	/**
	 * @brief Load image from PNG file.
	 * @param f - PNG file.
	 */
	void load_png(const papki::file& fi){
		tst::check(!fi.is_open(), SL);

		tst::check(this->buf_v.size() == 0, SL);

		papki::file::guard file_guard(fi); // this will guarantee that the file will be closed upon exit
//		TRACE(<< "raster_image::LoadPNG(): file opened" << std::endl)

	#define PNGSIGSIZE 8 // The size of PNG signature (max 8 bytes)
		std::array<png_byte, PNGSIGSIZE> sig;
		memset(sig.data(), 0, sig.size() * sizeof(sig[0]));

		{
			auto ret = // TODO: we should not rely on that it will always read the requested number of bytes (or should we?)

			fi.read(utki::make_span(sig));
			tst::check(ret == sig.size() * sizeof(sig[0]), SL);
		}

		if(png_sig_cmp(sig.data(), 0, sig.size() * sizeof(sig[0])) != 0){ // if it is not a PNG-file
			throw std::invalid_argument("raster_image::LoadPNG(): not a PNG file");
		}

		// Great!!! We have a PNG-file!
//		TRACE(<< "raster_image::LoadPNG(): file is a PNG" << std::endl)

		// Create internal PNG-structure to work with PNG file
		// (no warning and error callbacks)
		png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

		png_infop info_ptr = png_create_info_struct(png_ptr);// Create structure with file info

		png_set_sig_bytes(png_ptr, PNGSIGSIZE);// We've already read PNGSIGSIZE bytes

		// Set custom "ReadFromFile" function
		png_set_read_fn(png_ptr, const_cast<papki::file*>(&fi), png_custom_read_function);

		png_read_info(png_ptr, info_ptr); // Read in all information about file

		// Get information from info_ptr
		png_uint_32 width = 0;
		png_uint_32 height = 0;
		int bit_depth = 0;
		int color_type = 0;
		png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr);

		// Strip 16bit png  to 8bit
		if(bit_depth == 16){
			png_set_strip_16(png_ptr);
		}
		// Convert paletted PNG to rgb image
		if(color_type == PNG_COLOR_TYPE_PALETTE){
			png_set_palette_to_rgb(png_ptr);
		}
		// Convert grayscale PNG to 8bit greyscale PNG
		if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8){
			png_set_expand_gray_1_2_4_to_8(png_ptr);
		}
		// if(png_get_valid(png_ptr, info_ptr,PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

		// set gamma information
		double gamma = 0.0f;

		// if there's gamma info in the file, set it to 2.2
		if(png_get_gAMA(png_ptr, info_ptr, &gamma)){
			png_set_gamma(png_ptr, 2.2, gamma);
		}else{
			png_set_gamma(png_ptr, 2.2, 0.45455); // set to 0.45455 otherwise (good guess for GIF images on PCs)
		}

		// update info after all transformations
		png_read_update_info(png_ptr, info_ptr);
		// get all dimensions and color info again
		png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr);
		tst::check(bit_depth == 8, SL);

		// Set image type
		raster_image::color_depth image_type;
		switch(color_type){
			case PNG_COLOR_TYPE_GRAY:
				image_type = raster_image::color_depth::grey;
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				image_type = raster_image::color_depth::greya;
				break;
			case PNG_COLOR_TYPE_RGB:
				image_type = raster_image::color_depth::rgb;
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				image_type = raster_image::color_depth::rgba;
				break;
			default:
				throw std::invalid_argument("raster_image::LoadPNG(): unknown color_type");
				break;
		}
		// Great! Number of channels and bits per pixel are initialized now!

		// set image dimensions and set buffer size
		this->init(r4::vector2<unsigned>(width, height), image_type); // set buf array size (allocate memory)
		// Great! height and width are initialized and buffer memory allocated

//		TRACE(<< "raster_image::LoadPNG(): memory for image allocated" << std::endl)

		// Read image data
		png_size_t num_bytes_per_row = png_get_rowbytes(png_ptr, info_ptr); // get bytes per row

		// check that our expectations are correct
		if(num_bytes_per_row != png_size_t(this->dims().x()) * png_size_t(this->num_channels())){
			throw std::invalid_argument("raster_image::LoadPNG(): number of bytes per row does not match expected value");
		}

		tst::check((num_bytes_per_row * height) == this->buf_v.size(), SL);

//		TRACE(<< "raster_image::LoadPNG(): going to read in the data" << std::endl)
		{
			tst::check(this->dims().y() && this->buf_v.size(), SL);
			std::vector<png_bytep> rows(this->dims().y());
			// initialize row pointers
//			TRACE(<< "raster_image::LoadPNG(): this->buf.Buf() = " << std::hex << this->buf.Buf() << std::endl)
			for(unsigned i = 0; i < this->dims().y(); ++i){
				rows[i] = &*this->buf_v.begin() + i * num_bytes_per_row;
//				TRACE(<< "raster_image::LoadPNG(): rows[i] = " << std::hex << rows[i] << std::endl)
			}
//			TRACE(<< "raster_image::LoadPNG(): row pointers are set" << std::endl)
			// Read in image data!
			png_read_image(png_ptr, &*rows.begin());
//			TRACE(<< "raster_image::LoadPNG(): image data read" << std::endl)
		}

		png_destroy_info_struct(png_ptr, &info_ptr);
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
	}
};

namespace{
tst::set set("samples", [](tst::suite& suite){
    std::vector<std::string> files;

    {
		const std::regex suffix_regex("^.*\\.svg$");
		auto all_files = papki::fs_file(data_dir).list_dir();

		std::copy_if(
				all_files.begin(),
				all_files.end(),
				std::back_inserter(files),
				[&suffix_regex](auto& f){
					return std::regex_match(f, suffix_regex);
				}
			);
	}

    suite.add<std::string>(
        "sample",
		{
#if M_CPU_BITS != 64
			tst::flag::disabled
#endif
		},
        std::move(files),
        [](const auto& p){
            papki::fs_file in_file(data_dir + p);

            auto dom = svgdom::load(in_file);

            auto res = svgren::render(*dom);
            auto& img = res.pixels;

            papki::fs_file png_file(data_dir + render_backend_name + "/" + papki::not_suffix(in_file.not_dir()) + ".png");

			auto png_var = rasterimage::read_png(png_file);

            tst::check(!png_var.empty(), SL);

            tst::check(png_var.get_format() == rasterimage::format::rgba, SL) << "Error: PNG color format is not rgba: " << unsigned(png_var.get_format());
			tst::check(png_var.get_depth() == rasterimage::depth::uint_8_bit, SL) << "Error: PNG color depth is not 8 bit: " << unsigned(png_var.get_depth());;
            
            tst::check(res.dims == png_var.dims(), SL) << "Error: svg dims " << res.dims << " did not match PNG dims " << png_var.dims();

            tst::check(img.size() == png_var.buffer_size(), SL) << "Error: svg pixel buffer size (" << img.size() << ") did not match PNG pixel buffer size(" << png_var.buffer_size() << ")";

			const auto& png = png_var.get<rasterimage::format::rgba>();

            for(size_t i = 0; i != img.size(); ++i){
                std::array<uint8_t, 4> rgba;
                rgba[0] = img[i] & 0xff;
                rgba[1] = (img[i] >> 8) & 0xff;
                rgba[2] = (img[i] >> 16) & 0xff;
                rgba[3] = (img[i] >> 24) & 0xff;

                for(unsigned j = 0; j != rgba.size(); ++j){
                    auto c1 = rgba[j];
                    auto c2 = png.pixels()[i][j];
                    if(c1 > c2){
                        std::swap(c1, c2);
                    }

                    if(unsigned(c2 - c1) > tolerance){
						auto png_px = png.pixels()[i];

                        uint32_t pixel =
                            uint32_t(png_px.r()) |
                            (uint32_t(png_px.g()) << 8) |
                            (uint32_t(png_px.b()) << 16) |
                            (uint32_t(png_px.a()) << 24)
                        ;

                        tst::check(false, SL) << "Error: PNG pixel #" << std::dec << i << " [" << (i % res.dims.x()) << ", " << (i / res.dims.y()) << "]" << " (0x" << std::hex << pixel << ") did not match SVG pixel (0x" << img[i] << ")" << ", png_file = " << png_file.path();
                    }
                }
            }
        }
    );
});
}
