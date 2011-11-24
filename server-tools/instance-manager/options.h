/*
   Copyright (c) 2003-2006 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_OPTIONS_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_OPTIONS_H

/*
  Options - all possible command-line options for the Instance Manager grouped
  in one struct.
*/

#include <my_global.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class User_management_cmd;

struct Options
{
  /*
    NOTE: handle_options() expects value of my_bool type for GET_BOOL
    accessor (i.e. bool must not be used).
  */

  struct User_management
  {
    static User_management_cmd *cmd;

    static char *user_name;
    static char *password;
  };

  struct Main
  {
    /* this is not an option parsed by handle_options(). */
    static bool is_forced_default_file;

    static const char *pid_file_name;
#ifndef __WIN__
    static const char *socket_file_name;
#endif
    static const char *password_file_name;
    static const char *default_mysqld_path;
    static uint monitoring_interval;
    static uint port_number;
    static const char *bind_address;
    static const char *config_file;
    static my_bool mysqld_safe_compatible;
  };

#ifndef DBUG_OFF
  struct Debug
  {
    static const char *config_str;
  };
#endif

#ifndef __WIN__

  struct Daemon
  {
    static my_bool run_as_service;
    static const char *log_file_name;
    static const char *user;
    static const char *angel_pid_file_name;
  };

#else

  struct Service
  {
    static my_bool install_as_service;
    static my_bool remove_service;
    static my_bool stand_alone;
  };

#endif

public:
  /* Array of paths to be passed to my_search_option_files() later */
  static const char **default_directories;

  static int load(int argc, char **argv);
  static void cleanup();

private:
  Options(); /* Deny instantiation of this class. */

private:
  /* argv pointer returned by load_defaults() to be used by free_defaults() */
  static char **saved_argv;
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_OPTIONS_H
