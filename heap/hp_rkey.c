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

#include "heapdef.h"

int heap_rkey(HP_INFO *info, byte *record, int inx, const byte *key, 
              uint key_len, enum ha_rkey_function find_flag)
{
  byte *pos;
  HP_SHARE *share= info->s;
  HP_KEYDEF *keyinfo= share->keydef + inx;
  DBUG_ENTER("heap_rkey");
  DBUG_PRINT("enter",("base: %lx  inx: %d",info,inx));

  if ((uint) inx >= share->keys)
  {
    DBUG_RETURN(my_errno= HA_ERR_WRONG_INDEX);
  }
  info->lastinx= inx;
  info->current_record= (ulong) ~0L;		/* For heap_rrnd() */

  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    heap_rb_param custom_arg;

    custom_arg.keyseg= info->s->keydef[inx].seg;
    custom_arg.key_length= info->lastkey_len= 
      hp_rb_pack_key(keyinfo, (uchar*) info->lastkey,
		     (uchar*) key, key_len);
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
      DBUG_RETURN(my_errno= HA_ERR_KEY_NOT_FOUND);
    }
    memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos), sizeof(byte*));
    info->current_ptr= pos;
  }
  else
  {
    if (!(pos= hp_search(info, share->keydef + inx, key, 0)))
    {
      info->update= 0;
      DBUG_RETURN(my_errno);
    }
    if (!(keyinfo->flag & HA_NOSAME) || (keyinfo->flag & HA_END_SPACE_KEY))
      memcpy(info->lastkey, key, (size_t) keyinfo->length);
  }
  memcpy(record, pos, (size_t) share->reclength);
  info->update= HA_STATE_AKTIV;
  DBUG_RETURN(0);
}


	/* Quick find of record */

gptr heap_find(HP_INFO *info, int inx, const byte *key)
{
  return hp_search(info, info->s->keydef + inx, key, 0);
}
