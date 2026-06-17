/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Colour themes for the SVG plotting layer. A `theme_t` bundles the structural
 * colours (background, panel, foreground text, grid, axis), a categorical series
 * palette (cycled by line/scatter), and a 3-stop continuous gradient (wired into
 * the continuous heatmap). Two Solarized presets ship inline; a process-global
 * default current theme can be set with `plot::theme(...)`, and any driver call
 * may override it for a single render with the `plot::use{...}` named argument.
 * Dependency-free (standard library only).
 */
#ifndef PLOT_THEME_HPP
#define PLOT_THEME_HPP

#include <cstdint>
#include <array>
#include <vector>

#include "tags.hpp"

namespace plot {
	struct theme_t {
		std::uint32_t bg;                       // page / canvas background
		std::uint32_t panel;                    // plotting-area panel
		std::uint32_t fg;                       // default text / foreground
		std::uint32_t grid;                     // grid lines / axis ticks
		std::uint32_t axis;                     // axis lines
		std::vector<std::uint32_t> series;      // categorical palette (cycled)
		std::array<std::uint32_t,3> gradient;   // continuous 3-stop gradient
	};

	// Solarized (Ethan Schoonover) — exact hex.
	inline const theme_t solarized_dark = {
		/*bg   base03*/ 0x002b36,
		/*panel base02*/0x073642,
		/*fg   base0 */ 0x839496,
		/*grid base01*/ 0x586e75,
		/*axis base01*/ 0x586e75,
		/*series*/ { 0x268bd2, 0x859900, 0xb58900, 0xcb4b16,
		             0xdc322f, 0xd33682, 0x6c71c4, 0x2aa198 },
		/*gradient*/ { 0x268bd2, 0xb58900, 0xdc322f }
	};

	inline const theme_t solarized_light = {
		/*bg   base3 */ 0xfdf6e3,
		/*panel base2*/ 0xeee8d5,
		/*fg   base00*/ 0x657b83,
		/*grid base1 */ 0x93a1a1,
		/*axis base00*/ 0x657b83,
		/*series*/ { 0x268bd2, 0x859900, 0xb58900, 0xcb4b16,
		             0xdc322f, 0xd33682, 0x6c71c4, 0x2aa198 },
		/*gradient*/ { 0x268bd2, 0xb58900, 0xdc322f }
	};

	// process-global default current theme (header-only inline definition).
	inline theme_t current_theme = solarized_dark;

	// setter for the global default.
	inline void theme(const theme_t& t){ current_theme = t; }

	// per-call override: `plot::use{ plot::solarized_light }`. Carries a theme as
	// a named argument that line/scatter/heatmap honour over the global default.
	struct use {
		using value_type = tag::theme_t;
		theme_t value;
	};
}
#endif
