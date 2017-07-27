#include "Finder.hxx"

#include <svgdom/Visitor.hpp>

using namespace svgren;

Finder::Finder(const svgdom::Element& root) :
	root(root)
{
}

namespace{
class FindByIdVisitor : svgdom::Visitor{
public:
	const svgdom::Element* found = nullptr;
	
	void visitContainer(const svgdom::Container& c){
		//TODO:
	}
	void visitElement(const svgdom::Element& e){
		//TODO:
	}
	
	void visit(const svgdom::GElement& e) override{
		this->visitContainer(e);
	}
	void visit(const svgdom::SymbolElement& e) override{
		this->visitContainer(e);
	}
	void visit(const svgdom::SvgElement& e) override{
		this->visitContainer(e);
	}
	void visit(const svgdom::RadialGradientElement& e) override{
		this->visitContainer(e);
	}
	void visit(const svgdom::LinearGradientElement& e) override{
		this->visitContainer(e);
	}
	void visit(const svgdom::DefsElement& e) override{
		this->visitContainer(e);
	}

	void visit(const svgdom::UseElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::LineElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::EllipseElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::CircleElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::PolygonElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::Gradient::StopElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::PolylineElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::PathElement& e) override{
		this->visitElement(e);
	}
	void visit(const svgdom::RectElement& e) override{
		this->visitElement(e);
	}
};
}

Finder::ElementInfo Finder::findById(const std::string& id) {
	//TODO:
	return ElementInfo();
}
