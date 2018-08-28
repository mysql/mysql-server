/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Calculate a checksum for a row */

#include <sys/types.h>

#include "my_byteorder.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"

ha_checksum mi_checksum(MI_INFO *info, const uchar *buf) {
  uint i;
  ha_checksum crc = 0;
  MI_COLUMNDEF *rec = info->s->rec;

  for (i = info->s->base.fields; i--; buf += (rec++)->length) {
    uchar *pos;
    ulong length;
    switch (rec->type) {
      case FIELD_BLOB: {
        length =
            _mi_calc_blob_length(rec->length - portable_sizeof_char_ptr, buf);
        memcpy(&pos, buf + rec->length - portable_sizeof_char_ptr,
               sizeof(char *));
        break;
      }
      case FIELD_VARCHAR: {
        uint pack_length = HA_VARCHAR_PACKLENGTH(rec->length - 1);
        if (pack_length == 1)
          length = (ulong) * (uchar *)buf;
        else
          length = uint2korr(buf);
        pos = (uchar *)buf + pack_length;
        break;
      }
      default:
        length = rec->length;
        pos = (uchar *)buf;
        break;
    }
    crc = my_checksum(crc, pos ? pos : (uchar *)"", length);
  }
  return crc;
}

ha_checksum mi_static_checksum(MI_INFO *info, const uchar *pos) {
  return my_checksum(0, pos, info->s->base.reclength);
}
