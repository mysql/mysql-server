/* Copyright (C) 2000 MySQL AB

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
#include "my_sys.h"

/*
  Calculate a long checksum for a memoryblock.

  SYNOPSIS
    my_checksum()
      crc       start value for crc
      pos       pointer to memory block
      length    length of the block
*/

ha_checksum my_checksum(ha_checksum crc, const byte *pos, uint length)
{
#ifdef NOT_USED
  const byte *end=pos+length;
  for ( ; pos != end ; pos++)
    crc=((crc << 8) + *((uchar*) pos)) + (crc >> (8*sizeof(ha_checksum)-8));
  return crc;
#else
  return (ha_checksum)crc32((uint)crc, (const uchar *)pos, length);
#endif
}

