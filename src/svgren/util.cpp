#include "util.hxx"

#include <cstring>
#include <vector>

#include <utki/debug.hpp>
#include <utki/math.hpp>

#include <svgdom/Length.hpp>

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
}

void svgren::cairoQuadraticCurveTo(cairo_t *cr, double x1, double y1, double x, double y){
	double x0, y0; //current point, absolute coordinates
	if (cairo_has_current_point(cr)) {
		cairo_get_current_point(cr, &x0, &y0);
	}
	else {
		cairo_move_to(cr, 0, 0);
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
}

real svgren::degToRad(real deg){
	return deg * utki::pi<real>() / real(180);
}

std::array<real, 2> svgren::rotate(real x, real y, real angle){
    return {{x * std::cos(angle) - y * std::sin(angle), y * std::cos(angle) + x * std::sin(angle)}};
}

real svgren::pointAngle(real cx, real cy, real px, real py){
    return std::atan2(py - cy, px - cx);
}

CairoContextSaveRestore::CairoContextSaveRestore(cairo_t* cr) :
		cr(cr)
{
	ASSERT(this->cr)
	cairo_save(this->cr);
}

CairoMatrixSaveRestore::CairoMatrixSaveRestore(cairo_t* cr) :
		cr(cr)
{
	ASSERT(this->cr)
	cairo_get_matrix(this->cr, &this->m);
}

CairoContextSaveRestore::~CairoContextSaveRestore()noexcept{
	cairo_restore(this->cr);
}

CairoMatrixSaveRestore::~CairoMatrixSaveRestore()noexcept{
	cairo_set_matrix(this->cr, &this->m);
}


Surface svgren::getSubSurface(cairo_t* cr, const CanvasRegion& region){
//	TRACE(<< "region = (" << region[0] << ", " << region[1] << ") (" << region[2] << ", " << region[3] << ")" << std::endl)
	
	Surface ret;
	auto s = cairo_get_group_target(cr);
	ASSERT(s)
	
	ret.stride = cairo_image_surface_get_stride(s) / 4; //stride is returned in bytes
			
	auto sw = unsigned(cairo_image_surface_get_width(s));
	auto sh = unsigned(cairo_image_surface_get_height(s));

	ret.width = std::min(region.width, sw - region.x);
	ret.height = std::min(region.height, sh - region.y);
	ret.data = cairo_image_surface_get_data(s) + 4 * (region.y * ret.stride + region.x);
	ret.x = region.x;
	ret.y = region.y;

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


PushCairoGroupIfNeeded::PushCairoGroupIfNeeded(Renderer& renderer) :
		renderer(renderer)
{
	auto opacityP = this->renderer.styleStack.getStyleProperty(svgdom::StyleProperty_e::OPACITY);
	
	auto backgroundP = this->renderer.styleStack.getStyleProperty(svgdom::StyleProperty_e::ENABLE_BACKGROUND);
	
	if(backgroundP && backgroundP->enableBackground.value == svgdom::EnableBackground_e::NEW){
		this->oldBackground = this->renderer.background;
		this->renderer.background = getSubSurface(this->renderer.cr);
	}
	
	auto filterP = this->renderer.styleStack.getStyleProperty(svgdom::StyleProperty_e::FILTER);
	
	this->groupPushed = (opacityP && opacityP->opacity < svgdom::real(1)) || filterP || this->oldBackground.data;
	
	if(this->groupPushed){
//		TRACE(<< "setting temp context" << std::endl)
		cairo_push_group(this->renderer.cr);
		
		if(opacityP){
			this->opacity = opacityP->opacity;
		}
	}
}

PushCairoGroupIfNeeded::~PushCairoGroupIfNeeded()noexcept{
	if(!this->groupPushed){
		return;
	}
	
	cairo_pop_group_to_source(this->renderer.cr);
	cairo_paint_with_alpha(this->renderer.cr, this->opacity);
	
	//restore background if it was pushed
	if(this->oldBackground.data){
		this->renderer.background = this->oldBackground;
	}
}
