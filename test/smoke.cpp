/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Smoke test: the dependency-free core compiles and produces one result row
 * per x-axis point, with named args supplied out of order and the generic
 * sample hooks exercised.
 */
#include <bench/all>
#include <vector>
#include <cstddef>
#include <string_view>

int main() {
	bench::arg_x sizes{1'000, 10'000, 100'000};
	std::vector<char> buf(100'000 * sizeof(double), 1);
	std::size_t before_calls = 0, after_calls = 0;

	// args intentionally out of order to prove order-independent dispatch.
	bench::throughput(
		bench::sample{5}, sizes, bench::warmup{2},
		bench::before_sample{ [&]{ ++before_calls; } },
		bench::after_sample{  [&]{ ++after_calls;  } },
		bench::name{"memread"},
		[&](std::size_t /*idx*/, std::size_t n) {
			const std::size_t bytes = n * sizeof(double);
			volatile char acc = 0;
			for (std::size_t i = 0; i < bytes; ++i) acc ^= buf[i % buf.size()];
			(void)acc;
			return bytes * bench::units::B;
		});

	const auto& r = bench::store_t::get().results();
	if (r.size() != sizes.size()) return 1;           // one row per x point
	if (before_calls != 3 * 5 || after_calls != 3 * 5) return 2; // hooks fired
	for (const auto& row : r) {
		if (!(row.mean_runtime >= 0.0) || !(row.mean_throughput >= 0.0)) return 3;
		if (std::string_view(row.metric) != "throughput") return 4;
		if (std::string_view(row.unit) != "MiB/s") return 5;
	}
	return 0;
}
