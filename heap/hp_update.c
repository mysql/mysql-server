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

int heap_update(HP_INFO *info, const byte *old, const byte *new)
{
  uint key;
  byte *pos;
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_update");

  test_active(info);
  pos=info->current_ptr;

  if (info->opt_flag & READ_CHECK_USED && _hp_rectest(info,old))
    DBUG_RETURN(my_errno);				/* Record changed */
  if (--(share->records) < share->blength >> 1) share->blength>>= 1;
  share->changed=1;

  for (key=0 ; key < share->keys ; key++)
  {
    if (_hp_rec_key_cmp(share->keydef+key,old,new))
    {
      if (_hp_delete_key(info,share->keydef+key,old,pos,key ==
			 (uint) info->lastinx) ||
	  _hp_write_key(share,share->keydef+key,new,pos))
	goto err;
    }
  }

  memcpy(pos,new,(size_t) share->reclength);
  if (++(share->records) == share->blength) share->blength+= share->blength;
  DBUG_RETURN(0);

 err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey=key;
    do
    {
      if (_hp_rec_key_cmp(share->keydef+key,old,new))
      {
	if (_hp_delete_key(info,share->keydef+key,new,pos,0) ||
	    _hp_write_key(share,share->keydef+key,old,pos))
	  break;
      }
    } while (key-- > 0);
  }
  if (++(share->records) == share->blength) share->blength+= share->blength;
  DBUG_RETURN(my_errno);
} /* heap_update */
