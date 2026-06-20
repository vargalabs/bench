/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Metric-action smoke test: runtime, throughput, rate, and latency all compile
 * with typed units, emit rows, and carry report metadata.
 */
#include <bench/all>

#include <cstddef>
#include <string_view>
#include <vector>

namespace {

	bool row_is(const bench::result_t& row, std::string_view metric, std::string_view unit, bench::direction_t direction){
		return std::string_view(row.metric) == metric
			&& std::string_view(row.unit) == unit
			&& row.direction == direction
			&& row.mean_runtime >= 0.0
			&& row.mean_metric >= 0.0
			&& row.std_metric >= 0.0;
	}
}

int main() {
	bench::arg_x sizes{100, 1'000};
	std::vector<int> data(1'000, 1);

	bench::runtime(
		bench::name{"runtime"},
		sizes,
		bench::warmup{1}, bench::sample{2},
		bench::unit{bench::units::ms},
		[&](std::size_t, std::size_t n) {
			volatile int acc = 0;
			for(std::size_t i = 0; i < n; ++i) acc += data[i % data.size()];
			(void)acc;
			return bench::ok{};
		});

	bench::throughput(
		bench::name{"throughput"},
		sizes,
		bench::warmup{1}, bench::sample{2},
		bench::unit{bench::units::MB / bench::units::s},
		[&](std::size_t, std::size_t n) {
			volatile int acc = 0;
			for(std::size_t i = 0; i < n; ++i) acc += data[i % data.size()];
			(void)acc;
			return n * sizeof(int) * bench::units::B;
		});

	bench::rate(
		bench::name{"rate"},
		sizes,
		bench::warmup{1}, bench::sample{2},
		bench::unit{bench::units::op / bench::units::s},
		[&](std::size_t, std::size_t n) {
			volatile int acc = 0;
			for(std::size_t i = 0; i < n; ++i) acc += data[i % data.size()];
			(void)acc;
			return n * bench::units::op;
		});

	bench::latency(
		bench::name{"latency"},
		sizes,
		bench::warmup{1}, bench::sample{2},
		bench::unit{bench::units::us / bench::units::op},
		[&](std::size_t, std::size_t n) {
			volatile int acc = 0;
			for(std::size_t i = 0; i < n; ++i) acc += data[i % data.size()];
			(void)acc;
			return n * bench::units::op;
		});

	const auto& rows = bench::store_t::get().results();
	if(rows.size() != sizes.size() * 4) return 1;

	if(!row_is(rows[0], "runtime", "ms", bench::direction_t::lower_is_better)) return 2;
	if(!row_is(rows[2], "throughput", "MB/s", bench::direction_t::higher_is_better)) return 3;
	if(!row_is(rows[4], "rate", "op/s", bench::direction_t::higher_is_better)) return 4;
	if(!row_is(rows[6], "latency", "us/op", bench::direction_t::lower_is_better)) return 5;
	return 0;
}
