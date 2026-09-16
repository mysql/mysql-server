/*****************************************************************************

Copyright (c) 1997, 2026, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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
#include <ostream>

#include <array>
#include <iomanip>
#include <map>
#include <new>
#include <optional>
#include <span>
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
#include "fil0pages_persistence_interface.h" /* pages_persistence */
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0chkp.h"       /* log_next_checkpoint_header */
#include "log0encryption.h" /* log_read_encryption_info */
#include "log0files_io.h"
#include "log0log.h"
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

#ifdef HAVE_ASAN
#include <sanitizer/asan_interface.h>
#endif

#ifndef UNIV_HOTBACKUP
#include "buf0rea.h"
#include "ddl0ddl.h"
#include "log0handler.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#else /* !UNIV_HOTBACKUP */
#include "../meb/mutex.h"
#endif /* !UNIV_HOTBACKUP */

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

/* It's preferable for performance to read more in single IO, and also to avoid
resizing the buffer too often. If buffer size is less than 2KB, resize. */
static const size_t PARSING_BUF_MINIMUM_SIZE = 2048;

#ifdef UNIV_HOTBACKUP
std::list<std::pair<space_id_t, lsn_t>> index_load_list;

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
#endif /* UNIV_HOTBACKUP */

PSI_memory_key mem_log_recv_space_hash_key;

/** true when recv_init_crash_recovery() has been called. */
bool recv_needed_recovery;

/** true if buf_page_is_corrupted() should check if the log sequence
number (FIL_PAGE_LSN) is in the future.  Initially false, and set by
recv_recovery_from_checkpoint_start(). */
bool recv_lsn_checks_on;

/** The following counter is used to decide when to print info on
log scan */
static ulint recv_scan_print_counter;

/** This many blocks must be left in each Buffer Pool instance to be managed by
the LRU when we scan the log and store the scanned log records in a hashmap
allocated in the Buffer Pool in frames of non-LRU managed blocks. We will use
these free blocks to read in pages when we start applying the log records to the
database. */
size_t recv_n_frames_for_pages_per_pool_instance;

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
const byte *MetadataRecover::parseMetadataLog(table_id_t id, uint64_t version,
                                              const byte *ptr,
                                              const byte *end) {
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

  recv_sys = ut::new_withkey<recv_sys_t>(UT_NEW_THIS_FILE_PSI_KEY);
  ut_a(recv_sys->last_block_first_mtr_boundary == 0);
  mutex_create(LATCH_ID_RECV_SYS, &recv_sys->mutex);
  mutex_create(LATCH_ID_RECV_WRITER, &recv_sys->writer_mutex);

  recv_sys->spaces = nullptr;
}

/** Resize the recovery parsing buffer up to log_buffer_size */
bool recv_sys_resize_buf() {
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
  recv_sys->per_thread_applier.reset();

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

  mutex_free(&recv_sys->mutex);

#ifndef UNIV_HOTBACKUP
  ut_ad(!recv_writer_is_active());
#endif /* !UNIV_HOTBACKUP */
  mutex_free(&recv_sys->writer_mutex);

  ut::delete_(recv_sys);
  recv_sys = nullptr;
}

#ifndef UNIV_HOTBACKUP
/** Reset the state of the recovery system variables. */
void recv_sys_var_init() {
  recv_recovery_on = false;
  recv_needed_recovery = false;
  recv_lsn_checks_on = false;
  recv_scan_print_counter = 0;
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
@param[in]      buffer  buffer containing corrupted data
@param[in]      pos     position in buffer where the error occurred
@param[in]      lsn     last processed lsn before the error occurred */
static void recv_report_corrupt_log(std::span<const byte> buffer, size_t pos,
                                    lsn_t lsn) {
  ib::error(ER_IB_ERR_CORRUPT_LOG_RECORD_FOUND);

  /* We can't do much with corrupted data, but at least a possible
  record type can be fetched since it's guaranteed that `pos` is
  aligned to a record boundary. */
  int type = buffer.empty() ? 0 : buffer[0];

  ib::info(ER_IB_MSG_LOG_TYPE_PARSED_UP_TO, type, (unsigned long long)lsn);

  const size_t limit = 100;
  const size_t before = std::min(pos, limit);
  const size_t after = std::min(buffer.size() - pos, limit);

  std::ostringstream os{};
  ut_print_buf(os, buffer.data() + pos - before, before + after);
  ib::info(ER_IB_MSG_DUMP_BUFFER, before, after, os.str().c_str());

#ifndef UNIV_HOTBACKUP
  ib::warn(ER_IB_WARN_LOG_CORRUPT, FORCE_RECOVERY_MSG);
#endif /* !UNIV_HOTBACKUP */
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

  recv_sys->apply_log_recs = false;
  recv_sys->is_cloned_db = false;

  recv_sys->found_corrupt_log = false;

  recv_max_page_lsn = 0;

  recv_sys->dblwr =
      ut::new_withkey<dblwr::recv::DBLWR>(UT_NEW_THIS_FILE_PSI_KEY);

  recv_sys->metadata_recover =
      ut::new_withkey<MetadataRecover>(UT_NEW_THIS_FILE_PSI_KEY);

  mutex_exit(&recv_sys->mutex);
}

/** Empties the hash table when it has been fully processed. */
static void recv_sys_empty_hash() {
  ut_ad(mutex_own(&recv_sys->mutex));

  if (recv_sys->n_pages_to_recover.value() != 0) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_UNPROCESSED_REDO_LOG_RECORDS,
              recv_sys->n_pages_to_recover.value());
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
@param[in]      page_id        The <Tablespace ID,Page number> pair
@return the redo log entries or nullptr if not found */
static recv_addr_t *recv_get_rec(const page_id_t &page_id) {
  const recv_sys_t::Space *space = recv_get_page_map(page_id.space(), false);

  if (space != nullptr) {
    auto it = space->m_pages.find(page_id.page_no());

    if (it != space->m_pages.end()) {
      return it->second;
    }
  }

  return nullptr;
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

static void one_less_page_to_recover() {
  ut_ad(mutex_own(&recv_sys->mutex));
  recv_sys->n_pages_to_recover.decrement();
}

#ifndef UNIV_HOTBACKUP

/** Reads in pages which have hashed log records, from an area around a given
page number.
@param[in]     requested_page_id
                   The page which has to be read in anyway, so we have an
                   opportunity to read pages nearby.
@param[in]     page_size
                   Size of pages in this page's space */
static void recv_read_in_area(const page_id_t &requested_page_id,
                              [[maybe_unused]] const page_size_t &page_size) {
  const page_no_t low_limit =
      ut_uint64_align_down(requested_page_id.page_no(), RECV_READ_AHEAD_AREA);

  size_t n = 0;

  std::array<page_no_t, RECV_READ_AHEAD_AREA> page_nos;

  for (page_no_t page_no = low_limit;
       page_no < low_limit + RECV_READ_AHEAD_AREA; ++page_no) {
    const page_id_t nearby_page_id(requested_page_id.space(), page_no);
    recv_addr_t *recv_addr = recv_get_rec(nearby_page_id);

    /* TODO: we could check if state is RECV_NOT_PROCESSED if we hadn't released
    the mutex. We could even base our decision entirely on state, without
    looking into BP - at worst, the buf_read_recv_pages => buf_read_page_low =>
    buf_page_init_for_read would detect the page is already in BP and not read
    it. Furthermore, as you can see, reads requested by recv_read_in_area are
    preceded by changing the state to RECV_BEING_READ, so the only way the page
    could be already in BP hashmap, yet still have RECV_NOT_PROCESSED state is
    if some other function requested it to be read in - the only plausible
    reason it could happen is as part of IBUF merge in io completer, or
    during dict_boot().

    This code should be revisited after WL#15372. */
    if (recv_addr != nullptr && !buf_page_peek(nearby_page_id)) {
      mutex_enter(&recv_sys->mutex);

      if (recv_addr->state == RECV_NOT_PROCESSED) {
        recv_addr->state = RECV_BEING_READ;

        page_nos[n++] = page_no;
      } else {
        /* TODO: If we are here then it means we saw the page missing from BP,
        yet somehow now it has a state different than RECV_NOT_PROCESSED.
        What state could it be?

        It can't be RECV_DISCARDED, as we are the only thread which could set
        this state and we do so for all pages from a given space together, and
        if we are here it means the space isn't discarded.

        It can't be RECV_BEING_READ, as we are the only thread which could set
        this state and we do so right before requesting a read through BP, which
        beings by preparing the page for read in BP, so buf_page_peek would find
        it, unless it was already evicted which happens after applying all
        changes, so the state would be already changed to RECV_PROCESSED. But,
        if this page was really requested by us in previous call to
        recv_read_in_area(), then all other pages from the same
        RECV_READ_AHEAD_AREA had to be also requested then, which means none of
        them could be still in RECV_NOT_PROCESSED state, yet somehow we are now
        called for page_id from the same RECV_READ_AHEAD_AREA because we saw
        being RECV_NOT_PROCESSED - a contradiction.

        Another possibility is that it could be RECV_BEING_PROCESSED or
        RECV_PROCESSED if we are racing with some other thread which read the
        page in meanwhile. How could it happen? The only plausible reasons are:
        a) it is a page read in by dict_boot() which was later evicted, or
        b) it is an IBUF page read in as part of io completion for some other
        page. Such pages are either in IBUF_SPACE (0), or at fixed positions in
        other spaces.

        This code should be revisited after WL#15372. */
        ut_ad(recv_addr->state == RECV_PROCESSED ||
              recv_addr->state == RECV_BEING_PROCESSED);

        ut_ad(nearby_page_id.space() == IBUF_SPACE_ID ||
              ibuf_bitmap_page(nearby_page_id, page_size) ||
              nearby_page_id == page_id_t(DICT_HDR_SPACE, DICT_HDR_PAGE_NO));
      }

      mutex_exit(&recv_sys->mutex);
    }
  }

  if (n > 0) {
    /* There are pages that need to be read. Go ahead and read them
    for recovery. */
    buf_read_recv_pages(requested_page_id.space(), page_nos.data(), n);
  }
}

/** Apply the log records to a page
@param[in,out]  recv_addr       Redo log records to apply */
static void recv_apply_log_rec(recv_addr_t *recv_addr) {
  ut_ad(mutex_own(&recv_sys->mutex));
  ut_a(recv_addr->state != RECV_DISCARDED);

  bool found;
  const page_id_t page_id(recv_addr->space, recv_addr->page_no);

  const page_size_t page_size =
      fil_space_get_page_size(recv_addr->space, &found);
  ut_a(found);
  ut_a(!recv_sys->missing_ids.contains(recv_addr->space));
  ut_a(!recv_sys->deleted.contains(recv_addr->space));
  if (recv_addr->state == RECV_NOT_PROCESSED) {
    mutex_exit(&recv_sys->mutex);
    if (buf_page_peek(page_id)) {
      mtr_t mtr;

      mtr_start(&mtr);

      buf_block_t *block;

      block =
          buf_page_get(page_id, page_size, RW_X_LATCH, UT_LOCATION_HERE, &mtr);

      buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);
      /* TODO: when we start parsing a batch there's no page in BP, and we only
      add pages to BP once all deltas for a given batch are already in the
      hashmap. This can happen either during dict_boot() which reads a few pages
      before applying the last batch, or as part of applying a batch (which in
      case of last batch includes reading and applying changes from IBUF, too).
      In any case, if a page is in BP it must have been read after deltas meant
      for it were already added to the hashmap. This means io completer had to
      apply them. Therefore it makes no sense for us to try to apply anything,
      because it is guaranteed to be applied before we could get RW_X_LATCH on
      the block.

      We should simplify the code around here if below assert holds. */
#ifdef UNIV_DEBUG
      mutex_enter(&recv_sys->mutex);
      ut_a(recv_addr->state == RECV_PROCESSED);
      mutex_exit(&recv_sys->mutex);
#endif

      recv_recover_page(false, block);

      mtr_commit(&mtr);

    } else {
      recv_read_in_area(page_id, page_size);
    }

    mutex_enter(&recv_sys->mutex);
  }
}

/** Empties the hash table of stored log records, applying them to appropriate
pages. */
static void recv_apply_hashed_log_recs() {
  ut_ad(!recv_sys->found_corrupt_log);
  mutex_enter(&recv_sys->mutex);
  ut_a(!srv_read_only_mode);

  recv_sys->apply_log_recs = true;

  const auto batch_size = recv_sys->n_pages_to_recover.value();

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

  /* Iterate through all tablespaces which have changes to be applied on their
  pages */
  for (const auto &[space_id, space_info] : *recv_sys->spaces) {
    bool dropped = false;

    if (space_id != TRX_SYS_SPACE) {
      dberr_t err = fil_tablespace_open_for_recovery(space_id);
      if (err == DB_CORRUPTION) {
        /* Page couldn't be recovered from double-write, we cannot proceed
        with recovery. Skip applying redos and abort the startup. */
        mutex_exit(&recv_sys->mutex);
        ib::fatal(UT_LOCATION_HERE, ER_IB_ERR_CORRUPT_TABLESPACE_UNRECOVERABLE,
                  space_id);
      } else if (err != DB_SUCCESS) {
        ut_a_eq(err, DB_FAIL);

        /* Tablespace was dropped. It should not have been scanned unless it
        is an undo space that was under construction. */

        if (fil_tablespace_lookup_for_recovery(space_id)) {
          ut_ad(fsp_is_undo_tablespace(space_id));
        }
        dropped = true;
      }
    }

    /* Apply collected changes to pages which belong to this tablespace */
    for (auto pages : space_info.m_pages) {
      ut_ad(pages.second->space == space_id);

      if (dropped) {
        pages.second->state = RECV_DISCARDED;
        one_less_page_to_recover();
      } else {
        recv_apply_log_rec(pages.second);
      }

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
  ut_a(!srv_read_only_mode);

  /* Wait until all the pages have been processed */
  mutex_exit(&recv_sys->mutex);
  recv_sys->n_pages_to_recover.await_zero();
  mutex_enter(&recv_sys->mutex);
  ut_a_eq(recv_sys->n_pages_to_recover.value(), 0);

  /* Flush all the file pages to disk and invalidate them in the buffer pool */
  log_background_threads_inactive_validate();
  ut_a(recv_sys->flush_end != nullptr);

  mutex_exit(&recv_sys->mutex);

  /* Stop the recv_writer thread from issuing any LRU
  flush batches. */
  mutex_enter(&recv_sys->writer_mutex);

  /* Wait for any currently run batch to end. Note that BUF_FLUSH_LIST could
  only be initiated by us in earlier call, but buf_pool_invalidate() waits for
  all batches to finish, so only BUF_FLUSH_LRU can be running.
  TBD: why is it important to wait for BUF_FLUSH_LRU to finish here? */
  buf_flush_await_no_flushing(nullptr, BUF_FLUSH_LRU);

  os_event_reset(recv_sys->flush_end);

  /* We are about to request BUF_FLUSH_LIST, in hope to write all dirty pages
  back to disc, so that we can then invalidate the BP, before next batch.
  However, buf_flush_page_and_try_neighbors() skips over io-fixed pages, so
  they would be left in BP even if dirty. We awaited for
  recv_sys->n_pages_to_recover to drop to zero, but this happens before io
  completer releases the latch and io-fix from the block.
  Therefore we wait for all read operations to finish here by using a method
  which looks at a counter which is decremented only after io completer
  io-unfixes the block. Also, this is important for the subsequent
  buf_pool_invalidate() that internally uses buf_LRU_scan_and_free_block()
  which has the same issue: skips over io-fixed pages. */
  buf_pool_wait_for_no_pending_io();

  recv_sys->flush_type = BUF_FLUSH_LIST;

  os_event_set(recv_sys->flush_start);

  os_event_wait(recv_sys->flush_end);

  buf_pool_invalidate();

  /* Allow batches from recv_writer thread. */
  mutex_exit(&recv_sys->writer_mutex);

  mutex_enter(&recv_sys->mutex);

  recv_sys->apply_log_recs = false;

  recv_sys_empty_hash();

  mutex_exit(&recv_sys->mutex);

  ib::info(ER_IB_MSG_710);
}

#else /* !UNIV_HOTBACKUP */
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

    one_less_page_to_recover();

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
  {
    const auto data =
        page_size.is_compressed() ? block->page.zip.data : block->frame;
    const dberr_t err = fil_io(IORequest::Type::READ, true, page_id, page_size,
                               page_size.physical(), data, nullptr, false);

    if (err != DB_SUCCESS) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_CANNOT_READ_FROM_TABLESPACE_PAGE,
                ulong{recv_addr->space}, ulong{recv_addr->page_no});
    }

    if (page_size.is_compressed()) {
      if (!buf_zip_decompress(block, true)) {
        ut_error;
      }
    }
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

  {
    const auto data =
        page_size.is_compressed() ? block->page.zip.data : block->frame;
    const dberr_t err = fil_io(IORequest::Type::WRITE, true, page_id, page_size,
                               page_size.physical(), data, nullptr, false);

    if (err != DB_SUCCESS) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_CANNOT_WRITE_TO_TABLESPACE_PAGE,
                ulong{recv_addr->space}, ulong{recv_addr->page_no});
    }
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
functions. This function employs two callback functions that allow redo
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
  const size_t n_hash_cells = recv_sys->n_pages_to_recover.value();
  size_t i = 0;

  recv_sys->apply_log_recs = true;

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
      fprintf(stderr, "%zu ", ((100 * i) / n_hash_cells));
      fflush(stderr);
    }
  }

  /* wait till all the redo log records have been applied */
  (*wait_till_done_function)();

  /* write logs in next line */
  fprintf(stderr, "\n");
  recv_sys->apply_log_recs = false;
  recv_sys_empty_hash();
}

#endif /* !UNIV_HOTBACKUP */

/** Adds a new log record to the hash table of log records.
@param[in]      type            log record type
@param[in]      space_id        Tablespace id
@param[in]      page_no         page number
@param[in]      body            log record body
@param[in]      start_lsn       start lsn of the mtr
@param[in]      end_lsn         end lsn of the mtr */
static void recv_add_to_hash_table(mlog_id_t type, space_id_t space_id,
                                   page_no_t page_no,
                                   std::span<const byte> body, lsn_t start_lsn,
                                   lsn_t end_lsn) {
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
  recv->len = body.size();
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

    recv_sys->n_pages_to_recover.increment();
  }

  UT_LIST_ADD_LAST(recv_addr->rec_list, recv);

  recv_data_t **prev_field;

  prev_field = &recv->data;

  /* Store the log record body in chunks of less than UNIV_PAGE_SIZE:
  the heap grows into the buffer pool, and bigger chunks could not
  be allocated */

  while (!body.empty()) {
    const size_t len = std::min(body.size(), RECV_DATA_BLOCK_SIZE);

    auto *recv_data = static_cast<recv_data_t *>(
        mem_heap_alloc(space->m_heap, sizeof(recv_data_t) + len));

    *prev_field = recv_data;

    memcpy(recv_data + 1, body.data(), len);

    prev_field = &recv_data->next;

    body = body.subspan(len);
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

  recv_addr_t *recv_addr = recv_get_rec(block->page.id);
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

/** @brief Apply tablespace metadata side-effects after page record application.

This function handles tablespace-level side-effects that occur when applying
certain redo log records.

@note This function should be called immediately after redo_applier->apply()

@param record_handle Handle containing the redo log record type and body
@param page_handle   Handle containing the page buffer and metadata after
                     the page modifications have been applied. */
static void apply_tablespace_side_effects(
    const ib::redo::Record_handle &record_handle,
    const ib::redo::Page_handle &page_handle) {
  /* Extract values from handles */
  const uint32_t space_id = page_handle.space_id;
  const uint32_t page_no = page_handle.page_no;
  const page_t *page = page_handle.frame.data();
  /* Since the method is intended to apply tablespace metadata to the page
  page must be provided. */
  ut_a(page != nullptr);

  const std::span<const uint8_t> buffer = record_handle.body;

  /* Only apply side-effects if the FSP header (located in page 0) has been
  modified */
  if (page_no != 0) {
    return;
  }

  auto extract_offset_from_redo_buf =
      [](const std::span<const uint8_t> &buffer) {
        ut_a_le(2, buffer.size());
        return mach_read_from_2(buffer.data());
      };
  fil_space_t *space = fil_space_acquire(space_id);
  ut_ad(space != nullptr);

  /* The function handles side-effects for:
   - MLOG_4BYTES: FSP_SPACE_FLAGS updates, tablespace size changes,
                  free space limit and length updates
   - MLOG_1BYTE: Encryption operation progress updates */
  switch (record_handle.type) {
    case MLOG_4BYTES: {
      /* Parse the offset from the record */
      const ulint offs = extract_offset_from_redo_buf(buffer);

      ut_a_le(offs + 4, page_handle.frame.size());
      const uint32_t val = mach_read_from_4(page + offs);
      /* Handle FSP header fields */
      switch (offs) {
        case FSP_HEADER_OFFSET + FSP_SPACE_FLAGS: {
          /* Most FSP flags can only be changed by CREATE or ALTER with
          ALGORITHM=COPY, so they do not change once the file
          is created. The SDI flag is the only one that can be
          changed by a recoverable transaction. So if there is
          change in FSP flags, update the in-memory space structure
          (fil_space_t) */
          fil_space_set_flags(space, val);
          break;
        }

        case FSP_HEADER_OFFSET + FSP_SIZE: {
          /* Update cached header value, re-extend if needed */
          space->set_cached_fsp_size_in_header(val);

          if (val > space->m_size_in_pages) {
            ib::info(ER_IB_MSG_718, ulong{space->id}, space->name, ulong{val});
            if (!fil_space_extend(space, val)) {
              ib::error(ER_IB_MSG_719, ulong{space->id}, space->name,
                        ulong{val});
            }
          }
          break;
        }

        case FSP_HEADER_OFFSET + FSP_FREE_LIMIT: {
          space->set_cached_fsp_free_limit(val);
          break;
        }

        case FSP_HEADER_OFFSET + FSP_FREE + FLST_LEN: {
          space->set_cached_fsp_free_len(val);
          ut_ad_eq(val, flst_get_len(page + offs));
          break;
        }

        default: {
          break;
        }
      }
      break;
    }

    case MLOG_1BYTE: {
      /* Parse the offset from the record */
      const ulint offs = extract_offset_from_redo_buf(buffer);

      /* If 'ALTER TABLESPACE ... ENCRYPTION' was in progress and page 0 has
      REDO entry for this, now while applying this entry, set
      encryption_op_in_progress flag now so that any other page of this
      tablespace in redo log is written accordingly. */
      ulint offset =
          fsp_header_get_encryption_progress_offset(page_size_t(space->flags));
      if (offs == offset) {
        ut_a_le(offs + 1, page_handle.frame.size());
        byte op = mach_read_from_1(page + offset);
        switch (op) {
          case Encryption::ENCRYPT_IN_PROGRESS:
            space->encryption_op_in_progress = Encryption::Progress::ENCRYPTION;
            break;
          case Encryption::DECRYPT_IN_PROGRESS:
            space->encryption_op_in_progress = Encryption::Progress::DECRYPTION;
            break;
          default:
            space->encryption_op_in_progress = Encryption::Progress::NONE;
            break;
        }
      }

      break;
    }

    default: {
      /* No side-effects for other record types */
      break;
    }
  }
  fil_space_release(space);
}

void recv_recover_page_func(
#ifndef UNIV_HOTBACKUP
    bool just_read_in,
#endif /* !UNIV_HOTBACKUP */
    buf_block_t *block) {
  ut_ad(recv_recovery_is_on());
  mutex_enter(&recv_sys->mutex);

  if (recv_sys->apply_log_recs == false) {
    /* Log records should not be applied now */

    mutex_exit(&recv_sys->mutex);

    return;
  }

  recv_addr_t *recv_addr = recv_get_rec(block->page.id);

  if (recv_addr == nullptr || recv_addr->state == RECV_BEING_PROCESSED ||
      recv_addr->state == RECV_PROCESSED) {
#ifndef UNIV_HOTBACKUP
    ut_ad(recv_addr == nullptr || recv_needed_recovery ||
          recv_sys->scanned_lsn < recv_sys->checkpoint_lsn);
    /*TODO: If recv_addr == nullptr, then it means the reason we've read this
    page must be something other than that it has redo changes to be applied.
    One plausible reason is that it is an IBUF page which we have read in
    order to merge IBUF changes from it to another page which we needed to
    recover. Another is a DICT_HDR_SPACE DICT_HDR_PAGE_NO read in dict_boot().

    If recv_addr is not null, then it means we had some redo logs to be applied.
    Further, it can not be in RECV_BEING_PROCESSED state, because we call
    recv_recover_page_func while holding an X latch on it and the change to
    RECV_BEING_PROCESSED and RECV_PROCESSED happens within this function, so
    it's impossible to see this temporary state.
    Moreover, if the state is indeed already RECV_PROCESSED then it means that
    two different threads tried to read the same page in, and apply changes to
    it. How could that be? At most one thread can do that via recv_read_in_area
    as doing so requires changing state from RECV_NOT_PROCESSED to
    RECV_BEING_READ. Again the only plausible reasons are:
    - this is an IBUF page read as part of io completion for some other page,
    - this is dict_hdr_get() during dict_boot().

    The only reliable way to check if page is from IBUF is ibuf_page_low(..),
    but its contract disallows using it during recovery. Therefore we use a less
    stringent check, that the page id is from IBUF_SPACE_ID or a bitmap page
    from another space. Note that IBUF_SPACE_ID == DICT_HDR_SPACE == 0, and
    contains also other things, this is why this test isn't stringent.

    This should be revisited after WL#15372. */
    ut_ad(recv_recovery_is_on());
    ut_ad(recv_addr == nullptr || recv_addr->state == RECV_PROCESSED);
    ut_ad(block->page.id.space() == IBUF_SPACE_ID ||
          ibuf_bitmap_page(block->page.id, block->page.size) ||
          block->page.id == page_id_t(DICT_HDR_SPACE, DICT_HDR_PAGE_NO));

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

  lsn_t end_lsn = 0;
  lsn_t start_lsn = 0;
  bool modification_to_page = false;

  for (auto recv : recv_addr->rec_list) {
    end_lsn = recv->end_lsn;
#ifndef UNIV_HOTBACKUP
    ut_ad(end_lsn <= recv_sys->scanned_lsn);
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

    if (recv->start_lsn >= page_lsn) {
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

      uint8_t not_null[1];  // apply doesn't handle well a {nullptr,0} span
      ut_a(buf || recv->len == 0);

      ib::redo::Page_handle_wrapper page_handle_wrapper(*block);

      const ib::redo::Record_handle record_handle(
          recv->type, {(buf ? buf : not_null), recv->len});

      const auto success = recv_sys->per_thread_applier->apply(
          record_handle, page_handle_wrapper.handle());
      ut_a(success);
      apply_tablespace_side_effects(record_handle,
                                    page_handle_wrapper.handle());
      page_handle_wrapper.update_block();

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

  if (modification_to_page) {
    /* page lsn must be less than the lsn of a log record here. Otherwise,
    it would mean we are moving the page back in time. Also, indirectly this
    verifies the end_lsn is not 0. */
    ut_a(page_lsn < end_lsn);
#ifdef UNIV_HOTBACKUP
    UT_NOT_USED(start_lsn);
    /* MEB uses this PAGE_LSN to init page for writing in the
    meb_apply_log_record() */
    mach_write_to_8(FIL_PAGE_LSN + page, end_lsn);
#else  /* !UNIV_HOTBACKUP */
    buf_flush_note_modification(block, start_lsn, end_lsn, nullptr);
#endif /* !UNIV_HOTBACKUP */
  }

  /* Make sure that committing mtr does not change the modification
  LSN values of page */
  ut_a(mtr.get_log_mode() == MTR_LOG_NONE);

  mtr_commit(&mtr);

  mutex_enter(&recv_sys->mutex);

  if (recv_max_page_lsn < page_lsn) {
    recv_max_page_lsn = page_lsn;
  }

  recv_addr->state = RECV_PROCESSED;
  one_less_page_to_recover();

  mutex_exit(&recv_sys->mutex);

#ifdef UNIV_HOTBACKUP
  ib::trace_2() << "Applied " << applied_recs << " Skipped " << skipped_recs;
#endif /* UNIV_HOTBACKUP */
}

void recv_track_changes_of_recovered_lsn() {
  log_track_changes_of_recovered_lsn(recv_sys->previous_recovered_lsn,
                                     recv_sys->recovered_lsn,
                                     recv_sys->last_block_first_mtr_boundary);
  recv_sys->previous_recovered_lsn = recv_sys->recovered_lsn;
}

/** Process a table space record.
@param[in]      rec             record to process
@param[in]      start_lsn       start lsn of mtr containing the record
@return true on success, false otherwise.
*/
[[nodiscard]] static bool recv_process_space_record(
    const ib::redo::Record_view &rec, [[maybe_unused]] lsn_t start_lsn) {
  const auto space_id = rec.space().space_id;

#ifdef UNIV_HOTBACKUP
  /* While scanning redo logs during a backup operation a MLOG_INDEX_LOAD
  type redo log record indicates, that a DDL (create index, alter table...)
  is performed with 'algorithm=inplace'. The affected tablespace must be
  re-copied in the backup lock phase. Record it in the index_load_list. */
  if (rec.type() == MLOG_INDEX_LOAD && !recv_recovery_on) {
    index_load_list.emplace_back(space_id, start_lsn);
  }

  /* MEB does not execute file operations. It cares for all files to be at
  their final places when it applies the redo log. The exception is the
  restore of an incremental_with_redo_log_only backup. */
  if (!recv_sys->apply_file_operations) {
    return true;
  }
#endif /* UNIV_HOTBACKUP */

  switch (rec.type()) {
    case MLOG_FILE_CREATE:
      return fil_tablespace_redo_create_wrapper(
          rec.body().data(), rec.body().data() + rec.body().size(), space_id);
    case MLOG_FILE_RENAME:
      return fil_tablespace_redo_rename(rec.body().data(),
                                        rec.body().data() + rec.body().size(),
                                        space_id, false);
    case MLOG_FILE_DELETE:
      return fil_tablespace_redo_delete_wrapper(
          rec.body().data(), rec.body().data() + rec.body().size(), space_id);
    case MLOG_FILE_EXTEND:
      return fil_tablespace_redo_extend_wrapper(
          rec.body().data(), rec.body().data() + rec.body().size(), space_id);
  }

  return true;
}

/** Process a page record.
@param[in]      rec             record to process
@param[in]      start_lsn       start lsn of mtr containing the record
@param[in]      end_lsn         end lsn of mtr containing the record
@return true on success, false otherwise.
*/
[[nodiscard]] static bool recv_process_page_record(
    const ib::redo::Record_view &rec, lsn_t start_lsn, lsn_t end_lsn) {
#ifdef UNIV_HOTBACKUP
  if (!recv_recovery_on) {
    return true;
  }
#else  /* UNIV_HOTBACKUP */
  ut_a(recv_recovery_on);
#endif /* !UNIV_HOTBACKUP */

  const auto type = static_cast<mlog_id_t>(rec.type());
  const auto space_id = rec.page().space_id;
  const auto page_no = rec.page().page_no;

  switch (type) {
    case MLOG_INIT_FILE_PAGE:
    case MLOG_INIT_FILE_PAGE2:
      /* For clone, avoid initializing page-0. Page-0 should already have been
      initialized. This is to avoid erasing encryption information. We cannot
      update encryption information later with redo logged information for
      clone. Please check comments in MLOG_WRITE_STRING. */
      if (recv_sys->is_cloned_db && page_no == 0) {
        return true;
      }
      break;
    case MLOG_WRITE_STRING: {
      const auto *ptr = rec.body().data();
      const auto *end_ptr = rec.body().data() + rec.body().size();
      bool redo_encryption = true;
#ifdef UNIV_HOTBACKUP
      if (!meb_is_space_loaded(space_id)) {
        redo_encryption = false;
      }
#endif /* UNIV_HOTBACKUP */
      /* For encrypted tablespace, we need to get the encryption key
      information before the page 0 is recovered. Otherwise, redo will not
      find the key to decrypt the data pages. */
      if (redo_encryption && page_no == 0 &&
          !fsp_is_system_or_temp_tablespace(space_id)) {
        /* For clone recovery, redo log encryption information is encrypted
        with donor master key and must be ignored. Therefore, skip applying
        encryption information from redo log because this information is
        already updated in header page (i.e. page 0). */
        if (recv_sys->is_cloned_db) {
          return true;
        }
        const auto proceed_to_apply =
            fil_tablespace_redo_encryption(ptr, end_ptr, space_id, start_lsn);
        if (!proceed_to_apply) {
          return false;
        } else if (!proceed_to_apply.value()) {
          return true;
        }
      }
    }
    default:
      break;
  }

#ifndef UNIV_HOTBACKUP
  if (space_id != TRX_SYS_SPACE &&
      !fil_tablespace_lookup_for_recovery(space_id)) {
    return true;
  }
#endif /* !UNIV_HOTBACKUP */

  recv_add_to_hash_table(type, space_id, page_no, rec.body(), start_lsn,
                         end_lsn);
  return true;
}

/** Process an mtr.
@param[in]      mtr             mtr to process
@param[in]      start_lsn       mtr start lsn
@param[in]      end_lsn         mtr end lsn
@return true on success, false otherwise.
*/
[[nodiscard]] static bool process_mtr(const ib::redo::Mtr_view &mtr,
                                      lsn_t start_lsn, lsn_t end_lsn) {
  using Record_view = ib::redo::Record_view;

  for (const auto &rec : mtr.records()) {
    [[maybe_unused]] uint32_t space_id = 0;
    [[maybe_unused]] uint32_t page_no = 0;

    switch (rec.kind()) {
      case Record_view::Kind::Aux:
        if (rec.type() == MLOG_MULTI_REC_END) {
          /* Found the end mark for the records */
          DBUG_PRINT("ib_log",
                     ("scan " LSN_PF ": multi-log end total_len " ULINTPF
                      " n=" ULINTPF,
                      end_lsn, mtr.size(), mtr.records().size()));
          return true;
        }
        break;

      case Record_view::Kind::Table:
        if (rec.type() == MLOG_TABLE_DYNAMIC_META) {
          if (!recv_sys->metadata_recover->parseMetadataLog(
                  rec.table().table_id, rec.table().version, rec.body().data(),
                  rec.body().data() + rec.body().size())) {
            return false;
          }
        }
        break;

      case Record_view::Kind::Space:
        space_id = rec.space().space_id;
        if (!recv_process_space_record(rec, start_lsn)) {
          return false;
        }
        break;

      case Record_view::Kind::Page:
        space_id = rec.page().space_id;
        page_no = rec.page().page_no;
        if (!recv_process_page_record(rec, start_lsn, end_lsn)) {
          return false;
        }
        break;
    }

    DBUG_PRINT("ib_log",
               ("scan " LSN_PF ": %slog rec %s len " ULINTPF " " PAGE_ID_PF,
                start_lsn, (mtr.records().size() > 1 ? "multi-" : ""),
                get_mlog_string((mlog_id_t)rec.type()), rec.size(), space_id,
                page_no));
  }

  return true;
}

/** Parses the buffer starting from the beginning. If the buffer content causes
parse error, it sets the found_corrupt_log flag to true. The function assumes
recovered_lsn is the start_lsn of the mtr.

@param[in]  buffer   The buffer to parse
@return an mtr with parsed log records if parsing was successful,
otherwise an empty container that indicates end of the processing.
End of the processing may happen either due to a parsing error or,
buffer does not start with a full mtr */
[[nodiscard]] static std::optional<ib::redo::Mtr_view> parse_buffer(
    std::span<const byte> buffer) {
  const auto start_lsn = recv_sys->recovered_lsn;

  auto mtr = recv_sys->per_thread_applier->parse_mtr(buffer);
  if (!mtr) {
    switch (mtr.error()) {
      case ib::redo::Parse_error::Corrupted:
        recv_report_corrupt_log(buffer, mtr.error().pos(), start_lsn);
        recv_sys->found_corrupt_log = true;
        break;

      case ib::redo::Parse_error::Incomplete:
        /* This snippet is checked by the innodb.log_mtr_boundary test. */
        DBUG_EXECUTE_IF("mtr_filling_redo_block_recovery", {
          ib::info() << "Last MTR couldn't be parsed successfully."
                     << " Recovered till : " << start_lsn;
        });
        break;
    }

    return {};
  }

  return *mtr;
}

/** mtr buffer is processed if the start_lsn is more than checkpoint_lsn.
The function assumes recovered_lsn is the start_lsn of the mtr.
Processing involves the following two steps :
- Optionally applying all earlier records from the hashmap :
We follow the order that changes to the space has to happen before to the page
because a change which relates to whole space also implicitly applies to
each of its pages. As a consequence, changes to the page which have lsn
smaller than the change to the space are applied before the change to the
space.
- Adding page records to hashmap and, additionally executing some actions
like tablespace file manipulations, updating tablespace flags, dealing
with encryption etc.

@param[in]      mtr         mtr to process
@return true if end of processing */
[[nodiscard]] static bool process_mtr_and_possibly_apply_records(
    ib::redo::Mtr_view mtr) {
  ut_ad(!recv_sys->found_corrupt_log);

  const auto start_lsn = recv_sys->recovered_lsn;

#ifdef UNIV_HOTBACKUP
  lsn_t end_lsn = recv_calc_lsn_on_data_add(start_lsn, mtr.size());
#else
  lsn_t end_lsn = ib::redo::handler->compute_end_lsn(start_lsn, mtr.size());
#endif

  if (end_lsn > recv_sys->scanned_lsn) {
    /* The log record filled a log block, and we require
    that also the next log block should have been scanned in */
    return true;
  }

  if (start_lsn >= recv_sys->checkpoint_lsn) {
    if (!process_mtr(mtr, start_lsn, end_lsn)) {
      return true;
    }
  }

  recv_sys->recovered_lsn = end_lsn;
  recv_sys->recovered_offset += mtr.size();
  recv_track_changes_of_recovered_lsn();

  return false;
}

/** Parse log records from a buffer and stores them to a hash table.
Some (, none or all) of the oldest redo log records from the hash table may be
applied as a side effect, when the hash table overflows or an mtr requires
strict ordering.
@param  mem_limit  hash table threshold. max() value would skip applying
                   the log records
@return DB_SUCCESS if all goes well */
[[nodiscard]] dberr_t recv_parse_and_apply_log_recs(size_t mem_limit) {
  ut_ad(recv_sys->parse_start_lsn != 0);

  std::span<const byte> buffer(recv_sys->buf, recv_sys->len);

  while (recv_sys->recovered_offset < recv_sys->len) {
    auto mtr = parse_buffer(buffer.subspan(recv_sys->recovered_offset));
    if (!mtr) {
      if (recv_sys->found_corrupt_log) {
        return DB_CORRUPTION;
      }
      return DB_SUCCESS;
    }

    if (process_mtr_and_possibly_apply_records(mtr.value())) {
      if (recv_sys->found_corrupt_log) {
        return DB_CORRUPTION;
      }

#ifdef UNIV_HOTBACKUP
      if (recv_sys->found_corrupt_fs) {
        return DB_CORRUPTION;
      }
#endif /* UNIV_HOTBACKUP */

      return DB_SUCCESS;
    }

#ifndef UNIV_HOTBACKUP
    if (recv_heap_used() > mem_limit) {
      recv_apply_hashed_log_recs();
    }
#endif /* !UNIV_HOTBACKUP */
  }

  return DB_SUCCESS;
}

/** Moves the parsing buffer data left to the buffer start. */
#ifndef UNIV_HOTBACKUP
static
#endif
    void
    recv_reset_buffer() {
  ut_memmove(recv_sys->buf, recv_sys->buf + recv_sys->recovered_offset,
             recv_sys->len - recv_sys->recovered_offset);

  recv_sys->len -= recv_sys->recovered_offset;

  recv_sys->recovered_offset = 0;
}

#ifndef UNIV_HOTBACKUP
[[nodiscard]] dberr_t recv_recovery_begin(lsn_t checkpoint_lsn) {
  using ib::redo::Buffer;
  using ib::redo::Handler_interface;
  using ib::redo::Status;

  mutex_enter(&recv_sys->mutex);
  DBUG_PRINT("ib_log",
             ("Starting recovery from checkpoint lsn " LSN_PF, checkpoint_lsn));

  recv_sys->len = 0;
  recv_sys->recovered_offset = 0;
  recv_sys_empty_hash();

  /* Since 8.0, we can start recovery at checkpoint_lsn which points
  to the middle of log record. In such case we first to need to find
  the beginning of the first group of log records, which is at lsn
  greater than the checkpoint_lsn. */
  recv_sys->parse_start_lsn = 0;

  recv_sys->checkpoint_lsn = checkpoint_lsn;
  recv_sys->scanned_lsn = checkpoint_lsn;
  recv_sys->recovered_lsn = checkpoint_lsn;

  /* We have to trust that the first_rec_group in the first block is
  correct as we can't start parsing earlier to check it ourselves. */
  recv_sys->previous_recovered_lsn = checkpoint_lsn;
  recv_sys->last_block_first_mtr_boundary = 0;

  recv_sys->scanned_epoch_no = 0;
  ut_ad(recv_max_page_lsn == 0);

  const auto pages_to_be_kept_free = std::min(
      size_t{buf_pool_get_n_pages()} / 2,
      /* This value should be greater than the number of pages we want
      to apply redo records for concurrently. This should be greater
      than number of concurrent IOs we want to sustain. We should also keep in
      mind that the limit for the deltas hashmap is not strictly enforced and
      this number includes the not-well specified safety margin. */
      size_t{256} * srv_buf_pool_instances);
  const size_t delta_hashmap_max_mem =
      UNIV_PAGE_SIZE * (buf_pool_get_n_pages() - pages_to_be_kept_free);

  if (log_test == nullptr) {
    recv_n_frames_for_pages_per_pool_instance =
        pages_to_be_kept_free / srv_buf_pool_instances;
    /* We need at least 2 pages for IO, to allow a loop in
    `buf_read_recv_pages()` to be able to break. Currently, the Buffer Pool
    chunk, and thus the Buffer Pool instance, will have at least 16 pages (of
    size of 64KB), so half of that, 8, will easily satisfy that, but we
    nevertheless don't assume current implementation and assert the real
    requirements. */
    ut_a_le(2, recv_n_frames_for_pages_per_pool_instance);
    /* We need at least a page for the redo deltas hashmap. */
    ut_a_lt(0, delta_hashmap_max_mem);
    /* Currently the hashmap memory limit is not strictly enforced, and we need
    some not well defined safety margin. Currently the Buffer Pool minimum size
    is no less than 80 pages (of size of 64KB). With at least half of that
    allocated to pages_to_be_kept_free, it should contain enough margin, which
    we approximate to 10 pages. */
    ut_a_lt(10, pages_to_be_kept_free);
    /* Simulated AIO is waken up after placing all requests in a read-ahead
    area, and there should be at least this much pages in BufferPool to
    accommodate them (in the worst case all in one pool instance). As the
    minimum pool instance size is 1 chunk, which is minimum 1MB, this assertion
    should always be true. */
    ut_a_le(RECV_READ_AHEAD_AREA, recv_n_frames_for_pages_per_pool_instance);
  } else {
    recv_n_frames_for_pages_per_pool_instance = 0;
  }

  mutex_exit(&recv_sys->mutex);

  /* checkpoint_lsn can be in middle of an MTR. Thus adjust checkpoint_lsn to
  MTR boundary for first read. */
  recv_sys->parse_start_lsn =
      ib::redo::handler->align_down_to_known_boundary(checkpoint_lsn);

  if (recv_sys->parse_start_lsn == 0) {
    ib::error(ER_IB_MSG_REDO_PARSE_START_NOT_FOUND);
    return DB_ERROR;
  }

  if (recv_sys->parse_start_lsn < recv_sys->checkpoint_lsn) {
    recv_sys->previous_recovered_lsn = recv_sys->parse_start_lsn;
  }
  ut_a(recv_sys->parse_start_lsn > 0);

  recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
  recv_sys->recovered_lsn = recv_sys->parse_start_lsn;
  recv_track_changes_of_recovered_lsn();

  ib::info(ER_IB_MSG_PARSE_START_AND_CHECKPOINT,
           (ulonglong)recv_sys->parse_start_lsn,
           (ulonglong)recv_sys->checkpoint_lsn);

  lsn_t start_lsn = recv_sys->parse_start_lsn;
  size_t log_segments_read_counter = 0;

  /* In following loop, keep reading REDOs from the Redo Log Handler and
  then keep parsing them until we get STREAM_END from the Redo Log Handler. */
  while (1) {
    log_background_threads_inactive_validate();

    DBUG_EXECUTE_IF("simulate_3mb_mtr_recovery", {
      uint saved_len = recv_sys->len;
      recv_sys->len = 3 * 1024 * 1024;
      recv_sys_resize_buf();
      recv_sys->len = saved_len;
    });

    /* Setup the parsing buffer to store raw REDOs */
    ut_ad(recv_sys->buf_len >= recv_sys->len);

    if (recv_sys->buf_len - recv_sys->len < PARSING_BUF_MINIMUM_SIZE) {
      if (!recv_sys_resize_buf()) {
        recv_sys->found_corrupt_log = true;
        ib::fatal(UT_LOCATION_HERE,
                  ER_IB_ERR_NOT_ENOUGH_MEMORY_FOR_PARSE_BUFFER)
            << "Insufficient memory for InnoDB parse buffer; want "
            << recv_sys->buf_len;
      }
    }

    ut_ad(recv_sys->buf_len - recv_sys->len >= PARSING_BUF_MINIMUM_SIZE);
    Buffer read_buf{recv_sys->buf + recv_sys->len,
                    recv_sys->buf_len - recv_sys->len};

#ifdef HAVE_ASAN
    DBUG_EXECUTE_IF("innodb_recover_byte_by_byte", {
      const auto first_byte = read_buf.subspan(0, 1);
      const auto rest = read_buf.subspan(1);
      ASAN_POISON_MEMORY_REGION(rest.data(), rest.size());
      read_buf = first_byte;
    });
#endif /* HAVE_ASAN */

    /* Read next chunk of REDOs */
    const Status read_status = ib::redo::handler->read(start_lsn, read_buf);
    log_segments_read_counter++;

    ut_ad(read_status == Status::SUCCESS || read_status == Status::STREAM_END ||
          read_status == Status::TORN_STREAM_END);

    const size_t read_data_length = read_buf.size();
    ut_ad(read_status == Status::SUCCESS || read_data_length == 0);

    recv_sys->scanned_lsn =
        ib::redo::handler->compute_end_lsn(start_lsn, read_data_length);
    recv_sys->len += read_data_length;
    ut_a(recv_sys->len <= recv_sys->buf_len);
    const bool seen_writes_above_checkpoint =
        (recv_sys->checkpoint_lsn < recv_sys->scanned_lsn) ||
        (recv_sys->checkpoint_lsn == recv_sys->scanned_lsn &&
         read_status == Status::TORN_STREAM_END);
    if (!recv_needed_recovery && seen_writes_above_checkpoint) {
      /* As we've seen in the redo log proofs of writes above checkpoint_lsn
      the system wasn't shut down in a clean way and recovery is necessary. */
      if (srv_read_only_mode) {
        ut_a(srv_force_recovery < SRV_FORCE_NO_LOG_REDO);
        ib::warn(ER_IB_MSG_RECOVERY_SKIPPED_IN_READ_ONLY_MODE);
        ib::error(ER_IB_MSG_RECOVERY_IN_READ_ONLY);
        return DB_ERROR;
      }

      ib::info(ER_IB_MSG_722, ulonglong{recv_sys->scanned_lsn});

      recv_init_crash_recovery();
    }

    if (recv_needed_recovery) {
      ++recv_scan_print_counter;

      if ((recv_scan_print_counter % 80) == 0) {
        ib::info(ER_IB_MSG_725, ulonglong{recv_sys->scanned_lsn});
      }
    }
    /* For backward compatibility, we handle read failures (by breaking out of
    the loop) only after handling "turns out we need to start recovery"
    conditions above, to produce the expected error log messages. */
    if (read_status != Status::SUCCESS) {
      break;
    }

    /* At this point we have raw REDO logs in parsing buffer (recv_sys->buf).
    Parse them, add them into hash table. If the hash table overflows then
    apply the hashed log records. */
    const dberr_t err = recv_parse_and_apply_log_recs(delta_hashmap_max_mem);
    if (err != DB_SUCCESS) {
      ib::info(ER_IB_MSG_725, ulonglong(recv_sys->scanned_lsn))
          << " log_segments_read:" << log_segments_read_counter;
      return err;
    }

#ifdef HAVE_ASAN
    DBUG_EXECUTE_IF("innodb_recover_byte_by_byte", {
      ASAN_UNPOISON_MEMORY_REGION(recv_sys->buf + recv_sys->len,
                                  recv_sys->buf_len - recv_sys->len);
    });
#endif /* HAVE_ASAN */

    /* At this point, following 2 conditions are possible :
    [1] Either parsing buffer is empty, i.e. all REDOs logs have been processed.
    [2] The data read into parsing buffer ends in the middle of an mtr, and its
        prefix wasn't processed by recv_parse_log_recv().

    For case [2], we shift the unparsed REDOs from the end of parsing buffer to
    the beginning. */
    recv_reset_buffer();

    /* Continue reading next chunk */
    start_lsn = recv_sys->scanned_lsn;
  } /* while */

  if (recv_scan_print_counter > 0) {
    ib::info(ER_IB_MSG_725, ulonglong{recv_sys->scanned_lsn});
  }
  if (!recv_sys->found_corrupt_log) {
    ut_a(recv_sys->spaces != nullptr);
    if (srv_read_only_mode) {
      ut_a_eq(recv_sys->n_pages_to_recover.value(), 0);
      ut_a(recv_sys->spaces->empty());
    } else if (log_test == nullptr) {
      recv_apply_hashed_log_recs();
    }
  }

  DBUG_PRINT("ib_log",
             ("Finished recovery at lsn " LSN_PF, recv_sys->recovered_lsn));
  return DB_SUCCESS;
}

/** Initialize crash recovery environment. Can be called iff
recv_needed_recovery == false. */
static void recv_init_crash_recovery() {
  ut_ad(!srv_read_only_mode);
  ut_a(!recv_needed_recovery);

  recv_needed_recovery = true;

  ib::info(ER_IB_MSG_726);
  ib::info(ER_IB_MSG_727);

  if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
    /* Spawn the background thread to flush dirty pages
    from the buffer pools. */

    srv_threads.m_recv_writer =
        os_thread_create(recv_writer_thread_key, 0, recv_writer_thread);

    srv_threads.m_recv_writer.start();
  }
}

dberr_t recv_recovery_from_checkpoint_start(lsn_t flush_lsn) {
  if (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO) {
    ib::info(ER_IB_MSG_728);

    /* We leave redo log not started and this is read-only mode. */
    ut_a(srv_read_only_mode);

    return DB_SUCCESS;
  }

  recv_recovery_on = true;

  ut_a(log_checkpointing != nullptr);
  if (const auto err = log_checkpointing->load_checkpoint_value();
      err != DB_SUCCESS) {
    return err;
  }

  /* Start reading the log from the checkpoint LSN up. */
  const lsn_t checkpoint_lsn = pages_persistence->get_checkpoint_lsn();

  ut_ad(recv_sys->n_pages_to_recover.value() == 0);

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

  if (const auto err = recv_recovery_begin(checkpoint_lsn); err != DB_SUCCESS) {
    return err;
  }

  const auto check_scanned_lsn = recv_sys->scanned_lsn;

  const lsn_t recovered_lsn = recv_sys->recovered_lsn;

  ut_a(recv_needed_recovery || checkpoint_lsn == recovered_lsn);
  ut_a(!srv_read_only_mode || !recv_needed_recovery);

  if (check_scanned_lsn < checkpoint_lsn ||
      check_scanned_lsn < recv_max_page_lsn) {
    ib::error(ER_IB_MSG_737, ulonglong{check_scanned_lsn},
              ulonglong{checkpoint_lsn}, ulonglong{recv_max_page_lsn});
  }
  ut_a(checkpoint_lsn <= recovered_lsn);

  /* If any of the flags were set, then `recv_recovery_begin()` would return
  DB_CORRUPTION and we wouldn't be here. */
  ut_a(!recv_sys->found_corrupt_log);
#ifdef UNIV_HOTBACKUP
  ut_a(!recv_sys->found_corrupt_fs);
#endif

  ut_a(pages_persistence->get_checkpoint_lsn() == checkpoint_lsn);
  ut_a(recv_sys->spaces == nullptr || recv_sys->spaces->empty());

  return DB_SUCCESS;
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

  /* By acquiring the mutex we ensure that the recv_writer thread won't trigger
  any more LRU batches. Now wait for currently in progress batches to finish.
  Note that BUF_FLUSH_LIST batches are awaited to finish before we get here.
  TBD: Why is it important to wait for BUF_FLUSH_LRU to finish here? */
  buf_flush_await_no_flushing(nullptr, BUF_FLUSH_LRU);

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

    case OBSOLETE_MLOG_REC_INSERT_8027:
      return "OBSOLETE_MLOG_REC_INSERT_8027";

    case OBSOLETE_MLOG_REC_CLUST_DELETE_MARK_8027:
      return "OBSOLETE_MLOG_REC_CLUST_DELETE_MARK_8027";

    case MLOG_REC_SEC_DELETE_MARK:
      return "MLOG_REC_SEC_DELETE_MARK";

    case OBSOLETE_MLOG_REC_UPDATE_IN_PLACE_8027:
      return "OBSOLETE_MLOG_REC_UPDATE_IN_PLACE_8027";

    case OBSOLETE_MLOG_REC_DELETE_8027:
      return "OBSOLETE_MLOG_REC_DELETE_8027";

    case OBSOLETE_MLOG_LIST_END_DELETE_8027:
      return "OBSOLETE_MLOG_LIST_END_DELETE_8027";

    case OBSOLETE_MLOG_LIST_START_DELETE_8027:
      return "OBSOLETE_MLOG_LIST_START_DELETE_8027";

    case OBSOLETE_MLOG_LIST_END_COPY_CREATED_8027:
      return "OBSOLETE_MLOG_LIST_END_COPY_CREATED_8027";

    case OBSOLETE_MLOG_PAGE_REORGANIZE_8027:
      return "OBSOLETE_MLOG_PAGE_REORGANIZE_8027";

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

    case OBSOLETE_MLOG_COMP_REC_INSERT_8027:
      return "OBSOLETE_MLOG_COMP_REC_INSERT_8027";

    case OBSOLETE_MLOG_COMP_REC_CLUST_DELETE_MARK_8027:
      return "OBSOLETE_MLOG_COMP_REC_CLUST_DELETE_MARK_8027";

    case OBSOLETE_MLOG_COMP_REC_SEC_DELETE_MARK:
      return "OBSOLETE_MLOG_COMP_REC_SEC_DELETE_MARK";

    case OBSOLETE_MLOG_COMP_REC_UPDATE_IN_PLACE_8027:
      return "OBSOLETE_MLOG_COMP_REC_UPDATE_IN_PLACE_8027";

    case OBSOLETE_MLOG_COMP_REC_DELETE_8027:
      return "OBSOLETE_MLOG_COMP_REC_DELETE_8027";

    case OBSOLETE_MLOG_COMP_LIST_END_DELETE_8027:
      return "OBSOLETE_MLOG_COMP_LIST_END_DELETE_8027";

    case OBSOLETE_MLOG_COMP_LIST_START_DELETE_8027:
      return "OBSOLETE_MLOG_COMP_LIST_START_DELETE_8027";

    case OBSOLETE_MLOG_COMP_LIST_END_COPY_CREATED_8027:
      return "OBSOLETE_MLOG_COMP_LIST_END_COPY_CREATED_8027";

    case OBSOLETE_MLOG_COMP_PAGE_REORGANIZE_8027:
      return "OBSOLETE_MLOG_COMP_PAGE_REORGANIZE_8027";

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

    case OBSOLETE_MLOG_ZIP_PAGE_COMPRESS_NO_DATA_8027:
      return "OBSOLETE_MLOG_ZIP_PAGE_COMPRESS_NO_DATA_8027";

    case OBSOLETE_MLOG_ZIP_PAGE_REORGANIZE_8027:
      return "OBSOLETE_MLOG_ZIP_PAGE_REORGANIZE_8027";

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
