#include "FilterApplyer.hxx"

#include "util.hxx"

using namespace svgren;

void FilterApplyer::visit(const svgdom::FilterElement& e) {
	this->primitiveUnits = e.primitiveUnits;
	
	switch(e.filterUnits){
		default:
		case svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX:
			{
				//TODO:
			}
			break;
		case svgdom::CoordinateUnits_e::USER_SPACE_ON_USE:
			{
				double x = this->r.lengthToPx(e.x, 0);
				double y = this->r.lengthToPx(e.y, 1);
				double width = this->r.lengthToPx(e.width, 0);
				double height = this->r.lengthToPx(e.height, 1);
				
				cairo_user_to_device(this->r.cr, &x, &y);
				cairo_user_to_device_distance(this->r.cr, &width, &height);
				this->filterRegion[0] = unsigned(x);
				this->filterRegion[1] = unsigned(y);
				this->filterRegion[2] = unsigned(width);
				this->filterRegion[3] = unsigned(height);
			}
			break;
	}
	
	this->relayAccept(e);
}

void FilterApplyer::visit(const svgdom::FeGaussianBlurElement& e) {
	if (!e.isStdDeviationSpecified()) {
		return;
	}
	auto sd = e.getStdDeviation();
	if (this->primitiveUnits == svgdom::CoordinateUnits_e::USER_SPACE_ON_USE) {
		double x = double(sd[0]);
		double y = double(sd[1]);
		cairo_user_to_device_distance(this->r.cr, &x, &y);
		sd[0] = real(x);
		sd[1] = real(y);
	} else if (this->primitiveUnits == svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX) {
		sd[0] = this->r.curBoundingBoxDim[0] * sd[0];
		sd[1] = this->r.curBoundingBoxDim[1] * sd[1];
	} else {
		return;
	}
	cairoImageSurfaceBlur(getSubSurface(this->r.cr), sd);
}
