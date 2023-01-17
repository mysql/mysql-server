/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_SOCKET_POLLER_H
#define NDB_SOCKET_POLLER_H

#include "portlib/ndb_socket.h"

/*
  Portability layer used for waiting on socket events
*/

class ndb_socket_poller {
  // Max number of fds the list can hold, defaults to 1 and
  // can be dynamically expanded by calling 'set_max_count'
  unsigned m_max_count;

  // Current number of fds in the list
  unsigned m_count;

  // Number of sockets with SSL data ready to read when they
  // were added to the list.
  unsigned m_ssl_pending;

  // The list of pollfds, initial size is 1 and m_pfds will
  // then point at m_one_pfd. After dynamic expand points at
  // dynamic list of pollfds
  posix_poll_fd m_one_pfd;
  posix_poll_fd* m_pfds;

public:

  ndb_socket_poller(void) :
    m_max_count(1),
    m_pfds(&m_one_pfd)
  {
    clear();
  }

  void clear(void) {
    m_count = 0;
    m_ssl_pending = 0;
  }

  ~ndb_socket_poller() {
    if (m_pfds != &m_one_pfd)
      delete[] m_pfds;
    }

  bool set_max_count(unsigned count);

  unsigned add(ndb_socket_t sock, bool read, bool write);

  unsigned add_readable(ndb_socket_t sock, struct ssl_st *ssl = nullptr);

  unsigned add_writable(ndb_socket_t sock) {
    return add(sock, false, true);
  }

  unsigned count(void) const { return m_count; }

  bool is_socket_equal(unsigned index, ndb_socket_t socket) const {
    assert(index < m_count);
    return (m_pfds[index].fd == ndb_socket_get_native(socket));
  }

  bool has_read(unsigned index) const {
    assert(index < m_count);
    return (m_pfds[index].revents & (POLLIN | POLLHUP));
  }

  bool has_write(unsigned index) const {
    assert(index < m_count);
    return (m_pfds[index].revents & POLLOUT);
  }

  bool has_hup(unsigned index) const {
    assert(index < m_count);
    return (m_pfds[index].revents & (POLLHUP|POLLERR));
  }

  /*
    Wait for event(s) on socket(s) without retry of interrupted wait
  */
  int poll_unsafe(int timeout) {
    return ndb_poll_sockets(m_pfds, m_count, timeout);
  }

  /*
    Wait for event(s) on socket(s), retry interrupted wait
    if there is still time left
  */
  int poll(int timeout);
};


/*
  ndb_poll
  - Utility function for waiting on events on one socket
    with retry of interrupted wait
*/
static inline
int
ndb_poll(ndb_socket_t sock, bool read, bool write, int timeout_millis)
{
  ndb_socket_poller poller;
  (void)poller.add(sock, read, write);
  return poller.poll(timeout_millis);
}

static inline
bool
Ndb_check_socket_hup(const ndb_socket_t & sock)
{
  ndb_socket_poller poller;
  poller.add_readable(sock);
  if(poller.poll_unsafe(0) > 0 && poller.has_hup(0))
    return true;
  return false;
}


#endif
