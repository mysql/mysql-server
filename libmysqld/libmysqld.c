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

#include "embedded_priv.h"
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "errmsg.h"
#include <violite.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(MSDOS) && !defined(__WIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_TRANSACTIONS | CLIENT_PROTOCOL_41)

#if defined(MSDOS) || defined(__WIN__)
#define ERRNO WSAGetLastError()
#define perror(A)
#else
#include <errno.h>
#define ERRNO errno
#define SOCKET_ERROR -1
#define closesocket(A) close(A)
#endif

void free_old_query(MYSQL *mysql);
my_bool STDCALL
emb_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const char *header, ulong header_length,
		     const char *arg, ulong arg_length, my_bool skip_check);

/* From client.c */
void mysql_read_default_options(struct st_mysql_options *options,
				const char *filename,const char *group);
MYSQL * STDCALL 
cli_mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag);

void STDCALL cli_mysql_close(MYSQL *mysql);

#ifdef HAVE_GETPWUID
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
}
#endif

/**************************************************************************
** Shut down connection
**************************************************************************/

static void end_server(MYSQL *mysql)
{
  DBUG_ENTER("end_server");
  free_old_query(mysql);
  DBUG_VOID_RETURN;
}

/**************************************************************************
** Connect to sql server
** If host == 0 then use localhost
**************************************************************************/

MYSQL * STDCALL
mysql_connect(MYSQL *mysql,const char *host,
	      const char *user, const char *passwd)
{
  MYSQL *res;
  mysql=mysql_init(mysql);			/* Make it thread safe */
  {
    DBUG_ENTER("mysql_connect");
    if (!(res=mysql_real_connect(mysql,host,user,passwd,NullS,0,NullS,0)))
    {
      if (mysql->free_me)
	my_free((gptr) mysql,MYF(0));
    }
    DBUG_RETURN(res);
  }
}

static inline int mysql_init_charset(MYSQL *mysql)
{
  char charset_name_buff[16], *charset_name;

  if ((charset_name=mysql->options.charset_name))
  {
    const char *save=charsets_dir;
    if (mysql->options.charset_dir)
      charsets_dir=mysql->options.charset_dir;
    mysql->charset=get_charset_by_name(mysql->options.charset_name,
                                       MYF(MY_WME));
    charsets_dir=save;
  }
  else if (mysql->server_language)
  {
    charset_name=charset_name_buff;
    sprintf(charset_name,"%d",mysql->server_language);	/* In case of errors */
    mysql->charset=get_charset((uint8) mysql->server_language, MYF(MY_WME));
  }
  else
    mysql->charset=default_charset_info;

  if (!mysql->charset)
  {
    mysql->net.last_errno=CR_CANT_READ_CHARSET;
    strmov(mysql->net.sqlstate, "HY0000");
    if (mysql->options.charset_dir)
      sprintf(mysql->net.last_error,ER(mysql->net.last_errno),
              charset_name ? charset_name : "unknown",
              mysql->options.charset_dir);
    else
    {
      char cs_dir_name[FN_REFLEN];
      get_charsets_dir(cs_dir_name);
      sprintf(mysql->net.last_error,ER(mysql->net.last_errno),
              charset_name ? charset_name : "unknown",
              cs_dir_name);
    }
    return mysql->net.last_errno;
  }
  return 0;
}

/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

static void STDCALL emb_fetch_lengths(ulong *to, MYSQL_ROW column, uint field_count)
{ 
  MYSQL_ROW end;

  for (end=column + field_count; column != end ; column++,to++)
  {
    *to= *column ? strlen(*column) : 0;
  }
}


/*
** Note that the mysql argument must be initialized with mysql_init()
** before calling mysql_real_connect !
*/

static my_bool STDCALL emb_mysql_read_query_result(MYSQL *mysql);
static MYSQL_RES * STDCALL emb_mysql_store_result(MYSQL *mysql);
static MYSQL_RES * STDCALL emb_mysql_use_result(MYSQL *mysql);

static MYSQL_METHODS embedded_methods= 
{
  emb_mysql_read_query_result,
  emb_advanced_command,
  emb_mysql_store_result,
  emb_mysql_use_result,
  emb_fetch_lengths
};

MYSQL * STDCALL
mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		   uint port, const char *unix_socket,ulong client_flag)
{
  char          *db_name;
  DBUG_ENTER("mysql_real_connect");
  DBUG_PRINT("enter",("host: %s  db: %s  user: %s",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));

  if (mysql->options.methods_to_use == MYSQL_OPT_USE_REMOTE_CONNECTION)
    cli_mysql_real_connect(mysql, host, user, 
			   passwd, db, port, unix_socket, client_flag);
  if ((mysql->options.methods_to_use == MYSQL_OPT_GUESS_CONNECTION) &&
      host && strcmp(host,LOCAL_HOST))
    cli_mysql_real_connect(mysql, host, user, 
			   passwd, db, port, unix_socket, client_flag);

  mysql->methods= &embedded_methods;

  /* use default options */
  if (mysql->options.my_cnf_file || mysql->options.my_cnf_group)
  {
    mysql_read_default_options(&mysql->options,
			       (mysql->options.my_cnf_file ?
				mysql->options.my_cnf_file : "my"),
			       mysql->options.my_cnf_group);
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.my_cnf_file=mysql->options.my_cnf_group=0;
  }

  if (!db || !db[0])
    db=mysql->options.db;

  port=0;
  unix_socket=0;
  db_name = db ? my_strdup(db,MYF(MY_WME)) : NULL;

  mysql->thd= create_embedded_thd(client_flag, db_name);

  init_embedded_mysql(mysql, client_flag, db_name);

  if (mysql_init_charset(mysql))
    goto error;

  /* Send client information for access check */
  client_flag|=CLIENT_CAPABILITIES;
  client_flag&= ~CLIENT_COMPRESS;
  if (db)
    client_flag|=CLIENT_CONNECT_WITH_DB;
  mysql->server_status= SERVER_STATUS_AUTOCOMMIT;

  if (mysql->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end= ptr + init_commands->elements;

    for (; ptr<end; ptr++)
    {
      MYSQL_RES *res;
      if (mysql_query(mysql,*ptr))
	goto error;
      if (mysql->fields)
      {
	if (!(res= mysql_use_result(mysql)))
	  goto error;
	mysql_free_result(res);
      }
    }
  }

  DBUG_PRINT("exit",("Mysql handler: %lx",mysql));
  DBUG_RETURN(mysql);

error:
  DBUG_PRINT("error",("message: %u (%s)",mysql->net.last_errno,mysql->net.last_error));
  {
    /* Free alloced memory */
    my_bool free_me=mysql->free_me;
    end_server(mysql);
    mysql->free_me=0;
    mysql_close(mysql);
    mysql->free_me=free_me;
  }
  DBUG_RETURN(0);
}

/*************************************************************************
** Send a QUIT to the server and close the connection
** If handle is alloced by mysql connect free it.
*************************************************************************/

void STDCALL mysql_close(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close");
  if (mysql->methods != &embedded_methods)
  {
    cli_mysql_close(mysql);
    DBUG_VOID_RETURN;
  }

  if (mysql)					/* Some simple safety */
  {
    my_free(mysql->options.user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.host,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.password,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.unix_socket,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.db,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
    if (mysql->options.init_commands)
    {
      DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
      char **ptr= (char**)init_commands->buffer;
      char **end= ptr + init_commands->elements;
      for (; ptr<end; ptr++)
	my_free(*ptr,MYF(MY_WME));
      delete_dynamic(init_commands);
      my_free((char*)init_commands,MYF(MY_WME));
    }
    /* Clear pointers for better safety */
    bzero((char*) &mysql->options,sizeof(mysql->options));
#ifdef HAVE_OPENSSL
    ((VioConnectorFd*)(mysql->connector_fd))->delete();
    mysql->connector_fd = 0;
#endif /* HAVE_OPENSSL */
    if (mysql->free_me)
      my_free((gptr) mysql,MYF(0));
  }
  DBUG_VOID_RETURN;
}

static my_bool STDCALL emb_mysql_read_query_result(MYSQL *mysql)
{
  if (mysql->net.last_errno)
    return -1;

  if (mysql->field_count)
  {
    mysql->status=MYSQL_STATUS_GET_RESULT;
    mysql->affected_rows= mysql->result->row_count= mysql->result->data->rows;
    mysql->result->data_cursor= mysql->result->data->data;
  }

  return 0;
}

/**************************************************************************
** Alloc result struct for buffered results. All rows are read to buffer.
** mysql_data_seek may be used.
**************************************************************************/
static MYSQL_RES * STDCALL emb_mysql_store_result(MYSQL *mysql)
{
  MYSQL_RES *result= mysql->result;
  if (!result)
    return 0;
  
  result->methods= mysql->methods;
  mysql->result= NULL;
  *result->data->prev_ptr= 0;
  result->eof= 1;
  result->lengths= (ulong*)(result + 1);
  mysql->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=  result->data->data;

  mysql->status=MYSQL_STATUS_READY;		/* server is ready */
  return result;
}

/**************************************************************************
** Alloc struct for use with unbuffered reads. Data is fetched by domand
** when calling to mysql_fetch_row.
** mysql_data_seek is a noop.
**
** No other queries may be specified with the same MYSQL handle.
** There shouldn't be much processing per row because mysql server shouldn't
** have to wait for the client (and will not wait more than 30 sec/packet).
**************************************************************************/

static MYSQL_RES * STDCALL emb_mysql_use_result(MYSQL *mysql)
{
  DBUG_ENTER("mysql_use_result");
  if (mysql->options.separate_thread)
    DBUG_RETURN(0);

  DBUG_RETURN(emb_mysql_store_result(mysql));
}
