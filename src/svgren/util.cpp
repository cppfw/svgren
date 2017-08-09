#include "util.hxx"

#include <cstring>
#include <cmath>
#include <vector>

#include <utki/debug.hpp>
#include <utki/math.hpp>

using namespace svgren;

namespace{
void boxBlurHorizontal(
		std::uint8_t* dst,
		const std::uint8_t* src,
		unsigned stride,
		unsigned numRows,
		unsigned boxSize,
		unsigned boxOffset,
		unsigned channel
	)
{
	for(unsigned y = 0; y != numRows; ++y){
		unsigned sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = std::max(pos, 0);
			pos = std::min(pos, int(stride - 1));
			sum += src[(stride * y + pos) * sizeof(std::uint32_t) + channel];
		}
		for(unsigned x = 0; x != stride; ++x){
			int tmp = x - boxOffset;
			int last = std::max(tmp, 0);
			int next = std::min(tmp + boxSize, stride - 1);

			dst[(stride * y + x) * sizeof(std::uint32_t) + channel] = sum / boxSize;

			sum += src[(stride * y + next) * sizeof(std::uint32_t) + channel]
					- src[(stride * y + last) * sizeof(std::uint32_t) + channel];
		}
	}
}
}

namespace{
void boxBlurVertical(
		std::uint8_t* dst,
		const std::uint8_t* src,
		unsigned stride,
		unsigned numRows,
		unsigned boxSize,
		unsigned boxOffset,
		unsigned channel
	)
{
	for(unsigned x = 0; x != stride; ++x){
		unsigned sum = 0;
		for(unsigned i = 0; i != boxSize; ++i){
			int pos = i - boxOffset;
			pos = std::max(pos, 0);
			pos = std::min(pos, int(numRows - 1));
			sum += src[(stride * pos + x) * sizeof(std::uint32_t) + channel];
		}
		for(unsigned y = 0; y != numRows; ++y){
			int tmp = y - boxOffset;
			int last = std::max(tmp, 0);
			int next = std::min(tmp + boxSize, numRows - 1);

			dst[(stride * y + x) * sizeof(std::uint32_t) + channel] = sum / boxSize;

			sum += src[(x + stride * next) * sizeof(std::uint32_t) + channel]
					- src[(x + stride * last) * sizeof(std::uint32_t) + channel];
		}
	}
}
}



void svgren::cairoImageSurfaceBlur(cairo_surface_t* surface, std::array<real, 2> stdDeviation){
	if(cairo_image_surface_get_format (surface) != CAIRO_FORMAT_ARGB32){
		TRACE(<< "cairo_image_surface_blur(): ERROR: wrong surface format, only ARGB32 is supported." << std::endl)
		return;
	}

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	
	//NOTE: see https://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for Gaussian Blur approximation algorithm.
	
	std::array<unsigned, 2> d;
	for(unsigned i = 0; i != 2; ++i){
		d[i] = unsigned(float(stdDeviation[i]) * 3 * std::sqrt(2 * utki::pi<float>()) / 4 + 0.5f);
	}
	
	std::uint8_t* src = cairo_image_surface_get_data(surface);
	
	std::vector<std::uint8_t> tmp(width * height * sizeof(std::uint32_t));
	
	std::array<unsigned, 3> hBoxSize;
	std::array<unsigned, 3> hOffset;
	std::array<unsigned, 3> vBoxSize;
	std::array<unsigned, 3> vOffset;
	if(d[0] % 2 == 0){
		hOffset[0] = d[0] / 2;
		hBoxSize[0] = d[0];
		hOffset[1] = d[0] / 2 + 1;
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
		vOffset[0] = d[0] / 2;
		vBoxSize[0] = d[0];
		vOffset[1] = d[0] / 2 + 1;
		vBoxSize[1] = d[0];
		vOffset[2] = d[0] / 2;
		vBoxSize[2] = d[0] + 1;
	}else{
		vOffset[0] = d[0] / 2;
		vBoxSize[0] = d[0];
		vOffset[1] = d[0] / 2;
		vBoxSize[1] = d[0];
		vOffset[2] = d[0] / 2;
		vBoxSize[2] = d[0];
	}
	
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(&*tmp.begin(), src, width, height, hBoxSize[0], hOffset[0], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(src, &*tmp.begin(), width, height, hBoxSize[1], hOffset[1], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurHorizontal(&*tmp.begin(), src, width, height, hBoxSize[2], hOffset[2], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(src, &*tmp.begin(), width, height, vBoxSize[0], vOffset[0], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(&*tmp.begin(), src, width, height, vBoxSize[1], vOffset[1], channel);
	}
	for(auto channel = 0; channel != 4; ++channel){
		boxBlurVertical(src, &*tmp.begin(), width, height, vBoxSize[2], vOffset[2], channel);
	}
}
