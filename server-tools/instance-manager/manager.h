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

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_MANAGER_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_MANAGER_H

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

#include <my_global.h>

class Guardian;
class Instance_map;
class Thread_registry;
class User_map;

class Manager
{
public:
  static int main();

  static int flush_instances();

public:
  /**
    These methods return a non-NULL value only for the duration
    of main().
  */
  static Instance_map *get_instance_map() { return p_instance_map; }
  static Guardian *get_guardian() { return p_guardian; }
  static Thread_registry *get_thread_registry() { return p_thread_registry; }
  static User_map *get_user_map() { return p_user_map; }

public:
#ifndef __WIN__
  static bool is_linux_threads() { return linux_threads; }
#endif // __WIN__

private:
  static void stop_all_threads();
  static bool init_user_map(User_map *user_map);

private:
  static Guardian *p_guardian;
  static Instance_map *p_instance_map;
  static Thread_registry *p_thread_registry;
  static User_map *p_user_map;

#ifndef __WIN__
  /*
    This flag is set if Instance Manager is running on the system using
    LinuxThreads.
  */
  static bool linux_threads;
#endif // __WIN__
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_MANAGER_H
