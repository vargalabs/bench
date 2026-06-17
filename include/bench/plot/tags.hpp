/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Empty tag types used to identify the order-independent named arguments of the
 * SVG plotting layer. Ported from plot:: into bench::plot::. Dependency-free.
 */
#ifndef BENCH_PLOT_TAGS_HPP
#define BENCH_PLOT_TAGS_HPP

namespace bench::plot::tag {
	struct width_t{};
	struct height_t{};
	struct position_t{};
	struct font_t{};
	struct color_t{};
	struct layout_t{};
	struct stroke_t{};
	struct opacity_t{};
	struct marker_t{};
	namespace axis {
		struct x_t{};
		struct y_t{};
	}
	struct text_t{};
	struct title_t{};
	struct footnote_t{};
	struct legend_t{};
	struct id_t{};
	struct style_t{};
	struct degree_t{};
	struct radian_t{};
	struct cw_t{};
	struct ccw_t{};
	struct horizontal_t{};
	struct vertical_t{};
	struct margin_t{};
}
#endif
