// Copyright 2022 Peter Dimov
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_COUNTL_HPP
#define BOOST_DECIMAL_DETAIL_COUNTL_HPP

#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include "int128.hpp"
#include <cstdint>
#include <limits>
#endif

namespace boost {
namespace decimal {
namespace detail {
namespace impl {

#if BOOST_DECIMAL_HAS_BUILTIN(__builtin_clz) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))

BOOST_DECIMAL_CUDA_CONSTEXPR int countl_impl(unsigned char x) noexcept
{
    return x ? __builtin_clz(x) -
               (std::numeric_limits<unsigned int>::digits - std::numeric_limits<unsigned char>::digits)
             : std::numeric_limits<unsigned char>::digits;
}

BOOST_DECIMAL_CUDA_CONSTEXPR int countl_impl(unsigned short x) noexcept
{
    return x ? __builtin_clz(x) -
               (std::numeric_limits<unsigned int>::digits - std::numeric_limits<unsigned short>::digits)
             : std::numeric_limits<unsigned short>::digits;
}

BOOST_DECIMAL_CUDA_CONSTEXPR int countl_impl(unsigned int x) noexcept
{
    return x ? __builtin_clz(x) : std::numeric_limits<unsigned int>::digits;
}

BOOST_DECIMAL_CUDA_CONSTEXPR int countl_impl(unsigned long x) noexcept
{
    return x ? __builtin_clzl(x) : std::numeric_limits<unsigned long>::digits;
}

BOOST_DECIMAL_CUDA_CONSTEXPR int countl_impl(unsigned long long x) noexcept
{
    return x ? __builtin_clzll(x) : std::numeric_limits<unsigned long long>::digits;
}

#else

#if !(defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE int index64[64] = {
    0, 47,  1, 56, 48, 27,  2, 60,
    57, 49, 41, 37, 28, 16,  3, 61,
    54, 58, 35, 52, 50, 42, 21, 44,
    38, 32, 29, 23, 17, 11,  4, 62,
    46, 55, 26, 59, 40, 36, 15, 53,
    34, 51, 20, 43, 31, 22, 10, 45,
    25, 39, 14, 33, 19, 30,  9, 24,
    13, 18,  8, 12,  7,  6,  5, 63
};

#endif

// See: http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
BOOST_DECIMAL_CUDA_CONSTEXPR auto bit_scan_reverse(std::uint64_t bb) noexcept -> int
{
    #if (defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))

    constexpr int index64[64] = {
        0, 47,  1, 56, 48, 27,  2, 60,
        57, 49, 41, 37, 28, 16,  3, 61,
        54, 58, 35, 52, 50, 42, 21, 44,
        38, 32, 29, 23, 17, 11,  4, 62,
        46, 55, 26, 59, 40, 36, 15, 53,
        34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30,  9, 24,
        13, 18,  8, 12,  7,  6,  5, 63
    };

    #endif

    constexpr auto debruijn64 {UINT64_C(0x03f79d71b4cb0a89)};

    BOOST_DECIMAL_ASSERT(bb != 0);

    bb |= bb >> 1;
    bb |= bb >> 2;
    bb |= bb >> 4;
    bb |= bb >> 8;
    bb |= bb >> 16;
    bb |= bb >> 32;

    return index64[(bb * debruijn64) >> 58];
}

template <typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR int countl_impl(T x) noexcept
{
    return x ? bit_scan_reverse(static_cast<std::uint64_t>(x)) ^ 63 : std::numeric_limits<T>::digits;
}

#endif

} //namespace impl

template <typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR int countl_zero(T x) noexcept
{
    static_assert(std::is_integral<T>::value && !std::numeric_limits<T>::is_signed,
                  "Can only count with unsigned integers");

    return impl::countl_impl(x);
}

template <>
BOOST_DECIMAL_CUDA_CONSTEXPR int countl_zero(const int128::uint128_t x) noexcept
{
    return int128::countl_zero(x);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_COUNTL_HPP
