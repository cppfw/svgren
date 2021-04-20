#include "filter_applier.hxx"

#include <cmath>
#include <stdexcept>

#include <utki/debug.hpp>
#include <utki/math.hpp>

#include <r4/matrix.hpp>

#include "util.hxx"

using namespace svgren;

namespace{
void boxBlurHorizontal(
		uint32_t* dst,
		const uint32_t* src,
		unsigned dstStride,
		unsigned srcStride,
		unsigned width,
		unsigned height,
		unsigned boxSize,
		unsigned boxOffset
	)
{
	if(boxSize == 0){
		return;
	}
	for(unsigned y = 0; y != height; ++y){
		using std::min;
		using std::max;

		r4::vector4<unsigned> sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = max(pos, 0);
			pos = min(pos, int(width - 1));
			sum += to_rgba(src[srcStride * y + pos]);
		}
		for(unsigned x = 0; x != width; ++x){
			int tmp = x - boxOffset;
			int last = max(tmp, 0);
			int next = min(tmp + boxSize, width - 1);

			dst[dstStride * y + x] = to_pixel((sum / boxSize));

			sum += to_rgba(src[srcStride * y + next]) - to_rgba(src[srcStride * y + last]);
		}
	}
}
}

namespace{
void boxBlurVertical(
		uint32_t* dst,
		const uint32_t* src,
		unsigned dstStride,
		unsigned srcStride,
		unsigned width,
		unsigned height,
		unsigned boxSize,
		unsigned boxOffset
	)
{
	if(boxSize == 0){
		return;
	}
	for(unsigned x = 0; x != width; ++x){
		using std::min;
		using std::max;

		r4::vector4<unsigned> sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = max(pos, 0);
			pos = min(pos, int(height - 1));

			sum += to_rgba(src[srcStride * pos + x]);
		}
		for(unsigned y = 0; y != height; ++y){
			int tmp = y - boxOffset;
			int last = max(tmp, 0);
			int next = min(tmp + boxSize, height - 1);

			dst[dstStride * y + x] = to_pixel(sum / boxSize);

			sum += to_rgba(src[srcStride * next + x]) - to_rgba(src[srcStride * last + x]);
		}
	}
}
}

namespace{
filter_result allocateResult(const surface& src){
	filter_result ret;
	ret.surface = src;
	auto dataSize = src.d.x() * src.d.y();
	if (dataSize != 0) {
		ret.data.resize(dataSize);
		ASSERT_INFO(ret.data.size() != 0, "src.d = " << src.d)
		ret.surface.span = utki::make_span(ret.data);
		ret.surface.stride = ret.surface.d.x();
	}else{
		ret.data.clear();
		ret.surface.span = nullptr;
		ret.surface.stride = 0;
	}
	
	return ret;
}
}

namespace{
filter_result blur_surface(const surface& src, r4::vector2<real> stdDeviation){
	// see https://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for Gaussian Blur approximation algorithm
	
	ASSERT(src.d.x() <= src.stride)
	
	using std::sqrt;
	auto d = (stdDeviation * (3 * sqrt(2 * utki::pi<real>()) / 4) + real(0.5)).to<unsigned>();
	
//	TRACE(<< "d = " << d[0] << ", " << d[1] << std::endl)
	
	filter_result ret = allocateResult(src);
	
	std::vector<uint32_t> tmp(ret.data.size());
	
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
	
	boxBlurHorizontal(tmp.data(), src.span.data(), src.d.x(), src.stride, src.d.x(), src.d.y(), hBoxSize[0], hOffset[0]);
	boxBlurHorizontal(ret.surface.span.data(), tmp.data(), ret.surface.stride, src.d.x(), src.d.x(), src.d.y(), hBoxSize[1], hOffset[1]);
	boxBlurHorizontal(tmp.data(), ret.surface.span.data(), src.d.x(), ret.surface.stride, src.d.x(), src.d.y(), hBoxSize[2], hOffset[2]);

	boxBlurVertical(ret.surface.span.data(), tmp.data(), ret.surface.stride, src.d.x(), src.d.x(), src.d.y(), vBoxSize[0], vOffset[0]);
	boxBlurVertical(tmp.data(), ret.surface.span.data(), src.d.x(), ret.surface.stride, src.d.x(), src.d.y(), vBoxSize[1], vOffset[1]);
	boxBlurVertical(ret.surface.span.data(), tmp.data(), ret.surface.stride, src.d.x(), src.d.x(), src.d.y(), vBoxSize[2], vOffset[2]);
	
	return ret;
}
}

surface filter_applier::get_source_graphic(){
	return this->r.canvas.get_sub_surface(this->filterRegion);
}

void filter_applier::set_result(const std::string& name, filter_result&& result){
	this->results[name] = std::move(result);
	this->lastResult = &this->results[name];
	ASSERT(this->lastResult->data.size() == 0 || this->lastResult->surface.span.data() == this->lastResult->data.data())
}

surface filter_applier::get_source(const std::string& in){
	if(in == "SourceGraphic"){
		// TRACE(<< "source graphic" << std::endl)
		return this->get_source_graphic();
	}
	if(in == "SourceAlpha"){
		// TODO: implement
		throw std::runtime_error("SourceAlpha not implemented");
	}
	if(in == "BackgroundImage"){
		// TRACE(<< "background image" << std::endl)
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
		// TRACE(<< "results" << std::endl)
		return i->second.surface;
	}
	
	if(in.length() == 0){
		// TRACE(<< "length = 0" << std::endl)
		return this->get_source_graphic();
	}
	
	// return empty surface
	// TRACE(<< "empty surface" << std::endl)
	return surface();
}

surface filter_applier::get_last_result(){
	if (!this->lastResult) {
		return surface();
	}
	return this->lastResult->surface;
}

void filter_applier::visit(const svgdom::filter_element& e){
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
						percent_to_fraction(e.x),
						percent_to_fraction(e.y)
					};

					r4::vector2<real> fe_dims{
						percent_to_fraction(e.width),
						percent_to_fraction(e.height)
					};

					fr.p = this->r.device_space_bounding_box.p1 + fe_pos.comp_mul(this->r.device_space_bounding_box.dims());
					fr.d = fe_dims.comp_mul(this->r.device_space_bounding_box.dims());
				}
				break;
			case svgdom::coordinate_units::user_space_on_use:
				{
					auto p1 = this->r.length_to_px(e.x, e.y);
					auto p2 = p1 + this->r.length_to_px(e.width, e.height);

					std::array<r4::vector2<real>, 4> rectVertices = {{
						p1,
						p2,
						{p1.x(), p2.y()},
						{p2.x(), p1.y()}
					}};

					r4::segment2<real> frBb;
					frBb.set_empty_bounding_box();

					for(auto& vertex : rectVertices){
						vertex = this->r.canvas.matrix_mul(vertex);

						r4::segment2<real> bb;
						bb.p1.x() = decltype(bb.p1.x())(vertex.x());
						bb.p2.x() = decltype(bb.p2.x())(vertex.x());
						bb.p1.y() = decltype(bb.p1.y())(vertex.y());
						bb.p2.y() = decltype(bb.p2.y())(vertex.y());

						frBb.unite(bb);
					}
					fr.p = frBb.p1;
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

void filter_applier::visit(const svgdom::fe_gaussian_blur_element& e){
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
					this->r.user_space_bounding_box.d.comp_mul(sd)
				);
			break;
	}

	auto s = this->get_source(e.in).intersection(this->filterRegion);
	
	// TODO: set filter sub-region

	this->set_result(e.result, blur_surface(s, sd));
}

namespace{
filter_result color_matrix(const surface& s, const r4::matrix4<real>& m, const r4::vector4<real>& mc5){
//	TRACE(<< "colorMatrix(): s.width = " << s.width << " s.height = " << s.height << std::endl)
	ASSERT(!s.span.empty() || s.d.is_zero())
	filter_result ret = allocateResult(s);
	
	ASSERT(!s.span.empty() || s.d.is_zero())
	
	for(unsigned y = 0; y != s.d.y(); ++y){
		auto sp = &s.span[y * s.stride];
		ASSERT_INFO(sp < s.span.end(), "sp = " << std::hex << static_cast<const void*>(sp) << " s.end = " << static_cast<const void*>(s.span.end()))
		auto dp = &ret.surface.span[y * ret.surface.stride];
		for(unsigned x = 0; x != s.d.x(); ++x){
			auto cc = to_rgba(*sp);
			++sp;

			if(cc.a() != 0xff && cc.a() != 0){
				// unpremultiply alpha
				
				r4::vector3<unsigned> rgb{cc};
				rgb *= 0xff; // first multiply
				rgb /= cc.a(); // then divide
				cc = decltype(cc){rgb, cc.a()};
			}
			
			auto c = min(cc.to<real>() / 0xff, 1); // clamp top

			ASSERT_INFO(real(0) <= c.r() && c.r() <= real(1), "c = " << c << ", cc = " << cc)
			ASSERT_INFO(real(0) <= c.g() && c.g() <= real(1), "c = " << c << ", cc = " << cc)
			ASSERT_INFO(real(0) <= c.b() && c.b() <= real(1), "c = " << c << ", cc = " << cc)
			ASSERT_INFO(real(0) <= c.a() && c.a() <= real(1), "c = " << c << ", cc = " << cc)

			auto c1 = m * c + mc5;

			// alpha can change, so always premultiply alpha back
			c1.r() *= c1.a();
			c1.g() *= c1.a();
			c1.b() *= c1.a();
			
			*dp = to_pixel(min((c1 * 0xff).to<unsigned>(), 0xff)); // clamp top
			++dp;
		}
	}
	
	return ret;
}
}

void filter_applier::visit(const svgdom::fe_color_matrix_element& e){
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
	
	// TRACE(<< "color matrix getSource()" << std::endl)
	auto src = this->get_source(e.in);
	ASSERT(!src.span.empty() || src.d.is_zero())
	auto s = src.intersection(this->filterRegion);	
	
	// TODO: set filter sub-region
	
	this->set_result(e.result, color_matrix(s, m, mc5));
}

namespace{
filter_result blend(const surface& in, const surface& in2, svgdom::fe_blend_element::mode mode){
//	TRACE(<< "in.width = " << in.width << " in2.width = " << in2.width << std::endl)
	auto s1 = in.intersection(in2);
	auto s2 = in2.intersection(in);
	
	ASSERT_INFO(s1.d.x() == s2.d.x(), "s1.d.x() = " << s1.d.x() << " s2.d.x() = " << s2.d.x())
	ASSERT(s1.d.y() == s2.d.y())
	ASSERT(s1.p == s2.p)
	
	auto ret = allocateResult(s1);
	
	for(unsigned y = 0; y != ret.surface.d.y(); ++y){
		auto sp1 = &s1.span[y * s1.stride];
		auto sp2 = &s2.span[y * s2.stride];
		auto dp = &ret.surface.span[y * ret.surface.stride];
		for(unsigned x = 0; x != ret.surface.d.x(); ++x){
			// TODO: optimize by using integer arithmetics instead of floating point
			auto c01 = to_rgba(*sp1).to<real>() / 0xff;
			++sp1;
			auto c02 = to_rgba(*sp2).to<real>() / 0xff;
			++sp2;

			/*
				cr = Result color (RGB) - premultiplied 
				qr = Result opacity
				qa = Opacity value at a given pixel for image A 
				qb = Opacity value at a given pixel for image B 
				ca = Color (RGB) at a given pixel for image A - premultiplied 
				cb = Color (RGB) at a given pixel for image B - premultiplied 
			*/
			r4::vector3<real> cr;
			switch(mode){
				default:
					ASSERT(false)
					[[fallthrough]];
				case svgdom::fe_blend_element::mode::normal:
					// cr = (1 - qa) * cb + ca
					cr = c02 * (real(1) - c01.a()) + c01;
					break;
				case svgdom::fe_blend_element::mode::multiply:
					// cr = (1 - qa) * cb + (1 - qb) * ca + ca * cb
					cr = c02 * (1 - c01.a()) + c01 * (1 - c02.a()) + c01.comp_mul(c02);
					break;
				case svgdom::fe_blend_element::mode::screen:
					// cr = cb + ca - ca * cb
					cr = c02 + c01 - c01.comp_mul(c02);
					break;
				case svgdom::fe_blend_element::mode::darken:
					using std::min;
					// cr = min((1 - qa) * cb + ca, (1 - qb) * ca + cb)
					cr = min(c02 * (1 - c01.a()) + c01, c01 * (1 - c02.a()) + c02);
					break;
				case svgdom::fe_blend_element::mode::lighten:
					using std::max;
					// cr = max((1 - qa) * cb + ca, (1 - qb) * ca + cb)
					cr = max(c02 * (1 - c01.a()) + c01, c01 * (1 - c02.a()) + c02);
					break;
			}
			// qr = 1 - (1 - qa) * (1 - qb)
			auto qr = 1 - (1 - c01.a()) * (1 - c02.a());

			*dp = to_pixel((r4::vector4<real>{cr, qr} * 0xff).to<unsigned>());
			++dp;
		}
	}
	
	return ret;
}
}

void filter_applier::visit(const svgdom::fe_blend_element& e){
	auto s1 = this->get_source(e.in).intersection(this->filterRegion);
	if(s1.span.empty()){
		return;
	}
	
	auto s2 = this->get_source(e.in2).intersection(this->filterRegion);
	if(s2.span.empty()){
		return;
	}
	
	// TODO: set filter sub-region
	
	this->set_result(e.result, blend(s1, s2, e.mode_));
}

namespace{
filter_result composite(const surface& in, const surface& in2, const svgdom::fe_composite_element& e){
//	TRACE(<< "in.width = " << in.width << " in2.width = " << in2.width << std::endl)
	auto s1 = in.intersection(in2);
	auto s2 = in2.intersection(in);
	
	ASSERT_INFO(s1.d.x() == s2.d.x(), "s1.d.x() = " << s1.d.x() << " s2.d.x()) = " << s2.d.x())
	ASSERT(s1.d.y() == s2.d.y())
	ASSERT(s1.p == s2.p)
	
	auto ret = allocateResult(s1);
	
	for(unsigned y = 0; y != ret.surface.d.y(); ++y){
		auto sp1 = &s1.span[y * s1.stride];
		auto sp2 = &s2.span[y * s2.stride];
		auto dp = &ret.surface.span[y * ret.surface.stride];
		for(unsigned x = 0; x != ret.surface.d.x(); ++x){
			// TODO: optimize by using integer arithmetics instead of floating point
			auto c01 = to_rgba(*sp1).to<real>() / 0xff;
			++sp1;
			auto c02 = to_rgba(*sp2).to<real>() / 0xff;
			++sp2;
			
			r4::vector4<real> o;
			switch(e.operator__){
				case svgdom::fe_composite_element::operator_::over:
					// co = as * Cs + ab * Cb * (1 – as)
					// ao = as + ab * (1 – as)
					o = c01 + c02 * (1 - c01.a());
					break;
				case svgdom::fe_composite_element::operator_::in:
					// co = as * Cs * ab
					// ao = as x ab
					o = c01 * c02.a();
					break;
				case svgdom::fe_composite_element::operator_::out:
					// co = as * Cs * (1 – ab)
					// ao = as * (1 – ab)
					o = c01 * (1 - c02.a());
					break;
				case svgdom::fe_composite_element::operator_::atop:
					// co = as * Cs * ab + ab * Cb * (1 – as)
					// ao = as * ab + ab * (1 – as)
					o = c01 * c02.a() + c02 * (1 - c01.a());
					break;
				case svgdom::fe_composite_element::operator_::xor_:
					// co = as * Cs * (1 - ab) + ab * Cb * (1 – as)
					// ao = as * (1 - ab) + ab * (1 – as)
					o = c01 * (1 - c02.a()) + c02 * (1 - c01.a());
					break;
				case svgdom::fe_composite_element::operator_::arithmetic:
					using std::min;
					// result = k1 * i1 * i2 + k2 * i1 + k3 * i2 + k4
					o = min(c01.comp_mul(c02) * real(e.k1) + c01 * real(e.k2) + c02 * real(e.k3) + real(e.k4), 1);
					break;
				default:
					ASSERT(false)
					break;
			}

			*dp = to_pixel((o * 0xff).to<unsigned>());
			++dp;
		}
	}
	
	return ret;
}
}

void filter_applier::visit(const svgdom::fe_composite_element& e){
	auto s1 = this->get_source(e.in).intersection(this->filterRegion);
	if(s1.span.empty()){
		return;
	}
	
	auto s2 = this->get_source(e.in2).intersection(this->filterRegion);
	if(s2.span.empty()){
		return;
	}
	
	// TODO: set filter sub-region
	
	this->set_result(e.result, composite(s1, s2, e));
}
