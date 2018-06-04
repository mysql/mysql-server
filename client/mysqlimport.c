/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
**	   mysqlimport.c  - Imports all given files
**			    into a table(s).
*/

#define IMPORT_VERSION "3.7"

#include "client_priv.h"
#include "my_default.h"
#include "mysql_version.h"

#include <welcome_copyright_notice.h>   /* ORACLE_WELCOME_COPYRIGHT_NOTICE */


/* Global Thread counter */
uint counter;
native_mutex_t init_mutex;
native_mutex_t counter_mutex;
native_cond_t count_threshold;

static void db_error_with_table(MYSQL *mysql, char *table);
static void db_error(MYSQL *mysql);
static char *field_escape(char *to,const char *from,uint length);
static char *add_load_option(char *ptr,const char *object,
			     const char *statement);

static my_bool	verbose=0,lock_tables=0,ignore_errors=0,opt_delete=0,
		replace=0,silent=0,ignore=0,opt_compress=0,
                opt_low_priority= 0, tty_password= 0, opt_secure_auth= TRUE;
static my_bool debug_info_flag= 0, debug_check_flag= 0;
static uint opt_use_threads=0, opt_local_file=0, my_end_arg= 0;
static char	*opt_password=0, *current_user=0,
		*current_host=0, *current_db=0, *fields_terminated=0,
		*lines_terminated=0, *enclosed=0, *opt_enclosed=0,
		*escaped=0, *opt_columns=0, 
		*default_charset= (char*) MYSQL_AUTODETECT_CHARSET_NAME;
static uint opt_enable_cleartext_plugin= 0;
static my_bool using_opt_enable_cleartext_plugin= 0;
static uint     opt_mysql_port= 0, opt_protocol= 0;
static char *opt_bind_addr = NULL;
static char * opt_mysql_unix_port=0;
static char *opt_plugin_dir= 0, *opt_default_auth= 0;
static longlong opt_ignore_lines= -1;
#include <sslopt-vars.h>
#include <caching_sha2_passwordopt-vars.h>

#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
static char *shared_memory_base_name=0;
#endif

static struct my_option my_long_options[] =
{
  {"bind-address", 0, "IP address to bind to.",
   (uchar**) &opt_bind_addr, (uchar**) &opt_bind_addr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", &charsets_dir,
   &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", &default_charset,
   &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"columns", 'c',
   "Use only these columns to import the data to. Give the column names in a comma separated list. This is same as giving columns to LOAD DATA INFILE.",
   &opt_columns, &opt_columns, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
#ifdef DBUG_OFF
   {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
   {"debug-check", OPT_DEBUG_CHECK, "This is a non-debug version. Catch this and exit.",
   0, 0, 0,
   GET_DISABLED, NO_ARG, 0, 0, 0, 0, 0, 0},
   {"debug-info", OPT_DEBUG_INFO, "This is a non-debug version. Catch this and exit.", 0,
   0, 0, GET_DISABLED, NO_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug",'#', "Output debug log. Often this is 'd:t:o,filename'.", 0, 0, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
   &debug_info_flag, &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"default_auth", OPT_DEFAULT_AUTH,
   "Default authentication client-side plugin to use.",
   &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delete", 'd', "First delete all rows from table.", &opt_delete,
   &opt_delete, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"enable_cleartext_plugin", OPT_ENABLE_CLEARTEXT_PLUGIN,
   "Enable/disable the clear text authentication plugin.",
   &opt_enable_cleartext_plugin, &opt_enable_cleartext_plugin,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-terminated-by", OPT_FTB,
   "Fields in the input file are terminated by the given string.", 
   &fields_terminated, &fields_terminated, 0, 
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-enclosed-by", OPT_ENC,
   "Fields in the import file are enclosed by the given character.", 
   &enclosed, &enclosed, 0, 
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-optionally-enclosed-by", OPT_O_ENC,
   "Fields in the input file are optionally enclosed by the given character.", 
   &opt_enclosed, &opt_enclosed, 0, 
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-escaped-by", OPT_ESC, 
   "Fields in the input file are escaped by the given character.",
   &escaped, &escaped, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"force", 'f', "Continue even if we get an SQL error.",
   &ignore_errors, &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"help", '?', "Displays this help and exits.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", &current_host,
   &current_host, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ignore", 'i', "If duplicate unique key was found, keep old row.",
   &ignore, &ignore, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ignore-lines", OPT_IGN_LINES, "Ignore first n lines of data infile.",
   &opt_ignore_lines, &opt_ignore_lines, 0, GET_LL,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lines-terminated-by", OPT_LTB, 
   "Lines in the input file are terminated by the given string.",
   &lines_terminated, &lines_terminated, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"local", 'L', "Read all files through the client.", &opt_local_file,
   &opt_local_file, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"lock-tables", 'l', "Lock all tables for write (this disables threads).",
    &lock_tables, &lock_tables, 0, GET_BOOL, NO_ARG, 
    0, 0, 0, 0, 0, 0},
  {"low-priority", OPT_LOW_PRIORITY,
   "Use LOW_PRIORITY when updating the table.", &opt_low_priority,
   &opt_low_priority, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef _WIN32
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
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
   &opt_mysql_port,
   &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol to use for connection (tcp, socket, pipe, memory).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replace", 'r', "If duplicate unique key was found, replace old row.",
   &replace, &replace, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"secure-auth", OPT_SECURE_AUTH, "Refuse client connecting to server if it"
    " uses old (pre-4.1.1) protocol. Deprecated. Always TRUE",
    &opt_secure_auth, &opt_secure_auth, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name, &shared_memory_base_name,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"silent", 's', "Be more silent.", &silent, &silent, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
#include <caching_sha2_passwordopt-longopts.h>
  {"use-threads", OPT_USE_THREADS,
   "Load files in parallel. The argument is the number "
   "of threads to use for loading data.",
   &opt_use_threads, &opt_use_threads, 0, 
   GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", &current_user,
   &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Print info about the various stages.", &verbose,
   &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static const char *load_default_groups[]= { "mysqlimport","client",0 };


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n" ,my_progname,
	  IMPORT_VERSION, MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  struct my_option *optp;
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("\
Loads tables from text files in various formats.  The base name of the\n\
text file must be the name of the table that should be used.\n\
If one uses sockets to connect to the MySQL server, the server will open and\n\
read the text file directly. In other cases the client will open the text\n\
file. The SQL command 'LOAD DATA INFILE' is used to import the rows.\n");

  printf("\nUsage: %s [OPTIONS] database textfile...",my_progname);
  print_defaults("my",load_default_groups);
  /*
    Turn default for zombies off so that the help on how to 
    turn them off text won't show up.
    This is safe to do since it's followed by a call to exit().
  */
  for (optp= my_long_options; optp->name; optp++)
  {
    if (optp->id == OPT_SECURE_AUTH)
    {
      optp->def_value= 0;
      break;
    }
  }
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


static my_bool
get_one_option(int optid, const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument)
{
  switch(optid) {
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";			/* Don't require password */
    if (argument)
    {
      char *start=argument;
      my_free(opt_password);
      opt_password=my_strdup(PSI_NOT_INSTRUMENTED,
                             argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
	start[1]=0;				/* Cut length of argument */
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
#ifdef _WIN32
  case 'W':
    opt_protocol = MYSQL_PROTOCOL_PIPE;
    opt_local_file=1;
    break;
#endif
  case OPT_ENABLE_CLEARTEXT_PLUGIN:
    using_opt_enable_cleartext_plugin= TRUE;
    break;
  case OPT_MYSQL_PROTOCOL:
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
                                    opt->name);
    break;
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
    debug_check_flag= 1;
    break;
#include <sslopt-case.h>
  case 'V': print_version(); exit(0);
  case 'I':
  case '?':
    usage();
    exit(0);
  case OPT_SECURE_AUTH:
    /* --secure-auth is a zombie option. */
    if (!opt_secure_auth)
    {
      fprintf(stderr, "mysqlimport: [ERROR] --skip-secure-auth is not supported.\n");
      exit(1);
    }
    else
      CLIENT_WARN_DEPRECATED_NO_REPLACEMENT("--secure-auth");
    break;
  }
  return 0;
}


static int get_options(int *argc, char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;

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



static int write_to_table(char *filename, MYSQL *mysql)
{
  char tablename[FN_REFLEN], hard_path[FN_REFLEN],
       escaped_name[FN_REFLEN * 2 + 1],
       sql_statement[FN_REFLEN*16+256], *end, *pos;
  DBUG_ENTER("write_to_table");
  DBUG_PRINT("enter",("filename: %s",filename));

  fn_format(tablename, filename, "", "", 1 | 2); /* removes path & ext. */
  if (!opt_local_file)
    my_stpcpy(hard_path,filename);
  else
    my_load_path(hard_path, filename, NULL); /* filename includes the path */

  if (opt_delete)
  {
    if (verbose)
      fprintf(stdout, "Deleting the old data from table %s\n", tablename);
    my_snprintf(sql_statement, FN_REFLEN*16+256, "DELETE FROM %s", tablename);
    if (mysql_query(mysql, sql_statement))
    {
      db_error_with_table(mysql, tablename);
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
  mysql_real_escape_string_quote(mysql, escaped_name, hard_path,
                                 (unsigned long) strlen(hard_path), '\'');
  sprintf(sql_statement, "LOAD DATA %s %s INFILE '%s'",
	  opt_low_priority ? "LOW_PRIORITY" : "",
	  opt_local_file ? "LOCAL" : "", escaped_name);
  end= strend(sql_statement);
  if (replace)
    end= my_stpcpy(end, " REPLACE");
  if (ignore)
    end= my_stpcpy(end, " IGNORE");
  end= my_stpcpy(end, " INTO TABLE `");
  /* Turn any ` into `` in table name. */
  for (pos= tablename; *pos; pos++)
  {
    if (*pos == '`')
      *end++= '`';
    *end++= *pos;
  }
  end= my_stpcpy(end, "`");

  if (fields_terminated || enclosed || opt_enclosed || escaped)
      end= my_stpcpy(end, " FIELDS");
  end= add_load_option(end, fields_terminated, " TERMINATED BY");
  end= add_load_option(end, enclosed, " ENCLOSED BY");
  end= add_load_option(end, opt_enclosed,
		       " OPTIONALLY ENCLOSED BY");
  end= add_load_option(end, escaped, " ESCAPED BY");
  end= add_load_option(end, lines_terminated, " LINES TERMINATED BY");
  if (opt_ignore_lines >= 0)
    end= my_stpcpy(longlong10_to_str(opt_ignore_lines, 
				  my_stpcpy(end, " IGNORE "),10), " LINES");
  if (opt_columns)
    end= my_stpcpy(my_stpcpy(my_stpcpy(end, " ("), opt_columns), ")");
  *end= '\0';

  if (mysql_query(mysql, sql_statement))
  {
    db_error_with_table(mysql, tablename);
    DBUG_RETURN(1);
  }
  if (!silent)
  {
    if (mysql_info(mysql)) /* If NULL-pointer, print nothing */
    {
      fprintf(stdout, "%s.%s: %s\n", current_db, tablename,
	      mysql_info(mysql));
    }
  }
  DBUG_RETURN(0);
}



static void lock_table(MYSQL *mysql, int tablecount, char **raw_tablename)
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
  if (mysql_real_query(mysql, query.str, (ulong)(query.length-1)))
    db_error(mysql); /* We shall countinue here, if --force was given */
}




static MYSQL *db_connect(char *host, char *database,
                         char *user, char *passwd)
{
  MYSQL *mysql;
  if (verbose)
    fprintf(stdout, "Connecting to %s\n", host ? host : "localhost");
  if (opt_use_threads && !lock_tables)
  {
    native_mutex_lock(&init_mutex);
    if (!(mysql= mysql_init(NULL)))
    {
      native_mutex_unlock(&init_mutex);
      return 0;
    }
    native_mutex_unlock(&init_mutex);
  }
  else
    if (!(mysql= mysql_init(NULL)))
      return 0;
  if (opt_compress)
    mysql_options(mysql,MYSQL_OPT_COMPRESS,NullS);
  if (opt_local_file)
    mysql_options(mysql,MYSQL_OPT_LOCAL_INFILE,
		  (char*) &opt_local_file);
  SSL_SET_OPTIONS(mysql);
  if (opt_protocol)
    mysql_options(mysql,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
  if (opt_bind_addr)
    mysql_options(mysql,MYSQL_OPT_BIND,opt_bind_addr);
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  if (shared_memory_base_name)
    mysql_options(mysql,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif

  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, opt_default_auth);

  if (using_opt_enable_cleartext_plugin)
    mysql_options(mysql, MYSQL_ENABLE_CLEARTEXT_PLUGIN,
                  (char*)&opt_enable_cleartext_plugin);

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset);
  mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD,
                 "program_name", "mysqlimport");

  set_server_public_key(mysql);
  set_get_server_public_key_option(mysql);

  if (!(mysql_real_connect(mysql,host,user,passwd,
                           database,opt_mysql_port,opt_mysql_unix_port,
                           0)))
  {
    ignore_errors=0;	  /* NO RETURN FROM db_error */
    db_error(mysql);
  }
  mysql->reconnect= 0;
  if (verbose)
    fprintf(stdout, "Selecting database %s\n", database);
  if (mysql_select_db(mysql, database))
  {
    ignore_errors=0;
    db_error(mysql);
  }
  return mysql;
}



static void db_disconnect(char *host, MYSQL *mysql)
{
  if (verbose)
    fprintf(stdout, "Disconnecting from %s\n", host ? host : "localhost");
  mysql_close(mysql);
}



static void safe_exit(int error, MYSQL *mysql)
{
  if (ignore_errors)
    return;
  if (mysql)
    mysql_close(mysql);
  exit(error);
}



static void db_error_with_table(MYSQL *mysql, char *table)
{
  my_printf_error(0,"Error: %d, %s, when using table: %s",
		  MYF(0), mysql_errno(mysql), mysql_error(mysql), table);
  safe_exit(1, mysql);
}



static void db_error(MYSQL *mysql)
{
  my_printf_error(0,"Error: %d %s", MYF(0), mysql_errno(mysql), mysql_error(mysql));
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

int exitcode= 0;

void *worker_thread(void *arg)
{
  int error;
  char *raw_table_name= (char *)arg;
  MYSQL *mysql= 0;

  if (mysql_thread_init())
    goto error;
  
  if (!(mysql= db_connect(current_host,current_db,current_user,opt_password)))
  {
    goto error;
  }

  if (mysql_query(mysql, "/*!40101 set @@character_set_database=binary */;"))
  {
    db_error(mysql); /* We shall countinue here, if --force was given */
    goto error;
  }

  /*
    We are not currently catching the error here.
  */
  if((error= write_to_table(raw_table_name, mysql)))
    if (exitcode == 0)
      exitcode= error;

error:
  if (mysql)
    db_disconnect(current_host, mysql);

  native_mutex_lock(&counter_mutex);
  counter--;
  native_cond_signal(&count_threshold);
  native_mutex_unlock(&counter_mutex);
  mysql_thread_end();
  my_thread_exit(0);
  return 0;
}


int main(int argc, char **argv)
{
  int error=0;
  char **argv_to_free;
  MY_INIT(argv[0]);

  my_getopt_use_args_separator= TRUE;
  if (load_defaults("my",load_default_groups,&argc,&argv))
    return 1;
  my_getopt_use_args_separator= FALSE;

  /* argv is changed in the program */
  argv_to_free= argv;
  if (get_options(&argc, &argv))
  {
    free_defaults(argv_to_free);
    return(1);
  }

  if (opt_use_threads && !lock_tables)
  {
    char **save_argv;
    uint worker_thread_count= 0, table_count= 0, i= 0;
    my_thread_handle *worker_threads;       /* Thread descriptor */
    my_thread_attr_t attr;                  /* Thread attributes */
    my_thread_attr_init(&attr);
    my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

    native_mutex_init(&init_mutex, NULL);
    native_mutex_init(&counter_mutex, NULL);
    native_cond_init(&count_threshold);

    /* Count the number of tables. This number denotes the total number
       of threads spawn.
    */
    save_argv= argv;
    for (table_count= 0; *argv != NULL; argv++)
      table_count++;
    argv= save_argv;

    if (!(worker_threads= (my_thread_handle*) my_malloc(PSI_NOT_INSTRUMENTED,
                                              table_count *
                                              sizeof(*worker_threads),
                                              MYF(0))))
      return -2;

    for (counter= 0; *argv != NULL; argv++) /* Loop through tables */
    {
      native_mutex_lock(&counter_mutex);
      while (counter == opt_use_threads)
      {
        struct timespec abstime;

        set_timespec(&abstime, 3);
        native_cond_timedwait(&count_threshold, &counter_mutex, &abstime);
      }
      /* Before exiting the lock we set ourselves up for the next thread */
      counter++;
      native_mutex_unlock(&counter_mutex);
      /* now create the thread */
      if (my_thread_create(&worker_threads[worker_thread_count], &attr,
                           worker_thread, (void *)*argv) != 0)
      {
        native_mutex_lock(&counter_mutex);
        counter--;
        native_mutex_unlock(&counter_mutex);
        fprintf(stderr,"%s: Could not create thread\n", my_progname);
        continue;
      }
      worker_thread_count++;
    }

    /*
      We loop until we know that all children have cleaned up.
    */
    native_mutex_lock(&counter_mutex);
    while (counter)
    {
      struct timespec abstime;

      set_timespec(&abstime, 3);
      native_cond_timedwait(&count_threshold, &counter_mutex, &abstime);
    }
    native_mutex_unlock(&counter_mutex);
    native_mutex_destroy(&init_mutex);
    native_mutex_destroy(&counter_mutex);
    native_cond_destroy(&count_threshold);
    my_thread_attr_destroy(&attr);

    for(i= 0; i < worker_thread_count; i++)
    {
      if (my_thread_join(&worker_threads[i], NULL))
        fprintf(stderr,"%s: Could not join worker thread.\n", my_progname);
    }

    my_free(worker_threads);
  }
  else
  {
    MYSQL *mysql= 0;
    if (!(mysql= db_connect(current_host,current_db,current_user,opt_password)))
    {
      free_defaults(argv_to_free);
      return(1); /* purecov: deadcode */
    }

    if (mysql_query(mysql, "/*!40101 set @@character_set_database=binary */;"))
    {
      db_error(mysql); /* We shall countinue here, if --force was given */
      return(1);
    }

    if (lock_tables)
      lock_table(mysql, argc, argv);
    for (; *argv != NULL; argv++)
      if ((error= write_to_table(*argv, mysql)))
        if (exitcode == 0)
          exitcode= error;
    db_disconnect(current_host, mysql);
  }
  my_free(opt_password);
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  my_free(shared_memory_base_name);
#endif
  free_defaults(argv_to_free);
  my_end(my_end_arg);
  return(exitcode);
}
