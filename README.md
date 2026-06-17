# bench

> Header-only C++ micro-benchmark library with compile-time type-axis dispatch.

[![CI](https://github.com/vargalabs/bench/actions/workflows/ci.yml/badge.svg)](https://github.com/vargalabs/bench/actions/workflows/ci.yml)
[![MIT License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Documentation](https://img.shields.io/badge/docs-stable-blue)](https://vargalabs.github.io/bench/)

| OS / Compiler | GCC 13        | GCC 14        | Clang 18     | Clang 20     | Apple Clang | MSVC         |
|---------------|---------------|---------------|--------------|--------------|-------------|--------------|
| Ubuntu 22.04  | ![gcc13][200] | ![NA][NA]     | ![cl18][201] | ![NA][NA]    | ![NA][NA]   | ![NA][NA]    |
| Ubuntu 24.04  | ![NA][NA]     | ![gcc14][300] | ![NA][NA]    | ![cl20][301] | ![NA][NA]   | ![NA][NA]    |
| macOS 15      | ![NA][NA]     | ![NA][NA]     | ![NA][NA]    | ![NA][NA]    | ![ac][400]  | ![NA][NA]    |
| Windows       | ![NA][NA]     | ![NA][NA]     | ![NA][NA]    | ![NA][NA]    | ![NA][NA]   | ![msvc][500] |

> Note: the badge images and the documentation link are served from GitHub
> Pages. As `bench` is a private repository, those URLs only resolve once Pages
> is enabled for it (Settings → Pages → deploy from the `gh-pages` branch).

## Quick start

```cpp
#include <bench/all>
#include <tuple>

int main() {
  bench::arg_x record_size{10'000, 100'000, 1'000'000};

  // one compile-time type axis, crossed with a runtime size sweep
  bench::throughput<std::tuple<int, double, float>>(
    bench::name{"write"}, record_size,           // named args, any order
    bench::warmup{3}, bench::sample{10},
    [&]<class T>(std::size_t idx, std::size_t n) -> double {
      // ... do work for n elements of T ...
      return n * sizeof(T);                       // bytes moved -> throughput
    });
}
```

One measured row per `(type, size)` pair, with mean/stddev for runtime and
throughput.

## Features

- **Compile-time type-axis dispatch** — `bench::throughput<std::tuple<Ts...>>`
  folds the *same* generic-lambda body over a compile-time type list via
  `static_for`, each type fully specialized (no type erasure), yielding one row
  per `(type, size)`.
- **Order-independent named args** — `bench::name`, `bench::arg_x` (size sweep),
  `bench::warmup`, `bench::sample`, and the `bench::before_sample` /
  `bench::after_sample` hooks may be passed in any order.
- **Pluggable sinks** — the default `bench::stdout_sink` prints a table; opt-in
  `bench::csv_sink` and `bench::json_sink` emit to any `std::ostream`, and the
  opt-in `bench::svg_sink` renders heatmaps via the external `plot` library.
  Swap with `bench::set_sink(...)`.
- **Dependency-free core** — no HDF5, zlib, or boost; just a C++23 toolchain.

## Build & install

`bench` is header-only — copy `include/bench` onto your include path, or consume
it via CMake:

```cmake
find_package(bench REQUIRED)
target_link_libraries(my_app PRIVATE bench::bench)
```

Build the tests and examples from source:

```bash
git clone https://github.com/vargalabs/bench.git
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBENCH_BUILD_TESTS=ON -DBENCH_BUILD_EXAMPLES=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The opt-in `bench::svg_sink` needs the external `plot` library
(`vargalabs/plots`) via `find_package(plot)`. When `plot` is absent the SVG
example/test auto-skip and the core still builds, configures, and tests cleanly.

## Examples

Self-contained, runnable programs live in [`examples/`](examples/) and build by
default (toggle with `-DBENCH_BUILD_EXAMPLES=OFF`):

| Example | Shows |
| --- | --- |
| `memcpy_bandwidth` | scalar `throughput` over an `arg_x` size sweep; default stdout table |
| `type_dispatch` | `bench::throughput<std::tuple<...>>` running one body across several element types |
| `sinks` | emitting results to CSV and JSON via `csv_sink` / `json_sink` and `set_sink(...)` |
| `svg_sink` | rendering a throughput heatmap via the opt-in `plot`-backed SVG sink |

## Documentation

Full API reference and examples: [vargalabs.github.io/bench](https://vargalabs.github.io/bench/)

## License

MIT — see [LICENSE](LICENSE).

[NA]: https://vargalabs.github.io/bench/badges/na.svg

<!-- Ubuntu 22.04 -->
[200]: https://vargalabs.github.io/bench/badges/ubuntu-22.04-gcc-13.svg
[201]: https://vargalabs.github.io/bench/badges/ubuntu-22.04-clang-18.svg

<!-- Ubuntu 24.04 -->
[300]: https://vargalabs.github.io/bench/badges/ubuntu-24.04-gcc-14.svg
[301]: https://vargalabs.github.io/bench/badges/ubuntu-24.04-clang-20.svg

<!-- macOS 15 -->
[400]: https://vargalabs.github.io/bench/badges/macos-15-apple-clang.svg

<!-- Windows -->
[500]: https://vargalabs.github.io/bench/badges/windows-latest-msvc.svg
