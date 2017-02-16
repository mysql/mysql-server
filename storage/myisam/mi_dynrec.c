/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Functions to handle space-packed-records and blobs
 
  A row may be stored in one or more linked blocks.
  The block size is between MI_MIN_BLOCK_LENGTH and MI_MAX_BLOCK_LENGTH.
  Each block is aligned on MI_DYN_ALIGN_SIZE.
  The reson for the max block size is to not have too many different types
  of blocks.  For the differnet block types, look at _mi_get_block_info()
*/

#include "myisamdef.h"

/* Enough for comparing if number is zero */
static char zero_string[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int write_dynamic_record(MI_INFO *info,const uchar *record,
				ulong reclength);
static int _mi_find_writepos(MI_INFO *info,ulong reclength,my_off_t *filepos,
			     ulong *length);
static int update_dynamic_record(MI_INFO *info,my_off_t filepos,uchar *record,
				 ulong reclength);
static int delete_dynamic_record(MI_INFO *info,my_off_t filepos,
				 uint second_read);
static int _mi_cmp_buffer(File file, const uchar *buff, my_off_t filepos,
			  uint length);

/* Play it safe; We have a small stack when using threads */
#undef my_alloca
#undef my_afree
#define my_alloca(A) my_malloc((A),MYF(0))
#define my_afree(A) my_free((A))

	/* Interface function from MI_INFO */

#ifdef HAVE_MMAP

/*
  Create mmaped area for MyISAM handler

  SYNOPSIS
    mi_dynmap_file()
    info		MyISAM handler

  RETURN
    0  ok
    1  error.
*/

my_bool mi_dynmap_file(MI_INFO *info, my_off_t size)
{
  DBUG_ENTER("mi_dynmap_file");
  if (size == 0 || size > (my_off_t) (~((size_t) 0)))
  {
    if (size)
      DBUG_PRINT("warning", ("File is too large for mmap"));
    else
      DBUG_PRINT("warning", ("Do not mmap zero-length"));
    DBUG_RETURN(1);
  }
  /*
    I wonder if it is good to use MAP_NORESERVE. From the Linux man page:
    MAP_NORESERVE
      Do not reserve swap space for this mapping. When swap space is
      reserved, one has the guarantee that it is possible to modify the
      mapping. When swap space is not reserved one might get SIGSEGV
      upon a write if no physical memory is available.
  */
  info->s->file_map= (uchar*)
                  my_mmap(0, (size_t) size,
                          info->s->mode==O_RDONLY ? PROT_READ :
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_NORESERVE,
                          info->dfile, 0L);
  if (info->s->file_map == (uchar*) MAP_FAILED)
  {
    info->s->file_map= NULL;
    DBUG_RETURN(1);
  }
#if defined(HAVE_MADVISE)
  madvise((char*) info->s->file_map, size, MADV_RANDOM);
#endif
  info->s->mmaped_length= size;
  info->s->file_read= mi_mmap_pread;
  info->s->file_write= mi_mmap_pwrite;
  DBUG_RETURN(0);
}


/*
  Destroy mmaped area for MyISAM handler

  SYNOPSIS
    mi_munmap_file()
    info                  MyISAM handler

  RETURN
    0  ok
   !0  error.
*/

int mi_munmap_file(MI_INFO *info)
{
  int ret;
  DBUG_ENTER("mi_unmap_file");
  if ((ret= my_munmap(info->s->file_map, (size_t) info->s->mmaped_length)))
    DBUG_RETURN(ret);
  info->s->file_read= mi_nommap_pread;
  info->s->file_write= mi_nommap_pwrite;
  info->s->file_map= 0;
  info->s->mmaped_length= 0;
  DBUG_RETURN(0);
}


/*
  Resize mmaped area for MyISAM handler

  SYNOPSIS
    mi_remap_file()
    info		MyISAM handler

  RETURN
*/

void mi_remap_file(MI_INFO *info, my_off_t size)
{
  if (info->s->file_map)
  {
    mi_munmap_file(info);
    mi_dynmap_file(info, size);
  }
}
#endif


/*
  Read bytes from MySAM handler, using mmap or pread

  SYNOPSIS
    mi_mmap_pread()
    info		MyISAM handler
    Buffer              Input buffer
    Count               Count of bytes for read
    offset              Start position
    MyFlags             

  RETURN
    0  ok
*/

size_t mi_mmap_pread(MI_INFO *info, uchar *Buffer,
                    size_t Count, my_off_t offset, myf MyFlags)
{
  DBUG_PRINT("info", ("mi_read with mmap %d\n", info->dfile));
  if (info->s->concurrent_insert)
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
    if (info->s->concurrent_insert)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return 0;
  }
  else
  {
    if (info->s->concurrent_insert)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return mysql_file_pread(info->dfile, Buffer, Count, offset, MyFlags);
  }
}


        /* wrapper for mysql_file_pread in case if mmap isn't used */

size_t mi_nommap_pread(MI_INFO *info, uchar *Buffer,
                       size_t Count, my_off_t offset, myf MyFlags)
{
  return mysql_file_pread(info->dfile, Buffer, Count, offset, MyFlags);
}


/*
  Write bytes to MySAM handler, using mmap or pwrite

  SYNOPSIS
    mi_mmap_pwrite()
    info		MyISAM handler
    Buffer              Output buffer
    Count               Count of bytes for write
    offset              Start position
    MyFlags             

  RETURN
    0  ok
    !=0  error.  In this case return error from pwrite
*/

size_t mi_mmap_pwrite(MI_INFO *info, const uchar *Buffer,
                      size_t Count, my_off_t offset, myf MyFlags)
{
  DBUG_PRINT("info", ("mi_write with mmap %d\n", info->dfile));
  if (info->s->concurrent_insert)
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
    if (info->s->concurrent_insert)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return 0;
  }
  else
  {
    info->s->nonmmaped_inserts++;
    if (info->s->concurrent_insert)
      mysql_rwlock_unlock(&info->s->mmap_lock);
    return mysql_file_pwrite(info->dfile, Buffer, Count, offset, MyFlags);
  }

}


        /* wrapper for mysql_file_pwrite in case if mmap isn't used */

size_t mi_nommap_pwrite(MI_INFO *info, const uchar *Buffer,
                      size_t Count, my_off_t offset, myf MyFlags)
{
  return mysql_file_pwrite(info->dfile, Buffer, Count, offset, MyFlags);
}


int _mi_write_dynamic_record(MI_INFO *info, const uchar *record)
{
  ulong reclength=_mi_rec_pack(info,info->rec_buff,record);
  return (write_dynamic_record(info,info->rec_buff,reclength));
}

int _mi_update_dynamic_record(MI_INFO *info, my_off_t pos, const uchar *record)
{
  uint length=_mi_rec_pack(info,info->rec_buff,record);
  return (update_dynamic_record(info,pos,info->rec_buff,length));
}

int _mi_write_blob_record(MI_INFO *info, const uchar *record)
{
  uchar *rec_buff;
  int error;
  ulong reclength,reclength2,extra;

  extra= (ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
	  MI_DYN_DELETE_BLOCK_HEADER+1);
  reclength= (info->s->base.pack_reclength +
	      _mi_calc_total_blob_length(info,record)+ extra);
  if (!(rec_buff=(uchar*) my_alloca(reclength)))
  {
    my_errno= HA_ERR_OUT_OF_MEM; /* purecov: inspected */
    return(-1);
  }
  reclength2= _mi_rec_pack(info,rec_buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER),
			   record);
  DBUG_PRINT("info",("reclength: %lu  reclength2: %lu",
		     reclength, reclength2));
  DBUG_ASSERT(reclength2 <= reclength);
  error=write_dynamic_record(info,rec_buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER),
			     reclength2);
  my_afree(rec_buff);
  return(error);
}


int _mi_update_blob_record(MI_INFO *info, my_off_t pos, const uchar *record)
{
  uchar *rec_buff;
  int error;
  ulong reclength,extra;

  extra= (ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
	  MI_DYN_DELETE_BLOCK_HEADER);
  reclength= (info->s->base.pack_reclength+
	      _mi_calc_total_blob_length(info,record)+ extra);
  if (!(rec_buff=(uchar*) my_alloca(reclength)))
  {
    my_errno= HA_ERR_OUT_OF_MEM; /* purecov: inspected */
    return(-1);
  }
  reclength=_mi_rec_pack(info,rec_buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER),
			 record);
  error=update_dynamic_record(info,pos,
			      rec_buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER),
			      reclength);
  my_afree(rec_buff);
  return(error);
}


int _mi_delete_dynamic_record(MI_INFO *info)
{
  return delete_dynamic_record(info,info->lastpos,0);
}


	/* Write record to data-file */

static int write_dynamic_record(MI_INFO *info, const uchar *record,
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
    we always use MI_MAX_DYN_BLOCK_HEADER while it can be
    less in the most of the cases.
  */

  if (unlikely(info->s->base.max_data_file_length -
               info->state->data_file_length <
               reclength + MI_MAX_DYN_BLOCK_HEADER))
  {
    if (info->s->base.max_data_file_length - info->state->data_file_length +
        info->state->empty - info->state->del * MI_MAX_DYN_BLOCK_HEADER <
        reclength + MI_MAX_DYN_BLOCK_HEADER)
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(1);
    }
  }

  do
  {
    if (_mi_find_writepos(info,reclength,&filepos,&length))
      goto err;
    if (_mi_write_part_record(info,filepos,length,
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

static int _mi_find_writepos(MI_INFO *info,
			     ulong reclength, /* record length */
			     my_off_t *filepos, /* Return file pos */
			     ulong *length)   /* length of block at filepos */
{
  MI_BLOCK_INFO block_info;
  ulong tmp;
  DBUG_ENTER("_mi_find_writepos");

  if (info->s->state.dellink != HA_OFFSET_ERROR &&
      !info->append_insert_at_end)
  {
    /* Deleted blocks exists;  Get last used block */
    *filepos=info->s->state.dellink;
    block_info.second_read=0;
    info->rec_cache.seek_not_done=1;
    if (!(_mi_get_block_info(&block_info,info->dfile,info->s->state.dellink) &
	   BLOCK_DELETED))
    {
      DBUG_PRINT("error",("Delete link crashed"));
      my_errno=HA_ERR_WRONG_IN_RECORD;
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
      tmp= ((tmp+MI_DYN_ALIGN_SIZE-1) &
	    (~ (ulong) (MI_DYN_ALIGN_SIZE-1)));
    if (info->state->data_file_length >
	(info->s->base.max_data_file_length - tmp))
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(-1);
    }
    if (tmp > MI_MAX_BLOCK_LENGTH)
      tmp=MI_MAX_BLOCK_LENGTH;
    *length= tmp;
    info->state->data_file_length+= tmp;
    info->s->state.split++;
    info->update|=HA_STATE_WRITE_AT_END;
  }
  DBUG_RETURN(0);
} /* _mi_find_writepos */



/*
  Unlink a deleted block from the deleted list.
  This block will be combined with the preceding or next block to form
  a big block.
*/

static my_bool unlink_deleted_block(MI_INFO *info, MI_BLOCK_INFO *block_info)
{
  DBUG_ENTER("unlink_deleted_block");
  if (block_info->filepos == info->s->state.dellink)
  {
    /* First deleted block;  We can just use this ! */
    info->s->state.dellink=block_info->next_filepos;
  }
  else
  {
    MI_BLOCK_INFO tmp;
    tmp.second_read=0;
    /* Unlink block from the previous block */
    if (!(_mi_get_block_info(&tmp,info->dfile,block_info->prev_filepos)
	  & BLOCK_DELETED))
      DBUG_RETURN(1);				/* Something is wrong */
    mi_sizestore(tmp.header+4,block_info->next_filepos);
    if (info->s->file_write(info, tmp.header+4,8,
		  block_info->prev_filepos+4, MYF(MY_NABP)))
      DBUG_RETURN(1);
    /* Unlink block from next block */
    if (block_info->next_filepos != HA_OFFSET_ERROR)
    {
      if (!(_mi_get_block_info(&tmp,info->dfile,block_info->next_filepos)
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
    (mi_rrnd() or mi_scan(), then ensure that we skip over this block
    when doing next mi_rrnd() or mi_scan().
  */
  if (info->nextpos == block_info->filepos)
    info->nextpos+=block_info->block_len;
  DBUG_RETURN(0);
}


/*
  Add a backward link to delete block

  SYNOPSIS
    update_backward_delete_link()
    info		MyISAM handler
    delete_block	Position to delete block to update.
			If this is 'HA_OFFSET_ERROR', nothing will be done
    filepos		Position to block that 'delete_block' should point to

  RETURN
    0  ok
    1  error.  In this case my_error is set.
*/

static int update_backward_delete_link(MI_INFO *info, my_off_t delete_block,
				       my_off_t filepos)
{
  MI_BLOCK_INFO block_info;
  DBUG_ENTER("update_backward_delete_link");

  if (delete_block != HA_OFFSET_ERROR)
  {
    block_info.second_read=0;
    if (_mi_get_block_info(&block_info,info->dfile,delete_block)
	& BLOCK_DELETED)
    {
      uchar buff[8];
      mi_sizestore(buff,filepos);
      if (info->s->file_write(info,buff, 8, delete_block+12, MYF(MY_NABP)))
	DBUG_RETURN(1);				/* Error on write */
    }
    else
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      DBUG_RETURN(1);				/* Wrong delete link */
    }
  }
  DBUG_RETURN(0);
}

	/* Delete datarecord from database */
	/* info->rec_cache.seek_not_done is updated in cmp_record */

static int delete_dynamic_record(MI_INFO *info, my_off_t filepos,
				 uint second_read)
{
  uint length,b_type;
  MI_BLOCK_INFO block_info,del_block;
  int error;
  my_bool remove_next_block;
  DBUG_ENTER("delete_dynamic_record");

  /* First add a link from the last block to the new one */
  error= update_backward_delete_link(info, info->s->state.dellink, filepos);

  block_info.second_read=second_read;
  do
  {
    /* Remove block at 'filepos' */
    if ((b_type=_mi_get_block_info(&block_info,info->dfile,filepos))
	& (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	   BLOCK_FATAL_ERROR) ||
	(length=(uint) (block_info.filepos-filepos) +block_info.block_len) <
	MI_MIN_BLOCK_LENGTH)
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      DBUG_RETURN(1);
    }
    /* Check if next block is a delete block */
    del_block.second_read=0;
    remove_next_block=0;
    if (_mi_get_block_info(&del_block,info->dfile,filepos+length) &
	BLOCK_DELETED && del_block.block_len+length < MI_DYN_MAX_BLOCK_LENGTH)
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
    if (info->s->file_write(info,(uchar*) block_info.header,20,filepos,
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

int _mi_write_part_record(MI_INFO *info,
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
  uchar temp[MI_SPLIT_LENGTH+MI_DYN_DELETE_BLOCK_HEADER];
  DBUG_ENTER("_mi_write_part_record");

  next_delete_block=HA_OFFSET_ERROR;

  res_length=extra_length=0;
  if (length > *reclength + MI_SPLIT_LENGTH)
  {						/* Splitt big block */
    res_length=MY_ALIGN(length- *reclength - MI_EXTEND_BLOCK_LENGTH,
			MI_DYN_ALIGN_SIZE);
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
      if (*reclength > MI_MAX_BLOCK_LENGTH)
      {
	head_length= 16;
	temp[0]=13;
	mi_int4store(temp+1,*reclength);
	mi_int3store(temp+5,length-head_length);
	mi_sizestore((uchar*) temp+8,next_filepos);
      }
      else
      {
	head_length=5+8+long_block*2;
	temp[0]=5+(uchar) long_block;
	if (long_block)
	{
	  mi_int3store(temp+1,*reclength);
	  mi_int3store(temp+4,length-head_length);
	  mi_sizestore((uchar*) temp+7,next_filepos);
	}
	else
	{
	  mi_int2store(temp+1,*reclength);
	  mi_int2store(temp+3,length-head_length);
	  mi_sizestore((uchar*) temp+5,next_filepos);
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
	mi_sizestore((uchar*) temp+4,next_filepos);
      }
      else
      {
	mi_int2store(temp+1,length-head_length);
	mi_sizestore((uchar*) temp+3,next_filepos);
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
    length=	  *reclength+head_length;	/* Write only what is needed */
  }
  DBUG_DUMP("header",(uchar*) temp,head_length);

	/* Make a long block for one write */
  record_end= *record+length-head_length;
  del_length=(res_length ? MI_DYN_DELETE_BLOCK_HEADER : 0);
  bmove((uchar*) (*record-head_length),(uchar*) temp,head_length);
  memcpy(temp,record_end,(size_t) (extra_length+del_length));
  bzero((uchar*) record_end,extra_length);

  if (res_length)
  {
    /* Check first if we can join this block with the next one */
    MI_BLOCK_INFO del_block;
    my_off_t next_block=filepos+length+extra_length+res_length;

    del_block.second_read=0;
    if (next_block < info->state->data_file_length &&
	info->s->state.dellink != HA_OFFSET_ERROR)
    {
      if ((_mi_get_block_info(&del_block,info->dfile,next_block)
	   & BLOCK_DELETED) &&
	  res_length + del_block.block_len < MI_DYN_MAX_BLOCK_LENGTH)
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
      if (my_block_write(&info->rec_cache,(uchar*) *record-head_length,
			 length+extra_length+del_length,filepos))
      goto err;
    }
    else if (my_b_write(&info->rec_cache,(uchar*) *record-head_length,
			length+extra_length+del_length))
      goto err;
  }
  else
  {
    info->rec_cache.seek_not_done=1;
    if (info->s->file_write(info,(uchar*) *record-head_length,length+extra_length+
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
} /*_mi_write_part_record */


	/* update record from datafile */

static int update_dynamic_record(MI_INFO *info, my_off_t filepos, uchar *record,
				 ulong reclength)
{
  int flag;
  uint error;
  ulong length;
  MI_BLOCK_INFO block_info;
  DBUG_ENTER("update_dynamic_record");

  flag=block_info.second_read=0;
  /*
     Check if we have enough room for the record.
     First we do simplified check to make usual case faster.
     Then we do more precise check for the space left.
     Though it still is not absolutely precise, as
     we always use MI_MAX_DYN_BLOCK_HEADER while it can be
     less in the most of the cases.
  */

  /*
    compare with just the reclength as we're going
    to get some space from the old replaced record
  */
  if (unlikely(info->s->base.max_data_file_length -
        info->state->data_file_length < reclength))
  {
    /*
       let's read the old record's block to find out the length of the
       old record
    */
    if ((error=_mi_get_block_info(&block_info,info->dfile,filepos))
        & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR | BLOCK_FATAL_ERROR))
    {
      DBUG_PRINT("error",("Got wrong block info"));
      if (!(error & BLOCK_FATAL_ERROR))
        my_errno=HA_ERR_WRONG_IN_RECORD;
      goto err;
    }

    /*
      if new record isn't longer, we can go on safely
    */
    if (block_info.rec_len < reclength)
    {
      if (info->s->base.max_data_file_length - info->state->data_file_length +
          info->state->empty - info->state->del * MI_MAX_DYN_BLOCK_HEADER <
          reclength - block_info.rec_len + MI_MAX_DYN_BLOCK_HEADER)
      {
        my_errno=HA_ERR_RECORD_FILE_FULL;
        goto err;
      }
    }
    block_info.second_read=0;
  }

  while (reclength > 0)
  {
    if (filepos != info->s->state.dellink)
    {
      block_info.next_filepos= HA_OFFSET_ERROR;
      if ((error=_mi_get_block_info(&block_info,info->dfile,filepos))
	  & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	     BLOCK_FATAL_ERROR))
      {
	DBUG_PRINT("error",("Got wrong block info"));
	if (!(error & BLOCK_FATAL_ERROR))
	  my_errno=HA_ERR_WRONG_IN_RECORD;
	goto err;
      }
      length=(ulong) (block_info.filepos-filepos) + block_info.block_len;
      if (length < reclength)
      {
	uint tmp=MY_ALIGN(reclength - length + 3 +
			  test(reclength >= 65520L),MI_DYN_ALIGN_SIZE);
	/* Don't create a block bigger than MI_MAX_BLOCK_LENGTH */
	tmp= min(length+tmp, MI_MAX_BLOCK_LENGTH)-length;
	/* Check if we can extend this block */
	if (block_info.filepos + block_info.block_len ==
	    info->state->data_file_length &&
	    info->state->data_file_length <
	    info->s->base.max_data_file_length-tmp)
	{
	  /* extend file */
	  DBUG_PRINT("info",("Extending file with %d bytes",tmp));
	  if (info->nextpos == info->state->data_file_length)
	    info->nextpos+= tmp;
	  info->state->data_file_length+= tmp;
	  info->update|= HA_STATE_WRITE_AT_END | HA_STATE_EXTEND_BLOCK;
	  length+=tmp;
	}
	else if (length < MI_MAX_BLOCK_LENGTH - MI_MIN_BLOCK_LENGTH)
	{
	  /*
	    Check if next block is a deleted block
	    Above we have MI_MIN_BLOCK_LENGTH to avoid the problem where
	    the next block is so small it can't be splited which could
	    casue problems
	  */

	  MI_BLOCK_INFO del_block;
	  del_block.second_read=0;
	  if (_mi_get_block_info(&del_block,info->dfile,
				 block_info.filepos + block_info.block_len) &
	      BLOCK_DELETED)
	  {
	    /* Use; Unlink it and extend the current block */
	    DBUG_PRINT("info",("Extending current block"));
	    if (unlink_deleted_block(info,&del_block))
	      goto err;
	    if ((length+=del_block.block_len) > MI_MAX_BLOCK_LENGTH)
	    {
	      /*
		New block was too big, link overflow part back to
		delete list
	      */
	      my_off_t next_pos;
	      ulong rest_length= length-MI_MAX_BLOCK_LENGTH;
	      set_if_bigger(rest_length, MI_MIN_BLOCK_LENGTH);
	      next_pos= del_block.filepos+ del_block.block_len - rest_length;

	      if (update_backward_delete_link(info, info->s->state.dellink,
					      next_pos))
		DBUG_RETURN(1);

	      /* create delete link for data that didn't fit into the page */
	      del_block.header[0]=0;
	      mi_int3store(del_block.header+1, rest_length);
	      mi_sizestore(del_block.header+4,info->s->state.dellink);
	      bfill(del_block.header+12,8,255);
	      if (info->s->file_write(info,(uchar*) del_block.header,20, next_pos,
			    MYF(MY_NABP)))
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
      if (_mi_find_writepos(info,reclength,&filepos,&length))
	goto err;
    }
    if (_mi_write_part_record(info,filepos,length,block_info.next_filepos,
			      &record,&reclength,&flag))
      goto err;
    if ((filepos=block_info.next_filepos) == HA_OFFSET_ERROR)
    {
      /* Start writing data on deleted blocks */
      filepos=info->s->state.dellink;
    }
  }

  if (block_info.next_filepos != HA_OFFSET_ERROR)
  {
    /*
      delete_dynamic_record() may change data file position.
      IO cache must be notified as it may still have cached
      data, which has to be flushed later.
    */
    info->rec_cache.seek_not_done= 1;
    if (delete_dynamic_record(info,block_info.next_filepos,1))
      goto err;
  }
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


	/* Pack a record. Return new reclength */

uint _mi_rec_pack(MI_INFO *info, register uchar *to,
                  register const uchar *from)
{
  uint		length,new_length,flag,bit,i;
  uchar		*pos,*end,*startpos,*packpos;
  enum en_fieldtype type;
  reg3 MI_COLUMNDEF *rec;
  MI_BLOB	*blob;
  DBUG_ENTER("_mi_rec_pack");

  flag=0 ; bit=1;
  startpos=packpos=to; to+= info->s->base.pack_bits; blob=info->blobs;
  rec=info->s->rec;

  for (i=info->s->base.fields ; i-- > 0; from+= length,rec++)
  {
    length=(uint) rec->length;
    if ((type = (enum en_fieldtype) rec->type) != FIELD_NORMAL)
    {
      if (type == FIELD_BLOB)
      {
	if (!blob->length)
	  flag|=bit;
	else
	{
	  char *temp_pos;
	  size_t tmp_length=length-portable_sizeof_char_ptr;
	  memcpy((uchar*) to,from,tmp_length);
	  memcpy(&temp_pos,from+tmp_length,sizeof(char*));
	  memcpy(to+tmp_length,temp_pos,(size_t) blob->length);
	  to+=tmp_length+blob->length;
	}
	blob++;
      }
      else if (type == FIELD_SKIP_ZERO)
      {
	if (memcmp((uchar*) from,zero_string,length) == 0)
	  flag|=bit;
	else
	{
	  memcpy((uchar*) to,from,(size_t) length); to+=length;
	}
      }
      else if (type == FIELD_SKIP_ENDSPACE ||
	       type == FIELD_SKIP_PRESPACE)
      {
	pos= (uchar*) from; end= (uchar*) from + length;
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
	if (new_length +1 + test(rec->length > 255 && new_length > 127)
	    < length)
	{
	  if (rec->length > 255 && new_length > 127)
	  {
            to[0]= (uchar) ((new_length & 127) + 128);
            to[1]= (uchar) (new_length >> 7);
            to+=2;
          }
          else
            *to++= (uchar) new_length;
	  memcpy((uchar*) to,pos,(size_t) new_length); to+=new_length;
	  flag|=bit;
	}
	else
	{
	  memcpy(to,from,(size_t) length); to+=length;
	}
      }
      else if (type == FIELD_VARCHAR)
      {
        uint pack_length= HA_VARCHAR_PACKLENGTH(rec->length -1);
	uint tmp_length;
        if (pack_length == 1)
        {
          tmp_length= (uint) *(uchar*) from;
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
        *packpos++= (uchar) flag;
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
    *to++= (uchar) info->checksum;
  DBUG_PRINT("exit",("packed length: %d",(int) (to-startpos)));
  DBUG_RETURN((uint) (to-startpos));
} /* _mi_rec_pack */



/*
  Check if a record was correctly packed. Used only by myisamchk
  Returns 0 if record is ok.
*/

my_bool _mi_rec_check(MI_INFO *info,const uchar *record, uchar *rec_buff,
                      ulong packed_length, my_bool with_checksum)
{
  uint		length,new_length,flag,bit,i;
  uchar		*pos,*end,*packpos,*to;
  enum en_fieldtype type;
  reg3 MI_COLUMNDEF *rec;
  DBUG_ENTER("_mi_rec_check");

  packpos=rec_buff; to= rec_buff+info->s->base.pack_bits;
  rec=info->s->rec;
  flag= *packpos; bit=1;

  for (i=info->s->base.fields ; i-- > 0; record+= length, rec++)
  {
    length=(uint) rec->length;
    if ((type = (enum en_fieldtype) rec->type) != FIELD_NORMAL)
    {
      if (type == FIELD_BLOB)
      {
	uint blob_length=
	  _mi_calc_blob_length(length-portable_sizeof_char_ptr,record);
	if (!blob_length && !(flag & bit))
	  goto err;
	if (blob_length)
	  to+=length - portable_sizeof_char_ptr+ blob_length;
      }
      else if (type == FIELD_SKIP_ZERO)
      {
	if (memcmp((uchar*) record,zero_string,length) == 0)
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
	pos= (uchar*) record; end= (uchar*) record + length;
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
	if (new_length +1 + test(rec->length > 255 && new_length > 127)
	    < length)
	{
	  if (!(flag & bit))
	    goto err;
	  if (rec->length > 255 && new_length > 127)
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
        uint pack_length= HA_VARCHAR_PACKLENGTH(rec->length -1);
	uint tmp_length;
        if (pack_length == 1)
        {
          tmp_length= (uint) *(uchar*) record;
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
  if (packed_length != (uint) (to - rec_buff) + test(info->s->calc_checksum) ||
      (bit != 1 && (flag & ~(bit - 1))))
    goto err;
  if (with_checksum && ((uchar) info->checksum != (uchar) *to))
  {
    DBUG_PRINT("error",("wrong checksum for row"));
    goto err;
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}



	/* Unpacks a record */
	/* Returns -1 and my_errno =HA_ERR_RECORD_DELETED if reclength isn't */
	/* right. Returns reclength (>0) if ok */

ulong _mi_rec_unpack(register MI_INFO *info, register uchar *to, uchar *from,
		     ulong found_length)
{
  uint flag,bit,length,rec_length,min_pack_length;
  enum en_fieldtype type;
  uchar *from_end,*to_end,*packpos;
  reg3 MI_COLUMNDEF *rec,*end_field;
  DBUG_ENTER("_mi_rec_unpack");

  to_end=to + info->s->base.reclength;
  from_end=from+found_length;
  flag= (uchar) *from; bit=1; packpos=from;
  if (found_length < info->s->base.min_pack_length)
    goto err;
  from+= info->s->base.pack_bits;
  min_pack_length=info->s->base.min_pack_length - info->s->base.pack_bits;

  for (rec=info->s->rec , end_field=rec+info->s->base.fields ;
       rec < end_field ; to+= rec_length, rec++)
  {
    rec_length=rec->length;
    if ((type = (enum en_fieldtype) rec->type) != FIELD_NORMAL &&
	(type != FIELD_CHECK))
    {
      if (type == FIELD_VARCHAR)
      {
        uint pack_length= HA_VARCHAR_PACKLENGTH(rec_length-1);
        if (pack_length == 1)
        {
          length= (uint) *(uchar*) from;
          if (length > rec_length-1)
            goto err;
          *to= *from++;
        }
        else
        {
          get_key_length(length, from);
          if (length > rec_length-2)
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
	  bzero((uchar*) to,rec_length);
	else if (type == FIELD_SKIP_ENDSPACE ||
		 type == FIELD_SKIP_PRESPACE)
	{
	  if (rec->length > 255 && *from & 128)
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
	  if (length >= rec_length ||
	      min_pack_length + length > (uint) (from_end - from))
	    goto err;
	  if (type == FIELD_SKIP_ENDSPACE)
	  {
	    memcpy(to,(uchar*) from,(size_t) length);
	    bfill((uchar*) to+length,rec_length-length,' ');
	  }
	  else
	  {
	    bfill((uchar*) to,rec_length-length,' ');
	    memcpy(to+rec_length-length,(uchar*) from,(size_t) length);
	  }
	  from+=length;
	}
      }
      else if (type == FIELD_BLOB)
      {
	uint size_length=rec_length- portable_sizeof_char_ptr;
	ulong blob_length=_mi_calc_blob_length(size_length,from);
        ulong from_left= (ulong) (from_end - from);
        if (from_left < size_length ||
            from_left - size_length < blob_length ||
            from_left - size_length - blob_length < min_pack_length)
          goto err;
	memcpy(to, from, (size_t) size_length);
	from+=size_length;
	memcpy(to+size_length, &from, sizeof(char*));
	from+=blob_length;
      }
      else
      {
	if (type == FIELD_SKIP_ENDSPACE || type == FIELD_SKIP_PRESPACE)
	  min_pack_length--;
	if (min_pack_length + rec_length > (uint) (from_end - from))
	  goto err;
	memcpy(to,(uchar*) from,(size_t) rec_length); from+=rec_length;
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
      min_pack_length-=rec_length;
      memcpy(to, (uchar*) from, (size_t) rec_length);
      from+=rec_length;
    }
  }
  if (info->s->calc_checksum)
    from++;
  if (to == to_end && from == from_end && (bit == 1 || !(flag & ~(bit-1))))
    DBUG_RETURN(found_length);

err:
  my_errno= HA_ERR_WRONG_IN_RECORD;
  DBUG_PRINT("error",("to_end: 0x%lx -> 0x%lx  from_end: 0x%lx -> 0x%lx",
		      (long) to, (long) to_end, (long) from, (long) from_end));
  DBUG_DUMP("from",(uchar*) info->rec_buff,info->s->base.min_pack_length);
  DBUG_RETURN(MY_FILE_ERROR);
} /* _mi_rec_unpack */


	/* Calc length of blob. Update info in blobs->length */

ulong _mi_calc_total_blob_length(MI_INFO *info, const uchar *record)
{
  ulong length;
  MI_BLOB *blob,*end;

  for (length=0, blob= info->blobs, end=blob+info->s->base.blobs ;
       blob != end;
       blob++)
  {
    blob->length=_mi_calc_blob_length(blob->pack_length,record + blob->offset);
    length+=blob->length;
  }
  return length;
}


ulong _mi_calc_blob_length(uint length, const uchar *pos)
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


void _mi_store_blob_length(uchar *pos,uint pack_length,uint length)
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
    _mi_read_dynamic_record()
      info                      MI_INFO pointer to table.
      filepos                   From where to read the record.
      buf                       Destination for record.

  NOTE

    If a write buffer is active, it needs to be flushed if its contents
    intersects with the record to read. We always check if the position
    of the first byte of the write buffer is lower than the position
    past the last byte to read. In theory this is also true if the write
    buffer is completely below the read segment. That is, if there is no
    intersection. But this case is unusual. We flush anyway. Only if the
    first byte in the write buffer is above the last byte to read, we do
    not flush.

    A dynamic record may need several reads. So this check must be done
    before every read. Reading a dynamic record starts with reading the
    block header. If the record does not fit into the free space of the
    header, the block may be longer than the header. In this case a
    second read is necessary. These one or two reads repeat for every
    part of the record.

  RETURN
    0           OK
    -1          Error
*/

int _mi_read_dynamic_record(MI_INFO *info, my_off_t filepos, uchar *buf)
{
  int block_of_record;
  uint b_type,UNINIT_VAR(left_length);
  uchar *UNINIT_VAR(to);
  MI_BLOCK_INFO block_info;
  File file;
  DBUG_ENTER("mi_read_dynamic_record");

  if (filepos != HA_OFFSET_ERROR)
  {
    file=info->dfile;
    block_of_record= 0;   /* First block of record is numbered as zero. */
    block_info.second_read= 0;
    do
    {
      /* A corrupted table can have wrong pointers. (Bug# 19835) */
      if (filepos == HA_OFFSET_ERROR)
        goto panic;
      if (info->opt_flag & WRITE_CACHE_USED &&
	  info->rec_cache.pos_in_file < filepos + MI_BLOCK_INFO_HEADER_LENGTH &&
	  flush_io_cache(&info->rec_cache))
	goto err;
      info->rec_cache.seek_not_done=1;
      if ((b_type= _mi_get_block_info(&block_info, file, filepos))
	  & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	     BLOCK_FATAL_ERROR))
      {
	if (b_type & (BLOCK_SYNC_ERROR | BLOCK_DELETED))
	  my_errno=HA_ERR_RECORD_DELETED;
	goto err;
      }
      if (block_of_record++ == 0)			/* First block */
      {
	if (block_info.rec_len > (uint) info->s->base.max_pack_length)
	  goto panic;
	if (info->s->base.blobs)
	{
	  if (!(to=mi_alloc_rec_buff(info, block_info.rec_len,
				     &info->rec_buff)))
	    goto err;
	}
	else
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
          memcpy((uchar*) to, block_info.header + offset, prefetch_len);
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
        if (info->s->file_read(info, (uchar*) to, block_info.data_len,
                               filepos, MYF(MY_NABP)))
          goto panic;
        left_length-=block_info.data_len;
        to+=block_info.data_len;
      }
      filepos= block_info.next_filepos;
    } while (left_length);

    info->update|= HA_STATE_AKTIV;	/* We have a aktive record */
    fast_mi_writeinfo(info);
    DBUG_RETURN(_mi_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
		MY_FILE_ERROR ? 0 : -1);
  }
  fast_mi_writeinfo(info);
  DBUG_RETURN(-1);			/* Wrong data to read */

panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;
err:
  (void) _mi_writeinfo(info,0);
  DBUG_RETURN(-1);
}

	/* compare unique constraint between stored rows */

int _mi_cmp_dynamic_unique(MI_INFO *info, MI_UNIQUEDEF *def,
			   const uchar *record, my_off_t pos)
{
  uchar *rec_buff,*old_record;
  int error;
  DBUG_ENTER("_mi_cmp_dynamic_unique");

  if (!(old_record=my_alloca(info->s->base.reclength)))
    DBUG_RETURN(1);

  /* Don't let the compare destroy blobs that may be in use */
  rec_buff=info->rec_buff;
  if (info->s->base.blobs)
    info->rec_buff=0;
  error=_mi_read_dynamic_record(info,pos,old_record);
  if (!error)
    error=mi_unique_comp(def, record, old_record, def->null_are_equal);
  if (info->s->base.blobs)
  {
    my_free(mi_get_rec_buff_ptr(info, info->rec_buff));
    info->rec_buff=rec_buff;
  }
  my_afree(old_record);
  DBUG_RETURN(error);
}


	/* Compare of record one disk with packed record in memory */

int _mi_cmp_dynamic_record(register MI_INFO *info, register const uchar *record)
{
  uint flag,reclength,b_type;
  my_off_t filepos;
  uchar *buffer;
  MI_BLOCK_INFO block_info;
  DBUG_ENTER("_mi_cmp_dynamic_record");

  if (info->opt_flag & WRITE_CACHE_USED)
  {
    info->update&= ~(HA_STATE_WRITE_AT_END | HA_STATE_EXTEND_BLOCK);
    if (flush_io_cache(&info->rec_cache))
      DBUG_RETURN(-1);
  }
  info->rec_cache.seek_not_done=1;

	/* If nobody have touched the database we don't have to test rec */

  buffer=info->rec_buff;
  if ((info->opt_flag & READ_CHECK_USED))
  {						/* If check isn't disabled  */
    if (info->s->base.blobs)
    {
      if (!(buffer=(uchar*) my_alloca(info->s->base.pack_reclength+
				     _mi_calc_total_blob_length(info,record))))
	DBUG_RETURN(-1);
    }
    reclength=_mi_rec_pack(info,buffer,record);
    record= buffer;

    filepos=info->lastpos;
    flag=block_info.second_read=0;
    block_info.next_filepos=filepos;
    while (reclength > 0)
    {
      if ((b_type=_mi_get_block_info(&block_info,info->dfile,
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
	my_errno=HA_ERR_WRONG_IN_RECORD;
	goto err;
      }
      reclength-=block_info.data_len;
      if (_mi_cmp_buffer(info->dfile,record,block_info.filepos,
			 block_info.data_len))
      {
	my_errno=HA_ERR_RECORD_CHANGED;
	goto err;
      }
      flag=1;
      record+=block_info.data_len;
    }
  }
  my_errno=0;
err:
  if (buffer != info->rec_buff)
    my_afree((uchar*) buffer);
  DBUG_RETURN(my_errno);
}


	/* Compare file to buffert */

static int _mi_cmp_buffer(File file, const uchar *buff, my_off_t filepos,
			  uint length)
{
  uint next_length;
  uchar temp_buff[IO_SIZE*2];
  DBUG_ENTER("_mi_cmp_buffer");

  next_length= IO_SIZE*2 - (uint) (filepos & (IO_SIZE-1));

  while (length > IO_SIZE*2)
  {
    if (mysql_file_pread(file, temp_buff, next_length, filepos, MYF(MY_NABP)) ||
	memcmp(buff, temp_buff, next_length))
      goto err;
    filepos+=next_length;
    buff+=next_length;
    length-= next_length;
    next_length=IO_SIZE*2;
  }
  if (mysql_file_pread(file, temp_buff, length, filepos, MYF(MY_NABP)))
    goto err;
  DBUG_RETURN(memcmp(buff,temp_buff,length));
err:
  DBUG_RETURN(1);
}


/*
  Read record from datafile.

  SYNOPSIS
    _mi_read_rnd_dynamic_record()
      info                      MI_INFO pointer to table.
      buf                       Destination for record.
      filepos                   From where to read the record.
      skip_deleted_blocks       If to repeat reading until a non-deleted
                                record is found.

  NOTE

    If a write buffer is active, it needs to be flushed if its contents
    intersects with the record to read. We always check if the position
    of the first byte of the write buffer is lower than the position
    past the last byte to read. In theory this is also true if the write
    buffer is completely below the read segment. That is, if there is no
    intersection. But this case is unusual. We flush anyway. Only if the
    first byte in the write buffer is above the last byte to read, we do
    not flush.

    A dynamic record may need several reads. So this check must be done
    before every read. Reading a dynamic record starts with reading the
    block header. If the record does not fit into the free space of the
    header, the block may be longer than the header. In this case a
    second read is necessary. These one or two reads repeat for every
    part of the record.

  RETURN
    0           OK
    != 0        Error
*/

int _mi_read_rnd_dynamic_record(MI_INFO *info, uchar *buf,
				register my_off_t filepos,
				my_bool skip_deleted_blocks)
{
  int block_of_record, info_read, save_errno;
  uint left_len,b_type;
  uchar *UNINIT_VAR(to);
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_read_rnd_dynamic_record");

  info_read=0;

  if (info->lock_type == F_UNLCK)
  {
#ifndef UNSAFE_LOCKING
    if (share->tot_locks == 0)
    {
      if (my_lock(share->kfile,F_RDLCK,0L,F_TO_EOF,
		  MYF(MY_SEEK_NOT_DONE) | info->lock_wait))
	DBUG_RETURN(my_errno);
    }
#else
    info->tmp_lock_type=F_RDLCK;
#endif
  }
  else
    info_read=1;				/* memory-keyinfoblock is ok */

  block_of_record= 0;   /* First block of record is numbered as zero. */
  block_info.second_read= 0;
  left_len=1;
  do
  {
    if (filepos >= info->state->data_file_length)
    {
      if (!info_read)
      {						/* Check if changed */
	info_read=1;
	info->rec_cache.seek_not_done=1;
	if (mi_state_info_read_dsk(share->kfile,&share->state,1))
	  goto panic;
      }
      if (filepos >= info->state->data_file_length)
      {
	my_errno= HA_ERR_END_OF_FILE;
	goto err;
      }
    }
    if (info->opt_flag & READ_CACHE_USED)
    {
      if (_mi_read_cache(&info->rec_cache,(uchar*) block_info.header,filepos,
			 sizeof(block_info.header),
			 (!block_of_record && skip_deleted_blocks ?
                          READING_NEXT : 0) | READING_HEADER))
	goto panic;
      b_type=_mi_get_block_info(&block_info,-1,filepos);
    }
    else
    {
      if (info->opt_flag & WRITE_CACHE_USED &&
	  info->rec_cache.pos_in_file < filepos + MI_BLOCK_INFO_HEADER_LENGTH &&
	  flush_io_cache(&info->rec_cache))
	DBUG_RETURN(my_errno);
      info->rec_cache.seek_not_done=1;
      b_type=_mi_get_block_info(&block_info,info->dfile,filepos);
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
	my_errno=HA_ERR_RECORD_DELETED;
	info->lastpos=block_info.filepos;
	info->nextpos=block_info.filepos+block_info.block_len;
      }
      goto err;
    }
    if (block_of_record == 0)				/* First block */
    {
      if (block_info.rec_len > (uint) share->base.max_pack_length)
	goto panic;
      info->lastpos=filepos;
      if (share->base.blobs)
      {
	if (!(to= mi_alloc_rec_buff(info, block_info.rec_len,
				    &info->rec_buff)))
	  goto err;
      }
      else
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
	memcpy((uchar*) to, block_info.header+offset,tmp_length);
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
	if (_mi_read_cache(&info->rec_cache,(uchar*) to,filepos,
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
        /* mysql_file_seek(info->dfile, filepos, MY_SEEK_SET, MYF(0)); */
        if (mysql_file_read(info->dfile, (uchar*) to, block_info.data_len,
                            MYF(MY_NABP)))
	{
	  if (my_errno == HA_ERR_FILE_TOO_SHORT)
	    my_errno= HA_ERR_WRONG_IN_RECORD;	/* Unexpected end of file */
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
      info->nextpos= block_info.filepos + block_info.block_len;
      skip_deleted_blocks= 0;
    }
    left_len-=block_info.data_len;
    to+=block_info.data_len;
    filepos=block_info.next_filepos;
  } while (left_len);

  info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;
  fast_mi_writeinfo(info);
  if (_mi_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
      MY_FILE_ERROR)
    DBUG_RETURN(0);
  DBUG_RETURN(my_errno);			/* Wrong record */

panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;		/* Something is fatal wrong */
err:
  save_errno=my_errno;
  (void) _mi_writeinfo(info,0);
  DBUG_RETURN(my_errno=save_errno);
}


	/* Read and process header from a dynamic-record-file */

uint _mi_get_block_info(MI_BLOCK_INFO *info, File file, my_off_t filepos)
{
  uint return_val=0;
  uchar *header=info->header;

  if (file >= 0)
  {
    /*
      We do not use mysql_file_pread() here because we want to have the file
      pointer set to the end of the header after this function.
      mysql_file_pread() may leave the file pointer untouched.
    */
    mysql_file_seek(file, filepos, MY_SEEK_SET, MYF(0));
    if (mysql_file_read(file, header, sizeof(info->header), MYF(0)) !=
	sizeof(info->header))
      goto err;
  }
  DBUG_DUMP("header",header,MI_BLOCK_INFO_HEADER_LENGTH);
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
	MI_MIN_BLOCK_LENGTH ||
	(info->block_len & (MI_DYN_ALIGN_SIZE -1)))
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
  my_errno=HA_ERR_WRONG_IN_RECORD;	 /* Garbage */
  return BLOCK_ERROR;
}
