/* Copyright (c) 2000-2002, 2005-2007 MySQL AB
   Use is subject to license terms

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

#include "heapdef.h"

	/* Read prev record for key */


int heap_rprev(HP_INFO *info, uchar *record)
{
  uchar *pos;
  HP_SHARE *share=info->s;
  HP_KEYDEF *keyinfo;
  DBUG_ENTER("heap_rprev");

  if (info->lastinx < 0)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  keyinfo = share->keydef + info->lastinx;
  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    heap_rb_param custom_arg;

    /* If no active record and last was not deleted */
    if (!(info->update & (HA_STATE_AKTIV | HA_STATE_NO_KEY |
                          HA_STATE_DELETED)))
    {
      if (info->update & HA_STATE_PREV_FOUND)
        pos= 0;                            /* Can't search before first row */
      else
      {
        /* Last was 'next' after last record; search after last record */
        pos= tree_search_edge(&keyinfo->rb_tree, info->parents,
                              &info->last_pos, offsetof(TREE_ELEMENT, right));
      }
    }
    else if (info->last_pos)
      pos = tree_search_next(&keyinfo->rb_tree, &info->last_pos,
                             offsetof(TREE_ELEMENT, right),
                             offsetof(TREE_ELEMENT, left));
    else
    {
      custom_arg.keyseg = keyinfo->seg;
      custom_arg.key_length = keyinfo->length;
      custom_arg.search_flag = SEARCH_SAME;
      info->last_find_flag= HA_READ_KEY_OR_PREV;
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
    if (info->current_ptr || (info->update & HA_STATE_NEXT_FOUND))
    {
      if ((info->update & HA_STATE_DELETED))
        pos= hp_search(info, share->keydef + info->lastinx, info->lastkey, 3);
      else
        pos= hp_search(info, share->keydef + info->lastinx, info->lastkey, 2);
    }
    else
    {
      pos=0;					/* Read next after last */
      my_errno=HA_ERR_KEY_NOT_FOUND;
    }
  }
  if (!pos)
  {
    info->update=HA_STATE_PREV_FOUND;		/* For heap_rprev */
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno=HA_ERR_END_OF_FILE;
    DBUG_RETURN(my_errno);
  }
  memcpy(record,pos,(size_t) share->reclength);
  info->update=HA_STATE_AKTIV | HA_STATE_PREV_FOUND;
  DBUG_RETURN(0);
}
