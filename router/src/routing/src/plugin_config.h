/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef PLUGIN_CONFIG_ROUTING_INCLUDED
#define PLUGIN_CONFIG_ROUTING_INCLUDED

#include "mysqlrouter/routing_plugin_export.h"

#include <string>

#include "mysql/harness/config_option.h"
#include "mysql/harness/filesystem.h"  // Path
#include "mysql/harness/plugin_config.h"
#include "mysqlrouter/routing.h"  // RoutingStrategy, AccessMode
#include "protocol/protocol.h"    // Protocol::Type
#include "ssl_mode.h"
#include "tcp_address.h"

extern const std::array<const char *, 29> routing_supported_options;

/**
 * route specific configuration.
 */
class ROUTING_PLUGIN_EXPORT RoutingPluginConfig
    : public mysql_harness::BasePluginConfig {
 private:
  // is this [routing] entry for static routing or metadata-cache ?
  // it's mutable because we discover it while calling getter for
  // option destinations
  mutable bool metadata_cache_;

 public:
  /** Constructor.
   *
   * @param section from configuration file provided as ConfigSection
   */
  RoutingPluginConfig(const mysql_harness::ConfigSection *section);

  std::string get_default(const std::string &option) const override;
  bool is_required(const std::string &option) const override;

  uint16_t get_option_max_connections(
      const mysql_harness::ConfigSection *section);

  const Protocol::Type protocol;                 //!< protocol (classic, x)
  const std::string destinations;                //!< destinations
  const int bind_port;                           //!< TCP port to bind to
  const mysql_harness::TCPAddress bind_address;  //!< IP address to bind to
  const mysql_harness::Path
      named_socket;                //!< unix domain socket path to bind to
  const int connect_timeout;       //!< connect-timeout in seconds
  const routing::AccessMode mode;  //!< read-only/read-write
  routing::RoutingStrategy
      routing_strategy;       //!< routing strategy (next-avail, ...)
  const int max_connections;  //!< max connections allowed
  const unsigned long long max_connect_errors;  //!< max connect errors
  const unsigned int
      client_connect_timeout;            //!< client connect timeout in seconds
  const unsigned int net_buffer_length;  //!< Size of buffer to receive packets
  const unsigned int thread_stack_size;  //!< thread stack size in kilobytes

  SslMode source_ssl_mode;  //!< SslMode of the client side connection.
  const std::string source_ssl_cert;       //!< Cert file
  const std::string source_ssl_key;        //!< Key file
  const std::string source_ssl_cipher;     //!< allowed TLS ciphers
  const std::string source_ssl_curves;     //!< allowed TLS curves
  const std::string source_ssl_dh_params;  //!< DH params

  const SslMode dest_ssl_mode;      //!< SslMode of the server side connection.
  const SslVerify dest_ssl_verify;  //!< How to verify the server-side cert.
  const std::string dest_ssl_cipher;  //!< allowed TLS ciphers
  const std::string
      dest_ssl_ca_file;  //!< CA file to used to verify destinations' identity
  const std::string dest_ssl_ca_dir;  //!< directory of CA files used to verify
                                      //!< destinations' identity
  const std::string
      dest_ssl_crl_file;  //!< CRL file used to check revoked certificates
  const std::string dest_ssl_crl_dir;  //!< directory of CRL files
  const std::string dest_ssl_curves;   //!< allowed TLS curves

  const std::chrono::seconds unreachable_destination_refresh_interval;
};

#endif  // PLUGIN_CONFIG_ROUTING_INCLUDED
