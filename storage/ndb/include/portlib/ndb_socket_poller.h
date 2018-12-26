/*
   Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <portlib/NdbTick.h>

/*
  Portability layer used for waiting on socket events
*/

class ndb_socket_poller {
  // Max number of fds the list can hold, defaults to 1 and
  // can be dynamically expanded by calling 'set_max_count'
  unsigned m_max_count;

  // Current number of fds in the list
  unsigned m_count;

#ifdef HAVE_POLL
  // The list of pollfds, initial size is 1 and m_pfds will
  // then point at m_one_pfd. After dynamic expand points at
  // dynamic list of pollfds
  struct pollfd m_one_pfd;
  struct pollfd* m_pfds;
#else
#if defined(_WIN32)
  // Utility functions for dynamically expanding the fd_set
  // on Windows to get around the hardcoded FD_SETSIZE limit.
  static bool
  set_max_count(fd_set* set, fd_set* static_set, unsigned count) {
    void* ptr = malloc(sizeof(fd_set) + count-1*sizeof(SOCKET));
    if (!ptr)
      return false;
    if (set != static_set)
      free(set);
    set = (fd_set*)ptr;
    clear(set);
    return true;
  }

  static void
  set_fd(fd_set* set, SOCKET s) {
    // Avoid use of FD_SET since it silently drop
    // sockets when FD_SETSIZE fd_count is reached
    set->fd_array[set->fd_count++] = s;
  }

  static void
  clear(fd_set* set) {
    FD_ZERO(set);
  }
#endif
  fd_set m_one_read_set;
  fd_set m_one_write_set;
  fd_set m_one_excp_set;
  fd_set* m_read_set;
  fd_set* m_write_set;
  fd_set* m_excp_set;

  // Mapping from "index" to "fd"
  ndb_native_socket_t m_one_fd;
  ndb_native_socket_t* m_fds;

  int m_nfds; // Max fd number for 'select'
#endif

public:

  ndb_socket_poller(void) :
    m_max_count(1)
#ifdef HAVE_POLL
    , m_pfds(&m_one_pfd)
#else
    , m_read_set(&m_one_read_set)
    , m_write_set(&m_one_write_set)
    , m_excp_set(&m_one_excp_set)
    , m_fds(&m_one_fd)
#endif
  {
    clear();
  }

  void clear(void) {
    m_count = 0;
#ifndef HAVE_POLL
    FD_ZERO(m_read_set);
    FD_ZERO(m_write_set);
    FD_ZERO(m_excp_set);
    m_nfds = 0;
#endif
  }

  ~ndb_socket_poller() {
#ifdef HAVE_POLL
    if (m_pfds != &m_one_pfd)
      delete[] m_pfds;
#else
#ifdef _WIN32
    if (m_read_set != &m_one_read_set)
      free(m_read_set);
    if (m_write_set != &m_one_write_set)
      free(m_write_set);
    if (m_excp_set != &m_one_excp_set)
      free(m_excp_set);
#endif
    if (m_fds != &m_one_fd)
      delete[] m_fds;
#endif
    }

  bool set_max_count(unsigned count) {
    if (count <= m_max_count)
    {
      // Ignore decrease or setting same value
      return true;
    }
#ifdef HAVE_POLL
    struct pollfd* pfds = new struct pollfd[count];
    if (pfds == NULL)
      return false;
    if (m_pfds != &m_one_pfd)
      delete[] m_pfds;
    m_pfds = pfds;
#else
#if defined(_WIN32)
    if (count > FD_SETSIZE)
    {
      // Expand the arrays above the builtin FD_SETSIZE
      if (!set_max_count(m_read_set, &m_one_read_set, count) ||
          !set_max_count(m_write_set, &m_one_write_set, count) ||
          !set_max_count(m_excp_set, &m_one_excp_set, count))
        return false;
    }
#endif
    ndb_native_socket_t* fds = new ndb_native_socket_t[count];
    if (fds == NULL)
      return false;
    if (m_fds != &m_one_fd)
      delete[] m_fds;
    m_fds = fds;
#endif
    m_max_count = count;
    return true;
  }

  unsigned add(ndb_socket_t sock, bool read, bool write, bool error) {
    const unsigned index = m_count;
#ifdef HAVE_POLL
    assert(m_count < m_max_count);
    struct pollfd &pfd = m_pfds[m_count++];
    pfd.fd = ndb_socket_get_native(sock);

    short events = 0;
    if (read)
      events |= POLLIN;
    if (write)
      events |= POLLOUT;
    if (error)
      events |= POLLPRI;
    pfd.events = events;

    pfd.revents = 0;
#else
#if defined(_WIN32)
    if (read)
      set_fd(m_read_set, ndb_socket_get_native(sock));
    if (write)
      set_fd(m_write_set, ndb_socket_get_native(sock));
    if (error)
      set_fd(m_excp_set, ndb_socket_get_native(sock));
    // Not counting nfds on Windows since select ignores it anyway
    assert(m_nfds == 0);
#else
    int fd = ndb_socket_get_native(sock);
    if (fd < 0 || fd >= FD_SETSIZE)
    {
      fprintf(stderr, "Maximum value for FD_SETSIZE: %d exceeded when"
        "trying to add fd: %d", FD_SETSIZE, fd);
      fflush(stderr);
      abort();
    }
    if (read)
      FD_SET(fd, m_read_set);
    if (write)
      FD_SET(fd, m_write_set);
    if (error)
      FD_SET(fd, m_excp_set);
    if (fd > m_nfds)
      m_nfds = fd;
#endif
    // Maintain mapping from index to fd
    m_fds[m_count++] = ndb_socket_get_native(sock);
#endif
    assert(m_count > index);
    return index;
  }

  unsigned count(void) const {
    return m_count;
  }

  bool is_socket_equal(unsigned index, ndb_socket_t socket) const {
    assert(index < m_count);
    assert(m_count <= m_max_count);
#ifdef HAVE_POLL
    return (m_pfds[index].fd == ndb_socket_get_native(socket));
#else
    return (m_fds[index] == ndb_socket_get_native(socket));
#endif
  }

  bool has_read(unsigned index) const {
    assert(index < m_count);
    assert(m_count <= m_max_count);
#ifdef HAVE_POLL
    return (m_pfds[index].revents & POLLIN);
#else
    return FD_ISSET(m_fds[index], m_read_set);
#endif
  }

  bool has_write(unsigned index) const {
    assert(index < m_count);
    assert(m_count <= m_max_count);
#ifdef HAVE_POLL
    return (m_pfds[index].revents & POLLOUT);
#else
    return FD_ISSET(m_fds[index], m_write_set);
#endif
  }

  /*
    Wait for event(s) on socket(s) without retry of interrupted wait
  */
  int poll_unsafe(int timeout)
  {
#ifdef HAVE_POLL
    return ::poll(m_pfds, m_count, timeout);
#else

#ifdef _WIN32
    if (m_count == 0)
    {
      // Windows does not sleep on 'select' with 0 sockets
      Sleep(timeout);
      return 0; // Timeout occured
    }
#endif

    struct timeval tv;
    tv.tv_sec  = (timeout / 1000);
    tv.tv_usec = (timeout % 1000) * 1000;

    return select(m_nfds+1, m_read_set, m_write_set, m_excp_set,
                  timeout == -1 ? NULL : &tv);
#endif
  }

  /*
    Wait for event(s) on socket(s), retry interrupted wait
    if there is still time left
  */
  int poll(int timeout)
  {
    do
    {
      const NDB_TICKS start = NdbTick_getCurrentTicks();

      const int res = poll_unsafe(timeout);
      if (likely(res >= 0))
        return res; // Default return path

      const int error = ndb_socket_errno();
      if (res == -1 &&
          (error == EINTR || error == EAGAIN))
      {
        // Retry if any time left of timeout

        // Subtract function call time from remaining timeout
        const NDB_TICKS now = NdbTick_getCurrentTicks();
        timeout -= (int)NdbTick_Elapsed(start,now).milliSec();

        if (timeout <= 0)
          return 0; // Timeout occured

        //fprintf(stderr, "Got interrupted, retrying... timeout left: %d\n",
        //        timeout_millis);

        continue; // Retry interrupted poll
      }

      // Unhandled error code, return it
      return res;

    } while (true);

    abort(); // Never reached
  }

};


/*
  ndb_poll
  - Utility function for waiting on events on one socket
    with retry of interrupted wait
*/

static inline
int
ndb_poll(ndb_socket_t sock,
         bool read, bool write, bool error, int timeout_millis)
{
  ndb_socket_poller poller;
  (void)poller.add(sock, read, write, error);

  const int res = poller.poll(timeout_millis);
  if (res <= 0)
    return res;

  assert(res >= 1);

  return res;
}

#endif
