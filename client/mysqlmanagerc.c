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

#define MANAGER_CLIENT_VERSION "1.4"

#include <my_global.h>
#include <mysql.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <my_sys.h>
#include <m_string.h>
#include <my_getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MYSQL_MANAGER_PORT
#define MYSQL_MANAGER_PORT 9305
#endif

static void die(const char* fmt, ...);

const char* user="root",*host="localhost";
char* pass=0;
my_bool quiet=0;
uint port=MYSQL_MANAGER_PORT;
static const char *load_default_groups[]= { "mysqlmanagerc",0 };
char** default_argv;
MYSQL_MANAGER *manager;
FILE* fp, *fp_out;

static struct my_option my_long_options[] =
{
  {"host", 'h', "Connect to host.", (gptr*) &host, (gptr*) &host, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login.", (gptr*) &user, (gptr*) &user, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p', "Password to use when connecting to server.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &port,
   (gptr*) &port, 0, GET_UINT, REQUIRED_ARG, MYSQL_MANAGER_PORT, 0, 0, 0, 0,
   0},
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",  0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quiet", 'q', "Suppress all normal output.", (gptr*) &quiet, (gptr*) &quiet,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void die(const char* fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "%s: ", my_progname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);
  exit(1);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,
	 MANAGER_CLIENT_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  printf("MySQL AB, by Sasha\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Command-line client for MySQL manager daemon.\n\n");
  printf("Usage: %s [OPTIONS] < command_file\n", my_progname);
  my_print_help(my_long_options);
  printf("  --no-defaults         Don't read default options from any options file.\n");
  my_print_variables(my_long_options);
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  my_bool tty_password=0;
  
  switch (optid) {
  case 'p':
    if (argument)
    {
      my_free(pass, MYF(MY_ALLOW_ZERO_PTR));
      pass= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';	/* Destroy argument */
    }
    else
      tty_password=1;
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);				
  }
  return 0;
}


int parse_args(int argc, char **argv)
{
  int ho_error;

  load_defaults("my",load_default_groups,&argc,&argv);
  default_argv= argv;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option, NULL)))
    exit(ho_error);

  return 0;
}


int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  fp=stdin;
  fp_out=stdout;
  parse_args(argc,argv);
  if (!(manager=mysql_manager_init(0)))
    die("Failed in mysql_manager_init()");
  if (!mysql_manager_connect(manager,host,user,pass,port))
    die("Could not connect to MySQL manager: %s (%d)",manager->last_error,
	manager->last_errno);
  for (;!feof(fp);)
  {
    char buf[4096];
    if (!fgets(buf,sizeof(buf),fp))
      break;
    if (!quiet)
      fprintf(fp_out,"<<%s",buf);
    if (mysql_manager_command(manager,buf,strlen(buf)))
      die("Error in command: %s (%d)",manager->last_error,manager->last_errno);
    while (!manager->eof)
    {
      if (mysql_manager_fetch_line(manager,buf,sizeof(buf)))
	die("Error fetching result line: %s (%d)", manager->last_error,
	    manager->last_errno);
      if (!quiet)
        fprintf(fp_out,">>%s\n",buf);
    }
  }
  mysql_manager_close(manager);
  return 0;
}
