
#ifndef MY_SOCKET_H
#define MY_SOCKET_H


#ifdef __WIN__
#include <ndb_socket_win32.h>
#else
#include <ndb_socket_posix.h>
#endif

C_MODE_START

/*
  create a pair of connected sockets
*/
int my_socketpair(ndb_socket_t s[2]);

C_MODE_END

#endif
