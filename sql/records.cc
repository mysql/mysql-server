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


/* Functions to read, write and lock records */

#include "mysql_priv.h"

static int rr_quick(READ_RECORD *info);
static int rr_sequential(READ_RECORD *info);
static int rr_from_tempfile(READ_RECORD *info);
static int rr_from_pointers(READ_RECORD *info);
static int rr_from_cache(READ_RECORD *info);
static int init_rr_cache(READ_RECORD *info);
static int rr_cmp(uchar *a,uchar *b);

	/* init struct for read with info->read_record */

void init_read_record(READ_RECORD *info,THD *thd, TABLE *table,
		      SQL_SELECT *select,
		      int use_record_cache, bool print_error)
{
  IO_CACHE *tempfile;
  DBUG_ENTER("init_read_record");

  bzero((char*) info,sizeof(*info));
  info->thd=thd;
  info->table=table;
  info->file= table->file;
  info->forms= &info->table;		/* Only one table */
  info->record=table->record[0];
  info->ref_length=table->file->ref_length;
  info->select=select;
  info->print_error=print_error;
  table->status=0;			/* And it's allways found */

  if (select && my_b_inited(&select->file))
    tempfile= &select->file;
  else
    tempfile= table->io_cache;
  if (select && select->quick && (! tempfile || !tempfile->buffer))
  {
    DBUG_PRINT("info",("using rr_quick"));
    info->read_record=rr_quick;
  }
  else if (tempfile && my_b_inited(tempfile)) // Test if ref-records was used
  {
    DBUG_PRINT("info",("using rr_from_tempfile"));
    info->read_record=rr_from_tempfile;
    info->io_cache=tempfile;
    reinit_io_cache(info->io_cache,READ_CACHE,0L,0,0);
    info->ref_pos=table->file->ref;
    table->file->rnd_init(0);

    if (! (specialflag & SPECIAL_SAFE_MODE) &&
	my_default_record_cache_size &&
	!table->file->fast_key_read() &&
	(table->db_stat & HA_READ_ONLY ||
	 table->reginfo.lock_type == TL_READ) &&
	(ulonglong) table->reclength*(table->file->records+
				      table->file->deleted) >
	(ulonglong) MIN_FILE_LENGTH_TO_USE_ROW_CACHE &&
	info->io_cache->end_of_file/info->ref_length*table->reclength >
	(my_off_t) MIN_ROWS_TO_USE_TABLE_CACHE &&
	!table->blob_fields)
    {
      if (! init_rr_cache(info))
      {
	DBUG_PRINT("info",("using rr_from_cache"));
	info->read_record=rr_from_cache;
      }
    }
  }
  else if (table->record_pointers)
  {
    table->file->rnd_init(0);
    info->cache_pos=table->record_pointers;
    info->cache_end=info->cache_pos+ table->found_records*info->ref_length;
    info->read_record= rr_from_pointers;
  }
  else
  {
    DBUG_PRINT("info",("using rr_sequential"));
    info->read_record=rr_sequential;
    table->file->rnd_init();
    /* We can use record cache if we don't update dynamic length tables */
    if (use_record_cache > 0 ||
	(int) table->reginfo.lock_type <= (int) TL_READ_HIGH_PRIORITY ||
	!(table->db_options_in_use & HA_OPTION_PACK_RECORD) ||
	(use_record_cache < 0 &&
	 !(table->file->option_flag() & HA_NOT_DELETE_WITH_CACHE)))
      VOID(table->file->extra(HA_EXTRA_CACHE));	// Cache reads
  }
  DBUG_VOID_RETURN;
} /* init_read_record */


void end_read_record(READ_RECORD *info)
{					/* free cache if used */
  if (info->cache)
  {
    my_free_lock((char*) info->cache,MYF(0));
    info->cache=0;
  }
  if (info->table)
  {
    (void) info->file->extra(HA_EXTRA_NO_CACHE);
    (void) info->file->rnd_end();
    info->table=0;
  }
}

	/* Read a record from head-database */

static int rr_quick(READ_RECORD *info)
{
  int tmp=info->select->quick->get_next();
  if (tmp)
  {
    if (tmp == HA_ERR_END_OF_FILE)
      tmp= -1;
    else if (info->print_error)
      info->file->print_error(tmp,MYF(0));
  }
  return tmp;
}


static int rr_sequential(READ_RECORD *info)
{
  int tmp;
  while ((tmp=info->file->rnd_next(info->record)))
  {
    if (info->thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0));
      return 1;
    }
    if (tmp != HA_ERR_RECORD_DELETED)
    {
      if (tmp == HA_ERR_END_OF_FILE)
	tmp= -1;
      else if (info->print_error)
	info->table->file->print_error(tmp,MYF(0));
      break;
    }
  }
  return tmp;
}


static int rr_from_tempfile(READ_RECORD *info)
{
  int tmp;
tryNext:
  if (my_b_read(info->io_cache,info->ref_pos,info->ref_length))
    return -1;					/* End of file */
  tmp=info->file->rnd_pos(info->record,info->ref_pos);
  if (tmp)
  {
    if (tmp == HA_ERR_END_OF_FILE)
      tmp= -1;
    else if (tmp == HA_ERR_RECORD_DELETED)
      goto tryNext;
    else if (info->print_error)
      info->file->print_error(tmp,MYF(0));
  }
  return tmp;
} /* rr_from_tempfile */

static int rr_from_pointers(READ_RECORD *info)
{
  byte *cache_pos;
tryNext:
  if (info->cache_pos == info->cache_end)
    return -1;					/* End of file */
  cache_pos=info->cache_pos;
  info->cache_pos+=info->ref_length;

  int tmp=info->file->rnd_pos(info->record,cache_pos);
  if (tmp)
  {
    if (tmp == HA_ERR_END_OF_FILE)
      tmp= -1;
    else if (tmp == HA_ERR_RECORD_DELETED)
      goto tryNext;
    else if (info->print_error)
      info->file->print_error(tmp,MYF(0));
  }
  return tmp;
}

	/* cacheing of records from a database */

static int init_rr_cache(READ_RECORD *info)
{
  uint rec_cache_size;
  DBUG_ENTER("init_rr_cache");

  info->struct_length=3+MAX_REFLENGTH;
  info->reclength=ALIGN_SIZE(info->table->reclength+1);
  if (info->reclength < info->struct_length)
    info->reclength=ALIGN_SIZE(info->struct_length);

  info->error_offset=info->table->reclength;
  info->cache_records=my_default_record_cache_size/
    (info->reclength+info->struct_length);
  rec_cache_size=info->cache_records*info->reclength;
  info->rec_cache_size=info->cache_records*info->ref_length;

  if (info->cache_records <= 2 ||
      !(info->cache=(byte*) my_malloc_lock(rec_cache_size+info->cache_records*
					   info->struct_length,
					   MYF(0))))
    DBUG_RETURN(1);
#ifdef HAVE_purify
  bzero(info->cache,rec_cache_size);		// Avoid warnings in qsort
#endif
  DBUG_PRINT("info",("Allocated buffert for %d records",info->cache_records));
  info->read_positions=info->cache+rec_cache_size;
  info->cache_pos=info->cache_end=info->cache;
  DBUG_RETURN(0);
} /* init_rr_cache */


static int rr_from_cache(READ_RECORD *info)
{
  reg1 uint i;
  ulong length;
  my_off_t rest_of_file;
  int16 error;
  byte *position,*ref_position,*record_pos;
  ulong record;

  for (;;)
  {
    if (info->cache_pos != info->cache_end)
    {
      if (info->cache_pos[info->error_offset])
      {
	shortget(error,info->cache_pos);
	if (info->print_error)
	  info->table->file->print_error(error,MYF(0));
      }
      else
      {
	error=0;
	memcpy(info->record,info->cache_pos,(size_t) info->table->reclength);
      }
      info->cache_pos+=info->reclength;
      return ((int) error);
    }
    length=info->rec_cache_size;
    rest_of_file=info->io_cache->end_of_file - my_b_tell(info->io_cache);
    if ((my_off_t) length > rest_of_file)
      length= (ulong) rest_of_file;
    if (!length || my_b_read(info->io_cache,info->cache,length))
    {
      DBUG_PRINT("info",("Found end of file"));
      return -1;			/* End of file */
    }

    length/=info->ref_length;
    position=info->cache;
    ref_position=info->read_positions;
    for (i=0 ; i < length ; i++,position+=info->ref_length)
    {
      memcpy(ref_position,position,(size_s) info->ref_length);
      ref_position+=MAX_REFLENGTH;
      int3store(ref_position,(long) i);
      ref_position+=3;
    }
    qsort(info->read_positions,length,info->struct_length,(qsort_cmp) rr_cmp);

    position=info->read_positions;
    for (i=0 ; i < length ; i++)
    {
      memcpy(info->ref_pos,position,(size_s) info->ref_length);
      position+=MAX_REFLENGTH;
      record=uint3korr(position);
      position+=3;
      record_pos=info->cache+record*info->reclength;
      if ((error=(int16) info->file->rnd_pos(record_pos,info->ref_pos)))
      {
	record_pos[info->error_offset]=1;
	shortstore(record_pos,error);
	DBUG_PRINT("error",("Got error: %d:%d when reading row",
			    my_errno, error));
      }
      else
	record_pos[info->error_offset]=0;
    }
    info->cache_end=(info->cache_pos=info->cache)+length*info->reclength;
  }
} /* rr_from_cache */


static int rr_cmp(uchar *a,uchar *b)
{
  if (a[0] != b[0])
    return (int) a[0] - (int) b[0];
  if (a[1] != b[1])
    return (int) a[1] - (int) b[1];
  if (a[2] != b[2])
    return (int) a[2] - (int) b[2];
#if MAX_REFLENGTH == 4
  return (int) a[3] - (int) b[3];
#else
  if (a[3] != b[3])
    return (int) a[3] - (int) b[3];
  if (a[4] != b[4])
    return (int) a[4] - (int) b[4];
  if (a[5] != b[5])
    return (int) a[1] - (int) b[5];
  if (a[6] != b[6])
    return (int) a[6] - (int) b[6];
  return (int) a[7] - (int) b[7];
#endif
}
