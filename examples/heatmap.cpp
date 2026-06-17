/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * GR.jl-style heatmap: a synthetic throughput field (MB/s) over a payload-size ×
 * thread-count grid, rendered with the Solarized dark theme's continuous
 * blue->yellow->red gradient. Each cell carries its value as an SVG <title>
 * (hover tooltip). Writes heatmap.svg — open it in a browser.
 *
 * Build with -DBENCH_BUILD_EXAMPLES=ON; run; open heatmap.svg.
 * (For the benchmark-driven heatmap via bench::svg_sink, see svg_sink.cpp.)
 */
#include <plot/all>

#include <vector>
#include <string>
#include <cmath>
#include <cstddef>

int main(){
	plot::theme(plot::solarized_dark);   // per-call override also via plot::use{...}

	// axes: payload sizes (columns) and thread counts (rows).
	std::vector<std::string> sizes  { "1K", "4K", "16K", "64K", "256K", "1M" };
	std::vector<std::string> threads{ "1", "2", "4", "8", "16", "32" };
	const std::size_t cols = sizes.size();
	const std::size_t rows = threads.size();

	// synthetic throughput surface: rises with concurrency (diminishing returns)
	// and with payload size (saturating) — row-major [thread][size] for plot::mat.
	std::vector<double> z(rows * cols);
	for(std::size_t r = 0; r < rows; ++r){
		const double t = std::pow(2.0, double(r));               // 1..32 threads
		const double conc = t / (1.0 + 0.18 * t);                // saturating speedup
		for(std::size_t c = 0; c < cols; ++c){
			const double bytes = std::pow(4.0, double(c)) * 1024.0;
			const double size_factor = std::log2(bytes) / 20.0;  // larger = better
			z[r * cols + c] = 220.0 * conc * size_factor;        // ~MB/s
		}
	}

	plot::mat<double> field{ z.data(), rows, cols };
	const plot::position title_pos{ std::size_t{4}, std::size_t{10} };

	plot::heatmap(std::string("heatmap.svg"), field,
		plot::axis::x(sizes),
		plot::axis::y(threads),
		plot::title(std::string("throughput [MB/s]: payload size x threads"), title_pos));

	return 0;
}
