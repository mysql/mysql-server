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

#include "isamdef.h"

/* Enough for comparing if number is zero */
static char zero_string[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int write_dynamic_record(N_INFO *info,const byte *record,
				uint reclength);
static int _nisam_find_writepos(N_INFO *info,uint reclength,ulong *filepos,
			     uint *length);
static int update_dynamic_record(N_INFO *info,ulong filepos,byte *record,
				 uint reclength);
static int delete_dynamic_record(N_INFO *info,ulong filepos,
				 uint second_read);
static int _nisam_cmp_buffer(File file, const byte *buff, ulong filepos,
			  uint length);

#ifdef THREAD
/* Play it safe; We have a small stack when using threads */
#undef my_alloca
#undef my_afree
#define my_alloca(A) my_malloc((A),MYF(0))
#define my_afree(A) my_free((A),MYF(0))
#endif

	/* Interface function from N_INFO */

int _nisam_write_dynamic_record(N_INFO *info, const byte *record)
{
  uint reclength=_nisam_rec_pack(info,info->rec_buff,record);
  return (write_dynamic_record(info,info->rec_buff,reclength));
}

int _nisam_update_dynamic_record(N_INFO *info, ulong pos, const byte *record)
{
  uint length=_nisam_rec_pack(info,info->rec_buff,record);
  return (update_dynamic_record(info,pos,info->rec_buff,length));
}

int _nisam_write_blob_record(N_INFO *info, const byte *record)
{
  byte *rec_buff;
  int error;
  uint reclength,extra;

  extra=ALIGN_SIZE(MAX_DYN_BLOCK_HEADER)+N_SPLITT_LENGTH+
    DYN_DELETE_BLOCK_HEADER;
  if (!(rec_buff=(byte*) my_alloca(info->s->base.pack_reclength+
				   _calc_total_blob_length(info,record)+
				   extra)))
    return(-1);
  reclength=_nisam_rec_pack(info,rec_buff+ALIGN_SIZE(MAX_DYN_BLOCK_HEADER),
			 record);
  error=write_dynamic_record(info,rec_buff+ALIGN_SIZE(MAX_DYN_BLOCK_HEADER),
			     reclength);
  my_afree(rec_buff);
  return(error);
}


int _nisam_update_blob_record(N_INFO *info, ulong pos, const byte *record)
{
  byte *rec_buff;
  int error;
  uint reclength,extra;

  extra=ALIGN_SIZE(MAX_DYN_BLOCK_HEADER)+N_SPLITT_LENGTH+
    DYN_DELETE_BLOCK_HEADER;
  if (!(rec_buff=(byte*) my_alloca(info->s->base.pack_reclength+
				   _calc_total_blob_length(info,record)+
				   extra)))
    return(-1);
  reclength=_nisam_rec_pack(info,rec_buff+ALIGN_SIZE(MAX_DYN_BLOCK_HEADER),
			 record);
  error=update_dynamic_record(info,pos,
			      rec_buff+ALIGN_SIZE(MAX_DYN_BLOCK_HEADER),
			      reclength);
  my_afree(rec_buff);
  return(error);
}

int _nisam_delete_dynamic_record(N_INFO *info)
{
  return delete_dynamic_record(info,info->lastpos,0);
}


	/* Write record to data-file */

static int write_dynamic_record(N_INFO *info, const byte *record,
				uint reclength)
{
  int flag;
  uint length;
  ulong filepos;
  DBUG_ENTER("write_dynamic_record");

  flag=0;
  while (reclength)
  {
    if (_nisam_find_writepos(info,reclength,&filepos,&length))
      goto err;
    if (_nisam_write_part_record(info,filepos,length,info->s->state.dellink,
			      (byte**) &record,&reclength,&flag))
      goto err;
  }

  DBUG_RETURN(0);
 err:
  DBUG_RETURN(1);
}


	/* Get a block for data ; The given data-area must be used !! */

static int _nisam_find_writepos(N_INFO *info,
			     uint reclength, /* record length */
			     ulong *filepos, /* Return file pos */
			     uint *length)   /* length of block at filepos */
{
  BLOCK_INFO block_info;
  DBUG_ENTER("_nisam_find_writepos");

  if (info->s->state.dellink != NI_POS_ERROR)
  {
    *filepos=info->s->state.dellink;
    block_info.second_read=0;
    info->rec_cache.seek_not_done=1;

    if (!(_nisam_get_block_info(&block_info,info->dfile,
				info->s->state.dellink) & BLOCK_DELETED))
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      DBUG_RETURN(-1);
    }
    info->s->state.dellink=block_info.next_filepos;
    info->s->state.del--;
    info->s->state.empty-= block_info.block_len;
    *length= block_info.block_len;
  }
  else
  {
    if (info->s->state.data_file_length > info->s->base.max_data_file_length)
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(-1);
    }
    *filepos=info->s->state.data_file_length;		/* New block last */
    if ((*length=reclength+3 + test(reclength > 65532)) <
	info->s->base.min_block_length)
      *length=info->s->base.min_block_length;
    info->s->state.data_file_length+= *length;
    info->s->state.splitt++;
    info->update|=HA_STATE_WRITE_AT_END;
  }
  DBUG_RETURN(0);
} /* _nisam_find_writepos */


	/* Write a block to datafile */

int _nisam_write_part_record(N_INFO *info,
			  ulong filepos,	/* points at empty block */
			  uint length,		/* length of block */
			  ulong next_filepos,	/* Next empty block */
			  byte **record,	/* pointer to record ptr */
			  uint *reclength,	/* length of *record */
			  int *flag)		/* *flag == 0 if header */
{
  uint head_length,res_length,extra_length,long_block,del_length;
  byte *pos,*record_end;
  uchar temp[N_SPLITT_LENGTH+DYN_DELETE_BLOCK_HEADER];
  DBUG_ENTER("_nisam_write_part_record");

  res_length=extra_length=0;
  if (length > *reclength + N_SPLITT_LENGTH)
  {						/* Splitt big block */
    res_length=length- *reclength - 3 - N_EXTEND_BLOCK_LENGTH;
    length-= res_length;			/* Use this for first part */
  }
  long_block= (length < 65535L && *reclength < 65535L) ? 0 : 1;
  if (length-long_block == *reclength+3 || length == *reclength + 4)
  {						/* Exact what we need */
    temp[0]=(uchar) (1+ *flag);			/* 1, or 9 */
    if (long_block)
    {
      int3store(temp+1,*reclength);
    }
    else
    {
      int2store(temp+1,*reclength);
    }
    head_length=3+long_block;
    if (length-long_block == *reclength+4)
    {
      length--;
      temp[0]++;				/* 2 or 10 */
      extra_length++;				/* One empty */
    }
  }
  else if (length-long_block*2 < *reclength+5)
  {						/* To short block */
    if (next_filepos == NI_POS_ERROR)
      next_filepos=info->s->state.dellink != NI_POS_ERROR ?
	info->s->state.dellink : info->s->state.data_file_length;
    if (*flag == 0)				/* First block */
    {
      head_length=5+4+long_block*2;
      temp[0]=4;
      if (long_block)
      {
	int3store(temp+1,*reclength);
	int3store(temp+4,length-head_length);
	int4store((byte*) temp+7,next_filepos);
      }
      else
      {
	int2store(temp+1,*reclength);
	int2store(temp+3,length-head_length);
	int4store((byte*) temp+5,next_filepos);
      }
    }
    else
    {
      head_length=3+4+long_block;
      temp[0]=12;
      if (long_block)
      {
	int3store(temp+1,length-head_length);
	int4store((byte*) temp+4,next_filepos);
      }
      else
      {
	int2store(temp+1,length-head_length);
	int4store((byte*) temp+3,next_filepos);
      }
    }
  }
  else
  {					/* Block with empty info last */
    head_length=5+long_block*2;
    temp[0]= (uchar) (3+ *flag);	/* 3 or 11 */
    if (long_block)
    {
      int3store(temp+1,*reclength);
      int3store(temp+4,length-7);
    }
    else
    {
      int2store(temp+1,*reclength);
      int2store(temp+3,length-5);
    }
    extra_length= length- *reclength-head_length;
    length=	  *reclength+head_length;	/* Write only what is needed */
  }
  temp[0]+=(uchar) (long_block*4);
  DBUG_DUMP("header",(byte*) temp,head_length);

	/* Make a long block for one write */
  record_end= *record+length-head_length;
  del_length=(res_length ? DYN_DELETE_BLOCK_HEADER : 0);
  bmove((byte*) (*record-head_length),(byte*) temp,head_length);
  memcpy(temp,record_end,(size_t) (extra_length+del_length));
  bzero((byte*) record_end,extra_length);
  if (res_length)
  {
    pos=record_end+extra_length;
    pos[0]= '\0';
    int3store(pos+1,res_length);
    int4store(pos+4,info->s->state.dellink);
    info->s->state.dellink= filepos+length+extra_length;
    info->s->state.del++;
    info->s->state.empty+=res_length;
    info->s->state.splitt++;
  }
  if (info->opt_flag & WRITE_CACHE_USED && info->update & HA_STATE_WRITE_AT_END)
  {
    if (my_b_write(&info->rec_cache,(byte*) *record-head_length,
		   length+extra_length+del_length))
      goto err;
  }
  else
  {
    info->rec_cache.seek_not_done=1;
    if (my_pwrite(info->dfile,(byte*) *record-head_length,length+extra_length+
		  del_length,filepos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
      goto err;
  }
  memcpy(record_end,temp,(size_t) (extra_length+del_length));
  *record=record_end;
  *reclength-=(length-head_length);
  *flag=8;

  DBUG_RETURN(0);
err:
  DBUG_PRINT("exit",("errno: %d",my_errno));
  DBUG_RETURN(1);
} /*_nisam_write_part_record */


	/* update record from datafile */

static int update_dynamic_record(N_INFO *info, ulong filepos, byte *record, uint reclength)
{
  int flag;
  uint error,length;
  BLOCK_INFO block_info;
  DBUG_ENTER("update_dynamic_record");

  flag=block_info.second_read=0;
  while (reclength > 0)
  {
    if (filepos != info->s->state.dellink)
    {
      block_info.next_filepos= NI_POS_ERROR;
      if ((error=_nisam_get_block_info(&block_info,info->dfile,filepos))
	  & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	     BLOCK_FATAL_ERROR))
      {
	if (!(error & BLOCK_FATAL_ERROR))
	  my_errno=HA_ERR_WRONG_IN_RECORD;
	goto err;
      }
      length=(uint) (block_info.filepos-filepos) + block_info.block_len;
    }
    else
    {
      if (_nisam_find_writepos(info,reclength,&filepos,&length))
	goto err;
    }
    if (_nisam_write_part_record(info,filepos,length,block_info.next_filepos,
			      &record,&reclength,&flag))
      goto err;
    if ((filepos=block_info.next_filepos) == NI_POS_ERROR)
      filepos=info->s->state.dellink;
  }

  if (block_info.next_filepos != NI_POS_ERROR)
    if (delete_dynamic_record(info,block_info.next_filepos,1))
      goto err;
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

	/* Delete datarecord from database */
	/* info->rec_cache.seek_not_done is updated in cmp_record */

static int delete_dynamic_record(N_INFO *info, ulong filepos, uint second_read)
{
  uint length,b_type;
  BLOCK_INFO block_info;
  DBUG_ENTER("delete_dynamic_record");

  block_info.second_read=second_read;
  do
  {
    if ((b_type=_nisam_get_block_info(&block_info,info->dfile,filepos))
	& (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
	   BLOCK_FATAL_ERROR) ||
	(length=(uint) (block_info.filepos-filepos) +block_info.block_len) <
	N_MIN_BLOCK_LENGTH)
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      DBUG_RETURN(1);
    }
    block_info.header[0]=0;
    length=(uint) (block_info.filepos-filepos) +block_info.block_len;
    int3store(block_info.header+1,length);
    int4store(block_info.header+4,info->s->state.dellink);
    if (my_pwrite(info->dfile,(byte*) block_info.header,8,filepos,
		  MYF(MY_NABP)))
      DBUG_RETURN(1);
    info->s->state.dellink = filepos;
    info->s->state.del++;
    info->s->state.empty+=length;
    filepos=block_info.next_filepos;
  } while (!(b_type & BLOCK_LAST));
  DBUG_RETURN(0);
}


	/* Pack a record. Return new reclength */

uint _nisam_rec_pack(N_INFO *info, register byte *to, register const byte *from)
{
  uint		length,new_length,flag,bit,i;
  char		*pos,*end,*startpos,*packpos;
  enum en_fieldtype type;
  reg3 N_RECINFO *rec;
  N_BLOB	*blob;
  DBUG_ENTER("_nisam_rec_pack");

  flag=0 ; bit=1;
  startpos=packpos=to; to+= info->s->base.pack_bits; blob=info->blobs;
  rec=info->s->rec;

  for (i=info->s->base.fields ; i-- > 0; from+= (rec++)->base.length)
  {
    length=(uint) rec->base.length;
    if ((type = (enum en_fieldtype) rec->base.type) != FIELD_NORMAL)
    {
      if (type == FIELD_BLOB)
      {
	if (!blob->length)
	  flag|=bit;
	else
	{
	  char *temp_pos;
	  memcpy((byte*) to,from,(size_t) length);
	  memcpy_fixed(&temp_pos,from+length,sizeof(char*));
	  memcpy(to+length,temp_pos,(size_t) blob->length);
	  to+=length+blob->length;
	}
	blob++;
	from+=sizeof(char*);			/* Skipp blob-pointer */
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
	if (new_length +1 + test(rec->base.length > 255 && new_length > 127)
	    < length)
	{
	  if (rec->base.length > 255 && new_length > 127)
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
      else if (type == FIELD_ZERO)
	continue;				/* Don't store this */
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
  DBUG_PRINT("exit",("packed length: %d",(int) (to-startpos)));
  DBUG_RETURN((uint) (to-startpos));
} /* _nisam_rec_pack */



/*
** Check if a record was correctly packed. Used only by isamchk
** Returns 0 if record is ok.
*/

my_bool _nisam_rec_check(N_INFO *info,const char *from)
{
  uint		length,new_length,flag,bit,i;
  char		*pos,*end,*packpos,*to;
  enum en_fieldtype type;
  reg3 N_RECINFO *rec;
  DBUG_ENTER("_nisam_rec_check");

  packpos=info->rec_buff; to= info->rec_buff+info->s->base.pack_bits;
  rec=info->s->rec;
  flag= *packpos; bit=1;

  for (i=info->s->base.fields ; i-- > 0; from+= (rec++)->base.length)
  {
    length=(uint) rec->base.length;
    if ((type = (enum en_fieldtype) rec->base.type) != FIELD_NORMAL)
    {
      if (type == FIELD_BLOB)
      {
	uint blob_length= _calc_blob_length(length,from);
	if (!blob_length && !(flag & bit))
	  goto err;
	if (blob_length)
	  to+=length+ blob_length;
	from+=sizeof(char*);
      }
      else if (type == FIELD_SKIPP_ZERO)
      {
	if (memcmp((byte*) from,zero_string,length) == 0)
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
	pos= (byte*) from; end= (byte*) from + length;
	if (type == FIELD_SKIPP_ENDSPACE)
	{					/* Pack trailing spaces */
	  while (end > from && *(end-1) == ' ')
	    end--;
	}
	else
	{					/* Pack pre-spaces */
	  while (pos < end && *pos == ' ')
	    pos++;
	}
	new_length=(uint) (end-pos);
	if (new_length +1 + test(rec->base.length > 255 && new_length > 127)
	    < length)
	{
	  if (!(flag & bit))
	    goto err;
	  if (rec->base.length > 255 && new_length > 127)
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
      else
      {
	if (type != FIELD_ZERO)
	  to+=length;				/* Not packed field */
	continue;
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
  if (bit != 1)
    *packpos= (char) (uchar) flag;
  if (info->packed_length == (uint) (to - info->rec_buff) &&
      (bit == 1 || !(flag & ~(bit - 1))))
    DBUG_RETURN(0);

 err:
  DBUG_RETURN(1);
}



	/* Unpacks a record */
	/* Returns -1 and my_errno =HA_ERR_RECORD_DELETED if reclength isn't */
	/* right. Returns reclength (>0) if ok */

uint _nisam_rec_unpack(register N_INFO *info, register byte *to, byte *from,
		    uint found_length)
{
  uint flag,bit,length,rec_length,min_pack_length;
  enum en_fieldtype type;
  byte *from_end,*to_end,*packpos;
  reg3 N_RECINFO *rec,*end_field;
  DBUG_ENTER("_nisam_rec_unpack");

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
    rec_length=rec->base.length;
    if ((type = (enum en_fieldtype) rec->base.type) != FIELD_NORMAL)
    {
      if (type == FIELD_ZERO)
	continue;				/* Skipp this */
      if (flag & bit)
      {
	if (type == FIELD_BLOB)
	{
	  bzero((byte*) to,rec_length+sizeof(char*));
	  to+=sizeof(char*);
	}
	else if (type == FIELD_SKIPP_ZERO)
	  bzero((byte*) to,rec_length);
	else if (type == FIELD_SKIPP_ENDSPACE ||
		 type == FIELD_SKIPP_PRESPACE)
	{
	  if (rec->base.length > 255 && *from & 128)
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
	ulong blob_length=_calc_blob_length(rec_length,from);
	if ((ulong) (from_end-from) - rec_length < blob_length ||
	    min_pack_length > (uint) (from_end -(from+rec_length+blob_length)))
	  goto err;
	memcpy((byte*) to,(byte*) from,(size_t) rec_length);
	from+=rec_length;
	/* memcpy crasches alpha egcs 1.1.2 */
	bmove((byte*) to+rec_length,(byte*) &from,sizeof(char*));
	from+=blob_length;
	to+=sizeof(char*);
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
  if (to == to_end && from == from_end && (bit == 1 || !(flag & ~(bit-1))))
    DBUG_RETURN((info->packed_length=found_length));
 err:
  my_errno=HA_ERR_RECORD_DELETED;
  DBUG_PRINT("error",("to_end: %lx -> %lx  from_end: %lx -> %lx",
		      to,to_end,from,from_end));
  DBUG_DUMP("from",(byte*) info->rec_buff,info->s->base.min_pack_length);
  DBUG_RETURN(MY_FILE_ERROR);
} /* _nisam_rec_unpack */


	/* Calc length of blob. Update info in blobs->length */

uint _calc_total_blob_length(N_INFO *info, const byte *record)
{
  uint i,length;
  N_BLOB *blob;

  for (i=length=0, blob= info->blobs; i++ < info->s->base.blobs ; blob++)
  {
    blob->length=_calc_blob_length(blob->pack_length,record + blob->offset);
    length+=blob->length;
  }
  return length;
}


uint _calc_blob_length(uint length, const byte *pos)
{
  switch (length) {
  case 1:
    return (uint) (uchar) *pos;
  case 2:
    {
      short j; shortget(j,pos);
      return (uint) (unsigned short) j;
    }
#ifdef MSDOS
    break;					/* skipp microsoft warning */
#endif
  case 3:
    return uint3korr(pos);
  case 4:
    {
      long j; longget(j,pos);
      return (uint) j;
    }
#ifdef MSDOS
    break;
#endif
  default:
    break;
  }
  return 0; /* Impossible */
}

	/* Read record from datafile */
	/* Returns 0 if ok, -1 if error */

int _nisam_read_dynamic_record(N_INFO *info, ulong filepos, byte *buf)
{
  int flag;
  uint b_type,left_length;
  byte *to;
  BLOCK_INFO block_info;
  File file;
  DBUG_ENTER("ni_read_dynamic_record");

  if (filepos != NI_POS_ERROR)
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
      if ((b_type=_nisam_get_block_info(&block_info,file,
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
	  if (!(to=fix_rec_buff_for_blob(info,block_info.rec_len)))
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
    VOID(_nisam_writeinfo(info,0));
    DBUG_RETURN(_nisam_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
		MY_FILE_ERROR ? 0 : -1);
  }
  VOID(_nisam_writeinfo(info,0));
  DBUG_RETURN(-1);			/* Wrong data to read */

panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;
err:
  VOID(_nisam_writeinfo(info,0));
  DBUG_RETURN(-1);
}


byte *fix_rec_buff_for_blob(N_INFO *info, uint length)
{
  uint extra;
  if (! info->rec_buff || length > info->alloced_rec_buff_length)
  {
    byte *newptr;
    extra=ALIGN_SIZE(MAX_DYN_BLOCK_HEADER)+N_SPLITT_LENGTH+
      DYN_DELETE_BLOCK_HEADER;
    if (!(newptr=(byte*) my_realloc((gptr) info->rec_alloc,length+extra,
				    MYF(MY_ALLOW_ZERO_PTR))))
      return newptr;
    info->rec_alloc=newptr;
    info->rec_buff=newptr+ALIGN_SIZE(DYN_DELETE_BLOCK_HEADER);
    info->alloced_rec_buff_length=length;
  }
  return info->rec_buff;
}


	/* Compare of record one disk with packed record in memory */

int _nisam_cmp_dynamic_record(register N_INFO *info, register const byte *record)
{
  uint flag,reclength,b_type;
  ulong filepos;
  byte *buffer;
  BLOCK_INFO block_info;
  DBUG_ENTER("_nisam_cmp_dynamic_record");

	/* We are going to do changes; dont let anybody disturb */
  dont_break();				/* Dont allow SIGHUP or SIGINT */

  if (info->opt_flag & WRITE_CACHE_USED)
  {
    info->update&= ~HA_STATE_WRITE_AT_END;
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
				     _calc_total_blob_length(info,record))))
	DBUG_RETURN(-1);
    }
    reclength=_nisam_rec_pack(info,buffer,record);
    record= buffer;

    filepos=info->lastpos;
    flag=block_info.second_read=0;
    block_info.next_filepos=filepos;
    while (reclength > 0)
    {
      if ((b_type=_nisam_get_block_info(&block_info,info->dfile,
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
      if (_nisam_cmp_buffer(info->dfile,record,block_info.filepos,
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

static int _nisam_cmp_buffer(File file, const byte *buff, ulong filepos, uint length)
{
  uint next_length;
  char temp_buff[IO_SIZE*2];
  DBUG_ENTER("_nisam_cmp_buffer");

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


int _nisam_read_rnd_dynamic_record(N_INFO *info, byte *buf, register ulong filepos, int skipp_deleted_blocks)
{
  int flag,info_read,fatal_errcode;
  uint left_len,b_type;
  byte *to;
  BLOCK_INFO block_info;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("_nisam_read_rnd_dynamic_record");

  info_read=0;
  fatal_errcode= -1;
  LINT_INIT(to);

#ifndef NO_LOCKING
  if (info->lock_type == F_UNLCK)
  {
#ifndef UNSAFE_LOCKING
    if (share->r_locks == 0 && share->w_locks == 0)
    {
      if (my_lock(share->kfile,F_RDLCK,0L,F_TO_EOF,
		  MYF(MY_SEEK_NOT_DONE) | info->lock_wait))
	DBUG_RETURN(fatal_errcode);
    }
#else
    info->tmp_lock_type=F_RDLCK;
#endif
  }
  else
    info_read=1;				/* memory-keyinfoblock is ok */
#endif /* !NO_LOCKING */

  flag=block_info.second_read=0;
  left_len=1;
  do
  {
    if (filepos >= share->state.data_file_length)
    {
#ifndef NO_LOCKING
      if (!info_read)
      {						/* Check if changed */
	info_read=1;
	info->rec_cache.seek_not_done=1;
	if (my_pread(share->kfile,(char*) &share->state.header,
		     share->state_length, 0L,MYF(MY_NABP)))
	  goto err;
      }
      if (filepos >= share->state.data_file_length)
#endif
      {
	my_errno= HA_ERR_END_OF_FILE;
	goto err;
      }
    }
    if (info->opt_flag & READ_CACHE_USED)
    {
      if (_nisam_read_cache(&info->rec_cache,(byte*) block_info.header,filepos,
			sizeof(block_info.header),
			test(!flag && skipp_deleted_blocks) | 2))
	goto err;
      b_type=_nisam_get_block_info(&block_info,-1,filepos);
    }
    else
    {
      if (info->opt_flag & WRITE_CACHE_USED &&
	  info->rec_cache.pos_in_file <= filepos &&
	  flush_io_cache(&info->rec_cache))
	DBUG_RETURN(-1);
      info->rec_cache.seek_not_done=1;
      b_type=_nisam_get_block_info(&block_info,info->dfile,filepos);
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
	fatal_errcode=1;
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
	if (!(to=fix_rec_buff_for_blob(info,block_info.rec_len)))
	  goto err;
      }
      else
	to= info->rec_buff;
      left_len=block_info.rec_len;
    }
    if (left_len < block_info.data_len)
      goto panic;				/* Wrong linked record */

    if (info->opt_flag & READ_CACHE_USED)
    {
      if (_nisam_read_cache(&info->rec_cache,(byte*) to,block_info.filepos,
			block_info.data_len,
			test(!flag && skipp_deleted_blocks)))
	goto err;
    }
    else
    {
      VOID(my_seek(info->dfile,block_info.filepos,MY_SEEK_SET,MYF(0)));
      if (my_read(info->dfile,(byte*) to,block_info.data_len,MYF(MY_NABP)))
	goto err;
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
  VOID(_nisam_writeinfo(info,0));
  if (_nisam_rec_unpack(info,buf,info->rec_buff,block_info.rec_len) !=
      MY_FILE_ERROR)
    DBUG_RETURN(0);
  DBUG_RETURN(fatal_errcode);			/* Wrong record */

panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;		/* Something is fatal wrong */
err:
  VOID(_nisam_writeinfo(info,0));
  DBUG_RETURN(fatal_errcode);
}


	/* Read and process header from a dynamic-record-file */

uint _nisam_get_block_info(BLOCK_INFO *info, File file, ulong filepos)
{
  uint return_val=0,length;
  uchar *header=info->header;

  if (file >= 0)
  {
    VOID(my_seek(file,filepos,MY_SEEK_SET,MYF(0)));
    if ((length=my_read(file,(char*) header,BLOCK_INFO_HEADER_LENGTH,MYF(0)))
	== MY_FILE_ERROR)
      return BLOCK_FATAL_ERROR;
    if (length != BLOCK_INFO_HEADER_LENGTH)
    {					/* Test if short block */
      if (length < 3)
      {
	my_errno=HA_ERR_WRONG_IN_RECORD;	 /* Garbage */
	return BLOCK_FATAL_ERROR;
      }
      bzero((byte*) header+length,BLOCK_INFO_HEADER_LENGTH-length);
    }
  }
  DBUG_DUMP("header",(byte*) header,BLOCK_INFO_HEADER_LENGTH);
  if (info->second_read)
  {
    if (info->header[0] <= 8)
      return_val=BLOCK_SYNC_ERROR;
  }
  else
  {
    if (info->header[0] > 8)
      return_val=BLOCK_SYNC_ERROR;
  }
  info->next_filepos= (ulong) NI_POS_ERROR; /* Dummy ifall no next block */

  switch (info->header[0]) {
  case 0:
    if ((info->block_len=(uint) uint3korr(header+1)) < N_MIN_BLOCK_LENGTH)
      return BLOCK_FATAL_ERROR;
    info->filepos=filepos;
    info->next_filepos=uint4korr(header+4);
    if (info->next_filepos == (uint32) ~0)	/* Fix for 64 bit long */
      info->next_filepos=NI_POS_ERROR;
    return return_val | BLOCK_DELETED;		/* Deleted block */
  case 1:
    info->rec_len=info->data_len=info->block_len=uint2korr(header+1);
    info->filepos=filepos+3;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 2:
    info->block_len=(info->rec_len=info->data_len=uint2korr(header+1))+1;
    info->filepos=filepos+3;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 3:
    info->rec_len=info->data_len=uint2korr(header+1);
    info->block_len=uint2korr(header+3);
    info->filepos=filepos+5;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 4:
    info->rec_len=uint2korr(header+1);
    info->block_len=info->data_len=uint2korr(header+3);
    info->next_filepos=uint4korr(header+5);
    info->second_read=1;
    info->filepos=filepos+9;
    return return_val | BLOCK_FIRST;
#if defined(_MSC_VER) || !defined(__WIN__)
  case 5:
    info->rec_len=info->data_len=info->block_len=uint3korr(header+1);
    info->filepos=filepos+4;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 6:
    info->block_len=(info->rec_len=info->data_len=uint3korr(header+1))+1;
    info->filepos=filepos+4;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 7:
    info->rec_len=info->data_len=uint3korr(header+1);
    info->block_len=uint3korr(header+4);
    info->filepos=filepos+7;
    return return_val | BLOCK_FIRST | BLOCK_LAST;
  case 8:
    info->rec_len=uint3korr(header+1);
    info->block_len=info->data_len=uint3korr(header+4);
    info->next_filepos=uint4korr(header+7);
    info->second_read=1;
    info->filepos=filepos+11;
    return return_val | BLOCK_FIRST;
#endif
  case 9:
    info->data_len=info->block_len=uint2korr(header+1);
    info->filepos=filepos+3;
    return return_val | BLOCK_LAST;
  case 10:
    info->block_len=(info->data_len=uint2korr(header+1))+1;
    info->filepos=filepos+3;
    return return_val | BLOCK_LAST;
  case 11:
    info->data_len=uint2korr(header+1);
    info->block_len=uint2korr(header+3);
    info->filepos=filepos+5;
    return return_val | BLOCK_LAST;
  case 12:
    info->data_len=info->block_len=uint2korr(header+1);
    info->next_filepos=uint4korr(header+3);
    info->second_read=1;
    info->filepos=filepos+7;
    return return_val;
#if defined(_MSC_VER) || !defined(__WIN__)
  case 13:
    info->data_len=info->block_len=uint3korr(header+1);
    info->filepos=filepos+4;
    return return_val | BLOCK_LAST;
  case 14:
    info->block_len=(info->data_len=uint3korr(header+1))+1;
    info->filepos=filepos+4;
    return return_val | BLOCK_LAST;
  case 15:
    info->data_len=uint3korr(header+1);
    info->block_len=uint3korr(header+4);
    info->filepos=filepos+7;
    return return_val | BLOCK_LAST;
  case 16:
    info->data_len=info->block_len=uint3korr(header+1);
    info->next_filepos=uint4korr(header+4);
    info->second_read=1;
    info->filepos=filepos+8;
    return return_val;
#endif
  default:
    my_errno=HA_ERR_WRONG_IN_RECORD;	 /* Garbage */
    return BLOCK_ERROR;
  }
}
