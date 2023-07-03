/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XSESSION_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XSESSION_H_

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

    /**
      The server may want know more about the client. MySQL X clients connection
      the server may differ in: application version, MySQL library version,
      OS, CPU, CPU endianess, programming language. Having more information
      about clients/statistics is going to help server administrators with
      finding faulty clients, targeting potential issues and allow better
      optimizing.
      Capability type: OBJECT. Value: associative array of strings where
      key name must not exceed 32 characters and value must not exceed
      1024 characters.
     */
    Capability_session_connect_attrs,

    /**
      Enable compression and choose the algorithm and style.

      Capability type: OBJECT.
      Key: "algorithm" = type STRING;
            one of "deflate_stream|lz4_message|zstd_stream".
      Key: "server_combine_mixed_messages" = type BOOL;
           if true, server is allowed to combine different message types
           into a compressed message.
      Key: "server_max_combine_messages" = type INT;
           if set, the server MUST not store more than N uncompressed
           messages into a compressed message.
     */
    Capability_compression,
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
      Define session establishment timeout, which involves the following steps:
      * hostname resolve
      * socket-connection
      * X Protocol handshake
      * X Protocol authentication until AuthenticationOk
      Default value of this parameter is set to "-1", which means infinite
      block. Values greater than "-1" define the timeout in milliseconds.

      Default: -1.
      Option type: INTEGER.
     */
    Session_connect_timeout,
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
    /** Determine what should be the length of a DATETIME field so that it
        would be possible to distinguish if it contain only date or both
        date and time parts.

      Default: 10
      Option type: INT
    */
    Datetime_length_discriminator,
    /** Network namespace (if any) which should be used at connection
      establishment.

      Default:
      Option type: STRING
    */
    Network_namespace,
    /** Compression negotiation check which compression algorithms or styles
      are supported by the server and choose one supported by both sides.
      Setting compression from "options" has priority over
      "capabilities". If "compression_negotiation_mode" will have other value
      than "DISABLED", then it will overwrite settings done by "capabilities".

      Following modes are possible:

      * "DISABLED" - client doesn't wont to use negotiation
      * "REQUIRED" - if there is no common compression configuration or setting
                     it fails, then such connection is rejected
      * "PREFERRED" - client tries to negotiate compression configuration still
                     when it fail, the connection is still usable


      Default: "DISABLED"
      Option type: STRING
    */
    Compression_negotiation_mode,
    /** Try to negotiate following compression algorithms

      Default: ["deflate_stream","lz4_message","zstd_stream"]
      Option type: STRING, ARRAY OF STRINGS
    */
    Compression_algorithms,
    /** The server is allowed to combine different message types
      into a compressed message.

      Default: true
      Option type: BOOL
     */
    Compression_combine_mixed_messages,
    /** The server MUST not store more than N uncompressed
      messages into a compressed message.

      Default: 0 (no limit)
      Option type: INTEGER
     */
    Compression_max_combine_messages,

    /** The server can compress messages at a given level.

      Default: -2^63 (default level depend on compression algorithm
                      and the server configuration)
      Option type: INTEGER
     */
    Compression_level_server,
    /** The client can compress messages at a given level.

      Default: -2^63 (default level depend on compression algorithm;
                      deflate_stream:3, lz4_frame:2, zstd_stream:3)
      Option type: INTEGER
     */
    Compression_level_client,
    /** The client can read responses while pipelining multiple requests.

      Default: 64k
      Option type: INTEGER
     */
    Buffer_recevie_size
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
    @param required     define if connection should be accepted by client when
                        server rejected the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const bool value,
                                const bool required = true) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign string value to the capability
    @param required     define if connection should be accepted by client when
                        server rejected the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const std::string &value,
                                const bool required = true) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign "C" string value to the capability
    @param required     define if connection should be accepted by client when
                        server rejected the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const char *value,
                                const bool required = true) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign integer value to the capability
    @param required     define if connection should be accepted by client when
                        server rejected the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const int64_t value,
                                const bool required = true) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign object value to the capability
    @param required     define if connection should be accepted by client when
                        server rejected the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const Argument_object &value,
                                const bool required = true) = 0;

  /**
    Set X protocol capabilities.

    All capabilities set before calling `XSession::connect` method are
    committed to the server (other side of the connection).

    @param capability   capability to set or modify
    @param value        assign 'unordered' object value to the capability
    @param required     define if connection should be accepted by client when
                        server rejected the capability

    @return Error code with description
      @retval != true     OK
      @retval == true     error occurred
  */
  virtual XError set_capability(const Mysqlx_capability capability,
                                const Argument_uobject &value,
                                const bool required = true) = 0;

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
  virtual std::unique_ptr<XQuery_result> execute_stmt(
      const std::string &ns, const std::string &stmt,
      const Argument_array &args, XError *out_error) = 0;

  /**
    Graceful shutdown maintaing the close connection message flow.

    Client application should call this directly before destroying the XSession
    object.
  */
  virtual void close() = 0;

  /**
   Get pre-filled session connection attributes.
   If necessary could be supplemented with additional information before
   sending it as capability to the server.
   */
  virtual Argument_uobject get_connect_attrs() const = 0;
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

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XSESSION_H_
