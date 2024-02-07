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

#ifndef GCS_MYSQL_NETWORK_PROVIDER_INCLUDED
#define GCS_MYSQL_NETWORK_PROVIDER_INCLUDED

#include <map>

#include "include/mysql.h"

#include <mysql.h>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"

#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/replication_threads_api.h"
#include "sql/sql_class.h"

/**
 * @brief IoC interface to allow abstraction of the retrieval of Security
 * Credentials
 *
 */
class Gcs_mysql_network_provider_auth_interface {
 public:
  virtual ~Gcs_mysql_network_provider_auth_interface() {}

  /**
   * @brief Get the user credentials needed to establish MySQL connections.
   *
   * This interface is used either as a proxy for @see Replication_thread_api
   * or to be injected by unit tests.
   *
   * @param username username for the mysql connection
   * @param password password for the mysql connection
   *
   * @return the operation status
   *  @retval false   OK
   *  @retval true    Error, channel not found
   */
  virtual bool get_credentials(std::string &username,
                               std::string &password) = 0;
};

/**
 * @brief Implementation of Gcs_mysql_network_provider_auth_interface
 * that retrieves auth data from MySQL.
 */
class Gcs_mysql_network_provider_auth_interface_impl
    : public Gcs_mysql_network_provider_auth_interface {
 public:
  Gcs_mysql_network_provider_auth_interface_impl()
      : m_recovery_channel("group_replication_recovery") {}
  virtual ~Gcs_mysql_network_provider_auth_interface_impl() override {}

  /**
   * @brief Get the user credentials needed to establish MySQL connections.
   *
   * @see Gcs_mysql_network_provider_auth_interface#get_credentials
   */
  bool get_credentials(std::string &username, std::string &password) override;

 private:
  Replication_thread_api m_recovery_channel;
};

/**
 * @brief IoC interface to allow abstraction of MySQL Client API
 *
 */
class Gcs_mysql_network_provider_native_interface {
 public:
  virtual ~Gcs_mysql_network_provider_native_interface() {}

  /**
   * @brief Proxy method to mysql_real_connect from the MySQL client API
   *
   * @param mysql       mysql client connection reference. Must have been
   *                     initializaed with mysql_init
   * @param host        hostname to connect
   * @param user        username for the connection
   * @param passwd      password for the connection
   * @param db          database/schema to use
   * @param port        remote port to connect
   * @param unix_socket unix socket file (if applicable)
   * @param clientflag  client flags
   * @return MYSQL* a mysql client connection.
   */
  virtual MYSQL *mysql_real_connect(MYSQL *mysql, const char *host,
                                    const char *user, const char *passwd,
                                    const char *db, unsigned int port,
                                    const char *unix_socket,
                                    unsigned long clientflag) = 0;
  /**
   * @brief Proxy method to simple_command from the MySQL client API
   *
   * @param mysql an active MySQL connection
   * @param command the command to send
   * @param arg command arguments
   * @param length length of the arguments
   * @param skip_check skip checking the command
   *
   * @return true in case of error. false, otherwise
   *
   */
  virtual bool send_command(MYSQL *mysql, enum enum_server_command command,
                            const unsigned char *arg, size_t length,
                            bool skip_check) = 0;

  /**
   * @brief Proxy method to mysql_init from the MySQL Client API
   *
   * @param sock the connection to initialize
   */
  virtual MYSQL *mysql_init(MYSQL *sock) = 0;

  /**
   * @brief Proxy method to mysql_close from the MySQL Client API
   *
   * @param sock the connection to close
   */
  virtual void mysql_close(MYSQL *sock) = 0;

  /**
    Method to get the network namespace configured for a channel

    @param[out] net_ns   The network namespace to extract

    @return the operation status
      @retval false   OK
      @retval true    Error, channel not found
  */
  virtual int channel_get_network_namespace(std::string &net_ns) = 0;

  /**
    Set active network namespace specified by a name.

    @param network_namespace  the name of a network namespace to be set active

    @return false on success, true on error
    @note all opened descriptors used during function run are closed on error
  */
  virtual bool set_network_namespace(const std::string &network_namespace) = 0;

  /**
    Restore original network namespace used to be active before a new network
    namespace has been set.

    @return false on success, true on failure
  */
  virtual bool restore_original_network_namespace() = 0;

  /**
   * @brief Proxy method to mysql_free from the MySQL Memory API
   *
   * @param ptr the pointer to free
   */
  virtual void mysql_free(void *ptr) = 0;

  /**
   * @brief Proxy method to mysql_options from the MySQL Memory API
   *
   * @param mysql  connection to set an option
   * @param option option to set
   * @param arg    value of the option to set
   *
   * @return int > 0 in case of error.
   */
  virtual int mysql_options(MYSQL *mysql, enum mysql_option option,
                            const void *arg) = 0;

  /**
   * @brief Proxy method to mysql_ssl_set from the MySQL Memory API
   *
   * @param mysql connection to set SSL options
   * @param key connection key
   * @param cert connection certificate
   * @param ca connection CA
   * @param capath the CA path
   * @param cipher cipher to use
   *
   * @return true in case of error;
   * @return false otherwise.
   */
  virtual bool mysql_ssl_set(MYSQL *mysql, const char *key, const char *cert,
                             const char *ca, const char *capath,
                             const char *cipher) = 0;
};

/**
 * @brief Internal implementation of
 * Gcs_mysql_network_provider_native_interface_impl that serves as a proxy
 * for MySQL Client API functions.
 *
 */
class Gcs_mysql_network_provider_native_interface_impl
    : public Gcs_mysql_network_provider_native_interface,
      public Network_namespace_manager {
 public:
  Gcs_mysql_network_provider_native_interface_impl()
      : m_recovery_channel("group_replication_recovery") {}
  virtual ~Gcs_mysql_network_provider_native_interface_impl() override {}

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#mysql_real_connect
   */
  MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user,
                            const char *passwd, const char *db,
                            unsigned int port, const char *unix_socket,
                            unsigned long clientflag) override;
  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#send_command
   */
  bool send_command(MYSQL *mysql, enum enum_server_command command,
                    const unsigned char *arg, size_t length,
                    bool skip_check) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#mysql_init
   */
  MYSQL *mysql_init(MYSQL *sock) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#mysql_close
   */
  void mysql_close(MYSQL *sock) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#channel_get_network_namespace
   */
  int channel_get_network_namespace(std::string &net_ns) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#set_network_namespace
   */
  bool set_network_namespace(const std::string &network_namespace) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#restore_original_network_namespace
   */
  bool restore_original_network_namespace() override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#my_free
   */
  void mysql_free(void *ptr) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#mysql_options
   */
  int mysql_options(MYSQL *mysql, enum mysql_option option,
                    const void *arg) override;

  /**
   * @brief Implementation of @see
   * Gcs_mysql_network_provider_native_interface#mysql_ssl_set
   */
  bool mysql_ssl_set(MYSQL *mysql, const char *key, const char *cert,
                     const char *ca, const char *capath,
                     const char *cipher) override;

 private:
  Replication_thread_api m_recovery_channel;
};

/**
 * @brief Implementation of a \see Network_provider that will manage MySQL
 * protocol connection for GCS/XCOM.
 */
class Gcs_mysql_network_provider : public Network_provider {
 private:
  /**
   * @brief A map that holds all open MySQL client connections.
   *
   * Since the public interface of Network Managers only knows about File
   * Descriptors, this is the repository for all MySQL client connections. This
   * object is required when using mysql_close.
   *
   * The map's index is the open connection's file descriptor.
   */
  std::map<int, MYSQL *> m_connection_map;

  /**
   * @brief A map that holds THD's for all open MySQL Server connections.
   *
   * We need to maintain this reference in order to call the appropriate closing
   * mechanisms when destroying an incoming connection.
   *
   * The map's index is the open connection's file descriptor.
   */
  std::map<int, THD *> m_incoming_connection_map;

  // Locking for the connection map
  mysql_mutex_t m_GR_LOCK_connection_map_mutex;

  // Configuration parameters for this Provider
  Network_configuration_parameters m_config_parameters;

  /**
   * External IoC dependencies.
   * - A provider for authentication parameters
   * - A provider for all mysql native methods
   */
  Gcs_mysql_network_provider_auth_interface *m_auth_provider;
  Gcs_mysql_network_provider_native_interface *m_native_interface;

 public:
  /**
   * @brief Construct a new Gcs_mysql_network_provider
   *
   * @param auth_provider A provider interface implementation for authentication
   * parameters.
   *
   * @param native_interface  A provider interface for all mysql native methods.
   */
  Gcs_mysql_network_provider(
      Gcs_mysql_network_provider_auth_interface *auth_provider,
      Gcs_mysql_network_provider_native_interface *native_interface)
      : m_connection_map(),
        m_incoming_connection_map(),
        m_GR_LOCK_connection_map_mutex(),
        m_config_parameters(),
        m_auth_provider(nullptr),
        m_native_interface(nullptr) {
    m_config_parameters.ssl_params.ssl_mode = SSL_DISABLED;

    m_auth_provider = auth_provider;
    m_native_interface = native_interface;
    mysql_mutex_init(key_GR_LOCK_connection_map,
                     &m_GR_LOCK_connection_map_mutex, MY_MUTEX_INIT_FAST);
  }

  virtual ~Gcs_mysql_network_provider() override {
    /*Close all client connections*/
    if (!m_connection_map.empty()) {
      std::for_each(m_connection_map.begin(), m_connection_map.end(),
                    [this](const auto &client_connection) {
                      m_native_interface->mysql_close(client_connection.second);
                      m_native_interface->mysql_free(client_connection.second);
                    });
      m_connection_map.clear();
    }
    mysql_mutex_destroy(&m_GR_LOCK_connection_map_mutex);
  }

  /**
   * @brief See @see Network_provider#start
   */
  std::pair<bool, int> start() override;

  /**
   * @brief See @see Network_provider#stop
   */
  std::pair<bool, int> stop() override;

  /**
   * @brief Get the communication stack implemented by this class
   *
   * @return a CommunicationStack enum value. In this case -> MYSQL_PROTOCOL
   */
  enum_transport_protocol get_communication_stack() const override {
    return MYSQL_PROTOCOL;
  }

  /**
   * @brief See @see Network_provider#configure
   */
  bool configure(const Network_configuration_parameters &params) override;

  /**
   * @brief See @see Network_provider#configure_secure_connections
   */
  bool configure_secure_connections(
      const Network_configuration_parameters &params) override;

  void cleanup_secure_connections_context() override;

  bool finalize_secure_connections_context() override;

  /**
   * @brief See @see Network_provider#open_connection
   */
  std::unique_ptr<Network_connection> open_connection(
      const std::string &address, const unsigned short port,
      const Network_security_credentials &security_credentials,
      int connection_timeout = Network_provider::default_connection_timeout(),
      network_provider_dynamic_log_level log_level =
          network_provider_dynamic_log_level::PROVIDED) override;

  int close_connection(const Network_connection &connection) override;

  /**
   * @brief Set the new connection coming form MySQL server
   *
   * @param thd the THD to which the connection belongs to.
   * @param connection the connection data itself.
   */
  void set_new_connection(THD *thd, Network_connection *connection);
};

/**
 * @brief Utilitarian class for Gcs_mysql_network_provider
 *
 */
class Gcs_mysql_network_provider_util {
 public:
  // Out of range log value
  static constexpr int OUT_OF_RANGE_LOG_LEVEL = 255;

 private:
  /**
   * @brief Maps between Network Provider generic log level and MySQL error
   * Log level
   *
   * @param net_provider_log_level Network Provider generic log level
   * @return int MySQL error Log level if there is mapping
   *             OUT_OF_RANGE_LOG_LEVEL, otherwise
   */
  static int from_network_provider_dynamic_log_level_mapping(
      network_provider_dynamic_log_level net_provider_log_level) {
    switch (net_provider_log_level) {
      case network_provider_dynamic_log_level::FATAL:
        return SYSTEM_LEVEL;

      case network_provider_dynamic_log_level::ERROR:
        return ERROR_LEVEL;

      case network_provider_dynamic_log_level::WARNING:
        return WARNING_LEVEL;

      case network_provider_dynamic_log_level::INFO:
        return INFORMATION_LEVEL;

      default:
        // If there is no mapping present, we will return an out of range
        //  value in order to feed LogPluginErr.
        // When provided a non-valid but non-negative number to LogPluginErr
        //  it means that such levels will result in suppression of the
        //  messages being logged
        return Gcs_mysql_network_provider_util::OUT_OF_RANGE_LOG_LEVEL;
    }
  }

 public:
  /**
   * @brief Converts from the intended developer fixed level to a dynamic
   *        level provided from the API call, based on runtime conditions.
   *
   *        A developer might code that wants ERROR level to be written to the
   *        log, but a runtime condition might modify it.
   *
   *        If log_level is PROVIDED, nothing changes and coded_log_level is
   *        used. If log_level is other than PROVIDED, we will do a mapping
   *        between log_level and MySQL log level.
   *
   *        For more information about this mechanism @see
   *        network_provider_dynamic_log_level
   *
   * @param coded_log_level Developer intended log level
   * @param log_level External API call log level
   * @return int the actual runtime log level
   */
  static int log_level_adaptation(
      int coded_log_level, network_provider_dynamic_log_level log_level) {
    return log_level == network_provider_dynamic_log_level::PROVIDED
               ? coded_log_level
               : Gcs_mysql_network_provider_util::
                     from_network_provider_dynamic_log_level_mapping(log_level);
  }
};

#endif /* GCS_MYSQL_NETWORK_PROVIDER_INCLUDED */
