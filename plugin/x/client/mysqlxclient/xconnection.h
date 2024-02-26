/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XCONNECTION_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XCONNECTION_H_

#include <string>

#include "mysql.h"
#include "mysqlxclient/xerror.h"

namespace xcl {

/**
  'Enum' that defines allowed version of Internet Protocol.

  The value defines which "socket-proto" must be used by implementer
  of XConnection interface also tell the resolver which IP addresses
  are allowed when resolving hostname to IP address.
*/
enum class Internet_protocol {
  Any = 0,
  V4,
  V6,
};

/** 'Enum' that defines how the network connection should be closed. */
enum class Shutdown_type { Send, Recv, Both };

/** 'Enum' that defines the connection type. */
enum class Connection_type { Unix_socket, Tcp };

/**
 Interface defining network layer.

 This is the lowest layer on which XSession or XProtocol implementers
 can operate on. It defines basic blocking I/O operations on a connection.
 Additionally it handles all data stream encoding/decoding (for example SSL).
*/
class XConnection {
 public:
  /** Interface describing the connection state. */
  class State {
   public:
    virtual ~State() = default;

    /** Check if SSL was configured */
    virtual bool is_ssl_configured() const = 0;

    /** Check if SSL layer works */
    virtual bool is_ssl_activated() const = 0;

    /** Check connection state */
    virtual bool is_connected() const = 0;

    /** Get version of the SSL protocol used */
    virtual std::string get_ssl_version() const = 0;

    /** Get cipher used by SSL layer */
    virtual std::string get_ssl_cipher() const = 0;

    /** Get connection type */
    virtual Connection_type get_connection_type() const = 0;

    /** Returns true if there is more data in TCP or SSL layers pending */
    virtual bool has_data() const = 0;
  };

 public:
  virtual ~XConnection() = default;

  /**
    Connect to UNIX socket.

    Connect is going to block until:

    * operation completed successfully
    * I/O error occurred
    * timeout occurred (@ref XSession::Mysqlx_option::Connect_timeout)

    @param unix_socket   UNIX socket file created by X Plugin

    @return Object holding result of the operation
      @retval == true   error occurred
      @retval == false  operation successful
  */
  virtual XError connect_to_localhost(const std::string &unix_socket) = 0;

  /**
    Connect to host through TCP/IP.

    Connect is going to block until:

    * operation completed successfully
    * I/O error occurred
    * timeout occurred

    @param host     hostname or IPv4 or IPv6 address
    @param port     TCP port used by X Plugin (running with X Protocol)
    @param ip_mode  defines allowed IP version

    @return Object holding result of the operation
      @retval == true   error occurred
      @retval == false  operation successful
  */
  virtual XError connect(const std::string &host, const uint16_t port,
                         const Internet_protocol ip_mode) = 0;

  /**
    Get the connections file descriptor (socket).

    Please be aware that after enabling SSL the data could be fetched inside
    SSLs buffers. Thus checking for activity by executing 'select'
    could lead to infinite wait. Similar problem can occur after enabling
    'timeouts' by calling 'set_read_timeout' or 'set_write_timeout'.

    @return Socket - file descriptor
  */
  virtual my_socket get_socket_fd() = 0;

  /**
    Activate TLS on the lowest layer.

    This method activates SSL and validates certificate authority when
    "SSL mode" is set to:

    * VERIFY_CA
    * VERIFY_IDENTITY

    Other SSL checks are going to be done by layer that calls this method.

    @return Object holding result of the operation
      @retval == true   error occurred
      @retval == false  operation successful
    */
  virtual XError activate_tls() = 0;

  /**
    Shutdown the connection.

    @param how_to_shutdown   define which part of the socket
                             should be closed (sending/receiving)

    @return Object holding result of the operation
      @retval == true   error occurred
      @retval == false  operation successful
   */
  virtual XError shutdown(const Shutdown_type how_to_shutdown) = 0;

  /**
    Write the data.

    Write operation is going to block until expected number of bytes has been
    send on TCP stack. In case when the write-timeout was set, the operation
    can block at most the given number of seconds.

    If the SSL is enabled the data is first encoded and then sent.

    @param data          payload to be sent
    @param data_length   size of the payload

    @return Object holding result of the operation
      @retval == true   error occurred
      @retval == false  operation successful
   */
  virtual XError write(const uint8_t *data, const std::size_t data_length) = 0;

  /**
    Read the data.

    Read operation is going to block until expected number of bytes has been
    received. In case when the read-timeout was set, the operation can block
    at most the given number of seconds.

    If the SSL is enabled the data is first decoded and then put into receive
    buffer.

    @param data          buffer which should receive/get data
    @param data_length   number of bytes which must be read from the
                         connection

    @return Object holding result of the operation
      @retval == true   error occurred
      @retval == false  operation successful
   */
  virtual XError read(uint8_t *data, const std::size_t data_length) = 0;

  /** Define timeout behavior when reading from the connection:

  @param deadline_seconds - values greater than 0, set number of seconds which
                            read operation can block
                          - value set to zero, do non blocking op
                          - value less than 0, do blocking op */
  virtual XError set_read_timeout(const int deadline_seconds) = 0;

  /** Define timeout behavior when writing from the connection:

  @param deadline_seconds - values greater than 0, set number of seconds which
                            write operation can block
                          - value set to zero, do non blocking op
                          - value less than 0, do blocking op */
  virtual XError set_write_timeout(const int deadline_seconds) = 0;

  /** Close connection. */
  virtual void close() = 0;

  /** Get state of the connection. */
  virtual const State &state() = 0;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XCONNECTION_H_
