/* Copyright (C) 2000 MySQL AB

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

#include "client_priv.h"
#include <my_dir.h>

#ifdef __WIN__
const char *mysqlcheck_name= "mysqlcheck.exe";
const char *mysql_name= "mysql.exe";
#else
const char *mysqlcheck_name= "mysqlcheck";
const char *mysql_name= "mysql";
#endif /*__WIN__*/

static my_bool opt_force= 0, opt_verbose= 0, tty_password= 0;
static char *user= (char*) "root", *basedir= 0, *datadir= 0, *opt_password= 0;
static my_bool upgrade_defaults_created= 0;
static my_string opt_mysql_port, opt_mysql_unix_port= 0;
static char *default_dbug_option= (char*) "d:t:O,/tmp/comp_err.trace";
static my_bool info_flag= 0;

static struct my_option my_long_options[]=
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Specifies the directory where MySQL is installed",
   (gptr*) &basedir,
   (gptr*) &basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd', "Specifies the data directory", (gptr*) &datadir,
   (gptr*) &datadir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log", (gptr *) & default_dbug_option,
   (gptr *) & default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-info", 'T', "Print some debug info at exit.", (gptr *) & info_flag,
   (gptr *) & info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Continue even if we get an sql-error.",
   (gptr*) &opt_force, (gptr*) &opt_force, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's solicited on the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
   (gptr*) &opt_mysql_port, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"protocol", OPT_MYSQL_PROTOCOL,
   "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", (gptr*) &user,
   (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Display more output about the process", (gptr*) &opt_verbose,
    (gptr *) &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static const char *load_default_groups[]=
{
  "mysql_upgrade", "client", 0
};

#include <help_end.h>

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__ ((unused)),
               char *argument)
{
  switch (optid) {
  case '?':
    puts
      ("MySQL utility script to upgrade database to the current server version");
    puts("");
    my_print_help(my_long_options);
    exit(0);
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
  case 'f':
    opt_force= TRUE;
    break;
  case 'p':
    tty_password= 1;
    if (argument)
    {
      char *start= argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument)
        *argument++= 'x';                       /* Destroy argument */
      if (*start)
        start[1]= 0;                            /* Cut length of argument */
      tty_password= 0;
    }
    break;
  default:;
  };
  return 0;
}


/* buffer should be not smaller than FN_REFLEN */
static my_bool test_file_exists_res(const char *dir, const char *fname,
                                    char *buffer, char **buf_end)
{
  MY_STAT stat_info;

  *buf_end= strxnmov(buffer, FN_REFLEN-1, dir, "/", fname, NullS);
  unpack_filename(buffer, buffer);
  return my_stat(buffer, &stat_info, MYF(0)) != 0;
}


static my_bool test_file_exists(const char *dir, const char *fname)
{
  char path[FN_REFLEN];
  char *path_end;
  return test_file_exists_res(dir, fname, path, &path_end);
}


static int create_check_file(const char *path)
{
  File check_file= my_open(path, O_CREAT | O_WRONLY, MYF(MY_FAE | MY_WME));
  int error;

  if (check_file < 0)
    return 1;

  error= my_write(check_file,
                  MYSQL_SERVER_VERSION, strlen(MYSQL_SERVER_VERSION),
                  MYF(MY_WME | MY_FNABP));
  error= my_close(check_file, MYF(MY_FAE | MY_WME)) || error;
  return error;
}


static int create_defaults_file(const char *path, const char *our_defaults_path)
{
  uint b_read;
  File our_defaults_file, defaults_file;
  char buffer[512];
  char *buffer_end;
  int error;

  /* check if the defaults file is needed at all */
  if (!opt_password)
    return 0;

  defaults_file= my_open(path, O_BINARY | O_CREAT | O_WRONLY,
                         MYF(MY_FAE | MY_WME));

  if (defaults_file < 0)
    return 1;
  upgrade_defaults_created= 1;
  if (our_defaults_path)
  {
    our_defaults_file= my_open(our_defaults_path, O_RDONLY,
                               MYF(MY_FAE | MY_WME));
    if (our_defaults_file < 0)
      return 1;
    do
    {
      if (((b_read= my_read(our_defaults_file, buffer,
                            sizeof(buffer), MYF(MY_WME))) == MY_FILE_ERROR) ||
          my_write(defaults_file, buffer, b_read, MYF(MY_FNABP | MY_WME)))
      {
        error= 1;
        goto close_return;
      }
    } while (b_read == sizeof(buffer));
  }
  buffer_end= strnmov(buffer, "\n[client]", sizeof(buffer));
  if (opt_password)
    buffer_end= strxnmov(buffer, sizeof(buffer),
                         "\npassword=", opt_password, NullS);
  error= my_write(defaults_file, buffer, (int) (buffer_end - buffer),
                  MYF(MY_WME | MY_FNABP));
close_return:
  return my_close(defaults_file, MYF(MY_WME)) || error;
}


int main(int argc, char **argv)
{
  char bindir[FN_REFLEN];
  char *bindir_end, *buf_end;
  char datadir_buf[FN_REFLEN];
  char mysqlcheck_line[FN_REFLEN], *mysqlcheck_end;
  char check_file_name[FN_REFLEN];
  int check_file;
  char fix_priv_tables_cmd[FN_REFLEN], *fix_cmd_end;
  char script_line[FN_REFLEN];
  int error;
  char *forced_defaults_file;
  char *forced_extra_defaults;
  char *defaults_group_suffix;
  char upgrade_defaults_path[FN_REFLEN], *defaults_to_use= 0;
  char port_socket[100], *port_socket_end;

  MY_INIT(argv[0]);
#ifdef __NETWARE__
  setscreenmode(SCR_AUTOCLOSE_ON_EXIT);
#endif

  load_defaults("my", load_default_groups, &argc, &argv);

  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(error);

  if (tty_password)
    opt_password= get_tty_password(NullS);

  /* Check if we want to force the use a specific default file */
  get_defaults_options(argc, argv,
                       &forced_defaults_file, &forced_extra_defaults,
                       &defaults_group_suffix);

  port_socket_end= port_socket;
  if (opt_mysql_port)
    port_socket_end= strxnmov(port_socket, sizeof(port_socket) - 1, " --port=",
                              opt_mysql_port, NullS);
  if (opt_mysql_unix_port)
    port_socket_end= strxnmov(port_socket_end,
                              sizeof(port_socket) -
                                (int)(port_socket_end - port_socket) - 1,
                              " --socket=", opt_mysql_unix_port, NullS);
  *port_socket_end= 0;

  if (basedir)
  {
    bindir_end= strmake(bindir, basedir, sizeof(bindir)-1);
  }
  else
  {
    if (test_file_exists("./share/mysql/english", "errmsg.sys")
        && (test_file_exists("./bin", "mysqld") ||
            test_file_exists("./libexec", "mysqld")))
    {
      my_getwd(bindir, sizeof(bindir), MYF(0));
      bindir_end= bindir + strlen(bindir);
    }
    else
    {
      bindir_end= strmake(bindir, DEFAULT_MYSQL_HOME, sizeof(bindir)-1);
    }
  }

  if (!datadir)
  {
    datadir= datadir_buf;
    if (test_file_exists(bindir, "data/mysql"))
    {
      *strxnmov(datadir_buf, sizeof(datadir_buf)-1, bindir, "/data", NullS)= 0;
    }
    else if (test_file_exists(bindir, "var/mysql"))
    {
      *strxnmov(datadir_buf, sizeof(datadir_buf)-1, bindir, "/var", NullS)= 0;
    }
    else
      datadir= (char*) DATADIR;
  }

  strmake(bindir_end, "/bin", sizeof(bindir) - (int) (bindir_end - bindir)-1);

  if (!test_file_exists_res
      (bindir, mysqlcheck_name, mysqlcheck_line, &mysqlcheck_end))
  {
    printf("Can't find program '%s'\n", mysqlcheck_line);
    puts("Please restart with --basedir=mysql-install-directory");
    exit(1);
  }

  if (!test_file_exists(datadir, "mysql/user.frm"))
  {
    puts
      ("Can't find data directory. Please restart with --datadir=path-to-data-dir");
    exit(1);
  }

  /* create the modified defaults file to be used by mysqlcheck */
  /* and mysql tools                                            */
  *strxnmov(upgrade_defaults_path, sizeof(upgrade_defaults_path)-1,
           datadir, "/upgrade_defaults", NullS)= 0;
  unpack_filename(upgrade_defaults_path, upgrade_defaults_path);
  if ((error=
       create_defaults_file(upgrade_defaults_path, forced_extra_defaults)))
    goto err_exit;

  defaults_to_use= upgrade_defaults_created ?
    upgrade_defaults_path : forced_extra_defaults;

  if (test_file_exists_res(datadir, "mysql_upgrade_info", check_file_name,
                           &buf_end) && !opt_force)
  {
    char chf_buffer[50];
    int b_read;
    check_file= my_open(check_file_name, O_RDONLY, MYF(0));
    b_read= my_read(check_file, chf_buffer, sizeof(chf_buffer)-1, MYF(0));
    chf_buffer[b_read]= 0;
    my_close(check_file, MYF(0));
    if (!strcmp(chf_buffer, MYSQL_SERVER_VERSION))
    {
      if (opt_verbose)
        puts("mysql_upgrade already done for this version");
      goto fix_priv_tables;
    }
  }

  if (defaults_to_use)
  {
    mysqlcheck_end= strxnmov(mysqlcheck_end,
                             sizeof(mysqlcheck_line) - (int) (mysqlcheck_end -
                                                              mysqlcheck_line),
                             " --defaults-extra-file=", defaults_to_use,NullS);
  }

  mysqlcheck_end= strxnmov(mysqlcheck_end,
                           sizeof(mysqlcheck_line) -
                             (int) (mysqlcheck_end - mysqlcheck_line - 1),
                           " --check-upgrade --all-databases --auto-repair --user=",
                           user, port_socket, NullS);
  *mysqlcheck_end= 0;

  if (opt_verbose)
    printf("Running %s\n", mysqlcheck_line);
  if ((error= system(mysqlcheck_line)))
  {
    printf("Error executing '%s'\n", mysqlcheck_line);
    goto err_exit;
  }

  if ((error= create_check_file(check_file_name)))
    goto err_exit;

fix_priv_tables:
  if (!test_file_exists_res(bindir, mysql_name,
                            fix_priv_tables_cmd, &fix_cmd_end))
  {
    puts("Could not find MySQL command-line client (mysql).");
    puts
      ("Please use --basedir to specify the directory where MySQL is installed.");
    error= 1;
    goto err_exit;
  }

  if (!test_file_exists_res(basedir,
                            "support_files/mysql_fix_privilege_tables.sql",
                            script_line, &buf_end)
      && !test_file_exists_res(basedir, "share/mysql_fix_privilege_tables.sql",
                               script_line, &buf_end)
      && !test_file_exists_res(basedir,
                               "share/mysql/mysql_fix_privilege_tables.sql",
                               script_line, &buf_end)
      && !test_file_exists_res(basedir,
                               "scripts/mysql_fix_privilege_tables.sql",
                               script_line, &buf_end)
      && !test_file_exists_res("/usr/local/mysql/share/mysql",
                               "mysql_fix_privilege_tables.sql", script_line,
                               &buf_end))
  {
    puts("Could not find file mysql_fix_privilege_tables.sql");
    puts
      ("Please use --basedir to specify the directory where MySQL is installed");
    error= 1;
    goto err_exit;
  }

  if (defaults_to_use)
  {
    fix_cmd_end= strxnmov(fix_cmd_end,
                          sizeof(fix_priv_tables_cmd) -
                            (int) (fix_cmd_end - fix_priv_tables_cmd - 1),
                          " --defaults-extra-file=", defaults_to_use, NullS);
  }
  fix_cmd_end= strxnmov(fix_cmd_end,
           sizeof(fix_priv_tables_cmd) - (int) (fix_cmd_end -
                                                fix_priv_tables_cmd),
           " --user=", user, port_socket, " mysql < ", script_line, NullS);
  *fix_cmd_end= 0;

  if ((error= system(fix_priv_tables_cmd)))
  {
    /* Problem is that the 'Duplicate column' error           */
    /* which is not a bug for the script makes 'mysql' return */
    /* an error                                               */
    /* printf("Error executing '%s'\n", fix_priv_tables_cmd); */
  }

err_exit:
  if (upgrade_defaults_created)
    my_delete(upgrade_defaults_path, MYF(0));
  my_end(info_flag ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  return error;
}                                               /* main */
