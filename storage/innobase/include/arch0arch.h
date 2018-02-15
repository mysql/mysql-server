/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/arch0arch.h
 Common interface for redo log and dirty page archiver system

 *******************************************************/

#ifndef ARCH_ARCH_INCLUDE
#define ARCH_ARCH_INCLUDE

#include "db0err.h"
#include "log0log.h"
#include "univ.i"
#include "ut0mutex.h"
#include "ut0new.h"

#include <list>

/** @name Archive file name prefix and constant length parameters. */
/* @{ */
/** Archive directory prefix */
const char ARCH_DIR[] = "#ib_archive";

/** Archive Log group directory prefix */
const char ARCH_LOG_DIR[] = "log_group_";

/** Archive Page group directory prefix */
const char ARCH_PAGE_DIR[] = "page_group_";

/** Archive log file prefix */
const char ARCH_LOG_FILE[] = "ib_log_";

/** Archive page file prefix */
const char ARCH_PAGE_FILE[] = "ib_page_";

/** Byte length for printing LSN.
Each archive group name is appended with start LSN */
const uint MAX_LSN_DECIMAL_DIGIT = 32;

/** Max string length for archive log file name */
const uint MAX_ARCH_LOG_FILE_NAME_LEN =
    sizeof(ARCH_DIR) + 1 + sizeof(ARCH_LOG_DIR) + MAX_LSN_DECIMAL_DIGIT + 1 +
    sizeof(ARCH_LOG_FILE) + MAX_LSN_DECIMAL_DIGIT + 1;

/** Max string length for archive page file name */
const uint MAX_ARCH_PAGE_FILE_NAME_LEN =
    sizeof(ARCH_DIR) + 1 + sizeof(ARCH_PAGE_DIR) + MAX_LSN_DECIMAL_DIGIT + 1 +
    sizeof(ARCH_PAGE_FILE) + MAX_LSN_DECIMAL_DIGIT + 1;

/** Max string length for archive group directory name */
const uint MAX_ARCH_DIR_NAME_LEN =
    sizeof(ARCH_DIR) + 1 + sizeof(ARCH_PAGE_DIR) + MAX_LSN_DECIMAL_DIGIT + 1;
/* @} */

/** Archiver background thread */
void archiver_thread();

/** Archiver thread event to signal that data is available */
extern os_event_t archiver_thread_event;

/** Global to indicate if archiver thread is active. */
extern bool archiver_is_active;

/** Archiver client state.
Archiver clients request archiving for specific interval using
the start and stop interfaces. During this time the client is
attached to global Archiver system. A client copies archived
data for the interval after calling stop. System keeps the data
till the time client object is destroyed.

@startuml

  state ARCH_CLIENT_STATE_INIT
  state ARCH_CLIENT_STATE_STARTED
  state ARCH_CLIENT_STATE_STOPPED

  [*] -down-> ARCH_CLIENT_STATE_INIT
  ARCH_CLIENT_STATE_INIT -down-> ARCH_CLIENT_STATE_STARTED : Attach and start
archiving ARCH_CLIENT_STATE_STARTED -right-> ARCH_CLIENT_STATE_STOPPED : Stop
archiving ARCH_CLIENT_STATE_STOPPED -down-> [*] : Detach client

@enduml */
enum Arch_Client_State {
  /** Client is initialized */
  ARCH_CLIENT_STATE_INIT = 0,

  /** Archiving started by client */
  ARCH_CLIENT_STATE_STARTED,

  /** Archiving stopped by client */
  ARCH_CLIENT_STATE_STOPPED
};

/** Archiver system state.
Archiver state changes are triggered by client request to start or
stop archiving and system wide events like shutdown fatal error etc.
Following diagram shows the state transfer.

@startuml

  state ARCH_STATE_INIT
  state ARCH_STATE_ACTIVE
  state ARCH_STATE_PREPARE_IDLE
  state ARCH_STATE_IDLE
  state ARCH_STATE_ABORT

  [*] -down-> ARCH_STATE_INIT
  ARCH_STATE_INIT -down-> ARCH_STATE_ACTIVE : Start archiving
  ARCH_STATE_ACTIVE -right-> ARCH_STATE_PREPARE_IDLE : Stop archiving
  ARCH_STATE_PREPARE_IDLE -right-> ARCH_STATE_IDLE : All data archived
  ARCH_STATE_IDLE -down-> ARCH_STATE_ABORT : Shutdown or Fatal Error
  ARCH_STATE_PREPARE_IDLE --> ARCH_STATE_ACTIVE : Resume archiving
  ARCH_STATE_IDLE --> ARCH_STATE_ACTIVE : Start archiving
  ARCH_STATE_ABORT -down-> [*]

@enduml */
enum Arch_State {
  /** Archiver is initialized */
  ARCH_STATE_INIT = 0,

  /** Archiver is active and archiving data */
  ARCH_STATE_ACTIVE,

  /** Archiver is processing last data chunks before idle state */
  ARCH_STATE_PREPARE_IDLE,

  /** Archiver is idle */
  ARCH_STATE_IDLE,

  /** Archiver is aborted */
  ARCH_STATE_ABORT
};

/** Archived data block state.
A data block is a block in memory that holds dirty page IDs before persisting
into disk. Shown below is the state transfer diagram for a data block.

@startuml

  state ARCH_BLOCK_INIT
  state ARCH_BLOCK_ACTIVE
  state ARCH_BLOCK_READY_TO_FLUSH
  state ARCH_BLOCK_FLUSHED

  [*] -down-> ARCH_BLOCK_INIT
  ARCH_BLOCK_INIT -> ARCH_BLOCK_ACTIVE : Writing page ID
  ARCH_BLOCK_ACTIVE -> ARCH_BLOCK_READY_TO_FLUSH : Block is full
  ARCH_BLOCK_READY_TO_FLUSH -> ARCH_BLOCK_FLUSHED : Block is flushed
  ARCH_BLOCK_FLUSHED --> ARCH_BLOCK_ACTIVE : Writing page ID
  ARCH_BLOCK_FLUSHED -down-> [*]

@enduml */
enum Arch_Blk_State {
  /** Data block is initialized */
  ARCH_BLOCK_INIT = 0,

  /** Data block is active and having data */
  ARCH_BLOCK_ACTIVE,

  /** Data block is full but not flushed to disk */
  ARCH_BLOCK_READY_TO_FLUSH,

  /** Data block is flushed and can be reused */
  ARCH_BLOCK_FLUSHED
};

/** Archiver block flush type */
enum Arch_Blk_Flush_Type {
  /** Flush when block is full */
  ARCH_FLUSH_NORMAL = 0,

  /** Flush partial block.
  Needed for persistent page tracking. */
  ARCH_FLUSH_PARTIAL
};

/** Initialize Page and Log archiver system
@return error code */
dberr_t arch_init();

/** Free Page and Log archiver system */
void arch_free();

/** Start archiver background thread.
@return true if successful */
bool start_archiver_background();

class Arch_Group;
class Arch_Log_Sys;

/** Archiver file context.
Represents a set of fixed size files within a group */
class Arch_File_Ctx {
 public:
  /** Constructor: Initialize members */
  Arch_File_Ctx()
      : m_name_buf(nullptr),
        m_name_len(),
        m_base_len(),
        m_path_name(nullptr),
        m_dir_name(nullptr),
        m_file_name(nullptr),
        m_index(),
        m_count(),
        m_offset(),
        m_size() {
    m_file.m_file = OS_FILE_CLOSED;
  }

  /** Destructor: Close open file and free resources */
  ~Arch_File_Ctx() {
    if (m_file.m_file != OS_FILE_CLOSED) {
      os_file_close(m_file);
    }

    if (m_name_buf != nullptr) {
      ut_free(m_name_buf);
    }
  }

  /** Initializes archiver file context.
  @param[in]	path		path to the file
  @param[in]	base_dir	directory name prefix
  @param[in]	base_file	file name prefix
  @param[in]	num_files	initial number of files
  @param[in]	file_size	file size in bytes
  @return error code. */
  dberr_t init(const char *path, const char *base_dir, const char *base_file,
               uint num_files, ib_uint64_t file_size);

  /** Open a file at specific index
  @param[in]	read_only	open in read only mode
  @param[in]	start_lsn	start lsn for the group
  @param[in]	file_index	index of the file within the group
  @param[in]	file_offset	start offset
  @return error code. */
  dberr_t open(bool read_only, lsn_t start_lsn, uint file_index,
               ib_uint64_t file_offset);

  /** Add a new file and open
  @param[in]	start_lsn	start lsn for the group
  @param[in]	file_offset	start offset
  @return error code. */
  dberr_t open_new(lsn_t start_lsn, ib_uint64_t file_offset);

  /** Open next file for read
  @param[in]	start_lsn	start lsn for the group
  @param[in]	file_offset	start offset
  @return error code. */
  dberr_t open_next(lsn_t start_lsn, ib_uint64_t file_offset);

  /** Close current file, if open */
  void close() {
    if (m_file.m_file != OS_FILE_CLOSED) {
      os_file_close(m_file);
      m_file.m_file = OS_FILE_CLOSED;
    }
  }

  /** Write data to this file context.
  Data source is another file context or buffer. If buffer is NULL,
  data is copied from input file context. Caller must ensure that
  the size is within the limits of current file for both source and
  destination file context.
  @param[in]	from_file	file context to copy data from
  @param[in]	from_buffer	buffer to copy data or NULL
  @param[in]	size		size of data to copy in bytes
  @return error code */
  dberr_t write(Arch_File_Ctx *from_file, byte *from_buffer, uint size);

  /** Check if current file is closed
  @return true, if file is closed */
  bool is_closed() const { return (m_file.m_file == OS_FILE_CLOSED); }

  /** Get file size
  @return file size in bytes */
  ib_uint64_t get_size() const { return (m_size); }

  /** Get number of files
  @return current file count */
  uint get_file_count() const { return (m_count); }

  /** Check how much is left in current file
  @return length left in bytes */
  ib_uint64_t bytes_left() const {
    ut_ad(m_size >= m_offset);
    return (m_size - m_offset);
  }

  /** Construct file name at specific index
  @param[in]	idx	file index
  @param[in]	dir_lsn	lsn of the group
  @param[out]	buffer	file name including path.
                          The buffer is allocated by caller.
  @param[in]	length	buffer length */
  void build_name(uint idx, lsn_t dir_lsn, char *buffer, uint length);

  /** Construct group directory name
  @param[in]	dir_lsn	lsn of the group
  @param[out]	buffer	directory name.
                          The buffer is allocated by caller.
  @param[in]	length	buffer length */
  void build_dir_name(lsn_t dir_lsn, char *buffer, uint length);

 private:
  /** File name buffer.
  Used if caller doesn't allocate buffer. */
  char *m_name_buf;

  /** File name buffer length */
  uint m_name_len;

  /** Fixed length part of the file.
  Path ended with directory separator. */
  uint m_base_len;

  /** Fixed part of the path to file */
  const char *m_path_name;

  /** Directory name prefix */
  const char *m_dir_name;

  /** File name prefix */
  const char *m_file_name;

  /** Current file descriptor */
  pfs_os_file_t m_file;

  /** File index within the archive group */
  uint m_index;

  /** Current number of files in the archive group */
  uint m_count;

  /** Current file offset */
  ib_uint64_t m_offset;

  /** File size in bytes */
  ib_uint64_t m_size;
};

/** Contiguous archived data for redo log or page tracking.
If there is a gap, that is if archiving is stopped and started, a new
group is created. */
class Arch_Group {
 public:
  /** Constructor: Initialize members
  @param[in]	start_lsn	start LSN for the group
  @param[in]	header_len	length of header for archived files
  @param[in]	mutex		archive system mutex from caller */
  Arch_Group(lsn_t start_lsn, uint header_len, ib_mutex_t *mutex)
      : m_is_active(true),
        m_ref_count(),
        m_dur_ref_count(),
        m_num_active(),
        m_begin_lsn(start_lsn),
        m_end_lsn(LSN_MAX),
        m_header_len(header_len)
#ifdef UNIV_DEBUG
        ,
        m_arch_mutex(mutex)
#endif /* UNIV_DEBUG */
  {
  }

  /** Destructor: Delete all files for non-durable archiving. */
  ~Arch_Group() {
    ut_ad(!m_is_active);
    ut_ad(m_ref_count == 0);

    m_file_ctx.close();

    if (m_dur_ref_count == 0) {
      delete_files();
    }
  }

#ifdef UNIV_DEBUG

  /** Adjust end LSN to end of file. This is used in debug
  mode to test the case when LSN is at file boundary.
  @param[in,out]	stop_lsn	stop lsn for client
  @param[out]	blk_len		last block length */
  void adjust_end_lsn(lsn_t &stop_lsn, uint32_t &blk_len);

  /** Adjust redo copy length to end of file. This is used
  in debug mode to archive only till end of file.
  @param[in,out]	length	data to copy in bytes */
  void adjust_copy_length(uint32_t &length);

#endif /* UNIV_DEBUG */

  /** Initialize the file context for the archive group.
  File context keeps the archived data in files on disk. There
  is one file context for a archive group.
  @param[in]	path		path to the file
  @param[in]	base_dir	directory name prefix
  @param[in]	base_file	file name prefix
  @param[in]	num_files	initial number of files
  @param[in]	file_size	file size in bytes
  @return error code. */
  dberr_t init_file_ctx(const char *path, const char *base_dir,
                        const char *base_file, uint num_files,
                        ib_uint64_t file_size) {
    return (m_file_ctx.init(path, base_dir, base_file, num_files, file_size));
  }

  /** Mark archive group inactive.
  A group is marked inactive by archiver background before entering
  into idle state ARCH_STATE_IDLE.
  @param[in]	end_lsn	lsn where redo archiving is stopped
  @param[in]	end_pos	block number where page tracking is stopped
  @return	number of client references. */
  uint disable(lsn_t end_lsn, ib_uint64_t end_pos) {
    ut_ad(mutex_own(m_arch_mutex));

    m_is_active = false;

    if (end_lsn != LSN_MAX) {
      m_end_lsn = end_lsn;
    }

    return (m_ref_count);
  }

  /** Attach a client to the archive group.
  LSN and block position is not used as of now. It would be needed
  for supporting durable page tracking and reset.
  @param[in]	start_lsn	archive start lsn for the client
  @param[in]	start_pos	start block for page tracking
  @param[in]	is_durable	true, if durable tracking is requested
  @return	number of client references. */
  uint attach(lsn_t start_lsn, ib_uint64_t start_pos, bool is_durable) {
    ut_ad(mutex_own(m_arch_mutex));
    ++m_ref_count;
    ++m_num_active;

    if (is_durable) {
      ++m_dur_ref_count;
    }

    return (m_ref_count);
  }

  /** Detach a client when archiving is stopped by the client.
  The client still has reference to the group so that the group
  is not destroyed when it retrieves the archived data. The
  reference is removed later by #Arch_Group::release.
  @param[in]	stop_lsn	archive stop lsn for client
  @return number of active clients */
  uint detach(lsn_t stop_lsn) {
    ut_ad(m_num_active > 0);
    ut_ad(mutex_own(m_arch_mutex));
    --m_num_active;

    if (m_num_active == 0) {
      m_end_lsn = stop_lsn;
    }

    return (m_num_active);
  }

  /** Release the archive group from a client.
  Reduce the reference count. When all clients release the group,
  the reference count falls down to zero. The function would then
  return zero and the caller can remove the group.
  @param[in]	is_durable	the client needs durable archiving
  @return number of references by other clients. */
  uint release(bool is_durable) {
    ut_ad(m_ref_count > 0);
    ut_ad(mutex_own(m_arch_mutex));
    --m_ref_count;

    if (is_durable) {
      ut_ad(m_dur_ref_count > 0);
      --m_dur_ref_count;
    }

    return (m_ref_count);
  }

  /** Get archived file name at specific index in this group.
  Caller would use it to open and copy data from archived files.
  @param[in]	idx		file index in the group
  @param[out]	name_buf	file name and path. Caller must
                                  allocate the buffer.
  @param[in]	buf_len		allocated buffer length */
  void get_file_name(uint idx, char *name_buf, uint buf_len) {
    ut_ad(name_buf != nullptr);

    /* Build name from the file context. */
    m_file_ctx.build_name(idx, m_begin_lsn, name_buf, buf_len);
  }

  /** Get file size for this group.
  Fixed size files are used for archiving data in a group.
  @return file size in bytes */
  ib_uint64_t get_file_size() const { return (m_file_ctx.get_size()); }

  /** Get start LSN for this group
  @return start LSN */
  lsn_t get_begin_lsn() const { return (m_begin_lsn); }

  /** Check if archiving is going on for this group
  @return true, if the group is active */
  bool is_active() const { return (m_is_active); }

  /** Archive data to one or more files.
  The source is either a file context or buffer. Caller must ensure
  that data is in single file in source file context.
  @param[in]	from_file	file context to copy data from
  @param[in]	from_buffer	buffer to copy data or NULL
  @param[in]	length		size of data to copy in bytes
  @return error code */
  dberr_t write_to_file(Arch_File_Ctx *from_file, byte *from_buffer,
                        uint length);

  /** Disable copy construction */
  Arch_Group(Arch_Group const &) = delete;

  /** Disable assignment */
  Arch_Group &operator=(Arch_Group const &) = delete;

 private:
  /** Delete all files for this archive group */
  void delete_files();

  /** Get the directory name for this archive group.
  It is used for cleaning up the archive directory.
  @param[out]	name_buf	directory name and path. Caller must
                                  allocate the buffer.
  @param[in]	buf_len		buffer length */
  void get_dir_name(char *name_buf, uint buf_len) {
    ut_ad(name_buf != nullptr);
    m_file_ctx.build_dir_name(m_begin_lsn, name_buf, buf_len);
  }

 private:
  /** If the group is active */
  bool m_is_active;

  /** Number of clients referencing the group */
  uint m_ref_count;

  /** Number of clients referencing for durable archiving */
  uint m_dur_ref_count;

  /** Number of clients for which archiving is in progress */
  uint m_num_active;

  /** Start LSN for the archive group */
  lsn_t m_begin_lsn;

  /** End lsn for this archive group */
  lsn_t m_end_lsn;

  /** Header length for the archived files */
  uint m_header_len;

  /** Archive file context */
  Arch_File_Ctx m_file_ctx;

#ifdef UNIV_DEBUG
  /** Mutex protecting concurrent operations by multiple clients.
  This is either the redo log or page archive system mutex. Currently
  used for assert checks. */
  ib_mutex_t *m_arch_mutex;
#endif /* UNIV_DEBUG */
};

/** A list of archive groups */
using Arch_Grp_List = std::list<Arch_Group *, ut_allocator<Arch_Group *>>;

/** An iterator for archive group */
using Arch_Grp_List_Iter = Arch_Grp_List::iterator;

/** Redo log archiving system */
class Arch_Log_Sys {
 public:
  /** Constructor: Initialize members */
  Arch_Log_Sys()
      : m_state(ARCH_STATE_INIT),
        m_archived_lsn(LSN_MAX),
        m_group_list(),
        m_current_group() {
    mutex_create(LATCH_ID_LOG_ARCH, &m_mutex);
  }

  /** Destructor: Free mutex */
  ~Arch_Log_Sys() {
    ut_ad(m_state == ARCH_STATE_INIT || m_state == ARCH_STATE_ABORT);
    ut_ad(m_current_group == nullptr);
    ut_ad(m_group_list.empty());

    mutex_free(&m_mutex);
  }

  /** Check if archiving is in progress.
  In #ARCH_STATE_PREPARE_IDLE state, all clients have already detached
  but archiver background task is yet to finish.
  @return true, if archiving is active */
  bool is_active() {
    return (m_state == ARCH_STATE_ACTIVE || m_state == ARCH_STATE_PREPARE_IDLE);
  }

  /** Check if archiver system is in initial state
  @return true, if redo log archiver state is #ARCH_STATE_INIT */
  bool is_init() { return (m_state == ARCH_STATE_INIT); }

  /** Get LSN up to which redo is archived
  @return last archived redo LSN */
  lsn_t get_archived_lsn() {
    ut_ad(log_writer_mutex_own(*log_sys));
    return (m_archived_lsn);
  }

  /** Get current redo log archive group
  @return current archive group */
  Arch_Group *get_arch_group() { return (m_current_group); }

  /** Start redo log archiving.
  If archiving is already in progress, the client
  is attached to current group.
  @param[out]	group		log archive group
  @param[out]	start_lsn	start lsn for client
  @param[out]	header		redo log header
  @param[in]	is_durable	if client needs durable archiving
  @return error code */
  dberr_t start(Arch_Group *&group, lsn_t &start_lsn, byte *header,
                bool is_durable);

  /** Stop redo log archiving.
  If other clients are there, the client is detached from
  the current group.
  @param[out]	group		log archive group
  @param[out]	stop_lsn	stop lsn for client
  @param[out]	log_blk		redo log trailer block
  @param[in,out]	blk_len		length in bytes
  @return error code */
  dberr_t stop(Arch_Group *group, lsn_t &stop_lsn, byte *log_blk,
               uint32_t &blk_len);

  /** Release the current group from client.
  @param[in]	group		group the client is attached to
  @param[in]	is_durable	if client needs durable archiving */
  void release(Arch_Group *group, bool is_durable);

  /** Archive accumulated redo log in current group.
  This interface is for archiver background task to archive redo log
  data by calling it repeatedly over time.
  @param[in]	init		true, if called for first time
  @param[in]	curr_ctx	system redo logs to copy data from
  @param[out]	arch_lsn	LSN up to which archiving is completed
  @param[out]	wait		true, if no more redo to archive
  @return true, if archiving is aborted */
  bool archive(bool init, Arch_File_Ctx *curr_ctx, lsn_t *arch_lsn, bool *wait);

  /** Acquire redo log archiver mutex.
  It synchronizes concurrent start and stop operations by
  multiple clients. */
  void arch_mutex_enter() { mutex_enter(&m_mutex); }

  /** Release redo log archiver mutex */
  void arch_mutex_exit() { mutex_exit(&m_mutex); }

  /** Disable copy construction */
  Arch_Log_Sys(Arch_Log_Sys const &) = delete;

  /** Disable assignment */
  Arch_Log_Sys &operator=(Arch_Log_Sys const &) = delete;

 private:
  /** Wait for redo log archive up to the target LSN.
  We need to wait till current log sys LSN during archive stop.
  @param[in]	target_lsn	target archive LSN to wait for
  @return error code */
  dberr_t wait_archive_complete(lsn_t target_lsn);

  /** Update checkpoint LSN and related information in redo
  log header block.
  @param[in,out]	header		redo log header buffer
  @param[in]	checkpoint_lsn	checkpoint LSN for recovery */
  void update_header(byte *header, lsn_t checkpoint_lsn);

  /** Check and set log archive system state and output the
  amount of redo log available for archiving.
  @param[in]	is_abort	need to abort
  @param[in,out]	archived_lsn	LSN up to which redo log is archived
  @param[out]	to_archive	amount of redo log to be archived */
  Arch_State check_set_state(bool is_abort, lsn_t *archived_lsn,
                             uint *to_archive);

  /** Copy redo log from file context to archiver files.
  @param[in]	file_ctx	file context for system redo logs
  @param[in]	length		data to copy in bytes
  @return error code */
  dberr_t copy_log(Arch_File_Ctx *file_ctx, uint length);

 private:
  /** Mutex to protect concurrent start, stop operations */
  ib_mutex_t m_mutex;

  /** Archiver system state.
  #m_state is protected by #m_mutex and #log_t::writer_mutex. For changing
  the state both needs to be acquired. For reading, hold any of the two
  mutexes. Same is true for #m_archived_lsn. */
  Arch_State m_state;

  /** System has archived log up to this LSN */
  lsn_t m_archived_lsn;

  /** List of log archive groups */
  Arch_Grp_List m_group_list;

  /** Current archive group */
  Arch_Group *m_current_group;

  /** Chunk size to copy redo data */
  uint m_chunk_size;

  /** System log file number where the archiving started */
  uint m_start_log_index;

  /** System log file offset where the archiving started */
  ib_uint64_t m_start_log_offset;
};

/** Position in page ID archiving system */
struct Arch_Page_Pos {
  /** Initialize a position */
  void init();

  /** Position in the beginning of next block */
  void set_next();

  /** Unique block number */
  ib_uint64_t m_block_num;

  /** Offset within a block */
  uint m_offset;
};

/** In memory data block in Page ID archiving system */
class Arch_Block {
 public:
  /** Constructor: Initialize elements
  @param[in]	blk_buf	buffer for data block
  @param[in]	size	buffer size */
  Arch_Block(byte *blk_buf, uint size)
      : m_state(ARCH_BLOCK_INIT),
        m_number(),
        m_oldest_lsn(LSN_MAX),
        m_start_lsn(LSN_MAX),
        m_data(blk_buf),
        m_data_len(),
        m_size(size) {}

  /** Destructor */
  ~Arch_Block() {}

  /** Set the block ready to begin writing page ID
  @param[in]	pos		position to initiate block number
  @param[in]	start_lsn	start LSN for group last reset */
  void begin_write(Arch_Page_Pos *pos, lsn_t start_lsn);

  /** End writing to a block.
  Change state to #ARCH_BLOCK_READY_TO_FLUSH */
  void end_write();

  /** Wait till the block is flushed and is ready for write
  @return true, if the block is flushed */
  bool wait_flush();

  /** Set current block flushed.
  Must hold page archiver sys operation mutex.  */
  void set_flushed() { m_state = ARCH_BLOCK_FLUSHED; }

  /** Add page ID to current block
  @param[in]	page	page from buffer pool
  @param[in]	pos	Archiver current position
  @return true, if successful
          false, if no more space in current block */
  bool add_page(buf_page_t *page, Arch_Page_Pos *pos);

  /** Copy page Ids from in block at read position
  @param[in]	read_pos	current read position
  @param[in]	read_len	length of data to copy
  @param[out]	read_buff	buffer to copy page IDs.
                                  Caller must allocate the buffer.
  @return true, if successful
          false, if block is already overwritten */
  bool copy_pages(Arch_Page_Pos *read_pos, uint read_len, byte *read_buff);

  /** Flush this block to the file group
  @param[in]	file_group	current archive group
  @param[in]	type		flush type
  @return error code. */
  dberr_t flush(Arch_Group *file_group, Arch_Blk_Flush_Type type);

  /** Get oldest LSN among the pages that are added to this block
  @return oldest LSN in block pages */
  lsn_t get_oldest_lsn() { return (m_oldest_lsn); }

  /** Get current state of the block
  @return block state */
  Arch_Blk_State get_state() { return (m_state); }

 private:
  /** Block state */
  Arch_Blk_State m_state;

  /** Unique block number */
  ib_uint64_t m_number;

  /** Oldest LSN of all the page IDs added to the block */
  lsn_t m_oldest_lsn;

  /** Start LSN or the last reset LSN of the group */
  lsn_t m_start_lsn;

  /** Block data buffer */
  byte *m_data;

  /** Block data length in bytes */
  uint m_data_len;

  /** Total block size in bytes */
  uint m_size;
};

/** Vector of page archive in memory blocks */
using Arch_Block_Vec = std::vector<Arch_Block *, ut_allocator<Arch_Block *>>;

/** Page archiver in memory data */
struct ArchPageData {
  /** Constructor */
  ArchPageData() : m_blocks(), m_block_size(), m_num_blocks(), m_buffer() {}

  /** Allocate buffer and initialize blocks
  @return true, if successful */
  bool init();

  /** Delete blocks and buffer */
  void clean();

  /** Get the block for a position
  @param[in]	pos	position in page archive sys
  @return page archive in memory block */
  Arch_Block *get_block(Arch_Page_Pos *pos);

  /** Vector of data blocks */
  Arch_Block_Vec m_blocks;

  /** Block size in bytes */
  uint m_block_size;

  /** Total number of blocks */
  uint m_num_blocks;

  /** In memory buffer */
  byte *m_buffer;
};

/** Dirty page archive system */
class Arch_Page_Sys {
 public:
  /** Constructor: Initialize elements and create mutex */
  Arch_Page_Sys()
      : m_state(ARCH_STATE_INIT),
        m_group_list(),
        m_last_pos(),
        m_last_lsn(LSN_MAX),
        m_current_group(),
        m_data(),
        m_flush_pos(),
        m_write_pos() {
    mutex_create(LATCH_ID_PAGE_ARCH, &m_mutex);
    mutex_create(LATCH_ID_PAGE_ARCH_OPER, &m_oper_mutex);
  }

  /** Destructor: Free memory buffer and mutexes */
  ~Arch_Page_Sys() {
    ut_ad(m_state == ARCH_STATE_INIT || m_state == ARCH_STATE_ABORT);
    ut_ad(m_current_group == nullptr);
    ut_ad(m_group_list.empty());

    m_data.clean();

    mutex_free(&m_mutex);
    mutex_free(&m_oper_mutex);
  }

  /** Start dirty page ID archiving.
  If archiving is already in progress, the client
  is attached to current group.
  @param[out]	group		page archive group
  @param[out]	start_lsn	start lsn for client
  @param[out]	start_pos	Start position in archived data
  @param[in]	is_durable	if client needs durable archiving
  @return error code */
  dberr_t start(Arch_Group **group, lsn_t *start_lsn, Arch_Page_Pos *start_pos,
                bool is_durable);

  /** Stop dirty page ID archiving.
  If other clients are there, the client is detached from
  the current group.
  @param[out]	group		page archive group
  @param[out]	stop_lsn	stop lsn for client
  @param[out]	stop_pos	stop position in archived data
  @return error code */
  dberr_t stop(Arch_Group *group, lsn_t *stop_lsn, Arch_Page_Pos *stop_pos);

  /** Release the current group from client.
  @param[in]	group		group the client is attached to
  @param[in]	is_durable	if client needs durable archiving */
  void release(Arch_Group *group, bool is_durable);

  /** Check and add page ID to archived data.
  Check for duplicate page.
  @param[in]	bpage		page to track
  @param[in]	track_lsn	LSN when tracking started
  @param[in]	frame_lsn	current LSN of the page
  @param[in]	force		if true, add page ID without check */
  void track_page(buf_page_t *bpage, lsn_t track_lsn, lsn_t frame_lsn,
                  bool force);

  /** Get page IDs from a specific position.
  Caller must ensure that read_len doesn't exceed the block.
  @param[in]	group		archive group
  @param[in]	read_pos	position in archived data
  @param[in]	read_len	amount of data to read
  @param[out]	read_buff	buffer to return the page IDs.
                                  Caller must allocate the buffer. */
  bool get_pages(Arch_Group *group, Arch_Page_Pos *read_pos, uint read_len,
                 byte *read_buff);

  /** Archive dirty page IDs in current group.
  This interface is for archiver background task to flush page archive
  data to disk by calling it repeatedly over time.
  @param[out]	wait	true, if no more data to archive
  @return true, if archiving is aborted */
  bool archive(bool *wait);

  /** Check if archiver system is in initial state
  @return true, if page ID archiver state is #ARCH_STATE_INIT */
  bool is_init() { return (m_state == ARCH_STATE_INIT); }

  /** Acquire dirty page ID archiver mutex.
  It synchronizes concurrent start and stop operations by
  multiple clients. */
  void arch_mutex_enter() { mutex_enter(&m_mutex); }

  /** Release page ID archiver mutex */
  void arch_mutex_exit() { mutex_exit(&m_mutex); }

  /** Acquire dirty page ID archive operation mutex.
  It synchronizes concurrent page ID write to memory buffer. */
  void arch_oper_mutex_enter() { mutex_enter(&m_oper_mutex); }

  /** Release page ID archiver operatiion  mutex */
  void arch_oper_mutex_exit() { mutex_exit(&m_oper_mutex); }

  /** Disable copy construction */
  Arch_Page_Sys(Arch_Page_Sys const &) = delete;

  /** Disable assignment */
  Arch_Page_Sys &operator=(Arch_Page_Sys const &) = delete;

 private:
  /** Wait for archive system to come out of #ARCH_STATE_PREPARE_IDLE.
  If the system is preparing to idle, #start needs to wait
  for it to come to idle state.
  @return true, if successful
          false, if needs to abort */
  bool wait_idle();

  /** Check if the gap from last reset is short.
  If not many page IDs are added till last reset, we avoid
  taking a new reset point
  @return true, if the gap is small. */
  bool is_gap_small();

  /** Enable tracking pages in all buffer pools.
  @param[in]	tracking_lsn	track pages from this LSN */
  void set_tracking_buf_pool(lsn_t tracking_lsn);

  /** Track pages for which IO is already started. */
  void track_initial_pages();

 private:
  /** Mutex protecting concurrent start, stop operations */
  ib_mutex_t m_mutex;

  /** Archiver system state. */
  Arch_State m_state;

  /** List of log archive groups */
  Arch_Grp_List m_group_list;

  /** Position where last client started archiving */
  Arch_Page_Pos m_last_pos;

  /** LSN when last client started archiving */
  lsn_t m_last_lsn;

  /** Mutex protecting concurrent operation on data */
  ib_mutex_t m_oper_mutex;

  /** Current archive group */
  Arch_Group *m_current_group;

  /** In memory data buffer */
  ArchPageData m_data;

  /** Position for start flushing */
  Arch_Page_Pos m_flush_pos;

  /** Position to add new page ID */
  Arch_Page_Pos m_write_pos;
};

/** Redo log archiver system global */
extern Arch_Log_Sys *arch_log_sys;

/** Dirty page ID archiver system global */
extern Arch_Page_Sys *arch_page_sys;

#endif /* ARCH_ARCH_INCLUDE */
