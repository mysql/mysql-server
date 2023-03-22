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

#ifndef NETWORK_PROVIDER_MANAGER_H
#define NETWORK_PROVIDER_MANAGER_H

#include <functional>
#include <string>
#include <unordered_map>

#include "xcom/network/include/network_management_interface.h"
#include "xcom/network/include/network_provider.h"
#include "xcom/network/xcom_network_provider.h"

#include "xcom/node_connection.h"
#include "xcom/result.h"
#include "xcom/xcom_common.h"

/**
 * @brief Manages all running instances of a network provider.
 */
class Network_provider_manager : public Network_provider_management_interface,
                                 public Network_provider_operations_interface {
 public:
  static Network_provider_manager &getInstance() {
    static Network_provider_manager instance;
    return instance;
  }

  Network_provider_manager(Network_provider_manager const &) =
      delete;  // Copy construct
  Network_provider_manager(Network_provider_manager &&) =
      delete;  // Move construct
  Network_provider_manager &operator=(Network_provider_manager const &) =
      delete;  // Copy assign
  Network_provider_manager &operator=(Network_provider_manager &&) =
      delete;  // Move assign

  /**
   * @brief Initialize the network manager. It also creates the default XCom
   *        provider and adds it to the manager.
   *
   * @return true in case of error. false otherwise.
   */
  bool initialize() override;

  /**
   * @brief Finalize the network manager. It removes the default XCom
   *        provider,
   *
   * @return true in case of error. false otherwise.
   */
  bool finalize() override;

  /**
   * @brief Add a new Gcs_network_provider instance
   *
   * @param provider an already instantiated shared_ptr object of a
   *                 Gcs_network_provider
   */
  void add_network_provider(
      std::shared_ptr<Network_provider> provider) override;

  /**
   * @brief Add a new Gcs_network_provider instance and start it.
   *
   * @param provider an already instantiated shared_ptr object of a
   *                 Gcs_network_provider
   */
  void add_and_start_network_provider(
      std::shared_ptr<Network_provider> provider);

  /**
   * @brief Remove an active network provider
   *
   * @param provider_key a valid value of CommunicationStack of the provider
   *                     that you want to remove.
   */
  void remove_network_provider(enum_transport_protocol provider_key) override;

  /**
   * @brief Removes all configured network providers
   *
   */
  void remove_all_network_provider() override;

  /**
   * @brief Starts an already added network provider
   *
   * @param provider_key a valid value of CommunicationStack of the provider
   *                     that you want to start.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  bool start_network_provider(enum_transport_protocol provider_key);

  /**
   * @brief Stops all network providers.
   *
   * @return true In case of success stopping ALL network providers
   * @return false In case of failure in stopping AT LEAST ONE network provider
   */
  bool stop_all_network_providers() override;

  /**
   * @brief Stops a running network provider
   *
   * @param provider_key a valid value of CommunicationStack of the provider
   *                     that you want to stop.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  bool stop_network_provider(enum_transport_protocol provider_key);

  /**
   * @brief Sets the running Communication Stack, thus defining the active
   * provider.
   *
   * In runtime, this is will change the way we establish connections.
   *
   * @param new_value value of the Communication Stack
   */
  void set_running_protocol(enum_transport_protocol new_value) override {
    m_running_protocol = new_value;
  }

  /**
   * @brief Gets the configured running protocol
   *
   * It returns the value that is currently configured in the Running
   * Communication Stack
   *
   * Since this value is dynamic, it can cause a mismatch from the provider
   * that we are actively receiving connections and the provider that we use
   * to establish new connections
   *
   * @return CommunicationStack value.
   */
  enum_transport_protocol get_running_protocol() const override {
    return m_running_protocol;
  }

  /**
   * @brief Get the incoming connections Communication Stack
   *
   * This is the value that is used to report upwards the protocol in * which
   * we are currently accepting connections.
   *
   * @return CommunicationStack
   */
  enum_transport_protocol get_incoming_connections_protocol() const override {
    return m_incoming_connections_protocol;
  }

  /**
   * @brief Gets a configured provider
   *
   * @param provider const std::shared_ptr<Gcs_network_provider> a shared_ptr to
   * the active provider.
   * @return const std::shared_ptr<Network_provider>
   */
  const std::shared_ptr<Network_provider> get_provider(
      enum_transport_protocol provider);

  /**
   * @brief Retrieves the active provider. This is determined by the value set
   * in set_running_protocol.
   *
   * @return const std::shared_ptr<Gcs_network_provider> a shared_ptr to the
   * active provider.
   */
  const std::shared_ptr<Network_provider> get_active_provider();

  /**
   * @brief Retrieves the active provider for incoming connections.
   * This is determined by the value set in set_running_protocol when the active
   * provider is started.
   *
   * @return const std::shared_ptr<Gcs_network_provider> a shared_ptr to the
   * active provider for incoming connections.
   */
  const std::shared_ptr<Network_provider> get_incoming_connections_provider();

  /**
   * @brief Start the active provider.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  bool start_active_network_provider() override;

  /**
   * @brief Stops the active provider.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  bool stop_active_network_provider() override;

  /**
   * @brief Configures the active provider
   *
   * @param params configuration parameters.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  bool configure_active_provider(
      Network_configuration_parameters &params) override;

  /**
   * @brief Configures the active provider with all things needed to establish
   * SSL connections
   *
   * @param params configuration parameters for SSL.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  bool configure_active_provider_secure_connections(
      Network_configuration_parameters &params) override;

  // Interface methods...
  /**
   * @brief Method to call to get Server Connections that are waiting to be
   *        accepted.
   *
   * Loop over this method to accept connections. They will be accepted from
   * the provider that is configured in the Incoming Connection protocol.
   *
   * @return connection_descriptor* a pointer to a connection_descriptor. If the
   *                                pointer is nullptr, no new connections are
   *                                available.
   */
  connection_descriptor *incoming_connection();

  /**
   * @brief Closes an open connection to another XCom endpoint served by the
   *        a Network provider.
   *
   * @param connection_handle an open and valid connection
   * @return int an error code in case of error. 0, otherwise.
   */
  int close_xcom_connection(connection_descriptor *connection_handle);

  /**
   * @brief Opens a new connection to another XCom endpoint served by the same
   *        Network provider.
   *
   * @param server  address of the remote endpoint
   * @param port    port of the remote endpoint
   * @param use_ssl if this connection should use SSL
   * @param connection_timeout optional connection timeout.
   *
   * @return connection_descriptor an established connection.
   *                                                 nullptr in case of failure.
   */
  connection_descriptor *open_xcom_connection(
      const char *server, xcom_port port, bool use_ssl,
      int connection_timeout = Network_provider::default_connection_timeout());

  // SSL RELATED OPERATIONS

  /*
    Return whether the SSL will be used to encrypt data or not.

    Return 1 if it is enabled 0 otherwise.
  */
  int is_xcom_using_ssl() const override;

  /*
  Set the operation mode which might be the following:

  . SSL_DISABLED (1): The SSL mode will be disabled and this is the default
    value.

  . SSL_PREFERRED (2): The SSL mode will be always disabled if this value is
    provided and is only allowed to keep the solution compatibility with
    MySQL server.

  . SSL_REQUIRED (4): The SSL mode will be enabled but the verifications
    described in the next modes are not performed.

  . SSL_VERIFY_CA (4) - Verify the server TLS certificate against the
  configured Certificate Authority (CA) certificates. The connection attempt
  fails if no valid matching CA certificates are found.

  . SSL_VERIFY_IDENTITY (5): Like VERIFY_CA, but additionally verify that the
    server certificate matches the host to which the connection is attempted.

  If a different value is provide, INVALID_SSL_MODE (-1) is returned.
*/
  int xcom_set_ssl_mode(int mode) override;

  /*
    Return the operation mode as an integer from an operation mode provided
    as a string. Note that the string must be provided in upper case letters
    and the possible values are: "DISABLED", "PREFERRED", "REQUIRED",
    "VERIFY_CA" or "VERIFY_IDENTITY".

    If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */
  int xcom_get_ssl_mode(const char *mode) override;

  /*
  Return the configured value into SSL mode
  */
  int xcom_get_ssl_mode() override;

  /*
  Set the operation fips mode which might be the following:

  . SSL_FIPS_MODE_OFF (0): This will set openssl fips mode value to 0

  . SSL_FIPS_MODE_ON (1): This will set openssl fips mode value to 1

  . SSL_FIPS_MODE_STRICT (2): This will set openssl fips mode value to 2

  If a different value is provide, INVALID_SSL_FIPS_MODE (-1) is returned.
  */
  int xcom_set_ssl_fips_mode(int mode) override;

  /*
   Return the operation fips mode as an integer from an operation fips mode
   provided as a string. Note that the string must be provided in upper case
   letters and the possible values are: "OFF", "ON", "STRICT",

   If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */
  int xcom_get_ssl_fips_mode(const char *mode) override;

  /*
  Returns the configured FIPS mode
  */
  int xcom_get_ssl_fips_mode() override;

  /**
   * @brief Cleans up SSL context.
   */
  void cleanup_secure_connections_context() override;
  void finalize_secure_connections_context() override;

 private:
  Network_provider_manager()
      : m_running_protocol(XCOM_PROTOCOL),
        m_incoming_connections_protocol(XCOM_PROTOCOL),
        m_ssl_mode(SSL_DISABLED),
        m_ssl_fips_mode(FIPS_MODE_OFF) {}
  virtual ~Network_provider_manager() override { m_network_providers.clear(); }

  void set_incoming_connections_protocol(enum_transport_protocol value) {
    m_incoming_connections_protocol = value;
  }

  std::unordered_map<enum_transport_protocol, std::shared_ptr<Network_provider>,
                     std::hash<int>>
      m_network_providers;

  enum_transport_protocol m_running_protocol;
  enum_transport_protocol m_incoming_connections_protocol;

  int m_ssl_mode;
  int m_ssl_fips_mode;

  Network_configuration_parameters m_active_provider_configuration;
  Network_configuration_parameters
      m_active_provider_secure_connections_configuration;

  // Default provider. It is encapsulated in the Network Manager.
  std::shared_ptr<Xcom_network_provider> m_xcom_network_provider;
};

/**
 * @brief Proxy class to access funcionality in Network_provider_manager
 *
 * This way, we avoid spreading singleton calls in all the code, thus
 * encapsulting all calls.
 *
 */
class Network_Management_Interface
    : public Network_provider_management_interface,
      public Network_provider_operations_interface {
 public:
  Network_Management_Interface() {
    m_get_manager = Network_provider_manager::getInstance;
  }

  virtual ~Network_Management_Interface() override = default;

  Network_Management_Interface(Network_Management_Interface const &) =
      delete;  // Copy construct
  Network_Management_Interface &operator=(
      Network_Management_Interface const &) = delete;  // Copy assign

  bool initialize() override { return m_get_manager().initialize(); }

  bool finalize() override { return m_get_manager().finalize(); }

  void set_running_protocol(enum_transport_protocol new_value) override {
    m_get_manager().set_running_protocol(new_value);
  }

  enum_transport_protocol get_running_protocol() const override {
    return m_get_manager().get_running_protocol();
  }

  enum_transport_protocol get_incoming_connections_protocol() const override {
    return m_get_manager().get_incoming_connections_protocol();
  }
  void add_network_provider(
      std::shared_ptr<Network_provider> provider) override {
    m_get_manager().add_network_provider(provider);
  }

  bool start_active_network_provider() override {
    return m_get_manager().start_active_network_provider();
  }

  bool stop_all_network_providers() override {
    return m_get_manager().stop_all_network_providers();
  }

  bool stop_active_network_provider() override {
    return m_get_manager().stop_active_network_provider();
  }

  void remove_network_provider(enum_transport_protocol provider_key) override {
    return m_get_manager().remove_network_provider(provider_key);
  }

  void remove_all_network_provider() override {
    return m_get_manager().remove_all_network_provider();
  }

  bool configure_active_provider(
      Network_configuration_parameters &params) override {
    return m_get_manager().configure_active_provider(params);
  }

  bool configure_active_provider_secure_connections(
      Network_configuration_parameters &params) override {
    return m_get_manager().configure_active_provider_secure_connections(params);
  }

  int is_xcom_using_ssl() const override {
    return m_get_manager().is_xcom_using_ssl();
  }

  int xcom_set_ssl_mode(int mode) override {
    return m_get_manager().xcom_set_ssl_mode(mode);
  }
  int xcom_get_ssl_mode(const char *mode) override {
    return m_get_manager().xcom_get_ssl_mode(mode);
  }
  int xcom_get_ssl_mode() override {
    return m_get_manager().xcom_get_ssl_mode();
  }
  int xcom_set_ssl_fips_mode(int mode) override {
    return m_get_manager().xcom_set_ssl_fips_mode(mode);
  }
  int xcom_get_ssl_fips_mode(const char *mode) override {
    return m_get_manager().xcom_get_ssl_fips_mode(mode);
  }
  int xcom_get_ssl_fips_mode() override {
    return m_get_manager().xcom_get_ssl_fips_mode();
  }
  void cleanup_secure_connections_context() override {
    return m_get_manager().cleanup_secure_connections_context();
  }

  void finalize_secure_connections_context() override {
    return m_get_manager().finalize_secure_connections_context();
  }

 private:
  std::function<Network_provider_manager &()> m_get_manager;
};

#endif  // GCS_XCOM_NETWORK_PROVIDER_MANAGER_H
