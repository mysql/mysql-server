/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>
#include <my_net.h>
#include <NdbTCP.h>

extern "C"
int 
Ndb_getInAddr(struct in_addr * dst, const char *address) {
  //  DBUG_ENTER("Ndb_getInAddr");
  {
    int tmp_errno;
    struct hostent tmp_hostent, *hp;
    char buff[GETHOSTBYNAME_BUFF_SIZE];
    hp = my_gethostbyname_r(address,&tmp_hostent,buff,sizeof(buff),
			    &tmp_errno);
    if (hp)
    {
      memcpy(dst, hp->h_addr, min(sizeof(*dst), (size_t) hp->h_length));
      my_gethostbyname_r_free();
      return 0; //DBUG_RETURN(0);
    }
    my_gethostbyname_r_free();
  }
  /* Try it as aaa.bbb.ccc.ddd. */
  dst->s_addr = inet_addr(address);
  if (dst->s_addr != 
#ifdef INADDR_NONE
      INADDR_NONE
#else
      -1
#endif
      )
  {
    return 0; //DBUG_RETURN(0);
  }
  //  DBUG_PRINT("error",("inet_addr(%s) - %d - %s",
  //		      address, errno, strerror(errno)));
  return -1; //DBUG_RETURN(-1);
}

#ifndef DBUG_OFF
extern "C"
int NDB_CLOSE_SOCKET(int fd)
{
  DBUG_PRINT("info", ("NDB_CLOSE_SOCKET(%d)", fd));
  return _NDB_CLOSE_SOCKET(fd);
}
#endif

#if 0
int 
Ndb_getInAddr(struct in_addr * dst, const char *address) {
  struct hostent host, * hostPtr;
  char buf[1024];
  int h_errno;
  hostPtr = gethostbyname_r(address, &host, &buf[0], 1024, &h_errno);
  if (hostPtr != NULL) {
    dst->s_addr = ((struct in_addr *) *hostPtr->h_addr_list)->s_addr;
    return 0;
  }
  
  /* Try it as aaa.bbb.ccc.ddd. */
  dst->s_addr = inet_addr(address);
  if (dst->s_addr != -1) {
    return 0;
  }
  return -1;
}
#endif

int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock)
{
#ifdef HAVE_POLL
  struct pollfd pfd[1];
  int r;

  pfd[0].fd= sock;
  pfd[0].events= POLLHUP | POLLIN | POLLOUT | POLLNVAL;
  pfd[0].revents= 0;
  r= poll(pfd,1,0);
  if(pfd[0].revents & (POLLHUP|POLLERR))
    return 1;

  return 0;
#else /* HAVE_POLL */
  fd_set readfds, writefds, errorfds;
  struct timeval tv= {0,0};
  int s_err;
  int s_err_size= sizeof(s_err);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);

  FD_SET(sock, &readfds);
  FD_SET(sock, &writefds);
  FD_SET(sock, &errorfds);

  if(select(1, &readfds, &writefds, &errorfds, &tv)<0)
    return 1;

  if(FD_ISSET(sock,&errorfds))
    return 1;

  s_err=0;
  if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return(1);

  if (s_err)
  {                                             /* getsockopt could succeed */
    return(1);                                 /* but return an error... */
  }

  return 0;
#endif /* HAVE_POLL */
}
