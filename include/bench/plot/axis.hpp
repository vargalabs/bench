/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * X/Y axis elements: lay out tick labels along a grid and render them through
 * the canvas. Ported from plot:: into bench::plot::. Dependency-free.
 */
#ifndef BENCH_PLOT_AXIS_HPP
#define BENCH_PLOT_AXIS_HPP

#include <vector>
#include <string>
#include <optional>
#include <cmath>
#include <cstddef>

#include "attributes.hpp"
#include "canvas.hpp"
#include "utils.hpp"
#include "meta.hpp"

namespace bench::plot::axis {
	constexpr float rad = 3.14159265358979f / 180.0f;

	template <class T, class... Ts>
	struct x : public attribute::element_t, public impl::io_t< x<T,Ts...>> {
		using value_type = tag::axis::x_t;
		using attribute::element_t::color;
		using attribute::element_t::position;
		using font_t = attribute::font_t;
		using layout_t = attribute::layout_t;
		using degree_t = attribute::degree_t;
		using position_t = attribute::position_t;
		using attribute_t = bench::plot::attribute::element_t;

		x(const std::vector<T>& labels, Ts... args)
			: attribute::element_t(args...), labels(labels) {
			if( !rotate ) rotate = degree_t{-90.0} ;
			dx.resize(labels.size()); dy.resize(labels.size());
			set_font( arg::get<font_t>(args...) );
		}
		void ostream( impl::canvas_t& os ) const {
			os.grid_x = grid;
			attribute_t text_attr;
			text_attr.rotate = this->rotate;
			os.group(position->x, position->y, *this,
			[&]() -> void {
				for(std::size_t i=0; i<labels.size(); i++)
					os.text(labels[i], static_cast<std::size_t>(dx[i]), static_cast<std::size_t>(dy[i]), text_attr);
			});
		}
		void set_font( const std::optional<font_t>& font_ ){
			this->font = font_ ? font_ : font_t{"Ubuntu Mono", "bold", 6};
			this->grid = 1.6f * font->size;
			for (std::size_t i=0; i < labels.size(); i++)
			  	dy[i] = 0.0f, dx[i] = i * grid;
			float angle = rotate->value * rad;
			float max = utils::size_of_max_value( labels ) * 0.6f * font->size * std::abs(std::sin(angle));
			if( !position )
		   		position = position_t{static_cast<std::size_t>(2 * grid), static_cast<std::size_t>(max)};
		}
		float get_x(){
			return static_cast<float>(
					std::get<std::size_t>(position->x)) - 0.5f * grid;
		}
		const std::vector<T>& labels;
		std::vector<float> dx,dy;
		float grid = 0, max_x = 0;
	};

	template <class T, class... Ts>
	struct y : public  attribute::element_t, impl::io_t< y<T, Ts...>> {
		using value_type = tag::axis::y_t;
		using attribute::element_t::position;
		using attribute::element_t::color;
		using font_t = attribute::font_t;
		using layout_t = attribute::layout_t;
		using degree_t = attribute::degree_t;
		using position_t = attribute::position_t;
		using attribute_t = bench::plot::attribute::element_t;

		y(const std::vector<T>& labels, Ts... args)
			: attribute::element_t(args...), labels(labels) {
			if( !rotate ) rotate = degree_t{0.0} ;
			this->layout = bench::plot::layout::vertical;
			dx.resize(labels.size()); dy.resize(labels.size());
			set_font( arg::get<font_t>(args...) );
		}

		void ostream( impl::canvas_t& os ) const {
			os.grid_y = grid;

			attribute_t text_attr;
			os.group(position->x, position->y, *this,
			[&]() -> void {
				for(std::size_t i=0; i<labels.size(); i++)
					os.text(labels[i], static_cast<std::size_t>(dx[i]), static_cast<std::size_t>(dy[i] + .25 * grid), text_attr);
			});
		}

		void set_font( const std::optional<font_t>& font_ ){
			this->font = font_ ? font_ : font_t{"Ubuntu Mono", "bold", 6};
			this->grid = 1.2f * font->size;
			for (std::size_t i=0; i < labels.size(); i++)
				dx[i] = 0.0f, dy[i] = i * grid;
			if( !position )
				position = position_t{std::size_t{0}, std::size_t{0}};
		}

		float get_y(){
			return dy.back() + grid;
		}
		float get_x(){
			float angle = rotate->value * rad;
			float max = utils::size_of_max_value( labels ) * 0.6f * font->size * std::abs(std::cos(angle));

			return static_cast<float>(
					std::get<std::size_t>(position->x)) + max;
		}

		const std::vector<T>& labels;
		std::vector<float> dx,dy;
		float grid = 0, max_x = 0, max_y = 0;
	};
}
#endif
