/* Copyright (c) 2000-2002, 2004-2007 MySQL AB
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

/* Read first record with the current key */

int heap_rfirst(HP_INFO *info, uchar *record, int inx)
{
  HP_SHARE *share = info->s;
  HP_KEYDEF *keyinfo = share->keydef + inx;
  
  DBUG_ENTER("heap_rfirst");
  info->lastinx= inx;
  info->key_version= info->s->key_version;

  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
  {
    uchar *pos;

    if ((pos = tree_search_edge(&keyinfo->rb_tree, info->parents,
                                &info->last_pos, offsetof(TREE_ELEMENT, left))))
    {
      memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos), 
	     sizeof(uchar*));
      info->current_ptr = pos;
      memcpy(record, pos, (size_t)share->reclength);
      /*
        If we're performing index_first on a table that was taken from
        table cache, info->lastkey_len is initialized to previous query.
        Thus we set info->lastkey_len to proper value for subsequent
        heap_rnext() calls.
        This is needed for DELETE queries only, otherwise this variable is
        not used.
        Note that the same workaround may be needed for heap_rlast(), but
        for now heap_rlast() is never used for DELETE queries.
      */
      info->lastkey_len= 0;
      info->update = HA_STATE_AKTIV;
    }
    else
    {
      info->update= HA_STATE_NO_KEY;
      my_errno = HA_ERR_END_OF_FILE;
      DBUG_RETURN(my_errno);
    }
    DBUG_RETURN(0);
  }
  else
  {
    /* We can't scan a non existing key value with hash index */
    my_errno= HA_ERR_WRONG_COMMAND;
    DBUG_RETURN(my_errno);
  }
}
