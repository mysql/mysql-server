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


/* Sorts a database */

#include "mysql_priv.h"
#ifdef HAVE_STDDEF_H
#include <stddef.h>			/* for macro offsetof */
#endif
#include <m_ctype.h>
#include "sql_sort.h"

#ifndef THREAD
#define SKIP_DBUG_IN_FILESORT
#endif

	/* How to write record_ref. */

#define WRITE_REF(file,from) \
if (my_b_write((file),(byte*) (from),param->ref_length)) \
  DBUG_RETURN(1);

	/* functions defined in this file */

static char **make_char_array(register uint fields, uint length, myf my_flag);
static BUFFPEK *read_buffpek_from_file(IO_CACHE *buffer_file, uint count);
static ha_rows find_all_keys(SORTPARAM *param,SQL_SELECT *select,
			     uchar * *sort_keys, IO_CACHE *buffer_file,
			     IO_CACHE *tempfile,IO_CACHE *indexfile);
static int write_keys(SORTPARAM *param,uchar * *sort_keys,
		      uint count, IO_CACHE *buffer_file, IO_CACHE *tempfile);
static void make_sortkey(SORTPARAM *param,uchar *to, byte *ref_pos);
static int merge_index(SORTPARAM *param,uchar *sort_buffer,
		       BUFFPEK *buffpek,
		       uint maxbuffer,IO_CACHE *tempfile,
		       IO_CACHE *outfile);
static bool save_index(SORTPARAM *param,uchar **sort_keys, uint count);
static uint sortlength(SORT_FIELD *sortorder,uint length);

/*
  Sort a table

  SYNOPSIS
    filesort()
    table		Table to sort
    sortorder		How to sort the table
    s_length		Number of elements in sortorder	
    select		condition to apply to the rows
    special		Not used.
			(This could be used to sort the rows pointed on by
			select->file)
   examined_rows	Store number of examined rows here

  IMPLEMENTATION
    Creates a set of pointers that can be used to read the rows
    in sorted order. This should be done with the functions
    in records.cc
  
  REQUIREMENTS
    Before calling filesort, one must have done
    table->file->info(HA_STATUS_VARIABLE)

  RETURN
    HA_POS_ERROR	Error
    #			Number of rows

    examined_rows	will be set to number of examined rows

    The result set is stored in table->io_cache or
    table->record_pointers
*/

ha_rows filesort(TABLE *table, SORT_FIELD *sortorder, uint s_length,
		 SQL_SELECT *select, ha_rows special, ha_rows max_rows,
		 ha_rows *examined_rows)
{
  int error;
  ulong memavl, min_sort_memory;
  uint maxbuffer;
  BUFFPEK *buffpek;
  ha_rows records;
  uchar **sort_keys;
  IO_CACHE tempfile, buffpek_pointers, *selected_records_file, *outfile; 
  SORTPARAM param;
  THD *thd= current_thd;

  DBUG_ENTER("filesort");
  DBUG_EXECUTE("info",TEST_filesort(sortorder,s_length,special););
#ifdef SKIP_DBUG_IN_FILESORT
  DBUG_PUSH("");		/* No DBUG here */
#endif

  outfile= table->io_cache;
  my_b_clear(&tempfile);
  my_b_clear(&buffpek_pointers);
  buffpek=0;
  sort_keys= (uchar **) NULL;
  error= 1;
  bzero((char*) &param,sizeof(param));
  param.ref_length= table->file->ref_length;
  param.sort_length=sortlength(sortorder,s_length)+ param.ref_length;
  param.max_rows= max_rows;

  if (select && select->quick)
  {
    statistic_increment(filesort_range_count, &LOCK_status);
  }
  else
  {
    statistic_increment(filesort_scan_count, &LOCK_status);
  }
  if (select && my_b_inited(&select->file))
  {
    records=special=select->records;		/* purecov: deadcode */
    selected_records_file= &select->file;	/* purecov: deadcode */
    reinit_io_cache(selected_records_file,READ_CACHE,0L,0,0); /* purecov: deadcode */
  }
  else if (special)
  {
    records=special;				/* purecov: deadcode */
    selected_records_file= outfile;		/* purecov: deadcode */
    reinit_io_cache(selected_records_file,READ_CACHE,0L,0,0); /* purecov: deadcode */
  }
#ifdef CAN_TRUST_RANGE
  else if (select && select->quick && select->quick->records > 0L)
  {
    records=min((ha_rows) (select->quick->records*2+EXTRA_RECORDS*2),
		table->file->records)+EXTRA_RECORDS;
    selected_records_file=0;
  }
#endif
  else
  {
    records=table->file->estimate_number_of_rows();
    selected_records_file= 0;
  }

#ifdef USE_STRCOLL
  if (use_strcoll(default_charset_info) &&
      !(param.tmp_buffer=my_malloc(param.sort_length,MYF(MY_WME))))
    goto err;
#endif

  memavl= thd->variables.sortbuff_size;
  min_sort_memory= max(MIN_SORT_MEMORY, param.sort_length*MERGEBUFF2);
  while (memavl >= min_sort_memory)
  {
    ulong old_memavl;
    ulong keys= memavl/(param.sort_length+sizeof(char*));
    param.keys=(uint) min(records+1, keys);
    if ((sort_keys= (uchar **) make_char_array(param.keys, param.sort_length,
					       MYF(0))))
      break;
    old_memavl=memavl;
    if ((memavl=memavl/4*3) < min_sort_memory && old_memavl > min_sort_memory)
      memavl= min_sort_memory;
  }
  if (memavl < min_sort_memory)
  {
    my_error(ER_OUTOFMEMORY,MYF(ME_ERROR+ME_WAITTANG),
	     thd->variables.sortbuff_size);
    goto err;
  }
  if (open_cached_file(&buffpek_pointers,mysql_tmpdir,TEMP_PREFIX,
		       DISK_BUFFER_SIZE, MYF(MY_WME)))
    goto err;

  param.keys--;
  param.sort_form= table;
  param.end=(param.local_sortorder=sortorder)+s_length;
  if ((records=find_all_keys(&param,select,sort_keys, &buffpek_pointers,
			     &tempfile, selected_records_file)) ==
      HA_POS_ERROR)
    goto err;
  maxbuffer= (uint) (my_b_tell(&buffpek_pointers)/sizeof(*buffpek));

  if (maxbuffer == 0)			// The whole set is in memory
  {
    if (save_index(&param,sort_keys,(uint) records))
      goto err;
  }
  else
  {
    if (!(buffpek=read_buffpek_from_file(&buffpek_pointers, maxbuffer)))
      goto err;
    close_cached_file(&buffpek_pointers);
	/* Open cached file if it isn't open */
    if (! my_b_inited(outfile) &&
	open_cached_file(outfile,mysql_tmpdir,TEMP_PREFIX,READ_RECORD_BUFFER,
			  MYF(MY_WME)))
      goto err;
    reinit_io_cache(outfile,WRITE_CACHE,0L,0,0);

    /*
      Use also the space previously used by string pointers in sort_buffer
      for temporary key storage.
    */
    param.keys=((param.keys*(param.sort_length+sizeof(char*))) /
		param.sort_length-1);
    maxbuffer--;				// Offset from 0
    if (merge_many_buff(&param,(uchar*) sort_keys,buffpek,&maxbuffer,
			&tempfile))
      goto err;
    if (flush_io_cache(&tempfile) ||
	reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
      goto err;
    if (merge_index(&param,(uchar*) sort_keys,buffpek,maxbuffer,&tempfile,
		    outfile))
      goto err;
  }
  if (records > param.max_rows)
    records=param.max_rows;
  error =0;

 err:
#ifdef USE_STRCOLL
  if (use_strcoll(default_charset_info))
    x_free(param.tmp_buffer);
#endif
  x_free((gptr) sort_keys);
  x_free((gptr) buffpek);
  close_cached_file(&tempfile);
  close_cached_file(&buffpek_pointers);
  if (my_b_inited(outfile))
  {
    if (flush_io_cache(outfile))
      error=1;
    {
      my_off_t save_pos=outfile->pos_in_file;
      /* For following reads */
      if (reinit_io_cache(outfile,READ_CACHE,0L,0,0))
	error=1;
      outfile->end_of_file=save_pos;
    }
  }
  if (error)
    my_error(ER_FILSORT_ABORT,MYF(ME_ERROR+ME_WAITTANG));
  else
    statistic_add(filesort_rows, (ulong) records, &LOCK_status);
  *examined_rows= param.examined_rows;
#ifdef SKIP_DBUG_IN_FILESORT
  DBUG_POP();			/* Ok to DBUG */
#endif
  DBUG_PRINT("exit",("records: %ld",records));
  DBUG_RETURN(error ? HA_POS_ERROR : records);
} /* filesort */


	/* Make a array of string pointers */

static char **make_char_array(register uint fields, uint length, myf my_flag)
{
  register char **pos;
  char **old_pos,*char_pos;
  DBUG_ENTER("make_char_array");

  if ((old_pos= (char**) my_malloc((uint) fields*(length+sizeof(char*)),
				    my_flag)))
  {
    pos=old_pos; char_pos=((char*) (pos+fields)) -length;
    while (fields--) *(pos++) = (char_pos+= length);
  }

  DBUG_RETURN(old_pos);
} /* make_char_array */


	/* Read all buffer pointers into memory */

static BUFFPEK *read_buffpek_from_file(IO_CACHE *buffpek_pointers, uint count)
{
  ulong length;
  BUFFPEK *tmp;
  DBUG_ENTER("read_buffpek_from_file");
  tmp=(BUFFPEK*) my_malloc(length=sizeof(BUFFPEK)*count, MYF(MY_WME));
  if (tmp)
  {
    if (reinit_io_cache(buffpek_pointers,READ_CACHE,0L,0,0) ||
	my_b_read(buffpek_pointers, (byte*) tmp, length))
    {
      my_free((char*) tmp, MYF(0));
      tmp=0;
    }      
  }
  DBUG_RETURN(tmp);
}



	/* Search after sort_keys and place them in a temp. file */

static ha_rows find_all_keys(SORTPARAM *param, SQL_SELECT *select,
			     uchar **sort_keys,
			     IO_CACHE *buffpek_pointers,
			     IO_CACHE *tempfile, IO_CACHE *indexfile)
{
  int error,flag,quick_select;
  uint idx,indexpos,ref_length;
  byte *ref_pos,*next_pos,ref_buff[MAX_REFLENGTH];
  my_off_t record;
  TABLE *sort_form;
  volatile bool *killed= &current_thd->killed;
  handler *file;
  DBUG_ENTER("find_all_keys");
  DBUG_PRINT("info",("using: %s",(select?select->quick?"ranges":"where":"every row")));

  idx=indexpos=0;
  error=quick_select=0;
  sort_form=param->sort_form;
  file=sort_form->file;
  ref_length=param->ref_length;
  ref_pos= ref_buff;
  quick_select=select && select->quick;
  record=0;
  flag= ((!indexfile && file->table_flags() & HA_REC_NOT_IN_SEQ)
	 || quick_select);
  if (indexfile || flag)
    ref_pos= &file->ref[0];
  next_pos=ref_pos;
  if (! indexfile && ! quick_select)
  {
    file->reset();			// QQ; Shouldn't be needed
    if (sort_form->key_read)		// QQ Can be removed after the reset
      file->extra(HA_EXTRA_KEYREAD);	// QQ is removed
    next_pos=(byte*) 0;			/* Find records in sequence */
    file->rnd_init();
    file->extra_opt(HA_EXTRA_CACHE,
		    current_thd->variables.read_buff_size);
  }

  for (;;)
  {
    if (quick_select)
    {
      if ((error=select->quick->get_next()))
	break;
      file->position(sort_form->record[0]);
    }
    else					/* Not quick-select */
    {
      if (indexfile)
      {
	if (my_b_read(indexfile,(byte*) ref_pos,ref_length)) /* purecov: deadcode */
	{
	  error= my_errno ? my_errno : -1;		/* Abort */
	  break;
	}
	error=file->rnd_pos(sort_form->record[0],next_pos);
      }
      else
      {
	error=file->rnd_next(sort_form->record[0]);
	if (!flag)
	{
	  ha_store_ptr(ref_pos,ref_length,record); // Position to row
	  record+=sort_form->db_record_offset;
	}
	else
	  file->position(sort_form->record[0]);
      }
      if (error && error != HA_ERR_RECORD_DELETED)
	break;
    }
    if (*killed)
    {
      DBUG_PRINT("info",("Sort killed by user"));
      (void) file->extra(HA_EXTRA_NO_CACHE);
      file->rnd_end();
      DBUG_RETURN(HA_POS_ERROR);		/* purecov: inspected */
    }
    if (error == 0)
      param->examined_rows++;
    if (error == 0 && (!select || select->skipp_record() == 0))
    {
      if (idx == param->keys)
      {
	if (write_keys(param,sort_keys,idx,buffpek_pointers,tempfile))
	  DBUG_RETURN(HA_POS_ERROR);
	idx=0;
	indexpos++;
      }
      make_sortkey(param,sort_keys[idx++],ref_pos);
    }
    else
      file->unlock_row();
  }
  (void) file->extra(HA_EXTRA_NO_CACHE);	/* End cacheing of records */
  file->rnd_end();
  DBUG_PRINT("test",("error: %d  indexpos: %d",error,indexpos));
  if (error != HA_ERR_END_OF_FILE)
  {
    file->print_error(error,MYF(ME_ERROR | ME_WAITTANG)); /* purecov: inspected */
    DBUG_RETURN(HA_POS_ERROR);			/* purecov: inspected */
  }
  if (indexpos && idx &&
      write_keys(param,sort_keys,idx,buffpek_pointers,tempfile))
    DBUG_RETURN(HA_POS_ERROR);			/* purecov: inspected */
  DBUG_RETURN(my_b_inited(tempfile) ?
	      (ha_rows) (my_b_tell(tempfile)/param->sort_length) :
	      idx);
} /* find_all_keys */


	/* Skriver en buffert med nycklar till filen */

static int
write_keys(SORTPARAM *param, register uchar **sort_keys, uint count,
	   IO_CACHE *buffpek_pointers, IO_CACHE *tempfile)
{
  uint sort_length;
  uchar **end;
  BUFFPEK buffpek;
  DBUG_ENTER("write_keys");

  sort_length=param->sort_length;
#ifdef MC68000
  quicksort(sort_keys,count,sort_length);
#else
  my_string_ptr_sort((gptr) sort_keys,(uint) count,sort_length);
#endif
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile,mysql_tmpdir,TEMP_PREFIX,DISK_BUFFER_SIZE,
			MYF(MY_WME)))
    goto err;					/* purecov: inspected */
  buffpek.file_pos=my_b_tell(tempfile);
  if ((ha_rows) count > param->max_rows)
    count=(uint) param->max_rows;		/* purecov: inspected */
  buffpek.count=(ha_rows) count;
  for (end=sort_keys+count ; sort_keys != end ; sort_keys++)
    if (my_b_write(tempfile,(byte*) *sort_keys,(uint) sort_length))
      goto err;
  if (my_b_write(buffpek_pointers, (byte*) &buffpek, sizeof(buffpek)))
    goto err;
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
} /* write_keys */


	/* makes a sort-key from record */

static void make_sortkey(register SORTPARAM *param,
			 register uchar *to, byte *ref_pos)
{
  reg3 Field *field;
  reg1 SORT_FIELD *sort_field;
  reg5 uint length;

  for (sort_field=param->local_sortorder ;
       sort_field != param->end ;
       sort_field++)
  {
    bool maybe_null=0;
    if ((field=sort_field->field))
    {						// Field
      if (field->maybe_null())
      {
	if (field->is_null())
	{
	  if (sort_field->reverse)
	    bfill(to,sort_field->length+1,(char) 255);
	  else
	    bzero((char*) to,sort_field->length+1);
	  to+= sort_field->length+1;
	  continue;
	}
	else
	  *to++=1;
      }
      field->sort_string((char*) to,sort_field->length);
    }
    else
    {						// Item
      Item *item=sort_field->item;
      switch (sort_field->result_type) {
      case STRING_RESULT:
	{
	  if ((maybe_null=item->maybe_null))
	    *to++=1;
	  /* All item->str() to use some extra byte for end null.. */
	  String tmp((char*) to,sort_field->length+4);
	  String *res=item->val_str(&tmp);
	  if (!res)
	  {
	    if (item->maybe_null)
	      bzero((char*) to-1,sort_field->length+1);
	    else
	    {
	      DBUG_PRINT("warning",
			 ("Got null on something that shouldn't be null"));
	      bzero((char*) to,sort_field->length);	// Avoid crash
	    }
	    break;
	  }
	  length=res->length();
	  int diff=(int) (sort_field->length-length);
	  if (diff < 0)
	  {
	    diff=0;				/* purecov: inspected */
	    length=sort_field->length;
	  }
#ifdef USE_STRCOLL
          if (use_strcoll(default_charset_info))
          {
            if (item->binary)
            {
              if (res->ptr() != (char*) to)
                memcpy(to,res->ptr(),length);
              bzero((char*) to+length,diff);
            }
            else
            {
              char *from=(char*) res->ptr();
              if ((unsigned char *)from == to)
              {
                set_if_smaller(length,sort_field->length);
                memcpy(param->tmp_buffer,from,length);
                from=param->tmp_buffer;
              }
              uint tmp_length=my_strnxfrm(default_charset_info,
                                          to,(unsigned char *) from,
                                          sort_field->length,
                                          length);
              if (tmp_length < sort_field->length)
                bzero((char*) to+tmp_length,sort_field->length-tmp_length);
            }
          }
          else
          {
#endif
            if (res->ptr() != (char*) to)
              memcpy(to,res->ptr(),length);
            bzero((char *)to+length,diff);
            if (!item->binary)
              case_sort((char*) to,length);
#ifdef USE_STRCOLL
          }
#endif
	  break;
	}
      case INT_RESULT:
	{
	  longlong value=item->val_int();
	  if ((maybe_null=item->maybe_null))
	    *to++=1;				/* purecov: inspected */
	  if (item->null_value)
	  {
	    if (item->maybe_null)
	      bzero((char*) to-1,sort_field->length+1);
	    else
	    {
	      DBUG_PRINT("warning",
			 ("Got null on something that shouldn't be null"));
	      bzero((char*) to,sort_field->length);
	    }
	    break;
	  }
#if SIZEOF_LONG_LONG > 4
	  to[7]= (uchar) value;
	  to[6]= (uchar) (value >> 8);
	  to[5]= (uchar) (value >> 16);
	  to[4]= (uchar) (value >> 24);
	  to[3]= (uchar) (value >> 32);
	  to[2]= (uchar) (value >> 40);
	  to[1]= (uchar) (value >> 48);
	  to[0]= (uchar) (value >> 56) ^ 128;	// Fix sign
#else
	  to[3]= (uchar) value;
	  to[2]= (uchar) (value >> 8);
	  to[1]= (uchar) (value >> 16);
	  to[0]= (uchar) (value >> 24) ^ 128;	// Fix sign
#endif
	  break;
	}
      case REAL_RESULT:
	{
	  double value=item->val();
	  if ((maybe_null=item->null_value))
	  {
	    bzero((char*) to,sort_field->length+1);
	    to++;
	    break;
	  }
	  if ((maybe_null=item->maybe_null))
	    *to++=1;
	  change_double_for_sort(value,(byte*) to);
	  break;
	}
      }
    }
    if (sort_field->reverse)
    {							/* Revers key */
      if (maybe_null)
        to[-1]= ~to[-1];
      length=sort_field->length;
      while (length--)
      {
	*to = (uchar) (~ *to);
	to++;
      }
    }
    else
      to+= sort_field->length;
  }
  memcpy((byte*) to,ref_pos,(size_s) param->ref_length);/* Save filepos last */
  return;
}


static bool save_index(SORTPARAM *param, uchar **sort_keys, uint count)
{
  uint offset,ref_length;
  byte *to;
  DBUG_ENTER("save_index");

  my_string_ptr_sort((gptr) sort_keys,(uint) count,param->sort_length);
  ref_length=param->ref_length;
  offset=param->sort_length-ref_length;
  if ((ha_rows) count > param->max_rows)
    count=(uint) param->max_rows;
  if (!(to=param->sort_form->record_pointers=
	(byte*) my_malloc(ref_length*count,MYF(MY_WME))))
    DBUG_RETURN(1);				/* purecov: inspected */
  for (uchar **end=sort_keys+count ; sort_keys != end ; sort_keys++)
  {
    memcpy(to,*sort_keys+offset,ref_length);
    to+=ref_length;
  }
  DBUG_RETURN(0);
}


	/* Merge buffers to make < MERGEBUFF2 buffers */

int merge_many_buff(SORTPARAM *param, uchar *sort_buffer,
		    BUFFPEK *buffpek, uint *maxbuffer, IO_CACHE *t_file)
{
  register int i;
  IO_CACHE t_file2,*from_file,*to_file,*temp;
  BUFFPEK *lastbuff;
  DBUG_ENTER("merge_many_buff");

  if (*maxbuffer < MERGEBUFF2)
    DBUG_RETURN(0);				/* purecov: inspected */
  if (flush_io_cache(t_file) ||
      open_cached_file(&t_file2,mysql_tmpdir,TEMP_PREFIX,DISK_BUFFER_SIZE,
			MYF(MY_WME)))
    DBUG_RETURN(1);				/* purecov: inspected */

  from_file= t_file ; to_file= &t_file2;
  while (*maxbuffer >= MERGEBUFF2)
  {
    reinit_io_cache(from_file,READ_CACHE,0L,0,0);
    reinit_io_cache(to_file,WRITE_CACHE,0L,0,0);
    lastbuff=buffpek;
    for (i=0 ; i <= (int) *maxbuffer-MERGEBUFF*3/2 ; i+=MERGEBUFF)
    {
      if (merge_buffers(param,from_file,to_file,sort_buffer,lastbuff++,
			buffpek+i,buffpek+i+MERGEBUFF-1,0))
	break;					/* purecov: inspected */
    }
    if (merge_buffers(param,from_file,to_file,sort_buffer,lastbuff++,
		      buffpek+i,buffpek+ *maxbuffer,0))
      break;					/* purecov: inspected */
    if (flush_io_cache(to_file))
      break;					/* purecov: inspected */
    temp=from_file; from_file=to_file; to_file=temp;
    setup_io_cache(from_file);
    setup_io_cache(to_file);
    *maxbuffer= (uint) (lastbuff-buffpek)-1;
  }
  close_cached_file(to_file);			// This holds old result
  if (to_file == t_file)
  {
    *t_file=t_file2;				// Copy result file
    setup_io_cache(t_file);
  }

  DBUG_RETURN(*maxbuffer >= MERGEBUFF2);	/* Return 1 if interrupted */
} /* merge_many_buff */


	/* Read data to buffer */
	/* This returns (uint) -1 if something goes wrong */

uint read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
		    uint sort_length)
{
  register uint count;
  uint length;

  if ((count=(uint) min((ha_rows) buffpek->max_keys,buffpek->count)))
  {
    if (my_pread(fromfile->file,(byte*) buffpek->base,
		 (length= sort_length*count),buffpek->file_pos,MYF_RW))
      return((uint) -1);			/* purecov: inspected */
    buffpek->key=buffpek->base;
    buffpek->file_pos+= length;			/* New filepos */
    buffpek->count-=	count;
    buffpek->mem_count= count;
  }
  return (count*sort_length);
} /* read_to_buffer */


	/* Merge buffers to one buffer */

int merge_buffers(SORTPARAM *param, IO_CACHE *from_file,
		  IO_CACHE *to_file, uchar *sort_buffer,
		  BUFFPEK *lastbuff, BUFFPEK *Fb, BUFFPEK *Tb,
		  int flag)
{
  int error;
  uint sort_length,offset;
  ulong maxcount;
  ha_rows max_rows,org_max_rows;
  my_off_t to_start_filepos;
  uchar *strpos;
  BUFFPEK *buffpek,**refpek;
  QUEUE queue;
  qsort2_cmp    cmp;
  volatile bool *killed= &current_thd->killed;
  bool not_killable;
  DBUG_ENTER("merge_buffers");

  statistic_increment(filesort_merge_passes, &LOCK_status);
  if (param->not_killable)
  {
    killed= &not_killable;
    not_killable=0;
  }

  error=0;
  offset=(sort_length=param->sort_length)-param->ref_length;
  maxcount=(ulong) (param->keys/((uint) (Tb-Fb) +1));
  to_start_filepos=my_b_tell(to_file);
  strpos=(uchar*) sort_buffer;
  org_max_rows=max_rows=param->max_rows;

  if (init_queue(&queue,(uint) (Tb-Fb)+1,offsetof(BUFFPEK,key),0,
		 (queue_compare)
		 (cmp=get_ptr_compare(sort_length)),(void*) &sort_length))
    DBUG_RETURN(1);				/* purecov: inspected */
  for (buffpek= Fb ; buffpek <= Tb ; buffpek++)
  {
    buffpek->base= strpos;
    buffpek->max_keys=maxcount;
    strpos+= (uint) (error=(int) read_to_buffer(from_file,buffpek,
						sort_length));
    if (error == -1)
      goto err;					/* purecov: inspected */
    buffpek->max_keys= buffpek->mem_count;	// If less data in buffers than expected
    queue_insert(&queue,(byte*) buffpek);
  }

  if (param->unique_buff)
  {
    /* 
       Called by Unique::get()
       Copy the first argument to param->unique_buff for unique removal.
       Store it also in 'to_file'.

       This is safe as we know that there is always more than one element
       in each block to merge (This is guaranteed by the Unique:: algorithm
    */
    buffpek=(BUFFPEK*) queue_top(&queue);
    memcpy(param->unique_buff, buffpek->key, sort_length);
    if (my_b_write(to_file,(byte*) buffpek->key, sort_length))
    {
      error=1; goto err;			/* purecov: inspected */
    }
    buffpek->key+=sort_length;
    buffpek->mem_count--;
    if (!--max_rows)
    {
      error=0;					/* purecov: inspected */
      goto end;					/* purecov: inspected */
    }
    queue_replaced(&queue);			// Top element has been used
  }
  else
    cmp=0;					// Not unique

  while (queue.elements > 1)
  {
    if (*killed)
    {
      error=1; goto err;                        /* purecov: inspected */
    }
    for (;;)
    {
      buffpek=(BUFFPEK*) queue_top(&queue);
      if (cmp)					// Remove duplicates
      {
	if (!(*cmp)(&sort_length, &(param->unique_buff),
		    (uchar**) &buffpek->key))
	  goto skip_duplicate;
	memcpy(param->unique_buff, (uchar*) buffpek->key,sort_length);
      }
      if (flag == 0)
      {
	if (my_b_write(to_file,(byte*) buffpek->key, sort_length))
	{
	  error=1; goto err;			/* purecov: inspected */
	}
      }
      else
      {
	WRITE_REF(to_file,(byte*) buffpek->key+offset);
      }
      if (!--max_rows)
      {
	error=0;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }

    skip_duplicate:
      buffpek->key+=sort_length;
      if (! --buffpek->mem_count)
      {
	if (!(error=(int) read_to_buffer(from_file,buffpek,
					 sort_length)))
	{
	  uchar *base=buffpek->base;
	  ulong max_keys=buffpek->max_keys;

	  VOID(queue_remove(&queue,0));

	  /* Put room used by buffer to use in other buffer */
	  for (refpek= (BUFFPEK**) &queue_top(&queue);
	       refpek <= (BUFFPEK**) &queue_end(&queue);
	       refpek++)
	  {
	    buffpek= *refpek;
	    if (buffpek->base+buffpek->max_keys*sort_length == base)
	    {
	      buffpek->max_keys+=max_keys;
	      break;
	    }
	    else if (base+max_keys*sort_length == buffpek->base)
	    {
	      buffpek->base=base;
	      buffpek->max_keys+=max_keys;
	      break;
	    }
	  }
	  break;			/* One buffer have been removed */
	}
	else if (error == -1)
	  goto err;			/* purecov: inspected */
      }
      queue_replaced(&queue);		/* Top element has been replaced */
    }
  }
  buffpek=(BUFFPEK*) queue_top(&queue);
  buffpek->base= sort_buffer;
  buffpek->max_keys=param->keys;

  /*
    As we know all entries in the buffer are unique, we only have to
    check if the first one is the same as the last one we wrote
  */
  if (cmp)
  {
    if (!(*cmp)(&sort_length, &(param->unique_buff), (uchar**) &buffpek->key))
    {
      buffpek->key+=sort_length;	// Remove duplicate
      --buffpek->mem_count;
    }
  }

  do
  {
    if ((ha_rows) buffpek->mem_count > max_rows)
    {					/* Don't write too many records */
      buffpek->mem_count=(uint) max_rows;
      buffpek->count=0;			/* Don't read more */
    }
    max_rows-=buffpek->mem_count;
    if (flag == 0)
    {
      if (my_b_write(to_file,(byte*) buffpek->key,
		     (sort_length*buffpek->mem_count)))
      {
	error=1; goto err;			/* purecov: inspected */
      }
    }
    else
    {
      register uchar *end;
      strpos= buffpek->key+offset;
      for (end=strpos+buffpek->mem_count*sort_length;
	   strpos != end ;
	   strpos+=sort_length)
      {
	WRITE_REF(to_file,strpos);
      }
    }
  }
  while ((error=(int) read_to_buffer(from_file,buffpek,sort_length))
	 != -1 && error != 0);

end:
  lastbuff->count=min(org_max_rows-max_rows,param->max_rows);
  lastbuff->file_pos=to_start_filepos;
err:
  delete_queue(&queue);
  DBUG_RETURN(error);
} /* merge_buffers */


	/* Do a merge to output-file (save only positions) */

static int merge_index(SORTPARAM *param, uchar *sort_buffer,
		       BUFFPEK *buffpek, uint maxbuffer,
		       IO_CACHE *tempfile, IO_CACHE *outfile)
{
  DBUG_ENTER("merge_index");
  if (merge_buffers(param,tempfile,outfile,sort_buffer,buffpek,buffpek,
		    buffpek+maxbuffer,1))
    DBUG_RETURN(1);				/* purecov: inspected */
  DBUG_RETURN(0);
} /* merge_index */


	/* Calculate length of sort key */

static uint
sortlength(SORT_FIELD *sortorder, uint s_length)
{
  reg2 uint length;
  THD *thd= current_thd;

  length=0;
  for (; s_length-- ; sortorder++)
  {
    if (sortorder->field)
    {
      if (sortorder->field->type() == FIELD_TYPE_BLOB)
	sortorder->length= thd->variables.max_sort_length;
      else
      {
	sortorder->length=sortorder->field->pack_length();
#ifdef USE_STRCOLL
	if (use_strcoll(default_charset_info) && !sortorder->field->binary())
	  sortorder->length= sortorder->length*MY_STRXFRM_MULTIPLY;
#endif
      }
      if (sortorder->field->maybe_null())
	length++;				// Place for NULL marker
    }
    else
    {
      switch ((sortorder->result_type=sortorder->item->result_type())) {
      case STRING_RESULT:
	sortorder->length=sortorder->item->max_length;
#ifdef USE_STRCOLL
	if (use_strcoll(default_charset_info) && !sortorder->item->binary)
	  sortorder->length= sortorder->length*MY_STRXFRM_MULTIPLY;
#endif
	break;
      case INT_RESULT:
#if SIZEOF_LONG_LONG > 4
	sortorder->length=8;			// Size of intern longlong
#else
	sortorder->length=4;
#endif
	break;
      case REAL_RESULT:
	sortorder->length=sizeof(double);
	break;
      }
      if (sortorder->item->maybe_null)
	length++;				// Place for NULL marker
    }
    set_if_smaller(sortorder->length, thd->variables.max_sort_length);
    length+=sortorder->length;
  }
  sortorder->field= (Field*) 0;			// end marker
  DBUG_PRINT("info",("sort_length: %d",length));
  return length;
}


/*
** functions to change a double or float to a sortable string
** The following should work for IEEE
*/

#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

void change_double_for_sort(double nr,byte *to)
{
  uchar *tmp=(uchar*) to;
  if (nr == 0.0)
  {						/* Change to zero string */
    tmp[0]=(uchar) 128;
    bzero((char*) tmp+1,sizeof(nr)-1);
  }
  else
  {
#ifdef WORDS_BIGENDIAN
    memcpy_fixed(tmp,&nr,sizeof(nr));
#else
    {
      uchar *ptr= (uchar*) &nr;
#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
      tmp[0]= ptr[3]; tmp[1]=ptr[2]; tmp[2]= ptr[1]; tmp[3]=ptr[0];
      tmp[4]= ptr[7]; tmp[5]=ptr[6]; tmp[6]= ptr[5]; tmp[7]=ptr[4];
#else
      tmp[0]= ptr[7]; tmp[1]=ptr[6]; tmp[2]= ptr[5]; tmp[3]=ptr[4];
      tmp[4]= ptr[3]; tmp[5]=ptr[2]; tmp[6]= ptr[1]; tmp[7]=ptr[0];
#endif
    }
#endif
    if (tmp[0] & 128)				/* Negative */
    {						/* make complement */
      uint i;
      for (i=0 ; i < sizeof(nr); i++)
	tmp[i]=tmp[i] ^ (uchar) 255;
    }
    else
    {					/* Set high and move exponent one up */
      ushort exp_part=(((ushort) tmp[0] << 8) | (ushort) tmp[1] |
		       (ushort) 32768);
      exp_part+= (ushort) 1 << (16-1-DBL_EXP_DIG);
      tmp[0]= (uchar) (exp_part >> 8);
      tmp[1]= (uchar) exp_part;
    }
  }
}
