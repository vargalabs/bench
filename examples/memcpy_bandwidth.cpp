/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * memcpy_bandwidth — a type-aware memory-bandwidth sweep.
 *
 * Uses the *type-axis* throughput driver: the same body is folded over a
 * compile-time tuple of element widths, with a typed source/destination buffer
 * per type. We sweep a set of buffer sizes via `bench::arg_x{...}` and, for each
 * (type, size) point, copy a source buffer into a destination buffer with
 * std::memcpy. The body returns the number of bytes touched (read + written),
 * which the engine divides by the measured time to report MiB/s. Output goes to
 * the default stdout table sink.
 *
 * Dependency-free: only the bench core + standard library.
 */
#include <bench/all>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <vector>
int main() {
    using types = std::tuple<uint8_t, uint16_t, uint32_t, uint64_t>;
    constexpr std::size_t max_bytes = 16 * 1024 * 1024;
    bench::arg_x sizes{ 4*1024, 64*1024, 1*1024*1024, max_bytes };
    bench::throughput<types>(
        bench::name{"memcpy"}, sizes, bench::warmup{5}, bench::sample{50}, bench::unit{bench::units::MiB / bench::units::s},
        [&]<class T>(std::size_t /*idx*/, std::size_t n) {
            static std::vector<T> src(max_bytes/sizeof(T), T{1}), dst(src.size());
            std::memcpy(dst.data(), src.data(), n);
            volatile T sink = dst[n/sizeof(T) - 1]; (void)sink;
            return (2 * n) * bench::units::B;
        });
    return 0;
}
