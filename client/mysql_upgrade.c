/* Copyright (C) 2000 MySQL AB

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

#include "client_priv.h"
#include <sslopt-vars.h>
#include "../scripts/mysql_fix_privilege_tables_sql.c"

static char mysql_path[FN_REFLEN];
static char mysqlcheck_path[FN_REFLEN];
static char defaults_file_option[32+FN_REFLEN];

static my_bool opt_force, opt_verbose;
static char *opt_user= (char*)"root";

static DYNAMIC_STRING ds_options;

static char *opt_password= 0;
static my_bool tty_password= 0;

#ifndef DBUG_OFF
static char *default_dbug_option= (char*) "d:t:O,/tmp/mysql_upgrade.trace";
#endif

static char **defaults_argv;

static File defaults_fd= -1;

static my_bool not_used; /* Can't use GET_BOOL without a value pointer */

#include <help_start.h>

static struct my_option my_long_options[]=
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Not used by mysql_upgrade. Only for backward compatibilty",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd',
   "Not used by mysql_upgrade. Only for backward compatibilty",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log", (gptr *) & default_dbug_option,
   (gptr *) & default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", OPT_COMPRESS, "Use compression in server/client protocol.",
   (gptr*)&not_used, (gptr*)&not_used, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Force execution of mysqlcheck even if mysql_upgrade "
   "has already been executed for the current version of MySQL.",
   (gptr*)&opt_force, (gptr*)&opt_force, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host",'h', "Connect to host.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given"
   " it's solicited on the tty.", (gptr*) &opt_password,(gptr*) &opt_password,
   0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
   "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", 'S', "Socket file to use for connection.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", (gptr*) &opt_user,
   (gptr*) &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"verbose", 'v', "Display more output about the process",
   (gptr*) &opt_verbose, (gptr*) &opt_verbose, 0,
   GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

#include <help_end.h>


static void free_used_memory(void)
{
  /* Close the defaults file */
  if (defaults_fd != -1)
    my_close(defaults_fd, MYF(0));

  /* Free memory allocated by 'load_defaults' */
  free_defaults(defaults_argv);

  dynstr_free(&ds_options);
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

  free_used_memory();
  my_end(MY_CHECK_ERROR);
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
  or by defaults file(my.cnf) - to a dynamic string that later
  can be written to a temporary file. In this way we pass the
  same arguments on to mysql and mysql_check
*/

static void add_one_option(DYNAMIC_STRING* ds,
                           const struct my_option *opt,
                           const char* argument)
{
  dynstr_append(ds, opt->name);

  if (opt->arg_type != NO_ARG)
  {
    dynstr_append(ds, "=");
    switch (opt->var_type & GET_TYPE_MASK) {
    case GET_STR:
    case GET_STR_ALLOC:
      dynstr_append(ds, argument);
      break;
    default:
      die("internal error at %s: %d",__FILE__, __LINE__);
    }
  }
  dynstr_append(ds, "\n");
}


static my_bool
get_one_option(int optid, const struct my_option *opt,
               char *argument)
{
  my_bool add_option= TRUE;

  switch (optid) {

  case '?':
    printf("MySQL utility for upgrading database to MySQL version %s\n",
           MYSQL_SERVER_VERSION);
    my_print_help(my_long_options);
    exit(0);
    break;

  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    add_option= FALSE;
    break;

  case 'p':
    tty_password= 1;
    add_option= FALSE;
    if (argument)
    {
      /* Add password to ds_options before overwriting the arg with x's */
      add_one_option(&ds_options, opt, argument);
      while (*argument)
        *argument++= 'x';                       /* Destroy argument */
      tty_password= 0;
    }
    break;

  case 'b': /* --basedir   */
  case 'v': /* --verbose   */
  case 'd': /* --datadir   */
  case 'f': /* --force     */
    add_option= FALSE;
    break;
  }

  if (add_option)
  {
    /*
      This is an option that is accpted by mysql_upgrade just so
      it can be passed on to "mysql" and "mysqlcheck"
      Save it in the ds_options string
    */
    add_one_option(&ds_options, opt, argument);
  }
  return 0;
}


/*
  Write the options that should be passed on to
  mysql and mysqlcheck to a temporary file
*/

static void create_defaults_file(void)
{
  static char defaults_file_path[FN_REFLEN];
  DBUG_ENTER("create_defaults_file");

  if ((defaults_fd= create_temp_file(defaults_file_path, NULL,
                            "cnf", O_CREAT | O_SHARE,
                            MYF(MY_WME))) < 0)
    die("Failed to create temporary file for defaults");

  DBUG_PRINT("info", ("Writing options: %s", ds_options.str));
  if (my_write(defaults_fd, ds_options.str, ds_options.length,
               MYF(MY_FNABP | MY_WME)))
    die("Failed to write to '%s'", defaults_file_path);

  /*
     Dont close the temporary file yet, it will be used
     by mysql and mysqlcheck
  */

  /*
    Create the option that should be added to
    tools in order to use this file
  */
  my_snprintf(defaults_file_option, sizeof(defaults_file_option),
              "--defaults-file=%s", defaults_file_path);
  DBUG_PRINT("info", ("defaults_file_option: %s", defaults_file_option));

  DBUG_VOID_RETURN;
}


static int run_command(char* cmd,
                       DYNAMIC_STRING *ds_res)
{
  char buf[512]= {0};
  FILE *res_file;

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

  return WEXITSTATUS(pclose(res_file));
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

  DBUG_PRINT("info", ("Running: %s", ds_cmdline.str));
  ret= run_command(ds_cmdline.str, ds_res);
  DBUG_PRINT("exit", ("ret: %d", ret));
  dynstr_free(&ds_cmdline);
  DBUG_RETURN(ret);
}


/*
  Try to get the full path to this exceutable

  Return 0 if path found

*/

static my_bool get_full_path_to_executable(char* path)
{
  my_bool ret;
  DBUG_ENTER("get_full_path_to_executable");
#ifdef WIN
  ret= GetModuleFileName(NULL, path, FN_REFLEN) != 0;
#else
  /* my_readlink returns 0 if a symlink was read */
  ret= my_readlink(path, "/proc/self/exe", MYF(0)) != 0;
  /* Might also want to try with /proc/$$/exe if the above fails */
#endif
  DBUG_PRINT("exit", ("path: %s", path));
  DBUG_RETURN(ret);
}


/*
  Look for the tool in the same directory as mysql_upgrade.

  When running in a not yet installed build the the program
  will exist but it need to be invoked via it's libtool wrapper.
  Check if the found tool can executed and if not look in the
  directory one step higher up where the libtool wrapper normally
  is found
*/

static void find_tool(char *tool_path, const char *tool_name)
{
  char path[FN_REFLEN];
  DYNAMIC_STRING ds_tmp;
  DBUG_ENTER("find_tool");
  DBUG_PRINT("enter", ("progname: %s", my_progname));

  if (init_dynamic_string(&ds_tmp, "", 32, 32))
    die("Out of memory");

  /* Initialize path with the full path to this program */
  if (get_full_path_to_executable(path))
  {
    /*
      Easy way to get full executable path failed, try
      other methods
    */
    if (my_progname[0] == FN_LIBCHAR)
    {
      /* 1. my_progname contains full path */
      strmake(path, my_progname, FN_REFLEN);
    }
    else if (my_progname[0] == '.')
    {
      /* 2. my_progname contains relative path, prepend wd */
      char buf[FN_REFLEN];
      my_getwd(buf, FN_REFLEN, MYF(0));
      my_snprintf(path, FN_REFLEN, "%s%s", buf, my_progname);
    }
    else
    {
      /* 3. Just go for it and hope tool is in path */
      path[0]= 0;
    }
  }
  do
  {
    DBUG_PRINT("enter", ("path: %s", path));

    /* Chop off last char(since it might be a /) */
    size_t pos= max((strlen(path)-1), 0);
    path[pos]= 0;

    /* Chop off last dir part */
    dirname_part(path, path);

    /* Format name of the tool to search for */
    fn_format(tool_path, tool_name,
              path, "", MYF(MY_REPLACE_DIR));

    verbose("Looking for '%s' in: %s", tool_name, tool_path);

    /* Make sure the tool exists */
    if (my_access(tool_path, F_OK) != 0)
      die("Can't find '%s'", tool_path);

    /*
      Make sure it can be executed, otherwise try again
      in higher level directory
    */
  }
  while(run_tool(tool_path,
                 &ds_tmp, /* Get output from command, discard*/
                 "--help",
                 "2>&1",
                 IF_WIN("> NUL", "> /dev/null"),
                 NULL));

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
  DBUG_ENTER("run_query");
  DBUG_PRINT("enter", ("query: %s", query));
  if ((fd= create_temp_file(query_file_path, NULL,
                            "sql", O_CREAT | O_SHARE,
                            MYF(MY_WME))) < 0)
    die("Failed to create temporary file for defaults");

  if (my_write(fd, query, strlen(query),
               MYF(MY_FNABP | MY_WME)))
    die("Failed to write to '%s'", query_file_path);

  ret= run_tool(mysql_path,
                ds_res,
                defaults_file_option,
                "--database=mysql",
                "--batch", /* Turns off pager etc. */
                force ? "--force": "--skip-force",
                ds_res ? "--silent": "",
                "<",
                query_file_path,
                "2>&1",
                NULL);

  my_close(fd, MYF(0));

  DBUG_RETURN(ret);
}


/*
  Extract the value returned from result of "show variable like ..."
*/

static int extract_variable_from_show(DYNAMIC_STRING* ds, char* value)
{
  char *value_start, *value_end;
  /*
    The query returns "datadir\t<datadir>\n", skip past
    the tab
  */
  if ((value_start= strchr(ds->str, '\t')) == NULL)
    return 1; /* Unexpected result */
  value_start++;

  /* Don't copy the ending newline */
  if (value_start && (value_end= strchr(value_start, '\n')) == NULL)
    return 1; /* Unexpected result */

  strncpy(value, value_start, min(FN_REFLEN, value_end-value_start));
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

  if (get_upgrade_info_file_name(upgrade_info_file))
    return 0; /* Could not get filename => not sure */

  if (!(in= my_fopen(upgrade_info_file, O_RDONLY, MYF(0))))
    return 0; /* Could not open file => not sure */

  /*
    Read from file, don't care if it fails since it
    will be detected by the strncmp
  */
  bzero(buf, sizeof(buf));
  fgets(buf, sizeof(buf), in);

  my_fclose(in, MYF(0));

  return (strncmp(buf, MYSQL_SERVER_VERSION,
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

    /* Write new version to file, just print a message if it fails */
  if (!fputs(MYSQL_SERVER_VERSION, out))
    fprintf(stderr,
            "Could not write to the upgrade info file '%s' in "
            "the MySQL Servers datadir, errno: %d\n",
            upgrade_info_file, errno);

  my_fclose(out, MYF(0));
}


/*
  Check and upgrade(if neccessary) all tables
  in the server using "mysqlcheck --check-upgrade .."
*/

static int run_mysqlcheck_upgrade(void)
{
  verbose("Running 'mysqlcheck'...");
  return run_tool(mysqlcheck_path,
                  NULL, /* Send output from mysqlcheck directly to screen */
                  defaults_file_option,
                  "--check-upgrade",
                  "--all-databases",
                  "--auto-repair",
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
  DYNAMIC_STRING ds_result;
  DBUG_ENTER("run_sql_fix_privilege_tables");

  if (init_dynamic_string(&ds_result, "", 512, 512))
    die("Out of memory");

  verbose("Running 'mysql_fix_privilege_tables'...");
  run_query(mysql_fix_privilege_tables,
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
    } while ((line= get_line(line)) && *line);
  }

  dynstr_free(&ds_result);
  return found_real_errors;
}


static const char *load_default_groups[]=
{
  "client", /* Read settings how to connect to server */
  "mysql_upgrade", /* Read special settings for mysql_upgrade*/
  0
};


int main(int argc, char **argv)
{
  MY_INIT(argv[0]);
#ifdef __NETWARE__
  setscreenmode(SCR_AUTOCLOSE_ON_EXIT);
#endif

  if (init_dynamic_string(&ds_options, "[client]\n", 512, 256))
    die("Out of memory");

  load_defaults("my", load_default_groups, &argc, &argv);
  defaults_argv= argv; /* Must be freed by 'free_defaults' */

  if (handle_options(&argc, &argv, my_long_options, get_one_option))
    die(NULL);

  if (tty_password)
  {
    opt_password= get_tty_password(NullS);
    /* add password to defaults file */
    dynstr_append(&ds_options, "password=");
    dynstr_append(&ds_options, opt_password);
    dynstr_append(&ds_options, "\n");
  }
  /* add user to defaults file */
  dynstr_append(&ds_options, "user=");
  dynstr_append(&ds_options, opt_user);
  dynstr_append(&ds_options, "\n");

  /* Find mysql */
  find_tool(mysql_path, IF_WIN("mysql.exe", "mysql"));

  /* Find mysqlcheck */
  find_tool(mysqlcheck_path, IF_WIN("mysqlcheck.exe", "mysqlcheck"));

  /*
     Create the defaults file(a small my.cnf) which will pass
     all arguments accepted by mysql_upgrade on to mysqlcheck
     and mysql command line client
   */
  create_defaults_file();

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

  /*
    Run "mysqlcheck" and "mysql_fix_privilege_tables.sql"
  */
  if (run_mysqlcheck_upgrade() ||
      run_sql_fix_privilege_tables())
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
  my_end(MY_CHECK_ERROR);
  exit(0);
}

