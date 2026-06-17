/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Heatmap renderers over the SVG canvas:
 *   - categorical (integral/enum): unique value -> palette index;
 *   - continuous (arithmetic, e.g. double): per-cell normalize across [min,max]
 *     then map to a 3-stop blue->yellow->red gradient.
 * Ported from plot:: into plot::; the original third-party matrix
 * dependency is replaced by a tiny non-owning row-major `mat<T>` view.
 * Dependency-free.
 */
#ifndef PLOT_HEATMAP_HPP
#define PLOT_HEATMAP_HPP

#include <string>
#include <vector>
#include <tuple>
#include <array>
#include <algorithm>
#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <ostream>
#include <format>

#include "attributes.hpp"
#include "canvas.hpp"
#include "axis.hpp"
#include "text.hpp"
#include "meta.hpp"

namespace plot {
	// Non-owning, row-major matrix view — the dependency-free replacement for the
	// third-party dense matrix the original heatmap overload consumed.
	template<class T> struct mat {
		const T* p; std::size_t rows, cols;
		T operator()(std::size_t i, std::size_t j) const { return p[i*cols + j]; }
		const T* begin() const { return p; }
		const T* end() const { return p + rows*cols; }
	};
}

namespace plot::data {
	template <class T>
	struct point_t {
		std::size_t x,y;
		T value;
		std::string label,href;
	};
}

namespace plot::impl {
	// 3-stop blue -> yellow -> red linear gradient; t in [0,1] -> 0xRRGGBB.
	inline std::uint32_t gradient(double t){
		if( t < 0.0 ) t = 0.0; else if( t > 1.0 ) t = 1.0;
		// stop colours
		static constexpr double blue[3]   = {  0.0,  64.0, 255.0};
		static constexpr double yellow[3] = {255.0, 255.0,   0.0};
		static constexpr double red[3]    = {220.0,  30.0,  30.0};
		double r,g,b;
		if( t < 0.5 ){
			double u = t / 0.5;
			r = blue[0] + u*(yellow[0]-blue[0]);
			g = blue[1] + u*(yellow[1]-blue[1]);
			b = blue[2] + u*(yellow[2]-blue[2]);
		} else {
			double u = (t - 0.5) / 0.5;
			r = yellow[0] + u*(red[0]-yellow[0]);
			g = yellow[1] + u*(red[1]-yellow[1]);
			b = yellow[2] + u*(red[2]-yellow[2]);
		}
		auto clamp8 = [](double v)->std::uint32_t {
			if( v < 0 ) v = 0; else if( v > 255 ) v = 255;
			return static_cast<std::uint32_t>(v + 0.5);
		};
		return (clamp8(r) << 16) | (clamp8(g) << 8) | clamp8(b);
	}
}

namespace plot::impl {
	// ---- categorical heatmap over a mat<T> view (integral / enum) -------------
	template <typename T>
	typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value>
	::type heatmap(impl::canvas_t& canvas, float x, float y,
			std::vector<float>& dx, std::vector<float>& dy, float width, float height,
			const plot::mat<T>& data, const plot::attribute::color_t& palette ) {
		using attribute_t = plot::attribute::element_t;
		attribute_t mock_attr, gr_attr;
		// find unique elements
		std::vector<T> M(data.begin(), data.end());
		std::sort(M.begin(), M.end());
		auto last = std::unique(M.begin(), M.end());
	   	M.erase(last, M.end());

		canvas.group(static_cast<std::size_t>(x), static_cast<std::size_t>(y), mock_attr, [&]() -> void {
			for( auto value : M ) {
				gr_attr.color = palette[static_cast<std::ptrdiff_t>(value)];
				canvas.group(std::size_t{0}, std::size_t{0}, gr_attr, [&]() -> void {
					for(std::size_t j = 0; j < data.cols; j++) for( std::size_t i = 0; i < data.rows; i++)
						if( data(i,j) == value )
							canvas.rect( dx[j], dy[i], width, height, 1.5, 1.5, mock_attr);
				});
			}
		});
	}
}

namespace plot::impl {
	// ---- categorical heatmap over a point list (integral / enum) --------------
	template <typename T>
	typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value>
	::type heatmap(impl::canvas_t& canvas, float x, float y,
			std::vector<float>& dx, std::vector<float>& dy, float width, float height,
			const std::vector<plot::data::point_t<T>>& data, const plot::attribute::color_t& palette ) {
		using attribute_t = plot::attribute::element_t;
		attribute_t rect_attr, gr_attr;

		gr_attr.color.reset();
		canvas.group(static_cast<std::size_t>(x), static_cast<std::size_t>(y), gr_attr, [&]() -> void {
			for(auto v : data){
				rect_attr.href = v.href; rect_attr.label = v.label; rect_attr.color = palette[v.value];
				canvas.rect(dx[v.x], dy[v.y], width, height, 1.5, 1.5, rect_attr);
			}
		});
	}
}

namespace plot::impl {
	// ---- continuous-gradient heatmap over a mat<T> view (arithmetic) ----------
	// Each cell is normalized across the grid's [min,max] and coloured via the
	// blue->yellow->red gradient; a <title> carries the numeric value.
	template <typename T>
	typename std::enable_if<std::is_arithmetic<T>::value && !std::is_integral<T>::value>
	::type heatmap(impl::canvas_t& canvas, float x, float y,
			std::vector<float>& dx, std::vector<float>& dy, float width, float height,
			const plot::mat<T>& data, const plot::attribute::color_t& /*palette*/ ) {
		using attribute_t = plot::attribute::element_t;
		if( data.rows == 0 || data.cols == 0 ) return;

		double lo = static_cast<double>(*data.begin());
		double hi = lo;
		for(const T& v : data){
			double d = static_cast<double>(v);
			if( d < lo ) lo = d;
			if( d > hi ) hi = d;
		}
		const double span = (hi > lo) ? (hi - lo) : 1.0;

		attribute_t gr_attr;
		canvas.group(static_cast<std::size_t>(x), static_cast<std::size_t>(y), gr_attr, [&]() -> void {
			for(std::size_t i = 0; i < data.rows; i++)
				for(std::size_t j = 0; j < data.cols; j++){
					double value = static_cast<double>(data(i,j));
					double t = (value - lo) / span;
					attribute_t cell;
					cell.color = plot::attribute::color_t{ impl::gradient(t) };
					cell.label = std::format("{:.4g}", value);
					canvas.rect( dx[j], dy[i], width, height, 1.5, 1.5, cell );
				}
		});
	}
}

namespace plot {
	// ---- the heatmap driver (writes a standalone .svg to an ostream) ----------
	template <class T, class... arg_t>
	void heatmap(std::ostream& os, const T& data, arg_t... args ) {
		using title_t    = typename arg::tpos<tag::title_t, arg_t...>;
		using footnote_t = typename arg::tpos<tag::footnote_t, arg_t...>;
		using legend_t   = typename arg::tpos<tag::legend_t, arg_t...>;
		using axisx_t    = typename arg::tpos<tag::axis::x_t, arg_t...>;
		using axisy_t    = typename arg::tpos<tag::axis::y_t, arg_t...>;
		using width_t    = typename arg::tpos<tag::width_t, arg_t...>;
		using height_t   = typename arg::tpos<tag::height_t, arg_t...>;
		using margin_t   = typename arg::tpos<tag::margin_t, arg_t...>;

		static_assert( axisx_t::present, "x axis must be specified..." );
		static_assert( axisy_t::present, "y axis must be specified..." );

		auto tuple = std::forward_as_tuple(args...);

		std::array<float,4> margin{5,5,5,5};
		if constexpr( margin_t::present )
			margin = std::get<margin_t::position>( tuple ).value;

		auto x_axis = std::get<axisx_t::position>( tuple );
		auto y_axis = std::get<axisy_t::position>( tuple );

		float offset_x = std::get<std::size_t>(x_axis.position->x) - .5f * x_axis.grid;
		float offset_y = std::get<std::size_t>(x_axis.position->y) + .5f * y_axis.grid;
		// when y axis label position is not preset, compute it from x layout
		float pos_x = x_axis.dx.back() + 1.5f * x_axis.grid + offset_x;
		y_axis.position = position{static_cast<std::size_t>(pos_x), static_cast<std::size_t>(offset_y + .5f * y_axis.grid) };
		// compute size when not set
		std::size_t width, height;
	   	if constexpr( width_t::present )
			width = std::get<width_t::position>( tuple ).value;
		else
			width = static_cast<std::size_t>(y_axis.get_x() + margin[2]);
	   	if constexpr( height_t::present )
			height = std::get<height_t::position>( tuple ).value;
		else
			height = static_cast<std::size_t>(y_axis.get_y() + offset_y + margin[3]);

		auto canvas = plot::impl::canvas_t(os, width, height + 10, margin);
		canvas << x_axis; canvas << y_axis;

		if constexpr (title_t::present)    canvas << std::get<title_t::position>( tuple );
		if constexpr (footnote_t::present) canvas << std::get<footnote_t::position>( tuple );

		plot::attribute::color_t palette{ 0x4060FF };
		if constexpr (legend_t::present) {
			canvas << std::get<legend_t::position>( tuple );
			auto legend = std::get<legend_t::position>( tuple );
			if( legend.color ) palette = legend.color.value();
		}
		impl::heatmap(canvas,
				offset_x, offset_y, x_axis.dx, y_axis.dy,
				.9f * x_axis.grid, .9f * y_axis.grid, data, palette );
	}

	// filename convenience: opens a truncating ofstream and renders into it.
	template <class T, class... arg_t>
	void heatmap(const std::string& filename, const T& data, arg_t... args ) {
		std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
		heatmap(ofs, data, args...);
	}
}
#endif
