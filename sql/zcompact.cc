/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "zgroups.h"


int Compact_encoding::write_unsigned(File fd, ulonglong n, myf my_flags)
{
  DBUG_ENTER("Compact_encoding::write_unsigned");
  uchar buf[10];
  // len is the total number of bytes to write
  int len= 16;
  if (n & 0x00FFFFFFFfffffffLL) // 56 bits set
    len-= 8;
  if (n & 0xff0000000fffffffLL) // 28 bits set
    len-= 4;
  if (n & 0xff0003fff0003fffLL) // 14 bits set
    len-= 2;
  if (n & 0x7f01fc07f01fc07fLL) //  7 bits set
    len--;
  if (len > 8)
    int2store(buf + 8, (uint)(n >> (64 - len)));
  n= (n << len) + (1 << (len - 1));
  int8store(buf, n);
  int ret= my_write(fd, buf, len, my_flags);
  DBUG_RETURN(ret);
}


int Compact_encoding::read_unsigned(File fd, ulonglong *out, myf my_flags)
{
  DBUG_ENTER("Compact_encoding::read_unsigned");
  // read first byte
  uchar b;
  if (my_read(fd, &b, 1, my_flags) != 1)
    DBUG_RETURN(0);
  if (b & 1)
  {
    *out= b >> 1;
    DBUG_RETURN(1);
  }
  // read second byte if needed
  int extra_byte= 0;
  if (b == 0)
  {
    if (my_read(fd, &b, 1, my_flags) != 1)
      DBUG_RETURN(-1);
    if (!(b & 3))
      DBUG_RETURN(-0x10001);
    extra_byte= 1;
  }
  // set len to the position of the least significant 1-bit in b (range 1..8)
  int len= 8;
  if (b & 0x0f)
    len-= 4;
  if (b & 0x33)
    len-= 2;
  if (b & 0x55)
    len--;
  // read len bytes
  uchar buf[8]= {0, 0, 0, 0, 0, 0, 0, 0};
  int n_read_bytes;
  n_read_bytes= my_read(fd, buf, len + extra_byte * 8, my_flags);
  if (n_read_bytes != len + extra_byte * 8)
    DBUG_RETURN(-(1 + n_read_bytes));
  ulonglong o= uint8korr(buf);
  if (o > (1 << (64 - (8 - len))))
    DBUG_RETURN(-(0x10001 + len + extra_byte + extra_byte * 8));
  o <<= (8 - len);
  o |= b >> len;
  *out= o;
  return 1 + extra_byte + len + extra_byte * 8;
}


int Compact_encoding::write_signed(File fd, longlong n, myf my_flags)
{
  return write_unsigned(fd, n >= 0 ? 2 * (ulonglong)n :
                        1 + 2 * (ulonglong)-(n + 1), my_flags);
}


int Compact_encoding::read_signed(File fd, longlong *out, myf my_flags)
{
  ulonglong o;
  int ret= read_unsigned(fd, &o, my_flags);
  if (ret <= 0)
    return ret;
  if (o & 1)
    *out= o >> 1;
  else
    *out= -1 - (longlong)(o >> 1);
  return ret;
}

