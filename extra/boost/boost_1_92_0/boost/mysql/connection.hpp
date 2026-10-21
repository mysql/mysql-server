//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_HPP
#define BOOST_MYSQL_CONNECTION_HPP

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/connection_impl.hpp>
#include <boost/mysql/detail/engine_stream_adaptor.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/rebind_executor.hpp>
#include <boost/mysql/detail/socket_stream.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

/// The Boost libraries namespace.
namespace boost {
/// Boost.MySQL library namespace.
namespace mysql {

// Forward declarations
template <class... StaticRow>
class static_execution_state;

/**
 * \brief (Legacy) A connection to a MySQL server.
 * \details
 * Represents a templated connection to a MySQL server.
 *
 * `connection` owns a `Stream` object that
 * is accessed by functions involving network operations, as well as session state. You can access
 * the stream using \ref connection::stream, and its executor via \ref connection::get_executor. The
 * executor used by this object is always the same as the underlying stream.
 *
 * \par Single outstanding async operation per connection
 * At any given point in time, only one async operation can be outstanding
 * per connection. If an async operation is initiated while another one is in progress,
 * it will fail with \ref client_errc::operation_in_progress.
 *
 * \par Thread safety
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 * This class is <b>not thread-safe</b>: for a single object, if you
 * call its member functions concurrently from separate threads, you will get a race condition.
 *
 * \par Legacy
 * New code should use \ref any_connection instead of this class, as it's simpler to use
 * and provides the same level of performance.
 */
template <class Stream>
class connection
{
    detail::connection_impl impl_;

public:
    /**
     * \brief Initializing constructor.
     * \details
     * As part of the initialization, an internal `Stream` object is created.
     *
     * \par Exception safety
     * Basic guarantee. Throws if the `Stream` constructor throws
     * or if memory allocation for internal state fails.
     *
     * \param args Arguments to be forwarded to the `Stream` constructor.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(Args&&... args) : connection(buffer_params(), std::forward<Args>(args)...)
    {
    }

    /**
     * \brief Initializing constructor with buffer params.
     * \details
     * As part of the initialization, an internal `Stream` object is created.
     *
     * \par Exception safety
     * Basic guarantee. Throws if the `Stream` constructor throws
     * or if memory allocation for internal state fails.
     *
     * \param buff_params Specifies initial sizes for internal buffers.
     * \param args Arguments to be forwarded to the `Stream` constructor.
     */
    template <
        class... Args,
        class EnableIf = typename std::enable_if<std::is_constructible<Stream, Args...>::value>::type>
    connection(const buffer_params& buff_params, Args&&... args)
        : impl_(
              buff_params.initial_read_size(),
              static_cast<std::size_t>(-1),
              detail::make_engine<Stream>(std::forward<Args>(args)...)
          )
    {
    }

    /**
     * \brief Move constructor.
     */
    connection(connection&& other) = default;

    /**
     * \brief Move assignment.
     */
    connection& operator=(connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
#endif

    /// The executor type associated to this object.
    using executor_type = typename Stream::executor_type;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return stream().get_executor(); }

    /// The `Stream` type this connection is using.
    using stream_type = Stream;

    /**
     * \brief Retrieves the underlying Stream object.
     * \details
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    Stream& stream() noexcept { return detail::stream_from_engine<Stream>(impl_.get_engine()); }

    /**
     * \brief Retrieves the underlying Stream object.
     * \details
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    const Stream& stream() const noexcept { return detail::stream_from_engine<Stream>(impl_.get_engine()); }

    /**
     * \brief Returns whether the connection negotiated the use of SSL or not.
     * \details
     * This function can be used to determine whether you are using a SSL
     * connection or not when using SSL negotiation.
     * \n
     * This function always returns `false` if the underlying
     * stream does not support SSL. This function always returns `false`
     * for connections that haven't been
     * established yet (handshake not run yet). If the handshake fails,
     * the return value is undefined.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \returns Whether the connection is using SSL.
     */
    bool uses_ssl() const noexcept { return impl_.ssl_active(); }

    /// \copydoc any_connection::meta_mode
    metadata_mode meta_mode() const noexcept { return impl_.meta_mode(); }

    /// \copydoc any_connection::set_meta_mode
    void set_meta_mode(metadata_mode v) noexcept { impl_.set_meta_mode(v); }

    /**
     * \brief Establishes a connection to a MySQL server.
     * \details
     * This function is only available if `Stream` satisfies the
     * `SocketStream` concept.
     * \n
     * Connects the underlying stream and performs the handshake
     * with the server. The underlying stream is closed in case of error. Prefer
     * this function to \ref connection::handshake.
     * \n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     * \n
     * `endpoint` should be convertible to `Stream::lowest_layer_type::endpoint_type`.
     */
    template <typename EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& ec,
        diagnostics& diag
    )
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "connect can only be used if Stream satisfies the SocketStream concept"
        );
        impl_.connect<typename Stream::lowest_layer_type::endpoint_type>(endpoint, params, ec, diag);
    }

    /// \copydoc connect
    template <typename EndpointType>
    void connect(const EndpointType& endpoint, const handshake_params& params)
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "connect can only be used if Stream satisfies the SocketStream concept"
        );
        error_code err;
        diagnostics diag;
        connect(endpoint, params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc connect
     * \par Object lifetimes
     * The strings pointed to by `params` should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
     * `endpoint` is copied as required and doesn't need to be kept alive.
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
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_connect can only be used if Stream satisfies the SocketStream concept"
        );
        return async_connect(endpoint, params, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_connect
    template <
        typename EndpointType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_connect can only be used if Stream satisfies the SocketStream concept"
        );
        return impl_.async_connect<typename Stream::lowest_layer_type::endpoint_type>(
            endpoint,
            params,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /**
     * \brief Performs the MySQL-level handshake.
     * \details
     * Does not connect the underlying stream.
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::connect instead of this function.
     * \n
     * If using a SSL-capable stream, the SSL handshake will be performed by this function.
     */
    void handshake(const handshake_params& params, error_code& ec, diagnostics& diag)
    {
        impl_.run(impl_.make_params_handshake(params), ec, diag);
    }

    /// \copydoc handshake
    void handshake(const handshake_params& params)
    {
        error_code err;
        diagnostics diag;
        handshake(params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc handshake
     * \par Object lifetimes
     * The strings pointed to by `params` should be kept alive by the caller
     * until the operation completes, as no copy is made by the library.
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
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_handshake(
        const handshake_params& params,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_handshake_t<CompletionToken&&>)
    {
        return async_handshake(params, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_handshake
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_handshake(
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_handshake_t<CompletionToken&&>)
    {
        return impl_
            .async_run(impl_.make_params_handshake(params), diag, std::forward<CompletionToken>(token));
    }

    /// \copydoc any_connection::execute
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

    /// \copydoc any_connection::async_execute
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_RESULTS_TYPE ResultsType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_execute_t<ExecutionRequest&&, ResultsType, CompletionToken&&>)
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
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_execute_t<ExecutionRequest&&, ResultsType, CompletionToken&&>)
    {
        return impl_.async_execute(
            std::forward<ExecutionRequest>(req),
            result,
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc any_connection::start_execution
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

    /// \copydoc any_connection::async_start_execution
    template <
        BOOST_MYSQL_EXECUTION_REQUEST ExecutionRequest,
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
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
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
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

    /// \copydoc any_connection::prepare_statement
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

    /// \copydoc any_connection::async_prepare_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_prepare_statement(
        string_view stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_prepare_statement_t<CompletionToken&&>)
    {
        return async_prepare_statement(stmt, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_prepare_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::statement))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_prepare_statement(
        string_view stmt,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_prepare_statement_t<CompletionToken&&>)
    {
        return impl_.async_run(
            detail::prepare_statement_algo_params{stmt},
            diag,
            std::forward<CompletionToken>(token)
        );
    }

    /// \copydoc any_connection::close_statement
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

    /// \copydoc any_connection::async_close_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_close_statement(
        const statement& stmt,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_close_statement_t<CompletionToken&&>)
    {
        return async_close_statement(stmt, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close_statement
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_close_statement(
        const statement& stmt,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_close_statement_t<CompletionToken&&>)
    {
        return impl_
            .async_run(impl_.make_params_close_statement(stmt), diag, std::forward<CompletionToken>(token));
    }

    /// \copydoc any_connection::read_some_rows
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

    /// \copydoc any_connection::async_read_some_rows(execution_state&,CompletionToken&&)
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_read_some_rows(
        execution_state& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_read_some_rows_dynamic_t<CompletionToken&&>)
    {
        return async_read_some_rows(st, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_some_rows(execution_state&,CompletionToken&&)
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_read_some_rows(
        execution_state& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_read_some_rows_dynamic_t<CompletionToken&&>)
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
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
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
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
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
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
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
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanElementType> output,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
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
     * the greater the batch size (up to a maximum). You can set the initial buffer size in `connection`'s
     * constructor, using \ref buffer_params::initial_read_size. The buffer may be
     * grown bigger by other read operations, if required.
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
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_read_some_rows(
        static_execution_state<StaticRow...>& st,
        span<SpanElementType> output,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
        return impl_.async_run(
            impl_.make_params_read_some_rows_static(st, output),
            diag,
            std::forward<CompletionToken>(token)
        );
    }
#endif

    /// \copydoc any_connection::read_resultset_head
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

    /// \copydoc any_connection::async_read_resultset_head
    template <
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_read_resultset_head(
        ExecutionStateType& st,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_read_resultset_head_t<CompletionToken&&>)
    {
        return async_read_resultset_head(st, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_read_resultset_head
    template <
        BOOST_MYSQL_EXECUTION_STATE_TYPE ExecutionStateType,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_read_resultset_head(
        ExecutionStateType& st,
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_read_resultset_head_t<CompletionToken&&>)
    {
        return impl_
            .async_run(impl_.make_params_read_resultset_head(st), diag, std::forward<CompletionToken>(token));
    }

    /// \copydoc any_connection::ping
    void ping(error_code& err, diagnostics& diag) { impl_.run(detail::ping_algo_params{}, err, diag); }

    /// \copydoc ping
    void ping()
    {
        error_code err;
        diagnostics diag;
        ping(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /// \copydoc any_connection::async_ping
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_ping(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        BOOST_MYSQL_RETURN_TYPE(detail::async_ping_t<CompletionToken&&>)
    {
        return async_ping(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_ping
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_ping(
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_ping_t<CompletionToken&&>)
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
     * This function will not reset the current physical connection and won't cause re-authentication.
     * It is faster than closing and re-opening a connection.
     * \n
     * The connection must be connected and authenticated before calling this function.
     * This function involves communication with the server, and thus may fail.
     *
     * \par Warning on character sets
     * This function will restore the connection's character set and collation **to the server's default**,
     * and not to the one specified during connection establishment. Some servers have `latin1` as their
     * default character set, which is not usually what you want. Use a `SET NAMES` statement after using
     * this function to be sure.
     * \n
     * You can find the character set that your server will use after reset by running:
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
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_reset_connection(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        BOOST_MYSQL_RETURN_TYPE(detail::async_reset_connection_t<CompletionToken&&>)
    {
        return async_reset_connection(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_reset_connection
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_reset_connection(
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_reset_connection_t<CompletionToken&&>)
    {
        return impl_
            .async_run(detail::reset_connection_algo_params{}, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Closes the connection to the server.
     * \details
     * This function is only available if `Stream` satisfies the `SocketStream` concept.
     * \n
     * Sends a quit request, performs the TLS shutdown (if required)
     * and closes the underlying stream. Prefer this function to \ref connection::quit.
     * \n
     */
    void close(error_code& err, diagnostics& diag)
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "close can only be used if Stream satisfies the SocketStream concept"
        );
        impl_.run(detail::close_connection_algo_params{}, err, diag);
    }

    /// \copydoc close
    void close()
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "close can only be used if Stream satisfies the SocketStream concept"
        );
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
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_close(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        BOOST_MYSQL_RETURN_TYPE(detail::async_close_connection_t<CompletionToken&&>)
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_close can only be used if Stream satisfies the SocketStream concept"
        );
        return async_close(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_close(
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_close_connection_t<CompletionToken&&>)
    {
        static_assert(
            detail::is_socket_stream<Stream>::value,
            "async_close can only be used if Stream satisfies the SocketStream concept"
        );
        return impl_
            .async_run(detail::close_connection_algo_params{}, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Notifies the MySQL server that the client wants to end the session and shutdowns SSL.
     * \details Sends a quit request to the MySQL server. If the connection is using SSL,
     * this function will also perform the SSL shutdown. You should
     * close the underlying physical connection after calling this function.
     * \n
     * If the `Stream` template parameter fulfills the `SocketConnection`
     * requirements, use \ref connection::close instead of this function,
     * as it also takes care of closing the underlying stream.
     */
    void quit(error_code& err, diagnostics& diag)
    {
        impl_.run(detail::quit_connection_algo_params{}, err, diag);
    }

    /// \copydoc quit
    void quit()
    {
        error_code err;
        diagnostics diag;
        quit(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    /**
     * \copydoc quit
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
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_quit(CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        BOOST_MYSQL_RETURN_TYPE(detail::async_quit_connection_t<CompletionToken&&>)
    {
        return async_quit(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_quit
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
                  CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    auto async_quit(
        diagnostics& diag,
        CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    ) BOOST_MYSQL_RETURN_TYPE(detail::async_quit_connection_t<CompletionToken&&>)
    {
        return impl_
            .async_run(detail::quit_connection_algo_params{}, diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Rebinds the connection type to another executor.
     * \details
     * The `Stream` type must either provide a `rebind_executor`
     * member with the same semantics, or be an instantiation of `boost::asio::ssl::stream` with
     * a `Stream` type providing a `rebind_executor` member.
     */
    template <typename Executor1>
    struct rebind_executor
    {
        /// The connection type when rebound to the specified executor.
        using other = connection<typename detail::rebind_executor<Stream, Executor1>::type>;
    };
};

}  // namespace mysql
}  // namespace boost

#endif
