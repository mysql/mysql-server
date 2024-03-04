/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_ROUTING_SUPPORTED_ROUTING_INCLUDED
#define MYSQLROUTER_ROUTING_SUPPORTED_ROUTING_INCLUDED

#include <array>
#include <string_view>

static constexpr std::array<const char *, 48> routing_supported_options{
    "protocol",
    "destinations",
    "bind_port",
    "bind_address",
    "socket",
    "connect_timeout",
    "routing_strategy",
    "max_connect_errors",
    "max_connections",
    "client_connect_timeout",
    "net_buffer_length",
    "thread_stack_size",
    "client_ssl_mode",
    "client_ssl_cert",
    "client_ssl_key",
    "client_ssl_cipher",
    "client_ssl_ca",
    "client_ssl_capath",
    "client_ssl_crl",
    "client_ssl_crlpath",
    "client_ssl_curves",
    "client_ssl_dh_params",
    "server_ssl_mode",
    "server_ssl_cert",
    "server_ssl_key",
    "server_ssl_verify",
    "server_ssl_cipher",
    "server_ssl_ca",
    "server_ssl_capath",
    "server_ssl_crl",
    "server_ssl_crlpath",
    "server_ssl_curves",
    "connection_sharing",
    "connection_sharing_delay",
    "client_ssl_session_cache_mode",
    "client_ssl_session_cache_size",
    "client_ssl_session_cache_timeout",
    "server_ssl_session_cache_mode",
    "server_ssl_session_cache_size",
    "server_ssl_session_cache_timeout",
    "connect_retry_timeout",
    "access_mode",
    "wait_for_my_writes",
    "wait_for_my_writes_timeout",
    "router_require_enforce",
};

#endif /* MYSQLROUTER_ROUTING_SUPPORTED_ROUTING_INCLUDED */
