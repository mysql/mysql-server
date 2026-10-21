#ifndef BOOST_UUID_DETAIL_TO_CHARS_GENERIC_HPP_INCLUDED
#define BOOST_UUID_DETAIL_TO_CHARS_GENERIC_HPP_INCLUDED

// Copyright 2009 Andy Tompkins
// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/config.hpp>

namespace boost {
namespace uuids {
namespace detail {

constexpr char const* to_chars_digits( char const* ) noexcept
{
    return "0123456789abcdef-";
}

constexpr wchar_t const* to_chars_digits( wchar_t const* ) noexcept
{
    return L"0123456789abcdef-";
}

constexpr char16_t const* to_chars_digits( char16_t const* ) noexcept
{
    return u"0123456789abcdef-";
}

constexpr char32_t const* to_chars_digits( char32_t const* ) noexcept
{
    return U"0123456789abcdef-";
}

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

constexpr char8_t const* to_chars_digits( char8_t const* ) noexcept
{
    return u8"0123456789abcdef-";
}

#endif

template<class Ch> BOOST_CXX14_CONSTEXPR inline Ch* to_chars_generic( uuid const& u, Ch* out ) noexcept
{
    constexpr Ch const* digits = to_chars_digits( static_cast<Ch const*>( nullptr ) );

    for( std::size_t i = 0; i < 16; ++i )
    {
        std::uint8_t ch = u.data()[ i ];

        *out++ = digits[ (ch >> 4) & 0x0F ];
        *out++ = digits[ ch & 0x0F ];

        if( i == 3 || i == 5 || i == 7 || i == 9 )
        {
            *out++ = digits[ 16 ];
        }
    }

    return out;
}

}}} //namespace boost::uuids::detail

#endif // BOOST_UUID_DETAIL_TO_CHARS_GENERIC_HPP_INCLUDED
