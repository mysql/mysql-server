/* Copyright (C) 2003 MySQL AB

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

#include <ndb_global.h>
#include "NdbTCP.h"

int 
Ndb_getInAddr(struct in_addr * dst, const char *address) 
{
    struct hostent * hostPtr;

    /* Try it as aaa.bbb.ccc.ddd. */
    dst->s_addr = inet_addr(address);
    if (dst->s_addr != -1) {
        return 0;
    }

    hostPtr = gethostbyname(address);
    if (hostPtr != NULL) {
        dst->s_addr = ((struct in_addr *) *hostPtr->h_addr_list)->s_addr;
        return 0;
    }
    
    return -1;
}

int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock)
{
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

  if(select(1, &readfds, &writefds, &errorfds, &tv)==SOCKET_ERROR)
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
}
