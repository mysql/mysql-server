#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_CONNECTION_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_CONNECTION_H
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

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

#include <my_global.h>
#include <my_pthread.h>


C_MODE_START

pthread_handler_decl(mysql_connection, arg);

C_MODE_END


class Thread_registry;
class User_map;
class Instance_map;
struct st_vio;

struct Mysql_connection_thread_args
{
  struct st_vio *vio;
  Thread_registry &thread_registry;
  const User_map &user_map;
  ulong connection_id;
  Instance_map &instance_map;

  Mysql_connection_thread_args(struct st_vio *vio_arg,
                               Thread_registry &thread_registry_arg,
                               const User_map &user_map_arg,
                               ulong connection_id_arg,
                               Instance_map &instance_map_arg);
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_CONNECTION_H
