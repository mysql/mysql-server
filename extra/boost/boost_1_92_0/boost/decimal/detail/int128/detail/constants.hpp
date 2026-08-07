// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_CONSTANTS_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_CONSTANTS_HPP

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <cstdint>
#include <limits>

#endif

namespace boost {
namespace int128 {
namespace detail {

BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR std::uint64_t low_word_mask {(std::numeric_limits<std::uint64_t>::max)()};

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR T offset_value_v = static_cast<T>((std::numeric_limits<std::uint64_t>::max)());

} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_CONSTANTS_HPP
