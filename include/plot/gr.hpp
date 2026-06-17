/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * GR.jl-style high-level drivers over the SVG canvas:
 *   - plot::line(os, xs, ys, opts...)              single auto-scaled series
 *   - plot::line(os, xs, {{"label",data},...}, …)  multi-series + legend
 *   - plot::scatter(os, xs, ys, opts...)           points via <circle>
 *
 * Shared named-argument options (same order-independent `tpos` dispatch the
 * heatmap uses): plot::title{}, plot::xlabel{}, plot::ylabel{}, plot::xlog{base},
 * plot::ylog{base}, plot::legend{}, plot::width{}, plot::height{}, plot::margin{},
 * plot::use{theme}. Themes drive every colour: a background rect is painted with
 * theme.bg, text/grid default to theme.fg/theme.grid, and series cycle through
 * theme.series. Dependency-free (standard library only).
 */
#ifndef PLOT_GR_HPP
#define PLOT_GR_HPP

#include <string>
#include <vector>
#include <array>
#include <tuple>
#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <ostream>
#include <format>

#include "tags.hpp"
#include "meta.hpp"
#include "attributes.hpp"
#include "canvas.hpp"
#include "theme.hpp"

namespace plot {
	// ---- GR-style named-argument option carriers ----------------------------
	struct xlabel { using value_type = tag::xlabel_t; std::string value; };
	struct ylabel { using value_type = tag::ylabel_t; std::string value; };
	// log-scale on an axis; `base` selects the logarithm base (default 10).
	struct xlog   { using value_type = tag::xlog_t;   double base = 10.0; };
	struct ylog   { using value_type = tag::ylog_t;   double base = 10.0; };
}

namespace plot::impl {
	// a labelled data series for the multi-series line() overload.
	struct series_t { std::string label; std::vector<double> data; };

	// pick the active theme: per-call plot::use{} override or the global default.
	template <class... arg_t>
	const theme_t& resolve_theme(arg_t... args){
		using theme_pos = typename arg::tpos<tag::theme_t, arg_t...>;
		if constexpr( theme_pos::present ){
			auto tuple = std::forward_as_tuple(args...);
			// stash the override so a reference outlives this scope: copy into a
			// function-local static is unsafe across threads, so return by value
			// is preferred — but the drivers only need a const ref for the render
			// duration, so we materialise into thread_local storage.
			thread_local theme_t picked;
			picked = std::get<theme_pos::position>(tuple).value;
			return picked;
		} else {
			return current_theme;
		}
	}

	// theme-driven continuous gradient: 3 stops, t in [0,1] -> 0xRRGGBB.
	inline std::uint32_t gradient3(const std::array<std::uint32_t,3>& g, double t){
		if( t < 0.0 ) t = 0.0; else if( t > 1.0 ) t = 1.0;
		auto chan = [](std::uint32_t c, int sh){ return double((c >> sh) & 0xFF); };
		auto lerp = [](double a, double b, double u){ return a + u*(b-a); };
		double r,gn,b;
		if( t < 0.5 ){
			double u = t / 0.5;
			r  = lerp(chan(g[0],16), chan(g[1],16), u);
			gn = lerp(chan(g[0], 8), chan(g[1], 8), u);
			b  = lerp(chan(g[0], 0), chan(g[1], 0), u);
		} else {
			double u = (t - 0.5) / 0.5;
			r  = lerp(chan(g[1],16), chan(g[2],16), u);
			gn = lerp(chan(g[1], 8), chan(g[2], 8), u);
			b  = lerp(chan(g[1], 0), chan(g[2], 0), u);
		}
		auto c8 = [](double v)->std::uint32_t {
			if( v < 0 ) v = 0; else if( v > 255 ) v = 255;
			return std::uint32_t(v + 0.5); };
		return (c8(r) << 16) | (c8(gn) << 8) | c8(b);
	}

	// "nice" tick step (1/2/5 * 10^k) covering [lo,hi] with ~count ticks.
	inline double nice_step(double range, int count){
		if( range <= 0 || count <= 0 ) return 1.0;
		double raw = range / count;
		double mag = std::pow(10.0, std::floor(std::log10(raw)));
		double norm = raw / mag;
		double step;
		if( norm < 1.5 )      step = 1;
		else if( norm < 3 )   step = 2;
		else if( norm < 7 )   step = 5;
		else                  step = 10;
		return step * mag;
	}

	// numeric axis tick model: min/max plus rendered tick label strings/positions.
	struct scale_t {
		double lo = 0, hi = 1;       // data domain (already log-transformed if log)
		bool   log = false;
		double base = 10.0;
		std::vector<double> ticks;   // tick positions in domain space
		std::vector<std::string> labels;

		// map a (already-transformed) value to [0,1] across the domain.
		double norm(double v) const {
			double span = (hi > lo) ? (hi - lo) : 1.0;
			return (v - lo) / span;
		}
		// transform a raw data value into domain space (log if enabled).
		double xform(double v) const {
			if( !log ) return v;
			double b = (base > 0 && base != 1.0) ? base : 10.0;
			double safe = v > 0 ? v : std::numeric_limits<double>::min();
			return std::log(safe) / std::log(b);
		}
	};

	inline std::string tick_label(double v){
		// integer-ish values print without a decimal point; else 4 sig figs.
		if( std::abs(v - std::llround(v)) < 1e-9 * (1.0 + std::abs(v)) )
			return std::format("{}", std::llround(v));
		return std::format("{:.4g}", v);
	}

	// build an axis scale from raw min/max, applying log transform + nice ticks.
	inline scale_t make_scale(double dmin, double dmax, bool log, double base){
		scale_t s; s.log = log; s.base = base;
		if( log ){
			double b = (base > 0 && base != 1.0) ? base : 10.0;
			double lmin = dmin > 0 ? dmin : std::numeric_limits<double>::min();
			double lmax = dmax > 0 ? dmax : std::numeric_limits<double>::min();
			s.lo = std::log(lmin)/std::log(b);
			s.hi = std::log(lmax)/std::log(b);
			if( s.hi <= s.lo ) s.hi = s.lo + 1.0;
			// nudge by an epsilon before floor/ceil: log10(1000) computes to
			// 2.9999999996, whose floor is 2 — which would add a spurious decade
			// (a 100 tick below a data minimum of 1000). The epsilon snaps it.
			double t0 = std::floor(s.lo + 1e-9), t1 = std::ceil(s.hi - 1e-9);
			s.lo = t0; s.hi = t1;
			for(double t = t0; t <= t1 + 0.5; t += 1.0){
				s.ticks.push_back(t);
				s.labels.push_back(tick_label(std::pow(b, t)));
			}
		} else {
			if( dmax <= dmin ){ dmax = dmin + 1.0; }
			double step = nice_step(dmax - dmin, 5);
			double t0 = std::floor(dmin/step)*step;
			double t1 = std::ceil(dmax/step)*step;
			s.lo = t0; s.hi = t1;
			for(double t = t0; t <= t1 + step*0.5; t += step){
				s.ticks.push_back(t);
				s.labels.push_back(tick_label(t));
			}
		}
		return s;
	}

	// shared core: lay out a cartesian panel (bg + panel + grid + axes + labels),
	// then invoke draw(canvas, px, py) where px/py map domain coords to pixels.
	// Used by both line() and scatter(). Callers pre-transform values (log etc.)
	// via scale_t::xform before calling px/py.
	template <class draw_fn>
	void render_panel(std::ostream& os, const theme_t& th,
			std::size_t width, std::size_t height, const std::array<float,4>& margin,
			const scale_t& sx, const scale_t& sy,
			const std::string& title, const std::string& xlab, const std::string& ylab,
			const std::vector<std::string>& legend_labels,
			draw_fn&& draw){
		using attribute_t = plot::attribute::element_t;
		using color_t = plot::attribute::color_t;
		using stroke_t = plot::attribute::stroke_t;

		canvas_t canvas(os, width, height, margin);

		// reserved gutters (px) for the axes/labels/title.
		const float left   = 56.0f;
		const float right  = 14.0f;
		const float top    = title.empty() ? 18.0f : 34.0f;
		const float bottom = 44.0f;
		const float x0 = left;
		const float y0 = top;
		const float pw = std::max(1.0f, float(width)  - left - right);
		const float ph = std::max(1.0f, float(height) - top - bottom);

		// background (theme.bg) over the whole canvas.
		{ attribute_t bg; bg.color = color_t{ th.bg };
		  canvas.rect(0,0, float(width), float(height), 0,0, bg); }
		// panel (theme.panel).
		{ attribute_t pn; pn.color = color_t{ th.panel };
		  canvas.rect(x0, y0, pw, ph, 2,2, pn); }

		auto px = [&](double v){ return x0 + float(sx.norm(v)) * pw; };
		auto py = [&](double v){ return y0 + ph - float(sy.norm(v)) * ph; };

		// grid + tick labels (theme.grid / theme.fg).
		attribute_t grid_attr; grid_attr.color = color_t{ th.grid };
		grid_attr.stroke = stroke_t{1.0f, 0.5f, {}, {}, {}};
		attribute_t tick_attr; tick_attr.color = color_t{ th.fg };
		tick_attr.font = plot::attribute::font_t{"Ubuntu Mono, monospace", "normal", 9u};

		// the bottom scale sits `label_gap` px below the x-axis; the left scale
		// uses the SAME gap from the y-axis. Tick text is left-anchored by the
		// canvas, so the y labels are placed by their (monospace) width so their
		// right edge lands `label_gap` px left of the axis — matching the bottom.
		const float label_gap = 14.0f;
		const float tick_char = 0.60f * 9.0f;   // Ubuntu Mono advance at 9px
		for(std::size_t i=0;i<sx.ticks.size();++i){
			float X = px(sx.ticks[i]);
			canvas.line(X, y0, X, y0+ph, grid_attr);
			attribute_t a = tick_attr; a.align = plot::attribute::align_t::center;
			canvas.text(sx.labels[i], std::size_t(X), std::size_t(y0+ph+label_gap), a);
		}
		for(std::size_t i=0;i<sy.ticks.size();++i){
			float Y = py(sy.ticks[i]);
			canvas.line(x0, Y, x0+pw, Y, grid_attr);
			attribute_t a = tick_attr; a.align = plot::attribute::align_t::right;
			float tx = x0 - label_gap - tick_char * float(sy.labels[i].size());
			canvas.text(sy.labels[i], std::size_t(tx < 0 ? 0 : tx), std::size_t(Y+3), a);
		}

		// axis frame (theme.axis).
		attribute_t axis_attr; axis_attr.color = color_t{ th.axis };
		axis_attr.stroke = stroke_t{1.0f, 1.2f, {}, {}, {}};
		canvas.line(x0, y0, x0, y0+ph, axis_attr);
		canvas.line(x0, y0+ph, x0+pw, y0+ph, axis_attr);

		// title + axis labels (theme.fg).
		if( !title.empty() ){
			attribute_t a; a.color = color_t{ th.fg };
			a.font = plot::attribute::font_t{"Arial, sans-serif", "bold", 13u};
			a.align = plot::attribute::align_t::center;
			canvas.text(title, std::size_t(x0 + pw/2), std::size_t(top-16), a);
		}
		if( !xlab.empty() ){
			attribute_t a; a.color = color_t{ th.fg };
			a.font = plot::attribute::font_t{"Arial, sans-serif", "normal", 10u};
			a.align = plot::attribute::align_t::center;
			canvas.text(xlab, std::size_t(x0 + pw/2), std::size_t(height-8), a);
		}
		if( !ylab.empty() ){
			attribute_t a; a.color = color_t{ th.fg };
			a.font = plot::attribute::font_t{"Arial, sans-serif", "normal", 10u};
			a.align = plot::attribute::align_t::center;
			a.rotate = plot::attribute::degree_t{270.0f};
			canvas.text(ylab, std::size_t(14), std::size_t(y0 + ph/2), a);
		}

		// the series payload.
		draw(canvas, px, py);

		// legend swatches (top-right inside the panel), cycling theme.series.
		if( !legend_labels.empty() ){
			float lx = x0 + pw - 110.0f;
			float ly = y0 + 12.0f;
			for(std::size_t i=0;i<legend_labels.size();++i){
				std::uint32_t c = th.series.empty() ? th.fg
								: th.series[i % th.series.size()];
				attribute_t sw; sw.color = color_t{ c };
				canvas.rect(lx, ly + float(i)*14.0f - 8.0f, 10, 10, 1,1, sw);
				attribute_t tx; tx.color = color_t{ th.fg };
				tx.font = plot::attribute::font_t{"Arial, sans-serif", "normal", 9u};
				tx.align = plot::attribute::align_t::left;
				canvas.text(legend_labels[i], std::size_t(lx+14),
						std::size_t(ly + float(i)*14.0f), tx);
			}
		}
	}

	// extract width/height/margin from named args, with sensible defaults.
	template <class... arg_t>
	std::tuple<std::size_t,std::size_t,std::array<float,4>>
	geometry(arg_t... args){
		using width_t  = typename arg::tpos<tag::width_t,  arg_t...>;
		using height_t = typename arg::tpos<tag::height_t, arg_t...>;
		using margin_t = typename arg::tpos<tag::margin_t, arg_t...>;
		auto tuple = std::forward_as_tuple(args...);
		std::size_t W = 640, H = 400;
		std::array<float,4> M{5,5,5,5};
		if constexpr( width_t::present )  W = std::get<width_t::position>(tuple).value;
		if constexpr( height_t::present ) H = std::get<height_t::position>(tuple).value;
		if constexpr( margin_t::present ) M = std::get<margin_t::position>(tuple).value;
		return {W,H,M};
	}

	template <class... arg_t>
	std::tuple<std::string,std::string,std::string>
	texts(arg_t... args){
		using title_t  = typename arg::tpos<tag::title_t,  arg_t...>;
		using xlabel_t = typename arg::tpos<tag::xlabel_t, arg_t...>;
		using ylabel_t = typename arg::tpos<tag::ylabel_t, arg_t...>;
		auto tuple = std::forward_as_tuple(args...);
		std::string t, xl, yl;
		if constexpr( title_t::present )  t  = std::get<title_t::position>(tuple).txt;
		if constexpr( xlabel_t::present ) xl = std::get<xlabel_t::position>(tuple).value;
		if constexpr( ylabel_t::present ) yl = std::get<ylabel_t::position>(tuple).value;
		return {t, xl, yl};
	}

	// log-flag/base extraction.
	template <class... arg_t>
	std::pair<bool,double> xlog_of(arg_t... args){
		using xlog_t = typename arg::tpos<tag::xlog_t, arg_t...>;
		auto tuple = std::forward_as_tuple(args...);
		if constexpr( xlog_t::present ) return {true, std::get<xlog_t::position>(tuple).base};
		else return {false, 10.0};
	}
	template <class... arg_t>
	std::pair<bool,double> ylog_of(arg_t... args){
		using ylog_t = typename arg::tpos<tag::ylog_t, arg_t...>;
		auto tuple = std::forward_as_tuple(args...);
		if constexpr( ylog_t::present ) return {true, std::get<ylog_t::position>(tuple).base};
		else return {false, 10.0};
	}

	template <class V>
	std::pair<double,double> minmax_of(const V& v){
		double lo = std::numeric_limits<double>::infinity();
		double hi = -std::numeric_limits<double>::infinity();
		for(auto e : v){ double d = double(e); if(d<lo) lo=d; if(d>hi) hi=d; }
		if( !(lo <= hi) ){ lo = 0; hi = 1; }
		return {lo, hi};
	}
}

namespace plot {
	// ---- single-series line --------------------------------------------------
	template <class X, class Y, class... arg_t>
	void line(std::ostream& os, const std::vector<X>& xs, const std::vector<Y>& ys, arg_t... args){
		const theme_t& th = impl::resolve_theme(args...);
		auto [W,H,M] = impl::geometry(args...);
		auto [title, xl, yl] = impl::texts(args...);
		auto [xlg, xb] = impl::xlog_of(args...);
		auto [ylg, yb] = impl::ylog_of(args...);

		auto [xmn,xmx] = impl::minmax_of(xs);
		auto [ymn,ymx] = impl::minmax_of(ys);
		impl::scale_t sx = impl::make_scale(xmn,xmx,xlg,xb);
		impl::scale_t sy = impl::make_scale(ymn,ymx,ylg,yb);

		impl::render_panel(os, th, W, H, M, sx, sy, title, xl, yl, {},
			[&](impl::canvas_t& cv, auto px, auto py){
				using attribute_t = plot::attribute::element_t;
				std::vector<float> xpx, ypx;
				const std::size_t n = std::min(xs.size(), ys.size());
				for(std::size_t i=0;i<n;++i){
					xpx.push_back(px(sx.xform(double(xs[i]))));
					ypx.push_back(py(sy.xform(double(ys[i]))));
				}
				attribute_t a;
				a.color = plot::attribute::color_t{ th.series.empty()? th.fg : th.series[0] };
				a.stroke = plot::attribute::stroke_t{1.0f, 1.8f, {}, {}, {}};
				cv.poly_line(xpx, ypx, a);
			});
	}

	// ---- multi-series line (legend) -----------------------------------------
	template <class X, class... arg_t>
	void line(std::ostream& os, const std::vector<X>& xs,
			const std::vector<impl::series_t>& series, arg_t... args){
		const theme_t& th = impl::resolve_theme(args...);
		auto [W,H,M] = impl::geometry(args...);
		auto [title, xl, yl] = impl::texts(args...);
		auto [xlg, xb] = impl::xlog_of(args...);
		auto [ylg, yb] = impl::ylog_of(args...);

		auto [xmn,xmx] = impl::minmax_of(xs);
		double ymn =  std::numeric_limits<double>::infinity();
		double ymx = -std::numeric_limits<double>::infinity();
		for(const auto& s : series){
			auto [a,b] = impl::minmax_of(s.data);
			if(a<ymn) ymn=a; if(b>ymx) ymx=b;
		}
		if( !(ymn<=ymx) ){ ymn=0; ymx=1; }
		impl::scale_t sx = impl::make_scale(xmn,xmx,xlg,xb);
		impl::scale_t sy = impl::make_scale(ymn,ymx,ylg,yb);

		std::vector<std::string> labels;
		for(const auto& s : series) labels.push_back(s.label);

		impl::render_panel(os, th, W, H, M, sx, sy, title, xl, yl, labels,
			[&](impl::canvas_t& cv, auto px, auto py){
				using attribute_t = plot::attribute::element_t;
				for(std::size_t k=0;k<series.size();++k){
					const auto& s = series[k];
					std::vector<float> xpx, ypx;
					const std::size_t n = std::min(xs.size(), s.data.size());
					for(std::size_t i=0;i<n;++i){
						xpx.push_back(px(sx.xform(double(xs[i]))));
						ypx.push_back(py(sy.xform(double(s.data[i]))));
					}
					std::uint32_t c = th.series.empty()? th.fg
									: th.series[k % th.series.size()];
					attribute_t a; a.color = plot::attribute::color_t{ c };
					a.stroke = plot::attribute::stroke_t{1.0f, 1.8f, {}, {}, {}};
					cv.poly_line(xpx, ypx, a);
				}
			});
	}

	// initializer-list convenience: plot::line(os, xs, {{"a",da},{"b",db}}, ...)
	template <class X, class... arg_t>
	void line(std::ostream& os, const std::vector<X>& xs,
			std::initializer_list<impl::series_t> series, arg_t... args){
		line(os, xs, std::vector<impl::series_t>(series), args...);
	}

	// ---- scatter -------------------------------------------------------------
	template <class X, class Y, class... arg_t>
	void scatter(std::ostream& os, const std::vector<X>& xs, const std::vector<Y>& ys, arg_t... args){
		const theme_t& th = impl::resolve_theme(args...);
		auto [W,H,M] = impl::geometry(args...);
		auto [title, xl, yl] = impl::texts(args...);
		auto [xlg, xb] = impl::xlog_of(args...);
		auto [ylg, yb] = impl::ylog_of(args...);

		auto [xmn,xmx] = impl::minmax_of(xs);
		auto [ymn,ymx] = impl::minmax_of(ys);
		impl::scale_t sx = impl::make_scale(xmn,xmx,xlg,xb);
		impl::scale_t sy = impl::make_scale(ymn,ymx,ylg,yb);

		impl::render_panel(os, th, W, H, M, sx, sy, title, xl, yl, {},
			[&](impl::canvas_t& cv, auto px, auto py){
				using attribute_t = plot::attribute::element_t;
				const std::size_t n = std::min(xs.size(), ys.size());
				std::uint32_t c = th.series.empty()? th.fg : th.series[0];
				for(std::size_t i=0;i<n;++i){
					attribute_t a; a.color = plot::attribute::color_t{ c };
					cv.circle(px(sx.xform(double(xs[i]))),
							  py(sy.xform(double(ys[i]))), 3.0f, a);
				}
			});
	}

	// ---- filename convenience overloads -------------------------------------
	template <class X, class Y, class... arg_t>
	void line(const std::string& filename, const std::vector<X>& xs, const std::vector<Y>& ys, arg_t... args){
		std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
		line(ofs, xs, ys, args...);
	}
	template <class X, class... arg_t>
	void line(const std::string& filename, const std::vector<X>& xs,
			std::initializer_list<impl::series_t> series, arg_t... args){
		std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
		line(ofs, xs, std::vector<impl::series_t>(series), args...);
	}
	template <class X, class Y, class... arg_t>
	void scatter(const std::string& filename, const std::vector<X>& xs, const std::vector<Y>& ys, arg_t... args){
		std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
		scatter(ofs, xs, ys, args...);
	}
}
#endif
