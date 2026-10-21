// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_CHARCONV_DETAIL_FROM_CHARS_INTEGER_IMPL_HPP
#define BOOST_CHARCONV_DETAIL_FROM_CHARS_INTEGER_IMPL_HPP

#include <boost/charconv/detail/apply_sign.hpp>
#include <boost/charconv/detail/config.hpp>
#include <boost/charconv/detail/from_chars_result.hpp>
#include <boost/charconv/detail/emulated128.hpp>
#include <boost/charconv/detail/type_traits.hpp>
#include <boost/charconv/config.hpp>
#include <boost/config.hpp>
#include <system_error>
#include <type_traits>
#include <limits>
#include <cstdlib>
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace boost { namespace charconv { namespace detail {

#if !(defined(BOOST_CHARCONV_ENABLE_CUDA) && defined(__CUDACC__))

static constexpr unsigned char uchar_values[] =
     {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        0,   1,   2,   3,   4,   5,   6,   7,   8,   9, 255, 255, 255, 255, 255, 255,
      255,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
       25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35, 255, 255, 255, 255, 255,
      255,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
       25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

static_assert(sizeof(uchar_values) == 256, "uchar_values should represent all 256 values of unsigned char");

static constexpr double log_2_table[] =
{
    0.0,
    0.0,
    1.0,
    0.630929753571,
    0.5,
    0.430676558073,
    0.386852807235,
    0.356207187108,
    0.333333333333,
    0.315464876786,
    0.301029995664,
    0.289064826318,
    0.278942945651,
    0.270238154427,
    0.262649535037,
    0.255958024810,
    0.25,
    0.244650542118,
    0.239812466568,
    0.235408913367,
    0.231378213160,
    0.227670248697,
    0.224243824218,
    0.221064729458,
    0.218104291986,
    0.215338279037,
    0.212746053553,
    0.210309917857,
    0.208014597677,
    0.205846832460,
    0.203795047091,
    0.201849086582,
    0.2,
    0.198239863171,
    0.196561632233,
    0.194959021894,
    0.193426403617
};

#endif // __NVCC__

// Convert characters for 0-9, A-Z, a-z to 0-35. Anything else is 255
BOOST_CHARCONV_HOST_DEVICE constexpr unsigned char digit_from_char(const char val) noexcept
{
    #if defined(BOOST_CHARCONV_ENABLE_CUDA) && defined(__CUDACC__)

    constexpr unsigned char uchar_values[] =
    {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
       0,   1,   2,   3,   4,   5,   6,   7,   8,   9, 255, 255, 255, 255, 255, 255,
     255,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
      25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35, 255, 255, 255, 255, 255,
     255,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
      25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

    #endif // __NVCC__

    return uchar_values[static_cast<unsigned char>(val)];
}

#ifdef BOOST_MSVC
# pragma warning(push)
# pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
# pragma warning(disable: 4189) // 'is_negative': local variable is initialized but not referenced
# pragma warning(disable: 4127) // Conditional expression is constant (if constexpr pre-c++17)
#elif defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wconstant-conversion"

#elif defined(__GNUC__) && (__GNUC__ < 7)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Woverflow"
# pragma GCC diagnostic ignored "-Wconversion"
# pragma GCC diagnostic ignored "-Wsign-conversion"

#elif defined(__GNUC__) && (__GNUC__ >= 7)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
# pragma GCC diagnostic ignored "-Wconversion"

#endif

#if defined(BOOST_CHARCONV_ENABLE_CUDA) && defined(__CUDACC__)

template <typename T>
__host__ __device__ constexpr T get_max_value()
{
    using UT = typename std::make_unsigned<T>::type;
    return std::is_signed<T>::value
        ? static_cast<T>(static_cast<UT>(-1) >> 1)
        : static_cast<T>(static_cast<UT>(-1));
}

#else

template <typename T>
constexpr T get_max_value()
{
    return (std::numeric_limits<T>::max)();
}

#endif

template <typename Integer, typename Unsigned_Integer>
BOOST_CHARCONV_HOST_DEVICE BOOST_CXX14_CONSTEXPR from_chars_result from_chars_integer_impl(const char* first, const char* last, Integer& value, int base) noexcept
{
    #if defined(BOOST_CHARCONV_ENABLE_CUDA) && defined(__CUDACC__)

    constexpr double log_2_table[] =
    {
        0.0,
        0.0,
        1.0,
        0.630929753571,
        0.5,
        0.430676558073,
        0.386852807235,
        0.356207187108,
        0.333333333333,
        0.315464876786,
        0.301029995664,
        0.289064826318,
        0.278942945651,
        0.270238154427,
        0.262649535037,
        0.255958024810,
        0.25,
        0.244650542118,
        0.239812466568,
        0.235408913367,
        0.231378213160,
        0.227670248697,
        0.224243824218,
        0.221064729458,
        0.218104291986,
        0.215338279037,
        0.212746053553,
        0.210309917857,
        0.208014597677,
        0.205846832460,
        0.203795047091,
        0.201849086582,
        0.2,
        0.198239863171,
        0.196561632233,
        0.194959021894,
        0.193426403617
    };

    #endif // __NVCC__

    Unsigned_Integer result = 0;
    Unsigned_Integer overflow_value = 0;
    Unsigned_Integer max_digit = 0;
    
    // Check pre-conditions
    if (!((first <= last) && (base >= 2 && base <= 36)))
    {
        return {first, std::errc::invalid_argument};
    }

    const auto unsigned_base = static_cast<Unsigned_Integer>(base);

    // Strip sign if the type is signed
    // Negative sign will be appended at the end of parsing
    BOOST_ATTRIBUTE_UNUSED bool is_negative = false;
    auto next = first;

    BOOST_CHARCONV_IF_CONSTEXPR (is_signed<Integer>::value)
    {
        if (next != last)
        {
            if (*next == '-')
            {
                is_negative = true;
                ++next;
            }
            else if (*next == '+' || *next == ' ')
            {
                return {next, std::errc::invalid_argument};
            }
        }

        #ifdef BOOST_CHARCONV_HAS_INT128
        BOOST_IF_CONSTEXPR (std::is_same<Integer, boost::int128_type>::value)
        {
            overflow_value = BOOST_CHARCONV_INT128_MAX;
            max_digit = BOOST_CHARCONV_INT128_MAX;
        }
        else
        #endif
        {
            overflow_value = static_cast<Unsigned_Integer>(get_max_value<Integer>());
            max_digit = static_cast<Unsigned_Integer>(get_max_value<Integer>());
        }

        if (is_negative)
        {
            ++overflow_value;
            ++max_digit;
        }
    }
    else
    {
        if (next != last && (*next == '-' || *next == '+' || *next == ' '))
        {
            return {first, std::errc::invalid_argument};
        }
        
        #ifdef BOOST_CHARCONV_HAS_INT128
        BOOST_IF_CONSTEXPR (std::is_same<Integer, boost::uint128_type>::value)
        {
            overflow_value = BOOST_CHARCONV_UINT128_MAX;
            max_digit = BOOST_CHARCONV_UINT128_MAX;
        }
        else
        #endif
        {
            overflow_value = get_max_value<Unsigned_Integer>();
            max_digit = get_max_value<Unsigned_Integer>();
        }
    }

    BOOST_IF_CONSTEXPR ((std::numeric_limits<Integer>::is_signed && sizeof(Integer) == 16)
                        #ifdef BOOST_CHARCONV_HAS_INT128
                        || std::is_same<boost::int128_type, Integer>::value
                        #endif
                        )
    {
        overflow_value /= unsigned_base;
        max_digit %= unsigned_base;
        #ifndef __GLIBCXX_TYPE_INT_N_0
        if (base != 10)
        {
            // Overflow value would cause INT128_MIN in non-base10 to fail
            overflow_value *= static_cast<Unsigned_Integer>(2);
        }
        #endif
    }
    else
    {
        overflow_value /= unsigned_base;
        max_digit %= unsigned_base;
    }

    // If the only character was a sign abort now
    if (next == last)
    {
        return {first, std::errc::invalid_argument};
    }

    bool overflowed = false;

    const std::ptrdiff_t nc = last - next;

    // In non-GNU mode on GCC numeric limits may not be specialized
    #if defined(BOOST_CHARCONV_HAS_INT128) && !defined(__GLIBCXX_TYPE_INT_N_0)
    constexpr std::ptrdiff_t nd_2 = std::is_same<Integer, boost::int128_type>::value ? 127 :
                                    std::is_same<Integer, boost::uint128_type>::value ? 128 :
                                    std::numeric_limits<Integer>::digits;
    #else
    constexpr std::ptrdiff_t nd_2 = std::numeric_limits<Integer>::digits;
    #endif

    const auto nd = static_cast<std::ptrdiff_t>(nd_2 * log_2_table[static_cast<std::size_t>(unsigned_base)]);

    {
        // Check that the first character is valid before proceeding
        const unsigned char first_digit = digit_from_char(*next);

        if (first_digit >= unsigned_base)
        {
            return {first, std::errc::invalid_argument};
        }

        result = static_cast<Unsigned_Integer>(result * unsigned_base + first_digit);
        ++next;
        std::ptrdiff_t i = 1;

        for( ; i < nd && i < nc; ++i )
        {
            // overflow is not possible in the first nd characters

            const unsigned char current_digit = digit_from_char(*next);

            if (current_digit >= unsigned_base)
            {
                break;
            }

            result = static_cast<Unsigned_Integer>(result * unsigned_base + current_digit);
            ++next;
        }

        for( ; i < nc; ++i )
        {
            const unsigned char current_digit = digit_from_char(*next);

            if (current_digit >= unsigned_base)
            {
                break;
            }

            if (result < overflow_value || (result == overflow_value && current_digit <= max_digit))
            {
                result = static_cast<Unsigned_Integer>(result * unsigned_base + current_digit);
            }
            else
            {
                // Required to keep updating the value of next, but the result is garbage
                overflowed = true;
            }

            ++next;
        }
    }

    // Return the parsed value, adding the sign back if applicable
    // If we have overflowed then we do not return the result 
    if (overflowed)
    {
        return {next, std::errc::result_out_of_range};
    }

    value = static_cast<Integer>(result);

    BOOST_IF_CONSTEXPR (is_signed<Integer>::value)
    {
        if (is_negative)
        {
            value = static_cast<Integer>(-(static_cast<Unsigned_Integer>(value)));
        }
    }

    return {next, std::errc()};
}

#ifdef BOOST_MSVC
# pragma warning(pop)
#elif defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

// Only from_chars for integer types is constexpr (as of C++23)
template <typename Integer>
BOOST_CHARCONV_HOST_DEVICE BOOST_CHARCONV_GCC5_CONSTEXPR from_chars_result from_chars(const char* first, const char* last, Integer& value, int base = 10) noexcept
{
    using Unsigned_Integer = typename std::make_unsigned<Integer>::type;
    return detail::from_chars_integer_impl<Integer, Unsigned_Integer>(first, last, value, base);
}

#ifdef BOOST_CHARCONV_HAS_INT128
template <typename Integer>
BOOST_CHARCONV_GCC5_CONSTEXPR from_chars_result from_chars128(const char* first, const char* last, Integer& value, int base = 10) noexcept
{
    using Unsigned_Integer = boost::uint128_type;
    return detail::from_chars_integer_impl<Integer, Unsigned_Integer>(first, last, value, base);
}
#endif

#if !(defined(BOOST_CHARCONV_ENABLE_CUDA) && defined(__CUDACC__))
BOOST_CHARCONV_GCC5_CONSTEXPR from_chars_result from_chars128(const char* first, const char* last, uint128& value, int base = 10) noexcept
{
    return from_chars_integer_impl<uint128, uint128>(first, last, value, base);
}
#endif

}}} // Namespaces

#endif // BOOST_CHARCONV_DETAIL_FROM_CHARS_INTEGER_IMPL_HPP
