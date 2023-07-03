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

  Protocol::Type protocol;                 //!< protocol (classic, x)
  std::string destinations;                //!< destinations
  int bind_port;                           //!< TCP port to bind to
  mysql_harness::TCPAddress bind_address;  //!< IP address to bind to
  mysql_harness::Path named_socket;  //!< unix domain socket path to bind to
  int connect_timeout;               //!< connect-timeout in seconds
  routing::AccessMode mode;          //!< read-only/read-write
  routing::RoutingStrategy
      routing_strategy;  //!< routing strategy (next-avail, ...)
  int max_connections;   //!< max connections allowed
  unsigned long long max_connect_errors;  //!< max connect errors
  unsigned int client_connect_timeout;    //!< client connect timeout in seconds
  unsigned int net_buffer_length;         //!< Size of buffer to receive packets
  unsigned int thread_stack_size;         //!< thread stack size in kilobytes

  SslMode source_ssl_mode;           //!< SslMode of the client side connection.
  std::string source_ssl_cert;       //!< Cert file
  std::string source_ssl_key;        //!< Key file
  std::string source_ssl_cipher;     //!< allowed TLS ciphers
  std::string source_ssl_curves;     //!< allowed TLS curves
  std::string source_ssl_dh_params;  //!< DH params

  SslMode dest_ssl_mode;        //!< SslMode of the server side connection.
  SslVerify dest_ssl_verify;    //!< How to verify the server-side cert.
  std::string dest_ssl_cipher;  //!< allowed TLS ciphers
  std::string
      dest_ssl_ca_file;  //!< CA file to used to verify destinations' identity
  std::string dest_ssl_ca_dir;  //!< directory of CA files used to verify
                                //!< destinations' identity
  std::string
      dest_ssl_crl_file;  //!< CRL file used to check revoked certificates
  std::string dest_ssl_crl_dir;  //!< directory of CRL files
  std::string dest_ssl_curves;   //!< allowed TLS curves

  bool connection_sharing;  //!< if connection sharing is allowed.
  std::chrono::milliseconds
      connection_sharing_delay;  //!< delay before an idling connection is
                                 //!< moved to the pool and connection sharing
                                 //!< is allowed.
};

#endif  // PLUGIN_CONFIG_ROUTING_INCLUDED
