// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_CTZ_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_CTZ_HPP

#include <boost/decimal/detail/int128/detail/config.hpp>

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <limits>
#include <cstdint>

#endif

namespace boost {
namespace int128 {
namespace detail {

namespace impl {

#if BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_ctz) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

constexpr int countr_impl(unsigned int x) noexcept
{
    return x ? __builtin_ctz(x) : std::numeric_limits<unsigned int>::digits;
}

constexpr int countr_impl(unsigned long x) noexcept
{
    return x ? __builtin_ctzl(x) : std::numeric_limits<unsigned long>::digits;
}

constexpr int countr_impl(unsigned long long x) noexcept
{
    return x ? __builtin_ctzll(x) : std::numeric_limits<unsigned long long>::digits;
}

#endif

#if !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR int countr_mod37[37] = {
    32, 0, 1, 26, 2, 23, 27, 0,
    3, 16, 24, 30, 28, 11, 0, 13,
    4, 7, 17, 0, 25, 22, 31, 15,
    29, 10, 12, 6, 0, 21, 14, 9,
    5, 20, 8, 19, 18
};

#endif

#if defined(_MSC_VER) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_ctz)

#pragma warning(push)
#pragma warning(disable : 4146) // unary minus operator applied to unsigned type, result still unsigned

constexpr int countr_impl(std::uint32_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return countr_mod37[(-x & x) % 37]; // LCOV_EXCL_LINE
    }
    else
    {
        unsigned long r {};

        if(_BitScanForward(&r, x))
        {
            return static_cast<int>(r);
        }
        else
        {
            return 32;
        }
    }
}

#pragma warning(pop)

#elif !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_ctz) || (defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countr_impl(std::uint32_t x) noexcept
{
    #if defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA)

    constexpr int countr_mod37[37] = {
        32, 0, 1, 26, 2, 23, 27, 0,
        3, 16, 24, 30, 28, 11, 0, 13,
        4, 7, 17, 0, 25, 22, 31, 15,
        29, 10, 12, 6, 0, 21, 14, 9,
        5, 20, 8, 19, 18
    };

    #endif

    return countr_mod37[(-x & x) % 37];
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif

#if (defined(_M_AMD64) || defined(_M_ARM64)) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_ctz) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

constexpr int countr_impl(std::uint64_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return static_cast<std::uint32_t>(x) != 0 ? countr_impl(static_cast<std::uint32_t>(x)) : countr_impl(static_cast<std::uint32_t>(x >> 32)) + 32; // LCOV_EXCL_LINE
    }
    else
    {
        unsigned long r {};

        if(_BitScanForward64(&r, x))
        {
            return static_cast<int>(r);
        }
        else
        {
            return 64;
        }
    }
}

#elif !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_ctz) || (defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countr_impl(std::uint64_t x) noexcept
{
    return static_cast<std::uint32_t>(x) != 0 ? countr_impl(static_cast<std::uint32_t>(x)) :
                                                countr_impl(static_cast<std::uint32_t>(x >> 32)) + 32;
}

#endif

} // namespace impl

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int countr_zero(T x) noexcept
{
    static_assert(std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed,
                  "Can only count with unsigned integers");

    return impl::countr_impl(x);
}

} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_CTZ_HPP
