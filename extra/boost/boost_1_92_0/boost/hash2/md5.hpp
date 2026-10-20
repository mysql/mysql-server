#ifndef BOOST_HASH2_MD5_HPP_INCLUDED
#define BOOST_HASH2_MD5_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// MD5 message digest algorithm, https://tools.ietf.org/html/rfc1321

#include <boost/hash2/digest.hpp>
#include <boost/hash2/hmac.hpp>
#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/hash2/detail/config.hpp>
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

class md5_128
{
private:

    std::uint32_t state_[ 4 ] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u };

    static constexpr int N = 64;

    unsigned char buffer_[ N ] = {};
    std::size_t m_ = 0; // == n_ % N

    std::uint64_t n_ = 0;

private:

    static BOOST_FORCEINLINE constexpr std::uint32_t F( std::uint32_t x, std::uint32_t y, std::uint32_t z )
    {
        return (x & y) | (~x & z);
    }

    static BOOST_FORCEINLINE constexpr std::uint32_t G( std::uint32_t x, std::uint32_t y, std::uint32_t z )
    {
        return (x & z) | (y & ~z);
    }

    static BOOST_FORCEINLINE constexpr std::uint32_t H( std::uint32_t x, std::uint32_t y, std::uint32_t z )
    {
        return x ^ y ^ z;
    }

    static BOOST_FORCEINLINE constexpr std::uint32_t I( std::uint32_t x, std::uint32_t y, std::uint32_t z )
    {
        return y ^ (x | ~z);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void FF( std::uint32_t & a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t x, int s, std::uint32_t ac )
    {
        a += F( b, c, d ) + x + ac;
        a = detail::rotl( a, s );
        a += b;
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void GG( std::uint32_t & a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t x, int s, std::uint32_t ac )
    {
        a += G( b, c, d ) + x + ac;
        a = detail::rotl( a, s );
        a += b;
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void HH( std::uint32_t & a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t x, int s, std::uint32_t ac )
    {
        a += H( b, c, d ) + x + ac;
        a = detail::rotl( a, s );
        a += b;
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void II( std::uint32_t & a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t x, int s, std::uint32_t ac )
    {
        a += I( b, c, d ) + x + ac;
        a = detail::rotl( a, s );
        a += b;
    }

    static constexpr int S11 = 7;
    static constexpr int S12 = 12;
    static constexpr int S13 = 17;
    static constexpr int S14 = 22;
    static constexpr int S21 = 5;
    static constexpr int S22 = 9;
    static constexpr int S23 = 14;
    static constexpr int S24 = 20;
    static constexpr int S31 = 4;
    static constexpr int S32 = 11;
    static constexpr int S33 = 16;
    static constexpr int S34 = 23;
    static constexpr int S41 = 6;
    static constexpr int S42 = 10;
    static constexpr int S43 = 15;
    static constexpr int S44 = 21;

    BOOST_CXX14_CONSTEXPR void transform( unsigned char const block[ 64 ] )
    {
        std::uint32_t a = state_[ 0 ];
        std::uint32_t b = state_[ 1 ];
        std::uint32_t c = state_[ 2 ];
        std::uint32_t d = state_[ 3 ];

        std::uint32_t x[ 16 ] = {};

        for( int i = 0; i < 16; ++i )
        {
            x[ i ] = detail::read32le( block + i * 4 );
        }

        FF( a, b, c, d, x[ 0], S11, 0xd76aa478 );
        FF( d, a, b, c, x[ 1], S12, 0xe8c7b756 );
        FF( c, d, a, b, x[ 2], S13, 0x242070db );
        FF( b, c, d, a, x[ 3], S14, 0xc1bdceee );
        FF( a, b, c, d, x[ 4], S11, 0xf57c0faf );
        FF( d, a, b, c, x[ 5], S12, 0x4787c62a );
        FF( c, d, a, b, x[ 6], S13, 0xa8304613 );
        FF( b, c, d, a, x[ 7], S14, 0xfd469501 );
        FF( a, b, c, d, x[ 8], S11, 0x698098d8 );
        FF( d, a, b, c, x[ 9], S12, 0x8b44f7af );
        FF( c, d, a, b, x[10], S13, 0xffff5bb1 );
        FF( b, c, d, a, x[11], S14, 0x895cd7be );
        FF( a, b, c, d, x[12], S11, 0x6b901122 );
        FF( d, a, b, c, x[13], S12, 0xfd987193 );
        FF( c, d, a, b, x[14], S13, 0xa679438e );
        FF( b, c, d, a, x[15], S14, 0x49b40821 );

        GG( a, b, c, d, x[ 1], S21, 0xf61e2562 );
        GG( d, a, b, c, x[ 6], S22, 0xc040b340 );
        GG( c, d, a, b, x[11], S23, 0x265e5a51 );
        GG( b, c, d, a, x[ 0], S24, 0xe9b6c7aa );
        GG( a, b, c, d, x[ 5], S21, 0xd62f105d );
        GG( d, a, b, c, x[10], S22,  0x2441453 );
        GG( c, d, a, b, x[15], S23, 0xd8a1e681 );
        GG( b, c, d, a, x[ 4], S24, 0xe7d3fbc8 );
        GG( a, b, c, d, x[ 9], S21, 0x21e1cde6 );
        GG( d, a, b, c, x[14], S22, 0xc33707d6 );
        GG( c, d, a, b, x[ 3], S23, 0xf4d50d87 );
        GG( b, c, d, a, x[ 8], S24, 0x455a14ed );
        GG( a, b, c, d, x[13], S21, 0xa9e3e905 );
        GG( d, a, b, c, x[ 2], S22, 0xfcefa3f8 );
        GG( c, d, a, b, x[ 7], S23, 0x676f02d9 );
        GG( b, c, d, a, x[12], S24, 0x8d2a4c8a );

        HH( a, b, c, d, x[ 5], S31, 0xfffa3942 );
        HH( d, a, b, c, x[ 8], S32, 0x8771f681 );
        HH( c, d, a, b, x[11], S33, 0x6d9d6122 );
        HH( b, c, d, a, x[14], S34, 0xfde5380c );
        HH( a, b, c, d, x[ 1], S31, 0xa4beea44 );
        HH( d, a, b, c, x[ 4], S32, 0x4bdecfa9 );
        HH( c, d, a, b, x[ 7], S33, 0xf6bb4b60 );
        HH( b, c, d, a, x[10], S34, 0xbebfbc70 );
        HH( a, b, c, d, x[13], S31, 0x289b7ec6 );
        HH( d, a, b, c, x[ 0], S32, 0xeaa127fa );
        HH( c, d, a, b, x[ 3], S33, 0xd4ef3085 );
        HH( b, c, d, a, x[ 6], S34,  0x4881d05 );
        HH( a, b, c, d, x[ 9], S31, 0xd9d4d039 );
        HH( d, a, b, c, x[12], S32, 0xe6db99e5 );
        HH( c, d, a, b, x[15], S33, 0x1fa27cf8 );
        HH( b, c, d, a, x[ 2], S34, 0xc4ac5665 );

        II( a, b, c, d, x[ 0], S41, 0xf4292244 );
        II( d, a, b, c, x[ 7], S42, 0x432aff97 );
        II( c, d, a, b, x[14], S43, 0xab9423a7 );
        II( b, c, d, a, x[ 5], S44, 0xfc93a039 );
        II( a, b, c, d, x[12], S41, 0x655b59c3 );
        II( d, a, b, c, x[ 3], S42, 0x8f0ccc92 );
        II( c, d, a, b, x[10], S43, 0xffeff47d );
        II( b, c, d, a, x[ 1], S44, 0x85845dd1 );
        II( a, b, c, d, x[ 8], S41, 0x6fa87e4f );
        II( d, a, b, c, x[15], S42, 0xfe2ce6e0 );
        II( c, d, a, b, x[ 6], S43, 0xa3014314 );
        II( b, c, d, a, x[13], S44, 0x4e0811a1 );
        II( a, b, c, d, x[ 4], S41, 0xf7537e82 );
        II( d, a, b, c, x[11], S42, 0xbd3af235 );
        II( c, d, a, b, x[ 2], S43, 0x2ad7d2bb );
        II( b, c, d, a, x[ 9], S44, 0xeb86d391 );

        state_[ 0 ] += a;
        state_[ 1 ] += b;
        state_[ 2 ] += c;
        state_[ 3 ] += d;
    }

public:

    using result_type = digest<16>;

    static constexpr std::size_t block_size = 64;

    md5_128() = default;

    BOOST_CXX14_CONSTEXPR explicit md5_128( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR md5_128( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    md5_128( void const * p, std::size_t n ): md5_128( static_cast<unsigned char const*>( p ), n )
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

        detail::write64le( bits, n_ * 8 );

        std::size_t k = m_ < 56? 56 - m_: 120 - m_;

        unsigned char padding[ 64 ] = { 0x80 };

        update( padding, k );

        update( bits, 8 );

        BOOST_ASSERT( m_ == 0 );

        result_type digest = {{}};

        for( int i = 0; i < 4; ++i )
        {
            detail::write32le( digest.data() + i * 4, state_[ i ] );
        }

        return digest;
    }
};

using hmac_md5_128 = hmac<md5_128>;

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_MD5_HPP_INCLUDED
