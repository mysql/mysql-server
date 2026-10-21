//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_POOL_PARAMS_HPP
#define BOOST_MYSQL_POOL_PARAMS_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/defaults.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/assert.hpp>
#include <boost/optional/optional.hpp>

#include <chrono>
#include <cstddef>
#include <string>

namespace boost {
namespace mysql {

/// Type tag for \ref pool_charset_strategy.
enum class pool_charset_strategy_type
{
    /// Connections are set to use `utf8mb4` explicitly.
    set_to_utf8mb4,

    /// Connections use the server's default character set, which is supplied by the user.
    use_server_default,
};

/**
 * \brief Describes how pooled connections manage their character set.
 * \details
 * This is a lightweight variant-like type that determines how connections in a pool
 * set its character set and restore it when they are returned
 * to the pool. Create instances using the \ref set_to_utf8mb4 and \ref use_server_default
 * factory functions.
 *
 * \li When using \ref set_to_utf8mb4 (the default), connections are established using
 *     a collation ID of `utf8mb4_general_ci`, and a `SET NAMES` statement is issued
 *     after returning them to the pool and resetting them. The character set is always
 *     known and equal to `utf8mb4`.
 * \li When using \ref use_server_default, connections are established using
 *     \ref invalid_collation_id as collation ID, causing them to use the server's default.
 *     No `SET NAMES` statement is issued after resetting the connections,
 *     causing them again to use the server's default.
 *     When using this value, you must provide the server's default character set
 *     as an argument, and it must match what is actually configured in the server
 *     (no runtime check is performed). This option should only be used as an optimization,
 *     and if you know what you are doing.
 */
class pool_charset_strategy
{
    pool_charset_strategy_type type_{pool_charset_strategy_type::set_to_utf8mb4};
    character_set server_default_charset_{};

    explicit pool_charset_strategy(character_set c)
        : type_(pool_charset_strategy_type::use_server_default), server_default_charset_(c)
    {
    }

public:
    /**
     * \brief Default constructor, equivalent to \ref set_to_utf8mb4.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pool_charset_strategy() = default;

    /**
     * \brief Creates a strategy that makes pooled connections use `utf8mb4`.
     * \details
     * Creates an object with type \ref pool_charset_strategy_type::set_to_utf8mb4.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    static inline pool_charset_strategy set_to_utf8mb4() noexcept { return {}; }

    /**
     * \brief Creates a strategy that makes pooled connections use the server's default character set.
     * \details
     * Creates an object with type \ref pool_charset_strategy_type::use_server_default
     * and a \ref server_default_charset equal to the passed charset.
     *
     * The passed `charset` must equal the server's configured default character set.
     * No runtime check is performed to verify this assertion. If it does not hold,
     * vulnerabilities may arise. If unsure, prefer \ref set_to_utf8mb4.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    static inline pool_charset_strategy use_server_default(character_set charset) noexcept
    {
        return pool_charset_strategy(charset);
    }

    /**
     * \brief Retrieves the kind of strategy represented by this object.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pool_charset_strategy_type type() const noexcept { return type_; }

    /**
     * \brief Retrieves the server's default character set.
     *
     * \details Returns the character set that was passed to \ref use_server_default.
     *
     * \par Preconditions
     * `this->type() == pool_charset_strategy_type::use_server_default`
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    character_set server_default_charset() const noexcept
    {
        BOOST_ASSERT(type_ == pool_charset_strategy_type::use_server_default);
        return server_default_charset_;
    }
};

/**
 * \brief Configuration parameters for \ref connection_pool.
 * \details
 * This is an owning type.
 */
struct pool_params
{
    /**
     * \brief Determines how to establish a physical connection to the MySQL server.
     * \details
     * Connections created by the pool will use this address to connect to the
     * server. This can be either a host and port or a UNIX socket path.
     * Defaults to (localhost, 3306).
     */
    any_address server_address;

    /// User name that connections created by the pool should use to authenticate as.
    std::string username;

    /// Password that connections created by the pool should use.
    std::string password;

    /**
     * \brief Database name that connections created by the pool will use when connecting.
     * \details Leave it empty to select no database (this is the default).
     */
    std::string database;

    /**
     * \brief Controls whether connections created by the pool will use TLS or not.
     * \details
     * See \ref ssl_mode for more information about the possible modes.
     * This option is only relevant when `server_address.type() == address_type::host_and_port`.
     * UNIX socket connections will never use TLS, regardless of this value.
     */
    ssl_mode ssl{ssl_mode::enable};

    /**
     * \brief Whether to enable support for semicolon-separated text queries for connections created by the
     * pool. \details Disabled by default.
     */
    bool multi_queries{false};

    /// Initial size (in bytes) of the internal buffer for the connections created by the pool.
    std::size_t initial_buffer_size{default_initial_read_buffer_size};

    /**
     * \brief Initial number of connections to create.
     * \details
     * When \ref connection_pool::async_run starts running, this number of connections
     * will be created and connected.
     */
    std::size_t initial_size{1};

    /**
     * \brief Max number of connections to create.
     * \details
     * When a connection is requested, but all connections are in use, new connections
     * will be created and connected up to this size.
     * \n
     * Defaults to the maximum number of concurrent connections that MySQL
     * servers allow by default. If you increase this value, increase the server's
     * max number of connections, too (by setting the `max_connections` global variable).
     * \n
     * This value must be `> 0` and `>= initial_size`.
     */
    std::size_t max_size{151};

    /**
     * \brief The SSL context to use for connections using TLS.
     * \details
     * If a non-empty value is provided, all connections created by the pool
     * will use the passed context when using TLS. This allows setting TLS options
     * to pool-created connections.
     * \n
     * If an empty value is passed (the default) and the connections require TLS,
     * an internal SSL context with suitable options will be created by the pool.
     */
    boost::optional<asio::ssl::context> ssl_ctx{};

    /**
     * \brief The timeout to use when connecting.
     * \details
     * Connections will be connected by the pool before being handed to the user
     * (using \ref any_connection::async_connect).
     * If the operation takes longer than this timeout,
     * the operation will be interrupted, considered as failed and retried later.
     * \n
     * Set this timeout to zero to disable it.
     * \n
     * This value must not be negative.
     */
    std::chrono::steady_clock::duration connect_timeout{std::chrono::seconds(20)};

    /**
     * \brief The interval between connect attempts.
     * \details
     * When session establishment fails, the operation will be retried until
     * success. This value determines the interval between consecutive connection
     * attempts.
     * \n
     * This value must be greater than zero.
     */
    std::chrono::steady_clock::duration retry_interval{std::chrono::seconds(30)};

    /**
     * \brief The health-check interval.
     * \details
     * If a connection becomes idle and hasn't been handed to the user for
     * `ping_interval`, a health-check will be performed (using \ref any_connection::async_ping).
     * Pings will be sent with a periodicity of `ping_interval` until the connection
     * is handed to the user, or a ping fails.
     *
     * Health checks serve two purposes:
     *
     *   \li They prevent connections from closing during period of no activity.
     *   \li They detect network errors and trigger reconnection. Without health checks,
     *       you may be handed over connections that are no longer functional, but
     *       haven't been diagnosed yet.
     *
     * Set this interval to zero to disable pings.
     *
     * This value must not be negative. It should be smaller than the server's
     * idle timeout (as determined by the
     * <a
     * href="https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html#sysvar_wait_timeout">wait_timeout</a>
     * session variable). Otherwise, the server might close connections
     * without the pool detecting it.
     */
    std::chrono::steady_clock::duration ping_interval{std::chrono::seconds(10)};

    /**
     * \brief The timeout to use for pings and session resets.
     * \details
     * If pings (as per \ref any_connection::async_ping) or session resets
     * (as per \ref any_connection::async_reset_connection) take longer than this
     * timeout, they will be cancelled, and the operation will be considered failed.
     * \n
     * Set this timeout to zero to disable it.
     * \n
     * This value must not be negative.
     */
    std::chrono::steady_clock::duration ping_timeout{std::chrono::seconds(10)};

    /**
     * \brief Enables or disables thread-safety.
     * \details
     * When set to `true`, the resulting connection pool are able to
     * be shared between threads at the cost of some performance.
     *
     * Enabling thread safety for a pool creates an internal `asio::strand` object
     * wrapping the executor passed to the pool's constructor.
     * All state-mutating functions (including \ref connection_pool::async_run,
     * \ref connection_pool::async_get_connection and returning connections)
     * will be run through the created strand.
     *
     * Thread-safety doesn't extend to individual connections: \ref pooled_connection
     * objects can't be shared between threads. Thread-safety does not protect
     * objects that don't belong to the pool. For instance, `asio::cancel_after`
     * creates a timer that must be protected with a strand.
     * Refer to
     * <a href="../../connection_pool.html#mysql.connection_pool.thread_safe">this
     * page</a> for more info.
     */
    bool thread_safe{false};

    /**
     * \brief The executor to be used by individual connections created by the pool.
     * \details
     * If this member is set to a non-empty value
     * (that is, `static_cast<bool>(connection_executor) == true`),
     * individual connections will be created using this executor.
     * Otherwise, connections will use the pool's executor
     * (as per \ref connection_pool::get_executor).
     */
    asio::any_io_executor connection_executor{};

    /**
     * \brief Determines the character set used by connections created by the pool.
     * \details
     * Controls how pooled connections manage their character set.
     * Defaults to \ref pool_charset_strategy::set_to_utf8mb4,
     * which makes all connections use `utf8mb4` by setting it explicitly.
     *
     * This is an advanced setting. Please read how character set tracking works
     * before changing it.
     */
    pool_charset_strategy charset_strategy{};
};

}  // namespace mysql
}  // namespace boost

#endif
