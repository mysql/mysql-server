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

namespace routing {
namespace options {
static constexpr const char kProtocol[]{"protocol"};
static constexpr const char kDestinations[]{"destinations"};
static constexpr const char kBindPort[]{"bind_port"};
static constexpr const char kBindAddress[]{"bind_address"};
static constexpr const char kMaxConnections[]{"max_connections"};
static constexpr const char kConnectTimeout[]{"connect_timeout"};
static constexpr const char kSocket[]{"socket"};
static constexpr const char kMaxConnectErrors[]{"max_connect_errors"};
static constexpr const char kRoutingStrategy[]{"routing_strategy"};
static constexpr const char kClientConnectTimeout[]{"client_connect_timeout"};
static constexpr const char kNetBufferLength[]{"net_buffer_length"};
static constexpr const char kThreadStackSize[]{"thread_stack_size"};
static constexpr const char kClientSslMode[]{"client_ssl_mode"};
static constexpr const char kClientSslCert[]{"client_ssl_cert"};
static constexpr const char kClientSslKey[]{"client_ssl_key"};
static constexpr const char kClientSslCipher[]{"client_ssl_cipher"};
static constexpr const char kClientSslCa[]{"client_ssl_ca"};
static constexpr const char kClientSslCaPath[]{"client_ssl_capath"};
static constexpr const char kClientSslCrl[]{"client_ssl_crl"};
static constexpr const char kClientSslCrlPath[]{"client_ssl_crlpath"};
static constexpr const char kClientSslCurves[]{"client_ssl_curves"};
static constexpr const char kClientSslDhParams[]{"client_ssl_dh_params"};
static constexpr const char kServerSslMode[]{"server_ssl_mode"};
static constexpr const char kServerSslCert[]{"server_ssl_cert"};
static constexpr const char kServerSslKey[]{"server_ssl_key"};
static constexpr const char kServerSslVerify[]{"server_ssl_verify"};
static constexpr const char kServerSslCipher[]{"server_ssl_cipher"};
static constexpr const char kServerSslCa[]{"server_ssl_ca"};
static constexpr const char kServerSslCaPath[]{"server_ssl_capath"};
static constexpr const char kServerSslCrl[]{"server_ssl_crl"};
static constexpr const char kServerSslCrlPath[]{"server_ssl_crlpath"};
static constexpr const char kServerSslCurves[]{"server_ssl_curves"};
static constexpr const char kConnectionSharing[]{"connection_sharing"};
static constexpr const char kConnectionSharingDelay[]{
    "connection_sharing_delay"};

static constexpr const char kClientSslSessionCacheMode[]{
    "client_ssl_session_cache_mode"};
static constexpr const char kClientSslSessionCacheSize[]{
    "client_ssl_session_cache_size"};
static constexpr const char kClientSslSessionCacheTimeout[]{
    "client_ssl_session_cache_timeout"};

static constexpr const char kServerSslSessionCacheMode[]{
    "server_ssl_session_cache_mode"};
static constexpr const char kServerSslSessionCacheSize[]{
    "server_ssl_session_cache_size"};
static constexpr const char kServerSslSessionCacheTimeout[]{
    "server_ssl_session_cache_timeout"};

static constexpr const char kConnectRetryTimeout[]{"connect_retry_timeout"};
static constexpr const char kAccessMode[]{"access_mode"};
static constexpr const char kWaitForMyWrites[]{"wait_for_my_writes"};
static constexpr const char kWaitForMyWritesTimeout[]{
    "wait_for_my_writes_timeout"};
static constexpr const char kRouterRequireEnforce[]{"router_require_enforce"};
}  // namespace options
}  // namespace routing

static constexpr std::array routing_supported_options{
    routing::options::kProtocol,
    routing::options::kDestinations,
    routing::options::kBindPort,
    routing::options::kBindAddress,
    routing::options::kSocket,
    routing::options::kConnectTimeout,
    routing::options::kRoutingStrategy,
    routing::options::kMaxConnectErrors,
    routing::options::kMaxConnections,
    routing::options::kClientConnectTimeout,
    routing::options::kNetBufferLength,
    routing::options::kThreadStackSize,
    routing::options::kClientSslMode,
    routing::options::kClientSslCert,
    routing::options::kClientSslKey,
    routing::options::kClientSslCipher,
    routing::options::kClientSslCa,
    routing::options::kClientSslCaPath,
    routing::options::kClientSslCrl,
    routing::options::kClientSslCrlPath,
    routing::options::kClientSslCurves,
    routing::options::kClientSslDhParams,
    routing::options::kServerSslMode,
    routing::options::kServerSslCert,
    routing::options::kServerSslKey,
    routing::options::kServerSslCipher,
    routing::options::kServerSslCa,
    routing::options::kServerSslCaPath,
    routing::options::kServerSslCrl,
    routing::options::kServerSslCrlPath,
    routing::options::kServerSslCurves,
    routing::options::kServerSslVerify,
    routing::options::kConnectionSharing,
    routing::options::kConnectionSharingDelay,
    routing::options::kClientSslSessionCacheMode,
    routing::options::kClientSslSessionCacheSize,
    routing::options::kClientSslSessionCacheTimeout,
    routing::options::kServerSslSessionCacheMode,
    routing::options::kServerSslSessionCacheSize,
    routing::options::kServerSslSessionCacheTimeout,
    routing::options::kConnectRetryTimeout,
    routing::options::kAccessMode,
    routing::options::kWaitForMyWrites,
    routing::options::kWaitForMyWritesTimeout,
    routing::options::kRouterRequireEnforce,
};

#endif /* MYSQLROUTER_ROUTING_SUPPORTED_ROUTING_INCLUDED */
