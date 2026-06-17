/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * sinks — emit results somewhere other than the default stdout table.
 *
 * A `bench::sink` decides how each result_t row is rendered. The global store
 * holds one sink (default: bench::stdout_sink) and flushes every collected row
 * to it at program exit. `bench::set_sink(...)` swaps it.
 *
 * The CSV and JSON sinks are OPT-IN — they are not pulled in by <bench/all>,
 * so include their headers explicitly. Each borrows a std::ostream you own.
 *
 * This example runs one small benchmark, then:
 *   1. writes the collected rows to a CSV file and a JSON file by hand — the
 *      store keeps every row in memory and exposes them via
 *      bench::store_t::get().results(), so any sink can replay them; and
 *   2. swaps the GLOBAL sink with bench::set_sink(...) so the automatic
 *      program-exit flush re-emits the same rows through it (here a CSV sink
 *      writing to std::cout, to show set_sink in action with a safe lifetime).
 *
 * Note on lifetime: the store is a singleton flushed during static destruction,
 * so a sink handed to set_sink must borrow a stream that outlives the store
 * (std::cout is safe). That is why the file outputs above are written eagerly
 * rather than deferred to the exit flush.
 */
#include <bench/all>
#include <bench/sink/csv.hpp>
#include <bench/sink/json.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

int main() {
	bench::arg_x sizes{ 10'000, 100'000 };
	std::vector<int> buf(100'000);

	bench::throughput(
		bench::name{"fill"},
		sizes,
		bench::warmup{3}, bench::sample{20},
		[&](std::size_t /*idx*/, std::size_t n) -> double {
			for (std::size_t i = 0; i < n; ++i) buf[i] = static_cast<int>(i);
			volatile int sink = buf[n - 1];
			(void)sink;
			return static_cast<double>(n * sizeof(int));
		});

	// --- replay the collected rows into a CSV file and a JSON file -----------
	// The store buffers every row; pull them out and feed each opt-in sink.
	const std::vector<bench::result_t>& rows = bench::store_t::get().results();
	{
		std::ofstream csv_out("sinks_results.csv");
		bench::csv_sink csv(csv_out);
		for (const bench::result_t& r : rows) csv.write(r);
		csv.flush();
	}
	{
		std::ofstream js_out("sinks_results.json");
		bench::json_sink js(js_out);
		for (const bench::result_t& r : rows) js.write(r);
		js.flush();
	}
	std::cout << "wrote sinks_results.csv and sinks_results.json\n";

	// --- swap the GLOBAL sink so the program-exit flush uses CSV-on-stdout ---
	// set_sink replaces the store's sink; the store re-emits all rows through
	// it when it is destroyed at exit. std::cout outlives the store, so the
	// borrowed stream is safe.
	bench::set_sink(std::make_shared<bench::csv_sink>(std::cout));
	std::cout << "the same rows will also print as CSV below at exit:\n";

	return 0;
}
