
#ifndef MY_SOCKET_H
#define MY_SOCKET_H


#ifdef __WIN__
#include <my_socket_win32.h>
#else
#include <my_socket_posix.h>
#endif

C_MODE_START

/*
  create a pair of connected sockets
*/
int my_socketpair(my_socket s[2]);

C_MODE_END

#endif
