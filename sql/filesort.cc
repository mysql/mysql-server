/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


/**
  @file

  @brief
  Sorts a database
*/

#include "sql_priv.h"
#include "filesort.h"
#include "unireg.h"                      // REQUIRED by other includes
#ifdef HAVE_STDDEF_H
#include <stddef.h>			/* for macro offsetof */
#endif
#include <m_ctype.h>
#include "sql_sort.h"
#include "probes_mysql.h"
#include "sql_test.h"                           // TEST_filesort
#include "opt_range.h"                          // SQL_SELECT
#include "debug_sync.h"

/// How to write record_ref.
#define WRITE_REF(file,from) \
if (my_b_write((file),(uchar*) (from),param->ref_length)) \
  DBUG_RETURN(1);

	/* functions defined in this file */

static size_t char_array_size(uint fields, uint length);
static uchar **make_char_array(uchar **old_pos, uint fields,
                               uint length, myf my_flag);
static uchar *read_buffpek_from_file(IO_CACHE *buffer_file, uint count,
                                     uchar *buf);
static ha_rows find_all_keys(SORTPARAM *param,SQL_SELECT *select,
			     uchar * *sort_keys, IO_CACHE *buffer_file,
			     IO_CACHE *tempfile);
static int write_keys(SORTPARAM *param,uchar * *sort_keys,
		      uint count, IO_CACHE *buffer_file, IO_CACHE *tempfile);
static void make_sortkey(SORTPARAM *param,uchar *to, uchar *ref_pos);
static void register_used_fields(SORTPARAM *param);
static int merge_index(SORTPARAM *param,uchar *sort_buffer,
		       BUFFPEK *buffpek,
		       uint maxbuffer,IO_CACHE *tempfile,
		       IO_CACHE *outfile);
static bool save_index(SORTPARAM *param,uchar **sort_keys, uint count, 
                       FILESORT_INFO *table_sort);
static uint suffix_length(ulong string_length);
static uint sortlength(THD *thd, SORT_FIELD *sortorder, uint s_length,
		       bool *multi_byte_charset);
static SORT_ADDON_FIELD *get_addon_fields(THD *thd, Field **ptabfield,
                                          uint sortlength, uint *plength);
static void unpack_addon_fields(struct st_sort_addon_field *addon_field,
                                uchar *buff);
/**
  Sort a table.
  Creates a set of pointers that can be used to read the rows
  in sorted order. This should be done with the functions
  in records.cc.

  Before calling filesort, one must have done
  table->file->info(HA_STATUS_VARIABLE)

  The result set is stored in table->io_cache or
  table->record_pointers.

  @param thd           Current thread
  @param table		Table to sort
  @param sortorder	How to sort the table
  @param s_length	Number of elements in sortorder
  @param select		condition to apply to the rows
  @param max_rows	Return only this many rows
  @param sort_positions	Set to 1 if we want to force sorting by position
			(Needed by UPDATE/INSERT or ALTER TABLE)
  @param examined_rows	Store number of examined rows here

  @note
    If we sort by position (like if sort_positions is 1) filesort() will
    call table->prepare_for_position().

  @retval
    HA_POS_ERROR	Error
  @retval
    \#			Number of rows
  @retval
    examined_rows	will be set to number of examined rows
*/

ha_rows filesort(THD *thd, TABLE *table, SORT_FIELD *sortorder, uint s_length,
		 SQL_SELECT *select, ha_rows max_rows,
                 bool sort_positions, ha_rows *examined_rows)
{
  int error;
  ulong memory_available= thd->variables.sortbuff_size;
  uint maxbuffer;
  BUFFPEK *buffpek;
  ha_rows num_rows= HA_POS_ERROR;
  uchar **sort_keys= 0;
  IO_CACHE tempfile, buffpek_pointers, *outfile; 
  SORTPARAM param;
  bool multi_byte_charset;
  DBUG_ENTER("filesort");
  DBUG_EXECUTE("info",TEST_filesort(sortorder,s_length););
#ifdef SKIP_DBUG_IN_FILESORT
  DBUG_PUSH("");		/* No DBUG here */
#endif
  FILESORT_INFO table_sort;
  TABLE_LIST *tab= table->pos_in_table_list;
  Item_subselect *subselect= tab ? tab->containing_subselect() : 0;

  MYSQL_FILESORT_START(table->s->db.str, table->s->table_name.str);
  DEBUG_SYNC(thd, "filesort_start");

  /*
   Release InnoDB's adaptive hash index latch (if holding) before
   running a sort.
  */
  ha_release_temporary_latches(thd);

  /* 
    Don't use table->sort in filesort as it is also used by 
    QUICK_INDEX_MERGE_SELECT. Work with a copy and put it back at the end 
    when index_merge select has finished with it.
  */
  memcpy(&table_sort, &table->sort, sizeof(FILESORT_INFO));
  table->sort.io_cache= NULL;
  DBUG_ASSERT(table_sort.record_pointers == NULL);
  
  outfile= table_sort.io_cache;
  my_b_clear(&tempfile);
  my_b_clear(&buffpek_pointers);
  buffpek=0;
  error= 1;
  bzero((char*) &param,sizeof(param));
  param.sort_length= sortlength(thd, sortorder, s_length, &multi_byte_charset);
  param.ref_length= table->file->ref_length;
  param.addon_field= 0;
  param.addon_length= 0;
  if (!(table->file->ha_table_flags() & HA_FAST_KEY_READ) &&
      !table->fulltext_searched && !sort_positions)
  {
    /* 
      Get the descriptors of all fields whose values are appended 
      to sorted fields and get its total length in param.spack_length.
    */
    param.addon_field= get_addon_fields(thd, table->field, 
                                        param.sort_length,
                                        &param.addon_length);
  }

  table_sort.addon_buf= 0;
  table_sort.addon_length= param.addon_length;
  table_sort.addon_field= param.addon_field;
  table_sort.unpack= unpack_addon_fields;
  if (param.addon_field)
  {
    param.res_length= param.addon_length;
    if (!(table_sort.addon_buf= (uchar *) my_malloc(param.addon_length,
                                                    MYF(MY_WME))))
      goto err;
  }
  else
  {
    param.res_length= param.ref_length;
    /* 
      The reference to the record is considered 
      as an additional sorted field
    */
    param.sort_length+= param.ref_length;
  }
  param.rec_length= param.sort_length+param.addon_length;
  param.max_rows= max_rows;

  if (select && select->quick)
    status_var_increment(thd->status_var.filesort_range_count);
  else
    status_var_increment(thd->status_var.filesort_scan_count);

  // If number of rows is not known, use as much of sort buffer as possible. 
  num_rows= table->file->estimate_rows_upper_bound();

  if (multi_byte_charset &&
      !(param.tmp_buffer= (char*) my_malloc(param.sort_length,MYF(MY_WME))))
    goto err;

  {
    const ulong min_sort_memory=
      max(MIN_SORT_MEMORY,
          ALIGN_SIZE(MERGEBUFF2 * (param.rec_length + sizeof(uchar*))));
    while (memory_available >= min_sort_memory)
    {
      ulong keys= memory_available / (param.rec_length + sizeof(char*));
      param.keys=(uint) min(num_rows, keys);
      if (table_sort.sort_keys &&
          table_sort.sort_keys_size != char_array_size(param.keys,
                                                       param.rec_length))
      {
        my_free(table_sort.sort_keys);
        table_sort.sort_keys= NULL;
        table_sort.sort_keys_size= 0;
      }
      if ((table_sort.sort_keys=
           make_char_array(table_sort.sort_keys,
                           param.keys, param.rec_length, MYF(0))))
      {
        table_sort.sort_keys_size=
          char_array_size(param.keys, param.rec_length);
        break;
      }
      ulong old_memory_available= memory_available;
      memory_available= memory_available/4*3;
      if (memory_available < min_sort_memory &&
          old_memory_available > min_sort_memory)
        memory_available= min_sort_memory;
    }
    sort_keys= table_sort.sort_keys;
    if (memory_available < min_sort_memory)
    {
      my_error(ER_OUT_OF_SORTMEMORY,MYF(ME_ERROR + ME_FATALERROR));
      goto err;
    }
  }
  if (open_cached_file(&buffpek_pointers,mysql_tmpdir,TEMP_PREFIX,
		       DISK_BUFFER_SIZE, MYF(MY_WME)))
    goto err;

  param.sort_form= table;
  param.end=(param.local_sortorder=sortorder)+s_length;
  num_rows= find_all_keys(&param,
                          select,
                          sort_keys,
                          &buffpek_pointers,
                          &tempfile);
  if (num_rows == HA_POS_ERROR)
    goto err;
  maxbuffer= (uint) (my_b_tell(&buffpek_pointers)/sizeof(*buffpek));

  if (maxbuffer == 0)			// The whole set is in memory
  {
    if (save_index(&param,sort_keys,(uint) num_rows, &table_sort))
      goto err;
  }
  else
  {
    /* filesort cannot handle zero-length records during merge. */
    DBUG_ASSERT(param.sort_length != 0);

    if (table_sort.buffpek && table_sort.buffpek_len < maxbuffer)
    {
      my_free(table_sort.buffpek);
      table_sort.buffpek= 0;
    }
    if (!(table_sort.buffpek=
          (uchar *) read_buffpek_from_file(&buffpek_pointers, maxbuffer,
                                 table_sort.buffpek)))
      goto err;
    buffpek= (BUFFPEK *) table_sort.buffpek;
    table_sort.buffpek_len= maxbuffer;
    close_cached_file(&buffpek_pointers);
	/* Open cached file if it isn't open */
    if (! my_b_inited(outfile) &&
	open_cached_file(outfile,mysql_tmpdir,TEMP_PREFIX,READ_RECORD_BUFFER,
			  MYF(MY_WME)))
      goto err;
    if (reinit_io_cache(outfile,WRITE_CACHE,0L,0,0))
      goto err;

    /*
      Use also the space previously used by string pointers in sort_buffer
      for temporary key storage.
    */
    param.keys= table_sort.sort_keys_size / param.rec_length;
    maxbuffer--;				// Offset from 0
    if (merge_many_buff(&param,(uchar*) sort_keys,buffpek,&maxbuffer,
			&tempfile))
      goto err;
    if (flush_io_cache(&tempfile) ||
	reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
      goto err;
    if (merge_index(&param,
                    (uchar*) sort_keys,
                    buffpek,
                    maxbuffer,
                    &tempfile,
		    outfile))
      goto err;
  }

  if (num_rows > param.max_rows)
  {
    // If find_all_keys() produced more results than the query LIMIT.
    num_rows= param.max_rows;
  }
  error= 0;

 err:
  my_free(param.tmp_buffer);
  if (!subselect || !subselect->is_uncacheable())
  {
    my_free(sort_keys);
    table_sort.sort_keys= 0;
    table_sort.sort_keys_size= 0;
    my_free(buffpek);
    table_sort.buffpek= 0;
    table_sort.buffpek_len= 0;
  }
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
  {
    int kill_errno= thd->killed_errno();
    DBUG_ASSERT(thd->is_error() || kill_errno);
    my_printf_error(ER_FILSORT_ABORT,
                    "%s: %s",
                    MYF(ME_ERROR + ME_WAITTANG),
                    ER_THD(thd, ER_FILSORT_ABORT),
                    kill_errno ? ((kill_errno == THD::KILL_CONNECTION &&
                                 !shutdown_in_progress) ? ER(THD::KILL_QUERY) :
                                                          ER(kill_errno)) :
                                 thd->stmt_da->message());
                    
    if (global_system_variables.log_warnings > 1)
    {
      sql_print_warning("%s, host: %s, user: %s, thread: %lu, query: %-.4096s",
                        ER_THD(thd, ER_FILSORT_ABORT),
                        thd->security_ctx->host_or_ip,
                        &thd->security_ctx->priv_user[0],
                        (ulong) thd->thread_id,
                        thd->query());
    }
  }
  else
    statistic_add(thd->status_var.filesort_rows,
		  (ulong) num_rows, &LOCK_status);
  *examined_rows= param.examined_rows;
#ifdef SKIP_DBUG_IN_FILESORT
  DBUG_POP();			/* Ok to DBUG */
#endif
  memcpy(&table->sort, &table_sort, sizeof(FILESORT_INFO));
  DBUG_PRINT("exit",("num_rows: %ld", (long) num_rows));
  MYSQL_FILESORT_DONE(error, num_rows);
  DBUG_RETURN(error ? HA_POS_ERROR : num_rows);
} /* filesort */


void filesort_free_buffers(TABLE *table, bool full)
{
  DBUG_ENTER("filesort_free_buffers");
  my_free(table->sort.record_pointers);
  table->sort.record_pointers= NULL;

  if (full)
  {
    my_free(table->sort.sort_keys);
    table->sort.sort_keys= NULL;
    table->sort.sort_keys_size= 0;
    my_free(table->sort.buffpek);
    table->sort.buffpek= NULL;
    table->sort.buffpek_len= 0;
  }

  my_free(table->sort.addon_buf);
  my_free(table->sort.addon_field);
  table->sort.addon_buf= NULL;
  table->sort.addon_field= NULL;
  DBUG_VOID_RETURN;
}

/** Make a array of string pointers. */

static size_t char_array_size(uint fields, uint length)
{
  return fields * (length + sizeof(uchar*));
}


static uchar **make_char_array(uchar **old_pos, uint fields,
                               uint length, myf my_flag)
{
  register uchar **pos;
  uchar *char_pos;
  DBUG_ENTER("make_char_array");

  DBUG_EXECUTE_IF("make_char_array_fail",
                  DBUG_SET("+d,simulate_out_of_memory"););

  if (old_pos ||
      (old_pos= (uchar**) my_malloc(char_array_size(fields, length), my_flag)))
  {
    pos=old_pos;
    char_pos= ((uchar*)(pos+fields)) -length;
    while (fields--)
    {
      *(pos++) = (char_pos+= length);
    }
  }

  DBUG_RETURN(old_pos);
} /* make_char_array */


/** Read 'count' number of buffer pointers into memory. */

static uchar *read_buffpek_from_file(IO_CACHE *buffpek_pointers, uint count,
                                     uchar *buf)
{
  ulong length= sizeof(BUFFPEK)*count;
  uchar *tmp= buf;
  DBUG_ENTER("read_buffpek_from_file");
  if (count > UINT_MAX/sizeof(BUFFPEK))
    return 0; /* sizeof(BUFFPEK)*count will overflow */
  if (!tmp)
    tmp= (uchar *)my_malloc(length, MYF(MY_WME));
  if (tmp)
  {
    if (reinit_io_cache(buffpek_pointers,READ_CACHE,0L,0,0) ||
	my_b_read(buffpek_pointers, (uchar*) tmp, length))
    {
      my_free(tmp);
      tmp=0;
    }
  }
  DBUG_RETURN(tmp);
}

#ifndef DBUG_OFF
/*
  Print a text, SQL-like record representation into dbug trace.

  Note: this function is a work in progress: at the moment
   - column read bitmap is ignored (can print garbage for unused columns)
   - there is no quoting
*/
static void dbug_print_record(TABLE *table, bool print_rowid)
{
  char buff[1024];
  Field **pfield;
  String tmp(buff,sizeof(buff),&my_charset_bin);
  DBUG_LOCK_FILE;
  
  fprintf(DBUG_FILE, "record (");
  for (pfield= table->field; *pfield ; pfield++)
    fprintf(DBUG_FILE, "%s%s", (*pfield)->field_name, (pfield[1])? ", ":"");
  fprintf(DBUG_FILE, ") = ");

  fprintf(DBUG_FILE, "(");
  for (pfield= table->field; *pfield ; pfield++)
  {
    Field *field=  *pfield;

    if (field->is_null())
      fwrite("NULL", sizeof(char), 4, DBUG_FILE);
   
    if (field->type() == MYSQL_TYPE_BIT)
      (void) field->val_int_as_str(&tmp, 1);
    else
      field->val_str(&tmp);

    fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE);
    if (pfield[1])
      fwrite(", ", sizeof(char), 2, DBUG_FILE);
  }
  fprintf(DBUG_FILE, ")");
  if (print_rowid)
  {
    fprintf(DBUG_FILE, " rowid ");
    for (uint i=0; i < table->file->ref_length; i++)
    {
      fprintf(DBUG_FILE, "%x", (uchar)table->file->ref[i]);
    }
  }
  fprintf(DBUG_FILE, "\n");
  DBUG_UNLOCK_FILE;
}
#endif 

/**
  Search after sort_keys and write them into tempfile.
  All produced sequences are guaranteed to be non-empty.

  @param param             Sorting parameter
  @param select            Use this to get source data
  @param sort_keys         Array of pointers to sort key + addon buffers.
  @param buffpek_pointers  File to write BUFFPEKs describing sorted segments
                           in tempfile.
  @param tempfile          File to write sorted sequences of sortkeys to.

  @note
    Basic idea:
    @verbatim
     while (get_next_sortkey())
     {
       if (no free space in sort_keys buffers)
       {
         sort sort_keys buffer;
         dump sorted sequence to 'tempfile';
         dump BUFFPEK describing sequence location into 'buffpek_pointers';
       }
       put sort key into 'sort_keys';
     }
     if (sort_keys has some elements && dumped at least once)
       sort-dump-dump as above;
     else
       don't sort, leave sort_keys array to be sorted by caller.
  @endverbatim

  @retval
    Number of records written on success.
  @retval
    HA_POS_ERROR on error.
*/

static ha_rows find_all_keys(SORTPARAM *param, SQL_SELECT *select,
			     uchar **sort_keys,
			     IO_CACHE *buffpek_pointers,
			     IO_CACHE *tempfile)
{
  int error,flag,quick_select;
  uint idx,indexpos,ref_length;
  uchar *ref_pos,*next_pos,ref_buff[MAX_REFLENGTH];
  my_off_t record;
  TABLE *sort_form;
  THD *thd= current_thd;
  volatile THD::killed_state *killed= &thd->killed;
  handler *file;
  MY_BITMAP *save_read_set, *save_write_set;
  bool skip_record;
  DBUG_ENTER("find_all_keys");
  DBUG_PRINT("info",("using: %s",
                     (select ? select->quick ? "ranges" : "where":
                      "every row")));

  idx=indexpos=0;
  error=quick_select=0;
  sort_form=param->sort_form;
  file=sort_form->file;
  ref_length=param->ref_length;
  ref_pos= ref_buff;
  quick_select=select && select->quick;
  record=0;
  flag= ((file->ha_table_flags() & HA_REC_NOT_IN_SEQ) || quick_select);
  if (flag)
    ref_pos= &file->ref[0];
  next_pos=ref_pos;
  if (!quick_select)
  {
    next_pos=(uchar*) 0;			/* Find records in sequence */
    DBUG_EXECUTE_IF("bug14365043_1",
                    DBUG_SET("+d,ha_rnd_init_fail"););
    if ((error= file->ha_rnd_init(1)))
    {
      file->print_error(error, MYF(0));
      DBUG_RETURN(HA_POS_ERROR);
    }
    file->extra_opt(HA_EXTRA_CACHE,
		    current_thd->variables.read_buff_size);
  }

  if (quick_select)
  {
    if (select->quick->reset())
      DBUG_RETURN(HA_POS_ERROR);
  }

  /* Remember original bitmaps */
  save_read_set=  sort_form->read_set;
  save_write_set= sort_form->write_set;
  /* Set up temporary column read map for columns used by sort */
  bitmap_clear_all(&sort_form->tmp_set);
  /* Temporary set for register_used_fields and register_field_in_read_map */
  sort_form->read_set= &sort_form->tmp_set;
  register_used_fields(param);
  if (select && select->cond)
    select->cond->walk(&Item::register_field_in_read_map, 1,
                       (uchar*) sort_form);
  sort_form->column_bitmaps_set(&sort_form->tmp_set, &sort_form->tmp_set);

  for (;;)
  {
    if (quick_select)
    {
      if ((error= select->quick->get_next()))
        break;
      file->position(sort_form->record[0]);
      DBUG_EXECUTE_IF("debug_filesort", dbug_print_record(sort_form, TRUE););
    }
    else					/* Not quick-select */
    {
      {
	error= file->rnd_next(sort_form->record[0]);
	if (!flag)
	{
	  my_store_ptr(ref_pos,ref_length,record); // Position to row
	  record+= sort_form->s->db_record_offset;
	}
	else if (!error)
	  file->position(sort_form->record[0]);
      }
      if (error && error != HA_ERR_RECORD_DELETED)
	break;
    }

    if (*killed)
    {
      DBUG_PRINT("info",("Sort killed by user"));
      if (!quick_select)
      {
        (void) file->extra(HA_EXTRA_NO_CACHE);
        file->ha_rnd_end();
      }
      DBUG_RETURN(HA_POS_ERROR);		/* purecov: inspected */
    }
    if (error == 0)
      param->examined_rows++;
    if (!error && (!select ||
                   (!select->skip_record(thd, &skip_record) && !skip_record)))
    {
      if (idx == param->keys)
      {
	if (write_keys(param, sort_keys,
                       idx, buffpek_pointers, tempfile))
	  DBUG_RETURN(HA_POS_ERROR);
	idx= 0;
	indexpos++;
      }
      make_sortkey(param,sort_keys[idx++],ref_pos);
    }
    else
      file->unlock_row();
    /* It does not make sense to read more keys in case of a fatal error */
    if (thd->is_error())
      break;
  }
  if (!quick_select)
  {
    (void) file->extra(HA_EXTRA_NO_CACHE);	/* End cacheing of records */
    if (!next_pos)
      file->ha_rnd_end();
  }

  if (thd->is_error())
    DBUG_RETURN(HA_POS_ERROR);
  
  /* Signal we should use orignal column read and write maps */
  sort_form->column_bitmaps_set(save_read_set, save_write_set);

  DBUG_PRINT("test",("error: %d  indexpos: %d",error,indexpos));
  if (error != HA_ERR_END_OF_FILE)
  {
    file->print_error(error,MYF(ME_ERROR | ME_WAITTANG)); // purecov: inspected
    DBUG_RETURN(HA_POS_ERROR);			/* purecov: inspected */
  }
  if (indexpos && idx &&
      write_keys(param, sort_keys,
                 idx, buffpek_pointers, tempfile))
    DBUG_RETURN(HA_POS_ERROR);			/* purecov: inspected */
  const ha_rows retval=
    my_b_inited(tempfile) ?
    (ha_rows) (my_b_tell(tempfile)/param->rec_length) : idx;
  DBUG_RETURN(retval);
} /* find_all_keys */


/**
  @details
  Sort the buffer and write:
  -# the sorted sequence to tempfile
  -# a BUFFPEK describing the sorted sequence position to buffpek_pointers

    (was: Skriver en buffert med nycklar till filen)

  @param param             Sort parameters
  @param sort_keys         Array of pointers to keys to sort
  @param count             Number of elements in sort_keys array
  @param buffpek_pointers  One 'BUFFPEK' struct will be written into this file.
                           The BUFFPEK::{file_pos, count} will indicate where
                           the sorted data was stored.
  @param tempfile          The sorted sequence will be written into this file.

  @retval
    0 OK
  @retval
    1 Error
*/

static int
write_keys(SORTPARAM *param, register uchar **sort_keys, uint count,
           IO_CACHE *buffpek_pointers, IO_CACHE *tempfile)
{
  size_t sort_length, rec_length;
  uchar **end;
  BUFFPEK buffpek;
  DBUG_ENTER("write_keys");

  sort_length= param->sort_length;
  rec_length= param->rec_length;
#ifdef MC68000
  quicksort(sort_keys,count,sort_length);
#else
  my_string_ptr_sort((uchar*) sort_keys, (uint) count, sort_length);
#endif
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, mysql_tmpdir, TEMP_PREFIX, DISK_BUFFER_SIZE,
                       MYF(MY_WME)))
    goto err;                                   /* purecov: inspected */
  /* check we won't have more buffpeks than we can possibly keep in memory */
  if (my_b_tell(buffpek_pointers) + sizeof(BUFFPEK) > (ulonglong)UINT_MAX)
    goto err;
  buffpek.file_pos= my_b_tell(tempfile);
  if ((ha_rows) count > param->max_rows)
    count=(uint) param->max_rows;               /* purecov: inspected */
  buffpek.count=(ha_rows) count;
  for (end=sort_keys+count ; sort_keys != end ; sort_keys++)
    if (my_b_write(tempfile, (uchar*) *sort_keys, (uint) rec_length))
      goto err;
  if (my_b_write(buffpek_pointers, (uchar*) &buffpek, sizeof(buffpek)))
    goto err;
  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
} /* write_keys */


/**
  Store length as suffix in high-byte-first order.
*/

static inline void store_length(uchar *to, uint length, uint pack_length)
{
  switch (pack_length) {
  case 1:
    *to= (uchar) length;
    break;
  case 2:
    mi_int2store(to, length);
    break;
  case 3:
    mi_int3store(to, length);
    break;
  default:
    mi_int4store(to, length);
    break;
  }
}


/** Make a sort-key from record. */

static void make_sortkey(register SORTPARAM *param,
			 register uchar *to, uchar *ref_pos)
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
      field->sort_string(to, sort_field->length);
    }
    else
    {						// Item
      Item *item=sort_field->item;
      maybe_null= item->maybe_null;
      switch (sort_field->result_type) {
      case STRING_RESULT:
      {
        CHARSET_INFO *cs=item->collation.collation;
        char fill_char= ((cs->state & MY_CS_BINSORT) ? (char) 0 : ' ');
        int diff;
        uint sort_field_length;

        if (maybe_null)
          *to++=1;
        /* All item->str() to use some extra byte for end null.. */
        String tmp((char*) to,sort_field->length+4,cs);
        String *res= item->str_result(&tmp);
        if (!res)
        {
          if (maybe_null)
            bzero((char*) to-1,sort_field->length+1);
          else
          {
            /* purecov: begin deadcode */
            /*
              This should only happen during extreme conditions if we run out
              of memory or have an item marked not null when it can be null.
              This code is here mainly to avoid a hard crash in this case.
            */
            DBUG_ASSERT(0);
            DBUG_PRINT("warning",
                       ("Got null on something that shouldn't be null"));
            bzero((char*) to,sort_field->length);	// Avoid crash
            /* purecov: end */
          }
          break;
        }
        length= res->length();
        sort_field_length= sort_field->length - sort_field->suffix_length;
        diff=(int) (sort_field_length - length);
        if (diff < 0)
        {
          diff=0;
          length= sort_field_length;
        }
        if (sort_field->suffix_length)
        {
          /* Store length last in result_string */
          store_length(to + sort_field_length, length,
                       sort_field->suffix_length);
        }
        if (sort_field->need_strxnfrm)
        {
          char *from=(char*) res->ptr();
          uint tmp_length;
          if ((uchar*) from == to)
          {
            set_if_smaller(length,sort_field->length);
            memcpy(param->tmp_buffer,from,length);
            from=param->tmp_buffer;
          }
          tmp_length= my_strnxfrm(cs,to,sort_field->length,
                                  (uchar*) from, length);
          DBUG_ASSERT(tmp_length == sort_field->length);
        }
        else
        {
          my_strnxfrm(cs,(uchar*)to,length,(const uchar*)res->ptr(),length);
          cs->cset->fill(cs, (char *)to+length,diff,fill_char);
        }
        break;
      }
      case INT_RESULT:
	{
          longlong value= item->val_int_result();
          if (maybe_null)
          {
	    *to++=1;				/* purecov: inspected */
            if (item->null_value)
            {
              if (maybe_null)
                bzero((char*) to-1,sort_field->length+1);
              else
              {
                DBUG_PRINT("warning",
                           ("Got null on something that shouldn't be null"));
                bzero((char*) to,sort_field->length);
              }
              break;
            }
          }
#if SIZEOF_LONG_LONG > 4
	  to[7]= (uchar) value;
	  to[6]= (uchar) (value >> 8);
	  to[5]= (uchar) (value >> 16);
	  to[4]= (uchar) (value >> 24);
	  to[3]= (uchar) (value >> 32);
	  to[2]= (uchar) (value >> 40);
	  to[1]= (uchar) (value >> 48);
          if (item->unsigned_flag)                    /* Fix sign */
            to[0]= (uchar) (value >> 56);
          else
            to[0]= (uchar) (value >> 56) ^ 128;	/* Reverse signbit */
#else
	  to[3]= (uchar) value;
	  to[2]= (uchar) (value >> 8);
	  to[1]= (uchar) (value >> 16);
          if (item->unsigned_flag)                    /* Fix sign */
            to[0]= (uchar) (value >> 24);
          else
            to[0]= (uchar) (value >> 24) ^ 128;	/* Reverse signbit */
#endif
	  break;
	}
      case DECIMAL_RESULT:
        {
          my_decimal dec_buf, *dec_val= item->val_decimal_result(&dec_buf);
          if (maybe_null)
          {
            if (item->null_value)
            { 
              bzero((char*)to, sort_field->length+1);
              to++;
              break;
            }
            *to++=1;
          }
          my_decimal2binary(E_DEC_FATAL_ERROR, dec_val, to,
                            item->max_length - (item->decimals ? 1:0),
                            item->decimals);
         break;
        }
      case REAL_RESULT:
	{
          double value= item->val_result();
	  if (maybe_null)
          {
            if (item->null_value)
            {
              bzero((char*) to,sort_field->length+1);
              to++;
              break;
            }
	    *to++=1;
          }
	  change_double_for_sort(value,(uchar*) to);
	  break;
	}
      case ROW_RESULT:
      default: 
	// This case should never be choosen
	DBUG_ASSERT(0);
	break;
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

  if (param->addon_field)
  {
    /* 
      Save field values appended to sorted fields.
      First null bit indicators are appended then field values follow.
      In this implementation we use fixed layout for field values -
      the same for all records.
    */
    SORT_ADDON_FIELD *addonf= param->addon_field;
    uchar *nulls= to;
    DBUG_ASSERT(addonf != 0);
    bzero((char *) nulls, addonf->offset);
    to+= addonf->offset;
    for ( ; (field= addonf->field) ; addonf++)
    {
      if (addonf->null_bit && field->is_null())
      {
        nulls[addonf->null_offset]|= addonf->null_bit;
      }
      else
      {
        (void) field->pack(to, field->ptr);
      }
      to+= addonf->length;
    }
  }
  else
  {
    /* Save filepos last */
    memcpy((uchar*) to, ref_pos, (size_t) param->ref_length);
  }
  return;
}


/*
  Register fields used by sorting in the sorted table's read set
*/

static void register_used_fields(SORTPARAM *param)
{
  reg1 SORT_FIELD *sort_field;
  TABLE *table=param->sort_form;
  MY_BITMAP *bitmap= table->read_set;

  for (sort_field= param->local_sortorder ;
       sort_field != param->end ;
       sort_field++)
  {
    Field *field;
    if ((field= sort_field->field))
    {
      if (field->table == table)
      bitmap_set_bit(bitmap, field->field_index);
    }
    else
    {						// Item
      sort_field->item->walk(&Item::register_field_in_read_map, 1,
                             (uchar *) table);
    }
  }

  if (param->addon_field)
  {
    SORT_ADDON_FIELD *addonf= param->addon_field;
    Field *field;
    for ( ; (field= addonf->field) ; addonf++)
      bitmap_set_bit(bitmap, field->field_index);
  }
  else
  {
    /* Save filepos last */
    table->prepare_for_position();
  }
}


static bool save_index(SORTPARAM *param, uchar **sort_keys, uint count, 
                       FILESORT_INFO *table_sort)
{
  uint offset,res_length;
  uchar *to;
  DBUG_ENTER("save_index");

  my_string_ptr_sort((uchar*) sort_keys, (uint) count, param->sort_length);
  res_length= param->res_length;
  offset= param->rec_length-res_length;
  if ((ha_rows) count > param->max_rows)
    count=(uint) param->max_rows;
  if (!(to= table_sort->record_pointers= 
        (uchar*) my_malloc(res_length*count, MYF(MY_WME))))
    DBUG_RETURN(1);                 /* purecov: inspected */
  for (uchar **end= sort_keys+count ; sort_keys != end ; sort_keys++)
  {
    memcpy(to, *sort_keys+offset, res_length);
    to+= res_length;
  }
  DBUG_RETURN(0);
}


/** Merge buffers to make < MERGEBUFF2 buffers. */

int merge_many_buff(SORTPARAM *param, uchar *sort_buffer,
		    BUFFPEK *buffpek, uint *maxbuffer, IO_CACHE *t_file)
{
  register uint i;
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
    if (reinit_io_cache(from_file,READ_CACHE,0L,0,0))
      goto cleanup;
    if (reinit_io_cache(to_file,WRITE_CACHE,0L,0,0))
      goto cleanup;
    lastbuff=buffpek;
    for (i=0 ; i <= *maxbuffer-MERGEBUFF*3/2 ; i+=MERGEBUFF)
    {
      if (merge_buffers(param,from_file,to_file,sort_buffer,lastbuff++,
			buffpek+i,buffpek+i+MERGEBUFF-1,0))
      goto cleanup;
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
cleanup:
  close_cached_file(to_file);			// This holds old result
  if (to_file == t_file)
  {
    *t_file=t_file2;				// Copy result file
    setup_io_cache(t_file);
  }

  DBUG_RETURN(*maxbuffer >= MERGEBUFF2);	/* Return 1 if interrupted */
} /* merge_many_buff */


/**
  Read data to buffer.

  @retval
    (uint)-1 if something goes wrong
*/

uint read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
		    uint rec_length)
{
  register uint count;
  uint length;

  if ((count=(uint) min((ha_rows) buffpek->max_keys,buffpek->count)))
  {
    if (mysql_file_pread(fromfile->file, (uchar*) buffpek->base,
                         (length= rec_length*count),
                         buffpek->file_pos, MYF_RW))
      return((uint) -1);			/* purecov: inspected */
    buffpek->key=buffpek->base;
    buffpek->file_pos+= length;			/* New filepos */
    buffpek->count-=	count;
    buffpek->mem_count= count;
  }
  return (count*rec_length);
} /* read_to_buffer */


/**
  Put all room used by freed buffer to use in adjacent buffer.

  Note, that we can't simply distribute memory evenly between all buffers,
  because new areas must not overlap with old ones.

  @param[in] queue      list of non-empty buffers, without freed buffer
  @param[in] reuse      empty buffer
  @param[in] key_length key length
*/

void reuse_freed_buff(QUEUE *queue, BUFFPEK *reuse, uint key_length)
{
  uchar *reuse_end= reuse->base + reuse->max_keys * key_length;
  for (uint i= 0; i < queue->elements; ++i)
  {
    BUFFPEK *bp= (BUFFPEK *) queue_element(queue, i);
    if (bp->base + bp->max_keys * key_length == reuse->base)
    {
      bp->max_keys+= reuse->max_keys;
      return;
    }
    else if (bp->base == reuse_end)
    {
      bp->base= reuse->base;
      bp->max_keys+= reuse->max_keys;
      return;
    }
  }
  DBUG_ASSERT(0);
}


/**
  Merge buffers to one buffer.

  @param param        Sort parameter
  @param from_file    File with source data (BUFFPEKs point to this file)
  @param to_file      File to write the sorted result data.
  @param sort_buffer  Buffer for data to store up to MERGEBUFF2 sort keys.
  @param lastbuff     OUT Store here BUFFPEK describing data written to to_file
  @param Fb           First element in source BUFFPEKs array
  @param Tb           Last element in source BUFFPEKs array
  @param flag

  @retval
    0      OK
  @retval
    other  error
*/

int merge_buffers(SORTPARAM *param, IO_CACHE *from_file,
                  IO_CACHE *to_file, uchar *sort_buffer,
                  BUFFPEK *lastbuff, BUFFPEK *Fb, BUFFPEK *Tb,
                  int flag)
{
  int error;
  uint rec_length,res_length,offset;
  size_t sort_length;
  ulong maxcount;
  ha_rows max_rows,org_max_rows;
  my_off_t to_start_filepos;
  uchar *strpos;
  BUFFPEK *buffpek;
  QUEUE queue;
  qsort2_cmp cmp;
  void *first_cmp_arg;
  volatile THD::killed_state *killed= &current_thd->killed;
  THD::killed_state not_killable;
  DBUG_ENTER("merge_buffers");

  status_var_increment(current_thd->status_var.filesort_merge_passes);
  if (param->not_killable)
  {
    killed= &not_killable;
    not_killable= THD::NOT_KILLED;
  }

  error=0;
  rec_length= param->rec_length;
  res_length= param->res_length;
  sort_length= param->sort_length;
  offset= rec_length-res_length;
  maxcount= (ulong) (param->keys/((uint) (Tb-Fb) +1));
  to_start_filepos= my_b_tell(to_file);
  strpos= (uchar*) sort_buffer;
  org_max_rows=max_rows= param->max_rows;

  /* The following will fire if there is not enough space in sort_buffer */
  DBUG_ASSERT(maxcount!=0);
  
  if (param->unique_buff)
  {
    cmp= param->compare;
    first_cmp_arg= (void *) &param->cmp_context;
  }
  else
  {
    cmp= get_ptr_compare(sort_length);
    first_cmp_arg= (void*) &sort_length;
  }
  if (init_queue(&queue, (uint) (Tb-Fb)+1, offsetof(BUFFPEK,key), 0,
                 (queue_compare) cmp, first_cmp_arg))
    DBUG_RETURN(1);                                /* purecov: inspected */
  for (buffpek= Fb ; buffpek <= Tb ; buffpek++)
  {
    buffpek->base= strpos;
    buffpek->max_keys= maxcount;
    strpos+=
      (uint) (error= (int) read_to_buffer(from_file, buffpek, rec_length));
    if (error == -1)
      goto err;					/* purecov: inspected */
    buffpek->max_keys= buffpek->mem_count;	// If less data in buffers than expected
    queue_insert(&queue, (uchar*) buffpek);
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
    buffpek= (BUFFPEK*) queue_top(&queue);
    memcpy(param->unique_buff, buffpek->key, rec_length);
    if (my_b_write(to_file, (uchar*) buffpek->key, rec_length))
    {
      error=1; goto err;                        /* purecov: inspected */
    }
    buffpek->key+= rec_length;
    buffpek->mem_count--;
    if (!--max_rows)
    {
      error= 0;                                       /* purecov: inspected */
      goto end;                                       /* purecov: inspected */
    }
    queue_replaced(&queue);                        // Top element has been used
  }
  else
    cmp= 0;                                        // Not unique

  while (queue.elements > 1)
  {
    if (*killed)
    {
      error= 1; goto err;                        /* purecov: inspected */
    }
    for (;;)
    {
      buffpek= (BUFFPEK*) queue_top(&queue);
      if (cmp)                                        // Remove duplicates
      {
        if (!(*cmp)(first_cmp_arg, &(param->unique_buff),
                    (uchar**) &buffpek->key))
              goto skip_duplicate;
            memcpy(param->unique_buff, (uchar*) buffpek->key, rec_length);
      }
      if (flag == 0)
      {
        if (my_b_write(to_file,(uchar*) buffpek->key, rec_length))
        {
          error=1; goto err;                        /* purecov: inspected */
        }
      }
      else
      {
        if (my_b_write(to_file, (uchar*) buffpek->key+offset, res_length))
        {
          error=1; goto err;                        /* purecov: inspected */
        }
      }
      if (!--max_rows)
      {
        error= 0;                               /* purecov: inspected */
        goto end;                               /* purecov: inspected */
      }

    skip_duplicate:
      buffpek->key+= rec_length;
      if (! --buffpek->mem_count)
      {
        if (!(error= (int) read_to_buffer(from_file,buffpek,
                                          rec_length)))
        {
          (void) queue_remove(&queue,0);
          reuse_freed_buff(&queue, buffpek, rec_length);
          break;                        /* One buffer have been removed */
        }
        else if (error == -1)
          goto err;                        /* purecov: inspected */
      }
      queue_replaced(&queue);              /* Top element has been replaced */
    }
  }
  buffpek= (BUFFPEK*) queue_top(&queue);
  buffpek->base= sort_buffer;
  buffpek->max_keys= param->keys;

  /*
    As we know all entries in the buffer are unique, we only have to
    check if the first one is the same as the last one we wrote
  */
  if (cmp)
  {
    if (!(*cmp)(first_cmp_arg, &(param->unique_buff), (uchar**) &buffpek->key))
    {
      buffpek->key+= rec_length;         // Remove duplicate
      --buffpek->mem_count;
    }
  }

  do
  {
    if ((ha_rows) buffpek->mem_count > max_rows)
    {                                        /* Don't write too many records */
      buffpek->mem_count= (uint) max_rows;
      buffpek->count= 0;                        /* Don't read more */
    }
    max_rows-= buffpek->mem_count;
    if (flag == 0)
    {
      if (my_b_write(to_file,(uchar*) buffpek->key,
                     (rec_length*buffpek->mem_count)))
      {
        error= 1; goto err;                        /* purecov: inspected */
      }
    }
    else
    {
      register uchar *end;
      strpos= buffpek->key+offset;
      for (end= strpos+buffpek->mem_count*rec_length ;
           strpos != end ;
           strpos+= rec_length)
      {     
        if (my_b_write(to_file, (uchar *) strpos, res_length))
        {
          error=1; goto err;                        
        }
      }
    }
  }
  while ((error=(int) read_to_buffer(from_file,buffpek, rec_length))
         != -1 && error != 0);

end:
  lastbuff->count= min(org_max_rows-max_rows, param->max_rows);
  lastbuff->file_pos= to_start_filepos;
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


static uint suffix_length(ulong string_length)
{
  if (string_length < 256)
    return 1;
  if (string_length < 256L*256L)
    return 2;
  if (string_length < 256L*256L*256L)
    return 3;
  return 4;                                     // Can't sort longer than 4G
}



/**
  Calculate length of sort key.

  @param thd			  Thread handler
  @param sortorder		  Order of items to sort
  @param s_length	          Number of items to sort
  @param[out] multi_byte_charset Set to 1 if we are using multi-byte charset
                                 (In which case we have to use strxnfrm())

  @note
    sortorder->length is updated for each sort item.
  @n
    sortorder->need_strxnfrm is set 1 if we have to use strxnfrm

  @return
    Total length of sort buffer in bytes
*/

static uint
sortlength(THD *thd, SORT_FIELD *sortorder, uint s_length,
           bool *multi_byte_charset)
{
  reg2 uint length;
  CHARSET_INFO *cs;
  *multi_byte_charset= 0;

  length=0;
  for (; s_length-- ; sortorder++)
  {
    sortorder->need_strxnfrm= 0;
    sortorder->suffix_length= 0;
    if (sortorder->field)
    {
      cs= sortorder->field->sort_charset();
      sortorder->length= sortorder->field->sort_length();

      if (use_strnxfrm((cs=sortorder->field->sort_charset())))
      {
        sortorder->need_strxnfrm= 1;
        *multi_byte_charset= 1;
        sortorder->length= cs->coll->strnxfrmlen(cs, sortorder->length);
      }
      if (sortorder->field->maybe_null())
	length++;				// Place for NULL marker
    }
    else
    {
      sortorder->result_type= sortorder->item->result_type();
      if (sortorder->item->result_as_longlong())
        sortorder->result_type= INT_RESULT;
      switch (sortorder->result_type) {
      case STRING_RESULT:
	sortorder->length=sortorder->item->max_length;
        set_if_smaller(sortorder->length, thd->variables.max_sort_length);
	if (use_strnxfrm((cs=sortorder->item->collation.collation)))
	{ 
          sortorder->length= cs->coll->strnxfrmlen(cs, sortorder->length);
	  sortorder->need_strxnfrm= 1;
	  *multi_byte_charset= 1;
	}
        else if (cs == &my_charset_bin)
        {
          /* Store length last to be able to sort blob/varbinary */
          sortorder->suffix_length= suffix_length(sortorder->length);
          sortorder->length+= sortorder->suffix_length;
        }
	break;
      case INT_RESULT:
#if SIZEOF_LONG_LONG > 4
	sortorder->length=8;			// Size of intern longlong
#else
	sortorder->length=4;
#endif
	break;
      case DECIMAL_RESULT:
        sortorder->length=
          my_decimal_get_binary_size(sortorder->item->max_length - 
                                     (sortorder->item->decimals ? 1 : 0),
                                     sortorder->item->decimals);
        break;
      case REAL_RESULT:
	sortorder->length=sizeof(double);
	break;
      case ROW_RESULT:
      default: 
	// This case should never be choosen
	DBUG_ASSERT(0);
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


/**
  Get descriptors of fields appended to sorted fields and
  calculate its total length.

  The function first finds out what fields are used in the result set.
  Then it calculates the length of the buffer to store the values of
  these fields together with the value of sort values. 
  If the calculated length is not greater than max_length_for_sort_data
  the function allocates memory for an array of descriptors containing
  layouts for the values of the non-sorted fields in the buffer and
  fills them.

  @param thd                 Current thread
  @param ptabfield           Array of references to the table fields
  @param sortlength          Total length of sorted fields
  @param[out] plength        Total length of appended fields

  @note
    The null bits for the appended values are supposed to be put together
    and stored the buffer just ahead of the value of the first field.

  @return
    Pointer to the layout descriptors for the appended fields, if any
  @retval
    NULL   if we do not store field values with sort data.
*/

static SORT_ADDON_FIELD *
get_addon_fields(THD *thd, Field **ptabfield, uint sortlength, uint *plength)
{
  Field **pfield;
  Field *field;
  SORT_ADDON_FIELD *addonf;
  uint length= 0;
  uint fields= 0;
  uint null_fields= 0;
  MY_BITMAP *read_set= (*ptabfield)->table->read_set;

  /*
    If there is a reference to a field in the query add it
    to the the set of appended fields.
    Note for future refinement:
    This this a too strong condition.
    Actually we need only the fields referred in the
    result set. And for some of them it makes sense to use 
    the values directly from sorted fields.
  */
  *plength= 0;

  for (pfield= ptabfield; (field= *pfield) ; pfield++)
  {
    if (!bitmap_is_set(read_set, field->field_index))
      continue;
    if (field->flags & BLOB_FLAG)
      return 0;
    length+= field->max_packed_col_length(field->pack_length());
    if (field->maybe_null())
      null_fields++;
    fields++;
  } 
  if (!fields)
    return 0;
  length+= (null_fields+7)/8;

  if (length+sortlength > thd->variables.max_length_for_sort_data ||
      !(addonf= (SORT_ADDON_FIELD *) my_malloc(sizeof(SORT_ADDON_FIELD)*
                                               (fields+1), MYF(MY_WME))))
    return 0;

  *plength= length;
  length= (null_fields+7)/8;
  null_fields= 0;
  for (pfield= ptabfield; (field= *pfield) ; pfield++)
  {
    if (!bitmap_is_set(read_set, field->field_index))
      continue;
    addonf->field= field;
    addonf->offset= length;
    if (field->maybe_null())
    {
      addonf->null_offset= null_fields/8;
      addonf->null_bit= 1<<(null_fields & 7);
      null_fields++;
    }
    else
    {
      addonf->null_offset= 0;
      addonf->null_bit= 0;
    }
    addonf->length= field->max_packed_col_length(field->pack_length());
    length+= addonf->length;
    addonf++;
  }
  addonf->field= 0;     // Put end marker
  
  DBUG_PRINT("info",("addon_length: %d",length));
  return (addonf-fields);
}


/**
  Copy (unpack) values appended to sorted fields from a buffer back to
  their regular positions specified by the Field::ptr pointers.

  @param addon_field     Array of descriptors for appended fields
  @param buff            Buffer which to unpack the value from

  @note
    The function is supposed to be used only as a callback function
    when getting field values for the sorted result set.

  @return
    void.
*/

static void 
unpack_addon_fields(struct st_sort_addon_field *addon_field, uchar *buff)
{
  Field *field;
  SORT_ADDON_FIELD *addonf= addon_field;

  for ( ; (field= addonf->field) ; addonf++)
  {
    if (addonf->null_bit && (addonf->null_bit & buff[addonf->null_offset]))
    {
      field->set_null();
      continue;
    }
    field->set_notnull();
    field->unpack(field->ptr, buff + addonf->offset);
  }
}

/*
** functions to change a double or float to a sortable string
** The following should work for IEEE
*/

#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

void change_double_for_sort(double nr,uchar *to)
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
    memcpy(tmp, &nr, sizeof(nr));
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
