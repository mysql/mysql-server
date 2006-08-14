/*
  Copyright (c) 2002 Novell, Inc. All Rights Reserved. 

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
#include <errno.h>

#include "my_config.h"
#include "my_manage.h"

/******************************************************************************

	global variables
	
******************************************************************************/
char autoclose;
char basedir[PATH_MAX];
char datadir[PATH_MAX];
char err_log[PATH_MAX];
char out_log[PATH_MAX];
char mysqld[PATH_MAX];
char hostname[PATH_MAX];
char sql_file[PATH_MAX];
char default_option[PATH_MAX];

/******************************************************************************

	prototypes
	
******************************************************************************/

void start_defaults(int, char*[]);
void finish_defaults();
void read_defaults(arg_list_t *);
void parse_args(int, char*[]);
void get_options(int, char*[]);
void create_paths();
int mysql_install_db(int argc, char *argv[]);

/******************************************************************************

	functions
	
******************************************************************************/

/******************************************************************************

	start_defaults()
	
	Start setting the defaults.

******************************************************************************/
void start_defaults(int argc, char *argv[])
{
  struct stat buf;
  int i;
  
  // default options
  static char *default_options[] =
  {
  	"--no-defaults",
  	"--defaults-file=",
  	"--defaults-extra-file=",
  	NULL
  };
  
  // autoclose
  autoclose = FALSE;
  
  // basedir
  get_basedir(argv[0], basedir);
  
  // hostname
  if (gethostname(hostname,PATH_MAX) < 0)
  {
    // default
    strcpy(hostname,"mysql");
  }

  // default option
	default_option[0] = NULL;
  for (i=0; (argc > 1) && default_options[i]; i++)
	{
		if(!strnicmp(argv[1], default_options[i], strlen(default_options[i])))
		{
			strncpy(default_option, argv[1], PATH_MAX);
			break;
		}
	}
  
  // set after basedir is established
  datadir[0] = NULL;
  err_log[0] = NULL;
  out_log[0] = NULL;
  mysqld[0] = NULL;
  sql_file[0] = NULL;
}

/******************************************************************************

	finish_defaults()
	
	Finish setting the defaults.

******************************************************************************/
void finish_defaults()
{
  struct stat buf;
  int i;
  
  // datadir
  if (!datadir[0]) snprintf(datadir, PATH_MAX, "%s/data", basedir);
  
  // err-log
  if (!err_log[0]) snprintf(err_log, PATH_MAX, "%s/%s.err", datadir, hostname);

  // out-log
  if (!out_log[0]) snprintf(out_log, PATH_MAX, "%s/%s.out", datadir, hostname);

  // sql-file
  if (!sql_file[0]) snprintf(sql_file, PATH_MAX, "%s/bin/init_db.sql", basedir);

  // mysqld
  if (!mysqld[0]) snprintf(mysqld, PATH_MAX, "%s/bin/mysqld", basedir);
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
  if (default_option[0]) add_arg(&al, default_option);
  add_arg(&al, "mysqld");
  add_arg(&al, "mysql_install_db");

	spawn(mydefaults, &al, TRUE, NULL, defaults_file, NULL);

  free_args(&al);

	// gather defaults
	if((fp = fopen(defaults_file, "r")) != NULL)
	{
	  while(fgets(line, PATH_MAX, fp))
	  {
      char *p;
      
      // remove end-of-line character
      if ((p = strrchr(line, '\n')) != NULL) *p = '\0';
      
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
  int index = 0;
  int c;
  
  // parse options
  enum opts
  {
    OPT_BASEDIR = 0xFF,
    OPT_DATADIR,
    OPT_SQL_FILE
  };
  
  static struct option options[] =
  {
    {"autoclose",     no_argument,        &autoclose,   TRUE},
    {"basedir",       required_argument,  0,            OPT_BASEDIR},
    {"datadir",       required_argument,  0,            OPT_DATADIR},
    {"sql-file",      required_argument,  0,            OPT_SQL_FILE},
    {0,               0,                  0,            0}
  };
  
  // we have to reset getopt_long because we use it multiple times
  optind = 1;
  
  // turn off error reporting
  opterr = 0;
  
  while ((c = getopt_long(argc, argv, "b:h:", options, &index)) >= 0)
  {
    switch (c)
    {
    case OPT_BASEDIR:
    case 'b':
      strcpy(basedir, optarg);
      break;
      
    case OPT_DATADIR:
    case 'h':
      strcpy(datadir, optarg);
      break;
    
    case OPT_SQL_FILE:
      strcpy(sql_file, optarg);
      break;
    
    default:
      // ignore
      break;
    }
  }
}

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

	create_paths()
	
	Create database paths.

******************************************************************************/
void create_paths()
{
	struct stat info;
  char temp[PATH_MAX];
  
  // check for tables
  snprintf(temp, PATH_MAX, "%s/mysql/host.frm", datadir);
  if (!stat(temp, &info))
  {
    printf("A database already exists in the directory:\n");
    printf("\t%s\n\n", datadir);
    exit(-1);
  }
  
  // data directory
  if (stat(datadir, &info))
  {
    mkdir(datadir, 0);
  }
}

/******************************************************************************

	mysql_install_db()
	
	Install the database.

******************************************************************************/
int mysql_install_db(int argc, char *argv[])
{
	arg_list_t al;
	int i, j, err;
	char skip;
  
  // private options
  static char *private_options[] =
  {
  	"--autoclose",
  	"--sql-file=",
  	NULL
  };
  
	// args
	init_args(&al);
	add_arg(&al, "%s", mysqld);
	
	// parent args
	for(i = 1; i < argc; i++)
	{
    skip = FALSE;
    
    // skip private arguments
    for (j=0; private_options[j]; j++)
    {
      if(!strnicmp(argv[i], private_options[j], strlen(private_options[j])))
      {
        skip = TRUE;
        break;
      }
    }
		
    if (!skip) add_arg(&al, "%s", argv[i]);
	}
	
	add_arg(&al, "--bootstrap");
	add_arg(&al, "--skip-grant-tables");
	add_arg(&al, "--skip-innodb");

  // spawn mysqld
  err = spawn(mysqld, &al, TRUE, sql_file, out_log, err_log);

	// free args
	free_args(&al);
  
  return err;
}

/******************************************************************************

	main()
	
******************************************************************************/
int main(int argc, char **argv)
{
	// get options
	get_options(argc, argv);

  // check for an autoclose option
  if (!autoclose) setscreenmode(SCR_NO_MODE);
  
  // header
  printf("MySQL Server %s, for %s (%s)\n\n", VERSION, SYSTEM_TYPE,
         MACHINE_TYPE);
  
  // create paths
  create_paths();

	// install the database
  if (mysql_install_db(argc, argv))
  {
    printf("ERROR - The database creation failed!\n");
    printf("        %s\n", strerror(errno));
    printf("See the following log for more infomration:\n");
    printf("\t%s\n\n", err_log);
    exit(-1);
  }
	
  // status
  printf("Initial database successfully created in the directory:\n");
  printf("\t%s\n", datadir);

  // info
  printf("\nPLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !\n");

  printf("\nThis is done with:\n");
  printf("\tmysqladmin -u root password 'new-password'\n");
  
  printf("\nSee the manual for more instructions.\n");
    
  printf("\nYou can start the MySQL daemon with:\n");
  printf("\tmysqld_safe\n");
	
  printf("\nPlease report any problems with:\n");
  printf("\t/mysql/mysqlbug.txt\n");
  
  printf("\nThe latest information about MySQL is available on the web at\n");
  printf("\thttp://www.mysql.com\n");
  
  printf("\nSupport MySQL by buying support at http://shop.mysql.com\n\n");
  
  return 0;
}
