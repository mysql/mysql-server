#ifndef BOOST_UUID_UUID_IO_HPP_INCLUDED
#define BOOST_UUID_UUID_IO_HPP_INCLUDED

// Copyright 2009 Andy Tompkins
// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/detail/to_chars.hpp>
#include <boost/uuid/detail/from_chars.hpp>
#include <boost/uuid/detail/static_assert.hpp>
#include <boost/uuid/detail/uuid_from_string.hpp>
#include <boost/config.hpp>
#include <iosfwd>
#include <ios>
#include <iterator>
#include <string>
#include <cstddef>

namespace boost {
namespace uuids {

// to_chars

namespace detail
{

template<class Vt> struct output_value_type
{
    using type = char;
};

template<> struct output_value_type<wchar_t>
{
    using type = wchar_t;
};

template<> struct output_value_type<char16_t>
{
    using type = char16_t;
};

template<> struct output_value_type<char32_t>
{
    using type = char32_t;
};

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

template<> struct output_value_type<char8_t>
{
    using type = char8_t;
};

#endif

} // namespace detail

template<class OutputIterator,
    class Vt = typename std::iterator_traits<OutputIterator>::value_type
>
BOOST_CXX14_CONSTEXPR OutputIterator to_chars( uuid const& u, OutputIterator out )
{
    using Ch = typename detail::output_value_type<Vt>::type;

    alignas( 16 ) Ch tmp[ 36 ] = {};
    detail::to_chars( u, tmp );

    for( std::size_t i = 0; i < 36; ++i )
    {
        *out++ = tmp[ i ];
    }

    return out;
}

template<class Ch>
BOOST_CXX14_CONSTEXPR inline bool to_chars( uuid const& u, Ch* first, Ch* last ) noexcept
{
    if( last - first < 36 )
    {
        return false;
    }

    detail::to_chars( u, first );
    return true;
}

template<class Ch, std::size_t N>
BOOST_CXX14_CONSTEXPR inline Ch* to_chars( uuid const& u, Ch (&buffer)[ N ] ) noexcept
{
    BOOST_UUID_STATIC_ASSERT( N >= 37 );

    detail::to_chars( u, buffer + 0 );
    buffer[ 36 ] = 0;

    return buffer + 36;
}

// only provided for compatibility; deprecated
template<class Ch>
BOOST_DEPRECATED( "Use Ch[37] instead of Ch[36] to allow for the null terminator" )
BOOST_CXX14_CONSTEXPR inline Ch* to_chars( uuid const& u, Ch (&buffer)[ 36 ] ) noexcept
{
    detail::to_chars( u, buffer + 0 );
    return buffer + 36;
}

// operator<<

template<class Ch, class Traits>
std::basic_ostream<Ch, Traits>& operator<<( std::basic_ostream<Ch, Traits>& os, uuid const& u )
{
    alignas( 16 ) Ch tmp[ 37 ];
    to_chars( u, tmp );

    os << tmp;
    return os;
}

// operator>>

template<class Ch, class Traits>
std::basic_istream<Ch, Traits>& operator>>( std::basic_istream<Ch, Traits>& is, uuid& u )
{
    alignas( 16 ) Ch tmp[ 37 ] = {};

    is.width( 37 ); // required for pre-C++20

    if( is >> tmp )
    {
        if( !from_chars( tmp, tmp + 36, u ) )
        {
            is.setstate( std::ios_base::failbit );
        }
    }

    return is;
}

// to_string

inline std::string to_string( uuid const& u )
{
    std::string result( 36, char() );

    // string::data() returns const char* before C++17
    detail::to_chars( u, &result[0] );
    return result;
}

inline std::wstring to_wstring( uuid const& u )
{
    std::wstring result( 36, wchar_t() );

    detail::to_chars( u, &result[0] );
    return result;
}

}} //namespace boost::uuids

#endif // BOOST_UUID_UUID_IO_HPP_INCLUDED
