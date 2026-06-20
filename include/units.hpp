#pragma once

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>

/**
 * @brief Minimal compile-time units for benchmark measurements.
 *
 * API design inspiration: mp-units by Mateusz Pusz.
 * This is an independent, intentionally narrow implementation for bench;
 * it does not include mp-units source code.
 */

namespace bench::units {
    struct dimension_t {
        std::int8_t time, bytes, operations;
    };

    template<dimension_t dimension_v, std::int64_t numerator_v = 1, std::int64_t denominator_v = 1>
    struct unit_t {
        static constexpr auto dimension = dimension_v;
        static constexpr auto numerator = numerator_v;
        static constexpr auto denominator = denominator_v;
    };

    template<typename unit_p, typename rep_p = double>
    struct quantity_t {
        using unit_type = unit_p;
        using rep_type = rep_p;

        template<typename target_unit_p>
        [[nodiscard]] constexpr auto in(target_unit_p) const;
        rep_p value;
    };

    inline constexpr dimension_t time_dimension{.time = 1};
    inline constexpr dimension_t bytes_dimension{.bytes = 1};
    inline constexpr dimension_t operation_dimension{.operations = 1};

    inline constexpr unit_t<dimension_t{1, 0, 0}, 1, 1'000'000'000> ns;
    inline constexpr unit_t<dimension_t{1, 0, 0}, 1, 1'000'000> us;
    inline constexpr unit_t<dimension_t{1, 0, 0}, 1, 1'000> ms;
    inline constexpr unit_t<dimension_t{1, 0, 0}> s;

    inline constexpr unit_t<dimension_t{0, 1, 0}> B;
    inline constexpr unit_t<dimension_t{0, 1, 0}, 1000> KB;
    inline constexpr unit_t<dimension_t{0, 1, 0}, 1000 * 1000> MB;
    inline constexpr unit_t<dimension_t{0, 1, 0}, 1000LL * 1000 * 1000> GB;
    inline constexpr unit_t<dimension_t{0, 1, 0}, 1024> KiB;
    inline constexpr unit_t<dimension_t{0, 1, 0}, 1024 * 1024> MiB;
    inline constexpr unit_t<dimension_t{0, 1, 0}, 1024LL * 1024 * 1024> GiB;

    inline constexpr unit_t<dimension_t{0, 0, 1}> op;
}

namespace bench::units {
    [[nodiscard]] constexpr bool same_dimension(dimension_t lhs, dimension_t rhs) noexcept {
        return lhs.time == rhs.time && lhs.bytes == rhs.bytes && lhs.operations == rhs.operations;
    }

    [[nodiscard]] constexpr dimension_t add_dimension(dimension_t lhs, dimension_t rhs) noexcept {
        return {
            .time = static_cast<std::int8_t>(lhs.time + rhs.time),
            .bytes = static_cast<std::int8_t>(lhs.bytes + rhs.bytes),
            .operations = static_cast<std::int8_t>(lhs.operations + rhs.operations),
        };
    }

    [[nodiscard]] constexpr dimension_t subtract_dimension(dimension_t lhs, dimension_t rhs) noexcept {
        return {
            .time = static_cast<std::int8_t>(lhs.time - rhs.time),
            .bytes = static_cast<std::int8_t>(lhs.bytes - rhs.bytes),
            .operations = static_cast<std::int8_t>(lhs.operations - rhs.operations),
        };
    }

    template<typename type_p> struct is_unit_t : std::false_type {};
    template<dimension_t dimension_v, std::int64_t numerator_v, std::int64_t denominator_v>
    struct is_unit_t<unit_t<dimension_v, numerator_v, denominator_v>> : std::true_type {};

    template<typename type_p> struct is_quantity_t : std::false_type {};
    template<typename unit_p, typename rep_p>
    struct is_quantity_t<quantity_t<unit_p, rep_p>> : std::true_type {};

    template<typename type_p> concept unit_c = is_unit_t<std::remove_cvref_t<type_p>>::value;
    template<typename type_p> concept quantity_c = is_quantity_t<std::remove_cvref_t<type_p>>::value;
    template<typename type_p>concept scalar_c = std::is_arithmetic_v<std::remove_cvref_t<type_p>>;

    template<unit_c lhs_unit_p, unit_c rhs_unit_p>
    [[nodiscard]] constexpr auto operator*(lhs_unit_p, rhs_unit_p) {
        using lhs_unit_t = std::remove_cvref_t<lhs_unit_p>;
        using rhs_unit_t = std::remove_cvref_t<rhs_unit_p>;

        return unit_t<
            add_dimension(lhs_unit_t::dimension, rhs_unit_t::dimension),
            lhs_unit_t::numerator * rhs_unit_t::numerator,
            lhs_unit_t::denominator * rhs_unit_t::denominator>{};
    }

    template<unit_c lhs_unit_p, unit_c rhs_unit_p>
    [[nodiscard]] constexpr auto operator/(lhs_unit_p, rhs_unit_p) {
        using lhs_unit_t = std::remove_cvref_t<lhs_unit_p>;
        using rhs_unit_t = std::remove_cvref_t<rhs_unit_p>;

        return unit_t<
            subtract_dimension(lhs_unit_t::dimension, rhs_unit_t::dimension),
            lhs_unit_t::numerator * rhs_unit_t::denominator, lhs_unit_t::denominator * rhs_unit_t::numerator>{};
    }

    template<scalar_c rep_p, unit_c unit_p>
    [[nodiscard]] constexpr auto operator*(rep_p value, unit_p) {
        using unit_type_t = std::remove_cvref_t<unit_p>;
        return quantity_t<unit_type_t, rep_p>{value};
    }

    template<unit_c unit_p, scalar_c rep_p>
    [[nodiscard]] constexpr auto operator*(unit_p unit, rep_p value) {
        return value * unit;
    }

    template<typename unit_p, typename rep_p>
    template<typename target_unit_p>
    [[nodiscard]] constexpr auto quantity_t<unit_p, rep_p>::in(target_unit_p) const {
        using target_unit_t = std::remove_cvref_t<target_unit_p>;
        using result_t = std::common_type_t<rep_p, long double>;

        static_assert(unit_c<target_unit_t>, "target must be a bench::units::unit_t");
        static_assert(same_dimension(unit_p::dimension, target_unit_t::dimension), "cannot convert between different dimensions");
        constexpr result_t scale =
            (static_cast<result_t>(unit_p::numerator) / static_cast<result_t>(unit_p::denominator)) /
            (static_cast<result_t>(target_unit_t::numerator) / static_cast<result_t>(target_unit_t::denominator));
        return static_cast<result_t>(value) * scale;
    }

    template<unit_c unit_p, scalar_c lhs_rep_p, scalar_c rhs_rep_p>
    [[nodiscard]] constexpr auto operator*(quantity_t<unit_p, lhs_rep_p> quantity, rhs_rep_p value) {
        using result_rep_t = std::common_type_t<lhs_rep_p, rhs_rep_p>;
        return quantity_t<unit_p, result_rep_t>{
            static_cast<result_rep_t>(quantity.value) * static_cast<result_rep_t>(value)};
    }

    template<scalar_c lhs_rep_p, unit_c unit_p, scalar_c rhs_rep_p>
    [[nodiscard]] constexpr auto operator*(lhs_rep_p value, quantity_t<unit_p, rhs_rep_p> quantity) {
        return quantity * value;
    }

    template<unit_c unit_p, scalar_c lhs_rep_p, scalar_c rhs_rep_p>
    [[nodiscard]] constexpr auto operator/(quantity_t<unit_p, lhs_rep_p> quantity, rhs_rep_p value){
        using result_rep_t = std::common_type_t<lhs_rep_p, rhs_rep_p>;
        return quantity_t<unit_p, result_rep_t> {
            static_cast<result_rep_t>(quantity.value) / static_cast<result_rep_t>(value)};
    }

    template<unit_c lhs_unit_p, typename lhs_rep_p, unit_c rhs_unit_p, typename rhs_rep_p>
    [[nodiscard]] constexpr auto operator*(quantity_t<lhs_unit_p, lhs_rep_p> lhs, quantity_t<rhs_unit_p, rhs_rep_p> rhs) {
        using result_unit_t = std::remove_cvref_t<decltype(lhs_unit_p{} * rhs_unit_p{})>;
        using result_rep_t = std::common_type_t<lhs_rep_p, rhs_rep_p>;
        return quantity_t<result_unit_t, result_rep_t>{
            static_cast<result_rep_t>(lhs.value) * static_cast<result_rep_t>(rhs.value)};
    }

    template<unit_c lhs_unit_p, typename lhs_rep_p, unit_c rhs_unit_p, typename rhs_rep_p>
    [[nodiscard]] constexpr auto operator/(quantity_t<lhs_unit_p, lhs_rep_p> lhs,  quantity_t<rhs_unit_p, rhs_rep_p> rhs) {
        using result_unit_t = std::remove_cvref_t<decltype(lhs_unit_p{} / rhs_unit_p{})>;
        using result_rep_t = std::common_type_t<lhs_rep_p, rhs_rep_p>;
        return quantity_t<result_unit_t, result_rep_t>{
            static_cast<result_rep_t>(lhs.value) / static_cast<result_rep_t>(rhs.value)};
    }

    template<unit_c lhs_unit_p, typename lhs_rep_p, unit_c rhs_unit_p, typename rhs_rep_p>
    [[nodiscard]] constexpr auto operator+(quantity_t<lhs_unit_p, lhs_rep_p> lhs, quantity_t<rhs_unit_p, rhs_rep_p> rhs){
        static_assert(same_dimension(lhs_unit_p::dimension, rhs_unit_p::dimension), "cannot add quantities with different dimensions");
        using result_rep_t = std::common_type_t<lhs_rep_p, rhs_rep_p, long double>;
        return quantity_t<lhs_unit_p, result_rep_t>{
            static_cast<result_rep_t>(lhs.value) + static_cast<result_rep_t>(rhs.in(lhs_unit_p{}))};
    }

    template<unit_c lhs_unit_p, typename lhs_rep_p, unit_c rhs_unit_p, typename rhs_rep_p>
    [[nodiscard]] constexpr auto operator-(quantity_t<lhs_unit_p, lhs_rep_p> lhs, quantity_t<rhs_unit_p, rhs_rep_p> rhs)  {
        static_assert(same_dimension(lhs_unit_p::dimension, rhs_unit_p::dimension), "cannot subtract quantities with different dimensions");
        using result_rep_t = std::common_type_t<lhs_rep_p, rhs_rep_p, long double>;
        return quantity_t<lhs_unit_p, result_rep_t>{
            static_cast<result_rep_t>(lhs.value) - static_cast<result_rep_t>(rhs.in(lhs_unit_p{}))};
    }

    inline constexpr auto B_per_s = B / s;
    inline constexpr auto KB_per_s = KB / s;
    inline constexpr auto MB_per_s = MB / s;
    inline constexpr auto GB_per_s = GB / s;
    inline constexpr auto KiB_per_s = KiB / s;
    inline constexpr auto MiB_per_s = MiB / s;
    inline constexpr auto GiB_per_s = GiB / s;
    inline constexpr auto op_per_s = op / s;
    inline constexpr auto ns_per_op = ns / op;
    inline constexpr auto us_per_op = us / op;
    inline constexpr auto ms_per_op = ms / op;
    inline constexpr auto s_per_op = s / op;

    struct display_value_t {
        long double value;
        std::string_view unit;
    };

    [[nodiscard]] constexpr long double absolute_value(long double value) noexcept {
        return value < 0 ? -value : value;
    }

    template<unit_c unit_p, typename rep_p>
    [[nodiscard]] constexpr auto display_value(quantity_t<unit_p, rep_p> quantity) {
        if constexpr (same_dimension(unit_p::dimension, time_dimension)) {
            if (absolute_value(quantity.in(s)) >= 1.0L)  return display_value_t{quantity.in(s), "s"};
            if (absolute_value(quantity.in(ms)) >= 1.0L) return display_value_t{quantity.in(ms), "ms"};
            if (absolute_value(quantity.in(us)) >= 1.0L) return display_value_t{quantity.in(us), "us"};
            return display_value_t{quantity.in(ns), "ns"};
        }
        if constexpr (same_dimension(unit_p::dimension, bytes_dimension)) {
            if (absolute_value(quantity.in(GiB)) >= 1.0L) return display_value_t{quantity.in(GiB), "GiB"};
            if (absolute_value(quantity.in(MiB)) >= 1.0L) return display_value_t{quantity.in(MiB), "MiB"};
            if (absolute_value(quantity.in(KiB)) >= 1.0L) return display_value_t{quantity.in(KiB), "KiB"};
            return display_value_t{quantity.in(B), "B"};
        }
        if constexpr (same_dimension(unit_p::dimension, operation_dimension)) return display_value_t{quantity.in(op), "op"};
        if constexpr (same_dimension(unit_p::dimension, decltype(B_per_s)::dimension)) {
            if (absolute_value(quantity.in(GiB_per_s)) >= 1.0L) return display_value_t{quantity.in(GiB_per_s), "GiB/s"};
            if (absolute_value(quantity.in(MiB_per_s)) >= 1.0L) return display_value_t{quantity.in(MiB_per_s), "MiB/s"};
            if (absolute_value(quantity.in(KiB_per_s)) >= 1.0L) return display_value_t{quantity.in(KiB_per_s), "KiB/s"};
            return display_value_t{quantity.in(B_per_s), "B/s"};
        }
        if constexpr (same_dimension(unit_p::dimension, decltype(op_per_s)::dimension)) return display_value_t{quantity.in(op_per_s), "op/s"};
        if constexpr (same_dimension(unit_p::dimension, decltype(ns_per_op)::dimension)) {
            if (absolute_value(quantity.in(s_per_op)) >= 1.0L)  return display_value_t{quantity.in(s_per_op), "s/op"};
            if (absolute_value(quantity.in(ms_per_op)) >= 1.0L) return display_value_t{quantity.in(ms_per_op), "ms/op"};
            if (absolute_value(quantity.in(us_per_op)) >= 1.0L) return display_value_t{quantity.in(us_per_op), "us/op"};
            return display_value_t{quantity.in(ns_per_op), "ns/op"};
        }
        return display_value_t{static_cast<long double>(quantity.value), ""};
    }

    template<unit_c unit_p>
    [[nodiscard]] constexpr std::string_view unit_symbol(unit_p = {}) {
        using unit_t = std::remove_cvref_t<unit_p>;
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(ns)>>) return "ns";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(us)>>) return "us";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(ms)>>) return "ms";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(s)>>) return "s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(B)>>) return "B";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(KB)>>) return "KB";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(MB)>>) return "MB";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(GB)>>) return "GB";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(KiB)>>) return "KiB";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(MiB)>>) return "MiB";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(GiB)>>) return "GiB";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(op)>>) return "op";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(B_per_s)>>) return "B/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(KB_per_s)>>) return "KB/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(MB_per_s)>>) return "MB/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(GB_per_s)>>) return "GB/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(KiB_per_s)>>) return "KiB/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(MiB_per_s)>>) return "MiB/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(GiB_per_s)>>) return "GiB/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(op_per_s)>>) return "op/s";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(ns_per_op)>>) return "ns/op";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(us_per_op)>>) return "us/op";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(ms_per_op)>>) return "ms/op";
        if constexpr (std::is_same_v<unit_t, std::remove_cvref_t<decltype(s_per_op)>>) return "s/op";
        return display_value(1.0L * unit_t{}).unit;
    }
}

namespace bench {
    enum class direction_t : std::uint8_t { lower_is_better, higher_is_better, neutral };
    template<units::unit_c unit_p, typename rep_p>
    struct metric_t {
        std::string_view name, quantity;
        units::quantity_t<unit_p, rep_p> value;
        direction_t direction;
    };

    template<typename unit_p, typename rep_p>
    metric_t(std::string_view, std::string_view, units::quantity_t<unit_p, rep_p>, direction_t) -> metric_t<unit_p, rep_p>;
}
