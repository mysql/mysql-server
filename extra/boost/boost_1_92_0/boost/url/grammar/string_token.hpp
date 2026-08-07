//
// Copyright (c) 2021 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_STRING_TOKEN_HPP
#define BOOST_URL_GRAMMAR_STRING_TOKEN_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/detail/except.hpp>
#include <memory>
#include <string>

namespace boost {
namespace urls {
namespace string_token {

/** Base class for string tokens, and algorithm parameters

    This abstract interface provides a means
    for an algorithm to generically obtain a
    modifiable, contiguous character buffer
    of prescribed size.

    A @ref StringToken should be derived
    from this class. As the author of an
    algorithm using a @ref StringToken,
    simply declare an rvalue reference
    as a parameter type.

    Instances of this type are intended only
    to be used once and then destroyed.

    @par Example
    The declared function accepts any
    temporary instance of `arg` to be
    used for writing:
    @code
    void algorithm( string_token::arg&& dest );
    @endcode

    To implement the interface for your type
    or use-case, derive from the class and
    implement the prepare function.
*/
struct arg
{
    /** Return a modifiable character buffer

        This function attempts to obtain a
        character buffer with space for at
        least `n` characters. Upon success,
        a pointer to the beginning of the
        buffer is returned. Ownership is not
        transferred; the caller should not
        attempt to free the storage. The
        buffer shall remain valid until
        `this` is destroyed.

        @note
        This function may only be called once.
        After invoking the function, the only
        valid operation is destruction.

        @param n The number of characters needed
        @return A pointer to the buffer
    */
    virtual char* prepare(std::size_t n) = 0;

    /// Virtual destructor
    virtual ~arg() = default;

    /// Default constructor
    arg() = default;

    /// Default move constructor
    arg(arg&&) = default;

    /// Deleted copy constructor
    arg(arg const&) = delete;

    /// Deleted move assignment
    arg& operator=(arg&&) = delete;

    /// Deleted copy assignment
    arg& operator=(arg const&) = delete;
};

//------------------------------------------------

namespace implementation_defined {
template<class T, class = void>
struct is_token : std::false_type {};

template<class T>
struct is_token<T, void_t<
    decltype(std::declval<T&>().prepare(
        std::declval<std::size_t>())),
    decltype(std::declval<T&>().result())
    > > : std::integral_constant<bool,
        std::is_convertible<decltype(
            std::declval<T&>().result()),
            typename T::result_type>::value &&
        std::is_same<decltype(
            std::declval<T&>().prepare(0)),
            char*>::value &&
        std::is_base_of<arg, T>::value &&
        std::is_convertible<T const volatile*,
            arg const volatile*>::value
    >
{
};
} // implementation_defined

/** Trait to determine if a type is a string token

    This trait returns `true` if `T` is a valid
    @ref StringToken type, and `false` otherwise.

    @par Example
    @code
    static_assert( string_token::is_token<T>::value );
    @endcode
 */
template<class T>
using is_token = implementation_defined::is_token<T>;

#ifdef BOOST_URL_HAS_CONCEPTS
/** Concept for a string token

    This concept is satisfied if `T` is a
    valid string token type.

    A string token is an rvalue passed to a function template
    which customizes the return type of the function and also
    controls how a modifiable character buffer is obtained and presented.

    The string token's lifetime extends only for the duration of the
    function call in which it appears as a parameter.

    A string token cannot be copied, moved, or assigned, and must be
    destroyed when the function returns or throws.

    @par Semantics

    `T::result_type` determines the return type of functions
    that accept a string token.

    The `prepare()` function overrides the virtual function
    in the base class @ref arg. It must return a pointer to
    a character buffer of at least size `n`, otherwise
    throw an exception. This function is called only
    once or not at all.

    The `result()` function is invoked by the algorithm
    to receive the result from the string token.
    It is only invoked if `prepare()` returned
    successfully and the string token was not destroyed.
    It is only called after `prepare()` returns
    successfully, and the string token is destroyed
    when the algorithm completes or if an exception
    is thrown.

    String tokens cannot be reused.

    @par Exemplars
    String token prototype:

    @code
    struct StringToken : string_token::arg
    {
        using result_type = std::string;

        char* prepare( std::size_t n ) override;

        result_type result();
    };
    @endcode

    Algorithm prototype:

    @code
    namespace detail {

    // Algorithm implementation may be placed
    // out of line, and written as an ordinary
    // function (no template required).
    void algorithm_impl( string_token::arg& token )
    {
        std::size_t n = 0;

        // calculate space needed in n
        // ...

        // acquire a destination buffer
        char* dest = token.prepare( n );

        // write the characters to the buffer
    }
    } // detail

    // public interface is a function template,
    // defaulting to return std::string.
    template< class StringToken = string_token::return_string >
    auto
    algorithm( StringToken&& token = {} ) ->
        typename StringToken::result_type
    {
        // invoke the algorithm with the token
        algorithm_impl( token );

        // return the result from the token
        return token.result();
    }
    @endcode

    @par Models
    The following classes and functions implement and
    generate string tokens.

    @li @ref return_string
    @li @ref assign_to
    @li @ref preserve_size

 */
template <class T>
concept StringToken =
    std::derived_from<T, string_token::arg> &&
    requires (T t, std::size_t n)
{
    typename T::result_type;
    { t.prepare(n) } -> std::same_as<char*>;
    { t.result() } -> std::convertible_to<typename T::result_type>;
};
#endif

//------------------------------------------------

namespace implementation_defined {
struct return_string
    : arg
{
    using result_type = std::string;

    char*
    prepare(std::size_t n) override
    {
        s_.resize(n);
        return &s_[0];
    }

    result_type
    result() noexcept
    {
        return std::move(s_);
    }

private:
    result_type s_;
};
} // implementation_defined

/** A string token for returning a plain string

    This @ref StringToken is used to customize
    a function to return a plain string.

    This is default token type used by
    the methods of @ref url_view_base
    that return decoded strings.
 */
using return_string = implementation_defined::return_string;

//------------------------------------------------

namespace implementation_defined {
template<class Alloc>
struct append_to_t
    : arg
{
    using string_type = std::basic_string<
        char, std::char_traits<char>,
            Alloc>;

    using result_type = string_type&;

    explicit
    append_to_t(
        string_type& s) noexcept
        : s_(s)
    {
    }

    char*
    prepare(std::size_t n) override
    {
        std::size_t n0 = s_.size();
        if(n > s_.max_size() - n0)
            urls::detail::throw_length_error();
        s_.resize(n0 + n);
        return &s_[n0];
    }

    result_type
    result() noexcept
    {
        return s_;
    }

private:
    string_type& s_;
};
} // implementation_defined

/** Create a string token for appending to a plain string

    This function creates a @ref StringToken
    which appends to an existing plain string.

    Functions using this token will append
    the result to the existing string and
    return a reference to it.

    @param s The string to append
    @return A string token
 */
template<
    class Alloc =
        std::allocator<char>>
implementation_defined::append_to_t<Alloc>
append_to(
    std::basic_string<
        char,
        std::char_traits<char>,
        Alloc>& s)
{
    return implementation_defined::append_to_t<Alloc>(s);
}

//------------------------------------------------

namespace implementation_defined {
template<class Alloc>
struct assign_to_t
    : arg
{
    using string_type = std::basic_string<
        char, std::char_traits<char>,
            Alloc>;

    using result_type = string_type&;

    explicit
    assign_to_t(
        string_type& s) noexcept
        : s_(s)
    {
    }

    char*
    prepare(std::size_t n) override
    {
        s_.resize(n);
        return &s_[0];
    }

    result_type
    result() noexcept
    {
        return s_;
    }

private:
    string_type& s_;
};
} // implementation_defined

/** Create a string token for assigning to a plain string

    This function creates a @ref StringToken
    which assigns to an existing plain string.

    Functions using this token will assign
    the result to the existing string and
    return a reference to it.

    @param s The string to assign
    @return A string token
 */
template<
    class Alloc =
        std::allocator<char>>
implementation_defined::assign_to_t<Alloc>
assign_to(
    std::basic_string<
        char,
        std::char_traits<char>,
        Alloc>& s)
{
    return implementation_defined::assign_to_t<Alloc>(s);
}

//------------------------------------------------

namespace implementation_defined {
template<class Alloc>
struct preserve_size_t
    : arg
{
    using result_type = core::string_view;

    using string_type = std::basic_string<
        char, std::char_traits<char>,
            Alloc>;

    explicit
    preserve_size_t(
        string_type& s) noexcept
        : s_(s)
    {
    }

    char*
    prepare(std::size_t n) override
    {
        n_ = n;
        // preserve size() to
        // avoid value-init
        if(s_.size() < n)
            s_.resize(n);
        return &s_[0];
    }

    result_type
    result() noexcept
    {
        return core::string_view(
            s_.data(), n_);
    }

private:
    string_type& s_;
    std::size_t n_ = 0;
};
} // implementation_defined

/** Create a string token for a durable core::string_view

    This function creates a @ref StringToken
    which assigns to an existing plain string.

    Functions using this token will assign
    the result to the existing string and
    return a `core::string_view` to it.

    @param s The string to preserve
    @return A string token
 */
template<
    class Alloc =
        std::allocator<char>>
implementation_defined::preserve_size_t<Alloc>
preserve_size(
    std::basic_string<
        char,
        std::char_traits<char>,
        Alloc>& s)
{
    return implementation_defined::preserve_size_t<Alloc>(s);
}
} // string_token

namespace grammar {
namespace string_token = ::boost::urls::string_token;
} // grammar

} // urls
} // boost

#endif
