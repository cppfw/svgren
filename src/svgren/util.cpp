#include "util.hxx"

#include <cstring>
#include <vector>
#include <stdexcept>

#include <utki/debug.hpp>
#include <utki/math.hpp>

#include <svgdom/length.hpp>

#include "Renderer.hxx"

using namespace svgren;



void svgren::cairoRelQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y){
	cairo_rel_curve_to(cr,
			2.0 / 3.0 * x1,
			2.0 / 3.0 * y1,
			2.0 / 3.0 * x1 + 1.0 / 3.0 * x,
			2.0 / 3.0 * y1 + 1.0 / 3.0 * y,
			x,
			y
		);
	ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
}

void svgren::cairoQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y){
	double x0, y0; //current point, absolute coordinates
	if (cairo_has_current_point(cr)) {
		ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
		cairo_get_current_point(cr, &x0, &y0);
		ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
	}
	else {
		ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
		cairo_move_to(cr, 0, 0);
		ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
		x0 = 0;
		y0 = 0;
	}
	cairo_curve_to(cr,
			2.0 / 3.0 * x1 + 1.0 / 3.0 * x0,
			2.0 / 3.0 * y1 + 1.0 / 3.0 * y0,
			2.0 / 3.0 * x1 + 1.0 / 3.0 * x,
			2.0 / 3.0 * y1 + 1.0 / 3.0 * y,
			x,
			y
		);
	ASSERT(cairo_status(cr) == CAIRO_STATUS_SUCCESS)
}

CairoContextSaveRestore::CairoContextSaveRestore(cairo_t* cr) :
		cr(cr)
{
	ASSERT(this->cr)
	cairo_save(this->cr);
}

CairoContextSaveRestore::~CairoContextSaveRestore()noexcept{
	cairo_restore(this->cr);
}

CairoMatrixSaveRestore::CairoMatrixSaveRestore(cairo_t* cr) :
		cr(cr)
{
	ASSERT(this->cr)
	cairo_get_matrix(this->cr, &this->m);
}

CairoMatrixSaveRestore::~CairoMatrixSaveRestore()noexcept{
	cairo_set_matrix(this->cr, &this->m);
}


Surface svgren::getSubSurface(cairo_t* cr, const CanvasRegion& region){
//	TRACE(<< "region = (" << region[0] << ", " << region[1] << ") (" << region[2] << ", " << region[3] << ")" << std::endl)
	
	Surface ret;
	auto s = cairo_get_group_target(cr);
	ASSERT(s)
	
	ret.stride = cairo_image_surface_get_stride(s) / sizeof(std::uint32_t); //stride is returned in bytes
			
	auto sw = unsigned(cairo_image_surface_get_width(s));
	auto sh = unsigned(cairo_image_surface_get_height(s));

	ret.width = std::min(region.width, sw - region.x);
	ret.height = std::min(region.height, sh - region.y);
	ret.data = cairo_image_surface_get_data(s) + 4 * (region.y * ret.stride + region.x);
	ret.end = cairo_image_surface_get_data(s) + cairo_image_surface_get_stride(s) * cairo_image_surface_get_height(s);
	ret.x = region.x;
	ret.y = region.y;
	
	ASSERT(ret.height <= sh)
	ASSERT(&ret.data[ret.stride * (ret.height - 1) * sizeof(std::uint32_t)] < ret.end || ret.height == 0)

	return ret;
}


real svgren::percentLengthToFraction(const svgdom::Length& l){
	if(l.isPercent()){
		return l.value / real(100);
	}
	if(l.unit == svgdom::Length::Unit_e::NUMBER){
		return l.value;
	}
	return 0;
}

void DeviceSpaceBoundingBox::setEmpty() {
	this->left = std::numeric_limits<decltype(this->left)>::max();
	this->top = std::numeric_limits<decltype(this->top)>::max();
	this->right = std::numeric_limits<decltype(this->right)>::min();
	this->bottom = std::numeric_limits<decltype(this->bottom)>::min();
}

bool DeviceSpaceBoundingBox::isEmpty() const noexcept{
	return this->right - this->left < 0;
}

void DeviceSpaceBoundingBox::merge(const DeviceSpaceBoundingBox& bb) {
	this->left = std::min(this->left, bb.left);
	this->top = std::min(this->top, bb.top);
	this->right = std::max(this->right, bb.right);
	this->bottom = std::max(this->bottom, bb.bottom);
}

real DeviceSpaceBoundingBox::width() const noexcept{
	auto w = this->right - this->left;
	return std::max(w, decltype(w)(0));
}

real DeviceSpaceBoundingBox::height() const noexcept{
	auto h = this->bottom - this->top;
	return std::max(h, decltype(h)(0));
}

DeviceSpaceBoundingBoxPush::DeviceSpaceBoundingBoxPush(Renderer& r) :
		r(r),
		oldBb(r.deviceSpaceBoundingBox)
{
	this->r.deviceSpaceBoundingBox.setEmpty();
}

DeviceSpaceBoundingBoxPush::~DeviceSpaceBoundingBoxPush() noexcept{
	this->oldBb.merge(this->r.deviceSpaceBoundingBox);
	this->r.deviceSpaceBoundingBox = this->oldBb;
}

ViewportPush::ViewportPush(Renderer& r, const decltype(oldViewport)& viewport) :
		r(r),
		oldViewport(r.viewport)
{
	this->r.viewport = viewport;
}

ViewportPush::~ViewportPush() noexcept{
	this->r.viewport = this->oldViewport;
}


PushCairoGroupIfNeeded::PushCairoGroupIfNeeded(Renderer& renderer, bool isContainer) :
		renderer(renderer)
{
	auto backgroundP = this->renderer.styleStack.get_style_property(svgdom::StyleProperty_e::enable_background);
	
	if(backgroundP && backgroundP->enableBackground.value == svgdom::EnableBackground_e::NEW){
		this->oldBackground = this->renderer.background;
	}
	
	auto filterP = this->renderer.styleStack.get_style_property(svgdom::StyleProperty_e::filter);
	
	if(auto maskP = this->renderer.styleStack.get_style_property(svgdom::StyleProperty_e::mask)){
		if(auto ei = this->renderer.finder.findById(maskP->getLocalIdFromIri())){
			this->maskElement = &ei->e;
		}
	}
	
	this->groupPushed = filterP || this->maskElement || this->oldBackground.data;

	auto opacity = svgdom::real(1);
	{
		auto strokeP = this->renderer.styleStack.get_style_property(svgdom::StyleProperty_e::stroke);
		auto fillP = this->renderer.styleStack.get_style_property(svgdom::StyleProperty_e::fill);

		// OPTIMIZATION: if opacity is set on an element then push cairo group only in case it is a Container element, like 'g' or 'svg',
		//               or in case the fill or stroke is a non-solid color, like gradient or pattern,
		// 				or both fill and stroke are non-none.
		//               If element is non-container and one of stroke or fill is solid color and other one is none,
		//               then opacity will be applied later without pushing cairo group.
		if(this->groupPushed
				|| isContainer
				|| (strokeP && strokeP->isUrl())
				|| (fillP && fillP->isUrl())
				|| (fillP && strokeP && !fillP->isNone() && !strokeP->isNone())
			)
		{
			if(auto p = this->renderer.styleStack.get_style_property(svgdom::StyleProperty_e::opacity)){
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
		this->renderer.background = getSubSurface(this->renderer.cr);
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
			
			class MaskRenderer : public svgdom::ConstVisitor{
				Renderer& r;
			public:
				MaskRenderer(Renderer& r) : r(r){}
				
				void visit(const svgdom::MaskElement& e)override{
					svgdom::style_stack::push pushStyles(this->r.styleStack, e);
	
					this->r.relayAccept(e);
				}
			} maskRenderer(this->renderer);
			
			this->maskElement->accept(maskRenderer);
			
			appendLuminanceToAlpha(getSubSurface(this->renderer.cr));
		}
	}catch(...){
		//rendering mask failed, just ignore it
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
		if(this->opacity < svgdom::real(1)){
			cairo_paint_with_alpha(this->renderer.cr, this->opacity);
		}else{
			cairo_paint(this->renderer.cr);
		}
	}
	
	//restore background if it was pushed
	if(this->oldBackground.data){
		this->renderer.background = this->oldBackground;
	}
}



void svgren::appendLuminanceToAlpha(Surface s){
	ASSERT((s.end - s.data) % 4 == 0)
	
	//Luminance is calculated using formula L = 0.2126 * R + 0.7152 * G + 0.0722 * B
	//For faster calculation it can be simplified to L = (2 * R + 3 * G + B) / 6
		
	for(auto p = s.data; p != s.end; ++p){
		std::uint32_t l = 2 * std::uint32_t(*p);
		++p;
		l += 3 * std::uint32_t(*p);
		++p;
		l += std::uint32_t(*p);
		++p;
		
		l /= 6;
		
		//Cairo uses premultiplied alpha, so no need to multiply alpha by liminance.
		ASSERT(l <= 255)
		*p = std::uint8_t(l);
	}
}
