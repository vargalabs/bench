# bench

A small, header-only C++ micro-benchmark library with **order-independent named
arguments** and **compile-time type-axis dispatch**.

`bench` is dependency-free: the core has no I/O, framework, or third-party
requirement. Its distinguishing feature is a `static_for`-based dispatch engine
that runs the *same* benchmark body across a compile-time list of types — each
fully specialized, no type erasure — crossed with runtime parameter sweeps.

```cpp
#include <bench/all>
#include <tuple>

int main() {
  bench::arg_x record_size{10'000, 100'000, 1'000'000};

  bench::throughput<std::tuple<int, double, float>>(  // compile-time type axis
    bench::name{"write"}, record_size,        // named args, any order
    bench::warmup{3}, bench::sample{10},
    [&]<class T>(std::size_t idx, std::size_t n) -> double {
      // ... do work for n elements of T ...
      return n * sizeof(T);                   // bytes moved -> throughput
    });
}
```

This produces one measured row per `(type, size)` pair, with mean/stddev for
runtime and throughput.

## Features

- **Engine** — a single timing loop (warmup + sampled runs, mean/stddev for
  runtime and throughput) driven by `bench::throughput(...)`. The body returns
  the bytes it moved; the engine reports MiB/s. Order-independent named args:
  `bench::name`, `bench::arg_x` (size sweep), `bench::warmup`, `bench::sample`,
  and the untimed/timed hooks `bench::before_sample` / `bench::after_sample`.
- **Type-axis dispatch** — the headline feature. `bench::throughput<`
  `std::tuple<Ts...>>(...)` folds the *same* generic-lambda body over a
  compile-time type list passed as an explicit template argument, each fully
  specialized, yielding one row per `(type, size)` (rows named
  `"<name>/<typelabel>"`).
- **Sinks** — results flow through a pluggable `bench::sink`. The default
  `bench::stdout_sink` prints a table; opt-in `bench::csv_sink` and
  `bench::json_sink` (in `<bench/sink/csv.hpp>` / `<bench/sink/json.hpp>`, not
  pulled in by `<bench/all>`) emit CSV/JSON to any `std::ostream`. Swap the
  active sink with `bench::set_sink(...)`; the in-memory store also replays all
  rows via `bench::store_t::get().results()`.
- **Util** — `bench::util::get_test_data<T>(count, seed, width)` returns a
  deterministic `std::vector<T>` (arithmetic types and `std::string`) so
  benchmark inputs are byte-reproducible across runs.

## Examples

Self-contained, runnable programs live in [`examples/`](examples/) and build by
default (toggle with `-DBENCH_BUILD_EXAMPLES=OFF`):

| Example | Shows |
| --- | --- |
| `memcpy_bandwidth.cpp` | scalar `throughput` over an `arg_x` size sweep; default stdout table |
| `type_dispatch.cpp` | `bench::throughput<std::tuple<...>>` running one body across several element types, fed by `get_test_data<T>` |
| `sinks.cpp` | emitting results to CSV and JSON via `csv_sink` / `json_sink` and `set_sink(...)` |

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/examples/memcpy_bandwidth
./build/examples/type_dispatch
./build/examples/sinks
```

## Status

Green-field. Seeded from an HDF5-coupled proof-of-concept and being decoupled
into a general-purpose tool. See open issues for the active lanes.

## License

MIT — see [LICENSE](LICENSE).
