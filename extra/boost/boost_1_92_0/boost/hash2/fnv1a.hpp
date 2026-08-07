#ifndef BOOST_HASH2_FNV1A_HPP_INCLUDED
#define BOOST_HASH2_FNV1A_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// FNV-1a
//
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

#include <boost/hash2/detail/write.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <cstdint>
#include <cstddef>

namespace boost
{
namespace hash2
{

namespace detail
{

template<class T> struct fnv1a_const;

template<> struct fnv1a_const<std::uint32_t>
{
    static constexpr std::uint32_t basis = 0x811C9DC5ul;
    static constexpr std::uint32_t prime = 0x01000193ul;
};

template<> struct fnv1a_const<std::uint64_t>
{
    static constexpr std::uint64_t basis = 0xCBF29CE484222325ull;
    static constexpr std::uint64_t prime = 0x00000100000001B3ull;
};

template<class T> class fnv1a
{
private:

    T st_ = fnv1a_const<T>::basis;

public:

    typedef T result_type;

    constexpr fnv1a() = default;

    BOOST_CXX14_CONSTEXPR explicit fnv1a( std::uint64_t seed )
    {
        if( seed )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );
            update( tmp, 8 );
        }
    }

    BOOST_CXX14_CONSTEXPR fnv1a( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );

            unsigned char tmp[ 4 ] = {};
            detail::write32le( tmp, static_cast<std::uint32_t>( n ) );
            update( tmp, 4 );
        }
    }

    fnv1a( void const * p, std::size_t n ): fnv1a( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const * p, std::size_t n )
    {
        T h = st_;

        for( std::size_t i = 0; i < n; ++i )
        {
            h ^= static_cast<T>( p[i] );
            h *= fnv1a_const<T>::prime;
        }

        st_ = h;
    }

    void update( void const * pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR T result()
    {
        T r = st_;

        // advance as if by update( "\xFF", 1 ), to allow
        // multiple result() calls to generate a sequence
        // of distinct values

        st_ = ( st_ ^ 0xFF ) * fnv1a_const<T>::prime;

        return r;
    }
};

} // namespace detail

class fnv1a_32: public detail::fnv1a<std::uint32_t>
{
public:

    constexpr fnv1a_32() = default;

    BOOST_CXX14_CONSTEXPR explicit fnv1a_32( std::uint64_t seed ): detail::fnv1a<std::uint32_t>( seed )
    {
    }

    BOOST_CXX14_CONSTEXPR fnv1a_32( unsigned char const * p, std::size_t n ): detail::fnv1a<std::uint32_t>( p, n )
    {
    }

    fnv1a_32( void const * p, std::size_t n ): detail::fnv1a<std::uint32_t>( p, n )
    {
    }
};

class fnv1a_64: public detail::fnv1a<std::uint64_t>
{
public:

    constexpr fnv1a_64() = default;

    BOOST_CXX14_CONSTEXPR explicit fnv1a_64( std::uint64_t seed ): detail::fnv1a<std::uint64_t>( seed )
    {
    }

    BOOST_CXX14_CONSTEXPR fnv1a_64( unsigned char const * p, std::size_t n ): detail::fnv1a<std::uint64_t>( p, n )
    {
    }

    fnv1a_64( void const * p, std::size_t n ): detail::fnv1a<std::uint64_t>( p, n )
    {
    }
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_FNV1A_HPP_INCLUDED
