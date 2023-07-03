/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SEMISYNC_SOURCE_SOCKET_LISTENER
#define SEMISYNC_SOURCE_SOCKET_LISTENER
#include "plugin/semisync/semisync_source_ack_receiver.h"

#ifdef HAVE_POLL
#include <poll.h>
#include <vector>

class Poll_socket_listener {
 public:
  Poll_socket_listener() = default;

  bool listen_on_sockets() {
    return poll(m_fds.data(), m_fds.size(), 1000 /*1 Second timeout*/);
  }

  bool is_socket_active(int index) { return m_fds[index].revents & POLLIN; }

  void clear_socket_info(int index) {
    m_fds[index].fd = -1;
    m_fds[index].events = 0;
  }

  bool init_replica_sockets(Slave_vector &slaves) {
    m_slaves.clear();
    m_fds.clear();
    for (uint i = 0; i < slaves.size(); i++) {
      /*
        Do not consider the slave's socket
        if the slave is in the process of leaving.
      */
      if (slaves[i].m_status != Slave::EnumStatus::up) {
        slaves[i].m_status = Slave::EnumStatus::down;
        continue;
      }
      pollfd poll_fd;
      poll_fd.fd = slaves[i].sock_fd();
      poll_fd.events = POLLIN;
      poll_fd.revents = 0;
      m_fds.push_back(poll_fd);
      m_slaves.push_back(slaves[i]);
    }
    return true;
  }
  uint number_of_slave_sockets() { return m_slaves.size(); }

  Slave get_slave_obj(int index) { return m_slaves[index]; }

 private:
  Slave_vector m_slaves;
  std::vector<pollfd> m_fds;
};

typedef class Poll_socket_listener Socket_listener;

#else  // NO POLL

class Select_socket_listener {
 public:
  Select_socket_listener() : m_max_fd(INVALID_SOCKET) {}

  bool listen_on_sockets() {
    /* Reinitialze the fds with active fds before calling select */
    m_fds = m_init_fds;
    struct timeval tv = {1, 0};
    /* select requires max fd + 1 for the first argument */
    return select(m_max_fd + 1, &m_fds, NULL, NULL, &tv);
  }

  bool is_socket_active(int index) {
    return FD_ISSET(m_slaves[index].sock_fd(), &m_fds);
  }

  void clear_socket_info(int index) {
    FD_CLR(m_slaves[index].sock_fd(), &m_init_fds);
  }

  bool init_replica_sockets(Slave_vector &slaves) {
    m_slaves.clear();
    FD_ZERO(&m_init_fds);
    for (uint i = 0; i < slaves.size(); i++) {
      /*
        Do not consider the slave's socket
        if the slave is in the process of leaving.
      */
      if (slaves[i].m_status != Slave::EnumStatus::up) {
        slaves[i].m_status = Slave::EnumStatus::down;
        continue;
      }
      my_socket socket_id = slaves[i].sock_fd();
      m_max_fd = (socket_id > m_max_fd ? socket_id : m_max_fd);
#ifndef _WIN32
      if (socket_id > FD_SETSIZE) {
        LogErr(ERROR_LEVEL, ER_SEMISYNC_SOCKET_FD_TOO_LARGE, socket_id,
               FD_SETSIZE);
        return false;
      }
#endif  // _WIN32
      FD_SET(socket_id, &m_init_fds);
      m_slaves.push_back(slaves[i]);
    }
    return true;
  }
  uint number_of_slave_sockets() { return m_slaves.size(); }

  Slave get_slave_obj(int index) { return m_slaves[index]; }

 private:
  Slave_vector m_slaves;
  my_socket m_max_fd;
  fd_set m_init_fds;
  fd_set m_fds;
};

typedef class Select_socket_listener Socket_listener;
#endif  // HAVE_POLL
#endif  // SEMISYNC_SOURCE_SOCKET_LISTENER
