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


//const int Compact_coder::MAX_ENCODED_LENGTH;


int Compact_coder::get_unsigned_encoded_length(ulonglong n)
{
  if (n == 0)
    return 1;
  // len is the total number of bytes to write
  int len= 16;
  if ((n & 0xFF00000000000000LL) == 0) // 56 bits set
    len-= 8;
  else
    n >>= 56;
  if ((n & 0x00fffffff0000000LL) == 0) // 28 bits set
    len-= 4;
  else
    n >>= 28;
  if ((n & 0x000000000fffc000LL) == 0) // 14 bits set
    len-= 2;
  else
    n >>= 14;
  if ((n & 0x0000000000003f80LL) == 0) //  7 bits set
    len--;
  return len;
}


int Compact_coder::write_unsigned(uchar *buf, ulonglong n)
{
  DBUG_ENTER("Compact_coder::write_unsigned(ulonglong , char *)");
  int len= get_unsigned_encoded_length(n);
  DBUG_PRINT("info", ("encoded-length=%u\n", len));
  if (len > 8)
    int2store(buf + 8, (uint)(n >> (64 - len)));
  n= (n << len) + (1 << (len - 1));
  int8store(buf, n);
  DBUG_RETURN(len);
}


enum_append_status
Compact_coder::append_unsigned(Appender *appender, ulonglong n)
{
  DBUG_ENTER("Compact_coder::append_unsigned(Appender *, ulonglong)");
  uchar buf[MAX_ENCODED_LENGTH];
  int len= write_unsigned(buf, n);
  PROPAGATE_APPEND_STATUS(appender->append(buf, len));
  DBUG_RETURN(APPEND_OK);
}


enum_read_status
Compact_coder::read_unsigned(Reader *reader, ulonglong *out)
{
  DBUG_ENTER("Compact_coder::read_unsigned");
  // read first byte
  uchar b;
  PROPAGATE_READ_STATUS(reader->read(&b, 1));
  if (b & 1)
  {
    *out= b >> 1;
    DBUG_RETURN(READ_OK);
  }
  // read second byte if needed
  uint extra_byte= 0;
  if (b == 0)
  {
    PROPAGATE_READ_STATUS_NOEOF(reader->read(&b, 1));
    // one of the two lowest bits must be set in order for the number
    // to be termiated after the 64th bit.
    if (!(b & 3))
      goto file_format_error;
    extra_byte= 1;
  }
  {
    // set len to the position of the least significant 1-bit in b (range 0..7)
    uint b_tmp= b;
    uint len= 7;
    if (b & 0x0f)
      len-= 4;
    else
      b_tmp >>= 4;
    if (b_tmp & 0x03)
      len-= 2;
    else
      b_tmp >>= 2;
    if (b_tmp & 0x01)
      len--;
    // read len bytes
    uchar buf[8]= {0, 0, 0, 0, 0, 0, 0, 0};
    PROPAGATE_READ_STATUS_NOEOF(reader->read(buf, len + extra_byte * 7));
    {
      ulonglong o= uint8korr(buf);
      // check that the result will fit in 64 bits
      if ((o & (0xfe00000000000000ULL << len)) != 0)
        goto file_format_error;
      // put the high bits of b in the low bits of o
      o <<= (7 - len);
      o |= b >> (len + 1);
      *out= o;
      DBUG_RETURN(READ_OK);
    }
  }
file_format_error:
  my_off_t ofs;
  reader->tell(&ofs);
  BINLOG_ERROR(("File '%.250s' has an unknown format at position %lld, "
                "it may be corrupt.",
                reader->get_source_name(), ofs),
               (ER_FILE_FORMAT, MYF(0), reader->get_source_name(), ofs));
  DBUG_RETURN(READ_ERROR);
}


enum_read_status
Compact_coder::read_unsigned(Reader *reader, ulong *out)
{
  DBUG_ENTER("Compact_coder::read_unsigned");
  // read first byte
  uchar b;
  PROPAGATE_READ_STATUS(reader->read(&b, 1));
  if (b & 1)
  {
    *out= b >> 1;
    DBUG_RETURN(READ_OK);
  }
  if (b == 0)
    goto file_format_error;
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
    PROPAGATE_READ_STATUS_NOEOF(reader->read(buf, len));
    {
      ulonglong o= uint8korr(buf);
      o <<= (7 - len);
      o |= b >> (len + 1);
      // check that the result will fit in 64 bits
      if (o >= 0x100000000ull)
        goto file_format_error;
      // put the high bits of b in the low bits of o
      *out= (ulong)o;
      DBUG_RETURN(READ_OK);
    }
  }
file_format_error:
  my_off_t ofs;
  reader->tell(&ofs);
  BINLOG_ERROR(("File '%.250s' has an unknown format at position %lld, "
                "it may be corrupt.",
                reader->get_source_name(), ofs),
               (ER_FILE_FORMAT, MYF(0), reader->get_source_name(), ofs));
  DBUG_RETURN(READ_ERROR);
}


enum_read_status Compact_coder::read_signed(Reader *reader, longlong *out)
{
  DBUG_ENTER("Compact_coder::read_signed");
  ulonglong o;
  PROPAGATE_READ_STATUS(read_unsigned(reader, &o));
  *out= unsigned_to_signed(o);
  DBUG_RETURN(READ_OK);
}


enum_read_status
Compact_coder::read_type_code(Reader *reader,
                              int min_fatal, int min_ignorable,
                              uchar *out, int code)
{
  DBUG_ENTER("Compact_coder::read_type_code");
  DBUG_ASSERT((min_fatal & 1) == 0);
  DBUG_ASSERT((min_ignorable & 1) == 1);

  if (code != -1)
    *out= code;
  else
  {
    PROPAGATE_READ_STATUS(reader->read(out, 1));
    code= *out;
  }

  if ((code & 1) == 0)
  {
    // even type code
    if (code < min_fatal)
      DBUG_RETURN(READ_OK);
    // unknown even type code: fatal
    my_off_t ofs;
    reader->tell(&ofs);
    BINLOG_ERROR(("File '%.200s' has an unknown format at position %lld, "
                  "it may be corrupt.",
                  reader->get_source_name(), ofs),
                 (ER_FILE_FORMAT, MYF(0), reader->get_source_name(), ofs));
    DBUG_RETURN(READ_ERROR);
  }
  else
  {
    // odd type code
    if (code < min_ignorable)
      DBUG_RETURN(READ_OK);
    // unknown odd type code: ignorable
    ulonglong skip_len;
    PROPAGATE_READ_STATUS_NOEOF(read_unsigned(reader, &skip_len));
    PROPAGATE_READ_STATUS_NOEOF(reader->seek(skip_len));
    DBUG_RETURN(READ_OK);
  }
}


enum_read_status Compact_coder::read_string(Reader *reader, uchar *buf,
                                            size_t *length, size_t max_length,
                                            bool null_terminated)
{
  DBUG_ENTER("Compact_coder::read_string(Reader *, uchar *, size_t *, size_t, bool)");
  if (null_terminated)
    max_length--;
  ulonglong length_ulonglong;
  PROPAGATE_READ_STATUS(read_unsigned(reader, &length_ulonglong, max_length));
  *length= (size_t) length_ulonglong;
  PROPAGATE_READ_STATUS_NOEOF(reader->read(buf, *length));
  buf[*length]= 0;
  DBUG_RETURN(READ_OK);
}


enum_append_status Compact_coder::append_string(Appender *appender,
                                                const uchar *string,
                                                size_t length)
{
  DBUG_ENTER("Compact_coder::append_string(Reader *, uchar *, size_t)");
  PROPAGATE_APPEND_STATUS(append_unsigned(appender, length));
  PROPAGATE_APPEND_STATUS(appender->append(string, length));
  DBUG_RETURN(APPEND_OK);
}


#endif /* ifdef HAVE_UGID */
