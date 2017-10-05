#include "FilterApplyer.hxx"

#include "util.hxx"

using namespace svgren;

void FilterApplyer::visit(const svgdom::FilterElement& e) {
	this->primitiveUnits = e.primitiveUnits;
	
	double x, y, width, height;
	
	switch(e.filterUnits){
		default:
		case svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX:
			{
				auto& bb = this->r.getBoundingBoxDim();
				
				x = percentLengthToFraction(e.x) * bb[0];
				y = percentLengthToFraction(e.y) * bb[1];
				width = percentLengthToFraction(e.width) * bb[0];
				height = percentLengthToFraction(e.height) * bb[1];
			}
			break;
		case svgdom::CoordinateUnits_e::USER_SPACE_ON_USE:
			x = this->r.lengthToPx(e.x, 0);
			y = this->r.lengthToPx(e.y, 1);
			width = this->r.lengthToPx(e.width, 0);
			height = this->r.lengthToPx(e.height, 1);
			break;
	}
		
	cairo_user_to_device(this->r.cr, &x, &y);
	cairo_user_to_device_distance(this->r.cr, &width, &height);

	this->filterRegion[0] = unsigned(std::max(x, decltype(x)(0)));
	this->filterRegion[1] = unsigned(std::max(y, decltype(y)(0)));
	this->filterRegion[2] = unsigned(std::max(width, decltype(width)(0)));
	this->filterRegion[3] = unsigned(std::max(height, decltype(height)(0)));
	
	this->relayAccept(e);
}

void FilterApplyer::visit(const svgdom::FeGaussianBlurElement& e) {
	if (!e.isStdDeviationSpecified()) {
		return;
	}
	auto sd = e.getStdDeviation();
	
	{
		double x, y;

		switch(this->primitiveUnits){
			default:
			case svgdom::CoordinateUnits_e::USER_SPACE_ON_USE:
				x = double(sd[0]);
				y = double(sd[1]);
				break;
			case svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX:
				x = double(this->r.getBoundingBoxDim()[0] * sd[0]);
				y = double(this->r.getBoundingBoxDim()[1] * sd[1]);
				break;
		}
		cairo_user_to_device_distance(this->r.cr, &x, &y);
		sd[0] = real(x);
		sd[1] = real(y);
	}

	cairoImageSurfaceBlur(getSubSurface(this->r.cr, this->filterRegion), sd);
}
