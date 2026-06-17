/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Dependency-free micro-benchmark engine. Order-independent named arguments
 * dispatched through bench::meta. No I/O framework or platform dependency.
 */
#ifndef BENCH_BENCH_HPP
#define BENCH_BENCH_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <tuple>
#include <chrono>
#include <numeric>
#include <memory>
#include <utility>
#include "meta.hpp"

namespace bench::impl::tag {
	struct name_t {};
	struct warmup_t {};
	struct sample_t {};
	struct x_t {};
	struct y_t {};
	struct z_t {};
	struct before_sample_t {};
	struct after_sample_t {};
}

namespace bench::impl {

	template <class T, class tag_t>
	struct value_t {
		using value_type = tag_t;
		T value;
	};

	template <typename Tag, int N>
	struct array_t {
		using value_type = Tag;
		array_t(const std::initializer_list<std::size_t>& list ) : rank(0){
			for(std::size_t v: list) value[rank] = v, rank++;
		}
		array_t() : rank(0){};
		array_t( array_t&& arg ) = default;
		array_t( const array_t& arg ) = default;
		array_t& operator=( array_t&& arg ) = default;
		array_t& operator=( const array_t& arg ) = default;

		std::size_t& operator[](std::size_t i){ return *(value + i); }
		const std::size_t& operator[](std::size_t i) const { return *(value + i); }
		std::size_t* operator*() { return value; }
		const std::size_t* operator*() const { return value; }

		std::size_t size() const { return rank; }
		const std::size_t* begin()const { return value; }
		std::size_t* begin() { return value; }
		const std::size_t* end() const { return value+rank; }
		std::size_t* end() { return value+rank; }

		std::size_t value[N];
		std::size_t rank;
	};

	// Dependency-free, RTTI-free compile-time type name. Extracts the spelled-out
	// type from the compiler's pretty-function macro; falls back to an empty label
	// on unknown compilers (the caller then suffixes a numeric index instead).
	template <class T>
	constexpr std::string type_name() {
#if defined(__clang__)
		// "std::string bench::impl::type_name() [T = int]"
		constexpr const char* p = __PRETTY_FUNCTION__;
		std::string_view sv(p);
		const auto b = sv.find("[T = ");
		if (b == std::string_view::npos) return std::string();
		const auto s = b + 5;
		const auto e = sv.rfind(']');
		return std::string(sv.substr(s, e - s));
#elif defined(__GNUC__)
		// "constexpr std::string bench::impl::type_name() [with T = int; ...]"
		constexpr const char* p = __PRETTY_FUNCTION__;
		std::string_view sv(p);
		const auto b = sv.find("[with T = ");
		if (b == std::string_view::npos) return std::string();
		const auto s = b + 10;
		auto e = sv.find(';', s);
		if (e == std::string_view::npos) e = sv.rfind(']');
		return std::string(sv.substr(s, e - s));
#elif defined(_MSC_VER)
		// "...type_name<int>(void)"
		constexpr const char* p = __FUNCSIG__;
		std::string_view sv(p);
		const auto b = sv.find("type_name<");
		if (b == std::string_view::npos) return std::string();
		const auto s = b + 10;
		const auto e = sv.rfind('>');
		return std::string(sv.substr(s, e - s));
#else
		return std::string();
#endif
	}

	// mean / population-stddev. FIX vs the original POC: the accumulate seed must
	// be T{0} (a double), not the integer literal 0, which truncated the result.
	template <class T>
	std::pair<T,T> stats(const std::vector<T>& data){
		if (data.empty()) return {T{0}, T{0}};
		T mean = std::accumulate(data.begin(), data.end(), T{0}) / static_cast<T>(data.size());
		T var  = std::accumulate(data.begin(), data.end(), T{0}, [&](const T& s, const T& v ) -> T {
			T m = (v-mean);
			return s + m*m;
		}) / static_cast<T>(data.size());
		return std::pair<T,T>{mean, std::sqrt(var)};
	}
}

namespace bench {
	constexpr std::size_t max_dims = 32;
	namespace arg = bench::meta::arg;

	// ---- compile-time type axis -------------------------------------------
	// Marker carrying the list of types to fold the benchmark body over.
	template <class... Ts> struct types {};

	// ---- named, order-independent arguments -------------------------------
	using name   = impl::value_t<std::string,   impl::tag::name_t>;
	using warmup = impl::value_t<std::uint16_t, impl::tag::warmup_t>;
	using sample = impl::value_t<std::uint16_t, impl::tag::sample_t>;
	using arg_x  = impl::array_t<impl::tag::x_t, max_dims>;
	using arg_y  = impl::array_t<impl::tag::y_t, max_dims>;
	using arg_z  = impl::array_t<impl::tag::z_t, max_dims>;

	// generic reset/flush hooks — replace the old HDF5 `h5::flush(pt)` special
	// case. `before_sample` runs untimed (setup); `after_sample` runs inside the
	// timed region (e.g. to capture a flush/commit cost).
	using before_sample = impl::value_t<std::function<void()>, impl::tag::before_sample_t>;
	using after_sample  = impl::value_t<std::function<void()>, impl::tag::after_sample_t>;

	// ---- result record (plain POD; was the HDF5-registered test_t) ---------
	struct result_t {
		static constexpr std::size_t max_name = 64;
		std::uint16_t warmup;
		std::uint16_t sample;
		std::uint64_t x;
		double mean_runtime;     // microseconds
		double std_runtime;      // microseconds
		double mean_throughput;  // bytes / microsecond (== MB/s)
		double std_throughput;
		char   name[max_name];
	};

	// ---- pluggable result sink --------------------------------------------
	struct sink {
		virtual ~sink() = default;
		virtual void write(const result_t&) = 0;
		virtual void flush() {}
	};

	// default sink: the fixed-width table the POC printed (now dependency-free).
	struct stdout_sink : sink {
		bool header = false;
		void write(const result_t& r) override {
			if(!header){
				std::printf("[name                                              ]"
					"[total events][Mi events/s] [ms runtime / stddev] [    MiB/s / stddev ]\n");
				header = true;
			}
			std::printf("%-52s %12llu %12.4f  %11.2f  %8.3f %10.2f   %7.1f\n",
				r.name, static_cast<unsigned long long>(r.x),
				r.mean_runtime > 0 ? r.x / r.mean_runtime : 0.0,
				r.mean_runtime/1000.0, r.std_runtime/1000.0,
				r.mean_throughput, r.std_throughput);
		}
	};

	// ---- in-memory store; emits to the sink at program exit ----------------
	struct store_t {
		store_t(store_t const&)        = delete;
		void operator=(store_t const&) = delete;
		static store_t& get() { static store_t instance; return instance; }

		void set_sink(std::shared_ptr<sink> s){ sink_ = std::move(s); }
		void push(const result_t& data){ list_.push_back(data); }

		template<class V, class T>
		void push(const std::string& nm, std::uint64_t x, V wu, V su,
				  const std::pair<T,T>& time, const std::pair<T,T>& tp ) {
			result_t item{};
			item.warmup = wu;
			item.sample = su;
			item.x = x;
			item.mean_runtime = time.first;
			item.std_runtime  = time.second;
			item.mean_throughput = tp.first;
			item.std_throughput  = tp.second;
			std::strncpy(item.name, nm.data(), result_t::max_name - 1);
			list_.push_back(item);
		}

		const std::vector<result_t>& results() const { return list_; }

		private:
			store_t() : sink_(std::make_shared<stdout_sink>()) {}
			~store_t(){
				for(const auto& r: list_) sink_->write(r);
				sink_->flush();
			}
			std::shared_ptr<sink> sink_;
			std::vector<result_t> list_;
	};

	inline void set_sink(std::shared_ptr<sink> s){ store_t::get().set_sink(std::move(s)); }

	// ---- shared per-axis timing loop --------------------------------------
	// `fn(j, n)` returns bytes moved; `before`/`after` are the (optional) sample
	// hooks. Pushes one result_t row per x-axis point. Single source of truth so
	// the scalar and type-axis drivers time identically.
	namespace impl {
		template <class fn_t, class before_t, class after_t>
		void run_axis(store_t& store, const std::string& nm,
					  const arg_x& x, std::uint16_t wu, std::uint16_t su,
					  fn_t&& fn, before_t&& before, after_t&& after) {
			std::vector<double> time, tput;
			for(std::size_t j=0; j<x.size(); j++){
				for(std::uint16_t i=0; i<wu; i++){ volatile double s = fn(j, x[j]); (void)s; }
				time.clear(); tput.clear();
				for(std::uint16_t i=0; i<su; i++) {
					namespace cl = std::chrono;
					before();
					const auto start = cl::steady_clock::now();
						const double transfer = fn(j, x[j]);
						after();
					const auto end = cl::steady_clock::now();
					const double delta_us = static_cast<double>(
						cl::duration_cast<cl::nanoseconds>(end - start).count()) / 1'000.0;
					time.push_back(delta_us);
					tput.push_back(delta_us > 0 ? transfer / delta_us : 0.0);
				}
				store.push(nm, x[j], wu, su, bench::impl::stats(time), bench::impl::stats(tput));
			}
		}
	}

	// ---- the throughput driver --------------------------------------------
	// body signature: double(std::size_t idx, std::size_t n) -> bytes moved.
	// (The compile-time type-axis overload `throughput(types<Ts...>{}, ...)`
	//  is added by a separate lane.)
	template <class... args_t>
	void throughput(args_t... args) {
		using callback_t = std::function<double(std::size_t, std::size_t)>;

		using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
		using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
		using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
		using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
		using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
		using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
		using callback_f = typename arg::tpos<callback_t, args_t...>;

		static_assert( name_t::present,     "benchmark name must be specified (bench::name{...})" );
		static_assert( x_t::present,        "x axis must be specified (bench::arg_x{...})" );
		static_assert( callback_f::present, "benchmark body callback must be specified" );

		auto tuple = std::forward_as_tuple(args...);
		store_t& store = store_t::get();

		name nm = std::get<name_t::position>(tuple);
		arg_x x = std::get<x_t::position>(tuple);
		std::uint16_t wu = 3, su = 20;
		if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
		if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;

		callback_t fn = std::get<callback_f::position>(tuple);

		const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
		const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };

		impl::run_axis(store, nm.value, x, wu, su, fn, before, after);
	}

	// ---- the type-axis throughput driver ----------------------------------
	// Folds the benchmark body over the compile-time typelist `types<Ts...>`.
	// `body` is a C++20 generic lambda invoked once per type as
	//   body.template operator()<T>(std::size_t idx, std::size_t n) -> double
	// returning bytes moved (same contract as the scalar driver). Honours the
	// same named args/hooks; one result row per (type, x-point), the row name
	// suffixed with the type label so types are distinguishable.
	template <class... Ts, class... args_t>
	void throughput(types<Ts...>, args_t... args) {
		using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
		using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
		using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
		using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
		using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
		using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;

		static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
		static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );
		static_assert( sizeof...(Ts) > 0, "type axis must list at least one type" );

		auto tuple = std::forward_as_tuple(args...);
		store_t& store = store_t::get();

		name nm = std::get<name_t::position>(tuple);
		arg_x x = std::get<x_t::position>(tuple);
		std::uint16_t wu = 3, su = 20;
		if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
		if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;

		const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
		const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };

		// the generic body is the only positional (un-tagged) argument
		auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

		using list_t = std::tuple<Ts...>;
		meta::impl::static_for<list_t>([&](auto I){
			using T = std::tuple_element_t<decltype(I)::value, list_t>;
			std::string label = impl::type_name<T>();
			if (label.empty()) label = "T" + std::to_string(decltype(I)::value);
			const std::string row_name = nm.value + "/" + label;

			impl::run_axis(store, row_name, x, wu, su,
				[&](std::size_t idx, std::size_t n) -> double {
					return body.template operator()<T>(idx, n);
				}, before, after);
		});
	}
}

#endif
