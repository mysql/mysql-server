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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation        // gcc: Class implementation
#endif

#include "../mysql_priv.h"

#ifdef HAVE_ARCHIVE_DB
#include "ha_archive.h"

/*
  First, if you want to understand storage engines you should look at 
  ha_example.cc and ha_example.h. 
  This example was written as a test case for a customer who needed
  a storage engine without indexes that could compress data very well.
  So, welcome to a completely compressed storage engine. This storage
  engine only does inserts. No replace, deletes, or updates. All reads are 
  complete table scans. Compression is done through gzip (bzip compresses
  better, but only marginally, if someone asks I could add support for
  it too, but beaware that it costs a lot more in CPU time then gzip).
  
  We keep a file pointer open for each instance of ha_archive for each read
  but for writes we keep one open file handle just for that. We flush it
  only if we have a read occur. gzip handles compressing lots of records
  at once much better then doing lots of little records between writes.
  It is possible to not lock on writes but this would then mean we couldn't
  handle bulk inserts as well (that is if someone was trying to read at
  the same time since we would want to flush).

  A "meta" file is kept alongside the data file. This file serves two purpose.
  The first purpose is to track the number of rows in the table. The second 
  purpose is to determine if the table was closed properly or not. When the 
  meta file is first opened it is marked as dirty. It is opened when the table 
  itself is opened for writing. When the table is closed the new count for rows 
  is written to the meta file and the file is marked as clean. If the meta file 
  is opened and it is marked as dirty, it is assumed that a crash occured. At 
  this point an error occurs and the user is told to rebuild the file.
  A rebuild scans the rows and rewrites the meta file. If corruption is found
  in the data file then the meta file is not repaired.

  At some point a recovery method for such a drastic case needs to be divised.

  Locks are row level, and you will get a consistant read. 

  For performance as far as table scans go it is quite fast. I don't have
  good numbers but locally it has out performed both Innodb and MyISAM. For
  Innodb the question will be if the table can be fit into the buffer
  pool. For MyISAM its a question of how much the file system caches the
  MyISAM file. With enough free memory MyISAM is faster. Its only when the OS
  doesn't have enough memory to cache entire table that archive turns out 
  to be any faster. For writes it is always a bit slower then MyISAM. It has no
  internal limits though for row length.

  Examples between MyISAM (packed) and Archive.

  Table with 76695844 identical rows:
  29680807 a_archive.ARZ
  920350317 a.MYD


  Table with 8991478 rows (all of Slashdot's comments):
  1922964506 comment_archive.ARZ
  2944970297 comment_text.MYD


  TODO:
   Add bzip optional support.
   Allow users to set compression level.
   Add truncate table command.
   Implement versioning, should be easy.
   Allow for errors, find a way to mark bad rows.
   Talk to the gzip guys, come up with a writable format so that updates are doable
     without switching to a block method.
   Add optional feature so that rows can be flushed at interval (which will cause less
     compression but may speed up ordered searches).
   Checkpoint the meta file to allow for faster rebuilds.
   Dirty open (right now the meta file is repaired if a crash occured).
   Option to allow for dirty reads, this would lower the sync calls, which would make
     inserts a lot faster, but would mean highly arbitrary reads.

    -Brian
*/
/*
  Notes on file formats.
  The Meta file is layed out as:
  check - Just an int of 254 to make sure that the the file we are opening was
          never corrupted.
  version - The current version of the file format.
  rows - This is an unsigned long long which is the number of rows in the data
         file.
  check point - Reserved for future use
  dirty - Status of the file, whether or not its values are the latest. This
          flag is what causes a repair to occur

  The data file:
  check - Just an int of 254 to make sure that the the file we are opening was
          never corrupted.
  version - The current version of the file format.
  data - The data is stored in a "row +blobs" format.
*/

/* If the archive storage engine has been inited */
static bool archive_inited= 0;
/* Variables for archive share methods */
pthread_mutex_t archive_mutex;
static HASH archive_open_tables;

/* The file extension */
#define ARZ ".ARZ"               // The data file
#define ARN ".ARN"               // Files used during an optimize call
#define ARM ".ARM"               // Meta file
/*
  uchar + uchar + ulonglong + ulonglong + uchar
*/
#define META_BUFFER_SIZE 19      // Size of the data used in the meta file
/*
  uchar + uchar
*/
#define DATA_BUFFER_SIZE 2       // Size of the data used in the data file
#define ARCHIVE_CHECK_HEADER 254 // The number we use to determine corruption

/*
  Used for hash table that tracks open tables.
*/
static byte* archive_get_key(ARCHIVE_SHARE *share,uint *length,
                             my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}


/*
  Initialize the archive handler.

  SYNOPSIS
    archive_db_init()
    void

  RETURN
    FALSE       OK
    TRUE        Error
*/

bool archive_db_init()
{
  archive_inited= 1;
  VOID(pthread_mutex_init(&archive_mutex, MY_MUTEX_INIT_FAST));
  return (hash_init(&archive_open_tables, system_charset_info, 32, 0, 0,
                    (hash_get_key) archive_get_key, 0, 0));
}


/*
  Release the archive handler.

  SYNOPSIS
    archive_db_end()
    void

  RETURN
    FALSE       OK
*/

bool archive_db_end()
{
  if (archive_inited)
  {
    hash_free(&archive_open_tables);
    VOID(pthread_mutex_destroy(&archive_mutex));
  }
  archive_inited= 0;
  return FALSE;
}


/*
  This method reads the header of a datafile and returns whether or not it was successful.
*/
int ha_archive::read_data_header(gzFile file_to_read)
{
  uchar data_buffer[DATA_BUFFER_SIZE];
  DBUG_ENTER("ha_archive::read_data_header");

  if (gzrewind(file_to_read) == -1)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  if (gzread(file_to_read, data_buffer, DATA_BUFFER_SIZE) != DATA_BUFFER_SIZE)
    DBUG_RETURN(errno ? errno : -1);
  
  DBUG_PRINT("ha_archive::read_data_header", ("Check %u", data_buffer[0]));
  DBUG_PRINT("ha_archive::read_data_header", ("Version %u", data_buffer[1]));
  
  if ((data_buffer[0] != (uchar)ARCHIVE_CHECK_HEADER) &&  
      (data_buffer[1] != (uchar)ARCHIVE_VERSION))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  DBUG_RETURN(0);
}

/*
  This method writes out the header of a datafile and returns whether or not it was successful.
*/
int ha_archive::write_data_header(gzFile file_to_write)
{
  uchar data_buffer[DATA_BUFFER_SIZE];
  DBUG_ENTER("ha_archive::write_data_header");

  data_buffer[0]= (uchar)ARCHIVE_CHECK_HEADER;
  data_buffer[1]= (uchar)ARCHIVE_VERSION;

  if (gzwrite(file_to_write, &data_buffer, DATA_BUFFER_SIZE) != 
      DATA_BUFFER_SIZE)
    goto error;
  DBUG_PRINT("ha_archive::write_data_header", ("Check %u", (uint)data_buffer[0]));
  DBUG_PRINT("ha_archive::write_data_header", ("Version %u", (uint)data_buffer[1]));

  DBUG_RETURN(0);
error:
  DBUG_RETURN(errno);
}

/*
  This method reads the header of a meta file and returns whether or not it was successful.
  *rows will contain the current number of rows in the data file upon success.
*/
int ha_archive::read_meta_file(File meta_file, ulonglong *rows)
{
  uchar meta_buffer[META_BUFFER_SIZE];
  ulonglong check_point;

  DBUG_ENTER("ha_archive::read_meta_file");

  VOID(my_seek(meta_file, 0, MY_SEEK_SET, MYF(0)));
  if (my_read(meta_file, (byte*)meta_buffer, META_BUFFER_SIZE, 0) != META_BUFFER_SIZE)
    DBUG_RETURN(-1);
  
  /*
    Parse out the meta data, we ignore version at the moment
  */
  *rows= uint8korr(meta_buffer + 2);
  check_point= uint8korr(meta_buffer + 10);

  DBUG_PRINT("ha_archive::read_meta_file", ("Check %d", (uint)meta_buffer[0]));
  DBUG_PRINT("ha_archive::read_meta_file", ("Version %d", (uint)meta_buffer[1]));
  DBUG_PRINT("ha_archive::read_meta_file", ("Rows %lld", *rows));
  DBUG_PRINT("ha_archive::read_meta_file", ("Checkpoint %lld", check_point));
  DBUG_PRINT("ha_archive::read_meta_file", ("Dirty %d", (int)meta_buffer[18]));

  if ((meta_buffer[0] != (uchar)ARCHIVE_CHECK_HEADER) || 
      ((bool)meta_buffer[18] == TRUE))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  my_sync(meta_file, MYF(MY_WME));

  DBUG_RETURN(0);
}

/*
  This method writes out the header of a meta file and returns whether or not it was successful.
  By setting dirty you say whether or not the file represents the actual state of the data file.
  Upon ::open() we set to dirty, and upon ::close() we set to clean. If we determine during
  a read that the file was dirty we will force a rebuild of this file.
*/
int ha_archive::write_meta_file(File meta_file, ulonglong rows, bool dirty)
{
  uchar meta_buffer[META_BUFFER_SIZE];
  ulonglong check_point= 0; //Reserved for the future

  DBUG_ENTER("ha_archive::write_meta_file");

  meta_buffer[0]= (uchar)ARCHIVE_CHECK_HEADER;
  meta_buffer[1]= (uchar)ARCHIVE_VERSION;
  int8store(meta_buffer + 2, rows); 
  int8store(meta_buffer + 10, check_point); 
  *(meta_buffer + 18)= (uchar)dirty;
  DBUG_PRINT("ha_archive::write_meta_file", ("Check %d", (uint)ARCHIVE_CHECK_HEADER));
  DBUG_PRINT("ha_archive::write_meta_file", ("Version %d", (uint)ARCHIVE_VERSION));
  DBUG_PRINT("ha_archive::write_meta_file", ("Rows %llu", rows));
  DBUG_PRINT("ha_archive::write_meta_file", ("Checkpoint %llu", check_point));
  DBUG_PRINT("ha_archive::write_meta_file", ("Dirty %d", (uint)dirty));

  VOID(my_seek(meta_file, 0, MY_SEEK_SET, MYF(0)));
  if (my_write(meta_file, (byte *)meta_buffer, META_BUFFER_SIZE, 0) != META_BUFFER_SIZE)
    DBUG_RETURN(-1);
  
  my_sync(meta_file, MYF(MY_WME));

  DBUG_RETURN(0);
}


/*
  We create the shared memory space that we will use for the open table. 
  See ha_example.cc for a longer description.
*/
ARCHIVE_SHARE *ha_archive::get_share(const char *table_name, TABLE *table)
{
  ARCHIVE_SHARE *share;
  char meta_file_name[FN_REFLEN];
  uint length;
  char *tmp_name;

  pthread_mutex_lock(&archive_mutex);
  length=(uint) strlen(table_name);

  if (!(share=(ARCHIVE_SHARE*) hash_search(&archive_open_tables,
                                           (byte*) table_name,
                                           length)))
  {
    if (!my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                          &share, sizeof(*share),
                          &tmp_name, length+1,
                          NullS)) 
    {
      pthread_mutex_unlock(&archive_mutex);
      return NULL;
    }

    share->use_count= 0;
    share->table_name_length= length;
    share->table_name= tmp_name;
    fn_format(share->data_file_name,table_name,"",ARZ,MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    fn_format(meta_file_name,table_name,"",ARM,MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    strmov(share->table_name,table_name);
    /*
      We will use this lock for rows.
    */
    VOID(pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST));
    if ((share->meta_file= my_open(meta_file_name, O_RDWR, MYF(0))) == -1)
      goto error;
    
    if (read_meta_file(share->meta_file, &share->rows_recorded))
    {
      /*
        The problem here is that for some reason, probably a crash, the meta
        file has been corrupted. So what do we do? Well we try to rebuild it
        ourself. Once that happens, we reread it, but if that fails we just
        call it quits and return an error.
      */
      if (rebuild_meta_file(share->table_name, share->meta_file))
        goto error;
      if (read_meta_file(share->meta_file, &share->rows_recorded))
        goto error;
    }
    /*
      After we read, we set the file to dirty. When we close, we will do the 
      opposite.
    */
    (void)write_meta_file(share->meta_file, share->rows_recorded, TRUE);

    /* 
      It is expensive to open and close the data files and since you can't have
      a gzip file that can be both read and written we keep a writer open
      that is shared amoung all open tables.
    */
    if ((share->archive_write= gzopen(share->data_file_name, "ab")) == NULL)
      goto error2;
    if (my_hash_insert(&archive_open_tables, (byte*) share))
      goto error3;
    thr_lock_init(&share->lock);
  }
  share->use_count++;
  pthread_mutex_unlock(&archive_mutex);

  return share;

error3:
  /* We close, but ignore errors since we already have errors */
  (void)gzclose(share->archive_write);
error2:
  my_close(share->meta_file,MYF(0));
error:
  pthread_mutex_unlock(&archive_mutex);
  VOID(pthread_mutex_destroy(&share->mutex));
  my_free((gptr) share, MYF(0));

  return NULL;
}


/* 
  Free the share.
  See ha_example.cc for a description.
*/
int ha_archive::free_share(ARCHIVE_SHARE *share)
{
  int rc= 0;
  pthread_mutex_lock(&archive_mutex);
  if (!--share->use_count)
  {
    hash_delete(&archive_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    VOID(pthread_mutex_destroy(&share->mutex));
    (void)write_meta_file(share->meta_file, share->rows_recorded, FALSE);
    if (gzclose(share->archive_write) == Z_ERRNO)
      rc= 1;
    if (my_close(share->meta_file, MYF(0)))
      rc= 1;
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&archive_mutex);

  return rc;
}


/* 
  We just implement one additional file extension.
*/
const char **ha_archive::bas_ext() const
{ static const char *ext[]= { ARZ, ARN, ARM, NullS }; return ext; }


/* 
  When opening a file we:
  Create/get our shared structure.
  Init out lock.
  We open the file we will read from.
*/
int ha_archive::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_archive::open");

  if (!(share= get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock,&lock,NULL);

  if ((archive= gzopen(share->data_file_name, "rb")) == NULL)
  {
    (void)free_share(share); //We void since we already have an error
    DBUG_RETURN(errno ? errno : -1);
  }

  DBUG_RETURN(0);
}


/*
  Closes the file.

  SYNOPSIS
    close();
  
  IMPLEMENTATION:

  We first close this storage engines file handle to the archive and
  then remove our reference count to the table (and possibly free it
  as well).

  RETURN
    0  ok
    1  Error
*/

int ha_archive::close(void)
{
  int rc= 0;
  DBUG_ENTER("ha_archive::close");

  /* First close stream */
  if (gzclose(archive) == Z_ERRNO)
    rc= 1;
  /* then also close share */
  rc|= free_share(share);

  DBUG_RETURN(rc);
}


/*
  We create our data file here. The format is pretty simple. 
  You can read about the format of the data file above.
  Unlike other storage engines we do not "pack" our data. Since we 
  are about to do a general compression, packing would just be a waste of 
  CPU time. If the table has blobs they are written after the row in the order 
  of creation.
*/

int ha_archive::create(const char *name, TABLE *table_arg,
                       HA_CREATE_INFO *create_info)
{
  File create_file;  // We use to create the datafile and the metafile
  char name_buff[FN_REFLEN];
  int error;
  DBUG_ENTER("ha_archive::create");

  if ((create_file= my_create(fn_format(name_buff,name,"",ARM,
                                        MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                              O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
  {
    error= my_errno;
    goto error;
  }
  write_meta_file(create_file, 0, FALSE);
  my_close(create_file,MYF(0));

  /* 
    We reuse name_buff since it is available.
  */
  if ((create_file= my_create(fn_format(name_buff,name,"",ARZ,
                                        MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                              O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
  {
    error= my_errno;
    goto error;
  }
  if ((archive= gzdopen(create_file, "wb")) == NULL)
  {
    error= errno;
    goto error2;
  }
  if (write_data_header(archive))
  {
    error= errno;
    goto error3;
  }

  if (gzclose(archive))
  {
    error= errno;
    goto error2;
  }

  my_close(create_file, MYF(0));

  DBUG_RETURN(0);

error3:
  /* We already have an error, so ignore results of gzclose. */
  (void)gzclose(archive);
error2:
  my_close(create_file, MYF(0));
  delete_table(name);
error:
  /* Return error number, if we got one */
  DBUG_RETURN(error ? error : -1);
}


/* 
  Look at ha_archive::open() for an explanation of the row format.
  Here we just write out the row.

  Wondering about start_bulk_insert()? We don't implement it for
  archive since it optimizes for lots of writes. The only save
  for implementing start_bulk_insert() is that we could skip 
  setting dirty to true each time.
*/
int ha_archive::write_row(byte * buf)
{
  z_off_t written;
  Field_blob **field;
  DBUG_ENTER("ha_archive::write_row");

  statistic_increment(ha_write_count,&LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  pthread_mutex_lock(&share->mutex);
  written= gzwrite(share->archive_write, buf, table->reclength);
  DBUG_PRINT("ha_archive::get_row", ("Wrote %d bytes expected %d", written, table->reclength));
  share->dirty= TRUE;
  if (written != (z_off_t)table->reclength)
    goto error;
  /*
    We should probably mark the table as damagaged if the record is written
    but the blob fails.
  */
  for (field= table->blob_field ; *field ; field++)
  {
    char *ptr;
    uint32 size= (*field)->get_length();

    if (size)
    {
      (*field)->get_ptr(&ptr);
      written= gzwrite(share->archive_write, ptr, (unsigned)size);
      if (written != (z_off_t)size)
        goto error;
    }
  }
  share->rows_recorded++;
  pthread_mutex_unlock(&share->mutex);

  DBUG_RETURN(0);
error:
  pthread_mutex_unlock(&share->mutex);
  DBUG_RETURN(errno ? errno : -1);
}


/*
  All calls that need to scan the table start with this method. If we are told
  that it is a table scan we rewind the file to the beginning, otherwise
  we assume the position will be set.
*/

int ha_archive::rnd_init(bool scan)
{
  DBUG_ENTER("ha_archive::rnd_init");

  /* We rewind the file so that we can read from the beginning if scan */
  if (scan)
  {
    scan_rows= share->rows_recorded;
    records= 0;

    /* 
      If dirty, we lock, and then reset/flush the data.
      I found that just calling gzflush() doesn't always work.
    */
    if (share->dirty == TRUE)
    {
      pthread_mutex_lock(&share->mutex);
      if (share->dirty == TRUE)
      {
        gzflush(share->archive_write, Z_SYNC_FLUSH);
        share->dirty= FALSE;
      }
      pthread_mutex_unlock(&share->mutex);
    }

    if (read_data_header(archive))
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  DBUG_RETURN(0);
}


/*
  This is the method that is used to read a row. It assumes that the row is 
  positioned where you want it.
*/
int ha_archive::get_row(gzFile file_to_read, byte *buf)
{
  int read; // Bytes read, gzread() returns int
  char *last;
  size_t total_blob_length= 0;
  Field_blob **field;
  DBUG_ENTER("ha_archive::get_row");

  read= gzread(file_to_read, buf, table->reclength);
  DBUG_PRINT("ha_archive::get_row", ("Read %d bytes expected %d", read, table->reclength));

  if (read == Z_STREAM_ERROR)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /* If we read nothing we are at the end of the file */
  if (read == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  /* If the record is the wrong size, the file is probably damaged */
  if ((ulong) read != table->reclength)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /* Calculate blob length, we use this for our buffer */
  for (field=table->blob_field; *field ; field++)
    total_blob_length += (*field)->get_length();

  /* Adjust our row buffer if we need be */
  buffer.alloc(total_blob_length);
  last= (char *)buffer.ptr();

  /* Loop through our blobs and read them */
  for (field=table->blob_field; *field ; field++)
  {
    size_t size= (*field)->get_length();
    if (size)
    {
      read= gzread(file_to_read, last, size);
      if ((size_t) read != size)
        DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
      (*field)->set_ptr(size, last);
      last += size;
    }
  }
  DBUG_RETURN(0);
}


/* 
  Called during ORDER BY. Its position is either from being called sequentially
  or by having had ha_archive::rnd_pos() called before it is called.
*/

int ha_archive::rnd_next(byte *buf)
{
  int rc;
  DBUG_ENTER("ha_archive::rnd_next");

  if (!scan_rows)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  scan_rows--;

  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  current_position= gztell(archive);
  rc= get_row(archive, buf);


  if (rc != HA_ERR_END_OF_FILE)
    records++;

  DBUG_RETURN(rc);
}


/* 
  Thanks to the table flag HA_REC_NOT_IN_SEQ this will be called after
  each call to ha_archive::rnd_next() if an ordering of the rows is
  needed.
*/

void ha_archive::position(const byte *record)
{
  DBUG_ENTER("ha_archive::position");
  ha_store_ptr(ref, ref_length, current_position);
  DBUG_VOID_RETURN;
}


/*
  This is called after a table scan for each row if the results of the
  scan need to be ordered. It will take *pos and use it to move the
  cursor in the file so that the next row that is called is the
  correctly ordered row.
*/

int ha_archive::rnd_pos(byte * buf, byte *pos)
{
  DBUG_ENTER("ha_archive::rnd_pos");
  statistic_increment(ha_read_rnd_count,&LOCK_status);
  current_position= ha_get_ptr(pos, ref_length);
  (void)gzseek(archive, current_position, SEEK_SET);

  DBUG_RETURN(get_row(archive, buf));
}

/*
  This method rebuilds the meta file. It does this by walking the datafile and 
  rewriting the meta file.
*/
int ha_archive::rebuild_meta_file(char *table_name, File meta_file)
{
  int rc;
  byte *buf; 
  ulonglong rows_recorded= 0;
  gzFile rebuild_file;            /* Archive file we are working with */
  char data_file_name[FN_REFLEN];
  DBUG_ENTER("ha_archive::rebuild_meta_file");

  /*
    Open up the meta file to recreate it.
  */
  fn_format(data_file_name, table_name, "", ARZ,
            MY_REPLACE_EXT|MY_UNPACK_FILENAME);
  if ((rebuild_file= gzopen(data_file_name, "rb")) == NULL)
    DBUG_RETURN(errno ? errno : -1);

  if ((rc= read_data_header(rebuild_file)))
    goto error;

  /*
    We malloc up the buffer we will use for counting the rows. 
    I know, this malloc'ing memory but this should be a very 
    rare event.
  */
  if (!(buf= (byte*) my_malloc(table->rec_buff_length > sizeof(ulonglong) +1 ? 
                               table->rec_buff_length : sizeof(ulonglong) +1 ,
                               MYF(MY_WME))))
  {
    rc= HA_ERR_CRASHED_ON_USAGE;
    goto error;
  }

  while (!(rc= get_row(rebuild_file, buf)))
    rows_recorded++;

  /* 
    Only if we reach the end of the file do we assume we can rewrite.
    At this point we reset rc to a non-message state.
  */
  if (rc == HA_ERR_END_OF_FILE)
  {
    (void)write_meta_file(meta_file, rows_recorded, FALSE);
    rc= 0;
  }

  my_free((gptr) buf, MYF(0));
error:
  gzclose(rebuild_file);

  DBUG_RETURN(rc);
}

/*
  The table can become fragmented if data was inserted, read, and then
  inserted again. What we do is open up the file and recompress it completely. 
*/
int ha_archive::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("ha_archive::optimize");
  int read; // Bytes read, gzread() returns int
  gzFile reader, writer;
  char block[IO_SIZE];
  char writer_filename[FN_REFLEN];

  /* Lets create a file to contain the new data */
  fn_format(writer_filename, share->table_name, "", ARN, 
            MY_REPLACE_EXT|MY_UNPACK_FILENAME);

  /* Closing will cause all data waiting to be flushed, to be flushed */
  gzclose(share->archive_write);

  if ((reader= gzopen(share->data_file_name, "rb")) == NULL)
    DBUG_RETURN(-1); 

  if ((writer= gzopen(writer_filename, "wb")) == NULL)
  {
    gzclose(reader);
    DBUG_RETURN(-1); 
  }

  while ((read= gzread(reader, block, IO_SIZE)))
    gzwrite(writer, block, read);

  gzclose(reader);
  gzclose(writer);

  my_rename(writer_filename,share->data_file_name,MYF(0));

  /* 
    We reopen the file in case some IO is waiting to go through.
    In theory the table is closed right after this operation,
    but it is possible for IO to still happen.
    I may be being a bit too paranoid right here.
  */
  if ((share->archive_write= gzopen(share->data_file_name, "ab")) == NULL)
    DBUG_RETURN(errno ? errno : -1);
  share->dirty= FALSE;

  DBUG_RETURN(0); 
}


/*
  No transactions yet, so this is pretty dull.
*/
int ha_archive::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_archive::external_lock");
  DBUG_RETURN(0);
}

/* 
  Below is an example of how to setup row level locking.
*/
THR_LOCK_DATA **ha_archive::store_lock(THD *thd,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) 
  {
    /* 
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set 
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers 
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables
        && !thd->tablespace_op)
      lock_type = TL_WRITE_ALLOW_WRITE;

    /* 
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2. 
    */

    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables) 
      lock_type = TL_READ;

    lock.type=lock_type;
  }

  *to++= &lock;

  return to;
}


/******************************************************************************

  Everything below here is default, please look at ha_example.cc for 
  descriptions.

 ******************************************************************************/

int ha_archive::update_row(const byte * old_data, byte * new_data)
{

  DBUG_ENTER("ha_archive::update_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_archive::delete_row(const byte * buf)
{
  DBUG_ENTER("ha_archive::delete_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_archive::index_read(byte * buf, const byte * key,
                           uint key_len __attribute__((unused)),
                           enum ha_rkey_function find_flag
                           __attribute__((unused)))
{
  DBUG_ENTER("ha_archive::index_read");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_archive::index_read_idx(byte * buf, uint index, const byte * key,
                               uint key_len __attribute__((unused)),
                               enum ha_rkey_function find_flag
                               __attribute__((unused)))
{
  DBUG_ENTER("ha_archive::index_read_idx");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


int ha_archive::index_next(byte * buf)
{
  DBUG_ENTER("ha_archive::index_next");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_archive::index_prev(byte * buf)
{
  DBUG_ENTER("ha_archive::index_prev");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_archive::index_first(byte * buf)
{
  DBUG_ENTER("ha_archive::index_first");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_archive::index_last(byte * buf)
{
  DBUG_ENTER("ha_archive::index_last");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}


void ha_archive::info(uint flag)
{
  DBUG_ENTER("ha_archive::info");

  /* This is a lie, but you don't want the optimizer to see zero or 1 */
  records= share->rows_recorded;
  deleted= 0;

  DBUG_VOID_RETURN;
}

int ha_archive::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_archive::extra");
  DBUG_RETURN(0);
}

int ha_archive::reset(void)
{
  DBUG_ENTER("ha_archive::reset");
  DBUG_RETURN(0);
}

ha_rows ha_archive::records_in_range(uint inx, key_range *min_key,
                                     key_range *max_key)
{
  DBUG_ENTER("ha_archive::records_in_range ");
  DBUG_RETURN(records); // HA_ERR_WRONG_COMMAND 
}

/*
  We cancel a truncate command. The only way to delete an archive table is to drop it.
  This is done for security reasons. In a later version we will enable this by 
  allowing the user to select a different row format.
*/
int ha_archive::delete_all_rows()
{
  DBUG_ENTER("ha_archive::delete_all_rows");
  DBUG_RETURN(0);
}
#endif /* HAVE_ARCHIVE_DB */
