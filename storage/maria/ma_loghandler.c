/* Copyright (C) 2007 MySQL AB & Sanja Belkin. 2010 Monty Program Ab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "maria_def.h"
#include "trnman.h"
#include "ma_blockrec.h" /* for some constants and in-write hooks */
#include "ma_key_recover.h" /* For some in-write hooks */
#include "ma_checkpoint.h"

/*
  On Windows, neither my_open() nor my_sync() work for directories.
  Also there is no need to flush filesystem changes ,i.e to sync()
  directories.
*/
#ifdef __WIN__
#define sync_dir(A,B) 0
#else
#define sync_dir(A,B) my_sync(A,B)
#endif

/**
   @file
   @brief Module which writes and reads to a transaction log
*/

/* 0xFF can never be valid first byte of a chunk */
#define TRANSLOG_FILLER 0xFF

/* number of opened log files in the pagecache (should be at least 2) */
#define OPENED_FILES_NUM 3
#define CACHED_FILES_NUM 5
#define CACHED_FILES_NUM_DIRECT_SEARCH_LIMIT 7
#if CACHED_FILES_NUM > CACHED_FILES_NUM_DIRECT_SEARCH_LIMIT
#include <hash.h>
#include <m_ctype.h>
#endif

/* transaction log file descriptor */
typedef struct st_translog_file
{
  uint32 number;
  PAGECACHE_FILE handler;
  my_bool was_recovered;
  my_bool is_sync;
} TRANSLOG_FILE;

/* records buffer size (should be TRANSLOG_PAGE_SIZE * n) */
#define TRANSLOG_WRITE_BUFFER (1024*1024)
/*
  pagecache_read/write/inject() use bmove512() on their buffers so those must
  be long-aligned, which we guarantee by using the type below:
*/
typedef union
{
  ulonglong dummy;
  uchar buffer[TRANSLOG_PAGE_SIZE];
} TRANSLOG_PAGE_SIZE_BUFF;

/* min chunk length */
#define TRANSLOG_MIN_CHUNK 3
/*
  Number of buffers used by loghandler

  Should be at least 4, because one thread can block up to 2 buffers in
  normal circumstances (less then half of one and full other, or just
  switched one and other), But if we met end of the file in the middle and
  have to switch buffer it will be 3.  + 1 buffer for flushing/writing.
  We have a bigger number here for higher concurrency and to make division
  faster.

  The number should be power of 2 to be fast.
*/
#define TRANSLOG_BUFFERS_NO 8
/* number of bytes (+ header) which can be unused on first page in sequence */
#define TRANSLOG_MINCHUNK_CONTENT 1
/* version of log file */
#define TRANSLOG_VERSION_ID 10000               /* 1.00.00 */

#define TRANSLOG_PAGE_FLAGS 6 /* transaction log page flags offset */

/* Maximum length of compressed LSNs (the worst case of whole LSN storing) */
#define COMPRESSED_LSN_MAX_STORE_SIZE (2 + LSN_STORE_SIZE)
#define MAX_NUMBER_OF_LSNS_PER_RECORD 2


/* max lsn calculation for buffer */
#define BUFFER_MAX_LSN(B)  \
  ((B)->last_lsn == LSN_IMPOSSIBLE ? (B)->prev_last_lsn : (B)->last_lsn)

/* log write buffer descriptor */
struct st_translog_buffer
{
  /*
    Cache for current log. Comes first to be aligned for bmove512() in
    pagecache_inject()
  */
  uchar buffer[TRANSLOG_WRITE_BUFFER];
  /*
    Maximum LSN of records which ends in this buffer (or IMPOSSIBLE_LSN
    if no LSNs ends here)
  */
  LSN last_lsn;
  /* last_lsn of previous buffer or IMPOSSIBLE_LSN if it is very first one */
  LSN prev_last_lsn;
  /* This buffer offset in the file */
  TRANSLOG_ADDRESS offset;
  /*
    Next buffer offset in the file (it is not always offset + size,
    in case of flush by LSN it can be offset + size - TRANSLOG_PAGE_SIZE)
  */
  TRANSLOG_ADDRESS next_buffer_offset;
  /* Previous buffer offset to detect it flush finish */
  TRANSLOG_ADDRESS prev_buffer_offset;
  /*
     How much is written (or will be written when copy_to_buffer_in_progress
     become 0) to this buffer
  */
  translog_size_t size;
  /* File handler for this buffer */
  TRANSLOG_FILE *file;
  /* Threads which are waiting for buffer filling/freeing */
  pthread_cond_t waiting_filling_buffer;
  /* Number of records which are in copy progress */
  uint copy_to_buffer_in_progress;
  /* list of waiting buffer ready threads */
  struct st_my_thread_var *waiting_flush;
  /*
    If true then previous buffer overlap with this one (due to flush of
    loghandler, the last page of that buffer is the same as the first page
    of this buffer) and have to be written first (because contain old
    content of page which present in both buffers)
  */
  my_bool overlay;
  uint buffer_no;
  /*
    Lock for the buffer.

    Current buffer also lock the whole handler (if one want lock the handler
    one should lock the current buffer).

    Buffers are locked only in one direction (with overflow and beginning
    from the first buffer). If we keep lock on buffer N we can lock only
    buffer N+1 (never N-1).

    One thread do not lock more then 2 buffer in a time, so to make dead
    lock it should be N thread (where N equal number of buffers) takes one
    buffer and try to lock next. But it is impossible because there is only
    2 cases when thread take 2 buffers: 1) one thread finishes current
    buffer (where horizon is) and start next (to which horizon moves).  2)
    flush start from buffer after current (oldest) and go till the current
    crabbing by buffer sequence. And there is  only one flush in a moment
    (they are serialised).

   Because of above and number of buffers equal 5 we can't get dead lock (it is
   impossible to get all 5 buffers locked simultaneously).
  */
  pthread_mutex_t mutex;
  /*
    Some thread is going to close the buffer and it should be
    done only by that thread
  */
  my_bool is_closing_buffer;
  /*
    Version of the buffer increases every time buffer the buffer flushed.
    With file and offset it allow detect buffer changes
  */
  uint8 ver;

  /*
    When previous buffer sent to disk it set its address here to allow
    to detect when it is done
    (we have to keep it in this buffer to lock buffers only in one direction).
  */
  TRANSLOG_ADDRESS prev_sent_to_disk;
  pthread_cond_t prev_sent_to_disk_cond;
};


struct st_buffer_cursor
{
  /* pointer into the buffer */
  uchar *ptr;
  /* current buffer */
  struct st_translog_buffer *buffer;
  /* How many bytes we wrote on the current page */
  uint16 current_page_fill;
  /*
    How many times we write the page on the disk during flushing process
    (for sector protection).
  */
  uint16 write_counter;
  /* previous write offset */
  uint16 previous_offset;
  /* Number of current buffer */
  uint8 buffer_no;
  /*
    True if it is just filling buffer after advancing the pointer to
    the horizon.
  */
  my_bool chaser;
  /*
    Is current page of the cursor already finished (sector protection
    should be applied if it is needed)
  */
  my_bool protected;
};


typedef uint8 dirty_buffer_mask_t;

struct st_translog_descriptor
{
  /* *** Parameters of the log handler *** */

  /* Page cache for the log reads */
  PAGECACHE *pagecache;
  uint flags;
  /* File open flags */
  uint open_flags;
  /* max size of one log size (for new logs creation) */
  uint32 log_file_max_size;
  uint32 server_version;
  /* server ID (used for replication) */
  uint32 server_id;
  /* Loghandler's buffer capacity in case of chunk 2 filling */
  uint32 buffer_capacity_chunk_2;
  /*
    Half of the buffer capacity in case of chunk 2 filling,
    used to decide will we write a record in one group or many.
    It is written to the variable just to avoid devision every
    time we need it.
  */
  uint32 half_buffer_capacity_chunk_2;
  /* Page overhead calculated by flags (whether CRC is enabled, etc) */
  uint16 page_overhead;
  /*
    Page capacity ("useful load") calculated by flags
    (TRANSLOG_PAGE_SIZE - page_overhead-1)
  */
  uint16 page_capacity_chunk_2;
  /* Path to the directory where we store log store files */
  char directory[FN_REFLEN];

  /* *** Current state of the log handler *** */
  /* list of opened files */
  DYNAMIC_ARRAY open_files;
  /* min/max number of file in the array */
  uint32 max_file, min_file;
  /* the opened files list guard */
  rw_lock_t open_files_lock;

  /*
    File descriptor of the directory where we store log files for syncing
    it.
  */
  File directory_fd;
  /* buffers for log writing */
  struct st_translog_buffer buffers[TRANSLOG_BUFFERS_NO];
  /* Mask where 1 in position N mean that buffer N is not flushed */
  dirty_buffer_mask_t dirty_buffer_mask;
  /* The above variable protection */
  pthread_mutex_t dirty_buffer_mask_lock;
  /*
     horizon - visible end of the log (here is absolute end of the log:
     position where next chunk can start
  */
  TRANSLOG_ADDRESS horizon;
  /* horizon buffer cursor */
  struct st_buffer_cursor bc;
  /* maximum LSN of the current (not finished) file */
  LSN max_lsn;

  /*
    Last flushed LSN (protected by log_flush_lock).
    Pointers in the log ordered like this:
    last_lsn_checked <= flushed <= sent_to_disk <= in_buffers_only <=
    max_lsn <= horizon
  */
  LSN flushed;
  /* Last LSN sent to the disk (but maybe not written yet) */
  LSN sent_to_disk;
  /* Horizon from which log started after initialization */
  TRANSLOG_ADDRESS log_start;
  TRANSLOG_ADDRESS previous_flush_horizon;
  /* All what is after this address is not sent to disk yet */
  TRANSLOG_ADDRESS in_buffers_only;
  /* protection of sent_to_disk and in_buffers_only */
  pthread_mutex_t sent_to_disk_lock;
  /*
    Protect flushed (see above) and for flush serialization (will
    be removed in v1.5
  */
  pthread_mutex_t log_flush_lock;
  pthread_cond_t log_flush_cond;

  /* Protects changing of headers of finished files (max_lsn) */
  pthread_mutex_t file_header_lock;

  /*
    Sorted array (with protection) of files where we started writing process
    and so we can't give last LSN yet
  */
  pthread_mutex_t unfinished_files_lock;
  DYNAMIC_ARRAY unfinished_files;

  /*
    minimum number of still need file calculeted during last
    translog_purge call
  */
  uint32 min_need_file;
  /* Purger data: minimum file in the log (or 0 if unknown) */
  uint32 min_file_number;
  /* Protect purger from many calls and it's data */
  pthread_mutex_t purger_lock;
  /* last low water mark checked */
  LSN last_lsn_checked;
  /**
    Must be set to 0 under loghandler lock every time a new LSN
    is generated.
  */
  my_bool is_everything_flushed;
  /* True when flush pass is in progress */
  my_bool flush_in_progress;
  /* The flush number (used to distinguish two flushes goes one by one) */
  volatile int flush_no;
  /* Next flush pass variables */
  TRANSLOG_ADDRESS next_pass_max_lsn;
  pthread_t max_lsn_requester;
};

static struct st_translog_descriptor log_descriptor;

ulong log_purge_type= TRANSLOG_PURGE_IMMIDIATE;
ulong log_file_size= TRANSLOG_FILE_SIZE;
ulong sync_log_dir= TRANSLOG_SYNC_DIR_NEWFILE;

/* Marker for end of log */
static uchar end_of_log= 0;
#define END_OF_LOG &end_of_log

enum enum_translog_status translog_status= TRANSLOG_UNINITED;

/* chunk types */
#define TRANSLOG_CHUNK_LSN   0x00      /* 0 chunk refer as LSN (head or tail */
#define TRANSLOG_CHUNK_FIXED (1 << 6)  /* 1 (pseudo)fixed record (also LSN) */
#define TRANSLOG_CHUNK_NOHDR (2 << 6)  /* 2 no head chunk (till page end) */
#define TRANSLOG_CHUNK_LNGTH (3 << 6)  /* 3 chunk with chunk length */
#define TRANSLOG_CHUNK_TYPE  (3 << 6)  /* Mask to get chunk type */
#define TRANSLOG_REC_TYPE    0x3F      /* Mask to get record type */
#define TRANSLOG_CHUNK_0_CONT 0x3F     /* the type to mark chunk 0 continue */

/* compressed (relative) LSN constants */
#define TRANSLOG_CLSN_LEN_BITS 0xC0    /* Mask to get compressed LSN length */


#include <my_atomic.h>
/* an array that maps id of a MARIA_SHARE to this MARIA_SHARE */
static MARIA_SHARE **id_to_share= NULL;
/* lock for id_to_share */
static my_atomic_rwlock_t LOCK_id_to_share;

static my_bool translog_dummy_callback(uchar *page,
                                       pgcache_page_no_t page_no,
                                       uchar* data_ptr);
static my_bool translog_page_validator(uchar *page,
                                       pgcache_page_no_t page_no,
                                       uchar* data_ptr);

static my_bool translog_get_next_chunk(TRANSLOG_SCANNER_DATA *scanner);
static uint32 translog_first_file(TRANSLOG_ADDRESS horizon, int is_protected);
LSN translog_next_LSN(TRANSLOG_ADDRESS addr, TRANSLOG_ADDRESS horizon);


/*
  Initialize log_record_type_descriptors
*/

LOG_DESC log_record_type_descriptor[LOGREC_NUMBER_OF_TYPES];


#ifndef DBUG_OFF

#define translog_buffer_lock_assert_owner(B) \
  safe_mutex_assert_owner(&(B)->mutex)
#define translog_lock_assert_owner() \
  safe_mutex_assert_owner(&log_descriptor.bc.buffer->mutex)
void translog_lock_handler_assert_owner()
{
  translog_lock_assert_owner();
}

/**
  @brief check the description table validity

  @param num             how many records should be filled
*/

static void check_translog_description_table(int num)
{
  int i;
  DBUG_ENTER("check_translog_description_table");
  DBUG_PRINT("enter", ("last record: %d", num));
  DBUG_ASSERT(num > 0);
  /* last is reserved for extending the table */
  DBUG_ASSERT(num < LOGREC_NUMBER_OF_TYPES - 1);
  DBUG_ASSERT(log_record_type_descriptor[0].rclass == LOGRECTYPE_NOT_ALLOWED);

  for (i= 0; i <= num; i++)
  {
    DBUG_PRINT("info",
               ("record type: %d  class: %d  fixed: %u  header: %u  LSNs: %u  "
                "name: %s",
                i, log_record_type_descriptor[i].rclass,
                (uint)log_record_type_descriptor[i].fixed_length,
                (uint)log_record_type_descriptor[i].read_header_len,
                (uint)log_record_type_descriptor[i].compressed_LSN,
                log_record_type_descriptor[i].name));
    switch (log_record_type_descriptor[i].rclass) {
    case LOGRECTYPE_NOT_ALLOWED:
      DBUG_ASSERT(i == 0);
      break;
    case LOGRECTYPE_VARIABLE_LENGTH:
      DBUG_ASSERT(log_record_type_descriptor[i].fixed_length == 0);
      DBUG_ASSERT((log_record_type_descriptor[i].compressed_LSN == 0) ||
                  ((log_record_type_descriptor[i].compressed_LSN == 1) &&
                   (log_record_type_descriptor[i].read_header_len >=
                    LSN_STORE_SIZE)) ||
                  ((log_record_type_descriptor[i].compressed_LSN == 2) &&
                   (log_record_type_descriptor[i].read_header_len >=
                    LSN_STORE_SIZE * 2)));
      break;
    case LOGRECTYPE_PSEUDOFIXEDLENGTH:
      DBUG_ASSERT(log_record_type_descriptor[i].fixed_length ==
                  log_record_type_descriptor[i].read_header_len);
      DBUG_ASSERT(log_record_type_descriptor[i].compressed_LSN > 0);
      DBUG_ASSERT(log_record_type_descriptor[i].compressed_LSN <= 2);
      break;
    case LOGRECTYPE_FIXEDLENGTH:
      DBUG_ASSERT(log_record_type_descriptor[i].fixed_length ==
                  log_record_type_descriptor[i].read_header_len);
      DBUG_ASSERT(log_record_type_descriptor[i].compressed_LSN == 0);
      break;
    default:
      DBUG_ASSERT(0);
    }
  }
  for (i= num + 1; i < LOGREC_NUMBER_OF_TYPES; i++)
  {
    DBUG_ASSERT(log_record_type_descriptor[i].rclass ==
                LOGRECTYPE_NOT_ALLOWED);
  }
  DBUG_VOID_RETURN;
}
#else
#define translog_buffer_lock_assert_owner(B) {}
#define translog_lock_assert_owner() {}
#endif

static LOG_DESC INIT_LOGREC_RESERVED_FOR_CHUNKS23=
{LOGRECTYPE_NOT_ALLOWED, 0, 0, NULL, NULL, NULL, 0,
 "reserved", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL };

static LOG_DESC INIT_LOGREC_REDO_INSERT_ROW_HEAD=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE, NULL,
 write_hook_for_redo, NULL, 0,
 "redo_insert_row_head", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_INSERT_ROW_TAIL=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE, NULL,
 write_hook_for_redo, NULL, 0,
 "redo_insert_row_tail", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_NEW_ROW_HEAD=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE, NULL,
 write_hook_for_redo, NULL, 0,
 "redo_new_row_head", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_NEW_ROW_TAIL=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE, NULL,
 write_hook_for_redo, NULL, 0,
 "redo_new_row_tail", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_INSERT_ROW_BLOBS=
{LOGRECTYPE_VARIABLE_LENGTH, 0, FILEID_STORE_SIZE, NULL,
 write_hook_for_redo, NULL, 0,
 "redo_insert_row_blobs", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_PURGE_ROW_HEAD=
{LOGRECTYPE_FIXEDLENGTH,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 NULL, write_hook_for_redo, NULL, 0,
 "redo_purge_row_head", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_PURGE_ROW_TAIL=
{LOGRECTYPE_FIXEDLENGTH,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 NULL, write_hook_for_redo, NULL, 0,
 "redo_purge_row_tail", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_FREE_BLOCKS=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 FILEID_STORE_SIZE + PAGERANGE_STORE_SIZE,
 NULL, write_hook_for_redo, NULL, 0,
 "redo_free_blocks", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_FREE_HEAD_OR_TAIL=
{LOGRECTYPE_FIXEDLENGTH,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE,
 NULL, write_hook_for_redo, NULL, 0,
 "redo_free_head_or_tail", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

/* not yet used; for when we have versioning */
static LOG_DESC INIT_LOGREC_REDO_DELETE_ROW=
{LOGRECTYPE_FIXEDLENGTH, 16, 16, NULL, write_hook_for_redo, NULL, 0,
 "redo_delete_row", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

/** @todo RECOVERY BUG unused, remove? */
static LOG_DESC INIT_LOGREC_REDO_UPDATE_ROW_HEAD=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, write_hook_for_redo, NULL, 0,
 "redo_update_row_head", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_INDEX=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 9, NULL, write_hook_for_redo, NULL, 0,
 "redo_index", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_INDEX_NEW_PAGE=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2 + KEY_NR_STORE_SIZE + 1,
 NULL, write_hook_for_redo, NULL, 0,
 "redo_index_new_page", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_INDEX_FREE_PAGE=
{LOGRECTYPE_FIXEDLENGTH, FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2,
 NULL, write_hook_for_redo, NULL, 0,
 "redo_index_free_page", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_UNDELETE_ROW=
{LOGRECTYPE_FIXEDLENGTH, 16, 16, NULL, write_hook_for_redo, NULL, 0,
 "redo_undelete_row", LOGREC_NOT_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_CLR_END=
{LOGRECTYPE_VARIABLE_LENGTH, 0, LSN_STORE_SIZE + FILEID_STORE_SIZE +
 CLR_TYPE_STORE_SIZE, NULL, write_hook_for_clr_end, NULL, 1,
 "clr_end", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_PURGE_END=
{LOGRECTYPE_PSEUDOFIXEDLENGTH, 5, 5, NULL, NULL, NULL, 1,
 "purge_end", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_ROW_INSERT=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 NULL, write_hook_for_undo_row_insert, NULL, 1,
 "undo_row_insert", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_ROW_DELETE=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 NULL, write_hook_for_undo_row_delete, NULL, 1,
 "undo_row_delete", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_ROW_UPDATE=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + PAGE_STORE_SIZE + DIRPOS_STORE_SIZE,
 NULL, write_hook_for_undo_row_update, NULL, 1,
 "undo_row_update", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_KEY_INSERT=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + KEY_NR_STORE_SIZE,
 NULL, write_hook_for_undo_key_insert, NULL, 1,
 "undo_key_insert", LOGREC_LAST_IN_GROUP, NULL, NULL};

/* This will never be in the log, only in the clr */
static LOG_DESC INIT_LOGREC_UNDO_KEY_INSERT_WITH_ROOT=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + KEY_NR_STORE_SIZE + PAGE_STORE_SIZE,
 NULL, write_hook_for_undo_key, NULL, 1,
 "undo_key_insert_with_root", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_KEY_DELETE=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + KEY_NR_STORE_SIZE,
 NULL, write_hook_for_undo_key_delete, NULL, 1,
 "undo_key_delete", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_KEY_DELETE_WITH_ROOT=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE + KEY_NR_STORE_SIZE + PAGE_STORE_SIZE,
 NULL, write_hook_for_undo_key_delete, NULL, 1,
 "undo_key_delete_with_root", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_PREPARE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0,
 "prepare", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_PREPARE_WITH_UNDO_PURGE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, LSN_STORE_SIZE, NULL, NULL, NULL, 1,
 "prepare_with_undo_purge", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_COMMIT=
{LOGRECTYPE_FIXEDLENGTH, 0, 0, NULL,
 write_hook_for_commit, NULL, 0, "commit", LOGREC_IS_GROUP_ITSELF, NULL,
 NULL};

static LOG_DESC INIT_LOGREC_COMMIT_WITH_UNDO_PURGE=
{LOGRECTYPE_PSEUDOFIXEDLENGTH, 5, 5, NULL, write_hook_for_commit, NULL, 1,
 "commit_with_undo_purge", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_CHECKPOINT=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0,
 "checkpoint", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_CREATE_TABLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 1 + 2, NULL, NULL, NULL, 0,
"redo_create_table", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_RENAME_TABLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0,
 "redo_rename_table", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_DROP_TABLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0,
 "redo_drop_table", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_DELETE_ALL=
{LOGRECTYPE_FIXEDLENGTH, FILEID_STORE_SIZE, FILEID_STORE_SIZE,
 NULL, write_hook_for_redo_delete_all, NULL, 0,
 "redo_delete_all", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_REPAIR_TABLE=
{LOGRECTYPE_FIXEDLENGTH, FILEID_STORE_SIZE + 8 + 8, FILEID_STORE_SIZE + 8 + 8,
 NULL, NULL, NULL, 0,
 "redo_repair_table", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_FILE_ID=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 2, NULL, write_hook_for_file_id, NULL, 0,
 "file_id", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_LONG_TRANSACTION_ID=
{LOGRECTYPE_FIXEDLENGTH, 6, 6, NULL, NULL, NULL, 0,
 "long_transaction_id", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_INCOMPLETE_LOG=
{LOGRECTYPE_FIXEDLENGTH, FILEID_STORE_SIZE, FILEID_STORE_SIZE,
 NULL, NULL, NULL, 0,
 "incomplete_log", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_INCOMPLETE_GROUP=
{LOGRECTYPE_FIXEDLENGTH, 0, 0,
 NULL, NULL, NULL, 0,
 "incomplete_group", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_UNDO_BULK_INSERT=
{LOGRECTYPE_VARIABLE_LENGTH, 0,
 LSN_STORE_SIZE + FILEID_STORE_SIZE,
 NULL, write_hook_for_undo_bulk_insert, NULL, 1,
 "undo_bulk_insert", LOGREC_LAST_IN_GROUP, NULL, NULL};

static LOG_DESC INIT_LOGREC_REDO_BITMAP_NEW_PAGE=
{LOGRECTYPE_FIXEDLENGTH, FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2,
 FILEID_STORE_SIZE + PAGE_STORE_SIZE * 2,
 NULL, NULL, NULL, 0,
 "redo_create_bitmap", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_IMPORTED_TABLE=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0,
 "imported_table", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

static LOG_DESC INIT_LOGREC_DEBUG_INFO=
{LOGRECTYPE_VARIABLE_LENGTH, 0, 0, NULL, NULL, NULL, 0,
 "info", LOGREC_IS_GROUP_ITSELF, NULL, NULL};

const myf log_write_flags= MY_WME | MY_NABP | MY_WAIT_IF_FULL;

void translog_table_init()
{
  int i;
  log_record_type_descriptor[LOGREC_RESERVED_FOR_CHUNKS23]=
    INIT_LOGREC_RESERVED_FOR_CHUNKS23;
  log_record_type_descriptor[LOGREC_REDO_INSERT_ROW_HEAD]=
    INIT_LOGREC_REDO_INSERT_ROW_HEAD;
  log_record_type_descriptor[LOGREC_REDO_INSERT_ROW_TAIL]=
    INIT_LOGREC_REDO_INSERT_ROW_TAIL;
  log_record_type_descriptor[LOGREC_REDO_NEW_ROW_HEAD]=
    INIT_LOGREC_REDO_NEW_ROW_HEAD;
  log_record_type_descriptor[LOGREC_REDO_NEW_ROW_TAIL]=
    INIT_LOGREC_REDO_NEW_ROW_TAIL;
  log_record_type_descriptor[LOGREC_REDO_INSERT_ROW_BLOBS]=
    INIT_LOGREC_REDO_INSERT_ROW_BLOBS;
  log_record_type_descriptor[LOGREC_REDO_PURGE_ROW_HEAD]=
    INIT_LOGREC_REDO_PURGE_ROW_HEAD;
  log_record_type_descriptor[LOGREC_REDO_PURGE_ROW_TAIL]=
    INIT_LOGREC_REDO_PURGE_ROW_TAIL;
  log_record_type_descriptor[LOGREC_REDO_FREE_BLOCKS]=
    INIT_LOGREC_REDO_FREE_BLOCKS;
  log_record_type_descriptor[LOGREC_REDO_FREE_HEAD_OR_TAIL]=
    INIT_LOGREC_REDO_FREE_HEAD_OR_TAIL;
  log_record_type_descriptor[LOGREC_REDO_DELETE_ROW]=
    INIT_LOGREC_REDO_DELETE_ROW;
  log_record_type_descriptor[LOGREC_REDO_UPDATE_ROW_HEAD]=
    INIT_LOGREC_REDO_UPDATE_ROW_HEAD;
  log_record_type_descriptor[LOGREC_REDO_INDEX]=
    INIT_LOGREC_REDO_INDEX;
  log_record_type_descriptor[LOGREC_REDO_INDEX_NEW_PAGE]=
    INIT_LOGREC_REDO_INDEX_NEW_PAGE;
  log_record_type_descriptor[LOGREC_REDO_INDEX_FREE_PAGE]=
    INIT_LOGREC_REDO_INDEX_FREE_PAGE;
  log_record_type_descriptor[LOGREC_REDO_UNDELETE_ROW]=
    INIT_LOGREC_REDO_UNDELETE_ROW;
  log_record_type_descriptor[LOGREC_CLR_END]=
    INIT_LOGREC_CLR_END;
  log_record_type_descriptor[LOGREC_PURGE_END]=
    INIT_LOGREC_PURGE_END;
  log_record_type_descriptor[LOGREC_UNDO_ROW_INSERT]=
    INIT_LOGREC_UNDO_ROW_INSERT;
  log_record_type_descriptor[LOGREC_UNDO_ROW_DELETE]=
    INIT_LOGREC_UNDO_ROW_DELETE;
  log_record_type_descriptor[LOGREC_UNDO_ROW_UPDATE]=
    INIT_LOGREC_UNDO_ROW_UPDATE;
  log_record_type_descriptor[LOGREC_UNDO_KEY_INSERT]=
    INIT_LOGREC_UNDO_KEY_INSERT;
  log_record_type_descriptor[LOGREC_UNDO_KEY_INSERT_WITH_ROOT]=
    INIT_LOGREC_UNDO_KEY_INSERT_WITH_ROOT;
  log_record_type_descriptor[LOGREC_UNDO_KEY_DELETE]=
    INIT_LOGREC_UNDO_KEY_DELETE;
  log_record_type_descriptor[LOGREC_UNDO_KEY_DELETE_WITH_ROOT]=
    INIT_LOGREC_UNDO_KEY_DELETE_WITH_ROOT;
  log_record_type_descriptor[LOGREC_PREPARE]=
    INIT_LOGREC_PREPARE;
  log_record_type_descriptor[LOGREC_PREPARE_WITH_UNDO_PURGE]=
    INIT_LOGREC_PREPARE_WITH_UNDO_PURGE;
  log_record_type_descriptor[LOGREC_COMMIT]=
    INIT_LOGREC_COMMIT;
  log_record_type_descriptor[LOGREC_COMMIT_WITH_UNDO_PURGE]=
    INIT_LOGREC_COMMIT_WITH_UNDO_PURGE;
  log_record_type_descriptor[LOGREC_CHECKPOINT]=
    INIT_LOGREC_CHECKPOINT;
  log_record_type_descriptor[LOGREC_REDO_CREATE_TABLE]=
    INIT_LOGREC_REDO_CREATE_TABLE;
  log_record_type_descriptor[LOGREC_REDO_RENAME_TABLE]=
    INIT_LOGREC_REDO_RENAME_TABLE;
  log_record_type_descriptor[LOGREC_REDO_DROP_TABLE]=
    INIT_LOGREC_REDO_DROP_TABLE;
  log_record_type_descriptor[LOGREC_REDO_DELETE_ALL]=
    INIT_LOGREC_REDO_DELETE_ALL;
  log_record_type_descriptor[LOGREC_REDO_REPAIR_TABLE]=
    INIT_LOGREC_REDO_REPAIR_TABLE;
  log_record_type_descriptor[LOGREC_FILE_ID]=
    INIT_LOGREC_FILE_ID;
  log_record_type_descriptor[LOGREC_LONG_TRANSACTION_ID]=
    INIT_LOGREC_LONG_TRANSACTION_ID;
  log_record_type_descriptor[LOGREC_INCOMPLETE_LOG]=
    INIT_LOGREC_INCOMPLETE_LOG;
  log_record_type_descriptor[LOGREC_INCOMPLETE_GROUP]=
    INIT_LOGREC_INCOMPLETE_GROUP;
  log_record_type_descriptor[LOGREC_UNDO_BULK_INSERT]=
    INIT_LOGREC_UNDO_BULK_INSERT;
  log_record_type_descriptor[LOGREC_REDO_BITMAP_NEW_PAGE]=
    INIT_LOGREC_REDO_BITMAP_NEW_PAGE;
  log_record_type_descriptor[LOGREC_IMPORTED_TABLE]=
    INIT_LOGREC_IMPORTED_TABLE;
  log_record_type_descriptor[LOGREC_DEBUG_INFO]=
    INIT_LOGREC_DEBUG_INFO;

  for (i= LOGREC_FIRST_FREE; i < LOGREC_NUMBER_OF_TYPES; i++)
    log_record_type_descriptor[i].rclass= LOGRECTYPE_NOT_ALLOWED;
#ifndef DBUG_OFF
  check_translog_description_table(LOGREC_FIRST_FREE -1);
#endif
}


/* all possible flags page overheads */
static uint page_overhead[TRANSLOG_FLAGS_NUM];

typedef struct st_translog_validator_data
{
  TRANSLOG_ADDRESS *addr;
  my_bool was_recovered;
} TRANSLOG_VALIDATOR_DATA;


/*
  Check cursor/buffer consistence

  SYNOPSIS
    translog_check_cursor
    cursor               cursor which will be checked
*/

static void translog_check_cursor(struct st_buffer_cursor *cursor
                                 __attribute__((unused)))
{
  DBUG_ASSERT(cursor->chaser ||
              ((ulong) (cursor->ptr - cursor->buffer->buffer) ==
               cursor->buffer->size));
  DBUG_ASSERT(cursor->buffer->buffer_no == cursor->buffer_no);
  DBUG_ASSERT((cursor->ptr -cursor->buffer->buffer) %TRANSLOG_PAGE_SIZE ==
              cursor->current_page_fill % TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);
}


/**
  @brief switch the loghandler in read only mode in case of write error
*/

void translog_stop_writing()
{
  DBUG_ENTER("translog_stop_writing");
  DBUG_PRINT("error", ("errno: %d   my_errno: %d", errno, my_errno));
  translog_status= (translog_status == TRANSLOG_SHUTDOWN ?
                    TRANSLOG_UNINITED :
                    TRANSLOG_READONLY);
  log_descriptor.is_everything_flushed= 1;
  log_descriptor.open_flags= O_BINARY | O_RDONLY;
  DBUG_ASSERT(0);
  DBUG_VOID_RETURN;
}


/*
  @brief Get file name of the log by log number

  @param file_no         Number of the log we want to open
  @param path            Pointer to buffer where file name will be
                         stored (must be FN_REFLEN bytes at least)

  @return pointer to path
*/

char *translog_filename_by_fileno(uint32 file_no, char *path)
{
  char buff[11], *end;
  uint length;
  DBUG_ENTER("translog_filename_by_fileno");
  DBUG_ASSERT(file_no <= 0xfffffff);

  /* log_descriptor.directory is already formated */
  end= strxmov(path, log_descriptor.directory, "maria_log.0000000", NullS);
  length= (uint) (int10_to_str(file_no, buff, 10) - buff);
  strmov(end - length +1, buff);

  DBUG_PRINT("info", ("Path: '%s'  path: 0x%lx", path, (ulong) path));
  DBUG_RETURN(path);
}


/**
  @brief Create log file with given number without cache

  @param file_no         Number of the log we want to open

  retval -1  error
  retval # file descriptor number
*/

static File create_logfile_by_number_no_cache(uint32 file_no)
{
  File file;
  char path[FN_REFLEN];
  DBUG_ENTER("create_logfile_by_number_no_cache");

  if (translog_status != TRANSLOG_OK)
     DBUG_RETURN(-1);

  /* TODO: add O_DIRECT to open flags (when buffer is aligned) */
  if ((file= my_create(translog_filename_by_fileno(file_no, path),
                       0, O_BINARY | O_RDWR, MYF(MY_WME))) < 0)
  {
    DBUG_PRINT("error", ("Error %d during creating file '%s'", errno, path));
    translog_stop_writing();
    DBUG_RETURN(-1);
  }
  if (sync_log_dir >= TRANSLOG_SYNC_DIR_NEWFILE &&
      sync_dir(log_descriptor.directory_fd, MYF(MY_WME | MY_IGNORE_BADFD)))
  {
    DBUG_PRINT("error", ("Error %d during syncing directory '%s'",
                         errno, log_descriptor.directory));
    translog_stop_writing();
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("File: '%s'  handler: %d", path, file));
  DBUG_RETURN(file);
}

/**
  @brief Open (not create) log file with given number without cache

  @param file_no         Number of the log we want to open

  retval -1  error
  retval # file descriptor number
*/

static File open_logfile_by_number_no_cache(uint32 file_no)
{
  File file;
  char path[FN_REFLEN];
  DBUG_ENTER("open_logfile_by_number_no_cache");

  /* TODO: add O_DIRECT to open flags (when buffer is aligned) */
  /* TODO: use my_create() */
  if ((file= my_open(translog_filename_by_fileno(file_no, path),
                     log_descriptor.open_flags,
                     MYF(MY_WME))) < 0)
  {
    DBUG_PRINT("error", ("Error %d during opening file '%s'", errno, path));
    DBUG_RETURN(-1);
  }
  DBUG_PRINT("info", ("File: '%s'  handler: %d", path, file));
  DBUG_RETURN(file);
}


/**
  @brief get file descriptor by given number using cache

  @param file_no         Number of the log we want to open

  retval # file descriptor
  retval NULL file is not opened
*/

static TRANSLOG_FILE *get_logfile_by_number(uint32 file_no)
{
  TRANSLOG_FILE *file;
  DBUG_ENTER("get_logfile_by_number");
  rw_rdlock(&log_descriptor.open_files_lock);
  if (log_descriptor.max_file - file_no >=
      log_descriptor.open_files.elements)
  {
    DBUG_PRINT("info", ("File #%u is not opened", file_no));
    rw_unlock(&log_descriptor.open_files_lock);
    DBUG_RETURN(NULL);
  }
  DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
              log_descriptor.open_files.elements);
  DBUG_ASSERT(log_descriptor.max_file >= file_no);
  DBUG_ASSERT(log_descriptor.min_file <= file_no);

  file= *dynamic_element(&log_descriptor.open_files,
                         log_descriptor.max_file - file_no, TRANSLOG_FILE **);
  rw_unlock(&log_descriptor.open_files_lock);
  DBUG_PRINT("info", ("File 0x%lx File no: %lu, File handler: %d",
                      (ulong)file, (ulong)file_no,
                      (file ? file->handler.file : -1)));
  DBUG_ASSERT(!file || file->number == file_no);
  DBUG_RETURN(file);
}


/**
  @brief get current file descriptor

  retval # file descriptor
*/

static TRANSLOG_FILE *get_current_logfile()
{
  TRANSLOG_FILE *file;
  rw_rdlock(&log_descriptor.open_files_lock);
  DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
              log_descriptor.open_files.elements);
  file= *dynamic_element(&log_descriptor.open_files, 0, TRANSLOG_FILE **);
  rw_unlock(&log_descriptor.open_files_lock);
  return (file);
}

uchar	NEAR maria_trans_file_magic[]=
{ (uchar) 254, (uchar) 254, (uchar) 11, '\001', 'M', 'A', 'R', 'I', 'A',
 'L', 'O', 'G' };
#define LOG_HEADER_DATA_SIZE (sizeof(maria_trans_file_magic) + \
                              8 + 4 + 4 + 4 + 2 + 3 + \
                              LSN_STORE_SIZE)


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

static my_bool translog_write_file_header()
{
  TRANSLOG_FILE *file;
  ulonglong timestamp;
  uchar page_buff[TRANSLOG_PAGE_SIZE], *page= page_buff;
  my_bool rc;
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
  /* loghandler page_size */
  int2store(page, TRANSLOG_PAGE_SIZE - 1);
  page+= 2;
  /* file number */
  int3store(page, LSN_FILE_NO(log_descriptor.horizon));
  page+= 3;
  lsn_store(page, LSN_IMPOSSIBLE);
  page+= LSN_STORE_SIZE;
  memset(page, TRANSLOG_FILLER, sizeof(page_buff) - (page- page_buff));

  file= get_current_logfile();
  rc= my_pwrite(file->handler.file, page_buff, sizeof(page_buff), 0,
                log_write_flags) != 0;
  /*
    Dropping the flag in such way can make false alarm: signalling than the
    file in not sync when it is sync, but the situation is quite rare and
    protections with mutexes give much more overhead to the whole engine
  */
  file->is_sync= 0;
  DBUG_RETURN(rc);
}

/*
  @brief write the new LSN on the given file header

  @param file            The file descriptor
  @param lsn             That LSN which should be written

  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_max_lsn_to_header(File file, LSN lsn)
{
  uchar lsn_buff[LSN_STORE_SIZE];
  DBUG_ENTER("translog_max_lsn_to_header");
  DBUG_PRINT("enter", ("File descriptor: %ld  "
                       "lsn: (%lu,0x%lx)",
                       (long) file,
                       LSN_IN_PARTS(lsn)));

  lsn_store(lsn_buff, lsn);

  DBUG_RETURN(my_pwrite(file, lsn_buff,
                        LSN_STORE_SIZE,
                        (LOG_HEADER_DATA_SIZE - LSN_STORE_SIZE),
                        log_write_flags) != 0 ||
              my_sync(file, MYF(MY_WME)) != 0);
}


/*
  @brief Extract hander file information from loghandler file page

  @param desc header information descriptor to be filled with information
  @param page_buff buffer with the page content
*/

void translog_interpret_file_header(LOGHANDLER_FILE_INFO *desc,
                                    uchar *page_buff)
{
  uchar *ptr;

  ptr= page_buff + sizeof(maria_trans_file_magic);
  desc->timestamp= uint8korr(ptr);
  ptr+= 8;
  desc->maria_version= uint4korr(ptr);
  ptr+= 4;
  desc->mysql_version= uint4korr(ptr);
  ptr+= 4;
  desc->server_id= uint4korr(ptr + 4);
  ptr+= 4;
  desc->page_size= uint2korr(ptr) + 1;
  ptr+= 2;
  desc->file_number= uint3korr(ptr);
  ptr+=3;
  desc->max_lsn= lsn_korr(ptr);
}


/*
  @brief Read hander file information from loghandler file

  @param desc header information descriptor to be filled with information
  @param file file descriptor to read

  @retval 0 OK
  @retval 1 Error
*/

my_bool translog_read_file_header(LOGHANDLER_FILE_INFO *desc, File file)
{
  uchar page_buff[LOG_HEADER_DATA_SIZE];
  DBUG_ENTER("translog_read_file_header");

  if (my_pread(file, page_buff,
               sizeof(page_buff), 0, MYF(MY_FNABP | MY_WME)))
  {
    DBUG_PRINT("info", ("log read fail error: %d", my_errno));
    DBUG_RETURN(1);
  }
  translog_interpret_file_header(desc, page_buff);
  DBUG_PRINT("info", ("timestamp: %llu  maria ver: %lu mysql ver: %lu  "
                      "server id %lu page size %lu file number %lu  "
                      "max lsn: (%lu,0x%lx)",
                      (ulonglong) desc->timestamp,
                      (ulong) desc->maria_version,
                      (ulong) desc->mysql_version,
                      (ulong) desc->server_id,
                      desc->page_size, (ulong) desc->file_number,
                      LSN_IN_PARTS(desc->max_lsn)));
  DBUG_RETURN(0);
}


/*
  @brief set the lsn to the files from_file - to_file if it is greater
  then written in the file

  @param from_file       first file number (min)
  @param to_file         last file number (max)
  @param lsn             the lsn for writing
  @param is_locked       true if current thread locked the log handler

  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_set_lsn_for_files(uint32 from_file, uint32 to_file,
                                          LSN lsn, my_bool is_locked)
{
  uint32 file;
  DBUG_ENTER("translog_set_lsn_for_files");
  DBUG_PRINT("enter", ("From: %lu  to: %lu  lsn: (%lu,0x%lx)  locked: %d",
                       (ulong) from_file, (ulong) to_file,
                       LSN_IN_PARTS(lsn),
                       is_locked));
  DBUG_ASSERT(from_file <= to_file);
  DBUG_ASSERT(from_file > 0); /* we have not file 0 */

  /* Checks the current file (not finished yet file) */
  if (!is_locked)
    translog_lock();
  if (to_file == (uint32) LSN_FILE_NO(log_descriptor.horizon))
  {
    if (likely(cmp_translog_addr(lsn, log_descriptor.max_lsn) > 0))
      log_descriptor.max_lsn= lsn;
    to_file--;
  }
  if (!is_locked)
    translog_unlock();

  /* Checks finished files if they are */
  pthread_mutex_lock(&log_descriptor.file_header_lock);
  for (file= from_file; file <= to_file; file++)
  {
    LOGHANDLER_FILE_INFO info;
    File fd;
    LINT_INIT(info.max_lsn);

    fd= open_logfile_by_number_no_cache(file);
    if ((fd < 0) ||
        ((translog_read_file_header(&info, fd) ||
          (cmp_translog_addr(lsn, info.max_lsn) > 0 &&
           translog_max_lsn_to_header(fd, lsn))) |
          my_close(fd, MYF(MY_WME))))
    {
      translog_stop_writing();
      DBUG_RETURN(1);
    }
  }
  pthread_mutex_unlock(&log_descriptor.file_header_lock);

  DBUG_RETURN(0);
}


/* descriptor of file in unfinished_files */
struct st_file_counter
{
  uint32 file;            /* file number */
  uint32 counter;         /* counter for started writes */
};


/*
  @brief mark file "in progress" (for multi-group records)

  @param file            log file number
*/

static void translog_mark_file_unfinished(uint32 file)
{
  int place, i;
  struct st_file_counter fc, *fc_ptr;

  DBUG_ENTER("translog_mark_file_unfinished");
  DBUG_PRINT("enter", ("file: %lu", (ulong) file));

  fc.file= file; fc.counter= 1;
  pthread_mutex_lock(&log_descriptor.unfinished_files_lock);

  if (log_descriptor.unfinished_files.elements == 0)
  {
    insert_dynamic(&log_descriptor.unfinished_files, (uchar*) &fc);
    DBUG_PRINT("info", ("The first element inserted"));
    goto end;
  }

  for (place= log_descriptor.unfinished_files.elements - 1;
       place >= 0;
       place--)
  {
    fc_ptr= dynamic_element(&log_descriptor.unfinished_files,
                            place, struct st_file_counter *);
    if (fc_ptr->file <= file)
      break;
  }

  if (place >= 0 && fc_ptr->file == file)
  {
     fc_ptr->counter++;
     DBUG_PRINT("info", ("counter increased"));
     goto end;
  }

  if (place == (int)log_descriptor.unfinished_files.elements)
  {
    insert_dynamic(&log_descriptor.unfinished_files, (uchar*) &fc);
    DBUG_PRINT("info", ("The last element inserted"));
    goto end;
  }
  /* shift and assign new element */
  insert_dynamic(&log_descriptor.unfinished_files,
                 (uchar*)
                 dynamic_element(&log_descriptor.unfinished_files,
                                 log_descriptor.unfinished_files.elements- 1,
                                 struct st_file_counter *));
  for(i= log_descriptor.unfinished_files.elements - 1; i > place; i--)
  {
    /* we do not use set_dynamic() to avoid unneeded checks */
    memcpy(dynamic_element(&log_descriptor.unfinished_files,
                           i, struct st_file_counter *),
           dynamic_element(&log_descriptor.unfinished_files,
                           i + 1, struct st_file_counter *),
           sizeof(struct st_file_counter));
  }
  memcpy(dynamic_element(&log_descriptor.unfinished_files,
                         place + 1, struct st_file_counter *),
         &fc, sizeof(struct st_file_counter));
end:
  pthread_mutex_unlock(&log_descriptor.unfinished_files_lock);
  DBUG_VOID_RETURN;
}


/*
  @brief remove file mark "in progress" (for multi-group records)

  @param file            log file number
*/

static void translog_mark_file_finished(uint32 file)
{
  int i;
  struct st_file_counter *fc_ptr;
  DBUG_ENTER("translog_mark_file_finished");
  DBUG_PRINT("enter", ("file: %lu", (ulong) file));

  LINT_INIT(fc_ptr);

  pthread_mutex_lock(&log_descriptor.unfinished_files_lock);

  DBUG_ASSERT(log_descriptor.unfinished_files.elements > 0);
  for (i= 0;
       i < (int) log_descriptor.unfinished_files.elements;
       i++)
  {
    fc_ptr= dynamic_element(&log_descriptor.unfinished_files,
                            i, struct st_file_counter *);
    if (fc_ptr->file == file)
    {
      break;
    }
  }
  DBUG_ASSERT(i < (int) log_descriptor.unfinished_files.elements);

  if (! --fc_ptr->counter)
    delete_dynamic_element(&log_descriptor.unfinished_files, i);
  pthread_mutex_unlock(&log_descriptor.unfinished_files_lock);
  DBUG_VOID_RETURN;
}


/*
  @brief get max LSN of the record which parts stored in this file

  @param file            file number

  @return requested LSN or LSN_IMPOSSIBLE/LSN_ERROR
    @retval LSN_IMPOSSIBLE File is still not finished
    @retval LSN_ERROR Error opening file
    @retval # LSN of the record which parts stored in this file
*/

LSN translog_get_file_max_lsn_stored(uint32 file)
{
  uint32 limit= FILENO_IMPOSSIBLE;
  DBUG_ENTER("translog_get_file_max_lsn_stored");
  DBUG_PRINT("enter", ("file: %lu", (ulong)file));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  pthread_mutex_lock(&log_descriptor.unfinished_files_lock);

  /* find file with minimum file number "in progress" */
  if (log_descriptor.unfinished_files.elements > 0)
  {
    struct st_file_counter *fc_ptr;
    fc_ptr= dynamic_element(&log_descriptor.unfinished_files,
                            0, struct st_file_counter *);
    limit= fc_ptr->file; /* minimal file number "in progress" */
  }
  pthread_mutex_unlock(&log_descriptor.unfinished_files_lock);

  /*
    if there is no "in progress file" then unfinished file is in progress
    for sure
  */
  if (limit == FILENO_IMPOSSIBLE)
  {
    TRANSLOG_ADDRESS horizon= translog_get_horizon();
    limit= LSN_FILE_NO(horizon);
  }

  if (file >= limit)
  {
    DBUG_PRINT("info", ("The file in in progress"));
    DBUG_RETURN(LSN_IMPOSSIBLE);
  }

  {
    LOGHANDLER_FILE_INFO info;
	File fd;
    LINT_INIT_STRUCT(info);
    fd= open_logfile_by_number_no_cache(file);
    if ((fd < 0) ||
        (translog_read_file_header(&info, fd) | my_close(fd, MYF(MY_WME))))
    {
      DBUG_PRINT("error", ("Can't read file header"));
      DBUG_RETURN(LSN_ERROR);
    }
    DBUG_PRINT("info", ("Max lsn: (%lu,0x%lx)",
                         LSN_IN_PARTS(info.max_lsn)));
    DBUG_RETURN(info.max_lsn);
  }
}

/*
  Initialize transaction log file buffer

  SYNOPSIS
    translog_buffer_init()
    buffer               The buffer to initialize
    num                  Number of this buffer

  RETURN
    0  OK
    1  Error
*/

static my_bool translog_buffer_init(struct st_translog_buffer *buffer, int num)
{
  DBUG_ENTER("translog_buffer_init");
  buffer->prev_last_lsn= buffer->last_lsn= LSN_IMPOSSIBLE;
  DBUG_PRINT("info", ("last_lsn  and prev_last_lsn set to 0  buffer: 0x%lx",
                      (ulong) buffer));

  buffer->buffer_no= (uint8) num;
  /* This Buffer File */
  buffer->file= NULL;
  buffer->overlay= 0;
  /* cache for current log */
  memset(buffer->buffer, TRANSLOG_FILLER, TRANSLOG_WRITE_BUFFER);
  /* Buffer size */
  buffer->size= 0;
  /* cond of thread which is waiting for buffer filling */
  if (pthread_cond_init(&buffer->waiting_filling_buffer, 0))
    DBUG_RETURN(1);
  /* Number of records which are in copy progress */
  buffer->copy_to_buffer_in_progress= 0;
  /* list of waiting buffer ready threads */
  buffer->waiting_flush= 0;
  /*
    Buffers locked by fallowing mutex. As far as buffers create logical
    circle (after last buffer goes first) it trigger false alarm of deadlock
    detect system, so we remove check of deadlock for this buffers. In deed
    all mutex locks concentrated around current buffer except flushing
    thread (but it is only one thread). One thread can't take more then
    2 buffer locks at once. So deadlock is impossible here.

    To prevent false alarm of dead lock detection we switch dead lock
    detection for one buffer in the middle of the buffers chain. Excluding
    only one of eight buffers from deadlock detection hardly can hide other
    possible problems which include this mutexes.
  */
  if (my_pthread_mutex_init(&buffer->mutex, MY_MUTEX_INIT_FAST,
                            "translog_buffer->mutex",
                            (num == TRANSLOG_BUFFERS_NO - 2 ?
                             MYF_NO_DEADLOCK_DETECTION : 0)) ||
      pthread_cond_init(&buffer->prev_sent_to_disk_cond, 0))
    DBUG_RETURN(1);
  buffer->is_closing_buffer= 0;
  buffer->prev_sent_to_disk= LSN_IMPOSSIBLE;
  buffer->prev_buffer_offset= LSN_IMPOSSIBLE;
  buffer->ver= 0;
  DBUG_RETURN(0);
}


/*
  @brief close transaction log file by descriptor

  @param file            pagegecache file descriptor reference

  @return Operation status
    @retval 0  OK
    @retval 1  Error
*/

static my_bool translog_close_log_file(TRANSLOG_FILE *file)
{
  int rc= 0;
  flush_pagecache_blocks(log_descriptor.pagecache, &file->handler,
                         FLUSH_RELEASE);
  /*
    Sync file when we close it
    TODO: sync only we have changed the log
  */
  if (!file->is_sync)
    rc= my_sync(file->handler.file, MYF(MY_WME));
  rc|= my_close(file->handler.file, MYF(MY_WME));
  my_free(file, MYF(0));
  return test(rc);
}


/**
  @brief Dummy function for write failure (the log to not use
  pagecache writing)
*/

void translog_dummy_write_failure(uchar *data __attribute__((unused)))
{
  return;
}


/**
  @brief Initializes TRANSLOG_FILE structure

  @param file            reference on the file to initialize
  @param number          file number
  @param is_sync         is file synced on disk
*/

static void translog_file_init(TRANSLOG_FILE *file, uint32 number,
                               my_bool is_sync)
{
  pagecache_file_init(file->handler, &translog_page_validator,
                      &translog_dummy_callback,
                      &translog_dummy_write_failure,
                      maria_flush_log_for_page_none, file);
  file->number= number;
  file->was_recovered= 0;
  file->is_sync= is_sync;
}


/**
  @brief Create and fill header of new file.

  @note the caller must call it right after it has increased
   log_descriptor.horizon to the new file
   (log_descriptor.horizon+= LSN_ONE_FILE)


  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_create_new_file()
{
  TRANSLOG_FILE *file= (TRANSLOG_FILE*)my_malloc(sizeof(TRANSLOG_FILE),
                                                 MYF(0));

  TRANSLOG_FILE *old= get_current_logfile();
  uint32 file_no= LSN_FILE_NO(log_descriptor.horizon);
  DBUG_ENTER("translog_create_new_file");

  if (file == NULL)
    goto error;

  /*
    Writes max_lsn to the file header before finishing it (there is no need
    to lock file header buffer because it is still unfinished file, so only
    one thread can finish the file and nobody interested of LSN of current
    (unfinished) file, because no one can purge it).
  */
  if (translog_max_lsn_to_header(old->handler.file, log_descriptor.max_lsn))
    goto error;

  rw_wrlock(&log_descriptor.open_files_lock);
  DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
              log_descriptor.open_files.elements);
  DBUG_ASSERT(file_no == log_descriptor.max_file + 1);
  if (allocate_dynamic(&log_descriptor.open_files,
                       log_descriptor.max_file - log_descriptor.min_file + 2))
    goto error_lock;
  if ((file->handler.file=
       create_logfile_by_number_no_cache(file_no)) == -1)
    goto error_lock;
  translog_file_init(file, file_no, 0);

  /* this call just expand the array */
  insert_dynamic(&log_descriptor.open_files, (uchar*)&file);
  log_descriptor.max_file++;
  {
    char *start= (char*) dynamic_element(&log_descriptor.open_files, 0,
                                         TRANSLOG_FILE**);
    memmove(start + sizeof(TRANSLOG_FILE*), start,
            sizeof(TRANSLOG_FILE*) *
            (log_descriptor.max_file - log_descriptor.min_file + 1 - 1));
  }
  /* can't fail we because we expanded array */
  set_dynamic(&log_descriptor.open_files, (uchar*)&file, 0);
  DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
              log_descriptor.open_files.elements);
  rw_unlock(&log_descriptor.open_files_lock);

  DBUG_PRINT("info", ("file_no: %lu", (ulong)file_no));

  if (translog_write_file_header())
    DBUG_RETURN(1);

  if (ma_control_file_write_and_force(last_checkpoint_lsn, file_no,
                                      max_trid_in_control_file,
                                      recovery_failures))
  {
    translog_stop_writing();
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);

error_lock:
  rw_unlock(&log_descriptor.open_files_lock);
error:
  translog_stop_writing();
  DBUG_RETURN(1);
}


/**
  @brief Locks the loghandler buffer.

  @param buffer          This buffer which should be locked

  @note See comment before buffer 'mutex' variable.

  @retval 0 OK
  @retval 1 Error
*/

static void translog_buffer_lock(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_lock");
  DBUG_PRINT("enter",
             ("Lock buffer #%u: (0x%lx)", (uint) buffer->buffer_no,
              (ulong) buffer));
  pthread_mutex_lock(&buffer->mutex);
  DBUG_VOID_RETURN;
}


/*
  Unlock the loghandler buffer

  SYNOPSIS
    translog_buffer_unlock()
    buffer               This buffer which should be unlocked

  RETURN
    0  OK
    1  Error
*/

static void translog_buffer_unlock(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_unlock");
  DBUG_PRINT("enter", ("Unlock buffer... #%u (0x%lx)",
                       (uint) buffer->buffer_no, (ulong) buffer));

  pthread_mutex_unlock(&buffer->mutex);
  DBUG_VOID_RETURN;
}


/*
  Write a header on the page

  SYNOPSIS
    translog_new_page_header()
    horizon              Where to write the page
    cursor               Where to write the page

  NOTE
    - space for page header should be checked before
*/

static uchar translog_sector_random;

static void translog_new_page_header(TRANSLOG_ADDRESS *horizon,
                                     struct st_buffer_cursor *cursor)
{
  uchar *ptr;

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
  DBUG_ASSERT(TRANSLOG_PAGE_FLAGS == (ptr - cursor->ptr));
  cursor->ptr[TRANSLOG_PAGE_FLAGS]= (uchar) log_descriptor.flags;
  ptr++;
  if (log_descriptor.flags & TRANSLOG_PAGE_CRC)
  {
#ifndef DBUG_OFF
    DBUG_PRINT("info", ("write  0x11223344 CRC to (%lu,0x%lx)",
                        LSN_IN_PARTS(*horizon)));
    /* This will be overwritten by real CRC; This is just for debugging */
    int4store(ptr, 0x11223344);
#endif
    /* CRC will be put when page is finished */
    ptr+= CRC_SIZE;
  }
  if (log_descriptor.flags & TRANSLOG_SECTOR_PROTECTION)
  {
    /*
      translog_sector_randmo works like "random" values producer because
      it is enough to have such "random" for this purpose and it will
      not interfere with higher level pseudo random value generator
    */
    ptr[0]= translog_sector_random++;
    ptr+= TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
  }
  {
    uint len= (ptr - cursor->ptr);
    (*horizon)+= len; /* increasing the offset part of the address */
    cursor->current_page_fill= len;
    if (!cursor->chaser)
      cursor->buffer->size+= len;
  }
  cursor->ptr= ptr;
  DBUG_PRINT("info", ("NewP buffer #%u: 0x%lx  chaser: %d  Size: %lu (%lu)  "
                      "Horizon: (%lu,0x%lx)",
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer),
                      LSN_IN_PARTS(*horizon)));
  translog_check_cursor(cursor);
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

static void translog_put_sector_protection(uchar *page,
                                           struct st_buffer_cursor *cursor)
{
  uchar *table= page + log_descriptor.page_overhead -
    TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
  uint i, offset;
  uint16 last_protected_sector= ((cursor->previous_offset - 1) /
                                 DISK_DRIVE_SECTOR_SIZE);
  uint16 start_sector= cursor->previous_offset / DISK_DRIVE_SECTOR_SIZE;
  uint8 value= table[0] + cursor->write_counter;
  DBUG_ENTER("translog_put_sector_protection");

  if (start_sector == 0)
  {
    /* First sector is protected by file & page numbers in the page header. */
    start_sector= 1;
  }

  DBUG_PRINT("enter", ("Write counter:%u  value:%u  offset:%u, "
                       "last protected:%u  start sector:%u",
                       (uint) cursor->write_counter,
                       (uint) value,
                       (uint) cursor->previous_offset,
                       (uint) last_protected_sector, (uint) start_sector));
  if (last_protected_sector == start_sector)
  {
    i= last_protected_sector;
    offset= last_protected_sector * DISK_DRIVE_SECTOR_SIZE;
    /* restore data, because we modified sector which was protected */
    if (offset < cursor->previous_offset)
      page[offset]= table[i];
  }
  for (i= start_sector, offset= start_sector * DISK_DRIVE_SECTOR_SIZE;
       i < TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
       i++, (offset+= DISK_DRIVE_SECTOR_SIZE))
  {
    DBUG_PRINT("info", ("sector:%u  offset:%u  data 0x%x",
                        i, offset, (uint) page[offset]));
    table[i]= page[offset];
    page[offset]= value;
    DBUG_PRINT("info", ("sector:%u  offset:%u  data 0x%x",
                        i, offset, (uint) page[offset]));
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

static uint32 translog_crc(uchar *area, uint length)
{
  DBUG_ENTER("translog_crc");
  DBUG_RETURN(crc32(0L, (unsigned char*) area, length));
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
  uchar *page= cursor->ptr - cursor->current_page_fill;
  DBUG_ENTER("translog_finish_page");
  DBUG_PRINT("enter", ("Buffer: #%u 0x%lx  "
                       "Buffer addr: (%lu,0x%lx)  "
                       "Page addr: (%lu,0x%lx)  "
                       "size:%lu (%lu)  Pg:%u  left:%u",
                       (uint) cursor->buffer_no, (ulong) cursor->buffer,
                       LSN_IN_PARTS(cursor->buffer->offset),
                       (ulong) LSN_FILE_NO(*horizon),
                       (ulong) (LSN_OFFSET(*horizon) -
                                cursor->current_page_fill),
                       (ulong) cursor->buffer->size,
                       (ulong) (cursor->ptr -cursor->buffer->buffer),
                       (uint) cursor->current_page_fill, (uint) left));
  DBUG_ASSERT(LSN_FILE_NO(*horizon) == LSN_FILE_NO(cursor->buffer->offset));
  translog_check_cursor(cursor);
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
    memset(cursor->ptr, TRANSLOG_FILLER, left);
    cursor->ptr+= left;
    (*horizon)+= left; /* offset increasing */
    if (!cursor->chaser)
      cursor->buffer->size+= left;
    /* We are finishing the page so reset the counter */
    cursor->current_page_fill= 0;
    DBUG_PRINT("info", ("Finish Page buffer #%u: 0x%lx  "
                        "chaser: %d  Size: %lu (%lu)",
                        (uint) cursor->buffer->buffer_no,
                        (ulong) cursor->buffer, cursor->chaser,
                        (ulong) cursor->buffer->size,
                        (ulong) (cursor->ptr - cursor->buffer->buffer)));
    translog_check_cursor(cursor);
  }
  /*
    When we are finishing the page other thread might not finish the page
    header yet (in case if we started from the middle of the page) so we
    have to read log_descriptor.flags but not the flags from the page.
  */
  if (log_descriptor.flags & TRANSLOG_SECTOR_PROTECTION)
  {
    translog_put_sector_protection(page, cursor);
    DBUG_PRINT("info", ("drop write_counter"));
    cursor->write_counter= 0;
    cursor->previous_offset= 0;
  }
  if (log_descriptor.flags & TRANSLOG_PAGE_CRC)
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
  @brief Wait until all threads have finished closing this buffer.

  @param buffer          This buffer should be check
*/

static void translog_wait_for_closing(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_wait_for_closing");
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  copies in progress: %u  "
                       "is closing %u  File: %d  size: %lu",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (uint) buffer->copy_to_buffer_in_progress,
                       (uint) buffer->is_closing_buffer,
                       (buffer->file ? buffer->file->handler.file : -1),
                       (ulong) buffer->size));
  translog_buffer_lock_assert_owner(buffer);

  while (buffer->is_closing_buffer)
  {
    DBUG_PRINT("info", ("wait for writers... buffer: #%u  0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer));
    DBUG_ASSERT(buffer->file != NULL);
    pthread_cond_wait(&buffer->waiting_filling_buffer, &buffer->mutex);
    DBUG_PRINT("info", ("wait for writers done buffer: #%u  0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer));
  }

  DBUG_VOID_RETURN;
}


/*
  @brief Wait until all threads have finished filling this buffer.

  @param buffer          This buffer should be check
*/

static void translog_wait_for_writers(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_wait_for_writers");
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  copies in progress: %u  "
                       "is closing %u  File: %d  size: %lu",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (uint) buffer->copy_to_buffer_in_progress,
                       (uint) buffer->is_closing_buffer,
                       (buffer->file ? buffer->file->handler.file : -1),
                       (ulong) buffer->size));
  translog_buffer_lock_assert_owner(buffer);

  while (buffer->copy_to_buffer_in_progress)
  {
    DBUG_PRINT("info", ("wait for writers... buffer: #%u  0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer));
    DBUG_ASSERT(buffer->file != NULL);
    pthread_cond_wait(&buffer->waiting_filling_buffer, &buffer->mutex);
    DBUG_PRINT("info", ("wait for writers done buffer: #%u  0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer));
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
  TRANSLOG_ADDRESS offset= buffer->offset;
  TRANSLOG_FILE *file= buffer->file;
  uint8 ver= buffer->ver;
  DBUG_ENTER("translog_wait_for_buffer_free");
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  copies in progress: %u  "
                       "is closing %u  File: %d  size: %lu",
                       (uint) buffer->buffer_no, (ulong) buffer,
                       (uint) buffer->copy_to_buffer_in_progress,
                       (uint) buffer->is_closing_buffer,
                       (buffer->file ? buffer->file->handler.file : -1),
                       (ulong) buffer->size));

  translog_wait_for_writers(buffer);

  if (offset != buffer->offset || file != buffer->file || ver != buffer->ver)
    DBUG_VOID_RETURN; /* the buffer if already freed */

  while (buffer->file != NULL)
  {
    DBUG_PRINT("info", ("wait for writers... buffer: #%u  0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer));
    pthread_cond_wait(&buffer->waiting_filling_buffer, &buffer->mutex);
    DBUG_PRINT("info", ("wait for writers done. buffer: #%u  0x%lx",
                        (uint) buffer->buffer_no, (ulong) buffer));
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
  @brief Initialize buffer for the current file, and a cursor for this buffer.

  @param buffer          The buffer
  @param cursor          It's cursor
  @param buffer_no       Number of buffer
*/

static void translog_start_buffer(struct st_translog_buffer *buffer,
                                  struct st_buffer_cursor *cursor,
                                  uint buffer_no)
{
  DBUG_ENTER("translog_start_buffer");
  DBUG_PRINT("enter",
             ("Assign buffer: #%u (0x%lx) offset: 0x%lx(%lu)",
              (uint) buffer->buffer_no, (ulong) buffer,
              (ulong) LSN_OFFSET(log_descriptor.horizon),
              (ulong) LSN_OFFSET(log_descriptor.horizon)));
  DBUG_ASSERT(buffer_no == buffer->buffer_no);
  buffer->prev_last_lsn= buffer->last_lsn= LSN_IMPOSSIBLE;
  DBUG_PRINT("info", ("last_lsn and prev_last_lsn set to 0  buffer: 0x%lx",
                      (ulong) buffer));
  buffer->offset= log_descriptor.horizon;
  buffer->next_buffer_offset= LSN_IMPOSSIBLE;
  buffer->file= get_current_logfile();
  buffer->overlay= 0;
  buffer->size= 0;
  translog_cursor_init(cursor, buffer, buffer_no);
  DBUG_PRINT("info", ("file: #%ld (%d)  init cursor #%u: 0x%lx  "
                      "chaser: %d  Size: %lu (%lu)",
                      (long) (buffer->file ? buffer->file->number : 0),
                      (buffer->file ? buffer->file->handler.file : -1),
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer)));
  translog_check_cursor(cursor);
  pthread_mutex_lock(&log_descriptor.dirty_buffer_mask_lock);
  log_descriptor.dirty_buffer_mask|= (1 << buffer->buffer_no);
  pthread_mutex_unlock(&log_descriptor.dirty_buffer_mask_lock);

  DBUG_VOID_RETURN;
}


/*
  @brief Switch to the next buffer in a chain.

  @param horizon         \ Pointers on current position in file and buffer
  @param cursor          /
  @param new_file        Also start new file

  @note
   - loghandler should be locked
   - after return new and old buffer still are locked

  @retval 0 OK
  @retval 1 Error
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
                      LSN_IN_PARTS(log_descriptor.horizon), chasing));

  DBUG_ASSERT(cmp_translog_addr(log_descriptor.horizon, *horizon) >= 0);

  translog_finish_page(horizon, cursor);

  if (!chasing)
  {
    translog_buffer_lock(new_buffer);
#ifndef DBUG_OFF
    {
      TRANSLOG_ADDRESS offset= new_buffer->offset;
      TRANSLOG_FILE *file= new_buffer->file;
      uint8 ver= new_buffer->ver;
      translog_lock_assert_owner();
#endif
      translog_wait_for_buffer_free(new_buffer);
#ifndef DBUG_OFF
      /* We keep the handler locked so nobody can start this new buffer */
      DBUG_ASSERT(offset == new_buffer->offset && new_buffer->file == NULL &&
                  (file == NULL ? ver : (uint8)(ver + 1)) == new_buffer->ver);
    }
#endif
  }
  else
    DBUG_ASSERT(new_buffer->file != NULL);

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
  {
    translog_lock_assert_owner();
    translog_start_buffer(new_buffer, cursor, new_buffer_no);
    new_buffer->prev_buffer_offset=
      log_descriptor.buffers[old_buffer_no].offset;
    new_buffer->prev_last_lsn=
      BUFFER_MAX_LSN(log_descriptor.buffers + old_buffer_no);
  }
  log_descriptor.buffers[old_buffer_no].next_buffer_offset= new_buffer->offset;
  DBUG_PRINT("info", ("prev_last_lsn set to (%lu,0x%lx)  buffer: 0x%lx",
                      LSN_IN_PARTS(new_buffer->prev_last_lsn),
                      (ulong) new_buffer));
  translog_new_page_header(horizon, cursor);
  DBUG_RETURN(0);
}


/*
  Sets max LSN sent to file, and address from which data is only in the buffer

  SYNOPSIS
    translog_set_sent_to_disk()
    buffer               buffer which we have sent to disk

  TODO: use atomic operations if possible (64bit architectures?)
*/

static void translog_set_sent_to_disk(struct st_translog_buffer *buffer)
{
  LSN lsn= buffer->last_lsn;
  TRANSLOG_ADDRESS in_buffers= buffer->next_buffer_offset;

  DBUG_ENTER("translog_set_sent_to_disk");
  pthread_mutex_lock(&log_descriptor.sent_to_disk_lock);
  DBUG_PRINT("enter", ("lsn: (%lu,0x%lx) in_buffers: (%lu,0x%lx)  "
                       "in_buffers_only: (%lu,0x%lx)  start: (%lu,0x%lx)  "
                       "sent_to_disk: (%lu,0x%lx)",
                       LSN_IN_PARTS(lsn),
                       LSN_IN_PARTS(in_buffers),
                       LSN_IN_PARTS(log_descriptor.log_start),
                       LSN_IN_PARTS(log_descriptor.in_buffers_only),
                       LSN_IN_PARTS(log_descriptor.sent_to_disk)));
  /*
    We write sequentially (first part of following assert) but we rewrite
    the same page in case we started mysql and shut it down immediately
    (second part of the following assert)
  */
  DBUG_ASSERT(cmp_translog_addr(lsn, log_descriptor.sent_to_disk) >= 0 ||
              cmp_translog_addr(lsn, log_descriptor.log_start) < 0);
  log_descriptor.sent_to_disk= lsn;
  /* LSN_IMPOSSIBLE == 0 => it will work for very first time */
  if (cmp_translog_addr(in_buffers, log_descriptor.in_buffers_only) > 0)
  {
    log_descriptor.in_buffers_only= in_buffers;
    DBUG_PRINT("info", ("set new in_buffers_only"));
  }
  pthread_mutex_unlock(&log_descriptor.sent_to_disk_lock);
  DBUG_VOID_RETURN;
}


/*
  Sets address from which data is only in the buffer

  SYNOPSIS
    translog_set_only_in_buffers()
    lsn                  LSN to assign
    in_buffers           to assign to in_buffers_only
*/

static void translog_set_only_in_buffers(TRANSLOG_ADDRESS in_buffers)
{
  DBUG_ENTER("translog_set_only_in_buffers");
  pthread_mutex_lock(&log_descriptor.sent_to_disk_lock);
  DBUG_PRINT("enter", ("in_buffers: (%lu,0x%lx)  "
                       "in_buffers_only: (%lu,0x%lx)",
                       LSN_IN_PARTS(in_buffers),
                       LSN_IN_PARTS(log_descriptor.in_buffers_only)));
  /* LSN_IMPOSSIBLE == 0 => it will work for very first time */
  if (cmp_translog_addr(in_buffers, log_descriptor.in_buffers_only) > 0)
  {
    if (translog_status != TRANSLOG_OK)
      DBUG_VOID_RETURN;
    log_descriptor.in_buffers_only= in_buffers;
    DBUG_PRINT("info", ("set new in_buffers_only"));
  }
  pthread_mutex_unlock(&log_descriptor.sent_to_disk_lock);
  DBUG_VOID_RETURN;
}


/*
  Gets address from which data is only in the buffer

  SYNOPSIS
    translog_only_in_buffers()

  RETURN
    address from which data is only in the buffer
*/

static TRANSLOG_ADDRESS translog_only_in_buffers()
{
  register TRANSLOG_ADDRESS addr;
  DBUG_ENTER("translog_only_in_buffers");
  pthread_mutex_lock(&log_descriptor.sent_to_disk_lock);
  addr= log_descriptor.in_buffers_only;
  pthread_mutex_unlock(&log_descriptor.sent_to_disk_lock);
  DBUG_RETURN(addr);
}


/*
  Get max LSN sent to file

  SYNOPSIS
    translog_get_sent_to_disk()

  RETURN
    max LSN send to file
*/

static LSN translog_get_sent_to_disk()
{
  register LSN lsn;
  DBUG_ENTER("translog_get_sent_to_disk");
  pthread_mutex_lock(&log_descriptor.sent_to_disk_lock);
  lsn= log_descriptor.sent_to_disk;
  DBUG_PRINT("info", ("sent to disk up to (%lu,0x%lx)", LSN_IN_PARTS(lsn)));
  pthread_mutex_unlock(&log_descriptor.sent_to_disk_lock);
  DBUG_RETURN(lsn);
}


/*
  Get first chunk address on the given page

  SYNOPSIS
    translog_get_first_chunk_offset()
    page                 The page where to find first chunk

  RETURN
    first chunk offset
*/

static my_bool translog_get_first_chunk_offset(uchar *page)
{
  DBUG_ENTER("translog_get_first_chunk_offset");
  DBUG_ASSERT(page[TRANSLOG_PAGE_FLAGS] < TRANSLOG_FLAGS_NUM);
  DBUG_RETURN(page_overhead[page[TRANSLOG_PAGE_FLAGS]]);
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
translog_write_variable_record_1group_code_len(uchar *dst,
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

static translog_size_t translog_variable_record_1group_decode_len(uchar **src)
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

static uint16 translog_get_total_chunk_length(uchar *page, uint16 offset)
{
  DBUG_ENTER("translog_get_total_chunk_length");
  switch (page[offset] & TRANSLOG_CHUNK_TYPE) {
  case TRANSLOG_CHUNK_LSN:
  {
    /* 0 chunk referred as LSN (head or tail) */
    translog_size_t rec_len;
    uchar *start= page + offset;
    uchar *ptr= start + 1 + 2; /* chunk type and short trid */
    uint16 chunk_len, header_len, page_rest;
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LSN"));
    rec_len= translog_variable_record_1group_decode_len(&ptr);
    chunk_len= uint2korr(ptr);
    header_len= (uint16) (ptr -start) + 2;
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
  }
  case TRANSLOG_CHUNK_FIXED:
  {
    uchar *ptr;
    uint type= page[offset] & TRANSLOG_REC_TYPE;
    uint length;
    int i;
    /* 1 (pseudo)fixed record (also LSN) */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_FIXED"));
    DBUG_ASSERT(log_record_type_descriptor[type].rclass ==
                LOGRECTYPE_FIXEDLENGTH ||
                log_record_type_descriptor[type].rclass ==
                LOGRECTYPE_PSEUDOFIXEDLENGTH);
    if (log_record_type_descriptor[type].rclass == LOGRECTYPE_FIXEDLENGTH)
    {
      DBUG_PRINT("info",
                 ("Fixed length: %u",
                  (uint) (log_record_type_descriptor[type].fixed_length + 3)));
      DBUG_RETURN(log_record_type_descriptor[type].fixed_length + 3);
    }

    ptr= page + offset + 3;            /* first compressed LSN */
    length= log_record_type_descriptor[type].fixed_length + 3;
    for (i= 0; i < log_record_type_descriptor[type].compressed_LSN; i++)
    {
      /* first 2 bits is length - 2 */
      uint len= (((uint8) (*ptr)) >> 6) + 2;
      if (ptr[0] == 0 && ((uint8) ptr[1]) == 1)
        len+= LSN_STORE_SIZE; /* case of full LSN storing */
      ptr+= len;
      /* subtract saved bytes */
      length-= (LSN_STORE_SIZE - len);
    }
    DBUG_PRINT("info", ("Pseudo-fixed length: %u", length));
    DBUG_RETURN(length);
  }
  case TRANSLOG_CHUNK_NOHDR:
    /* 2 no header chunk (till page end) */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_NOHDR  length: %u",
                        (uint) (TRANSLOG_PAGE_SIZE - offset)));
    DBUG_RETURN(TRANSLOG_PAGE_SIZE - offset);
  case TRANSLOG_CHUNK_LNGTH:                   /* 3 chunk with chunk length */
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LNGTH"));
    DBUG_ASSERT(TRANSLOG_PAGE_SIZE - offset >= 3);
    DBUG_PRINT("info", ("length: %u", uint2korr(page + offset + 1) + 3));
    DBUG_RETURN(uint2korr(page + offset + 1) + 3);
  default:
    DBUG_ASSERT(0);
    DBUG_RETURN(0);
  }
}

/*
  @brief Waits previous buffer flush finish

  @param buffer          buffer for check

  @retval 0 previous buffer flushed and this thread have to flush this one
  @retval 1 previous buffer flushed and this buffer flushed by other thread too
*/

my_bool translog_prev_buffer_flush_wait(struct st_translog_buffer *buffer)
{
  TRANSLOG_ADDRESS offset= buffer->offset;
  TRANSLOG_FILE *file= buffer->file;
  uint8 ver= buffer->ver;
  DBUG_ENTER("translog_prev_buffer_flush_wait");
  DBUG_PRINT("enter", ("buffer: 0x%lx  #%u  offset: (%lu,0x%lx)  "
                       "prev sent: (%lu,0x%lx) prev offset: (%lu,0x%lx)",
                       (ulong) buffer, (uint) buffer->buffer_no,
                       LSN_IN_PARTS(buffer->offset),
                       LSN_IN_PARTS(buffer->prev_sent_to_disk),
                       LSN_IN_PARTS(buffer->prev_buffer_offset)));
  translog_buffer_lock_assert_owner(buffer);
  if (buffer->prev_buffer_offset != buffer->prev_sent_to_disk)
  {
    do {
      pthread_cond_wait(&buffer->prev_sent_to_disk_cond, &buffer->mutex);
      if (buffer->file != file || buffer->offset != offset ||
          buffer->ver != ver)
        DBUG_RETURN(1); /* some the thread flushed the buffer already */
    } while(buffer->prev_buffer_offset != buffer->prev_sent_to_disk);
  }
  DBUG_RETURN(0);
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
  uint32 i, pg;
  TRANSLOG_ADDRESS offset= buffer->offset;
  TRANSLOG_FILE *file= buffer->file;
  uint8 ver= buffer->ver;
  DBUG_ENTER("translog_buffer_flush");
  DBUG_PRINT("enter",
             ("Buffer: #%u 0x%lx file: %d  offset: (%lu,0x%lx)  size: %lu",
              (uint) buffer->buffer_no, (ulong) buffer,
              buffer->file->handler.file,
              LSN_IN_PARTS(buffer->offset),
              (ulong) buffer->size));
  translog_buffer_lock_assert_owner(buffer);

  if (buffer->file == NULL)
    DBUG_RETURN(0);

  translog_wait_for_writers(buffer);

  if (buffer->file != file || buffer->offset != offset || buffer->ver != ver)
    DBUG_RETURN(0); /* some the thread flushed the buffer already */

  if (buffer->is_closing_buffer)
  {
    /* some other flush in progress */
    translog_wait_for_closing(buffer);
    if (buffer->file != file || buffer->offset != offset || buffer->ver != ver)
      DBUG_RETURN(0); /* some the thread flushed the buffer already */
  }

  if (buffer->overlay && translog_prev_buffer_flush_wait(buffer))
    DBUG_RETURN(0); /* some the thread flushed the buffer already */

  /*
    Send page by page in the pagecache what we are going to write on the
    disk
  */
  file= buffer->file;
  for (i= 0, pg= LSN_OFFSET(buffer->offset) / TRANSLOG_PAGE_SIZE;
       i < buffer->size;
       i+= TRANSLOG_PAGE_SIZE, pg++)
  {
    TRANSLOG_ADDRESS addr= (buffer->offset + i);
    TRANSLOG_VALIDATOR_DATA data;
    DBUG_PRINT("info", ("send log form %lu till %lu  address: (%lu,0x%lx)  "
                        "page #: %lu  buffer size: %lu  buffer: 0x%lx",
                        (ulong) i, (ulong) (i + TRANSLOG_PAGE_SIZE),
                        LSN_IN_PARTS(addr), (ulong) pg, (ulong) buffer->size,
                        (ulong) buffer));
    data.addr= &addr;
    DBUG_ASSERT(log_descriptor.pagecache->block_size == TRANSLOG_PAGE_SIZE);
    DBUG_ASSERT(i + TRANSLOG_PAGE_SIZE <= buffer->size);
    if (translog_status != TRANSLOG_OK && translog_status != TRANSLOG_SHUTDOWN)
      DBUG_RETURN(1);
    if (pagecache_inject(log_descriptor.pagecache,
                        &file->handler, pg, 3,
                        buffer->buffer + i,
                        PAGECACHE_PLAIN_PAGE,
                        PAGECACHE_LOCK_LEFT_UNLOCKED,
                        PAGECACHE_PIN_LEFT_UNPINNED, 0,
                        LSN_IMPOSSIBLE))
    {
      DBUG_PRINT("error",
                 ("Can't write page (%lu,0x%lx) to pagecache, error: %d",
                  (ulong) buffer->file->number,
                  (ulong) (LSN_OFFSET(buffer->offset)+ i),
                  my_errno));
      translog_stop_writing();
      DBUG_RETURN(1);
    }
  }
  file->is_sync= 0;
  if (my_pwrite(file->handler.file, buffer->buffer,
                buffer->size, LSN_OFFSET(buffer->offset),
                log_write_flags))
  {
    DBUG_PRINT("error", ("Can't write buffer (%lu,0x%lx) size %lu "
                         "to the disk (%d)",
                         (ulong) file->handler.file,
                         (ulong) LSN_OFFSET(buffer->offset),
                         (ulong) buffer->size, errno));
    translog_stop_writing();
    DBUG_RETURN(1);
  }
  /*
    Dropping the flag in such way can make false alarm: signalling than the
    file in not sync when it is sync, but the situation is quite rare and
    protections with mutexes give much more overhead to the whole engine
  */
  file->is_sync= 0;

  if (LSN_OFFSET(buffer->last_lsn) != 0)    /* if buffer->last_lsn is set */
  {
    if (translog_prev_buffer_flush_wait(buffer))
      DBUG_RETURN(0); /* some the thread flushed the buffer already */
    translog_set_sent_to_disk(buffer);
  }
  else
    translog_set_only_in_buffers(buffer->next_buffer_offset);

  /* say to next buffer that we are finished */
  {
    struct st_translog_buffer *next_buffer=
      log_descriptor.buffers + ((buffer->buffer_no + 1) % TRANSLOG_BUFFERS_NO);
    if (likely(translog_status == TRANSLOG_OK)){
      translog_buffer_lock(next_buffer);
      next_buffer->prev_sent_to_disk= buffer->offset;
      translog_buffer_unlock(next_buffer);
      pthread_cond_broadcast(&next_buffer->prev_sent_to_disk_cond);
    }
    else
    {
      /*
        It is shutdown =>
          1) there is only one thread
          2) mutexes of other buffers can be destroyed => we can't use them
      */
      next_buffer->prev_sent_to_disk= buffer->offset;
    }
  }
  /* Free buffer */
  buffer->file= NULL;
  buffer->overlay= 0;
  buffer->ver++;
  pthread_mutex_lock(&log_descriptor.dirty_buffer_mask_lock);
  log_descriptor.dirty_buffer_mask&= ~(1 << buffer->buffer_no);
  pthread_mutex_unlock(&log_descriptor.dirty_buffer_mask_lock);
  pthread_cond_broadcast(&buffer->waiting_filling_buffer);
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

static my_bool translog_recover_page_up_to_sector(uchar *page, uint16 offset)
{
  uint16 chunk_offset= translog_get_first_chunk_offset(page), valid_chunk_end;
  DBUG_ENTER("translog_recover_page_up_to_sector");
  DBUG_PRINT("enter", ("offset: %u  first chunk: %u",
                       (uint) offset, (uint) chunk_offset));

  while (page[chunk_offset] != TRANSLOG_FILLER && chunk_offset < offset)
  {
    uint16 chunk_length;
    if ((chunk_length=
         translog_get_total_chunk_length(page, chunk_offset)) == 0)
    {
      DBUG_PRINT("error", ("cant get chunk length (offset %u)",
                           (uint) chunk_offset));
      DBUG_RETURN(1);
    }
    DBUG_PRINT("info", ("chunk: offset: %u  length %u",
                        (uint) chunk_offset, (uint) chunk_length));
    if (((ulong) chunk_offset) + ((ulong) chunk_length) > TRANSLOG_PAGE_SIZE)
    {
      DBUG_PRINT("error", ("damaged chunk (offset %u) in trusted area",
                           (uint) chunk_offset));
      DBUG_RETURN(1);
    }
    chunk_offset+= chunk_length;
  }

  valid_chunk_end= chunk_offset;
  /* end of trusted area - sector parsing */
  while (page[chunk_offset] != TRANSLOG_FILLER)
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

  memset(page + valid_chunk_end, TRANSLOG_FILLER,
         TRANSLOG_PAGE_SIZE - valid_chunk_end);

  DBUG_RETURN(0);
}


/**
  @brief Dummy write callback.
*/

static my_bool
translog_dummy_callback(uchar *page __attribute__((unused)),
                        pgcache_page_no_t page_no __attribute__((unused)),
                        uchar* data_ptr __attribute__((unused)))
{
  return 0;
}


/**
  @brief Checks and removes sector protection.

  @param page            reference on the page content.
  @param file            transaction log descriptor.

  @retvat 0 OK
  @retval 1 Error
*/

static my_bool
translog_check_sector_protection(uchar *page, TRANSLOG_FILE *file)
{
  uint i, offset;
  uchar *table= page + page_overhead[page[TRANSLOG_PAGE_FLAGS]] -
    TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
  uint8 current= table[0];
  DBUG_ENTER("translog_check_sector_protection");

  for (i= 1, offset= DISK_DRIVE_SECTOR_SIZE;
       i < TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
       i++, offset+= DISK_DRIVE_SECTOR_SIZE)
  {
    /*
      TODO: add chunk counting for "suspecting" sectors (difference is
      more than 1-2), if difference more then present chunks then it is
      the problem.
    */
    uint8 test= page[offset];
    DBUG_PRINT("info", ("sector: #%u  offset: %u  current: %lx "
                        "read: 0x%x  stored: 0x%x%x",
                        i, offset, (ulong) current,
                        (uint) uint2korr(page + offset), (uint) table[i],
                        (uint) table[i + 1]));
    /*
      3 is minimal possible record length. So we can have "distance"
      between 2 sectors value more then DISK_DRIVE_SECTOR_SIZE / 3
      only if it is old value, i.e. the sector was not written.
    */
    if (((test < current) &&
         ((uint)(0xFFL - current + test) > DISK_DRIVE_SECTOR_SIZE / 3)) ||
        ((test >= current) &&
         ((uint)(test - current) > DISK_DRIVE_SECTOR_SIZE / 3)))
    {
      if (translog_recover_page_up_to_sector(page, offset))
        DBUG_RETURN(1);
      file->was_recovered= 1;
      DBUG_RETURN(0);
    }

    /* Restore value on the page */
    page[offset]= table[i];
    current= test;
    DBUG_PRINT("info", ("sector: #%u  offset: %u  current: %lx  "
                        "read: 0x%x  stored: 0x%x",
                        i, offset, (ulong) current,
                        (uint) page[offset], (uint) table[i]));
  }
  DBUG_RETURN(0);
}


/**
  @brief Log page validator (read callback)

  @param page            The page data to check
  @param page_no         The page number (<offset>/<page length>)
  @param data_ptr        Read callback data pointer (pointer to TRANSLOG_FILE)

  @todo: add turning loghandler to read-only mode after merging with
  that patch.

  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_page_validator(uchar *page,
                                       pgcache_page_no_t page_no,
                                       uchar* data_ptr)
{
  uint this_page_page_overhead;
  uint flags;
  uchar *page_pos;
  TRANSLOG_FILE *data= (TRANSLOG_FILE *) data_ptr;
#ifndef DBUG_OFF
  pgcache_page_no_t offset= page_no * TRANSLOG_PAGE_SIZE;
#endif
  DBUG_ENTER("translog_page_validator");

  data->was_recovered= 0;

  if ((pgcache_page_no_t) uint3korr(page) != page_no ||
      (uint32) uint3korr(page + 3) != data->number)
  {
    DBUG_PRINT("error", ("Page (%lu,0x%lx): "
                         "page address written in the page is incorrect: "
                         "File %lu instead of %lu or page %lu instead of %lu",
                         (ulong) data->number, (ulong) offset,
                         (ulong) uint3korr(page + 3), (ulong) data->number,
                         (ulong) uint3korr(page),
                         (ulong) page_no));
    DBUG_RETURN(1);
  }
  flags= (uint)(page[TRANSLOG_PAGE_FLAGS]);
  this_page_page_overhead= page_overhead[flags];
  if (flags & ~(TRANSLOG_PAGE_CRC | TRANSLOG_SECTOR_PROTECTION |
                TRANSLOG_RECORD_CRC))
  {
    DBUG_PRINT("error", ("Page (%lu,0x%lx): "
                         "Garbage in the page flags field detected : %x",
                         (ulong) data->number, (ulong) offset,
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
      DBUG_PRINT("error", ("Page (%lu,0x%lx): "
                           "CRC mismatch: calculated: %lx on the page %lx",
                           (ulong) data->number, (ulong) offset,
                           (ulong) crc, (ulong) uint4korr(page_pos)));
      DBUG_RETURN(1);
    }
    page_pos+= CRC_SIZE;                      /* Skip crc */
  }
  if (flags & TRANSLOG_SECTOR_PROTECTION &&
      translog_check_sector_protection(page, data))
  {
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/**
  @brief Locks the loghandler.
*/

void translog_lock()
{
  uint8 current_buffer;
  DBUG_ENTER("translog_lock");

  /*
     Locking the loghandler mean locking current buffer, but it can change
     during locking, so we should check it
  */
  for (;;)
  {
    /*
      log_descriptor.bc.buffer_no is only one byte so its reading is
      an atomic operation
    */
    current_buffer= log_descriptor.bc.buffer_no;
    translog_buffer_lock(log_descriptor.buffers + current_buffer);
    if (log_descriptor.bc.buffer_no == current_buffer)
      break;
    translog_buffer_unlock(log_descriptor.buffers + current_buffer);
  }
  DBUG_VOID_RETURN;
}


/*
  Unlock the loghandler

  SYNOPSIS
    translog_unlock()

  RETURN
    0  OK
    1  Error
*/

void translog_unlock()
{
  translog_buffer_unlock(log_descriptor.bc.buffer);
}


/**
  @brief Get log page by file number and offset of the beginning of the page

  @param data            validator data, which contains the page address
  @param buffer          buffer for page placing
                         (might not be used in some cache implementations)
  @param direct_link     if it is not NULL then caller can accept direct
                         link to the page cache

  @retval NULL Error
  @retval #    pointer to the page cache which should be used to read this page
*/

static uchar *translog_get_page(TRANSLOG_VALIDATOR_DATA *data, uchar *buffer,
                                PAGECACHE_BLOCK_LINK **direct_link)
{
  TRANSLOG_ADDRESS addr= *(data->addr), in_buffers;
  uint32 file_no= LSN_FILE_NO(addr);
  TRANSLOG_FILE *file;
  DBUG_ENTER("translog_get_page");
  DBUG_PRINT("enter", ("File: %lu  Offset: %lu(0x%lx)",
                       (ulong) file_no,
                       (ulong) LSN_OFFSET(addr),
                       (ulong) LSN_OFFSET(addr)));

  /* it is really page address */
  DBUG_ASSERT(LSN_OFFSET(addr) % TRANSLOG_PAGE_SIZE == 0);
  if (direct_link)
    *direct_link= NULL;

restart:

  in_buffers= translog_only_in_buffers();
  DBUG_PRINT("info", ("in_buffers: (%lu,0x%lx)",
                      LSN_IN_PARTS(in_buffers)));
  if (in_buffers != LSN_IMPOSSIBLE &&
      cmp_translog_addr(addr, in_buffers) >= 0)
  {
    translog_lock();
    DBUG_ASSERT(cmp_translog_addr(addr, log_descriptor.horizon) < 0);
    /* recheck with locked loghandler */
    in_buffers= translog_only_in_buffers();
    if (cmp_translog_addr(addr, in_buffers) >= 0)
    {
      uint16 buffer_no= log_descriptor.bc.buffer_no;
#ifndef DBUG_OFF
      uint16 buffer_start= buffer_no;
#endif
      struct st_translog_buffer *buffer_unlock= log_descriptor.bc.buffer;
      struct st_translog_buffer *curr_buffer= log_descriptor.bc.buffer;
      for (;;)
      {
        /*
          if the page is in the buffer and it is the last version of the
          page (in case of division the page by buffer flush)
        */
        if (curr_buffer->file != NULL &&
            cmp_translog_addr(addr, curr_buffer->offset) >= 0 &&
            cmp_translog_addr(addr,
                              (curr_buffer->next_buffer_offset ?
                               curr_buffer->next_buffer_offset:
                               curr_buffer->offset + curr_buffer->size)) < 0)
        {
          TRANSLOG_ADDRESS offset= curr_buffer->offset;
          TRANSLOG_FILE *fl= curr_buffer->file;
          uchar *from, *table= NULL;
          int is_last_unfinished_page;
          uint last_protected_sector= 0;
          TRANSLOG_FILE file_copy;
          uint8 ver= curr_buffer->ver;
          translog_wait_for_writers(curr_buffer);
          if (offset != curr_buffer->offset || fl != curr_buffer->file ||
              ver != curr_buffer->ver)
          {
            DBUG_ASSERT(buffer_unlock == curr_buffer);
            translog_buffer_unlock(buffer_unlock);
            goto restart;
          }
          DBUG_ASSERT(LSN_FILE_NO(addr) ==  LSN_FILE_NO(curr_buffer->offset));
          from= curr_buffer->buffer + (addr - curr_buffer->offset);
          memcpy(buffer, from, TRANSLOG_PAGE_SIZE);
          /*
            We can use copy then in translog_page_validator() because it
            do not put it permanently somewhere.
            We have to use copy because after releasing log lock we can't
            guaranty that the file still be present (in real life it will be
            present but theoretically possible that it will be released
            already from last files cache);
          */
          file_copy= *(curr_buffer->file);
          file_copy.handler.callback_data= (uchar*) &file_copy;
          is_last_unfinished_page= ((log_descriptor.bc.buffer ==
                                     curr_buffer) &&
                                    (log_descriptor.bc.ptr >= from) &&
                                    (log_descriptor.bc.ptr <
                                     from + TRANSLOG_PAGE_SIZE));
          if (is_last_unfinished_page &&
              (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION))
          {
            last_protected_sector= ((log_descriptor.bc.previous_offset - 1) /
                                    DISK_DRIVE_SECTOR_SIZE);
            table= buffer + log_descriptor.page_overhead -
              TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
          }

          DBUG_ASSERT(buffer_unlock == curr_buffer);
          translog_buffer_unlock(buffer_unlock);
          if (is_last_unfinished_page)
          {
            uint i;
            /*
              This is last unfinished page => we should not check CRC and
              remove only that protection which already installed (no need
              to check it)

              We do not check the flag of sector protection, because if
              (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION) is
              not set then last_protected_sector will be 0 so following loop
              will be never executed
            */
            DBUG_PRINT("info", ("This is last unfinished page, "
                                "last protected sector %u",
                                last_protected_sector));
            for (i= 1; i <= last_protected_sector; i++)
            {
              uint offset= i * DISK_DRIVE_SECTOR_SIZE;
              DBUG_PRINT("info", ("Sector %u: 0x%02x <- 0x%02x",
                                  i, buffer[offset],
                                  table[i]));
              buffer[offset]= table[i];
            }
          }
          else
          {
            /*
              This IF should be true because we use in-memory data which
              supposed to be correct.
            */
            if (translog_page_validator(buffer,
                                        LSN_OFFSET(addr) / TRANSLOG_PAGE_SIZE,
                                        (uchar*) &file_copy))
            {
              DBUG_ASSERT(0);
              buffer= NULL;
            }
          }
          DBUG_RETURN(buffer);
        }
        buffer_no= (buffer_no + 1) % TRANSLOG_BUFFERS_NO;
        curr_buffer= log_descriptor.buffers + buffer_no;
        translog_buffer_lock(curr_buffer);
        translog_buffer_unlock(buffer_unlock);
        buffer_unlock= curr_buffer;
        /* we can't make a full circle */
        DBUG_ASSERT(buffer_start != buffer_no);
      }
    }
    translog_unlock();
  }
  file= get_logfile_by_number(file_no);
  DBUG_ASSERT(file != NULL);
  buffer= pagecache_read(log_descriptor.pagecache, &file->handler,
                         LSN_OFFSET(addr) / TRANSLOG_PAGE_SIZE,
                         3, (direct_link ? NULL : buffer),
                         PAGECACHE_PLAIN_PAGE,
                         (direct_link ?
                          PAGECACHE_LOCK_READ :
                          PAGECACHE_LOCK_LEFT_UNLOCKED),
                         direct_link);
  DBUG_PRINT("info", ("Direct link is assigned to : 0x%lx * 0x%lx",
                      (ulong) direct_link,
                      (ulong)(direct_link ? *direct_link : NULL)));
  data->was_recovered= file->was_recovered;
  DBUG_RETURN(buffer);
}


/**
  @brief free direct log page link

  @param direct_link the direct log page link to be freed

*/

static void translog_free_link(PAGECACHE_BLOCK_LINK *direct_link)
{
  DBUG_ENTER("translog_free_link");
  DBUG_PRINT("info", ("Direct link: 0x%lx",
                      (ulong) direct_link));
  if (direct_link)
    pagecache_unlock_by_link(log_descriptor.pagecache, direct_link,
                             PAGECACHE_LOCK_READ_UNLOCK, PAGECACHE_UNPIN,
                             LSN_IMPOSSIBLE, LSN_IMPOSSIBLE, 0, FALSE);
  DBUG_VOID_RETURN;
}


/**
  @brief Finds last full page of the given log file.

  @param addr            address structure to fill with data, which contain
                         file number of the log file
  @param last_page_ok    Result of the check whether last page OK.
                         (for now only we check only that file length
                         divisible on page length).
  @param no_errors       suppress messages about non-critical errors

  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_get_last_page_addr(TRANSLOG_ADDRESS *addr,
                                           my_bool *last_page_ok,
                                           my_bool no_errors)
{
  char path[FN_REFLEN];
  uint32 rec_offset;
  my_off_t file_size;
  uint32 file_no= LSN_FILE_NO(*addr);
  TRANSLOG_FILE *file;
#ifndef DBUG_OFF
  char buff[21];
#endif
  DBUG_ENTER("translog_get_last_page_addr");

  if (likely((file= get_logfile_by_number(file_no)) != NULL))
  {
    /*
      This function used only during initialization of loghandler or in
      scanner (which mean we need read that part of the log), so the
      requested log file have to be opened and can't be freed after
      returning pointer on it (file_size).
    */
    file_size= my_seek(file->handler.file, 0, SEEK_END, MYF(0));
  }
  else
  {
    /*
      This branch is used only during very early initialization
      when files are not opened.
    */
    File fd;
    if ((fd= my_open(translog_filename_by_fileno(file_no, path),
                     O_RDONLY, (no_errors ? MYF(0) : MYF(MY_WME)))) < 0)
    {
      my_errno= errno;
      DBUG_PRINT("error", ("Error %d during opening file #%d",
                           errno, file_no));
      DBUG_RETURN(1);
    }
    file_size= my_seek(fd, 0, SEEK_END, MYF(0));
    my_close(fd, MYF(0));
  }
  DBUG_PRINT("info", ("File size: %s", llstr(file_size, buff)));
  if (file_size == MY_FILEPOS_ERROR)
    DBUG_RETURN(1);
  DBUG_ASSERT(file_size < ULL(0xffffffff));
  if (((uint32)file_size) > TRANSLOG_PAGE_SIZE)
  {
    rec_offset= (((((uint32)file_size) / TRANSLOG_PAGE_SIZE) - 1) *
                       TRANSLOG_PAGE_SIZE);
    *last_page_ok= (((uint32)file_size) == rec_offset + TRANSLOG_PAGE_SIZE);
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


/**
  @brief Get number bytes for record length storing

  @param length          Record length which will be encoded

  @return 1,3,4,5 - number of bytes to store given length
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


/**
  @brief Gets header of this chunk.

  @param chunk           The pointer to the chunk beginning

  @retval # total length of the chunk
  @retval 0 Error
*/

static uint16 translog_get_chunk_header_length(uchar *chunk)
{
  DBUG_ENTER("translog_get_chunk_header_length");
  switch (*chunk & TRANSLOG_CHUNK_TYPE) {
  case TRANSLOG_CHUNK_LSN:
  {
    /* 0 chunk referred as LSN (head or tail) */
    translog_size_t rec_len;
    uchar *start= chunk;
    uchar *ptr= start + 1 + 2;
    uint16 chunk_len, header_len;
    DBUG_PRINT("info", ("TRANSLOG_CHUNK_LSN"));
    rec_len= translog_variable_record_1group_decode_len(&ptr);
    chunk_len= uint2korr(ptr);
    header_len= (uint16) (ptr - start) +2;
    DBUG_PRINT("info", ("rec len: %lu  chunk len: %u  header len: %u",
                        (ulong) rec_len, (uint) chunk_len, (uint) header_len));
    if (chunk_len)
    {
      /* TODO: fine header end */
      /*
        The last chunk of multi-group record can be base for it header
        calculation (we skip to the first group to read the header) so if we
        stuck here something is wrong.
      */
      DBUG_ASSERT(0);
      DBUG_RETURN(0);                               /* Keep compiler happy */
    }
    DBUG_RETURN(header_len);
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
    DBUG_RETURN(0);                               /* Keep compiler happy */
  }
}


/**
  @brief Truncate the log to the given address. Used during the startup if the
         end of log if corrupted.

  @param addr            new horizon

  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_truncate_log(TRANSLOG_ADDRESS addr)
{
  uchar *page;
  TRANSLOG_ADDRESS current_page;
  uint32 next_page_offset, page_rest;
  uint32 i;
  File fd;
  TRANSLOG_VALIDATOR_DATA data;
  char path[FN_REFLEN];
  uchar page_buff[TRANSLOG_PAGE_SIZE];
  DBUG_ENTER("translog_truncate_log");
  /* TODO: write warning to the client */
  DBUG_PRINT("warning", ("removing all records from (%lu,0x%lx) "
                         "till (%lu,0x%lx)",
                         LSN_IN_PARTS(addr),
                         LSN_IN_PARTS(log_descriptor.horizon)));
  DBUG_ASSERT(cmp_translog_addr(addr, log_descriptor.horizon) < 0);
  /* remove files between the address and horizon */
  for (i= LSN_FILE_NO(addr) + 1; i <= LSN_FILE_NO(log_descriptor.horizon); i++)
    if (my_delete(translog_filename_by_fileno(i, path),  MYF(MY_WME)))
    {
      translog_unlock();
      DBUG_RETURN(1);
    }

  /* truncate the last file up to the last page */
  next_page_offset= LSN_OFFSET(addr);
  next_page_offset= (next_page_offset -
                     ((next_page_offset - 1) % TRANSLOG_PAGE_SIZE + 1) +
                     TRANSLOG_PAGE_SIZE);
  page_rest= next_page_offset - LSN_OFFSET(addr);
  memset(page_buff, TRANSLOG_FILLER, page_rest);
  if ((fd= open_logfile_by_number_no_cache(LSN_FILE_NO(addr))) < 0 ||
      ((my_chsize(fd, next_page_offset, TRANSLOG_FILLER, MYF(MY_WME)) ||
        (page_rest && my_pwrite(fd, page_buff, page_rest, LSN_OFFSET(addr),
                                log_write_flags)) ||
        my_sync(fd, MYF(MY_WME))) |
       my_close(fd, MYF(MY_WME))) ||
      (sync_log_dir >= TRANSLOG_SYNC_DIR_ALWAYS &&
       sync_dir(log_descriptor.directory_fd, MYF(MY_WME | MY_IGNORE_BADFD))))
    DBUG_RETURN(1);

  /* fix the horizon */
  log_descriptor.horizon= addr;
  /* fix the buffer data */
  current_page= MAKE_LSN(LSN_FILE_NO(addr), (next_page_offset -
                                             TRANSLOG_PAGE_SIZE));
  data.addr= &current_page;
  if ((page= translog_get_page(&data, log_descriptor.buffers->buffer, NULL)) ==
      NULL)
    DBUG_RETURN(1);
  if (page != log_descriptor.buffers->buffer)
    memcpy(log_descriptor.buffers->buffer, page, TRANSLOG_PAGE_SIZE);
  log_descriptor.bc.buffer->offset= current_page;
  log_descriptor.bc.buffer->size= LSN_OFFSET(addr) - LSN_OFFSET(current_page);
  log_descriptor.bc.ptr=
    log_descriptor.buffers->buffer + log_descriptor.bc.buffer->size;
  log_descriptor.bc.current_page_fill= log_descriptor.bc.buffer->size;
  DBUG_RETURN(0);
}


/**
  Applies function 'callback' to all files (in a directory) which
  name looks like a log's name (maria_log.[0-9]{7}).
  If 'callback' returns TRUE this interrupts the walk and returns
  TRUE. Otherwise FALSE is returned after processing all log files.
  It cannot just use log_descriptor.directory because that may not yet have
  been initialized.

  @param  directory        directory to scan
  @param  callback         function to apply; is passed directory and base
                           name of found file
*/

my_bool translog_walk_filenames(const char *directory,
                                my_bool (*callback)(const char *,
                                                    const char *))
{
  MY_DIR *dirp;
  uint i;
  my_bool rc= FALSE;

  /* Finds and removes transaction log files */
  if (!(dirp = my_dir(directory, MYF(MY_DONT_SORT))))
    return FALSE;

  for (i= 0; i < dirp->number_off_files; i++)
  {
    char *file= dirp->dir_entry[i].name;
    if (strncmp(file, "maria_log.", 10) == 0 &&
        file[10] >= '0' && file[10] <= '9' &&
        file[11] >= '0' && file[11] <= '9' &&
        file[12] >= '0' && file[12] <= '9' &&
        file[13] >= '0' && file[13] <= '9' &&
        file[14] >= '0' && file[14] <= '9' &&
        file[15] >= '0' && file[15] <= '9' &&
        file[16] >= '0' && file[16] <= '9' &&
        file[17] >= '0' && file[17] <= '9' &&
        file[18] == '\0' && (*callback)(directory, file))
    {
      rc= TRUE;
      break;
    }
  }
  my_dirend(dirp);
  return rc;
}


/**
  @brief Fills table of dependence length of page header from page flags
*/

void translog_fill_overhead_table()
{
  uint i;
  for (i= 0; i < TRANSLOG_FLAGS_NUM; i++)
  {
     page_overhead[i]= 7;
     if (i & TRANSLOG_PAGE_CRC)
       page_overhead[i]+= CRC_SIZE;
     if (i & TRANSLOG_SECTOR_PROTECTION)
       page_overhead[i]+= TRANSLOG_PAGE_SIZE /
                           DISK_DRIVE_SECTOR_SIZE;
  }
}


/**
  Callback to find first log in directory.
*/

static my_bool translog_callback_search_first(const char *directory
                                              __attribute__((unused)),
                                              const char *filename
                                              __attribute__((unused)))
{
  return TRUE;
}


/**
  @brief Checks that chunk is LSN one

  @param type            type of the chunk

  @retval 1 the chunk is LNS
  @retval 0 the chunk is not LSN
*/

static my_bool translog_is_LSN_chunk(uchar type)
{
  DBUG_ENTER("translog_is_LSN_chunk");
  DBUG_PRINT("info", ("byte: %x  chunk type: %u  record type: %u",
                      type, type >> 6, type & TRANSLOG_REC_TYPE));
  DBUG_RETURN(((type & TRANSLOG_CHUNK_TYPE) == TRANSLOG_CHUNK_FIXED) ||
              (((type & TRANSLOG_CHUNK_TYPE) == TRANSLOG_CHUNK_LSN)  &&
               ((type & TRANSLOG_REC_TYPE)) != TRANSLOG_CHUNK_0_CONT));
}


/**
  @brief Initialize transaction log

  @param directory       Directory where log files are put
  @param log_file_max_size max size of one log size (for new logs creation)
  @param server_version  version of MySQL server (MYSQL_VERSION_ID)
  @param server_id       server ID (replication & Co)
  @param pagecache       Page cache for the log reads
  @param flags           flags (TRANSLOG_PAGE_CRC, TRANSLOG_SECTOR_PROTECTION
                           TRANSLOG_RECORD_CRC)
  @param read_only       Put transaction log in read-only mode
  @param init_table_func function to initialize record descriptors table
  @param no_errors       suppress messages about non-critical errors

  @todo
    Free used resources in case of error.

  @retval 0 OK
  @retval 1 Error
*/

my_bool translog_init_with_table(const char *directory,
                                 uint32 log_file_max_size,
                                 uint32 server_version,
                                 uint32 server_id, PAGECACHE *pagecache,
                                 uint flags, my_bool readonly,
                                 void (*init_table_func)(),
                                 my_bool no_errors)
{
  int i;
  int old_log_was_recovered= 0, logs_found= 0;
  uint old_flags= flags;
  uint32 start_file_num= 1;
  TRANSLOG_ADDRESS sure_page, last_page, last_valid_page, checkpoint_lsn;
  my_bool version_changed= 0;
  DBUG_ENTER("translog_init_with_table");

  id_to_share= NULL;
  log_descriptor.directory_fd= -1;
  log_descriptor.is_everything_flushed= 1;
  log_descriptor.flush_in_progress= 0;
  log_descriptor.flush_no= 0;
  log_descriptor.next_pass_max_lsn= LSN_IMPOSSIBLE;

  (*init_table_func)();
  compile_time_assert(sizeof(log_descriptor.dirty_buffer_mask) * 8 >=
                      TRANSLOG_BUFFERS_NO);
  log_descriptor.dirty_buffer_mask= 0;
  if (readonly)
    log_descriptor.open_flags= O_BINARY | O_RDONLY;
  else
    log_descriptor.open_flags= O_BINARY | O_RDWR;
  if (pthread_mutex_init(&log_descriptor.sent_to_disk_lock,
                         MY_MUTEX_INIT_FAST) ||
      pthread_mutex_init(&log_descriptor.file_header_lock,
                         MY_MUTEX_INIT_FAST) ||
      pthread_mutex_init(&log_descriptor.unfinished_files_lock,
                         MY_MUTEX_INIT_FAST) ||
      pthread_mutex_init(&log_descriptor.purger_lock,
                         MY_MUTEX_INIT_FAST) ||
      pthread_mutex_init(&log_descriptor.log_flush_lock,
                         MY_MUTEX_INIT_FAST) ||
      pthread_mutex_init(&log_descriptor.dirty_buffer_mask_lock,
                         MY_MUTEX_INIT_FAST) ||
      pthread_cond_init(&log_descriptor.log_flush_cond, 0) ||
      my_rwlock_init(&log_descriptor.open_files_lock,
                     NULL) ||
      my_init_dynamic_array(&log_descriptor.open_files,
                            sizeof(TRANSLOG_FILE*), 10, 10) ||
      my_init_dynamic_array(&log_descriptor.unfinished_files,
                            sizeof(struct st_file_counter),
                            10, 10))
    goto err;
  log_descriptor.min_need_file= 0;
  log_descriptor.min_file_number= 0;
  log_descriptor.last_lsn_checked= LSN_IMPOSSIBLE;

  /* Directory to store files */
  unpack_dirname(log_descriptor.directory, directory);
#ifndef __WIN__
  if ((log_descriptor.directory_fd= my_open(log_descriptor.directory,
                                            O_RDONLY, MYF(MY_WME))) < 0)
  {
    my_errno= errno;
    DBUG_PRINT("error", ("Error %d during opening directory '%s'",
                         errno, log_descriptor.directory));
    goto err;
  }
#endif
  log_descriptor.in_buffers_only= LSN_IMPOSSIBLE;
  DBUG_ASSERT(log_file_max_size % TRANSLOG_PAGE_SIZE == 0 &&
              log_file_max_size >= TRANSLOG_MIN_FILE_SIZE);
  /* max size of one log size (for new logs creation) */
  log_file_size= log_descriptor.log_file_max_size=
    log_file_max_size;
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
  translog_fill_overhead_table();
  log_descriptor.page_overhead= page_overhead[flags];
  log_descriptor.page_capacity_chunk_2=
    TRANSLOG_PAGE_SIZE - log_descriptor.page_overhead - 1;
  compile_time_assert(TRANSLOG_WRITE_BUFFER % TRANSLOG_PAGE_SIZE == 0);
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

  /* Just to init it somehow (hack for bootstrap)*/
  {
    TRANSLOG_FILE *file= 0;
    log_descriptor.min_file = log_descriptor.max_file= 1;
    insert_dynamic(&log_descriptor.open_files, (uchar *)&file);
    translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
    pop_dynamic(&log_descriptor.open_files);
  }

  /* Buffers for log writing */
  for (i= 0; i < TRANSLOG_BUFFERS_NO; i++)
  {
    if (translog_buffer_init(log_descriptor.buffers + i, i))
      goto err;
    DBUG_PRINT("info", ("translog_buffer buffer #%u: 0x%lx",
                        i, (ulong) log_descriptor.buffers + i));
  }

  /*
    last_logno and last_checkpoint_lsn were set in
    ma_control_file_create_or_open()
  */
  logs_found= (last_logno != FILENO_IMPOSSIBLE);

  translog_status= (readonly ? TRANSLOG_READONLY : TRANSLOG_OK);
  checkpoint_lsn= last_checkpoint_lsn;

  if (logs_found)
  {
    my_bool pageok;
    DBUG_PRINT("info", ("log found..."));
    /*
      TODO: scan directory for maria_log.XXXXXXXX files and find
       highest XXXXXXXX & set logs_found
      TODO: check that last checkpoint within present log addresses space

      find the log end
    */
    if (LSN_FILE_NO(last_checkpoint_lsn) == FILENO_IMPOSSIBLE)
    {
      DBUG_ASSERT(LSN_OFFSET(last_checkpoint_lsn) == 0);
      /* only last log needs to be checked */
      sure_page= MAKE_LSN(last_logno, TRANSLOG_PAGE_SIZE);
    }
    else
    {
      sure_page= last_checkpoint_lsn;
      DBUG_ASSERT(LSN_OFFSET(sure_page) % TRANSLOG_PAGE_SIZE != 0);
      sure_page-= LSN_OFFSET(sure_page) % TRANSLOG_PAGE_SIZE;
    }
    /* Set horizon to the beginning of the last file first */
    log_descriptor.horizon= last_page= MAKE_LSN(last_logno, 0);
    if (translog_get_last_page_addr(&last_page, &pageok, no_errors))
    {
      if (!translog_walk_filenames(log_descriptor.directory,
                                   &translog_callback_search_first))
      {
        /*
          Files was deleted, just start from the next log number, so that
          existing tables are in the past.
        */
        start_file_num= last_logno + 1;
        checkpoint_lsn= LSN_IMPOSSIBLE; /* no log so no checkpoint */
        logs_found= 0;
      }
      else
        goto err;
    }
    else if (LSN_OFFSET(last_page) == 0)
    {
      if (LSN_FILE_NO(last_page) == 1)
      {
        logs_found= 0;                          /* file #1 has no pages */
        DBUG_PRINT("info", ("log found. But is is empty => no log assumed"));
      }
      else
      {
        last_page-= LSN_ONE_FILE;
        if (translog_get_last_page_addr(&last_page, &pageok, 0))
          goto err;
      }
    }
    if (logs_found)
    {
      uint32 i;
      log_descriptor.min_file= translog_first_file(log_descriptor.horizon, 1);
      log_descriptor.max_file= last_logno;
      /* Open all files */
      if (allocate_dynamic(&log_descriptor.open_files,
                           log_descriptor.max_file -
                           log_descriptor.min_file + 1))
        goto err;
      for (i = log_descriptor.max_file; i >= log_descriptor.min_file; i--)
      {
        /*
          We can't allocate all file together because they will be freed
          one by one
        */
        TRANSLOG_FILE *file= (TRANSLOG_FILE *)my_malloc(sizeof(TRANSLOG_FILE),
                                                        MYF(0));

        compile_time_assert(MY_FILEPOS_ERROR > ULL(0xffffffff));
        if (file == NULL ||
            (file->handler.file=
             open_logfile_by_number_no_cache(i)) < 0 ||
            my_seek(file->handler.file, 0, SEEK_END, MYF(0)) >=
            ULL(0xffffffff))
        {
          int j;
          for (j= i - log_descriptor.min_file - 1; j > 0; j--)
          {
            TRANSLOG_FILE *el=
              *dynamic_element(&log_descriptor.open_files, j,
                               TRANSLOG_FILE **);
            my_close(el->handler.file, MYF(MY_WME));
            my_free(el, MYF(0));
          }
          if (file)
          {
            free(file);
            goto err;
          }
          else
            goto err;
        }
        translog_file_init(file, i, 1);
        /* we allocated space so it can't fail */
        insert_dynamic(&log_descriptor.open_files, (uchar *)&file);
      }
      DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
                  log_descriptor.open_files.elements);
    }
  }
  else if (readonly)
  {
    /* There is no logs and there is read-only mode => nothing to read */
    DBUG_PRINT("error", ("No logs and read-only mode"));
    goto err;
  }

  if (logs_found)
  {
    TRANSLOG_ADDRESS current_page= sure_page;
    my_bool pageok;

    DBUG_PRINT("info", ("The log is really present"));
    DBUG_ASSERT(sure_page <= last_page);

    /* TODO: check page size */

    last_valid_page= LSN_IMPOSSIBLE;
    /*
      Scans and validate pages. We need it to show "outside" only for sure
      valid part of the log. If the log was damaged then fixed we have to
      cut off damaged part before some other process start write something
      in the log.
    */
    do
    {
      TRANSLOG_ADDRESS current_file_last_page;
      current_file_last_page= current_page;
      if (translog_get_last_page_addr(&current_file_last_page, &pageok, 0))
        goto err;
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
        TRANSLOG_PAGE_SIZE_BUFF psize_buff;
        uchar *page;
        data.addr= &current_page;
        if ((page= translog_get_page(&data, psize_buff.buffer, NULL)) == NULL)
          goto err;
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
    if (last_valid_page == LSN_IMPOSSIBLE)
    {
      /* Panic!!! Even page which should be valid is invalid */
      /* TODO: issue error */
      goto err;
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
      TRANSLOG_PAGE_SIZE_BUFF psize_buff;
      uchar *page;
      uint16 chunk_offset;
      data.addr= &last_valid_page;
      /* continue old log */
      DBUG_ASSERT(LSN_FILE_NO(last_valid_page)==
                  LSN_FILE_NO(log_descriptor.horizon));
      if ((page= translog_get_page(&data, psize_buff.buffer, NULL)) == NULL ||
          (chunk_offset= translog_get_first_chunk_offset(page)) == 0)
        goto err;

      /* Puts filled part of old page in the buffer */
      log_descriptor.horizon= last_valid_page;
      translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
      /*
         Free space if filled with TRANSLOG_FILLER and first uchar of
         real chunk can't be TRANSLOG_FILLER
      */
      while (chunk_offset < TRANSLOG_PAGE_SIZE &&
             page[chunk_offset] != TRANSLOG_FILLER)
      {
        uint16 chunk_length;
        if ((chunk_length=
             translog_get_total_chunk_length(page, chunk_offset)) == 0)
          goto err;
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
      translog_check_cursor(&log_descriptor.bc);
    }
    if (!old_log_was_recovered && old_flags == flags)
    {
      LOGHANDLER_FILE_INFO info;
      LINT_INIT(info.maria_version);

      /*
        Accessing &log_descriptor.open_files without mutex is safe
        because it is initialization
      */
      if (translog_read_file_header(&info,
                                    (*dynamic_element(&log_descriptor.
                                                      open_files,
                                                      0, TRANSLOG_FILE **))->
                                    handler.file))
        goto err;
      version_changed= (info.maria_version != TRANSLOG_VERSION_ID);
    }
  }
  DBUG_PRINT("info", ("Logs found: %d  was recovered: %d",
                      logs_found, old_log_was_recovered));
  if (!logs_found)
  {
    TRANSLOG_FILE *file= (TRANSLOG_FILE*)my_malloc(sizeof(TRANSLOG_FILE),
                                                   MYF(0));
    DBUG_PRINT("info", ("The log is not found => we will create new log"));
    if (file == NULL)
       goto err;
    /* Start new log system from scratch */
    log_descriptor.horizon= MAKE_LSN(start_file_num,
                                     TRANSLOG_PAGE_SIZE); /* header page */
    if ((file->handler.file=
         create_logfile_by_number_no_cache(start_file_num)) == -1)
      goto err;
    translog_file_init(file, start_file_num, 0);
    if (insert_dynamic(&log_descriptor.open_files, (uchar*)&file))
      goto err;
    log_descriptor.min_file= log_descriptor.max_file= start_file_num;
    if (translog_write_file_header())
      goto err;
    DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
                log_descriptor.open_files.elements);

    if (ma_control_file_write_and_force(checkpoint_lsn, start_file_num,
                                        max_trid_in_control_file,
                                        recovery_failures))
      goto err;
    /* assign buffer 0 */
    translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
    translog_new_page_header(&log_descriptor.horizon, &log_descriptor.bc);
  }
  else if ((old_log_was_recovered || old_flags != flags || version_changed) &&
           !readonly)
  {
    /* leave the damaged file untouched */
    log_descriptor.horizon+= LSN_ONE_FILE;
    /* header page */
    log_descriptor.horizon= LSN_REPLACE_OFFSET(log_descriptor.horizon,
                                               TRANSLOG_PAGE_SIZE);
    if (translog_create_new_file())
      goto err;
    /*
      Buffer system left untouched after recovery => we should init it
      (starting from buffer 0)
    */
    translog_start_buffer(log_descriptor.buffers, &log_descriptor.bc, 0);
    translog_new_page_header(&log_descriptor.horizon, &log_descriptor.bc);
  }

  /* all LSNs that are on disk are flushed */
  log_descriptor.log_start= log_descriptor.sent_to_disk=
    log_descriptor.flushed= log_descriptor.horizon;
  log_descriptor.in_buffers_only= log_descriptor.bc.buffer->offset;
  log_descriptor.max_lsn= LSN_IMPOSSIBLE; /* set to 0 */
  log_descriptor.previous_flush_horizon= log_descriptor.horizon;
  /*
    Now 'flushed' is set to 'horizon' value, but 'horizon' is (potentially)
    address of the next LSN and we want indicate that all LSNs that are
    already on the disk are flushed so we need decrease horizon on 1 (we are
    sure that there is no LSN on the disk which is greater then 'flushed'
    and there will not be LSN created that is equal or less then the value
    of the 'flushed').
  */
  log_descriptor.flushed--; /* offset decreased */
  log_descriptor.sent_to_disk--; /* offset decreased */
  /*
    Log records will refer to a MARIA_SHARE by a unique 2-byte id; set up
    structures for generating 2-byte ids:
  */
  my_atomic_rwlock_init(&LOCK_id_to_share);
  id_to_share= (MARIA_SHARE **) my_malloc(SHARE_ID_MAX * sizeof(MARIA_SHARE*),
                                          MYF(MY_WME | MY_ZEROFILL));
  if (unlikely(!id_to_share))
    goto err;
  id_to_share--; /* min id is 1 */

  /* Check the last LSN record integrity */
  if (logs_found)
  {
    TRANSLOG_SCANNER_DATA scanner;
    TRANSLOG_ADDRESS page_addr;
    LSN last_lsn= LSN_IMPOSSIBLE;
    /*
      take very last page address and try to find LSN record on it
      if it fail take address of previous page and so on
    */
    page_addr= (log_descriptor.horizon -
                ((log_descriptor.horizon - 1) % TRANSLOG_PAGE_SIZE + 1));
    if (translog_scanner_init(page_addr, 1, &scanner, 1))
      goto err;
    scanner.page_offset= page_overhead[scanner.page[TRANSLOG_PAGE_FLAGS]];
    for (;;)
    {
      uint chunk_1byte;
      chunk_1byte= scanner.page[scanner.page_offset];
      while (!translog_is_LSN_chunk(chunk_1byte) &&
             scanner.page != END_OF_LOG &&
             scanner.page[scanner.page_offset] != TRANSLOG_FILLER &&
             scanner.page_addr == page_addr)
      {
        if (translog_get_next_chunk(&scanner))
        {
          translog_destroy_scanner(&scanner);
          goto err;
        }
        if (scanner.page != END_OF_LOG)
          chunk_1byte= scanner.page[scanner.page_offset];
      }
      if (translog_is_LSN_chunk(chunk_1byte))
      {
        last_lsn= scanner.page_addr + scanner.page_offset;
        if (translog_get_next_chunk(&scanner))
        {
          translog_destroy_scanner(&scanner);
          goto err;
        }
        if (scanner.page == END_OF_LOG)
          break; /* it was the last record */
        chunk_1byte= scanner.page[scanner.page_offset];
        continue; /* try to find other record on this page */
      }

      if (last_lsn != LSN_IMPOSSIBLE)
        break; /* there is no more records on the page */

      /* We have to make step back */
      if (unlikely(LSN_OFFSET(page_addr) == TRANSLOG_PAGE_SIZE))
      {
        uint32 file_no= LSN_FILE_NO(page_addr);
        my_bool last_page_ok;
        /* it is beginning of the current file */
        if (unlikely(file_no == 1))
        {
          /*
            It is beginning of the log => there is no LSNs in the log =>
            There is no harm in leaving it "as-is".
          */
          DBUG_RETURN(0);
        }
        file_no--;
        page_addr= MAKE_LSN(file_no, TRANSLOG_PAGE_SIZE);
        translog_get_last_page_addr(&page_addr, &last_page_ok, 0);
        /* page should be OK as it is not the last file */
        DBUG_ASSERT(last_page_ok);
      }
      else
      {
         page_addr-= TRANSLOG_PAGE_SIZE;
      }
      translog_destroy_scanner(&scanner);
      if (translog_scanner_init(page_addr, 1, &scanner, 1))
        goto err;
      scanner.page_offset= page_overhead[scanner.page[TRANSLOG_PAGE_FLAGS]];
    }
    translog_destroy_scanner(&scanner);

    /* Now scanner points to the last LSN chunk, lets check it */
    {
      TRANSLOG_HEADER_BUFFER rec;
      translog_size_t rec_len;
      int len;
      uchar buffer[1];
      DBUG_PRINT("info", ("going to check the last found record (%lu,0x%lx)",
                          LSN_IN_PARTS(last_lsn)));

      len=
        translog_read_record_header(last_lsn, &rec);
      if (unlikely (len == RECHEADER_READ_ERROR ||
                    len == RECHEADER_READ_EOF))
      {
        DBUG_PRINT("error", ("unexpected end of log or record during "
                             "reading record header: (%lu,0x%lx)  len: %d",
                             LSN_IN_PARTS(last_lsn), len));
        if (readonly)
          log_descriptor.log_start= log_descriptor.horizon= last_lsn;
        else if (translog_truncate_log(last_lsn))
        {
          translog_free_record_header(&rec);
          goto err;
        }
      }
      else
      {
        DBUG_ASSERT(last_lsn == rec.lsn);
        if (likely(rec.record_length != 0))
        {
          /*
            Reading the last byte of record will trigger scanning all
            record chunks for now
          */
          rec_len= translog_read_record(rec.lsn, rec.record_length - 1, 1,
                                        buffer, NULL);
          if (rec_len != 1)
          {
            DBUG_PRINT("error", ("unexpected end of log or record during "
                                 "reading record body: (%lu,0x%lx)  len: %d",
                                 LSN_IN_PARTS(rec.lsn),
                                 len));
            if (readonly)
              log_descriptor.log_start= log_descriptor.horizon= last_lsn;

            else if (translog_truncate_log(last_lsn))
            {
              translog_free_record_header(&rec);
              goto err;
            }
          }
        }
      }
      translog_free_record_header(&rec);
    }
  }
  DBUG_RETURN(0);
err:
  ma_message_no_user(0, "log initialization failed");
  DBUG_RETURN(1);
}


/*
  @brief Free transaction log file buffer.

  @param buffer_no       The buffer to free
*/

static void translog_buffer_destroy(struct st_translog_buffer *buffer)
{
  DBUG_ENTER("translog_buffer_destroy");
  DBUG_PRINT("enter",
             ("Buffer #%u: 0x%lx  file: %d  offset: (%lu,0x%lx)  size: %lu",
              (uint) buffer->buffer_no, (ulong) buffer,
              (buffer->file ? buffer->file->handler.file : -1),
              LSN_IN_PARTS(buffer->offset),
              (ulong) buffer->size));
  if (buffer->file != NULL)
  {
    /*
      We ignore errors here, because we can't do something about it
      (it is shutting down)

      We also have to take the locks even if there can't be any other
      threads running, because translog_buffer_flush()
      requires that we have the buffer locked.
    */
    translog_buffer_lock(buffer);
    translog_buffer_flush(buffer);
    translog_buffer_unlock(buffer);
  }
  DBUG_PRINT("info", ("Destroy mutex: 0x%lx", (ulong) &buffer->mutex));
  pthread_mutex_destroy(&buffer->mutex);
  pthread_cond_destroy(&buffer->waiting_filling_buffer);
  DBUG_VOID_RETURN;
}


/*
  Free log handler resources

  SYNOPSIS
    translog_destroy()
*/

void translog_destroy()
{
  TRANSLOG_FILE **file;
  uint i;
  uint8 current_buffer;
  DBUG_ENTER("translog_destroy");

  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  translog_lock();
  current_buffer= log_descriptor.bc.buffer_no;
  translog_status= (translog_status == TRANSLOG_READONLY ?
                    TRANSLOG_UNINITED :
                    TRANSLOG_SHUTDOWN);
  if (log_descriptor.bc.buffer->file != NULL)
    translog_finish_page(&log_descriptor.horizon, &log_descriptor.bc);
  translog_unlock();

  for (i= 0; i < TRANSLOG_BUFFERS_NO; i++)
  {
    struct st_translog_buffer *buffer= (log_descriptor.buffers +
                                        ((i + current_buffer + 1) %
                                         TRANSLOG_BUFFERS_NO));
    translog_buffer_destroy(buffer);
  }
  translog_status= TRANSLOG_UNINITED;

  /* close files */
  while ((file= (TRANSLOG_FILE **)pop_dynamic(&log_descriptor.open_files)))
    translog_close_log_file(*file);
  pthread_mutex_destroy(&log_descriptor.sent_to_disk_lock);
  pthread_mutex_destroy(&log_descriptor.file_header_lock);
  pthread_mutex_destroy(&log_descriptor.unfinished_files_lock);
  pthread_mutex_destroy(&log_descriptor.purger_lock);
  pthread_mutex_destroy(&log_descriptor.log_flush_lock);
  pthread_mutex_destroy(&log_descriptor.dirty_buffer_mask_lock);
  pthread_cond_destroy(&log_descriptor.log_flush_cond);
  rwlock_destroy(&log_descriptor.open_files_lock);
  delete_dynamic(&log_descriptor.open_files);
  delete_dynamic(&log_descriptor.unfinished_files);

  if (log_descriptor.directory_fd >= 0)
    my_close(log_descriptor.directory_fd, MYF(MY_WME));
  my_atomic_rwlock_destroy(&LOCK_id_to_share);
  if (id_to_share != NULL)
    my_free((id_to_share + 1), MYF(MY_WME));
  DBUG_VOID_RETURN;
}


/*
  @brief Starts new page.

  @param horizon         \ Position in file and buffer where we are
  @param cursor          /
  @param prev_buffer     Buffer which should be flushed will be assigned here.
                         This is always set (to NULL if nothing to flush).

  @note We do not want to flush the buffer immediately because we want to
  let caller of this function first advance 'horizon' pointer and unlock the
  loghandler and only then flush the log which can take some time.

  @retval 0 OK
  @retval 1 Error
*/

static my_bool translog_page_next(TRANSLOG_ADDRESS *horizon,
                                  struct st_buffer_cursor *cursor,
                                  struct st_translog_buffer **prev_buffer)
{
  struct st_translog_buffer *buffer= cursor->buffer;
  DBUG_ENTER("translog_page_next");

  *prev_buffer= NULL;
  if ((cursor->ptr + TRANSLOG_PAGE_SIZE >
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
                                           uchar *buffer)
{
  DBUG_ENTER("translog_write_data_on_page");
  DBUG_PRINT("enter", ("Chunk length: %lu  Page size %u",
                       (ulong) length, (uint) cursor->current_page_fill));
  DBUG_ASSERT(length > 0);
  DBUG_ASSERT(length + cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(length + cursor->ptr <= cursor->buffer->buffer +
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
  translog_check_cursor(cursor);

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
                       (uint) (cur + 1), (uint) parts->elements,
                       (uint) cursor->current_page_fill,
                       (ulong) cursor->buffer->size,
                       (ulong) (cursor->ptr - cursor->buffer->buffer)));
  DBUG_ASSERT(length > 0);
  DBUG_ASSERT(length + cursor->current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_ASSERT(length + cursor->ptr <= cursor->buffer->buffer +
              TRANSLOG_WRITE_BUFFER);

  do
  {
    translog_size_t len;
    LEX_CUSTRING *part;
    const uchar *buff;

    DBUG_ASSERT(cur < parts->elements);
    part= parts->parts + cur;
    buff= part->str;
    DBUG_PRINT("info", ("Part: %u  Length: %lu  left: %lu  buff: 0x%lx",
                        (uint) (cur + 1), (ulong) part->length, (ulong) left,
                        (ulong) buff));

    if (part->length > left)
    {
      /* we should write less then the current part */
      len= left;
      part->length-= len;
      part->str+= len;
      DBUG_PRINT("info", ("Set new part: %u  Length: %lu",
                          (uint) (cur + 1), (ulong) part->length));
    }
    else
    {
      len= (translog_size_t) part->length;
      cur++;
      DBUG_PRINT("info", ("moved to next part (len: %lu)", (ulong) len));
    }
    DBUG_PRINT("info", ("copy: 0x%lx <- 0x%lx  %u",
                        (ulong) cursor->ptr, (ulong)buff, (uint)len));
    if (likely(len))
    {
      memcpy(cursor->ptr, buff, len);
      left-= len;
      cursor->ptr+= len;
    }
  } while (left);

  DBUG_PRINT("info", ("Horizon: (%lu,0x%lx)  Length %lu(0x%lx)",
                      LSN_IN_PARTS(*horizon),
                      (ulong) length, (ulong) length));
  parts->current= cur;
  (*horizon)+= length; /* offset increasing */
  cursor->current_page_fill+= length;
  if (!cursor->chaser)
    cursor->buffer->size+= length;
  /*
    We do not not updating parts->total_record_length here because it is
    need only before writing record to have total length
  */
  DBUG_PRINT("info", ("Write parts buffer #%u: 0x%lx  "
                      "chaser: %d  Size: %lu (%lu)  "
                      "Horizon: (%lu,0x%lx)  buff offset: 0x%lx",
                      (uint) cursor->buffer->buffer_no, (ulong) cursor->buffer,
                      cursor->chaser, (ulong) cursor->buffer->size,
                      (ulong) (cursor->ptr - cursor->buffer->buffer),
                      LSN_IN_PARTS(*horizon),
                      (ulong) (LSN_OFFSET(cursor->buffer->offset) +
                               cursor->buffer->size)));
  translog_check_cursor(cursor);

  DBUG_RETURN(0);
}


/*
  Put 1 group chunk type 0 header into parts array

  SYNOPSIS
    translog_write_variable_record_1group_header()
    parts                Descriptor of record source parts
    type                 The log record type
    short_trid           Short transaction ID or 0 if it has no sense
    header_length        Calculated header length of chunk type 0
    chunk0_header        Buffer for the chunk header writing
*/

static void
translog_write_variable_record_1group_header(struct st_translog_parts *parts,
                                             enum translog_record_type type,
                                             SHORT_TRANSACTION_ID short_trid,
                                             uint16 header_length,
                                             uchar *chunk0_header)
{
  LEX_CUSTRING *part;
  DBUG_ASSERT(parts->current != 0);     /* first part is left for header */
  part= parts->parts + (--parts->current);
  parts->total_record_length+= (translog_size_t) (part->length= header_length);
  part->str= chunk0_header;
  /* puts chunk type */
  *chunk0_header= (uchar) (type | TRANSLOG_CHUNK_LSN);
  int2store(chunk0_header + 1, short_trid);
  /* puts record length */
  translog_write_variable_record_1group_code_len(chunk0_header + 3,
                                                 parts->record_length,
                                                 header_length);
  /* puts 0 as chunk length which indicate 1 group record */
  int2store(chunk0_header + header_length - 2, 0);
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
  translog_buffer_lock_assert_owner(buffer);
  buffer->copy_to_buffer_in_progress++;
  DBUG_PRINT("info", ("copy_to_buffer_in_progress. Buffer #%u  0x%lx  progress: %d",
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
  translog_buffer_lock_assert_owner(buffer);
  buffer->copy_to_buffer_in_progress--;
  DBUG_PRINT("info",
             ("copy_to_buffer_in_progress. Buffer #%u  0x%lx  progress: %d",
              (uint) buffer->buffer_no, (ulong) buffer,
              buffer->copy_to_buffer_in_progress));
  if (buffer->copy_to_buffer_in_progress == 0)
    pthread_cond_broadcast(&buffer->waiting_filling_buffer);
  DBUG_VOID_RETURN;
}


/**
  @brief Skip to the next page for chaser (thread which advanced horizon
  pointer and now feeling the buffer)

  @param horizon         \ Pointers on file position and buffer
  @param cursor          /

  @retval 1 OK
  @retval 0 Error
*/

static my_bool translog_chaser_page_next(TRANSLOG_ADDRESS *horizon,
                                         struct st_buffer_cursor *cursor)
{
  struct st_translog_buffer *buffer_to_flush;
  my_bool rc;
  DBUG_ENTER("translog_chaser_page_next");
  DBUG_ASSERT(cursor->chaser);
  rc= translog_page_next(horizon, cursor, &buffer_to_flush);
  if (buffer_to_flush != NULL)
  {
    translog_buffer_lock(buffer_to_flush);
    translog_buffer_decrease_writers(buffer_to_flush);
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    translog_buffer_unlock(buffer_to_flush);
  }
  DBUG_RETURN(rc);
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
  uchar chunk2_header[1];
  DBUG_ENTER("translog_write_variable_record_chunk2_page");
  chunk2_header[0]= TRANSLOG_CHUNK_NOHDR;

  if (translog_chaser_page_next(horizon, cursor))
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
  LEX_CUSTRING *part;
  uchar chunk3_header[1 + 2];
  DBUG_ENTER("translog_write_variable_record_chunk3_page");

  if (translog_chaser_page_next(horizon, cursor))
    DBUG_RETURN(1);

  if (length == 0)
  {
    /* It was call to write page header only (no data for chunk 3) */
    DBUG_PRINT("info", ("It is a call to make page header only"));
    DBUG_RETURN(0);
  }

  DBUG_ASSERT(parts->current != 0);       /* first part is left for header */
  part= parts->parts + (--parts->current);
  parts->total_record_length+= (translog_size_t) (part->length= 1 + 2);
  part->str= chunk3_header;
  /* Puts chunk type */
  *chunk3_header= (uchar) (TRANSLOG_CHUNK_LNGTH);
  /* Puts chunk length */
  int2store(chunk3_header + 1, length);

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

static my_bool translog_advance_pointer(int pages, uint16 last_page_data)
{
  translog_size_t last_page_offset= (log_descriptor.page_overhead +
                                     last_page_data);
  translog_size_t offset= (TRANSLOG_PAGE_SIZE -
                           log_descriptor.bc.current_page_fill +
                           pages * TRANSLOG_PAGE_SIZE + last_page_offset);
  translog_size_t buffer_end_offset, file_end_offset, min_offset;
  DBUG_ENTER("translog_advance_pointer");
  DBUG_PRINT("enter", ("Pointer:  (%lu, 0x%lx) + %u + %u pages + %u + %u",
                       LSN_IN_PARTS(log_descriptor.horizon),
                       (uint) (TRANSLOG_PAGE_SIZE -
                               log_descriptor.bc.current_page_fill),
                       pages, (uint) log_descriptor.page_overhead,
                       (uint) last_page_data));
  translog_lock_assert_owner();

  if (pages == -1)
  {
    /*
      It is special case when we advance the pointer on the same page.
      It can happened when we write last part of multi-group record.
    */
    DBUG_ASSERT(last_page_data + log_descriptor.bc.current_page_fill <=
                TRANSLOG_PAGE_SIZE);
    offset= last_page_data;
    last_page_offset= log_descriptor.bc.current_page_fill + last_page_data;
    goto end;
  }
  DBUG_PRINT("info", ("last_page_offset %lu", (ulong) last_page_offset));
  DBUG_ASSERT(last_page_offset <= TRANSLOG_PAGE_SIZE);

  /*
    The loop will be executed 1-3 times. Usually we advance the
    pointer to fill only the current buffer (if we have more then 1/2 of
    buffer free or 2 buffers (rest of current and all next). In case of
    really huge record end where we write last group with "table of
    content" of all groups and ignore buffer borders we can occupy
    3 buffers.
  */
  for (;;)
  {
    uint8 new_buffer_no;
    struct st_translog_buffer *new_buffer;
    struct st_translog_buffer *old_buffer;
    buffer_end_offset= TRANSLOG_WRITE_BUFFER - log_descriptor.bc.buffer->size;
    if (likely(log_descriptor.log_file_max_size >=
               LSN_OFFSET(log_descriptor.horizon)))
      file_end_offset= (log_descriptor.log_file_max_size -
                        LSN_OFFSET(log_descriptor.horizon));
    else
    {
      /*
        We already have written more then current file limit allow,
        So we will finish this page and start new file
      */
      file_end_offset= (TRANSLOG_PAGE_SIZE -
                        log_descriptor.bc.current_page_fill);
    }
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
#ifndef DBUG_OFF
    {
      TRANSLOG_ADDRESS offset= new_buffer->offset;
      TRANSLOG_FILE *file= new_buffer->file;
      uint8 ver= new_buffer->ver;
      translog_lock_assert_owner();
#endif
      translog_wait_for_buffer_free(new_buffer);
#ifndef DBUG_OFF
      /* We keep the handler locked so nobody can start this new buffer */
      DBUG_ASSERT(offset == new_buffer->offset && new_buffer->file == NULL &&
                  (file == NULL ? ver : (uint8)(ver + 1)) == new_buffer->ver);
    }
#endif

    min_offset= min(buffer_end_offset, file_end_offset);
    /* TODO: check is it ptr or size enough */
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
    old_buffer->next_buffer_offset= new_buffer->offset;
    new_buffer->prev_buffer_offset= old_buffer->offset;
    translog_buffer_unlock(old_buffer);
    offset-= min_offset;
  }
  DBUG_PRINT("info", ("drop write_counter"));
  log_descriptor.bc.write_counter= 0;
  log_descriptor.bc.previous_offset= 0;
end:
  log_descriptor.bc.ptr+= offset;
  log_descriptor.bc.buffer->size+= offset;
  translog_buffer_increase_writers(log_descriptor.bc.buffer);
  log_descriptor.horizon+= offset; /* offset increasing */
  log_descriptor.bc.current_page_fill= last_page_offset;
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
              LSN_IN_PARTS(log_descriptor.horizon)));
  translog_check_cursor(&log_descriptor.bc);
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

static uint translog_get_current_page_rest()
{
  return (TRANSLOG_PAGE_SIZE - log_descriptor.bc.current_page_fill);
}


/*
  Get buffer rest in full pages

  SYNOPSIS
     translog_get_current_buffer_rest()

  NOTE loghandler should be locked

  RETURN
    number of full pages left on the current buffer
*/

static uint translog_get_current_buffer_rest()
{
  return ((log_descriptor.bc.buffer->buffer + TRANSLOG_WRITE_BUFFER -
           log_descriptor.bc.ptr) /
          TRANSLOG_PAGE_SIZE);
}

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


static inline void set_lsn(LSN *lsn, LSN value)
{
  DBUG_ENTER("set_lsn");
  translog_lock_assert_owner();
  *lsn= value;
  /* we generate LSN so something is not flushed in log */
  log_descriptor.is_everything_flushed= 0;
  DBUG_PRINT("info", ("new LSN appeared: (%lu,0x%lx)", LSN_IN_PARTS(value)));
  DBUG_VOID_RETURN;
}


/**
   @brief Write variable record in 1 group.

   @param  lsn             LSN of the record will be written here
   @param  type            the log record type
   @param  short_trid      Short transaction ID or 0 if it has no sense
   @param  parts           Descriptor of record source parts
   @param  buffer_to_flush Buffer which have to be flushed if it is not 0
   @param  header_length   Calculated header length of chunk type 0
   @param  trn             Transaction structure pointer for hooks by
                           record log type, for short_id
   @param  hook_arg        Argument which will be passed to pre-write and
                           in-write hooks of this record.

   @note
     We must have a translog_lock() when entering this function
     We must have buffer_to_flush locked (if not null)

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

static my_bool
translog_write_variable_record_1group(LSN *lsn,
                                      enum translog_record_type type,
                                      MARIA_HA *tbl_info,
                                      SHORT_TRANSACTION_ID short_trid,
                                      struct st_translog_parts *parts,
                                      struct st_translog_buffer
                                      *buffer_to_flush, uint16 header_length,
                                      TRN *trn, void *hook_arg)
{
  TRANSLOG_ADDRESS horizon;
  struct st_buffer_cursor cursor;
  int rc= 0;
  uint i;
  translog_size_t record_rest, full_pages, first_page;
  uint additional_chunk3_page= 0;
  uchar chunk0_header[1 + 2 + 5 + 2];
  DBUG_ENTER("translog_write_variable_record_1group");
  translog_lock_assert_owner();
  if (buffer_to_flush)
    translog_buffer_lock_assert_owner(buffer_to_flush);

  set_lsn(lsn, horizon= log_descriptor.horizon);
  if (translog_set_lsn_for_files(LSN_FILE_NO(*lsn), LSN_FILE_NO(*lsn),
                                 *lsn, TRUE) ||
      (log_record_type_descriptor[type].inwrite_hook &&
       (*log_record_type_descriptor[type].inwrite_hook)(type, trn, tbl_info,
                                                        lsn, hook_arg)))
  {
    translog_unlock();
    DBUG_RETURN(1);
  }
  cursor= log_descriptor.bc;
  cursor.chaser= 1;

  /* Advance pointer to be able unlock the loghandler */
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
  rc|= translog_advance_pointer((int)(full_pages + additional_chunk3_page),
                                (record_rest ? record_rest + 3 : 0));
  log_descriptor.bc.buffer->last_lsn= *lsn;
  DBUG_PRINT("info", ("last_lsn set to (%lu,0x%lx)  buffer: 0x%lx",
                      LSN_IN_PARTS(log_descriptor.bc.buffer->last_lsn),
                      (ulong) log_descriptor.bc.buffer));

  translog_unlock();

  /*
     Check if we switched buffer and need process it (current buffer is
     unlocked already => we will not delay other threads
  */
  if (buffer_to_flush != NULL)
  {
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    translog_buffer_unlock(buffer_to_flush);
  }
  if (rc)
    DBUG_RETURN(1);

  translog_write_variable_record_1group_header(parts, type, short_trid,
                                               header_length, chunk0_header);

  /* fill the pages */
  translog_write_parts_on_page(&horizon, &cursor, first_page, parts);

  DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                      LSN_IN_PARTS(log_descriptor.horizon),
                      LSN_IN_PARTS(horizon)));

  for (i= 0; i < full_pages; i++)
  {
    if (translog_write_variable_record_chunk2_page(parts, &horizon, &cursor))
      DBUG_RETURN(1);

    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                        LSN_IN_PARTS(log_descriptor.horizon),
                        LSN_IN_PARTS(horizon)));
  }

  if (additional_chunk3_page)
  {
    if (translog_write_variable_record_chunk3_page(parts,
                                                   log_descriptor.
                                                   page_capacity_chunk_2 - 2,
                                                   &horizon, &cursor))
      DBUG_RETURN(1);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)",
                        LSN_IN_PARTS(log_descriptor.horizon),
                        LSN_IN_PARTS(horizon)));
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

  translog_buffer_lock(cursor.buffer);
  translog_buffer_decrease_writers(cursor.buffer);
  translog_buffer_unlock(cursor.buffer);
  DBUG_RETURN(rc);
}


/**
   @brief Write variable record in 1 chunk.

   @param  lsn             LSN of the record will be written here
   @param  type            the log record type
   @param  short_trid      Short transaction ID or 0 if it has no sense
   @param  parts           Descriptor of record source parts
   @param  buffer_to_flush Buffer which have to be flushed if it is not 0
   @param  header_length   Calculated header length of chunk type 0
   @param  trn             Transaction structure pointer for hooks by
                           record log type, for short_id
   @param  hook_arg        Argument which will be passed to pre-write and
                           in-write hooks of this record.

   @note
     We must have a translog_lock() when entering this function
     We must have buffer_to_flush locked (if not null)

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

static my_bool
translog_write_variable_record_1chunk(LSN *lsn,
                                      enum translog_record_type type,
                                      MARIA_HA *tbl_info,
                                      SHORT_TRANSACTION_ID short_trid,
                                      struct st_translog_parts *parts,
                                      struct st_translog_buffer
                                      *buffer_to_flush, uint16 header_length,
                                      TRN *trn, void *hook_arg)
{
  int rc;
  uchar chunk0_header[1 + 2 + 5 + 2];
  DBUG_ENTER("translog_write_variable_record_1chunk");
  translog_lock_assert_owner();
  if (buffer_to_flush)
    translog_buffer_lock_assert_owner(buffer_to_flush);

  translog_write_variable_record_1group_header(parts, type, short_trid,
                                               header_length, chunk0_header);
  set_lsn(lsn, log_descriptor.horizon);
  if (translog_set_lsn_for_files(LSN_FILE_NO(*lsn), LSN_FILE_NO(*lsn),
                                 *lsn, TRUE) ||
      (log_record_type_descriptor[type].inwrite_hook &&
       (*log_record_type_descriptor[type].inwrite_hook)(type, trn, tbl_info,
                                                        lsn, hook_arg)))
  {
    translog_unlock();
    DBUG_RETURN(1);
  }

  rc= translog_write_parts_on_page(&log_descriptor.horizon,
                                   &log_descriptor.bc,
                                   parts->total_record_length, parts);
  log_descriptor.bc.buffer->last_lsn= *lsn;
  DBUG_PRINT("info", ("last_lsn set to (%lu,0x%lx)  buffer: 0x%lx",
                      LSN_IN_PARTS(log_descriptor.bc.buffer->last_lsn),
                      (ulong) log_descriptor.bc.buffer));
  translog_unlock();

  /*
     check if we switched buffer and need process it (current buffer is
     unlocked already => we will not delay other threads
  */
  if (buffer_to_flush != NULL)
  {
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    translog_buffer_unlock(buffer_to_flush);
  }

  DBUG_RETURN(rc);
}


/*
  @brief Calculates and write LSN difference (compressed LSN).

  @param base_lsn        LSN from which we calculate difference
  @param lsn             LSN for codding
  @param dst             Result will be written to dst[-pack_length] .. dst[-1]

  @note To store an LSN in a compact way we will use the following compression:
    If a log record has LSN1, and it contains the LSN2 as a back reference,
    Instead of LSN2 we write LSN1-LSN2, encoded as:
     two bits     the number N (see below)
     14 bits
     N bytes
     That is, LSN is encoded in 2..5 bytes, and the number of bytes minus 2
     is stored in the first two bits.

  @note function made to write the result in backward direction with no
  special sense or tricks both directions are equal in complicity

  @retval #    pointer on coded LSN
*/

static uchar *translog_put_LSN_diff(LSN base_lsn, LSN lsn, uchar *dst)
{
  uint64 diff;
  DBUG_ENTER("translog_put_LSN_diff");
  DBUG_PRINT("enter", ("Base: (%lu,0x%lx)  val: (%lu,0x%lx)  dst: 0x%lx",
                       LSN_IN_PARTS(base_lsn), LSN_IN_PARTS(lsn),
                       (ulong) dst));
  DBUG_ASSERT(base_lsn > lsn);
  diff= base_lsn - lsn;
  DBUG_PRINT("info", ("Diff: 0x%llx", (ulonglong) diff));
  if (diff <= 0x3FFF)
  {
    dst-= 2;
    /*
      Note we store this high uchar first to ensure that first uchar has
      0 in the 3 upper bits.
    */
    dst[0]= (uchar)(diff >> 8);
    dst[1]= (uchar)(diff & 0xFF);
  }
  else if (diff <= 0x3FFFFFL)
  {
    dst-= 3;
    dst[0]= (uchar)(0x40 | (diff >> 16));
    int2store(dst + 1, diff & 0xFFFF);
  }
  else if (diff <= 0x3FFFFFFFL)
  {
    dst-= 4;
    dst[0]= (uchar)(0x80 | (diff >> 24));
    int3store(dst + 1, diff & 0xFFFFFFL);
  }
  else if (diff <= LL(0x3FFFFFFFFF))

  {
    dst-= 5;
    dst[0]= (uchar)(0xC0 | (diff >> 32));
    int4store(dst + 1, diff & 0xFFFFFFFFL);
  }
  else
  {
    /*
      It is full LSN after special 1 diff (which is impossible
      in real life)
    */
    dst-= 2 + LSN_STORE_SIZE;
    dst[0]= 0;
    dst[1]= 1;
    lsn_store(dst + 2, lsn);
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

static uchar *translog_get_LSN_from_diff(LSN base_lsn, uchar *src, uchar *dst)
{
  LSN lsn;
  uint32 diff;
  uint32 first_byte;
  uint32 file_no, rec_offset;
  uint8 code;
  DBUG_ENTER("translog_get_LSN_from_diff");
  DBUG_PRINT("enter", ("Base: (%lu,0x%lx)  src: 0x%lx  dst 0x%lx",
                       LSN_IN_PARTS(base_lsn), (ulong) src, (ulong) dst));
  first_byte= *((uint8*) src);
  code= first_byte >> 6; /* Length is in 2 most significant bits */
  first_byte&= 0x3F;
  src++;                                        /* Skip length + encode */
  file_no= LSN_FILE_NO(base_lsn);               /* Assume relative */
  DBUG_PRINT("info", ("code: %u  first byte: %lu",
                      (uint) code, (ulong) first_byte));
  switch (code) {
  case 0:
    if (first_byte == 0 && *((uint8*)src) == 1)
    {
      /*
        It is full LSN after special 1 diff (which is impossible
        in real life)
      */
      memcpy(dst, src + 1, LSN_STORE_SIZE);
      DBUG_PRINT("info", ("Special case of full LSN, new src: 0x%lx",
                          (ulong) (src + 1 + LSN_STORE_SIZE)));
      DBUG_RETURN(src + 1 + LSN_STORE_SIZE);
    }
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
    DBUG_ASSERT(base_offset - diff <= UINT_MAX);
    rec_offset= (uint32)(base_offset - diff);
    break;
  }
  default:
    DBUG_ASSERT(0);
    DBUG_RETURN(NULL);
  }
  lsn= MAKE_LSN(file_no, rec_offset);
  src+= code + 1;
  lsn_store(dst, lsn);
  DBUG_PRINT("info", ("new src: 0x%lx", (ulong) src));
  DBUG_RETURN(src);
}


/**
  @brief Encodes relative LSNs listed in the parameters.

  @param parts           Parts list with encoded LSN(s)
  @param base_lsn        LSN which is base for encoding
  @param lsns            number of LSN(s) to encode
  @param compressed_LSNs buffer which can be used for storing compressed LSN(s)
*/

static void  translog_relative_LSN_encode(struct st_translog_parts *parts,
                                          LSN base_lsn,
                                          uint lsns, uchar *compressed_LSNs)
{
  LEX_CUSTRING *part;
  uint lsns_len= lsns * LSN_STORE_SIZE;
  uchar buffer_src[MAX_NUMBER_OF_LSNS_PER_RECORD * LSN_STORE_SIZE];
  uchar *buffer= buffer_src;
  const uchar *cbuffer;

  DBUG_ENTER("translog_relative_LSN_encode");

  DBUG_ASSERT(parts->current != 0);
  part= parts->parts + parts->current;

  /* collect all LSN(s) in one chunk if it (they) is (are) divided */
  if (part->length < lsns_len)
  {
    uint copied= part->length;
    LEX_CUSTRING *next_part;
    DBUG_PRINT("info", ("Using buffer: 0x%lx", (ulong) compressed_LSNs));
    memcpy(buffer, part->str, part->length);
    next_part= parts->parts + parts->current + 1;
    do
    {
      DBUG_ASSERT(next_part < parts->parts + parts->elements);
      if ((next_part->length + copied) < lsns_len)
      {
        memcpy(buffer + copied, next_part->str,
               next_part->length);
        copied+= next_part->length;
        next_part->length= 0; next_part->str= 0;
        /* delete_dynamic_element(&parts->parts, parts->current + 1); */
        next_part++;
        parts->current++;
        part= parts->parts + parts->current;
      }
      else
      {
        uint len= lsns_len - copied;
        memcpy(buffer + copied, next_part->str, len);
        copied= lsns_len;
        next_part->str+= len;
        next_part->length-= len;
      }
    } while (copied < lsns_len);
    cbuffer= buffer;
  }
  else
  {
    cbuffer= part->str;
    part->str+= lsns_len;
    part->length-= lsns_len;
    parts->current--;
    part= parts->parts + parts->current;
  }

  {
    /* Compress */
    LSN ref;
    int economy;
    const uchar *src_ptr;
    uchar *dst_ptr= compressed_LSNs + (MAX_NUMBER_OF_LSNS_PER_RECORD *
                                      COMPRESSED_LSN_MAX_STORE_SIZE);
    /*
      We write the result in backward direction with no special sense or
      tricks both directions are equal in complicity
    */
    for (src_ptr= cbuffer + lsns_len - LSN_STORE_SIZE;
         src_ptr >= (const uchar*)cbuffer;
         src_ptr-= LSN_STORE_SIZE)
    {
      ref= lsn_korr(src_ptr);
      dst_ptr= translog_put_LSN_diff(base_lsn, ref, dst_ptr);
    }
    part->length= (uint)((compressed_LSNs +
                          (MAX_NUMBER_OF_LSNS_PER_RECORD *
                           COMPRESSED_LSN_MAX_STORE_SIZE)) -
                         dst_ptr);
    parts->record_length-= (economy= lsns_len - part->length);
    DBUG_PRINT("info", ("new length of LSNs: %lu  economy: %d",
                        (ulong)part->length, economy));
    parts->total_record_length-= economy;
    part->str= dst_ptr;
  }
  DBUG_VOID_RETURN;
}


/**
   @brief Write multi-group variable-size record.

   @param  lsn             LSN of the record will be written here
   @param  type            the log record type
   @param  short_trid      Short transaction ID or 0 if it has no sense
   @param  parts           Descriptor of record source parts
   @param  buffer_to_flush Buffer which have to be flushed if it is not 0
   @param  header_length   Header length calculated for 1 group
   @param  buffer_rest     Beginning from which we plan to write in full pages
   @param  trn             Transaction structure pointer for hooks by
                           record log type, for short_id
   @param  hook_arg        Argument which will be passed to pre-write and
                           in-write hooks of this record.

   @note
     We must have a translog_lock() when entering this function

     We must have buffer_to_flush locked (if not null)
     buffer_to_flush should *NOT* be locked when calling this function.
     (This is note is here as this is different from most other
     translog_write...() functions which require the buffer to be locked)

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

static my_bool
translog_write_variable_record_mgroup(LSN *lsn,
                                      enum translog_record_type type,
                                      MARIA_HA *tbl_info,
                                      SHORT_TRANSACTION_ID short_trid,
                                      struct st_translog_parts *parts,
                                      struct st_translog_buffer
                                      *buffer_to_flush,
                                      uint16 header_length,
                                      translog_size_t buffer_rest,
                                      TRN *trn, void *hook_arg)
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
  uchar chunk0_header[1 + 2 + 5 + 2 + 2], group_desc[7 + 1];
  uchar chunk2_header[1];
  uint header_fixed_part= header_length + 2;
  uint groups_per_page= (page_capacity - header_fixed_part) / (7 + 1);
  uint file_of_the_first_group;
  int pages_to_skip;
  struct st_translog_buffer *buffer_of_last_lsn;
  DBUG_ENTER("translog_write_variable_record_mgroup");
  translog_lock_assert_owner();

  chunk2_header[0]= TRANSLOG_CHUNK_NOHDR;

  if (my_init_dynamic_array(&groups,
                            sizeof(struct st_translog_group_descriptor),
                            10, 10))
  {
    translog_unlock();
    DBUG_PRINT("error", ("init array failed"));
    DBUG_RETURN(1);
  }

  first_page= translog_get_current_page_rest();
  record_rest= parts->record_length - (first_page - 1);
  DBUG_PRINT("info", ("Record Rest: %lu", (ulong) record_rest));

  if (record_rest < buffer_rest)
  {
    /*
      The record (group 1 type) is larger than the free space on the page
      - we need to split it in two. But when we split it in two, the first
      part is big enough to hold all the data of the record (because the
      header of the first part of the split is smaller than the header of
      the record as a whole when it takes only one chunk)
    */
    DBUG_PRINT("info", ("too many free space because changing header"));
    buffer_rest-= log_descriptor.page_capacity_chunk_2;
    DBUG_ASSERT(record_rest >= buffer_rest);
  }

  file_of_the_first_group= LSN_FILE_NO(log_descriptor.horizon);
  translog_mark_file_unfinished(file_of_the_first_group);
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
    if (insert_dynamic(&groups, (uchar*) &group))
    {
      DBUG_PRINT("error", ("insert into array failed"));
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
    rc|= translog_advance_pointer((int)full_pages, 0);

    translog_unlock();

    if (buffer_to_flush != NULL)
    {
      translog_buffer_decrease_writers(buffer_to_flush);
      if (!rc)
        rc= translog_buffer_flush(buffer_to_flush);
      translog_buffer_unlock(buffer_to_flush);
      buffer_to_flush= NULL;
    }
    if (rc)
    {
      DBUG_PRINT("error", ("flush of unlock buffer failed"));
      goto err;
    }

    translog_write_data_on_page(&horizon, &cursor, 1, chunk2_header);
    translog_write_parts_on_page(&horizon, &cursor, first_page - 1, parts);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx)  "
                        "Left  %lu",
                        LSN_IN_PARTS(log_descriptor.horizon),
                        LSN_IN_PARTS(horizon),
                        (ulong) (parts->record_length - (first_page - 1) -
                                 done)));

    for (i= 0; i < full_pages; i++)
    {
      if (translog_write_variable_record_chunk2_page(parts, &horizon, &cursor))
        goto err;

      DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  "
                          "local: (%lu,0x%lx)  "
                          "Left: %lu",
                          LSN_IN_PARTS(log_descriptor.horizon),
                          LSN_IN_PARTS(horizon),
                          (ulong) (parts->record_length - (first_page - 1) -
                                   i * log_descriptor.page_capacity_chunk_2 -
                                   done)));
    }

    done+= (first_page - 1 + buffer_rest);

    if (translog_chaser_page_next(&horizon, &cursor))
    {
      DBUG_PRINT("error", ("flush of unlock buffer failed"));
      goto err;
    }
    translog_buffer_lock(cursor.buffer);
    translog_buffer_decrease_writers(cursor.buffer);
    translog_buffer_unlock(cursor.buffer);

    translog_lock();

    /* Check that we have place for chunk type 2 */
    first_page= translog_get_current_page_rest();
    if (first_page <= 1)
    {
      if (translog_page_next(&log_descriptor.horizon, &log_descriptor.bc,
                             &buffer_to_flush))
        goto err_unlock;
      first_page= translog_get_current_page_rest();
    }
    buffer_rest= translog_get_current_group_size();
  } while ((translog_size_t)(first_page + buffer_rest) <
           (translog_size_t)(parts->record_length - done));

  group.addr= horizon= log_descriptor.horizon;
  cursor= log_descriptor.bc;
  cursor.chaser= 1;
  group.num= 0;                       /* 0 because it does not matter */
  if (insert_dynamic(&groups, (uchar*) &group))
  {
    DBUG_PRINT("error", ("insert into array failed"));
    goto err_unlock;
  }
  record_rest= parts->record_length - done;
  DBUG_PRINT("info", ("Record rest: %lu", (ulong) record_rest));
  if (first_page > record_rest + 1)
  {
    /*
      We have not so much data to fill all first page
      (no speaking about full pages)
      so it will be:
      <chunk0 <data>>
      or
      <chunk0>...<chunk0><chunk0 <data>>
      or
      <chunk3 <data>><chunk0>...<chunk0><chunk0 <possible data of 1 byte>>
    */
    chunk2_page= full_pages= 0;
    last_page_capacity= first_page;
    pages_to_skip= -1;
  }
  else
  {
    /*
      We will have:
      <chunk2 <data>>...<chunk2 <data>><chunk0 <data>>
      or
      <chunk2 <data>>...<chunk2 <data>><chunk0>...<chunk0><chunk0 <data>>
      or
      <chunk3 <data>><chunk0>...<chunk0><chunk0 <possible data of 1 byte>>
    */
    chunk2_page= 1;
    record_rest-= (first_page - 1);
    pages_to_skip= full_pages=
      record_rest / log_descriptor.page_capacity_chunk_2;
    record_rest= (record_rest % log_descriptor.page_capacity_chunk_2);
    last_page_capacity= page_capacity;
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
      pages_to_skip++;
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
  rc= translog_advance_pointer(pages_to_skip + (int)(chunk0_pages - 1),
                               record_rest + header_fixed_part +
                               (groups.elements -
                                ((page_capacity -
                                  header_fixed_part) / (7 + 1)) *
                                (chunk0_pages - 1)) * (7 + 1));
  buffer_of_last_lsn= log_descriptor.bc.buffer;
  translog_unlock();

  if (buffer_to_flush != NULL)
  {
    translog_buffer_decrease_writers(buffer_to_flush);
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    translog_buffer_unlock(buffer_to_flush);
    buffer_to_flush= NULL;
  }
  if (rc)
  {
    DBUG_PRINT("error", ("flush of unlock buffer failed"));
    goto err;
  }

  if (rc)
    goto err;

  if (chunk2_page)
  {
    DBUG_PRINT("info", ("chunk 2 to finish first page"));
    translog_write_data_on_page(&horizon, &cursor, 1, chunk2_header);
    translog_write_parts_on_page(&horizon, &cursor, first_page - 1, parts);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx) "
                        "Left: %lu",
                        LSN_IN_PARTS(log_descriptor.horizon),
                        LSN_IN_PARTS(horizon),
                        (ulong) (parts->record_length - (first_page - 1) -
                                 done)));
  }
  else if (chunk3_pages)
  {
    uchar chunk3_header[3];
    DBUG_PRINT("info", ("chunk 3"));
    DBUG_ASSERT(full_pages == 0);
    chunk3_pages= 0;
    chunk3_header[0]= TRANSLOG_CHUNK_LNGTH;
    int2store(chunk3_header + 1, chunk3_size);
    translog_write_data_on_page(&horizon, &cursor, 3, chunk3_header);
    translog_write_parts_on_page(&horizon, &cursor, chunk3_size, parts);
    DBUG_PRINT("info", ("absolute horizon: (%lu,0x%lx)  local: (%lu,0x%lx) "
                        "Left: %lu",
                        LSN_IN_PARTS(log_descriptor.horizon),
                        LSN_IN_PARTS(horizon),
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
                        LSN_IN_PARTS(log_descriptor.horizon),
                        LSN_IN_PARTS(horizon),
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
                      LSN_IN_PARTS(log_descriptor.horizon),
                      LSN_IN_PARTS(horizon)));

  *chunk0_header= (uchar) (type | TRANSLOG_CHUNK_LSN);
  int2store(chunk0_header + 1, short_trid);
  translog_write_variable_record_1group_code_len(chunk0_header + 3,
                                                 parts->record_length,
                                                 header_length);
  do
  {
    int limit;
    if (new_page_before_chunk0 &&
        translog_chaser_page_next(&horizon, &cursor))
    {
      DBUG_PRINT("error", ("flush of unlock buffer failed"));
      goto err;
    }
    new_page_before_chunk0= 1;

    if (first_chunk0)
    {
      first_chunk0= 0;

      /*
        We can drop "log_descriptor.is_everything_flushed" earlier when have
        lock on loghandler and assign initial value of "horizon" variable or
        before unlocking loghandler (because we will increase writers
        counter on the buffer and every thread which wanted flush the buffer
        will wait till we finish with it). But IMHO better here take short
        lock and do not bother other threads with waiting.
      */
      translog_lock();
      set_lsn(lsn, horizon);
      buffer_of_last_lsn->last_lsn= *lsn;
      DBUG_PRINT("info", ("last_lsn set to (%lu,0x%lx)  buffer: 0x%lx",
                          LSN_IN_PARTS(buffer_of_last_lsn->last_lsn),
                          (ulong) buffer_of_last_lsn));
      if (log_record_type_descriptor[type].inwrite_hook &&
          (*log_record_type_descriptor[type].inwrite_hook) (type, trn,
                                                            tbl_info,
                                                            lsn, hook_arg))
        goto err_unlock;
      translog_unlock();
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
    /* put special type to indicate that it is not LSN chunk */
    *chunk0_header= (uchar) (TRANSLOG_CHUNK_LSN | TRANSLOG_CHUNK_0_CONT);
  } while (chunk0_pages != 0);
  translog_buffer_lock(cursor.buffer);
  translog_buffer_decrease_writers(cursor.buffer);
  translog_buffer_unlock(cursor.buffer);
  rc= 0;

  if (translog_set_lsn_for_files(file_of_the_first_group, LSN_FILE_NO(*lsn),
                                 *lsn, FALSE))
    goto err;

  translog_mark_file_finished(file_of_the_first_group);

  delete_dynamic(&groups);
  DBUG_RETURN(rc);

err_unlock:

  translog_unlock();

err:
  if (buffer_to_flush != NULL)
  {
    /* This is to prevent locking buffer forever in case of error */
    translog_buffer_decrease_writers(buffer_to_flush);
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    translog_buffer_unlock(buffer_to_flush);
    buffer_to_flush= NULL;
  }


  translog_mark_file_finished(file_of_the_first_group);

  delete_dynamic(&groups);
  DBUG_RETURN(1);
}


/**
   @brief Write the variable length log record.

   @param  lsn             LSN of the record will be written here
   @param  type            the log record type
   @param  short_trid      Short transaction ID or 0 if it has no sense
   @param  parts           Descriptor of record source parts
   @param  trn             Transaction structure pointer for hooks by
                           record log type, for short_id
   @param  hook_arg        Argument which will be passed to pre-write and
                           in-write hooks of this record.

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

static my_bool translog_write_variable_record(LSN *lsn,
                                              enum translog_record_type type,
                                              MARIA_HA *tbl_info,
                                              SHORT_TRANSACTION_ID short_trid,
                                              struct st_translog_parts *parts,
                                              TRN *trn, void *hook_arg)
{
  struct st_translog_buffer *buffer_to_flush= NULL;
  uint header_length1= 1 + 2 + 2 +
    translog_variable_record_length_bytes(parts->record_length);
  ulong buffer_rest;
  uint page_rest;
  /* Max number of such LSNs per record is 2 */
  uchar compressed_LSNs[MAX_NUMBER_OF_LSNS_PER_RECORD *
    COMPRESSED_LSN_MAX_STORE_SIZE];
  my_bool res;
  DBUG_ENTER("translog_write_variable_record");

  translog_lock();
  DBUG_PRINT("info", ("horizon: (%lu,0x%lx)",
                      LSN_IN_PARTS(log_descriptor.horizon)));
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
    /* Chunk 2 header is 1 byte, so full page capacity will be one uchar more */
    page_rest= log_descriptor.page_capacity_chunk_2 + 1;
    DBUG_PRINT("info", ("page_rest: %u", page_rest));
  }

  /*
     To minimize compressed size we will compress always relative to
     very first chunk address (log_descriptor.horizon for now)
  */
  if (log_record_type_descriptor[type].compressed_LSN > 0)
  {
    translog_relative_LSN_encode(parts, log_descriptor.horizon,
                                 log_record_type_descriptor[type].
                                 compressed_LSN, compressed_LSNs);
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
    res= translog_write_variable_record_1chunk(lsn, type, tbl_info,
                                               short_trid,
                                               parts, buffer_to_flush,
                                               header_length1, trn, hook_arg);
    DBUG_RETURN(res);
  }

  buffer_rest= translog_get_current_group_size();

  if (buffer_rest >= parts->record_length + header_length1 - page_rest)
  {
    /* following function makes translog_unlock(); */
    res= translog_write_variable_record_1group(lsn, type, tbl_info,
                                               short_trid,
                                               parts, buffer_to_flush,
                                               header_length1, trn, hook_arg);
    DBUG_RETURN(res);
  }
  /* following function makes translog_unlock(); */
  res= translog_write_variable_record_mgroup(lsn, type, tbl_info,
                                             short_trid,
                                             parts, buffer_to_flush,
                                             header_length1,
                                             buffer_rest, trn, hook_arg);
  DBUG_RETURN(res);
}


/**
   @brief Write the fixed and pseudo-fixed log record.

   @param  lsn             LSN of the record will be written here
   @param  type            the log record type
   @param  short_trid      Short transaction ID or 0 if it has no sense
   @param  parts           Descriptor of record source parts
   @param  trn             Transaction structure pointer for hooks by
                           record log type, for short_id
   @param  hook_arg        Argument which will be passed to pre-write and
                           in-write hooks of this record.

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

static my_bool translog_write_fixed_record(LSN *lsn,
                                           enum translog_record_type type,
                                           MARIA_HA *tbl_info,
                                           SHORT_TRANSACTION_ID short_trid,
                                           struct st_translog_parts *parts,
                                           TRN *trn, void *hook_arg)
{
  struct st_translog_buffer *buffer_to_flush= NULL;
  uchar chunk1_header[1 + 2];
  /* Max number of such LSNs per record is 2 */
  uchar compressed_LSNs[MAX_NUMBER_OF_LSNS_PER_RECORD *
    COMPRESSED_LSN_MAX_STORE_SIZE];
  LEX_CUSTRING *part;
  int rc= 1;
  DBUG_ENTER("translog_write_fixed_record");
  DBUG_ASSERT((log_record_type_descriptor[type].rclass ==
               LOGRECTYPE_FIXEDLENGTH &&
               parts->record_length ==
               log_record_type_descriptor[type].fixed_length) ||
              (log_record_type_descriptor[type].rclass ==
               LOGRECTYPE_PSEUDOFIXEDLENGTH &&
               parts->record_length ==
               log_record_type_descriptor[type].fixed_length));

  translog_lock();
  DBUG_PRINT("info", ("horizon: (%lu,0x%lx)",
                      LSN_IN_PARTS(log_descriptor.horizon)));

  DBUG_ASSERT(log_descriptor.bc.current_page_fill <= TRANSLOG_PAGE_SIZE);
  DBUG_PRINT("info",
             ("Page size: %u  record: %u  next cond: %d",
              log_descriptor.bc.current_page_fill,
              (parts->record_length +
               log_record_type_descriptor[type].compressed_LSN * 2 + 3),
              ((((uint) log_descriptor.bc.current_page_fill) +
                (parts->record_length +
                 log_record_type_descriptor[type].compressed_LSN * 2 + 3)) >
               TRANSLOG_PAGE_SIZE)));
  /*
    check that there is enough place on current page.
    NOTE: compressing may increase page LSN size on two bytes for every LSN
  */
  if ((((uint) log_descriptor.bc.current_page_fill) +
       (parts->record_length +
        log_record_type_descriptor[type].compressed_LSN * 2 + 3)) >
      TRANSLOG_PAGE_SIZE)
  {
    DBUG_PRINT("info", ("Next page"));
    if (translog_page_next(&log_descriptor.horizon, &log_descriptor.bc,
                           &buffer_to_flush))
      goto err;                                 /* rc == 1 */
    if (buffer_to_flush)
      translog_buffer_lock_assert_owner(buffer_to_flush);
  }

  set_lsn(lsn, log_descriptor.horizon);
  if (translog_set_lsn_for_files(LSN_FILE_NO(*lsn), LSN_FILE_NO(*lsn),
                             *lsn, TRUE) ||
      (log_record_type_descriptor[type].inwrite_hook &&
       (*log_record_type_descriptor[type].inwrite_hook)(type, trn, tbl_info,
                                                        lsn, hook_arg)))
    goto err;

  /* compress LSNs */
  if (log_record_type_descriptor[type].rclass ==
      LOGRECTYPE_PSEUDOFIXEDLENGTH)
  {
    DBUG_ASSERT(log_record_type_descriptor[type].compressed_LSN > 0);
    translog_relative_LSN_encode(parts, *lsn,
                                 log_record_type_descriptor[type].
                                 compressed_LSN, compressed_LSNs);
  }

  /*
    Write the whole record at once (we know that there is enough place on
    the destination page)
  */
  DBUG_ASSERT(parts->current != 0);       /* first part is left for header */
  part= parts->parts + (--parts->current);
  parts->total_record_length+= (translog_size_t) (part->length= 1 + 2);
  part->str= chunk1_header;
  *chunk1_header= (uchar) (type | TRANSLOG_CHUNK_FIXED);
  int2store(chunk1_header + 1, short_trid);

  rc= translog_write_parts_on_page(&log_descriptor.horizon,
                                   &log_descriptor.bc,
                                   parts->total_record_length, parts);

  log_descriptor.bc.buffer->last_lsn= *lsn;
  DBUG_PRINT("info", ("last_lsn set to (%lu,0x%lx)  buffer: 0x%lx",
                      LSN_IN_PARTS(log_descriptor.bc.buffer->last_lsn),
                      (ulong) log_descriptor.bc.buffer));

err:
  translog_unlock();

  /*
    check if we switched buffer and need process it (current buffer is
    unlocked already => we will not delay other threads
  */
  if (buffer_to_flush != NULL)
  {
    if (!rc)
      rc= translog_buffer_flush(buffer_to_flush);
    translog_buffer_unlock(buffer_to_flush);
  }

  DBUG_RETURN(rc);
}


/**
   @brief Writes the log record

   If share has no 2-byte-id yet, gives an id to the share and logs
   LOGREC_FILE_ID. If transaction has not logged LOGREC_LONG_TRANSACTION_ID
   yet, logs it.

   @param  lsn             LSN of the record will be written here
   @param  type            the log record type
   @param  trn             Transaction structure pointer for hooks by
                           record log type, for short_id
   @param  tbl_info        MARIA_HA of table or NULL
   @param  rec_len         record length or 0 (count it)
   @param  part_no         number of parts or 0 (count it)
   @param  parts_data      zero ended (in case of number of parts is 0)
                           array of LEX_STRINGs (parts), first
                           TRANSLOG_INTERNAL_PARTS positions in the log
                           should be unused (need for loghandler)
   @param  store_share_id  if tbl_info!=NULL then share's id will
                           automatically be stored in the two first bytes
                           pointed (so pointer is assumed to be !=NULL)
   @param  hook_arg        argument which will be passed to pre-write and
                           in-write hooks of this record.

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

my_bool translog_write_record(LSN *lsn,
                              enum translog_record_type type,
                              TRN *trn, MARIA_HA *tbl_info,
                              translog_size_t rec_len,
                              uint part_no,
                              LEX_CUSTRING *parts_data,
                              uchar *store_share_id,
                              void *hook_arg)
{
  struct st_translog_parts parts;
  LEX_CUSTRING *part;
  int rc;
  uint short_trid= trn->short_id;
  DBUG_ENTER("translog_write_record");
  DBUG_PRINT("enter", ("type: %u (%s)  ShortTrID: %u  rec_len: %lu",
                       (uint) type, log_record_type_descriptor[type].name,
                       (uint) short_trid, (ulong) rec_len));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  if (unlikely(translog_status != TRANSLOG_OK))
  {
    DBUG_PRINT("error", ("Transaction log is write protected"));
    DBUG_RETURN(1);
  }

  if (tbl_info && type != LOGREC_FILE_ID)
  {
    MARIA_SHARE *share= tbl_info->s;
    DBUG_ASSERT(share->now_transactional);
    if (unlikely(share->id == 0))
    {
      /*
        First log write for this MARIA_SHARE; give it a short id.
        When the lock manager is enabled and needs a short id, it should be
        assigned in the lock manager (because row locks will be taken before
        log records are written; for example SELECT FOR UPDATE takes locks but
        writes no log record.
      */
      if (unlikely(translog_assign_id_to_share(tbl_info, trn)))
        DBUG_RETURN(1);
    }
    fileid_store(store_share_id, share->id);
  }
  if (unlikely(!(trn->first_undo_lsn & TRANSACTION_LOGGED_LONG_ID)))
  {
    LSN dummy_lsn;
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    uchar log_data[6];
    DBUG_ASSERT(trn->undo_lsn == LSN_IMPOSSIBLE);
    int6store(log_data, trn->trid);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    trn->first_undo_lsn|= TRANSACTION_LOGGED_LONG_ID; /* no recursion */
    if (unlikely(translog_write_record(&dummy_lsn, LOGREC_LONG_TRANSACTION_ID,
                                       trn, NULL, sizeof(log_data),
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, NULL, NULL)))
      DBUG_RETURN(1);
  }

  parts.parts= parts_data;

  /* count parts if they are not counted by upper level */
  if (part_no == 0)
  {
    for (part_no= TRANSLOG_INTERNAL_PARTS;
         parts_data[part_no].length != 0;
         part_no++);
  }
  parts.elements= part_no;
  parts.current= TRANSLOG_INTERNAL_PARTS;

  /* clear TRANSLOG_INTERNAL_PARTS */
  compile_time_assert(TRANSLOG_INTERNAL_PARTS != 0);
  parts_data[0].str= 0;
  parts_data[0].length= 0;

  /* count length of the record */
  if (rec_len == 0)
  {
    for(part= parts_data + TRANSLOG_INTERNAL_PARTS;\
        part < parts_data + part_no;
        part++)
    {
      rec_len+= (translog_size_t) part->length;
    }
  }
  parts.record_length= rec_len;

#ifndef DBUG_OFF
  {
    uint i;
    uint len= 0;
#ifdef HAVE_valgrind
    ha_checksum checksum= 0;
#endif
    for (i= TRANSLOG_INTERNAL_PARTS; i < part_no; i++)
    {
#ifdef HAVE_valgrind
      /* Find unitialized bytes early */
      checksum+= my_checksum(checksum, parts_data[i].str,
                             parts_data[i].length);
#endif
      len+= parts_data[i].length;
    }
    DBUG_ASSERT(len == rec_len);
  }
#endif
  /*
    Start total_record_length from record_length then overhead will
    be add
  */
  parts.total_record_length= parts.record_length;
  DBUG_PRINT("info", ("record length: %lu", (ulong) parts.record_length));

  /* process this parts */
  if (!(rc= (log_record_type_descriptor[type].prewrite_hook &&
             (*log_record_type_descriptor[type].prewrite_hook) (type, trn,
                                                                tbl_info,
                                                                hook_arg))))
  {
    switch (log_record_type_descriptor[type].rclass) {
    case LOGRECTYPE_VARIABLE_LENGTH:
      rc= translog_write_variable_record(lsn, type, tbl_info,
                                         short_trid, &parts, trn, hook_arg);
      break;
    case LOGRECTYPE_PSEUDOFIXEDLENGTH:
    case LOGRECTYPE_FIXEDLENGTH:
      rc= translog_write_fixed_record(lsn, type, tbl_info,
                                      short_trid, &parts, trn, hook_arg);
      break;
    case LOGRECTYPE_NOT_ALLOWED:
    default:
      DBUG_ASSERT(0);
      rc= 1;
    }
  }

  DBUG_PRINT("info", ("LSN: (%lu,0x%lx)", LSN_IN_PARTS(*lsn)));
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

static uchar *translog_relative_LSN_decode(LSN base_lsn,
                                          uchar *src, uchar *dst, uint lsns)
{
  uint i;
  for (i= 0; i < lsns; i++, dst+= LSN_STORE_SIZE)
  {
    src= translog_get_LSN_from_diff(base_lsn, src, dst);
  }
  return src;
}

/**
   @brief Get header of fixed/pseudo length record and call hook for
   it processing

   @param page            Pointer to the buffer with page where LSN chunk is
                          placed
   @param page_offset     Offset of the first chunk in the page
   @param buff            Buffer to be filled with header data

   @return Length of header or operation status
     @retval #  number of bytes in TRANSLOG_HEADER_BUFFER::header where
                stored decoded part of the header
*/

static int translog_fixed_length_header(uchar *page,
                                        translog_size_t page_offset,
                                        TRANSLOG_HEADER_BUFFER *buff)
{
  struct st_log_record_type_descriptor *desc=
    log_record_type_descriptor + buff->type;
  uchar *src= page + page_offset + 3;
  uchar *dst= buff->header;
  uchar *start= src;
  int lsns= desc->compressed_LSN;
  uint length= desc->fixed_length;
  DBUG_ENTER("translog_fixed_length_header");

  buff->record_length= length;

  if (desc->rclass == LOGRECTYPE_PSEUDOFIXEDLENGTH)
  {
    DBUG_ASSERT(lsns > 0);
    src= translog_relative_LSN_decode(buff->lsn, src, dst, lsns);
    lsns*= LSN_STORE_SIZE;
    dst+= lsns;
    length-= lsns;
    buff->compressed_LSN_economy= (lsns - (int) (src - start));
  }
  else
    buff->compressed_LSN_economy= 0;

  memcpy(dst, src, length);
  buff->non_header_data_start_offset= (uint16) (page_offset +
                                                ((src + length) -
                                                 (page + page_offset)));
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
    my_free(buff->groups, MYF(0));
    buff->groups_no= 0;
  }
  DBUG_VOID_RETURN;
}


/**
   @brief Returns the current horizon at the end of the current log

   @return Horizon
   @retval LSN_ERROR     error
   @retvar #             Horizon
*/

TRANSLOG_ADDRESS translog_get_horizon()
{
  TRANSLOG_ADDRESS res;
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  translog_lock();
  res= log_descriptor.horizon;
  translog_unlock();
  return res;
}


/**
   @brief Returns the current horizon at the end of the current log, caller is
   assumed to already hold the lock

   @return Horizon
   @retval LSN_ERROR     error
   @retvar #             Horizon
*/

TRANSLOG_ADDRESS translog_get_horizon_no_lock()
{
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  translog_lock_assert_owner();
  return log_descriptor.horizon;
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

static my_bool translog_scanner_set_last_page(TRANSLOG_SCANNER_DATA *scanner)
{
  my_bool page_ok;
  if (LSN_FILE_NO(scanner->page_addr) == LSN_FILE_NO(scanner->horizon))
  {
    /* It is last file => we can easy find last page address by horizon */
    uint pagegrest= LSN_OFFSET(scanner->horizon) % TRANSLOG_PAGE_SIZE;
    scanner->last_file_page= (scanner->horizon -
                              (pagegrest ? pagegrest : TRANSLOG_PAGE_SIZE));
    return (0);
  }
  scanner->last_file_page= scanner->page_addr;
  return (translog_get_last_page_addr(&scanner->last_file_page, &page_ok, 0));
}


/**
  @brief Get page from page cache according to requested method

  @param scanner         The scanner data

  @return operation status
  @retval 0 OK
  @retval 1 Error
*/

static my_bool
translog_scanner_get_page(TRANSLOG_SCANNER_DATA *scanner)
{
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_scanner_get_page");
  data.addr= &scanner->page_addr;
  data.was_recovered= 0;
  DBUG_RETURN((scanner->page=
               translog_get_page(&data, scanner->buffer,
                                 (scanner->use_direct_link ?
                                  &scanner->direct_link :
                                  NULL))) ==
               NULL);
}


/**
  @brief Initialize reader scanner.

  @param lsn             LSN with which it have to be inited
  @param fixed_horizon   true if it is OK do not read records which was written
                         after scanning beginning
  @param scanner         scanner which have to be inited
  @param use_direct      prefer using direct lings from page handler
                         where it is possible.

  @note If direct link was used translog_destroy_scanner should be
        called after it using

  @return status of the operation
  @retval 0 OK
  @retval 1 Error
*/

my_bool translog_scanner_init(LSN lsn,
                              my_bool fixed_horizon,
                              TRANSLOG_SCANNER_DATA *scanner,
                              my_bool use_direct)
{
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_scanner_init");
  DBUG_PRINT("enter", ("Scanner: 0x%lx  LSN: (%lu,0x%lx)",
                       (ulong) scanner, LSN_IN_PARTS(lsn)));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  data.addr= &scanner->page_addr;
  data.was_recovered= 0;

  scanner->page_offset= LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE;

  scanner->fixed_horizon= fixed_horizon;
  scanner->use_direct_link= use_direct;
  scanner->direct_link= NULL;

  scanner->horizon= translog_get_horizon();
  DBUG_PRINT("info", ("horizon: (%lu,0x%lx)", LSN_IN_PARTS(scanner->horizon)));

  /* lsn < horizon */
  DBUG_ASSERT(lsn <= scanner->horizon);

  scanner->page_addr= lsn;
  scanner->page_addr-= scanner->page_offset; /*decrease offset */

  if (translog_scanner_set_last_page(scanner))
    DBUG_RETURN(1);

  if (translog_scanner_get_page(scanner))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


/**
  @brief Destroy scanner object;

  @param scanner         The scanner object to destroy
*/

void translog_destroy_scanner(TRANSLOG_SCANNER_DATA *scanner)
{
  DBUG_ENTER("translog_destroy_scanner");
  DBUG_PRINT("enter", ("Scanner: 0x%lx", (ulong)scanner));
  translog_free_link(scanner->direct_link);
  DBUG_VOID_RETURN;
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
              LSN_IN_PARTS(scanner->horizon),
              LSN_IN_PARTS(scanner->page_addr),
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
  scanner->horizon= translog_get_horizon();
  DBUG_PRINT("info",
             ("Horizon is re-read, EOL: %d",
              scanner->horizon <= (scanner->page_addr +
                                   scanner->page_offset)));
  DBUG_RETURN(scanner->horizon <= (scanner->page_addr +
                                   scanner->page_offset));
}


/**
  @brief Cheks End of the Page

  @param scanner         Information about current chunk during scanning

  @retval 1  End of the Page
  @retval 0  OK
*/

static my_bool translog_scanner_eop(TRANSLOG_SCANNER_DATA *scanner)
{
  DBUG_ENTER("translog_scanner_eop");
  DBUG_RETURN(scanner->page_offset >= TRANSLOG_PAGE_SIZE ||
              scanner->page[scanner->page_offset] == TRANSLOG_FILLER);
}


/**
  @brief Checks End of the File (i.e. we are scanning last page, which do not
    mean end of this page)

  @param scanner         Information about current chunk during scanning

  @retval 1 End of the File
  @retval 0 OK
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
  DBUG_ENTER("translog_get_next_chunk");

  if (translog_scanner_eop(scanner))
    len= TRANSLOG_PAGE_SIZE - scanner->page_offset;
  else if ((len= translog_get_total_chunk_length(scanner->page,
                                                 scanner->page_offset)) == 0)
    DBUG_RETURN(1);
  scanner->page_offset+= len;

  if (translog_scanner_eol(scanner))
  {
    scanner->page= END_OF_LOG;
    scanner->page_offset= 0;
    DBUG_RETURN(0);
  }
  if (translog_scanner_eop(scanner))
  {
    /* before reading next page we should unpin current one if it was pinned */
    translog_free_link(scanner->direct_link);
    if (translog_scanner_eof(scanner))
    {
      DBUG_PRINT("info", ("horizon: (%lu,0x%lx)  pageaddr: (%lu,0x%lx)",
                          LSN_IN_PARTS(scanner->horizon),
                          LSN_IN_PARTS(scanner->page_addr)));
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

    if (translog_scanner_get_page(scanner))
      DBUG_RETURN(1);

    scanner->page_offset= translog_get_first_chunk_offset(scanner->page);
    if (translog_scanner_eol(scanner))
    {
      scanner->page= END_OF_LOG;
      scanner->page_offset= 0;
      DBUG_RETURN(0);
    }
    DBUG_ASSERT(scanner->page[scanner->page_offset] != TRANSLOG_FILLER);
  }
  DBUG_RETURN(0);
}


/**
   @brief Get header of variable length record and call hook for it processing

   @param page            Pointer to the buffer with page where LSN chunk is
                          placed
   @param page_offset     Offset of the first chunk in the page
   @param buff            Buffer to be filled with header data
   @param scanner         If present should be moved to the header page if
                          it differ from LSN page

   @return                Length of header or operation status
     @retval RECHEADER_READ_ERROR  error
     @retval RECHEADER_READ_EOF    End of the log reached during the read
     @retval #                     number of bytes in
                                   TRANSLOG_HEADER_BUFFER::header where
                                   stored decoded part of the header
*/

static int
translog_variable_length_header(uchar *page, translog_size_t page_offset,
                                TRANSLOG_HEADER_BUFFER *buff,
                                TRANSLOG_SCANNER_DATA *scanner)
{
  struct st_log_record_type_descriptor *desc= (log_record_type_descriptor +
                                               buff->type);
  uchar *src= page + page_offset + 1 + 2;
  uchar *dst= buff->header;
  LSN base_lsn;
  uint lsns= desc->compressed_LSN;
  uint16 chunk_len;
  uint16 length= desc->read_header_len;
  uint16 buffer_length= length;
  uint16 body_len;
  int rc;
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
    page_rest= (uint16) (TRANSLOG_PAGE_SIZE - (src - page));

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
      DBUG_RETURN(RECHEADER_READ_ERROR);
    DBUG_PRINT("info", ("Groups: %u", (uint) grp_no));
    src+= (2 + 2);
    page_rest= (uint16) (TRANSLOG_PAGE_SIZE - (src - page));
    curr= 0;
    header_to_skip= src - (page + page_offset);
    buff->chunk0_pages= 0;

    for (;;)
    {
      uint i, read_length= grp_no;

      buff->chunk0_pages++;
      if (page_rest < grp_no * (7 + 1))
        read_length= page_rest / (7 + 1);
      DBUG_PRINT("info", ("Read chunk0 page#%u  read: %u  left: %u  "
                          "start from: %u",
                          buff->chunk0_pages, read_length, grp_no, curr));
      for (i= 0; i < read_length; i++, curr++)
      {
        DBUG_ASSERT(curr < buff->groups_no);
        buff->groups[curr].addr= lsn_korr(src + i * (7 + 1));
        buff->groups[curr].num= src[i * (7 + 1) + 7];
        DBUG_PRINT("info", ("group #%u (%lu,0x%lx)  chunks: %u",
                            curr,
                            LSN_IN_PARTS(buff->groups[curr].addr),
                            (uint) buff->groups[curr].num));
      }
      grp_no-= read_length;
      if (grp_no == 0)
      {
        if (scanner)
        {
          buff->chunk0_data_addr= scanner->page_addr;
          /* offset increased */
          buff->chunk0_data_addr+= (page_offset + header_to_skip +
                                    read_length * (7 + 1));
        }
        else
        {
          buff->chunk0_data_addr= buff->lsn;
          /* offset increased */
          buff->chunk0_data_addr+= (header_to_skip + read_length * (7 + 1));
        }
        buff->chunk0_data_len= chunk_len - 2 - read_length * (7 + 1);
        DBUG_PRINT("info", ("Data address: (%lu,0x%lx)  len: %u",
                            LSN_IN_PARTS(buff->chunk0_data_addr),
                            buff->chunk0_data_len));
        break;
      }
      if (scanner == NULL)
      {
        DBUG_PRINT("info", ("use internal scanner for header reading"));
        scanner= &internal_scanner;
        if (translog_scanner_init(buff->lsn, 1, scanner, 0))
        {
          rc= RECHEADER_READ_ERROR;
          goto exit_and_free;
        }
      }
      if (translog_get_next_chunk(scanner))
      {
        if (scanner == &internal_scanner)
          translog_destroy_scanner(scanner);
        rc= RECHEADER_READ_ERROR;
        goto exit_and_free;
      }
      if (scanner->page == END_OF_LOG)
      {
        if (scanner == &internal_scanner)
          translog_destroy_scanner(scanner);
        rc= RECHEADER_READ_EOF;
        goto exit_and_free;
      }
      page= scanner->page;
      page_offset= scanner->page_offset;
      src= page + page_offset + header_to_skip;
      chunk_len= uint2korr(src - 2 - 2);
      DBUG_PRINT("info", ("Chunk len: %u", (uint) chunk_len));
      page_rest= (uint16) (TRANSLOG_PAGE_SIZE - (src - page));
    }

    if (scanner == NULL)
    {
      DBUG_PRINT("info", ("use internal scanner"));
      scanner= &internal_scanner;
    }
    else
    {
      translog_destroy_scanner(scanner);
    }
    base_lsn= buff->groups[0].addr;
    translog_scanner_init(base_lsn, 1, scanner, scanner == &internal_scanner);
    /* first group chunk is always chunk type 2 */
    page= scanner->page;
    page_offset= scanner->page_offset;
    src= page + page_offset + 1;
    page_rest= (uint16) (TRANSLOG_PAGE_SIZE - (src - page));
    body_len= page_rest;
    if (scanner == &internal_scanner)
      translog_destroy_scanner(scanner);
  }
  if (lsns)
  {
    uchar *start= src;
    src= translog_relative_LSN_decode(base_lsn, src, dst, lsns);
    lsns*= LSN_STORE_SIZE;
    dst+= lsns;
    length-= lsns;
    buff->record_length+= (buff->compressed_LSN_economy=
                           (int) (lsns - (src - start)));
    DBUG_PRINT("info", ("lsns: %u  length: %u  economy: %d  new length: %lu",
                        lsns / LSN_STORE_SIZE, (uint) length,
                        (int) buff->compressed_LSN_economy,
                        (ulong) buff->record_length));
    body_len-= (uint16) (src - start);
  }
  else
    buff->compressed_LSN_economy= 0;

  DBUG_ASSERT(body_len >= length);
  body_len-= length;
  memcpy(dst, src, length);
  buff->non_header_data_start_offset= (uint16) (src + length - page);
  buff->non_header_data_len= body_len;
  DBUG_PRINT("info", ("non_header_data_start_offset: %u  len: %u  buffer: %u",
                      buff->non_header_data_start_offset,
                      buff->non_header_data_len, buffer_length));
  DBUG_RETURN(buffer_length);

exit_and_free:
  my_free(buff->groups, MYF(0));
  buff->groups_no= 0; /* prevent try to use of buff->groups */
  DBUG_RETURN(rc);
}


/**
   @brief Read record header from the given buffer

   @param page            page content buffer
   @param page_offset     offset of the chunk in the page
   @param buff            destination buffer
   @param scanner         If this is set the scanner will be moved to the
                          record header page (differ from LSN page in case of
                          multi-group records)

   @return Length of header or operation status
     @retval RECHEADER_READ_ERROR  error
     @retval #                     number of bytes in
                                   TRANSLOG_HEADER_BUFFER::header where
                                   stored decoded part of the header
*/

int translog_read_record_header_from_buffer(uchar *page,
                                            uint16 page_offset,
                                            TRANSLOG_HEADER_BUFFER *buff,
                                            TRANSLOG_SCANNER_DATA *scanner)
{
  translog_size_t res;
  DBUG_ENTER("translog_read_record_header_from_buffer");
  DBUG_ASSERT(translog_is_LSN_chunk(page[page_offset]));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  DBUG_PRINT("info", ("page byte: 0x%x  offset: %u",
                      (uint) page[page_offset], (uint) page_offset));
  buff->type= (page[page_offset] & TRANSLOG_REC_TYPE);
  buff->short_trid= uint2korr(page + page_offset + 1);
  DBUG_PRINT("info", ("Type %u, Short TrID %u, LSN (%lu,0x%lx)",
                      (uint) buff->type, (uint)buff->short_trid,
                      LSN_IN_PARTS(buff->lsn)));
  /* Read required bytes from the header and call hook */
  switch (log_record_type_descriptor[buff->type].rclass) {
  case LOGRECTYPE_VARIABLE_LENGTH:
    res= translog_variable_length_header(page, page_offset, buff,
                                         scanner);
    break;
  case LOGRECTYPE_PSEUDOFIXEDLENGTH:
  case LOGRECTYPE_FIXEDLENGTH:
    res= translog_fixed_length_header(page, page_offset, buff);
    break;
  default:
    DBUG_ASSERT(0); /* we read some junk (got no LSN) */
    res= RECHEADER_READ_ERROR;
  }
  DBUG_RETURN(res);
}


/**
   @brief Read record header and some fixed part of a record (the part depend
   on record type).

   @param lsn             log record serial number (address of the record)
   @param buff            log record header buffer

   @note Some type of record can be read completely by this call
   @note "Decoded" header stored in TRANSLOG_HEADER_BUFFER::header (relative
   LSN can be translated to absolute one), some fields can be added (like
   actual header length in the record if the header has variable length)

   @return Length of header or operation status
     @retval RECHEADER_READ_ERROR  error
     @retval #                     number of bytes in
                                   TRANSLOG_HEADER_BUFFER::header where
                                   stored decoded part of the header
*/

int translog_read_record_header(LSN lsn, TRANSLOG_HEADER_BUFFER *buff)
{
  TRANSLOG_PAGE_SIZE_BUFF psize_buff;
  uchar *page;
  translog_size_t res, page_offset= LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE;
  PAGECACHE_BLOCK_LINK *direct_link;
  TRANSLOG_ADDRESS addr;
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_read_record_header");
  DBUG_PRINT("enter", ("LSN: (%lu,0x%lx)", LSN_IN_PARTS(lsn)));
  DBUG_ASSERT(LSN_OFFSET(lsn) % TRANSLOG_PAGE_SIZE != 0);
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  buff->lsn= lsn;
  buff->groups_no= 0;
  data.addr= &addr;
  data.was_recovered= 0;
  addr= lsn;
  addr-= page_offset; /* offset decreasing */
  res= (!(page= translog_get_page(&data, psize_buff.buffer, &direct_link))) ?
    RECHEADER_READ_ERROR :
    translog_read_record_header_from_buffer(page, page_offset, buff, 0);
  translog_free_link(direct_link);
  DBUG_RETURN(res);
}


/**
   @brief Read record header and some fixed part of a record (the part depend
   on record type).

   @param scan            scanner position to read
   @param buff            log record header buffer
   @param move_scanner    request to move scanner to the header position

   @note Some type of record can be read completely by this call
   @note "Decoded" header stored in TRANSLOG_HEADER_BUFFER::header (relative
   LSN can be translated to absolute one), some fields can be added (like
   actual header length in the record if the header has variable length)

   @return Length of header or operation status
     @retval RECHEADER_READ_ERROR  error
     @retval #                     number of bytes in
                                   TRANSLOG_HEADER_BUFFER::header where stored
                                   decoded part of the header
*/

int translog_read_record_header_scan(TRANSLOG_SCANNER_DATA *scanner,
                                     TRANSLOG_HEADER_BUFFER *buff,
                                     my_bool move_scanner)
{
  translog_size_t res;
  DBUG_ENTER("translog_read_record_header_scan");
  DBUG_PRINT("enter", ("Scanner: Cur: (%lu,0x%lx)  Hrz: (%lu,0x%lx)  "
                       "Lst: (%lu,0x%lx)  Offset: %u(%x)  fixed %d",
                       LSN_IN_PARTS(scanner->page_addr),
                       LSN_IN_PARTS(scanner->horizon),
                       LSN_IN_PARTS(scanner->last_file_page),
                       (uint) scanner->page_offset,
                       (uint) scanner->page_offset, scanner->fixed_horizon));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  buff->groups_no= 0;
  buff->lsn= scanner->page_addr;
  buff->lsn+= scanner->page_offset; /* offset increasing */
  res= translog_read_record_header_from_buffer(scanner->page,
                                               scanner->page_offset,
                                               buff,
                                               (move_scanner ?
                                                scanner : 0));
  DBUG_RETURN(res);
}


/**
   @brief Read record header and some fixed part of the next record (the part
   depend on record type).

   @param scanner         data for scanning if lsn is NULL scanner data
                          will be used for continue scanning.
                          The scanner can be NULL.

   @param buff            log record header buffer

   @return Length of header or operation status
     @retval RECHEADER_READ_ERROR  error
     @retval RECHEADER_READ_EOF    EOF
     @retval #                     number of bytes in
                                   TRANSLOG_HEADER_BUFFER::header where
                                   stored decoded part of the header
*/

int translog_read_next_record_header(TRANSLOG_SCANNER_DATA *scanner,
                                     TRANSLOG_HEADER_BUFFER *buff)
{
  translog_size_t res;

  DBUG_ENTER("translog_read_next_record_header");
  buff->groups_no= 0;        /* to be sure that we will free it right */
  DBUG_PRINT("enter", ("scanner: 0x%lx", (ulong) scanner));
  DBUG_PRINT("info", ("Scanner: Cur: (%lu,0x%lx)  Hrz: (%lu,0x%lx)  "
                      "Lst: (%lu,0x%lx)  Offset: %u(%x)  fixed: %d",
                      LSN_IN_PARTS(scanner->page_addr),
                      LSN_IN_PARTS(scanner->horizon),
                      LSN_IN_PARTS(scanner->last_file_page),
                      (uint) scanner->page_offset,
                      (uint) scanner->page_offset, scanner->fixed_horizon));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  do
  {
    if (translog_get_next_chunk(scanner))
      DBUG_RETURN(RECHEADER_READ_ERROR);
    if (scanner->page == END_OF_LOG)
    {
       DBUG_PRINT("info", ("End of file from the scanner"));
       /* Last record was read */
       buff->lsn= LSN_IMPOSSIBLE;
       DBUG_RETURN(RECHEADER_READ_EOF);
    }
    DBUG_PRINT("info", ("Page: (%lu,0x%lx)  offset: %lu  byte: %x",
                        LSN_IN_PARTS(scanner->page_addr),
                        (ulong) scanner->page_offset,
                        (uint) scanner->page[scanner->page_offset]));
  } while (!translog_is_LSN_chunk(scanner->page[scanner->page_offset]) &&
           scanner->page[scanner->page_offset] != TRANSLOG_FILLER);

  if (scanner->page[scanner->page_offset] == TRANSLOG_FILLER)
  {
    DBUG_PRINT("info", ("End of file"));
    /* Last record was read */
    buff->lsn= LSN_IMPOSSIBLE;
    /* Return 'end of log' marker */
    res= RECHEADER_READ_EOF;
  }
  else
    res= translog_read_record_header_scan(scanner, buff, 0);
  DBUG_RETURN(res);
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

static my_bool translog_record_read_next_chunk(TRANSLOG_READER_DATA *data)
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
    translog_destroy_scanner(&data->scanner);
    translog_scanner_init(data->header.groups[data->current_group].addr,
                          1, &data->scanner, 1);
  }
  else
  {
    data->current_chunk++;
    if (translog_get_next_chunk(&data->scanner))
      DBUG_RETURN(1);
     if (data->scanner.page == END_OF_LOG)
     {
       /*
         Actually it should not happened, but we want to quit nicely in case
         of a truncated log
       */
       DBUG_RETURN(1);
     }
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
    translog_destroy_scanner(&data->scanner);
    translog_scanner_init(data->header.chunk0_data_addr, 1, &data->scanner, 1);
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
    translog_get_chunk_header_length(data->scanner.page +
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
                                         TRANSLOG_READER_DATA *data)
{
  int read_header;
  DBUG_ENTER("translog_init_reader_data");
  if (translog_scanner_init(lsn, 1, &data->scanner, 1) ||
      ((read_header=
        translog_read_record_header_scan(&data->scanner, &data->header, 1))
       == RECHEADER_READ_ERROR))
    DBUG_RETURN(1);
  data->read_header= read_header;
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


/**
  @brief Destroy reader data object
*/

static void translog_destroy_reader_data(TRANSLOG_READER_DATA *data)
{
  translog_destroy_scanner(&data->scanner);
  translog_free_record_header(&data->header);
}


/*
  Read a part of the record.

  SYNOPSIS
    translog_read_record_header()
    lsn                  log record serial number (address of the record)
    offset               From the beginning of the record beginning (read
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
                                     uchar *buffer,
                                     TRANSLOG_READER_DATA *data)
{
  translog_size_t requested_length= length;
  translog_size_t end= offset + length;
  TRANSLOG_READER_DATA internal_data;
  DBUG_ENTER("translog_read_record");
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  if (data == NULL)
  {
    DBUG_ASSERT(lsn != LSN_IMPOSSIBLE);
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
                      LSN_IN_PARTS(data->scanner.page_addr),
                      LSN_IN_PARTS(data->scanner.horizon),
                      LSN_IN_PARTS(data->scanner.last_file_page),
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
    {
      translog_destroy_reader_data(data);
      DBUG_RETURN(requested_length);
    }
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
      set_if_smaller(len, length); /* in case we read beyond record's end */
      DBUG_ASSERT(offset >= data->current_offset);
      memcpy(buffer,
              data->scanner.page + data->body_offset +
              (offset - data->current_offset), len);
      length-= len;
      if (length == 0)
      {
        translog_destroy_reader_data(data);
        DBUG_RETURN(requested_length);
      }
      offset+= len;
      buffer+= len;
      DBUG_PRINT("info",
                 ("len: %u  offset: %lu  curr: %lu  length: %lu",
                  len, (ulong) offset, (ulong) data->current_offset,
                  (ulong) length));
    }
    if (translog_record_read_next_chunk(data))
    {
      translog_destroy_reader_data(data);
      DBUG_RETURN(requested_length - length);
    }
  }
}


/*
  @brief Force skipping to the next buffer

  @todo Do not copy old page content if all page protections are switched off
  (because we do not need calculate something or change old parts of the page)
*/

static void translog_force_current_buffer_to_finish()
{
  TRANSLOG_ADDRESS new_buff_beginning;
  uint16 old_buffer_no= log_descriptor.bc.buffer_no;
  uint16 new_buffer_no= (old_buffer_no + 1) % TRANSLOG_BUFFERS_NO;
  struct st_translog_buffer *new_buffer= (log_descriptor.buffers +
                                          new_buffer_no);
  struct st_translog_buffer *old_buffer= log_descriptor.bc.buffer;
  uchar *data= log_descriptor.bc.ptr - log_descriptor.bc.current_page_fill;
  uint16 left= TRANSLOG_PAGE_SIZE - log_descriptor.bc.current_page_fill;
  uint16 current_page_fill, write_counter, previous_offset;
  DBUG_ENTER("translog_force_current_buffer_to_finish");
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  "
                       "Buffer addr: (%lu,0x%lx)  "
                       "Page addr: (%lu,0x%lx)  "
                       "size: %lu (%lu)  Pg: %u  left: %u  in progress %u",
                       (uint) log_descriptor.bc.buffer_no,
                       (ulong) log_descriptor.bc.buffer,
                       LSN_IN_PARTS(log_descriptor.bc.buffer->offset),
                       (ulong) LSN_FILE_NO(log_descriptor.horizon),
                       (ulong) (LSN_OFFSET(log_descriptor.horizon) -
                                log_descriptor.bc.current_page_fill),
                       (ulong) log_descriptor.bc.buffer->size,
                       (ulong) (log_descriptor.bc.ptr -log_descriptor.bc.
                                buffer->buffer),
                       (uint) log_descriptor.bc.current_page_fill,
                       (uint) left,
                       (uint) log_descriptor.bc.buffer->
                       copy_to_buffer_in_progress));
  translog_lock_assert_owner();
  LINT_INIT(current_page_fill);
  new_buff_beginning= log_descriptor.bc.buffer->offset;
  new_buff_beginning+= log_descriptor.bc.buffer->size; /* increase offset */

  DBUG_ASSERT(log_descriptor.bc.ptr !=NULL);
  DBUG_ASSERT(LSN_FILE_NO(log_descriptor.horizon) ==
              LSN_FILE_NO(log_descriptor.bc.buffer->offset));
  translog_check_cursor(&log_descriptor.bc);
  DBUG_ASSERT(left < TRANSLOG_PAGE_SIZE);
  if (left)
  {
    /*
       TODO: if 'left' is so small that can't hold any other record
       then do not move the page
    */
    DBUG_PRINT("info", ("left: %u", (uint) left));

    /* decrease offset */
    new_buff_beginning-= log_descriptor.bc.current_page_fill;
    current_page_fill= log_descriptor.bc.current_page_fill;

    memset(log_descriptor.bc.ptr, TRANSLOG_FILLER, left);
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
    log_descriptor.bc.current_page_fill= 0;
  }

  translog_buffer_lock(new_buffer);
#ifndef DBUG_OFF
  {
    TRANSLOG_ADDRESS offset= new_buffer->offset;
    TRANSLOG_FILE *file= new_buffer->file;
    uint8 ver= new_buffer->ver;
    translog_lock_assert_owner();
#endif
    translog_wait_for_buffer_free(new_buffer);
#ifndef DBUG_OFF
    /* We keep the handler locked so nobody can start this new buffer */
    DBUG_ASSERT(offset == new_buffer->offset && new_buffer->file == NULL &&
                (file == NULL ? ver : (uint8)(ver + 1)) == new_buffer->ver);
  }
#endif

  write_counter= log_descriptor.bc.write_counter;
  previous_offset= log_descriptor.bc.previous_offset;
  translog_start_buffer(new_buffer, &log_descriptor.bc, new_buffer_no);
  /* Fix buffer offset (which was incorrectly set to horizon) */
  log_descriptor.bc.buffer->offset= new_buff_beginning;
  log_descriptor.bc.write_counter= write_counter;
  log_descriptor.bc.previous_offset= previous_offset;
  new_buffer->prev_last_lsn= BUFFER_MAX_LSN(old_buffer);
  DBUG_PRINT("info", ("prev_last_lsn set to (%lu,0x%lx)  buffer: 0x%lx",
                      LSN_IN_PARTS(new_buffer->prev_last_lsn),
                      (ulong) new_buffer));

  /*
    Advances this log pointer, increases writers and let other threads to
    write to the log while we process old page content
  */
  if (left)
  {
    log_descriptor.bc.ptr+= current_page_fill;
    log_descriptor.bc.buffer->size= log_descriptor.bc.current_page_fill=
      current_page_fill;
    new_buffer->overlay= 1;
  }
  else
    translog_new_page_header(&log_descriptor.horizon, &log_descriptor.bc);
  translog_buffer_increase_writers(new_buffer);
  translog_buffer_unlock(new_buffer);

  /*
    We have to wait until all writers finish before start changing the
    pages by applying protection and copying the page content in the
    new buffer.
  */
#ifndef DBUG_OFF
  {
    TRANSLOG_ADDRESS offset= old_buffer->offset;
    TRANSLOG_FILE *file= old_buffer->file;
    uint8 ver= old_buffer->ver;
#endif
    /*
      Now only one thread can flush log (buffer can flush many threads but
      log flush log flush where this function is used can do only one thread)
      so no other thread can set is_closing_buffer.
    */
    DBUG_ASSERT(!old_buffer->is_closing_buffer);
    old_buffer->is_closing_buffer= 1; /* Other flushes will wait */
    DBUG_PRINT("enter", ("Buffer #%u 0x%lx  is_closing_buffer set",
                         (uint) old_buffer->buffer_no, (ulong) old_buffer));
    translog_wait_for_writers(old_buffer);
#ifndef DBUG_OFF
    /* We blocked flushing this buffer so the buffer should not changed */
    DBUG_ASSERT(offset == old_buffer->offset && file == old_buffer->file &&
                ver == old_buffer->ver);
  }
#endif

  if (log_descriptor.flags & TRANSLOG_SECTOR_PROTECTION)
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

  if (log_descriptor.flags & TRANSLOG_PAGE_CRC)
  {
    uint32 crc= translog_crc(data + log_descriptor.page_overhead,
                             TRANSLOG_PAGE_SIZE -
                             log_descriptor.page_overhead);
    DBUG_PRINT("info", ("CRC: 0x%lx", (ulong) crc));
    int4store(data + 3 + 3 + 1, crc);
  }
  old_buffer->is_closing_buffer= 0;
  DBUG_PRINT("enter", ("Buffer #%u 0x%lx  is_closing_buffer cleared",
                       (uint) old_buffer->buffer_no, (ulong) old_buffer));
  pthread_cond_broadcast(&old_buffer->waiting_filling_buffer);

  if (left)
  {
    /*
      TODO: do not copy beginning of the page if we have no CRC or sector
      checks on
    */
    memcpy(new_buffer->buffer, data, current_page_fill);
  }
  old_buffer->next_buffer_offset= new_buffer->offset;
  translog_buffer_lock(new_buffer);
  new_buffer->prev_buffer_offset= old_buffer->offset;
  translog_buffer_decrease_writers(new_buffer);
  translog_buffer_unlock(new_buffer);

  DBUG_VOID_RETURN;
}


/**
  @brief Waits while given lsn will be flushed

  @param  lsn            log record serial number up to which (inclusive)
                         the log has to be flushed
*/

void  translog_flush_wait_for_end(LSN lsn)
{
  DBUG_ENTER("translog_flush_wait_for_end");
  DBUG_PRINT("enter", ("LSN: (%lu,0x%lx)", LSN_IN_PARTS(lsn)));
  safe_mutex_assert_owner(&log_descriptor.log_flush_lock);
  while (cmp_translog_addr(log_descriptor.flushed, lsn) < 0)
    pthread_cond_wait(&log_descriptor.log_flush_cond,
                      &log_descriptor.log_flush_lock);
  DBUG_VOID_RETURN;
}


/**
  @brief Sets goal for the next flush pass and waits for this pass end.

  @param  lsn            log record serial number up to which (inclusive)
                         the log has to be flushed
*/

void translog_flush_set_new_goal_and_wait(TRANSLOG_ADDRESS lsn)
{
  int flush_no= log_descriptor.flush_no;
  DBUG_ENTER("translog_flush_set_new_goal_and_wait");
  DBUG_PRINT("enter", ("LSN: (%lu,0x%lx)", LSN_IN_PARTS(lsn)));
  safe_mutex_assert_owner(&log_descriptor.log_flush_lock);
  if (cmp_translog_addr(lsn, log_descriptor.next_pass_max_lsn) > 0)
  {
    log_descriptor.next_pass_max_lsn= lsn;
    log_descriptor.max_lsn_requester= pthread_self();
  }
  while (flush_no == log_descriptor.flush_no)
  {
    pthread_cond_wait(&log_descriptor.log_flush_cond,
                      &log_descriptor.log_flush_lock);
  }
  DBUG_VOID_RETURN;
}


/**
  @brief Flush the log up to given LSN (included)

  @param  lsn            log record serial number up to which (inclusive)
                         the log has to be flushed

  @return Operation status
    @retval 0      OK
    @retval 1      Error

*/

my_bool translog_flush(TRANSLOG_ADDRESS lsn)
{
  LSN sent_to_disk= LSN_IMPOSSIBLE;
  TRANSLOG_ADDRESS flush_horizon;
  uint fn, i;
  dirty_buffer_mask_t dirty_buffer_mask;
  uint8 last_buffer_no, start_buffer_no;
  my_bool rc= 0;
  DBUG_ENTER("translog_flush");
  DBUG_PRINT("enter", ("Flush up to LSN: (%lu,0x%lx)", LSN_IN_PARTS(lsn)));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);
  LINT_INIT(sent_to_disk);
  LINT_INIT(last_buffer_no);

  pthread_mutex_lock(&log_descriptor.log_flush_lock);
  DBUG_PRINT("info", ("Everything is flushed up to (%lu,0x%lx)",
                      LSN_IN_PARTS(log_descriptor.flushed)));
  if (cmp_translog_addr(log_descriptor.flushed, lsn) >= 0)
  {
    pthread_mutex_unlock(&log_descriptor.log_flush_lock);
    DBUG_RETURN(0);
  }
  if (log_descriptor.flush_in_progress)
  {
    translog_flush_set_new_goal_and_wait(lsn);
    if (!pthread_equal(log_descriptor.max_lsn_requester, pthread_self()))
    {
      /* fix lsn if it was horizon */
      if (cmp_translog_addr(lsn, log_descriptor.bc.buffer->last_lsn) > 0)
          lsn= BUFFER_MAX_LSN(log_descriptor.bc.buffer);
      translog_flush_wait_for_end(lsn);
      pthread_mutex_unlock(&log_descriptor.log_flush_lock);
      DBUG_RETURN(0);
    }
    log_descriptor.next_pass_max_lsn= LSN_IMPOSSIBLE;
  }
  log_descriptor.flush_in_progress= 1;
  flush_horizon= log_descriptor.previous_flush_horizon;
  DBUG_PRINT("info", ("flush_in_progress is set"));
  pthread_mutex_unlock(&log_descriptor.log_flush_lock);

  translog_lock();
  if (log_descriptor.is_everything_flushed)
  {
    DBUG_PRINT("info", ("everything is flushed"));
    rc= (translog_status == TRANSLOG_READONLY);
    translog_unlock();
    goto out;
  }

  /*
    We will recheck information when will lock buffers one by
    one so we can use unprotected read here (this is just for
    speed up buffers processing)
  */
  dirty_buffer_mask= log_descriptor.dirty_buffer_mask;
  DBUG_PRINT("info", ("Dirty buffer mask: %lx  current buffer: %u",
                      (ulong) dirty_buffer_mask,
                      (uint) log_descriptor.bc.buffer_no));
  for (i= (log_descriptor.bc.buffer_no + 1) % TRANSLOG_BUFFERS_NO;
       i != log_descriptor.bc.buffer_no && !(dirty_buffer_mask & (1 << i));
       i= (i + 1) % TRANSLOG_BUFFERS_NO) {}
  start_buffer_no= i;

  DBUG_PRINT("info",
             ("start from: %u  current: %u  prev last lsn: (%lu,0x%lx)",
              (uint) start_buffer_no, (uint) log_descriptor.bc.buffer_no,
              LSN_IN_PARTS(log_descriptor.bc.buffer->prev_last_lsn)));


  /*
    if LSN up to which we have to flush bigger then maximum LSN of previous
    buffer and at least one LSN was saved in the current buffer (last_lsn !=
    LSN_IMPOSSIBLE) then we better finish the current buffer.
  */
  if (cmp_translog_addr(lsn, log_descriptor.bc.buffer->prev_last_lsn) > 0 &&
      log_descriptor.bc.buffer->last_lsn != LSN_IMPOSSIBLE)
  {
    struct st_translog_buffer *buffer= log_descriptor.bc.buffer;
    lsn= log_descriptor.bc.buffer->last_lsn; /* fix lsn if it was horizon */
    DBUG_PRINT("info", ("LSN to flush fixed to last lsn: (%lu,0x%lx)",
               LSN_IN_PARTS(lsn)));
    last_buffer_no= log_descriptor.bc.buffer_no;
    log_descriptor.is_everything_flushed= 1;
    translog_force_current_buffer_to_finish();
    translog_buffer_unlock(buffer);
  }
  else if (log_descriptor.bc.buffer->prev_last_lsn != LSN_IMPOSSIBLE)
  {
    /* fix lsn if it was horizon */
    lsn= log_descriptor.bc.buffer->prev_last_lsn;
    DBUG_PRINT("info", ("LSN to flush fixed to prev last lsn: (%lu,0x%lx)",
               LSN_IN_PARTS(lsn)));
    last_buffer_no= ((log_descriptor.bc.buffer_no + TRANSLOG_BUFFERS_NO -1) %
                     TRANSLOG_BUFFERS_NO);
    translog_unlock();
  }
  else if (log_descriptor.bc.buffer->last_lsn == LSN_IMPOSSIBLE)
  {
    DBUG_PRINT("info", ("There is no LSNs yet generated => do nothing"));
    translog_unlock();
    goto out;
  }
  sent_to_disk= translog_get_sent_to_disk();
  if (cmp_translog_addr(lsn, sent_to_disk) > 0)
  {

    DBUG_PRINT("info", ("Start buffer #: %u  last buffer #: %u",
                        (uint) start_buffer_no, (uint) last_buffer_no));
    last_buffer_no= (last_buffer_no + 1) % TRANSLOG_BUFFERS_NO;
    i= start_buffer_no;
    do
    {
      struct st_translog_buffer *buffer= log_descriptor.buffers + i;
      translog_buffer_lock(buffer);
      DBUG_PRINT("info", ("Check buffer: 0x%lx  #: %u  "
                          "prev last LSN: (%lu,0x%lx)  "
                          "last LSN: (%lu,0x%lx)  status: %s",
                          (ulong)(buffer),
                          (uint) i,
                          LSN_IN_PARTS(buffer->prev_last_lsn),
                          LSN_IN_PARTS(buffer->last_lsn),
                          (buffer->file ?
                           "dirty" : "closed")));
      if (buffer->prev_last_lsn <= lsn &&
          buffer->file != NULL)
      {
        DBUG_ASSERT(flush_horizon <= buffer->offset + buffer->size);
        flush_horizon= buffer->offset + buffer->size;
        translog_buffer_flush(buffer);
      }
      translog_buffer_unlock(buffer);
      i= (i + 1) % TRANSLOG_BUFFERS_NO;
    } while (i != last_buffer_no);
    sent_to_disk= translog_get_sent_to_disk();
  }

  /* sync files from previous flush till current one */
  for (fn= LSN_FILE_NO(log_descriptor.flushed); fn <= LSN_FILE_NO(lsn); fn++)
  {
    TRANSLOG_FILE *file= get_logfile_by_number(fn);
    DBUG_ASSERT(file != NULL);
    if (!file->is_sync)
    {
      if (my_sync(file->handler.file, MYF(MY_WME)))
      {
        rc= 1;
        translog_stop_writing();
        sent_to_disk= LSN_IMPOSSIBLE;
        goto out;
      }
      file->is_sync= 1;
    }
  }

  if (sync_log_dir >= TRANSLOG_SYNC_DIR_ALWAYS &&
      (LSN_FILE_NO(log_descriptor.previous_flush_horizon) !=
       LSN_FILE_NO(flush_horizon) ||
       ((LSN_OFFSET(log_descriptor.previous_flush_horizon) - 1) /
        TRANSLOG_PAGE_SIZE) !=
       ((LSN_OFFSET(flush_horizon) - 1) / TRANSLOG_PAGE_SIZE)))
    rc|= sync_dir(log_descriptor.directory_fd, MYF(MY_WME | MY_IGNORE_BADFD));
  log_descriptor.previous_flush_horizon= flush_horizon;
out:
  pthread_mutex_lock(&log_descriptor.log_flush_lock);
  if (sent_to_disk != LSN_IMPOSSIBLE)
    log_descriptor.flushed= sent_to_disk;
  log_descriptor.flush_in_progress= 0;
  log_descriptor.flush_no++;
  DBUG_PRINT("info", ("flush_in_progress is dropped"));
  pthread_mutex_unlock(&log_descriptor.log_flush_lock);\
  pthread_cond_broadcast(&log_descriptor.log_flush_cond);
  DBUG_RETURN(rc);
}


/**
   @brief Gives a 2-byte-id to MARIA_SHARE and logs this fact

   If a MARIA_SHARE does not yet have a 2-byte-id (unique over all currently
   open MARIA_SHAREs), give it one and record this assignment in the log
   (LOGREC_FILE_ID log record).

   @param  tbl_info        table
   @param  trn             calling transaction

   @return Operation status
     @retval 0      OK
     @retval 1      Error

   @note Can be called even if share already has an id (then will do nothing)
*/

int translog_assign_id_to_share(MARIA_HA *tbl_info, TRN *trn)
{
  uint16 id;
  MARIA_SHARE *share= tbl_info->s;
  /*
    If you give an id to a non-BLOCK_RECORD table, you also need to release
    this id somewhere. Then you can change the assertion.
  */
  DBUG_ASSERT(share->data_file_type == BLOCK_RECORD);
  /* re-check under mutex to avoid having 2 ids for the same share */
  pthread_mutex_lock(&share->intern_lock);
  if (likely(share->id == 0))
  {
    LSN lsn;
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
    uchar log_data[FILEID_STORE_SIZE];
    /* Inspired by set_short_trid() of trnman.c */
    uint i= share->kfile.file % SHARE_ID_MAX + 1;
    id= 0;
    do
    {
      my_atomic_rwlock_wrlock(&LOCK_id_to_share);
      for ( ; i <= SHARE_ID_MAX ; i++) /* the range is [1..SHARE_ID_MAX] */
      {
        void *tmp= NULL;
        if (id_to_share[i] == NULL &&
            my_atomic_casptr((void **)&id_to_share[i], &tmp, share))
        {
          id= (uint16) i;
          break;
        }
      }
      my_atomic_rwlock_wrunlock(&LOCK_id_to_share);
      i= 1; /* scan the whole array */
    } while (id == 0);
    DBUG_PRINT("info", ("id_to_share: 0x%lx -> %u", (ulong)share, id));
    fileid_store(log_data, id);
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    /*
      open_file_name is an unresolved name (symlinks are not resolved, datadir
      is not realpath-ed, etc) which is good: the log can be moved to another
      directory and continue working.
    */
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str=
      (uchar *)share->open_file_name.str;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length=
      share->open_file_name.length + 1;
    /*
      We can't unlock share->intern_lock before the log entry is written to
      ensure no one uses the id before it's logged.
    */
    if (unlikely(translog_write_record(&lsn, LOGREC_FILE_ID, trn, tbl_info,
                                       (translog_size_t)
                                       (sizeof(log_data) +
                                        log_array[TRANSLOG_INTERNAL_PARTS +
                                                  1].length),
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, NULL, NULL)))
    {
      pthread_mutex_unlock(&share->intern_lock);
      return 1;
    }
    /*
      Now when translog record is done, we can set share->id.
      If we set it before, then translog_write_record may pick up the id
      before it's written to the log.
    */
    share->id= id;
  }
  pthread_mutex_unlock(&share->intern_lock);
  return 0;
}


/**
   @brief Recycles a MARIA_SHARE's short id.

   @param  share           table

   @note Must be called only if share has an id (i.e. id != 0)
*/

void translog_deassign_id_from_share(MARIA_SHARE *share)
{
  DBUG_PRINT("info", ("id_to_share: 0x%lx id %u -> 0",
                      (ulong)share, share->id));
  /*
    We don't need any mutex as we are called only when closing the last
    instance of the table or at the end of REPAIR: no writes can be
    happening. But a Checkpoint may be reading share->id, so we require this
    mutex:
  */
  safe_mutex_assert_owner(&share->intern_lock);
  my_atomic_rwlock_rdlock(&LOCK_id_to_share);
  my_atomic_storeptr((void **)&id_to_share[share->id], 0);
  my_atomic_rwlock_rdunlock(&LOCK_id_to_share);
  share->id= 0;
  /* useless but safety: */
  share->lsn_of_file_id= LSN_IMPOSSIBLE;
}


void translog_assign_id_to_share_from_recovery(MARIA_SHARE *share,
                                               uint16 id)
{
  DBUG_ASSERT(maria_in_recovery && !maria_multi_threaded);
  DBUG_ASSERT(share->data_file_type == BLOCK_RECORD);
  DBUG_ASSERT(share->id == 0);
  DBUG_ASSERT(id_to_share[id] == NULL);
  id_to_share[share->id= id]= share;
}


/**
   @brief check if such log file exists

   @param file_no number of the file to test

   @retval 0 no such file
   @retval 1 there is file with such number
*/

my_bool translog_is_file(uint file_no)
{
  MY_STAT stat_buff;
  char path[FN_REFLEN];
  return (test(my_stat(translog_filename_by_fileno(file_no, path),
                       &stat_buff, MYF(0))));
}


/**
  @brief returns minimum log file number

  @param horizon         the end of the log
  @param is_protected    true if it is under purge_log protection

  @retval minimum file number
  @retval 0 no files found
*/

static uint32 translog_first_file(TRANSLOG_ADDRESS horizon, int is_protected)
{
  uint min_file= 0, max_file;
  DBUG_ENTER("translog_first_file");
  if (!is_protected)
    pthread_mutex_lock(&log_descriptor.purger_lock);
  if (log_descriptor.min_file_number &&
      translog_is_file(log_descriptor.min_file_number))
  {
    DBUG_PRINT("info", ("cached %lu",
                        (ulong) log_descriptor.min_file_number));
    if (!is_protected)
      pthread_mutex_unlock(&log_descriptor.purger_lock);
    DBUG_RETURN(log_descriptor.min_file_number);
  }

  max_file= LSN_FILE_NO(horizon);

  /* binary search for last file */
  while (min_file != max_file && min_file != (max_file - 1))
  {
    uint test= (min_file + max_file) / 2;
    DBUG_PRINT("info", ("min_file: %u  test: %u  max_file: %u",
                        min_file, test, max_file));
    if (test == max_file)
      test--;
    if (translog_is_file(test))
      max_file= test;
    else
      min_file= test;
  }
  log_descriptor.min_file_number= max_file;
  if (!is_protected)
    pthread_mutex_unlock(&log_descriptor.purger_lock);
  DBUG_PRINT("info", ("first file :%lu", (ulong) max_file));
  DBUG_ASSERT(max_file >= 1);
  DBUG_RETURN(max_file);
}


/**
  @brief returns the most close LSN higher the given chunk address

  @param addr the chunk address to start from
  @param horizon the horizon if it is known or LSN_IMPOSSIBLE

  @retval LSN_ERROR Error
  @retval LSN_IMPOSSIBLE no LSNs after the address
  @retval # LSN of the most close LSN higher the given chunk address
*/

LSN translog_next_LSN(TRANSLOG_ADDRESS addr, TRANSLOG_ADDRESS horizon)
{
  TRANSLOG_SCANNER_DATA scanner;
  LSN result;
  DBUG_ENTER("translog_next_LSN");

  if (horizon == LSN_IMPOSSIBLE)
    horizon= translog_get_horizon();

  if (addr == horizon)
    DBUG_RETURN(LSN_IMPOSSIBLE);

  translog_scanner_init(addr, 0, &scanner, 1);
  /*
    addr can point not to a chunk beginning but page end so next
    page beginning.
  */
  if (addr % TRANSLOG_PAGE_SIZE == 0)
  {
    /*
      We are emulating the page end which cased such horizon value to
      trigger translog_scanner_eop().

      We can't just increase addr on page header overhead because it
      can be file end so we allow translog_get_next_chunk() to skip
      to the next page in correct way
    */
    scanner.page_addr-= TRANSLOG_PAGE_SIZE;
    scanner.page_offset= TRANSLOG_PAGE_SIZE;
#ifndef DBUG_OFF
    scanner.page= NULL; /* prevent using incorrect page content */
#endif
  }
  /* addr can point not to a chunk beginning but to a page end */
  if (translog_scanner_eop(&scanner))
  {
    if (translog_get_next_chunk(&scanner))
    {
      result= LSN_ERROR;
      goto out;
    }
    if (scanner.page == END_OF_LOG)
    {
      result= LSN_IMPOSSIBLE;
      goto out;
    }
  }

  while (!translog_is_LSN_chunk(scanner.page[scanner.page_offset]) &&
         scanner.page[scanner.page_offset] != TRANSLOG_FILLER)
  {
    if (translog_get_next_chunk(&scanner))
    {
      result= LSN_ERROR;
      goto out;
    }
    if (scanner.page == END_OF_LOG)
    {
      result= LSN_IMPOSSIBLE;
      goto out;
    }
  }

  if (scanner.page[scanner.page_offset] == TRANSLOG_FILLER)
    result= LSN_IMPOSSIBLE; /* reached page filler */
  else
    result= scanner.page_addr + scanner.page_offset;
out:
  translog_destroy_scanner(&scanner);
  DBUG_RETURN(result);
}


/**
   @brief returns the LSN of the first record starting in this log

   @retval LSN_ERROR Error
   @retval LSN_IMPOSSIBLE no log or the log is empty
   @retval # LSN of the first record
*/

LSN translog_first_lsn_in_log()
{
  TRANSLOG_ADDRESS addr, horizon= translog_get_horizon();
  TRANSLOG_VALIDATOR_DATA data;
  uint file;
  uint16 chunk_offset;
  uchar *page;
  DBUG_ENTER("translog_first_lsn_in_log");
  DBUG_PRINT("info", ("Horizon: (%lu,0x%lx)", LSN_IN_PARTS(horizon)));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  if (!(file= translog_first_file(horizon, 0)))
  {
    /* log has no records yet */
    DBUG_RETURN(LSN_IMPOSSIBLE);
  }

  addr= MAKE_LSN(file, TRANSLOG_PAGE_SIZE); /* the first page of the file */
  data.addr= &addr;
  {
    TRANSLOG_PAGE_SIZE_BUFF psize_buff;
    if ((page= translog_get_page(&data, psize_buff.buffer, NULL)) == NULL ||
        (chunk_offset= translog_get_first_chunk_offset(page)) == 0)
      DBUG_RETURN(LSN_ERROR);
  }
  addr+= chunk_offset;

  DBUG_RETURN(translog_next_LSN(addr, horizon));
}


/**
   @brief Returns theoretical first LSN if first log is present

   @retval LSN_ERROR Error
   @retval LSN_IMPOSSIBLE no log
   @retval # LSN of the first record
*/

LSN translog_first_theoretical_lsn()
{
  TRANSLOG_ADDRESS addr= translog_get_horizon();
  TRANSLOG_PAGE_SIZE_BUFF psize_buff;
  uchar *page;
  TRANSLOG_VALIDATOR_DATA data;
  DBUG_ENTER("translog_first_theoretical_lsn");
  DBUG_PRINT("info", ("Horizon: (%lu,0x%lx)", LSN_IN_PARTS(addr)));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  if (!translog_is_file(1))
    DBUG_RETURN(LSN_IMPOSSIBLE);
  if (addr == MAKE_LSN(1, TRANSLOG_PAGE_SIZE))
  {
    /* log has no records yet */
    DBUG_RETURN(MAKE_LSN(1, TRANSLOG_PAGE_SIZE +
                         log_descriptor.page_overhead));
  }

  addr= MAKE_LSN(1, TRANSLOG_PAGE_SIZE); /* the first page of the file */
  data.addr= &addr;
  if ((page= translog_get_page(&data, psize_buff.buffer, NULL)) == NULL)
    DBUG_RETURN(LSN_ERROR);

  DBUG_RETURN(MAKE_LSN(1, TRANSLOG_PAGE_SIZE +
                       page_overhead[page[TRANSLOG_PAGE_FLAGS]]));
}


/**
  @brief Checks given low water mark and purge files if it is need

  @param low the last (minimum) address which is need

  @retval 0 OK
  @retval 1 Error
*/

my_bool translog_purge(TRANSLOG_ADDRESS low)
{
  uint32 last_need_file= LSN_FILE_NO(low);
  TRANSLOG_ADDRESS horizon= translog_get_horizon();
  int rc= 0;
  DBUG_ENTER("translog_purge");
  DBUG_PRINT("enter", ("low: (%lu,0x%lx)", LSN_IN_PARTS(low)));
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  pthread_mutex_lock(&log_descriptor.purger_lock);
  if (LSN_FILE_NO(log_descriptor.last_lsn_checked) < last_need_file)
  {
    uint32 i;
    uint32 min_file= translog_first_file(horizon, 1);
    DBUG_ASSERT(min_file != 0); /* log is already started */
    for(i= min_file; i < last_need_file && rc == 0; i++)
    {
      LSN lsn= translog_get_file_max_lsn_stored(i);
      if (lsn == LSN_IMPOSSIBLE)
        break;   /* files are still in writing */
      if (lsn == LSN_ERROR)
      {
        rc= 1;
        break;
      }
      if (cmp_translog_addr(lsn, low) >= 0)
        break;

      DBUG_PRINT("info", ("purge file %lu", (ulong) i));

      /* remove file descriptor from the cache */
      /*
        log_descriptor.min_file can be changed only here during execution
        and the function is serialized, so we can access it without problems
      */
      if (i >= log_descriptor.min_file)
      {
        TRANSLOG_FILE *file;
        rw_wrlock(&log_descriptor.open_files_lock);
        DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
                    log_descriptor.open_files.elements);
        DBUG_ASSERT(log_descriptor.min_file == i);
        file= *((TRANSLOG_FILE **)pop_dynamic(&log_descriptor.open_files));
        DBUG_PRINT("info", ("Files : %d", log_descriptor.open_files.elements));
        DBUG_ASSERT(i == file->number);
        log_descriptor.min_file++;
        DBUG_ASSERT(log_descriptor.max_file - log_descriptor.min_file + 1 ==
                    log_descriptor.open_files.elements);
        rw_unlock(&log_descriptor.open_files_lock);
        translog_close_log_file(file);
      }
      if (log_purge_type == TRANSLOG_PURGE_IMMIDIATE)
      {
        char path[FN_REFLEN], *file_name;
        file_name= translog_filename_by_fileno(i, path);
        rc= test(my_delete(file_name, MYF(MY_WME)));
      }
    }
    if (unlikely(rc == 1))
      log_descriptor.min_need_file= 0; /* impossible value */
    else
      log_descriptor.min_need_file= i;
  }

  pthread_mutex_unlock(&log_descriptor.purger_lock);
  DBUG_RETURN(rc);
}


/**
  @brief Purges files by stored min need file in case of
    "ondemend" purge type

  @note This function do real work only if it is "ondemend" purge type
    and translog_purge() was called at least once and last time without
    errors

  @retval 0 OK
  @retval 1 Error
*/

my_bool translog_purge_at_flush()
{
  uint32 i, min_file;
  int rc= 0;
  DBUG_ENTER("translog_purge_at_flush");
  DBUG_ASSERT(translog_status == TRANSLOG_OK ||
              translog_status == TRANSLOG_READONLY);

  if (unlikely(translog_status == TRANSLOG_READONLY))
  {
    DBUG_PRINT("info", ("The log is read only => exit"));
    DBUG_RETURN(0);
  }

  if (log_purge_type != TRANSLOG_PURGE_ONDEMAND)
  {
    DBUG_PRINT("info", ("It is not \"at_flush\" => exit"));
    DBUG_RETURN(0);
  }

  pthread_mutex_lock(&log_descriptor.purger_lock);

  if (unlikely(log_descriptor.min_need_file == 0))
  {
    DBUG_PRINT("info", ("No info about min need file => exit"));
    pthread_mutex_unlock(&log_descriptor.purger_lock);
    DBUG_RETURN(0);
  }

  min_file= translog_first_file(translog_get_horizon(), 1);
  DBUG_ASSERT(min_file != 0); /* log is already started */
  for(i= min_file; i < log_descriptor.min_need_file && rc == 0; i++)
  {
    char path[FN_REFLEN], *file_name;
    DBUG_PRINT("info", ("purge file %lu\n", (ulong) i));
    file_name= translog_filename_by_fileno(i, path);
    rc= test(my_delete(file_name, MYF(MY_WME)));
  }

  pthread_mutex_unlock(&log_descriptor.purger_lock);
  DBUG_RETURN(rc);
}


/**
  @brief Gets min file number

  @param horizon         the end of the log

  @retval minimum file number
  @retval 0 no files found
*/

uint32 translog_get_first_file(TRANSLOG_ADDRESS horizon)
{
  return translog_first_file(horizon, 0);
}


/**
  @brief Gets min file number which is needed

  @retval minimum file number
  @retval 0 unknown
*/

uint32 translog_get_first_needed_file()
{
  uint32 file_no;
  pthread_mutex_lock(&log_descriptor.purger_lock);
  file_no= log_descriptor.min_need_file;
  pthread_mutex_unlock(&log_descriptor.purger_lock);
  return file_no;
}


/**
  @brief Gets transaction log file size

  @return transaction log file size
*/

uint32 translog_get_file_size()
{
  uint32 res;
  translog_lock();
  res= log_descriptor.log_file_max_size;
  translog_unlock();
  return (res);
}


/**
  @brief Sets transaction log file size

  @return Returns actually set transaction log size
*/

void translog_set_file_size(uint32 size)
{
  struct st_translog_buffer *old_buffer= NULL;
  DBUG_ENTER("translog_set_file_size");
  translog_lock();
  DBUG_PRINT("enter", ("Size: %lu", (ulong) size));
  DBUG_ASSERT(size % TRANSLOG_PAGE_SIZE == 0 &&
              size >= TRANSLOG_MIN_FILE_SIZE);
  log_descriptor.log_file_max_size= size;
  /* if current file longer then finish it*/
  if (LSN_OFFSET(log_descriptor.horizon) >=  log_descriptor.log_file_max_size)
  {
    old_buffer= log_descriptor.bc.buffer;
    translog_buffer_next(&log_descriptor.horizon, &log_descriptor.bc, 1);
    translog_buffer_unlock(old_buffer);
  }
  translog_unlock();
  if (old_buffer)
  {
    translog_buffer_lock(old_buffer);
    translog_buffer_flush(old_buffer);
    translog_buffer_unlock(old_buffer);
  }
  DBUG_VOID_RETURN;
}


/**
  @brief Dump information about file header page.
*/

static void dump_header_page(uchar *buff)
{
  LOGHANDLER_FILE_INFO desc;
  char strbuff[21];
  translog_interpret_file_header(&desc, buff);
  printf("  This can be header page:\n"
         "    Timestamp: %s\n"
         "    Maria log version: %lu\n"
         "    Server version: %lu\n"
         "    Server id %lu\n"
         "    Page size %lu\n",
         llstr(desc.timestamp, strbuff),
         desc.maria_version,
         desc.mysql_version,
         desc.server_id,
         desc.page_size);
  if (desc.page_size != TRANSLOG_PAGE_SIZE)
    printf("      WARNING: page size is not equal compiled in one %lu!!!\n",
           (ulong) TRANSLOG_PAGE_SIZE);
  printf("    File number %lu\n"
         "    Max lsn: (%lu,0x%lx)\n",
         desc.file_number,
         LSN_IN_PARTS(desc.max_lsn));
}

static const char *record_class_string[]=
{
  "LOGRECTYPE_NOT_ALLOWED",
  "LOGRECTYPE_VARIABLE_LENGTH",
  "LOGRECTYPE_PSEUDOFIXEDLENGTH",
  "LOGRECTYPE_FIXEDLENGTH"
};


/**
  @brief dump information about transaction log chunk

  @param buffer          reference to the whole page
  @param ptr             pointer to the chunk

  @reval # reference to the next chunk
  @retval NULL can't interpret data
*/

static uchar *dump_chunk(uchar *buffer, uchar *ptr)
{
  uint length;
  if (*ptr == TRANSLOG_FILLER)
  {
    printf("  Filler till the page end\n");
    for (; ptr < buffer + TRANSLOG_PAGE_SIZE; ptr++)
    {
      if (*ptr != TRANSLOG_FILLER)
      {
        printf("    WARNING: non filler character met before page end "
               "(page + 0x%04x: 0x%02x) (stop interpretation)!!!",
               (uint) (ptr - buffer), (uint) ptr[0]);
        return NULL;
      }
    }
    return ptr;
  }
  if (*ptr == 0 || *ptr == 0xFF)
  {
    printf("    WARNING: chunk can't start from 0x0 "
           "(stop interpretation)!!!\n");
    return NULL;
  }
  switch (ptr[0] & TRANSLOG_CHUNK_TYPE) {
  case TRANSLOG_CHUNK_LSN:
    printf("    LSN chunk type 0 (variable length)\n");
    if (likely((ptr[0] & TRANSLOG_REC_TYPE) != TRANSLOG_CHUNK_0_CONT))
    {
      printf("      Record type %u: %s  record class %s compressed LSNs: %u\n",
             ptr[0] & TRANSLOG_REC_TYPE,
             (log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].name ?
              log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].name :
              "NULL"),
             record_class_string[log_record_type_descriptor[ptr[0] &
                                                            TRANSLOG_REC_TYPE].
                                                            rclass],
             log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].
             compressed_LSN);
      if (log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].rclass !=
          LOGRECTYPE_VARIABLE_LENGTH)
      {
        printf("        WARNING: this record class here can't be used "
               "(stop interpretation)!!!\n");
        break;
      }
    }
    else
      printf("      Continuation of previous chunk 0 header \n");
    printf("      Short transaction id: %u\n", (uint) uint2korr(ptr + 1));
    {
      uchar *hdr_ptr= ptr + 1 + 2; /* chunk type and short trid */
      uint16 chunk_len;
      printf ("      Record length: %lu\n",
              (ulong) translog_variable_record_1group_decode_len(&hdr_ptr));
      chunk_len= uint2korr(hdr_ptr);
      if (chunk_len == 0)
        printf ("      It is 1 group record (chunk length == 0)\n");
      else
      {
        uint16 groups, i;

        printf ("      Chunk length %u\n", (uint) chunk_len);
        groups= uint2korr(hdr_ptr + 2);
        hdr_ptr+= 4;
        printf ("      Number of groups left to the end %u:\n", (uint) groups);
        for(i= 0;
            i < groups && hdr_ptr < buffer + TRANSLOG_PAGE_SIZE;
            i++, hdr_ptr+= LSN_STORE_SIZE + 1)
        {
          TRANSLOG_ADDRESS gpr_addr= lsn_korr(hdr_ptr);
          uint pages= hdr_ptr[LSN_STORE_SIZE];
          printf ("        Group +#%u: (%lu,0x%lx)  pages: %u\n",
                  (uint) i, LSN_IN_PARTS(gpr_addr), pages);
        }
      }
    }
    break;
  case TRANSLOG_CHUNK_FIXED:
    printf("    LSN chunk type 1 (fixed size)\n");
    printf("      Record type %u: %s  record class %s compressed LSNs: %u\n",
           ptr[0] & TRANSLOG_REC_TYPE,
           (log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].name ?
            log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].name :
            "NULL"),
           record_class_string[log_record_type_descriptor[ptr[0] &
                                                          TRANSLOG_REC_TYPE].
                                                          rclass],
           log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].
           compressed_LSN);
    if (log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].rclass !=
        LOGRECTYPE_PSEUDOFIXEDLENGTH &&
        log_record_type_descriptor[ptr[0] & TRANSLOG_REC_TYPE].rclass !=
        LOGRECTYPE_FIXEDLENGTH)
    {
      printf("        WARNING: this record class here can't be used "
             "(stop interpretation)!!!\n");
    }
    printf("      Short transaction id: %u\n", (uint) uint2korr(ptr + 1));
    break;
  case TRANSLOG_CHUNK_NOHDR:
    printf("    No header chunk type 2(till the end of the page)\n");
    if (ptr[0] & TRANSLOG_REC_TYPE)
    {
      printf("      WARNING: chunk header content record type: 0x%02x "
             "(dtop interpretation)!!!",
             (uint) ptr[0]);
      return NULL;
    }
    break;
  case TRANSLOG_CHUNK_LNGTH:
    printf("    Chunk with length type 3\n");
    if (ptr[0] & TRANSLOG_REC_TYPE)
    {
      printf("      WARNING: chunk header content record type: 0x%02x "
             "(dtop interpretation)!!!",
             (uint) ptr[0]);
      return NULL;
    }
    break;
  }
  {
    intptr offset= ptr - buffer;
    DBUG_ASSERT(offset >= 0 && offset <= UINT_MAX16);
    length= translog_get_total_chunk_length(buffer, (uint16)offset);
  }
  printf("      Length %u\n", length);
  ptr+= length;
  return ptr;
}


/**
  @brief Dump information about page with data.
*/

static void dump_datapage(uchar *buffer, File handler)
{
  uchar *ptr;
  ulong offset;
  uint32 page, file;
  uint header_len;
  printf("  Page: %ld  File number: %ld\n",
         (ulong) (page= uint3korr(buffer)),
         (ulong) (file= uint3korr(buffer + 3)));
  if (page == 0)
    printf("    WARNING: page == 0!!!\n");
  if (file == 0)
    printf("    WARNING: file == 0!!!\n");
  offset= page * TRANSLOG_PAGE_SIZE;
  printf("  Flags (0x%x):\n", (uint) buffer[TRANSLOG_PAGE_FLAGS]);
  if (buffer[TRANSLOG_PAGE_FLAGS])
  {
    if (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_PAGE_CRC)
      printf("    Page CRC\n");
    if (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION)
      printf("    Sector protection\n");
    if (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_RECORD_CRC)
      printf("    Record CRC (WARNING: not yet implemented!!!)\n");
    if (buffer[TRANSLOG_PAGE_FLAGS] & ~(TRANSLOG_PAGE_CRC |
                                        TRANSLOG_SECTOR_PROTECTION |
                                        TRANSLOG_RECORD_CRC))
    {
      printf("    WARNING: unknown flags (stop interpretation)!!!\n");
      return;
    }
  }
  else
    printf("    No flags\n");
  printf("  Page header length: %u\n",
         (header_len= page_overhead[buffer[TRANSLOG_PAGE_FLAGS]]));
  if (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_RECORD_CRC)
  {
    uint32 crc= uint4korr(buffer + TRANSLOG_PAGE_FLAGS + 1);
    uint32 ccrc;
    printf ("  Page CRC 0x%04lx\n", (ulong) crc);
    ccrc= translog_crc(buffer + header_len, TRANSLOG_PAGE_SIZE - header_len);
    if (crc != ccrc)
      printf("    WARNING: calculated CRC: 0x%04lx!!!\n", (ulong) ccrc);
  }
  if (buffer[TRANSLOG_PAGE_FLAGS] & TRANSLOG_SECTOR_PROTECTION)
  {
    TRANSLOG_FILE tfile;
    {
      uchar *table= buffer + header_len -
        TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE;
      uint i;
      printf("    Sector protection current value: 0x%02x\n", (uint) table[0]);
      for (i= 1; i < TRANSLOG_PAGE_SIZE / DISK_DRIVE_SECTOR_SIZE; i++)
      {
         printf("    Sector protection in sector: 0x%02x  saved value 0x%02x\n",
                (uint)buffer[i * DISK_DRIVE_SECTOR_SIZE],
                (uint)table[i]);
      }
    }
    tfile.number= file;
    tfile.handler.file= handler;
    pagecache_file_init(tfile.handler, NULL, NULL, NULL, NULL, NULL);
    tfile.was_recovered= 0;
    tfile.is_sync= 1;
    if (translog_check_sector_protection(buffer, &tfile))
      printf("    WARNING: sector protection found problems!!!\n");
  }
  ptr= buffer + header_len;
  while (ptr && ptr < buffer + TRANSLOG_PAGE_SIZE)
  {
    printf("  Chunk (%lu,0x%lx):\n",
           (ulong)file, (ulong) offset + (ptr - buffer));
    ptr= dump_chunk(buffer, ptr);
  }
}


/**
  @brief Dump information about page.
*/

void dump_page(uchar *buffer, File handler)
{
  if (strncmp((char*)maria_trans_file_magic, (char*)buffer,
              sizeof(maria_trans_file_magic)) == 0)
  {
    dump_header_page(buffer);
  }
  dump_datapage(buffer, handler);
}


/**
   Write debug information to log if we EXTRA_DEBUG is enabled
*/

my_bool translog_log_debug_info(TRN *trn __attribute__((unused)),
                                enum translog_debug_info_type type
                                __attribute__((unused)),
                                uchar *info __attribute__((unused)),
                                size_t length __attribute__((unused)))
{
#ifdef EXTRA_DEBUG
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
  uchar debug_type;
  LSN lsn;

  if (!trn)
  {
    /*
      We can't log the current transaction because we don't have
      an active transaction. Use a temporary transaction object instead
    */
    trn= &dummy_transaction_object;
  }
  debug_type= (uchar) type;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str= &debug_type;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= 1;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str= info;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= length;
  return translog_write_record(&lsn, LOGREC_DEBUG_INFO,
                               trn, NULL,
                               (translog_size_t) (1+ length),
                               sizeof(log_array)/sizeof(log_array[0]),
                               log_array, NULL, NULL);
#else
  return 0;
#endif
}
