#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_OPTIONS_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_OPTIONS_H
/* Copyright (C) 2004 MySQL AB

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

#include <my_global.h>
#include <my_sys.h>
#include "parse.h"
#include "portability.h"

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

#define USUAL_INSTANCE 0
#define DEFAULT_SINGLE_INSTANCE 1

class Instance_options
{
public:
  Instance_options() :
    mysqld_version(0), mysqld_socket(0), mysqld_datadir(0),
    mysqld_bind_address(0), mysqld_pid_file(0), mysqld_port(0),
    mysqld_port_val(0), mysqld_path(0), nonguarded(0), shutdown_delay(0),
    shutdown_delay_val(0), filled_default_options(0)
  {}
  ~Instance_options();
  /* fills in argv */
  int complete_initialization(const char *default_path, uint instance_type);

  int add_option(const char* option);
  int init(const char *instance_name_arg);
  pid_t get_pid();
  int get_pid_filename(char *result);
  int unlink_pidfile();
  void print_argv();

public:
  /*
    We need this value to be greater or equal then FN_REFLEN found in
    my_global.h to use my_load_path()
  */
  enum { MAX_PATH_LEN= 512 };
  enum { MAX_NUMBER_OF_DEFAULT_OPTIONS= 2 };
  enum { MEM_ROOT_BLOCK_SIZE= 512 };
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
  const char *mysqld_bind_address;
  const char *mysqld_pid_file;
  const char *mysqld_port;
  uint mysqld_port_val;
  const char *instance_name;
  uint instance_name_len;
  const char *mysqld_path;
  uint mysqld_path_len;
  const char *nonguarded;
  const char *shutdown_delay;
  uint shutdown_delay_val;
  /* log enums are defined in parse.h */
  char *logs[3];

  /* this value is computed and cashed here */
  DYNAMIC_ARRAY options_array;
private:
  int fill_log_options();
  int fill_instance_version();
  int add_to_argv(const char *option);
  int get_default_option(char *result, size_t result_len,
                         const char *option_name);
private:
  uint filled_default_options;
  MEM_ROOT alloc;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_INSTANCE_OPTIONS_H */
