#include "../../src/svgren/render.hpp"

#include <utki/debug.hpp>
#include <utki/config.hpp>

#include <papki/FSFile.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int main(int argc, char **argv){
	{
		auto dom = svgdom::load(papki::FSFile("camera.svg"));

		ASSERT_ALWAYS(dom)

		unsigned imWidth = 10;
		unsigned imHeight = 10;
		auto img = svgren::render(*dom, imWidth, imHeight, 96, false);

		ASSERT_ALWAYS(imWidth == 10)
		ASSERT_ALWAYS(imHeight == 10)
		
//		TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)
	}
}
