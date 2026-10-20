// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_HPP
#define BOOST_DECIMAL_HPP

// Clang-Cl likes to throw warnings everywhere for this
// disable at the global level
#if defined(__clang__) && !defined(__GNUC__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif

// 3.4.7 evaluation format:
// This is defined at top level because it has ramifications for every successive header
#ifndef BOOST_DECIMAL_DEC_EVAL_METHOD
#  define BOOST_DECIMAL_DEC_EVAL_METHOD 0
#endif

#if BOOST_DECIMAL_DEC_EVAL_METHOD < 0 || BOOST_DECIMAL_DEC_EVAL_METHOD > 2
#  error "Unsupported value of BOOST_DECIMAL_DEC_EVAL_METHOD. Must be 0, 1, or 2"
#endif

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/cmath.hpp>
#include <boost/decimal/cstdlib.hpp>
#include <boost/decimal/cfenv.hpp>
#include <boost/decimal/literals.hpp>
#include <boost/decimal/hash.hpp>
#include <boost/decimal/cfloat.hpp>
#include <boost/decimal/charconv.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/decimal/format.hpp>
#include <boost/decimal/cstdio.hpp>
#include <boost/decimal/bid_conversion.hpp>
#include <boost/decimal/dpd_conversion.hpp>
#include <boost/decimal/string.hpp>
#include <boost/decimal/uint128_t.hpp>

#if defined(__clang__) && !defined(__GNUC__)
#  pragma clang diagnostic pop
#endif

#endif // BOOST_DECIMAL_HPP
