#include <utki/debug.hpp>
#include <papki/fs_file.hpp>

#include "../../src/svgren/render.hpp"

#include <thread>

int main(int argc, char** argv) {
	if(argc < 2){
		std::cerr << "at least 1 argument expected (SVG filename)" << std::endl;
		return 1;
	}
	
	std::vector<std::unique_ptr<svgdom::svg_element>> svgs;
	
	for(int i = 1; i != argc; ++i){
		svgs.push_back(svgdom::load(papki::fs_file(argv[i])));
	}
	
	std::vector<std::thread> threads;
	
	for(auto& svg : svgs){
		decltype(svg.get()) s = svg.get();
		threads.emplace_back(
				[s](){
					auto res = svgren::render(*s);
					TRACE_ALWAYS(<< "rendered, width = " << res.width << std::endl)
				}
			);
	}
	
	for(auto& t : threads){
		t.join();
	}
	
	return 0;
}

