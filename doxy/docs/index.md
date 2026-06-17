@mainpage bench — Header-only C++ micro-benchmark library

> **Measure the same benchmark body across a compile-time list of types
> in one call — no macros, no per-type boilerplate, no type erasure.**
> &nbsp; &nbsp; *Order-independent named args; results to stdout, CSV, JSON,
> or an opt-in SVG heatmap.*

```cpp
#include <bench/all>
#include <tuple>
#include <vector>

int main() {
    bench::arg_x counts{ 1'000, 100'000, 1'000'000 };

    // one compile-time type axis -> one measured row-group per type
    bench::throughput<std::tuple<std::uint8_t, std::uint32_t, double>>(
        bench::name{"sum"},
        counts,
        bench::warmup{3}, bench::sample{20},
        // generic body: invoked once per type with that type bound to T
        [&]<class T>(std::size_t /*idx*/, std::size_t n) -> double {
            const std::vector<T> data = bench::util::get_test_data<T>(n, 42);
            long double acc = 0;
            for (std::size_t i = 0; i < n; ++i)
                acc += static_cast<long double>(data[i]);
            volatile long double sink = acc; (void)sink;
            return static_cast<double>(n * sizeof(T)); // bytes moved
        });
}
```

Crossing the compile-time type list with the `arg_x` size sweep yields one
measured row per `(type, size)` pair. Inside the generic lambda `T` is the
concrete element type for the current fold — fully specialized, no type
erasure — so each type reports the bytes it actually moves. At program exit
the active sink prints the rows: `sum/unsigned char`, `sum/unsigned int`,
`sum/double`, each at every size in `counts`.

---

# Features

- **Compile-time type-axis dispatch.** `bench::throughput<std::tuple<Ts...>>`
  folds a single generic-lambda body over a compile-time type list via the
  internal `static_for`, specializing the body per type with zero runtime
  dispatch.
- **Order-independent named arguments.** Pass `bench::name`, `bench::arg_x`,
  `bench::warmup`, and `bench::sample` in any order; each is a distinct tag
  type matched by type, not by position.
- **Pluggable sinks.** Results render to stdout (default), CSV
  (`bench::csv_sink`), or JSON (`bench::json_sink`) — install one with
  `bench::set_sink(...)`; the store flushes through it at teardown.
- **Opt-in SVG heatmap.** `#include <bench/sink/svg.hpp>` adds
  `bench::svg_sink`, which renders a continuous-gradient `(type, size)`
  heatmap via the external `plot::` library (found with `find_package(plot)`;
  the core builds and tests cleanly without it).

# Benchmark heatmap

The opt-in SVG sink colours each `(type, size)` cell by mean throughput.
The image below was produced by `examples/svg_sink.cpp` over
`<int, float, double>` across four sizes:

@image html benchmark_heatmap.svg "bench::svg_sink — mean MB/s per (type, size) cell" width=480px

# Sinks at a glance

| sink              | header                    | output                       |
| ----------------- | ------------------------- | ---------------------------- |
| stdout (default)  | `<bench/all>`             | aligned table on `std::cout` |
| `bench::csv_sink` | `<bench/sink/csv.hpp>`    | one CSV row per measurement  |
| `bench::json_sink`| `<bench/sink/json.hpp>`   | a JSON array of measurements |
| `bench::svg_sink` | `<bench/sink/svg.hpp>`    | the heatmap above (needs `plot::`) |
