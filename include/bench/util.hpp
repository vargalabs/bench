/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Test-data generators. Header-only, dependency-free (standard library only).
 *
 * The single entry point is `bench::util::get_test_data<T>(count, seed, width)`
 * which returns a deterministic `std::vector<T>` of generated test data. Given
 * the same (count, seed, width) it produces byte-identical output on every run,
 * so benchmark inputs are fully reproducible.
 *
 *   - arithmetic T (int/float/double/...): seeded std::mt19937; `width` ignored.
 *   - std::string                        : `width` is the max/target length of
 *                                          each generated printable-ASCII string.
 */
#ifndef BENCH_UTIL_HPP
#define BENCH_UTIL_HPP

#include <cstddef>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace bench::util {

	namespace detail {
		// Printable ASCII range used for generated strings: '!' (33) .. '~' (126).
		inline constexpr char ascii_lo = '!';
		inline constexpr char ascii_hi = '~';
		inline constexpr std::size_t default_string_width = 16;
	} // namespace detail

	// Primary template handles arithmetic T.  std::string is split off below via
	// `if constexpr`, keeping a single user-facing signature.
	template<class T>
	std::vector<T> get_test_data(std::size_t count, unsigned seed = 0, std::size_t width = 0) {
		static_assert(std::is_arithmetic_v<T> || std::is_same_v<T, std::string>,
			"bench::util::get_test_data supports arithmetic types and std::string");

		std::vector<T> out;
		out.reserve(count);
		std::mt19937 gen(seed);

		if constexpr (std::is_same_v<T, std::string>) {
			// width == 0 -> use a sensible default target length.
			const std::size_t target = (width == 0) ? detail::default_string_width : width;
			// Each string has a length in [1, target]; chars are printable ASCII.
			std::uniform_int_distribution<std::size_t> len_dist(1, target);
			std::uniform_int_distribution<int> chr_dist(detail::ascii_lo, detail::ascii_hi);
			for (std::size_t i = 0; i < count; ++i) {
				const std::size_t n = len_dist(gen);
				std::string s;
				s.reserve(n);
				for (std::size_t j = 0; j < n; ++j)
					s.push_back(static_cast<char>(chr_dist(gen)));
				out.push_back(std::move(s));
			}
		} else if constexpr (std::is_floating_point_v<T>) {
			std::uniform_real_distribution<T> dist(T(0), T(1));
			for (std::size_t i = 0; i < count; ++i)
				out.push_back(dist(gen));
		} else { // integral (incl. bool-ish via promotion)
			using limits = std::numeric_limits<T>;
			std::uniform_int_distribution<T> dist(limits::min(), limits::max());
			for (std::size_t i = 0; i < count; ++i)
				out.push_back(dist(gen));
		}
		return out;
	}

} // namespace bench::util

#endif
