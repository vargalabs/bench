/* Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
 * MIT License — see LICENSE
 *
 * Small SVG helpers: HTML/XML text escaping, a C++17 is_base_of replacement,
 * and a label-width helper for axis layout. Ported from plot:: into
 * bench::plot::. Dependency-free (standard library only).
 */
#ifndef BENCH_PLOT_UTILS_HPP
#define BENCH_PLOT_UTILS_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <cstddef>

namespace bench::plot::util {
	inline std::string html_escape( const std::string& data ){
		std::string buffer;
		buffer.reserve(data.size());
		for(std::size_t pos = 0; pos != data.size(); ++pos) {
			switch(data[pos]) {
				case '&':  buffer.append("&amp;");       break;
				case '\"': buffer.append("&quot;");      break;
				case '\'': buffer.append("&apos;");      break;
				case '<':  buffer.append("&lt;");        break;
				case '>':  buffer.append("&gt;");        break;
				default:   buffer.append(&data[pos], 1); break;
			}
		}
		return buffer;
	}
}
namespace bench::plot::util::details {
    template <typename Base> std::true_type is_base_of_test_func(Base*);
    template <typename Base> std::false_type is_base_of_test_func(void*);
    template <typename Base, typename Derived>
    using pre_is_base_of = decltype(is_base_of_test_func<Base>(std::declval<Derived*>()));

    template <typename Base, typename Derived, typename = void>
    struct pre_is_base_of2 : public std::true_type { };
    // note std::void_t is a C++17 feature
    template <typename Base, typename Derived>
    struct pre_is_base_of2<Base, Derived, std::void_t<pre_is_base_of<Base, Derived>>> :
        public pre_is_base_of<Base, Derived> { };
}

namespace bench::plot::util {
template <typename Base, typename Derived>
struct is_base_of :
    public std::conditional_t<
        std::is_class<Base>::value && std::is_class<Derived>::value,
        details::pre_is_base_of2<Base, Derived>,
        std::false_type
    > { };
}

namespace bench::plot::utils {
	inline std::size_t size_of_max_value( const std::vector<std::string>& lbl ){
		auto it = std::max_element( std::begin(lbl), std::end(lbl),
				[](const auto& a, const auto& b) -> bool {
					return a.size() < b.size();
		});
		return it->size();
	}
}
#endif
