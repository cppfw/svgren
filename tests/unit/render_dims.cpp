#include <tst/set.hpp>
#include <tst/check.hpp>

#include <papki/fs_file.hpp>

#include "../../src/svgren/render.hpp"

namespace{
tst::set set("render_dims", [](tst::suite& suite){
	suite.add(
		"requested_dimensions",
		[](){
			auto dom = svgdom::load(papki::fs_file("samples_data/camera.svg"));

			ASSERT_ALWAYS(dom)

			svgren::parameters p;
			p.dims_request = decltype(p.dims_request){10, 10};
			auto res = svgren::render(*dom, p);

			ASSERT_ALWAYS(res.dims == p.dims_request)
		}
	);
});
}
