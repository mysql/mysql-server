//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_DELIM_RULE_HPP
#define BOOST_URL_GRAMMAR_DELIM_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/type_traits.hpp>
#include <type_traits>

namespace boost {
namespace urls {
namespace grammar {

namespace implementation_defined {
struct ch_delim_rule
{
    using value_type = core::string_view;

    constexpr
    ch_delim_rule(char ch) noexcept
        : ch_(ch)
    {
    }

    BOOST_URL_CXX20_CONSTEXPR
    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const noexcept;

private:
    char ch_;
};
} // implementation_defined

/** Match a character literal

    This matches the specified character.
    The value is a reference to the character
    in the underlying buffer, expressed as a
    `core::string_view`. The function @ref squelch
    may be used to turn this into `void` instead.
    If there is no more input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = core::string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    system::result< core::string_view > rv = parse( ".", delim_rule('.') );
    @endcode

    @par BNF
    @code
    char        = %00-FF
    @endcode

    @param ch The character to match
    @return A rule which matches the character.

    @see
        @ref parse,
        @ref squelch.
*/
constexpr
implementation_defined::ch_delim_rule
delim_rule( char ch ) noexcept
{
    return {ch};
}

//------------------------------------------------

namespace implementation_defined {
template<class CharSet>
struct cs_delim_rule
{
    using value_type = core::string_view;

    constexpr
    cs_delim_rule(
        CharSet const& cs) noexcept
        : cs_(cs)
    {
    }

    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const noexcept
    {
        if(it == end)
        {
            // end
            BOOST_URL_RETURN_EC(
                error::need_more);
        }
        if(! cs_(*it))
        {
            // wrong character
            BOOST_URL_RETURN_EC(
                error::mismatch);
        }
        return core::string_view{
            it++, 1 };
    }

private:
    CharSet cs_;
};
} // implementation_defined

/** Match a single character from a character set

    This matches exactly one character which
    belongs to the specified character set.
    The value is a reference to the character
    in the underlying buffer, expressed as a
    `core::string_view`. The function @ref squelch
    may be used to turn this into `void` instead.
    If there is no more input, the error code
    @ref error::need_more is returned.

    @par Value Type
    @code
    using value_type = core::string_view;
    @endcode

    @par Example
    Rules are used with the function @ref parse.
    @code
    system::result< core::string_view > rv = parse( "X", delim_rule( alpha_chars ) );
    @endcode

    @param cs The character set to use.
    @return A rule which matches a single character from the set.

    @see
        @ref alpha_chars,
        @ref parse,
        @ref squelch.
*/
template<BOOST_URL_CONSTRAINT(CharSet) CS>
constexpr
typename std::enable_if<
    ! std::is_convertible<
        CS, char>::value,
    implementation_defined::cs_delim_rule<CS>>::type
delim_rule(
    CS const& cs) noexcept
{
    // If you get a compile error here it
    // means that your type does not meet
    // the requirements for a CharSet.
    // Please consult the documentation.
    static_assert(
        is_charset<CS>::value,
        "CharSet requirements not met");

    return implementation_defined::cs_delim_rule<CS>(cs);
}

} // grammar
} // urls
} // boost

#include <boost/url/grammar/impl/delim_rule.hpp>

#endif
