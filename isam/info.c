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

#include "isamdef.h"
#ifdef	__WIN__
#include <sys/stat.h>
#endif

ulong nisam_position(N_INFO *info)
{
  return info->lastpos;
}

	/* If flag == 1 one only gets pos of last record */
	/* if flag == 2 one get current info (no sync from database */

int nisam_info(N_INFO *info, register N_ISAMINFO *x, int flag)
{
  struct stat state;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("nisam_info");

  x->recpos  = info->lastpos;
  if (flag & (HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE |
	      HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK))
  {
#ifndef NO_LOCKING
    if (!(flag & HA_STATUS_NO_LOCK))
    {
      pthread_mutex_lock(&share->intern_lock);
      VOID(_nisam_readinfo(info,F_RDLCK,0));
      VOID(_nisam_writeinfo(info,0));
      pthread_mutex_unlock(&share->intern_lock);
    }
#endif
    x->records	 = share->state.records;
    x->deleted	 = share->state.del;
    x->delete_length= share->state.empty;
    x->keys	 = share->state.keys;
    x->reclength = share->base.reclength;
    x->mean_reclength= share->state.records ?
      (share->state.data_file_length-share->state.empty)/share->state.records :
      share->min_pack_length;
    x->data_file_length=share->state.data_file_length;
    x->max_data_file_length=share->base.max_data_file_length;
    x->index_file_length=share->state.key_file_length;
    x->max_index_file_length=share->base.max_key_file_length;
    x->filenr	 = info->dfile;
    x->errkey	 = info->errkey;
    x->dupp_key_pos= info->dupp_key_pos;
    x->options	 = share->base.options;
    x->create_time=share->base.create_time;
    x->isamchk_time=share->base.isamchk_time;
    x->rec_per_key=share->base.rec_per_key;
    if ((flag & HA_STATUS_TIME) && !fstat(info->dfile,&state))
      x->update_time=state.st_mtime;
    else
      x->update_time=0;
    x->sortkey= -1;				/* No clustering */
  }
  DBUG_RETURN(0);
} /* nisam_info */
