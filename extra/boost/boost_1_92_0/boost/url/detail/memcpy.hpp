//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_MEMCPY_HPP
#define BOOST_URL_DETAIL_MEMCPY_HPP

#include <boost/url/detail/config.hpp>
#include <cstddef>
#include <cstring>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX14_CONSTEXPR_OR_FORCEINLINE
void
memcpy(
    unsigned char* dest,
    unsigned char const* src,
    std::size_t n) noexcept
{
#if defined(BOOST_URL_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
    if (!__builtin_is_constant_evaluated())
    {
        std::memcpy(dest, src, n);
    }
    else
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            dest[i] = src[i];
        }
    }
#elif defined(BOOST_NO_CXX14_CONSTEXPR)
    std::memcpy(dest, src, n);
#else
    // C++14 constexpr but no way to detect constant
    // evaluation: always use the byte loop.
    for (std::size_t i = 0; i < n; ++i)
    {
        dest[i] = src[i];
    }
#endif
}

} // detail
} // urls
} // boost

#endif
