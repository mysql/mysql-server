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
#include "NdbHost.h"


int NdbHost_GetHostName(char* buf)
{
    /* We must initialize TCP/IP if we want to call gethostname */
    WORD wVersionRequested;
    WSADATA wsaData;
    int err; 
    
    wVersionRequested = MAKEWORD( 2, 0 ); 
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {    
    /**
    * Tell the user that we couldn't find a usable
    * WinSock DLL.                               
        */
        return -1;
    }
    
    /* Get host name */
    if(gethostname(buf, MAXHOSTNAMELEN))
    {
        return -1;
    }
    return 0;
}


int NdbHost_GetProcessId(void)
{
    return _getpid();
}

