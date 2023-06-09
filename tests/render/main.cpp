#include "../../src/svgren/render.hpp"

#include <chrono>

#include <utki/debug.hpp>
#include <utki/config.hpp>

#include <papki/fs_file.hpp>

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

void write_png(const char* filename, int width, int height, uint32_t *buffer){
   FILE *fp = nullptr;
   png_structp png_ptr = nullptr;
   png_infop info_ptr = nullptr;
   
   // Open file for writing (binary mode)
   fp = fopen(filename, "w+b");
   if (fp == nullptr) {
      fprintf(stderr, "Could not open file %s for writing\n", filename);
	  std::stringstream ss;
	  ss << "could not open file '" << filename << "' for writing";
      throw std::system_error(errno, std::generic_category(), ss.str());
   }
   
   // Initialize write structure
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
   if (png_ptr == nullptr) {
      fprintf(stderr, "Could not allocate write struct\n");
      throw std::runtime_error("Could not allocate write struct");
   }

   // Initialize info structure
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == nullptr) {
      fprintf(stderr, "Could not allocate info struct\n");
      throw std::runtime_error("Could not allocate info struct");
   }
 
   png_init_io(png_ptr, fp);

   // Write header (8 bit colour depth)
   png_set_IHDR(png_ptr, info_ptr, width, height,
         8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);


   png_write_info(png_ptr, info_ptr);
   
   // Write image data
   int y;
   auto p = buffer;
   for (y=0 ; y<height ; y++, p += width) {
      png_write_row(png_ptr, reinterpret_cast<png_bytep>(p));
   }

   // End write
   png_write_end(png_ptr, nullptr);
 
   if (fp != nullptr) fclose(fp);
   if (info_ptr != nullptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
   if (png_ptr != nullptr) png_destroy_write_struct(&png_ptr, nullptr);
}

#ifdef DEBUG
namespace{
uint32_t getTicks(){
	return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
}
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
			filename = argv[1];
			out_filename = argv[2];
			break;
	}

#ifdef DEBUG
	auto loadStart = getTicks();
#endif
	
	auto dom = svgdom::load(papki::fs_file(filename));
	utki::assert(dom, SL);
	
	LOG([&](auto&o){o << "SVG loaded in " << float(getTicks() - loadStart) / 1000.0f << " sec." << std::endl;})
	
#ifdef DEBUG
	auto renderStart = getTicks();
#endif
	
	auto img = svgren::render(*dom);
	
	LOG([&](auto&o){o << "SVG rendered in " << float(getTicks() - renderStart) / 1000.0f << " sec." << std::endl;})
	
	LOG([&](auto&o){o << "img.dims = " << img.dims << " img.pixels.size() = " << img.pixels.size() << std::endl;})

	write_png(out_filename.c_str(), int(img.dims.x()), int(img.dims.y()), img.pixels.data());
	
#if M_OS == M_OS_LINUX
	auto width = int(img.dims.x() + 2);
	auto height = int(img.dims.y() + 2);

	Display *display = XOpenDisplay(nullptr);
	
	Visual *visual = DefaultVisual(display, 0);
	
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
			case Expose:
				{
					int dummy_int;
					unsigned dummy_unsigned;
					Window dummy_window;
					unsigned win_width = 0;
					unsigned win_height = 0;
					
					XGetGeometry(display, window, &dummy_window, &dummy_int, &dummy_int, &win_width, &win_height, &dummy_unsigned, &dummy_unsigned);
					
	//				TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << std::endl)
					
					using std::max;
					
					svgren::parameters p;
					p.dpi = 96;
					p.dims_request.x() = max(int(win_width) - 2, 0);
					p.dims_request.y() = max(int(win_height) - 2, 0);
					auto img = svgren::render(*dom, p);

					// RGBA -> BGRA
					for(auto &c : img.pixels){
						c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
					}

	//				TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)
					
					auto ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(img.pixels.data()), img.dims.x(), img.dims.y(), 8, 0);
					utki::scope_exit scope_exit([ximage](){
						ximage->data = nullptr; // Xlib will try to deallocate data which is owned by 'img' variable.
						XDestroyImage(ximage);
					});
					
					XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 1, 1, img.dims.x(), img.dims.y());
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
