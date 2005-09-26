/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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


#include "myrg_def.h"

/* todo: we could store some additional info to speedup lookups:
         column (key, keyseg) can be constant per table
         it can also be increasing (table1.val > table2.val > ...),
         or decreasing, <=, >=, etc.
                                                                   SerG
*/

int myrg_rkey(MYRG_INFO *info,byte *buf,int inx, const byte *key,
            uint key_len, enum ha_rkey_function search_flag)
{
  byte *key_buff;
  uint pack_key_length;
  MYRG_TABLE *table;
  MI_INFO *mi;
  int err;
  DBUG_ENTER("myrg_rkey");
  LINT_INIT(key_buff);
  LINT_INIT(pack_key_length);

  if (_myrg_init_queue(info,inx,search_flag))
    DBUG_RETURN(my_errno);

  for (table=info->open_tables ; table != info->end_table ; table++)
  {
    mi=table->table;

    if (table == info->open_tables)
    {
      err=mi_rkey(mi,0,inx,key,key_len,search_flag);
      /* Get the saved packed key and packed key length. */
      key_buff=(byte*) mi->lastkey+mi->s->base.max_key_length;
      pack_key_length=mi->pack_key_length;
    }
    else
    {
      mi->once_flags|= USE_PACKED_KEYS;
      err=mi_rkey(mi,0,inx,key_buff,pack_key_length,search_flag);
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
    queue_insert(&(info->by_key),(byte *)table);

  }

  DBUG_PRINT("info", ("tables with matches: %u", info->by_key.elements));
  if (!info->by_key.elements)
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);

  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  mi->once_flags|= RRND_PRESERVE_LASTINX;
  DBUG_PRINT("info", ("using table no: %d",
                      info->current_table - info->open_tables + 1));
  DBUG_DUMP("result key", (byte*) mi->lastkey, mi->lastkey_length);
  DBUG_RETURN(_myrg_mi_read_record(mi,buf));
}
