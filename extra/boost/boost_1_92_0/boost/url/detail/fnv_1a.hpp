//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_FNV_1A_HPP
#define BOOST_URL_DETAIL_FNV_1A_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <cstddef>

namespace boost {
namespace urls {
namespace detail {

class fnv_1a
{
public:
    using digest_type = std::size_t;

#if BOOST_URL_ARCH == 64
    static constexpr std::size_t const prime =
        static_cast<std::size_t>(0x100000001B3ULL);
    static constexpr std::size_t init_hash  =
        static_cast<std::size_t>(0xcbf29ce484222325ULL);
#else
    static constexpr std::size_t const prime =
        static_cast<std::size_t>(0x01000193UL);
    static constexpr std::size_t init_hash  =
        static_cast<std::size_t>(0x811C9DC5UL);
#endif

    explicit
    fnv_1a(std::size_t salt) noexcept
    : h_(init_hash + salt)
    {
    }

    void
    put(char c) noexcept
    {
        h_ ^= c;
        h_ *= prime;
    }

    void
    put(core::string_view s) noexcept
    {
        for (char c: s)
        {
            put(c);
        }
    }

    digest_type
    digest() const noexcept
    {
        return h_;
    }

private:
    std::size_t h_;
};

} // detail
} // urls
} // boost

#endif
