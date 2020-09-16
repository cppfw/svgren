#include "FilterApplier.hxx"

#include <cmath>
#include <stdexcept>

#include <utki/debug.hpp>
#include <utki/math.hpp>

#include <r4/matrix4.hpp>

#include "util.hxx"

using namespace svgren;

namespace{
void boxBlurHorizontal(
		uint8_t* dst,
		const uint8_t* src,
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
		using std::min;
		using std::max;

		unsigned sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = max(pos, 0);
			pos = min(pos, int(width - 1));
			sum += src[(srcStride * y + pos) * sizeof(uint32_t) + channel];
		}
		for(unsigned x = 0; x != width; ++x){
			int tmp = x - boxOffset;
			int last = max(tmp, 0);
			int next = min(tmp + boxSize, width - 1);

			dst[(dstStride * y + x) * sizeof(uint32_t) + channel] = sum / boxSize;

			sum += src[(srcStride * y + next) * sizeof(uint32_t) + channel]
					- src[(srcStride * y + last) * sizeof(uint32_t) + channel];
		}
	}
}
}

namespace{
void boxBlurVertical(
		uint8_t* dst,
		const uint8_t* src,
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
		using std::min;
		using std::max;
		unsigned sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = max(pos, 0);
			pos = min(pos, int(height - 1));
			sum += src[(srcStride * pos + x) * sizeof(uint32_t) + channel];
		}
		for(unsigned y = 0; y != height; ++y){
			int tmp = y - boxOffset;
			int last = max(tmp, 0);
			int next = min(tmp + boxSize, height - 1);

			dst[(dstStride * y + x) * sizeof(uint32_t) + channel] = sum / boxSize;

			sum += src[(x + srcStride * next) * sizeof(uint32_t) + channel]
					- src[(x + srcStride * last) * sizeof(uint32_t) + channel];
		}
	}
}
}

namespace{
FilterResult allocateResult(const Surface& src){
	FilterResult ret;
	ret.surface = src;
	auto dataSize = src.d.x() * src.d.y() * sizeof(uint32_t);
	if (dataSize != 0) {
		ret.data.resize(dataSize);
		ASSERT_INFO(ret.data.size() != 0, "src.d = " << src.d)
		ret.surface.data = ret.data.data();
		ret.surface.stride = ret.surface.d.x();
		ret.surface.end = ret.data.data() + ret.data.size();
	}else{
		ret.data.clear();
		ret.surface.data = nullptr;
		ret.surface.stride = 0;
		ret.surface.end = nullptr;
	}
	
	return ret;
}
}

namespace{
FilterResult cairoImageSurfaceBlur(const Surface& src, r4::vector2<real> stdDeviation){
	// see https://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for Gaussian Blur approximation algorithm
	
	ASSERT(src.d.x() <= src.stride)
	
	using std::sqrt;
	auto d = (stdDeviation * (3 * sqrt(2 * utki::pi<real>()) / 4) + real(0.5)).to<unsigned>();
	
//	TRACE(<< "d = " << d[0] << ", " << d[1] << std::endl)
	
	FilterResult ret = allocateResult(src);
	
	std::vector<uint8_t> tmp(ret.data.size());
	
	std::array<unsigned, 3> hBoxSize;
	std::array<unsigned, 3> hOffset;
	std::array<unsigned, 3> vBoxSize;
	std::array<unsigned, 3> vOffset;
	if(d[0] % 2 == 0){
		hOffset[0] = d[0] / 2;
		hBoxSize[0] = d[0];
		hOffset[1] = d[0] / 2 - 1; // it is ok if d[0] is 0 and -1 will give a large number because box size is also 0 in that case and blur will have no effect anyway
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
		vOffset[1] = d[1] / 2 - 1; // it is ok if d[0] is 0 and -1 will give a large number because box size is also 0 in that case and blur will have no effect anyway
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
		boxBlurHorizontal(tmp.data(), src.data, src.d.x(), src.stride, src.d.x(), src.d.y(), hBoxSize[0], hOffset[0], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(ret.surface.data, tmp.data(), ret.surface.stride, src.d.x(), src.d.x(), src.d.y(), hBoxSize[1], hOffset[1], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(tmp.data(), ret.surface.data, src.d.x(), ret.surface.stride, src.d.x(), src.d.y(), hBoxSize[2], hOffset[2], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(ret.surface.data, tmp.data(), ret.surface.stride, src.d.x(), src.d.x(), src.d.y(), vBoxSize[0], vOffset[0], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(tmp.data(), ret.surface.data, src.d.x(), ret.surface.stride, src.d.x(), src.d.y(), vBoxSize[1], vOffset[1], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(ret.surface.data, tmp.data(), ret.surface.stride, src.d.x(), src.d.x(), src.d.y(), vBoxSize[2], vOffset[2], channel);
	}
	
	return ret;
}
}

Surface FilterApplier::getSourceGraphic(){
	return getSubSurface(this->r.cr, this->filterRegion);
}

void FilterApplier::setResult(const std::string& name, FilterResult&& result){
	this->results[name] = std::move(result);
	this->lastResult = &this->results[name];
	ASSERT(this->lastResult->data.size() == 0 || this->lastResult->surface.data == &*this->lastResult->data.begin())
}

Surface FilterApplier::getSource(const std::string& in){
	if(in == "SourceGraphic"){
		return this->getSourceGraphic();
	}
	if(in == "SourceAlpha"){
		// TODO: implement
		throw std::runtime_error("SourceAlpha not implemented");
	}
	if(in == "BackgroundImage"){
		return this->r.background;
	}
	if(in == "BackgroundAlpha"){
		// TODO: implement
		throw std::runtime_error("BackgroundAlpha not implemented");
	}
	if(in == "FillPaint"){
		// TODO: implement
		throw std::runtime_error("FillPaint not implemented");
	}
	if(in == "StrokePaint"){
		// TODO: implement
		throw std::runtime_error("StrokePaint not implemented");
	}
	
	auto i = this->results.find(in);
	if(i != this->results.end()){
		return i->second.surface;
	}
	
	if(in.length() == 0){
		return this->getSourceGraphic();
	}
	
	// return empty surface
	return Surface();
}

Surface FilterApplier::getLastResult(){
	if (!this->lastResult) {
		return Surface();
	}
	return this->lastResult->surface;
}

void FilterApplier::visit(const svgdom::filter_element& e){
	this->primitiveUnits = e.primitive_units;
	
	// set filter region
	{
		// filter region
		r4::rectangle<real> fr;

		switch(e.filter_units){
			default:
			case svgdom::coordinate_units::object_bounding_box:
				{
					r4::vector2<real> fe_pos{
						percentLengthToFraction(e.x),
						percentLengthToFraction(e.y)
					};

					r4::vector2<real> fe_dims{
						percentLengthToFraction(e.width),
						percentLengthToFraction(e.height)
					};

					fr.p = this->r.deviceSpaceBoundingBox.pos() + fe_pos.comp_mul(this->r.deviceSpaceBoundingBox.dims());
					fr.d = fe_dims.comp_mul(this->r.deviceSpaceBoundingBox.dims());
				}
				break;
			case svgdom::coordinate_units::user_space_on_use:
				{
					auto x1 = this->r.length_to_px(e.x, 0);
					auto y1 = this->r.length_to_px(e.y, 1);
					auto x2 = x1 + this->r.length_to_px(e.width, 0);
					auto y2 = y1 + this->r.length_to_px(e.height, 1);

					std::array<r4::vector2<real>, 4> rectVertices = {{
						r4::vector2<real>{x1, y1},
						r4::vector2<real>{x2, y2},
						r4::vector2<real>{x1, y2},
						r4::vector2<real>{x2, y1}
					}};

					DeviceSpaceBoundingBox frBb;
					frBb.set_empty();

					for(auto& vertex : rectVertices){
						vertex = this->r.canvas.matrix_mul(vertex);

						DeviceSpaceBoundingBox bb;
						bb.left = decltype(bb.left)(vertex.x());
						bb.right = decltype(bb.right)(vertex.x());
						bb.top = decltype(bb.top)(vertex.y());
						bb.bottom = decltype(bb.bottom)(vertex.y());

						frBb.merge(bb);
					}
					fr.p = frBb.pos();
					fr.d = frBb.dims();
				}
				break;
		}

		using std::floor;
		using std::ceil;
		using std::max;

		this->filterRegion.p = max(floor(fr.p), 0).to<unsigned>();
		this->filterRegion.d = max(ceil(fr.d), 0).to<unsigned>();
	}
	
	this->relay_accept(e);
}

void FilterApplier::visit(const svgdom::fe_gaussian_blur_element& e){
	if (!e.is_std_deviation_specified()) {
		return;
	}
	auto sd = e.get_std_deviation().to<real>();

	switch(this->primitiveUnits){
		default:
		case svgdom::coordinate_units::user_space_on_use:
			sd = this->r.canvas.matrix_mul_distance(sd);
			break;
		case svgdom::coordinate_units::object_bounding_box:
			sd = this->r.canvas.matrix_mul_distance(
					this->r.userSpaceShapeBoundingBox.d.comp_mul(sd)
				);
			break;
	}

	auto s = this->getSource(e.in).intersectionSurface(this->filterRegion);
	
	// TODO: set filter sub-region

	this->setResult(e.result, cairoImageSurfaceBlur(s, sd));
}

namespace{
FilterResult color_matrix(const Surface& s, const r4::matrix4<real>& m, const r4::vector4<real>& mc5){
//	TRACE(<< "colorMatrix(): s.width = " << s.width << " s.height = " << s.height << std::endl)
	FilterResult ret = allocateResult(s);
	
	ASSERT(s.data)
	
	for(unsigned y = 0; y != s.d.y(); ++y){
		auto sp = &s.data[(y * s.stride) * sizeof(uint32_t)];
		ASSERT_INFO(sp < s.end, "sp = " << std::hex << static_cast<void*>(sp) << " s.end = " << static_cast<void*>(s.end))
		auto dp = &ret.surface.data[(y * ret.surface.stride) * sizeof(uint32_t)];
		for(unsigned x = 0; x != s.d.x(); ++x){
			auto bb = *sp;
			++sp;
			auto gg = *sp;
			++sp;
			auto rr = *sp;
			++sp;
			auto aa = *sp;
			++sp;
			
			if(aa != 0xff && aa != 0){
				// unpremultiply alpha
				rr = uint32_t(rr) * uint32_t(0xff) / uint32_t(aa);
				gg = uint32_t(gg) * uint32_t(0xff) / uint32_t(aa);
				bb = uint32_t(bb) * uint32_t(0xff) / uint32_t(aa);
			}
			
			r4::vector4<real> c;
			c.r() = real(rr) / real(0xff);
			c.g() = real(gg) / real(0xff);
			c.b() = real(bb) / real(0xff);
			c.a() = real(aa) / real(0xff);
			
			ASSERT_INFO(real(0) <= c.r() && c.r() <= real(1), "r = " << c.r() << ", rr = " << rr)
			ASSERT_INFO(real(0) <= c.g() && c.g() <= real(1), "g = " << c.g() << ", gg = " << gg)
			ASSERT_INFO(real(0) <= c.b() && c.b() <= real(1), "b = " << c.b() << ", bb = " << bb)
			ASSERT_INFO(real(0) <= c.a() && c.a() <= real(1), "a = " << c.a() << ", aa = " << aa)
			
			auto c1 = m * c + mc5;

			// alpha can change, so always premultiply alpha back
			c1.r() *= c1.a();
			c1.g() *= c1.a();
			c1.b() *= c1.a();
			
			*dp = uint8_t(c1.b() * real(0xff));
			++dp;
			*dp = uint8_t(c1.g() * real(0xff));
			++dp;
			*dp = uint8_t(c1.r() * real(0xff));
			++dp;
			*dp = uint8_t(c1.a() * real(0xff));
			++dp;
		}
	}
	
	return ret;
}
}

void FilterApplier::visit(const svgdom::fe_color_matrix_element& e){
	r4::matrix4<real> m; // 1st to 4th columns of the 5x4 matrix
	r4::vector4<real> mc5; // fifth column of the 5x4 matrix
	
	switch(e.type_){
		case svgdom::fe_color_matrix_element::type::matrix:
			for(unsigned i = 0, p = 0; i != m.size(); ++i){
				unsigned j = 0;
				for(; j != m[i].size(); ++j, ++p){
					ASSERT(p < e.values.size())
					m[i][j] = e.values[p];
//					TRACE(<< "m[" << i << "][" << j << "] = " << m[i][j] << std::endl)
				}
				ASSERT(p < e.values.size())
				mc5[i] = e.values[p];
				++p;
			}
			break;
		case svgdom::fe_color_matrix_element::type::saturate:
			/*
				| R' |     | 0.213 +0.787s  0.715-0.715s  0.072-0.072s 0  0 |   | R |
				| G' |     | 0.213 -0.213s  0.715+0.285s  0.072-0.072s 0  0 |   | G |
				| B' |  =  | 0.213 -0.213s  0.715-0.715s  0.072+0.928s 0  0 | * | B |
				| A' |     |             0             0             0 1  0 |   | A |
				| 1  |     |             0             0             0 0  1 |   | 1 |
			 */
			{
				auto s = real(e.values[0]);
				
				m = {
					{ real(0.213) + real(0.787) * s, real(0.715) - real(0.715) * s, real(0.072) - real(0.072) * s, real(0)},
					{ real(0.213) - real(0.213) * s, real(0.715) + real(0.285) * s, real(0.072) - real(0.072) * s, real(0)},
					{ real(0.213) - real(0.213) * s, real(0.715) - real(0.715) * s, real(0.072) + real(0.928) * s, real(0)},
					{ real(0),                       real(0),                       real(0),                       real(1)}
				};
				mc5.set(real(0));
			}
			break;
		case svgdom::fe_color_matrix_element::type::hue_rotate:
			/*
				| R' |     | a00  a01  a02  0  0 |   | R |
				| G' |     | a10  a11  a12  0  0 |   | G |
				| B' |  =  | a20  a21  a22  0  0 | * | B |
				| A' |     | 0    0    0    1  0 |   | A |
				| 1  |     | 0    0    0    0  1 |   | 1 |
			
				| a00 a01 a02 |   | +0.213 +0.715 +0.072 |
				| a10 a11 a12 | = | +0.213 +0.715 +0.072 | +
				| a20 a21 a22 |   | +0.213 +0.715 +0.072 |

				         | +0.787 -0.715 -0.072 |
				cos(a) * | -0.213 +0.285 -0.072 | +
				         | -0.213 -0.715 +0.928 |

				         | -0.213 -0.715 +0.928 |
				sin(a) * | +0.143 +0.140 -0.283 |
				         | -0.787 +0.715 +0.072 |
			*/
			{
				using std::sin;
				using std::cos;
				
				auto a = deg_to_rad(real(e.values[0]));
				auto sina = sin(a);
				auto cosa = cos(a);
				
				m = {
					{ real(0.213) + cosa * real(0.787) - sina * real(0.213), real(0.715) - cosa * real(0.715) - sina * real(0.715), real(0.072) - cosa * real(0.072) + sina * real(0.928), real(0) },
					{ real(0.213) - cosa * real(0.213) + sina * real(0.143), real(0.715) + cosa * real(0.285) + sina * real(0.140), real(0.072) - cosa * real(0.072) - sina * real(0.283), real(0) },
					{ real(0.213) - cosa * real(0.213) - sina * real(0.787), real(0.715) - cosa * real(0.715) + sina * real(0.715), real(0.072) + cosa * real(0.928) + sina * real(0.072), real(0) },
					{ real(0),                                               real(0),                                               real(0),                                               real(1) }
				};
				mc5.set(real(0));
			}
			break;
		case svgdom::fe_color_matrix_element::type::luminance_to_alpha:
			/*
				| R' |     |      0        0        0  0  0 |   | R |
				| G' |     |      0        0        0  0  0 |   | G |
				| B' |  =  |      0        0        0  0  0 | * | B |
				| A' |     | 0.2125   0.7154   0.0721  0  0 |   | A |
				| 1  |     |      0        0        0  0  1 |   | 1 |
			 */
			m = {
				{ real(0),      real(0),      real(0),      real(0) },
				{ real(0),      real(0),      real(0),      real(0) },
				{ real(0),      real(0),      real(0),      real(0) },
				{ real(0.2125), real(0.7154), real(0.0721), real(0) }
			};
			mc5.set(real(0));
			break;
	}
	
	auto s = this->getSource(e.in).intersectionSurface(this->filterRegion);	
	
	// TODO: set filter sub-region
	
	this->setResult(e.result, color_matrix(s, m, mc5));
}

namespace{
FilterResult blend(const Surface& in, const Surface& in2, svgdom::fe_blend_element::mode mode){
//	TRACE(<< "in.width = " << in.width << " in2.width = " << in2.width << std::endl)
	auto s1 = in.intersectionSurface(in2);
	auto s2 = in2.intersectionSurface(in);
	
	ASSERT_INFO(s1.d.x() == s2.d.x(), "s1.d.x() = " << s1.d.x() << " s2.d.x() = " << s2.d.x())
	ASSERT(s1.d.y() == s2.d.y())
	ASSERT(s1.p == s2.p)
	
	auto ret = allocateResult(s1);
	
	for(unsigned y = 0; y != ret.surface.d.y(); ++y){
		auto sp1 = &s1.data[(y * s1.stride) * sizeof(uint32_t)];
		auto sp2 = &s2.data[(y * s2.stride) * sizeof(uint32_t)];
		auto dp = &ret.surface.data[(y * ret.surface.stride) * sizeof(uint32_t)];
		for(unsigned x = 0; x != ret.surface.d.x(); ++x){
			// TODO: optimize by using integer arithmetics instead of floating point
			auto b01 = real(*sp1) / real(0xff);
			auto b02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto g01 = real(*sp1) / real(0xff);
			auto g02 = real(*sp2) / real(0xff);
			++sp1;
			++sp2;
			auto r01 = real(*sp1) / real(0xff);
			auto r02 = real(*sp2) / real(0xff);
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
				case svgdom::fe_blend_element::mode::normal:
					// cr = (1 - qa) * cb + ca
					*dp = uint8_t( ((1 - a01) * b02 + b01) * real(0xff));
					++dp;
					*dp = uint8_t( ((1 - a01) * g02 + g01) * real(0xff));
					++dp;
					*dp = uint8_t( ((1 - a01) * r02 + r01) * real(0xff));
					++dp;
					break;
				case svgdom::fe_blend_element::mode::multiply:
					// cr = (1 - qa) * cb + (1 - qb) * ca + ca * cb
					*dp = uint8_t( ((1 - a01) * b02 + (1 - a02) * b01 + b01 * b02) * real(0xff));
					++dp;
					*dp = uint8_t( ((1 - a01) * g02 + (1 - a02) * g01 + g01 * g02) * real(0xff));
					++dp;
					*dp = uint8_t( ((1 - a01) * r02 + (1 - a02) * r01 + r01 * r02) * real(0xff));
					++dp;
					break;
				case svgdom::fe_blend_element::mode::screen:
					// cr = cb + ca - ca * cb
					*dp = uint8_t( (b02 + b01 - b01 * b02) * real(0xff));
					++dp;
					*dp = uint8_t( (g02 + g01 - g01 * g02) * real(0xff));
					++dp;
					*dp = uint8_t( (r02 + r01 - r01 * r02) * real(0xff));
					++dp;
					break;
				case svgdom::fe_blend_element::mode::darken:
					using std::min;
					// cr = Min ((1 - qa) * cb + ca, (1 - qb) * ca + cb)
					*dp = uint8_t( min((1 - a01) * b02 + b01, (1 - a02) * b01 + b02) * real(0xff));
					++dp;
					*dp = uint8_t( min((1 - a01) * g02 + g01, (1 - a02) * g01 + g02) * real(0xff));
					++dp;
					*dp = uint8_t( min((1 - a01) * r02 + r01, (1 - a02) * r01 + r02) * real(0xff));
					++dp;
					break;
				case svgdom::fe_blend_element::mode::lighten:
					using std::max;
					// cr = Max ((1 - qa) * cb + ca, (1 - qb) * ca + cb)
					*dp = uint8_t( max((1 - a01) * b02 + b01, (1 - a02) * b01 + b02) * real(0xff));
					++dp;
					*dp = uint8_t( max((1 - a01) * g02 + g01, (1 - a02) * g01 + g02) * real(0xff));
					++dp;
					*dp = uint8_t( max((1 - a01) * r02 + r01, (1 - a02) * r01 + r02) * real(0xff));
					++dp;
					break;
				default:
					ASSERT(false)
					dp += 3;
					break;
			}
			
			// qr = 1 - (1 - qa) * (1 - qb)
			*dp = uint8_t((1 - (1 - a01)* (1 - a02)) * real(0xff));
			++dp;
		}
	}
	
	return ret;
}
}

void FilterApplier::visit(const svgdom::fe_blend_element& e){
	auto s1 = this->getSource(e.in).intersectionSurface(this->filterRegion);
	if(!s1.data){
		return;
	}
	
	auto s2 = this->getSource(e.in2).intersectionSurface(this->filterRegion);
	if(!s2.data){
		return;
	}
	
	// TODO: set filter sub-region
	
	this->setResult(e.result, blend(s1, s2, e.mode_));
}

namespace{
FilterResult composite(const Surface& in, const Surface& in2, const svgdom::fe_composite_element& e){
//	TRACE(<< "in.width = " << in.width << " in2.width = " << in2.width << std::endl)
	auto s1 = in.intersectionSurface(in2);
	auto s2 = in2.intersectionSurface(in);
	
	ASSERT_INFO(s1.d.x() == s2.d.x(), "s1.d.x() = " << s1.d.x() << " s2.d.x()) = " << s2.d.x())
	ASSERT(s1.d.y() == s2.d.y())
	ASSERT(s1.p == s2.p)
	
	auto ret = allocateResult(s1);
	
	for(unsigned y = 0; y != ret.surface.d.y(); ++y){
		auto sp1 = &s1.data[(y * s1.stride) * sizeof(uint32_t)];
		auto sp2 = &s2.data[(y * s2.stride) * sizeof(uint32_t)];
		auto dp = &ret.surface.data[(y * ret.surface.stride) * sizeof(uint32_t)];
		for(unsigned x = 0; x != ret.surface.d.x(); ++x){
			// TODO: optimize by using integer arithmetics instead of floating point
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
			
			switch(e.operator__){
				case svgdom::fe_composite_element::operator_::over:
					// co = as * Cs + ab * Cb * (1 – as)
					// ao = as + ab * (1 – as)
					*dp = uint8_t( (r01 + r02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (g01 + g02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (b01 + b02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (a01 + a02 * (1 - a01)) * real(0xff));
					++dp;
					break;
				case svgdom::fe_composite_element::operator_::in:
					// co = as * Cs * ab
					// ao = as x ab
					*dp = uint8_t( (r01 * a02) * real(0xff));
					++dp;
					*dp = uint8_t( (g01 * a02) * real(0xff));
					++dp;
					*dp = uint8_t( (b01 * a02) * real(0xff));
					++dp;
					*dp = uint8_t( (a01 * a02) * real(0xff));
					++dp;
					break;
				case svgdom::fe_composite_element::operator_::out:
					// co = as * Cs * (1 – ab)
					// ao = as * (1 – ab)
					*dp = uint8_t( (r01 * (1 - a02)) * real(0xff));
					++dp;
					*dp = uint8_t( (g01 * (1 - a02)) * real(0xff));
					++dp;
					*dp = uint8_t( (b01 * (1 - a02)) * real(0xff));
					++dp;
					*dp = uint8_t( (a01 * (1 - a02)) * real(0xff));
					++dp;
					break;
				case svgdom::fe_composite_element::operator_::atop:
					// co = as * Cs * ab + ab * Cb * (1 – as)
					// ao = as * ab + ab * (1 – as)
					*dp = uint8_t( (r01 * a02 + r02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (g01 * a02 + g02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (b01 * a02 + b02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (a01 * a02 + a02 * (1 - a01)) * real(0xff));
					++dp;
					break;
				case svgdom::fe_composite_element::operator_::xor_:
					// co = as * Cs * (1 - ab) + ab * Cb * (1 – as)
					// ao = as * (1 - ab) + ab * (1 – as)
					*dp = uint8_t( (r01 * (1 - a02) + r02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (g01 * (1 - a02) + g02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (b01 * (1 - a02) + b02 * (1 - a01)) * real(0xff));
					++dp;
					*dp = uint8_t( (a01 * (1 - a02) + a02 * (1 - a01)) * real(0xff));
					++dp;
					break;
				case svgdom::fe_composite_element::operator_::arithmetic:
					using std::min;
					// result = k1 * i1 * i2 + k2 * i1 + k3 * i2 + k4
					*dp = uint8_t( min(e.k1 * r01 * r02 + e.k2 * r01 + e.k3 * r02 + e.k4, real(1)) * real(0xff));
					++dp;
					*dp = uint8_t( min(e.k1 * g01 * g02 + e.k2 * g01 + e.k3 * g02 + e.k4, real(1)) * real(0xff));
					++dp;
					*dp = uint8_t( min(e.k1 * b01 * b02 + e.k2 * b01 + e.k3 * b02 + e.k4, real(1)) * real(0xff));
					++dp;
					*dp = uint8_t( min(e.k1 * a01 * a02 + e.k2 * a01 + e.k3 * a02 + e.k4, real(1)) * real(0xff));
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

void FilterApplier::visit(const svgdom::fe_composite_element& e){
	auto s1 = this->getSource(e.in).intersectionSurface(this->filterRegion);
	if(!s1.data){
		return;
	}
	
	auto s2 = this->getSource(e.in2).intersectionSurface(this->filterRegion);
	if(!s2.data){
		return;
	}
	
	// TODO: set filter sub-region
	
	this->setResult(e.result, composite(s1, s2, e));
}
