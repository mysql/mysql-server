// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_FWD_HPP
#define BOOST_DECIMAL_FWD_HPP

#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>

namespace boost {
namespace decimal {

class decimal32_t;
class decimal_fast32_t;
class decimal64_t;
class decimal_fast64_t;
class decimal128_t;
class decimal_fast128_t;

} // namespace decimal
} // namespace boost

namespace std {

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <>
class numeric_limits<boost::decimal::decimal32_t>;

template <>
class numeric_limits<boost::decimal::decimal_fast32_t>;

template <>
class numeric_limits<boost::decimal::decimal64_t>;

template <>
class numeric_limits<boost::decimal::decimal_fast64_t>;

template <>
class numeric_limits<boost::decimal::decimal128_t>;

template <>
class numeric_limits<boost::decimal::decimal_fast128_t>;

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

} // Namespace std

#endif // BOOST_DECIMAL_BUILD_MODULE

#endif // BOOST_DECIMAL_FWD_HPP
