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

#ifdef __GNUC__
#pragma implementation
#endif

#include "options.h"

#include <my_global.h>
#include <my_sys.h>
#include <my_getopt.h>

#include "priv.h"

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)

char Options::run_as_service;
const char *Options::log_file_name= QUOTE(DEFAULT_LOG_FILE_NAME);
const char *Options::pid_file_name= QUOTE(DEFAULT_PID_FILE_NAME);
const char *Options::socket_file_name= QUOTE(DEFAULT_SOCKET_FILE_NAME);
const char *Options::password_file_name= QUOTE(DEFAULT_PASSWORD_FILE_NAME);
const char *Options::default_mysqld_path= QUOTE(DEFAULT_MYSQLD_PATH);
const char *Options::default_admin_user= QUOTE(DEFAULT_USER);
const char *Options::default_admin_password= QUOTE(DEFAULT_PASSWORD);
const char *Options::bind_address= 0;              /* No default value */
uint Options::monitoring_interval= DEFAULT_MONITORING_INTERVAL;
uint Options::port_number= DEFAULT_PORT;

/*
  List of options, accepted by the instance manager.
  List must be closed with empty option.
*/

enum options {
  OPT_LOG= 256,
  OPT_PID_FILE,
  OPT_SOCKET,
  OPT_PASSWORD_FILE,
  OPT_MYSQLD_PATH,
  OPT_RUN_AS_SERVICE,
  OPT_USER,
  OPT_PASSWORD,
  OPT_DEFAULT_ADMIN_USER,
  OPT_DEFAULT_ADMIN_PASSWORD,
  OPT_MONITORING_INTERVAL,
  OPT_PORT,
  OPT_BIND_ADDRESS
};

static struct my_option my_long_options[] =
{
  { "help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "log", OPT_LOG, "Path to log file. Used only with --run-as-service.",
    (gptr *) &Options::log_file_name, (gptr *) &Options::log_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "pid-file", OPT_PID_FILE, "Pid file to use.",
    (gptr *) &Options::pid_file_name, (gptr *) &Options::pid_file_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "socket", OPT_SOCKET, "Socket file to use for connection.",
    (gptr *) &Options::socket_file_name, (gptr *) &Options::socket_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "bind_address", OPT_BIND_ADDRESS, "Bind address to use for connection.",
    (gptr *) &Options::bind_address, (gptr *) &Options::bind_address,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "port", OPT_PORT, "Port number to use for connections",
    (gptr *) &Options::port_number, (gptr *) &Options::port_number,
    0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "password-file", OPT_PASSWORD_FILE, "Look for Instane Manager users"
                                        " and passwords here.",
    (gptr *) &Options::password_file_name,
    (gptr *) &Options::password_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "default_mysqld_path", OPT_MYSQLD_PATH, "Where to look for MySQL"
                                            " Server binary.",
    (gptr *) &Options::default_mysqld_path, (gptr *) &Options::default_mysqld_path,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "default_admin_user", OPT_DEFAULT_ADMIN_USER, "Username to shutdown MySQL"
                                           " instances.",
                   (gptr *) &Options::default_admin_user,
                   (gptr *) &Options::default_admin_user,
                   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "default_admin_password", OPT_DEFAULT_ADMIN_PASSWORD, "Password to"
                                            "shutdown MySQL instances.",
                   (gptr *) &Options::default_admin_password,
                   (gptr *) &Options::default_admin_password,
                   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "monitoring_interval", OPT_MONITORING_INTERVAL, "Interval to monitor instances"
                                            " in seconds.",
                   (gptr *) &Options::monitoring_interval,
                   (gptr *) &Options::monitoring_interval,
                   0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "run-as-service", OPT_RUN_AS_SERVICE,
    "Daemonize and start angel process.", (gptr *) &Options::run_as_service,
    0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },

  { "version", 'V', "Output version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void version()
{
  printf("%s Ver %s for %s on %s\n", my_progname, mysqlmanager_version,
         SYSTEM_TYPE, MACHINE_TYPE);
}


static const char *default_groups[]= { "mysql", "manager", 0 };


static void usage()
{
  version();

  printf("Copyright (C) 2003, 2004 MySQL AB\n"
  "This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
  "and you are welcome to modify and redistribute it under the GPL license\n");
  printf("Usage: %s [OPTIONS] \n", my_progname);

  my_print_help(my_long_options);
  print_defaults("my", default_groups);
  my_print_variables(my_long_options);
}

C_MODE_START

static my_bool
get_one_option(int optid,
               const struct my_option *opt __attribute__((unused)),
               char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'V':
    version();
    exit(0);
  case 'I':
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

C_MODE_END


/*
  - call load_defaults to load configuration file section
  - call handle_options to assign defaults and command-line arguments
  to the class members
  if either of these function fail, exit the program
  May not return.
*/

void Options::load(int argc, char **argv)
{
  /* config-file options are prepended to command-line ones */
  load_defaults("my", default_groups, &argc, &argv);

  if (int rc= handle_options(&argc, &argv, my_long_options, get_one_option))
    exit(rc);
}

