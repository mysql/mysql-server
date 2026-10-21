#ifndef BOOST_HASH2_HMAC_HPP_INCLUDED
#define BOOST_HASH2_HMAC_HPP_INCLUDED

// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// HMAC message authentication algorithm, https://tools.ietf.org/html/rfc2104

#include <boost/hash2/detail/write.hpp>
#include <boost/hash2/detail/memcpy.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <cstdint>
#include <cstring>
#include <cstddef>

#if !defined(BOOST_NO_CXX14_CONSTEXPR) && !BOOST_WORKAROUND(BOOST_GCC, < 60000)

# define BOOST_HASH2_HMAC_CONSTEXPR constexpr

#else

// libs/hash2/test/concept.cpp:45:7:   in constexpr expansion of 'h1.boost::hash2::hmac<H>::hmac<boost::hash2::md5_128>()'
// ./boost/hash2/hmac.hpp:80:13:   in constexpr expansion of 'boost::hash2::hmac<H>::init<boost::hash2::md5_128>(0u, 0u)'
// libs/hash2/test/concept.cpp:45:7: internal compiler error: in fold_binary_loc, at fold-const.c:9925

# define BOOST_HASH2_HMAC_CONSTEXPR

#endif

namespace boost
{
namespace hash2
{

template<class H> class hmac
{
public:

    using result_type = typename H::result_type;
    static constexpr std::size_t block_size = H::block_size;

private:

    H outer_;
    H inner_;

private:

    BOOST_HASH2_HMAC_CONSTEXPR void init( unsigned char const* p, std::size_t n )
    {
        constexpr std::size_t m = block_size;

        unsigned char key[ m ] = {};

        if( n == 0 )
        {
            // memcpy from (NULL, 0) is undefined
        }
        else if( n <= m )
        {
            detail::memcpy( key, p, n );
        }
        else
        {
            H h;
            h.update( p, n );

            result_type r = h.result();

            detail::memcpy( key, &r[0], m < r.size()? m: r.size() );
        }

        for( std::size_t i = 0; i < m; ++i )
        {
            key[ i ] = static_cast<unsigned char>( key[ i ] ^ 0x36 );
        }

        inner_.update( key, m );

        for( std::size_t i = 0; i < m; ++i )
        {
            key[ i ] = static_cast<unsigned char>( key[ i ] ^ 0x36 ^ 0x5C );
        }

        outer_.update( key, m );
    }

public:

    BOOST_HASH2_HMAC_CONSTEXPR hmac()
    {
        init( 0, 0 );
    }

    explicit BOOST_HASH2_HMAC_CONSTEXPR hmac( std::uint64_t seed )
    {
        if( seed == 0 )
        {
            init( 0, 0 );
        }
        else
        {
            unsigned char tmp[ 8 ] = {};
            detail::write64le( tmp, seed );

            init( tmp, 8 );
        }
    }

    BOOST_HASH2_HMAC_CONSTEXPR hmac( unsigned char const* p, std::size_t n )
    {
        init( p, n );
    }

    hmac( void const* p, std::size_t n )
    {
        init( static_cast<unsigned char const*>( p ), n );
    }

    BOOST_CXX14_CONSTEXPR void update( unsigned char const* p, std::size_t n )
    {
        inner_.update( p, n );
    }

    void update( void const* pv, std::size_t n )
    {
        inner_.update( pv, n );
    }

    BOOST_CXX14_CONSTEXPR result_type result()
    {
        result_type r = inner_.result();

        outer_.update( &r[0], r.size() );

        return outer_.result();
    }
};

} // namespace hash2
} // namespace boost

#endif // #ifndef BOOST_HASH2_HMAC_HPP_INCLUDED
