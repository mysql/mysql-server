/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef X_CLIENT_MYSQLXCLIENT_XSESSION_H_
#define X_CLIENT_MYSQLXCLIENT_XSESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "mysqlxclient/xargument.h"
#include "mysqlxclient/xerror.h"
#include "mysqlxclient/xprotocol.h"

namespace xcl {

/**
  Interface that manages session.

  This interface is responsible for creation, configuration, release of
  connection and session management.
  It is also owner of other objects that are required to maintain a session.
  "SQL" or admin command can be execute through this interface.
  X Protocol specific features (or flows) may require manual sending of
  X Protocol messages (user needs to acquire XProtocol interface).
*/
class XSession {
 public:
  /**
    Capabilities supported by client library.

    Capabilities are settings that are transfered between client and server
    to change the behavior of both ends of X Protocol.
    Capabilities listed here are handled by implementation of
    XSession class. Setting capability may require to do some additional
    reconfiguration of communication channel. For example setting TLS
    capability reconfigures communication channel to use encryption.
    When setting Capabilities manually (using XProtocol interface) user
    needs to remember about the possible reconfiguration.
    Capabilities must be set before connecting through XSession interface.
  */
  enum Mysqlx_capability {
    /**
      User can handle expired passwords.

      Tell the server that the authentication attempt shouldn't be rejected
      when client uses expired MySQL Servers account. This gives the user
      a possibility to change the password like in "interactive" client. This
      capability shoudn't be used on scripts, plugins, internal connections.

      Capability type: BOOL. Value: enable/disable the support.
    */
    Capability_can_handle_expired_password,

    /**
      Handle input line by line and process it using the interactive pipeline.

      Capability type: BOOL. Value: enable/disable the support.
     */
    Capability_client_interactive,
  };

  /**
    Configuration options.

    Each value defines separate configurable behavior which can't be changed
    after connection establishment.
  */
  enum class Mysqlx_option {
    /** Defines behavior of `hostname` resolver:

     * "ANY": IPv4 and IPv6 addresses are accepted when `hostname`
       is resolved.
     * "IP4": IPv4 addresses are accepted when `hostname`
       is resolved.
     * "IP6": IPv6 addresses are accepted when `hostname`
       is resolved.

     Default: "ANY"
     Option type: STRING.
    */
    Hostname_resolve_to,
    /**
      Define timeout behavior for connection establishment.

      Default value of this parameter is set to "-1", which means infinite
      block. Values greater than "-1" define number of milliseconds which
      'connect' operation should wait for connection establishment.

      Default: -1.
      Option type: INTEGER.
    */
    Connect_timeout,
    /**
      Define timeout behavior when reading from the connection.

      Default value of this parameter is set to "-1", which means infinite
      block. Values greater than "-1" define number of milliseconds which
      'read' operation should wait for data.

      Default: -1.
      Option type: INTEGER.
    */
    Read_timeout,
    /**
      Define timeout behavior when writing from the connection.

      Default value of this parameter is set to "-1", which means infinite
      block. Values greater than "-1" define number of milliseconds which
      'write' operation should wait for data.

      Default: -1.
      Option type: INTEGER.
    */
    Write_timeout,
    /**
      TLS protocols permitted by the client for encrypted connections.

      The value is a comma-separated list containing one or more protocol
      names. (TLSv1,TLSv1.1,TLSv1.2)

      Default: ""
      Option type: STRING.
    */
    Allowed_tls,
    /**
      Configure the requirements regarding SSL connection.

      It can take as arguments following string values:

      * "PREFERRED": Establish a secure (encrypted) connection if the server
        supports secure connections. Fall back to an unencrypted connection
        otherwise. This is the default value.
      * "DISABLED": Establish an unencrypted connection. This is like the
        "mysql" clients legacy --ssl=0 option or its synonyms (--skip-ssl,
        --disable-ssl).
      * "REQUIRED": Establish a secure connection if the server supports
        secure connections. The connection attempt fails if a secure
        connection cannot be established.
      * "VERIFY_CA": Like REQUIRED, but additionally verify the server TLS
        certificate against the configured Certificate Authority
        (CA) certificates. The connection attempt fails if no valid matching
        CA certificates are found.
      * "VERIFY_IDENTITY": Like VERIFY_CA, but additionally verify that the
        server certificate matches the host to which the connection is
        attempted.

      Default: "PREFERRED".
      Option type: STRING.
    */
    Ssl_mode,
    /**
      Configure the requirements regarding SSL FIPS mode connection.

      It can take as arguments following string values:

      * "OFF": Set the openssl FIPS mode 0 (OFF)
      * "ON": Set the openssl FIPS mode 1 (ON)
      * "STRICT": Set the openssl FIPS mode 2 (STRICT)

      Default: "OFF".
      Option type: STRING.
    */
    Ssl_fips_mode,
    /** Path to the SSL key file in PEM format. Option type: STRING. */
    Ssl_key,
    /** Path to a file in PEM format that contains a list of trusted
    SSL certificate authorities. Option type: STRING. */
    Ssl_ca,
    /** Path to a directory that contains trusted SSL certificate authority
    certificates in PEM format. Option type: STRING. */
    Ssl_ca_path,
    /** Path to the SSL certificate file in PEM format. Option type: STRING. */
    Ssl_cert,
    /** A list of permissible ciphers to use for connection encryption.
      Option type: STRING. */
    Ssl_cipher,
    /** Path to a file containing certificate revocation lists in PEM
      format. Option type: STRING. */
    Ssl_crl,
    /** Path to a directory that contains files containing certificate
      revocation lists in PEM format. Option type: STRING. */
    Ssl_crl_path,
    /** Overwrite X Protocol authentication method:

    * "AUTO"         - let the library select authentication method (can not
                       be used with ARRAY OF STRINGS type)
    * "FALLBACK      - same as "AUTO" still do not use authentication methods
                       that are not compatible with MYSQL 5.7 (can not be used
                       with ARRAY OF STRINGS type)
    * "FROM_CAPABILITIES" - let the library select authentication method using
                            capabilities announced by server (can not be used
                            with ARRAY OF STRINGS type)
    * "SHA256_MEMORY - authentication based on memory-stored credentials
    * "MYSQL41"      - do not use plain password send through network
    * "PLAIN"        - use plain password for authentication

    Default: "AUTO".
    Option type: STRING, ARRAY OF STRINGS.
    */
    Authentication_method,
    /** Tells XSession what should happen when XProtocol notice handler
      didn't consume received notice.

      * true - consume it.
      * false - allow to return the notice by XProtocol::recv_single_message

      Default: true
      Option type: BOOL
     */
    Consume_all_notices,
    /** Determine what should be the lenght of a DATETIME field so that it
        would be possible to distinguish if it contain only date or both
        date and time parts.

      Default: 10
      Option type: INT
    */
    Datetime_length_discriminator
  };

 public:
  virtual ~XSession() = default;

  /**
    Get client identifier.

    The identifier is used in/by "list_object", "kill_client" admin commands.

    @return Identifier that represents current connection/client
            in X Plugins context.
      @retval XCL_CLIENT_ID_NOT_VALID    Connection not established
      @retval != XCL_CLIENT_ID_NOT_VALID Valid client id
  */
  virtual XProtocol::Client_id client_id() const = 0;

  /**
    Get protocol layer of XSession.

    The lower layer can by used to execute custom flows, data or
    even add new behavior to already implemented flows:

    * XSession::execute_sql
    * XSession::execute_stmt
    * XSession::connect
  */
  virtual XProtocol &get_protocol() = 0;

  /**
    Modify mysqlx options.

    This method may only be called before calling `XSession::connect` method.

    @param option   option to set or modify
    @param value    assign bool value to the option

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_mysql_option(const Mysqlx_option option,
                                  const bool value) = 0;

  /**
    Modify mysqlx options.

    This method may only be called before calling `XSession::connect` method.

    @param option   option to set or modify
    @param value    assign string value to the option

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_mysql_option(const Mysqlx_option option,
                                  const std::string &value) = 0;
  /**
    Modify mysqlx options.

    This method may only be called before calling `XSession::connect` method.

    @param option       option to set or modify
    @param values_list  assign list of string values to the option

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_mysql_option(
      const Mysqlx_option option,
      const std::vector<std::string> &values_list) = 0;

  /**
    Modify mysqlx options.

    This method may only be called before calling `XSession::connect` method.

    @param option   option to set or modify
    @param value    assign "C" string value to the option

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_mysql_option(const Mysqlx_option option,
                                  const char *value) = 0;

  /**
    Modify mysqlx options.

    This method may only be called before calling `XSession::connect` method.

    @param option   option to set or modify
    @param value    assign integer value to the option

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_mysql_option(const Mysqlx_option option,
                                  const int64_t value) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign bool value to the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const bool value) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign string value to the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const std::string &value) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign "C" string value to the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const char *value) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign integer value to the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const int64_t value) = 0;

  /**
    Establish and authenticate connection using TCP.

    @param host         specifies destination address as host, ipv4, ipv6 add
    @param port         specify the TCP port on which X Plugin accepts
                        connections. When the value is set to 0, the connect
                        method is going to use default MySQL X port (defined in
                        mysqlx_version.h).
    @param user         MySQL Server accounts user name
    @param pass         MySQL Server accounts user password
    @param schema       schema to be selected on start

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError connect(const char *host, const uint16_t port,
                         const char *user, const char *pass,
                         const char *schema) = 0;
  /**
    Establish and authenticate connection using UNIX socket

    @param socket_file  connect to the server using the unix socket file.
                        When the value is empty string or nullptr, the method
                        is going to use default UNIX socket (defined in
                        mysqlx_version.h).
    @param user         MySQL Server accounts user name
    @param pass         MySQL Server accounts user password
    @param schema       schema to be selected on start

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError connect(const char *socket_file, const char *user,
                         const char *pass, const char *schema) = 0;

  /**
    Reauthenticate connection.

    Reset already established session and reauthenticate using
    new MySQL Servers account.
    This method can only be called after authentication.

    @param user         MySQL Server accounts user name
    @param pass         MySQL Server accounts user password
    @param schema       schema to be selected on start

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError reauthenticate(const char *user, const char *pass,
                                const char *schema) = 0;

  /**
    Execute SQL.

    This method can only be called after authentication.

    @param sql             string containing SQL to be executed on the server
    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  error occurred
  */
  virtual std::unique_ptr<XQuery_result> execute_sql(const std::string &sql,
                                                     XError *out_error) = 0;

  /**
    Execute statement on the server.

    This method can only be called after authentication.

    @param ns              namespace in which the statement should be executed:
                           * "sql" - interpret "stmt" string as SQL
                           * "mysqlx" - interpret "stmt" string as an admin
    command
    @param stmt            statement to be executed
    @param args            container with multiple values used at statement
    execution
    @param[out] out_error  in case of error, the method is going to return error
                           code and description
    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  error occurred
  */
  virtual std::unique_ptr<XQuery_result> execute_stmt(const std::string &ns,
                                                      const std::string &stmt,
                                                      const Arguments &args,
                                                      XError *out_error) = 0;

  /**
    Graceful shutdown maintaing the close connection message flow.

    Client application should call this directly before destroying the XSession
    object.
  */
  virtual void close() = 0;
};

/**
  Create, connect and authenticate the session using UNIX socket.

  @param socket_file    connect to the server using the unix socket file
  @param user           MySQL Server accounts user name
  @param pass           MySQL Server accounts user password
  @param schema         schema to be selected on start
  @param[out] out_error in case of error, the method is going to return error
                        code and description

  @return Object implementing XSesstion interface
    @retval != nullptr     OK
    @retval == nullptr     error occurred
*/
std::unique_ptr<XSession> create_session(const char *socket_file,
                                         const char *user, const char *pass,
                                         const char *schema, XError *out_error);

/**
  Create, connect and authenticate the session using TCP.

  @param host           specifies destination address as host, ipv4, ipv6 add
  @param port           specify the TCP port on which X Plugin accepts
                        connections
  @param user           MySQL Server accounts user name
  @param pass           MySQL Server accounts user password
  @param schema         schema to be selected on start
  @param[out] out_error in case of error, the method is going to return error
                        code and description

  @return Object implementing XSesstion interface
    @retval != nullptr     OK
    @retval == nullptr     error occurred
*/
std::unique_ptr<XSession> create_session(const char *host, const uint16_t port,
                                         const char *user, const char *pass,
                                         const char *schema, XError *out_error);

/**
  Create not connected session object.

  @return Object implementing XSesstion interface
    @retval != nullptr     OK
    @retval == nullptr     error occurred
*/
std::unique_ptr<XSession> create_session();

}  // namespace xcl

#endif  // X_CLIENT_MYSQLXCLIENT_XSESSION_H_
