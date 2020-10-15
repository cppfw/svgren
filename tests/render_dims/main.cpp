#include "../../src/svgren/render.hpp"

#include <utki/debug.hpp>
#include <utki/config.hpp>

#include <papki/fs_file.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv){
	{
		auto dom = svgdom::load(papki::fs_file("camera.svg"));

		ASSERT_ALWAYS(dom)

		svgren::parameters p;
		p.dims_request = decltype(p.dims_request){10, 10};
		auto res = svgren::render(*dom, p);

		ASSERT_ALWAYS(res.dims == p.dims_request)
		
//		TRACE(<< "imWidth = " << imWidth << " imHeight = " << imHeight << " img.size() = " << img.size() << std::endl)
	}
}
