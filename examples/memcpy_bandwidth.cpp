/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * memcpy_bandwidth — the simplest possible bench: a memory-bandwidth sweep.
 *
 * Uses the *scalar* throughput driver. We sweep a set of buffer sizes via
 * `bench::arg_x{...}` and, for each size, copy a source buffer into a
 * destination buffer with std::memcpy. The body returns the number of bytes
 * touched (read + written), which the engine divides by the measured time to
 * report MiB/s. Output goes to the default stdout table sink.
 *
 * Dependency-free: only the bench core + standard library.
 */
#include <bench/all>

#include <cstddef>
#include <cstring>
#include <vector>

int main() {
	// x-axis: buffer sizes in bytes, small -> large (crosses cache levels).
	bench::arg_x sizes{ 4 * 1024, 64 * 1024, 1 * 1024 * 1024, 16 * 1024 * 1024 };

	// One source/destination pair, sized to the largest sweep point so the
	// body never reallocates inside the timed region.
	std::vector<char> src(16 * 1024 * 1024, 'x');
	std::vector<char> dst(src.size());

	bench::throughput(
		bench::name{"memcpy"},
		sizes,
		bench::warmup{5}, bench::sample{50},
		// idx = sweep index, n = the size in bytes for this point.
		[&](std::size_t /*idx*/, std::size_t n) -> double {
			std::memcpy(dst.data(), src.data(), n);
			// Defeat dead-store elimination by reading one byte back.
			volatile char sink = dst[n - 1];
			(void)sink;
			// Bytes moved across the bus: n read + n written.
			return static_cast<double>(2 * n);
		});

	// Results print as a table when the global store flushes at program exit.
	return 0;
}
