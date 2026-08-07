//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_DYNAMIC_BODY_FWD_HPP
#define BOOST_BEAST_HTTP_DYNAMIC_BODY_FWD_HPP

#include <boost/beast/http/basic_dynamic_body_fwd.hpp>

#include <memory>

namespace boost {
namespace beast {

#ifndef BOOST_BEAST_DOXYGEN
template<class Allocator>
class basic_multi_buffer;

using multi_buffer = basic_multi_buffer<std::allocator<char>>;
#endif

namespace http {

#ifndef BOOST_BEAST_DOXYGEN
using dynamic_body = basic_dynamic_body<multi_buffer>;
#endif

} // http
} // beast
} // boost

#endif
