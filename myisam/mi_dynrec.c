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

	/* Functions to handle space-packed-records and blobs */

#include "myisamdef.h"

/* Enough for comparing if number is zero */
static char zero_string[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int write_dynamic_record(MI_INFO *info,const byte *record,
				ulong reclength);
static int _mi_find_writepos(MI_INFO *info,ulong reclength,my_off_t *filepos,
			     ulong *length);
static int update_dynamic_record(MI_INFO *info,my_off_t filepos,byte *record,
				 ulong reclength);
static int delete_dynamic_record(MI_INFO *info,my_off_t filepos,
				 uint second_read);
static int _mi_cmp_buffer(File file, const byte *buff, my_off_t filepos,
			  uint length);

#ifdef THREAD
/* Play it safe; We have a small stack when using threads */
#undef my_alloca
#undef my_afree
#define my_alloca(A) my_malloc((A),MYF(0))
#define my_afree(A) my_free((A),MYF(0))
#endif

	/* Interface function from MI_INFO */

int _mi_write_dynamic_record(MI_INFO *info, const byte *record)
{
  ulong reclength=_mi_rec_pack(info,info->rec_buff,record);
  return (write_dynamic_record(info,info->rec_buff,reclength));
}

int _mi_update_dynamic_record(MI_INFO *info, my_off_t pos, const byte *record)
{
  uint length=_mi_rec_pack(info,info->rec_buff,record);
  return (update_dynamic_record(info,pos,info->rec_buff,length));
}

int _mi_write_blob_record(MI_INFO *info, const byte *record)
{
  byte *rec_buff;
  int error;
  ulong reclength,extra;

  extra=ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
    MI_DYN_DELETE_BLOCK_HEADER+1;
  reclength=info->s->base.pack_reclength+
    _my_calc_total_blob_length(info,record)+ extra;
  if (reclength > MI_DYN_MAX_ROW_LENGTH)
  {
    my_errno=HA_ERR_TO_BIG_ROW;
    return -1;
  }
  if (!(rec_buff=(byte*) my_alloca(reclength)))
  {
    my_errno=ENOMEM;
    return(-1);
  }
  reclength=_mi_rec_pack(info,rec_buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER),
			 record);
  error=write_dynamic_record(info,rec_buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER),
			     reclength);
  my_afree(rec_buff);
  return(error);
}


int _mi_update_blob_record(MI_INFO *info, my_off_t pos, const byte *record)
{
  byte *rec_buff;
  int error;
  ulong reclength,extra;

  extra=ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
    MI_DYN_DELETE_BLOCK_HEADER;
  reclength=info->s->base.pack_reclength+
    _my_calc_total_blob_length(info,record)+ extra;
  if (reclength > MI_DYN_MAX_ROW_LENGTH)
  {
    my_errno=HA_ERR_TO_BIG_ROW;
    return -1;
  }
  if (!(rec_buff=(byte*) my_alloca(reclength)))
  {
    my_errno=ENOMEM;
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

static int write_dynamic_record(MI_INFO *info, const byte *record,
				ulong reclength)
{
  int flag;
  ulong length;
  my_off_t filepos;
  DBUG_ENTER("write_dynamic_record");

  flag=0;
  while (reclength)
  {
    if (_mi_find_writepos(info,reclength,&filepos,&length))
      goto err;
    if (_mi_write_part_record(info,filepos,length,info->s->state.dellink,
			      (byte**) &record,&reclength,&flag))
      goto err;
  }

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
  DBUG_ENTER("_mi_find_writepos");

  if (info->s->state.dellink != HA_OFFSET_ERROR)
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
    if ((*length=reclength+3 + test(reclength > 65520)) <
	info->s->base.min_block_length)
      *length=info->s->base.min_block_length;
    else
      *length= ((*length+MI_DYN_ALIGN_SIZE-1) &
		(~ (ulong) (MI_DYN_ALIGN_SIZE-1)));
    if (info->state->data_file_length >
	(info->s->base.max_data_file_length- *length))
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(-1);
    }
    info->state->data_file_length+= *length;
    info->s->state.split++;
    info->update|=HA_STATE_WRITE_AT_END;
  }
  DBUG_RETURN(0);
} /* _mi_find_writepos */



/* Remove a deleted block from the deleted list */

static bool unlink_deleted_block(MI_INFO *info, MI_BLOCK_INFO *block_info)
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
    if (!(_mi_get_block_info(&tmp,info->dfile,block_info->prev_filepos)
	  & BLOCK_DELETED))
      DBUG_RETURN(1);				/* Something is wrong */
    mi_sizestore(tmp.header+4,block_info->next_filepos);
    if (my_pwrite(info->dfile,(char*) tmp.header+4,8,
		  block_info->prev_filepos+4, MYF(MY_NABP)))
      DBUG_RETURN(1);
    if (block_info->next_filepos != HA_OFFSET_ERROR)
    {
      if (!(_mi_get_block_info(&tmp,info->dfile,block_info->next_filepos)
	    & BLOCK_DELETED))
	DBUG_RETURN(1);				/* Something is wrong */
      mi_sizestore(tmp.header+12,block_info->prev_filepos);
      if (my_pwrite(info->dfile,(char*) tmp.header+12,8,
		    block_info->next_filepos+12,
		    MYF(MY_NABP)))
	DBUG_RETURN(1);
    }
  }
  info->state->del--;
  info->state->empty-= block_info->block_len;
  info->s->state.split--;

  /* Removing block that we are using through mi_rrnd */
  if (info->nextpos == block_info->filepos)
    info->nextpos+=block_info->block_len;
  DBUG_RETURN(0);
}

	/* Delete datarecord from database */
	/* info->rec_cache.seek_not_done is updated in cmp_record */

static int delete_dynamic_record(MI_INFO *info, my_off_t filepos,
				 uint second_read)
{
  uint length,b_type;
  MI_BLOCK_INFO block_info,del_block;
  int error=0;
  my_bool remove_next_block;
  DBUG_ENTER("delete_dynamic_record");

  /* First add a link from the last block to the new one */
  if (info->s->state.dellink != HA_OFFSET_ERROR)
  {
    block_info.second_read=0;
    if (_mi_get_block_info(&block_info,info->dfile,info->s->state.dellink)
	& BLOCK_DELETED)
    {
      char buff[8];
      mi_sizestore(buff,filepos);
      if (my_pwrite(info->dfile,buff,8,info->s->state.dellink+12,
		    MYF(MY_NABP)))
	error=1;				/* Error on write */
    }
    else
    {
      error=1;					/* Wrong delete link */
      my_errno=HA_ERR_WRONG_IN_RECORD;
    }
  }

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
    if (my_pwrite(info->dfile,(byte*) block_info.header,20,filepos,
		  MYF(MY_NABP)))
      DBUG_RETURN(1);
    info->s->state.dellink = filepos;
    info->state->del++;
    info->state->empty+=length;
    filepos=block_info.next_filepos;

    /* Now it's safe to unlink the block */
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
			  byte **record,	/* pointer to record ptr */
			  ulong *reclength,	/* length of *record */
			  int *flag)		/* *flag == 0 if header */
{
  ulong head_length,res_length,extra_length,long_block,del_length;
  byte *pos,*record_end;
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
      next_filepos=info->s->state.dellink != HA_OFFSET_ERROR ?
	info->s->state.dellink : info->state->data_file_length;
    if (*flag == 0)				/* First block */
    {
      head_length=5+8+long_block*2;
      temp[0]=5+(uchar) long_block;
      if (long_block)
      {
	mi_int3store(temp+1,*reclength);
	mi_int3store(temp+4,length-head_length);
	mi_sizestore((byte*) temp+7,next_filepos);
      }
      else
      {
	mi_int2store(temp+1,*reclength);
	mi_int2store(temp+3,length-head_length);
	mi_sizestore((byte*) temp+5,next_filepos);
      }
    }
    else
    {
      head_length=3+8+long_block;
      temp[0]=11+(uchar) long_block;
      if (long_block)
      {
	mi_int3store(temp+1,length-head_length);
	mi_sizestore((byte*) temp+4,next_filepos);
      }
      else
      {
	mi_int2store(temp+1,length-head_length);
	mi_sizestore((byte*) temp+3,next_filepos);
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
  DBUG_DUMP("header",(byte*) temp,head_length);

	/* Make a long block for one write */
  record_end= *record+length-head_length;
  del_length=(res_length ? MI_DYN_DELETE_BLOCK_HEADER : 0);
  bmove((byte*) (*record-head_length),(byte*) temp,head_length);
  memcpy(temp,record_end,(size_t) (extra_length+del_length));
  bzero((byte*) record_end,extra_length);

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
      if (my_block_write(&info->rec_cache,(byte*) *record-head_length,
		       length+extra_length+del_length,filepos))
      goto err;
    }
    else if (my_b_write(&info->rec_cache,(byte*) *record-head_length,
			length+extra_length+del_length))
      goto err;
  }
  else
  {
    info->rec_cache.seek_not_done=1;
    if (my_pwrite(info->dfile,(byte*) *record-head_length,length+extra_length+
		  del_length,filepos,info->s->write_flag))
      goto err;
  }
  memcpy(record_end,temp,(size_t) (extra_length+del_length));
  *record=record_end;
  *reclength-=(length-head_length);
  *flag=6;

  if (del_length && next_delete_block != HA_OFFSET_ERROR)
  {
    /* link the next delete block to this */
    MI_BLOCK_INFO del_block;
    del_block.second_read=0;
    if (!(_mi_get_block_info(&del_block,info->dfile,next_delete_block)
	  & BLOCK_DELETED))
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      goto err;
    }
    mi_sizestore(del_block.header+12,info->s->state.dellink);
    if (my_pwrite(info->dfile,(char*) del_block.header+12,8,
		  next_delete_block+12,
		  MYF(MY_NABP)))
      goto err;
  }

  DBUG_RETURN(0);
err:
  DBUG_PRINT("exit",("errno: %d",my_errno));
  DBUG_RETURN(1);
} /*_mi_write_part_record */


	/* update record from datafile */

static int update_dynamic_record(MI_INFO *info, my_off_t filepos, byte *record,
				 ulong reclength)
{
  int flag;
  uint error;
  ulong length;
  MI_BLOCK_INFO block_info;
  DBUG_ENTER("update_dynamic_record");

  flag=block_info.second_read=0;
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
	else
	{
	  /* Check if next block is a deleted block */
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
	    length+=del_block.block_len;
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
      filepos=info->s->state.dellink;
  }

  if (block_info.next_filepos != HA_OFFSET_ERROR)
    if (delete_dynamic_record(info,block_info.next_filepos,1))
      goto err;
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


	/* Pack a record. Return new reclength */

uint _mi_rec_pack(MI_INFO *info, register byte *to, register const byte *from)
{
  uint		length,new_length,flag,bit,i;
  char		*pos,*end,*startpos,*packpos;
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
	  size_t tmp_length=length-mi_portable_sizeof_char_ptr;
	  memcpy((byte*) to,from,tmp_length);
	  memcpy_fixed(&temp_pos,from+tmp_length,sizeof(char*));
	  memcpy(to+tmp_length,temp_pos,(size_t) blob->length);
	  to+=tmp_length+blob->length;
	}
	blob++;
      }
      else if (type == FIELD_SKIPP_ZERO)
      {
	if (memcmp((byte*) from,zero_string,length) == 0)
	  flag|=bit;
	else
	{
	  memcpy((byte*) to,from,(size_t) length); to+=length;
	}
      }
      else if (type == FIELD_SKIPP_ENDSPACE ||
	       type == FIELD_SKIPP_PRESPACE)
      {
	pos= (byte*) from; end= (byte*) from + length;
	if (type == FIELD_SKIPP_ENDSPACE)
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
	    to[0]=(char) ((new_length & 127)+128);
	    to[1]=(char) (new_length >> 7);
	    to+=2;
	  }
	  else
	    *to++= (char) new_length;
	  memcpy((byte*) to,pos,(size_t) new_length); to+=new_length;
	  flag|=bit;
	}
	else
	{
	  memcpy(to,from,(size_t) length); to+=length;
	}
      }
      else if (type == FIELD_VARCHAR)
      {
	uint tmp_length=uint2korr(from);
	store_key_length_inc(to,tmp_length);
	memcpy(to,from+2,tmp_length);
	to+=tmp_length;
	continue;
      }
      else
      {
	memcpy(to,from,(size_t) length); to+=length;
	continue;				/* Normal field */
      }
      if ((bit= bit << 1) >= 256)
      {
	*packpos++ = (char) (uchar) flag;
	bit=1; flag=0;
      }
    }
    else
    {
      memcpy(to,from,(size_t) length); to+=length;
    }
  }
  if (bit != 1)
    *packpos= (char) (uchar) flag;
  if (info->s->calc_checksum)
    *to++=(char) info->checksum;
  DBUG_PRINT("exit",("packed length: %d",(int) (to-startpos)));
  DBUG_RETURN((uint) (to-startpos));
} /* _mi_rec_pack */



/*
** Check if a record was correctly packed. Used only by isamchk
** Returns 0 if record is ok.
*/

my_bool _mi_rec_check(MI_INFO *info,const char *record)
{
  uint		length,new_length,flag,bit,i;
  char		*pos,*end,*packpos,*to;
  enum en_fieldtype type;
  reg3 MI_COLUMNDEF *rec;
  DBUG_ENTER("_mi_rec_check");

  packpos=info->rec_buff; to= info->rec_buff+info->s->base.pack_bits;
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
	  _mi_calc_blob_length(length-mi_portable_sizeof_char_ptr,record);
	if (!blob_length && !(flag & bit))
	  goto err;
	if (blob_length)
	  to+=length - mi_portable_sizeof_char_ptr+ blob_length;
      }
      else if (type == FIELD_SKIPP_ZERO)
      {
	if (memcmp((byte*) record,zero_string,length) == 0)
	{
	  if (!(flag & bit))
	    goto err;
	}
	else
	  to+=length;
      }
      else if (type == FIELD_SKIPP_ENDSPACE ||
	       type == FIELD_SKIPP_PRESPACE)
      {
	pos= (byte*) record; end= (byte*) record + length;
	if (type == FIELD_SKIPP_ENDSPACE)
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
	    if (to[0] != (char) ((new_length & 127)+128) ||
		to[1] != (char) (new_length >> 7))
	      goto err;
	    to+=2;
	  }
	  else if (*to++ != (char) new_length)
	    goto err;
	  to+=new_length;
	}
	else
	  to+=length;
      }
      else if (type == FIELD_VARCHAR)
      {
	uint tmp_length=uint2korr(record);
	to+=get_pack_length(tmp_length)+tmp_length;
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
    {
      to+=length;
    }
  }
  if (info->packed_length != (uint) (to - info->rec_buff)
      + test(info->s->calc_checksum) ||
      (bit != 1 && (flag & ~(bit - 1))))
    goto err;
  if (info->s->calc_checksum)
  {
    if ((uchar) info->checksum != (uchar) *to)
    {
      DBUG_PRINT("error",("wrong checksum for row"));
      goto err;
    }
  }
  DBUG_RETURN(0);

 err:
  DBUG_RETURN(1);
}



	/* Unpacks a record */
	/* Returns -1 and my_errno =HA_ERR_RECORD_DELETED if reclength isn't */
	/* right. Returns reclength (>0) if ok */

ulong _mi_rec_unpack(register MI_INFO *info, register byte *to, byte *from,
		     ulong found_length)
{
  uint flag,bit,length,rec_length,min_pack_length;
  enum en_fieldtype type;
  byte *from_end,*to_end,*packpos;
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
	get_key_length(length,from);
	if (length > rec_length-2)
	  goto err;
	int2store(to,length);
	memcpy(to+2,from,length);
	from+=length;
	continue;
      }
      if (flag & bit)
      {
	if (type == FIELD_BLOB || type == FIELD_SKIPP_ZERO)
	  bzero((byte*) to,rec_length);
	else if (type == FIELD_SKIPP_ENDSPACE ||
		 type == FIELD_SKIPP_PRESPACE)
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
	  if (type == FIELD_SKIPP_ENDSPACE)
	  {
	    memcpy(to,(byte*) from,(size_t) length);
	    bfill((byte*) to+length,rec_length-length,' ');
	  }
	  else
	  {
	    bfill((byte*) to,rec_length-length,' ');
	    memcpy(to+rec_length-length,(byte*) from,(size_t) length);
	  }
	  from+=length;
	}
      }
      else if (type == FIELD_BLOB)
      {
	uint size_length=rec_length- mi_portable_sizeof_char_ptr;
	ulong blob_length=_mi_calc_blob_length(size_length,from);
	if ((ulong) (from_end-from) - size_length < blob_length ||
	    min_pack_length > (uint) (from_end -(from+size_length+blob_length)))
	  goto err;
	memcpy((byte*) to,(byte*) from,(size_t) size_length);
	from+=size_length;
	memcpy_fixed((byte*) to+size_length,(byte*) &from,sizeof(char*));
	from+=blob_length;
      }
      else
      {
	if (type == FIELD_SKIPP_ENDSPACE || type == FIELD_SKIPP_PRESPACE)
	  min_pack_length--;
	if (min_pack_length + rec_length > (uint) (from_end - from))
	  goto err;
	memcpy(to,(byte*) from,(size_t) rec_length); from+=rec_length;
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
      memcpy(to,(byte*) from,(size_t) rec_length); from+=rec_length;
    }
  }
  if (info->s->calc_checksum)
    from++;
  if (to == to_end && from == from_end && (bit == 1 || !(flag & ~(bit-1))))
    DBUG_RETURN((info->packed_length=found_length));
 err:
  my_errno=HA_ERR_RECORD_DELETED;
  DBUG_PRINT("error",("to_end: %lx -> %lx  from_end: %lx -> %lx",
		      to,to_end,from,from_end));
  DBUG_DUMP("from",(byte*) info->rec_buff,info->s->base.min_pack_length);
  DBUG_RETURN(MY_FILE_ERROR);
} /* _mi_rec_unpack */


	/* Calc length of blob. Update info in blobs->length */

ulong _my_calc_total_blob_length(MI_INFO *info, const byte *record)
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


ulong _mi_calc_blob_length(uint length, const byte *pos)
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


void _my_store_blob_length(byte *pos,uint pack_length,uint length)
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


	/* Read record from datafile */
	/* Returns 0 if ok, -1 if error */

int _mi_read_dynamic_record(MI_INFO *info, my_off_t filepos, byte *buf)
{
  int flag;
  uint b_type,left_length;
  byte *to;
  MI_BLOCK_INFO block_info;
  File file;
  DBUG_ENTER("mi_read_dynamic_record");

  if (filepos != HA_OFFSET_ERROR)
  {
    LINT_INIT(to);
    LINT_INIT(left_length);
    file=info->dfile;
    block_info.next_filepos=filepos;	/* for easyer loop */
    flag=block_info.second_read=0;
    do
    {
      if (info->opt_flag & WRITE_CACHE_USED &&
	  info->rec_cache.pos_in_file <= block_info.next_filepos &&
	  flush_io_cache(&info->rec_cache))
	goto err;
      info->rec_cache.seek_not_done=1;
      if ((b_type=_mi_get_block_info(&block_info,file,
				     block_info.next_filepos))
	  & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	     BLOCK_FATAL_ERROR))
      {
	if (b_type & (BLOCK_SYNC_ERROR | BLOCK_DELETED))
	  my_errno=HA_ERR_RECORD_DELETED;
	goto err;
      }
      if (flag == 0)			/* First block */
      {
	flag=1;
	if (block_info.rec_len > (uint) info->s->base.max_pack_length)
	  goto panic;
	if (info->s->base.blobs)
	{
	  if (!(to=mi_fix_rec_buff_for_blob(info,block_info.rec_len)))
	    goto err;
	}
	else
	  to= info->rec_buff;
	left_length=block_info.rec_len;
      }
      if (left_length < block_info.data_len || ! block_info.data_len)
	goto panic;			/* Wrong linked record */
      if (my_pread(file,(byte*) to,block_info.data_len,block_info.filepos,
		   MYF(MY_NABP)))
	goto panic;
      left_length-=block_info.data_len;
      to+=block_info.data_len;
    } while (left_length);

    info->update|= HA_STATE_AKTIV;	/* We have a aktive record */
    VOID(_mi_writeinfo(info,0));
    DBUG_RETURN(_mi_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
		MY_FILE_ERROR ? 0 : -1);
  }
  VOID(_mi_writeinfo(info,0));
  DBUG_RETURN(-1);			/* Wrong data to read */

panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;
err:
  VOID(_mi_writeinfo(info,0));
  DBUG_RETURN(-1);
}


byte *mi_fix_rec_buff_for_blob(MI_INFO *info, ulong length)
{
  uint extra;
  if (! info->rec_buff || length > info->alloced_rec_buff_length)
  {
    byte *newptr;
    extra=ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
      MI_DYN_DELETE_BLOCK_HEADER;
    if (!(newptr=(byte*) my_realloc((gptr) info->rec_alloc,length+extra,
				    MYF(MY_ALLOW_ZERO_PTR))))
      return newptr;
    info->rec_alloc=newptr;
    info->rec_buff=newptr+ALIGN_SIZE(MI_DYN_DELETE_BLOCK_HEADER);
    info->alloced_rec_buff_length=length;
  }
  return info->rec_buff;
}


	/* compare unique constraint between stored rows */

int _mi_cmp_dynamic_unique(MI_INFO *info, MI_UNIQUEDEF *def,
			   const byte *record, my_off_t pos)
{
  byte *rec_buff,*rec_alloc,*old_record;
  uint alloced_rec_buff_length;
  int error;
  DBUG_ENTER("_mi_cmp_dynamic_unique");

  if (!(old_record=my_alloca(info->s->base.reclength)))
    DBUG_RETURN(1);

  /* Don't let the compare destroy blobs that may be in use */
  rec_buff=info->rec_buff;
  rec_alloc=info->rec_alloc;
  alloced_rec_buff_length=info->alloced_rec_buff_length;
  if (info->s->base.blobs)
  {
    info->rec_buff=0;
    info->rec_alloc=0;
    info->alloced_rec_buff_length=0;
  }
  error=_mi_read_dynamic_record(info,pos,old_record);
  if (!error)
    error=mi_unique_comp(def, record, old_record, def->null_are_equal);
  if (info->s->base.blobs)
  {
    my_free(info->rec_alloc,MYF(MY_ALLOW_ZERO_PTR));
    info->rec_buff=rec_buff;
    info->rec_alloc=rec_alloc;
    info->alloced_rec_buff_length=alloced_rec_buff_length;
  }
  my_afree(old_record);
  DBUG_RETURN(error);
}


	/* Compare of record one disk with packed record in memory */

int _mi_cmp_dynamic_record(register MI_INFO *info, register const byte *record)
{
  uint flag,reclength,b_type;
  my_off_t filepos;
  byte *buffer;
  MI_BLOCK_INFO block_info;
  DBUG_ENTER("_mi_cmp_dynamic_record");

	/* We are going to do changes; dont let anybody disturb */
  dont_break();				/* Dont allow SIGHUP or SIGINT */

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
      if (!(buffer=(byte*) my_alloca(info->s->base.pack_reclength+
				     _my_calc_total_blob_length(info,record))))
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
    my_afree((gptr) buffer);
  DBUG_RETURN(my_errno);
}


	/* Compare file to buffert */

static int _mi_cmp_buffer(File file, const byte *buff, my_off_t filepos,
			  uint length)
{
  uint next_length;
  char temp_buff[IO_SIZE*2];
  DBUG_ENTER("_mi_cmp_buffer");

  VOID(my_seek(file,filepos,MY_SEEK_SET,MYF(0)));
  next_length= IO_SIZE*2 - (uint) (filepos & (IO_SIZE-1));

  while (length > IO_SIZE*2)
  {
    if (my_read(file,temp_buff,next_length,MYF(MY_NABP)))
      goto err;
    if (memcmp((byte*) buff,temp_buff,IO_SIZE))
      DBUG_RETURN(1);
    buff+=next_length;
    length-= next_length;
    next_length=IO_SIZE*2;
  }
  if (my_read(file,temp_buff,length,MYF(MY_NABP)))
    goto err;
  DBUG_RETURN(memcmp((byte*) buff,temp_buff,length));
err:
  DBUG_RETURN(1);
}


int _mi_read_rnd_dynamic_record(MI_INFO *info, byte *buf,
				register my_off_t filepos,
				my_bool skipp_deleted_blocks)
{
  int flag,info_read,save_errno;
  uint left_len,b_type;
  byte *to;
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_read_rnd_dynamic_record");

  info_read=0;
  LINT_INIT(to);

  if (info->lock_type == F_UNLCK)
  {
#ifndef UNSAFE_LOCKING
    if (share->r_locks == 0 && share->w_locks == 0)
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

  flag=block_info.second_read=0;
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
      if (_mi_read_cache(&info->rec_cache,(byte*) block_info.header,filepos,
			sizeof(block_info.header),
			test(!flag && skipp_deleted_blocks) | 2))
	goto panic;
      b_type=_mi_get_block_info(&block_info,-1,filepos);
    }
    else
    {
      if (info->opt_flag & WRITE_CACHE_USED &&
	  info->rec_cache.pos_in_file <= filepos &&
	  flush_io_cache(&info->rec_cache))
	DBUG_RETURN(my_errno);
      info->rec_cache.seek_not_done=1;
      b_type=_mi_get_block_info(&block_info,info->dfile,filepos);
    }

    if (b_type & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
		  BLOCK_FATAL_ERROR))
    {
      if ((b_type & (BLOCK_DELETED | BLOCK_SYNC_ERROR))
	  && skipp_deleted_blocks)
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
    if (flag == 0)				/* First block */
    {
      if (block_info.rec_len > (uint) share->base.max_pack_length)
	goto panic;
      info->lastpos=filepos;
      if (share->base.blobs)
      {
	if (!(to=mi_fix_rec_buff_for_blob(info,block_info.rec_len)))
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
	memcpy((byte*) to, block_info.header+offset,tmp_length);
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
	if (_mi_read_cache(&info->rec_cache,(byte*) to,filepos,
			   block_info.data_len,
			   test(!flag && skipp_deleted_blocks)))
	  goto panic;
      }
      else
      {
	/* VOID(my_seek(info->dfile,filepos,MY_SEEK_SET,MYF(0))); */
	if (my_read(info->dfile,(byte*) to,block_info.data_len,MYF(MY_NABP)))
	{
	  if (my_errno == -1)
	    my_errno= HA_ERR_WRONG_IN_RECORD;	/* Unexpected end of file */
	  goto err;
	}
      }
    }
    if (flag++ == 0)
    {
      info->nextpos=block_info.filepos+block_info.block_len;
      skipp_deleted_blocks=0;
    }
    left_len-=block_info.data_len;
    to+=block_info.data_len;
    filepos=block_info.next_filepos;
  } while (left_len);

  info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;
  if (share->r_locks == 0 && share->w_locks == 0)
    VOID(_mi_writeinfo(info,0));
  if (_mi_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
      MY_FILE_ERROR)
    DBUG_RETURN(0);
  DBUG_RETURN(my_errno);			/* Wrong record */

panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;		/* Something is fatal wrong */
err:
  save_errno=my_errno;
  VOID(_mi_writeinfo(info,0));
  DBUG_RETURN(my_errno=save_errno);
}


	/* Read and process header from a dynamic-record-file */

uint _mi_get_block_info(MI_BLOCK_INFO *info, File file, my_off_t filepos)
{
  uint return_val=0;
  uchar *header=info->header;

  if (file >= 0)
  {
    VOID(my_seek(file,filepos,MY_SEEK_SET,MYF(0)));
    if (my_read(file,(char*) header,sizeof(info->header),MYF(0)) !=
	sizeof(info->header))
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      return BLOCK_FATAL_ERROR;
    }
  }
  DBUG_DUMP("header",(byte*) header,MI_BLOCK_INFO_HEADER_LENGTH);
  if (info->second_read)
  {
    if (info->header[0] <= 6)
      return_val=BLOCK_SYNC_ERROR;
  }
  else
  {
    if (info->header[0] > 6)
      return_val=BLOCK_SYNC_ERROR;
  }
  info->next_filepos= HA_OFFSET_ERROR; /* Dummy ifall no next block */

  switch (info->header[0]) {
  case 0:
    if ((info->block_len=(uint) mi_uint3korr(header+1)) <
	MI_MIN_BLOCK_LENGTH ||
	(info->block_len & (MI_DYN_ALIGN_SIZE -1)))
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      return BLOCK_ERROR;
    }
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
      return BLOCK_FATAL_ERROR;
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
  default:
    my_errno=HA_ERR_WRONG_IN_RECORD;	 /* Garbage */
    return BLOCK_ERROR;
  }
}
