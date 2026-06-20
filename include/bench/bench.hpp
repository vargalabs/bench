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
#include "../units.hpp"
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
	struct unit_t {};
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

	// Detects whether `T` is a specialization of the class template `Tmpl`.
	// Used to constrain the type-axis throughput overload to `std::tuple<...>`.
	template <class T, template <class...> class Tmpl>
	struct is_specialization_of : std::false_type {};
	template <template <class...> class Tmpl, class... Us>
	struct is_specialization_of<Tmpl<Us...>, Tmpl> : std::true_type {};

	template <class T>
	concept is_tuple = is_specialization_of<std::remove_cvref_t<T>, std::tuple>::value;

	template <bool present_v, class position_t, class default_unit_t>
	struct selected_unit_t { using type = default_unit_t; };
	template <class position_t, class default_unit_t>
	struct selected_unit_t<true, position_t, default_unit_t> { using type = typename position_t::type::unit_type; };

	template <class unit_p>
	concept runtime_unit_c = units::unit_c<unit_p> &&
		units::same_dimension(std::remove_cvref_t<unit_p>::dimension, units::time_dimension);
	template <class unit_p>
	concept throughput_unit_c = units::unit_c<unit_p> &&
		units::same_dimension(std::remove_cvref_t<unit_p>::dimension, decltype(units::B_per_s)::dimension);
	template <class unit_p>
	concept rate_unit_c = units::unit_c<unit_p> &&
		units::same_dimension(std::remove_cvref_t<unit_p>::dimension, decltype(units::op_per_s)::dimension);
	template <class unit_p>
	concept latency_unit_c = units::unit_c<unit_p> &&
		units::same_dimension(std::remove_cvref_t<unit_p>::dimension, decltype(units::ns_per_op)::dimension);
}

namespace bench {
	constexpr std::size_t max_dims = 32;
	namespace arg = bench::meta::arg;

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

	struct ok_t {};
	using ok = ok_t;

	template <units::unit_c unit_p>
	struct unit {
		using value_type = impl::tag::unit_t;
		using unit_type = std::remove_cvref_t<unit_p>;
		unit_type value;
	};
	template <units::unit_c unit_p>
	unit(unit_p) -> unit<unit_p>;

	inline std::string_view direction_name(direction_t direction){
		switch(direction){
			case direction_t::lower_is_better: return "lower_is_better";
			case direction_t::higher_is_better: return "higher_is_better";
			case direction_t::neutral:
			default: return "neutral";
		}
	}

	// ---- result record (plain POD; was the HDF5-registered test_t) ---------
	struct result_t {
		static constexpr std::size_t max_name = 64;
		static constexpr std::size_t max_metric = 32;
		static constexpr std::size_t max_unit = 16;
		std::uint16_t warmup;
		std::uint16_t sample;
		std::uint64_t x;
		double mean_runtime;     // microseconds
		double std_runtime;      // microseconds
		double mean_throughput;  // compatibility alias for mean_metric
		double std_throughput;   // compatibility alias for std_metric
		double mean_metric;
		double std_metric;
		direction_t direction;
		char   name[max_name];
		char   metric[max_metric];
		char   unit[max_unit];
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
					"[total events][metric/unit        ] [ms runtime / stddev] [     mean / stddev ]\n");
				header = true;
			}
			std::printf("%-52s %12llu %-9s/%-8s %11.2f  %8.3f %10.2f   %7.1f\n",
				r.name, static_cast<unsigned long long>(r.x), r.metric, r.unit,
				r.mean_runtime/1000.0, r.std_runtime/1000.0, r.mean_metric, r.std_metric);
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
			push(nm, x, wu, su, "throughput", "B/us", direction_t::higher_is_better, time, tp);
		}

		template<class V, class T>
		void push(const std::string& nm, std::uint64_t x, V wu, V su,
				  std::string_view metric, std::string_view unit, direction_t direction,
				  const std::pair<T,T>& time, const std::pair<T,T>& value ) {
			result_t item{};
			item.warmup = wu;
			item.sample = su;
			item.x = x;
			item.mean_runtime = time.first;
			item.std_runtime  = time.second;
			item.mean_throughput = value.first;
			item.std_throughput  = value.second;
			item.mean_metric = value.first;
			item.std_metric = value.second;
			item.direction = direction;
			std::strncpy(item.name, nm.data(), result_t::max_name - 1);
			std::strncpy(item.metric, metric.data(), result_t::max_metric - 1);
			std::strncpy(item.unit, unit.data(), result_t::max_unit - 1);
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
	namespace impl {
		template <class T>
		concept byte_quantity_c = units::quantity_c<T> &&
			units::same_dimension(std::remove_cvref_t<T>::unit_type::dimension, units::bytes_dimension);
		template <class T>
		concept op_quantity_c = units::quantity_c<T> &&
			units::same_dimension(std::remove_cvref_t<T>::unit_type::dimension, units::operation_dimension);

		template <class unit_t>
		auto unit_label(){
			return units::unit_symbol(unit_t{});
		}

		template <class unit_t, class work_t, class elapsed_t>
		auto runtime_metric(const work_t&, elapsed_t elapsed){
			return elapsed.in(unit_t{});
		}

		template <class unit_t, class work_t, class elapsed_t>
		auto throughput_metric(const work_t& work, elapsed_t elapsed){
			static_assert(byte_quantity_c<decltype(work)>, "bench::throughput body must return a byte quantity");
			return (work / elapsed).in(unit_t{});
		}

		template <class unit_t, class work_t, class elapsed_t>
		auto rate_metric(const work_t& work, elapsed_t elapsed){
			static_assert(op_quantity_c<decltype(work)>, "bench::rate body must return an operation quantity");
			return (work / elapsed).in(unit_t{});
		}

		template <class unit_t, class work_t, class elapsed_t>
		auto latency_metric(const work_t& work, elapsed_t elapsed){
			static_assert(op_quantity_c<decltype(work)>, "bench::latency body must return an operation quantity");
			return (elapsed / work).in(unit_t{});
		}

		template <class unit_t, class fn_t, class before_t, class after_t, class metric_fn_t>
		void run_axis(store_t& store, const std::string& nm,
					  const arg_x& x, std::uint16_t wu, std::uint16_t su,
					  std::string_view metric, std::string_view unit, direction_t direction,
					  fn_t&& fn, before_t&& before, after_t&& after, metric_fn_t&& metric_fn) {
			std::vector<double> time, values;
			for(std::size_t j=0; j<x.size(); j++){
				for(std::uint16_t i=0; i<wu; i++) (void)fn(j, x[j]);
				time.clear(); values.clear();
				for(std::uint16_t i=0; i<su; i++) {
					namespace cl = std::chrono;
					before();
					const auto start = cl::steady_clock::now();
						const auto work = fn(j, x[j]);
						after();
					const auto end = cl::steady_clock::now();
					const double delta_us = static_cast<double>(
						cl::duration_cast<cl::nanoseconds>(end - start).count()) / 1'000.0;
					const auto elapsed = delta_us * units::us;
					time.push_back(delta_us);
					values.push_back(delta_us > 0 ? metric_fn(work, elapsed) : 0.0);
				}
				store.push(nm, x[j], wu, su, metric, unit, direction, bench::impl::stats(time), bench::impl::stats(values));
			}
		}
	}

	namespace impl {
		template <class default_unit_t, class... args_t>
		using display_unit_t = typename selected_unit_t<
			arg::tpos<impl::tag::unit_t, args_t...>::present,
			typename arg::tpos<impl::tag::unit_t, args_t...>,
			default_unit_t>::type;

		template <class unit_t, class... args_t>
		void run_runtime(args_t... args) {
			static_assert(runtime_unit_c<unit_t>, "bench::runtime display unit must be a time unit");
			using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
			using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
			using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
			using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
			using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
			using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
			static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
			static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );

			auto tuple = std::forward_as_tuple(args...);
			store_t& store = store_t::get();
			name nm = std::get<name_t::position>(tuple);
			arg_x x = std::get<x_t::position>(tuple);
			std::uint16_t wu = 3, su = 20;
			if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
			if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;

			const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
			const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
			auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

			impl::run_axis<unit_t>(store, nm.value, x, wu, su, "runtime", impl::unit_label<unit_t>(),
				direction_t::lower_is_better, body, before, after,
				[](const auto& work, auto elapsed){ return impl::runtime_metric<unit_t>(work, elapsed); });
		}

		template <class unit_t, class... args_t>
		void run_throughput(args_t... args) {
			static_assert(throughput_unit_c<unit_t>, "bench::throughput display unit must be a byte/time unit");
			using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
			using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
			using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
			using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
			using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
			using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
			static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
			static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );

			auto tuple = std::forward_as_tuple(args...);
			store_t& store = store_t::get();
			name nm = std::get<name_t::position>(tuple);
			arg_x x = std::get<x_t::position>(tuple);
			std::uint16_t wu = 3, su = 20;
			if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
			if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;

			const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
			const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
			auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

			impl::run_axis<unit_t>(store, nm.value, x, wu, su, "throughput", impl::unit_label<unit_t>(),
				direction_t::higher_is_better, body, before, after,
				[](const auto& work, auto elapsed){ return impl::throughput_metric<unit_t>(work, elapsed); });
		}

		template <class unit_t, class... args_t>
		void run_rate(args_t... args) {
			static_assert(rate_unit_c<unit_t>, "bench::rate display unit must be an operation/time unit");
			using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
			using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
			using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
			using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
			using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
			using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
			static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
			static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );

			auto tuple = std::forward_as_tuple(args...);
			store_t& store = store_t::get();
			name nm = std::get<name_t::position>(tuple);
			arg_x x = std::get<x_t::position>(tuple);
			std::uint16_t wu = 3, su = 20;
			if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
			if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;

			const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
			const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
			auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

			impl::run_axis<unit_t>(store, nm.value, x, wu, su, "rate", impl::unit_label<unit_t>(),
				direction_t::higher_is_better, body, before, after,
				[](const auto& work, auto elapsed){ return impl::rate_metric<unit_t>(work, elapsed); });
		}

		template <class unit_t, class... args_t>
		void run_latency(args_t... args) {
			static_assert(latency_unit_c<unit_t>, "bench::latency display unit must be a time/operation unit");
			using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
			using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
			using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
			using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
			using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
			using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
			static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
			static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );

			auto tuple = std::forward_as_tuple(args...);
			store_t& store = store_t::get();
			name nm = std::get<name_t::position>(tuple);
			arg_x x = std::get<x_t::position>(tuple);
			std::uint16_t wu = 3, su = 20;
			if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
			if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;

			const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
			const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
			auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

			impl::run_axis<unit_t>(store, nm.value, x, wu, su, "latency", impl::unit_label<unit_t>(),
				direction_t::lower_is_better, body, before, after,
				[](const auto& work, auto elapsed){ return impl::latency_metric<unit_t>(work, elapsed); });
		}
	}

	template <class... args_t>
	void runtime(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::ms), args_t...>;
		impl::run_runtime<unit_t>(args...);
	}

	// ---- the throughput driver --------------------------------------------
	// body signature: quantity_t<bytes>(std::size_t idx, std::size_t n).
	// The compile-time type-axis form is the explicit-tuple overload below
	// (`throughput<std::tuple<Ts...>>(...)`).
	template <class... args_t>
	void throughput(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::MiB_per_s), args_t...>;
		impl::run_throughput<unit_t>(args...);
	}

	template <class... args_t>
	void rate(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::op_per_s), args_t...>;
		impl::run_rate<unit_t>(args...);
	}

	template <class... args_t>
	void latency(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::ns_per_op), args_t...>;
		impl::run_latency<unit_t>(args...);
	}

	template <class Tuple, class... args_t>
		requires impl::is_tuple<Tuple>
	void runtime(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::ms), args_t...>;
		using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
		using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
		using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
		using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
		using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
		using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
		static_assert( impl::runtime_unit_c<unit_t>, "bench::runtime display unit must be a time unit" );
		static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
		static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );
		static_assert( std::tuple_size_v<Tuple> > 0, "type axis must list at least one type" );

		auto tuple = std::forward_as_tuple(args...);
		store_t& store = store_t::get();
		name nm = std::get<name_t::position>(tuple);
		arg_x x = std::get<x_t::position>(tuple);
		std::uint16_t wu = 3, su = 20;
		if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
		if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;
		const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
		const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
		auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

		meta::impl::static_for<Tuple>([&](auto I){
			using T = std::tuple_element_t<decltype(I)::value, Tuple>;
			std::string label = impl::type_name<T>();
			if (label.empty()) label = "T" + std::to_string(decltype(I)::value);
			const std::string row_name = nm.value + "/" + label;
			impl::run_axis<unit_t>(store, row_name, x, wu, su, "runtime", impl::unit_label<unit_t>(),
				direction_t::lower_is_better,
				[&](std::size_t idx, std::size_t n){ return body.template operator()<T>(idx, n); }, before, after,
				[](const auto& work, auto elapsed){ return impl::runtime_metric<unit_t>(work, elapsed); });
		});
	}

	// ---- the type-axis throughput driver ----------------------------------
	// Folds the benchmark body over the compile-time typelist supplied as an
	// explicit template argument, a `std::tuple<Ts...>`. Invoke as
	//   bench::throughput<std::tuple<A,B,C>>(bench::name{...}, ...);
	// The leading template parameter `Tuple` is constrained (impl::is_tuple) to
	// be a `std::tuple` specialization so this never collides with the scalar
	// `throughput(args...)` overload — ordinary calls deduce nothing for `Tuple`
	// and select the scalar form, while an explicit tuple selects this one.
	//
	// `body` is a C++20 generic lambda invoked once per type as
	//   body.template operator()<T>(std::size_t idx, std::size_t n)
	// returning a byte quantity. Honours the
	// same named args/hooks; one result row per (type, x-point), the row name
	// suffixed with the type label so types are distinguishable.
	template <class Tuple, class... args_t>
		requires impl::is_tuple<Tuple>
	void throughput(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::MiB_per_s), args_t...>;
		using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
		using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
		using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
		using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
		using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
		using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;

		static_assert( impl::throughput_unit_c<unit_t>, "bench::throughput display unit must be a byte/time unit" );
		static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
		static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );
		static_assert( std::tuple_size_v<Tuple> > 0, "type axis must list at least one type" );

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

		meta::impl::static_for<Tuple>([&](auto I){
			using T = std::tuple_element_t<decltype(I)::value, Tuple>;
			std::string label = impl::type_name<T>();
			if (label.empty()) label = "T" + std::to_string(decltype(I)::value);
			const std::string row_name = nm.value + "/" + label;

			impl::run_axis<unit_t>(store, row_name, x, wu, su, "throughput", impl::unit_label<unit_t>(),
				direction_t::higher_is_better,
				[&](std::size_t idx, std::size_t n){ return body.template operator()<T>(idx, n); }, before, after,
				[](const auto& work, auto elapsed){ return impl::throughput_metric<unit_t>(work, elapsed); });
		});
	}

	template <class Tuple, class... args_t>
		requires impl::is_tuple<Tuple>
	void rate(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::op_per_s), args_t...>;
		using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
		using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
		using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
		using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
		using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
		using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
		static_assert( impl::rate_unit_c<unit_t>, "bench::rate display unit must be an operation/time unit" );
		static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
		static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );
		static_assert( std::tuple_size_v<Tuple> > 0, "type axis must list at least one type" );

		auto tuple = std::forward_as_tuple(args...);
		store_t& store = store_t::get();
		name nm = std::get<name_t::position>(tuple);
		arg_x x = std::get<x_t::position>(tuple);
		std::uint16_t wu = 3, su = 20;
		if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
		if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;
		const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
		const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
		auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

		meta::impl::static_for<Tuple>([&](auto I){
			using T = std::tuple_element_t<decltype(I)::value, Tuple>;
			std::string label = impl::type_name<T>();
			if (label.empty()) label = "T" + std::to_string(decltype(I)::value);
			const std::string row_name = nm.value + "/" + label;
			impl::run_axis<unit_t>(store, row_name, x, wu, su, "rate", impl::unit_label<unit_t>(),
				direction_t::higher_is_better,
				[&](std::size_t idx, std::size_t n){ return body.template operator()<T>(idx, n); }, before, after,
				[](const auto& work, auto elapsed){ return impl::rate_metric<unit_t>(work, elapsed); });
		});
	}

	template <class Tuple, class... args_t>
		requires impl::is_tuple<Tuple>
	void latency(args_t... args) {
		using unit_t = impl::display_unit_t<decltype(units::ns_per_op), args_t...>;
		using name_t     = typename arg::tpos<impl::tag::name_t,   args_t...>;
		using x_t        = typename arg::tpos<impl::tag::x_t,      args_t...>;
		using warmup_t   = typename arg::tpos<impl::tag::warmup_t, args_t...>;
		using sample_t   = typename arg::tpos<impl::tag::sample_t, args_t...>;
		using before_t   = typename arg::tpos<impl::tag::before_sample_t, args_t...>;
		using after_t    = typename arg::tpos<impl::tag::after_sample_t,  args_t...>;
		static_assert( impl::latency_unit_c<unit_t>, "bench::latency display unit must be a time/operation unit" );
		static_assert( name_t::present, "benchmark name must be specified (bench::name{...})" );
		static_assert( x_t::present,    "x axis must be specified (bench::arg_x{...})" );
		static_assert( std::tuple_size_v<Tuple> > 0, "type axis must list at least one type" );

		auto tuple = std::forward_as_tuple(args...);
		store_t& store = store_t::get();
		name nm = std::get<name_t::position>(tuple);
		arg_x x = std::get<x_t::position>(tuple);
		std::uint16_t wu = 3, su = 20;
		if constexpr(warmup_t::present) wu = std::get<warmup_t::position>(tuple).value;
		if constexpr(sample_t::present) su = std::get<sample_t::position>(tuple).value;
		const auto before = [&]{ if constexpr(before_t::present) std::get<before_t::position>(tuple).value(); };
		const auto after  = [&]{ if constexpr(after_t::present)  std::get<after_t::position>(tuple).value();  };
		auto&& body = arg::getn<sizeof...(args_t) - 1>(args...);

		meta::impl::static_for<Tuple>([&](auto I){
			using T = std::tuple_element_t<decltype(I)::value, Tuple>;
			std::string label = impl::type_name<T>();
			if (label.empty()) label = "T" + std::to_string(decltype(I)::value);
			const std::string row_name = nm.value + "/" + label;
			impl::run_axis<unit_t>(store, row_name, x, wu, su, "latency", impl::unit_label<unit_t>(),
				direction_t::lower_is_better,
				[&](std::size_t idx, std::size_t n){ return body.template operator()<T>(idx, n); }, before, after,
				[](const auto& work, auto elapsed){ return impl::latency_metric<unit_t>(work, elapsed); });
		});
	}
}

#endif
