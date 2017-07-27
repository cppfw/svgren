#pragma once

#include <svgdom/elements/Element.hpp>

#include "StyleStack.hxx"

namespace svgren{

class Finder{
	
	const svgdom::Element& root;
	
public:
	
	Finder(const svgdom::Element& root);
	
	struct ElementInfo{
		const svgdom::Element* e = nullptr;
		StyleStack ss;
		
		explicit operator bool()const{
			return this->e;
		}
	};
	
	ElementInfo findById(const std::string& id);
};
	
}
