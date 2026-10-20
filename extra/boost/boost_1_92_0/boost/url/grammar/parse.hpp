//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_PARSE_HPP
#define BOOST_URL_GRAMMAR_PARSE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/grammar/type_traits.hpp>

namespace boost {
namespace urls {
namespace grammar {

//------------------------------------------------

/** Parse a character buffer using a rule

    @param it A pointer to the start. The
    caller's variable is changed to
    reflect the amount of input consumed.

    @param end A pointer to the end.

    @param r The rule to use

    @return The parsed value upon success,
    otherwise an error.
*/
template<BOOST_URL_CONSTRAINT(Rule) R>
BOOST_URL_CXX14_CONSTEXPR
system::result<typename R::value_type>
parse(
    char const*& it,
    char const* end,
    R const& r);

/** Parse a character buffer using a rule

    This function parses a complete string into
    the specified sequence of rules. If the
    string is not completely consumed, an
    error is returned instead.

    @param s The input string

    @param r The rule to use

    @return The parsed value upon success,
    otherwise an error.
*/
template<BOOST_URL_CONSTRAINT(Rule) R>
BOOST_URL_CXX14_CONSTEXPR
system::result<typename R::value_type>
parse(
    core::string_view s,
    R const& r);

//------------------------------------------------

namespace implementation_defined {
template<class Rule>
struct rule_ref
{
    Rule const& r_;

    using value_type =
        typename Rule::value_type;

    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const
    {
        return r_.parse(it, end);
    }
};
} // implementation_defined

/** Return a reference to a rule

    This function returns a rule which
    references the specified object. This is
    used to reduce the number of bytes of
    storage (`sizeof`) required by a combinator
    when it stores a copy of the object.
    <br>
    Ownership of the object is not transferred;
    the caller is responsible for ensuring the
    lifetime of the object is extended until it
    is no longer referenced. For best results,
    `ref` should only be used with compile-time
    constants.

    @param r The rule to use
    @return The rule as a reference type
*/
template<BOOST_URL_CONSTRAINT(Rule) R>
constexpr
typename std::enable_if<
    is_rule<R>::value &&
    ! std::is_same<R,
        implementation_defined::rule_ref<R> >::value,
    implementation_defined::rule_ref<R> >::type
ref(R const& r) noexcept
{
    return implementation_defined::rule_ref<R>{r};
}

#ifndef BOOST_URL_DOCS
#ifndef BOOST_URL_MRDOCS
// If you get a compile error here it
// means you called ref with something
// that is not a CharSet or Rule!
constexpr
void
ref(...) = delete;
#endif
#endif

} // grammar
} // urls
} // boost

#include <boost/url/grammar/impl/parse.hpp>

#endif
