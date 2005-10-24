#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_BUFFER_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_BUFFER_H
/* Copyright (C) 2004 MySQL AB

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

#include <my_global.h>
#include <my_sys.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

/*
  This class is a simple implementation of the buffer of varying size.
  It is used to store MySQL client-server protocol packets. This is why
  the maximum buffer size if 16Mb. (See internals manual section
  7. MySQL Client/Server Protocol)
*/

class Buffer
{
private:
  static const uint BUFFER_INITIAL_SIZE;
  /* maximum buffer size is 16Mb */
  static const uint MAX_BUFFER_SIZE;
  size_t buffer_size;
  /* Error flag. Triggered if we get an error of some kind */
  int error;
public:
  Buffer(size_t buffer_size_arg= BUFFER_INITIAL_SIZE)
    :buffer_size(buffer_size_arg), error(0)
  {
    /*
      As append() will invokes realloc() anyway, it's ok if malloc returns 0
    */
    if (!(buffer= (char*) my_malloc(buffer_size, MYF(0))))
        buffer_size= 0;
  }

  ~Buffer()
  {
    my_free(buffer, MYF(0));
  }

public:
  char *buffer;
  int get_size();
  int is_error();
  int append(uint position, const char *string, uint len_arg);
  int reserve(uint position, uint len_arg);
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_BUFFER_H */
