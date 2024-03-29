/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

/* Read a record with random-access. The position to the record must
   get by MI_INFO. The next record can be read with pos= MI_POS_ERROR */

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"

/*
           Read a row based on position.
           If filepos= HA_OFFSET_ERROR then read next row
           Return values
           Returns one of following values:
           0 = Ok.
           HA_ERR_RECORD_DELETED = Record is deleted.
           HA_ERR_END_OF_FILE = EOF.
*/

int mi_rrnd(MI_INFO *info, uchar *buf, my_off_t filepos) {
  bool skip_deleted_blocks;
  DBUG_TRACE;

  skip_deleted_blocks = false;

  if (filepos == HA_OFFSET_ERROR) {
    skip_deleted_blocks = true;
    if (info->lastpos == HA_OFFSET_ERROR)    /* First read ? */
      filepos = info->s->pack.header_length; /* Read first record */
    else
      filepos = info->nextpos;
  }

  if (info->once_flags & RRND_PRESERVE_LASTINX)
    info->once_flags &= ~RRND_PRESERVE_LASTINX;
  else
    info->lastinx = -1; /* Can't forward or backward */
  /* Init all but update-flag */
  info->update &= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    return my_errno();

  return (*info->s->read_rnd)(info, buf, filepos, skip_deleted_blocks);
}
