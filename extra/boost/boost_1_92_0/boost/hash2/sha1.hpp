#ifndef BOOST_HASH2_SHA1_HPP_INCLUDED
#define BOOST_HASH2_SHA1_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// SHA1 message digest algorithm, https://tools.ietf.org/html/rfc3174

#include <boost/hash2/hmac.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <cstdint>
#include <array>
#include <cstring>
#include <cstddef>

namespace boost
{
namespace hash2
{

class sha1_160
{
private:

    std::uint32_t state_[ 5 ] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u };

    static constexpr int N = 64;

    unsigned char buffer_[ N ] = {};
    std::size_t m_ = 0; // == n_ % N

    std::uint64_t n_ = 0;

private:

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R1( std::uint32_t a, std::uint32_t & b, std::uint32_t c, std::uint32_t d, std::uint32_t & e, std::uint32_t w[], unsigned char const block[ 64 ], int i )
    {
        w[ i ] = detail::read32be( block + i * 4 );

        std::uint32_t f = (b & c) | (~b & d);

        e += detail::rotl( a, 5 ) + f + 0x5A827999 + w[ i ];
        b = detail::rotl( b, 30 );
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t W( std::uint32_t w[], int i )
    {
        return w[ i ] = detail::rotl( w[ i - 3 ] ^ w[ i - 8 ] ^ w[ i - 14 ] ^ w[ i - 16 ], 1 );
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R2( std::uint32_t a, std::uint32_t & b, std::uint32_t c, std::uint32_t d, std::uint32_t & e, std::uint32_t w[], int i )
    {
        std::uint32_t f = (b & c) | (~b & d);

        e += detail::rotl( a, 5 ) + f + 0x5A827999 + W( w, i );
        b = detail::rotl( b, 30 );
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R3( std::uint32_t a, std::uint32_t & b, std::uint32_t c, std::uint32_t d, std::uint32_t & e, std::uint32_t w[], int i )
    {
        std::uint32_t f = b ^ c ^ d;

        e += detail::rotl( a, 5 ) + f + 0x6ED9EBA1 + W( w, i );
        b = detail::rotl( b, 30 );
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R4( std::uint32_t a, std::uint32_t & b, std::uint32_t c, std::uint32_t d, std::uint32_t & e, std::uint32_t w[], int i )
    {
        std::uint32_t f = (b & c) | (b & d) | (c & d);

        e += detail::rotl( a, 5 ) + f + 0x8F1BBCDC + W( w, i );
        b = detail::rotl( b, 30 );
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R5( std::uint32_t a, std::uint32_t & b, std::uint32_t c, std::uint32_t d, std::uint32_t & e, std::uint32_t w[], int i )
    {
        std::uint32_t f = b ^ c ^ d;

        e += detail::rotl( a, 5 ) + f + 0xCA62C1D6 + W( w, i );
        b = detail::rotl( b, 30 );
    }

    BOOST_CXX14_CONSTEXPR void transform( unsigned char const block[ 64 ] )
    {
        std::uint32_t a = state_[ 0 ];
        std::uint32_t b = state_[ 1 ];
        std::uint32_t c = state_[ 2 ];
        std::uint32_t d = state_[ 3 ];
        std::uint32_t e = state_[ 4 ];

        std::uint32_t w[ 80 ] = {};

        R1( a, b, c, d, e, w, block,  0 );
        R1( e, a, b, c, d, w, block,  1 );
        R1( d, e, a, b, c, w, block,  2 );
        R1( c, d, e, a, b, w, block,  3 );
        R1( b, c, d, e, a, w, block,  4 );
        R1( a, b, c, d, e, w, block,  5 );
        R1( e, a, b, c, d, w, block,  6 );
        R1( d, e, a, b, c, w, block,  7 );
        R1( c, d, e, a, b, w, block,  8 );
        R1( b, c, d, e, a, w, block,  9 );
        R1( a, b, c, d, e, w, block, 10 );
        R1( e, a, b, c, d, w, block, 11 );
        R1( d, e, a, b, c, w, block, 12 );
        R1( c, d, e, a, b, w, block, 13 );
        R1( b, c, d, e, a, w, block, 14 );
        R1( a, b, c, d, e, w, block, 15 );

        R2( e, a, b, c, d, w, 16 );
        R2( d, e, a, b, c, w, 17 );
        R2( c, d, e, a, b, w, 18 );
        R2( b, c, d, e, a, w, 19 );

        R3( a, b, c, d, e, w, 20 );
        R3( e, a, b, c, d, w, 21 );
        R3( d, e, a, b, c, w, 22 );
        R3( c, d, e, a, b, w, 23 );
        R3( b, c, d, e, a, w, 24 );
        R3( a, b, c, d, e, w, 25 );
        R3( e, a, b, c, d, w, 26 );
        R3( d, e, a, b, c, w, 27 );
        R3( c, d, e, a, b, w, 28 );
        R3( b, c, d, e, a, w, 29 );
        R3( a, b, c, d, e, w, 30 );
        R3( e, a, b, c, d, w, 31 );
        R3( d, e, a, b, c, w, 32 );
        R3( c, d, e, a, b, w, 33 );
        R3( b, c, d, e, a, w, 34 );
        R3( a, b, c, d, e, w, 35 );
        R3( e, a, b, c, d, w, 36 );
        R3( d, e, a, b, c, w, 37 );
        R3( c, d, e, a, b, w, 38 );
        R3( b, c, d, e, a, w, 39 );

        R4( a, b, c, d, e, w, 40 );
        R4( e, a, b, c, d, w, 41 );
        R4( d, e, a, b, c, w, 42 );
        R4( c, d, e, a, b, w, 43 );
        R4( b, c, d, e, a, w, 44 );
        R4( a, b, c, d, e, w, 45 );
        R4( e, a, b, c, d, w, 46 );
        R4( d, e, a, b, c, w, 47 );
        R4( c, d, e, a, b, w, 48 );
        R4( b, c, d, e, a, w, 49 );
        R4( a, b, c, d, e, w, 50 );
        R4( e, a, b, c, d, w, 51 );
        R4( d, e, a, b, c, w, 52 );
        R4( c, d, e, a, b, w, 53 );
        R4( b, c, d, e, a, w, 54 );
        R4( a, b, c, d, e, w, 55 );
        R4( e, a, b, c, d, w, 56 );
        R4( d, e, a, b, c, w, 57 );
        R4( c, d, e, a, b, w, 58 );
        R4( b, c, d, e, a, w, 59 );

        R5( a, b, c, d, e, w, 60 );
        R5( e, a, b, c, d, w, 61 );
        R5( d, e, a, b, c, w, 62 );
        R5( c, d, e, a, b, w, 63 );
        R5( b, c, d, e, a, w, 64 );
        R5( a, b, c, d, e, w, 65 );
        R5( e, a, b, c, d, w, 66 );
        R5( d, e, a, b, c, w, 67 );
        R5( c, d, e, a, b, w, 68 );
        R5( b, c, d, e, a, w, 69 );
        R5( a, b, c, d, e, w, 70 );
        R5( e, a, b, c, d, w, 71 );
        R5( d, e, a, b, c, w, 72 );
        R5( c, d, e, a, b, w, 73 );
        R5( b, c, d, e, a, w, 74 );
        R5( a, b, c, d, e, w, 75 );
        R5( e, a, b, c, d, w, 76 );
        R5( d, e, a, b, c, w, 77 );
        R5( c, d, e, a, b, w, 78 );
        R5( b, c, d, e, a, w, 79 );

        state_[ 0 ] += a;
        state_[ 1 ] += b;
        state_[ 2 ] += c;
        state_[ 3 ] += d;
        state_[ 4 ] += e;
    }

public:

    typedef digest<20> result_type;

    static constexpr std::size_t block_size = 64;

    sha1_160() = default;

    explicit BOOST_CXX14_CONSTEXPR sha1_160( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha1_160( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    sha1_160( void const * p, std::size_t n ): sha1_160( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        BOOST_ASSERT( m_ == n_ % N );

        if( n == 0 ) return;

        n_ += n;

        if( m_ > 0 )
        {
            std::size_t k = N - m_;

            if( n < k )
            {
                k = n;
            }

            detail::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;
            m_ += k;

            if( m_ < N ) return;

            BOOST_ASSERT( m_ == N );

            transform( buffer_ );
            m_ = 0;

            detail::memset( buffer_, 0, N );
        }

        BOOST_ASSERT( m_ == 0 );

        while( n >= N )
        {
            transform( p );

            p += N;
            n -= N;
        }

        BOOST_ASSERT( n < N );

        if( n > 0 )
        {
            detail::memcpy( buffer_, p, n );
            m_ = n;
        }

        BOOST_ASSERT( m_ == n_ % N );
    }

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        BOOST_ASSERT( m_ == n_ % N );

        unsigned char bits[ 8 ] = {};

        detail::write64be( bits, n_ * 8 );

        std::size_t k = m_ < 56? 56 - m_: 120 - m_;

        unsigned char padding[ 64 ] = { 0x80 };

        update( padding, k );

        update( bits, 8 );

        BOOST_ASSERT( m_ == 0 );

        result_type digest;

        for( int i = 0; i < 5; ++i )
        {
            detail::write32be( digest.data() + i * 4, state_[ i ] );
        }

        return digest;
    }
};

using hmac_sha1_160 = hmac<sha1_160>;

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_SHA1_HPP_INCLUDED
