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

#include "sql/conn_handler/socket_connection.h"

#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <algorithm>
#include <new>
#include <utility>

#include "m_string.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "my_thread.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/conn_handler/channel_info.h"               // Channel_info
#include "sql/conn_handler/init_net_server_extension.h"  // init_net_server_extension
#include "sql/log.h"
#include "sql/mysqld.h"     // key_socket_tcpip
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "violite.h"  // Vio
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_LIBWRAP
#include <syslog.h>
#ifndef HAVE_LIBWRAP_PROTOTYPES
extern "C" {
#include <tcpd.h>
}
#else
#include <tcpd.h>
#endif
#endif

using std::max;

ulong Mysqld_socket_listener::connection_errors_select = 0;
ulong Mysqld_socket_listener::connection_errors_accept = 0;
ulong Mysqld_socket_listener::connection_errors_tcpwrap = 0;

///////////////////////////////////////////////////////////////////////////
// Channel_info_local_socket implementation
///////////////////////////////////////////////////////////////////////////

/**
  This class abstracts the info. about local socket mode of communication with
  the server.
*/
class Channel_info_local_socket : public Channel_info {
  // connect socket object
  MYSQL_SOCKET m_connect_sock;

 protected:
  virtual Vio *create_and_init_vio() const {
    Vio *vio =
        mysql_socket_vio_new(m_connect_sock, VIO_TYPE_SOCKET, VIO_LOCALHOST);
#ifdef USE_PPOLL_IN_VIO
    if (vio != nullptr) {
      vio->thread_id = my_thread_self();
      vio->signal_mask = mysqld_signal_mask;
    }
#endif
    return vio;
  }

 public:
  /**
    Constructor that sets the connect socket.

    @param connect_socket set connect socket descriptor.
  */
  Channel_info_local_socket(MYSQL_SOCKET connect_socket)
      : m_connect_sock(connect_socket) {}

  virtual THD *create_thd() {
    THD *thd = Channel_info::create_thd();

    if (thd != NULL) {
      init_net_server_extension(thd);
      thd->security_context()->set_host_ptr(my_localhost, strlen(my_localhost));
    }
    return thd;
  }

  virtual void send_error_and_close_channel(uint errorcode, int error,
                                            bool senderror) {
    Channel_info::send_error_and_close_channel(errorcode, error, senderror);

    mysql_socket_shutdown(m_connect_sock, SHUT_RDWR);
    mysql_socket_close(m_connect_sock);
  }
};

///////////////////////////////////////////////////////////////////////////
// Channel_info_tcpip_socket implementation
///////////////////////////////////////////////////////////////////////////

/**
  This class abstracts the info. about TCP/IP socket mode of communication with
  the server.
*/
class Channel_info_tcpip_socket : public Channel_info {
  // connect socket object
  MYSQL_SOCKET m_connect_sock;

 protected:
  virtual Vio *create_and_init_vio() const {
    Vio *vio = mysql_socket_vio_new(m_connect_sock, VIO_TYPE_TCPIP, 0);
#ifdef USE_PPOLL_IN_VIO
    if (vio != nullptr) {
      vio->thread_id = my_thread_self();
      vio->signal_mask = mysqld_signal_mask;
    }
#endif
    return vio;
  }

 public:
  /**
    Constructor that sets the connect socket.

    @param connect_socket set connect socket descriptor.
  */
  Channel_info_tcpip_socket(MYSQL_SOCKET connect_socket)
      : m_connect_sock(connect_socket) {}

  virtual THD *create_thd() {
    THD *thd = Channel_info::create_thd();

    if (thd != NULL) init_net_server_extension(thd);
    return thd;
  }

  virtual void send_error_and_close_channel(uint errorcode, int error,
                                            bool senderror) {
    Channel_info::send_error_and_close_channel(errorcode, error, senderror);

    mysql_socket_shutdown(m_connect_sock, SHUT_RDWR);
    mysql_socket_close(m_connect_sock);
  }
};

///////////////////////////////////////////////////////////////////////////
// TCP_socket implementation
///////////////////////////////////////////////////////////////////////////

/**
  MY_BIND_ALL_ADDRESSES defines a special value for the bind-address option,
  which means that the server should listen to all available network addresses,
  both IPv6 (if available) and IPv4.

  Basically, this value instructs the server to make an attempt to bind the
  server socket to '::' address, and rollback to '0.0.0.0' if the attempt fails.
*/
const char *MY_BIND_ALL_ADDRESSES = "*";

/**
  TCP_socket class represents the TCP sockets abstraction. It provides
  the get_listener_socket that setup a TCP listener socket to listen.
*/
class TCP_socket {
  std::string m_bind_addr_str;  // IP address as string.
  uint m_tcp_port;              // TCP port to bind to
  uint m_backlog;       // Backlog length for queue of pending connections.
  uint m_port_timeout;  // Port timeout

  MYSQL_SOCKET create_socket(const struct addrinfo *addrinfo_list,
                             int addr_family, struct addrinfo **use_addrinfo) {
    for (const struct addrinfo *cur_ai = addrinfo_list; cur_ai != NULL;
         cur_ai = cur_ai->ai_next) {
      if (cur_ai->ai_family != addr_family) continue;

      MYSQL_SOCKET sock =
          mysql_socket_socket(key_socket_tcpip, cur_ai->ai_family,
                              cur_ai->ai_socktype, cur_ai->ai_protocol);

      char ip_addr[INET6_ADDRSTRLEN];

      if (vio_getnameinfo(cur_ai->ai_addr, ip_addr, sizeof(ip_addr), NULL, 0,
                          NI_NUMERICHOST)) {
        ip_addr[0] = 0;
      }

      if (mysql_socket_getfd(sock) == INVALID_SOCKET) {
        LogErr(ERROR_LEVEL, ER_CONN_TCP_NO_SOCKET,
               (addr_family == AF_INET) ? "IPv4" : "IPv6",
               (const char *)ip_addr, (int)socket_errno);
      } else {
        LogErr(INFORMATION_LEVEL, ER_CONN_TCP_CREATED, (const char *)ip_addr);

        *use_addrinfo = (struct addrinfo *)cur_ai;
        return sock;
      }
    }

    return MYSQL_INVALID_SOCKET;
  }

 public:
  /**
    Constructor that takes tcp port and ip address string and other
    related parameters to set up listener tcp to listen for connection
    events.

    @param  bind_addr_str  ip address as string value.
    @param  tcp_port  tcp port number.
    @param  backlog backlog specifying length of pending connection queue.
    @param  port_timeout port timeout value
  */
  TCP_socket(std::string bind_addr_str, uint tcp_port, uint backlog,
             uint port_timeout)
      : m_bind_addr_str(bind_addr_str),
        m_tcp_port(tcp_port),
        m_backlog(backlog),
        m_port_timeout(port_timeout) {}

  /**
    Set up a listener to listen for connection events.

    @retval   valid socket if successful else MYSQL_INVALID_SOCKET on failure.
  */
  MYSQL_SOCKET get_listener_socket() {
    struct addrinfo *ai;
    const char *bind_address_str = NULL;

    LogErr(INFORMATION_LEVEL, ER_CONN_TCP_ADDRESS, m_bind_addr_str.c_str(),
           m_tcp_port);

    // Get list of IP-addresses associated with the bind-address.

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    char port_buf[NI_MAXSERV];
    snprintf(port_buf, NI_MAXSERV, "%d", m_tcp_port);

    if (native_strcasecmp(my_bind_addr_str, MY_BIND_ALL_ADDRESSES) == 0) {
      /*
        That's the case when bind-address is set to a special value ('*'),
        meaning "bind to all available IP addresses". If the box supports
        the IPv6 stack, that means binding to '::'. If only IPv4 is available,
        bind to '0.0.0.0'.
      */

      bool ipv6_available = false;
      const char *ipv6_all_addresses = "::";
      if (!getaddrinfo(ipv6_all_addresses, port_buf, &hints, &ai)) {
        /*
          IPv6 might be available (the system might be able to resolve an IPv6
          address, but not be able to create an IPv6-socket). Try to create a
          dummy IPv6-socket. Do not instrument that socket by P_S.
        */

        MYSQL_SOCKET s = mysql_socket_socket(0, AF_INET6, SOCK_STREAM, 0);
        ipv6_available = mysql_socket_getfd(s) != INVALID_SOCKET;
        if (ipv6_available) mysql_socket_close(s);
      }
      if (ipv6_available) {
        LogErr(INFORMATION_LEVEL, ER_CONN_TCP_IPV6_AVAILABLE);

        // Address info (ai) for IPv6 address is already set.

        bind_address_str = ipv6_all_addresses;
      } else {
        LogErr(INFORMATION_LEVEL, ER_CONN_TCP_IPV6_UNAVAILABLE);

        // Retrieve address info (ai) for IPv4 address.

        const char *ipv4_all_addresses = "0.0.0.0";
        if (getaddrinfo(ipv4_all_addresses, port_buf, &hints, &ai)) {
          LogErr(ERROR_LEVEL, ER_CONN_TCP_ERROR_WITH_STRERROR, strerror(errno));
          LogErr(ERROR_LEVEL, ER_CONN_TCP_CANT_RESOLVE_HOSTNAME);
          return MYSQL_INVALID_SOCKET;
        }

        bind_address_str = ipv4_all_addresses;
      }
    } else {
      if (getaddrinfo(m_bind_addr_str.c_str(), port_buf, &hints, &ai)) {
        LogErr(ERROR_LEVEL, ER_CONN_TCP_ERROR_WITH_STRERROR, strerror(errno));
        LogErr(ERROR_LEVEL, ER_CONN_TCP_CANT_RESOLVE_HOSTNAME);
        return MYSQL_INVALID_SOCKET;
      }

      bind_address_str = m_bind_addr_str.c_str();
    }

    // Log all the IP-addresses
    for (struct addrinfo *cur_ai = ai; cur_ai != NULL;
         cur_ai = cur_ai->ai_next) {
      char ip_addr[INET6_ADDRSTRLEN];

      if (vio_getnameinfo(cur_ai->ai_addr, ip_addr, sizeof(ip_addr), NULL, 0,
                          NI_NUMERICHOST)) {
        LogErr(ERROR_LEVEL, ER_CONN_TCP_IP_NOT_LOGGED);
        continue;
      }

      LogErr(INFORMATION_LEVEL, ER_CONN_TCP_RESOLVE_INFO, bind_address_str,
             ip_addr);
    }

    /*
      If the 'bind-address' option specifies the hostname, which resolves to
      multiple IP-address, use the following rule:
      - if there are IPv4-addresses, use the first IPv4-address
      returned by getaddrinfo();
      - if there are IPv6-addresses, use the first IPv6-address
      returned by getaddrinfo();
    */

    struct addrinfo *a = nullptr;
    MYSQL_SOCKET listener_socket = create_socket(ai, AF_INET, &a);

    if (mysql_socket_getfd(listener_socket) == INVALID_SOCKET)
      listener_socket = create_socket(ai, AF_INET6, &a);

    // Report user-error if we failed to create a socket.
    if (mysql_socket_getfd(listener_socket) == INVALID_SOCKET) {
      LogErr(ERROR_LEVEL, ER_CONN_TCP_ERROR_WITH_STRERROR, strerror(errno));
      return MYSQL_INVALID_SOCKET;
    }

    mysql_socket_set_thread_owner(listener_socket);

#ifndef _WIN32
    /*
      We should not use SO_REUSEADDR on windows as this would enable a
      user to open two mysqld servers with the same TCP/IP port.
    */
    {
      int option_flag = 1;
      (void)mysql_socket_setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR,
                                    (char *)&option_flag, sizeof(option_flag));
    }
#endif
#ifdef IPV6_V6ONLY
    /*
      For interoperability with older clients, IPv6 socket should
      listen on both IPv6 and IPv4 wildcard addresses.
      Turn off IPV6_V6ONLY option.

      NOTE: this will work starting from Windows Vista only.
      On Windows XP dual stack is not available, so it will not
      listen on the corresponding IPv4-address.
    */
    if (a->ai_family == AF_INET6) {
      int option_flag = 0;

      if (mysql_socket_setsockopt(listener_socket, IPPROTO_IPV6, IPV6_V6ONLY,
                                  (char *)&option_flag, sizeof(option_flag))) {
        LogErr(WARNING_LEVEL, ER_CONN_TCP_CANT_RESET_V6ONLY, (int)socket_errno);
      }
    }
#endif
    /*
      Sometimes the port is not released fast enough when stopping and
      restarting the server. This happens quite often with the test suite
      on busy Linux systems. Retry to bind the address at these intervals:
      Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
      Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
      Limit the sequence by m_port_timeout (set --port-open-timeout=#).
    */
    uint this_wait = 0;
    int ret = 0;
    for (uint waited = 0, retry = 1;; retry++, waited += this_wait) {
      if (((ret = mysql_socket_bind(listener_socket, a->ai_addr,
                                    a->ai_addrlen)) >= 0) ||
          (socket_errno != SOCKET_EADDRINUSE) || (waited >= m_port_timeout))
        break;
      LogErr(INFORMATION_LEVEL, ER_CONN_TCP_BIND_RETRY, mysqld_port);
      this_wait = retry * retry / 3 + 1;
      sleep(this_wait);
    }
    freeaddrinfo(ai);
    if (ret < 0) {
      DBUG_PRINT("error", ("Got error: %d from bind", socket_errno));
      LogErr(ERROR_LEVEL, ER_CONN_TCP_BIND_FAIL, strerror(socket_errno));
      LogErr(ERROR_LEVEL, ER_CONN_TCP_IS_THERE_ANOTHER_USING_PORT, m_tcp_port);
      mysql_socket_close(listener_socket);
      return MYSQL_INVALID_SOCKET;
    }

    if (mysql_socket_listen(listener_socket, static_cast<int>(m_backlog)) < 0) {
      LogErr(ERROR_LEVEL, ER_CONN_TCP_START_FAIL, strerror(errno));
      LogErr(ERROR_LEVEL, ER_CONN_TCP_LISTEN_FAIL, socket_errno);
      mysql_socket_close(listener_socket);
      return MYSQL_INVALID_SOCKET;
    }

#if !defined(NO_FCNTL_NONBLOCK)
    (void)mysql_sock_set_nonblocking(listener_socket);
#endif

    return listener_socket;
  }
};

#if defined(HAVE_SYS_UN_H)
///////////////////////////////////////////////////////////////////////////
// Unix_socket implementation
///////////////////////////////////////////////////////////////////////////

/**
  The Unix_socket class represents an abstraction for creating a unix
  socket ready to listen for new connections from clients.
*/
class Unix_socket {
  std::string m_unix_sockname;  // pathname for socket to bind to.
  uint m_backlog;  // backlog specifying lenght of pending queue connection.
  /**
    Create a lockfile which contains the pid of the mysqld instance started
    and pathname as name of unix socket pathname appended with .lock

    @retval   false if lockfile creation is successful else true if lockfile
              file could not be created.

  */
  bool create_lockfile();

 public:
  /**
    Constructor that takes pathname for unix socket to bind to
    and backlog specifying the length of pending connection queue.

    @param  unix_sockname pointer to pathname for the created unix socket
            to bind.
    @param  backlog   specifying the length of pending connection queue.
  */
  Unix_socket(const std::string *unix_sockname, uint backlog)
      : m_unix_sockname(*unix_sockname), m_backlog(backlog) {}

  /**
    Set up a listener socket which is ready to listen for connection from
    clients.

    @retval   valid socket if successful else MYSQL_INVALID_SOCKET on failure.
  */
  MYSQL_SOCKET get_listener_socket() {
    struct sockaddr_un UNIXaddr;
    DBUG_PRINT("general", ("UNIX Socket is %s", m_unix_sockname.c_str()));

    // Check path length, probably move to set unix port?
    if (m_unix_sockname.length() > (sizeof(UNIXaddr.sun_path) - 1)) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_PATH_TOO_LONG,
             (uint)sizeof(UNIXaddr.sun_path) - 1, m_unix_sockname.c_str());
      return MYSQL_INVALID_SOCKET;
    }

    if (create_lockfile()) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_FAIL);
      return MYSQL_INVALID_SOCKET;
    }

    MYSQL_SOCKET listener_socket =
        mysql_socket_socket(key_socket_unix, AF_UNIX, SOCK_STREAM, 0);

    if (mysql_socket_getfd(listener_socket) < 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_NO_FD, strerror(errno));
      return MYSQL_INVALID_SOCKET;
    }

    mysql_socket_set_thread_owner(listener_socket);

    memset(&UNIXaddr, 0, sizeof(UNIXaddr));
    UNIXaddr.sun_family = AF_UNIX;
    my_stpcpy(UNIXaddr.sun_path, m_unix_sockname.c_str());
    (void)unlink(m_unix_sockname.c_str());

    // Set socket option SO_REUSEADDR
    int option_enable = 1;
    (void)mysql_socket_setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR,
                                  (char *)&option_enable,
                                  sizeof(option_enable));
    // bind
    umask(0);
    if (mysql_socket_bind(listener_socket,
                          reinterpret_cast<struct sockaddr *>(&UNIXaddr),
                          sizeof(UNIXaddr)) < 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_NO_BIND_NO_START, strerror(errno));
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_IS_THERE_ANOTHER_USING_SOCKET,
             m_unix_sockname.c_str());
      mysql_socket_close(listener_socket);
      return MYSQL_INVALID_SOCKET;
    }
    umask(((~my_umask) & 0666));

    // listen
    if (mysql_socket_listen(listener_socket, (int)m_backlog) < 0)
      LogErr(WARNING_LEVEL, ER_CONN_UNIX_LISTEN_FAILED, socket_errno);

      // set sock fd non blocking.
#if !defined(NO_FCNTL_NONBLOCK)
    (void)mysql_sock_set_nonblocking(listener_socket);
#endif

    return listener_socket;
  }
};

bool Unix_socket::create_lockfile() {
  int fd;
  char buffer[8];
  pid_t cur_pid = getpid();
  std::string lock_filename = m_unix_sockname + ".lock";

  static_assert(sizeof(pid_t) == 4, "");
  int retries = 3;
  while (true) {
    if (!retries--) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_GIVING_UP,
             lock_filename.c_str());
      return true;
    }

    fd = open(lock_filename.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);

    if (fd >= 0) break;

    if (errno != EEXIST) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_CREATE,
             lock_filename.c_str());
      return true;
    }

    fd = open(lock_filename.c_str(), O_RDONLY, 0600);
    if (fd < 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_OPEN,
             lock_filename.c_str());
      return true;
    }

    ssize_t len;
    if ((len = read(fd, buffer, sizeof(buffer) - 1)) < 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_READ,
             lock_filename.c_str());
      close(fd);
      return true;
    }

    close(fd);

    if (len == 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_EMPTY, lock_filename.c_str());
      return true;
    }
    buffer[len] = '\0';

    pid_t parent_pid = getppid();
    pid_t read_pid = atoi(buffer);

    if (read_pid <= 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_PIDLESS,
             lock_filename.c_str());
      return true;
    }

    if (read_pid != cur_pid && read_pid != parent_pid) {
      if (kill(read_pid, 0) == 0) {
        LogErr(ERROR_LEVEL, ER_CONN_UNIX_PID_CLAIMED_SOCKET_FILE,
               static_cast<int>(read_pid));
        return true;
      }
    }

    /*
      Unlink the lock file as it is not associated with any process and
      retry.
    */
    if (unlink(lock_filename.c_str()) < 0) {
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_DELETE,
             lock_filename.c_str(), errno);
      return true;
    }
  }

  snprintf(buffer, sizeof(buffer), "%d\n", static_cast<int>(cur_pid));
  if (write(fd, buffer, strlen(buffer)) !=
      static_cast<signed>(strlen(buffer))) {
    close(fd);
    LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_WRITE,
           lock_filename.c_str(), errno);

    if (unlink(lock_filename.c_str()) == -1)
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_DELETE,
             lock_filename.c_str(), errno);

    return true;
  }

  if (fsync(fd) != 0) {
    close(fd);
    LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_SYNC, lock_filename.c_str(),
           errno);

    if (unlink(lock_filename.c_str()) == -1)
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_DELETE,
             lock_filename.c_str(), errno);

    return true;
  }

  if (close(fd) != 0) {
    LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_CLOSE,
           lock_filename.c_str(), errno);

    if (unlink(lock_filename.c_str()) == -1)
      LogErr(ERROR_LEVEL, ER_CONN_UNIX_LOCK_FILE_CANT_DELETE,
             lock_filename.c_str(), errno);

    return true;
  }
  return false;
}

#endif  // HAVE_SYS_UN_H

///////////////////////////////////////////////////////////////////////////
// Mysqld_socket_listener implementation
///////////////////////////////////////////////////////////////////////////

Mysqld_socket_listener::Mysqld_socket_listener(std::string bind_addr_str,
                                               uint tcp_port, uint backlog,
                                               uint port_timeout,
                                               std::string unix_sockname)
    : m_bind_addr_str(bind_addr_str),
      m_tcp_port(tcp_port),
      m_backlog(backlog),
      m_port_timeout(port_timeout),
      m_unix_sockname(unix_sockname),
      m_unlink_sockname(false),
      m_error_count(0) {
#ifdef HAVE_LIBWRAP
  m_deny_severity = LOG_WARNING;
  m_libwrap_name = my_progname + dirname_length(my_progname);
  if (!opt_log_syslog_enable) openlog(m_libwrap_name, LOG_PID, LOG_AUTH);
#endif /* HAVE_LIBWRAP */
}

bool Mysqld_socket_listener::setup_listener() {
  // Setup tcp socket listener
  if (m_tcp_port) {
    TCP_socket tcp_socket(m_bind_addr_str, m_tcp_port, m_backlog,
                          m_port_timeout);

    MYSQL_SOCKET mysql_socket = tcp_socket.get_listener_socket();
    if (mysql_socket.fd == INVALID_SOCKET) return true;

    m_socket_map.insert(std::pair<MYSQL_SOCKET, bool>(mysql_socket, false));
  }
#if defined(HAVE_SYS_UN_H)
  // Setup unix socket listener
  if (m_unix_sockname != "") {
    Unix_socket unix_socket(&m_unix_sockname, m_backlog);

    MYSQL_SOCKET mysql_socket = unix_socket.get_listener_socket();
    if (mysql_socket.fd == INVALID_SOCKET) return true;

    m_socket_map.insert(std::pair<MYSQL_SOCKET, bool>(mysql_socket, true));
    m_unlink_sockname = true;
  }
#endif /* HAVE_SYS_UN_H */

    // Setup for connection events for poll or select
#ifdef HAVE_POLL
  int count = 0;
#endif
  for (socket_map_iterator_t sock_map_iter = m_socket_map.begin();
       sock_map_iter != m_socket_map.end(); ++sock_map_iter) {
    MYSQL_SOCKET listen_socket = sock_map_iter->first;
    mysql_socket_set_thread_owner(listen_socket);
#ifdef HAVE_POLL
    m_poll_info.m_fds[count].fd = mysql_socket_getfd(listen_socket);
    m_poll_info.m_fds[count].events = POLLIN;
    m_poll_info.m_pfs_fds[count] = listen_socket;
    count++;
#else   // HAVE_POLL
    FD_SET(mysql_socket_getfd(listen_socket), &m_select_info.m_client_fds);
    if ((uint)mysql_socket_getfd(listen_socket) >
        m_select_info.m_max_used_connection)
      m_select_info.m_max_used_connection = mysql_socket_getfd(listen_socket);
#endif  // HAVE_POLL
  }
  return false;
}

Channel_info *Mysqld_socket_listener::listen_for_connection_event() {
#ifdef HAVE_POLL
  int retval = poll(&m_poll_info.m_fds[0], m_socket_map.size(), -1);
#else
  m_select_info.m_read_fds = m_select_info.m_client_fds;
  int retval = select((int)m_select_info.m_max_used_connection,
                      &m_select_info.m_read_fds, 0, 0, 0);
#endif

  if (retval < 0 && socket_errno != SOCKET_EINTR) {
    /*
      select(2)/poll(2) failed on the listening port.
      There is not much details to report about the client,
      increment the server global status variable.
    */
    connection_errors_select++;
    if (!select_errors++ && !connection_events_loop_aborted())
      LogErr(ERROR_LEVEL, ER_CONN_SOCKET_SELECT_FAILED, socket_errno);
  }

  if (retval < 0 || connection_events_loop_aborted()) return NULL;

  /* Is this a new connection request ? */
  MYSQL_SOCKET listen_sock = MYSQL_INVALID_SOCKET;
  bool is_unix_socket = false;
#ifdef HAVE_POLL
  for (uint i = 0; i < m_socket_map.size(); ++i) {
    if (m_poll_info.m_fds[i].revents & POLLIN) {
      listen_sock = m_poll_info.m_pfs_fds[i];
      is_unix_socket = m_socket_map[listen_sock];
      break;
    }
  }
#else   // HAVE_POLL
  for (socket_map_iterator_t sock_map_iter = m_socket_map.begin();
       sock_map_iter != m_socket_map.end(); ++sock_map_iter) {
    if (FD_ISSET(mysql_socket_getfd(sock_map_iter->first),
                 &m_select_info.m_read_fds)) {
      listen_sock = sock_map_iter->first;
      is_unix_socket = sock_map_iter->second;
      break;
    }
  }
#endif  // HAVE_POLL

  MYSQL_SOCKET connect_sock;
  struct sockaddr_storage cAddr;
  for (uint retry = 0; retry < MAX_ACCEPT_RETRY; retry++) {
    socket_len_t length = sizeof(struct sockaddr_storage);
    connect_sock =
        mysql_socket_accept(key_socket_client_connection, listen_sock,
                            (struct sockaddr *)(&cAddr), &length);
    if (mysql_socket_getfd(connect_sock) != INVALID_SOCKET ||
        (socket_errno != SOCKET_EINTR && socket_errno != SOCKET_EAGAIN))
      break;
  }
  if (mysql_socket_getfd(connect_sock) == INVALID_SOCKET) {
    /*
      accept(2) failed on the listening port, after many retries.
      There is not much details to report about the client,
      increment the server global status variable.
    */
    connection_errors_accept++;
    if ((m_error_count++ & 255) == 0)  // This can happen often
      LogErr(ERROR_LEVEL, ER_CONN_SOCKET_ACCEPT_FAILED, strerror(errno));
    if (socket_errno == SOCKET_ENFILE || socket_errno == SOCKET_EMFILE)
      sleep(1);  // Give other threads some time
    return NULL;
  }

#ifdef HAVE_LIBWRAP
  if (!is_unix_socket) {
    struct request_info req;
    signal(SIGCHLD, SIG_DFL);
    request_init(&req, RQ_DAEMON, m_libwrap_name, RQ_FILE,
                 mysql_socket_getfd(connect_sock), NULL);
    fromhost(&req);

    if (!hosts_access(&req)) {
      /*
        This may be stupid but refuse() includes an exit(0)
        which we surely don't want...
        clean_exit() - same stupid thing ...
      */
      syslog(LOG_AUTH | m_deny_severity, "refused connect from %s",
             eval_client(&req));

#ifdef HAVE_LIBWRAP_PROTOTYPES
      // Some distros have patched tcpd.h to have proper prototypes
      if (req.sink) (req.sink)(req.fd);
#else
      // Some distros have not patched tcpd.h
      if (req.sink) ((void (*)(int))req.sink)(req.fd);
#endif
      /*
        The connection was refused by TCP wrappers.
        There are no details (by client IP) available to update the host_cache.
      */
      mysql_socket_shutdown(connect_sock, SHUT_RDWR);
      mysql_socket_close(connect_sock);

      connection_errors_tcpwrap++;
      return NULL;
    }
  }
#endif  // HAVE_LIBWRAP

  Channel_info *channel_info = NULL;
  if (is_unix_socket)
    channel_info = new (std::nothrow) Channel_info_local_socket(connect_sock);
  else
    channel_info = new (std::nothrow) Channel_info_tcpip_socket(connect_sock);
  if (channel_info == NULL) {
    (void)mysql_socket_shutdown(connect_sock, SHUT_RDWR);
    (void)mysql_socket_close(connect_sock);
    connection_errors_internal++;
    return NULL;
  }

  return channel_info;
}

void Mysqld_socket_listener::close_listener() {
  for (socket_map_iterator_t sock_map_iter = m_socket_map.begin();
       sock_map_iter != m_socket_map.end(); ++sock_map_iter) {
    (void)mysql_socket_shutdown(sock_map_iter->first, SHUT_RDWR);
    (void)mysql_socket_close(sock_map_iter->first);
  }

#if defined(HAVE_SYS_UN_H)
  if (m_unix_sockname != "" && m_unlink_sockname) {
    std::string lock_filename = m_unix_sockname + ".lock";
    (void)unlink(lock_filename.c_str());
    (void)unlink(m_unix_sockname.c_str());
  }
#endif

  m_socket_map.clear();
}
