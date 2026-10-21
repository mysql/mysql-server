// Copyright 2023 - 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_FMT_FORMAT_HPP
#define BOOST_DECIMAL_FMT_FORMAT_HPP

#if __has_include(<fmt/format.h>) && __has_include(<fmt/base.h>)

#define BOOST_DECIMAL_HAS_FMTLIB_SUPPORT

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <fmt/format.h>
#include <fmt/base.h>
#include <fmt/xchar.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/locale_conversion.hpp>
#include <boost/decimal/charconv.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE

#include <algorithm>
#include <format>
#include <iostream>
#include <string>
#include <tuple>
#include <cctype>

#endif // BOOST_DECIMAL_BUILD_MODULE

namespace boost {
namespace decimal {
namespace detail {
namespace fmt_detail {

enum class sign_option
{
    plus,
    minus,
    space
};

enum class align_option
{
    none,
    left,    // '<'
    right,   // '>'
    center   // '^'
};

template <typename ParseContext>
constexpr auto parse_impl(ParseContext &ctx)
{
    using CharType = typename ParseContext::char_type;

    auto sign_character = sign_option::minus;
    auto alignment = align_option::none;
    CharType fill_char = static_cast<CharType>(' ');
    int ctx_precision = -1;
    boost::decimal::chars_format fmt = boost::decimal::chars_format::general;
    bool is_upper = false;
    int width = 0;
    bool use_locale = false;
    auto it {ctx.begin()};

    if (it == nullptr)
    {
        return std::make_tuple(ctx_precision, fmt, is_upper, width, sign_character, use_locale, alignment, fill_char, it);
    }

    // Helper to check if a character is an alignment specifier
    auto is_align_char = [](CharType c) -> align_option {
        if (c == static_cast<CharType>('<')) { return align_option::left; }
        if (c == static_cast<CharType>('>')) { return align_option::right; }
        if (c == static_cast<CharType>('^')) { return align_option::center; }
        return align_option::none;
    };

    // Parse [[fill]align] - fill is any character followed by an alignment specifier
    // If we see an alignment specifier as the second character, the first is fill
    // If we see an alignment specifier as the first character, no fill specified
    if (it != ctx.end())
    {
        auto next_it = it;
        ++next_it;

        if (next_it != ctx.end())
        {
            auto next_align = is_align_char(*next_it);
            if (next_align != align_option::none)
            {
                // First char is fill, second is align
                fill_char = *it;
                alignment = next_align;
                it = next_it;
                ++it;
            }
            else
            {
                // Check if first char is alignment
                auto first_align = is_align_char(*it);
                if (first_align != align_option::none)
                {
                    alignment = first_align;
                    ++it;
                }
            }
        }
        else
        {
            // Only one character - check if it's alignment
            auto first_align = is_align_char(*it);
            if (first_align != align_option::none)
            {
                alignment = first_align;
                ++it;
            }
        }
    }

    // Check for a sign character
    if (it != ctx.end())
    {
        switch (*it)
        {
            case static_cast<CharType>('-'):
                sign_character = sign_option::minus;
                ++it;
                break;
            case static_cast<CharType>('+'):
                sign_character = sign_option::plus;
                ++it;
                break;
            case static_cast<CharType>(' '):
                sign_character = sign_option::space;
                ++it;
                break;
            default:
                break;
        }
    }

    // Check for width
    while (it != ctx.end() && *it >= static_cast<CharType>('0') && *it <= static_cast<CharType>('9'))
    {
        width = width * 10 + (*it - static_cast<CharType>('0'));
        ++it;
    }

    // If there is a . then we need to capture the precision argument
    if (it != ctx.end() && *it == static_cast<CharType>('.'))
    {
        ++it;
        ctx_precision = 0;
        while (it != ctx.end() && *it >= static_cast<CharType>('0') && *it <= static_cast<CharType>('9'))
        {
            ctx_precision = ctx_precision * 10 + (*it - static_cast<CharType>('0'));
            ++it;
        }
    }

    // Lastly we capture the format to include if it's upper case
    if (it != ctx.end() && *it != '}')
    {
        switch (*it)
        {
            case static_cast<CharType>('G'):
                is_upper = true;
                fmt = chars_format::general;
                break;
            case static_cast<CharType>('g'):
                fmt = chars_format::general;
                break;

            case static_cast<CharType>('F'):
                is_upper = true;
                fmt = chars_format::fixed;
                break;
            case static_cast<CharType>('f'):
                fmt = chars_format::fixed;
                break;

            case static_cast<CharType>('E'):
                is_upper = true;
                fmt = chars_format::scientific;
                break;
            case static_cast<CharType>('e'):
                fmt = chars_format::scientific;
                break;

            case static_cast<CharType>('X'):
                is_upper = true;
                fmt = chars_format::hex;
                break;
            case static_cast<CharType>('x'):
                fmt = chars_format::hex;
                break;

            case static_cast<CharType>('A'):
                if (ctx_precision != -1)
                {
                    BOOST_DECIMAL_THROW_EXCEPTION(std::logic_error("Cohort preservation is mutually exclusive with precision"));
                }

                is_upper = true;
                fmt = chars_format::cohort_preserving_scientific;
                break;
            case static_cast<CharType>('a'):
                if (ctx_precision != -1)
                {
                    BOOST_DECIMAL_THROW_EXCEPTION(std::logic_error("Cohort preservation is mutually exclusive with precision"));
                }

                fmt = chars_format::cohort_preserving_scientific;
                break;
            // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_THROW_EXCEPTION(std::logic_error("Invalid format specifier"));
            // LCOV_EXCL_STOP
        }
        ++it;
    }

    // Check for the locale character
    if (it != ctx.end() && *it == static_cast<CharType>('L'))
    {
        use_locale = true;
        ++it;
    }

    // Verify we're at the closing brace
    if (it != ctx.end() && *it != static_cast<CharType>('}'))
    {
        BOOST_DECIMAL_THROW_EXCEPTION(std::logic_error("Expected '}' in format string")); // LCOV_EXCL_LINE
    }

    return std::make_tuple(ctx_precision, fmt, is_upper, width, sign_character, use_locale, alignment, fill_char, it);
}

template <typename T, typename CharType = char>
struct formatter
{
    sign_option sign;
    align_option alignment;
    CharType fill_char;
    chars_format fmt;
    int width;
    int ctx_precision;
    bool is_upper;
    bool use_locale;

    constexpr formatter() : sign{sign_option::minus},
                            alignment{align_option::none},
                            fill_char{static_cast<CharType>(' ')},
                            fmt{chars_format::general},
                            width{0},
                            ctx_precision{6},
                            is_upper{false},
                            use_locale{false} {}

    template <typename ParseCharType>
    constexpr auto parse(fmt::parse_context<ParseCharType> &ctx)
    {
        const auto res {boost::decimal::detail::fmt_detail::parse_impl(ctx)};

        ctx_precision = std::get<0>(res);
        fmt = std::get<1>(res);
        is_upper = std::get<2>(res);
        width = std::get<3>(res);
        sign = std::get<4>(res);
        use_locale = std::get<5>(res);
        alignment = std::get<6>(res);
        fill_char = static_cast<CharType>(std::get<7>(res));

        return std::get<8>(res);
    }

    template <typename FormatContext>
    auto format(const T& v, FormatContext& ctx) const
    {
        std::array<char, 128> buffer {};
        auto buffer_front = buffer.data();
        bool has_sign {false};
        switch (sign)
        {
            case sign_option::minus:
                if (signbit(v))
                {
                    has_sign = true;
                }
                break;
            case sign_option::plus:
                if (!signbit(v))
                {
                    *buffer_front++ = '+';
                }
                has_sign = true;
                break;
            case sign_option::space:
                if (!signbit(v))
                {
                    *buffer_front++ = ' ';
                }
                has_sign = true;
                break;
            // LCOV_EXCL_START
            default:
                BOOST_DECIMAL_UNREACHABLE;
            // LCOV_EXCL_STOP
        }

        boost::decimal::to_chars_result r {};
        if (ctx_precision != -1)
        {
            r = boost::decimal::to_chars(buffer_front, buffer.data() + buffer.size(), v, fmt, ctx_precision);
        }
        else
        {
            r = boost::decimal::to_chars(buffer_front, buffer.data() + buffer.size(), v, fmt);
        }

        std::string s(buffer.data(), static_cast<std::size_t>(r.ptr - buffer.data()));

        if (is_upper)
        {
            #ifdef _MSC_VER
            #  pragma warning(push)
            #  pragma warning(disable : 4244)
            #endif

            std::transform(s.begin() + static_cast<std::size_t>(has_sign), s.end(), s.begin() + static_cast<std::size_t>(has_sign),
                           [](unsigned char c)
                           { return std::toupper(c); });

            #ifdef _MSC_VER
            #  pragma warning(pop)
            #endif
        }

        // Apply width with fill and alignment
        if (width > 0 && s.size() < static_cast<std::size_t>(width))
        {
            const auto padding_needed = static_cast<std::size_t>(width) - s.size();
            const auto fill_ch = static_cast<char>(fill_char);

            switch (alignment)
            {
                case align_option::left:
                    s.append(padding_needed, fill_ch);
                    break;
                case align_option::right:
                    s.insert(0, padding_needed, fill_ch);
                    break;
                case align_option::center:
                {
                    const auto left_pad = padding_needed / 2;
                    const auto right_pad = padding_needed - left_pad;
                    s.insert(0, left_pad, fill_ch);
                    s.append(right_pad, fill_ch);
                    break;
                }
                case align_option::none:
                default:
                    // Default behavior: right-align for numbers
                    s.insert(0, padding_needed, fill_ch);
                    break;
            }
        }

        if (use_locale)
        {
            // We need approximately 1/3 more space in order to insert the thousands separators,
            // but after we have done our processing we need to shrink the string back down
            const auto initial_length {s.length()};
            s.resize(s.length() * 4 / 3 + 1);
            const auto offset {static_cast<std::size_t>(convert_pointer_pair_to_local_locale(const_cast<char*>(s.data()), s.data() + s.length()))};
            s.resize(initial_length + offset);
        }

        #ifdef BOOST_DECIMAL_NO_CXX17_IF_CONSTEXPR

        return fmt::format_to(ctx.out(), "{}", s);

        #else

        using OutputCharType = typename FormatContext::char_type;

        if constexpr (std::is_same_v<OutputCharType, char>)
        {
            return fmt::format_to(ctx.out(), "{}", s);
        }
        else if constexpr (std::is_same_v<OutputCharType, wchar_t>)
        {
            std::wstring result;
            result.reserve(s.size());
            for (const char c : s)
            {
                result.push_back(static_cast<OutputCharType>(static_cast<unsigned char>(c)));
            }

            return fmt::format_to(ctx.out(), L"{}", result);
        }
        else
        {
            // For other character types (char16_t, char32_t, etc.)

            std::basic_string<OutputCharType> result;
            result.reserve(s.size());
            for (const char c : s)
            {
                result.push_back(static_cast<OutputCharType>(static_cast<unsigned char>(c)));
            }

            if constexpr (std::is_same_v<OutputCharType, char16_t>)
            {
                return fmt::format_to(ctx.out(), u"{}", result);
            }
            else if constexpr (std::is_same_v<OutputCharType, char32_t>)
            {
                return fmt::format_to(ctx.out(), U"{}", result);
            }
            #ifdef BOOST_DECIMAL_HAS_CHAR8_T
            else
            {
                static_assert(std::is_same_v<OutputCharType, char8_t>, "Unsupported wide character type");
                return fmt::format_to(ctx.out(), u8"{}", result);
            }
            #else
            else
            {
                static_assert(std::is_same_v<OutputCharType, char>, "Unsupported wide character type");
                return fmt::format_to(ctx.out(), u8"{}", result);
            }
            #endif
        }

        #endif // BOOST_DECIMAL_NO_CXX17_IF_CONSTEXPR
    }
};

} // namespace fmt_detail
} // namespace detail
} // namespace decimal
} // Namespace boost

namespace fmt {

#ifdef BOOST_DECIMAL_NO_CXX17_IF_CONSTEXPR

template <>
struct formatter<boost::decimal::decimal32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal32_t> {};

template <>
struct formatter<boost::decimal::decimal_fast32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast32_t> {};

template <>
struct formatter<boost::decimal::decimal64_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal64_t> {};

template <>
struct formatter<boost::decimal::decimal_fast64_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast64_t> {};

template <>
struct formatter<boost::decimal::decimal128_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal128_t> {};

template <>
struct formatter<boost::decimal::decimal_fast128_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast128_t> {};

#else

template <>
struct formatter<boost::decimal::decimal32_t, char>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal32_t, char> {};

template <>
struct formatter<boost::decimal::decimal32_t, wchar_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal32_t, wchar_t> {};

template <>
struct formatter<boost::decimal::decimal32_t, char16_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal32_t, char16_t> {};

template <>
struct formatter<boost::decimal::decimal32_t, char32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal32_t, char32_t> {};

template <>
struct formatter<boost::decimal::decimal_fast32_t, char>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast32_t, char> {};

template <>
struct formatter<boost::decimal::decimal_fast32_t, wchar_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast32_t, wchar_t> {};

template <>
struct formatter<boost::decimal::decimal_fast32_t, char16_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast32_t, char16_t> {};

template <>
struct formatter<boost::decimal::decimal_fast32_t, char32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast32_t, char32_t> {};

template <>
struct formatter<boost::decimal::decimal64_t, char>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal64_t, char> {};

template <>
struct formatter<boost::decimal::decimal64_t, wchar_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal64_t, wchar_t> {};

template <>
struct formatter<boost::decimal::decimal64_t, char16_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal64_t, char16_t> {};

template <>
struct formatter<boost::decimal::decimal64_t, char32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal64_t, char32_t> {};

template <>
struct formatter<boost::decimal::decimal_fast64_t, char>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast64_t, char> {};

template <>
struct formatter<boost::decimal::decimal_fast64_t, wchar_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast64_t, wchar_t> {};

template <>
struct formatter<boost::decimal::decimal_fast64_t, char16_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast64_t, char16_t> {};

template <>
struct formatter<boost::decimal::decimal_fast64_t, char32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast64_t, char32_t> {};

template <>
struct formatter<boost::decimal::decimal128_t, char>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal128_t, char> {};

template <>
struct formatter<boost::decimal::decimal128_t, wchar_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal128_t, wchar_t> {};

template <>
struct formatter<boost::decimal::decimal128_t, char16_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal128_t, char16_t> {};

template <>
struct formatter<boost::decimal::decimal128_t, char32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal128_t, char32_t> {};

template <>
struct formatter<boost::decimal::decimal_fast128_t, char>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast128_t, char> {};

template <>
struct formatter<boost::decimal::decimal_fast128_t, wchar_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast128_t, wchar_t> {};

template <>
struct formatter<boost::decimal::decimal_fast128_t, char16_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast128_t, char16_t> {};

template <>
struct formatter<boost::decimal::decimal_fast128_t, char32_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast128_t, char32_t> {};

#ifdef BOOST_DECIMAL_HAS_CHAR8_T

template <>
struct formatter<boost::decimal::decimal32_t, char8_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal32_t, char8_t> {};

template <>
struct formatter<boost::decimal::decimal_fast32_t, char8_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast32_t, char8_t> {};

template <>
struct formatter<boost::decimal::decimal64_t, char8_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal64_t, char8_t> {};

template <>
struct formatter<boost::decimal::decimal_fast64_t, char8_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast64_t, char8_t> {};

template <>
struct formatter<boost::decimal::decimal128_t, char8_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal128_t, char8_t> {};

template <>
struct formatter<boost::decimal::decimal_fast128_t, char8_t>
    : public boost::decimal::detail::fmt_detail::formatter<boost::decimal::decimal_fast128_t, char8_t> {};

#endif // BOOST_DECIMAL_HAS_CHAR8_T

#endif

} // namespace fmt

#endif // __has_include(<fmt/format.h>)

#endif // BOOST_DECIMAL_FMT_FORMAT_HPP
