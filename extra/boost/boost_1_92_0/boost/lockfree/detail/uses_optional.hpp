//  Copyright (C) 2023 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_USES_OPTIONAL_HPP
#define BOOST_LOCKFREE_DETAIL_USES_OPTIONAL_HPP

#include <boost/config.hpp>

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL

#    include <optional>

namespace boost { namespace lockfree {

struct uses_optional_t
{};

#    ifdef BOOST_NO_CXX17_INLINE_VARIABLES
static
#    else
inline
#    endif
    constexpr uses_optional_t uses_optional;

}} // namespace boost::lockfree

#endif

#endif /* BOOST_LOCKFREE_DETAIL_USES_OPTIONAL_HPP */
