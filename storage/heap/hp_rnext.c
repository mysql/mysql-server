/* Copyright (C) 2000-2002 MySQL AB

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

#include "heapdef.h"

/* Read next record with the same key */

int heap_rnext(HP_INFO *info, uchar *record)
{
  uchar *pos;
  HP_SHARE *share=info->s;
  HP_KEYDEF *keyinfo;
  DBUG_ENTER("heap_rnext");
  
  if (info->lastinx < 0)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);

  keyinfo = share->keydef + info->lastinx;
  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    heap_rb_param custom_arg;

    if (info->last_pos)
    {
      /*
        We enter this branch for non-DELETE queries after heap_rkey()
        or heap_rfirst(). As last key position (info->last_pos) is available,
        we only need to climb the tree using tree_search_next().
      */
      pos = tree_search_next(&keyinfo->rb_tree, &info->last_pos,
                             offsetof(TREE_ELEMENT, left),
                             offsetof(TREE_ELEMENT, right));
    }
    else if (!info->lastkey_len)
    {
      /*
        We enter this branch only for DELETE queries after heap_rfirst(). E.g.
        DELETE FROM t1 WHERE a<10. As last key position is not available
        (last key is removed by heap_delete()), we must restart search as it
        is done in heap_rfirst().

        It should be safe to handle this situation without this branch. That is
        branch below should find smallest element in a tree as lastkey_len is
        zero. tree_search_edge() is a kind of optimisation here as it should be
        faster than tree_search_key().
      */
      pos= tree_search_edge(&keyinfo->rb_tree, info->parents,
                            &info->last_pos, offsetof(TREE_ELEMENT, left));
    }
    else
    {
      /*
        We enter this branch only for DELETE queries after heap_rkey(). E.g.
        DELETE FROM t1 WHERE a=10. As last key position is not available
        (last key is removed by heap_delete()), we must restart search as it
        is done in heap_rkey().
      */
      custom_arg.keyseg = keyinfo->seg;
      custom_arg.key_length = info->lastkey_len;
      custom_arg.search_flag = SEARCH_SAME | SEARCH_FIND;
      pos = tree_search_key(&keyinfo->rb_tree, info->lastkey, info->parents, 
                           &info->last_pos, info->last_find_flag, &custom_arg);
    }
    if (pos)
    {
      memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos), 
	     sizeof(uchar*));
      info->current_ptr = pos;
    }
    else
    {
      my_errno = HA_ERR_KEY_NOT_FOUND;
    }
  }
  else
  {
    if (info->current_hash_ptr)
      pos= hp_search_next(info, keyinfo, info->lastkey,
			   info->current_hash_ptr);
    else
    {
      if (!info->current_ptr && (info->update & HA_STATE_NEXT_FOUND))
      {
	pos=0;					/* Read next after last */
	my_errno=HA_ERR_KEY_NOT_FOUND;
      }
      else if (!info->current_ptr)		/* Deleted or first call */
	pos= hp_search(info, keyinfo, info->lastkey, 0);
      else
	pos= hp_search(info, keyinfo, info->lastkey, 1);
    }
  }
  if (!pos)
  {
    info->update=HA_STATE_NEXT_FOUND;		/* For heap_rprev */
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno=HA_ERR_END_OF_FILE;
    DBUG_RETURN(my_errno);
  }
  memcpy(record,pos,(size_t) share->reclength);
  info->update=HA_STATE_AKTIV | HA_STATE_NEXT_FOUND;
  DBUG_RETURN(0);
}
