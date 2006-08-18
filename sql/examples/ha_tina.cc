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

  First off, this is a play thing for me, there are a number of things wrong with it:
 *) It was designed for csv and therefor its performance is highly questionable.
 *) Indexes have not been implemented. This is because the files can be traded in
 and out of the table directory without having to worry about rebuilding anything.
 *) NULLs and "" are treated equally (like a spreadsheet).
 *) There was in the beginning no point to anyone seeing this other then me, so there
 is a good chance that I haven't quite documented it well.
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

#ifdef HAVE_CSV_DB

#include "ha_tina.h"
#include <sys/mman.h>

/* Stuff for shares */
pthread_mutex_t tina_mutex;
static HASH tina_open_tables;
static int tina_init= 0;

handlerton tina_hton= {
  "CSV",
  SHOW_OPTION_YES,
  "CSV storage engine", 
  DB_TYPE_CSV_DB,
  NULL,    /* One needs to be written! */
  0,       /* slot */
  0,       /* savepoint size. */
  NULL,    /* close_connection */
  NULL,    /* savepoint */
  NULL,    /* rollback to savepoint */
  NULL,    /* release savepoint */
  NULL,    /* commit */
  NULL,    /* rollback */
  NULL,    /* prepare */
  NULL,    /* recover */
  NULL,    /* commit_by_xid */
  NULL,    /* rollback_by_xid */
  NULL,    /* create_cursor_read_view */
  NULL,    /* set_cursor_read_view */
  NULL,    /* close_cursor_read_view */
  HTON_CAN_RECREATE
};

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
  return ( a->begin > b->begin ? -1 : ( a->begin < b->begin ? 1 : 0 ) );
}

static byte* tina_get_key(TINA_SHARE *share,uint *length,
                          my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}


int free_mmap(TINA_SHARE *share)
{
  DBUG_ENTER("ha_tina::free_mmap");
  if (share->mapped_file)
  {
    /*
      Invalidate the mapped in pages. Some operating systems (eg OpenBSD)
      would reuse already cached pages even if the file has been altered
      using fd based I/O. This may be optimized by perhaps only invalidating
      the last page but optimization of deprecated code is not important.
    */
    msync(share->mapped_file, 0, MS_INVALIDATE);
    if (munmap(share->mapped_file, share->file_stat.st_size))
      DBUG_RETURN(1);
  }
  share->mapped_file= NULL;
  DBUG_RETURN(0);
}

/*
  Reloads the mmap file.
*/
int get_mmap(TINA_SHARE *share, int write)
{
  DBUG_ENTER("ha_tina::get_mmap");
  
  if (free_mmap(share))
    DBUG_RETURN(1);

  if (my_fstat(share->data_file, &share->file_stat, MYF(MY_WME)) == -1)
    DBUG_RETURN(1);

  if (share->file_stat.st_size) 
  {
    if (write)
      share->mapped_file= (byte *)mmap(NULL, share->file_stat.st_size, 
                                       PROT_READ|PROT_WRITE, MAP_SHARED,
                                       share->data_file, 0);
    else
      share->mapped_file= (byte *)mmap(NULL, share->file_stat.st_size, 
                                       PROT_READ, MAP_PRIVATE,
                                       share->data_file, 0);
    if ((share->mapped_file ==(caddr_t)-1)) 
    {
      /*
        Bad idea you think? See the problem is that nothing actually checks
        the return value of ::rnd_init(), so tossing an error is about
        it for us.
        Never going to happen right? :)
      */
      my_message(errno, "Woops, blew up opening a mapped file", 0);
      DBUG_ASSERT(0);
      DBUG_RETURN(1);
    }
  }
  else 
    share->mapped_file= NULL;

  DBUG_RETURN(0);
}

/*
  Simple lock controls.
*/
static TINA_SHARE *get_share(const char *table_name, TABLE *table)
{
  TINA_SHARE *share;
  char *tmp_name;
  uint length;

  if (!tina_init)
  {
    /* Hijack a mutex for init'ing the storage engine */
    pthread_mutex_lock(&LOCK_mysql_create_db);
    if (!tina_init)
    {
      tina_init++;
      VOID(pthread_mutex_init(&tina_mutex,MY_MUTEX_INIT_FAST));
      (void) hash_init(&tina_open_tables,system_charset_info,32,0,0,
                       (hash_get_key) tina_get_key,0,0);
    }
    pthread_mutex_unlock(&LOCK_mysql_create_db);
  }
  pthread_mutex_lock(&tina_mutex);
  length=(uint) strlen(table_name);
  if (!(share=(TINA_SHARE*) hash_search(&tina_open_tables,
                                        (byte*) table_name,
                                        length)))
  {
    char data_file_name[FN_REFLEN];
    if (!my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                         &share, sizeof(*share),
                         &tmp_name, length+1,
                         NullS)) 
    {
      pthread_mutex_unlock(&tina_mutex);
      return NULL;
    }

    share->use_count=0;
    share->table_name_length=length;
    share->table_name=tmp_name;
    strmov(share->table_name,table_name);
    fn_format(data_file_name, table_name, "", ".CSV",
              MY_REPLACE_EXT | MY_UNPACK_FILENAME);

    if ((share->data_file= my_open(data_file_name, O_RDWR|O_APPEND,
                                   MYF(0))) == -1)
      goto error;

    if (my_hash_insert(&tina_open_tables, (byte*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);

    /* We only use share->data_file for writing, so we scan to the end to append */
    if (my_seek(share->data_file, 0, SEEK_END, MYF(0)) == MY_FILEPOS_ERROR)
      goto error2;

    share->mapped_file= NULL; // We don't know the state since we just allocated it
    if (get_mmap(share, 0) > 0)
      goto error3;
  }
  share->use_count++;
  pthread_mutex_unlock(&tina_mutex);

  return share;

error3:
  my_close(share->data_file,MYF(0));
error2:
  thr_lock_delete(&share->lock);
  pthread_mutex_destroy(&share->mutex);
  hash_delete(&tina_open_tables, (byte*) share);
error:
  pthread_mutex_unlock(&tina_mutex);
  my_free((gptr) share, MYF(0));

  return NULL;
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
    /* Drop the mapped file */
    free_mmap(share);
    result_code= my_close(share->data_file,MYF(0));
    hash_delete(&tina_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&tina_mutex);

  DBUG_RETURN(result_code);
}

bool tina_end()
{
  if (tina_init)
  {
    hash_free(&tina_open_tables);
    VOID(pthread_mutex_destroy(&tina_mutex));
  }
  tina_init= 0;
  return FALSE;
}

/* 
  Finds the end of a line.
  Currently only supports files written on a UNIX OS.
*/
byte * find_eoln(byte *data, off_t begin, off_t end) 
{
  for (off_t x= begin; x < end; x++) 
    if (data[x] == '\n')
      return data + x;

  return 0;
}


ha_tina::ha_tina(TABLE *table_arg)
  :handler(&tina_hton, table_arg),
  /*
    These definitions are found in hanler.h
    These are not probably completely right.
  */
  current_position(0), next_position(0), chain_alloced(0),
  chain_size(DEFAULT_CHAIN_LENGTH), records_is_known(0)
{
  /* Set our original buffers from pre-allocated memory */
  buffer.set(byte_buffer, IO_SIZE, system_charset_info);
  chain= chain_buffer;
}

/*
  Encode a buffer into the quoted format.
*/
int ha_tina::encode_quote(byte *buf) 
{
  char attribute_buffer[1024];
  String attribute(attribute_buffer, sizeof(attribute_buffer), &my_charset_bin);

  buffer.length(0);
  for (Field **field=table->field ; *field ; field++)
  {
    const char *ptr;
    const char *end_ptr;

    (*field)->val_str(&attribute,&attribute);
    ptr= attribute.ptr();
    end_ptr= attribute.length() + ptr;

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

  return (buffer.length());
}

/*
  chain_append() adds delete positions to the chain that we use to keep track of space.
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
        if ((chain= (tina_set *)my_realloc((gptr)chain,chain_size,MYF(MY_WME))) == NULL)
          return -1;
      }
      else
      {
        tina_set *ptr= (tina_set *)my_malloc(chain_size * sizeof(tina_set),MYF(MY_WME));
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
  byte *mapped_ptr= (byte *)share->mapped_file + current_position;
  byte *end_ptr;
  DBUG_ENTER("ha_tina::find_current_row");

  /* EOF should be counted as new line */
  if ((end_ptr=  find_eoln(share->mapped_file, current_position, share->file_stat.st_size)) == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  for (Field **field=table->field ; *field ; field++)
  {
    buffer.length(0);
    mapped_ptr++; // Increment past the first quote
    for(;mapped_ptr != end_ptr; mapped_ptr++)
    {
      //Need to convert line feeds!
      if (*mapped_ptr == '"' && 
          (((mapped_ptr[1] == ',') && (mapped_ptr[2] == '"')) || (mapped_ptr == end_ptr -1 )))
      {
        mapped_ptr += 2; // Move past the , and the "
        break;
      } 
      if (*mapped_ptr == '\\' && mapped_ptr != (end_ptr - 1)) 
      {
        mapped_ptr++;
        if (*mapped_ptr == 'r')
          buffer.append('\r');
        else if (*mapped_ptr == 'n' )
          buffer.append('\n');
        else if ((*mapped_ptr == '\\') || (*mapped_ptr == '"'))
          buffer.append(*mapped_ptr);
        else  /* This could only happed with an externally created file */
        {
          buffer.append('\\');
          buffer.append(*mapped_ptr);
        }
      } 
      else
        buffer.append(*mapped_ptr);
    }
    (*field)->store(buffer.ptr(), buffer.length(), system_charset_info);
  }
  next_position= (end_ptr - share->mapped_file)+1;
  /* Maybe use \N for null? */
  memset(buf, 0, table->s->null_bytes); /* We do not implement nulls! */

  DBUG_RETURN(0);
}

/*
  If frm_error() is called in table.cc this is called to find out what file
  extensions exist for this handler.
*/
static const char *ha_tina_exts[] = {
  ".CSV",
  NullS
};

const char **ha_tina::bas_ext() const
{
  return ha_tina_exts;
}


/* 
  Open a database file. Keep in mind that tables are caches, so
  this will not be called for every request. Any sort of positions
  that need to be reset should be kept in the ::extra() call.
*/
int ha_tina::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_tina::open");

  if (!(share= get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock,&lock,NULL);
  ref_length=sizeof(off_t);

  DBUG_RETURN(0);
}


/*
  Close a database file. We remove ourselves from the shared strucutre.
  If it is empty we destroy it and free the mapped file.
*/
int ha_tina::close(void)
{
  DBUG_ENTER("ha_tina::close");
  DBUG_RETURN(free_share(share));
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

  statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);

  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  size= encode_quote(buf);

  /*
    we are going to alter the file so we must invalidate the in memory pages
    otherwise we risk a race between the in memory pages and the disk pages.
  */
  if (free_mmap(share))
    DBUG_RETURN(-1);

  if (my_write(share->data_file, buffer.ptr(), size, MYF(MY_WME | MY_NABP)))
    DBUG_RETURN(-1);

  /* 
    Ok, this is means that we will be doing potentially bad things 
    during a bulk insert on some OS'es. What we need is a cleanup
    call for ::write_row that would let us fix up everything after the bulk
    insert. The archive handler does this with an extra mutx call, which
    might be a solution for this.
  */
  if (get_mmap(share, 0) > 0) 
    DBUG_RETURN(-1);
  records++;
  DBUG_RETURN(0);
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

  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);

  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();

  size= encode_quote(new_data);

  if (chain_append())
    DBUG_RETURN(-1);

  /*
    we are going to alter the file so we must invalidate the in memory pages
    otherwise we risk a race between the in memory pages and the disk pages.
  */
  if (free_mmap(share))
    DBUG_RETURN(-1);

  if (my_write(share->data_file, buffer.ptr(), size, MYF(MY_WME | MY_NABP)))
    DBUG_RETURN(-1);

  /* 
    Ok, this is means that we will be doing potentially bad things 
    during a bulk update on some OS'es. Ideally, we should extend the length
    of the file, redo the mmap and then write all the updated rows. Upon
    finishing the bulk update, truncate the file length to the final length.
    Since this code is all being deprecated, not point now to optimize.
  */
  if (get_mmap(share, 0) > 0) 
    DBUG_RETURN(-1);

  DBUG_RETURN(0);
}


/* 
  Deletes a row. First the database will find the row, and then call this method.
  In the case of a table scan, the previous call to this will be the ::rnd_next()
  that found this row.
  The exception to this is an ORDER BY. This will cause the table handler to walk
  the table noting the positions of all rows that match a query. The table will
  then be deleted/positioned based on the ORDER (so RANDOM, DESC, ASC).
*/
int ha_tina::delete_row(const byte * buf)
{
  DBUG_ENTER("ha_tina::delete_row");
  statistic_increment(table->in_use->status_var.ha_delete_count,&LOCK_status);

  if (chain_append())
    DBUG_RETURN(-1);

  --records;

  DBUG_RETURN(0);
}

/*
  Fill buf with value from key. Simply this is used for a single index read 
  with a key.
*/
int ha_tina::index_read(byte * buf, const byte * key,
                        uint key_len __attribute__((unused)),
                        enum ha_rkey_function find_flag
                        __attribute__((unused)))
{
  DBUG_ENTER("ha_tina::index_read");
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
}

/*
  Fill buf with value from key. Simply this is used for a single index read 
  with a key.
  Whatever the current key is we will use it. This is what will be in "index".
*/
int ha_tina::index_read_idx(byte * buf, uint index, const byte * key,
                            uint key_len __attribute__((unused)),
                            enum ha_rkey_function find_flag
                            __attribute__((unused)))
{
  DBUG_ENTER("ha_tina::index_read_idx");
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
}


/*
  Read the next position in the index.
*/
int ha_tina::index_next(byte * buf)
{
  DBUG_ENTER("ha_tina::index_next");
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
}

/*
  Read the previous position in the index.
*/
int ha_tina::index_prev(byte * buf)
{
  DBUG_ENTER("ha_tina::index_prev");
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
}

/*
  Read the first position in the index
*/
int ha_tina::index_first(byte * buf)
{
  DBUG_ENTER("ha_tina::index_first");
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
}

/*
  Read the last position in the index
  With this we don't need to do a filesort() with index.
  We just read the last row and call previous.
*/
int ha_tina::index_last(byte * buf)
{
  DBUG_ENTER("ha_tina::index_last");
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
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

  current_position= next_position= 0;
  records= 0;
  records_is_known= 0;
  chain_ptr= chain;
#ifdef HAVE_MADVISE
  if (scan)
    (void)madvise(share->mapped_file,share->file_stat.st_size,MADV_SEQUENTIAL);
#endif

  DBUG_RETURN(0);
}

/*
  ::rnd_next() does all the heavy lifting for a table scan. You will need to populate *buf
  with the correct field data. You can walk the field to determine at what position you 
  should store the data (take a look at how ::find_current_row() works). The structure
  is something like:
  0Foo  Dog  Friend
  The first offset is for the first attribute. All space before that is reserved for null count.
  Basically this works as a mask for which rows are nulled (compared to just empty).
  This table handler doesn't do nulls and does not know the difference between NULL and "". This
  is ok since this table handler is for spreadsheets and they don't know about them either :)
*/
int ha_tina::rnd_next(byte *buf)
{
  DBUG_ENTER("ha_tina::rnd_next");

  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);

  current_position= next_position;
  if (!share->mapped_file) 
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  if (HA_ERR_END_OF_FILE == find_current_row(buf) ) 
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  records++;
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
  statistic_increment(table->in_use->status_var.ha_read_rnd_next_count,
		      &LOCK_status);
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
  if (!records_is_known && records < 2) 
    records= 2;
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
  DBUG_RETURN(0);
}

/* 
  This is no longer used.
*/
int ha_tina::reset(void)
{
  DBUG_ENTER("ha_tina::reset");
  ha_tina::extra(HA_EXTRA_RESET);
  DBUG_RETURN(0);
}


/*
  Called after deletes, inserts, and updates. This is where we clean up all of
  the dead space we have collected while writing the file. 
*/
int ha_tina::rnd_end()
{
  DBUG_ENTER("ha_tina::rnd_end");

  records_is_known= 1;

  /* First position will be truncate position, second will be increment */
  if ((chain_ptr - chain)  > 0)
  {
    tina_set *ptr;
    off_t length;

    /* 
      Setting up writable map, this will contain all of the data after the
      get_mmap call that we have added to the file.
    */
    if (get_mmap(share, 1) > 0) 
      DBUG_RETURN(-1);
    length= share->file_stat.st_size;

    /*
      The sort handles updates/deletes with random orders.
      It also sorts so that we move the final blocks to the
      beginning so that we move the smallest amount of data possible.
    */
    qsort(chain, (size_t)(chain_ptr - chain), sizeof(tina_set), (qsort_cmp)sort_set);
    for (ptr= chain; ptr < chain_ptr; ptr++)
    {
      memmove(share->mapped_file + ptr->begin, share->mapped_file + ptr->end,
              length - (size_t)ptr->end);
      length= length - (size_t)(ptr->end - ptr->begin);
    }

    /* Invalidate all cached mmap pages */
    if (free_mmap(share))
      DBUG_RETURN(-1);

    /* Truncate the file to the new size */
    if (my_chsize(share->data_file, length, 0, MYF(MY_WME)))
      DBUG_RETURN(-1);

    if (get_mmap(share, 0) > 0) 
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/* 
  DELETE without WHERE calls it
*/
int ha_tina::delete_all_rows()
{
  DBUG_ENTER("ha_tina::delete_all_rows");

  if (!records_is_known)
    return (my_errno=HA_ERR_WRONG_COMMAND);

  /* Invalidate all cached mmap pages */
  if (free_mmap(share)) 
    DBUG_RETURN(-1);

  int rc= my_chsize(share->data_file, 0, 0, MYF(MY_WME));

  if (get_mmap(share, 0) > 0) 
    DBUG_RETURN(-1);

  records=0;
  DBUG_RETURN(rc);
}

/*
  Always called by the start of a transaction (or by "lock tables");
*/
int ha_tina::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_tina::external_lock");
  DBUG_RETURN(0);          // No external locking
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

int ha_tina::create(const char *name, TABLE *table_arg, HA_CREATE_INFO *create_info)
{
  char name_buff[FN_REFLEN];
  File create_file;
  DBUG_ENTER("ha_tina::create");

  if ((create_file= my_create(fn_format(name_buff,name,"",".CSV",MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                              O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    DBUG_RETURN(-1);

  my_close(create_file,MYF(0));

  DBUG_RETURN(0);
}

#endif /* enable CSV */
