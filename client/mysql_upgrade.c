/*
   Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/

#include "client_priv.h"
#include "my_default.h"
#include <sslopt-vars.h>
#include "../scripts/mysql_fix_privilege_tables_sql.c"

#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

#define VER "1.1"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifndef WEXITSTATUS
# ifdef __WIN__
#  define WEXITSTATUS(stat_val) (stat_val)
# else
#  define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
# endif
#endif

static char mysql_path[FN_REFLEN];
static char mysqlcheck_path[FN_REFLEN];

static my_bool opt_force, opt_verbose, debug_info_flag, debug_check_flag,
               opt_systables_only, opt_version_check;
static uint my_end_arg= 0;
static char *opt_user= (char*)"root";

static DYNAMIC_STRING ds_args;
static DYNAMIC_STRING conn_args;

static char *opt_password= 0;
static char *opt_plugin_dir= 0, *opt_default_auth= 0;

static my_bool tty_password= 0;

static char opt_tmpdir[FN_REFLEN] = "";

#ifndef DBUG_OFF
static char *default_dbug_option= (char*) "d:t:O,/tmp/mysql_upgrade.trace";
#endif

static char **defaults_argv;

static my_bool not_used; /* Can't use GET_BOOL without a value pointer */

static my_bool opt_write_binlog;

static struct my_option my_long_options[]=
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Not used by mysql_upgrade. Only for backward compatibility.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", OPT_COMPRESS, "Use compression in server/client protocol.",
   &not_used, &not_used, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd',
   "Not used by mysql_upgrade. Only for backward compatibility.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log.", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", 'T', "Print some debug info at exit.", &debug_info_flag,
   &debug_info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", OPT_DEFAULT_AUTH,
   "Default authentication client-side plugin to use.",
   &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Force execution of mysqlcheck even if mysql_upgrade "
   "has already been executed for the current version of MySQL.",
   &opt_force, &opt_force, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host",'h', "Connect to host.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given,"
   " it's solicited on the tty.", &opt_password,&opt_password,
   0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
   &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
   "The protocol to use for connection (tcp, socket, pipe, memory).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"version-check", 'k', "Run this program only if its \'server version\' "
   "matches the version of the server to which it's connecting, (enabled by "
   "default); use --skip-version-check to avoid this check. Note: the \'server "
   "version\' of the program is the version of the MySQL server with which it "
   "was built/distributed.", &opt_version_check, &opt_version_check, 0,
   GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"tmpdir", 't', "Directory for temporary files.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"upgrade-system-tables", 's', "Only upgrade the system tables "
   "do not try to upgrade the data.",
   &opt_systables_only, &opt_systables_only, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", &opt_user,
   &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Display more output about the process.",
   &opt_verbose, &opt_verbose, 0,
   GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"write-binlog", OPT_WRITE_BINLOG,
   "All commands including mysqlcheck are binlogged. Disabled by default; "
   "use when commands should be sent to replication slaves.",
   &opt_write_binlog, &opt_write_binlog, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void free_used_memory(void)
{
  /* Free memory allocated by 'load_defaults' */
  free_defaults(defaults_argv);

  dynstr_free(&ds_args);
  dynstr_free(&conn_args);
}


static void die(const char *fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");

  /* Print the error message */
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "FATAL ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);

  DBUG_LEAVE;
  free_used_memory();
  my_end(my_end_arg);
  exit(1);
}


static void verbose(const char *fmt, ...)
{
  va_list args;

  if (!opt_verbose)
    return;

  /* Print the verbose message */
  va_start(args, fmt);
  if (fmt)
  {
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);
  }
  va_end(args);
}


/*
  Add one option - passed to mysql_upgrade on command line
  or by defaults file(my.cnf) - to a dynamic string, in
  this way we pass the same arguments on to mysql and mysql_check
*/

static void add_one_option(DYNAMIC_STRING* ds,
                           const struct my_option *opt,
                           const char* argument)

{
  const char* eq= NullS;
  const char* arg= NullS;
  if (opt->arg_type != NO_ARG)
  {
    eq= "=";
    switch (opt->var_type & GET_TYPE_MASK) {
    case GET_STR:
    case GET_PASSWORD:
      arg= argument;
      break;
    case GET_BOOL:
      arg= (*(my_bool *)opt->value) ? "1" : "0";
      break;
    default:
      die("internal error at %s: %d",__FILE__, __LINE__);
    }
  }
  dynstr_append_os_quoted(ds, "--", opt->name, eq, arg, NullS);
  dynstr_append(ds, " ");
}


static my_bool
get_one_option(int optid, const struct my_option *opt,
               char *argument)
{
  my_bool add_option= TRUE;

  switch (optid) {

  case '?':
    printf("%s  Ver %s Distrib %s, for %s (%s)\n",
           my_progname, VER, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
    puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
    puts("MySQL utility for upgrading databases to new MySQL versions.\n");
    my_print_help(my_long_options);
    exit(0);
    break;

  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    add_option= FALSE;
    debug_check_flag= 1;
    break;

  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";			/* Don't require password */
    tty_password= 1;
    add_option= FALSE;
    if (argument)
    {
      /* Add password to ds_args before overwriting the arg with x's */
      add_one_option(&ds_args, opt, argument);
      while (*argument)
        *argument++= 'x';                       /* Destroy argument */
      tty_password= 0;
    }
    break;

  case 't':
    strnmov(opt_tmpdir, argument, sizeof(opt_tmpdir));
    add_option= FALSE;
    break;

  case 'b': /* --basedir   */
  case 'd': /* --datadir   */
    fprintf(stderr, "%s: the '--%s' option is always ignored\n",
            my_progname, optid == 'b' ? "basedir" : "datadir");
    /* FALLTHROUGH */

  case 'k':                                     /* --version-check */
  case 'v': /* --verbose   */
  case 'f': /* --force     */
  case 's':                                     /* --upgrade-system-tables */
  case OPT_WRITE_BINLOG:                        /* --write-binlog */
    add_option= FALSE;
    break;

  case 'h': /* --host */
  case 'W': /* --pipe */
  case 'P': /* --port */
  case 'S': /* --socket */
  case OPT_MYSQL_PROTOCOL: /* --protocol */
  case OPT_SHARED_MEMORY_BASE_NAME: /* --shared-memory-base-name */
  case OPT_PLUGIN_DIR:                          /* --plugin-dir */
  case OPT_DEFAULT_AUTH:                        /* --default-auth */
    add_one_option(&conn_args, opt, argument);
    break;
  }

  if (add_option)
  {
    /*
      This is an option that is accpted by mysql_upgrade just so
      it can be passed on to "mysql" and "mysqlcheck"
      Save it in the ds_args string
    */
    add_one_option(&ds_args, opt, argument);
  }
  return 0;
}


/**
  Run a command using the shell, storing its output in the supplied dynamic
  string.
*/
static int run_command(char* cmd,
                       DYNAMIC_STRING *ds_res)
{
  char buf[512]= {0};
  FILE *res_file;
  int error;

  if (! ds_res)
  {
    fflush(stdout);
    fflush(stderr);
  }
  if (!(res_file= popen(cmd, "r")))
    die("popen(\"%s\", \"r\") failed", cmd);

  while (fgets(buf, sizeof(buf), res_file))
  {
    DBUG_PRINT("info", ("buf: %s", buf));
    if(ds_res)
    {
      /* Save the output of this command in the supplied string */
      dynstr_append(ds_res, buf);
    }
    else
    {
      /* Print it directly on screen */
      fprintf(stdout, "%s", buf);
    }
  }

  if (! ds_res)
  {
    fflush(stdout);
    fflush(stderr);
  }

  error= pclose(res_file);
  return WEXITSTATUS(error);
}


static int run_tool(char *tool_path, DYNAMIC_STRING *ds_res, ...)
{
  int ret;
  const char* arg;
  va_list args;
  DYNAMIC_STRING ds_cmdline;

  DBUG_ENTER("run_tool");
  DBUG_PRINT("enter", ("tool_path: %s", tool_path));

  if (init_dynamic_string(&ds_cmdline, IF_WIN("\"", ""), FN_REFLEN, FN_REFLEN))
    die("Out of memory");

  dynstr_append_os_quoted(&ds_cmdline, tool_path, NullS);
  dynstr_append(&ds_cmdline, " ");

  va_start(args, ds_res);

  while ((arg= va_arg(args, char *)))
  {
    /* Options should be os quoted */
    if (strncmp(arg, "--", 2) == 0)
      dynstr_append_os_quoted(&ds_cmdline, arg, NullS);
    else
      dynstr_append(&ds_cmdline, arg);
    dynstr_append(&ds_cmdline, " ");
  }

  va_end(args);

#ifdef __WIN__
  dynstr_append(&ds_cmdline, "\"");
#endif

  DBUG_PRINT("info", ("Running: %s", ds_cmdline.str));
  ret= run_command(ds_cmdline.str, ds_res);
  DBUG_PRINT("exit", ("ret: %d", ret));
  dynstr_free(&ds_cmdline);
  DBUG_RETURN(ret);
}


/**
  Look for the filename of given tool, with the presumption that it is in the
  same directory as mysql_upgrade and that the same executable-searching 
  mechanism will be used when we run our sub-shells with popen() later.
*/
static void find_tool(char *tool_executable_name, const char *tool_name, 
                      const char *self_name)
{
  char *last_fn_libchar;
  DYNAMIC_STRING ds_tmp;
  DBUG_ENTER("find_tool");
  DBUG_PRINT("enter", ("progname: %s", my_progname));

  if (init_dynamic_string(&ds_tmp, "", 32, 32))
    die("Out of memory");

  last_fn_libchar= strrchr(self_name, FN_LIBCHAR);

  if (last_fn_libchar == NULL)
  {
    /*
      mysql_upgrade was found by the shell searching the path.  A sibling
      next to us should be found the same way.
    */
    strncpy(tool_executable_name, tool_name, FN_REFLEN);
  }
  else
  {
    int len;

    /*
      mysql_upgrade was run absolutely or relatively.  We can find a sibling
      by replacing our name after the LIBCHAR with the new tool name.
    */

    /*
      When running in a not yet installed build and using libtool,
      the program(mysql_upgrade) will be in .libs/ and executed
      through a libtool wrapper in order to use the dynamic libraries
      from this build. The same must be done for the tools(mysql and
      mysqlcheck). Thus if path ends in .libs/, step up one directory
      and execute the tools from there
    */
    if (((last_fn_libchar - 6) >= self_name) &&
        (strncmp(last_fn_libchar - 5, ".libs", 5) == 0) &&
        (*(last_fn_libchar - 6) == FN_LIBCHAR))
    {
      DBUG_PRINT("info", ("Chopping off \".libs\" from end of path"));
      last_fn_libchar -= 6;
    }

    len= last_fn_libchar - self_name;

    my_snprintf(tool_executable_name, FN_REFLEN, "%.*s%c%s",
                len, self_name, FN_LIBCHAR, tool_name);
  }

  verbose("Looking for '%s' as: %s", tool_name, tool_executable_name);

  /*
    Make sure it can be executed
  */
  if (run_tool(tool_executable_name,
               &ds_tmp, /* Get output from command, discard*/
               "--no-defaults",
               "--help",
               "2>&1",
               IF_WIN("> NUL", "> /dev/null"),
               NULL))
    die("Can't execute '%s'", tool_executable_name);

  dynstr_free(&ds_tmp);

  DBUG_VOID_RETURN;
}


/*
  Run query using "mysql"
*/

static int run_query(const char *query, DYNAMIC_STRING *ds_res,
                     my_bool force)
{
  int ret;
  File fd;
  char query_file_path[FN_REFLEN];
  const uchar sql_log_bin[]= "SET SQL_LOG_BIN=0;";

  DBUG_ENTER("run_query");
  DBUG_PRINT("enter", ("query: %s", query));
  if ((fd= create_temp_file(query_file_path, 
                            opt_tmpdir[0] ? opt_tmpdir : NULL,
                            "sql", O_CREAT | O_SHARE | O_RDWR,
                            MYF(MY_WME))) < 0)
    die("Failed to create temporary file for defaults");

  /*
    Master and slave should be upgraded separately. All statements executed
    by mysql_upgrade will not be binlogged.
    'SET SQL_LOG_BIN=0' is executed before any other statements.
   */
  if (!opt_write_binlog)
  {
    if (my_write(fd, sql_log_bin, sizeof(sql_log_bin)-1,
                 MYF(MY_FNABP | MY_WME)))
    {
      my_close(fd, MYF(0));
      my_delete(query_file_path, MYF(0));
      die("Failed to write to '%s'", query_file_path);
    }
  }

  if (my_write(fd, (uchar*) query, strlen(query),
               MYF(MY_FNABP | MY_WME)))
  {
    my_close(fd, MYF(0));
    my_delete(query_file_path, MYF(0));
    die("Failed to write to '%s'", query_file_path);
  }

  ret= run_tool(mysql_path,
                ds_res,
                "--no-defaults",
                ds_args.str,
                "--database=mysql",
                "--batch", /* Turns off pager etc. */
                force ? "--force": "--skip-force",
                ds_res ? "--silent": "",
                "<",
                query_file_path,
                "2>&1",
                NULL);

  my_close(fd, MYF(0));
  my_delete(query_file_path, MYF(0));

  DBUG_RETURN(ret);
}


/*
  Extract the value returned from result of "show variable like ..."
*/

static int extract_variable_from_show(DYNAMIC_STRING* ds, char* value)
{
  char *value_start, *value_end;
  size_t len;

  /*
    The query returns "datadir\t<datadir>\n", skip past
    the tab
  */
  if ((value_start= strchr(ds->str, '\t')) == NULL)
    return 1; /* Unexpected result */
  value_start++;

  /* Don't copy the ending newline */
  if ((value_end= strchr(value_start, '\n')) == NULL)
    return 1; /* Unexpected result */

  len= (size_t) MY_MIN(FN_REFLEN, value_end-value_start);
  strncpy(value, value_start, len);
  value[len]= '\0';
  return 0;
}


static int get_upgrade_info_file_name(char* name)
{
  DYNAMIC_STRING ds_datadir;
  DBUG_ENTER("get_upgrade_info_file_name");

  if (init_dynamic_string(&ds_datadir, NULL, 32, 32))
    die("Out of memory");

  if (run_query("show variables like 'datadir'",
                &ds_datadir, FALSE) ||
      extract_variable_from_show(&ds_datadir, name))
  {
    dynstr_free(&ds_datadir);
    DBUG_RETURN(1); /* Query failed */
  }

  dynstr_free(&ds_datadir);

  fn_format(name, "mysql_upgrade_info", name, "", MYF(0));
  DBUG_PRINT("exit", ("name: %s", name));
  DBUG_RETURN(0);
}


/*
  Read the content of mysql_upgrade_info file and
  compare the version number form file against
  version number wich mysql_upgrade was compiled for

  NOTE
  This is an optimization to avoid running mysql_upgrade
  when it's already been performed for the particular
  version of MySQL.

  In case the MySQL server can't return the upgrade info
  file it's always better to report that the upgrade hasn't
  been performed.

*/

static int upgrade_already_done(void)
{
  FILE *in;
  char upgrade_info_file[FN_REFLEN]= {0};
  char buf[sizeof(MYSQL_SERVER_VERSION)+1];
  char *res;

  if (get_upgrade_info_file_name(upgrade_info_file))
    return 0; /* Could not get filename => not sure */

  if (!(in= my_fopen(upgrade_info_file, O_RDONLY, MYF(0))))
    return 0; /* Could not open file => not sure */

  /*
    Read from file, don't care if it fails since it
    will be detected by the strncmp
  */
  memset(buf, 0, sizeof(buf));
  res= fgets(buf, sizeof(buf), in);

  my_fclose(in, MYF(0));

  if (!res)
    return 0; /* Could not read from file => not sure */

  return (strncmp(res, MYSQL_SERVER_VERSION,
                  sizeof(MYSQL_SERVER_VERSION)-1)==0);
}


/*
  Write mysql_upgrade_info file in servers data dir indicating that
  upgrade has been done for this version

  NOTE
  This might very well fail but since it's just an optimization
  to run mysql_upgrade only when necessary the error can be
  ignored.

*/

static void create_mysql_upgrade_info_file(void)
{
  FILE *out;
  char upgrade_info_file[FN_REFLEN]= {0};

  if (get_upgrade_info_file_name(upgrade_info_file))
    return; /* Could not get filename => skip */

  if (!(out= my_fopen(upgrade_info_file, O_TRUNC | O_WRONLY, MYF(0))))
  {
    fprintf(stderr,
            "Could not create the upgrade info file '%s' in "
            "the MySQL Servers datadir, errno: %d\n",
            upgrade_info_file, errno);
    return;
  }

  /* Write new version to file */
  fputs(MYSQL_SERVER_VERSION, out);
  my_fclose(out, MYF(0));

  /*
    Check if the upgrad_info_file was properly created/updated
    It's not a fatal error -> just print a message if it fails
  */
  if (!upgrade_already_done())
    fprintf(stderr,
            "Could not write to the upgrade info file '%s' in "
            "the MySQL Servers datadir, errno: %d\n",
            upgrade_info_file, errno);
  return;
}


/*
  Print connection-related arguments.
*/

static void print_conn_args(const char *tool_name)
{
  if (conn_args.str[0])
    verbose("Running '%s' with connection arguments: %s", tool_name,
          conn_args.str);
  else
    verbose("Running '%s with default connection arguments", tool_name);
}  


/*
  Check and upgrade(if neccessary) all tables
  in the server using "mysqlcheck --check-upgrade .."
*/

static int run_mysqlcheck_upgrade(void)
{
  print_conn_args("mysqlcheck");
  return run_tool(mysqlcheck_path,
                  NULL, /* Send output from mysqlcheck directly to screen */
                  "--no-defaults",
                  ds_args.str,
                  "--check-upgrade",
                  "--all-databases",
                  "--skip-database=mysql",
                  "--auto-repair",
                  opt_write_binlog ? "--write-binlog" : "--skip-write-binlog",
                  NULL);
}


static int run_mysqlcheck_fixnames(void)
{
  print_conn_args("mysqlcheck");
  return run_tool(mysqlcheck_path,
                  NULL, /* Send output from mysqlcheck directly to screen */
                  "--no-defaults",
                  ds_args.str,
                  "--all-databases",
                  "--skip-database=mysql",
                  "--fix-db-names",
                  "--fix-table-names",
                  opt_write_binlog ? "--write-binlog" : "--skip-write-binlog",
                  NULL);
}

/** performs the same operation as mysqlcheck_upgrade, but on the mysql db */
static int run_mysqlcheck_mysql_db_upgrade(void)
{
  print_conn_args("mysqlcheck");
  return run_tool(mysqlcheck_path,
                  NULL, /* Send output from mysqlcheck directly to screen */
                  "--no-defaults",
                  ds_args.str,
                  "--check-upgrade",
                  "--databases",
                  "--auto-repair",
                  opt_write_binlog ? "--write-binlog" : "--skip-write-binlog",
                  "mysql",
                  NULL);
}


/** performs the same operation as mysqlcheck_upgrade, but on the mysql db */
static int run_mysqlcheck_mysql_db_fixnames(void)
{
  print_conn_args("mysqlcheck");
  return run_tool(mysqlcheck_path,
                  NULL, /* Send output from mysqlcheck directly to screen */
                  "--no-defaults",
                  ds_args.str,
                  "--databases",
                  "--fix-db-names",
                  "--fix-table-names",
                  opt_write_binlog ? "--write-binlog" : "--skip-write-binlog",
                  "mysql",
                  NULL);
}
static const char *expected_errors[]=
{
  "ERROR 1060", /* Duplicate column name */
  "ERROR 1061", /* Duplicate key name */
  "ERROR 1054", /* Unknown column */
  0
};


static my_bool is_expected_error(const char* line)
{
  const char** error= expected_errors;
  while (*error)
  {
    /*
      Check if lines starting with ERROR
      are in the list of expected errors
    */
    if (strncmp(line, "ERROR", 5) != 0 ||
        strncmp(line, *error, strlen(*error)) == 0)
      return 1; /* Found expected error */
    error++;
  }
  return 0;
}


static char* get_line(char* line)
{
  while (*line && *line != '\n')
    line++;
  if (*line)
    line++;
  return line;
}


/* Print the current line to stderr */
static void print_line(char* line)
{
  while (*line && *line != '\n')
  {
    fputc(*line, stderr);
    line++;
  }
  fputc('\n', stderr);
}


/*
  Update all system tables in MySQL Server to current
  version using "mysql" to execute all the SQL commands
  compiled into the mysql_fix_privilege_tables array
*/

static int run_sql_fix_privilege_tables(void)
{
  int found_real_errors= 0;
  const char **query_ptr;
  DYNAMIC_STRING ds_script;
  DYNAMIC_STRING ds_result;
  DBUG_ENTER("run_sql_fix_privilege_tables");

  if (init_dynamic_string(&ds_script, "", 65536, 1024))
    die("Out of memory");

  if (init_dynamic_string(&ds_result, "", 512, 512))
    die("Out of memory");

  verbose("Running 'mysql_fix_privilege_tables'...");

  /*
    Individual queries can not be executed independently by invoking
    a forked mysql client, because the script uses session variables
    and prepared statements.
  */
  for ( query_ptr= &mysql_fix_privilege_tables[0];
        *query_ptr != NULL;
        query_ptr++
      )
  {
    dynstr_append(&ds_script, *query_ptr);
  }

  run_query(ds_script.str,
            &ds_result, /* Collect result */
            TRUE);
  {
    /*
      Scan each line of the result for real errors
      and ignore the expected one(s) like "Duplicate column name",
      "Unknown column" and "Duplicate key name" since they just
      indicate the system tables are already up to date
    */
    char *line= ds_result.str;
    do
    {
      if (!is_expected_error(line))
      {
        /* Something unexpected failed, dump error line to screen */
        found_real_errors++;
        print_line(line);
      }
      else if ((strncmp(line, "WARNING", 7) == 0) ||
               (strncmp(line, "Warning", 7) == 0))
      {
        print_line(line);
      }
    } while ((line= get_line(line)) && *line);
  }

  dynstr_free(&ds_result);
  dynstr_free(&ds_script);
  DBUG_RETURN(found_real_errors);
}


static const char *load_default_groups[]=
{
  "client", /* Read settings how to connect to server */
  "mysql_upgrade", /* Read special settings for mysql_upgrade*/
  0
};


/* Convert the specified version string into the numeric format. */
static ulong STDCALL calc_server_version(char *some_version)
{
  uint major, minor, version;
  char *point= some_version, *end_point;
  major=   (uint) strtoul(point, &end_point, 10);   point=end_point+1;
  minor=   (uint) strtoul(point, &end_point, 10);   point=end_point+1;
  version= (uint) strtoul(point, &end_point, 10);
  return (ulong) major * 10000L + (ulong)(minor * 100 + version);
}

/**
  Check if the server version matches with the server version mysql_upgrade
  was compiled with.

  @return 0 match successful
          1 failed
*/
static int check_version_match(void)
{
  DYNAMIC_STRING ds_version;
  char version_str[NAME_CHAR_LEN + 1];

  if (init_dynamic_string(&ds_version, NULL, NAME_CHAR_LEN, NAME_CHAR_LEN))
    die("Out of memory");

  if (run_query("show variables like 'version'",
                &ds_version, FALSE) ||
      extract_variable_from_show(&ds_version, version_str))
  {
    dynstr_free(&ds_version);
    return 1;                                   /* Query failed */
  }

  dynstr_free(&ds_version);

  if (calc_server_version((char *) version_str) != MYSQL_VERSION_ID)
  {
    fprintf(stderr, "Error: Server version (%s) does not match with the "
            "version of\nthe server (%s) with which this program was built/"
            "distributed. You can\nuse --skip-version-check to skip this "
            "check.\n", version_str, MYSQL_SERVER_VERSION);
    return 1;
  }
  else
    return 0;
}


int main(int argc, char **argv)
{
  char self_name[FN_REFLEN];

  MY_INIT(argv[0]);

#if __WIN__
  if (GetModuleFileName(NULL, self_name, FN_REFLEN) == 0)
#endif
  {
    strncpy(self_name, argv[0], FN_REFLEN);
  }

  if (init_dynamic_string(&ds_args, "", 512, 256) ||
      init_dynamic_string(&conn_args, "", 512, 256))
    die("Out of memory");

  my_getopt_use_args_separator= TRUE;
  if (load_defaults("my", load_default_groups, &argc, &argv))
    die(NULL);
  my_getopt_use_args_separator= FALSE;
  defaults_argv= argv; /* Must be freed by 'free_defaults' */

  if (handle_options(&argc, &argv, my_long_options, get_one_option))
    die(NULL);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;

  if (tty_password)
  {
    opt_password= get_tty_password(NullS);
    /* add password to defaults file */
    dynstr_append_os_quoted(&ds_args, "--password=", opt_password, NullS);
    dynstr_append(&ds_args, " ");
  }
  /* add user to defaults file */
  dynstr_append_os_quoted(&ds_args, "--user=", opt_user, NullS);
  dynstr_append(&ds_args, " ");

  /* Find mysql */
  find_tool(mysql_path, IF_WIN("mysql.exe", "mysql"), self_name);

  if (!opt_systables_only)
  {
    /* Find mysqlcheck */
    find_tool(mysqlcheck_path, IF_WIN("mysqlcheck.exe", "mysqlcheck"), self_name);
  }
  else
  {
    printf("The --upgrade-system-tables option was used, databases won't be touched.\n");
  }

  /*
    Read the mysql_upgrade_info file to check if mysql_upgrade
    already has been run for this installation of MySQL
  */
  if (!opt_force && upgrade_already_done())
  {
    printf("This installation of MySQL is already upgraded to %s, "
           "use --force if you still need to run mysql_upgrade\n",
           MYSQL_SERVER_VERSION);
    die(NULL);
  }

  if (opt_version_check && check_version_match())
    die("Upgrade failed");

  /*
    Run "mysqlcheck" and "mysql_fix_privilege_tables.sql"
    First run mysqlcheck on the system database.
    Then do the upgrade.
    And then run mysqlcheck on all tables.
  */
  if ((!opt_systables_only &&
       (run_mysqlcheck_mysql_db_fixnames() || run_mysqlcheck_mysql_db_upgrade())) ||
      run_sql_fix_privilege_tables() ||
      (!opt_systables_only &&
      (run_mysqlcheck_fixnames() || run_mysqlcheck_upgrade())))
  {
    /*
      The upgrade failed to complete in some way or another,
      significant error message should have been printed to the screen
    */
    die("Upgrade failed" );
  }
  verbose("OK");

  /* Create a file indicating upgrade has been performed */
  create_mysql_upgrade_info_file();

  free_used_memory();
  my_end(my_end_arg);
  exit(0);
}

