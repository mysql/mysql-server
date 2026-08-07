// Copyright 2023 - 2026 Matt Borland
// Copyright 2023 - 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_FREXP_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_FREXP_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/pow_impl.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cmath>
#include <type_traits>
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127)
#endif

template <typename T>
constexpr auto frexp_impl(const T v, int* expon) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    // This implementation of frexp follows closely that of eval_frexp
    // in Boost.Multiprecision's cpp_dec_float template class.
    constexpr T zero { 0, 0 };

    T result_frexp { zero };

    const int v_fp { fpclassify(v) };

    if (v_fp != FP_NORMAL)
    {
        if (expon != nullptr) { *expon = 0; }

        #ifndef BOOST_DECIMAL_FAST_MATH
        if (v_fp == FP_NAN)
        {
            result_frexp = std::numeric_limits<T>::quiet_NaN();
        }
        else if (v_fp == FP_INFINITE)
        {
            result_frexp = std::numeric_limits<T>::infinity();
        }
        #endif
    }
    else
    {
        result_frexp = v;

        const bool b_neg { signbit(v) };

        if(b_neg) { result_frexp = -result_frexp; }

        // Use frexp10 to obtain an estimate of the decimal exponent.
        // This is stored in t_pre for later use.

        int t_pre { };
        static_cast<void>(frexp10(result_frexp, &t_pre));

        if (t_pre != 0)
        {
            // Handle known powers of two in t_pre and along the way,
            // do an approximate conversion of t_pre from base-10 to base-2.

            t_pre = (t_pre * 1000) / 301;

            result_frexp = result_frexp * detail::pow_2_impl<T>(-t_pre);
        }

        // Do a performance-optimized binary reduction of the remaining
        // mantissa bits that are left-over from the call to frexp10.

        int t { };

        BOOST_DECIMAL_IF_CONSTEXPR (std::numeric_limits<T>::digits10 > 20)
        {
            constexpr T two_pow_64 { T { UINT64_C(0xFFFFFFFFFFFFFFFF) } + 1 };
            constexpr T two_pow_32 { UINT64_C(0x100000000) };
            constexpr T two_pow_16 { UINT32_C(0x10000) };

            while (result_frexp >= two_pow_64) { result_frexp = result_frexp / two_pow_64; t += 64; }
            if    (result_frexp >= two_pow_32) { result_frexp = result_frexp / two_pow_32; t += 32; }
            if    (result_frexp >= two_pow_16) { result_frexp = result_frexp / two_pow_16; t += 16; }
        }
        else
        BOOST_DECIMAL_IF_CONSTEXPR (std::numeric_limits<T>::digits10 <= 20 && std::numeric_limits<T>::digits10 > 10)
        {
            constexpr T two_pow_32 { UINT64_C(0x100000000) };
            constexpr T two_pow_16 { UINT32_C(0x10000) };

            while (result_frexp >= two_pow_32) { result_frexp = result_frexp / two_pow_32; t += 32; }
            if    (result_frexp >= two_pow_16) { result_frexp = result_frexp / two_pow_16; t += 16; }
        }
        else
        BOOST_DECIMAL_IF_CONSTEXPR (std::numeric_limits<T>::digits10 <= 10)
        {
            constexpr T two_pow_16 { UINT32_C(0x10000) };

            while (result_frexp >= two_pow_16) { result_frexp = result_frexp / two_pow_16; t += 16; }
        }

        constexpr T two_pow_08 { UINT32_C(0x100) };
        constexpr T two_pow_04 { UINT32_C(0x10) };
        constexpr T two_pow_02 { UINT32_C(0x4) };

        if (result_frexp >= two_pow_08) { result_frexp = result_frexp / two_pow_08; t += 8; }
        if (result_frexp >= two_pow_04) { result_frexp = result_frexp / two_pow_04; t += 4; }
        if (result_frexp >= two_pow_02) { result_frexp = result_frexp / two_pow_02; t += 2; }

        constexpr T local_one { 1, 0 };
        constexpr T local_two { 2, 0 };

        while (result_frexp >= local_one)
        {
            result_frexp /= local_two;

            ++t;
        }

        if (expon != nullptr) { *expon = t + t_pre; }

        if(b_neg) { result_frexp = -result_frexp; }
    }

    return result_frexp;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto frexp(const T v, int* expon) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::frexp_impl(static_cast<evaluation_type>(v), expon));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_FREXP_HPP
