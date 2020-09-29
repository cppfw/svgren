#include "renderer.hxx"

#include <utki/math.hpp>

#include <svgdom/elements/coordinate_units.hpp>

#include "util.hxx"
#include "config.hxx"

#include "filter_applier.hxx"

using namespace svgren;

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
	for (auto& t : transformations) {
		this->apply_transformation(t);
	}
}

void renderer::apply_viewbox(const svgdom::view_boxed& e, const svgdom::aspect_ratioed& ar){
	if (!e.is_view_box_specified()) {
		return;
	}

	if (ar.preserve_aspect_ratio.preserve != svgdom::aspect_ratioed::aspect_ratio_preservation::none) {
		if (e.view_box[3] >= 0 && this->viewport[1] >= 0) { // if viewBox width and viewport width are not 0
			real scaleFactor, dx, dy;

			real viewBoxAspect = e.view_box[2] / e.view_box[3];
			real viewportAspect = this->viewport[0] / this->viewport[1];

			if ((viewBoxAspect >= viewportAspect && ar.preserve_aspect_ratio.slice) || (viewBoxAspect < viewportAspect && !ar.preserve_aspect_ratio.slice)) {
				// fit by Y
				scaleFactor = this->viewport[1] / e.view_box[3];
				dx = e.view_box[2] - this->viewport[0];
				dy = 0;
			} else { // viewBoxAspect < viewportAspect
				// fit by X
				scaleFactor = this->viewport[0] / e.view_box[2];
				dx = 0;
				dy = e.view_box[3] - this->viewport[1];
			}
			switch (ar.preserve_aspect_ratio.preserve) {
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
		if (e.view_box[2] != 0 && e.view_box[3] != 0) { // if viewBox width and height are not 0
			this->canvas.scale(
					this->viewport[0] / e.view_box[2],
					this->viewport[1] / e.view_box[3]
				);
		}
	}
	this->canvas.translate(-e.view_box[0], -e.view_box[1]);
}

void renderer::set_gradient_properties(svgren::gradient& gradient, const svgdom::gradient& g, const svgdom::style_stack& ss){
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
			return this->g.get_tag();
		}
	} effective_gradient_styleable(g);

	effective_gradient_styleable.styles = this->gradient_get_styles(g);
	effective_gradient_styleable.classes = this->gradient_get_classes(g);
	effective_gradient_styleable.presentation_attributes = this->gradient_get_presentation_attributes(g);

	ASSERT(!gradient_ss.stack.empty())
	gradient_ss.stack.pop_back();
	gradient_ss.stack.push_back(effective_gradient_styleable);

	struct gradient_stops_adder : public svgdom::const_visitor{
		svgren::gradient& gradient;
		svgdom::style_stack& ss;

		gradient_stops_adder(svgren::gradient& gradient, svgdom::style_stack& ss) :
				gradient(gradient),
				ss(ss)
		{}

		void visit(const svgdom::gradient::stop_element& stop)override{
			svgdom::style_stack::push stylePush(this->ss, stop);

			r4::vector3<real> rgb;
			if(auto p = this->ss.get_style_property(svgdom::style_property::stop_color)){
				rgb = p->get_rgb().to<real>();
			}else{
				rgb.set(0);
			}

			svgdom::real opacity;
			if(auto p = this->ss.get_style_property(svgdom::style_property::stop_opacity)){
				opacity = p->opacity;
			}else{
				opacity = 1;
			}
			this->gradient.stops.push_back(gradient::stop{
					{rgb, opacity},
					real(stop.offset)
				});
		}
	} visitor(gradient, gradient_ss);

	for(auto& stop : this->gradient_get_stops(g)){
		stop->accept(visitor);
	}

	gradient.spread_method = this->gradient_get_spread_method(g);
}

void renderer::apply_filter(){
	if(auto filter = this->style_stack.get_style_property(svgdom::style_property::filter)){
		this->apply_filter(filter->get_local_id_from_iri());
	}
}

void renderer::apply_filter(const std::string& id){
	auto f = this->finder.find_by_id(id);
	if(!f){
		return;
	}

	filter_applier visitor(*this);

	ASSERT(f)
	f->e.accept(visitor);

	this->blit(visitor.get_last_result());
}

void renderer::set_gradient(const std::string& id){
	auto g = this->finder.find_by_id(id);
	if(!g){
		this->canvas.set_source(0);
		return;
	}

	struct CommonGradientPush{
		canvas_matrix_push matrix_push; // here we need to save/restore only matrix!

		std::unique_ptr<viewport_push> viewportPush;

		CommonGradientPush(renderer& r, const svgdom::gradient& gradient) :
				matrix_push(r.canvas)
		{
			if(r.gradient_get_units(gradient) == svgdom::coordinate_units::object_bounding_box){
				r.canvas.translate(r.user_space_bounding_box.p);
				r.canvas.scale(r.user_space_bounding_box.d);
				this->viewportPush = std::make_unique<viewport_push>(r, 1);
			}

			r.apply_transformations(r.gradient_get_transformations(gradient));
		}

		~CommonGradientPush()noexcept{}
	};

	struct GradientSetter : public svgdom::const_visitor{
		renderer& r;

		const svgdom::style_stack& ss;

		GradientSetter(renderer& r, const svgdom::style_stack& ss) : r(r), ss(ss) {}

		void visit(const svgdom::linear_gradient_element& gradient)override{
			CommonGradientPush commonPush(this->r, gradient);

			linear_gradient g;

			g.p0 = this->r.length_to_px(
					this->r.gradient_get_x1(gradient),
					this->r.gradient_get_y1(gradient)
				);
			g.p1 = this->r.length_to_px(
					this->r.gradient_get_x2(gradient),
					this->r.gradient_get_y2(gradient)
				);
			
			this->r.set_gradient_properties(g, gradient, this->ss);

			this->r.canvas.set_source(g);
		}

		void visit(const svgdom::radial_gradient_element& gradient)override{
			CommonGradientPush commonPush(this->r, gradient);

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

			radial_gradient g;

			g.c0 = this->r.length_to_px(fx, fy);
			g.c1 = this->r.length_to_px(cx, cy);
			g.r0 = 0;
			g.r1 = this->r.length_to_px(radius);

			this->r.set_gradient_properties(g, gradient, this->ss);

			this->r.canvas.set_source(g);
		}

		void default_visit(const svgdom::element&)override{
			this->r.canvas.set_source(0);
		}
	} visitor(*this, g->ss);

	g->e.accept(visitor);
}

void renderer::update_bounding_box(){
	this->user_space_bounding_box = this->canvas.get_shape_bounding_box();

	// TRACE(<< "bb = " << this->user_space_bounding_box << std::endl)

	if(this->user_space_bounding_box.d[0] == 0){
		// empty path
		return;
	}

	// set device space bounding box
	std::array<r4::vector2<real>, 4> rectVertices = {{
		this->user_space_bounding_box.p,
		this->user_space_bounding_box.x2_y2(),
		this->user_space_bounding_box.x1_y2(),
		this->user_space_bounding_box.x2_y1()
	}};

	for(auto& vertex : rectVertices){
		vertex = this->canvas.matrix_mul(vertex);

		DeviceSpaceBoundingBox bb;
		bb.left = decltype(bb.left)(vertex[0]);
		bb.right = decltype(bb.right)(vertex[0]);
		bb.top = decltype(bb.top)(vertex[1]);
		bb.bottom = decltype(bb.bottom)(vertex[1]);

		this->device_space_bounding_box.unite(bb);
	}
}

void renderer::render_shape(bool isCairoGroupPushed){
	this->update_bounding_box();

	if(auto p = this->style_stack.get_style_property(svgdom::style_property::fill_rule)){
		this->canvas.set_fill_rule(p->fill_rule);
	}else{
		this->canvas.set_fill_rule(svgdom::fill_rule::nonzero);
	}

	svgdom::style_value blackFill;

	auto fill = this->style_stack.get_style_property(svgdom::style_property::fill);
	if (!fill) {
		blackFill = svgdom::style_value::parse_paint("black");
		fill = &blackFill;
	}

	auto stroke = this->style_stack.get_style_property(svgdom::style_property::stroke);

	// OPTIMIZATION: in case there is 'opacity' style property and only one of
	//               'stroke' or 'fill' is not none and is a solid color (not pattern/gradient),
	//               then there is no need to push cairo group, but just multiply
	//               the 'stroke-opacity' or 'fill-opacity' by 'opacity' value.
	auto opacity = svgdom::real(1);
	if(!isCairoGroupPushed){
		if(auto p = this->style_stack.get_style_property(svgdom::style_property::opacity)){
			opacity = p->opacity;
		}
	}

	ASSERT(fill)
	if(!fill->is_none()){
		if(fill->is_url()){
			this->set_gradient(fill->get_local_id_from_iri());
		}else{
			svgdom::real fillOpacity = 1;
			if(auto p = this->style_stack.get_style_property(svgdom::style_property::fill_opacity)){
				fillOpacity = p->opacity;
			}

			auto fillRgb = fill->get_rgb().to<real>();
			this->canvas.set_source({fillRgb, fillOpacity * opacity});
		}

		this->canvas.fill();
	}

	if(stroke && !stroke->is_none()){
		if(auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_width)){
			this->canvas.set_line_width(this->length_to_px(p->stroke_width));
		}else{
			this->canvas.set_line_width(1);
		}

		if(auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_linecap)){
			this->canvas.set_line_cap(p->stroke_line_cap);
		}else{
			this->canvas.set_line_cap(svgdom::stroke_line_cap::butt);
		}

		if(auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_linejoin)){
			this->canvas.set_line_join(p->stroke_line_join);
		}else{
			this->canvas.set_line_join(svgdom::stroke_line_join::miter);
		}

		if(stroke->is_url()){
			this->set_gradient(stroke->get_local_id_from_iri());
		}else{
			svgdom::real strokeOpacity = 1;
			if(auto p = this->style_stack.get_style_property(svgdom::style_property::stroke_opacity)){
				strokeOpacity = p->opacity;
			}

			auto rgb = stroke->get_rgb().to<real>();
			this->canvas.set_source({rgb, strokeOpacity * opacity});
		}

		this->canvas.stroke();
	}

	// clear path if any left
	this->canvas.clear_path();
	
	this->apply_filter();
}

void renderer::render_element(
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

	canvas_group_push group_push(*this, true);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

	if(this->is_outermost_element){
		this->canvas.translate(this->length_to_px(x, y));
	}

	viewport_push viewportPush(*this, this->length_to_px(width, height));

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
		r4::vector2<real> canvasSize,
		const svgdom::svg_element& root
	) :
		canvas(canvas),
		finder(root),
		dpi(real(dpi)),
		viewport(canvasSize)
{
	this->device_space_bounding_box.set_empty();
	this->background = this->canvas.get_sub_surface();
}

void renderer::visit(const svgdom::g_element& e){
//	TRACE(<< "rendering GElement: id = " << e.id << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_group_invisible()){
		return;
	}

	canvas_group_push group_push(*this, true);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

	this->apply_transformations(e.transformations);

	this->relay_accept(e);

	this->apply_filter();
}

void renderer::visit(const svgdom::use_element& e){
//	TRACE(<< "rendering UseElement" << std::endl)
	auto ref = this->finder.find_by_id(e.get_local_id_from_iri());
	if(!ref){
		return;
	}

	struct RefRenderer : public svgdom::const_visitor{
		renderer& r;
		const svgdom::use_element& ue;
		svgdom::g_element fakeGElement;

		RefRenderer(renderer& r, const svgdom::use_element& e) :
				r(r), ue(e)
		{
			this->fakeGElement.styles = e.styles;
			this->fakeGElement.presentation_attributes = e.presentation_attributes;
			this->fakeGElement.transformations = e.transformations;

			// add x and y transformation
			{
				svgdom::transformable::transformation t;
				t.type_ = svgdom::transformable::transformation::type::translate;
				auto p = this->r.length_to_px(e.x, e.y);
				t.x = p.x();
				t.y = p.y();

				this->fakeGElement.transformations.push_back(t);
			}
		}

		void visit(const svgdom::symbol_element& symbol)override{
			struct FakeSvgElement : public svgdom::element{
				renderer& r;
				const svgdom::use_element& ue;
				const svgdom::symbol_element& se;

				FakeSvgElement(renderer& r, const svgdom::use_element& ue, const svgdom::symbol_element& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					const auto hundredPercent = svgdom::length(100, svgdom::length_unit::percent);

					this->r.render_element(
							this->se,
							this->se,
							this->se,
							this->se,
							svgdom::length(0),
							svgdom::length(0),
							this->ue.width.is_valid() ? this->ue.width : hundredPercent,
							this->ue.height.is_valid() ? this->ue.height : hundredPercent
						);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, this->ue, symbol));
			this->fakeGElement.accept(this->r);
		}

		void visit(const svgdom::svg_element& svg)override{
			struct FakeSvgElement : public svgdom::element{
				renderer& r;
				const svgdom::use_element& ue;
				const svgdom::svg_element& se;

				FakeSvgElement(renderer& r, const svgdom::use_element& ue, const svgdom::svg_element& se) :
						r(r), ue(ue), se(se)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					// width and height of <use> element override those of <svg> element.
					this->r.render_element(
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
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, this->ue, svg));
			this->fakeGElement.accept(this->r);
		}

		void default_visit(const svgdom::element& element)override{
			struct FakeSvgElement : public svgdom::element{
				renderer& r;
				const svgdom::element& e;

				FakeSvgElement(renderer& r, const svgdom::element& e) :
						r(r), e(e)
				{}

				void accept(svgdom::visitor& visitor)override{
					ASSERT(false)
				}
				void accept(svgdom::const_visitor& visitor) const override{
					this->e.accept(this->r);
				}
			};

			this->fakeGElement.children.push_back(std::make_unique<FakeSvgElement>(this->r, element));
			this->fakeGElement.accept(this->r);
		}

		void default_visit(const svgdom::element& element, const svgdom::container& c)override{
			this->default_visit(element);
		}
	} visitor(*this, e);

	ASSERT(ref)

	ref->e.accept(visitor);
}

void renderer::visit(const svgdom::svg_element& e){
//	TRACE(<< "rendering SvgElement" << std::endl)
	render_element(e, e, e, e, e.x, e.y, e.width, e.height);
}

bool renderer::is_invisible(){
	if(auto p = this->style_stack.get_style_property(svgdom::style_property::visibility)){
		if(p->visibility != svgdom::visibility::visible){
			return true;
		}
	}
	return this->is_group_invisible();
}

bool renderer::is_group_invisible(){
	if(auto p = this->style_stack.get_style_property(svgdom::style_property::display)){
		if(p->display == svgdom::display::none){
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

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

	this->apply_transformations(e.transformations);

	r4::vector2<real> prev_quadratic_p{0};

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

	this->render_shape(group_push.is_pushed());
}

void renderer::visit(const svgdom::circle_element& e){
//	TRACE(<< "rendering CircleElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

	this->apply_transformations(e.transformations);

	auto c = this->length_to_px(e.cx, e.cy);
	auto r = this->length_to_px(e.r);

	this->canvas.move_to_abs(c + r4::vector2<real>{r, 0}); // move to start point
	this->canvas.arc_abs(c,	r, 0, 2 * utki::pi<real>());
	this->canvas.close_path();

	this->render_shape(group_push.is_pushed());
}

void renderer::visit(const svgdom::polyline_element& e){
//	TRACE(<< "rendering PolylineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	// TODO: make a common push class for shapes

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

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

	this->render_shape(group_push.is_pushed());
}

void renderer::visit(const svgdom::polygon_element& e){
//	TRACE(<< "rendering PolygonElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

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

	this->render_shape(group_push.is_pushed());
}

void renderer::visit(const svgdom::line_element& e){
//	TRACE(<< "rendering LineElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

	this->apply_transformations(e.transformations);

	this->canvas.move_to_abs(this->length_to_px(e.x1, e.y1));
	this->canvas.line_to_abs(this->length_to_px(e.x2, e.y2));

	this->render_shape(group_push.is_pushed());
}

void renderer::visit(const svgdom::ellipse_element& e){
//	TRACE(<< "rendering EllipseElement" << std::endl)
	svgdom::style_stack::push pushStyles(this->style_stack, e);

	if(this->is_invisible()){
		return;
	}

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

	this->apply_transformations(e.transformations);

	auto c = this->length_to_px(e.cx, e.cy);
	auto r = this->length_to_px(e.rx, e.ry);
	this->canvas.move_to_abs(c + r4::vector2<real>{r.x(), 0}); // move to start point
	this->canvas.arc_abs(c, r, 0, real(2) * utki::pi<real>());
	this->canvas.close_path();

	this->render_shape(group_push.is_pushed());
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

	canvas_group_push group_push(*this, false);

	DeviceSpaceBoundingBoxPush deviceSpaceBoundingBoxPush(*this);

	canvas_context_push context_push(this->canvas);

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

		this->canvas.move_to_abs(p + r4::vector2<real>{r.x(), 0});
		this->canvas.line_to_abs(p + r4::vector2<real>{dims.x() - r.x(), 0});

		this->canvas.arc_abs(
				p + r4::vector2<real>{dims.x() - r.x(), r.y()},
				r,
				-utki::pi<real>() / 2,
				utki::pi<real>() / 2
			);

		this->canvas.line_to_abs(p + dims - r4::vector2<real>{0, r.y()});

		this->canvas.arc_abs(
				p + dims - r,
				r,
				0,
				utki::pi<real>() / 2
			);

		this->canvas.line_to_abs(p + r4::vector2<real>{r.x(), dims.y()});

		this->canvas.arc_abs(
				p + r4::vector2<real>{r.x(), dims.y() - r.y()},
				r,
				utki::pi<real>() / 2,
				utki::pi<real>() / 2
			);

		this->canvas.line_to_abs(p + r4::vector2<real>{0, r.y()});

		this->canvas.arc_abs(
				p + r,
				r,
				utki::pi<real>(),
				utki::pi<real>() / 2
			);

		this->canvas.close_path();
	}

	this->render_shape(group_push.is_pushed());
}

const decltype(svgdom::transformable::transformations)& renderer::gradient_get_transformations(const svgdom::gradient& g){
	if(g.transformations.size() != 0){
		return g.transformations;
	}

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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

	auto refId = g.get_local_id_from_iri();
	if(refId.length() != 0){
		auto ref = this->finder.find_by_id(refId);

		if(ref){
			gradient_caster caster;
			ref->e.accept(caster);
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
