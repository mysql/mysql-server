/*
   Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SOCKET_CONNECTION_INCLUDED
#define SOCKET_CONNECTION_INCLUDED

#include "my_config.h"

#include <sys/types.h>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "my_psi_config.h"
#include "mysql/components/services/psi_statement_bits.h"
#include "mysql/psi/mysql_socket.h"  // MYSQL_SOCKET
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

class Channel_info;

extern const char *MY_BIND_ALL_ADDRESSES;
extern const char *ipv4_all_addresses;
extern const char *ipv6_all_addresses;

#ifdef HAVE_PSI_STATEMENT_INTERFACE
extern PSI_statement_info stmt_info_new_packet;
#endif

/**
  Key Comparator for socket_map_t used in Mysqld_socket_listener
*/
struct Socket_lt_type {
  bool operator()(const MYSQL_SOCKET &s1, const MYSQL_SOCKET &s2) const {
    return mysql_socket_getfd(s1) < mysql_socket_getfd(s2);
  }
};

/**
  Typedef representing socket map type which hold the sockets and a
  corresponding bool which is true if it is unix socket and false for tcp
  socket.
*/
typedef std::map<MYSQL_SOCKET, bool, Socket_lt_type> socket_map_t;

// iterator type for socket map type.
typedef std::map<MYSQL_SOCKET, bool, Socket_lt_type>::iterator
    socket_map_iterator_t;

/**
  This class represents the Mysqld_socket_listener which prepare the
  listener sockets to recieve connection events from the client. The
  Mysqld_socket_listener may be composed of either or both a tcp socket
  which listen on a default mysqld tcp port or a user specified  port
  via mysqld command-line  and a unix socket which is bind to a mysqld
  defaul pathname.
*/
class Mysqld_socket_listener {
  std::list<std::string> m_bind_addresses;  // addresses to listen to
  uint m_tcp_port;                          // TCP port to bind to
  uint m_backlog;       // backlog specifying length of pending connection queue
  uint m_port_timeout;  // port timeout value
  std::string m_unix_sockname;  // unix socket pathname to bind to
  bool m_unlink_sockname;       // Unlink socket & lock file if true.
  /*
    Map indexed by MYSQL socket fds and correspoding bool to distinguish
    between unix and tcp socket.
  */
  socket_map_t m_socket_map;  // map indexed by mysql socket fd and index

  uint m_error_count;  // Internal variable for maintaining error count.

#ifdef HAVE_POLL
  struct poll_info_t {
    std::vector<struct pollfd> m_fds;
    std::vector<MYSQL_SOCKET> m_pfs_fds;
  };
  // poll related info. used in poll for listening to connection events.
  poll_info_t m_poll_info;
#else
  struct select_info_t {
    fd_set m_read_fds, m_client_fds;
    my_socket m_max_used_connection;
    select_info_t() : m_max_used_connection(0) { FD_ZERO(&m_client_fds); }
  };
  // select info for used in select for listening to connection events.
  select_info_t m_select_info;
#endif  // HAVE_POLL

#ifdef HAVE_LIBWRAP
  const char *m_libwrap_name;
  int m_deny_severity;
#endif

  /** Number of connection errors when selecting on the listening port */
  static ulong connection_errors_select;
  /** Number of connection errors when accepting sockets in the listening port.
   */
  static ulong connection_errors_accept;
  /** Number of connection errors from TCP wrappers. */
  static ulong connection_errors_tcpwrap;

 public:
  static ulong get_connection_errors_select() {
    return connection_errors_select;
  }

  static ulong get_connection_errors_accept() {
    return connection_errors_accept;
  }

  static ulong get_connection_errors_tcpwrap() {
    return connection_errors_tcpwrap;
  }

  /**
    Constructor to setup a listener for listen to connect events from
    clients.

    @param   bind_addresses  list of addresses to listen to
    @param   tcp_port        TCP port to bind to
    @param   backlog         backlog specifying length of pending
                             connection queue used in listen.
    @param   port_timeout    portname.
    @param   unix_sockname   pathname for unix socket to bind to
  */
  Mysqld_socket_listener(const std::list<std::string> &bind_addresses,
                         uint tcp_port, uint backlog, uint port_timeout,
                         std::string unix_sockname);

  /**
    Set up a listener - set of sockets to listen for connection events
    from clients.

    @retval false  listener sockets setup to be used to listen for connect
    events true   failure in setting up the listener.
  */
  bool setup_listener();

  /**
    The body of the event loop that listen for connection events from clients.

    @retval Channel_info   Channel_info object abstracting the connected client
                           details for processing this connection.
  */
  Channel_info *listen_for_connection_event();

  /**
    Close the listener.
  */
  void close_listener();

  ~Mysqld_socket_listener() {
    if (!m_socket_map.empty()) close_listener();
  }
};

#endif  // SOCKET_CONNECTION_INCLUDED.
