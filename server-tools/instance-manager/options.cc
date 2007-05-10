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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "options.h"

#include <my_global.h>
#include <my_sys.h>
#include <my_getopt.h>
#include <mysql_com.h>

#include "exit_codes.h"
#include "log.h"
#include "portability.h"
#include "priv.h"
#include "user_management_commands.h"

#define QUOTE2(x) #x
#define QUOTE(x) QUOTE2(x)

#ifdef __WIN__

/* Define holders for default values. */

static char win_dflt_config_file_name[FN_REFLEN];
static char win_dflt_password_file_name[FN_REFLEN];
static char win_dflt_pid_file_name[FN_REFLEN];

static char win_dflt_mysqld_path[FN_REFLEN];

/* Define and initialize Windows-specific options. */

my_bool Options::Service::install_as_service;
my_bool Options::Service::remove_service;
my_bool Options::Service::stand_alone;

const char *Options::Main::config_file= win_dflt_config_file_name;
const char *Options::Main::password_file_name= win_dflt_password_file_name;
const char *Options::Main::pid_file_name= win_dflt_pid_file_name;

const char *Options::Main::default_mysqld_path= win_dflt_mysqld_path;

static int setup_windows_defaults();

#else /* UNIX */

/* Define and initialize UNIX-specific options. */

my_bool Options::Daemon::run_as_service= FALSE;
const char *Options::Daemon::log_file_name= QUOTE(DEFAULT_LOG_FILE_NAME);
const char *Options::Daemon::user= NULL; /* No default value */
const char *Options::Daemon::angel_pid_file_name= NULL;

const char *Options::Main::config_file= QUOTE(DEFAULT_CONFIG_FILE);
const char *
Options::Main::password_file_name= QUOTE(DEFAULT_PASSWORD_FILE_NAME);
const char *Options::Main::pid_file_name= QUOTE(DEFAULT_PID_FILE_NAME);
const char *Options::Main::socket_file_name= QUOTE(DEFAULT_SOCKET_FILE_NAME);

const char *Options::Main::default_mysqld_path= QUOTE(DEFAULT_MYSQLD_PATH);

#endif

/* Remember if the config file was forced. */

bool Options::Main::is_forced_default_file= FALSE;

/* Define and initialize common options. */

const char *Options::Main::bind_address= NULL; /* No default value */
uint Options::Main::monitoring_interval= DEFAULT_MONITORING_INTERVAL;
uint Options::Main::port_number= DEFAULT_PORT;
my_bool Options::Main::mysqld_safe_compatible= FALSE;

/* Options::User_management */

char *Options::User_management::user_name= NULL;
char *Options::User_management::password= NULL;

User_management_cmd *Options::User_management::cmd= NULL;

/* Private members. */

char **Options::saved_argv= NULL;

#ifndef DBUG_OFF
const char *Options::Debug::config_str= "d:t:i:O,im.trace";
#endif

static const char * const ANGEL_PID_FILE_SUFFIX= ".angel.pid";
static const int ANGEL_PID_FILE_SUFFIX_LEN= strlen(ANGEL_PID_FILE_SUFFIX);

/*
  List of options, accepted by the instance manager.
  List must be closed with empty option.
*/

enum options {
  OPT_USERNAME= 'u',
  OPT_PASSWORD= 'p',
  OPT_LOG= 256,
  OPT_PID_FILE,
  OPT_SOCKET,
  OPT_PASSWORD_FILE,
  OPT_MYSQLD_PATH,
#ifdef __WIN__
  OPT_INSTALL_SERVICE,
  OPT_REMOVE_SERVICE,
  OPT_STAND_ALONE,
#else
  OPT_RUN_AS_SERVICE,
  OPT_USER,
  OPT_ANGEL_PID_FILE,
#endif
  OPT_MONITORING_INTERVAL,
  OPT_PORT,
  OPT_WAIT_TIMEOUT,
  OPT_BIND_ADDRESS,
  OPT_PRINT_PASSWORD_LINE,
  OPT_ADD_USER,
  OPT_DROP_USER,
  OPT_EDIT_USER,
  OPT_CLEAN_PASSWORD_FILE,
  OPT_CHECK_PASSWORD_FILE,
  OPT_LIST_USERS,
  OPT_MYSQLD_SAFE_COMPATIBLE
};

static struct my_option my_long_options[] =
{
  { "help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "add-user", OPT_ADD_USER,
    "Add a user to the password file",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

#ifndef __WIN__
  { "angel-pid-file", OPT_ANGEL_PID_FILE, "Pid file for angel process.",
    (uchar* *) &Options::Daemon::angel_pid_file_name,
    (uchar* *) &Options::Daemon::angel_pid_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif

  { "bind-address", OPT_BIND_ADDRESS, "Bind address to use for connection.",
    (uchar* *) &Options::Main::bind_address,
    (uchar* *) &Options::Main::bind_address,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "check-password-file", OPT_CHECK_PASSWORD_FILE,
    "Check the password file for consistency",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "clean-password-file", OPT_CLEAN_PASSWORD_FILE,
    "Clean the password file",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

#ifndef DBUG_OFF
  {"debug", '#', "Debug log.",
   (uchar* *) &Options::Debug::config_str,
   (uchar* *) &Options::Debug::config_str,
   0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif

  { "default-mysqld-path", OPT_MYSQLD_PATH, "Where to look for MySQL"
    " Server binary.",
    (uchar* *) &Options::Main::default_mysqld_path,
    (uchar* *) &Options::Main::default_mysqld_path,
    0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0 },

  { "drop-user", OPT_DROP_USER,
    "Drop existing user from the password file",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "edit-user", OPT_EDIT_USER,
    "Edit existing user in the password file",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

#ifdef __WIN__
  { "install", OPT_INSTALL_SERVICE, "Install as system service.",
    (uchar* *) &Options::Service::install_as_service,
    (uchar* *) &Options::Service::install_as_service,
    0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
#endif

  { "list-users", OPT_LIST_USERS,
    "Print out a list of registered users",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

#ifndef __WIN__
  { "log", OPT_LOG, "Path to log file. Used only with --run-as-service.",
    (uchar* *) &Options::Daemon::log_file_name,
    (uchar* *) &Options::Daemon::log_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif

  { "monitoring-interval", OPT_MONITORING_INTERVAL, "Interval to monitor"
    " instances in seconds.",
    (uchar* *) &Options::Main::monitoring_interval,
    (uchar* *) &Options::Main::monitoring_interval,
    0, GET_UINT, REQUIRED_ARG, DEFAULT_MONITORING_INTERVAL,
    0, 0, 0, 0, 0 },

  { "mysqld-safe-compatible", OPT_MYSQLD_SAFE_COMPATIBLE,
    "Start Instance Manager in mysqld_safe compatible manner",
    (uchar* *) &Options::Main::mysqld_safe_compatible,
    (uchar* *) &Options::Main::mysqld_safe_compatible,
    0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },

  { "print-password-line", OPT_PRINT_PASSWORD_LINE,
    "Print out a user entry as a line for the password file and exit.",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "password", OPT_PASSWORD, "Password to update the password file",
    (uchar* *) &Options::User_management::password,
    (uchar* *) &Options::User_management::password,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "password-file", OPT_PASSWORD_FILE,
    "Look for Instance Manager users and passwords here.",
    (uchar* *) &Options::Main::password_file_name,
    (uchar* *) &Options::Main::password_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "pid-file", OPT_PID_FILE, "Pid file to use.",
    (uchar* *) &Options::Main::pid_file_name,
    (uchar* *) &Options::Main::pid_file_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "port", OPT_PORT, "Port number to use for connections",
    (uchar* *) &Options::Main::port_number,
    (uchar* *) &Options::Main::port_number,
    0, GET_UINT, REQUIRED_ARG, DEFAULT_PORT, 0, 0, 0, 0, 0 },

#ifdef __WIN__
  { "remove", OPT_REMOVE_SERVICE, "Remove system service.",
    (uchar* *) &Options::Service::remove_service,
    (uchar* *) &Options::Service::remove_service,
    0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0},
#else
  { "run-as-service", OPT_RUN_AS_SERVICE,
    "Daemonize and start angel process.",
    (uchar* *) &Options::Daemon::run_as_service,
    0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0 },
#endif

#ifndef __WIN__
  { "socket", OPT_SOCKET, "Socket file to use for connection.",
    (uchar* *) &Options::Main::socket_file_name,
    (uchar* *) &Options::Main::socket_file_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif

#ifdef __WIN__
  { "standalone", OPT_STAND_ALONE, "Run the application in stand alone mode.",
    (uchar* *) &Options::Service::stand_alone,
    (uchar* *) &Options::Service::stand_alone,
    0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 0, 0},
#else
  { "user", OPT_USER, "Username to start mysqlmanager",
    (uchar* *) &Options::Daemon::user,
    (uchar* *) &Options::Daemon::user,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
#endif

  { "username", OPT_USERNAME,
    "Username to update the password file",
    (uchar* *) &Options::User_management::user_name,
    (uchar* *) &Options::User_management::user_name,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },

  { "version", 'V', "Output version information and exit.", 0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },

  { "wait-timeout", OPT_WAIT_TIMEOUT, "The number of seconds IM waits "
    "for activity on a connection before closing it.",
    (uchar* *) &net_read_timeout,
    (uchar* *) &net_read_timeout,
    0, GET_ULONG, REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0 },

  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

static void version()
{
  printf("%s Ver %s for %s on %s\n", my_progname,
         (const char *) mysqlmanager_version.str,
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
  printf("\nThe following options may be given as the first argument:\n"
  "--print-defaults        Print the program argument list and exit\n"
  "--defaults-file=#       Only read manager configuration and instance\n"
  "                        setings from the given file #. The same file\n"
  "                        will be used to modify configuration of instances\n"
  "                        with SET commands.\n");
  my_print_variables(my_long_options);
}


C_MODE_START

static my_bool
get_one_option(int optid,
               const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  switch(optid) {
  case 'V':
    version();
    exit(0);
  case OPT_PRINT_PASSWORD_LINE:
  case OPT_ADD_USER:
  case OPT_DROP_USER:
  case OPT_EDIT_USER:
  case OPT_CLEAN_PASSWORD_FILE:
  case OPT_CHECK_PASSWORD_FILE:
  case OPT_LIST_USERS:
    if (Options::User_management::cmd)
    {
      fprintf(stderr, "Error: only one password-management command "
              "can be specified at a time.\n");
      exit(ERR_INVALID_USAGE);
    }

    switch (optid) {
    case OPT_PRINT_PASSWORD_LINE:
      Options::User_management::cmd= new Print_password_line_cmd();
      break;
    case OPT_ADD_USER:
      Options::User_management::cmd= new Add_user_cmd();
      break;
    case OPT_DROP_USER:
      Options::User_management::cmd= new Drop_user_cmd();
      break;
    case OPT_EDIT_USER:
      Options::User_management::cmd= new Edit_user_cmd();
      break;
    case OPT_CLEAN_PASSWORD_FILE:
      Options::User_management::cmd= new Clean_db_cmd();
      break;
    case OPT_CHECK_PASSWORD_FILE:
      Options::User_management::cmd= new Check_db_cmd();
      break;
    case OPT_LIST_USERS:
      Options::User_management::cmd= new List_users_cmd();
      break;
    }

    break;
  case '?':
    usage();
    exit(0);
  case '#':
#ifndef DBUG_OFF
    DBUG_SET(argument ? argument : Options::Debug::config_str);
    DBUG_SET_INITIAL(argument ? argument : Options::Debug::config_str);
#endif
    break;
  }
  return 0;
}

C_MODE_END


/*
  - Process argv of original program: get tid of --defaults-extra-file
    and print a message if met there.
  - call load_defaults to load configuration file section and save the pointer
    for free_defaults.
  - call handle_options to assign defaults and command-line arguments
  to the class members.
  if either of these function fail, return the error code.
*/

int Options::load(int argc, char **argv)
{
  if (argc >= 2)
  {
    if (is_prefix(argv[1], "--defaults-file="))
    {
      Main::config_file= strchr(argv[1], '=') + 1;
      Main::is_forced_default_file= TRUE;
    }
    if (is_prefix(argv[1], "--defaults-extra-file=") ||
        is_prefix(argv[1], "--no-defaults"))
    {
      /* the log is not enabled yet */
      fprintf(stderr, "The --defaults-extra-file and --no-defaults options"
              " are not supported by\n"
              "Instance Manager. Program aborted.\n");
      return ERR_INVALID_USAGE;
    }
  }

#ifdef __WIN__
  if (setup_windows_defaults())
  {
    fprintf(stderr, "Internal error: could not setup default values.\n");
    return ERR_OUT_OF_MEMORY;
  }
#endif

  /* load_defaults will reset saved_argv with a new allocated list */
  saved_argv= argv;

  /* config-file options are prepended to command-line ones */

  log_info("Loading config file '%s'...",
           (const char *) Main::config_file);

  load_defaults(Main::config_file, default_groups, &argc, &saved_argv);

  if ((handle_options(&argc, &saved_argv, my_long_options, get_one_option)))
    return ERR_INVALID_USAGE;

  if (!User_management::cmd &&
      (User_management::user_name || User_management::password))
  {
    fprintf(stderr,
            "--username and/or --password options have been specified, "
            "but no password-management command has been given.\n");
    return ERR_INVALID_USAGE;
  }

#ifndef __WIN__
  if (Options::Daemon::run_as_service)
  {
    if (Options::Daemon::angel_pid_file_name == NULL)
    {
      /*
        Calculate angel pid file on the IM pid file basis: replace the
        extension (everything after the last dot) of the pid file basename to
        '.angel.pid'.
      */

      char *local_angel_pid_file_name;
      char *base_name_ptr;
      char *ext_ptr;

      local_angel_pid_file_name=
        (char *) malloc(strlen(Options::Main::pid_file_name) +
                        ANGEL_PID_FILE_SUFFIX_LEN);

      strcpy(local_angel_pid_file_name, Options::Main::pid_file_name);

      base_name_ptr= strrchr(local_angel_pid_file_name, '/');

      if (!base_name_ptr)
        base_name_ptr= local_angel_pid_file_name + 1;

      ext_ptr= strrchr(base_name_ptr, '.');
      if (ext_ptr)
        *ext_ptr= 0;

      strcat(local_angel_pid_file_name, ANGEL_PID_FILE_SUFFIX);

      Options::Daemon::angel_pid_file_name= local_angel_pid_file_name;
    }
    else
    {
      Options::Daemon::angel_pid_file_name=
        strdup(Options::Daemon::angel_pid_file_name);
    }
  }
#endif

  return 0;
}

void Options::cleanup()
{
  if (saved_argv)
    free_defaults(saved_argv);

  delete User_management::cmd;

#ifndef __WIN__
  if (Options::Daemon::run_as_service)
    free((void *) Options::Daemon::angel_pid_file_name);
#endif
}

#ifdef __WIN__

static int setup_windows_defaults()
{
  char module_full_name[FN_REFLEN];
  char dir_name[FN_REFLEN];
  char base_name[FN_REFLEN];
  char im_name[FN_REFLEN];
  char *base_name_ptr;
  char *ptr;

  /* Determine dirname and basename. */

  if (!GetModuleFileName(NULL, module_full_name, sizeof (module_full_name)) ||
      !GetFullPathName(module_full_name, sizeof (dir_name), dir_name,
                       &base_name_ptr))
  {
    return 1;
  }

  strmake(base_name, base_name_ptr, FN_REFLEN);
  *base_name_ptr= 0;

  strmake(im_name, base_name, FN_REFLEN);
  ptr= strrchr(im_name, '.');

  if (!ptr)
     return 1;

  *ptr= 0;

  /* Initialize the defaults. */

  strxmov(win_dflt_config_file_name, dir_name, DFLT_CONFIG_FILE_NAME, NullS);
  strxmov(win_dflt_mysqld_path, dir_name, DFLT_MYSQLD_PATH, NullS);
  strxmov(win_dflt_password_file_name, dir_name, im_name, DFLT_PASSWD_FILE_EXT,
          NullS);
  strxmov(win_dflt_pid_file_name, dir_name, im_name, DFLT_PID_FILE_EXT, NullS);

  return 0;
}

#endif
