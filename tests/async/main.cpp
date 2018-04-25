#include <utki/debug.hpp>
#include <papki/FSFile.hpp>

#include "../../src/svgren/render.hpp"

#include <thread>


int main(int argc, char** argv) {
	if(argc < 2){
		std::cerr << "1 argument expected (SVG filename)" << std::endl;
		return 1;
	}
	
	auto dom = svgdom::load(papki::FSFile(argv[1]));
	
	std::thread t1([&dom](){
		auto res = svgren::render(*dom);
		TRACE_ALWAYS(<< "t1 rendered, width = " << res.width << std::endl)
	});
	
	std::thread t2([&dom](){
		auto res = svgren::render(*dom);
		TRACE_ALWAYS(<< "t2 rendered, width = " << res.width << std::endl)
	});
	
	
	t1.join();
	t2.join();
	
	return 0;
}

