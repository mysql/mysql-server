/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

#define SHOW_VERSION "8.2"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>

static my_string host=0,opt_password=0,user=0;
static my_bool opt_show_keys=0,opt_compress=0,opt_status=0;

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
#include "sslopt-vars.h"

int main(int argc, char **argv)
{
  int error;
  char *wild;
  MYSQL mysql;
  MY_INIT(argv[0]);
  load_defaults("my",load_default_groups,&argc,&argv);
  get_options(&argc,&argv);

  wild=0;
  if (argc && strcont(argv[argc-1],"*?%_"))
  {
    char *pos;

    wild=argv[--argc];
    for (pos=wild ; *pos ; pos++)
    {					/* Unix wildcards to sql  */
      if (*pos == '*')
	*pos='%';
      else if (*pos == '?')
	*pos='_';
    }
  }
  else if (argc == 3)			/* We only want one field */
    wild=argv[--argc];

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
		  opt_ssl_capath);
#endif
  if (!(mysql_real_connect(&mysql,host,user,opt_password,
			   argv[0],opt_mysql_port,opt_mysql_unix_port,
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
  my_end(0);
  exit(error ? 1 : 0);
  return 0;				/* No compiler warnings */
}


static struct option long_options[] =
{
  {"character-sets-dir", required_argument, 0, 'c'},
  {"compress",	no_argument,	   0, 'C'},
  {"debug",	optional_argument, 0, '#'},
  {"help",	no_argument,	   0, '?'},
  {"host",	required_argument, 0, 'h'},
  {"status",	no_argument,	   0, 'i'},
  {"keys",	no_argument,	   0, 'k'},
  {"password",	optional_argument, 0, 'p'},
  {"port",	required_argument, 0, 'P'},
#ifdef __WIN__
  {"pipe",	no_argument,	   0, 'W'},
#endif
  {"socket",	required_argument, 0, 'S'},
#include "sslopt-longopts.h"
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",	required_argument, 0, 'u'},
#endif
  {"version",	no_argument,	   0, 'V'},
  {0, 0, 0, 0}
};


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,SHOW_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  puts("Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Shows the structure of a mysql database (databases,tables and columns)\n");
  printf("Usage: %s [OPTIONS] [database [table [column]]]\n",my_progname);
  printf("\n\
  -#, --debug=...       output debug log. Often this is 'd:t:o,filename`\n\
  -?, --help		display this help and exit\n\
  -c, --character-sets-dir=...\n\
                        Directory where character sets are\n\
  -C, --compress        Use compression in server/client protocol\n\
  -h, --host=...	connect to host\n\
  -i, --status		Shows a lot of extra information about each table\n\
  -k, --keys		show keys for table\n\
  -p, --password[=...]	password to use when connecting to server\n\
			If password is not given it's asked from the tty.\n");
#ifdef __WIN__
  puts("-W, --pipe		Use named pipes to connect to server");
#endif
  printf("\
  -P  --port=...	Port number to use for connection\n\
  -S  --socket=...	Socket file to use for connection\n");
#include "sslopt-usage.h"
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		user for login if not current user\n");
#endif
  printf("\
  -V, --version		output version information and exit\n");

  puts("\n\
If last argument contains a shell or SQL wildcard (*,?,% or _) then only\n\
what\'s matched by the wildcard is shown.\n\
If no database is given then all matching databases are shown.\n\
If no table is given then all matching tables in database are shown\n\
If no column is given then all matching columns and columntypes in table\n\
are shown");
  print_defaults("my",load_default_groups);
}


static void
get_options(int *argc,char ***argv)
{
  int c,option_index;
  my_bool tty_password=0;

  while ((c=getopt_long(*argc,*argv,"c:h:p::u:#::P:S:Ck?VWi",long_options,
			&option_index)) != EOF)
  {
    switch(c) {
    case 'C':
      opt_compress=1;
      break;
    case 'c':
      charsets_dir= optarg;
      break;
    case 'h':
      host = optarg;
      break;
    case 'i':
      opt_status=1;
      break;
    case 'k':
      opt_show_keys=1;
      break;
    case 'p':
      if (optarg)
      {
	char *start=optarg;
	my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
	opt_password=my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		/* Destroy argument */
	if (*start)
	  start[1]=0;				/* Cut length of argument */
      }
      else
	tty_password=1;
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      user=optarg;
      break;
#endif
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'S':
      opt_mysql_unix_port= optarg;
      break;
    case 'W':
#ifdef __WIN__
      opt_mysql_unix_port=MYSQL_NAMEDPIPE;
#endif
      break;
#include "sslopt-case.h"
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o");
      break;
    case 'V':
      print_version();
      exit(0);
      break;
    default:
      fprintf(stderr,"Illegal option character '%c'\n",opterr);
      /* Fall throught */
    case '?':
    case 'I':					/* Info */
      usage();
      exit(0);
    }
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return;
}


static int
list_dbs(MYSQL *mysql,const char *wild)
{
  const char *header;
  uint length;
  MYSQL_FIELD *field;
  MYSQL_RES *result;
  MYSQL_ROW row;

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

  print_header(header,length,NullS);
  while ((row = mysql_fetch_row(result)))
    print_row(row[0],length,0);
  print_trailer(length,0);
  mysql_free_result(result);
  return 0;
}


static int
list_tables(MYSQL *mysql,const char *db,const char *table)
{
  const char *header;
  uint head_length;
  MYSQL_FIELD *field;
  MYSQL_RES *result;
  MYSQL_ROW row;

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

  print_header(header,head_length,NullS);
  while ((row = mysql_fetch_row(result)))
    print_row(row[0],head_length,0);
  print_trailer(head_length,0);
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
  end=strmov(strmov(query,"show columns from "),table);
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
    end=strmov(strmov(query,"show keys from "),table);
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
    printf(" %-*s|",field->max_length+1,field->name);
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
