#include "../../src/svgren/render.hpp"

#include <utki/debug.hpp>

#include <papki/FSFile.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

//XImage *CreateTrueColorImage(Display *display, Visual *visual, unsigned char *image, int width, int height)
//{
//	int i, j;
//	unsigned char *image32=(unsigned char *)malloc(width*height*4);
//	unsigned char *p=image32;
//	for(i=0; i<width; i++)
//	{
//		for(j=0; j<height; j++)
//		{
//			if((i<256)&&(j<256))
//			{
//				*p++=rand()%256; // blue
//				*p++=rand()%256; // green
//				*p++=rand()%256; // red
//			}
//			else
//			{
//				*p++=i%256; // blue
//				*p++=j%256; // green
//				if(i<256)
//					*p++=i%256; // red
//				else if(j<256)
//					*p++=j%256; // red
//				else
//					*p++=(256-j)%256; // red
//			}
//			p++;
//		}
//	}
//	return XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(image32), width, height, 32, 0);
//}

void processEvent(Display *display, Window window, XImage *ximage, int width, int height){
	XEvent ev;
	XNextEvent(display, &ev);
	switch(ev.type)
	{
	case Expose:
		XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, width, height);
		break;
	case ButtonPress:
		exit(0);
	}
}

int main(int argc, char **argv){
	auto dom = svgdom::load(papki::FSFile("tiger.svg"));
	
	ASSERT_ALWAYS(dom)
	
	
	
	XImage *ximage;
	
	int width=512, height=512;
	
	Display *display=XOpenDisplay(NULL);
	
	Visual *visual=DefaultVisual(display, 0);
	
	Window window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, width, height, 1, 0, 0);
	
	if(visual->c_class != TrueColor){
		TRACE_ALWAYS(<< "Cannot handle non true color visual ...\n" << std::endl)
		return 1;
	}

	auto img = svgren::render(*dom, 512, 512);
	
	ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(&*img.begin()), width, height, 8, 0);
	
	XSelectInput(display, window, ButtonPressMask|ExposureMask);
	
	XMapWindow(display, window);
	
	while(true){
		processEvent(display, window, ximage, width, height);
	}

	TRACE_ALWAYS(<< "[PASSED]" << std::endl)
}
