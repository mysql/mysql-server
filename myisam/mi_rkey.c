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

#include "myisamdef.h"


	/* Read a record using key */
	/* Ordinary search_flag is 0 ; Give error if no record with key */

int mi_rkey(MI_INFO *info, byte *buf, int inx, const byte *key, uint key_len,
	     enum ha_rkey_function search_flag)
{
  uchar *key_buff;
  MYISAM_SHARE *share=info->s;
  uint pack_key_length;
  DBUG_ENTER("mi_rkey");
  DBUG_PRINT("enter",("base: %lx  inx: %d  search_flag: %d",
		      info,inx,search_flag));

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  if (!info->use_packed_key)
  {
    if (key_len == 0)
      key_len=USE_WHOLE_KEY;
    key_buff=info->lastkey+info->s->base.max_key_length;
    pack_key_length=_mi_pack_key(info,(uint) inx,key_buff,(uchar*) key,key_len);
    info->last_rkey_length=pack_key_length;
    DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE,share->keyinfo[inx].seg,
				     key_buff,pack_key_length););
  }
  else
  {
    /* key is already packed! */
    key_buff=info->lastkey+info->s->base.max_key_length;
    info->last_rkey_length=pack_key_length=key_len;
    bmove(key_buff,key,key_len);
  }

  if (_mi_readinfo(info,F_RDLCK,1))
    goto err;
  if (share->concurrent_insert)
    rw_rdlock(&share->key_root_lock[inx]);
  if (!_mi_search(info,info->s->keyinfo+inx,key_buff,pack_key_length,
		  myisam_read_vec[search_flag],info->s->state.key_root[inx]))
  {
    while (info->lastpos >= info->state->data_file_length)
    {
      /*
	Skip rows that are inserted by other threads since we got a lock
	Note that this can only happen if we are not searching after an
	exact key, because the keys are sorted according to position
      */

      if  (_mi_search_next(info,info->s->keyinfo+inx,info->lastkey,
			   info->lastkey_length,
			   myisam_readnext_vec[search_flag],
			   info->s->state.key_root[inx]))
	break;
    }
  }
  if (share->concurrent_insert)
    rw_unlock(&share->key_root_lock[inx]);

  if (!buf)
    DBUG_RETURN(info->lastpos==HA_OFFSET_ERROR ? my_errno : 0);

  if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }

  info->lastpos = HA_OFFSET_ERROR;		/* Didn't find key */

  /* Store key for read next */
  memcpy(info->lastkey,key_buff,pack_key_length);
  bzero((char*) info->lastkey+pack_key_length,info->s->base.rec_reflength);
  info->lastkey_length=pack_key_length+info->s->base.rec_reflength;

  if (search_flag == HA_READ_AFTER_KEY)
    info->update|=HA_STATE_NEXT_FOUND;		/* Previous gives last row */
err:
  DBUG_RETURN(my_errno);
} /* _mi_rkey */
