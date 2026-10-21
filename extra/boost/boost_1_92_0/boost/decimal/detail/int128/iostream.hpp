// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_IOSTREAM_HPP
#define BOOST_DECIMAL_DETAIL_INT128_IOSTREAM_HPP

#include <boost/decimal/detail/int128/int128.hpp>
#include <boost/decimal/detail/int128/detail/mini_from_chars.hpp>
#include <boost/decimal/detail/int128/detail/mini_to_chars.hpp>
#include <boost/decimal/detail/int128/detail/utilities.hpp>
#include <boost/decimal/detail/int128/detail/config.hpp>

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <type_traits>
#include <iostream>
#include <iomanip>
#include <cstring>

#endif

namespace boost {
namespace int128 {

namespace detail {

template <typename T>
struct streamable_overload
{
    static constexpr bool value = std::is_same<T, uint128_t>::value || std::is_same<T, int128_t>::value;
};

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR bool is_streamable_overload_v = streamable_overload<T>::value;

} // namespace detail

BOOST_DECIMAL_DETAIL_INT128_EXPORT template <typename charT, typename traits, typename LibIntegerType>
auto operator>>(std::basic_istream<charT, traits>& is, LibIntegerType& v)
    -> std::enable_if_t<detail::is_streamable_overload_v<LibIntegerType>, std::basic_istream<charT, traits>&>
{
    charT t_buffer[64] {};
    is >> std::ws >> std::setw(63) >> t_buffer;

    const auto t_buffer_len {std::char_traits<charT>::length(t_buffer)};

    char buffer[64] {};
    auto buffer_start {buffer};

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (!std::is_same<charT, char>::value)
    {
        auto first {buffer};
        auto t_first {t_buffer};
        const auto t_buffer_end {t_buffer + detail::strlen(t_buffer)};

        while (t_first != t_buffer_end)
        {
            *first++ = static_cast<char>(*t_first++);
        }
    }
    else
    {
        std::memcpy(buffer, t_buffer, sizeof(t_buffer));
    }

    const auto flags {is.flags()};
    int base {10};
    if (flags & std::ios_base::oct)
    {
        base = 8;
        if (*buffer_start == '0')
        {
            ++buffer_start;
        }
    }
    else if (flags & std::ios_base::hex)
    {
        base = 16;
        if (*buffer_start == '0')
        {
            buffer_start += 2;
        }
    }

    const auto r {detail::from_chars(buffer_start, buffer + detail::strlen(buffer), v, base)};

    // Put back unconsumed characters
    // If r is greater than 0 then an errno values has been hit
    const auto consumed {static_cast<std::size_t>(r > 0 ? 0 : -r)};
    BOOST_DECIMAL_DETAIL_INT128_ASSERT(t_buffer_len >= consumed);
    const auto return_chars {static_cast<std::size_t>(t_buffer_len - consumed)};

    for (std::size_t i {}; i < return_chars; ++i)
    {
        is.putback(t_buffer[t_buffer_len - i - 1]);
    }

    return is;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT template <typename charT, typename traits, typename LibIntegerType>
auto operator<<(std::basic_ostream<charT, traits>& os, const LibIntegerType& v)
    -> std::enable_if_t<detail::is_streamable_overload_v<LibIntegerType>, std::basic_ostream<charT, traits>&>
{
    char buffer[64U] {};

    const auto flags {os.flags()};
    int base {10};
    bool uppercase {false};
    if (flags & std::ios_base::oct)
    {
        base = 8;
    }
    else if (flags & std::ios_base::hex)
    {
        base = 16;
    }

    if (flags & std::ios_base::uppercase)
    {
        uppercase = true;
    }

    auto first {detail::mini_to_chars(buffer, v, base, uppercase)};

    if (flags & std::ios_base::showbase)
    {
        if (base == 8)
        {
            *--first = '0';
        }
        else if (base == 16)
        {
            *--first = uppercase ? 'X' : 'x';
            *--first = '0';
        }
    }

    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (!std::is_same<charT, char>::value)
    {
        charT t_buffer[64U] {};

        auto t_first {t_buffer};
        while (*first != '\0')
        {
            *t_first++ = static_cast<charT>(*first++);
        }

        os << t_buffer;
    }
    else
    {
        os << first;
    }

    return os;
}

} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_IOSTREAM_HPP
