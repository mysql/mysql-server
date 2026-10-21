#ifndef BOOST_HASH2_DETAIL_WRITE_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_WRITE_HPP_INCLUDED

// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/endian.hpp>
#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/config.hpp>
#include <type_traits>
#include <cstdint>
#include <cstring>

namespace boost
{
namespace hash2
{
namespace detail
{

// little endian

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write16le( unsigned char* p, std::uint16_t v ) noexcept
{
    if( !detail::is_constant_evaluated() && endian::native == endian::little )
    {
        std::memcpy( p, &v, sizeof(v) );
    }
    else
    {
        p[0] = static_cast<unsigned char>( v & 0xFF );
        p[1] = static_cast<unsigned char>( ( v >> 8 ) & 0xFF );
    }
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write32le( unsigned char* p, std::uint32_t v ) noexcept
{
    if( !detail::is_constant_evaluated() && endian::native == endian::little )
    {
        std::memcpy( p, &v, sizeof(v) );
    }
    else
    {
        p[0] = static_cast<unsigned char>( v & 0xFF );
        p[1] = static_cast<unsigned char>( ( v >>  8 ) & 0xFF );
        p[2] = static_cast<unsigned char>( ( v >> 16 ) & 0xFF );
        p[3] = static_cast<unsigned char>( ( v >> 24 ) & 0xFF );
    }
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write64le( unsigned char* p, std::uint64_t v ) noexcept
{
    if( !detail::is_constant_evaluated() && endian::native == endian::little )
    {
        std::memcpy( p, &v, sizeof(v) );
    }
    else
    {
        p[0] = static_cast<unsigned char>( v & 0xFF );
        p[1] = static_cast<unsigned char>( ( v >>  8 ) & 0xFF );
        p[2] = static_cast<unsigned char>( ( v >> 16 ) & 0xFF );
        p[3] = static_cast<unsigned char>( ( v >> 24 ) & 0xFF );
        p[4] = static_cast<unsigned char>( ( v >> 32 ) & 0xFF );
        p[5] = static_cast<unsigned char>( ( v >> 40 ) & 0xFF );
        p[6] = static_cast<unsigned char>( ( v >> 48 ) & 0xFF );
        p[7] = static_cast<unsigned char>( ( v >> 56 ) & 0xFF );
    }
}

// big endian

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write16be( unsigned char* p, std::uint16_t v ) noexcept
{
    p[0] = static_cast<unsigned char>( ( v >> 8 ) & 0xFF );
    p[1] = static_cast<unsigned char>( v & 0xFF );
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write32be( unsigned char* p, std::uint32_t v ) noexcept
{
    p[0] = static_cast<unsigned char>( ( v >> 24 ) & 0xFF );
    p[1] = static_cast<unsigned char>( ( v >> 16 ) & 0xFF );
    p[2] = static_cast<unsigned char>( ( v >>  8 ) & 0xFF );
    p[3] = static_cast<unsigned char>( v & 0xFF );
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write64be( unsigned char* p, std::uint64_t v ) noexcept
{
    p[0] = static_cast<unsigned char>( ( v >> 56 ) & 0xFF );
    p[1] = static_cast<unsigned char>( ( v >> 48 ) & 0xFF );
    p[2] = static_cast<unsigned char>( ( v >> 40 ) & 0xFF );
    p[3] = static_cast<unsigned char>( ( v >> 32 ) & 0xFF );
    p[4] = static_cast<unsigned char>( ( v >> 24 ) & 0xFF );
    p[5] = static_cast<unsigned char>( ( v >> 16 ) & 0xFF );
    p[6] = static_cast<unsigned char>( ( v >>  8 ) & 0xFF );
    p[7] = static_cast<unsigned char>( v & 0xFF );
}

// any endian

// template<class T> BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void write( T v, endian e, unsigned char (&w)[ sizeof(T) ] ) noexcept;

template<class T>
BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR
typename std::enable_if<std::is_integral<T>::value && sizeof(T) == 1, void>::type
write( T v, endian /*e*/, unsigned char (&w)[ sizeof(T) ] ) noexcept
{
    w[ 0 ] = static_cast<unsigned char>( v );
}

template<class T>
BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR
typename std::enable_if<std::is_integral<T>::value && sizeof(T) == 2, void>::type
write( T v, endian e, unsigned char (&w)[ sizeof(T) ] ) noexcept
{
    if( e == endian::little )
    {
        write16le( w, static_cast<std::uint16_t>( v ) );
    }
    else
    {
        write16be( w, static_cast<std::uint16_t>( v ) );
    }
}

template<class T>
BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR
typename std::enable_if<std::is_integral<T>::value && sizeof(T) == 4, void>::type
write( T v, endian e, unsigned char (&w)[ sizeof(T) ] ) noexcept
{
    if( e == endian::little )
    {
        write32le( w, static_cast<std::uint32_t>( v ) );
    }
    else
    {
        write32be( w, static_cast<std::uint32_t>( v ) );
    }
}

template<class T>
BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR
typename std::enable_if<std::is_integral<T>::value && sizeof(T) == 8, void>::type
write( T v, endian e, unsigned char (&w)[ sizeof(T) ] ) noexcept
{
    if( e == endian::little )
    {
        write64le( w, static_cast<std::uint64_t>( v ) );
    }
    else
    {
        write64be( w, static_cast<std::uint64_t>( v ) );
    }
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_WRITE_HPP_INCLUDED
