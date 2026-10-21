//
// Copyright (c) 2021 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_CHARSET_HPP
#define BOOST_URL_GRAMMAR_CHARSET_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/detail/charset.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <cstdint>
#include <type_traits>
#include <utility>

#ifdef BOOST_URL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace urls {
namespace grammar {

namespace implementation_defined
{
template<class T, class = void>
struct is_charset : std::false_type {};

template<class T>
struct is_charset<T, void_t<
    decltype(
    std::declval<bool&>() =
        std::declval<T const&>().operator()(
            std::declval<char>())
            ) > > : std::true_type
{
};
}

/** Alias for `std::true_type` if T satisfies @ref CharSet.

    This metafunction determines if the
    type `T` meets these requirements of
    <em>CharSet</em>:

    @li An instance of `T` is invocable
    with this equivalent function signature:
    @code
    bool T::operator()( char ) const noexcept;
    @endcode

    @par Example
    Use with `enable_if` on the return value:
    @code
    template< class CharSet >
    typename std::enable_if< is_charset<T>::value >::type
    func( CharSet const& cs );
    @endcode

    @tparam T the type to check.
*/
template<class T>
using is_charset = BOOST_URL_SEE_BELOW(implementation_defined::is_charset<T>);

#ifdef BOOST_URL_HAS_CONCEPTS
/** Concept for a CharSet

    A `CharSet` is a unary predicate which is invocable with
    this equivalent signature:

    @code
    bool( char ch ) const noexcept;
    @endcode

    The predicate returns `true` if `ch` is a member of the
    set, or `false` otherwise.

    @par Exemplar

    For best results, it is suggested that all constructors and
    member functions for character sets be marked `constexpr`.

    @code
    struct CharSet
    {
        bool operator()( char c ) const noexcept;

        // These are both optional. If either or both are left
        // unspecified, a default implementation will be used.
        //
        char const* find_if( char const* first, char const* last ) const noexcept;
        char const* find_if_not( char const* first, char const* last ) const noexcept;
    };
    @endcode

    @par Models

    @li @ref alnum_chars
    @li @ref alpha_chars
    @li @ref digit_chars
    @li @ref hexdig_chars
    @li @ref lut_chars

    @see
        @ref is_charset,
        @ref find_if,
        @ref find_if_not.

 */
template <class T>
concept CharSet =
    requires (T const t, char c)
{
    { t(c) } -> std::convertible_to<bool>;
};
#endif


//------------------------------------------------

/** Find the first character in the string that is in the set.

    @par Exception Safety
    Throws nothing.

    @return A pointer to the found character,
    otherwise the value `last`.

    @param first A pointer to the first character
    in the string to search.

    @param last A pointer to one past the last
    character in the string to search.

    @param cs The character set to use.

    @see
        @ref find_if_not.
*/
template<BOOST_URL_CONSTRAINT(CharSet) CS>
BOOST_URL_CXX14_CONSTEXPR
char const*
find_if(
    char const* const first,
    char const* const last,
    CS const& cs) noexcept
{
    // If you get a compile error here
    // it means your type does not meet
    // the requirements. Please check the
    // documentation.
    static_assert(
        is_charset<CS>::value,
        "CharSet requirements not met");

    return detail::find_if(first, last, cs,
        detail::has_find_if<CS>{});
}

/** Find the first character in the string that is not in CharSet

    @par Exception Safety
    Throws nothing.

    @return A pointer to the found character,
    otherwise the value `last`.

    @param first A pointer to the first character
    in the string to search.

    @param last A pointer to one past the last
    character in the string to search.

    @param cs The character set to use.

    @see
        @ref find_if_not.
*/
template<BOOST_URL_CONSTRAINT(CharSet) CS>
BOOST_URL_CXX14_CONSTEXPR
char const*
find_if_not(
    char const* const first,
    char const* const last,
    CS const& cs) noexcept
{
    // If you get a compile error here
    // it means your type does not meet
    // the requirements. Please check the
    // documentation.
    static_assert(
        is_charset<CS>::value,
        "CharSet requirements not met");

    return detail::find_if_not(first, last, cs,
        detail::has_find_if_not<CS>{});
}

//------------------------------------------------

namespace implementation_defined {
template<class CharSet>
struct charset_ref
{
    CharSet const& cs_;

    constexpr
    bool
    operator()(char ch) const noexcept
    {
        return cs_(ch);
    }

    BOOST_URL_CXX14_CONSTEXPR
    char const*
    find_if(
        char const* first,
        char const* last) const noexcept
    {
        return grammar::find_if(
            first, last, cs_);
    }

    BOOST_URL_CXX14_CONSTEXPR
    char const*
    find_if_not(
        char const* first,
        char const* last) const noexcept
    {
        return grammar::find_if_not(
            first, last, cs_ );
    }
};
} // implementation_defined

/** Return a reference to a character set

    This function returns a character set which
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

    @tparam CharSet The character set type
    @param cs The character set to use
    @return The character set as a reference type
*/
template<BOOST_URL_CONSTRAINT(CharSet) CS>
constexpr
typename std::enable_if<
    is_charset<CS>::value &&
    ! std::is_same<CS,
        implementation_defined::charset_ref<CS> >::value,
    implementation_defined::charset_ref<CS> >::type
ref(CS const& cs) noexcept
{
    return implementation_defined::charset_ref<CS>{cs};
}

} // grammar
} // urls
} // boost

#endif
