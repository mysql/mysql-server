#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
  Thread_info thread_info;
  Thread_registry *thread_registry;
  User_map *user_map;
  static const int LISTEN_BACK_LOG_SIZE= 5;     /* standard backlog size */
  ulong total_connection_count;

  int     sockets[2];
  int     num_sockets;
  fd_set  read_fds;
  void handle_new_mysql_connection(struct st_vio *vio);
  int   create_tcp_socket();
  int   create_unix_socket(struct sockaddr_un &unix_socket_address);
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
