//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_FIELDS_FWD_HPP
#define BOOST_BEAST_HTTP_FIELDS_FWD_HPP

#include <memory>

namespace boost {
namespace beast {
namespace http {

template<class Allocator>
class basic_fields;

#ifndef BOOST_BEAST_DOXYGEN
using fields = basic_fields<std::allocator<char>>;
#endif

} // http
} // beast
} // boost

#endif
