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


#include "NdbTCP.h"


int 
Ndb_getInAddr(struct in_addr * dst, const char *address) {
  struct hostent * host;
  host = gethostbyname_r(address);
  if(host != 0){
    dst->s_addr = ((struct in_addr *) *host->h_addr_list)->s_addr;
    free_buf((union SIGNAL **)&host);
    return 0;
  }
  /* Try it as aaa.bbb.ccc.ddd. */
  dst->s_addr = inet_addr(address);
  if (dst->s_addr != INADDR_NONE) {
    return 0;
  }
  return -1;
}


