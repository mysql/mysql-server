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

#include <NdbTCP.h>
#include <socket_io.h>
#include <NdbOut.hpp>

extern "C"
int
read_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
	    char * buf, int buflen){
  if(buflen < 1)
    return 0;
  
  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(socket, &readset);
  
  struct timeval timeout;
  timeout.tv_sec  = (timeout_millis / 1000);
  timeout.tv_usec = (timeout_millis % 1000) * 1000;

  const int selectRes = select(socket + 1, &readset, 0, 0, &timeout);
  if(selectRes == 0)
    return 0;
  
  if(selectRes == -1){
    return -1;
  }

  return recv(socket, &buf[0], buflen, 0);
}

extern "C"
int
readln_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
	      char * buf, int buflen){
  if(buflen <= 1)
    return 0;

  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(socket, &readset);
  
  struct timeval timeout;
  timeout.tv_sec  = (timeout_millis / 1000);
  timeout.tv_usec = (timeout_millis % 1000) * 1000;

  const int selectRes = select(socket + 1, &readset, 0, 0, &timeout);
  if(selectRes == 0)
    return 0;
  
  if(selectRes == -1){
    return -1;
  }
  
  int pos = 0; buf[pos] = 0;
  while(true){
    const int t = recv(socket, &buf[pos], 1, 0);
    if(t != 1){
      return -1;
    }
    if(buf[pos] == '\n'){
      buf[pos] = 0;

      if(pos > 0 && buf[pos-1] == '\r'){
	pos--;
	buf[pos] = 0;
      }

      return pos;
    }
    pos++;
    if(pos == (buflen - 1)){
      buf[pos] = 0;
      return buflen;
    }
    
    FD_ZERO(&readset);
    FD_SET(socket, &readset);
    timeout.tv_sec  = (timeout_millis / 1000);
    timeout.tv_usec = (timeout_millis % 1000) * 1000;
    const int selectRes = select(socket + 1, &readset, 0, 0, &timeout);
    if(selectRes != 1){
      return -1;
    }
  }
}

extern "C"
int
write_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
	     const char buf[], int len){
  fd_set writeset;
  FD_ZERO(&writeset);
  FD_SET(socket, &writeset);
  struct timeval timeout;
  timeout.tv_sec  = (timeout_millis / 1000);
  timeout.tv_usec = (timeout_millis % 1000) * 1000;

  const int selectRes = select(socket + 1, 0, &writeset, 0, &timeout);
  if(selectRes != 1){
    return -1;
  }

  const char * tmp = &buf[0];
  while(len > 0){
    const int w = send(socket, tmp, len, 0);
    if(w == -1){
      return -1;
    }
    len -= w;
    tmp += w;
    
    if(len == 0)
      break;
    
    FD_ZERO(&writeset);
    FD_SET(socket, &writeset);
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    const int selectRes = select(socket + 1, 0, &writeset, 0, &timeout);
    if(selectRes != 1){
      return -1;
    }
  }
  
  return 0;
}

extern "C"
int
print_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
	     const char * fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  int ret = vprint_socket(socket, timeout_millis, fmt, ap);
  va_end(ap);

  return ret;
}

extern "C"
int
println_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
	       const char * fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  int ret = vprintln_socket(socket, timeout_millis, fmt, ap);
  va_end(ap);
  return ret;
}

extern "C"
int
vprint_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
	      const char * fmt, va_list ap){
  char buf[1000];
  char *buf2 = buf;
  size_t size = sizeof(buf);

  if (fmt != 0) {
    size = vsnprintf(buf, sizeof(buf), fmt, ap);
    /* Check if the output was truncated */
    if(size >= sizeof(buf)) {
      buf2 = (char *)malloc(size+1);
      if(buf2 == NULL)
	return -1;
      vsnprintf(buf2, size, fmt, ap);
    } else
      size = sizeof(buf);
  } else
    buf[0] = 0;

  int ret = write_socket(socket, timeout_millis, buf2, strlen(buf2));
  if(buf2 != buf)
    free(buf2);
  return ret;
}

extern "C"
int
vprintln_socket(NDB_SOCKET_TYPE socket, int timeout_millis, 
		const char * fmt, va_list ap){
  char buf[1000];
  char *buf2 = buf;
  size_t size = sizeof(buf);

  if (fmt != 0) {
    size = vsnprintf(buf, sizeof(buf), fmt, ap);
    /* Check if the output was truncated */
    if(size >= sizeof(buf)-1) {
      buf2 = (char *)malloc(size+2);
      if(buf2 == NULL)
	return -1;
      vsnprintf(buf2, size+1, fmt, ap);
    } else
      size = sizeof(buf);
  } else
    buf[0] = 0;
  strlcat(buf2, "\n", size+2);

  int ret = write_socket(socket, timeout_millis, buf2, strlen(buf2));
  if(buf2 != buf)
    free(buf2);
  return ret;
}

#ifdef NDB_WIN32

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

