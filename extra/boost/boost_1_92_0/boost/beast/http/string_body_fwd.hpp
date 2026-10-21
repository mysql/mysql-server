//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_STRING_BODY_FWD_HPP
#define BOOST_BEAST_HTTP_STRING_BODY_FWD_HPP

#include <memory>
#include <string>

namespace boost {
namespace beast {
namespace http {

#ifndef BOOST_BEAST_DOXYGEN
template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>>
struct basic_string_body;

using string_body = basic_string_body<char>;
#endif

} // http
} // beast
} // boost

#endif
