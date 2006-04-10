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

#include <stdlib.h>
#include "client_priv.h"
#include <my_dir.h>

static my_bool opt_force=0, opt_verbose=0;
static char *user= (char*)"root", *basedir=0, *datadir=0;

static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Specifies the directory where MySQL is installed", (gptr*) &basedir,
   (gptr*) &basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd', "Specifies the data directory", (gptr*) &datadir,
   (gptr*) &datadir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Continue even if we get an sql-error.",
   (gptr*) &opt_force, (gptr*) &opt_force, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", (gptr*) &user,
   (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Display more output about the process", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static const char *load_default_groups[] = { "mysql_upgrade", "client", 0 };

#include <help_end.h>

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch(optid) {
  case '?':
    puts("MySQL utility script to upgrade database to the current server version\n");
    puts("\n");
    my_print_help(my_long_options);
    exit(0);
  case 'f':
    opt_force= TRUE;
    break;
  case 'v':
    opt_verbose= TRUE;
    break;
  default:;
  };
  return 0;
}


static my_bool test_file_exists_res(const char *dir, const char *fname, char *buffer)
{
  MY_STAT stat_info;

  if (fname)
    sprintf(buffer, "%s/%s", dir, fname);
  else
    strcpy(buffer, dir);
  return my_stat(buffer, &stat_info, MYF(0)) != 0;
}


static my_bool test_file_exists(const char *dir, const char *fname)
{
  char path[1024];
  return test_file_exists_res(dir, fname, path);
}


int main(int argc, char **argv)
{
  char bindir[1024];
  char datadir_buf[1024];
  char mysqlcheck_line[1024];
  char check_file_name[1024];
  int check_file;
  char fix_priv_tables_cmd[1024];
  char script_line[1024];
  int error;
#ifdef __NETWARE__
  setscreenmode(SCR_AUTOCLOSE_ON_EXIT);
#endif

  load_defaults("my", load_default_groups, &argc, &argv);

  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(error);

  if (!basedir)
  {
    if (test_file_exists("./share/mysql/english", "errmsg.sys")
        && (test_file_exists("./bin", "mysqld") ||
            test_file_exists("./libexec", "mysqld")))
    {
      getcwd(bindir, sizeof(bindir));
    }
    else
    {
      strcpy(bindir, DEFAULT_MYSQL_HOME);
    }
  }
  else
  {
    strcpy(bindir, basedir);
  }

  if (!datadir)
  {
    datadir= datadir_buf;
    if (test_file_exists(bindir,"data/mysql"))
    {
      sprintf(datadir, "%s/data", bindir);
    }
    else if (test_file_exists(bindir, "var/mysql"))
    {
      sprintf(datadir, "%s/var", bindir);
    }
    else
      strcpy(datadir, DATADIR);
  }

  strcat(bindir, "/bin");

  if (!test_file_exists(bindir, "mysqlcheck"))
  {
    printf("Can't find program '%s/mysqlcheck'\n", bindir);
    puts("Please restart with --basedir=mysql-install-directory\n");
    exit(1);
  }

  if (!test_file_exists(datadir, "mysql/user.frm"))
  {
    puts("Can't find data directory. Please restart with --datadir=path-to-data-dir\n");
    exit(1);
  }

  if (test_file_exists_res(datadir, "mysql_upgrade_info", check_file_name) && !opt_force)
  {
    char chf_buffer[10];
    int b_read;
    check_file= open(check_file_name, O_RDONLY);
    b_read= read(check_file, chf_buffer, sizeof(chf_buffer));
    chf_buffer[b_read]= 0;
    if (strcmp(chf_buffer, VERSION))
    {
      if (opt_verbose)
        puts("mysql_upgrade already done for this version\n");
      goto fix_priv_tables;
    }
    close(check_file);
  }

  sprintf(mysqlcheck_line,
          "%s/mysqlcheck --check-upgrade --all-databases --auto-repair --user=%s",
          bindir, user);
  if (opt_verbose)
    printf("Running %s\n", mysqlcheck_line);
  system(mysqlcheck_line);

  check_file= open(check_file_name, O_CREAT | O_WRONLY);
  write(check_file, VERSION, strlen(VERSION));
  close(check_file);

fix_priv_tables:
  if (!test_file_exists(bindir, "mysql"))
  {
    puts("Could not find MySQL command-line client (mysql).\n");
    puts("Please use --basedir to specify the directory where MySQL is installed.\n");
    exit(1);
  }

  if (!test_file_exists_res(basedir,
        "support_files/mysql_fix_privilege_tables.sql", script_line) &&
      !test_file_exists_res(basedir,
        "share/mysql_fix_privileges_tables.sql", script_line) &&
      !test_file_exists_res(basedir,
        "share/mysql/mysql_fix_privilege_tables.sql", script_line) &&
      !test_file_exists_res(basedir,
        "scripts/mysql_fix_privilege_tables.sql", script_line) &&
      !test_file_exists_res("/usr/local/mysql/share/mysql",
        "mysql_fix_privilege_tables.sql", script_line))
  {
    puts("Could not find file mysql_fix_privilege_tables.sql\n");
    puts("Please use --basedir to specify the directory where MySQL is installed\n");
    exit(1);
  }
    
  sprintf(fix_priv_tables_cmd, "%s/mysql < %s", bindir, script_line);
  system(fix_priv_tables_cmd);

  return error;
} /* main */
