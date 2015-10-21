/* 
 * File:   render.cpp
 * Author: ivan
 * 
 * Created on October 12, 2015, 11:04 PM
 */

#include "render.hpp"

#include <cmath>

#include <cairo/cairo.h>

#include <utki/util.hpp>

#include "config.hpp"

using namespace svgren;



namespace{

class CairoMatrixPush{
	cairo_matrix_t m;
	cairo_t* cr;
public:
	CairoMatrixPush(cairo_t* cr) :
			cr(cr)
	{
		ASSERT(this->cr)
		cairo_get_matrix(this->cr, &this->m);
	}
	~CairoMatrixPush()noexcept{
		cairo_set_matrix(this->cr, &this->m);
	}
};


class Renderer : public svgdom::Renderer{
	cairo_t* curCr;
	
	cairo_t* cr;
	
	std::vector<std::array<real, 2>> viewportStack;//stack of width, height
	
	std::array<real, 2> curBoundingBoxPos = {{0, 0}};
	std::array<real, 2> curBoundingBoxDim = {{0, 0}};
	
	real lengthToNum(const svgdom::Length& l, unsigned coordIndex = 0)const noexcept{
		if(l.isPercent()){
			if(this->viewportStack.size() == 0){
				return 0;
			}
			ASSERT(coordIndex < this->viewportStack.back().size())
			return this->viewportStack.back()[coordIndex] * (l.value / 100);
		}
		return l.value;
	}
	
	void applyTransformations(const decltype(svgdom::Transformable::transformations)& transformations)const{
		for(auto& t : transformations){
			switch(t.type){
				case svgdom::Transformable::Transformation::EType::TRANSLATE:
					cairo_translate(this->curCr, t.x, t.y);
					break;
				case svgdom::Transformable::Transformation::EType::MATRIX:
					{
						cairo_matrix_t matrix;
						matrix.xx = t.a;
						matrix.yx = t.b;
						matrix.xy = t.c;
						matrix.yy = t.d;
						matrix.x0 = t.e;
						matrix.y0 = t.f;
						cairo_transform(this->curCr, &matrix);
					}
					break;
				case svgdom::Transformable::Transformation::EType::SCALE:
					cairo_scale(this->curCr, t.x, t.y);
					break;
				case svgdom::Transformable::Transformation::EType::ROTATE:
					cairo_translate(this->curCr, t.x, t.y);
					cairo_rotate(this->curCr, t.angle);
					cairo_translate(this->curCr, -t.x, -t.y);
					break;
				case svgdom::Transformable::Transformation::EType::SKEWX:
					{
						cairo_matrix_t matrix;
						matrix.xx = 1;
						matrix.yx = 0;
						matrix.xy = std::tan(t.angle);
						matrix.yy = 1;
						matrix.x0 = 0;
						matrix.y0 = 0;
						cairo_transform(this->curCr, &matrix);
					}
					break;
				case svgdom::Transformable::Transformation::EType::SKEWY:
					{
						cairo_matrix_t matrix;
						matrix.xx = 1;
						matrix.yx = std::tan(t.angle);
						matrix.xy = 0;
						matrix.yy = 1;
						matrix.x0 = 0;
						matrix.y0 = 0;
						cairo_transform(this->curCr, &matrix);
					}
					break;
				default:
					ASSERT(false)
					break;
			}
		}
	}
	
	void setCairoPatternSource(cairo_pattern_t* pat, const svgdom::Gradient& g){
		if(!pat){
			return;
		}
		
		for(auto& stop : g.getStops()){
			if(auto s = dynamic_cast<svgdom::Gradient::StopElement*>(stop.get())){
				svgdom::Rgb rgb;
				if(auto p = s->getStyleProperty(svgdom::EStyleProperty::STOP_COLOR)){
					rgb = p->getRgb();
				}else{
					rgb.r = 0;
					rgb.g = 0;
					rgb.b = 0;
				}

				svgdom::real opacity;
				if(auto p = s->getStyleProperty(svgdom::EStyleProperty::STOP_OPACITY)){
					opacity = p->opacity;
				}else{
					opacity = 1;
				}
				cairo_pattern_add_color_stop_rgba(pat, s->offset, rgb.r, rgb.g, rgb.b, opacity);
			}
		}
		switch(g.getSpreadMethod()){
			default:
			case svgdom::Gradient::ESpreadMethod::DEFAULT:
				ASSERT(false)
				break;
			case svgdom::Gradient::ESpreadMethod::PAD:
				cairo_pattern_set_extend(pat, CAIRO_EXTEND_PAD);
				break;
			case svgdom::Gradient::ESpreadMethod::REFLECT:
				cairo_pattern_set_extend(pat, CAIRO_EXTEND_REFLECT);
				break;
			case svgdom::Gradient::ESpreadMethod::REPEAT:
				cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
				break;
		}
		cairo_set_source (this->curCr, pat);
	}
	
	void setGradient(const svgdom::Element* gradientElement){
		if(auto gradient = dynamic_cast<const svgdom::Gradient*>(gradientElement)){
			CairoMatrixPush cairoMatrixPush(this->curCr);
			
			if(gradient->isBoundingBoxUnits()){
				cairo_translate(this->curCr, this->curBoundingBoxPos[0], this->curBoundingBoxPos[1]);
				cairo_scale(this->curCr, this->curBoundingBoxDim[0], this->curBoundingBoxDim[1]);
				this->viewportStack.push_back({1, 1});
			}
			utki::ScopeExit gradScopeExit([this, gradient](){
				if(gradient->isBoundingBoxUnits()){
					this->viewportStack.pop_back();
				}
			});

			this->applyTransformations(gradient->transformations);
			
			cairo_pattern_t* pat = nullptr;
			utki::ScopeExit patScopeExit([&pat](){
				cairo_pattern_destroy(pat);
			});
			
			if(auto g = dynamic_cast<const svgdom::LinearGradientElement*>(gradient)){
				pat = cairo_pattern_create_linear(
						this->lengthToNum(g->getX1(), 0),
						this->lengthToNum(g->getY1(), 1),
						this->lengthToNum(g->getX2(), 0),
						this->lengthToNum(g->getY2(), 1)
					);
				this->setCairoPatternSource(pat, *g);
				return;
			}else if(auto g = dynamic_cast<const svgdom::RadialGradientElement*>(gradient)){
				auto cx = g->getCx();
				auto cy = g->getCy();
				auto r = g->getR();
				auto fx = g->getFx();
				auto fy = g->getFy();
				
				if(!fx.isValid()){
					fx = cx;
				}
				if(!fy.isValid()){
					fy = cy;
				}
				
				pat = cairo_pattern_create_radial(
						this->lengthToNum(fx, 0),
						this->lengthToNum(fy, 1),
						0,
						this->lengthToNum(cx, 0),
						this->lengthToNum(cy, 1),
						this->lengthToNum(r)
					);
				this->setCairoPatternSource(pat, *g);
				return;
			}
		}
		cairo_set_source_rgba(this->curCr, 0, 0, 0, 0);
	}
	
	void updateCurBoundingBox(){
		double x1, y1, x2, y2;
		
		cairo_path_extents(this->curCr,
				&x1,
				&y1,
				&x2,
				&y2
			);
		
		this->curBoundingBoxPos[0] = x1;
		this->curBoundingBoxPos[1] = y1;
		this->curBoundingBoxDim[0] = x2 - x1;
		this->curBoundingBoxDim[1] = y2 - y1;
	}
	
	void renderCurrentShape(const svgdom::Shape& e){
		auto fill = e.getStyleProperty(svgdom::EStyleProperty::FILL);
		auto stroke = e.getStyleProperty(svgdom::EStyleProperty::STROKE);
		
		this->updateCurBoundingBox();
		
		if(fill && !fill->isNone()){
			if(fill->isUrl()){
				this->setGradient(fill->url);
			}else{
				svgdom::real opacity;
				if(auto p = e.getStyleProperty(svgdom::EStyleProperty::FILL_OPACITY)){
					opacity = p->opacity;
				}else{
					opacity = 1;
				}
				
				auto fillRgb = fill->getRgb();
				cairo_set_source_rgba(this->curCr, fillRgb.r, fillRgb.g, fillRgb.b, opacity);
			}

			if(stroke && !stroke->isNone()){
				cairo_fill_preserve(this->curCr);
			}else{
				cairo_fill(this->curCr);
			}
		}
		
		if(stroke && !stroke->isNone()){
			if(auto p = e.getStyleProperty(svgdom::EStyleProperty::STROKE_WIDTH)){
				cairo_set_line_width(this->curCr, this->lengthToNum(p->length));
			}else{
				cairo_set_line_width(this->curCr, 1);
			}
			
			if(stroke->isUrl()){
				this->setGradient(stroke->url);
			}else{
				svgdom::real opacity;
				if(auto p = e.getStyleProperty(svgdom::EStyleProperty::STROKE_OPACITY)){
					opacity = p->opacity;
				}else{
					opacity = 1;
				}
				
				auto rgb = stroke->getRgb();
				cairo_set_source_rgba(this->curCr, rgb.r, rgb.g, rgb.b, opacity);
			}
			
			cairo_stroke(this->curCr);
		}
	}
	
public:
	Renderer(cairo_t* cr) :
			cr(cr)
	{
		this->curCr = this->cr;
	}
	
	void render(const svgdom::GElement& e)override{
		CairoMatrixPush cairoMatrixPush(this->curCr);
		
		this->applyTransformations(e.transformations);
		
		e.Container::render(*this);
	}
	
	void render(const svgdom::SvgElement& e)override{
		//if viewport stack is empty then it is an outermost 'svg' element, x and y should be 0.
		this->viewportStack.push_back(
				{
					this->lengthToNum(e.width),
					this->lengthToNum(e.height)
				}
			);
		utki::ScopeExit scopeExit([this](){
			this->viewportStack.pop_back();
		});
		
		e.Container::render(*this);
	}
	
	void render(const svgdom::PathElement& e)override{
		CairoMatrixPush cairoMatrixPush(this->curCr);
		
		this->applyTransformations(e.transformations);
		
//		const svgdom::PathElement::Step* prev = nullptr;
		for(auto& s : e.path){
			switch(s.type){
				case svgdom::PathElement::Step::EType::MOVE_ABS:
					cairo_move_to(this->curCr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::MOVE_REL:
					cairo_rel_move_to(this->curCr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::LINE_ABS:
					cairo_line_to(this->curCr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::LINE_REL:
					cairo_rel_line_to(this->curCr, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::HORIZONTAL_LINE_ABS:
					{
						double x, y;
						if(cairo_has_current_point(this->curCr)){
							cairo_get_current_point(this->curCr, &x, &y);
						}else{
							y = 0;
						}
						cairo_line_to(this->curCr, s.x, y);
					}
					break;
				case svgdom::PathElement::Step::EType::HORIZONTAL_LINE_REL:
					{
						double x, y;
						if(cairo_has_current_point(this->curCr)){
							cairo_get_current_point(this->curCr, &x, &y);
						}else{
							y = 0;
						}
						cairo_rel_line_to(this->curCr, s.x, y);
					}
					break;
				case svgdom::PathElement::Step::EType::VERTICAL_LINE_ABS:
					{
						double x, y;
						if(cairo_has_current_point(this->curCr)){
							cairo_get_current_point(this->curCr, &x, &y);
						}else{
							x = 0;
						}
						cairo_line_to(this->curCr, x, s.y);
					}
					break;
				case svgdom::PathElement::Step::EType::VERTICAL_LINE_REL:
					{
						double x, y;
						if(cairo_has_current_point(this->curCr)){
							cairo_get_current_point(this->curCr, &x, &y);
						}else{
							x = 0;
						}
						cairo_rel_line_to(this->curCr, x, s.y);
					}
					break;
				case svgdom::PathElement::Step::EType::CLOSE:
					cairo_close_path(this->curCr);
					break;
				case svgdom::PathElement::Step::EType::CUBIC_ABS:
					cairo_curve_to(this->curCr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
					break;
				case svgdom::PathElement::Step::EType::CUBIC_REL:
					cairo_rel_curve_to(this->curCr, s.x1, s.y1, s.x2, s.y2, s.x, s.y);
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
		
		this->renderCurrentShape(e);
	}
	
	void render(const svgdom::EllipseElement& e) override{
		CairoMatrixPush cairoMatrixPush(this->curCr);
		
		this->applyTransformations(e.transformations);
		
		cairo_save(this->curCr);
		cairo_translate (this->curCr, this->lengthToNum(e.cx, 0), this->lengthToNum(e.cy, 1));
		cairo_scale (this->curCr, this->lengthToNum(e.rx, 0), this->lengthToNum(e.ry, 1));
		cairo_arc(this->curCr, 0, 0, 1, 0, 2 * M_PI);
		cairo_close_path(this->curCr);
		cairo_restore (this->curCr);
		
		this->renderCurrentShape(e);
	}
	
	void render(const svgdom::RectElement& e) override{
		CairoMatrixPush cairoMatrixPush(this->curCr);
		
		this->applyTransformations(e.transformations);
		
		if((e.rx.value == 0 || !e.rx.isValid())
				&& (e.ry.value == 0 || !e.ry.isValid()))
		{
			cairo_rectangle(this->curCr, this->lengthToNum(e.x, 0), this->lengthToNum(e.y, 1), this->lengthToNum(e.width, 0), this->lengthToNum(e.height, 1));
		}else{
			//compute real rx and ry
			auto rx = e.rx;
			auto ry = e.ry;
			
			if(!ry.isValid() && rx.isValid()){
				ry = rx;
			}else if(!rx.isValid() && ry.isValid()){
				rx = ry;
			}
			ASSERT(rx.isValid() && ry.isValid())
			
			if(this->lengthToNum(rx, 0) > this->lengthToNum(e.width, 0) / 2){
				rx = e.width;
				rx.value /= 2;
			}
			if(this->lengthToNum(ry, 1) > this->lengthToNum(e.height, 1) / 2){
				ry = e.height;
				ry.value /= 2;
			}
			
			cairo_move_to(this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1));
			cairo_line_to(this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(e.width, 0) - this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1));
			
			cairo_save (this->curCr);
			cairo_translate (this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(e.width, 0) - this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(ry, 1));
			cairo_scale (this->curCr, this->lengthToNum(rx, 0), this->lengthToNum(ry, 1));
			cairo_arc (this->curCr, 0, 0, 1, -M_PI / 2, 0);
			cairo_restore (this->curCr);
			
			cairo_line_to(this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(e.width, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(e.height, 1) - this->lengthToNum(ry, 1));
			
			cairo_save (this->curCr);
			cairo_translate (this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(e.width, 0) - this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(e.height, 1) - this->lengthToNum(ry, 1));
			cairo_scale (this->curCr, this->lengthToNum(rx, 0), this->lengthToNum(ry, 1));
			cairo_arc (this->curCr, 0, 0, 1, 0, M_PI / 2);
			cairo_restore (this->curCr);
			
			cairo_line_to(this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(e.height, 1));
			
			cairo_save (this->curCr);
			cairo_translate (this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(e.height, 1) - this->lengthToNum(ry, 1));
			cairo_scale (this->curCr, this->lengthToNum(rx, 0), this->lengthToNum(ry, 1));
			cairo_arc (this->curCr, 0, 0, 1, M_PI / 2, M_PI);
			cairo_restore (this->curCr);
			
			cairo_line_to(this->curCr, this->lengthToNum(e.x, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(ry, 1));
			
			cairo_save (this->curCr);
			cairo_translate (this->curCr, this->lengthToNum(e.x, 0) + this->lengthToNum(rx, 0), this->lengthToNum(e.y, 1) + this->lengthToNum(ry, 1));
			cairo_scale (this->curCr, this->lengthToNum(rx, 0), this->lengthToNum(ry, 1));
			cairo_arc (this->curCr, 0, 0, 1, M_PI, M_PI * 3 / 2);
			cairo_restore (this->curCr);
			
			cairo_close_path(this->curCr);
		}
		
		this->renderCurrentShape(e);
	}
};

}//~namespace



std::vector<std::uint32_t> svgren::render(const svgdom::SvgElement& svg, unsigned width, unsigned height){
//	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	int stride = width * 4;
	
	TRACE(<< "width = " << width << " stride = " << stride / 4 << std::endl)
	
	std::vector<std::uint32_t> ret((stride / sizeof(std::uint32_t)) * height);
	
	for(auto& c : ret){
		c = 0xffffffff;//TODO: fill 0
	}
	
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
