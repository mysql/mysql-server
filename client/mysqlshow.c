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

/* Show databases, tables or columns */

#define SHOW_VERSION "9.4"

#include "client_priv.h"
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <signal.h>
#include <stdarg.h>
#include <sslopt-vars.h>

static my_string host=0,opt_password=0,user=0;
static my_bool opt_show_keys=0,opt_compress=0,opt_status=0, tty_password=0;
static uint opt_verbose=0;
static char *default_charset= (char*) MYSQL_DEFAULT_CHARSET_NAME;

#ifdef HAVE_SMEM 
static char *shared_memory_base_name=0;
#endif
static uint opt_protocol=0;

static void get_options(int *argc,char ***argv);
static uint opt_mysql_port=0;
static int list_dbs(MYSQL *mysql,const char *wild);
static int list_tables(MYSQL *mysql,const char *db,const char *table);
static int list_table_status(MYSQL *mysql,const char *db,const char *table);
static int list_fields(MYSQL *mysql,const char *db,const char *table,
		       const char *field);
static void print_header(const char *header,uint head_length,...);
static void print_row(const char *header,uint head_length,...);
static void print_trailer(uint length,...);
static void print_res_header(MYSQL_RES *result);
static void print_res_top(MYSQL_RES *result);
static void print_res_row(MYSQL_RES *result,MYSQL_ROW cur);

static const char *load_default_groups[]= { "mysqlshow","client",0 };
static my_string opt_mysql_unix_port=0;

int main(int argc, char **argv)
{
  int error;
  my_bool first_argument_uses_wildcards=0;
  char *wild;
  MYSQL mysql;
  MY_INIT(argv[0]);
  load_defaults("my",load_default_groups,&argc,&argv);
  get_options(&argc,&argv);

  wild=0;
  if (argc)
  {
    char *pos= argv[argc-1], *to;
    for (to= pos ; *pos ; pos++, to++)
    {
      switch (*pos)
      {
      case '*':
	*pos= '%';
	first_argument_uses_wildcards= 1;
	break;
      case '?':
	*pos= '_';
	first_argument_uses_wildcards= 1;
	break;
      case '%':
      case '_':
	first_argument_uses_wildcards= 1;
	break;
      case '\\':
	pos++;
      default: break;
      }
      *to= *pos;
    }    
    *to= *pos; /* just to copy a '\0'  if '\\' was used */
  }
  if (first_argument_uses_wildcards)
    wild= argv[--argc];
  else if (argc == 3)			/* We only want one field */
    wild= argv[--argc];

  if (argc > 2)
  {
    fprintf(stderr,"%s: Too many arguments\n",my_progname);
    exit(1);
  }
  mysql_init(&mysql);
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#endif
  if (opt_protocol)
    mysql_options(&mysql,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(&mysql,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif
  mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, default_charset);

  if (!(mysql_real_connect(&mysql,host,user,opt_password,
			   (first_argument_uses_wildcards) ? "" : argv[0],opt_mysql_port,opt_mysql_unix_port,
			   0)))
  {
    fprintf(stderr,"%s: %s\n",my_progname,mysql_error(&mysql));
    exit(1);
  }

  switch (argc)
  {
  case 0:  error=list_dbs(&mysql,wild); break;
  case 1:
    if (opt_status)
      error=list_table_status(&mysql,argv[0],wild);
    else
      error=list_tables(&mysql,argv[0],wild);
    break;
  default:
    if (opt_status && ! wild)
      error=list_table_status(&mysql,argv[0],argv[1]);
    else
      error=list_fields(&mysql,argv[0],argv[1],wild);
    break;
  }
  mysql_close(&mysql);			/* Close & free connection */
  if (opt_password)
    my_free(opt_password,MYF(0));
#ifdef HAVE_SMEM
  my_free(shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif
  my_end(0);
  exit(error ? 1 : 0);
  return 0;				/* No compiler warnings */
}

static struct my_option my_long_options[] =
{
  {"character-sets-dir", 'c', "Directory where character sets are.",
   (gptr*) &charsets_dir, (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", (gptr*) &default_charset,
   (gptr*) &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (gptr*) &host, (gptr*) &host, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"status", 'i', "Shows a lot of extra information about each table.",
   (gptr*) &opt_status, (gptr*) &opt_status, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"keys", 'k', "Show keys for table.", (gptr*) &opt_show_keys,
   (gptr*) &opt_show_keys, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
   (gptr*) &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0,
   0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", (gptr*) &shared_memory_base_name, (gptr*) &shared_memory_base_name,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (gptr*) &user,
   (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v',
   "More verbose output; You can use this multiple times to get even more verbose output.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

  
#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,SHOW_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Shows the structure of a mysql database (databases,tables and columns)\n");
  printf("Usage: %s [OPTIONS] [database [table [column]]]\n",my_progname);
  puts("\n\
If last argument contains a shell or SQL wildcard (*,?,% or _) then only\n\
what\'s matched by the wildcard is shown.\n\
If no database is given then all matching databases are shown.\n\
If no table is given then all matching tables in database are shown\n\
If no column is given then all matching columns and columntypes in table\n\
are shown");
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
  case 'v':
    opt_verbose++;
    break;
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
      tty_password=1;
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
  case OPT_MYSQL_PROTOCOL:
  {
    if ((opt_protocol= find_type(argument, &sql_protocol_typelib,0)) <= 0)
    {
      fprintf(stderr, "Unknown option to protocol: %s\n", argument);
      exit(1);
    }
    break;
  }
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
    break;
#include <sslopt-case.h>
  case 'V':
    print_version();
    exit(0);
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(0);
  }
  return 0;
}


static void
get_options(int *argc,char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);
  
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return;
}


static int
list_dbs(MYSQL *mysql,const char *wild)
{
  const char *header;
  uint length, counter = 0;
  ulong rowcount = 0L;
  char tables[NAME_LEN+1], rows[NAME_LEN+1];
  char query[255];
  MYSQL_FIELD *field;
  MYSQL_RES *result;
  MYSQL_ROW row, trow, rrow;

  if (!(result=mysql_list_dbs(mysql,wild)))
  {
    fprintf(stderr,"%s: Cannot list databases: %s\n",my_progname,
	    mysql_error(mysql));
    return 1;
  }
  if (wild)
    printf("Wildcard: %s\n",wild);

  header="Databases";
  length=(uint) strlen(header);
  field=mysql_fetch_field(result);
  if (length < field->max_length)
    length=field->max_length;

  if (!opt_verbose)
    print_header(header,length,NullS);
  else if (opt_verbose == 1)
    print_header(header,length,"Tables",6,NullS);
  else
    print_header(header,length,"Tables",6,"Total Rows",12,NullS);

  while ((row = mysql_fetch_row(result)))
  {
    counter++;

    if (opt_verbose)
    {
      /*
       *  Original code by MG16373;  Slightly modified by Monty.
       *  Print now the count of tables and rows for each database.
       */

      if (!(mysql_select_db(mysql,row[0])))
      {
	MYSQL_RES *tresult = mysql_list_tables(mysql,(char*)NULL);
	if (mysql_affected_rows(mysql) > 0)
	{
	  sprintf(tables,"%6lu",(ulong) mysql_affected_rows(mysql));
	  rowcount = 0;
	  if (opt_verbose > 1)
	  {
	    while ((trow = mysql_fetch_row(tresult)))
	    {
	      sprintf(query,"SELECT COUNT(*) FROM `%s`",trow[0]);
	      if (!(mysql_query(mysql,query)))
	      {
		MYSQL_RES *rresult;
		if ((rresult = mysql_store_result(mysql)))
		{
		  rrow = mysql_fetch_row(rresult);
		  rowcount += (ulong) strtoull(rrow[0], (char**) 0, 10);
		  mysql_free_result(rresult);
		}
	      }
	    }
	    sprintf(rows,"%12lu",rowcount);
	  }
	}
	else
	{
	  sprintf(tables,"%6d",0);
	  sprintf(rows,"%12d",0);
	}
	mysql_free_result(tresult);
      }
      else
      {
	strmov(tables,"N/A");
	strmov(rows,"N/A");
      }
    }

    if (!opt_verbose)
      print_row(row[0],length,0);
    else if (opt_verbose == 1)
      print_row(row[0],length,tables,6,NullS);
    else
      print_row(row[0],length,tables,6,rows,12,NullS);
  }

  print_trailer(length,
		(opt_verbose > 0 ? 6 : 0),
		(opt_verbose > 1 ? 12 :0),
		0);

  if (counter && opt_verbose)
    printf("%u row%s in set.\n",counter,(counter > 1) ? "s" : "");
  mysql_free_result(result);
  return 0;
}


static int
list_tables(MYSQL *mysql,const char *db,const char *table)
{
  const char *header;
  uint head_length, counter = 0;
  char query[255], rows[64], fields[16];
  MYSQL_FIELD *field;
  MYSQL_RES *result;
  MYSQL_ROW row, rrow;

  if (mysql_select_db(mysql,db))
  {
    fprintf(stderr,"%s: Cannot connect to db %s: %s\n",my_progname,db,
	    mysql_error(mysql));
    return 1;
  }
  if (!(result=mysql_list_tables(mysql,table)))
  {
    fprintf(stderr,"%s: Cannot list tables in %s: %s\n",my_progname,db,
	    mysql_error(mysql));
    exit(1);
  }
  printf("Database: %s",db);
  if (table)
    printf("  Wildcard: %s",table);
  putchar('\n');

  header="Tables";
  head_length=(uint) strlen(header);
  field=mysql_fetch_field(result);
  if (head_length < field->max_length)
    head_length=field->max_length;

  if (!opt_verbose)
    print_header(header,head_length,NullS);
  else if (opt_verbose == 1)
    print_header(header,head_length,"Columns",8,NullS);
  else
    print_header(header,head_length,"Columns",8, "Total Rows",10,NullS);

  while ((row = mysql_fetch_row(result)))
  {
    /*
     *   Modified by MG16373
     *   Print now the count of rows for each table.
     */
    counter++;
    if (opt_verbose > 0)
    {
      if (!(mysql_select_db(mysql,db)))
      {
	MYSQL_RES *rresult = mysql_list_fields(mysql,row[0],NULL);
	ulong rowcount=0L;
	if (!rresult)
	{
	  strmov(fields,"N/A");
	  strmov(rows,"N/A");
	}
	else
	{
	  sprintf(fields,"%8u",(uint) mysql_num_fields(rresult));
	  mysql_free_result(rresult);

	  if (opt_verbose > 1)
	  {
	    sprintf(query,"SELECT COUNT(*) FROM `%s`",row[0]);
	    if (!(mysql_query(mysql,query)))
	    {
	      if ((rresult = mysql_store_result(mysql)))
	      {
		rrow = mysql_fetch_row(rresult);
		rowcount += (unsigned long) strtoull(rrow[0], (char**) 0, 10);
		mysql_free_result(rresult);
	      }
	      sprintf(rows,"%10lu",rowcount);
	    }
	    else
	      sprintf(rows,"%10d",0);
	  }
	}
      }
      else
      {
	strmov(fields,"N/A");
	strmov(rows,"N/A");
      }
    }
    if (!opt_verbose)
      print_row(row[0],head_length,NullS);
    else if (opt_verbose == 1)
      print_row(row[0],head_length, fields,8, NullS);
    else 
      print_row(row[0],head_length, fields,8, rows,10, NullS);
  }

  print_trailer(head_length,
		(opt_verbose > 0 ? 8 : 0),
		(opt_verbose > 1 ? 10 :0),
		0);

  if (counter && opt_verbose)
    printf("%u row%s in set.\n\n",counter,(counter > 1) ? "s" : "");

  mysql_free_result(result);
  return 0;
}


static int
list_table_status(MYSQL *mysql,const char *db,const char *wild)
{
  char query[1024],*end;
  MYSQL_RES *result;
  MYSQL_ROW row;

  end=strxmov(query,"show table status from ",db,NullS);
  if (wild && wild[0])
    strxmov(end," like '",wild,"'",NullS);
  if (mysql_query(mysql,query) || !(result=mysql_store_result(mysql)))
  {
    fprintf(stderr,"%s: Cannot get status for db: %s, table: %s: %s\n",
	    my_progname,db,wild ? wild : "",mysql_error(mysql));
    if (mysql_errno(mysql) == ER_PARSE_ERROR)
      fprintf(stderr,"This error probably means that your MySQL server doesn't support the\n\'show table status' command.\n");
    return 1;
  }

  printf("Database: %s",db);
  if (wild)
    printf("  Wildcard: %s",wild);
  putchar('\n');

  print_res_header(result);
  while ((row=mysql_fetch_row(result)))
    print_res_row(result,row);
  print_res_top(result);
  mysql_free_result(result);
  return 0;
}

/*
** list fields uses field interface as an example of how to parse
** a MYSQL FIELD
*/

static int
list_fields(MYSQL *mysql,const char *db,const char *table,
	    const char *wild)
{
  char query[1024],*end;
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (mysql_select_db(mysql,db))
  {
    fprintf(stderr,"%s: Cannot connect to db: %s: %s\n",my_progname,db,
	    mysql_error(mysql));
    return 1;
  }
  end=strmov(strmov(strmov(query,"show /*!32332 FULL */ columns from `"),table),"`");
  if (wild && wild[0])
    strxmov(end," like '",wild,"'",NullS);
  if (mysql_query(mysql,query) || !(result=mysql_store_result(mysql)))
  {
    fprintf(stderr,"%s: Cannot list columns in db: %s, table: %s: %s\n",
	    my_progname,db,table,mysql_error(mysql));
    return 1;
  }

  printf("Database: %s  Table: %s  Rows: %lu", db,table,
	 (ulong) mysql->extra_info);
  if (wild && wild[0])
    printf("  Wildcard: %s",wild);
  putchar('\n');

  print_res_header(result);
  while ((row=mysql_fetch_row(result)))
    print_res_row(result,row);
  print_res_top(result);
  if (opt_show_keys)
  {
    end=strmov(strmov(strmov(query,"show keys from `"),table),"`");
    if (mysql_query(mysql,query) || !(result=mysql_store_result(mysql)))
    {
      fprintf(stderr,"%s: Cannot list keys in db: %s, table: %s: %s\n",
	      my_progname,db,table,mysql_error(mysql));
      return 1;
    }
    if (mysql_num_rows(result))
    {
      print_res_header(result);
      while ((row=mysql_fetch_row(result)))
	print_res_row(result,row);
      print_res_top(result);
    }
    else
      puts("Table has no keys");
  }
  mysql_free_result(result);
  return 0;
}


/*****************************************************************************
** General functions to print a nice ascii-table from data
*****************************************************************************/

static void
print_header(const char *header,uint head_length,...)
{
  va_list args;
  uint length,i,str_length,pre_space;
  const char *field;

  va_start(args,head_length);
  putchar('+');
  field=header; length=head_length;
  for (;;)
  {
    for (i=0 ; i < length+2 ; i++)
      putchar('-');
    putchar('+');
    if (!(field=va_arg(args,my_string)))
      break;
    length=va_arg(args,uint);
  }
  va_end(args);
  putchar('\n');

  va_start(args,head_length);
  field=header; length=head_length;
  putchar('|');
  for (;;)
  {
    str_length=(uint) strlen(field);
    if (str_length > length)
      str_length=length+1;
    pre_space=(uint) (((int) length-(int) str_length)/2)+1;
    for (i=0 ; i < pre_space ; i++)
      putchar(' ');
    for (i = 0 ; i < str_length ; i++)
      putchar(field[i]);
    length=length+2-str_length-pre_space;
    for (i=0 ; i < length ; i++)
      putchar(' ');
    putchar('|');
    if (!(field=va_arg(args,my_string)))
      break;
    length=va_arg(args,uint);
  }
  va_end(args);
  putchar('\n');

  va_start(args,head_length);
  putchar('+');
  field=header; length=head_length;
  for (;;)
  {
    for (i=0 ; i < length+2 ; i++)
      putchar('-');
    putchar('+');
    if (!(field=va_arg(args,my_string)))
      break;
    length=va_arg(args,uint);
  }
  va_end(args);
  putchar('\n');
}


static void
print_row(const char *header,uint head_length,...)
{
  va_list args;
  const char *field;
  uint i,length,field_length;

  va_start(args,head_length);
  field=header; length=head_length;
  for (;;)
  {
    putchar('|');
    putchar(' ');
    fputs(field,stdout);
    field_length=(uint) strlen(field);
    for (i=field_length ; i <= length ; i++)
      putchar(' ');
    if (!(field=va_arg(args,my_string)))
      break;
    length=va_arg(args,uint);
  }
  va_end(args);
  putchar('|');
  putchar('\n');
}


static void
print_trailer(uint head_length,...)
{
  va_list args;
  uint length,i;

  va_start(args,head_length);
  length=head_length;
  putchar('+');
  for (;;)
  {
    for (i=0 ; i < length+2 ; i++)
      putchar('-');
    putchar('+');
    if (!(length=va_arg(args,uint)))
      break;
  }
  va_end(args);
  putchar('\n');
}


static void print_res_header(MYSQL_RES *result)
{
  MYSQL_FIELD *field;

  print_res_top(result);
  mysql_field_seek(result,0);
  putchar('|');
  while ((field = mysql_fetch_field(result)))
  {
    printf(" %-*s|",(int) field->max_length+1,field->name);
  }
  putchar('\n');
  print_res_top(result);
}


static void print_res_top(MYSQL_RES *result)
{
  uint i,length;
  MYSQL_FIELD *field;

  putchar('+');
  mysql_field_seek(result,0);
  while((field = mysql_fetch_field(result)))
  {
    if ((length=(uint) strlen(field->name)) > field->max_length)
      field->max_length=length;
    else
      length=field->max_length;
    for (i=length+2 ; i--> 0 ; )
      putchar('-');
    putchar('+');
  }
  putchar('\n');
}


static void print_res_row(MYSQL_RES *result,MYSQL_ROW cur)
{
  uint i,length;
  MYSQL_FIELD *field;
  putchar('|');
  mysql_field_seek(result,0);
  for (i=0 ; i < mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    length=field->max_length;
    printf(" %-*s|",length+1,cur[i] ? (char*) cur[i] : "");
  }
  putchar('\n');
}
