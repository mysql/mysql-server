//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_SERIALIZER_FWD_HPP
#define BOOST_BEAST_HTTP_SERIALIZER_FWD_HPP

#include <boost/beast/http/fields_fwd.hpp>

namespace boost {
namespace beast {
namespace http {

#ifndef BOOST_BEAST_DOXYGEN
template<bool isRequest, class Body, class Fields = fields>
class serializer;

template<class Body, class Fields = fields>
using request_serializer = serializer<true, Body, Fields>;

template<class Body, class Fields = fields>
using response_serializer = serializer<false, Body, Fields>;
#endif

} // http
} // beast
} // boost

#endif
