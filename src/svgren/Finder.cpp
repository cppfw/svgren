#include "Finder.hxx"

#include <utki/debug.hpp>

#include <svgdom/Visitor.hpp>

using namespace svgren;

Finder::Finder(const svgdom::Element& root) :
	root(root)
{
}

namespace{
class FindByIdVisitor : virtual public svgdom::Visitor{
	const std::string& id;
public:
	FindByIdVisitor(const std::string& id) :
			id(id)
	{}
	
	Finder::ElementInfo found;
	
	void visitContainer(const svgdom::Element& e, const svgdom::Container& c, const svgdom::Styleable& s){
		this->found.ss.stack.push_back(s);
		if(this->id == e.id){
			this->found.e = &e;
			return;
		}
		c.relayAccept(*this);
		if(this->found){
			return;
		}
		this->found.ss.stack.pop_back();
	}
	void visitElement(const svgdom::Element& e, const svgdom::Styleable& s){
		this->found.ss.stack.push_back(s);
		if(this->id == e.id){
			this->found.e = &e;
			return;
		}
		this->found.ss.stack.pop_back();
	}
	void defaultVisit(const svgdom::Element& e) override{
		if(this->id == e.id){
			this->found.e = &e;
			return;
		}
	}
	
	void visit(const svgdom::GElement& e) override{
		this->visitContainer(e, e, e);
	}
	void visit(const svgdom::SymbolElement& e) override{
		this->visitContainer(e, e, e);
	}
	void visit(const svgdom::SvgElement& e) override{
		this->visitContainer(e, e, e);
	}
	void visit(const svgdom::RadialGradientElement& e) override{
		this->visitContainer(e, e, e);
	}
	void visit(const svgdom::LinearGradientElement& e) override{
		this->visitContainer(e, e, e);
	}
	void visit(const svgdom::DefsElement& e) override{
		this->visitContainer(e, e, e);
	}
	void visit(const svgdom::FilterElement& e) override{
		this->visitContainer(e, e, e);
	}
	
	void visit(const svgdom::PolylineElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::CircleElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::UseElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::Gradient::StopElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::PathElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::RectElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::LineElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::EllipseElement& e) override{
		this->visitElement(e, e);
	}
	void visit(const svgdom::PolygonElement& e) override{
		this->visitElement(e, e);
	}	
	void visit(const svgdom::FeGaussianBlurElement& e) override{
		this->visitElement(e, e);
	}
};
}

const Finder::ElementInfo& Finder::findById(const std::string& id) {
	auto i = this->cache.find(id);
	if(i != this->cache.end()){
		return i->second;
	}
	
	FindByIdVisitor visitor(id);
	
	//for empty ID we don't search, but just create an empty entry in cache
	if(id.length() != 0){
		this->root.accept(visitor);
	}
	
	auto ret = this->cache.insert(std::make_pair(id, visitor.found));
	ASSERT(ret.second)
	return ret.first->second;
}

void Finder::clearCache() {
	this->cache.clear();
}
