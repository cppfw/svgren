#include "../../src/svgren/render.hpp"

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
		
	auto dom = svgdom::load(papki::FSFile(filename));
	
	ASSERT_ALWAYS(dom)
	
	unsigned imWidth = 0;
	unsigned imHeight = 0;
	auto img = svgren::render(*dom, imWidth, imHeight);
	
	TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)

	writePng(outFilename.c_str(), imWidth, imHeight, &*img.begin());

	
#if M_OS == M_OS_LINUX
	int width = imWidth + 2;
	int height = imHeight + 2;

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
				unsigned imWidth = 0;
				unsigned imHeight = 0;
				
				XGetGeometry(display, window, &dummyWindow, &dummyInt, &dummyInt, &imWidth, &imHeight, &dummyUnsigned, &dummyUnsigned);
				
				imWidth = std::max(int(imWidth) - 2, 0);
				imHeight = std::max(int(imHeight) - 2, 0);
				
				TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << std::endl)
				
				auto img = svgren::render(*dom, imWidth, imHeight, 96, true);

				TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)
				
				auto ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(&*img.begin()), imWidth, imHeight, 8, 0);
				utki::ScopeExit scopeexit([ximage](){
					ximage->data = nullptr;//Xlib will try to deallocate data which is owned by 'img' variable.
					XDestroyImage(ximage);
				});
				
				XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 1, 1, imWidth, imHeight);
			}
			break;
		case ButtonPress:
			exit(0);
		}
	}
#endif

	TRACE_ALWAYS(<< "[PASSED]" << std::endl)
}
