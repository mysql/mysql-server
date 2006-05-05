/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Remove all rows from a MARIA table */
/* This clears the status information and truncates files */

#include "maria_def.h"

int maria_delete_all_rows(MARIA_HA *info)
{
  uint i;
  MARIA_SHARE *share=info->s;
  MARIA_STATE_INFO *state=&share->state;
  DBUG_ENTER("maria_delete_all_rows");

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    DBUG_RETURN(my_errno=EACCES);
  }
  if (_ma_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);
  if (_ma_mark_file_changed(info))
    goto err;

  info->state->records=info->state->del=state->split=0;
  state->dellink = HA_OFFSET_ERROR;
  state->sortkey=  (ushort) ~0;
  info->state->key_file_length=share->base.keystart;
  info->state->data_file_length=0;
  info->state->empty=info->state->key_empty=0;
  info->state->checksum=0;

  for (i=share->base.max_key_block_length/MARIA_MIN_KEY_BLOCK_LENGTH ; i-- ; )
    state->key_del[i]= HA_OFFSET_ERROR;
  for (i=0 ; i < share->base.keys ; i++)
    state->key_root[i]= HA_OFFSET_ERROR;

  /*
    If we are using delayed keys or if the user has done changes to the tables
    since it was locked then there may be key blocks in the key cache
  */
  flush_key_blocks(share->key_cache, share->kfile, FLUSH_IGNORE_CHANGED);
  if (my_chsize(info->dfile, 0, 0, MYF(MY_WME)) ||
      my_chsize(share->kfile, share->base.keystart, 0, MYF(MY_WME))  )
    goto err;
  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
#ifdef HAVE_MMAP
  /* Resize mmaped area */
  rw_wrlock(&info->s->mmap_lock);
  _ma_remap_file(info, (my_off_t)0);
  rw_unlock(&info->s->mmap_lock);
#endif
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  {
    int save_errno=my_errno;
    VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
    info->update|=HA_STATE_WRITTEN;	/* Buffer changed */
    allow_break();			/* Allow SIGHUP & SIGINT */
    DBUG_RETURN(my_errno=save_errno);
  }
} /* maria_delete */
