/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_NETWORK_PROVIDER_H
#define XCOM_NETWORK_PROVIDER_H

//#include
//"plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_thread.h"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "xcom/network/include/network_provider.h"

#include "xcom/network/xcom_network_provider_native_lib.h"
#include "xcom/result.h"
#include "xcom/xcom_common.h"

class Xcom_network_provider : public Network_provider {
 public:
  Xcom_network_provider(Xcom_network_provider &param) = delete;
  Xcom_network_provider(Xcom_network_provider &&param)
      : Network_provider(std::move(param)) {}

  /**
   * @brief Construct a new Xcom_network_provider object
   */
  Xcom_network_provider()
      : m_port(0),
        m_initialized(false),
        m_init_error(false),
        m_shutdown_tcp_server(false),
        m_open_server_socket{0, 0} {}

  virtual ~Xcom_network_provider() override {}

  /**
   * Inherited methods from Gcs_network_provider
   */
  std::pair<bool, int> start() override;
  std::pair<bool, int> stop() override;
  enum_transport_protocol get_communication_stack() const override {
    return XCOM_PROTOCOL;
  }

  /**
   * @brief Configures this network provider. It is mandatory to be called in
   *        This provider, else we won't know which listen port to use.
   *
   * @param params Network_configuration_parameters with the listen port
   *               configured
   * @return true if configure went well. False otherwise.
   */
  bool configure(const Network_configuration_parameters &params) override {
    m_port = params.port;

    return true;
  }

  bool configure_secure_connections(
      const Network_configuration_parameters &params) override {
    bool const successful =
        (Xcom_network_provider_ssl_library::xcom_init_ssl(
             params.ssl_params.server_key_file,
             params.ssl_params.server_cert_file,
             params.ssl_params.client_key_file,
             params.ssl_params.client_cert_file, params.ssl_params.ca_file,
             params.ssl_params.ca_path, params.ssl_params.crl_file,
             params.ssl_params.crl_path, params.ssl_params.cipher,
             params.tls_params.tls_version,
             params.tls_params.tls_ciphersuites) == 1);
    return successful;
  }

  bool cleanup_secure_connections_context() override;

  bool finalize_secure_connections_context() override;

  std::unique_ptr<Network_connection> open_connection(
      const std::string &address, const unsigned short port,
      const Network_security_credentials &security_credentials,
      int connection_timeout =
          Network_provider::default_connection_timeout()) override;

  int close_connection(const Network_connection &connection) override;

  /**
   * @brief Waits for the provider to become ready. This call is blocking.
   *
   * @return true in case of error or timeout
   * @return false in case of success
   */
  bool wait_for_provider_ready();

  /**
   * @brief Notify that the provider is ready. It unblocks
   * wait_for_provider_ready()
   *
   * @param init_error sets the error state of this notifier
   */
  void notify_provider_ready(bool init_error = false);

  xcom_port get_port() const { return m_port; }
  void set_port(xcom_port port) { m_port = port; }

  bool is_provider_initialized() const {
    std::lock_guard<std::mutex> lck(m_init_lock);
    return m_initialized;
  }

  bool should_shutdown_tcp_server() const { return m_shutdown_tcp_server; }
  void set_shutdown_tcp_server(bool shutdown_tcp_server) {
    m_shutdown_tcp_server = shutdown_tcp_server;
  }

  result get_open_server_socket() const { return m_open_server_socket; }
  void set_open_server_socket(result open_socket) {
    m_open_server_socket = open_socket;
  }

 private:
  xcom_port m_port;
  std::thread m_network_provider_tcp_server;

  bool m_initialized;
  bool m_init_error;
  mutable std::mutex m_init_lock;
  mutable std::condition_variable m_init_cond_var;

  bool m_shutdown_tcp_server;

  result m_open_server_socket;
};

#endif  // XCOM_NETWORK_PROVIDER_H
