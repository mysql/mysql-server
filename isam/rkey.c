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

/* L{ser p} basen av en isam_nyckel */

#include "isamdef.h"


	/* Read a record using key */
	/* Ordinary search_flag is 0 ; Give error if no record with key */

int nisam_rkey(N_INFO *info, byte *buf, int inx, const byte *key, uint key_len, enum ha_rkey_function search_flag)
{
  uchar *key_buff;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("nisam_rkey");
  DBUG_PRINT("enter",("base: %lx  inx: %d  search_flag: %d",
		      info,inx,search_flag));

  if ((inx = _nisam_check_index(info,inx)) < 0)
    DBUG_RETURN(-1);
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  if (key_len >= (share->keyinfo[inx].base.keylength - share->rec_reflength)
      && !(info->s->keyinfo[inx].base.flag & HA_SPACE_PACK_USED))
    key_len=USE_HOLE_KEY;
  key_buff=info->lastkey+info->s->base.max_key_length;
  key_len=_nisam_pack_key(info,(uint) inx,key_buff,(uchar*) key,key_len);
  DBUG_EXECUTE("key",_nisam_print_key(DBUG_FILE,share->keyinfo[inx].seg,
				    (uchar*) key););

#ifndef NO_LOCKING
  if (_nisam_readinfo(info,F_RDLCK,1))
    goto err;
#endif

  VOID(_nisam_search(info,info->s->keyinfo+inx,key_buff,key_len,
		  nisam_read_vec[search_flag],info->s->state.key_root[inx]));
  if ((*info->read_record)(info,info->lastpos,buf) >= 0)
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }

  info->lastpos = NI_POS_ERROR;			/* Didn't find key */
  VOID(_nisam_move_key(info->s->keyinfo+inx,info->lastkey,key_buff));
  if (search_flag == HA_READ_AFTER_KEY)
    info->update|=HA_STATE_NEXT_FOUND;		/* Previous gives last row */
err:
  DBUG_RETURN(-1);
} /* nisam_rkey */
