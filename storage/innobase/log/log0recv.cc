/*****************************************************************************

Copyright (c) 1997, 2023, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

/** @file log/log0recv.cc
 Recovery

 Created 9/20/1997 Heikki Tuuri
 *******************************************************/

#include "ha_prototypes.h"

#include <my_aes.h>
#include <sys/types.h>

#include <array>
#include <iomanip>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "arch0arch.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "clone0api.h"
#include "dict0dd.h"
#include "fil0fil.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0chkp.h"       /* log_next_checkpoint_header */
#include "log0encryption.h" /* log_encryption_read */
#include "log0files_io.h"
#include "log0pre_8_0_30.h"
#include "log0recv.h"
#include "log0test.h"
#include "mem0mem.h"
#include "mtr0log.h"
#include "mtr0mtr.h"
#include "os0thread-create.h"
#include "page0cur.h"
#include "page0zip.h"
#include "trx0rec.h"
#include "trx0undo.h"
#include "ut0new.h"

#include "my_dbug.h"

#ifndef UNIV_HOTBACKUP
#include "buf0rea.h"
#include "ddl0ddl.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#else /* !UNIV_HOTBACKUP */
#include "../meb/mutex.h"
#endif /* !UNIV_HOTBACKUP */

std::list<space_id_t> recv_encr_ts_list;

/** Log records are stored in the hash table in chunks at most of this size;
this must be less than UNIV_PAGE_SIZE as it is stored in the buffer pool */
#define RECV_DATA_BLOCK_SIZE (MEM_MAX_ALLOC_IN_BUF - sizeof(recv_data_t))

/** Read-ahead area in applying log records to file pages */
static const size_t RECV_READ_AHEAD_AREA = 32;

/** The recovery system */
recv_sys_t *recv_sys = nullptr;

/** true when applying redo log records during crash recovery; false
otherwise.  Note that this is false while a background thread is
rolling back incomplete transactions. */
volatile bool recv_recovery_on;

#ifdef UNIV_HOTBACKUP
std::list<std::pair<space_id_t, lsn_t>> index_load_list;
volatile lsn_t backup_redo_log_flushed_lsn;

extern bool meb_is_space_loaded(const space_id_t space_id);

/* Re-define mutex macros to use the Mutex class defined by the MEB
source. MEB calls the routines in "fil0fil.cc" in parallel and,
therefore, the mutex protecting the critical sections of the tablespace
memory cache must be included also in the MEB compilation of this
module. (For other modules the mutex macros are defined as no ops in the
MEB compilation in "meb/src/include/bh_univ.i".) */

#undef mutex_enter
#undef mutex_exit
#undef mutex_own
#undef mutex_validate

#define mutex_enter(M) recv_mutex.lock()
#define mutex_exit(M) recv_mutex.unlock()
#define mutex_own(M) 1
#define mutex_validate(M) 1

/* Re-define the mutex macros for the mutex protecting the critical
sections of the log subsystem using an object of the meb::Mutex class. */

meb::Mutex recv_mutex;
extern meb::Mutex log_mutex;
meb::Mutex apply_log_mutex;

#undef log_mutex_enter
#undef log_mutex_exit
#define log_mutex_enter() log_mutex.lock()
#define log_mutex_exit() log_mutex.unlock()

/** Print important values from a page header.
@param[in]      page    page */
void meb_print_page_header(const page_t *page) {
  ib::trace_1() << "space_id " << mach_read_from_4(page + FIL_PAGE_SPACE_ID)
                << " page_nr " << mach_read_from_4(page + FIL_PAGE_OFFSET)
                << " lsn " << mach_read_from_8(page + FIL_PAGE_LSN) << " type "
                << mach_read_from_2(page + FIL_PAGE_TYPE);
}
#endif /* UNIV_HOTBACKUP */

//#ifndef UNIV_HOTBACKUP
PSI_memory_key mem_log_recv_page_hash_key;
PSI_memory_key mem_log_recv_space_hash_key;
//#endif /* !UNIV_HOTBACKUP */

/** true when recv_init_crash_recovery() has been called. */
bool recv_needed_recovery;

/** true if buf_page_is_corrupted() should check if the log sequence
number (FIL_PAGE_LSN) is in the future.  Initially false, and set by
recv_recovery_from_checkpoint_start(). */
bool recv_lsn_checks_on;

/** If the following is true, the buffer pool file pages must be invalidated
after recovery and no ibuf operations are allowed; this becomes true if
the log record hash table becomes too full, and log records must be merged
to file pages already before the recovery is finished: in this case no
ibuf operations are allowed, as they could modify the pages read in the
buffer pool before the pages have been recovered to the up-to-date state.

true means that recovery is running and no operations on the log files
are allowed yet: the variable name is misleading. */
bool recv_no_ibuf_operations;

/** true When the redo log is being backed up */
bool recv_is_making_a_backup = false;

/** true when recovering from a backed up redo log file */
bool recv_is_from_backup = false;

/** The following counter is used to decide when to print info on
log scan */
static ulint recv_scan_print_counter;

/** The type of the previous parsed redo log record */
static mlog_id_t recv_previous_parsed_rec_type;

/** The offset of the previous parsed redo log record */
static ulint recv_previous_parsed_rec_offset;

/** The 'multi' flag of the previous parsed redo log record */
static ulint recv_previous_parsed_rec_is_multi;

/** This many frames must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free frames to read in pages when we start applying the
log records to the database.
This is the default value. If the actual size of the buffer pool is
larger than 10 MB we'll set this value to 512. */
ulint recv_n_pool_free_frames;

/** The maximum lsn we see for a page during the recovery process. If this
is bigger than the lsn we are able to scan up to, that is an indication that
the recovery failed and the database may be corrupt. */
static lsn_t recv_max_page_lsn;

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t recv_writer_thread_key;
#endif /* UNIV_PFS_THREAD */

static bool recv_writer_is_active() {
  return srv_thread_is_active(srv_threads.m_recv_writer);
}

#endif /* !UNIV_HOTBACKUP */

/* prototypes */

#ifndef UNIV_HOTBACKUP

/** Reads a specified log segment to a buffer.
@param[in,out]  log             redo log
@param[in,out]  buf             buffer where to read
@param[in]      start_lsn       read area start
@param[in]      end_lsn         read area end
@return lsn up to which data was available on disk (ideally end_lsn) */
static lsn_t recv_read_log_seg(log_t &log, byte *buf, lsn_t start_lsn,
                               lsn_t end_lsn);

/** Initialize crash recovery environment. Can be called iff
recv_needed_recovery == false. */
static void recv_init_crash_recovery();
#endif /* !UNIV_HOTBACKUP */

/** Calculates the new value for lsn when more data is added to the log.
@param[in]      lsn             Old LSN
@param[in]      len             This many bytes of data is added, log block
                                headers not included
@return LSN after data addition */
lsn_t recv_calc_lsn_on_data_add(lsn_t lsn, os_offset_t len) {
  os_offset_t frag_len;
  os_offset_t lsn_len;

  frag_len = (lsn % OS_FILE_LOG_BLOCK_SIZE) - LOG_BLOCK_HDR_SIZE;

  ut_ad(frag_len <
        OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE - LOG_BLOCK_TRL_SIZE);

  lsn_len = len;

  lsn_len +=
      (lsn_len + frag_len) /
      (OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE - LOG_BLOCK_TRL_SIZE) *
      (LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE);

  return lsn + lsn_len;
}

/** Destructor */
MetadataRecover::~MetadataRecover() {
  for (auto &table : m_tables) {
    ut::delete_(table.second);
  }
}

/** Get the dynamic metadata of a specified table, create a new one
if not exist
@param[in]      id      table id
@return the metadata of the specified table */
PersistentTableMetadata *MetadataRecover::getMetadata(table_id_t id) {
  PersistentTableMetadata *metadata = nullptr;
  PersistentTables::iterator iter = m_tables.find(id);

  if (iter == m_tables.end()) {
    metadata = ut::new_withkey<PersistentTableMetadata>(
        UT_NEW_THIS_FILE_PSI_KEY, id, 0);

    m_tables.insert(std::make_pair(id, metadata));
  } else {
    metadata = iter->second;
    ut_ad(metadata->get_table_id() == id);
  }

  ut_ad(metadata != nullptr);
  return metadata;
}

/** Parse a dynamic metadata redo log of a table and store
the metadata locally
@param[in]      id      table id
@param[in]      version table dynamic metadata version
@param[in]      ptr     redo log start
@param[in]      end     end of redo log
@retval ptr to next redo log record, nullptr if this log record
was truncated */
byte *MetadataRecover::parseMetadataLog(table_id_t id, uint64_t version,
                                        byte *ptr, byte *end) {
  if (ptr + 2 > end) {
    /* At least we should get type byte and another one byte
    for data, if not, it's an incomplete log */
    return nullptr;
  }

  persistent_type_t type = static_cast<persistent_type_t>(ptr[0]);

  ut_ad(dict_persist->persisters != nullptr);

  Persister *persister = dict_persist->persisters->get(type);
  if (persister == nullptr) {
    recv_sys->found_corrupt_log = true;
    return ptr;
  }

  ptr++;

  PersistentTableMetadata *metadata = getMetadata(id);

  PersistentTableMetadata new_entry{id, version};
  bool corrupt;
  ulint consumed = persister->read(new_entry, ptr, end - ptr, &corrupt);

  if (corrupt) {
    recv_sys->found_corrupt_log = true;
    return ptr + consumed;
  }

  if (consumed == 0) {
    return nullptr;
  }

  persister->aggregate(*metadata, new_entry);
  return ptr + consumed;
}

/** Creates the recovery system. */
void recv_sys_create() {
  if (recv_sys != nullptr) {
    return;
  }

  recv_sys = static_cast<recv_sys_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*recv_sys)));
  ut_a(recv_sys->last_block_first_mtr_boundary == 0);
  mutex_create(LATCH_ID_RECV_SYS, &recv_sys->mutex);
  mutex_create(LATCH_ID_RECV_WRITER, &recv_sys->writer_mutex);

  recv_sys->spaces = nullptr;
}

/** Resize the recovery parsing buffer up to log_buffer_size */
static bool recv_sys_resize_buf() {
  ut_ad(recv_sys->buf_len <= srv_log_buffer_size);

#ifndef UNIV_HOTBACKUP
  /* If the buffer cannot be extended further, return false. */
  if (recv_sys->buf_len == srv_log_buffer_size) {
    ib::error(ER_IB_MSG_723, srv_log_buffer_size);
    return false;
  }
#else  /* !UNIV_HOTBACKUP */
  if ((recv_sys->buf_len >= srv_log_buffer_size) ||
      (recv_sys->len >= srv_log_buffer_size)) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_ERR_LOG_PARSING_BUFFER_OVERFLOW)
        << "Log parsing buffer overflow. Log parse failed. "
        << "Please increase --limit-memory above "
        << srv_log_buffer_size / 1024 / 1024 << " (MB)";
  }
#endif /* !UNIV_HOTBACKUP */

  /* Extend the buffer by double the current size with the resulting
  size not more than srv_log_buffer_size. */
  recv_sys->buf_len = ((recv_sys->buf_len * 2) >= srv_log_buffer_size)
                          ? srv_log_buffer_size
                          : recv_sys->buf_len * 2;

  /* Resize the buffer to the new size. */
  recv_sys->buf = static_cast<byte *>(ut::realloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, recv_sys->buf, recv_sys->buf_len));

  ut_ad(recv_sys->buf != nullptr);

  /* Return error and fail the recovery if not enough memory available */
  if (recv_sys->buf == nullptr) {
    ib::error(ER_IB_MSG_740);
    return false;
  }

  ib::info(ER_IB_MSG_739, recv_sys->buf_len);
  return true;
}

/** Free up recovery data structures. */
static void recv_sys_finish() {
#ifndef UNIV_HOTBACKUP
  recv_sys->dblwr->recovered();
#endif /* !UNIV_HOTBACKUP */

  if (recv_sys->spaces != nullptr) {
    for (auto &space : *recv_sys->spaces) {
      if (space.second.m_heap != nullptr) {
        mem_heap_free(space.second.m_heap);
        space.second.m_heap = nullptr;
      }
    }

    ut::delete_(recv_sys->spaces);
  }

  ut::free(recv_sys->buf);
  ut::delete_(recv_sys->metadata_recover);

  recv_sys->buf = nullptr;
  recv_sys->spaces = nullptr;
  recv_sys->metadata_recover = nullptr;
}

/** Release recovery system mutexes. */
void recv_sys_close() {
  if (recv_sys == nullptr) {
    return;
  }

  recv_sys_finish();

#ifndef UNIV_HOTBACKUP
  if (recv_sys->flush_start != nullptr) {
    os_event_destroy(recv_sys->flush_start);
  }

  if (recv_sys->flush_end != nullptr) {
    os_event_destroy(recv_sys->flush_end);
  }

#endif /* !UNIV_HOTBACKUP */

  ut::delete_(recv_sys->dblwr);

  call_destructor(&recv_sys->deleted);
  call_destructor(&recv_sys->missing_ids);
  call_destructor(&recv_sys->saved_recs);

  mutex_free(&recv_sys->mutex);

#ifndef UNIV_HOTBACKUP
  ut_ad(!recv_writer_is_active());
#endif /* !UNIV_HOTBACKUP */
  mutex_free(&recv_sys->writer_mutex);

  ut::free(recv_sys);
  recv_sys = nullptr;
}

#ifndef UNIV_HOTBACKUP
/** Reset the state of the recovery system variables. */
void recv_sys_var_init() {
  recv_recovery_on = false;
  recv_needed_recovery = false;
  recv_lsn_checks_on = false;
  recv_no_ibuf_operations = false;
  recv_scan_print_counter = 0;
  recv_previous_parsed_rec_type = MLOG_SINGLE_REC_FLAG;
  recv_previous_parsed_rec_offset = 0;
  recv_previous_parsed_rec_is_multi = 0;
  recv_n_pool_free_frames = 256;
  recv_max_page_lsn = 0;
}
#endif /* !UNIV_HOTBACKUP */

/** Get the number of bytes used by all the heaps
@return number of bytes used */
#ifndef UNIV_HOTBACKUP
static size_t recv_heap_used()
#else  /* !UNIV_HOTBACKUP */
size_t meb_heap_used()
#endif /* !UNIV_HOTBACKUP */
{
  size_t size = 0;

  for (auto &space : *recv_sys->spaces) {
    if (space.second.m_heap != nullptr) {
      size += mem_heap_get_size(space.second.m_heap);
    }
  }

  return size;
}

/** Prints diagnostic info of corrupt log.
@param[in]      ptr     pointer to corrupt log record
@param[in]      type    type of the log record (could be garbage)
@param[in]      space   tablespace ID (could be garbage)
@param[in]      page_no page number (could be garbage)
@return whether processing should continue */
static bool recv_report_corrupt_log(const byte *ptr, int type, space_id_t space,
                                    page_no_t page_no) {
  ib::error(ER_IB_MSG_694);

  ib::info(
      ER_IB_MSG_695, type, ulong{space}, ulong{page_no},
      ulonglong{recv_sys->recovered_lsn}, int{recv_previous_parsed_rec_type},
      ulonglong{recv_previous_parsed_rec_is_multi},
      ssize_t{ptr - recv_sys->buf}, ulonglong{recv_previous_parsed_rec_offset});

#ifdef UNIV_HOTBACKUP
  ut_ad(ptr >= recv_sys->buf);
#endif /* UNIV_HOTBACKUP */
  ut_ad(ptr <= recv_sys->buf + recv_sys->len);

  const ulint limit = 100;
  const ulint before = std::min(recv_previous_parsed_rec_offset, limit);
  const ulint after = std::min(recv_sys->len - (ptr - recv_sys->buf), limit);

  ib::info(ER_IB_MSG_696, ulonglong{before}, ulonglong{after});

  ut_print_buf(
      stderr, recv_sys->buf + recv_previous_parsed_rec_offset - before,
      ptr - recv_sys->buf + before + after - recv_previous_parsed_rec_offset);
  putc('\n', stderr);

#ifndef UNIV_HOTBACKUP
  if (srv_force_recovery == 0) {
    ib::info(ER_IB_MSG_697);

    return false;
  }

  ib::warn(ER_IB_MSG_LOG_CORRUPT, FORCE_RECOVERY_MSG);
#endif /* !UNIV_HOTBACKUP */

  return true;
}

void recv_sys_init() {
  if (recv_sys->spaces != nullptr) {
    return;
  }

  mutex_enter(&recv_sys->mutex);

#ifndef UNIV_HOTBACKUP
  if (!srv_read_only_mode) {
    recv_sys->flush_start = os_event_create();
    recv_sys->flush_end = os_event_create();
  }
#else  /* !UNIV_HOTBACKUP */
  recv_is_from_backup = true;
  recv_sys->apply_file_operations = false;
#endif /* !UNIV_HOTBACKUP */

  recv_sys->buf_len =
      std::min<unsigned long>(RECV_PARSING_BUF_SIZE, srv_log_buffer_size);
  recv_sys->buf = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, recv_sys->buf_len));

  recv_sys->len = 0;
  recv_sys->recovered_offset = 0;

  using Spaces = recv_sys_t::Spaces;

  recv_sys->spaces = ut::new_withkey<Spaces>(
      ut::make_psi_memory_key(mem_log_recv_space_hash_key));

  recv_sys->n_addrs = 0;

  recv_sys->apply_log_recs = false;
  recv_sys->apply_batch_on = false;
  recv_sys->is_cloned_db = false;

  recv_sys->found_corrupt_log = false;
  recv_sys->found_corrupt_fs = false;

  recv_max_page_lsn = 0;

  recv_sys->dblwr =
      ut::new_withkey<dblwr::recv::DBLWR>(UT_NEW_THIS_FILE_PSI_KEY);

  new (&recv_sys->deleted) recv_sys_t::Missing_Ids();

  new (&recv_sys->missing_ids) recv_sys_t::Missing_Ids();

  new (&recv_sys->saved_recs) recv_sys_t::Mlog_records();

  recv_sys->saved_recs.resize(recv_sys_t::MAX_SAVED_MLOG_RECS);

  recv_sys->metadata_recover =
      ut::new_withkey<MetadataRecover>(UT_NEW_THIS_FILE_PSI_KEY);

  mutex_exit(&recv_sys->mutex);
}

/** Empties the hash table when it has been fully processed. */
static void recv_sys_empty_hash() {
  ut_ad(mutex_own(&recv_sys->mutex));

  if (recv_sys->n_addrs != 0) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_699, ulonglong{recv_sys->n_addrs});
  }

  for (auto &space : *recv_sys->spaces) {
    if (space.second.m_heap != nullptr) {
      mem_heap_free(space.second.m_heap);
      space.second.m_heap = nullptr;
    }
  }

  ut::delete_(recv_sys->spaces);

  using Spaces = recv_sys_t::Spaces;

  recv_sys->spaces = ut::new_withkey<Spaces>(
      ut::make_psi_memory_key(mem_log_recv_space_hash_key));
}

/** Check the 4-byte checksum to the trailer checksum field of a log
block.
@param[in]      block   pointer to a log block
@return whether the checksum matches */
#ifndef UNIV_HOTBACKUP
static
#endif /* !UNIV_HOTBACKUP */
    bool
    log_block_checksum_is_ok(const byte *block) {
  return !srv_log_checksums ||
         log_block_get_checksum(block) == log_block_calc_checksum(block);
}

/** Get the page map for a tablespace. It will create one if one isn't found.
@param[in]      space_id        Tablespace ID for which page map required.
@param[in]      create          false if lookup only
@return the space data or null if not found */
static recv_sys_t::Space *recv_get_page_map(space_id_t space_id, bool create) {
  auto it = recv_sys->spaces->find(space_id);

  if (it != recv_sys->spaces->end()) {
    return &it->second;

  } else if (create) {
    mem_heap_t *heap;

    heap = mem_heap_create(256, UT_LOCATION_HERE, MEM_HEAP_FOR_RECV_SYS);

    using Space = recv_sys_t::Space;
    using Value = recv_sys_t::Spaces::value_type;

    auto where = recv_sys->spaces->insert(it, Value{space_id, Space(heap)});

    return &where->second;
  }

  return nullptr;
}

/** Gets the list of log records for a <space, page>.
@param[in]      space_id        Tablespace ID
@param[in]      page_no         Page number
@return the redo log entries or nullptr if not found */
static recv_addr_t *recv_get_rec(space_id_t space_id, page_no_t page_no) {
  recv_sys_t::Space *space;

  space = recv_get_page_map(space_id, false);

  if (space != nullptr) {
    auto it = space->m_pages.find(page_no);

    if (it != space->m_pages.end()) {
      return it->second;
    }
  }

  return nullptr;
}

/** Checks if a given log data block could be considered a next valid block,
with regards to the epoch_no it has stored in its header, during the recovery.
@param[in]  log_block_epoch_no  epoch_no of the log data block to check
@param[in]  last_epoch_no       epoch_no of the last data block scanned
@return true iff the provided log block has valid epoch_no */
static bool log_block_epoch_no_is_valid(uint32_t log_block_epoch_no,
                                        uint32_t last_epoch_no) {
  const auto expected_next_epoch_no = last_epoch_no + 1;

  return log_block_epoch_no == last_epoch_no ||
         log_block_epoch_no == expected_next_epoch_no;
}

#ifndef UNIV_HOTBACKUP
/** Store the collected persistent dynamic metadata to
mysql.innodb_dynamic_metadata */
void MetadataRecover::store() {
  ut_ad(dict_sys->dynamic_metadata != nullptr);
  ut_ad(dict_persist->table_buffer != nullptr);

  DDTableBuffer *table_buffer = dict_persist->table_buffer;

  if (empty()) {
    return;
  }

  mutex_enter(&dict_persist->mutex);

  for (auto meta : m_tables) {
    table_id_t table_id = meta.first;
    PersistentTableMetadata *metadata = meta.second;
    byte buffer[REC_MAX_DATA_SIZE];
    size_t size;

    size = dict_persist->persisters->write(*metadata, buffer);

    dberr_t error =
        table_buffer->replace(table_id, metadata->get_version(), buffer, size);
    if (error != DB_SUCCESS) {
      ut_d(ut_error);
    }
  }

  mutex_exit(&dict_persist->mutex);
}

/** recv_writer thread tasked with flushing dirty pages from the buffer
pools. */
static void recv_writer_thread() {
  ut_ad(!srv_read_only_mode);

  /* The code flow is as follows:
  Step 1: In recv_recovery_from_checkpoint_start().
  Step 2: This recv_writer thread is started.
  Step 3: In recv_recovery_from_checkpoint_finish().
  Step 4: Wait for recv_writer thread to complete.
  Step 5: Assert that recv_writer thread is not active anymore.

  It is possible that the thread that is started in step 2,
  becomes active only after step 4 and hence the assert in
  step 5 fails.  So mark this thread active only if necessary. */
  mutex_enter(&recv_sys->writer_mutex);

  if (!recv_recovery_on) {
    mutex_exit(&recv_sys->writer_mutex);
    return;
  }
  mutex_exit(&recv_sys->writer_mutex);

  while (srv_shutdown_state.load() == SRV_SHUTDOWN_NONE) {
    ut_a(srv_shutdown_state_matches([](auto state) {
      return state == SRV_SHUTDOWN_NONE || state == SRV_SHUTDOWN_EXIT_THREADS;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mutex_enter(&recv_sys->writer_mutex);

    if (!recv_recovery_on) {
      mutex_exit(&recv_sys->writer_mutex);
      break;
    }

    if (log_test != nullptr) {
      mutex_exit(&recv_sys->writer_mutex);
      continue;
    }

    /* Flush pages from end of LRU if required */
    os_event_reset(recv_sys->flush_end);
    recv_sys->flush_type = BUF_FLUSH_LRU;
    os_event_set(recv_sys->flush_start);
    os_event_wait(recv_sys->flush_end);

    mutex_exit(&recv_sys->writer_mutex);
  }
}

#endif /* !UNIV_HOTBACKUP */

/** Frees the recovery system. */
void recv_sys_free() {
  mutex_enter(&recv_sys->mutex);

  recv_sys_finish();

#ifndef UNIV_HOTBACKUP
  /* wake page cleaner up to progress */
  if (!srv_read_only_mode) {
    ut_ad(!recv_recovery_on);
    ut_ad(!recv_writer_is_active());
    if (buf_flush_event != nullptr) {
      os_event_reset(buf_flush_event);
    }
    os_event_set(recv_sys->flush_start);
  }
#endif /* !UNIV_HOTBACKUP */

  /* Free encryption data structures. */
  if (recv_sys->keys != nullptr) {
    for (auto &key : *recv_sys->keys) {
      if (key.ptr != nullptr) {
        ut::free(key.ptr);
        key.ptr = nullptr;
      }

      if (key.iv != nullptr) {
        ut::free(key.iv);
        key.iv = nullptr;
      }
    }

    recv_sys->keys->swap(*recv_sys->keys);

    ut::delete_(recv_sys->keys);
    recv_sys->keys = nullptr;
  }

  mutex_exit(&recv_sys->mutex);
}

#ifndef UNIV_HOTBACKUP

/** Determine if a redo log from a version before MySQL 8.0.30 is clean.
@param[in,out]  log             redo log
@return error code
@retval DB_SUCCESS  if the redo log is clean
@retval DB_ERROR    if the redo log is corrupted or dirty */
dberr_t recv_verify_log_is_clean_pre_8_0_30(log_t &log) {
  ut_a(log.m_format < Log_format::CURRENT);

  const size_t n_files = log_files_number_of_existing_files(log.m_files);
  ut_a(n_files >= 2);

  ib::info(ER_IB_MSG_LOG_FORMAT_OLD, ulong{to_int(log.m_format)});

  using namespace log_pre_8_0_30;

  const auto logfile0 = log.m_files.file(0);
  ut_a(logfile0 != log.m_files.end());

  const os_offset_t file_size = logfile0->m_size_in_bytes;

  /* For unknown reasons, InnoDB before 8.0.30 was choosing the latest
  checkpoint by comparing checkpoints' numbers instead of checkpoints'
  LSN values. These should be ordered the same and there shouldn't be
  difference, but to preserve the full compatibility, we prefer to do
  it the same way as it was (after 8.0.30, checkpoints are compared by
  their LSN values because we no longer store checkpoint numbers). */
  byte header_buf[OS_FILE_LOG_BLOCK_SIZE] = {};

  Checkpoint_header chkp_header = {};
  bool checkpoint_found = false;
  for (auto hdr_no : {Log_checkpoint_header_no::HEADER_1,
                      Log_checkpoint_header_no::HEADER_2}) {
    auto file_handle = logfile0->open(Log_file_access_mode::READ_ONLY);
    if (!file_handle.is_open()) {
      return DB_CANNOT_OPEN_FILE;
    }

    const dberr_t err =
        log_checkpoint_header_read(file_handle, hdr_no, header_buf);
    if (err != DB_SUCCESS) {
      return DB_ERROR;
    }

    Checkpoint_header h;
    if (!checkpoint_header_deserialize(header_buf, h)) {
      continue;
    }

    if (!checkpoint_found || h.m_checkpoint_no > chkp_header.m_checkpoint_no) {
      chkp_header = h;
      checkpoint_found = true;
    }
  }

  if (!checkpoint_found) {
    ib::error(ER_IB_MSG_RECOVERY_CHECKPOINT_NOT_FOUND);
    return DB_ERROR;
  }

  if (log_encryption_read(log, *logfile0) != DB_SUCCESS) {
    return DB_ERROR;
  }

  os_offset_t source_offset =
      chkp_header.m_checkpoint_offset % (file_size * n_files);

  const Log_file_id file_id = source_offset / file_size;

  source_offset %= file_size;

  static const char *RTFM_LINK = REFMAN "upgrading.html";

  byte buf[OS_FILE_LOG_BLOCK_SIZE];

  auto file_handle =
      Log_file::open(log.m_files_ctx, file_id, Log_file_access_mode::READ_ONLY,
                     log.m_encryption_metadata);
  ut_a(file_handle.is_open());

  const dberr_t err = file_handle.read(
      ut_uint64_align_down(source_offset, OS_FILE_LOG_BLOCK_SIZE),
      OS_FILE_LOG_BLOCK_SIZE, buf);
  ut_a(err == DB_SUCCESS);

  file_handle.close();

  if (!log_block_checksum_is_ok(buf)) {
    ib::error(ER_IB_MSG_LOG_FORMAT_OLD_AND_LOG_CORRUPTED,
              log.m_creator_name.c_str(), RTFM_LINK);
    return DB_ERROR;
  }

  /* On a shutdown with innodb-fast-shutdown < 2, the redo log will be
  logically empty after the checkpoint LSN. */

  if (log_block_get_data_len(buf) !=
      (source_offset & (OS_FILE_LOG_BLOCK_SIZE - 1))) {
    ib::error(ER_IB_MSG_LOG_FORMAT_OLD_AND_NO_CLEAN_SHUTDOWN,
              log.m_creator_name.c_str(), RTFM_LINK);
    return DB_ERROR;
  }

  /* This lsn might be larger than flushed_lsn found in system tablespace if the
  shutdown wasn't slow. This isn't officially supported scenario, but we can
  handle it if redo was logically empty, by creating new redo with start_lsn
  larger than the checkpoint_lsn found here. */
  recv_sys->checkpoint_lsn = chkp_header.m_checkpoint_lsn;

  return DB_SUCCESS;
}

/** Describes location of a single checkpoint. */
struct Log_checkpoint_location {
  /** File containing checkpoint header and checkpoint lsn. */
  Log_file_id m_checkpoint_file_id{0};

  /** Checkpoint header number. */
  Log_checkpoint_header_no m_checkpoint_header_no{};

  /** Checkpoint LSN. */
  lsn_t m_checkpoint_lsn{0};
};

/** Find the latest checkpoint in the given log file.
@param[in]      file_handle     handle for the opened redo log file
@param[out]     checkpoint      the latest checkpoint found (if any)
@return true iff any checkpoint has been found */
[[nodiscard]] static bool recv_find_max_checkpoint(
    log_t &, Log_file_handle &file_handle,
    Log_checkpoint_location &checkpoint) {
  bool found = false;
  checkpoint = {};

  for (auto checkpoint_header_no : {Log_checkpoint_header_no::HEADER_1,
                                    Log_checkpoint_header_no::HEADER_2}) {
    Log_checkpoint_header checkpoint_header;
    const dberr_t err = log_checkpoint_header_read(
        file_handle, checkpoint_header_no, checkpoint_header);
    if (err != DB_SUCCESS) {
      /* Crash if IO error on read */
      ut_a(err == DB_CORRUPTION);
      continue;
    }

    const lsn_t checkpoint_lsn = checkpoint_header.m_checkpoint_lsn;
    if (checkpoint_lsn == 0) {
      continue;
    }

    DBUG_PRINT("ib_log", ("checkpoint at " LSN_PF, checkpoint_lsn));

    if (!found || checkpoint_lsn > checkpoint.m_checkpoint_lsn) {
      ut_a(checkpoint_lsn >= LOG_START_LSN);
      found = true;
      checkpoint.m_checkpoint_file_id = file_handle.file_id();
      checkpoint.m_checkpoint_header_no = checkpoint_header_no;
      checkpoint.m_checkpoint_lsn = checkpoint_lsn;
    }
  }

  return found;
}

/** Find the latest checkpoint (check all existing redo log files).
@param[in,out]  log             redo log
@param[out]     checkpoint      the latest checkpoint found (if any)
@return true iff any checkpoint has been found */
static bool recv_find_max_checkpoint(log_t &log,
                                     Log_checkpoint_location &checkpoint) {
  bool found = false;
  checkpoint = {};

  log_files_for_each(log.m_files, [&](const Log_file &file) {
    auto file_handle = file.open(Log_file_access_mode::READ_ONLY);
    ut_a(file_handle.is_open());

    Log_checkpoint_location checkpoint_in_file;

    if (!recv_find_max_checkpoint(log, file_handle, checkpoint_in_file)) {
      return;
    }

    if (!file.contains(checkpoint_in_file.m_checkpoint_lsn)) {
      const auto file_path = file_handle.file_path();
      ib::error(ER_IB_MSG_RECOVERY_CHECKPOINT_OUTSIDE_LOG_FILE,
                ulonglong{checkpoint_in_file.m_checkpoint_lsn},
                file_path.c_str(), ulonglong{file.m_start_lsn},
                ulonglong{file.m_end_lsn});
      return;
    }

    if (!found ||
        checkpoint_in_file.m_checkpoint_lsn > checkpoint.m_checkpoint_lsn) {
      found = true;
      checkpoint = checkpoint_in_file;
    }
  });

  return found;
}

/** Reads in pages which have hashed log records, from an area around a given
page number.
@param[in]      page_id         Read the pages around this page number
@return number of pages found */
static ulint recv_read_in_area(const page_id_t &page_id) {
  page_no_t low_limit;

  low_limit = page_id.page_no() - (page_id.page_no() % RECV_READ_AHEAD_AREA);

  ulint n = 0;

  std::array<page_no_t, RECV_READ_AHEAD_AREA> page_nos;

  for (page_no_t page_no = low_limit;
       page_no < low_limit + RECV_READ_AHEAD_AREA; ++page_no) {
    recv_addr_t *recv_addr;

    recv_addr = recv_get_rec(page_id.space(), page_no);

    const page_id_t cur_page_id(page_id.space(), page_no);

    if (recv_addr != nullptr && !buf_page_peek(cur_page_id)) {
      mutex_enter(&recv_sys->mutex);

      if (recv_addr->state == RECV_NOT_PROCESSED) {
        recv_addr->state = RECV_BEING_READ;

        page_nos[n] = page_no;

        ++n;
      }

      mutex_exit(&recv_sys->mutex);
    }
  }

  if (n > 0) {
    /* There are pages that need to be read. Go ahead and read them
    for recovery. */
    buf_read_recv_pages(false, page_id.space(), &page_nos[0], n);
  }

  return n;
}

/** Apply the log records to a page
@param[in,out]  recv_addr       Redo log records to apply */
static void recv_apply_log_rec(recv_addr_t *recv_addr) {
  if (recv_addr->state == RECV_DISCARDED) {
    ut_a(recv_sys->n_addrs > 0);
    --recv_sys->n_addrs;
    return;
  }

  bool found;
  const page_id_t page_id(recv_addr->space, recv_addr->page_no);

  const page_size_t page_size =
      fil_space_get_page_size(recv_addr->space, &found);

  if (!found || recv_sys->missing_ids.find(recv_addr->space) !=
                    recv_sys->missing_ids.end()) {
    /* Tablespace was discarded or dropped after changes were
    made to it. Or, we have ignored redo log for this tablespace
    earlier and somehow it has been found now. We can't apply
    this redo log out of order. */

    recv_addr->state = RECV_PROCESSED;

    ut_a(recv_sys->n_addrs > 0);
    --recv_sys->n_addrs;

    /* If the tablespace has been explicitly deleted, we
    can safely ignore it. */

    if (recv_sys->deleted.find(recv_addr->space) == recv_sys->deleted.end()) {
      recv_sys->missing_ids.insert(recv_addr->space);
    }

  } else if (recv_addr->state == RECV_NOT_PROCESSED) {
    mutex_exit(&recv_sys->mutex);

    if (buf_page_peek(page_id)) {
      mtr_t mtr;

      mtr_start(&mtr);

      buf_block_t *block;

      block =
          buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, &mtr);

      buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

      recv_recover_page(false, block);

      mtr_commit(&mtr);

    } else {
      recv_read_in_area(page_id);
    }

    mutex_enter(&recv_sys->mutex);
  }
}

dberr_t recv_apply_hashed_log_recs(log_t &log, bool allow_ibuf) {
  for (;;) {
    mutex_enter(&recv_sys->mutex);

    if (!recv_sys->apply_batch_on) {
      break;
    }

    mutex_exit(&recv_sys->mutex);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  if (!allow_ibuf) {
    recv_no_ibuf_operations = true;
  }

  recv_sys->apply_log_recs = true;
  recv_sys->apply_batch_on = true;

  auto batch_size = recv_sys->n_addrs;

  ib::info(ER_IB_MSG_707, ulonglong{batch_size});

  static const size_t PCT = 10;

  size_t pct = PCT;
  size_t applied = 0;
  auto unit = batch_size / PCT;

  if (unit <= PCT) {
    pct = 100;
    unit = batch_size;
  }

  auto start_time = std::chrono::steady_clock::now();

  for (const auto &space : *recv_sys->spaces) {
    bool dropped;

    if (space.first == TRX_SYS_SPACE) {
      dropped = false;
    } else {
      dberr_t err = fil_tablespace_open_for_recovery(space.first);
      if (err == DB_SUCCESS) {
        dropped = false;
      } else if (err == DB_CORRUPTION) {
        /* Page couldn't be recovered from doublewrite, we cannot proceed
        with recovery. Skip applying redos and abort the startup. */
        mutex_exit(&recv_sys->mutex);
        return err;
      } else {
        /* Tablespace was dropped. It should not have been scanned unless it
        is an undo space that was under construction. */

        if (fil_tablespace_lookup_for_recovery(space.first)) {
          ut_ad(fsp_is_undo_tablespace(space.first));
        }
        dropped = true;
      }
    }

    for (auto pages : space.second.m_pages) {
      ut_ad(pages.second->space == space.first);

      if (dropped) {
        pages.second->state = RECV_DISCARDED;
      }

      recv_apply_log_rec(pages.second);

      ++applied;

      if (unit == 0 || (applied % unit) == 0) {
        ib::info(ER_IB_MSG_708) << pct << "%";

        pct += PCT;

        start_time = std::chrono::steady_clock::now();

      } else if (std::chrono::steady_clock::now() - start_time >=
                 PRINT_INTERVAL) {
        start_time = std::chrono::steady_clock::now();

        ib::info(ER_IB_MSG_709)
            << std::setprecision(2)
            << ((double)applied * 100) / (double)batch_size << "%";
      }
    }
  }

  /* Wait until all the pages have been processed */

  while (recv_sys->n_addrs != 0) {
    mutex_exit(&recv_sys->mutex);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    mutex_enter(&recv_sys->mutex);
  }

  if (!allow_ibuf) {
    /* Flush all the file pages to disk and invalidate them in
    the buffer pool */
    ut_d(log.disable_redo_writes = true);
    ut_a(recv_sys->flush_end != nullptr);

    mutex_exit(&recv_sys->mutex);

    /* Stop the recv_writer thread from issuing any LRU
    flush batches. */
    mutex_enter(&recv_sys->writer_mutex);

    /* Wait for any currently run batch to end. */
    buf_flush_wait_LRU_batch_end();

    os_event_reset(recv_sys->flush_end);

    recv_sys->flush_type = BUF_FLUSH_LIST;

    os_event_set(recv_sys->flush_start);

    os_event_wait(recv_sys->flush_end);

    buf_pool_invalidate();

    /* Allow batches from recv_writer thread. */
    mutex_exit(&recv_sys->writer_mutex);

    ut_d(log.disable_redo_writes = false);

    mutex_enter(&recv_sys->mutex);

    recv_no_ibuf_operations = false;
  }

  recv_sys->apply_log_recs = false;
  recv_sys->apply_batch_on = false;

  recv_sys_empty_hash();

  mutex_exit(&recv_sys->mutex);

  ib::info(ER_IB_MSG_710);
  return DB_SUCCESS;
}

#else /* !UNIV_HOTBACKUP */
/** Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned.
@param[in]      buf                     buffer containing log data
@param[in]      buf_len                 data length in that buffer
@param[in,out]  scanned_lsn             LSN of buffer start, we return scanned
lsn
@param[in,out]  scanned_epoch_no        the highest scanned epoch number so far
@param[out]     block_no        highest block no in scanned buffer.
@param[out]     n_bytes_scanned         how much we were able to scan, smaller
than buf_len if log data ended here
+@param[out]    has_encrypted_log       set true, if buffer contains encrypted
+redo log, set false otherwise */
void meb_scan_log_seg(byte *buf, size_t buf_len, lsn_t *scanned_lsn,
                      uint32_t *scanned_epoch_no, uint32_t *block_no,
                      size_t *n_bytes_scanned, bool *has_encrypted_log) {
  *n_bytes_scanned = 0;
  *has_encrypted_log = false;

  for (auto log_block = buf; log_block < buf + buf_len;
       log_block += OS_FILE_LOG_BLOCK_SIZE) {
    Log_data_block_header block_header;
    log_data_block_header_deserialize(log_block, block_header);
    uint32_t no = block_header.m_hdr_no;
    bool is_encrypted = log_block_get_encrypt_bit(log_block);

    if (is_encrypted) {
      *has_encrypted_log = true;
      return;
    }

    if (no != log_block_convert_lsn_to_hdr_no(*scanned_lsn) ||
        !log_block_checksum_is_ok(log_block)) {
      ib::trace_2() << "Scanned lsn: " << *scanned_lsn << " header no: " << no
                    << " converted no: "
                    << log_block_convert_lsn_to_hdr_no(*scanned_lsn)
                    << " checksum: " << log_block_checksum_is_ok(log_block)
                    << " block epoch no: " << block_header.m_epoch_no;

      /* Garbage or an incompletely written log block */

      log_block += OS_FILE_LOG_BLOCK_SIZE;
      break;
    }

    if (*scanned_epoch_no > 0 &&
        !log_block_epoch_no_is_valid(block_header.m_epoch_no,
                                     *scanned_epoch_no)) {
      /* Garbage from a log buffer flush which was made
      before the most recent database recovery */

      ib::trace_2() << "Scanned ep no: " << *scanned_epoch_no << " block ep no "
                    << block_header.m_epoch_no;

      break;
    }

    const auto data_len = block_header.m_data_len;

    *scanned_epoch_no = block_header.m_epoch_no;
    *scanned_lsn += data_len;

    *n_bytes_scanned += data_len;

    if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
      /* Log data ends here */

      break;
    }
    *block_no = no;
  }
}

/** Apply a single log record stored in the hash table.
@param[in,out]  recv_addr       a parsed log record
@param[in,out]  block           a buffer pool frame for applying the record */
void meb_apply_log_record(recv_addr_t *recv_addr, buf_block_t *block) {
  bool found;
  const page_id_t page_id(recv_addr->space, recv_addr->page_no);

  const page_size_t &page_size =
      fil_space_get_page_size(recv_addr->space, &found);

  ib::trace_3() << "meb_apply_log_record: recv state " << recv_addr->state
                << " space_id " << recv_addr->space << " page_nr "
                << recv_addr->page_no << " page size " << page_size << " found "
                << found;

  if (!found) {
    recv_addr->state = RECV_DISCARDED;

    mutex_enter(&recv_sys->mutex);

    ut_a(recv_sys->n_addrs);
    --recv_sys->n_addrs;

    mutex_exit(&recv_sys->mutex);

    return;
  }

  mutex_enter(&recv_sys->mutex);

  /* We simulate a page read made by the buffer pool, to
  make sure the recovery apparatus works ok. We must init
  the block. */

  meb_page_init(page_id, page_size, block);

  /* Extend the tablespace's last file if the page_no
  does not fall inside its bounds; we assume the last
  file is auto-extending, and mysqlbackup copied the file
  when it still was smaller */

  fil_space_t *space = fil_space_get(recv_addr->space);

  bool success;

  success = fil_space_extend(space, recv_addr->page_no + 1);

  if (!success) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_711)
        << "Cannot extend tablespace " << recv_addr->space << " to hold "
        << recv_addr->page_no << " pages";
  }

  mutex_exit(&recv_sys->mutex);

  /* Read the page from the tablespace file. */

  dberr_t err;

  if (page_size.is_compressed()) {
    err = fil_io(IORequestRead, true, page_id, page_size, 0,
                 page_size.physical(), block->page.zip.data, nullptr);

    if (err == DB_SUCCESS && !buf_zip_decompress(block, true)) {
      ut_error;
    }
  } else {
    err = fil_io(IORequestRead, true, page_id, page_size, 0,
                 page_size.logical(), block->frame, nullptr);
  }

  if (err != DB_SUCCESS) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_712)
        << "Cannot read from tablespace " << recv_addr->space << " page number "
        << recv_addr->page_no;
  }

  apply_log_mutex.lock();

  /* Apply the log records to this page */
  recv_recover_page(false, block);

  apply_log_mutex.unlock();

  mutex_enter(&recv_sys->mutex);

  /* Write the page back to the tablespace file using the
  fil0fil.cc routines */

  buf_flush_init_for_writing(block, block->frame, buf_block_get_page_zip(block),
                             mach_read_from_8(block->frame + FIL_PAGE_LSN),
                             fsp_is_checksum_disabled(block->page.id.space()),
                             true /* skip_lsn_check */);

  mutex_exit(&recv_sys->mutex);

  if (page_size.is_compressed()) {
    err = fil_io(IORequestWrite, true, page_id, page_size, 0,
                 page_size.physical(), block->page.zip.data, nullptr);
  } else {
    err = fil_io(IORequestWrite, true, page_id, page_size, 0,
                 page_size.logical(), block->frame, nullptr);
  }

  if (err != DB_SUCCESS) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_713)
        << "Cannot write to tablespace " << recv_addr->space << " page number "
        << recv_addr->page_no;
  }
}

/** Apply a single log record stored in the hash table using default block.
@param[in,out]  recv_addr       a parsed log record */
void meb_apply_log_rec_func(recv_addr_t *recv_addr) {
  meb_apply_log_record(recv_addr, back_block1);
}

/** Dummy wait function for meb_apply_log_recs_via_callback(). */
void meb_nowait_func() { return; }

/** Applies log records in the hash table to a backup. */
void meb_apply_log_recs() {
  meb_apply_log_recs_via_callback(meb_apply_log_rec_func, meb_nowait_func);
}

/** Apply all log records in the hash table to a backup using callback
functions. This function employes two callback functions that allow redo
log records to be applied in parallel. The apply_log_record_function
assigns a parsed redo log record for application. The
apply_log_record_function is called repeatedly until all log records in
the hash table are assigned for application. After that the
wait_till_done_function is called once. The wait_till_done_function
function blocks until the application of all the redo log records
previously assigned with apply_log_record_function calls is complete.
Even though this function assigns the log records in the hash table
sequentially, the application of the log records may be done in parallel
if the apply_log_record_function delegates the actual application work
to multiple worker threads running in parallel.
@param[in]  apply_log_record_function   a function that assigns one redo log
record for application
@param[in]  wait_till_done_function     a function that blocks until all
assigned redo log records have been applied */
void meb_apply_log_recs_via_callback(
    void (*apply_log_record_function)(recv_addr_t *),
    void (*wait_till_done_function)()) {
  ulint n_hash_cells = recv_sys->n_addrs;
  ulint i = 0;

  recv_sys->apply_log_recs = true;
  recv_sys->apply_batch_on = true;

  ib::info(ER_IB_MSG_714) << "Starting to apply a batch of log records to the"
                          << " database...";

  fputs("InnoDB: Progress in percent: ", stderr);

  for (const auto &space : *recv_sys->spaces) {
    for (auto pages : space.second.m_pages) {
      ut_ad(pages.second->space == space.first);

      (*apply_log_record_function)(pages.second);
    }

    ++i;
    if ((100 * i) / n_hash_cells != (100 * (i + 1)) / n_hash_cells) {
      fprintf(stderr, "%lu ", (ulong)((100 * i) / n_hash_cells));
      fflush(stderr);
    }
  }

  /* wait till all the redo log records have been applied */
  (*wait_till_done_function)();

  /* write logs in next line */
  fprintf(stderr, "\n");
  recv_sys->apply_log_recs = false;
  recv_sys->apply_batch_on = false;
  recv_sys_empty_hash();
}

#endif /* !UNIV_HOTBACKUP */

/** Check if redo log is for encryption information.
@param[in]      page_no         Page number
@param[in]      space_id        Tablespace identifier
@param[in]      start           Redo log record body
@param[in]      end             End of buffer
@return true if encryption information. */
static inline bool check_encryption(page_no_t page_no, space_id_t space_id,
                                    const byte *start, const byte *end) {
  /* Only page zero contains encryption metadata. */
  if (page_no != 0 || fsp_is_system_or_temp_tablespace(space_id) ||
      end < start + 4) {
    return false;
  }

  bool found = false;

  const page_size_t &page_size = fil_space_get_page_size(space_id, &found);

  if (!found) {
    return false;
  }

  auto encryption_offset = fsp_header_get_encryption_offset(page_size);
  auto offset = mach_read_from_2(start);

  /* Encryption offset at page 0 is the only way we can identify encryption
  information as of today. Ideally we should have a separate redo type. */
  if (offset == encryption_offset) {
    auto len = mach_read_from_2(start + 2);
    ut_ad(len == Encryption::INFO_SIZE);

    if (len != Encryption::INFO_SIZE) {
      /* purecov: begin inspected */
      ib::warn(ER_IB_WRN_ENCRYPTION_INFO_SIZE_MISMATCH, size_t{len},
               Encryption::INFO_SIZE);
      return false;
      /* purecov: end */
    }
    return true;
  }

  return false;
}

/** Try to parse a single log record body and also applies it if
specified.
@param[in]      type            Redo log entry type
@param[in]      ptr             Redo log record body
@param[in]      end_ptr         End of buffer
@param[in]      space_id        Tablespace identifier
@param[in]      page_no         Page number
@param[in,out]  block           Buffer block, or nullptr if
                                a page log record should not be applied
                                or if it is a MLOG_FILE_ operation
@param[in,out]  mtr             Mini-transaction, or nullptr if
                                a page log record should not be applied
@param[in]      parsed_bytes    Number of bytes parsed so far
@param[in]      start_lsn       lsn for REDO record
@return log record end, nullptr if not a complete record */
static byte *recv_parse_or_apply_log_rec_body(
    mlog_id_t type, byte *ptr, byte *end_ptr, space_id_t space_id,
    page_no_t page_no, buf_block_t *block, mtr_t *mtr, ulint parsed_bytes,
    lsn_t start_lsn) {
  bool applying_redo = (block != nullptr);

  switch (type) {
#ifndef UNIV_HOTBACKUP
    case MLOG_FILE_DELETE:

      return fil_tablespace_redo_delete(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          recv_sys->bytes_to_ignore_before_checkpoint != 0);

    case MLOG_FILE_CREATE:

      return fil_tablespace_redo_create(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          recv_sys->bytes_to_ignore_before_checkpoint != 0);

    case MLOG_FILE_RENAME:

      return fil_tablespace_redo_rename(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          recv_sys->bytes_to_ignore_before_checkpoint != 0);

    case MLOG_FILE_EXTEND:

      return fil_tablespace_redo_extend(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          recv_sys->bytes_to_ignore_before_checkpoint != 0);
#else  /* !UNIV_HOTBACKUP */
      // Mysqlbackup does not execute file operations. It cares for all
      // files to be at their final places when it applies the redo log.
      // The exception is the restore of an incremental_with_redo_log_only
      // backup.
    case MLOG_FILE_DELETE:

      return fil_tablespace_redo_delete(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          !recv_sys->apply_file_operations);

    case MLOG_FILE_CREATE:

      return fil_tablespace_redo_create(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          !recv_sys->apply_file_operations);

    case MLOG_FILE_RENAME:

      return fil_tablespace_redo_rename(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          !recv_sys->apply_file_operations);

    case MLOG_FILE_EXTEND:

      return fil_tablespace_redo_extend(
          ptr, end_ptr, page_id_t(space_id, page_no), parsed_bytes,
          !recv_sys->apply_file_operations);
#endif /* !UNIV_HOTBACKUP */

    case MLOG_INDEX_LOAD:
#ifdef UNIV_HOTBACKUP
      // While scanning redo logs during a backup operation a
      // MLOG_INDEX_LOAD type redo log record indicates, that a DDL
      // (create index, alter table...) is performed with
      // 'algorithm=inplace'. The affected tablespace must be re-copied
      // in the backup lock phase. Record it in the index_load_list.
      if (!recv_recovery_on) {
        index_load_list.emplace_back(
            std::pair<space_id_t, lsn_t>(space_id, recv_sys->recovered_lsn));
      }
#endif /* UNIV_HOTBACKUP */
      if (end_ptr < ptr + 8) {
        return nullptr;
      }

      return ptr + 8;

    case MLOG_WRITE_STRING:

#ifdef UNIV_HOTBACKUP
      if (recv_recovery_on && meb_is_space_loaded(space_id)) {
#endif /* UNIV_HOTBACKUP */
        /* For encrypted tablespace, we need to get the encryption key
        information before the page 0 is recovered. Otherwise, redo will not
        find the key to decrypt the data pages. */
        if (page_no == 0 && !applying_redo &&
            !fsp_is_system_or_temp_tablespace(space_id) &&
            /* For cloned db header page has the encryption information. */
            !recv_sys->is_cloned_db) {
          ut_ad(LSN_MAX != start_lsn);
          return fil_tablespace_redo_encryption(ptr, end_ptr, space_id,
                                                start_lsn);
        }
#ifdef UNIV_HOTBACKUP
      }
#endif /* UNIV_HOTBACKUP */

      break;

    default:
      break;
  }

  page_t *page;
  page_zip_des_t *page_zip;
  dict_index_t *index = nullptr;

#ifdef UNIV_DEBUG
  ulint page_type;
#endif /* UNIV_DEBUG */

#if defined(UNIV_HOTBACKUP) && defined(UNIV_DEBUG)
  ib::trace_3() << "recv_parse_or_apply_log_rec_body: type "
                << get_mlog_string(type) << " space_id " << space_id
                << " page_nr " << page_no << " ptr "
                << static_cast<const void *>(ptr) << " end_ptr "
                << static_cast<const void *>(end_ptr) << " block "
                << static_cast<const void *>(block) << " mtr "
                << static_cast<const void *>(mtr);
#endif /* UNIV_HOTBACKUP && UNIV_DEBUG */

  if (applying_redo) {
    /* Applying a page log record. */
    ut_ad(mtr != nullptr);

    page = block->frame;
    page_zip = buf_block_get_page_zip(block);

    ut_d(page_type = fil_page_get_type(page));
#if defined(UNIV_HOTBACKUP) && defined(UNIV_DEBUG)
    if (page_type == 0) {
      meb_print_page_header(page);
    }
#endif /* UNIV_HOTBACKUP && UNIV_DEBUG */

  } else {
    /* Parsing a page log record. */
    ut_ad(mtr == nullptr);
    page = nullptr;
    page_zip = nullptr;

    ut_d(page_type = FIL_PAGE_TYPE_ALLOCATED);
  }

  const byte *old_ptr = ptr;

  switch (type) {
#ifdef UNIV_LOG_LSN_DEBUG
    case MLOG_LSN:
      /* The LSN is checked in recv_parse_log_rec(). */
      break;
#endif /* UNIV_LOG_LSN_DEBUG */
    case MLOG_4BYTES:

      ut_ad(page == nullptr || end_ptr > ptr + 2);

      /* Most FSP flags can only be changed by CREATE or ALTER with
      ALGORITHM=COPY, so they do not change once the file
      is created. The SDI flag is the only one that can be
      changed by a recoverable transaction. So if there is
      change in FSP flags, update the in-memory space structure
      (fil_space_t) */

      if (page != nullptr && page_no == 0 &&
          mach_read_from_2(ptr) == FSP_HEADER_OFFSET + FSP_SPACE_FLAGS) {
        ptr = mlog_parse_nbytes(MLOG_4BYTES, ptr, end_ptr, page, page_zip);

        /* When applying log, we have complete records.
        They can be incomplete (ptr=nullptr) only during
        scanning (page==nullptr) */

        ut_ad(ptr != nullptr);

        fil_space_t *space = fil_space_acquire(space_id);

        ut_ad(space != nullptr);

        fil_space_set_flags(space, mach_read_from_4(FSP_HEADER_OFFSET +
                                                    FSP_SPACE_FLAGS + page));
        fil_space_release(space);

        break;
      }

      [[fallthrough]];

    case MLOG_1BYTE:
      /* If 'ALTER TABLESPACE ... ENCRYPTION' was in progress and page 0 has
      REDO entry for this, now while applying this entry, set
      encryption_op_in_progress flag now so that any other page of this
      tablespace in redo log is written accordingly. */
      if (page_no == 0 && page != nullptr && end_ptr >= ptr + 2) {
        ulint offs = mach_read_from_2(ptr);

        fil_space_t *space = fil_space_acquire(space_id);
        ut_ad(space != nullptr);
        ulint offset = fsp_header_get_encryption_progress_offset(
            page_size_t(space->flags));

        if (offs == offset) {
          ptr = mlog_parse_nbytes(MLOG_1BYTE, ptr, end_ptr, page, page_zip);
          byte op = mach_read_from_1(page + offset);
          switch (op) {
            case Encryption::ENCRYPT_IN_PROGRESS:
              space->encryption_op_in_progress =
                  Encryption::Progress::ENCRYPTION;
              break;
            case Encryption::DECRYPT_IN_PROGRESS:
              space->encryption_op_in_progress =
                  Encryption::Progress::DECRYPTION;
              break;
            default:
              space->encryption_op_in_progress = Encryption::Progress::NONE;
              break;
          }
        }
        fil_space_release(space);
      }

      [[fallthrough]];

    case MLOG_2BYTES:
    case MLOG_8BYTES:
#ifdef UNIV_DEBUG
      if (page && page_type == FIL_PAGE_TYPE_ALLOCATED && end_ptr >= ptr + 2) {
        /* It is OK to set FIL_PAGE_TYPE and certain
        list node fields on an empty page.  Any other
        write is not OK. */

        /* NOTE: There may be bogus assertion failures for
        dict_hdr_create(), trx_rseg_header_create(),
        trx_sys_create_doublewrite_buf(), and
        trx_sysf_create().
        These are only called during database creation. */

        ulint offs = mach_read_from_2(ptr);

        switch (type) {
          default:
            ut_error;
          case MLOG_2BYTES:
            /* Note that this can fail when the
            redo log been written with something
            older than InnoDB Plugin 1.0.4. */
            ut_ad(
                offs == FIL_PAGE_TYPE ||
                offs == IBUF_TREE_SEG_HEADER + IBUF_HEADER + FSEG_HDR_OFFSET ||
                offs == PAGE_BTR_IBUF_FREE_LIST + PAGE_HEADER + FIL_ADDR_BYTE ||
                offs == PAGE_BTR_IBUF_FREE_LIST + PAGE_HEADER + FIL_ADDR_BYTE +
                            FIL_ADDR_SIZE ||
                offs == PAGE_BTR_SEG_LEAF + PAGE_HEADER + FSEG_HDR_OFFSET ||
                offs == PAGE_BTR_SEG_TOP + PAGE_HEADER + FSEG_HDR_OFFSET ||
                offs == PAGE_BTR_IBUF_FREE_LIST_NODE + PAGE_HEADER +
                            FIL_ADDR_BYTE + 0 /*FLST_PREV*/
                || offs == PAGE_BTR_IBUF_FREE_LIST_NODE + PAGE_HEADER +
                               FIL_ADDR_BYTE + FIL_ADDR_SIZE /*FLST_NEXT*/);
            break;
          case MLOG_4BYTES:
            /* Note that this can fail when the
            redo log been written with something
            older than InnoDB Plugin 1.0.4. */
            ut_ad(
                0 ||
                offs == IBUF_TREE_SEG_HEADER + IBUF_HEADER + FSEG_HDR_SPACE ||
                offs == IBUF_TREE_SEG_HEADER + IBUF_HEADER + FSEG_HDR_PAGE_NO ||
                offs == PAGE_BTR_IBUF_FREE_LIST + PAGE_HEADER /* flst_init */
                ||
                offs == PAGE_BTR_IBUF_FREE_LIST + PAGE_HEADER + FIL_ADDR_PAGE ||
                offs == PAGE_BTR_IBUF_FREE_LIST + PAGE_HEADER + FIL_ADDR_PAGE +
                            FIL_ADDR_SIZE ||
                offs == PAGE_BTR_SEG_LEAF + PAGE_HEADER + FSEG_HDR_PAGE_NO ||
                offs == PAGE_BTR_SEG_LEAF + PAGE_HEADER + FSEG_HDR_SPACE ||
                offs == PAGE_BTR_SEG_TOP + PAGE_HEADER + FSEG_HDR_PAGE_NO ||
                offs == PAGE_BTR_SEG_TOP + PAGE_HEADER + FSEG_HDR_SPACE ||
                offs == PAGE_BTR_IBUF_FREE_LIST_NODE + PAGE_HEADER +
                            FIL_ADDR_PAGE + 0 /*FLST_PREV*/
                || offs == PAGE_BTR_IBUF_FREE_LIST_NODE + PAGE_HEADER +
                               FIL_ADDR_PAGE + FIL_ADDR_SIZE /*FLST_NEXT*/);
            break;
        }
      }
#endif /* UNIV_DEBUG */

      ptr = mlog_parse_nbytes(type, ptr, end_ptr, page, page_zip);

      if (ptr != nullptr && page != nullptr && page_no == 0 &&
          type == MLOG_4BYTES) {
        ulint offs = mach_read_from_2(old_ptr);

        switch (offs) {
          fil_space_t *space;
          uint32_t val;
          default:
            break;

          case FSP_HEADER_OFFSET + FSP_SPACE_FLAGS:
          case FSP_HEADER_OFFSET + FSP_SIZE:
          case FSP_HEADER_OFFSET + FSP_FREE_LIMIT:
          case FSP_HEADER_OFFSET + FSP_FREE + FLST_LEN:

            space = fil_space_get(space_id);

            ut_a(space != nullptr);

            val = mach_read_from_4(page + offs);

            switch (offs) {
              case FSP_HEADER_OFFSET + FSP_SPACE_FLAGS:
                space->flags = val;
                break;

              case FSP_HEADER_OFFSET + FSP_SIZE:

                space->size_in_header = val;

                if (space->size >= val) {
                  break;
                }

                ib::info(ER_IB_MSG_718, ulong{space->id}, space->name,
                         ulong{val});

                if (fil_space_extend(space, val)) {
                  break;
                }

                ib::error(ER_IB_MSG_719, ulong{space->id}, space->name,
                          ulong{val});
                break;

              case FSP_HEADER_OFFSET + FSP_FREE_LIMIT:
                space->free_limit = val;
                break;

              case FSP_HEADER_OFFSET + FSP_FREE + FLST_LEN:
                space->free_len = val;
                ut_ad(val == flst_get_len(page + offs));
                break;
            }
        }
      }
      break;

    case MLOG_REC_INSERT:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_cur_parse_insert_rec(false, ptr, end_ptr, block, index, mtr);
      }
      break;

    case MLOG_REC_INSERT_8027:
    case MLOG_COMP_REC_INSERT_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr !=
          (ptr = mlog_parse_index_8027(
               ptr, end_ptr, type == MLOG_COMP_REC_INSERT_8027, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_cur_parse_insert_rec(false, ptr, end_ptr, block, index, mtr);
      }
      break;

    case MLOG_REC_CLUST_DELETE_MARK:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = btr_cur_parse_del_mark_set_clust_rec(ptr, end_ptr, page, page_zip,
                                                   index);
      }

      break;

    case MLOG_REC_CLUST_DELETE_MARK_8027:
    case MLOG_COMP_REC_CLUST_DELETE_MARK_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr !=
          (ptr = mlog_parse_index_8027(
               ptr, end_ptr, type == MLOG_COMP_REC_CLUST_DELETE_MARK_8027,
               &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = btr_cur_parse_del_mark_set_clust_rec(ptr, end_ptr, page, page_zip,
                                                   index);
      }

      break;

    case MLOG_COMP_REC_SEC_DELETE_MARK:

      ut_ad(!page || fil_page_type_is_index(page_type));

      /* This log record type is obsolete, but we process it for
      backward compatibility with MySQL 5.0.3 and 5.0.4. */

      ut_a(!page || page_is_comp(page));
      ut_a(!page_zip);

      ptr = mlog_parse_index_8027(ptr, end_ptr, true, &index);

      if (ptr == nullptr) {
        break;
      }

      [[fallthrough]];

    case MLOG_REC_SEC_DELETE_MARK:

      ut_ad(!page || fil_page_type_is_index(page_type));

      ptr = btr_cur_parse_del_mark_set_sec_rec(ptr, end_ptr, page, page_zip);
      break;

    case MLOG_REC_UPDATE_IN_PLACE:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr =
            btr_cur_parse_update_in_place(ptr, end_ptr, page, page_zip, index);
      }

      break;

    case MLOG_REC_UPDATE_IN_PLACE_8027:
    case MLOG_COMP_REC_UPDATE_IN_PLACE_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr !=
          (ptr = mlog_parse_index_8027(
               ptr, end_ptr, type == MLOG_COMP_REC_UPDATE_IN_PLACE_8027,
               &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr =
            btr_cur_parse_update_in_place(ptr, end_ptr, page, page_zip, index);
      }

      break;

    case MLOG_LIST_END_DELETE:
    case MLOG_LIST_START_DELETE:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_parse_delete_rec_list(type, ptr, end_ptr, block, index, mtr);
      }

      break;

    case MLOG_LIST_END_DELETE_8027:
    case MLOG_COMP_LIST_END_DELETE_8027:
    case MLOG_LIST_START_DELETE_8027:
    case MLOG_COMP_LIST_START_DELETE_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index_8027(
                          ptr, end_ptr,
                          type == MLOG_COMP_LIST_END_DELETE_8027 ||
                              type == MLOG_COMP_LIST_START_DELETE_8027,
                          &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_parse_delete_rec_list(type, ptr, end_ptr, block, index, mtr);
      }

      break;

    case MLOG_LIST_END_COPY_CREATED:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_parse_copy_rec_list_to_created_page(ptr, end_ptr, block,
                                                       index, mtr);
      }

      break;

    case MLOG_LIST_END_COPY_CREATED_8027:
    case MLOG_COMP_LIST_END_COPY_CREATED_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr !=
          (ptr = mlog_parse_index_8027(
               ptr, end_ptr, type == MLOG_COMP_LIST_END_COPY_CREATED_8027,
               &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_parse_copy_rec_list_to_created_page(ptr, end_ptr, block,
                                                       index, mtr);
      }

      break;

    case MLOG_PAGE_REORGANIZE:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = btr_parse_page_reorganize(ptr, end_ptr, index,
                                        type == MLOG_ZIP_PAGE_REORGANIZE_8027,
                                        block, mtr);
      }

      break;

    case MLOG_PAGE_REORGANIZE_8027:
      ut_ad(!page || fil_page_type_is_index(page_type));
      /* Uncompressed pages don't have any payload in the
      MTR so ptr and end_ptr can be, and are nullptr */
      mlog_parse_index_8027(ptr, end_ptr, false, &index);
      ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

      ptr = btr_parse_page_reorganize(ptr, end_ptr, index, false, block, mtr);

      break;

    case MLOG_ZIP_PAGE_REORGANIZE:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = btr_parse_page_reorganize(ptr, end_ptr, index, true, block, mtr);
      }

      break;

    case MLOG_COMP_PAGE_REORGANIZE_8027:
    case MLOG_ZIP_PAGE_REORGANIZE_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr !=
          (ptr = mlog_parse_index_8027(ptr, end_ptr, true, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = btr_parse_page_reorganize(ptr, end_ptr, index,
                                        type == MLOG_ZIP_PAGE_REORGANIZE_8027,
                                        block, mtr);
      }

      break;

    case MLOG_PAGE_CREATE:
    case MLOG_COMP_PAGE_CREATE:

      /* Allow anything in page_type when creating a page. */
      ut_a(!page_zip);

      page_parse_create(block, type == MLOG_COMP_PAGE_CREATE, FIL_PAGE_INDEX);

      break;

    case MLOG_PAGE_CREATE_RTREE:
    case MLOG_COMP_PAGE_CREATE_RTREE:

      page_parse_create(block, type == MLOG_COMP_PAGE_CREATE_RTREE,
                        FIL_PAGE_RTREE);

      break;

    case MLOG_PAGE_CREATE_SDI:
    case MLOG_COMP_PAGE_CREATE_SDI:

      page_parse_create(block, type == MLOG_COMP_PAGE_CREATE_SDI, FIL_PAGE_SDI);

      break;

    case MLOG_UNDO_INSERT:

      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

      ptr = trx_undo_parse_add_undo_rec(ptr, end_ptr, page);

      break;

    case MLOG_UNDO_ERASE_END:

      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

      ptr = trx_undo_parse_erase_page_end(ptr, end_ptr, page, mtr);

      break;

    case MLOG_UNDO_INIT:

      /* Allow anything in page_type when creating a page. */

      ptr = trx_undo_parse_page_init(ptr, end_ptr, page, mtr);

      break;
    case MLOG_UNDO_HDR_CREATE:
    case MLOG_UNDO_HDR_REUSE:

      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);

      ptr = trx_undo_parse_page_header(type, ptr, end_ptr, page, mtr);

      break;

    case MLOG_REC_MIN_MARK:
    case MLOG_COMP_REC_MIN_MARK:

      ut_ad(!page || fil_page_type_is_index(page_type));

      /* On a compressed page, MLOG_COMP_REC_MIN_MARK
      will be followed by MLOG_COMP_REC_DELETE
      or MLOG_ZIP_WRITE_HEADER(FIL_PAGE_PREV, FIL_nullptr)
      in the same mini-transaction. */

      ut_a(type == MLOG_COMP_REC_MIN_MARK || !page_zip);

      ptr = btr_parse_set_min_rec_mark(
          ptr, end_ptr, type == MLOG_COMP_REC_MIN_MARK, page, mtr);

      break;

    case MLOG_REC_DELETE:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_cur_parse_delete_rec(ptr, end_ptr, block, index, mtr);
      }

      break;

    case MLOG_REC_DELETE_8027:
    case MLOG_COMP_REC_DELETE_8027:

      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr !=
          (ptr = mlog_parse_index_8027(
               ptr, end_ptr, type == MLOG_COMP_REC_DELETE_8027, &index))) {
        ut_a(!page || page_is_comp(page) == dict_table_is_comp(index->table));

        ptr = page_cur_parse_delete_rec(ptr, end_ptr, block, index, mtr);
      }

      break;

    case MLOG_IBUF_BITMAP_INIT:

      /* Allow anything in page_type when creating a page. */

      ptr = ibuf_parse_bitmap_init(ptr, end_ptr, block, mtr);

      break;

    case MLOG_INIT_FILE_PAGE:
    case MLOG_INIT_FILE_PAGE2: {
      /* For clone, avoid initializing page-0. Page-0 should already have been
      initialized. This is to avoid erasing encryption information. We cannot
      update encryption information later with redo logged information for
      clone. Please check comments in MLOG_WRITE_STRING. */
      bool skip_init = (recv_sys->is_cloned_db && page_no == 0);

      if (!skip_init) {
        /* Allow anything in page_type when creating a page. */
        ptr = fsp_parse_init_file_page(ptr, end_ptr, block);
      }
      break;
    }

    case MLOG_WRITE_STRING: {
      ut_ad(!page || page_type != FIL_PAGE_TYPE_ALLOCATED || page_no == 0);
      bool is_encryption = check_encryption(page_no, space_id, ptr, end_ptr);

#ifndef UNIV_HOTBACKUP
      /* Reset in-mem encryption information for the tablespace here if this
      is "resetting encryprion info" log. */
      if (is_encryption && !recv_sys->is_cloned_db) {
        byte buf[Encryption::INFO_SIZE] = {0};

        if (memcmp(ptr + 4, buf, Encryption::INFO_SIZE - 4) == 0) {
          ut_a(DB_SUCCESS == fil_reset_encryption(space_id));
        }
      }

#endif
      auto apply_page = page;

      /* For clone recovery, skip applying encryption information from
      redo log. It is already updated in page 0. Redo log encryption
      information is encrypted with donor master key and must be ignored. */
      if (recv_sys->is_cloned_db && is_encryption) {
        apply_page = nullptr;
      }

      ptr = mlog_parse_string(ptr, end_ptr, apply_page, page_zip);
      break;
    }

    case MLOG_ZIP_WRITE_NODE_PTR:

      ut_ad(!page || fil_page_type_is_index(page_type));

      ptr = page_zip_parse_write_node_ptr(ptr, end_ptr, page, page_zip);

      break;

    case MLOG_ZIP_WRITE_BLOB_PTR:

      ut_ad(!page || fil_page_type_is_index(page_type));

      ptr = page_zip_parse_write_blob_ptr(ptr, end_ptr, page, page_zip);

      break;

    case MLOG_ZIP_WRITE_HEADER:

      ut_ad(!page || fil_page_type_is_index(page_type));

      ptr = page_zip_parse_write_header(ptr, end_ptr, page, page_zip);

      break;

    case MLOG_ZIP_PAGE_COMPRESS:

      /* Allow anything in page_type when creating a page. */
      ptr = page_zip_parse_compress(ptr, end_ptr, page, page_zip);
      break;

    case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:

      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr, &index))) {
        ut_a(!page || (page_is_comp(page) == dict_table_is_comp(index->table)));

        ptr = page_zip_parse_compress_no_data(ptr, end_ptr, page, page_zip,
                                              index);
      }

      break;

    case MLOG_ZIP_PAGE_COMPRESS_NO_DATA_8027:

      if (nullptr !=
          (ptr = mlog_parse_index_8027(ptr, end_ptr, true, &index))) {
        ut_a(!page || (page_is_comp(page) == dict_table_is_comp(index->table)));

        ptr = page_zip_parse_compress_no_data(ptr, end_ptr, page, page_zip,
                                              index);
      }

      break;

    case MLOG_TEST:
#ifndef UNIV_HOTBACKUP
      if (log_test != nullptr) {
        ptr = log_test->parse_mlog_rec(ptr, end_ptr);
      } else {
        /* Just parse and ignore record to pass it and go forward. Note that
        this record is also used in the innodb.log_first_rec_group mtr test.
        The record is written in the buf0flu.cc when flushing page in that
        case. */
        Log_test::Key key;
        Log_test::Value value;
        lsn_t start_lsn, end_lsn;

        ptr = Log_test::parse_mlog_rec(ptr, end_ptr, key, value, start_lsn,
                                       end_lsn);
      }
      break;
#endif /* !UNIV_HOTBACKUP */
      /* Fall through. */

    default:
      ptr = nullptr;
      recv_sys->found_corrupt_log = true;
  }

  if (index != nullptr) {
    dict_table_t *table = index->table;

    dict_mem_index_free(index);
    dict_mem_table_free(table);
  }

  return ptr;
}

/** Adds a new log record to the hash table of log records.
@param[in]      type            log record type
@param[in]      space_id        Tablespace id
@param[in]      page_no         page number
@param[in]      body            log record body
@param[in]      rec_end         log record end
@param[in]      start_lsn       start lsn of the mtr
@param[in]      end_lsn         end lsn of the mtr */
static void recv_add_to_hash_table(mlog_id_t type, space_id_t space_id,
                                   page_no_t page_no, byte *body, byte *rec_end,
                                   lsn_t start_lsn, lsn_t end_lsn) {
  ut_ad(type != MLOG_FILE_DELETE);
  ut_ad(type != MLOG_FILE_CREATE);
  ut_ad(type != MLOG_FILE_RENAME);
  ut_ad(type != MLOG_FILE_EXTEND);
  ut_ad(type != MLOG_DUMMY_RECORD);
  ut_ad(type != MLOG_INDEX_LOAD);

  recv_sys_t::Space *space;

  space = recv_get_page_map(space_id, true);

  recv_t *recv;

  recv = static_cast<recv_t *>(mem_heap_alloc(space->m_heap, sizeof(*recv)));

  recv->type = type;
  recv->end_lsn = end_lsn;
  recv->len = rec_end - body;
  recv->start_lsn = start_lsn;

  auto it = space->m_pages.find(page_no);

  recv_addr_t *recv_addr;

  if (it != space->m_pages.end()) {
    recv_addr = it->second;

  } else {
    recv_addr = static_cast<recv_addr_t *>(
        mem_heap_alloc(space->m_heap, sizeof(*recv_addr)));

    recv_addr->space = space_id;
    recv_addr->page_no = page_no;
    recv_addr->state = RECV_NOT_PROCESSED;

    UT_LIST_INIT(recv_addr->rec_list);

    using Value = recv_sys_t::Pages::value_type;

    space->m_pages.insert(it, Value{page_no, recv_addr});

    ++recv_sys->n_addrs;
  }

  UT_LIST_ADD_LAST(recv_addr->rec_list, recv);

  recv_data_t **prev_field;

  prev_field = &recv->data;

  /* Store the log record body in chunks of less than UNIV_PAGE_SIZE:
  the heap grows into the buffer pool, and bigger chunks could not
  be allocated */

  while (rec_end > body) {
    ulint len = rec_end - body;

    if (len > RECV_DATA_BLOCK_SIZE) {
      len = RECV_DATA_BLOCK_SIZE;
    }

    recv_data_t *recv_data;

    recv_data = static_cast<recv_data_t *>(
        mem_heap_alloc(space->m_heap, sizeof(*recv_data) + len));

    *prev_field = recv_data;

    memcpy(recv_data + 1, body, len);

    prev_field = &recv_data->next;

    body += len;
  }

  *prev_field = nullptr;
}

/** Copies the log record body from recv to buf.
@param[in]      buf             Buffer of length at least recv->len
@param[in]      recv            Log record */
static void recv_data_copy_to_buf(byte *buf, recv_t *recv) {
  ulint len = recv->len;
  recv_data_t *recv_data = recv->data;

  while (len > 0) {
    ulint part_len;

    if (len > RECV_DATA_BLOCK_SIZE) {
      part_len = RECV_DATA_BLOCK_SIZE;
    } else {
      part_len = len;
    }

    memcpy(buf, ((byte *)recv_data) + sizeof(*recv_data), part_len);

    buf += part_len;
    len -= part_len;

    recv_data = recv_data->next;
  }
}

bool recv_page_is_brand_new(buf_block_t *block) {
  mutex_enter(&recv_sys->mutex);

  recv_addr_t *recv_addr;
  recv_addr = recv_get_rec(block->page.id.space(), block->page.id.page_no());
  if (recv_addr == nullptr) {
    /* no redo log treated as brand new */
    mutex_exit(&recv_sys->mutex);
    return true;
  }

  auto recv = UT_LIST_GET_FIRST(recv_addr->rec_list);
  if (recv == nullptr) {
    /* no redo log treated as brand new */
    mutex_exit(&recv_sys->mutex);
    return true;
  }
  if (recv->type == MLOG_INIT_FILE_PAGE2 || recv->type == MLOG_INIT_FILE_PAGE) {
    mutex_exit(&recv_sys->mutex);
    return true;
  }

  mutex_exit(&recv_sys->mutex);
  return false;
}

/** Applies the hashed log records to the page, if the page lsn is less than
the lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.

@param[in]      just_read_in    true if the IO handler calls this for a freshly
                                read page
@param[in,out]  block           buffer block */
void recv_recover_page_func(
#ifndef UNIV_HOTBACKUP
    bool just_read_in,
#endif /* !UNIV_HOTBACKUP */
    buf_block_t *block) {
  mutex_enter(&recv_sys->mutex);

  if (recv_sys->apply_log_recs == false) {
    /* Log records should not be applied now */

    mutex_exit(&recv_sys->mutex);

    return;
  }

  recv_addr_t *recv_addr;

  recv_addr = recv_get_rec(block->page.id.space(), block->page.id.page_no());

  if (recv_addr == nullptr || recv_addr->state == RECV_BEING_PROCESSED ||
      recv_addr->state == RECV_PROCESSED) {
#ifndef UNIV_HOTBACKUP
    ut_ad(recv_addr == nullptr || recv_needed_recovery ||
          recv_sys->scanned_lsn < recv_sys->checkpoint_lsn);
#endif /* !UNIV_HOTBACKUP */

    mutex_exit(&recv_sys->mutex);

    return;
  }

#ifndef UNIV_HOTBACKUP
  /* The following block is the scope of usage of the following bpage object
  reference.*/
  {
    buf_page_t &bpage = block->page;

    if (!fsp_is_system_temporary(bpage.id.space()) &&
        (arch_page_sys != nullptr && arch_page_sys->is_active())) {
      page_t *frame;
      lsn_t frame_lsn;

      frame = bpage.zip.data;

      if (!frame) {
        frame = block->frame;
      }
      frame_lsn = mach_read_from_8(frame + FIL_PAGE_LSN);

      arch_page_sys->track_page(&bpage, LSN_MAX, frame_lsn, true);
    }
  }
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
  /* this is explicitly false in case of meb, skip the assert */
  ut_ad(recv_needed_recovery ||
        recv_sys->scanned_lsn < recv_sys->checkpoint_lsn);

  DBUG_PRINT("ib_log", ("Applying log to page %u:%u", recv_addr->space,
                        recv_addr->page_no));

#ifdef UNIV_DEBUG
  lsn_t max_lsn;

  ut_d(max_lsn = log_sys->m_scanned_lsn);
#endif /* UNIV_DEBUG */
#else  /* !UNIV_HOTBACKUP */
  ib::trace_2() << "Applying log to space_id " << recv_addr->space
                << " page_nr " << recv_addr->page_no;
#endif /* !UNIV_HOTBACKUP */

  recv_addr->state = RECV_BEING_PROCESSED;

  mutex_exit(&recv_sys->mutex);

  mtr_t mtr;

  mtr_start(&mtr);

  mtr_set_log_mode(&mtr, MTR_LOG_NONE);

  page_t *page = block->frame;

  page_zip_des_t *page_zip = buf_block_get_page_zip(block);

#ifndef UNIV_HOTBACKUP
  if (just_read_in) {
    /* Move the ownership of the x-latch on the page to
    this OS thread, so that we can acquire a second
    x-latch on it.  This is needed for the operations to
    the page to pass the debug checks. */

    rw_lock_x_lock_move_ownership(&block->lock);
  }

  bool success = buf_page_get_known_nowait(
      RW_X_LATCH, block, Cache_hint::KEEP_OLD, __FILE__, __LINE__, &mtr);
  ut_a(success);

  buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);
#endif /* !UNIV_HOTBACKUP */

  /* Read the newest modification lsn from the page */
  lsn_t page_lsn = mach_read_from_8(page + FIL_PAGE_LSN);

#ifndef UNIV_HOTBACKUP

  /* It may be that the page has been modified in the buffer
  pool: read the newest modification LSN there */

  lsn_t page_newest_lsn;

  page_newest_lsn = buf_page_get_newest_modification(&block->page);

  if (page_newest_lsn) {
    page_lsn = page_newest_lsn;
  }
#else  /* !UNIV_HOTBACKUP */
  /* In recovery from a backup we do not really use the buffer pool */
  lsn_t page_newest_lsn = 0;
  /* Count applied and skipped log records */
  size_t applied_recs = 0;
  size_t skipped_recs = 0;
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
  lsn_t end_lsn = 0;
#endif /* !UNIV_HOTBACKUP */
  lsn_t start_lsn = 0;
  bool modification_to_page = false;

  for (auto recv : recv_addr->rec_list) {
#ifndef UNIV_HOTBACKUP
    end_lsn = recv->end_lsn;

    ut_ad(end_lsn <= max_lsn);
#endif /* !UNIV_HOTBACKUP */

    byte *buf = nullptr;

    if (recv->len > RECV_DATA_BLOCK_SIZE) {
      /* We have to copy the record body to a separate
      buffer */

      buf = static_cast<byte *>(
          ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, recv->len));

      recv_data_copy_to_buf(buf, recv);
    } else if (recv->data != nullptr) {
      buf = ((byte *)(recv->data)) + sizeof(recv_data_t);
    } else {
      /* Redo record that does not have a payload, such as
       MLOG_UNDO_ERASE_END, MLOG_COMP_PAGE_CREATE, MLOG_INIT_FILE_PAGE2 etc.
     */
      ut_ad(recv->data == nullptr);
      ut_ad(recv->len == 0);
    }

    if (recv->type == MLOG_INIT_FILE_PAGE) {
      page_lsn = page_newest_lsn;

      memset(FIL_PAGE_LSN + page, 0, 8);
      memset(UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + page, 0, 8);

      if (page_zip) {
        memset(FIL_PAGE_LSN + page_zip->data, 0, 8);
      }
    }

    /* Ignore applying the redo logs for tablespace that is
    truncated. Truncated tablespaces are handled explicitly
    post-recovery, where we will restore the tablespace back
    to a normal state.

    Applying redo at this stage will cause problems because the
    redo will have action recorded on page before tablespace
    was re-inited and that would lead to a problem later. */

    if (recv->start_lsn >= page_lsn
#ifndef UNIV_HOTBACKUP
        && undo::is_active(recv_addr->space)
#endif /* !UNIV_HOTBACKUP */
    ) {

      lsn_t end_lsn;

      if (!modification_to_page) {
#ifndef UNIV_HOTBACKUP
        ut_a(recv_needed_recovery);
#endif /* !UNIV_HOTBACKUP */
        modification_to_page = true;
        start_lsn = recv->start_lsn;
      }

      DBUG_PRINT("ib_log", ("apply " LSN_PF ":"
                            " %s len " ULINTPF " page %u:%u",
                            recv->start_lsn, get_mlog_string(recv->type),
                            recv->len, recv_addr->space, recv_addr->page_no));
      /* Since buf can be a nullptr for record types without a payload we can
      end up with nullptr + 0 if we calc buf + recv->len. This is undefined
      behaviour. Avoid this by only calculating the end_ptr when there's
      actual data to work with, otherwise set it to nullptr. */
      unsigned char *buf_end = nullptr;
      if (buf != nullptr) {
        buf_end = buf + recv->len;
      }
      recv_parse_or_apply_log_rec_body(recv->type, buf, buf_end,
                                       recv_addr->space, recv_addr->page_no,
                                       block, &mtr, ULINT_UNDEFINED, LSN_MAX);

      end_lsn = recv->start_lsn + recv->len;

      mach_write_to_8(FIL_PAGE_LSN + page, end_lsn);

      mach_write_to_8(UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + page,
                      end_lsn);

      if (page_zip) {
        mach_write_to_8(FIL_PAGE_LSN + page_zip->data, end_lsn);
      }
#ifdef UNIV_HOTBACKUP
      ++applied_recs;
    } else {
      ++skipped_recs;
#endif /* UNIV_HOTBACKUP */
    }

    if (recv->len > RECV_DATA_BLOCK_SIZE) {
      ut::free(buf);
    }
  }

#ifdef UNIV_ZIP_DEBUG
  if (fil_page_index_page_check(page)) {
    page_zip_des_t *page_zip = buf_block_get_page_zip(block);

    ut_a(!page_zip || page_zip_validate_low(page_zip, page, nullptr, false));
  }
#endif /* UNIV_ZIP_DEBUG */

#ifndef UNIV_HOTBACKUP
  if (modification_to_page) {
    buf_flush_recv_note_modification(block, start_lsn, end_lsn);
  }
#else  /* !UNIV_HOTBACKUP */
  UT_NOT_USED(start_lsn);
#endif /* !UNIV_HOTBACKUP */

  /* Make sure that committing mtr does not change the modification
  LSN values of page */
  ut_a(mtr.get_log_mode() == MTR_LOG_NONE);

  mtr_commit(&mtr);

  mutex_enter(&recv_sys->mutex);

  if (recv_max_page_lsn < page_lsn) {
    recv_max_page_lsn = page_lsn;
  }

  recv_addr->state = RECV_PROCESSED;

  ut_a(recv_sys->n_addrs > 0);
  --recv_sys->n_addrs;

  mutex_exit(&recv_sys->mutex);

#ifdef UNIV_HOTBACKUP
  ib::trace_2() << "Applied " << applied_recs << " Skipped " << skipped_recs;
#endif /* UNIV_HOTBACKUP */
}

/** Tries to parse a single log record.
@param[out]     type            log record type
@param[in]      ptr             pointer to a buffer
@param[in]      end_ptr         end of the buffer
@param[out]     space_id        tablespace identifier
@param[out]     page_no         page number
@param[out]     body            start of log record body
@return length of the record, or 0 if the record was not complete */
static ulint recv_parse_log_rec(mlog_id_t *type, byte *ptr, byte *end_ptr,
                                space_id_t *space_id, page_no_t *page_no,
                                byte **body) {
  byte *new_ptr;

  *body = nullptr;

  UNIV_MEM_INVALID(type, sizeof *type);
  UNIV_MEM_INVALID(space_id, sizeof *space_id);
  UNIV_MEM_INVALID(page_no, sizeof *page_no);
  UNIV_MEM_INVALID(body, sizeof *body);

  if (ptr == end_ptr) {
    return 0;
  }

  switch (*ptr) {
#ifdef UNIV_LOG_LSN_DEBUG
    case MLOG_LSN | MLOG_SINGLE_REC_FLAG:
    case MLOG_LSN:

      new_ptr =
          mlog_parse_initial_log_record(ptr, end_ptr, type, space_id, page_no);

      if (new_ptr != nullptr) {
        const lsn_t lsn = static_cast<lsn_t>(*space_id) << 32 | *page_no;

        ut_a(lsn == recv_sys->recovered_lsn);
      }

      *type = MLOG_LSN;
      return new_ptr == nullptr ? 0 : new_ptr - ptr;
#endif /* UNIV_LOG_LSN_DEBUG */

    case MLOG_MULTI_REC_END:
    case MLOG_DUMMY_RECORD:
      *page_no = FIL_NULL;
      *space_id = SPACE_UNKNOWN;
      *type = static_cast<mlog_id_t>(*ptr);
      return 1;

    case MLOG_MULTI_REC_END | MLOG_SINGLE_REC_FLAG:
    case MLOG_DUMMY_RECORD | MLOG_SINGLE_REC_FLAG:
      recv_sys->found_corrupt_log = true;
      return 0;

    case MLOG_TABLE_DYNAMIC_META:
    case MLOG_TABLE_DYNAMIC_META | MLOG_SINGLE_REC_FLAG:

      table_id_t id;
      uint64_t version;

      *page_no = FIL_NULL;
      *space_id = SPACE_UNKNOWN;

      new_ptr =
          mlog_parse_initial_dict_log_record(ptr, end_ptr, type, &id, &version);

      if (new_ptr != nullptr) {
        new_ptr = recv_sys->metadata_recover->parseMetadataLog(
            id, version, new_ptr, end_ptr);
      }

      return new_ptr == nullptr ? 0 : new_ptr - ptr;
  }

  new_ptr =
      mlog_parse_initial_log_record(ptr, end_ptr, type, space_id, page_no);

  *body = new_ptr;

  if (new_ptr == nullptr) {
    return 0;
  }

  new_ptr = recv_parse_or_apply_log_rec_body(
      *type, new_ptr, end_ptr, *space_id, *page_no, nullptr, nullptr,
      new_ptr - ptr, recv_sys->recovered_lsn);

  if (new_ptr == nullptr) {
    return 0;
  }

  return new_ptr - ptr;
}

/** Subtracts next number of bytes to ignore before we reach the checkpoint
or returns information that there was nothing more to skip.
@param[in]      next_parsed_bytes       number of next bytes that were parsed,
which are supposed to be subtracted from bytes to ignore before checkpoint
@retval true    there were still bytes to ignore
@retval false   there was already 0 bytes to ignore, nothing changed. */
static bool recv_update_bytes_to_ignore_before_checkpoint(
    size_t next_parsed_bytes) {
  auto &to_ignore = recv_sys->bytes_to_ignore_before_checkpoint;

  if (to_ignore != 0) {
    if (to_ignore >= next_parsed_bytes) {
      to_ignore -= next_parsed_bytes;
    } else {
      to_ignore = 0;
    }
    return true;
  }

  return false;
}

/** Tracks changes of recovered_lsn and tracks proper values for what
first_rec_group should be for consecutive blocks. Must be called when
recv_sys->recovered_lsn is changed to next lsn pointing at boundary
between consecutive parsed mini-transactions. */
static void recv_track_changes_of_recovered_lsn() {
  if (recv_sys->parse_start_lsn == 0) {
    return;
  }
  /* If we have already found the first block with mtr beginning there,
  we started to track boundaries between blocks. Since then we track
  all proper values of first_rec_group for consecutive blocks.
  The reason for that is to ensure that the first_rec_group of the last
  block is correct. Even though we do not depend during this recovery
  on that value, it would become important if we crashed later, because
  the last recovered block would become the first used block in redo and
  since then we would depend on a proper value of first_rec_group there.
  The checksums of log blocks should detect if it was incorrect, but the
  checksums might be disabled in the configuration. */
  const auto old_block =
      recv_sys->previous_recovered_lsn / OS_FILE_LOG_BLOCK_SIZE;

  const auto new_block = recv_sys->recovered_lsn / OS_FILE_LOG_BLOCK_SIZE;

  if (old_block != new_block) {
    ut_a(new_block > old_block);

    recv_sys->last_block_first_mtr_boundary = recv_sys->recovered_lsn;
  }

  recv_sys->previous_recovered_lsn = recv_sys->recovered_lsn;
}

/** Parse and store a single log record entry.
@param[in]      ptr             start of buffer
@param[in]      end_ptr         end of buffer
@return true if end of processing */
static bool recv_single_rec(byte *ptr, byte *end_ptr) {
  /* The mtr did not modify multiple pages */

  lsn_t old_lsn = recv_sys->recovered_lsn;

  /* Try to parse a log record, fetching its type, space id,
  page no, and a pointer to the body of the log record */

  byte *body;
  mlog_id_t type;
  page_no_t page_no;
  space_id_t space_id;

  ulint len =
      recv_parse_log_rec(&type, ptr, end_ptr, &space_id, &page_no, &body);

  if (recv_sys->found_corrupt_log) {
    recv_report_corrupt_log(ptr, type, space_id, page_no);

#ifdef UNIV_HOTBACKUP
    return true;
#endif /* UNIV_HOTBACKUP */

  } else if (len == 0 || recv_sys->found_corrupt_fs) {
    return true;
  }

  lsn_t new_recovered_lsn;

  new_recovered_lsn = recv_calc_lsn_on_data_add(old_lsn, len);

  if (new_recovered_lsn > recv_sys->scanned_lsn) {
    /* The log record filled a log block, and we
    require that also the next log block should
    have been scanned in */

    return true;
  }

  recv_previous_parsed_rec_type = type;
  recv_previous_parsed_rec_is_multi = 0;
  recv_previous_parsed_rec_offset = recv_sys->recovered_offset;

  recv_sys->recovered_offset += len;
  recv_sys->recovered_lsn = new_recovered_lsn;

  recv_track_changes_of_recovered_lsn();

  if (recv_update_bytes_to_ignore_before_checkpoint(len)) {
    return false;
  }

  switch (type) {
    case MLOG_DUMMY_RECORD:
      /* Do nothing */
      break;

#ifdef UNIV_LOG_LSN_DEBUG
    case MLOG_LSN:
      /* Do not add these records to the hash table.
      The page number and space id fields are misused
      for something else. */
      break;
#endif /* UNIV_LOG_LSN_DEBUG */

    default:

      if (recv_recovery_on) {
#ifndef UNIV_HOTBACKUP
        if (space_id == TRX_SYS_SPACE ||
            fil_tablespace_lookup_for_recovery(space_id)) {
#endif /* !UNIV_HOTBACKUP */

          recv_add_to_hash_table(type, space_id, page_no, body, ptr + len,
                                 old_lsn, recv_sys->recovered_lsn);

#ifndef UNIV_HOTBACKUP
        } else {
          recv_sys->missing_ids.insert(space_id);
        }
#endif /* !UNIV_HOTBACKUP */
      }

      [[fallthrough]];

    case MLOG_INDEX_LOAD:
    case MLOG_FILE_DELETE:
    case MLOG_FILE_RENAME:
    case MLOG_FILE_CREATE:
    case MLOG_FILE_EXTEND:
    case MLOG_TABLE_DYNAMIC_META:

      /* These were already handled by
      recv_parse_log_rec() and
      recv_parse_or_apply_log_rec_body(). */

      DBUG_PRINT("ib_log",
                 ("scan " LSN_PF ": log rec %s"
                  " len " ULINTPF " " PAGE_ID_PF,
                  old_lsn, get_mlog_string(type), len, space_id, page_no));
      break;
  }

  return false;
}

/** Parse and store a multiple record log entry.
@param[in]      ptr             start of buffer
@param[in]      end_ptr         end of buffer
@return true if end of processing */
static bool recv_multi_rec(byte *ptr, byte *end_ptr) {
  /* Check that all the records associated with the single mtr
  are included within the buffer */

  ulint n_recs = 0;
  ulint total_len = 0;

  for (;;) {
    mlog_id_t type = MLOG_BIGGEST_TYPE;
    byte *body;
    page_no_t page_no = 0;
    space_id_t space_id = 0;

    ulint len =
        recv_parse_log_rec(&type, ptr, end_ptr, &space_id, &page_no, &body);

    if (recv_sys->found_corrupt_log) {
      recv_report_corrupt_log(ptr, type, space_id, page_no);

      return true;

    } else if (len == 0) {
      return true;

    } else if ((*ptr & MLOG_SINGLE_REC_FLAG)) {
      recv_sys->found_corrupt_log = true;

      recv_report_corrupt_log(ptr, type, space_id, page_no);

      return true;

    } else if (recv_sys->found_corrupt_fs) {
      return true;
    }

    recv_sys->save_rec(n_recs, space_id, page_no, type, body, len);

    recv_previous_parsed_rec_type = type;

    recv_previous_parsed_rec_offset = recv_sys->recovered_offset + total_len;

    recv_previous_parsed_rec_is_multi = 1;

    total_len += len;
    ++n_recs;

    ptr += len;

    if (type == MLOG_MULTI_REC_END) {
      DBUG_PRINT("ib_log", ("scan " LSN_PF ": multi-log end total_len " ULINTPF
                            " n=" ULINTPF,
                            recv_sys->recovered_lsn, total_len, n_recs));

      break;
    }

    DBUG_PRINT("ib_log",
               ("scan " LSN_PF ": multi-log rec %s len " ULINTPF " " PAGE_ID_PF,
                recv_sys->recovered_lsn, get_mlog_string(type), len, space_id,
                page_no));
  }

  lsn_t new_recovered_lsn =
      recv_calc_lsn_on_data_add(recv_sys->recovered_lsn, total_len);

  if (new_recovered_lsn > recv_sys->scanned_lsn) {
    /* The log record filled a log block, and we require
    that also the next log block should have been scanned in */

    return true;
  }

  /* Add all the records to the hash table */

  ptr = recv_sys->buf + recv_sys->recovered_offset;

  for (ulint i = 0; i < n_recs; i++) {
    lsn_t old_lsn = recv_sys->recovered_lsn;

    /* This will apply MLOG_FILE_ records. */
    space_id_t space_id = 0;
    page_no_t page_no = 0;

    mlog_id_t type = MLOG_BIGGEST_TYPE;

    byte *body = nullptr;
    size_t len = 0;

    /* Avoid parsing if we have the record saved already. */
    if (!recv_sys->get_saved_rec(i, space_id, page_no, type, body, len)) {
      len = recv_parse_log_rec(&type, ptr, end_ptr, &space_id, &page_no, &body);
    }

    if (recv_sys->found_corrupt_log &&
        !recv_report_corrupt_log(ptr, type, space_id, page_no)) {
      return true;

    } else if (recv_sys->found_corrupt_fs) {
      return true;
    }

    ut_a(len != 0);
    ut_a(!(*ptr & MLOG_SINGLE_REC_FLAG));

    recv_sys->recovered_offset += len;

    recv_sys->recovered_lsn = recv_calc_lsn_on_data_add(old_lsn, len);

    const bool apply = !recv_update_bytes_to_ignore_before_checkpoint(len);

    switch (type) {
      case MLOG_MULTI_REC_END:
        recv_track_changes_of_recovered_lsn();
        /* Found the end mark for the records */
        return false;

#ifdef UNIV_LOG_LSN_DEBUG
      case MLOG_LSN:
        /* Do not add these records to the hash table.
        The page number and space id fields are misused
        for something else. */
        break;
#endif /* UNIV_LOG_LSN_DEBUG */

      case MLOG_FILE_DELETE:
      case MLOG_FILE_CREATE:
      case MLOG_FILE_RENAME:
      case MLOG_FILE_EXTEND:
      case MLOG_TABLE_DYNAMIC_META:
        /* case MLOG_TRUNCATE: Disabled for WL6378 */
        /* These were already handled by
        recv_parse_or_apply_log_rec_body(). */
        break;

      default:

        if (!apply) {
          break;
        }

        if (recv_recovery_on) {
#ifndef UNIV_HOTBACKUP
          if (space_id == TRX_SYS_SPACE ||
              fil_tablespace_lookup_for_recovery(space_id)) {
#endif /* !UNIV_HOTBACKUP */

            recv_add_to_hash_table(type, space_id, page_no, body, ptr + len,
                                   old_lsn, new_recovered_lsn);

#ifndef UNIV_HOTBACKUP
          } else {
            recv_sys->missing_ids.insert(space_id);
          }
#endif /* !UNIV_HOTBACKUP */
        }
    }

    ptr += len;
  }

  return false;
}

/** Parse log records from a buffer and optionally store them to a
hash table to wait merging to file pages. */
static void recv_parse_log_recs() {
  ut_ad(recv_sys->parse_start_lsn != 0);

  for (;;) {
    byte *ptr = recv_sys->buf + recv_sys->recovered_offset;

    byte *end_ptr = recv_sys->buf + recv_sys->len;

    if (ptr == end_ptr) {
      return;
    }

    bool single_rec;

    switch (*ptr) {
#ifdef UNIV_LOG_LSN_DEBUG
      case MLOG_LSN:
#endif /* UNIV_LOG_LSN_DEBUG */
      case MLOG_DUMMY_RECORD:
        single_rec = true;
        break;
      default:
        single_rec = !!(*ptr & MLOG_SINGLE_REC_FLAG);
    }

    if (single_rec) {
      if (recv_single_rec(ptr, end_ptr)) {
        return;
      }

    } else if (recv_multi_rec(ptr, end_ptr)) {
      return;
    }
  }
}

/** Adds data from a new log block to the parsing buffer of recv_sys if
recv_sys->parse_start_lsn is non-zero.
@param[in]      log_block               log block
@param[in]      scanned_lsn             lsn of how far we were able
                                        to find data in this log block
@return true if more data added */
static bool recv_sys_add_to_parsing_buf(const byte *log_block,
                                        lsn_t scanned_lsn) {
  ut_ad(scanned_lsn >= recv_sys->scanned_lsn);

  if (!recv_sys->parse_start_lsn) {
    /* Cannot start parsing yet because no start point for
    it found */

    return false;
  }

  ulint more_len;
  ulint data_len = log_block_get_data_len(log_block);

  if (recv_sys->parse_start_lsn >= scanned_lsn) {
    return false;

  } else if (recv_sys->scanned_lsn >= scanned_lsn) {
    return false;

  } else if (recv_sys->parse_start_lsn > recv_sys->scanned_lsn) {
    more_len = (ulint)(scanned_lsn - recv_sys->parse_start_lsn);

  } else {
    more_len = (ulint)(scanned_lsn - recv_sys->scanned_lsn);
  }

  if (more_len == 0) {
    return false;
  }

  ut_ad(data_len >= more_len);

  ulint start_offset = data_len - more_len;

  if (start_offset < LOG_BLOCK_HDR_SIZE) {
    start_offset = LOG_BLOCK_HDR_SIZE;
  }

  ulint end_offset = data_len;

  if (end_offset > OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
    end_offset = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE;
  }

  ut_ad(start_offset <= end_offset);

  if (start_offset < end_offset) {
    memcpy(recv_sys->buf + recv_sys->len, log_block + start_offset,
           end_offset - start_offset);

    recv_sys->len += end_offset - start_offset;

    ut_a(recv_sys->len <= recv_sys->buf_len);
  }

  return true;
}

/** Moves the parsing buffer data left to the buffer start. */
static void recv_reset_buffer() {
  ut_memmove(recv_sys->buf, recv_sys->buf + recv_sys->recovered_offset,
             recv_sys->len - recv_sys->recovered_offset);

  recv_sys->len -= recv_sys->recovered_offset;

  recv_sys->recovered_offset = 0;
}

/** Scans log from a buffer and stores new log data to the parsing buffer.
Parses and hashes the log records if new data found.  Unless
UNIV_HOTBACKUP is defined, this function will apply log records
automatically when the hash table becomes full.
@param[in,out]  log             redo log
@param[in]      max_memory      we let the hash table of recs to grow to
                                this size, at the maximum
@param[in]      buf             buffer containing a log segment or garbage
@param[in]      len             buffer length
@param[in]      start_lsn       buffer start lsn
@param[out]  read_upto_lsn  scanning succeeded up to this lsn
@return true if not able to scan any more in this log */
#ifndef UNIV_HOTBACKUP
static bool recv_scan_log_recs(log_t &log,
#else  /* !UNIV_HOTBACKUP */
bool meb_scan_log_recs(
#endif /* !UNIV_HOTBACKUP */
                               size_t max_memory, const byte *buf, size_t len,
                               lsn_t start_lsn, lsn_t *read_upto_lsn) {
  const byte *log_block = buf;
  lsn_t scanned_lsn = start_lsn;
  bool finished = false;
  bool more_data = false;

  ut_ad(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_ad(len % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_ad(len >= OS_FILE_LOG_BLOCK_SIZE);

  do {
    ut_ad(!finished);

    Log_data_block_header block_header;
    log_data_block_header_deserialize(log_block, block_header);

    const uint32_t expected_hdr_no =
        log_block_convert_lsn_to_hdr_no(scanned_lsn);

    if (block_header.m_hdr_no != expected_hdr_no) {
      /* Garbage or an incompletely written log block.

      We will not report any error, because this can
      happen when InnoDB was killed while it was
      writing redo log. We simply treat this as an
      abrupt end of the redo log. */

      finished = true;

      break;
    }

    if (!log_block_checksum_is_ok(log_block)) {
      uint32_t checksum1 = log_block_get_checksum(log_block);
      uint32_t checksum2 = log_block_calc_checksum(log_block);
      ib::error(ER_IB_MSG_720, ulong{block_header.m_hdr_no},
                ulonglong{scanned_lsn}, ulong{checksum1}, ulong{checksum2});

      /* Garbage or an incompletely written log block.

      This could be the result of killing the server
      while it was writing this log block. We treat
      this as an abrupt end of the redo log. */

      finished = true;

      break;
    }

    const auto data_len = block_header.m_data_len;

    if (scanned_lsn + data_len > recv_sys->scanned_lsn &&
        recv_sys->scanned_epoch_no > 0 &&
        !log_block_epoch_no_is_valid(block_header.m_epoch_no,
                                     recv_sys->scanned_epoch_no)) {
      /* Garbage from a log buffer flush which was made
      before the most recent database recovery */

      finished = true;

      break;
    }

    if (!recv_sys->parse_start_lsn && block_header.m_first_rec_group > 0) {
      /* We found a point from which to start the parsing of log records */

      recv_sys->parse_start_lsn = scanned_lsn + block_header.m_first_rec_group;

      ib::info(ER_IB_MSG_1261)
          << "Starting to parse redo log at lsn = " << recv_sys->parse_start_lsn
          << ", whereas checkpoint_lsn = " << recv_sys->checkpoint_lsn
          << " and start_lsn = " << start_lsn;

      if (recv_sys->parse_start_lsn < recv_sys->checkpoint_lsn) {
        /* We start to parse log records even before
        checkpoint_lsn, from the beginning of the log
        block which contains the checkpoint_lsn.

        That's because the first group of log records
        in the log block, starts before checkpoint_lsn,
        and checkpoint_lsn could potentially point to
        the middle of some log record. We need to find
        the first group of log records that starts at
        or after checkpoint_lsn. This could be only
        achieved by traversing all groups of log records
        that start within the log block since the first
        one (to discover their beginnings we need to
        parse them). However, we don't want to report
        missing tablespaces for space_id in log records
        before checkpoint_lsn. Hence we need to ignore
        those records and that's why we need a counter
        of bytes to ignore. */

        recv_sys->bytes_to_ignore_before_checkpoint =
            recv_sys->checkpoint_lsn - recv_sys->parse_start_lsn;

        ut_a(recv_sys->bytes_to_ignore_before_checkpoint <=
             OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE);

        ut_a(recv_sys->checkpoint_lsn % OS_FILE_LOG_BLOCK_SIZE +
                 LOG_BLOCK_TRL_SIZE <
             OS_FILE_LOG_BLOCK_SIZE);

        ut_a(recv_sys->parse_start_lsn % OS_FILE_LOG_BLOCK_SIZE >=
             LOG_BLOCK_HDR_SIZE);
      }

      recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
      recv_sys->recovered_lsn = recv_sys->parse_start_lsn;

      recv_track_changes_of_recovered_lsn();
    }

    scanned_lsn += data_len;

    if (scanned_lsn > recv_sys->scanned_lsn) {
#ifndef UNIV_HOTBACKUP
      if (!recv_needed_recovery && scanned_lsn > recv_sys->checkpoint_lsn) {
        if (srv_read_only_mode) {
          ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);
          ib::warn(ER_IB_MSG_RECOVERY_SKIPPED_IN_READ_ONLY_MODE);
          *read_upto_lsn = scanned_lsn;
          return true;
        }

        ib::info(ER_IB_MSG_722, ulonglong{recv_sys->scanned_lsn});

        recv_init_crash_recovery();
      }
#endif /* !UNIV_HOTBACKUP */

      /* We were able to find more log data: add it to the
      parsing buffer if parse_start_lsn is already
      non-zero */

      DBUG_EXECUTE_IF("simulate_3mb_mtr_recovery", {
        uint saved_len = recv_sys->len;
        recv_sys->len = 3 * 1024 * 1024;
        recv_sys_resize_buf();
        recv_sys->len = saved_len;
      });

      if (recv_sys->len + 4 * OS_FILE_LOG_BLOCK_SIZE >= recv_sys->buf_len) {
        if (!recv_sys_resize_buf()) {
          recv_sys->found_corrupt_log = true;

#ifndef UNIV_HOTBACKUP
          if (srv_force_recovery == 0) {
            ib::error(ER_IB_MSG_724);
            return true;
          }
#else  /* !UNIV_HOTBACKUP */
          ib::fatal(UT_LOCATION_HERE,
                    ER_IB_ERR_NOT_ENOUGH_MEMORY_FOR_PARSE_BUFFER)
              << "Insufficient memory for InnoDB parse buffer; want "
              << recv_sys->buf_len;
#endif /* !UNIV_HOTBACKUP */
        }
      }

      if (!recv_sys->found_corrupt_log) {
        more_data = recv_sys_add_to_parsing_buf(log_block, scanned_lsn);
      }

      recv_sys->scanned_lsn = scanned_lsn;

      recv_sys->scanned_epoch_no = block_header.m_epoch_no;
    }

    if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
      /* Log data for this group ends here */
      finished = true;

      break;

    } else {
      log_block += OS_FILE_LOG_BLOCK_SIZE;
    }

  } while (log_block < buf + len);

  *read_upto_lsn = scanned_lsn;

  if (recv_needed_recovery ||
      (recv_is_from_backup && !recv_is_making_a_backup)) {
    ++recv_scan_print_counter;

    if (finished || (recv_scan_print_counter % 80) == 0) {
      ib::info(ER_IB_MSG_725, ulonglong{scanned_lsn});
    }
  }

  if (more_data && !recv_sys->found_corrupt_log) {
    /* Try to parse more log records */

    recv_parse_log_recs();

#ifndef UNIV_HOTBACKUP
    if (recv_heap_used() > max_memory) {
      recv_apply_hashed_log_recs(log, false);
    }
#endif /* !UNIV_HOTBACKUP */

    if (recv_sys->recovered_offset > recv_sys->buf_len / 4) {
      /* Move parsing buffer data to the buffer start */

      recv_reset_buffer();
    }
  }

  return finished;
}

#ifndef UNIV_HOTBACKUP
static lsn_t recv_read_log_seg(log_t &log, byte *buf, lsn_t start_lsn,
                               const lsn_t end_lsn) {
  log_background_threads_inactive_validate();

  ut_a(start_lsn < end_lsn);

  auto file = log.m_files.find(start_lsn);

  if (file == log.m_files.end()) {
    /* Missing valid file ! */
    return start_lsn;
  }

  auto file_handle = file->open(Log_file_access_mode::READ_ONLY);
  ut_a(file_handle.is_open());

  do {
    os_offset_t source_offset;

    source_offset = file->offset(start_lsn);

    ut_a(end_lsn - start_lsn <= ULINT_MAX);

    os_offset_t len = end_lsn - start_lsn;

    ut_ad(len != 0);

    bool switch_to_next_file = false;

    if (source_offset + len > file->m_size_in_bytes) {
      /* If the above condition is true then len
      (which is unsigned) is > the expression below,
      so the typecast is ok */
      ut_a(file->m_size_in_bytes > source_offset);
      len = file->m_size_in_bytes - source_offset;
      switch_to_next_file = true;
    }

    ++log.n_log_ios;

    const dberr_t err =
        log_data_blocks_read(file_handle, source_offset, len, buf);
    ut_a(err == DB_SUCCESS);

    start_lsn += len;
    buf += len;

    if (switch_to_next_file) {
      auto next_id = file->next_id();

      const auto next_file = log.m_files.file(next_id);

      if (next_file == log.m_files.end() || !next_file->contains(start_lsn)) {
        return start_lsn;
      }

      file_handle.close();

      file = next_file;

      file_handle = file->open(Log_file_access_mode::READ_ONLY);
      ut_a(file_handle.is_open());
    }

  } while (start_lsn != end_lsn);

  ut_a(start_lsn == end_lsn);

  return end_lsn;
}

/** Scans log from a buffer and stores new log data to the parsing buffer.
Parses and hashes the log records if new data found.
@param[in,out]  log                     redo log
@param[in,out]  checkpoint_lsn          log sequence number found in checkpoint
                                        header. May be inexact (in a middle of
                                        an mtr which we can ignore, as it is
                                        already applied to tablespace files)
                                        until which all redo log has been
                                        scanned */
static void recv_recovery_begin(log_t &log, const lsn_t checkpoint_lsn) {
  mutex_enter(&recv_sys->mutex);

  recv_sys->len = 0;
  recv_sys->recovered_offset = 0;
  recv_sys->n_addrs = 0;
  recv_sys_empty_hash();

  /* Since 8.0, we can start recovery at checkpoint_lsn which points
  to the middle of log record. In such case we first to need to find
  the beginning of the first group of log records, which is at lsn
  greater than the checkpoint_lsn. */
  recv_sys->parse_start_lsn = 0;

  /* This is updated when we find value for parse_start_lsn. */
  recv_sys->bytes_to_ignore_before_checkpoint = 0;

  recv_sys->checkpoint_lsn = checkpoint_lsn;
  recv_sys->scanned_lsn = checkpoint_lsn;
  recv_sys->recovered_lsn = checkpoint_lsn;

  /* We have to trust that the first_rec_group in the first block is
  correct as we can't start parsing earlier to check it ourselves. */
  recv_sys->previous_recovered_lsn = checkpoint_lsn;
  recv_sys->last_block_first_mtr_boundary = 0;

  recv_sys->scanned_epoch_no = 0;
  recv_previous_parsed_rec_type = MLOG_SINGLE_REC_FLAG;
  recv_previous_parsed_rec_offset = 0;
  recv_previous_parsed_rec_is_multi = 0;
  ut_ad(recv_max_page_lsn == 0);

  mutex_exit(&recv_sys->mutex);

  ulint max_mem =
      UNIV_PAGE_SIZE * (buf_pool_get_n_pages() -
                        (recv_n_pool_free_frames * srv_buf_pool_instances));

  lsn_t start_lsn =
      ut_uint64_align_down(checkpoint_lsn, OS_FILE_LOG_BLOCK_SIZE);

  bool finished = false;

  while (!finished) {
    const lsn_t end_lsn =
        recv_read_log_seg(log, log.buf, start_lsn, start_lsn + RECV_SCAN_SIZE);

    if (end_lsn == start_lsn) {
      /* This could happen if we crashed just after completing file,
      and before next file has been successfully created. */
      break;
    }

    finished = recv_scan_log_recs(log, max_mem, log.buf, end_lsn - start_lsn,
                                  start_lsn, &log.m_scanned_lsn);

    start_lsn = end_lsn;
  }

  DBUG_PRINT("ib_log", ("scan " LSN_PF " completed", log.m_scanned_lsn));
}

/** Initialize crash recovery environment. Can be called iff
recv_needed_recovery == false. */
static void recv_init_crash_recovery() {
  ut_ad(!srv_read_only_mode);
  ut_a(!recv_needed_recovery);

  recv_needed_recovery = true;

  ib::info(ER_IB_MSG_726);
  ib::info(ER_IB_MSG_727);

  recv_sys->dblwr->recover();

  if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
    /* Spawn the background thread to flush dirty pages
    from the buffer pools. */

    srv_threads.m_recv_writer =
        os_thread_create(recv_writer_thread_key, 0, recv_writer_thread);

    srv_threads.m_recv_writer.start();
  }
}
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP

dberr_t recv_recovery_from_checkpoint_start(log_t &log, lsn_t flush_lsn) {
  /* Initialize red-black tree for fast insertions into the
  flush_list during recovery process. */
  buf_flush_init_flush_rbt();

  if (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO) {
    ib::info(ER_IB_MSG_728);

    /* We leave redo log not started and this is read-only mode. */
    ut_a(log.sn == 0);
    ut_a(srv_read_only_mode);

    return DB_SUCCESS;
  }

  recv_recovery_on = true;

  ut_a(log.m_format == Log_format::CURRENT);

  /* Look for the latest checkpoint */
  Log_checkpoint_location checkpoint;
  if (!recv_find_max_checkpoint(log, checkpoint)) {
    ib::error(ER_IB_MSG_RECOVERY_CHECKPOINT_NOT_FOUND);
    return DB_ERROR;
  }

  const auto checkpoint_file = log.m_files.find(checkpoint.m_checkpoint_lsn);

  /* When reading checkpoints from redo log files, error would be reported
  if checkpoint_lsn was outside the redo log file from which it was read,
  and such file would be skipped. If no checkpoint was found because of that,
  then recv_find_max_checkpoint would return false. Therefore here we know
  that InnoDB found a valid checkpoint (for which there is a redo log file
  which contains the checkpoint_lsn). */
  if (checkpoint_file == log.m_files.end()) {
    ut_d(ut_error);
    ut_o(return DB_ERROR);
  }

  log.last_checkpoint_lsn.store(checkpoint.m_checkpoint_lsn);

  const auto file_path = log_file_path(log.m_files_ctx, checkpoint_file->m_id);
  ib::info(ER_IB_MSG_LOG_CHECKPOINT_FOUND,
           ulonglong{checkpoint.m_checkpoint_lsn}, file_path.c_str());

  Log_checkpoint_header checkpoint_header;

  auto checkpoint_file_handle =
      checkpoint_file->open(Log_file_access_mode::READ_ONLY);

  if (!checkpoint_file_handle.is_open()) {
    return DB_CANNOT_OPEN_FILE;
  }

  dberr_t err = log_checkpoint_header_read(checkpoint_file_handle,
                                           checkpoint.m_checkpoint_header_no,
                                           checkpoint_header);
  if (err != DB_SUCCESS) {
    return err;
  }

  checkpoint_file_handle.close();

  const lsn_t checkpoint_lsn = checkpoint.m_checkpoint_lsn;

  ut_a(checkpoint_lsn == checkpoint_header.m_checkpoint_lsn);

  /* Read the encryption header to get the encryption information. */
  err = log_encryption_read(log);
  if (err != DB_SUCCESS) {
    return DB_ERROR;
  }

  /* Start reading the log from the checkpoint LSN up. */

  ut_ad(RECV_SCAN_SIZE <= log.buf_size);

  ut_ad(recv_sys->n_addrs == 0);

  /* NOTE: we always do a 'recovery' at startup, but only if
  there is something wrong we will print a message to the
  user about recovery: */

  if (checkpoint_lsn != flush_lsn) {
    if (checkpoint_lsn < flush_lsn) {
      ib::warn(ER_IB_MSG_RECOVERY_CHECKPOINT_FROM_BEFORE_CLEAN_SHUTDOWN,
               ulonglong{checkpoint_lsn}, ulonglong{flush_lsn});
    }

    if (!recv_needed_recovery) {
      ib::info(ER_IB_MSG_RECOVERY_IS_NEEDED, ulonglong{flush_lsn},
               ulonglong{checkpoint_lsn});

      if (srv_read_only_mode) {
        ib::error(ER_IB_MSG_RECOVERY_IN_READ_ONLY);

        return DB_ERROR;
      }

      recv_init_crash_recovery();
    }
  }

  recv_recovery_begin(log, checkpoint_lsn);

  if (srv_read_only_mode && log.m_scanned_lsn > checkpoint_lsn) {
    ib::error(ER_IB_MSG_RECOVERY_IN_READ_ONLY);
    return DB_ERROR;
  }

  lsn_t recovered_lsn;

  recovered_lsn = recv_sys->recovered_lsn;

  ut_a(recv_needed_recovery || checkpoint_lsn == recovered_lsn);

  ut_a(!srv_read_only_mode || !recv_needed_recovery);
  ut_a(!srv_read_only_mode || checkpoint_lsn == recovered_lsn);

  log.recovered_lsn = recovered_lsn;

  ut_a(log.m_files.find(recovered_lsn) != log.m_files.end());

  /* If it is at block boundary, add header size. */
  auto check_scanned_lsn = log.m_scanned_lsn;
  if (check_scanned_lsn % OS_FILE_LOG_BLOCK_SIZE == 0) {
    check_scanned_lsn += LOG_BLOCK_HDR_SIZE;
  }

  if (check_scanned_lsn < checkpoint_lsn ||
      check_scanned_lsn < recv_max_page_lsn) {
    ib::error(ER_IB_MSG_737, ulonglong{log.m_scanned_lsn},
              ulonglong{checkpoint_lsn}, ulonglong{recv_max_page_lsn});
  }

  if (recovered_lsn < checkpoint_lsn) {
    /* No harm in trying to do RO access. */
    if (!srv_read_only_mode) {
      ut_error;
    }

    return DB_ERROR;
  }

  if ((recv_sys->found_corrupt_log && srv_force_recovery == 0) ||
      recv_sys->found_corrupt_fs) {
    return DB_ERROR;
  }

  /* Disallow checkpoints until recovery is finished, and changes gathered
  in recv_sys->metadata_recover (dict_metadata) are transferred to
  dict_table_t objects (happens in srv0start.cc). */

  err = log_start(log, checkpoint_lsn, recovered_lsn, false);
  if (err != DB_SUCCESS) {
    return err;
  }

  /* Make the preservation of max checkpoint info on disk certain by writing
  the checkpoint also to the other checkpoint header. After that both headers
  will have the same checkpoint_lsn. This is an extra protection in case next
  checkpoint write will become corrupted because of crash during the write. */

  if (!srv_read_only_mode) {
    log.next_checkpoint_header_no =
        log_next_checkpoint_header(checkpoint.m_checkpoint_header_no);

    err = log_files_next_checkpoint(log, checkpoint_lsn);
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  mutex_enter(&recv_sys->mutex);
  recv_sys->apply_log_recs = true;
  mutex_exit(&recv_sys->mutex);

  /* The database is now ready to start almost normal processing of user
  transactions: transaction rollbacks and the application of the log
  records in the hash table can be run in background. */

  return DB_SUCCESS;
}

/** Check the page type, if there is a mismtach then throw
fatal error. It may so happen that data file before 5.7 GA version
may contain uninitialized bytes in the FIL_PAGE_TYPE field.
@param[in]  page_id         Page id to verify
@param[in]  type            Expected page type
*/
static void verify_page_type(page_id_t page_id, page_type_t type) {
  mtr_t mtr;
  mtr_start(&mtr);
  /* We should not write to redo log before checkpointing is enabled as it risks
  running out of space, and we don't expect to write anything in this mtr.
  It should be read only */
  mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

  const auto *block =
      buf_page_get(page_id, univ_page_size, RW_S_LATCH, UT_LOCATION_HERE, &mtr);

  const auto page_type = fil_page_get_type(block->frame);
  if (page_type != type) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_INVALID_PAGE_TYPE, unsigned{type},
              unsigned{page_type}, ulong{page_id.space()},
              ulong{page_id.page_no()});
  }
  mtr_commit(&mtr);
}

MetadataRecover *recv_recovery_from_checkpoint_finish(bool aborting) {
  /* Make sure that the recv_writer thread is done. This is
  required because it grabs various mutexes and we want to
  ensure that when we enable sync_order_checks there is no
  mutex currently held by any thread. */
  mutex_enter(&recv_sys->writer_mutex);

  /* Restore state. */
  if (recv_sys->is_meb_db) dblwr::g_mode = recv_sys->dblwr_state;

  /* Free the resources of the recovery system */
  recv_recovery_on = false;

  /* By acquiring the mutex we ensure that the recv_writer thread
  won't trigger any more LRU batches. Now wait for currently
  in progress batches to finish. */
  buf_flush_wait_LRU_batch_end();

  mutex_exit(&recv_sys->writer_mutex);

  uint32_t count = 0;

  while (recv_writer_is_active()) {
    ++count;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (count >= 600) {
      ib::info(ER_IB_MSG_738);
      count = 0;
    }
  }

  MetadataRecover *metadata{};

  if (!aborting) {
    std::swap(metadata, recv_sys->metadata_recover);
  }

  recv_sys_free();

  if (!aborting) {
    /* Validate a few system page types that were left uninitialized
    by older versions of MySQL. */
    verify_page_type({IBUF_SPACE_ID, FSP_IBUF_HEADER_PAGE_NO},
                     FIL_PAGE_TYPE_SYS);
    verify_page_type({TRX_SYS_SPACE, FSP_FIRST_RSEG_PAGE_NO},
                     FIL_PAGE_TYPE_SYS);
    verify_page_type({TRX_SYS_SPACE, TRX_SYS_PAGE_NO}, FIL_PAGE_TYPE_TRX_SYS);
    verify_page_type({TRX_SYS_SPACE, FSP_DICT_HDR_PAGE_NO}, FIL_PAGE_TYPE_SYS);
  }

  /* Free up the flush_rbt. */
  buf_flush_free_flush_rbt();

  return metadata;
}

#endif /* !UNIV_HOTBACKUP */

#if defined(UNIV_DEBUG) || defined(UNIV_HOTBACKUP)
/** Return string name of the redo log record type.
@param[in]      type    record log record enum
@return string name of record log record */
const char *get_mlog_string(mlog_id_t type) {
  switch (type) {
    case MLOG_SINGLE_REC_FLAG:
      return "MLOG_SINGLE_REC_FLAG";

    case MLOG_1BYTE:
      return "MLOG_1BYTE";

    case MLOG_2BYTES:
      return "MLOG_2BYTES";

    case MLOG_4BYTES:
      return "MLOG_4BYTES";

    case MLOG_8BYTES:
      return "MLOG_8BYTES";

    case MLOG_REC_INSERT_8027:
      return "MLOG_REC_INSERT_8027";

    case MLOG_REC_CLUST_DELETE_MARK_8027:
      return "MLOG_REC_CLUST_DELETE_MARK_8027";

    case MLOG_REC_SEC_DELETE_MARK:
      return "MLOG_REC_SEC_DELETE_MARK";

    case MLOG_REC_UPDATE_IN_PLACE_8027:
      return "MLOG_REC_UPDATE_IN_PLACE_8027";

    case MLOG_REC_DELETE_8027:
      return "MLOG_REC_DELETE_8027";

    case MLOG_LIST_END_DELETE_8027:
      return "MLOG_LIST_END_DELETE_8027";

    case MLOG_LIST_START_DELETE_8027:
      return "MLOG_LIST_START_DELETE_8027";

    case MLOG_LIST_END_COPY_CREATED_8027:
      return "MLOG_LIST_END_COPY_CREATED_8027";

    case MLOG_PAGE_REORGANIZE_8027:
      return "MLOG_PAGE_REORGANIZE_8027";

    case MLOG_PAGE_CREATE:
      return "MLOG_PAGE_CREATE";

    case MLOG_UNDO_INSERT:
      return "MLOG_UNDO_INSERT";

    case MLOG_UNDO_ERASE_END:
      return "MLOG_UNDO_ERASE_END";

    case MLOG_UNDO_INIT:
      return "MLOG_UNDO_INIT";

    case MLOG_UNDO_HDR_REUSE:
      return "MLOG_UNDO_HDR_REUSE";

    case MLOG_UNDO_HDR_CREATE:
      return "MLOG_UNDO_HDR_CREATE";

    case MLOG_REC_MIN_MARK:
      return "MLOG_REC_MIN_MARK";

    case MLOG_IBUF_BITMAP_INIT:
      return "MLOG_IBUF_BITMAP_INIT";

#ifdef UNIV_LOG_LSN_DEBUG
    case MLOG_LSN:
      return "MLOG_LSN";
#endif /* UNIV_LOG_LSN_DEBUG */

    case MLOG_INIT_FILE_PAGE:
      return "MLOG_INIT_FILE_PAGE";

    case MLOG_WRITE_STRING:
      return "MLOG_WRITE_STRING";

    case MLOG_MULTI_REC_END:
      return "MLOG_MULTI_REC_END";

    case MLOG_DUMMY_RECORD:
      return "MLOG_DUMMY_RECORD";

    case MLOG_FILE_DELETE:
      return "MLOG_FILE_DELETE";

    case MLOG_COMP_REC_MIN_MARK:
      return "MLOG_COMP_REC_MIN_MARK";

    case MLOG_COMP_PAGE_CREATE:
      return "MLOG_COMP_PAGE_CREATE";

    case MLOG_COMP_REC_INSERT_8027:
      return "MLOG_COMP_REC_INSERT_8027";

    case MLOG_COMP_REC_CLUST_DELETE_MARK_8027:
      return "MLOG_COMP_REC_CLUST_DELETE_MARK_8027";

    case MLOG_COMP_REC_SEC_DELETE_MARK:
      return "MLOG_COMP_REC_SEC_DELETE_MARK";

    case MLOG_COMP_REC_UPDATE_IN_PLACE_8027:
      return "MLOG_COMP_REC_UPDATE_IN_PLACE_8027";

    case MLOG_COMP_REC_DELETE_8027:
      return "MLOG_COMP_REC_DELETE_8027";

    case MLOG_COMP_LIST_END_DELETE_8027:
      return "MLOG_COMP_LIST_END_DELETE_8027";

    case MLOG_COMP_LIST_START_DELETE_8027:
      return "MLOG_COMP_LIST_START_DELETE_8027";

    case MLOG_COMP_LIST_END_COPY_CREATED_8027:
      return "MLOG_COMP_LIST_END_COPY_CREATED_8027";

    case MLOG_COMP_PAGE_REORGANIZE_8027:
      return "MLOG_COMP_PAGE_REORGANIZE_8027";

    case MLOG_FILE_CREATE:
      return "MLOG_FILE_CREATE";

    case MLOG_ZIP_WRITE_NODE_PTR:
      return "MLOG_ZIP_WRITE_NODE_PTR";

    case MLOG_ZIP_WRITE_BLOB_PTR:
      return "MLOG_ZIP_WRITE_BLOB_PTR";

    case MLOG_ZIP_WRITE_HEADER:
      return "MLOG_ZIP_WRITE_HEADER";

    case MLOG_ZIP_PAGE_COMPRESS:
      return "MLOG_ZIP_PAGE_COMPRESS";

    case MLOG_ZIP_PAGE_COMPRESS_NO_DATA_8027:
      return "MLOG_ZIP_PAGE_COMPRESS_NO_DATA_8027";

    case MLOG_ZIP_PAGE_REORGANIZE_8027:
      return "MLOG_ZIP_PAGE_REORGANIZE_8027";

    case MLOG_FILE_RENAME:
      return "MLOG_FILE_RENAME";

    case MLOG_FILE_EXTEND:
      return "MLOG_FILE_EXTEND";

    case MLOG_PAGE_CREATE_RTREE:
      return "MLOG_PAGE_CREATE_RTREE";

    case MLOG_COMP_PAGE_CREATE_RTREE:
      return "MLOG_COMP_PAGE_CREATE_RTREE";

    case MLOG_INIT_FILE_PAGE2:
      return "MLOG_INIT_FILE_PAGE2";

    case MLOG_INDEX_LOAD:
      return "MLOG_INDEX_LOAD";

      /* Disabled for WL6378
      case MLOG_TRUNCATE:
              return "MLOG_TRUNCATE";
      */

    case MLOG_TABLE_DYNAMIC_META:
      return "MLOG_TABLE_DYNAMIC_META";

    case MLOG_PAGE_CREATE_SDI:
      return "MLOG_PAGE_CREATE_SDI";

    case MLOG_COMP_PAGE_CREATE_SDI:
      return "MLOG_COMP_PAGE_CREATE_SDI";

    case MLOG_REC_INSERT:
      return "MLOG_REC_INSERT";

    case MLOG_REC_CLUST_DELETE_MARK:
      return "MLOG_REC_CLUST_DELETE_MARK";

    case MLOG_REC_DELETE:
      return "MLOG_REC_DELETE";

    case MLOG_REC_UPDATE_IN_PLACE:
      return "MLOG_REC_UPDATE_IN_PLACE";

    case MLOG_LIST_END_COPY_CREATED:
      return "MLOG_LIST_END_COPY_CREATED";

    case MLOG_PAGE_REORGANIZE:
      return "MLOG_PAGE_REORGANIZE";

    case MLOG_ZIP_PAGE_REORGANIZE:
      return "MLOG_ZIP_PAGE_REORGANIZE";

    case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:
      return "MLOG_ZIP_PAGE_COMPRESS_NO_DATA";

    case MLOG_LIST_END_DELETE:
      return "MLOG_LIST_END_DELETE";

    case MLOG_LIST_START_DELETE:
      return "MLOG_LIST_START_DELETE";

    case MLOG_TEST:
      return "MLOG_TEST";
  }

  assert(0);

  return nullptr;
}
#endif /* UNIV_DEBUG || UNIV_HOTBACKUP */
