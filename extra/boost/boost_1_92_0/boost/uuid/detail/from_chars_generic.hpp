#ifndef BOOST_UUID_DETAIL_FROM_CHARS_GENERIC_HPP_INCLUDED
#define BOOST_UUID_DETAIL_FROM_CHARS_GENERIC_HPP_INCLUDED

// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/detail/from_chars_result.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/config.hpp>
#include <cstddef>

namespace boost {
namespace uuids {
namespace detail {

// 0-9, A-F, a-f are consecutive in both ASCII and EBCDIC

constexpr char const* from_chars_digits( char const* ) noexcept
{
    return "09AFaf-{}";
}

constexpr wchar_t const* from_chars_digits( wchar_t const* ) noexcept
{
    return L"09AFaf-{}";
}

constexpr char16_t const* from_chars_digits( char16_t const* ) noexcept
{
    return u"09AFaf-{}";
}

constexpr char32_t const* from_chars_digits( char32_t const* ) noexcept
{
    return U"09AFaf-{}";
}

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

constexpr char8_t const* from_chars_digits( char8_t const* ) noexcept
{
    return u8"09AFaf-{}";
}

#endif

template<class Ch>
BOOST_CXX14_CONSTEXPR inline
unsigned char from_chars_digit_value( Ch ch ) noexcept
{
    constexpr Ch const* digits = detail::from_chars_digits( static_cast<Ch const*>( nullptr ) );

    if( ch >= digits[ 0 ] && ch <= digits[ 1 ] )
    {
        return static_cast<unsigned char>( ch - digits[ 0 ] );
    }

    if( ch >= digits[ 2 ] && ch <= digits[ 3 ] )
    {
        return static_cast<unsigned char>( ch - digits[ 2 ] + 10 );
    }

    if( ch >= digits[ 4 ] && ch <= digits[ 5 ] )
    {
        return static_cast<unsigned char>( ch - digits[ 4 ] + 10 );
    }

    return 255;
}

template<class Ch>
BOOST_CXX14_CONSTEXPR inline
bool from_chars_is_dash( Ch ch ) noexcept
{
    constexpr Ch const* digits = detail::from_chars_digits( static_cast<Ch const*>( nullptr ) );
    return ch == digits[ 6 ];
}

template<class Ch>
BOOST_CXX14_CONSTEXPR inline
bool from_chars_is_opening_brace( Ch ch ) noexcept
{
    constexpr Ch const* digits = detail::from_chars_digits( static_cast<Ch const*>( nullptr ) );
    return ch == digits[ 7 ];
}

template<class Ch>
BOOST_CXX14_CONSTEXPR inline
bool from_chars_is_closing_brace( Ch ch ) noexcept
{
    constexpr Ch const* digits = detail::from_chars_digits( static_cast<Ch const*>( nullptr ) );
    return ch == digits[ 8 ];
}

template<class Ch>
BOOST_CXX14_CONSTEXPR inline
from_chars_result<Ch> from_chars_generic( Ch const* first, Ch const* last, uuid& u ) noexcept
{
    u = {};

    for( std::size_t i = 0; i < 16; ++i )
    {
        if( first == last )
        {
            return { first, from_chars_error::unexpected_end_of_input };
        }

        unsigned char v1 = detail::from_chars_digit_value( *first );

        if( v1 == 255 )
        {
            return { first, from_chars_error::hex_digit_expected };
        }

        ++first;

        if( first == last )
        {
            return { first, from_chars_error::unexpected_end_of_input };
        }

        unsigned char v2 = detail::from_chars_digit_value( *first );

        if( v2 == 255 )
        {
            return { first, from_chars_error::hex_digit_expected };
        }

        ++first;

        u.data()[ i ] = static_cast<unsigned char>( ( v1 << 4 ) + v2 );

        if( i == 3 || i == 5 || i == 7 || i == 9 )
        {
            if( first == last )
            {
                return { first, from_chars_error::unexpected_end_of_input };
            }

            if( !detail::from_chars_is_dash( *first ) )
            {
                return { first, from_chars_error::dash_expected };
            }

            ++first;
        }
    }

    return { first, from_chars_error::none };
}

}}} //namespace boost::uuids::detail

#endif // BOOST_UUID_DETAIL_TO_CHARS_GENERIC_HPP_INCLUDED
