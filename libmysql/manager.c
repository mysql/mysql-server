/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

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
#define SOCKET_ERROR -1
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
  uint32 ip_addr;
  char msg_buf[MAX_MYSQL_MANAGER_MSG];
  int msg_len;

  if (!host)
    host="localhost";
  if (!user)
    user="root";
  if (!passwd)
    passwd="";
  
  if ((sock=(my_socket)socket(AF_INET,SOCK_STREAM,0)) == SOCKET_ERROR)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Cannot create socket");
    goto err;
  }
  if (!(con->vio=vio_new(sock,VIO_TYPE_TCPIP,FALSE)))
  {
    con->last_errno=ENOMEM;
    strmov(con->last_error,"Cannot create network I/O object");
    goto err;
  }
  vio_blocking(con->vio,TRUE);
  bzero((char*) &sock_addr,sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  if ((int) (ip_addr = inet_addr(host)) != (int) INADDR_NONE)
  {
    memcpy_fixed(&sock_addr.sin_addr,&ip_addr,sizeof(ip_addr));
  }
  else
#if defined(HAVE_GETHOSTBYNAME_R) && defined(_REENTRANT) && defined(THREAD)
  {
    int tmp_errno;
    struct hostent tmp_hostent,*hp;
    char buff2[GETHOSTBYNAME_BUFF_SIZE];
    hp = my_gethostbyname_r(host,&tmp_hostent,buff2,sizeof(buff2),
			    &tmp_errno);
    if (!hp)
    {
      con->last_errno=tmp_errno;
      sprintf(con->last_error,"Could not resolve host '%s'",host);
      goto err;
    }
    memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
  }
#else
  {
    struct hostent *hp;
    if (!(hp=gethostbyname(host)))
    {
      con->last_errno=socket_errno;
      sprintf(con->last_error, "Could not resolve host '%s'", host);
      goto err;
    }
    memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
  }
#endif
  sock_addr.sin_port = (ushort) htons((ushort) port);
  if (my_connect(sock,(struct sockaddr *) &sock_addr, sizeof(sock_addr),
		 0) <0)
  {
    con->last_errno=errno;
    sprintf(con->last_error ,"Could not connect to %-.64s", host);
    goto err;
  }
  /* read the greating */
  if (vio_read(con->vio,msg_buf,MAX_MYSQL_MANAGER_MSG)<=0)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Read error on socket");
    goto err;
  }
  sprintf(msg_buf,"%-.16s %-.16s\n",user,passwd);
  msg_len=strlen(msg_buf);
  if (vio_write(con->vio,msg_buf,msg_len)!=msg_len)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Write error on socket");
    goto err;
  }
  if (vio_read(con->vio,msg_buf,MAX_MYSQL_MANAGER_MSG)<=0)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Read error on socket");
    goto err;
  }
  if ((con->cmd_status=atoi(msg_buf)) != MANAGER_OK)
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

void            STDCALL mysql_manager_close(MYSQL_MANAGER* con)
{
  my_free((gptr)con->host,MYF(MY_ALLOW_ZERO_PTR));
  /* no need to free con->user and con->passwd, because they were
     allocated in my_multimalloc() along with con->host, freeing
     con->hosts frees the whole block
  */
  if (con->vio)
  {
    vio_delete(con->vio);
    con->vio=0;
  }
  if (con->free_me)
    my_free((gptr)con,MYF(0));
}

int STDCALL mysql_manager_command(MYSQL_MANAGER* con,const char* cmd,
				  int cmd_len)
{
  if (!cmd_len)
    cmd_len=strlen(cmd);
  if (vio_write(con->vio,(char*)cmd,cmd_len) != cmd_len)
  {
    con->last_errno=errno;
    strmov(con->last_error,"Write error on socket");
    return 1;
  }
  con->eof=0;
  return 0;
}

int  STDCALL mysql_manager_fetch_line(MYSQL_MANAGER* con, char* res_buf,
						 int res_buf_size)
{
  char* res_buf_end=res_buf+res_buf_size;
  char* net_buf_pos=con->net_buf_pos, *net_buf_end=con->net_data_end;
  int res_buf_shift=RES_BUF_SHIFT;
  int done=0;
  
  if (res_buf_size<RES_BUF_SHIFT)
  {
    con->last_errno=ENOMEM;
    strmov(con->last_error,"Result buffer too small");
    return 1;
  }
  
  for (;;)
  {
    for (;net_buf_pos<net_buf_end && res_buf<res_buf_end;
	 net_buf_pos++,res_buf++)
    {
      char c=*net_buf_pos;
      if (c == '\r')
	c=*++net_buf_pos;
      if (c == '\n')
      {
	*res_buf=0;
	net_buf_pos++;
	done=1;
	break;
      }
      else
	*res_buf=*net_buf_pos;
    }
    if (done || res_buf==res_buf_end)
      break;
      
    if (net_buf_pos == net_buf_end && res_buf<res_buf_end)
    {
      int num_bytes;
      if ((num_bytes=vio_read(con->vio,con->net_buf,con->net_buf_size))<=0)
      {
	con->last_errno=errno;
	strmov(con->last_error,"socket read failed");
	return 1;
      }
      net_buf_pos=con->net_buf;
      net_buf_end=net_buf_pos+num_bytes;
    }
  }
  con->net_buf_pos=net_buf_pos;
  con->net_data_end=net_buf_end;
  res_buf=res_buf_end-res_buf_size;
  if ((con->eof=(res_buf[3]==' ')))
    res_buf_shift--;
  res_buf_end-=res_buf_shift;
  for (;res_buf<res_buf_end;res_buf++)
  {
    if(!(*res_buf=res_buf[res_buf_shift]))
      break;
  }
  return 0;
}




