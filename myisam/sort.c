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

/*
  Creates a index for a database by reading keys, sorting them and outputing
  them in sorted order through SORT_INFO functions.
*/

#include "myisamdef.h"
#if defined(MSDOS) || defined(__WIN__)
#include <fcntl.h>
#else
#include <stddef.h>
#endif
#include <queues.h>

	/* static variabels */

#define MERGEBUFF 15
#define MERGEBUFF2 31
#define MIN_SORT_MEMORY (4096-MALLOC_OVERHEAD)
#define MYF_RW	MYF(MY_NABP | MY_WME | MY_WAIT_IF_FULL)
#define DISK_BUFFER_SIZE (IO_SIZE*16)

typedef struct st_buffpek {
  my_off_t file_pos;			/* position to buffer */
  ha_rows count;			/* keys in buffer */
  uchar *base,*key;			/* Pekare inom sort_key - indexdel */
  uint mem_count;			/* keys left in memory */
  uint max_keys;			/* Max keys in buffert */
} BUFFPEK;

extern void print_error _VARARGS((const char *fmt,...));

	/* functions defined in this file */

static ha_rows NEAR_F find_all_keys(MI_SORT_PARAM *info,uint keys,
				    uchar **sort_keys,
				    BUFFPEK *buffpek,int *maxbuffer,
				    IO_CACHE *tempfile);
static int NEAR_F write_keys(MI_SORT_PARAM *info,uchar * *sort_keys,
			     uint count, BUFFPEK *buffpek,IO_CACHE *tempfile);
static int NEAR_F write_index(MI_SORT_PARAM *info,uchar * *sort_keys,
			      uint count);
static int NEAR_F merge_many_buff(MI_SORT_PARAM *info,uint keys,
				  uchar * *sort_keys,
				  BUFFPEK *buffpek,int *maxbuffer,
				  IO_CACHE *t_file);
static uint NEAR_F read_to_buffer(IO_CACHE *fromfile,BUFFPEK *buffpek,
				  uint sort_length);
static int NEAR_F merge_buffers(MI_SORT_PARAM *info,uint keys,
				IO_CACHE *from_file, IO_CACHE *to_file,
				uchar * *sort_keys, BUFFPEK *lastbuff,
				BUFFPEK *Fb, BUFFPEK *Tb);
static int NEAR_F merge_index(MI_SORT_PARAM *,uint,uchar **,BUFFPEK *, int,
			      IO_CACHE *);
static char **make_char_array(uint fields,uint length,myf my_flag);

	/* Creates a index of sorted keys */
	/* Returns 0 if everything went ok */

int _create_index_by_sort(MI_SORT_PARAM *info,my_bool no_messages,
			  ulong sortbuff_size)
{
  int error,maxbuffer,skr;
  uint memavl,old_memavl,keys,sort_length;
  BUFFPEK *buffpek;
  ha_rows records;
  uchar **sort_keys;
  IO_CACHE tempfile;
  DBUG_ENTER("_create_index_by_sort");
  DBUG_PRINT("enter",("sort_length: %d", info->key_length));

  my_b_clear(&tempfile);
  buffpek= (BUFFPEK *) NULL; sort_keys= (uchar **) NULL; error= 1;
  maxbuffer=1;

  memavl=max(sortbuff_size,MIN_SORT_MEMORY);
  records=	info->max_records;
  sort_length=	info->key_length;
  LINT_INIT(keys);

  while (memavl >= MIN_SORT_MEMORY)
  {
    if ((my_off_t) (records+1)*(sort_length+sizeof(char*)) <=
	(my_off_t) memavl)
      keys= records+1;
    else
      do
      {
	skr=maxbuffer;
	if (memavl < sizeof(BUFFPEK)*(uint) maxbuffer ||
	    (keys=(memavl-sizeof(BUFFPEK)*(uint) maxbuffer)/
	     (sort_length+sizeof(char*))) <= 1)
	{
	  mi_check_print_error(info->sort_info->param,
			       "sort_buffer_size is to small");
	  goto err;
	}
      }
      while ((maxbuffer= (int) (records/(keys-1)+1)) != skr);

    if ((sort_keys= (uchar **) make_char_array(keys,sort_length,MYF(0))))
    {
      if ((buffpek = (BUFFPEK*) my_malloc((uint) (sizeof(BUFFPEK)*
						  (uint) maxbuffer),
					  MYF(0))))
	break;
      else
	my_free((gptr) sort_keys,MYF(0));
    }
    old_memavl=memavl;
    if ((memavl=memavl/4*3) < MIN_SORT_MEMORY && old_memavl > MIN_SORT_MEMORY)
      memavl=MIN_SORT_MEMORY;
  }
  if (memavl < MIN_SORT_MEMORY)
  {
    mi_check_print_error(info->sort_info->param,"Sort buffer to small"); /* purecov: tested */
    goto err; /* purecov: tested */
  }
  (*info->lock_in_memory)(info->sort_info->param);/* Everything is allocated */

  if (!no_messages)
    printf("  - Searching for keys, allocating buffer for %d keys\n",keys);

  if ((records=find_all_keys(info,keys,sort_keys,buffpek,&maxbuffer,&tempfile))
      == HA_POS_ERROR)
    goto err; /* purecov: tested */
  if (maxbuffer == 0)
  {
    if (!no_messages)
      printf("  - Dumping %lu keys\n",records);
    if (write_index(info,sort_keys,(uint) records))
      goto err; /* purecov: inspected */
  }
  else
  {
    keys=(keys*(sort_length+sizeof(char*)))/sort_length;
    if (maxbuffer >= MERGEBUFF2)
    {
      if (!no_messages)
	printf("  - Merging %lu keys\n",records); /* purecov: tested */
      if (merge_many_buff(info,keys,sort_keys,buffpek,&maxbuffer,&tempfile))
	goto err; /* purecov: inspected */
    }
    if (flush_io_cache(&tempfile) ||
	reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
      goto err; /* purecov: inspected */
    if (!no_messages)
      puts("  - Last merge and dumping keys"); /* purecov: tested */
    if (merge_index(info,keys,sort_keys,buffpek,maxbuffer,&tempfile))
      goto err; /* purecov: inspected */
  }
  error =0;

err:
  if (sort_keys)
    my_free((gptr) sort_keys,MYF(0));
  if (buffpek)
    my_free((gptr) buffpek,MYF(0));
  close_cached_file(&tempfile);

  DBUG_RETURN(error ? -1 : 0);
} /* _create_index_by_sort */


	/* Search after all keys and place them in a temp. file */

static ha_rows NEAR_F find_all_keys(MI_SORT_PARAM *info, uint keys,
				    uchar **sort_keys, BUFFPEK *buffpek,
				    int *maxbuffer, IO_CACHE *tempfile)
{
  int error;
  uint idx,indexpos;
  DBUG_ENTER("find_all_keys");

  idx=indexpos=error=0;

  while (!(error=(*info->key_read)(info->sort_info,sort_keys[idx])))
  {
    if ((uint) ++idx == keys)
    {
      if (indexpos >= (uint) *maxbuffer ||
	  write_keys(info,sort_keys,idx-1,buffpek+indexpos,tempfile))
	DBUG_RETURN(HA_POS_ERROR); /* purecov: inspected */
      memcpy(sort_keys[0],sort_keys[idx-1],(size_t) info->key_length);
      idx=1; indexpos++;
    }
  }
  if (error > 0)
    DBUG_RETURN(HA_POS_ERROR);		/* Aborted by get_key */ /* purecov: inspected */
  if (indexpos)
    if (indexpos >= (uint) *maxbuffer ||
	write_keys(info,sort_keys,idx,buffpek+indexpos,tempfile))
      DBUG_RETURN(HA_POS_ERROR); /* purecov: inspected */
  *maxbuffer=(int) indexpos;
  DBUG_RETURN(indexpos*(keys-1)+idx);
} /* find_all_keys */


	/* Write all keys in memory to file for later merge */

static int NEAR_F write_keys(MI_SORT_PARAM *info, register uchar **sort_keys,
			     uint count, BUFFPEK *buffpek,
			     IO_CACHE *tempfile)
{
  uchar **end;
  uint sort_length=info->key_length;
  DBUG_ENTER("write_keys");

  qsort2((byte*) sort_keys,count,sizeof(byte*),(qsort2_cmp) info->key_cmp,
	info->sort_info);
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, info->tmpdir, "ST", DISK_BUFFER_SIZE,
		       info->myf_rw))
    DBUG_RETURN(1); /* purecov: inspected */
  buffpek->file_pos=my_b_tell(tempfile);
  buffpek->count=count;

  for (end=sort_keys+count ; sort_keys != end ; sort_keys++)
    if (my_b_write(tempfile,(byte*) *sort_keys,(uint) sort_length))
      DBUG_RETURN(1); /* purecov: inspected */
  DBUG_RETURN(0);
} /* write_keys */


	/* Write index */

static int NEAR_F write_index(MI_SORT_PARAM *info, register uchar **sort_keys,
			      register uint count)
{
  DBUG_ENTER("write_index");

  qsort2((gptr) sort_keys,(size_t) count,sizeof(byte*),
	(qsort2_cmp) info->key_cmp,info->sort_info);
  while (count--)
    if ((*info->key_write)(info->sort_info,*sort_keys++))
      DBUG_RETURN(-1); /* purecov: inspected */
  DBUG_RETURN(0);
} /* write_index */


	/* Merge buffers to make < MERGEBUFF2 buffers */

static int NEAR_F merge_many_buff(MI_SORT_PARAM *info, uint keys,
				  uchar **sort_keys, BUFFPEK *buffpek,
				  int *maxbuffer, IO_CACHE *t_file)
{
  register int i;
  IO_CACHE t_file2, *from_file, *to_file, *temp;
  BUFFPEK *lastbuff;
  DBUG_ENTER("merge_many_buff");

  if (*maxbuffer < MERGEBUFF2)
    DBUG_RETURN(0);				/* purecov: inspected */
  if (flush_io_cache(t_file) ||
      open_cached_file(&t_file2,info->tmpdir,"ST",DISK_BUFFER_SIZE,
		       info->myf_rw))
    DBUG_RETURN(1);				/* purecov: inspected */

  from_file= t_file ; to_file= &t_file2;
  while (*maxbuffer >= MERGEBUFF2)
  {
    reinit_io_cache(from_file,READ_CACHE,0L,0,0);
    reinit_io_cache(to_file,WRITE_CACHE,0L,0,0);
    lastbuff=buffpek;
    for (i=0 ; i <= *maxbuffer-MERGEBUFF*3/2 ; i+=MERGEBUFF)
    {
      if (merge_buffers(info,keys,from_file,to_file,sort_keys,lastbuff++,
			buffpek+i,buffpek+i+MERGEBUFF-1))
	break; /* purecov: inspected */
    }
    if (merge_buffers(info,keys,from_file,to_file,sort_keys,lastbuff++,
		      buffpek+i,buffpek+ *maxbuffer))
      break; /* purecov: inspected */
    if (flush_io_cache(to_file))
      break;					/* purecov: inspected */
    temp=from_file; from_file=to_file; to_file=temp;
    *maxbuffer= (int) (lastbuff-buffpek)-1;
  }
  close_cached_file(to_file);			/* This holds old result */
  if (to_file == t_file)
    *t_file=t_file2;				/* Copy result file */

  DBUG_RETURN(*maxbuffer >= MERGEBUFF2);	/* Return 1 if interrupted */
} /* merge_many_buff */


	/* Read data to buffer */
	/* This returns (uint) -1 if something goes wrong */

static uint NEAR_F read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
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
	/* If to_file == 0 then use info->key_write */

static int NEAR_F
merge_buffers(MI_SORT_PARAM *info, uint keys, IO_CACHE *from_file, 
	      IO_CACHE *to_file, uchar **sort_keys, BUFFPEK *lastbuff,
	      BUFFPEK *Fb, BUFFPEK *Tb)
{
  int error;
  uint sort_length,maxcount;
  ha_rows count;
  my_off_t to_start_filepos;
  uchar *strpos;
  BUFFPEK *buffpek,**refpek;
  QUEUE queue;
  DBUG_ENTER("merge_buffers");

  count=error=0;
  maxcount=keys/((uint) (Tb-Fb) +1);
  LINT_INIT(to_start_filepos);
  if (to_file)
    to_start_filepos=my_b_tell(to_file);
  strpos=(uchar*) sort_keys;
  sort_length=info->key_length;

  if (init_queue(&queue,(uint) (Tb-Fb)+1,offsetof(BUFFPEK,key),0,
		 (int (*)(void*, byte *,byte*)) info->key_cmp,
		 (void*) info->sort_info))
    DBUG_RETURN(1); /* purecov: inspected */

  for (buffpek= Fb ; buffpek <= Tb ; buffpek++)
  {
    count+= buffpek->count;
    buffpek->base= strpos;
    buffpek->max_keys=maxcount;
    strpos+= (uint) (error=(int) read_to_buffer(from_file,buffpek,
						sort_length));
    if (error == -1)
      goto err; /* purecov: inspected */
    queue_insert(&queue,(void*) buffpek);
  }

  while (queue.elements > 1)
  {
    for (;;)
    {
      buffpek=(BUFFPEK*) queue_top(&queue);
      if (to_file)
      {
	if (my_b_write(to_file,(byte*) buffpek->key,(uint) sort_length))
	{
	  error=1; goto err; /* purecov: inspected */
	}
      }
      else
      {
	if ((*info->key_write)(info->sort_info,(void*) buffpek->key))
	{
	  error=1; goto err; /* purecov: inspected */
	}
      }
      buffpek->key+=sort_length;
      if (! --buffpek->mem_count)
      {
	if (!(error=(int) read_to_buffer(from_file,buffpek,sort_length)))
	{
	  uchar *base=buffpek->base;
	  uint max_keys=buffpek->max_keys;

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
	  break;		/* One buffer have been removed */
	}
      }
      else if (error == -1)
	goto err;		/* purecov: inspected */
      queue_replaced(&queue);	/* Top element has been replaced */
    }
  }
  buffpek=(BUFFPEK*) queue_top(&queue);
  buffpek->base=(uchar *) sort_keys;
  buffpek->max_keys=keys;
  do
  {
    if (to_file)
    {
      if (my_b_write(to_file,(byte*) buffpek->key,
		     (sort_length*buffpek->mem_count)))
      {
	error=1; goto err; /* purecov: inspected */
      }
    }
    else
    {
      register uchar *end;
      strpos= buffpek->key;
      for (end=strpos+buffpek->mem_count*sort_length;
	   strpos != end ;
	   strpos+=sort_length)
      {
	if ((*info->key_write)(info->sort_info,(void*) strpos))
	{
	  error=1; goto err; /* purecov: inspected */
	}
      }
    }
  }
  while ((error=(int) read_to_buffer(from_file,buffpek,sort_length)) != -1 &&
	 error != 0);

  lastbuff->count=count;
  if (to_file)
    lastbuff->file_pos=to_start_filepos;
err:
  delete_queue(&queue);
  DBUG_RETURN(error);
} /* merge_buffers */


	/* Do a merge to output-file (save only positions) */

static int NEAR_F
merge_index(MI_SORT_PARAM *info, uint keys, uchar **sort_keys,
	    BUFFPEK *buffpek, int maxbuffer, IO_CACHE *tempfile)
{
  DBUG_ENTER("merge_index");
  if (merge_buffers(info,keys,tempfile,(IO_CACHE*) 0,sort_keys,buffpek,buffpek,
		    buffpek+maxbuffer))
    DBUG_RETURN(1); /* purecov: inspected */
  DBUG_RETURN(0);
} /* merge_index */


	/* Make a pointer of arrays to keys */

static char **make_char_array(register uint fields, uint length, myf my_flag)
{
  register char **pos;
  char **old_pos,*char_pos;
  DBUG_ENTER("make_char_array");

  if ((old_pos= (char**) my_malloc( fields*(length+sizeof(char*)), my_flag)))
  {
    pos=old_pos; char_pos=((char*) (pos+fields)) -length;
    while (fields--)
      *(pos++) = (char_pos+= length);
  }

  DBUG_RETURN(old_pos);
} /* make_char_array */
