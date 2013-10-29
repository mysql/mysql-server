/* Copyright (C) 2000-2001, 2003-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/* Calculate a checksum for a row */

#include "myisamdef.h"

ha_checksum mi_checksum(MI_INFO *info, const uchar *buf)
{
  ha_checksum crc=0;
  const uchar *record= buf;
  MI_COLUMNDEF *column= info->s->rec;
  MI_COLUMNDEF *column_end= column+ info->s->base.fields;
  my_bool skip_null_bits= test(info->s->options & HA_OPTION_NULL_FIELDS);

  for ( ; column != column_end ; buf+= column++->length)
  {
    const uchar *pos;
    ulong length;

    if ((record[column->null_pos] & column->null_bit) &&
        skip_null_bits)
      continue;                                 /* Null field */

    switch (column->type) {
    case FIELD_BLOB:
    {
      length=_mi_calc_blob_length(column->length-
                                  portable_sizeof_char_ptr,
                                  buf);
      memcpy(&pos, buf+column->length - portable_sizeof_char_ptr,
	     sizeof(char*));
      break;
    }
    case FIELD_VARCHAR:
    {
      uint pack_length= HA_VARCHAR_PACKLENGTH(column->length-1);
      if (pack_length == 1)
        length= (ulong) *(uchar*) buf;
      else
        length= uint2korr(buf);
      pos= buf+pack_length;
      break;
    }
    default:
      length=column->length;
      pos=buf;
      break;
    }
    crc=my_checksum(crc, pos ? pos : (uchar*) "", length);
  }
  return crc;
}


ha_checksum mi_static_checksum(MI_INFO *info, const uchar *pos)
{
  return my_checksum(0, pos, info->s->base.reclength);
}
