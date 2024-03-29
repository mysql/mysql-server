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

#ifndef NETWORK_PROVIDER_H
#define NETWORK_PROVIDER_H

#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enum that describes the available XCom Communication Stacks
 */
enum enum_transport_protocol {
  INVALID_PROTOCOL = -1,
  XCOM_PROTOCOL = 0,
  MYSQL_PROTOCOL
};

/*
  Possible operation modes as explained further down. If you
  want to add a new mode, do it before the LAST_SSL_MODE.
*/
enum ssl_enum_mode_options {
  INVALID_SSL_MODE = -1,
  SSL_DISABLED = 1,
  SSL_PREFERRED,
  SSL_REQUIRED,
  SSL_VERIFY_CA,
  SSL_VERIFY_IDENTITY,
  LAST_SSL_MODE
};

/*
  Possible operation fips modes as explained further down. If you
  want to add a new ssl fips mode, do it before the LAST_SSL_FIPS_MODE.
*/
enum ssl_enum_fips_mode_options {
  INVALID_SSL_FIPS_MODE = -1,
  FIPS_MODE_OFF = 0,
  FIPS_MODE_ON,
  FIPS_MODE_STRICT,
  LAST_SSL_FIPS_MODE
};

/**
 * @brief This class is a helper to translate a Communication Stack to a
 string
 *
 */
class Communication_stack_to_string {
 public:
  static const char *to_string(enum_transport_protocol protocol) {
    static std::vector<const char *> m_running_protocol_to_string = {"XCom",
                                                                     "MySQL"};

    return protocol > INVALID_PROTOCOL && protocol <= MYSQL_PROTOCOL
               ? m_running_protocol_to_string[protocol]
               : "Invalid Protocol";
  }
};

/**
 * @brief Security credentials to establish a connection
 */
struct Network_security_credentials {
  std::string user;
  std::string pass;
  bool use_ssl;
};

/*
  Set the necessary SSL parameters before initialization.

  server_key_file  - Path of file that contains the server's X509 key in PEM
                     format.
  server_cert_file - Path of file that contains the server's X509 certificate
                     in PEM format.
  client_key_file  - Path of file that contains the client's X509 key in PEM
                     format.
  client_cert_file - Path of file that contains the client's X509 certificate
                     in PEM format.
  ca_file          - Path of file that contains list of trusted SSL CAs.
  ca_path          - Path of directory that contains trusted SSL CA
                     certificates in PEM format.
  crl_file         - Path of file that contains certificate revocation lists.
  crl_path         - Path of directory that contains certificate revocation
                     list files.
  cipher           - List of permitted ciphers to use for connection
                     encryption.
  tls_version      - Protocols permitted for secure connections.
  tls_ciphersuites - List of permitted ciphersuites to use for TLS 1.3
                     connection encryption.

  Note that only the server_key_file/server_cert_file and the client_key_file/
  client_cert_file are required and the rest of the pointers can be NULL.
  If the key is provided along with the certificate, either the key file or
  the other can be omitted.

  The caller can free the parameters after the SSL is started
  if this is necessary.
*/
struct ssl_parameters {
  int ssl_mode;
  const char *server_key_file;
  const char *server_cert_file;
  const char *client_key_file;
  const char *client_cert_file;
  const char *ca_file;
  const char *ca_path;
  const char *crl_file;
  const char *crl_path;
  const char *cipher;
};
struct tls_parameters {
  const char *tls_version;
  const char *tls_ciphersuites;
};

/**
 * @brief Possible configuration parameters
 */
struct Network_configuration_parameters {
  unsigned short port;

  struct ssl_parameters ssl_params;
  struct tls_parameters tls_params;
};

/**
 * @brief Represents an open connection.
 */
struct Network_connection {
  Network_connection(int parameter_fd)
      : fd(parameter_fd)
#ifndef XCOM_WITHOUT_OPENSSL
        ,
        ssl_fd(nullptr)
#endif
        ,
        has_error(false) {
  }

  Network_connection(int parameter_fd
#ifndef XCOM_WITHOUT_OPENSSL
                     ,
                     SSL *parameter_ssl_fd
#endif
                     )
      : fd(parameter_fd)
#ifndef XCOM_WITHOUT_OPENSSL
        ,
        ssl_fd(parameter_ssl_fd)
#endif
        ,
        has_error(false) {
  }

  Network_connection(int parameter_fd
#ifndef XCOM_WITHOUT_OPENSSL
                     ,
                     SSL *parameter_ssl_fd
#endif
                     ,
                     bool parameter_has_error)
      : fd(parameter_fd)
#ifndef XCOM_WITHOUT_OPENSSL
        ,
        ssl_fd(parameter_ssl_fd)
#endif
        ,
        has_error(parameter_has_error) {
  }

  int fd;
#ifndef XCOM_WITHOUT_OPENSSL
  SSL *ssl_fd;
#endif
  bool has_error;
};

/**
 * @brief Class that provides Network Namespace services
 */
class Network_namespace_manager {
 public:
  virtual ~Network_namespace_manager() {}

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
};

/**
 * @brief Base class for External Network Providers
 *
 * This virtual class will serve as base class for any external entity that
 * whishes to provide network connections to XCom.
 *
 * It will have to implement the following methods:
 * - start();
 * - stop();
 * - get_ĩd();
 * - configure();
 * - open_connection();
 * - close_connection();
 *
 * If provides a lock free implementation of (set)\(get)_connection() for
 * multithreaded usage.
 *
 *
 */
class Network_provider {
 public:
  Network_provider() : m_shared_connection() {
    m_shared_connection.store(nullptr);
  }
  Network_provider(Network_provider &&param)
      : m_shared_connection(param.m_shared_connection.load()) {}

  Network_provider &operator=(Network_provider &param) = delete;
  Network_provider(Network_provider &param) = delete;

  virtual ~Network_provider() {}

  /**
   * @brief Starts the network provider.
   *
   * Each implementation will place here any code that it needs to start a
   * network provider.
   *
   * start() is synchronous. After start() succeeded, it is assumed that XCom
   * is ready to receive new connections.
   *
   * @return a pair of <bool,int>
   *         bool indicates the success of the operation. false means success.
   *         int returns an error code.
   */
  virtual std::pair<bool, int> start() = 0;

  /**
   * @brief Stops the network provider.
   *
   * Each implementation will place here any code that it needs to stop a
   * network provider.
   *
   * stop() is synchronous. After stop() succeeded, it is assumed that XCom
   * shall not receive any new connection.
   *
   * @return a pair of <bool,int>
   *         bool indicates the success of the operation. false means success.
   *         int returns an error code.
   */
  virtual std::pair<bool, int> stop() = 0;

  /**
   * @brief Get the communication stack implemented by this provider
   *
   * Return a valid value withint the range of RunningProtocol enum.
   *
   * @return RunningProtocol valid value
   */
  virtual enum_transport_protocol get_communication_stack() const = 0;

  /**
   * @brief Configures a network provider
   *
   * @param params a sensible list of possibly configurable network parameters
   *
   * @return true in case of a successful configuration.
   * @return false in case of a unsuccessful configuration.
   */
  virtual bool configure(const Network_configuration_parameters &params) = 0;

  /**
   * @brief Configures the active provider with all things needed to establish
   * SSL connections
   *
   * @param params configuration parameters for SSL.
   *
   * @return true  In case of success.
   * @return false In case of failure.
   */
  virtual bool configure_secure_connections(
      const Network_configuration_parameters &params) = 0;

  virtual void cleanup_secure_connections_context() = 0;

  virtual std::function<void()> get_secure_connections_context_cleaner() {
    std::function<void()> retval = []() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
      ERR_remove_thread_state(0);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    };

    return retval;
  }

  virtual bool finalize_secure_connections_context() = 0;

  /**
   * @brief Opens a new connection to another XCom endpoint served by the same
   *        Network provider.
   *
   * @param address address of the remote endpoint
   * @param port port of the remote endpoint
   * @param security_credentials security credentials to connect to the remote
   *                             endpoint
   * @param connection_timeout
   * @return std::unique_ptr<Network_connection> an established connection.
   *                                                 nullptr in case of failure.
   */
  virtual std::unique_ptr<Network_connection> open_connection(
      const std::string &address, const unsigned short port,
      const Network_security_credentials &security_credentials,
      int connection_timeout = default_connection_timeout()) = 0;

  /**
   * @brief Closes an open connection to another XCom endpoint served by the
   *        same Network provider.
   *
   * @param connection an open and valid connection
   * @return int an error code in case of error. 0, otherwise.
   */
  virtual int close_connection(const Network_connection &connection) = 0;

  /**
   * @brief Lock-free Set connection
   *
   * Sets a new connection received by this provider. It will be consumed
   * internally by get_new_connection().
   *
   * @param connection a newly created connection.
   */
  void set_new_connection(Network_connection *connection) {
    Network_connection *null_desired_value;
    do {
      null_desired_value = nullptr;
    } while (!m_shared_connection.compare_exchange_weak(null_desired_value,
                                                        connection));
  }

  /**
   * @brief Get the new connection object
   *
   * @return Network_connection* a new connection coming from this network
   *                                 provider
   */
  Network_connection *get_new_connection() {
    Network_connection *new_connection = nullptr;

    new_connection = m_shared_connection.load();

    if (new_connection != nullptr) m_shared_connection.store(nullptr);

    return new_connection;
  }

  void reset_new_connection() {
    Network_connection *to_purge = get_new_connection();

    if (to_purge) {
      close_connection(*to_purge);
    }

    delete to_purge;
  }

  static constexpr int default_connection_timeout() { return 3000; }

 private:
  std::atomic<Network_connection *> m_shared_connection;
};

#endif  // NETWORK_PROVIDER_H
