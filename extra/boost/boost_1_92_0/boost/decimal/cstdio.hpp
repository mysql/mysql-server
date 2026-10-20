// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_CSTDIO_HPP
#define BOOST_DECIMAL_CSTDIO_HPP

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/locale_conversion.hpp>
#include <boost/decimal/detail/parser.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/charconv.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <memory>
#include <new>
#include <cctype>
#include <cstdio>
#endif

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

// H is the type modifier used for decimal32_t
// D is the type modifier used for deciaml64
// DD is the type modifier used for decimal128_t

namespace boost {
namespace decimal {

namespace detail {

enum class decimal_type : unsigned
{
    decimal32_t = 1 << 0,
    decimal64_t = 1 << 1,
    decimal128_t = 1 << 2
};

// Internal use only
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpadded"
#endif

struct parameters
{
    int precision;
    chars_format fmt;
    decimal_type return_type;
    bool upper_case;
};

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

inline auto parse_format(const char* format) -> parameters
{
    // If the format is unspecified or incorrect, we will use this as the default values
    parameters params {6, chars_format::general, decimal_type::decimal64_t, false};

    auto iter {format + 1};
    const auto last {format + std::strlen(format)};

    if (iter == last)
    {
        return params;
    }

    // Case where we have a precision argumet
    if (*iter == '.')
    {
        ++iter;
        if (iter == last)
        {
            return params;
        }

        params.precision = 0;
        while (iter != last && is_integer_char(*iter))
        {
            params.precision = params.precision * 10 + digit_from_char(*iter);
            ++iter;
        }

        if (iter == last)
        {
            return params;
        }
    }

    // Now get the type specifier
    if (*iter == 'H')
    {
        params.return_type = decimal_type::decimal32_t;
        ++iter;
    }
    else if (*iter == 'D')
    {
        ++iter;
        if (iter == last)
        {
            return params;
        }
        else if (*iter == 'D')
        {
            params.return_type = decimal_type::decimal128_t;
            ++iter;
        }
    }

    // And lastly the format
    if (iter == last)
    {
        return params;
    }

    switch (*iter)
    {
        case 'G':
            params.upper_case = true;
            break;
        case 'E':
            params.upper_case = true;
            params.fmt = chars_format::scientific;
            break;
        case 'e':
            params.fmt = chars_format::scientific;
            break;
        case 'f':
            params.fmt = chars_format::fixed;
            break;
        case 'A':
            params.upper_case = true;
            params.fmt = chars_format::hex;
            break;
        case 'a':
            params.fmt = chars_format::hex;
            break;
        default:
            // Invalid specifier
            return params;
    }

    return params;
}

inline void make_uppercase(char* first, const char* last) noexcept
{
    while (first != last)
    {
        if ((*first >= 'a' && *first <= 'f') || *first == 'p')
        {
            *first = static_cast<char>(std::toupper(static_cast<int>(*first)));
        }
        ++first;
    }
}

// Cast of return value avoids warning when sizeof(std::ptrdiff_t) > sizeof(int) e.g. when not in 32-bit
#if defined(__GNUC__) && defined(__i386__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

template <typename... T>
inline auto snprintf_impl(char* buffer, const std::size_t buf_size, const char* format, const T... values) noexcept
    #ifndef BOOST_DECIMAL_HAS_CONCEPTS
    -> std::enable_if_t<detail::conjunction_v<detail::is_decimal_floating_point<T>...>, int>
    #else
    -> int requires detail::conjunction_v<detail::is_decimal_floating_point<T>...>
    #endif
{
    if (buffer == nullptr || format == nullptr)
    {
        return -1;
    }

    std::size_t byte_count {};
    const std::initializer_list<std::common_type_t<T...>> values_list {values...};
    auto value_iter = values_list.begin();
    const char* iter {format};
    const char* buffer_begin {buffer};
    const char* buffer_end {buffer + buf_size};

    const auto format_size {std::strlen(format)};

    while (buffer < buffer_end && byte_count < format_size)
    {
        while (buffer < buffer_end && byte_count < format_size && *iter != '%')
        {
            *buffer++ = *iter++;
            ++byte_count;
        }

        if (byte_count == format_size || buffer == buffer_end)
        {
            break;
        }

        char params_buffer[10] {};
        std::size_t param_iter {};
        while (param_iter < 10U && byte_count < format_size && *iter != ' ' && *iter != '"')
        {
            params_buffer[param_iter] = *iter++;
            ++byte_count;
            ++param_iter;
        }

        const auto params = parse_format(params_buffer);
        to_chars_result r;
        switch (params.return_type)
        {
            // Subtract 1 from all cases to ensure there is room to insert the null terminator
            case detail::decimal_type::decimal32_t:
                r = to_chars(buffer, buffer + buf_size - byte_count, static_cast<decimal32_t>(*value_iter), params.fmt, params.precision);
                break;
            case detail::decimal_type::decimal64_t:
                r = to_chars(buffer, buffer + buf_size - byte_count, static_cast<decimal64_t>(*value_iter), params.fmt, params.precision);
                break;
            default:
                r = to_chars(buffer, buffer + buf_size - byte_count, static_cast<decimal128_t>(*value_iter), params.fmt, params.precision);
                break;
        }

        if (!r)
        {
            errno = static_cast<int>(r.ec);
            return -1;
        }

        // Adjust the capitalization and locale
        if (params.upper_case)
        {
            detail::make_uppercase(buffer, r.ptr);
        }
        *r.ptr = '\0';
        const auto offset {convert_pointer_pair_to_local_locale(buffer, buffer + buf_size - byte_count)};

        buffer = r.ptr + (offset == -1 ? 0 : offset);

        if (value_iter != values_list.end())
        {
            ++value_iter;
        }
    }

    *buffer = '\0';
    return static_cast<int>(buffer - buffer_begin);
}

#if defined(__GNUC__) && defined(__i386__)
#  pragma GCC diagnostic pop
#endif

} // namespace detail

template <typename... T>
inline auto snprintf(char* buffer, const std::size_t buf_size, const char* format, const T... values) noexcept
    #ifndef BOOST_DECIMAL_HAS_CONCEPTS
    -> std::enable_if_t<detail::conjunction_v<detail::is_decimal_floating_point<T>...>, int>
    #else
    -> int requires detail::conjunction_v<detail::is_decimal_floating_point<T>...>
    #endif
{
    return detail::snprintf_impl(buffer, buf_size, format, values...);
}

template <typename... T>
inline auto fprintf(std::FILE* buffer, const char* format, const T... values) noexcept
    #ifndef BOOST_DECIMAL_HAS_CONCEPTS
    -> std::enable_if_t<detail::conjunction_v<detail::is_decimal_floating_point<T>...>, int>
    #else
    -> int requires detail::conjunction_v<detail::is_decimal_floating_point<T>...>
    #endif
{
    if (format == nullptr)
    {
        return -1;
    }

    // Heuristics for how much extra space we need to write the values
    using common_t = std::common_type_t<T...>;
    const std::initializer_list<common_t> values_list {values...};
    const auto value_space {detail::max_string_length_v<common_t> * values_list.size()};

    const auto format_len{std::strlen(format)};
    int bytes {};
    char char_buffer[1024];

    if (format_len + value_space <= ((1024 * 2) / 3))
    {
        bytes = detail::snprintf_impl(char_buffer, sizeof(char_buffer), format, values...);
        if (bytes)
        {
            bytes += static_cast<int>(std::fwrite(char_buffer, sizeof(char), static_cast<std::size_t>(bytes), buffer));
        }
    }
    else
    {
        // Add 50% overage in case we need to do locale conversion
        std::unique_ptr<char[]> longer_char_buffer(new(std::nothrow) char[(3 * (format_len + value_space + 1)) / 2]);
        if (longer_char_buffer == nullptr)
        {
            errno = ENOMEM;
            return -1;
        }

        bytes = detail::snprintf_impl(longer_char_buffer.get(), format_len, format, values...);
        if (bytes)
        {
            bytes += static_cast<int>(std::fwrite(longer_char_buffer.get(), sizeof(char), static_cast<std::size_t>(bytes), buffer));
        }
    }

    return bytes;
}

template <typename... T>
inline auto printf(const char* format, const T... values) noexcept
    #ifndef BOOST_DECIMAL_HAS_CONCEPTS
    -> std::enable_if_t<detail::conjunction_v<detail::is_decimal_floating_point<T>...>, int>
    #else
    -> int requires detail::conjunction_v<detail::is_decimal_floating_point<T>...>
    #endif
{
    return fprintf(stdout, format, values...);
}

} // namespace decimal
} // namespace boost

#endif // #if !defined(BOOST_DECIMAL_DISABLE_CLIB)

#endif //BOOST_DECIMAL_CSTDIO_HPP
