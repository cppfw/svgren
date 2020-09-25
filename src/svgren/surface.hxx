#pragma once

#include <cstdint>

#include <utki/span.hpp>

#include <r4/rectangle.hpp>

namespace svgren{

struct surface : public r4::rectangle<unsigned>{
	utki::span<uint8_t> span; // RGBA data
	unsigned stride = 0;

	surface() :
			r4::rectangle<unsigned>{0, std::numeric_limits<unsigned>::max()}
	{}

	surface intersection(const r4::rectangle<unsigned>& r)const;
};

}
