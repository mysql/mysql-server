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
  Calculate a long checksum for a memoryblock. Used to verify pack_isam 
   
  SYNOPSIS
    checksum()
      mem	Pointer to memory block
      count	Count of bytes 
*/

ulong checksum(const byte *mem, uint count)
{
  ulong crc;
  for (crc= 0; count-- ; mem++)
    crc= ((crc << 1) + *((uchar*) mem)) +
      test(crc & ((ulong) 1L << (8*sizeof(ulong)-1)));
  return crc;
}
