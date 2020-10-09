#include "util.hxx"

#include <cstring>
#include <vector>
#include <stdexcept>

#include <utki/debug.hpp>
#include <utki/math.hpp>

#include <svgdom/length.hpp>

#include "renderer.hxx"

using namespace svgren;

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

DeviceSpaceBoundingBoxPush::~DeviceSpaceBoundingBoxPush()noexcept{
	this->oldBb.unite(this->r.device_space_bounding_box);
	this->r.device_space_bounding_box = this->oldBb;
}

renderer_viewport_push::renderer_viewport_push(renderer& r, const decltype(old_viewport)& viewport) :
		r(r),
		old_viewport(r.viewport)
{
	this->r.viewport = viewport;
}

renderer_viewport_push::~renderer_viewport_push()noexcept{
	this->r.viewport = this->old_viewport;
}

canvas_group_push::canvas_group_push(svgren::renderer& renderer, bool is_container) :
		renderer(renderer)
{
	auto background_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::enable_background);
	
	if(background_prop && background_prop->enable_background.value == svgdom::enable_background::new_){
		this->old_background = this->renderer.background;
	}
	
	auto filter_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::filter);
	
	if(auto mask_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::mask)){
		if(auto ei = this->renderer.finder.find_by_id(mask_prop->get_local_id_from_iri())){
			this->mask_element = &ei->e;
		}
	}
	
	this->group_pushed = filter_prop || this->mask_element || !this->old_background.span.empty();

	auto opacity = svgdom::real(1);
	{
		auto stroke_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::stroke);
		auto fill_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::fill);

		// OPTIMIZATION: if opacity is set on an element then push cairo group only in case it is a Container element, like 'g' or 'svg',
		//               or in case the fill or stroke is a non-solid color, like gradient or pattern,
		//               or both fill and stroke are non-none.
		//               If element is non-container and one of stroke or fill is solid color and other one is none,
		//               then opacity will be applied later without pushing cairo group.
		if(this->group_pushed
				|| is_container
				|| (stroke_prop && stroke_prop->is_url())
				|| (fill_prop && fill_prop->is_url())
				|| (fill_prop && stroke_prop && !fill_prop->is_none() && !stroke_prop->is_none())
			)
		{
			if(auto p = this->renderer.style_stack.get_style_property(svgdom::style_property::opacity)){
				opacity = p->opacity;
				this->group_pushed = this->group_pushed || opacity < 1;
			}
		}
	}
	
	if(this->group_pushed){
//		TRACE(<< "setting temp context" << std::endl)
		this->renderer.canvas.push_group();
		
		this->opacity = opacity;
	}
	
	if(!this->old_background.span.empty()){
		this->renderer.background = this->renderer.canvas.get_sub_surface();
	}
}

canvas_group_push::~canvas_group_push()noexcept{
	if(!this->group_pushed){
		return;
	}
	
	if(this->mask_element){
		// render mask
		try{
			this->renderer.canvas.push_group();
			
			utki::scope_exit scope_exit([this](){
				this->renderer.canvas.pop_group(1);
			});
			
			// TODO: setup the correct coordinate system based on maskContentUnits value (userSpaceOnUse/objectBoundingBox)
			//       Currently nothing on that is done which is equivalent to userSpaceOnUse
			
			class mask_renderer : public svgdom::const_visitor{
				svgren::renderer& r;
			public:
				mask_renderer(svgren::renderer& r) : r(r){}
				
				void visit(const svgdom::mask_element& e)override{
					svgdom::style_stack::push pushStyles(this->r.style_stack, e);
	
					this->r.relay_accept(e);
				}
			} mr(this->renderer);
			
			this->mask_element->accept(mr);
			
			scope_exit.reset();

			this->renderer.canvas.pop_mask_and_group();
		}catch(...){
			// rendering mask failed, just ignore it
		}
	}else{
		this->renderer.canvas.pop_group(this->opacity);
	}
	
	// restore background if it was pushed
	if(!this->old_background.span.empty()){
		this->renderer.background = this->old_background;
	}
}
