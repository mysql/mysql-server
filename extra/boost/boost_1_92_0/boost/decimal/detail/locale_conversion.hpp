// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_LOCALE_CONVERSION_HPP
#define BOOST_DECIMAL_DETAIL_LOCALE_CONVERSION_HPP

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <locale>
#include <clocale>
#include <cstring>
#endif

namespace boost {
namespace decimal {
namespace detail {

// GCC-9 issues an erroneous warning for char == -30 being outside of type limits
#if defined(__GNUC__) && __GNUC__ >= 9
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wtype-limits"
#elif defined(__clang__) && __clang_major__ == 12
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif

inline void convert_string_to_c_locale(char* buffer, const std::locale& loc) noexcept
{
    const std::numpunct<char>& np = std::use_facet<std::numpunct<char>>(loc);

    const auto locale_decimal_point {np.decimal_point()};
    auto locale_thousands_sep {np.thousands_sep()};
    if (locale_thousands_sep == -30)
    {
        locale_thousands_sep = ' ';
    }
    const bool has_grouping {!np.grouping().empty() && np.grouping()[0] > 0};

    // Remove thousands separator if it exists and grouping is enabled
    if (has_grouping && locale_thousands_sep != '\0')
    {
        // Find the decimal point first to know where the integer part ends
        const auto decimal_pos {std::strchr(buffer, static_cast<int>(locale_decimal_point))};
        const auto int_end {decimal_pos ? decimal_pos : (buffer + std::strlen(buffer))};

        // Find the start of the number to include skipping sign
        auto start {buffer};
        if (*start == '-' || *start == '+')
        {
            ++start;
        }

        // Only remove thousands separators from the integer part
        auto read {start};
        auto write {start};

        while (read < int_end)
        {
            const auto ch = *read;
            if (ch != locale_thousands_sep)
            {
                *write++ = *read;
            }
            ++read;
        }

        // Copy the rest of the string (decimal point and fractional part)
        while (*read != '\0')
        {
            *write++ = *read++;
        }
        *write = '\0';
    }

    if (locale_decimal_point != '.')
    {
        const auto p {std::strchr(buffer, static_cast<int>(locale_decimal_point))};
        if (p != nullptr)
        {
            *p = '.';
        }
    }
}

inline void convert_string_to_c_locale(char* buffer) noexcept
{
    convert_string_to_c_locale(buffer, std::locale());
}

// Cast of return value avoids warning when sizeof(std::ptrdiff_t) > sizeof(int) e.g. when not in 32-bit
#if defined(__GNUC__) && defined(__i386__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

inline int convert_pointer_pair_to_local_locale(char* first, const char* last, const std::locale& loc) noexcept
{
    const std::numpunct<char>& np = std::use_facet<std::numpunct<char>>(loc);

    const auto locale_decimal_point {np.decimal_point()};
    auto locale_thousands_sep {np.thousands_sep()};
    if (locale_thousands_sep == -30)
    {
        locale_thousands_sep = ' ';
    }
    const bool has_grouping {!np.grouping().empty() && np.grouping()[0] > 0};
    const int grouping_size {has_grouping ? np.grouping()[0] : 0};

    // Find the start of the number (skip sign if present)
    char* start = first;
    if (start < last && (*start == '-' || *start == '+'))
    {
        ++start;
    }

    // Find the actual end of the string
    auto string_end {start};
    while (string_end < last && *string_end != '\0')
    {
        ++string_end;
    }

    // Find decimal point position
    char* decimal_pos {nullptr};
    for (char* p = start; p < string_end; ++p)
    {
        if (*p == '.')
        {
            decimal_pos = p;
            *decimal_pos = locale_decimal_point;
            break;
        }
    }

    // Determine the end of the integer part
    const auto int_end {decimal_pos != nullptr ? decimal_pos : string_end};
    const auto int_digits {(int_end - start)};

    // Calculate how many separators we need
    int num_separators {};
    if (has_grouping && locale_thousands_sep != '\0' && int_digits > 0)
    {
        if (int_digits > grouping_size)
        {
            num_separators = static_cast<int>((int_digits - 1) / grouping_size);
        }
    }

    // If we need to add separators, shift content and insert them
    if (num_separators > 0)
    {
        const auto original_length {(string_end - first)};
        const auto new_length {original_length + num_separators};

        // Check if we have enough space in the buffer
        if (first + new_length >= last)
        {
            // Not enough space, return error indicator
            return -1;
        }

        // Shift everything after the integer part to make room
        // Work backwards to avoid overwriting
        auto old_pos {string_end};
        auto new_pos {first + new_length};

        // Copy from end (including null terminator) back to the end of integer part
        while (old_pos >= int_end)
        {
            *new_pos-- = *old_pos--;
        }

        // Now insert the integer digits with separators
        // Count digits from right to left (from decimal point backwards)
        old_pos = int_end - 1;
        int digits_from_right {1};

        while (old_pos >= start)
        {
            *new_pos-- = *old_pos--;

            // Insert separator after every grouping_size digits from the right
            // but not after the leftmost digit
            if (old_pos >= start && digits_from_right % grouping_size == 0)
            {
                *new_pos-- = locale_thousands_sep;
            }
            ++digits_from_right;
        }
    }

    return num_separators;
}

// Cast of return value avoids warning when sizeof(std::ptrdiff_t) > sizeof(int) e.g. when not in 32-bit
#if defined(__GNUC__) && defined(__i386__)
#  pragma GCC diagnostic pop
#endif

#if defined(__GNUC__) && __GNUC__ == 9
#  pragma GCC diagnostic pop
#elif defined(__clang__) && __clang_major__ == 12
#  pragma clang diagnostic pop
#endif

inline int convert_pointer_pair_to_local_locale(char* first, const char* last)
{
    const auto loc {std::locale()};
    return convert_pointer_pair_to_local_locale(first, last, loc);
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_LOCALE_CONVERSION_HPP
