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

#include "priv.h"
#include "port.h"
#include <my_sys.h>
#include <my_getopt.h>
#include <m_string.h>
#include <mysql_com.h>

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)

const char *default_password_file_name = QUOTE(DEFAULT_PASSWORD_FILE_NAME);
const char *default_log_file_name = QUOTE(DEFAULT_LOG_FILE_NAME);
char default_config_file[FN_REFLEN] = "/etc/my.cnf";

#ifndef __WIN__
char Options::run_as_service;
const char *Options::user= 0;                   /* No default value */
#else
char Options::install_as_service;
char Options::remove_service;
#endif
const char *Options::log_file_name= default_log_file_name;
const char *Options::pid_file_name= QUOTE(DEFAULT_PID_FILE_NAME);
const char *Options::socket_file_name= QUOTE(DEFAULT_SOCKET_FILE_NAME);
const char *Options::password_file_name= default_password_file_name;
const char *Options::default_mysqld_path= QUOTE(DEFAULT_MYSQLD_PATH);
const char *Options::first_option= 0;           /* No default value */
const char *Options::bind_address= 0;           /* No default value */
uint Options::monitoring_interval= DEFAULT_MONITORING_INTERVAL;
uint Options::port_number= DEFAULT_PORT;
/* just to declare */
char **Options::saved_argv;
const char *Options::config_file = NULL;

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
#ifndef __WIN__
  OPT_RUN_AS_SERVICE,
  OPT_USER,
#else
  OPT_INSTALL_SERVICE,
  OPT_REMOVE_SERVICE,
#endif
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

  { "passwd", 'P', "Prepare entry for passwd file and exit.", 0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "bind-address", OPT_BIND_ADDRESS, "Bind address to use for connection.",
    (gptr *) &Options::bind_address, (gptr *) &Options::bind_address,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "port", OPT_PORT, "Port number to use for connections",
    (gptr *) &Options::port_number, (gptr *) &Options::port_number,
    0, GET_UINT, REQUIRED_ARG, DEFAULT_PORT, 0, 0, 0, 0, 0 },

  { "password-file", OPT_PASSWORD_FILE, "Look for Instane Manager users"
                                        " and passwords here.",
    (gptr *) &Options::password_file_name,
    (gptr *) &Options::password_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "default-mysqld-path", OPT_MYSQLD_PATH, "Where to look for MySQL"
                                            " Server binary.",
    (gptr *) &Options::default_mysqld_path, (gptr *) &Options::default_mysqld_path,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "monitoring-interval", OPT_MONITORING_INTERVAL, "Interval to monitor instances"
                                            " in seconds.",
                   (gptr *) &Options::monitoring_interval,
                   (gptr *) &Options::monitoring_interval,
                   0, GET_UINT, REQUIRED_ARG, DEFAULT_MONITORING_INTERVAL,
                   0, 0, 0, 0, 0 },
#ifdef __WIN__
  { "install", OPT_INSTALL_SERVICE, "Install as system service.", 
    (gptr *) &Options::install_as_service, (gptr*) &Options::install_as_service, 
    0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
  { "remove", OPT_REMOVE_SERVICE, "Remove system service.", 
  (gptr *)&Options::remove_service, (gptr*) &Options::remove_service, 
    0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0},
#else
  { "run-as-service", OPT_RUN_AS_SERVICE,
    "Daemonize and start angel process.", (gptr *) &Options::run_as_service,
    0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },

  { "user", OPT_USER, "Username to start mysqlmanager",
                   (gptr *) &Options::user,
                   (gptr *) &Options::user,
                   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif
  { "version", 'V', "Output version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void version()
{
  printf("%s Ver %s for %s on %s\n", my_progname, mysqlmanager_version,
         SYSTEM_TYPE, MACHINE_TYPE);
}


static const char *default_groups[]= { "manager", 0 };


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


static void passwd()
{
  char user[1024], *p;
  const char *pw1, *pw2;
  char pw1msg[]= "Enter password: ";
  char pw2msg[]= "Re-type password: ";
  char crypted_pw[SCRAMBLED_PASSWORD_CHAR_LENGTH + 1];

  fprintf(stderr, "Creating record for new user.\n");
  fprintf(stderr, "Enter user name: ");
  if (!fgets(user, sizeof(user), stdin))
  {
    fprintf(stderr, "Unable to read user.\n");
    return;
  }
  if ((p= strchr(user, '\n'))) *p= 0;

  pw1= get_tty_password(pw1msg);
  pw2= get_tty_password(pw2msg);

  if (strcmp(pw1, pw2))
  {
    fprintf(stderr, "Sorry, passwords do not match.\n");
    return;
  }

  make_scrambled_password(crypted_pw, pw1);
  printf("%s:%s\n", user, crypted_pw);
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
  case 'P':
    passwd();
    exit(0);
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

int Options::load(int argc, char **argv)
{
  int rc;
  char** argv_ptr = argv;

#ifdef __WIN__
  setup_windows_defaults(*argv);
#endif

  config_file=NULL;
  if (argc >= 2)
  {
	  if (is_prefix(argv[1], "--defaults-file="))
		  config_file=argv[1];
    if (is_prefix(argv[1],"--defaults-file=") ||
        is_prefix(argv[1],"--defaults-extra-file="))
      Options::first_option= argv[1];
  }

	// we were not given a config file on the command line so we
  // set have to construct a new argv array
	if (config_file == NULL)
	{
#ifdef __WIN__
    ::GetModuleFileName(NULL, default_config_file, sizeof(default_config_file));
    char *filename = strstr(default_config_file, "mysqlmanager.exe");
    strcpy(filename, "my.ini");
#endif
    config_file = default_config_file;
	}

  /* config-file options are prepended to command-line ones */
  load_defaults(config_file, default_groups, &argc, &argv);

  rc= handle_options(&argc, &argv, my_long_options, get_one_option);

  if (rc != 0)
    return rc;

  Options::saved_argv= argv;

  return 0;
}

void Options::cleanup()
{
  /* free_defaults returns nothing */
  free_defaults(Options::saved_argv);

#ifdef __WIN__
  free((char*)default_password_file_name);
#endif
}

#ifdef __WIN__

char* change_extension(const char *src, const char *newext)
{
  char *dot = (char*)strrchr(src, '.');
  if (!dot) return (char*)src;
  
  int newlen = dot-src+strlen(newext)+1;
  char *temp = (char*)malloc(newlen);
  bzero(temp, newlen);
  strncpy(temp, src, dot-src+1);
  strcat(temp, newext);
  return temp;
}

void Options::setup_windows_defaults(const char *progname)
{
  Options::password_file_name = default_password_file_name = change_extension(progname, "passwd");
  Options::log_file_name = default_log_file_name = change_extension(progname, "log");
}

#endif
