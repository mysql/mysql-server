/* Copyright (C) 2000-2003 MySQL AB

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

#include <my_global.h>
#if defined(__WIN__) || defined(_WIN32) || defined(_WIN64)
#include <winsock.h>
#include <odbcinst.h>
#endif
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "errmsg.h"
#include <violite.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <assert.h> /* for DBUG_ASSERT() */
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(MSDOS) && !defined(__WIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#if defined(THREAD) && !defined(__WIN__)
#include <my_pthread.h>				/* because of signal()	*/
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

static my_bool	mysql_client_init=0;
uint		mysql_port=0;
my_string	mysql_unix_port=0;
ulong 		net_buffer_length=8192;
ulong		max_allowed_packet= 1024L*1024L*1024L;
ulong		net_read_timeout=  NET_READ_TIMEOUT;
ulong		net_write_timeout= NET_WRITE_TIMEOUT;
const char	*unknown_sqlstate= "000000";

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG	  \
                             | CLIENT_LOCAL_FILES   | CLIENT_TRANSACTIONS \
			     | CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION)


#ifdef __WIN__
#define CONNECT_TIMEOUT 20
#else
#define CONNECT_TIMEOUT 0
#endif

#if defined(MSDOS) || defined(__WIN__)
/* socket_errno is defined in my_global.h for all platforms */
#define perror(A)
#else
#include <errno.h>
#define SOCKET_ERROR -1
#endif /* __WIN__ */

#ifdef HAVE_SMEM
char *shared_memory_base_name=0;
const char *def_shared_memory_base_name=default_shared_memory_base_name;
#endif

const char *sql_protocol_names_lib[] =
{ "TCP", "SOCKET", "PIPE", "MEMORY",NullS };
TYPELIB sql_protocol_typelib = {array_elements(sql_protocol_names_lib)-1,"",
			   sql_protocol_names_lib};
/*
  If allowed through some configuration, then this needs to
  be changed
*/
#define MAX_LONG_DATA_LENGTH 8192
#define protocol_41(A) ((A)->server_capabilities & CLIENT_PROTOCOL_41)
#define unsigned_field(A) ((A)->flags & UNSIGNED_FLAG)

static MYSQL_DATA *read_rows (MYSQL *mysql,MYSQL_FIELD *fields,
			      uint field_count);
static int read_one_row(MYSQL *mysql,uint fields,MYSQL_ROW row,
			ulong *lengths);
static void end_server(MYSQL *mysql);
static void read_user_name(char *name);
static void append_wild(char *to,char *end,const char *wild);
static my_bool mysql_reconnect(MYSQL *mysql);
static my_bool send_file_to_server(MYSQL *mysql,const char *filename);
static sig_handler pipe_sig_handler(int sig);
static ulong mysql_sub_escape_string(CHARSET_INFO *charset_info, char *to,
				     const char *from, ulong length);
static my_bool stmt_close(MYSQL_STMT *stmt, my_bool skip_list);
static void fetch_lengths(ulong *to, MYSQL_ROW column, uint field_count);
static my_bool org_my_init_done=0;

extern ulong STDCALL net_field_length(uchar **packet);
extern my_ulonglong net_field_length_ll(uchar **packet);

int STDCALL mysql_server_init(int argc __attribute__((unused)),
			      char **argv __attribute__((unused)),
			      char **groups __attribute__((unused)))
{
  mysql_once_init();
  return 0;
}

void STDCALL mysql_server_end()
{
  /* If library called my_init(), free memory allocated by it */
  if (!org_my_init_done)
    my_end(0);
  else
    mysql_thread_end();
}

my_bool STDCALL mysql_thread_init()
{
#ifdef THREAD
    return my_thread_init();
#else
    return 0;
#endif
}

void STDCALL mysql_thread_end()
{
#ifdef THREAD
    my_thread_end();
#endif
}

/*
  Let the user specify that we don't want SIGPIPE;  This doesn't however work
  with threaded applications as we can have multiple read in progress.
*/

#if !defined(__WIN__) && defined(SIGPIPE) && !defined(THREAD)
#define init_sigpipe_variables  sig_return old_signal_handler=(sig_return) 0;
#define set_sigpipe(mysql)     if ((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE) old_signal_handler=signal(SIGPIPE,pipe_sig_handler)
#define reset_sigpipe(mysql) if ((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE) signal(SIGPIPE,old_signal_handler);
#else
#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#endif

#define _libmysql_c
#include "../sql-common/client.c"

static MYSQL* spawn_init(MYSQL* parent, const char* host,
			 unsigned int port,
			 const char* user,
			 const char* passwd);

/*
  Create new shared memory connection, return handler of connection

  SYNOPSIS
    create_shared_memory()
    mysql		Pointer of mysql structure
    net			Pointer of net structure
    connect_timeout	Timeout of connection
*/

#ifdef HAVE_SMEM
HANDLE create_shared_memory(MYSQL *mysql,NET *net, uint connect_timeout)
{
  ulong smem_buffer_length = shared_memory_buffer_length + 4;
/*
  event_connect_request is event object for start connection actions
  event_connect_answer is event object for confirm, that server put data
  handle_connect_file_map is file-mapping object, use for create shared memory
  handle_connect_map is pointer on shared memory
  handle_map is pointer on shared memory for client
  event_server_wrote,
  event_server_read,
  event_client_wrote,
  event_client_read are events for transfer data between server and client
  handle_file_map is file-mapping object, use for create shared memory
*/
  HANDLE event_connect_request = NULL;
  HANDLE event_connect_answer = NULL;
  HANDLE handle_connect_file_map = NULL;
  char *handle_connect_map = NULL;

  char *handle_map = NULL;
  HANDLE event_server_wrote = NULL;
  HANDLE event_server_read = NULL;
  HANDLE event_client_wrote = NULL;
  HANDLE event_client_read = NULL;
  HANDLE handle_file_map = NULL;
  ulong connect_number;
  char connect_number_char[22], *p;
  char tmp[64];
  char *suffix_pos;
  DWORD error_allow = 0;
  DWORD error_code = 0;
  char *shared_memory_base_name = mysql->options.shared_memory_base_name;

/*
  The name of event and file-mapping events create agree next rule:
            shared_memory_base_name+unique_part
  Where:
    shared_memory_base_name is unique value for each server
    unique_part is uniquel value for each object (events and file-mapping)
*/
  suffix_pos = strxmov(tmp,shared_memory_base_name,"_",NullS);
  strmov(suffix_pos, "CONNECT_REQUEST");
  if (!(event_connect_request= OpenEvent(EVENT_ALL_ACCESS,FALSE,tmp)))
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_REQUEST_ERROR;
    goto err;
  }
  strmov(suffix_pos, "CONNECT_ANSWER");
  if (!(event_connect_answer= OpenEvent(EVENT_ALL_ACCESS,FALSE,tmp)))
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_ANSWER_ERROR;
    goto err;
  }
  strmov(suffix_pos, "CONNECT_DATA");
  if (!(handle_connect_file_map= OpenFileMapping(FILE_MAP_WRITE,FALSE,tmp)))
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_FILE_MAP_ERROR;
    goto err;
  }
  if (!(handle_connect_map= MapViewOfFile(handle_connect_file_map,
					  FILE_MAP_WRITE,0,0,sizeof(DWORD))))
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_MAP_ERROR;
    goto err;
  }
  /*
    Send to server request of connection
  */
  if (!SetEvent(event_connect_request))
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_SET_ERROR;
    goto err;
  }
  /*
    Wait of answer from server
  */
  if (WaitForSingleObject(event_connect_answer,connect_timeout*1000) !=
      WAIT_OBJECT_0)
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_ABANDODED_ERROR;
    goto err;
  }
  /*
    Get number of connection
  */
  connect_number = uint4korr(handle_connect_map);/*WAX2*/
  p= int2str(connect_number, connect_number_char, 10);

  /*
    The name of event and file-mapping events create agree next rule:
    shared_memory_base_name+unique_part+number_of_connection
    Where:
      shared_memory_base_name is uniquel value for each server
      unique_part is uniquel value for each object (events and file-mapping)
      number_of_connection is number of connection between server and client
  */
  suffix_pos = strxmov(tmp,shared_memory_base_name,"_",connect_number_char,
		       "_",NullS);
  strmov(suffix_pos, "DATA");
  if ((handle_file_map = OpenFileMapping(FILE_MAP_WRITE,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_FILE_MAP_ERROR;
    goto err2;
  }
  if ((handle_map = MapViewOfFile(handle_file_map,FILE_MAP_WRITE,0,0,
				  smem_buffer_length)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_MAP_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "SERVER_WROTE");
  if ((event_server_wrote = OpenEvent(EVENT_ALL_ACCESS,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "SERVER_READ");
  if ((event_server_read = OpenEvent(EVENT_ALL_ACCESS,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "CLIENT_WROTE");
  if ((event_client_wrote = OpenEvent(EVENT_ALL_ACCESS,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "CLIENT_READ");
  if ((event_client_read = OpenEvent(EVENT_ALL_ACCESS,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }
  /*
    Set event that server should send data
  */
  SetEvent(event_server_read);

err2:
  if (error_allow == 0)
  {
    net->vio= vio_new_win32shared_memory(net,handle_file_map,handle_map,
					 event_server_wrote,
                                         event_server_read,event_client_wrote,
					 event_client_read);
  }
  else
  {
    error_code = GetLastError();
    if (event_server_read)
      CloseHandle(event_server_read);
    if (event_server_wrote)
      CloseHandle(event_server_wrote);
    if (event_client_read)
      CloseHandle(event_client_read);
    if (event_client_wrote)
      CloseHandle(event_client_wrote);
    if (handle_map)
      UnmapViewOfFile(handle_map);
    if (handle_file_map)
      CloseHandle(handle_file_map);
  }
err:
  if (error_allow)
    error_code = GetLastError();
  if (event_connect_request)
    CloseHandle(event_connect_request);
  if (event_connect_answer)
    CloseHandle(event_connect_answer);
  if (handle_connect_map)
    UnmapViewOfFile(handle_connect_map);
  if (handle_connect_file_map)
    CloseHandle(handle_connect_file_map);
  if (error_allow)
  {
    net->last_errno=error_allow;
    strmov(net->sqlstate, unknown_sqlstate);
    if (error_allow == CR_SHARED_MEMORY_EVENT_ERROR)
      sprintf(net->last_error,ER(net->last_errno),suffix_pos,error_code);
    else
      sprintf(net->last_error,ER(net->last_errno),error_code);
    return(INVALID_HANDLE_VALUE);
  }
  return(handle_map);
}
#endif

/*
  Expand wildcard to a sql string
*/

static void
append_wild(char *to, char *end, const char *wild)
{
  end-=5;					/* Some extra */
  if (wild && wild[0])
  {
    to=strmov(to," like '");
    while (*wild && to < end)
    {
      if (*wild == '\\' || *wild == '\'')
	*to++='\\';
      *to++= *wild++;
    }
    if (*wild)					/* Too small buffer */
      *to++='%';				/* Nicer this way */
    to[0]='\'';
    to[1]=0;
  }
}


/**************************************************************************
  Init debugging if MYSQL_DEBUG environment variable is found
**************************************************************************/

void STDCALL
mysql_debug(const char *debug __attribute__((unused)))
{
#ifndef DBUG_OFF
  char	*env;
  if (_db_on_)
    return;					/* Already using debugging */
  if (debug)
  {
    DEBUGGER_ON;
    DBUG_PUSH(debug);
  }
  else if ((env = getenv("MYSQL_DEBUG")))
  {
    DEBUGGER_ON;
    DBUG_PUSH(env);
#if !defined(_WINVER) && !defined(WINVER)
    puts("\n-------------------------------------------------------");
    puts("MYSQL_DEBUG found. libmysql started with the following:");
    puts(env);
    puts("-------------------------------------------------------\n");
#else
    {
      char buff[80];
      strmov(strmov(buff,"libmysql: "),env);
      MessageBox((HWND) 0,"Debugging variable MYSQL_DEBUG used",buff,MB_OK);
    }
#endif
  }
#endif
}


/**************************************************************************
  Close the server connection if we get a SIGPIPE
   ARGSUSED
**************************************************************************/

static sig_handler
pipe_sig_handler(int sig __attribute__((unused)))
{
  DBUG_PRINT("info",("Hit by signal %d",sig));
#ifdef DONT_REMEMBER_SIGNAL
  (void) signal(SIGPIPE,pipe_sig_handler);
#endif
}

/* perform query on master */
my_bool STDCALL mysql_master_query(MYSQL *mysql, const char *q,
			       unsigned long length)
{
  DBUG_ENTER("mysql_master_query");
  if (mysql_master_send_query(mysql, q, length))
    DBUG_RETURN(1);
  DBUG_RETURN(mysql_read_query_result(mysql));
}

my_bool STDCALL mysql_master_send_query(MYSQL *mysql, const char *q,
					unsigned long length)
{
  MYSQL *master = mysql->master;
  DBUG_ENTER("mysql_master_send_query");
  if (!master->net.vio && !mysql_real_connect(master,0,0,0,0,0,0,0))
    DBUG_RETURN(1);
  mysql->last_used_con = master;
  DBUG_RETURN(simple_command(master, COM_QUERY, q, length, 1));
}


/* perform query on slave */
my_bool STDCALL mysql_slave_query(MYSQL *mysql, const char *q,
				  unsigned long length)
{
  DBUG_ENTER("mysql_slave_query");
  if (mysql_slave_send_query(mysql, q, length))
    DBUG_RETURN(1);
  DBUG_RETURN(mysql_read_query_result(mysql));
}


my_bool STDCALL mysql_slave_send_query(MYSQL *mysql, const char *q,
				   unsigned long length)
{
  MYSQL* last_used_slave, *slave_to_use = 0;
  DBUG_ENTER("mysql_slave_send_query");

  if ((last_used_slave = mysql->last_used_slave))
    slave_to_use = last_used_slave->next_slave;
  else
    slave_to_use = mysql->next_slave;
  /*
    Next_slave is always safe to use - we have a circular list of slaves
    if there are no slaves, mysql->next_slave == mysql
  */
  mysql->last_used_con = mysql->last_used_slave = slave_to_use;
  if (!slave_to_use->net.vio && !mysql_real_connect(slave_to_use, 0,0,0,
						   0,0,0,0))
    DBUG_RETURN(1);
  DBUG_RETURN(simple_command(slave_to_use, COM_QUERY, q, length, 1));
}


/* enable/disable parsing of all queries to decide
   if they go on master or slave */
void STDCALL mysql_enable_rpl_parse(MYSQL* mysql)
{
  mysql->options.rpl_parse = 1;
}

void STDCALL mysql_disable_rpl_parse(MYSQL* mysql)
{
  mysql->options.rpl_parse = 0;
}

/* get the value of the parse flag */
int STDCALL mysql_rpl_parse_enabled(MYSQL* mysql)
{
  return mysql->options.rpl_parse;
}

/*  enable/disable reads from master */
void STDCALL mysql_enable_reads_from_master(MYSQL* mysql)
{
  mysql->options.no_master_reads = 0;
}

void STDCALL mysql_disable_reads_from_master(MYSQL* mysql)
{
  mysql->options.no_master_reads = 1;
}

/* get the value of the master read flag */
my_bool STDCALL mysql_reads_from_master_enabled(MYSQL* mysql)
{
  return !(mysql->options.no_master_reads);
}


/*
  We may get an error while doing replication internals.
  In this case, we add a special explanation to the original
  error
*/

static void expand_error(MYSQL* mysql, int error)
{
  char tmp[MYSQL_ERRMSG_SIZE];
  char *p;
  uint err_length;
  strmake(tmp, mysql->net.last_error, MYSQL_ERRMSG_SIZE-1);
  p = strmake(mysql->net.last_error, ER(error), MYSQL_ERRMSG_SIZE-1);
  err_length= (uint) (p - mysql->net.last_error);
  strmake(p, tmp, MYSQL_ERRMSG_SIZE-1 - err_length);
  mysql->net.last_errno = error;
}

/*
  This function assumes we have just called SHOW SLAVE STATUS and have
  read the given result and row
*/

static my_bool get_master(MYSQL* mysql, MYSQL_RES* res, MYSQL_ROW row)
{
  MYSQL* master;
  DBUG_ENTER("get_master");
  if (mysql_num_fields(res) < 3)
    DBUG_RETURN(1); /* safety */

  /* use the same username and password as the original connection */
  if (!(master = spawn_init(mysql, row[0], atoi(row[2]), 0, 0)))
    DBUG_RETURN(1);
  mysql->master = master;
  DBUG_RETURN(0);
}


/*
  Assuming we already know that mysql points to a master connection,
  retrieve all the slaves
*/

static my_bool get_slaves_from_master(MYSQL* mysql)
{
  MYSQL_RES* res = 0;
  MYSQL_ROW row;
  my_bool error = 1;
  int has_auth_info;
  int port_ind;
  DBUG_ENTER("get_slaves_from_master");

  if (!mysql->net.vio && !mysql_real_connect(mysql,0,0,0,0,0,0,0))
  {
    expand_error(mysql, CR_PROBE_MASTER_CONNECT);
    DBUG_RETURN(1);
  }

  if (mysql_query(mysql, "SHOW SLAVE HOSTS") ||
      !(res = mysql_store_result(mysql)))
  {
    expand_error(mysql, CR_PROBE_SLAVE_HOSTS);
    DBUG_RETURN(1);
  }

  switch (mysql_num_fields(res)) {
  case 5:
    has_auth_info = 0;
    port_ind=2;
    break;
  case 7:
    has_auth_info = 1;
    port_ind=4;
    break;
  default:
    goto err;
  }

  while ((row = mysql_fetch_row(res)))
  {
    MYSQL* slave;
    const char* tmp_user, *tmp_pass;

    if (has_auth_info)
    {
      tmp_user = row[2];
      tmp_pass = row[3];
    }
    else
    {
      tmp_user = mysql->user;
      tmp_pass = mysql->passwd;
    }

    if (!(slave = spawn_init(mysql, row[1], atoi(row[port_ind]),
			    tmp_user, tmp_pass)))
      goto err;

    /* Now add slave into the circular linked list */
    slave->next_slave = mysql->next_slave;
    mysql->next_slave = slave;
  }
  error = 0;
err:
  if (res)
   mysql_free_result(res);
  DBUG_RETURN(error);
}


my_bool STDCALL mysql_rpl_probe(MYSQL* mysql)
{
  MYSQL_RES *res= 0;
  MYSQL_ROW row;
  my_bool error= 1;
  DBUG_ENTER("mysql_rpl_probe");

  /*
    First determine the replication role of the server we connected to
    the most reliable way to do this is to run SHOW SLAVE STATUS and see
    if we have a non-empty master host. This is still not fool-proof -
    it is not a sin to have a master that has a dormant slave thread with
    a non-empty master host. However, it is more reliable to check
    for empty master than whether the slave thread is actually running
  */
  if (mysql_query(mysql, "SHOW SLAVE STATUS") ||
      !(res = mysql_store_result(mysql)))
  {
    expand_error(mysql, CR_PROBE_SLAVE_STATUS);
    DBUG_RETURN(1);
  }

  row= mysql_fetch_row(res);
  /*
    Check master host for emptiness/NULL
    For MySQL 4.0 it's enough to check for row[0]
  */
  if (row && row[0] && *(row[0]))
  {
    /* this is a slave, ask it for the master */
    if (get_master(mysql, res, row) || get_slaves_from_master(mysql))
      goto err;
  }
  else
  {
    mysql->master = mysql;
    if (get_slaves_from_master(mysql))
      goto err;
  }

  error = 0;
err:
  if (res)
    mysql_free_result(res);
  DBUG_RETURN(error);
}


/*
  Make a not so fool-proof decision on where the query should go, to
  the master or the slave. Ideally the user should always make this
  decision himself with mysql_master_query() or mysql_slave_query().
  However, to be able to more easily port the old code, we support the
  option of an educated guess - this should work for most applications,
  however, it may make the wrong decision in some particular cases. If
  that happens, the user would have to change the code to call
  mysql_master_query() or mysql_slave_query() explicitly in the place
  where we have made the wrong decision
*/

enum mysql_rpl_type
STDCALL mysql_rpl_query_type(const char* q, int len)
{
  const char *q_end= q + len;
  for (; q < q_end; ++q)
  {
    char c;
    if (my_isalpha(&my_charset_latin1, (c= *q)))
    {
      switch (my_tolower(&my_charset_latin1,c)) {
      case 'i':  /* insert */
      case 'u':  /* update or unlock tables */
      case 'l':  /* lock tables or load data infile */
      case 'd':  /* drop or delete */
      case 'a':  /* alter */
	return MYSQL_RPL_MASTER;
      case 'c':  /* create or check */
	return my_tolower(&my_charset_latin1,q[1]) == 'h' ? MYSQL_RPL_ADMIN :
	  MYSQL_RPL_MASTER;
      case 's': /* select or show */
	return my_tolower(&my_charset_latin1,q[1]) == 'h' ? MYSQL_RPL_ADMIN :
	  MYSQL_RPL_SLAVE;
      case 'f': /* flush */
      case 'r': /* repair */
      case 'g': /* grant */
	return MYSQL_RPL_ADMIN;
      default:
	return MYSQL_RPL_SLAVE;
      }
    }
  }
  return MYSQL_RPL_MASTER;		/* By default, send to master */
}

/*
  Fill in SSL part of MYSQL structure and set 'use_ssl' flag.
  NB! Errors are not reported until you do mysql_real_connect.
*/

#define strdup_if_not_null(A) (A) == 0 ? 0 : my_strdup((A),MYF(MY_WME))

my_bool STDCALL
mysql_ssl_set(MYSQL *mysql __attribute__((unused)) ,
	      const char *key __attribute__((unused)),
	      const char *cert __attribute__((unused)),
	      const char *ca __attribute__((unused)),
	      const char *capath __attribute__((unused)),
	      const char *cipher __attribute__((unused)))
{
#ifdef HAVE_OPENSSL
  mysql->options.ssl_key=    strdup_if_not_null(key);
  mysql->options.ssl_cert=   strdup_if_not_null(cert);
  mysql->options.ssl_ca=     strdup_if_not_null(ca);
  mysql->options.ssl_capath= strdup_if_not_null(capath);
  mysql->options.ssl_cipher= strdup_if_not_null(cipher);
#endif /* HAVE_OPENSSL */
  return 0;
}

/**************************************************************************
  Connect to sql server
  If host == 0 then use localhost
**************************************************************************/

#ifdef USE_OLD_FUNCTIONS
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
#endif

/**************************************************************************
  Change user and database
**************************************************************************/

my_bool	STDCALL mysql_change_user(MYSQL *mysql, const char *user,
				  const char *passwd, const char *db)
{
  char buff[512],*end=buff;
  DBUG_ENTER("mysql_change_user");

  if (!user)
    user="";
  if (!passwd)
    passwd="";

   /* Store user into the buffer */
  end=strmov(end,user)+1;

  /*
    We always start with old type handshake the only difference is message sent
    If server handles secure connection type we'll not send the real scramble
  */
  if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    if (passwd[0])
    {
      /* Prepare false scramble  */
      bfill(end, SCRAMBLE_LENGTH, 'x');
      end+=SCRAMBLE_LENGTH;
      *end=0;

    }
    else  /* For empty password*/
      *end=0; /* Store zero length scramble */
  }
  else
  {
   /*
     Real scramble is only sent to old servers. This can be blocked 
     by calling mysql_options(MYSQL *, MYSQL_SECURE_CONNECT, (char*) &1);
   */
    end=scramble(end, mysql->scramble_buff, passwd,
                 (my_bool) (mysql->protocol_version == 9));
  }
  /* Add database if needed */
  end=strmov(end+1,db ? db : "");

  /* Write authentication package */
  simple_command(mysql,COM_CHANGE_USER, buff,(ulong) (end-buff),1);

  if (mysql_autenticate(mysql, passwd))
    goto error;

  /* Free old connect information */
  my_free(mysql->user,MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->passwd,MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));

  /* alloc new connect information */
  mysql->user=  my_strdup(user,MYF(MY_WME));
  mysql->passwd=my_strdup(passwd,MYF(MY_WME));
  mysql->db=    db ? my_strdup(db,MYF(MY_WME)) : 0;
  DBUG_RETURN(0);

error:
  DBUG_RETURN(1);
}

/**************************************************************************
  Do a query. If query returned rows, free old rows.
  Read data by mysql_store_result or by repeat call of mysql_fetch_row
**************************************************************************/

int STDCALL
mysql_query(MYSQL *mysql, const char *query)
{
  return mysql_real_query(mysql,query, (uint) strlen(query));
}


static MYSQL* spawn_init(MYSQL* parent, const char* host,
			 unsigned int port, const char* user,
			 const char* passwd)
{
  MYSQL* child;
  if (!(child = mysql_init(0)))
    return 0;

  child->options.user = my_strdup((user) ? user :
				  (parent->user ? parent->user :
				   parent->options.user), MYF(0));
  child->options.password = my_strdup((passwd) ? passwd :
				      (parent->passwd ?
				       parent->passwd :
				       parent->options.password), MYF(0));
  child->options.port = port;
  child->options.host = my_strdup((host) ? host :
				  (parent->host ?
				   parent->host :
				   parent->options.host), MYF(0));
  if (parent->db)
    child->options.db = my_strdup(parent->db, MYF(0));
  else if (parent->options.db)
    child->options.db = my_strdup(parent->options.db, MYF(0));

  child->options.rpl_parse = child->options.rpl_probe = child->rpl_pivot = 0;

  return child;
}


int
STDCALL mysql_set_master(MYSQL* mysql, const char* host,
			 unsigned int port, const char* user,
			 const char* passwd)
{
  if (mysql->master != mysql && !mysql->master->rpl_pivot)
    mysql_close(mysql->master);
  if (!(mysql->master = spawn_init(mysql, host, port, user, passwd)))
    return 1;
  mysql->master->rpl_pivot = 0;
  mysql->master->options.rpl_parse = 0;
  mysql->master->options.rpl_probe = 0;
  return 0;
}

int
STDCALL mysql_add_slave(MYSQL* mysql, const char* host,
					   unsigned int port,
					   const char* user,
					   const char* passwd)
{
  MYSQL* slave;
  if (!(slave = spawn_init(mysql, host, port, user, passwd)))
    return 1;
  slave->next_slave = mysql->next_slave;
  mysql->next_slave = slave;
  return 0;
}

/**************************************************************************
  Alloc struct for use with unbuffered reads. Data is fetched by domand
  when calling to mysql_fetch_row.
  mysql_data_seek is a noop.

  No other queries may be specified with the same MYSQL handle.
  There shouldn't be much processing per row because mysql server shouldn't
  have to wait for the client (and will not wait more than 30 sec/packet).
**************************************************************************/

MYSQL_RES * STDCALL
mysql_use_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_use_result");

  mysql = mysql->last_used_con;

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    strmov(mysql->net.sqlstate, unknown_sqlstate);
    strmov(mysql->net.last_error,
	   ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    DBUG_RETURN(0);
  }
  if (!(result=(MYSQL_RES*) my_malloc(sizeof(*result)+
				      sizeof(ulong)*mysql->field_count,
				      MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(0);
  result->lengths=(ulong*) (result+1);
  if (!(result->row=(MYSQL_ROW)
	my_malloc(sizeof(result->row[0])*(mysql->field_count+1), MYF(MY_WME))))
  {					/* Ptrs: to one row */
    my_free((gptr) result,MYF(0));
    DBUG_RETURN(0);
  }
  result->fields=	mysql->fields;
  result->field_alloc=	mysql->field_alloc;
  result->field_count=	mysql->field_count;
  result->current_field=0;
  result->handle=	mysql;
  result->current_row=	0;
  mysql->fields=0;			/* fields is now in result */
  mysql->status=MYSQL_STATUS_USE_RESULT;
  DBUG_RETURN(result);			/* Data is read to be fetched */
}



/**************************************************************************
  Return next field of the query results
**************************************************************************/

MYSQL_FIELD * STDCALL
mysql_fetch_field(MYSQL_RES *result)
{
  if (result->current_field >= result->field_count)
    return(NULL);
  return &result->fields[result->current_field++];
}

ulong * STDCALL
mysql_fetch_lengths(MYSQL_RES *res)
{
  MYSQL_ROW column;

  if (!(column=res->current_row))
    return 0;					/* Something is wrong */
  if (res->data)
    fetch_lengths(res->lengths, column, res->field_count);
  return res->lengths;
}

/*************************************************************************
  put the row or field cursor one a position one got from mysql_row_tell()
  This doesn't restore any data. The next mysql_fetch_row or
  mysql_fetch_field will return the next row or field after the last used
*************************************************************************/

MYSQL_ROW_OFFSET STDCALL
mysql_row_seek(MYSQL_RES *result, MYSQL_ROW_OFFSET row)
{
  MYSQL_ROW_OFFSET return_value=result->data_cursor;
  result->current_row= 0;
  result->data_cursor= row;
  return return_value;
}


MYSQL_FIELD_OFFSET STDCALL
mysql_field_seek(MYSQL_RES *result, MYSQL_FIELD_OFFSET field_offset)
{
  MYSQL_FIELD_OFFSET return_value=result->current_field;
  result->current_field=field_offset;
  return return_value;
}

/*****************************************************************************
  List all databases
*****************************************************************************/

MYSQL_RES * STDCALL
mysql_list_dbs(MYSQL *mysql, const char *wild)
{
  char buff[255];
  DBUG_ENTER("mysql_list_dbs");

  append_wild(strmov(buff,"show databases"),buff+sizeof(buff),wild);
  if (mysql_query(mysql,buff))
    DBUG_RETURN(0);
  DBUG_RETURN (mysql_store_result(mysql));
}


/*****************************************************************************
  List all tables in a database
  If wild is given then only the tables matching wild is returned
*****************************************************************************/

MYSQL_RES * STDCALL
mysql_list_tables(MYSQL *mysql, const char *wild)
{
  char buff[255];
  DBUG_ENTER("mysql_list_tables");

  append_wild(strmov(buff,"show tables"),buff+sizeof(buff),wild);
  if (mysql_query(mysql,buff))
    DBUG_RETURN(0);
  DBUG_RETURN (mysql_store_result(mysql));
}


/**************************************************************************
  List all fields in a table
  If wild is given then only the fields matching wild is returned
  Instead of this use query:
  show fields in 'table' like "wild"
**************************************************************************/

MYSQL_RES * STDCALL
mysql_list_fields(MYSQL *mysql, const char *table, const char *wild)
{
  MYSQL_RES *result;
  MYSQL_DATA *query;
  char	     buff[257],*end;
  DBUG_ENTER("mysql_list_fields");
  DBUG_PRINT("enter",("table: '%s'  wild: '%s'",table,wild ? wild : ""));

  LINT_INIT(query);

  end=strmake(strmake(buff, table,128)+1,wild ? wild : "",128);
  if (simple_command(mysql,COM_FIELD_LIST,buff,(ulong) (end-buff),1) ||
      !(query = read_rows(mysql,(MYSQL_FIELD*) 0, 
			  protocol_41(mysql) ? 7 : 6)))
    DBUG_RETURN(NULL);

  free_old_query(mysql);
  if (!(result = (MYSQL_RES *) my_malloc(sizeof(MYSQL_RES),
					 MYF(MY_WME | MY_ZEROFILL))))
  {
    free_rows(query);
    DBUG_RETURN(NULL);
  }
  result->field_alloc=mysql->field_alloc;
  mysql->fields=0;
  result->field_count = (uint) query->rows;
  result->fields= unpack_fields(query,&result->field_alloc,
				result->field_count, 1,
				mysql->server_capabilities);
  result->eof=1;
  DBUG_RETURN(result);
}


/* List all running processes (threads) in server */

MYSQL_RES * STDCALL
mysql_list_processes(MYSQL *mysql)
{
  MYSQL_DATA *fields;
  uint field_count;
  uchar *pos;
  DBUG_ENTER("mysql_list_processes");

  LINT_INIT(fields);
  if (simple_command(mysql,COM_PROCESS_INFO,0,0,0))
    DBUG_RETURN(0);
  free_old_query(mysql);
  pos=(uchar*) mysql->net.read_pos;
  field_count=(uint) net_field_length(&pos);
  if (!(fields = read_rows(mysql,(MYSQL_FIELD*) 0,
			   protocol_41(mysql) ? 6 : 5)))
    DBUG_RETURN(NULL);
  if (!(mysql->fields=unpack_fields(fields,&mysql->field_alloc,field_count,0,
				    mysql->server_capabilities)))
    DBUG_RETURN(0);
  mysql->status=MYSQL_STATUS_GET_RESULT;
  mysql->field_count=field_count;
  DBUG_RETURN(mysql_store_result(mysql));
}


#ifdef USE_OLD_FUNCTIONS
int  STDCALL
mysql_create_db(MYSQL *mysql, const char *db)
{
  DBUG_ENTER("mysql_createdb");
  DBUG_PRINT("enter",("db: %s",db));
  DBUG_RETURN(simple_command(mysql,COM_CREATE_DB,db, (ulong) strlen(db),0));
}


int  STDCALL
mysql_drop_db(MYSQL *mysql, const char *db)
{
  DBUG_ENTER("mysql_drop_db");
  DBUG_PRINT("enter",("db: %s",db));
  DBUG_RETURN(simple_command(mysql,COM_DROP_DB,db,(ulong) strlen(db),0));
}
#endif


int STDCALL
mysql_shutdown(MYSQL *mysql)
{
  DBUG_ENTER("mysql_shutdown");
  DBUG_RETURN(simple_command(mysql,COM_SHUTDOWN,0,0,0));
}


int STDCALL
mysql_refresh(MYSQL *mysql,uint options)
{
  uchar bits[1];
  DBUG_ENTER("mysql_refresh");
  bits[0]= (uchar) options;
  DBUG_RETURN(simple_command(mysql,COM_REFRESH,(char*) bits,1,0));
}

int STDCALL
mysql_kill(MYSQL *mysql,ulong pid)
{
  char buff[12];
  DBUG_ENTER("mysql_kill");
  int4store(buff,pid);
  DBUG_RETURN(simple_command(mysql,COM_PROCESS_KILL,buff,4,0));
}


int STDCALL
mysql_dump_debug_info(MYSQL *mysql)
{
  DBUG_ENTER("mysql_dump_debug_info");
  DBUG_RETURN(simple_command(mysql,COM_DEBUG,0,0,0));
}

const char * STDCALL
mysql_stat(MYSQL *mysql)
{
  DBUG_ENTER("mysql_stat");
  if (simple_command(mysql,COM_STATISTICS,0,0,0))
    return mysql->net.last_error;
  mysql->net.read_pos[mysql->packet_length]=0;	/* End of stat string */
  if (!mysql->net.read_pos[0])
  {
    strmov(mysql->net.sqlstate, unknown_sqlstate);
    mysql->net.last_errno=CR_WRONG_HOST_INFO;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    return mysql->net.last_error;
  }
  DBUG_RETURN((char*) mysql->net.read_pos);
}


int STDCALL
mysql_ping(MYSQL *mysql)
{
  DBUG_ENTER("mysql_ping");
  DBUG_RETURN(simple_command(mysql,COM_PING,0,0,0));
}


const char * STDCALL
mysql_get_server_info(MYSQL *mysql)
{
  return((char*) mysql->server_version);
}


/*
  Get version number for server in a form easy to test on

  SYNOPSIS
    mysql_get_server_version()
    mysql		Connection

  EXAMPLE
    4.1.0-alfa ->  40100
  
  NOTES
    We will ensure that a newer server always has a bigger number.

  RETURN
   Signed number > 323000
*/

ulong STDCALL
mysql_get_server_version(MYSQL *mysql)
{
  uint major, minor, version;
  char *pos= mysql->server_version, *end_pos;
  major=   (uint) strtoul(pos, &end_pos, 10);	pos=end_pos+1;
  minor=   (uint) strtoul(pos, &end_pos, 10);	pos=end_pos+1;
  version= (uint) strtoul(pos, &end_pos, 10);
  return (ulong) major*10000L+(ulong) (minor*100+version);
}


const char * STDCALL
mysql_get_host_info(MYSQL *mysql)
{
  return(mysql->host_info);
}


uint STDCALL
mysql_get_proto_info(MYSQL *mysql)
{
  return (mysql->protocol_version);
}

const char * STDCALL
mysql_get_client_info(void)
{
  return (char*) MYSQL_SERVER_VERSION;
}


int STDCALL
mysql_options(MYSQL *mysql,enum mysql_option option, const char *arg)
{
  DBUG_ENTER("mysql_option");
  DBUG_PRINT("enter",("option: %d",(int) option));
  switch (option) {
  case MYSQL_OPT_CONNECT_TIMEOUT:
    mysql->options.connect_timeout= *(uint*) arg;
    break;
  case MYSQL_OPT_COMPRESS:
    mysql->options.compress= 1;			/* Remember for connect */
    mysql->options.client_flag|= CLIENT_COMPRESS;
    break;
  case MYSQL_OPT_NAMED_PIPE:
    mysql->options.protocol=MYSQL_PROTOCOL_PIPE; /* Force named pipe */
    break;
  case MYSQL_OPT_LOCAL_INFILE:			/* Allow LOAD DATA LOCAL ?*/
    if (!arg || test(*(uint*) arg))
      mysql->options.client_flag|= CLIENT_LOCAL_FILES;
    else
      mysql->options.client_flag&= ~CLIENT_LOCAL_FILES;
    break;
  case MYSQL_INIT_COMMAND:
    add_init_command(&mysql->options,arg);
    break;
  case MYSQL_READ_DEFAULT_FILE:
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.my_cnf_file=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_READ_DEFAULT_GROUP:
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.my_cnf_group=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_SET_CHARSET_DIR:
    my_free(mysql->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.charset_dir=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_SET_CHARSET_NAME:
    my_free(mysql->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.charset_name=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_OPT_PROTOCOL:
    mysql->options.protocol= *(uint*) arg;
    break;
  case MYSQL_SHARED_MEMORY_BASE_NAME:
#ifdef HAVE_SMEM
    if (mysql->options.shared_memory_base_name != def_shared_memory_base_name)
      my_free(mysql->options.shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.shared_memory_base_name=my_strdup(arg,MYF(MY_WME));
#endif
    break;
  default:
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

my_bool STDCALL mysql_eof(MYSQL_RES *res)
{
  return res->eof;
}

MYSQL_FIELD * STDCALL mysql_fetch_field_direct(MYSQL_RES *res,uint fieldnr)
{
  return &(res)->fields[fieldnr];
}

MYSQL_FIELD * STDCALL mysql_fetch_fields(MYSQL_RES *res)
{
  return (res)->fields;
}

MYSQL_ROW_OFFSET STDCALL mysql_row_tell(MYSQL_RES *res)
{
  return res->data_cursor;
}

MYSQL_FIELD_OFFSET STDCALL mysql_field_tell(MYSQL_RES *res)
{
  return (res)->current_field;
}

/* MYSQL */

unsigned int STDCALL mysql_field_count(MYSQL *mysql)
{
  return mysql->last_used_con->field_count;
}

my_ulonglong STDCALL mysql_affected_rows(MYSQL *mysql)
{
  return mysql->last_used_con->affected_rows;
}

my_ulonglong STDCALL mysql_insert_id(MYSQL *mysql)
{
  return mysql->last_used_con->insert_id;
}

uint STDCALL mysql_errno(MYSQL *mysql)
{
  return mysql->net.last_errno;
}

const char *STDCALL mysql_sqlstate(MYSQL *mysql)
{
  return mysql->net.sqlstate;
}

const char * STDCALL mysql_error(MYSQL *mysql)
{
  return mysql->net.last_error;
}

uint STDCALL mysql_warning_count(MYSQL *mysql)
{
  return mysql->warning_count;
}

const char *STDCALL mysql_info(MYSQL *mysql)
{
  return mysql->info;
}

ulong STDCALL mysql_thread_id(MYSQL *mysql)
{
  return (mysql)->thread_id;
}

const char * STDCALL mysql_character_set_name(MYSQL *mysql)
{
  return mysql->charset->name;
}


uint STDCALL mysql_thread_safe(void)
{
#ifdef THREAD
  return 1;
#else
  return 0;
#endif
}

/****************************************************************************
  Some support functions
****************************************************************************/

/*
  Functions called my my_net_init() to set some application specific variables
*/

void my_net_local_init(NET *net)
{
  net->max_packet=   (uint) net_buffer_length;
  net->read_timeout= (uint) net_read_timeout;
  net->write_timeout=(uint) net_write_timeout;
  net->retry_count=  1;
  net->max_packet_size= max(net_buffer_length, max_allowed_packet);
}

/*
  Add escape characters to a string (blob?) to make it suitable for a insert
  to should at least have place for length*2+1 chars
  Returns the length of the to string
*/

ulong STDCALL
mysql_escape_string(char *to,const char *from,ulong length)
{
  return mysql_sub_escape_string(default_charset_info,to,from,length);
}

ulong STDCALL
mysql_real_escape_string(MYSQL *mysql, char *to,const char *from,
			 ulong length)
{
  return mysql_sub_escape_string(mysql->charset,to,from,length);
}


static ulong
mysql_sub_escape_string(CHARSET_INFO *charset_info, char *to,
			const char *from, ulong length)
{
  const char *to_start=to;
  const char *end;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(charset_info);
#endif
  for (end=from+length; from != end ; from++)
  {
#ifdef USE_MB
    int l;
    if (use_mb_flag && (l = my_ismbchar(charset_info, from, end)))
    {
      while (l--)
	  *to++ = *from++;
      from--;
      continue;
    }
#endif
    switch (*from) {
    case 0:				/* Must be escaped for 'mysql' */
      *to++= '\\';
      *to++= '0';
      break;
    case '\n':				/* Must be escaped for logs */
      *to++= '\\';
      *to++= 'n';
      break;
    case '\r':
      *to++= '\\';
      *to++= 'r';
      break;
    case '\\':
      *to++= '\\';
      *to++= '\\';
      break;
    case '\'':
      *to++= '\\';
      *to++= '\'';
      break;
    case '"':				/* Better safe than sorry */
      *to++= '\\';
      *to++= '"';
      break;
    case '\032':			/* This gives problems on Win32 */
      *to++= '\\';
      *to++= 'Z';
      break;
    default:
      *to++= *from;
    }
  }
  *to=0;
  return (ulong) (to-to_start);
}


char * STDCALL
mysql_odbc_escape_string(MYSQL *mysql,
			 char *to, ulong to_length,
			 const char *from, ulong from_length,
			 void *param,
			 char * (*extend_buffer)
			 (void *, char *, ulong *))
{
  char *to_end=to+to_length-5;
  const char *end;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(mysql->charset);
#endif

  for (end=from+from_length; from != end ; from++)
  {
    if (to >= to_end)
    {
      to_length = (ulong) (end-from)+512;	/* We want this much more */
      if (!(to=(*extend_buffer)(param, to, &to_length)))
	return to;
      to_end=to+to_length-5;
    }
#ifdef USE_MB
    {
      int l;
      if (use_mb_flag && (l = my_ismbchar(mysql->charset, from, end)))
      {
	while (l--)
	  *to++ = *from++;
	from--;
	continue;
      }
    }
#endif
    switch (*from) {
    case 0:				/* Must be escaped for 'mysql' */
      *to++= '\\';
      *to++= '0';
      break;
    case '\n':				/* Must be escaped for logs */
      *to++= '\\';
      *to++= 'n';
      break;
    case '\r':
      *to++= '\\';
      *to++= 'r';
      break;
    case '\\':
      *to++= '\\';
      *to++= '\\';
      break;
    case '\'':
      *to++= '\\';
      *to++= '\'';
      break;
    case '"':				/* Better safe than sorry */
      *to++= '\\';
      *to++= '"';
      break;
    case '\032':			/* This gives problems on Win32 */
      *to++= '\\';
      *to++= 'Z';
      break;
    default:
      *to++= *from;
    }
  }
  return to;
}

void STDCALL
myodbc_remove_escape(MYSQL *mysql,char *name)
{
  char *to;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(mysql->charset);
  char *end;
  LINT_INIT(end);
  if (use_mb_flag)
    for (end=name; *end ; end++) ;
#endif

  for (to=name ; *name ; name++)
  {
#ifdef USE_MB
    int l;
    if (use_mb_flag && (l = my_ismbchar( mysql->charset, name , end ) ) )
    {
      while (l--)
	*to++ = *name++;
      name--;
      continue;
    }
#endif
    if (*name == '\\' && name[1])
      name++;
    *to++= *name;
  }
  *to=0;
}

/********************************************************************

 Implementation of new client-server prototypes for 4.1 version
 starts from here ..

 mysql_* are real prototypes used by applications

*********************************************************************/

/********************************************************************
 Misc Utility functions
********************************************************************/

/*
  Set the internal stmt error messages
*/

static void set_stmt_error(MYSQL_STMT * stmt, int errcode,
			   const char *sqlstate)
{
  DBUG_ENTER("set_stmt_error");
  DBUG_PRINT("enter", ("error: %d '%s'", errcode, ER(errcode)));
  DBUG_ASSERT(stmt != 0);

  stmt->last_errno= errcode;
  strmov(stmt->last_error, ER(errcode));
  strmov(stmt->sqlstate, sqlstate);

  DBUG_VOID_RETURN;
}


/*
  Copy error message to statement handler
*/

static void set_stmt_errmsg(MYSQL_STMT * stmt, const char *err, int errcode,
			    const char *sqlstate)
{
  DBUG_ENTER("set_stmt_error_msg");
  DBUG_PRINT("enter", ("error: %d/%s '%s'", errcode, sqlstate, err));
  DBUG_ASSERT(stmt != 0);

  stmt->last_errno= errcode;
  if (err && err[0])
    strmov(stmt->last_error, err);
  strmov(stmt->sqlstate, sqlstate);

  DBUG_VOID_RETURN;
}


/*
  Set the internal error message to mysql handler
*/

static void set_mysql_error(MYSQL * mysql, int errcode, const char *sqlstate)
{
  DBUG_ENTER("set_mysql_error");
  DBUG_PRINT("enter", ("error :%d '%s'", errcode, ER(errcode)));
  DBUG_ASSERT(mysql != 0);

  mysql->net.last_errno= errcode;
  strmov(mysql->net.last_error, ER(errcode));
  strmov(mysql->net.sqlstate, sqlstate);
}


/*
  Reallocate the NET package to be at least of 'length' bytes

  SYNPOSIS
   my_realloc_str()
   net			The NET structure to modify
   int length		Ensure that net->buff is at least this big

  RETURN VALUES
  0	ok
  1	Error

*/

static my_bool my_realloc_str(NET *net, ulong length)
{
  ulong buf_length= (ulong) (net->write_pos - net->buff);
  my_bool res=0;
  DBUG_ENTER("my_realloc_str");
  if (buf_length + length > net->max_packet)
  {
    res= net_realloc(net, buf_length + length);
    net->write_pos= net->buff+ buf_length;
  }
  DBUG_RETURN(res);
}

/********************************************************************
  Prepare related implementations
********************************************************************/

/*
  Read the prepare statement results ..

  NOTE
    This is only called for connection to servers that supports
    prepared statements (and thus the 4.1 protocol)

  RETURN VALUES
    0	ok
    1	error
*/

static my_bool read_prepare_result(MYSQL *mysql, MYSQL_STMT *stmt)
{
  uchar *pos;
  uint field_count;
  ulong length, param_count;
  MYSQL_DATA *fields_data;
  DBUG_ENTER("read_prepare_result");

  mysql= mysql->last_used_con;
  if ((length= net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(1);

  pos= (uchar*) mysql->net.read_pos;
  stmt->stmt_id= uint4korr(pos+1); pos+= 5;
  field_count=   uint2korr(pos);   pos+= 2;
  param_count=   uint2korr(pos);   pos+= 2;

  if (field_count != 0)
  {
    if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
      mysql->server_status|= SERVER_STATUS_IN_TRANS;

    mysql->extra_info= net_field_length_ll(&pos);
    if (!(fields_data= read_rows(mysql, (MYSQL_FIELD*) 0, 9)))
      DBUG_RETURN(1);
    if (!(stmt->fields= unpack_fields(fields_data,&stmt->mem_root,
				      field_count,0,
				      mysql->server_capabilities)))
      DBUG_RETURN(1);
  }
  if (!(stmt->params= (MYSQL_BIND *) alloc_root(&stmt->mem_root,
						sizeof(MYSQL_BIND)*
                                                (param_count + 
                                                 field_count))))
  {
    set_stmt_error(stmt, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  stmt->bind=	      (stmt->params + param_count);
  stmt->field_count=  (uint) field_count;
  stmt->param_count=  (ulong) param_count;
  mysql->status=      MYSQL_STATUS_READY;
  DBUG_RETURN(0);
}


/*
  Prepare the query and return the new statement handle to
  caller.

  Also update the total parameter count along with resultset
  metadata information by reading from server
*/


MYSQL_STMT *STDCALL
mysql_prepare(MYSQL  *mysql, const char *query, ulong length)
{
  MYSQL_STMT  *stmt;
  DBUG_ENTER("mysql_prepare");
  DBUG_ASSERT(mysql != 0);

#ifdef CHECK_EXTRA_ARGUMENTS
  if (!query)
  {
    set_mysql_error(mysql, CR_NULL_POINTER, unknown_sqlstate);
    DBUG_RETURN(0);
  }
#endif

  if (!(stmt= (MYSQL_STMT *) my_malloc(sizeof(MYSQL_STMT),
				       MYF(MY_WME | MY_ZEROFILL))) ||
      !(stmt->query= my_strdup_with_length((byte *) query, length, MYF(0))))
  {
    my_free((gptr) stmt, MYF(MY_ALLOW_ZERO_PTR));
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  if (simple_command(mysql, COM_PREPARE, query, length, 1))
  {
    stmt_close(stmt, 1);
    DBUG_RETURN(0);
  }

  init_alloc_root(&stmt->mem_root,8192,0);
  if (read_prepare_result(mysql, stmt))
  {
    stmt_close(stmt, 1);
    DBUG_RETURN(0);
  }
  stmt->state= MY_ST_PREPARE;
  stmt->mysql= mysql;
  mysql->stmts= list_add(mysql->stmts, &stmt->list);
  stmt->list.data= stmt;
  DBUG_PRINT("info", ("Parameter count: %ld", stmt->param_count));
  DBUG_RETURN(stmt);
}

/*
  Get the execute query meta information for non-select 
  statements (on demand).
*/

unsigned int alloc_stmt_fields(MYSQL_STMT *stmt)
{
  MYSQL_FIELD *fields, *field, *end;
  MEM_ROOT *alloc= &stmt->mem_root;
  MYSQL *mysql= stmt->mysql->last_used_con;
  
  if (stmt->state != MY_ST_EXECUTE || !mysql->field_count)
    return 0;
  
  stmt->field_count= mysql->field_count;
  
  /*
    Get the field information for non-select statements 
    like SHOW and DESCRIBE commands
  */
  if (!(stmt->fields= (MYSQL_FIELD *) alloc_root(alloc, 
        sizeof(MYSQL_FIELD) * stmt->field_count)) || 
      !(stmt->bind= (MYSQL_BIND *) alloc_root(alloc, 
        sizeof(MYSQL_BIND ) * stmt->field_count)))
    return 0;
  
  for (fields= mysql->fields, end= fields+stmt->field_count, 
       field= stmt->fields;
       field && fields < end; fields++, field++)
  {
    field->db       = strdup_root(alloc,fields->db);
    field->table    = strdup_root(alloc,fields->table);
    field->org_table= strdup_root(alloc,fields->org_table);
    field->name     = strdup_root(alloc,fields->name);
    field->org_name = strdup_root(alloc,fields->org_name);
    field->charsetnr= fields->charsetnr;
    field->length   = fields->length;
    field->type     = fields->type;
    field->flags    = fields->flags;
    field->decimals = fields->decimals;
    field->def      = fields->def ? strdup_root(alloc,fields->def): 0;
    field->max_length= 0;
  }
  return stmt->field_count;
}

/*
  Returns prepared meta information in the form of resultset
  to client.
*/

MYSQL_RES * STDCALL
mysql_prepare_result(MYSQL_STMT *stmt)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_prepare_result");
  
  if (!stmt->field_count || !stmt->fields)
  {
    if (!alloc_stmt_fields(stmt))
      DBUG_RETURN(0);
  }  
  if (!(result=(MYSQL_RES*) my_malloc(sizeof(*result)+
				      sizeof(ulong)*stmt->field_count,
				      MYF(MY_WME | MY_ZEROFILL))))
    return 0;

  result->eof=1;	/* Marker for buffered */
  result->fields=	stmt->fields;
  result->field_count=	stmt->field_count;
  DBUG_RETURN(result);
}

/*
  Returns parameter columns meta information in the form of 
  resultset.
*/

MYSQL_RES * STDCALL
mysql_param_result(MYSQL_STMT *stmt)
{
  DBUG_ENTER("mysql_param_result");
  
  if (!stmt->param_count)
    DBUG_RETURN(0);

  /*
    TODO: Fix this when server sends the information. 
    Till then keep a dummy prototype 
  */
  DBUG_RETURN(0); 
}



/********************************************************************
 Prepare-execute, and param handling
*********************************************************************/

/*
  Store the buffer type
*/

static void store_param_type(NET *net, uint type)
{
  int2store(net->write_pos, type);
  net->write_pos+=2;
}

/*
  Store the length of parameter data
  (Same function as in sql/net_pkg.cc)
*/

char *
net_store_length(char *pkg, ulong length)
{
  uchar *packet=(uchar*) pkg;
  if (length < 251)
  {
    *packet=(uchar) length;
    return (char*) packet+1;
  }
  /* 251 is reserved for NULL */
  if (length < 65536L)
  {
    *packet++=252;
    int2store(packet,(uint) length);
    return (char*) packet+2;
  }
  if (length < 16777216L)
  {
    *packet++=253;
    int3store(packet,(ulong) length);
    return (char*) packet+3;
  }
  *packet++=254;
  int8store(packet, (ulonglong) length);
  return (char*) packet+9;
}


/****************************************************************************
  Functions to store parameter data from a prepared statement.

  All functions has the following characteristics:

  SYNOPSIS
    store_param_xxx()
    net			MySQL NET connection
    param		MySQL bind param

  RETURN VALUES
    0	ok
    1	Error	(Can't alloc net->buffer)
****************************************************************************/


static void store_param_tinyint(NET *net, MYSQL_BIND *param)
{
  *(net->write_pos++)= (uchar) *param->buffer;
}

static void store_param_short(NET *net, MYSQL_BIND *param)
{
  short value= *(short*) param->buffer;
  int2store(net->write_pos,value);
  net->write_pos+=2;
}

static void store_param_int32(NET *net, MYSQL_BIND *param)
{
  int32 value= *(int32*) param->buffer;
  int4store(net->write_pos,value);
  net->write_pos+=4;
}

static void store_param_int64(NET *net, MYSQL_BIND *param)
{
  longlong value= *(longlong*) param->buffer;
  int8store(net->write_pos,value);
  net->write_pos+= 8;
}

static void store_param_float(NET *net, MYSQL_BIND *param)
{
  float value= *(float*) param->buffer;
  float4store(net->write_pos, value);
  net->write_pos+= 4;
}

static void store_param_double(NET *net, MYSQL_BIND *param)
{
  double value= *(double*) param->buffer;
  float8store(net->write_pos, value);
  net->write_pos+= 8;
}

static void store_param_time(NET *net, MYSQL_BIND *param)
{
  MYSQL_TIME *tm= (MYSQL_TIME *) param->buffer;
  char buff[15], *pos;
  uint length;

  pos= buff+1;
  pos[0]= tm->neg ? 1: 0;
  int4store(pos+1, tm->day);
  pos[5]= (uchar) tm->hour;
  pos[6]= (uchar) tm->minute;
  pos[7]= (uchar) tm->second;
  int4store(pos+8, tm->second_part);
  if (tm->second_part)
    length= 11;
  else if (tm->hour || tm->minute || tm->second || tm->day)
    length= 8;
  else
    length= 0;
  buff[0]= (char) length++;  
  memcpy((char *)net->write_pos, buff, length);
  net->write_pos+= length;
}

static void net_store_datetime(NET *net, MYSQL_TIME *tm)
{
  char buff[12], *pos;
  uint length;

  pos= buff+1;

  int2store(pos, tm->year);
  pos[2]= (uchar) tm->month;
  pos[3]= (uchar) tm->day;
  pos[4]= (uchar) tm->hour;
  pos[5]= (uchar) tm->minute;
  pos[6]= (uchar) tm->second;
  int4store(pos+7, tm->second_part);
  if (tm->second_part)
    length= 11;
  else if (tm->hour || tm->minute || tm->second)
    length= 7;
  else if (tm->year || tm->month || tm->day)
    length= 4;
  else
    length= 0;
  buff[0]= (char) length++;  
  memcpy((char *)net->write_pos, buff, length);
  net->write_pos+= length;
}

static void store_param_date(NET *net, MYSQL_BIND *param)
{
  MYSQL_TIME *tm= (MYSQL_TIME *) param->buffer;
  tm->hour= tm->minute= tm->second= 0;
  tm->second_part= 0;
  net_store_datetime(net, tm);
}

static void store_param_datetime(NET *net, MYSQL_BIND *param)
{
  MYSQL_TIME *tm= (MYSQL_TIME *) param->buffer;
  net_store_datetime(net, tm);
}
    
static void store_param_str(NET *net, MYSQL_BIND *param)
{
  ulong length= min(*param->length, param->buffer_length);
  char *to= (char *) net_store_length((char *) net->write_pos, length);
  memcpy(to, param->buffer, length);
  net->write_pos= (uchar*) to+length;
}


/*
  Mark if the parameter is NULL.

  SYNOPSIS
    store_param_null()
    net			MySQL NET connection
    param		MySQL bind param

  DESCRIPTION
    A data package starts with a string of bits where we set a bit
    if a parameter is NULL
*/

static void store_param_null(NET *net, MYSQL_BIND *param)
{
  uint pos= param->param_number;
  net->buff[pos/8]|=  (uchar) (1 << (pos & 7));
}


/*
  Set parameter data by reading from input buffers from the
  client application
*/


static my_bool store_param(MYSQL_STMT *stmt, MYSQL_BIND *param)
{
  MYSQL *mysql= stmt->mysql;
  NET	*net  = &mysql->net;
  DBUG_ENTER("store_param");
  DBUG_PRINT("enter",("type: %d, buffer:%lx, length: %lu  is_null: %d",
		      param->buffer_type,
		      param->buffer ? param->buffer : "0", *param->length,
		      *param->is_null));

  if (*param->is_null)
    store_param_null(net, param);
  else
  {
    /*
      Param->length should ALWAYS point to the correct length for the type
      Either to the length pointer given by the user or param->buffer_length
    */
    if ((my_realloc_str(net, 9 + *param->length)))
    {
      set_stmt_error(stmt, CR_OUT_OF_MEMORY, unknown_sqlstate);
      DBUG_RETURN(1);
    }
    (*param->store_param_func)(net, param);
  }
  DBUG_RETURN(0);
}


/*
  Send the prepare query to server for execution
*/

static my_bool execute(MYSQL_STMT * stmt, char *packet, ulong length)
{
  MYSQL *mysql= stmt->mysql;
  NET	*net= &mysql->net;
  char buff[MYSQL_STMT_HEADER];
  DBUG_ENTER("execute");
  DBUG_PRINT("enter",("packet: %s, length :%d",packet ? packet :" ", length));

  mysql->last_used_con= mysql;
  int4store(buff, stmt->stmt_id);		/* Send stmt id to server */
  if (advanced_command(mysql, COM_EXECUTE, buff, MYSQL_STMT_HEADER, packet,
		       length, 1) ||
      mysql_read_query_result(mysql))
  {
    set_stmt_errmsg(stmt, net->last_error, net->last_errno, net->sqlstate);
    DBUG_RETURN(1);
  }
  stmt->state= MY_ST_EXECUTE;
  mysql_free_result(stmt->result);
  stmt->result= (MYSQL_RES *)0;
  stmt->result_buffered= 0;
  stmt->current_row= 0;
  DBUG_RETURN(0);
}


/*
  Execute the prepare query
*/

int STDCALL mysql_execute(MYSQL_STMT *stmt)
{
  DBUG_ENTER("mysql_execute");

  if (stmt->state == MY_ST_UNKNOWN)
  {
    set_stmt_error(stmt, CR_NO_PREPARE_STMT, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  if (stmt->param_count)
  {
    NET        *net= &stmt->mysql->net;
    MYSQL_BIND *param, *param_end;
    char       *param_data;
    ulong length;
    uint null_count;
    my_bool    result;

#ifdef CHECK_EXTRA_ARGUMENTS
    if (!stmt->param_buffers)
    {
      /* Parameters exists, but no bound buffers */
      set_stmt_error(stmt, CR_NOT_ALL_PARAMS_BOUND, unknown_sqlstate);
      DBUG_RETURN(1);
    }
#endif
    net_clear(net);				/* Sets net->write_pos */
    /* Reserve place for null-marker bytes */
    null_count= (stmt->param_count+7) /8;
    bzero((char*) net->write_pos, null_count);
    net->write_pos+= null_count;
    param_end= stmt->params + stmt->param_count;

    /* In case if buffers (type) altered, indicate to server */
    *(net->write_pos)++= (uchar) stmt->send_types_to_server;
    if (stmt->send_types_to_server)
    {
      /*
	Store types of parameters in first in first package
	that is sent to the server.
      */
      for (param= stmt->params;	param < param_end ; param++)
	store_param_type(net, (uint) param->buffer_type);
    }

    for (param= stmt->params; param < param_end; param++)
    {
      /* check if mysql_long_data() was used */
      if (param->long_data_used)
	param->long_data_used= 0;	/* Clear for next execute call */
      else if (store_param(stmt, param))
	DBUG_RETURN(1);
    }
    length= (ulong) (net->write_pos - net->buff);
    /* TODO: Look into avoding the following memdup */
    if (!(param_data= my_memdup((const char*) net->buff, length, MYF(0))))
    {
      set_stmt_error(stmt, CR_OUT_OF_MEMORY, unknown_sqlstate);
      DBUG_RETURN(1);
    }
    net->write_pos= net->buff;			/* Reset for net_write() */
    result= execute(stmt, param_data, length);
    stmt->send_types_to_server=0;
    my_free(param_data, MYF(MY_WME));
    DBUG_RETURN(result);
  }
  DBUG_RETURN((int) execute(stmt,0,0));
}


/*
  Return total parameters count in the statement
*/

ulong STDCALL mysql_param_count(MYSQL_STMT * stmt)
{
  DBUG_ENTER("mysql_param_count");
  DBUG_RETURN(stmt->param_count);
}

/*
  Return total affected rows from the last statement
*/

my_ulonglong STDCALL mysql_stmt_affected_rows(MYSQL_STMT *stmt)
{
  return stmt->mysql->last_used_con->affected_rows;
}


static my_bool int_is_null_true= 1;		/* Used for MYSQL_TYPE_NULL */
static my_bool int_is_null_false= 0;
static my_bool int_is_null_dummy;
static unsigned long param_length_is_dummy;

/*
  Setup the parameter data buffers from application
*/

my_bool STDCALL mysql_bind_param(MYSQL_STMT *stmt, MYSQL_BIND * bind)
{
  uint count=0;
  MYSQL_BIND *param, *end;
  DBUG_ENTER("mysql_bind_param");

#ifdef CHECK_EXTRA_ARGUMENTS
  if (stmt->state == MY_ST_UNKNOWN)
  {
    set_stmt_error(stmt, CR_NO_PREPARE_STMT, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  if (!stmt->param_count)
  {
    set_stmt_error(stmt, CR_NO_PARAMETERS_EXISTS, unknown_sqlstate);
    DBUG_RETURN(1);
  }
#endif

  /* Allocated on prepare */
  memcpy((char*) stmt->params, (char*) bind,
	 sizeof(MYSQL_BIND) * stmt->param_count);

  for (param= stmt->params, end= param+stmt->param_count;
       param < end ;
       param++)
  {
    param->param_number= count++;
    param->long_data_used= 0;

    /*
      If param->length is not given, change it to point to buffer_length.
      This way we can always use *param->length to get the length of data
    */
    if (!param->length)
      param->length= &param->buffer_length;

    /* If param->is_null is not set, then the value can never be NULL */
    if (!param->is_null)
      param->is_null= &int_is_null_false;

    /* Setup data copy functions for the different supported types */
    switch (param->buffer_type) {
    case MYSQL_TYPE_NULL:
      param->is_null= &int_is_null_true;
      break;
    case MYSQL_TYPE_TINY:
      /* Force param->length as this is fixed for this type */
      param->length= &param->buffer_length;
      param->buffer_length= 1;
      param->store_param_func= store_param_tinyint;
      break;
    case MYSQL_TYPE_SHORT:
      param->length= &param->buffer_length;
      param->buffer_length= 2;
      param->store_param_func= store_param_short;
      break;
    case MYSQL_TYPE_LONG:
      param->length= &param->buffer_length;
      param->buffer_length= 4;
      param->store_param_func= store_param_int32;
      break;
    case MYSQL_TYPE_LONGLONG:
      param->length= &param->buffer_length;
      param->buffer_length= 8;
      param->store_param_func= store_param_int64;
      break;
    case MYSQL_TYPE_FLOAT:
      param->length= &param->buffer_length;
      param->buffer_length= 4;
      param->store_param_func= store_param_float;
      break;
    case MYSQL_TYPE_DOUBLE:
      param->length= &param->buffer_length;
      param->buffer_length= 8;
      param->store_param_func= store_param_double;
      break;
    case MYSQL_TYPE_TIME:
      /* Buffer length ignored for DATE, TIME and DATETIME */
      param->store_param_func= store_param_time;
      break;
    case MYSQL_TYPE_DATE:
      param->store_param_func= store_param_date;
      break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      param->store_param_func= store_param_datetime;
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      param->store_param_func= store_param_str;
      break;
    default:
      strmov(stmt->sqlstate, unknown_sqlstate);
      sprintf(stmt->last_error,
	      ER(stmt->last_errno= CR_UNSUPPORTED_PARAM_TYPE),
	      param->buffer_type, count);
      DBUG_RETURN(1);
    }
  }
  /* We have to send/resendtype information to MySQL */
  stmt->send_types_to_server= 1;
  stmt->param_buffers= 1;
  DBUG_RETURN(0);
}


/********************************************************************
 Long data implementation
*********************************************************************/

/*
  Send long data in pieces to the server

  SYNOPSIS
    mysql_send_long_data()
    stmt			Statement handler
    param_number		Parameter number (0 - N-1)
    data			Data to send to server
    length			Length of data to send (may be 0)

  RETURN VALUES
    0	ok
    1	error
*/


my_bool STDCALL
mysql_send_long_data(MYSQL_STMT *stmt, uint param_number,
		     const char *data, ulong length)
{
  MYSQL_BIND *param;
  DBUG_ENTER("mysql_send_long_data");
  DBUG_ASSERT(stmt != 0);
  DBUG_PRINT("enter",("param no : %d, data : %lx, length : %ld",
		      param_number, data, length));

  if (param_number >= stmt->param_count)
  {
    set_stmt_error(stmt, CR_INVALID_PARAMETER_NO, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  param= stmt->params+param_number;
  if (param->buffer_type < MYSQL_TYPE_TINY_BLOB ||
      param->buffer_type > MYSQL_TYPE_STRING)
  {
    /*
      Long data handling should be used only for string/binary
      types only
    */
    strmov(stmt->sqlstate, unknown_sqlstate);
    sprintf(stmt->last_error, ER(stmt->last_errno= CR_INVALID_BUFFER_USE),
	    param->param_number);
    DBUG_RETURN(1);
  }
  /* Mark for execute that the result is already sent */
  param->long_data_used= 1;
  if (length)
  {
    MYSQL *mysql= stmt->mysql;
    char   *packet, extra_data[MYSQL_LONG_DATA_HEADER];

    packet= extra_data;
    int4store(packet, stmt->stmt_id);	   packet+=4;
    int2store(packet, param_number);	   packet+=2;

    /*
      Note that we don't get any ok packet from the server in this case
      This is intentional to save bandwidth.
    */
    if (advanced_command(mysql, COM_LONG_DATA, extra_data,
			 MYSQL_LONG_DATA_HEADER, data, length, 1))
    {
      set_stmt_errmsg(stmt, mysql->net.last_error,
		      mysql->net.last_errno, mysql->net.sqlstate);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


/********************************************************************
  Fetch-bind related implementations
*********************************************************************/

/****************************************************************************
  Functions to fetch data to application buffers

  All functions has the following characteristics:

  SYNOPSIS
    fetch_result_xxx()
    param   MySQL bind param
    row     Row value

  RETURN VALUES
    0	ok
    1	Error	(Can't alloc net->buffer)
****************************************************************************/

static void set_zero_time(MYSQL_TIME *tm)
{
  tm->year= tm->month= tm->day= 0;
  tm->hour= tm->minute= tm->second= 0;
  tm->second_part= 0;
  tm->neg= (bool)0;
}

/* Read TIME from binary packet and return it to MYSQL_TIME */
static uint read_binary_time(MYSQL_TIME *tm, uchar **pos)
{
  uchar *to;
  uint  length;
 
  if (!(length= net_field_length(pos)))
  {
    set_zero_time(tm);
    return 0;
  }
  
  to= *pos;     
  tm->second_part= (length > 8 ) ? (ulong) sint4korr(to+7): 0;

  tm->day=    (ulong) sint4korr(to+1);
  tm->hour=   (uint) to[5];
  tm->minute= (uint) to[6];
  tm->second= (uint) to[7];

  tm->year= tm->month= 0;
  tm->neg= (bool)to[0];
  return length;
}

/* Read DATETIME from binary packet and return it to MYSQL_TIME */
static uint read_binary_datetime(MYSQL_TIME *tm, uchar **pos)
{
  uchar *to;
  uint  length;
 
  if (!(length= net_field_length(pos)))
  {
    set_zero_time(tm);
    return 0;
  }
  
  to= *pos;     
  tm->second_part= (length > 7 ) ? (ulong) sint4korr(to+7): 0;
    
  if (length > 4)
  {
    tm->hour=   (uint) to[4];
    tm->minute= (uint) to[5];
    tm->second= (uint) to[6];
  }
  else
    tm->hour= tm->minute= tm->second= 0;
    
  tm->year=   (uint) sint2korr(to);
  tm->month=  (uint) to[2];
  tm->day=    (uint) to[3];
  tm->neg=    0;
  return length;
}

/* Read DATE from binary packet and return it to MYSQL_TIME */
static uint read_binary_date(MYSQL_TIME *tm, uchar **pos)
{
  uchar *to;
  uint  length;
 
  if (!(length= net_field_length(pos)))
  {
    set_zero_time(tm);
    return 0;
  }
  
  to= *pos;     
  tm->year =  (uint) sint2korr(to);
  tm->month=  (uint) to[2];
  tm->day= (uint) to[3];

  tm->hour= tm->minute= tm->second= 0;
  tm->second_part= 0;
  tm->neg= 0;
  return length;
}

/* Convert Numeric to buffer types */
static void send_data_long(MYSQL_BIND *param, longlong value)
{  
  char *buffer= param->buffer;
  
  switch(param->buffer_type) {
  case MYSQL_TYPE_NULL: /* do nothing */
    break;
  case MYSQL_TYPE_TINY:
    *param->buffer= (uchar) value;
    break;
  case MYSQL_TYPE_SHORT:
    int2store(buffer, value);
    break;
  case MYSQL_TYPE_LONG:
    int4store(buffer, value);
    break;
  case MYSQL_TYPE_LONGLONG:
    int8store(buffer, value);
    break;
  case MYSQL_TYPE_FLOAT:
    {
      float data= (float)value;
      float4store(buffer, data);
      break;
    }
  case MYSQL_TYPE_DOUBLE:
    {
      double data= (double)value;
      float8store(buffer, data);
      break;
    }
  default:
    {
      char tmp[NAME_LEN];
      uint length= (uint)(longlong10_to_str(value,(char *)tmp,10)-tmp);
      ulong copy_length= min((ulong)length-param->offset, param->buffer_length);
      memcpy(buffer, (char *)tmp+param->offset, copy_length);
      *param->length= length;		
  
      if (copy_length != param->buffer_length)
        *(buffer+copy_length)= '\0';
    }
  } 
}


/* Convert Double to buffer types */
static void send_data_double(MYSQL_BIND *param, double value)
{  
  char *buffer= param->buffer;

  switch(param->buffer_type) {
  case MYSQL_TYPE_NULL: /* do nothing */
    break;
  case MYSQL_TYPE_TINY:
    *buffer= (uchar)value;
    break;
  case MYSQL_TYPE_SHORT:
    int2store(buffer, (short)value);
    break;
  case MYSQL_TYPE_LONG:
    int4store(buffer, (long)value);
    break;
  case MYSQL_TYPE_LONGLONG:
    int8store(buffer, (longlong)value);
    break;
  case MYSQL_TYPE_FLOAT:
    {
      float data= (float)value;
      float4store(buffer, data);
      break;
    }
  case MYSQL_TYPE_DOUBLE:
    {
      double data= (double)value;
      float8store(buffer, data);
      break;
    }
  default:
    {
      char tmp[NAME_LEN];
      uint length= my_sprintf(tmp,(tmp,"%g",value));
      ulong copy_length= min((ulong)length-param->offset, param->buffer_length);
      memcpy(buffer, (char *)tmp+param->offset, copy_length);
      *param->length= length;		
  
      if (copy_length != param->buffer_length)
        *(buffer+copy_length)= '\0';
    }
  } 
}

/* Convert string to buffer types */
static void send_data_str(MYSQL_BIND *param, char *value, uint length)
{  
  char *buffer= param->buffer;
  int err=0;

  switch(param->buffer_type) {
  case MYSQL_TYPE_NULL: /* do nothing */
    break;
  case MYSQL_TYPE_TINY:
  {
    uchar data= (uchar)my_strntol(&my_charset_latin1,value,length,10,NULL,
				  &err);
    *buffer= data;
    break;
  }
  case MYSQL_TYPE_SHORT:
  {
    short data= (short)my_strntol(&my_charset_latin1,value,length,10,NULL,
				  &err);
    int2store(buffer, data);
    break;
  }
  case MYSQL_TYPE_LONG:
  {
    int32 data= (int32)my_strntol(&my_charset_latin1,value,length,10,NULL,
				  &err);
    int4store(buffer, data);    
    break;
  }
  case MYSQL_TYPE_LONGLONG:
  {
    longlong data= my_strntoll(&my_charset_latin1,value,length,10,NULL,&err);
    int8store(buffer, data);
    break;
  }
  case MYSQL_TYPE_FLOAT:
  {
    float data = (float)my_strntod(&my_charset_latin1,value,length,NULL,&err);
    float4store(buffer, data);
    break;
  }
  case MYSQL_TYPE_DOUBLE:
  {
    double data= my_strntod(&my_charset_latin1,value,length,NULL,&err);
    float8store(buffer, data);
    break;
  }
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
    *param->length= length;
    length= min(length-param->offset, param->buffer_length);
    memcpy(buffer, value+param->offset, length);
    break;
  default:
    *param->length= length;
    length= min(length-param->offset, param->buffer_length);
    memcpy(buffer, value+param->offset, length);
    if (length != param->buffer_length)
      buffer[length]= '\0';
  } 
}

static void send_data_time(MYSQL_BIND *param, MYSQL_TIME ltime, 
                           uint length)
{
  switch (param->buffer_type) {
  case MYSQL_TYPE_NULL: /* do nothing */
    break;

  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
  {
    MYSQL_TIME *tm= (MYSQL_TIME *)param->buffer;
    
    tm->year= ltime.year;
    tm->month= ltime.month;
    tm->day= ltime.day;

    tm->hour= ltime.hour;
    tm->minute= ltime.minute;
    tm->second= ltime.second;

    tm->second_part= ltime.second_part;
    tm->neg= ltime.neg;
    break;   
  }
  default:
  {
    char buff[25];
    
    if (!length)
      ltime.time_type= MYSQL_TIMESTAMP_NONE;
    switch (ltime.time_type) {
    case MYSQL_TIMESTAMP_DATE:
      length= my_sprintf(buff,(buff, "%04d-%02d-%02d", ltime.year,
                         ltime.month,ltime.day));      
      break;
    case MYSQL_TIMESTAMP_FULL:
      length= my_sprintf(buff,(buff, "%04d-%02d-%02d %02d:%02d:%02d",
	                       ltime.year,ltime.month,ltime.day,
	                       ltime.hour,ltime.minute,ltime.second));
      break;
    case MYSQL_TIMESTAMP_TIME:
      length= my_sprintf(buff, (buff, "%02d:%02d:%02d",
	                 	     ltime.hour,ltime.minute,ltime.second));
      break;
    default:
      length= 0;
      buff[0]='\0';
    }
    send_data_str(param, (char *)buff, length); 
  }
  }
}
                              


/* Fetch data to buffers */
static void fetch_results(MYSQL_BIND *param, uint field_type, uchar **row, 
                          my_bool field_is_unsigned)
{
  ulong length;
  
  switch (field_type) {
  case MYSQL_TYPE_TINY:
  {
    char value= (char) **row;
    longlong data= (field_is_unsigned) ? (longlong) (unsigned char) value:
                                         (longlong) value;
    send_data_long(param,data);
    length= 1;
    break;
  }
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_YEAR:
  {
    short value= sint2korr(*row);
    longlong data= (field_is_unsigned) ? (longlong) (unsigned short) value:
                                         (longlong) value;
    send_data_long(param,data);
    length= 2;    
    break;
  }
  case MYSQL_TYPE_LONG:
  {
    long value= sint4korr(*row);
    longlong data= (field_is_unsigned) ? (longlong) (unsigned long) value:
                                         (longlong) value;
    send_data_long(param,data);
    length= 4;
    break;
  }
  case MYSQL_TYPE_LONGLONG:
  {
    longlong value= (longlong)sint8korr(*row);
    send_data_long(param,value);
    length= 8;
    break;
  }
  case MYSQL_TYPE_FLOAT:
  {
    float value;
    float4get(value,*row);
    send_data_double(param,value);
    length= 4;
    break;
  }
  case MYSQL_TYPE_DOUBLE:
  {
    double value;
    float8get(value,*row);
    send_data_double(param,value);
    length= 8;
    break;
  }
  case MYSQL_TYPE_DATE:
  {
    MYSQL_TIME tm;
 
    length= read_binary_date(&tm, row);
    tm.time_type= MYSQL_TIMESTAMP_DATE;
    send_data_time(param, tm, length);
    break;
  }
  case MYSQL_TYPE_TIME:
  {
    MYSQL_TIME tm;
 
    length= read_binary_time(&tm, row);
    tm.time_type= MYSQL_TIMESTAMP_TIME;
    send_data_time(param, tm, length);
    break;
  }
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
  {
    MYSQL_TIME tm;
 
    length= read_binary_datetime(&tm, row);
    tm.time_type= MYSQL_TIMESTAMP_FULL;
    send_data_time(param, tm, length);
    break;
  }
  default:      
    length= net_field_length(row); 
    send_data_str(param,(char*) *row,length);
    break;
  }
  *row+= length;
}

static void fetch_result_tinyint(MYSQL_BIND *param, uchar **row)
{
  *param->buffer= (uchar) **row;
  (*row)++;
}

static void fetch_result_short(MYSQL_BIND *param, uchar **row)
{
  short value = (short)sint2korr(*row);
  int2store(param->buffer, value);
  *row+= 2;
}

static void fetch_result_int32(MYSQL_BIND *param, uchar **row)
{
  int32 value= (int32)sint4korr(*row);
  int4store(param->buffer, value);
  *row+= 4;
}

static void fetch_result_int64(MYSQL_BIND *param, uchar **row)
{  
  longlong value= (longlong)sint8korr(*row);
  int8store(param->buffer, value);
  *row+= 8;
}

static void fetch_result_float(MYSQL_BIND *param, uchar **row)
{
  float value;
  float4get(value,*row);
  float4store(param->buffer, value);
  *row+= 4;
}

static void fetch_result_double(MYSQL_BIND *param, uchar **row)
{
  double value;
  float8get(value,*row);
  float8store(param->buffer, value);
  *row+= 8;
}

static void fetch_result_time(MYSQL_BIND *param, uchar **row)
{
  MYSQL_TIME *tm= (MYSQL_TIME *)param->buffer;
  *row+= read_binary_time(tm, row);
}

static void fetch_result_date(MYSQL_BIND *param, uchar **row)
{
  MYSQL_TIME *tm= (MYSQL_TIME *)param->buffer;
  *row+= read_binary_date(tm, row);
}

static void fetch_result_datetime(MYSQL_BIND *param, uchar **row)
{
  MYSQL_TIME *tm= (MYSQL_TIME *)param->buffer;
  *row+= read_binary_datetime(tm, row);
}

static void fetch_result_bin(MYSQL_BIND *param, uchar **row)
{  
  ulong length= net_field_length(row);
  ulong copy_length= min(length, param->buffer_length);
  memcpy(param->buffer, (char *)*row, copy_length);
  *param->length= length;		
  *row+= length;
}

static void fetch_result_str(MYSQL_BIND *param, uchar **row)
{
  ulong length= net_field_length(row);
  ulong copy_length= min(length, param->buffer_length);
  memcpy(param->buffer, (char *)*row, copy_length);
  /* Add an end null if there is room in the buffer */
  if (copy_length != param->buffer_length)
    *(param->buffer+copy_length)= '\0';
  *param->length= length;			/* return total length */
  *row+= length;
}

static uint default_binary_field_length(uint field_type)
{
  switch(field_type) {
  case MYSQL_TYPE_TINY:
    return 1;
  case MYSQL_TYPE_SHORT:
    return 2;
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_FLOAT:
    return 4;
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_DOUBLE:
    return 8;
  default:
    return 0;
  } 
}


/*
  Setup the bind buffers for resultset processing
*/

my_bool STDCALL mysql_bind_result(MYSQL_STMT *stmt, MYSQL_BIND *bind)
{
  MYSQL_BIND *param, *end;
  ulong       bind_count;
  uint        param_count= 0;
  DBUG_ENTER("mysql_bind_result");
  DBUG_ASSERT(stmt != 0);

#ifdef CHECK_EXTRA_ARGUMENTS
  if (stmt->state == MY_ST_UNKNOWN)
  {
    set_stmt_error(stmt, CR_NO_PREPARE_STMT, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  if (!bind)
  {
    set_stmt_error(stmt, CR_NULL_POINTER, unknown_sqlstate);
    DBUG_RETURN(1);
  }
#endif
  if (!(bind_count= stmt->field_count) && 
      !(bind_count= alloc_stmt_fields(stmt)))
    DBUG_RETURN(0);
  
  memcpy((char*) stmt->bind, (char*) bind,
	 sizeof(MYSQL_BIND)*bind_count);

  for (param= stmt->bind, end= param+bind_count; param < end ; param++)
  {
    /*
      Set param->is_null to point to a dummy variable if it's not set.
      This is to make the excute code easier
    */
    if (!param->is_null)
      param->is_null= &int_is_null_dummy;

    if (!param->length)
      param->length= &param_length_is_dummy;

    param->param_number= param_count++;
    param->offset= 0;

    /* Setup data copy functions for the different supported types */
    switch (param->buffer_type) {
    case MYSQL_TYPE_NULL: /* for dummy binds */
      break;
    case MYSQL_TYPE_TINY:
      param->fetch_result= fetch_result_tinyint;
      *param->length= 1;
      break;
    case MYSQL_TYPE_SHORT:
      param->fetch_result= fetch_result_short;
      *param->length= 2;
      break;
    case MYSQL_TYPE_LONG:
      param->fetch_result= fetch_result_int32;
      *param->length= 4;
      break;
    case MYSQL_TYPE_LONGLONG:
      param->fetch_result= fetch_result_int64;
      *param->length= 8;
      break;
    case MYSQL_TYPE_FLOAT:
      param->fetch_result= fetch_result_float;
      *param->length= 4;
      break;
    case MYSQL_TYPE_DOUBLE:
      param->fetch_result= fetch_result_double;
      *param->length= 8;
      break;
    case MYSQL_TYPE_TIME:
      param->fetch_result= fetch_result_time;
      *param->length= sizeof(MYSQL_TIME);
      break;
    case MYSQL_TYPE_DATE:
      param->fetch_result= fetch_result_date;
      *param->length= sizeof(MYSQL_TIME);
      break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      param->fetch_result= fetch_result_datetime;
      *param->length= sizeof(MYSQL_TIME);
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      DBUG_ASSERT(param->buffer_length != 0);
      param->fetch_result= fetch_result_bin;
      break;
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      DBUG_ASSERT(param->buffer_length != 0);
      param->fetch_result= fetch_result_str;
      break;
    default:
      strmov(stmt->sqlstate, unknown_sqlstate);
      sprintf(stmt->last_error,
	      ER(stmt->last_errno= CR_UNSUPPORTED_PARAM_TYPE),
	      param->buffer_type, param_count);
      DBUG_RETURN(1);
    }
  }
  stmt->res_buffers= 1;
  DBUG_RETURN(0);
}

/*
  Fetch row data to bind buffers
*/

static int stmt_fetch_row(MYSQL_STMT *stmt, uchar *row)
{
  MYSQL_BIND  *bind, *end;
  MYSQL_FIELD *field, *field_end;
  uchar *null_ptr, bit;

  if (!row || !stmt->res_buffers)
    return 0;
  
  null_ptr= row; 
  row+= (stmt->field_count+9)/8;		/* skip null bits */
  bit= 4;					/* first 2 bits are reserved */
  
  /* Copy complete row to application buffers */
  for (bind= stmt->bind, end= (MYSQL_BIND *) bind + stmt->field_count, 
       field= stmt->fields, 
       field_end= (MYSQL_FIELD *)stmt->fields+stmt->field_count;
       bind < end && field < field_end;
       bind++, field++)
  {         
    if (*null_ptr & bit)
      *bind->is_null= bind->null_field= 1;
    else
    { 
      *bind->is_null= bind->null_field= 0;
      bind->inter_buffer= row;
      if (field->type == bind->buffer_type)
        (*bind->fetch_result)(bind, &row);
      else 
      {
        my_bool field_is_unsigned= (field->flags & UNSIGNED_FLAG) ? 1: 0;
        fetch_results(bind, field->type, &row, field_is_unsigned);
      }
    }
    if (!((bit<<=1) & 255))
    {
      bit= 1;					/* To next byte */
      null_ptr++;
    }
  }
  return 0;
}

/*
  Fetch and return row data to bound buffers, if any
*/

int STDCALL mysql_fetch(MYSQL_STMT *stmt)
{
  MYSQL *mysql= stmt->mysql;
  uchar *row;
  DBUG_ENTER("mysql_fetch");

  stmt->last_fetched_column= 0; /* reset */
  if (stmt->result_buffered) /* buffered */
  {
    MYSQL_RES *res;
    
    if (!(res= stmt->result))
      goto no_data;

    if (!res->data_cursor) 
    {
      res->current_row= 0;
      goto no_data;
    }    
    row= (uchar *)res->data_cursor->data;
    res->data_cursor= res->data_cursor->next;
    res->current_row= (MYSQL_ROW)row;    
  }
  else /* un-buffered */
  {
    if (packet_error == net_safe_read(mysql))
    {
      set_stmt_errmsg(stmt, mysql->net.last_error, mysql->net.last_errno,
		      mysql->net.sqlstate);
      DBUG_RETURN(1);
    }
    if (mysql->net.read_pos[0] == 254)
    {
      mysql->status= MYSQL_STATUS_READY;
      stmt->current_row= 0;
      goto no_data;
    }
    row= mysql->net.read_pos+1;  
    stmt->current_row= row;
  }
  DBUG_RETURN(stmt_fetch_row(stmt, row));

no_data:
  DBUG_PRINT("info", ("end of data"));    
  DBUG_RETURN(MYSQL_NO_DATA); /* no more data */
}

/*
  Fetch only specified column data to buffers
*/

int STDCALL mysql_fetch_column(MYSQL_STMT *stmt, MYSQL_BIND *bind, 
                               my_ulonglong icol, 
                               ulong offset)
{
  uchar   *row;
  my_bool null_data;

  DBUG_ENTER("mysql_fetch_column");

  if (stmt->result_buffered)
  {
    if (!stmt->result || !(row= (uchar *)stmt->result->current_row))
      goto no_data;
  }
  else if (!(row= stmt->current_row))
    goto no_data;

#ifdef CHECK_EXTRA_ARGUMENTS  
  if (!bind || icol >= stmt->field_count)
  {
    DBUG_PRINT("error",("Invalid column position"));
    DBUG_RETURN(1);
  }
#endif

  /* column '0' == first column */
  if (stmt->res_buffers)
  {
    /* 
      Already buffers are parsed and cached to stmt->bind 
      during mysql_fetch() call.
    */
    MYSQL_BIND *param= stmt->bind+icol; 
    null_data= param->null_field;
    row= param->inter_buffer;
  }
  else
  { 
    if (stmt->last_fetched_column == icol+1)
    {
      /* 
        Data buffer is already parsed during the last call, get 
        the cached information 
      */
      if (!stmt->last_fetched_buffer)
        null_data= 1;
      else
      {
        null_data= 0;
        row= stmt->last_fetched_buffer;
      }
    }
    else
    {
      /* 
        Advance the data buffer to icol position and cache 
        the information for subsequent calls
      */
      uint bit= icol > 6 ? 1 : 4;
      stmt->last_fetched_column= icol+1;

      if (row[icol/8] & (bit << icol & 7))
      {
        stmt->last_fetched_buffer= 0;
        null_data= 1;
      }
      else
      {
        uint length, i;

        null_data= 0;
        row+= (stmt->field_count+9)/8;		/* skip null bits */
        
        for (i=0; i < icol; i++)
        {
          if (!(length= default_binary_field_length((uint)(stmt->fields[i].type))))
            length= net_field_length(&row);
          row+= length;
        }
        stmt->last_fetched_buffer= row;
      }
    }
  }
  if (null_data)
  {
    if (bind->is_null)
      *bind->is_null= 1;
  }
  else
  {    
    MYSQL_FIELD *field= stmt->fields+icol; 
    my_bool field_is_unsigned= (field->flags & UNSIGNED_FLAG) ? 1: 0;
    
    bind->offset= offset;
    if (bind->is_null)
      *bind->is_null= 0;
    if (bind->length) /* Set the length if non char/binary types */
      *bind->length= default_binary_field_length(field->type);
    else
      bind->length= &param_length_is_dummy;
    fetch_results(bind, field->type, &row, field_is_unsigned);
  }
  DBUG_RETURN(0);

no_data:
  DBUG_PRINT("info", ("end of data"));    
  DBUG_RETURN(MYSQL_NO_DATA); /* no more data */
}


/* 
  Read all rows of data from server  (binary format)
*/

static MYSQL_DATA *read_binary_rows(MYSQL_STMT *stmt)
{
  ulong      pkt_len;
  uchar      *cp;
  MYSQL      *mysql= stmt->mysql;
  MYSQL_DATA *result;
  MYSQL_ROWS *cur, **prev_ptr;
  NET        *net = &mysql->net;
  DBUG_ENTER("read_binary_rows");
 
  mysql= mysql->last_used_con;
  if ((pkt_len= net_safe_read(mysql)) == packet_error)
  {
    set_stmt_errmsg(stmt, mysql->net.last_error, mysql->net.last_errno,
		    mysql->net.sqlstate);

    DBUG_RETURN(0);
  }
  if (mysql->net.read_pos[0] == 254) /* end of data */
    return 0;				

  if (!(result=(MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
				       MYF(MY_WME | MY_ZEROFILL))))
  {
    set_stmt_errmsg(stmt, ER(CR_OUT_OF_MEMORY), CR_OUT_OF_MEMORY,
		    unknown_sqlstate);
    DBUG_RETURN(0);
  }
  init_alloc_root(&result->alloc,8192,0);	/* Assume rowlength < 8192 */
  result->alloc.min_malloc= sizeof(MYSQL_ROWS);
  prev_ptr= &result->data;
  result->rows= 0;

  while (*(cp=net->read_pos) != 254 || pkt_len >= 8)
  {
    result->rows++;

    if (!(cur= (MYSQL_ROWS*) alloc_root(&result->alloc,sizeof(MYSQL_ROWS))) ||
	      !(cur->data= ((MYSQL_ROW) alloc_root(&result->alloc, pkt_len))))
    {
      free_rows(result);
      set_stmt_errmsg(stmt, ER(CR_OUT_OF_MEMORY), CR_OUT_OF_MEMORY,
		      unknown_sqlstate);
      DBUG_RETURN(0);
    }
    *prev_ptr= cur;
    prev_ptr= &cur->next;
    memcpy(cur->data, (char*)cp+1, pkt_len-1); 
	  
    if ((pkt_len=net_safe_read(mysql)) == packet_error)
    {
      free_rows(result);
      DBUG_RETURN(0);
    }
  }
  *prev_ptr= 0;
  if (pkt_len > 1)
  {
    mysql->warning_count= uint2korr(cp+1);
    DBUG_PRINT("info",("warning_count:  %ld", mysql->warning_count));
  }
  DBUG_PRINT("exit",("Got %d rows",result->rows));
  DBUG_RETURN(result);
}

/*
  Store or buffer the binary results to stmt
*/

int STDCALL mysql_stmt_store_result(MYSQL_STMT *stmt)
{
  MYSQL *mysql= stmt->mysql;
  MYSQL_RES *result;
  DBUG_ENTER("mysql_stmt_store_result");

  mysql= mysql->last_used_con;

  if (!stmt->field_count)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    set_stmt_error(stmt, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  mysql->status= MYSQL_STATUS_READY;		/* server is ready */
  if (!(result= (MYSQL_RES*) my_malloc((uint) (sizeof(MYSQL_RES)+
					      sizeof(ulong) *
					      stmt->field_count),
				      MYF(MY_WME | MY_ZEROFILL))))
  {
    set_stmt_error(stmt, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  stmt->result_buffered= 1;
  if (!(result->data= read_binary_rows(stmt)))
  {
    my_free((gptr) result,MYF(0));
    DBUG_RETURN(0);
  }
  mysql->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=	result->data->data;
  result->fields=	stmt->fields;
  result->field_count=	stmt->field_count;
  stmt->result= result;
  DBUG_RETURN(0); /* Data buffered, must be fetched with mysql_fetch() */
}

/*
  Seek to desired row in the statement result set
*/

MYSQL_ROW_OFFSET STDCALL
mysql_stmt_row_seek(MYSQL_STMT *stmt, MYSQL_ROW_OFFSET row)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_stmt_row_seek");
  
  if ((result= stmt->result))
  {
    MYSQL_ROW_OFFSET return_value= result->data_cursor;
    result->current_row= 0;
    result->data_cursor= row;
    DBUG_RETURN(return_value);
  }
  
  DBUG_PRINT("exit", ("stmt doesn't contain any resultset"));
  DBUG_RETURN(0);
}

/*
  Return the current statement row cursor position
*/

MYSQL_ROW_OFFSET STDCALL 
mysql_stmt_row_tell(MYSQL_STMT *stmt)
{
  DBUG_ENTER("mysql_stmt_row_tell");
  
  if (stmt->result)
    DBUG_RETURN(stmt->result->data_cursor);
  
  DBUG_PRINT("exit", ("stmt doesn't contain any resultset"));
  DBUG_RETURN(0);
}

/*
  Move the stmt result set data cursor to specified row
*/

void STDCALL
mysql_stmt_data_seek(MYSQL_STMT *stmt, my_ulonglong row)
{
  MYSQL_RES   *result;
  DBUG_ENTER("mysql_stmt_data_seek");
  DBUG_PRINT("enter",("row id to seek: %ld",(long) row));
  
  if ((result= stmt->result))
  {
    MYSQL_ROWS	*tmp= 0;
    if (result->data)
      for (tmp=result->data->data; row-- && tmp ; tmp = tmp->next) ;
    result->current_row= 0;
    result->data_cursor= tmp;
  }
  else
    DBUG_PRINT("exit", ("stmt doesn't contain any resultset"));
}

/*
  Return total rows the current statement result set
*/

my_ulonglong STDCALL mysql_stmt_num_rows(MYSQL_STMT *stmt)
{
  DBUG_ENTER("mysql_stmt_num_rows");
    
  if (stmt->result)
    DBUG_RETURN(stmt->result->row_count);
  
  DBUG_PRINT("exit", ("stmt doesn't contain any resultset"));
  DBUG_RETURN(0);
}

/********************************************************************
 statement error handling and close
*********************************************************************/

/*
  Close the statement handle by freeing all alloced resources

  SYNOPSIS
    mysql_stmt_close()
    stmt	       Statement handle
    skip_list    Flag to indicate delete from list or not
  RETURN VALUES
    0	ok
    1	error
*/
static my_bool stmt_close(MYSQL_STMT *stmt, my_bool skip_list)
{
  MYSQL *mysql;
  DBUG_ENTER("mysql_stmt_close");

  DBUG_ASSERT(stmt != 0);
  
  if (!(mysql= stmt->mysql))
  {
    my_free((gptr) stmt, MYF(MY_WME));
    DBUG_RETURN(0);
  }
  if (mysql->status != MYSQL_STATUS_READY)
  {
    /* Clear the current execution status */
    DBUG_PRINT("warning",("Not all packets read, clearing them"));
    for (;;)
    {
      ulong pkt_len;
      if ((pkt_len= net_safe_read(mysql)) == packet_error)
        break;
      if (pkt_len <= 8 && mysql->net.read_pos[0] == 254)
        break;	
    }
    mysql->status= MYSQL_STATUS_READY;
  }
  if (stmt->state == MY_ST_PREPARE || stmt->state == MY_ST_EXECUTE)
  {
    char buff[4];
    int4store(buff, stmt->stmt_id);
    if (simple_command(mysql, COM_CLOSE_STMT, buff, 4, 1))
    {
      set_stmt_errmsg(stmt, mysql->net.last_error, mysql->net.last_errno,
		      mysql->net.sqlstate);
      stmt->mysql= NULL; /* connection isn't valid anymore */
      DBUG_RETURN(1);
    }
  }
  mysql_free_result(stmt->result);
  stmt->field_count= 0;
  free_root(&stmt->mem_root, MYF(0));
  if (!skip_list)
    mysql->stmts= list_delete(mysql->stmts, &stmt->list);
  mysql->status= MYSQL_STATUS_READY;
  my_free((gptr) stmt->query, MYF(MY_WME));
  my_free((gptr) stmt, MYF(MY_WME));
  DBUG_RETURN(0);
}

my_bool STDCALL mysql_stmt_close(MYSQL_STMT *stmt)
{
  return stmt_close(stmt, 0);
}

/*
  Return statement error code
*/

uint STDCALL mysql_stmt_errno(MYSQL_STMT * stmt)
{
  DBUG_ENTER("mysql_stmt_errno");
  DBUG_RETURN(stmt->last_errno);
}

const char *STDCALL mysql_stmt_sqlstate(MYSQL_STMT * stmt)
{
  DBUG_ENTER("mysql_stmt_sqlstate");
  DBUG_RETURN(stmt->sqlstate);
}

/*
  Return statement error message
*/

const char *STDCALL mysql_stmt_error(MYSQL_STMT * stmt)
{
  DBUG_ENTER("mysql_stmt_error");
  DBUG_RETURN(stmt->last_error);
}

/********************************************************************
 Transactional APIs
*********************************************************************/

/*
  Commit the current transaction
*/

my_bool STDCALL mysql_commit(MYSQL * mysql)
{
  DBUG_ENTER("mysql_commit");
  DBUG_RETURN((my_bool) mysql_real_query(mysql, "commit", 6));
}

/*
  Rollback the current transaction
*/

my_bool STDCALL mysql_rollback(MYSQL * mysql)
{
  DBUG_ENTER("mysql_rollback");
  DBUG_RETURN((my_bool) mysql_real_query(mysql, "rollback", 8));
}


/*
  Set autocommit to either true or false
*/

my_bool STDCALL mysql_autocommit(MYSQL * mysql, my_bool auto_mode)
{
  DBUG_ENTER("mysql_autocommit");
  DBUG_PRINT("enter", ("mode : %d", auto_mode));

  if (auto_mode) /* set to true */
    DBUG_RETURN((my_bool) mysql_real_query(mysql, "set autocommit=1", 16));
  DBUG_RETURN((my_bool) mysql_real_query(mysql, "set autocommit=0", 16));
}


/********************************************************************
 Multi query execution + SPs APIs
*********************************************************************/

/*
  Returns if there are any more query results exists to be read using 
  mysql_next_result()
*/

my_bool STDCALL mysql_more_results(MYSQL *mysql)
{
  my_bool result;
  DBUG_ENTER("mysql_more_results");
  
  result= (mysql->last_used_con->server_status & SERVER_MORE_RESULTS_EXISTS) ? 
          1: 0;
  
  DBUG_PRINT("exit",("More results exists ? %d", result)); 
  DBUG_RETURN(result);
}

/*
  Reads and returns the next query results
*/

my_bool STDCALL mysql_next_result(MYSQL *mysql)
{
  DBUG_ENTER("mysql_next_result");
  
  mysql->net.last_error[0]= 0;
  mysql->net.last_errno= 0;
  strmov(mysql->net.sqlstate, unknown_sqlstate);
  mysql->affected_rows= ~(my_ulonglong) 0;

  if (mysql->last_used_con->server_status & SERVER_MORE_RESULTS_EXISTS)
    DBUG_RETURN(mysql_read_query_result(mysql));
  
  DBUG_RETURN(0);
}
