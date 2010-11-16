/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include <my_net.h>
#include <NdbTCP.h>



extern "C"
int 
Ndb_getInAddr(struct in_addr * dst, const char *address)
{
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
      return 0;
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
    return 0;
  }
  return -1;
}


static inline
int my_socket_nfds(ndb_socket_t s, int nfds)
{
#ifdef _WIN32
  (void)s;
#else
  if(s.fd > nfds)
    return s.fd;
#endif
  return nfds;
}

#define my_FD_SET(sock,set)   FD_SET(ndb_socket_get_native(sock), set)
#define my_FD_ISSET(sock,set) FD_ISSET(ndb_socket_get_native(sock), set)


int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock)
{
#ifdef HAVE_POLL
  struct pollfd pfd[1];
  int r;

  pfd[0].fd= sock.fd; // FIXME: THIS IS A BUG
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
  SOCKET_SIZE_TYPE s_err_size= sizeof(s_err);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);

  my_FD_SET(sock, &readfds);
  my_FD_SET(sock, &writefds);
  my_FD_SET(sock, &errorfds);

  if(select(my_socket_nfds(sock,0)+1, &readfds, &writefds, &errorfds, &tv)<0)
    return 1;

  if(my_FD_ISSET(sock,&errorfds))
    return 1;

  s_err=0;
  if (my_getsockopt(sock, SOL_SOCKET, SO_ERROR, &s_err, &s_err_size) != 0)
    return(1);

  if (s_err)
  {                                             /* getsockopt could succeed */
    return(1);                                 /* but return an error... */
  }

  return 0;
#endif /* HAVE_POLL */
}
