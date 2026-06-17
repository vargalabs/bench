/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Demonstrates the opt-in bench::svg_sink: run a small type-dispatch throughput
 * benchmark over <int, float, double> across a few sizes, install the SVG sink,
 * and emit a continuous-gradient heatmap to report.svg at program exit.
 *
 * Build with -DBENCH_BUILD_EXAMPLES=ON; run, then open report.svg in a browser.
 */
#include <bench/all>
#include <bench/sink/svg.hpp>

#include <vector>
#include <cstddef>
#include <memory>

int main(){
	// colour each (type, size) cell by mean throughput; write report.svg.
	bench::set_sink(std::make_shared<bench::svg_sink>("report.svg"));

	bench::arg_x sizes{1'000, 10'000, 100'000, 1'000'000};

	std::vector<unsigned char> buf(1'000'000 * sizeof(double), 1);

	bench::throughput(
		bench::types<int, float, double>{},
		bench::name{"sum"},
		sizes,
		bench::warmup{2}, bench::sample{10},
		[&]<class T>(std::size_t /*idx*/, std::size_t n) -> double {
			const std::size_t bytes = n * sizeof(T);
			const T* data = reinterpret_cast<const T*>(buf.data());
			const std::size_t count = buf.size() / sizeof(T);
			volatile T acc = T{};
			for(std::size_t i = 0; i < n; ++i) acc += data[i % count];
			(void)acc;
			return static_cast<double>(bytes);
		});

	// the sink renders report.svg when bench::store_t is torn down at exit.
	return 0;
}
