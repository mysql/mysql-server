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
#include <my_net.h>
#include <NdbTCP.h>

extern "C"
int 
Ndb_getInAddr(struct in_addr * dst, const char *address) {
  DBUG_ENTER("Ndb_getInAddr");
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
      DBUG_RETURN(0);
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
    DBUG_RETURN(0);
  }
  DBUG_PRINT("error",("inet_addr(%s) - %d - %s",
		      address, errno, strerror(errno)));
  DBUG_RETURN(-1);
}

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
