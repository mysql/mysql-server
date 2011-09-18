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


#ifdef HAVE_UGID


#include "mysqld_error.h"


const int Compact_encoding::MAX_ENCODED_LENGTH;


int Compact_encoding::get_unsigned_encoded_length(ulonglong n)
{
  if (n == 0)
    return 1;
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
  return len;
}


int Compact_encoding::write_unsigned(ulonglong n, uchar *buf)
{
  DBUG_ENTER("Compact_encoding::write_unsigned(ulonglong , char *)");
  int len= get_unsigned_encoded_length(n);
  DBUG_PRINT("info", ("encoded-length=%u\n", len));
  if (len > 8)
    int2store(buf + 8, (uint)(n >> (64 - len)));
  n= (n << len) + (1 << (len - 1));
  int8store(buf, n);
  DBUG_RETURN(len);
}


int Compact_encoding::write_unsigned(File fd, ulonglong n, myf my_flags)
{
  DBUG_ENTER("Compact_encoding::write_unsigned");
  uchar buf[10];
  int len= write_unsigned(n, buf);
  int ret= my_write(fd, buf, len, my_flags);
  DBUG_RETURN(ret != len ? -ret : ret);
}


int Compact_encoding::read_unsigned(File fd, ulonglong *out, myf my_flags)
{
  DBUG_ENTER("Compact_encoding::read_unsigned");
  // read first byte
  uchar b;
  size_t ret= my_read(fd, &b, 1, my_flags);
  if (ret != 1)
  {
    *out= (ret == 0 ? 1 : 0);
    DBUG_RETURN(0);
  }
  if (b & 1)
  {
    *out= b >> 1;
    DBUG_RETURN(1);
  }
  // read second byte if needed
  uint extra_byte= 0;
  if (b == 0)
  {
    ret= my_read(fd, &b, 1, my_flags);
    if (ret != 1)
    {
      *out= (ret == 0 ? 1 : 0); // 0 = IO error; 1 = file truncated
      if (my_flags & MY_WME)
        my_error(ER_ERROR_ON_READ, MYF(0), my_filename(fd), my_errno);
      DBUG_RETURN(-1);
    }
    // one of the two lowest bits must be set in order for the number
    // to be termiated after the 64th bit.
    if (!(b & 3))
    {
      *out= 2; // 2 = file format error
      if (my_flags & MY_WME)
        my_error(ER_FILE_FORMAT, MYF(0), my_filename(fd));
      DBUG_RETURN(-1);
    }
    extra_byte= 1;
  }
  // set len to the position of the least significant 1-bit in b (range 0..7)
  uint len= 7;
  if (b & 0x0f)
    len-= 4;
  if (b & 0x33)
    len-= 2;
  if (b & 0x55)
    len--;
  // read len bytes
  uchar buf[8]= {0, 0, 0, 0, 0, 0, 0, 0};
  ret= my_read(fd, buf, len + extra_byte * 7, my_flags);
  if (ret != len + extra_byte * 7)
  {
    *out= (ret < len + extra_byte * 7 ? 1 : 0);
    if (my_flags & MY_WME)
      my_error(ER_ERROR_ON_READ, MYF(0), my_filename(fd), my_errno);
    DBUG_RETURN(-(1 + extra_byte + ret));
  }
  ulonglong o= uint8korr(buf);
  // check that the result will fit in 64 bits
  if (o >= (1 << (64 - (7 - len))))
  {
    *out= 2; // 2 = file format error
    if (my_flags & MY_WME)
      my_error(ER_FILE_FORMAT, MYF(0), my_filename(fd));
    DBUG_RETURN(-(1 + extra_byte + len + extra_byte * 7));
  }
  // put the high bits of b in the low bits of o
  o <<= (7 - len);
  o |= b >> (len + 1);
  *out= o;
  return 1 + extra_byte + len + extra_byte * 7;
}


int Compact_encoding::write_signed(File fd, longlong n, myf my_flags)
{
  return write_unsigned(fd, signed_to_unsigned(n), my_flags);
}


int Compact_encoding::read_signed(File fd, longlong *out, myf my_flags)
{
  ulonglong o;
  int ret= read_unsigned(fd, &o, my_flags);
  if (ret <= 0)
    return ret;
  *out= unsigned_to_signed(o);
  return ret;
}


#endif /* ifdef HAVE_UGID */
