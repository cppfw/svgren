#include "FilterApplyer.hxx"

#include <cmath>

#include <utki/debug.hpp>
#include <utki/math.hpp>
#include <utki/Exc.hpp>

#include "util.hxx"

using namespace svgren;


namespace{
void boxBlurHorizontal(
		std::uint8_t* dst,
		const std::uint8_t* src,
		unsigned dstStride,
		unsigned srcStride,
		unsigned width,
		unsigned height,
		unsigned boxSize,
		unsigned boxOffset,
		unsigned channel
	)
{
	if(boxSize == 0){
		return;
	}
	for(unsigned y = 0; y != height; ++y){
		unsigned sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = std::max(pos, 0);
			pos = std::min(pos, int(width - 1));
			sum += src[(srcStride * y + pos) * sizeof(std::uint32_t) + channel];
		}
		for(unsigned x = 0; x != width; ++x){
			int tmp = x - boxOffset;
			int last = std::max(tmp, 0);
			int next = std::min(tmp + boxSize, width - 1);

			dst[(dstStride * y + x) * sizeof(std::uint32_t) + channel] = sum / boxSize;

			sum += src[(srcStride * y + next) * sizeof(std::uint32_t) + channel]
					- src[(srcStride * y + last) * sizeof(std::uint32_t) + channel];
		}
	}
}
}

namespace{
void boxBlurVertical(
		std::uint8_t* dst,
		const std::uint8_t* src,
		unsigned dstStride,
		unsigned srcStride,
		unsigned width,
		unsigned height,
		unsigned boxSize,
		unsigned boxOffset,
		unsigned channel
	)
{
	if(boxSize == 0){
		return;
	}
	for(unsigned x = 0; x != width; ++x){
		unsigned sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = std::max(pos, 0);
			pos = std::min(pos, int(height - 1));
			sum += src[(srcStride * pos + x) * sizeof(std::uint32_t) + channel];
		}
		for(unsigned y = 0; y != height; ++y){
			int tmp = y - boxOffset;
			int last = std::max(tmp, 0);
			int next = std::min(tmp + boxSize, height - 1);

			dst[(dstStride * y + x) * sizeof(std::uint32_t) + channel] = sum / boxSize;

			sum += src[(x + srcStride * next) * sizeof(std::uint32_t) + channel]
					- src[(x + srcStride * last) * sizeof(std::uint32_t) + channel];
		}
	}
}
}


namespace{
FilterResult cairoImageSurfaceBlur(const Surface& src, std::array<real, 2> stdDeviation){
	//NOTE: see https://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for Gaussian Blur approximation algorithm.
	
	ASSERT(src.width <= src.stride)
	
	std::array<unsigned, 2> d;
	for(unsigned i = 0; i != 2; ++i){
		d[i] = unsigned(float(stdDeviation[i]) * 3 * std::sqrt(2 * utki::pi<float>()) / 4 + 0.5f);
	}
	
//	TRACE(<< "d = " << d[0] << ", " << d[1] << std::endl)
	
	FilterResult ret;
	ret.data.resize(src.width * src.height * sizeof(std::uint32_t));
	ret.surface = src;
	ret.surface.data = &*ret.data.begin();
	ret.surface.stride = ret.surface.width;
	
	std::vector<std::uint8_t> tmp(ret.data.size());
	
	std::array<unsigned, 3> hBoxSize;
	std::array<unsigned, 3> hOffset;
	std::array<unsigned, 3> vBoxSize;
	std::array<unsigned, 3> vOffset;
	if(d[0] % 2 == 0){
		hOffset[0] = d[0] / 2;
		hBoxSize[0] = d[0];
		hOffset[1] = d[0] / 2 - 1; //it is ok if d[0] is 0 and -1 will give a large number because box size is also 0 in that case and blur will have no effect anyway
		hBoxSize[1] = d[0];
		hOffset[2] = d[0] / 2;
		hBoxSize[2] = d[0] + 1;
	}else{
		hOffset[0] = d[0] / 2;
		hBoxSize[0] = d[0];
		hOffset[1] = d[0] / 2;
		hBoxSize[1] = d[0];
		hOffset[2] = d[0] / 2;
		hBoxSize[2] = d[0];
	}
	
	if(d[1] % 2 == 0){
		vOffset[0] = d[1] / 2;
		vBoxSize[0] = d[1];
		vOffset[1] = d[1] / 2 - 1; //it is ok if d[0] is 0 and -1 will give a large number because box size is also 0 in that case and blur will have no effect anyway
		vBoxSize[1] = d[1];
		vOffset[2] = d[1] / 2;
		vBoxSize[2] = d[1] + 1;
	}else{
		vOffset[0] = d[1] / 2;
		vBoxSize[0] = d[1];
		vOffset[1] = d[1] / 2;
		vBoxSize[1] = d[1];
		vOffset[2] = d[1] / 2;
		vBoxSize[2] = d[1];
	}
	
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(&*tmp.begin(), src.data, src.width, src.stride, src.width, src.height, hBoxSize[0], hOffset[0], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(ret.surface.data, &*tmp.begin(), ret.surface.stride, src.width, src.width, src.height, hBoxSize[1], hOffset[1], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(&*tmp.begin(), ret.surface.data, src.width, ret.surface.stride, src.width, src.height, hBoxSize[2], hOffset[2], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(ret.surface.data, &*tmp.begin(), ret.surface.stride, src.width, src.width, src.height, vBoxSize[0], vOffset[0], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(&*tmp.begin(), ret.surface.data, src.width, ret.surface.stride, src.width, src.height, vBoxSize[1], vOffset[1], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(ret.surface.data, &*tmp.begin(), ret.surface.stride, src.width, src.width, src.height, vBoxSize[2], vOffset[2], channel);
	}
	
	return ret;
}
}


void FilterApplyer::visit(const svgdom::FilterElement& e) {
	this->primitiveUnits = e.primitiveUnits;
	
	//set filter region
	{
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
	}
	
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
				x = double(this->r.userSpaceShapeBoundingBoxDim[0] * sd[0]);
				y = double(this->r.userSpaceShapeBoundingBoxDim[1] * sd[1]);
				break;
		}
		cairo_user_to_device_distance(this->r.cr, &x, &y);
		sd[0] = real(x);
		sd[1] = real(y);
	}

	this->setResult(e.result, cairoImageSurfaceBlur(this->getSource(e.in), sd));
}

Surface FilterApplyer::getSourceGraphic() {
	return getSubSurface(this->r.cr, this->filterRegion);
}


void FilterApplyer::setResult(const std::string& name, FilterResult&& result) {
	this->results[name] = std::move(result);
	this->lastResult = &this->results[name];
	ASSERT(this->lastResult->surface.data == &*this->lastResult->data.begin())
}

Surface FilterApplyer::getSource(const std::string& in) {
	if(in == "SourceGraphic"){
		return this->getSourceGraphic();
	}
	if(in == "SourceAlpha"){
		//TODO: implement
		throw utki::Exc("not implemented");
	}
	if(in == "BackgroundImage"){
		return this->r.background;
	}
	if(in == "BackgroundAlpha"){
		//TODO: implement
		throw utki::Exc("not implemented");
	}
	if(in == "FillPaint"){
		//TODO: implement
		throw utki::Exc("not implemented");
	}
	if(in == "StrokePaint"){
		//TODO: implement
		throw utki::Exc("not implemented");
	}
	
	auto i = this->results.find(in);
	if(i != this->results.end()){
		return i->second.surface;
	}
	
	if(in.length() == 0){
		return this->getSourceGraphic();
	}
	
	//return empty surface
	return Surface();
}

Surface FilterApplyer::getLastResult() {
	if (!this->lastResult) {
		return Surface();
	}
	return this->lastResult->surface;
}
