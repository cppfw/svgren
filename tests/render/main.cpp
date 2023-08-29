#include "../../src/svgren/render.hpp"

#include <chrono>

#include <utki/debug.hpp>
#include <utki/config.hpp>
#include <utki/time.hpp>

#include <papki/fs_file.hpp>

#include <rasterimage/image_variant.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <png.h>

#if M_OS == M_OS_LINUX
#	include <X11/Xlib.h>
#	include <X11/Xutil.h>
#endif

#ifdef assert
#	undef assert
#endif

// NOLINTNEXTLINE(bugprone-exception-escape): fatal exceptions are not caught
int main(int argc, char **argv){
	std::string filename;
	std::string out_filename;
	switch(argc){
		case 0:
		case 1:
			std::cout << "Warning: 2 arguments expected: <in-svg-file> <out-png-file>" << std::endl;
			std::cout << "\t Got 0 arguments, assume <in-svg-file>=tiger.svg <out-png-file>=tiger.png" << std::endl;
			filename = "tiger.svg";
			out_filename = "tiger.png";
			break;
		case 2:
			std::cout << "Warning: 2 arguments expected: <in-svg-file> <out-png-file>" << std::endl;
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			filename = argv[1];
			{
				auto dot_index = filename.find_last_of(".", filename.size());
				if(dot_index == std::string::npos){
					dot_index = filename.size();
				}
				out_filename = filename.substr(0, dot_index) + ".png";
			}
			std::cout << "\t Got 1 argument, assume <in-svg-file>=" << filename << " <out-png-file>=" << out_filename << std::endl;
			break;
		default:
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			filename = argv[1];
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			out_filename = argv[2];
			break;
	}

#ifdef DEBUG
	auto loadStart = utki::get_ticks_ms();
#endif
	
	auto dom = svgdom::load(papki::fs_file(filename));
	utki::assert(dom, SL);
	
	LOG([&](auto&o){o << "SVG loaded in " << float(utki::get_ticks_ms() - loadStart) / 1000.0f << " sec." << std::endl;})
	
	auto render_start_ms = utki::get_ticks_ms();
	
	auto image = rasterimage::image_variant(svgren::rasterize(*dom));

	const auto& img = image.get<rasterimage::format::rgba>();
	
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
	utki::log([&](auto&o){o << "SVG rendered in " << float(utki::get_ticks_ms() - render_start_ms) / 1000.0f << " sec." << std::endl;});
	
	utki::log([&](auto&o){o << "img.dims = " << img.dims() << " img.pixels.size() = " << img.pixels().size() << std::endl;});

	image.write_png(papki::fs_file(out_filename));
	
#if M_OS == M_OS_LINUX
	auto width = int(img.dims().x() + 2);
	auto height = int(img.dims().y() + 2);

	Display *display = XOpenDisplay(nullptr);
	
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)
	Visual *visual = DefaultVisual(display, 0);
	
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)
	Window window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, width, height, 1, 0, 0);
	
	if(visual->c_class != TrueColor){
		utki::log([](auto&o){o << "Cannot handle non true color visual ...\n" << std::endl;});
		return 1;
	}	
	
	XSelectInput(display, window, ButtonPressMask|ExposureMask|KeyPressMask);
	
	XMapWindow(display, window);
	
	while(true){
		XEvent ev;
		XNextEvent(display, &ev);
		switch(ev.type){
			default:
				break;
			case Expose:
				{
					int dummy_int = 0;
					unsigned dummy_unsigned = 0;
					Window dummy_window = 0;
					unsigned win_width = 0;
					unsigned win_height = 0;
					
					XGetGeometry(display, window, &dummy_window, &dummy_int, &dummy_int, &win_width, &win_height, &dummy_unsigned, &dummy_unsigned);
										
					using std::max;
					
					svgren::parameters p;
					p.dpi = svgren::parameters::default_dpi;
					p.dims_request.x() = max(int(win_width) - 2, 0);
					p.dims_request.y() = max(int(win_height) - 2, 0);
					auto img = svgren::rasterize(*dom, p);

					img.swap_red_blue();

					// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
					auto ximage = XCreateImage(display, visual, utki::byte_bits * 3, ZPixmap, 0, reinterpret_cast<char*>(img.pixels().data()), img.dims().x(), img.dims().y(), utki::byte_bits, 0);
					utki::scope_exit scope_exit([ximage](){
						ximage->data = nullptr; // Xlib will try to deallocate data which is owned by 'img' variable.
						XDestroyImage(ximage);
					});
					
					// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)
					XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 1, 1, img.dims().x(), img.dims().y());
				}
				break;
			case KeyPress:
			case ButtonPress:
				exit(0);
				break;
		}
	}
#endif

	utki::log([](auto&o){o << "[PASSED]" << std::endl;});
}
