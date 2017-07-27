#pragma once

#include <vector>

#include <svgdom/elements/Styleable.hpp>

namespace svgren{
class StyleStack{
public:
	std::vector<const svgdom::Styleable*> stack;
	
	const svgdom::StylePropertyValue* getStyleProperty(svgdom::StyleProperty_e p);
	
	class Push{
		StyleStack& ss;
	public:
		Push(StyleStack& ss, const svgdom::Styleable& s);
		~Push()noexcept;
	};
};
}
