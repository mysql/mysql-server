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
#include "client_settings.h"
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

extern ulong net_buffer_length;
extern ulong max_allowed_packet;

#if defined(MSDOS) || defined(__WIN__)
#define ERRNO WSAGetLastError()
#define perror(A)
#else
#include <errno.h>
#define ERRNO errno
#define SOCKET_ERROR -1
#define closesocket(A) close(A)
#endif

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


int mysql_init_character_set(MYSQL *mysql);

MYSQL * STDCALL
mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		   uint port, const char *unix_socket,ulong client_flag)
{
  char *db_name;
  char name_buff[USERNAME_LENGTH];

  DBUG_ENTER("mysql_real_connect");
  DBUG_PRINT("enter",("host: %s  db: %s  user: %s",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));

  if (!host || !host[0])
    host= mysql->options.host;

  if (mysql->options.methods_to_use == MYSQL_OPT_USE_REMOTE_CONNECTION ||
      (mysql->options.methods_to_use == MYSQL_OPT_GUESS_CONNECTION &&
       host && *host && strcmp(host,LOCAL_HOST)))
    DBUG_RETURN(cli_mysql_real_connect(mysql, host, user, 
				       passwd, db, port, 
				       unix_socket, client_flag));

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

  if (!user || !user[0])
    user=mysql->options.user;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!passwd)
  {
    passwd=mysql->options.password;
#if !defined(DONT_USE_MYSQL_PWD)
    if (!passwd)
      passwd=getenv("MYSQL_PWD");		/* get it from environment */
#endif
  }
  mysql->passwd= passwd ? my_strdup(passwd,MYF(0)) : NULL;
#endif /*!NO_EMBEDDED_ACCESS_CHECKS*/
  if (!user || !user[0])
  {
    read_user_name(name_buff);
    if (name_buff[0])
      user= name_buff;
  }

  if (!user)
    user= "";
  mysql->user=my_strdup(user,MYF(0));

  port=0;
  unix_socket=0;
  db_name = db ? my_strdup(db,MYF(MY_WME)) : NULL;

  mysql->thd= create_embedded_thd(client_flag, db_name);

  init_embedded_mysql(mysql, client_flag, db_name);

  if (mysql_init_character_set(mysql))
    goto error;

  if (check_embedded_connection(mysql))
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
	if (!(res= (*mysql->methods->use_result)(mysql)))
	  goto error;
	mysql_free_result(res);
      }
    }
  }

  DBUG_PRINT("exit",("Mysql handler: %lx",mysql));
  DBUG_RETURN(mysql);

error:
  embedded_get_error(mysql);
  DBUG_PRINT("error",("message: %u (%s)", mysql->net.last_errno,
		      mysql->net.last_error));
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

