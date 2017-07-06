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
		XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 1, 1, width, height);
		break;
	case ButtonPress:
		exit(0);
	}
}
#endif

int main(int argc, char **argv){
	auto dom = svgdom::load(papki::FSFile("../samples/testdata/tiger_with_smooth_cubic_curves.svg"));
//	auto dom = svgdom::load(papki::FSFile("tiger.svg"));
	
	ASSERT_ALWAYS(dom)
	
	unsigned imWidth = 0;
	unsigned imHeight = 0;
	auto img = svgren::render(*dom, imWidth, imHeight, 96, false);
	
	TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)

	{
		papki::FSFile file("out.data");
		papki::File::Guard guard(file, papki::File::E_Mode::CREATE);
		file.write(utki::Buf<std::uint8_t>(reinterpret_cast<std::uint8_t*>(&*img.begin()), img.size() * 4));
	}

	for(auto& c : img){
		c = (c & 0xff00ff00) | ((c << 16) & 0xff0000) | ((c >> 16) & 0xff);
	}
	
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

	ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(&*img.begin()), imWidth, imHeight, 8, 0);
	
	XSelectInput(display, window, ButtonPressMask|ExposureMask);
	
	XMapWindow(display, window);
	
	while(true){
		processEvent(display, window, ximage, imWidth, imHeight);
	}
#endif

	TRACE_ALWAYS(<< "[PASSED]" << std::endl)
}
