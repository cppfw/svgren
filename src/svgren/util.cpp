/*
The MIT License (MIT)

Copyright (c) 2015-2023 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* ================ LICENSE END ================ */

#include "util.hxx"

#include <cstring>
#include <ratio>
#include <stdexcept>
#include <vector>

#include <svgdom/length.hpp>
#include <utki/debug.hpp>
#include <utki/math.hpp>

#include "renderer.hxx"

using namespace svgren;

real svgren::percent_to_fraction(const svgdom::length& l)
{
	if (l.is_percent()) {
		return l.value / real(std::centi::den);
	}
	if (l.unit == svgdom::length_unit::number) {
		return l.value;
	}
	return 0;
}

renderer_viewport_push::renderer_viewport_push(renderer& r, const decltype(old_viewport) & viewport) :
	r(r),
	old_viewport(r.viewport)
{
	this->r.viewport = viewport;
}

renderer_viewport_push::~renderer_viewport_push() noexcept
{
	this->r.viewport = this->old_viewport;
}

common_element_push::common_element_push(svgren::renderer& renderer, bool is_container) :
	renderer(renderer),
	matrix_push(this->renderer.canvas),
	old_device_space_bounding_box(renderer.device_space_bounding_box)
{
	// old device space bounding box is saved, set current one to empty
	this->renderer.device_space_bounding_box.set_empty_bounding_box();

	auto background_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::enable_background);

	if (background_prop && std::holds_alternative<svgdom::enable_background_property>(*background_prop) &&
		std::get_if<svgdom::enable_background_property>(background_prop)->value ==
			svgdom::enable_background::new_background)
	{
		this->old_background = this->renderer.background;
	}

	auto filter_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::filter);

	if (auto mask_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::mask)) {
		if (std::holds_alternative<std::string>(*mask_prop)) {
			if (auto ei = this->renderer.finder_by_id.find(svgdom::get_local_id_from_iri(*mask_prop))) {
				this->mask_element = ei;
			}
		}
	}

	this->group_pushed = filter_prop || this->mask_element || !this->old_background.span.empty();

	auto opacity = svgdom::real(1);
	{
		auto stroke_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::stroke);
		auto fill_prop = this->renderer.style_stack.get_style_property(svgdom::style_property::fill);

		// OPTIMIZATION: if opacity is set on an element then push cairo group only in case it is a container element,
		// like 'g' or 'svg',
		//               or in case the fill or stroke is a non-solid color, like gradient or pattern,
		//               or both fill and stroke are non-none.
		//               If element is non-container and one of stroke or fill is solid color and other one is none,
		//               then opacity will be applied later without pushing cairo group.
		if (this->group_pushed || is_container || (stroke_prop && std::holds_alternative<std::string>(*stroke_prop)) ||
			(fill_prop && std::holds_alternative<std::string>(*fill_prop)) ||
			(fill_prop && stroke_prop && !svgdom::is_none(*fill_prop) && !svgdom::is_none(*stroke_prop)))
		{
			auto p = this->renderer.style_stack.get_style_property(svgdom::style_property::opacity);
			if (p && std::holds_alternative<svgdom::real>(*p)) {
				opacity = *std::get_if<svgdom::real>(p);
				this->group_pushed = this->group_pushed || opacity < 1;
			}
		}
	}

	if (this->group_pushed) {
		//		TRACE(<< "setting temp context" << std::endl)
		this->renderer.canvas.push_group();

		this->opacity = opacity;
	}

	if (!this->old_background.span.empty()) {
		this->renderer.background = this->renderer.canvas.get_sub_surface();
	}
}

common_element_push::~common_element_push() noexcept
{
	// restore device space bounding box
	this->old_device_space_bounding_box.unite(this->renderer.device_space_bounding_box);
	this->renderer.device_space_bounding_box = this->old_device_space_bounding_box;

	if (!this->group_pushed) {
		return;
	}

	if (this->mask_element) {
		// render mask
		try {
			this->renderer.canvas.push_group();

			utki::scope_exit scope_exit([this]() {
				this->renderer.canvas.pop_group(1);
			});

			// TODO: setup the correct coordinate system based on maskContentUnits value
			// (userSpaceOnUse/objectBoundingBox)
			//       Currently nothing on that is done which is equivalent to userSpaceOnUse

			class mask_renderer : public svgdom::const_visitor
			{
				svgren::renderer& r;

			public:
				mask_renderer(svgren::renderer& r) :
					r(r)
				{}

				void visit(const svgdom::mask_element& e) override
				{
					svgdom::style_stack::push push_styles(this->r.style_stack, e);

					this->r.relay_accept(e);
				}
			} mr(this->renderer);

			this->mask_element->accept(mr);

			scope_exit.release();

			this->renderer.canvas.pop_mask_and_group();
			// NOLINTNEXTLINE(bugprone-empty-catch)
		} catch (...) {
			// rendering mask failed, just ignore it
		}
	} else {
		this->renderer.canvas.pop_group(this->opacity);
	}

	// restore background if it was pushed
	if (!this->old_background.span.empty()) {
		this->renderer.background = this->old_background;
	}
}
