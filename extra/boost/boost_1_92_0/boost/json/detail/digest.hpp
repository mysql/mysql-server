//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_DETAIL_DIGEST_HPP
#define BOOST_JSON_DETAIL_DIGEST_HPP

#include <boost/json/detail/config.hpp>

#include <algorithm>
#include <iterator>

namespace boost {
namespace json {
namespace detail {

// Calculate salted digest of string
template<class ForwardIterator>
std::size_t
digest(ForwardIterator b, ForwardIterator e, std::size_t salt) noexcept
{
    std::size_t const len = std::distance(b, e);

#if BOOST_JSON_ARCH == 64

    using state_type = std::uint64_t;
    state_type const m = 0xc6a4a7935bd1e995ULL;
    int const r = 47;
    state_type hash = salt ^ (len * m);

    constexpr std::size_t N = sizeof(state_type);
    e = std::next( b, len & ~std::size_t(N-1) );
    for( ; b != e; std::advance(b, N) )
    {
        state_type num;
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4996)
#endif
        std::copy_n( b, N, reinterpret_cast<unsigned char*>(&num) );
#ifdef _MSC_VER
# pragma warning(pop)
#endif

        num *= m;
        num ^= num >> r;
        num *= m;
        hash ^= num;
        hash *= m;
    }

    switch( len & (N - 1) )
    {
    case 7: hash ^= state_type( *std::next(b, 6) ) << 48; // fall through
    case 6: hash ^= state_type( *std::next(b, 5) ) << 40; // fall through
    case 5: hash ^= state_type( *std::next(b, 4) ) << 32; // fall through
    case 4: hash ^= state_type( *std::next(b, 3) ) << 24; // fall through
    case 3: hash ^= state_type( *std::next(b, 2) ) << 16; // fall through
    case 2: hash ^= state_type( *std::next(b, 1) ) << 8;  // fall through
    case 1: hash ^= state_type( *std::next(b, 0) );
            hash *= m;
    };

    hash ^= hash >> r;
    hash *= m;
    hash ^= hash >> r;

#else

    using state_type = std::uint32_t;
    state_type const m = 0x5bd1e995;
    int const r = 24;
    state_type hash = salt ^ len;

    constexpr std::size_t N = sizeof(state_type);
    e = std::next( b, len & ~std::size_t(N-1) );
    for( ; b != e; std::advance(b, N) )
    {
        state_type num;
        std::copy_n( b, N, reinterpret_cast<unsigned char*>(&num) );

        num *= m;
        num ^= num >> r;
        num *= m;
        hash *= m;
        hash ^= num;
    }

    switch( len & (N - 1) )
    {
    case 3: hash ^= state_type( *std::next(b, 2) ) << 16; // fall through
    case 2: hash ^= state_type( *std::next(b, 1) ) << 8;  // fall through
    case 1: hash ^= state_type( *std::next(b, 0) );
            hash *= m;
    };

    hash ^= hash >> 13;
    hash *= m;
    hash ^= hash >> 15;

#endif

    return hash;
}

} // detail
} // namespace json
} // namespace boost

#endif
