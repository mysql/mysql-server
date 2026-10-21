//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/http_proto
//

#ifndef BOOST_URL_GRAMMAR_TOKEN_RULE_HPP
#define BOOST_URL_GRAMMAR_TOKEN_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/error_types.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/empty_value.hpp>
#include <type_traits>

namespace boost {
namespace urls {
namespace grammar {

namespace implementation_defined {
template<class CharSet>
struct token_rule_t
    : private empty_value<CharSet>
{
    using value_type = core::string_view;

    static_assert(
        is_charset<CharSet>::value,
        "CharSet requirements not met");

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;

    constexpr
    token_rule_t(
        CharSet const& cs) noexcept
        : empty_value<CharSet>(
            empty_init, cs)
    {
    }

    template<class CS = CharSet>
    constexpr
    token_rule_t(
        typename std::enable_if<
            std::is_default_constructible<CS>::value,
            int>::type = 0) noexcept
        : empty_value<CharSet>(
            empty_init)
    {
    }
};
}

/** Match a non-empty string of characters from a set

    If there is no more input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = core::string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    system::result< core::string_view > rv = parse( "abcdef", token_rule( alpha_chars ) );
    @endcode

    @par BNF
    @code
    token     = 1*( ch )
    @endcode

    @param cs The character set to use
    @return The token rule

    @see
        @ref alpha_chars,
        @ref parse.
*/
template<BOOST_URL_CONSTRAINT(CharSet) CS>
constexpr
auto
token_rule(
    CS const& cs) noexcept ->
        implementation_defined::token_rule_t<CS>
{
    return {cs};
}

/** Match a non-empty string of characters from a default-constructible set

    This overload is only available when CharSet is
    default constructible.

    If there is no more input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = core::string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    system::result< core::string_view > rv = parse( "abcdef", token_rule<alpha_chars_t>() );
    @endcode

    @par BNF
    @code
    token     = 1*( ch )
    @endcode

    @tparam CharSet The character set type to use
    @return The token rule

    @see
        @ref alpha_chars,
        @ref parse.
*/
template<BOOST_URL_CONSTRAINT(CharSet) CharSet>
constexpr
auto
token_rule() noexcept ->
    typename std::enable_if<
        std::is_default_constructible<CharSet>::value,
        implementation_defined::token_rule_t<CharSet>>::type
{
    return implementation_defined::token_rule_t<CharSet>();
}

} // grammar
} // urls
} // boost

#include <boost/url/grammar/impl/token_rule.hpp>

#endif
