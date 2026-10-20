// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_PARSER_HPP
#define BOOST_DECIMAL_DETAIL_PARSER_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/from_chars_result.hpp>
#include <boost/decimal/detail/from_chars_integer_impl.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include <boost/decimal/detail/chars_format.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#if !defined(BOOST_DECIMAL_DISABLE_CLIB)
#include <system_error>
#endif
#include <type_traits>

#endif // BOOST_DECIMAL_BUILD_MODULE

namespace boost {
namespace decimal {
namespace detail {

BOOST_DECIMAL_CUDA_CONSTEXPR auto is_integer_char(char c) noexcept -> bool
{
    return (c >= '0') && (c <= '9');
}

BOOST_DECIMAL_CUDA_CONSTEXPR auto is_hex_char(char c) noexcept -> bool
{
    return is_integer_char(c) || (((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')));
}

BOOST_DECIMAL_CUDA_CONSTEXPR auto is_payload_char(const char c) noexcept -> bool
{
    return is_integer_char(c) || (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
}

BOOST_DECIMAL_CUDA_CONSTEXPR auto is_delimiter(char c, chars_format fmt) noexcept -> bool
{
    if (fmt != chars_format::hex)
    {
        return !is_integer_char(c) && c != 'e' && c != 'E';
    }

    return !is_hex_char(c) && c != 'p' && c != 'P';
}

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_chars_dispatch(const char* first, const char* last, std::uint64_t& value, int base) noexcept -> from_chars_result
{
    return boost::decimal::detail::from_chars(first, last, value, base);
}

BOOST_DECIMAL_CUDA_CONSTEXPR auto from_chars_dispatch(const char* first, const char* last, int128::uint128_t& value, int base) noexcept -> from_chars_result
{
    return boost::decimal::detail::from_chars128(first, last, value, base);
}
#endif

#ifdef BOOST_DECIMAL_HAS_INT128
BOOST_DECIMAL_CUDA_CONSTEXPR auto from_chars_dispatch(const char* first, const char* last, builtin_uint128_t& value, int base) noexcept -> from_chars_result
{
    return boost::decimal::detail::from_chars128(first, last, value, base);
}
#endif

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)
template <typename Unsigned_Integer, typename Integer>
BOOST_DECIMAL_CUDA_CONSTEXPR auto parser(const char* first, const char* last, bool& sign, Unsigned_Integer& significand, Integer& exponent, const chars_format fmt = chars_format::general) noexcept -> from_chars_result
{
    if (first >= last)
    {
        return {first, std::errc::invalid_argument};
    }

    auto next = first;
    bool all_zeros = true;

    // First extract the sign
    if (*next == '-')
    {
        sign = true;
        ++next;
    }
    else if (*next == '+')
    {
        return {next, std::errc::invalid_argument};
    }
    else
    {
        sign = false;
    }

    constexpr std::size_t significand_buffer_size = std::numeric_limits<Unsigned_Integer>::digits10 ;
    char significand_buffer[significand_buffer_size] {};

    // Handle non-finite values
    // Stl allows for string like "iNf" to return inf
    //
    // This is nested ifs rather than a big one-liner to ensure that once we hit an invalid character
    // or an end of buffer we return the correct value of next
    bool signaling {};
    if (next != last && (*next == 'i' || *next == 'I'))
    {
        ++next;
        if (next != last && (*next == 'n' || *next == 'N'))
        {
            ++next;
            if (next != last && (*next == 'f' || *next == 'F'))
            {
                ++next;
                exponent = 0;
                return {next, std::errc::value_too_large};
            }
        }

        return {first, std::errc::invalid_argument};
    }

    if (next != last && (*next == 's' || *next == 'S'))
    {
        ++next;
        signaling = true;
    }

    if (next != last && (*next == 'n' || *next == 'N'))
    {
        ++next;
        if (next != last && (*next == 'a' || *next == 'A'))
        {
            ++next;
            if (next != last && (*next == 'n' || *next == 'N'))
            {
                ++next;
                if (next != last)
                {
                    const auto current_pos {next};

                    bool any_valid_char {false};
                    bool has_opening_brace {false};
                    if (*next == '(')
                    {
                        ++next;
                        has_opening_brace = true;
                    }

                    // Handle nan(SNAN)
                    if ((last - next) >= 4 && (*next == 's' || *next == 'S') && (*(next + 1) == 'n' || *(next + 1) == 'N')
                        && (*(next + 2) == 'a' || *(next + 2) == 'A') && (*(next + 3) == 'n' || *(next + 3) == 'N'))
                    {
                        next += 4;
                        signaling = true;
                        any_valid_char = true;
                    }
                    // Handle Nan(IND)
                    else if ((last - next) >= 3 && (*next == 'i' || *next == 'I') && (*(next + 1) == 'n' || *(next + 1) == 'N')
                        && (*(next + 2) == 'd' || *(next + 2) == 'D'))
                    {
                        next += 3;
                        sign = true;
                        any_valid_char = true;
                    }

                    // Arbitrary numerical payload
                    bool has_numerical_payload {false};
                    auto significand_buffer_first {significand_buffer};
                    std::size_t significand_characters {};
                    while (next != last && (*next != ')'))
                    {
                        if (significand_characters < significand_buffer_size && is_integer_char(*next))
                        {
                            ++significand_characters;
                            *significand_buffer_first++ = *next++;
                            any_valid_char = true;
                            has_numerical_payload = true;
                        }
                        else
                        {
                            // End of valid payload even if there are more characters
                            // e.g. SNAN42JUNK stops at J
                            break;
                        }
                    }

                    // Non-numerical payload still needs to be parsed
                    // e.g. nan(PAYLOAD)
                    if (!has_numerical_payload && has_opening_brace)
                    {
                        while (next != last && (*next != ')'))
                        {
                            if (is_payload_char(*next))
                            {
                                any_valid_char = true;
                                ++next;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    if (next != last && any_valid_char)
                    {
                        // One past the end if we need to
                        ++next;
                    }

                    if (significand_characters != 0)
                    {
                        from_chars_dispatch(significand_buffer, significand_buffer + significand_characters, significand, 10);
                    }

                    if (!any_valid_char)
                    {
                        // If we have nan(..BAD..) we should point to (
                        next = current_pos;
                    }

                    exponent = static_cast<Integer>(signaling);
                    return {next, std::errc::not_supported};
                }
                else
                {
                    exponent = static_cast<Integer>(signaling);
                    return {next, std::errc::not_supported};
                }
            }
        }

        return {first, std::errc::invalid_argument};
    }

    // Ignore leading zeros (e.g. 00005 or -002.3e+5)
    while (next != last && *next == '0')
    {
        ++next;
    }

    const char exp_char {fmt != chars_format::hex ? 'e' : 'p'};
    const char capital_exp_char {fmt != chars_format::hex ? 'E' : 'P'};

    // Plain "0" (or "00...") with no fractional or exponent part: significand and exponent are both 0.
    if (next == last)
    {
        significand = 0;
        exponent = 0;
        return {next, std::errc()};
    }

    // "0e-3" or "0E+5": significand is 0, but the exponent must be parsed so the cohort is preserved
    // per IEEE 754-2008 3.5.1 ("the cohort of +0 contains a representation for each exponent").
    // Set significand to 0 here and fall through to the existing significand-then-exponent flow,
    // which will hit the exp_char branch at line 380 and read the exponent value into `exponent`.
    if (*next == exp_char || *next == capital_exp_char)
    {
        significand = 0;
    }

    // Next we get the significand
    std::size_t i = 0;
    std::size_t dot_position = 0;
    Integer extra_zeros = 0;
    Integer leading_zero_powers = 0;
    const auto char_validation_func = (fmt != chars_format::hex) ? is_integer_char : is_hex_char;
    const int base = (fmt != chars_format::hex) ? 10 : 16;

    while (next != last && char_validation_func(*next) && i < significand_buffer_size)
    {
        all_zeros = false;
        significand_buffer[i] = *next;
        ++next;
        ++i;
    }

    bool fractional = false;
    if (next == last)
    {
        // if fmt is chars_format::scientific the e is required
        if (fmt == chars_format::scientific || fmt == chars_format::cohort_preserving_scientific)
        {
            return {first, std::errc::invalid_argument};
        }

        exponent = 0;
        std::size_t offset = i;

        const from_chars_result r {from_chars_dispatch(significand_buffer, significand_buffer + offset, significand, base)};
        switch (r.ec)
        {
            case std::errc::invalid_argument:
                return {first, std::errc::invalid_argument};
            case std::errc::result_out_of_range:
                return {next, std::errc::result_out_of_range};
            default:
                return {next, std::errc()};
        }
    }
    else if (*next == '.')
    {
        ++next;
        fractional = true;
        dot_position = i;

        // Process the fractional part if we have it
        //
        // if fmt is chars_format::scientific the e is required
        // if fmt is chars_format::fixed and not scientific the e is disallowed
        // if fmt is chars_format::general (which is scientific and fixed) the e is optional

        // If we have the value 0.00001 we can continue to chop zeros and adjust the exponent
        // so that we get the useful parts of the fraction
        if (all_zeros)
        {
            while (next != last && *next == '0')
            {
                ++next;
                --leading_zero_powers;
            }

            if (next == last)
            {
                // Inputs like "0.0" or "0.000" are mathematically zero, but the trailing
                // fractional zeros define the cohort (IEEE 754-2008 3.5.1). Propagate
                // leading_zero_powers as the resulting exponent so the cohort is preserved.
                significand = 0;
                exponent = static_cast<Integer>(leading_zero_powers);
                return {last, std::errc()};
            }
        }

        while (next != last && char_validation_func(*next) && i < significand_buffer_size)
        {
            significand_buffer[i] = *next;
            ++next;
            ++i;
        }
    }

    if (i == significand_buffer_size)
    {
        // We can not process any more significant figures into the significand so skip to the end
        // or the exponent part and capture the additional orders of magnitude for the exponent
        bool found_dot = false;
        while (next != last && (char_validation_func(*next) || *next == '.'))
        {
            ++next;
            if (!fractional && !found_dot)
            {
                ++extra_zeros;
            }
            if (next != last && *next == '.')
            {
                found_dot = true;
            }
        }
    }

    if (next == last || is_delimiter(*next, fmt))
    {
        if (fmt == chars_format::scientific || fmt == chars_format::cohort_preserving_scientific)
        {
            return {first, std::errc::invalid_argument};
        }

        if (dot_position != 0 || fractional)
        {
            exponent = static_cast<Integer>(dot_position) - static_cast<Integer>(i) + extra_zeros + leading_zero_powers;
        }
        else
        {
            exponent = extra_zeros + leading_zero_powers;
        }
        std::size_t offset = i;

        const from_chars_result r {from_chars_dispatch(significand_buffer, significand_buffer + offset, significand, base)};
        switch (r.ec)
        {
            case std::errc::result_out_of_range:
                return {next, std::errc::result_out_of_range};
            case std::errc::invalid_argument:
                return {first, std::errc::invalid_argument};
            default:
                return {next, std::errc()};
        }
    }
    else if (*next == exp_char || *next == capital_exp_char)
    {
        // Would be a number without a significand e.g. e+03
        if (next == first || fmt == chars_format::fixed)
        {
            return {next, std::errc::invalid_argument};
        }

        ++next;

        exponent = static_cast<Integer>(i - 1);
        std::size_t offset = i;
        bool round = false;
        // If more digits are present than representable in the significand of the target type
        // we set the maximum
        if (offset > significand_buffer_size)
        {
            offset = significand_buffer_size - 1;
            i = significand_buffer_size;
            if (significand_buffer[offset] >= '5')
            {
                round = true;
            }
        }

        // If the significand is 0 from chars will return std::errc::invalid_argument because there is nothing in the buffer,
        // but it is a valid value. We need to continue parsing to get the correct value of ptr even
        // though we know we could bail now.
        //
        // See GitHub issue #29: https://github.com/cppalliance/charconv/issues/29
        if (offset != 0)
        {
            from_chars_result r = from_chars_dispatch(significand_buffer, significand_buffer + offset, significand, base);
            switch (r.ec)
            {
                case std::errc::invalid_argument:
                    return {first, std::errc::invalid_argument};
                case std::errc::result_out_of_range:
                    return {next, std::errc::result_out_of_range};
                default:
                    break;
            }

            if (round)
            {
                ++significand;
            }
        }
    }
    else
    {
        return {first, std::errc::invalid_argument}; // LCOV_EXCL_LINE
    }

    // Finally we get the exponent
    constexpr std::size_t exponent_buffer_size = 6; // Float128 min exp is −16382
    char exponent_buffer[exponent_buffer_size] {};
    const auto significand_digits = i;
    i = 0;

    // Get the sign first
    if (next != last && *next == '-')
    {
        exponent_buffer[i] = *next;
        ++next;
        ++i;
    }
    else if (next != last && *next == '+')
    {
        ++next;
    }

    // Next strip any leading zeros
    while (next != last && *next == '0')
    {
        ++next;
    }

    // Process the significant values
    while (next != last && is_integer_char(*next) && i < exponent_buffer_size)
    {
        exponent_buffer[i] = *next;
        ++next;
        ++i;
    }

    // If the exponent can't fit in the buffer the number is not representable
    if (next != last && i == exponent_buffer_size)
    {
        return {next, std::errc::result_out_of_range};
    }

    // If the exponent was e+00 or e-00
    if (i == 0 || (i == 1 && exponent_buffer[0] == '-'))
    {
        if (fractional)
        {
            exponent = static_cast<Integer>(dot_position - significand_digits);
        }
        else
        {
            exponent = extra_zeros;
        }

        return {next, std::errc()};
    }

    const auto r = from_chars(exponent_buffer, exponent_buffer + i, exponent);

    BOOST_DECIMAL_ASSERT(r.ec == std::errc());

    exponent += leading_zero_powers;

    if (fractional)
    {
        // Need to take the offset from 1.xxx because compute_floatXXX assumes the significand is an integer
        // so the exponent is off by the number of digits in the significand - 1
        exponent -= static_cast<Integer>(significand_digits - dot_position);
    }
    else
    {
        exponent += extra_zeros;
    }

    return {next, r.ec};
}
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_PARSER_HPP
