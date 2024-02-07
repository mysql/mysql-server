/* Copyright (c) 2015, 2024, Oracle and/or its affiliates. All rights
reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "xcom/network/xcom_network_provider_native_lib.h"

#include "xcom/site_def.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_transport.h"

#ifdef WIN32
// In OpenSSL before 1.1.0, we need this first.
#include <winsock2.h>
#endif  // WIN32

#ifndef _WIN32
#include <poll.h>
#endif

static inline void dump_error(int err) {
  if (err) {
#ifndef XCOM_WITHOUT_OPENSSL
    if (is_ssl_err(err)) {
      IFDBG(D_BUG, FN; NDBG(from_ssl_err(err), d));
    } else {
#endif
      IFDBG(D_BUG, FN; NDBG(from_errno(err), d); STREXP(strerror(err)));
#ifndef XCOM_WITHOUT_OPENSSL
    }
#endif
  }
}

static xcom_socket_accept_cb xcom_socket_accept_callback = nullptr;
int set_xcom_socket_accept_cb(xcom_socket_accept_cb x) {
  xcom_socket_accept_callback = x;
  return 1;
}

/**
 * @brief Initializes a sockaddr prepared to be used in bind()
 *
 * @param sock_addr struct sockaddr out parameter. You will need to free
 it
 *                  after being used.
 * @param sock_len socklen_t out parameter. It will contain the length of
 *                 sock_addr
 * @param port the port to bind.
 * @param family the address family
 */
void Xcom_network_provider_library::init_server_addr(
    struct sockaddr **sock_addr, socklen_t *sock_len, xcom_port port,
    int family) {
  struct addrinfo *address_info = nullptr, hints, *address_info_loop;
  memset(&hints, 0, sizeof(hints));

  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  checked_getaddrinfo_port(nullptr, port, &hints, &address_info);

  address_info_loop = address_info;
  while (address_info_loop) {
    if (address_info_loop->ai_family == family) {
      if (*sock_addr == nullptr) {
        *sock_addr = (struct sockaddr *)malloc(address_info_loop->ai_addrlen);
      }
      memcpy(*sock_addr, address_info_loop->ai_addr,
             address_info_loop->ai_addrlen);

      *sock_len = address_info_loop->ai_addrlen;

      break;
    }
    address_info_loop = address_info_loop->ai_next;
  }

  if (address_info) freeaddrinfo(address_info);
}

/**
 * Wrapper function which retries and checks errors from socket
 */
result Xcom_network_provider_library::xcom_checked_socket(int domain, int type,
                                                          int protocol) {
  result ret = {0, 0};
  int retry = 1000;
  do {
    SET_OS_ERR(0);
    ret.val = (int)socket(domain, type, protocol);
    ret.funerr = to_errno(GET_OS_ERR);
  } while (--retry && ret.val == -1 && (from_errno(ret.funerr) == SOCK_EAGAIN));
  if (ret.val == -1) {
    task_dump_err(ret.funerr);
  }
  return ret;
}

result Xcom_network_provider_library::create_server_socket() {
  result fd = {0, 0};
  /* Create socket */
  if ((fd = xcom_checked_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)).val < 0) {
    G_MESSAGE(
        "Unable to create socket v6"
        "(socket=%d, errno=%d)!",
        fd.val, to_errno(GET_OS_ERR));
    return fd;
  }
  {
    int reuse = 1;
    SET_OS_ERR(0);
    if (setsockopt(fd.val, SOL_SOCKET, SOCK_OPT_REUSEADDR, (xcom_buf *)&reuse,
                   sizeof(reuse)) < 0) {
      fd.funerr = to_errno(GET_OS_ERR);
      G_MESSAGE(
          "Unable to set socket options "
          "(socket=%d, errno=%d)!",
          fd.val, to_errno(GET_OS_ERR));

      connection_descriptor cd;
      cd.fd = fd.val;
      close_open_connection(&cd);
      return fd;
    }
    /*
     This code sets the acceptor socket as dual-stacked. What happens is
     that we expose the XCom server socket as V6 only, and it will accept
     V4 requests. V4 requests are then represented as IPV4-mapped
     addresses.
    */
    int mode = 0;
    SET_OS_ERR(0);
    if (setsockopt(fd.val, IPPROTO_IPV6, IPV6_V6ONLY, (xcom_buf *)&mode,
                   sizeof(mode)) < 0) {
      fd.funerr = to_errno(GET_OS_ERR);
      G_MESSAGE(
          "Unable to set socket options "
          "(socket=%d, errno=%d)!",
          fd.val, to_errno(GET_OS_ERR));
      connection_descriptor cd;
      cd.fd = fd.val;
      close_open_connection(&cd);
      return fd;
    }
  }
  return fd;
}

/* purecov: begin deadcode */
result Xcom_network_provider_library::create_server_socket_v4() {
  result fd = {0, 0};
  /* Create socket */
  if ((fd = xcom_checked_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)).val < 0) {
    G_MESSAGE(
        "Unable to create socket v4"
        "(socket=%d, errno=%d)!",
        fd.val, to_errno(GET_OS_ERR));
    return fd;
  }
  {
    int reuse = 1;
    SET_OS_ERR(0);
    if (setsockopt(fd.val, SOL_SOCKET, SOCK_OPT_REUSEADDR, (xcom_buf *)&reuse,
                   sizeof(reuse)) < 0) {
      fd.funerr = to_errno(GET_OS_ERR);
      G_MESSAGE(
          "Unable to set socket options "
          "(socket=%d, errno=%d)!",
          fd.val, to_errno(GET_OS_ERR));
      connection_descriptor cd;
      cd.fd = fd.val;
      close_open_connection(&cd);
      return fd;
    }
  }
  return fd;
}
/* purecov: end */

result Xcom_network_provider_library::announce_tcp(xcom_port port) {
  result fd;
  struct sockaddr *sock_addr = nullptr;
  socklen_t sock_addr_len;
  int server_socket_v6_ok = 0;

  // Try and create a V6 server socket. It should succeed if the OS
  // supports IPv6, and fail otherwise.
#ifndef FORCE_IPV4
  fd = create_server_socket();
#else
  /* Force ipv4 for now */
  fd.val = -1;
#endif
  if (fd.val < 0) {
    /* purecov: begin deadcode */
    // If the OS does not support IPv6, we fall back to IPv4.
    fd = create_server_socket_v4();
    if (fd.val < 0) {
      return fd;
    }
    /* purecov: end */
  } else {
    server_socket_v6_ok = 1;
  }
  init_server_addr(&sock_addr, &sock_addr_len, port,
                   server_socket_v6_ok ? AF_INET6 : AF_INET);
  if (sock_addr == nullptr || (bind(fd.val, sock_addr, sock_addr_len) < 0)) {
    // If we fail to bind to the desired address, we fall back to an
    // IPv4 socket.
    /* purecov: begin deadcode */
    fd = create_server_socket_v4();
    if (fd.val < 0) {
      return fd;
    }

    free(sock_addr);
    sock_addr = nullptr;
    init_server_addr(&sock_addr, &sock_addr_len, port, AF_INET);
    if (bind(fd.val, sock_addr, sock_addr_len) < 0) {
      int err = to_errno(GET_OS_ERR);
      G_MESSAGE("Unable to bind to INADDR_ANY:%d (socket=%d, errno=%d)!", port,
                fd.val, err);
      fd.val = -1;
      goto err;
    }
    /* purecov: end */
  }

  G_DEBUG("Successfully bound to %s:%d (socket=%d).", "INADDR_ANY", port,
          fd.val);
  if (listen(fd.val, 32) < 0) {
    G_MESSAGE(
        "Unable to listen backlog to 32. "
        "(socket=%d, errno=%d)!",
        fd.val, to_errno(GET_OS_ERR));
    goto err;
  }
  G_DEBUG("Successfully set listen backlog to 32 (socket=%d)!", fd.val);

  free(sock_addr);
  return fd;

err:
  fd.funerr = to_errno(GET_OS_ERR);
  dump_error(fd.funerr);

  if (fd.val > 0) {
    connection_descriptor cd;
    cd.fd = fd.val;
    close_open_connection(&cd);
  }

  free(sock_addr);

  return fd;
}

int Xcom_network_provider_library::allowlist_socket_accept(
    int fd, site_def const *xcom_config) {
  return xcom_socket_accept_callback != nullptr
             ? xcom_socket_accept_callback(fd, xcom_config)
             : 0;
}

#if defined(_WIN32)
void Xcom_network_provider_library::gcs_shutdown_socket(int *sock) {
  static LPFN_DISCONNECTEX DisconnectEx = nullptr;
  if (DisconnectEx == nullptr) {
    DWORD dwBytesReturned;
    GUID guidDisconnectEx = WSAID_DISCONNECTEX;
    WSAIoctl(*sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidDisconnectEx,
             sizeof(GUID), &DisconnectEx, sizeof(DisconnectEx),
             &dwBytesReturned, nullptr, nullptr);
  }
  if (DisconnectEx != nullptr) {
    DisconnectEx(*sock, (LPOVERLAPPED) nullptr, (DWORD)0, (DWORD)0);
  } else {
    shutdown(*sock, SOCK_SHUT_RDWR);
  }
}

#else

void Xcom_network_provider_library::gcs_shutdown_socket(int *sock) {
  shutdown(*sock, SOCK_SHUT_RD);
  shutdown(*sock, SOCK_SHUT_RW);
}
#endif

result Xcom_network_provider_library::gcs_close_socket(int *sock) {
  result res = {0, 0};
  if (*sock != -1) {
    SET_OS_ERR(0);
    res.val = CLOSESOCKET(*sock);
    res.funerr = to_errno(GET_OS_ERR);
    *sock = -1;
  }
  return res;
}

result Xcom_network_provider_library::gcs_shut_close_socket(int *sock) {
  result res = {0, 0};
  if (*sock >= 0) {
    gcs_shutdown_socket(sock);
    res = gcs_close_socket(sock);
  }
  return res;
}

#define CONNECT_FAIL \
  ret_fd = -1;       \
  goto end

bool Xcom_network_provider_library::poll_for_timed_connects(int fd,
                                                            int timeout) {
  int sysret;
  int syserr;

  struct pollfd fds;
  fds.fd = fd;
  fds.events = POLLOUT;
  fds.revents = 0;

  int poll_timeout = timeout;
#if defined(_WIN32)
  // Windows does not detect connect failures on connect
  // It needs to go to poll, that also does not detect them.
  // Lets add a very shot timeout on Windows to make it easier
  // to detect these situations
  constexpr int first_poll_timeout = 50;
  poll_timeout = first_poll_timeout;
#endif
  while ((sysret = poll(&fds, 1, poll_timeout)) < 0) {
    syserr = GET_OS_ERR;
    if (syserr != SOCK_EINTR && syserr != SOCK_EINPROGRESS) {
      return true;  // Error in poll
    }
#if defined(_WIN32)
    else
      poll_timeout = timeout;
#endif
  }
  SET_OS_ERR(0);

  if (sysret == 0) {
    G_WARNING(
        "Timed out while waiting for a connection via poll to be established! "
        "Cancelling connection attempt. (socket= %d, error=%d)",
        fd, sysret);
    return true;  // We had a poll timeout.
  }

  return Xcom_network_provider_library::verify_poll_errors(fd, sysret, fds);
}

bool Xcom_network_provider_library::verify_poll_errors(int fd, int sysret,
                                                       struct pollfd &fds) {
  if (is_socket_error(sysret)) {
    G_DEBUG(
        "poll - Error while connecting! "
        "(socket= %d, error=%d)",
        fd, GET_OS_ERR);
    return true;
  }

  int socket_errno = 0;
  socklen_t socket_errno_len = sizeof(socket_errno);

  if ((fds.revents & POLLOUT) == 0) {
    return true;
  }

  if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
    return true;
  }
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (xcom_buf *)&socket_errno,
                 &socket_errno_len) != 0) {
    G_DEBUG("getsockopt socket %d failed.", fd);
    return true;
  } else {
    if (socket_errno != 0) {
      G_DEBUG("Connection to socket %d failed with error %d.", fd,
              socket_errno);
      return true;
    }
  }

  return false;
}

int Xcom_network_provider_library::timed_connect_msec(
    int fd, struct sockaddr *sock_addr, socklen_t sock_size, int timeout) {
  int ret_fd = fd;
  int syserr;
  int sysret;

  /* Set non-blocking */
  if (unblock_fd(fd) < 0) return -1;

  /* Trying to connect with timeout */
  SET_OS_ERR(0);
  sysret = connect(fd, sock_addr, sock_size);

  if (is_socket_error(sysret)) {
    syserr = GET_OS_ERR;
    /* If the error is SOCK_EWOULDBLOCK or SOCK_EINPROGRESS or SOCK_EALREADY,
     * wait. */
    switch (syserr) {
      case SOCK_EWOULDBLOCK:
      case SOCK_EINPROGRESS:
      case SOCK_EALREADY:
        break;
      default:
        G_DEBUG(
            "connect - Error connecting "
            "(socket=%d, error=%d).",
            fd, GET_OS_ERR);
        CONNECT_FAIL;
    }
  }

  SET_OS_ERR(0);

  if (Xcom_network_provider_library::poll_for_timed_connects(fd, timeout)) {
    CONNECT_FAIL;
  }

end:
  /* Set blocking */
  SET_OS_ERR(0);
  if (block_fd(fd) < 0) {
    G_DEBUG(
        "Unable to set socket back to blocking state. "
        "(socket=%d, error=%d).",
        fd, GET_OS_ERR);
    return -1;
  }
  return ret_fd;
}

int Xcom_network_provider_library::timed_connect(int fd,
                                                 struct sockaddr *sock_addr,
                                                 socklen_t sock_size) {
  return timed_connect_msec(fd, sock_addr, sock_size, 10000);
}

/* purecov: begin deadcode */
int Xcom_network_provider_library::timed_connect_sec(int fd,
                                                     struct sockaddr *sock_addr,
                                                     socklen_t sock_size,
                                                     int timeout) {
  return timed_connect_msec(fd, sock_addr, sock_size, timeout * 1000);
}
/* purecov: end */

result Xcom_network_provider_library::checked_create_socket(int domain,
                                                            int type,
                                                            int protocol) {
  result retval = {0, 0};
  int nr_attempts = 1005;

  do {
    SET_OS_ERR(0);
    retval.val = (int)socket(domain, type, protocol);
    retval.funerr = to_errno(GET_OS_ERR);
    if (nr_attempts % 10 == 0) xcom_sleep(1);
  } while (--nr_attempts && retval.val == -1 &&
           (from_errno(retval.funerr) == SOCK_EAGAIN));

  if (retval.val == -1) {
    task_dump_err(retval.funerr);
#if defined(_WIN32)
    G_MESSAGE("Socket creation failed with error: %d", retval.funerr);
#else
    G_MESSAGE("Socket creation failed with error %d - %s", retval.funerr,
              strerror(retval.funerr));
#endif
  }
  return retval;
}

/**
  @brief Retrieves a node IPv4 address, if it exists.

  If a node is v4 reachable, means one of two:
  - The raw address is V4
  - a name was resolved to a V4/V6 address

  If the later is the case, we are going to prefer the first v4
  address in the list, since it is the common language between
  old and new version. If you want exclusive V6, please configure your
  DNS server to serve V6 names

  @param retrieved a previously retrieved struct addrinfo
  @return struct addrinfo* An addrinfo of the first IPv4 address. Else it will
                           return the entry parameter.
 */
struct addrinfo *Xcom_network_provider_library::does_node_have_v4_address(
    struct addrinfo *retrieved) {
  struct addrinfo *cycle = nullptr;

  int v4_reachable = is_node_v4_reachable_with_info(retrieved);

  if (v4_reachable) {
    cycle = retrieved;
    while (cycle) {
      if (cycle->ai_family == AF_INET) {
        return cycle;
      }
      cycle = cycle->ai_next;
    }
  }

  /* If something goes really wrong... we fallback to avoid crashes */
  return retrieved;
}
