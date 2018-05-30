#include "../../src/svgren/render.hpp"

#include <chrono>

#include <utki/debug.hpp>
#include <utki/config.hpp>

#include <papki/FSFile.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#if M_OS == M_OS_LINUX
#	include <X11/Xlib.h>
#	include <X11/Xutil.h>
#endif



int writePng(const char* filename, int width, int height, std::uint32_t *buffer)
{
   int code = 0;
   FILE *fp = NULL;
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL;
   
   // Open file for writing (binary mode)
   fp = fopen(filename, "w+b");
   if (fp == NULL) {
      fprintf(stderr, "Could not open file %s for writing\n", filename);
      code = 1;
      throw utki::Exc("Could not open file for writing");
   }
   
   // Initialize write structure
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (png_ptr == NULL) {
      fprintf(stderr, "Could not allocate write struct\n");
      code = 1;
      throw utki::Exc("Could not allocate write struct");
   }

   // Initialize info structure
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL) {
      fprintf(stderr, "Could not allocate info struct\n");
      code = 1;
      throw utki::Exc("Could not allocate info struct");
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
   png_write_end(png_ptr, NULL);
 
   if (fp != NULL) fclose(fp);
   if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
   if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

   return code;
}

namespace{
std::uint32_t getTicks(){
	return std::uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
}


int main(int argc, char **argv){
	std::string filename;
	std::string outFilename;
	switch(argc){
		case 0:
		case 1:
			std::cout << "Warning: 2 arguments expected: <in-svg-file> <out-png-file>" << std::endl;
			std::cout << "\t Got 0 arguments, assume <in-svg-file>=tiger.svg <out-png-file>=tiger.png" << std::endl;
			filename = "tiger.svg";
			outFilename = "tiger.png";
			break;
		case 2:
			std::cout << "Warning: 2 arguments expected: <in-svg-file> <out-png-file>" << std::endl;
			filename = argv[1];
			{
				auto dotIndex = filename.find_last_of(".", filename.size());
				if(dotIndex == std::string::npos){
					dotIndex = filename.size();
				}
				outFilename = filename.substr(0, dotIndex) + ".png";
			}
			std::cout << "\t Got 1 argument, assume <in-svg-file>=" << filename << " <out-png-file>=" << outFilename << std::endl;
			break;
		default:
			filename = argv[1];
			outFilename = argv[2];
			break;
	}
	
	auto loadStart = getTicks();
	
	auto dom = svgdom::load(papki::FSFile(filename));
	ASSERT_ALWAYS(dom)
	
	TRACE(<< "SVG loaded in " << float(getTicks() - loadStart) / 1000.0f << " sec." << std::endl)
	
	
	auto renderStart = getTicks();
	
	auto img = svgren::render(*dom);
	
	TRACE(<< "SVG rendered in " << float(getTicks() - renderStart) / 1000.0f << " sec." << std::endl)
	
	TRACE(<< "imWidth = " << img.width << " imHeight = " << img.height << " img.size() = " << img.pixels.size() << std::endl)

	writePng(outFilename.c_str(), img.width, img.height, &*img.pixels.begin());

	
#if M_OS == M_OS_LINUX
	int width = img.width + 2;
	int height = img.height + 2;

	Display *display = XOpenDisplay(NULL);
	
	Visual *visual = DefaultVisual(display, 0);
	
	Window window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, width, height, 1, 0, 0);
	
	if(visual->c_class != TrueColor){
		TRACE_ALWAYS(<< "Cannot handle non true color visual ...\n" << std::endl)
		return 1;
	}	
	
	XSelectInput(display, window, ButtonPressMask|ExposureMask);
	
	XMapWindow(display, window);
	
	while(true){
		XEvent ev;
		XNextEvent(display, &ev);
		switch(ev.type)
		{
		case Expose:
			{
				int dummyInt;
				unsigned dummyUnsigned;
				Window dummyWindow;
				unsigned winWidth = 0;
				unsigned winHeight = 0;
				
				XGetGeometry(display, window, &dummyWindow, &dummyInt, &dummyInt, &winWidth, &winHeight, &dummyUnsigned, &dummyUnsigned);
				
//				TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << std::endl)
				
				using std::max;
				
				svgren::Parameters p;
				p.bgra = true;
				p.dpi = 96;
				p.widthRequest = max(int(winWidth) - 2, 0);
				p.heightRequest = max(int(winHeight) - 2, 0);
				auto img = svgren::render(*dom, p);

//				TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)
				
				auto ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(&*img.pixels.begin()), img.width, img.height, 8, 0);
				utki::ScopeExit scopeexit([ximage](){
					ximage->data = nullptr;//Xlib will try to deallocate data which is owned by 'img' variable.
					XDestroyImage(ximage);
				});
				
				XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 1, 1, img.width, img.height);
			}
			break;
		case ButtonPress:
			exit(0);
		}
	}
#endif

	TRACE_ALWAYS(<< "[PASSED]" << std::endl)
}
