//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_PARSER_FWD_HPP
#define BOOST_BEAST_HTTP_PARSER_FWD_HPP

#include <memory>

namespace boost {
namespace beast {
namespace http {

#ifndef BOOST_BEAST_DOXYGEN
template<
    bool isRequest,
    class Body,
    class Allocator = std::allocator<char>>
class parser;

template<class Body, class Allocator = std::allocator<char>>
using request_parser = parser<true, Body, Allocator>;

template<class Body, class Allocator = std::allocator<char>>
using response_parser = parser<false, Body, Allocator>;
#endif

} // http
} // beast
} // boost

#endif
