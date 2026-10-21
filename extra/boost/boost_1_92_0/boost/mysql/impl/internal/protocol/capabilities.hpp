//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_CAPABILITIES_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_CAPABILITIES_HPP

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

enum class capabilities : std::uint32_t
{
    // CLIENT_LONG_PASSWORD: Use the improved version of Old Password Authentication
    long_password = 1,

    // CLIENT_FOUND_ROWS: Send found rows instead of affected rows in EOF_Packet
    found_rows = 2,

    // CLIENT_LONG_FLAG: Get all column flags
    long_flag = 4,

    // CLIENT_CONNECT_WITH_DB: Database (schema) name can be specified on connect in Handshake Response Packet
    connect_with_db = 8,

    // CLIENT_NO_SCHEMA: Don't allow database.table.column
    no_schema = 16,

    // CLIENT_COMPRESS: Compression protocol supported
    compress = 32,

    // CLIENT_ODBC: Special handling of ODBC behavior
    odbc = 64,

    // CLIENT_LOCAL_FILES: Can use LOAD DATA LOCAL
    local_files = 128,

    // CLIENT_IGNORE_SPACE: Ignore spaces before '('
    ignore_space = 256,

    // CLIENT_PROTOCOL_41: New 4.1 protocol
    protocol_41 = 512,

    // CLIENT_INTERACTIVE: This is an interactive client
    interactive = 1024,

    // CLIENT_SSL: Use SSL encryption for the session
    ssl = 2048,

    // CLIENT_IGNORE_SIGPIPE: Client only flag
    ignore_sigpipe = 4096,

    // CLIENT_TRANSACTIONS: Client knows about transactions
    transactions = 8192,

    // CLIENT_RESERVED: DEPRECATED: Old flag for 4.1 protocol
    reserved = 16384,

    // CLIENT_SECURE_CONNECTION: DEPRECATED: Old flag for 4.1 authentication, required by MariaDB
    secure_connection = 32768,

    // CLIENT_MULTI_STATEMENTS: Enable/disable multi-stmt support
    multi_statements = (1UL << 16),

    // CLIENT_MULTI_RESULTS: Enable/disable multi-results
    multi_results = (1UL << 17),

    // CLIENT_PS_MULTI_RESULTS: Multi-results and OUT parameters in PS-protocol
    ps_multi_results = (1UL << 18),

    // CLIENT_PLUGIN_AUTH: Client supports plugin authentication
    plugin_auth = (1UL << 19),

    // CLIENT_CONNECT_ATTRS: Client supports connection attributes
    connect_attrs = (1UL << 20),

    // CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA: Enable authentication response packet to be larger than 255
    // bytes
    plugin_auth_lenenc_data = (1UL << 21),

    // CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS: Don't close the connection for a user account with expired
    // password
    can_handle_expired_passwords = (1UL << 22),

    // CLIENT_SESSION_TRACK: Capable of handling server state change information
    session_track = (1UL << 23),

    // CLIENT_DEPRECATE_EOF: Client no longer needs EOF_Packet and will use OK_Packet instead
    deprecate_eof = (1UL << 24),

    // CLIENT_SSL_VERIFY_SERVER_CERT: Verify server certificate
    ssl_verify_server_cert = (1UL << 30),

    // CLIENT_OPTIONAL_RESULTSET_METADATA: The client can handle optional metadata information in the
    // resultset
    optional_resultset_metadata = (1UL << 25),

    // CLIENT_REMEMBER_OPTIONS: Don't reset the options after an unsuccessful connect
    remember_options = (1UL << 31),
};

constexpr capabilities operator&(capabilities lhs, capabilities rhs)
{
    return static_cast<capabilities>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr capabilities operator|(capabilities lhs, capabilities rhs)
{
    return static_cast<capabilities>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

// Are all capabilities in subset in caps?
constexpr bool has_capabilities(capabilities caps, capabilities subset) { return (caps & subset) == subset; }

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
