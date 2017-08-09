#include "util.hxx"

#include <cstring>

#include <utki/debug.hpp>
#include <vector>

using namespace svgren;


namespace{

}



void svgren::cairoImageSurfaceBlur(cairo_surface_t* surface, std::array<real, 2> stdDeviation){
	if(cairo_image_surface_get_format (surface) != CAIRO_FORMAT_ARGB32){
		TRACE(<< "cairo_image_surface_blur(): ERROR: wrong surface format, only ARGB32 is supported." << std::endl)
		return;
	}

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	
	
	double radius = stdDeviation[0];
	
	// Steve Hanov, 2009
	// Released into the public domain.
	
	std::vector<std::uint8_t> destination(width * height * 4);
	std::vector<unsigned> precalculate(width * height * sizeof(unsigned));
	
	unsigned char *src = cairo_image_surface_get_data(surface);
	
	double mul = 1.0f / ((radius * 2) * (radius * 2));

	// The number of times to perform the averaging. According to wikipedia,
	// three iterations is good enough to pass for a gaussian.
	const unsigned MAX_ITERATIONS = 3;

	auto dst = &*destination.begin();
	auto precalc = &*precalculate.begin();
	memcpy(dst, src, width * height * 4);

	for(unsigned iteration = 0; iteration < MAX_ITERATIONS; ++iteration){
		for(unsigned channel = 0; channel < 4; channel++){
			double x, y;

			// Pre-computation step.
			unsigned char *pix = src;
			unsigned *pre = precalc;

			pix += channel;
			for (y = 0; y < height; y++) {
				for (x = 0; x < width; x++) {
					int tot = pix[0];
					if (x > 0)
						tot += pre[-1];
					if (y > 0)
						tot += pre[-width];
					if (x > 0 && y > 0)
						tot -= pre[-width - 1];
					*pre++ = tot;
					pix += 4;
				}
			}

			// Blur step.
			pix = dst + (int)radius * width * 4 + (int)radius * 4 + channel;
			for (y = radius; y < height - radius; y++) {
				for (x = radius; x < width - radius; x++) {
					double l = x < radius ? 0 : x - radius;
					double t = y < radius ? 0 : y - radius;
					double r = x + radius >= width ? width - 1 : x + radius;
					double b = y + radius >= height ? height - 1 : y + radius;
					double tot = precalc[(int)(r + b * width)] + precalc[(int)(l + t * width)] - precalc[(int)(l + b * width)] -
					precalc[(int)(r + t * width)];
					*pix = (unsigned char)(tot * mul);
					pix += 4;
				}
				pix += (int)radius * 2 * 4;
			}
		}
		memcpy(src, dst, width * height * 4);
	}
}
