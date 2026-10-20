#ifndef BOOST_HASH2_MURMUR3_HPP_INCLUDED
#define BOOST_HASH2_MURMUR3_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// MurmurHash3, https://github.com/aappleby/smhasher/wiki/MurmurHash3

#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/assert.hpp>
#include <cstdint>
#include <array>
#include <cstring>
#include <cstddef>

namespace boost
{
namespace hash2
{

class murmur3_32
{
private:

    std::uint32_t h_;

    unsigned char buffer_[ 4 ];
    std::size_t m_; // == n_ % 4

    std::size_t n_;

private:

    static const std::uint32_t c1 = 0xcc9e2d51u;
    static const std::uint32_t c2 = 0x1b873593u;

private:

    static BOOST_FORCEINLINE void mix( std::uint32_t & h, std::uint32_t k )
    {
        k *= c1;
        k = detail::rotl( k, 15 );
        k *= c2;

        h ^= k;
        h = detail::rotl( h, 13 );
        h = h * 5 + 0xe6546b64;
    }

    void update_( unsigned char const * p, std::size_t m )
    {
        std::uint32_t h = h_;

        for( std::size_t i = 0; i < m; ++i, p += 4 )
        {
            std::uint32_t k = detail::read32le( p );
            mix( h, k );
        }

        h_ = h;
    }

public:

    typedef std::uint32_t result_type;
    typedef std::uint32_t size_type;

    explicit murmur3_32( std::uint64_t seed = 0 ): m_( 0 ), n_( 0 )
    {
        h_ = static_cast<std::uint32_t>( seed );

        std::uint32_t k = static_cast<std::uint32_t>( seed >> 32 );

        if( k != 0 )
        {
            mix( h_, k );
        }
    }

    murmur3_32( unsigned char const * p, std::size_t n ): m_( 0 ), n_( 0 )
    {
        if( n == 0 )
        {
            h_ = 0;
        }
        else if( n <= 4 )
        {
            unsigned char q[ 4 ] = {};
            std::memcpy( q, p, n );

            h_ = detail::read32le( q );
        }
        else
        {
            h_ = detail::read32le( p );

            p += 4;
            n -= 4;

            update( p, n );
            result();
        }
    }

    void update( void const * pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );

        BOOST_ASSERT( m_ == n_ % 4 );

        if( n == 0 ) return;

        n_ += n;

        if( m_ > 0 )
        {
            std::size_t k = 4 - m_;

            if( n < k )
            {
                k = n;
            }

            std::memcpy( buffer_ + m_, p, k );

            p += k;
            n -= k;
            m_ += k;

            if( m_ < 4 ) return;

            BOOST_ASSERT( m_ == 4 );

            update_( buffer_, 1 );
            m_ = 0;
        }

        BOOST_ASSERT( m_ == 0 );

        {
            std::size_t k = n / 4;

            update_( p, k );

            p += 4 * k;
            n -= 4 * k;
        }

        BOOST_ASSERT( n < 4 );

        if( n > 0 )
        {
            std::memcpy( buffer_, p, n );
            m_ = n;
        }

        BOOST_ASSERT( m_ == n_ % 4 );
    }

    std::uint32_t result()
    {
        BOOST_ASSERT( m_ == n_ % 4 );

        // std::memset( buffer_ + m_, 0, 4 - m_ );
        // std::uint32_t k = detail::read32le( buffer_ );

        std::uint32_t k = 0;

        switch( m_ )
        {
        case 1:

            k = buffer_[0];
            break;

        case 2:

            k = buffer_[0] + (buffer_[1] << 8);
            break;

        case 3:

            k = buffer_[0] + (buffer_[1] << 8) + (buffer_[2] << 16);
            break;
        }

        k *= c1;
        k = detail::rotl( k, 15 );
        k *= c2;

        std::uint32_t h = h_;

        h ^= k;
        h ^= static_cast<std::uint32_t>( n_ );

        h ^= h >> 16; 
        h *= 0x85ebca6b; 
        h ^= h >> 13; 
        h *= 0xc2b2ae35; 
        h ^= h >> 16; 

        n_ += 4 - m_;
        m_ = 0;

        // clear buffered plaintext
        std::memset( buffer_, 0, 4 );

        return h;
    }
};

class murmur3_128
{
private:

    std::uint64_t h1_, h2_;

    unsigned char buffer_[ 16 ];
    std::size_t m_; // == n_ % 16

    std::size_t n_;

private:

    static const std::uint64_t c1 = 0x87c37b91114253d5ull;
    static const std::uint64_t c2 = 0x4cf5ad432745937full;

private:

    void update_( unsigned char const * p, std::size_t k )
    {
        std::uint64_t h1 = h1_, h2 = h2_;

        for( std::size_t i = 0; i < k; ++i, p += 16 )
        {
            std::uint64_t k1 = detail::read64le( p + 0 );
            std::uint64_t k2 = detail::read64le( p + 8 );

            k1 *= c1; k1 = detail::rotl( k1, 31 ); k1 *= c2; h1 ^= k1;

            h1 = detail::rotl( h1, 27 ); h1 += h2; h1 = h1 * 5 + 0x52dce729;

            k2 *= c2; k2 = detail::rotl( k2, 33 ); k2 *= c1; h2 ^= k2;

            h2 = detail::rotl( h2, 31 ); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
        }

        h1_ = h1; h2_ = h2;
    }

    static std::uint64_t fmix( std::uint64_t k )
    { 
        k ^= k >> 33; 
        k *= 0xff51afd7ed558ccdull;
        k ^= k >> 33; 
        k *= 0xc4ceb9fe1a85ec53ull;
        k ^= k >> 33; 

        return k; 
    } 

public:

    typedef std::array<unsigned char, 16> result_type;
    typedef std::uint64_t size_type;

    explicit murmur3_128( std::uint64_t seed = 0 ): m_( 0 ), n_( 0 )
    {
        h1_ = seed;
        h2_ = seed;
    }

    murmur3_128( std::uint64_t seed1, std::uint64_t seed2 ): m_( 0 ), n_( 0 )
    {
        h1_ = seed1;
        h2_ = seed2;
    }

    murmur3_128( unsigned char const * p, std::size_t n ): m_( 0 ), n_( 0 )
    {
        if( n == 0 )
        {
            h1_ = h2_ = 0;
        }
        else if( n <= 8 )
        {
            unsigned char q[ 8 ] = {};
            std::memcpy( q, p, n );

            h1_ = h2_ = detail::read64le( q );
        }
        else if( n <= 16 )
        {
            unsigned char q[ 18 ] = {};
            std::memcpy( q, p, n );

            h1_ = detail::read64le( q + 0 );
            h2_ = detail::read64le( q + 8 );
        }
        else
        {
            h1_ = detail::read64le( p + 0 );
            h2_ = detail::read64le( p + 8 );

            p += 16;
            n -= 16;

            update( p, n );
            result();
        }
    }

    void update( void const * pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );

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

            std::memcpy( buffer_ + m_, p, k );

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
            std::memcpy( buffer_, p, n );
            m_ = n;
        }

        BOOST_ASSERT( m_ == n_ % 16 );
    }

    result_type result()
    {
        BOOST_ASSERT( m_ == n_ % 16 );

        std::memset( buffer_ + m_, 0, 16 - m_ );

        std::uint64_t h1 = h1_, h2 = h2_;

        std::uint64_t k1 = detail::read64le( buffer_ + 0 );
        std::uint64_t k2 = detail::read64le( buffer_ + 8 );

        k1 *= c1; k1 = detail::rotl( k1, 31 ); k1 *= c2; h1 ^= k1;
        k2 *= c2; k2 = detail::rotl( k2, 33 ); k2 *= c1; h2 ^= k2;

        h1 ^= n_;
        h2 ^= n_;

        h1 += h2;
        h2 += h1;

        h1 = fmix( h1 );
        h2 = fmix( h2 );

        h1 += h2;
        h2 += h1;

        n_ += 16 - m_;
        m_ = 0;

        // clear buffered plaintext
        std::memset( buffer_, 0, 16 );

        result_type r;

        detail::write64le( &r[ 0 ], h1 );
        detail::write64le( &r[ 8 ], h2 );

        return r;
    }
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_MURMUR3_HPP_INCLUDED
