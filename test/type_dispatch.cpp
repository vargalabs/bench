/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Type-axis test: the compile-time `throughput<std::tuple<Ts...>>(...)` overload
 * folds a generic benchmark body over a typelist and emits one result row per
 * (type, x-point), with the type label distinguishing the rows.
 */
#include <bench/all>
#include <vector>
#include <cstddef>
#include <string>
#include <string_view>

int main() {
	bench::arg_x sizes{1'000, 10'000, 100'000};
	std::size_t before_calls = 0, after_calls = 0;

	constexpr std::size_t n_types  = 3;          // int, double, float
	const std::size_t     n_points = sizes.size();
	constexpr std::uint16_t wu = 2, su = 5;

	std::vector<unsigned char> buf(100'000 * sizeof(double), 1);

	// args intentionally out of order; the type axis is an explicit tuple arg.
	bench::throughput<std::tuple<int, double, float>>(
		bench::sample{su}, sizes, bench::warmup{wu},
		bench::before_sample{ [&]{ ++before_calls; } },
		bench::after_sample{  [&]{ ++after_calls;  } },
		bench::name{"typed"},
		[&]<class T>(std::size_t /*idx*/, std::size_t n) -> double {
			const std::size_t bytes = n * sizeof(T);
			volatile T acc = T{};
			const T* data = reinterpret_cast<const T*>(buf.data());
			const std::size_t count = buf.size() / sizeof(T);
			for (std::size_t i = 0; i < n; ++i) acc += data[i % count];
			(void)acc;
			return static_cast<double>(bytes);
		});

	const auto& r = bench::store_t::get().results();

	// one row per (type, x-point)
	if (r.size() != n_types * n_points) return 1;

	// hooks fired once per sample across every (type, x-point)
	if (before_calls != n_types * n_points * su) return 2;
	if (after_calls  != n_types * n_points * su) return 3;

	// sane, non-negative stats and the type label is carried in the name
	for (const auto& row : r) {
		if (!(row.mean_runtime    >= 0.0)) return 4;
		if (!(row.std_runtime     >= 0.0)) return 5;
		if (!(row.mean_throughput >= 0.0)) return 6;
		if (!(row.std_throughput  >= 0.0)) return 7;
		if (row.warmup != wu || row.sample != su) return 8;
		const std::string_view nm(row.name);
		if (nm.find("typed/") != 0) return 9;       // name prefix + type suffix
	}

	// each declared type appears in at least one row label
	bool have_int = false, have_double = false, have_float = false;
	for (const auto& row : r) {
		const std::string_view nm(row.name);
		if (nm.find("int")    != std::string_view::npos) have_int    = true;
		if (nm.find("double") != std::string_view::npos) have_double = true;
		if (nm.find("float")  != std::string_view::npos) have_float  = true;
	}
	if (!(have_int && have_double && have_float)) return 10;

	return 0;
}
