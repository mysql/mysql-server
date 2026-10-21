/* Copyright 2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/unordered for library home page.
 */

#ifndef BOOST_UNORDERED_DETAIL_RANGES_SUPPORT_HPP
#define BOOST_UNORDERED_DETAIL_RANGES_SUPPORT_HPP

#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#if defined(BOOST_NO_CXX20_HDR_CONCEPTS) || defined(BOOST_NO_CXX20_HDR_RANGES)
#define BOOST_UNORDERED_NO_RANGES
#elif BOOST_WORKAROUND(BOOST_CLANG_VERSION, < 170100) && \
      defined(BOOST_LIBSTDCXX_VERSION)
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109647
 * https://github.com/llvm/llvm-project/issues/49620
 */
#define BOOST_UNORDERED_NO_RANGES
#endif

#if !defined(BOOST_UNORDERED_NO_RANGES)
#include <concepts>
#include <functional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace boost {
namespace unordered {

struct from_range_t { explicit from_range_t() = default; };
inline constexpr from_range_t from_range{};

namespace detail {

/* Existence of std::from_range_t is governed by feature macro
 * __cpp_lib_containers_ranges, but at least one stdlib implementation 
 * (GCC 14.1) does not honor it, hence the workadound below: code picks 
 * std::from_range_t if it exists, boost::unordered::from_range_t otherwise.
 * Technique explained at
 https://bannalia.blogspot.com/2016/09/compile-time-checking-existence-of.html
 */

struct from_range_t_hook{};

} /* namespace detail */
} /* namespace unordered */
} /* namespace boost */

namespace std {

template<> struct hash< ::boost::unordered::detail::from_range_t_hook>
{
  using from_range_t_type = decltype([] {
    using namespace ::boost::unordered;
    return from_range_t{};
  }());

  /* make standard happy */
  std::size_t operator()(
    const ::boost::unordered::detail::from_range_t_hook&) const;
};

} /* namespace std */

namespace boost {
namespace unordered {
namespace detail {

template<typename T>
concept convertible_to_from_range_t =
  std::is_convertible_v<T, boost::unordered::from_range_t> ||
  std::is_convertible_v<T, std::hash<from_range_t_hook>::from_range_t_type>;

template<class R, class T>
concept container_compatible_range =
  std::ranges::input_range<R> &&
  std::convertible_to<std::ranges::range_reference_t<R>, T>;

template<std::ranges::input_range Range>
using range_key_t =
  std::remove_cvref_t<
    std::tuple_element_t<0, std::ranges::range_value_t<Range>>>;

template<std::ranges::input_range Range>
using range_mapped_t =
  std::remove_cvref_t<
    std::tuple_element_t<1, std::ranges::range_value_t<Range>>>;

template<std::ranges::input_range Range>
using range_to_alloc_t = 
  std::pair<const range_key_t<Range>, range_mapped_t<Range>>;

} /* namespace detail */
} /* namespace unordered */
} /* namespace boost */

#endif /* !defined(BOOST_UNORDERED_NO_RANGES) */
#endif
