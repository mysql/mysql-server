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

/*
**	   mysqlimport.c  - Imports all given files
**			    into a table(s).
**
**			   *************************
**			   *			   *
**			   * AUTHOR: Monty & Jani  *
**			   * DATE:   June 24, 1997 *
**			   *			   *
**			   *************************
*/
#define IMPORT_VERSION "3.4"

#include "client_priv.h"
#include "mysql_version.h"

static void db_error_with_table(MYSQL *mysql, char *table);
static void db_error(MYSQL *mysql);
static char *field_escape(char *to,const char *from,uint length);
static char *add_load_option(char *ptr,const char *object,
			     const char *statement);

static my_bool	verbose=0,lock_tables=0,ignore_errors=0,opt_delete=0,
		replace=0,silent=0,ignore=0,opt_compress=0,opt_local_file=0,
                opt_low_priority= 0, tty_password= 0;
static MYSQL	mysql_connection;
static char	*opt_password=0, *current_user=0,
		*current_host=0, *current_db=0, *fields_terminated=0,
		*lines_terminated=0, *enclosed=0, *opt_enclosed=0,
		*escaped=0, *opt_columns=0, *default_charset;
static uint     opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static longlong opt_ignore_lines= -1;
#include <sslopt-vars.h>

static struct my_option my_long_options[] =
{
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are", (gptr*) &charsets_dir,
   (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", (gptr*) &default_charset,
   (gptr*) &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"columns", 'c',
   "Use only these columns to import the data to. Give the column names in a comma separated list. This is same as giving columns to LOAD DATA INFILE.",
   (gptr*) &opt_columns, (gptr*) &opt_columns, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"debug",'#', "Output debug log. Often this is 'd:t:o,filename'", 0, 0, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0}, 
  {"delete", 'd', "First delete all rows from table.", (gptr*) &opt_delete,
   (gptr*) &opt_delete, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-terminated-by", OPT_FTB,
   "Fields in the textfile are terminated by ...", (gptr*) &fields_terminated,
   (gptr*) &fields_terminated, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-enclosed-by", OPT_ENC,
   "Fields in the importfile are enclosed by ...", (gptr*) &enclosed,
   (gptr*) &enclosed, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-optionally-enclosed-by", OPT_O_ENC,
   "Fields in the i.file are opt. enclosed by ...", (gptr*) &opt_enclosed,
   (gptr*) &opt_enclosed, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-escaped-by", OPT_ESC, "Fields in the i.file are escaped by ...",
   (gptr*) &escaped, (gptr*) &escaped, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"force", 'f', "Continue even if we get an sql-error.",
   (gptr*) &ignore_errors, (gptr*) &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"help", '?', "Displays this help and exits.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (gptr*) &current_host,
   (gptr*) &current_host, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ignore", 'i', "If duplicate unique key was found, keep old row.",
   (gptr*) &ignore, (gptr*) &ignore, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ignore-lines", OPT_IGN_LINES, "Ignore first n lines of data infile.",
   (gptr*) &opt_ignore_lines, (gptr*) &opt_ignore_lines, 0, GET_LL,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lines-terminated-by", OPT_LTB, "Lines in the i.file are terminated by ...",
   (gptr*) &lines_terminated, (gptr*) &lines_terminated, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"local", 'L', "Read all files through the client", (gptr*) &opt_local_file,
   (gptr*) &opt_local_file, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"lock-tables", 'l', "Lock all tables for write.", (gptr*) &lock_tables,
   (gptr*) &lock_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"low-priority", OPT_LOW_PRIORITY,
   "Use LOW_PRIORITY when updating the table", (gptr*) &opt_low_priority,
   (gptr*) &opt_low_priority, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
   (gptr*) &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0,
   0},
  {"replace", 'r', "If duplicate unique key was found, replace old row.",
   (gptr*) &replace, (gptr*) &replace, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent.", (gptr*) &silent, (gptr*) &silent, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (gptr*) &current_user,
   (gptr*) &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v', "Print info about the various stages.", (gptr*) &verbose,
   (gptr*) &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static const char *load_default_groups[]= { "mysqlimport","client",0 };

#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n" ,my_progname,
	  IMPORT_VERSION, MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  printf("\
Loads tables from text files in various formats.  The base name of the\n\
text file must be the name of the table that should be used.\n\
If one uses sockets to connect to the MySQL server, the server will open and\n\
read the text file directly. In other cases the client will open the text\n\
file. The SQL command 'LOAD DATA INFILE' is used to import the rows.\n");

  printf("\nUsage: %s [OPTIONS] database textfile...",my_progname);
  print_defaults("my",load_default_groups);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

#include <help_end.h>

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
      opt_password=my_strdup(argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
	start[1]=0;				/* Cut length of argument */
    }
    else
      tty_password= 1;
    break;
#ifdef __WIN__
  case 'W':
    opt_mysql_unix_port=MYSQL_NAMEDPIPE;
    opt_local_file=1;
    break;
#endif
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
    break;
#include <sslopt-case.h>
  case 'V': print_version(); exit(0);
  case 'I':
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


static int get_options(int *argc, char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (enclosed && opt_enclosed)
  {
    fprintf(stderr, "You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n");
    return(1);
  }
  if (replace && ignore)
  {
    fprintf(stderr, "You can't use --ignore (-i) and --replace (-r) at the same time.\n");
    return(1);
  }
  if (default_charset)
  {
    if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
      exit(1);
  }
  if (*argc < 2)
  {
    usage();
    return 1;
  }
  current_db= *((*argv)++);
  (*argc)--;
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return(0);
}



static int write_to_table(char *filename, MYSQL *sock)
{
  char tablename[FN_REFLEN], hard_path[FN_REFLEN],
       sql_statement[FN_REFLEN*16+256], *end;
  my_bool local_file;
  DBUG_ENTER("write_to_table");
  DBUG_PRINT("enter",("filename: %s",filename));

  fn_format(tablename, filename, "", "", 1 | 2); /* removes path & ext. */
  if (! opt_local_file)
    strmov(hard_path,filename);
  else
    my_load_path(hard_path, filename, NULL); /* filename includes the path */

  if (opt_delete)
  {
    if (verbose)
      fprintf(stdout, "Deleting the old data from table %s\n", tablename);
    sprintf(sql_statement, "DELETE FROM %s", tablename);
    if (mysql_query(sock, sql_statement))
    {
      db_error_with_table(sock, tablename);
      DBUG_RETURN(1);
    }
  }
  to_unix_path(hard_path);
  if (verbose)
  {
    if (opt_local_file)
      fprintf(stdout, "Loading data from LOCAL file: %s into %s\n",
	      hard_path, tablename);
    else
      fprintf(stdout, "Loading data from SERVER file: %s into %s\n",
	      hard_path, tablename);
  }
  sprintf(sql_statement, "LOAD DATA %s %s INFILE '%s'",
	  opt_low_priority ? "LOW_PRIORITY" : "",
	  opt_local_file ? "LOCAL" : "", hard_path);
  end= strend(sql_statement);
  if (replace)
    end= strmov(end, " REPLACE");
  if (ignore)
    end= strmov(end, " IGNORE");
  end= strmov(strmov(end, " INTO TABLE "), tablename);

  if (fields_terminated || enclosed || opt_enclosed || escaped)
      end= strmov(end, " FIELDS");
  end= add_load_option(end, fields_terminated, " TERMINATED BY");
  end= add_load_option(end, enclosed, " ENCLOSED BY");
  end= add_load_option(end, opt_enclosed,
		       " OPTIONALLY ENCLOSED BY");
  end= add_load_option(end, escaped, " ESCAPED BY");
  end= add_load_option(end, lines_terminated, " LINES TERMINATED BY");
  if (opt_ignore_lines >= 0)
    end= strmov(longlong10_to_str(opt_ignore_lines, 
				  strmov(end, " IGNORE "),10), " LINES");
  if (opt_columns)
    end= strmov(strmov(strmov(end, " ("), opt_columns), ")");
  *end= '\0';

  if (mysql_query(sock, sql_statement))
  {
    db_error_with_table(sock, tablename);
    DBUG_RETURN(1);
  }
  if (!silent)
  {
    if (mysql_info(sock)) /* If NULL-pointer, print nothing */
    {
      fprintf(stdout, "%s.%s: %s\n", current_db, tablename,
	      mysql_info(sock));
    }
  }
  DBUG_RETURN(0);
}



static void lock_table(MYSQL *sock, int tablecount, char **raw_tablename)
{
  DYNAMIC_STRING query;
  int i;
  char tablename[FN_REFLEN];

  if (verbose)
    fprintf(stdout, "Locking tables for write\n");
  init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
  for (i=0 ; i < tablecount ; i++)
  {
    fn_format(tablename, raw_tablename[i], "", "", 1 | 2);
    dynstr_append(&query, tablename);
    dynstr_append(&query, " WRITE,");
  }
  if (mysql_real_query(sock, query.str, query.length-1))
    db_error(sock); /* We shall countinue here, if --force was given */
}




static MYSQL *db_connect(char *host, char *database, char *user, char *passwd)
{
  MYSQL *sock;
  if (verbose)
    fprintf(stdout, "Connecting to %s\n", host ? host : "localhost");
  mysql_init(&mysql_connection);
  if (opt_compress)
    mysql_options(&mysql_connection,MYSQL_OPT_COMPRESS,NullS);
  if (opt_local_file)
    mysql_options(&mysql_connection,MYSQL_OPT_LOCAL_INFILE,
		  (char*) &opt_local_file);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&mysql_connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#endif
  if (!(sock= mysql_real_connect(&mysql_connection,host,user,passwd,
				 database,opt_mysql_port,opt_mysql_unix_port,
				 0)))
  {
    ignore_errors=0;	  /* NO RETURN FROM db_error */
    db_error(&mysql_connection);
  }
  if (verbose)
    fprintf(stdout, "Selecting database %s\n", database);
  if (mysql_select_db(sock, database))
  {
    ignore_errors=0;
    db_error(&mysql_connection);
  }
  return sock;
}



static void db_disconnect(char *host, MYSQL *sock)
{
  if (verbose)
    fprintf(stdout, "Disconnecting from %s\n", host ? host : "localhost");
  mysql_close(sock);
}



static void safe_exit(int error, MYSQL *sock)
{
  if (ignore_errors)
    return;
  if (sock)
    mysql_close(sock);
  exit(error);
}



static void db_error_with_table(MYSQL *mysql, char *table)
{
  my_printf_error(0,"Error: %s, when using table: %s",
		  MYF(0), mysql_error(mysql), table);
  safe_exit(1, mysql);
}



static void db_error(MYSQL *mysql)
{
  my_printf_error(0,"Error: %s", MYF(0), mysql_error(mysql));
  safe_exit(1, mysql);
}


static char *add_load_option(char *ptr, const char *object,
			     const char *statement)
{
  if (object)
  {
    /* Don't escape hex constants */
    if (object[0] == '0' && (object[1] == 'x' || object[1] == 'X'))
      ptr= strxmov(ptr," ",statement," ",object,NullS);
    else
    {
      /* char constant; escape */
      ptr= strxmov(ptr," ",statement," '",NullS);
      ptr= field_escape(ptr,object,(uint) strlen(object));
      *ptr++= '\'';
    }
  }
  return ptr;
}

/*
** Allow the user to specify field terminator strings like:
** "'", "\", "\\" (escaped backslash), "\t" (tab), "\n" (newline)
** This is done by doubleing ' and add a end -\ if needed to avoid
** syntax errors from the SQL parser.
*/ 

static char *field_escape(char *to,const char *from,uint length)
{
  const char *end;
  uint end_backslashes=0; 

  for (end= from+length; from != end; from++)
  {
    *to++= *from;
    if (*from == '\\')
      end_backslashes^=1;    /* find odd number of backslashes */
    else 
    {
      if (*from == '\'' && !end_backslashes)
	*to++= *from;      /* We want a dublicate of "'" for MySQL */
      end_backslashes=0;
    }
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
    *to++= '\\';          
  return to;
}



int main(int argc, char **argv)
{
  int exitcode=0, error=0;
  char **argv_to_free;
  MYSQL *sock=0;
  MY_INIT(argv[0]);

  load_defaults("my",load_default_groups,&argc,&argv);
  /* argv is changed in the program */
  argv_to_free= argv;
  if (get_options(&argc, &argv))
  {
    free_defaults(argv_to_free);
    return(1);
  }
  if (!(sock= db_connect(current_host,current_db,current_user,opt_password)))
  {
    free_defaults(argv_to_free);
    return(1); /* purecov: deadcode */
  }
  if (lock_tables)
    lock_table(sock, argc, argv);
  for (; *argv != NULL; argv++)
    if ((error=write_to_table(*argv, sock)))
      if (exitcode == 0)
	exitcode = error;
  db_disconnect(current_host, sock);
  my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
  free_defaults(argv_to_free);
  my_end(0);
  return(exitcode);
}
