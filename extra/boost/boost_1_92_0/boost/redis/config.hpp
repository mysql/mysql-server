/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONFIG_HPP
#define BOOST_REDIS_CONFIG_HPP

#include <boost/redis/request.hpp>

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace boost::redis {

/// Address of a Redis server.
struct address {
   /// Redis host.
   std::string host = "127.0.0.1";
   /// Redis port.
   std::string port = "6379";
};

/** @brief Compares two addresses for equality.
 *  @relates address
 *
 *  @param a Left hand side address.
 *  @param b Right hand side address.
 */
inline bool operator==(address const& a, address const& b)
{
   return a.host == b.host && a.port == b.port;
}

/** @brief Compares two addresses for inequality.
 *  @relates address
 *
 *  @param a Left hand side address.
 *  @param b Right hand side address.
 */
inline bool operator!=(address const& a, address const& b) { return !(a == b); }

/// Identifies the possible roles of a Redis server.
enum class role
{
   /// The server is a master.
   master,

   /// The server is a replica.
   replica,
};

/// Configuration values to use when using Sentinel.
struct sentinel_config {
   /**
    * @brief A list of (hostname, port) pairs where the Sentinels are listening.
    *
    * Sentinels in this list will be contacted in order, until a successful
    * connection is made. At this point, the `SENTINEL SENTINELS` command
    * will be used to retrieve any additional Sentinels monitoring the configured master.
    * Thus, it is not required to keep this list comprehensive - if Sentinels are added
    * later, they will be detected at runtime.
    *
    * Sentinel will only be used if this value is not empty.
    *
    * Numeric IP addresses are also allowed as hostnames.
    */
   std::vector<address> addresses{};

   /**
    * @brief The name of the master to connect to, as configured in the
    *        `sentinel monitor` statement in `sentinel.conf`.
    *
    * This field is required even when connecting to replicas.
    */
   std::string master_name{};

   /**
    * @brief Whether connections to Sentinels should use TLS or not.
    *        Does not affect connections to masters.
    *
    * When set to `true`, physical connections to Sentinels will be established
    * using TLS. This setting does *not* influence how masters and replicas are contacted.
    * To use TLS when connecting to these, set @ref config::use_ssl to `true`.
    */
   bool use_ssl = false;

   /**
    * @brief A request to be sent to Sentinels upon connection establishment.
    *
    * This request is executed every time a Sentinel is contacted, and before
    * commands like `SENTINEL GET-MASTER-NAME-BY-ADDR` are run.
    * By default, this field contains a `HELLO 3` command.
    * You can use this request to set up any authorization required by Sentinels.
    *
    * This request should ensure that the connection is upgraded to RESP3
    * by executing `HELLO 3` or similar. RESP2 is not supported yet.
    */
   request setup = detail::make_hello_request();

   /**
    * @brief Time span that the Sentinel resolve operation is allowed to elapse.
    *        Does not affect connections to masters and replicas, controlled by @ref config::resolve_timeout.
    */
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::milliseconds{500};

   /**
    * @brief Time span that the Sentinel connect operation is allowed to elapse.
    *        Does not affect connections to masters and replicas, controlled by @ref config::connect_timeout.
    */
   std::chrono::steady_clock::duration connect_timeout = std::chrono::milliseconds{500};

   /**
    * @brief Time span that the Sentinel TLS handshake operation is allowed to elapse.
    *        Does not affect connections to masters and replicas, controlled by @ref config::ssl_handshake_timeout.
    */
   std::chrono::steady_clock::duration ssl_handshake_timeout = std::chrono::seconds{5};

   /**
    * @brief Time span that the Sentinel request/response exchange is allowed to elapse.
    *        Includes executing the commands in @ref setup and the commands required to
    *        resolve the server's address.
    */
   std::chrono::steady_clock::duration request_timeout = std::chrono::seconds{5};

   /**
    * @brief Whether to connect to a Redis master or to a replica.
    *
    * The library resolves and connects to the Redis master, by default.
    * Set this value to @ref role::replica to connect to one of the replicas
    * of the master identified by @ref master_name.
    * The particular replica will be chosen randomly.
    */
   role server_role = role::master;
};

/// Configure parameters used by the connection classes.
struct config {
   /**
    * @brief Whether to use TLS instead of plaintext connections.
    *
    * When using Sentinel, configures whether to use TLS when connecting to masters and replicas.
    * Use @ref sentinel_config::use_ssl to control TLS for Sentinels.
    */
   bool use_ssl = false;

   /// For TCP connections, hostname and port of the Redis server. Ignored when using Sentinel.
   address addr = address{"127.0.0.1", "6379"};

   /**
    * @brief The UNIX domain socket path where the server is listening.
    *
    * If non-empty, communication with the server will happen using
    * UNIX domain sockets, and @ref addr will be ignored.
    * 
    * UNIX domain sockets can't be used with SSL: if `unix_socket` is non-empty,
    * @ref use_ssl must be `false`. UNIX domain sockets can't be used with Sentinel, either.
    *
    * UNIX domain sockets can't be used with Sentinel.
    */
   std::string unix_socket{};

   /** @brief (Deprecated) Username used for authentication during connection establishment.
    * 
    * If @ref use_setup is false (the default), during connection establishment,
    * authentication is performed by sending a `HELLO` command.
    * This field contains the username to employ.
    *
    * If the username equals the literal `"default"` (the default)
    * and no password is specified, the `HELLO` command is sent
    * without authentication parameters.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    * Use @ref sentinel_config::setup to configure authorization for Sentinels.
    *
    * @par Deprecated
    * This setting is deprecated and will be removed in a subsequent release.
    * Please set @ref setup, instead:
    *
    * @code
    * cfg.use_setup = true;
    * cfg.setup.clear();
    * cfg.setup.hello("my_username", "my_password");
    * @endcode
    */
   std::string username = "default";

   /** @brief (Deprecated) Password used for authentication during connection establishment.
    * 
    * If @ref use_setup is false (the default), during connection establishment,
    * authentication is performed by sending a `HELLO` command.
    * This field contains the password to employ.
    *
    * If the username equals the literal `"default"` (the default)
    * and no password is specified, the `HELLO` command is sent
    * without authentication parameters.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    * Use @ref sentinel_config::setup to configure authorization for Sentinels.
    *
    * @par Deprecated
    * This setting is deprecated and will be removed in a subsequent release.
    * Please set @ref setup, instead:
    *
    * @code
    * cfg.use_setup = true;
    * cfg.setup.clear();
    * cfg.setup.hello("my_username", "my_password");
    * @endcode
    */
   std::string password{};

   /** @brief (Deprecated) Client name parameter to use during connection establishment.
    * 
    * If @ref use_setup is false (the default), during connection establishment,
    * a `HELLO` command is sent. If this field is not empty, the `HELLO` command
    * will contain a `SETNAME` subcommand containing this value.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    * Use @ref sentinel_config::setup to configure this value for Sentinels.
    *
    * @par Deprecated
    * This setting is deprecated and will be removed in a subsequent release.
    * Please set @ref setup, instead:
    *
    * @code
    * cfg.use_setup = true;
    * cfg.setup.clear();
    * cfg.setup.hello_setname("my_client_name");
    * @endcode
    */
   std::string clientname = "Boost.Redis";

   /** @brief (Deprecated) Database index to pass to the `SELECT` command during connection establishment.
    * 
    * If @ref use_setup is false (the default), and this field is set to a
    * non-empty optional, and its value is different than zero,
    * a `SELECT` command will be issued during connection establishment to set the logical
    * database index. By default, no `SELECT` command is sent.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    *
    * @par Deprecated
    * This setting is deprecated and will be removed in a subsequent release.
    * Please set @ref setup, instead:
    *
    * @code
    * cfg.use_setup = true;
    * cfg.setup.push("SELECT", 4); // select database index 4
    * @endcode
    */
   std::optional<int> database_index = 0;

   /// Message used by `PING` commands sent by the health checker.
   std::string health_check_id = "Boost.Redis";

   /**
    * @brief (Deprecated) Sets the logger prefix, a string printed before log messages.
    * 
    * Setting a prefix in this struct is deprecated. If you need to change how log messages
    * look like, please construct a logger object passing a formatting function, and use that
    * logger in connection's constructor. This member will be removed in subsequent releases.
    */
   std::string log_prefix = "(Boost.Redis) ";

   /**
    * @brief Time span that the resolve operation is allowed to elapse.
    *        When using Sentinel, this setting applies to masters and replicas.
    */
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};

   /**
    * @brief Time span that the connect operation is allowed to elapse.
    *        When using Sentinel, this setting applies to masters and replicas.
    */
   std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};

   /**
    * @brief Time span that the SSL handshake operation is allowed to elapse.
    *        When using Sentinel, this setting applies to masters and replicas.
    */
   std::chrono::steady_clock::duration ssl_handshake_timeout = std::chrono::seconds{10};

   /** @brief Time span between successive health checks.
    *  Set to zero to disable health-checks.
    *
    * When this value is set to a non-zero duration, @ref basic_connection::async_run
    * will issue `PING` commands whenever no command is sent to the server for more
    * than `health_check_interval`. You can configure the message passed to the `PING`
    * command using @ref health_check_id.
    *
    * Enabling health checks also sets timeouts to individual network
    * operations. The connection is considered dead if:
    *
    * @li No byte can be written to the server after `health_check_interval`.
    * @li No byte is read from the server after `2 * health_check_interval`.
    *
    * If the health checker finds that the connection is unresponsive, it will be closed,
    * and a reconnection will be triggered, as if a network error had occurred.
    *
    * The exact timeout values are *not* part of the interface, and might change
    * in future versions.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    * Sentinels are not health-checked.
    */
   std::chrono::steady_clock::duration health_check_interval = std::chrono::seconds{2};

   /** @brief Time span to wait between successive connection retries.
    *         Set to zero to disable reconnection.
    *
    * When using Sentinel, this setting applies to masters, replicas and Sentinels.
    * If none of the configured Sentinels can be contacted, this time span will
    * be waited before trying again. After a connection error with a master or replica
    * is encountered, this time span will be waited before contacting Sentinels again.
    */
   std::chrono::steady_clock::duration reconnect_wait_interval = std::chrono::seconds{1};

   /** @brief Maximum size of the socket read-buffer in bytes.
    *  
    * Sets a limit on how large the read-buffer is allowed to grow. It can be
    * used to prevent DDOS.
    *
    * When using Sentinel, this setting applies to masters, replicas and Sentinels.
    */
   std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();

   /** @brief Grow size of the read buffer.
    *
    * The size by which the read buffer grows when more space is
    * needed. This can help avoiding some memory allocations. Once the
    * maximum size is reached no more memory allocations are made
    * since the buffer is reused.
    *
    * When using Sentinel, this setting applies to masters, replicas and Sentinels.
    */
   std::size_t read_buffer_append_size = 4096;

   /** @brief Enables using a custom requests during connection establishment.
    * 
    * If set to true, the @ref setup member will be sent to the server immediately after
    * connection establishment. Every time a reconnection happens, the setup
    * request will be executed before any other request.
    * It can be used to perform authentication,
    * subscribe to channels or select a database index.
    *
    * When set to true, *the custom setup request replaces the built-in HELLO
    * request generated by the library*. The @ref username, @ref password,
    * @ref clientname and @ref database_index fields *will be ignored*.
    *
    * By default, @ref setup contains a `"HELLO 3"` command, which upgrades the
    * protocol to RESP3. You might modify this request as you like,
    * but you should ensure that the resulting connection uses RESP3.
    *
    * To prevent sending any setup request at all, set this field to true
    * and @ref setup to an empty request. This can be used to interface with
    * systems that don't support `HELLO`.
    *
    * By default, this field is false, and @ref setup will not be used.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    * Use @ref sentinel_config::setup for Sentinels.
    */
   bool use_setup = false;

   /** @brief Request to be executed after connection establishment.
    *
    * This member is only used if @ref use_setup is `true`. Please consult
    * @ref use_setup docs for more info.
    *
    * By default, `setup` contains a `"HELLO 3"` command.
    *
    * When using Sentinel, this setting applies to masters and replicas.
    * Use @ref sentinel_config::setup for Sentinels.
    */
   request setup = detail::make_hello_request();

   /**
    * @brief Configuration values for Sentinel. Sentinel is enabled only if
    *        @ref sentinel_config::addresses is not empty.
    */
   sentinel_config sentinel{};
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONFIG_HPP
