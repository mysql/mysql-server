// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_CMATH_HPP
#define BOOST_DECIMAL_CMATH_HPP

#include <boost/decimal/detail/fwd_log.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/detail/cmath/abs.hpp>
#include <boost/decimal/detail/cmath/acosh.hpp>
#include <boost/decimal/detail/cmath/asinh.hpp>
#include <boost/decimal/detail/cmath/atanh.hpp>
#include <boost/decimal/detail/cmath/ceil.hpp>
#include <boost/decimal/detail/cmath/cos.hpp>
#include <boost/decimal/detail/cmath/cosh.hpp>
#include <boost/decimal/detail/cmath/ellint_1.hpp>
#include <boost/decimal/detail/cmath/ellint_2.hpp>
#include <boost/decimal/detail/cmath/exp.hpp>
#include <boost/decimal/detail/cmath/exp2.hpp>
#include <boost/decimal/detail/cmath/expm1.hpp>
#include <boost/decimal/detail/cmath/fabs.hpp>
#include <boost/decimal/detail/cmath/fdim.hpp>
#include <boost/decimal/detail/cmath/floor.hpp>
#include <boost/decimal/detail/cmath/fmax.hpp>
#include <boost/decimal/detail/cmath/fmod.hpp>
#include <boost/decimal/detail/cmath/fmin.hpp>
#include <boost/decimal/detail/cmath/fpclassify.hpp>
#include <boost/decimal/detail/cmath/frexp.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>
#include <boost/decimal/detail/cmath/hypot.hpp>
#include <boost/decimal/detail/cmath/ilogb.hpp>
#include <boost/decimal/detail/cmath/isfinite.hpp>
#include <boost/decimal/detail/cmath/isgreater.hpp>
#include <boost/decimal/detail/cmath/isless.hpp>
#include <boost/decimal/detail/cmath/isunordered.hpp>
#include <boost/decimal/detail/cmath/ldexp.hpp>
#include <boost/decimal/detail/cmath/lgamma.hpp>
#include <boost/decimal/detail/cmath/log.hpp>
#include <boost/decimal/detail/cmath/log1p.hpp>
#include <boost/decimal/detail/cmath/log10.hpp>
#include <boost/decimal/detail/cmath/log2.hpp>
#include <boost/decimal/detail/cmath/modf.hpp>
#include <boost/decimal/detail/cmath/nearbyint.hpp>
#include <boost/decimal/detail/cmath/next.hpp>
#include <boost/decimal/detail/cmath/pow.hpp>
#include <boost/decimal/detail/cmath/remainder.hpp>
#include <boost/decimal/detail/cmath/remquo.hpp>
#include <boost/decimal/detail/cmath/riemann_zeta.hpp>
#include <boost/decimal/detail/cmath/rint.hpp>
#include <boost/decimal/detail/cmath/round.hpp>
#include <boost/decimal/detail/cmath/sin.hpp>
#include <boost/decimal/detail/cmath/sinh.hpp>
#include <boost/decimal/detail/cmath/sqrt.hpp>
#include <boost/decimal/detail/cmath/tan.hpp>
#include <boost/decimal/detail/cmath/tanh.hpp>
#include <boost/decimal/detail/cmath/tgamma.hpp>
#include <boost/decimal/detail/cmath/trunc.hpp>
#include <boost/decimal/detail/cmath/nan.hpp>
#include <boost/decimal/detail/cmath/logb.hpp>
#include <boost/decimal/detail/cmath/cbrt.hpp>
#include <boost/decimal/detail/cmath/erf.hpp>
#include <boost/decimal/detail/cmath/atan.hpp>
#include <boost/decimal/detail/cmath/asin.hpp>
#include <boost/decimal/detail/cmath/acos.hpp>
#include <boost/decimal/detail/cmath/atan2.hpp>
#include <boost/decimal/detail/cmath/fma.hpp>
#include <boost/decimal/detail/cmath/hermite.hpp>
#include <boost/decimal/detail/cmath/laguerre.hpp>
#include <boost/decimal/detail/cmath/assoc_laguerre.hpp>
#include <boost/decimal/detail/cmath/legendre.hpp>
#include <boost/decimal/detail/cmath/assoc_legendre.hpp>
#include <boost/decimal/detail/cmath/rescale.hpp>
#include <boost/decimal/detail/cmath/beta.hpp>
#include <boost/decimal/detail/cmath/normalize.hpp>
#include <boost/decimal/detail/cmath/comparetotal.hpp>
#include <boost/decimal/detail/cmath/decompose.hpp>
#include <boost/decimal/numbers.hpp>

// Macros from 3.6.2
#define BOOST_DECIMAL_HUGE_VAL_D32 std::numeric_limits<boost::decimal::decimal32_t>::infinity()
#define BOOST_DECIMAL_HUGE_VAL_D64 std::numeric_limits<boost::decimal::decimal64_t>::infinity()
#define BOOST_DECIMAL_DEC_INFINITY std::numeric_limits<boost::decimal::decimal64_t>::infinity()
#define BOOST_DECIMAL_DEC_NAN std::numeric_limits<boost::decimal::decimal64_t>::signaling_NaN()
#define BOOST_DECIMAL_FP_FAST_FMAD32 1
#define BOOST_DECIMAL_FP_FAST_FMAD64 1
#define BOOST_DECIMAL_FP_FAST_FMAD128 1

namespace boost {
namespace decimal {

// Overloads for all the functions that are implemented individually as friends

BOOST_DECIMAL_EXPORT constexpr auto scalbn(decimal32_t num, int expval) noexcept -> decimal32_t
{
    return scalbnd32(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbn(decimal_fast32_t num, int expval) noexcept -> decimal_fast32_t
{
    return scalbnd32f(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbn(decimal64_t num, int expval) noexcept -> decimal64_t
{
    return scalbnd64(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbn(decimal_fast64_t num, int expval) noexcept -> decimal_fast64_t
{
    return scalbnd64f(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbn(decimal128_t num, int expval) noexcept -> decimal128_t
{
    return scalbnd128(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbn(decimal_fast128_t num, int expval) noexcept -> decimal_fast128_t
{
    return scalbnd128f(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbln(decimal32_t num, long expval) noexcept -> decimal32_t
{
    return scalblnd32(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbln(decimal_fast32_t num, long expval) noexcept -> decimal_fast32_t
{
    return scalblnd32f(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbln(decimal64_t num, long expval) noexcept -> decimal64_t
{
    return scalblnd64(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbln(decimal_fast64_t num, long expval) noexcept -> decimal_fast64_t
{
    return scalblnd64f(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbln(decimal128_t num, long expval) noexcept -> decimal128_t
{
    return scalblnd128(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto scalbln(decimal_fast128_t num, long expval) noexcept -> decimal_fast128_t
{
    return scalblnd128f(num, expval);
}

BOOST_DECIMAL_EXPORT constexpr auto copysign(decimal32_t mag, decimal32_t sgn) noexcept -> decimal32_t
{
    return copysignd32(mag, sgn);
}

BOOST_DECIMAL_EXPORT constexpr auto copysign(decimal_fast32_t mag, decimal_fast32_t sgn) noexcept -> decimal_fast32_t
{
    return copysignd32f(mag, sgn);
}

BOOST_DECIMAL_EXPORT constexpr auto copysign(decimal64_t mag, decimal64_t sgn) noexcept -> decimal64_t
{
    return copysignd64(mag, sgn);
}

BOOST_DECIMAL_EXPORT constexpr auto copysign(decimal_fast64_t mag, decimal_fast64_t sgn) noexcept -> decimal_fast64_t
{
    return copysignd64f(mag, sgn);
}

BOOST_DECIMAL_EXPORT constexpr auto copysign(decimal128_t mag, decimal128_t sgn) noexcept -> decimal128_t
{
    return copysignd128(mag, sgn);
}

BOOST_DECIMAL_EXPORT constexpr auto copysign(decimal_fast128_t mag, decimal_fast128_t sgn) noexcept -> decimal_fast128_t
{
    return copysignd128f(mag, sgn);
}

BOOST_DECIMAL_EXPORT constexpr auto samequantum(decimal32_t lhs, decimal32_t rhs) noexcept -> bool
{
    return samequantumd32(lhs, rhs);
}

BOOST_DECIMAL_EXPORT constexpr auto samequantum(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool
{
    return samequantumd32f(lhs, rhs);
}

BOOST_DECIMAL_EXPORT constexpr auto samequantum(decimal64_t lhs, decimal64_t rhs) noexcept -> bool
{
    return samequantumd64(lhs, rhs);
}

BOOST_DECIMAL_EXPORT constexpr auto samequantum(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool
{
    return samequantumd64f(lhs, rhs);
}

BOOST_DECIMAL_EXPORT constexpr auto samequantum(decimal128_t lhs, decimal128_t rhs) noexcept -> bool
{
    return samequantumd128(lhs, rhs);
}

BOOST_DECIMAL_EXPORT constexpr auto samequantum(decimal_fast128_t lhs, decimal_fast128_t rhs) noexcept -> bool
{
    return samequantumd128f(lhs, rhs);
}

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal32_t x) noexcept -> int
{
    return quantexpd32(x);
}

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal_fast32_t x) noexcept -> int
{
    return quantexpd32f(x);
}

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal64_t x) noexcept -> int
{
    return quantexpd64(x);
}

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal_fast64_t x) noexcept -> int
{
    return quantexpd64f(x);
}

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal128_t x) noexcept -> int
{
    return quantexpd128(x);
}

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal_fast128_t x) noexcept -> int
{
    return quantexpd128f(x);
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_CMATH_HPP
