/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_global.h"

#include "portlib/ndb_socket_poller.h"
#include "portlib/NdbTick.h"
#include "util/socket_io.h"
#include "util/BaseString.hpp"

static inline
int
poll_socket(ndb_socket_t socket, bool read, bool write,
            int timeout_millis, int* total_elapsed_millis)
{
  const NDB_TICKS start = NdbTick_getCurrentTicks();

  timeout_millis -= *total_elapsed_millis;

  if (timeout_millis <= 0)
    return 0; // Timeout occurred

  const int res = ndb_poll(socket, read, write, timeout_millis);

  // Calculate elapsed time in this function
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const int elapsed_millis = (int)(NdbTick_Elapsed(start,now).milliSec());

  // Update the total elapsed time
  *total_elapsed_millis += elapsed_millis;

  return res;
}


extern "C"
int
read_socket(ndb_socket_t socket, int timeout_millis,
	    char * buf, int buflen){
  if(buflen < 1)
    return 0;

  int elapsed_millis = 0;
  const int res = poll_socket(socket, true, false,
                              timeout_millis, &elapsed_millis);
  if (res <= 0)
    return res;

  return (int)ndb_recv(socket, &buf[0], buflen, 0);
}

extern "C"
int
readln_socket(ndb_socket_t socket, int timeout_millis, int *time,
	      char * buf, int buflen, NdbMutex *mutex){
  if(buflen <= 1)
    return 0;

  if(mutex)
    NdbMutex_Unlock(mutex);

  const int res = poll_socket(socket, true, false,
                              timeout_millis, time);

  if(mutex)
    NdbMutex_Lock(mutex);

  if (res <= 0)
    return res;

  char* ptr = buf;
  int len = buflen;
  do
  {
    int t;
    while((t = (int)ndb_recv(socket, ptr, len, MSG_PEEK)) == -1
          && socket_errno == EINTR);
    
    if(t < 1)
    {
      return -1;
    }

    
    for(int i = 0; i<t; i++)
    {
      if(ptr[i] == '\n')
      {
	/**
	 * Now consume
	 */
	for (len = 1 + i; len; )
	{
          while ((t = (int)ndb_recv(socket, ptr, len, 0)) == -1
                 && socket_errno == EINTR);
	  if (t < 1)
	    return -1;
	  ptr += t;
	  len -= t;
	}
	if (t > 1 && ptr[-2] == '\r')
	{
	  ptr[-2] = '\n';
          ptr[-1] = '\0';
	  ptr--;
	}

        *time = 0;

	ptr[0]= 0;
	return (int)(ptr - buf);
      }
    }
    
    for (int tmp = t; tmp; )
    {
      while ((t = (int)ndb_recv(socket, ptr, tmp, 0)) == -1 && socket_errno == EINTR);
      if (t < 1)
      {
	return -1;
      }
      ptr += t;
      len -= t;
      tmp -= t;
      if (t > 0 && buf[t-1] == '\r')
      {
        buf[t-1] = '\n';
        ptr--;
      }
    }

    if (poll_socket(socket, true, false, timeout_millis, time) != 1)
    {
      // Read some bytes but didn't find newline before all time was
      // used up => return error
      return -1;
    }

  } while (len > 0);
  
  return -1;
}

extern "C"
int
write_socket(ndb_socket_t socket, int timeout_millis, int *time,
	     const char buf[], int len){

  if (poll_socket(socket, false, true, timeout_millis, time) != 1)
    return -1;

  const char * tmp = &buf[0];
  while(len > 0){
    const int w = (int)ndb_send(socket, tmp, len, 0);
    if(w == -1){
      return -1;
    }
    len -= w;
    tmp += w;
    
    if(len == 0)
      break;

    if (poll_socket(socket, false, true, timeout_millis, time) != 1)
      return -1;
  }
  
  return 0;
}

#ifdef _WIN32

class INIT_WINSOCK2
{
public:
    INIT_WINSOCK2(void);
    ~INIT_WINSOCK2(void);

private:
    bool m_bAcceptable;
};

INIT_WINSOCK2 g_init_winsock2;

INIT_WINSOCK2::INIT_WINSOCK2(void)
: m_bAcceptable(false)
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    
    wVersionRequested = MAKEWORD( 2, 2 );
    
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        m_bAcceptable = false;
    }
    
    /* Confirm that the WinSock DLL supports 2.2.*/
    /* Note that if the DLL supports versions greater    */
    /* than 2.2 in addition to 2.2, it will still return */
    /* 2.2 in wVersion since that is the version we      */
    /* requested.                                        */
    
    if ( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 ) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        WSACleanup( );
        m_bAcceptable = false; 
    }
    
    /* The WinSock DLL is acceptable. Proceed. */
    m_bAcceptable = true;
}

INIT_WINSOCK2::~INIT_WINSOCK2(void)
{
    if(m_bAcceptable)
    {
        m_bAcceptable = false;
        WSACleanup();
    }
}

#endif

