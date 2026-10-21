//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_MESSAGE_FWD_HPP
#define BOOST_BEAST_HTTP_MESSAGE_FWD_HPP

#include <boost/beast/http/fields_fwd.hpp>

namespace boost {
namespace beast {
namespace http {

#ifndef BOOST_BEAST_DOXYGEN
template<bool isRequest, class Fields = fields>
class header;

template<bool isRequest, class Body, class Fields = fields>
class message;

template<class Fields = fields>
using request_header = header<true, Fields>;

template<class Fields = fields>
using response_header = header<false, Fields>;

template<class Body, class Fields = fields>
using request = message<true, Body, Fields>;

template<class Body, class Fields = fields>
using response = message<false, Body, Fields>;
#endif

} // http
} // beast
} // boost

#endif
