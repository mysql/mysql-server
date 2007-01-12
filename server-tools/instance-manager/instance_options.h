#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_OPTIONS_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_OPTIONS_H
/* Copyright (C) 2004 MySQL AB

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

#include <my_global.h>
#include <my_sys.h>

#include "parse.h"
#include "portability.h" /* for pid_t on Win32 */

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif


/*
  This class contains options of an instance and methods to operate them.

  We do not provide this class with the means of synchronization as it is
  supposed that options for instances are all loaded at once during the
  instance_map initilization and we do not change them later. This way we
  don't have to synchronize between threads.
*/

class Instance_options
{
public:
  /* The operation is used to check if the option is IM-specific or not. */
   static bool is_option_im_specific(const char *option_name);

public:
  Instance_options();
  ~Instance_options();

  bool complete_initialization();

  bool set_option(Named_value *option);
  void unset_option(const char *option_name);

  inline int get_num_options() const;
  inline Named_value get_option(int idx) const;

public:
  bool init(const LEX_STRING *instance_name_arg);
  pid_t load_pid();
  int get_pid_filename(char *result);
  int unlink_pidfile();
  void print_argv();

  uint get_shutdown_delay() const;
  int get_mysqld_port() const;

public:
  /*
    We need this value to be greater or equal then FN_REFLEN found in
    my_global.h to use my_load_path()
  */
  enum { MAX_PATH_LEN= 512 };
  enum { MAX_NUMBER_OF_DEFAULT_OPTIONS= 2 };
  char pid_file_with_path[MAX_PATH_LEN];
  char **argv;
  /*
    Here we cache the version string, obtained from mysqld --version.
    In the case when mysqld binary is not found we get NULL here.
  */
  const char *mysqld_version;
  /* We need the some options, so we store them as a separate pointers */
  const char *mysqld_socket;
  const char *mysqld_datadir;
  const char *mysqld_pid_file;
  LEX_STRING instance_name;
  LEX_STRING mysqld_path;
  LEX_STRING mysqld_real_path;
  const char *nonguarded;
  /* log enums are defined in parse.h */
  char *logs[3];

private:
  bool fill_log_options();
  bool fill_instance_version();
  bool fill_mysqld_real_path();
  int add_to_argv(const char *option);
  int get_default_option(char *result, size_t result_len,
                         const char *option_name);

  void update_var(const char *option_name, const char *option_value);
  int find_option(const char *option_name);

private:
  const char *mysqld_port;
  uint mysqld_port_val;
  const char *shutdown_delay;
  uint shutdown_delay_val;

  uint filled_default_options;
  MEM_ROOT alloc;

  Named_value_arr options;
};


inline int Instance_options::get_num_options() const
{
  return options.get_size();
}


inline Named_value Instance_options::get_option(int idx) const
{
  return options.get_element(idx);
}

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_OPTIONS_H */
