/* Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "sql_class.h"                          // SSV
#include <mysql/plugin.h>
#include <mysql/psi/mysql_file.h>
#include "ha_tina.h"
#include "probes_mysql.h"

#include <algorithm>

using std::min;
using std::max;

/*
  uchar + uchar + ulonglong + ulonglong + ulonglong + ulonglong + uchar
*/
#define META_BUFFER_SIZE sizeof(uchar) + sizeof(uchar) + sizeof(ulonglong) \
  + sizeof(ulonglong) + sizeof(ulonglong) + sizeof(ulonglong) + sizeof(uchar)
#define TINA_CHECK_HEADER 254 // The number we use to determine corruption
#define BLOB_MEMROOT_ALLOC_SIZE 8192

/* The file extension */
#define CSV_EXT ".CSV"               // The data file
#define CSN_EXT ".CSN"               // Files used during repair and update
#define CSM_EXT ".CSM"               // Meta file


static TINA_SHARE *get_share(const char *table_name, TABLE *table);
static int free_share(TINA_SHARE *share);
static int read_meta_file(File meta_file, ha_rows *rows);
static int write_meta_file(File meta_file, ha_rows rows, bool dirty);

extern "C" void tina_get_status(void* param, int concurrent_insert);
extern "C" void tina_update_status(void* param);
extern "C" my_bool tina_check_status(void* param);

/* Stuff for shares */
mysql_mutex_t tina_mutex;
static HASH tina_open_tables;
static handler *tina_create_handler(handlerton *hton,
                                    TABLE_SHARE *table, 
                                    MEM_ROOT *mem_root);


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

static uchar* tina_get_key(TINA_SHARE *share, size_t *length,
                          my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length=share->table_name_length;
  return (uchar*) share->table_name;
}

static PSI_memory_key csv_key_memory_tina_share;
static PSI_memory_key csv_key_memory_blobroot;
static PSI_memory_key csv_key_memory_tina_set;
static PSI_memory_key csv_key_memory_row;

#ifdef HAVE_PSI_INTERFACE

static PSI_mutex_key csv_key_mutex_tina, csv_key_mutex_TINA_SHARE_mutex;

static PSI_mutex_info all_tina_mutexes[]=
{
  { &csv_key_mutex_tina, "tina", PSI_FLAG_GLOBAL},
  { &csv_key_mutex_TINA_SHARE_mutex, "TINA_SHARE::mutex", 0}
};

static PSI_file_key csv_key_file_metadata, csv_key_file_data,
  csv_key_file_update;

static PSI_file_info all_tina_files[]=
{
  { &csv_key_file_metadata, "metadata", 0},
  { &csv_key_file_data, "data", 0},
  { &csv_key_file_update, "update", 0}
};

static PSI_memory_info all_tina_memory[]=
{
  { &csv_key_memory_tina_share, "TINA_SHARE", PSI_FLAG_GLOBAL},
  { &csv_key_memory_blobroot, "blobroot", 0},
  { &csv_key_memory_tina_set, "tina_set", 0},
  { &csv_key_memory_row, "row", 0},
  { &csv_key_memory_Transparent_file, "Transparent_file", 0}
};

static void init_tina_psi_keys(void)
{
  const char* category= "csv";
  int count;

  count= array_elements(all_tina_mutexes);
  mysql_mutex_register(category, all_tina_mutexes, count);

  count= array_elements(all_tina_files);
  mysql_file_register(category, all_tina_files, count);

  count= array_elements(all_tina_memory);
  mysql_memory_register(category, all_tina_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */

static int tina_init_func(void *p)
{
  handlerton *tina_hton;

#ifdef HAVE_PSI_INTERFACE
  init_tina_psi_keys();
#endif

  tina_hton= (handlerton *)p;
  mysql_mutex_init(csv_key_mutex_tina, &tina_mutex, MY_MUTEX_INIT_FAST);
  (void) my_hash_init(&tina_open_tables,system_charset_info,32,0,0,
                      (my_hash_get_key) tina_get_key,0,0,
                      csv_key_memory_tina_share);
  tina_hton->state= SHOW_OPTION_YES;
  tina_hton->db_type= DB_TYPE_CSV_DB;
  tina_hton->create= tina_create_handler;
  tina_hton->flags= (HTON_CAN_RECREATE | HTON_SUPPORT_LOG_TABLES | 
                     HTON_NO_PARTITION);
  return 0;
}

static int tina_done_func(void *p)
{
  my_hash_free(&tina_open_tables);
  mysql_mutex_destroy(&tina_mutex);

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

  mysql_mutex_lock(&tina_mutex);
  length=(uint) strlen(table_name);

  /*
    If share is not present in the hash, create a new share and
    initialize its members.
  */
  if (!(share=(TINA_SHARE*) my_hash_search(&tina_open_tables,
                                           (uchar*) table_name,
                                           length)))
  {
    if (!my_multi_malloc(csv_key_memory_tina_share,
                         MYF(MY_WME | MY_ZEROFILL),
                         &share, sizeof(*share),
                         &tmp_name, length+1,
                         NullS))
    {
      mysql_mutex_unlock(&tina_mutex);
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
    share->data_file_version= 0;
    my_stpcpy(share->table_name, table_name);
    fn_format(share->data_file_name, table_name, "", CSV_EXT,
              MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    fn_format(meta_file_name, table_name, "", CSM_EXT,
              MY_REPLACE_EXT|MY_UNPACK_FILENAME);

    if (mysql_file_stat(csv_key_file_data,
                        share->data_file_name, &file_stat, MYF(MY_WME)) == NULL)
      goto error;
    share->saved_data_file_length= file_stat.st_size;

    if (my_hash_insert(&tina_open_tables, (uchar*) share))
      goto error;
    thr_lock_init(&share->lock);
    mysql_mutex_init(csv_key_mutex_TINA_SHARE_mutex,
                     &share->mutex, MY_MUTEX_INIT_FAST);

    /*
      Open or create the meta file. In the latter case, we'll get
      an error during read_meta_file and mark the table as crashed.
      Usually this will result in auto-repair, and we will get a good
      meta-file in the end.
    */
    if (((share->meta_file= mysql_file_open(csv_key_file_metadata,
                                            meta_file_name,
                                            O_RDWR|O_CREAT,
                                            MYF(MY_WME))) == -1) ||
        read_meta_file(share->meta_file, &share->rows_recorded))
      share->crashed= TRUE;
  }

  share->use_count++;
  mysql_mutex_unlock(&tina_mutex);

  return share;

error:
  mysql_mutex_unlock(&tina_mutex);
  my_free(share);

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

  mysql_file_seek(meta_file, 0, MY_SEEK_SET, MYF(0));
  if (mysql_file_read(meta_file, (uchar*)meta_buffer, META_BUFFER_SIZE, 0)
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

  mysql_file_sync(meta_file, MYF(MY_WME));

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

  mysql_file_seek(meta_file, 0, MY_SEEK_SET, MYF(0));
  if (mysql_file_write(meta_file, (uchar *)meta_buffer, META_BUFFER_SIZE, 0)
      != META_BUFFER_SIZE)
    DBUG_RETURN(-1);

  mysql_file_sync(meta_file, MYF(MY_WME));

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
        mysql_file_open(csv_key_file_data,
                        share->data_file_name, O_RDWR|O_APPEND,
                        MYF(MY_WME))) == -1)
  {
    DBUG_PRINT("info", ("Could not open tina file writes"));
    share->crashed= TRUE;
    DBUG_RETURN(my_errno() ? my_errno() : -1);
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
  mysql_mutex_lock(&tina_mutex);
  int result_code= 0;
  if (!--share->use_count){
    /* Write the meta file. Mark it as crashed if needed. */
    (void)write_meta_file(share->meta_file, share->rows_recorded,
                          share->crashed ? TRUE :FALSE);
    if (mysql_file_close(share->meta_file, MYF(0)))
      result_code= 1;
    if (share->tina_write_opened)
    {
      if (mysql_file_close(share->tina_write_filedes, MYF(0)))
        result_code= 1;
      share->tina_write_opened= FALSE;
    }

    my_hash_delete(&tina_open_tables, (uchar*) share);
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->mutex);
    my_free(share);
  }
  mysql_mutex_unlock(&tina_mutex);

  DBUG_RETURN(result_code);
}


/*
  This function finds the end of a line and returns the length
  of the line ending.

  We support three kinds of line endings:
  '\r'     --  Old Mac OS line ending
  '\n'     --  Traditional Unix and Mac OS X line ending
  '\r''\n' --  DOS\Windows line ending
*/

my_off_t find_eoln_buff(Transparent_file *data_buff, my_off_t begin,
                     my_off_t end, int *eoln_len)
{
  *eoln_len= 0;

  for (my_off_t x= begin; x < end; x++)
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


static handler *tina_create_handler(handlerton *hton,
                                    TABLE_SHARE *table, 
                                    MEM_ROOT *mem_root)
{
  return new (mem_root) ha_tina(hton, table);
}


ha_tina::ha_tina(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg),
  /*
    These definitions are found in handler.h
    They are not probably completely right.
  */
  current_position(0), next_position(0), local_saved_data_file_length(0),
  file_buff(0), chain_alloced(0), chain_size(DEFAULT_CHAIN_LENGTH),
  local_data_file_version(0), records_is_known(0)
{
  /* Set our original buffers from pre-allocated memory */
  buffer.set((char*)byte_buffer, IO_SIZE, &my_charset_bin);
  chain= chain_buffer;
  file_buff= new Transparent_file();
  init_alloc_root(csv_key_memory_blobroot,
                  &blobroot, BLOB_MEMROOT_ALLOC_SIZE, 0);
}


/*
  Encode a buffer into the quoted format.
*/

int ha_tina::encode_quote(uchar *buf)
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
    const bool was_null= (*field)->is_null();

    /*
      assistance for backwards compatibility in production builds.
      note: this will not work for ENUM columns.
    */
    if (was_null)
    {
      (*field)->set_default();
      (*field)->set_notnull();
    }

    (*field)->val_str(&attribute,&attribute);

    if (was_null)
      (*field)->set_null();

    if ((*field)->str_needs_quotes())
    {
      ptr= attribute.ptr();
      end_ptr= attribute.length() + ptr;

      buffer.append('"');

      for (; ptr < end_ptr; ptr++)
      {
        if (*ptr == '"')
        {
          buffer.append('\\');
          buffer.append('"');
        }
        else if (*ptr == '\r')
        {
          buffer.append('\\');
          buffer.append('r');
        }
        else if (*ptr == '\\')
        {
          buffer.append('\\');
          buffer.append('\\');
        }
        else if (*ptr == '\n')
        {
          buffer.append('\\');
          buffer.append('n');
        }
        else
          buffer.append(*ptr);
      }
      buffer.append('"');
    }
    else
    {
      buffer.append(attribute);
    }

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
    if ((size_t)(chain_ptr - chain) == (chain_size -1))
    {
      my_off_t location= chain_ptr - chain;
      chain_size += DEFAULT_CHAIN_LENGTH;
      if (chain_alloced)
      {
        /* Must cast since my_malloc unlike malloc doesn't have a void ptr */
        if ((chain= (tina_set *) my_realloc(csv_key_memory_tina_set,
                                            (uchar*)chain,
                                            chain_size*sizeof(tina_set), MYF(MY_WME))) == NULL)
          return -1;
      }
      else
      {
        tina_set *ptr= (tina_set *) my_malloc(csv_key_memory_tina_set,
                                              chain_size * sizeof(tina_set),
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
int ha_tina::find_current_row(uchar *buf)
{
  my_off_t end_offset, curr_offset= current_position;
  int eoln_len;
  my_bitmap_map *org_bitmap;
  int error;
  bool read_all;
  DBUG_ENTER("ha_tina::find_current_row");

  free_root(&blobroot, MYF(0));

  /*
    We do not read further then local_saved_data_file_length in order
    not to conflict with undergoing concurrent insert.
  */
  if ((end_offset=
        find_eoln_buff(file_buff, current_position,
                       local_saved_data_file_length, &eoln_len)) == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  /* We must read all columns in case a table is opened for update */
  read_all= !bitmap_is_clear_all(table->write_set);
  /* Avoid asserts in ::store() for columns that are not going to be updated */
  org_bitmap= dbug_tmp_use_all_columns(table, table->write_set);
  error= HA_ERR_CRASHED_ON_USAGE;

  memset(buf, 0, table->s->null_bytes);

  /*
    Parse the line obtained using the following algorithm
   
    BEGIN
      1) Store the EOL (end of line) for the current row
      2) Until all the fields in the current query have not been 
         filled
         2.1) If the current character is a quote
              2.1.1) Until EOL has not been reached
                     a) If end of current field is reached, move
                        to next field and jump to step 2.3
                     b) If current character is a \\ handle
                        \\n, \\r, \\, \\"
                     c) else append the current character into the buffer
                        before checking that EOL has not been reached.
          2.2) If the current character does not begin with a quote
               2.2.1) Until EOL has not been reached
                      a) If the end of field has been reached move to the
                         next field and jump to step 2.3
                      b) If current character begins with \\ handle
                        \\n, \\r, \\, \\"
                      c) else append the current character into the buffer
                         before checking that EOL has not been reached.
          2.3) Store the current field value and jump to 2)
    TERMINATE
  */  

  for (Field **field=table->field ; *field ; field++)
  {
    char curr_char;
    
    buffer.length(0);
    if (curr_offset >= end_offset)
      goto err;
    curr_char= file_buff->get_value(curr_offset);
    /* Handle the case where the first character is a quote */
    if (curr_char == '"')
    {
      /* Increment past the first quote */
      curr_offset++;

      /* Loop through the row to extract the values for the current field */
      for ( ; curr_offset < end_offset; curr_offset++)
      {
        curr_char= file_buff->get_value(curr_offset);
        /* check for end of the current field */
        if (curr_char == '"' &&
            (curr_offset == end_offset - 1 ||
             file_buff->get_value(curr_offset + 1) == ','))
        {
          /* Move past the , and the " */
          curr_offset+= 2;
          break;
        }
        if (curr_char == '\\' && curr_offset != (end_offset - 1))
        {
          curr_offset++;
          curr_char= file_buff->get_value(curr_offset);
          if (curr_char == 'r')
            buffer.append('\r');
          else if (curr_char == 'n' )
            buffer.append('\n');
          else if (curr_char == '\\' || curr_char == '"')
            buffer.append(curr_char);
          else  /* This could only happed with an externally created file */
          {
            buffer.append('\\');
            buffer.append(curr_char);
          }
        }
        else // ordinary symbol
        {
          /*
            If we are at final symbol and no last quote was found =>
            we are working with a damaged file.
          */
          if (curr_offset == end_offset - 1)
            goto err;
          buffer.append(curr_char);
        }
      }
    }
    else 
    {
      for ( ; curr_offset < end_offset; curr_offset++)
      {
        curr_char= file_buff->get_value(curr_offset);
        /* Move past the ,*/
        if (curr_char == ',')
        {
          curr_offset++;
          break;
        }
        if (curr_char == '\\' && curr_offset != (end_offset - 1))
        {
          curr_offset++;
          curr_char= file_buff->get_value(curr_offset);
          if (curr_char == 'r')
            buffer.append('\r');
          else if (curr_char == 'n' )
            buffer.append('\n');
          else if (curr_char == '\\' || curr_char == '"')
            buffer.append(curr_char);
          else  /* This could only happed with an externally created file */
          {
            buffer.append('\\');
            buffer.append(curr_char);
          }
        }
        else
        {
          /*
             We are at the final symbol and a quote was found for the
             unquoted field => We are working with a damaged field.
          */
          if (curr_offset == end_offset - 1 && curr_char == '"')
            goto err;
          buffer.append(curr_char);
        }
      }
    }

    if (read_all || bitmap_is_set(table->read_set, (*field)->field_index))
    {
      bool is_enum= ((*field)->real_type() ==  MYSQL_TYPE_ENUM);
      /*
        Here CHECK_FIELD_WARN checks that all values in the csv file are valid
        which is normally the case, if they were written  by
        INSERT -> ha_tina::write_row. '0' values on ENUM fields are considered
        invalid by Field_enum::store() but it can store them on INSERT anyway.
        Thus, for enums we silence the warning, as it doesn't really mean
        an invalid value.
      */
      if ((*field)->store(buffer.ptr(), buffer.length(), buffer.charset(),
                          is_enum ? CHECK_FIELD_IGNORE : CHECK_FIELD_WARN))
      {
        if (!is_enum)
          goto err;
      }
      if ((*field)->flags & BLOB_FLAG)
      {
        Field_blob *blob= *(Field_blob**) field;
        uchar *src, *tgt;
        uint length, packlength;

        packlength= blob->pack_length_no_ptr();
        length= blob->get_length(blob->ptr);
        memcpy(&src, blob->ptr + packlength, sizeof(char*));
        if (src)
        {
          tgt= (uchar*) alloc_root(&blobroot, length);
          memmove(tgt, src, length);
          memcpy(blob->ptr + packlength, &tgt, sizeof(char*));
        }
      }
    }
  }
  next_position= end_offset + eoln_len;
  error= 0;

err:
  dbug_tmp_restore_column_map(table->write_set, org_bitmap);

  DBUG_RETURN(error);
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
    mysql_mutex_lock(&share->mutex);
    local_saved_data_file_length= share->saved_data_file_length;
    mysql_mutex_unlock(&share->mutex);
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

  local_data_file_version= share->data_file_version;
  if ((data_file= mysql_file_open(csv_key_file_data,
                                  share->data_file_name,
                                  O_RDONLY, MYF(MY_WME))) == -1)
  {
    free_share(share);
    DBUG_RETURN(my_errno() ? my_errno() : -1);
  }

  /*
    Init locking. Pass handler object to the locking routines,
    so that they could save/update local_saved_data_file_length value
    during locking. This is needed to enable concurrent inserts.
  */
  thr_lock_data_init(&share->lock, &lock, (void*) this);
  ref_length= sizeof(my_off_t);

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
  rc= mysql_file_close(data_file, MYF(0));
  DBUG_RETURN(free_share(share) || rc);
}

/*
  This is an INSERT. At the moment this handler just seeks to the end
  of the file and appends the data. In an error case it really should
  just truncate to the original position (this is not done yet).
*/
int ha_tina::write_row(uchar * buf)
{
  int size;
  DBUG_ENTER("ha_tina::write_row");

  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  ha_statistic_increment(&SSV::ha_write_count);

  size= encode_quote(buf);

  if (!share->tina_write_opened)
    if (init_tina_writer())
      DBUG_RETURN(-1);

   /* use pwrite, as concurrent reader could have changed the position */
  if (mysql_file_write(share->tina_write_filedes, (uchar*)buffer.ptr(), size,
                       MYF(MY_WME | MY_NABP)))
    DBUG_RETURN(-1);

  /* update local copy of the max position to see our own changes */
  local_saved_data_file_length+= size;

  /* update shared info */
  mysql_mutex_lock(&share->mutex);
  share->rows_recorded++;
  /* update status for the log tables */
  if (share->is_log_table)
    update_status();
  mysql_mutex_unlock(&share->mutex);

  stats.records++;
  DBUG_RETURN(0);
}


int ha_tina::open_update_temp_file_if_needed()
{
  char updated_fname[FN_REFLEN];

  if (!share->update_file_opened)
  {
    if ((update_temp_file=
           mysql_file_create(csv_key_file_update,
                             fn_format(updated_fname, share->table_name,
                                       "", CSN_EXT,
                                       MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                             0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
      return 1;
    share->update_file_opened= TRUE;
    temp_file_length= 0;
  }
  return 0;
}

/*
  This is called for an update.
  Make sure you put in code to increment the auto increment.
  Currently auto increment is not being
  fixed since autoincrements have yet to be added to this table handler.
  This will be called in a table scan right before the previous ::rnd_next()
  call.
*/
int ha_tina::update_row(const uchar * old_data, uchar * new_data)
{
  int size;
  int rc= -1;
  DBUG_ENTER("ha_tina::update_row");

  ha_statistic_increment(&SSV::ha_update_count);

  size= encode_quote(new_data);

  /*
    During update we mark each updating record as deleted 
    (see the chain_append()) then write new one to the temporary data file. 
    At the end of the sequence in the rnd_end() we append all non-marked
    records from the data file to the temporary data file then rename it.
    The temp_file_length is used to calculate new data file length.
  */
  if (chain_append())
    goto err;

  if (open_update_temp_file_if_needed())
    goto err;

  if (mysql_file_write(update_temp_file, (uchar*)buffer.ptr(), size,
                       MYF(MY_WME | MY_NABP)))
    goto err;
  temp_file_length+= size;
  rc= 0;

  /* UPDATE should never happen on the log tables */
  DBUG_ASSERT(!share->is_log_table);

err:
  DBUG_PRINT("info",("rc = %d", rc));
  DBUG_RETURN(rc);
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
int ha_tina::delete_row(const uchar * buf)
{
  DBUG_ENTER("ha_tina::delete_row");
  ha_statistic_increment(&SSV::ha_delete_count);

  if (chain_append())
    DBUG_RETURN(-1);

  stats.records--;
  /* Update shared info */
  DBUG_ASSERT(share->rows_recorded);
  mysql_mutex_lock(&share->mutex);
  share->rows_recorded--;
  mysql_mutex_unlock(&share->mutex);

  /* DELETE should never happen on the log table */
  DBUG_ASSERT(!share->is_log_table);

  DBUG_RETURN(0);
}


/**
  @brief Initialize the data file.
  
  @details Compare the local version of the data file with the shared one.
  If they differ, there are some changes behind and we have to reopen
  the data file to make the changes visible.
  Call @c file_buff->init_buff() at the end to read the beginning of the 
  data file into buffer.
  
  @retval  0  OK.
  @retval  1  There was an error.
*/

int ha_tina::init_data_file()
{
  if (local_data_file_version != share->data_file_version)
  {
    local_data_file_version= share->data_file_version;
    if (mysql_file_close(data_file, MYF(0)) ||
        (data_file= mysql_file_open(csv_key_file_data,
                                    share->data_file_name, O_RDONLY,
                                    MYF(MY_WME))) == -1)
      return my_errno() ? my_errno() : -1;
  }
  file_buff->init_buff(data_file);
  return 0;
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
  if (share->crashed || init_data_file())
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
int ha_tina::rnd_next(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_tina::rnd_next");
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       TRUE);

  if (share->crashed)
  {
    rc= HA_ERR_CRASHED_ON_USAGE;
    goto end;
  }

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  current_position= next_position;

  /* don't scan an empty file */
  if (!local_saved_data_file_length)
  {
    rc= HA_ERR_END_OF_FILE;
    goto end;
  }

  if ((rc= find_current_row(buf)))
    goto end;

  stats.records++;
  rc= 0;
end:
  MYSQL_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
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
void ha_tina::position(const uchar *record)
{
  DBUG_ENTER("ha_tina::position");
  my_store_ptr(ref, ref_length, current_position);
  DBUG_VOID_RETURN;
}


/*
  Used to fetch a row from a posiion stored with ::position().
  my_get_ptr() retrieves the data for you.
*/

int ha_tina::rnd_pos(uchar * buf, uchar *pos)
{
  int rc;
  DBUG_ENTER("ha_tina::rnd_pos");
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       FALSE);
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  current_position= my_get_ptr(pos,ref_length);
  rc= find_current_row(buf);
  MYSQL_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
*/
int ha_tina::info(uint flag)
{
  DBUG_ENTER("ha_tina::info");
  /* This is a lie, but you don't want the optimizer to see zero or 1 */
  if (!records_is_known && stats.records < 2) 
    stats.records= 2;
  DBUG_RETURN(0);
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
   mysql_mutex_lock(&share->mutex);
   share->is_log_table= TRUE;
   mysql_mutex_unlock(&share->mutex);
 }
  DBUG_RETURN(0);
}


/*
  Set end_pos to the last valid byte of continuous area, closest
  to the given "hole", stored in the buffer. "Valid" here means,
  not listed in the chain of deleted records ("holes").
*/
bool ha_tina::get_write_pos(my_off_t *end_pos, tina_set *closest_hole)
{
  if (closest_hole == chain_ptr) /* no more chains */
    *end_pos= file_buff->end();
  else
    *end_pos= min(file_buff->end(), closest_hole->begin);
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
  my_off_t file_buffer_start= 0;
  DBUG_ENTER("ha_tina::rnd_end");

  free_root(&blobroot, MYF(0));
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
    my_qsort(chain, (size_t)(chain_ptr - chain), sizeof(tina_set),
             (qsort_cmp)sort_set);

    my_off_t write_begin= 0, write_end;

    /* create the file to write updated table if it wasn't yet created */
    if (open_update_temp_file_if_needed())
      DBUG_RETURN(-1);

    /* write the file with updated info */
    while ((file_buffer_start != (my_off_t)-1))     // while not end of file
    {
      bool in_hole= get_write_pos(&write_end, ptr);
      my_off_t write_length= write_end - write_begin;

      /* if there is something to write, write it */
      if (write_length)
      {
        if (mysql_file_write(update_temp_file,
                             (uchar*) (file_buff->ptr() +
                                       (write_begin - file_buff->start())),
                             (size_t)write_length, MYF_RW))
          goto error;
        temp_file_length+= write_length;
      }
      if (in_hole)
      {
        /* skip hole */
        while (file_buff->end() <= ptr->end &&
               file_buffer_start != (my_off_t)-1)
          file_buffer_start= file_buff->read_next();
        write_begin= ptr->end;
        ptr++;
      }
      else
        write_begin= write_end;

      if (write_end == file_buff->end())
        file_buffer_start= file_buff->read_next(); /* shift the buffer */

    }

    if (mysql_file_sync(update_temp_file, MYF(MY_WME)) ||
        mysql_file_close(update_temp_file, MYF(0)))
      DBUG_RETURN(-1);

    share->update_file_opened= FALSE;

    if (share->tina_write_opened)
    {
      if (mysql_file_close(share->tina_write_filedes, MYF(0)))
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
    if (mysql_file_close(data_file, MYF(0)) ||
        mysql_file_rename(csv_key_file_data,
                          fn_format(updated_fname, share->table_name,
                                    "", CSN_EXT,
                                    MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                          share->data_file_name, MYF(0)))
      DBUG_RETURN(-1);

    /* Open the file again */
    if ((data_file= mysql_file_open(csv_key_file_data,
                                    share->data_file_name,
                                    O_RDONLY, MYF(MY_WME))) == -1)
      DBUG_RETURN(my_errno() ? my_errno() : -1);
    /*
      As we reopened the data file, increase share->data_file_version 
      in order to force other threads waiting on a table lock and  
      have already opened the table to reopen the data file.
      That makes the latest changes become visible to them.
      Update local_data_file_version as no need to reopen it in the 
      current thread.
    */
    share->data_file_version++;
    local_data_file_version= share->data_file_version;
    /*
      The datafile is consistent at this point and the write filedes is
      closed, so nothing worrying will happen to it in case of a crash.
      Here we record this fact to the meta-file.
    */
    (void)write_meta_file(share->meta_file, share->rows_recorded, FALSE);
    /* 
      Update local_saved_data_file_length with the real length of the 
      data file.
    */
    local_saved_data_file_length= temp_file_length;
  }

  DBUG_RETURN(0);
error:
  mysql_file_close(update_temp_file, MYF(0));
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
  uchar *buf;
  File repair_file;
  int rc;
  ha_rows rows_repaired= 0;
  my_off_t write_begin= 0, write_end;
  DBUG_ENTER("ha_tina::repair");

  /* empty file */
  if (!share->saved_data_file_length)
  {
    share->rows_recorded= 0;
    goto end;
  }

  /* Don't assert in field::val() functions */
  table->use_all_columns();
  if (!(buf= (uchar*) my_malloc(csv_key_memory_row,
                                table->s->reclength, MYF(MY_WME))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  /* position buffer to the start of the file */
  if (init_data_file())
    DBUG_RETURN(HA_ERR_CRASHED_ON_REPAIR);

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
    thd_inc_row_count(thd);
    rows_repaired++;
    current_position= next_position;
  }

  free_root(&blobroot, MYF(0));

  my_free(buf);

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
  if ((repair_file= mysql_file_create(csv_key_file_update,
                                      fn_format(repaired_fname,
                                                share->table_name,
                                                "", CSN_EXT,
                                                MY_REPLACE_EXT|MY_UNPACK_FILENAME),
                                      0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
    DBUG_RETURN(HA_ERR_CRASHED_ON_REPAIR);

  file_buff->init_buff(data_file);


  /* we just truncated the file up to the first bad row. update rows count. */
  share->rows_recorded= rows_repaired;

  /* write repaired file */
  while (1)
  {
    write_end= min(file_buff->end(), current_position);
    if ((write_end - write_begin) &&
        (mysql_file_write(repair_file, (uchar*)file_buff->ptr(),
                          (size_t) (write_end - write_begin), MYF_RW)))
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
    when trying to delete the "to"-file in mysql_file_rename().
  */
  if (share->tina_write_opened)
  {
    /*
      Data file might be opened twice, on table opening stage and
      during write_row execution. We need to close both instances
      to satisfy Win.
    */
    if (mysql_file_close(share->tina_write_filedes, MYF(0)))
      DBUG_RETURN(my_errno() ? my_errno() : -1);
    share->tina_write_opened= FALSE;
  }
  if (mysql_file_close(data_file, MYF(0)) ||
      mysql_file_close(repair_file, MYF(0)) ||
      mysql_file_rename(csv_key_file_data,
                        repaired_fname, share->data_file_name, MYF(0)))
    DBUG_RETURN(-1);

  /* Open the file again, it should now be repaired */
  if ((data_file= mysql_file_open(csv_key_file_data,
                                  share->data_file_name, O_RDWR|O_APPEND,
                                  MYF(MY_WME))) == -1)
    DBUG_RETURN(my_errno() ? my_errno() : -1);

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
  {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    DBUG_RETURN(HA_ERR_WRONG_COMMAND);
  }

  if (!share->tina_write_opened)
    if (init_tina_writer())
      DBUG_RETURN(-1);

  /* Truncate the file to zero size */
  rc= mysql_file_chsize(share->tina_write_filedes, 0, 0, MYF(MY_WME));

  stats.records=0;
  /* Update shared info */
  mysql_mutex_lock(&share->mutex);
  share->rows_recorded= 0;
  mysql_mutex_unlock(&share->mutex);
  local_saved_data_file_length= 0;
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

  /*
    check columns
  */
  for (Field **field= table_arg->s->field; *field; field++)
  {
    if ((*field)->real_maybe_null())
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "nullable columns");
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
    }
  }
  

  if ((create_file= mysql_file_create(csv_key_file_metadata,
                                      fn_format(name_buff, name, "", CSM_EXT,
                                                MY_REPLACE_EXT|MY_UNPACK_FILENAME),
                                      0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
    DBUG_RETURN(-1);

  write_meta_file(create_file, 0, FALSE);
  mysql_file_close(create_file, MYF(0));

  if ((create_file= mysql_file_create(csv_key_file_data,
                                      fn_format(name_buff, name, "", CSV_EXT,
                                                MY_REPLACE_EXT|MY_UNPACK_FILENAME),
                                      0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
    DBUG_RETURN(-1);

  mysql_file_close(create_file, MYF(0));

  DBUG_RETURN(0);
}

int ha_tina::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  int rc= 0;
  uchar *buf;
  const char *old_proc_info;
  ha_rows count= share->rows_recorded;
  DBUG_ENTER("ha_tina::check");

  old_proc_info= thd_proc_info(thd, "Checking table");
  if (!(buf= (uchar*) my_malloc(csv_key_memory_row,
                                table->s->reclength, MYF(MY_WME))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  /* position buffer to the start of the file */
   if (init_data_file())
     DBUG_RETURN(HA_ERR_CRASHED);

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
    thd_inc_row_count(thd);
    count--;
    current_position= next_position;
  }

  free_root(&blobroot, MYF(0));

  my_free(buf);
  thd_proc_info(thd, old_proc_info);

  if ((rc != HA_ERR_END_OF_FILE) || count)
  {
    share->crashed= TRUE;
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  }

  DBUG_RETURN(HA_ADMIN_OK);
}


bool ha_tina::check_if_incompatible_data(HA_CREATE_INFO *info,
					   uint table_changes)
{
  return COMPATIBLE_DATA_YES;
}

struct st_mysql_storage_engine csv_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(csv)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &csv_storage_engine,
  "CSV",
  "Brian Aker, MySQL AB",
  "CSV storage engine",
  PLUGIN_LICENSE_GPL,
  tina_init_func, /* Plugin Init */
  tina_done_func, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;

