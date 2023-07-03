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

#ifndef NETWORK_MANAGEMENT_INTERFACE_H
#define NETWORK_MANAGEMENT_INTERFACE_H

#include "xcom/network/include/network_provider.h"

#include <functional>

/**
 * @brief Inversion of Control interface to manage Network providers
 */
class Network_provider_management_interface {
 public:
  explicit Network_provider_management_interface() {}
  virtual ~Network_provider_management_interface() {}

  Network_provider_management_interface(
      Network_provider_management_interface const &) =
      delete;  // Copy construct
  Network_provider_management_interface &operator=(
      Network_provider_management_interface const &) = delete;  // Copy assign

  Network_provider_management_interface(
      Network_provider_management_interface &&) = default;  // Move construct
  Network_provider_management_interface &operator=(
      Network_provider_management_interface &&) = default;  // Move assign

  /**
   * @brief Initialize the network manager. It also creates the default XCom
   *        provider and adds it to the manager.
   *
   * @return true in case of error. false otherwise.
   */
  virtual bool initialize() = 0;

  /**
   * @brief Finalize the network manager. It removes the default XCom
   *        provider,
   *
   * @return true in case of error. false otherwise.
   */
  virtual bool finalize() = 0;

  /**
   * @brief Sets the running Communication Stack, thus defining the active
   * provider.
   *
   * In runtime, this is will change the way we establish connections.
   *
   * @param new_value value of the Communication Stack
   */
  virtual void set_running_protocol(enum_transport_protocol new_value) = 0;

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
  virtual enum_transport_protocol get_running_protocol() const = 0;

  /**
   * @brief Get the incoming connections Communication Stack
   *
   * This is the value that is used to report upwards the protocol in * which
   * we are currently accepting connections.
   *
   * @return CommunicationStack
   */
  virtual enum_transport_protocol get_incoming_connections_protocol() const = 0;

  /**
   * @brief Add a new Gcs_network_provider instance
   *
   * @param provider an already instantiated shared_ptr object of a
   *                 Gcs_network_provider
   */
  virtual void add_network_provider(
      std::shared_ptr<Network_provider> provider) = 0;

  virtual void remove_all_network_provider() = 0;

  virtual void remove_network_provider(
      enum_transport_protocol provider_key) = 0;

  // SSL RELATED OPERATIONS
  /**
   Return whether the SSL will be used to encrypt data or not.

   Return 1 if it is enabled 0 otherwise.
*/
  virtual int is_xcom_using_ssl() const = 0;

  /**
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
  virtual int xcom_set_ssl_mode(int mode) = 0;

  /**
    Return the operation mode as an integer from an operation mode provided
    as a string. Note that the string must be provided in upper case letters
    and the possible values are: "DISABLED", "PREFERRED", "REQUIRED",
    "VERIFY_CA" or "VERIFY_IDENTITY".

    If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */
  virtual int xcom_get_ssl_mode(const char *mode) = 0;

  /**
  Return the configured value into SSL mode
  */
  virtual int xcom_get_ssl_mode() = 0;

  /**
  Set the operation fips mode which might be the following:

  . SSL_FIPS_MODE_OFF (0): This will set openssl fips mode value to 0

  . SSL_FIPS_MODE_ON (1): This will set openssl fips mode value to 1

  . SSL_FIPS_MODE_STRICT (2): This will set openssl fips mode value to 2

  If a different value is provide, INVALID_SSL_FIPS_MODE (-1) is returned.
  */
  virtual int xcom_set_ssl_fips_mode(int mode) = 0;

  /**
   Return the operation fips mode as an integer from an operation fips mode
   provided as a string. Note that the string must be provided in upper case
   letters and the possible values are: "OFF", "ON", "STRICT",

   If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */
  virtual int xcom_get_ssl_fips_mode(const char *mode) = 0;

  /**
  Returns the configured FIPS mode
  */
  virtual int xcom_get_ssl_fips_mode() = 0;

  /**
   * @brief Cleans up SSL context.
   */
  virtual void cleanup_secure_connections_context() = 0;

  /**
   * @brief Destroys all things SSL related
   */
  virtual void finalize_secure_connections_context() = 0;
};

/**
 * @brief Inversion of Control proxy interface to operate Network providers
 *
 * For full documentation @see Network_provider_manager
 */
class Network_provider_operations_interface {
 public:
  Network_provider_operations_interface() {}
  virtual ~Network_provider_operations_interface() {}

  Network_provider_operations_interface(
      Network_provider_operations_interface const &) =
      delete;  // Copy construct
  Network_provider_operations_interface &operator=(
      Network_provider_operations_interface const &) = delete;  // Copy assign
  /**
   * @brief Start the active provider.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  virtual bool start_active_network_provider() = 0;

  /**
   * @brief Stops all network providers.
   *
   * @return true In case of success stopping ALL network providers
   * @return false In case of failure in stopping AT LEAST ONE network provider
   */
  virtual bool stop_all_network_providers() = 0;

  /**
   * @brief Stops the active provider.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  virtual bool stop_active_network_provider() = 0;

  /**
   * @brief Configures the active provider
   *
   * @param params configuration parameters.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  virtual bool configure_active_provider(
      Network_configuration_parameters &params) = 0;

  /**
   * @brief COnfigures the active provider SSL parameters
   *
   * @param params the security parameters.
   *
   * @return true in case of error. false otherwise.
   */
  virtual bool configure_active_provider_secure_connections(
      Network_configuration_parameters &params) = 0;
};

#endif  // NETWORK_MANAGEMENT_INTERFACE_H
