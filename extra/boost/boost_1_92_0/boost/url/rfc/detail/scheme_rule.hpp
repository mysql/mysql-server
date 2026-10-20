//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_SCHEME_RULE_HPP
#define BOOST_URL_RFC_DETAIL_SCHEME_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/scheme.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for scheme

    @par BNF
    @code
    scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.1"
        >3.1. Scheme (rfc3986)</a>

    @see
        @ref scheme.
*/
struct scheme_rule
{
    struct value_type
    {
        core::string_view scheme;
        urls::scheme scheme_id =
            urls::scheme::unknown;
    };

    BOOST_URL_CXX20_CONSTEXPR
    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const noexcept;
};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/scheme_rule.hpp>

#endif
