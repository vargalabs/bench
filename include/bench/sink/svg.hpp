/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Opt-in SVG result sink. Buffers result rows and, on flush()/destruction,
 * pivots them into a 2-D grid and renders a continuous-gradient heatmap of a
 * chosen metric (mean_throughput by default) via the dependency-free
 * bench::plot layer.
 *
 *   - The type axis is the suffix of result_t::name after '/', matching rows
 *     emitted by the type-dispatch driver ("<name>/<typelabel>"); rows without
 *     a '/' collapse onto a single "value" type row.
 *   - The size axis is result_t::x.
 *   - Produces a standalone, well-formed .svg (starts "<svg", ends "</svg>").
 *
 * Header-only and dependency-free — include <bench/sink/svg.hpp> explicitly
 * (NOT in <bench/all>).
 */
#ifndef BENCH_SINK_SVG_HPP
#define BENCH_SINK_SVG_HPP

#include <ostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstddef>

#include "../bench.hpp"
#include "../plot/all"

namespace bench {

	// Buffers results, then renders one continuous heatmap on flush()/destruction.
	struct svg_sink : sink {
		// which metric drives the colour of each cell
		enum class metric { mean_throughput, std_throughput, mean_runtime, std_runtime };

		explicit svg_sink(std::string filename = "report.svg", metric m = metric::mean_throughput)
			: filename_(std::move(filename)), metric_(m) {}
		// render into a borrowed stream instead of a file
		explicit svg_sink(std::ostream& os, metric m = metric::mean_throughput)
			: os_(&os), metric_(m) {}

		~svg_sink() override { flush(); }

		void write(const result_t& r) override { rows_.push_back(r); }

		void flush() override {
			if(emitted_) return;
			emitted_ = true;
			if(os_) { render(*os_); }
			else {
				std::ofstream ofs(filename_, std::ios::out | std::ios::trunc);
				render(ofs);
			}
		}

		private:
			// split "name/typelabel" -> {typelabel}; no '/' -> {"value"}.
			static std::string type_of(const char* name){
				std::string s(name);
				const auto pos = s.rfind('/');
				if(pos == std::string::npos) return "value";
				return s.substr(pos + 1);
			}

			double value_of(const result_t& r) const {
				switch(metric_){
					case metric::std_throughput: return r.std_throughput;
					case metric::mean_runtime:   return r.mean_runtime;
					case metric::std_runtime:    return r.std_runtime;
					case metric::mean_throughput:
					default:                     return r.mean_throughput;
				}
			}

			static const char* metric_label(metric m){
				switch(m){
					case metric::std_throughput: return "std MB/s";
					case metric::mean_runtime:   return "mean us";
					case metric::std_runtime:    return "std us";
					case metric::mean_throughput:
					default:                     return "mean MB/s";
				}
			}

			void render(std::ostream& os){
				// ---- pivot: discover the type axis (rows) and x axis (cols) -----
				std::vector<std::string>   types;   // y axis labels
				std::vector<std::uint64_t> xs;       // x axis values
				for(const auto& r : rows_){
					std::string t = type_of(r.name);
					if(std::find(types.begin(), types.end(), t) == types.end())
						types.push_back(t);
					if(std::find(xs.begin(), xs.end(), r.x) == xs.end())
						xs.push_back(r.x);
				}
				std::sort(xs.begin(), xs.end());

				// degenerate input: still emit a valid (empty) svg document.
				if(types.empty() || xs.empty()){
					bench::plot::impl::canvas_t canvas(os, 64, 32, {5,5,5,5});
					return; // dtor writes </svg>
				}

				const std::size_t rows = types.size();
				const std::size_t cols = xs.size();

				// dense grid in [type][x] order, row-major for bench::plot::mat.
				std::vector<double> grid(rows * cols, 0.0);
				for(const auto& r : rows_){
					const std::string t = type_of(r.name);
					const auto ti = std::find(types.begin(), types.end(), t) - types.begin();
					const auto xi = std::find(xs.begin(), xs.end(), r.x) - xs.begin();
					grid[static_cast<std::size_t>(ti) * cols + static_cast<std::size_t>(xi)] = value_of(r);
				}

				// axis tick labels
				std::vector<std::string> x_labels;
				x_labels.reserve(cols);
				for(auto v : xs) x_labels.push_back(std::to_string(v));

				bench::plot::mat<double> m{ grid.data(), rows, cols };

				// title carries the metric being coloured; place it within the canvas.
				const bench::plot::position title_pos{ std::size_t{4}, std::size_t{10} };
				bench::plot::heatmap(os, m,
					bench::plot::axis::x(x_labels),
					bench::plot::axis::y(types),
					bench::plot::title(std::string("bench: ") + metric_label(metric_), title_pos) );
			}

			std::string filename_ = "report.svg";
			std::ostream* os_ = nullptr;
			metric metric_ = metric::mean_throughput;
			std::vector<result_t> rows_;
			bool emitted_ = false;
	};
}

#endif
