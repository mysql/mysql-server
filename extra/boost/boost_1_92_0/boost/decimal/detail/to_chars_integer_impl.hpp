// Copyright 2020-2023 Junekey Jeon
// Copyright 2022 Peter Dimov
// Copyright 2023-2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_TO_CHARS_INTEGER_IMPL_HPP
#define BOOST_TO_CHARS_INTEGER_IMPL_HPP

#include <boost/decimal/detail/apply_sign.hpp>
#include "int128.hpp"
#include <boost/decimal/detail/to_chars_result.hpp>
#include <boost/decimal/detail/memcpy.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <system_error>
#include <type_traits>
#include <array>
#include <limits>
#include <utility>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cstdint>
#include <climits>
#include <cmath>
#endif

namespace boost {
namespace decimal {
namespace detail {

#if !defined(__CUDACC__) || !defined(BOOST_DECIMAL_ENABLE_CUDA)
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE char digit_table[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z'
};
#endif

#if defined(__GNUC__) && __GNUC__ == 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

// Use a simple lookup table to put together the Integer in character form
template <typename Integer, typename Unsigned_Integer>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_chars_integer_impl(char* first, char* last, Integer value, int) noexcept
    BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_integral_v, Integer, detail::is_integral_v, Unsigned_Integer, to_chars_result)
{
    #if defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA)
    constexpr char digit_table[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z'
    };
    #endif

    const std::ptrdiff_t output_length = last - first;

    Unsigned_Integer unsigned_value {};

    BOOST_DECIMAL_IF_CONSTEXPR (std::is_signed<Integer>::value)
    {
        if (value < static_cast<Integer>(0))
        {
            *first++ = '-';
            unsigned_value = static_cast<Unsigned_Integer>(detail::apply_sign(value));
        }
        else
        {
            unsigned_value = static_cast<Unsigned_Integer>(value);
        }
    }
    else
    {
        unsigned_value = static_cast<Unsigned_Integer>(value);
    }

    constexpr auto buffer_size = sizeof(Unsigned_Integer) * CHAR_BIT;
    char buffer[buffer_size] {};
    const char* buffer_end = buffer + buffer_size;
    char* end = buffer + buffer_size - 1;

    // Work from LSB to MSB
    while (unsigned_value != 0U)
    {
        *end-- = digit_table[static_cast<std::size_t>(unsigned_value & 15U)]; // 1<<4 - 1
        unsigned_value >>= 4U;
    }

    const std::ptrdiff_t num_chars = buffer_end - end - 1;

    if (num_chars > output_length)
    {
        return {last, std::errc::value_too_large};
    }

    boost::decimal::detail::memcpy(first, buffer + (buffer_size - static_cast<unsigned long>(num_chars)), static_cast<std::size_t>(num_chars));

    return {first + num_chars, std::errc()};
}

// Specialization for base-10

#if !defined(__CUDACC__) || !defined(BOOST_DECIMAL_ENABLE_CUDA)
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE char radix_table[] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4',
    '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
    '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
    '2', '0', '2', '1', '2', '2', '2', '3', '2', '4',
    '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4',
    '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
    '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
    '5', '0', '5', '1', '5', '2', '5', '3', '5', '4',
    '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4',
    '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
    '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
    '8', '0', '8', '1', '8', '2', '8', '3', '8', '4',
    '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4',
    '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'
};
#endif

// See: https://jk-jeon.github.io/posts/2022/02/jeaiii-algorithm/
// https://arxiv.org/abs/2101.11408
BOOST_DECIMAL_CUDA_CONSTEXPR char* decompose32(std::uint32_t value, char* buffer) noexcept
{
    #if defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA)
    constexpr char radix_table[] = {
        '0', '0', '0', '1', '0', '2', '0', '3', '0', '4',
        '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
        '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
        '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
        '2', '0', '2', '1', '2', '2', '2', '3', '2', '4',
        '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
        '3', '0', '3', '1', '3', '2', '3', '3', '3', '4',
        '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
        '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
        '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
        '5', '0', '5', '1', '5', '2', '5', '3', '5', '4',
        '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
        '6', '0', '6', '1', '6', '2', '6', '3', '6', '4',
        '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
        '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
        '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
        '8', '0', '8', '1', '8', '2', '8', '3', '8', '4',
        '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
        '9', '0', '9', '1', '9', '2', '9', '3', '9', '4',
        '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'
    };
    #endif

    constexpr auto mask = (UINT64_C(1) << 57) - 1;
    auto y = value * UINT64_C(1441151881);

    for (std::size_t i {}; i < 10; i += 2)
    {
        boost::decimal::detail::memcpy(buffer + i, radix_table + (y >> 57) * 2, 2);
        y &= mask;
        y *= 100U;
    }

    return buffer + 10;
}

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4127 4146)
#endif

template <typename Integer, std::enable_if_t<sizeof(Integer) != 16, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR to_chars_result to_chars_integer_impl(char* first, char* last, Integer value) noexcept
{
    #if defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA)
    constexpr char radix_table[] = {
        '0', '0', '0', '1', '0', '2', '0', '3', '0', '4',
        '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
        '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
        '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
        '2', '0', '2', '1', '2', '2', '2', '3', '2', '4',
        '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
        '3', '0', '3', '1', '3', '2', '3', '3', '3', '4',
        '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
        '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
        '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
        '5', '0', '5', '1', '5', '2', '5', '3', '5', '4',
        '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
        '6', '0', '6', '1', '6', '2', '6', '3', '6', '4',
        '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
        '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
        '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
        '8', '0', '8', '1', '8', '2', '8', '3', '8', '4',
        '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
        '9', '0', '9', '1', '9', '2', '9', '3', '9', '4',
        '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'
    };
    #endif

    using Unsigned_Integer = make_unsigned_t<Integer>;
    Unsigned_Integer unsigned_value {};

    char buffer[10] {};
    int converted_value_digits {};
    bool is_negative = false;

    // Strip the sign from the value and apply at the end after parsing if the type is signed
    BOOST_DECIMAL_IF_CONSTEXPR (is_signed_v<Integer>)
    {
        if (value < static_cast<Integer>(0))
        {
            is_negative = true;
            unsigned_value = apply_sign(value);
        }
        else
        {
            unsigned_value = static_cast<Unsigned_Integer>(value);
        }
    }
    else
    {
        unsigned_value = static_cast<Unsigned_Integer>(value);
    }

    const std::ptrdiff_t user_buffer_size = last - first - static_cast<std::ptrdiff_t>(is_negative);

    // If the type is less than 32 bits we can use this without change
    // If the type is greater than 32 bits we use a binary search tree to figure out how many digits
    // are present and then decompose the value into two (or more) std::uint32_t of known length so that we
    // don't have the issue of removing leading zeros from the least significant digits

    // Yields: warning C4127: conditional expression is constant because first half of the expression is constant,
    // but we need to short circuit to avoid UB on the second half
    if (std::numeric_limits<Integer>::digits <= std::numeric_limits<std::uint32_t>::digits ||
        unsigned_value <= static_cast<Unsigned_Integer>((std::numeric_limits<std::uint32_t>::max)()))
    {
        const auto converted_value = static_cast<std::uint32_t>(unsigned_value);
        converted_value_digits = num_digits(converted_value);

        if (converted_value_digits > user_buffer_size)
        {
            return {last, std::errc::value_too_large};
        }

        decompose32(converted_value, buffer);

        if (is_negative)
        {
            *first++ = '-';
        }

        boost::decimal::detail::memcpy(first, buffer + (sizeof(buffer) - static_cast<unsigned>(converted_value_digits)),
                                        static_cast<std::size_t>(converted_value_digits));
    }
    else if (std::numeric_limits<Integer>::digits <= std::numeric_limits<std::uint64_t>::digits ||
             static_cast<std::uint64_t>(unsigned_value) <= (std::numeric_limits<std::uint64_t>::max)())
    {
        auto converted_value = static_cast<std::uint64_t>(unsigned_value);
        converted_value_digits = num_digits(converted_value);

        if (converted_value_digits > user_buffer_size)
        {
            return {last, std::errc::value_too_large};
        }

        if (is_negative)
        {
            *first++ = '-';
        }

        // Only store 9 digits in each to avoid overflow
        if (num_digits(converted_value) <= 18)
        {
            const auto x = static_cast<std::uint32_t>(converted_value / UINT64_C(1000000000));
            const auto y = static_cast<std::uint32_t>(converted_value % UINT64_C(1000000000));
            const int first_value_chars = num_digits(x);

            decompose32(x, buffer);
            boost::decimal::detail::memcpy(first, buffer + (sizeof(buffer) - static_cast<unsigned>(first_value_chars)),
                                            static_cast<std::size_t>(first_value_chars));

            decompose32(y, buffer);
            boost::decimal::detail::memcpy(first + first_value_chars, buffer + 1, sizeof(buffer) - 1);
        }
        else
        {
            const auto x = static_cast<std::uint32_t>(converted_value / UINT64_C(100000000000));
            converted_value -= x * UINT64_C(100000000000);
            const auto y = static_cast<std::uint32_t>(converted_value / UINT64_C(100));
            const auto z = static_cast<std::uint32_t>(converted_value % UINT64_C(100));

            if (converted_value_digits == 19)
            {
                decompose32(x, buffer);
                boost::decimal::detail::memcpy(first, buffer + 2, sizeof(buffer) - 2);

                decompose32(y, buffer);
                boost::decimal::detail::memcpy(first + 8, buffer + 1, sizeof(buffer) - 1);

                // Always prints 2 digits last
                boost::decimal::detail::memcpy(first + 17, radix_table + z * 2, 2);
            }
            else // 20
            {
                decompose32(x, buffer);
                boost::decimal::detail::memcpy(first, buffer + 1, sizeof(buffer) - 1);

                decompose32(y, buffer);
                boost::decimal::detail::memcpy(first + 9, buffer + 1, sizeof(buffer) - 1);

                // Always prints 2 digits last
                boost::decimal::detail::memcpy(first + 18, radix_table + z * 2, 2);
            }
        }
    }

    return {first + converted_value_digits, std::errc()};
}

template <typename Integer, std::enable_if_t<sizeof(Integer) == 16, bool> = true>
BOOST_DECIMAL_CUDA_CONSTEXPR to_chars_result to_chars_integer_impl(char* first, char* last, Integer value) noexcept
{
    using Unsigned_Integer = boost::int128::uint128_t;

    Unsigned_Integer unsigned_value {};

    const std::ptrdiff_t user_buffer_size = last - first;
    BOOST_DECIMAL_ATTRIBUTE_UNUSED bool is_negative = false;

    // Strip the sign from the value and apply at the end after parsing if the type is signed
    BOOST_DECIMAL_IF_CONSTEXPR (std::numeric_limits<Integer>::is_signed
                        #ifdef BOOST_DECIMAL_HAS_INT128
                        || std::is_same<boost::decimal::detail::builtin_int128_t, Integer>::value
                        #endif
                        )
    {
        if (value < 0U)
        {
            is_negative = true;
            unsigned_value = -(static_cast<Unsigned_Integer>(value));
        }
        else
        {
            unsigned_value = static_cast<Unsigned_Integer>(value);
        }
    }
    else
    {
        unsigned_value = static_cast<Unsigned_Integer>(value);
    }

    auto converted_value = unsigned_value;

    const int converted_value_digits = num_digits(converted_value);

    if (converted_value_digits > user_buffer_size)
    {
        return {last, std::errc::value_too_large};
    }

    if (is_negative)
    {
        *first++ = '-';
    }

    // If the value fits into 64 bits use the other method of processing
    if (converted_value < (std::numeric_limits<std::uint64_t>::max)())
    {
        return to_chars_integer_impl(first, last, static_cast<std::uint64_t>(value));
    }

    constexpr std::uint32_t ten_9 = UINT32_C(1000000000);
    char buffer[5][10] {};
    int num_chars[5] {};
    int i = 0;

    while (converted_value != 0U)
    {
        const auto digits = static_cast<std::uint32_t>(converted_value % ten_9);
        num_chars[i] = num_digits(digits);
        decompose32(digits, buffer[i]); // Always returns 10 digits (to include leading 0s) which we want
        converted_value = (converted_value - digits) / ten_9;
        ++i;
    }

    --i;
    auto offset = static_cast<std::size_t>(num_chars[i]);
    boost::decimal::detail::memcpy(first, buffer[i] + 10 - offset, offset);

    while (i > 0)
    {
        --i;
        boost::decimal::detail::memcpy(first + offset, buffer[i] + 1, 9);
        offset += 9;
    }

    return {first + converted_value_digits, std::errc()};
}

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#if defined(__GNUC__) && __GNUC__ == 7
#pragma GCC diagnostic pop
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_TO_CHARS_INTEGER_IMPL_HPP
