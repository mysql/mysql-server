/*
   Copyright (c) 2004, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql_priv.h"
#include "probes_mysql.h"
#include "sql_class.h"                          // SSV
#include "sql_table.h"
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
  complete table scans. Compression is done through a combination of packing
  and making use of the zlib library
  
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
   Allow users to set compression level.
   Allow adjustable block size.
   Implement versioning, should be easy.
   Allow for errors, find a way to mark bad rows.
   Add optional feature so that rows can be flushed at interval (which will cause less
     compression but may speed up ordered searches).
   Checkpoint the meta file to allow for faster rebuilds.
   Option to allow for dirty reads, this would lower the sync calls, which would make
     inserts a lot faster, but would mean highly arbitrary reads.

    -Brian

  Archive file format versions:
  <5.1.5 - v.1
  5.1.5-5.1.15 - v.2
  >5.1.15 - v.3
*/

/* The file extension */
#define ARZ ".ARZ"               // The data file
#define ARN ".ARN"               // Files used during an optimize call
#define ARM ".ARM"               // Meta file (deprecated)

/* 5.0 compatibility */
#define META_V1_OFFSET_CHECK_HEADER  0
#define META_V1_OFFSET_VERSION       1
#define META_V1_OFFSET_ROWS_RECORDED 2
#define META_V1_OFFSET_CHECK_POINT   10
#define META_V1_OFFSET_CRASHED       18
#define META_V1_LENGTH               19

/*
  uchar + uchar
*/
#define DATA_BUFFER_SIZE 2       // Size of the data used in the data file
#define ARCHIVE_CHECK_HEADER 254 // The number we use to determine corruption

#ifdef HAVE_PSI_INTERFACE
extern "C" PSI_file_key arch_key_file_data;
#endif

/* Static declarations for handerton */
static handler *archive_create_handler(handlerton *hton, 
                                       TABLE_SHARE *table, 
                                       MEM_ROOT *mem_root);
int archive_discover(handlerton *hton, THD* thd, const char *db, 
                     const char *name,
                     uchar **frmblob, 
                     size_t *frmlen);

/*
  Number of rows that will force a bulk insert.
*/
#define ARCHIVE_MIN_ROWS_TO_USE_BULK_INSERT 2

/*
  Size of header used for row
*/
#define ARCHIVE_ROW_HEADER_SIZE 4

static handler *archive_create_handler(handlerton *hton,
                                       TABLE_SHARE *table, 
                                       MEM_ROOT *mem_root)
{
  return new (mem_root) ha_archive(hton, table);
}


#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key az_key_mutex_Archive_share_mutex;

static PSI_mutex_info all_archive_mutexes[]=
{
  { &az_key_mutex_Archive_share_mutex, "Archive_share::mutex", 0}
};

PSI_file_key arch_key_file_metadata, arch_key_file_data, arch_key_file_frm;
static PSI_file_info all_archive_files[]=
{
    { &arch_key_file_metadata, "metadata", 0},
    { &arch_key_file_data, "data", 0},
    { &arch_key_file_frm, "FRM", 0}
};

static void init_archive_psi_keys(void)
{
  const char* category= "archive";
  int count;

  count= array_elements(all_archive_mutexes);
  mysql_mutex_register(category, all_archive_mutexes, count);

  count= array_elements(all_archive_files);
  mysql_file_register(category, all_archive_files, count);
}


#endif /* HAVE_PSI_INTERFACE */

/*
  Initialize the archive handler.

  SYNOPSIS
    archive_db_init()
    void *

  RETURN
    FALSE       OK
    TRUE        Error
*/

int archive_db_init(void *p)
{
  DBUG_ENTER("archive_db_init");
  handlerton *archive_hton;

#ifdef HAVE_PSI_INTERFACE
  init_archive_psi_keys();
#endif

  archive_hton= (handlerton *)p;
  archive_hton->state= SHOW_OPTION_YES;
  archive_hton->db_type= DB_TYPE_ARCHIVE_DB;
  archive_hton->create= archive_create_handler;
  archive_hton->flags= HTON_NO_FLAGS;
  archive_hton->discover= archive_discover;

  DBUG_RETURN(0);
}


Archive_share::Archive_share()
{
  crashed= false;
  in_optimize= false;
  archive_write_open= false;
  dirty= false;
  DBUG_PRINT("ha_archive", ("Archive_share: %p",
                            this));
  thr_lock_init(&lock);
  /*
    We will use this lock for rows.
  */
  mysql_mutex_init(az_key_mutex_Archive_share_mutex,
                   &mutex, MY_MUTEX_INIT_FAST);
}


ha_archive::ha_archive(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg), share(NULL), bulk_insert(0)
{
  /* Set our original buffer from pre-allocated memory */
  buffer.set((char *)byte_buffer, IO_SIZE, system_charset_info);

  /* The size of the offset value we will use for position() */
  ref_length= sizeof(my_off_t);
  archive_reader_open= FALSE;
}

int archive_discover(handlerton *hton, THD* thd, const char *db, 
                     const char *name,
                     uchar **frmblob, 
                     size_t *frmlen)
{
  DBUG_ENTER("archive_discover");
  DBUG_PRINT("archive_discover", ("db: %s, name: %s", db, name)); 
  azio_stream frm_stream;
  char az_file[FN_REFLEN];
  char *frm_ptr;
  MY_STAT file_stat; 

  build_table_filename(az_file, sizeof(az_file) - 1, db, name, ARZ, 0);

  if (!(mysql_file_stat(arch_key_file_data, az_file, &file_stat, MYF(0))))
    goto err;

  if (!(azopen(&frm_stream, az_file, O_RDONLY|O_BINARY)))
  {
    if (errno == EROFS || errno == EACCES)
      DBUG_RETURN(my_errno= errno);
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  if (frm_stream.frm_length == 0)
    goto err;

  frm_ptr= (char *)my_malloc(sizeof(char) * frm_stream.frm_length, MYF(0));
  azread_frm(&frm_stream, frm_ptr);
  azclose(&frm_stream);

  *frmlen= frm_stream.frm_length;
  *frmblob= (uchar*) frm_ptr;

  DBUG_RETURN(0);
err:
  my_errno= 0;
  DBUG_RETURN(1);
}

static void save_auto_increment(TABLE *table, ulonglong *value)
{
  Field *field= table->found_next_number_field;
  ulonglong auto_value=
    (ulonglong) field->val_int(table->record[0] +
                               field->offset(table->record[0]));
  if (*value <= auto_value)
    *value= auto_value + 1;
}

/**
  @brief Read version 1 meta file (5.0 compatibility routine).

  @return Completion status
    @retval  0 Success
    @retval !0 Failure
*/

int Archive_share::read_v1_metafile()
{
  char file_name[FN_REFLEN];
  uchar buf[META_V1_LENGTH];
  File fd;
  DBUG_ENTER("Archive_share::read_v1_metafile");

  fn_format(file_name, data_file_name, "", ARM, MY_REPLACE_EXT);
  if ((fd= mysql_file_open(arch_key_file_metadata, file_name, O_RDONLY, MYF(0))) == -1)
    DBUG_RETURN(-1);

  if (mysql_file_read(fd, buf, sizeof(buf), MYF(0)) != sizeof(buf))
  {
    mysql_file_close(fd, MYF(0));
    DBUG_RETURN(-1);
  }
  
  rows_recorded= uint8korr(buf + META_V1_OFFSET_ROWS_RECORDED);
  crashed= buf[META_V1_OFFSET_CRASHED];
  mysql_file_close(fd, MYF(0));
  DBUG_RETURN(0);
}


/**
  @brief Write version 1 meta file (5.0 compatibility routine).

  @return Completion status
    @retval  0 Success
    @retval !0 Failure
*/

int Archive_share::write_v1_metafile()
{
  char file_name[FN_REFLEN];
  uchar buf[META_V1_LENGTH];
  File fd;
  DBUG_ENTER("Archive_share::write_v1_metafile");

  buf[META_V1_OFFSET_CHECK_HEADER]= ARCHIVE_CHECK_HEADER;
  buf[META_V1_OFFSET_VERSION]= 1;
  int8store(buf + META_V1_OFFSET_ROWS_RECORDED, rows_recorded);
  int8store(buf + META_V1_OFFSET_CHECK_POINT, (ulonglong) 0);
  buf[META_V1_OFFSET_CRASHED]= crashed;

  fn_format(file_name, data_file_name, "", ARM, MY_REPLACE_EXT);
  if ((fd= mysql_file_open(arch_key_file_metadata, file_name, O_WRONLY, MYF(0))) == -1)
    DBUG_RETURN(-1);

  if (mysql_file_write(fd, buf, sizeof(buf), MYF(0)) != sizeof(buf))
  {
    mysql_file_close(fd, MYF(0));
    DBUG_RETURN(-1);
  }
  
  mysql_file_close(fd, MYF(0));
  DBUG_RETURN(0);
}


/**
  @brief Pack version 1 row (5.0 compatibility routine).

  @param[in]  record  the record to pack

  @return Length of packed row
*/

unsigned int ha_archive::pack_row_v1(uchar *record)
{
  uint *blob, *end;
  uchar *pos;
  DBUG_ENTER("pack_row_v1");
  memcpy(record_buffer->buffer, record, table->s->reclength);
  pos= record_buffer->buffer + table->s->reclength;
  for (blob= table->s->blob_field, end= blob + table->s->blob_fields;
       blob != end; blob++)
  {
    uint32 length= ((Field_blob *) table->field[*blob])->get_length();
    if (length)
    {
      uchar *data_ptr;
      ((Field_blob *) table->field[*blob])->get_ptr(&data_ptr);
      memcpy(pos, data_ptr, length);
      pos+= length;
    }
  }
  DBUG_RETURN(pos - record_buffer->buffer);
}


/*
  This method reads the header of a datafile and returns whether or not it was successful.
*/
int ha_archive::read_data_header(azio_stream *file_to_read)
{
  int error;
  unsigned long ret;
  uchar data_buffer[DATA_BUFFER_SIZE];
  DBUG_ENTER("ha_archive::read_data_header");

  if (azrewind(file_to_read) == -1)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  if (file_to_read->version >= 3)
    DBUG_RETURN(0);
  /* Everything below this is just legacy to version 2< */

  DBUG_PRINT("ha_archive", ("Reading legacy data header"));

  ret= azread(file_to_read, data_buffer, DATA_BUFFER_SIZE, &error);

  if (ret != DATA_BUFFER_SIZE)
  {
    DBUG_PRINT("ha_archive", ("Reading, expected %d got %lu", 
                              DATA_BUFFER_SIZE, ret));
    DBUG_RETURN(1);
  }

  if (error)
  {
    DBUG_PRINT("ha_archive", ("Compression error (%d)", error));
    DBUG_RETURN(1);
  }
  
  DBUG_PRINT("ha_archive", ("Check %u", data_buffer[0]));
  DBUG_PRINT("ha_archive", ("Version %u", data_buffer[1]));

  if ((data_buffer[0] != (uchar)ARCHIVE_CHECK_HEADER) &&  
      (data_buffer[1] != (uchar)ARCHIVE_VERSION))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  DBUG_RETURN(0);
}


/*
  We create the shared memory space that we will use for the open table. 
  No matter what we try to get or create a share. This is so that a repair
  table operation can occur.

  See ha_example.cc for a longer description.
*/
Archive_share *ha_archive::get_share(const char *table_name, int *rc)
{
  Archive_share *tmp_share;

  DBUG_ENTER("ha_archive::get_share");

  lock_shared_ha_data();
  if (!(tmp_share= static_cast<Archive_share*>(get_ha_share_ptr())))
  {
    azio_stream archive_tmp;

    tmp_share= new Archive_share;

    if (!tmp_share)
    {
      *rc= HA_ERR_OUT_OF_MEM;
      goto err;
    }
    DBUG_PRINT("ha_archive", ("new Archive_share: %p",
                              tmp_share));

    fn_format(tmp_share->data_file_name, table_name, "",
              ARZ, MY_REPLACE_EXT | MY_UNPACK_FILENAME);
    strmov(tmp_share->table_name, table_name);
    DBUG_PRINT("ha_archive", ("Data File %s",
                        tmp_share->data_file_name));

    /*
      We read the meta file, but do not mark it dirty. Since we are not
      doing a write we won't mark it dirty (and we won't open it for
      anything but reading... open it for write and we will generate null
      compression writes).
    */
    if (!(azopen(&archive_tmp, tmp_share->data_file_name, O_RDONLY|O_BINARY)))
    {
      delete tmp_share;
      *rc= my_errno ? my_errno : HA_ERR_CRASHED;
      tmp_share= NULL;
      goto err;
    }
    stats.auto_increment_value= archive_tmp.auto_increment + 1;
    tmp_share->rows_recorded= (ha_rows)archive_tmp.rows;
    tmp_share->crashed= archive_tmp.dirty;
    share= tmp_share;
    if (archive_tmp.version == 1)
      share->read_v1_metafile();
    azclose(&archive_tmp);

    set_ha_share_ptr(static_cast<Handler_share*>(tmp_share));
  }
  if (tmp_share->crashed)
    *rc= HA_ERR_CRASHED_ON_USAGE;
err:
  unlock_shared_ha_data();

  DBUG_ASSERT(tmp_share || *rc);

  DBUG_RETURN(tmp_share);
}


int Archive_share::init_archive_writer()
{
  DBUG_ENTER("Archive_share::init_archive_writer");
  /*
    It is expensive to open and close the data files and since you can't have
    a gzip file that can be both read and written we keep a writer open
    that is shared amoung all open tables.
  */
  if (!(azopen(&archive_write, data_file_name,
               O_RDWR|O_BINARY)))
  {
    DBUG_PRINT("ha_archive", ("Could not open archive write file"));
    crashed= true;
    DBUG_RETURN(1);
  }
  archive_write_open= true;

  DBUG_RETURN(0);
}


void Archive_share::close_archive_writer()
{
  mysql_mutex_assert_owner(&mutex);
  if (archive_write_open)
  {
    if (archive_write.version == 1)
      (void) write_v1_metafile();
    azclose(&archive_write);
    archive_write_open= false;
    dirty= false;
  }
}


/*
  No locks are required because it is associated with just one handler instance
*/
int ha_archive::init_archive_reader()
{
  DBUG_ENTER("ha_archive::init_archive_reader");
  /*
    It is expensive to open and close the data files and since you can't have
    a gzip file that can be both read and written we keep a writer open
    that is shared amoung all open tables, but have one reader open for
    each handler instance.
  */
  if (!archive_reader_open)
  {
    if (!(azopen(&archive, share->data_file_name, O_RDONLY|O_BINARY)))
    {
      DBUG_PRINT("ha_archive", ("Could not open archive read file"));
      share->crashed= TRUE;
      DBUG_RETURN(1);
    }
    archive_reader_open= TRUE;
  }

  DBUG_RETURN(0);
}


/*
  We just implement one additional file extension.
*/
static const char *ha_archive_exts[] = {
  ARZ,
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

  DBUG_PRINT("ha_archive", ("archive table was opened for crash: %s",
                      (open_options & HA_OPEN_FOR_REPAIR) ? "yes" : "no"));
  share= get_share(name, &rc);
  if (!share)
    DBUG_RETURN(rc);

  /* Allow open on crashed table in repair mode only. */
  switch (rc)
  {
  case 0:
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    if (open_options & HA_OPEN_FOR_REPAIR)
      break;
    /* fall through */
  default:
    DBUG_RETURN(rc);
  }

  record_buffer= create_record_buffer(table->s->reclength + 
                                      ARCHIVE_ROW_HEADER_SIZE);

  if (!record_buffer)
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  thr_lock_data_init(&share->lock, &lock, NULL);

  DBUG_PRINT("ha_archive", ("archive table was crashed %s", 
                      rc == HA_ERR_CRASHED_ON_USAGE ? "yes" : "no"));
  if (rc == HA_ERR_CRASHED_ON_USAGE && open_options & HA_OPEN_FOR_REPAIR)
  {
    DBUG_RETURN(0);
  }

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

  destroy_record_buffer(record_buffer);

  if (archive_reader_open)
  {
    if (azclose(&archive))
      rc= 1;
  }

  DBUG_RETURN(rc);
}


void ha_archive::frm_load(const char *name, azio_stream *dst)
{
  char name_buff[FN_REFLEN];
  MY_STAT file_stat;
  File frm_file;
  uchar *frm_ptr;
  DBUG_ENTER("ha_archive::frm_load");
  fn_format(name_buff, name, "", ".frm", MY_REPLACE_EXT | MY_UNPACK_FILENAME);

  /* Here is where we open up the frm and pass it to archive to store */
  if ((frm_file= mysql_file_open(arch_key_file_frm, name_buff, O_RDONLY, MYF(0))) >= 0)
  {
    if (!mysql_file_fstat(frm_file, &file_stat, MYF(MY_WME)))
    {
      frm_ptr= (uchar *) my_malloc(sizeof(uchar) * (size_t) file_stat.st_size, MYF(0));
      if (frm_ptr)
      {
        if (mysql_file_read(frm_file, frm_ptr, (size_t) file_stat.st_size, MYF(0)) ==
            (size_t) file_stat.st_size)
          azwrite_frm(dst, (char *) frm_ptr, (size_t) file_stat.st_size);
        my_free(frm_ptr);
      }
    }
    mysql_file_close(frm_file, MYF(0));
  }
  DBUG_VOID_RETURN;
}


/**
  Copy a frm blob between streams.

  @param[in]  src   The source stream.
  @param[in]  dst   The destination stream.

  @return Zero on success, non-zero otherwise.
*/

int ha_archive::frm_copy(azio_stream *src, azio_stream *dst)
{
  int rc= 0;
  char *frm_ptr;

  /* If there is no .frm in source stream, try to read .frm from file. */
  if (!src->frm_length)
  {
    frm_load(table->s->normalized_path.str, dst);
    return 0;
  }

  if (!(frm_ptr= (char *) my_malloc(src->frm_length, MYF(0))))
    return HA_ERR_OUT_OF_MEM;

  /* Write file offset is set to the end of the file. */
  if (azread_frm(src, frm_ptr) ||
      azwrite_frm(dst, frm_ptr, src->frm_length))
    rc= my_errno ? my_errno : HA_ERR_INTERNAL_ERROR;

  my_free(frm_ptr);

  return rc;
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
  char name_buff[FN_REFLEN];
  char linkname[FN_REFLEN];
  int error;
  azio_stream create_stream;            /* Archive file we are working with */
  MY_STAT file_stat;  // Stat information for the data file

  DBUG_ENTER("ha_archive::create");

  stats.auto_increment_value= create_info->auto_increment_value;

  for (uint key= 0; key < table_arg->s->keys; key++)
  {
    KEY *pos= table_arg->key_info+key;
    KEY_PART_INFO *key_part=     pos->key_part;
    KEY_PART_INFO *key_part_end= key_part + pos->user_defined_key_parts;

    for (; key_part != key_part_end; key_part++)
    {
      Field *field= key_part->field;

      if (!(field->flags & AUTO_INCREMENT_FLAG))
      {
        error= -1;
        DBUG_PRINT("ha_archive", ("Index error in creating archive table"));
        goto error;
      }
    }
  }

  /* 
    We reuse name_buff since it is available.
  */
#ifdef HAVE_READLINK
  if (my_enable_symlinks &&
      create_info->data_file_name &&
      create_info->data_file_name[0] != '#')
  {
    DBUG_PRINT("ha_archive", ("archive will create stream file %s", 
                        create_info->data_file_name));
                        
    fn_format(name_buff, create_info->data_file_name, "", ARZ,
              MY_REPLACE_EXT | MY_UNPACK_FILENAME);
    fn_format(linkname, name, "", ARZ,
              MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  }
  else
#endif /* HAVE_READLINK */
  {
    if (create_info->data_file_name)
    {
      push_warning_printf(table_arg->in_use, Sql_condition::SL_WARNING,
                          WARN_OPTION_IGNORED,
                          ER_DEFAULT(WARN_OPTION_IGNORED),
                          "DATA DIRECTORY");
    }
    fn_format(name_buff, name, "", ARZ,
              MY_REPLACE_EXT | MY_UNPACK_FILENAME);
    linkname[0]= 0;
  }

  /* Archive engine never uses INDEX DIRECTORY. */
  if (create_info->index_file_name)
  {
    push_warning_printf(table_arg->in_use, Sql_condition::SL_WARNING,
                        WARN_OPTION_IGNORED,
                        ER_DEFAULT(WARN_OPTION_IGNORED),
                        "INDEX DIRECTORY");
  }

  /*
    There is a chance that the file was "discovered". In this case
    just use whatever file is there.
  */
  if (!(mysql_file_stat(arch_key_file_data, name_buff, &file_stat, MYF(0))))
  {
    my_errno= 0;
    if (!(azopen(&create_stream, name_buff, O_CREAT|O_RDWR|O_BINARY)))
    {
      error= errno;
      goto error2;
    }

    if (linkname[0])
      my_symlink(name_buff, linkname, MYF(0));

    frm_load(name, &create_stream);

    if (create_info->comment.str)
      azwrite_comment(&create_stream, create_info->comment.str, 
                      create_info->comment.length);

    /* 
      Yes you need to do this, because the starting value 
      for the autoincrement may not be zero.
    */
    create_stream.auto_increment= stats.auto_increment_value ?
                                    stats.auto_increment_value - 1 : 0;
    if (azclose(&create_stream))
    {
      error= errno;
      goto error2;
    }
  }
  else
    my_errno= 0;

  DBUG_PRINT("ha_archive", ("Creating File %s", name_buff));
  DBUG_PRINT("ha_archive", ("Creating Link %s", linkname));


  DBUG_RETURN(0);

error2:
  delete_table(name);
error:
  /* Return error number, if we got one */
  DBUG_RETURN(error ? error : -1);
}

/*
  This is where the actual row is written out.
*/
int ha_archive::real_write_row(uchar *buf, azio_stream *writer)
{
  my_off_t written;
  unsigned int r_pack_length;
  DBUG_ENTER("ha_archive::real_write_row");

  /* We pack the row for writing */
  r_pack_length= pack_row(buf, writer);

  written= azwrite(writer, record_buffer->buffer, r_pack_length);
  if (written != r_pack_length)
  {
    DBUG_PRINT("ha_archive", ("Wrote %d bytes expected %d", 
                                              (uint32) written, 
                                              (uint32)r_pack_length));
    DBUG_RETURN(-1);
  }

  if (!bulk_insert)
    share->dirty= TRUE;

  DBUG_RETURN(0);
}


/* 
  Calculate max length needed for row. This includes
  the bytes required for the length in the header.
*/

uint32 ha_archive::max_row_length(const uchar *buf)
{
  uint32 length= (uint32)(table->s->reclength + table->s->fields*2);
  length+= ARCHIVE_ROW_HEADER_SIZE;

  uint *ptr, *end;
  for (ptr= table->s->blob_field, end=ptr + table->s->blob_fields ;
       ptr != end ;
       ptr++)
  {
    if (!table->field[*ptr]->is_null())
      length += 2 + ((Field_blob*)table->field[*ptr])->get_length();
  }

  return length;
}


unsigned int ha_archive::pack_row(uchar *record, azio_stream *writer)
{
  uchar *ptr;

  DBUG_ENTER("ha_archive::pack_row");


  if (fix_rec_buff(max_row_length(record)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM); /* purecov: inspected */

  if (writer->version == 1)
    DBUG_RETURN(pack_row_v1(record));

  /* Copy null bits */
  memcpy(record_buffer->buffer+ARCHIVE_ROW_HEADER_SIZE, 
         record, table->s->null_bytes);
  ptr= record_buffer->buffer + table->s->null_bytes + ARCHIVE_ROW_HEADER_SIZE;

  for (Field **field=table->field ; *field ; field++)
  {
    if (!((*field)->is_null()))
      ptr= (*field)->pack(ptr, record + (*field)->offset(record));
  }

  int4store(record_buffer->buffer, (int)(ptr - record_buffer->buffer -
                                         ARCHIVE_ROW_HEADER_SIZE)); 
  DBUG_PRINT("ha_archive",("Pack row length %u", (unsigned int)
                           (ptr - record_buffer->buffer - 
                             ARCHIVE_ROW_HEADER_SIZE)));

  DBUG_RETURN((unsigned int) (ptr - record_buffer->buffer));
}


/* 
  Look at ha_archive::open() for an explanation of the row format.
  Here we just write out the row.

  Wondering about start_bulk_insert()? We don't implement it for
  archive since it optimizes for lots of writes. The only save
  for implementing start_bulk_insert() is that we could skip 
  setting dirty to true each time.
*/
int ha_archive::write_row(uchar *buf)
{
  int rc;
  uchar *read_buf= NULL;
  ulonglong temp_auto;
  uchar *record=  table->record[0];
  DBUG_ENTER("ha_archive::write_row");

  if (share->crashed)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  ha_statistic_increment(&SSV::ha_write_count);
  mysql_mutex_lock(&share->mutex);

  if (!share->archive_write_open && share->init_archive_writer())
  {
    rc= HA_ERR_CRASHED_ON_USAGE;
    goto error;
  }

  if (table->next_number_field && record == table->record[0])
  {
    KEY *mkey= &table->s->key_info[0]; // We only support one key right now
    update_auto_increment();
    temp_auto= (((Field_num*) table->next_number_field)->unsigned_flag ||
                table->next_number_field->val_int() > 0 ?
                table->next_number_field->val_int() : 0);

    /*
      We don't support decremening auto_increment. They make the performance
      just cry.
    */
    if (temp_auto <= share->archive_write.auto_increment && 
        mkey->flags & HA_NOSAME)
    {
      rc= HA_ERR_FOUND_DUPP_KEY;
      goto error;
    }
#ifdef DEAD_CODE
    /*
      Bad news, this will cause a search for the unique value which is very 
      expensive since we will have to do a table scan which will lock up 
      all other writers during this period. This could perhaps be optimized 
      in the future.
    */
    {
      /* 
        First we create a buffer that we can use for reading rows, and can pass
        to get_row().
      */
      if (!(read_buf= (uchar*) my_malloc(table->s->reclength, MYF(MY_WME))))
      {
        rc= HA_ERR_OUT_OF_MEM;
        goto error;
      }
       /* 
         All of the buffer must be written out or we won't see all of the
         data 
       */
      azflush(&(share->archive_write), Z_SYNC_FLUSH);
      /*
        Set the position of the local read thread to the beginning position.
      */
      if (read_data_header(&archive))
      {
        rc= HA_ERR_CRASHED_ON_USAGE;
        goto error;
      }

      Field *mfield= table->next_number_field;

      while (!(get_row(&archive, read_buf)))
      {
        if (!memcmp(read_buf + mfield->offset(record),
                    table->next_number_field->ptr,
                    mfield->max_display_length()))
        {
          rc= HA_ERR_FOUND_DUPP_KEY;
          goto error;
        }
      }
    }
#endif
    else
    {
      if (temp_auto > share->archive_write.auto_increment)
        stats.auto_increment_value=
          (share->archive_write.auto_increment= temp_auto) + 1;
    }
  }

  /*
    Notice that the global auto_increment has been increased.
    In case of a failed row write, we will never try to reuse the value.
  */
  share->rows_recorded++;
  rc= real_write_row(buf,  &(share->archive_write));
error:
  mysql_mutex_unlock(&share->mutex);
  if (read_buf)
    my_free(read_buf);
  DBUG_RETURN(rc);
}


void ha_archive::get_auto_increment(ulonglong offset, ulonglong increment,
                                    ulonglong nb_desired_values,
                                    ulonglong *first_value,
                                    ulonglong *nb_reserved_values)
{
  *nb_reserved_values= ULONGLONG_MAX;
  *first_value= share->archive_write.auto_increment + 1;
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
int ha_archive::index_read(uchar *buf, const uchar *key,
                             uint key_len, enum ha_rkey_function find_flag)
{
  int rc;
  DBUG_ENTER("ha_archive::index_read");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc= index_read_idx(buf, active_index, key, key_len, find_flag);
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}


int ha_archive::index_read_idx(uchar *buf, uint index, const uchar *key,
                                 uint key_len, enum ha_rkey_function find_flag)
{
  int rc;
  bool found= 0;
  KEY *mkey= &table->s->key_info[index];
  current_k_offset= mkey->key_part->offset;
  current_key= key;
  current_key_len= key_len;


  DBUG_ENTER("ha_archive::index_read_idx");

  rc= rnd_init(TRUE);

  if (rc)
    goto error;

  while (!(get_row(&archive, buf)))
  {
    if (!memcmp(current_key, buf + current_k_offset, current_key_len))
    {
      found= 1;
      break;
    }
  }

  if (found)
  {
    /* notify handler that a record has been found */
    table->status= 0;
    DBUG_RETURN(0);
  }

error:
  DBUG_RETURN(rc ? rc : HA_ERR_END_OF_FILE);
}


int ha_archive::index_next(uchar * buf) 
{ 
  bool found= 0;
  int rc;

  DBUG_ENTER("ha_archive::index_next");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);

  while (!(get_row(&archive, buf)))
  {
    if (!memcmp(current_key, buf+current_k_offset, current_key_len))
    {
      found= 1;
      break;
    }
  }

  rc= found ? 0 : HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
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

  init_archive_reader();

  /* We rewind the file so that we can read from the beginning if scan */
  if (scan)
  {
    scan_rows= stats.records;
    DBUG_PRINT("info", ("archive will retrieve %llu rows", 
                        (unsigned long long) scan_rows));

    if (read_data_header(&archive))
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  DBUG_RETURN(0);
}


/*
  This is the method that is used to read a row. It assumes that the row is 
  positioned where you want it.
*/
int ha_archive::get_row(azio_stream *file_to_read, uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_archive::get_row");
  DBUG_PRINT("ha_archive", ("Picking version for get_row() %d -> %d", 
                            (uchar)file_to_read->version, 
                            ARCHIVE_VERSION));
  if (file_to_read->version == ARCHIVE_VERSION)
    rc= get_row_version3(file_to_read, buf);
  else
    rc= get_row_version2(file_to_read, buf);

  DBUG_PRINT("ha_archive", ("Return %d\n", rc));

  DBUG_RETURN(rc);
}

/* Reallocate buffer if needed */
bool ha_archive::fix_rec_buff(unsigned int length)
{
  DBUG_ENTER("ha_archive::fix_rec_buff");
  DBUG_PRINT("ha_archive", ("Fixing %u for %u", 
                            length, record_buffer->length));
  DBUG_ASSERT(record_buffer->buffer);

  if (length > record_buffer->length)
  {
    uchar *newptr;
    if (!(newptr=(uchar*) my_realloc((uchar*) record_buffer->buffer, 
                                    length,
				    MYF(MY_ALLOW_ZERO_PTR))))
      DBUG_RETURN(1);
    record_buffer->buffer= newptr;
    record_buffer->length= length;
  }

  DBUG_ASSERT(length <= record_buffer->length);

  DBUG_RETURN(0);
}

int ha_archive::unpack_row(azio_stream *file_to_read, uchar *record)
{
  DBUG_ENTER("ha_archive::unpack_row");

  unsigned int read;
  int error;
  uchar size_buffer[ARCHIVE_ROW_HEADER_SIZE], *size_buffer_p= size_buffer;
  unsigned int row_len;

  /* First we grab the length stored */
  read= azread(file_to_read, size_buffer, ARCHIVE_ROW_HEADER_SIZE, &error);

  if (error == Z_STREAM_ERROR ||  (read && read < ARCHIVE_ROW_HEADER_SIZE))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /* If we read nothing we are at the end of the file */
  if (read == 0 || read != ARCHIVE_ROW_HEADER_SIZE)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  row_len= uint4korr(size_buffer_p);
  DBUG_PRINT("ha_archive",("Unpack row length %u -> %u", row_len, 
                           (unsigned int)table->s->reclength));

  if (fix_rec_buff(row_len))
  {
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }
  DBUG_ASSERT(row_len <= record_buffer->length);

  read= azread(file_to_read, record_buffer->buffer, row_len, &error);

  if (read != row_len || error)
  {
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  /* Copy null bits */
  const uchar *ptr= record_buffer->buffer;
  /*
    Field::unpack() is not called when field is NULL. For VARCHAR
    Field::unpack() only unpacks as much bytes as occupied by field
    value. In these cases respective memory area on record buffer is
    not initialized.

    These uninitialized areas may be accessed by CHECKSUM TABLE or
    by optimizer using temporary table (BUG#12997905). We may remove
    this memset() when they're fixed.
  */
  memset(record, 0, table->s->reclength);
  memcpy(record, ptr, table->s->null_bytes);
  ptr+= table->s->null_bytes;
  for (Field **field=table->field ; *field ; field++)
  {
    if (!((*field)->is_null_in_record(record)))
    {
      ptr= (*field)->unpack(record + (*field)->offset(table->record[0]), ptr);
    }
  }
  DBUG_RETURN(0);
}


int ha_archive::get_row_version3(azio_stream *file_to_read, uchar *buf)
{
  DBUG_ENTER("ha_archive::get_row_version3");

  int returnable= unpack_row(file_to_read, buf);

  DBUG_RETURN(returnable);
}


int ha_archive::get_row_version2(azio_stream *file_to_read, uchar *buf)
{
  unsigned int read;
  int error;
  uint *ptr, *end;
  char *last;
  size_t total_blob_length= 0;
  MY_BITMAP *read_set= table->read_set;
  DBUG_ENTER("ha_archive::get_row_version2");

  read= azread(file_to_read, (voidp)buf, table->s->reclength, &error);

  /* If we read nothing we are at the end of the file */
  if (read == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  if (read != table->s->reclength)
  {
    DBUG_PRINT("ha_archive::get_row_version2", ("Read %u bytes expected %u", 
                                                read, 
                                                (unsigned int)table->s->reclength));
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  if (error == Z_STREAM_ERROR || error == Z_DATA_ERROR )
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  /* If the record is the wrong size, the file is probably damaged. */
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
        read= azread(file_to_read, last, size, &error);

        if (error)
          DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

        if ((size_t) read != size)
          DBUG_RETURN(HA_ERR_END_OF_FILE);
        ((Field_blob*) table->field[*ptr])->set_ptr(size, (uchar*) last);
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

int ha_archive::rnd_next(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_archive::rnd_next");
  MYSQL_READ_ROW_START(table_share->db.str,
                       table_share->table_name.str, TRUE);

  if (share->crashed)
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

  if (!scan_rows)
  {
    rc= HA_ERR_END_OF_FILE;
    goto end;
  }
  scan_rows--;

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  current_position= aztell(&archive);
  rc= get_row(&archive, buf);

  table->status=rc ? STATUS_NOT_FOUND: 0;

end:
  MYSQL_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}


/*
  Thanks to the table flag HA_REC_NOT_IN_SEQ this will be called after
  each call to ha_archive::rnd_next() if an ordering of the rows is
  needed.
*/

void ha_archive::position(const uchar *record)
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

int ha_archive::rnd_pos(uchar * buf, uchar *pos)
{
  int rc;
  DBUG_ENTER("ha_archive::rnd_pos");
  MYSQL_READ_ROW_START(table_share->db.str,
                       table_share->table_name.str, FALSE);
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  current_position= (my_off_t)my_get_ptr(pos, ref_length);
  if (azseek(&archive, current_position, SEEK_SET) == (my_off_t)(-1L))
  {
    rc= HA_ERR_CRASHED_ON_USAGE;
    goto end;
  }
  rc= get_row(&archive, buf);
end:
  MYSQL_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

/*
  This method repairs the meta file. It does this by walking the datafile and 
  rewriting the meta file. If EXTENDED repair is requested, we attempt to
  recover as much data as possible.
*/
int ha_archive::repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("ha_archive::repair");
  int rc= optimize(thd, check_opt);

  if (rc)
    DBUG_RETURN(HA_ADMIN_CORRUPT);

  share->crashed= FALSE;
  DBUG_RETURN(0);
}

/*
  The table can become fragmented if data was inserted, read, and then
  inserted again. What we do is open up the file and recompress it completely. 
*/
int ha_archive::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  int rc= 0;
  azio_stream writer;
  ha_rows count;
  my_bitmap_map *org_bitmap;
  char writer_filename[FN_REFLEN];
  DBUG_ENTER("ha_archive::optimize");

  mysql_mutex_lock(&share->mutex);
  if (share->in_optimize)
  {
    mysql_mutex_unlock(&share->mutex);
    DBUG_RETURN(HA_ADMIN_FAILED);
  }
  share->in_optimize= true;
  /* remember the number of rows */
  count= share->rows_recorded;
  if (share->archive_write_open)
    azflush(&share->archive_write, Z_SYNC_FLUSH);
  mysql_mutex_unlock(&share->mutex);

  init_archive_reader();

  /* Lets create a file to contain the new data */
  fn_format(writer_filename, share->table_name, "", ARN, 
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);

  if (!(azopen(&writer, writer_filename, O_CREAT|O_RDWR|O_BINARY)))
  {
    share->in_optimize= false;
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
  }

  /*
    Transfer the embedded FRM so that the file can be discoverable.
    Write file offset is set to the end of the file.
  */
  if ((rc= frm_copy(&archive, &writer)))
  {
    share->in_optimize= false;
    goto error;
  }
  /* 
    An extended rebuild is a lot more effort. We open up each row and re-record it. 
    Any dead rows are removed (aka rows that may have been partially recorded). 

    As of Archive format 3, this is the only type that is performed, before this
    version it was just done on T_EXTEND
  */

  DBUG_PRINT("ha_archive", ("archive extended rebuild"));

  /*
    Now we will rewind the archive file so that we are positioned at the 
    start of the file.
  */
  if ((rc= read_data_header(&archive)))
  {
    share->in_optimize= false;
    goto error;
  }

  stats.auto_increment_value= 1;
  org_bitmap= tmp_use_all_columns(table, table->read_set);
  /* read rows upto the remembered rows */ 
  for (ha_rows cur_count= count; cur_count; cur_count--)
  {
    if ((rc= get_row(&archive, table->record[0])))
      break;
    real_write_row(table->record[0], &writer);
    if (table->found_next_number_field)
      save_auto_increment(table, &stats.auto_increment_value);
  }

  mysql_mutex_lock(&share->mutex);

  share->close_archive_writer();
  if (!rc)
  {
    /* read the remaining rows */
    for (count= share->rows_recorded - count; count; count--)
    {
      if ((rc= get_row(&archive, table->record[0])))
        break;
      real_write_row(table->record[0], &writer);
      if (table->found_next_number_field)
        save_auto_increment(table, &stats.auto_increment_value);
    }
  }

  tmp_restore_column_map(table->read_set, org_bitmap);
  share->rows_recorded= (ha_rows) writer.rows;
  share->archive_write.auto_increment= stats.auto_increment_value - 1;
  DBUG_PRINT("info", ("recovered %llu archive rows", 
                      (unsigned long long)share->rows_recorded));

  DBUG_PRINT("ha_archive", ("recovered %llu archive rows", 
                      (unsigned long long)share->rows_recorded));

  /*
    If REPAIR ... EXTENDED is requested, try to recover as much data
    from data file as possible. In this case if we failed to read a
    record, we assume EOF. This allows massive data loss, but we can
    hardly do more with broken zlib stream. And this is the only way
    to restore at least what is still recoverable.
  */
  if (rc && rc != HA_ERR_END_OF_FILE && !(check_opt->flags & T_EXTEND))
  {
    share->in_optimize= false;
    mysql_mutex_unlock(&share->mutex);
    goto error;
  }

  azclose(&writer);
  share->dirty= FALSE;
  azclose(&archive);
  archive_reader_open= FALSE;

  // make the file we just wrote be our data file
  rc= my_rename(writer_filename, share->data_file_name, MYF(0));
  share->in_optimize= false;
  mysql_mutex_unlock(&share->mutex);

  DBUG_RETURN(rc);
error:
  DBUG_PRINT("ha_archive", ("Failed to recover, error was %d", rc));
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
  char tmp_real_path[FN_REFLEN];
  DBUG_ENTER("ha_archive::update_create_info");

  ha_archive::info(HA_STATUS_AUTO);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  {
    create_info->auto_increment_value= stats.auto_increment_value;
  }

  if (!(my_readlink(tmp_real_path, share->data_file_name, MYF(0))))
    create_info->data_file_name= sql_strdup(tmp_real_path);

  DBUG_VOID_RETURN;
}


/*
  Hints for optimizer, see ha_tina for more information
*/
int ha_archive::info(uint flag)
{
  DBUG_ENTER("ha_archive::info");

  mysql_mutex_lock(&share->mutex);
  if (share->dirty)
  {
    DBUG_PRINT("ha_archive", ("archive flushing out rows for scan"));
    DBUG_ASSERT(share->archive_write_open);
    azflush(&(share->archive_write), Z_SYNC_FLUSH);
    share->dirty= FALSE;
  }

  /* 
    This should be an accurate number now, though bulk inserts can
    cause the number to be inaccurate.
  */
  stats.records= share->rows_recorded;
  mysql_mutex_unlock(&share->mutex);

  stats.deleted= 0;

  DBUG_PRINT("ha_archive", ("Stats rows is %d\n", (int)stats.records));
  /* Costs quite a bit more to get all information */
  if (flag & (HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE))
  {
    MY_STAT file_stat;  // Stat information for the data file

    (void) mysql_file_stat(arch_key_file_data, share->data_file_name, &file_stat, MYF(MY_WME));

    if (flag & HA_STATUS_TIME)
      stats.update_time= (ulong) file_stat.st_mtime;
    if (flag & HA_STATUS_CONST)
    {
      stats.max_data_file_length= share->rows_recorded * stats.mean_rec_length;
      stats.max_data_file_length= MAX_FILE_SIZE;
      stats.create_time= (ulong) file_stat.st_ctime;
    }
    if (flag & HA_STATUS_VARIABLE)
    {
      stats.delete_length= 0;
      stats.data_file_length= file_stat.st_size;
      stats.index_file_length=0;
      stats.mean_rec_length= stats.records ?
        ulong(stats.data_file_length / stats.records) : table->s->reclength;
    }
  }

  if (flag & HA_STATUS_AUTO)
  {
    /* TODO: Use the shared writer instead during the lock above. */
    init_archive_reader();
    mysql_mutex_lock(&share->mutex);
    azflush(&archive, Z_SYNC_FLUSH);
    mysql_mutex_unlock(&share->mutex);
    stats.auto_increment_value= archive.auto_increment + 1;
  }

  DBUG_RETURN(0);
}


/**
  Handler hints.

  @param operation  Operation to prepare for.

  @return Operation status
    @return 0    Success
    @return != 0 Error
*/

int ha_archive::extra(enum ha_extra_function operation)
{
  int ret= 0;
  DBUG_ENTER("ha_archive::extra");
  /* On windows we need to close all files before rename/delete. */
#ifdef __WIN__
  switch (operation)
  {
  case HA_EXTRA_PREPARE_FOR_RENAME:
  case HA_EXTRA_FORCE_REOPEN:
    /* Close both reader and writer so we don't have the file open. */
    if (archive_reader_open)
    {
      ret= azclose(&archive);
      archive_reader_open= false;
    }
    mysql_mutex_lock(&share->mutex);
    share->close_archive_writer();
    mysql_mutex_unlock(&share->mutex);
    break;
  default:
    /* Nothing to do. */
    ;
  }
#endif
  DBUG_RETURN(ret);
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
  mysql_mutex_lock(&share->mutex);
  if (share->archive_write_open)
    share->dirty= true;
  mysql_mutex_unlock(&share->mutex);
  DBUG_RETURN(0);
}

/*
  We cancel a truncate command. The only way to delete an archive table is to drop it.
  This is done for security reasons. In a later version we will enable this by 
  allowing the user to select a different row format.
*/
int ha_archive::truncate()
{
  DBUG_ENTER("ha_archive::truncate");
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


/**
  @brief Check for upgrade

  @param[in]  check_opt  check options

  @return Completion status
    @retval HA_ADMIN_OK            No upgrade required
    @retval HA_ADMIN_CORRUPT       Cannot read meta-data
    @retval HA_ADMIN_NEEDS_UPGRADE Upgrade required
*/

int ha_archive::check_for_upgrade(HA_CHECK_OPT *check_opt)
{
  DBUG_ENTER("ha_archive::check_for_upgrade");
  if (init_archive_reader())
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  if (archive.version < ARCHIVE_VERSION)
    DBUG_RETURN(HA_ADMIN_NEEDS_UPGRADE);
  DBUG_RETURN(HA_ADMIN_OK);
}


/*
  Simple scan of the tables to make sure everything is ok.
*/

int ha_archive::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  int rc= 0;
  const char *old_proc_info;
  ha_rows count;
  DBUG_ENTER("ha_archive::check");

  old_proc_info= thd_proc_info(thd, "Checking table");
  mysql_mutex_lock(&share->mutex);
  count= share->rows_recorded;
  /* Flush any waiting data */
  if (share->archive_write_open)
    azflush(&(share->archive_write), Z_SYNC_FLUSH);
  mysql_mutex_unlock(&share->mutex);

  if (init_archive_reader())
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  /*
    Now we will rewind the archive file so that we are positioned at the 
    start of the file.
  */
  read_data_header(&archive);
  for (ha_rows cur_count= count; cur_count; cur_count--)
  {
    if ((rc= get_row(&archive, table->record[0])))
      goto error;
  }
  /*
    Now read records that may have been inserted concurrently.
    Acquire share->mutex so tail of the table is not modified by
    concurrent writers.
  */
  mysql_mutex_lock(&share->mutex);
  count= share->rows_recorded - count;
  if (share->archive_write_open)
    azflush(&(share->archive_write), Z_SYNC_FLUSH);
  while (!(rc= get_row(&archive, table->record[0])))
    count--;
  mysql_mutex_unlock(&share->mutex);

  if ((rc && rc != HA_ERR_END_OF_FILE) || count)  
    goto error;

  thd_proc_info(thd, old_proc_info);
  DBUG_RETURN(HA_ADMIN_OK);

error:
  thd_proc_info(thd, old_proc_info);
  share->crashed= FALSE;
  DBUG_RETURN(HA_ADMIN_CORRUPT);
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

archive_record_buffer *ha_archive::create_record_buffer(unsigned int length) 
{
  DBUG_ENTER("ha_archive::create_record_buffer");
  archive_record_buffer *r;
  if (!(r= 
        (archive_record_buffer*) my_malloc(sizeof(archive_record_buffer),
                                           MYF(MY_WME))))
  {
    DBUG_RETURN(NULL); /* purecov: inspected */
  }
  r->length= (int)length;

  if (!(r->buffer= (uchar*) my_malloc(r->length,
                                    MYF(MY_WME))))
  {
    my_free(r);
    DBUG_RETURN(NULL); /* purecov: inspected */
  }

  DBUG_RETURN(r);
}

void ha_archive::destroy_record_buffer(archive_record_buffer *r)
{
  DBUG_ENTER("ha_archive::destroy_record_buffer");
  my_free(r->buffer);
  my_free(r);
  DBUG_VOID_RETURN;
}

bool ha_archive::check_if_incompatible_data(HA_CREATE_INFO *info,
                                            uint table_changes)
{
  if (info->auto_increment_value != stats.auto_increment_value ||
      (info->used_fields & HA_CREATE_USED_DATADIR) ||
      info->data_file_name ||
      (info->used_fields & HA_CREATE_USED_COMMENT) ||
      table_changes != IS_EQUAL_YES)
    return COMPATIBLE_DATA_NO;

  return COMPATIBLE_DATA_YES;
}


struct st_mysql_storage_engine archive_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(archive)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &archive_storage_engine,
  "ARCHIVE",
  "Brian Aker, MySQL AB",
  "Archive storage engine",
  PLUGIN_LICENSE_GPL,
  archive_db_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0300 /* 3.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;

