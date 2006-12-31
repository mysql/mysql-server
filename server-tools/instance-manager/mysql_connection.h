/* Copyright (C) 2004-2006 MySQL AB

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

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_CONNECTION_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_CONNECTION_H

#include "thread_registry.h"
#include <mysql_com.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

struct st_vio;
class User_map;

/*
  MySQL connection - handle one connection with mysql command line client
  See also comments in mysqlmanager.cc to picture general Instance Manager
  architecture.
  We use conventional technique to work with classes without exceptions:
  class acquires all vital resource in init(); Thus if init() succeed,
  a user must call cleanup(). All other methods are valid only between
  init() and cleanup().
*/

class Mysql_connection: public Thread
{
public:
  Mysql_connection(Thread_registry *thread_registry_arg,
                   User_map *user_map_arg,
                   struct st_vio *vio_arg,
                   ulong connection_id_arg);
  virtual ~Mysql_connection();

protected:
  virtual void run();

private:
  struct st_vio *vio;
  ulong connection_id;
  Thread_info thread_info;
  Thread_registry *thread_registry;
  User_map *user_map;
  NET net;
  struct rand_struct rand_st;
  char scramble[SCRAMBLE_LENGTH + 1];
  uint status;
  ulong client_capabilities;
private:
  /* The main loop implementation triad */
  bool init();
  void main();
  void cleanup();

  /* Names are conventionally the same as in mysqld */
  int check_connection();
  int do_command();
  int dispatch_command(enum enum_server_command command, const char *text);
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_CONNECTION_H
