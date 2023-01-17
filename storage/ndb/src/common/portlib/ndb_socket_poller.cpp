/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include "ndb_global.h"

#include <openssl/ssl.h>

#include "portlib/NdbTick.h"
#include "portlib/ndb_socket.h"
#include "portlib/ndb_socket_poller.h"


bool ndb_socket_poller::set_max_count(unsigned count)
{
  if (count <= m_max_count)
  {
    // Ignore decrease or setting same value
    return true;
  }
  posix_poll_fd* pfds = new posix_poll_fd[count];
  if (pfds == nullptr)
    return false;
  if (m_pfds != &m_one_pfd)
    delete[] m_pfds;
  m_pfds = pfds;
  m_max_count = count;
  return true;
}

unsigned int
ndb_socket_poller::add_readable(ndb_socket_t sock, SSL * ssl)
{
  if(ssl && SSL_pending(ssl))
  {
    assert(m_count < m_max_count);
    const unsigned index = m_count;
    m_ssl_pending++;
    posix_poll_fd &pfd = m_pfds[m_count++];
    pfd.events = 0;        // don't actually poll
    pfd.revents = POLLIN;  // show that socket is ready to read
    return index;
  }

  return add(sock, true, false);
}

unsigned int
ndb_socket_poller::add(ndb_socket_t sock, bool read, bool write)
{
  const unsigned index = m_count;
  assert(m_count < m_max_count);
  posix_poll_fd &pfd = m_pfds[m_count++];
  pfd.fd = ndb_socket_get_native(sock);

  short events = 0;
  if (read)
    events |= POLLIN;
  if (write)
    events |= POLLOUT;

  pfd.events = events;

  pfd.revents = 0;
  assert(m_count > index);
  return index;
}


int
ndb_socket_poller::poll(int timeout)
{
  if(m_ssl_pending > 0 && m_ssl_pending == m_count)
    return m_ssl_pending; // no need to actually poll

  do
  {
    const NDB_TICKS start = NdbTick_getCurrentTicks();

    const int res = poll_unsafe(timeout);
    if (likely(res >= 0))
      return res + m_ssl_pending;
    else if(m_ssl_pending)
      return m_ssl_pending;

    const int error = ndb_socket_errno();
    if (res == -1 && (error == EINTR || error == EAGAIN))
    {
      // Subtract function call time from remaining timeout
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      timeout -= (int)NdbTick_Elapsed(start,now).milliSec();

      if (timeout <= 0) return 0; // Timeout occurred
      continue; // Retry interrupted poll
    }

    return res;

  } while (true);

  abort(); // Never reached
}
