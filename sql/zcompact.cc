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


int Compact_encoding::write_unsigned(uchar *buf, ulonglong n)
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


enum_append_status
Compact_encoding::append_unsigned(Appender *appender, ulonglong n)
{
  DBUG_ENTER("Compact_encoding::append_unsigned(Appender *, ulonglong)");
  uchar buf[MAX_ENCODED_LENGTH];
  int len= write_unsigned(buf, n);
  PROPAGATE_APPEND_STATUS(appender->append(buf, len));
  DBUG_RETURN(APPEND_OK);
}


enum_read_status
Compact_encoding::read_unsigned(Reader *reader, ulonglong *out)
{
  DBUG_ENTER("Compact_encoding::read_unsigned");
  // read first byte
  uchar b;
  my_off_t saved_pos;
  if (reader->tell(&saved_pos) != RETURN_STATUS_OK)
    DBUG_RETURN(READ_ERROR);
  enum_read_status ret= reader->read(&b, 1);
  if (ret != READ_OK)
    DBUG_RETURN(ret);
  if (b & 1)
  {
    *out= b >> 1;
    DBUG_RETURN(READ_OK);
  }
  // read second byte if needed
  uint extra_byte= 0;
  if (b == 0)
  {
    ret= reader->read(&b, 1);
    if (ret != READ_OK)
      goto rewind;
    // one of the two lowest bits must be set in order for the number
    // to be termiated after the 64th bit.
    if (!(b & 3))
      goto file_format_error;
    extra_byte= 1;
  }
  {
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
    ret= reader->read(buf, len + extra_byte * 7);
    if (ret != READ_OK)
      goto rewind;
    {
      ulonglong o= uint8korr(buf);
      // check that the result will fit in 64 bits
      if (o >= (1 << (64 - (7 - len))))
        goto file_format_error;
      // put the high bits of b in the low bits of o
      o <<= (7 - len);
      o |= b >> (len + 1);
      *out= o;
      DBUG_RETURN(READ_OK);
    }
  }
file_format_error:
  my_error(ER_FILE_FORMAT, MYF(0), reader->get_source_name());
  ret= READ_ERROR;
  // FALLTHROUGH
rewind:
  if (reader->seek(saved_pos) != RETURN_STATUS_OK)
    DBUG_RETURN(READ_ERROR);
  DBUG_RETURN(ret == READ_EOF ? READ_TRUNCATED : ret);
}


enum_append_status Compact_encoding::append_signed(Appender *appender, longlong n)
{
  return append_unsigned(appender, signed_to_unsigned(n));
}


enum_read_status Compact_encoding::read_signed(Reader *reader, longlong *out)
{
  DBUG_ENTER("Compact_encoding::read_signed");
  ulonglong o;
  PROPAGATE_READ_STATUS(read_unsigned(reader, &o));
  *out= unsigned_to_signed(o);
  DBUG_RETURN(READ_OK);
}


#endif /* ifdef HAVE_UGID */
