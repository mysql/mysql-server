/*****************************************************************************

Copyright (c) 1997, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/log0recv.h
 Recovery

 Created 9/20/1997 Heikki Tuuri
 *******************************************************/

#ifndef log0recv_h
#define log0recv_h

#include "buf0types.h"
#include "dict0types.h"
#include "hash0hash.h"
#include "log0sys.h"
#include "mtr0types.h"

/* OS_FILE_LOG_BLOCK_SIZE */
#include "os0file.h"

#include "ut0byte.h"
#include "ut0new.h"

#include <list>
#include <set>
#include <unordered_map>

class MetadataRecover;
class PersistentTableMetadata;

#ifdef UNIV_HOTBACKUP

struct recv_addr_t;

/** list of tablespaces, that experienced an inplace DDL during a backup op */
extern std::list<std::pair<space_id_t, lsn_t>> index_load_list;
/** the last redo log flush len as seen by MEB */
extern volatile lsn_t backup_redo_log_flushed_lsn;
/** true when the redo log is being backed up */
extern bool recv_is_making_a_backup;

/** Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned.
@param[in]      buf                     buffer containing log data
@param[in]      buf_len                 data length in that buffer
@param[in,out]  scanned_lsn             lsn of buffer start, we return scanned
lsn
@param[in,out]  scanned_checkpoint_no   4 lowest bytes of the highest scanned
@param[out]     block_no        highest block no in scanned buffer.
checkpoint number so far
@param[out]     n_bytes_scanned         how much we were able to scan, smaller
than buf_len if log data ended here
@param[out]     has_encrypted_log       set true, if buffer contains encrypted
redo log, set false otherwise */
void meb_scan_log_seg(byte *buf, size_t buf_len, lsn_t *scanned_lsn,
                      uint32_t *scanned_checkpoint_no, uint32_t *block_no,
                      size_t *n_bytes_scanned, bool *has_encrypted_log);

/** Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.


@param[in,out]  block           buffer block */
void recv_recover_page_func(buf_block_t *block);

/** Wrapper for recv_recover_page_func().
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.
@param jri in: true if just read in (the i/o handler calls this for
a freshly read page)
@param block in,out: the buffer block
*/
static inline void recv_recover_page(bool jri [[maybe_unused]],
                                     buf_block_t *block) {
  recv_recover_page_func(block);
}

/** Applies log records in the hash table to a backup. */
void meb_apply_log_recs(void);

/** Applies log records in the hash table to a backup using a callback
functions.
@param[in]      apply_log_record_function  function for apply
@param[in]      wait_till_done_function    function for wait */
void meb_apply_log_recs_via_callback(
    void (*apply_log_record_function)(recv_addr_t *),
    void (*wait_till_done_function)());

/** Applies a log record in the hash table to a backup.
@param[in]      recv_addr       chain of log records
@param[in,out]  block           buffer block to apply the records to */
void meb_apply_log_record(recv_addr_t *recv_addr, buf_block_t *block);

/** Process a file name passed as an input
@param[in]      name            absolute path of tablespace file
@param[in]      space_id        the tablespace ID
@retval         true            if able to process file successfully.
@retval         false           if unable to process the file */
void meb_fil_name_process(const char *name, space_id_t space_id);

/** Scans log from a buffer and stores new log data to the parsing buffer.
Parses and hashes the log records if new data found.  Unless
UNIV_HOTBACKUP is defined, this function will apply log records
automatically when the hash table becomes full.
@param[in]      available_memory        we let the hash table of recs
to grow to this size, at the maximum
@param[in]      buf                     buffer containing a log
segment or garbage
@param[in]      len                     buffer length
@param[in]      start_lsn               buffer start lsn
@param[out]     group_scanned_lsn       scanning succeeded up to this lsn
@param[out]     err  error code as returned by recv_init_crash_recovery().
@retval	true  if limit_lsn has been reached, or not able to scan any
more in this log group
@retval false   otherwise */
bool meb_scan_log_recs(size_t available_memory, const byte *buf, size_t len,
                       lsn_t start_lsn, lsn_t *group_scanned_lsn, dberr_t &err);

/** Check the 4-byte checksum to the trailer checksum field of a log
block.
@param[in]      block   pointer to a log block
@return whether the checksum matches */
bool log_block_checksum_is_ok(const byte *block);
#else /* UNIV_HOTBACKUP */

/** Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.


@param[in]      just_read_in    true if the IO handler calls this for a freshly
                                read page
@param[in,out]  block           buffer block */
void recv_recover_page_func(bool just_read_in, buf_block_t *block);

/** Wrapper for recv_recover_page_func().
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.
@param jri in: true if just read in (the i/o handler calls this for
a freshly read page)
@param[in,out]  block   buffer block */
static inline void recv_recover_page(bool jri, buf_block_t *block) {
  recv_recover_page_func(jri, block);
}

#endif /* UNIV_HOTBACKUP */

/** Frees the recovery system. */
void recv_sys_free();

/** Reset the state of the recovery system variables. */
void recv_sys_var_init();

#ifdef UNIV_HOTBACKUP
/** Get the number of bytes used by all the heaps
@return number of bytes used */
size_t meb_heap_used();
#endif /* UNIV_HOTBACKUP */

/** Returns true if recovery is currently running.
@return recv_recovery_on */
[[nodiscard]] static inline bool recv_recovery_is_on();

/** Returns true if the page is brand new (the next log record is init_file_page
or no records to apply).
@param[in]      block           buffer block
@return true if brand new */
bool recv_page_is_brand_new(buf_block_t *block);

/** Start recovering from a redo log checkpoint.
@see recv_recovery_from_checkpoint_finish
@param[in,out]  log        redo log
@param[in]      flush_lsn  lsn stored at offset FIL_PAGE_FILE_FLUSH_LSN
                           in the system tablespace header
@return error code or DB_SUCCESS */
[[nodiscard]] dberr_t recv_recovery_from_checkpoint_start(log_t &log,
                                                          lsn_t flush_lsn);

/** Complete the recovery from the latest checkpoint.
@param[in]      aborting        true if the server has to abort due to an error
@return recovered persistent metadata or nullptr if aborting*/
[[nodiscard]] MetadataRecover *recv_recovery_from_checkpoint_finish(
    bool aborting);

/** Creates the recovery system. */
void recv_sys_create();

/** Release recovery system mutexes. */
void recv_sys_close();

/** Inits the recovery system for a recovery operation. */
void recv_sys_init();

/** Calculates the new value for lsn when more data is added to the log.
@param[in]      lsn             Old LSN
@param[in]      len             This many bytes of data is added, log block
                                headers not included
@return LSN after data addition */
lsn_t recv_calc_lsn_on_data_add(lsn_t lsn, os_offset_t len);

/** Empties the hash table of stored log records, applying them to appropriate
pages.
@param[in,out]  log             redo log
@param[in]      allow_ibuf      if true, ibuf operations are allowed during
                                the application; if false, no ibuf operations
                                are allowed, and after the application all
                                file pages are flushed to disk and invalidated
                                in buffer pool: this alternative means that
                                no new log records can be generated during
                                the application; the caller must in this case
                                own the log mutex */
dberr_t recv_apply_hashed_log_recs(log_t &log, bool allow_ibuf);

#if defined(UNIV_DEBUG) || defined(UNIV_HOTBACKUP)
/** Return string name of the redo log record type.
@param[in]      type    record log record enum
@return string name of record log record */
const char *get_mlog_string(mlog_id_t type);
#endif /* UNIV_DEBUG || UNIV_HOTBACKUP */

/** Block of log record data */
struct recv_data_t {
  /** pointer to the next block or NULL.  The log record data
  is stored physically immediately after this struct, max amount
  RECV_DATA_BLOCK_SIZE bytes of it */

  recv_data_t *next;
};

/** Stored log record struct */
struct recv_t {
  using Node = UT_LIST_NODE_T(recv_t);

  /** Log record type */
  mlog_id_t type;

  /** Log record body length in bytes */
  ulint len;

  /** Chain of blocks containing the log record body */
  recv_data_t *data;

  /** Start lsn of the log segment written by the mtr which generated
  this log record: NOTE that this is not necessarily the start lsn of
  this log record */
  lsn_t start_lsn;

  /** End lsn of the log segment written by the mtr which generated
  this log record: NOTE that this is not necessarily the end LSN of
  this log record */
  lsn_t end_lsn;

  /** List node, list anchored in recv_addr_t */
  Node rec_list;
};

/** States of recv_addr_t */
enum recv_addr_state {

  /** not yet processed */
  RECV_NOT_PROCESSED,

  /** page is being read */
  RECV_BEING_READ,

  /** log records are being applied on the page */
  RECV_BEING_PROCESSED,

  /** log records have been applied on the page */
  RECV_PROCESSED,

  /** log records have been discarded because the tablespace
  does not exist */
  RECV_DISCARDED
};

/** Hashed page file address struct */
struct recv_addr_t {
  using List = UT_LIST_BASE_NODE_T(recv_t, rec_list);

  /** recovery state of the page */
  recv_addr_state state;

  /** Space ID */
  space_id_t space;

  /** Page number */
  page_no_t page_no;

  /** List of log records for this page */
  List rec_list;
};

// Forward declaration
namespace dblwr {
namespace recv {
class DBLWR;
}
}  // namespace dblwr

/** Class to parse persistent dynamic metadata redo log, store and
merge them and apply them to in-memory table objects finally */
class MetadataRecover {
  using PersistentTables = std::map<
      table_id_t, PersistentTableMetadata *, std::less<table_id_t>,
      ut::allocator<std::pair<const table_id_t, PersistentTableMetadata *>>>;

 public:
  /** Default constructor */
  MetadataRecover() UNIV_NOTHROW = default;

  /** Destructor */
  ~MetadataRecover();

  /** Parse a dynamic metadata redo log of a table and store
  the metadata locally
  @param[in]    id      table id
  @param[in]    version table dynamic metadata version
  @param[in]    ptr     redo log start
  @param[in]    end     end of redo log
  @retval ptr to next redo log record, nullptr if this log record
  was truncated */
  byte *parseMetadataLog(table_id_t id, uint64_t version, byte *ptr, byte *end);

  /** Apply the collected persistent dynamic metadata to in-memory
  table objects */
  void apply();

  /** Store the collected persistent dynamic metadata to
  mysql.innodb_dynamic_metadata */
  void store();

  /** If there is any metadata to be applied
  @return       true if any metadata to be applied, otherwise false */
  bool empty() const { return m_tables.empty(); }

 private:
  /** Get the dynamic metadata of a specified table,
  create a new one if not exist
  @param[in]    id      table id
  @return the metadata of the specified table */
  PersistentTableMetadata *getMetadata(table_id_t id);

 private:
  /** Map used to store and merge persistent dynamic metadata */
  PersistentTables m_tables;
};

/** Recovery system data structure */
struct recv_sys_t {
  using Pages =
      std::unordered_map<page_no_t, recv_addr_t *, std::hash<page_no_t>,
                         std::equal_to<page_no_t>>;

  /** Every space has its own heap and pages that belong to it. */
  struct Space {
    /** Constructor
    @param[in,out]      heap    Heap to use for the log records. */
    explicit Space(mem_heap_t *heap) : m_heap(heap), m_pages() {}

    /** Default constructor */
    Space() : m_heap(), m_pages() {}

    /** Memory heap of log records and file addresses */
    mem_heap_t *m_heap;

    /** Pages that need to be recovered */
    Pages m_pages;
  };

  using Missing_Ids = std::set<space_id_t>;

  using Spaces = std::unordered_map<space_id_t, Space, std::hash<space_id_t>,
                                    std::equal_to<space_id_t>>;

  /* Recovery encryption information */
  struct Encryption_Key {
    /** Tablespace ID */
    space_id_t space_id;

    /** LSN of REDO log encryption entry */
    lsn_t lsn;

    /** Encryption key */
    byte *ptr;

    /** Encryption IV */
    byte *iv;
  };

  using Encryption_Keys = std::vector<Encryption_Key>;

  /** Mini transaction log record. */
  struct Mlog_record {
    /* Space ID */
    space_id_t space_id;
    /* Page number */
    page_no_t page_no;
    /* Log type */
    mlog_id_t type;
    /* Log body */
    const byte *body;
    /* Record size */
    size_t size;
  };

  using Mlog_records = std::vector<Mlog_record, ut::allocator<Mlog_record>>;

  /** While scanning logs for multi-record mini-transaction (mtr), we have two
  passes. In first pass, we check if all the logs of the mtr is present in
  current recovery buffer or not. If yes, then in second pass we go through the
  logs again the add to hash table for apply. To avoid parsing multiple times,
  we save the parsed records in first pass and reuse them in second pass.

  Parsing of redo log takes significant amount of time and this optimization of
  avoiding second parse gave about 1.8x speed up on recovery scan time of 1G of
  redo log from sysbench rw test.

  There is currently no limit for maximum number of logs in an mtr. Practically,
  from sysbench rw test recovery with 1G of redo log to recover from the record
  count were spread from 3 - 1235 with majority between 600 - 700. So, it is
  likely by saving 1k records we could avoid most of the re-parsing overhead.
  Considering possible bigger number of records in other load and future changes
  the limit for number of saved records is kept at 8k. The same value from the
  contribution patch. The memory requirement 32 x 8k = 256k seems fine as one
  time overhead for the entire instance.  */
  static constexpr size_t MAX_SAVED_MLOG_RECS = 8 * 1024;

  /** Save mlog record information. Silently returns if cannot save. Works only
  in single threaded recovery scanner.
  @param[in]    rec_num         record number in multi record group
  @param[in]    space_id        space ID for the log record
  @param[in]    page_no         page number for the log record
  @param[in]    type            log record type
  @param[in]    body            pointer to log record body in recovery buffer
  @param[in]    len             length of the log record */
  void save_rec(size_t rec_num, space_id_t space_id, page_no_t page_no,
                mlog_id_t type, const byte *body, size_t len) {
    /* No more space to save log. */
    if (rec_num >= MAX_SAVED_MLOG_RECS) {
      return;
    }

    ut_ad(rec_num < saved_recs.size());

    if (rec_num >= saved_recs.size()) {
      return;
    }

    auto &saved_rec = saved_recs[rec_num];

    saved_rec.space_id = space_id;
    saved_rec.page_no = page_no;
    saved_rec.type = type;
    saved_rec.body = body;
    saved_rec.size = len;
  }

  /** Return saved mlog record information, if there. Works only
  in single threaded recovery scanner.
  @param[in]    rec_num         record number in multi record group
  @param[out]   space_id        space ID for the log record
  @param[out]   page_no         page number for the log record
  @param[out]   type            log record type
  @param[out]   body            pointer to log record body in recovery buffer
  @param[out]   len             length of the log record
  @return true iff saved record data is found. */
  bool get_saved_rec(size_t rec_num, space_id_t &space_id, page_no_t &page_no,
                     mlog_id_t &type, byte *&body, size_t &len) {
    if (rec_num >= MAX_SAVED_MLOG_RECS) {
      return false;
    }

    ut_ad(rec_num < saved_recs.size());

    if (rec_num >= saved_recs.size()) {
      return false;
    }

    auto &saved_rec = saved_recs[rec_num];

    space_id = saved_rec.space_id;
    page_no = saved_rec.page_no;
    type = saved_rec.type;
    body = const_cast<byte *>(saved_rec.body);
    len = saved_rec.size;

    return true;
  }

#ifndef UNIV_HOTBACKUP

  /*!< mutex protecting the fields apply_log_recs, n_addrs, and the
  state field in each recv_addr struct */
  ib_mutex_t mutex;

  /** mutex coordinating flushing between recv_writer_thread and
  the recovery thread. */
  ib_mutex_t writer_mutex;

  /** event to activate page cleaner threads */
  os_event_t flush_start;

  /** event to signal that the page cleaner has finished the request */
  os_event_t flush_end;

  /** type of the flush request. BUF_FLUSH_LRU: flush end of LRU,
  keeping free blocks.  BUF_FLUSH_LIST: flush all of blocks. */
  buf_flush_t flush_type;

#else  /* !UNIV_HOTBACKUP */
  bool apply_file_operations;
#endif /* !UNIV_HOTBACKUP */

  /** This is true when log rec application to pages is allowed;
  this flag tells the i/o-handler if it should do log record
  application */
  bool apply_log_recs;

  /** This is true when a log rec application batch is running */
  bool apply_batch_on;

  /** Possible incomplete last recovered log block */
  byte *last_block;

  /** Buffer for parsing log records */
  byte *buf;

  /** Size of the parsing buffer */
  size_t buf_len;

  /** Amount of data in buf */
  ulint len;

  /** This is the lsn from which we were able to start parsing
  log records and adding them to the hash table; zero if a suitable
  start point not found yet */
  lsn_t parse_start_lsn;

  /** Checkpoint lsn that was used during recovery (read from file). */
  lsn_t checkpoint_lsn;

  /** Number of data bytes to ignore until we reach checkpoint_lsn. */
  ulint bytes_to_ignore_before_checkpoint;

  /** The log data has been scanned up to this lsn */
  lsn_t scanned_lsn;

  /** The log data has been scanned up to this epoch_no */
  uint32_t scanned_epoch_no;

  /** Start offset of non-parsed log records in buf */
  ulint recovered_offset;

  /** The log records have been parsed up to this lsn */
  lsn_t recovered_lsn;

  /** The previous value of recovered_lsn - before we parsed the last mtr.
  It is equal to recovered_lsn before we parsed any mtr. This is used to
  find moments in which recovered_lsn moves to the next block in which case
  we should update the last_block_first_rec_group (described below). */
  lsn_t previous_recovered_lsn;

  /** Tracks what should be the proper value of first_rec_group field in the
  header of the block to which recovered_lsn belongs. It might be also zero,
  in which case it means we do not know. */
  uint32_t last_block_first_rec_group;

  /** Set when finding a corrupt log block or record, or there
  is a log parsing buffer overflow */
  bool found_corrupt_log;

  /** Set when an inconsistency with the file system contents
  is detected during log scan or apply */
  bool found_corrupt_fs;

  /** Data directory has been recognized as cloned data directory. */
  bool is_cloned_db;

  /** Data directory has been recognized as data directory from MEB. */
  bool is_meb_db;

  /** Doublewrite buffer state before MEB recovery starts. We restore to this
  state after MEB recovery completes and disable the doublewrite buffer during
  MEB recovery. */
  bool dblwr_state;

  /** Hash table of pages, indexed by SpaceID. */
  Spaces *spaces;

  /** Number of not processed hashed file addresses in the hash table */
  ulint n_addrs;

  /** Doublewrite buffer pages, destroyed after recovery completes */
  dblwr::recv::DBLWR *dblwr;

  /** We store and merge all table persistent data here during
  scanning redo logs */
  MetadataRecover *metadata_recover;

  /** Encryption Key information per tablespace ID */
  Encryption_Keys *keys;

  /** Tablespace IDs that were ignored during redo log apply. */
  Missing_Ids missing_ids;

  /** Tablespace IDs that were explicitly deleted. */
  Missing_Ids deleted;

  /* Saved log records to avoid second round parsing log. */
  Mlog_records saved_recs;
};

/** The recovery system */
extern recv_sys_t *recv_sys;

/** true when applying redo log records during crash recovery; false
otherwise.  Note that this is false while a background thread is
rolling back incomplete transactions. */
extern volatile bool recv_recovery_on;

/** If the following is true, the buffer pool file pages must be invalidated
after recovery and no ibuf operations are allowed; this becomes true if
the log record hash table becomes too full, and log records must be merged
to file pages already before the recovery is finished: in this case no
ibuf operations are allowed, as they could modify the pages read in the
buffer pool before the pages have been recovered to the up-to-date state.

true means that recovery is running and no operations on the log files
are allowed yet: the variable name is misleading. */
extern bool recv_no_ibuf_operations;

/** true when recv_init_crash_recovery() has been called. */
extern bool recv_needed_recovery;

/** true if buf_page_is_corrupted() should check if the log sequence
number (FIL_PAGE_LSN) is in the future.  Initially false, and set by
recv_recovery_from_checkpoint_start(). */
extern bool recv_lsn_checks_on;

/** Size of the parsing buffer; it must accommodate RECV_SCAN_SIZE many
times! */
constexpr uint32_t RECV_PARSING_BUF_SIZE = 2 * 1024 * 1024;

/** Size of block reads when the log groups are scanned forward to do a
roll-forward */
#define RECV_SCAN_SIZE (4 * UNIV_PAGE_SIZE)

/** This many frames must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free frames to read in pages when we start applying the
log records to the database. */
extern ulint recv_n_pool_free_frames;

/** A list of tablespaces for which (un)encryption process was not
completed before crash. */
extern std::list<space_id_t> recv_encr_ts_list;

#include "log0recv.ic"

#endif
