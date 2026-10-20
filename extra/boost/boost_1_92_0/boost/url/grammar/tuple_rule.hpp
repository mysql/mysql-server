//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_TUPLE_RULE_HPP
#define BOOST_URL_GRAMMAR_TUPLE_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/detail/tuple.hpp>
#include <boost/url/grammar/type_traits.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <boost/core/empty_value.hpp>
#include <tuple>

namespace boost {
namespace urls {
namespace grammar {

namespace implementation_defined {
template<
    class R0,
    class... Rn>
class tuple_rule_t
    : empty_value<
        detail::tuple<R0, Rn...>>
{
    using T = mp11::mp_remove<
        std::tuple<
            typename R0::value_type,
            typename Rn::value_type...>,
        void>;
    static constexpr bool IsList =
        mp11::mp_size<T>::value != 1;

public:
    using value_type =
        mp11::mp_eval_if_c<IsList,
            T, mp11::mp_first, T>;

    constexpr
    tuple_rule_t(
        R0 const& r0,
        Rn const&... rn) noexcept
        : empty_value<
            detail::tuple<R0, Rn...>>(
                empty_init,
                r0, rn...)
    {
    }

    BOOST_URL_CXX14_CONSTEXPR
    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const;

};
} // implementation_defined

/** Match a series of rules in order

    This matches a series of rules in the
    order specified. Upon success the input
    is adjusted to point to the first
    unconsumed character. There is no
    implicit specification of linear white
    space between each rule.

    @par Value Type
    @code
    using value_type = __see_below__;
    @endcode

    The sequence rule usually returns a
    `std::tuple` containing the the `value_type`
    of each corresponding rule in the sequence,
    except that `void` values are removed.
    However, if there is exactly one non-void
    value type `T`, then the sequence rule
    returns `system::result<T>` instead of
    `system::result<tuple<...>>`.

    @par Example
    Rules are used with the function @ref parse.
    @code
    system::result< std::tuple< unsigned char, unsigned char, unsigned char, unsigned char > > rv =
        parse( "192.168.0.1",
            tuple_rule(
                dec_octet_rule,
                squelch( delim_rule('.') ),
                dec_octet_rule,
                squelch( delim_rule('.') ),
                dec_octet_rule,
                squelch( delim_rule('.') ),
                dec_octet_rule ) );
    @endcode

    @par BNF
    @code
    sequence     = rule1 rule2 rule3...
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc5234#section-3.1"
        >3.1.  Concatenation (rfc5234)</a>

    @param r0 The first rule to match
    @param rn A list of one or more rules to match
    @return The sequence rule

    @see
        @ref dec_octet_rule,
        @ref delim_rule,
        @ref parse,
        @ref squelch.
*/
template<
    BOOST_URL_CONSTRAINT(Rule) R0,
    BOOST_URL_CONSTRAINT(Rule)... Rn>
constexpr
auto
tuple_rule(
    R0 const& r0,
    Rn const&... rn) noexcept ->
        implementation_defined::tuple_rule_t<
            R0, Rn...>
{
    BOOST_CORE_STATIC_ASSERT(
        mp11::mp_all<
            is_rule<R0>,
            is_rule<Rn>...>::value);
    return { r0, rn... };
}

namespace implementation_defined {

template<class Rule>
struct squelch_rule_t
    : empty_value<Rule>
{
    using value_type = void;

    constexpr
    squelch_rule_t(
        Rule const& r) noexcept
        : empty_value<Rule>(
            empty_init, r)
    {
    }

    BOOST_URL_CXX14_CONSTEXPR
    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const
    {
        auto rv = this->get().parse(it, end);
        if(rv.error())
            return rv.error();
        return {}; // void
    }
};

} // implementation_defined

/** Squelch the value of a rule

    This function returns a new rule which
    matches the specified rule, and converts
    its value type to `void`. This is useful
    for matching delimiters in a grammar,
    where the value for the delimiter is not
    needed.

    @par Value Type
    @code
    using value_type = void;
    @endcode

    @par Example 1
    With `squelch`:
    @code
    system::result< std::tuple< decode_view, core::string_view > > rv = parse(
        "www.example.com:443",
        tuple_rule(
            pct_encoded_rule(unreserved_chars + '-' + '.'),
            squelch( delim_rule( ':' ) ),
            token_rule( digit_chars ) ) );
    @endcode

    @par Example 2
    Without `squelch`:
    @code
    system::result< std::tuple< decode_view, core::string_view, core::string_view > > rv = parse(
        "www.example.com:443",
        tuple_rule(
            pct_encoded_rule(unreserved_chars + '-' + '.'),
            delim_rule( ':' ),
            token_rule( digit_chars ) ) );
    @endcode

    @param r The rule to squelch
    @return The squelched rule

    @see
        @ref delim_rule,
        @ref digit_chars,
        @ref parse,
        @ref tuple_rule,
        @ref token_rule,
        @ref decode_view,
        @ref pct_encoded_rule,
        @ref unreserved_chars.
*/
template<BOOST_URL_CONSTRAINT(Rule) R>
constexpr
BOOST_URL_IMPLEMENTATION_DEFINED(implementation_defined::squelch_rule_t<R>)
squelch( R const& r ) noexcept
{
    BOOST_CORE_STATIC_ASSERT(is_rule<R>::value);
    return { r };
}

} // grammar
} // urls
} // boost

#include <boost/url/grammar/impl/tuple_rule.hpp>

#endif
