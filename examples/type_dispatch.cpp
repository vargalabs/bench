/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * type_dispatch — the headline feature.
 *
 * A SINGLE call to the type-axis throughput driver runs the SAME benchmark
 * body across a compile-time list of element types. The body is a C++20
 * generic lambda; inside it `T` is the concrete element type for the current
 * fold, fully specialized (no type erasure). Crossed with the `arg_x` size
 * sweep this yields one measured row per (type, size) pair — rows are named
 * "<name>/<typelabel>" so the types stay distinguishable in the output.
 *
 * Input data is populated deterministically with bench::util::get_test_data<T>,
 * so the same run reproduces byte-identical inputs every time.
 */
#include <bench/all>

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

int main() {
	// Element counts (NOT bytes) — the body multiplies by sizeof(T) itself,
	// so each type reports the bytes it actually moves.
	bench::arg_x counts{ 1'000, 100'000, 1'000'000 };

	// the compile-time type axis: one row-group per type
	bench::throughput<std::tuple<std::uint8_t, std::uint32_t, double>>(
		bench::name{"sum"},
		counts,
		bench::warmup{3}, bench::sample{20},
		// generic body: invoked once per type with that type bound to T.
		[&]<class T>(std::size_t /*idx*/, std::size_t n) -> double {
			// Deterministic, reproducible input of n elements of T.
			const std::vector<T> data = bench::util::get_test_data<T>(n, /*seed=*/42);

			// Trivial reduction over the buffer; the volatile sink keeps the
			// optimizer from discarding the loop.
			long double acc = 0;
			for (std::size_t i = 0; i < n; ++i)
				acc += static_cast<long double>(data[i]);
			volatile long double sink = acc;
			(void)sink;

			// Bytes read for this (type, size) point.
			return static_cast<double>(n * sizeof(T));
		});

	// At program exit the table prints rows: sum/unsigned char, sum/unsigned int,
	// sum/double — each at every size in `counts`.
	return 0;
}
