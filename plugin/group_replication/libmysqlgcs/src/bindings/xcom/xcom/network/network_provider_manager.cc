/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include "xcom/task_debug.h"

#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif /* _WIN32 */

#include <openssl/err.h>
#include <openssl/ssl.h>
#endif /*! XCOM_WITHOUT_OPENSSL*/

#include "xcom/network/network_provider_manager.h"

#include "my_compiler.h"

static const char *ssl_mode_options[] = {"DISABLED", "PREFERRED", "REQUIRED",
                                         "VERIFY_CA", "VERIFY_IDENTITY"};

static const char *ssl_fips_mode_options[] = {"OFF", "ON", "STRICT"};

#define SSL_MODE_OPTIONS_COUNT \
  (sizeof(ssl_mode_options) / sizeof(*ssl_mode_options))

#define SSL_MODE_FIPS_OPTIONS_COUNT \
  (sizeof(ssl_fips_mode_options) / sizeof(*ssl_fips_mode_options))

bool Network_provider_manager::initialize() {
  // Add the default provider, which is XCom
  m_xcom_network_provider = std::make_shared<Xcom_network_provider>();
  this->add_network_provider(m_xcom_network_provider);

  return false;
}

bool Network_provider_manager::finalize() {
  this->cleanup_secure_connections_context();
  this->finalize_secure_connections_context();

  // Remove the default provider, which is XCom
  this->remove_network_provider(XCOM_PROTOCOL);

  return false;
}

void Network_provider_manager::add_network_provider(
    std::shared_ptr<Network_provider> provider) {
  if (m_network_providers.find(provider->get_communication_stack()) !=
      m_network_providers.end()) {
    this->stop_network_provider(provider->get_communication_stack());
    this->remove_network_provider(provider->get_communication_stack());
  }

  m_network_providers.emplace(provider->get_communication_stack(), provider);
}

void Network_provider_manager::add_and_start_network_provider(
    std::shared_ptr<Network_provider> provider) {
  enum_transport_protocol key = provider->get_communication_stack();

  this->add_network_provider(provider);
  this->start_network_provider(key);
}

void Network_provider_manager::remove_network_provider(
    enum_transport_protocol provider_key) {
  m_network_providers.erase(provider_key);
}

void Network_provider_manager::remove_all_network_provider() {
  m_network_providers.clear();
}

bool Network_provider_manager::start_network_provider(
    enum_transport_protocol provider_key) {
  auto net_provider = this->get_provider(provider_key);

  return net_provider ? net_provider->start().first : true;
}

bool Network_provider_manager::stop_all_network_providers() {
  bool retval = false;
  for (auto &&i : m_network_providers) {
    // Logical Sum of all stop() operations. If any of the operations fail,
    // it will report the whole operation as botched, but it will stop all
    // providers
    retval |= i.second->stop().first;
  }

  set_incoming_connections_protocol(get_running_protocol());

  return retval;
}

bool Network_provider_manager::stop_network_provider(
    enum_transport_protocol provider_key) {
  auto net_provider = this->get_provider(provider_key);

  return net_provider ? net_provider->stop().first : true;
}

const std::shared_ptr<Network_provider>
Network_provider_manager::get_active_provider() {
  return this->get_provider(get_running_protocol());
}

const std::shared_ptr<Network_provider>
Network_provider_manager::get_incoming_connections_provider() {
  return this->get_provider(get_incoming_connections_protocol());
}

bool Network_provider_manager::start_active_network_provider() {
  auto net_provider = this->get_active_provider();

  if (!net_provider) return true;

  set_incoming_connections_protocol(get_running_protocol());

  bool config_ok = net_provider->configure(m_active_provider_configuration);

  m_ssl_data_context_cleaner =
      net_provider->get_secure_connections_context_cleaner();

  G_MESSAGE("Using %s as Communication Stack for XCom",
            Communication_stack_to_string::to_string(
                net_provider->get_communication_stack()))

  return config_ok ? net_provider->start().first : true;
}

bool Network_provider_manager::stop_active_network_provider() {
  auto net_provider = this->get_active_provider();

  if (!net_provider) return true;

  set_incoming_connections_protocol(get_running_protocol());

  m_ssl_data_context_cleaner =
      net_provider->get_secure_connections_context_cleaner();

  return net_provider ? net_provider->stop().first : true;
}

bool Network_provider_manager::configure_active_provider(
    Network_configuration_parameters &params) {
  m_active_provider_configuration = params;

  return false;
}

#define DEEP_COPY_NET_PARAMS_FIELD(field)                    \
  m_active_provider_secure_connections_configuration.field = \
      params.field ? strdup(params.field) : nullptr;         \
  G_DEBUG("SSL " #field " %s",                               \
          m_active_provider_secure_connections_configuration.field)

#define CLEANUP_NET_PARAMS_FIELD(field)                           \
  free(const_cast<char *>(                                        \
      m_active_provider_secure_connections_configuration.field)); \
  m_active_provider_secure_connections_configuration.field = nullptr;

bool Network_provider_manager::configure_active_provider_secure_connections(
    Network_configuration_parameters &params) {
  m_active_provider_secure_connections_configuration.ssl_params.ssl_mode =
      params.ssl_params.ssl_mode;

  G_DEBUG("Network Provider Manager SSL Parameters:");
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.server_key_file);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.server_cert_file);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.client_key_file);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.client_cert_file);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.ca_file);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.ca_path);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.crl_file);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.crl_path);
  DEEP_COPY_NET_PARAMS_FIELD(ssl_params.cipher);
  DEEP_COPY_NET_PARAMS_FIELD(tls_params.tls_version);
  DEEP_COPY_NET_PARAMS_FIELD(tls_params.tls_ciphersuites);

  bool config_ssl_ok = true;
  auto net_provider = get_active_provider();
  if (net_provider && is_xcom_using_ssl()) {
    config_ssl_ok = net_provider->configure_secure_connections(
        m_active_provider_secure_connections_configuration);
  }

  return config_ssl_ok;
}

const std::shared_ptr<Network_provider> Network_provider_manager::get_provider(
    enum_transport_protocol provider) {
  auto net_provider = m_network_providers.find(provider);

  if (net_provider == m_network_providers.end()) return nullptr;

  return net_provider->second;
}

connection_descriptor *Network_provider_manager::open_xcom_connection(
    const char *server, xcom_port port, bool use_ssl, int connection_timeout) {
  auto provider = Network_provider_manager::getInstance().get_active_provider();
  connection_descriptor *xcom_connection = nullptr;

  if (provider) {
    Network_security_credentials credentials{"", "", use_ssl};

    auto connection = provider.get()->open_connection(server, port, credentials,
                                                      connection_timeout);

    xcom_connection = new_connection(connection->fd
#ifndef XCOM_WITHOUT_OPENSSL
                                     ,
                                     connection->ssl_fd
#endif
    );

    if (xcom_connection) {
      set_protocol_stack(xcom_connection, provider->get_communication_stack());
    }
  } else {
    xcom_connection = new_connection(-1
#ifndef XCOM_WITHOUT_OPENSSL
                                     ,
                                     nullptr
#endif
    );
  }
  return xcom_connection;
}

int Network_provider_manager::close_xcom_connection(
    connection_descriptor *connection_handle) {
  auto provider = Network_provider_manager::getInstance().get_provider(
      connection_handle->protocol_stack);

  int retval = -1;
  if (provider)
    retval = provider->close_connection({connection_handle->fd
#ifndef XCOM_WITHOUT_OPENSSL
                                         ,
                                         connection_handle->ssl_fd
#endif
    });

  return retval;
}

connection_descriptor *Network_provider_manager::incoming_connection() {
  connection_descriptor *xcom_connection = nullptr;

  auto provider = Network_provider_manager::getInstance()
                      .get_incoming_connections_provider();
  if (provider) {
    auto incoming_new_connection = provider->get_new_connection();

    if (incoming_new_connection != nullptr) {
      xcom_connection = new_connection(incoming_new_connection->fd
#ifndef XCOM_WITHOUT_OPENSSL
                                       ,
                                       incoming_new_connection->ssl_fd
#endif
      );
      set_connected(xcom_connection, CON_FD);
      set_protocol_stack(xcom_connection, provider->get_communication_stack());

      delete incoming_new_connection;
    }
  }
  return xcom_connection;
}

int Network_provider_manager::is_xcom_using_ssl() const {
  return m_ssl_mode != SSL_DISABLED;
}

int Network_provider_manager::xcom_set_ssl_fips_mode(int mode) {
  int retval = INVALID_SSL_FIPS_MODE;

  if (mode >= FIPS_MODE_OFF && mode < LAST_SSL_FIPS_MODE) {
    retval = m_ssl_fips_mode = mode;
  }

  return retval;
}

int Network_provider_manager::xcom_get_ssl_fips_mode(const char *mode) {
  int retval = INVALID_SSL_FIPS_MODE;
  int idx = 0;

  for (; idx < (int)SSL_MODE_FIPS_OPTIONS_COUNT; ++idx) {
    if (strcmp(mode, ssl_fips_mode_options[idx]) == 0) {
      retval = idx;
      break;
    }
  }

  return retval;
}

int Network_provider_manager::xcom_get_ssl_fips_mode() {
  return m_ssl_fips_mode;
}

int Network_provider_manager::xcom_get_ssl_mode(const char *mode) {
  int retval = INVALID_SSL_MODE;
  int idx = 0;

  for (; idx < (int)SSL_MODE_OPTIONS_COUNT; ++idx) {
    if (strcmp(mode, ssl_mode_options[idx]) == 0) {
      retval = idx + 1; /* The enumeration is shifted. */
      break;
    }
  }

  return retval;
}

int Network_provider_manager::xcom_set_ssl_mode(int mode) {
  int retval = INVALID_SSL_MODE;

  mode = (mode == SSL_PREFERRED ? SSL_DISABLED : mode);
  if (mode >= SSL_DISABLED && mode < LAST_SSL_MODE) retval = m_ssl_mode = mode;

  return retval;
}

int Network_provider_manager::xcom_get_ssl_mode() { return m_ssl_mode; }

void Network_provider_manager::delayed_cleanup_secure_connections_context() {
  if (!Network_provider_manager::getInstance().is_xcom_using_ssl()) return;

  if (m_ssl_data_context_cleaner) std::invoke(m_ssl_data_context_cleaner);
}

void Network_provider_manager::cleanup_secure_connections_context() {
  if (!Network_provider_manager::getInstance().is_xcom_using_ssl()) return;

  auto active_provider = get_active_provider();
  if (active_provider) {
    active_provider->cleanup_secure_connections_context();
  }
}

void Network_provider_manager::finalize_secure_connections_context() {
  if (!Network_provider_manager::getInstance().is_xcom_using_ssl()) return;

  auto active_provider = get_active_provider();
  if (active_provider) {
    active_provider->finalize_secure_connections_context();
  }

  CLEANUP_NET_PARAMS_FIELD(ssl_params.server_key_file);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.server_cert_file);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.client_key_file);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.client_cert_file);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.ca_file);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.ca_path);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.crl_file);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.crl_path);
  CLEANUP_NET_PARAMS_FIELD(ssl_params.cipher);
  CLEANUP_NET_PARAMS_FIELD(tls_params.tls_version);
  CLEANUP_NET_PARAMS_FIELD(tls_params.tls_ciphersuites);
}
