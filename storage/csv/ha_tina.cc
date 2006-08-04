/* Copyright (C) 2003 MySQL AB

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
  Make sure to look at ha_tina.h for more details.

  First off, this is a play thing for me, there are a number of things
  wrong with it:
    *) It was designed for csv and therefore its performance is highly
       questionable.
    *) Indexes have not been implemented. This is because the files can
       be traded in and out of the table directory without having to worry
       about rebuilding anything.
    *) NULLs and "" are treated equally (like a spreadsheet).
    *) There was in the beginning no point to anyone seeing this other
       then me, so there is a good chance that I haven't quite documented
       it well.
    *) Less design, more "make it work"

  Now there are a few cool things with it:
    *) Errors can result in corrupted data files.
    *) Data files can be read by spreadsheets directly.

TODO:
 *) Move to a block system for larger files
 *) Error recovery, its all there, just need to finish it
 *) Document how the chains work.

 -Brian
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation        // gcc: Class implementation
#endif

#include "mysql_priv.h"

#include "ha_tina.h"

#include <mysql/plugin.h>

/*
  uchar + uchar + ulonglong + ulonglong + ulonglong + ulonglong + uchar
*/
#define META_BUFFER_SIZE sizeof(uchar) + sizeof(uchar) + sizeof(ulonglong) \
  + sizeof(ulonglong) + sizeof(ulonglong) + sizeof(ulonglong) + sizeof(uchar)
#define TINA_CHECK_HEADER 254 // The number we use to determine corruption

/* The file extension */
#define CSV_EXT ".CSV"               // The data file
#define CSN_EXT ".CSN"               // Files used during repair and update
#define CSM_EXT ".CSM"               // Meta file


static TINA_SHARE *get_share(const char *table_name, TABLE *table);
static int free_share(TINA_SHARE *share);
static int read_meta_file(File meta_file, ha_rows *rows);
static int write_meta_file(File meta_file, ha_rows rows, bool dirty);

/* Stuff for shares */
pthread_mutex_t tina_mutex;
static HASH tina_open_tables;
static int tina_init= 0;
static handler *tina_create_handler(TABLE_SHARE *table, MEM_ROOT *mem_root);
static int tina_init_func();

off_t Transparent_file::read_next()
{
  off_t bytes_read;

  /*
     No need to seek here, as the file managed by Transparent_file class
     always points to upper_bound byte
  */
  if ((bytes_read= my_read(filedes, buff, buff_size, MYF(0))) == MY_FILE_ERROR)
    return -1;

  /* end of file */
  if (!bytes_read)
    return -1;

  lower_bound= upper_bound;
  upper_bound+= bytes_read;

  return lower_bound;
}


char Transparent_file::get_value(off_t offset)
{
  off_t bytes_read;

  /* check boundaries */
  if ((lower_bound <= offset) && (offset < upper_bound))
    return buff[offset - lower_bound];
  else
  {
    VOID(my_seek(filedes, offset, MY_SEEK_SET, MYF(0)));
    /* read appropriate portion of the file */
    if ((bytes_read= my_read(filedes, buff, buff_size,
                             MYF(0))) == MY_FILE_ERROR)
      return 0;

    lower_bound= offset;
    upper_bound= lower_bound + bytes_read;

    /* end of file */
    if (upper_bound == offset)
      return 0;

    return buff[0];
  }
}
handlerton tina_hton;

/*****************************************************************************
 ** TINA tables
 *****************************************************************************/

/*
  Used for sorting chains with qsort().
*/
int sort_set (tina_set *a, tina_set *b)
{
  /*
    We assume that intervals do not intersect. So, it is enought to compare
    any two points. Here we take start of intervals for comparison.
  */
  return ( a->begin > b->begin ? 1 : ( a->begin < b->begin ? -1 : 0 ) );
}

static byte* tina_get_key(TINA_SHARE *share,uint *length,
                          my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}

static int tina_init_func()
{
  if (!tina_init)
  {
    tina_init++;
    VOID(pthread_mutex_init(&tina_mutex,MY_MUTEX_INIT_FAST));
    (void) hash_init(&tina_open_tables,system_charset_info,32,0,0,
                     (hash_get_key) tina_get_key,0,0);
    tina_hton.state= SHOW_OPTION_YES;
    tina_hton.db_type= DB_TYPE_CSV_DB;
    tina_hton.create= tina_create_handler;
    tina_hton.panic= tina_end;
    tina_hton.flags= HTON_CAN_RECREATE;
  }
  return 0;
}

static int tina_done_func()
{
  if (tina_init)
  {
    if (tina_open_tables.records)
    {
      return 1;
    }
    hash_free(&tina_open_tables);
    pthread_mutex_destroy(&tina_mutex);
    tina_init--;
  }
  return 0;
}
	

/*
  Simple lock controls.
*/
static TINA_SHARE *get_share(const char *table_name, TABLE *table)
{
  TINA_SHARE *share;
  char meta_file_name[FN_REFLEN];
  MY_STAT file_stat;                /* Stat information for the data file */
  char *tmp_name;
  uint length;

  if (!tina_init)
     tina_init_func();

  pthread_mutex_lock(&tina_mutex);
  length=(uint) strlen(table_name);

  /*
    If share is not present in the hash, create a new share and
    initialize its members.
  */
  if (!(share=(TINA_SHARE*) hash_search(&tina_open_tables,
                                        (byte*) table_name,
                                       length)))
  {
    if (!my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                         &share, sizeof(*share),
                         &tmp_name, length+1,
                         NullS))
    {
      pthread_mutex_unlock(&tina_mutex);
      return NULL;
    }

    share->use_count= 0;
    share->is_log_table= FALSE;
    share->table_name_length= length;
    share->table_name= tmp_name;
    share->crashed= FALSE;
    share->rows_recorded= 0;
    share->update_file_opened= FALSE;
    share->tina_write_opened= FALSE;
    strmov(share->table_name, table_name);
    fn_format(share->data_file_name, table_name, "", CSV_EXT,
              MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    fn_format(meta_file_name, table_name, "", CSM_EXT,
              MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    if (my_hash_insert(&tina_open_tables, (byte*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);

    /*
      Open or create the meta file. In the latter case, we'll get
      an error during read_meta_file and mark the table as crashed.
      Usually this will result in auto-repair, and we will get a good
      meta-file in the end.
    */
    if ((share->meta_file= my_open(meta_file_name,
                                   O_RDWR|O_CREAT, MYF(0))) == -1)
      share->crashed= TRUE;

    /*
      If the meta file will not open we assume it is crashed and
      mark it as such.
    */
    if (read_meta_file(share->meta_file, &share->rows_recorded))
      share->crashed= TRUE;

    if (my_stat(share->data_file_name, &file_stat, MYF(MY_WME)) == NULL)
      goto error2;
    share->saved_data_file_length= file_stat.st_size;
  }
  share->use_count++;
  pthread_mutex_unlock(&tina_mutex);

  return share;

error2:
  thr_lock_delete(&share->lock);
  pthread_mutex_destroy(&share->mutex);
error:
  pthread_mutex_unlock(&tina_mutex);
  my_free((gptr) share, MYF(0));

  return NULL;
}


/*
  Read CSV meta-file

  SYNOPSIS
    read_meta_file()
    meta_file   The meta-file filedes
    ha_rows     Pointer to the var we use to store rows count.
                These are read from the meta-file.

  DESCRIPTION

    Read the meta-file info. For now we are only interested in
    rows counf, crashed bit and magic number.

  RETURN
    0 - OK
    non-zero - error occurred
*/

static int read_meta_file(File meta_file, ha_rows *rows)
{
  uchar meta_buffer[META_BUFFER_SIZE];
  uchar *ptr= meta_buffer;

  DBUG_ENTER("ha_tina::read_meta_file");

  VOID(my_seek(meta_file, 0, MY_SEEK_SET, MYF(0)));
  if (my_read(meta_file, (byte*)meta_buffer, META_BUFFER_SIZE, 0)
      != META_BUFFER_SIZE)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /*
    Parse out the meta data, we ignore version at the moment
  */

  ptr+= sizeof(uchar)*2; // Move past header
  *rows= (ha_rows)uint8korr(ptr);
  ptr+= sizeof(ulonglong); // Move past rows
  /*
    Move past check_point, auto_increment and forced_flushes fields.
    They are present in the format, but we do not use them yet.
  */
  ptr+= 3*sizeof(ulonglong);

  /* check crashed bit and magic number */
  if ((meta_buffer[0] != (uchar)TINA_CHECK_HEADER) ||
      ((bool)(*ptr)== TRUE))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  my_sync(meta_file, MYF(MY_WME));

  DBUG_RETURN(0);
}


/*
  Write CSV meta-file

  SYNOPSIS
    write_meta_file()
    meta_file   The meta-file filedes
    ha_rows     The number of rows we have in the datafile.
    dirty       A flag, which marks whether we have a corrupt table

  DESCRIPTION

    Write meta-info the the file. Only rows count, crashed bit and
    magic number matter now.

  RETURN
    0 - OK
    non-zero - error occurred
*/

static int write_meta_file(File meta_file, ha_rows rows, bool dirty)
{
  uchar meta_buffer[META_BUFFER_SIZE];
  uchar *ptr= meta_buffer;

  DBUG_ENTER("ha_tina::write_meta_file");

  *ptr= (uchar)TINA_CHECK_HEADER;
  ptr+= sizeof(uchar);
  *ptr= (uchar)TINA_VERSION;
  ptr+= sizeof(uchar);
  int8store(ptr, (ulonglong)rows);
  ptr+= sizeof(ulonglong);
  memset(ptr, 0, 3*sizeof(ulonglong));
  /*
     Skip over checkpoint, autoincrement and forced_flushes fields.
     We'll need them later.
  */
  ptr+= 3*sizeof(ulonglong);
  *ptr= (uchar)dirty;

  VOID(my_seek(meta_file, 0, MY_SEEK_SET, MYF(0)));
  if (my_write(meta_file, (byte *)meta_buffer, META_BUFFER_SIZE, 0)
      != META_BUFFER_SIZE)
    DBUG_RETURN(-1);

  my_sync(meta_file, MYF(MY_WME));

  DBUG_RETURN(0);
}

bool ha_tina::check_and_repair(THD *thd)
{
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_tina::check_and_repair");

  check_opt.init();

  DBUG_RETURN(repair(thd, &check_opt));
}


int ha_tina::init_tina_writer()
{
  DBUG_ENTER("ha_tina::init_tina_writer");

  /*
    Mark the file as crashed. We will set the flag back when we close
    the file. In the case of the crash it will remain marked crashed,
    which enforce recovery.
  */
  (void)write_meta_file(share->meta_file, share->rows_recorded, TRUE);

  if ((share->tina_write_filedes=
        my_open(share->data_file_name, O_RDWR|O_APPEND, MYF(0))) == -1)
  {
    DBUG_PRINT("info", ("Could not open tina file writes"));
    share->crashed= TRUE;
    DBUG_RETURN(1);
  }
  share->tina_write_opened= TRUE;

  DBUG_RETURN(0);
}


bool ha_tina::is_crashed() const
{
  DBUG_ENTER("ha_tina::is_crashed");
  DBUG_RETURN(share->crashed);
}

/*
  Free lock controls.
*/
static int free_share(TINA_SHARE *share)
{
  DBUG_ENTER("ha_tina::free_share");
  pthread_mutex_lock(&tina_mutex);
  int result_code= 0;
  if (!--share->use_count){
    /* Write the meta file. Mark it as crashed if needed. */
    (void)write_meta_file(share->meta_file, share->rows_recorded,
                          share->crashed ? TRUE :FALSE);
    if (my_close(share->meta_file, MYF(0)))
      result_code= 1;
    if (share->tina_write_opened)
    {
      if (my_close(share->tina_write_filedes, MYF(0)))
        result_code= 1;
      share->tina_write_opened= FALSE;
    }

    hash_delete(&tina_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&tina_mutex);

  DBUG_RETURN(result_code);
}

int tina_end(ha_panic_function type)
{
  return tina_done_func();
}


/*
  This function finds the end of a line and returns the length
  of the line ending.

  We support three kinds of line endings:
  '\r'     --  Old Mac OS line ending
  '\n'     --  Traditional Unix and Mac OS X line ending
  '\r''\n' --  DOS\Windows line ending
*/

off_t find_eoln_buff(Transparent_file *data_buff, off_t begin,
                     off_t end, int *eoln_len)
{
  *eoln_len= 0;

  for (off_t x= begin; x < end; x++)
  {
    /* Unix (includes Mac OS X) */
    if (data_buff->get_value(x) == '\n')
      *eoln_len= 1;
    else
      if (data_buff->get_value(x) == '\r') // Mac or Dos
      {
        /* old Mac line ending */
        if (x + 1 == end || (data_buff->get_value(x + 1) != '\n'))
          *eoln_len= 1;
        else // DOS style ending
          *eoln_len= 2;
      }

    if (*eoln_len)  // end of line was found
      return x;
  }

  return 0;
}


static handler *tina_create_handler(TABLE_SHARE *table, MEM_ROOT *mem_root)
{
  return new (mem_root) ha_tina(table);
}


ha_tina::ha_tina(TABLE_SHARE *table_arg)
  :handler(&tina_hton, table_arg),
  /*
    These definitions are found in handler.h
    They are not probably completely right.
  */
  current_position(0), next_position(0), local_saved_data_file_length(0),
  file_buff(0), chain_alloced(0), chain_size(DEFAULT_CHAIN_LENGTH),
  records_is_known(0)
{
  /* Set our original buffers from pre-allocated memory */
  buffer.set((char*)byte_buffer, IO_SIZE, system_charset_info);
  chain= chain_buffer;
  file_buff= new Transparent_file();
}


/*
  Encode a buffer into the quoted format.
*/

int ha_tina::encode_quote(byte *buf)
{
  char attribute_buffer[1024];
  String attribute(attribute_buffer, sizeof(attribute_buffer),
                   &my_charset_bin);

  my_bitmap_map *org_bitmap= dbug_tmp_use_all_columns(table, table->read_set);
  buffer.length(0);
  for (Field **field=table->field ; *field ; field++)
  {
    const char *ptr;
    const char *end_ptr;

    /*
      Write an empty string to the buffer in case of a NULL value.
      Basically this is a safety check, as no one ensures that the
      field content is cleaned up every time we use Field::set_null()
      in the code.
    */
    if ((*field)->is_null())
      ptr= end_ptr= 0;
    else
    {
      (*field)->val_str(&attribute,&attribute);
      ptr= attribute.ptr();
      end_ptr= attribute.length() + ptr;
    }

    buffer.append('"');

    while (ptr < end_ptr) 
    {
      if (*ptr == '"')
      {
        buffer.append('\\');
        buffer.append('"');
        *ptr++;
      }
      else if (*ptr == '\r')
      {
        buffer.append('\\');
        buffer.append('r');
        *ptr++;
      }
      else if (*ptr == '\\')
      {
        buffer.append('\\');
        buffer.append('\\');
        *ptr++;
      }
      else if (*ptr == '\n')
      {
        buffer.append('\\');
        buffer.append('n');
        *ptr++;
      }
      else
        buffer.append(*ptr++);
    }
    buffer.append('"');
    buffer.append(',');
  }
  // Remove the comma, add a line feed
  buffer.length(buffer.length() - 1);
  buffer.append('\n');
  //buffer.replace(buffer.length(), 0, "\n", 1);

  dbug_tmp_restore_column_map(table->read_set, org_bitmap);
  return (buffer.length());
}

/*
  chain_append() adds delete positions to the chain that we use to keep
  track of space. Then the chain will be used to cleanup "holes", occurred
  due to deletes and updates.
*/
int ha_tina::chain_append()
{
  if ( chain_ptr != chain && (chain_ptr -1)->end == current_position)
    (chain_ptr -1)->end= next_position;
  else
  {
    /* We set up for the next position */
    if ((off_t)(chain_ptr - chain) == (chain_size -1))
    {
      off_t location= chain_ptr - chain;
      chain_size += DEFAULT_CHAIN_LENGTH;
      if (chain_alloced)
      {
        /* Must cast since my_malloc unlike malloc doesn't have a void ptr */
        if ((chain= (tina_set *) my_realloc((gptr)chain,
                                            chain_size, MYF(MY_WME))) == NULL)
          return -1;
      }
      else
      {
        tina_set *ptr= (tina_set *) my_malloc(chain_size * sizeof(tina_set),
                                              MYF(MY_WME));
        memcpy(ptr, chain, DEFAULT_CHAIN_LENGTH * sizeof(tina_set));
        chain= ptr;
        chain_alloced++;
      }
      chain_ptr= chain + location;
    }
    chain_ptr->begin= current_position;
    chain_ptr->end= next_position;
    chain_ptr++;
  }

  return 0;
}


/*
  Scans for a row.
*/
int ha_tina::find_current_row(byte *buf)
{
  off_t end_offset, curr_offset= current_position;
  int eoln_len;
  my_bitmap_map *org_bitmap;
  DBUG_ENTER("ha_tina::find_current_row");

  /*
    We do not read further then local_saved_data_file_length in order
    not to conflict with undergoing concurrent insert.
  */
  if ((end_offset=
        find_eoln_buff(file_buff, current_position,
                       local_saved_data_file_length, &eoln_len)) == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  /* Avoid asserts in ::store() for columns that are not going to be updated */
  org_bitmap= dbug_tmp_use_all_columns(table, table->write_set);

  for (Field **field=table->field ; *field ; field++)
  {
    buffer.length(0);
    if (file_buff->get_value(curr_offset) == '"')
      curr_offset++; // Incrementpast the first quote
    else
    {
      dbug_tmp_restore_column_map(table->write_set, org_bitmap);
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
    }
    for(;curr_offset != end_offset; curr_offset++)
    {
      // Need to convert line feeds!
      if (file_buff->get_value(curr_offset) == '"' &&
          (((file_buff->get_value(curr_offset + 1) == ',') &&
            (file_buff->get_value(curr_offset + 2) == '"')) ||
           (curr_offset == end_offset -1 )))
      {
        curr_offset+= 2; // Move past the , and the "
        break;
      }
      if (file_buff->get_value(curr_offset) == '\\' &&
          curr_offset != (end_offset - 1))
      {
        curr_offset++;
        if (file_buff->get_value(curr_offset) == 'r')
          buffer.append('\r');
        else if (file_buff->get_value(curr_offset) == 'n' )
          buffer.append('\n');
        else if ((file_buff->get_value(curr_offset) == '\\') ||
                 (file_buff->get_value(curr_offset) == '"'))
          buffer.append(file_buff->get_value(curr_offset));
        else  /* This could only happed with an externally created file */
        {
          buffer.append('\\');
          buffer.append(file_buff->get_value(curr_offset));
        }
      }
      else // ordinary symbol
      {
        /*
          We are at final symbol and no last quote was found =>
          we are working with a damaged file.
        */
        if (curr_offset == end_offset - 1)
        {
          dbug_tmp_restore_column_map(table->write_set, org_bitmap);
          DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
        }
        buffer.append(file_buff->get_value(curr_offset));
      }
    }
    if (bitmap_is_set(table->read_set, (*field)->field_index))
      (*field)->store(buffer.ptr(), buffer.length(), system_charset_info);
  }
  next_position= end_offset + eoln_len;
  /* Maybe use \N for null? */
  memset(buf, 0, table->s->null_bytes); /* We do not implement nulls! */
  dbug_tmp_restore_column_map(table->write_set, org_bitmap);

  DBUG_RETURN(0);
}

/*
  If frm_error() is called in table.cc this is called to find out what file
  extensions exist for this handler.
*/
static const char *ha_tina_exts[] = {
  CSV_EXT,
  CSM_EXT,
  NullS
};

const char **ha_tina::bas_ext() const
{
  return ha_tina_exts;
}

/*
  Three functions below are needed to enable concurrent insert functionality
  for CSV engine. For more details see mysys/thr_lock.c
*/

void tina_get_status(void* param, int concurrent_insert)
{
  ha_tina *tina= (ha_tina*) param;
  tina->get_status();
}

void tina_update_status(void* param)
{
  ha_tina *tina= (ha_tina*) param;
  tina->update_status();
}

/* this should exist and return 0 for concurrent insert to work */
my_bool tina_check_status(void* param)
{
  return 0;
}

/*
  Save the state of the table

  SYNOPSIS
    get_status()

  DESCRIPTION
    This function is used to retrieve the file length. During the lock
    phase of concurrent insert. For more details see comment to
    ha_tina::update_status below.
*/

void ha_tina::get_status()
{
  if (share->is_log_table)
  {
    /*
      We have to use mutex to follow pthreads memory visibility
      rules for share->saved_data_file_length
    */
    pthread_mutex_lock(&share->mutex);
    local_saved_data_file_length= share->saved_data_file_length;
    pthread_mutex_unlock(&share->mutex);
    return;
  }
  local_saved_data_file_length= share->saved_data_file_length;
}


/*
  Correct the state of the table. Called by unlock routines
  before the write lock is released.

  SYNOPSIS
    update_status()

  DESCRIPTION
    When we employ concurrent insert lock, we save current length of the file
    during the lock phase. We do not read further saved value, as we don't
    want to interfere with undergoing concurrent insert. Writers update file
    length info during unlock with update_status().

  NOTE
    For log tables concurrent insert works different. The reason is that
    log tables are always opened and locked. And as they do not unlock
    tables, the file length after writes should be updated in a different
    way. For this purpose we need is_log_table flag. When this flag is set
    we call update_status() explicitly after each row write.
*/

void ha_tina::update_status()
{
  /* correct local_saved_data_file_length for writers */
  share->saved_data_file_length= local_saved_data_file_length;
}


bool ha_tina::check_if_locking_is_allowed(uint sql_command,
                                          ulong type, TABLE *table,
                                          uint count,
                                          bool called_by_logger_thread)
{
  if (!called_by_logger_thread)
    return check_if_log_table_locking_is_allowed(sql_command, type, table);

  return TRUE;
}

/*
  Open a database file. Keep in mind that tables are caches, so
  this will not be called for every request. Any sort of positions
  that need to be reset should be kept in the ::extra() call.
*/
int ha_tina::open(const char *name, int mode, uint open_options)
{
  DBUG_ENTER("ha_tina::open");

  if (!(share= get_share(name, table)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  if (share->crashed && !(open_options & HA_OPEN_FOR_REPAIR))
  {
    free_share(share);
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  if ((data_file= my_open(share->data_file_name, O_RDONLY, MYF(0))) == -1)
    DBUG_RETURN(0);

  /*
    Init locking. Pass handler object to the locking routines,
    so that they could save/update local_saved_data_file_length value
    during locking. This is needed to enable concurrent inserts.
  */
  thr_lock_data_init(&share->lock, &lock, (void*) this);
  ref_length=sizeof(off_t);

  share->lock.get_status= tina_get_status;
  share->lock.update_status= tina_update_status;
  share->lock.check_status= tina_check_status;

  DBUG_RETURN(0);
}


/*
  Close a database file. We remove ourselves from the shared strucutre.
  If it is empty we destroy it.
*/
int ha_tina::close(void)
{
  int rc= 0;
  DBUG_ENTER("ha_tina::close");
  rc= my_close(data_file, MYF(0));
  DBUG_RETURN(free_share(share) || rc);
}

/*
  This is an INSERT. At the moment this handler just seeks to the end
  of the file and appends the data. In an error case it really should
  just truncate to the original position (this is not done yet).
*/
int ha_tina::write_row(byte * buf)
{
  int size;
  DBUG_ENTER("ha_tina::write_row");

  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  ha_statistic_increment(&SSV::ha_write_count);

  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  size= encode_quote(buf);

  if (!share->tina_write_opened)
    if (init_tina_writer())
      DBUG_RETURN(-1);

   /* use pwrite, as concurrent reader could have changed the position */
  if (my_write(share->tina_write_filedes, (byte*)buffer.ptr(), size,
               MYF(MY_WME | MY_NABP)))
    DBUG_RETURN(-1);

  /* update local copy of the max position to see our own changes */
  local_saved_data_file_length+= size;

  /* update shared info */
  pthread_mutex_lock(&share->mutex);
  share->rows_recorded++;
  /* update status for the log tables */
  if (share->is_log_table)
    update_status();
  pthread_mutex_unlock(&share->mutex);

  stats.records++;
  DBUG_RETURN(0);
}


int ha_tina::open_update_temp_file_if_needed()
{
  char updated_fname[FN_REFLEN];

  if (!share->update_file_opened)
  {
    if ((update_temp_file=
           my_create(fn_format(updated_fname, share->table_name,
                               "", CSN_EXT,
                               MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                     0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
      return 1;
    share->update_file_opened= TRUE;
  }
  return 0;
}

/*
  This is called for an update.
  Make sure you put in code to increment the auto increment, also
  update any timestamp data. Currently auto increment is not being
  fixed since autoincrements have yet to be added to this table handler.
  This will be called in a table scan right before the previous ::rnd_next()
  call.
*/
int ha_tina::update_row(const byte * old_data, byte * new_data)
{
  int size;
  DBUG_ENTER("ha_tina::update_row");

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();

  size= encode_quote(new_data);

  if (chain_append())
    DBUG_RETURN(-1);

  if (open_update_temp_file_if_needed())
    DBUG_RETURN(-1);

  if (my_write(update_temp_file, (byte*)buffer.ptr(), size,
               MYF(MY_WME | MY_NABP)))
    DBUG_RETURN(-1);

  /* UPDATE should never happen on the log tables */
  DBUG_ASSERT(!share->is_log_table);

  DBUG_RETURN(0);
}


/*
  Deletes a row. First the database will find the row, and then call this
  method. In the case of a table scan, the previous call to this will be
  the ::rnd_next() that found this row.
  The exception to this is an ORDER BY. This will cause the table handler
  to walk the table noting the positions of all rows that match a query.
  The table will then be deleted/positioned based on the ORDER (so RANDOM,
  DESC, ASC).
*/
int ha_tina::delete_row(const byte * buf)
{
  DBUG_ENTER("ha_tina::delete_row");
  ha_statistic_increment(&SSV::ha_delete_count);

  if (chain_append())
    DBUG_RETURN(-1);

  stats.records--;

  /* DELETE should never happen on the log table */
  DBUG_ASSERT(!share->is_log_table);

  DBUG_RETURN(0);
}


/*
  All table scans call this first.
  The order of a table scan is:

  ha_tina::store_lock
  ha_tina::external_lock
  ha_tina::info
  ha_tina::rnd_init
  ha_tina::extra
  ENUM HA_EXTRA_CACHE   Cash record in HA_rrnd()
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::rnd_next
  ha_tina::extra
  ENUM HA_EXTRA_NO_CACHE   End cacheing of records (def)
  ha_tina::external_lock
  ha_tina::extra
  ENUM HA_EXTRA_RESET   Reset database to after open

  Each call to ::rnd_next() represents a row returned in the can. When no more
  rows can be returned, rnd_next() returns a value of HA_ERR_END_OF_FILE.
  The ::info() call is just for the optimizer.

*/

int ha_tina::rnd_init(bool scan)
{
  DBUG_ENTER("ha_tina::rnd_init");

  /* set buffer to the beginning of the file */
  file_buff->init_buff(data_file);
  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  current_position= next_position= 0;
  stats.records= 0;
  records_is_known= 0;
  chain_ptr= chain;

  DBUG_RETURN(0);
}

/*
  ::rnd_next() does all the heavy lifting for a table scan. You will need to
  populate *buf with the correct field data. You can walk the field to
  determine at what position you should store the data (take a look at how
  ::find_current_row() works). The structure is something like:
  0Foo  Dog  Friend
  The first offset is for the first attribute. All space before that is
  reserved for null count.
  Basically this works as a mask for which rows are nulled (compared to just
  empty).
  This table handler doesn't do nulls and does not know the difference between
  NULL and "". This is ok since this table handler is for spreadsheets and
  they don't know about them either :)
*/
int ha_tina::rnd_next(byte *buf)
{
  int rc;
  DBUG_ENTER("ha_tina::rnd_next");

  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  current_position= next_position;

  /* don't scan an empty file */
  if (!local_saved_data_file_length)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  if ((rc= find_current_row(buf)))
    DBUG_RETURN(rc);

  stats.records++;
  DBUG_RETURN(0);
}

/*
  In the case of an order by rows will need to be sorted.
  ::position() is called after each call to ::rnd_next(),
  the data it stores is to a byte array. You can store this
  data via my_store_ptr(). ref_length is a variable defined to the
  class that is the sizeof() of position being stored. In our case
  its just a position. Look at the bdb code if you want to see a case
  where something other then a number is stored.
*/
void ha_tina::position(const byte *record)
{
  DBUG_ENTER("ha_tina::position");
  my_store_ptr(ref, ref_length, current_position);
  DBUG_VOID_RETURN;
}


/*
  Used to fetch a row from a posiion stored with ::position().
  my_get_ptr() retrieves the data for you.
*/

int ha_tina::rnd_pos(byte * buf, byte *pos)
{
  DBUG_ENTER("ha_tina::rnd_pos");
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  current_position= my_get_ptr(pos,ref_length);
  DBUG_RETURN(find_current_row(buf));
}

/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
*/
void ha_tina::info(uint flag)
{
  DBUG_ENTER("ha_tina::info");
  /* This is a lie, but you don't want the optimizer to see zero or 1 */
  if (!records_is_known && stats.records < 2) 
    stats.records= 2;
  DBUG_VOID_RETURN;
}

/*
  Grab bag of flags that are sent to the able handler every so often.
  HA_EXTRA_RESET and HA_EXTRA_RESET_STATE are the most frequently called.
  You are not required to implement any of these.
*/
int ha_tina::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_tina::extra");
 if (operation == HA_EXTRA_MARK_AS_LOG_TABLE)
 {
   pthread_mutex_lock(&share->mutex);
   share->is_log_table= TRUE;
   pthread_mutex_unlock(&share->mutex);
 }
  DBUG_RETURN(0);
}

/*
  Set end_pos to the last valid byte of continuous area, closest
  to the given "hole", stored in the buffer. "Valid" here means,
  not listed in the chain of deleted records ("holes").
*/
bool ha_tina::get_write_pos(off_t *end_pos, tina_set *closest_hole)
{
  if (closest_hole == chain_ptr) /* no more chains */
    *end_pos= file_buff->end();
  else
    *end_pos= min(file_buff->end(),
                  closest_hole->begin);
  return (closest_hole != chain_ptr) && (*end_pos == closest_hole->begin);
}


/*
  Called after each table scan. In particular after deletes,
  and updates. In the last case we employ chain of deleted
  slots to clean up all of the dead space we have collected while
  performing deletes/updates.
*/
int ha_tina::rnd_end()
{
  char updated_fname[FN_REFLEN];
  off_t file_buffer_start= 0;
  DBUG_ENTER("ha_tina::rnd_end");

  records_is_known= 1;

  if ((chain_ptr - chain)  > 0)
  {
    tina_set *ptr= chain;

    /*
      Re-read the beginning of a file (as the buffer should point to the
      end of file after the scan).
    */
    file_buff->init_buff(data_file);

    /*
      The sort is needed when there were updates/deletes with random orders.
      It sorts so that we move the firts blocks to the beginning.
    */
    qsort(chain, (size_t)(chain_ptr - chain), sizeof(tina_set),
          (qsort_cmp)sort_set);

    off_t write_begin= 0, write_end;

    /* create the file to write updated table if it wasn't yet created */
    if (open_update_temp_file_if_needed())
      DBUG_RETURN(-1);

    /* write the file with updated info */
    while ((file_buffer_start != -1))     // while not end of file
    {
      bool in_hole= get_write_pos(&write_end, ptr);

      /* if there is something to write, write it */
      if ((write_end - write_begin) &&
          (my_write(update_temp_file,
                    (byte*)(file_buff->ptr() +
                            (write_begin - file_buff->start())),
                    write_end - write_begin, MYF_RW)))
        goto error;

      if (in_hole)
      {
        /* skip hole */
        while (file_buff->end() <= ptr->end && file_buffer_start != -1)
          file_buffer_start= file_buff->read_next();
        write_begin= ptr->end;
        ptr++;
      }
      else
        write_begin= write_end;

      if (write_end == file_buff->end())
        file_buffer_start= file_buff->read_next(); /* shift the buffer */

    }

    if (my_sync(update_temp_file, MYF(MY_WME)) ||
        my_close(update_temp_file, MYF(0)))
      DBUG_RETURN(-1);

    share->update_file_opened= FALSE;

    if (share->tina_write_opened)
    {
      if (my_close(share->tina_write_filedes, MYF(0)))
        DBUG_RETURN(-1);
      /*
        Mark that the writer fd is closed, so that init_tina_writer()
        will reopen it later.
      */
      share->tina_write_opened= FALSE;
    }

    /*
      Close opened fildes's. Then move updated file in place
      of the old datafile.
    */
    if (my_close(data_file, MYF(0)) ||
        my_rename(fn_format(updated_fname, share->table_name, "", CSN_EXT,
                            MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                  share->data_file_name, MYF(0)))
      DBUG_RETURN(-1);

    /* Open the file again */
    if (((data_file= my_open(share->data_file_name, O_RDONLY, MYF(0))) == -1))
      DBUG_RETURN(-1);
    /*
      The datafile is consistent at this point and the write filedes is
      closed, so nothing worrying will happen to it in case of a crash.
      Here we record this fact to the meta-file.
    */
    (void)write_meta_file(share->meta_file, share->rows_recorded, FALSE);
  }

  DBUG_RETURN(0);
error:
  my_close(update_temp_file, MYF(0));
  share->update_file_opened= FALSE;
  DBUG_RETURN(-1);
}


/*
  Repair CSV table in the case, it is crashed.

  SYNOPSIS
    repair()
    thd         The thread, performing repair
    check_opt   The options for repair. We do not use it currently.

  DESCRIPTION
    If the file is empty, change # of rows in the file and complete recovery.
    Otherwise, scan the table looking for bad rows. If none were found,
    we mark file as a good one and return. If a bad row was encountered,
    we truncate the datafile up to the last good row.

   TODO: Make repair more clever - it should try to recover subsequent
         rows (after the first bad one) as well.
*/

int ha_tina::repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  char repaired_fname[FN_REFLEN];
  byte *buf;
  File repair_file;
  int rc;
  ha_rows rows_repaired= 0;
  off_t write_begin= 0, write_end;
  DBUG_ENTER("ha_tina::repair");

  /* empty file */
  if (!share->saved_data_file_length)
  {
    share->rows_recorded= 0;
    goto end;
  }

  /* Don't assert in field::val() functions */
  table->use_all_columns();
  if (!(buf= (byte*) my_malloc(table->s->reclength, MYF(MY_WME))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  /* position buffer to the start of the file */
  file_buff->init_buff(data_file);

  /*
    Local_saved_data_file_length is initialized during the lock phase.
    Sometimes this is not getting executed before ::repair (e.g. for
    the log tables). We set it manually here.
  */
  local_saved_data_file_length= share->saved_data_file_length;
  /* set current position to the beginning of the file */
  current_position= next_position= 0;

  /* Read the file row-by-row. If everything is ok, repair is not needed. */
  while (!(rc= find_current_row(buf)))
  {
    rows_repaired++;
    current_position= next_position;
  }

  my_free((char*)buf, MYF(0));

  if (rc == HA_ERR_END_OF_FILE)
  {
    /*
      All rows were read ok until end of file, the file does not need repair.
      If rows_recorded != rows_repaired, we should update rows_recorded value
      to the current amount of rows.
    */
    share->rows_recorded= rows_repaired;
    goto end;
  }

  /*
    Otherwise we've encountered a bad row => repair is needed.
    Let us create a temporary file.
  */
  if ((repair_file= my_create(fn_format(repaired_fname, share->table_name,
                                        "", CSN_EXT,
                                        MY_REPLACE_EXT|MY_UNPACK_FILENAME),
                           0, O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    DBUG_RETURN(HA_ERR_CRASHED_ON_REPAIR);

  file_buff->init_buff(data_file);


  /* we just truncated the file up to the first bad row. update rows count. */
  share->rows_recorded= rows_repaired;

  /* write repaired file */
  while (1)
  {
    write_end= min(file_buff->end(), current_position);
    if ((write_end - write_begin) &&
        (my_write(repair_file, (byte*)file_buff->ptr(),
                  write_end - write_begin, MYF_RW)))
      DBUG_RETURN(-1);

    write_begin= write_end;
    if (write_end== current_position)
      break;
    else
      file_buff->read_next(); /* shift the buffer */
  }

  /*
    Close the files and rename repaired file to the datafile.
    We have to close the files, as on Windows one cannot rename
    a file, which descriptor is still open. EACCES will be returned
    when trying to delete the "to"-file in my_rename().
  */
  if (my_close(data_file,MYF(0)) || my_close(repair_file, MYF(0)) ||
      my_rename(repaired_fname, share->data_file_name, MYF(0)))
    DBUG_RETURN(-1);

  /* Open the file again, it should now be repaired */
  if ((data_file= my_open(share->data_file_name, O_RDWR|O_APPEND,
                          MYF(0))) == -1)
     DBUG_RETURN(-1);

  /* Set new file size. The file size will be updated by ::update_status() */
  local_saved_data_file_length= (size_t) current_position;

end:
  share->crashed= FALSE;
  DBUG_RETURN(HA_ADMIN_OK);
}

/*
  DELETE without WHERE calls this
*/

int ha_tina::delete_all_rows()
{
  int rc;
  DBUG_ENTER("ha_tina::delete_all_rows");

  if (!records_is_known)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_COMMAND);

  if (!share->tina_write_opened)
    if (init_tina_writer())
      DBUG_RETURN(-1);

  /* Truncate the file to zero size */
  rc= my_chsize(share->tina_write_filedes, 0, 0, MYF(MY_WME));

  stats.records=0;
  DBUG_RETURN(rc);
}

/*
  Called by the database to lock the table. Keep in mind that this
  is an internal lock.
*/
THR_LOCK_DATA **ha_tina::store_lock(THD *thd,
                                    THR_LOCK_DATA **to,
                                    enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
  return to;
}

/* 
  Create a table. You do not want to leave the table open after a call to
  this (the database will call ::open() if it needs to).
*/

int ha_tina::create(const char *name, TABLE *table_arg,
                    HA_CREATE_INFO *create_info)
{
  char name_buff[FN_REFLEN];
  File create_file;
  DBUG_ENTER("ha_tina::create");

  if ((create_file= my_create(fn_format(name_buff, name, "", CSM_EXT,
                                        MY_REPLACE_EXT|MY_UNPACK_FILENAME), 0,
                              O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    DBUG_RETURN(-1);

  write_meta_file(create_file, 0, FALSE);
  my_close(create_file, MYF(0));

  if ((create_file= my_create(fn_format(name_buff, name, "", CSV_EXT,
                                        MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                              O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    DBUG_RETURN(-1);

  my_close(create_file, MYF(0));

  DBUG_RETURN(0);
}

int ha_tina::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  int rc= 0;
  byte *buf;
  const char *old_proc_info;
  ha_rows count= share->rows_recorded;
  DBUG_ENTER("ha_tina::check");

  old_proc_info= thd_proc_info(thd, "Checking table");
  if (!(buf= (byte*) my_malloc(table->s->reclength, MYF(MY_WME))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  /* position buffer to the start of the file */
  file_buff->init_buff(data_file);

  /*
    Local_saved_data_file_length is initialized during the lock phase.
    Check does not use store_lock in certain cases. So, we set it
    manually here.
  */
  local_saved_data_file_length= share->saved_data_file_length;
  /* set current position to the beginning of the file */
  current_position= next_position= 0;
  /* Read the file row-by-row. If everything is ok, repair is not needed. */
  while (!(rc= find_current_row(buf)))
  {
    count--;
    current_position= next_position;
  }

  my_free((char*)buf, MYF(0));
  thd_proc_info(thd, old_proc_info);

  if ((rc != HA_ERR_END_OF_FILE) || count)
  {
    share->crashed= TRUE;
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  }
  else
    DBUG_RETURN(HA_ADMIN_OK);
}


bool ha_tina::check_if_incompatible_data(HA_CREATE_INFO *info,
					   uint table_changes)
{
  return COMPATIBLE_DATA_YES;
}

struct st_mysql_storage_engine csv_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION, &tina_hton };

mysql_declare_plugin(csv)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &csv_storage_engine,
  "CSV",
  "Brian Aker, MySQL AB",
  "CSV storage engine",
  tina_init_func, /* Plugin Init */
  tina_done_func, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  0
}
mysql_declare_plugin_end;

