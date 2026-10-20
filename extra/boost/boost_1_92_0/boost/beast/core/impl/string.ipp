//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_STRING_IPP
#define BOOST_BEAST_IMPL_STRING_IPP

#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/string.hpp>

#include <algorithm>

namespace boost {
namespace beast {

bool
iequals(
    beast::string_view lhs,
    beast::string_view rhs)
{
    if(lhs.size() != rhs.size())
        return false;
    auto n  = lhs.size();
    auto p1 = lhs.data();
    auto p2 = rhs.data();
    // fast loop
    while(n--)
    {
        if(*p1++ != *p2++)
            goto slow;
    }
    return true;
slow:
    --p1;
    --p2;
    do
    {
        if( detail::ascii_tolower(*p1++) !=
            detail::ascii_tolower(*p2++))
            return false;
    }
    while(n--);
    return true;
}

bool
iless::operator()(
    string_view lhs,
    string_view rhs) const
{
    using std::begin;
    using std::end;
    return std::lexicographical_compare(
        begin(lhs), end(lhs), begin(rhs), end(rhs),
        [](char c1, char c2)
        {
            return detail::ascii_tolower(c1) < detail::ascii_tolower(c2);
        }
    );
}

} // beast
} // boost

#endif
