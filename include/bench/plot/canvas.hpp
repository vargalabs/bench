/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * The SVG canvas: opens an <svg> document on construction, writes primitives
 * (rect/text/line/poly_line/circle/group) via std::format, and closes it on
 * destruction. Ported from plot:: into bench::plot::; the original matrix and
 * formatting third-party dependencies are removed (std::format only).
 * Dependency-free.
 */
#ifndef BENCH_PLOT_CANVAS_HPP
#define BENCH_PLOT_CANVAS_HPP

#include <string>
#include <string_view>
#include <fstream>
#include <ostream>
#include <array>
#include <vector>
#include <variant>
#include <optional>
#include <functional>
#include <format>
#include <stdexcept>
#include <cstddef>

#include "attributes.hpp"
#include "utils.hpp"

namespace bench::plot::impl {
	struct canvas_t {
		using color_t = bench::plot::attribute::color_t;
		using font_t = bench::plot::attribute::font_t;
		using stroke_t = bench::plot::attribute::stroke_t;
		using opacity_t = bench::plot::attribute::opacity_t;
		using id_t = bench::plot::attribute::id_t;
		using position_t = bench::plot::attribute::position_t;
		using attribute_t = bench::plot::attribute::element_t;
		using align_t = bench::plot::attribute::align_t;

		// stream-backed: render into any std::ostream (file or in-memory)
		canvas_t(std::ostream& os, std::size_t x, std::size_t y, const std::array<float,4>& margin);
		~canvas_t();

		void group (std::variant<std::size_t, align_t> x, std::variant<std::size_t, align_t> y,
				const attribute_t& attr, std::function<void()> const& call );
		void rect(float x, float y, float width, float height, float rx, float ry, const attribute_t& attr );
		void circle(float cx, float cy, float radius, const attribute_t& attr );
		void line(float x1, float y1, float x2, float y2, const attribute_t& attr );
		void poly_line(const std::vector<float>& x, const std::vector<float>& y, const attribute_t& attr );
		static std::pair<std::size_t,std::size_t>
			bounding_box( 	const std::vector<std::string>& x_axis, double angle_x,
							const std::vector<std::string>& y_axis, double angle_y, std::size_t font_size );
		void text( const std::string& txt,
				std::variant<std::size_t, align_t> x, std::variant<std::size_t, align_t> y, const attribute_t& attr );

		std::optional<float> grid_x, grid_y;

		private:
		// std::format requires a consteval format string, but the SVG templates
		// below are runtime std::string members — so route every call through
		// std::vformat, which accepts a runtime format string.
		template <class... Args>
		static std::string fmt(std::string_view f, Args... args){
			return std::vformat(f, std::make_format_args(args...));
		}
		void align( std::optional<align_t> alignment );
		float align_x( std::variant<std::size_t, align_t> x );
		float align_y( std::variant<std::size_t, align_t> y, const std::optional<font_t>& font);

		std::size_t width, height;
		float font_size = 0;
		std::array<float,4> margin;
		static constexpr double rad  = 0.017453292519943295;
		std::ostream& os;

		const std::string gr_begin_ = "<g {}>\n";
		const std::string gr_end_ = "</g>\n";
		const std::string rotate_ = " rotate({} {} {})";
		const std::string translate_ = "translate({} {})";
		const std::string transform_ = " transform=\"{}\"";

		const std::string svg_start_  = "<svg viewBox=\"{} {} {} {}\"  xmlns=\"http://www.w3.org/2000/svg\">\n";
		const std::string svg_end_ = "</svg>\n";
		const std::string href_begin_ = "<a href=\"{}\">";
		const std::string href_end_   = "</a>\n";

		const std::string text_anchor_ = " text-anchor=\"{}\"";
		const std::string font_ = " font-size=\"{}\" font-family=\"{}\" font-weight=\"{}\"";
		const std::string fill_ = " fill=\"#{:06X}\"";
		const std::string stroke_attr_ = " stroke=\"#{:06X}\" stroke-width=\"{:.2f}\"";
		const std::array<std::string, 5> align_horizontal = {"start", "middle", "end", "", ""};

		const std::string rect_ = "<rect x=\"{:.2f}\" y=\"{:.2f}\" width=\"{:.2f}\" height=\"{:.2f}\" rx=\"{:.2f}\" ry=\"{:.2f}\" {}>{}</rect>\n";
		const std::string line_ = "<line x1=\"{:.2f}\" y1=\"{:.2f}\" x2=\"{:.2f}\" y2=\"{:.2f}\"{}/>\n";
		const std::string polyline_ = "<polyline points=\"{}\" fill=\"none\"{}/>\n";
		const std::string circle_ = "<circle cx=\"{:.2f}\" cy=\"{:.2f}\" r=\"{:.2f}\" {}>{}</circle>\n";
		const std::string text_ = "<text x=\"{:.2f}\" y=\"{:.2f}\"{}>{}</text>\n";
		const std::string title_ = "<title>{}</title>";
	};
}

namespace bench::plot::impl {
	template <class Derived >
	struct io_t {
		void ostream( impl::canvas_t& ) const {
		}

		friend impl::canvas_t& operator<<(impl::canvas_t& cs, const io_t<Derived>& attr){
				const Derived& derived = static_cast<const Derived&>(attr);
				derived.ostream( cs );
				return cs;
			}
	};
}

inline bench::plot::impl::canvas_t::canvas_t( std::ostream& os, std::size_t width, std::size_t height, const std::array<float,4>& margin )
	: width(width), height(height), margin(margin), os(os) {

	os << fmt( svg_start_, 0,0, width,height);
}

inline bench::plot::impl::canvas_t::~canvas_t(){
	os << svg_end_;
}


inline void bench::plot::impl::canvas_t::group (std::variant<std::size_t, align_t> x, std::variant<std::size_t, align_t> y,
		const attribute_t& attr, std::function<void()> const& call ){
	float _x = align_x( x ); float _y = align_y( y, attr.font );

	std::string _attr = fmt(transform_, fmt(translate_, _x, _y));
	if( attr.font )   _attr += fmt(font_, attr.font->size, attr.font->family, attr.font->weight);
	if( attr.color )  _attr += fmt(fill_, static_cast<unsigned>(attr.color.value()) );

	os << fmt(gr_begin_, _attr); // group has properties set
		call();
	os << gr_end_;
}

inline std::pair<std::size_t,std::size_t>
bench::plot::impl::canvas_t::bounding_box( const std::vector<std::string>&, double,
	   	const std::vector<std::string>&, double,  std::size_t ){

	return std::make_pair(std::size_t{0}, std::size_t{0});
}

inline void bench::plot::impl::canvas_t::rect(float x, float y, float width, float height, float rx, float ry,
		const attribute_t& attr){
	std::string _attr, _lbl;
	if( attr.color )  _attr += fmt(fill_, static_cast<unsigned>(attr.color.value()) );
	if( attr.label )  _lbl = fmt(title_, util::html_escape(attr.label.value()));

	if( attr.href ) os << fmt(href_begin_, attr.href.value());
		os << fmt(rect_, x, y, width, height, rx, ry, _attr, _lbl );
	if( attr.href ) os << fmt(href_end_);
}

inline void bench::plot::impl::canvas_t::circle(float cx, float cy, float radius, const attribute_t& attr){
	std::string _attr, _lbl;
	if( attr.color )  _attr += fmt(fill_, static_cast<unsigned>(attr.color.value()) );
	if( attr.stroke ) _attr += fmt(stroke_attr_, attr.color ? static_cast<unsigned>(attr.color.value()) : 0u, attr.stroke->width);
	if( attr.label )  _lbl = fmt(title_, util::html_escape(attr.label.value()));

	if( attr.href ) os << fmt(href_begin_, attr.href.value());
		os << fmt(circle_, cx, cy, radius, _attr, _lbl );
	if( attr.href ) os << fmt(href_end_);
}

inline void bench::plot::impl::canvas_t::line(float x1, float y1, float x2, float y2, const attribute_t& attr){
	const unsigned color = attr.color ? static_cast<unsigned>(attr.color.value()) : 0u;
	const float w = attr.stroke ? attr.stroke->width : 1.0f;
	os << fmt(line_, x1, y1, x2, y2, fmt(stroke_attr_, color, w));
}

inline void bench::plot::impl::canvas_t::poly_line(const std::vector<float>& x, const std::vector<float>& y,
		const attribute_t& attr){
	const unsigned color = attr.color ? static_cast<unsigned>(attr.color.value()) : 0u;
	const float w = attr.stroke ? attr.stroke->width : 1.0f;
	std::string points;
	const std::size_t n = x.size() < y.size() ? x.size() : y.size();
	for(std::size_t i=0; i<n; i++){
		if(i) points += ' ';
		points += std::format("{:.2f},{:.2f}", x[i], y[i]);
	}
	os << fmt(polyline_, points, fmt(stroke_attr_, color, w));
}

inline void bench::plot::impl::canvas_t::text( const std::string& txt,
		std::variant<std::size_t, align_t> x, std::variant<std::size_t, align_t> y, const attribute_t& attr ){
	float _x = align_x( x ); float _y = align_y( y, attr.font );
	int i = std::holds_alternative<std::size_t>( x ) ? 0 : static_cast<int>(std::get<align_t>(x));

	std::string _attr;
	if( attr.font )   _attr += fmt(font_, attr.font->size, attr.font->family, attr.font->weight);
	if( attr.color )  _attr += fmt(fill_, static_cast<unsigned>(attr.color.value()) );
	if( attr.rotate ) _attr += fmt(transform_, fmt(rotate_, attr.rotate->value, _x, _y));
	_attr += fmt(text_anchor_, align_horizontal[i]);

	os << fmt(text_, _x, _y, _attr, util::html_escape(txt) );
}

inline void bench::plot::impl::canvas_t::align( std::optional<align_t> x ) {
	if( !x ) return;
		os << fmt(text_anchor_, align_horizontal[ static_cast<int>(x.value()) ]);
}

inline float bench::plot::impl::canvas_t::align_x( std::variant<std::size_t, align_t> x ) {
	if( std::holds_alternative<std::size_t>( x ) )
		return static_cast<float>(std::get<std::size_t>( x ));
	switch( std::get<align_t>(x) ){
		case align_t::left: return std::get<0>(margin);
		case align_t::right: return width - std::get<2>(margin);
		case align_t::center: return width / 2 - std::get<0>(margin);
		default: throw std::runtime_error("fixme: #169");
	}
	return std::get<0>(margin);
}

inline float bench::plot::impl::canvas_t::align_y( std::variant<std::size_t, align_t> y, const std::optional<font_t>& font ) {
	if( std::holds_alternative<std::size_t>( y ) )
		return static_cast<float>(std::get<std::size_t>( y ));
	switch( std::get<align_t>(y) ){
		case align_t::top: return std::get<1>(margin) + (font ? font->size : 0);
		case align_t::bottom: return height - std::get<3>(margin);
		case align_t::center: return height / 2 - std::get<1>(margin);
		default:
			throw std::runtime_error("fixme: 182");
			break;
	}
	return std::get<1>(margin);
}
#endif
