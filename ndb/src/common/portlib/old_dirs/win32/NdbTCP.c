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

