//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_HIER_PART_RULE_HPP
#define BOOST_URL_RFC_DETAIL_HIER_PART_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/rfc/authority_rule.hpp>
#include <cstdlib>

namespace boost {
namespace urls {
namespace detail {

/** Rule for hier-part

    @par BNF
    @code
    hier-part     = "//" authority path-abempty
                  / path-absolute
                  / path-rootless
                  / path-empty
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3"
        >3. Syntax Components (rfc3986)</a>
*/
struct hier_part_rule_t
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
        char const* const end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr hier_part_rule_t hier_part_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/hier_part_rule.hpp>

#endif
