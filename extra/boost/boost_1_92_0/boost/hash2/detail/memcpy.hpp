#ifndef BOOST_HASH2_DETAIL_MEMCPY_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_MEMCPY_HPP_INCLUDED

// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/config.hpp>
#include <cstring>

namespace boost
{
namespace hash2
{
namespace detail
{

#if defined(BOOST_NO_CXX14_CONSTEXPR)

BOOST_FORCEINLINE void memcpy( unsigned char* d, unsigned char const* s, std::size_t n ) noexcept
{
    std::memcpy( d, s, n );
}

#else

constexpr void memcpy( unsigned char* d, unsigned char const* s, std::size_t n ) noexcept
{
    if( !detail::is_constant_evaluated() )
    {
        std::memcpy( d, s, n );
    }
    else
    {
        for( std::size_t i = 0; i < n; ++i )
        {
            d[ i ] = s[ i ];
        }
    }
}

#endif

} // namespace detail
} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_DETAIL_MEMCPY_HPP_INCLUDED
