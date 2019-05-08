/*****************************************************************************

Copyright (c) 2017, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

#include <mysql/components/services/page_track_service.h>
#include "log0log.h"
#include "ut0mutex.h"

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

/** //@} */

/** File name for the durable file which indicates whether a group was made
durable or not. Required to differentiate durable group from group left over by
crash during clone operation. */
constexpr char ARCH_PAGE_GROUP_DURABLE_FILE_NAME[] = "durable";

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

/** Log archiver background thread */
void log_archiver_thread();

/** Archiver thread event to signal that data is available */
extern os_event_t log_archiver_thread_event;

/** Global to indicate if log archiver thread is active. */
extern bool log_archiver_is_active;

/** Memory block size */
constexpr uint ARCH_PAGE_BLK_SIZE = UNIV_PAGE_SIZE_DEF;

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
  ARCH_CLIENT_STATE_INIT -down-> ARCH_CLIENT_STATE_STARTED : Attach and start \
  archiving
  ARCH_CLIENT_STATE_STARTED -right-> ARCH_CLIENT_STATE_STOPPED : Stop \
  archiving
  ARCH_CLIENT_STATE_STOPPED -down-> [*] : Detach client

@enduml */
enum Arch_Client_State {
  /** Client is initialized */
  ARCH_CLIENT_STATE_INIT = 0,

  /** Archiving started by client */
  ARCH_CLIENT_STATE_STARTED,

  /** Archiving stopped by client */
  ARCH_CLIENT_STATE_STOPPED
};

/** Remove files related to page and log archiving.
@param[in]	file_path	path to the file
@param[in]	file_name	name of the file */
void arch_remove_file(const char *file_path, const char *file_name);

/** Remove group directory and the files related to page and log archiving.
@param[in]	dir_path	path to the directory
@param[in]	dir_name	directory name */
void arch_remove_dir(const char *dir_path, const char *dir_name);

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

/** Archiver block type */
enum Arch_Blk_Type {
  /* Block which holds reset information */
  ARCH_RESET_BLOCK = 0,

  /* Block which holds archived page IDs */
  ARCH_DATA_BLOCK
};

/** Archiver block flush type */
enum Arch_Blk_Flush_Type {
  /** Flush when block is full */
  ARCH_FLUSH_NORMAL = 0,

  /** Flush partial block.
  Needed for persistent page tracking. */
  ARCH_FLUSH_PARTIAL
};

/** Page Archive doublewrite buffer block offsets */
enum Arch_Page_Dblwr_Offset {
  /** Archive doublewrite buffer page offset for RESET page. */
  ARCH_PAGE_DBLWR_RESET_PAGE = 0,

  /* Archive doublewrite buffer page offset for FULL FLUSH page. */
  ARCH_PAGE_DBLWR_FULL_FLUSH_PAGE,

  /* Archive doublewrite buffer page offset for PARTIAL FLUSH page. */
  ARCH_PAGE_DBLWR_PARTIAL_FLUSH_PAGE
};

/** Initialize Page and Log archiver system
@return error code */
dberr_t arch_init();

/** Free Page and Log archiver system */
void arch_free();

/** Start log archiver background thread.
@return error code */
int start_log_archiver_background();

/** Start page archiver background thread.
@return error code */
int start_page_archiver_background();

/** Archiver thread event to signal that data is available */
extern os_event_t page_archiver_thread_event;

/** Global to indicate if page archiver thread is active. */
extern bool page_archiver_is_active;

/** Page archiver background thread */
void page_archiver_thread();

/** Forward declarations */
class Arch_Group;
class Arch_Log_Sys;
class Arch_Dblwr_Ctx;
struct Arch_Recv_Group_Info;

/** Position in page ID archiving system */
struct Arch_Page_Pos {
  /** Initialize a position */
  void init();

  /** Position in the beginning of next block */
  void set_next();

  /** Unique block number */
  uint64_t m_block_num;

  /** Offset within a block */
  uint m_offset;

  bool operator<(Arch_Page_Pos pos) {
    if (m_block_num < pos.m_block_num ||
        (m_block_num == pos.m_block_num && m_offset <= pos.m_offset)) {
      return (true);
    }
    return (false);
  }
};

/** Structure which represents a point in a file. */
struct Arch_Point {
  /** LSN of the point */
  lsn_t lsn{LSN_MAX};

  /** Position of the point */
  Arch_Page_Pos pos;
};

/* Structure which represents a file in a group and its reset points. */
struct Arch_Reset_File {
  /* Initialize the structure. */
  void init();

  /* Index of the file in the group */
  uint m_file_index{0};

  /* LSN of the first reset point in the vector of reset points this
  structure maintains. Treated as the file LSN. */
  lsn_t m_lsn{LSN_MAX};

  /* Vector of reset points which belong to this file */
  std::vector<Arch_Point> m_start_point;
};

/* Structure representing list of archived files. */
using Arch_Reset = std::deque<Arch_Reset_File>;

/** In memory data block in Page ID archiving system */
class Arch_Block {
 public:
  /** Constructor: Initialize elements
  @param[in]	blk_buf	buffer for data block
  @param[in]	size	buffer size
  @param[in]	type	block type */
  Arch_Block(byte *blk_buf, uint size, Arch_Blk_Type type)
      : m_data(blk_buf), m_size(size), m_type(type) {}

  /** Do a deep copy of the members of the block passed as the parameter.
  @note This member needs to be updated whenever a new data member is added to
  this class. */
  void copy_data(const Arch_Block *block);

  /** Set the block ready to begin writing page ID
  @param[in]	pos		position to initiate block number */
  void begin_write(Arch_Page_Pos pos);

  /** End writing to a block.
  Change state to #ARCH_BLOCK_READY_TO_FLUSH */
  void end_write();

  /** Check if block is initialised or not.
  @return true if it has been initialised, else false  */
  bool is_init() const { return (m_state == ARCH_BLOCK_INIT); }

  bool is_active() const { return (m_state == ARCH_BLOCK_ACTIVE); }
  /** Check if the block can be flushed or not.
  @return true, if the block cannot be flushed */
  bool is_flushable() const { return (m_state != ARCH_BLOCK_READY_TO_FLUSH); }

  /** Set current block flushed.
  Must hold page archiver sys operation mutex.  */
  void set_flushed() { m_state = ARCH_BLOCK_FLUSHED; }

  /** Add page ID to current block
  @param[in]	page	page from buffer pool
  @param[in]	pos	Archiver current position
  @return true, if successful
          false, if no more space in current block */
  bool add_page(buf_page_t *page, Arch_Page_Pos *pos);

  /* Add reset information to the current reset block.
  @param[in]	reset_lsn	reset lsn info
  @param[in]	reset_pos	reset pos info which needs to be added
  to the current reset block */
  void add_reset(lsn_t reset_lsn, Arch_Page_Pos reset_pos);

  /** Copy page Ids from this block at read position to a buffer.
  @param[in]	read_pos	current read position
  @param[in]	read_len	length of data to copy
  @param[out]	read_buff	buffer to copy page IDs.
                                  Caller must allocate the buffer.
  @return true, if successful
          false, if block is already overwritten */
  bool get_data(Arch_Page_Pos *read_pos, uint read_len, byte *read_buff);

  /** Copy page Ids from a buffer to this block.
  @param[in]	read_len	length of data to copy
  @param[in]	read_buff	buffer to copy page IDs from
  @param[in]	read_offset	offset from where to write
  @return true if successful */
  bool set_data(uint read_len, byte *read_buff, uint read_offset);

  /** Flush this block to the file group
  @param[in]	file_group	current archive group
  @param[in]	type		flush type
  @return error code. */
  dberr_t flush(Arch_Group *file_group, Arch_Blk_Flush_Type type);

  /* Update the block header with the given LSN
  @param[in]	stop_lsn	stop LSN to update in the block header
  @param[in]	reset_lsn	reset LSN to update in the blk header */
  void update_block_header(lsn_t stop_lsn, lsn_t reset_lsn);

  void read(Arch_Group *group, uint64_t offset);

  /** Set the data length of the block.
  @param[in]	data_len	data length */
  void set_data_len(uint data_len) { m_data_len = data_len; }

  /** @return data length of the block. */
  uint get_data_len() const { return (m_data_len); }

  /** @return block number of the block. */
  uint64_t get_number() const { return (m_number); }

  /** @return stop lsn */
  lsn_t get_stop_lsn() const { return (m_stop_lsn); }

  /** Get oldest LSN among the pages that are added to this block
  @return oldest LSN in block pages */
  lsn_t get_oldest_lsn() const { return (m_oldest_lsn); }

  /** Get current state of the block
  @return block state */
  Arch_Blk_State get_state() const { return (m_state); }

  /** Check if the block contains only zeroes.
  @param[in]  block   block data
  @return true if block is filled with zeroes. */
  static bool is_zeroes(const byte *block);

  /** Check if the block data is valid.
  @param[in]  block   block to be validated
  @return true if it's a valid block, else false */
  static bool validate(byte *block);

  /** Get file index of the file the block belongs to.
  @return file index */
  static uint get_file_index(uint64_t block_num);

  /** Get block type from the block header.
  @param[in]     block   block from where to get the type
  @return block type */
  static uint get_type(byte *block);

  /** Get block data length from the block header.
  @param[in]     block   block from where to get the data length
  @return block data length */
  static uint get_data_len(byte *block);

  /** Get the stop lsn stored in the block header.
  @param[in]     block   block from where to fetch the stop lsn
  @return stop lsn */
  static lsn_t get_stop_lsn(byte *block);

  /** Get the block number from the block header.
  @param[in]     block   block from where to fetch the block number
  @return block number */
  static uint64_t get_block_number(byte *block);

  /** Get the reset lsn stored in the block header.
  @param[in]     block   block from where to fetch the reset lsn
  @return reset lsn */
  static lsn_t get_reset_lsn(byte *block);

  /** Get the checksum stored in the block header.
  @param[in]     block   block from where to fetch the checksum
  @return checksum */
  static uint32_t get_checksum(byte *block);

  /** Fetch the offset for a block in the archive file.
  @param[in]	block_num	block number
  @param[in]	type		type of block
  @return file offset of the block */
  static uint64_t get_file_offset(uint64_t block_num, Arch_Blk_Type type);

 private:
  /* @note member function copy_data needs to be updated whenever a new data
  member is added to this class. */

  /** Block data buffer */
  byte *m_data;

  /** Block data length in bytes */
  uint m_data_len{};

  /** Total block size in bytes */
  uint m_size;

  /** State of the block. */
  Arch_Blk_State m_state{ARCH_BLOCK_INIT};

  /** Unique block number */
  uint64_t m_number{};

  /** Type of block. */
  Arch_Blk_Type m_type;

  /** Checkpoint lsn at the time the last page ID was added to the
  block. */
  lsn_t m_stop_lsn{LSN_MAX};

  /** Oldest LSN of all the page IDs added to the block since the last
   * checkpoint */
  lsn_t m_oldest_lsn{LSN_MAX};

  /** Start LSN or the last reset LSN of the group */
  lsn_t m_reset_lsn{LSN_MAX};
};

/** Archiver file context.
Represents a set of fixed size files within a group */
class Arch_File_Ctx {
 public:
  /** Constructor: Initialize members */
  Arch_File_Ctx() { m_file.m_file = OS_FILE_CLOSED; }

  /** Destructor: Close open file and free resources */
  ~Arch_File_Ctx() {
    close();

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
               uint num_files, uint64_t file_size);

  /** Open a file at specific index
  @param[in]	read_only	open in read only mode
  @param[in]	start_lsn	start lsn for the group
  @param[in]	file_index	index of the file within the group which needs
  to be opened
  @param[in]	file_offset	start offset
  @return error code. */
  dberr_t open(bool read_only, lsn_t start_lsn, uint file_index,
               uint64_t file_offset);

  /** Add a new file and open
  @param[in]	start_lsn	start lsn for the group
  @param[in]	file_offset	start offset
  @return error code. */
  dberr_t open_new(lsn_t start_lsn, uint64_t file_offset);

  /** Open next file for read
  @param[in]	start_lsn	start lsn for the group
  @param[in]	file_offset	start offset
  @return error code. */
  dberr_t open_next(lsn_t start_lsn, uint64_t file_offset);

  /** Read data from the current file that is open.
  Caller must ensure that the size is within the limits of current file
  context.
  @param[in,out]	to_buffer	read data into this buffer
  @param[in]		offset		file offset from where to read
  @param[in]		size		size of data to read in bytes
  @return error code */
  dberr_t read(byte *to_buffer, const uint offset, const uint size);

  /** Write data to this file context from the given file offset.
  Data source is another file context or buffer. If buffer is NULL, data is
  copied from input file context. Caller must ensure that the size is within
  the limits of current file for both source and destination file context.
  @param[in]	from_file	file context to copy data from
  @param[in]	from_buffer	buffer to copy data or NULL
  @param[in]	offset		file offset from where to write
  @param[in]	size		size of data to copy in bytes
  @return error code */
  dberr_t write(Arch_File_Ctx *from_file, byte *from_buffer, uint offset,
                uint size);

  /** Write data to this file context from the current offset.
  Data source is another file context or buffer. If buffer is NULL, data is
  copied from input file context. Caller must ensure that the size is within
  the limits of current file for both source and destination file context.
  @param[in]	from_file	file context to copy data from
  @param[in]	from_buffer	buffer to copy data or NULL
  @param[in]	size		size of data to copy in bytes
  @return error code */
  dberr_t write(Arch_File_Ctx *from_file, byte *from_buffer, uint size);

  /** Flush file. */
  void flush() {
    if (m_file.m_file != OS_FILE_CLOSED) {
      os_file_flush(m_file);
    }
  }

  /** Close file, if open */
  void close() {
    if (m_file.m_file != OS_FILE_CLOSED) {
      os_file_close(m_file);
      m_file.m_file = OS_FILE_CLOSED;
    }
  }

  /** Check if file is closed
  @return true, if file is closed */
  bool is_closed() const { return (m_file.m_file == OS_FILE_CLOSED); }

  /** Check how much is left in current file
  @return length left in bytes */
  uint64_t bytes_left() const {
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

  /** Get the logical size of a file.
  @return logical file size. */
  uint64_t get_size() const { return (m_size); }

  /* Fetch offset of the file open in this context.
  @return file offset */
  uint get_offset() const { return (m_offset); }

  /** Get number of files
  @return current file count */
  uint get_count() const { return (m_count); }

  /** Get the physical size of a file that is open in this context.
  @return physical file size */
  uint64_t get_phy_size() const {
    ut_ad(m_name_buf != nullptr);
    os_file_size_t file_size = os_file_get_size(m_name_buf);
    return (file_size.m_total_size);
  }

  /** Update stop lsn of a file in the group.
  @param[in]	file_index	file_index the current write_pos belongs to
  @param[in]	stop_lsn	stop point */
  void update_stop_point(uint file_index, lsn_t stop_lsn);

#ifdef UNIV_DEBUG
  /** Print recovery related data.
  @param[in]	file_start_index	file index from where to begin */
  void recovery_reset_print(uint file_start_index);

  /** Check if the information maintained in the memory is the same
  as the information maintained in the files.
  @return true if both sets of information are the same
  @param[in]	group	group whose file is being validated
  @param[in]	file_index	index of the file which is being validated
  @param[in]	start_lsn
  @param[in,out]	reset_count	count of files which has been validated
  @return true if both the sets of information are the same. */
  bool validate(Arch_Group *group, uint file_index, lsn_t start_lsn,
                uint &reset_count);
#endif

  /** Update the reset information in the in-memory structure that we maintain
  for faster access.
  @param[in]	lsn     lsn at the time of reset
  @param[in]	pos     pos at the time of reset
  @retval true if the reset point was saved
  @retval false if the reset point wasn't saved because it was already saved */
  void save_reset_point_in_mem(lsn_t lsn, Arch_Page_Pos pos);

  /** Find the appropriate reset LSN that is less than or equal to the
  given lsn and fetch the reset point.
  @param[in]	check_lsn	LSN to be searched against
  @param[out]	reset_point	reset position of the fetched reset point
  @return true if the search was successful. */
  bool find_reset_point(lsn_t check_lsn, Arch_Point &reset_point);

  /** Find the first stop LSN that is greater than the given LSN and fetch
  the stop point.
  @param[in]	group		the group whose stop_point we're interested in
  @param[in]	check_lsn	LSN to be searched against
  @param[out]	stop_point	stop point
  @param[in]	last_pos	position of the last block in the group;
  m_write_pos if group is active and m_stop_pos if not
  @return true if the search was successful. */
  bool find_stop_point(Arch_Group *group, lsn_t check_lsn,
                       Arch_Point &stop_point, Arch_Page_Pos last_pos);

  /** Delete a single file belonging to the specified file index.
  @param[in]	file_index	file index of the file which needs to be deleted
  @param[in]	begin_lsn	group's start lsn
  @return true if successful, else false. */
  bool delete_file(uint file_index, lsn_t begin_lsn);

  /** Delete all files for this archive group
  @param[in]	begin_lsn	group's start lsn */
  void delete_files(lsn_t begin_lsn);

  /** Purge archived files until the specified purge LSN.
  @param[in]	begin_lsn	start LSN of the group
  @param[in]	end_lsn	end LSN of the group
  @param[in]    purge_lsn   purge LSN until which files needs to be purged
  @return LSN until which purging was successful
  @retval LSN_MAX if there was no purging done. */
  lsn_t purge(lsn_t begin_lsn, lsn_t end_lsn, lsn_t purge_lsn);

  /** Fetch the last reset file and last stop point info during recovery
  @param[out]	reset_file	last reset file to be updated
  @param[out]	stop_lsn	last stop lsn to be updated */
  void recovery_fetch_info(Arch_Reset_File &reset_file, lsn_t &stop_lsn) {
    if (m_reset.size() != 0) {
      reset_file = m_reset.back();
    }

    stop_lsn = get_last_stop_point();
  }

  /** Fetch the status of the page tracking system.
  @param[out]	status	vector of a pair of (ID, bool) where ID is the
  start/stop point and bool is true if the ID is a start point else false */
  void get_status(std::vector<std::pair<lsn_t, bool>> &status) {
    for (auto reset_file : m_reset) {
      for (auto reset_point : reset_file.m_start_point) {
        status.push_back(std::make_pair(reset_point.lsn, true));
      }
    }
  }

  /** @return the stop_point which was stored last */
  lsn_t get_last_stop_point() const {
    if (m_stop_points.size() == 0) {
      return (LSN_MAX);
    }

    return (m_stop_points.back());
  }

  /** Fetch the reset points pertaining to a file.
  @param[in]   file_index      file index of the file from which reset points
  needs to be fetched
  @param[in,out]	reset_pos	Update the reset_pos while fetching the
  reset points
  @return error code. */
  dberr_t fetch_reset_points(uint file_index, Arch_Page_Pos &reset_pos);

  /** Fetch the stop lsn pertaining to a file.
  @param[in]	last_file	true if the file for which the stop point is
  being fetched for is the last file
  @param[in,out]	write_pos	Update the write_pos while fetching the
  stop points
  @return error code. */
  dberr_t fetch_stop_points(bool last_file, Arch_Page_Pos &write_pos);

 private:
#ifdef UNIV_DEBUG
  /** Check if the reset information maintained in the memory is the same
  as the information maintained in the given file.
  @param[in]	file	file descriptor
  @param[in]	file_index	index of the file
  @param[in,out]	reset_count	number of files processed containing
  reset data
  @return true if both sets of information are the same */
  bool validate_reset_block_in_file(pfs_os_file_t file, uint file_index,
                                    uint &reset_count);

  /** Check if the stop LSN maintained in the memory is the same as the
  information maintained in the files.
  @param[in]	group	group whose file is being validated
  @param[in]	file	file descriptor
  @param[in]	file_index	index of the file for which the validation is
  happening
  @return true if both the sets of information are the same. */
  bool validate_stop_point_in_file(Arch_Group *group, pfs_os_file_t file,
                                   uint file_index);
#endif

  /** Fetch reset lsn of a particular reset point pertaining to a file.
  @param[in]   block_num       block number where the reset occured.
  @return reset lsn */
  lsn_t fetch_reset_lsn(uint64_t block_num);

 private:
  /** File name buffer.
  Used if caller doesn't allocate buffer. */
  char *m_name_buf{nullptr};

  /** File name buffer length */
  uint m_name_len{};

  /** Fixed length part of the file.
  Path ended with directory separator. */
  uint m_base_len{};

  /** Fixed part of the path to file */
  const char *m_path_name{nullptr};

  /** Directory name prefix */
  const char *m_dir_name{nullptr};

  /** File name prefix */
  const char *m_file_name{nullptr};

  /** Current file descriptor */
  pfs_os_file_t m_file;

  /** File index within the archive group */
  uint m_index{};

  /** Current number of files in the archive group */
  uint m_count{};

  /** Current file offset */
  uint64_t m_offset{};

  /** File size limit in bytes */
  uint64_t m_size{};

  /** Queue of file structure holding reset information pertaining to
  their respective files in a group.
  Protected by Arch_Page_Sys::m_mutex and Arch_Page_Sys::m_oper_mutex.
  @note used only by the page archiver */
  Arch_Reset m_reset;

  /** Vector of stop points corresponding to a file.
  Stop point refers to the stop lsn (checkpoint lsn) until which the pages are
  guaranteed to be tracked in a file. Each block in a file maintains this
  information.
  Protected by Arch_Page_Sys::m_oper_mutex.
  @note used only by the page archiver */
  std::vector<lsn_t> m_stop_points;
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
      : m_begin_lsn(start_lsn),
        m_header_len(header_len)
#ifdef UNIV_DEBUG
        ,
        m_arch_mutex(mutex)
#endif /* UNIV_DEBUG */
  {
    m_active_file.m_file = OS_FILE_CLOSED;
    m_durable_file.m_file = OS_FILE_CLOSED;
    m_stop_pos.init();
  }

  /** Destructor: Delete all files for non-durable archiving. */
  ~Arch_Group();

  /** Initialize the doublewrite buffer file context for the archive group.
  @param[in]	path		path to the file
  @param[in]	base_file	file name prefix
  @param[in]	num_files	initial number of files
  @param[in]	file_size	file size in bytes
  @return error code. */
  static dberr_t init_dblwr_file_ctx(const char *path, const char *base_file,
                                     uint num_files, uint64_t file_size);

  /** Initialize the file context for the archive group.
  File context keeps the archived data in files on disk. There
  is one file context for a archive group.
  @param[in]	path			path to the file
  @param[in]	base_dir		directory name prefix
  @param[in]	base_file		file name prefix
  @param[in]	num_files		initial number of files
  @param[in]	file_size		file size in bytes
  @return error code. */
  dberr_t init_file_ctx(const char *path, const char *base_dir,
                        const char *base_file, uint num_files,
                        uint64_t file_size) {
    return (m_file_ctx.init(path, base_dir, base_file, num_files, file_size));
  }

  /* Close the file contexts when they're not required anymore. */
  void close_file_ctxs() {
    m_file_ctx.close();

    if (m_durable_file.m_file != OS_FILE_CLOSED) {
      os_file_close(m_durable_file);
      m_durable_file.m_file = OS_FILE_CLOSED;
    }
  }

  /** Mark archive group inactive.
  A group is marked inactive by archiver background before entering
  into idle state ARCH_STATE_IDLE.
  @param[in]	end_lsn	lsn where redo archiving is stopped */
  void disable(lsn_t end_lsn) {
    m_is_active = false;

    if (end_lsn != LSN_MAX) {
      m_end_lsn = end_lsn;
    }
  }

  /** Attach a client to the archive group.
  @param[in]	is_durable	true, if durable tracking is requested
  @return	number of client references */
  void attach(bool is_durable) {
    ut_ad(mutex_own(m_arch_mutex));
    ++m_num_active;

    if (is_durable) {
      ++m_dur_ref_count;
    } else {
      ++m_ref_count;
    }
  }

  /** Detach a client when archiving is stopped by the client.
  The client still has reference to the group so that the group
  is not destroyed when it retrieves the archived data. The
  reference is removed later by #Arch_Group::release.
  @param[in]	stop_lsn	archive stop lsn for client
  @param[in]	stop_pos	archive stop position for client. Used only by
  the page_archiver.
  @return number of active clients */
  uint detach(lsn_t stop_lsn, Arch_Page_Pos *stop_pos) {
    ut_ad(m_num_active > 0);
    ut_ad(mutex_own(m_arch_mutex));
    --m_num_active;

    if (m_num_active == 0) {
      m_end_lsn = stop_lsn;
      if (stop_pos != nullptr) {
        m_stop_pos = *stop_pos;
      }
    }

    return (m_num_active);
  }

  /** Release the archive group from a client.
  Reduce the reference count. When all clients release the group,
  the reference count falls down to zero. The function would then
  return zero and the caller can remove the group.
  @param[in]	is_durable	the client needs durable archiving */
  void release(bool is_durable) {
    ut_ad(mutex_own(m_arch_mutex));
    ut_a(!is_durable);

    ut_ad(m_ref_count > 0);
    --m_ref_count;
  }

  /** Construct file name for the active file which indicates whether a group
  is active or not.
  @note Used only by the page archiver.
  @return error code. */
  dberr_t build_active_file_name();

  /** Construct file name for the durable file which indicates whether a group
  was made durable or not.
  @note Used only by the page archiver.
  @return error code. */
  dberr_t build_durable_file_name();

  /** Mark the group active by creating a file in the respective group
  directory. This is required at the time of recovery to know whether a group
  was active or not in case of a crash.
  @note Used only by the page archiver.
  @return error code. */
  int mark_active();

  /** Mark the group durable by creating a file in the respective group
  directory. This is required at the time of recovery to differentiate durable
  group from group left over by crash during clone operation.
  @note Used only by the page archiver.
  @return error code. */
  int mark_durable();

  /** Mark the group inactive by deleting the 'active' file. This is required
  at the time of crash recovery to know whether a group was active or not in
  case of a crash.
  @note Used only by the page archiver.
  @return error code */
  int mark_inactive();

  /** Check if archiving is going on for this group
  @return true, if the group is active */
  bool is_active() const { return (m_is_active); }

  /** Write the header (RESET page) to an archived file.
  @note Used only by the Page Archiver and not by the Redo Log Archiver.
  @param[in]	from_buffer	buffer to copy data
  @param[in]	length		size of data to copy in bytes
  @note Used only by the Page Archiver.
  @return error code */
  dberr_t write_file_header(byte *from_buffer, uint length);

  /** Write to the doublewrite buffer before writing archived data to a file.
  The source is either a file context or buffer. Caller must ensure that data
  is in single file in source file context.
  @param[in]	from_file	file context to copy data from
  @param[in]	from_buffer	buffer to copy data or NULL
  @param[in]	write_size	size of data to write in bytes
  @param[in]	offset		offset from where to write
  @note Used only by the Page Archiver.
  @return error code */
  static dberr_t write_to_doublewrite_file(Arch_File_Ctx *from_file,
                                           byte *from_buffer, uint write_size,
                                           Arch_Page_Dblwr_Offset offset);

  /** Archive data to one or more files.
  The source is either a file context or buffer. Caller must ensure that data
  is in single file in source file context.
  @param[in]	from_file	file context to copy data from
  @param[in]	from_buffer	buffer to copy data or NULL
  @param[in]	length		size of data to copy in bytes
  @param[in]	partial_write	true if the operation is part of partial flush
  @param[in]	do_persist	doublewrite to ensure persistence
  @return error code */
  dberr_t write_to_file(Arch_File_Ctx *from_file, byte *from_buffer,
                        uint length, bool partial_write, bool do_persist);

  /** Find the appropriate reset LSN that is less than or equal to the
  given lsn and fetch the reset point.
  @param[in]	check_lsn	LSN to be searched against
  @param[out]	reset_point	reset position of the fetched reset point
  @return true if the search was successful. */
  bool find_reset_point(lsn_t check_lsn, Arch_Point &reset_point) {
    return (m_file_ctx.find_reset_point(check_lsn, reset_point));
  }

  /** Find the first stop LSN that is greater than the given LSN and fetch
  the stop point.
  @param[in]	check_lsn	LSN to be searched against
  @param[out]	stop_point	stop point
  @param[in]	write_pos	latest write_pos
  @return true if the search was successful. */
  bool find_stop_point(lsn_t check_lsn, Arch_Point &stop_point,
                       Arch_Page_Pos write_pos) {
    ut_ad(validate_info_in_files());
    Arch_Page_Pos last_pos = is_active() ? write_pos : m_stop_pos;
    return (m_file_ctx.find_stop_point(this, check_lsn, stop_point, last_pos));
  }

#ifdef UNIV_DEBUG
  /** Adjust end LSN to end of file. This is used in debug
  mode to test the case when LSN is at file boundary.
  @param[in,out]        stop_lsn        stop lsn for client
  @param[out]   blk_len         last block length */
  void adjust_end_lsn(lsn_t &stop_lsn, uint32_t &blk_len);

  /** Adjust redo copy length to end of file. This is used
  in debug mode to archive only till end of file.
  @param[in,out]        length  data to copy in bytes */
  void adjust_copy_length(uint32_t &length);

  /** Check if the information maintained in the memory is the same
  as the information maintained in the files.
  @return true if both sets of information are the same */
  bool validate_info_in_files();
#endif /* UNIV_DEBUG */

  /** Get the total number of archived files belonging to this group.
  @return number of archived files */
  uint get_file_count() const { return (m_file_ctx.get_count()); }

  /** Check if any client (durable or not) is attached to the archiver.
  @return true if any client is attached, else false */
  bool is_referenced() const {
    return (m_ref_count > 0) || (m_dur_ref_count > 0);
  }

  /** Check if any client requiring durable archiving is active.
  @return true if any durable client is still attached, else false */
  bool is_durable_client_active() const {
    return (m_num_active != m_ref_count);
  }

  /** Check if any client requires durable archiving.
  @return true if there is at least 1 client that requires durable archiving*/
  bool is_durable() const { return (m_dur_ref_count > 0); }

  /** Attach system client to the archiver during recovery if any group was
  active at the time of crash. */
  void attach_during_recovery() { ++m_dur_ref_count; }

  /** Purge archived files until the specified purge LSN.
  @param[in]	purge_lsn	LSN until which archived files needs to be
  purged
  @param[out]	purged_lsn	LSN until which purging is successfule;
  LSN_MAX if there was no purging done
  @return error code */
  uint purge(lsn_t purge_lsn, lsn_t &purged_lsn);

  /** Operations to be done at the time of shutdown. */
  static void shutdown() { s_dblwr_file_ctx.close(); }

  /** Update the reset information in the in-memory structure that we maintain
  for faster access.
  @param[in]	lsn     lsn at the time of reset
  @param[in]	pos     pos at the time of reset
  @retval true if the reset point was saved
  @retval false if the reset point wasn't saved because it was already saved */
  void save_reset_point_in_mem(lsn_t lsn, Arch_Page_Pos pos) {
    m_file_ctx.save_reset_point_in_mem(lsn, pos);
  }

  /** Update stop lsn of a file in the group.
  @param[in]	pos		stop position
  @param[in]	stop_lsn	stop point */
  void update_stop_point(Arch_Page_Pos pos, lsn_t stop_lsn) {
    m_file_ctx.update_stop_point(Arch_Block::get_file_index(pos.m_block_num),
                                 stop_lsn);
  }

  /** Recover the information belonging to this group from the archived files.
  @param[in,out]	group_info	structure containing information of a
  group obtained during recovery by scanning files
  @param[in,out]	new_empty_file	true if there is/was an empty archived
  file
  @param[in]		dblwr_ctx	file context related to doublewrite
  buffer
  @param[out]		write_pos	latest write position at the time of
  crash /shutdown that needs to be filled
  @param[out]		reset_pos   latest reset position at the time crash
  /shutdown that needs to be filled
  @return error code */
  dberr_t recover(Arch_Recv_Group_Info *group_info, bool &new_empty_file,
                  Arch_Dblwr_Ctx *dblwr_ctx, Arch_Page_Pos &write_pos,
                  Arch_Page_Pos &reset_pos);

  /** Reads the latest data block and reset block.
  This would be required in case of active group to start page archiving after
  recovery, and in case of inactive group to fetch stop lsn. So we perform this
  operation regardless of whether it's an active or inactive group.
  @param[in]	buf	buffer to read the blocks into
  @param[in]	offset	offset from where to read
  @param[in]	type	block type
  @return error code */
  dberr_t recovery_read_latest_blocks(byte *buf, uint64_t offset,
                                      Arch_Blk_Type type);

  /** Fetch the last reset file and last stop point info during recovery
  @param[out]   reset_file  last reset file to be updated
  @param[out]   stop_lsn    last stop lsn to be updated */
  void recovery_fetch_info(Arch_Reset_File &reset_file, lsn_t &stop_lsn) {
    m_file_ctx.recovery_fetch_info(reset_file, stop_lsn);
  }

#ifdef UNIV_DEBUG
  /** Print recovery related data.
  @param[in]	file_start_index	file index from where to begin */
  void recovery_reset_print(uint file_start_index) {
    DBUG_PRINT("page_archiver", ("Group : %" PRIu64 "", m_begin_lsn));
    m_file_ctx.recovery_reset_print(file_start_index);
    DBUG_PRINT("page_archiver", ("End lsn: %" PRIu64 "", m_end_lsn));
  }
#endif

  /** Parse block for block info (header/data).
  @param[in]	cur_pos		position to read
  @param[in,out]	buff	buffer into which to write the parsed data
  @param[in]	buff_len	length of the buffer
  @return error code */
  int read_data(Arch_Page_Pos cur_pos, byte *buff, uint buff_len);

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
  uint64_t get_file_size() const { return (m_file_ctx.get_size()); }

  /** Get start LSN for this group
  @return start LSN */
  lsn_t get_begin_lsn() const { return (m_begin_lsn); }

  /** @return stop LSN for this group */
  lsn_t get_end_lsn() const { return (m_end_lsn); }

  /** @return stop block position of the group. */
  Arch_Page_Pos get_stop_pos() const { return (m_stop_pos); }

  /** Fetch the status of the page tracking system.
  @param[out]	status	vector of a pair of (ID, bool) where ID is the
  start/stop point and bool is true if the ID is a start point else false */
  void get_status(std::vector<std::pair<lsn_t, bool>> &status) {
    m_file_ctx.get_status(status);

    if (!is_active()) {
      status.push_back(std::make_pair(m_end_lsn, false));
    }
  }

  /** Disable copy construction */
  Arch_Group(Arch_Group const &) = delete;

  /** Disable assignment */
  Arch_Group &operator=(Arch_Group const &) = delete;

 private:
  /** Get page IDs from archived file
  @param[in]	read_pos	position to read from
  @param[in]	read_len	length of data to read
  @param[in]	read_buff	buffer to read page IDs
  @return error code */
  int read_from_file(Arch_Page_Pos *read_pos, uint read_len, byte *read_buff);

  /** Get the directory name for this archive group.
  It is used for cleaning up the archive directory.
  @param[out]	name_buf	directory name and path. Caller must
                                  allocate the buffer.
  @param[in]	buf_len		buffer length */
  void get_dir_name(char *name_buf, uint buf_len) {
    m_file_ctx.build_dir_name(m_begin_lsn, name_buf, buf_len);
  }

  /** Check and replace blocks in archived files belonging to a group
  from the doublewrite buffer if required.
  @param[in]	dblwr_ctx	Doublewrite context which has the doublewrite
  buffer blocks
  @return error code */
  dberr_t recovery_replace_pages_from_dblwr(Arch_Dblwr_Ctx *dblwr_ctx);

  /** Delete the last file if there are no blocks flushed to it.
  @param[out]	num_files	number of files present in the group
  @param[in]	start_index	file index from where the files are present
  If this is not 0 then the files with file index less that this might have
  been purged.
  @param[in]	durable		true if the group is durable
  @param[out]	empty_file	true if there is/was an empty archived file
  @return error code. */
  dberr_t recovery_cleanup_if_required(uint &num_files, uint start_index,
                                       bool durable, bool &empty_file);

  /** Start parsing the archive file for archive group information.
  @param[out]		write_pos	latest write position at the time of
  crash /shutdown that needs to be filled
  @param[out]		reset_pos   latest reset position at the time crash
  /shutdown that needs to be filled
  @param[in]	start_index	file index from where the files are present
  If this is not 0 then the files with file index less that this might have
  been purged.
  @return error code */
  dberr_t recovery_parse(Arch_Page_Pos &write_pos, Arch_Page_Pos &reset_pos,
                         size_t start_index);

  /** Open the file which was open at the time of a crash, during crash
  recovery, and set the file offset to the last written offset.
  @param[in]	write_pos	block position from where page IDs will be
  tracked
  @param[in]	empty_file	true if an empty archived file was present at
  the time of crash. We delete this file as part of crash recovery process so
  this needs to be handled here.
  @return error code. */
  dberr_t open_file_during_recovery(Arch_Page_Pos write_pos, bool empty_file);

 private:
  /** If the group is active */
  bool m_is_active{true};

  /** To know which group was active at the time of a crash/shutdown during
  recovery we create an empty file in the group directory. This holds the name
  of the file. */
  char *m_active_file_name{nullptr};

  /** File descriptor for a file required to indicate that the group was
  active at the time of crash during recovery . */
  pfs_os_file_t m_active_file;

  /** File name for the durable file which indicates whether a group was made
  durable or not. Required to differentiate durable group from group left over
  by crash during clone operation. */
  char *m_durable_file_name{nullptr};

  /** File descriptor for a file to indicate that the group was made durable or
  not. Required to differentiate durable group from group left over by crash
  during clone operation. */
  pfs_os_file_t m_durable_file;

  /** Number of clients referencing the group */
  uint m_ref_count{};

  /** Number of clients referencing for durable archiving */
  uint m_dur_ref_count{};

  /** Number of clients for which archiving is in progress */
  uint m_num_active{};

  /** Start LSN for the archive group */
  lsn_t m_begin_lsn{LSN_MAX};

  /** End lsn for this archive group */
  lsn_t m_end_lsn{LSN_MAX};

  /** Stop position of the group, if it's not active. */
  Arch_Page_Pos m_stop_pos{};

  /** Header length for the archived files */
  uint m_header_len{};

  /** Archive file context */
  Arch_File_Ctx m_file_ctx;

  /** Doublewrite buffer file context.
  Note - Used only in the case of page archiver. */
  static Arch_File_Ctx s_dblwr_file_ctx;

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
  lsn_t get_archived_lsn() { return (m_archived_lsn.load()); }

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
  int start(Arch_Group *&group, lsn_t &start_lsn, byte *header,
            bool is_durable);

  /** Stop redo log archiving.
  If other clients are there, the client is detached from
  the current group.
  @param[out]	group		log archive group
  @param[out]	stop_lsn	stop lsn for client
  @param[out]	log_blk		redo log trailer block
  @param[in,out]	blk_len		length in bytes
  @return error code */
  int stop(Arch_Group *group, lsn_t &stop_lsn, byte *log_blk,
           uint32_t &blk_len);

  /** Force to abort the archiver (state becomes ARCH_STATE_ABORT). */
  void force_abort();

  /** Release the current group from client.
  @param[in]	group		group the client is attached to
  @param[in]	is_durable	if client needs durable archiving */
  void release(Arch_Group *group, bool is_durable);

  /** Archive accumulated redo log in current group.
  This interface is for archiver background task to archive redo log
  data by calling it repeatedly over time.
  @param[in] init		true when called for first time; it will then
                                be set to false
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
  int wait_archive_complete(lsn_t target_lsn);

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
  atomic_lsn_t m_archived_lsn;

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

/** Vector of page archive in memory blocks */
using Arch_Block_Vec = std::vector<Arch_Block *, ut_allocator<Arch_Block *>>;

/** Page archiver in memory data */
struct ArchPageData {
  /** Constructor */
  ArchPageData() {}

  /** Allocate buffer and initialize blocks
  @return true, if successful */
  bool init();

  /** Delete blocks and buffer */
  void clean();

  /** Get the block for a position
  @param[in]	pos	position in page archive sys
  @param[in]	type	block type
  @return page archive in memory block */
  Arch_Block *get_block(Arch_Page_Pos *pos, Arch_Blk_Type type);

  /** @return temporary block used to copy active block for partial flush. */
  Arch_Block *get_partial_flush_block() const {
    return (m_partial_flush_block);
  }

  /** Vector of data blocks */
  Arch_Block_Vec m_data_blocks{};

  /** Reset block */
  Arch_Block *m_reset_block{nullptr};

  /** Temporary block used to copy active block for partial flush. */
  Arch_Block *m_partial_flush_block{nullptr};

  /** Block size in bytes */
  uint m_block_size{};

  /** Total number of blocks */
  uint m_num_data_blocks{};

  /** In memory buffer */
  byte *m_buffer{nullptr};
};

/** Forward declaration. */
class Page_Arch_Client_Ctx;

/** Dirty page archive system */
class Arch_Page_Sys {
 public:
  /** Constructor: Initialize elements and create mutex */
  Arch_Page_Sys();

  /** Destructor: Free memory buffer and mutexes */
  ~Arch_Page_Sys();

  /** Start dirty page ID archiving.
  If archiving is already in progress, the client is attached to current group.
  @param[out]	group		page archive group the client gets attached to
  @param[out]	start_lsn	start lsn for client in archived data
  @param[out]	start_pos	start position for client in archived data
  @param[in]	is_durable	true if client needs durable archiving
  @param[in]	restart	true if client is already attached to current group
  @param[in]	recovery	true if archiving is being started during
  recovery
  @return error code */
  int start(Arch_Group **group, lsn_t *start_lsn, Arch_Page_Pos *start_pos,
            bool is_durable, bool restart, bool recovery);

  /** Stop dirty page ID archiving.
  If other clients are there, the client is detached from the current group.
  @param[in]	group		page archive group the client is attached to
  @param[out]	stop_lsn	stop lsn for client
  @param[out]	stop_pos	stop position in archived data
  @param[in]	is_durable	true if client needs durable archiving
  @return error code */
  int stop(Arch_Group *group, lsn_t *stop_lsn, Arch_Page_Pos *stop_pos,
           bool is_durable);

  /** Start dirty page ID archiving during recovery.
  @param[in]	group	Group which needs to be attached to the archiver
  @param[in]	new_empty_file  true if there was a empty file created
  @return error code */
  int start_during_recovery(Arch_Group *group, bool new_empty_file);

  /** Release the current group from client.
  @param[in]	group		group the client is attached to
  @param[in]	is_durable	if client needs durable archiving
  @param[in]	start_pos	start position when the client calling the
  release was started */
  void release(Arch_Group *group, bool is_durable, Arch_Page_Pos start_pos);

  /** Check and add page ID to archived data.
  Check for duplicate page.
  @param[in]	bpage		page to track
  @param[in]	track_lsn	LSN when tracking started
  @param[in]	frame_lsn	current LSN of the page
  @param[in]	force		if true, add page ID without check */
  void track_page(buf_page_t *bpage, lsn_t track_lsn, lsn_t frame_lsn,
                  bool force);

  /** Flush all the unflushed inactive blocks and flush the active block if
  required.
  @note Used only during the checkpointing process.
  @param[in]	checkpoint_lsn	next checkpoint LSN */
  void flush_at_checkpoint(lsn_t checkpoint_lsn);

  /** Archive dirty page IDs in current group.
  This interface is for archiver background task to flush page archive
  data to disk by calling it repeatedly over time.
  @param[out]	wait	true, if no more data to archive
  @return true, if archiving is aborted */
  bool archive(bool *wait);

  /** Acquire dirty page ID archiver mutex.
  It synchronizes concurrent start and stop operations by multiple clients. */
  void arch_mutex_enter() { mutex_enter(&m_mutex); }

  /** Release page ID archiver mutex */
  void arch_mutex_exit() { mutex_exit(&m_mutex); }

  /** Acquire dirty page ID archive operation mutex.
  It synchronizes concurrent page ID write to memory buffer. */
  void arch_oper_mutex_enter() { mutex_enter(&m_oper_mutex); }

  /** Release page ID archiver operatiion  mutex */
  void arch_oper_mutex_exit() { mutex_exit(&m_oper_mutex); }

  /* Save information at the time of a reset considered as the reset point.
  @return error code */
  void save_reset_point(bool is_durable);

  /** Wait for reset info to be flushed to disk.
  @param[in]	request_block	block number until which blocks need to be
  flushed
  @return true if flushed, else false */
  bool wait_for_reset_info_flush(uint64_t request_block);

  /** Get the group which has tracked pages between the start_id and stop_id.
  @param[in,out]	start_id	start LSN from which tracked pages are
  required; updated to the actual start LSN used for the search
  @param[in,out]	stop_id     stop_lsn until when tracked pages are
  required; updated to the actual stop LSN used for the search
  @param[out]		group       group which has the required tracked
  pages, else nullptr.
  @return error */
  int fetch_group_within_lsn_range(lsn_t &start_id, lsn_t &stop_id,
                                   Arch_Group **group);

  /** Purge the archived files until the specified purge LSN.
  @param[in]	purge_lsn	purge lsn until where files needs to be purged
  @return error code
  @retval 0 if purge was successful */
  uint purge(lsn_t *purge_lsn);

  /** Update the stop point in all the required structures.
  @param[in]	cur_blk	block which needs to be updated with the stop info */
  void update_stop_info(Arch_Block *cur_blk);

  /** Fetch the status of the page tracking system.
  @param[out]	status	vector of a pair of (ID, bool) where ID is the
  start/stop point and bool is true if the ID is a start point else false */
  void get_status(std::vector<std::pair<lsn_t, bool>> &status) {
    for (auto group : m_group_list) {
      group->get_status(status);
    }
  }

  /** Given start and stop position find number of pages tracked between them
  @param[in]	start_pos	start position
  @param[in]	stop_pos	stop position
  @param[out]	num_pages	number of pages tracked between start and stop
  position
  @return false if start_pos and stop_pos are invalid else true */
  bool get_num_pages(Arch_Page_Pos start_pos, Arch_Page_Pos stop_pos,
                     uint64_t &num_pages);

  /** Get approximate number of tracked pages between two given LSN values.
  @param[in,out]      start_id        fetch archived page Ids from this LSN
  @param[in,out]      stop_id         fetch archived page Ids until this LSN
  @param[out]         num_pages       number of pages tracked between specified
  LSN range
  @return error code */
  int get_num_pages(lsn_t &start_id, lsn_t &stop_id, uint64_t *num_pages);

  /** Get page IDs from a specific position.
  Caller must ensure that read_len doesn't exceed the block.
  @param[in]	group		group whose pages we're interested in
  @param[in]	read_pos	position in archived data
  @param[in]	read_len	amount of data to read
  @param[out]	read_buff	buffer to return the page IDs.
  @note Caller must allocate the buffer.
  @return true if we could successfully read the block. */
  bool get_pages(Arch_Group *group, Arch_Page_Pos *read_pos, uint read_len,
                 byte *read_buff);

  /** Get archived page Ids between two given LSN values.
  Attempt to read blocks directly from in memory buffer. If overwritten,
  copy from archived files.
  @param[in]	thd		thread handle
  @param[in]      cbk_func        called repeatedly with page ID buffer
  @param[in]      cbk_ctx         callback function context
  @param[in,out]  start_id        fetch archived page Ids from this LSN
  @param[in,out]  stop_id         fetch archived page Ids until this LSN
  @param[in]      buf             buffer to fill page IDs
  @param[in]      buf_len         buffer length in bytes
  @return error code */
  int get_pages(MYSQL_THD thd, Page_Track_Callback cbk_func, void *cbk_ctx,
                lsn_t &start_id, lsn_t &stop_id, byte *buf, uint buf_len);

  /** Set the latest stop LSN to the checkpoint LSN at the time it's called. */
  void post_recovery_init();

  /** Recover the archiver system at the time of startup. Recover information
  related to all the durable groups and start archiving if any group was active
  at the time of crash/shutdown.
  @return error code */
  dberr_t recover();

#ifdef UNIV_DEBUG
  /** Print information related to the archiver for debugging purposes. */
  void print();
#endif

  /** Check if archiver system is in initial state
  @return true, if page ID archiver state is #ARCH_STATE_INIT */
  bool is_init() const { return (m_state == ARCH_STATE_INIT); }

  /** Check if archiver system is active
  @return true, if page ID archiver state is #ARCH_STATE_ACTIVE or
  #ARCH_STATE_PREPARE_IDLE. */
  bool is_active() const {
    return (m_state == ARCH_STATE_ACTIVE || m_state == ARCH_STATE_PREPARE_IDLE);
  }

  /** @return true if in abort state */
  bool is_abort() const { return (m_state == ARCH_STATE_ABORT); }

  /** Get the mutex protecting concurrent start, stop operations required
  for initialising group during recovery.
  @return mutex */
  ib_mutex_t *get_mutex() { return (&m_mutex); }

  /** @return operation mutex */
  ib_mutex_t *get_oper_mutex() { return (&m_oper_mutex); }

  /** Fetch the system client context.
  @return system client context. */
  Page_Arch_Client_Ctx *get_sys_client() const { return (m_ctx); }

  /** @return the latest stop LSN */
  lsn_t get_latest_stop_lsn() const { return (m_latest_stop_lsn); }

  /** Disable copy construction */
  Arch_Page_Sys(Arch_Page_Sys const &) = delete;

  /** Disable assignment */
  Arch_Page_Sys &operator=(Arch_Page_Sys const &) = delete;

  class Recv;

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

  /** Flush the blocks to disk.
  @param[out]	wait	true, if no more data to archive
  @return error code */
  dberr_t flush_blocks(bool *wait);

  /** Flush all the blocks which are ready to be flushed but not flushed.
  @param[out]	cur_pos	position of block which needs to be flushed
  @param[in]	end_pos	position of block until which the blocks need to
  be flushed
  @return error code */
  dberr_t flush_inactive_blocks(Arch_Page_Pos &cur_pos, Arch_Page_Pos end_pos);

  /** Do a partial flush of the current active block
  @param[in]	cur_pos	position of block which needs to be flushed
  @param[in]	partial_reset_block_flush	true if reset block needs to be
  flushed
  @return error code */
  dberr_t flush_active_block(Arch_Page_Pos cur_pos,
                             bool partial_reset_block_flush);

 private:
  /** Mutex protecting concurrent start, stop operations */
  ib_mutex_t m_mutex;

  /** Archiver system state. */
  Arch_State m_state{ARCH_STATE_INIT};

  /** List of log archive groups */
  Arch_Grp_List m_group_list{};

  /** Position where last client started archiving */
  Arch_Page_Pos m_last_pos{};

  /** LSN when last client started archiving */
  lsn_t m_last_lsn{LSN_MAX};

  /** Latest LSN until where the tracked pages have been flushed. */
  lsn_t m_latest_stop_lsn{LSN_MAX};

  /** LSN until where the groups are purged. */
  lsn_t m_latest_purged_lsn{LSN_MAX};

  /** Mutex protecting concurrent operation on data */
  ib_mutex_t m_oper_mutex;

  /** Current archive group */
  Arch_Group *m_current_group{nullptr};

  /** In memory data buffer */
  ArchPageData m_data{};

  /** Position to add new page ID */
  Arch_Page_Pos m_write_pos{};

  /** Position to add new reset element */
  Arch_Page_Pos m_reset_pos{};

  /** Position set to explicitly request the flush archiver to flush until
  this position.
  @note this is always increasing and is only updated by the requester thread
  like checkpoint */
  Arch_Page_Pos m_request_flush_pos{};

  /** Block number set to explicitly request the flush archiver to partially
  flush the current active block with reset LSN.
  @note this is always increasing and is only updated by the requester thread
  like checkpoint */
  uint64_t m_request_blk_num_with_lsn{std::numeric_limits<uint64_t>::max()};

  /** Block number set once the flush archiver partially flushes the current
  active block with reset LSN.
  @note this is always increasing and is only updated by the requester thread
  like checkpoint */
  uint64_t m_flush_blk_num_with_lsn{std::numeric_limits<uint64_t>::max()};

  /** Position for start flushing
  @note this is always increasing and is only updated by the page archiver
  thread */
  Arch_Page_Pos m_flush_pos{};

  /** The index of the file the last reset belonged to.  */
  uint m_last_reset_file_index{0};

  /** System client. */
  Page_Arch_Client_Ctx *m_ctx;
};

/** Redo log archiver system global */
extern Arch_Log_Sys *arch_log_sys;

/** Dirty page ID archiver system global */
extern Arch_Page_Sys *arch_page_sys;

#endif /* ARCH_ARCH_INCLUDE */
