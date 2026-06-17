/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Exercises bench::util::get_test_data for arithmetic and string types:
 * size, determinism, and string-width constraints.
 */
#include <bench/all>
#include <cstddef>
#include <string>
#include <vector>

int main() {
	using bench::util::get_test_data;

	const std::size_t count = 1'000;
	const unsigned seed = 42;

	// --- size ---
	auto ints = get_test_data<int>(count, seed);
	if (ints.size() != count) return 1;

	auto dbls = get_test_data<double>(count, seed);
	if (dbls.size() != count) return 2;

	const std::size_t width = 12;
	auto strs = get_test_data<std::string>(count, seed, width);
	if (strs.size() != count) return 3;

	// --- determinism: same (count, seed, width) => identical output ---
	if (get_test_data<int>(count, seed) != ints) return 4;
	if (get_test_data<double>(count, seed) != dbls) return 5;
	if (get_test_data<std::string>(count, seed, width) != strs) return 6;

	// --- different seed => (very likely) different output ---
	if (get_test_data<int>(count, seed + 1) == ints) return 7;

	// --- string width respected: length in [1, width], printable ASCII ---
	for (const auto& s : strs) {
		if (s.empty() || s.size() > width) return 8;
		for (char c : s)
			if (c < '!' || c > '~') return 9;
	}

	// --- width default (0) still produces non-empty strings of bounded length ---
	auto def_strs = get_test_data<std::string>(8, seed);
	if (def_strs.size() != 8) return 10;
	for (const auto& s : def_strs)
		if (s.empty()) return 11;

	return 0;
}
