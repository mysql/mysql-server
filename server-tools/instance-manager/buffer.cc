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

#ifdef __GNUC__
#pragma implementation
#endif

#include "buffer.h"
#include <m_string.h>


/*
  Puts the given string to the buffer.

  SYNOPSYS
    append()
    position          start position in the buffer
    string            string to be put in the buffer
    len_arg           the length of the string. This way we can avoid some
                      strlens.

  DESCRIPTION

    The method puts a string into the buffer, starting from position .
    In the case when the buffer is too small it reallocs the buffer. The
    total size of the buffer is restricted with 16.

  RETURN
    0 - ok
    1 - The buffer came to 16Mb barrier
*/

int Buffer::append(uint position, const char *string, uint len_arg)
{
  if (reserve(position, len_arg))
    return 1;

  strnmov(buffer + position, string, len_arg);
  return 0;
}


/*
  Checks whether the current buffer size is ok to put a string of the length
  "len_arg" starting from "position" and reallocs it if no.

  SYNOPSYS
    reserve()
    position          the number starting byte on the buffer to store a buffer
    len_arg           the length of the string.

  DESCRIPTION

    The method checks whether it is possible to pus a string of teh "len_arg"
    length into the buffer, starting from "position" byte. In the case when the
    buffer is too small it reallocs the buffer. The total size of the buffer is
    restricted with 16 Mb.

  RETURN
    0 - ok
    1 - The buffer came to 16Mb barrier
*/

int Buffer::reserve(uint position, uint len_arg)
{
  if (position + len_arg >= MAX_BUFFER_SIZE)
    goto err;

  if (position + len_arg>= buffer_size)
  {
    buffer= (char *) realloc(buffer,
                             min(MAX_BUFFER_SIZE,
                                 max((uint) (buffer_size*1.5),
                                     position + len_arg)));
    if (buffer= NULL)
      goto err;
    buffer_size= (uint) (buffer_size*1.5);
  }
  return 0;

err:
  return 1;
}

