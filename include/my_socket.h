
#ifndef MY_SOCKET_H
#define MY_SOCKET_H


#ifdef __WIN__
#include <my_socket_win32.h>
#else
#include <my_socket_posix.h>
#endif

#endif
