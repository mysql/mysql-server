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

#include "mysql_priv.h"
#include <myisam.h>

#include "ha_archive.h"
#include <my_dir.h>

#include <mysql/plugin.h>

/*
  First, if you want to understand storage engines you should look at 
  ha_example.cc and ha_example.h. 
  This example was written as a test case for a customer who needed
  a storage engine without indexes that could compress data very well.
  So, welcome to a completely compressed storage engine. This storage
  engine only does inserts. No replace, deletes, or updates. All reads are 
  complete table scans. Compression is done through azip (bzip compresses
  better, but only marginally, if someone asks I could add support for
  it too, but beaware that it costs a lot more in CPU time then azip).
  
  We keep a file pointer open for each instance of ha_archive for each read
  but for writes we keep one open file handle just for that. We flush it
  only if we have a read occur. azip handles compressing lots of records
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
  to be any faster. 

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
   Implement versioning, should be easy.
   Allow for errors, find a way to mark bad rows.
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
  auto increment - MAX value for autoincrement
  dirty - Status of the file, whether or not its values are the latest. This
          flag is what causes a repair to occur

  The data file:
  check - Just an int of 254 to make sure that the the file we are opening was
          never corrupted.
  version - The current version of the file format.
  data - The data is stored in a "row +blobs" format.
*/

/* If the archive storage engine has been inited */
static bool archive_inited= FALSE;
/* Variables for archive share methods */
pthread_mutex_t archive_mutex;
static HASH archive_open_tables;

/* The file extension */
#define ARZ ".ARZ"               // The data file
#define ARN ".ARN"               // Files used during an optimize call
#define ARM ".ARM"               // Meta file
/*
  uchar + uchar + ulonglong + ulonglong + ulonglong + ulonglong + FN_REFLEN 
  + uchar
*/
#define META_BUFFER_SIZE sizeof(uchar) + sizeof(uchar) + sizeof(ulonglong) \
  + sizeof(ulonglong) + sizeof(ulonglong) + sizeof(ulonglong) + FN_REFLEN \
  + sizeof(uchar)

/*
  uchar + uchar
*/
#define DATA_BUFFER_SIZE 2       // Size of the data used in the data file
#define ARCHIVE_CHECK_HEADER 254 // The number we use to determine corruption

/* Static declarations for handerton */
static handler *archive_create_handler(TABLE_SHARE *table, MEM_ROOT *mem_root);
/*
  Number of rows that will force a bulk insert.
*/
#define ARCHIVE_MIN_ROWS_TO_USE_BULK_INSERT 2

handlerton archive_hton;

static handler *archive_create_handler(TABLE_SHARE *table, MEM_ROOT *mem_root)
{
  return new (mem_root) ha_archive(table);
}

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

int archive_db_init()
{
  DBUG_ENTER("archive_db_init");
  if (archive_inited)
    DBUG_RETURN(FALSE);

  archive_hton.state=SHOW_OPTION_YES;
  archive_hton.db_type=DB_TYPE_ARCHIVE_DB;
  archive_hton.create=archive_create_handler;
  archive_hton.panic=archive_db_end;
  archive_hton.flags=HTON_NO_FLAGS;

  if (pthread_mutex_init(&archive_mutex, MY_MUTEX_INIT_FAST))
    goto error;
  if (hash_init(&archive_open_tables, system_charset_info, 32, 0, 0,
                (hash_get_key) archive_get_key, 0, 0))
  {
    VOID(pthread_mutex_destroy(&archive_mutex));
  }
  else
  {
    archive_inited= TRUE;
    DBUG_RETURN(FALSE);
  }
error:
  DBUG_RETURN(TRUE);
}

/*
  Release the archive handler.

  SYNOPSIS
    archive_db_done()
    void

  RETURN
    FALSE       OK
*/

int archive_db_done()
{
  if (archive_inited)
  {
    hash_free(&archive_open_tables);
    VOID(pthread_mutex_destroy(&archive_mutex));
  }
  archive_inited= 0;
  return 0;
}


int archive_db_end(ha_panic_function type)
{
  return archive_db_done();
}

ha_archive::ha_archive(TABLE_SHARE *table_arg)
  :handler(&archive_hton, table_arg), delayed_insert(0), bulk_insert(0)
{
  /* Set our original buffer from pre-allocated memory */
  buffer.set((char *)byte_buffer, IO_SIZE, system_charset_info);

  /* The size of the offset value we will use for position() */
  ref_length = sizeof(my_off_t);
}

/*
  This method reads the header of a datafile and returns whether or not it was successful.
*/
int ha_archive::read_data_header(azio_stream *file_to_read)
{
  uchar data_buffer[DATA_BUFFER_SIZE];
  DBUG_ENTER("ha_archive::read_data_header");

  if (azrewind(file_to_read) == -1)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  if (azread(file_to_read, data_buffer, DATA_BUFFER_SIZE) != DATA_BUFFER_SIZE)
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
int ha_archive::write_data_header(azio_stream *file_to_write)
{
  uchar data_buffer[DATA_BUFFER_SIZE];
  DBUG_ENTER("ha_archive::write_data_header");

  data_buffer[0]= (uchar)ARCHIVE_CHECK_HEADER;
  data_buffer[1]= (uchar)ARCHIVE_VERSION;

  if (azwrite(file_to_write, &data_buffer, DATA_BUFFER_SIZE) != 
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
int ha_archive::read_meta_file(File meta_file, ha_rows *rows, 
                               ulonglong *auto_increment,
                               ulonglong *forced_flushes,
                               char *real_path)
{
  uchar meta_buffer[META_BUFFER_SIZE];
  uchar *ptr= meta_buffer;
  ulonglong check_point;

  DBUG_ENTER("ha_archive::read_meta_file");

  VOID(my_seek(meta_file, 0, MY_SEEK_SET, MYF(0)));
  if (my_read(meta_file, (byte*)meta_buffer, META_BUFFER_SIZE, 0) != META_BUFFER_SIZE)
    DBUG_RETURN(-1);
  
  /*
    Parse out the meta data, we ignore version at the moment
  */

  ptr+= sizeof(uchar)*2; // Move past header
  *rows= (ha_rows)uint8korr(ptr);
  ptr+= sizeof(ulonglong); // Move past rows
  check_point= uint8korr(ptr);
  ptr+= sizeof(ulonglong); // Move past check_point
  *auto_increment= uint8korr(ptr);
  ptr+= sizeof(ulonglong); // Move past auto_increment
  *forced_flushes= uint8korr(ptr);
  ptr+= sizeof(ulonglong); // Move past forced_flush
  memmove(real_path, ptr, FN_REFLEN);
  ptr+= FN_REFLEN; // Move past the possible location of the file

  DBUG_PRINT("ha_archive::read_meta_file", ("Check %d", (uint)meta_buffer[0]));
  DBUG_PRINT("ha_archive::read_meta_file", ("Version %d", (uint)meta_buffer[1]));
  DBUG_PRINT("ha_archive::read_meta_file", ("Rows %llu", *rows));
  DBUG_PRINT("ha_archive::read_meta_file", ("Checkpoint %llu", check_point));
  DBUG_PRINT("ha_archive::read_meta_file", ("Auto-Increment %llu", *auto_increment));
  DBUG_PRINT("ha_archive::read_meta_file", ("Forced Flushes %llu", *forced_flushes));
  DBUG_PRINT("ha_archive::read_meta_file", ("Real Path %s", real_path));
  DBUG_PRINT("ha_archive::read_meta_file", ("Dirty %d", (int)(*ptr)));

  if ((meta_buffer[0] != (uchar)ARCHIVE_CHECK_HEADER) || 
      ((bool)(*ptr)== TRUE))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  my_sync(meta_file, MYF(MY_WME));

  DBUG_RETURN(0);
}

/*
  This method writes out the header of a meta file and returns whether or not it was successful.
  By setting dirty you say whether or not the file represents the actual state of the data file.
  Upon ::open() we set to dirty, and upon ::close() we set to clean.
*/
int ha_archive::write_meta_file(File meta_file, ha_rows rows, 
                                ulonglong auto_increment, 
                                ulonglong forced_flushes, 
                                char *real_path,
                                bool dirty)
{
  uchar meta_buffer[META_BUFFER_SIZE];
  uchar *ptr= meta_buffer;
  ulonglong check_point= 0; //Reserved for the future

  DBUG_ENTER("ha_archive::write_meta_file");

  *ptr= (uchar)ARCHIVE_CHECK_HEADER;
  ptr += sizeof(uchar);
  *ptr= (uchar)ARCHIVE_VERSION;
  ptr += sizeof(uchar);
  int8store(ptr, (ulonglong)rows); 
  ptr += sizeof(ulonglong);
  int8store(ptr, check_point); 
  ptr += sizeof(ulonglong);
  int8store(ptr, auto_increment); 
  ptr += sizeof(ulonglong);
  int8store(ptr, forced_flushes); 
  ptr += sizeof(ulonglong);
  // No matter what, we pad with nulls
  if (real_path)
    strncpy((char *)ptr, real_path, FN_REFLEN);
  else
    bzero(ptr, FN_REFLEN);
  ptr += FN_REFLEN;
  *ptr= (uchar)dirty;
  DBUG_PRINT("ha_archive::write_meta_file", ("Check %d", 
                                             (uint)ARCHIVE_CHECK_HEADER));
  DBUG_PRINT("ha_archive::write_meta_file", ("Version %d", 
                                             (uint)ARCHIVE_VERSION));
  DBUG_PRINT("ha_archive::write_meta_file", ("Rows %llu", (ulonglong)rows));
  DBUG_PRINT("ha_archive::write_meta_file", ("Checkpoint %llu", check_point));
  DBUG_PRINT("ha_archive::write_meta_file", ("Auto Increment %llu",
                                             auto_increment));
  DBUG_PRINT("ha_archive::write_meta_file", ("Forced Flushes %llu", 
                                            forced_flushes));
  DBUG_PRINT("ha_archive::write_meta_file", ("Real path %s", 
                                            real_path));
  DBUG_PRINT("ha_archive::write_meta_file", ("Dirty %d", (uint)dirty));

  VOID(my_seek(meta_file, 0, MY_SEEK_SET, MYF(0)));
  if (my_write(meta_file, (byte *)meta_buffer, META_BUFFER_SIZE, 0) != META_BUFFER_SIZE)
    DBUG_RETURN(-1);
  
  my_sync(meta_file, MYF(MY_WME));

  DBUG_RETURN(0);
}


/*
  We create the shared memory space that we will use for the open table. 
  No matter what we try to get or create a share. This is so that a repair
  table operation can occur. 

  See ha_example.cc for a longer description.
*/
ARCHIVE_SHARE *ha_archive::get_share(const char *table_name, 
                                     TABLE *table, int *rc)
{
  ARCHIVE_SHARE *share;
  char meta_file_name[FN_REFLEN];
  uint length;
  char *tmp_name;
  DBUG_ENTER("ha_archive::get_share");

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
      *rc= HA_ERR_OUT_OF_MEM;
      DBUG_RETURN(NULL);
    }

    share->use_count= 0;
    share->table_name_length= length;
    share->table_name= tmp_name;
    share->crashed= FALSE;
    share->archive_write_open= FALSE;
    fn_format(share->data_file_name, table_name, "",
              ARZ,MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    fn_format(meta_file_name, table_name, "", ARM,
              MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    strmov(share->table_name,table_name);
    /*
      We will use this lock for rows.
    */
    VOID(pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST));
    if ((share->meta_file= my_open(meta_file_name, O_RDWR, MYF(0))) == -1)
      share->crashed= TRUE;
    DBUG_PRINT("info", ("archive opening (1) up write at %s", 
                        share->data_file_name));
    
    /*
      We read the meta file, but do not mark it dirty unless we actually do
      a write.
    */
    if (read_meta_file(share->meta_file, &share->rows_recorded, 
                       &share->auto_increment_value,
                       &share->forced_flushes,
                       share->real_path))
      share->crashed= TRUE;
    /*
      Since we now possibly no real_path, we will use it instead if it exists.
    */
    if (*share->real_path)
      fn_format(share->data_file_name, share->real_path, "", ARZ,
                MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    VOID(my_hash_insert(&archive_open_tables, (byte*) share));
    thr_lock_init(&share->lock);
  }
  share->use_count++;
  DBUG_PRINT("info", ("archive table %.*s has %d open handles now", 
                      share->table_name_length, share->table_name,
                      share->use_count));
  if (share->crashed)
    *rc= HA_ERR_CRASHED_ON_USAGE;
  pthread_mutex_unlock(&archive_mutex);

  DBUG_RETURN(share);
}


/* 
  Free the share.
  See ha_example.cc for a description.
*/
int ha_archive::free_share(ARCHIVE_SHARE *share)
{
  int rc= 0;
  DBUG_ENTER("ha_archive::free_share");
  DBUG_PRINT("info", ("archive table %.*s has %d open handles on entrance", 
                      share->table_name_length, share->table_name,
                      share->use_count));

  pthread_mutex_lock(&archive_mutex);
  if (!--share->use_count)
  {
    hash_delete(&archive_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    VOID(pthread_mutex_destroy(&share->mutex));
    /* 
      We need to make sure we don't reset the crashed state.
      If we open a crashed file, wee need to close it as crashed unless
      it has been repaired.
      Since we will close the data down after this, we go on and count
      the flush on close;
    */
    share->forced_flushes++;
    (void)write_meta_file(share->meta_file, share->rows_recorded,
                          share->auto_increment_value, 
                          share->forced_flushes, 
                          share->real_path, 
                          share->crashed ? TRUE :FALSE);
    if (share->archive_write_open)
      if (azclose(&(share->archive_write)))
        rc= 1;
    if (my_close(share->meta_file, MYF(0)))
      rc= 1;
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&archive_mutex);

  DBUG_RETURN(rc);
}

int ha_archive::init_archive_writer()
{
  DBUG_ENTER("ha_archive::init_archive_writer");
  (void)write_meta_file(share->meta_file, share->rows_recorded,
                        share->auto_increment_value, 
                        share->forced_flushes, 
                        share->real_path,
                        TRUE);

  /* 
    It is expensive to open and close the data files and since you can't have
    a gzip file that can be both read and written we keep a writer open
    that is shared amoung all open tables.
  */
  if (!(azopen(&(share->archive_write), share->data_file_name, 
               O_WRONLY|O_APPEND|O_BINARY)))
  {
    DBUG_PRINT("info", ("Could not open archive write file"));
    share->crashed= TRUE;
    DBUG_RETURN(1);
  }
  share->archive_write_open= TRUE;

  DBUG_RETURN(0);
}


/*
  We just implement one additional file extension.
*/
static const char *ha_archive_exts[] = {
  ARZ,
  ARM,
  NullS
};

const char **ha_archive::bas_ext() const
{
  return ha_archive_exts;
}


/* 
  When opening a file we:
  Create/get our shared structure.
  Init out lock.
  We open the file we will read from.
*/
int ha_archive::open(const char *name, int mode, uint open_options)
{
  int rc= 0;
  DBUG_ENTER("ha_archive::open");

  DBUG_PRINT("info", ("archive table was opened for crash: %s", 
                      (open_options & HA_OPEN_FOR_REPAIR) ? "yes" : "no"));
  share= get_share(name, table, &rc);

  if (rc == HA_ERR_CRASHED_ON_USAGE && !(open_options & HA_OPEN_FOR_REPAIR))
  {
    free_share(share);
    DBUG_RETURN(rc);
  }
  else if (rc == HA_ERR_OUT_OF_MEM)
  {
    DBUG_RETURN(rc);
  }

  thr_lock_data_init(&share->lock,&lock,NULL);

  DBUG_PRINT("info", ("archive data_file_name %s", share->data_file_name));
  if (!(azopen(&archive, share->data_file_name, O_RDONLY|O_BINARY)))
  {
    if (errno == EROFS || errno == EACCES)
      DBUG_RETURN(my_errno= errno);
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  DBUG_PRINT("info", ("archive table was crashed %s", 
                      rc == HA_ERR_CRASHED_ON_USAGE ? "yes" : "no"));
  if (rc == HA_ERR_CRASHED_ON_USAGE && open_options & HA_OPEN_FOR_REPAIR)
  {
    DBUG_RETURN(0);
  }
  else
    DBUG_RETURN(rc);
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
  if (azclose(&archive))
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

  stats.auto_increment_value= (create_info->auto_increment_value ?
                               create_info->auto_increment_value -1 :
                   (ulonglong) 0);

  for (uint key= 0; key < table_arg->s->keys; key++)
  {
    KEY *pos= table_arg->key_info+key;
    KEY_PART_INFO *key_part=     pos->key_part;
    KEY_PART_INFO *key_part_end= key_part + pos->key_parts;

    for (; key_part != key_part_end; key_part++)
    {
      Field *field= key_part->field;

      if (!(field->flags & AUTO_INCREMENT_FLAG))
      {
        error= -1;
        DBUG_PRINT("info", ("Index error in creating archive table"));
        goto error;
      }
    }
  }

  if ((create_file= my_create(fn_format(name_buff,name,"",ARM,
                                        MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                              O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
  {
    error= my_errno;
    goto error;
  }

  write_meta_file(create_file, 0, stats.auto_increment_value, 0, 
                  (char *)create_info->data_file_name,
                  FALSE);
  my_close(create_file,MYF(0));

  /* 
    We reuse name_buff since it is available.
  */
  if (create_info->data_file_name)
  {
    char linkname[FN_REFLEN];
    DBUG_PRINT("info", ("archive will create stream file %s", 
                        create_info->data_file_name));
                        
    fn_format(name_buff, create_info->data_file_name, "", ARZ,
              MY_REPLACE_EXT|MY_UNPACK_FILENAME);
    fn_format(linkname, name, "", ARZ,
              MY_UNPACK_FILENAME | MY_APPEND_EXT);
    if ((create_file= my_create_with_symlink(linkname, name_buff, 0,
                                O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    {
      error= my_errno;
      goto error;
    }
  }
  else
  {
    if ((create_file= my_create(fn_format(name_buff, name,"", ARZ,
                                          MY_REPLACE_EXT|MY_UNPACK_FILENAME),0,
                                O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    {
      error= my_errno;
      goto error;
    }
  }
  if (!azdopen(&archive, create_file, O_WRONLY|O_BINARY))
  {
    error= errno;
    goto error2;
  }
  if (write_data_header(&archive))
  {
    error= errno;
    goto error3;
  }

  if (azclose(&archive))
  {
    error= errno;
    goto error2;
  }

  DBUG_RETURN(0);

error3:
  /* We already have an error, so ignore results of azclose. */
  (void)azclose(&archive);
error2:
  my_close(create_file, MYF(0));
  delete_table(name);
error:
  /* Return error number, if we got one */
  DBUG_RETURN(error ? error : -1);
}

/*
  This is where the actual row is written out.
*/
int ha_archive::real_write_row(byte *buf, azio_stream *writer)
{
  my_off_t written;
  uint *ptr, *end;
  DBUG_ENTER("ha_archive::real_write_row");

  written= azwrite(writer, buf, table->s->reclength);
  DBUG_PRINT("ha_archive::real_write_row", ("Wrote %d bytes expected %d", 
                                            written, table->s->reclength));
  if (!delayed_insert || !bulk_insert)
    share->dirty= TRUE;

  if (written != (my_off_t)table->s->reclength)
    DBUG_RETURN(errno ? errno : -1);
  /*
    We should probably mark the table as damagaged if the record is written
    but the blob fails.
  */
  for (ptr= table->s->blob_field, end= ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
  {
    char *data_ptr;
    uint32 size= ((Field_blob*) table->field[*ptr])->get_length();

    if (size)
    {
      ((Field_blob*) table->field[*ptr])->get_ptr(&data_ptr);
      written= azwrite(writer, data_ptr, (unsigned)size);
      if (written != (my_off_t)size)
        DBUG_RETURN(errno ? errno : -1);
    }
  }
  DBUG_RETURN(0);
}


/* 
  Look at ha_archive::open() for an explanation of the row format.
  Here we just write out the row.

  Wondering about start_bulk_insert()? We don't implement it for
  archive since it optimizes for lots of writes. The only save
  for implementing start_bulk_insert() is that we could skip 
  setting dirty to true each time.
*/
int ha_archive::write_row(byte *buf)
{
  int rc;
  byte *read_buf= NULL;
  ulonglong temp_auto;
  DBUG_ENTER("ha_archive::write_row");

  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  ha_statistic_increment(&SSV::ha_write_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  pthread_mutex_lock(&share->mutex);

  if (table->next_number_field)
  {
    KEY *mkey= &table->s->key_info[0]; // We only support one key right now
    update_auto_increment();
    temp_auto= table->next_number_field->val_int();

    /*
      Bad news, this will cause a search for the unique value which is very 
      expensive since we will have to do a table scan which will lock up 
      all other writers during this period. This could perhaps be optimized 
      in the future.
    */
    if (temp_auto == share->auto_increment_value && 
        mkey->flags & HA_NOSAME)
    {
      rc= HA_ERR_FOUND_DUPP_KEY;
      goto error;
    }

    if (temp_auto < share->auto_increment_value && 
        mkey->flags & HA_NOSAME)
    {
      /* 
        First we create a buffer that we can use for reading rows, and can pass
        to get_row().
      */
      if (!(read_buf= (byte*) my_malloc(table->s->reclength, MYF(MY_WME))))
      {
        rc= HA_ERR_OUT_OF_MEM;
        goto error;
      }
       /* 
         All of the buffer must be written out or we won't see all of the
         data 
       */
      azflush(&(share->archive_write), Z_SYNC_FLUSH);
      share->forced_flushes++;
      /*
        Set the position of the local read thread to the beginning postion.
      */
      if (read_data_header(&archive))
      {
        rc= HA_ERR_CRASHED_ON_USAGE;
        goto error;
      }

      /*
        Now we read and check all of the rows.
        if (!memcmp(table->next_number_field->ptr, mfield->ptr, mfield->max_length()))
        if ((longlong)temp_auto == 
            mfield->val_int((char*)(read_buf + mfield->offset())))
      */
      Field *mfield= table->next_number_field;

      while (!(get_row(&archive, read_buf)))
      {
        if (!memcmp(read_buf + mfield->offset(), table->next_number_field->ptr,
                    mfield->max_length()))
        {
          rc= HA_ERR_FOUND_DUPP_KEY;
          goto error;
        }
      }
    }
    else
    {
      if (temp_auto > share->auto_increment_value)
        stats.auto_increment_value= share->auto_increment_value= temp_auto;
    }
  }

  /*
    Notice that the global auto_increment has been increased.
    In case of a failed row write, we will never try to reuse the value.
  */
  if (!share->archive_write_open)
    if (init_archive_writer())
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /*
    Varchar structures are constant in size but are not cleaned up request
    to request. The following sets all unused space to null to improve
    compression.
  */
  for (Field **field=table->field ; *field ; field++)
  {
    DBUG_PRINT("archive",("Pack is %d\n", (*field)->pack_length()));
    DBUG_PRINT("archive",("MyPack is %d\n", (*field)->data_length((char*) buf + (*field)->offset())));
    if ((*field)->real_type() == MYSQL_TYPE_VARCHAR) 
    {
      uint actual_length= (*field)->data_length((char*) buf + (*field)->offset());
      uint offset= (*field)->offset() + actual_length + 
        (actual_length > 255 ? 2 : 1);
      DBUG_PRINT("archive",("Offset is %d -> %d\n", actual_length, offset));
      /*
      if ((*field)->pack_length() + (*field)->offset() != offset)
        bzero(buf + offset, (size_t)((*field)->pack_length() + (actual_length > 255 ? 2 : 1) - (*field)->data_length));
    */
    }
  }

  share->rows_recorded++;
  rc= real_write_row(buf, &(share->archive_write));
error:
  pthread_mutex_unlock(&share->mutex);
  if (read_buf)
    my_free((gptr) read_buf, MYF(0));

  DBUG_RETURN(rc);
}


void ha_archive::get_auto_increment(ulonglong offset, ulonglong increment,
                                    ulonglong nb_desired_values,
                                    ulonglong *first_value,
                                    ulonglong *nb_reserved_values)
{
  *nb_reserved_values= 1;
  *first_value= share->auto_increment_value + 1;
}

/* Initialized at each key walk (called multiple times unlike rnd_init()) */
int ha_archive::index_init(uint keynr, bool sorted)
{
  DBUG_ENTER("ha_archive::index_init");
  active_index= keynr;
  DBUG_RETURN(0);
}


/*
  No indexes, so if we get a request for an index search since we tell
  the optimizer that we have unique indexes, we scan
*/
int ha_archive::index_read(byte *buf, const byte *key,
                             uint key_len, enum ha_rkey_function find_flag)
{
  int rc;
  DBUG_ENTER("ha_archive::index_read");
  rc= index_read_idx(buf, active_index, key, key_len, find_flag);
  DBUG_RETURN(rc);
}


int ha_archive::index_read_idx(byte *buf, uint index, const byte *key,
                                 uint key_len, enum ha_rkey_function find_flag)
{
  int rc= 0;
  bool found= 0;
  KEY *mkey= &table->s->key_info[index];
  current_k_offset= mkey->key_part->offset;
  current_key= key;
  current_key_len= key_len;


  DBUG_ENTER("ha_archive::index_read_idx");

  /* 
    All of the buffer must be written out or we won't see all of the
    data 
  */
  pthread_mutex_lock(&share->mutex);
  azflush(&(share->archive_write), Z_SYNC_FLUSH);
  share->forced_flushes++;
  pthread_mutex_unlock(&share->mutex);

  /*
    Set the position of the local read thread to the beginning postion.
  */
  if (read_data_header(&archive))
  {
    rc= HA_ERR_CRASHED_ON_USAGE;
    goto error;
  }

  while (!(get_row(&archive, buf)))
  {
    if (!memcmp(current_key, buf + current_k_offset, current_key_len))
    {
      found= 1;
      break;
    }
  }

  if (found)
    DBUG_RETURN(0);

error:
  DBUG_RETURN(rc ? rc : HA_ERR_END_OF_FILE);
}


int ha_archive::index_next(byte * buf) 
{ 
  bool found= 0;

  DBUG_ENTER("ha_archive::index_next");

  while (!(get_row(&archive, buf)))
  {
    if (!memcmp(current_key, buf+current_k_offset, current_key_len))
    {
      found= 1;
      break;
    }
  }

  DBUG_RETURN(found ? 0 : HA_ERR_END_OF_FILE); 
}

/*
  All calls that need to scan the table start with this method. If we are told
  that it is a table scan we rewind the file to the beginning, otherwise
  we assume the position will be set.
*/

int ha_archive::rnd_init(bool scan)
{
  DBUG_ENTER("ha_archive::rnd_init");
  
  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /* We rewind the file so that we can read from the beginning if scan */
  if (scan)
  {
    scan_rows= share->rows_recorded;
    DBUG_PRINT("info", ("archive will retrieve %llu rows", scan_rows));
    stats.records= 0;

    /* 
      If dirty, we lock, and then reset/flush the data.
      I found that just calling azflush() doesn't always work.
    */
    if (share->dirty == TRUE)
    {
      pthread_mutex_lock(&share->mutex);
      if (share->dirty == TRUE)
      {
        DBUG_PRINT("info", ("archive flushing out rows for scan"));
        azflush(&(share->archive_write), Z_SYNC_FLUSH);
        share->forced_flushes++;
        share->dirty= FALSE;
      }
      pthread_mutex_unlock(&share->mutex);
    }

    if (read_data_header(&archive))
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  DBUG_RETURN(0);
}


/*
  This is the method that is used to read a row. It assumes that the row is 
  positioned where you want it.
*/
int ha_archive::get_row(azio_stream *file_to_read, byte *buf)
{
  int read; // Bytes read, azread() returns int
  uint *ptr, *end;
  char *last;
  size_t total_blob_length= 0;
  MY_BITMAP *read_set= table->read_set;
  DBUG_ENTER("ha_archive::get_row");

  read= azread(file_to_read, buf, table->s->reclength);
  DBUG_PRINT("ha_archive::get_row", ("Read %d bytes expected %d", read, 
                                     table->s->reclength));

  if (read == Z_STREAM_ERROR)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /* If we read nothing we are at the end of the file */
  if (read == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  /* 
    If the record is the wrong size, the file is probably damaged, unless 
    we are dealing with a delayed insert or a bulk insert.
  */
  if ((ulong) read != table->s->reclength)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  /* Calculate blob length, we use this for our buffer */
  for (ptr= table->s->blob_field, end=ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
  {
    if (bitmap_is_set(read_set,
                      (((Field_blob*) table->field[*ptr])->field_index)))
        total_blob_length += ((Field_blob*) table->field[*ptr])->get_length();
  }

  /* Adjust our row buffer if we need be */
  buffer.alloc(total_blob_length);
  last= (char *)buffer.ptr();

  /* Loop through our blobs and read them */
  for (ptr= table->s->blob_field, end=ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
  {
    size_t size= ((Field_blob*) table->field[*ptr])->get_length();
    if (size)
    {
      if (bitmap_is_set(read_set,
                        ((Field_blob*) table->field[*ptr])->field_index))
      {
        read= azread(file_to_read, last, size);
        if ((size_t) read != size)
          DBUG_RETURN(HA_ERR_END_OF_FILE);
        ((Field_blob*) table->field[*ptr])->set_ptr(size, last);
        last += size;
      }
      else
      {
        (void)azseek(file_to_read, size, SEEK_CUR);
      }
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

  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  if (!scan_rows)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  scan_rows--;

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  current_position= aztell(&archive);
  rc= get_row(&archive, buf);


  if (rc != HA_ERR_END_OF_FILE)
    stats.records++;

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
  my_store_ptr(ref, ref_length, current_position);
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
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  current_position= (my_off_t)my_get_ptr(pos, ref_length);
  (void)azseek(&archive, current_position, SEEK_SET);

  DBUG_RETURN(get_row(&archive, buf));
}

/*
  This method repairs the meta file. It does this by walking the datafile and 
  rewriting the meta file. Currently it does this by calling optimize with
  the extended flag.
*/
int ha_archive::repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("ha_archive::repair");
  check_opt->flags= T_EXTEND;
  int rc= optimize(thd, check_opt);

  if (rc)
    DBUG_RETURN(HA_ERR_CRASHED_ON_REPAIR);

  share->crashed= FALSE;
  DBUG_RETURN(0);
}

/*
  The table can become fragmented if data was inserted, read, and then
  inserted again. What we do is open up the file and recompress it completely. 
*/
int ha_archive::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("ha_archive::optimize");
  int rc= 0;
  azio_stream writer;
  char writer_filename[FN_REFLEN];

  /* Open up the writer if we haven't yet */
  if (!share->archive_write_open)
    init_archive_writer();

  /* Flush any waiting data */
  azflush(&(share->archive_write), Z_SYNC_FLUSH);
  share->forced_flushes++;

  /* Lets create a file to contain the new data */
  fn_format(writer_filename, share->table_name, "", ARN, 
            MY_REPLACE_EXT|MY_UNPACK_FILENAME);

  if (!(azopen(&writer, writer_filename, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY)))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE); 

  /* 
    An extended rebuild is a lot more effort. We open up each row and re-record it. 
    Any dead rows are removed (aka rows that may have been partially recorded). 
  */

  if (check_opt->flags == T_EXTEND)
  {
    DBUG_PRINT("info", ("archive extended rebuild"));
    byte *buf; 

    /* 
      First we create a buffer that we can use for reading rows, and can pass
      to get_row().
    */
    if (!(buf= (byte*) my_malloc(table->s->reclength, MYF(MY_WME))))
    {
      rc= HA_ERR_OUT_OF_MEM;
      goto error;
    }

    /*
      Now we will rewind the archive file so that we are positioned at the 
      start of the file.
    */
    rc= read_data_header(&archive);
    
    /*
      Assuming now error from rewinding the archive file, we now write out the 
      new header for out data file.
    */
    if (!rc)
      rc= write_data_header(&writer);

    /* 
      On success of writing out the new header, we now fetch each row and
      insert it into the new archive file. 
    */
    if (!rc)
    {
      share->rows_recorded= 0;
      stats.auto_increment_value= share->auto_increment_value= 0;
      while (!(rc= get_row(&archive, buf)))
      {
        real_write_row(buf, &writer);
        if (table->found_next_number_field)
        {
          Field *field= table->found_next_number_field;
          ulonglong auto_value=
            (ulonglong) field->val_int((char*)(buf + field->offset()));
          if (share->auto_increment_value < auto_value)
            stats.auto_increment_value= share->auto_increment_value=
              auto_value;
        }
        share->rows_recorded++;
      }
    }
    DBUG_PRINT("info", ("recovered %llu archive rows", share->rows_recorded));

    my_free((char*)buf, MYF(0));
    if (rc && rc != HA_ERR_END_OF_FILE)
      goto error;
  } 
  else
  {
    DBUG_PRINT("info", ("archive quick rebuild"));
    /* 
      The quick method is to just read the data raw, and then compress it directly.
    */
    int read; // Bytes read, azread() returns int
    char block[IO_SIZE];
    if (azrewind(&archive) == -1)
    {
      rc= HA_ERR_CRASHED_ON_USAGE;
      DBUG_PRINT("info", ("archive HA_ERR_CRASHED_ON_USAGE"));
      goto error;
    }

    while ((read= azread(&archive, block, IO_SIZE)) > 0)
      azwrite(&writer, block, read);
  }

  azclose(&writer);
  share->dirty= FALSE;
  share->forced_flushes= 0;
  
  // now we close both our writer and our reader for the rename
  azclose(&(share->archive_write));
  azclose(&archive);

  // make the file we just wrote be our data file
  rc = my_rename(writer_filename,share->data_file_name,MYF(0));

  /*
    now open the shared writer back up
    we don't check rc here because we want to open the file back up even
    if the optimize failed but we will return rc below so that we will
    know it failed.
    We also need to reopen our read descriptor since it has changed.
  */
  DBUG_PRINT("info", ("Reopening archive data file"));
  if (!azopen(&(share->archive_write), share->data_file_name,
               O_WRONLY|O_APPEND|O_BINARY) ||
      !azopen(&archive, share->data_file_name, O_RDONLY|O_BINARY))
  {
    DBUG_PRINT("info", ("Could not open archive write file"));
    rc= HA_ERR_CRASHED_ON_USAGE;
  }

  DBUG_RETURN(rc);
error:
  azclose(&writer);

  DBUG_RETURN(rc); 
}

/* 
  Below is an example of how to setup row level locking.
*/
THR_LOCK_DATA **ha_archive::store_lock(THD *thd,
                                       THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type == TL_WRITE_DELAYED)
    delayed_insert= TRUE;
  else
    delayed_insert= FALSE;

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) 
  {
    /* 
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set 
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers 
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd_in_lock_tables(thd)
        && !thd_tablespace_op(thd))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /* 
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2. 
    */

    if (lock_type == TL_READ_NO_INSERT && !thd_in_lock_tables(thd)) 
      lock_type = TL_READ;

    lock.type=lock_type;
  }

  *to++= &lock;

  return to;
}

void ha_archive::update_create_info(HA_CREATE_INFO *create_info)
{
  ha_archive::info(HA_STATUS_AUTO | HA_STATUS_CONST);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  {
    create_info->auto_increment_value= stats.auto_increment_value;
  }
  if (*share->real_path)
    create_info->data_file_name= share->real_path;
}


/*
  Hints for optimizer, see ha_tina for more information
*/
void ha_archive::info(uint flag)
{
  DBUG_ENTER("ha_archive::info");
  /* 
    This should be an accurate number now, though bulk and delayed inserts can
    cause the number to be inaccurate.
  */
  stats.records= share->rows_recorded;
  stats.deleted= 0;
  /* Costs quite a bit more to get all information */
  if (flag & HA_STATUS_TIME)
  {
    MY_STAT file_stat;  // Stat information for the data file

    VOID(my_stat(share->data_file_name, &file_stat, MYF(MY_WME)));

    stats.mean_rec_length= table->s->reclength + buffer.alloced_length();
    stats.data_file_length= file_stat.st_size;
    stats.create_time= file_stat.st_ctime;
    stats.update_time= file_stat.st_mtime;
    stats.max_data_file_length= share->rows_recorded * stats.mean_rec_length;
  }
  stats.delete_length= 0;
  stats.index_file_length=0;

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= share->auto_increment_value;

  DBUG_VOID_RETURN;
}


/*
  This method tells us that a bulk insert operation is about to occur. We set
  a flag which will keep write_row from saying that its data is dirty. This in
  turn will keep selects from causing a sync to occur.
  Basically, yet another optimizations to keep compression working well.
*/
void ha_archive::start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("ha_archive::start_bulk_insert");
  if (!rows || rows >= ARCHIVE_MIN_ROWS_TO_USE_BULK_INSERT)
    bulk_insert= TRUE;
  DBUG_VOID_RETURN;
}


/* 
  Other side of start_bulk_insert, is end_bulk_insert. Here we turn off the bulk insert
  flag, and set the share dirty so that the next select will call sync for us.
*/
int ha_archive::end_bulk_insert()
{
  DBUG_ENTER("ha_archive::end_bulk_insert");
  bulk_insert= FALSE;
  share->dirty= TRUE;
  DBUG_RETURN(0);
}

/*
  We cancel a truncate command. The only way to delete an archive table is to drop it.
  This is done for security reasons. In a later version we will enable this by 
  allowing the user to select a different row format.
*/
int ha_archive::delete_all_rows()
{
  DBUG_ENTER("ha_archive::delete_all_rows");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

/*
  We just return state if asked.
*/
bool ha_archive::is_crashed() const 
{
  DBUG_ENTER("ha_archive::is_crashed");
  DBUG_RETURN(share->crashed); 
}

/*
  Simple scan of the tables to make sure everything is ok.
*/

int ha_archive::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  int rc= 0;
  byte *buf; 
  const char *old_proc_info;
  ha_rows count= share->rows_recorded;
  DBUG_ENTER("ha_archive::check");

  old_proc_info= thd_proc_info(thd, "Checking table");
  /* Flush any waiting data */
  azflush(&(share->archive_write), Z_SYNC_FLUSH);
  share->forced_flushes++;

  /* 
    First we create a buffer that we can use for reading rows, and can pass
    to get_row().
  */
  if (!(buf= (byte*) my_malloc(table->s->reclength, MYF(MY_WME))))
    rc= HA_ERR_OUT_OF_MEM;

  /*
    Now we will rewind the archive file so that we are positioned at the 
    start of the file.
  */
  if (!rc)
    read_data_header(&archive);

  if (!rc)
    while (!(rc= get_row(&archive, buf)))
      count--;

  my_free((char*)buf, MYF(0));

  thd_proc_info(thd, old_proc_info);

  if ((rc && rc != HA_ERR_END_OF_FILE) || count)  
  {
    share->crashed= FALSE;
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  }
  else
  {
    DBUG_RETURN(HA_ADMIN_OK);
  }
}

/*
  Check and repair the table if needed.
*/
bool ha_archive::check_and_repair(THD *thd) 
{
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_archive::check_and_repair");

  check_opt.init();

  DBUG_RETURN(repair(thd, &check_opt));
}

struct st_mysql_storage_engine archive_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION, &archive_hton };

mysql_declare_plugin(archive)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &archive_storage_engine,
  "ARCHIVE",
  "Brian Aker, MySQL AB",
  "Archive storage engine",
  archive_db_init, /* Plugin Init */
  archive_db_done, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;

