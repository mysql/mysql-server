//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_VARIANT_RULE_HPP
#define BOOST_URL_GRAMMAR_VARIANT_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/variant.hpp>
#include <boost/url/grammar/detail/tuple.hpp>
#include <boost/url/grammar/type_traits.hpp>

namespace boost {
namespace urls {
namespace grammar {

namespace implementation_defined {
template<
    class R0, class... Rn>
class variant_rule_t
{
public:
    using value_type = variant2::variant<
        typename R0::value_type,
        typename Rn::value_type...>;

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end) const ->
            system::result<value_type>;

    constexpr
    variant_rule_t(
        R0 const& r0,
        Rn const&... rn) noexcept
        : rn_(r0, rn...)
    {
    }

private:

    detail::tuple<R0, Rn...> rn_;
};
} // implementation_defined

/** Match one of a set of rules

    Each specified rule is tried in sequence.
    When the first match occurs, the result
    is stored and returned in the variant. If
    no match occurs, an error is returned.

    @param r0 The first rule to match
    @param rn A list of one or more rules to match
    @return The variant rule

    @par Value Type
    @code
    using value_type = variant< typename Rules::value_type... >;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    // request-target = origin-form
    //                / absolute-form
    //                / authority-form
    //                / asterisk-form

    system::result< variant< url_view, url_view, authority_view, core::string_view > > rv = grammar::parse(
        "/index.html?width=full",
        variant_rule(
            origin_form_rule,
            absolute_uri_rule,
            authority_rule,
            delim_rule('*') ) );
    @endcode

    @par BNF
    @code
    variant     = rule1 / rule2 / rule3...
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc5234#section-3.2"
        >3.2.  Alternatives (rfc5234)</a>
    @li <a href="https://datatracker.ietf.org/doc/html/rfc7230#section-5.3"
        >5.3.  Request Target (rfc7230)</a>

    @see
        @ref absolute_uri_rule,
        @ref authority_rule,
        @ref delim_rule,
        @ref parse,
        @ref origin_form_rule,
        @ref url_view.
*/
template<
    BOOST_URL_CONSTRAINT(Rule) R0,
    BOOST_URL_CONSTRAINT(Rule)... Rn>
constexpr
auto
variant_rule(
    R0 const& r0,
    Rn const&... rn) noexcept ->
        implementation_defined::variant_rule_t<R0, Rn...>;

} // grammar
} // urls
} // boost

#include <boost/url/grammar/impl/variant_rule.hpp>

#endif
