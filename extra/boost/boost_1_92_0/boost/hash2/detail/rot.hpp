#ifndef BOOST_HASH2_DETAIL_ROT_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_ROT_HPP_INCLUDED

// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <cstdint>

namespace boost
{
namespace hash2
{
namespace detail
{

// k must not be 0
BOOST_FORCEINLINE constexpr std::uint32_t rotl( std::uint32_t v, int k ) noexcept
{
    return ( v << k ) | ( v >> ( 32 - k ) );
}

// k must not be 0
BOOST_FORCEINLINE constexpr std::uint64_t rotl( std::uint64_t v, int k ) noexcept
{
    return ( v << k ) | ( v >> ( 64 - k ) );
}

// k must not be 0
BOOST_FORCEINLINE constexpr std::uint32_t rotr( std::uint32_t v, int k ) noexcept
{
    return ( v >> k ) | ( v << ( 32 - k ) );
}

// k must not be 0
BOOST_FORCEINLINE constexpr std::uint64_t rotr( std::uint64_t v, int k ) noexcept
{
    return ( v >> k ) | ( v << ( 64 - k ) );
}

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_ROT_HPP_INCLUDED
