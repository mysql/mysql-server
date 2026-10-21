//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_RELATIVE_PART_RULE_HPP
#define BOOST_URL_RFC_DETAIL_RELATIVE_PART_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/rfc/authority_rule.hpp>
#include <cstdlib>

namespace boost {
namespace urls {
namespace detail {

/** Rule for relative-part

    @par BNF
    @code
    relative-part = "//" authority path-abempty
                  / path-absolute
                  / path-noscheme
                  / path-abempty
                  / path-empty
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-4.2"
        >4.2. Relative Reference (rfc3986)</a>
    @li <a href="https://www.rfc-editor.org/errata/eid5428"
        >Errata ID: 5428 (rfc3986)</a>

    @see
        @ref authority_rule.
*/
struct relative_part_rule_t
{
    struct value_type
    {
        authority_view authority;
        pct_string_view path;
        std::size_t segment_count = 0;
        bool has_authority = false;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr relative_part_rule_t relative_part_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/relative_part_rule.hpp>

#endif
