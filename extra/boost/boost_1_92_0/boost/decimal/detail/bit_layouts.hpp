// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_BIT_LAYOUTS_HPP
#define BOOST_DECIMAL_DETAIL_BIT_LAYOUTS_HPP

#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <cfloat>
#endif

// Layouts of floating point types as specified by IEEE 754
// See page 23 of IEEE 754-2008

namespace boost { namespace decimal { namespace detail {

struct ieee754_binary32
{
    static constexpr int significand_bits = 23;
    static constexpr int exponent_bits = 8;
    static constexpr int min_exponent = -126;
    static constexpr int max_exponent = 127;
    static constexpr int exponent_bias = -127;
    static constexpr int decimal_digits = 9;
};

struct ieee754_binary64
{
    static constexpr int significand_bits = 52;
    static constexpr int exponent_bits = 11;
    static constexpr int min_exponent = -1022;
    static constexpr int max_exponent = 1023;
    static constexpr int exponent_bias = -1023;
    static constexpr int decimal_digits = 17;
};

// 80 bit long double (e.g. x86-64)
#if LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384

struct IEEEl2bits
{
#if BOOST_DECIMAL_ENDIAN_LITTLE_BYTE
    std::uint32_t mantissa_l : 32;
    std::uint32_t mantissa_h : 32;
    std::uint32_t exponent : 15;
    std::uint32_t sign : 1;
    std::uint32_t pad : 32;
#else // Big endian
    std::uint32_t pad : 32;
    std::uint32_t sign : 1;
    std::uint32_t exponent : 15;
    std::uint32_t mantissa_h : 32;
    std::uint32_t mantissa_l : 32;
#endif
};

struct ieee754_binary80
{
    static constexpr int significand_bits = 63;
    static constexpr int exponent_bits = 15;
    static constexpr int min_exponent = -16382;
    static constexpr int max_exponent = 16383;
    static constexpr int exponent_bias = 16383;
    static constexpr int decimal_digits = 18;
};

#define BOOST_DECIMAL_LDBL_BITS 80

// 128 bit long double (e.g. s390x, ppcle64)
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384

struct IEEEl2bits
{
#if BOOST_DECIMAL_ENDIAN_LITTLE_BYTE
    std::uint64_t mantissa_l : 64;
    std::uint64_t mantissa_h : 48;
    std::uint32_t exponent : 15;
    std::uint32_t sign : 1;
#else // Big endian
    std::uint32_t sign : 1;
    std::uint32_t exponent : 15;
    std::uint64_t mantissa_h : 48;
    std::uint64_t mantissa_l : 64;
#endif
};

#define BOOST_DECIMAL_LDBL_BITS 128

// 64 bit long double (double == long double on ARM)
#elif LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024

struct IEEEl2bits
{
#if (defined(BOOST_DECIMAL_ENDIAN_LITTLE_BYTE) && (BOOST_DECIMAL_ENDIAN_LITTLE_BYTE != 0))
    std::uint32_t mantissa_l : 32;
    std::uint32_t mantissa_h : 20;
    std::uint32_t exponent : 11;
    std::uint32_t sign : 1;
#else // Big endian
    std::uint32_t sign : 1;
    std::uint32_t exponent : 11;
    std::uint32_t mantissa_h : 20;
    std::uint32_t mantissa_l : 32;
#endif
};

#define BOOST_DECIMAL_LDBL_BITS 64

#else // Unsupported long double representation
#  define BOOST_DECIMAL_LDBL_BITS 0
#  define BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
#endif

struct IEEEbinary128
{
#if (defined(BOOST_DECIMAL_ENDIAN_LITTLE_BYTE) && (BOOST_DECIMAL_ENDIAN_LITTLE_BYTE != 0))
    std::uint64_t mantissa_l : 64;
    std::uint64_t mantissa_h : 48;
    std::uint32_t exponent : 15;
    std::uint32_t sign : 1;
#else // Big endian
    std::uint32_t sign : 1;
    std::uint32_t exponent : 15;
    std::uint64_t mantissa_h : 48;
    std::uint64_t mantissa_l : 64;
#endif
};

struct ieee754_binary128
{
    static constexpr int significand_bits = 112;
    static constexpr int exponent_bits = 15;
    static constexpr int min_exponent = -16382;
    static constexpr int max_exponent = 16383;
    static constexpr int exponent_bias = 16383;
    static constexpr int decimal_digits = 33;
};

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_BIT_LAYOUTS_HPP
