/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* Read record based on a key */

/*
 *    HA_READ_KEY_EXACT   => SEARCH_BIGGER
 *    HA_READ_KEY_OR_NEXT => SEARCH_BIGGER
 *    HA_READ_AFTER_KEY   => SEARCH_BIGGER
 *    HA_READ_PREFIX      => SEARCH_BIGGER
 *    HA_READ_KEY_OR_PREV => SEARCH_SMALLER
 *    HA_READ_BEFORE_KEY  => SEARCH_SMALLER
 *    HA_READ_PREFIX_LAST => SEARCH_SMALLER
 */


#include <stddef.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisammrg/myrg_def.h"

/* todo: we could store some additional info to speedup lookups:
         column (key, keyseg) can be constant per table
         it can also be increasing (table1.val > table2.val > ...),
         or decreasing, <=, >=, etc.
                                                                   SerG
*/

int myrg_rkey(MYRG_INFO *info,uchar *buf,int inx, const uchar *key,
            key_part_map keypart_map, enum ha_rkey_function search_flag)
{
  uchar *key_buff= NULL;
  uint pack_key_length= 0;
  uint16 last_used_keyseg= 0;
  MYRG_TABLE *table;
  MI_INFO *mi;
  int err;
  DBUG_ENTER("myrg_rkey");

  if (_myrg_init_queue(info,inx,search_flag))
    DBUG_RETURN(my_errno());

  for (table=info->open_tables ; table != info->end_table ; table++)
  {
    mi=table->table;

    if (table == info->open_tables)
    {
      err=mi_rkey(mi, 0, inx, key, keypart_map, search_flag);
      /* Get the saved packed key and packed key length. */
      key_buff=(uchar*) mi->lastkey+mi->s->base.max_key_length;
      pack_key_length=mi->pack_key_length;
      last_used_keyseg= mi->last_used_keyseg;
    }
    else
    {
      mi->once_flags|= USE_PACKED_KEYS;
      mi->last_used_keyseg= last_used_keyseg;
      err=mi_rkey(mi, 0, inx, key_buff, pack_key_length, search_flag);
    }
    info->last_used_table=table+1;

    if (err)
    {
      if (err == HA_ERR_KEY_NOT_FOUND)
	continue;
      DBUG_PRINT("exit", ("err: %d", err));
      DBUG_RETURN(err);
    }
    /* adding to queue */
    queue_insert(&(info->by_key),(uchar *)table);

  }

  DBUG_PRINT("info", ("tables with matches: %u", info->by_key.elements));
  if (!info->by_key.elements)
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);

  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  mi->once_flags|= RRND_PRESERVE_LASTINX;
  DBUG_PRINT("info", ("using table no: %d",
                      (int) (info->current_table - info->open_tables + 1)));
  DBUG_DUMP("result key", (uchar*) mi->lastkey, mi->lastkey_length);
  DBUG_RETURN(_myrg_mi_read_record(mi,buf));
}
