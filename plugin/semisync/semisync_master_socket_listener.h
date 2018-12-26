/* Copyright (c) 2016, 2018 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SEMISYNC_MASTER_SOCKET_LISTENER
#define SEMISYNC_MASTER_SOCKET_LISTENER
#include "semisync_master_ack_receiver.h"

#ifdef HAVE_POLL
#include <sys/poll.h>
#include <vector>

class Poll_socket_listener
{
public:
  Poll_socket_listener()
  {
  }

  bool listen_on_sockets()
  {
    return poll(m_fds.data(), m_fds.size(), 1000 /*1 Second timeout*/);
  }

  bool is_socket_active(int index)
  {
    return m_fds[index].revents & POLLIN;
  }

  void clear_socket_info(int index)
  {
    m_fds[index].fd= -1;
    m_fds[index].events= 0;
  }

  bool init_slave_sockets(Slave_vector slaves)
  {
    m_slaves= slaves;
    m_fds.clear();
    for (uint i= 0; i < m_slaves.size(); i++)
    {
      pollfd poll_fd;
      poll_fd.fd= m_slaves[i].sock_fd();
      poll_fd.events= POLLIN;
      m_fds.push_back(poll_fd);
    }
    return true;
  }
  uint number_of_slave_sockets()
  {
    return m_slaves.size();
  }

  Slave get_slave_obj(int index)
  {
    return m_slaves[index];
  }

private:
  Slave_vector m_slaves;
  std::vector<pollfd> m_fds;
};

typedef class Poll_socket_listener Socket_listener;
#else //NO POLL

class Select_socket_listener
{
public:
  Select_socket_listener()
    :m_max_fd(INVALID_SOCKET)
  {
  }

  bool listen_on_sockets()
  {
    /* Reinitialze the fds with active fds before calling select */
    m_fds= m_init_fds;
    struct timeval tv= {1,0};
    /* select requires max fd + 1 for the first argument */
    return select(m_max_fd+1, &m_fds, NULL, NULL, &tv);
  }

  bool is_socket_active(int index)
  {
    return FD_ISSET(m_slaves[index].sock_fd(), &m_fds);
  }

  void clear_socket_info(int index)
  {
    FD_CLR(m_slaves[index].sock_fd(), &m_init_fds);
  }

  bool init_slave_sockets(Slave_vector slaves)
  {
    m_slaves= slaves;
    FD_ZERO(&m_init_fds);
    for (uint i= 0; i < m_slaves.size(); i++)
    {
      my_socket socket_id= m_slaves[i].sock_fd();
      m_max_fd= (socket_id > m_max_fd ? socket_id : m_max_fd);
#ifndef WINDOWS
      if (socket_id > FD_SETSIZE)
      {
        sql_print_error("Semisync slave socket fd is %u. "
                        "select() cannot handle if the socket fd is "
                        "bigger than %u (FD_SETSIZE).", socket_id, FD_SETSIZE);
        return false;
      }
#endif //WINDOWS
      FD_SET(socket_id, &m_init_fds);
    }
    return true;
  }
  uint number_of_slave_sockets()
  {
    return m_slaves.size();
  }

  Slave get_slave_obj(int index)
  {
    return m_slaves[index];
  }

private:
  Slave_vector m_slaves;
  my_socket m_max_fd;
  fd_set m_init_fds;
  fd_set m_fds;
};

typedef class Select_socket_listener Socket_listener;
#endif //HAVE_POLL
#endif //SEMISYNC_MASTER_SOCKET_LISTENER
