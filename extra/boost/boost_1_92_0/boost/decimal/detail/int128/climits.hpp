// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_CLIMITS_HPP
#define BOOST_DECIMAL_DETAIL_INT128_CLIMITS_HPP

#include <boost/decimal/detail/int128/detail/int128_imp.hpp>
#include <boost/decimal/detail/int128/detail/uint128_imp.hpp>
#include <climits>

#define BOOST_DECIMAL_DETAIL_INT128_UINT128_MAX boost::int128::uint128_t{UINT64_MAX, UINT64_MAX}

#define BOOST_DECIMAL_DETAIL_INT128_INT128_MIN  boost::int128::int128_t{INT64_MIN, 0}
#define BOOST_DECIMAL_DETAIL_INT128_INT128_MAX  boost::int128::int128_t{INT64_MAX, UINT64_MAX}

#endif // BOOST_DECIMAL_DETAIL_INT128_CLIMITS_HPP
