// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_COMMON_MUL_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_COMMON_MUL_HPP

#include <boost/decimal/detail/int128/detail/config.hpp>

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <cstdint>
#include <cstring>

#endif

namespace boost {
namespace int128 {
namespace detail {

// See: The Art of Computer Programming Volume 2 (Semi-numerical algorithms) section 4.3.1
// Algorithm M: Multiplication of Non-negative integers
template <typename ReturnType, std::size_t u_size, std::size_t v_size>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr ReturnType knuth_multiply(const std::uint32_t (&u)[u_size],
                                                              const std::uint32_t (&v)[v_size]) noexcept
{
    using high_word_type = decltype(ReturnType{}.high);

    std::uint32_t w[u_size + v_size] {};

    // M.1
    for (std::size_t j {}; j < v_size; ++j)
    {
        // M.2
        if (v[j] == 0)
        {
            w[j + u_size] = 0;
            continue;
        }

        // M.3
        std::uint64_t t {};
        for (std::size_t i {}; i < u_size; ++i)
        {
            // M.4
            t += static_cast<std::uint64_t>(u[i]) * v[j] + w[i + j];
            w[i + j] = static_cast<std::uint32_t>(t);
            t >>= 32u;
        }

        // M.5
        w[j + u_size] = static_cast<std::uint32_t>(t);
    }

    const auto low {static_cast<std::uint64_t>(w[0]) | (static_cast<std::uint64_t>(w[1]) << 32)};
    const auto high {static_cast<std::uint64_t>(w[2]) | (static_cast<std::uint64_t>(w[3]) << 32)};

    return {static_cast<high_word_type>(high), low};
}

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void to_words(const T& x, std::uint32_t (&words)[4]) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION

    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(&words, &x, sizeof(T));
        return;
    }

    #endif

    words[0] = static_cast<std::uint32_t>(x.low & UINT32_MAX);                                  // LCOV_EXCL_LINE
    words[1] = static_cast<std::uint32_t>(x.low >> 32);                                         // LCOV_EXCL_LINE
    words[2] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) & UINT32_MAX);     // LCOV_EXCL_LINE
    words[3] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) >> 32);            // LCOV_EXCL_LINE
}


BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void to_words(const std::uint64_t x, std::uint32_t (&words)[2]) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION

    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(&words, &x, sizeof(std::uint64_t));
        return;
    }

    #endif

    words[0] = static_cast<std::uint32_t>(x & UINT32_MAX);  // LCOV_EXCL_LINE
    words[1] = static_cast<std::uint32_t>(x >> 32);         // LCOV_EXCL_LINE
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr void to_words(const std::uint32_t x, std::uint32_t (&words)[1]) noexcept
{
    words[0] = x;
}

} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_COMMON_MUL_HPP
