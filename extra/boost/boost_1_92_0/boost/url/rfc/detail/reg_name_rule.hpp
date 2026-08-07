//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_REG_NAME_RULE_HPP
#define BOOST_URL_RFC_DETAIL_REG_NAME_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/pct_encoded_rule.hpp>
#include <boost/url/rfc/sub_delim_chars.hpp>
#include <boost/url/rfc/unreserved_chars.hpp>

namespace boost {
namespace urls {
namespace detail {

/*  VFALCO In theory we could enforce these
    additional requirements from errata 4942:

    Such a name consists of a sequence of domain
    labels separated by ".", each domain label
    starting and ending with an alphanumeric character
    and possibly also containing "-" characters.  The
    rightmost domain label of a fully qualified domain
    name in DNS may be followed by a single "." and
    should be if it is necessary to distinguish between
    the complete domain name and some local domain.
*/

/** Rule for reg-name

    @par BNF
    @code
    reg-name    = *( unreserved / pct-encoded / "-" / ".")
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>
    @li <a href="https://www.rfc-editor.org/errata/eid4942"
        >Errata ID: 4942</a>
*/
constexpr auto reg_name_rule =
    pct_encoded_rule(unreserved_chars + sub_delim_chars);

} // detail
} // urls
} // boost

#endif
