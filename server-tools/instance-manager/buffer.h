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

#ifdef __GNUC__
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
  enum { BUFFER_INITIAL_SIZE= 4096 };
  /* maximum buffer size is 16Mb */
  enum { MAX_BUFFER_SIZE= 16777216 };
  size_t buffer_size;
public:
  Buffer()
  {
    buffer=(char *) malloc(BUFFER_INITIAL_SIZE);
    buffer_size= BUFFER_INITIAL_SIZE;
  }

  ~Buffer()
  {
    free(buffer);
  }

public:
  char *buffer;
  int append(uint position, const char *string, uint len_arg);
  int reserve(uint position, uint len_arg);
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_BUFFER_H */
