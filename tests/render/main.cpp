#include "../../src/svgren/render.hpp"

#include <utki/debug.hpp>
#include <utki/config.hpp>

#include <papki/FSFile.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if M_OS == M_OS_LINUX
#	include <X11/Xlib.h>
#endif


#if M_OS == M_OS_LINUX
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
#endif

int main(int argc, char **argv){
	auto dom = svgdom::load(papki::FSFile("tiger.svg"));
	
	ASSERT_ALWAYS(dom)
	
	
#if M_OS == M_OS_LINUX
	XImage *ximage;
	
	int width = 800, height=800;

	Display *display = XOpenDisplay(NULL);
	
	Visual *visual = DefaultVisual(display, 0);
	
	Window window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, width, height, 1, 0, 0);
	
	if(visual->c_class != TrueColor){
		TRACE_ALWAYS(<< "Cannot handle non true color visual ...\n" << std::endl)
		return 1;
	}
#endif
	
	unsigned imWidth = 0;
	unsigned imHeight = 0;
	auto img = svgren::render(*dom, imWidth, imHeight);
	
	TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << std::endl)
	
	for(auto& c : img){
		c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
	}
	
#if M_OS == M_OS_LINUX
	ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(&*img.begin()), imWidth, imHeight, 8, 0);
	
	XSelectInput(display, window, ButtonPressMask|ExposureMask);
	
	XMapWindow(display, window);
	
	while(true){
		processEvent(display, window, ximage, imWidth, imHeight);
	}
#endif

	TRACE_ALWAYS(<< "[PASSED]" << std::endl)
}
