// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_CLZ_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_CLZ_HPP

#include <boost/decimal/detail/int128/detail/config.hpp>

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <limits>
#include <cstdint>

#endif

namespace boost {
namespace int128 {
namespace detail {

namespace impl {

#if !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

// See: http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR int index64[64] = {
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

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int bit_scan_reverse(std::uint64_t bb) noexcept
{
    #if defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA)

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

    BOOST_DECIMAL_DETAIL_INT128_ASSUME(bb != 0); // LCOV_EXCL_LINE

    bb |= bb >> 1;
    bb |= bb >> 2;
    bb |= bb >> 4;
    bb |= bb >> 8;
    bb |= bb >> 16;
    bb |= bb >> 32;

    return index64[(bb * debruijn64) >> 58];
}

#if !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR int countl_mod37[37] = {
    32, 31, 6, 30, 9, 5, 0, 29,
    16, 8, 2, 4, 21, 0, 19, 28,
    25, 15, 0, 7, 10, 1, 17, 3,
    22, 20, 26, 0, 11, 18, 23,
    27, 12, 24, 13, 14, 0
};

#endif

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int backup_countl_impl(std::uint32_t x) noexcept
{
    #if defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA)

    constexpr int countl_mod37[37] = {
        32, 31, 6, 30, 9, 5, 0, 29,
        16, 8, 2, 4, 21, 0, 19, 28,
        25, 15, 0, 7, 10, 1, 17, 3,
        22, 20, 26, 0, 11, 18, 23,
        27, 12, 24, 13, 14, 0
    };

    #endif

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return countl_mod37[x % 37];
}

#if BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_clz) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

constexpr int countl_impl(unsigned int x) noexcept
{
    return x ? __builtin_clz(x) : std::numeric_limits<unsigned int>::digits;
}

constexpr int countl_impl(unsigned long x) noexcept
{
    return x ? __builtin_clzl(x) : std::numeric_limits<unsigned long>::digits;
}

constexpr int countl_impl(unsigned long long x) noexcept
{
    return x ? __builtin_clzll(x) : std::numeric_limits<unsigned long long>::digits;
}

#elif (defined(_M_AMD64) || defined(_M_ARM64)) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

constexpr int countl_impl(std::uint32_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return backup_countl_impl(x); // LCOV_EXCL_LINE
    }
    else
    {
        unsigned long r {};

        if (_BitScanReverse(&r, x))
        {
            return 31 - static_cast<int>(r);
        }
        else
        {
            return 32;
        }
    }
}

constexpr int countl_impl(std::uint64_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return x ? bit_scan_reverse(static_cast<std::uint64_t>(x)) ^ 63 : std::numeric_limits<std::uint64_t>::digits; // LCOV_EXCL_LINE
    }
    else
    {
        unsigned long r {};

        if (_BitScanReverse64(&r, x))
        {
            return 63 - static_cast<int>(r);
        }
        else
        {
            return 64;
        }
    }
}

#elif defined(_M_IX86) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

constexpr int countl_impl(std::uint32_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return backup_countl_impl(x); // LCOV_EXCL_LINE
    }
    else
    {
        unsigned long r {};

        if (_BitScanReverse(&r, x))
        {
            return 31 - static_cast<int>(r);
        }
        else
        {
            return 32;
        }
    }
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countl_impl(std::uint64_t x) noexcept
{
    return x ? bit_scan_reverse(static_cast<std::uint64_t>(x)) ^ 63 : std::numeric_limits<std::uint64_t>::digits;
}

#else

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countl_impl(T x) noexcept
{
    return x ? bit_scan_reverse(static_cast<std::uint64_t>(x)) ^ 63 : std::numeric_limits<T>::digits;
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countl_impl(std::uint32_t x) noexcept
{
    return backup_countl_impl(x);
}


#endif

} // namespace impl

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countl_zero(T x) noexcept
{
    static_assert(std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed,
                  "Can only count with unsigned integers");

    return impl::countl_impl(x);
}

} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_CLZ_HPP
