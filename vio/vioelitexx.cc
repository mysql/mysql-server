/* Copyright Abandoned 2000 Monty Program KB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
 * Renamed of violite.cc to violitexx.cc because of clashes
 * with violite.c
 * This file implements the same functions as in violite.c, but now using
 * the Vio class
 */

#include "vio-global.h"

Vio*
vio_new(my_socket sd, enum_vio_type type, my_bool localhost)
{
  return my_reinterpret_cast(Vio*) (new VioSocket(sd, type, localhost));
}


#ifdef __WIN32__
Vio
*vio_new_win32pipe(HANDLE hPipe)
{
  return my_reinterpret_cast(Vio*) (new VioPipe(hPipe));
}
#endif
