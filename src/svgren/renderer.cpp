/*
The MIT License (MIT)

Copyright (c) 2015-2021 Ivan Gagis <igagis@gmail.com>

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

#include "renderer.hxx"

#include <utki/math.hpp>

#include <svgdom/elements/coordinate_units.hpp>

#include "util.hxx"
#include "config.hxx"

#include "filter_applier.hxx"

using namespace svgren;

namespace{
const std::string fake_svg_element_tag = "fake_svg_element";
}

real renderer::length_to_px(const svgdom::length& l)const noexcept{
	if(l.is_percent()){
		return this->viewport.x() * (l.value / 100);
	}
	return real(l.to_px(this->dpi));
}

r4::vector2<real> renderer::length_to_px(const svgdom::length& x, const svgdom::length& y)const noexcept{
	return r4::vector2<real>{
			x.is_percent() ? (this->viewport.x() * (x.value / 100)) : x.to_px(this->dpi),
			y.is_percent() ? (this->viewport.y() * (y.value / 100)) : y.to_px(this->dpi)
		};
}

void renderer::apply_transformation(const svgdom::transformable::transformation& t){
//	TRACE(<< "renderer::applyCairoTransformation(): applying transformation " << unsigned(t.type) << std::endl)
	switch (t.type_) {
		case svgdom::transformable::transformation::type::translate:
//			TRACE(<< "translate x,y = (" << t.x << ", " << t.y << ")" << std::endl)
			this->canvas.translate(t.x, t.y);
			break;
		case svgdom::transformable::transformation::type::matrix:
			this->canvas.transform({
					{t.a, t.c, t.e},
					{t.b, t.d, t.f}
				});
			break;
		case svgdom::transformable::transformation::type::scale:
//			TRACE(<< "scale transformation factors = (" << t.x << ", " << t.y << ")" << std::endl)
			this->canvas.scale(t.x, t.y);
			break;
		case svgdom::transformable::transformation::type::rotate:
			this->canvas.translate(t.x, t.y);
			this->canvas.rotate(deg_to_rad(t.angle));
			this->canvas.translate(-t.x, -t.y);
			break;
		case svgdom::transformable::transformation::type::skewx:
			{
				using std::tan;
				this->canvas.transform({
						{ 1, tan(deg_to_rad(t.angle)), 0 },
						{ 0, 1,                        0 }
					});
			}
			break;
		case svgdom::transformable::transformation::type::skewy:
			{
				using std::tan;
				this->canvas.transform({
						{ 1,                        0, 0 },
						{ tan(deg_to_rad(t.angle)), 1, 0 }
					});
			}
			break;
		default:
			ASSERT(false)
			break;
	}

#if SVGREN_BACKEND == SVGREN_BACKEND_CAIRO
	// WORKAROUND: Due to cairo/pixman bug https://bugs.freedesktop.org/show_bug.cgi?id=102966
	//             we have to limit the maximum value of matrix element by 16 bit integer (+-0x7fff).
	auto m = this->canvas.get_matrix();
	for(auto& r : m){
		using std::min;
		using std::max;
		const real max_value = 0x7fff;
		r = max(-max_value, min(r, max_value));
	}
	this->canvas.set_matrix(m);
#endif
}

void renderer::apply_transformations(const decltype(svgdom::transformable::transformations)& transformations){
	for(auto& t : transformations){
		this->apply_transformation(t);
	}
}

void renderer::apply_viewbox(const svgdom::view_boxed& e, const svgdom::aspect_ratioed& ar){
	// TRACE(<< "vb = " << e.view_box[0] << ", " << e.view_box[1] << ", " << e.view_box[2] << ", " << e.view_box[3] << std::endl)
	if(!e.is_view_box_specified()){
		return;
	}

	if(ar.preserve_aspect_ratio.preserve != svgdom::aspect_ratioed::aspect_ratio_preservation::none){
		if(e.view_box[3] >= 0 && this->viewport[1] >= 0){ // if viewBox width and viewport width are not 0
			real scaleFactor, dx, dy;

			real viewBoxAspect = e.view_box[2] / e.view_box[3];
			real viewportAspect = this->viewport[0] / this->viewport[1];

			if((viewBoxAspect >= viewportAspect && ar.preserve_aspect_ratio.slice) || (viewBoxAspect < viewportAspect && !ar.preserve_aspect_ratio.slice)){
				// fit by Y
				scaleFactor = this->viewport[1] / e.view_box[3];
				dx = e.view_box[2] - this->viewport[0];
				dy = 0;
			}else{ // viewBoxAspect < viewportAspect
				// fit by X
				scaleFactor = this->viewport[0] / e.view_box[2];
				dx = 0;
				dy = e.view_box[3] - this->viewport[1];
			}
			switch(ar.preserve_aspect_ratio.preserve){
				case svgdom::aspect_ratioed::aspect_ratio_preservation::none:
					ASSERT(false)
				default:
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_min_y_max:
					this->canvas.translate(0, dy);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_min_y_mid:
					this->canvas.translate(0, dy / 2);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_min_y_min:
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_mid_y_max:
					this->canvas.translate(dx / 2, dy);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_mid_y_mid:
					this->canvas.translate(dx / 2, dy / 2);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_mid_y_min:
					this->canvas.translate(dx / 2, 0);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_max_y_max:
					this->canvas.translate(dx, dy);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_max_y_mid:
					this->canvas.translate(dx, dy / 2);
					break;
				case svgdom::aspect_ratioed::aspect_ratio_preservation::x_max_y_min:
					this->canvas.translate(dx, 0);
					break;
			}

			this->canvas.scale(scaleFactor, scaleFactor);
		}
	}else{ // if no preserveAspectRatio enforced
		if(e.view_box[2] != 0 && e.view_box[3] != 0){ // if viewBox width and height are not 0
			this->canvas.scale(this->viewport.comp_div({e.view_box[2], e.view_box[3]}));
		}
	}
	this->canvas.translate(-e.view_box[0], -e.view_box[1]);
}

void renderer::set_gradient_properties(canvas::gradient& gradient, const svgdom::gradient& g, const svgdom::style_stack& ss){
	// Gradient inherits all attributes from other gradients it refers via href.
	// Here we need to make sure that the gradient inherits 'styles', 'class' and all possible presentation attributes.
	// For that we need to replace the gradient element in the style stack with the one which has those inherited
	// attributes substituted. The currently handled gradient element is at the top of the style stack.

	svgdom::style_stack gradient_ss(ss); // copy style stack, so that we are able to modify it

	struct dummy_styleable : public svgdom::styleable{
		const svgdom::gradient& g;

		dummy_styleable(const svgdom::gradient& g) : g(g) {}

		const std::string& get_id()const override{
			return this->g.get_id();
		}

		const std::string& get_tag()const override{
			return static_cast<const svgdom::element&>(this->g).get_tag();
		}
	} effective_gradient_styleable(g);

	effective_gradient_styleable.styles = this->gradient_get_styles(g);
	effective_gradient_styleable.classes = this->gradient_get_classes(g);
	effective_gradient_styleable.presentation_attributes = this->gradient_get_presentation_attributes(g);

	ASSERT(!gradient_ss.stack.empty())
	gradient_ss.stack.pop_back();
	gradient_ss.stack.push_back(effective_gradient_styleable);

	struct gradient_stops_adder : public svgdom::const_visitor{
		std::vector<canvas::gradient::stop> stops;
		svgdom::style_stack& ss;

		gradient_stops_adder(svgdom::style_stack& ss) :
				ss(ss)
		{}

		void visit(const svgdom::gradient::stop_element& stop)override{
			svgdom::style_stack::push stylePush(this->ss, stop);

			r4::vector3<real> rgb;
			if(auto p = this->ss.get_style_property(svgdom::style_property::stop_color)){
				rgb = svgdom::get_rgb(*p).to<real>();
			}else{
				rgb.set(0);
			}

			svgdom::real opacity = 1;
			if(auto p = this->ss.get_style_property(svgdom::style_property::stop_opacity)){
				if(std::holds_alternative<svgdom::real>(*p)){
					opacity = real(*std::get_if<svgdom::real>(p));
				}
			}
			this->stops.push_back(canvas::gradient::stop{
					{rgb, opacity},
					real(stop.offset)
				});
		}
	} visitor(gradient_ss);

	for(auto& stop : this->gradient_get_stops(g)){
		stop->accept(visitor);
	}

	gradient.set_stops(utki::make_span(visitor.stops));
	gradient.set_spread_method(this->gradient_get_spread_method(g));
}

void renderer::apply_filter(){
	if(auto filter = this->style_stack.get_style_property(svgdom::style_property::filter)){
		if(std::holds_alternative<std::string>(*filter)){
			this->apply_filter(svgdom::get_local_id_from_iri(*filter));
		}
	}
}

void renderer::apply_filter(const std::string& id){
	auto e = this->finder_by_id.find(id);
	if(!e){
		return;
	}

	filter_applier visitor(*this);

	ASSERT(e)
	e->accept(visitor);

	this->blit(visitor.get_last_result());
}

void renderer::set_gradient(const std::string& id){
	auto ss = this->style_stack_cache.find(id);
	auto e = this->finder_by_id.find(id);
	ASSERT((ss && e) || !ss)
	if(!ss){
		this->canvas.set_source(r4::vector4<real>(0));
		return;
	}
	ASSERT(e)

	struct common_gradient_push{
		canvas_matrix_push matrix_push;

		std::unique_ptr<renderer_viewport_push> viewport_push;

		common_gradient_push(renderer& r, const svgdom::gradient& gradient) :
				matrix_push(r.canvas)
		{
			if(r.gradient_get_units(gradient) == svgdom::coordinate_units::object_bounding_box){
				r.canvas.translate(r.user_space_bounding_box.p);

				// apply scale only if bounding box dimensions are not zero to avoid non-invertible matrix
				if(r.user_space_bounding_box.d.is_positive()){
					r.canvas.scale(r.user_space_bounding_box.d);
				}

				ASSERT(r.canvas.get_matrix().det() != 0, [&](auto&o){o << "matrix =\n" << r.canvas.get_matrix();})
				this->viewport_push = std::make_unique<renderer_viewport_push>(r, real(1));
			}

			r.apply_transformations(r.gradient_get_transformations(gradient));
		}

		~common_gradient_push()noexcept{}
	};

	struct gradient_setter : public svgdom::const_visitor{
		renderer& r;

		const svgdom::style_stack& ss;

		gradient_setter(renderer& r, const svgdom::style_stack& ss) :
				r(r),
				ss(ss)
		{}

		void visit(const svgdom::linear_gradient_element& gradient)override{
			common_gradient_push commonPush(this->r, gradient);

			auto g = std::make_shared<canvas::linear_gradient>(
					this->r.length_to_px(
							this->r.gradient_get_x1(gradient),
							this->r.gradient_get_y1(gradient)
						),
					this->r.length_to_px(
							this->r.gradient_get_x2(gradient),
							this->r.gradient_get_y2(gradient)
						)
				);
			
			this->r.set_gradient_properties(*g, gradient, this->ss);

			this->r.canvas.set_source(g);
		}

		void visit(const svgdom::radial_gradient_element& gradient)override{
			common_gradient_push commonPush(this->r, gradient);

			auto cx = this->r.gradient_get_cx(gradient);
			auto cy = this->r.gradient_get_cy(gradient);
			auto radius = this->r.gradient_get_r(gradient);
			auto fx = this->r.gradient_get_fx(gradient);
			auto fy = this->r.gradient_get_fy(gradient);

			if(!fx.is_valid()){
				fx = cx;
			}
			if(!fy.is_valid()){
				fy = cy;
			}

			auto g = std::make_shared<canvas::radial_gradient>(
					this->r.length_to_px(fx, fy),
					this->r.length_to_px(cx, cy),
					this->r.length_to_px(radius)
				);

			this->r.set_gradient_properties(*g, gradient, this->ss);

			this->r.canvas.set_source(g);
		}

		void default_visit(const svgdom::element&)override{
			this->r.canvas.set_source(r4::vector4<real>{0});
		}
	} visitor(*this, *ss);

	e->accept(visitor);
}

void renderer::update_bounding_box(){
	this->user_space_bounding_box = this->canvas.get_shape_bounding_box();

	// TRACE(<< "bb = " << this->user_space_bounding_box << std::endl)

	if(this->user_space_bounding_box.d[0] == 0){
		// empty path
		return;
	}

	// set device space bounding box
	std::array<r4::vector2<real>, 4> rect_vertices = {{
		this->user_space_bounding_box.p,
		this->user_space_bounding_box.x2_y2(),
		this->user_space_bounding_box.x1_y2(),
		this->user_space_bounding_box.x2_y1()
	}};

	for(auto& vertex : rect_vertices){
		vertex = this->canvas.matrix_mul(vertex);

		r4::segment2<real> bb;
		bb.p1.x() = decltype(bb.p1.x())(vertex.x());
		bb.p2.x() = decltype(bb.p2.x())(vertex.x());
		bb.p1.y() = decltype(bb.p1.y())(vertex.y());
		bb.p2.y() = decltype(bb.p2.y())(vertex.y());

		this->device_space_bounding_box.unite(bb);
	}
}

void renderer::render_shape(bool isCairoGroupPushed){
	this->update_bounding_box();

	{
		auto p = this->style_stack.get_style_property(svgdom::style_property::fill_rule);
		if(p && std::holds_alternative<svgdom::fill_rule>(*p)){
			this->canvas.set_fill_rule(*std::get_if<svgdom::fill_rule>(p));
		}else{
			this->canvas.set_fill_rule(svgdom::fill_rule::nonzero);
		}
	}

	svgdom::style_value blackFill;

	auto fill = this->style_stack.get_style_property(svgdom::style_property::fill);
	if (!fill) {
		blackFill = svgdom::parse_paint("black");
		fill = &blackFill;
	}

	auto stroke = this->style_stack.get_style_property(svgdom::style_property::stroke);

	// OPTIMIZATION: in case there is 'opacity' style property and only one of
	//               'stroke' or 'fill' is not none and is a solid color (not pattern/gradient),
	//               then there is no need to push cairo group, but just multiply
	//               the 'stroke-opacity' or 'fill-opacity' by 'opacity' value.
	auto opacity = svgdom::real(1);
	if(!isCairoGroupPushed){
		auto p = this->style_stack.get_style_property(svgdom::style_property::opacity);
		if(p && std::holds_alternative<svgdom::real>(*p)){
			opacity = *std::get_if<svgdom::real>(p);
		}
	}

	ASSERT(fill)
	if(!svgdom::is_none(*fill)){
		if(std::holds_alternative<std::string>(*fill)){
			this->set_gradient(svgdom::get_local_id_from_iri(*fill));
		}else{
			svgdom::real fillOpacity = 1;
			auto p = this->style_stack.get_style_property(svgdom::style_property::fill_opacity);
			if(p && std::holds_alternative<svgdom::real>(*p)){
				fillOpacity = *std::get_if<svgdom::real>(p);
			}

			auto fillRgb = svgdom::get_rgb(*fill).to<real>();
			this->canvas.set_source(r4::vector4<real>{fillRgb, fillOpacity * opacity});
		}

		this->canvas.fill();
	}

	if(stroke && !svgdom::is_none(*stroke)){
		{
			auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_width);
			if(p && std::holds_alternative<svgdom::length>(*p)){
				this->canvas.set_line_width(this->length_to_px(*std::get_if<svgdom::length>(p)));
			}else{
				this->canvas.set_line_width(1);
			}
		}

		{
			auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_linecap);
			if(p && std::holds_alternative<svgdom::stroke_line_cap>(*p)){
				this->canvas.set_line_cap(*std::get_if<svgdom::stroke_line_cap>(p));
			}else{
				this->canvas.set_line_cap(svgdom::stroke_line_cap::butt);
			}
		}

		{
			auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_linejoin);
			if(p && std::holds_alternative<svgdom::stroke_line_join>(*p)){
				this->canvas.set_line_join(*std::get_if<svgdom::stroke_line_join>(p));
			}else{
				this->canvas.set_line_join(svgdom::stroke_line_join::miter);
			}
		}

		{
			auto dasharray_prop = this->style_stack.get_style_property(svgdom::style_property::stroke_dasharray);
			if(dasharray_prop && std::holds_alternative<std::vector<svgdom::length>>(*dasharray_prop)){
				auto dashoffset_prop = this->style_stack.get_style_property(svgdom::style_property::stroke_dashoffset);
				real dashoffset = 0;
				if(dashoffset_prop && std::holds_alternative<svgdom::length>(*dashoffset_prop)){
					dashoffset = this->length_to_px(*std::get_if<svgdom::length>(dashoffset_prop));
				}

				const auto& lenarr = *std::get_if<std::vector<svgdom::length>>(dasharray_prop);

				// convert lengthes to pixels
				std::vector<real> dasharray(lenarr.size());
				auto dst = dasharray.begin();
				for(auto src = lenarr.begin(); src != lenarr.end(); ++src, ++dst){
					ASSERT(dst != dasharray.end())
					*dst = this->length_to_px(*src);
				}
				ASSERT(dst == dasharray.end())

				this->canvas.set_dash_pattern(utki::make_span(dasharray), dashoffset);
			}else{
				this->canvas.set_dash_pattern(nullptr, 0); // no dashing
			}
		}

		ASSERT(stroke)
		if(std::holds_alternative<std::string>(*stroke)){
			this->set_gradient(svgdom::get_local_id_from_iri(*std::get_if<std::string>(stroke)));
		}else{
			svgdom::real strokeOpacity = 1;
			auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_opacity);
			if(p && std::holds_alternative<svgdom::real>(*p)){
				strokeOpacity = *std::get_if<svgdom::real>(p);
			}

			auto rgb = svgdom::get_rgb(*stroke).to<real>();
			this->canvas.set_source(r4::vector4<real>{rgb, strokeOpacity * opacity});
		}

		this->canvas.stroke();
	}

	// clear path if any left
	this->canvas.clear_path();
	
	this->apply_filter();
}

void renderer::render_svg_element(
		const svgdom::container& e,
		const svgdom::styleable& s,
		const svgdom::view_boxed& v,
		const svgdom::aspect_ratioed& a,
		const svgdom::length& x,
		const svgdom::length& y,
		const svgdom::length& width,
		const svgdom::length& height
	)
{
	svgdom::style_stack::push pushStyles(this->style_stack, s);

	if(this->is_group_invisible()){
		return;
	}

	common_element_push group_push(*this, true);

	if(!this->is_outermost_element){
		this->canvas.translate(this->length_to_px(x, y));
	}

	renderer_viewport_push viewport_push(*this, this->length_to_px(width, height));

	this->apply_viewbox(v, a);

	{
		bool oldOutermostElementFlag = this->is_outermost_element;
		this->is_outermost_element = false;
		utki::scope_exit scope_exit([oldOutermostElementFlag, this](){
			this->is_outermost_element = oldOutermostElementFlag;
		});

		this->relay_accept(e);
	}

	this->apply_filter();
}

renderer::renderer(
		svgren::canvas& canvas,
		unsigned dpi,
		r4::vector2<real> viewport,
		const svgdom::svg_element& root
	) :
		canvas(canvas),
		finder_by_id(root),
		style_stack_cache(root),
		dpi(real(dpi)),
		viewport(viewport)
{
	this->device_space_bounding_box.set_empty_bounding_box();
	this->background = this->canvas.get_sub_surface();

#ifdef SVGREN_BACKGROUND
	this->canvas.set_source(
		r4::vector4<real>{
			unsigned((SVGREN_BACKGROUND >> 0) & 0xff),
			unsigned((SVGREN_BACKGROUND >> 8) & 0xff),
			unsigned((SVGREN_BACKGROUND >> 16) & 0xff),
			unsigned((SVGREN_BACKGROUND >> 24) & 0xff)
		} / 0xff
	);
	this->canvas.rectangle({0, viewport});
	this->canvas.fill();
	this->canvas.clear_path();
	this->canvas.set_source({0, 0, 0, 0});
#endif
}

void renderer::visit(const svgdom::g_element& e){
//	TRACE(<< "rendering GElement: id = " << e.id << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_group_invisible()){
		return;
	}

	common_element_push group_push(*this, true);

	this->apply_transformations(e.transformations);

	this->relay_accept(e);

	this->apply_filter();
}

void renderer::visit(const svgdom::use_element& e){
//	TRACE(<< "rendering UseElement" << std::endl)
	auto ref = this->finder_by_id.find(e.get_local_id_from_iri());
	if(!ref){
		return;
	}

	struct ref_renderer : public svgdom::const_visitor{
		renderer& r;
		const svgdom::use_element& ue;
		svgdom::g_element fake_g_element;

		ref_renderer(renderer& r, const svgdom::use_element& e) :
				r(r), ue(e)
		{
			this->fake_g_element.styles = e.styles;
			this->fake_g_element.presentation_attributes = e.presentation_attributes;
			this->fake_g_element.transformations = e.transformations;

			// add x and y transformation
			{
				svgdom::transformable::transformation t;
				t.type_ = svgdom::transformable::transformation::type::translate;
				auto p = this->r.length_to_px(e.x, e.y);
				t.x = p.x();
				t.y = p.y();

				this->fake_g_element.transformations.push_back(t);
			}
		}

		void visit(const svgdom::symbol_element& symbol)override{
			struct fake_svg_element : public svgdom::element{
				renderer& r;
				const svgdom::use_element& ue;
				const svgdom::symbol_element& se;

				fake_svg_element(renderer& r, const svgdom::use_element& ue, const svgdom::symbol_element& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor)const override{
					const auto hundred_percent = svgdom::length(100, svgdom::length_unit::percent);

					this->r.render_svg_element(
							this->se,
							this->se,
							this->se,
							this->se,
							svgdom::length(0),
							svgdom::length(0),
							this->ue.width.is_valid() ? this->ue.width : hundred_percent,
							this->ue.height.is_valid() ? this->ue.height : hundred_percent
						);
				}

				const std::string& get_tag()const override{
					return fake_svg_element_tag;
				}
			};

			this->fake_g_element.children.push_back(std::make_unique<fake_svg_element>(this->r, this->ue, symbol));
			this->fake_g_element.accept(this->r);
		}

		void visit(const svgdom::svg_element& svg)override{
			struct fake_svg_element : public svgdom::element{
				renderer& r;
				const svgdom::use_element& ue;
				const svgdom::svg_element& se;

				fake_svg_element(renderer& r, const svgdom::use_element& ue, const svgdom::svg_element& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					// width and height of <use> element override those of <svg> element.
					this->r.render_svg_element(
							this->se,
							this->se,
							this->se,
							this->se,
							this->se.x,
							this->se.y,
							this->ue.width.is_valid() ? this->ue.width : this->se.width,
							this->ue.height.is_valid() ? this->ue.height : this->se.height
						);
				}

				const std::string& get_tag()const override{
					return fake_svg_element_tag;
				}
			};

			this->fake_g_element.children.push_back(std::make_unique<fake_svg_element>(this->r, this->ue, svg));
			this->fake_g_element.accept(this->r);
		}

		void default_visit(const svgdom::element& element)override{
			struct fake_svg_element : public svgdom::element{
				renderer& r;
				const svgdom::element& e;

				fake_svg_element(renderer& r, const svgdom::element& e) :
						r(r), e(e)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					this->e.accept(this->r);
				}

				const std::string& get_tag()const override{
					return fake_svg_element_tag;
				}
			};

			this->fake_g_element.children.push_back(std::make_unique<fake_svg_element>(this->r, element));
			this->fake_g_element.accept(this->r);
		}

		void default_visit(const svgdom::element& element, const svgdom::container& c)override{
			this->default_visit(element);
		}
	} visitor(*this, e);

	ASSERT(ref)

	ref->accept(visitor);
}

void renderer::visit(const svgdom::svg_element& e){
//	TRACE(<< "rendering SvgElement" << std::endl)
	render_svg_element(e, e, e, e, e.x, e.y, e.width, e.height);
}

bool renderer::is_invisible(){
	auto p = this->style_stack.get_style_property(svgdom::style_property::visibility);
	if(p && std::holds_alternative<svgdom::visibility>(*p)){
		if(*std::get_if<svgdom::visibility>(p) != svgdom::visibility::visible){
			return true;
		}
	}
	return this->is_group_invisible();
}

bool renderer::is_group_invisible(){
	auto p = this->style_stack.get_style_property(svgdom::style_property::display);
	if(p && std::holds_alternative<svgdom::display>(*p)){
		if(*std::get_if<svgdom::display>(p) == svgdom::display::none){
			return true;
		}
	}
	return false;
}

void renderer::visit(const svgdom::path_element& e){
//	TRACE(<< "rendering PathElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	r4::vector2<real> prev_quadratic_p = 0;

	const svgdom::path_element::step* prevStep = nullptr;

	for(auto& s : e.path){
		switch(s.type_){
			case svgdom::path_element::step::type::move_abs:
				this->canvas.move_to_abs({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::move_rel:
				this->canvas.move_to_rel({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::line_abs:
				this->canvas.line_to_abs({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::line_rel:
				this->canvas.line_to_rel({real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::horizontal_line_abs:
				this->canvas.line_to_abs({
						real(s.x),
						this->canvas.get_current_point().y()
					});
				break;
			case svgdom::path_element::step::type::horizontal_line_rel:
				this->canvas.line_to_rel({real(s.x), 0});
				break;
			case svgdom::path_element::step::type::vertical_line_abs:
				this->canvas.line_to_abs({
						this->canvas.get_current_point().x(),
						real(s.y)
					});
				break;
			case svgdom::path_element::step::type::vertical_line_rel:
				this->canvas.line_to_rel({0, real(s.y)});
				break;
			case svgdom::path_element::step::type::close:
				this->canvas.close_path();
				break;
			case svgdom::path_element::step::type::quadratic_abs:
				this->canvas.quadratic_curve_to_abs({real(s.x1), real(s.y1)}, {real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::quadratic_rel:
				this->canvas.quadratic_curve_to_rel({real(s.x1), real(s.y1)}, {real(s.x), real(s.y)});
				break;
			case svgdom::path_element::step::type::quadratic_smooth_abs:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p1 = r4::vector2<real>{real(prevStep->x1), real(prevStep->y1)};

					r4::vector2<real> cp1; // control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::quadratic_abs:
							cp1 = -(p1 - cur_p) + cur_p;
							break;
						case svgdom::path_element::step::type::quadratic_smooth_abs:
							cp1 = -(prev_quadratic_p - cur_p) + cur_p;
							break;
						case svgdom::path_element::step::type::quadratic_rel:
							cp1 = -(p1 - p) + cur_p;
							break;
						case svgdom::path_element::step::type::quadratic_smooth_rel:
							cp1 = -(prev_quadratic_p - p) + cur_p;
							break;
						default:
							// No previous step or previous step is not a quadratic Bezier curve.
							// Set first control point equal to current point
							cp1 = cur_p;
							break;
					}
					prev_quadratic_p = cp1;
					this->canvas.quadratic_curve_to_abs(cp1, {real(s.x), real(s.y)});
				}
				break;
			case svgdom::path_element::step::type::quadratic_smooth_rel:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p1 = r4::vector2<real>{real(prevStep->x1), real(prevStep->y1)};

					r4::vector2<real> cp1; // control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::quadratic_smooth_abs:
							cp1 = -(prev_quadratic_p - cur_p);
							break;
						case svgdom::path_element::step::type::quadratic_abs:
							cp1 = -(p1 - cur_p);
							break;
						case svgdom::path_element::step::type::quadratic_smooth_rel:
							cp1 = -(prev_quadratic_p - p);
							break;
						case svgdom::path_element::step::type::quadratic_rel:
							cp1 = -(p1 - p);
							break;
						default:
							// No previous step or previous step is not a quadratic Bezier curve.
							// Set first control point equal to current point, i.e. 0 because this is Relative step.
							cp1.set(0);
							break;
					}
					prev_quadratic_p = cp1;
					this->canvas.quadratic_curve_to_rel(cp1, {real(s.x), real(s.y)});
				}
				break;
			case svgdom::path_element::step::type::cubic_abs:
				this->canvas.cubic_curve_to_abs(
						{real(s.x1), real(s.y1)},
						{real(s.x2), real(s.y2)},
						{real(s.x), real(s.y)}
					);
				break;
			case svgdom::path_element::step::type::cubic_rel:
				this->canvas.cubic_curve_to_rel(
						{real(s.x1), real(s.y1)},
						{real(s.x2), real(s.y2)},
						{real(s.x), real(s.y)}
					);
				break;
			case svgdom::path_element::step::type::cubic_smooth_abs:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p2 = r4::vector2<real>{real(prevStep->x2), real(prevStep->y2)};

					r4::vector2<real> cp1; // first control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::cubic_smooth_abs:
						case svgdom::path_element::step::type::cubic_abs:
							cp1 = -(p2 - cur_p) + cur_p;
							break;
						case svgdom::path_element::step::type::cubic_smooth_rel:
						case svgdom::path_element::step::type::cubic_rel:
							cp1 = -(p2 - p) + cur_p;
							break;
						default:
							// No previous step or previous step is not a cubic Bezier curve.
							// Set first control point equal to current point
							cp1 = cur_p;
							break;
					}
					this->canvas.cubic_curve_to_abs(
							cp1,
							{real(s.x2), real(s.y2)},
							{real(s.x), real(s.y)}
						);
				}
				break;
			case svgdom::path_element::step::type::cubic_smooth_rel:
				{
					auto cur_p = this->canvas.get_current_point();
					auto p = r4::vector2<real>{real(prevStep->x), real(prevStep->y)};
					auto p2 = r4::vector2<real>{real(prevStep->x2), real(prevStep->y2)};

					r4::vector2<real> cp1; // first control point
					switch(prevStep ? prevStep->type_ : svgdom::path_element::step::type::unknown){
						case svgdom::path_element::step::type::cubic_smooth_abs:
						case svgdom::path_element::step::type::cubic_abs:
							cp1 = -(p2 - cur_p);
							break;
						case svgdom::path_element::step::type::cubic_smooth_rel:
						case svgdom::path_element::step::type::cubic_rel:
							cp1 = -(p2 - p);
							break;
						default:
							// No previous step or previous step is not a cubic Bezier curve.
							// Set first control point equal to current point, i.e. 0 because this is Relative step.
							cp1.set(0);
							break;
					}
					this->canvas.cubic_curve_to_rel(
							cp1,
							{real(s.x2), real(s.y2)},
							{real(s.x), real(s.y)}
						);
				}
				break;
			case svgdom::path_element::step::type::arc_abs:
				this->canvas.arc_abs(
						{real(s.x), real(s.y)},
						{real(s.rx), real(s.ry)},
						deg_to_rad(real(s.x_axis_rotation)),
						s.flags.large_arc,
						s.flags.sweep
					);
				break;
			case svgdom::path_element::step::type::arc_rel:
				this->canvas.arc_rel(
						{real(s.x), real(s.y)},
						{real(s.rx), real(s.ry)},
						deg_to_rad(real(s.x_axis_rotation)),
						s.flags.large_arc,
						s.flags.sweep
					);
				break;
			default:
				ASSERT_INFO(false, "unknown path step type: " << unsigned(s.type_))
				break;
		}
		prevStep = &s;
	}

	this->render_shape(group_push.is_group_pushed());
}

void renderer::visit(const svgdom::circle_element& e){
//	TRACE(<< "rendering CircleElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	auto c = this->length_to_px(e.cx, e.cy);
	auto r = this->length_to_px(e.r);

	this->canvas.circle(c, r);

	this->render_shape(group_push.is_group_pushed());
}

void renderer::visit(const svgdom::polyline_element& e){
//	TRACE(<< "rendering PolylineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	if(e.points.empty()){
		return;
	}

	auto i = e.points.begin();
	this->canvas.move_to_abs(i->to<real>());
	++i;

	for(; i != e.points.end(); ++i){
		this->canvas.line_to_abs(i->to<real>());
	}

	this->render_shape(group_push.is_group_pushed());
}

void renderer::visit(const svgdom::polygon_element& e){
//	TRACE(<< "rendering PolygonElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	if (e.points.size() == 0) {
		return;
	}

	auto i = e.points.begin();
	this->canvas.move_to_abs(i->to<real>());
	++i;

	for (; i != e.points.end(); ++i) {
		this->canvas.line_to_abs(i->to<real>());
	}

	this->canvas.close_path();

	this->render_shape(group_push.is_group_pushed());
}

void renderer::visit(const svgdom::line_element& e){
//	TRACE(<< "rendering LineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	this->canvas.move_to_abs(this->length_to_px(e.x1, e.y1));
	this->canvas.line_to_abs(this->length_to_px(e.x2, e.y2));

	this->render_shape(group_push.is_group_pushed());
}

void renderer::visit(const svgdom::ellipse_element& e){
//	TRACE(<< "rendering EllipseElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	auto c = this->length_to_px(e.cx, e.cy);
	auto r = this->length_to_px(e.rx, e.ry);
	this->canvas.move_to_abs(c + r4::vector2<real>{r.x(), 0}); // move to start point
	this->canvas.arc_abs(c, r, 0, real(2) * utki::pi<real>());
	this->canvas.close_path();

	this->render_shape(group_push.is_group_pushed());
}

void renderer::visit(const svgdom::style_element& e){
	this->style_stack.add_css(e.css);
}

void renderer::visit(const svgdom::rect_element& e){
//	TRACE(<< "rendering RectElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	common_element_push group_push(*this, false);

	this->apply_transformations(e.transformations);

	auto dims = this->length_to_px(e.width, e.height);

	// NOTE: see SVG sect: https://www.w3.org/TR/SVG/shapes.html#RectElementWidthAttribute
	//       Zero values disable rendering of the element.
	if(dims.x() == real(0) || dims.y() == real(0)){
		return;
	}

	if((e.rx.value == 0 || !e.rx.is_valid()) & (e.ry.value == 0 || !e.ry.is_valid())){
		this->canvas.rectangle({this->length_to_px(e.x, e.y), dims});
	}else{
		// compute real rx and ry
		auto rx = e.rx;
		auto ry = e.ry;

		if(!ry.is_valid() && rx.is_valid()){
			ry = rx;
		}else if(!rx.is_valid() && ry.is_valid()){
			rx = ry;
		}
		ASSERT(rx.is_valid() && ry.is_valid())

		auto r = this->length_to_px(rx, ry);

		if(r.x() > dims.x() / 2){
			rx = e.width;
			rx.value /= 2;
		}

		if(r.y() > dims.y() / 2){
			ry = e.height;
			ry.value /= 2;
		}

		r = this->length_to_px(rx, ry);

		auto p = this->length_to_px(e.x, e.y);

		this->canvas.rectangle({p, dims}, r);
	}

	this->render_shape(group_push.is_group_pushed());
}

const decltype(svgdom::transformable::transformations)& renderer::gradient_get_transformations(const svgdom::gradient& g){
	if(g.transformations.size() != 0){
		return g.transformations;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				return this->gradient_get_transformations(*caster.gradient);
			}
		}
	}
	return g.transformations;
}

svgdom::coordinate_units renderer::gradient_get_units(const svgdom::gradient& g){
	if(g.units != svgdom::coordinate_units::unknown){
		return g.units;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				return this->gradient_get_units(*caster.gradient);
			}
		}
	}
	return svgdom::coordinate_units::object_bounding_box; // object bounding box is default
}

svgdom::length renderer::gradient_get_x1(const svgdom::linear_gradient_element& g){
	if(g.x1.is_valid()){
		return g.x1;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.linear){
				return this->gradient_get_x1(*caster.linear);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_y1(const svgdom::linear_gradient_element& g){
	if(g.y1.is_valid()){
		return g.y1;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.linear){
				return this->gradient_get_y1(*caster.linear);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_x2(const svgdom::linear_gradient_element& g) {
	if(g.x2.is_valid()){
		return g.x2;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.linear){
				return this->gradient_get_x2(*caster.linear);
			}
		}
	}
	return svgdom::length(100, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_y2(const svgdom::linear_gradient_element& g) {
	if(g.y2.is_valid()){
		return g.y2;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.linear){
				return this->gradient_get_y2(*caster.linear);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_cx(const svgdom::radial_gradient_element& g){
	if(g.cx.is_valid()){
		return g.cx;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.radial){
				return this->gradient_get_cx(*caster.radial);
			}
		}
	}
	return svgdom::length(50, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_cy(const svgdom::radial_gradient_element& g){
	if(g.cy.is_valid()){
		return g.cy;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.radial){
				return this->gradient_get_cy(*caster.radial);
			}
		}
	}
	return svgdom::length(50, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_r(const svgdom::radial_gradient_element& g) {
	if(g.r.is_valid()){
		return g.r;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.radial){
				return this->gradient_get_r(*caster.radial);
			}
		}
	}
	return svgdom::length(50, svgdom::length_unit::percent);
}

svgdom::length renderer::gradient_get_fx(const svgdom::radial_gradient_element& g) {
	if(g.fx.is_valid()){
		return g.fx;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.radial){
				return this->gradient_get_fx(*caster.radial);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::unknown);
}

svgdom::length renderer::gradient_get_fy(const svgdom::radial_gradient_element& g) {
	if(g.fy.is_valid()){
		return g.fy;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.radial){
				return this->gradient_get_fy(*caster.radial);
			}
		}
	}
	return svgdom::length(0, svgdom::length_unit::unknown);
}

const decltype(svgdom::container::children)& renderer::gradient_get_stops(const svgdom::gradient& g) {
	if(g.children.size() != 0){
		return g.children;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				return this->gradient_get_stops(*caster.gradient);
			}
		}
	}

	return g.children;
}

const decltype(svgdom::styleable::styles)& renderer::gradient_get_styles(const svgdom::gradient& g){
	if(g.styles.size() != 0){
		return g.styles;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				return this->gradient_get_styles(*caster.gradient);
			}
		}
	}

	return g.styles;
}

const decltype(svgdom::styleable::classes)& renderer::gradient_get_classes(const svgdom::gradient& g){
	if(!g.classes.empty()){
		return g.classes;
	}

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				return this->gradient_get_classes(*caster.gradient);
			}
		}
	}

	return g.classes;
}

svgdom::gradient::spread_method renderer::gradient_get_spread_method(const svgdom::gradient& g){
	if(g.spread_method_ != svgdom::gradient::spread_method::default_){
		return g.spread_method_;
	}

	ASSERT(g.spread_method_ == svgdom::gradient::spread_method::default_)

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				return this->gradient_get_spread_method(*caster.gradient);
			}
		}
	}

	return svgdom::gradient::spread_method::pad;
}

decltype(svgdom::styleable::presentation_attributes) renderer::gradient_get_presentation_attributes(const svgdom::gradient& g){
	decltype(svgdom::styleable::presentation_attributes) ret = g.presentation_attributes; // copy

	decltype(svgdom::styleable::presentation_attributes) ref_attrs;

	auto ref_id = g.get_local_id_from_iri();
	if(ref_id.size() != 0){
		auto ref = this->finder_by_id.find(ref_id);

		if(ref){
			gradient_caster caster;
			ref->accept(caster);
			if(caster.gradient){
				ref_attrs = this->gradient_get_presentation_attributes(*caster.gradient);
			}
		}
	}

	for(auto& ra : ref_attrs){
		if(ret.find(ra.first) == ret.end()){
			ret.insert(std::make_pair(ra.first, std::move(ra.second)));
		}
	}

	return ret;
}

void renderer::blit(const surface& s){
	if(s.span.empty() || s.d.x() == 0 || s.d.y() == 0){
		TRACE(<< "renderer::blit(): source image is empty" << std::endl)
		return;
	}
	ASSERT(!s.span.empty() && s.d.x() != 0 && s.d.y() != 0)

	auto dst = this->canvas.get_sub_surface();

	if(s.p.x() >= dst.d.x() || s.p.y() >= dst.d.y()){
		TRACE(<< "renderer::blit(): source image is out of canvas" << std::endl)
		return;
	}

	auto dstp = dst.span.data() + s.p.y() * dst.stride + s.p.x();
	auto srcp = s.span.data();
	using std::min;
	auto dp = min(s.d, dst.d - s.p);

	for(unsigned y = 0; y != dp.y(); ++y){
		auto p = dstp + y * dst.stride;
		auto sp = srcp + y * s.stride;
		for(unsigned x = 0; x != dp.x(); ++x, ++p, ++sp){
			*p = *sp;
		}
	}
}
