/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   There are special exceptions to the terms and conditions of the GPL as it
   is applied to this software. View the full text of the exception in file
   EXCEPTIONS-CLIENT in the directory of this software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#if defined(THREAD)
#include <my_pthread.h>				/* because of signal()	*/
#endif
#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include <my_net.h>
#include <errmsg.h>
#include <violite.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#if defined(OS2)
#  include <sys/un.h>
#elif defined(__NETWARE__)
#include <netdb.h>
#include <sys/select.h>
#include <sys/utsname.h>
#elif !defined( __WIN__)
#include <sys/resource.h>
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/utsname.h>
#endif /* __WIN__ */

#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

#define RES_BUF_SHIFT 5
#define NET_BUF_SIZE  2048

MYSQL_MANAGER*  STDCALL mysql_manager_init(MYSQL_MANAGER* con)
{
  int net_buf_size=NET_BUF_SIZE;
  if (!con)
  {
    if (!(con=(MYSQL_MANAGER*)my_malloc(sizeof(*con)+net_buf_size,
					MYF(MY_WME|MY_ZEROFILL))))
      return 0;
    con->free_me=1;
    con->net_buf=(char*)con+sizeof(*con);    
  }
  else
  {
    bzero((char*)con,sizeof(*con));
    if (!(con->net_buf=my_malloc(net_buf_size,MYF(0))))
      return 0;
  }
  con->net_buf_pos=con->net_data_end=con->net_buf;
  con->net_buf_size=net_buf_size;
  return con;
}

MYSQL_MANAGER*  STDCALL mysql_manager_connect(MYSQL_MANAGER* con,
					      const char* host,
					      const char* user,
					      const char* passwd,
					      unsigned int port)
{
  my_socket sock;
  struct sockaddr_in sock_addr;
  in_addr_t ip_addr;
  char msg_buf[MAX_MYSQL_MANAGER_MSG];
  int msg_len;
  Vio* vio;
  my_bool not_used;

  if (!host)
    host="localhost";
  if (!user)
    user="root";
  if (!passwd)
    passwd="";

  if ((sock=(my_socket)socket(AF_INET,SOCK_STREAM,0)) == INVALID_SOCKET)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Cannot create socket");
    goto err;
  }
  if (!(vio=vio_new(sock,VIO_TYPE_TCPIP,FALSE)))
  {
    con->last_errno=ENOMEM;
    strmov(con->last_error,"Cannot create network I/O object");
    goto err;
  }
  vio_blocking(vio, TRUE, &not_used);
  my_net_init(&con->net,vio);
  bzero((char*) &sock_addr,sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
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
      con->last_errno=tmp_errno;
      sprintf(con->last_error,"Could not resolve host '%-.64s'",host);
      my_gethostbyname_r_free();
      goto err;
    }
    memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
    my_gethostbyname_r_free();
  }
  sock_addr.sin_port = (ushort) htons((ushort) port);
  if (my_connect(sock,(struct sockaddr *) &sock_addr, sizeof(sock_addr),
		 0))
  {
    con->last_errno=errno;
    sprintf(con->last_error ,"Could not connect to %-.64s", host);
    goto err;
  }
  /* read the greating */
  if (my_net_read(&con->net) == packet_error)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Read error on socket");
    goto err;
  }
  sprintf(msg_buf,"%-.16s %-.16s\n",user,passwd);
  msg_len=strlen(msg_buf);
  if (my_net_write(&con->net,msg_buf,msg_len) || net_flush(&con->net))
  {
    con->last_errno=con->net.last_errno;
    strmov(con->last_error,"Write error on socket");
    goto err;
  }
  if (my_net_read(&con->net) == packet_error)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Read error on socket");
    goto err;
  }
  if ((con->cmd_status=atoi((char*) con->net.read_pos)) != MANAGER_OK)
  {
    strmov(con->last_error,"Access denied");
    goto err;
  }
  if (!my_multi_malloc(MYF(0), &con->host, (uint)strlen(host)+1,
		       &con->user, (uint)strlen(user)+1,
		       &con->passwd, (uint)strlen(passwd)+1,
		       NullS))
  {
    con->last_errno=ENOMEM;
    strmov(con->last_error,"Out of memory");
    goto err;
  }
  strmov(con->host,host);
  strmov(con->user,user);
  strmov(con->passwd,passwd);
  return con;

err:
  {
    my_bool free_me=con->free_me;
    con->free_me=0;
    mysql_manager_close(con);
    con->free_me=free_me;
  }
  return 0;
}

void STDCALL mysql_manager_close(MYSQL_MANAGER* con)
{
  /*
    No need to free con->user and con->passwd, because they were
    allocated in my_multimalloc() along with con->host, freeing
    con->hosts frees the whole block
  */
  my_free((gptr)con->host,MYF(MY_ALLOW_ZERO_PTR));
  net_end(&con->net);
  if (con->free_me)
    my_free((gptr)con,MYF(0));
}


int STDCALL mysql_manager_command(MYSQL_MANAGER* con,const char* cmd,
				  int cmd_len)
{
  if (!cmd_len)
    cmd_len=strlen(cmd);
  if (my_net_write(&con->net,(char*)cmd,cmd_len) || net_flush(&con->net))
  {
    con->last_errno=errno;
    strmov(con->last_error,"Write error on socket");
    return 1;
  }
  con->eof=0;
  return 0;
}


int STDCALL mysql_manager_fetch_line(MYSQL_MANAGER* con, char* res_buf,
				     int res_buf_size)
{
  char* res_buf_end=res_buf+res_buf_size;
  char* net_buf=(char*) con->net.read_pos, *net_buf_end;
  int res_buf_shift=RES_BUF_SHIFT;
  ulong num_bytes;

  if (res_buf_size<RES_BUF_SHIFT)
  {
    con->last_errno=ENOMEM;
    strmov(con->last_error,"Result buffer too small");
    return 1;
  }

  if ((num_bytes=my_net_read(&con->net)) == packet_error)
  {
    con->last_errno=errno;
    strmov(con->last_error,"socket read failed");
    return 1;
  }

  net_buf_end=net_buf+num_bytes;

  if ((con->eof=(net_buf[3]==' ')))
    res_buf_shift--;
  net_buf+=res_buf_shift;
  res_buf_end[-1]=0;
  for (;net_buf<net_buf_end && res_buf < res_buf_end;res_buf++,net_buf++)
  {
    if ((*res_buf=*net_buf) == '\r')
    {
      *res_buf=0;
      break;
    }
  }
  return 0;
}
