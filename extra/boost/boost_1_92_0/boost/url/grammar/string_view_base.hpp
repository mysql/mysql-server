//
// Copyright (c) 2022 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_STRING_VIEW_BASE_HPP
#define BOOST_URL_GRAMMAR_STRING_VIEW_BASE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/detail/string_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace urls {
namespace grammar {

/** Common functionality for string views

    This base class is used to provide common
    member functions for reference types that
    behave like string views. This cannot be
    instantiated directly; instead, derive
    from the type and provide constructors
    which offer any desired preconditions
    and invariants.
*/
class string_view_base
{
protected:
    /** The referenced character buffer
    */
    core::string_view s_;

    /** Constructor

        @param s The string view
    */
    constexpr
    string_view_base(
        core::string_view s) noexcept
        : s_(s)
    {
    }

    /** Constructor

        @param data The character buffer
        @param size The number of characters
    */
    constexpr
    string_view_base(
        char const* data,
        std::size_t size) noexcept
        : s_(data, size)
    {
    }

    /** Swap

        @param s The object to swap with
    */
    // VFALCO No idea why this fails in msvc
    /*BOOST_CXX14_CONSTEXPR*/
    void
    swap(
        string_view_base& s ) noexcept
    {
        std::swap(s_, s.s_);
    }

    /** Constructor
    */
    string_view_base() = default;

    /** Constructor
    */
    string_view_base(
        string_view_base const&) = default;

    /** Assignment

        @param other The object to assign
        @return A reference to this object
    */
    string_view_base& operator=(
        string_view_base const& other) = default;

public:
    /// The character traits
    typedef std::char_traits<char> traits_type;
    /// The value type
    typedef char value_type;
    /// The pointer type
    typedef char* pointer;
    /// The const pointer type
    typedef char const* const_pointer;
    /// The reference type
    typedef char& reference;
    /// The const reference type
    typedef char const& const_reference;
    /// The const iterator type
    typedef char const* const_iterator;
    /// The iterator type
    typedef const_iterator iterator;
    /// The const reverse iterator type
    typedef std::reverse_iterator<
        const_iterator> const_reverse_iterator;
    /// The reverse iterator type
    typedef const_reverse_iterator reverse_iterator;
    /// The size type
    typedef std::size_t size_type;
    /// The difference type
    typedef std::ptrdiff_t difference_type;

    /// A constant used to represent "no position"
    static constexpr std::size_t npos = core::string_view::npos;

    //--------------------------------------------

    /** Conversion

        @return A string view with the same contents
     */
    constexpr operator
    core::string_view() const noexcept
    {
        return s_;
    }

    /** Conversion

        @return A string view with the same contents
     */
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
    operator
    std::string_view() const noexcept
    {
        return std::string_view(s_);
    }
#endif

    /** Conversion

        Conversion to std::string is explicit
        because assigning to string using an
        implicit constructor does not preserve
        capacity.

        @return A string with the same contents
    */
    explicit
    operator
    std::string() const
    {
        return std::string(s_);
    }

    //--------------------------------------------

    // iterator support

    /** Return an iterator to the beginning

        See `core::string_view::begin`

        @return An iterator to the beginning
    */
    BOOST_CONSTEXPR const_iterator begin() const noexcept
    {
        return s_.begin();
    }

    /** Return an iterator to the end

        See `core::string_view::end`

        @return An iterator to the end
    */
    BOOST_CONSTEXPR const_iterator end() const noexcept
    {
        return s_.end();
    }

    /** Return an iterator to the beginning

        See `core::string_view::cbegin`

        @return An iterator to the beginning
    */
    BOOST_CONSTEXPR const_iterator cbegin() const noexcept
    {
        return s_.cbegin();
    }

    /** Return an iterator to the end

        See `core::string_view::cend`

        @return An iterator to the end
    */
    BOOST_CONSTEXPR const_iterator cend() const noexcept
    {
        return s_.cend();
    }

    /** Return a reverse iterator to the end

        See `core::string_view::rbegin`

        @return A reverse iterator to the end
    */
    BOOST_URL_LIB_ARRAY_CONSTEXPR
    const_reverse_iterator rbegin() const noexcept
    {
        return s_.rbegin();
    }

    /** Return a reverse iterator to the beginning

        See `core::string_view::rend`

        @return A reverse iterator to the beginning
    */
    BOOST_URL_LIB_ARRAY_CONSTEXPR
    const_reverse_iterator rend() const noexcept
    {
        return s_.rend();
    }

    /** Return a reverse iterator to the end

        See `core::string_view::crbegin`

        @return A reverse iterator to the end
    */
    BOOST_URL_LIB_ARRAY_CONSTEXPR
    const_reverse_iterator crbegin() const noexcept
    {
        return s_.crbegin();
    }

    /** Return a reverse iterator to the beginning

        See `core::string_view::crend`

        @return A reverse iterator to the beginning
    */
    BOOST_URL_LIB_ARRAY_CONSTEXPR
    const_reverse_iterator crend() const noexcept
    {
        return s_.crend();
    }

    // capacity

    /** Return the size

        See `core::string_view::size`

        @return The size
    */
    BOOST_CONSTEXPR size_type size() const noexcept
    {
        return s_.size();
    }

    /** Return the size

        See `core::string_view::length`

        @return The size
    */
    BOOST_CONSTEXPR size_type length() const noexcept
    {
        return s_.length();
    }

    /** Return the maximum allowed size

        See `core::string_view::max_size`

        @return The maximum allowed size
    */
    BOOST_CONSTEXPR size_type max_size() const noexcept
    {
        return s_.max_size();
    }

    /** Return true if the string is empty

        See `core::string_view::size`

        @return `true` if the string is empty
    */
    BOOST_CONSTEXPR bool empty() const noexcept
    {
        return s_.empty();
    }
   
    // element access

    /** Access a character

        See `core::string_view::operator[]`

        @param pos The position to access
        @return The character at the position
    */
    BOOST_CXX14_CONSTEXPR const_reference
        operator[]( size_type pos ) const noexcept
    {
        return s_[pos];
    }

    /** Access a character

        See `core::string_view::at`

        @param pos The position to access
        @return The character at the position
    */
    BOOST_CXX14_CONSTEXPR const_reference
        at( size_type pos ) const
    {
        return s_.at(pos);
    }

    /** Return the first character

        See `core::string_view::front`

        @return The first character
    */
    BOOST_CXX14_CONSTEXPR const_reference
        front() const noexcept
    {
        return s_.front();
    }

    /** Return the last character

        See `core::string_view::back`

        @return The last character
    */
    BOOST_CXX14_CONSTEXPR const_reference
        back() const noexcept
    {
        return s_.back();
    }

    /** Return a pointer to the character buffer

        See `core::string_view::data`

        @return A pointer to the character buffer
    */
    BOOST_CONSTEXPR const_pointer
        data() const noexcept
    {
        return s_.data();
    }

    // string operations

    /** Copy the characters to another buffer

        See `core::string_view::copy`

        @param s The destination buffer
        @param n The number of characters to copy
        @param pos The position to start from
        @return The number of characters copied
    */
    BOOST_CXX14_CONSTEXPR size_type copy(
        char* s, size_type n, size_type pos = 0 ) const
    {
        return s_.copy(s, n, pos);
    }

    /** Return a view to part of the string

        See `core::string_view::substr`

        @param pos The position to start from
        @param n The number of characters
        @return A view to the substring
    */
    BOOST_CXX14_CONSTEXPR core::string_view substr(
        size_type pos = 0, size_type n = core::string_view::npos ) const
    {
        return s_.substr(pos, n);
    }

    // comparison

    /** Return the result of comparing to another string

        See `core::string_view::compare`

        @param str The string to compare
        @return The result of the comparison
    */
    BOOST_CXX14_CONSTEXPR int
        compare( core::string_view str ) const noexcept
    {
        return s_.compare(str);
    }

    /** Return the result of comparing to another string

        See `core::string_view::compare`

        @param pos1 The position to start comparing from
        @param n1 The number of characters to compare
        @param str The string to compare
        @return The result of the comparison
    */
    BOOST_CONSTEXPR int compare(
        size_type pos1, size_type n1, core::string_view str ) const
    {
        return s_.compare(pos1, n1, str);
    }

    /** Return the result of comparing to another string

        See `core::string_view::compare`

        @param pos1 The position to start comparing from
        @param n1 The number of characters to compare
        @param str The string to compare
        @param pos2 The position to start comparing from
        @param n2 The number of characters to compare
        @return The result of the comparison
    */
    BOOST_CONSTEXPR int compare(
        size_type pos1, size_type n1, core::string_view str,
        size_type pos2, size_type n2 ) const
    {
        return s_.compare(pos1, n1, str, pos2, n2);
    }

    /** Return the result of comparing to another string

        See `core::string_view::compare`

        @param s The string to compare
        @return The result of the comparison
    */
    BOOST_CONSTEXPR int compare(
        char const* s ) const noexcept
    {
        return s_.compare(s);
    }

    /** Return the result of comparing to another string

        See `core::string_view::compare`

        @param pos1 The position to start comparing from
        @param n1 The number of characters to compare
        @param s The string to compare
        @return The result of the comparison
    */
    BOOST_CONSTEXPR int compare(
        size_type pos1, size_type n1, char const* s ) const
    {
        return s_.compare(pos1, n1, s);
    }

    /** Return the result of comparing to another string

        See `core::string_view::compare`

        @param pos1 The position to start comparing from
        @param n1 The number of characters to compare
        @param s The string to compare
        @param n2 The number of characters to compare
        @return The result of the comparison
    */
    BOOST_CONSTEXPR int compare(
        size_type pos1, size_type n1,
        char const* s, size_type n2 ) const
    {
        return s_.compare(pos1, n1, s, n2);
    }

    // starts_with

    /** Return true if a matching prefix exists

        See `core::string_view::starts_with`

        @param x The string to search for
        @return `true` if the prefix matches
    */
    BOOST_CONSTEXPR bool starts_with(
        core::string_view x ) const noexcept
    {
        return s_.starts_with(x);
    }

    /** Return true if a matching prefix exists

        See `core::string_view::starts_with`

        @param x The character to search for
        @return `true` if the prefix matches
    */
    BOOST_CONSTEXPR bool starts_with(
        char x ) const noexcept
    {
        return s_.starts_with(x);
    }

    /** Return true if a matching prefix exists

        See `core::string_view::starts_with`

        @param x The string to search for
        @return `true` if the prefix matches
    */
    BOOST_CONSTEXPR bool starts_with(
        char const* x ) const noexcept
    {
        return s_.starts_with(x);
    }

    // ends_with

    /** Return true if a matching suffix exists

        See `core::string_view::ends_with`

        @param x The string to search for
        @return `true` if the suffix matches
    */
    BOOST_CONSTEXPR bool ends_with(
        core::string_view x ) const noexcept
    {
        return s_.ends_with(x);
    }

    /** Return true if a matching suffix exists

        See `core::string_view::ends_with`

        @param x The character to search for
        @return `true` if the suffix matches
    */
    BOOST_CONSTEXPR bool ends_with(
        char x ) const noexcept
    {
        return s_.ends_with(x);
    }

    /** Return true if a matching suffix exists

        See `core::string_view::ends_with`

        @param x The string to search for
        @return `true` if the suffix matches
    */
    BOOST_CONSTEXPR bool ends_with(
        char const* x ) const noexcept
    {
        return s_.ends_with(x);
    }

    // find

    /** Return the position of matching characters

        See `core::string_view::find`

        @param str The characters to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CONSTEXPR size_type find(
        core::string_view str, size_type pos = 0 ) const noexcept
    {
        return s_.find(str, pos);
    }

    /** Return the position of matching characters

        See `core::string_view::find`

        @param c The character to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type find(
        char c, size_type pos = 0 ) const noexcept
    {
        return s_.find(c, pos);
    }

    /** Return the position of matching characters

        See `core::string_view::find`

        @param s The characters to search for
        @param pos The position to start searching from
        @param n The number of characters to search for
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type find(
        char const* s, size_type pos, size_type n ) const noexcept
    {
        return s_.find(s, pos, n);
    }

    /** Return the position of matching characters

        See `core::string_view::find`

        @param s The characters to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CONSTEXPR size_type find(
        char const* s, size_type pos = 0 ) const noexcept
    {
        return s_.find(s, pos);
    }

    // rfind

    /** Return the position of matching characters

        See `core::string_view::rfind`

        @param str The characters to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CONSTEXPR size_type rfind(
        core::string_view str, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.rfind(str, pos);
    }

    /** Return the position of matching characters

        See `core::string_view::rfind`

        @param c The character to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type rfind(
        char c, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.rfind(c, pos);
    }

    /** Return the position of matching characters

        See `core::string_view::rfind`

        @param s The characters to search for
        @param pos The position to start searching from
        @param n The number of characters to search for
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type rfind(
        char const* s, size_type pos, size_type n ) const noexcept
    {
        return s_.rfind(s, pos, n);
    }

    /** Return the position of matching characters

        See `core::string_view::rfind`

        @param s The characters to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CONSTEXPR size_type rfind(
        char const* s, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.rfind(s, pos);
    }

    // find_first_of

    /** Return the position of the first match

        See `core::string_view::find_first_of`

        @param str The characters to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_of(
        core::string_view str, size_type pos = 0 ) const noexcept
    {
        return s_.find_first_of(str, pos);
    }

    /** Return the position of the first match

        See `core::string_view::find_first_of`

        @param c The character to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CONSTEXPR size_type find_first_of(
        char c, size_type pos = 0 ) const noexcept
    {
        return s_.find_first_of(c, pos);
    }

    /** Return the position of the first match

        See `core::string_view::find_first_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @param n The number of characters to search for
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_of(
        char const* s, size_type pos, size_type n ) const noexcept
    {
        return s_.find_first_of(s, pos, n);
    }

    /** Return the position of the first match

        See `core::string_view::find_first_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @return The position of the first match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_of(
        char const* s, size_type pos = 0 ) const noexcept
    {
        return s_.find_first_of(s, pos);
    }

    // find_last_of

    /** Return the position of the last match

        See `core::string_view::find_last_of`

        @param str The characters to search for
        @param pos The position to start searching from
        @return The position of the last match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_of(
        core::string_view str, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.find_last_of(str, pos);
    }

    /** Return the position of the last match

        See `core::string_view::find_last_of`

        @param c The character to search for
        @param pos The position to start searching from
        @return The position of the last match
    */
    BOOST_CONSTEXPR size_type find_last_of(
        char c, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.find_last_of(c, pos);
    }

    /** Return the position of the last match

        See `core::string_view::find_last_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @param n The number of characters to search for
        @return The position of the last match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_of(
        char const* s, size_type pos, size_type n ) const noexcept
    {
        return s_.find_last_of(s, pos, n);
    }

    /** Return the position of the last match

        See `core::string_view::find_last_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @return The position of the last match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_of(
        char const* s, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.find_last_of(s, pos);
    }

    // find_first_not_of

    /** Return the position of the first non-match

        See `core::string_view::find_first_not_of`

        @param str The characters to search for
        @param pos The position to start searching from
        @return The position of the first non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_not_of(
        core::string_view str, size_type pos = 0 ) const noexcept
    {
        return s_.find_first_not_of(str, pos);
    }

    /** Return the position of the first non-match

        See `core::string_view::find_first_not_of`

        @param c The character to search for
        @param pos The position to start searching from
        @return The position of the first non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_not_of(
        char c, size_type pos = 0 ) const noexcept
    {
        return s_.find_first_not_of(c, pos);
    }

    /** Return the position of the first non-match

        See `core::string_view::find_first_not_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @param n The number of characters to search for
        @return The position of the first non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_not_of(
        char const* s, size_type pos, size_type n ) const noexcept
    {
        return s_.find_first_not_of(s, pos, n);
    }

    /** Return the position of the first non-match

        See `core::string_view::find_first_not_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @return The position of the first non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_first_not_of(
        char const* s, size_type pos = 0 ) const noexcept
    {
        return s_.find_first_not_of(s, pos);
    }

    // find_last_not_of

    /** Return the position of the last non-match

        See `core::string_view::find_last_not_of`

        @param str The characters to search for
        @param pos The position to start searching from
        @return The position of the last non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_not_of(
        core::string_view str, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.find_last_not_of(str, pos);
    }

    /** Return the position of the last non-match

        See `core::string_view::find_last_not_of`

        @param c The character to search for
        @param pos The position to start searching from
        @return The position of the last non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_not_of(
        char c, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.find_last_not_of(c, pos);
    }

    /** Return the position of the last non-match

        See `core::string_view::find_last_not_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @param n The number of characters to search for
        @return The position of the last non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_not_of(
        char const* s, size_type pos, size_type n ) const noexcept
    {
        return s_.find_last_not_of(s, pos, n);
    }

    /** Return the position of the last non-match

        See `core::string_view::find_last_not_of`

        @param s The characters to search for
        @param pos The position to start searching from
        @return The position of the last non-match
    */
    BOOST_CXX14_CONSTEXPR size_type find_last_not_of(
        char const* s, size_type pos = core::string_view::npos ) const noexcept
    {
        return s_.find_last_not_of(s, pos);
    }

    // contains

    /** Return true if matching characters are found

        See `core::string_view::contains`

        @param sv The string to search for
        @return `true` if the string contains the characters, otherwise `false`
    */
    BOOST_CONSTEXPR bool contains( core::string_view sv ) const noexcept
    {
        return s_.contains(sv);
    }

    /** Return true if matching characters are found

        See `core::string_view::contains`

        @param c The character to search for
        @return `true` if the string contains the character, otherwise `false`
    */
    BOOST_CXX14_CONSTEXPR bool contains( char c ) const noexcept
    {
        return s_.contains(c);
    }

    /** Return true if matching characters are found

        See `core::string_view::contains`

        @param s The string to search for
        @return `true` if the string contains the characters, otherwise `false`
    */
    BOOST_CONSTEXPR bool contains( char const* s ) const noexcept
    {
        return s_.contains(s);
    }

    // relational operators
#ifndef BOOST_URL_DOCS
private:
    template<class S0, class S1>
    using is_match = std::integral_constant<bool,
        std::is_convertible<S0, core::string_view>::value &&
        std::is_convertible<S1, core::string_view>::value && (
            (std::is_base_of<string_view_base,
                typename std::decay<S0>::type>::value &&
            std::is_convertible<S0 const volatile*,
                string_view_base const volatile*>::value) ||
            (std::is_base_of<string_view_base,
                typename std::decay<S1>::type>::value &&
            std::is_convertible<S1 const volatile*,
                string_view_base const volatile*>::value))>;
public:

    /** Compare two string views for equality

        This function is only enabled if both arguments
        are convertible to `core::string_view` and at least
        one of the arguments is derived from `string_view_base`.

        @param s0 The first string
        @param s1 The second string
        @return `true` if the strings are equal, otherwise `false`
     */
    template<class S0, class S1>
    BOOST_CXX14_CONSTEXPR friend auto operator==(
        S0 const& s0, S1 const& s1) noexcept ->
        typename std::enable_if<
            is_match<S0, S1>::value, bool>::type
    {
        return urls::detail::to_sv(s0) == urls::detail::to_sv(s1);
    }

    /** Compare two string views for inequality

        This function is only enabled if both arguments
        are convertible to `core::string_view` and at least
        one of the arguments is derived from `string_view_base`.

        @param s0 The first string
        @param s1 The second string
        @return `true` if the strings are not equal, otherwise `false`
     */
    template<class S0, class S1>
    BOOST_CXX14_CONSTEXPR friend auto operator!=(
        S0 const& s0, S1 const& s1) noexcept ->
        typename std::enable_if<
            is_match<S0, S1>::value, bool>::type
    {
        return urls::detail::to_sv(s0) != urls::detail::to_sv(s1);
    }

    /** Compare two string views for less than

        This function is only enabled if both arguments
        are convertible to `core::string_view` and at least
        one of the arguments is derived from `string_view_base`.

        @param s0 The first string
        @param s1 The second string
        @return `true` if the first string is less than the second, otherwise `false`
     */
    template<class S0, class S1>
    BOOST_CXX14_CONSTEXPR friend auto operator<(
        S0 const& s0, S1 const& s1) noexcept ->
        typename std::enable_if<
            is_match<S0, S1>::value, bool>::type
    {
        return urls::detail::to_sv(s0) < urls::detail::to_sv(s1);
    }

    /** Compare two string views for less than or equal

        This function is only enabled if both arguments
        are convertible to `core::string_view` and at least
        one of the arguments is derived from `string_view_base`.

        @param s0 The first string
        @param s1 The second string
        @return `true` if the first string is less than or equal to the second, otherwise `false`
     */
    template<class S0, class S1>
    BOOST_CXX14_CONSTEXPR friend auto operator<=(
        S0 const& s0, S1 const& s1) noexcept ->
        typename std::enable_if<
            is_match<S0, S1>::value, bool>::type
    {
        return urls::detail::to_sv(s0) <= urls::detail::to_sv(s1);
    }

    /** Compare two string views for greater than

        This function is only enabled if both arguments
        are convertible to `core::string_view` and at least
        one of the arguments is derived from `string_view_base`.

        @param s0 The first string
        @param s1 The second string
        @return `true` if the first string is greater than the second, otherwise `false`
     */
    template<class S0, class S1>
    BOOST_CXX14_CONSTEXPR friend auto operator>(
        S0 const& s0, S1 const& s1) noexcept ->
        typename std::enable_if<
            is_match<S0, S1>::value, bool>::type
    {
        return urls::detail::to_sv(s0) > urls::detail::to_sv(s1);
    }

    /** Compare two string views for greater than or equal

        This function is only enabled if both arguments
        are convertible to `core::string_view` and at least
        one of the arguments is derived from `string_view_base`.

        @param s0 The first string
        @param s1 The second string
        @return `true` if the first string is greater than or equal to the second, otherwise `false`
     */
    template<class S0, class S1>
    BOOST_CXX14_CONSTEXPR friend auto operator>=(
        S0 const& s0, S1 const& s1) noexcept ->
        typename std::enable_if<
            is_match<S0, S1>::value, bool>::type
    {
        return urls::detail::to_sv(s0) >= urls::detail::to_sv(s1);
    }
#endif

    //--------------------------------------------

    /** Format a string to an output stream

        @param os The output stream to write to
        @param s The string to write
        @return A reference to the output stream, for chaining
     */
    BOOST_URL_DECL
    friend
    std::ostream&
    operator<<(
        std::ostream& os,
        string_view_base const& s);
};

//------------------------------------------------

/** Format a string to an output stream
*/
BOOST_URL_DECL
std::ostream&
operator<<(
    std::ostream& os,
    string_view_base const& s);

} // grammar

#ifndef BOOST_URL_DOCS
namespace detail {
template <>
inline
core::string_view
to_sv(grammar::string_view_base const& s) noexcept
{
    return s.operator core::string_view();
}
} // detail
#endif

} // urls
} // boost

#endif
