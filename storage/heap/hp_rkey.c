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

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

int heap_rkey(HP_INFO *info, uchar *record, int inx, const uchar *key, 
              key_part_map keypart_map, enum ha_rkey_function find_flag)
{
  uchar *pos;
  HP_SHARE *share= info->s;
  HP_KEYDEF *keyinfo= share->keydef + inx;
  DBUG_ENTER("heap_rkey");
  DBUG_PRINT("enter",("info: %p  inx: %d", info, inx));

  if ((uint) inx >= share->keys)
  {
    set_my_errno(HA_ERR_WRONG_INDEX);
    DBUG_RETURN(HA_ERR_WRONG_INDEX);
  }
  info->lastinx= inx;
  info->current_record= (ulong) ~0L;		/* For heap_rrnd() */

  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    heap_rb_param custom_arg;

    custom_arg.keyseg= info->s->keydef[inx].seg;
    custom_arg.key_length= info->lastkey_len= 
      hp_rb_pack_key(keyinfo, (uchar*) info->lastkey,
		     (uchar*) key, keypart_map);
    custom_arg.search_flag= SEARCH_FIND | SEARCH_SAME;
    /* for next rkey() after deletion */
    if (find_flag == HA_READ_AFTER_KEY)
      info->last_find_flag= HA_READ_KEY_OR_NEXT;
    else if (find_flag == HA_READ_BEFORE_KEY)
      info->last_find_flag= HA_READ_KEY_OR_PREV;
    else
      info->last_find_flag= find_flag;
    if (!(pos= tree_search_key(&keyinfo->rb_tree, info->lastkey, info->parents,
			       &info->last_pos, find_flag, &custom_arg)))
    {
      info->update= 0;
      set_my_errno(HA_ERR_KEY_NOT_FOUND);
      DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);
    }
    memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos), sizeof(uchar*));
    info->current_ptr= pos;
  }
  else
  {
    if (!(pos= hp_search(info, share->keydef + inx, key, 0)))
    {
      info->update= 0;
      DBUG_RETURN(my_errno());
    }
    /*
      If key is unique and can accept NULL values, we still
      need to copy it to info->lastkey, which in turn is used
      to search subsequent records.
    */
    if (!(keyinfo->flag & HA_NOSAME) || (keyinfo->flag & HA_NULL_PART_KEY))
      memcpy(info->lastkey, key, (size_t) keyinfo->length);
  }
  memcpy(record, pos, (size_t) share->reclength);
  info->update= HA_STATE_AKTIV;
  DBUG_RETURN(0);
}


	/* Quick find of record */

uchar* heap_find(HP_INFO *info, int inx, const uchar *key)
{
  return hp_search(info, info->s->keydef + inx, key, 0);
}
