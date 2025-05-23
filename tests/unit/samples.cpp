#include <tst/set.hpp>
#include <tst/check.hpp>

#include <regex>

#include <utki/config.hpp>
#include <utki/span.hpp>
#include <r4/vector.hpp>
#include <papki/fs_file.hpp>
#include <svgdom/dom.hpp>

#include <rasterimage/image_variant.hpp>
#include <rasterimage/operations.hpp>

#include "../../src/svgren/render.hpp"

namespace{
const unsigned tolerance = 10;

const std::string data_dir = "samples_data/";

const std::string expected_dir = "expected";
}

namespace{
const tst::set set("samples", [](tst::suite& suite){
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
        "sample__deprecated_render",
		{
#if CFG_CPU_BITS != 64
			tst::flag::disabled
#endif
		},
        files,
        [](const auto& p){
            papki::fs_file in_file(data_dir + p);

            auto dom = svgdom::load(in_file);

#if CFG_COMPILER == CFG_COMPILER_GCC || CFG_COMPILER == CFG_COMPILER_CLANG
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if CFG_COMPILER == CFG_COMPILER_MSVC
#	pragma warning(disable : 4996)
#endif
            // NOLINTNEXTLINE(clang-diagnostic-deprecated-declarations, "false positive")
            auto res = svgren::render(*dom);
#if CFG_COMPILER == CFG_COMPILER_MSVC
#	pragma warning(enable: 4996)
#endif
#if CFG_COMPILER == CFG_COMPILER_GCC
#	pragma GCC diagnostic pop
#endif

            auto& img = res.pixels;

            papki::fs_file png_file(data_dir + expected_dir + "/" + papki::not_suffix(in_file.not_dir()) + ".png");

			auto png_var = rasterimage::read_png(png_file);

            tst::check(!png_var.empty(), SL);

            tst::check(png_var.get_format() == rasterimage::format::rgba, SL) << "Error: PNG color format is not rgba: " << unsigned(png_var.get_format());
			tst::check(png_var.get_depth() == rasterimage::depth::uint_8_bit, SL) << "Error: PNG color depth is not 8 bit: " << unsigned(png_var.get_depth());;
            
            tst::check(res.dims == png_var.dims(), SL) << "Error: svg dims " << res.dims << " did not match PNG dims " << png_var.dims();

            tst::check(img.size() == png_var.buffer_size(), SL) << "Error: svg pixel buffer size (" << img.size() << ") did not match PNG pixel buffer size(" << png_var.buffer_size() << ")";

			const auto& png = png_var.get<rasterimage::format::rgba>();

            for(size_t i = 0; i != img.size(); ++i){
                std::array<uint8_t, 4> rgba = rasterimage::from_32bit_pixel(img[i]);

                for(unsigned j = 0; j != rgba.size(); ++j){
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    auto c1 = rgba[j];
                    auto c2 = png.pixels()[i][j];
                    if(c1 > c2){
                        std::swap(c1, c2);
                    }

                    if(unsigned(c2 - c1) > tolerance){
						auto png_px = png.pixels()[i];

                        uint32_t pixel = rasterimage::to_32bit_pixel(png_px);

                        tst::check(false, SL) << "Error: PNG pixel #" << std::dec << i << " [" << (i % res.dims.x()) << ", " << (i / res.dims.y()) << "]" << " (0x" << std::hex << pixel << ") did not match SVG pixel (0x" << img[i] << ")" << ", png_file = " << png_file.path();
                    }
                }
            }
        }
    );

	suite.add<std::string>(
        "sample",
		{
#if CFG_CPU_BITS != 64
			tst::flag::disabled
#endif
		},
        files,
        [](const auto& p){
            papki::fs_file in_file(data_dir + p);

            auto dom = svgdom::load(in_file);

            auto im = svgren::rasterize(*dom);

            papki::fs_file png_file(data_dir + expected_dir + "/" + papki::not_suffix(in_file.not_dir()) + ".png");

			auto png_var = rasterimage::read_png(png_file);

            tst::check(!png_var.empty(), SL);

            tst::check(png_var.get_format() == rasterimage::format::rgba, SL) << "Error: PNG color format is not rgba: " << unsigned(png_var.get_format());
			tst::check(png_var.get_depth() == rasterimage::depth::uint_8_bit, SL) << "Error: PNG color depth is not 8 bit: " << unsigned(png_var.get_depth());;
            
            tst::check(im.dims() == png_var.dims(), SL) << "Error: svg dims " << im.dims() << " did not match PNG dims " << png_var.dims();

            tst::check(im.pixels().size() == png_var.buffer_size(), SL) << "Error: svg pixel buffer size (" << im.pixels().size() << ") did not match PNG pixel buffer size(" << png_var.buffer_size() << ")";

			const auto& png = png_var.get<rasterimage::format::rgba>();

            for(size_t i = 0; i != im.pixels().size(); ++i){
                auto rgba = im.pixels()[i];
				auto png_px = png.pixels()[i];

                for(unsigned j = 0; j != rgba.size(); ++j){
                    auto c1 = rgba[j];
                    auto c2 = png_px[j];
                    if(c1 > c2){
                        std::swap(c1, c2);
                    }

                    if(unsigned(c2 - c1) > tolerance){
                        uint32_t png_pixel = rasterimage::to_32bit_pixel(png_px);
                        uint32_t svg_pixel = rasterimage::to_32bit_pixel(rgba);

                        tst::check(false, SL) << "Error: PNG pixel #" << std::dec << i << " [" << (i % im.dims().x()) << ", " << (i / im.dims().y()) << "]" << " (0x" << std::hex << png_pixel << ") did not match SVG pixel (0x" << svg_pixel << ")" << ", png_file = " << png_file.path();
                    }
                }
            }
        }
    );
});
}
