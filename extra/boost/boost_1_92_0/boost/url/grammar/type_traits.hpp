//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_TYPE_TRAITS_HPP
#define BOOST_URL_GRAMMAR_TYPE_TRAITS_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <type_traits>

namespace boost {
namespace urls {
namespace grammar {

namespace implementation_defined
{
template<class T, class = void>
struct is_rule : std::false_type {};

template<class T>
struct is_rule<T, void_t<decltype(
    std::declval<system::result<typename T::value_type>&>() =
        std::declval<T const&>().parse(
            std::declval<char const*&>(),
            std::declval<char const*>())
    )>> : std::is_nothrow_copy_constructible<T>
{
};
}

/** Determine if T meets the requirements of @ref Rule

    This is an alias for `std::true_type` if
    `T` meets the requirements, otherwise it
    is an alias for `std::false_type`.

    @par Example
    @code
    struct U
    {
        struct value_type;

        auto
        parse(
            char const*& it,
            char const* end) const ->
                system::result<value_type>
    };

    static_assert( is_rule<U>::value, "Requirements not met" );
    @endcode

    @see
        @ref parse.
*/
template<class T>
using is_rule = implementation_defined::is_rule<T>;

#ifdef BOOST_URL_HAS_CONCEPTS
/** Concept for a grammar Rule

    This concept is satisfied if `T` is a
    valid grammar Rule

    A `Rule` defines an algorithm used to match an input
    buffer of ASCII characters against a set of syntactical
    specifications.

    Each rule represents either a terminal symbol or a
    composition in the represented grammar.

    The library comes with a set of rules for productions
    typically found in RFC documents.
    Rules are not invoked directly; instead, rule variables are
    used with overloads of @ref parse which provide a convenient,
    uniform front end.

    @par Exemplar

    For best results, it is suggested that all constructors for
    rules be marked `constexpr`.

    @code
    struct Rule
    {
        struct value_type;

        constexpr Rule( Rule const& ) noexcept = default;

        auto parse( char const*& it, char const* end ) const -> result< value_type >;
    };

    // Declare a variable of type Rule for notational convenience
    constexpr Rule rule{};
    @endcode

    @par Model

    @li @ref dec_octet_rule
    @li @ref delim_rule
    @li @ref not_empty_rule
    @li @ref optional_rule
    @li @ref range_rule
    @li @ref token_rule
    @li @ref tuple_rule
    @li @ref unsigned_rule
    @li @ref variant_rule

    @see
        @ref parse,
        @ref is_rule.
 */
template <class T>
concept Rule =
    requires (T t, char const*& it, char const* end)
    {
        typename T::value_type;
        { t.parse(it, end) } -> std::same_as<system::result<typename T::value_type>>;
    };
#endif


} // grammar
} // urls
} // boost

#endif
