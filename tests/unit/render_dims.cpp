#include <tst/set.hpp>
#include <tst/check.hpp>

#include <papki/fs_file.hpp>

#include "../../src/svgren/render.hpp"

#ifdef assert
#	undef assert
#endif

namespace{
tst::set set("render_dims", [](tst::suite& suite){
	suite.add(
		"requested_dimensions",
		[](){
			auto dom = svgdom::load(papki::fs_file("samples_data/camera.svg"));

			utki::assert(dom, SL);

			svgren::parameters p;
			p.dims_request = decltype(p.dims_request){10, 10};
			auto res = svgren::rasterize(*dom, p);

			utki::assert(res.dims() == p.dims_request, SL);
		}
	);
});
}
