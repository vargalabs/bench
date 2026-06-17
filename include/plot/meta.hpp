/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Bridge: the SVG plotting layer reuses the compile-time argument-dispatch
 * machinery already shipped in <bench/meta.hpp> (bench::meta::{arg,impl}) rather
 * than carrying a second copy. The plot headers were originally written against
 * `plot::arg` / `plot::impl`, so we re-export the equivalent bench::meta names
 * under plot. Pure C++ — no third-party or platform dependency.
 */
#ifndef PLOT_META_HPP
#define PLOT_META_HPP

#include <bench/meta.hpp>

namespace plot {
	// order-independent named-argument lookup (tpos / get / getn / required)
	namespace arg = bench::meta::arg;
}

namespace plot::impl {
	// feature detection + compile-time tuple iteration used by the plot headers
	using bench::meta::impl::compat::is_detected;
	using bench::meta::impl::has_value_type;
	using bench::meta::impl::get_value_type;
	using bench::meta::impl::is_value_type;
	using bench::meta::impl::static_for;
	namespace compat = bench::meta::impl::compat;
}

#endif
