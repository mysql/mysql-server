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

/*
  Functions to handle space-packed-records and blobs

  A row may be stored in one or more linked blocks.
  The block size is between MARIA_MIN_BLOCK_LENGTH and MARIA_MAX_BLOCK_LENGTH.
  Each block is aligned on MARIA_DYN_ALIGN_SIZE.
  The reson for the max block size is to not have too many different types
  of blocks.  For the differnet block types, look at _ma_get_block_info()
*/

#include "maria_def.h"

static my_bool write_dynamic_record(MARIA_HA *info,const uchar *record,
                                    ulong reclength);
static int _ma_find_writepos(MARIA_HA *info,ulong reclength,my_off_t *filepos,
			     ulong *length);
static my_bool update_dynamic_record(MARIA_HA *info, MARIA_RECORD_POS filepos,
                                     uchar *record, ulong reclength);
static my_bool delete_dynamic_record(MARIA_HA *info,MARIA_RECORD_POS filepos,
                                     uint second_read);
static my_bool _ma_cmp_buffer(File file, const uchar *buff, my_off_t filepos,
                              uint length);

	/* Interface function from MARIA_HA */

#ifdef HAVE_MMAP

/*
  Create mmaped area for MARIA handler

  SYNOPSIS
    _ma_dynmap_file()
    info		MARIA handler

  RETURN
    0  ok
    1  error.
*/

my_bool _ma_dynmap_file(MARIA_HA *info, my_off_t size)
{
  DBUG_ENTER("_ma_dynmap_file");
  if (size > (my_off_t) (~((size_t) 0)) - MEMMAP_EXTRA_MARGIN)
  {
    DBUG_PRINT("warning", ("File is too large for mmap"));
    DBUG_RETURN(1);
  }
  /*
    Ingo wonders if it is good to use MAP_NORESERVE. From the Linux man page:
    MAP_NORESERVE
      Do not reserve swap space for this mapping. When swap space is
      reserved, one has the guarantee that it is possible to modify the
      mapping. When swap space is not reserved one might get SIGSEGV
      upon a write if no physical memory is available.
  */
  info->s->file_map= (uchar*)
                  my_mmap(0, (size_t)(size + MEMMAP_EXTRA_MARGIN),
                          info->s->mode==O_RDONLY ? PROT_READ :
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_NORESERVE,
                          info->dfile.file, 0L);
  if (info->s->file_map == (uchar*) MAP_FAILED)
  {
    info->s->file_map= NULL;
    DBUG_RETURN(1);
  }
#if defined(HAVE_MADVISE)
  madvise((char*) info->s->file_map, size, MADV_RANDOM);
#endif
  info->s->mmaped_length= size;
  DBUG_RETURN(0);
}


/*
  Resize mmaped area for MARIA handler

  SYNOPSIS
    _ma_remap_file()
    info		MARIA handler

  RETURN
*/

void _ma_remap_file(MARIA_HA *info, my_off_t size)
{
  if (info->s->file_map)
  {
    my_munmap((char*) info->s->file_map,
                   (size_t) info->s->mmaped_length + MEMMAP_EXTRA_MARGIN);
    _ma_dynmap_file(info, size);
  }
}
#endif


/*
  Read bytes from MySAM handler, using mmap or pread

  SYNOPSIS
    _ma_mmap_pread()
    info		MARIA handler
    Buffer              Input buffer
    Count               Count of bytes for read
    offset              Start position
    MyFlags

  RETURN
    0  ok
*/

size_t _ma_mmap_pread(MARIA_HA *info, uchar *Buffer,
		      size_t Count, my_off_t offset, myf MyFlags)
{
  DBUG_PRINT("info", ("maria_read with mmap %d\n", info->dfile.file));
  if (info->s->lock_key_trees)
    mysql_rwlock_rdlock(&info->s->mmap_lock);

  /*
    The following test may fail in the following cases:
    - We failed to remap a memory area (fragmented memory?)
    - This thread has done some writes, but not yet extended the
    memory mapped area.
  */

  if (info->s->mmaped_length >= offset + Count)
  {
    memcpy(Buffer, info->s->file_map + offset, Count);
    if (info->s->lock_key_trees)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return 0;
  }
  else
  {
    if (info->s->lock_key_trees)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return mysql_file_pread(info->dfile.file, Buffer, Count, offset, MyFlags);
  }
}


        /* wrapper for my_pread in case if mmap isn't used */

size_t _ma_nommap_pread(MARIA_HA *info, uchar *Buffer,
			size_t Count, my_off_t offset, myf MyFlags)
{
  return mysql_file_pread(info->dfile.file, Buffer, Count, offset, MyFlags);
}


/*
  Write bytes to MySAM handler, using mmap or pwrite

  SYNOPSIS
    _ma_mmap_pwrite()
    info		MARIA handler
    Buffer              Output buffer
    Count               Count of bytes for write
    offset              Start position
    MyFlags

  RETURN
    0  ok
    !=0  error.  In this case return error from pwrite
*/

size_t _ma_mmap_pwrite(MARIA_HA *info, const uchar *Buffer,
		       size_t Count, my_off_t offset, myf MyFlags)
{
  DBUG_PRINT("info", ("maria_write with mmap %d\n", info->dfile.file));
  if (info->s->lock_key_trees)
    mysql_rwlock_rdlock(&info->s->mmap_lock);

  /*
    The following test may fail in the following cases:
    - We failed to remap a memory area (fragmented memory?)
    - This thread has done some writes, but not yet extended the
    memory mapped area.
  */

  if (info->s->mmaped_length >= offset + Count)
  {
    memcpy(info->s->file_map + offset, Buffer, Count);
    if (info->s->lock_key_trees)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return 0;
  }
  else
  {
    info->s->nonmmaped_inserts++;
    if (info->s->lock_key_trees)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return my_pwrite(info->dfile.file, Buffer, Count, offset, MyFlags);
  }

}


        /* wrapper for my_pwrite in case if mmap isn't used */

size_t _ma_nommap_pwrite(MARIA_HA *info, const uchar *Buffer,
			 size_t Count, my_off_t offset, myf MyFlags)
{
  return my_pwrite(info->dfile.file, Buffer, Count, offset, MyFlags);
}


my_bool _ma_write_dynamic_record(MARIA_HA *info, const uchar *record)
{
  ulong reclength= _ma_rec_pack(info,info->rec_buff + MARIA_REC_BUFF_OFFSET,
                                record);
  return (write_dynamic_record(info,info->rec_buff + MARIA_REC_BUFF_OFFSET,
                               reclength));
}

my_bool _ma_update_dynamic_record(MARIA_HA *info, MARIA_RECORD_POS pos,
                                  const uchar *oldrec __attribute__ ((unused)),
                                  const uchar *record)
{
  uint length= _ma_rec_pack(info, info->rec_buff + MARIA_REC_BUFF_OFFSET,
                            record);
  return (update_dynamic_record(info, pos,
                                info->rec_buff + MARIA_REC_BUFF_OFFSET,
                                length));
}


my_bool _ma_write_blob_record(MARIA_HA *info, const uchar *record)
{
  uchar *rec_buff;
  int error;
  ulong reclength,reclength2,extra;

  extra= (ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER)+MARIA_SPLIT_LENGTH+
	  MARIA_DYN_DELETE_BLOCK_HEADER+1);
  reclength= (info->s->base.pack_reclength +
	      _ma_calc_total_blob_length(info,record)+ extra);
  if (!(rec_buff=(uchar*) my_safe_alloca(reclength,
                                         MARIA_MAX_RECORD_ON_STACK)))
  {
    my_errno= HA_ERR_OUT_OF_MEM; /* purecov: inspected */
    return(1);
  }
  reclength2= _ma_rec_pack(info,
                           rec_buff+ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER),
			   record);
  DBUG_PRINT("info",("reclength: %lu  reclength2: %lu",
		     reclength, reclength2));
  DBUG_ASSERT(reclength2 <= reclength);
  error= write_dynamic_record(info,
                              rec_buff+ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER),
                              reclength2);
  my_safe_afree(rec_buff, reclength, MARIA_MAX_RECORD_ON_STACK);
  return(error != 0);
}


my_bool _ma_update_blob_record(MARIA_HA *info, MARIA_RECORD_POS pos,
                               const uchar *oldrec __attribute__ ((unused)),
                               const uchar *record)
{
  uchar *rec_buff;
  int error;
  ulong reclength,extra;

  extra= (ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER)+MARIA_SPLIT_LENGTH+
	  MARIA_DYN_DELETE_BLOCK_HEADER);
  reclength= (info->s->base.pack_reclength+
	      _ma_calc_total_blob_length(info,record)+ extra);
#ifdef NOT_USED					/* We now support big rows */
  if (reclength > MARIA_DYN_MAX_ROW_LENGTH)
  {
    my_errno=HA_ERR_TO_BIG_ROW;
    return 1;
  }
#endif
  if (!(rec_buff=(uchar*) my_safe_alloca(reclength,
                                         MARIA_MAX_RECORD_ON_STACK)))
  {
    my_errno= HA_ERR_OUT_OF_MEM; /* purecov: inspected */
    return(1);
  }
  reclength= _ma_rec_pack(info,rec_buff+ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER),
			 record);
  error=update_dynamic_record(info,pos,
			      rec_buff+ALIGN_SIZE(MARIA_MAX_DYN_BLOCK_HEADER),
			      reclength);
  my_safe_afree(rec_buff, reclength, MARIA_MAX_RECORD_ON_STACK);
  return(error != 0);
}


my_bool _ma_delete_dynamic_record(MARIA_HA *info,
                                  const uchar *record __attribute__ ((unused)))
{
  return delete_dynamic_record(info, info->cur_row.lastpos, 0);
}


/**
  Write record to data-file.

  @todo it's cheating: it casts "const uchar*" to uchar*.
*/

static my_bool write_dynamic_record(MARIA_HA *info, const uchar *record,
                                    ulong reclength)
{
  int flag;
  ulong length;
  my_off_t filepos;
  DBUG_ENTER("write_dynamic_record");

  flag=0;

  /*
    Check if we have enough room for the new record.
    First we do simplified check to make usual case faster.
    Then we do more precise check for the space left.
    Though it still is not absolutely precise, as
    we always use MARIA_MAX_DYN_BLOCK_HEADER while it can be
    less in the most of the cases.
  */

  if (unlikely(info->s->base.max_data_file_length -
               info->state->data_file_length <
               reclength + MARIA_MAX_DYN_BLOCK_HEADER))
  {
    if (info->s->base.max_data_file_length - info->state->data_file_length +
        info->state->empty - info->state->del * MARIA_MAX_DYN_BLOCK_HEADER <
        reclength + MARIA_MAX_DYN_BLOCK_HEADER)
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(1);
    }
  }

  do
  {
    if (_ma_find_writepos(info,reclength,&filepos,&length))
      goto err;
    if (_ma_write_part_record(info,filepos,length,
                              (info->append_insert_at_end ?
                               HA_OFFSET_ERROR : info->s->state.dellink),
			      (uchar**) &record,&reclength,&flag))
      goto err;
  } while (reclength);

  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


	/* Get a block for data ; The given data-area must be used !! */

static int _ma_find_writepos(MARIA_HA *info,
			     ulong reclength, /* record length */
			     my_off_t *filepos, /* Return file pos */
			     ulong *length)   /* length of block at filepos */
{
  MARIA_BLOCK_INFO block_info;
  ulong tmp;
  DBUG_ENTER("_ma_find_writepos");

  if (info->s->state.dellink != HA_OFFSET_ERROR &&
      !info->append_insert_at_end)
  {
    /* Deleted blocks exists;  Get last used block */
    *filepos=info->s->state.dellink;
    block_info.second_read=0;
    info->rec_cache.seek_not_done=1;
    if (!(_ma_get_block_info(info, &block_info, info->dfile.file,
                             info->s->state.dellink) &
	   BLOCK_DELETED))
    {
      DBUG_PRINT("error",("Delete link crashed"));
      _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
      DBUG_RETURN(-1);
    }
    info->s->state.dellink=block_info.next_filepos;
    info->state->del--;
    info->state->empty-= block_info.block_len;
    *length= block_info.block_len;
  }
  else
  {
    /* No deleted blocks;  Allocate a new block */
    *filepos=info->state->data_file_length;
    if ((tmp=reclength+3 + test(reclength >= (65520-3))) <
	info->s->base.min_block_length)
      tmp= info->s->base.min_block_length;
    else
      tmp= ((tmp+MARIA_DYN_ALIGN_SIZE-1) &
	    (~ (ulong) (MARIA_DYN_ALIGN_SIZE-1)));
    if (info->state->data_file_length >
	(info->s->base.max_data_file_length - tmp))
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(-1);
    }
    if (tmp > MARIA_MAX_BLOCK_LENGTH)
      tmp=MARIA_MAX_BLOCK_LENGTH;
    *length= tmp;
    info->state->data_file_length+= tmp;
    info->s->state.split++;
    info->update|=HA_STATE_WRITE_AT_END;
  }
  DBUG_RETURN(0);
} /* _ma_find_writepos */



/*
  Unlink a deleted block from the deleted list.
  This block will be combined with the preceding or next block to form
  a big block.
*/

static my_bool unlink_deleted_block(MARIA_HA *info,
                                    MARIA_BLOCK_INFO *block_info)
{
  DBUG_ENTER("unlink_deleted_block");
  if (block_info->filepos == info->s->state.dellink)
  {
    /* First deleted block;  We can just use this ! */
    info->s->state.dellink=block_info->next_filepos;
  }
  else
  {
    MARIA_BLOCK_INFO tmp;
    tmp.second_read=0;
    /* Unlink block from the previous block */
    if (!(_ma_get_block_info(info, &tmp, info->dfile.file,
                             block_info->prev_filepos)
	  & BLOCK_DELETED))
      DBUG_RETURN(1);				/* Something is wrong */
    mi_sizestore(tmp.header+4,block_info->next_filepos);
    if (info->s->file_write(info, tmp.header+4,8,
		  block_info->prev_filepos+4, MYF(MY_NABP)))
      DBUG_RETURN(1);
    /* Unlink block from next block */
    if (block_info->next_filepos != HA_OFFSET_ERROR)
    {
      if (!(_ma_get_block_info(info, &tmp, info->dfile.file,
                               block_info->next_filepos)
	    & BLOCK_DELETED))
	DBUG_RETURN(1);				/* Something is wrong */
      mi_sizestore(tmp.header+12,block_info->prev_filepos);
      if (info->s->file_write(info, tmp.header+12,8,
		    block_info->next_filepos+12,
		    MYF(MY_NABP)))
	DBUG_RETURN(1);
    }
  }
  /* We now have one less deleted block */
  info->state->del--;
  info->state->empty-= block_info->block_len;
  info->s->state.split--;

  /*
    If this was a block that we where accessing through table scan
    (maria_rrnd() or maria_scan(), then ensure that we skip over this block
    when doing next maria_rrnd() or maria_scan().
  */
  if (info->cur_row.nextpos == block_info->filepos)
    info->cur_row.nextpos+= block_info->block_len;
  DBUG_RETURN(0);
}


/*
  Add a backward link to delete block

  SYNOPSIS
    update_backward_delete_link()
    info		MARIA handler
    delete_block	Position to delete block to update.
			If this is 'HA_OFFSET_ERROR', nothing will be done
    filepos		Position to block that 'delete_block' should point to

  RETURN
    0  ok
    1  error.  In this case my_error is set.
*/

static my_bool update_backward_delete_link(MARIA_HA *info,
                                           my_off_t delete_block,
                                           MARIA_RECORD_POS filepos)
{
  MARIA_BLOCK_INFO block_info;
  DBUG_ENTER("update_backward_delete_link");

  if (delete_block != HA_OFFSET_ERROR)
  {
    block_info.second_read=0;
    if (_ma_get_block_info(info, &block_info, info->dfile.file, delete_block)
	& BLOCK_DELETED)
    {
      uchar buff[8];
      mi_sizestore(buff,filepos);
      if (info->s->file_write(info,buff, 8, delete_block+12, MYF(MY_NABP)))
	DBUG_RETURN(1);				/* Error on write */
    }
    else
    {
      _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
      DBUG_RETURN(1);				/* Wrong delete link */
    }
  }
  DBUG_RETURN(0);
}

/* Delete datarecord from database */
/* info->rec_cache.seek_not_done is updated in cmp_record */

static my_bool delete_dynamic_record(MARIA_HA *info, MARIA_RECORD_POS filepos,
                                     uint second_read)
{
  uint length,b_type;
  MARIA_BLOCK_INFO block_info,del_block;
  int error;
  my_bool remove_next_block;
  DBUG_ENTER("delete_dynamic_record");

  /* First add a link from the last block to the new one */
  error= update_backward_delete_link(info, info->s->state.dellink, filepos);

  block_info.second_read=second_read;
  do
  {
    /* Remove block at 'filepos' */
    if ((b_type= _ma_get_block_info(info, &block_info, info->dfile.file,
                                    filepos))
	& (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	   BLOCK_FATAL_ERROR) ||
	(length=(uint) (block_info.filepos-filepos) +block_info.block_len) <
	MARIA_MIN_BLOCK_LENGTH)
    {
      _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
      DBUG_RETURN(1);
    }
    /* Check if next block is a delete block */
    del_block.second_read=0;
    remove_next_block=0;
    if (_ma_get_block_info(info, &del_block, info->dfile.file,
                           filepos + length) &
	BLOCK_DELETED && del_block.block_len+length <
        MARIA_DYN_MAX_BLOCK_LENGTH)
    {
      /* We can't remove this yet as this block may be the head block */
      remove_next_block=1;
      length+=del_block.block_len;
    }

    block_info.header[0]=0;
    mi_int3store(block_info.header+1,length);
    mi_sizestore(block_info.header+4,info->s->state.dellink);
    if (b_type & BLOCK_LAST)
      bfill(block_info.header+12,8,255);
    else
      mi_sizestore(block_info.header+12,block_info.next_filepos);
    if (info->s->file_write(info, block_info.header, 20, filepos,
		  MYF(MY_NABP)))
      DBUG_RETURN(1);
    info->s->state.dellink = filepos;
    info->state->del++;
    info->state->empty+=length;
    filepos=block_info.next_filepos;

    /* Now it's safe to unlink the deleted block directly after this one */
    if (remove_next_block && unlink_deleted_block(info,&del_block))
      error=1;
  } while (!(b_type & BLOCK_LAST));

  DBUG_RETURN(error);
}


	/* Write a block to datafile */

int _ma_write_part_record(MARIA_HA *info,
			  my_off_t filepos,	/* points at empty block */
			  ulong length,		/* length of block */
			  my_off_t next_filepos,/* Next empty block */
			  uchar **record,	/* pointer to record ptr */
			  ulong *reclength,	/* length of *record */
			  int *flag)		/* *flag == 0 if header */
{
  ulong head_length,res_length,extra_length,long_block,del_length;
  uchar *pos,*record_end;
  my_off_t  next_delete_block;
  uchar temp[MARIA_SPLIT_LENGTH+MARIA_DYN_DELETE_BLOCK_HEADER];
  DBUG_ENTER("_ma_write_part_record");

  next_delete_block=HA_OFFSET_ERROR;

  res_length=extra_length=0;
  if (length > *reclength + MARIA_SPLIT_LENGTH)
  {						/* Splitt big block */
    res_length=MY_ALIGN(length- *reclength - MARIA_EXTEND_BLOCK_LENGTH,
			MARIA_DYN_ALIGN_SIZE);
    length-= res_length;			/* Use this for first part */
  }
  long_block= (length < 65520L && *reclength < 65520L) ? 0 : 1;
  if (length == *reclength+ 3 + long_block)
  {
    /* Block is exactly of the right length */
    temp[0]=(uchar) (1+ *flag)+(uchar) long_block;	/* Flag is 0 or 6 */
    if (long_block)
    {
      mi_int3store(temp+1,*reclength);
      head_length=4;
    }
    else
    {
      mi_int2store(temp+1,*reclength);
      head_length=3;
    }
  }
  else if (length-long_block < *reclength+4)
  {						/* To short block */
    if (next_filepos == HA_OFFSET_ERROR)
      next_filepos= (info->s->state.dellink != HA_OFFSET_ERROR &&
                     !info->append_insert_at_end ?
                     info->s->state.dellink : info->state->data_file_length);
    if (*flag == 0)				/* First block */
    {
      if (*reclength > MARIA_MAX_BLOCK_LENGTH)
      {
	head_length= 16;
	temp[0]=13;
	mi_int4store(temp+1,*reclength);
	mi_int3store(temp+5,length-head_length);
	mi_sizestore(temp+8,next_filepos);
      }
      else
      {
	head_length=5+8+long_block*2;
	temp[0]=5+(uchar) long_block;
	if (long_block)
	{
	  mi_int3store(temp+1,*reclength);
	  mi_int3store(temp+4,length-head_length);
	  mi_sizestore(temp+7,next_filepos);
	}
	else
	{
	  mi_int2store(temp+1,*reclength);
	  mi_int2store(temp+3,length-head_length);
	  mi_sizestore(temp+5,next_filepos);
	}
      }
    }
    else
    {
      head_length=3+8+long_block;
      temp[0]=11+(uchar) long_block;
      if (long_block)
      {
	mi_int3store(temp+1,length-head_length);
	mi_sizestore(temp+4,next_filepos);
      }
      else
      {
	mi_int2store(temp+1,length-head_length);
	mi_sizestore(temp+3,next_filepos);
      }
    }
  }
  else
  {					/* Block with empty info last */
    head_length=4+long_block;
    extra_length= length- *reclength-head_length;
    temp[0]= (uchar) (3+ *flag)+(uchar) long_block; /* 3,4 or 9,10 */
    if (long_block)
    {
      mi_int3store(temp+1,*reclength);
      temp[4]= (uchar) (extra_length);
    }
    else
    {
      mi_int2store(temp+1,*reclength);
      temp[3]= (uchar) (extra_length);
    }
    length= *reclength+head_length;	/* Write only what is needed */
  }
  DBUG_DUMP("header", temp, head_length);

	/* Make a long block for one write */
  record_end= *record+length-head_length;
  del_length=(res_length ? MARIA_DYN_DELETE_BLOCK_HEADER : 0);
  bmove((*record-head_length), temp, head_length);
  memcpy(temp,record_end,(size_t) (extra_length+del_length));
  bzero(record_end, extra_length);

  if (res_length)
  {
    /* Check first if we can join this block with the next one */
    MARIA_BLOCK_INFO del_block;
    my_off_t next_block=filepos+length+extra_length+res_length;

    del_block.second_read=0;
    if (next_block < info->state->data_file_length &&
	info->s->state.dellink != HA_OFFSET_ERROR)
    {
      if ((_ma_get_block_info(info, &del_block, info->dfile.file, next_block)
	   & BLOCK_DELETED) &&
	  res_length + del_block.block_len < MARIA_DYN_MAX_BLOCK_LENGTH)
      {
	if (unlink_deleted_block(info,&del_block))
	  goto err;
	res_length+=del_block.block_len;
      }
    }

    /* Create a delete link of the last part of the block */
    pos=record_end+extra_length;
    pos[0]= '\0';
    mi_int3store(pos+1,res_length);
    mi_sizestore(pos+4,info->s->state.dellink);
    bfill(pos+12,8,255);			/* End link */
    next_delete_block=info->s->state.dellink;
    info->s->state.dellink= filepos+length+extra_length;
    info->state->del++;
    info->state->empty+=res_length;
    info->s->state.split++;
  }
  if (info->opt_flag & WRITE_CACHE_USED &&
      info->update & HA_STATE_WRITE_AT_END)
  {
    if (info->update & HA_STATE_EXTEND_BLOCK)
    {
      info->update&= ~HA_STATE_EXTEND_BLOCK;
      if (my_block_write(&info->rec_cache, *record-head_length,
			 length+extra_length+del_length,filepos))
      goto err;
    }
    else if (my_b_write(&info->rec_cache, *record-head_length,
			length+extra_length+del_length))
      goto err;
  }
  else
  {
    info->rec_cache.seek_not_done=1;
    if (info->s->file_write(info, *record-head_length,
                            length+extra_length+
                            del_length,filepos,info->s->write_flag))
      goto err;
  }
  memcpy(record_end,temp,(size_t) (extra_length+del_length));
  *record=record_end;
  *reclength-=(length-head_length);
  *flag=6;

  if (del_length)
  {
    /* link the next delete block to this */
    if (update_backward_delete_link(info, next_delete_block,
				    info->s->state.dellink))
      goto err;
  }

  DBUG_RETURN(0);
err:
  DBUG_PRINT("exit",("errno: %d",my_errno));
  DBUG_RETURN(1);
} /* _ma_write_part_record */


	/* update record from datafile */

static my_bool update_dynamic_record(MARIA_HA *info, MARIA_RECORD_POS filepos,
                                     uchar *record, ulong reclength)
{
  int flag;
  uint error;
  ulong length;
  MARIA_BLOCK_INFO block_info;
  DBUG_ENTER("update_dynamic_record");

  flag=block_info.second_read=0;
  /*
    Check if we have enough room for the record.
    First we do simplified check to make usual case faster.
    Then we do more precise check for the space left.
    Though it still is not absolutely precise, as
    we always use MARIA_MAX_DYN_BLOCK_HEADER while it can be
    less in the most of the cases.
  */

  /*
    compare with just the reclength as we're going
    to get some space from the old replaced record
  */
  if (unlikely(info->s->base.max_data_file_length -
        info->state->data_file_length < reclength))
  {
    /* If new record isn't longer, we can go on safely */
    if (info->cur_row.total_length < reclength)
    {
      if (info->s->base.max_data_file_length - info->state->data_file_length +
          info->state->empty - info->state->del * MARIA_MAX_DYN_BLOCK_HEADER <
          reclength - info->cur_row.total_length + MARIA_MAX_DYN_BLOCK_HEADER)
      {
        my_errno=HA_ERR_RECORD_FILE_FULL;
        goto err;
      }
    }
  }
  /* Remember length for updated row if it's updated again */
  info->cur_row.total_length= reclength;

  while (reclength > 0)
  {
    if (filepos != info->s->state.dellink)
    {
      block_info.next_filepos= HA_OFFSET_ERROR;
      if ((error= _ma_get_block_info(info, &block_info, info->dfile.file,
                                     filepos))
	  & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	     BLOCK_FATAL_ERROR))
      {
	DBUG_PRINT("error",("Got wrong block info"));
	if (!(error & BLOCK_FATAL_ERROR))
          _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
	goto err;
      }
      length=(ulong) (block_info.filepos-filepos) + block_info.block_len;
      if (length < reclength)
      {
	uint tmp=MY_ALIGN(reclength - length + 3 +
			  test(reclength >= 65520L),MARIA_DYN_ALIGN_SIZE);
	/* Don't create a block bigger than MARIA_MAX_BLOCK_LENGTH */
	tmp= min(length+tmp, MARIA_MAX_BLOCK_LENGTH)-length;
	/* Check if we can extend this block */
	if (block_info.filepos + block_info.block_len ==
	    info->state->data_file_length &&
	    info->state->data_file_length <
	    info->s->base.max_data_file_length-tmp)
	{
	  /* extend file */
	  DBUG_PRINT("info",("Extending file with %d bytes",tmp));
	  if (info->cur_row.nextpos == info->state->data_file_length)
	    info->cur_row.nextpos+= tmp;
	  info->state->data_file_length+= tmp;
	  info->update|= HA_STATE_WRITE_AT_END | HA_STATE_EXTEND_BLOCK;
	  length+=tmp;
	}
	else if (length < MARIA_MAX_BLOCK_LENGTH - MARIA_MIN_BLOCK_LENGTH)
	{
	  /*
	    Check if next block is a deleted block
	    Above we have MARIA_MIN_BLOCK_LENGTH to avoid the problem where
	    the next block is so small it can't be splited which could
	    casue problems
	  */

	  MARIA_BLOCK_INFO del_block;
	  del_block.second_read=0;
	  if (_ma_get_block_info(info, &del_block, info->dfile.file,
				 block_info.filepos + block_info.block_len) &
	      BLOCK_DELETED)
	  {
	    /* Use; Unlink it and extend the current block */
	    DBUG_PRINT("info",("Extending current block"));
	    if (unlink_deleted_block(info,&del_block))
	      goto err;
	    if ((length+=del_block.block_len) > MARIA_MAX_BLOCK_LENGTH)
	    {
	      /*
		New block was too big, link overflow part back to
		delete list
	      */
	      my_off_t next_pos;
	      ulong rest_length= length-MARIA_MAX_BLOCK_LENGTH;
	      set_if_bigger(rest_length, MARIA_MIN_BLOCK_LENGTH);
	      next_pos= del_block.filepos+ del_block.block_len - rest_length;

	      if (update_backward_delete_link(info, info->s->state.dellink,
					      next_pos))
		DBUG_RETURN(1);

	      /* create delete link for data that didn't fit into the page */
	      del_block.header[0]=0;
	      mi_int3store(del_block.header+1, rest_length);
	      mi_sizestore(del_block.header+4,info->s->state.dellink);
	      bfill(del_block.header+12,8,255);
	      if (info->s->file_write(info, del_block.header, 20,
                                      next_pos, MYF(MY_NABP)))
		DBUG_RETURN(1);
	      info->s->state.dellink= next_pos;
	      info->s->state.split++;
	      info->state->del++;
	      info->state->empty+= rest_length;
	      length-= rest_length;
	    }
	  }
	}
      }
    }
    else
    {
      if (_ma_find_writepos(info,reclength,&filepos,&length))
	goto err;
    }
    if (_ma_write_part_record(info,filepos,length,block_info.next_filepos,
			      &record,&reclength,&flag))
      goto err;
    if ((filepos=block_info.next_filepos) == HA_OFFSET_ERROR)
    {
      /* Start writing data on deleted blocks */
      filepos=info->s->state.dellink;
    }
  }

  if (block_info.next_filepos != HA_OFFSET_ERROR)
    if (delete_dynamic_record(info,block_info.next_filepos,1))
      goto err;

  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


	/* Pack a record. Return new reclength */

uint _ma_rec_pack(MARIA_HA *info, register uchar *to,
                  register const uchar *from)
{
  uint		length,new_length,flag,bit,i;
  const uchar   *pos,*end;
  uchar         *startpos,*packpos;
  enum en_fieldtype type;
  reg3 MARIA_COLUMNDEF *column;
  MARIA_BLOB	*blob;
  DBUG_ENTER("_ma_rec_pack");

  flag= 0;
  bit= 1;
  startpos= packpos=to;
  to+= info->s->base.pack_bytes;
  blob= info->blobs;
  column= info->s->columndef;
  if (info->s->base.null_bytes)
  {
    memcpy(to, from, info->s->base.null_bytes);
    from+= info->s->base.null_bytes;
    to+=   info->s->base.null_bytes;
  }

  for (i=info->s->base.fields ; i-- > 0; from+= length, column++)
  {
    length=(uint) column->length;
    if ((type = (enum en_fieldtype) column->type) != FIELD_NORMAL)
    {
      if (type == FIELD_BLOB)
      {
	if (!blob->length)
	  flag|=bit;
	else
	{
	  char *temp_pos;
	  size_t tmp_length=length-portable_sizeof_char_ptr;
	  memcpy(to,from,tmp_length);
	  memcpy(&temp_pos,from+tmp_length,sizeof(char*));
	  memcpy(to+tmp_length,temp_pos,(size_t) blob->length);
	  to+=tmp_length+blob->length;
	}
	blob++;
      }
      else if (type == FIELD_SKIP_ZERO)
      {
	if (memcmp(from, maria_zero_string, length) == 0)
	  flag|=bit;
	else
	{
          memcpy(to, from, (size_t) length);
          to+=length;
	}
      }
      else if (type == FIELD_SKIP_ENDSPACE ||
	       type == FIELD_SKIP_PRESPACE)
      {
	pos= from; end= from + length;
	if (type == FIELD_SKIP_ENDSPACE)
	{					/* Pack trailing spaces */
	  while (end > from && *(end-1) == ' ')
	    end--;
	}
	else
	{					/* Pack pref-spaces */
	  while (pos < end && *pos == ' ')
	    pos++;
	}
	new_length=(uint) (end-pos);
	if (new_length +1 + test(column->length > 255 && new_length > 127)
	    < length)
	{
	  if (column->length > 255 && new_length > 127)
	  {
            to[0]= (uchar) ((new_length & 127) + 128);
            to[1]= (uchar) (new_length >> 7);
	    to+=2;
	  }
	  else
	    *to++= (uchar) new_length;
	  memcpy(to, pos, (size_t) new_length); to+=new_length;
	  flag|=bit;
	}
	else
	{
	  memcpy(to,from,(size_t) length); to+=length;
	}
      }
      else if (type == FIELD_VARCHAR)
      {
        uint pack_length= HA_VARCHAR_PACKLENGTH(column->length -1);
	uint tmp_length;
        if (pack_length == 1)
        {
          tmp_length= (uint) *from;
          *to++= *from;
        }
        else
        {
          tmp_length= uint2korr(from);
          store_key_length_inc(to,tmp_length);
        }
        memcpy(to, from+pack_length,tmp_length);
        to+= tmp_length;
        continue;
      }
      else
      {
	memcpy(to,from,(size_t) length); to+=length;
	continue;				/* Normal field */
      }
      if ((bit= bit << 1) >= 256)
      {
	*packpos++ = (uchar) flag;
	bit=1; flag=0;
      }
    }
    else
    {
      memcpy(to,from,(size_t) length); to+=length;
    }
  }
  if (bit != 1)
    *packpos= (uchar) flag;
  if (info->s->calc_checksum)
    *to++= (uchar) info->cur_row.checksum;
  DBUG_PRINT("exit",("packed length: %d",(int) (to-startpos)));
  DBUG_RETURN((uint) (to-startpos));
} /* _ma_rec_pack */



/*
  Check if a record was correctly packed. Used only by maria_chk
  Returns 0 if record is ok.
*/

my_bool _ma_rec_check(MARIA_HA *info,const uchar *record, uchar *rec_buff,
                      ulong packed_length, my_bool with_checksum,
                      ha_checksum checksum)
{
  uint		length,new_length,flag,bit,i;
  const uchar   *pos,*end;
  uchar         *packpos,*to;
  enum en_fieldtype type;
  reg3 MARIA_COLUMNDEF *column;
  DBUG_ENTER("_ma_rec_check");

  packpos=rec_buff; to= rec_buff+info->s->base.pack_bytes;
  column= info->s->columndef;
  flag= *packpos; bit=1;
  record+= info->s->base.null_bytes;
  to+= info->s->base.null_bytes;

  for (i=info->s->base.fields ; i-- > 0; record+= length, column++)
  {
    length=(uint) column->length;
    if ((type = (enum en_fieldtype) column->type) != FIELD_NORMAL)
    {
      if (type == FIELD_BLOB)
      {
	uint blob_length=
	  _ma_calc_blob_length(length-portable_sizeof_char_ptr,record);
	if (!blob_length && !(flag & bit))
	  goto err;
	if (blob_length)
	  to+=length - portable_sizeof_char_ptr+ blob_length;
      }
      else if (type == FIELD_SKIP_ZERO)
      {
	if (memcmp(record, maria_zero_string, length) == 0)
	{
	  if (!(flag & bit))
	    goto err;
	}
	else
	  to+=length;
      }
      else if (type == FIELD_SKIP_ENDSPACE ||
	       type == FIELD_SKIP_PRESPACE)
      {
	pos= record; end= record + length;
	if (type == FIELD_SKIP_ENDSPACE)
	{					/* Pack trailing spaces */
	  while (end > record && *(end-1) == ' ')
	    end--;
	}
	else
	{					/* Pack pre-spaces */
	  while (pos < end && *pos == ' ')
	    pos++;
	}
	new_length=(uint) (end-pos);
	if (new_length +1 + test(column->length > 255 && new_length > 127)
	    < length)
	{
	  if (!(flag & bit))
	    goto err;
	  if (column->length > 255 && new_length > 127)
	  {
            /* purecov: begin inspected */
            if (to[0] != (uchar) ((new_length & 127) + 128) ||
                to[1] != (uchar) (new_length >> 7))
	      goto err;
	    to+=2;
            /* purecov: end */
	  }
	  else if (*to++ != (uchar) new_length)
	    goto err;
	  to+=new_length;
	}
	else
	  to+=length;
      }
      else if (type == FIELD_VARCHAR)
      {
        uint pack_length= HA_VARCHAR_PACKLENGTH(column->length -1);
	uint tmp_length;
        if (pack_length == 1)
        {
          tmp_length= (uint) *record;
          to+= 1+ tmp_length;
          continue;
        }
        else
        {
          tmp_length= uint2korr(record);
          to+= get_pack_length(tmp_length)+tmp_length;
        }
        continue;
      }
      else
      {
	to+=length;
	continue;				/* Normal field */
      }
      if ((bit= bit << 1) >= 256)
      {
	flag= *++packpos;
	bit=1;
      }
    }
    else
      to+= length;
  }
  if (packed_length != (uint) (to - rec_buff) +
      test(info->s->calc_checksum) || (bit != 1 && (flag & ~(bit - 1))))
    goto err;
  if (with_checksum && ((uchar) checksum != (uchar) *to))
  {
    DBUG_PRINT("error",("wrong checksum for row"));
    goto err;
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}


/*
  @brief Unpacks a record

  @return Recordlength
  @retval >0  ok
  @retval MY_FILE_ERROR (== -1)  Error.
          my_errno is set to HA_ERR_WRONG_IN_RECORD
*/

ulong _ma_rec_unpack(register MARIA_HA *info, register uchar *to, uchar *from,
		     ulong found_length)
{
  uint flag,bit,length,min_pack_length, column_length;
  enum en_fieldtype type;
  uchar *from_end,*to_end,*packpos;
  reg3 MARIA_COLUMNDEF *column, *end_column;
  DBUG_ENTER("_ma_rec_unpack");

  to_end=to + info->s->base.reclength;
  from_end=from+found_length;
  flag= (uchar) *from; bit=1; packpos=from;
  if (found_length < info->s->base.min_pack_length)
    goto err;
  from+= info->s->base.pack_bytes;
  min_pack_length= info->s->base.min_pack_length - info->s->base.pack_bytes;

  if ((length= info->s->base.null_bytes))
  {
    memcpy(to, from, length);
    from+= length;
    to+= length;
    min_pack_length-= length;
  }

  for (column= info->s->columndef, end_column= column + info->s->base.fields;
       column < end_column ; to+= column_length, column++)
  {
    column_length= column->length;
    if ((type = (enum en_fieldtype) column->type) != FIELD_NORMAL &&
	(type != FIELD_CHECK))
    {
      if (type == FIELD_VARCHAR)
      {
        uint pack_length= HA_VARCHAR_PACKLENGTH(column_length-1);
        if (pack_length == 1)
        {
          length= (uint) *(uchar*) from;
          if (length > column_length-1)
            goto err;
          *to= *from++;
        }
        else
        {
          get_key_length(length, from);
          if (length > column_length-2)
            goto err;
          int2store(to,length);
        }
        if (from+length > from_end)
          goto err;
        memcpy(to+pack_length, from, length);
        from+= length;
        min_pack_length--;
        continue;
      }
      if (flag & bit)
      {
	if (type == FIELD_BLOB || type == FIELD_SKIP_ZERO)
	  bzero(to, column_length);
	else if (type == FIELD_SKIP_ENDSPACE ||
		 type == FIELD_SKIP_PRESPACE)
	{
	  if (column->length > 255 && *from & 128)
	  {
	    if (from + 1 >= from_end)
	      goto err;
	    length= (*from & 127)+ ((uint) (uchar) *(from+1) << 7); from+=2;
	  }
	  else
	  {
	    if (from == from_end)
	      goto err;
	    length= (uchar) *from++;
	  }
	  min_pack_length--;
	  if (length >= column_length ||
	      min_pack_length + length > (uint) (from_end - from))
	    goto err;
	  if (type == FIELD_SKIP_ENDSPACE)
	  {
	    memcpy(to, from, (size_t) length);
	    bfill(to+length, column_length-length, ' ');
	  }
	  else
	  {
	    bfill(to, column_length-length, ' ');
	    memcpy(to+column_length-length, from, (size_t) length);
	  }
	  from+=length;
	}
      }
      else if (type == FIELD_BLOB)
      {
	uint size_length=column_length- portable_sizeof_char_ptr;
	ulong blob_length= _ma_calc_blob_length(size_length,from);
        ulong from_left= (ulong) (from_end - from);
        if (from_left < size_length ||
            from_left - size_length < blob_length ||
            from_left - size_length - blob_length < min_pack_length)
	  goto err;
	memcpy(to, from, (size_t) size_length);
	from+=size_length;
	memcpy(to+size_length,(uchar*) &from,sizeof(char*));
	from+=blob_length;
      }
      else
      {
	if (type == FIELD_SKIP_ENDSPACE || type == FIELD_SKIP_PRESPACE)
	  min_pack_length--;
	if (min_pack_length + column_length > (uint) (from_end - from))
	  goto err;
	memcpy(to, from, (size_t) column_length); from+=column_length;
      }
      if ((bit= bit << 1) >= 256)
      {
	flag= (uchar) *++packpos; bit=1;
      }
    }
    else
    {
      if (min_pack_length > (uint) (from_end - from))
	goto err;
      min_pack_length-=column_length;
      memcpy(to, from, (size_t) column_length);
      from+=column_length;
    }
  }
  if (info->s->calc_checksum)
    info->cur_row.checksum= (uint) (uchar) *from++;
  if (to == to_end && from == from_end && (bit == 1 || !(flag & ~(bit-1))))
    DBUG_RETURN(found_length);

err:
  _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
  DBUG_PRINT("error",("to_end: 0x%lx -> 0x%lx  from_end: 0x%lx -> 0x%lx",
		      (long) to, (long) to_end, (long) from, (long) from_end));
  DBUG_DUMP("from", info->rec_buff, info->s->base.min_pack_length);
  DBUG_RETURN(MY_FILE_ERROR);
} /* _ma_rec_unpack */


	/* Calc length of blob. Update info in blobs->length */

ulong _ma_calc_total_blob_length(MARIA_HA *info, const uchar *record)
{
  ulong length;
  MARIA_BLOB *blob,*end;

  for (length=0, blob= info->blobs, end=blob+info->s->base.blobs ;
       blob != end;
       blob++)
  {
    blob->length= _ma_calc_blob_length(blob->pack_length,
                                       record + blob->offset);
    length+=blob->length;
  }
  return length;
}


ulong _ma_calc_blob_length(uint length, const uchar *pos)
{
  switch (length) {
  case 1:
    return (uint) (uchar) *pos;
  case 2:
    return (uint) uint2korr(pos);
  case 3:
    return uint3korr(pos);
  case 4:
    return uint4korr(pos);
  default:
    break;
  }
  return 0; /* Impossible */
}


void _ma_store_blob_length(uchar *pos,uint pack_length,uint length)
{
  switch (pack_length) {
  case 1:
    *pos= (uchar) length;
    break;
  case 2:
    int2store(pos,length);
    break;
  case 3:
    int3store(pos,length);
    break;
  case 4:
    int4store(pos,length);
  default:
    break;
  }
  return;
}


/*
  Read record from datafile.

  SYNOPSIS
    _ma_read_dynamic_record()
      info                      MARIA_HA pointer to table.
      filepos                   From where to read the record.
      buf                       Destination for record.

  NOTE
    If a write buffer is active, it needs to be flushed if its contents
    intersects with the record to read. We always check if the position
    of the first uchar of the write buffer is lower than the position
    past the last uchar to read. In theory this is also true if the write
    buffer is completely below the read segment. That is, if there is no
    intersection. But this case is unusual. We flush anyway. Only if the
    first uchar in the write buffer is above the last uchar to read, we do
    not flush.

    A dynamic record may need several reads. So this check must be done
    before every read. Reading a dynamic record starts with reading the
    block header. If the record does not fit into the free space of the
    header, the block may be longer than the header. In this case a
    second read is necessary. These one or two reads repeat for every
    part of the record.

  RETURN
    0          OK
    #          Error number
*/

int _ma_read_dynamic_record(MARIA_HA *info, uchar *buf,
                            MARIA_RECORD_POS filepos)
{
  int block_of_record;
  uint b_type;
  MARIA_BLOCK_INFO block_info;
  File file;
  uchar *UNINIT_VAR(to);
  uint UNINIT_VAR(left_length);
  DBUG_ENTER("_ma_read_dynamic_record");

  if (filepos == HA_OFFSET_ERROR)
    goto err;

  file= info->dfile.file;
  block_of_record= 0;   /* First block of record is numbered as zero. */
  block_info.second_read= 0;
  do
  {
    /* A corrupted table can have wrong pointers. (Bug# 19835) */
    if (filepos == HA_OFFSET_ERROR)
      goto panic;
    if (info->opt_flag & WRITE_CACHE_USED &&
        (info->rec_cache.pos_in_file < filepos +
         MARIA_BLOCK_INFO_HEADER_LENGTH) &&
        flush_io_cache(&info->rec_cache))
      goto err;
    info->rec_cache.seek_not_done=1;
    if ((b_type= _ma_get_block_info(info, &block_info, file, filepos)) &
        (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
         BLOCK_FATAL_ERROR))
    {
      if (b_type & (BLOCK_SYNC_ERROR | BLOCK_DELETED))
        my_errno=HA_ERR_RECORD_DELETED;
      goto err;
    }
    if (block_of_record++ == 0)			/* First block */
    {
      info->cur_row.total_length= block_info.rec_len;
      if (block_info.rec_len > (uint) info->s->base.max_pack_length)
        goto panic;
      if (info->s->base.blobs)
      {
        if (_ma_alloc_buffer(&info->rec_buff, &info->rec_buff_size,
                             block_info.rec_len +
                             info->s->base.extra_rec_buff_size))
          goto err;
      }
      to= info->rec_buff;
      left_length=block_info.rec_len;
    }
    if (left_length < block_info.data_len || ! block_info.data_len)
      goto panic;			/* Wrong linked record */
    /* copy information that is already read */
    {
      uint offset= (uint) (block_info.filepos - filepos);
      uint prefetch_len= (sizeof(block_info.header) - offset);
      filepos+= sizeof(block_info.header);

      if (prefetch_len > block_info.data_len)
        prefetch_len= block_info.data_len;
      if (prefetch_len)
      {
        memcpy(to, block_info.header + offset, prefetch_len);
        block_info.data_len-= prefetch_len;
        left_length-= prefetch_len;
        to+= prefetch_len;
      }
    }
    /* read rest of record from file */
    if (block_info.data_len)
    {
      if (info->opt_flag & WRITE_CACHE_USED &&
          info->rec_cache.pos_in_file < filepos + block_info.data_len &&
          flush_io_cache(&info->rec_cache))
        goto err;
      /*
        What a pity that this method is not called 'file_pread' and that
        there is no equivalent without seeking. We are at the right
        position already. :(
      */
      if (info->s->file_read(info, to, block_info.data_len,
                             filepos, MYF(MY_NABP)))
        goto panic;
      left_length-=block_info.data_len;
      to+=block_info.data_len;
    }
    filepos= block_info.next_filepos;
  } while (left_length);

  info->update|= HA_STATE_AKTIV;	/* We have a aktive record */
  fast_ma_writeinfo(info);
  DBUG_RETURN(_ma_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
              MY_FILE_ERROR ? 0 : my_errno);

err:
  fast_ma_writeinfo(info);
  DBUG_RETURN(my_errno);

panic:
  _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
  goto err;
}

	/* compare unique constraint between stored rows */

my_bool _ma_cmp_dynamic_unique(MARIA_HA *info, MARIA_UNIQUEDEF *def,
                               const uchar *record, MARIA_RECORD_POS pos)
{
  uchar *old_rec_buff,*old_record;
  size_t old_rec_buff_size;
  my_bool error;
  DBUG_ENTER("_ma_cmp_dynamic_unique");

  if (!(old_record= my_safe_alloca(info->s->base.reclength,
                                   MARIA_MAX_RECORD_ON_STACK)))
    DBUG_RETURN(1);

  /* Don't let the compare destroy blobs that may be in use */
  old_rec_buff=      info->rec_buff;
  old_rec_buff_size= info->rec_buff_size;

  if (info->s->base.blobs)
  {
    info->rec_buff= 0;
    info->rec_buff_size= 0;
  }
  error= _ma_read_dynamic_record(info, old_record, pos) != 0;
  if (!error)
    error=_ma_unique_comp(def, record, old_record, def->null_are_equal) != 0;
  if (info->s->base.blobs)
  {
    my_free(info->rec_buff);
    info->rec_buff=      old_rec_buff;
    info->rec_buff_size= old_rec_buff_size;
  }
  my_safe_afree(old_record, info->s->base.reclength,
                MARIA_MAX_RECORD_ON_STACK);
  DBUG_RETURN(error);
}


	/* Compare of record on disk with packed record in memory */

my_bool _ma_cmp_dynamic_record(register MARIA_HA *info,
                               register const uchar *record)
{
  uint flag, reclength, b_type,cmp_length;
  my_off_t filepos;
  uchar *buffer;
  MARIA_BLOCK_INFO block_info;
  my_bool error= 1;
  size_t buffer_length;
  DBUG_ENTER("_ma_cmp_dynamic_record");
  LINT_INIT(buffer_length);

  if (info->opt_flag & WRITE_CACHE_USED)
  {
    info->update&= ~(HA_STATE_WRITE_AT_END | HA_STATE_EXTEND_BLOCK);
    if (flush_io_cache(&info->rec_cache))
      DBUG_RETURN(1);
  }
  info->rec_cache.seek_not_done=1;

	/* If nobody have touched the database we don't have to test rec */

  buffer=info->rec_buff;
  if ((info->opt_flag & READ_CHECK_USED))
  {						/* If check isn't disabled  */
    if (info->s->base.blobs)
    {
      buffer_length= (info->s->base.pack_reclength +
                      _ma_calc_total_blob_length(info,record));
      if (!(buffer=(uchar*) my_safe_alloca(buffer_length,
                                           MARIA_MAX_RECORD_ON_STACK)))
	DBUG_RETURN(1);
    }
    reclength= _ma_rec_pack(info,buffer,record);
    record= buffer;

    filepos= info->cur_row.lastpos;
    flag=block_info.second_read=0;
    block_info.next_filepos=filepos;
    while (reclength > 0)
    {
      if ((b_type= _ma_get_block_info(info, &block_info, info->dfile.file,
				    block_info.next_filepos))
	  & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	     BLOCK_FATAL_ERROR))
      {
	if (b_type & (BLOCK_SYNC_ERROR | BLOCK_DELETED))
	  my_errno=HA_ERR_RECORD_CHANGED;
	goto err;
      }
      if (flag == 0)				/* First block */
      {
	flag=1;
	if (reclength != block_info.rec_len)
	{
	  my_errno=HA_ERR_RECORD_CHANGED;
	  goto err;
	}
      } else if (reclength < block_info.data_len)
      {
        _ma_set_fatal_error(info->s, HA_ERR_WRONG_IN_RECORD);
	goto err;
      }
      reclength-= block_info.data_len;
      cmp_length= block_info.data_len;
      if (!reclength && info->s->calc_checksum)
        cmp_length--;        /* 'record' may not contain checksum */

      if (_ma_cmp_buffer(info->dfile.file, record, block_info.filepos,
			 cmp_length))
      {
	my_errno=HA_ERR_RECORD_CHANGED;
	goto err;
      }
      flag=1;
      record+=block_info.data_len;
    }
  }
  my_errno=0;
  error= 0;
err:
  if (buffer != info->rec_buff)
    my_safe_afree(buffer, buffer_length, MARIA_MAX_RECORD_ON_STACK);
  DBUG_PRINT("exit", ("result: %d", error));
  DBUG_RETURN(error);
}


	/* Compare file to buffert */

static my_bool _ma_cmp_buffer(File file, const uchar *buff, my_off_t filepos,
                              uint length)
{
  uint next_length;
  uchar temp_buff[IO_SIZE*2];
  DBUG_ENTER("_ma_cmp_buffer");

  next_length= IO_SIZE*2 - (uint) (filepos & (IO_SIZE-1));

  while (length > IO_SIZE*2)
  {
    if (mysql_file_pread(file,temp_buff,next_length,filepos, MYF(MY_NABP)) ||
	memcmp(buff, temp_buff, next_length))
      goto err;
    filepos+=next_length;
    buff+=next_length;
    length-= next_length;
    next_length=IO_SIZE*2;
  }
  if (mysql_file_pread(file,temp_buff,length,filepos,MYF(MY_NABP)))
    goto err;
  DBUG_RETURN(memcmp(buff, temp_buff, length) != 0);
err:
  DBUG_RETURN(1);
}


/*
  Read next record from datafile during table scan.

  SYNOPSIS
    _ma_read_rnd_dynamic_record()
      info                      MARIA_HA pointer to table.
      buf                       Destination for record.
      filepos                   From where to read the record.
      skip_deleted_blocks       If to repeat reading until a non-deleted
                                record is found.

  NOTE
    This is identical to _ma_read_dynamic_record(), except the following
    cases:

    - If there is no active row at 'filepos', continue scanning for
      an active row. (This is becasue the previous
      _ma_read_rnd_dynamic_record() call stored the next block position
      in filepos, but this position may not be a start block for a row
    - We may have READ_CACHING enabled, in which case we use the cache
      to read rows.

   For other comments, check _ma_read_dynamic_record()

  RETURN
    0           OK
    != 0        Error number
*/

int _ma_read_rnd_dynamic_record(MARIA_HA *info,
                                uchar *buf,
                                MARIA_RECORD_POS filepos,
				my_bool skip_deleted_blocks)
{
  int block_of_record;
#ifdef MARIA_EXTERNAL_LOCKING
  int info_read;
#endif
  uint left_len,b_type;
  uchar *UNINIT_VAR(to);
  MARIA_BLOCK_INFO block_info;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_read_rnd_dynamic_record");

#ifdef MARIA_EXTERNAL_LOCKING
  info_read=0;
#endif

  if (info->lock_type == F_UNLCK)
  {
#ifndef UNSAFE_LOCKING
#else
    info->tmp_lock_type=F_RDLCK;
#endif
  }
#ifdef MARIA_EXTERNAL_LOCKING
  else
    info_read=1;				/* memory-keyinfoblock is ok */
#endif

  block_of_record= 0;   /* First block of record is numbered as zero. */
  block_info.second_read= 0;
  left_len=1;
  do
  {
    if (filepos >= info->state->data_file_length)
    {
#ifdef MARIA_EXTERNAL_LOCKING
      if (!info_read)
      {						/* Check if changed */
	info_read=1;
	info->rec_cache.seek_not_done=1;
	if (_ma_state_info_read_dsk(share->kfile.file, &share->state))
	  goto panic;
      }
      if (filepos >= info->state->data_file_length)
      {
	my_errno= HA_ERR_END_OF_FILE;
	goto err;
      }
#else
      my_errno= HA_ERR_END_OF_FILE;
      goto err;
#endif
    }
    if (info->opt_flag & READ_CACHE_USED)
    {
      if (_ma_read_cache(info, &info->rec_cache, block_info.header, filepos,
			 sizeof(block_info.header),
			 (!block_of_record && skip_deleted_blocks ?
                          READING_NEXT : 0) | READING_HEADER))
	goto panic;
      b_type= _ma_get_block_info(info, &block_info,-1,filepos);
    }
    else
    {
      if (info->opt_flag & WRITE_CACHE_USED &&
	  info->rec_cache.pos_in_file < filepos + MARIA_BLOCK_INFO_HEADER_LENGTH &&
	  flush_io_cache(&info->rec_cache))
	DBUG_RETURN(my_errno);
      info->rec_cache.seek_not_done=1;
      b_type= _ma_get_block_info(info, &block_info, info->dfile.file, filepos);
    }

    if (b_type & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
		  BLOCK_FATAL_ERROR))
    {
      if ((b_type & (BLOCK_DELETED | BLOCK_SYNC_ERROR))
	  && skip_deleted_blocks)
      {
	filepos=block_info.filepos+block_info.block_len;
	block_info.second_read=0;
	continue;		/* Search after next_record */
      }
      if (b_type & (BLOCK_DELETED | BLOCK_SYNC_ERROR))
      {
	my_errno= HA_ERR_RECORD_DELETED;
	info->cur_row.lastpos= block_info.filepos;
	info->cur_row.nextpos= block_info.filepos+block_info.block_len;
      }
      goto err;
    }
    if (block_of_record == 0)				/* First block */
    {
      info->cur_row.total_length= block_info.rec_len;
      if (block_info.rec_len > (uint) share->base.max_pack_length)
	goto panic;
      info->cur_row.lastpos= filepos;
      if (share->base.blobs)
      {
	if (_ma_alloc_buffer(&info->rec_buff, &info->rec_buff_size,
                             block_info.rec_len +
                             info->s->base.extra_rec_buff_size))
	  goto err;
      }
      to= info->rec_buff;
      left_len=block_info.rec_len;
    }
    if (left_len < block_info.data_len)
      goto panic;				/* Wrong linked record */

    /* copy information that is already read */
    {
      uint offset=(uint) (block_info.filepos - filepos);
      uint tmp_length= (sizeof(block_info.header) - offset);
      filepos=block_info.filepos;

      if (tmp_length > block_info.data_len)
	tmp_length= block_info.data_len;
      if (tmp_length)
      {
	memcpy(to, block_info.header+offset, tmp_length);
	block_info.data_len-=tmp_length;
	left_len-=tmp_length;
	to+=tmp_length;
	filepos+=tmp_length;
     }
    }
    /* read rest of record from file */
    if (block_info.data_len)
    {
      if (info->opt_flag & READ_CACHE_USED)
      {
	if (_ma_read_cache(info, &info->rec_cache, to,filepos,
			   block_info.data_len,
			   (!block_of_record && skip_deleted_blocks) ?
                           READING_NEXT : 0))
	  goto panic;
      }
      else
      {
        if (info->opt_flag & WRITE_CACHE_USED &&
            info->rec_cache.pos_in_file <
            block_info.filepos + block_info.data_len &&
            flush_io_cache(&info->rec_cache))
          goto err;
	/* VOID(my_seek(info->dfile.file, filepos, MY_SEEK_SET, MYF(0))); */
	if (mysql_file_read(info->dfile.file, to, block_info.data_len, MYF(MY_NABP)))
	{
	  if (my_errno == HA_ERR_FILE_TOO_SHORT)
          {
            /* Unexpected end of file */
            _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
          }
	  goto err;
	}
      }
    }
    /*
      Increment block-of-record counter. If it was the first block,
      remember the position behind the block for the next call.
    */
    if (block_of_record++ == 0)
    {
      info->cur_row.nextpos= block_info.filepos+block_info.block_len;
      skip_deleted_blocks=0;
    }
    left_len-=block_info.data_len;
    to+=block_info.data_len;
    filepos=block_info.next_filepos;
  } while (left_len);

  info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;
  fast_ma_writeinfo(info);
  if (_ma_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
      MY_FILE_ERROR)
    DBUG_RETURN(0);
  DBUG_RETURN(my_errno);			/* Wrong record */

panic:
  /* Something is fatal wrong */
  _ma_set_fatal_error(share, HA_ERR_WRONG_IN_RECORD);
err:
  fast_ma_writeinfo(info);
  DBUG_RETURN(my_errno);
}


	/* Read and process header from a dynamic-record-file */

uint _ma_get_block_info(MARIA_HA *handler, MARIA_BLOCK_INFO *info, File file,
                        my_off_t filepos)
{
  uint return_val=0;
  uchar *header=info->header;

  if (file >= 0)
  {
    /*
      We do not use my_pread() here because we want to have the file
      pointer set to the end of the header after this function.
      my_pread() may leave the file pointer untouched.
    */
    mysql_file_seek(file,filepos,MY_SEEK_SET,MYF(0));
    if (mysql_file_read(file, header, sizeof(info->header),MYF(0)) !=
	sizeof(info->header))
    {
      /*
        This is either an error or just reading at end of file.
        Don't give a fatal error for this case.
      */
      my_errno= HA_ERR_WRONG_IN_RECORD;
      return BLOCK_ERROR;
    }
  }
  DBUG_DUMP("header",header,MARIA_BLOCK_INFO_HEADER_LENGTH);
  if (info->second_read)
  {
    if (info->header[0] <= 6 || info->header[0] == 13)
      return_val=BLOCK_SYNC_ERROR;
  }
  else
  {
    if (info->header[0] > 6 && info->header[0] != 13)
      return_val=BLOCK_SYNC_ERROR;
  }
  info->next_filepos= HA_OFFSET_ERROR; /* Dummy if no next block */

  switch (info->header[0]) {
  case 0:
    if ((info->block_len=(uint) mi_uint3korr(header+1)) <
	MARIA_MIN_BLOCK_LENGTH ||
	(info->block_len & (MARIA_DYN_ALIGN_SIZE -1)))
      goto err;
    info->filepos=filepos;
    info->next_filepos=mi_sizekorr(header+4);
    info->prev_filepos=mi_sizekorr(header+12);
#if SIZEOF_OFF_T == 4
    if ((mi_uint4korr(header+4) != 0 &&
	 (mi_uint4korr(header+4) != (ulong) ~0 ||
	  info->next_filepos != (ulong) ~0)) ||
	(mi_uint4korr(header+12) != 0 &&
	 (mi_uint4korr(header+12) != (ulong) ~0 ||
	  info->prev_filepos != (ulong) ~0)))
      goto err;
#endif
    return return_val | BLOCK_DELETED;		/* Deleted block */

  case 1:
    info->rec_len=info->data_len=info->block_len=mi_uint2korr(header+1);
    info->filepos=filepos+3;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 2:
    info->rec_len=info->data_len=info->block_len=mi_uint3korr(header+1);
    info->filepos=filepos+4;
    return return_val | BLOCK_FIRST | BLOCK_LAST;

  case 13:
    info->rec_len=mi_uint4korr(header+1);
    info->block_len=info->data_len=mi_uint3korr(header+5);
    info->next_filepos=mi_sizekorr(header+8);
    info->second_read=1;
    info->filepos=filepos+16;
    return return_val | BLOCK_FIRST;

  case 3:
    info->rec_len=info->data_len=mi_uint2korr(header+1);
    info->block_len=info->rec_len+ (uint) header[3];
    info->filepos=filepos+4;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 4:
    info->rec_len=info->data_len=mi_uint3korr(header+1);
    info->block_len=info->rec_len+ (uint) header[4];
    info->filepos=filepos+5;
    return return_val | BLOCK_FIRST | BLOCK_LAST;

  case 5:
    info->rec_len=mi_uint2korr(header+1);
    info->block_len=info->data_len=mi_uint2korr(header+3);
    info->next_filepos=mi_sizekorr(header+5);
    info->second_read=1;
    info->filepos=filepos+13;
    return return_val | BLOCK_FIRST;
  case 6:
    info->rec_len=mi_uint3korr(header+1);
    info->block_len=info->data_len=mi_uint3korr(header+4);
    info->next_filepos=mi_sizekorr(header+7);
    info->second_read=1;
    info->filepos=filepos+15;
    return return_val | BLOCK_FIRST;

    /* The following blocks are identical to 1-6 without rec_len */
  case 7:
    info->data_len=info->block_len=mi_uint2korr(header+1);
    info->filepos=filepos+3;
    return return_val | BLOCK_LAST;
  case 8:
    info->data_len=info->block_len=mi_uint3korr(header+1);
    info->filepos=filepos+4;
    return return_val | BLOCK_LAST;

  case 9:
    info->data_len=mi_uint2korr(header+1);
    info->block_len=info->data_len+ (uint) header[3];
    info->filepos=filepos+4;
    return return_val | BLOCK_LAST;
  case 10:
    info->data_len=mi_uint3korr(header+1);
    info->block_len=info->data_len+ (uint) header[4];
    info->filepos=filepos+5;
    return return_val | BLOCK_LAST;

  case 11:
    info->data_len=info->block_len=mi_uint2korr(header+1);
    info->next_filepos=mi_sizekorr(header+3);
    info->second_read=1;
    info->filepos=filepos+11;
    return return_val;
  case 12:
    info->data_len=info->block_len=mi_uint3korr(header+1);
    info->next_filepos=mi_sizekorr(header+4);
    info->second_read=1;
    info->filepos=filepos+12;
    return return_val;
  }

err:
  if (!handler->in_check_table)
  {
    /* We may be scanning the table for new rows; Don't give an error */
    _ma_set_fatal_error(handler->s, HA_ERR_WRONG_IN_RECORD);
  }
  return BLOCK_ERROR;
}
