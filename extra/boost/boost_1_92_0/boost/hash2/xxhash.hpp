#ifndef BOOST_HASH2_XXHASH_HPP_INCLUDED
#define BOOST_HASH2_XXHASH_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// xxHash, https://cyan4973.github.io/xxHash/

#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/memset.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
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

class xxhash_32
{
private:

    static constexpr std::uint32_t P1 = 2654435761U;
    static constexpr std::uint32_t P2 = 2246822519U;
    static constexpr std::uint32_t P3 = 3266489917U;
    static constexpr std::uint32_t P4 =  668265263U;
    static constexpr std::uint32_t P5 =  374761393U;

private:

    std::uint32_t v1_ = P1 + P2;
    std::uint32_t v2_ = P2;
    std::uint32_t v3_ = 0;
    std::uint32_t v4_ = static_cast<std::uint32_t>( 0 ) - P1;

    unsigned char buffer_[ 16 ] = {};
    std::size_t m_ = 0; // == n_ % 16

    std::size_t n_ = 0;

private:

    BOOST_CXX14_CONSTEXPR void init( std::uint32_t seed )
    {
        v1_ = seed + P1 + P2;
        v2_ = seed + P2;
        v3_ = seed;
        v4_ = seed - P1;
    }

    BOOST_CXX14_CONSTEXPR static std::uint32_t round( std::uint32_t seed, std::uint32_t input )
    {
        seed += input * P2;
        seed = detail::rotl( seed, 13 );
        seed *= P1;
        return seed;
    }

    BOOST_CXX14_CONSTEXPR void update_( unsigned char const * p, std::size_t k )
    {
        std::uint32_t v1 = v1_;
        std::uint32_t v2 = v2_;
        std::uint32_t v3 = v3_;
        std::uint32_t v4 = v4_;

        for( std::size_t i = 0; i < k; ++i, p += 16 )
        {
            v1 = round( v1, detail::read32le( p +  0 ) );
            v2 = round( v2, detail::read32le( p +  4 ) );
            v3 = round( v3, detail::read32le( p +  8 ) );
            v4 = round( v4, detail::read32le( p + 12 ) );
        }

        v1_ = v1; 
        v2_ = v2; 
        v3_ = v3; 
        v4_ = v4; 
    }

public:

    using result_type = std::uint32_t;

    xxhash_32() = default;

    BOOST_CXX14_CONSTEXPR explicit xxhash_32( std::uint64_t seed )
    {
        std::uint32_t s0 = static_cast<std::uint32_t>( seed );
        std::uint32_t s1 = static_cast<std::uint32_t>( seed >> 32 );

        init( s0 );

        if( s1 != 0 )
        {
            v1_ = round( v1_, s1 );
            v2_ = round( v2_, s1 );
            v3_ = round( v3_, s1 );
            v4_ = round( v4_, s1 );
        }
    }

    BOOST_CXX14_CONSTEXPR xxhash_32( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    xxhash_32( void const * p, std::size_t n ): xxhash_32( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        BOOST_ASSERT( m_ == n_ % 16 );

        if( n == 0 ) return;

        n_ += n;

        if( m_ > 0 )
        {
            std::size_t k = 16 - m_;

            if( n < k )
            {
                k = n;
            }

            detail::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;
            m_ += k;

            if( m_ < 16 ) return;

            BOOST_ASSERT( m_ == 16 );

            update_( buffer_, 1 );
            m_ = 0;
        }

        BOOST_ASSERT( m_ == 0 );

        {
            std::size_t k = n / 16;

            update_( p, k );

            p += 16 * k;
            n -= 16 * k;
        }

        BOOST_ASSERT( n < 16 );

        if( n > 0 )
        {
            detail::memcpy( buffer_, p, n );
            m_ = n;
        }

        BOOST_ASSERT( m_ == n_ % 16 );
    }

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR std::uint32_t result()
    {
        BOOST_ASSERT( m_ == n_ % 16 );

        std::uint32_t h = 0;

        if( n_ >= 16 )
        {
            h = detail::rotl( v1_, 1 ) + detail::rotl( v2_, 7 ) + detail::rotl( v3_, 12 ) + detail::rotl( v4_, 18 );
        }
        else
        {
            h = v3_ + P5;
        }

        h += static_cast<std::uint32_t>( n_ );

        unsigned char const * p = buffer_;

        std::size_t m = m_;

        while( m >= 4 )
        {
            h += detail::read32le( p ) * P3;
            h = detail::rotl( h, 17 ) * P4;

            p += 4;
            m -= 4;
        }

        while( m > 0 )
        {
            h += p[0] * P5;
            h = detail::rotl( h, 11 ) * P1;

            ++p;
            --m;
        }

        n_ += 16 - m_;
        m_ = 0;

        // clear buffered plaintext
        detail::memset( buffer_, 0, 16 );

        // perturb state
        v1_ += h;
        v2_ += h;
        v3_ -= h;
        v4_ -= h;

        // apply final mix
        h ^= h >> 15;
        h *= P2;
        h ^= h >> 13;
        h *= P3;
        h ^= h >> 16;

        return h;
    }
};

class xxhash_64
{
private:

    static constexpr std::uint64_t P1 = 11400714785074694791ULL;
    static constexpr std::uint64_t P2 = 14029467366897019727ULL;
    static constexpr std::uint64_t P3 =  1609587929392839161ULL;
    static constexpr std::uint64_t P4 =  9650029242287828579ULL;
    static constexpr std::uint64_t P5 =  2870177450012600261ULL;

private:

    std::uint64_t v1_ = P1 + P2;
    std::uint64_t v2_ = P2;
    std::uint64_t v3_ = 0;
    std::uint64_t v4_ = static_cast<std::uint64_t>( 0 ) - P1;

    unsigned char buffer_[ 32 ] = {};
    std::size_t m_ = 0; // == n_ % 32

    std::uint64_t n_ = 0;

private:

    BOOST_CXX14_CONSTEXPR void init( std::uint64_t seed )
    {
        v1_ = seed + P1 + P2;
        v2_ = seed + P2;
        v3_ = seed;
        v4_ = seed - P1;
    }

    BOOST_CXX14_CONSTEXPR static std::uint64_t round( std::uint64_t seed, std::uint64_t input )
    {
        seed += input * P2;
        seed = detail::rotl( seed, 31 );
        seed *= P1;
        return seed;
    }

    BOOST_CXX14_CONSTEXPR static std::uint64_t merge_round( std::uint64_t acc, std::uint64_t val )
    {
        val = round( 0, val );
        acc ^= val;
        acc = acc * P1 + P4;
        return acc;
    }

    BOOST_CXX14_CONSTEXPR void update_( unsigned char const * p, std::size_t k )
    {
        std::uint64_t v1 = v1_;
        std::uint64_t v2 = v2_;
        std::uint64_t v3 = v3_;
        std::uint64_t v4 = v4_;

        for( std::size_t i = 0; i < k; ++i, p += 32 )
        {
            v1 = round( v1, detail::read64le( p +  0 ) );
            v2 = round( v2, detail::read64le( p +  8 ) );
            v3 = round( v3, detail::read64le( p + 16 ) );
            v4 = round( v4, detail::read64le( p + 24 ) );
        }

        v1_ = v1; 
        v2_ = v2; 
        v3_ = v3; 
        v4_ = v4; 
    }

public:

    typedef std::uint64_t result_type;

    xxhash_64() = default;

    BOOST_CXX14_CONSTEXPR explicit xxhash_64( std::uint64_t seed )
    {
        init( seed );
    }

    BOOST_CXX14_CONSTEXPR xxhash_64( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
        }
    }

    xxhash_64( void const * p, std::size_t n ): xxhash_64( static_cast<unsigned char const*>( p ), n )
    {
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        BOOST_ASSERT( m_ == n_ % 32 );

        if( n == 0 ) return;

        n_ += n;

        if( m_ > 0 )
        {
            std::size_t k = 32 - m_;

            if( n < k )
            {
                k = n;
            }

            detail::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;
            m_ += k;

            if( m_ < 32 ) return;

            BOOST_ASSERT( m_ == 32 );

            update_( buffer_, 1 );
            m_ = 0;
        }

        BOOST_ASSERT( m_ == 0 );

        {
            std::size_t k = n / 32;

            update_( p, k );

            p += 32 * k;
            n -= 32 * k;
        }

        BOOST_ASSERT( n < 32 );

        if( n > 0 )
        {
            detail::memcpy( buffer_, p, n );
            m_ = n;
        }

        BOOST_ASSERT( m_ == n_ % 32 );
    }

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_CXX14_CONSTEXPR std::uint64_t result()
    {
        BOOST_ASSERT( m_ == n_ % 32 );

        std::uint64_t h = 0;

        if( n_ >= 32 )
        {
            h = detail::rotl( v1_, 1 ) + detail::rotl( v2_, 7 ) + detail::rotl( v3_, 12 ) + detail::rotl( v4_, 18 );

            h = merge_round( h, v1_ );
            h = merge_round( h, v2_ );
            h = merge_round( h, v3_ );
            h = merge_round( h, v4_ );
        }
        else
        {
            h = v3_ + P5;
        }

        h += n_;

        unsigned char const * p = buffer_;

        std::size_t m = m_;

        while( m >= 8 )
        {
            std::uint64_t k1 = round( 0, detail::read64le( p ) );

            h ^= k1;
            h = detail::rotl( h, 27 ) * P1 + P4;

            p += 8;
            m -= 8;
        }

        while( m >= 4 )
        {
            h ^= static_cast<std::uint64_t>( detail::read32le( p ) ) * P1;
            h = detail::rotl( h, 23 ) * P2 + P3;

            p += 4;
            m -= 4;
        }

        while( m > 0 )
        {
            h ^= p[0] * P5;
            h = detail::rotl( h, 11 ) * P1;

            ++p;
            --m;
        }

        n_ += 32 - m_;
        m_ = 0;

        // clear buffered plaintext
        detail::memset( buffer_, 0, 32 );

        // perturb state
        v1_ += h;
        v2_ += h;
        v3_ -= h;
        v4_ -= h;

        // apply final mix
        h ^= h >> 33;
        h *= P2;
        h ^= h >> 29;
        h *= P3;
        h ^= h >> 32;

        return h;
    }
};

} // namespace hash2
} // namespace boost

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(pop)
#endif

#endif // #ifndef BOOST_HASH2_XXHASH_HPP_INCLUDED
