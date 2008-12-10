/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Calculate a checksum for a row */

#include "maria_def.h"

/**
   Calculate a checksum for the record

   _ma_checksum()
   @param info		Maria handler
   @param record	Record

   @note
     To ensure that the checksum is independent of the row format
     we need to always calculate the checksum in the original field order.

   @return  checksum
*/

ha_checksum _ma_checksum(MARIA_HA *info, const uchar *record)
{
  ha_checksum crc=0;
  uint i,end;
  MARIA_COLUMNDEF *base_column= info->s->columndef;
  uint16 *column_nr= info->s->column_nr;

  if (info->s->base.null_bytes)
    crc= my_checksum(crc, record, info->s->base.null_bytes);

  for (i= 0, end= info->s->base.fields ; i < end ; i++)
  {
    MARIA_COLUMNDEF *column= base_column + column_nr[i];
    const uchar *pos;
    ulong length;

    if (record[column->null_pos] & column->null_bit)
      continue;                                 /* Null field */

    pos= record + column->offset;
    switch (column->type) {
    case FIELD_BLOB:
    {
      uint blob_size_length= column->length- portable_sizeof_char_ptr;
      length= _ma_calc_blob_length(blob_size_length, pos);
      if (length)
      {
        memcpy((char*) &pos, pos + blob_size_length, sizeof(char*));
        crc= my_checksum(crc, pos, length);
      }
      continue;
    }
    case FIELD_VARCHAR:
    {
      uint pack_length= column->fill_length;
      if (pack_length == 1)
        length= (ulong) *pos;
      else
        length= uint2korr(pos);
      pos+= pack_length;                        /* Skip length information */
      break;
    }
    default:
      length= column->length;
      break;
    }
    crc= my_checksum(crc, pos, length);
  }
  return crc;
}


ha_checksum _ma_static_checksum(MARIA_HA *info, const uchar *pos)
{
  return my_checksum(0, pos, info->s->base.reclength);
}
