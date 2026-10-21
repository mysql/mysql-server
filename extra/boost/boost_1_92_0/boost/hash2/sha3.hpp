#ifndef BOOST_HASH2_SHA3_HPP_INCLUDED
#define BOOST_HASH2_SHA3_HPP_INCLUDED

// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

// https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf
// https://github.com/XKCP/XKCP/blob/master/Standalone/CompactFIPS202/C/Keccak-readable-and-compact.c
// https://keccak.team/files/Keccak-reference-3.0.pdf

#include <boost/hash2/digest.hpp>
#include <boost/hash2/hmac.hpp>
#include <boost/hash2/detail/keccak.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <cstdint>

namespace boost
{
namespace hash2
{
namespace detail
{

template<std::uint8_t PaddingDelim, int C, int D>
struct keccak_base
{
private:

    static constexpr int R = 1600 - C;

    unsigned char state_[ 200 ] = {};
    std::size_t m_ = 0;

    bool finalized_ = false;

public:

    using result_type = digest<D / 8>;
    static constexpr std::size_t block_size = R / 8;

    void update( void const* pv, std::size_t n )
    {
        unsigned char const* p = static_cast<unsigned char const*>( pv );
        update( p, n );
    }

    BOOST_HASH2_SHA3_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        finalized_ = false;

        auto const block_len = R / 8;

        if( m_ > 0 )
        {
            std::size_t k = block_len - m_;

            if( n < k )
            {
                k = n;
            }

            for( std::size_t i = 0; i < k; ++i )
            {
                state_[ m_ + i ] ^= p[ i ];
            }

            p += k;
            n -= k;
            m_ += k;

            if( m_ < block_len ) return;

            BOOST_ASSERT( m_ == block_len );

            keccak_permute( state_ );
            m_ = 0;
        }

        while( n >= block_len )
        {
            for( int i = 0; i < block_len; ++i )
            {
                state_[ i ] ^= p[ i ];
            }

            keccak_permute( state_ );

            p += block_len;
            n -= block_len;
        }

        BOOST_ASSERT( n < block_len );

        if( n > 0 )
        {
            for( std::size_t i = 0; i < n; ++i )
            {
                state_[ i ] ^= p[ i ];
            }

            m_ = n;
        }

        BOOST_ASSERT( m_ == n % block_len );
    }

    BOOST_HASH2_SHA3_CONSTEXPR result_type result()
    {
        result_type digest;

        if( !finalized_ )
        {
            state_[ m_ ] ^= PaddingDelim;
            state_[ R / 8 - 1 ] ^= 0x80;

            m_ = 0;
            finalized_ = true;
        }

        keccak_permute( state_ );
        detail::memcpy( digest.data(), state_, digest.size() );

        return digest;
    }
};

#if defined(BOOST_NO_CXX17_INLINE_VARIABLES)

template<std::uint8_t PaddingDelim, int C, int D>
constexpr std::size_t keccak_base<PaddingDelim, C, D>::block_size;

#endif

} // namespace detail

class sha3_224: public detail::keccak_base<0x06, 2 * 224, 224>
{
public:

    BOOST_HASH2_SHA3_CONSTEXPR sha3_224()
    {
    }

    BOOST_HASH2_SHA3_CONSTEXPR explicit sha3_224( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};

            detail::write64le( tmp, seed );
            update( tmp, 8 );

            result();
            update( tmp, 0 ); // sets finalized_ to false
        }
    }

    BOOST_HASH2_SHA3_CONSTEXPR sha3_224( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
            update( p, 0 ); // sets finalized_ to false
        }
    }

    sha3_224( void const * p, std::size_t n ): sha3_224( static_cast<unsigned char const*>( p ), n )
    {
    }
};

class sha3_256: public detail::keccak_base<0x06, 2 * 256, 256>
{
public:

    BOOST_HASH2_SHA3_CONSTEXPR sha3_256()
    {
    }

    BOOST_HASH2_SHA3_CONSTEXPR explicit sha3_256( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};

            detail::write64le( tmp, seed );
            update( tmp, 8 );

            result();
            update( tmp, 0 ); // sets finalized_ to false
        }
    }

    BOOST_HASH2_SHA3_CONSTEXPR sha3_256( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
            update( p, 0 ); // sets finalized_ to false
        }
    }

    sha3_256( void const * p, std::size_t n ): sha3_256( static_cast<unsigned char const*>( p ), n )
    {
    }
};

class sha3_384: public detail::keccak_base<0x06, 2 * 384, 384>
{
public:

    BOOST_HASH2_SHA3_CONSTEXPR sha3_384()
    {
    }

    BOOST_HASH2_SHA3_CONSTEXPR explicit sha3_384( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};

            detail::write64le( tmp, seed );
            update( tmp, 8 );

            result();
            update( tmp, 0 ); // sets finalized_ to false
        }
    }

    BOOST_HASH2_SHA3_CONSTEXPR sha3_384( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
            update( p, 0 ); // sets finalized_ to false
        }
    }

    sha3_384( void const * p, std::size_t n ): sha3_384( static_cast<unsigned char const*>( p ), n )
    {
    }
};

class sha3_512: public detail::keccak_base<0x06, 2 * 512, 512>
{
public:

    BOOST_HASH2_SHA3_CONSTEXPR sha3_512()
    {
    }

    BOOST_HASH2_SHA3_CONSTEXPR explicit sha3_512( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};

            detail::write64le( tmp, seed );
            update( tmp, 8 );

            result();
            update( tmp, 0 ); // sets finalized_ to false
        }
    }

    BOOST_HASH2_SHA3_CONSTEXPR sha3_512( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
            update( p, 0 ); // sets finalized_ to false
        }
    }

    sha3_512( void const * p, std::size_t n ): sha3_512( static_cast<unsigned char const*>( p ), n )
    {
    }
};

class shake_128: public detail::keccak_base<0x1f, 256, 1600 - 256>
{
public:

    BOOST_HASH2_SHA3_CONSTEXPR shake_128()
    {
    }

    BOOST_HASH2_SHA3_CONSTEXPR explicit shake_128( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};

            detail::write64le( tmp, seed );
            update( tmp, 8 );

            result();
            update( tmp, 0 ); // sets finalized_ to false
        }
    }

    BOOST_HASH2_SHA3_CONSTEXPR shake_128( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
            update( p, 0 ); // sets finalized_ to false
        }
    }

    shake_128( void const * p, std::size_t n ): shake_128( static_cast<unsigned char const*>( p ), n )
    {
    }
};

class shake_256: public detail::keccak_base<0x1f, 512, 1600 - 512>
{
public:

    BOOST_HASH2_SHA3_CONSTEXPR shake_256()
    {
    }

    BOOST_HASH2_SHA3_CONSTEXPR explicit shake_256( std::uint64_t seed )
    {
        if( seed != 0 )
        {
            unsigned char tmp[ 8 ] = {};

            detail::write64le( tmp, seed );
            update( tmp, 8 );

            result();
            update( tmp, 0 ); // sets finalized_ to false
        }
    }

    BOOST_HASH2_SHA3_CONSTEXPR shake_256( unsigned char const * p, std::size_t n )
    {
        if( n != 0 )
        {
            update( p, n );
            result();
            update( p, 0 ); // sets finalized_ to false
        }
    }

    shake_256( void const * p, std::size_t n ): shake_256( static_cast<unsigned char const*>( p ), n )
    {
    }
};

using hmac_sha3_224 = hmac<sha3_224>;
using hmac_sha3_256 = hmac<sha3_256>;
using hmac_sha3_384 = hmac<sha3_384>;
using hmac_sha3_512 = hmac<sha3_512>;

} // namespace hash2
} // namespace boost

#endif // BOOST_HASH2_SHA3_HPP_INCLUDED
