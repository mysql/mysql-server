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

/* Ger tillbaka en struct med information om isam-filen */

#include "myisamdef.h"
#ifdef	__WIN__
#include <sys/stat.h>
#endif

	/* Get position to last record */

my_off_t mi_position(MI_INFO *info)
{
  return info->lastpos;
}


/* Get information about the table */
/* if flag == 2 one get current info (no sync from database */

int mi_status(MI_INFO *info, register MI_ISAMINFO *x, uint flag)
{
  MY_STAT state;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_status");

  x->recpos  = info->lastpos;
  if (flag == HA_STATUS_POS)
    DBUG_RETURN(0);				/* Compatible with ISAM */
  if (!(flag & HA_STATUS_NO_LOCK))
  {
    pthread_mutex_lock(&share->intern_lock);
    VOID(_mi_readinfo(info,F_RDLCK,0));
    fast_mi_writeinfo(info);
    pthread_mutex_unlock(&share->intern_lock);
  }
  if (flag & HA_STATUS_VARIABLE)
  {
    x->records	 	= info->state->records;
    x->deleted	 	= info->state->del;
    x->delete_length	= info->state->empty;
    x->data_file_length	=info->state->data_file_length;
    x->index_file_length=info->state->key_file_length;

    x->keys	 	= share->state.header.keys;
    x->check_time	= share->state.check_time;
    x->mean_reclength	= info->state->records ?
      (ulong) ((info->state->data_file_length-info->state->empty)/
	       info->state->records) : (ulong) share->min_pack_length;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    x->errkey	 = info->errkey;
    x->dupp_key_pos= info->dupp_key_pos;
  }
  if (flag & HA_STATUS_CONST)
  {
    x->reclength	= share->base.reclength;
    x->max_data_file_length=share->base.max_data_file_length;
    x->max_index_file_length=info->s->base.max_key_file_length;
    x->filenr	 = info->dfile;
    x->options	 = share->options;
    x->create_time=share->state.create_time;
    x->reflength= mi_get_pointer_length(share->base.max_data_file_length,4);
    x->record_offset= ((share->options &
			(HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ?
		       0L : share->base.pack_reclength);
    x->sortkey= -1;				/* No clustering */
    /* The following should be included even if we are not compiling with
       USE_RAID as the client must be able to request it! */
    x->rec_per_key	= share->state.rec_per_key_part;
    x->raid_type= share->base.raid_type;
    x->raid_chunks= share->base.raid_chunks;
    x->raid_chunksize= share->base.raid_chunksize;
    x->key_map	 	= share->state.key_map;
    x->data_file_name   = share->data_file_name;
    x->index_file_name  = share->index_file_name;
  }
  if ((flag & HA_STATUS_TIME) && !my_fstat(info->dfile,&state,MYF(0)))
    x->update_time=state.st_mtime;
  else
    x->update_time=0;
  if (flag & HA_STATUS_AUTO)
  {
    x->auto_increment= share->state.auto_increment+1;
    if (!x->auto_increment)			/* This shouldn't happen */
      x->auto_increment= ~(ulonglong) 0;
  }
  DBUG_RETURN(0);
}
