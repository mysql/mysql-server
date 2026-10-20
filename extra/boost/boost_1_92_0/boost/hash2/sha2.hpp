#ifndef BOOST_HASH2_SHA2_HPP_INCLUDED
#define BOOST_HASH2_SHA2_HPP_INCLUDED

// Copyright 2024 Peter Dimov.
// Copyright 2024 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// SHA2 message digest algorithm, https://csrc.nist.gov/pubs/fips/180-4/upd1/final, https://www.rfc-editor.org/rfc/rfc6234

#include <boost/hash2/hmac.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/assert.hpp>
#include <array>
#include <cstdint>
#include <cstring>

namespace boost
{
namespace hash2
{

namespace detail
{

template<class Word, class Algo, int M>
struct sha2_base
{
    Word state_[ 8 ] = {};

    static constexpr int N = M;

    unsigned char buffer_[ N ] = {};
    std::size_t m_ = 0; // == n_ % N

    std::uint64_t n_ = 0;

    constexpr sha2_base() = default;

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {

        BOOST_ASSERT( m_ == n_ % N );

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

            Algo::transform( buffer_, state_ );
            m_ = 0;

            detail::memset( buffer_, 0, N );
        }

        BOOST_ASSERT( m_ == 0 );

        while( n >= N )
        {
            Algo::transform( p, state_ );

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
};

template<class = void>
struct sha2_256_constants
{
    constexpr static std::uint32_t const K[ 64 ] =
    {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };
};

template<class = void>
struct sha2_512_constants
{
    constexpr static std::uint64_t const K[ 80 ] =
    {
        0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
        0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
        0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
        0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
        0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
        0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
        0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
        0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
        0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
        0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
        0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
        0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
        0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
        0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
        0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
        0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
        0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
        0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
        0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
        0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817,
    };
};


// copy-paste from Boost.Unordered's prime_fmod approach
#if defined(BOOST_NO_CXX17_INLINE_VARIABLES)

// https://en.cppreference.com/w/cpp/language/static#Constant_static_members
// If a const non-inline (since C++17) static data member or a constexpr
// static data member (since C++11)(until C++17) is odr-used, a definition
// at namespace scope is still required, but it cannot have an
// initializer.
template<class T>
constexpr std::uint32_t sha2_256_constants<T>::K[ 64 ];

template<class  T>
constexpr std::uint64_t sha2_512_constants<T>::K[ 80 ];

#endif

struct sha2_256_base : public sha2_base<std::uint32_t, sha2_256_base, 64>
{
    BOOST_CXX14_CONSTEXPR static std::uint32_t Sigma0( std::uint32_t x ) noexcept
    {
        return detail::rotr( x, 2 ) ^ detail::rotr( x, 13 ) ^ detail::rotr( x, 22 );
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t Sigma1( std::uint32_t x ) noexcept
    {
        return detail::rotr( x, 6 ) ^ detail::rotr( x, 11 ) ^ detail::rotr( x, 25 );
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t sigma0( std::uint32_t x ) noexcept
    {
        return detail::rotr( x, 7 ) ^ detail::rotr( x, 18 ) ^ ( x >> 3 );
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t sigma1( std::uint32_t x ) noexcept
    {
        return detail::rotr( x, 17 ) ^ detail::rotr( x, 19 ) ^ ( x >> 10 );
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t Ch( std::uint32_t x, std::uint32_t y, std::uint32_t z ) noexcept
    {
        return ( x & y ) ^ ( ~x & z );
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t Maj( std::uint32_t x, std::uint32_t y, std::uint32_t z ) noexcept
    {
        return ( x & y ) ^ ( x & z ) ^ ( y & z );
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t W( std::uint32_t w[], int t )
    {
        return w[ t ] = ( sigma1( w[ t - 2 ] ) + w[ t - 7] + sigma0( w[ t - 15 ] ) + w[ t - 16 ] );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static void R1( std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t& d,
                                                             std::uint32_t e, std::uint32_t f, std::uint32_t g, std::uint32_t& h,
                                                             unsigned char const block[ 64 ], std::uint32_t const* K, std::uint32_t w[], int t )
    {
        w[ t ] = detail::read32be( block + t * 4);
        std::uint32_t T1 = h + Sigma1( e ) + Ch( e, f, g ) + K[ t ] + w[ t ];
        std::uint32_t T2 = Sigma0( a ) + Maj( a, b, c );

        d += T1;
        h = T1 + T2;
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static void R2( std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t& d,
                                                            std::uint32_t e, std::uint32_t f, std::uint32_t g, std::uint32_t& h,
                                                            unsigned char const block[ 64 ], std::uint32_t const* K, std::uint32_t w[], int t )
    {
        (void)block;

        std::uint32_t T1 = h + Sigma1( e ) + Ch( e, f, g ) + K[ t ] + W( w, t );
        std::uint32_t T2 = Sigma0( a ) + Maj( a, b, c );

        d += T1;
        h = T1 + T2;
    }


    BOOST_CXX14_CONSTEXPR static void transform( unsigned char const block[ 64 ], std::uint32_t state[ 8 ] )
    {
        auto K = sha2_256_constants<>::K;

        std::uint32_t w[ 64 ] = {};

        std::uint32_t a = state[ 0 ];
        std::uint32_t b = state[ 1 ];
        std::uint32_t c = state[ 2 ];
        std::uint32_t d = state[ 3 ];
        std::uint32_t e = state[ 4 ];
        std::uint32_t f = state[ 5 ];
        std::uint32_t g = state[ 6 ];
        std::uint32_t h = state[ 7 ];

        R1( a, b, c, d, e, f, g, h, block, K, w, 0 );
        R1( h, a, b, c, d, e, f, g, block, K, w, 1 );
        R1( g, h, a, b, c, d, e, f, block, K, w, 2 );
        R1( f, g, h, a, b, c, d, e, block, K, w, 3 );
        R1( e, f, g, h, a, b, c, d, block, K, w, 4 );
        R1( d, e, f, g, h, a, b, c, block, K, w, 5 );
        R1( c, d, e, f, g, h, a, b, block, K, w, 6 );
        R1( b, c, d, e, f, g, h, a, block, K, w, 7 );

        R1( a, b, c, d, e, f, g, h, block, K, w, 8 );
        R1( h, a, b, c, d, e, f, g, block, K, w, 9 );
        R1( g, h, a, b, c, d, e, f, block, K, w, 10 );
        R1( f, g, h, a, b, c, d, e, block, K, w, 11 );
        R1( e, f, g, h, a, b, c, d, block, K, w, 12 );
        R1( d, e, f, g, h, a, b, c, block, K, w, 13 );
        R1( c, d, e, f, g, h, a, b, block, K, w, 14 );
        R1( b, c, d, e, f, g, h, a, block, K, w, 15 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 16 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 17 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 18 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 19 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 20 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 21 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 22 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 23 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 24 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 25 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 26 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 27 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 28 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 29 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 30 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 31 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 32 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 33 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 34 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 35 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 36 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 37 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 38 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 39 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 40 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 41 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 42 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 43 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 44 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 45 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 46 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 47 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 48 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 49 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 50 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 51 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 52 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 53 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 54 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 55 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 56 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 57 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 58 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 59 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 60 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 61 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 62 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 63 );

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
};

struct sha2_512_base : public sha2_base<std::uint64_t, sha2_512_base, 128>
{
    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t Ch( std::uint64_t x, std::uint64_t y, std::uint64_t z ) noexcept
    {
        return ( x & y ) ^ ( ~x & z );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t Maj( std::uint64_t x, std::uint64_t y, std::uint64_t z ) noexcept
    {
        return ( x & y ) ^ ( x & z ) ^ ( y & z );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t Sigma0( std::uint64_t x ) noexcept
    {
        return detail::rotr( x, 28 ) ^ detail::rotr( x, 34 ) ^ detail::rotr( x, 39 );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t Sigma1( std::uint64_t x ) noexcept
    {
        return detail::rotr( x, 14 ) ^ detail::rotr( x, 18 ) ^ detail::rotr( x, 41 );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t sigma0( std::uint64_t x ) noexcept
    {
        return detail::rotr( x, 1 ) ^ detail::rotr( x, 8 ) ^ ( x >> 7 );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t sigma1( std::uint64_t x ) noexcept
    {
        return detail::rotr( x, 19 ) ^ detail::rotr( x, 61 ) ^ ( x >> 6 );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t W( std::uint64_t w[], int t )
    {
        return w[ t ] = ( sigma1( w[ t - 2 ] ) + w[ t - 7] + sigma0( w[ t - 15 ] ) + w[ t - 16 ] );
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static void R1( std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t& d,
                                                            std::uint64_t e, std::uint64_t f, std::uint64_t g, std::uint64_t& h,
                                                            unsigned char const block[ 64 ], std::uint64_t const* K, std::uint64_t w[], int t )
    {
        w[ t ] = detail::read64be( block + t * 8 );
        std::uint64_t T1 = h + Sigma1( e ) + Ch( e, f, g ) + K[ t ] + w[ t ];
        std::uint64_t T2 = Sigma0( a ) + Maj( a, b, c );

        d += T1;
        h = T1 + T2;
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static void R2( std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t& d,
                                                            std::uint64_t e, std::uint64_t f, std::uint64_t g, std::uint64_t& h,
                                                            unsigned char const block[ 64 ], std::uint64_t const* K, std::uint64_t w[], int t )
    {
        (void)block;

        std::uint64_t T1 = h + Sigma1( e ) + Ch( e, f, g ) + K[ t ] + W( w, t );
        std::uint64_t T2 = Sigma0( a ) + Maj( a, b, c );

        d += T1;
        h = T1 + T2;
    }

    BOOST_CXX14_CONSTEXPR static void transform( unsigned char const block[ 128 ], std::uint64_t state[ 8 ] )
    {
        auto K = sha2_512_constants<>::K;

        std::uint64_t w[ 80 ] = {};

        std::uint64_t a = state[ 0 ];
        std::uint64_t b = state[ 1 ];
        std::uint64_t c = state[ 2 ];
        std::uint64_t d = state[ 3 ];
        std::uint64_t e = state[ 4 ];
        std::uint64_t f = state[ 5 ];
        std::uint64_t g = state[ 6 ];
        std::uint64_t h = state[ 7 ];

        R1( a, b, c, d, e, f, g, h, block, K, w, 0 );
        R1( h, a, b, c, d, e, f, g, block, K, w, 1 );
        R1( g, h, a, b, c, d, e, f, block, K, w, 2 );
        R1( f, g, h, a, b, c, d, e, block, K, w, 3 );
        R1( e, f, g, h, a, b, c, d, block, K, w, 4 );
        R1( d, e, f, g, h, a, b, c, block, K, w, 5 );
        R1( c, d, e, f, g, h, a, b, block, K, w, 6 );
        R1( b, c, d, e, f, g, h, a, block, K, w, 7 );

        R1( a, b, c, d, e, f, g, h, block, K, w, 8 );
        R1( h, a, b, c, d, e, f, g, block, K, w, 9 );
        R1( g, h, a, b, c, d, e, f, block, K, w, 10 );
        R1( f, g, h, a, b, c, d, e, block, K, w, 11 );
        R1( e, f, g, h, a, b, c, d, block, K, w, 12 );
        R1( d, e, f, g, h, a, b, c, block, K, w, 13 );
        R1( c, d, e, f, g, h, a, b, block, K, w, 14 );
        R1( b, c, d, e, f, g, h, a, block, K, w, 15 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 16 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 17 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 18 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 19 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 20 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 21 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 22 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 23 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 24 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 25 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 26 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 27 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 28 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 29 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 30 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 31 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 32 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 33 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 34 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 35 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 36 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 37 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 38 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 39 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 40 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 41 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 42 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 43 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 44 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 45 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 46 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 47 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 48 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 49 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 50 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 51 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 52 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 53 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 54 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 55 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 56 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 57 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 58 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 59 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 60 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 61 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 62 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 63 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 64 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 65 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 66 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 67 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 68 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 69 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 70 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 71 );

        R2( a, b, c, d, e, f, g, h, block, K, w, 72 );
        R2( h, a, b, c, d, e, f, g, block, K, w, 73 );
        R2( g, h, a, b, c, d, e, f, block, K, w, 74 );
        R2( f, g, h, a, b, c, d, e, block, K, w, 75 );
        R2( e, f, g, h, a, b, c, d, block, K, w, 76 );
        R2( d, e, f, g, h, a, b, c, block, K, w, 77 );
        R2( c, d, e, f, g, h, a, b, block, K, w, 78 );
        R2( b, c, d, e, f, g, h, a, block, K, w, 79 );

        state[ 0 ] += a;
        state[ 1 ] += b;
        state[ 2 ] += c;
        state[ 3 ] += d;
        state[ 4 ] += e;
        state[ 5 ] += f;
        state[ 6 ] += g;
        state[ 7 ] += h;
    }
};

} // namespace detail

class sha2_256 : detail::sha2_256_base
{
private:

    BOOST_CXX14_CONSTEXPR void init()
    {
        state_[ 0 ] = 0x6a09e667;
        state_[ 1 ] = 0xbb67ae85;
        state_[ 2 ] = 0x3c6ef372;
        state_[ 3 ] = 0xa54ff53a;
        state_[ 4 ] = 0x510e527f;
        state_[ 5 ] = 0x9b05688c;
        state_[ 6 ] = 0x1f83d9ab;
        state_[ 7 ] = 0x5be0cd19;
    }

public:

    using result_type = digest<32>;

    static constexpr std::size_t block_size = 64;

    BOOST_CXX14_CONSTEXPR sha2_256()
    {
        init();
    }

    BOOST_CXX14_CONSTEXPR explicit sha2_256( std::uint64_t seed )
    {
        init();

        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha2_256( unsigned char const * p, std::size_t n )
    {
        init();

        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    sha2_256( void const * p, std::size_t n ): sha2_256( static_cast<unsigned char const*>( p ), n )
    {
    }

    using detail::sha2_256_base::update;

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        unsigned char bits[ 8 ] = {};
        detail::write64be( bits, n_ * 8 );

        std::size_t k = m_ < 56 ? 56 - m_ : 64 + 56 - m_;
        unsigned char padding[ 64 ] = { 0x80 };

        update( padding, k );
        update( bits, 8 );
        BOOST_ASSERT( m_ == 0 );

        result_type digest;
        for( int i = 0; i < 8; ++i )
        {
            detail::write32be( &digest[ i * 4 ], state_[ i ] );
        }

        return digest;
    }
};

class sha2_224 : detail::sha2_256_base
{
private:

    BOOST_CXX14_CONSTEXPR void init()
    {
        state_[ 0 ] = 0xc1059ed8;
        state_[ 1 ] = 0x367cd507;
        state_[ 2 ] = 0x3070dd17;
        state_[ 3 ] = 0xf70e5939;
        state_[ 4 ] = 0xffc00b31;
        state_[ 5 ] = 0x68581511;
        state_[ 6 ] = 0x64f98fa7;
        state_[ 7 ] = 0xbefa4fa4;
    }

public:

    using result_type = digest<28>;

    static constexpr std::size_t block_size = 64;

    BOOST_CXX14_CONSTEXPR sha2_224()
    {
        init();
    }

    BOOST_CXX14_CONSTEXPR explicit sha2_224( std::uint64_t seed )
    {
        init();

        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha2_224( unsigned char const * p, std::size_t n )
    {
        init();

        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }


    sha2_224( void const * p, std::size_t n ): sha2_224( static_cast<unsigned char const*>( p ), n )
    {
    }

    using detail::sha2_256_base::update;

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        unsigned char bits[ 8 ] = {};
        detail::write64be( bits, n_ * 8 );

        std::size_t k = m_ < 56 ? 56 - m_ : 64 + 56 - m_;
        unsigned char padding[ 64 ] = { 0x80 };

        update( padding, k );
        update( bits, 8 );
        BOOST_ASSERT( m_ == 0 );

        result_type digest;
        for( int i = 0; i < 7; ++i ) {
            detail::write32be( &digest[ i * 4 ], state_[ i ] );
        }

        return digest;
    }
};

class sha2_512 : detail::sha2_512_base
{
private:

    BOOST_CXX14_CONSTEXPR void init()
    {
        state_[ 0 ] = 0x6a09e667f3bcc908;
        state_[ 1 ] = 0xbb67ae8584caa73b;
        state_[ 2 ] = 0x3c6ef372fe94f82b;
        state_[ 3 ] = 0xa54ff53a5f1d36f1;
        state_[ 4 ] = 0x510e527fade682d1;
        state_[ 5 ] = 0x9b05688c2b3e6c1f;
        state_[ 6 ] = 0x1f83d9abfb41bd6b;
        state_[ 7 ] = 0x5be0cd19137e2179;
    }

public:

    using result_type = digest<64>;

    using detail::sha2_512_base::update;

    static constexpr std::size_t block_size = 128;

    BOOST_CXX14_CONSTEXPR sha2_512()
    {
        init();
    }

    BOOST_CXX14_CONSTEXPR explicit sha2_512( std::uint64_t seed )
    {
        init();

        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha2_512( unsigned char const * p, std::size_t n )
    {
        init();

        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    sha2_512( void const * p, std::size_t n ): sha2_512( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        unsigned char bits[ 16 ] = { 0 };
        detail::write64be( bits + 8, n_ * 8 );

        std::size_t k = m_ < 112 ? 112 - m_ : 128 + 112 - m_;
        unsigned char padding[ 128 ] = { 0x80 };

        update( padding, k );
        update( bits, 16 );
        BOOST_ASSERT( m_ == 0 );

        result_type digest;
        for( int i = 0; i < 8; ++i )
        {
            detail::write64be( &digest[ i * 8 ], state_[ i ] );
        }

        return digest;
    }
};

class sha2_384 : detail::sha2_512_base
{
private:

    BOOST_CXX14_CONSTEXPR void init()
    {
        state_[ 0 ] = 0xcbbb9d5dc1059ed8;
        state_[ 1 ] = 0x629a292a367cd507;
        state_[ 2 ] = 0x9159015a3070dd17;
        state_[ 3 ] = 0x152fecd8f70e5939;
        state_[ 4 ] = 0x67332667ffc00b31;
        state_[ 5 ] = 0x8eb44a8768581511;
        state_[ 6 ] = 0xdb0c2e0d64f98fa7;
        state_[ 7 ] = 0x47b5481dbefa4fa4;
    }

public:

    using result_type = digest<48>;

    static constexpr std::size_t block_size = 128;

    using detail::sha2_512_base::update;

    BOOST_CXX14_CONSTEXPR sha2_384()
    {
        init();
    }

    BOOST_CXX14_CONSTEXPR explicit sha2_384( std::uint64_t seed )
    {
        init();

        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha2_384( unsigned char const * p, std::size_t n )
    {
        init();

        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    sha2_384( void const * p, std::size_t n ): sha2_384( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        unsigned char bits[ 16 ] = { 0 };
        detail::write64be( bits + 8, n_ * 8 );

        std::size_t k = m_ < 112 ? 112 - m_ : 128 + 112 - m_;
        unsigned char padding[ 128 ] = { 0x80 };

        update( padding, k );
        update( bits, 16 );
        BOOST_ASSERT( m_ == 0 );

        result_type digest;
        for( int i = 0; i < 6; ++i )
        {
            detail::write64be( &digest[ i * 8 ], state_[ i ] );
        }

        return digest;
    }
};

class sha2_512_224 : detail::sha2_512_base
{
private:

    BOOST_CXX14_CONSTEXPR void init()
    {
        state_[ 0 ] = 0x8c3d37c819544da2;
        state_[ 1 ] = 0x73e1996689dcd4d6;
        state_[ 2 ] = 0x1dfab7ae32ff9c82;
        state_[ 3 ] = 0x679dd514582f9fcf;
        state_[ 4 ] = 0x0f6d2b697bd44da8;
        state_[ 5 ] = 0x77e36f7304c48942;
        state_[ 6 ] = 0x3f9d85a86a1d36c8;
        state_[ 7 ] = 0x1112e6ad91d692a1;
    }

public:

    using result_type = digest<28>;

    static constexpr std::size_t block_size = 128;

    using detail::sha2_512_base::update;

    BOOST_CXX14_CONSTEXPR sha2_512_224()
    {
        init();
    }

    BOOST_CXX14_CONSTEXPR explicit sha2_512_224( std::uint64_t seed )
    {
        init();

        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha2_512_224( unsigned char const * p, std::size_t n )
    {
        init();

        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }


    sha2_512_224( void const * p, std::size_t n ): sha2_512_224( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        unsigned char bits[ 16 ] = { 0 };
        detail::write64be( bits + 8, n_ * 8 );

        std::size_t k = m_ < 112 ? 112 - m_ : 128 + 112 - m_;
        unsigned char padding[ 128 ] = { 0x80 };

        update( padding, k );
        update( bits, 16 );
        BOOST_ASSERT( m_ == 0 );

        result_type digest;
        for( int i = 0; i < 3; ++i )
        {
            detail::write64be( &digest[ i * 8 ], state_[ i ] );
        }
        detail::write32be( &digest[ 3 * 8 ], state_[ 3 ] >> 32 );

        return digest;
    }
};

class sha2_512_256 : detail::sha2_512_base
{
private:

    BOOST_CXX14_CONSTEXPR void init()
    {
        state_[ 0 ] = 0x22312194fc2bf72c;
        state_[ 1 ] = 0x9f555fa3c84c64c2;
        state_[ 2 ] = 0x2393b86b6f53b151;
        state_[ 3 ] = 0x963877195940eabd;
        state_[ 4 ] = 0x96283ee2a88effe3;
        state_[ 5 ] = 0xbe5e1e2553863992;
        state_[ 6 ] = 0x2b0199fc2c85b8aa;
        state_[ 7 ] = 0x0eb72ddc81c52ca2;
    }

public:

    using result_type = digest<32>;

    static constexpr std::size_t block_size = 128;

    using detail::sha2_512_base::update;

    BOOST_CXX14_CONSTEXPR sha2_512_256()
    {
        init();
    }

    BOOST_CXX14_CONSTEXPR explicit sha2_512_256( std::uint64_t seed )
    {
        init();

        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            update( tmp, 8 );
            result();
        }
    }

    BOOST_CXX14_CONSTEXPR sha2_512_256( unsigned char const * p, std::size_t n )
    {
        init();

        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    sha2_512_256( void const * p, std::size_t n ): sha2_512_256( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        unsigned char bits[ 16 ] = { 0 };
        detail::write64be( bits + 8, n_ * 8 );

        std::size_t k = m_ < 112 ? 112 - m_ : 128 + 112 - m_;
        unsigned char padding[ 128 ] = { 0x80 };

        update( padding, k );
        update( bits, 16 );
        BOOST_ASSERT( m_ == 0 );

        result_type digest;
        for( int i = 0; i < 4; ++i )
        {
            detail::write64be( &digest[ i * 8 ], state_[ i ] );
        }

        return digest;
    }
};

// hmac wrappers

using hmac_sha2_256 = hmac<sha2_256>;
using hmac_sha2_224 = hmac<sha2_224>;
using hmac_sha2_512 = hmac<sha2_512>;
using hmac_sha2_384 = hmac<sha2_384>;
using hmac_sha2_512_224 = hmac<sha2_512_224>;
using hmac_sha2_512_256 = hmac<sha2_512_256>;

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_SHA2_HPP_INCLUDED
