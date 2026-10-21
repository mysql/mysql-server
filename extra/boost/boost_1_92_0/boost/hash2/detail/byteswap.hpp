#ifndef BOOST_HASH2_DETAIL_BYTESWAP_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_BYTESWAP_HPP_INCLUDED

// Copyright 2025 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/config.hpp>
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace boost
{
namespace hash2
{
namespace detail
{

BOOST_CXX14_CONSTEXPR inline std::uint32_t byteswap_impl( std::uint32_t x ) noexcept
{
    std::uint32_t step16 = x << 16 | x >> 16;
    return ( ( step16 << 8 ) & 0xff00ff00 ) | ( ( step16 >> 8 ) & 0x00ff00ff );
}

BOOST_CXX14_CONSTEXPR inline std::uint64_t byteswap_impl( std::uint64_t x ) noexcept
{
    std::uint64_t step32 = x << 32 | x >> 32;
    std::uint64_t step16 = ( step32 & 0x0000ffff0000ffffull ) << 16 | ( step32 & 0xffff0000ffff0000ull ) >> 16;
    return ( step16 & 0x00ff00ff00ff00ffull ) << 8 | ( step16 & 0xff00ff00ff00ff00ull ) >> 8;
}

BOOST_CXX14_CONSTEXPR inline std::uint32_t byteswap( std::uint32_t x ) noexcept
{
#if defined(__GNUC__) || defined(__clang__)

    return __builtin_bswap32( x );

#elif defined(_MSC_VER)

    if( !detail::is_constant_evaluated() )
    {
        return _byteswap_ulong( x );
    }
    else
    {
        return byteswap_impl( x );
    }

#else

    return byteswap_impl( x );

#endif
}

BOOST_CXX14_CONSTEXPR inline std::uint64_t byteswap( std::uint64_t x ) noexcept
{
#if defined(__GNUC__) || defined(__clang__)

    return __builtin_bswap64( x );

#elif defined(_MSC_VER)

    if( !detail::is_constant_evaluated() )
    {
        return _byteswap_uint64( x );
    }
    else
    {
        return byteswap_impl( x );
    }

#else

    return byteswap_impl( x );

#endif
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_BYTESWAP_HPP_INCLUDED
