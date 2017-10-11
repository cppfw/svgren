#include "FilterApplyer.hxx"

#include "util.hxx"

using namespace svgren;

void FilterApplyer::visit(const svgdom::FilterElement& e) {
	this->primitiveUnits = e.primitiveUnits;
	
	real frX;
	real frY;
	real frWidth;
	real frHeight;
	
	switch(e.filterUnits){
		default:
		case svgdom::CoordinateUnits_e::OBJECT_BOUNDING_BOX:
			{
				auto w = this->r.deviceSpaceBoundingBox.width();
				auto h = this->r.deviceSpaceBoundingBox.height();
				
				frX = this->r.deviceSpaceBoundingBox.left + percentLengthToFraction(e.x) * w;
				frY = this->r.deviceSpaceBoundingBox.top + percentLengthToFraction(e.y) * h;
				frWidth = percentLengthToFraction(e.width) * w;
				frHeight = percentLengthToFraction(e.height) * h;
			}
			break;
		case svgdom::CoordinateUnits_e::USER_SPACE_ON_USE:
			{
				double x1, y1, x2, y2;
				x1 = this->r.lengthToPx(e.x, 0);
				y1 = this->r.lengthToPx(e.y, 1);
				x2 = x1 + this->r.lengthToPx(e.width, 0);
				y2 = y1 + this->r.lengthToPx(e.height, 1);
				
				std::array<std::array<double, 2>, 4> rectVertices = {{
					{{x1 ,y1}},
					{{x2, y2}},
					{{x1, y2}},
					{{x2, y1}}
				}};

				DeviceSpaceBoundingBox frBb;
				frBb.setEmpty();
				
				for(auto& vertex : rectVertices){
					cairo_user_to_device(this->r.cr, &vertex[0], &vertex[1]);

					DeviceSpaceBoundingBox bb;
					bb.left = decltype(bb.left)(vertex[0]);
					bb.right = decltype(bb.right)(vertex[0]);
					bb.top = decltype(bb.top)(vertex[1]);
					bb.bottom = decltype(bb.bottom)(vertex[1]);

					frBb.merge(bb);
				}
				frX = frBb.left;
				frY = frBb.top;
				frWidth = frBb.width();
				frHeight = frBb.height();
			}
			break;
	}
	
	this->filterRegion.x = unsigned(std::max(frX, decltype(frX)(0)));
	this->filterRegion.y = unsigned(std::max(frY, decltype(frY)(0)));
	this->filterRegion.width = unsigned(std::max(frWidth, decltype(frWidth)(0)));
	this->filterRegion.height = unsigned(std::max(frHeight, decltype(frHeight)(0)));
	
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
				x = double(this->r.getDeviceSpaceBoundingBoxDim()[0] * sd[0]);
				y = double(this->r.getDeviceSpaceBoundingBoxDim()[1] * sd[1]);
				break;
		}
		cairo_user_to_device_distance(this->r.cr, &x, &y);
		sd[0] = real(x);
		sd[1] = real(y);
	}

	cairoImageSurfaceBlur(getSubSurface(this->r.cr, this->filterRegion), sd);
}
