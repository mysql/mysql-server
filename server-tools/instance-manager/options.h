#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_OPTIONS_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_OPTIONS_H
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

/*
  Options - all possible options for the instance manager grouped in one
  struct.
*/
#include <my_global.h>

struct Options
{
#ifdef __WIN__
  static char install_as_service;
  static char remove_service;
  static char stand_alone;
#else
  static char run_as_service;        /* handle_options doesn't support bool */
  static const char *user;
#endif
  static const char *log_file_name;
  static const char *pid_file_name;
  static const char *socket_file_name;
  static const char *password_file_name;
  static const char *default_mysqld_path;
  /* the option which should be passed to process_default_option_files */
  static uint monitoring_interval;
  static uint port_number;
  static const char *bind_address;
  static const char *config_file;

  /* argv pointer returned by load_defaults() to be used by free_defaults() */
  static char **saved_argv;

  int load(int argc, char **argv);
  void cleanup();
#ifdef __WIN__
  int setup_windows_defaults();
#endif
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_OPTIONS_H
