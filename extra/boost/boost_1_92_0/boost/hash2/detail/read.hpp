#ifndef BOOST_HASH2_DETAIL_READ_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_READ_HPP_INCLUDED

// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/endian.hpp>
#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/config.hpp>
#include <cstdint>
#include <cstring>

namespace boost
{
namespace hash2
{
namespace detail
{

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t read32le( unsigned char const * p ) noexcept
{
    if( !detail::is_constant_evaluated() && endian::native == endian::little )
    {
        std::uint32_t v = 0;
        std::memcpy( &v, p, sizeof(v) );
        return v;
    }
    else
    {
        return
            static_cast<std::uint32_t>( p[0] ) |
            ( static_cast<std::uint32_t>( p[1] ) <<  8 ) |
            ( static_cast<std::uint32_t>( p[2] ) << 16 ) |
            ( static_cast<std::uint32_t>( p[3] ) << 24 );
    }
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint64_t read64le( unsigned char const * p ) noexcept
{
    if( !detail::is_constant_evaluated() && endian::native == endian::little )
    {
        std::uint64_t v = 0;
        std::memcpy( &v, p, sizeof(v) );
        return v;
    }
    else
    {
        return
            static_cast<std::uint64_t>( p[0] ) |
            ( static_cast<std::uint64_t>( p[1] ) <<  8 ) |
            ( static_cast<std::uint64_t>( p[2] ) << 16 ) |
            ( static_cast<std::uint64_t>( p[3] ) << 24 ) |
            ( static_cast<std::uint64_t>( p[4] ) << 32 ) |
            ( static_cast<std::uint64_t>( p[5] ) << 40 ) |
            ( static_cast<std::uint64_t>( p[6] ) << 48 ) |
            ( static_cast<std::uint64_t>( p[7] ) << 56 );
    }
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t read32be( unsigned char const * p ) noexcept
{
    return
        static_cast<std::uint32_t>( p[3] ) |
        ( static_cast<std::uint32_t>( p[2] ) <<  8 ) |
        ( static_cast<std::uint32_t>( p[1] ) << 16 ) |
        ( static_cast<std::uint32_t>( p[0] ) << 24 );
}

BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint64_t read64be( unsigned char const * p ) noexcept
{
    return
        static_cast<std::uint64_t>( p[7] ) |
        ( static_cast<std::uint64_t>( p[6] ) <<  8 ) |
        ( static_cast<std::uint64_t>( p[5] ) << 16 ) |
        ( static_cast<std::uint64_t>( p[4] ) << 24 ) |
        ( static_cast<std::uint64_t>( p[3] ) << 32 ) |
        ( static_cast<std::uint64_t>( p[2] ) << 40 ) |
        ( static_cast<std::uint64_t>( p[1] ) << 48 ) |
        ( static_cast<std::uint64_t>( p[0] ) << 56 );
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_READ_HPP_INCLUDED
