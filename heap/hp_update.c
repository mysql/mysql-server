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

/* Update current record in heap-database */

#include "heapdef.h"

int heap_update(HP_INFO *info, const byte *old, const byte *heap_new)
{
  HP_KEYDEF *keydef, *end, *p_lastinx;
  byte *pos;
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_update");

  test_active(info);
  pos=info->current_ptr;

  if (info->opt_flag & READ_CHECK_USED && hp_rectest(info,old))
    DBUG_RETURN(my_errno);				/* Record changed */
  if (--(share->records) < share->blength >> 1) share->blength>>= 1;
  share->changed=1;

  p_lastinx = share->keydef + info->lastinx;
  for (keydef = share->keydef, end = keydef + share->keys; keydef < end; 
       keydef++)
  {
    if (hp_rec_key_cmp(keydef, old, heap_new))
    {
      if ((*keydef->delete_key)(info, keydef, old, pos, keydef == p_lastinx) ||
          (*keydef->write_key)(info, keydef, heap_new, pos))
        goto err;
    }
  }

  memcpy(pos,heap_new,(size_t) share->reclength);
  if (++(share->records) == share->blength) share->blength+= share->blength;
  DBUG_RETURN(0);

 err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey = keydef - share->keydef;
    if (keydef->algorithm == HA_KEY_ALG_BTREE)
    {
      /* we don't need to delete non-inserted key from rb-tree */
      if ((*keydef->write_key)(info, keydef, old, pos))
      {
        if (++(share->records) == share->blength) share->blength+= share->blength;
        DBUG_RETURN(my_errno);
      }
      keydef--;
    }
    while (keydef >= share->keydef)
    {
      if (hp_rec_key_cmp(keydef, old, heap_new))
      {
	if ((*keydef->delete_key)(info, keydef, heap_new, pos, 0) ||
	    (*keydef->write_key)(info, keydef, old, pos))
	  break;
      }
      keydef--;
    }
  }
  if (++(share->records) == share->blength) share->blength+= share->blength;
  DBUG_RETURN(my_errno);
} /* heap_update */
