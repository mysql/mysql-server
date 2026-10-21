#ifndef BOOST_HASH2_XXH3_HPP_INCLUDED
#define BOOST_HASH2_XXH3_HPP_INCLUDED

// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// xxHash, https://cyan4973.github.io/xxHash/

#include <boost/hash2/digest.hpp>
#include <boost/hash2/detail/byteswap.hpp>
#include <boost/hash2/detail/is_constant_evaluated.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/hash2/detail/mul128.hpp>
#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <cstdint>
#include <cstring>
#include <cstddef>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(push)
# pragma warning(disable: 4307) // '+': integral constant overflow
#endif

namespace boost
{
namespace hash2
{

template<class = void>
struct xxh3_128_constants
{
    constexpr static unsigned char const default_secret[ 192 ] =
    {
        0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
        0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
        0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
        0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
        0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
        0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
        0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
        0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
        0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
        0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
        0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
        0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
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
constexpr unsigned char xxh3_128_constants<T>::default_secret[ 192 ];

#endif

class xxh3_128
{
private:

    static constexpr std::size_t const default_secret_len = 192;
    static constexpr std::size_t const min_secret_len     = 136;
    static constexpr std::size_t const buffer_size        = 256;

    static constexpr std::uint64_t const P32_1     = 0x9E3779B1U;
    static constexpr std::uint64_t const P32_2     = 0x85EBCA77U;
    static constexpr std::uint64_t const P32_3     = 0xC2B2AE3DU;
    static constexpr std::uint64_t const P64_1     = 0x9E3779B185EBCA87ULL;
    static constexpr std::uint64_t const P64_2     = 0xC2B2AE3D27D4EB4FULL;
    static constexpr std::uint64_t const P64_3     = 0x165667B19E3779F9ULL;
    static constexpr std::uint64_t const P64_4     = 0x85EBCA77C2B2AE63ULL;
    static constexpr std::uint64_t const P64_5     = 0x27D4EB2F165667C5ULL;
    static constexpr std::uint64_t const PRIME_MX1 = 0x165667919E3779F9ULL;
    static constexpr std::uint64_t const PRIME_MX2 = 0x9FB21C651E98DF25ULL;

    BOOST_CXX14_CONSTEXPR void init_secret_from_seed( std::uint64_t seed )
    {
        auto const secret = xxh3_128_constants<>::default_secret;

        std::size_t num_rounds = default_secret_len / 16;
        for( std::size_t i = 0; i < num_rounds; ++i )
        {
            auto low  = detail::read64le( secret + 16 * i ) + seed;
            auto high = detail::read64le( secret + 16 * i + 8 ) - seed;

            detail::write64le( secret_ + 16 * i, low );
            detail::write64le( secret_ + 16 * i + 8, high );
        }
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t avalanche( std::uint64_t x )
    {
        x ^= ( x >> 37 );
        x *= PRIME_MX1;
        x ^= ( x >> 32 );
        return x;
    }

    BOOST_FORCEINLINE BOOST_CXX14_CONSTEXPR static std::uint64_t avalanche_xxh64( std::uint64_t x )
    {
        x ^= x >> 33;
        x *= P64_2;
        x ^= x >> 29;
        x *= P64_3;
        x ^= x >> 32;
        return x;
    }

    BOOST_CXX14_CONSTEXPR std::uint64_t mix_step( unsigned char const* data, std::size_t secret_offset, std::uint64_t seed )
    {
        auto const secret = with_secret_? secret_: xxh3_128_constants<>::default_secret;

        std::uint64_t data_words[ 2 ] = {};
        std::uint64_t secret_words[ 2 ] = {};
        for( int i = 0; i < 2; ++i )
        {
            data_words[ i ] = detail::read64le( data + 8 * i );
            secret_words[ i ] = detail::read64le( secret + secret_offset + 8 * i );
        }

        detail::uint128 r = detail::mul128( data_words[ 0 ] ^ ( secret_words[ 0 ] + seed ), data_words[ 1 ] ^ ( secret_words[ 1 ] - seed ) );
        return r.low ^ r.high;
    }

    BOOST_CXX14_CONSTEXPR void mix_two_chunks( unsigned char const* x, unsigned char const* y, std::size_t secret_offset, std::uint64_t seed, std::uint64_t (&acc)[ 2 ] )
    {
        std::uint64_t data_words1[ 2 ] = {};
        std::uint64_t data_words2[ 2 ] = {};

        for( int i = 0; i < 2; ++i )
        {
            data_words1[ i ] = detail::read64le( x + 8 * i );
            data_words2[ i ] = detail::read64le( y + 8 * i );
        }

        acc[ 0 ] += mix_step( x, secret_offset, seed );
        acc[ 1 ] += mix_step( y, secret_offset + 16, seed );
        acc[ 0 ] ^= ( data_words2[ 0 ] + data_words2[ 1 ] );
        acc[ 1 ] ^= ( data_words1[ 0 ] + data_words1[ 1 ] );
    }

    BOOST_CXX14_CONSTEXPR void accumulate( std::uint64_t stripe[ 8 ], std::size_t secret_offset )
    {
        std::uint64_t secret_words[ 8 ] = {};
        for( int i = 0; i < 8; ++i )
        {
            secret_words[ i ] = detail::read64le( secret_ + secret_offset + 8 * i );
        }

        for( int i = 0; i < 8; ++i )
        {
            std::uint64_t value = stripe[ i ] ^ secret_words[ i ];
            acc_[ i ^ 1 ] = acc_[ i ^ 1 ] + stripe[ i ];
            acc_[ i ] = acc_[ i ] + ( value & 0xffffffff ) * ( value >> 32 );
        }
    }

    BOOST_CXX14_CONSTEXPR void scramble()
    {
        std::uint64_t secret_words[ 8 ] = {};
        for( int i = 0; i < 8; ++i )
        {
            secret_words[ i ] = detail::read64le( secret_ + ( secret_len_ - 64 ) + ( 8 * i ) );
        }

        for( int i = 0; i < 8; ++i )
        {
            acc_[ i ] ^= acc_[ i ] >> 47;
            acc_[ i ] ^= secret_words[ i ];
            acc_[ i ] *= P32_1;
        }
    }

    BOOST_CXX14_CONSTEXPR void last_round()
    {
        unsigned char last_stripe[ 64 ] = {};
        unsigned char* last_stripe_ptr = nullptr;

        if( m_ >= 64 )
        {
            std::size_t num_stripes = ( m_ == 0 ? 0 : ( m_ - 1 ) / 64 );
            for( std::size_t n = 0; n < num_stripes; ++n )
            {
                std::uint64_t stripe[ 8 ] = {};
                for( int i = 0; i < 8; ++i )
                {
                    stripe[ i ] = detail::read64le( buffer_ + ( 64 * n ) + ( 8 * i ) );
                }
                accumulate( stripe, 8 * num_stripes_++ );

                std::size_t const stripes_per_block_ = ( secret_len_ - 64 ) / 8;
                (void)stripes_per_block_;

                BOOST_ASSERT( num_stripes_ <= stripes_per_block_ );
            }

            last_stripe_ptr = buffer_ + m_ - 64;
        }
        else
        {
            std::size_t len = 64 - m_;

            detail::memcpy( last_stripe, buffer_ + buffer_size - len, len );
            detail::memcpy( last_stripe + len, buffer_, m_ );

            last_stripe_ptr = last_stripe;
        }

        std::uint64_t stripe[ 8 ] = {};
        for( int i = 0; i < 8; ++i )
        {
            stripe[ i ] = detail::read64le( last_stripe_ptr + ( 8 * i ) );
        }

        accumulate( stripe, secret_len_ - 71 );
    }

    BOOST_CXX14_CONSTEXPR std::uint64_t final_merge( std::uint64_t init_value, std::size_t secret_offset )
    {
        std::uint64_t secret_words[ 8 ] = {};
        for( int i = 0; i < 8; ++i )
        {
            secret_words[ i ] = detail::read64le( secret_ + secret_offset + 8 * i );
        }

        std::uint64_t result = init_value;
        for( int i = 0; i < 4; ++i )
        {
            auto mul_result = detail::mul128( acc_[ 2 * i ] ^ secret_words[ 2 * i ], acc_[ 2 * i + 1 ] ^ secret_words[ 2 * i + 1 ] );
            result += mul_result.low ^ mul_result.high;
        }

        return avalanche( result );
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_empty()
    {
        auto const secret = with_secret_? secret_: xxh3_128_constants<>::default_secret;

        std::uint64_t secret_words[ 4 ] = {};
        for( int i = 0; i < 4; ++i )
        {
            secret_words[ i ] = detail::read64le( secret + 64 + 8 * i );
        }

        digest<16> r;
        detail::write64be( r.data() + 8, avalanche_xxh64( seed_ ^ secret_words[ 0 ] ^ secret_words[ 1 ] ) );
        detail::write64be( r.data() + 0, avalanche_xxh64( seed_ ^ secret_words[ 2 ] ^ secret_words[ 3 ] ) );
        return r;
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_1to3()
    {
        auto const secret = with_secret_? secret_: xxh3_128_constants<>::default_secret;

        std::uint32_t v1 = buffer_[ ( n_ - 1 ) ];
        std::uint32_t v2 = static_cast<std::uint32_t>( n_ << 8 );
        std::uint32_t v3 = buffer_[ 0 ] << 16;
        std::uint32_t v4 = buffer_[ ( n_ >> 1 ) ] << 24;

        std::uint32_t combined = v1 | v2 | v3 | v4;

        std::uint32_t secret_words[ 4 ] = {};
        for( int i = 0; i < 4; ++i )
        {
            secret_words[ i ] = detail::read32le( secret + 4 * i );
        }

        std::uint64_t low  = ( ( secret_words[ 0 ] ^ secret_words[ 1 ] ) + seed_ ) ^ combined;
        std::uint64_t high = ( ( secret_words[ 2 ] ^ secret_words[ 3 ] ) - seed_ ) ^ ( detail::rotl( detail::byteswap( combined ), 13 ) );

        digest<16> r;
        detail::write64be( r.data() + 8, avalanche_xxh64( low ) );
        detail::write64be( r.data() + 0, avalanche_xxh64( high ) );

        return r;
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_4to8()
    {
        auto const secret = with_secret_? secret_: xxh3_128_constants<>::default_secret;

        std::uint32_t input_first = detail::read32le( buffer_ );
        std::uint32_t input_last  = detail::read32le( buffer_ + ( n_ - 4 ) );
        std::uint64_t modified_seed = seed_ ^ ( std::uint64_t{ detail::byteswap( static_cast<std::uint32_t>( seed_ ) ) } << 32 );

        std::uint64_t secret_words[ 2 ] = {};
        for( int i = 0; i < 2; ++i )
        {
            secret_words[ i ] = detail::read64le( secret + 16 + i * 8 );
        }

        std::uint64_t combined = std::uint64_t{ input_first } | ( std::uint64_t{ input_last } << 32 );
        std::uint64_t value = ( ( secret_words[ 0 ] ^ secret_words[ 1 ] ) + modified_seed ) ^ combined;

        detail::uint128 mul_result = detail::mul128( value, P64_1 + ( n_ << 2 ) );
        std::uint64_t high = mul_result.high;
        std::uint64_t low = mul_result.low;

        high += ( low << 1 );
        low ^= ( high >> 3 );
        low ^= ( low >> 35 );
        low *= PRIME_MX2;
        low ^= ( low >> 28 );

        high = avalanche( high );

        digest<16> r;
        detail::write64be( r.data() + 0, high );
        detail::write64be( r.data() + 8, low );

        return r;
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_9to16()
    {
        auto const secret = with_secret_? secret_: xxh3_128_constants<>::default_secret;

        std::uint64_t input_first = detail::read64le( buffer_ );
        std::uint64_t input_last = detail::read64le( buffer_ + ( n_ - 8 ) );

        std::uint64_t secret_words[ 4 ] = {};
        for( int i = 0; i < 4; ++i )
        {
            secret_words[ i ] = detail::read64le( secret + 32 + ( i * 8 ) );
        }

        std::uint64_t val1 = ( ( secret_words[ 0 ] ^ secret_words[ 1 ] ) - seed_ ) ^ input_first ^ input_last;
        std::uint64_t val2 = ( ( secret_words[ 2 ] ^ secret_words[ 3 ] ) + seed_ ) ^ input_last;

        detail::uint128 mul_result = detail::mul128( val1, P64_1 );
        std::uint64_t low = mul_result.low + ( std::uint64_t{ n_ - 1 } << 54 );
        std::uint64_t high = mul_result.high + val2 + ( val2 & 0x00000000ffffffff ) * ( P32_2 - 1 );

        low ^= detail::byteswap( high );

        detail::uint128 mul_result2 = detail::mul128( low, P64_2 );
        low = mul_result2.low;
        high = mul_result2.high + high * P64_2;

        digest<16> r;
        detail::write64be( r.data() + 0, avalanche( high ) );
        detail::write64be( r.data() + 8, avalanche( low ) );

        return r;
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_17to128()
    {
        std::uint64_t acc[ 2 ] = { n_ * P64_1, 0 };

        std::uint64_t num_rounds = ( ( n_ - 1 ) >> 5 ) + 1;
        for( std::int64_t i = num_rounds - 1; i >= 0; --i )
        {
            std::size_t offset_start = static_cast<std::size_t>( 16 * i );
            std::size_t offset_end = n_ - static_cast<std::size_t>( 16 * i ) - 16;

            mix_two_chunks( buffer_ + offset_start, buffer_ + offset_end, static_cast<std::size_t>( 32 * i ), seed_, acc );
        }

        std::uint64_t low = acc[ 0 ] + acc[ 1 ];
        std::uint64_t high = ( acc[ 0 ] * P64_1 ) + ( acc[ 1 ] * P64_4 ) + ( ( std::uint64_t{ n_ } - seed_ ) * P64_2 );

        digest<16> r;
        detail::write64be( r.data() + 0, std::uint64_t{ 0 } - avalanche( high ) );
        detail::write64be( r.data() + 8, avalanche( low ) );

        return r;
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_129to240()
    {
        std::uint64_t acc[ 2 ] = { n_ * P64_1, 0 };

        std::uint64_t num_chunks = n_ >> 5;

        for( std::size_t i = 0; i < 4; ++i )
        {
            mix_two_chunks( buffer_ + 32 * i, buffer_ + ( 32 * i ) + 16, 32 * i, seed_, acc );
        }

        acc[ 0 ] = avalanche( acc[ 0 ] );
        acc[ 1 ] = avalanche( acc[ 1 ] );

        for( std::size_t i = 4; i < num_chunks; ++i )
        {
            mix_two_chunks( buffer_ + 32 * i, buffer_ + ( 32 * i ) + 16, ( i - 4 ) * 32 + 3, seed_, acc );
        }

        mix_two_chunks( buffer_ + n_ - 16, buffer_ + n_ - 32, 103, std::uint64_t{ 0 } - seed_, acc );

        std::uint64_t low = acc[ 0 ] + acc[ 1 ];
        std::uint64_t high = ( acc[ 0 ] * P64_1 ) + ( acc[ 1 ] * P64_4 ) + ( ( std::uint64_t{ n_ } - seed_ ) * P64_2 );

        digest<16> r;
        detail::write64be( r.data() + 0, std::uint64_t{ 0 } - avalanche( high ) );
        detail::write64be( r.data() + 8, avalanche( low ) );

        return r;
    }

    BOOST_CXX14_CONSTEXPR digest<16> xxh3_128_digest_long()
    {
        last_round();

        std::uint64_t low = final_merge( n_ * P64_1, 11 );
        std::uint64_t high = final_merge( ~( n_ * P64_2 ), secret_len_ - 75 );

        digest<16> r;
        detail::write64be( r.data() + 0, high );
        detail::write64be( r.data() + 8, low );

        return r;
    }

    BOOST_CXX14_CONSTEXPR static std::uint64_t combine( std::uint64_t v1, std::uint64_t v2 )
    {
        return avalanche( v1 + v2 );
    }

private:

    unsigned char secret_[ default_secret_len ] = {};
    std::uint64_t seed_ = 0;
    bool with_secret_ = false;

    unsigned char buffer_[ buffer_size ] = {};

    std::uint64_t acc_[ 8 ] = { P32_3, P64_1, P64_2, P64_3, P64_4, P32_2, P64_5, P32_1 };

    std::size_t n_ = 0;
    std::size_t m_ = 0;

    std::size_t secret_len_ = default_secret_len;

    std::size_t num_stripes_ = 0; // current number of processed stripes

public:

    using result_type = digest<16>;

    BOOST_CXX14_CONSTEXPR xxh3_128()
    {
        detail::memcpy( secret_, xxh3_128_constants<>::default_secret, default_secret_len );
    }

    BOOST_CXX14_CONSTEXPR explicit xxh3_128( std::uint64_t seed ): seed_( seed )
    {
        init_secret_from_seed( seed );
    }

    xxh3_128( void const* p, std::size_t n ): xxh3_128( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR xxh3_128( unsigned char const* p, std::size_t n )
    {
        detail::memcpy( secret_, xxh3_128_constants<>::default_secret, default_secret_len );

        if( n == 0 ) return;

        with_secret_ = true;

        std::size_t const n2 = n;
        std::uint64_t seed = 0;

        while( n >= default_secret_len )
        {
            for( std::size_t i = 0; i < default_secret_len / 8; ++i )
            {
                std::uint64_t v1 = detail::read64le( p + i * 8 );
                std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

                detail::write64le( secret_ + i * 8, combine( v1, v2 ) );

                seed = combine( seed, v1 );
            }

            p += default_secret_len;
            n -= default_secret_len;
        }

        {
            std::size_t i = 0;

            for( ; i < n / 8; ++i )
            {
                std::uint64_t v1 = detail::read64le( p + i * 8 );
                std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

                detail::write64le( secret_ + i * 8, combine( v1, v2 ) );

                seed = combine( seed, v1 );
            }

            n = n % 8;

            if( n > 0 )
            {
                unsigned char w[ 8 ] = {};
                detail::memcpy( w, p + i * 8, n );

                std::uint64_t v1 = detail::read64le( w );
                std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

                detail::write64le( secret_ + i * 8, combine( v1, v2 ) );

                seed = combine( seed, v1 );
            }
        }

        {
            std::size_t const i = 0;

            std::uint64_t v1 = n2;
            std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

            detail::write64le( secret_ + i * 8, combine( v1, v2 ) );

            seed = combine( seed, v1 );
        }

        seed_ = seed;
    }

private: // supporting constructor for the static factory functions

    BOOST_CXX14_CONSTEXPR xxh3_128( std::uint64_t seed, unsigned char const* p, std::size_t n, bool with_secret ): seed_( seed ), with_secret_( with_secret )
    {
        if( n < min_secret_len )
        {
            // this is a precondition violation for XXH3, but we try to do something reasonable
            detail::memcpy( secret_, xxh3_128_constants<>::default_secret, default_secret_len );
            secret_len_ = default_secret_len;
        }
        else if( n < default_secret_len )
        {
            secret_len_ = n;
        }
        else
        {
            secret_len_ = default_secret_len;
        }

        // incorporate passed secret into secret_
        // in the case where min_secret_len <= n <= default_secret_len,
        //  this is a simple copy because the initial secret_ is {}

        while( n >= default_secret_len )
        {
            for( std::size_t i = 0; i < default_secret_len / 8; ++i )
            {
                std::uint64_t v1 = detail::read64le( p + i * 8 );
                std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

                detail::write64le( secret_ + i * 8, v1 + v2 );
            }

            p += default_secret_len;
            n -= default_secret_len;
        }

        {
            std::size_t i = 0;

            for( ; i < n / 8; ++i )
            {
                std::uint64_t v1 = detail::read64le( p + i * 8 );
                std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

                detail::write64le( secret_ + i * 8, v1 + v2 );
            }

            n = n % 8;

            if( n > 0 )
            {
                unsigned char w[ 8 ] = {};
                detail::memcpy( w, p + i * 8, n );

                std::uint64_t v1 = detail::read64le( w );
                std::uint64_t v2 = detail::read64le( secret_ + i * 8 );

                detail::write64le( secret_ + i * 8, v1 + v2 );
            }
        }
    }

public:

    // XXH3-specific named constructors, matching the reference implementation

    // for completeness only
    static BOOST_CXX14_CONSTEXPR xxh3_128 with_seed( std::uint64_t seed )
    {
        return xxh3_128( seed );
    }

    static BOOST_CXX14_CONSTEXPR xxh3_128 with_secret( unsigned char const* p, std::size_t n )
    {
        return xxh3_128( 0, p, n, true );
    }

    static xxh3_128 with_secret( void const* p, std::size_t n )
    {
        return with_secret( static_cast<unsigned char const*>( p ), n );
    }

    static BOOST_CXX14_CONSTEXPR xxh3_128 with_secret_and_seed( unsigned char const* p, std::size_t n, std::uint64_t seed )
    {
        return xxh3_128( seed, p, n, false );
    }

    static xxh3_128 with_secret_and_seed( void const* p, std::size_t n, std::uint64_t seed )
    {
        return with_secret_and_seed( static_cast<unsigned char const*>( p ), n, seed );
    }

    void update( void const* p, std::size_t n )
    {
        update( static_cast<unsigned char const*>( p ), n );
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        if( n == 0 ) return;

        std::size_t const stripes_per_block_ = ( secret_len_ - 64 ) / 8;

        n_ += n;

        if( n <= buffer_size - m_ )
        {
            detail::memcpy( buffer_ + m_, p, n );
            m_ += n;
            return;
        }

        if( m_ > 0 )
        {
            std::size_t k = buffer_size - m_;
            detail::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;

            for( std::size_t i = 0; i < 4; ++i )
            {
                std::uint64_t stripe[ 8 ] = {};
                for( int j = 0; j < 8; ++j )
                {
                    stripe[ j ] = detail::read64le( buffer_ + ( 64 * i)  + ( 8 * j ) );
                }
                accumulate( stripe, 8 * num_stripes_ );
                ++num_stripes_;

                if( num_stripes_ == stripes_per_block_ )
                {
                    scramble();
                    num_stripes_ = 0;
                }
            }

            m_ = 0;
        }

        if( n > buffer_size )
        {
            while( n > 64 )
            {
                std::uint64_t stripe[ 8 ] = {};
                for( int j = 0; j < 8; ++j )
                {
                    stripe[ j ] = detail::read64le( p + ( 8 * j ) );
                }
                accumulate( stripe, 8 * num_stripes_ );
                ++num_stripes_;

                if( num_stripes_ == stripes_per_block_ )
                {
                    scramble();
                    num_stripes_ = 0;
                }

                p += 64;
                n -= 64;
            }

            detail::memcpy( buffer_ + buffer_size - 64, p - 64, 64 );

            BOOST_ASSERT( n <= 64 );
        }

        if( n > 0 )
        {
            detail::memcpy( buffer_, p, n );
            m_ = n;
        }
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        result_type r;

        if( n_ == 0 )
        {
           r = xxh3_128_digest_empty();

            // perturb state to enable result extension
            seed_ += P64_5;
        }
        else if( n_ < 4 )
        {
            r = xxh3_128_digest_1to3();
        }
        else if( n_ < 9 )
        {
            r = xxh3_128_digest_4to8();
        }
        else if( n_ < 17 )
        {
            r = xxh3_128_digest_9to16();
        }
        else if( n_ < 129 )
        {
            r = xxh3_128_digest_17to128();
        }
        else if( n_ < 241 )
        {
            r = xxh3_128_digest_129to240();
        }
        else
        {
            r = xxh3_128_digest_long();

            // perturb state to enable result extension
            acc_[ 0 ] -= P64_1;
            acc_[ 1 ] += P64_1;
            acc_[ 2 ] -= P64_2;
            acc_[ 3 ] += P64_2;
            acc_[ 4 ] -= P64_3;
            acc_[ 5 ] += P64_3;
            acc_[ 6 ] -= P64_4;
            acc_[ 7 ] += P64_4;
        }

        // finalize buffer to clear plainext and enable result extension

        {
            std::uint64_t h1 = detail::read64le( r.data() + 0 );
            std::uint64_t h2 = detail::read64le( r.data() + 8 );

            std::uint64_t v1 = detail::read64le( buffer_ + 0 );
            detail::write64le( buffer_ + 0, v1 + h1 );

            std::uint64_t v2 = detail::read64le( buffer_ + 8 );
            detail::write64le( buffer_ + 8, v2 + h2 );

            detail::memset( buffer_ + 16, 0, buffer_size - 16 );
        }

        return r;
    }
};

} // namespace hash2
} // namespace boost

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(pop)
#endif

#endif // #ifndef BOOST_HASH2_XXH3_HPP_INCLUDED
