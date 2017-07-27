#pragma once

#include <svgdom/elements/Element.hpp>

namespace svgren{

class Finder{
	
	const svgdom::Element& root;
	
public:
	
	Finder(const svgdom::Element& root);
	
	const svgdom::Element* findById(const std::string& id);
};
	
}
