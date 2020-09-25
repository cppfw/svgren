#pragma once

#include <cstdint>

#include <r4/rectangle.hpp>

namespace svgren{

struct surface : public r4::rectangle<unsigned>{
	uint8_t* data = nullptr; // RGBA data
	uint8_t* end = nullptr;
	unsigned stride = 0;

	surface() :
			r4::rectangle<unsigned>{0, std::numeric_limits<unsigned>::max()}
	{}

	surface intersection(const r4::rectangle<unsigned>& r)const;
};

}
