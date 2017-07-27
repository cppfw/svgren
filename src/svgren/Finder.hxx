#pragma once

#include <map>

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
	
	const ElementInfo& findById(const std::string& id);
	
	void clearCache();
	
private:
	std::map<std::string, ElementInfo> cache;
};
	
}
