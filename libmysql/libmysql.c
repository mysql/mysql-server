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

static MYSQL* spawn_init(MYSQL* parent, const char* host,
			 unsigned int port,
			 const char* user,
			 const char* passwd);


/****************************************************************************
  A modified version of connect().  my_connect() allows you to specify
  a timeout value, in seconds, that we should wait until we
  derermine we can't connect to a particular host.  If timeout is 0,
  my_connect() will behave exactly like connect().

  Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

my_bool my_connect(my_socket s, const struct sockaddr *name,
		   uint namelen, uint timeout)
{
#if defined(__WIN__) || defined(OS2) || defined(__NETWARE__)
  return connect(s, (struct sockaddr*) name, namelen) != 0;
#else
  int flags, res, s_err;
  SOCKOPT_OPTLEN_TYPE s_err_size = sizeof(uint);
  fd_set sfds;
  struct timeval tv;
  time_t start_time, now_time;

  /*
    If they passed us a timeout of zero, we should behave
    exactly like the normal connect() call does.
  */

  if (timeout == 0)
    return connect(s, (struct sockaddr*) name, namelen) != 0;

  flags = fcntl(s, F_GETFL, 0);		  /* Set socket to not block */
#ifdef O_NONBLOCK
  fcntl(s, F_SETFL, flags | O_NONBLOCK);  /* and save the flags..  */
#endif

  res = connect(s, (struct sockaddr*) name, namelen);
  s_err = errno;			/* Save the error... */
  fcntl(s, F_SETFL, flags);
  if ((res != 0) && (s_err != EINPROGRESS))
  {
    errno = s_err;			/* Restore it */
    return(1);
  }
  if (res == 0)				/* Connected quickly! */
    return(0);

  /*
    Otherwise, our connection is "in progress."  We can use
    the select() call to wait up to a specified period of time
    for the connection to succeed.  If select() returns 0
    (after waiting howevermany seconds), our socket never became
    writable (host is probably unreachable.)  Otherwise, if
    select() returns 1, then one of two conditions exist:

    1. An error occured.  We use getsockopt() to check for this.
    2. The connection was set up sucessfully: getsockopt() will
    return 0 as an error.

    Thanks goes to Andrew Gierth <andrew@erlenstar.demon.co.uk>
    who posted this method of timing out a connect() in
    comp.unix.programmer on August 15th, 1997.
  */

  FD_ZERO(&sfds);
  FD_SET(s, &sfds);
  /*
    select could be interrupted by a signal, and if it is,
    the timeout should be adjusted and the select restarted
    to work around OSes that don't restart select and
    implementations of select that don't adjust tv upon
    failure to reflect the time remaining
  */
  start_time = time(NULL);
  for (;;)
  {
    tv.tv_sec = (long) timeout;
    tv.tv_usec = 0;
#if defined(HPUX10) && defined(THREAD)
    if ((res = select(s+1, NULL, (int*) &sfds, NULL, &tv)) > 0)
      break;
#else
    if ((res = select(s+1, NULL, &sfds, NULL, &tv)) > 0)
      break;
#endif
    if (res == 0)					/* timeout */
      return -1;
    now_time=time(NULL);
    timeout-= (uint) (now_time - start_time);
    if (errno != EINTR || (int) timeout <= 0)
      return 1;
  }

  /*
    select() returned something more interesting than zero, let's
    see if we have any errors.  If the next two statements pass,
    we've got an open socket!
  */

  s_err=0;
  if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return(1);

  if (s_err)
  {						/* getsockopt could succeed */
    errno = s_err;
    return(1);					/* but return an error... */
  }
  return (0);					/* ok */

#endif
}


/*
  Create a named pipe connection
*/

#ifdef __WIN__

HANDLE create_named_pipe(NET *net, uint connect_timeout, char **arg_host,
			 char **arg_unix_socket)
{
  HANDLE hPipe=INVALID_HANDLE_VALUE;
  char szPipeName [ 257 ];
  DWORD dwMode;
  int i;
  my_bool testing_named_pipes=0;
  char *host= *arg_host, *unix_socket= *arg_unix_socket;

  if ( ! unix_socket || (unix_socket)[0] == 0x00)
    unix_socket = mysql_unix_port;
  if (!host || !strcmp(host,LOCAL_HOST))
    host=LOCAL_HOST_NAMEDPIPE;

  sprintf( szPipeName, "\\\\%s\\pipe\\%s", host, unix_socket);
  DBUG_PRINT("info",("Server name: '%s'.  Named Pipe: %s",
		     host, unix_socket));

  for (i=0 ; i < 100 ; i++)			/* Don't retry forever */
  {
    if ((hPipe = CreateFile(szPipeName,
			    GENERIC_READ | GENERIC_WRITE,
			    0,
			    NULL,
			    OPEN_EXISTING,
			    0,
			    NULL )) != INVALID_HANDLE_VALUE)
      break;
    if (GetLastError() != ERROR_PIPE_BUSY)
    {
      net->last_errno=CR_NAMEDPIPEOPEN_ERROR;
      sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	      (ulong) GetLastError());
      return INVALID_HANDLE_VALUE;
    }
    /* wait for for an other instance */
    if (! WaitNamedPipe(szPipeName, connect_timeout*1000) )
    {
      net->last_errno=CR_NAMEDPIPEWAIT_ERROR;
      sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	      (ulong) GetLastError());
      return INVALID_HANDLE_VALUE;
    }
  }
  if (hPipe == INVALID_HANDLE_VALUE)
  {
    net->last_errno=CR_NAMEDPIPEOPEN_ERROR;
    sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	    (ulong) GetLastError());
    return INVALID_HANDLE_VALUE;
  }
  dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
  if ( !SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL) )
  {
    CloseHandle( hPipe );
    net->last_errno=CR_NAMEDPIPESETSTATE_ERROR;
    sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	    (ulong) GetLastError());
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
    if (error_allow == CR_SHARED_MEMORY_EVENT_ERROR)
      sprintf(net->last_error,ER(net->last_errno),suffix_pos,error_code);
    else
      sprintf(net->last_error,ER(net->last_errno),error_code);
    return(INVALID_HANDLE_VALUE);
  }
  return(handle_map);
}
#endif


/*****************************************************************************
  Read a packet from server. Give error message if socket was down
  or packet is an error message
*****************************************************************************/

ulong
net_safe_read(MYSQL *mysql)
{
  NET *net= &mysql->net;
  ulong len=0;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);
  if (net->vio != 0)
    len=my_net_read(net);
  reset_sigpipe(mysql);

  if (len == packet_error || len == 0)
  {
    DBUG_PRINT("error",("Wrong connection or packet. fd: %s  len: %d",
			vio_description(net->vio),len));
    end_server(mysql);
    net->last_errno=(net->last_errno == ER_NET_PACKET_TOO_LARGE ?
		     CR_NET_PACKET_TOO_LARGE:
		     CR_SERVER_LOST);
    strmov(net->last_error,ER(net->last_errno));
    return (packet_error);
  }
  if (net->read_pos[0] == 255)
  {
    if (len > 3)
    {
      char *pos=(char*) net->read_pos+1;
      net->last_errno=uint2korr(pos);
      pos+=2;
      len-=2;
      (void) strmake(net->last_error,(char*) pos,
		     min((uint) len,(uint) sizeof(net->last_error)-1));
    }
    else
    {
      net->last_errno=CR_UNKNOWN_ERROR;
      (void) strmov(net->last_error,ER(net->last_errno));
    }
    DBUG_PRINT("error",("Got error: %d (%s)", net->last_errno,
			net->last_error));
    return(packet_error);
  }
  return len;
}

static void free_rows(MYSQL_DATA *cur)
{
  if (cur)
  {
    free_root(&cur->alloc,MYF(0));
    my_free((gptr) cur,MYF(0));
  }
}


static my_bool
advanced_command(MYSQL *mysql, enum enum_server_command command,
		 const char *header, ulong header_length,
		 const char *arg, ulong arg_length, my_bool skip_check)
{
  NET *net= &mysql->net;
  my_bool result= 1;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);

  if (mysql->net.vio == 0)
  {						/* Do reconnect if possible */
    if (mysql_reconnect(mysql))
      return 1;
  }
  if (mysql->status != MYSQL_STATUS_READY)
  {
    strmov(net->last_error,ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    return 1;
  }

  mysql->net.last_error[0]=0;
  mysql->net.last_errno=0;
  mysql->info=0;
  mysql->affected_rows= ~(my_ulonglong) 0;
  net_clear(&mysql->net);			/* Clear receive buffer */

  if (net_write_command(net,(uchar) command, header, header_length,
			arg, arg_length))
  {
    DBUG_PRINT("error",("Can't send command to server. Error: %d",
			socket_errno));
    if (net->last_errno == ER_NET_PACKET_TOO_LARGE)
    {
      net->last_errno=CR_NET_PACKET_TOO_LARGE;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
    end_server(mysql);
    if (mysql_reconnect(mysql))
      goto end;
    if (net_write_command(net,(uchar) command, header, header_length,
			  arg, arg_length))
    {
      net->last_errno=CR_SERVER_GONE_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
  }
  result=0;
  if (!skip_check)
    result= ((mysql->packet_length=net_safe_read(mysql)) == packet_error ?
	     1 : 0);
 end:
  reset_sigpipe(mysql);
  return result;
}


my_bool
simple_command(MYSQL *mysql,enum enum_server_command command, const char *arg,
	       ulong length, my_bool skip_check)
{
  return advanced_command(mysql, command, NullS, 0, arg, length, skip_check);
}


static void free_old_query(MYSQL *mysql)
{
  DBUG_ENTER("free_old_query");
  if (mysql->fields)
    free_root(&mysql->field_alloc,MYF(0));
  init_alloc_root(&mysql->field_alloc,8192,0);	/* Assume rowlength < 8192 */
  mysql->fields=0;
  mysql->field_count=0;				/* For API */
  DBUG_VOID_RETURN;
}


#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif


#if defined(__NETWARE__)
/* default to "root" on NetWare */
static void read_user_name(char *name)
{
  char *str=getenv("USER");
  strmake(name, str ? str : "UNKNOWN_USER", USERNAME_LENGTH);
}

#elif !defined(MSDOS) && ! defined(VMS) && !defined(__WIN__) && !defined(OS2)

static void read_user_name(char *name)
{
  DBUG_ENTER("read_user_name");
  if (geteuid() == 0)
    (void) strmov(name,"root");		/* allow use of surun */
  else
  {
#ifdef HAVE_GETPWUID
    struct passwd *skr;
    const char *str;
    if ((str=getlogin()) == NULL)
    {
      if ((skr=getpwuid(geteuid())) != NULL)
	str=skr->pw_name;
      else if (!(str=getenv("USER")) && !(str=getenv("LOGNAME")) &&
	       !(str=getenv("LOGIN")))
	str="UNKNOWN_USER";
    }
    (void) strmake(name,str,USERNAME_LENGTH);
#elif HAVE_CUSERID
    (void) cuserid(name);
#else
    strmov(name,"UNKNOWN_USER");
#endif
  }
  DBUG_VOID_RETURN;
}

#else /* If MSDOS || VMS */

static void read_user_name(char *name)
{
  char *str=getenv("USER");		/* ODBC will send user variable */
  strmake(name,str ? str : "ODBC", USERNAME_LENGTH);
}

#endif

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
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


/**************************************************************************
  Shut down connection
**************************************************************************/

static void
end_server(MYSQL *mysql)
{
  DBUG_ENTER("end_server");
  if (mysql->net.vio != 0)
  {
    init_sigpipe_variables
    DBUG_PRINT("info",("Net: %s", vio_description(mysql->net.vio)));
    set_sigpipe(mysql);
    vio_delete(mysql->net.vio);
    reset_sigpipe(mysql);
    mysql->net.vio= 0;          /* Marker */
  }
  net_end(&mysql->net);
  free_old_query(mysql);
  DBUG_VOID_RETURN;
}


void STDCALL
mysql_free_result(MYSQL_RES *result)
{
  DBUG_ENTER("mysql_free_result");
  DBUG_PRINT("enter",("mysql_res: %lx",result));
  if (result)
  {
    if (result->handle && result->handle->status == MYSQL_STATUS_USE_RESULT)
    {
      DBUG_PRINT("warning",("Not all rows in set where read; Ignoring rows"));
      for (;;)
      {
	ulong pkt_len;
	if ((pkt_len=net_safe_read(result->handle)) == packet_error)
	  break;
	if (pkt_len <= 8 && result->handle->net.read_pos[0] == 254)
	  break;				/* End of data */
      }
      result->handle->status=MYSQL_STATUS_READY;
    }
    free_rows(result->data);
    if (result->fields)
      free_root(&result->field_alloc,MYF(0));
    if (result->row)
      my_free((gptr) result->row,MYF(0));
    my_free((gptr) result,MYF(0));
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
  "replication-probe", "enable-reads-from-master", "repl-parse-query",
  "ssl-cipher", "max-allowed-packet",
  "protocol", "shared-memory-base-name",
  NullS
};

static TYPELIB option_types={array_elements(default_options)-1,
			     "options",default_options};

static int add_init_command(struct st_mysql_options *options, const char *cmd)
{
  char *tmp;

  if (!options->init_commands)
  {
    options->init_commands= (DYNAMIC_ARRAY*)my_malloc(sizeof(DYNAMIC_ARRAY),
						      MYF(MY_WME));
    init_dynamic_array(options->init_commands,sizeof(char*),0,5 CALLER_INFO);
  }

  if (!(tmp= my_strdup(cmd,MYF(MY_WME))) ||
      insert_dynamic(options->init_commands, (gptr)&tmp))
  {
    my_free(tmp, MYF(MY_ALLOW_ZERO_PTR));
    return 1;
  }

  return 0;
}

static void mysql_read_default_options(struct st_mysql_options *options,
				       const char *filename,const char *group)
{
  int argc;
  char *argv_buff[1],**argv;
  const char *groups[3];
  DBUG_ENTER("mysql_read_default_options");
  DBUG_PRINT("enter",("file: %s  group: %s",filename,group ? group :"NULL"));

  argc=1; argv=argv_buff; argv_buff[0]= (char*) "client";
  groups[0]= (char*) "client"; groups[1]= (char*) group; groups[2]=0;

  load_defaults(filename, groups, &argc, &argv);
  if (argc != 1)				/* If some default option */
  {
    char **option=argv;
    while (*++option)
    {
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
	switch (find_type(*option+2,&option_types,2)) {
	case 1:				/* port */
	  if (opt_arg)
	    options->port=atoi(opt_arg);
	  break;
	case 2:				/* socket */
	  if (opt_arg)
	  {
	    my_free(options->unix_socket,MYF(MY_ALLOW_ZERO_PTR));
	    options->unix_socket=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 3:				/* compress */
	  options->compress=1;
	  options->client_flag|= CLIENT_COMPRESS;
	  break;
	case 4:				/* password */
	  if (opt_arg)
	  {
	    my_free(options->password,MYF(MY_ALLOW_ZERO_PTR));
	    options->password=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
        case 5:
          options->protocol = MYSQL_PROTOCOL_PIPE;
	case 20:			/* connect_timeout */
	case 6:				/* timeout */
	  if (opt_arg)
	    options->connect_timeout=atoi(opt_arg);
	  break;
	case 7:				/* user */
	  if (opt_arg)
	  {
	    my_free(options->user,MYF(MY_ALLOW_ZERO_PTR));
	    options->user=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 8:				/* init-command */
	  add_init_command(options,opt_arg);
	  break;
	case 9:				/* host */
	  if (opt_arg)
	  {
	    my_free(options->host,MYF(MY_ALLOW_ZERO_PTR));
	    options->host=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 10:			/* database */
	  if (opt_arg)
	  {
	    my_free(options->db,MYF(MY_ALLOW_ZERO_PTR));
	    options->db=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 11:			/* debug */
	  mysql_debug(opt_arg ? opt_arg : "d:t:o,/tmp/client.trace");
	  break;
	case 12:			/* return-found-rows */
	  options->client_flag|=CLIENT_FOUND_ROWS;
	  break;
#ifdef HAVE_OPENSSL
	case 13:			/* ssl_key */
	  my_free(options->ssl_key, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_key = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case 14:			/* ssl_cert */
	  my_free(options->ssl_cert, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_cert = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case 15:			/* ssl_ca */
	  my_free(options->ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_ca = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case 16:			/* ssl_capath */
	  my_free(options->ssl_capath, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_capath = my_strdup(opt_arg, MYF(MY_WME));
          break;
#else
	case 13:				/* Ignore SSL options */
	case 14:
	case 15:
	case 16:
	  break;
#endif /* HAVE_OPENSSL */
	case 17:			/* charset-lib */
	  my_free(options->charset_dir,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_dir = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case 18:
	  my_free(options->charset_name,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_name = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case 19:				/* Interactive-timeout */
	  options->client_flag|= CLIENT_INTERACTIVE;
	  break;
	case 21:
	  if (!opt_arg || atoi(opt_arg) != 0)
	    options->client_flag|= CLIENT_LOCAL_FILES;
	  else
	    options->client_flag&= ~CLIENT_LOCAL_FILES;
	  break;
	case 22:
	  options->client_flag&= CLIENT_LOCAL_FILES;
          break;
	case 23:  /* replication probe */
	  options->rpl_probe= 1;
	  break;
	case 24: /* enable-reads-from-master */
	  options->no_master_reads= 0;
	  break;
	case 25: /* repl-parse-query */
	  options->rpl_parse= 1;
	  break;
	case 27:
	  options->max_allowed_packet= atoi(opt_arg);
	  break;
        case 28:		/* protocol */
          if ((options->protocol = find_type(opt_arg, &sql_protocol_typelib,0)) == ~(ulong) 0)
          {
            fprintf(stderr, "Unknown option to protocol: %s\n", opt_arg);
            exit(1);
          }
          break;
        case 29:		/* shared_memory_base_name */
#ifdef HAVE_SMEM
          if (options->shared_memory_base_name != def_shared_memory_base_name)
            my_free(options->shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
          options->shared_memory_base_name=my_strdup(opt_arg,MYF(MY_WME));
#endif
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


/***************************************************************************
  Change field rows to field structs
***************************************************************************/

static MYSQL_FIELD *
unpack_fields(MYSQL_DATA *data,MEM_ROOT *alloc,uint fields,
	      my_bool default_value, uint server_capabilities)
{
  MYSQL_ROWS	*row;
  MYSQL_FIELD	*field,*result;
  ulong lengths[8];				/* Max of fields */
  DBUG_ENTER("unpack_fields");

  field=result=(MYSQL_FIELD*) alloc_root(alloc,
					 (uint) sizeof(MYSQL_FIELD)*fields);
  if (!result)
  {
    free_rows(data);				/* Free old data */
    DBUG_RETURN(0);
  }
  bzero((char*) field, (uint) sizeof(MYSQL_FIELD)*fields);
  if (server_capabilities & CLIENT_PROTOCOL_41)
  {
    /* server is 4.1, and returns the new field result format */
    for (row=data->data; row ; row = row->next,field++)
    {
      uchar *pos;
      fetch_lengths(&lengths[0], row->data, default_value ? 7 : 6);
      field->db       = strdup_root(alloc,(char*) row->data[0]);
      field->table    = strdup_root(alloc,(char*) row->data[1]);
      field->org_table= strdup_root(alloc,(char*) row->data[2]);
      field->name     = strdup_root(alloc,(char*) row->data[3]);
      field->org_name = strdup_root(alloc,(char*) row->data[4]);

      field->db_length=		lengths[0];
      field->table_length=	lengths[1];
      field->org_table_length=	lengths[2];
      field->name_length=	lengths[3];
      field->org_name_length=	lengths[4];

      /* Unpack fixed length parts */
      pos= (uchar*) row->data[5];
      field->charsetnr= uint2korr(pos);
      field->length=	(uint) uint3korr(pos+2);
      field->type=	(enum enum_field_types) pos[5];
      field->flags=	uint2korr(pos+6);
      field->decimals=  (uint) pos[8];

      if (INTERNAL_NUM_FIELD(field))
        field->flags|= NUM_FLAG;
      if (default_value && row->data[6])
      {
        field->def=strdup_root(alloc,(char*) row->data[6]);
	field->def_length= lengths[6];
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
      fetch_lengths(&lengths[0], row->data, default_value ? 6 : 5);
      field->org_table= field->table=  strdup_root(alloc,(char*) row->data[0]);
      field->name=   strdup_root(alloc,(char*) row->data[1]);
      field->length= (uint) uint3korr(row->data[2]);
      field->type=   (enum enum_field_types) (uchar) row->data[3][0];

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
      if (INTERNAL_NUM_FIELD(field))
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

static MYSQL_DATA *read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
			     uint fields)
{
  uint	field;
  ulong pkt_len;
  ulong len;
  uchar *cp;
  char	*to, *end_to;
  MYSQL_DATA *result;
  MYSQL_ROWS **prev_ptr,*cur;
  NET *net = &mysql->net;
  DBUG_ENTER("read_rows");

  if ((pkt_len= net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(0);
  if (!(result=(MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
				       MYF(MY_WME | MY_ZEROFILL))))
  {
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
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
      net->last_errno=CR_OUT_OF_MEMORY;
      strmov(net->last_error,ER(net->last_errno));
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
          net->last_errno=CR_MALFORMED_PACKET;
          strmov(net->last_error,ER(net->last_errno));
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
    if ((pkt_len=net_safe_read(mysql)) == packet_error)
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
    DBUG_PRINT("info",("warning_count:  %ld", mysql->warning_count));
  }
  DBUG_PRINT("exit",("Got %d rows",result->rows));
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
  uchar *pos, *end_pos;
  uchar *prev_pos;

  if ((pkt_len=net_safe_read(mysql)) == packet_error)
    return -1;
  if (pkt_len <= 8 && mysql->net.read_pos[0] == 254)
  {
    if (pkt_len > 1)				/* MySQL 4.1 protocol */
    {
      mysql->warning_count= uint2korr(mysql->net.read_pos+1);
      mysql->server_status= uint2korr(mysql->net.read_pos+3);
    }
    return 1;				/* End of data */
  }
  prev_pos= 0;				/* allowed to write at packet[-1] */
  pos=mysql->net.read_pos;
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
        mysql->net.last_errno=CR_UNKNOWN_ERROR;
        strmov(mysql->net.last_error,ER(mysql->net.last_errno));
        return -1;
      }
      row[field] = (char*) pos;
      pos+=len;
      *lengths++=len;
    }
    if (prev_pos)
      *prev_pos=0;				/* Terminate prev field */
    prev_pos= pos;
  }
  row[field]=(char*) prev_pos+1;		/* End of last field */
  *prev_pos=0;					/* Terminate last field */
  return 0;
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


/****************************************************************************
  Init MySQL structure or allocate one
****************************************************************************/

MYSQL * STDCALL
mysql_init(MYSQL *mysql)
{
  mysql_once_init();
  if (!mysql)
  {
    if (!(mysql=(MYSQL*) my_malloc(sizeof(*mysql),MYF(MY_WME | MY_ZEROFILL))))
      return 0;
    mysql->free_me=1;
  }
  else
    bzero((char*) (mysql),sizeof(*(mysql)));
  mysql->options.connect_timeout=CONNECT_TIMEOUT;
  mysql->last_used_con = mysql->next_slave = mysql->master = mysql;

  /*
    By default, we are a replication pivot. The caller must reset it
    after we return if this is not the case.
  */
  mysql->rpl_pivot = 1;
#if defined(SIGPIPE) && defined(THREAD) && !defined(__WIN__)
  if (!((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE))
    (void) signal(SIGPIPE,pipe_sig_handler);
#endif

/*
  Only enable LOAD DATA INFILE by default if configured with
  --enable-local-infile
*/
#ifdef ENABLED_LOCAL_INFILE
  mysql->options.client_flag|= CLIENT_LOCAL_FILES;
#endif
#ifdef HAVE_SMEM
  mysql->options.shared_memory_base_name=(char*)def_shared_memory_base_name;
#endif
  return mysql;
}


/*
  Initialize the MySQL library

  SYNOPSIS
    mysql_once_init()

  NOTES
    Can't be static on NetWare
    This function is called by mysql_init() and indirectly called
    by mysql_query(), so one should never have to call this from an
    outside program.
*/

void mysql_once_init(void)
{
  if (!mysql_client_init)
  {
    mysql_client_init=1;
    org_my_init_done=my_init_done;
    my_init();					/* Will init threads */
    init_client_errs();
    if (!mysql_port)
    {
      mysql_port = MYSQL_PORT;
#ifndef MSDOS
      {
	struct servent *serv_ptr;
	char	*env;
	if ((serv_ptr = getservbyname("mysql", "tcp")))
	  mysql_port = (uint) ntohs((ushort) serv_ptr->s_port);
	if ((env = getenv("MYSQL_TCP_PORT")))
	  mysql_port =(uint) atoi(env);
      }
#endif
    }
    if (!mysql_unix_port)
    {
      char *env;
#ifdef __WIN__
      mysql_unix_port = (char*) MYSQL_NAMEDPIPE;
#else
      mysql_unix_port = (char*) MYSQL_UNIX_ADDR;
#endif
      if ((env = getenv("MYSQL_UNIX_PORT")))
	mysql_unix_port = env;
    }
    mysql_debug(NullS);
#if defined(SIGPIPE) && !defined(THREAD) && !defined(__WIN__)
    (void) signal(SIGPIPE,SIG_IGN);
#endif
  }
#ifdef THREAD
  else
    my_thread_init();         /* Init if new thread */
#endif
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


/*
  Free strings in the SSL structure and clear 'use_ssl' flag.
  NB! Errors are not reported until you do mysql_real_connect.
*/

#ifdef HAVE_OPENSSL
static void
mysql_ssl_free(MYSQL *mysql __attribute__((unused)))
{
  my_free(mysql->options.ssl_key, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_cert, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_capath, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_cipher, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->connector_fd,MYF(MY_ALLOW_ZERO_PTR));
  mysql->options.ssl_key = 0;
  mysql->options.ssl_cert = 0;
  mysql->options.ssl_ca = 0;
  mysql->options.ssl_capath = 0;
  mysql->options.ssl_cipher= 0;
  mysql->options.use_ssl = FALSE;
  mysql->connector_fd = 0;
}
#endif /* HAVE_OPENSSL */


/*
  Handle password authentication
*/

static my_bool mysql_autenticate(MYSQL *mysql, const char *passwd)
{
  ulong pkt_length;
  NET *net= &mysql->net;
  char buff[SCRAMBLE41_LENGTH];
  char password_hash[SCRAMBLE41_LENGTH]; /* Used for storage of stage1 hash */

  /* We shall only query server if it expect us to do so */
  if ((pkt_length=net_safe_read(mysql)) == packet_error)
    goto error;

  if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    /*
      This should always happen with new server unless empty password
      OK/Error packets have zero as the first char
    */
    if (pkt_length == 24 && net->read_pos[0])
    {
      /* Old passwords will have '*' at the first byte of hash */
      if (net->read_pos[0] != '*')
      {
        /* Build full password hash as it is required to decode scramble */
        password_hash_stage1(buff, passwd);
        /* Store copy as we'll need it later */
        memcpy(password_hash,buff,SCRAMBLE41_LENGTH);
        /* Finally hash complete password using hash we got from server */
        password_hash_stage2(password_hash,(const char*) net->read_pos);
        /* Decypt and store scramble 4 = hash for stage2 */
        password_crypt((const char*) net->read_pos+4,mysql->scramble_buff,
		       password_hash, SCRAMBLE41_LENGTH);
        mysql->scramble_buff[SCRAMBLE41_LENGTH]=0;
        /* Encode scramble with password. Recycle buffer */
        password_crypt(mysql->scramble_buff,buff,buff,SCRAMBLE41_LENGTH);
      }
      else
      {
	/* Create password to decode scramble */
	create_key_from_old_password(passwd,password_hash);
	/* Decypt and store scramble 4 = hash for stage2 */
	password_crypt((const char*) net->read_pos+4,mysql->scramble_buff,
		       password_hash, SCRAMBLE41_LENGTH);
	mysql->scramble_buff[SCRAMBLE41_LENGTH]=0;
	/* Finally scramble decoded scramble with password */
	scramble(buff, mysql->scramble_buff, passwd,0);
      }
      /* Write second package of authentication */
      if (my_net_write(net,buff,SCRAMBLE41_LENGTH) || net_flush(net))
      {
        net->last_errno= CR_SERVER_LOST;
        strmov(net->last_error,ER(net->last_errno));
        goto error;
      }
      /* Read what server thinks about out new auth message report */
      if (net_safe_read(mysql) == packet_error)
	goto error;
    }
  }
  return 0;

error:
  return 1;
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


/*
  Note that the mysql argument must be initialized with mysql_init()
  before calling mysql_real_connect !
*/

MYSQL * STDCALL
mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		   uint port, const char *unix_socket,ulong client_flag)
{
  char		buff[NAME_LEN+USERNAME_LENGTH+100],charset_name_buff[16];
  char		*end,*host_info,*charset_name;
  my_socket	sock;
  uint32	ip_addr;
  struct	sockaddr_in sock_addr;
  ulong		pkt_length;
  NET		*net= &mysql->net;
#ifdef __WIN__
  HANDLE	hPipe=INVALID_HANDLE_VALUE;
#endif
#ifdef HAVE_SYS_UN_H
  struct	sockaddr_un UNIXaddr;
#endif
  init_sigpipe_variables
  DBUG_ENTER("mysql_real_connect");
  LINT_INIT(host_info);

  DBUG_PRINT("enter",("host: %s  db: %s  user: %s",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);
  net->vio = 0;				/* If something goes wrong */
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

  /* Some empty-string-tests are done because of ODBC */
  if (!host || !host[0])
    host=mysql->options.host;
  if (!user || !user[0])
    user=mysql->options.user;
  if (!passwd)
  {
    passwd=mysql->options.password;
#ifndef DONT_USE_MYSQL_PWD
    if (!passwd)
      passwd=getenv("MYSQL_PWD");		/* get it from environment */
#endif
  }
  if (!db || !db[0])
    db=mysql->options.db;
  if (!port)
    port=mysql->options.port;
  if (!unix_socket)
    unix_socket=mysql->options.unix_socket;

  mysql->reconnect=1;				/* Reconnect as default */
  mysql->server_status=SERVER_STATUS_AUTOCOMMIT;

  /*
    Grab a socket and connect it to the server
  */
#if defined(HAVE_SMEM)
  if ((!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_MEMORY) &&
      (!host || !strcmp(host,LOCAL_HOST)))
  {
    if ((create_shared_memory(mysql,net, mysql->options.connect_timeout)) ==
	INVALID_HANDLE_VALUE)
    {
      DBUG_PRINT("error",
		 ("host: '%s'  socket: '%s'  shared memory: %s  have_tcpip: %d",
		  host ? host : "<null>",
		  unix_socket ? unix_socket : "<null>",
		  (int) mysql->options.shared_memory_base_name,
		  (int) have_tcpip));
      if (mysql->options.protocol == MYSQL_PROTOCOL_MEMORY)
	goto error;
      /* Try also with PIPE or TCP/IP */
    }
    else
    {
      mysql->options.protocol=MYSQL_PROTOCOL_MEMORY;
      sock=0;
      unix_socket = 0;
      host=mysql->options.shared_memory_base_name;
      host_info=(char*) ER(CR_SHARED_MEMORY_CONNECTION);
    }
  } else
#endif /* HAVE_SMEM */
#if defined(HAVE_SYS_UN_H)
    if ((!mysql->options.protocol ||
	 mysql->options.protocol == MYSQL_PROTOCOL_SOCKET)&&
	(!host || !strcmp(host,LOCAL_HOST)) &&
	(unix_socket || mysql_unix_port))
    {
      host=LOCAL_HOST;
      if (!unix_socket)
	unix_socket=mysql_unix_port;
      host_info=(char*) ER(CR_LOCALHOST_CONNECTION);
      DBUG_PRINT("info",("Using UNIX sock '%s'",unix_socket));
      if ((sock = socket(AF_UNIX,SOCK_STREAM,0)) == SOCKET_ERROR)
      {
	net->last_errno=CR_SOCKET_CREATE_ERROR;
	sprintf(net->last_error,ER(net->last_errno),socket_errno);
	goto error;
      }
      net->vio = vio_new(sock, VIO_TYPE_SOCKET, TRUE);
      bzero((char*) &UNIXaddr,sizeof(UNIXaddr));
      UNIXaddr.sun_family = AF_UNIX;
      strmov(UNIXaddr.sun_path, unix_socket);
      if (my_connect(sock,(struct sockaddr *) &UNIXaddr, sizeof(UNIXaddr),
		     mysql->options.connect_timeout))
      {
	DBUG_PRINT("error",("Got error %d on connect to local server",
			    socket_errno));
	net->last_errno=CR_CONNECTION_ERROR;
	sprintf(net->last_error,ER(net->last_errno),unix_socket,socket_errno);
	goto error;
      }
      else
	mysql->options.protocol=MYSQL_PROTOCOL_SOCKET;
    }
    else
#elif defined(__WIN__)
    {
      if ((!mysql->options.protocol ||
	   mysql->options.protocol == MYSQL_PROTOCOL_PIPE)&&
	  ((unix_socket || !host && is_NT() ||
	    host && !strcmp(host,LOCAL_HOST_NAMEDPIPE) ||! have_tcpip))&&
	  (!net->vio))
      {
	sock=0;
	if ((hPipe=create_named_pipe(net, mysql->options.connect_timeout,
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
	  net->vio=vio_new_win32pipe(hPipe);
	  sprintf(host_info=buff, ER(CR_NAMEDPIPE_CONNECTION), host,
		  unix_socket);
	}
      }
    }
#endif
  if ((!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_TCP)&&(!net->vio))
  {
    unix_socket=0;				/* This is not used */
    if (!port)
      port=mysql_port;
    if (!host)
      host=LOCAL_HOST;
    sprintf(host_info=buff,ER(CR_TCP_CONNECTION),host);
    DBUG_PRINT("info",("Server name: '%s'.  TCP sock: %d", host,port));
    /* _WIN64 ;  Assume that the (int) range is enough for socket() */
    if ((sock = (my_socket) socket(AF_INET,SOCK_STREAM,0)) == SOCKET_ERROR)
    {
      net->last_errno=CR_IPSOCK_ERROR;
      sprintf(net->last_error,ER(net->last_errno),socket_errno);
      goto error;
    }
    net->vio = vio_new(sock,VIO_TYPE_TCPIP,FALSE);
    bzero((char*) &sock_addr,sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;

    /*
      The server name may be a host name or IP address
    */

    if ((int) (ip_addr = inet_addr(host)) != (int) INADDR_NONE)
    {
      memcpy_fixed(&sock_addr.sin_addr,&ip_addr,sizeof(ip_addr));
    }
    else
    {
      int tmp_errno;
      struct hostent tmp_hostent,*hp;
      char buff2[GETHOSTBYNAME_BUFF_SIZE];
      hp = my_gethostbyname_r(host,&tmp_hostent,buff2,sizeof(buff2),
			      &tmp_errno);
      if (!hp)
      {
	my_gethostbyname_r_free();
	net->last_errno=CR_UNKNOWN_HOST;
	sprintf(net->last_error, ER(CR_UNKNOWN_HOST), host, tmp_errno);
	goto error;
      }
      memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
      my_gethostbyname_r_free();
    }
    sock_addr.sin_port = (ushort) htons((ushort) port);
    if (my_connect(sock,(struct sockaddr *) &sock_addr, sizeof(sock_addr),
		   mysql->options.connect_timeout))
    {
      DBUG_PRINT("error",("Got error %d on connect to '%s'",socket_errno,
			  host));
      net->last_errno= CR_CONN_HOST_ERROR;
      sprintf(net->last_error ,ER(CR_CONN_HOST_ERROR), host, socket_errno);
      goto error;
    }
  }
  else if (!net->vio)
  {
    DBUG_PRINT("error",("Unknow protocol %d ",mysql->options.protocol));
    net->last_errno= CR_CONN_UNKNOW_PROTOCOL;
    sprintf(net->last_error ,ER(CR_CONN_UNKNOW_PROTOCOL));
    goto error;
  }

  if (!net->vio || my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0;
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  vio_keepalive(net->vio,TRUE);

  /* Get version info */
  mysql->protocol_version= PROTOCOL_VERSION;	/* Assume this */
  if (mysql->options.connect_timeout &&
      vio_poll_read(net->vio, mysql->options.connect_timeout))
  {
    net->last_errno= CR_SERVER_LOST;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  if ((pkt_length=net_safe_read(mysql)) == packet_error)
    goto error;

  /* Check if version of protocol matches current one */

  mysql->protocol_version= net->read_pos[0];
  DBUG_DUMP("packet",(char*) net->read_pos,10);
  DBUG_PRINT("info",("mysql protocol version %d, server=%d",
		     PROTOCOL_VERSION, mysql->protocol_version));
  if (mysql->protocol_version != PROTOCOL_VERSION)
  {
    net->last_errno= CR_VERSION_ERROR;
    sprintf(net->last_error, ER(CR_VERSION_ERROR), mysql->protocol_version,
	    PROTOCOL_VERSION);
    goto error;
  }
  end=strend((char*) net->read_pos+1);
  mysql->thread_id=uint4korr(end+1);
  end+=5;
  strmake(mysql->scramble_buff,end,8);
  end+=9;
  if (pkt_length >= (uint) (end+1 - (char*) net->read_pos))
    mysql->server_capabilities=uint2korr(end);
  if (pkt_length >= (uint) (end+18 - (char*) net->read_pos))
  {
    /* New protocol with 16 bytes to describe server characteristics */
    mysql->server_language=end[2];
    mysql->server_status=uint2korr(end+3);
  }

  /* Set character set */
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
    if (!(mysql->charset =
	  get_charset((uint8) mysql->server_language, MYF(0))))
      mysql->charset = default_charset_info; /* shouldn't be fatal */
  }
  else
    mysql->charset=default_charset_info;

  if (!mysql->charset)
  {
    net->last_errno=CR_CANT_READ_CHARSET;
    if (mysql->options.charset_dir)
      sprintf(net->last_error,ER(net->last_errno),
              charset_name ? charset_name : "unknown",
              mysql->options.charset_dir);
    else
    {
      char cs_dir_name[FN_REFLEN];
      get_charsets_dir(cs_dir_name);
      sprintf(net->last_error,ER(net->last_errno),
              charset_name ? charset_name : "unknown",
              cs_dir_name);
    }
    goto error;
  }

  /* Save connection information */
  if (!user) user="";
  if (!passwd) passwd="";
  if (!my_multi_malloc(MYF(0),
		       &mysql->host_info, (uint) strlen(host_info)+1,
		       &mysql->host,      (uint) strlen(host)+1,
		       &mysql->unix_socket,unix_socket ?
		       (uint) strlen(unix_socket)+1 : (uint) 1,
		       &mysql->server_version,
		       (uint) (end - (char*) net->read_pos),
		       NullS) ||
      !(mysql->user=my_strdup(user,MYF(0))) ||
      !(mysql->passwd=my_strdup(passwd,MYF(0))))
  {
    strmov(net->last_error, ER(net->last_errno=CR_OUT_OF_MEMORY));
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
  client_flag|=mysql->options.client_flag;

  /* Send client information for access check */
  client_flag|=CLIENT_CAPABILITIES;

#ifdef HAVE_OPENSSL
  if (mysql->options.ssl_key || mysql->options.ssl_cert ||
      mysql->options.ssl_ca || mysql->options.ssl_capath ||
      mysql->options.ssl_cipher)
    mysql->options.use_ssl= 1;
  if (mysql->options.use_ssl)
    client_flag|=CLIENT_SSL;
#endif /* HAVE_OPENSSL */
  if (db)
    client_flag|=CLIENT_CONNECT_WITH_DB;
  /* Remove options that server doesn't support */
  client_flag= ((client_flag &
		 ~(CLIENT_COMPRESS | CLIENT_SSL | CLIENT_PROTOCOL_41)) |
		(client_flag & mysql->server_capabilities));

#ifndef HAVE_COMPRESS
  client_flag&= ~CLIENT_COMPRESS;
#endif

  if (client_flag & CLIENT_PROTOCOL_41)
  {
    /* 4.1 server and 4.1 client has a 4 byte option flag */
    int4store(buff,client_flag);
    int4store(buff+4,max_allowed_packet);
    end= buff+8;
  }
  else
  {
    int2store(buff,client_flag);
    int3store(buff+2,max_allowed_packet);
    end= buff+5;
  }
  mysql->client_flag=client_flag;

#ifdef HAVE_OPENSSL
  /*
    Oops.. are we careful enough to not send ANY information without
    encryption?
  */
  if (client_flag & CLIENT_SSL)
  {
    struct st_mysql_options *options= &mysql->options;
    if (my_net_write(net,buff,(uint) (end-buff)) || net_flush(net))
    {
      net->last_errno= CR_SERVER_LOST;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    /* Do the SSL layering. */
    if (!(mysql->connector_fd=
	  (gptr) new_VioSSLConnectorFd(options->ssl_key,
				       options->ssl_cert,
				       options->ssl_ca,
				       options->ssl_capath,
				       options->ssl_cipher)))
    {
      net->last_errno= CR_SSL_CONNECTION_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslconnect((struct st_VioSSLConnectorFd*)(mysql->connector_fd),
		   mysql->net.vio, (long) (mysql->options.connect_timeout)))
    {
      net->last_errno= CR_SSL_CONNECTION_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    DBUG_PRINT("info", ("IO layer change done!"));
  }
#endif /* HAVE_OPENSSL */

  DBUG_PRINT("info",("Server version = '%s'  capabilites: %lu  status: %u  client_flag: %lu",
		     mysql->server_version,mysql->server_capabilities,
		     mysql->server_status, client_flag));
  /* This needs to be changed as it's not useful with big packets */
  if (user && user[0])
    strmake(end,user,32);			/* Max user name */
  else
    read_user_name((char*) end);
  /* We have to handle different version of handshake here */
#ifdef _CUSTOMCONFIG_
#include "_cust_libmysql.h";
#endif
  DBUG_PRINT("info",("user: %s",end));
  /*
    We always start with old type handshake the only difference is message sent
    If server handles secure connection type we'll not send the real scramble
  */
  if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    if (passwd[0])
    {
      /* Prepare false scramble  */
      end=strend(end)+1;
      bfill(end, SCRAMBLE_LENGTH, 'x');
      end+=SCRAMBLE_LENGTH;
      *end=0;
    }
    else				/* For empty password*/
    {
      end=strend(end)+1;
      *end=0;				/* Store zero length scramble */
    }
  }
  else
  {
    /*
      Real scramble is only sent to old servers. This can be blocked 
      by calling mysql_options(MYSQL *, MYSQL_SECURE_CONNECT, (char*) &1);
    */
    end=scramble(strend(end)+1, mysql->scramble_buff, passwd,
                 (my_bool) (mysql->protocol_version == 9));
  }
  /* Add database if needed */
  if (db && (mysql->server_capabilities & CLIENT_CONNECT_WITH_DB))
  {
    end=strmake(end+1,db,NAME_LEN);
    mysql->db=my_strdup(db,MYF(MY_WME));
    db=0;
  }
  /* Write authentication package */
  if (my_net_write(net,buff,(ulong) (end-buff)) || net_flush(net))
  {
    net->last_errno= CR_SERVER_LOST;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }

  if (mysql_autenticate(mysql, passwd))
    goto error;

  if (client_flag & CLIENT_COMPRESS)		/* We will use compression */
    net->compress=1;
  if (mysql->options.max_allowed_packet)
    net->max_packet_size= mysql->options.max_allowed_packet;
  if (db && mysql_select_db(mysql,db))
    goto error;

  if (mysql->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end= ptr + init_commands->elements;

    my_bool reconnect=mysql->reconnect;
    mysql->reconnect=0;

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

    mysql->reconnect=reconnect;
  }

  if (mysql->options.rpl_probe && mysql_rpl_probe(mysql))
    goto error;

  DBUG_PRINT("exit",("Mysql handler: %lx",mysql));
  reset_sigpipe(mysql);
  DBUG_RETURN(mysql);

error:
  reset_sigpipe(mysql);
  DBUG_PRINT("error",("message: %u (%s)",net->last_errno,net->last_error));
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


/* needed when we move MYSQL structure to a different address */

static void mysql_fix_pointers(MYSQL* mysql, MYSQL* old_mysql)
{
  MYSQL *tmp, *tmp_prev;
  if (mysql->master == old_mysql)
    mysql->master = mysql;
  if (mysql->last_used_con == old_mysql)
    mysql->last_used_con = mysql;
  if (mysql->last_used_slave == old_mysql)
    mysql->last_used_slave = mysql;
  for (tmp_prev = mysql, tmp = mysql->next_slave;
       tmp != old_mysql;tmp = tmp->next_slave)
  {
    tmp_prev = tmp;
  }
  tmp_prev->next_slave = mysql;
}


static my_bool mysql_reconnect(MYSQL *mysql)
{
  MYSQL tmp_mysql;
  DBUG_ENTER("mysql_reconnect");

  if (!mysql->reconnect ||
      (mysql->server_status & SERVER_STATUS_IN_TRANS) || !mysql->host_info)
  {
   /* Allow reconnect next time */
    mysql->server_status&= ~SERVER_STATUS_IN_TRANS;
    mysql->net.last_errno=CR_SERVER_GONE_ERROR;
    strmov(mysql->net.last_error,ER(mysql->net.last_errno));
    DBUG_RETURN(1);
  }
  mysql_init(&tmp_mysql);
  tmp_mysql.options=mysql->options;
  bzero((char*) &mysql->options,sizeof(mysql->options));
  tmp_mysql.rpl_pivot = mysql->rpl_pivot;
  if (!mysql_real_connect(&tmp_mysql,mysql->host,mysql->user,mysql->passwd,
			  mysql->db, mysql->port, mysql->unix_socket,
			  mysql->client_flag))
  {
    mysql->net.last_errno= tmp_mysql.net.last_errno;
    strmov(mysql->net.last_error, tmp_mysql.net.last_error);
    DBUG_RETURN(1);
  }
  tmp_mysql.free_me=mysql->free_me;
  mysql->free_me=0;
  mysql_close(mysql);
  *mysql=tmp_mysql;
  mysql_fix_pointers(mysql, &tmp_mysql); /* adjust connection pointers */
  net_clear(&mysql->net);
  mysql->affected_rows= ~(my_ulonglong) 0;
  DBUG_RETURN(0);
}


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
  Set current database
**************************************************************************/

int STDCALL
mysql_select_db(MYSQL *mysql, const char *db)
{
  int error;
  DBUG_ENTER("mysql_select_db");
  DBUG_PRINT("enter",("db: '%s'",db));

  if ((error=simple_command(mysql,COM_INIT_DB,db,(ulong) strlen(db),0)))
    DBUG_RETURN(error);
  my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));
  mysql->db=my_strdup(db,MYF(MY_WME));
  DBUG_RETURN(0);
}


/*************************************************************************
  Send a QUIT to the server and close the connection
  If handle is alloced by mysql connect free it.
*************************************************************************/

void STDCALL
mysql_close(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close");
  if (mysql)					/* Some simple safety */
  {
    if (mysql->net.vio != 0)
    {
      free_old_query(mysql);
      mysql->status=MYSQL_STATUS_READY; /* Force command */
      mysql->reconnect=0;
      simple_command(mysql,COM_QUIT,NullS,0,1);
      end_server(mysql);			/* Sets mysql->net.vio= 0 */
    }
    my_free((gptr) mysql->host_info,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->passwd,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));
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
#ifdef HAVE_OPENSSL
    mysql_ssl_free(mysql);
#endif /* HAVE_OPENSSL */
#ifdef HAVE_SMEM
    if (mysql->options.shared_memory_base_name != def_shared_memory_base_name)
      my_free(mysql->options.shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif /* HAVE_SMEM */
    /* Clear pointers for better safety */
    mysql->host_info=mysql->user=mysql->passwd=mysql->db=0;
    bzero((char*) &mysql->options,sizeof(mysql->options));

    /* free/close slave list */
    if (mysql->rpl_pivot)
    {
      MYSQL* tmp;
      for (tmp = mysql->next_slave; tmp != mysql; )
      {
	/* trick to avoid following freed pointer */
	MYSQL* tmp1 = tmp->next_slave;
	mysql_close(tmp);
	tmp = tmp1;
      }
      mysql->rpl_pivot=0;
    }
    if (mysql->stmts)
    {
      /* Free any open prepared statements */
      LIST *element, *next_element;
      for (element= mysql->stmts; element; element= next_element)
      {
        next_element= element->next;
        stmt_close((MYSQL_STMT *)element->data, 0);
      }
    }
    if (mysql != mysql->master)
      mysql_close(mysql->master);
    if (mysql->free_me)
      my_free((gptr) mysql,MYF(0));
  }
  DBUG_VOID_RETURN;
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


/*
  Send the query and return so we can do something else.
  Needs to be followed by mysql_read_query_result() when we want to
  finish processing it.
*/

int STDCALL
mysql_send_query(MYSQL* mysql, const char* query, ulong length)
{
  DBUG_ENTER("mysql_send_query");
  DBUG_PRINT("enter",("rpl_parse: %d  rpl_pivot: %d",
		      mysql->options.rpl_parse, mysql->rpl_pivot));

  if (mysql->options.rpl_parse && mysql->rpl_pivot)
  {
    switch (mysql_rpl_query_type(query, length)) {
    case MYSQL_RPL_MASTER:
      DBUG_RETURN(mysql_master_send_query(mysql, query, length));
    case MYSQL_RPL_SLAVE:
      DBUG_RETURN(mysql_slave_send_query(mysql, query, length));
    case MYSQL_RPL_ADMIN:
      break;					/* fall through */
    }
  }

  mysql->last_used_con = mysql;
  DBUG_RETURN(simple_command(mysql, COM_QUERY, query, length, 1));
}


my_bool STDCALL mysql_read_query_result(MYSQL *mysql)
{
  uchar *pos;
  ulong field_count;
  MYSQL_DATA *fields;
  ulong length;
  DBUG_ENTER("mysql_read_query_result");

  /*
    Read from the connection which we actually used, which
    could differ from the original connection if we have slaves
  */
  mysql = mysql->last_used_con;

  if ((length = net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(1);
  free_old_query(mysql);			/* Free old result */
get_info:
  pos=(uchar*) mysql->net.read_pos;
  if ((field_count= net_field_length(&pos)) == 0)
  {
    mysql->affected_rows= net_field_length_ll(&pos);
    mysql->insert_id=	  net_field_length_ll(&pos);
    if (protocol_41(mysql))
    {
      mysql->server_status=uint2korr(pos); pos+=2;
      mysql->warning_count=uint2korr(pos); pos+=2;
    }
    else if (mysql->server_capabilities & CLIENT_TRANSACTIONS)
    {
      mysql->server_status=uint2korr(pos); pos+=2;
      mysql->warning_count= 0;
    }
    DBUG_PRINT("info",("status: %ld  warning_count:  %ld",
		       mysql->server_status, mysql->warning_count));
    if (pos < mysql->net.read_pos+length && net_field_length(&pos))
      mysql->info=(char*) pos;
    DBUG_RETURN(0);
  }
  if (field_count == NULL_LENGTH)		/* LOAD DATA LOCAL INFILE */
  {
    int error=send_file_to_server(mysql,(char*) pos);
    if ((length=net_safe_read(mysql)) == packet_error || error)
      DBUG_RETURN(1);
    goto get_info;				/* Get info packet */
  }
  if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
    mysql->server_status|= SERVER_STATUS_IN_TRANS;

  mysql->extra_info= net_field_length_ll(&pos); /* Maybe number of rec */

  if (!(fields=read_rows(mysql,(MYSQL_FIELD*)0, protocol_41(mysql) ? 6 : 5)))
    DBUG_RETURN(1);
  if (!(mysql->fields=unpack_fields(fields,&mysql->field_alloc,
				    (uint) field_count,0,
				    mysql->server_capabilities)))
    DBUG_RETURN(1);
  mysql->status= MYSQL_STATUS_GET_RESULT;
  mysql->field_count= (uint) field_count;
  mysql->warning_count= 0;
  DBUG_RETURN(0);
}


int STDCALL
mysql_real_query(MYSQL *mysql, const char *query, ulong length)
{
  DBUG_ENTER("mysql_real_query");
  DBUG_PRINT("enter",("handle: %lx",mysql));
  DBUG_PRINT("query",("Query = '%-.4096s'",query));

  if (mysql_send_query(mysql,query,length))
    DBUG_RETURN(1);
  DBUG_RETURN((int) mysql_read_query_result(mysql));
}


static my_bool
send_file_to_server(MYSQL *mysql, const char *filename)
{
  int fd, readcount;
  my_bool result= 1;
  uint packet_length=MY_ALIGN(mysql->net.max_packet-16,IO_SIZE);
  char *buf, tmp_name[FN_REFLEN];
  DBUG_ENTER("send_file_to_server");

  if (!(buf=my_malloc(packet_length,MYF(0))))
  {
    strmov(mysql->net.last_error, ER(mysql->net.last_errno=CR_OUT_OF_MEMORY));
    DBUG_RETURN(1);
  }

  fn_format(tmp_name,filename,"","",4);		/* Convert to client format */
  if ((fd = my_open(tmp_name,O_RDONLY, MYF(0))) < 0)
  {
    my_net_write(&mysql->net,"",0);		/* Server needs one packet */
    net_flush(&mysql->net);
    mysql->net.last_errno=EE_FILENOTFOUND;
    my_snprintf(mysql->net.last_error,sizeof(mysql->net.last_error)-1,
		EE(mysql->net.last_errno),tmp_name, errno);
    goto err;
  }

  while ((readcount = (int) my_read(fd,(byte*) buf,packet_length,MYF(0))) > 0)
  {
    if (my_net_write(&mysql->net,buf,readcount))
    {
      DBUG_PRINT("error",("Lost connection to MySQL server during LOAD DATA of local file"));
      mysql->net.last_errno=CR_SERVER_LOST;
      strmov(mysql->net.last_error,ER(mysql->net.last_errno));
      goto err;
    }
  }
  /* Send empty packet to mark end of file */
  if (my_net_write(&mysql->net,"",0) || net_flush(&mysql->net))
  {
    mysql->net.last_errno=CR_SERVER_LOST;
    sprintf(mysql->net.last_error,ER(mysql->net.last_errno),errno);
    goto err;
  }
  if (readcount < 0)
  {
    mysql->net.last_errno=EE_READ; /* the errmsg for not entire file read */
    my_snprintf(mysql->net.last_error,sizeof(mysql->net.last_error)-1,
		tmp_name,errno);
    goto err;
  }
  result=0;					/* Ok */

err:
  if (fd >= 0)
    (void) my_close(fd,MYF(0));
  my_free(buf,MYF(0));
  DBUG_RETURN(result);
}


/**************************************************************************
  Alloc result struct for buffered results. All rows are read to buffer.
  mysql_data_seek may be used.
**************************************************************************/

MYSQL_RES * STDCALL
mysql_store_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_store_result");

  /* read from the actually used connection */
  mysql = mysql->last_used_con;

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    strmov(mysql->net.last_error,
	   ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    DBUG_RETURN(0);
  }
  mysql->status=MYSQL_STATUS_READY;		/* server is ready */
  if (!(result=(MYSQL_RES*) my_malloc((uint) (sizeof(MYSQL_RES)+
					      sizeof(ulong) *
					      mysql->field_count),
				      MYF(MY_WME | MY_ZEROFILL))))
  {
    mysql->net.last_errno=CR_OUT_OF_MEMORY;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    DBUG_RETURN(0);
  }
  result->eof=1;				/* Marker for buffered */
  result->lengths=(ulong*) (result+1);
  if (!(result->data=read_rows(mysql,mysql->fields,mysql->field_count)))
  {
    my_free((gptr) result,MYF(0));
    DBUG_RETURN(0);
  }
  mysql->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=	result->data->data;
  result->fields=	mysql->fields;
  result->field_alloc=	mysql->field_alloc;
  result->field_count=	mysql->field_count;
  result->current_field=0;
  result->current_row=0;			/* Must do a fetch first */
  mysql->fields=0;				/* fields is now in result */
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
      if (!(read_one_row(res->handle,res->field_count,res->row, res->lengths)))
      {
	res->row_count++;
	DBUG_RETURN(res->current_row=res->row);
      }
      else
      {
	DBUG_PRINT("info",("end of data"));
	res->eof=1;
	res->handle->status=MYSQL_STATUS_READY;
	/* Don't clear handle in mysql_free_results */
	res->handle=0;
      }
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

static void fetch_lengths(ulong *to, MYSQL_ROW column, uint field_count)
{ 
  ulong *prev_length;
  byte *start=0;
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

/**************************************************************************
  Move to a specific row and column
**************************************************************************/

void STDCALL
mysql_data_seek(MYSQL_RES *result, my_ulonglong row)
{
  MYSQL_ROWS	*tmp=0;
  DBUG_PRINT("info",("mysql_data_seek(%ld)",(long) row));
  if (result->data)
    for (tmp=result->data->data; row-- && tmp ; tmp = tmp->next) ;
  result->current_row=0;
  result->data_cursor = tmp;
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

static void set_stmt_error(MYSQL_STMT * stmt, int errcode)
{
  DBUG_ENTER("set_stmt_error");
  DBUG_PRINT("enter", ("error: %d '%s'", errcode, ER(errcode)));
  DBUG_ASSERT(stmt != 0);

  stmt->last_errno= errcode;
  strmov(stmt->last_error, ER(errcode));

  DBUG_VOID_RETURN;
}


/*
  Copy error message to statement handler
*/

static void set_stmt_errmsg(MYSQL_STMT * stmt, char *err, int errcode)
{
  DBUG_ENTER("set_stmt_error_msg");
  DBUG_PRINT("enter", ("error: %d '%s'", errcode, err));
  DBUG_ASSERT(stmt != 0);

  stmt->last_errno= errcode;
  if (err && err[0])
    strmov(stmt->last_error, err);

  DBUG_VOID_RETURN;
}


/*
  Set the internal error message to mysql handler
*/

static void set_mysql_error(MYSQL * mysql, int errcode)
{
  DBUG_ENTER("set_mysql_error");
  DBUG_PRINT("enter", ("error :%d '%s'", errcode, ER(errcode)));
  DBUG_ASSERT(mysql != 0);

  mysql->net.last_errno= errcode;
  strmov(mysql->net.last_error, ER(errcode));
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

static my_bool read_prepare_result(MYSQL_STMT *stmt)
{
  uchar *pos;
  uint field_count;
  ulong length, param_count;
  MYSQL_DATA *fields_data;
  MYSQL *mysql= stmt->mysql;
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
    if (!(fields_data= read_rows(mysql, (MYSQL_FIELD*) 0, 8)))
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
    set_stmt_error(stmt, CR_OUT_OF_MEMORY);
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
    set_mysql_error(mysql, CR_NULL_POINTER);
    DBUG_RETURN(0);
  }
#endif

  if (!(stmt= (MYSQL_STMT *) my_malloc(sizeof(MYSQL_STMT),
				       MYF(MY_WME | MY_ZEROFILL))) ||
      !(stmt->query= my_strdup_with_length((byte *) query, length, MYF(0))))
  {
    my_free((gptr) stmt, MYF(MY_ALLOW_ZERO_PTR));
    set_mysql_error(mysql, CR_OUT_OF_MEMORY);
    DBUG_RETURN(0);
  }
  if (simple_command(mysql, COM_PREPARE, query, length, 1))
  {
    stmt_close(stmt, 1);
    DBUG_RETURN(0);
  }

  init_alloc_root(&stmt->mem_root,8192,0);
  stmt->mysql= mysql;
  if (read_prepare_result(stmt))
  {
    stmt_close(stmt, 1);
    DBUG_RETURN(0);
  }
  stmt->state= MY_ST_PREPARE;
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
  
  if (!stmt->mysql->field_count)
    return 0;
  
  stmt->field_count= stmt->mysql->field_count;
  
  /*
    Get the field information for non-select statements 
    like SHOW and DESCRIBE commands
  */
  if (!(stmt->fields= (MYSQL_FIELD *) alloc_root(alloc, 
        sizeof(MYSQL_FIELD) * stmt->field_count)) || 
      !(stmt->bind= (MYSQL_BIND *) alloc_root(alloc, 
        sizeof(MYSQL_BIND ) * stmt->field_count)))
    return 0;
  
  for (fields= stmt->mysql->fields, end= fields+stmt->field_count, 
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
      set_stmt_error(stmt, CR_OUT_OF_MEMORY);
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
    set_stmt_errmsg(stmt, net->last_error, net->last_errno);
    DBUG_RETURN(1);
  }
  stmt->state= MY_ST_EXECUTE;
  mysql_free_result(stmt->result);
  stmt->result= (MYSQL_RES *)0;
  stmt->result_buffered= 0;
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
    set_stmt_error(stmt, CR_NO_PREPARE_STMT);
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
      set_stmt_error(stmt, CR_NOT_ALL_PARAMS_BOUND);
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
      set_stmt_error(stmt, CR_OUT_OF_MEMORY);
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
    set_stmt_error(stmt, CR_NO_PREPARE_STMT);
    DBUG_RETURN(1);
  }
  if (!stmt->param_count)
  {
    set_stmt_error(stmt, CR_NO_PARAMETERS_EXISTS);
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
    set_stmt_error(stmt, CR_INVALID_PARAMETER_NO);
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
      set_stmt_errmsg(stmt,(char *) mysql->net.last_error,
		      mysql->net.last_errno);
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
      uint length= (uint)(longlong10_to_str(value,buffer,10)-buffer);
      *param->length= length;
      buffer[length]='\0';
    }
  } 
}


/* Convert Double to buffer types */
static void send_data_double(MYSQL_BIND *param, double value)
{  
  char *buffer= param->buffer;

  switch(param->buffer_type) {
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
      uint length= my_sprintf(buffer,(buffer,"%g",value));
      *param->length= length;
      buffer[length]='\0';
    }
  } 
}

/* Convert string to buffer types */
static void send_data_str(MYSQL_BIND *param, char *value, uint length)
{  
  char *buffer= param->buffer;
  int err=0;

  switch(param->buffer_type) {
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
  default:
    *param->length= length;
    length= min(length, param->buffer_length);
    memcpy(buffer, value, length);
    if (length != param->buffer_length)
      buffer[length]='\0';
  } 
}

static void send_data_time(MYSQL_BIND *param, MYSQL_TIME ltime, 
                           uint length)
{
  switch (param->buffer_type) {

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
    set_stmt_error(stmt, CR_NO_PREPARE_STMT);
    DBUG_RETURN(1);
  }
  if (!bind)
  {
    set_stmt_error(stmt, CR_NULL_POINTER);
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
    /* Setup data copy functions for the different supported types */
    switch (param->buffer_type) {
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
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      DBUG_ASSERT(param->buffer_length != 0);
      param->fetch_result= fetch_result_str;
      break;
    default:
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
      *bind->is_null= 1;
    else
    { 
      *bind->is_null= 0;
      if (field->type == bind->buffer_type)
        (*bind->fetch_result)(bind, &row);
      else 
      {
        my_bool field_is_unsigned= (field->flags & UNSIGNED_FLAG) ? 1: 0;
        fetch_results(bind, field->type, &row, field_is_unsigned);
      }
    }
    if (! ((bit<<=1) & 255))
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

  row= (uchar *)0;
  if (stmt->result_buffered) /* buffered */
  {
    MYSQL_RES *res;
    
    if (!(res= stmt->result) || !res->data_cursor) 
      goto no_data;
    
    row= (uchar *)res->data_cursor->data;
    res->data_cursor= res->data_cursor->next;
    res->current_row= (MYSQL_ROW)row;    
  }
  else /* un-buffered */
  {
    if (packet_error == net_safe_read(mysql))
    {
      set_stmt_errmsg(stmt,(char *)mysql->net.last_error,
                      mysql->net.last_errno);
      DBUG_RETURN(1);
    }
    if (mysql->net.read_pos[0] == 254)
    {
      mysql->status= MYSQL_STATUS_READY;
      goto no_data;
    }
    row= mysql->net.read_pos+1;
  }
  DBUG_RETURN(stmt_fetch_row(stmt, row));

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
    set_stmt_errmsg(stmt,(char *)mysql->net.last_error,
                    mysql->net.last_errno);
    DBUG_RETURN(0);
  }
  if (mysql->net.read_pos[0] == 254) /* end of data */
    return 0;				

  if (!(result=(MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
				       MYF(MY_WME | MY_ZEROFILL))))
  {
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
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
      net->last_errno=CR_OUT_OF_MEMORY;
      strmov(net->last_error,ER(net->last_errno));
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
    mysql->server_status= uint2korr(cp+3);
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
    set_stmt_error(stmt, CR_COMMANDS_OUT_OF_SYNC);
    DBUG_RETURN(1);
  }
  mysql->status= MYSQL_STATUS_READY;		/* server is ready */
  if (!(result= (MYSQL_RES*) my_malloc((uint) (sizeof(MYSQL_RES)+
					      sizeof(ulong) *
					      stmt->field_count),
				      MYF(MY_WME | MY_ZEROFILL))))
  {
    set_stmt_error(stmt, CR_OUT_OF_MEMORY);
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
      set_stmt_errmsg(stmt, mysql->net.last_error, mysql->net.last_errno);
      stmt->mysql= NULL; /* connection isn't valid anymore */
      DBUG_RETURN(1);
    }
  }
  mysql_free_result(stmt->result);
  free_root(&stmt->mem_root, MYF(0));
  if (!skip_list)
    mysql->stmts= list_delete(mysql->stmts, &stmt->list);
  mysql->status= MYSQL_STATUS_READY;
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
  mysql->affected_rows= ~(my_ulonglong) 0;

  if (mysql->last_used_con->server_status & SERVER_MORE_RESULTS_EXISTS)
    DBUG_RETURN(mysql_read_query_result(mysql));
  
  DBUG_RETURN(0);
}
