/* 
 * File:   render.cpp
 * Author: ivan
 * 
 * Created on October 12, 2015, 11:04 PM
 */

#include "render.hpp"

#include <cairo/cairo.h>

#include <utki/util.hpp>

using namespace svgren;



namespace{

class Renderer : public svgdom::Renderer{
	cairo_t* cr;
public:
	Renderer(cairo_t* cr) :
			cr(cr)
	{}
	
	void render(const svgdom::PathElement& e)const override{
		cairo_set_source_rgb(cr, 1.0, 0, 0);
		
//		const svgdom::PathElement::Step* prev = nullptr;
		for(auto& s : e.path){
			switch(s.type){
				case svgdom::PathElement::Step::EType::MOVE_ABS:
					cairo_move_to(this->cr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::MOVE_REL:
					cairo_rel_move_to(this->cr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::LINE_ABS:
					cairo_line_to(this->cr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::LINE_REL:
					cairo_rel_line_to(this->cr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::HORIZONTAL_LINE_ABS:
					{
						double x, y;
						if(cairo_has_current_point(this->cr)){
							cairo_get_current_point(this->cr, &x, &y);
						}else{
							y = 0;
						}
						cairo_line_to(this->cr, s.x, y);
					}
					break;
				case svgdom::PathElement::Step::EType::HORIZONTAL_LINE_REL:
					{
						double x, y;
						if(cairo_has_current_point(this->cr)){
							cairo_get_current_point(this->cr, &x, &y);
						}else{
							y = 0;
						}
						cairo_rel_line_to(this->cr, s.x, y);
					}
					break;
				case svgdom::PathElement::Step::EType::VERTICAL_LINE_ABS:
					{
						double x, y;
						if(cairo_has_current_point(this->cr)){
							cairo_get_current_point(this->cr, &x, &y);
						}else{
							x = 0;
						}
						cairo_line_to(this->cr, x, s.y);
					}
					break;
				case svgdom::PathElement::Step::EType::VERTICAL_LINE_REL:
					{
						double x, y;
						if(cairo_has_current_point(this->cr)){
							cairo_get_current_point(this->cr, &x, &y);
						}else{
							x = 0;
						}
						cairo_rel_line_to(this->cr, x, s.y);
					}
					break;
				case svgdom::PathElement::Step::EType::CLOSE:
					cairo_close_path(this->cr);
					break;
				case svgdom::PathElement::Step::EType::CUBIC_ABS:
					cairo_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::CUBIC_REL:
					cairo_rel_curve_to(this->cr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
					break;
//				case svgdom::PathElement::Step::EType::CUBIC_SMOOTH_ABS:
//					{
//						double x, y;
//						if(cairo_has_current_point(this->cr)){
//							cairo_get_current_point(this->cr, &x, &y);
//						}else{
//							x = 0;
//							y = 0;
//						}
//						double x1, y1;
//						if(prev){
//							x1 = -(prev->x2 - x) + x;
//							y1 = -(prev->y2 - y) + y;
//						}else{
//							x1 = x;
//							y1 = y;
//						}
//						cairo_curve_to(this->cr, x1, y1, s.x2, s.y2, s.x, s.y);
//					}
//					break;
				default:
					ASSERT(false)
					break;
			}
//			prev = &s;
		}
				
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	}
};

}



std::vector<std::uint32_t> svgren::render(const svgdom::SvgElement& svg, unsigned width, unsigned height){
//	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	int stride = width * 4;
	
	TRACE(<< "width = " << width << " stride = " << stride / 4 << std::endl)
	
	std::vector<std::uint32_t> ret((stride / sizeof(std::uint32_t)) * height);
	
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
			reinterpret_cast<unsigned char*>(&*ret.begin()),
			CAIRO_FORMAT_ARGB32,
			width,
			height,
			stride
		);
	if(!surface){
		ret.clear();
		return ret;
	}
	utki::ScopeExit scopeExitSurface([&surface](){
		cairo_surface_destroy(surface);
	});
	
	cairo_t* cr = cairo_create(surface);
	if(!cr){
		ret.clear();
		return ret;
	}
	utki::ScopeExit scopeExitContext([&cr](){
		cairo_destroy(cr);
	});
	
	Renderer r(cr);
	
	svg.render(r);
	
	return ret;
}
