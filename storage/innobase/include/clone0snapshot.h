/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/** @file include/clone0snapshot.h
 Database Physical Snapshot

 *******************************************************/

#ifndef CLONE_SNAPSHOT_INCLUDE
#define CLONE_SNAPSHOT_INCLUDE

#include "univ.i"

#include "arch0log.h"
#include "arch0page.h"
#include "clone0api.h"
#include "clone0desc.h"
#include "clone0monitor.h"
#include "fil0fil.h"
#include "sql/handler.h"

#include <map>
#include <vector>

struct Clone_file_ctx {
  /** File state:
  [CREATED] -------------> [DROPPING] --> [DROPPED] --> [DROPPED_HANDLED]
      |                        ^
      |                        |
       ----> [RENAMING] -> [RENAMED]
                 |             |
                  <------------
  */
  enum class State {
    /* Invalid state. */
    NONE,
    /* File is being dropped. */
    DROPPING,
    /* File is being renamed. */
    RENAMING,
    /* Newly created file or pre-existing before clone. */
    CREATED,
    /* File is renamed during clone. */
    RENAMED,
    /* File is deleted during clone. */
    DROPPED,
    /* File is deleted and chunk information is handled. */
    DROPPED_HANDLED
  };

  /** File extension to use with name. */
  enum class Extension {
    /* No extension. */
    NONE,
    /* Replace extension - clone file to be replaced during recovery. */
    REPLACE,
    /* DDL extension - temporary extension used during rename. */
    DDL
  };

  /** Initialize file state.
  @param[in]    extn    file name extension */
  void init(Extension extn) {
    m_state.store(State::CREATED);
    m_extension = extn;

    m_pin.store(0);
    m_modified_ddl = false;
    m_waiting = 0;

    m_next_state = CLONE_SNAPSHOT_NONE;

    m_meta.init();
  }

  /** Get file name with extension.
  @param[out]   name    file name. */
  void get_file_name(std::string &name) const;

  /** Mark file added by DDL.
  @param[in]    next_state      next snapshot state */
  void set_ddl(Snapshot_State next_state) {
    m_modified_ddl = true;
    m_next_state = next_state;
  }

  /** @return true iff added or modified by ddl in previous state.
  @param[in]    state   current snapshot state */
  bool by_ddl(Snapshot_State state) const {
    return m_modified_ddl && (state <= m_next_state);
  }

  /** Start waiting for DDL */
  void begin_wait() { ++m_waiting; }

  /** Finish waiting for DDL */
  void end_wait() {
    ut_a(m_waiting > 0);
    --m_waiting;
  }

  /** @return true, iff there are waiting clone tasks. */
  bool is_waiting() const { return (m_waiting > 0); }

  /** Pin the file. */
  void pin() { ++m_pin; }

  /** Unpin the file. */
  void unpin() {
    ut_a(m_pin > 0);
    --m_pin;
  }

  /** @return true, iff clone tasks are using the file. */
  bool is_pinned() const { return (m_pin.load() > 0); }

  /** @return true, iff DDL is modifying file. */
  bool modifying() const {
    State state = m_state.load();
    return (state == State::RENAMING || state == State::DROPPING);
  }

  /** @return true, iff DDL is deleting file. */
  bool deleting() const {
    State state = m_state.load();
    return (state == State::DROPPING);
  }

  /** @return true, iff file is already deleted. */
  bool deleted() const {
    State state = m_state.load();
    return (state == State::DROPPED || state == State::DROPPED_HANDLED);
  }

  /** @return true, iff file is already renamed. */
  bool renamed() const {
    State state = m_state.load();
    return (state == State::RENAMED);
  }

  /** @return file metadata. */
  Clone_File_Meta *get_file_meta() { return &m_meta; }

  /** @return file metadata for read. */
  const Clone_File_Meta *get_file_meta_read() const { return &m_meta; }

  /** File metadata state. Modified by DDL commands. Protected by snapshot
  mutex. Atomic operation helps clone to skip mutex when no ddl. */
  std::atomic<State> m_state;

  /** File name extension. */
  Extension m_extension;

 private:
  /** Pin count incremented and decremented by clone tasks to synchronize with
  concurrent DDL. Protected by snapshot mutex. */
  std::atomic<uint32_t> m_pin;

  /** Waiting count incremented and decremented by clone tasks while waiting
  DDL file operation in progress. Protected by snapshot mutex. */
  uint32_t m_waiting;

  /** true, if file created or modified after clone is started. */
  bool m_modified_ddl{false};

  /** Next state when ddl last modified file. */
  Snapshot_State m_next_state{CLONE_SNAPSHOT_DONE};

  /** File metadata. */
  Clone_File_Meta m_meta;
};

/** Vector type for storing clone files */
using Clone_File_Vec = std::vector<Clone_file_ctx *>;

/** Map type for mapping space ID to clone file index */
using Clone_File_Map = std::map<space_id_t, uint>;

/** Page identified by space and page number */
struct Clone_Page {
  /** Tablespace ID */
  uint32_t m_space_id;

  /** Page number within tablespace */
  uint32_t m_page_no;
};

/** Comparator for storing sorted page ID. */
struct Less_Clone_Page {
  /** Less than operator for page ID.
  @param[in]    page1   first page
  @param[in]    page2   second page
  @return true, if page1 is less than page2 */
  inline bool operator()(const Clone_Page &page1,
                         const Clone_Page &page2) const {
    if (page1.m_space_id < page2.m_space_id) {
      return (true);
    }

    if (page1.m_space_id == page2.m_space_id &&
        page1.m_page_no < page2.m_page_no) {
      return (true);
    }
    return (false);
  }
};

/** Vector type for storing clone page IDs */
using Clone_Page_Vec = std::vector<Clone_Page>;

/** Set for storing unique page IDs. */
using Clone_Page_Set = std::set<Clone_Page, Less_Clone_Page>;

/** Clone handle type */
enum Clone_Handle_Type {
  /** Clone Handle for COPY */
  CLONE_HDL_COPY = 1,

  /** Clone Handle for APPLY */
  CLONE_HDL_APPLY
};

/** Default chunk size in power of 2 in unit of pages.
Chunks are reserved by each thread for multi-threaded clone. For 16k page
size, chunk size is 64M. */
const uint SNAPSHOT_DEF_CHUNK_SIZE_POW2 = 12;

/** Default block size in power of 2 in unit of pages.
Data transfer callback is invoked once for each block. This is also
the maximum size of data that would be re-send if clone is stopped
and resumed. For 16k page size, block size is 1M. */
const uint SNAPSHOT_DEF_BLOCK_SIZE_POW2 = 6;

/** Maximum block size in power of 2 in unit of pages.
For 16k page size, maximum block size is 64M. */
const uint SNAPSHOT_MAX_BLOCK_SIZE_POW2 = 12;

/** Dynamic database snapshot: Holds metadata and handle to data */
class Clone_Snapshot {
 public:
  /** RAII style guard for begin & end  of snapshot state transition. */
  class State_transit {
   public:
    /** Constructor to begin state transition.
    @param[in,out]      snapshot        Clone Snapshot
    @param[in]          new_state       State to transit */
    explicit State_transit(Clone_Snapshot *snapshot, Snapshot_State new_state);

    /** Destructor to end state transition. */
    ~State_transit();

    /** @return error code */
    int get_error() const { return m_error; }

    /** Disable copy construction */
    State_transit(State_transit const &) = delete;

    /** Disable assignment */
    State_transit &operator=(State_transit const &) = delete;

   private:
    /** Clone Snapshot */
    Clone_Snapshot *m_snapshot;

    /** Saved error while beginning transition. */
    int m_error;
  };

  /** Construct snapshot
  @param[in]    hdl_type        copy, apply
  @param[in]    clone_type      clone type
  @param[in]    arr_idx         index in global array
  @param[in]    snap_id         unique snapshot ID */
  Clone_Snapshot(Clone_Handle_Type hdl_type, Ha_clone_type clone_type,
                 uint arr_idx, uint64_t snap_id);

  /** Release contexts and free heap */
  ~Clone_Snapshot();

  /** DDL notification before the operation.
  @param[in]    type            type of DDL notification
  @param[in]    space           space ID for the ddl operation
  @param[in]    no_wait         return with error if needs to wait
  @param[in]    check_intr      check for interrupt during wait
  @param[out]   error           mysql error code
  @return true iff clone state change is blocked. */
  bool begin_ddl_state(Clone_notify::Type type, space_id_t space, bool no_wait,
                       bool check_intr, int &error);

  /** DDL notification after the operation.
  @param[in]    type    type of DDL notification
  @param[in]    space   space ID for the ddl operation */
  void end_ddl_state(Clone_notify::Type type, space_id_t space);

  /** Wait for concurrent DDL file operation and pin file.
  @param[in,out]        file_ctx        file context
  @param[out]           handle_delete   if caller needs to handle deleted state
  @return mysql error code. */
  int pin_file(Clone_file_ctx *file_ctx, bool &handle_delete);

  /** Unpin a file.
  @param[in,out]        file_ctx        file context */
  void unpin_file(Clone_file_ctx *file_ctx) { file_ctx->unpin(); }

  /** Check if DDL needs to block clone operation.
  @param[in]    file_ctx        file context
  @return true iff clone operation needs to be blocked. */
  bool blocks_clone(const Clone_file_ctx *file_ctx);

  /** @return estimated bytes on disk */
  uint64_t get_disk_estimate() const { return (m_data_bytes_disk); }

  /** Get unique snapshot identifier
  @return snapshot ID */
  uint64_t get_id() { return (m_snapshot_id); }

  /** Get snapshot index in global array
  @return array index */
  uint get_index() { return (m_snapshot_arr_idx); }

  /** Get performance schema accounting object used to monitor stage
  progress.
  @return PFS stage object */
  Clone_Monitor &get_clone_monitor() { return (m_monitor); }

  /** Get snapshot heap used for allocation during clone.
  @return heap */
  mem_heap_t *lock_heap() {
    mutex_enter(&m_snapshot_mutex);
    return (m_snapshot_heap);
  }

  /* Release snapshot heap */
  void release_heap(mem_heap_t *&heap) {
    heap = nullptr;
    mutex_exit(&m_snapshot_mutex);
  }

  /** Get snapshot state
  @return state */
  Snapshot_State get_state() { return (m_snapshot_state); }

  /** Get the redo file size for the snapshot
  @return redo file size */
  uint64_t get_redo_file_size() { return (m_redo_file_size); }

  /** Get total number of chunks for current state
  @return number of data chunks */
  uint get_num_chunks() { return (m_num_current_chunks); }

  /** Get maximum file length seen till now
  @return file name length */
  size_t get_max_file_name_length() { return (m_max_file_name_len); }

  /** Get maximum buffer size required for clone
  @return maximum dynamic buffer */
  uint get_dyn_buffer_length() {
    uint ret_len = 0;

    if (is_copy() && m_snapshot_type != HA_CLONE_BLOCKING) {
      ret_len = static_cast<uint>(2 * UNIV_PAGE_SIZE);
    }

    return (ret_len);
  }

  using File_Cbk_Func = std::function<int(Clone_file_ctx *)>;

  /** Iterate through all files in current state
  @param[in]    func    callback function
  @return error code */
  int iterate_files(File_Cbk_Func &&func);

  /** Iterate through all data files
  @param[in]    func    callback function
  @return error code */
  int iterate_data_files(File_Cbk_Func &&func);

  /** Iterate through all redo files
  @param[in]    func    callback function
  @return error code */
  int iterate_redo_files(File_Cbk_Func &&func);

  /** Fill state descriptor from snapshot
  @param[in]  do_estimate   estimate data bytes to transfer
  @param[out] state_desc    snapshot state descriptor */
  void get_state_info(bool do_estimate, Clone_Desc_State *state_desc);

  /** Set state information during apply
  @param[in]    state_desc      snapshot state descriptor */
  void set_state_info(Clone_Desc_State *state_desc);

  /** Get next state based on snapshot type
  @return next state */
  Snapshot_State get_next_state();

  /** Try to attach to snapshot
  @param[in]    hdl_type        copy, apply
  @param[in]    pfs_monitor     enable PFS monitoring
  @return true if successfully attached */
  bool attach(Clone_Handle_Type hdl_type, bool pfs_monitor);

  /** Detach from snapshot. */
  void detach();

  /** Set current snapshot aborted state. Used in error cases before exiting
  clone to make sure any DDL notifier exits waiting. */
  void set_abort();

  /** @return true, iff clone has aborted. */
  bool is_aborted() const;

  /** Start transition to new state
  @param[in]    state_desc      descriptor for next state
  @param[in]    new_state       state to move for apply
  @param[in]    temp_buffer     buffer used for collecting page IDs
  @param[in]    temp_buffer_len buffer length
  @param[in]    cbk             alter callback for long wait
  @return error code */
  int change_state(Clone_Desc_State *state_desc, Snapshot_State new_state,
                   byte *temp_buffer, uint temp_buffer_len,
                   Clone_Alert_Func cbk);

  /** Add file metadata entry at destination
  @param[in]    file_meta       file metadata from donor
  @param[in]    data_dir        destination data directory
  @param[in]    desc_create     create if doesn't exist
  @param[out]   desc_exists     descriptor already exists
  @param[out]   file_ctx        if there, set to current file context
  @return error code */
  int get_file_from_desc(const Clone_File_Meta *file_meta, const char *data_dir,
                         bool desc_create, bool &desc_exists,
                         Clone_file_ctx *&file_ctx);

  /** Rename an existing file descriptor.
  @param[in]    file_meta       renamed file metadata from donor
  @param[in]    data_dir        destination data directory
  @param[out]   file_ctx        if there, set to current file context
  @return error code */
  int rename_desc(const Clone_File_Meta *file_meta, const char *data_dir,
                  Clone_file_ctx *&file_ctx);

  /** Fix files renamed with ddl extension. The file name is checked against
  existing file and added to appropriate status file.
  @param[in]            data_dir        destination data directory
  @param[in,out]        file_ctx        Set to correct extension
  @return error code */
  int fix_ddl_extension(const char *data_dir, Clone_file_ctx *file_ctx);

  /** Add file descriptor to file list
  @param[in,out]        file_ctx        current file context
  @param[in]            ddl_create      added by DDL concurrently
  @return true, if it is the last file. */
  bool add_file_from_desc(Clone_file_ctx *&file_ctx, bool ddl_create);

  /** Extract file information from node and add to snapshot
  @param[in]    node    file node
  @param[in]    by_ddl  node is added concurrently by DDL
  @return error code */
  dberr_t add_node(fil_node_t *node, bool by_ddl);

  /** Add page ID to to the set of pages in snapshot
  @param[in]    space_id        page tablespace
  @param[in]    page_num        page number within tablespace
  @return error code */
  int add_page(uint32_t space_id, uint32_t page_num);

  /** Add redo file to snapshot
  @param[in]    file_name       file name
  @param[in]    file_size       file size in bytes
  @param[in]    file_offset     start offset
  @return error code. */
  int add_redo_file(char *file_name, uint64_t file_size, uint64_t file_offset);

  /** Get file metadata by index for current state
  @param[in]    index   file index
  @return file metadata entry */
  Clone_File_Meta *get_file_by_index(uint index);

  /** Get clone file context by index for current state
  @param[in]    index   file index
  @return file context */
  Clone_file_ctx *get_file_ctx_by_index(uint index);

  /** Get clone file context by chunk and block number.
  @param[in]    chunk_num       chunk number
  @param[in]    block_num       block number
  @param[in]    hint_index      hint file index number to start search.
  @return file context */
  Clone_file_ctx *get_file_ctx(uint32_t chunk_num, uint32_t block_num,
                               uint32_t hint_index);

  /** Get next block of data to transfer
  @param[in]    chunk_num       current chunk
  @param[in,out]        block_num       current/next block
  @param[in,out]        file_ctx        current/next block file context
  @param[out]   data_offset     block offset in file
  @param[out]   data_buf        data buffer or NULL if transfer from file
  @param[out]   data_size       size of data in bytes
  @param[out]   file_size       updated file size if extended
  @return error code */
  int get_next_block(uint chunk_num, uint &block_num,
                     const Clone_file_ctx *&file_ctx, uint64_t &data_offset,
                     byte *&data_buf, uint32_t &data_size, uint64_t &file_size);

  /** Update snapshot block size based on caller's buffer size
  @param[in]    buff_size       buffer size for clone transfer */
  void update_block_size(uint buff_size);

  /** @return chunk size in bytes. */
  inline uint32_t get_chunk_size() const {
    return chunk_size() * UNIV_PAGE_SIZE;
  }

  /** @return number of blocks per chunk for different states. */
  uint32_t get_blocks_per_chunk() const;

  /** Check if copy snapshot
  @return true if snapshot is for copy */
  bool is_copy() const { return (m_snapshot_handle_type == CLONE_HDL_COPY); }

  /** Update file size when file is extended during page copy
  @param[in]    file_index      current file index
  @param[in]    file_size       new file size */
  void update_file_size(uint32_t file_index, uint64_t file_size);

  /** Encrypt tablespace key in header page with master key.
  @param[in]            page_size       page size descriptor
  @param[in,out]        page_data       page data to update
  @return true, if successful. */
  bool encrypt_key_in_header(const page_size_t &page_size, byte *page_data);

  /** Encrypt tablespace key in header page with master key.
  @param[in,out]        log_header      page data to update
  @param[in]            header_len      length of log header
  @return true, if successful. */
  bool encrypt_key_in_log_header(byte *log_header, uint32_t header_len);

  /** Decrypt tablespace key in header page with master key.
  @param[in]            file_meta       clone file metadata
  @param[in]            page_size       page size descriptor
  @param[in,out]        page_data       page data to update */
  void decrypt_key_in_header(const Clone_File_Meta *file_meta,
                             const page_size_t &page_size, byte *&page_data);

  /** @return maximum blocks to transfer with file pinned. */
  uint32_t get_max_blocks_pin() const;

  /** Skip all blocks belonging to currently deleted file context.
  @param[in]            chunk_num       current chunk
  @param[in,out]        block_num       current, next block */
  void skip_deleted_blocks(uint32_t chunk_num, uint32_t &block_num);

 private:
  /** Allow DDL file operation after 64 pages. */
  const static uint32_t S_MAX_PAGES_PIN = 64;

  /** Allow DDL file operation after every block (1M data by default) */
  const static uint32_t S_MAX_BLOCKS_PIN = 1;

  /** File name allocation size base. */
  const static size_t S_FILE_NAME_BASE_LEN = 256;

  /** Various wait types related to snapshot state. */
  enum class Wait_type {
    /* DDL- limited wait if clone is waiting for another DDL. */
    STATE_TRANSIT_WAIT,
    /* DDL- Wait till snapshot state transition is over. */
    STATE_TRANSIT,
    /* DDL- Wait till PAGE COPY state is over. */
    STATE_END_PAGE_COPY,
    /* Clone - Wait till there are no blockers for state transition. */
    STATE_BLOCKER,
    /*DDL - Wait till the waiting clone threads are active. This are
    clone threads from last DDL and useful to prevent starvation. */
    DATA_FILE_WAIT,
    /* DDL - Wait till all threads have closed active data files. */
    DATA_FILE_CLOSE,
    /* Clone - Wait till DDL file operation is complete. */
    DDL_FILE_OPERATION
  };

#ifdef UNIV_DEBUG
  /** Debug sync Wait during state transition. */
  void debug_wait_state_transit();
#endif /* UNIV_DEBUG */

  /** Update deleted state of a file if not yet done.
  @param[in,out]        file_ctx        file context
  @return true, if updated state */
  bool update_deleted_state(Clone_file_ctx *file_ctx);

  /** Get clone data file context by chunk number.
  @param[in]    chunk_num       chunk number
  @param[in]    hint_index      hint file index number to start search.
  @return file context */
  Clone_file_ctx *get_data_file_ctx(uint32_t chunk_num, uint32_t hint_index);

  /** Get clone page file context by chunk number and block number.
  @param[in]    chunk_num       chunk number
  @param[in]    block_num       block number
  @return file context */
  Clone_file_ctx *get_page_file_ctx(uint32_t chunk_num, uint32_t block_num);

  /** Get clone redo file context by chunk number.
  @param[in]    chunk_num       chunk number
  @param[in]    hint_index      hint file index number to start search.
  @return file context */
  Clone_file_ctx *get_redo_file_ctx(uint32_t chunk_num, uint32_t hint_index);

  /** Get wait information string based on wait type.
  @param[in]    wait_type       wait type
  @return wait information string. */
  const char *wait_string(Wait_type wait_type) const;

  /** Wait for various operations based on type.
  @param[in]    type            wait type
  @param[in]    ctx             file context when relevant
  @param[in]    no_wait         return with error if needs to wait
  @param[in]    check_intr      check for interrupt during wait
  @return mysql error code. */
  int wait(Wait_type type, const Clone_file_ctx *ctx, bool no_wait,
           bool check_intr);

  /** During wait get relevant message string for logging.
  @param[in]    wait_type       wait type
  @param[out]   info            notification to log while waiting
  @param[out]   error           error message to log on timeout */
  void get_wait_mesg(Wait_type wait_type, std::string &info,
                     std::string &error);

  /** Block clone state transition. Clone must wait.
  @param[in]    type            type of DDL notification
  @param[in]    space           space ID for the ddl operation
  @param[in]    no_wait         return with error if needs to wait
  @param[in]    check_intr      check for interrupt during wait`
  @param[out]   error           mysql error code
  @return true iff clone state change is blocked. */
  bool block_state_change(Clone_notify::Type type, space_id_t space,
                          bool no_wait, bool check_intr, int &error);

  /** Unblock clone state transition. */
  void unblock_state_change();

  /** Get next file state while being modified by ddl.
  @param[in]    type    ddl notification type
  @param[in]    begin   true, if DDL begin notification
                        false, if DDL end notification
  @return target file state. */
  Clone_file_ctx::State get_target_file_state(Clone_notify::Type type,
                                              bool begin);

  /** Handle files for DDL begin notification.
  @param[in]    type            type of DDL notification
  @param[in]    space           space ID for the ddl operation
  @param[in]    no_wait         return with error if needs to wait
  @param[in]    check_intr      check for interrupt during wait
  @return mysql error code */
  int begin_ddl_file(Clone_notify::Type type, space_id_t space, bool no_wait,
                     bool check_intr);

  /** Handle files for DDL end notification.
  @param[in]    type    type of DDL notification
  @param[in]    space   space ID for the ddl operation */
  void end_ddl_file(Clone_notify::Type type, space_id_t space);

  /** Synchronize snapshot with binary log and GTID.
  @param[in]    cbk     alert callback for long wait
  @return error code. */
  int synchronize_binlog_gtid(Clone_Alert_Func cbk);

  /** Make sure that the trx sys page binary log position correctly reflects
  all transactions committed to innodb. It updates binary log position
  in transaction sys page, if required. The caller must ensure that any new
  transaction is committed in order of binary log.
  @return error code. */
  int update_binlog_position();

  /** Wait for already prepared binlog transactions to end.
  @return error code. */
  int wait_for_binlog_prepared_trx();

  /** Wait for a transaction to end.
  @param[in]    thd     current THD
  @param[in]    trx_id  transaction to wait for
  @return error code. */
  int wait_trx_end(THD *thd, trx_id_t trx_id);

  /** Begin state transition before waiting for DDL. */
  void begin_transit_ddl_wait() {
    mutex_own(&m_snapshot_mutex);
    /* Update number of clones to transit to new state. Set this prior to
    waiting for DDLs blocking state transfer. This would help a new DDL to
    find if clone is blocked by other DDL before state transition. */
    m_num_clones_transit = m_num_clones;
  }

  /** Begin state transition.
  @param[in]    new_state       state to transit to */
  void begin_transit(Snapshot_State new_state) {
    mutex_own(&m_snapshot_mutex);
    m_snapshot_next_state = new_state;
    /* Move to next state. This is ok as the snapshot
    mutex is not released till transition is ended, This
    could change later when we ideally should release
    the snapshot mutex during transition. */
    m_snapshot_state = m_snapshot_next_state;
  }

  /** End state transition. */
  void end_transit() {
    mutex_own(&m_snapshot_mutex);
    m_num_clones_transit = 0;
    m_snapshot_next_state = CLONE_SNAPSHOT_NONE;
  }

  /** Check if state transition is in progress
  @return true during state transition */
  bool in_transit_state() const {
    mutex_own(&m_snapshot_mutex);
    return (m_snapshot_next_state != CLONE_SNAPSHOT_NONE);
  }

  /** @return true, if waiting before starting transition. Generally the
  case when some DDL blocks state transition. */
  bool in_transit_wait() const {
    mutex_own(&m_snapshot_mutex);
    return (!in_transit_state() && m_num_clones_transit != 0);
  }

  /** Start redo archiving.
  @return error code */
  int init_redo_archiving();

  /** Initialize snapshot state for file copy
  @param[in]    new_state       state to move for apply
  @return error code */
  int init_file_copy(Snapshot_State new_state);

  /** Initialize disk byte estimate. */
  void init_disk_estimate() {
    /* Initial size is set to the redo file size on disk. */
    IB_mutex_guard latch{&(log_sys->limits_mutex), UT_LOCATION_HERE};
    m_data_bytes_disk = log_sys->m_capacity.current_physical_capacity();
  }

  /** Initialize snapshot state for page copy
  @param[in]    new_state       state to move for apply
  @param[in]    page_buffer     temporary buffer to copy page IDs
  @param[in]    page_buffer_len buffer length
  @return error code */
  int init_page_copy(Snapshot_State new_state, byte *page_buffer,
                     uint page_buffer_len);

  /** Initialize snapshot state for redo copy
  @param[in]    new_state       state to move for apply
  @param[in]    cbk             alert callback for long wait
  @return error code */
  int init_redo_copy(Snapshot_State new_state, Clone_Alert_Func cbk);

  /** Initialize state while applying cloned data
  @param[in]    state_desc      snapshot state descriptor
  @return error code */
  int init_apply_state(Clone_Desc_State *state_desc);

  /** Extend and flush files after copying data
  @param[in]    is_redo if true flush redo, otherwise data
  @return error code */
  int extend_and_flush_files(bool is_redo);

  /** Create file descriptor and add to current file list
  @param[in]    data_dir        destination data directory
  @param[in]    file_meta       file metadata from donor
  @param[in]    is_ddl          if ddl temporary file
  @param[out]   file_ctx        file context
  @return error code */
  int create_desc(const char *data_dir, const Clone_File_Meta *file_meta,
                  bool is_ddl, Clone_file_ctx *&file_ctx);

  /** Get file context for current chunk
  @param[in]    file_vector     clone file vector
  @param[in]    chunk_num       current chunk number
  @param[in]    start_index     index for starting the search
  @return file context */
  Clone_file_ctx *get_file(Clone_File_Vec &file_vector, uint32_t chunk_num,
                           uint32_t start_index);

  /** Get next page from buffer pool
  @param[in]    chunk_num       current chunk
  @param[in,out]        block_num       current, next block
  @param[in,out]        file_ctx        current, next block file context
  @param[out]   data_offset     offset in file
  @param[out]   data_buf        page data
  @param[out]   data_size       page data size
  @param[out]   file_size       updated file size if extended
  @return error code */
  int get_next_page(uint chunk_num, uint &block_num,
                    const Clone_file_ctx *&file_ctx, uint64_t &data_offset,
                    byte *&data_buf, uint32_t &data_size, uint64_t &file_size);

  /** Get page from buffer pool and make ready for write
  @param[in]    page_id         page ID chunk
  @param[in]    page_size       page size descriptor
  @param[in]    file_ctx        clone file context
  @param[out]   page_data       data page
  @param[out]   data_size       page size in bytes
  @return error code */
  int get_page_for_write(const page_id_t &page_id, const page_size_t &page_size,
                         const Clone_file_ctx *file_ctx, byte *&page_data,
                         uint &data_size);

  /* Make page ready for flush by updating LSN anc checksum
  @param[in]            page_size       page size descriptor
  @param[in]            page_lsn        LSN to update the page with
  @param[in,out]        page_data       data page */
  void page_update_for_flush(const page_size_t &page_size, lsn_t page_lsn,
                             byte *&page_data);

  /** Build file metadata entry
  @param[in]    file_name       name of the file
  @param[in]    file_size       file size in bytes
  @param[in]    file_offset     start offset
  @param[in]    num_chunks      total number of chunks in the file
  @return file context */
  Clone_file_ctx *build_file(const char *file_name, uint64_t file_size,
                             uint64_t file_offset, uint &num_chunks);

  /** Allocate and set clone file name.
  @param[in,out]        file_meta       file metadata
  @param[in]            file_name       file name
  @return true iff successful. */
  bool build_file_name(Clone_File_Meta *file_meta, const char *file_name);

  /** Add buffer pool dump file to the file list
  @return error code */
  int add_buf_pool_file();

  /** Add file to snapshot
  @param[in]    name            file name
  @param[in]    size_bytes      file size in bytes
  @param[in]    alloc_bytes     allocation size on disk for sparse file
  @param[in]    node            file node
  @param[in]    by_ddl          node is added concurrently by DDL
  @return error code. */
  int add_file(const char *name, uint64_t size_bytes, uint64_t alloc_bytes,
               fil_node_t *node, bool by_ddl);

  /** Check if file context has been changed by ddl.
  @param[in]    node            tablespace file node
  @param[out]   file_ctx        file context if exists
  @return true iff file is created or modified by DDL. */
  bool file_ctx_changed(const fil_node_t *node, Clone_file_ctx *&file_ctx);

  /** Get chunk size
  @return chunk size in pages */
  inline uint32_t chunk_size() const {
    auto size = static_cast<uint32_t>(ut_2_exp(m_chunk_size_pow2));
    return size;
  }

  /** Get block size for file copy
  @return block size in pages */
  uint32_t block_size() {
    ut_a(m_block_size_pow2 <= SNAPSHOT_MAX_BLOCK_SIZE_POW2);
    auto size = static_cast<uint32_t>(ut_2_exp(m_block_size_pow2));

    return size;
  }

  /** Get number of blocks per chunk for file copy
  @return blocks per chunk */
  inline uint32_t blocks_per_chunk() const {
    ut_a(m_block_size_pow2 <= m_chunk_size_pow2);
    return (1 << (m_chunk_size_pow2 - m_block_size_pow2));
  }

  /** Update system file name from configuration.
  @param[in]            replace         if replacing current data directory
  @param[in]            file_meta       file descriptor
  @param[in,out]        file_name       file name to update
  @return error code */
  int update_sys_file_name(bool replace, const Clone_File_Meta *file_meta,
                           std::string &file_name);

  /** Build file name along with path for cloned data files.
  @param[in]    data_dir    clone data directory
  @param[in]    file_desc   file descriptor
  @param[out]   file_path   built file path if returned 0
  @return error code (0 on success) */
  int build_file_path(const char *data_dir, const Clone_File_Meta *file_desc,
                      std::string &file_path);

  /** Build file context from file path.
  @param[in]    extn            file extension type
  @param[in]    file_meta       file descriptor
  @param[in]    file_path       data file along with path
  @param[out]   file_ctx        created file context
  @return error code */
  int build_file_ctx(Clone_file_ctx::Extension extn,
                     const Clone_File_Meta *file_meta,
                     const std::string &file_path, Clone_file_ctx *&file_ctx);

  /** Check for existing file and if clone extension is needed. This function
  has the side effect to add undo file indexes.
  @param[in]    replace         if data directory is replaced
  @param[in]    undo_file       if undo tablespace file
  @param[in]    redo_file       if redo file
  @param[in]    data_file_index index of file
  @param[in]    data_file       data file name
  @param[out]   extn            file extension needs to be used
  @return error code */
  int handle_existing_file(bool replace, bool undo_file, bool redo_file,
                           uint32_t data_file_index,
                           const std::string &data_file,
                           Clone_file_ctx::Extension &extn);

  /** @return number of data files to transfer. */
  inline size_t num_data_files() const { return m_data_file_vector.size(); }

  /** @return number of redo files to transfer. */
  inline size_t num_redo_files() const { return m_redo_file_vector.size(); }

 private:
  /** @name Snapshot type and ID */

  /** Snapshot handle type */
  Clone_Handle_Type m_snapshot_handle_type;

  /** Clone type */
  Ha_clone_type m_snapshot_type;

  /** Unique snapshot ID */
  uint64_t m_snapshot_id;

  /** Index in global snapshot array */
  uint m_snapshot_arr_idx;

  /** @name Snapshot State  */

  /** Mutex to handle access by concurrent clones */
  mutable ib_mutex_t m_snapshot_mutex;

  /** Number of blockers for state change. Usually DDLs for short duration. */
  uint32_t m_num_blockers;

  /** Set to true only if clone is aborted after error. */
  bool m_aborted;

  /** Number of clones attached to this snapshot */
  uint m_num_clones;

  /** Number of clones in in state transition */
  uint m_num_clones_transit;

  /** Current state */
  Snapshot_State m_snapshot_state;

  /** Next state to move to. Set only during state transfer. */
  Snapshot_State m_snapshot_next_state;

  /** @name Snapshot data block */

  /** Memory allocation heap */
  mem_heap_t *m_snapshot_heap;

  /** Chunk size in power of 2 */
  uint m_chunk_size_pow2;

  /** Block size in power of 2 */
  uint m_block_size_pow2;

  /** Number of chunks in current state */
  uint m_num_current_chunks;

  /** Maximum file name length observed till now. */
  size_t m_max_file_name_len;

  /** @name Snapshot file data */

  /** All data files for transfer */
  Clone_File_Vec m_data_file_vector;

  /** Map space ID to file vector index */
  Clone_File_Map m_data_file_map;

  /** Total number of data chunks */
  uint m_num_data_chunks;

  /** Number of bytes on disk. */
  uint64_t m_data_bytes_disk;

  /** Index into m_data_file_vector for all undo files. */
  std::vector<int> m_undo_file_indexes;

  /** @name Snapshot page data */

  /** Page archiver client */
  Page_Arch_Client_Ctx m_page_ctx;

  /** Set of unique page IDs */
  Clone_Page_Set m_page_set;

  /** Sorted page IDs to transfer */
  Clone_Page_Vec m_page_vector;

  /** Number of pages to transfer */
  uint m_num_pages;

  /** Number of duplicate pages found */
  uint m_num_duplicate_pages;

  /** @name Snapshot redo data */

  /** redo log archiver client */
  Log_Arch_Client_Ctx m_redo_ctx;

  /** All archived redo files to transfer */
  Clone_File_Vec m_redo_file_vector;

  /** Start offset in first redo file */
  uint64_t m_redo_start_offset;

  /** Redo header block */
  byte *m_redo_header;

  /** Redo header size */
  uint m_redo_header_size;

  /** Redo trailer block */
  byte *m_redo_trailer;

  /** Redo trailer size */
  uint m_redo_trailer_size;

  /** Redo trailer block offset */
  uint64_t m_redo_trailer_offset;

  /** Archived redo file size */
  uint64_t m_redo_file_size;

  /** Total number of redo data chunks */
  uint m_num_redo_chunks;

  /** Enable PFS monitoring */
  bool m_enable_pfs;

  /** Performance Schema accounting object to monitor stage progress */
  Clone_Monitor m_monitor;
};

#endif /* CLONE_SNAPSHOT_INCLUDE */
