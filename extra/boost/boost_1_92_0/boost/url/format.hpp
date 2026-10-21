//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_FORMAT_HPP
#define BOOST_URL_FORMAT_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/url.hpp>
#include <boost/url/detail/vformat.hpp>
#include <initializer_list>

#ifdef BOOST_URL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace urls {

/** A temporary reference to a named formatting argument

    This class represents a temporary reference
    to a named formatting argument used by the
    @ref format function.

    Named arguments should always be created
    with the @ref arg function.

    Any type that can be formatted into a URL
    with the @ref format function can also be used
    in a named argument. All named arguments
    are convertible to @ref format_arg and
    can be used in the @ref format function.

    @see
        @ref arg,
        @ref format,
        @ref format_to,
        @ref format_arg.
  */
template <class T>
using named_arg = detail::named_arg<T>;

/** A temporary reference to a formatting argument

    This class represents a temporary reference
    to a formatting argument used by the
    @ref format function.

    A @ref format argument should always be
    created by passing the argument to be
    formatted directly to the @ref format function.

    Any type that can be formatted into a URL
    with the @ref format function is convertible
    to this type.

    This includes basic types, types convertible
    to `core::string_view`, and @ref named_arg.

    @see
        @ref format,
        @ref format_to,
        @ref arg.
  */
using format_arg = detail::format_arg;

/** Format arguments into a URL

    Format arguments according to the format
    URL string into a @ref url.

    The rules for a format URL string are the same
    as for a `std::format_string`, where replacement
    fields are delimited by curly braces.

    The URL components to which replacement fields
    belong are identified before replacement is
    applied and any invalid characters for that
    formatted argument are percent-escaped.

    Hence, the delimiters between URL components,
    such as `:`, `//`, `?`, and `#`, should be
    included in the URL format string. Likewise,
    a format string with a single `"{}"` is
    interpreted as a path and any replacement
    characters invalid in this component will be
    encoded to form a valid URL.

    @par Example
    @code
    assert(format("{}://{}:{}/rfc/{}",
        "https", "www.ietf.org", 80, "rfc2396.txt"
        ).buffer() == "https://www.ietf.org:80/rfc/rfc2396.txt");
    @endcode

    Arguments that contain special characters are
    automatically percent-encoded for the URL
    component where they appear:

    @code
    assert(format("https://example.com/~{}",
        "John Doe"
        ).buffer() == "https://example.com/~John%20Doe");
    @endcode

    @note
    The formatting machinery relies on language and library
    features that are broken on GCC 4.8 and GCC 5.x, so this
    function is not supported on those compilers.

    @par Preconditions
    All replacement fields must be valid and the
    resulting URL should be valid after arguments
    are formatted into the URL.

    Because any invalid characters for a URL
    component are encoded by this function, only
    replacements in the scheme and port components
    might be invalid, as these components do not
    allow percent-encoding of arbitrary
    characters.

    @return A URL holding the formatted result.

    @param fmt The format URL string.
    @param args Arguments to be formatted.

    @throws system_error
    `fmt` contains an invalid format string and
    the result contains an invalid URL after
    replacements are applied.

    @par BNF
    @code
    replacement_field ::=  "{" [arg_id] [":" (format_spec | chrono_format_spec)] "}"
    arg_id            ::=  integer | identifier
    integer           ::=  digit+
    digit             ::=  "0"..."9"
    identifier        ::=  id_start id_continue*
    id_start          ::=  "a"..."z" | "A"..."Z" | "_"
    id_continue       ::=  id_start | digit
    @endcode

    @par Specification
    @li <a href="https://fmt.dev/latest/syntax.html"
        >Format String Syntax</a>

    @see
        @ref format_to,
        @ref arg.
*/
template <BOOST_URL_CONSTRAINT(std::convertible_to<format_arg>)... Args>
url
format(
    core::string_view fmt,
    Args&&... args)
{
    return detail::vformat(
        fmt, detail::make_format_args(
            std::forward<Args>(args)...));
}

/** Format arguments into a URL

    Format arguments according to the format
    URL string into a @ref url_base.

    The rules for a format URL string are the same
    as for a `std::format_string`, where replacement
    fields are delimited by curly braces.

    The URL components to which replacement fields
    belong are identified before replacement is
    applied and any invalid characters for that
    formatted argument are percent-escaped.

    Hence, the delimiters between URL components,
    such as `:`, `//`, `?`, and `#`, should be
    included in the URL format string. Likewise,
    a format string with a single `"{}"` is
    interpreted as a path and any replacement
    characters invalid in this component will be
    encoded to form a valid URL.

    @par Example
    @code
    static_url<50> u;
    format_to(u, "{}://{}:{}/rfc/{}",
        "https", "www.ietf.org", 80, "rfc2396.txt");
    assert(u.buffer() == "https://www.ietf.org:80/rfc/rfc2396.txt");
    @endcode

    @par Preconditions
    All replacement fields must be valid and the
    resulting URL should be valid after arguments
    are formatted into the URL.

    Because any invalid characters for a URL
    component are encoded by this function, only
    replacements in the scheme and port components
    might be invalid, as these components do not
    allow percent-encoding of arbitrary
    characters.

    @par Exception Safety
    Strong guarantee.

    @param u An object that derives from @ref url_base.
    @param fmt The format URL string.
    @param args Arguments to be formatted.

    @throws system_error
    `fmt` contains an invalid format string and
    `u` contains an invalid URL after replacements
    are applied.

    @par BNF
    @code
    replacement_field ::=  "{" [arg_id] [":" (format_spec | chrono_format_spec)] "}"
    arg_id            ::=  integer | identifier
    integer           ::=  digit+
    digit             ::=  "0"..."9"
    identifier        ::=  id_start id_continue*
    id_start          ::=  "a"..."z" | "A"..."Z" | "_"
    id_continue       ::=  id_start | digit
    @endcode

    @par Specification
    @li <a href="https://fmt.dev/latest/syntax.html"
        >Format String Syntax</a>

    @see
        @ref format.

*/
template <BOOST_URL_CONSTRAINT(std::convertible_to<format_arg>)... Args>
void
format_to(
    url_base& u,
    core::string_view fmt,
    Args&&... args)
{
    detail::vformat_to(
        u, fmt, detail::make_format_args(
            std::forward<Args>(args)...));
}

/** Format arguments into a URL

    Format arguments according to the format
    URL string into a @ref url.

    This overload allows type-erased arguments
    to be passed as an initializer_list, which
    is mostly convenient for named parameters.

    All arguments must be convertible to a
    implementation defined type able to store a
    type-erased reference to any valid format
    argument.

    The rules for a format URL string are the same
    as for a `std::format_string`, where replacement
    fields are delimited by curly braces.

    The URL components to which replacement fields
    belong are identified before replacement is
    applied and any invalid characters for that
    formatted argument are percent-escaped.

    Hence, the delimiters between URL components,
    such as `:`, `//`, `?`, and `#`, should be
    included in the URL format string. Likewise,
    a format string with a single `"{}"` is
    interpreted as a path and any replacement
    characters invalid in this component will be
    encoded to form a valid URL.

    @par Example
    @code
    assert(format(
        "{scheme}://{host}:{port}/{dir}/{file}",
        {{"scheme", "https"}, {"port", 80},
         {"host", "example.com"},
         {"dir", "path/to"},
         {"file", "file.txt"}}
        ).buffer() == "https://example.com:80/path/to/file.txt");
    @endcode

    @par Preconditions
    All replacement fields must be valid and the
    resulting URL should be valid after arguments
    are formatted into the URL.

    Because any invalid characters for a URL
    component are encoded by this function, only
    replacements in the scheme and port components
    might be invalid, as these components do not
    allow percent-encoding of arbitrary
    characters.

    @return A URL holding the formatted result.

    @param fmt The format URL string.
    @param args Arguments to be formatted.

    @throws system_error
    `fmt` contains an invalid format string and
    the result contains an invalid URL after
    replacements are applied.

    @par BNF
    @code
    replacement_field ::=  "{" [arg_id] [":" (format_spec | chrono_format_spec)] "}"
    arg_id            ::=  integer | identifier
    integer           ::=  digit+
    digit             ::=  "0"..."9"
    identifier        ::=  id_start id_continue*
    id_start          ::=  "a"..."z" | "A"..."Z" | "_"
    id_continue       ::=  id_start | digit
    @endcode

    @par Specification
    @li <a href="https://fmt.dev/latest/syntax.html"
        >Format String Syntax</a>

    @see
        @ref format_to.

*/
inline
url
format(
    core::string_view fmt,
    std::initializer_list<format_arg> args)
{
    return detail::vformat(
        fmt, detail::format_args(
            args.begin(), args.end()));
}

/** Format arguments into a URL

    Format arguments according to the format
    URL string into a @ref url_base.

    This overload allows type-erased arguments
    to be passed as an initializer_list, which
    is mostly convenient for named parameters.

    All arguments must be convertible to a
    implementation defined type able to store a
    type-erased reference to any valid format
    argument.

    The rules for a format URL string are the same
    as for a `std::format_string`, where replacement
    fields are delimited by curly braces.

    The URL components to which replacement fields
    belong are identified before replacement is
    applied and any invalid characters for that
    formatted argument are percent-escaped.

    Hence, the delimiters between URL components,
    such as `:`, `//`, `?`, and `#`, should be
    included in the URL format string. Likewise,
    a format string with a single `"{}"` is
    interpreted as a path and any replacement
    characters invalid in this component will be
    encoded to form a valid URL.

    @par Example
    @code
    url u;
    format_to(u,
        "{scheme}://{host}:{port}/{dir}/{file}",
        {{"scheme", "https"}, {"port", 80},
         {"host", "example.com"},
         {"dir", "path/to"},
         {"file", "file.txt"}});
    assert(u.buffer() == "https://example.com:80/path/to/file.txt");
    @endcode

    @par Preconditions
    All replacement fields must be valid and the
    resulting URL should be valid after arguments
    are formatted into the URL.

    Because any invalid characters for a URL
    component are encoded by this function, only
    replacements in the scheme and port components
    might be invalid, as these components do not
    allow percent-encoding of arbitrary
    characters.

    @par Exception Safety
    Strong guarantee.

    @param u An object that derives from @ref url_base.
    @param fmt The format URL string.
    @param args Arguments to be formatted.

    @throws system_error
    `fmt` contains an invalid format string and
    `u` contains an invalid URL after replacements
    are applied.

    @par BNF
    @code
    replacement_field ::=  "{" [arg_id] [":" (format_spec | chrono_format_spec)] "}"
    arg_id            ::=  integer | identifier
    integer           ::=  digit+
    digit             ::=  "0"..."9"
    identifier        ::=  id_start id_continue*
    id_start          ::=  "a"..."z" | "A"..."Z" | "_"
    id_continue       ::=  id_start | digit
    @endcode

    @par Specification
    @li <a href="https://fmt.dev/latest/syntax.html"
        >Format String Syntax</a>

    @see
        @ref format.

*/
inline
void
format_to(
    url_base& u,
    core::string_view fmt,
    std::initializer_list<format_arg> args)
{
    detail::vformat_to(
        u, fmt, detail::format_args(
            args.begin(), args.end()));
}

/** Designate a named argument for a replacement field

    Construct a named argument for a format URL
    string that contains named replacement fields.

    The function parameters should be convertible
    to an implementation defined type able to
    store the name and a reference to any type
    potentially used as a format argument.

    @par Example
    @code
    assert(format(
        "https://example.com/~{username}",
        arg("username", "mark")
        ).buffer() == "https://example.com/~mark");
    @endcode

    @return A temporary object with reference
    semantics for a named argument

    @param name The format argument name
    @param arg The format argument value

    @see
        @ref format,
        @ref format_to.

*/
template <class T>
named_arg<T>
arg(core::string_view name, T const& arg)
{
    return {name, arg};
}

} // url
} // boost

#endif
