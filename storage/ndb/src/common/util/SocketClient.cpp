/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

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

#include <ndb_global.h>

#include <cassert>

#include "EventLogger.hpp"
#include "SocketAuthenticator.hpp"
#include "SocketClient.hpp"
#include "portlib/NdbTCP.h"
#include "portlib/ndb_socket_poller.h"

#if 0
#define DEBUG_FPRINTF(arglist) \
  do {                         \
    fprintf arglist;           \
  } while (0)
#define HAVE_DEBUG_FPRINTF 1
#else
#define DEBUG_FPRINTF(a)
#define HAVE_DEBUG_FPRINTF 0
#endif

SocketClient::SocketClient(SocketAuthenticator *sa)
    : m_connect_timeout_millisec(0),  // Blocking connect by default
      m_last_used_port(0),
      m_auth(sa) {
  ndb_socket_initialize(&m_sockfd);
}

SocketClient::~SocketClient() {
  if (ndb_socket_valid(m_sockfd)) ndb_socket_close(m_sockfd);
  if (m_auth) delete m_auth;
}

bool SocketClient::init(int af) {
  assert(!ndb_socket_valid(m_sockfd));
  if (ndb_socket_valid(m_sockfd)) ndb_socket_close(m_sockfd);

  m_sockfd = ndb_socket_create(af);
  if (!ndb_socket_valid(m_sockfd)) {
    return false;
  }
  DBUG_PRINT("info",
             ("NDB_SOCKET: %s", ndb_socket_to_string(m_sockfd).c_str()));
  return true;
}

int SocketClient::bind(ndb_sockaddr local) {
  const bool no_local_port = (local.get_port() == 0);

  if (!ndb_socket_valid(m_sockfd)) return -1;

  {
    // Try to bind to the same port as last successful connect instead of
    // any ephemeral port. Intention is to reuse any previous TIME_WAIT TCB
    local.set_port(m_last_used_port);
  }

  if (ndb_socket_reuseaddr(m_sockfd, true) == -1) {
    int ret = ndb_socket_errno();
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return ret;
  }

  while (ndb_bind(m_sockfd, &local) == -1) {
    if (no_local_port && m_last_used_port != 0) {
      // Failed to bind same port as last, retry with any
      // ephemeral port(as originally requested)
      m_last_used_port = 0;  // Reset last used port
      local.set_port(0);     // Try bind with any port
      continue;
    }

    int ret = ndb_socket_errno();
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return ret;
  }

  return 0;
}

#ifdef _WIN32
#define NONBLOCKERR(E) (E != SOCKET_EAGAIN && E != SOCKET_EWOULDBLOCK)
#else
#define NONBLOCKERR(E) (E != EINPROGRESS)
#endif

NdbSocket SocketClient::connect(ndb_sockaddr server_addr) {
  if (!ndb_socket_valid(m_sockfd)) return {};

  // Reset last used port(in case connect fails)
  m_last_used_port = 0;

  // Set socket non blocking
  if (ndb_socket_nonblock(m_sockfd, true) < 0) {
    DEBUG_FPRINTF((stderr, "Failed to set socket nonblocking in connect\n"));
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return {};
  }

  if (server_addr.need_dual_stack()) {
    [[maybe_unused]] bool ok = ndb_socket_dual_stack(m_sockfd, 1);
  }

  // Start non blocking connect
#if HAVE_DEBUG_FPRINTF
  char server_addrstr[NDB_ADDR_STRLEN];
  Ndb_inet_ntop(&server_addr, server_addrstr, sizeof(server_addrstr));
#endif
  DEBUG_FPRINTF((stderr, "Connect to %s port %d\n", server_addrstr,
                 server_addr.get_port()));
  int r = ndb_connect(m_sockfd, &server_addr);
  if (r == 0) goto done;  // connected immediately.

  if (r < 0 && NONBLOCKERR(ndb_socket_errno())) {
    // Start of non blocking connect failed
    DEBUG_FPRINTF((stderr, "Failed to connect_inet in connect\n"));
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return {};
  }

  if (ndb_poll(m_sockfd, true, true,
               m_connect_timeout_millisec > 0 ? m_connect_timeout_millisec
                                              : -1) <= 0) {
    // Nothing has happened on the socket after timeout
    // or an error occurred
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return {};
  }

  // Activity detected on the socket

  {
    // Check socket level error code
    int so_error = 0;
    if (ndb_getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &so_error) < 0) {
      DEBUG_FPRINTF((stderr, "Failed to set sockopt in connect\n"));
      ndb_socket_close(m_sockfd);
      ndb_socket_invalidate(&m_sockfd);
      return {};
    }

    if (so_error) {
      DEBUG_FPRINTF((stderr, "so_error: %d in connect\n", so_error));
      ndb_socket_close(m_sockfd);
      ndb_socket_invalidate(&m_sockfd);
      return {};
    }
  }

done:
  if (ndb_socket_nonblock(m_sockfd, false) < 0) {
    DEBUG_FPRINTF((stderr, "ndb_socket_nonblock failed in connect\n"));
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return {};
  }

  // Remember the local port used for this connection
  assert(m_last_used_port == 0);
  ndb_socket_get_port(m_sockfd, &m_last_used_port);

  // Transfer the fd to the NdbSocket
  NdbSocket secureSocket{m_sockfd};
  ndb_socket_invalidate(&m_sockfd);
  return secureSocket;
}

int SocketClient::authenticate(const NdbSocket &secureSocket) {
  assert(m_auth);
  int r = m_auth->client_authenticate(secureSocket);
  if (r < SocketAuthenticator::AuthOk) {
    if (r != SocketAuthenticator::negotiation_failed) {
      g_eventLogger->error("Socket authentication failed: %s\n",
                           m_auth->error(r));
    }
    secureSocket.shutdown();  // Make it unusable, caller should close
  }
  return r;
}
