# bench

A small, header-only C++ micro-benchmark library with **order-independent named
arguments** and **compile-time type-axis dispatch**.

`bench` is dependency-free: the core has no I/O, framework, or third-party
requirement. Its distinguishing feature is a `static_for`-based dispatch engine
that runs the *same* benchmark body across a compile-time list of types — each
fully specialized, no type erasure — crossed with runtime parameter sweeps.

```cpp
#include <bench/all>

namespace b = bench;

int main() {
  b::arg_x record_size{10'000, 100'000, 1'000'000};

  b::throughput(
    b::types<int, double, float>{},          // compile-time type axis
    b::name{"write"}, record_size,            // named args, any order
    b::warmup{3}, b::sample{10},
    [&]<class T>(std::size_t idx, std::size_t n) -> double {
      // ... do work for n elements of T ...
      return n * sizeof(T);                   // bytes moved -> throughput
    });
}
```

This produces one measured row per `(type, size)` pair, with mean/stddev for
runtime and throughput.

## Status

Green-field. Seeded from an HDF5-coupled proof-of-concept and being decoupled
into a general-purpose tool. See open issues for the active lanes.

## License

MIT — see [LICENSE](LICENSE).
