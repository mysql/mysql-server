//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_FILE_BODY_FWD_HPP
#define BOOST_BEAST_HTTP_FILE_BODY_FWD_HPP

#include <boost/beast/http/basic_file_body_fwd.hpp>

#include <boost/beast/core/file.hpp>

namespace boost {
namespace beast {
namespace http {

#ifndef BOOST_BEAST_DOXYGEN
using file_body = basic_file_body<file>;
#endif

} // http
} // beast
} // boost

#endif
