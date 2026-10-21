#ifndef BOOST_HASH2_RIPEMD_HPP_INCLUDED
#define BOOST_HASH2_RIPEMD_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov
// Copyright 2024 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// RIPEMD-160 message digest algorithm, https://www.esat.kuleuven.be/cosic/publications/article-317.pdf
//                                      https://homes.esat.kuleuven.be/~bosselae/ripemd/rmd160.c
//                                      https://homes.esat.kuleuven.be/~bosselae/ripemd/rmd160.h
// RIPEMD-128 message digest algorithm, https://homes.esat.kuleuven.be/~bosselae/ripemd/rmd128.txt
//                                      https://homes.esat.kuleuven.be/~bosselae/ripemd/rmd128.c
//                                      https://homes.esat.kuleuven.be/~bosselae/ripemd/rmd128.h

#include <boost/hash2/hmac.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <cstdint>
#include <cstring>
#include <cstddef>

namespace boost
{
namespace hash2
{

class ripemd_128
{
private:

    std::uint32_t state_[ 4 ] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u };

    static constexpr int N = 64;

    unsigned char buffer_[ N ] = {};
    std::size_t m_ = 0; // == n_ % N

    std::uint64_t n_ = 0;

private:

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F1( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return x ^ y ^ z; }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F2( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) | (~x & z); }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F3( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x | ~y) ^ z; }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F4( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & z) | (y & ~z); }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R1( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F1(b, c, d) + x;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R2( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F2(b, c, d) + x + 0x5a827999u;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R3( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F3(b, c, d) + x + 0x6ed9eba1u;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R4( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F4(b, c, d) + x + 0x8f1bbcdcu;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR1( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F4(b, c, d) + x + 0x50a28be6u;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR2( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F3(b, c, d) + x + 0x5c4dd124u;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR3( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F2(b, c, d) + x + 0x6d703ef3u;
        a = detail::rotl(a, s);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR4( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t x, std::uint32_t s )
    {
        a += F1(b, c, d) + x;
        a = detail::rotl(a, s);
    }

    BOOST_CXX14_CONSTEXPR void transform( unsigned char const block[ 64 ] )
    {
        std::uint32_t aa = state_[ 0 ];
        std::uint32_t bb = state_[ 1 ];
        std::uint32_t cc = state_[ 2 ];
        std::uint32_t dd = state_[ 3 ];

        std::uint32_t aaa = state_[ 0 ];
        std::uint32_t bbb = state_[ 1 ];
        std::uint32_t ccc = state_[ 2 ];
        std::uint32_t ddd = state_[ 3 ];

        std::uint32_t X[ 16 ] = {};

        for( int i = 0; i < 16; ++i )
        {
            X[ i ] = detail::read32le( block + i * 4 );
        }

        //  A   B   C   D
        R1(aa, bb, cc, dd, X[ 0], 11);
        R1(dd, aa, bb, cc, X[ 1], 14);
        R1(cc, dd, aa, bb, X[ 2], 15);
        R1(bb, cc, dd, aa, X[ 3], 12);
        R1(aa, bb, cc, dd, X[ 4],  5);
        R1(dd, aa, bb, cc, X[ 5],  8);
        R1(cc, dd, aa, bb, X[ 6],  7);
        R1(bb, cc, dd, aa, X[ 7],  9);
        R1(aa, bb, cc, dd, X[ 8], 11);
        R1(dd, aa, bb, cc, X[ 9], 13);
        R1(cc, dd, aa, bb, X[10], 14);
        R1(bb, cc, dd, aa, X[11], 15);
        R1(aa, bb, cc, dd, X[12],  6);
        R1(dd, aa, bb, cc, X[13],  7);
        R1(cc, dd, aa, bb, X[14],  9);
        R1(bb, cc, dd, aa, X[15],  8);

        RR1(aaa, bbb, ccc, ddd, X[ 5],  8);
        RR1(ddd, aaa, bbb, ccc, X[14],  9);
        RR1(ccc, ddd, aaa, bbb, X[ 7],  9);
        RR1(bbb, ccc, ddd, aaa, X[ 0], 11);
        RR1(aaa, bbb, ccc, ddd, X[ 9], 13);
        RR1(ddd, aaa, bbb, ccc, X[ 2], 15);
        RR1(ccc, ddd, aaa, bbb, X[11], 15);
        RR1(bbb, ccc, ddd, aaa, X[ 4],  5);
        RR1(aaa, bbb, ccc, ddd, X[13],  7);
        RR1(ddd, aaa, bbb, ccc, X[ 6],  7);
        RR1(ccc, ddd, aaa, bbb, X[15],  8);
        RR1(bbb, ccc, ddd, aaa, X[ 8], 11);
        RR1(aaa, bbb, ccc, ddd, X[ 1], 14);
        RR1(ddd, aaa, bbb, ccc, X[10], 14);
        RR1(ccc, ddd, aaa, bbb, X[ 3], 12);
        RR1(bbb, ccc, ddd, aaa, X[12],  6);

        R2(aa, bb, cc, dd, X[ 7],  7);
        R2(dd, aa, bb, cc, X[ 4],  6);
        R2(cc, dd, aa, bb, X[13],  8);
        R2(bb, cc, dd, aa, X[ 1], 13);
        R2(aa, bb, cc, dd, X[10], 11);
        R2(dd, aa, bb, cc, X[ 6],  9);
        R2(cc, dd, aa, bb, X[15],  7);
        R2(bb, cc, dd, aa, X[ 3], 15);
        R2(aa, bb, cc, dd, X[12],  7);
        R2(dd, aa, bb, cc, X[ 0], 12);
        R2(cc, dd, aa, bb, X[ 9], 15);
        R2(bb, cc, dd, aa, X[ 5],  9);
        R2(aa, bb, cc, dd, X[ 2], 11);
        R2(dd, aa, bb, cc, X[14],  7);
        R2(cc, dd, aa, bb, X[11], 13);
        R2(bb, cc, dd, aa, X[ 8], 12);

        RR2(aaa, bbb, ccc, ddd, X[ 6],  9);
        RR2(ddd, aaa, bbb, ccc, X[11], 13);
        RR2(ccc, ddd, aaa, bbb, X[ 3], 15);
        RR2(bbb, ccc, ddd, aaa, X[ 7],  7);
        RR2(aaa, bbb, ccc, ddd, X[ 0], 12);
        RR2(ddd, aaa, bbb, ccc, X[13],  8);
        RR2(ccc, ddd, aaa, bbb, X[ 5],  9);
        RR2(bbb, ccc, ddd, aaa, X[10], 11);
        RR2(aaa, bbb, ccc, ddd, X[14],  7);
        RR2(ddd, aaa, bbb, ccc, X[15],  7);
        RR2(ccc, ddd, aaa, bbb, X[ 8], 12);
        RR2(bbb, ccc, ddd, aaa, X[12],  7);
        RR2(aaa, bbb, ccc, ddd, X[ 4],  6);
        RR2(ddd, aaa, bbb, ccc, X[ 9], 15);
        RR2(ccc, ddd, aaa, bbb, X[ 1], 13);
        RR2(bbb, ccc, ddd, aaa, X[ 2], 11);

        R3(aa, bb, cc, dd, X[ 3], 11);
        R3(dd, aa, bb, cc, X[10], 13);
        R3(cc, dd, aa, bb, X[14],  6);
        R3(bb, cc, dd, aa, X[ 4],  7);
        R3(aa, bb, cc, dd, X[ 9], 14);
        R3(dd, aa, bb, cc, X[15],  9);
        R3(cc, dd, aa, bb, X[ 8], 13);
        R3(bb, cc, dd, aa, X[ 1], 15);
        R3(aa, bb, cc, dd, X[ 2], 14);
        R3(dd, aa, bb, cc, X[ 7],  8);
        R3(cc, dd, aa, bb, X[ 0], 13);
        R3(bb, cc, dd, aa, X[ 6],  6);
        R3(aa, bb, cc, dd, X[13],  5);
        R3(dd, aa, bb, cc, X[11], 12);
        R3(cc, dd, aa, bb, X[ 5],  7);
        R3(bb, cc, dd, aa, X[12],  5);

        RR3(aaa, bbb, ccc, ddd, X[15],  9);
        RR3(ddd, aaa, bbb, ccc, X[ 5],  7);
        RR3(ccc, ddd, aaa, bbb, X[ 1], 15);
        RR3(bbb, ccc, ddd, aaa, X[ 3], 11);
        RR3(aaa, bbb, ccc, ddd, X[ 7],  8);
        RR3(ddd, aaa, bbb, ccc, X[14],  6);
        RR3(ccc, ddd, aaa, bbb, X[ 6],  6);
        RR3(bbb, ccc, ddd, aaa, X[ 9], 14);
        RR3(aaa, bbb, ccc, ddd, X[11], 12);
        RR3(ddd, aaa, bbb, ccc, X[ 8], 13);
        RR3(ccc, ddd, aaa, bbb, X[12],  5);
        RR3(bbb, ccc, ddd, aaa, X[ 2], 14);
        RR3(aaa, bbb, ccc, ddd, X[10], 13);
        RR3(ddd, aaa, bbb, ccc, X[ 0], 13);
        RR3(ccc, ddd, aaa, bbb, X[ 4],  7);
        RR3(bbb, ccc, ddd, aaa, X[13],  5);

        R4(aa, bb, cc, dd, X[ 1], 11);
        R4(dd, aa, bb, cc, X[ 9], 12);
        R4(cc, dd, aa, bb, X[11], 14);
        R4(bb, cc, dd, aa, X[10], 15);
        R4(aa, bb, cc, dd, X[ 0], 14);
        R4(dd, aa, bb, cc, X[ 8], 15);
        R4(cc, dd, aa, bb, X[12],  9);
        R4(bb, cc, dd, aa, X[ 4],  8);
        R4(aa, bb, cc, dd, X[13],  9);
        R4(dd, aa, bb, cc, X[ 3], 14);
        R4(cc, dd, aa, bb, X[ 7],  5);
        R4(bb, cc, dd, aa, X[15],  6);
        R4(aa, bb, cc, dd, X[14],  8);
        R4(dd, aa, bb, cc, X[ 5],  6);
        R4(cc, dd, aa, bb, X[ 6],  5);
        R4(bb, cc, dd, aa, X[ 2], 12);

        RR4(aaa, bbb, ccc, ddd, X[ 8], 15);
        RR4(ddd, aaa, bbb, ccc, X[ 6],  5);
        RR4(ccc, ddd, aaa, bbb, X[ 4],  8);
        RR4(bbb, ccc, ddd, aaa, X[ 1], 11);
        RR4(aaa, bbb, ccc, ddd, X[ 3], 14);
        RR4(ddd, aaa, bbb, ccc, X[11], 14);
        RR4(ccc, ddd, aaa, bbb, X[15],  6);
        RR4(bbb, ccc, ddd, aaa, X[ 0], 14);
        RR4(aaa, bbb, ccc, ddd, X[ 5],  6);
        RR4(ddd, aaa, bbb, ccc, X[12],  9);
        RR4(ccc, ddd, aaa, bbb, X[ 2], 12);
        RR4(bbb, ccc, ddd, aaa, X[13],  9);
        RR4(aaa, bbb, ccc, ddd, X[ 9], 12);
        RR4(ddd, aaa, bbb, ccc, X[ 7],  5);
        RR4(ccc, ddd, aaa, bbb, X[10], 15);
        RR4(bbb, ccc, ddd, aaa, X[14],  8);

        ddd += cc + state_[ 1 ];
        state_[ 1 ] = state_[ 2 ] + dd + aaa;
        state_[ 2 ] = state_[ 3 ] + aa + bbb;
        state_[ 3 ] = state_[ 0 ] + bb + ccc;
        state_[ 0 ] = ddd;
    }

public:

    typedef digest<16> result_type;

    static constexpr std::size_t block_size = 64;

    ripemd_128() = default;

    explicit BOOST_CXX14_CONSTEXPR ripemd_128( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR ripemd_128( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    ripemd_128( void const * p, std::size_t n ): ripemd_128( static_cast<unsigned char const*>( p ), n )
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

    void update( void const * pv, std::size_t n )
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

        result_type digest;

        for( int i = 0; i < 4; ++i )
        {
            detail::write32le( &digest[ i * 4 ], state_[ i ] );
        }

        return digest;
    }
};

class ripemd_160
{
private:

    std::uint32_t state_[ 5 ] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u };

    static constexpr int N = 64;

    unsigned char buffer_[ N ] = {};
    std::size_t m_ = 0; // == n_ % N

    std::uint64_t n_ = 0;

private:

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F1( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return x ^ y ^ z; }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F2( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) | (~x & z); }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F3( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x | ~y) ^ z; }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F4( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & z) | (y & ~z); }
    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR std::uint32_t F5( std::uint32_t x, std::uint32_t y, std::uint32_t z) { return x ^ (y | ~z); }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R1( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F1(b, c, d) + x;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R2( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F2(b, c, d) + x + 0x5a827999u;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R3( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F3(b, c, d) + x + 0x6ed9eba1u;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R4( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F4(b, c, d) + x + 0x8f1bbcdcu;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void R5( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F5(b, c, d) + x + 0xa953fd4eu;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR1( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F5(b, c, d) + x + 0x50a28be6u;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR2( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F4(b, c, d) + x + 0x5c4dd124u;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR3( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F3(b, c, d) + x + 0x6d703ef3u;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR4( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F2(b, c, d) + x + 0x7a6d76e9u;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    static BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR void RR5( std::uint32_t & a, std::uint32_t b, std::uint32_t & c, std::uint32_t d, std::uint32_t e, std::uint32_t x, std::uint32_t s )
    {
        a += F1(b, c, d) + x;
        a = detail::rotl(a, s) + e;
        c = detail::rotl(c, 10);
    }

    BOOST_CXX14_CONSTEXPR void transform( unsigned char const block[ 64 ] )
    {
        std::uint32_t aa = state_[ 0 ];
        std::uint32_t bb = state_[ 1 ];
        std::uint32_t cc = state_[ 2 ];
        std::uint32_t dd = state_[ 3 ];
        std::uint32_t ee = state_[ 4 ];

        std::uint32_t aaa = state_[ 0 ];
        std::uint32_t bbb = state_[ 1 ];
        std::uint32_t ccc = state_[ 2 ];
        std::uint32_t ddd = state_[ 3 ];
        std::uint32_t eee = state_[ 4 ];

        std::uint32_t X[ 16 ] = {};

        for( int i = 0; i < 16; ++i )
        {
            X[ i ] = detail::read32le( block + i * 4 );
        }

        // each function mutates a and c inputs so that we can easily rotate the arguments when expanding the evaluation
        // of the core algorithm in the paper:
        // T = rotl(A + f(j, B, C, D) + X[i][r[j]] + K[j], s)
        // A = E
        // E = D
        // D = rotl(C, 10)
        // C = B
        // B = T
        // mutating A in-place as T permits us to use it as B in the next call and so on and so forth
        // mutating C in-place permits us to use it as D in later calls as well and so on and so forth

        //  A   B   C   D   E
        R1(aa, bb, cc, dd, ee, X[ 0], 11);
        R1(ee, aa, bb, cc, dd, X[ 1], 14);
        R1(dd, ee, aa, bb, cc, X[ 2], 15);
        R1(cc, dd, ee, aa, bb, X[ 3], 12);
        R1(bb, cc, dd, ee, aa, X[ 4],  5);
        R1(aa, bb, cc, dd, ee, X[ 5],  8);
        R1(ee, aa, bb, cc, dd, X[ 6],  7);
        R1(dd, ee, aa, bb, cc, X[ 7],  9);
        R1(cc, dd, ee, aa, bb, X[ 8], 11);
        R1(bb, cc, dd, ee, aa, X[ 9], 13);
        R1(aa, bb, cc, dd, ee, X[10], 14);
        R1(ee, aa, bb, cc, dd, X[11], 15);
        R1(dd, ee, aa, bb, cc, X[12],  6);
        R1(cc, dd, ee, aa, bb, X[13],  7);
        R1(bb, cc, dd, ee, aa, X[14],  9);
        R1(aa, bb, cc, dd, ee, X[15],  8);

        RR1(aaa, bbb, ccc, ddd, eee, X[ 5],  8);
        RR1(eee, aaa, bbb, ccc, ddd, X[14],  9);
        RR1(ddd, eee, aaa, bbb, ccc, X[ 7],  9);
        RR1(ccc, ddd, eee, aaa, bbb, X[ 0], 11);
        RR1(bbb, ccc, ddd, eee, aaa, X[ 9], 13);
        RR1(aaa, bbb, ccc, ddd, eee, X[ 2], 15);
        RR1(eee, aaa, bbb, ccc, ddd, X[11], 15);
        RR1(ddd, eee, aaa, bbb, ccc, X[ 4],  5);
        RR1(ccc, ddd, eee, aaa, bbb, X[13],  7);
        RR1(bbb, ccc, ddd, eee, aaa, X[ 6],  7);
        RR1(aaa, bbb, ccc, ddd, eee, X[15],  8);
        RR1(eee, aaa, bbb, ccc, ddd, X[ 8], 11);
        RR1(ddd, eee, aaa, bbb, ccc, X[ 1], 14);
        RR1(ccc, ddd, eee, aaa, bbb, X[10], 14);
        RR1(bbb, ccc, ddd, eee, aaa, X[ 3], 12);
        RR1(aaa, bbb, ccc, ddd, eee, X[12],  6);

        R2(ee, aa, bb, cc, dd, X[ 7],  7);
        R2(dd, ee, aa, bb, cc, X[ 4],  6);
        R2(cc, dd, ee, aa, bb, X[13],  8);
        R2(bb, cc, dd, ee, aa, X[ 1], 13);
        R2(aa, bb, cc, dd, ee, X[10], 11);
        R2(ee, aa, bb, cc, dd, X[ 6],  9);
        R2(dd, ee, aa, bb, cc, X[15],  7);
        R2(cc, dd, ee, aa, bb, X[ 3], 15);
        R2(bb, cc, dd, ee, aa, X[12],  7);
        R2(aa, bb, cc, dd, ee, X[ 0], 12);
        R2(ee, aa, bb, cc, dd, X[ 9], 15);
        R2(dd, ee, aa, bb, cc, X[ 5],  9);
        R2(cc, dd, ee, aa, bb, X[ 2], 11);
        R2(bb, cc, dd, ee, aa, X[14],  7);
        R2(aa, bb, cc, dd, ee, X[11], 13);
        R2(ee, aa, bb, cc, dd, X[ 8], 12);

        RR2(eee, aaa, bbb, ccc, ddd, X[ 6],  9);
        RR2(ddd, eee, aaa, bbb, ccc, X[11], 13);
        RR2(ccc, ddd, eee, aaa, bbb, X[ 3], 15);
        RR2(bbb, ccc, ddd, eee, aaa, X[ 7],  7);
        RR2(aaa, bbb, ccc, ddd, eee, X[ 0], 12);
        RR2(eee, aaa, bbb, ccc, ddd, X[13],  8);
        RR2(ddd, eee, aaa, bbb, ccc, X[ 5],  9);
        RR2(ccc, ddd, eee, aaa, bbb, X[10], 11);
        RR2(bbb, ccc, ddd, eee, aaa, X[14],  7);
        RR2(aaa, bbb, ccc, ddd, eee, X[15],  7);
        RR2(eee, aaa, bbb, ccc, ddd, X[ 8], 12);
        RR2(ddd, eee, aaa, bbb, ccc, X[12],  7);
        RR2(ccc, ddd, eee, aaa, bbb, X[ 4],  6);
        RR2(bbb, ccc, ddd, eee, aaa, X[ 9], 15);
        RR2(aaa, bbb, ccc, ddd, eee, X[ 1], 13);
        RR2(eee, aaa, bbb, ccc, ddd, X[ 2], 11);

        R3(dd, ee, aa, bb, cc, X[ 3], 11);
        R3(cc, dd, ee, aa, bb, X[10], 13);
        R3(bb, cc, dd, ee, aa, X[14],  6);
        R3(aa, bb, cc, dd, ee, X[ 4],  7);
        R3(ee, aa, bb, cc, dd, X[ 9], 14);
        R3(dd, ee, aa, bb, cc, X[15],  9);
        R3(cc, dd, ee, aa, bb, X[ 8], 13);
        R3(bb, cc, dd, ee, aa, X[ 1], 15);
        R3(aa, bb, cc, dd, ee, X[ 2], 14);
        R3(ee, aa, bb, cc, dd, X[ 7],  8);
        R3(dd, ee, aa, bb, cc, X[ 0], 13);
        R3(cc, dd, ee, aa, bb, X[ 6],  6);
        R3(bb, cc, dd, ee, aa, X[13],  5);
        R3(aa, bb, cc, dd, ee, X[11], 12);
        R3(ee, aa, bb, cc, dd, X[ 5],  7);
        R3(dd, ee, aa, bb, cc, X[12],  5);

        RR3(ddd, eee, aaa, bbb, ccc, X[15],  9);
        RR3(ccc, ddd, eee, aaa, bbb, X[ 5],  7);
        RR3(bbb, ccc, ddd, eee, aaa, X[ 1], 15);
        RR3(aaa, bbb, ccc, ddd, eee, X[ 3], 11);
        RR3(eee, aaa, bbb, ccc, ddd, X[ 7],  8);
        RR3(ddd, eee, aaa, bbb, ccc, X[14],  6);
        RR3(ccc, ddd, eee, aaa, bbb, X[ 6],  6);
        RR3(bbb, ccc, ddd, eee, aaa, X[ 9], 14);
        RR3(aaa, bbb, ccc, ddd, eee, X[11], 12);
        RR3(eee, aaa, bbb, ccc, ddd, X[ 8], 13);
        RR3(ddd, eee, aaa, bbb, ccc, X[12],  5);
        RR3(ccc, ddd, eee, aaa, bbb, X[ 2], 14);
        RR3(bbb, ccc, ddd, eee, aaa, X[10], 13);
        RR3(aaa, bbb, ccc, ddd, eee, X[ 0], 13);
        RR3(eee, aaa, bbb, ccc, ddd, X[ 4],  7);
        RR3(ddd, eee, aaa, bbb, ccc, X[13],  5);

        R4(cc, dd, ee, aa, bb, X[ 1], 11);
        R4(bb, cc, dd, ee, aa, X[ 9], 12);
        R4(aa, bb, cc, dd, ee, X[11], 14);
        R4(ee, aa, bb, cc, dd, X[10], 15);
        R4(dd, ee, aa, bb, cc, X[ 0], 14);
        R4(cc, dd, ee, aa, bb, X[ 8], 15);
        R4(bb, cc, dd, ee, aa, X[12],  9);
        R4(aa, bb, cc, dd, ee, X[ 4],  8);
        R4(ee, aa, bb, cc, dd, X[13],  9);
        R4(dd, ee, aa, bb, cc, X[ 3], 14);
        R4(cc, dd, ee, aa, bb, X[ 7],  5);
        R4(bb, cc, dd, ee, aa, X[15],  6);
        R4(aa, bb, cc, dd, ee, X[14],  8);
        R4(ee, aa, bb, cc, dd, X[ 5],  6);
        R4(dd, ee, aa, bb, cc, X[ 6],  5);
        R4(cc, dd, ee, aa, bb, X[ 2], 12);

        RR4(ccc, ddd, eee, aaa, bbb, X[ 8], 15);
        RR4(bbb, ccc, ddd, eee, aaa, X[ 6],  5);
        RR4(aaa, bbb, ccc, ddd, eee, X[ 4],  8);
        RR4(eee, aaa, bbb, ccc, ddd, X[ 1], 11);
        RR4(ddd, eee, aaa, bbb, ccc, X[ 3], 14);
        RR4(ccc, ddd, eee, aaa, bbb, X[11], 14);
        RR4(bbb, ccc, ddd, eee, aaa, X[15],  6);
        RR4(aaa, bbb, ccc, ddd, eee, X[ 0], 14);
        RR4(eee, aaa, bbb, ccc, ddd, X[ 5],  6);
        RR4(ddd, eee, aaa, bbb, ccc, X[12],  9);
        RR4(ccc, ddd, eee, aaa, bbb, X[ 2], 12);
        RR4(bbb, ccc, ddd, eee, aaa, X[13],  9);
        RR4(aaa, bbb, ccc, ddd, eee, X[ 9], 12);
        RR4(eee, aaa, bbb, ccc, ddd, X[ 7],  5);
        RR4(ddd, eee, aaa, bbb, ccc, X[10], 15);
        RR4(ccc, ddd, eee, aaa, bbb, X[14],  8);

        R5(bb, cc, dd, ee, aa, X[ 4],  9);
        R5(aa, bb, cc, dd, ee, X[ 0], 15);
        R5(ee, aa, bb, cc, dd, X[ 5],  5);
        R5(dd, ee, aa, bb, cc, X[ 9], 11);
        R5(cc, dd, ee, aa, bb, X[ 7],  6);
        R5(bb, cc, dd, ee, aa, X[12],  8);
        R5(aa, bb, cc, dd, ee, X[ 2], 13);
        R5(ee, aa, bb, cc, dd, X[10], 12);
        R5(dd, ee, aa, bb, cc, X[14],  5);
        R5(cc, dd, ee, aa, bb, X[ 1], 12);
        R5(bb, cc, dd, ee, aa, X[ 3], 13);
        R5(aa, bb, cc, dd, ee, X[ 8], 14);
        R5(ee, aa, bb, cc, dd, X[11], 11);
        R5(dd, ee, aa, bb, cc, X[ 6],  8);
        R5(cc, dd, ee, aa, bb, X[15],  5);
        R5(bb, cc, dd, ee, aa, X[13],  6);

        RR5(bbb, ccc, ddd, eee, aaa, X[12] ,  8);
        RR5(aaa, bbb, ccc, ddd, eee, X[15] ,  5);
        RR5(eee, aaa, bbb, ccc, ddd, X[10] , 12);
        RR5(ddd, eee, aaa, bbb, ccc, X[ 4] ,  9);
        RR5(ccc, ddd, eee, aaa, bbb, X[ 1] , 12);
        RR5(bbb, ccc, ddd, eee, aaa, X[ 5] ,  5);
        RR5(aaa, bbb, ccc, ddd, eee, X[ 8] , 14);
        RR5(eee, aaa, bbb, ccc, ddd, X[ 7] ,  6);
        RR5(ddd, eee, aaa, bbb, ccc, X[ 6] ,  8);
        RR5(ccc, ddd, eee, aaa, bbb, X[ 2] , 13);
        RR5(bbb, ccc, ddd, eee, aaa, X[13] ,  6);
        RR5(aaa, bbb, ccc, ddd, eee, X[14] ,  5);
        RR5(eee, aaa, bbb, ccc, ddd, X[ 0] , 15);
        RR5(ddd, eee, aaa, bbb, ccc, X[ 3] , 13);
        RR5(ccc, ddd, eee, aaa, bbb, X[ 9] , 11);
        RR5(bbb, ccc, ddd, eee, aaa, X[11] , 11);

        ddd += cc + state_[ 1 ];
        state_[ 1 ] = state_[ 2 ] + dd + eee;
        state_[ 2 ] = state_[ 3 ] + ee + aaa;
        state_[ 3 ] = state_[ 4 ] + aa + bbb;
        state_[ 4 ] = state_[ 0 ] + bb + ccc;
        state_[ 0 ] = ddd;
    }

public:

    typedef digest<20> result_type;

    static constexpr std::size_t block_size = 64;

    ripemd_160() = default;

    explicit BOOST_CXX14_CONSTEXPR ripemd_160( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR ripemd_160( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    ripemd_160( void const * p, std::size_t n ): ripemd_160( static_cast<unsigned char const*>( p ), n )
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

    void update( void const * pv, std::size_t n )
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

        result_type digest;

        for( int i = 0; i < 5; ++i )
        {
            detail::write32le( &digest[ i * 4 ], state_[ i ] );
        }

        return digest;
    }
};

using hmac_ripemd_160 = hmac<ripemd_160>;
using hmac_ripemd_128 = hmac<ripemd_128>;

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_RIPEMD_HPP_INCLUDED
