#ifndef BOOST_HASH2_DETAIL_KECCAK_HPP_INCLUDED
#define BOOST_HASH2_DETAIL_KECCAK_HPP_INCLUDED

// Copyright 2025 Christian Mazakas
// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/read.hpp>
#include <boost/hash2/detail/rot.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#include <cstdint>

#if BOOST_WORKAROUND(BOOST_GCC, >= 50000 && BOOST_GCC < 60000)
#define BOOST_HASH2_SHA3_CONSTEXPR
#else
#define BOOST_HASH2_SHA3_CONSTEXPR BOOST_CXX14_CONSTEXPR
#endif

namespace boost
{
namespace hash2
{
namespace detail
{

BOOST_FORCEINLINE BOOST_HASH2_SHA3_CONSTEXPR std::uint64_t read_lane( unsigned char const (&state)[ 200 ], int x, int y )
{
    return detail::read64le( state + ( x + 5 * y ) * 8 );
}

BOOST_FORCEINLINE BOOST_HASH2_SHA3_CONSTEXPR void write_lane( unsigned char (&state)[ 200 ], int x, int y, std::uint64_t v )
{
    detail::write64le( state + ( x + 5 * y ) * 8, v );
}

BOOST_FORCEINLINE BOOST_HASH2_SHA3_CONSTEXPR void xor_lane( unsigned char (&state)[ 200 ], int x, int y, std::uint64_t v )
{
    write_lane( state, x, y, read_lane( state, x, y ) ^ v );
}

BOOST_FORCEINLINE BOOST_HASH2_SHA3_CONSTEXPR void rho_and_pi_round( unsigned char (&state)[ 200 ], std::uint64_t& lane, int rot, int x, int y )
{
    std::uint64_t temp = read_lane( state, x, y );
    write_lane( state, x, y, detail::rotl( lane, rot ) );
    lane = temp;
}

inline BOOST_HASH2_SHA3_CONSTEXPR void keccak_round( unsigned char (&state)[ 200 ] )
{
    {
        // theta

        std::uint64_t const C1[ 5 ] =
        {
            // state[ x ] ^ state[ x + 5 ] ^ state[ x + 10 ] ^ state[ x + 15 ] ^ state[ x + 20 ]

            read_lane( state, 0, 0 ) ^ read_lane( state, 0, 1 ) ^ read_lane( state, 0, 2 ) ^ read_lane( state, 0, 3 ) ^ read_lane( state, 0, 4 ),
            read_lane( state, 1, 0 ) ^ read_lane( state, 1, 1 ) ^ read_lane( state, 1, 2 ) ^ read_lane( state, 1, 3 ) ^ read_lane( state, 1, 4 ),
            read_lane( state, 2, 0 ) ^ read_lane( state, 2, 1 ) ^ read_lane( state, 2, 2 ) ^ read_lane( state, 2, 3 ) ^ read_lane( state, 2, 4 ),
            read_lane( state, 3, 0 ) ^ read_lane( state, 3, 1 ) ^ read_lane( state, 3, 2 ) ^ read_lane( state, 3, 3 ) ^ read_lane( state, 3, 4 ),
            read_lane( state, 4, 0 ) ^ read_lane( state, 4, 1 ) ^ read_lane( state, 4, 2 ) ^ read_lane( state, 4, 3 ) ^ read_lane( state, 4, 4 ),
        };

        std::uint64_t const C2[ 5 ] =
        {
            // detail::rotl( C1[ x ], 1 )

            detail::rotl( C1[ 0 ], 1 ),
            detail::rotl( C1[ 1 ], 1 ),
            detail::rotl( C1[ 2 ], 1 ),
            detail::rotl( C1[ 3 ], 1 ),
            detail::rotl( C1[ 4 ], 1 ),
        };

        for( int y = 0; y < 5; ++y )
        {
            // for( int x = 0; x < 5; ++x )
            // {
            //     state[ 5 * y + x ] ^= C1[ ( x + 4 ) % 5] ^ C2[ ( x + 1 ) % 5 ];
            // }

            xor_lane( state, 0, y, C1[ 4 ] ^ C2[ 1 ] );
            xor_lane( state, 1, y, C1[ 0 ] ^ C2[ 2 ] );
            xor_lane( state, 2, y, C1[ 1 ] ^ C2[ 3 ] );
            xor_lane( state, 3, y, C1[ 2 ] ^ C2[ 4 ] );
            xor_lane( state, 4, y, C1[ 3 ] ^ C2[ 0 ] );
        }
    }

    {
        // rho and pi fused

        std::uint64_t lane = read_lane( state, 1, 0 );

        rho_and_pi_round( state, lane,  1, 0, 2 );
        rho_and_pi_round( state, lane,  3, 2, 1 );
        rho_and_pi_round( state, lane,  6, 1, 2 );
        rho_and_pi_round( state, lane, 10, 2, 3 );
        rho_and_pi_round( state, lane, 15, 3, 3 );
        rho_and_pi_round( state, lane, 21, 3, 0 );
        rho_and_pi_round( state, lane, 28, 0, 1 );
        rho_and_pi_round( state, lane, 36, 1, 3 );
        rho_and_pi_round( state, lane, 45, 3, 1 );
        rho_and_pi_round( state, lane, 55, 1, 4 );
        rho_and_pi_round( state, lane,  2, 4, 4 );
        rho_and_pi_round( state, lane, 14, 4, 0 );
        rho_and_pi_round( state, lane, 27, 0, 3 );
        rho_and_pi_round( state, lane, 41, 3, 4 );
        rho_and_pi_round( state, lane, 56, 4, 3 );
        rho_and_pi_round( state, lane,  8, 3, 2 );
        rho_and_pi_round( state, lane, 25, 2, 2 );
        rho_and_pi_round( state, lane, 43, 2, 0 );
        rho_and_pi_round( state, lane, 62, 0, 4 );
        rho_and_pi_round( state, lane, 18, 4, 2 );
        rho_and_pi_round( state, lane, 39, 2, 4 );
        rho_and_pi_round( state, lane, 61, 4, 1 );
        rho_and_pi_round( state, lane, 20, 1, 1 );
        rho_and_pi_round( state, lane, 44, 1, 0 );
    }

    {
        // chi

        for( int y = 0; y < 5; ++y )
        {
            std::uint64_t const plane[ 5 ] =
            {
                read_lane( state, 0, y ),
                read_lane( state, 1, y ),
                read_lane( state, 2, y ),
                read_lane( state, 3, y ),
                read_lane( state, 4, y ),
            };

            // state[ 5 * y + x ] = plane[ x ] ^ ( ( ~plane[ ( x + 1 ) % 5 ] ) & plane[ ( x + 2 ) % 5 ] );

            write_lane( state, 0, y, plane[ 0 ] ^ ( ~plane[ 1 ] & plane[ 2 ] ) );
            write_lane( state, 1, y, plane[ 1 ] ^ ( ~plane[ 2 ] & plane[ 3 ] ) );
            write_lane( state, 2, y, plane[ 2 ] ^ ( ~plane[ 3 ] & plane[ 4 ] ) );
            write_lane( state, 3, y, plane[ 3 ] ^ ( ~plane[ 4 ] & plane[ 0 ] ) );
            write_lane( state, 4, y, plane[ 4 ] ^ ( ~plane[ 0 ] & plane[ 1 ] ) );
        }
    }
}

template<class = void> struct iota_rc_holder
{
    static constexpr std::uint64_t data[ 24 ] =
    {
        0x0000000000000001ull, 0x0000000000008082ull, 0x800000000000808aull,
        0x8000000080008000ull, 0x000000000000808bull, 0x0000000080000001ull,
        0x8000000080008081ull, 0x8000000000008009ull, 0x000000000000008aull,
        0x0000000000000088ull, 0x0000000080008009ull, 0x000000008000000aull,
        0x000000008000808bull, 0x800000000000008bull, 0x8000000000008089ull,
        0x8000000000008003ull, 0x8000000000008002ull, 0x8000000000000080ull,
        0x000000000000800aull, 0x800000008000000aull, 0x8000000080008081ull,
        0x8000000000008080ull, 0x0000000080000001ull, 0x8000000080008008ull,
    };
};

#if defined(BOOST_NO_CXX17_INLINE_VARIABLES)

template<class T> constexpr std::uint64_t iota_rc_holder<T>::data[ 24 ];

#endif

inline BOOST_HASH2_SHA3_CONSTEXPR void keccak_permute( unsigned char (&state)[ 200 ] )
{
    for( int i = 0; i < 24; ++i )
    {
        keccak_round( state );
        xor_lane( state, 0, 0, iota_rc_holder<>::data[ i ] );
    }
}

} // namespace detail
} // namespace hash2
} // namespace boost


#endif // BOOST_HASH2_DETAIL_KECCAK_HPP_INCLUDED
