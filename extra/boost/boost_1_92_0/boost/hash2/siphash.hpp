#ifndef BOOST_HASH2_SIPHASH_HPP_INCLUDED
#define BOOST_HASH2_SIPHASH_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// SipHash, https://131002.net/siphash/

#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/hash2/detail/config.hpp>
#include <boost/assert.hpp>
#include <cstdint>
#include <cstring>
#include <cstddef>

namespace boost
{
namespace hash2
{

class siphash_64
{
private:

    std::uint64_t v0 = 0x736f6d6570736575ULL;
    std::uint64_t v1 = 0x646f72616e646f6dULL;
    std::uint64_t v2 = 0x6c7967656e657261ULL;
    std::uint64_t v3 = 0x7465646279746573ULL;

    unsigned char buffer_[ 8 ] = {};
    std::size_t m_ = 0; // == n_ % 8

    std::uint64_t n_ = 0;

private:

    BOOST_CXX14_CONSTEXPR void sipround()
    {
        v0 += v1;
        v1 = detail::rotl(v1, 13);
        v1 ^= v0;
        v0 = detail::rotl(v0, 32);
        v2 += v3;
        v3 = detail::rotl(v3, 16);
        v3 ^= v2;
        v0 += v3;
        v3 = detail::rotl(v3, 21);
        v3 ^= v0;
        v2 += v1;
        v1 = detail::rotl(v1, 17);
        v1 ^= v2;
        v2 = detail::rotl(v2, 32);
    }

    BOOST_CXX14_CONSTEXPR void update_( unsigned char const * p )
    {
        std::uint64_t m = detail::read64le( p );

        v3 ^= m;

        sipround();
        sipround();

        v0 ^= m;
    }

public:

    using result_type = std::uint64_t;

    siphash_64() = default;

    BOOST_CXX14_CONSTEXPR explicit siphash_64( std::uint64_t seed )
    {
        v0 ^= seed;
        v2 ^= seed;
    }

    BOOST_CXX14_CONSTEXPR siphash_64( unsigned char const * p, std::size_t n )
    {
        if( n == 16 )
        {
            std::uint64_t k0 = detail::read64le( p + 0 );
            std::uint64_t k1 = detail::read64le( p + 8 );

            v0 ^= k0;
            v1 ^= k1;
            v2 ^= k0;
            v3 ^= k1;
        }
        else if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    siphash_64( void const * p, std::size_t n ): siphash_64( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        BOOST_ASSERT( m_ == n_ % 8 );

        if( n == 0 ) return;

        n_ += n;

        if( m_ > 0 )
        {
            std::size_t k = 8 - m_;

            if( n < k )
            {
                k = n;
            }

            detail::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;
            m_ += k;

            if( m_ < 8 ) return;

            BOOST_ASSERT( m_ == 8 );

            update_( buffer_ );
            m_ = 0;

            // clear buffered plaintext
            detail::memset( buffer_, 0, 8 );
        }

        BOOST_ASSERT( m_ == 0 );

        while( n >= 8 )
        {
            update_( p );

            p += 8;
            n -= 8;
        }

        BOOST_ASSERT( n < 8 );

        if( n > 0 )
        {
            detail::memcpy( buffer_, p, n );
            m_ = n;
        }

        BOOST_ASSERT( m_ == n_ % 8 );
    }

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR std::uint64_t result()
    {
        BOOST_ASSERT( m_ == n_ % 8 );

        detail::memset( buffer_ + m_, 0, 8 - m_ );

        buffer_[ 7 ] = static_cast<unsigned char>( n_ & 0xFF );

        update_( buffer_ );

        v2 ^= 0xFF;

        sipround();
        sipround();
        sipround();
        sipround();

        n_ += 8 - m_;
        m_ = 0;

        // clear buffered plaintext
        detail::memset( buffer_, 0, 8 );

        return v0 ^ v1 ^ v2 ^ v3;
    }
};

class siphash_32
{
private:

    std::uint32_t v0 = 0;
    std::uint32_t v1 = 0;
    std::uint32_t v2 = 0x6c796765;
    std::uint32_t v3 = 0x74656462;

    unsigned char buffer_[ 4 ] = {};
    std::uint32_t m_ = 0; // == n_ % 4

    std::uint32_t n_ = 0;

private:

    BOOST_CXX14_CONSTEXPR void sipround()
    {
        v0 += v1;
        v1 = detail::rotl(v1, 5);
        v1 ^= v0;
        v0 = detail::rotl(v0, 16);
        v2 += v3;
        v3 = detail::rotl(v3, 8);
        v3 ^= v2;
        v0 += v3;
        v3 = detail::rotl(v3, 7);
        v3 ^= v0;
        v2 += v1;
        v1 = detail::rotl(v1, 13);
        v1 ^= v2;
        v2 = detail::rotl(v2, 16);
    }

    BOOST_CXX14_CONSTEXPR void update_( unsigned char const * p )
    {
        std::uint32_t m = detail::read32le( p );

        v3 ^= m;

        sipround();
        sipround();

        v0 ^= m;
    }

public:

    using result_type = std::uint32_t;

    siphash_32() = default;

    BOOST_CXX14_CONSTEXPR explicit siphash_32( std::uint64_t seed )
    {
        std::uint32_t k0 = static_cast<std::uint32_t>( seed );
        std::uint32_t k1 = static_cast<std::uint32_t>( seed >> 32 );

        v0 ^= k0;
        v1 ^= k1;
        v2 ^= k0;
        v3 ^= k1;
    }

    BOOST_CXX14_CONSTEXPR siphash_32( unsigned char const * p, std::size_t n )
    {
        if( n == 8 )
        {
            std::uint32_t k0 = detail::read32le( p + 0 );
            std::uint32_t k1 = detail::read32le( p + 4 );

            v0 ^= k0;
            v1 ^= k1;
            v2 ^= k0;
            v3 ^= k1;
        }
        else if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    siphash_32( void const * p, std::size_t n ): siphash_32( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        BOOST_ASSERT( m_ == n_ % 4 );

        if( n == 0 ) return;

        n_ += static_cast<std::uint32_t>( n );

        if( m_ > 0 )
        {
            std::uint32_t k = 4 - m_;

            if( n < k )
            {
                k = static_cast<std::uint32_t>( n );
            }

            detail::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;
            m_ += k;

            if( m_ < 4 ) return;

            BOOST_ASSERT( m_ == 4 );

            update_( buffer_ );
            m_ = 0;

            // clear buffered plaintext
            detail::memset( buffer_, 0, 4 );
        }

        BOOST_ASSERT( m_ == 0 );

        while( n >= 4 )
        {
            update_( p );

            p += 4;
            n -= 4;
        }

        BOOST_ASSERT( n < 4 );

        if( n > 0 )
        {
            detail::memcpy( buffer_, p, n );
            m_ = static_cast<std::uint32_t>( n );
        }

        BOOST_ASSERT( m_ == n_ % 4 );
    }

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR std::uint32_t result()
    {
        BOOST_ASSERT( m_ == n_ % 4 );

        detail::memset( buffer_ + m_, 0, 4 - m_ );

        buffer_[ 3 ] = static_cast<unsigned char>( n_ & 0xFF );

        update_( buffer_ );

        v2 ^= 0xFF;

        sipround();
        sipround();
        sipround();
        sipround();

        n_ += 4 - m_;
        m_ = 0;

        // clear buffered plaintext
        detail::memset( buffer_, 0, 4 );

        return v1 ^ v3;
    }
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_SIPHASH_HPP_INCLUDED
