//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ANY_CONNECTION_HPP
#define BOOST_MYSQL_ANY_CONNECTION_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/with_diagnostics.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connect_params_helpers.hpp>
#include <boost/mysql/detail/connection_impl.hpp>
#include <boost/mysql/detail/engine.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/ssl_fwd.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/assert.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/result.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

// Forward declarations
template <class... StaticRow>
class static_execution_state;

class pipeline_request;
class stage_response;

/**
 * \brief Configuration parameters that can be passed to \ref any_connection's constructor.
 */
struct any_connection_params
{
    /**
     * \brief An external SSL context containing options to configure TLS.
     * \details
     * Relevant only for SSL connections (those that result on \ref
     * any_connection::uses_ssl returning `true`).
     * \n
     * If the connection is configured to use TLS, an internal `asio::ssl::stream`
     * object will be created. If this member is set to a non-null value,
     * this internal object will be initialized using the passed context.
     * This is the only way to configure TLS options in `any_connection`.
     * \n
     * If the connection is configured to use TLS and this member is `nullptr`,
     * an internal `asio::ssl::context` object with suitable default options
     * will be created.
     *
     * \par Object lifetimes
     * If set to non-null, the pointee object must be kept alive until
     * all \ref any_connection objects constructed from `*this` are destroyed.
     */
    asio::ssl::context* ssl_context{};

    /**
     * \brief The initial size of the connection's buffer, in bytes.
     * \details A bigger read buffer can increase the number of rows
     * returned by \ref any_connection::read_some_rows.
     */
    std::size_t initial_buffer_size{default_initial_read_buffer_size};

    /**
     * \brief The maximum size of the connection's buffer, in bytes (64MB by default).
     * \details
     * Attempting to read or write a protocol packet bigger than this size
     * will fail with a \ref client_errc::max_buffer_size_exceeded error.
     * \n
     * This effectively means: \n
     *   - Each request sent to the server must be smaller than this value.
     *   - Each individual row received from the server must be smaller than this value.
     *     Note that when using `execute` or `async_execute`, results objects may
     *     allocate memory beyond this limit if the total number of rows is high.
     * \n
     * If you need to send or receive larger packets, you may need to adjust
     * your server's <a
     * href="https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_max_allowed_packet">`max_allowed_packet`</a>
     * system variable, too.
     */
    std::size_t max_buffer_size{0x4000000};
};

/**
 * \brief A connection to a MySQL server.
 * \details
 * Represents a connection to a MySQL server.
 * This is the main I/O object that this library implements. It's logically comprised
 * of session state and an internal stream (usually a socket). The stream is not directly
 * accessible. It's constructed using the executor passed to the constructor.
 *
 * This class supports establishing connections
 * with servers using TCP, TCP over TLS and UNIX sockets.
 *
 * The class is named `any_connection` because it's not templated on a `Stream`
 * type, as opposed to \ref connection. New code should prefer using `any_connection`
 * whenever possible.
 *
 * Compared to \ref connection, this class:
 *
 * - Is type-erased. The type of the connection doesn't depend on the transport being used.
 * - Is easier to connect, as \ref connect and \ref async_connect handle hostname resolution.
 * - Can always be re-connected after being used or encountering an error.
 * - Always uses `asio::any_io_executor`.
 * - Has the same level of performance.
 *
 * This is a move-only type.
 *
 * \par Single outstanding async operation per connection
 * At any given point in time, only one async operation can be outstanding
 * per connection. If an async operation is initiated while another one is in progress,
 * it will fail with \ref client_errc::operation_in_progress.
 *
 * \par Default completion tokens
 * The default completion token for all async operations in this class is
 * `with_diagnostics(asio::deferred)`, which allows you to use `co_await`
 * and have the expected exceptions thrown on error.
 *
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 * This class is <b>not thread-safe</b>: for a single object, if you
 * call its member functions concurrently from separate threads, you will get a race condition.
 */
class any_connection
{
    detail::connection_impl impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    BOOST_MYSQL_DECL
    static std::unique_ptr<detail::engine> create_engine(asio::any_io_executor ex, asio::ssl::context* ctx);

    // Used by tests
    any_connection(std::unique_ptr<detail::engine> eng, any_connection_params params)
        : impl_(params.initial_buffer_size, params.max_buffer_size, std::move(eng))
    {
    }

public:
    /**
     * \brief Constructs a connection object from an executor and an optional set of parameters.
     * \details
     * The resulting connection has `this->get_executor() == ex`. Any internally required I/O objects
     * will be constructed using this executor.
     * \n
     * You can configure extra parameters, like the SSL context and buffer sizes, by passing
     * an \ref any_connection_params object to this constructor.
     */
    any_connection(boost::asio::any_io_executor ex, any_connection_params params = {})
        : any_connection(create_engine(std::move(ex), params.ssl_context), params)
    {
    }

    /**
     * \brief Constructs a connection object from an execution context and an optional set of parameters.
     * \details
     * The resulting connection has `this->get_executor() == ctx.get_executor()`.
     * Any internally required I/O objects will be constructed using this executor.
     * \n
     * You can configure extra parameters, like the SSL context and buffer sizes, by passing
     * an \ref any_connection_params object to this constructor.
     * \n
     * This function participates in overload resolution only if `ExecutionContext`
     * satisfies the `ExecutionContext` requirements imposed by Boost.Asio.
     */
    template <
        class ExecutionContext
#ifndef BOOST_MYSQL_DOXYGEN
        ,
        class = typename std::enable_if<std::is_convertible<
            decltype(std::declval<ExecutionContext&>().get_executor()),
            asio::any_io_executor>::value>::type
#endif
        >
    any_connection(ExecutionContext& ctx, any_connection_params params = {})
        : any_connection(ctx.get_executor(), params)
    {
    }

    /**
     * \brief Move constructor.
     */
    any_connection(any_connection&& other) = default;

    /**
     * \brief Move assignment.
     */
    any_connection& operator=(any_connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    any_connection(const any_connection&) = delete;
    any_connection& operator=(const any_connection&) = delete;
#endif

    /**
     * \brief Destructor.
     * \details
     * Closes the connection at the transport layer (by closing any underlying socket objects).
     * If you require a clean close, call \ref close or \ref async_close before the connection
     * is destroyed.
     */
    ~any_connection() = default;

    /// The executor type associated to this object.
    using executor_type = asio::any_io_executor;

    /**
     * \brief Retrieves the executor associated to this object.
     * \par Exception safety
     * No-throw guarantee.
     */
    executor_type get_executor() noexcept { return impl_.get_engine().get_executor(); }

    /**
     * \brief Returns whether the connection negotiated the use of SSL or not.
     * \details
     * This function can be used to determine whether you are using a SSL
     * connection or not when using SSL negotiation.
     * \n
     * This function always returns `false`
     * for connections that haven't been established yet. If the connection establishment fails,
     * the return value is undefined.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool uses_ssl() const noexcept { return impl_.ssl_active(); }

    /**
     * \brief Returns whether backslashes are being treated as escape sequences.
     * \details
     * By default, the server treats backslashes in string values as escape characters.
     * This behavior can be disabled by activating the <a
     *   href="https://dev.mysql.com/doc/refman/8.0/en/sql-mode.html#sqlmode_no_backslash_escapes">`NO_BACKSLASH_ESCAPES`</a>
     * SQL mode.
     * \n
     * Every time an operation involving server communication completes, the server reports whether
     * this mode was activated or not as part of the response. Connections store this information
     * and make it available through this function.
     * \n
     * \li If backslash are treated like escape characters, returns `true`.
     * \li If `NO_BACKSLASH_ESCAPES` has been activated, returns `false`.
     * \li If connection establishment hasn't happened yet, returns `true`.
     * \li Calling this function while an async operation that changes backslash behavior
     *     is outstanding may return `true` or `false`.
     * \n
     * This function does not involve server communication.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool backslash_escapes() const noexcept { return impl_.backslash_escapes(); }

    /**
     * \brief Returns the character set used by this connection.
     * \details
     * Connections attempt to keep track of the current character set.
     * Deficiencies in the protocol can cause the character set to be unknown, though.
     * When the character set is known, this function returns
     * the character set currently in use. Otherwise, returns \ref client_errc::unknown_character_set.
     * \n
     * The following functions can modify the return value of this function: \n
     *   \li Prior to connection, the character set is always unknown.
     *   \li \ref connect and \ref async_connect may set the current character set
     *       to a known value, depending on the requested collation.
     *   \li \ref set_character_set always and \ref async_set_character_set always
     *       set the current character set to the passed value.
     *   \li \ref reset_connection and \ref async_reset_connection always makes the current character
     *       unknown.
     *
     * \par Avoid changing the character set directly
     * If you change the connection's character set directly using SQL statements
     * like `"SET NAMES utf8mb4"`, the client has no way to track this change,
     * and this function will return incorrect results.
     *
     * \par Errors
     * \li \ref client_errc::unknown_character_set if the current character set is unknown.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    system::result<character_set> current_character_set() const noexcept
    {
        return impl_.current_character_set();
    }

    /**
     * \brief Returns format options suitable to format SQL according to the current connection configuration.
     * \details
     * If the current character set is known (as given by \ref current_character_set), returns
     * a value suitable to be passed to SQL formatting functions. Otherwise, returns an error.
     *
     * \par Errors
     * \li \ref client_errc::unknown_character_set if the current character set is unknown.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    system::result<format_options> format_opts() const noexcept
    {
        auto res = current_character_set();
        if (res.has_error())
            return res.error();
        return format_options{res.value(), backslash_escapes()};
    }

    /**
     * \brief Returns the current metadata mode that this connection is using.
     * \details
     * \par Exception safety
     * No-throw guarantee.
     *
     * \returns The metadata mode that will be used for queries and statement executions.
     */
    metadata_mode meta_mode() const noexcept { return impl_.meta_mode(); }

    /**
     * \brief Sets the metadata mode.
     * \details
     * Will affect any query and statement executions performed after the call.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Preconditions
     * No asynchronous operation should be outstanding when this function is called.
     *
     * \param v The new metadata mode.
     */
    void set_meta_mode(metadata_mode v) noexcept { impl_.set_meta_mode(v); }

    /**
     * \brief Retrieves the connection id associated to the current session.
     * \details
     * If a session has been established, returns its associated connection id.
     * If no session has been established (i.e. \ref async_connect hasn't been called yet)
     * or the session has been terminated (i.e. \ref async_close has been called), an empty
     * optional is returned.
     *
     * The connection id is a 4 byte value that uniquely identifies a client session
     * at a given point in time. It can be used with the
     * <a href="https://dev.mysql.com/doc/refman/8.4/en/kill.html">`KILL`</a> SQL statement
     * to cancel queries and terminate connections.
     *
     * The server sends the connection id assigned to the current session as part of the
     * handshake process. The value is stored and made available through this function.
     * The same id can also be obtained by calling the
     * <a
     * href="https://dev.mysql.com/doc/refman/8.4/en/information-functions.html#function_connection-id">CONNECTION_ID()</a>
     * SQL function. However, this function is faster and more reliable, since it does not entail
     * communication with the server.
     *
     * This function is equivalent to the
     * <a href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-thread-id.html">`mysql_thread_id`</a> function
     * in the C connector. This function works properly in 64-bit systems, as opposed to what
     * the official docs suggest (see
     * <a href="https://dev.mysql.com/doc/relnotes/mysql/5.7/en/news-5-7-5.html">this changelog</a>).
     *
     * It is safe to call this function while an async operation is outstanding, except for \ref async_connect
     * and \ref async_close.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    boost::optional<std::uint32_t> connection_id() const noexcept { return impl_.connection_id(); }

    /**
     * \brief Establishes a connection to a MySQL server.
     * \details
     * This function performs the following:
     * \n
     * \li If a connection has already been established (by a previous call to \ref connect
     *     or \ref async_connect), closes it at the transport layer (by closing any underlying socket)
     *     and discards any protocol state associated to it. (If you require
     *     a clean close, call \ref close or \ref async_close before using this function).
     * \li If the connection is configured to use TCP (`params.server_address.type() ==
     *     address_type::host_and_port`), resolves the passed hostname to a set of endpoints. An empty
     *     hostname is equivalent to `"localhost"`.
     * \li Establishes the physical connection (performing the
     *     TCP or UNIX socket connect).
     * \li Performs the MySQL handshake to establish a session. If the
     *     connection is configured to use TLS, the TLS handshake is performed as part of this step.
     * \li If any of the above steps fail, the TCP or UNIX socket connection is closed.
     * \n
     * You can configure some options using the \ref connect_params struct.
     * \n
     * The decision to use TLS or not is performed using the following:
     * \n
     * \li If the transport is not TCP (`params.server_address.type() != address_type::host_and_port`),
     *     the connection will never use TLS.
     * \li If the transport is TCP, and `params.ssl == ssl_mode::disable`, the connection will not use TLS.
     * \li If the transport is TCP, and `params.ssl == ssl_mode::enable`, the connection will use TLS
     *     only if the server supports it.
     * \li If the transport is TCP, and `params.ssl == ssl_mode::require`, the connection will always use TLS.
     *     If the server doesn't support it, the operation will fail with \ref
     *     client_errc::server_doesnt_support_ssl.
     * \n
     * If `params.connection_collation` is within a set of well-known collations, this function
     * sets the current character set, such that \ref current_character_set returns a non-null value.
     * The default collation (`utf8mb4_general_ci`) is the only one guaranteed to be in the set of well-known
     * collations.
     */
    void connect(const connect_params& params, error_code& ec, diagnostics& diag)
    {
        impl_.connect_v2(params, ec, diag);
    }

    /// \copydoc connect
    void connect(const connect_params& params)
    {
        error_code err;
        diagnostics diag;
        connect(params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc connect
     *
     * \par Object lifetimes
     * params needs to be kept alive until the operation completes, as no
     * copies will be made by the library.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_connect(const connect_params& params, diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_connect_v2_t<CompletionToken&&>)
    {
        return impl_.async_connect_v2(params, diag, std::forward<CompletionToken>(token));
    }

    /// \copydoc async_connect
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_connect(const connect_params& params, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_connect_v2_t<CompletionToken&&>)
    {
        return async_connect(params, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Executes a text query or prepared statement.
     * \details
     * Sends `req` to the server for execution and reads the response into `result`.
     * `result` may be either a \ref results or \ref static_results object.
     * `req` should may be either a type convertible to \ref string_view containing valid SQL
     * or a bound prepared statement, obtained by calling \ref statement::bind.
     * If a string, it must be encoded using the connection's character set.
     * Any string parameters provided to \ref statement::bind should also be encoded
     * using the connection's character set.
     * \n
     * After this operation completes successfully, `result.has_value() == true`.
     * \n
     * Metadata in `result` will be populated according to `this->meta_mode()`.
     */
    template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
    void execute(ExecutionRequest&& req, ResultsType& result, error_code& err, diagnostics& diag)
    {
        impl_.execute(std::forward<ExecutionRequest>(req), result, err, diag);
    }

    /// \copydoc execute
    template <BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest, BOOST_MYSQL_RESULTS_TYPE ResultsType>
    void execute(ExecutionRequest&& req, ResultsType& result)
    {
        error_code err;
        diagnostics diag;
        execute(std::forward<ExecutionRequest>(req), result, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc execute
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the caller is
     * responsible for managing `req`'s validity following these rules:
     * \n
     * \li If `req` is `string_view`, the string pointed to by `req`
     *     must be kept alive by the caller until the operation is initiated.
     * \li If `req` is a \ref bound_statement_tuple, and any of the parameters is a reference
     *     type (like `string_view`), the caller must keep the values pointed by these references alive
     *     until the operation is initiated.
     * \li If `req` is a \ref bound_statement_iterator_range, the caller must keep objects in
     *     the iterator range passed to \ref statement::bind alive until the  operation is initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_RESULTS_TYPE ResultsType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_execute(ExecutionRequest&& req, ResultsType& result, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_execute_t<ExecutionRequest&&, ResultsType, CompletionToken&&>)
    {
        return async_execute(
            std::forward<ExecutionRequest>(req),
            result,
            impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_execute
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_RESULTS_TYPE ResultsType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        diagnostics& diag,
        CompletionToken&& token = {}
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_execute_t<ExecutionRequest&&, ResultsType, CompletionToken&&>)
    {
        return impl_.async_execute(
            std::forward<ExecutionRequest>(req),
            result,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Starts a SQL execution as a multi-function operation.
     * \details
     * Writes the execution request and reads the initial server response and the column
     * metadata, but not the generated rows or subsequent resultsets, if any.
     * `st` may be either an \ref execution_state or \ref static_execution_state object.
     * \n
     * After this operation completes, `st` will have
     * \ref execution_state::meta populated.
     * Metadata will be populated according to `this->meta_mode()`.
     * \n
     * If the operation generated any rows or more than one resultset, these <b>must</b> be read (by using
     * \ref read_some_rows and \ref read_resultset_head) before engaging in any further network operation.
     * Otherwise, the results are undefined.
     * \n
     * req may be either a type convertible to \ref string_view containing valid SQL
     * or a bound prepared statement, obtained by calling \ref statement::bind.
     * If a string, it must be encoded using the connection's character set.
     * Any string parameters provided to \ref statement::bind should also be encoded
     * using the connection's character set.
     * \n
     * When using the static interface, this function will detect schema mismatches for the first
     * resultset. Further errors may be detected by \ref read_resultset_head and \ref read_some_rows.
     * \n
     */
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    void start_execution(ExecutionRequest&& req, ExecutionStateType& st, error_code& err, diagnostics& diag)
    {
        impl_.start_execution(std::forward<ExecutionRequest>(req), st, err, diag);
    }

    /// \copydoc start_execution
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    void start_execution(ExecutionRequest&& req, ExecutionStateType& st)
    {
        error_code err;
        diagnostics diag;
        start_execution(std::forward<ExecutionRequest>(req), st, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc start_execution
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the caller is
     * responsible for managing `req`'s validity following these rules:
     * \n
     * \li If `req` is `string_view`, the string pointed to by `req`
     *     must be kept alive by the caller until the operation is initiated.
     * \li If `req` is a \ref bound_statement_tuple, and any of the parameters is a reference
     *     type (like `string_view`), the caller must keep the values pointed by these references alive
     *     until the operation is initiated.
     * \li If `req` is a \ref bound_statement_iterator_range, the caller must keep objects in
     *     the iterator range passed to \ref statement::bind alive until the  operation is initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_start_execution(ExecutionRequest&& req, ExecutionStateType& st, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_start_execution_t<
                                ExecutionRequest&&,
                                ExecutionStateType,
                                CompletionToken&&>)
    {
        return async_start_execution(
            std::forward<ExecutionRequest>(req),
            st,
            impl_.shared_diag(),
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc async_start_execution
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& st,
        diagnostics& diag,
        CompletionToken&& token = {}
    )
        BOOST_MYSQL_RETURN_TYPE(detail::async_start_execution_t<
                                ExecutionRequest&&,
                                ExecutionStateType,
                                CompletionToken&&>)
    {
        return impl_.async_start_execution(
            std::forward<ExecutionRequest>(req),
            st,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Prepares a statement server-side.
     * \details
     * `stmt` should be encoded using the connection's character set.
     * \n
     * The returned statement has `valid() == true`.
     */
    statement prepare_statement(string_view stmt, error_code& err, diagnostics& diag)
    {
        return impl_.run(detail::prepare_statement_algo_params{stmt}, err, diag);
    }

    /// \copydoc prepare_statement
    statement prepare_statement(string_view stmt)
    {
        error_code err;
        diagnostics diag;
        statement res = prepare_statement(stmt, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
        return res;
    }

    /**
     * \copydoc prepare_statement
     * \details
     * \par Object lifetimes
     * If `CompletionToken` is a deferred completion token (e.g. `use_awaitable`), the string
     * pointed to by `stmt` must be kept alive by the caller until the operation is
     * initiated.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code, boost::mysql::statement)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_prepare_statement(string_view stmt, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_prepare_statement_t<CompletionToken&&>)
    {
        return async_prepare_statement(stmt, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_prepare_statement
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_prepare_statement(string_view stmt, diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_prepare_statement_t<CompletionToken&&>)
    {
        return impl_.async_run(
            detail::prepare_statement_algo_params{stmt},
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Closes a statement, deallocating it from the server.
     * \details
     * After this operation succeeds, `stmt` must not be used again for execution.
     * \n
     * \par Preconditions
     *    `stmt.valid() == true`
     */
    void close_statement(const statement& stmt, error_code& err, diagnostics& diag)
    {
        impl_.run(impl_.make_params_close_statement(stmt), err, diag);
    }

    /// \copydoc close_statement
    void close_statement(const statement& stmt)
    {
        error_code err;
        diagnostics diag;
        close_statement(stmt, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc close_statement
     * \details
     * \par Object lifetimes
     * It is not required to keep `stmt` alive, as copies are made by the implementation as required.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_close_statement(const statement& stmt, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_close_statement_t<CompletionToken&&>)
    {
        return async_close_statement(stmt, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close_statement
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_close_statement(const statement& stmt, diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_close_statement_t<CompletionToken&&>)
    {
        return impl_
            .async_run(impl_.make_params_close_statement(stmt), diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads a batch of rows.
     * \details
     * The number of rows that will be read is unspecified. If the operation represented by `st`
     * has still rows to read, at least one will be read. If there are no more rows, or
     * `st.should_read_rows() == false`, returns an empty `rows_view`.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be
     * grown bigger by other read operations, if required.
     * \n
     * The returned view points into memory owned by `*this`. It will be valid until
     * `*this` performs the next network operation or is destroyed.
     */
    rows_view read_some_rows(execution_state& st, error_code& err, diagnostics& diag)
    {
        return impl_.run(impl_.make_params_read_some_rows(st), err, diag);
    }

    /// \copydoc read_some_rows(execution_state&,error_code&,diagnostics&)
    rows_view read_some_rows(execution_state& st)
    {
        error_code err;
        diagnostics diag;
        rows_view res = read_some_rows(st, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
        return res;
    }

    /**
     * \copydoc read_some_rows(execution_state&,error_code&,diagnostics&)
     * \details
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::rows_view)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_read_some_rows(execution_state& st, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_read_some_rows_dynamic_t<CompletionToken&&>)
    {
        return async_read_some_rows(st, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_some_rows(execution_state&,CompletionToken&&)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_read_some_rows(execution_state& st, diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_read_some_rows_dynamic_t<CompletionToken&&>)
    {
        return impl_
            .async_run(impl_.make_params_read_some_rows(st), diag, std::forward<CompletionToken>(token));
    }

#ifdef BOOST_MYSQL_CXX14

    /**
     * \brief Reads a batch of rows.
     * \details
     * Reads a batch of rows of unspecified size into the storage given by `output`.
     * At most `output.size()` rows will be read. If the operation represented by `st`
     * has still rows to read, and `output.size() > 0`, at least one row will be read.
     * \n
     * Returns the number of read rows.
     * \n
     * If there are no more rows, or `st.should_read_rows() == false`, this function is a no-op and returns
     * zero.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * The type `SpanElementType` must be the underlying row type for one of the types in the
     * `StaticRow` parameter pack (i.e., one of the types in `underlying_row_t<StaticRow>...`).
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanElementType`
     * must exactly be `underlying_row_t<T2>`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     */
    template <class SpanElementType, class... StaticRow>
    std::size_t read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanElementType> output,
        error_code& err,
        diagnostics& diag
    )
    {
        return impl_.run(impl_.make_params_read_some_rows_static(st, output), err, diag);
    }

    /**
     * \brief Reads a batch of rows.
     * \details
     * Reads a batch of rows of unspecified size into the storage given by `output`.
     * At most `output.size()` rows will be read. If the operation represented by `st`
     * has still rows to read, and `output.size() > 0`, at least one row will be read.
     * \n
     * Returns the number of read rows.
     * \n
     * If there are no more rows, or `st.should_read_rows() == false`, this function is a no-op and returns
     * zero.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * The type `SpanElementType` must be the underlying row type for one of the types in the
     * `StaticRow` parameter pack (i.e., one of the types in `underlying_row_t<StaticRow>...`).
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanElementType`
     * must exactly be `underlying_row_t<T2>`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     */
    template <class SpanElementType, class... StaticRow>
    std::size_t read_some_rows(static_execution_state<StaticRow...>& st, span<SpanElementType> output)
    {
        error_code err;
        diagnostics diag;
        std::size_t res = read_some_rows(st, output, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
        return res;
    }

    /**
     * \brief Reads a batch of rows.
     * \details
     * Reads a batch of rows of unspecified size into the storage given by `output`.
     * At most `output.size()` rows will be read. If the operation represented by `st`
     * has still rows to read, and `output.size() > 0`, at least one row will be read.
     * \n
     * Returns the number of read rows.
     * \n
     * If there are no more rows, or `st.should_read_rows() == false`, this function is a no-op and returns
     * zero.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * The type `SpanElementType` must be the underlying row type for one of the types in the
     * `StaticRow` parameter pack (i.e., one of the types in `underlying_row_t<StaticRow>...`).
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanElementType`
     * must exactly be `underlying_row_t<T2>`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     *
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::size_t)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     *
     * \par Object lifetimes
     * The storage that `output` references must be kept alive until the operation completes.
     */
    template <
        class SpanElementType,
        class... StaticRow,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanElementType> output,
        CompletionToken&& token = {}
    )
    {
        return async_read_some_rows(st, output, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /**
     * \brief Reads a batch of rows.
     * \details
     * Reads a batch of rows of unspecified size into the storage given by `output`.
     * At most `output.size()` rows will be read. If the operation represented by `st`
     * has still rows to read, and `output.size() > 0`, at least one row will be read.
     * \n
     * Returns the number of read rows.
     * \n
     * If there are no more rows, or `st.should_read_rows() == false`, this function is a no-op and returns
     * zero.
     * \n
     * The number of rows that will be read depends on the connection's buffer size. The bigger the buffer,
     * the greater the batch size (up to a maximum). You can set the initial buffer size in the
     * constructor. The buffer may be grown bigger by other read operations, if required.
     * \n
     * Rows read by this function are owning objects, and don't hold any reference to
     * the connection's internal buffers (contrary what happens with the dynamic interface's counterpart).
     * \n
     * The type `SpanElementType` must be the underlying row type for one of the types in the
     * `StaticRow` parameter pack (i.e., one of the types in `underlying_row_t<StaticRow>...`).
     * The type must match the resultset that is currently being processed by `st`. For instance,
     * given `static_execution_state<T1, T2>`, when reading rows for the second resultset, `SpanElementType`
     * must exactly be `underlying_row_t<T2>`. If this is not the case, a runtime error will be issued.
     * \n
     * This function can report schema mismatches.
     *
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, std::size_t)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     *
     * \par Object lifetimes
     * The storage that `output` references must be kept alive until the operation completes.
     */
    template <
        class SpanElementType,
        class... StaticRow,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanElementType> output,
        diagnostics& diag,
        CompletionToken&& token = {}
    )
    {
        return impl_.async_run(
            impl_.make_params_read_some_rows_static(st, output),
            diag,
            std::forward<CompletionToken>(token)
        );
    }
#endif

    /**
     * \brief Reads metadata for subsequent resultsets in a multi-resultset operation.
     * \details
     * If `st.should_read_head() == true`, this function will read the next resultset's
     * initial response message and metadata, if any. If the resultset indicates a failure
     * (e.g. the query associated to this resultset contained an error), this function will fail
     * with that error.
     * \n
     * If `st.should_read_head() == false`, this function is a no-op.
     * \n
     * `st` may be either an \ref execution_state or \ref static_execution_state object.
     * \n
     * This function is only relevant when using multi-function operations with statements
     * that return more than one resultset.
     * \n
     * When using the static interface, this function will detect schema mismatches for the resultset
     * currently being read. Further errors may be detected by subsequent invocations of this function
     * and by \ref read_some_rows.
     * \n
     */
    template <BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    void read_resultset_head(ExecutionStateType& st, error_code& err, diagnostics& diag)
    {
        return impl_.run(impl_.make_params_read_resultset_head(st), err, diag);
    }

    /// \copydoc read_resultset_head
    template <BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType>
    void read_resultset_head(ExecutionStateType& st)
    {
        error_code err;
        diagnostics diag;
        read_resultset_head(st, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc read_resultset_head
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_read_resultset_head(ExecutionStateType& st, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_read_resultset_head_t<CompletionToken&&>)
    {
        return async_read_resultset_head(st, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_resultset_head
    template <
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_read_resultset_head(ExecutionStateType& st, diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_read_resultset_head_t<CompletionToken&&>)
    {
        return impl_
            .async_run(impl_.make_params_read_resultset_head(st), diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Sets the connection's character set, as per SET NAMES.
     * \details
     * Sets the connection's character set by running a
     * <a href="https://dev.mysql.com/doc/refman/8.0/en/set-names.html">`SET NAMES`</a>
     * SQL statement, using the passed \ref character_set::name as the charset name to set.
     * \n
     * This function will also update the value returned by \ref current_character_set, so
     * prefer using this function over raw SQL statements.
     * \n
     * If the server was unable to set the character set to the requested value (e.g. because
     * the server does not support the requested charset), this function will fail,
     * as opposed to how \ref connect behaves when an unsupported collation is passed.
     * This is a limitation of MySQL servers.
     * \n
     * You need to perform connection establishment for this function to succeed, since it
     * involves communicating with the server.
     *
     * \par Object lifetimes
     * `charset` will be copied as required, and does not need to be kept alive.
     */
    void set_character_set(const character_set& charset, error_code& err, diagnostics& diag)
    {
        impl_.run(detail::set_character_set_algo_params{charset}, err, diag);
    }

    /// \copydoc set_character_set
    void set_character_set(const character_set& charset)
    {
        error_code err;
        diagnostics diag;
        set_character_set(charset, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc set_character_set
     * \details
     * \n
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_set_character_set(const character_set& charset, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_set_character_set_t<CompletionToken&&>)
    {
        return async_set_character_set(charset, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_set_character_set
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_set_character_set(
        const character_set& charset,
        diagnostics& diag,
        CompletionToken&& token = {}
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_set_character_set_t<CompletionToken&&>)
    {
        return impl_.async_run(
            detail::set_character_set_algo_params{charset},
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Checks whether the server is alive.
     * \details
     * If the server is alive, this function will complete without error.
     * If it's not, it will fail with the relevant network or protocol error.
     * \n
     * Note that ping requests are treated as any other type of request at the protocol
     * level, and won't be prioritized anyhow by the server. If the server is stuck
     * in a long-running query, the ping request won't be answered until the query is
     * finished.
     */
    void ping(error_code& err, diagnostics& diag) { impl_.run(detail::ping_algo_params{}, err, diag); }

    /// \copydoc ping
    void ping()
    {
        error_code err;
        diagnostics diag;
        ping(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc ping
     * \details
     * \n
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_ping(CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_ping_t<CompletionToken&&>)
    {
        return async_ping(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_ping
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_ping(diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_ping_t<CompletionToken&&>)
    {
        return impl_.async_run(detail::ping_algo_params{}, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Resets server-side session state, like variables and prepared statements.
     * \details
     * Resets all server-side state for the current session:
     * \n
     *   \li Rolls back any active transactions and resets autocommit mode.
     *   \li Releases all table locks.
     *   \li Drops all temporary tables.
     *   \li Resets all session system variables to their default values (including the ones set by `SET
     *       NAMES`) and clears all user-defined variables.
     *   \li Closes all prepared statements.
     * \n
     * A full reference on the affected session state can be found
     * <a href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-reset-connection.html">here</a>.
     * \n
     * \n
     * This function will not reset the current physical connection and won't cause re-authentication.
     * It is faster than closing and re-opening a connection.
     * \n
     * The connection must be connected and authenticated before calling this function.
     * This function involves communication with the server, and thus may fail.
     *
     * \par Warning on character sets
     * This function will restore the connection's character set and collation **to the server's default**,
     * and not to the one specified during connection establishment. Some servers have `latin1` as their
     * default character set, which is not usually what you want. Since there is no way to know this
     * character set, \ref current_character_set will return `nullptr` after the operation succeeds.
     * We recommend always using \ref set_character_set or \ref async_set_character_set after calling this
     * function.
     * \n
     * You can find the character set that your server will use after the reset by running:
     * \code
     * "SELECT @@global.character_set_client, @@global.character_set_results;"
     * \endcode
     */
    void reset_connection(error_code& err, diagnostics& diag)
    {
        impl_.run(detail::reset_connection_algo_params{}, err, diag);
    }

    /// \copydoc reset_connection
    void reset_connection()
    {
        error_code err;
        diagnostics diag;
        reset_connection(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc reset_connection
     * \details
     * \n
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_reset_connection(CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_reset_connection_t<CompletionToken&&>)
    {
        return async_reset_connection(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_reset_connection
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_reset_connection(diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_reset_connection_t<CompletionToken&&>)
    {
        return impl_
            .async_run(detail::reset_connection_algo_params{}, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Cleanly closes the connection to the server.
     * \details
     * This function does the following:
     * \n
     * \li Sends a quit request. This is required by the MySQL protocol, to inform
     *     the server that we're closing the connection gracefully.
     * \li If the connection is using TLS (`this->uses_ssl() == true`), performs
     *     the TLS shutdown.
     * \li Closes the transport-level connection (the TCP or UNIX socket).
     * \n
     * Since this function involves writing a message to the server, it can fail.
     * Only use this function if you know that the connection is healthy and you want
     * to cleanly close it.
     * \n
     * If you don't call this function, the destructor or successive connects will
     * perform a transport-layer close. This doesn't cause any resource leaks, but may
     * cause warnings to be written to the server logs.
     */
    void close(error_code& err, diagnostics& diag)
    {
        impl_.run(detail::close_connection_algo_params{}, err, diag);
    }

    /// \copydoc close
    void close()
    {
        error_code err;
        diagnostics diag;
        close(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc close
     * \details
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_close(CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_close_connection_t<CompletionToken&&>)
    {
        return async_close(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_close(diagnostics& diag, CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(detail::async_close_connection_t<CompletionToken&&>)
    {
        return this->impl_
            .async_run(detail::close_connection_algo_params{}, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Runs a set of pipelined requests.
     * \details
     * Runs the pipeline described by `req` and stores its response in `res`.
     * After the operation completes, `res` will have as many elements as stages
     * were in `req`, even if the operation fails.
     * \n
     * Request stages are seen by the server as a series of unrelated requests.
     * As a consequence, all stages are always run, even if previous stages fail.
     * \n
     * If all stages succeed, the operation completes successfully. Thus, there is no need to check
     * the per-stage error code in `res` if this operation completed successfully.
     * \n
     * If any stage fails with a non-fatal error (as per \ref is_fatal_error), the result of the operation
     * is the first encountered error. You can check which stages succeeded and which ones didn't by
     * inspecting each stage in `res`.
     * \n
     * If any stage fails with a fatal error, the result of the operation is the fatal error.
     * Successive stages will be marked as failed with the fatal error. The server may or may
     * not have processed such stages.
     */
    void run_pipeline(
        const pipeline_request& req,
        std::vector<stage_response>& res,
        error_code& err,
        diagnostics& diag
    )
    {
        impl_.run(impl_.make_params_pipeline(req, res), err, diag);
    }

    /// \copydoc run_pipeline
    void run_pipeline(const pipeline_request& req, std::vector<stage_response>& res)
    {
        error_code err;
        diagnostics diag;
        run_pipeline(req, res, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc run_pipeline
     * \details
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`.
     *
     * \par Executor
     * Intermediate completion handlers, as well as the final handler, are executed using
     * `token`'s associated executor, or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     *
     * \par Object lifetimes
     * The request and response objects must be kept alive and should not be modified
     * until the operation completes.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_run_pipeline(
        const pipeline_request& req,
        std::vector<stage_response>& res,
        CompletionToken&& token = {}
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_run_pipeline_t<CompletionToken&&>)
    {
        return async_run_pipeline(req, res, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_run_pipeline
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_run_pipeline(
        const pipeline_request& req,
        std::vector<stage_response>& res,
        diagnostics& diag,
        CompletionToken&& token = {}
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_run_pipeline_t<CompletionToken&&>)
    {
        return this->impl_
            .async_run(impl_.make_params_pipeline(req, res), diag, std::forward<CompletionToken>(token));
    }
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/any_connection.ipp>
#endif

#endif
