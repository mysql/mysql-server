/* Copyright (c) 2003, 2016, Oracle and/or its affiliates.
   Copyright (c) 2009, 2016, MariaDB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/*
  This file is included by both libmysql.c (the MySQL client C API)
  and the mysqld server to connect to another MYSQL server.

  The differences for the two cases are:

  - Things that only works for the client:
  - Trying to automaticly determinate user name if not supplied to
    mysql_real_connect()
  - Support for reading local file with LOAD DATA LOCAL
  - SHARED memory handling
  - Prepared statements
  
  - Things that only works for the server
  - Alarm handling on connect
  
  In all other cases, the code should be idential for the client and
  server.
*/ 

#include <my_global.h>

#include "mysql.h"

#ifndef __WIN__
#include <netdb.h>
#endif

/* Remove client convenience wrappers */
#undef max_allowed_packet
#undef net_buffer_length

#ifdef EMBEDDED_LIBRARY

#undef MYSQL_SERVER

#ifndef MYSQL_CLIENT
#define MYSQL_CLIENT
#endif

#define CLI_MYSQL_REAL_CONNECT STDCALL cli_mysql_real_connect

#undef net_flush
my_bool	net_flush(NET *net);

#else  /*EMBEDDED_LIBRARY*/
#define CLI_MYSQL_REAL_CONNECT STDCALL mysql_real_connect
#endif /*EMBEDDED_LIBRARY*/
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql_version.h"
#include "mysqld_error.h"
#include "errmsg.h"
#include <violite.h>
#if !defined(__WIN__)
#include <my_pthread.h>				/* because of signal()	*/
#endif /* !defined(__WIN__) */

#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(__WIN__)
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
#endif /* !defined(__WIN__) */
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif

#if defined(__WIN__)
#define perror(A)
#else
#include <errno.h>
#define SOCKET_ERROR -1
#endif

#ifdef __WIN__
#define CONNECT_TIMEOUT 20
#else
#define CONNECT_TIMEOUT 0
#endif

#include "client_settings.h"
#include <sql_common.h>
#include <mysql/client_plugin.h>
#include <my_context.h>
#include <mysql_async.h>

#define native_password_plugin_name "mysql_native_password"
#define old_password_plugin_name    "mysql_old_password"

uint            mariadb_deinitialize_ssl= 1;
uint		mysql_port=0;
char		*mysql_unix_port= 0;
const char	*unknown_sqlstate= "HY000";
const char	*not_error_sqlstate= "00000";
const char	*cant_connect_sqlstate= "08001";
#ifdef HAVE_SMEM
char		 *shared_memory_base_name= 0;
const char 	*def_shared_memory_base_name= default_shared_memory_base_name;
#endif

static void mysql_close_free_options(MYSQL *mysql);
static void mysql_close_free(MYSQL *mysql);
static void mysql_prune_stmt_list(MYSQL *mysql);
static int cli_report_progress(MYSQL *mysql, uchar *packet, uint length);

#if !defined(__WIN__)
static int wait_for_data(my_socket fd, uint timeout);
#endif

CHARSET_INFO *default_client_charset_info = &my_charset_latin1;

/* Server error code and message */
unsigned int mysql_server_last_errno;
char mysql_server_last_error[MYSQL_ERRMSG_SIZE];

/****************************************************************************
  A modified version of connect().  my_connect() allows you to specify
  a timeout value, in seconds, that we should wait until we
  derermine we can't connect to a particular host.  If timeout is 0,
  my_connect() will behave exactly like connect().

  Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

int my_connect(my_socket fd, const struct sockaddr *name, uint namelen,
	       uint timeout)
{
#if defined(__WIN__)
  DBUG_ENTER("my_connect");
  DBUG_RETURN(connect(fd, (struct sockaddr*) name, namelen));
#else
  int flags, res, s_err;
  DBUG_ENTER("my_connect");
  DBUG_PRINT("enter", ("fd: %d  timeout: %u", fd, timeout));

  /*
    If they passed us a timeout of zero, we should behave
    exactly like the normal connect() call does.
  */

  if (timeout == 0)
    DBUG_RETURN(connect(fd, (struct sockaddr*) name, namelen));

  flags = fcntl(fd, F_GETFL, 0);	  /* Set socket to not block */
#ifdef O_NONBLOCK
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);  /* and save the flags..  */
#endif

  DBUG_PRINT("info", ("connecting non-blocking"));
  res= connect(fd, (struct sockaddr*) name, namelen);
  DBUG_PRINT("info", ("connect result: %d  errno: %d", res, errno));
  s_err= errno;			/* Save the error... */
  fcntl(fd, F_SETFL, flags);
  if ((res != 0) && (s_err != EINPROGRESS))
  {
    errno= s_err;			/* Restore it */
    DBUG_RETURN(-1);
  }
  if (res == 0)				/* Connected quickly! */
    DBUG_RETURN(0);
  DBUG_RETURN(wait_for_data(fd, timeout));
#endif
}


/*
  Wait up to timeout seconds for a connection to be established.

  We prefer to do this with poll() as there is no limitations with this.
  If not, we will use select()
*/

#if !defined(__WIN__)

static int wait_for_data(my_socket fd, uint timeout)
{
#ifdef HAVE_POLL
  struct pollfd ufds;
  int res;
  DBUG_ENTER("wait_for_data");

  DBUG_PRINT("info", ("polling"));
  ufds.fd= fd;
  ufds.events= POLLIN | POLLPRI;
  if (!(res= poll(&ufds, 1, (int) timeout*1000)))
  {
    DBUG_PRINT("info", ("poll timed out"));
    errno= EINTR;
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info",
             ("poll result: %d  errno: %d  revents: 0x%02d  events: 0x%02d",
              res, errno, ufds.revents, ufds.events));
  if (res < 0 || !(ufds.revents & (POLLIN | POLLPRI)))
    DBUG_RETURN(-1);
  /*
    At this point, we know that something happened on the socket.
    But this does not means that everything is alright.
    The connect might have failed. We need to retrieve the error code
    from the socket layer. We must return success only if we are sure
    that it was really a success. Otherwise we might prevent the caller
    from trying another address to connect to.
  */
  {
    int         s_err;
    socklen_t   s_len= sizeof(s_err);

    DBUG_PRINT("info", ("Get SO_ERROR from non-blocked connected socket."));
    res= getsockopt(fd, SOL_SOCKET, SO_ERROR, &s_err, &s_len);
    DBUG_PRINT("info", ("getsockopt res: %d  s_err: %d", res, s_err));
    if (res)
      DBUG_RETURN(res);
    /* getsockopt() was successful, check the retrieved status value. */
    if (s_err)
    {
      errno= s_err;
      DBUG_RETURN(-1);
    }
    /* Status from connect() is zero. Socket is successfully connected. */
  }
  DBUG_RETURN(0);
#else
  SOCKOPT_OPTLEN_TYPE s_err_size = sizeof(uint);
  fd_set sfds;
  struct timeval tv;
  time_t start_time, now_time;
  int res, s_err;
  DBUG_ENTER("wait_for_data");

  if (fd >= FD_SETSIZE)				/* Check if wrong error */
    DBUG_RETURN(0);					/* Can't use timeout */

  /*
    Our connection is "in progress."  We can use the select() call to wait
    up to a specified period of time for the connection to suceed.
    If select() returns 0 (after waiting howevermany seconds), our socket
    never became writable (host is probably unreachable.)  Otherwise, if
    select() returns 1, then one of two conditions exist:
   
    1. An error occured.  We use getsockopt() to check for this.
    2. The connection was set up sucessfully: getsockopt() will
    return 0 as an error.
   
    Thanks goes to Andrew Gierth <andrew@erlenstar.demon.co.uk>
    who posted this method of timing out a connect() in
    comp.unix.programmer on August 15th, 1997.
  */

  FD_ZERO(&sfds);
  FD_SET(fd, &sfds);
  /*
    select could be interrupted by a signal, and if it is, 
    the timeout should be adjusted and the select restarted
    to work around OSes that don't restart select and 
    implementations of select that don't adjust tv upon
    failure to reflect the time remaining
   */
  start_time= my_time(0);
  for (;;)
  {
    tv.tv_sec = (long) timeout;
    tv.tv_usec = 0;
#if defined(HPUX10)
    if ((res = select(fd+1, NULL, (int*) &sfds, NULL, &tv)) > 0)
      break;
#else
    if ((res = select(fd+1, NULL, &sfds, NULL, &tv)) > 0)
      break;
#endif
    if (res == 0)					/* timeout */
      DBUG_RETURN(-1);
    now_time= my_time(0);
    timeout-= (uint) (now_time - start_time);
    if (errno != EINTR || (int) timeout <= 0)
      DBUG_RETURN(-1);
  }

  /*
    select() returned something more interesting than zero, let's
    see if we have any errors.  If the next two statements pass,
    we've got an open socket!
  */

  s_err=0;
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    DBUG_RETURN(-1);

  if (s_err)
  {						/* getsockopt could succeed */
    errno = s_err;
    DBUG_RETURN(-1);					/* but return an error... */
  }
  DBUG_RETURN(0);					/* ok */
#endif /* HAVE_POLL */
}
#endif /* !defined(__WIN__) */

/**
  Set the internal error message to mysql handler

  @param mysql    connection handle (client side)
  @param errcode  CR_ error code, passed to ER macro to get
                  error text
  @parma sqlstate SQL standard sqlstate
*/

void set_mysql_error(MYSQL *mysql, int errcode, const char *sqlstate)
{
  NET *net;
  DBUG_ENTER("set_mysql_error");
  DBUG_PRINT("enter", ("error :%d '%s'", errcode, ER(errcode)));
  DBUG_ASSERT(mysql != 0);

  if (mysql)
  {
    net= &mysql->net;
    net->last_errno= errcode;
    strmov(net->last_error, ER(errcode));
    strmov(net->sqlstate, sqlstate);
  }
  else
  {
    mysql_server_last_errno= errcode;
    strmov(mysql_server_last_error, ER(errcode));
  }
  DBUG_VOID_RETURN;
}

/**
  Clear possible error state of struct NET

  @param net  clear the state of the argument
*/

void net_clear_error(NET *net)
{
  net->last_errno= 0;
  net->last_error[0]= '\0';
  strmov(net->sqlstate, not_error_sqlstate);
}

/**
  Set an error message on the client.

  @param mysql     connection handle
  @param errcode   CR_* errcode, for client errors
  @param sqlstate  SQL standard sql state, unknown_sqlstate for the
                   majority of client errors.
  @param format    error message template, in sprintf format
  @param ...       variable number of arguments
*/

void set_mysql_extended_error(MYSQL *mysql, int errcode,
                                     const char *sqlstate,
                                     const char *format, ...)
{
  NET *net;
  va_list args;
  DBUG_ENTER("set_mysql_extended_error");
  DBUG_PRINT("enter", ("error :%d '%s'", errcode, format));
  DBUG_ASSERT(mysql != 0);

  net= &mysql->net;
  net->last_errno= errcode;
  va_start(args, format);
  my_vsnprintf(net->last_error, sizeof(net->last_error)-1,
               format, args);
  va_end(args);
  strmov(net->sqlstate, sqlstate);

  DBUG_VOID_RETURN;
}



/*
  Create a named pipe connection
*/

#ifdef __WIN__

HANDLE create_named_pipe(MYSQL *mysql, uint connect_timeout, char **arg_host,
			 char **arg_unix_socket)
{
  HANDLE hPipe=INVALID_HANDLE_VALUE;
  char pipe_name[1024];
  DWORD dwMode;
  int i;
  my_bool testing_named_pipes=0;
  char *host= *arg_host, *unix_socket= *arg_unix_socket;

  if ( ! unix_socket || (unix_socket)[0] == 0x00)
    unix_socket = mysql_unix_port;
  if (!host || !strcmp(host,LOCAL_HOST))
    host=LOCAL_HOST_NAMEDPIPE;

  
  pipe_name[sizeof(pipe_name)-1]= 0;		/* Safety if too long string */
  strxnmov(pipe_name, sizeof(pipe_name)-1, "\\\\", host, "\\pipe\\",
	   unix_socket, NullS);
  DBUG_PRINT("info",("Server name: '%s'.  Named Pipe: %s", host, unix_socket));

  for (i=0 ; i < 100 ; i++)			/* Don't retry forever */
  {
    if ((hPipe = CreateFile(pipe_name,
			    GENERIC_READ | GENERIC_WRITE,
			    0,
			    NULL,
			    OPEN_EXISTING,
			    FILE_FLAG_OVERLAPPED,
			    NULL )) != INVALID_HANDLE_VALUE)
      break;
    if (GetLastError() != ERROR_PIPE_BUSY)
    {
      set_mysql_extended_error(mysql, CR_NAMEDPIPEOPEN_ERROR,
                               unknown_sqlstate, ER(CR_NAMEDPIPEOPEN_ERROR),
                               host, unix_socket, (ulong) GetLastError());
      return INVALID_HANDLE_VALUE;
    }
    /* wait for for an other instance */
    if (! WaitNamedPipe(pipe_name, connect_timeout*1000) )
    {
      set_mysql_extended_error(mysql, CR_NAMEDPIPEWAIT_ERROR, unknown_sqlstate,
                               ER(CR_NAMEDPIPEWAIT_ERROR),
                               host, unix_socket, (ulong) GetLastError());
      return INVALID_HANDLE_VALUE;
    }
  }
  if (hPipe == INVALID_HANDLE_VALUE)
  {
    set_mysql_extended_error(mysql, CR_NAMEDPIPEOPEN_ERROR, unknown_sqlstate,
                             ER(CR_NAMEDPIPEOPEN_ERROR), host, unix_socket,
                             (ulong) GetLastError());
    return INVALID_HANDLE_VALUE;
  }
  dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
  if ( !SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL) )
  {
    CloseHandle( hPipe );
    set_mysql_extended_error(mysql, CR_NAMEDPIPESETSTATE_ERROR,
                             unknown_sqlstate, ER(CR_NAMEDPIPESETSTATE_ERROR),
                             host, unix_socket, (ulong) GetLastError());
    return INVALID_HANDLE_VALUE;
  }
  *arg_host=host ; *arg_unix_socket=unix_socket;	/* connect arg */
  return (hPipe);
}
#endif


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
    handle_connect_file_map is file-mapping object, use for create shared
    memory
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
  HANDLE event_conn_closed = NULL;
  HANDLE handle_file_map = NULL;
  ulong connect_number;
  char connect_number_char[22], *p;
  char *tmp= NULL;
  char *suffix_pos;
  DWORD error_allow = 0;
  DWORD error_code = 0;
  DWORD event_access_rights= SYNCHRONIZE | EVENT_MODIFY_STATE;
  char *shared_memory_base_name = mysql->options.shared_memory_base_name;
  static const char *name_prefixes[] = {"","Global\\"};
  const char *prefix;
  int i;

  /*
    If this is NULL, somebody freed the MYSQL* options.  mysql_close()
    is a good candidate.  We don't just silently (re)set it to
    def_shared_memory_base_name as that would create really confusing/buggy
    behavior if the user passed in a different name on the command-line or
    in a my.cnf.
  */
  DBUG_ASSERT(shared_memory_base_name != NULL);

  /*
     get enough space base-name + '_' + longest suffix we might ever send
   */
  if (!(tmp= (char *)my_malloc(strlen(shared_memory_base_name) + 32L, MYF(MY_FAE))))
    goto err;

  /*
    The name of event and file-mapping events create agree next rule:
    shared_memory_base_name+unique_part
    Where:
    shared_memory_base_name is unique value for each server
    unique_part is uniquel value for each object (events and file-mapping)
  */
  for (i = 0; i< array_elements(name_prefixes); i++)
  {
    prefix= name_prefixes[i];
    suffix_pos = strxmov(tmp, prefix , shared_memory_base_name, "_", NullS);
    strmov(suffix_pos, "CONNECT_REQUEST");
    event_connect_request= OpenEvent(event_access_rights, FALSE, tmp);
    if (event_connect_request)
    {
      break;
    }
  }
  if (!event_connect_request)
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_REQUEST_ERROR;
    goto err;
  }
  strmov(suffix_pos, "CONNECT_ANSWER");
  if (!(event_connect_answer= OpenEvent(event_access_rights,FALSE,tmp)))
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

  /* Send to server request of connection */
  if (!SetEvent(event_connect_request))
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_SET_ERROR;
    goto err;
  }

  /* Wait of answer from server */
  if (WaitForSingleObject(event_connect_answer,connect_timeout*1000) !=
      WAIT_OBJECT_0)
  {
    error_allow = CR_SHARED_MEMORY_CONNECT_ABANDONED_ERROR;
    goto err;
  }

  /* Get number of connection */
  connect_number = uint4korr(handle_connect_map);/*WAX2*/
  p= int10_to_str(connect_number, connect_number_char, 10);

  /*
    The name of event and file-mapping events create agree next rule:
    shared_memory_base_name+unique_part+number_of_connection

    Where:
    shared_memory_base_name is uniquel value for each server
    unique_part is uniquel value for each object (events and file-mapping)
    number_of_connection is number of connection between server and client
  */
  suffix_pos = strxmov(tmp, prefix , shared_memory_base_name, "_", connect_number_char,
		       "_", NullS);
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
  if ((event_server_wrote = OpenEvent(event_access_rights,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "SERVER_READ");
  if ((event_server_read = OpenEvent(event_access_rights,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "CLIENT_WROTE");
  if ((event_client_wrote = OpenEvent(event_access_rights,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "CLIENT_READ");
  if ((event_client_read = OpenEvent(event_access_rights,FALSE,tmp)) == NULL)
  {
    error_allow = CR_SHARED_MEMORY_EVENT_ERROR;
    goto err2;
  }

  strmov(suffix_pos, "CONNECTION_CLOSED");
  if ((event_conn_closed = OpenEvent(event_access_rights,FALSE,tmp)) == NULL)
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
    net->vio= vio_new_win32shared_memory(handle_file_map,handle_map,
                                         event_server_wrote,
                                         event_server_read,event_client_wrote,
                                         event_client_read,event_conn_closed);
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
    if (event_conn_closed)
      CloseHandle(event_conn_closed);
    if (handle_map)
      UnmapViewOfFile(handle_map);
    if (handle_file_map)
      CloseHandle(handle_file_map);
  }
err:
  my_free(tmp);
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
    if (error_allow == CR_SHARED_MEMORY_EVENT_ERROR)
      set_mysql_extended_error(mysql, error_allow, unknown_sqlstate,
                               ER(error_allow), suffix_pos, error_code);
    else
      set_mysql_extended_error(mysql, error_allow, unknown_sqlstate,
                               ER(error_allow), error_code);
    return(INVALID_HANDLE_VALUE);
  }
  return(handle_map);
}
#endif

/**
  Read a packet from server. Give error message if socket was down
  or packet is an error message

  @retval  packet_error    An error occurred during reading.
                           Error message is set.
  @retval  
*/

ulong
cli_safe_read(MYSQL *mysql)
{
  NET *net= &mysql->net;
  ulong len=0;

restart:
  if (net->vio != 0)
    len=my_net_read(net);

  if (len == packet_error || len == 0)
  {
    DBUG_PRINT("error",("Wrong connection or packet. fd: %s  len: %lu",
			vio_description(net->vio),len));
#ifdef MYSQL_SERVER
    if (net->vio && vio_was_interrupted(net->vio))
      return (packet_error);
#endif /*MYSQL_SERVER*/
    end_server(mysql);
    set_mysql_error(mysql, net->last_errno == ER_NET_PACKET_TOO_LARGE ?
                    CR_NET_PACKET_TOO_LARGE: CR_SERVER_LOST, unknown_sqlstate);
    return (packet_error);
  }
  if (net->read_pos[0] == 255)
  {
    if (len > 3)
    {
      uchar *pos= net->read_pos+1;
      uint last_errno=uint2korr(pos);

      if (last_errno == 65535 &&
          (mysql->server_capabilities & CLIENT_PROGRESS))
      {
        if (cli_report_progress(mysql, pos+2, (uint) (len-3)))
        {
          /* Wrong packet */
          set_mysql_error(mysql,  CR_MALFORMED_PACKET, unknown_sqlstate);
          return (packet_error);
        }
        goto restart;
      }
      net->last_errno= last_errno;
      
      pos+=2;
      len-=2;
      if (protocol_41(mysql) && (char) pos[0] == '#')
      {
	strmake_buf(net->sqlstate, (char*) pos+1);
	pos+= SQLSTATE_LENGTH+1;
      }
      else
      {
        /*
          The SQL state hasn't been received -- it should be reset to HY000
          (unknown error sql state).
        */

        strmov(net->sqlstate, unknown_sqlstate);
      }

      (void) strmake(net->last_error,(char*) pos,
		     min((uint) len,(uint) sizeof(net->last_error)-1));
    }
    else
      set_mysql_error(mysql, CR_UNKNOWN_ERROR, unknown_sqlstate);
    /*
      Cover a protocol design error: error packet does not
      contain the server status. Therefore, the client has no way
      to find out whether there are more result sets of
      a multiple-result-set statement pending. Luckily, in 5.0 an
      error always aborts execution of a statement, wherever it is
      a multi-statement or a stored procedure, so it should be
      safe to unconditionally turn off the flag here.
    */
    mysql->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

    DBUG_PRINT("error",("Got error: %d/%s (%s)",
                        net->last_errno,
                        net->sqlstate,
                        net->last_error));
    return(packet_error);
  }
  return len;
}

void free_rows(MYSQL_DATA *cur)
{
  if (cur)
  {
    free_root(&cur->alloc,MYF(0));
    my_free(cur);
  }
}

my_bool
cli_advanced_command(MYSQL *mysql, enum enum_server_command command,
		     const uchar *header, ulong header_length,
		     const uchar *arg, ulong arg_length, my_bool skip_check,
                     MYSQL_STMT *stmt)
{
  NET *net= &mysql->net;
  my_bool result= 1;
  my_bool stmt_skip= stmt ? stmt->state != MYSQL_STMT_INIT_DONE : FALSE;
  DBUG_ENTER("cli_advanced_command");

  if (mysql->net.vio == 0)
  {						/* Do reconnect if possible */
    if (mysql_reconnect(mysql) || stmt_skip)
      DBUG_RETURN(1);
  }
  if (mysql->status != MYSQL_STATUS_READY ||
      mysql->server_status & SERVER_MORE_RESULTS_EXISTS)
  {
    DBUG_PRINT("error",("state: %d", mysql->status));
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    DBUG_RETURN(1);
  }

  net_clear_error(net);
  mysql->info=0;
  mysql->affected_rows= ~(my_ulonglong) 0;
  /*
    We don't want to clear the protocol buffer on COM_QUIT, because if
    the previous command was a shutdown command, we may have the
    response for the COM_QUIT already in the communication buffer
  */
  net_clear(&mysql->net, (command != COM_QUIT));

  if (net_write_command(net,(uchar) command, header, header_length,
			arg, arg_length))
  {
    DBUG_PRINT("error",("Can't send command to server. Error: %d",
			socket_errno));
    if (net->last_errno == ER_NET_PACKET_TOO_LARGE)
    {
      set_mysql_error(mysql, CR_NET_PACKET_TOO_LARGE, unknown_sqlstate);
      goto end;
    }
    end_server(mysql);
    if (mysql_reconnect(mysql) || stmt_skip)
      goto end;
    if (net_write_command(net,(uchar) command, header, header_length,
			  arg, arg_length))
    {
      set_mysql_error(mysql, CR_SERVER_GONE_ERROR, unknown_sqlstate);
      goto end;
    }
  }
  result=0;
  if (!skip_check)
    result= ((mysql->packet_length=cli_safe_read(mysql)) == packet_error ?
	     1 : 0);
end:
  DBUG_PRINT("exit",("result: %d", result));
  DBUG_RETURN(result);
}

void free_old_query(MYSQL *mysql)
{
  DBUG_ENTER("free_old_query");
  if (mysql->fields)
    free_root(&mysql->field_alloc,MYF(0));
  init_alloc_root(&mysql->field_alloc,8192,0); /* Assume rowlength < 8192 */
  mysql->fields= 0;
  mysql->field_count= 0;			/* For API */
  mysql->warning_count= 0;
  mysql->info= 0;
  DBUG_VOID_RETURN;
}


/**
  Finish reading of a partial result set from the server.
  Get the EOF packet, and update mysql->status
  and mysql->warning_count.

  @return  TRUE if a communication or protocol error, an error
           is set in this case, FALSE otherwise.
*/

my_bool flush_one_result(MYSQL *mysql)
{
  ulong packet_length;

  DBUG_ASSERT(mysql->status != MYSQL_STATUS_READY);

  do
  {
    packet_length= cli_safe_read(mysql);
    /*
      There is an error reading from the connection,
      or (sic!) there were no error and no
      data in the stream, i.e. no more data from the server.
      Since we know our position in the stream (somewhere in
      the middle of a result set), this latter case is an error too
      -- each result set must end with a EOF packet.
      cli_safe_read() has set an error for us, just return.
    */
    if (packet_length == packet_error)
      return TRUE;
  }
  while (packet_length > 8 || mysql->net.read_pos[0] != 254);

  /* Analyze EOF packet of the result set. */

  if (protocol_41(mysql))
  {
    char *pos= (char*) mysql->net.read_pos + 1;
    mysql->warning_count=uint2korr(pos);
    pos+=2;
    mysql->server_status=uint2korr(pos);
    pos+=2;
  }
  return FALSE;
}


/**
  Read a packet from network. If it's an OK packet, flush it.

  @return  TRUE if error, FALSE otherwise. In case of 
           success, is_ok_packet is set to TRUE or FALSE,
           based on what we got from network.
*/

my_bool opt_flush_ok_packet(MYSQL *mysql, my_bool *is_ok_packet)
{
  ulong packet_length= cli_safe_read(mysql);

  if (packet_length == packet_error)
    return TRUE;

  /* cli_safe_read always reads a non-empty packet. */
  DBUG_ASSERT(packet_length);

  *is_ok_packet= mysql->net.read_pos[0] == 0;
  if (*is_ok_packet)
  {
    uchar *pos= mysql->net.read_pos + 1;

    net_field_length_ll(&pos); /* affected rows */
    net_field_length_ll(&pos); /* insert id */

    mysql->server_status=uint2korr(pos);
    pos+=2;

    if (protocol_41(mysql))
    {
      mysql->warning_count=uint2korr(pos);
      pos+=2;
    }
  }
  return FALSE;
}


/*
  Flush result set sent from server
*/

static void cli_flush_use_result(MYSQL *mysql, my_bool flush_all_results)
{
  /* Clear the current execution status */
  DBUG_ENTER("cli_flush_use_result");
  DBUG_PRINT("warning",("Not all packets read, clearing them"));

  if (flush_one_result(mysql))
    DBUG_VOID_RETURN;                           /* An error occurred */

  if (! flush_all_results)
    DBUG_VOID_RETURN;

  while (mysql->server_status & SERVER_MORE_RESULTS_EXISTS)
  {
    my_bool is_ok_packet;
    if (opt_flush_ok_packet(mysql, &is_ok_packet))
      DBUG_VOID_RETURN;                         /* An error occurred. */
    if (is_ok_packet)
    {
      /*
        Indeed what we got from network was an OK packet, and we
        know that OK is the last one in a multi-result-set, so
        just return.
      */
      DBUG_VOID_RETURN;
    }
    /*
      It's a result set, not an OK packet. A result set contains
      of two result set subsequences: field metadata, terminated
      with EOF packet, and result set data, again terminated with
      EOF packet. Read and flush them.
    */
    if (flush_one_result(mysql) || flush_one_result(mysql))
      DBUG_VOID_RETURN;                         /* An error occurred. */
  }

  DBUG_VOID_RETURN;
}


/*
  Report progress to the client

  RETURN VALUES
    0  ok
    1  error
*/

static int cli_report_progress(MYSQL *mysql, uchar *packet, uint length)
{
  uint stage, max_stage, proc_length;
  double progress;
  uchar *start= packet;

  if (length < 5)
    return 1;                         /* Wrong packet */

  if (!(mysql->options.extension && mysql->options.extension->report_progress))
    return 0;                         /* No callback, ignore packet */

  packet++;                           /* Ignore number of strings */
  stage= (uint) *packet++;
  max_stage= (uint) *packet++;
  progress= uint3korr(packet)/1000.0;
  packet+= 3;
  proc_length= net_field_length(&packet);
  if (packet + proc_length > start + length)
    return 1;                         /* Wrong packet */
  (*mysql->options.extension->report_progress)(mysql, stage, max_stage,
                                               progress, (char*) packet,
                                               proc_length);
  return 0;
}

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
}
#endif

/**************************************************************************
  Shut down connection
**************************************************************************/

void end_server(MYSQL *mysql)
{
  int save_errno= errno;
  DBUG_ENTER("end_server");
  if (mysql->net.vio != 0)
  {
    DBUG_PRINT("info",("Net: %s", vio_description(mysql->net.vio)));
#ifdef MYSQL_SERVER
    slave_io_thread_detach_vio();
#endif
    vio_delete(mysql->net.vio);
    mysql->net.vio= 0;          /* Marker */
    mysql_prune_stmt_list(mysql);
  }
  net_end(&mysql->net);
  free_old_query(mysql);
  errno= save_errno;
  DBUG_VOID_RETURN;
}


void STDCALL
mysql_free_result(MYSQL_RES *result)
{
  DBUG_ENTER("mysql_free_result");
  DBUG_PRINT("enter",("mysql_res: 0x%lx", (long) result));
  if (result)
  {
    MYSQL *mysql= result->handle;
    if (mysql)
    {
      if (mysql->unbuffered_fetch_owner == &result->unbuffered_fetch_cancelled)
        mysql->unbuffered_fetch_owner= 0;
      if (mysql->status == MYSQL_STATUS_USE_RESULT)
      {
        (*mysql->methods->flush_use_result)(mysql, FALSE);
        mysql->status=MYSQL_STATUS_READY;
        if (mysql->unbuffered_fetch_owner)
          *mysql->unbuffered_fetch_owner= TRUE;
      }
    }
    free_rows(result->data);
    if (result->fields)
      free_root(&result->field_alloc,MYF(0));
    my_free(result->row);
    my_free(result);
  }
  DBUG_VOID_RETURN;
}

/****************************************************************************
  Get options from my.cnf
****************************************************************************/

static const char *default_options[]=
{
  "port","socket","compress","password","pipe", "timeout", "user",
  "init-command", "host", "database", "debug", "return-found-rows",
  "ssl-key" ,"ssl-cert" ,"ssl-ca" ,"ssl-capath",
  "character-sets-dir", "default-character-set", "interactive-timeout",
  "connect-timeout", "local-infile", "disable-local-infile",
  "ssl-cipher", "max-allowed-packet", "protocol", "shared-memory-base-name",
  "multi-results", "multi-statements", "multi-queries", "secure-auth",
  "report-data-truncation", "plugin-dir", "default-auth",
  "enable-cleartext-plugin",
  NullS
};
enum option_id {
  OPT_port=1, OPT_socket, OPT_compress, OPT_password, OPT_pipe, OPT_timeout, OPT_user, 
  OPT_init_command, OPT_host, OPT_database, OPT_debug, OPT_return_found_rows, 
  OPT_ssl_key, OPT_ssl_cert, OPT_ssl_ca, OPT_ssl_capath, 
  OPT_character_sets_dir, OPT_default_character_set, OPT_interactive_timeout, 
  OPT_connect_timeout, OPT_local_infile, OPT_disable_local_infile, 
  OPT_ssl_cipher, OPT_max_allowed_packet, OPT_protocol, OPT_shared_memory_base_name, 
  OPT_multi_results, OPT_multi_statements, OPT_multi_queries, OPT_secure_auth, 
  OPT_report_data_truncation, OPT_plugin_dir, OPT_default_auth, 
  OPT_enable_cleartext_plugin,
  OPT_keep_this_one_last
};

static TYPELIB option_types={array_elements(default_options)-1,
			     "options",default_options, NULL};

const char *sql_protocol_names_lib[] =
{ "TCP", "SOCKET", "PIPE", "MEMORY", NullS };
TYPELIB sql_protocol_typelib = {array_elements(sql_protocol_names_lib)-1,"",
				sql_protocol_names_lib, NULL};

static int add_init_command(struct st_mysql_options *options, const char *cmd)
{
  char *tmp;

  if (!options->init_commands)
  {
    options->init_commands= (DYNAMIC_ARRAY*)my_malloc(sizeof(DYNAMIC_ARRAY),
						      MYF(MY_WME));
    init_dynamic_array(options->init_commands,sizeof(char*),5,5);
  }

  if (!(tmp= my_strdup(cmd,MYF(MY_WME))) ||
      insert_dynamic(options->init_commands, (uchar*)&tmp))
  {
    my_free(tmp);
    return 1;
  }

  return 0;
}

#define EXTENSION_SET(OPTS, X, VAL)                              \
    if (!(OPTS)->extension)                                      \
      (OPTS)->extension= (struct st_mysql_options_extention *)   \
        my_malloc(sizeof(struct st_mysql_options_extention),     \
                  MYF(MY_WME | MY_ZEROFILL));                    \
    (OPTS)->extension->X= VAL;

#define EXTENSION_SET_STRING(OPTS, X, STR)                       \
    if ((OPTS)->extension)                                       \
      my_free((OPTS)->extension->X);                             \
    EXTENSION_SET(OPTS, X, my_strdup((STR), MYF(MY_WME)));

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
static char *set_ssl_option_unpack_path(const char *arg)
{
  char *opt_var= NULL;
  if (arg)
  {
    char buff[FN_REFLEN + 1];
    unpack_filename(buff, (char *)arg);
    opt_var= my_strdup(buff, MYF(MY_WME));
  }
  return opt_var;
}
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */

void mysql_read_default_options(struct st_mysql_options *options,
				const char *filename,const char *group)
{
  int argc;
  char *argv_buff[1],**argv;
  const char *groups[5];
  DBUG_ENTER("mysql_read_default_options");
  DBUG_PRINT("enter",("file: %s  group: %s",filename,group ? group :"NULL"));

  compile_time_assert(OPT_keep_this_one_last ==
                      array_elements(default_options));

  argc=1; argv=argv_buff; argv_buff[0]= (char*) "client";
  groups[0]= (char*) "client";
  groups[1]= (char*) "client-server";
  groups[2]= (char*) "client-mariadb";
  groups[3]= (char*) group;
  groups[4]=0;

  my_load_defaults(filename, groups, &argc, &argv, NULL);
  if (argc != 1)				/* If some default option */
  {
    char **option=argv;
    while (*++option)
    {
      if (my_getopt_is_args_separator(option[0]))          /* skip arguments separator */
        continue;
      /* DBUG_PRINT("info",("option: %s",option[0])); */
      if (option[0][0] == '-' && option[0][1] == '-')
      {
	char *end=strcend(*option,'=');
	char *opt_arg=0;
	if (*end)
	{
	  opt_arg=end+1;
	  *end=0;				/* Remove '=' */
	}
	/* Change all '_' in variable name to '-' */
	for (end= *option ; *(end= strcend(end,'_')) ; )
	  *end= '-';
	switch (find_type(*option + 2, &option_types, FIND_TYPE_BASIC)) {
	case OPT_port:
	  if (opt_arg)
	    options->port=atoi(opt_arg);
	  break;
	case OPT_socket:
	  if (opt_arg)
	  {
	    my_free(options->unix_socket);
	    options->unix_socket=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case OPT_compress:
	  options->compress=1;
	  options->client_flag|= CLIENT_COMPRESS;
	  break;
        case OPT_password:
	  if (opt_arg)
	  {
	    my_free(options->password);
	    options->password=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
        case OPT_pipe:
          options->protocol = MYSQL_PROTOCOL_PIPE;
        case OPT_connect_timeout: 
	case OPT_timeout:
	  if (opt_arg)
	    options->connect_timeout=atoi(opt_arg);
	  break;
	case OPT_user:
	  if (opt_arg)
	  {
	    my_free(options->user);
	    options->user=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case OPT_init_command:
	  add_init_command(options,opt_arg);
	  break;
	case OPT_host:
	  if (opt_arg)
	  {
	    my_free(options->host);
	    options->host=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
        case OPT_database:
	  if (opt_arg)
	  {
	    my_free(options->db);
	    options->db=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case OPT_debug:
#ifdef MYSQL_CLIENT
	  mysql_debug(opt_arg ? opt_arg : "d:t:o,/tmp/client.trace");
	  break;
#endif
	case OPT_return_found_rows:
	  options->client_flag|=CLIENT_FOUND_ROWS;
	  break;
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
	case OPT_ssl_key:
	  my_free(options->ssl_key);
          options->ssl_key = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case OPT_ssl_cert:
	  my_free(options->ssl_cert);
          options->ssl_cert = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case OPT_ssl_ca:
	  my_free(options->ssl_ca);
          options->ssl_ca = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case OPT_ssl_capath:
	  my_free(options->ssl_capath);
          options->ssl_capath = my_strdup(opt_arg, MYF(MY_WME));
          break;
        case OPT_ssl_cipher:
          my_free(options->ssl_cipher);
          options->ssl_cipher= my_strdup(opt_arg, MYF(MY_WME));
          break;
#else
	case OPT_ssl_key:
	case OPT_ssl_cert:
	case OPT_ssl_ca:
	case OPT_ssl_capath:
        case OPT_ssl_cipher:
	  break;
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
	case OPT_character_sets_dir:
	  my_free(options->charset_dir);
          options->charset_dir = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case OPT_default_character_set:
	  my_free(options->charset_name);
          options->charset_name = my_strdup(opt_arg, MYF(MY_WME));
	  break;
        case OPT_interactive_timeout:
	  options->client_flag|= CLIENT_INTERACTIVE;
	  break;
        case OPT_local_infile:
	  if (!opt_arg || atoi(opt_arg) != 0)
	    options->client_flag|= CLIENT_LOCAL_FILES;
	  else
	    options->client_flag&= ~CLIENT_LOCAL_FILES;
	  break;
        case OPT_disable_local_infile:
	  options->client_flag&= ~CLIENT_LOCAL_FILES;
          break;
	case OPT_max_allowed_packet:
          if (opt_arg)
	    options->max_allowed_packet= atoi(opt_arg);
	  break;
        case OPT_protocol:
          if ((options->protocol= find_type(opt_arg, &sql_protocol_typelib,
                                            FIND_TYPE_BASIC)) <= 0)
          {
            fprintf(stderr, "Unknown option to protocol: %s\n", opt_arg);
            exit(1);
          }
          break;
        case OPT_shared_memory_base_name:
#ifdef HAVE_SMEM
          if (options->shared_memory_base_name != def_shared_memory_base_name)
            my_free(options->shared_memory_base_name);
          options->shared_memory_base_name=my_strdup(opt_arg,MYF(MY_WME));
#endif
          break;
        case OPT_multi_results:
	  options->client_flag|= CLIENT_MULTI_RESULTS;
	  break;
        case OPT_multi_statements:
        case OPT_multi_queries:
	  options->client_flag|= CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS;
	  break;
        case OPT_secure_auth:
          options->secure_auth= TRUE;
          break;
        case OPT_report_data_truncation:
          options->report_data_truncation= opt_arg ? test(atoi(opt_arg)) : 1;
          break;
        case OPT_plugin_dir:
          {
            char buff[FN_REFLEN], buff2[FN_REFLEN];
            if (strlen(opt_arg) >= FN_REFLEN)
              opt_arg[FN_REFLEN]= '\0';
            if (my_realpath(buff, opt_arg, 0))
            {
              DBUG_PRINT("warning",("failed to normalize the plugin path: %s",
                                    opt_arg));
              break;
            }
            convert_dirname(buff2, buff, NULL);
            EXTENSION_SET_STRING(options, plugin_dir, buff2);
          }
          break;
        case OPT_default_auth:
          EXTENSION_SET_STRING(options, default_auth, opt_arg);
          break;
        case OPT_enable_cleartext_plugin:
          break;
	default:
	  DBUG_PRINT("warning",("unknown option: %s",option[0]));
	}
      }
    }
  }
  free_defaults(argv);
  DBUG_VOID_RETURN;
}


/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

static void cli_fetch_lengths(ulong *to, MYSQL_ROW column,
			      unsigned int field_count)
{ 
  ulong *prev_length;
  char *start=0;
  MYSQL_ROW end;

  prev_length=0;				/* Keep gcc happy */
  for (end=column + field_count + 1 ; column != end ; column++, to++)
  {
    if (!*column)
    {
      *to= 0;					/* Null */
      continue;
    }
    if (start)					/* Found end of prev string */
      *prev_length= (ulong) (*column-start-1);
    start= *column;
    prev_length= to;
  }
}

/***************************************************************************
  Change field rows to field structs
***************************************************************************/

MYSQL_FIELD *
unpack_fields(MYSQL *mysql, MYSQL_DATA *data,MEM_ROOT *alloc,uint fields,
	      my_bool default_value, uint server_capabilities)
{
  MYSQL_ROWS	*row;
  MYSQL_FIELD	*field,*result;
  ulong lengths[9];				/* Max of fields */
  DBUG_ENTER("unpack_fields");

  field= result= (MYSQL_FIELD*) alloc_root(alloc,
					   (uint) sizeof(*field)*fields);
  if (!result)
  {
    free_rows(data);				/* Free old data */
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  bzero((char*) field, (uint) sizeof(MYSQL_FIELD)*fields);
  if (server_capabilities & CLIENT_PROTOCOL_41)
  {
    /* server is 4.1, and returns the new field result format */
    for (row=data->data; row ; row = row->next,field++)
    {
      uchar *pos;
      /* fields count may be wrong */
      DBUG_ASSERT((uint) (field - result) < fields);
      cli_fetch_lengths(&lengths[0], row->data, default_value ? 8 : 7);
      field->catalog=   strmake_root(alloc,(char*) row->data[0], lengths[0]);
      field->db=        strmake_root(alloc,(char*) row->data[1], lengths[1]);
      field->table=     strmake_root(alloc,(char*) row->data[2], lengths[2]);
      field->org_table= strmake_root(alloc,(char*) row->data[3], lengths[3]);
      field->name=      strmake_root(alloc,(char*) row->data[4], lengths[4]);
      field->org_name=  strmake_root(alloc,(char*) row->data[5], lengths[5]);

      field->catalog_length=	lengths[0];
      field->db_length=		lengths[1];
      field->table_length=	lengths[2];
      field->org_table_length=	lengths[3];
      field->name_length=	lengths[4];
      field->org_name_length=	lengths[5];

      /* Unpack fixed length parts */
      if (lengths[6] != 12)
      {
        /* malformed packet. signal an error. */
        free_rows(data);			/* Free old data */
        set_mysql_error(mysql, CR_MALFORMED_PACKET, unknown_sqlstate);
        DBUG_RETURN(0);
      }

      pos= (uchar*) row->data[6];
      field->charsetnr= uint2korr(pos);
      field->length=	(uint) uint4korr(pos+2);
      field->type=	(enum enum_field_types) pos[6];
      field->flags=	uint2korr(pos+7);
      field->decimals=  (uint) pos[9];

      if (IS_NUM(field->type))
        field->flags|= NUM_FLAG;
      if (default_value && row->data[7])
      {
        field->def=strmake_root(alloc,(char*) row->data[7], lengths[7]);
	field->def_length= lengths[7];
      }
      else
        field->def=0;
      field->max_length= 0;
    }
  }
#ifndef DELETE_SUPPORT_OF_4_0_PROTOCOL
  else
  {
    /* old protocol, for backward compatibility */
    for (row=data->data; row ; row = row->next,field++)
    {
      cli_fetch_lengths(&lengths[0], row->data, default_value ? 6 : 5);
      field->org_table= field->table=  strdup_root(alloc,(char*) row->data[0]);
      field->name=   strdup_root(alloc,(char*) row->data[1]);
      field->length= (uint) uint3korr(row->data[2]);
      field->type=   (enum enum_field_types) (uchar) row->data[3][0];

      field->catalog=(char*)  "";
      field->db=     (char*)  "";
      field->catalog_length= 0;
      field->db_length= 0;
      field->org_table_length=	field->table_length=	lengths[0];
      field->name_length=	lengths[1];

      if (server_capabilities & CLIENT_LONG_FLAG)
      {
        field->flags=   uint2korr(row->data[4]);
        field->decimals=(uint) (uchar) row->data[4][2];
      }
      else
      {
        field->flags=   (uint) (uchar) row->data[4][0];
        field->decimals=(uint) (uchar) row->data[4][1];
      }
      if (IS_NUM(field->type))
        field->flags|= NUM_FLAG;
      if (default_value && row->data[5])
      {
        field->def=strdup_root(alloc,(char*) row->data[5]);
	field->def_length= lengths[5];
      }
      else
        field->def=0;
      field->max_length= 0;
    }
  }
#endif /* DELETE_SUPPORT_OF_4_0_PROTOCOL */
  free_rows(data);				/* Free old data */
  DBUG_RETURN(result);
}

/* Read all rows (fields or data) from server */

MYSQL_DATA *cli_read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
			  unsigned int fields)
{
  uint	field;
  ulong pkt_len;
  ulong len;
  uchar *cp;
  char	*to, *end_to;
  MYSQL_DATA *result;
  MYSQL_ROWS **prev_ptr,*cur;
  NET *net = &mysql->net;
  DBUG_ENTER("cli_read_rows");

  if ((pkt_len= cli_safe_read(mysql)) == packet_error)
    DBUG_RETURN(0);
  if (!(result=(MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
				       MYF(MY_WME | MY_ZEROFILL))))
  {
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  init_alloc_root(&result->alloc,8192,0);	/* Assume rowlength < 8192 */
  result->alloc.min_malloc=sizeof(MYSQL_ROWS);
  prev_ptr= &result->data;
  result->rows=0;
  result->fields=fields;

  /*
    The last EOF packet is either a single 254 character or (in MySQL 4.1)
    254 followed by 1-7 status bytes.

    This doesn't conflict with normal usage of 254 which stands for a
    string where the length of the string is 8 bytes. (see net_field_length())
  */

  while (*(cp=net->read_pos) != 254 || pkt_len >= 8)
  {
    result->rows++;
    if (!(cur= (MYSQL_ROWS*) alloc_root(&result->alloc,
					sizeof(MYSQL_ROWS))) ||
	!(cur->data= ((MYSQL_ROW)
		      alloc_root(&result->alloc,
				 (fields+1)*sizeof(char *)+pkt_len))))
    {
      free_rows(result);
      set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
      DBUG_RETURN(0);
    }
    *prev_ptr=cur;
    prev_ptr= &cur->next;
    to= (char*) (cur->data+fields+1);
    end_to=to+pkt_len-1;
    for (field=0 ; field < fields ; field++)
    {
      if ((len=(ulong) net_field_length(&cp)) == NULL_LENGTH)
      {						/* null field */
	cur->data[field] = 0;
      }
      else
      {
	cur->data[field] = to;
        if (len > (ulong) (end_to - to))
        {
          free_rows(result);
          set_mysql_error(mysql, CR_MALFORMED_PACKET, unknown_sqlstate);
          DBUG_RETURN(0);
        }
	memcpy(to,(char*) cp,len); to[len]=0;
	to+=len+1;
	cp+=len;
	if (mysql_fields)
	{
	  if (mysql_fields[field].max_length < len)
	    mysql_fields[field].max_length=len;
	}
      }
    }
    cur->data[field]=to;			/* End of last field */
    if ((pkt_len=cli_safe_read(mysql)) == packet_error)
    {
      free_rows(result);
      DBUG_RETURN(0);
    }
  }
  *prev_ptr=0;					/* last pointer is null */
  if (pkt_len > 1)				/* MySQL 4.1 protocol */
  {
    mysql->warning_count= uint2korr(cp+1);
    mysql->server_status= uint2korr(cp+3);
    DBUG_PRINT("info",("status: %u  warning_count:  %u",
		       mysql->server_status, mysql->warning_count));
  }
  DBUG_PRINT("exit", ("Got %lu rows", (ulong) result->rows));
  DBUG_RETURN(result);
}

/*
  Read one row. Uses packet buffer as storage for fields.
  When next packet is read, the previous field values are destroyed
*/


static int
read_one_row(MYSQL *mysql,uint fields,MYSQL_ROW row, ulong *lengths)
{
  uint field;
  ulong pkt_len,len;
  uchar *pos, *prev_pos, *end_pos;
  NET *net= &mysql->net;

  if ((pkt_len=cli_safe_read(mysql)) == packet_error)
    return -1;
  if (pkt_len <= 8 && net->read_pos[0] == 254)
  {
    if (pkt_len > 1)				/* MySQL 4.1 protocol */
    {
      mysql->warning_count= uint2korr(net->read_pos+1);
      mysql->server_status= uint2korr(net->read_pos+3);
    }
    return 1;				/* End of data */
  }
  prev_pos= 0;				/* allowed to write at packet[-1] */
  pos=net->read_pos;
  end_pos=pos+pkt_len;
  for (field=0 ; field < fields ; field++)
  {
    if ((len=(ulong) net_field_length(&pos)) == NULL_LENGTH)
    {						/* null field */
      row[field] = 0;
      *lengths++=0;
    }
    else
    {
      if (len > (ulong) (end_pos - pos))
      {
        set_mysql_error(mysql, CR_UNKNOWN_ERROR, unknown_sqlstate);
        return -1;
      }
      row[field] = (char*) pos;
      pos+=len;
      *lengths++=len;
    }
    if (prev_pos)
      *prev_pos=0;				/* Terminate prev field */
    prev_pos=pos;
  }
  row[field]=(char*) prev_pos+1;		/* End of last field */
  *prev_pos=0;					/* Terminate last field */
  return 0;
}


/****************************************************************************
  Init MySQL structure or allocate one
****************************************************************************/

MYSQL * STDCALL
mysql_init(MYSQL *mysql)
{
  if (mysql_server_init(0, NULL, NULL))
    return 0;
  if (!mysql)
  {
    if (!(mysql=(MYSQL*) my_malloc(sizeof(*mysql),MYF(MY_WME | MY_ZEROFILL))))
    {
      set_mysql_error(NULL, CR_OUT_OF_MEMORY, unknown_sqlstate);
      return 0;
    }
    mysql->free_me=1;
  }
  else
    bzero((char*) (mysql), sizeof(*(mysql)));
  mysql->options.connect_timeout= CONNECT_TIMEOUT;
  mysql->charset=default_client_charset_info;
  strmov(mysql->net.sqlstate, not_error_sqlstate);

  /*
    Only enable LOAD DATA INFILE by default if configured with
    --enable-local-infile
  */

#if defined(ENABLED_LOCAL_INFILE) && !defined(MYSQL_SERVER)
  mysql->options.client_flag|= CLIENT_LOCAL_FILES;
#endif

#ifdef HAVE_SMEM
  mysql->options.shared_memory_base_name= (char*) def_shared_memory_base_name;
#endif

  mysql->options.methods_to_use= MYSQL_OPT_GUESS_CONNECTION;
  mysql->options.report_data_truncation= TRUE;  /* default */

  /*
    By default we don't reconnect because it could silently corrupt data (after
    reconnection you potentially lose table locks, user variables, session
    variables (transactions but they are specifically dealt with in
    mysql_reconnect()).
    This is a change: < 5.0.3 mysql->reconnect was set to 1 by default.
    How this change impacts existing apps:
    - existing apps which relyed on the default will see a behaviour change;
    they will have to set reconnect=1 after mysql_real_connect().
    - existing apps which explicitely asked for reconnection (the only way they
    could do it was by setting mysql.reconnect to 1 after mysql_real_connect())
    will not see a behaviour change.
    - existing apps which explicitely asked for no reconnection
    (mysql.reconnect=0) will not see a behaviour change.
  */
  mysql->reconnect= 0;

  DBUG_PRINT("mysql",("mysql: 0x%lx", (long) mysql));
  return mysql;
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
  DBUG_ENTER("mysql_ssl_set");
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  my_free(mysql->options.ssl_key);
  my_free(mysql->options.ssl_cert);
  my_free(mysql->options.ssl_ca);
  my_free(mysql->options.ssl_capath);
  my_free(mysql->options.ssl_cipher);
  mysql->options.ssl_key=    set_ssl_option_unpack_path(key);
  mysql->options.ssl_cert=   set_ssl_option_unpack_path(cert);
  mysql->options.ssl_ca=     set_ssl_option_unpack_path(ca);
  mysql->options.ssl_capath= set_ssl_option_unpack_path(capath);
  mysql->options.ssl_cipher= strdup_if_not_null(cipher);
  mysql->options.use_ssl= TRUE;
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
  DBUG_RETURN(0);
}


/*
  Free strings in the SSL structure and clear 'use_ssl' flag.
  NB! Errors are not reported until you do mysql_real_connect.
*/

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)

static void
mysql_ssl_free(MYSQL *mysql __attribute__((unused)))
{
  struct st_VioSSLFd *ssl_fd= (struct st_VioSSLFd*) mysql->connector_fd;
  DBUG_ENTER("mysql_ssl_free");

  my_free(mysql->options.ssl_key);
  my_free(mysql->options.ssl_cert);
  my_free(mysql->options.ssl_ca);
  my_free(mysql->options.ssl_capath);
  my_free(mysql->options.ssl_cipher);
  if (ssl_fd)
    SSL_CTX_free(ssl_fd->ssl_context);
  my_free(mysql->connector_fd);
  mysql->options.ssl_key = 0;
  mysql->options.ssl_cert = 0;
  mysql->options.ssl_ca = 0;
  mysql->options.ssl_capath = 0;
  mysql->options.ssl_cipher= 0;
  mysql->options.use_ssl = FALSE;
  mysql->connector_fd = 0;
  DBUG_VOID_RETURN;
}

#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */

/*
  Return the SSL cipher (if any) used for current
  connection to the server.

  SYNOPSYS
    mysql_get_ssl_cipher()
      mysql pointer to the mysql connection

*/

const char * STDCALL
mysql_get_ssl_cipher(MYSQL *mysql __attribute__((unused)))
{
  DBUG_ENTER("mysql_get_ssl_cipher");
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if (mysql->net.vio && mysql->net.vio->ssl_arg)
    DBUG_RETURN(SSL_get_cipher_name((SSL*)mysql->net.vio->ssl_arg));
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
  DBUG_RETURN(NULL);
}


/*
  Check the server's (subject) Common Name against the
  hostname we connected to

  SYNOPSIS
  ssl_verify_server_cert()
    vio              pointer to a SSL connected vio
    server_hostname  name of the server that we connected to
    errptr           if we fail, we'll return (a pointer to a string
                     describing) the reason here

  RETURN VALUES
   0 Success
   1 Failed to validate server

 */

#if defined(HAVE_OPENSSL)

static int ssl_verify_server_cert(Vio *vio, const char* server_hostname, const char **errptr)
{
  SSL *ssl;
  X509 *server_cert= NULL;
  char *cn= NULL;
  int cn_loc= -1;
  ASN1_STRING *cn_asn1= NULL;
  X509_NAME_ENTRY *cn_entry= NULL;
  X509_NAME *subject= NULL;
  int ret_validation= 1;

  DBUG_ENTER("ssl_verify_server_cert");
  DBUG_PRINT("enter", ("server_hostname: %s", server_hostname));

  if (!(ssl= (SSL*)vio->ssl_arg))
  {
    *errptr= "No SSL pointer found";
    goto error;
  }

  if (!server_hostname)
  {
    *errptr= "No server hostname supplied";
    goto error;
  }

  if (!(server_cert= SSL_get_peer_certificate(ssl)))
  {
    *errptr= "Could not get server certificate";
    goto error;
  }

  if (X509_V_OK != SSL_get_verify_result(ssl))
  {
    *errptr= "Failed to verify the server certificate";
    goto error;
  }
  /*
    We already know that the certificate exchanged was valid; the SSL library
    handled that. Now we need to verify that the contents of the certificate
    are what we expect.
  */

  /*
   Some notes for future development
   We should check host name in alternative name first and then if needed check in common name.
   Currently yssl doesn't support alternative name.
   openssl 1.0.2 support X509_check_host method for host name validation, we may need to start using
   X509_check_host in the future.
  */

  subject= X509_get_subject_name(server_cert);
  cn_loc= X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
  if (cn_loc < 0)
  {
    *errptr= "Failed to get CN location in the certificate subject";
    goto error;
  }

  cn_entry= X509_NAME_get_entry(subject, cn_loc);
  if (cn_entry == NULL)
  {
    *errptr= "Failed to get CN entry using CN location";
    goto error;
  }

  cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
  if (cn_asn1 == NULL)
  {
    *errptr= "Failed to get CN from CN entry";
    goto error;
  }

  cn= (char *) ASN1_STRING_data(cn_asn1);

  if ((size_t)ASN1_STRING_length(cn_asn1) != strlen(cn))
  {
    *errptr= "NULL embedded in the certificate CN";
    goto error;
  }

  DBUG_PRINT("info", ("Server hostname in cert: %s", cn));
  if (!strcmp(cn, server_hostname))
  {
    /* Success */
    ret_validation= 0;
  }

  *errptr= "SSL certificate validation failure";

error:
  if (server_cert != NULL)
    X509_free (server_cert);
  DBUG_RETURN(ret_validation);
}

#endif /* HAVE_OPENSSL */


/*
  Note that the mysql argument must be initialized with mysql_init()
  before calling mysql_real_connect !
*/

static my_bool cli_read_query_result(MYSQL *mysql);
static MYSQL_RES *cli_use_result(MYSQL *mysql);

int cli_read_change_user_result(MYSQL *mysql)
{
  return cli_safe_read(mysql);
}

static MYSQL_METHODS client_methods=
{
  cli_read_query_result,                       /* read_query_result */
  cli_advanced_command,                        /* advanced_command */
  cli_read_rows,                               /* read_rows */
  cli_use_result,                              /* use_result */
  cli_fetch_lengths,                           /* fetch_lengths */
  cli_flush_use_result,                        /* flush_use_result */
  cli_read_change_user_result                  /* read_change_user_result */
#ifndef MYSQL_SERVER
  ,cli_list_fields,                            /* list_fields */
  cli_read_prepare_result,                     /* read_prepare_result */
  cli_stmt_execute,                            /* stmt_execute */
  cli_read_binary_rows,                        /* read_binary_rows */
  cli_unbuffered_fetch,                        /* unbuffered_fetch */
  NULL,                                        /* free_embedded_thd */
  cli_read_statistics,                         /* read_statistics */
  cli_read_query_result,                       /* next_result */
  cli_read_binary_rows                         /* read_rows_from_cursor */
#endif
};



typedef enum my_cs_match_type_enum
{
  /* MySQL and OS charsets are fully compatible */
  my_cs_exact,
  /* MySQL charset is very close to OS charset  */
  my_cs_approx,
  /*
    MySQL knows this charset, but it is not supported as client character set.
  */
  my_cs_unsupp
} my_cs_match_type;


typedef struct str2str_st
{
  const char *os_name;
  const char *my_name;
  my_cs_match_type param;
} MY_CSET_OS_NAME;

const MY_CSET_OS_NAME charsets[]=
{
#ifdef __WIN__
  {"cp437",          "cp850",    my_cs_approx},
  {"cp850",          "cp850",    my_cs_exact},
  {"cp852",          "cp852",    my_cs_exact},
  {"cp858",          "cp850",    my_cs_approx},
  {"cp866",          "cp866",    my_cs_exact},
  {"cp874",          "tis620",   my_cs_approx},
  {"cp932",          "cp932",    my_cs_exact},
  {"cp936",          "gbk",      my_cs_approx},
  {"cp949",          "euckr",    my_cs_approx},
  {"cp950",          "big5",     my_cs_exact},
  {"cp1200",         "utf16le",  my_cs_unsupp},
  {"cp1201",         "utf16",    my_cs_unsupp},
  {"cp1250",         "cp1250",   my_cs_exact},
  {"cp1251",         "cp1251",   my_cs_exact},
  {"cp1252",         "latin1",   my_cs_exact},
  {"cp1253",         "greek",    my_cs_exact},
  {"cp1254",         "latin5",   my_cs_exact},
  {"cp1255",         "hebrew",   my_cs_approx},
  {"cp1256",         "cp1256",   my_cs_exact},
  {"cp1257",         "cp1257",   my_cs_exact},
  {"cp10000",        "macroman", my_cs_exact},
  {"cp10001",        "sjis",     my_cs_approx},
  {"cp10002",        "big5",     my_cs_approx},
  {"cp10008",        "gb2312",   my_cs_approx},
  {"cp10021",        "tis620",   my_cs_approx},
  {"cp10029",        "macce",    my_cs_exact},
  {"cp12001",        "utf32",    my_cs_unsupp},
  {"cp20107",        "swe7",     my_cs_exact},
  {"cp20127",        "latin1",   my_cs_approx},
  {"cp20866",        "koi8r",    my_cs_exact},
  {"cp20932",        "ujis",     my_cs_exact},
  {"cp20936",        "gb2312",   my_cs_approx},
  {"cp20949",        "euckr",    my_cs_approx},
  {"cp21866",        "koi8u",    my_cs_exact},
  {"cp28591",        "latin1",   my_cs_approx},
  {"cp28592",        "latin2",   my_cs_exact},
  {"cp28597",        "greek",    my_cs_exact},
  {"cp28598",        "hebrew",   my_cs_exact},
  {"cp28599",        "latin5",   my_cs_exact},
  {"cp28603",        "latin7",   my_cs_exact},
#ifdef UNCOMMENT_THIS_WHEN_WL_4579_IS_DONE
  {"cp28605",        "latin9",   my_cs_exact},
#endif
  {"cp38598",        "hebrew",   my_cs_exact},
  {"cp51932",        "ujis",     my_cs_exact},
  {"cp51936",        "gb2312",   my_cs_exact},
  {"cp51949",        "euckr",    my_cs_exact},
  {"cp51950",        "big5",     my_cs_exact},
#ifdef UNCOMMENT_THIS_WHEN_WL_WL_4024_IS_DONE
  {"cp54936",        "gb18030",  my_cs_exact},
#endif
  {"cp65001",        "utf8",     my_cs_exact},

#else /* not Windows */

  {"646",            "latin1",   my_cs_approx}, /* Default on Solaris */
  {"ANSI_X3.4-1968", "latin1",   my_cs_approx},
  {"ansi1251",       "cp1251",   my_cs_exact},
  {"armscii8",       "armscii8", my_cs_exact},
  {"armscii-8",      "armscii8", my_cs_exact},
  {"ASCII",          "latin1",   my_cs_approx},
  {"Big5",           "big5",     my_cs_exact},
  {"cp1251",         "cp1251",   my_cs_exact},
  {"cp1255",         "hebrew",   my_cs_approx},
  {"CP866",          "cp866",    my_cs_exact},
  {"eucCN",          "gb2312",   my_cs_exact},
  {"euc-CN",         "gb2312",   my_cs_exact},
  {"eucJP",          "ujis",     my_cs_exact},
  {"euc-JP",         "ujis",     my_cs_exact},
  {"eucKR",          "euckr",    my_cs_exact},
  {"euc-KR",         "euckr",    my_cs_exact},
#ifdef UNCOMMENT_THIS_WHEN_WL_WL_4024_IS_DONE
  {"gb18030",        "gb18030",  my_cs_exact},
#endif
  {"gb2312",         "gb2312",   my_cs_exact},
  {"gbk",            "gbk",      my_cs_exact},
  {"georgianps",     "geostd8",  my_cs_exact},
  {"georgian-ps",    "geostd8",  my_cs_exact},
  {"IBM-1252",       "cp1252",   my_cs_exact},

  {"iso88591",       "latin1",   my_cs_approx},
  {"ISO_8859-1",     "latin1",   my_cs_approx},
  {"ISO8859-1",      "latin1",   my_cs_approx},
  {"ISO-8859-1",     "latin1",   my_cs_approx},

  {"iso885913",      "latin7",   my_cs_exact},
  {"ISO_8859-13",    "latin7",   my_cs_exact},
  {"ISO8859-13",     "latin7",   my_cs_exact},
  {"ISO-8859-13",    "latin7",   my_cs_exact},

#ifdef UNCOMMENT_THIS_WHEN_WL_4579_IS_DONE
  {"iso885915",      "latin9",   my_cs_exact},
  {"ISO_8859-15",    "latin9",   my_cs_exact},
  {"ISO8859-15",     "latin9",   my_cs_exact},
  {"ISO-8859-15",    "latin9",   my_cs_exact},
#endif

  {"iso88592",       "latin2",   my_cs_exact},
  {"ISO_8859-2",     "latin2",   my_cs_exact},
  {"ISO8859-2",      "latin2",   my_cs_exact},
  {"ISO-8859-2",     "latin2",   my_cs_exact},

  {"iso88597",       "greek",    my_cs_exact},
  {"ISO_8859-7",     "greek",    my_cs_exact},
  {"ISO8859-7",      "greek",    my_cs_exact},
  {"ISO-8859-7",     "greek",    my_cs_exact},

  {"iso88598",       "hebrew",   my_cs_exact},
  {"ISO_8859-8",     "hebrew",   my_cs_exact},
  {"ISO8859-8",      "hebrew",   my_cs_exact},
  {"ISO-8859-8",     "hebrew",   my_cs_exact},

  {"iso88599",       "latin5",   my_cs_exact},
  {"ISO_8859-9",     "latin5",   my_cs_exact},
  {"ISO8859-9",      "latin5",   my_cs_exact},
  {"ISO-8859-9",     "latin5",   my_cs_exact},

  {"koi8r",          "koi8r",    my_cs_exact},
  {"KOI8-R",         "koi8r",    my_cs_exact},
  {"koi8u",          "koi8u",    my_cs_exact},
  {"KOI8-U",         "koi8u",    my_cs_exact},

  {"roman8",         "hp8",      my_cs_exact}, /* Default on HP UX */

  {"Shift_JIS",      "sjis",     my_cs_exact},
  {"SJIS",           "sjis",     my_cs_exact},
  {"shiftjisx0213",  "sjis",     my_cs_exact},
  
  {"tis620",         "tis620",   my_cs_exact},
  {"tis-620",        "tis620",   my_cs_exact},

  {"ujis",           "ujis",     my_cs_exact},

  {"US-ASCII",       "latin1",   my_cs_approx},

  {"utf8",           "utf8",     my_cs_exact},
  {"utf-8",          "utf8",     my_cs_exact},
#endif
  {NULL,             NULL,       0}
};


static const char *
my_os_charset_to_mysql_charset(const char *csname)
{
  const MY_CSET_OS_NAME *csp;
  for (csp= charsets; csp->os_name; csp++)
  {
    if (!my_strcasecmp(&my_charset_latin1, csp->os_name, csname))
    {
      switch (csp->param)
      {
      case my_cs_exact:
        return csp->my_name;

      case my_cs_approx:
        /*
          Maybe we should print a warning eventually:
          character set correspondence is not exact.
        */
        return csp->my_name;

      default:
        my_printf_error(ER_UNKNOWN_ERROR,
                        "OS character set '%s'"
                        " is not supported by MySQL client",
                         MYF(0), csp->my_name);
        goto def;
      }
    }
  }

  my_printf_error(ER_UNKNOWN_ERROR,
                  "Unknown OS character set '%s'.",
                  MYF(0), csname);

def:
  csname= MYSQL_DEFAULT_CHARSET_NAME;
  my_printf_error(ER_UNKNOWN_ERROR,
                  "Switching to the default character set '%s'.",
                  MYF(0), csname);
  return csname;
}


#ifndef __WIN__
#include <stdlib.h> /* for getenv() */
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#endif /* __WIN__ */


static int
mysql_autodetect_character_set(MYSQL *mysql)
{
  const char *csname= MYSQL_DEFAULT_CHARSET_NAME;

#ifdef __WIN__
  char cpbuf[64];
  {
    UINT cp= GetConsoleCP();
    if (cp == 0)
      cp= GetACP();
    my_snprintf(cpbuf, sizeof(cpbuf), "cp%d", (int)cp);
    csname= my_os_charset_to_mysql_charset(cpbuf);
  }
#elif defined(HAVE_SETLOCALE) && defined(HAVE_NL_LANGINFO)
  {
    if (setlocale(LC_CTYPE, "") && (csname= nl_langinfo(CODESET)))
      csname= my_os_charset_to_mysql_charset(csname);
  }
#endif

  if (mysql->options.charset_name)
    my_free(mysql->options.charset_name);
  if (!(mysql->options.charset_name= my_strdup(csname, MYF(MY_WME))))
    return 1;
  return 0;
}


static void
mysql_set_character_set_with_default_collation(MYSQL *mysql)
{
  const char *save= charsets_dir;
  if (mysql->options.charset_dir)
    charsets_dir=mysql->options.charset_dir;

  if ((mysql->charset= get_charset_by_csname(mysql->options.charset_name,
                                             MY_CS_PRIMARY, MYF(MY_WME))))
  {
    /* Try to set compiled default collation when it's possible. */
    CHARSET_INFO *collation;
    if ((collation= 
         get_charset_by_name(MYSQL_DEFAULT_COLLATION_NAME, MYF(MY_WME))) &&
                             my_charset_same(mysql->charset, collation))
    {
      mysql->charset= collation;
    }
    else
    {
      /*
        Default compiled collation not found, or is not applicable
        to the requested character set.
        Continue with the default collation of the character set.
      */
    }
  }
  charsets_dir= save;
}


C_MODE_START
int mysql_init_character_set(MYSQL *mysql)
{
  /* Set character set */
  if (!mysql->options.charset_name)
  {
    if (!(mysql->options.charset_name= 
       my_strdup(MYSQL_DEFAULT_CHARSET_NAME,MYF(MY_WME))))
      return 1;
  }
  else if (!strcmp(mysql->options.charset_name,
                   MYSQL_AUTODETECT_CHARSET_NAME) &&
            mysql_autodetect_character_set(mysql))
    return 1;

  mysql_set_character_set_with_default_collation(mysql);

  if (!mysql->charset)
  {
    if (mysql->options.charset_dir)
      set_mysql_extended_error(mysql, CR_CANT_READ_CHARSET, unknown_sqlstate,
                               ER(CR_CANT_READ_CHARSET),
                               mysql->options.charset_name,
                               mysql->options.charset_dir);
    else
    {
      char cs_dir_name[FN_REFLEN];
      get_charsets_dir(cs_dir_name);
      set_mysql_extended_error(mysql, CR_CANT_READ_CHARSET, unknown_sqlstate,
                               ER(CR_CANT_READ_CHARSET),
                               mysql->options.charset_name,
                               cs_dir_name);
    }
    return 1;
  }
  return 0;
}
C_MODE_END

/*********** client side authentication support **************************/

typedef struct st_mysql_client_plugin_AUTHENTICATION auth_plugin_t;
static int client_mpvio_write_packet(struct st_plugin_vio*, const uchar*, int);
static int native_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);
static int old_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);

static auth_plugin_t native_password_client_plugin=
{
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN,
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN_INTERFACE_VERSION,
  native_password_plugin_name,
  "R.J.Silk, Sergei Golubchik",
  "Native MySQL authentication",
  {1, 0, 0},
  "GPL",
  NULL,
  NULL,
  NULL,
  NULL,
  native_password_auth_client
};

static auth_plugin_t old_password_client_plugin=
{
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN,
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN_INTERFACE_VERSION,
  old_password_plugin_name,
  "R.J.Silk, Sergei Golubchik",
  "Old MySQL-3.23 authentication",
  {1, 0, 0},
  "GPL",
  NULL,
  NULL,
  NULL,
  NULL,
  old_password_auth_client
};

struct st_mysql_client_plugin *mysql_client_builtins[]=
{
  (struct st_mysql_client_plugin *)&native_password_client_plugin,
  (struct st_mysql_client_plugin *)&old_password_client_plugin,
  0
};

/* this is a "superset" of MYSQL_PLUGIN_VIO, in C++ I use inheritance */
typedef struct {
  int (*read_packet)(struct st_plugin_vio *vio, uchar **buf);
  int (*write_packet)(struct st_plugin_vio *vio, const uchar *pkt, int pkt_len);
  void (*info)(struct st_plugin_vio *vio, struct st_plugin_vio_info *info);
  /* -= end of MYSQL_PLUGIN_VIO =- */
  MYSQL *mysql;
  auth_plugin_t *plugin;            /**< what plugin we're under */
  const char *db;
  struct {
    uchar *pkt;                     /**< pointer into NET::buff */
    uint pkt_len;
  } cached_server_reply;
  int packets_read, packets_written; /**< counters for send/received packets */
  int mysql_change_user;            /**< if it's mysql_change_user() */
  int last_read_packet_len;         /**< the length of the last *read* packet */
} MCPVIO_EXT;

/**
  sends a COM_CHANGE_USER command with a caller provided payload

  Packet format:
   
    Bytes       Content
    -----       ----
    n           user name - \0-terminated string
    n           password
                  3.23 scramble - \0-terminated string (9 bytes)
                  otherwise - length (1 byte) coded
    n           database name - \0-terminated string
    2           character set number (if the server >= 4.1.x)
    n           client auth plugin name - \0-terminated string,
                  (if the server supports plugin auth)

  @retval 0 ok
  @retval 1 error
*/
static int send_change_user_packet(MCPVIO_EXT *mpvio,
                                   const uchar *data, int data_len)
{
  MYSQL *mysql= mpvio->mysql;
  char *buff, *end;
  int res= 1;

  buff= my_alloca(USERNAME_LENGTH+1 + data_len+1 + NAME_LEN+1 + 2 + NAME_LEN+1);

  end= strmake(buff, mysql->user, USERNAME_LENGTH) + 1;

  if (!data_len)
    *end++= 0;
  else
  {
    if (mysql->client_flag & CLIENT_SECURE_CONNECTION)
    {
      DBUG_ASSERT(data_len <= 255);
      if (data_len > 255)
      {
        set_mysql_error(mysql, CR_MALFORMED_PACKET, unknown_sqlstate);
        goto error;
      }
      *end++= data_len;
    }
    else
    {
      DBUG_ASSERT(data_len == SCRAMBLE_LENGTH_323 + 1);
      DBUG_ASSERT(data[SCRAMBLE_LENGTH_323] == 0);
    }
    memcpy(end, data, data_len);
    end+= data_len;
  }
  end= strmake(end, mpvio->db ? mpvio->db : "", NAME_LEN) + 1;

  if (mysql->server_capabilities & CLIENT_PROTOCOL_41)
  {
    int2store(end, (ushort) mysql->charset->number);
    end+= 2;
  }

  if (mysql->server_capabilities & CLIENT_PLUGIN_AUTH)
    end= strmake(end, mpvio->plugin->name, NAME_LEN) + 1;

  res= simple_command(mysql, COM_CHANGE_USER,
                      (uchar*)buff, (ulong)(end-buff), 1);

error:
  my_afree(buff);
  return res;
}


/**
  sends a client authentication packet (second packet in the 3-way handshake)

  Packet format (when the server is 4.0 or earlier):
   
    Bytes       Content
    -----       ----
    2           client capabilities
    3           max packet size
    n           user name, \0-terminated
    9           scramble_323, \0-terminated

  Packet format (when the server is 4.1 or newer):
   
    Bytes       Content
    -----       ----
    4           client capabilities
    4           max packet size
    1           charset number
    23          reserved (always 0)
    n           user name, \0-terminated
    n           plugin auth data (e.g. scramble), length (1 byte) coded
    n           database name, \0-terminated
                (if CLIENT_CONNECT_WITH_DB is set in the capabilities)
    n           client auth plugin name - \0-terminated string,
                (if CLIENT_PLUGIN_AUTH is set in the capabilities)

  @retval 0 ok
  @retval 1 error
*/
static int send_client_reply_packet(MCPVIO_EXT *mpvio,
                                    const uchar *data, int data_len)
{
  MYSQL *mysql= mpvio->mysql;
  NET *net= &mysql->net;
  char *buff, *end;

  /* see end= buff+32 below, fixed size of the packet is 32 bytes */
  buff= my_alloca(33 + USERNAME_LENGTH + data_len + NAME_LEN + NAME_LEN);
  
  mysql->client_flag|= mysql->options.client_flag;
  mysql->client_flag|= CLIENT_CAPABILITIES;

  if (mysql->client_flag & CLIENT_MULTI_STATEMENTS)
    mysql->client_flag|= CLIENT_MULTI_RESULTS;

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if (mysql->options.use_ssl)
    mysql->client_flag|= CLIENT_SSL;
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY*/

  if (mpvio->db)
    mysql->client_flag|= CLIENT_CONNECT_WITH_DB;

  /* Remove options that server doesn't support */
  mysql->client_flag= mysql->client_flag &
                       (~(CLIENT_COMPRESS | CLIENT_SSL | CLIENT_PROTOCOL_41) 
                       | mysql->server_capabilities);

#ifndef HAVE_COMPRESS
  mysql->client_flag&= ~CLIENT_COMPRESS;
#endif

  if (mysql->client_flag & CLIENT_PROTOCOL_41)
  {
    /* 4.1 server and 4.1 client has a 32 byte option flag */
    int4store(buff,mysql->client_flag);
    int4store(buff+4, net->max_packet_size);
    buff[8]= (char) mysql->charset->number;
    bzero(buff+9, 32-9);
    end= buff+32;
  }
  else
  {
    int2store(buff, mysql->client_flag);
    int3store(buff+2, net->max_packet_size);
    end= buff+5;
  }
#ifdef HAVE_OPENSSL

  /*
     If client uses ssl and client also has to verify the server
     certificate, a ssl connection is required.
     If the server does not support ssl, we abort the connection.
  */
  if (mysql->options.use_ssl &&
      (mysql->client_flag & CLIENT_SSL_VERIFY_SERVER_CERT) &&
      !(mysql->server_capabilities & CLIENT_SSL))
  {
    set_mysql_extended_error(mysql, CR_SSL_CONNECTION_ERROR, unknown_sqlstate,
                             ER(CR_SSL_CONNECTION_ERROR),
                             "SSL is required, but the server does not "
                             "support it");
    goto error;
  }

  if (mysql->client_flag & CLIENT_SSL)
  {
    /* Do the SSL layering. */
    struct st_mysql_options *options= &mysql->options;
    struct st_VioSSLFd *ssl_fd;
    enum enum_ssl_init_error ssl_init_error;
    const char *cert_error;
    unsigned long ssl_error;

    /*
      Send mysql->client_flag, max_packet_size - unencrypted otherwise
      the server does not know we want to do SSL
    */
    if (my_net_write(net, (uchar*)buff, (size_t) (end-buff)) || net_flush(net))
    {
      set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                               ER(CR_SERVER_LOST_EXTENDED),
                               "sending connection information to server",
                               errno);
      goto error;
    }

    /* Create the VioSSLConnectorFd - init SSL and load certs */
    if (!(ssl_fd= new_VioSSLConnectorFd(options->ssl_key,
                                        options->ssl_cert,
                                        options->ssl_ca,
                                        options->ssl_capath,
                                        options->ssl_cipher,
                                        &ssl_init_error)))
    {
      set_mysql_extended_error(mysql, CR_SSL_CONNECTION_ERROR, unknown_sqlstate,
                               ER(CR_SSL_CONNECTION_ERROR), sslGetErrString(ssl_init_error));
      goto error;
    }
    mysql->connector_fd= (unsigned char *) ssl_fd;

    /* Connect to the server */
    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslconnect(ssl_fd, net->vio,
                   (long) (mysql->options.connect_timeout), &ssl_error))
    {    
      char buf[512];
      ERR_error_string_n(ssl_error, buf, 512);
      buf[511]= 0;
      set_mysql_extended_error(mysql, CR_SSL_CONNECTION_ERROR, unknown_sqlstate,
                               ER(CR_SSL_CONNECTION_ERROR),
                               buf);
      goto error;
    }
    DBUG_PRINT("info", ("IO layer change done!"));

    /* Verify server cert */
    if ((mysql->client_flag & CLIENT_SSL_VERIFY_SERVER_CERT) &&
        ssl_verify_server_cert(net->vio, mysql->host, &cert_error))
    {
      set_mysql_extended_error(mysql, CR_SSL_CONNECTION_ERROR, unknown_sqlstate,
                               ER(CR_SSL_CONNECTION_ERROR), cert_error);
      goto error;
    }
  }
#endif /* HAVE_OPENSSL */

  DBUG_PRINT("info",("Server version = '%s'  capabilites: %lu  status: %u  client_flag: %lu",
		     mysql->server_version, mysql->server_capabilities,
		     mysql->server_status, mysql->client_flag));

  /* This needs to be changed as it's not useful with big packets */
  if (mysql->user[0])
    strmake(end, mysql->user, USERNAME_LENGTH);
  else
    read_user_name(end);

  /* We have to handle different version of handshake here */
  DBUG_PRINT("info",("user: %s",end));
  end= strend(end) + 1;
  if (data_len)
  {
    if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
    {
      *end++= data_len;
      memcpy(end, data, data_len);
      end+= data_len;
    }
    else
    {
      DBUG_ASSERT(data_len == SCRAMBLE_LENGTH_323 + 1); /* incl. \0 at the end */
      memcpy(end, data, data_len);
      end+= data_len;
    }
  }
  else
    *end++= 0;

  /* Add database if needed */
  if (mpvio->db && (mysql->server_capabilities & CLIENT_CONNECT_WITH_DB))
  {
    end= strmake(end, mpvio->db, NAME_LEN) + 1;
    mysql->db= my_strdup(mpvio->db, MYF(MY_WME));
  }

  if (mysql->server_capabilities & CLIENT_PLUGIN_AUTH)
    end= strmake(end, mpvio->plugin->name, NAME_LEN) + 1;

  /* Write authentication package */
  if (my_net_write(net, (uchar*) buff, (size_t) (end-buff)) || net_flush(net))
  {
    set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                             ER(CR_SERVER_LOST_EXTENDED),
                             "sending authentication information",
                             errno);
    goto error;
  }
  my_afree(buff);
  return 0;
  
error:
  my_afree(buff);
  return 1;
}


/**
  vio->read_packet() callback method for client authentication plugins

  This function is called by a client authentication plugin, when it wants
  to read data from the server.
*/
static int client_mpvio_read_packet(struct st_plugin_vio *mpv, uchar **buf)
{
  MCPVIO_EXT *mpvio= (MCPVIO_EXT*)mpv;
  MYSQL *mysql= mpvio->mysql;
  ulong  pkt_len;

  /* there are cached data left, feed it to a plugin */
  if (mpvio->cached_server_reply.pkt)
  {
    *buf= mpvio->cached_server_reply.pkt;
    mpvio->cached_server_reply.pkt= 0;
    mpvio->packets_read++;
    return mpvio->cached_server_reply.pkt_len;
  }

  if (mpvio->packets_read == 0)
  {
    /*
      the server handshake packet came from the wrong plugin,
      or it's mysql_change_user(). Either way, there is no data
      for a plugin to read. send a dummy packet to the server
      to initiate a dialog.
    */
    if (client_mpvio_write_packet(mpv, 0, 0))
      return (int)packet_error;
  }

  /* otherwise read the data */
  pkt_len= (*mysql->methods->read_change_user_result)(mysql);
  mpvio->last_read_packet_len= pkt_len;
  *buf= mysql->net.read_pos;

  /* was it a request to change plugins ? */
  if (**buf == 254)
    return (int)packet_error; /* if yes, this plugin shan't continue */

  /*
    the server sends \1\255 or \1\254 instead of just \255 or \254 -
    for us to not confuse it with an error or "change plugin" packets.
    We remove this escaping \1 here.

    See also server_mpvio_write_packet() where the escaping is done.
  */
  if (pkt_len && **buf == 1)
  {
    (*buf)++;
    pkt_len--;
  }
  mpvio->packets_read++;
  return pkt_len;
}


/**
  vio->write_packet() callback method for client authentication plugins

  This function is called by a client authentication plugin, when it wants
  to send data to the server.

  It transparently wraps the data into a change user or authentication
  handshake packet, if neccessary.
*/
static int client_mpvio_write_packet(struct st_plugin_vio *mpv,
                                     const uchar *pkt, int pkt_len)
{
  int res;
  MCPVIO_EXT *mpvio= (MCPVIO_EXT*)mpv;

  if (mpvio->packets_written == 0)
  {
    if (mpvio->mysql_change_user)
      res= send_change_user_packet(mpvio, pkt, pkt_len);
    else
      res= send_client_reply_packet(mpvio, pkt, pkt_len);
  }
  else
  {
    NET *net= &mpvio->mysql->net;
    if (mpvio->mysql->thd)
      res= 1; /* no chit-chat in embedded */
    else
      res= my_net_write(net, pkt, pkt_len) || net_flush(net);
    if (res)
      set_mysql_extended_error(mpvio->mysql, CR_SERVER_LOST, unknown_sqlstate,
                               ER(CR_SERVER_LOST_EXTENDED),
                               "sending authentication information",
                               errno);
  }
  mpvio->packets_written++;
  return res;
}


/**
  fills MYSQL_PLUGIN_VIO_INFO structure with the information about the
  connection
*/
void mpvio_info(Vio *vio, MYSQL_PLUGIN_VIO_INFO *info)
{
  bzero(info, sizeof(*info));
  switch (vio->type) {
  case VIO_TYPE_TCPIP:
    info->protocol= MYSQL_VIO_TCP;
    info->socket= vio->sd;
    return;
  case VIO_TYPE_SOCKET:
    info->protocol= MYSQL_VIO_SOCKET;
    info->socket= vio->sd;
    return;
  case VIO_TYPE_SSL:
    {
      struct sockaddr addr;
      SOCKET_SIZE_TYPE addrlen= sizeof(addr);
      if (getsockname(vio->sd, &addr, &addrlen))
        return;
      info->protocol= addr.sa_family == AF_UNIX ?
        MYSQL_VIO_SOCKET : MYSQL_VIO_TCP;
      info->socket= vio->sd;
      return;
    }
#ifdef _WIN32
  case VIO_TYPE_NAMEDPIPE:
    info->protocol= MYSQL_VIO_PIPE;
    info->handle= vio->hPipe;
    return;
  case VIO_TYPE_SHARED_MEMORY:
    info->protocol= MYSQL_VIO_MEMORY;
#ifdef HAVE_SMEM
    info->handle= vio->handle_file_map; /* or what ? */
#endif
    return;
#endif
  default: DBUG_ASSERT(0);
  }
}


static void client_mpvio_info(MYSQL_PLUGIN_VIO *vio,
                              MYSQL_PLUGIN_VIO_INFO *info)
{
  MCPVIO_EXT *mpvio= (MCPVIO_EXT*)vio;
  mpvio_info(mpvio->mysql->net.vio, info);
}


/**
  Client side of the plugin driver authentication.

  @note this is used by both the mysql_real_connect and mysql_change_user

  @param mysql       mysql
  @param data        pointer to the plugin auth data (scramble) in the
                     handshake packet
  @param data_len    the length of the data
  @param data_plugin a plugin that data were prepared for
                     or 0 if it's mysql_change_user()
  @param db          initial db to use, can be 0

  @retval 0 ok
  @retval 1 error
*/
int run_plugin_auth(MYSQL *mysql, char *data, uint data_len,
                    const char *data_plugin, const char *db)
{
  const char    *auth_plugin_name;
  auth_plugin_t *auth_plugin;
  MCPVIO_EXT    mpvio;
  ulong		pkt_length;
  int           res;

  DBUG_ENTER ("run_plugin_auth");
  /* determine the default/initial plugin to use */
  if (mysql->options.extension && mysql->options.extension->default_auth &&
      mysql->server_capabilities & CLIENT_PLUGIN_AUTH)
  {
    auth_plugin_name= mysql->options.extension->default_auth;
    if (!(auth_plugin= (auth_plugin_t*) mysql_client_find_plugin(mysql,
                       auth_plugin_name, MYSQL_CLIENT_AUTHENTICATION_PLUGIN)))
      DBUG_RETURN (1); /* oops, not found */
  }
  else
  {
    auth_plugin= mysql->server_capabilities & CLIENT_PROTOCOL_41 ?
      &native_password_client_plugin : &old_password_client_plugin;
    auth_plugin_name= auth_plugin->name;
  }

  DBUG_PRINT ("info", ("using plugin %s", auth_plugin_name));

  mysql->net.last_errno= 0; /* just in case */

  if (data_plugin && strcmp(data_plugin, auth_plugin_name))
  {
    /* data was prepared for a different plugin, don't show it to this one */
    data= 0;
    data_len= 0;
  }

  mpvio.mysql_change_user= data_plugin == 0;
  mpvio.cached_server_reply.pkt= (uchar*)data;
  mpvio.cached_server_reply.pkt_len= data_len;
  mpvio.read_packet= client_mpvio_read_packet;
  mpvio.write_packet= client_mpvio_write_packet;
  mpvio.info= client_mpvio_info;
  mpvio.mysql= mysql;
  mpvio.packets_read= mpvio.packets_written= 0;
  mpvio.db= db;
  mpvio.plugin= auth_plugin;

  res= auth_plugin->authenticate_user((struct st_plugin_vio *)&mpvio, mysql);
  DBUG_PRINT ("info", ("authenticate_user returned %s", 
                       res == CR_OK ? "CR_OK" : 
                       res == CR_ERROR ? "CR_ERROR" :
                       res == CR_OK_HANDSHAKE_COMPLETE ? 
                         "CR_OK_HANDSHAKE_COMPLETE" : "error"));

  compile_time_assert(CR_OK == -1);
  compile_time_assert(CR_ERROR == 0);
  if (res > CR_OK && mysql->net.read_pos[0] != 254)
  {
    /*
      the plugin returned an error. write it down in mysql,
      unless the error code is CR_ERROR and mysql->net.last_errno
      is already set (the plugin has done it)
    */
    DBUG_PRINT ("info", ("res=%d", res));
    if (res > CR_ERROR)
      set_mysql_error(mysql, res, unknown_sqlstate);
    else
      if (!mysql->net.last_errno)
        set_mysql_error(mysql, CR_UNKNOWN_ERROR, unknown_sqlstate);
    DBUG_RETURN (1);
  }

  /* read the OK packet (or use the cached value in mysql->net.read_pos */
  if (res == CR_OK)
    pkt_length= (*mysql->methods->read_change_user_result)(mysql);
  else /* res == CR_OK_HANDSHAKE_COMPLETE */
    pkt_length= mpvio.last_read_packet_len;

  DBUG_PRINT ("info", ("OK packet length=%lu", pkt_length));
  if (pkt_length == packet_error)
  {
    if (mysql->net.last_errno == CR_SERVER_LOST)
      set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                               ER(CR_SERVER_LOST_EXTENDED),
                               "reading authorization packet",
                               errno);
    DBUG_RETURN (1);
  }

  if (mysql->net.read_pos[0] == 254)
  {
    /* The server asked to use a different authentication plugin */
    if (pkt_length == 1)
    {
      /* old "use short scramble" packet */
      DBUG_PRINT ("info", ("old use short scramble packet from server"));
      auth_plugin_name= old_password_plugin_name;
      mpvio.cached_server_reply.pkt= (uchar*)mysql->scramble;
      mpvio.cached_server_reply.pkt_len= SCRAMBLE_LENGTH + 1;
    }
    else
    {
      /* new "use different plugin" packet */
      uint len;
      auth_plugin_name= (char*)mysql->net.read_pos + 1;
      len= strlen(auth_plugin_name); /* safe as my_net_read always appends \0 */
      mpvio.cached_server_reply.pkt_len= pkt_length - len - 2;
      mpvio.cached_server_reply.pkt= mysql->net.read_pos + len + 2;
      DBUG_PRINT ("info", ("change plugin packet from server for plugin %s",
                           auth_plugin_name));
    }

    if (!(auth_plugin= (auth_plugin_t *) mysql_client_find_plugin(mysql,
                         auth_plugin_name, MYSQL_CLIENT_AUTHENTICATION_PLUGIN)))
      DBUG_RETURN (1);

    mpvio.plugin= auth_plugin;
    res= auth_plugin->authenticate_user((struct st_plugin_vio *)&mpvio, mysql);

    DBUG_PRINT ("info", ("second authenticate_user returned %s", 
                         res == CR_OK ? "CR_OK" : 
                         res == CR_ERROR ? "CR_ERROR" :
                         res == CR_OK_HANDSHAKE_COMPLETE ? 
                         "CR_OK_HANDSHAKE_COMPLETE" : "error"));
    if (res > CR_OK)
    {
      if (res > CR_ERROR)
        set_mysql_error(mysql, res, unknown_sqlstate);
      else
        if (!mysql->net.last_errno)
          set_mysql_error(mysql, CR_UNKNOWN_ERROR, unknown_sqlstate);
      DBUG_RETURN (1);
    }

    if (res != CR_OK_HANDSHAKE_COMPLETE)
    {
      /* Read what server thinks about out new auth message report */
      if (cli_safe_read(mysql) == packet_error)
      {
        if (mysql->net.last_errno == CR_SERVER_LOST)
          set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                                   ER(CR_SERVER_LOST_EXTENDED),
                                   "reading final connect information",
                                   errno);
        DBUG_RETURN (1);
      }
    }
  }
  /*
    net->read_pos[0] should always be 0 here if the server implements
    the protocol correctly
  */
  DBUG_RETURN (mysql->net.read_pos[0] != 0);
}


static int
connect_sync_or_async(MYSQL *mysql, NET *net, my_socket fd,
                      const struct sockaddr *name, uint namelen)
{
  if (mysql->options.extension && mysql->options.extension->async_context &&
      mysql->options.extension->async_context->active)
  {
    my_bool old_mode;
    vio_blocking(net->vio, FALSE, &old_mode);
    return my_connect_async(mysql->options.extension->async_context, fd,
                            name, namelen, mysql->options.connect_timeout);
  }

  return my_connect(fd, name, namelen, mysql->options.connect_timeout);
}

MYSQL * STDCALL 
CLI_MYSQL_REAL_CONNECT(MYSQL *mysql,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag)
{
  char		buff[NAME_LEN+USERNAME_LENGTH+100];
  int           scramble_data_len, pkt_scramble_len= 0;
  char          *end,*host_info= 0, *server_version_end, *pkt_end;
  char          *scramble_data;
  const char    *scramble_plugin;
  ulong		pkt_length;
  NET		*net= &mysql->net;
#ifdef __WIN__
  HANDLE	hPipe=INVALID_HANDLE_VALUE;
#endif
#ifdef HAVE_SYS_UN_H
  struct	sockaddr_un UNIXaddr;
#endif
  DBUG_ENTER("mysql_real_connect");
  LINT_INIT(pkt_scramble_len);

  DBUG_PRINT("enter",("host: %s  db: %s  user: %s (client)",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));

  /* Test whether we're already connected */
  if (net->vio)
  {
    set_mysql_error(mysql, CR_ALREADY_CONNECTED, unknown_sqlstate);
    DBUG_RETURN(0);
  }

  mysql->methods= &client_methods;
  net->vio = 0;				/* If something goes wrong */
  mysql->client_flag=0;			/* For handshake */

  /* use default options */
  if (mysql->options.my_cnf_file || mysql->options.my_cnf_group)
  {
    mysql_read_default_options(&mysql->options,
			       (mysql->options.my_cnf_file ?
				mysql->options.my_cnf_file : "my"),
			       mysql->options.my_cnf_group);
    my_free(mysql->options.my_cnf_file);
    my_free(mysql->options.my_cnf_group);
    mysql->options.my_cnf_file=mysql->options.my_cnf_group=0;
  }

  /* Some empty-string-tests are done because of ODBC */
  if (!host || !host[0])
    host=mysql->options.host;
  if (!user || !user[0])
  {
    user=mysql->options.user;
    if (!user)
      user= "";
  }
  if (!passwd)
  {
    passwd=mysql->options.password;
#if !defined(DONT_USE_MYSQL_PWD) && !defined(MYSQL_SERVER)
    if (!passwd)
      passwd=getenv("MYSQL_PWD");		/* get it from environment */
#endif
    if (!passwd)
      passwd= "";
  }
  if (!db || !db[0])
    db=mysql->options.db;
  if (!port)
    port=mysql->options.port;
  if (!unix_socket)
    unix_socket=mysql->options.unix_socket;

  mysql->server_status=SERVER_STATUS_AUTOCOMMIT;
  DBUG_PRINT("info", ("Connecting"));

  /*
    Part 0: Grab a socket and connect it to the server
  */
#if defined(HAVE_SMEM)
  if ((!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_MEMORY) &&
      (!host || !strcmp(host,LOCAL_HOST)) &&
      mysql->options.shared_memory_base_name)
  {
    DBUG_PRINT("info", ("Using shared memory"));
    if ((create_shared_memory(mysql,net, mysql->options.connect_timeout)) ==
	INVALID_HANDLE_VALUE)
    {
      DBUG_PRINT("error",
		 ("host: '%s'  socket: '%s'  shared memory: %s  have_tcpip: %d",
		  host ? host : "<null>",
		  unix_socket ? unix_socket : "<null>",
		  mysql->options.shared_memory_base_name,
		  (int) have_tcpip));
      if (mysql->options.protocol == MYSQL_PROTOCOL_MEMORY)
	goto error;

      /*
        Try also with PIPE or TCP/IP. Clear the error from
        create_shared_memory().
      */

      net_clear_error(net);
    }
    else
    {
      mysql->options.protocol=MYSQL_PROTOCOL_MEMORY;
      unix_socket = 0;
      host=mysql->options.shared_memory_base_name;
      my_snprintf(host_info=buff, sizeof(buff)-1,
                  ER(CR_SHARED_MEMORY_CONNECTION), host);
    }
  }
#endif /* HAVE_SMEM */
#if defined(HAVE_SYS_UN_H)
  if (!net->vio &&
      (!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_SOCKET) &&
      (unix_socket || mysql_unix_port) &&
      (!host || !strcmp(host,LOCAL_HOST)))
  {
    my_socket sock= socket(AF_UNIX, SOCK_STREAM, 0);
    DBUG_PRINT("info", ("Using socket"));
    if (sock == SOCKET_ERROR)
    {
      set_mysql_extended_error(mysql, CR_SOCKET_CREATE_ERROR,
                               unknown_sqlstate,
                               ER(CR_SOCKET_CREATE_ERROR),
                               socket_errno);
      goto error;
    }

    net->vio= vio_new(sock, VIO_TYPE_SOCKET,
                      VIO_LOCALHOST | VIO_BUFFERED_READ);
    if (!net->vio)
    {
      DBUG_PRINT("error",("Unknow protocol %d ", mysql->options.protocol));
      set_mysql_error(mysql, CR_CONN_UNKNOW_PROTOCOL, unknown_sqlstate);
      closesocket(sock);
      goto error;
    }

    host= LOCAL_HOST;
    if (!unix_socket)
      unix_socket= mysql_unix_port;
    host_info= (char*) ER(CR_LOCALHOST_CONNECTION);
    DBUG_PRINT("info", ("Using UNIX sock '%s'", unix_socket));

    bzero((char*) &UNIXaddr, sizeof(UNIXaddr));
    UNIXaddr.sun_family= AF_UNIX;
    strmake_buf(UNIXaddr.sun_path, unix_socket);
    if (connect_sync_or_async(mysql, net, sock,
                              (struct sockaddr *) &UNIXaddr, sizeof(UNIXaddr)))
    {
      DBUG_PRINT("error",("Got error %d on connect to local server",
			  socket_errno));
      set_mysql_extended_error(mysql, CR_CONNECTION_ERROR,
                               unknown_sqlstate,
                               ER(CR_CONNECTION_ERROR),
                               unix_socket, socket_errno);
      vio_delete(net->vio);
      net->vio= 0;
      goto error;
    }
    mysql->options.protocol=MYSQL_PROTOCOL_SOCKET;
  }
#elif defined(__WIN__)
  if (!net->vio &&
      (mysql->options.protocol == MYSQL_PROTOCOL_PIPE ||
       (host && !strcmp(host,LOCAL_HOST_NAMEDPIPE)) ||
       (! have_tcpip && (unix_socket || !host && is_NT()))))
  {
    if ((hPipe= create_named_pipe(mysql, mysql->options.connect_timeout,
                                  (char**) &host, (char**) &unix_socket)) ==
	INVALID_HANDLE_VALUE)
    {
      DBUG_PRINT("error",
		 ("host: '%s'  socket: '%s'  have_tcpip: %d",
		  host ? host : "<null>",
		  unix_socket ? unix_socket : "<null>",
		  (int) have_tcpip));
      if (mysql->options.protocol == MYSQL_PROTOCOL_PIPE ||
	  (host && !strcmp(host,LOCAL_HOST_NAMEDPIPE)) ||
	  (unix_socket && !strcmp(unix_socket,MYSQL_NAMEDPIPE)))
	goto error;
      /* Try also with TCP/IP */
    }
    else
    {
      net->vio= vio_new_win32pipe(hPipe);
      my_snprintf(host_info=buff, sizeof(buff)-1,
                  ER(CR_NAMEDPIPE_CONNECTION), unix_socket);
    }
  }
#endif
  DBUG_PRINT("info", ("net->vio: %p  protocol: %d",
                      net->vio, mysql->options.protocol));
  if (!net->vio &&
      (!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_TCP))
  {
    struct addrinfo *res_lst, hints, *t_res;
    int gai_errno;
    char port_buf[NI_MAXSERV];
    my_socket sock= SOCKET_ERROR;
    int saved_error= 0, status= -1;

    unix_socket=0;				/* This is not used */

    if (!port)
      port= mysql_port;

    if (!host)
      host= LOCAL_HOST;

    my_snprintf(host_info=buff, sizeof(buff)-1, ER(CR_TCP_CONNECTION), host);
    DBUG_PRINT("info",("Server name: '%s'.  TCP sock: %d", host, port));
    DBUG_PRINT("info",("IP '%s'", "client"));

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype= SOCK_STREAM;
    hints.ai_protocol= IPPROTO_TCP;
    hints.ai_family= AF_UNSPEC;

    DBUG_PRINT("info",("IPV6 getaddrinfo %s", host));
    my_snprintf(port_buf, NI_MAXSERV, "%d", port);
    gai_errno= getaddrinfo(host, port_buf, &hints, &res_lst);

    if (gai_errno != 0) 
    { 
      /* 
        For DBUG we are keeping the right message but for client we default to
        historical error message.
      */
      DBUG_PRINT("info",("IPV6 getaddrinfo error %d", gai_errno));
      set_mysql_extended_error(mysql, CR_UNKNOWN_HOST, unknown_sqlstate,
                               ER(CR_UNKNOWN_HOST), host, errno);

      goto error;
    }

    /*
      A hostname might map to multiple IP addresses (IPv4/IPv6). Go over the
      list of IP addresses until a successful connection can be established.
    */
    DBUG_PRINT("info", ("Try connect on all addresses for host."));
    for (t_res= res_lst; t_res; t_res= t_res->ai_next)
    {
      DBUG_PRINT("info", ("Create socket, family: %d  type: %d  proto: %d",
                          t_res->ai_family, t_res->ai_socktype,
                          t_res->ai_protocol));
      sock= socket(t_res->ai_family, t_res->ai_socktype, t_res->ai_protocol);
      if (sock == SOCKET_ERROR)
      {
        saved_error= socket_errno;
        continue;
      }

      net->vio= vio_new(sock, VIO_TYPE_TCPIP, VIO_BUFFERED_READ);
      if (!net->vio)
      {
        set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
        closesocket(sock);
        freeaddrinfo(res_lst);
        goto error;
      }

      DBUG_PRINT("info", ("Connect socket"));
      status= connect_sync_or_async(mysql, net, sock,
                                    t_res->ai_addr, t_res->ai_addrlen);
      /*
        Here we rely on my_connect() to return success only if the
        connect attempt was really successful. Otherwise we would stop
        trying another address, believing we were successful.
      */
      if (!status)
        break;

      /*
        Save value as socket errno might be overwritten due to
        calling a socket function below.
      */
      saved_error= socket_errno;

      DBUG_PRINT("info", ("No success, close socket, try next address."));
      vio_delete(mysql->net.vio);
      mysql->net.vio= 0;
    }
    DBUG_PRINT("info",
               ("End of connect attempts, sock: %d  status: %d  error: %d",
                sock, status, saved_error));

    freeaddrinfo(res_lst);

    if (sock == SOCKET_ERROR)
    {
      set_mysql_extended_error(mysql, CR_IPSOCK_ERROR, unknown_sqlstate,
                                ER(CR_IPSOCK_ERROR), saved_error);
      goto error;
    }

    if (status)
    {
      DBUG_PRINT("error",("Got error %d on connect to '%s'", saved_error, host));
      set_mysql_extended_error(mysql, CR_CONN_HOST_ERROR, unknown_sqlstate,
                                ER(CR_CONN_HOST_ERROR), host, saved_error);
      goto error;
    }
  }

  DBUG_PRINT("info", ("net->vio: %p", net->vio));
  if (!net->vio)
  {
    DBUG_PRINT("error",("Unknow protocol %d ",mysql->options.protocol));
    set_mysql_error(mysql, CR_CONN_UNKNOW_PROTOCOL, unknown_sqlstate);
    goto error;
  }

  if (mysql->options.extension && mysql->options.extension->async_context)
    net->vio->async_context= mysql->options.extension->async_context;

  if (my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0;
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    goto error;
  }
  vio_keepalive(net->vio,TRUE);

  /* If user set read_timeout, let it override the default */
  if (mysql->options.read_timeout)
    my_net_set_read_timeout(net, mysql->options.read_timeout);

  /* If user set write_timeout, let it override the default */
  if (mysql->options.write_timeout)
    my_net_set_write_timeout(net, mysql->options.write_timeout);

  if (mysql->options.max_allowed_packet)
    net->max_packet_size= mysql->options.max_allowed_packet;

  /* Get version info */
  mysql->protocol_version= PROTOCOL_VERSION;	/* Assume this */
  if (mysql->options.connect_timeout &&
      vio_poll_read(net->vio, mysql->options.connect_timeout))
  {
    set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                             ER(CR_SERVER_LOST_EXTENDED),
                             "waiting for initial communication packet",
                             errno);
    goto error;
  }

  /*
    Part 1: Connection established, read and parse first packet
  */
  DBUG_PRINT("info", ("Read first packet."));

  if ((pkt_length=cli_safe_read(mysql)) == packet_error)
  {
    if (mysql->net.last_errno == CR_SERVER_LOST)
      set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                               ER(CR_SERVER_LOST_EXTENDED),
                               "reading initial communication packet",
                               errno);
    goto error;
  }
  pkt_end= (char*)net->read_pos + pkt_length;
  /* Check if version of protocol matches current one */
  mysql->protocol_version= net->read_pos[0];
  DBUG_DUMP("packet",(uchar*) net->read_pos,10);
  DBUG_PRINT("info",("mysql protocol version %d, server=%d",
		     PROTOCOL_VERSION, mysql->protocol_version));
  if (mysql->protocol_version != PROTOCOL_VERSION)
  {
    set_mysql_extended_error(mysql, CR_VERSION_ERROR, unknown_sqlstate,
                             ER(CR_VERSION_ERROR), mysql->protocol_version,
                             PROTOCOL_VERSION);
    goto error;
  }
  server_version_end= end= strend((char*) net->read_pos+1);
  mysql->thread_id=uint4korr(end+1);
  end+=5;
  /* 
    Scramble is split into two parts because old clients do not understand
    long scrambles; here goes the first part.
  */
  scramble_data= end;
  scramble_data_len= SCRAMBLE_LENGTH_323 + 1;
  scramble_plugin= old_password_plugin_name;
  end+= scramble_data_len;

  if (pkt_end >= end + 1)
    mysql->server_capabilities=uint2korr(end);
  if (pkt_end >= end + 18)
  {
    /* New protocol with 16 bytes to describe server characteristics */
    mysql->server_language=end[2];
    mysql->server_status=uint2korr(end+3);
    mysql->server_capabilities|= uint2korr(end+5) << 16;
    pkt_scramble_len= end[7];
    if (pkt_scramble_len < 0)
    {
      set_mysql_error(mysql, CR_MALFORMED_PACKET,
                      unknown_sqlstate);        /* purecov: inspected */
      goto error;
    }
  }
  end+= 18;

  if (mysql->options.secure_auth && passwd[0] &&
      !(mysql->server_capabilities & CLIENT_SECURE_CONNECTION))
  {
    set_mysql_error(mysql, CR_SECURE_AUTH, unknown_sqlstate);
    goto error;
  }

  if (mysql_init_character_set(mysql))
    goto error;

  /* Save connection information */
  if (!my_multi_malloc(MYF(0),
		       &mysql->host_info, (uint) strlen(host_info)+1,
		       &mysql->host,      (uint) strlen(host)+1,
		       &mysql->unix_socket,unix_socket ?
		       (uint) strlen(unix_socket)+1 : (uint) 1,
		       &mysql->server_version,
		       (uint) (server_version_end - (char*) net->read_pos + 1),
		       NullS) ||
      !(mysql->user=my_strdup(user,MYF(0))) ||
      !(mysql->passwd=my_strdup(passwd,MYF(0))))
  {
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    goto error;
  }
  strmov(mysql->host_info,host_info);
  strmov(mysql->host,host);
  if (unix_socket)
    strmov(mysql->unix_socket,unix_socket);
  else
    mysql->unix_socket=0;
  strmov(mysql->server_version,(char*) net->read_pos+1);
  mysql->port=port;

  /*
    remove the rpl hack from the version string,
    see RPL_VERSION_HACK comment
  */
  if ((mysql->server_capabilities & CLIENT_PLUGIN_AUTH) &&
      strncmp(mysql->server_version, RPL_VERSION_HACK,
              sizeof(RPL_VERSION_HACK) - 1) == 0)
    mysql->server_version+= sizeof(RPL_VERSION_HACK) - 1;

  if (pkt_end >= end + SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1)
  {
    /*
     move the first scramble part - directly in the NET buffer -
     to get a full continuous scramble. We've read all the header,
     and can overwrite it now.
    */
    memmove(end - SCRAMBLE_LENGTH_323, scramble_data,
            SCRAMBLE_LENGTH_323);
    scramble_data= end - SCRAMBLE_LENGTH_323;
    if (mysql->server_capabilities & CLIENT_PLUGIN_AUTH)
    {
      scramble_data_len= pkt_scramble_len;
      scramble_plugin= scramble_data + scramble_data_len;
      if (scramble_data + scramble_data_len > pkt_end)
        scramble_data_len= pkt_end - scramble_data;
    }
    else
    {
      scramble_data_len= pkt_end - scramble_data;
      scramble_plugin= native_password_plugin_name;
    }
  }
  else
    mysql->server_capabilities&= ~CLIENT_SECURE_CONNECTION;

  mysql->client_flag= client_flag;

  /*
    Part 2: invoke the plugin to send the authentication data to the server
  */

  if (run_plugin_auth(mysql, scramble_data, scramble_data_len,
                      scramble_plugin, db))
    goto error;

  /*
    Part 3: authenticated, finish the initialization of the connection
  */

  if (mysql->client_flag & CLIENT_COMPRESS)      /* We will use compression */
    net->compress=1;

  if (db && !mysql->db && mysql_select_db(mysql, db))
  {
    if (mysql->net.last_errno == CR_SERVER_LOST)
        set_mysql_extended_error(mysql, CR_SERVER_LOST, unknown_sqlstate,
                                 ER(CR_SERVER_LOST_EXTENDED),
                                 "Setting intital database",
                                 errno);
    goto error;
  }

  /*
     Using init_commands is not supported when connecting from within the
     server.
  */
#ifndef MYSQL_SERVER
  if (mysql->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end_command= ptr + init_commands->elements;

    my_bool reconnect=mysql->reconnect;
    mysql->reconnect=0;

    for (; ptr < end_command; ptr++)
    {
      int status;

      if (mysql_real_query(mysql,*ptr, (ulong) strlen(*ptr)))
	goto error;

      do {
        if (mysql->fields)
        {
          MYSQL_RES *res;
          if (!(res= cli_use_result(mysql)))
            goto error;
          mysql_free_result(res);
        }
        if ((status= mysql_next_result(mysql)) > 0)
          goto error;
      } while (status == 0);
    }
    mysql->reconnect=reconnect;
  }
#endif

  DBUG_PRINT("exit", ("Mysql handler: 0x%lx", (long) mysql));
  DBUG_RETURN(mysql);

error:
  DBUG_PRINT("error",("message: %u/%s (%s)",
                      net->last_errno,
                      net->sqlstate,
                      net->last_error));
  {
    /* Free alloced memory */
    end_server(mysql);
    mysql_close_free(mysql);
  }
  DBUG_RETURN(0);
}


struct my_hook_data {
  MYSQL *orig_mysql;
  MYSQL *new_mysql;
  /* This is always NULL currently, but restoring does not hurt just in case. */
  Vio *orig_vio;
};
/*
  Callback hook to make the new VIO accessible via the old MYSQL to calling
  application when suspending a non-blocking call during automatic reconnect.
*/
static void
my_suspend_hook(my_bool suspend, void *data)
{
  struct my_hook_data *hook_data= (struct my_hook_data *)data;
  if (suspend)
  {
    hook_data->orig_vio= hook_data->orig_mysql->net.vio;
    hook_data->orig_mysql->net.vio= hook_data->new_mysql->net.vio;
  }
  else
    hook_data->orig_mysql->net.vio= hook_data->orig_vio;
}

my_bool mysql_reconnect(MYSQL *mysql)
{
  MYSQL tmp_mysql;
  struct my_hook_data hook_data;
  struct mysql_async_context *ctxt= NULL;
  DBUG_ENTER("mysql_reconnect");
  DBUG_ASSERT(mysql);
  DBUG_PRINT("enter", ("mysql->reconnect: %d", mysql->reconnect));

  if (!mysql->reconnect ||
      (mysql->server_status & SERVER_STATUS_IN_TRANS) || !mysql->host_info)
  {
    /* Allow reconnect next time */
    mysql->server_status&= ~SERVER_STATUS_IN_TRANS;
    set_mysql_error(mysql, CR_SERVER_GONE_ERROR, unknown_sqlstate);
    DBUG_RETURN(1);
  }
  mysql_init(&tmp_mysql);
  tmp_mysql.options= mysql->options;
  tmp_mysql.options.my_cnf_file= tmp_mysql.options.my_cnf_group= 0;

  /*
    If we are automatically re-connecting inside a non-blocking API call, we
    may need to suspend and yield to the user application during the reconnect.
    If so, the user application will need access to the new VIO already then
    so that it can correctly wait for I/O to become ready.
    To achieve this, we temporarily install a hook that will temporarily put in
    the VIO while we are suspended.
    (The vio will be put in the original MYSQL permanently once we successfully
    reconnect, or be discarded if we fail to reconnect.)
  */
  if (mysql->options.extension &&
      (ctxt= mysql->options.extension->async_context) &&
      mysql->options.extension->async_context->active)
  {
    hook_data.orig_mysql= mysql;
    hook_data.new_mysql= &tmp_mysql;
    hook_data.orig_vio= mysql->net.vio;
    my_context_install_suspend_resume_hook(ctxt, my_suspend_hook, &hook_data);
  }
  if (!mysql_real_connect(&tmp_mysql,mysql->host,mysql->user,mysql->passwd,
			  mysql->db, mysql->port, mysql->unix_socket,
			  mysql->client_flag))
  {
    if (ctxt)
      my_context_install_suspend_resume_hook(ctxt, NULL, NULL);
    mysql->net.last_errno= tmp_mysql.net.last_errno;
    strmov(mysql->net.last_error, tmp_mysql.net.last_error);
    strmov(mysql->net.sqlstate, tmp_mysql.net.sqlstate);
    DBUG_RETURN(1);
  }
  if (mysql_set_character_set(&tmp_mysql, mysql->charset->csname))
  {
    DBUG_PRINT("error", ("mysql_set_character_set() failed"));
    bzero((char*) &tmp_mysql.options,sizeof(tmp_mysql.options));
    mysql_close(&tmp_mysql);
    if (ctxt)
      my_context_install_suspend_resume_hook(ctxt, NULL, NULL);
    mysql->net.last_errno= tmp_mysql.net.last_errno;
    strmov(mysql->net.last_error, tmp_mysql.net.last_error);
    strmov(mysql->net.sqlstate, tmp_mysql.net.sqlstate);
    DBUG_RETURN(1);
  }
  if (ctxt)
    my_context_install_suspend_resume_hook(ctxt, NULL, NULL);

  DBUG_PRINT("info", ("reconnect succeded"));
  tmp_mysql.reconnect= 1;
  tmp_mysql.free_me= mysql->free_me;

  /* Move prepared statements (if any) over to the new mysql object */
  tmp_mysql.stmts= mysql->stmts;
  mysql->stmts= 0;

  /* Don't free options as these are now used in tmp_mysql */
  bzero((char*) &mysql->options,sizeof(mysql->options));
  mysql->free_me=0;
  mysql_close(mysql);
  *mysql=tmp_mysql;
  net_clear(&mysql->net, 1);
  mysql->affected_rows= ~(my_ulonglong) 0;
  DBUG_RETURN(0);
}


/**************************************************************************
  Set current database
**************************************************************************/

int STDCALL
mysql_select_db(MYSQL *mysql, const char *db)
{
  int error;
  DBUG_ENTER("mysql_select_db");
  DBUG_PRINT("enter",("db: '%s'",db));

  if ((error=simple_command(mysql,COM_INIT_DB, (const uchar*) db,
                            (ulong) strlen(db),0)))
    DBUG_RETURN(error);
  my_free(mysql->db);
  mysql->db=my_strdup(db,MYF(MY_WME));
  DBUG_RETURN(0);
}


/*************************************************************************
  Send a QUIT to the server and close the connection
  If handle is alloced by mysql connect free it.
*************************************************************************/

static void mysql_close_free_options(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close_free_options");

  my_free(mysql->options.user);
  my_free(mysql->options.host);
  my_free(mysql->options.password);
  my_free(mysql->options.unix_socket);
  my_free(mysql->options.db);
  my_free(mysql->options.my_cnf_file);
  my_free(mysql->options.my_cnf_group);
  my_free(mysql->options.charset_dir);
  my_free(mysql->options.charset_name);
  my_free(mysql->options.client_ip);
  if (mysql->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end= ptr + init_commands->elements;
    for (; ptr<end; ptr++)
      my_free(*ptr);
    delete_dynamic(init_commands);
    my_free(init_commands);
  }
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  mysql_ssl_free(mysql);
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
#ifdef HAVE_SMEM
  if (mysql->options.shared_memory_base_name != def_shared_memory_base_name)
    my_free(mysql->options.shared_memory_base_name);
#endif /* HAVE_SMEM */
  if (mysql->options.extension)
  {
    struct mysql_async_context *ctxt= mysql->options.extension->async_context;
    my_free(mysql->options.extension->plugin_dir);
    my_free(mysql->options.extension->default_auth);
    if (ctxt)
    {
      my_context_destroy(&ctxt->async_context);
      my_free(ctxt);
    }
    my_free(mysql->options.extension);
  }
  bzero((char*) &mysql->options,sizeof(mysql->options));
  DBUG_VOID_RETURN;
}


static void mysql_close_free(MYSQL *mysql)
{
  my_free(mysql->host_info);
  my_free(mysql->user);
  my_free(mysql->passwd);
  my_free(mysql->db);
#if defined(EMBEDDED_LIBRARY) || MYSQL_VERSION_ID >= 50100
  my_free(mysql->info_buffer);
  mysql->info_buffer= 0;
#endif
  /* Clear pointers for better safety */
  mysql->host_info= mysql->user= mysql->passwd= mysql->db= 0;
}


/**
  For use when the connection to the server has been lost (in which case 
  the server has discarded all information about prepared statements
  associated with the connection).

  Mark all statements in mysql->stmts by setting stmt->mysql= 0 if the
  statement has transitioned beyond the MYSQL_STMT_INIT_DONE state, and
  unlink the statement from the mysql->stmts list.

  The remaining pruned list of statements (if any) is kept in mysql->stmts.

  @param mysql       pointer to the MYSQL object

  @return none
*/
static void mysql_prune_stmt_list(MYSQL *mysql)
{
  LIST *element= mysql->stmts;
  LIST *pruned_list= 0;

  for (; element; element= element->next)
  {
    MYSQL_STMT *stmt= (MYSQL_STMT *) element->data;
    if (stmt->state != MYSQL_STMT_INIT_DONE)
    {
      stmt->mysql= 0;
      stmt->last_errno= CR_SERVER_LOST;
      strmov(stmt->last_error, ER(CR_SERVER_LOST));
      strmov(stmt->sqlstate, unknown_sqlstate);
    }
    else
    {
      pruned_list= list_add(pruned_list, element);
    }
  }

  mysql->stmts= pruned_list;
}


/*
  Clear connection pointer of every statement: this is necessary
  to give error on attempt to use a prepared statement of closed
  connection.

  SYNOPSYS
    mysql_detach_stmt_list()
      stmt_list  pointer to mysql->stmts
      func_name  name of calling function

  NOTE
    There is similar code in mysql_reconnect(), so changes here
    should also be reflected there.
*/

void mysql_detach_stmt_list(LIST **stmt_list __attribute__((unused)),
                            const char *func_name __attribute__((unused)))
{
#ifdef MYSQL_CLIENT
  /* Reset connection handle in all prepared statements. */
  LIST *element= *stmt_list;
  char buff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("mysql_detach_stmt_list");

  my_snprintf(buff, sizeof(buff)-1, ER(CR_STMT_CLOSED), func_name);
  for (; element; element= element->next)
  {
    MYSQL_STMT *stmt= (MYSQL_STMT *) element->data;
    set_stmt_error(stmt, CR_STMT_CLOSED, unknown_sqlstate, buff);
    stmt->mysql= 0;
    /* No need to call list_delete for statement here */
  }
  *stmt_list= 0;
  DBUG_VOID_RETURN;
#endif /* MYSQL_CLIENT */
}


/*
  Close a MySQL connection and free all resources attached to it.

  This function is coded in such that it can be called multiple times
  (As some clients call this after mysql_real_connect() fails)
*/

/*
  mysql_close() can actually block, at least in theory, if the socket buffer
  is full when sending the COM_QUIT command.

  On the other hand, the latter part of mysql_close() needs to free the stack
  used for non-blocking operation of blocking stuff, so that later part can
  _not_ be done non-blocking.

  Therefore, mysql_close_slow_part() is used to run the parts of mysql_close()
  that may block. It can be called before mysql_close(), and in that case
  mysql_close() is guaranteed not to need to block.  */
void STDCALL mysql_close_slow_part(MYSQL *mysql)
{
  /* If connection is still up, send a QUIT message */
  if (mysql->net.vio != 0)
  {
    free_old_query(mysql);
    mysql->status=MYSQL_STATUS_READY; /* Force command */
    mysql->reconnect=0;
    simple_command(mysql,COM_QUIT,(uchar*) 0,0,1);
    end_server(mysql);			/* Sets mysql->net.vio= 0 */
  }
}

void STDCALL mysql_close(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close");
  DBUG_PRINT("enter", ("mysql: 0x%lx", (long) mysql));

  if (mysql)					/* Some simple safety */
  {
    mysql_close_slow_part(mysql);
    mysql_close_free_options(mysql);
    mysql_close_free(mysql);
    mysql_detach_stmt_list(&mysql->stmts, "mysql_close");
#ifndef MYSQL_SERVER
    if (mysql->thd)
    {
      (*mysql->methods->free_embedded_thd)(mysql);
      mysql->thd= 0;
    }
#endif
    if (mysql->free_me)
      my_free(mysql);
  }
  DBUG_VOID_RETURN;
}


static my_bool cli_read_query_result(MYSQL *mysql)
{
  uchar *pos;
  ulong field_count;
  MYSQL_DATA *fields;
  ulong length;
  DBUG_ENTER("cli_read_query_result");

  if ((length = cli_safe_read(mysql)) == packet_error)
    DBUG_RETURN(1);
  free_old_query(mysql);		/* Free old result */
#ifdef MYSQL_CLIENT			/* Avoid warn of unused labels*/
get_info:
#endif
  pos=(uchar*) mysql->net.read_pos;
  if ((field_count= net_field_length(&pos)) == 0)
  {
    mysql->affected_rows= net_field_length_ll(&pos);
    mysql->insert_id=	  net_field_length_ll(&pos);
    DBUG_PRINT("info",("affected_rows: %lu  insert_id: %lu",
		       (ulong) mysql->affected_rows,
		       (ulong) mysql->insert_id));
    if (protocol_41(mysql))
    {
      mysql->server_status=uint2korr(pos); pos+=2;
      mysql->warning_count=uint2korr(pos); pos+=2;
    }
    else if (mysql->server_capabilities & CLIENT_TRANSACTIONS)
    {
      /* MySQL 4.0 protocol */
      mysql->server_status=uint2korr(pos); pos+=2;
      mysql->warning_count= 0;
    }
    DBUG_PRINT("info",("status: %u  warning_count: %u",
		       mysql->server_status, mysql->warning_count));
    if (pos < mysql->net.read_pos+length && net_field_length(&pos))
      mysql->info=(char*) pos;
    DBUG_RETURN(0);
  }
#ifdef MYSQL_CLIENT
  if (field_count == NULL_LENGTH)		/* LOAD DATA LOCAL INFILE */
  {
    int error;

    if (!(mysql->options.client_flag & CLIENT_LOCAL_FILES))
    {
      set_mysql_error(mysql, CR_MALFORMED_PACKET, unknown_sqlstate);
      DBUG_RETURN(1);
    }   

    error= handle_local_infile(mysql,(char*) pos);
    if ((length= cli_safe_read(mysql)) == packet_error || error)
      DBUG_RETURN(1);
    goto get_info;				/* Get info packet */
  }
#endif
  if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
    mysql->server_status|= SERVER_STATUS_IN_TRANS;

  if (!(fields=cli_read_rows(mysql,(MYSQL_FIELD*)0, protocol_41(mysql) ? 7:5)))
    DBUG_RETURN(1);
  if (!(mysql->fields=unpack_fields(mysql, fields,&mysql->field_alloc,
				    (uint) field_count,0,
				    mysql->server_capabilities)))
    DBUG_RETURN(1);
  mysql->status= MYSQL_STATUS_GET_RESULT;
  mysql->field_count= (uint) field_count;
  DBUG_PRINT("exit",("ok"));
  DBUG_RETURN(0);
}


/*
  Send the query and return so we can do something else.
  Needs to be followed by mysql_read_query_result() when we want to
  finish processing it.
*/

int STDCALL
mysql_send_query(MYSQL* mysql, const char* query, ulong length)
{
  DBUG_ENTER("mysql_send_query");
  DBUG_RETURN(simple_command(mysql, COM_QUERY, (uchar*) query, length, 1));
}


int STDCALL
mysql_real_query(MYSQL *mysql, const char *query, ulong length)
{
  DBUG_ENTER("mysql_real_query");
  DBUG_PRINT("enter",("handle: 0x%lx", (long) mysql));
  DBUG_PRINT("query",("Query = '%-.4096s'",query));

  if (mysql_send_query(mysql,query,length))
    DBUG_RETURN(1);
  DBUG_RETURN((int) (*mysql->methods->read_query_result)(mysql));
}


/**************************************************************************
  Alloc result struct for buffered results. All rows are read to buffer.
  mysql_data_seek may be used.
**************************************************************************/

MYSQL_RES * STDCALL mysql_store_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_store_result");

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  mysql->status=MYSQL_STATUS_READY;		/* server is ready */
  if (!(result=(MYSQL_RES*) my_malloc((uint) (sizeof(MYSQL_RES)+
					      sizeof(ulong) *
					      mysql->field_count),
				      MYF(MY_WME | MY_ZEROFILL))))
  {
    set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  result->methods= mysql->methods;
  result->eof=1;				/* Marker for buffered */
  result->lengths=(ulong*) (result+1);
  if (!(result->data=
	(*mysql->methods->read_rows)(mysql,mysql->fields,mysql->field_count)))
  {
    my_free(result);
    DBUG_RETURN(0);
  }
  mysql->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=	result->data->data;
  result->fields=	mysql->fields;
  result->field_alloc=	mysql->field_alloc;
  result->field_count=	mysql->field_count;
  /* The rest of result members is bzeroed in malloc */
  mysql->fields=0;				/* fields is now in result */
  clear_alloc_root(&mysql->field_alloc);
  /* just in case this was mistakenly called after mysql_stmt_execute() */
  mysql->unbuffered_fetch_owner= 0;
  DBUG_RETURN(result);				/* Data fetched */
}


/**************************************************************************
  Alloc struct for use with unbuffered reads. Data is fetched by domand
  when calling to mysql_fetch_row.
  mysql_data_seek is a noop.

  No other queries may be specified with the same MYSQL handle.
  There shouldn't be much processing per row because mysql server shouldn't
  have to wait for the client (and will not wait more than 30 sec/packet).
**************************************************************************/

static MYSQL_RES * cli_use_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("cli_use_result");

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    DBUG_RETURN(0);
  }
  if (!(result=(MYSQL_RES*) my_malloc(sizeof(*result)+
				      sizeof(ulong)*mysql->field_count,
				      MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(0);
  result->lengths=(ulong*) (result+1);
  result->methods= mysql->methods;
  if (!(result->row=(MYSQL_ROW)
	my_malloc(sizeof(result->row[0])*(mysql->field_count+1), MYF(MY_WME))))
  {					/* Ptrs: to one row */
    my_free(result);
    DBUG_RETURN(0);
  }
  result->fields=	mysql->fields;
  result->field_alloc=	mysql->field_alloc;
  result->field_count=	mysql->field_count;
  result->current_field=0;
  result->handle=	mysql;
  result->current_row=	0;
  mysql->fields=0;			/* fields is now in result */
  clear_alloc_root(&mysql->field_alloc);
  mysql->status=MYSQL_STATUS_USE_RESULT;
  mysql->unbuffered_fetch_owner= &result->unbuffered_fetch_cancelled;
  DBUG_RETURN(result);			/* Data is read to be fetched */
}


/**************************************************************************
  Return next row of the query results
**************************************************************************/

MYSQL_ROW STDCALL
mysql_fetch_row(MYSQL_RES *res)
{
  DBUG_ENTER("mysql_fetch_row");
  if (!res->data)
  {						/* Unbufferred fetch */
    if (!res->eof)
    {
      MYSQL *mysql= res->handle;
      if (mysql->status != MYSQL_STATUS_USE_RESULT)
      {
        set_mysql_error(mysql,
                        res->unbuffered_fetch_cancelled ? 
                        CR_FETCH_CANCELED : CR_COMMANDS_OUT_OF_SYNC,
                        unknown_sqlstate);
      }
      else if (!(read_one_row(mysql, res->field_count, res->row, res->lengths)))
      {
	res->row_count++;
	DBUG_RETURN(res->current_row=res->row);
      }
      DBUG_PRINT("info",("end of data"));
      res->eof=1;
      mysql->status=MYSQL_STATUS_READY;
      /*
        Reset only if owner points to us: there is a chance that somebody
        started new query after mysql_stmt_close():
      */
      if (mysql->unbuffered_fetch_owner == &res->unbuffered_fetch_cancelled)
        mysql->unbuffered_fetch_owner= 0;
      /* Don't clear handle in mysql_free_result */
      res->handle=0;
    }
    DBUG_RETURN((MYSQL_ROW) NULL);
  }
  {
    MYSQL_ROW tmp;
    if (!res->data_cursor)
    {
      DBUG_PRINT("info",("end of data"));
      DBUG_RETURN(res->current_row=(MYSQL_ROW) NULL);
    }
    tmp = res->data_cursor->data;
    res->data_cursor = res->data_cursor->next;
    DBUG_RETURN(res->current_row=tmp);
  }
}


/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

ulong * STDCALL
mysql_fetch_lengths(MYSQL_RES *res)
{
  MYSQL_ROW column;

  if (!(column=res->current_row))
    return 0;					/* Something is wrong */
  if (res->data)
    (*res->methods->fetch_lengths)(res->lengths, column, res->field_count);
  return res->lengths;
}


#define ASYNC_CONTEXT_DEFAULT_STACK_SIZE (4096*15)

int STDCALL
mysql_options(MYSQL *mysql,enum mysql_option option, const void *arg)
{
  struct mysql_async_context *ctxt;
  size_t stacksize;

  DBUG_ENTER("mysql_options");
  DBUG_PRINT("enter",("option: %d",(int) option));
  switch (option) {
  case MYSQL_OPT_CONNECT_TIMEOUT:
    mysql->options.connect_timeout= *(uint*) arg;
    break;
  case MYSQL_OPT_READ_TIMEOUT:
    mysql->options.read_timeout= *(uint*) arg;
    break;
  case MYSQL_OPT_WRITE_TIMEOUT:
    mysql->options.write_timeout= *(uint*) arg;
    break;
  case MYSQL_OPT_COMPRESS:
    mysql->options.compress= 1;			/* Remember for connect */
    mysql->options.client_flag|= CLIENT_COMPRESS;
    break;
  case MYSQL_OPT_NAMED_PIPE:			/* This option is depricated */
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
    my_free(mysql->options.my_cnf_file);
    mysql->options.my_cnf_file=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_READ_DEFAULT_GROUP:
    my_free(mysql->options.my_cnf_group);
    mysql->options.my_cnf_group=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_SET_CHARSET_DIR:
    my_free(mysql->options.charset_dir);
    mysql->options.charset_dir=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_SET_CHARSET_NAME:
    my_free(mysql->options.charset_name);
    mysql->options.charset_name=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_OPT_PROTOCOL:
    mysql->options.protocol= *(uint*) arg;
    break;
  case MYSQL_SHARED_MEMORY_BASE_NAME:
#ifdef HAVE_SMEM
    if (mysql->options.shared_memory_base_name != def_shared_memory_base_name)
      my_free(mysql->options.shared_memory_base_name);
    mysql->options.shared_memory_base_name=my_strdup(arg,MYF(MY_WME));
#endif
    break;
  case MYSQL_OPT_USE_REMOTE_CONNECTION:
  case MYSQL_OPT_USE_EMBEDDED_CONNECTION:
  case MYSQL_OPT_GUESS_CONNECTION:
    mysql->options.methods_to_use= option;
    break;
  case MYSQL_SET_CLIENT_IP:
    my_free(mysql->options.client_ip);
    mysql->options.client_ip= my_strdup(arg, MYF(MY_WME));
    break;
  case MYSQL_SECURE_AUTH:
    mysql->options.secure_auth= *(my_bool *) arg;
    break;
  case MYSQL_REPORT_DATA_TRUNCATION:
    mysql->options.report_data_truncation= test(*(my_bool *) arg);
    break;
  case MYSQL_OPT_RECONNECT:
    mysql->reconnect= *(my_bool *) arg;
    break;
  case MYSQL_OPT_SSL_VERIFY_SERVER_CERT:
    if (*(my_bool*) arg)
      mysql->options.client_flag|= CLIENT_SSL_VERIFY_SERVER_CERT;
    else
      mysql->options.client_flag&= ~CLIENT_SSL_VERIFY_SERVER_CERT;
    break;
  case MYSQL_PLUGIN_DIR:
    EXTENSION_SET_STRING(&mysql->options, plugin_dir, arg);
    break;
  case MYSQL_DEFAULT_AUTH:
    EXTENSION_SET_STRING(&mysql->options, default_auth, arg);
    break;
  case MYSQL_ENABLE_CLEARTEXT_PLUGIN:
    break;
  case MYSQL_PROGRESS_CALLBACK:
    if (!mysql->options.extension)
      mysql->options.extension= (struct st_mysql_options_extention *)
        my_malloc(sizeof(struct st_mysql_options_extention),
                  MYF(MY_WME | MY_ZEROFILL));
    if (mysql->options.extension)
      mysql->options.extension->report_progress= 
        (void (*)(const MYSQL *, uint, uint, double, const char *, uint)) arg;
    break;
  case MYSQL_OPT_NONBLOCK:
    if (mysql->options.extension &&
        (ctxt = mysql->options.extension->async_context) != 0)
    {
      /*
        We must not allow changing the stack size while a non-blocking call is
        suspended (as the stack is then in use).
      */
      if (ctxt->suspended)
        DBUG_RETURN(1);
      my_context_destroy(&ctxt->async_context);
      my_free(ctxt);
    }
    if (!(ctxt= (struct mysql_async_context *)
          my_malloc(sizeof(*ctxt), MYF(MY_ZEROFILL))))
    {
      set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
      DBUG_RETURN(1);
    }
    stacksize= 0;
    if (arg)
      stacksize= *(const size_t *)arg;
    if (!stacksize)
      stacksize= ASYNC_CONTEXT_DEFAULT_STACK_SIZE;
    if (my_context_init(&ctxt->async_context, stacksize))
    {
      set_mysql_error(mysql, CR_OUT_OF_MEMORY, unknown_sqlstate);
      my_free(ctxt);
      DBUG_RETURN(1);
    }
    EXTENSION_SET(&(mysql->options), async_context, ctxt)
    if (mysql->net.vio)
      mysql->net.vio->async_context= ctxt;
    break;
  default:
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/****************************************************************************
  Functions to get information from the MySQL structure
  These are functions to make shared libraries more usable.
****************************************************************************/

/* MYSQL_RES */
my_ulonglong STDCALL mysql_num_rows(MYSQL_RES *res)
{
  return res->row_count;
}

unsigned int STDCALL mysql_num_fields(MYSQL_RES *res)
{
  return res->field_count;
}

uint STDCALL mysql_errno(MYSQL *mysql)
{
  return mysql ? mysql->net.last_errno : mysql_server_last_errno;
}


const char * STDCALL mysql_error(MYSQL *mysql)
{
  return mysql ? mysql->net.last_error : mysql_server_last_error;
}


/*
  Get version number for server in a form easy to test on

  SYNOPSIS
    mysql_get_server_version()
    mysql		Connection

  EXAMPLE
    MariaDB-4.1.0-alfa ->  40100
  
  NOTES
    We will ensure that a newer server always has a bigger number.

  RETURN
   Signed number > 323000
   Zero if there is no connection
*/

ulong STDCALL
mysql_get_server_version(MYSQL *mysql)
{
  ulong major= 0, minor= 0, version= 0;

  if (mysql->server_version)
  {
    const char *pos= mysql->server_version;
    char *end_pos;
    /* Skip possible prefix */
    while (*pos && !my_isdigit(&my_charset_latin1, *pos))
      pos++;
    major=   strtoul(pos, &end_pos, 10);	pos=end_pos+1;
    minor=   strtoul(pos, &end_pos, 10);	pos=end_pos+1;
    version= strtoul(pos, &end_pos, 10);
  }
  else
  {
    set_mysql_error(mysql, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
  }

  return major*10000 + minor*100 + version;
}


/* 
   mysql_set_character_set function sends SET NAMES cs_name to
   the server (which changes character_set_client, character_set_result
   and character_set_connection) and updates mysql->charset so other
   functions like mysql_real_escape will work correctly.
*/
int STDCALL mysql_set_character_set(MYSQL *mysql, const char *cs_name)
{
  CHARSET_INFO *cs;
  const char *save_csdir= charsets_dir;

  if (mysql->options.charset_dir)
    charsets_dir= mysql->options.charset_dir;

  if (strlen(cs_name) < MY_CS_NAME_SIZE &&
     (cs= get_charset_by_csname(cs_name, MY_CS_PRIMARY, MYF(0))))
  {
    char buff[MY_CS_NAME_SIZE + 10];
    charsets_dir= save_csdir;
    /* Skip execution of "SET NAMES" for pre-4.1 servers */
    if (mysql_get_server_version(mysql) < 40100)
      return 0;
    sprintf(buff, "SET NAMES %s", cs_name);
    if (!mysql_real_query(mysql, buff, (uint) strlen(buff)))
    {
      mysql->charset= cs;
    }
  }
  else
  {
    char cs_dir_name[FN_REFLEN];
    get_charsets_dir(cs_dir_name);
    set_mysql_extended_error(mysql, CR_CANT_READ_CHARSET, unknown_sqlstate,
                             ER(CR_CANT_READ_CHARSET), cs_name, cs_dir_name);
  }
  charsets_dir= save_csdir;
  return mysql->net.last_errno;
}

/**
  client authentication plugin that does native MySQL authentication
  using a 20-byte (4.1+) scramble
*/
static int native_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  int pkt_len;
  uchar *pkt;

  DBUG_ENTER("native_password_auth_client");


  if (((MCPVIO_EXT *)vio)->mysql_change_user)
  {
    /*
      in mysql_change_user() the client sends the first packet.
      we use the old scramble.
    */
    pkt= (uchar*)mysql->scramble;
    pkt_len= SCRAMBLE_LENGTH + 1;
  }
  else
  {
    /* read the scramble */
    if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
      DBUG_RETURN(CR_ERROR);

    if (pkt_len != SCRAMBLE_LENGTH + 1)
      DBUG_RETURN(CR_SERVER_HANDSHAKE_ERR);

    /* save it in MYSQL */
    memcpy(mysql->scramble, pkt, SCRAMBLE_LENGTH);
    mysql->scramble[SCRAMBLE_LENGTH] = 0;
  }

  if (mysql->passwd[0])
  {
    char scrambled[SCRAMBLE_LENGTH + 1];
    DBUG_PRINT("info", ("sending scramble"));
    scramble(scrambled, (char*)pkt, mysql->passwd);
    if (vio->write_packet(vio, (uchar*)scrambled, SCRAMBLE_LENGTH))
      DBUG_RETURN(CR_ERROR);
  }
  else
  {
    DBUG_PRINT("info", ("no password"));
    if (vio->write_packet(vio, 0, 0)) /* no password */
      DBUG_RETURN(CR_ERROR);
  }

  DBUG_RETURN(CR_OK);
}

/**
  client authentication plugin that does old MySQL authentication
  using an 8-byte (4.0-) scramble
*/
static int old_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  uchar *pkt;
  int pkt_len;

  DBUG_ENTER("old_password_auth_client");

  if (((MCPVIO_EXT *)vio)->mysql_change_user)
  {
    /*
      in mysql_change_user() the client sends the first packet.
      we use the old scramble.
    */
    pkt= (uchar*)mysql->scramble;
    pkt_len= SCRAMBLE_LENGTH_323 + 1;
  }
  else
  {
    /* read the scramble */
    if ((pkt_len= vio->read_packet(vio, &pkt)) < 0)
      DBUG_RETURN(CR_ERROR);

    if (pkt_len != SCRAMBLE_LENGTH_323 + 1 &&
        pkt_len != SCRAMBLE_LENGTH + 1)
        DBUG_RETURN(CR_SERVER_HANDSHAKE_ERR);

    /* save it in MYSQL */
    memmove(mysql->scramble, pkt, pkt_len - 1);
    mysql->scramble[pkt_len - 1] = 0;
  }

  if (mysql->passwd[0])
  {
    char scrambled[SCRAMBLE_LENGTH_323 + 1];
    scramble_323(scrambled, (char*)pkt, mysql->passwd);
    if (vio->write_packet(vio, (uchar*)scrambled, SCRAMBLE_LENGTH_323 + 1))
      DBUG_RETURN(CR_ERROR);
  }
  else
    if (vio->write_packet(vio, 0, 0)) /* no password */
      DBUG_RETURN(CR_ERROR);

  DBUG_RETURN(CR_OK);
}


my_socket STDCALL
mysql_get_socket(const MYSQL *mysql)
{
  if (mysql->net.vio)
    return mysql->net.vio->sd;
  return INVALID_SOCKET;
}
