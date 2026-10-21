#ifndef BOOST_HASH2_DETAIL_MUL128_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_MUL128_HPP_INCLUDED

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

struct uint128
{
    std::uint64_t low;
    std::uint64_t high;
};

BOOST_CXX14_CONSTEXPR inline uint128 mul128_impl( std::uint64_t x, std::uint64_t y ) noexcept
{
    std::uint64_t lo_lo = ( x & 0xffffffff ) * ( y & 0xffffffff );
    std::uint64_t hi_lo = ( x >> 32 ) * ( y & 0xffffffff );
    std::uint64_t lo_hi = ( x & 0xffffffff ) * ( y >> 32 );
    std::uint64_t hi_hi = ( x >> 32 ) * ( y >> 32 );

    std::uint64_t cross = ( lo_lo >> 32 ) + ( hi_lo & 0xffffffff ) + lo_hi;
    std::uint64_t upper = ( hi_lo >> 32 ) + ( cross >> 32 ) + hi_hi;
    std::uint64_t lower = ( cross << 32 ) | ( lo_lo & 0xffffffff );

    uint128 r = { 0, 0 };
    r.low  = lower;
    r.high = upper;
    return r;
}

BOOST_CXX14_CONSTEXPR inline uint128 mul128( std::uint64_t x, std::uint64_t y ) noexcept
{
#if defined(BOOST_HAS_INT128)

    __uint128_t product = __uint128_t( x ) * __uint128_t( y );

    uint128 r = { 0, 0 };
    r.low  = static_cast<std::uint64_t>( product );
    r.high = static_cast<std::uint64_t>( product >> 64 );
    return r;

#elif ( defined(_M_X64) || defined(_M_IA64) ) && !defined(_M_ARM64EC)

    if( !detail::is_constant_evaluated() )
    {
        uint128 r = { 0, 0 };
        std::uint64_t high_product = 0;
        r.low = _umul128( x, y, &high_product );
        r.high = high_product;
        return r;
    }
    else
    {
        return mul128_impl( x, y );
    }

#elif defined(_M_ARM64) || defined(_M_ARM64EC)

BOOST_CXX14_CONSTEXPR inline uint128 mul128_impl( std::uint64_t x, std::uint64_t y ) noexcept
{
    if( !detail::is_constant_evaluated() )
    {
        uint128 r = { 0, 0 };
        r.low  = x * y;
        r.high = __umulh( x, y );
        return r;
    }
    else
    {
        return mul128_impl( x, y );
    }

#else

    return mul128_impl( x, y );

#endif
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_MUL128_HPP_INCLUDED
