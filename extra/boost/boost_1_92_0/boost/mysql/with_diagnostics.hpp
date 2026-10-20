//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_WITH_DIAGNOSTICS_HPP
#define BOOST_MYSQL_WITH_DIAGNOSTICS_HPP

#include <boost/mysql/detail/access.hpp>

#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

/**
 * \brief A completion token adapter used to include server diagnostics in exceptions.
 * \details
 * When passed to an async initiating function, transforms its handler signature from `void(error_code, T...)`
 * to `void(std::exception_ptr, T...)`.
 * Uses knowledge of Boost.MySQL internals to grab any \ref diagnostics
 * that the operation may produce to create a `std::exception_ptr` pointing to an \ref error_with_diagnostics
 * object. On success, the generated `std::exception_ptr` will be `nullptr`.
 *
 * Using `with_diagnostics` to wrap tokens that throw exceptions (like `deferred` + `co_await` or
 * `yield_context`) enhances the thrown exceptions with diagnostics information, matching the ones thrown by
 * sync functions. If you don't use this token, Asio will use `system_error` exceptions, containing less info.
 *
 * This token can only be used with operations involving Boost.MySQL, as it relies on its internals.
 *
 * Like `asio::as_tuple`, this class wraps another completion token. For instance,
 * `with_diagnostics(asio::deferred)` will generate a deferred operation with an adapted
 * signature, which will throw `error_with_diagnostics` when `co_await`'ed.
 *
 * If this token is applied to a function with a handler signature that
 * does not match `void(error_code, T...)`, the token acts as a pass-through:
 * it does not modify the signature, and calls the underlying token's initiation directly.
 * This has the following implications:
 *
 *   - `asio::as_tuple(with_diagnostics(X))` is equivalent to `asio::as_tuple(X)`.
 *   - `asio::redirect_error(with_diagnostics(X))` is equivalent to `asio::redirect_error(X)`.
 *   - Tokens like `asio::as_tuple` and `asio::redirect_error` can be used as partial tokens
 *     when `with_diagnostics` is the default completion token, as is the case for \ref any_connection.
 */
template <class CompletionToken>
class with_diagnostics_t
{
    CompletionToken impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    /**
     * \brief Default constructor.
     * \details
     * Only valid if `CompletionToken` is default-constructible.
     *
     * \par Exception safety
     * Strong guarantee. Any exceptions thrown by default-constructing
     * `CompletionToken` are propagated.
     */
    constexpr with_diagnostics_t() : impl_{} {}

    /**
     * \brief Constructor.
     * \details
     * The passed `token` should be convertible to `CompletionToken`.
     *
     * \par Exception safety
     * Strong guarantee. Any exceptions thrown by constructing `CompletionToken` are propagated.
     */
    template <class T>
    constexpr explicit with_diagnostics_t(T&& token) : impl_(std::forward<T>(token))
    {
    }
};

/**
 * \brief Creates a \ref with_diagnostics_t from a completion token.
 * \details
 * The passed token is decay-copied into the \ref with_diagnostics_t object.
 *
 * \par Exception safety
 * Strong guarantee. Any exceptions thrown by constructing `CompletionToken` are propagated.
 */
template <class CompletionToken>
constexpr with_diagnostics_t<typename std::decay<CompletionToken>::type> with_diagnostics(
    CompletionToken&& token
)
{
    return with_diagnostics_t<typename std::decay<CompletionToken>::type>(std::forward<CompletionToken>(token)
    );
}

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/with_diagnostics.hpp>

#endif
