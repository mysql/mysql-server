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
#include "rt_index.h"

	/* Read a record using key */
	/* Ordinary search_flag is 0 ; Give error if no record with key */

int mi_rkey(MI_INFO *info, byte *buf, int inx, const byte *key, uint key_len,
	    enum ha_rkey_function search_flag)
{
  uchar *key_buff;
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *keyinfo;
  HA_KEYSEG *last_used_keyseg;
  uint pack_key_length, use_key_length, nextflag;
  DBUG_ENTER("mi_rkey");
  DBUG_PRINT("enter", ("base: %lx  buf: %lx  inx: %d  search_flag: %d",
                       (long) info, (long) buf, inx, search_flag));

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->last_key_func= search_flag;
  keyinfo= share->keyinfo + inx;

  if (info->once_flags & USE_PACKED_KEYS)
  {
    info->once_flags&= ~USE_PACKED_KEYS;	/* Reset flag */
    /*
      key is already packed!;  This happens when we are using a MERGE TABLE
    */
    key_buff=info->lastkey+info->s->base.max_key_length;
    pack_key_length= key_len;
    bmove(key_buff,key,key_len);
    last_used_keyseg= 0;
  }
  else
  {
    if (key_len == 0)
      key_len=USE_WHOLE_KEY;
    /* Save the packed key for later use in the second buffer of lastkey. */
    key_buff=info->lastkey+info->s->base.max_key_length;
    pack_key_length=_mi_pack_key(info,(uint) inx, key_buff, (uchar*) key,
				 key_len, &last_used_keyseg);
    /* Save packed_key_length for use by the MERGE engine. */
    info->pack_key_length= pack_key_length;
    DBUG_EXECUTE("key",_mi_print_key(DBUG_FILE, keyinfo->seg,
				     key_buff, pack_key_length););
  }

  if (fast_mi_readinfo(info))
    goto err;
  if (share->concurrent_insert)
    rw_rdlock(&share->key_root_lock[inx]);

  nextflag=myisam_read_vec[search_flag];
  use_key_length=pack_key_length;
  if (!(nextflag & (SEARCH_FIND | SEARCH_NO_FIND | SEARCH_LAST)))
    use_key_length=USE_WHOLE_KEY;

  switch (info->s->keyinfo[inx].key_alg) {
#ifdef HAVE_RTREE_KEYS
  case HA_KEY_ALG_RTREE:
    if (rtree_find_first(info,inx,key_buff,use_key_length,nextflag) < 0)
    {
      mi_print_error(info->s, HA_ERR_CRASHED);
      my_errno=HA_ERR_CRASHED;
      goto err;
    }
    break;
#endif
  case HA_KEY_ALG_BTREE:
  default:
    if (!_mi_search(info, keyinfo, key_buff, use_key_length,
		  myisam_read_vec[search_flag], info->s->state.key_root[inx]))
    {
      while (info->lastpos >= info->state->data_file_length)
      {
        /*
	  Skip rows that are inserted by other threads since we got a lock
	  Note that this can only happen if we are not searching after an
	  exact key, because the keys are sorted according to position
        */

        if  (_mi_search_next(info, keyinfo, info->lastkey,
			   info->lastkey_length,
			   myisam_readnext_vec[search_flag],
			   info->s->state.key_root[inx]))
	  break;
      }
    }
  }
  if (share->concurrent_insert)
    rw_unlock(&share->key_root_lock[inx]);

  /* Calculate length of the found key;  Used by mi_rnext_same */
  if ((keyinfo->flag & HA_VAR_LENGTH_KEY) && last_used_keyseg &&
      info->lastpos != HA_OFFSET_ERROR)
    info->last_rkey_length= _mi_keylength_part(keyinfo, info->lastkey,
					       last_used_keyseg);
  else
    info->last_rkey_length= pack_key_length;

  /* Check if we don't want to have record back, only error message */
  if (!buf)
    DBUG_RETURN(info->lastpos == HA_OFFSET_ERROR ? my_errno : 0);

  if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }

  info->lastpos = HA_OFFSET_ERROR;		/* Didn't find key */

  /* Store last used key as a base for read next */
  memcpy(info->lastkey,key_buff,pack_key_length);
  info->last_rkey_length= pack_key_length;
  bzero((char*) info->lastkey+pack_key_length,info->s->base.rec_reflength);
  info->lastkey_length=pack_key_length+info->s->base.rec_reflength;

  if (search_flag == HA_READ_AFTER_KEY)
    info->update|=HA_STATE_NEXT_FOUND;		/* Previous gives last row */
err:
  DBUG_RETURN(my_errno);
} /* _mi_rkey */
