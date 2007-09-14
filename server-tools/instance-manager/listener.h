/* Copyright (C) 2003-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H

#include "thread_registry.h"

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class Thread_registry;
class User_map;

/**
  Listener - a thread listening on sockets and spawning
  connection threads.
*/

class Listener: public Thread
{
public:
  Listener(Thread_registry *thread_registry_arg, User_map *user_map_arg);

protected:
  virtual void run();

private:
  static const int LISTEN_BACK_LOG_SIZE;

private:
  Thread_info thread_info;
  Thread_registry *thread_registry;
  User_map *user_map;

  ulong total_connection_count;

  int sockets[2];
  int num_sockets;
  fd_set read_fds;

private:
  void handle_new_mysql_connection(struct st_vio *vio);
  int create_tcp_socket();
  int create_unix_socket(struct sockaddr_un &unix_socket_address);
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
