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
FilterResult allocateResult(const Surface& src){
	FilterResult ret;
	ret.data.resize(src.width * src.height * sizeof(std::uint32_t));
	ret.surface = src;
	ret.surface.data = &*ret.data.begin();
	ret.surface.stride = ret.surface.width;
	
	return ret;
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
	
	FilterResult ret = allocateResult(src);
	
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

		this->filterRegion.x = unsigned(std::max(std::floor(frX), decltype(frX)(0)));
		this->filterRegion.y = unsigned(std::max(std::floor(frY), decltype(frY)(0)));
		this->filterRegion.width = unsigned(std::max(std::ceil(frWidth), decltype(frWidth)(0)));
		this->filterRegion.height = unsigned(std::max(std::ceil(frHeight), decltype(frHeight)(0)));
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
	
	auto s = this->getSource(e.in).intersectionSurface(this->filterRegion);
	
	//TODO: set filter sub-region

	this->setResult(e.result, cairoImageSurfaceBlur(s, sd));
}

namespace{
FilterResult colorMatrix(const Surface& s, const std::array<std::array<real, 5>, 4>& m){
//	TRACE(<< "colorMatrix(): s.width = " << s.width << " s.height = " << s.height << std::endl)
	FilterResult ret = allocateResult(s);
	
	for(unsigned y = 0; y != s.height; ++y){
		auto sp = &s.data[(y * s.stride) * sizeof(std::uint32_t)];
		auto dp = &ret.surface.data[(y * ret.surface.stride) * sizeof(std::uint32_t)];
		for(unsigned x = 0; x != s.width; ++x){
			auto r0 = real(*sp) / real(0xff);
//			TRACE(<< "r0 = " << r0 << std::endl)
			++sp;
			auto g0 = real(*sp) / real(0xff);
			++sp;
			auto b0 = real(*sp) / real(0xff);
			++sp;
			auto isOpaque = (*sp == 0xff);
			auto a0 = real(*sp) / real(0xff);
			++sp;
			
			if(!isOpaque){
				if(a0 > 0){
					//unpremultiply alpha
					r0 /= a0;
					g0 /= a0;
					b0 /= a0;
				}
			}
			
			ASSERT_INFO(real(0) <= r0 && r0 <= real(1), "r0 = " << r0)
			ASSERT_INFO(real(0) <= g0 && g0 <= real(1), "g0 = " << g0)
			ASSERT_INFO(real(0) <= b0 && b0 <= real(1), "b0 = " << b0)
			ASSERT_INFO(real(0) <= a0 && a0 <= real(1), "a0 = " << a0)
			
			auto r1 = m[0][0] * r0 + m[0][1] * g0 + m[0][2] * b0 + m[0][3] * a0 + m[0][4];
			auto g1 = m[1][0] * r0 + m[1][1] * g0 + m[1][2] * b0 + m[1][3] * a0 + m[1][4];
			auto b1 = m[2][0] * r0 + m[2][1] * g0 + m[2][2] * b0 + m[2][3] * a0 + m[2][4];
			auto a1 = m[3][0] * r0 + m[3][1] * g0 + m[3][2] * b0 + m[3][3] * a0 + m[3][4];
			
			//Alpha can change, so always premultiply alpha back
			r1 *= a1;
			g1 *= a1;
			b1 *= a1;
			
			*dp = std::uint8_t(r1 * real(0xff));
			++dp;
			*dp = std::uint8_t(g1 * real(0xff));
			++dp;
			*dp = std::uint8_t(b1 * real(0xff));
			++dp;
			*dp = std::uint8_t(a1 * real(0xff));
			++dp;
		}
	}
	
	return ret;
}
}

void FilterApplyer::visit(const svgdom::FeColorMatrixElement& e){
	std::array<std::array<real, 5>, 4> m; //first index = row, second index = column
	
	switch(e.type){
		case svgdom::FeColorMatrixElement::Type_e::MATRIX:
			for(unsigned i = 0, p = 0; i != m.size(); ++i){
				for(unsigned j = 0; j != m[i].size(); ++j, ++p){
					ASSERT(p < e.values.size())
					m[i][j] = e.values[p];
//					TRACE(<< "m[" << i << "][" << j << "] = " << m[i][j] << std::endl)
				}
			}
			break;
		case svgdom::FeColorMatrixElement::Type_e::SATURATE:
			/*
				| R' |     |0.213+0.787s  0.715-0.715s  0.072-0.072s 0  0 |   | R |
				| G' |     |0.213-0.213s  0.715+0.285s  0.072-0.072s 0  0 |   | G |
				| B' |  =  |0.213-0.213s  0.715-0.715s  0.072+0.928s 0  0 | * | B |
				| A' |     |           0            0             0  1  0 |   | A |
				| 1  |     |           0            0             0  0  1 |   | 1 |
			 */
			{
				auto s = real(e.values[0]);
				
				m = {{
					{{ real(0.213) + real(0.787) * s, real(0.715) - real(0.715) * s, real(0.072) - real(0.072) * s, real(0), real(0)}},
					{{ real(0.213) - real(0.213) * s, real(0.715) + real(0.285) * s, real(0.072) - real(0.072) * s, real(0), real(0)}},
					{{ real(0.213) - real(0.213) * s, real(0.715) - real(0.715) * s, real(0.072) + real(0.928) * s, real(0), real(0)}},
					{{ real(0),                       real(0),                       real(0),                       real(1), real(0)}}
				}};
			}
			break;
		case svgdom::FeColorMatrixElement::Type_e::HUE_ROTATE:
			/*
				| R' |     | a00  a01  a02  0  0 |   | R |
				| G' |     | a10  a11  a12  0  0 |   | G |
				| B' |  =  | a20  a21  a22  0  0 | * | B |
				| A' |     | 0    0    0    1  0 |   | A |
				| 1  |     | 0    0    0    0  1 |   | 1 |
			
				| a00 a01 a02 |   |+0.213 +0.715 +0.072|
				| a10 a11 a12 | = |+0.213 +0.715 +0.072| +
				| a20 a21 a22 |   |+0.213 +0.715 +0.072|

				         |+0.787 -0.715 -0.072|
				cos(a) * |-0.213 +0.285 -0.072| +
				         |-0.213 -0.715 +0.928|

				         |-0.213 -0.715+0.928|
				sin(a) * |+0.143 +0.140-0.283|
				         |-0.787 +0.715+0.072|
			*/
			{
				auto a = degToRad(real(e.values[0]));
				auto sina = std::sin(a);
				auto cosa = std::cos(a);
				
				m = {{
					{{ real(0.213) + cosa * real(0.787) - sina * real(0.213), real(0.715) - cosa * real(0.715) - sina * real(0.715), real(0.072) - cosa * real(0.072) + sina * real(0.928), real(0), real(0) }},
					{{ real(0.213) - cosa * real(0.213) + sina * real(0.143), real(0.715) + cosa * real(0.285) + sina * real(0.140), real(0.072) - cosa * real(0.072) - sina * real(0.283), real(0), real(0) }},
					{{ real(0.213) - cosa * real(0.213) - sina * real(0.787), real(0.715) - cosa * real(0.715) + sina * real(0.715), real(0.072) + cosa * real(0.928) + sina * real(0.072), real(0), real(0) }},
					{{ real(0),                                               real(0),                                               real(0),                                               real(1), real(0) }}
				}};
			}
			break;
		case svgdom::FeColorMatrixElement::Type_e::LUMINANCE_TO_ALPHA:
			/*
				| R' |     |      0        0        0  0  0 |   | R |
				| G' |     |      0        0        0  0  0 |   | G |
				| B' |  =  |      0        0        0  0  0 | * | B |
				| A' |     | 0.2125   0.7154   0.0721  0  0 |   | A |
				| 1  |     |      0        0        0  0  1 |   | 1 |
			 */
			m = {{
				{{ real(0),      real(0),      real(0),      real(0), real(0) }},
				{{ real(0),      real(0),      real(0),      real(0), real(0) }},
				{{ real(0),      real(0),      real(0),      real(0), real(0) }},
				{{ real(0.2125), real(0.7154), real(0.0721), real(0), real(0) }}
			}};
			break;
	}
	
	auto s = this->getSource(e.in).intersectionSurface(this->filterRegion);	
	
	//TODO: set filter sub-region
	
	this->setResult(e.result, colorMatrix(s, m));
}


namespace{
FilterResult blend(const Surface& in, const Surface& in2, svgdom::FeBlendElement::Mode_e mode){
//	TRACE(<< "in.width = " << in.width << " in2.width = " << in2.width << std::endl)
	auto s1 = in.intersectionSurface(in2);
	auto s2 = in2.intersectionSurface(in);
	
	ASSERT_INFO(s1.width == s2.width, "s1.width = " << s1.width << " s2.width = " << s2.width)
	ASSERT(s1.height == s2.height)
	ASSERT(s1.x == s2.x)
	ASSERT(s1.y == s2.y)
	
	auto ret = allocateResult(s1);
	
	for(unsigned y = 0; y != ret.surface.height; ++y){
		auto sp1 = &s1.data[(y * s1.stride) * sizeof(std::uint32_t)];
		auto sp2 = &s2.data[(y * s2.stride) * sizeof(std::uint32_t)];
		auto dp = &ret.surface.data[(y * ret.surface.stride) * sizeof(std::uint32_t)];
		for(unsigned x = 0; x != ret.surface.width; ++x){
			//TODO: optimize by using integer arithmetics instead of floating point
			auto r01 = real(*sp1) / real(0xff);
			auto r02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto g01 = real(*sp1) / real(0xff);
			auto g02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto b01 = real(*sp1) / real(0xff);
			auto b02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto a01 = real(*sp1) / real(0xff);
			auto a02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			
			/*
				cr = Result color (RGB) - premultiplied 
				qa = Opacity value at a given pixel for image A 
				qb = Opacity value at a given pixel for image B 
				ca = Color (RGB) at a given pixel for image A - premultiplied 
				cb = Color (RGB) at a given pixel for image B - premultiplied 
			*/
			switch(mode){
				case svgdom::FeBlendElement::Mode_e::NORMAL:
					//cr = (1 - qa) * cb + ca
					*dp = std::uint8_t( ((1 - a01) * r02 + r01) * real(0xff));
					++dp;
					*dp = std::uint8_t( ((1 - a01) * g02 + g01) * real(0xff));
					++dp;
					*dp = std::uint8_t( ((1 - a01) * b02 + b01) * real(0xff));
					++dp;
					break;
				case svgdom::FeBlendElement::Mode_e::MULTIPLY:
					//cr = (1-qa)*cb + (1-qb)*ca + ca*cb
					*dp = std::uint8_t( ((1 - a01) * r02 + (1 - a02) * r01 + r01 * r02) * real(0xff));
					++dp;
					*dp = std::uint8_t( ((1 - a01) * g02 + (1 - a02) * g01 + g01 * g02) * real(0xff));
					++dp;
					*dp = std::uint8_t( ((1 - a01) * b02 + (1 - a02) * b01 + b01 * b02) * real(0xff));
					++dp;
					break;
				case svgdom::FeBlendElement::Mode_e::SCREEN:
					//cr = cb + ca - ca * cb
					*dp = std::uint8_t( (r02 + r01 - r01 * r02) * real(0xff));
					++dp;
					*dp = std::uint8_t( (g02 + g01 - g01 * g02) * real(0xff));
					++dp;
					*dp = std::uint8_t( (b02 + b01 - b01 * b02) * real(0xff));
					++dp;
					break;
				case svgdom::FeBlendElement::Mode_e::DARKEN:
					//cr = Min ((1 - qa) * cb + ca, (1 - qb) * ca + cb)
					*dp = std::uint8_t( std::min((1 - a01) * r02 + r01, (1 - a02) * r01 + r02) * real(0xff));
					++dp;
					*dp = std::uint8_t( std::min((1 - a01) * g02 + g01, (1 - a02) * g01 + g02) * real(0xff));
					++dp;
					*dp = std::uint8_t( std::min((1 - a01) * b02 + b01, (1 - a02) * b01 + b02) * real(0xff));
					++dp;
					break;
				case svgdom::FeBlendElement::Mode_e::LIGHTEN:
					//cr = Max ((1 - qa) * cb + ca, (1 - qb) * ca + cb)
					*dp = std::uint8_t( std::max((1 - a01) * r02 + r01, (1 - a02) * r01 + r02) * real(0xff));
					++dp;
					*dp = std::uint8_t( std::max((1 - a01) * g02 + g01, (1 - a02) * g01 + g02) * real(0xff));
					++dp;
					*dp = std::uint8_t( std::max((1 - a01) * b02 + b01, (1 - a02) * b01 + b02) * real(0xff));
					++dp;
					break;
				default:
					ASSERT(false)
					dp += 3;
					break;
			}
			
			//qr = 1 - (1-qa)*(1-qb)
			*dp = std::uint8_t((1 - (1 - a01)* (1 - a02)) * real(0xff));
			++dp;
		}
	}
	
	return ret;
}
}

void FilterApplyer::visit(const svgdom::FeBlendElement& e){
	auto s1 = this->getSource(e.in).intersectionSurface(this->filterRegion);
	if(!s1.data){
		return;
	}
	
	auto s2 = this->getSource(e.in2).intersectionSurface(this->filterRegion);
	if(!s2.data){
		return;
	}
	
	//TODO: set filter sub-region
	
	this->setResult(e.result, blend(s1, s2, e.mode));
}

namespace{
FilterResult composite(const Surface& in, const Surface& in2, const svgdom::FeCompositeElement& e){
//	TRACE(<< "in.width = " << in.width << " in2.width = " << in2.width << std::endl)
	auto s1 = in.intersectionSurface(in2);
	auto s2 = in2.intersectionSurface(in);
	
	ASSERT_INFO(s1.width == s2.width, "s1.width = " << s1.width << " s2.width = " << s2.width)
	ASSERT(s1.height == s2.height)
	ASSERT(s1.x == s2.x)
	ASSERT(s1.y == s2.y)
	
	auto ret = allocateResult(s1);
	
	for(unsigned y = 0; y != ret.surface.height; ++y){
		auto sp1 = &s1.data[(y * s1.stride) * sizeof(std::uint32_t)];
		auto sp2 = &s2.data[(y * s2.stride) * sizeof(std::uint32_t)];
		auto dp = &ret.surface.data[(y * ret.surface.stride) * sizeof(std::uint32_t)];
		for(unsigned x = 0; x != ret.surface.width; ++x){
			//TODO: optimize by using integer arithmetics instead of floating point
			auto r01 = real(*sp1) / real(0xff);
			auto r02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto g01 = real(*sp1) / real(0xff);
			auto g02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto b01 = real(*sp1) / real(0xff);
			auto b02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto a01 = real(*sp1) / real(0xff);
			auto a02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			
			switch(e.operator_v){
				case svgdom::FeCompositeElement::Operator_e::OVER:
					//co = as * Cs + ab * Cb * (1 – as)
					//ao = as + ab * (1 – as)
					*dp = std::uint8_t( (r01 + r02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (g01 + g02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (b01 + b02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (a01 + a02 * (1 - a01)) * real(0xff));
					++dp;
					break;
				case svgdom::FeCompositeElement::Operator_e::IN:
					//co = as * Cs * ab
					//ao = as x ab
					*dp = std::uint8_t( (r01 * a02) * real(0xff));
					++dp;
					*dp = std::uint8_t( (g01 * a02) * real(0xff));
					++dp;
					*dp = std::uint8_t( (b01 * a02) * real(0xff));
					++dp;
					*dp = std::uint8_t( (a01 * a02) * real(0xff));
					++dp;
					break;
				case svgdom::FeCompositeElement::Operator_e::OUT:
					//co = as * Cs * (1 – ab)
					//ao = as * (1 – ab)
					*dp = std::uint8_t( (r01 * (1 - a02)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (g01 * (1 - a02)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (b01 * (1 - a02)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (a01 * (1 - a02)) * real(0xff));
					++dp;
					break;
				case svgdom::FeCompositeElement::Operator_e::ATOP:
					//co = as * Cs * ab + ab * Cb * (1 – as)
					//ao = as * ab + ab * (1 – as)
					*dp = std::uint8_t( (r01 * a02 + r02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (g01 * a02 + g02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (b01 * a02 + b02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (a01 * a02 + a02 * (1 - a01)) * real(0xff));
					++dp;
					break;
				case svgdom::FeCompositeElement::Operator_e::XOR:
					//co = as * Cs * (1 - ab) + ab * Cb * (1 – as)
					//ao = as * (1 - ab) + ab * (1 – as)
					*dp = std::uint8_t( (r01 * (1 - a02) + r02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (g01 * (1 - a02) + g02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (b01 * (1 - a02) + b02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = std::uint8_t( (a01 * (1 - a02) + a02 * (1 - a01)) * real(0xff));
					++dp;
					break;
				case svgdom::FeCompositeElement::Operator_e::ARITHMETIC:
					//result = k1*i1*i2 + k2*i1 + k3*i2 + k4
					*dp = std::uint8_t( (e.k1 * r01 * r02 + e.k2 * r01 + e.k3 * r02 + e.k4) * real(0xff));
					++dp;
					*dp = std::uint8_t( (e.k1 * g01 * g02 + e.k2 * g01 + e.k3 * g02 + e.k4) * real(0xff));
					++dp;
					*dp = std::uint8_t( (e.k1 * b01 * b02 + e.k2 * b01 + e.k3 * b02 + e.k4) * real(0xff));
					++dp;
					*dp = std::uint8_t( (e.k1 * a01 * a02 + e.k2 * a01 + e.k3 * a02 + e.k4) * real(0xff));
					++dp;
					break;
				default:
					ASSERT(false)
					dp += 4;
					break;
			}
		}
	}
	
	return ret;
}
}

void FilterApplyer::visit(const svgdom::FeCompositeElement& e){
	auto s1 = this->getSource(e.in).intersectionSurface(this->filterRegion);
	if(!s1.data){
		return;
	}
	
	auto s2 = this->getSource(e.in2).intersectionSurface(this->filterRegion);
	if(!s2.data){
		return;
	}
	
	//TODO: set filter sub-region
	
	this->setResult(e.result, composite(s1, s2, e));
}
