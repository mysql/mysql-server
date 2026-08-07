//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_POOL_HPP
#define BOOST_MYSQL_CONNECTION_POOL_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>
#include <boost/mysql/with_diagnostics.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool_fwd.hpp>
#include <boost/mysql/detail/initiation_base.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>

#include <chrono>
#include <memory>
#include <utility>

namespace boost {
namespace mysql {

/**
 * \brief A proxy to a connection owned by a pool that returns it to the pool when destroyed.
 * \details
 * A `pooled_connection` behaves like to a `std::unique_ptr`: it has exclusive ownership of an
 * \ref any_connection created by the pool. When destroyed, it returns the connection to the pool.
 * A `pooled_connection` may own nothing. We say such a connection is invalid (`this->valid() == false`).
 *
 * This class is movable but not copyable.
 *
 * \par Object lifetimes
 * While `*this` is alive, the \ref connection_pool internal data will be kept alive
 * automatically. It's safe to destroy the `connection_pool` object before `*this`.
 *
 * \par Thread safety
 * This object and the \ref any_connection object it may point to are **not thread safe**,
 * even if the connection pool used to obtain them was constructed with
 * \ref pool_params::thread_safe set to true.
 *
 * Functions that return the underlying connection to the pool
 * cause a mutation on the pool state object. Calling such functions
 * on objects obtained from the same pool
 * is thread-safe only if the pool was constructed with \ref pool_params::thread_safe set to true.
 *
 * In other words, individual connections can't be shared between threads. Pools can
 * be shared only if they're constructed with \ref pool_params::thread_safe set to true.
 *
 * - Distinct objects: safe if the \ref connection_pool that was used to obtain the objects
 *   was created with \ref pool_params::thread_safe set to true. Otherwise, unsafe.
 * - Shared objects: always unsafe.
 */
class pooled_connection
{
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
    friend class detail::basic_pool_impl<any_connection, std::chrono::steady_clock, pooled_connection>;
#endif

    struct impl_t
    {
        detail::connection_node* node;
        std::shared_ptr<detail::pool_impl> pool;
    } impl_{};

    pooled_connection(detail::connection_node& node, std::shared_ptr<detail::pool_impl> pool_impl) noexcept
        : impl_{&node, std::move(pool_impl)}
    {
        BOOST_ASSERT(impl_.pool);
    }

public:
    /**
     * \brief Constructs an invalid pooled connection.
     * \details
     * The resulting object is invalid (`this->valid() == false`).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pooled_connection() noexcept = default;

    /**
     * \brief Move constructor.
     * \details
     * Transfers connection ownership from `other` to `*this`.
     *
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pooled_connection(pooled_connection&& other) noexcept : impl_(std::move(other.impl_)) {}

    /**
     * \brief Move assignment.
     * \details
     * If `this->valid()`, returns the connection owned by `*this` to the pool and marks
     * it as pending reset (as if the destructor was called).
     * It then transfers connection ownership from `other` to `*this`.
     *
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     *
     * \par Thread-safety
     * May cause a mutation on the connection pool that `this` points to.
     * Thread-safe for a shared pool only if it was constructed with
     * \ref pool_params::thread_safe set to true.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pooled_connection& operator=(pooled_connection&& other) noexcept
    {
        if (valid())
        {
            detail::return_connection(*impl_.pool, *impl_.node, true);
        }
        impl_ = std::move(other.impl_);
        return *this;
    }

#ifndef BOOST_MYSQL_DOXYGEN
    pooled_connection(const pooled_connection&) = delete;
    pooled_connection& operator=(const pooled_connection&) = delete;
#endif

    /**
     * \brief Destructor.
     * \details
     * If `this->valid() == true`, returns the owned connection to the pool
     * and marks it as pending reset. If your connection doesn't need to be reset
     * (e.g. because you didn't mutate session state), use \ref return_without_reset.
     *
     * \par Thread-safety
     * May cause a mutation on the connection pool that `this` points to.
     * Thread-safe for a shared pool only if it was constructed with
     * \ref pool_params::thread_safe set to true.
     */
    ~pooled_connection()
    {
        if (valid())
            detail::return_connection(*impl_.pool, *impl_.node, true);
    }

    /**
     * \brief Returns whether the object owns a connection or not.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool valid() const noexcept { return impl_.pool.get() != nullptr; }

    /**
     * \brief Retrieves the connection owned by this object.
     * \par Preconditions
     * The object should own a connection (`this->valid() == true`).
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` or an object
     * move-constructed or move-assigned from `*this` is alive.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    any_connection& get() noexcept { return detail::get_connection(*impl_.node); }

    /// \copydoc get
    const any_connection& get() const noexcept { return detail::get_connection(*impl_.node); }

    /// \copydoc get
    any_connection* operator->() noexcept { return &get(); }

    /// \copydoc get
    const any_connection* operator->() const noexcept { return &get(); }

    /**
     * \brief Returns the owned connection to the pool and marks it as not requiring reset.
     * \details
     * Returns a connection to the pool and marks it as idle. This will
     * skip the \ref any_connection::async_reset_connection call to wipe session state.
     * \n
     * This can provide a performance gain, but must be used with care. Failing to wipe
     * session state can lead to resource leaks (prepared statements not being released),
     * incorrect results and vulnerabilities (different logical operations interacting due
     * to leftover state).
     * \n
     * Please read the documentation on \ref any_connection::async_reset_connection before
     * calling this function. If in doubt, don't use it, and leave the destructor return
     * the connection to the pool for you.
     * \n
     * When this function returns, `*this` will own nothing (`this->valid() == false`).
     *
     * \par Preconditions
     * `this->valid() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thread-safety
     * Causes a mutation on the connection pool that `this` points to.
     * Thread-safe for a shared pool only if it was constructed with
     * \ref pool_params::thread_safe set to true.
     */
    void return_without_reset() noexcept
    {
        BOOST_ASSERT(valid());
        detail::return_connection(*impl_.pool, *impl_.node, false);
        impl_ = impl_t{};
    }
};

/**
 * \brief A pool of connections of variable size.
 * \details
 * A connection pool creates and manages \ref any_connection objects.
 * Using a pool allows to reuse sessions, avoiding part of the overhead associated
 * to session establishment. It also features built-in error handling and reconnection.
 * See the discussion and examples for more details on when to use this class.
 *
 * Connections are retrieved by \ref async_get_connection, which yields a
 * \ref pooled_connection object. They are returned to the pool when the
 * `pooled_connection` is destroyed, or by calling \ref pooled_connection::return_without_reset.
 *
 * A pool needs to be run before it can return any connection. Use \ref async_run for this.
 * Pools can only be run once.
 *
 * Connections are created, connected and managed internally by the pool, following
 * a well-defined state model. Please refer to the discussion for details.
 *
 * Due to oddities in Boost.Asio's universal async model, this class only
 * exposes async functions. You can use `asio::use_future` to transform them
 * into sync functions (please read the discussion for details).
 *
 * This is a move-only type.
 *
 * \par Default completion tokens
 * The default completion token for all async operations in this class is
 * `with_diagnostics(asio::deferred)`, which allows you to use `co_await`
 * and have the expected exceptions thrown on error.
 *
 * \par Thread-safety
 * Pools are composed of an internal state object, plus a handle to such state.
 * Each component has different thread-safety rules.
 *
 * Regarding **internal state**, connection pools are **not thread-safe by default**,
 * but can be made safe by constructing them with
 * \ref pool_params::thread_safe set to `true`.
 * Internal state is also mutated by some functions outside `connection_pool`, like
 * returning connections.
 *
 * The following actions imply a pool state mutation, and are protected by a strand
 * when thread-safety is enabled:
 *
 * - Calling \ref connection_pool::async_run.
 * - Calling \ref connection_pool::async_get_connection.
 * - Cancelling \ref async_get_connection by emitting a cancellation signal.
 * - Returning a connection by destroying a \ref pooled_connection or
 *   calling \ref pooled_connection::return_without_reset.
 * - Cancelling the pool by calling \ref connection_pool::cancel,
 *   emitting a cancellation signal for \ref async_run, or destroying the
 *   `connection_pool` object.
 *
 * The **handle to the pool state** is **never thread-safe**, even for
 * pools with thread-safety enabled. Functions like assignments
 * modify the handle, and cause race conditions if called
 * concurrently with other functions. Other objects,
 * like \ref pooled_connection, have their own state handle,
 * and thus interact only with the pool state.
 *
 * If configured to be thread-safe, the protection applies only to the pool's state.
 * In particular, be careful when using `asio::cancel_after` and similar tokens.
 * Please read
 * <a href="../connection_pool.html#mysql.connection_pool.thread_safe">this page</a> for more info.
 *
 * In summary:
 *
 * - Distinct objects: safe. \n
 * - Shared objects: unsafe. Setting \ref pool_params::thread_safe
 *   to `true` makes some functions safe.
 *
 * \par Object lifetimes
 * Connection pool objects create an internal state object that is referenced
 * by other objects and operations (like \ref pooled_connection). This object
 * will be kept alive using shared ownership semantics even after the `connection_pool`
 * object is destroyed. This results in intuitive lifetime rules.
 */
class connection_pool
{
    std::shared_ptr<detail::pool_impl> impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    struct initiate_run : detail::initiation_base
    {
        using detail::initiation_base::initiation_base;

        // Having diagnostics* here makes async_run compatible with with_diagnostics
        template <class Handler>
        void operator()(Handler&& h, diagnostics*, std::shared_ptr<detail::pool_impl> self)
        {
            async_run_erased(std::move(self), std::forward<Handler>(h));
        }
    };

    BOOST_MYSQL_DECL
    static void async_run_erased(
        std::shared_ptr<detail::pool_impl> pool,
        asio::any_completion_handler<void(error_code)> handler
    );

    struct initiate_get_connection : detail::initiation_base
    {
        using detail::initiation_base::initiation_base;

        template <class Handler>
        void operator()(Handler&& h, diagnostics* diag, std::shared_ptr<detail::pool_impl> self)
        {
            async_get_connection_erased(std::move(self), diag, std::forward<Handler>(h));
        }
    };

    BOOST_MYSQL_DECL
    static void async_get_connection_erased(
        std::shared_ptr<detail::pool_impl> pool,
        diagnostics* diag,
        asio::any_completion_handler<void(error_code, pooled_connection)> handler
    );

    template <class CompletionToken>
    auto async_get_connection_impl(diagnostics* diag, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code, pooled_connection)>(
            std::declval<initiate_get_connection>(),
            token,
            diag,
            impl_
        ))
    {
        BOOST_ASSERT(valid());
        return asio::async_initiate<CompletionToken, void(error_code, pooled_connection)>(
            initiate_get_connection{get_executor()},
            token,
            diag,
            impl_
        );
    }

    BOOST_MYSQL_DECL
    connection_pool(asio::any_io_executor ex, pool_params&& params, int);

public:
    /**
     * \brief Constructs a connection pool.
     * \details
     *
     * The pool is created in a "not-running" state. Call \ref async_run to transition to the
     * "running" state.
     *
     * The constructed pool is always valid (`this->valid() == true`).
     *
     * \par Executor
     * The passed executor becomes the pool executor, available through \ref get_executor.
     * `ex` is used as follows:
     *
     *   - If `params.thread_safe == true`, `ex` is used to build a strand. The strand is used
     *     to build internal I/O objects, like timers.
     *   - If `params.thread_safe == false`, `ex` is used directly to build internal I/O objects.
     *   - If `params.connection_executor` is empty, `ex` is used to build individual connections,
     *     regardless of the chosen thread-safety mode. Otherwise, `params.connection_executor`
     *     is used.
     *
     * \par Exception safety
     * Strong guarantee. Exceptions may be thrown by memory allocations.
     * \throws std::invalid_argument If `params` contains values that violate the rules described in \ref
     *         pool_params.
     */
    connection_pool(asio::any_io_executor ex, pool_params params)
        : connection_pool(std::move(ex), std::move(params), 0)
    {
    }

    /**
     * \brief Constructs a connection pool.
     * \details
     * Equivalent to `connection_pool(ctx.get_executor(), params)`.
     *
     * This function participates in overload resolution only if `ExecutionContext`
     * satisfies the `ExecutionContext` requirements imposed by Boost.Asio.
     *
     * \par Exception safety
     * Strong guarantee. Exceptions may be thrown by memory allocations.
     * \throws std::invalid_argument If `params` contains values that violate the rules described in \ref
     *         pool_params.
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
    connection_pool(ExecutionContext& ctx, pool_params params)
        : connection_pool(ctx.get_executor(), std::move(params), 0)
    {
    }

#ifndef BOOST_MYSQL_DOXYGEN
    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;
#endif

    /**
     * \brief Move-constructor.
     * \details
     * Constructs a connection pool by taking ownership of `other`.
     *
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     *
     * Moving a connection pool with outstanding async operations
     * is safe.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thread-safety
     * Mutates `other`'s internal state handle. Does not access the pool state.
     * This function **can never be called concurrently with other functions
     * that read the internal state handle**, even for pools created
     * with \ref pool_params::thread_safe set to true.
     *
     * The internal pool state is not accessed, so this function can be called
     * concurrently with functions that only access the pool's internal state,
     * like returning connections.
     */
    connection_pool(connection_pool&& other) = default;

    /**
     * \brief Move assignment.
     * \details
     * Assigns `other` to `*this`, transferring ownership.
     *
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     *
     * Moving a connection pool with outstanding async operations
     * is safe.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thread-safety
     * Mutates `*this` and `other`'s internal state handle. Does not access the pool state.
     * This function **can never be called concurrently with other functions
     * that read the internal state handle**, even for pools created
     * with \ref pool_params::thread_safe set to true.
     *
     * The internal pool state is not accessed, so this function can be called
     * concurrently with functions that only access the pool's internal state,
     * like returning connections.
     */
    connection_pool& operator=(connection_pool&& other) = default;

    /**
     * \brief Destructor.
     * \details
     * Cancels all outstanding async operations on `*this`, as per \ref cancel.
     *
     * \par Thread-safety
     * Mutates the internal state handle. Mutates the pool state.
     * This function **can never be called concurrently with other functions
     * that read the internal state handle**, even for pools created
     * with \ref pool_params::thread_safe set to true.
     *
     * The internal pool state is modified as per \ref cancel.
     * If thread-safety is enabled, it's safe to call the destructor concurrently
     * with functions that only access the pool's internal state,
     * like returning connections.
     */
    ~connection_pool()
    {
        if (valid())
            cancel();
    }

    /**
     * \brief Returns whether the object is in a moved-from state.
     * \details
     * This function returns always `true` except for pools that have been
     * moved-from. Moved-from objects don't represent valid pools. They can only
     * be assigned to or destroyed.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thread-safety
     * Reads the internal state handle. Does not access the pool state.
     * Can be called concurrently with any other function that reads the state handle,
     * like \ref async_run or \ref async_get_connection.
     * It can't be called concurrently with functions modifying the handle, like assignments,
     * even if \ref pool_params::thread_safe is set to true.
     */
    bool valid() const noexcept { return impl_.get() != nullptr; }

    /// The executor type associated to this object.
    using executor_type = asio::any_io_executor;

    /**
     * \brief Retrieves the executor associated to this object.
     * \details
     * Returns the executor used to construct the pool as first argument.
     * This is the case even when using \ref pool_params::thread_safe -
     * the internal strand created in this case is never exposed.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thread-safety
     * Reads the internal state handle. Reads the pool state.
     * If the pool was built with thread-safety enabled, it can be called
     * concurrently with other functions that don't modify the state handle.
     */
    BOOST_MYSQL_DECL
    executor_type get_executor() noexcept;

    /**
     * \brief Runs the pool task in charge of managing connections.
     * \details
     * This function creates and connects new connections, and resets and pings
     * already created ones. You need to call this function for \ref async_get_connection
     * to succeed.
     *
     * The async operation will run indefinitely, until the pool is cancelled
     * (by calling \ref cancel or using per-operation cancellation on the `async_run` operation).
     * The operation completes once all internal connection operations
     * (including connects, pings and resets) complete.
     *
     * It is safe to call this function after calling \ref cancel.
     *
     * \par Preconditions
     * This function can be called at most once for a single pool.
     * Formally, `async_run` hasn't been called before on `*this` or any object
     * used to move-construct or move-assign `*this`.
     *
     * Additionally, `this->valid() == true`.
     *
     * \par Object lifetimes
     * While the operation is outstanding, the pool's internal data will be kept alive.
     * It is safe to destroy `*this` while the operation is outstanding.
     *
     * \par Handler signature
     * The handler signature for this operation is `void(boost::mysql::error_code)`
     *
     * \par Executor
     *
     * The final handler is executed using `token`'s associated executor,
     * or `this->get_executor()` if the token doesn't have an associated
     * executor. The final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     *
     * If the pool was constructed with thread-safety enabled, intermediate
     * completion handlers are executed using an internal strand that wraps `this->get_executor()`.
     * Otherwise, intermediate handlers are executed using `this->get_executor()`.
     * In any case, the token's associated executor is only used for the final handler.
     *
     * \par Per-operation cancellation
     * This operation supports per-operation cancellation. Cancelling `async_run`
     * is equivalent to calling \ref connection_pool::cancel.
     * The following `asio::cancellation_type_t` values are supported:
     *
     *   - `asio::cancellation_type_t::terminal`
     *   - `asio::cancellation_type_t::partial`
     *
     * Note that `asio::cancellation_type_t::total` is not supported because invoking
     * `async_run` always has observable side effects.
     *
     * \par Errors
     * This function always complete successfully. The handler signature ensures
     * maximum compatibility with Boost.Asio infrastructure.
     *
     * \par Thread-safety
     * Reads the internal state handle. Mutates the pool state.
     * If the pool was built with thread-safety enabled, it can be called
     * concurrently with other functions that don't modify the state handle.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_run(CompletionToken&& token = {})
        BOOST_MYSQL_RETURN_TYPE(decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            std::declval<initiate_run>(),
            token,
            static_cast<diagnostics*>(nullptr),
            impl_
        )))
    {
        BOOST_ASSERT(valid());
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_run{get_executor()},
            token,
            static_cast<diagnostics*>(nullptr),
            impl_
        );
    }

    /**
     * \brief Retrieves a connection from the pool.
     * \details
     * Retrieves an idle connection from the pool to be used.
     *
     * If this function completes successfully (empty error code), the return \ref pooled_connection
     * will have `valid() == true` and will be usable. If it completes with a non-empty error code,
     * it will have `valid() == false`.
     *
     * If a connection is idle when the operation is started, it will complete immediately
     * with that connection. Otherwise, it will wait for a connection to become idle
     * (possibly creating one in the process, if pool configuration allows it), until
     * the operation is cancelled (by emitting a cancellation signal) or the pool
     * is cancelled (by calling \ref connection_pool::cancel).
     * If the pool is not running, the operation fails immediately.
     *
     * If the operation is cancelled, and the overload with \ref diagnostics was used,
     * the output diagnostics will contain the most recent error generated by
     * the connections attempting to connect (via \ref any_connection::async_connect), if any.
     * In cases where \ref async_get_connection doesn't complete because connections are unable
     * to connect, this feature can help figuring out where the problem is.
     *
     * \par Preconditions
     * `this->valid() == true` \n
     *
     * \par Object lifetimes
     * While the operation is outstanding, the pool's internal data will be kept alive.
     * It is safe to destroy `*this` while the operation is outstanding.
     *
     * \par Handler signature
     * The handler signature for this operation is
     * `void(boost::mysql::error_code, boost::mysql::pooled_connection)`
     *
     * \par Executor
     *
     * If the final handler has an associated immediate executor, and the operation
     * completes immediately, the final handler is dispatched to it.
     * Otherwise, the final handler is called as if it was submitted using `asio::post`,
     * and is never be called inline from within this function.
     * Immediate completions can only happen when thread-safety is not enabled.
     *
     * The final handler is executed using `token`'s associated executor,
     * or `this->get_executor()` if the token doesn't have an associated
     * executor.
     *
     * If the pool was constructed with thread-safety enabled, intermediate
     * completion handlers are executed using an internal strand that wraps `this->get_executor()`.
     * Otherwise, intermediate handlers are executed using
     * `token`'s associated executor if it has one, or `this->get_executor()` if it hasn't.
     *
     * **Caution**: be careful when using thread-safety and `asio::cancel_after`, as it
     * can result in inadvertent race conditions. Please refer to
     * <a href="../../../connection_pool.html#mysql.connection_pool.thread_safe">this
     * page</a> for more info.
     *
     * \par Per-operation cancellation
     * This operation supports per-operation cancellation.
     * Cancelling `async_get_connection` has no observable side effects.
     * The following `asio::cancellation_type_t` values are supported:
     *
     *   - `asio::cancellation_type_t::terminal`
     *   - `asio::cancellation_type_t::partial`
     *   - `asio::cancellation_type_t::total`
     *
     * \par Errors
     *   - \ref client_errc::no_connection_available, if the `async_get_connection`
     *     operation is cancelled before a connection becomes available.
     *   - \ref client_errc::pool_not_running, if the `async_get_connection`
     *     operation is cancelled before async_run is called.
     *   - \ref client_errc::pool_cancelled, if the pool is cancelled before
     *     the operation completes, or `async_get_connection` is called
     *     on a pool that has been cancelled.
     *
     * \par Thread-safety
     * Reads the internal state handle. Mutates the pool state.
     * If the pool was built with thread-safety enabled, it can be called
     * concurrently with other functions that don't modify the state handle.
     */
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_get_connection(CompletionToken&& token = {}) BOOST_MYSQL_RETURN_TYPE(
        decltype(async_get_connection_impl(nullptr, std::forward<CompletionToken>(token)))
    )
    {
        return async_get_connection_impl(nullptr, std::forward<CompletionToken>(token));
    }

    /// \copydoc async_get_connection
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::pooled_connection))
            CompletionToken = with_diagnostics_t<asio::deferred_t>>
    auto async_get_connection(diagnostics& diag, CompletionToken&& token = {}) BOOST_MYSQL_RETURN_TYPE(
        decltype(async_get_connection_impl(nullptr, std::forward<CompletionToken>(token)))
    )
    {
        return async_get_connection_impl(&diag, std::forward<CompletionToken>(token));
    }

    /**
     * \brief Stops any current outstanding operation and marks the pool as cancelled.
     * \details
     * This function has the following effects:
     *
     * \li Stops the currently outstanding \ref async_run operation, if any, which will complete
     *     with a success error code.
     * \li Cancels any outstanding \ref async_get_connection operations.
     * \li Marks the pool as cancelled. Successive `async_get_connection` calls will
     *     fail immediately.
     *
     * This function will return immediately, without waiting for the cancelled operations to complete.
     *
     * You may call this function any number of times. Successive calls will have no effect.
     *
     * \par Preconditions
     * `this->valid() == true`
     *
     * \par Exception safety
     * Basic guarantee. Memory allocations and acquiring mutexes may throw.
     *
     * \par Thread-safety
     * Reads the internal state handle. Mutates the pool state.
     * If the pool was built with thread-safety enabled, it can be called
     * concurrently with other functions that don't modify the state handle.
     */
    BOOST_MYSQL_DECL
    void cancel();
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_pool.ipp>
#endif

#endif
