/*
  Copyright (c) 2003 Novell, Inc. All Rights Reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/stat.h>
#include <monitor.h>
#include <strings.h>
#include <getopt.h>
#include <screen.h>
#include <dirent.h>

#include "my_config.h"
#include "my_manage.h"
#include "mysql_version.h"

/******************************************************************************

	global variables

******************************************************************************/
char autoclose;
char basedir[PATH_MAX];
char checktables;
char datadir[PATH_MAX];
char pid_file[PATH_MAX];
char address[PATH_MAX];
char port[PATH_MAX];
char err_log[PATH_MAX];
char safe_log[PATH_MAX];
char mysqld[PATH_MAX];
char hostname[PATH_MAX];
char default_option[PATH_MAX];

FILE *log_fd= NULL;

/******************************************************************************

	prototypes

******************************************************************************/

void usage(void);
void vlog(char *, va_list);
void log(char *, ...);
void start_defaults(int, char *[]);
void finish_defaults();
void read_defaults(arg_list_t *);
void parse_args(int, char *[]);
void get_options(int, char *[]);
void check_data_vol();
void check_setup();
void check_tables();
void mysql_start(int, char *[]);
void parse_setvar(char *arg);

/******************************************************************************

	functions

******************************************************************************/

/******************************************************************************

  usage()

  Show usage.

******************************************************************************/
void usage(void)
{
  // keep the screen up
  setscreenmode(SCR_NO_MODE);

  puts("\
\n\
usage: mysqld_safe [options]\n\
\n\
Program to start the MySQL daemon and restart it if it dies unexpectedly.\n\
All options, besides those listed below, are passed on to the MySQL daemon.\n\
\n\
options:\n\
\n\
--autoclose                 Automatically close the mysqld_safe screen.\n\
\n\
--check-tables              Check the tables before starting the MySQL daemon.\n\
\n\
--err-log=<file>            Send the MySQL daemon error output to <file>.\n\
\n\
--help                      Show this help information.\n\
\n\
--mysqld=<file>             Use the <file> MySQL daemon.\n\
\n\
  ");

  exit(-1);
}

/******************************************************************************

  vlog()

  Log the message.

******************************************************************************/
void vlog(char *format, va_list ap)
{
  vfprintf(stdout, format, ap);
  fflush(stdout);

  if (log_fd)
  {
    vfprintf(log_fd, format, ap);
    fflush(log_fd);
  }
}

/******************************************************************************

  log()

  Log the message.

******************************************************************************/
void log(char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  vlog(format, ap);

  va_end(ap);
}

/******************************************************************************

	start_defaults()

	Start setting the defaults.

******************************************************************************/
void start_defaults(int argc, char *argv[])
{
  struct stat buf;
  int i;

  // default options
  static char *default_options[]=
  {
    "--no-defaults",
    "--defaults-file=",
    "--defaults-extra-file=",
    NULL
  };

  // autoclose
  autoclose= FALSE;

  // basedir
  get_basedir(argv[0], basedir);

  // check-tables
  checktables= FALSE;

  // hostname
  if (gethostname(hostname, PATH_MAX) < 0)
  {
    // default
    strcpy(hostname, "mysql");
  }

  // address
  snprintf(address, PATH_MAX, "0.0.0.0");

  // port
  snprintf(port, PATH_MAX, "%d", MYSQL_PORT);

  // default option
  default_option[0]= NULL;
  for (i= 0; (argc > 1) && default_options[i]; i++)
  {
    if (!strnicmp(argv[1], default_options[i], strlen(default_options[i])))
    {
      strncpy(default_option, argv[1], PATH_MAX);
      break;
    }
  }

  // set after basedir is established
  datadir[0]= NULL;
  pid_file[0]= NULL;
  err_log[0]= NULL;
  safe_log[0]= NULL;
  mysqld[0]= NULL;
}

/******************************************************************************

	finish_defaults()

	Finish settig the defaults.

******************************************************************************/
void finish_defaults()
{
  struct stat buf;
  int i;

  // datadir
  if (!datadir[0])
    snprintf(datadir, PATH_MAX, "%s/data", basedir);

  // pid-file
  if (!pid_file[0])
    snprintf(pid_file, PATH_MAX, "%s/%s.pid", datadir, hostname);

  // err-log
  if (!err_log[0])
    snprintf(err_log, PATH_MAX, "%s/%s.err", datadir, hostname);

  // safe-log
  if (!safe_log[0])
    snprintf(safe_log, PATH_MAX, "%s/%s.safe", datadir, hostname);

  // mysqld
  if (!mysqld[0])
    snprintf(mysqld, PATH_MAX, "%s/bin/mysqld-max", basedir);

  if (stat(mysqld, &buf))
  {
    snprintf(mysqld, PATH_MAX, "%s/bin/mysqld", basedir);
  }
}

/******************************************************************************

	read_defaults()

	Read the defaults.

******************************************************************************/
void read_defaults(arg_list_t *pal)
{
  arg_list_t al;
  char defaults_file[PATH_MAX];
  char mydefaults[PATH_MAX];
  char line[PATH_MAX];
  FILE *fp;

  // defaults output file
  snprintf(defaults_file, PATH_MAX, "%s/bin/defaults.out", basedir);
  remove(defaults_file);

  // mysqladmin file
  snprintf(mydefaults, PATH_MAX, "%s/bin/my_print_defaults", basedir);

  // args
  init_args(&al);
  add_arg(&al, mydefaults);
  if (default_option[0])
    add_arg(&al, default_option);
  add_arg(&al, "mysqld");
  add_arg(&al, "server");
  add_arg(&al, "mysqld_safe");
  add_arg(&al, "safe_mysqld");

  spawn(mydefaults, &al, TRUE, NULL, defaults_file, NULL);

  free_args(&al);

  // gather defaults
  if ((fp= fopen(defaults_file, "r")) != NULL)
  {
    while (fgets(line, PATH_MAX, fp))
    {
      char *p;

      // remove end-of-line character
      if ((p= strrchr(line, '\n')) != NULL)
	*p= '\0';

      // add the option as an argument
      add_arg(pal, line);
    }

    fclose(fp);
  }

  // remove file
  remove(defaults_file);
}

/******************************************************************************

	parse_args()

	Get the options.

******************************************************************************/
void parse_args(int argc, char *argv[])
{
  int index= 0;
  int c;

  // parse options
  enum opts
  {
    OPT_BASEDIR= 0xFF,
    OPT_DATADIR,
    OPT_PID_FILE,
    OPT_BIND_ADDRESS,
    OPT_PORT,
    OPT_ERR_LOG,
    OPT_SAFE_LOG,
    OPT_MYSQLD,
    OPT_HELP,
    OPT_SETVAR
  };

  static struct option options[]=
  {
    {"autoclose", no_argument, &autoclose, TRUE},
    {"basedir", required_argument, 0, OPT_BASEDIR},
    {"check-tables", no_argument, &checktables, TRUE},
    {"datadir", required_argument, 0, OPT_DATADIR},
    {"pid-file", required_argument, 0, OPT_PID_FILE},
    {"bind-address", required_argument, 0, OPT_BIND_ADDRESS},
    {"port", required_argument, 0, OPT_PORT},
    {"err-log", required_argument, 0, OPT_ERR_LOG},
    {"safe-log", required_argument, 0, OPT_SAFE_LOG},
    {"mysqld", required_argument, 0, OPT_MYSQLD},
    {"help", no_argument, 0, OPT_HELP},
    {"set-variable", required_argument, 0, OPT_SETVAR},
    {0, 0, 0, 0}
  };

  // we have to reset getopt_long because we use it multiple times
  optind= 1;

  // turn off error reporting
  opterr= 0;

  while ((c= getopt_long(argc, argv, "b:h:P:", options, &index)) >= 0)
  {
    switch (c) {
    case OPT_BASEDIR:
    case 'b':
      strcpy(basedir, optarg);
      break;

    case OPT_DATADIR:
    case 'h':
      strcpy(datadir, optarg);
      break;

    case OPT_PID_FILE:
      strcpy(pid_file, optarg);
      break;

    case OPT_BIND_ADDRESS:
      strcpy(address, optarg);
      break;

    case OPT_PORT:
    case 'P':
      strcpy(port, optarg);
      break;

    case OPT_ERR_LOG:
      strcpy(err_log, optarg);
      break;

    case OPT_SAFE_LOG:
      strcpy(safe_log, optarg);
      break;

    case OPT_MYSQLD:
      strcpy(mysqld, optarg);
      break;

    case OPT_SETVAR:
      parse_setvar(optarg);
      break;

    case OPT_HELP:
      usage();
      break;

    default:
      // ignore
      break;
    }
  }
}

/*
  parse_setvar(char *arg)
  Pasrsing for port just to display the port num on the mysqld_safe screen
*/
void parse_setvar(char *arg)
{
  char *pos;

  if ((pos= strindex(arg, "port")))
  {
    for (; *pos && *pos != '='; pos++);
    if (*pos)
      strcpy(port, pos + 1);
  }
}

/******************************************************************************



/******************************************************************************

	get_options()

	Get the options.

******************************************************************************/
void get_options(int argc, char *argv[])
{
  arg_list_t al;

  // start defaults
  start_defaults(argc, argv);

  // default file arguments
  init_args(&al);
  add_arg(&al, "ignore");
  read_defaults(&al);
  parse_args(al.argc, al.argv);
  free_args(&al);

  // command-line arguments
  parse_args(argc, argv);

  // finish defaults
  finish_defaults();
}

/******************************************************************************

	check_data_vol()

	Check the database volume.

******************************************************************************/
void check_data_vol()
{
  // warn if the data is on a Traditional volume
  struct volume_info vol;
  char buff[PATH_MAX];
  char *p;

  // clear struct
  memset(&vol, 0, sizeof(vol));

  // find volume name
  strcpy(buff, datadir);
  if (p= strchr(buff, ':'))
  {
    // terminate after volume name
    *p= 0;
  }
  else
  {
    // assume SYS volume
    strcpy(buff, "SYS");
  }

  // retrieve information
  netware_vol_info_from_name(&vol, buff);

  if ((vol.flags & VOL_NSS_PRESENT) == 0)
  {
    log("Error: Either the data directory does not exist or is not on an NSS volume!\n\n");
    exit(-1);
  }
}

/******************************************************************************

	check_setup()

	Check the current setup.

******************************************************************************/
void check_setup()
{
  struct stat info;
  char temp[PATH_MAX];

  // remove any current pid_file
  if (!stat(pid_file, &info) && (remove(pid_file) < 0))
  {
    log("ERROR: Unable to remove current pid file!\n\n");
    exit(-1);
  }

  // check the data volume
  check_data_vol();

  // check for a database
  snprintf(temp, PATH_MAX, "%s/mysql/host.frm", datadir);
  if (stat(temp, &info))
  {
    log("ERROR: No database found in the data directory!\n\n");
    exit(-1);
  }
}

/******************************************************************************

	check_tables()

	Check the database tables.

******************************************************************************/
void check_tables()
{
  arg_list_t al;
  char mycheck[PATH_MAX];
  char table[PATH_MAX];
  char db[PATH_MAX];
  DIR *datadir_entry, *db_entry, *table_entry;

  // status
  log("checking tables...\n");

  // list databases
  if ((datadir_entry= opendir(datadir)) == NULL)
  {
    return;
  }

  while ((db_entry= readdir(datadir_entry)) != NULL)
  {
    if (db_entry->d_name[0] == '.')
    {
      // Skip
    }
    else if (S_ISDIR(db_entry->d_type))
    {
      // create long db name
      snprintf(db, PATH_MAX, "%s/%s", datadir, db_entry->d_name);

      // list tables
      if ((db_entry= opendir(db)) == NULL)
      {
	continue;
      }

      while ((table_entry= readdir(db_entry)) != NULL)
      {
	// create long table name
	snprintf(table, PATH_MAX, "%s/%s", db, strlwr(table_entry->d_name));

	if (strindex(table, ".myi"))
	{
	  // ** myisamchk

	  // mysqladmin file
	  snprintf(mycheck, PATH_MAX, "%s/bin/myisamchk", basedir);

	  // args
	  init_args(&al);
	  add_arg(&al, mycheck);
	  add_arg(&al, "--silent");
	  add_arg(&al, "--force");
	  add_arg(&al, "--fast");
	  add_arg(&al, "--medium-check");
	  add_arg(&al, "-O");
	  add_arg(&al, "key_buffer=64M");
	  add_arg(&al, "-O");
	  add_arg(&al, "sort_buffer=64M");
	  add_arg(&al, table);

	  spawn(mycheck, &al, TRUE, NULL, NULL, NULL);

	  free_args(&al);
	}
	else if (strindex(table, ".ism"))
	{
	  // ** isamchk

	  // mysqladmin file
	  snprintf(mycheck, PATH_MAX, "%s/bin/isamchk", basedir);

	  // args
	  init_args(&al);
	  add_arg(&al, mycheck);
	  add_arg(&al, "--silent");
	  add_arg(&al, "--force");
	  add_arg(&al, "-O");
	  add_arg(&al, "sort_buffer=64M");
	  add_arg(&al, table);

	  spawn(mycheck, &al, TRUE, NULL, NULL, NULL);

	  free_args(&al);
	}
      }
    }
  }
}

/******************************************************************************

	mysql_start()

	Start the mysql server.

******************************************************************************/
void mysql_start(int argc, char *argv[])
{
  arg_list_t al;
  int i, j, err;
  struct stat info;
  time_t cal;
  struct tm lt;
  char stamp[PATH_MAX];
  char skip;

  // private options
  static char *private_options[]=
  {
    "--autoclose",
    "--check-tables",
    "--help",
    "--err-log=",
    "--mysqld=",
    NULL
  };

  // args
  init_args(&al);
  add_arg(&al, "%s", mysqld);

  // parent args
  for (i= 1; i < argc; i++)
  {
    skip= FALSE;

    // skip private arguments
    for (j= 0; private_options[j]; j++)
    {
      if (!strnicmp(argv[i], private_options[j], strlen(private_options[j])))
      {
	skip= TRUE;
	break;
      }
    }

    if (!skip)
    {
      add_arg(&al, "%s", argv[i]);
    }
  }
  // spawn
  do
  {
    // check the database tables
    if (checktables)
      check_tables();

    // status
    time(&cal);
    localtime_r(&cal, &lt);
    strftime(stamp, PATH_MAX, "%d %b %Y %H:%M:%S", &lt);
    log("mysql started    : %s\n", stamp);

    // spawn mysqld
    spawn(mysqld, &al, TRUE, NULL, NULL, err_log);
  }
  while (!stat(pid_file, &info));

  // status
  time(&cal);
  localtime_r(&cal, &lt);
  strftime(stamp, PATH_MAX, "%d %b %Y %H:%M:%S", &lt);
  log("mysql stopped    : %s\n\n", stamp);

  // free args
  free_args(&al);
}

/******************************************************************************

	main()

******************************************************************************/
int main(int argc, char **argv)
{
  char temp[PATH_MAX];

  // get the options
  get_options(argc, argv);

  // keep the screen up
  if (!autoclose)
    setscreenmode(SCR_NO_MODE);

  // create log file
  log_fd= fopen(safe_log, "w+");

  // header
  log("MySQL Server %s, for %s (%s)\n\n", VERSION, SYSTEM_TYPE, MACHINE_TYPE);

  // status
  log("address          : %s\n", address);
  log("port             : %s\n", port);
  log("daemon           : %s\n", mysqld);
  log("base directory   : %s\n", basedir);
  log("data directory   : %s\n", datadir);
  log("pid file         : %s\n", pid_file);
  log("error file       : %s\n", err_log);
  log("log file         : %s\n", safe_log);
  log("\n");

  // check setup
  check_setup();

  // start the MySQL server
  mysql_start(argc, argv);

  // close log file
  if (log_fd)
    fclose(log_fd);

  return 0;
}
