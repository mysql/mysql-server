/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/gcs_mysql_network_provider.h"

#include "include/sql_common.h"

#include <mysql.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>
#include <sql-common/net_ns.h>
#include <sql/sql_class.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "sql/rpl_group_replication.h"

// Forward declaration of Group Replication callback...
int handle_group_replication_incoming_connection(THD *thd, int fd,
                                                 SSL *ssl_ctx);

bool Gcs_mysql_network_provider_auth_interface_impl::get_credentials(
    std::string &username, std::string &password) {
  return m_recovery_channel.get_channel_credentials(username, password);
}

MYSQL *Gcs_mysql_network_provider_native_interface_impl::mysql_real_connect(
    MYSQL *mysql, const char *host, const char *user, const char *passwd,
    const char *db, unsigned int port, const char *unix_socket,
    unsigned long clientflag) {
  return ::mysql_real_connect(mysql, host, user, passwd, db, port, unix_socket,
                              clientflag);
}

bool Gcs_mysql_network_provider_native_interface_impl::send_command(
    MYSQL *mysql, enum enum_server_command command, const unsigned char *arg,
    size_t length, bool skip_check) {
  return simple_command(mysql, command, arg, length, skip_check);
}

MYSQL *Gcs_mysql_network_provider_native_interface_impl::mysql_init(
    MYSQL *sock) {
  return ::mysql_init(sock);
}

void Gcs_mysql_network_provider_native_interface_impl::mysql_close(
    MYSQL *sock) {
  if (sock && sock->net.vio) {
    vio_set_blocking_flag(sock->net.vio, false);
  }

  return ::mysql_close(sock);
}

void Gcs_mysql_network_provider_native_interface_impl::mysql_free(void *ptr) {
  ::my_free(ptr);
}

int Gcs_mysql_network_provider_native_interface_impl::mysql_options(
    MYSQL *mysql, enum mysql_option option, const void *arg) {
  return ::mysql_options(mysql, option, arg);
}

bool Gcs_mysql_network_provider_native_interface_impl::mysql_ssl_set(
    MYSQL *mysql, const char *key, const char *cert, const char *ca,
    const char *capath, const char *cipher) {
  return ::mysql_options(mysql, MYSQL_OPT_SSL_KEY, key) +
         ::mysql_options(mysql, MYSQL_OPT_SSL_CERT, cert) +
         ::mysql_options(mysql, MYSQL_OPT_SSL_CA, ca) +
         ::mysql_options(mysql, MYSQL_OPT_SSL_CAPATH, capath) +
         ::mysql_options(mysql, MYSQL_OPT_SSL_CIPHER, cipher);
}

int Gcs_mysql_network_provider_native_interface_impl::
    channel_get_network_namespace(std::string &net_ns
#ifndef HAVE_SETNS
                                  [[maybe_unused]]
#endif
    ) {
#ifdef HAVE_SETNS
  return m_recovery_channel.get_channel_network_namespace(net_ns);
#else
  net_ns.assign("");
  return 0;
#endif
}
/* purecov: begin deadcode */
bool Gcs_mysql_network_provider_native_interface_impl::set_network_namespace(
    const std::string &network_namespace
#ifndef HAVE_SETNS
    [[maybe_unused]]
#endif
) {
#ifdef HAVE_SETNS
  return ::set_network_namespace(network_namespace);
#else
  return 0;
#endif
}
/* purecov: end */

bool Gcs_mysql_network_provider_native_interface_impl::
    restore_original_network_namespace() {
#ifdef HAVE_SETNS
  return ::restore_original_network_namespace();
#else
  return 0;
#endif
}

std::pair<bool, int> Gcs_mysql_network_provider::start() {
  set_gr_incoming_connection(handle_group_replication_incoming_connection);

  return std::make_pair(false, 0);
}

std::pair<bool, int> Gcs_mysql_network_provider::stop() {
  set_gr_incoming_connection(nullptr);

  mysql_mutex_lock(&m_GR_LOCK_connection_map_mutex);

  /*Close all server connections*/
  std::for_each(m_incoming_connection_map.begin(),
                m_incoming_connection_map.end(),
                [](const auto &server_connection) {
                  THD *to_close_thd = server_connection.second;
                  assert(to_close_thd);
                  mysql_mutex_lock(&to_close_thd->LOCK_thd_data);
                  to_close_thd->awake(THD::KILL_CONNECTION);
                  mysql_mutex_unlock(&to_close_thd->LOCK_thd_data);
                });
  m_incoming_connection_map.clear();
  mysql_mutex_unlock(&m_GR_LOCK_connection_map_mutex);

  this->reset_new_connection();

  return std::make_pair(false, 0);
}

bool Gcs_mysql_network_provider::configure(
    const Network_configuration_parameters &params [[maybe_unused]]) {
  return true;
}

bool Gcs_mysql_network_provider::configure_secure_connections(
    const Network_configuration_parameters &params) {
  m_config_parameters.ssl_params.ssl_mode = params.ssl_params.ssl_mode;
  m_config_parameters.ssl_params.server_key_file =
      params.ssl_params.server_key_file;
  m_config_parameters.ssl_params.server_cert_file =
      params.ssl_params.server_cert_file;
  m_config_parameters.ssl_params.client_key_file =
      params.ssl_params.client_key_file;
  m_config_parameters.ssl_params.client_cert_file =
      params.ssl_params.client_cert_file;
  m_config_parameters.ssl_params.ca_file = params.ssl_params.ca_file;
  m_config_parameters.ssl_params.ca_path = params.ssl_params.ca_path;
  m_config_parameters.ssl_params.crl_file = params.ssl_params.crl_file;
  m_config_parameters.ssl_params.crl_path = params.ssl_params.crl_path;
  m_config_parameters.ssl_params.cipher = params.ssl_params.cipher;
  m_config_parameters.tls_params.tls_version = params.tls_params.tls_version;
  m_config_parameters.tls_params.tls_ciphersuites =
      params.tls_params.tls_ciphersuites;

  return false;
}

void Gcs_mysql_network_provider::cleanup_secure_connections_context() {
  auto secure_connections_context_cleaner =
      this->get_secure_connections_context_cleaner();
  std::invoke(secure_connections_context_cleaner);
}

bool Gcs_mysql_network_provider::finalize_secure_connections_context() {
  return false;
}

std::unique_ptr<Network_connection> Gcs_mysql_network_provider::open_connection(
    const std::string &address, const unsigned short port,
    const Network_security_credentials &security_credentials [[maybe_unused]],
    int connection_timeout, network_provider_dynamic_log_level log_level) {
  MYSQL *mysql_connection = nullptr;
  ulong client_flag = CLIENT_REMEMBER_OPTIONS;
  auto retval = std::make_unique<Network_connection>(-1, nullptr);
  retval->has_error = true;

  mysql_connection = m_native_interface->mysql_init(mysql_connection);

  /* Get server public key for RSA key pair-based password exchange.*/
  bool get_key = true;
  m_native_interface->mysql_options(mysql_connection,
                                    MYSQL_OPT_GET_SERVER_PUBLIC_KEY, &get_key);

  auto client_ssl_mode = security_credentials.use_ssl
                             ? static_cast<enum mysql_ssl_mode>(
                                   m_config_parameters.ssl_params.ssl_mode)
                             : SSL_MODE_DISABLED;

  if (client_ssl_mode > SSL_MODE_DISABLED) {
    m_native_interface->mysql_ssl_set(
        mysql_connection,
        m_config_parameters.ssl_params.client_key_file &&
                m_config_parameters.ssl_params.client_key_file[0]
            ? m_config_parameters.ssl_params.client_key_file
            : nullptr,
        m_config_parameters.ssl_params.client_cert_file &&
                m_config_parameters.ssl_params.client_cert_file[0]
            ? m_config_parameters.ssl_params.client_cert_file
            : nullptr,
        m_config_parameters.ssl_params.ca_file &&
                m_config_parameters.ssl_params.ca_file[0]
            ? m_config_parameters.ssl_params.ca_file
            : nullptr,
        m_config_parameters.ssl_params.ca_path &&
                m_config_parameters.ssl_params.ca_path[0]
            ? m_config_parameters.ssl_params.ca_path
            : nullptr,
        m_config_parameters.ssl_params.cipher &&
                m_config_parameters.ssl_params.cipher[0]
            ? m_config_parameters.ssl_params.cipher
            : nullptr);

    m_native_interface->mysql_options(mysql_connection, MYSQL_OPT_SSL_CRL,
                                      m_config_parameters.ssl_params.crl_file);
    m_native_interface->mysql_options(mysql_connection, MYSQL_OPT_SSL_CRLPATH,
                                      m_config_parameters.ssl_params.crl_path);
    m_native_interface->mysql_options(
        mysql_connection, MYSQL_OPT_TLS_VERSION,
        m_config_parameters.tls_params.tls_version &&
                m_config_parameters.tls_params.tls_version[0]
            ? m_config_parameters.tls_params.tls_version
            : nullptr);
    if (m_config_parameters.tls_params.tls_ciphersuites) {
      m_native_interface->mysql_options(
          mysql_connection, MYSQL_OPT_TLS_CIPHERSUITES,
          m_config_parameters.tls_params.tls_ciphersuites);
    }
  }

  m_native_interface->mysql_options(mysql_connection, MYSQL_OPT_SSL_MODE,
                                    &client_ssl_mode);
  m_native_interface->mysql_options(mysql_connection, MYSQL_OPT_LOCAL_INFILE,
                                    nullptr);
  m_native_interface->mysql_options(mysql_connection, MYSQL_PLUGIN_DIR,
                                    nullptr);

  m_native_interface->mysql_options(mysql_connection, MYSQL_DEFAULT_AUTH,
                                    nullptr);

  uint connect_timeout = (connection_timeout / 1000)
                             ? connection_timeout / 1000
                             : 1;  // connection_timeout is a msec parameter

  m_native_interface->mysql_options(mysql_connection, MYSQL_OPT_CONNECT_TIMEOUT,
                                    &connect_timeout);

  std::string recovery_username, recovery_password, network_namespace;

  Replication_thread_api recovery_channel("group_replication_recovery");

  /* purecov: begin deadcode */
  m_native_interface->channel_get_network_namespace(network_namespace);

  if (!network_namespace.empty()) {
    m_native_interface->set_network_namespace(network_namespace);
  }
  /* purecov: end */

  if (m_auth_provider->get_credentials(recovery_username, recovery_password)) {
    LogPluginErr(
        ERROR_LEVEL, ER_GRP_RPL_CONFIGURATION_ACTION_ERROR,
        "Could not extract the access credentials for XCom connections");
    goto err;
  }

  if (!m_native_interface->mysql_real_connect(
          mysql_connection, address.c_str(), recovery_username.c_str(),
          recovery_password.c_str(), nullptr, port, nullptr, client_flag)) {
    // This Log Output might have its log level changed due to
    // @see network_provider_dynamic_log_level input parameter of
    // this method.
    LogPluginErr(Gcs_mysql_network_provider_util::log_level_adaptation(
                     ERROR_LEVEL, log_level),
                 ER_GRP_RPL_MYSQL_NETWORK_PROVIDER_CLIENT_ERROR_CONN_ERR);
    goto err;
  }

  if (m_native_interface->send_command(mysql_connection,
                                       COM_SUBSCRIBE_GROUP_REPLICATION_STREAM,
                                       nullptr, 0, 0)) {
    // This Log Output might have its log level changed due to
    // @see network_provider_dynamic_log_level input parameter of
    // this method.
    LogPluginErr(Gcs_mysql_network_provider_util::log_level_adaptation(
                     ERROR_LEVEL, log_level),
                 ER_GRP_RPL_MYSQL_NETWORK_PROVIDER_CLIENT_ERROR_COMMAND_ERR);
    goto err;
  }

  mysql_mutex_lock(&m_GR_LOCK_connection_map_mutex);
  mysql_connection->free_me = false;
  m_connection_map.emplace(mysql_connection->net.fd, mysql_connection);
  mysql_mutex_unlock(&m_GR_LOCK_connection_map_mutex);

  retval->fd = mysql_connection->net.fd;
  if (client_ssl_mode > SSL_MODE_DISABLED) {
    retval->ssl_fd = static_cast<SSL *>(mysql_connection->net.vio->ssl_arg);
  }
  retval->has_error = false;
err:
  /* purecov: begin deadcode */
  if (!network_namespace.empty()) {
    m_native_interface->restore_original_network_namespace();
  }
  /* purecov: end */
  if (retval->has_error) {
    m_native_interface->mysql_close(mysql_connection);
  }

  return retval;
}

int Gcs_mysql_network_provider::close_connection(
    const Network_connection &connection) {
  int retval = 1;

  mysql_mutex_lock(&m_GR_LOCK_connection_map_mutex);

  if (m_connection_map.find(connection.fd) != m_connection_map.end()) {
    MYSQL *connection_to_close = m_connection_map.at(connection.fd);

    m_native_interface->mysql_close(connection_to_close);
    m_native_interface->mysql_free(connection_to_close);

    m_connection_map.erase(connection.fd);

    retval = 0;

  } else if (m_incoming_connection_map.find(connection.fd) !=
             m_incoming_connection_map.end()) {
    THD *to_close_thd = m_incoming_connection_map.at(connection.fd);
    assert(to_close_thd);
    mysql_mutex_lock(&to_close_thd->LOCK_thd_data);
    to_close_thd->awake(THD::KILL_CONNECTION);
    mysql_mutex_unlock(&to_close_thd->LOCK_thd_data);

    m_incoming_connection_map.erase(connection.fd);

    retval = 0;
  }
  mysql_mutex_unlock(&m_GR_LOCK_connection_map_mutex);

  return retval;
}

void Gcs_mysql_network_provider::set_new_connection(
    THD *thd, Network_connection *connection) {
  mysql_mutex_lock(&m_GR_LOCK_connection_map_mutex);
  m_incoming_connection_map.emplace(thd->active_vio->mysql_socket.fd, thd);
  mysql_mutex_unlock(&m_GR_LOCK_connection_map_mutex);

  Network_provider::set_new_connection(connection);
}
