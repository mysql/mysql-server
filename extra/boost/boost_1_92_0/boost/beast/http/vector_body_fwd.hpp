//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_VECTOR_BODY_FWD_HPP
#define BOOST_BEAST_HTTP_VECTOR_BODY_FWD_HPP

#include <memory>

namespace boost {
namespace beast {
namespace http {

template<class T, class Allocator = std::allocator<T>>
struct vector_body;

} // http
} // beast
} // boost

#endif
