#include "FilterApplyer.hxx"

#include "util.hxx"

using namespace svgren;

void FilterApplyer::visit(const svgdom::FilterElement& e) {
	this->primitiveUnits = e.primitiveUnits;
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
	cairoImageSurfaceBlur(cairo_get_group_target(this->r.cr), sd);
}
