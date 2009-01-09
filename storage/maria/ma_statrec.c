/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

	/* Functions to handle fixed-length-records */

#include "maria_def.h"


my_bool _ma_write_static_record(MARIA_HA *info, const uchar *record)
{
  uchar temp[8];                                 /* max pointer length */
  if (info->s->state.dellink != HA_OFFSET_ERROR &&
      !info->append_insert_at_end)
  {
    my_off_t filepos=info->s->state.dellink;
    info->rec_cache.seek_not_done=1;		/* We have done a seek */
    if (info->s->file_read(info, &temp[0],info->s->base.rec_reflength,
		info->s->state.dellink+1,
		 MYF(MY_NABP)))
      goto err;
    info->s->state.dellink= _ma_rec_pos(info->s, temp);
    info->state->del--;
    info->state->empty-=info->s->base.pack_reclength;
    if (info->s->file_write(info, record, info->s->base.reclength,
                            filepos, MYF(MY_NABP)))
      goto err;
  }
  else
  {
    if (info->state->data_file_length > info->s->base.max_data_file_length-
	info->s->base.pack_reclength)
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      return(2);
    }
    if (info->opt_flag & WRITE_CACHE_USED)
    {				/* Cash in use */
      if (my_b_write(&info->rec_cache, record,
		     info->s->base.reclength))
	goto err;
      if (info->s->base.pack_reclength != info->s->base.reclength)
      {
	uint length=info->s->base.pack_reclength - info->s->base.reclength;
	bzero(temp,length);
	if (my_b_write(&info->rec_cache, temp,length))
	  goto err;
      }
    }
    else
    {
      info->rec_cache.seek_not_done=1;		/* We have done a seek */
      if (info->s->file_write(info, record, info->s->base.reclength,
                              info->state->data_file_length,
                              info->s->write_flag))
        goto err;
      if (info->s->base.pack_reclength != info->s->base.reclength)
      {
	uint length=info->s->base.pack_reclength - info->s->base.reclength;
	bzero(temp,length);
	if (info->s->file_write(info, temp,length,
		      info->state->data_file_length+
		      info->s->base.reclength,
		      info->s->write_flag))
    goto err;
      }
    }
    info->state->data_file_length+=info->s->base.pack_reclength;
    info->s->state.split++;
  }
  return 0;
 err:
  return 1;
}

my_bool _ma_update_static_record(MARIA_HA *info, MARIA_RECORD_POS pos,
                                 const uchar *oldrec __attribute__ ((unused)),
                                 const uchar *record)
{
  info->rec_cache.seek_not_done=1;		/* We have done a seek */
  return (info->s->file_write(info,
                              record, info->s->base.reclength,
		    pos,
		    MYF(MY_NABP)) != 0);
}


my_bool _ma_delete_static_record(MARIA_HA *info,
                                 const uchar *record __attribute__ ((unused)))
{
  uchar temp[9];                                 /* 1+sizeof(uint32) */
  info->state->del++;
  info->state->empty+=info->s->base.pack_reclength;
  temp[0]= '\0';			/* Mark that record is deleted */
  _ma_dpointer(info->s, temp+1, info->s->state.dellink);
  info->s->state.dellink= info->cur_row.lastpos;
  info->rec_cache.seek_not_done=1;
  return (info->s->file_write(info, temp, 1+info->s->rec_reflength,
		    info->cur_row.lastpos, MYF(MY_NABP)) != 0);
}


my_bool _ma_cmp_static_record(register MARIA_HA *info,
                              register const uchar *old)
{
  DBUG_ENTER("_ma_cmp_static_record");

  /* We are going to do changes; dont let anybody disturb */
  dont_break();				/* Dont allow SIGHUP or SIGINT */

  if (info->opt_flag & WRITE_CACHE_USED)
  {
    if (flush_io_cache(&info->rec_cache))
    {
      DBUG_RETURN(1);
    }
    info->rec_cache.seek_not_done=1;		/* We have done a seek */
  }

  if ((info->opt_flag & READ_CHECK_USED))
  {						/* If check isn't disabled  */
    info->rec_cache.seek_not_done=1;		/* We have done a seek */
    if (info->s->file_read(info, info->rec_buff, info->s->base.reclength,
                           info->cur_row.lastpos, MYF(MY_NABP)))
      DBUG_RETURN(1);
    if (memcmp(info->rec_buff, old, (uint) info->s->base.reclength))
    {
      DBUG_DUMP("read",old,info->s->base.reclength);
      DBUG_DUMP("disk",info->rec_buff,info->s->base.reclength);
      my_errno=HA_ERR_RECORD_CHANGED;		/* Record have changed */
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


my_bool _ma_cmp_static_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                              const uchar *record, MARIA_RECORD_POS pos)
{
  DBUG_ENTER("_ma_cmp_static_unique");

  info->rec_cache.seek_not_done=1;		/* We have done a seek */
  if (info->s->file_read(info, info->rec_buff, info->s->base.reclength,
	       pos, MYF(MY_NABP)))
    DBUG_RETURN(1);
  DBUG_RETURN(_ma_unique_comp(def, record, info->rec_buff,
                              def->null_are_equal));
}


/*
  Read a fixed-length-record

  RETURN
    0  Ok
    1  record delete
    -1 on read-error or locking-error
*/

int _ma_read_static_record(register MARIA_HA *info, register uchar *record,
                           MARIA_RECORD_POS pos)
{
  int error;
  DBUG_ENTER("_ma_read_static_record");

  if (pos != HA_OFFSET_ERROR)
  {
    if (info->opt_flag & WRITE_CACHE_USED &&
	info->rec_cache.pos_in_file <= pos &&
	flush_io_cache(&info->rec_cache))
      DBUG_RETURN(my_errno);
    info->rec_cache.seek_not_done=1;		/* We have done a seek */

    error= (int) info->s->file_read(info, record,info->s->base.reclength,
                                    pos, MYF(MY_NABP));
    if (! error)
    {
      fast_ma_writeinfo(info);
      if (!*record)
      {
        /* Record is deleted */
        DBUG_PRINT("warning", ("Record is deleted"));
	DBUG_RETURN((my_errno=HA_ERR_RECORD_DELETED));
      }
      info->update|= HA_STATE_AKTIV;		/* Record is read */
      DBUG_RETURN(0);
    }
  }
  fast_ma_writeinfo(info);			/* No such record */
  DBUG_RETURN(my_errno);
}


/**
   @brief  Read record from given position or next record

   @note
     When scanning, this function will return HA_ERR_RECORD_DELETED
     for deleted rows even if skip_deleted_blocks is set.
     The reason for this is to allow the caller to calculate the record
     position without having to do call maria_position() for each record.
*/

int _ma_read_rnd_static_record(MARIA_HA *info, uchar *buf,
                               MARIA_RECORD_POS filepos,
                               my_bool skip_deleted_blocks)
{
  int locked,error,cache_read;
  uint cache_length;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_read_rnd_static_record");

  cache_read=0;
  cache_length=0;
  if (info->opt_flag & READ_CACHE_USED)
  {						/* Cache in use */
    if (filepos == my_b_tell(&info->rec_cache) &&
	(skip_deleted_blocks || !filepos))
    {
      cache_read=1;				/* Read record using cache */
      cache_length= (uint) (info->rec_cache.read_end -
                            info->rec_cache.read_pos);
    }
    else
      info->rec_cache.seek_not_done=1;		/* Filepos is changed */
  }
  locked=0;
  if (info->lock_type == F_UNLCK)
  {
    if (filepos >= info->state->data_file_length)
    {						/* Test if new records */
      if (_ma_readinfo(info,F_RDLCK,0))
	DBUG_RETURN(my_errno);
      locked=1;
    }
    else
    {						/* We don't nead new info */
#ifndef UNSAFE_LOCKING
      if ((! cache_read || share->base.reclength > cache_length) &&
	  share->tot_locks == 0)
      {						/* record not in cache */
	locked=1;
      }
#else
      info->tmp_lock_type=F_RDLCK;
#endif
    }
  }
  if (filepos >= info->state->data_file_length)
  {
    DBUG_PRINT("test",("filepos: %ld (%ld)  records: %ld  del: %ld",
		       (long) filepos/share->base.reclength, (long) filepos,
		       (long) info->state->records, (long) info->state->del));
    fast_ma_writeinfo(info);
    DBUG_RETURN(my_errno=HA_ERR_END_OF_FILE);
  }
  info->cur_row.lastpos= filepos;
  info->cur_row.nextpos= filepos+share->base.pack_reclength;

  if (! cache_read)			/* No cacheing */
  {
    error= _ma_read_static_record(info, buf, filepos);
    DBUG_RETURN(error);
  }

	/* Read record with cacheing */
  error=my_b_read(&info->rec_cache, buf, share->base.reclength);
  if (info->s->base.pack_reclength != info->s->base.reclength && !error)
  {
    uchar tmp[8];				/* Skill fill bytes */
    error=my_b_read(&info->rec_cache, tmp,
		    info->s->base.pack_reclength - info->s->base.reclength);
  }
  if (locked)
    VOID(_ma_writeinfo(info,0));		/* Unlock keyfile */
  if (!error)
  {
    if (!buf[0])
    {						/* Record is removed */
      DBUG_RETURN(my_errno=HA_ERR_RECORD_DELETED);
    }
						/* Found and may be updated */
    info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;
    DBUG_RETURN(0);
  }
  /* my_errno should be set if rec_cache.error == -1 */
  if (info->rec_cache.error != -1 || my_errno == 0)
    my_errno=HA_ERR_WRONG_IN_RECORD;
  DBUG_RETURN(my_errno);			/* Something wrong (EOF?) */
}
