//
// Copyright (c) 2024 Dmitry Arkhipov (grisumbras@yandex.ru)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_DETAIL_LITERALS_HPP
#define BOOST_JSON_DETAIL_LITERALS_HPP

#include <boost/json/detail/config.hpp>
#include <boost/mp11/integral.hpp>

namespace boost {
namespace json {
namespace detail {

enum class literals
{
    null = 0,
    true_,
    false_,
    infinity,
    neg_infinity,
    nan,
    resume,
};

constexpr char const* literal_strings[] = {
    "null",
    "true",
    "false",
    "Infinity",
    "-Infinity",
    "NaN",
    "",
};

constexpr std::size_t literal_sizes[] = {
    4,
    4,
    5,
    8,
    9,
    3,
    0,
};

template<literals L>
using literals_c = std::integral_constant<literals, L>;

constexpr
unsigned char
literal_index(literals l)
{
    return static_cast<unsigned char>(l);
}

} // namespace detail
} // namespace json
} // namespace boost

#endif // BOOST_JSON_DETAIL_LITERALS_HPP
