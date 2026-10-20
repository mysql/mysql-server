// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_CHARCONV_DETAIL_ISSIGNALING_HPP
#define BOOST_CHARCONV_DETAIL_ISSIGNALING_HPP

#include <boost/charconv/detail/config.hpp>
#include <boost/charconv/detail/bit_layouts.hpp>
#include <cstdint>
#include <cstring>

namespace boost { namespace charconv { namespace detail {

template <typename T>
inline bool issignaling BOOST_PREVENT_MACRO_SUBSTITUTION (T x) noexcept;

#if BOOST_CHARCONV_LDBL_BITS == 128 || defined(BOOST_CHARCONV_HAS_QUADMATH)

struct words128
{
#if BOOST_CHARCONV_ENDIAN_LITTLE_BYTE
    std::uint64_t lo;
    std::uint64_t hi;
#else
    std::uint64_t hi;
    std::uint64_t lo;
#endif
};

template <typename T>
inline bool issignaling BOOST_PREVENT_MACRO_SUBSTITUTION (T x) noexcept
{
    words128 bits;
    std::memcpy(&bits, &x, sizeof(T));

    std::uint64_t hi_word = bits.hi;
    std::uint64_t lo_word = bits.lo;

    hi_word ^= UINT64_C(0x0000800000000000);
    hi_word |= (lo_word | -lo_word) >> 63;
    return ((hi_word & INT64_MAX) > UINT64_C(0x7FFF800000000000));
}

#endif

// 16-bit non-finite bit values:
//
// float16_t
// SNAN: 0x7D00
// QNAN: 0x7E00
//  INF: 0x7C00
//
// bfloat16_t
// SNAN: 0x7FA0
// QNAN: 0x7FC0
//  INF: 0x7F80

#ifdef BOOST_CHARCONV_HAS_FLOAT16

template <>
inline bool issignaling<std::float16_t> BOOST_PREVENT_MACRO_SUBSTITUTION (std::float16_t x) noexcept
{
    std::uint16_t bits;
    std::memcpy(&bits, &x, sizeof(std::uint16_t));
    return bits >= UINT16_C(0x7D00) && bits < UINT16_C(0x7E00);
}

#endif

#ifdef BOOST_CHARCONV_HAS_BRAINFLOAT16

template <>
inline bool issignaling<std::bfloat16_t> BOOST_PREVENT_MACRO_SUBSTITUTION (std::bfloat16_t x) noexcept
{
    std::uint16_t bits;
    std::memcpy(&bits, &x, sizeof(std::uint16_t));
    return bits >= UINT16_C(0x7FA0) && bits < UINT16_C(0x7FC0);
}

#endif

}}} // Namespaces

#endif // BOOST_CHARCONV_DETAIL_ISSIGNALING_HPP
