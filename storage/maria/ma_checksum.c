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

ha_checksum _ma_checksum(MARIA_HA *info, const byte *record)
{
  uint i;
  ha_checksum crc=0;
  MARIA_COLUMNDEF *rec=info->s->rec;

  if (info->s->base.null_bytes)
    crc= my_checksum(crc, record, info->s->base.null_bytes);

  for (i=info->s->base.fields ; i-- ; )
  {
    const byte *pos= record + rec->offset;
    ulong length;

    switch (rec->type) {
    case FIELD_BLOB:
    {
      length= _ma_calc_blob_length(rec->length-
					maria_portable_sizeof_char_ptr,
					pos);
      memcpy((char*) &pos, pos+rec->length- maria_portable_sizeof_char_ptr,
	     sizeof(char*));
      break;
    }
    case FIELD_VARCHAR:
    {
      uint pack_length= HA_VARCHAR_PACKLENGTH(rec->length-1);
      if (pack_length == 1)
        length= (ulong) *(uchar*) pos;
      else
        length= uint2korr(pos);
      pos+= pack_length;
      break;
    }
    default:
      length= rec->length;
      break;
    }
    crc= my_checksum(crc, pos ? pos : "", length);
  }
  return crc;
}


ha_checksum _ma_static_checksum(MARIA_HA *info, const byte *pos)
{
  return my_checksum(0, pos, info->s->base.reclength);
}
