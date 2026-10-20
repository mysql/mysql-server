//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_PARSE_HPP
#define BOOST_JSON_PARSE_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/error.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/string_view.hpp>
#include <boost/json/value.hpp>

namespace boost {
namespace json {

/** Return parsed JSON as a @ref value.

    This function parses input in one step to produce a complete JSON @ref
    value. If the input does not contain a complete serialized JSON, an error
    occurs. In this case **(1)**, **(2)**, **(4)**, and **(5)** return a null
    value that uses the
    \<\<default_memory_resource,default memory resource\>\>, and set `ec` to
    the corresponding error value. **(3)** and **(6)** throw an exception.

    @par Complexity
    @li **(1)**, **(2)**, **(3)** linear in `s.size()`.
    @li **(4)**, **(5)**, **(6)** linear in the size of consumed input.

    @par Exception Safety
    @li **(1)**, **(2)**, **(3)** strong guarantee.
    @li **(4)**, **(5)**, **(6)** basic guarantee.

    __(3)__, **(6)** throw `boost::system::system_error` on failed parse.
    Calls to `memory_resource::allocate` may throw.
    The stream `is` may throw as described by @ref std::ios::exceptions.

    @return A value representing the parsed JSON.

    @param s The string to parse.

    @param ec Set to the error, if any occurred.
    @param sp The memory resource that the new value and all of its elements
           will use.
    @param opt The options for the parser. If this parameter is omitted, the
           parser will accept only standard JSON.

    @see @ref parse_options, @ref stream_parser, @ref value::operator>>.

    @{
*/
BOOST_JSON_DECL
value
parse(
    string_view s,
    system::error_code& ec,
    storage_ptr sp = {},
    parse_options const& opt = {});

/// Overload
BOOST_JSON_DECL
value
parse(
    string_view s,
    std::error_code& ec,
    storage_ptr sp = {},
    parse_options const& opt = {});

/// Overload
BOOST_JSON_DECL
value
parse(
    string_view s,
    storage_ptr sp = {},
    parse_options const& opt = {});

/** Overload
    @param is The stream to read from.
    @param ec
    @param sp
    @param opt
*/
BOOST_JSON_DECL
value
parse(
    std::istream& is,
    system::error_code& ec,
    storage_ptr sp = {},
    parse_options const& opt = {});

/// Overload
BOOST_JSON_DECL
value
parse(
    std::istream& is,
    std::error_code& ec,
    storage_ptr sp = {},
    parse_options const& opt = {});

/// Overload
BOOST_JSON_DECL
value
parse(
    std::istream& is,
    storage_ptr sp = {},
    parse_options const& opt = {});
/// @}

} // namespace json
} // namespace boost

#endif
