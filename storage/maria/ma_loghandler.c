#include "maria_def.h"

/* number of opened log files in the pagecache (should be at least 2) */
#define OPENED_FILES_NUM 3

/* records buffer size (should be LOG_PAGE_SIZE * n) */
#define TRANSLOG_WRITE_BUFFER (1024*1024)
/* min chunk length */
#define TRANSLOG_MIN_CHUNK 3
/*
  Number of buffers used by loghandler

  Should be at least 4, because one thread can block up to 2 buffers in
  normal circumstances (less then half of one and full other, or just
  switched one and other), But if we met end of the file in the middle and
  have to switch buffer it will be 3.  + 1 buffer for flushing/writing.
  We have a bigger number here for higher concurrency.
*/
#define TRANSLOG_BUFFERS_NO 5
/* number of bytes (+ header) which can be unused on first page in sequence */
#define TRANSLOG_MINCHUNK_CONTENT 1
/* version of log file */
#define TRANSLOG_VERSION_ID 10000               /* 1.00.00 */

#define TRANSLOG_PAGE_FLAGS 6 /* transaction log page flags offset */

/* QQ:  For temporary debugging */
#define UNRECOVERABLE_ERROR(E) \
  do { \
    DBUG_PRINT("error", E); \
    printf E; \
    putchar('\n'); \
  } while(0);



/* record part descriptor */
struct st_translog_part
{
  translog_size_t len;
  byte *buff;
};


/* record parts descriptor */
struct st_translog_parts
{
  /* full record length */
  translog_size_t record_length;
  /* full record length with chunk headers */
  translog_size_t total_record_length;
  /* array of parts (st_translog_part) */
  DYNAMIC_ARRAY parts;
  /* current part index */
  uint current;
};

/* log write buffer descriptor */
struct st_translog_buffer
{
  LSN last_lsn;
  /* This buffer offset in the file */
  TRANSLOG_ADDRESS offset;
  /*
     How much written (or will be written when copy_to_buffer_in_progress
     become 0) to this buffer
  */
  translog_size_t size;
  /* File handler for this buffer */
  File file;
  /* Threads which are waiting for buffer filling/freeing */
  WQUEUE waiting_filling_buffer;
  /* Number of record which are in copy progress */
  uint copy_to_buffer_in_progress;
  /* list of waiting buffer ready threads */
  struct st_my_thread_var *waiting_flush;
  struct st_translog_buffer *overlay;
#ifndef DBUG_OFF
  uint buffer_no;
#endif
  /* lock for the buffer. Current buffer also lock the handler */
  pthread_mutex_t mutex;
  /* IO cache for current log */
  byte buffer[TRANSLOG_WRITE_BUFFER];
};


struct st_buffer_cursor
{
  /* pointer on the buffer */
  byte *ptr;
  /* current buffer */
  struct st_translog_buffer *buffer;
  /* current page fill */
  uint16 current_page_fill;
  /* how many times we finish this page to write it */
  uint16 write_counter;
  /* previous write offset */
  uint16 previous_offset;
  /* Number of current buffer */
  uint8 buffer_no;
  my_bool chaser, protected;
};


struct st_translog_descriptor
{
  /* *** Parameters of the log handler *** */

  /* Page cache for the log reads */
  PAGECACHE *pagecache;
  /* Flags */
  uint flags;
  /* max size of one log size (for new logs creation) */
  uint32 log_file_max_size;
  /* server version */
  uint32 server_version;
  /* server ID */
  uint32 server_id;
  /* Loghandler's buffer capacity in case of chunk 2 filling */
  uint32 buffer_capacity_chunk_2;
  /* Half of the buffer capacity in case of chunk 2 filling */
  uint32 half_buffer_capacity_chunk_2;
  /* Page overhead calculated by flags */
  uint16 page_overhead;
  /* Page capacity calculated by flags (TRANSLOG_PAGE_SIZE-page_overhead-1) */
  uint16 page_capacity_chunk_2;
  /* Directory to store files */
  char directory[FN_REFLEN];

  /* *** Current state of the log handler *** */
  /* Current and (OPENED_FILES_NUM-1) last logs number in page cache */
  File log_file_num[OPENED_FILES_NUM];
  File directory_fd;
  /* buffers for log writing */
  struct st_translog_buffer buffers[TRANSLOG_BUFFERS_NO];
  /*
     horizon - visible end of the log (here is absolute end of the log:
     position where next chunk can start
  */
  TRANSLOG_ADDRESS horizon;
  /* horizon buffer cursor */
  struct st_buffer_cursor bc;

  /* Last flushed LSN */
  LSN flushed;
  LSN sent_to_file;
  pthread_mutex_t sent_to_file_lock;
};

static struct st_translog_descriptor log_descriptor;

/* Marker for end of log */
static byte end_of_log= 0;

/* record classes */
enum record_class
{
  LOGRECTYPE_NOT_ALLOWED,
  LOGRECTYPE_VARIABLE_LENGTH,
  LOGRECTYPE_PSEUDOFIXEDLENGTH,
  LOGRECTYPE_FIXEDLENGTH
};

/* chunk types */
#define TRANSLOG_CHUNK_LSN   0x00      /* 0 chunk refer as LSN (head or tail */
#define TRANSLOG_CHUNK_FIXED (1 << 6)  /* 1 (pseudo)fixed record (also LSN) */
#define TRANSLOG_CHUNK_NOHDR (2 << 6)  /* 2 no head chunk (till page end) */
#define TRANSLOG_CHUNK_LNGTH (3 << 6)  /* 3 chunk with chunk length */
#define TRANSLOG_CHUNK_TYPE  (3 << 6)  /* Mask to get chunk type */
#define TRANSLOG_REC_TYPE    0x3F               /* Mask to get record type */

/* compressed (relative) LSN constants */
#define TRANSLOG_CLSN_LEN_BITS 0xC0    /* Mask to get compressed LSN length */
#define TRANSLOG_CLSN_MAX_LEN  5       /* Maximum length of compressed LSN */

typedef my_bool(*prewrite_rec_hook) (enum translog_record_type type,
                                     void *tcb,
                                     struct st_translog_parts *parts);

typedef my_bool(*inwrite_rec_hook) (enum translog_record_type type,
                                    void *tcb,
                                    LSN *lsn,
                                    struct st_translog_parts *parts);

typedef uint16(*read_rec_hook) (enum translog_record_type type,
                                uint16 read_length, uchar *read_buff,
                                byte *decoded_buff);

/*
  Descriptor of log record type
  Note: Don't reorder because of constructs later...
*/
struct st_log_record_type_descriptor
{
  /* internal class of the record */
  enum record_class class;
  /* length for fixed-size record, or maximum length of pseudo-fixed */
  uint16 fixed_length;
  /* how much record body (belonged to headers too) read with headers */
  uint16 read_header_len;
  /* HOOK for writing the record called before lock */
  prewrite_rec_hook prewrite_hook;
  /* HOOK for writing the record called when LSN is known */
  inwrite_rec_hook inwrite_hook;
  /* HOOK for reading headers */
  read_rec_hook read_hook;
  /*
     For pseudo fixed records number of compressed LSNs followed by
     system header
  */
  int16 compressed_LSN;
};


static struct st_log_record_type_descriptor
  log_record_type_descriptor[LOGREC_NUMBER_OF_TYPES]=
{
  /*LOGREC_RESERVED_FOR_CHUNKS23= 0 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_INSERT_ROW_HEAD= 1 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_INSERT_ROW_TAIL= 2 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_INSERT_ROW_BLOB= 3 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 8, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_INSERT_ROW_BLOBS= 4 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 10, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_PURGE_ROW= 5 */
  {LOGRECTYPE_FIXEDLENGTH, 9, 9, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_PURGE_BLOCKS= 6 */
  {LOGRECTYPE_FIXEDLENGTH, 10, 10, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_DELETE_ROW= 7 */
  {LOGRECTYPE_FIXEDLENGTH, 16, 16, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_UPDATE_ROW_HEAD= 8 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_INDEX= 9 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_UNDELETE_ROW= 10 */
  {LOGRECTYPE_FIXEDLENGTH, 16, 16, NULL, NULL, NULL, 0},
  /*LOGREC_CLR_END= 11 */
  {LOGRECTYPE_PSEUDOFIXEDLENGTH, 5, 5, NULL, NULL, NULL, 1},
  /*LOGREC_PURGE_END= 12 */
  {LOGRECTYPE_PSEUDOFIXEDLENGTH, 5, 5, NULL, NULL, NULL, 1},
  /*LOGREC_UNDO_ROW_INSERT= 13 */
  {LOGRECTYPE_PSEUDOFIXEDLENGTH, 14, 14, NULL, NULL, NULL, 1},
  /*LOGREC_UNDO_ROW_DELETE= 14 */
  {LOGRECTYPE_PSEUDOFIXEDLENGTH, 19, 19, NULL, NULL, NULL, 2},
  /*LOGREC_UNDO_ROW_UPDATE= 15 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 14, NULL, NULL, NULL, 2},
  /*LOGREC_UNDO_KEY_INSERT= 16 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 10, NULL, NULL, NULL, 1},
  /*LOGREC_UNDO_KEY_DELETE= 17 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 15, NULL, NULL, NULL, 2},
  /*LOGREC_PREPARE= 18 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_PREPARE_WITH_UNDO_PURGE= 19 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 5, NULL, NULL, NULL, 1},
  /*LOGREC_COMMIT= 20 */
  {LOGRECTYPE_FIXEDLENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_COMMIT_WITH_UNDO_PURGE= 21 */
  {LOGRECTYPE_PSEUDOFIXEDLENGTH, 5, 5, NULL, NULL, NULL, 1},
  /*LOGREC_CHECKPOINT_PAGE= 22 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 6, NULL, NULL, NULL, 0},
  /*LOGREC_CHECKPOINT_TRAN= 23 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_CHECKPOINT_TABL= 24 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 8, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_CREATE_TABLE= 25 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_RENAME_TABLE= 26 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_DROP_TABLE= 27 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_REDO_TRUNCATE_TABLE= 28 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_FILE_ID= 29 */
  {LOGRECTYPE_VARIABLE_LENGTH, 0, 4, NULL, NULL, NULL, 0},
  /*LOGREC_LONG_TRANSACTION_ID= 30 */
  {LOGRECTYPE_FIXEDLENGTH, 6, 6, NULL, NULL, NULL, 0},
  /*31 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*32 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*33 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*34 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*35 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*36 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*37 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*38 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*39 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*40 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*41 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*42 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*43 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*44 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*45 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*46 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*47 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*48 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*49 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*50 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*51 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*52 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*53 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*54 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*55 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*56 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*57 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*58 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*59 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*60 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*61 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*62 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0},
  /*LOGREC_RESERVED_FUTURE_EXTENSION= 63 */
  {LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0}
};

/* all possible flags page overheads */
static uint page_overhead[TRANSLOG_FLAGS_NUM];

typedef struct st_translog_validator_data
{
  TRANSLOG_ADDRESS *addr;
  my_bool was_recovered;
} TRANSLOG_VALIDATOR_DATA;


const char *maria_data_root;


/*
  Get file name of the log by log number

  SYNOPSIS
    translog_filename_by_fileno()
    file_no              Number of the log we want to open
    path                 Pointer to buffer where file name will be
                         stored (must be FN_REFLEN bytes at least
  RETURN
    pointer to path
*/

static char *translog_filename_by_fileno(uint32 file_no, char *path)
{
  char file_name[10 + 8 + 1];                   /* See my_sprintf */
  char *res;
  DBUG_ENTER("translog_filename_by_fileno");
  DBUG_ASSERT(file_no <= 0xfffffff);
  my_sprintf(file_name, (file_name, "maria_log.%08u", file_no));
  res= fn_format(path, file_name, log_descriptor.directory, "", MYF(MY_WME));
  DBUG_PRINT("info", ("Path: '%s'  path: 0x%lx  res: 0x%lx",
                      res, (ulong) path, (ulong) res));
  DBUG_RETURN(res);
}


/*
  Open log file with given number without cache

  SYNOPSIS
    open_logfile_by_number_no_cache()
    file_no              Number of the log we want to open

  RETURN
    -1  error
    #   file descriptor number
*/

static File open_logfile_by_number_no_cache(uint32 file_no)
{
  File file;
  char path[FN_REFLEN];
  DBUG_ENTER("open_logfile_by_number_no_cache");

  /* Todo: add O_DIRECT to open flags (when buffer is aligned) */
  if ((file= my_open(translog_filename_by_fileno(file_no, path),
                     O_CREAT | O_BINARY | O_RDWR,
                     MYF(MY_WME))) < 0)
  {
    UNRECOVERABLE_ERROR(("Error %d during opening file '%s'", errno, path));
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("File: '%s'  handler: %d", path, file));
  DBUG_RETURN(file);
}


/*
  Write log file page header in the just opened new log file

  SYNOPSIS
    translog_write_file_header();

   NOTES
    First page is just a marker page; We don't store any real log data in it.

  RETURN
    0 OK
    1 ERROR
*/

uchar	NEAR maria_trans_file_magic[]=
{ (uchar) 254, (uchar) 254, (uchar) 11, '\001', 'M', 'A', 'R', 'I', 'A',
 'L', 'O', 'G' };

static my_bool translog_write_file_header()
{
  ulonglong timestamp;
  byte page_buff[TRANSLOG_PAGE_SIZE], *page= page_buff;
  DBUG_ENTER("translog_write_file_header");

  /* file tag */
  memcpy(page, maria_trans_file_magic, sizeof(maria_trans_file_magic));
  page+= sizeof(maria_trans_file_magic);
  /* timestamp */
  timestamp= my_getsystime();
  int8store(page, timestamp);
  page+= 8;
  /* maria version */
  int4store(page, TRANSLOG_VERSION_ID);
  page+= 4;
  /* mysql version (MYSQL_VERSION_ID) */
  int4store(page, log_descriptor.server_version);
  page+= 4;
  /* server ID */
  int4store(page, log_descriptor.server_id);
  page+= 4;
  /* loghandler page_size/DISK_DRIVE_SECTOR_SIZE */
  int2store(page, TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE);
  page+= 2;
  /* file number */
  int3store(page, LSN_FILE_NO(log_descriptor.horizon));
  page+= 3;
  bzero(page, sizeof(page_buff) - (page- page_buff));

  DBUG_RETURN(my_pwrite(log_descriptor.log_file_num[0], page_buff,
                        sizeof(page_buff), 0, MYF(MY_WME | MY_NABP)) != 0);
}


/*
  Initialize transaction log file buffer

  SYNOPSIS
    translog_buffer_init()
    buffer               The buffer to initialize

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_buffer_init(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_init");
  /* This buffer offset */
  buffer->last_lsn= CONTROL_FILE_IMPOSSIBLE_LSN;
  /* This Buffer File */
  buffer->file= -1;
  buffer->overlay= 0;
  /* IO cache for current log */
  bzero(buffer->buffer, TRANSLOG_WRITE_BUFFER);
  /* Buffer size */
  buffer->size= 0;
  /* cond of thread which is waiting for buffer filling */
  buffer->waiting_filling_buffer.last_thread= 0;
  /* Number of record which are in copy progress */
  buffer->copy_to_buffer_in_progress= 0;
  /* list of waiting buffer ready threads */
  buffer->waiting_flush= 0;
  /* lock for the buffer. Current buffer also lock the handler */
  if (pthread_mutex_init(&buffer->mutex, MY_MUTEX_INIT_FAST))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/*
  Close transaction log file by descriptor

  SYNOPSIS
    translog_close_log_file()
    file                 file descriptor

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_close_log_file(File file)
{
  int rc;
  PAGECACHE_FILE fl;
  fl.file= file;
  flush_pagecache_blocks(log_descriptor.pagecache, &fl, FLUSH_RELEASE);
  /*
    Sync file when we close it
    TODO: sync only we have changed the log
  */
  rc= my_sync(file, MYF(MY_WME));
  rc|= my_close(file, MYF(MY_WME));
  return test(rc);
}


/*
  Create and fill header of new file

  SYNOPSIS
    translog_create_new_file()

  RETURN
    0 OK
    1 Error
*/

static my_bool translog_create_new_file()
{
  int i;
  uint32 file_no= LSN_FILE_NO(log_descriptor.horizon);
  DBUG_ENTER("translog_create_new_file");

  if (log_descriptor.log_file_num[OPENED_FILES_NUM - 1] != -1 &&
      translog_close_log_file(log_descriptor.log_file_num[OPENED_FILES_NUM -
                                                          1]))
    DBUG_RETURN(1);
  for (i= OPENED_FILES_NUM - 1; i > 0; i--)
    log_descriptor.log_file_num[i]= log_descriptor.log_file_num[i - 1];

  if ((log_descriptor.log_file_num[0]=
       open_logfile_by_number_no_cache(file_no)) == -1 ||
      translog_write_file_header())
    DBUG_RETURN(1);

  if (ma_control_file_write_and_force(CONTROL_FILE_IMPOSSIBLE_LSN, file_no,
                                      CONTROL_FILE_UPDATE_ONLY_LOGNO))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/*
  Lock the loghandler buffer

  SYNOPSIS
    translog_buffer_lock()
    buffer               This buffer which should be locked

  RETURN
    0  OK
    1  Error
*/

#ifndef DBUG_OFF
static my_bool translog_buffer_lock(struct st_translog_buffer *buffer)
{
  int res;
  DBUG_ENTER("translog_buffer_lock");
  DBUG_PRINT("enter",
             ("Lock buffer #%u: (0x%lx)  mutex: 0x%lx",
              (uint) buffer->buffer_no, (ulong) buffer,
              (ulong) &buffer->mutex));
  res= (pthread_mutex_lock(&buffer->mutex) != 0);
  DBUG_RETURN(res);
}
#else
#define translog_buffer_lock(B) \
  pthread_mutex_lock(&B->mutex);
#endif


/*
  Unlock the loghandler buffer

  SYNOPSIS
    translog_buffer_unlock()
    buffer               This buffer which should be unlocked

  RETURN
    0  OK
    1  Error
*/

#ifndef DBUG_OFF
static my_bool translog_buffer_unlock(struct st_translog_buffer *buffer)
{
  int res;
  DBUG_ENTER("translog_buffer_unlock");
  DBUG_PRINT("enter", ("Unlock buffer... #%u (0x%lx)  "
                       "mutex: 0x%lx",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (ulong) &buffer->mutex));

  res= (pthread_mutex_unlock(&buffer->mutex) != 0);
  DBUG_PRINT("enter", ("Unlocked buffer... #%u: 0x%lx  mutex: 0x%lx",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (ulong) &buffer->mutex));
  DBUG_RETURN(res);
}
#else
#define translog_buffer_unlock(B) \
  pthread_mutex_unlock(&B->mutex);
#endif


/*
  Write a header on the page

  SYNOPSIS
    translog_new_page_header()
    horizon              Where to write the page
    cursor               Where to write the page

  NOTE
    - space for page header should be checked before
*/

static void translog_new_page_header(TRANSLOG_ADDRESS *horizon,
                                     struct st_buffer_cursor *cursor)
{
  byte *ptr;

  DBUG_ENTER("translog_new_page_header");
  DBUG_ASSERT(cursor->ptr);

  cursor->protected= 0;

  ptr= cursor->ptr;
  /* Page number */
  int3store(ptr, LSN_OFFSET(*horizon) / TRANSLOG_PAGE_SIZE);
  ptr+= 3;
  /* File number */
  int3store(ptr, LSN_FILE_NO(*horizon));
  ptr+= 3;
  *(ptr++)= (byte) log_descriptor.flags;
  if (log_descriptor.flags & TRANSLOG_PAGE_CRC)
  {
#ifndef DBUG_OFF
    DBUG_PRINT("info", ("write  0x11223344 CRC to (%lu,0x%lx)",
                        (ulong) LSN_FILE_NO(*horizon),
                        (ulong) LSN_OFFSET(*horizon)));
    /* This will be overwritten by real CRC; This is just for debugging */
    int4store(ptr, 0x11223344);
#endif
    /* CRC will be put when page is finished */
    ptr+= CRC_LENGTH;
  }
  if (log_descriptor.flags & TRANSLOG_SECTOR_PROTECTION)
  {
    time_t tm;
    uint16 tmp_time= time(&tm);
    int2store(ptr, tmp_time);
    ptr+= (TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE) * 2;
  }
  {
    uint len= (ptr - cursor->ptr);
    (*horizon)+= len; /* it is increasing of offset part of the address */
    cursor->current_page_fill= len;
    if (!cursor->chaser)
      cursor->buffer->size+= len;
  }
  cursor->ptr= ptr;
  DBUG_PRINT("info", ("NewP buffer #%u: 0x%lx  chaser: %d  Size: %lu (%lu)",
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer)));
  DBUG_ASSERT(cursor->chaser ||
              ((ulong) (cursor->ptr - cursor->buffer->buffer) ==
               cursor->buffer->size));
  DBUG_ASSERT(cursor->buffer->buffer_no == cursor->buffer_no);
  DBUG_ASSERT(cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_VOID_RETURN;
}


/*
  Put sector protection on the page image

  SYNOPSIS
    translog_put_sector_protection()
    page                 reference on the page content
    cursor               cursor of the buffer

  NOTES
    We put a sector protection on all following sectors on the page,
    except the first sector that is protected by page header.
*/

static void translog_put_sector_protection(byte *page,
                                           struct st_buffer_cursor *cursor)
{
  byte *table= page + log_descriptor.page_overhead -
    (TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE) * 2;
  uint16 value= uint2korr(table) + cursor->write_counter;
  uint16 last_protected_sector= ((cursor->previous_offset - 1) /
                                 DISK_DRIVE_SECTOR_SIZE);
  uint16 start_sector= cursor->previous_offset / DISK_DRIVE_SECTOR_SIZE;
  uint i, offset;
  DBUG_ENTER("translog_put_sector_protection");

  if (start_sector == 0)
    start_sector= 1;                            /* First sector is protected */

  DBUG_PRINT("enter", ("Write counter:%u  value:%u  offset:%u, "
                       "last protected:%u  start sector:%u",
                       (uint) cursor->write_counter,
                       (uint) value,
                       (uint) cursor->previous_offset,
                       (uint) last_protected_sector, (uint) start_sector));
  if (last_protected_sector == start_sector)
  {
    i= last_protected_sector * 2;
    offset= last_protected_sector * DISK_DRIVE_SECTOR_SIZE;
    /* restore data, because we modified sector which was protected */
    if (offset < cursor->previous_offset)
      page[offset]= table[i];
    offset++;
    if (offset < cursor->previous_offset)
      page[offset]= table[i + 1];
  }
  for (i= start_sector * 2, offset= start_sector * DISK_DRIVE_SECTOR_SIZE;
       i < (TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE) * 2;
       (i+= 2), (offset+= DISK_DRIVE_SECTOR_SIZE))
  {
    DBUG_PRINT("info", ("sector:%u  offset:%u  data 0x%x%x",
                        i / 2, offset, (uint) page[offset],
                        (uint) page[offset + 1]));
    table[i]= page[offset];
    table[i + 1]= page[offset + 1];
    int2store(page + offset, value);
    DBUG_PRINT("info", ("sector:%u  offset:%u  data 0x%x%x",
                        i / 2, offset, (uint) page[offset],
                        (uint) page[offset + 1]));
  }
  DBUG_VOID_RETURN;
}


/*
  Calculate CRC32 of given area

  SYNOPSIS
    translog_crc()
    area                 Pointer of the area beginning
    length               The Area length

  RETURN
    CRC32
*/

static uint32 translog_crc(byte *area, uint length)
{
  return crc32(0L, area, length);
}


/*
  Finish current page with zeros

  SYNOPSIS
    translog_finish_page()
    horizon              \ horizon & buffer pointers
    cursor               /
*/

static void translog_finish_page(TRANSLOG_ADDRESS *horizon,
                                 struct st_buffer_cursor *cursor)
{
  uint16 left= TRANSLOG_PAGE_SIZE - cursor->current_page_fill;
  byte *page= cursor->ptr -cursor->current_page_fill;
  DBUG_ENTER("translog_finish_page");
  DBUG_PRINT("enter", ("Buffer: #%u 0x%lx  "
                       "Buffer addr: (%lu,0x%lx)  "
                       "Page addr: (%lu,0x%lx)  "
                       "size:%lu (%lu)  Pg:%u  left:%u",
                       (uint) cursor->buffer_no, (ulong) cursor->buffer,
                       (ulong) LSN_FILE_NO(cursor->buffer->offset),
                       (ulong) LSN_OFFSET(cursor->buffer->offset),
                       (ulong) LSN_FILE_NO(*horizon),
                       (ulong) (LSN_OFFSET(*horizon) -
                                cursor->current_page_fill),
                       (ulong) cursor->buffer->size,
                       (ulong) (cursor->ptr -cursor->buffer->buffer),
                       (uint) cursor->current_page_fill, (uint) left));
  DBUG_ASSERT(cursor->ptr !=NULL);
  DBUG_ASSERT((cursor->ptr -cursor->buffer->buffer) %TRANSLOG_PAGE_SIZE ==
              cursor->current_page_fill % TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(LSN_FILE_NO(*horizon) == LSN_FILE_NO(cursor->buffer->offset));
  DBUG_ASSERT(LSN_OFFSET(cursor->buffer->offset) +
              (cursor->ptr -cursor->buffer->buffer) == LSN_OFFSET(*horizon));
  if (cursor->protected)
  {
    DBUG_PRINT("info", ("Already protected and finished"));
    DBUG_VOID_RETURN;
  }
  cursor->protected= 1;

  DBUG_ASSERT(left < TRANSLOG_PAGE_SIZE);
  if (left != 0)
  {
    DBUG_PRINT("info", ("left: %u", (uint) left));
    bzero(cursor->ptr, left);
    cursor->ptr +=left;
    (*horizon)+= left; /* offset increasing */
    if (!cursor->chaser)
      cursor->buffer->size+= left;
    cursor->current_page_fill= 0;
    DBUG_PRINT("info", ("Finish Page buffer #%u: 0x%lx  "
                        "chaser: %d  Size: %lu (%lu)",
                        (uint) cursor->buffer->buffer_no,
                        (ulong) cursor->buffer, cursor->chaser,
                        (ulong) cursor->buffer->size,
                        (ulong) (cursor->ptr - cursor->buffer->buffer)));
    DBUG_ASSERT(cursor->chaser
                || ((ulong) (cursor->ptr - cursor->buffer->buffer) ==
                    cursor->buffer->size));
    DBUG_ASSERT(cursor->buffer->buffer_no == cursor->buffer_no);
  }
  if (page[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION)
  {
    translog_put_sector_protection(page, cursor);
    DBUG_PRINT("info", ("drop write_counter"));
    cursor->write_counter= 0;
    cursor->previous_offset= 0;
  }
  if (page[TRANSLOG_PAGE_FLAGS] & TRANSLOG_PAGE_CRC)
  {
    uint32 crc= translog_crc(page + log_descriptor.page_overhead,
                             TRANSLOG_PAGE_SIZE -
                             log_descriptor.page_overhead);
    DBUG_PRINT("info", ("CRC: %lx", (ulong) crc));
    /* We have page number, file number and flag before crc */
    int4store(page + 3 + 3 + 1, crc);
  }
  DBUG_VOID_RETURN;
}


/*
  Wait until all thread finish filling this buffer

  SYNOPSIS
    translog_wait_for_writers()
    buffer               This buffer should be check

  NOTE
    This buffer should be locked
*/

static void translog_wait_for_writers(struct st_translog_buffer *buffer)
{
  struct st_my_thread_var *thread= my_thread_var;
  DBUG_ENTER("translog_wait_for_writers");
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  copies in progress: %u",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (int) buffer->copy_to_buffer_in_progress));

  while (buffer->copy_to_buffer_in_progress)
  {
    DBUG_PRINT("info", ("wait for writers... "
                        "buffer: #%u 0x%lx  "
                        "mutex: 0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer,
                        (ulong) &buffer->mutex));
    DBUG_ASSERT(buffer->file != -1);
    wqueue_add_and_wait(&buffer->waiting_filling_buffer, thread,
                        &buffer->mutex);
    DBUG_PRINT("info", ("wait for writers done  "
                        "buffer: #%u 0x%lx  "
                        "mutex: 0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer,
                        (ulong) &buffer->mutex));
  }

  DBUG_VOID_RETURN;
}


/*

  Wait for buffer to become free

  SYNOPSIS
    translog_wait_for_buffer_free()
    buffer               The buffer we are waiting for

  NOTE
    - this buffer should be locked
*/

static void translog_wait_for_buffer_free(struct st_translog_buffer *buffer)
{
  struct st_my_thread_var *thread= my_thread_var;
  DBUG_ENTER("translog_wait_for_buffer_free");
  DBUG_PRINT("enter", ("Buffer: #%u 0x%lx  copies in progress: %u  "
                       "File: %d  size: 0x%lu",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (int) buffer->copy_to_buffer_in_progress,
                       buffer->file, (ulong) buffer->size));

  translog_wait_for_writers(buffer);

  while (buffer->file != -1)
  {
    DBUG_PRINT("info", ("wait for writers...  "
                        "buffer: #%u 0x%lx  "
                        "mutex: 0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer,
                        (ulong) &buffer->mutex));
    wqueue_add_and_wait(&buffer->waiting_filling_buffer, thread,
                        &buffer->mutex);
    DBUG_PRINT("info", ("wait for writers done.  "
                        "buffer: #%u 0x%lx  "
                        "mutex: 0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer,
                        (ulong) &buffer->mutex));
  }
  DBUG_ASSERT(buffer->copy_to_buffer_in_progress == 0);
  DBUG_VOID_RETURN;
}


/*
  Initialize the cursor for a buffer

  SYNOPSIS
    translog_cursor_init()
    buffer               The buffer
    cursor               It's cursor
    buffer_no            Number of buffer
*/

static void translog_cursor_init(struct st_buffer_cursor *cursor,
                                 struct st_translog_buffer *buffer,
                                 uint8 buffer_no)
{
  DBUG_ENTER("translog_cursor_init");
  cursor->ptr= buffer->buffer;
  cursor->buffer= buffer;
  cursor->buffer_no= buffer_no;
  cursor->current_page_fill= 0;
  cursor->chaser= (cursor != &log_descriptor.bc);
  cursor->write_counter= 0;
  cursor->previous_offset= 0;
  cursor->protected= 0;
  DBUG_VOID_RETURN;
}


/*
  Initialize buffer for current file

  SYNOPSIS
    translog_start_buffer()
    buffer               The buffer
    cursor               It's cursor
    buffer_no            Number of buffer
*/

static void translog_start_buffer(struct st_translog_buffer *buffer,
                                  struct st_buffer_cursor *cursor,
                                  uint buffer_no)
{
  DBUG_ENTER("translog_start_buffer");
  DBUG_PRINT("enter",
             ("Assign buffer: #%u (0x%lx)  to file: %d  offset: 0x%lx(%lu)",
              (uint) buffer->buffer_no, (ulong) buffer,
              log_descriptor.log_file_num[0],
              (ulong) LSN_OFFSET(log_descriptor.horizon),
              (ulong) LSN_OFFSET(log_descriptor.horizon)));
  DBUG_ASSERT(buffer_no == buffer->buffer_no);
  buffer->last_lsn= CONTROL_FILE_IMPOSSIBLE_LSN;
  buffer->offset= log_descriptor.horizon;
  buffer->file= log_descriptor.log_file_num[0];
  buffer->overlay= 0;
  buffer->size= 0;
  translog_cursor_init(cursor, buffer, buffer_no);
  DBUG_PRINT("info", ("init cursor #%u: 0x%lx  chaser: %d  Size: %lu (%lu)",
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer)));
  DBUG_ASSERT(cursor->chaser ||
              ((ulong) (cursor->ptr - cursor->buffer->buffer) ==
               cursor->buffer->size));
  DBUG_ASSERT(cursor->buffer->buffer_no == cursor->buffer_no);
  DBUG_VOID_RETURN;
}


/*
  Switch to the next buffer in a chain

  SYNOPSIS
    translog_buffer_next()
    horizon              \ Pointers on current position in file and buffer
    cursor               /
    next_file            Also start new file

  NOTE:
   - loghandler should be locked
   - after return new and old buffer still are locked

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_buffer_next(TRANSLOG_ADDRESS *horizon,
                                    struct st_buffer_cursor *cursor,
                                    my_bool new_file)
{
  uint old_buffer_no= cursor->buffer_no;
  uint new_buffer_no= (old_buffer_no + 1) % TRANSLOG_BUFFERS_NO;
  struct st_translog_buffer *new_buffer= log_descriptor.buffers + new_buffer_no;
  my_bool chasing= cursor->chaser;
  DBUG_ENTER("translog_buffer_next");

  DBUG_PRINT("info", ("horizon: (%lu,0x%lx)  chasing: %d",
                      (ulong) LSN_FILE_NO(log_descriptor.horizon),
                      (ulong) LSN_OFFSET(log_descriptor.horizon), chasing));

  DBUG_ASSERT(cmp_translog_addr(log_descriptor.horizon, *horizon) >= 0);

  translog_finish_page(horizon, cursor);

  if (!chasing)
  {
    translog_buffer_lock(new_buffer);
    translog_wait_for_buffer_free(new_buffer);
  }
#ifndef DBUG_OFF
  else
    DBUG_ASSERT(new_buffer->file != 0);
#endif
  if (new_file)
  {
    /* move the horizon to the next file and its header page */
    (*horizon)+= LSN_ONE_FILE;
    (*horizon)= LSN_REPLACE_OFFSET(*horizon, TRANSLOG_PAGE_SIZE);
    if (!chasing && translog_create_new_file())
    {
      DBUG_RETURN(1);
    }
  }

  /* prepare next page */
  if (chasing)
    translog_cursor_init(cursor, new_buffer, new_buffer_no);
  else
    translog_start_buffer(new_buffer, cursor, new_buffer_no);
  translog_new_page_header(horizon, cursor);
  DBUG_RETURN(0);
}


/*
  Set max LSN send to file

  SYNOPSIS
    translog_set_sent_to_file()
    lsn                  LSN to assign
*/

static void translog_set_sent_to_file(LSN *lsn)
{
  DBUG_ENTER("translog_set_sent_to_file");
  pthread_mutex_lock(&log_descriptor.sent_to_file_lock);
  DBUG_ASSERT(cmp_translog_addr(*lsn, log_descriptor.sent_to_file) >= 0);
  log_descriptor.sent_to_file= *lsn;
  pthread_mutex_unlock(&log_descriptor.sent_to_file_lock);
  DBUG_VOID_RETURN;
}


/*
  Get max LSN send to file

  SYNOPSIS
    translog_get_sent_to_file()
    lsn                  LSN to value
*/

static void translog_get_sent_to_file(LSN *lsn)
{
  DBUG_ENTER("translog_get_sent_to_file");
  pthread_mutex_lock(&log_descriptor.sent_to_file_lock);
  *lsn= log_descriptor.sent_to_file;
  pthread_mutex_unlock(&log_descriptor.sent_to_file_lock);
  DBUG_VOID_RETURN;
}


/*
  Get first chunk address on the given page

  SYNOPSIS
    translog_get_first_chunk_offset()
    page                 The page where to find first chunk

  RETURN
    first chunk offset
*/

static my_bool translog_get_first_chunk_offset(byte *page)
{
  uint16 page_header= 7;
  DBUG_ENTER("translog_get_first_chunk_offset");

  if (page[TRANSLOG_PAGE_FLAGS] & TRANSLOG_PAGE_CRC)
    page_header+= 4;
  if (page[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION)
    page_header+= (TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE) * 2;
  DBUG_RETURN(page_header);
}


/*
  Write coded length of record

  SYNOPSIS
    translog_write_variable_record_1group_code_len
    dst                  Destination buffer pointer
    length               Length which should be coded
    header_len           Calculated total header length
*/

static void
translog_write_variable_record_1group_code_len(byte *dst,
                                               translog_size_t length,
                                               uint16 header_len)
{
  switch (header_len) {
  case 6:                                      /* (5 + 1) */
    DBUG_ASSERT(length <= 250);
    *dst= (uint8) length;
    return;
  case 8:                                      /* (5 + 3) */
    DBUG_ASSERT(length <= 0xFFFF);
    *dst= 251;
    int2store(dst + 1, length);
    return;
  case 9:                                      /* (5 + 4) */
    DBUG_ASSERT(length <= (ulong) 0xFFFFFF);
    *dst= 252;
    int3store(dst + 1, length);
    return;
  case 10:                                     /* (5 + 5) */
    *dst= 253;
    int4store(dst + 1, length);
    return;
  default:
    DBUG_ASSERT(0);
  }
  return;
}


/*
  Decode record data length and advance given pointer to the next field

  SYNOPSIS
    translog_variable_record_1group_decode_len()
    src                  The pointer to the pointer to the length beginning

  RETURN
    decoded length
*/

static translog_size_t translog_variable_record_1group_decode_len(byte **src)
{
  uint8 first= (uint8) (**src);
  switch (first) {
  case 251:
    (*src)+= 3;
    return (uint2korr((*src) - 2));
  case 252:
    (*src)+= 4;
    return (uint3korr((*src) - 3));
  case 253:
    (*src)+= 5;
    return (uint4korr((*src) - 4));
  case 254:
  case 255:
    DBUG_ASSERT(0);                             /* reserved for future use */
    return (0);
  default:
    (*src)++;
    return (first);
  }
}


/*
  Get total length of this chunk (not only body)

  SYNOPSIS
    translog_get_total_chunk_length()
    page                 The page where chunk placed
    offset               Offset of the chunk on this place

  RETURN
    total length of the chunk
*/

static uint16 translog_get_total_chunk_length(byte *page, uint16 offset)
{
  DBUG_ENTER("translog_get_total_chunk_length");
  switch (page[offset] & TRANSLOG_CHUNK_TYPE) {
  case TRANSLOG_CHUNK_LSN:
  {
    /* 0 chunk referred as LSN (head or tail) */
    translog_size_t rec_len;
    byte *start= page + offset;
    byte *ptr= start + 1 + 2;
    uint16 chunk_len, header_len, page_rest;
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LSN"));
    rec_len= translog_variable_record_1group_decode_len(&ptr);
    chunk_len= uint2korr(ptr);
    header_len= (ptr -start) + 2;
    DBUG_PRINT("info", ("rec len: %lu  chunk len: %u  header len: %u",
                        (ulong) rec_len, (uint) chunk_len, (uint) header_len));
    if (chunk_len)
    {
      DBUG_PRINT("info", ("chunk len: %u + %u = %u",
                          (uint) header_len, (uint) chunk_len,
                          (uint) (chunk_len + header_len)));
      DBUG_RETURN(chunk_len + header_len);
    }
    page_rest= TRANSLOG_PAGE_SIZE - offset;
    DBUG_PRINT("info", ("page_rest %u", (uint) page_rest));
    if (rec_len + header_len < page_rest)
      DBUG_RETURN(rec_len + header_len);
    DBUG_RETURN(page_rest);
    break;
  }
  case TRANSLOG_CHUNK_FIXED:
  {
    byte *ptr;
    uint type= page[offset] & TRANSLOG_REC_TYPE;
    uint length;
    int i;
    /* 1 (pseudo)fixed record (also LSN) */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_FIXED"));
    DBUG_ASSERT(log_record_type_descriptor[type].class ==
                LOGRECTYPE_FIXEDLENGTH ||
                log_record_type_descriptor[type].class ==
                LOGRECTYPE_PSEUDOFIXEDLENGTH);
    if (log_record_type_descriptor[type].class == LOGRECTYPE_FIXEDLENGTH)
    {
      DBUG_PRINT("info",
                 ("Fixed length: %u",
                  (uint) (log_record_type_descriptor[type].fixed_length + 3)));
      DBUG_RETURN(log_record_type_descriptor[type].fixed_length + 3);
    }
    {
      ptr= page + offset + 3;            /* first compressed LSN */
      length= log_record_type_descriptor[type].fixed_length + 3;
      for (i= 0; i < log_record_type_descriptor[type].compressed_LSN; i++)
      {
        /* first 2 bits is length - 2 */
        uint len= ((((uint8) (*ptr)) & TRANSLOG_CLSN_LEN_BITS) >> 6) + 2;
        ptr+= len;
        /* subtract economized bytes */
        length-= (TRANSLOG_CLSN_MAX_LEN - len);
      }
      DBUG_PRINT("info", ("Pseudo-fixed length: %u", length));
      DBUG_RETURN(length);
    }
    break;
  }
  case TRANSLOG_CHUNK_NOHDR:
    /* 2 no header chunk (till page end) */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_NOHDR  length: %u",
                        (uint) (TRANSLOG_PAGE_SIZE - offset)));
    DBUG_RETURN(TRANSLOG_PAGE_SIZE - offset);
    break;
  case TRANSLOG_CHUNK_LNGTH:                   /* 3 chunk with chunk length */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LNGTH"));
    DBUG_ASSERT(TRANSLOG_PAGE_SIZE - offset >= 3);
    DBUG_PRINT("info", ("length: %u", uint2korr(page + offset + 1) + 3));
    DBUG_RETURN(uint2korr(page + offset + 1) + 3);
    break;
  default:
    DBUG_ASSERT(0);
  }
}


/*
  Flush given buffer

  SYNOPSIS
    translog_buffer_flush()
    buffer               This buffer should be flushed

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_buffer_flush(struct st_translog_buffer *buffer)
{
  uint32 i;
  DBUG_ENTER("translog_buffer_flush");
  DBUG_PRINT("enter",
             ("Buffer: #%u 0x%lx: "
              "file: %d  offset: (%lu,0x%lx)  size: %lu",
              (uint) buffer->buffer_no, (ulong) buffer,
              buffer->file,
              (ulong) LSN_FILE_NO(buffer->offset),
              (ulong) LSN_OFFSET(buffer->offset),
              (ulong) buffer->size));

  DBUG_ASSERT(buffer->file != -1);

  translog_wait_for_writers(buffer);
  if (buffer->overlay && buffer->overlay->file != -1)
  {
    struct st_translog_buffer *overlay= buffer->overlay;
    translog_buffer_unlock(buffer);
    translog_buffer_lock(overlay);
    translog_wait_for_buffer_free(overlay);
    translog_buffer_unlock(overlay);
    translog_buffer_lock(buffer);
  }

  for (i= 0; i < buffer->size; i+= TRANSLOG_PAGE_SIZE)
  {
    PAGECACHE_FILE file;
    file.file= buffer->file;
    if (pagecache_write(log_descriptor.pagecache,
                        &file,
                        (LSN_OFFSET(buffer->offset) + i) / TRANSLOG_PAGE_SIZE,
                        3,
                        buffer->buffer + i,
                        PAGECACHE_PLAIN_PAGE,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        PAGECACHE_PIN_LEFT_UNPINNED, PAGECACHE_WRITE_DONE, 0))
    {
      UNRECOVERABLE_ERROR(("Can't write page (%lu,0x%lx) to pagecache",
                           (ulong) buffer->file,
                           (ulong) (LSN_OFFSET(buffer->offset)+ i)));
    }
  }
  if (my_pwrite(buffer->file, (char*) buffer->buffer,
                buffer->size, LSN_OFFSET(buffer->offset),
                MYF(MY_WME | MY_NABP)))
  {
    UNRECOVERABLE_ERROR(("Can't write buffer (%lu,0x%lx) size %lu "
                         "to the disk (%d)",
                         (ulong) buffer->file,
                         (ulong) LSN_OFFSET(buffer->offset),
                         (ulong) buffer->size, errno));
    DBUG_RETURN(1);
  }
  if (LSN_OFFSET(buffer->last_lsn) != 0)      /* if buffer->last_lsn is set */
    translog_set_sent_to_file(&buffer->last_lsn);

  /* Free buffer */
  buffer->file= -1;
  buffer->overlay= 0;
  if (buffer->waiting_filling_buffer.last_thread)
  {
    wqueue_release_queue(&buffer->waiting_filling_buffer);
  }
  DBUG_RETURN(0);
}


/*
  Recover page with sector protection (wipe out failed chunks)

  SYNOPSYS
    translog_recover_page_up_to_sector()
    page                 reference on the page
    offset               offset of failed sector

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_recover_page_up_to_sector(byte *page, uint16 offset)
{
  uint16 chunk_offset= translog_get_first_chunk_offset(page), valid_chunk_end;
  DBUG_ENTER("translog_recover_page_up_to_sector");
  DBUG_PRINT("enter", ("offset: %u  first chunk: %u",
                       (uint) offset, (uint) chunk_offset));

  while (page[chunk_offset] != '\0' && chunk_offset < offset)
  {
    uint16 chunk_length;
    if ((chunk_length=
         translog_get_total_chunk_length(page, chunk_offset)) == 0)
    {
      UNRECOVERABLE_ERROR(("cant get chunk length (offset %u)",
                           (uint) chunk_offset));
      DBUG_RETURN(1);
    }
    DBUG_PRINT("info", ("chunk: offset: %u  length %u",
                        (uint) chunk_offset, (uint) chunk_length));
    if (((ulong) chunk_offset) + ((ulong) chunk_length) > TRANSLOG_PAGE_SIZE)
    {
      UNRECOVERABLE_ERROR(("damaged chunk (offset %u) in trusted area",
                           (uint) chunk_offset));
      DBUG_RETURN(1);
    }
    chunk_offset+= chunk_length;
  }

  valid_chunk_end= chunk_offset;
  /* end of trusted area - sector parsing */
  while (page[chunk_offset] != '\0')
  {
    uint16 chunk_length;
    if ((chunk_length=
         translog_get_total_chunk_length(page, chunk_offset)) == 0)
      break;

    DBUG_PRINT("info", ("chunk: offset: %u  length %u",
                        (uint) chunk_offset, (uint) chunk_length));
    if (((ulong) chunk_offset) + ((ulong) chunk_length) >
        (uint) (offset + DISK_DRIVE_SECTOR_SIZE))
      break;

    chunk_offset+= chunk_length;
    valid_chunk_end= chunk_offset;
  }
  DBUG_PRINT("info", ("valid chunk end offset: %u", (uint) valid_chunk_end));

  bzero(page + valid_chunk_end, TRANSLOG_PAGE_SIZE - valid_chunk_end);

  DBUG_RETURN(0);
}


/*
  Log page validator

  SYNOPSIS
    translog_page_validator()
    page_addr            The page to check
    data                 data, need for validation (address in this case)

  RETURN
    0  OK
    1  Error
*/
static my_bool translog_page_validator(byte *page_addr, gptr data_ptr)
{
  uint this_page_page_overhead;
  uint flags;
  byte *page= (byte*) page_addr, *page_pos;
  TRANSLOG_VALIDATOR_DATA *data= (TRANSLOG_VALIDATOR_DATA *) data_ptr;
  TRANSLOG_ADDRESS addr= *(data->addr);
  DBUG_ENTER("translog_page_validator");

  data->was_recovered= 0;

  if (uint3korr(page) != LSN_OFFSET(addr) / TRANSLOG_PAGE_SIZE ||
      uint3korr(page + 3) != LSN_FILE_NO(addr))
  {
    UNRECOVERABLE_ERROR(("Page (%lu,0x%lx): "
                         "page address written in the page is incorrect: "
                         "File %lu instead of %lu or page %lu instead of %lu",
                         (ulong) LSN_FILE_NO(addr), (ulong) LSN_OFFSET(addr),
                         (ulong) uint3korr(page + 3), (ulong) LSN_FILE_NO(addr),
                         (ulong) uint3korr(page),
                         (ulong) LSN_OFFSET(addr) / TRANSLOG_PAGE_SIZE));
    DBUG_RETURN(1);
  }
  flags= (uint)(page[TRANSLOG_PAGE_FLAGS]);
  this_page_page_overhead= page_overhead[flags];
  if (flags & ~(TRANSLOG_PAGE_CRC | TRANSLOG_SECTOR_PROTECTION |
                TRANSLOG_RECORD_CRC))
  {
    UNRECOVERABLE_ERROR(("Page (%lu,0x%lx): "
                         "Garbage in the page flags field detected : %x",
                         (ulong) LSN_FILE_NO(addr), (ulong) LSN_OFFSET(addr),
                         (uint) flags));
    DBUG_RETURN(1);
  }
  page_pos= page + (3 + 3 + 1);
  if (flags & TRANSLOG_PAGE_CRC)
  {
    uint32 crc= translog_crc(page + this_page_page_overhead,
                             TRANSLOG_PAGE_SIZE -
                             this_page_page_overhead);
    if (crc != uint4korr(page_pos))
    {
      UNRECOVERABLE_ERROR(("Page (%lu,0x%lx): "
                           "CRC mismatch: calculated: %lx on the page %lx",
                           (ulong) LSN_FILE_NO(addr), (ulong) LSN_OFFSET(addr),
                           (ulong) crc, (ulong) uint4korr(page_pos)));
      DBUG_RETURN(1);
    }
    page_pos+= CRC_LENGTH;                      /* Skip crc */
  }
  if (flags & TRANSLOG_SECTOR_PROTECTION)
  {
    uint i, offset;
    byte *table= page_pos;
    uint16 current= uint2korr(table);
    for (i= 2, offset= DISK_DRIVE_SECTOR_SIZE;
         i < (TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE) * 2;
         i+= 2, offset+= DISK_DRIVE_SECTOR_SIZE)
    {
      /*
         TODO: add chunk counting for "suspecting" sectors (difference is
         more than 1-2)
      */
      uint16 test= uint2korr(page + offset);
      DBUG_PRINT("info", ("sector: #%u  offset: %u  current: %lx "
                          "read: 0x%x  stored: 0x%x%x",
                          i / 2, offset, (ulong) current,
                          (uint) uint2korr(page + offset), (uint) table[i],
                          (uint) table[i + 1]));
      if (((test < current) &&
           (LL(0xFFFF) - current + test > DISK_DRIVE_SECTOR_SIZE / 3)) ||
          ((test >= current) &&
           (test - current > DISK_DRIVE_SECTOR_SIZE / 3)))
      {
        if (translog_recover_page_up_to_sector(page, offset))
          DBUG_RETURN(1);
        data->was_recovered= 1;
        DBUG_RETURN(0);
      }

      /* Return value on the page */
      page[offset]= table[i];
      page[offset + 1]= table[i + 1];
      current= test;
      DBUG_PRINT("info", ("sector: #%u  offset: %u  current: %lx  "
                          "read: 0x%x  stored: 0x%x%x",
                          i / 2, offset, (ulong) current,
                          (uint) uint2korr(page + offset), (uint) table[i],
                          (uint) table[i + 1]));
    }
  }
  DBUG_RETURN(0);
}


/*
  Get log page by file number and offset of the beginning of the page

  SYNOPSIS
    translog_get_page()
    data                 validator data, which contains the page address
    buffer               buffer for page placing
                         (might not be used in some cache implementations)

  RETURN
    NULL - Error
    #      pointer to the page cache which should be used to read this page
*/

static byte *translog_get_page(TRANSLOG_VALIDATOR_DATA *data, byte *buffer)
{
  TRANSLOG_ADDRESS addr= *(data->addr);
  uint cache_index;
  uint32 file_no= LSN_FILE_NO(addr);
  DBUG_ENTER("translog_get_page");
  DBUG_PRINT("enter", ("File: %lu  Offset: %lu(0x%lx)",
                       (ulong) file_no,
                       (ulong) LSN_OFFSET(addr),
                       (ulong) LSN_OFFSET(addr)));

  /* it is really page address */
  DBUG_ASSERT(LSN_OFFSET(addr) % TRANSLOG_PAGE_SIZE == 0);

  if ((cache_index= LSN_FILE_NO(log_descriptor.horizon) - file_no) <
      OPENED_FILES_NUM)
  {
    PAGECACHE_FILE file;
    /* file in the cache */
    if (log_descriptor.log_file_num[cache_index] == -1)
    {
      if ((log_descriptor.log_file_num[cache_index]=
           open_logfile_by_number_no_cache(file_no)) == -1)
        DBUG_RETURN(NULL);
    }
    file.file= log_descriptor.log_file_num[cache_index];

    buffer= (byte*)
      pagecache_valid_read(log_descriptor.pagecache, &file,
                           LSN_OFFSET(addr) / TRANSLOG_PAGE_SIZE,
                           3, (char*) buffer,
                           PAGECACHE_PLAIN_PAGE,
                           PAGECACHE_LOCK_LEFT_UNLOCKED, 0,
                           &translog_page_validator, (gptr) data);
  }
  else
  {
    /*
      TODO: WE KEEP THE LAST OPENED_FILES_NUM FILES IN THE LOG CACHE, NOT
      THE LAST USED FILES.  THIS WILL BE A NOTABLE PROBLEM IF WE ARE
      FOLLOWING AN UNDO CHAIN THAT GOES OVER MANY OLD LOG FILES.  WE WILL
      PROBABLY NEED SPECIAL HANDLING OF THIS OR HAVE A FILO FOR THE LOG
      FILES.
    */

    File file= open_logfile_by_number_no_cache(file_no);
    if (file == -1)
        DBUG_RETURN(NULL);
    if (my_pread(file, (char*) buffer, TRANSLOG_PAGE_SIZE,
                 LSN_OFFSET(addr), MYF(MY_FNABP | MY_WME)))
      buffer= NULL;
    else if (translog_page_validator((byte*) buffer, (gptr) data))
      buffer= NULL;
    my_close(file, MYF(MY_WME));
  }
  DBUG_RETURN(buffer);
}


/*
  Finds last page of the given log file

  SYNOPSIS
    translog_get_last_page_addr()
    addr                 address structure to fill with data, which contain
                         file number of the log file
    last_page_ok         assigned 1 if last page was OK

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_get_last_page_addr(TRANSLOG_ADDRESS *addr,
                                           my_bool *last_page_ok)
{
  MY_STAT stat_buff, *stat;
  char path[FN_REFLEN];
  uint32 rec_offset;
  uint32 file_no= LSN_FILE_NO(*addr);
  DBUG_ENTER("translog_get_last_page_addr");

  if (!(stat= my_stat(translog_filename_by_fileno(file_no, path),
                      &stat_buff, MYF(MY_WME))))
    DBUG_RETURN(1);
  DBUG_PRINT("info", ("File size: %lu", (ulong) stat->st_size));
  if (stat->st_size > TRANSLOG_PAGE_SIZE)
  {
    rec_offset= (((stat->st_size / TRANSLOG_PAGE_SIZE) - 1) *
                       TRANSLOG_PAGE_SIZE);
    *last_page_ok= (stat->st_size == rec_offset + TRANSLOG_PAGE_SIZE);
  }
  else
  {
    *last_page_ok= 0;
    rec_offset= 0;
  }
  *addr= MAKE_LSN(file_no, rec_offset);
  DBUG_PRINT("info", ("Last page: 0x%lx  ok: %d", (ulong) rec_offset,
                      *last_page_ok));
  DBUG_RETURN(0);
}


/*
  Get number bytes for record length storing

  SYNOPSIS
    translog_variable_record_length_bytes()
    length              Record length wich will be codded

  RETURN
    1,3,4,5 - number of bytes to store given length
*/

static uint translog_variable_record_length_bytes(translog_size_t length)
{
  if (length < 250)
    return 1;
  if (length < 0xFFFF)
    return 3;
  if (length < (ulong) 0xFFFFFF)
    return 4;
  return 5;
}


/*
  Get header of this chunk

  SYNOPSIS
    translog_get_chunk_header_length()
    page                 The page where chunk placed
    offset               Offset of the chunk on this place

  RETURN
    #  total length of the chunk
    0  Error
*/

static uint16 translog_get_chunk_header_length(byte *page, uint16 offset)
{
  DBUG_ENTER("translog_get_chunk_header_length");
  page+= offset;
  switch (*page & TRANSLOG_CHUNK_TYPE) {
  case TRANSLOG_CHUNK_LSN:
  {
    /* 0 chunk referred as LSN (head or tail) */
    translog_size_t rec_len;
    byte *start= page;
    byte *ptr= start + 1 + 2;
    uint16 chunk_len, header_len;
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LSN"));
    rec_len= translog_variable_record_1group_decode_len(&ptr);
    chunk_len= uint2korr(ptr);
    header_len= (ptr - start) +2;
    DBUG_PRINT("info", ("rec len: %lu  chunk len: %u  header len: %u",
                        (ulong) rec_len, (uint) chunk_len, (uint) header_len));
    if (chunk_len)
    {
      /* TODO: fine header end */
      DBUG_ASSERT(0);
    }
    DBUG_RETURN(header_len);
    break;
  }
  case TRANSLOG_CHUNK_FIXED:
  {
    /* 1 (pseudo)fixed record (also LSN) */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_FIXED = 3"));
    DBUG_RETURN(3);
  }
  case TRANSLOG_CHUNK_NOHDR:
    /* 2 no header chunk (till page end) */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_NOHDR = 1"));
    DBUG_RETURN(1);
    break;
  case TRANSLOG_CHUNK_LNGTH:
    /* 3 chunk with chunk length */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LNGTH = 3"));
    DBUG_RETURN(3);
    break;
  default:
    DBUG_ASSERT(0);
  }
}


/*
  Initialize transaction log

  SYNOPSIS
    translog_init()
    directory            Directory where log files are put
    log_file_max_size    max size of one log size (for new logs creation)
    server_version       version of MySQL server (MYSQL_VERSION_ID)
    server_id            server ID (replication & Co)
    pagecache            Page cache for the log reads
    flags                flags (TRANSLOG_PAGE_CRC, TRANSLOG_SECTOR_PROTECTION
                           TRANSLOG_RECORD_CRC)

  RETURN
    0  OK
    1  Error
*/

my_bool translog_init(const char *directory,
                      uint32 log_file_max_size,
                      uint32 server_version,
                      uint32 server_id, PAGECACHE *pagecache, uint flags)
{
  int i;
  int old_log_was_recovered= 0, logs_found= 0;
  uint old_flags= flags;
  TRANSLOG_ADDRESS sure_page, last_page, last_valid_page;
  DBUG_ENTER("translog_init");


  if (pthread_mutex_init(&log_descriptor.sent_to_file_lock,
                         MY_MUTEX_INIT_FAST))
    DBUG_RETURN(1);

  /* Directory to store files */
  unpack_dirname(log_descriptor.directory, directory);

  if ((log_descriptor.directory_fd= my_open(log_descriptor.directory,
                                            O_RDONLY, MYF(MY_WME))) < 0)
  {
    UNRECOVERABLE_ERROR(("Error %d during opening directory '%s'",
                         errno, log_descriptor.directory));
    DBUG_RETURN(1);
  }

  /* max size of one log size (for new logs creation) */
  log_descriptor.log_file_max_size=
    log_file_max_size - (log_file_max_size % TRANSLOG_PAGE_SIZE);
  /* server version */
  log_descriptor.server_version= server_version;
  /* server ID */
  log_descriptor.server_id= server_id;
  /* Page cache for the log reads */
  log_descriptor.pagecache= pagecache;
  /* Flags */
  DBUG_ASSERT((flags &
               ~(TRANSLOG_PAGE_CRC | TRANSLOG_SECTOR_PROTECTION |
                 TRANSLOG_RECORD_CRC)) == 0);
  log_descriptor.flags= flags;
  for (i= 0; i < TRANSLOG_FLAGS_NUM; i++)
  {
     page_overhead[i]= 7;
     if (i & TRANSLOG_PAGE_CRC)
       page_overhead[i]+= CRC_LENGTH;
     if (i & TRANSLOG_SECTOR_PROTECTION)
       page_overhead[i]+= (TRANSLOG_PAGE_SIZE /
                           DISK_DRIVE_SECTOR_SIZE) * 2;
  }
  log_descriptor.page_overhead= page_overhead[flags];
  log_descriptor.page_capacity_chunk_2=
    TRANSLOG_PAGE_SIZE - log_descriptor.page_overhead - 1;
  DBUG_ASSERT(TRANSLOG_WRITE_BUFFER % TRANSLOG_PAGE_SIZE == 0);
  log_descriptor.buffer_capacity_chunk_2=
    (TRANSLOG_WRITE_BUFFER / TRANSLOG_PAGE_SIZE) *
    log_descriptor.page_capacity_chunk_2;
  log_descriptor.half_buffer_capacity_chunk_2=
    log_descriptor.buffer_capacity_chunk_2 / 2;
  DBUG_PRINT("info",
             ("Overhead: %u  pc2: %u  bc2: %u,  bc2/2: %u",
              log_descriptor.page_overhead,
              log_descriptor.page_capacity_chunk_2,
              log_descriptor.buffer_capacity_chunk_2,
              log_descriptor.half_buffer_capacity_chunk_2));

  /* *** Current state of the log handler *** */

  /* Init log handler file handlers cache */
  for (i= 0; i < OPENED_FILES_NUM; i++)
    log_descriptor.log_file_num[i]= -1;

  /* just to init it somehow */
  translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);

  /* Buffers for log writing */
  for (i= 0; i < TRANSLOG_BUFFERS_NO; i++)
  {
    if (translog_buffer_init(log_descriptor.buffers + i))
      DBUG_RETURN(1);
#ifndef DBUG_OFF
    log_descriptor.buffers[i].buffer_no= (uint8) i;
#endif
    DBUG_PRINT("info", ("translog_buffer buffer #%u: 0x%lx",
                        i, (ulong) log_descriptor.buffers + i));
  }

  logs_found= (last_logno != CONTROL_FILE_IMPOSSIBLE_FILENO);

  if (logs_found)
  {
    my_bool pageok;
    /*
      TODO: scan directory for maria_log.XXXXXXXX files and find
       highest XXXXXXXX & set logs_found
      TODO: check that last checkpoint within present log addresses space

       find the log end
    */
    if (LSN_FILE_NO(last_checkpoint_lsn) == CONTROL_FILE_IMPOSSIBLE_FILENO)
    {
      DBUG_ASSERT(LSN_OFFSET(last_checkpoint_lsn) == 0);
      /* there was no checkpoints we will read from the beginning */
      sure_page= (LSN_ONE_FILE | TRANSLOG_PAGE_SIZE);
    }
    else
    {
      sure_page= last_checkpoint_lsn;
      DBUG_ASSERT(LSN_OFFSET(sure_page) % TRANSLOG_PAGE_SIZE != 0);
      sure_page-= LSN_OFFSET(sure_page) % TRANSLOG_PAGE_SIZE;
    }
    log_descriptor.horizon= last_page= MAKE_LSN(last_logno,0);
    if (translog_get_last_page_addr(&last_page, &pageok))
      DBUG_RETURN(1);
    if (LSN_OFFSET(last_page) == 0)
    {
      if (LSN_FILE_NO(last_page) == 1)
      {
        logs_found= 0;                          /* file #1 has no pages */
      }
      else
      {
        last_page-= LSN_ONE_FILE;
        if (translog_get_last_page_addr(&last_page, &pageok))
          DBUG_RETURN(1);
      }
    }
  }
  if (logs_found)
  {
    TRANSLOG_ADDRESS current_page= sure_page;
    my_bool pageok;

    DBUG_ASSERT(sure_page <= last_page);

    /* TODO: check page size */

    last_valid_page= CONTROL_FILE_IMPOSSIBLE_LSN;
    /* scan and validate pages */
    do
    {
      TRANSLOG_ADDRESS current_file_last_page;
      current_file_last_page= current_page;
      if (translog_get_last_page_addr(&current_file_last_page, &pageok))
        DBUG_RETURN(1);
      if (!pageok)
      {
        DBUG_PRINT("error", ("File %lu have no complete last page",
                             (ulong) LSN_FILE_NO(current_file_last_page)));
        old_log_was_recovered= 1;
        /* This file is not written till the end so it should be last */
        last_page= current_file_last_page;
        /* TODO: issue warning */
      }
      do
      {
        TRANSLOG_VALIDATOR_DATA data;
        byte buffer[TRANSLOG_PAGE_SIZE], *page;
        data.addr= &current_page;
        if ((page= translog_get_page(&data, buffer)) == NULL)
          DBUG_RETURN(1);
        if (data.was_recovered)
        {
          DBUG_PRINT("error", ("file no: %lu (%d)  "
                               "rec_offset: 0x%lx (%lu) (%d)",
                               (ulong) LSN_FILE_NO(current_page),
                               (uint3korr(page + 3) !=
                                LSN_FILE_NO(current_page)),
                               (ulong) LSN_OFFSET(current_page),
                               (ulong) (LSN_OFFSET(current_page) /
                                        TRANSLOG_PAGE_SIZE),
                               (uint3korr(page) !=
                                LSN_OFFSET(current_page) /
                                TRANSLOG_PAGE_SIZE)));
          old_log_was_recovered= 1;
          break;
        }
        old_flags= page[TRANSLOG_PAGE_FLAGS];
        last_valid_page= current_page;
        current_page+= TRANSLOG_PAGE_SIZE; /* increase offset */
      } while (current_page <= current_file_last_page);
      current_page+= LSN_ONE_FILE;
      current_page= LSN_REPLACE_OFFSET(current_page, TRANSLOG_PAGE_SIZE);
    } while (LSN_FILE_NO(current_page) <= LSN_FILE_NO(last_page) &&
             !old_log_was_recovered);
    if (last_valid_page == CONTROL_FILE_IMPOSSIBLE_LSN)
    {
      /* Panic!!! Even page which should be valid is invalid */
      /* TODO: issue error */
      DBUG_RETURN(1);
    }
    DBUG_PRINT("info", ("Last valid page is in file: %lu  "
                        "offset: %lu (0x%lx)  "
                        "Logs found: %d  was recovered: %d  "
                        "flags match: %d",
                        (ulong) LSN_FILE_NO(last_valid_page),
                        (ulong) LSN_OFFSET(last_valid_page),
                        (ulong) LSN_OFFSET(last_valid_page),
                        logs_found, old_log_was_recovered,
                        (old_flags == flags)));

    /* TODO: check server ID */
    if (logs_found && !old_log_was_recovered && old_flags == flags)
    {
      TRANSLOG_VALIDATOR_DATA data;
      byte buffer[TRANSLOG_PAGE_SIZE], *page;
      uint16 chunk_offset;
      data.addr= &last_valid_page;
      /* continue old log */
      DBUG_ASSERT(LSN_FILE_NO(last_valid_page)==
                  LSN_FILE_NO(log_descriptor.horizon));
      if ((page= translog_get_page(&data, buffer)) == NULL ||
          (chunk_offset= translog_get_first_chunk_offset(page)) == 0)
        DBUG_RETURN(1);

      /* Puts filled part of old page in the buffer */
      log_descriptor.horizon= last_valid_page;
      translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
      /*
         Free space if filled with 0 and first byte of
         real chunk can't be 0
      */
      while (chunk_offset < TRANSLOG_PAGE_SIZE && page[chunk_offset] != '\0')
      {
        uint16 chunk_length;
        if ((chunk_length=
             translog_get_total_chunk_length(page, chunk_offset)) == 0)
          DBUG_RETURN(1);
        DBUG_PRINT("info", ("chunk: offset: %u  length: %u",
                            (uint) chunk_offset, (uint) chunk_length));
        chunk_offset+= chunk_length;

        /* chunk can't cross the page border */
        DBUG_ASSERT(chunk_offset <= TRANSLOG_PAGE_SIZE);
      }
      memcpy(log_descriptor.buffers->buffer, page, chunk_offset);
      log_descriptor.bc.buffer->size+= chunk_offset;
      log_descriptor.bc.ptr+= chunk_offset;
      log_descriptor.bc.current_page_fill= chunk_offset;
      log_descriptor.horizon= LSN_REPLACE_OFFSET(log_descriptor.horizon,
                                                 (chunk_offset +
                                                  LSN_OFFSET(last_valid_page)));
      DBUG_PRINT("info", ("Move Page #%u: 0x%lx  chaser: %d  Size: %lu (%lu)",
                          (uint) log_descriptor.bc.buffer_no,
                          (ulong) log_descriptor.bc.buffer,
                          log_descriptor.bc.chaser,
                          (ulong) log_descriptor.bc.buffer->size,
                          (ulong) (log_descriptor.bc.ptr - log_descriptor.bc.
                                   buffer->buffer)));
      DBUG_ASSERT(log_descriptor.bc.chaser ||
                  ((ulong)
                   (log_descriptor.bc.ptr -log_descriptor.bc.buffer->buffer) ==
                   log_descriptor.bc.buffer->size));
      DBUG_ASSERT(log_descriptor.bc.buffer->buffer_no ==
                  log_descriptor.bc.buffer_no);
      DBUG_ASSERT(log_descriptor.bc.current_page_fill <= TRANSLOG_PAGE_SIZE);
    }
  }
  DBUG_PRINT("info", ("Logs found: %d  was recovered: %d",
                      logs_found, old_log_was_recovered));
  if (!logs_found)
  {
    /* Start new log system from scratch */
    /* Used space */
    log_descriptor.horizon= MAKE_LSN(1, TRANSLOG_PAGE_SIZE);    // header page
    /* Current logs file number in page cache */
    if ((log_descriptor.log_file_num[0]=
         open_logfile_by_number_no_cache(1)) == -1 ||
        translog_write_file_header())
      DBUG_RETURN(1);
    if (ma_control_file_write_and_force(CONTROL_FILE_IMPOSSIBLE_LSN, 1,
                                        CONTROL_FILE_UPDATE_ONLY_LOGNO))
      DBUG_RETURN(1);
    /* assign buffer 0 */
    translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
    translog_new_page_header(&log_descriptor.horizon, &log_descriptor.bc);
  }
  else if (old_log_was_recovered || old_flags != flags)
  {
    /* leave the damaged file untouched */
    log_descriptor.horizon+= LSN_ONE_FILE;
    /* header page */
    log_descriptor.horizon= LSN_REPLACE_OFFSET(log_descriptor.horizon,
                                               TRANSLOG_PAGE_SIZE);
    if (translog_create_new_file())
      DBUG_RETURN(1);
    /*
      Buffer system left untouched after recovery => we should init it
      (starting from buffer 0)
    */
    translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
    translog_new_page_header(&log_descriptor.horizon, &log_descriptor.bc);
  }

  /* all LSNs that are on disk are flushed */
  log_descriptor.sent_to_file= log_descriptor.flushed= log_descriptor.horizon;
  /*
    horizon is (potentially) address of the next LSN we need decrease
    it to signal that all LSNs before it are flushed
  */
  log_descriptor.flushed--; /* offset decreased */
  log_descriptor.sent_to_file--; /* offset decreased */

  DBUG_RETURN(0);
}


/*
  Free transaction log file buffer

  SYNOPSIS
    translog_buffer_destroy()
    buffer_no            The buffer to free

  NOTE
    This buffer should be locked
*/

static void translog_buffer_destroy(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_destroy");
  DBUG_PRINT("enter",
             ("Buffer #%u: 0x%lx  file: %d  offset: (%lu,0x%lx)  size: %lu",
              (uint) buffer->buffer_no, (ulong) buffer,
              buffer->file,
              (ulong) LSN_FILE_NO(buffer->offset),
              (ulong) LSN_OFFSET(buffer->offset),
              (ulong) buffer->size));
  DBUG_ASSERT(buffer->waiting_filling_buffer.last_thread == 0);
  if (buffer->file != -1)
  {
    /*
       We ignore errors here, because we can't do something about it
       (it is shutting down)
    */
    translog_buffer_flush(buffer);
  }
  DBUG_PRINT("info", ("Unlock mutex: 0x%lx", (ulong) &buffer->mutex));
  pthread_mutex_unlock(&buffer->mutex);
  DBUG_PRINT("info", ("Destroy mutex: 0x%lx", (ulong) &buffer->mutex));
  pthread_mutex_destroy(&buffer->mutex);
  DBUG_VOID_RETURN;
}


/*
  Free log handler resources

  SYNOPSIS
    translog_destroy()
*/

void translog_destroy()
{
  uint i;
  DBUG_ENTER("translog_destroy");
  if (log_descriptor.bc.buffer->file != -1)
    translog_finish_page(&log_descriptor.horizon, &log_descriptor.bc);

  for (i= 0; i < TRANSLOG_BUFFERS_NO; i++)
  {
    struct st_translog_buffer *buffer= log_descriptor.buffers + i;
    /*
      Lock the buffer just for safety, there should not be other
      threads running.
    */
    translog_buffer_lock(buffer);
    translog_buffer_destroy(buffer);
  }
  /* close files */
  for (i= 0; i < OPENED_FILES_NUM; i++)
  {
    if (log_descriptor.log_file_num[i] != -1)
      translog_close_log_file(log_descriptor.log_file_num[i]);
  }
  pthread_mutex_destroy(&log_descriptor.sent_to_file_lock);
  my_close(log_descriptor.directory_fd, MYF(MY_WME));
  DBUG_VOID_RETURN;
}


/*
  Lock the loghandler

  SYNOPSIS
    translog_lock()

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_lock()
{
  struct st_translog_buffer *current_buffer;
  DBUG_ENTER("translog_lock");

  /*
     Locking the loghandler mean locking current buffer, but it can change
     during locking, so we should check it
  */
  for (;;)
  {
    current_buffer= log_descriptor.bc.buffer;
    if (translog_buffer_lock(current_buffer))
      DBUG_RETURN(1);
    if (log_descriptor.bc.buffer == current_buffer)
      break;
    translog_buffer_unlock(current_buffer);
  }
  DBUG_RETURN(0);
}


/*
  Unlock the loghandler

  SYNOPSIS
    translog_unlock()

  RETURN
    0  OK
    1  Error
*/

static inline my_bool translog_unlock()
{
  DBUG_ENTER("translog_unlock");
  translog_buffer_unlock(log_descriptor.bc.buffer);

  DBUG_RETURN(0);
}


/*
  Start new page

  SYNOPSIS
    translog_page_next()
    horizon              \ Position in file and buffer where we are
    cursor               /
    prev_buffer          Buffer which should be flushed will be assigned
                         here if it is need. This is always set.

  NOTE
    handler should be locked

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_page_next(TRANSLOG_ADDRESS *horizon,
                                  struct st_buffer_cursor *cursor,
                                  struct st_translog_buffer **prev_buffer)
{
  struct st_translog_buffer *buffer= cursor->buffer;
  DBUG_ENTER("translog_page_next");

  if ((cursor->ptr +TRANSLOG_PAGE_SIZE >
       cursor->buffer->buffer + TRANSLOG_WRITE_BUFFER) ||
      (LSN_OFFSET(*horizon) >
       log_descriptor.log_file_max_size - TRANSLOG_PAGE_SIZE))
  {
    DBUG_PRINT("info", ("Switch to next buffer  Buffer Size: %lu (%lu) => %d  "
                        "File size: %lu  max: %lu => %d",
                        (ulong) cursor->buffer->size,
                        (ulong) (cursor->ptr - cursor->buffer->buffer),
                        (cursor->ptr + TRANSLOG_PAGE_SIZE >
                         cursor->buffer->buffer + TRANSLOG_WRITE_BUFFER),
                        (ulong) LSN_OFFSET(*horizon),
                        (ulong) log_descriptor.log_file_max_size,
                        (LSN_OFFSET(*horizon) >
                         (log_descriptor.log_file_max_size -
                          TRANSLOG_PAGE_SIZE))));
    if (translog_buffer_next(horizon, cursor,
                             LSN_OFFSET(*horizon) >
                             (log_descriptor.log_file_max_size -
                              TRANSLOG_PAGE_SIZE)))
      DBUG_RETURN(1);
    *prev_buffer= buffer;
    DBUG_PRINT("info", ("Buffer #%u (0x%lu): have to be flushed",
                        (uint) buffer->buffer_no, (ulong) buffer));
  }
  else
  {
    DBUG_PRINT("info", ("Use the same buffer #%u (0x%lu): "
                        "Buffer Size: %lu (%lu)",
                        (uint) buffer->buffer_no,
                        (ulong) buffer,
                        (ulong) cursor->buffer->size,
                        (ulong) (cursor->ptr - cursor->buffer->buffer)));
    translog_finish_page(horizon, cursor);
    translog_new_page_header(horizon, cursor);
    *prev_buffer= NULL;
  }
  DBUG_RETURN(0);
}


/*
  Write data of given length to the current page

  SYNOPSIS
    translog_write_data_on_page()
    horizon              \ Pointers on file and buffer
    cursor               /
    length               IN     length of the chunk
    buffer               buffer with data

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_write_data_on_page(TRANSLOG_ADDRESS *horizon,
                                           struct st_buffer_cursor *cursor,
                                           translog_size_t length,
                                           byte *buffer)
{
  DBUG_ENTER("translog_write_data_on_page");
  DBUG_PRINT("enter", ("Chunk length: %lu  Page size %u",
                       (ulong) length, (uint) cursor->current_page_fill));
  DBUG_ASSERT(length > 0);
  DBUG_ASSERT(length + cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(length + cursor->ptr <=cursor->buffer->buffer +
              TRANSLOG_WRITE_BUFFER);

  memcpy(cursor->ptr, buffer, length);
  cursor->ptr+= length;
  (*horizon)+= length; /* adds offset */
  cursor->current_page_fill+= length;
  if (!cursor->chaser)
    cursor->buffer->size+= length;
  DBUG_PRINT("info", ("Write data buffer #%u: 0x%lx  "
                      "chaser: %d  Size: %lu (%lu)",
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer)));
  DBUG_ASSERT(cursor->chaser ||
              ((ulong) (cursor->ptr - cursor->buffer->buffer) ==
               cursor->buffer->size));
  DBUG_ASSERT(cursor->buffer->buffer_no == cursor->buffer_no);
  DBUG_ASSERT(cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);

  DBUG_RETURN(0);
}


/*
  Write data from parts of given length to the current page

  SYNOPSIS
    translog_write_parts_on_page()
    horizon              \ Pointers on file and buffer
    cursor               /
    length               IN     length of the chunk
    parts                IN/OUT chunk source

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_write_parts_on_page(TRANSLOG_ADDRESS *horizon,
                                            struct st_buffer_cursor *cursor,
                                            translog_size_t length,
                                            struct st_translog_parts *parts)
{
  translog_size_t left= length;
  uint cur= (uint) parts->current;
  DBUG_ENTER("translog_write_parts_on_page");
  DBUG_PRINT("enter", ("Chunk length: %lu  parts: %u of %u. Page size: %u  "
                       "Buffer size: %lu (%lu)",
                       (ulong) length,
                       (uint) (cur + 1), (uint) parts->parts.elements,
                       (uint) cursor->current_page_fill,
                       (ulong) cursor->buffer->size,
                       (ulong) (cursor->ptr - cursor->buffer->buffer)));
  DBUG_ASSERT(length > 0);
  DBUG_ASSERT(length + cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(length + cursor->ptr <=cursor->buffer->buffer +
              TRANSLOG_WRITE_BUFFER);

  do
  {
    translog_size_t len;
    struct st_translog_part *part;
    byte *buff;

    DBUG_ASSERT(cur < parts->parts.elements);
    part= dynamic_element(&parts->parts, cur, struct st_translog_part *);
    buff= part->buff;
    DBUG_PRINT("info", ("Part: %u  Length: %lu  left: %lu",
                        (uint) (cur + 1), (ulong) part->len, (ulong) left));

    if (part->len > left)
    {
      /* we should write less then the current part */
      len= left;
      part->len-= len;
      part->buff+= len;
      DBUG_PRINT("info", ("Set new part: %u  Length: %lu",
                          (uint) (cur + 1), (ulong) part->len));
    }
    else
    {
      len= part->len;
      cur++;
      DBUG_PRINT("info", ("moved to next part (len: %lu)", (ulong) len));
    }
    DBUG_PRINT("info", ("copy: 0x%lx <- 0x%lx  %u",
                        (ulong) cursor->ptr, (ulong)buff, (uint)len));
    memcpy(cursor->ptr, buff, len);
    left-= len;
    cursor->ptr+= len;
  } while (left);

  DBUG_PRINT("info", ("Horizon: (%lu,0x%lx)  Length %lu(0x%lx)",
                      (ulong) LSN_FILE_NO(*horizon),
                      (ulong) LSN_OFFSET(*horizon),
                      (ulong) length, (ulong) length));
  parts->current= cur;
  (*horizon)+= length; /* offset increasing */
  cursor->current_page_fill+= length;
  if (!cursor->chaser)
    cursor->buffer->size+= length;
  DBUG_PRINT("info", ("Write parts buffer #%u: 0x%lx  "
                      "chaser: %d  Size: %lu (%lu)  "
                      "Horizon: (%lu,0x%lx)  buff offset: 0x%lx",
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer),
                      (ulong) LSN_FILE_NO(*horizon),
                      (ulong) LSN_OFFSET(*horizon),
                      (ulong) (LSN_OFFSET(cursor->buffer->offset) +
                               cursor->buffer->size)));
  /*
    TODO: make one check function for the buffer/loghandler
  */

  DBUG_ASSERT(cursor->chaser ||
              ((ulong) (cursor->ptr - cursor->buffer->buffer) ==
               cursor->buffer->size));
  DBUG_ASSERT(cursor->buffer->buffer_no == cursor->buffer_no);
  DBUG_ASSERT((cursor->ptr -cursor->buffer->buffer) %TRANSLOG_PAGE_SIZE ==
              cursor->current_page_fill % TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);

  DBUG_RETURN(0);
}


/*
  Put 1 group chunk type 0 header into parts array

  SYNOPSIS
    translog_write_variable_record_1group_header()
    parts                Descriptor of record source parts
    type                 The log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    header_length        Calculated header length of chunk type 0
    chunk0_header        Buffer for the chunk header writing
*/

static void
translog_write_variable_record_1group_header(struct st_translog_parts *parts,
                                             enum translog_record_type type,
                                             SHORT_TRANSACTION_ID short_trid,
                                             uint16 header_length,
                                             byte *chunk0_header)
{
  struct st_translog_part part;
  DBUG_ASSERT(parts->current != 0);     /* first part is left for header */
  parts->total_record_length+= (part.len= header_length);
  part.buff= chunk0_header;
  /* puts chunk type */
  *chunk0_header= (byte) (type | TRANSLOG_CHUNK_LSN);
  int2store(chunk0_header + 1, short_trid);
  /* puts record length */
  translog_write_variable_record_1group_code_len(chunk0_header + 3,
                                                 parts->record_length,
                                                 header_length);
  /* puts 0 as chunk length which indicate 1 group record */
  int2store(chunk0_header + header_length - 2, 0);
  parts->current--;
  set_dynamic(&parts->parts, (gptr) &part, parts->current);
}


/*
  Increase number of writers for this buffer

  SYNOPSIS
    translog_buffer_increase_writers()
    buffer               target buffer
*/

static inline void
translog_buffer_increase_writers(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_increase_writers");
  buffer->copy_to_buffer_in_progress++;
  DBUG_PRINT("info", ("copy_to_buffer_in_progress. Buffer #%u 0x%lx: %d",
                      (uint) buffer->buffer_no, (ulong) buffer,
                      buffer->copy_to_buffer_in_progress));
  DBUG_VOID_RETURN;
}


/*
  Decrease number of writers for this buffer

  SYNOPSIS
    translog_buffer_decrease_writers()
    buffer               target buffer
*/


static void translog_buffer_decrease_writers(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_decrease_writers");
  buffer->copy_to_buffer_in_progress--;
  DBUG_PRINT("info", ("copy_to_buffer_in_progress. Buffer #%u 0x%lx: %d",
                      (uint) buffer->buffer_no, (ulong) buffer,
                      buffer->copy_to_buffer_in_progress));
  if (buffer->copy_to_buffer_in_progress == 0 &&
      buffer->waiting_filling_buffer.last_thread != NULL)
    wqueue_release_queue(&buffer->waiting_filling_buffer);
  DBUG_VOID_RETURN;
}


/*
  Put chunk 2 from new page beginning

  SYNOPSIS
    translog_write_variable_record_chunk2_page()
    parts                Descriptor of record source parts
    horizon              \ Pointers on file position and buffer
    cursor               /

  RETURN
    0  OK
    1  Error
*/

static my_bool
translog_write_variable_record_chunk2_page(struct st_translog_parts *parts,
                                           TRANSLOG_ADDRESS *horizon,
                                           struct st_buffer_cursor *cursor)
{
  struct st_translog_buffer *buffer_to_flush;
  int rc;
  byte chunk2_header[1];
  DBUG_ENTER("translog_write_variable_record_chunk2_page");
  chunk2_header[0]= TRANSLOG_CHUNK_NOHDR;

  rc= translog_page_next(horizon, cursor, &buffer_to_flush);
  if (buffer_to_flush != NULL)
  {
    rc|= translog_buffer_lock(buffer_to_flush);
    translog_buffer_decrease_writers(buffer_to_flush);
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    rc|= translog_buffer_unlock(buffer_to_flush);
  }
  if (rc)
    DBUG_RETURN(1);

  /* Puts chunk type */
  translog_write_data_on_page(horizon, cursor, 1, chunk2_header);
  /* Puts chunk body */
  translog_write_parts_on_page(horizon, cursor,
                               log_descriptor.page_capacity_chunk_2, parts);
  DBUG_RETURN(0);
}


/*
  Put chunk 3 of requested length in the buffer from new page beginning

  SYNOPSIS
    translog_write_variable_record_chunk3_page()
    parts                Descriptor of record source parts
    length               Length of this chunk
    horizon              \ Pointers on file position and buffer
    cursor               /

  RETURN
    0  OK
    1  Error
*/

static my_bool
translog_write_variable_record_chunk3_page(struct st_translog_parts *parts,
                                           uint16 length,
                                           TRANSLOG_ADDRESS *horizon,
                                           struct st_buffer_cursor *cursor)
{
  struct st_translog_buffer *buffer_to_flush;
  struct st_translog_part part;
  int rc;
  byte chunk3_header[1 + 2];
  DBUG_ENTER("translog_write_variable_record_chunk3_page");

  rc= translog_page_next(horizon, cursor, &buffer_to_flush);
  if (buffer_to_flush != NULL)
  {
    rc|= translog_buffer_lock(buffer_to_flush);
    translog_buffer_decrease_writers(buffer_to_flush);
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    rc|= translog_buffer_unlock(buffer_to_flush);
  }
  if (rc)
    DBUG_RETURN(1);
  if (length == 0)
  {
    /* It was call to write page header only (no data for chunk 3) */
    DBUG_PRINT("info", ("It is a call to make page header only"));
    DBUG_RETURN(0);
  }

  DBUG_ASSERT(parts->current != 0);       /* first part is left for header */
  parts->total_record_length+= (part.len= 1 + 2);
  part.buff= chunk3_header;
  /* Puts chunk type */
  *chunk3_header= (byte) (TRANSLOG_CHUNK_LNGTH);
  /* Puts chunk length */
  int2store(chunk3_header + 1, length);
  parts->current--;
  set_dynamic(&parts->parts, (gptr) &part, parts->current);

  translog_write_parts_on_page(horizon, cursor, length + 1 + 2, parts);
  DBUG_RETURN(0);
}

/*
  Move log pointer (horizon) on given number pages starting from next page,
  and given offset on the last page

  SYNOPSIS
    translog_advance_pointer()
    pages                Number of full pages starting from the next one
    last_page_data       Plus this data on the last page

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_advance_pointer(uint pages, uint16 last_page_data)
{
  translog_size_t last_page_offset= (log_descriptor.page_overhead +
                                     last_page_data);
  translog_size_t offset= (TRANSLOG_PAGE_SIZE -
                           log_descriptor.bc.current_page_fill +
                           pages * TRANSLOG_PAGE_SIZE + last_page_offset);
  translog_size_t buffer_end_offset, file_end_offset, min_offset;
  DBUG_ENTER("translog_advance_pointer");
  DBUG_PRINT("enter", ("Pointer:  (%lu, 0x%lx) + %u + %u pages + %u + %u",
                       (ulong) LSN_FILE_NO(log_descriptor.horizon),
                       (ulong) LSN_OFFSET(log_descriptor.horizon),
                       (uint) (TRANSLOG_PAGE_SIZE -
                               log_descriptor.bc.current_page_fill),
                       pages, (uint) log_descriptor.page_overhead,
                       (uint) last_page_data));

  for (;;)
  {
    uint8 new_buffer_no;
    struct st_translog_buffer *new_buffer;
    struct st_translog_buffer *old_buffer;
    buffer_end_offset= TRANSLOG_WRITE_BUFFER - log_descriptor.bc.buffer->size;
    file_end_offset= (log_descriptor.log_file_max_size -
                      LSN_OFFSET(log_descriptor.horizon));
    DBUG_PRINT("info", ("offset: %lu  buffer_end_offs: %lu, "
                        "file_end_offs:  %lu",
                        (ulong) offset, (ulong) buffer_end_offset,
                        (ulong) file_end_offset));
    DBUG_PRINT("info", ("Buff #%u %u (0x%lx) offset 0x%lx + size 0x%lx = "
                        "0x%lx (0x%lx)",
                        (uint) log_descriptor.bc.buffer->buffer_no,
                        (uint) log_descriptor.bc.buffer_no,
                        (ulong) log_descriptor.bc.buffer,
                        (ulong) LSN_OFFSET(log_descriptor.bc.buffer->offset),
                        (ulong) log_descriptor.bc.buffer->size,
                        (ulong) (LSN_OFFSET(log_descriptor.bc.buffer->offset) +
                                 log_descriptor.bc.buffer->size),
                        (ulong) LSN_OFFSET(log_descriptor.horizon)));
    DBUG_ASSERT(LSN_OFFSET(log_descriptor.bc.buffer->offset) +
                log_descriptor.bc.buffer->size ==
                LSN_OFFSET(log_descriptor.horizon));

    if (offset <= buffer_end_offset && offset <= file_end_offset)
      break;
    old_buffer= log_descriptor.bc.buffer;
    new_buffer_no= (log_descriptor.bc.buffer_no + 1) % TRANSLOG_BUFFERS_NO;
    new_buffer= log_descriptor.buffers + new_buffer_no;

    translog_buffer_lock(new_buffer);
    translog_wait_for_buffer_free(new_buffer);

    min_offset= min(buffer_end_offset, file_end_offset);
    /*
      TODO: check is it ptr or size enough
    */
    log_descriptor.bc.buffer->size+= min_offset;
    log_descriptor.bc.ptr+= min_offset;
    DBUG_PRINT("info", ("NewP buffer #%u: 0x%lx  chaser: %d  Size: %lu (%lu)",
                        (uint) log_descriptor.bc.buffer->buffer_no,
                        (ulong) log_descriptor.bc.buffer,
                        log_descriptor.bc.chaser,
                        (ulong) log_descriptor.bc.buffer->size,
                        (ulong) (log_descriptor.bc.ptr -log_descriptor.bc.
                                 buffer->buffer)));
    DBUG_ASSERT((ulong) (log_descriptor.bc.ptr -
                         log_descriptor.bc.buffer->buffer) ==
                log_descriptor.bc.buffer->size);
    DBUG_ASSERT(log_descriptor.bc.buffer->buffer_no ==
                log_descriptor.bc.buffer_no);
    translog_buffer_increase_writers(log_descriptor.bc.buffer);

    if (file_end_offset <= buffer_end_offset)
    {
      log_descriptor.horizon+= LSN_ONE_FILE;
      log_descriptor.horizon= LSN_REPLACE_OFFSET(log_descriptor.horizon,
                                                 TRANSLOG_PAGE_SIZE);
      DBUG_PRINT("info", ("New file: %lu",
                          (ulong) LSN_FILE_NO(log_descriptor.horizon)));
      if (translog_create_new_file())
      {
        DBUG_RETURN(1);
      }
    }
    else
    {
      DBUG_PRINT("info", ("The same file"));
      log_descriptor.horizon+= min_offset; /* offset increasing */
    }
    translog_start_buffer(new_buffer, &log_descriptor.bc, new_buffer_no);
    if (translog_buffer_unlock(old_buffer))
      DBUG_RETURN(1);
    offset-= min_offset;
  }
  log_descriptor.bc.ptr+= offset;
  log_descriptor.bc.buffer->size+= offset;
  translog_buffer_increase_writers(log_descriptor.bc.buffer);
  log_descriptor.horizon+= offset; /* offset increasing */
  log_descriptor.bc.current_page_fill= last_page_offset;
  DBUG_PRINT("info", ("drop write_counter"));
  log_descriptor.bc.write_counter= 0;
  log_descriptor.bc.previous_offset= 0;
  DBUG_PRINT("info", ("NewP buffer #%u: 0x%lx  chaser: %d  Size: %lu (%lu)  "
                      "offset: %u  last page: %u",
                      (uint) log_descriptor.bc.buffer->buffer_no,
                      (ulong) log_descriptor.bc.buffer,
                      log_descriptor.bc.chaser,
                      (ulong) log_descriptor.bc.buffer->size,
                      (ulong) (log_descriptor.bc.ptr -
                               log_descriptor.bc.buffer->
                               buffer), (uint) offset,
                      (uint) last_page_offset));
  DBUG_PRINT("info",
             ("pointer moved to: (%lu, 0x%lx)",
              (ulong) LSN_FILE_NO(log_descriptor.horizon),
              (ulong) LSN_OFFSET(log_descriptor.horizon)));
  DBUG_ASSERT(log_descriptor.bc.chaser ||
              ((ulong) (log_descriptor.bc.ptr -
                        log_descriptor.bc.buffer->buffer)
               == log_descriptor.bc.buffer->size));
  DBUG_ASSERT(log_descriptor.bc.buffer->buffer_no ==
              log_descriptor.bc.buffer_no);
  DBUG_ASSERT((log_descriptor.bc.ptr - log_descriptor.bc.buffer->
               buffer) %TRANSLOG_PAGE_SIZE ==
              log_descriptor.bc.current_page_fill % TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(log_descriptor.bc.current_page_fill <= TRANSLOG_PAGE_SIZE);
  log_descriptor.bc.protected= 0;
  DBUG_RETURN(0);
}



/*
  Get page rest

  SYNOPSIS
    translog_get_current_page_rest()

  NOTE loghandler should be locked

  RETURN
    number of bytes left on the current page
*/

#define translog_get_current_page_rest() \
  (TRANSLOG_PAGE_SIZE - log_descriptor.bc.current_page_fill)

/*
  Get buffer rest in full pages

  SYNOPSIS
     translog_get_current_buffer_rest()

  NOTE loghandler should be locked

  RETURN
    number of full pages left on the current buffer
*/

#define translog_get_current_buffer_rest() \
  ((log_descriptor.bc.buffer->buffer + TRANSLOG_WRITE_BUFFER - \
    log_descriptor.bc.ptr) / \
   TRANSLOG_PAGE_SIZE)

/*
  Calculate possible group size without first (current) page

  SYNOPSIS
    translog_get_current_group_size()

  NOTE loghandler should be locked

  RETURN
    group size without first (current) page
*/

static translog_size_t translog_get_current_group_size()
{
  /* buffer rest in full pages */
  translog_size_t buffer_rest= translog_get_current_buffer_rest();
  DBUG_ENTER("translog_get_current_group_size");
  DBUG_PRINT("info", ("buffer_rest in pages: %u", buffer_rest));

  buffer_rest*= log_descriptor.page_capacity_chunk_2;
  /* in case of only half of buffer free we can write this and next buffer */
  if (buffer_rest < log_descriptor.half_buffer_capacity_chunk_2)
  {
    DBUG_PRINT("info", ("buffer_rest: %lu -> add %lu",
                        (ulong) buffer_rest,
                        (ulong) log_descriptor.buffer_capacity_chunk_2));
    buffer_rest+= log_descriptor.buffer_capacity_chunk_2;
  }

  DBUG_PRINT("info", ("buffer_rest: %lu", (ulong) buffer_rest));

  DBUG_RETURN(buffer_rest);
}


/*
  Write variable record in 1 group

  SYNOPSIS
    translog_write_variable_record_1group()
    lsn                  LSN of the record will be written here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    parts                Descriptor of record source parts
    buffer_to_flush      Buffer which have to be flushed if it is not 0
    header_length        Calculated header length of chunk type 0
    tcb                  Transaction control block pointer for hooks by
                         record log type

  RETURN
    0  OK
    1  Error
*/

static my_bool
translog_write_variable_record_1group(LSN *lsn,
                                      enum translog_record_type type,
                                      SHORT_TRANSACTION_ID short_trid,
                                      struct st_translog_parts *parts,
                                      struct st_translog_buffer
                                      *buffer_to_flush, uint16 header_length,
                                      void *tcb)
{
  TRANSLOG_ADDRESS horizon;
  struct st_buffer_cursor cursor;
  int rc= 0;
  uint i;
  translog_size_t record_rest, full_pages, first_page;
  uint additional_chunk3_page= 0;
  byte chunk0_header[1 + 2 + 5 + 2];
  DBUG_ENTER("translog_write_variable_record_1group");

  *lsn= horizon= log_descriptor.horizon;
  if (log_record_type_descriptor[type].inwrite_hook &&
      (*log_record_type_descriptor[type].inwrite_hook)(type, tcb, lsn, parts))
  {
    translog_unlock();
    DBUG_RETURN(1);
  }
  cursor= log_descriptor.bc;
  cursor.chaser= 1;

  /* Advance pointer To be able unlock the loghandler */
  first_page= translog_get_current_page_rest();
  record_rest= parts->record_length - (first_page - header_length);
  full_pages= record_rest / log_descriptor.page_capacity_chunk_2;
  record_rest= (record_rest % log_descriptor.page_capacity_chunk_2);

  if (record_rest + 1 == log_descriptor.page_capacity_chunk_2)
  {
    DBUG_PRINT("info", ("2 chunks type 3 is needed"));
    /* We will write 2 chunks type 3 at the end of this group */
    additional_chunk3_page= 1;
    record_rest= 1;
  }

  DBUG_PRINT("info", ("first_page: %u (%u)  full_pages: %u (%lu)  "
                      "additional: %u (%u)  rest %u = %u",
                      first_page, first_page - header_length,
                      full_pages,
                      (ulong) full_pages *
                      log_descriptor.page_capacity_chunk_2,
                      additional_chunk3_page,
                      additional_chunk3_page *
                      (log_descriptor.page_capacity_chunk_2 - 1),
                      record_rest, parts->record_length));
  /* record_rest + 3 is chunk type 3 overhead + record_rest */
  rc|= translog_advance_pointer(full_pages + additional_chunk3_page,
                                (record_rest ? record_rest + 3 : 0));
  log_descriptor.bc.buffer->last_lsn= *lsn;

  rc|= translog_unlock();

  /*
     Check if we switched buffer and need process it (current buffer is
     unlocked already => we will not delay other threads
  */
  if (buffer_to_flush != NULL)
  {
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    rc|= translog_buffer_unlock(buffer_to_flush);
  }
  if (rc)
    DBUG_RETURN(1);

  translog_write_variable_record_1group_header(parts, type, short_trid,
                                               header_length, chunk0_header);

  /* fill the pages */
  translog_write_parts_on_page(&horizon, &cursor, first_page, parts);


  DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                      (ulong) LSN_FILE_NO(log_descriptor.horizon),
                      (ulong) LSN_OFFSET(log_descriptor.horizon),
                      (ulong) LSN_FILE_NO(horizon),
                      (ulong) LSN_OFFSET(horizon)));

  for (i= 0; i < full_pages; i++)
  {
    if (translog_write_variable_record_chunk2_page(parts, &horizon, &cursor))
      DBUG_RETURN(1);

    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon)));
  }

  if (additional_chunk3_page)
  {
    if (translog_write_variable_record_chunk3_page(parts,
                                                   log_descriptor.
                                                   page_capacity_chunk_2 - 2,
                                                   &horizon, &cursor))
      DBUG_RETURN(1);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon)));
    DBUG_ASSERT(cursor.current_page_fill == TRANSLOG_PAGE_SIZE);
  }

  if (translog_write_variable_record_chunk3_page(parts,
                                                 record_rest,
                                                 &horizon, &cursor))
    DBUG_RETURN(1);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon)));

  if (!(rc= translog_buffer_lock(cursor.buffer)))
  {
    /*
       Check if we wrote something on 1:st not full page and need to reconstruct
       CRC and sector protection
    */
    translog_buffer_decrease_writers(cursor.buffer);
  }
  rc|= translog_buffer_unlock(cursor.buffer);
  DBUG_RETURN(rc);
}


/*
  Write variable record in 1 chunk

  SYNOPSIS
    translog_write_variable_record_1chunk()
    lsn                  LSN of the record will be written here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    parts                Descriptor of record source parts
    buffer_to_flush      Buffer which have to be flushed if it is not 0
    header_length        Calculated header length of chunk type 0
    tcb                  Transaction control block pointer for hooks by
                         record log type

  RETURN
    0  OK
    1  Error
*/

static my_bool
translog_write_variable_record_1chunk(LSN *lsn,
                                      enum translog_record_type type,
                                      SHORT_TRANSACTION_ID short_trid,
                                      struct st_translog_parts *parts,
                                      struct st_translog_buffer
                                      *buffer_to_flush, uint16 header_length,
                                      void *tcb)
{
  int rc;
  byte chunk0_header[1 + 2 + 5 + 2];
  DBUG_ENTER("translog_write_variable_record_1chunk");

  translog_write_variable_record_1group_header(parts, type, short_trid,
                                               header_length, chunk0_header);

  *lsn= log_descriptor.horizon;
  if (log_record_type_descriptor[type].inwrite_hook &&
      (*log_record_type_descriptor[type].inwrite_hook)(type, tcb,
                                                       lsn, parts))
  {
    translog_unlock();
    DBUG_RETURN(1);
  }

  rc= translog_write_parts_on_page(&log_descriptor.horizon,
                                   &log_descriptor.bc,
                                   parts->total_record_length, parts);
  log_descriptor.bc.buffer->last_lsn= *lsn;
  rc|= translog_unlock();

  /*
     check if we switched buffer and need process it (current buffer is
     unlocked already => we will not delay other threads
  */
  if (buffer_to_flush != NULL)
  {
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    rc|= translog_buffer_unlock(buffer_to_flush);
  }

  DBUG_RETURN(rc);
}


/*
  Calculate and write LSN difference (compressed LSN)

  SYNOPSIS
    translog_put_LSN_diff()
    base_lsn             LSN from which we calculate difference
    lsn                  LSN for codding
    dst                  Result will be written to dst[-pack_length] .. dst[-1]

  NOTE:
    To store an LSN in a compact way we will use the following compression:

    If a log record has LSN1, and it contains the lSN2 as a back reference,
    Instead of LSN2 we write LSN1-LSN2, encoded as:

     two bits     the number N (see below)
     14 bits
     N bytes

     That is, LSN is encoded in 2..5 bytes, and the number of bytes minus 2
     is stored in the first two bits.

  RETURN
    #     pointer on coded LSN
    NULL  Error
*/

static byte *translog_put_LSN_diff(LSN base_lsn, LSN lsn, byte *dst)
{
  DBUG_ENTER("translog_put_LSN_diff");
  DBUG_PRINT("enter", ("Base: (0x%lu,0x%lx)  val: (0x%lu,0x%lx)  dst: 0x%lx",
                       (ulong) LSN_FILE_NO(base_lsn),
                       (ulong) LSN_OFFSET(base_lsn),
                       (ulong) LSN_FILE_NO(lsn),
                       (ulong) LSN_OFFSET(lsn), (ulong) dst));
  if (LSN_FILE_NO(base_lsn) == LSN_FILE_NO(lsn))
  {
    uint32 diff;
    DBUG_ASSERT(base_lsn > lsn);
    diff= base_lsn - lsn;
    DBUG_PRINT("info", ("File is the same. Diff: 0x%lx", (ulong) diff));
    if (diff <= 0x3FFF)
    {
      dst-= 2;
      /*
        Note we store this high byte first to ensure that first byte has
        0 in the 3 upper bits.
      */
      dst[0]= diff >> 8;
      dst[1]= (diff & 0xFF);
    }
    else if (diff <= 0x3FFFFF)
    {
      dst-= 3;
      dst[0]= 0x40 | (diff >> 16);
      int2store(dst + 1, diff & 0xFFFF);
    }
    else if (diff <= 0x3FFFFFFF)
    {
      dst-= 4;
      dst[0]= 0x80 | (diff >> 24);
      int3store(dst + 1, diff & 0xFFFFFF);
    }
    else
    {
      dst-= 5;
      dst[0]= 0xC0;
      int4store(dst + 1, diff);
    }
  }
  else
  {
    uint32 diff;
    uint32 offset_diff;
    ulonglong base_offset= LSN_OFFSET(base_lsn);
    DBUG_ASSERT(base_lsn > lsn);
    diff= LSN_FILE_NO(base_lsn) - LSN_FILE_NO(lsn);
    DBUG_PRINT("info", ("File is different. Diff: 0x%lx", (ulong) diff));

    if (base_offset < LSN_OFFSET(lsn))
    {
      /* take 1 from file offset */
      diff--;
      base_offset+= LL(0x100000000);
    }
    offset_diff= base_offset - LSN_OFFSET(lsn);
    if (diff > 0x3f)
    {
      /*TODO: error - too long transaction - panic!!! */
      UNRECOVERABLE_ERROR(("Too big file diff: %lu", (ulong) diff));
      DBUG_RETURN(NULL);
    }
    dst-= 5;
    *dst= (0xC0 | diff);
    int4store(dst + 1, offset_diff);
  }
  DBUG_PRINT("info", ("new dst: 0x%lx", (ulong) dst));
  DBUG_RETURN(dst);
}


/*
  Get LSN from LSN-difference (compressed LSN)

  SYNOPSIS
    translog_get_LSN_from_diff()
    base_lsn             LSN from which we calculate difference
    src                  pointer to coded lsn
    dst                  pointer to buffer where to write 7byte LSN

  NOTE:
    To store an LSN in a compact way we will use the following compression:

    If a log record has LSN1, and it contains the lSN2 as a back reference,
    Instead of LSN2 we write LSN1-LSN2, encoded as:

     two bits     the number N (see below)
     14 bits
     N bytes

    That is, LSN is encoded in 2..5 bytes, and the number of bytes minus 2
    is stored in the first two bits.

  RETURN
    pointer to buffer after decoded LSN
*/

static byte *translog_get_LSN_from_diff(LSN base_lsn, byte *src, byte *dst)
{
  LSN lsn;
  uint32 diff;
  uint32 first_byte;
  uint32 file_no, rec_offset;
  uint8 code;
  DBUG_ENTER("translog_get_LSN_from_diff");
  DBUG_PRINT("enter", ("Base: (0x%lx,0x%lx)  src: 0x%lx  dst 0x%lx",
                       (ulong) LSN_FILE_NO(base_lsn),
                       (ulong) LSN_OFFSET(base_lsn),
                       (ulong) src, (ulong) dst));
  first_byte= *((uint8*) src);
  code= first_byte >> 6;                        /* Length in 2 upmost bits */
  first_byte&= 0x3F;
  src++;                                        /* Skip length + encode */
  file_no= LSN_FILE_NO(base_lsn);               /* Assume relative */
  DBUG_PRINT("info", ("code: %u  first byte: %lu",
                      (uint) code, (ulong) first_byte));
  switch (code) {
  case 0:
    rec_offset= LSN_OFFSET(base_lsn) - ((first_byte << 8) + *((uint8*)src));
    break;
  case 1:
    diff= uint2korr(src);
    rec_offset= LSN_OFFSET(base_lsn) - ((first_byte << 16) + diff);
    break;
  case 2:
    diff= uint3korr(src);
    rec_offset= LSN_OFFSET(base_lsn) - ((first_byte << 24) + diff);
    break;
  case 3:
  {
    ulonglong base_offset= LSN_OFFSET(base_lsn);
    diff= uint4korr(src);
    if (diff > LSN_OFFSET(base_lsn))
    {
      /* take 1 from file offset */
      first_byte++;
      base_offset+= LL(0x100000000);
    }
    file_no= LSN_FILE_NO(base_lsn) - first_byte;
    rec_offset= base_offset - diff;
    break;
  }
  default:
    DBUG_ASSERT(0);
    DBUG_RETURN(NULL);
  }
  lsn= MAKE_LSN(file_no, rec_offset);
  src+= code + 1;
  lsn_store(dst, lsn);
  DBUG_PRINT("info", ("new src: 0x%lx", (ulong) dst));
  DBUG_RETURN(src);
}


/*
  Encode relative LSNs listed in the parameters

  SYNOPSIS
    translog_relative_LSN_encode()
    parts                Parts list with encoded LSN(s)
    base_lsn             LSN which is base for encoding
    lsns                 number of LSN(s) to encode
    compressed_LSNs      buffer which can be used for storing compressed LSN(s)

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_relative_LSN_encode(struct st_translog_parts *parts,
                                            LSN base_lsn,
                                            uint lsns, byte *compressed_LSNs)
{
  struct st_translog_part *part;
  uint lsns_len= lsns * LSN_STORE_SIZE;

  DBUG_ENTER("translog_relative_LSN_encode");

  part= dynamic_element(&parts->parts, parts->current,
                        struct st_translog_part *);
  /* collect all LSN(s) in one chunk if it (they) is (are) divided */
  if (part->len < lsns_len)
  {
    uint copied= part->len;
    DBUG_PRINT("info", ("Using buffer: 0x%lx", (ulong) compressed_LSNs));
    memcpy(compressed_LSNs, part->buff, part->len);
    do
    {
      struct st_translog_part *next_part;
      next_part= dynamic_element(&parts->parts, parts->current + 1,
                                 struct st_translog_part *);
      if ((next_part->len + copied) < lsns_len)
      {
        memcpy(compressed_LSNs + copied, next_part->buff, next_part->len);
        copied+= next_part->len;
        delete_dynamic_element(&parts->parts, parts->current + 1);
      }
      else
      {
        uint len= lsns_len - copied;
        memcpy(compressed_LSNs + copied, next_part->buff, len);
        copied= lsns_len;
        next_part->buff+= len;
        next_part->len-= len;
      }
    } while (copied < lsns_len);
    part->len= lsns_len;
    part->buff= compressed_LSNs;
  }
  {
    /* Compress */
    LSN ref;
    uint economy;
    byte *ref_ptr= part->buff + lsns_len - LSN_STORE_SIZE;
    byte *dst_ptr= part->buff + lsns_len;
    for (; ref_ptr >= part->buff ; ref_ptr-= LSN_STORE_SIZE)
    {
      ref= lsn_korr(ref_ptr);
      if ((dst_ptr= translog_put_LSN_diff(base_lsn, ref, dst_ptr)) == NULL)
        DBUG_RETURN(1);
    }
    /* Note that dst_ptr did grow downward ! */
    economy= (uint) (dst_ptr - part->buff);
    DBUG_PRINT("info", ("Economy: %u", economy));
    part->len-= economy;
    parts->record_length-= economy;
    parts->total_record_length-= economy;
    part->buff= dst_ptr;
  }
  DBUG_RETURN(0);
}


/*
  Write multi-group variable-size record

  SYNOPSIS
    translog_write_variable_record_mgroup()
    lsn                  LSN of the record will be written here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    parts                Descriptor of record source parts
    buffer_to_flush      Buffer which have to be flushed if it is not 0
    header_length        Header length calculated for 1 group
    buffer_rest          Beginning from which we plan to write in full pages
    tcb                  Transaction control block pointer for hooks by
                         record log type

  RETURN
    0  OK
    1  Error
*/

static my_bool
translog_write_variable_record_mgroup(LSN *lsn,
                                      enum translog_record_type type,
                                      SHORT_TRANSACTION_ID short_trid,
                                      struct st_translog_parts *parts,
                                      struct st_translog_buffer
                                      *buffer_to_flush,
                                      uint16 header_length,
                                      translog_size_t buffer_rest,
                                      void *tcb)
{
  TRANSLOG_ADDRESS horizon;
  struct st_buffer_cursor cursor;
  int rc= 0;
  uint i, chunk2_page, full_pages;
  uint curr_group= 0;
  translog_size_t record_rest, first_page, chunk3_pages, chunk0_pages= 1;
  translog_size_t done= 0;
  struct st_translog_group_descriptor group;
  DYNAMIC_ARRAY groups;
  uint16 chunk3_size;
  uint16 page_capacity= log_descriptor.page_capacity_chunk_2 + 1;
  uint16 last_page_capacity;
  my_bool new_page_before_chunk0= 1, first_chunk0= 1;
  byte chunk0_header[1 + 2 + 5 + 2 + 2], group_desc[7 + 1];
  byte chunk2_header[1];
  uint header_fixed_part= header_length + 2;
  uint groups_per_page= (page_capacity - header_fixed_part) / (7 + 1);
  DBUG_ENTER("translog_write_variable_record_mgroup");

  chunk2_header[0]= TRANSLOG_CHUNK_NOHDR;

  if (init_dynamic_array(&groups, sizeof(struct st_translog_group_descriptor),
                         10, 10 CALLER_INFO))
  {
    translog_unlock();
    UNRECOVERABLE_ERROR(("init array failed"));
    DBUG_RETURN(1);
  }

  first_page= translog_get_current_page_rest();
  record_rest= parts->record_length - (first_page - 1);
  DBUG_PRINT("info", ("Record Rest: %lu", (ulong) record_rest));

  if (record_rest < buffer_rest)
  {
    DBUG_PRINT("info", ("too many free space because changing header"));
    buffer_rest-= log_descriptor.page_capacity_chunk_2;
    DBUG_ASSERT(record_rest >= buffer_rest);
  }

  do
  {
    group.addr= horizon= log_descriptor.horizon;
    cursor= log_descriptor.bc;
    cursor.chaser= 1;
    if ((full_pages= buffer_rest / log_descriptor.page_capacity_chunk_2) > 255)
    {
      /* sizeof(uint8) == 256 is max number of chunk in multi-chunks group */
      full_pages= 255;
      buffer_rest= full_pages * log_descriptor.page_capacity_chunk_2;
    }
    /*
       group chunks =
       full pages + first page (which actually can be full, too).
       But here we assign number of chunks - 1
    */
    group.num= full_pages;
    if (insert_dynamic(&groups, (gptr) &group))
    {
      UNRECOVERABLE_ERROR(("insert into array failed"));
      goto err_unlock;
    }

    DBUG_PRINT("info", ("chunk: #%u  first_page: %u (%u)  "
                        "full_pages: %lu (%lu)  "
                        "Left %lu",
                        groups.elements,
                        first_page, first_page - 1,
                        (ulong) full_pages,
                        (ulong) (full_pages *
                                 log_descriptor.page_capacity_chunk_2),
                        (ulong)(parts->record_length - (first_page - 1 +
                                                        buffer_rest) -
                                done)));
    rc|= translog_advance_pointer(full_pages, 0);

    rc|= translog_unlock();

    if (buffer_to_flush != NULL)
    {
      rc|= translog_buffer_lock(buffer_to_flush);
      translog_buffer_decrease_writers(buffer_to_flush);
      if (!rc)
        rc= translog_buffer_flush(buffer_to_flush);
      rc|= translog_buffer_unlock(buffer_to_flush);
      buffer_to_flush= NULL;
    }
    if (rc)
    {
      UNRECOVERABLE_ERROR(("flush of unlock buffer failed"));
      goto err;
    }

    translog_write_data_on_page(&horizon, &cursor, 1, chunk2_header);
    translog_write_parts_on_page(&horizon, &cursor, first_page - 1, parts);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)  "
                        "Left  %lu",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon),
                        (ulong) (parts->record_length - (first_page - 1) -
                                 done)));

    for (i= 0; i < full_pages; i++)
    {
      if (translog_write_variable_record_chunk2_page(parts, &horizon, &cursor))
        goto err;

      DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  "
                          "local: (%lu,0x%lx)  "
                          "Left: %lu",
                          (ulong) LSN_FILE_NO(log_descriptor.horizon),
                          (ulong) LSN_OFFSET(log_descriptor.horizon),
                          (ulong) LSN_FILE_NO(horizon),
                          (ulong) LSN_OFFSET(horizon),
                          (ulong) (parts->record_length - (first_page - 1) -
                                   i * log_descriptor.page_capacity_chunk_2 -
                                   done)));
    }

    done+= (first_page - 1 + buffer_rest);

    /* TODO: make separate function for following */
    rc= translog_page_next(&horizon, &cursor, &buffer_to_flush);
    if (buffer_to_flush != NULL)
    {
      rc|= translog_buffer_lock(buffer_to_flush);
      translog_buffer_decrease_writers(buffer_to_flush);
      if (!rc)
        rc= translog_buffer_flush(buffer_to_flush);
      rc|= translog_buffer_unlock(buffer_to_flush);
      buffer_to_flush= NULL;
    }
    if (rc)
    {
      UNRECOVERABLE_ERROR(("flush of unlock buffer failed"));
      goto err;
    }
    rc= translog_buffer_lock(cursor.buffer);
    if (!rc)
      translog_buffer_decrease_writers(cursor.buffer);
    rc|= translog_buffer_unlock(cursor.buffer);
    if (rc)
      goto err;

    translog_lock();

    first_page= translog_get_current_page_rest();
    buffer_rest= translog_get_current_group_size();
  } while (first_page + buffer_rest < (uint) (parts->record_length - done));

  group.addr= horizon= log_descriptor.horizon;
  cursor= log_descriptor.bc;
  cursor.chaser= 1;
  group.num= 0;                       /* 0 because it does not matter */
  if (insert_dynamic(&groups, (gptr) &group))
  {
    UNRECOVERABLE_ERROR(("insert into array failed"));
    goto err_unlock;
  }
  record_rest= parts->record_length - done;
  DBUG_PRINT("info", ("Record rest: %lu", (ulong) record_rest));
  if (first_page <= record_rest + 1)
  {
    chunk2_page= 1;
    record_rest-= (first_page - 1);
    full_pages= record_rest / log_descriptor.page_capacity_chunk_2;
    record_rest= (record_rest % log_descriptor.page_capacity_chunk_2);
    last_page_capacity= page_capacity;
  }
  else
  {
    chunk2_page= full_pages= 0;
    last_page_capacity= first_page;
  }
  chunk3_size= 0;
  chunk3_pages= 0;
  if (last_page_capacity > record_rest + 1 && record_rest != 0)
  {
    if (last_page_capacity >
        record_rest + header_fixed_part + groups.elements * (7 + 1))
    {
      /* 1 record of type 0 */
      chunk3_pages= 0;
    }
    else
    {
      chunk3_pages= 1;
      if (record_rest + 2 == last_page_capacity)
      {
        chunk3_size= record_rest - 1;
        record_rest= 1;
      }
      else
      {
        chunk3_size= record_rest;
        record_rest= 0;
      }
    }
  }
  /*
     A first non-full page will hold type 0 chunk only if it fit in it with
     all its headers
  */
  while (page_capacity <
         record_rest + header_fixed_part +
         (groups.elements - groups_per_page * (chunk0_pages - 1)) * (7 + 1))
    chunk0_pages++;
  DBUG_PRINT("info", ("chunk0_pages: %u  groups %u  groups per full page: %u  "
                      "Group on last page: %u",
                      chunk0_pages, groups.elements,
                      groups_per_page,
                      (groups.elements -
                       ((page_capacity - header_fixed_part) / (7 + 1)) *
                       (chunk0_pages - 1))));
  DBUG_PRINT("info", ("first_page: %u  chunk2: %u  full_pages: %u (%lu)  "
                      "chunk3: %u (%u)  rest: %u",
                      first_page,
                      chunk2_page, full_pages,
                      (ulong) full_pages *
                      log_descriptor.page_capacity_chunk_2,
                      chunk3_pages, (uint) chunk3_size, (uint) record_rest));
  rc= translog_advance_pointer(full_pages + chunk3_pages +
                               (chunk0_pages - 1),
                               record_rest + header_fixed_part +
                               (groups.elements -
                                ((page_capacity -
                                  header_fixed_part) / (7 + 1)) *
                                (chunk0_pages - 1)) * (7 + 1));
  rc|= translog_unlock();
  if (rc)
    goto err;

  if (chunk2_page)
  {
    DBUG_PRINT("info", ("chunk 2 to finish first page"));
    translog_write_data_on_page(&horizon, &cursor, 1, chunk2_header);
    translog_write_parts_on_page(&horizon, &cursor, first_page - 1, parts);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx) "
                        "Left: %lu",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon),
                        (ulong) (parts->record_length - (first_page - 1) -
                                 done)));
  }
  else if (chunk3_pages)
  {
    DBUG_PRINT("info", ("chunk 3"));
    DBUG_ASSERT(full_pages == 0);
    byte chunk3_header[3];
    chunk3_pages= 0;
    chunk3_header[0]= TRANSLOG_CHUNK_LNGTH;
    int2store(chunk3_header + 1, chunk3_size);
    translog_write_data_on_page(&horizon, &cursor, 3, chunk3_header);
    translog_write_parts_on_page(&horizon, &cursor, chunk3_size, parts);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx) "
                        "Left: %lu",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon),
                        (ulong) (parts->record_length - chunk3_size - done)));
  }
  else
  {
    DBUG_PRINT("info", ("no new_page_before_chunk0"));
    new_page_before_chunk0= 0;
  }

  for (i= 0; i < full_pages; i++)
  {
    DBUG_ASSERT(chunk2_page != 0);
    if (translog_write_variable_record_chunk2_page(parts, &horizon, &cursor))
      goto err;

    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx) "
                        "Left: %lu",
                        (ulong) LSN_FILE_NO(log_descriptor.horizon),
                        (ulong) LSN_OFFSET(log_descriptor.horizon),
                        (ulong) LSN_FILE_NO(horizon),
                        (ulong) LSN_OFFSET(horizon),
                        (ulong) (parts->record_length - (first_page - 1) -
                                 i * log_descriptor.page_capacity_chunk_2 -
                                 done)));
  }

  if (chunk3_pages &&
      translog_write_variable_record_chunk3_page(parts,
                                                 chunk3_size,
                                                 &horizon, &cursor))
    goto err;
  DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                      (ulong) LSN_FILE_NO(log_descriptor.horizon),
                      (ulong) LSN_OFFSET(log_descriptor.horizon),
                      (ulong) LSN_FILE_NO(horizon),
                      (ulong) LSN_OFFSET(horizon)));


  *chunk0_header= (byte) (type |TRANSLOG_CHUNK_LSN);
  int2store(chunk0_header + 1, short_trid);
  translog_write_variable_record_1group_code_len(chunk0_header + 3,
                                                 parts->record_length,
                                                 header_length);
  do
  {
    int limit;
    if (new_page_before_chunk0)
    {
      rc= translog_page_next(&horizon, &cursor, &buffer_to_flush);
      if (buffer_to_flush != NULL)
      {
        rc|= translog_buffer_lock(buffer_to_flush);
        translog_buffer_decrease_writers(buffer_to_flush);
        if (!rc)
          rc= translog_buffer_flush(buffer_to_flush);
        rc|= translog_buffer_unlock(buffer_to_flush);
        buffer_to_flush= NULL;
      }
      if (rc)
      {
        UNRECOVERABLE_ERROR(("flush of unlock buffer failed"));
        goto err;
      }
    }
    new_page_before_chunk0= 1;

    if (first_chunk0)
    {
      first_chunk0= 0;
      *lsn= horizon;
      if (log_record_type_descriptor[type].inwrite_hook &&
          (*log_record_type_descriptor[type].inwrite_hook) (type, tcb,
                                                            lsn, parts))
        goto err;
    }

    /*
       A first non-full page will hold type 0 chunk only if it fit in it with
       all its headers => the fist page is full or number of groups less then
       possible number of full page.
    */
    limit= (groups_per_page < groups.elements - curr_group ?
            groups_per_page : groups.elements - curr_group);
    DBUG_PRINT("info", ("Groups: %u  curr: %u  limit: %u",
                        (uint) groups.elements, (uint) curr_group,
                        (uint) limit));

    if (chunk0_pages == 1)
    {
      DBUG_PRINT("info", ("chunk_len: 2 + %u * (7+1) + %u = %u",
                          (uint) limit, (uint) record_rest,
                          (uint) (2 + limit * (7 + 1) + record_rest)));
      int2store(chunk0_header + header_length - 2,
                2 + limit * (7 + 1) + record_rest);
    }
    else
    {
      DBUG_PRINT("info", ("chunk_len: 2 + %u * (7+1) = %u",
                          (uint) limit, (uint) (2 + limit * (7 + 1))));
      int2store(chunk0_header + header_length - 2, 2 + limit * (7 + 1));
    }
    int2store(chunk0_header + header_length, groups.elements - curr_group);
    translog_write_data_on_page(&horizon, &cursor, header_fixed_part,
                                chunk0_header);
    for (i= curr_group; i < limit + curr_group; i++)
    {
      struct st_translog_group_descriptor *grp_ptr;
      grp_ptr= dynamic_element(&groups, i,
                               struct st_translog_group_descriptor *);
      lsn_store(group_desc, grp_ptr->addr);
      group_desc[7]= grp_ptr->num;
      translog_write_data_on_page(&horizon, &cursor, (7 + 1), group_desc);
    }

    if (chunk0_pages == 1 && record_rest != 0)
      translog_write_parts_on_page(&horizon, &cursor, record_rest, parts);

    chunk0_pages--;
    curr_group+= limit;

  } while (chunk0_pages != 0);
  rc= translog_buffer_lock(cursor.buffer);
  if (cmp_translog_addr(cursor.buffer->last_lsn, *lsn) < 0)
    cursor.buffer->last_lsn= *lsn;
  translog_buffer_decrease_writers(cursor.buffer);
  rc|= translog_buffer_unlock(cursor.buffer);

  delete_dynamic(&groups);
  DBUG_RETURN(rc);

err_unlock:
  translog_unlock();
err:
  delete_dynamic(&groups);
  DBUG_RETURN(1);
}


/*
  Write the variable length log record

  SYNOPSIS
    translog_write_variable_record()
    lsn                  LSN of the record will be written here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    parts                Descriptor of record source parts
    tcb                  Transaction control block pointer for hooks by
                         record log type

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_write_variable_record(LSN *lsn,
                                              enum translog_record_type type,
                                              SHORT_TRANSACTION_ID short_trid,
                                              struct st_translog_parts *parts,
                                              void *tcb)
{
  struct st_translog_buffer *buffer_to_flush= NULL;
  uint header_length1= 1 + 2 + 2 +
    translog_variable_record_length_bytes(parts->record_length);
  ulong buffer_rest;
  uint page_rest;
  /* Max number of such LSNs per record is 2 */
  byte compressed_LSNs[2 * LSN_STORE_SIZE];

  DBUG_ENTER("translog_write_variable_record");

  translog_lock();
  DBUG_PRINT("info", ("horizon: (%lu,0x%lx)",
                      (ulong) LSN_FILE_NO(log_descriptor.horizon),
                      (ulong) LSN_OFFSET(log_descriptor.horizon)));
  page_rest= TRANSLOG_PAGE_SIZE - log_descriptor.bc.current_page_fill;
  DBUG_PRINT("info", ("header length: %u  page_rest: %u",
                      header_length1, page_rest));

  /*
     header and part which we should read have to fit in one chunk
     TODO: allow to divide readable header
  */
  if (page_rest <
      (header_length1 + log_record_type_descriptor[type].read_header_len))
  {
    DBUG_PRINT("info",
               ("Next page, size: %u  header: %u + %u",
                log_descriptor.bc.current_page_fill,
                header_length1,
                log_record_type_descriptor[type].read_header_len));
    translog_page_next(&log_descriptor.horizon, &log_descriptor.bc,
                       &buffer_to_flush);
    /* Chunk 2 header is 1 byte, so full page capacity will be one byte more */
    page_rest= log_descriptor.page_capacity_chunk_2 + 1;
    DBUG_PRINT("info", ("page_rest: %u", page_rest));
  }

  /*
     To minimize compressed size we will compress always relative to
     very first chunk address (log_descriptor.horizon for now)
  */
  if (log_record_type_descriptor[type].compressed_LSN > 0)
  {
    if (translog_relative_LSN_encode(parts, log_descriptor.horizon,
                                     log_record_type_descriptor[type].
                                     compressed_LSN, compressed_LSNs))
    {
      translog_unlock();
      if (buffer_to_flush != NULL)
      {
        /*
          It is just try to finish log in nice way in case of error, so we
          do not check result of the following functions, because we are
          going return error state in any case
        */
        translog_buffer_flush(buffer_to_flush);
        translog_buffer_unlock(buffer_to_flush);
      }
      DBUG_RETURN(1);
    }
    /* recalculate header length after compression */
    header_length1= 1 + 2 + 2 +
      translog_variable_record_length_bytes(parts->record_length);
    DBUG_PRINT("info", ("after compressing LSN(s) header length: %u  "
                        "record length: %lu",
                        header_length1, (ulong)parts->record_length));
  }

  /* TODO: check space on current page for header + few bytes */
  if (page_rest >= parts->record_length + header_length1)
  {
    /* following function makes translog_unlock(); */
    DBUG_RETURN(translog_write_variable_record_1chunk(lsn, type, short_trid,
                                                      parts, buffer_to_flush,
                                                      header_length1, tcb));
  }

  buffer_rest= translog_get_current_group_size();

  if (buffer_rest >= parts->record_length + header_length1 - page_rest)
  {
    /* following function makes translog_unlock(); */
    DBUG_RETURN(translog_write_variable_record_1group(lsn, type, short_trid,
                                                      parts, buffer_to_flush,
                                                      header_length1, tcb));
  }
  /* following function makes translog_unlock(); */
  DBUG_RETURN(translog_write_variable_record_mgroup(lsn, type, short_trid,
                                                    parts, buffer_to_flush,
                                                    header_length1,
                                                    buffer_rest, tcb));
}


/*
  Write the fixed and pseudo-fixed log record

  SYNOPSIS
    translog_write_fixed_record()
    lsn                  LSN of the record will be written here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    parts                Descriptor of record source parts
    tcb                  Transaction control block pointer for hooks by
                         record log type

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_write_fixed_record(LSN *lsn,
                                           enum translog_record_type type,
                                           SHORT_TRANSACTION_ID short_trid,
                                           struct st_translog_parts *parts,
                                           void *tcb)
{
  struct st_translog_buffer *buffer_to_flush= NULL;
  byte chunk1_header[1 + 2];
  /* Max number of such LSNs per record is 2 */
  byte compressed_LSNs[2 * LSN_STORE_SIZE];
  struct st_translog_part part;
  int rc;
  DBUG_ENTER("translog_write_fixed_record");
  DBUG_ASSERT((log_record_type_descriptor[type].class ==
               LOGRECTYPE_FIXEDLENGTH &&
               parts->record_length ==
               log_record_type_descriptor[type].fixed_length) ||
              (log_record_type_descriptor[type].class ==
               LOGRECTYPE_PSEUDOFIXEDLENGTH &&
               (parts->record_length -
                log_record_type_descriptor[type].compressed_LSN * 2) <=
               log_record_type_descriptor[type].fixed_length));

  translog_lock();
  DBUG_PRINT("info", ("horizon: (%lu,0x%lx)",
                      (ulong) LSN_FILE_NO(log_descriptor.horizon),
                      (ulong) LSN_OFFSET(log_descriptor.horizon)));

  DBUG_ASSERT(log_descriptor.bc.current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_PRINT("info",
             ("Page size: %u  record: %u  next cond: %d",
              log_descriptor.bc.current_page_fill,
              (parts->record_length -
               log_record_type_descriptor[type].compressed_LSN * 2 + 3),
              ((((uint) log_descriptor.bc.current_page_fill) +
                (parts->record_length -
                 log_record_type_descriptor[type].compressed_LSN * 2 + 3)) >
               TRANSLOG_PAGE_SIZE)));
  /*
     check that there is enough place on current page:
     (log_record_type_descriptor[type].fixed_length - economized on compressed
     LSNs) bytes
  */
  if ((((uint) log_descriptor.bc.current_page_fill) +
       (parts->record_length -
        log_record_type_descriptor[type].compressed_LSN * 2 + 3)) >
      TRANSLOG_PAGE_SIZE)
  {
    DBUG_PRINT("info", ("Next page"));
    translog_page_next(&log_descriptor.horizon, &log_descriptor.bc,
                       &buffer_to_flush);
  }

  *lsn= log_descriptor.horizon;
  if (log_record_type_descriptor[type].inwrite_hook &&
      (*log_record_type_descriptor[type].inwrite_hook) (type, tcb,
                                                        lsn, parts))
  {
    rc= 1;
    goto err;
  }

  /* compress LSNs */
  if (log_record_type_descriptor[type].class == LOGRECTYPE_PSEUDOFIXEDLENGTH)
  {
    DBUG_ASSERT(log_record_type_descriptor[type].compressed_LSN > 0);
    if (translog_relative_LSN_encode(parts, *lsn,
                                     log_record_type_descriptor[type].
                                     compressed_LSN, compressed_LSNs))
    {
      rc= 1;
      goto err;
    }
  }

  /*
    Write the whole record at once (we know that there is enough place on
    the destination page)
  */
  DBUG_ASSERT(parts->current != 0);       /* first part is left for header */
  parts->total_record_length+= (part.len= 1 + 2);
  part.buff= chunk1_header;
  *chunk1_header= (byte) (type | TRANSLOG_CHUNK_FIXED);
  int2store(chunk1_header + 1, short_trid);
  parts->current--;
  set_dynamic(&parts->parts, (gptr) &part, parts->current);

  rc= translog_write_parts_on_page(&log_descriptor.horizon,
                                   &log_descriptor.bc,
                                   parts->total_record_length, parts);

  log_descriptor.bc.buffer->last_lsn= *lsn;

err:
  rc|= translog_unlock();

  /*
     check if we switched buffer and need process it (current buffer is
     unlocked already => we will not delay other threads
  */
  if (buffer_to_flush != NULL)
  {
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    rc|= translog_buffer_unlock(buffer_to_flush);
  }

  DBUG_RETURN(rc);
}


/*
  Write the log record

  SYNOPSIS
    translog_write_record()
    lsn                  LSN of the record will be written here
    type                 the log record type
    short_trid           Sort transaction ID or 0 if it has no sense
    tcb                  Transaction control block pointer for hooks by
                         record log type
    partN_length         length of Ns part of the log
    partN_buffer         pointer on Ns part buffer
    0                    sign of the end of parts

  RETURN
    0  OK
    1  Error
*/

my_bool translog_write_record(LSN *lsn,
                              enum translog_record_type type,
                              SHORT_TRANSACTION_ID short_trid,
                              void *tcb,
                              translog_size_t part1_length,
                              byte *part1_buff, ...)
{
  struct st_translog_parts parts;
  struct st_translog_part part;
  va_list pvar;
  int rc;
  DBUG_ENTER("translog_write_record");
  DBUG_PRINT("enter", ("type: %u  ShortTrID: %u",
                       (uint) type, (uint)short_trid));

  /* move information about parts into dynamic array */
  if (init_dynamic_array(&parts.parts, sizeof(struct st_translog_part),
                         10, 10 CALLER_INFO))
  {
    UNRECOVERABLE_ERROR(("init array failed"));
    DBUG_RETURN(1);
  }

  /* reserve place for header */
  parts.current= 1;
  part.len= 0;
  part.buff= 0;
  if (insert_dynamic(&parts.parts, (gptr) &part))
  {
    UNRECOVERABLE_ERROR(("insert into array failed"));
    DBUG_RETURN(1);
  }

  parts.record_length= part.len= part1_length;
  part.buff= part1_buff;
  if (insert_dynamic(&parts.parts, (gptr) &part))
  {
    UNRECOVERABLE_ERROR(("insert into array failed"));
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("record length: %lu  %lu ...",
                      (ulong) parts.record_length,
                      (ulong) parts.total_record_length));

  /* count record length */
  va_start(pvar, part1_buff);
  for (;;)
  {
    part.len= va_arg(pvar, translog_size_t);
    if (part.len == 0)
      break;
    parts.record_length+= part.len;
    part.buff= va_arg(pvar, byte*);
    if (insert_dynamic(&parts.parts, (gptr) &part))
    {
      UNRECOVERABLE_ERROR(("insert into array failed"));
      DBUG_RETURN(1);
    }
    DBUG_PRINT("info", ("record length: %lu  %lu ...",
                        (ulong) parts.record_length,
                        (ulong) parts.total_record_length));
  }
  va_end(pvar);

  /*
    Start total_record_length from record_length then overhead will
    be add
  */
  parts.total_record_length= parts.record_length;
  va_end(pvar);
  DBUG_PRINT("info", ("record length: %lu  %lu",
                      (ulong) parts.record_length,
                      (ulong) parts.total_record_length));

  /* process this parts */
  if (!(rc= (log_record_type_descriptor[type].prewrite_hook &&
             (*log_record_type_descriptor[type].prewrite_hook) (type, tcb,
                                                                &parts))))
  {
    switch (log_record_type_descriptor[type].class) {
    case LOGRECTYPE_VARIABLE_LENGTH:
      rc= translog_write_variable_record(lsn, type, short_trid, &parts, tcb);
      break;
    case LOGRECTYPE_PSEUDOFIXEDLENGTH:
    case LOGRECTYPE_FIXEDLENGTH:
      rc= translog_write_fixed_record(lsn, type, short_trid, &parts, tcb);
      break;
    case LOGRECTYPE_NOT_ALLOWED:
    default:
      DBUG_ASSERT(0);
      rc= 1;
    }
  }

  delete_dynamic(&parts.parts);
  DBUG_RETURN(rc);
}


/*
  Decode compressed (relative) LSN(s)

  SYNOPSIS
   translog_relative_lsn_decode()
   base_lsn              LSN for encoding
   src                   Decode LSN(s) from here
   dst                   Put decoded LSNs here
   lsns                  number of LSN(s)

   RETURN
     position in sources after decoded LSN(s)
*/

static byte *translog_relative_LSN_decode(LSN base_lsn,
                                          byte *src, byte *dst, uint lsns)
{
  uint i;
  for (i= 0; i < lsns; i++, dst+= LSN_STORE_SIZE)
  {
    src= translog_get_LSN_from_diff(base_lsn, src, dst);
  }
  return src;
}

/*
  Get header of fixed/pseudo length record and call hook for it processing

  SYNOPSIS
    translog_fixed_length_header()
    page                 Pointer to the buffer with page where LSN chunk is
                         placed
    page_offset          Offset of the first chunk in the page
    buff                 Buffer to be filled with header data

  RETURN
    0  error
    #  number of bytes in TRANSLOG_HEADER_BUFFER::header where stored decoded
       part of the header
*/

translog_size_t translog_fixed_length_header(byte *page,
                                             translog_size_t page_offset,
                                             TRANSLOG_HEADER_BUFFER *buff)
{
  struct st_log_record_type_descriptor *desc=
    log_record_type_descriptor + buff->type;
  byte *src= page + page_offset + 3;
  byte *dst= buff->header;
  byte *start= src;
  uint lsns= desc->compressed_LSN;
  uint length= desc->fixed_length + (lsns * 2);

  DBUG_ENTER("translog_fixed_length_header");

  buff->record_length= length;

  if (desc->class == LOGRECTYPE_PSEUDOFIXEDLENGTH)
  {
    DBUG_ASSERT(lsns > 0);
    src= translog_relative_LSN_decode(buff->lsn, src, dst, lsns);
    lsns*= LSN_STORE_SIZE;
    dst+= lsns;
    length-= lsns;
    buff->compressed_LSN_economy= (uint16) (lsns - (src - start));
  }
  else
    buff->compressed_LSN_economy= 0;

  memcpy(dst, src, length);
  buff->non_header_data_start_offset= page_offset +
    ((src + length) - (page + page_offset));
  buff->non_header_data_len= 0;
  DBUG_RETURN(buff->record_length);
}


/*
  Free resources used by TRANSLOG_HEADER_BUFFER

  SYNOPSIS
    translog_free_record_header();
*/

void translog_free_record_header(TRANSLOG_HEADER_BUFFER *buff)
{
  DBUG_ENTER("translog_free_record_header");
  if (buff->groups_no != 0)
  {
    my_free((gptr) buff->groups, MYF(0));
    buff->groups_no= 0;
  }
  DBUG_VOID_RETURN;
}


/*
  Set current horizon in the scanner data structure

  SYNOPSIS
    translog_scanner_set_horizon()
    scanner              Information about current chunk during scanning
*/

static void translog_scanner_set_horizon(struct st_translog_scanner_data
                                         *scanner)
{
  translog_lock();
  scanner->horizon= log_descriptor.horizon;
  translog_unlock();
}


/*
  Set last page in the scanner data structure

  SYNOPSIS
    translog_scanner_set_last_page()
    scanner              Information about current chunk during scanning

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_scanner_set_last_page(TRANSLOG_SCANNER_DATA
                                              *scanner)
{
  my_bool page_ok;
  scanner->last_file_page= scanner->page_addr;
  return (translog_get_last_page_addr(&scanner->last_file_page, &page_ok));
}


/*
  Initialize reader scanner

  SYNOPSIS
    translog_init_scanner()
    lsn                  LSN with which it have to be inited
    fixed_horizon        true if it is OK do not read records which was written
                         after scanning beginning
    scanner              scanner which have to be inited

  RETURN
    0  OK
    1  Error
*/

my_bool translog_init_scanner(LSN lsn,
                              my_bool fixed_horizon,
                              struct st_translog_scanner_data *scanner)
{
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_init_scanner");
  DBUG_PRINT("enter", ("LSN: (0x%lu,0x%lx)",
                       (ulong) LSN_FILE_NO(lsn),
                       (ulong) LSN_OFFSET(lsn));
  DBUG_ASSERT(LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE != 0);

  data.addr= &scanner->page_addr;
  data.was_recovered= 0;

  scanner->page_offset= LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE;

  scanner->fixed_horizon= fixed_horizon;

  translog_scanner_set_horizon(scanner);
  DBUG_PRINT("info", ("horizon: (0x%lu,0x%lx)",
                      (ulong) LSN_FILE_NO(scanner->horizon),
                      (ulong) LSN_OFFSET(scanner->horizon)));

  /* lsn < horizon */
  DBUG_ASSERT(lsn < scanner->horizon));

  scanner->page_addr= lsn;
  scanner->page_addr-= scanner->page_offset; /*decrease offset */

  if (translog_scanner_set_last_page(scanner))
    DBUG_RETURN(1);

  if ((scanner->page= translog_get_page(&data, scanner->buffer)) == NULL)
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/*
  Checks End of the Log

  SYNOPSIS
    translog_scanner_eol()
    scanner              Information about current chunk during scanning

  RETURN
    1  End of the Log
    0  OK
*/
static my_bool translog_scanner_eol(TRANSLOG_SCANNER_DATA *scanner)
{
  DBUG_ENTER("translog_scanner_eol");
  DBUG_PRINT("enter",
             ("Horizon: (%lu, 0x%lx)  Current: (%lu, 0x%lx+0x%x=0x%lx)",
              (ulong) LSN_FILE_NO(scanner->horizon),
              (ulong) LSN_OFFSET(scanner->horizon),
              (ulong) LSN_FILE_NO(scanner->page_addr),
              (ulong) LSN_OFFSET(scanner->page_addr),
              (uint) scanner->page_offset,
              (ulong) (LSN_OFFSET(scanner->page_addr) + scanner->page_offset)));
  if (scanner->horizon > (scanner->page_addr +
                          scanner->page_offset))
  {
    DBUG_PRINT("info", ("Horizon is not reached"));
    DBUG_RETURN(0);
  }
  if (scanner->fixed_horizon)
  {
    DBUG_PRINT("info", ("Horizon is fixed and reached"));
    DBUG_RETURN(1);
  }
  translog_scanner_set_horizon(scanner);
  DBUG_PRINT("info",
             ("Horizon is re-read, EOL: %d",
              scanner->horizon <= (scanner->page_addr +
                                   scanner->page_offset)));
  DBUG_RETURN(scanner->horizon <= (scanner->page_addr +
                                   scanner->page_offset));
}


/*
  Cheks End of the Page

  SYNOPSIS
    translog_scanner_eop()
    scanner              Information about current chunk during scanning

  RETURN
    1  End of the Page
    0  OK
*/
static my_bool translog_scanner_eop(TRANSLOG_SCANNER_DATA *scanner)
{
  DBUG_ENTER("translog_scanner_eop");
  DBUG_RETURN(scanner->page_offset >= TRANSLOG_PAGE_SIZE ||
              scanner->page[scanner->page_offset] == 0);
}


/*
  Checks End of the File (I.e. we are scanning last page, which do not
  mean end of this page)

  SYNOPSIS
    translog_scanner_eof()
    scanner              Information about current chunk during scanning

  RETURN
    1  End of the File
    0  OK
*/
static my_bool translog_scanner_eof(TRANSLOG_SCANNER_DATA *scanner)
{
  DBUG_ENTER("translog_scanner_eof");
  DBUG_ASSERT(LSN_FILE_NO(scanner->page_addr) ==
              LSN_FILE_NO(scanner->last_file_page));
  DBUG_PRINT("enter", ("curr Page: 0x%lx  last page: 0x%lx  "
                       "normal EOF: %d",
                       (ulong) LSN_OFFSET(scanner->page_addr),
                       (ulong) LSN_OFFSET(scanner->last_file_page),
                       LSN_OFFSET(scanner->page_addr) ==
                       LSN_OFFSET(scanner->last_file_page)));
  /*
     TODO: detect damaged file EOF,
     TODO: issue warning if damaged file EOF detected
  */
  DBUG_RETURN(scanner->page_addr ==
              scanner->last_file_page);
}


/*
  Move scanner to the next chunk

  SYNOPSIS
    translog_get_next_chunk()
    scanner              Information about current chunk during scanning

  RETURN
    0  OK
    1  Error
*/

static my_bool
translog_get_next_chunk(TRANSLOG_SCANNER_DATA *scanner)
{
  uint16 len;
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_get_next_chunk");

  if ((len= translog_get_total_chunk_length(scanner->page,
                                            scanner->page_offset)) == 0)
    DBUG_RETURN(1);
  scanner->page_offset+= len;

  if (translog_scanner_eol(scanner))
  {
    scanner->page= &end_of_log;
    scanner->page_offset= 0;
    DBUG_RETURN(0);
  }
  if (translog_scanner_eop(scanner))
  {
    if (translog_scanner_eof(scanner))
    {
      DBUG_PRINT("info", ("horizon: (%lu,0x%lx)  pageaddr: (%lu,0x%lx)",
                          (ulong) LSN_FILE_NO(scanner->horizon),
                          (ulong) LSN_OFFSET(scanner->horizon),
                          (ulong) LSN_FILE_NO(scanner->page_addr),
                          (ulong) LSN_OFFSET(scanner->page_addr)));
      /* if it is log end it have to be caught before */
      DBUG_ASSERT(LSN_FILE_NO(scanner->horizon) >
                  LSN_FILE_NO(scanner->page_addr));
      scanner->page_addr+= LSN_ONE_FILE;
      scanner->page_addr= LSN_REPLACE_OFFSET(scanner->page_addr,
                                             TRANSLOG_PAGE_SIZE);
      if (translog_scanner_set_last_page(scanner))
        DBUG_RETURN(1);
    }
    else
    {
      scanner->page_addr+= TRANSLOG_PAGE_SIZE; /* offset increased */
    }

    data.addr= &scanner->page_addr;
    data.was_recovered= 0;
    if ((scanner->page= translog_get_page(&data, scanner->buffer)) == NULL)
      DBUG_RETURN(1);

    scanner->page_offset= translog_get_first_chunk_offset(scanner->page);
    if (translog_scanner_eol(scanner))
    {
      scanner->page= &end_of_log;
      scanner->page_offset= 0;
      DBUG_RETURN(0);
    }
    DBUG_ASSERT(scanner->page[scanner->page_offset]);
  }
  DBUG_RETURN(0);
}


/*
  Get header of variable length record and call hook for it processing

  SYNOPSIS
    translog_variable_length_header()
    page                 Pointer to the buffer with page where LSN chunk is
                         placed
    page_offset          Offset of the first chunk in the page
    buff                 Buffer to be filled with header data
    scanner              If present should be moved to the header page if
                         it differ from LSN page

  RETURN
    0  error
    #  number of bytes in TRANSLOG_HEADER_BUFFER::header where stored decoded
       part of the header
*/

translog_size_t translog_variable_length_header(byte *page,
                                                translog_size_t page_offset,
                                                TRANSLOG_HEADER_BUFFER *buff,
                                                TRANSLOG_SCANNER_DATA
                                                *scanner)
{
  struct st_log_record_type_descriptor *desc= (log_record_type_descriptor +
                                               buff->type);
  byte *src= page + page_offset + 1 + 2;
  byte *dst= buff->header;
  LSN base_lsn;
  uint lsns= desc->compressed_LSN;
  uint16 chunk_len;
  uint16 length= desc->read_header_len + (lsns * 2);
  uint16 buffer_length= length;
  uint16 body_len;
  TRANSLOG_SCANNER_DATA internal_scanner;

  DBUG_ENTER("translog_variable_length_header");

  buff->record_length= translog_variable_record_1group_decode_len(&src);
  chunk_len= uint2korr(src);
  DBUG_PRINT("info", ("rec len: %lu  chunk len: %u  length: %u  bufflen: %u",
                      (ulong) buff->record_length, (uint) chunk_len,
                      (uint) length, (uint) buffer_length));
  if (chunk_len == 0)
  {
    uint16 page_rest;
    DBUG_PRINT("info", ("1 group"));
    src+= 2;
    page_rest= TRANSLOG_PAGE_SIZE - (src - page);

    base_lsn= buff->lsn;
    body_len= min(page_rest, buff->record_length);
  }
  else
  {
    uint grp_no, curr;
    uint header_to_skip;
    uint16 page_rest;

    DBUG_PRINT("info", ("multi-group"));
    grp_no= buff->groups_no= uint2korr(src + 2);
    if (!(buff->groups=
          (TRANSLOG_GROUP*) my_malloc(sizeof(TRANSLOG_GROUP) * grp_no,
                                      MYF(0))))
      DBUG_RETURN(0);
    DBUG_PRINT("info", ("Groups: %u", (uint) grp_no));
    src+= (2 + 2);
    page_rest= TRANSLOG_PAGE_SIZE - (src - page);
    curr= 0;
    header_to_skip= src - (page + page_offset);
    buff->chunk0_pages= 0;

    for (;;)
    {
      uint i, read= grp_no;

      buff->chunk0_pages++;
      if (page_rest < grp_no * (7 + 1))
        read= page_rest / (7 + 1);
      DBUG_PRINT("info", ("Read chunk0 page#%u  read: %u  left: %u  "
                          "start from: %u",
                          buff->chunk0_pages, read, grp_no, curr));
      for (i= 0; i < read; i++, curr++)
      {
        DBUG_ASSERT(curr < buff->groups_no);
        buff->groups[curr].addr= lsn_korr(src + i * (7 + 1));
        buff->groups[curr].num= src[i * (7 + 1) + 7];
        DBUG_PRINT("info", ("group #%u (%lu,0x%lx)  chunks: %u",
                            curr,
                            (ulong) LSN_FILE_NO(buff->groups[curr].addr),
                            (ulong) LSN_OFFSET(buff->groups[curr].addr),
                            (uint) buff->groups[curr].num));
      }
      grp_no-= read;
      if (grp_no == 0)
      {
        if (scanner)
        {
          buff->chunk0_data_addr= scanner->page_addr;
          buff->chunk0_data_addr+= (page_offset + header_to_skip +
                                    read * (7 + 1)); /* offset increased */
        }
        else
        {
          buff->chunk0_data_addr= buff->lsn;
          /* offset increased */
          buff->chunk0_data_addr+= (header_to_skip + read * (7 + 1));
        }
        buff->chunk0_data_len= chunk_len - 2 - read * (7 + 1);
        DBUG_PRINT("info", ("Data address: (%lu,0x%lx)  len: %u",
                            (ulong) LSN_FILE_NO(buff->chunk0_data_addr),
                            (ulong) LSN_OFFSET(buff->chunk0_data_addr),
                            buff->chunk0_data_len));
        break;
      }
      if (scanner == NULL)
      {
        DBUG_PRINT("info", ("use internal scanner for header reading"));
        scanner= &internal_scanner;
        translog_init_scanner(buff->lsn, 1, scanner);
      }
      translog_get_next_chunk(scanner);
      page= scanner->page;
      page_offset= scanner->page_offset;
      src= page + page_offset + header_to_skip;
      chunk_len= uint2korr(src - 2 - 2);
      DBUG_PRINT("info", ("Chunk len: %u", (uint) chunk_len));
      page_rest= TRANSLOG_PAGE_SIZE - (src - page);
    }

    if (scanner == NULL)
    {
      DBUG_PRINT("info", ("use internal scanner"));
      scanner= &internal_scanner;
    }

    base_lsn= buff->groups[0].addr;
    translog_init_scanner(base_lsn, 1, scanner);
    /* first group chunk is always chunk type 2 */
    page= scanner->page;
    page_offset= scanner->page_offset;
    src= page + page_offset + 1;
    page_rest= TRANSLOG_PAGE_SIZE - (src - page);
    body_len= page_rest;
  }
  if (lsns)
  {
    byte *start= src;
    src= translog_relative_LSN_decode(base_lsn, src, dst, lsns);
    lsns*= LSN_STORE_SIZE;
    dst+= lsns;
    length-= lsns;
    buff->record_length+= (buff->compressed_LSN_economy=
                           (uint16) (lsns - (src - start)));
    DBUG_PRINT("info", ("lsns: %u  length: %u  economy: %u  new length: %lu",
                        lsns / LSN_STORE_SIZE, (uint) length,
                        (uint) buff->compressed_LSN_economy,
                        (ulong) buff->record_length));
    body_len-= (src - start);
  }
  else
    buff->compressed_LSN_economy= 0;

  DBUG_ASSERT(body_len >= length);
  body_len-= length;
  memcpy(dst, src, length);
  buff->non_header_data_start_offset= src + length - page;
  buff->non_header_data_len= body_len;
  DBUG_PRINT("info", ("non_header_data_start_offset: %u  len: %u  buffer: %u",
                      buff->non_header_data_start_offset,
                      buff->non_header_data_len, buffer_length));
  DBUG_RETURN(buffer_length);
}


/*
  Read record header from the given buffer

  SYNOPSIS
    translog_read_record_header_from_buffer()
    page                 page content buffer
    page_offset          offset of the chunk in the page
    buff                 destination buffer
    scanner              If this is set the scanner will be moved to the
                         record header page (differ from LSN page in case of
                         multi-group records)
*/

translog_size_t
translog_read_record_header_from_buffer(byte *page,
                                        uint16 page_offset,
                                        TRANSLOG_HEADER_BUFFER *buff,
                                        TRANSLOG_SCANNER_DATA *scanner)
{
  DBUG_ENTER("translog_read_record_header_from_buffer");
  DBUG_ASSERT((page[page_offset] & TRANSLOG_CHUNK_TYPE) ==
              TRANSLOG_CHUNK_LSN ||
              (page[page_offset] & TRANSLOG_CHUNK_TYPE) ==
              TRANSLOG_CHUNK_FIXED);
  buff->type= (page[page_offset] & TRANSLOG_REC_TYPE);
  buff->short_trid= uint2korr(page + page_offset + 1);
  DBUG_PRINT("info", ("Type %u, Sort TrID %u, LSN (%lu,0x%lx)",
                      (uint) buff->type, (uint)buff->short_trid,
                      (ulong) LSN_FILE_NO(buff->lsn),
                      (ulong) LSN_OFFSET(buff->lsn)));
  /* Read required bytes from the header and call hook */
  switch (log_record_type_descriptor[buff->type].class) {
  case LOGRECTYPE_VARIABLE_LENGTH:
    DBUG_RETURN(translog_variable_length_header(page, page_offset, buff,
                                                scanner));
  case LOGRECTYPE_PSEUDOFIXEDLENGTH:
  case LOGRECTYPE_FIXEDLENGTH:
    DBUG_RETURN(translog_fixed_length_header(page, page_offset, buff));
  default:
    DBUG_ASSERT(0);
  }
  DBUG_RETURN(0);                               /* purecov: deadcode */
}


/*
  Read record header and some fixed part of a record (the part depend on
  record type).

  SYNOPSIS
    translog_read_record_header()
    lsn                  log record serial number (address of the record)
    buff                 log record header buffer

  NOTE
    - lsn can point to TRANSLOG_HEADER_BUFFER::lsn and it will be processed
      correctly.
    - Some type of record can be read completely by this call
    - "Decoded" header stored in TRANSLOG_HEADER_BUFFER::header (relative
      LSN can be translated to absolute one), some fields can be added
      (like actual header length in the record if the header has variable
      length)

  RETURN
    0  error
    #  number of bytes in TRANSLOG_HEADER_BUFFER::header where stored decoded
       part of the header
*/

translog_size_t translog_read_record_header(LSN lsn,
                                            TRANSLOG_HEADER_BUFFER *buff)
{
  byte buffer[TRANSLOG_PAGE_SIZE], *page;
  translog_size_t page_offset= LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE;
  TRANSLOG_ADDRESS addr;
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_read_record_header");
  DBUG_PRINT("enter", ("LSN: (0x%lu,0x%lx)",
                       (ulong) LSN_FILE_NO(lsn), (ulong) LSN_OFFSET(lsn)));
  DBUG_ASSERT(LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE != 0);

  buff->lsn= lsn;
  buff->groups_no= 0;
  data.addr= &addr;
  data.was_recovered= 0;
  addr= lsn;
  addr-= page_offset; /* offset decreasing */
  if (!(page= translog_get_page(&data, buffer)))
    DBUG_RETURN(0);

  DBUG_RETURN(translog_read_record_header_from_buffer(page, page_offset,
                                                      buff, 0));
}


/*
  Read record header and some fixed part of a record (the part depend on
  record type).

  SYNOPSIS
    translog_read_record_header_scan()
    scan                 scanner position to read
    buff                 log record header buffer
    move_scanner         request to move scanner to the header position

  NOTE
    - Some type of record can be read completely by this call
    - "Decoded" header stored in TRANSLOG_HEADER_BUFFER::header (relative
      LSN can be translated to absolute one), some fields can be added
      (like actual header length in the record if the header has variable
      length)

  RETURN
    0  error
    #  number of bytes in TRANSLOG_HEADER_BUFFER::header where stored decoded
       part of the header
*/

translog_size_t
translog_read_record_header_scan(TRANSLOG_SCANNER_DATA
                                 *scanner,
                                 TRANSLOG_HEADER_BUFFER *buff,
                                 my_bool move_scanner)
{
  DBUG_ENTER("translog_read_record_header_scan");
  DBUG_PRINT("enter", ("Scanner: Cur: (%lu,0x%lx)  Hrz: (%lu,0x%lx)  "
                       "Lst: (%lu,0x%lx)  Offset: %u(%x)  fixed %d",
                       (ulong) LSN_FILE_NO(scanner->page_addr),
                       (ulong) LSN_OFFSET(scanner->page_addr),
                       (ulong) LSN_FILE_NO(scanner->horizon),
                       (ulong) LSN_OFFSET(scanner->horizon),
                       (ulong) LSN_FILE_NO(scanner->last_file_page),
                       (ulong) LSN_OFFSET(scanner->last_file_page),
                       (uint) scanner->page_offset,
                       (uint) scanner->page_offset, scanner->fixed_horizon));
  buff->groups_no= 0;
  buff->lsn= scanner->page_addr;
  buff->lsn+= scanner->page_offset; /* offset increasing */
  DBUG_RETURN(translog_read_record_header_from_buffer(scanner->page,
                                                      scanner->page_offset,
                                                      buff,
                                                      (move_scanner ?
                                                       scanner : 0)));
}


/*
  Read record header and some fixed part of the next record (the part
  depend on record type).

  SYNOPSIS
    translog_read_next_record_header()
    scanner              data for scanning if lsn is NULL scanner data
                         will be used for continue scanning.
                         The scanner can be NULL.
    buff                 log record header buffer

  NOTE
    - it is like translog_read_record_header, but read next record, so see
      its NOTES.
    - in case of end of the log buff->lsn will be set to
      (CONTROL_FILE_IMPOSSIBLE_LSN)
  RETURN
    0                                    error
    TRANSLOG_RECORD_HEADER_MAX_SIZE + 1  End of the log
    #                                    number of bytes in
                                         TRANSLOG_HEADER_BUFFER::header
                                         where stored decoded
                                         part of the header
*/

translog_size_t translog_read_next_record_header(TRANSLOG_SCANNER_DATA
                                                 *scanner,
                                                 TRANSLOG_HEADER_BUFFER *buff)
{
  uint8 chunk_type;

  buff->groups_no= 0;        /* to be sure that we will free it right */

  DBUG_ENTER("translog_read_next_record_header");
  DBUG_PRINT("enter", ("scanner: 0x%lx", (ulong) scanner));
  DBUG_PRINT("info", ("Scanner: Cur: (%lu,0x%lx)  Hrz: (%lu,0x%lx)  "
                      "Lst: (%lu,0x%lx)  Offset: %u(%x)  fixed: %d",
                      (ulong) LSN_FILE_NO(scanner->page_addr),
                      (ulong) LSN_OFFSET(scanner->page_addr),
                      (ulong) LSN_FILE_NO(scanner->horizon),
                      (ulong) LSN_OFFSET(scanner->horizon),
                      (ulong) LSN_FILE_NO(scanner->last_file_page),
                      (ulong) LSN_OFFSET(scanner->last_file_page),
                      (uint) scanner->page_offset,
                      (uint) scanner->page_offset, scanner->fixed_horizon));

  do
  {
    if (translog_get_next_chunk(scanner))
      DBUG_RETURN(0);
    chunk_type= scanner->page[scanner->page_offset] & TRANSLOG_CHUNK_TYPE;
    DBUG_PRINT("info", ("type: %x  byte: %x", (uint) chunk_type,
                        (uint) scanner->page[scanner->page_offset]));
  } while (chunk_type != TRANSLOG_CHUNK_LSN && chunk_type !=
           TRANSLOG_CHUNK_FIXED && scanner->page[scanner->page_offset] != 0);

  if (scanner->page[scanner->page_offset] == 0)
  {
    /* Last record was read */
    buff->lsn= CONTROL_FILE_IMPOSSIBLE_LSN;
    /* Return 'end of log' marker */
    DBUG_RETURN(TRANSLOG_RECORD_HEADER_MAX_SIZE + 1);
  }
  DBUG_RETURN(translog_read_record_header_scan(scanner, buff, 0));
}


/*
  Moves record data reader to the next chunk and fill the data reader
  information about that chunk.

  SYNOPSIS
    translog_record_read_next_chunk()
    data                 data cursor

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_record_read_next_chunk(struct st_translog_reader_data
                                               *data)
{
  translog_size_t new_current_offset= data->current_offset + data->chunk_size;
  uint16 chunk_header_len, chunk_len;
  uint8 type;
  DBUG_ENTER("translog_record_read_next_chunk");

  if (data->eor)
  {
    DBUG_PRINT("info", ("end of the record flag set"));
    DBUG_RETURN(1);
  }

  if (data->header.groups_no &&
      data->header.groups_no - 1 != data->current_group &&
      data->header.groups[data->current_group].num == data->current_chunk)
  {
    /* Goto next group */
    data->current_group++;
    data->current_chunk= 0;
    DBUG_PRINT("info", ("skip to group: #%u", data->current_group));
    translog_init_scanner(data->header.groups[data->current_group].addr,
                          1, &data->scanner);
  }
  else
  {
    data->current_chunk++;
    if (translog_get_next_chunk(&data->scanner))
      DBUG_RETURN(1);
  }
  type= data->scanner.page[data->scanner.page_offset] & TRANSLOG_CHUNK_TYPE;

  if (type == TRANSLOG_CHUNK_LSN && data->header.groups_no)
  {
    DBUG_PRINT("info",
               ("Last chunk: data len: %u  offset: %u  group: %u of %u",
                data->header.chunk0_data_len, data->scanner.page_offset,
                data->current_group, data->header.groups_no - 1));
    DBUG_ASSERT(data->header.groups_no - 1 == data->current_group);
    DBUG_ASSERT(data->header.lsn ==
                data->scanner.page_addr + data->scanner.page_offset);
    translog_init_scanner(data->header.chunk0_data_addr, 1, &data->scanner);
    data->chunk_size= data->header.chunk0_data_len;
    data->body_offset= data->scanner.page_offset;
    data->current_offset= new_current_offset;
    data->eor= 1;
    DBUG_RETURN(0);
  }

  if (type == TRANSLOG_CHUNK_LSN || type == TRANSLOG_CHUNK_FIXED)
  {
    data->eor= 1;
    DBUG_RETURN(1);                             /* End of record */
  }

  chunk_header_len=
    translog_get_chunk_header_length(data->scanner.page,
                                     data->scanner.page_offset);
  chunk_len= translog_get_total_chunk_length(data->scanner.page,
                                             data->scanner.page_offset);
  data->chunk_size= chunk_len - chunk_header_len;
  data->body_offset= data->scanner.page_offset + chunk_header_len;
  data->current_offset= new_current_offset;
  DBUG_PRINT("info", ("grp: %u  chunk: %u  body_offset: %u  chunk_size: %u  "
                      "current_offset: %lu",
                      (uint) data->current_group,
                      (uint) data->current_chunk,
                      (uint) data->body_offset,
                      (uint) data->chunk_size, (ulong) data->current_offset));
  DBUG_RETURN(0);
}


/*
  Initialize record reader data from LSN

  SYNOPSIS
    translog_init_reader_data()
    lsn                  reference to LSN we should start from
    data                 reader data to initialize

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_init_reader_data(LSN lsn,
                                         struct st_translog_reader_data *data)
{
  DBUG_ENTER("translog_init_reader_data");
  if (translog_init_scanner(lsn, 1, &data->scanner) ||
      !(data->read_header=
        translog_read_record_header_scan(&data->scanner, &data->header, 1)))
  {
    DBUG_RETURN(1);
  }
  data->body_offset= data->header.non_header_data_start_offset;
  data->chunk_size= data->header.non_header_data_len;
  data->current_offset= data->read_header;
  data->current_group= 0;
  data->current_chunk= 0;
  data->eor= 0;
  DBUG_PRINT("info", ("read_header: %u  "
                      "body_offset: %u  chunk_size: %u  current_offset: %lu",
                      (uint) data->read_header,
                      (uint) data->body_offset,
                      (uint) data->chunk_size, (ulong) data->current_offset));
  DBUG_RETURN(0);
}


/*
  Read a part of the record.

  SYNOPSIS
    translog_read_record_header()
    lsn                  log record serial number (address of the record)
    offset               From the beginning of the record beginning (read§
                         by translog_read_record_header).
    length               Length of record part which have to be read.
    buffer               Buffer where to read the record part (have to be at
                         least 'length' bytes length)

  RETURN
    length of data actually read
*/

translog_size_t translog_read_record(LSN lsn,
                                     translog_size_t offset,
                                     translog_size_t length,
                                     byte *buffer,
                                     struct st_translog_reader_data *data)
{
  translog_size_t requested_length= length;
  translog_size_t end= offset + length;
  struct st_translog_reader_data internal_data;
  DBUG_ENTER("translog_read_record");

  if (data == NULL)
  {
    DBUG_ASSERT(lsn != CONTROL_FILE_IMPOSSIBLE_LSN);
    data= &internal_data;
  }
  if (lsn ||
      (offset < data->current_offset &&
       !(offset < data->read_header && offset + length < data->read_header)))
  {
    if (translog_init_reader_data(lsn, data))
      DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("Offset: %lu  length: %lu  "
                      "Scanner: Cur: (%lu,0x%lx)  Hrz: (%lu,0x%lx)  "
                      "Lst: (%lu,0x%lx)  Offset: %u(%x)  fixed: %d",
                      (ulong) offset, (ulong) length,
                      (ulong) LSN_FILE_NO(data->scanner.page_addr),
                      (ulong) LSN_OFFSET(data->scanner.page_addr),
                      (ulong) LSN_FILE_NO(data->scanner.horizon),
                      (ulong) LSN_OFFSET(data->scanner.horizon),
                      (ulong) LSN_FILE_NO(data->scanner.last_file_page),
                      (ulong) LSN_OFFSET(data->scanner.last_file_page),
                      (uint) data->scanner.page_offset,
                      (uint) data->scanner.page_offset,
                      data->scanner.fixed_horizon));
  if (offset < data->read_header)
  {
    uint16 len= min(data->read_header, end) - offset;
    DBUG_PRINT("info",
               ("enter header offset: %lu  length: %lu",
                (ulong) offset, (ulong) length));
    memcpy(buffer, data->header.header + offset, len);
    length-= len;
    if (length == 0)
      DBUG_RETURN(requested_length);
    offset+= len;
    buffer+= len;
    DBUG_PRINT("info",
               ("len: %u  offset: %lu   curr: %lu  length: %lu",
                len, (ulong) offset, (ulong) data->current_offset,
                (ulong) length));
  }
  /* TODO: find first page which we should read by offset */

  /* read the record chunk by chunk */
  for(;;)
  {
    uint page_end= data->current_offset + data->chunk_size;
    DBUG_PRINT("info",
               ("enter body offset: %lu  curr: %lu  "
                "length: %lu  page_end: %lu",
                (ulong) offset, (ulong) data->current_offset, (ulong) length,
                (ulong) page_end));
    if (offset < page_end)
    {
      uint len= page_end - offset;
      DBUG_ASSERT(offset >= data->current_offset);
      memcpy(buffer,
              data->scanner.page + data->body_offset +
              (offset - data->current_offset), len);
      length-= len;
      if (length == 0)
        DBUG_RETURN(requested_length);
      offset+= len;
      buffer+= len;
      DBUG_PRINT("info",
                 ("len: %u  offset: %lu  curr: %lu  length: %lu",
                  len, (ulong) offset, (ulong) data->current_offset,
                  (ulong) length));
    }
    if (translog_record_read_next_chunk(data))
      DBUG_RETURN(requested_length - length);
  }
}


/*
  Force skipping to the next buffer

  SYNOPSIS
    translog_force_current_buffer_to_finish()
*/

static void translog_force_current_buffer_to_finish()
{
  TRANSLOG_ADDRESS new_buff_begunning;
  uint16 old_buffer_no= log_descriptor.bc.buffer_no;
  uint16 new_buffer_no= (old_buffer_no + 1) % TRANSLOG_BUFFERS_NO;
  struct st_translog_buffer *new_buffer= (log_descriptor.buffers +
                                          new_buffer_no);
  struct st_translog_buffer *old_buffer= log_descriptor.bc.buffer;
  byte *data= log_descriptor.bc.ptr -log_descriptor.bc.current_page_fill;
  uint16 left= TRANSLOG_PAGE_SIZE - log_descriptor.bc.current_page_fill;
  uint16 current_page_fill, write_counter, previous_offset;
  DBUG_ENTER("translog_force_current_buffer_to_finish");
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  "
                       "Buffer addr: (%lu,0x%lx)  "
                       "Page addr: (%lu,0x%lx)  "
                       "New Buff: (%lu,0x%lx)  "
                       "size: %lu (%lu)  Pg: %u  left: %u",
                       (uint) log_descriptor.bc.buffer_no,
                       (ulong) log_descriptor.bc.buffer,
                       (ulong) LSN_FILE_NO(log_descriptor.bc.buffer->offset),
                       (ulong) LSN_OFFSET(log_descriptor.bc.buffer->offset),
                       (ulong) LSN_FILE_NO(log_descriptor.horizon),
                       (ulong) (LSN_OFFSET(log_descriptor.horizon) -
                                log_descriptor.bc.current_page_fill),
                       (ulong) LSN_FILE_NO(new_buff_begunning),
                       (ulong) LSN_OFFSET(new_buff_begunning),
                       (ulong) log_descriptor.bc.buffer->size,
                       (ulong) (log_descriptor.bc.ptr -log_descriptor.bc.
                                buffer->buffer),
                       (uint) log_descriptor.bc.current_page_fill,
                       (uint) left));

  new_buff_begunning= log_descriptor.bc.buffer->offset;
  new_buff_begunning+= log_descriptor.bc.buffer->size; /* increase offset */

  DBUG_ASSERT(log_descriptor.bc.ptr !=NULL);
  DBUG_ASSERT((log_descriptor.bc.ptr -log_descriptor.bc.buffer->buffer)
              % TRANSLOG_PAGE_SIZE ==
              log_descriptor.bc.current_page_fill % TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(LSN_FILE_NO(log_descriptor.horizon) ==
              LSN_FILE_NO(log_descriptor.bc.buffer->offset));
  DBUG_ASSERT(LSN_OFFSET(log_descriptor.bc.buffer->offset) +
              (log_descriptor.bc.ptr -log_descriptor.bc.buffer->buffer) ==
              LSN_OFFSET(log_descriptor.horizon));
  DBUG_ASSERT(left < TRANSLOG_PAGE_SIZE);
  if (left != 0)
  {
    /*
       TODO: if 'left' is so small that can't hold any other record
       then do not move the page
    */
    DBUG_PRINT("info", ("left: %u", (uint) left));

    /* decrease offset */
    new_buff_begunning-= log_descriptor.bc.current_page_fill;
    current_page_fill= log_descriptor.bc.current_page_fill;

    bzero(log_descriptor.bc.ptr, left);
    log_descriptor.bc.buffer->size+= left;
    DBUG_PRINT("info", ("Finish Page buffer #%u: 0x%lx  "
                        "Size: %lu",
                        (uint) log_descriptor.bc.buffer->buffer_no,
                        (ulong) log_descriptor.bc.buffer,
                        (ulong) log_descriptor.bc.buffer->size));
    DBUG_ASSERT(log_descriptor.bc.buffer->buffer_no ==
                log_descriptor.bc.buffer_no);
  }
  else
  {
    left= 0;
    log_descriptor.bc.current_page_fill= 0;
  }

  translog_buffer_lock(new_buffer);
  translog_wait_for_buffer_free(new_buffer);

  write_counter= log_descriptor.bc.write_counter;
  previous_offset= log_descriptor.bc.previous_offset;
  translog_start_buffer(new_buffer, &log_descriptor.bc, new_buffer_no);
  log_descriptor.bc.buffer->offset= new_buff_begunning;
  log_descriptor.bc.write_counter= write_counter;
  log_descriptor.bc.previous_offset= previous_offset;

  if (data[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION)
  {
    translog_put_sector_protection(data, &log_descriptor.bc);
    if (left)
    {
      log_descriptor.bc.write_counter++;
      log_descriptor.bc.previous_offset= current_page_fill;
    }
    else
    {
      DBUG_PRINT("info", ("drop write_counter"));
      log_descriptor.bc.write_counter= 0;
      log_descriptor.bc.previous_offset= 0;
    }
  }

  if (data[TRANSLOG_PAGE_FLAGS] & TRANSLOG_PAGE_CRC)
  {
    uint32 crc= translog_crc(data + log_descriptor.page_overhead,
                             TRANSLOG_PAGE_SIZE -
                             log_descriptor.page_overhead);
    DBUG_PRINT("info", ("CRC: 0x%lx", (ulong) crc));
    int4store(data + 3 + 3 + 1, crc);
  }

  if (left)
  {
    memcpy(new_buffer->buffer, data, current_page_fill);
    log_descriptor.bc.ptr +=current_page_fill;
    log_descriptor.bc.buffer->size= log_descriptor.bc.current_page_fill=
      current_page_fill;
    new_buffer->overlay= old_buffer;
  }
  else
    translog_new_page_header(&log_descriptor.horizon, &log_descriptor.bc);

  DBUG_VOID_RETURN;
}


/*
  Flush the log up to given LSN (included)

  SYNOPSIS
    translog_flush()
    lsn                  log record serial number up to which (inclusive)
                         the log have to be flushed

  RETURN
    0  OK
    1  Error
*/

my_bool translog_flush(LSN lsn)
{
  LSN old_flushed, sent_to_file;
  int rc= 0;
  uint i;
  my_bool full_circle= 0;
  DBUG_ENTER("translog_flush");
  DBUG_PRINT("enter", ("Flush up to LSN: (%lu,0x%lx)",
                       (ulong) LSN_FILE_NO(lsn),
                       (ulong) LSN_OFFSET(lsn)));

  translog_lock();
  old_flushed= log_descriptor.flushed;
  for (;;)
  {
    uint16 buffer_no= log_descriptor.bc.buffer_no;
    uint16 buffer_start= buffer_no;
    struct st_translog_buffer *buffer_unlock= log_descriptor.bc.buffer;
    struct st_translog_buffer *buffer= log_descriptor.bc.buffer;
    /* we can't flush in future */
    DBUG_ASSERT(cmp_translog_addr(log_descriptor.horizon, lsn) >= 0);
    if (cmp_translog_addr(log_descriptor.flushed, lsn) >= 0)
    {
      DBUG_PRINT("info", ("already flushed: (%lu,0x%lx)",
                          (ulong) LSN_FILE_NO(log_descriptor.flushed),
                          (ulong) LSN_OFFSET(log_descriptor.flushed)));
      translog_unlock();
      DBUG_RETURN(0);
    }
    /* send to the file if it is not sent */
    translog_get_sent_to_file(&sent_to_file);
    if (cmp_translog_addr(sent_to_file, lsn) >= 0)
      break;

    do
    {
      buffer_no= (buffer_no + 1) % TRANSLOG_BUFFERS_NO;
      buffer= log_descriptor.buffers + buffer_no;
      translog_buffer_lock(buffer);
      translog_buffer_unlock(buffer_unlock);
      buffer_unlock= buffer;
      if (buffer->file != -1)
      {
        buffer_unlock= NULL;
        if (buffer_start == buffer_no)
        {
          /* we made a circle */
          full_circle= 1;
          translog_force_current_buffer_to_finish();
        }
        break;
      }
    } while ((buffer_start != buffer_no) &&
             cmp_translog_addr(log_descriptor.flushed, lsn) < 0);
    if (buffer_unlock != NULL)
      translog_buffer_unlock(buffer_unlock);
    rc= translog_buffer_flush(buffer);
    translog_buffer_unlock(buffer);
    if (rc)
      DBUG_RETURN(1);
    if (!full_circle)
      translog_lock();
  }

  for (i= LSN_FILE_NO(old_flushed); i <= LSN_FILE_NO(lsn); i++)
  {
    uint cache_index;
    File file;

    if ((cache_index= LSN_FILE_NO(log_descriptor.horizon) - i) <
        OPENED_FILES_NUM)
    {
      /* file in the cache */
      if (log_descriptor.log_file_num[cache_index] == -1)
      {
        if ((log_descriptor.log_file_num[cache_index]=
             open_logfile_by_number_no_cache(i)) == -1)
        {
          translog_unlock();
          DBUG_RETURN(1);
        }
      }
      file= log_descriptor.log_file_num[cache_index];
      rc|= my_sync(file, MYF(MY_WME));
    }
    /* We sync file when we are closing it => do nothing if file closed */
  }
  log_descriptor.flushed= sent_to_file;
  rc|= my_sync(log_descriptor.directory_fd, MYF(MY_WME));
  translog_unlock();
  DBUG_RETURN(rc);
}
