#include "util.hxx"

#include <cstring>
#include <vector>
#include <stdexcept>

#include <utki/debug.hpp>
#include <utki/math.hpp>

#include <svgdom/length.hpp>

#include "renderer.hxx"

using namespace svgren;

canvas_context_push::canvas_context_push(canvas& c) :
		c(c)
{
	this->c.push_context();
}

canvas_context_push::~canvas_context_push()noexcept{
	this->c.pop_context();
}

canvas_matrix_push::canvas_matrix_push(canvas& c) :
		c(c)
{
	this->m = this->c.get_matrix();
}

canvas_matrix_push::~canvas_matrix_push()noexcept{
	this->c.set_matrix(this->m);
}

real svgren::percentLengthToFraction(const svgdom::length& l){
	if(l.is_percent()){
		return l.value / real(100);
	}
	if(l.unit == svgdom::length_unit::number){
		return l.value;
	}
	return 0;
}

void DeviceSpaceBoundingBox::set_empty(){
	this->left = std::numeric_limits<decltype(this->left)>::max();
	this->top = std::numeric_limits<decltype(this->top)>::max();
	this->right = std::numeric_limits<decltype(this->right)>::min();
	this->bottom = std::numeric_limits<decltype(this->bottom)>::min();
}

void DeviceSpaceBoundingBox::unite(const DeviceSpaceBoundingBox& bb){
	using std::min;
	using std::max;
	this->left = min(this->left, bb.left);
	this->top = min(this->top, bb.top);
	this->right = max(this->right, bb.right);
	this->bottom = max(this->bottom, bb.bottom);
}

real DeviceSpaceBoundingBox::width()const noexcept{
	using std::max;
	auto w = this->right - this->left;
	return max(w, decltype(w)(0));
}

real DeviceSpaceBoundingBox::height() const noexcept{
	using std::max;
	auto h = this->bottom - this->top;
	return max(h, decltype(h)(0));
}

DeviceSpaceBoundingBoxPush::DeviceSpaceBoundingBoxPush(renderer& r) :
		r(r),
		oldBb(r.device_space_bounding_box)
{
	this->r.device_space_bounding_box.set_empty();
}

DeviceSpaceBoundingBoxPush::~DeviceSpaceBoundingBoxPush() noexcept{
	this->oldBb.unite(this->r.device_space_bounding_box);
	this->r.device_space_bounding_box = this->oldBb;
}

ViewportPush::ViewportPush(renderer& r, const decltype(oldViewport)& viewport) :
		r(r),
		oldViewport(r.viewport)
{
	this->r.viewport = viewport;
}

ViewportPush::~ViewportPush() noexcept{
	this->r.viewport = this->oldViewport;
}

PushCairoGroupIfNeeded::PushCairoGroupIfNeeded(svgren::renderer& renderer, bool isContainer) :
		renderer(renderer)
{
	auto backgroundP = this->renderer.style_stack.get_style_property(svgdom::style_property::enable_background);
	
	if(backgroundP && backgroundP->enable_background.value == svgdom::enable_background::new_){
		this->oldBackground = this->renderer.background;
	}
	
	auto filterP = this->renderer.style_stack.get_style_property(svgdom::style_property::filter);
	
	if(auto maskP = this->renderer.style_stack.get_style_property(svgdom::style_property::mask)){
		if(auto ei = this->renderer.finder.find_by_id(maskP->get_local_id_from_iri())){
			this->maskElement = &ei->e;
		}
	}
	
	this->groupPushed = filterP || this->maskElement || this->oldBackground.data;

	auto opacity = svgdom::real(1);
	{
		auto strokeP = this->renderer.style_stack.get_style_property(svgdom::style_property::stroke);
		auto fillP = this->renderer.style_stack.get_style_property(svgdom::style_property::fill);

		// OPTIMIZATION: if opacity is set on an element then push cairo group only in case it is a Container element, like 'g' or 'svg',
		//               or in case the fill or stroke is a non-solid color, like gradient or pattern,
		//               or both fill and stroke are non-none.
		//               If element is non-container and one of stroke or fill is solid color and other one is none,
		//               then opacity will be applied later without pushing cairo group.
		if(this->groupPushed
				|| isContainer
				|| (strokeP && strokeP->is_url())
				|| (fillP && fillP->is_url())
				|| (fillP && strokeP && !fillP->is_none() && !strokeP->is_none())
			)
		{
			if(auto p = this->renderer.style_stack.get_style_property(svgdom::style_property::opacity)){
				opacity = p->opacity;
				this->groupPushed = this->groupPushed || opacity < 1;
			}
		}
	}
	
	if(this->groupPushed){
//		TRACE(<< "setting temp context" << std::endl)
		cairo_push_group(this->renderer.cr);
		if(cairo_status(this->renderer.cr) != CAIRO_STATUS_SUCCESS){
			throw std::runtime_error("cairo_push_group() failed");
		}
		
		this->opacity = opacity;
	}
	
	if(this->oldBackground.data){
		this->renderer.background = this->renderer.canvas.get_sub_surface();
	}
}

PushCairoGroupIfNeeded::~PushCairoGroupIfNeeded()noexcept{
	if(!this->groupPushed){
		return;
	}
	
	// render mask
	cairo_pattern_t* mask = nullptr;
	try{
		if(this->maskElement){
			cairo_push_group(this->renderer.cr);
			if(cairo_status(this->renderer.cr) != CAIRO_STATUS_SUCCESS){
				throw std::runtime_error("cairo_push_group() failed");
			}
			
			utki::scope_exit scope_exit([&mask, this](){
				mask = cairo_pop_group(this->renderer.cr);
			});
			
			// TODO: setup the correct coordinate system based on maskContentUnits value (userSpaceOnUse/objectBoundingBox)
			//       Currently nothing on that is done which is equivalent to userSpaceOnUse
			
			class MaskRenderer : public svgdom::const_visitor{
				svgren::renderer& r;
			public:
				MaskRenderer(svgren::renderer& r) : r(r){}
				
				void visit(const svgdom::mask_element& e)override{
					svgdom::style_stack::push pushStyles(this->r.style_stack, e);
	
					this->r.relay_accept(e);
				}
			} maskRenderer(this->renderer);
			
			this->maskElement->accept(maskRenderer);
			
			appendLuminanceToAlpha(this->renderer.canvas.get_sub_surface());
		}
	}catch(...){
		// rendering mask failed, just ignore it
	}
	
	utki::scope_exit scope_exit([mask](){
		if(mask){
			cairo_pattern_destroy(mask);
		}
	});
	
	cairo_pop_group_to_source(this->renderer.cr);
	
	if(mask){
		cairo_mask(this->renderer.cr, mask);
	}else{
		ASSERT(0 <= this->opacity && this->opacity <= 1)
		if(this->opacity < real(1)){
			cairo_paint_with_alpha(this->renderer.cr, this->opacity);
		}else{
			cairo_paint(this->renderer.cr);
		}
	}
	
	// restore background if it was pushed
	if(this->oldBackground.data){
		this->renderer.background = this->oldBackground;
	}
}

void svgren::appendLuminanceToAlpha(surface s){
	ASSERT((s.end - s.data) % 4 == 0)
	
	// Luminance is calculated using formula L = 0.2126 * R + 0.7152 * G + 0.0722 * B
	// For faster calculation it can be simplified to L = (2 * R + 3 * G + B) / 6
	
	// TODO: take stride into account, do not append luminance to alpha for data out of the surface width
	for(auto p = s.data; p != s.end; ++p){
		uint32_t l = 2 * uint32_t(*p);
		++p;
		l += 3 * uint32_t(*p);
		++p;
		l += uint32_t(*p);
		++p;
		
		l /= 6;
		
		// Cairo uses premultiplied alpha, so no need to multiply alpha by liminance.
		ASSERT(l <= 255)
		*p = uint8_t(l);
	}
}
