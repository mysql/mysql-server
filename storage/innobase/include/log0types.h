/*****************************************************************************

Copyright (c) 2013, 2022, Oracle and/or its affiliates.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/**************************************************/ /**
 @file include/log0types.h

 Redo log basic types

 *******************************************************/

#ifndef log0types_h
#define log0types_h

/* std::atomic<X> */
#include <atomic>

/* std::chrono::X */
#include <chrono>

/* std::string */
#include <string>

/* byte */
#include "univ.i"

/* os_offset_t */
#include "os0file.h"

/* ut::INNODB_CACHE_LINE_SIZE */
#include "ut0cpu_cache.h"

/** Type used for all log sequence number storage and arithmetic. */
typedef uint64_t lsn_t;

/** Log file id (0 for ib_redo0) */
typedef size_t Log_file_id;

/** Log flags (stored in file header of log file). */
typedef uint32_t Log_flags;

/** Number which tries to uniquely identify a created set of redo log files.
Redo log files, which have different values of Log_uuid, most likely have been
created for different directories and cannot be mixed. This way foreign redo
files might be easily recognized. When that is the case, most likely something
went wrong when copying files. */
typedef uint32_t Log_uuid;

/** Print format for lsn_t values, used in functions like printf. */
#define LSN_PF UINT64PF

/** Alias for atomic based on lsn_t. */
using atomic_lsn_t = std::atomic<lsn_t>;

/** Type used for sn values, which enumerate bytes of data stored in the log.
Note that these values skip bytes of headers and footers of log blocks. */
typedef uint64_t sn_t;

/** Alias for atomic based on sn_t. */
using atomic_sn_t = std::atomic<sn_t>;

/* The lsn_t, sn_t, Log_file_id are known so constants can be expressed. */
#include "log0constants.h"

/** Enumerates checkpoint headers in the redo log file. */
enum class Log_checkpoint_header_no : uint32_t {
  /** The first checkpoint header. */
  HEADER_1,

  /** The second checkpoint header. */
  HEADER_2
};

/** Type used for counters in log_t: flushes_requested and flushes_expected.
They represent number of requests to flush the redo log to disk. */
typedef std::atomic<int64_t> log_flushes_t;

/** Type of redo log file. */
enum class Log_file_type {
  /** Usual redo log file, most likely with important redo data. */
  NORMAL,
  /** Unused redo log file, might always be removed. */
  UNUSED
};

/** Callback called on each read or write operation on a redo log file.
@param[in]  file_id    id of the redo log file (target of the IO operation)
@param[in]  file_type  type of the redo log file
@param[in]  offset     offset in the file, at which read or write operation
                       is going to start (expressed in bytes and computed
                       from the beginning of the file)
@param[in]  size       size of data that is going to be read or written in
                       the IO operation */
typedef std::function<void(Log_file_id file_id, Log_file_type file_type,
                           os_offset_t offset, os_offset_t size)>
    Log_file_io_callback;

/** Function used to calculate checksums of log blocks. */
typedef std::atomic<uint32_t (*)(const byte *log_block)>
    Log_checksum_algorithm_atomic_ptr;

/** Clock used to measure time spent in redo log (e.g. when flushing). */
using Log_clock = std::chrono::high_resolution_clock;

/** Time point defined by the Log_clock. */
using Log_clock_point = std::chrono::time_point<Log_clock>;

/** Supported redo log formats. Stored in LOG_HEADER_FORMAT. */
enum class Log_format : uint32_t {
  /** Unknown format of redo file. */
  LEGACY = 0,

  /** The MySQL 5.7.9 redo log format identifier. We can support recovery
  from this format if the redo log is clean (logically empty). */
  VERSION_5_7_9 = 1,

  /** Remove MLOG_FILE_NAME and MLOG_CHECKPOINT, introduce MLOG_FILE_OPEN
  redo log record. */
  VERSION_8_0_1 = 2,

  /** Allow checkpoint_lsn to point any data byte within redo log (before
  it had to point the beginning of a group of log records). */
  VERSION_8_0_3 = 3,

  /** Expand ulint compressed form. */
  VERSION_8_0_19 = 4,

  /** Row versioning header. */
  VERSION_8_0_28 = 5,

  /** Introduced with innodb_redo_log_capacity:
   - write LSN does not re-enter file with checkpoint_lsn,
   - epoch_no is checked strictly during recovery. */
  VERSION_8_0_30 = 6,

  /** The redo log format identifier
  corresponding to the current format version. */
  CURRENT = VERSION_8_0_30
};

/** Ruleset defining how redo log files are named, where they are stored,
when they are created and what sizes could they have. */
enum class Log_files_ruleset {
  /** Redo log files were named ib_logfile0, ib_logfile1, ... ib_logfile99.
  Redo log files were pre-created during startup and re-used after wrapping.
  Redo log files had the same file size and supported formats < VERSION_8_0_30.
  The non-initialized set of redo log files was denoted by existence of the
  ib_logfile101. The log files were located directly in the root directory
  (innodb_log_group_home_dir if specified; else: datadir). */
  PRE_8_0_30,

  /** Redo log files are named #ib_redo0, #ib_redo1, ... and no longer wrapped.
  Redo log files are created on-demand during runtime and might have different
  sizes. Formats >= VERSION_8_0_30 are supported. The redo log files are located
  in #innodb_redo subdirectory in the root directory - for example:
    - if innodb_log_group_home_dir = '/srv/my_db/logs', then redo files are in
      '/srv/my_db/logs/#innodb_redo/',
    - if innodb_log_group_home_dir is not specified and datadir='/srv/my_db',
      then redo files are in '/srv/my_db/#innodb_redo'. */
  CURRENT
};

/** Direction of resize operation. */
enum class Log_resize_mode {
  /** No pending resize. */
  NONE,

  /** Resizing down. */
  RESIZING_DOWN
};

/** Configures path to the root directory, where redo subdirectory might be
located (or redo log files if the ruleset is older). Configures the ruleset
that should be used when locating redo log files. */
struct Log_files_context {
  explicit Log_files_context(
      const std::string &root_path = "",
      Log_files_ruleset files_ruleset = Log_files_ruleset::CURRENT);

  /** Path to the root directory. */
  std::string m_root_path;

  /** Ruleset determining how file paths are built. */
  Log_files_ruleset m_files_ruleset;
};

/** Meta data stored in log file header. */
struct Log_file_header {
  /** Format of the log file. */
  uint32_t m_format;

  /** LSN of the first log block (%512 == 0). */
  lsn_t m_start_lsn;

  /** Creator name. */
  std::string m_creator_name;

  /** Log flags. Meaning of bit positions is to be found in documentation
  of LOG_HEADER_FLAG_* constants in log0constants.h. */
  Log_flags m_log_flags;

  /** UUID value describing the whole group of log files. */
  Log_uuid m_log_uuid;
};

/** Meta data stored in one of two checkpoint headers. */
struct Log_checkpoint_header {
  /** Checkpoint LSN (oldest_lsn_lwm from the moment of checkpoint). */
  lsn_t m_checkpoint_lsn;
};

/** Meta data stored in header of a log data block. */
struct Log_data_block_header {
  /** Together with m_hdr_no form unique identifier of this block,
  @see LOG_BLOCK_EPOCH_NO. */
  uint32_t m_epoch_no;

  /** Together with m_epoch_no form unique identifier of this block,
  @see log_block_get_hdr_no. Each next log data block has hdr_no
  incremented by 1 (unless wrapped). */
  uint32_t m_hdr_no;

  /** Offset up to which this block has data inside, computed from the
  beginning of the block. */
  uint16_t m_data_len;

  /** Offset to the first mtr starting in this block, or 0 if there is no
  mtr starting in this block. */
  uint16_t m_first_rec_group;

  /** Sets m_epoch_no and m_hdr_no from a single lsn
   */
  void set_lsn(lsn_t lsn);
};

/** Pair of: log file id and log file size (expressed in bytes). */
struct Log_file_id_and_size {
  Log_file_id_and_size() = default;

  Log_file_id_and_size(Log_file_id id, os_offset_t size)
      : m_id(id), m_size_in_bytes(size) {}

  /** Id of the file. */
  Log_file_id m_id{};

  /** Size of file, expressed in bytes. */
  os_offset_t m_size_in_bytes{};
};

/** Pair of: log file id and log file header. */
struct Log_file_id_and_header {
  Log_file_id_and_header() = default;

  Log_file_id_and_header(Log_file_id id, Log_file_header header)
      : m_id(id), m_header(header) {}

  /** Id of the file. */
  Log_file_id m_id{};

  /** Main header of the file. */
  Log_file_header m_header{};
};

/** Type of access allowed for the opened redo log file. */
enum class Log_file_access_mode {
  /** The opened file can be both read and written. */
  READ_WRITE,
  /** The opened file can be only read. */
  READ_ONLY,
  /** The opened file can be only written. */
  WRITE_ONLY,
};

/** Handle which allows to do reads / writes for the opened file.
For particular kind of reads or writes (for checkpoint headers,
data blocks, main file header or encryption header) there are helper
functions defined outside this class. Unless you wanted to transfer
the whole file as-is, you should rather use those functions for
read/write operations. */
class Log_file_handle {
 public:
  explicit Log_file_handle(Encryption_metadata &encryption_metadata);

  Log_file_handle(Log_file_handle &&other);
  Log_file_handle &operator=(Log_file_handle &&rhs);

  /** Closes handle if was opened (calling fsync if was modified).
  Destructs the handle object. */
  ~Log_file_handle();

  /** @return true iff file is opened (by this handle) */
  bool is_open() const;

  /** Closes file represented by this handle (must be opened). */
  void close();

  /** @return id of the log file */
  Log_file_id file_id() const;

  /** @return path to the log file (including the file name) */
  const std::string &file_path() const;

  /** @return file size in bytes */
  os_offset_t file_size() const;

  /** Reads from the log file at the given offset (to the provided buffer).
  @param[in]   read_offset     offset in bytes from the beginning of the file
  @param[in]   read_size       number of bytes to read
  @param[out]  buf             allocated buffer to fill when reading
  @return DB_SUCCESS or error */
  dberr_t read(os_offset_t read_offset, os_offset_t read_size, byte *buf);

  /** Writes the provided buffer to the log file at the given offset.
  @param[in]   write_offset    offset in bytes from the beginning of the file
  @param[in]   write_size      number of bytes to write
  @param[in]   buf             buffer to write
  @return DB_SUCCESS or error */
  dberr_t write(os_offset_t write_offset, os_offset_t write_size,
                const byte *buf);

  /** Executes fsync operation for this redo log file. */
  void fsync();

  /** @return number of fsyncs in-progress */
  static uint64_t fsyncs_in_progress() { return s_fsyncs_in_progress.load(); }

  /** @return total number of fsyncs that have been started since
              the server has started */
  static uint64_t total_fsyncs() { return s_total_fsyncs.load(); }

  /** Callback called on each read operation. */
  static Log_file_io_callback s_on_before_read;

  /** Callback called on each write operation. */
  static Log_file_io_callback s_on_before_write;

  /** True iff all fsyncs should be no-op. */
  static bool s_skip_fsyncs;

 private:
  friend struct Log_file;

  /** Tries to open a given redo log file with with a given access mode.
  If succeeded then this handle represents the opened file and allows to
  performs reads and/or writes (depends on the requested access mode).
  If encountered error during the attempt to open, error message is emitted
  to the error log, in which case this handle remains closed.
  @param[in]  files_ctx            context within which files exist
  @param[in]  id                   id of the log file
  @param[in]  access_mode          access mode for the opened file
  @param[in]  encryption_metadata  encryption metadata
  @param[in]  file_type            type of redo file */
  Log_file_handle(const Log_files_context &files_ctx, Log_file_id id,
                  Log_file_access_mode access_mode,
                  Encryption_metadata &encryption_metadata,
                  Log_file_type file_type);

  Log_file_handle(const Log_file_handle &other) = delete;
  Log_file_handle &operator=(const Log_file_handle &rhs) = delete;

  /** Open the log file with the configured access mode.
  @return DB_SUCCESS or error */
  dberr_t open();

  /** Creates and configures an io request object according to currently
  configured encryption metadata (m_encryption_*) and m_block_size.
  @param[in] req_type            defines type of IO operation (read or write)
  @param[in] offset              offset in bytes from the beginning of the file
  @param[in] size                number of bytes to write or read
  @param[in] can_use_encryption  e.g. whether newly blocks should be encrypted
  @return IORequest instance */
  IORequest prepare_io_request(int req_type, os_offset_t offset,
                               os_offset_t size, bool can_use_encryption);

#ifdef UNIV_DEBUG
  /** Number of all opened Log_file_handle existing currently. */
  static std::atomic<size_t> s_n_open;
#endif /* UNIV_DEBUG */

  /** Number of fsyncs in-progress. */
  static std::atomic<uint64_t> s_fsyncs_in_progress;

  /** Total number of fsyncs that have been started since
  the server has started. */
  static std::atomic<uint64_t> s_total_fsyncs;

  /** Id of the redo log file (part of its file name) */
  Log_file_id m_file_id;

  /** Access mode allowed for this handle (if not yet closed). */
  Log_file_access_mode m_access_mode;

  /** Encryption metadata to be used for all IO operations on this file
  except those related to the first LOG_FILE_HDR_SIZE bytes.
  @remarks If Encryption::set_key() and Encryption::set_initial_vector()
  started to accept their arguments as const pointers, this could become
  a const pointer too). In such case, all usages of Encryption_metadata*
  inside redo log code, could also become changed to usages of the const
  pointer. */
  Encryption_metadata &m_encryption_metadata;

  /** Type of redo log file. */
  Log_file_type m_file_type;

  /** Whether file is opened */
  bool m_is_open;

  /** Whether file has been modified using this handle since it was opened. */
  bool m_is_modified;

  /** File name */
  std::string m_file_path;

  /** OS handle for file (if opened) */
  pfs_os_file_t m_raw_handle;

  /** Size of single physical block (if opened) */
  os_offset_t m_block_size;

  /** Size of file in bytes (if opened) */
  os_offset_t m_file_size;
};

/** Meta information about single log file. */
struct Log_file {
  Log_file(const Log_files_context &files_ctx,
           Encryption_metadata &encryption_metadata);

  Log_file(const Log_files_context &files_ctx, Log_file_id id, bool consumed,
           bool full, os_offset_t size_in_bytes, lsn_t start_lsn, lsn_t end_lsn,
           Encryption_metadata &encryption_metadata);

  Log_file(const Log_file &other) = default;

  Log_file &operator=(const Log_file &other);

  /** Context within which this file exists. */
  const Log_files_context &m_files_ctx;

  /** ID of the file. */
  Log_file_id m_id;

  /** Set to true when file becomes consumed. */
  bool m_consumed;

  /** Set to true when file became full and next file exists. */
  bool m_full;

  /** Size, expressed in bytes, including LOG_FILE_HDR_SIZE. */
  os_offset_t m_size_in_bytes;

  /** LSN of the first byte within the file, aligned to
  OS_FILE_LOG_BLOCK_SIZE. */
  lsn_t m_start_lsn;

  /** LSN of the first byte after the file, aligned to
  OS_FILE_LOG_BLOCK_SIZE. */
  lsn_t m_end_lsn;

  /** Encryption metadata passed to opened file handles
  @see Log_file_handle::m_encryption_metadata */
  Encryption_metadata &m_encryption_metadata;

  /** Checks if this object is equal to a given another object.
  @param[in]  rhs   the object to compare against
  @return true iff all related fields of the two objects are equal */
  bool operator==(const Log_file &rhs) const {
    return m_id == rhs.m_id && m_consumed == rhs.m_consumed &&
           m_full == rhs.m_full && m_size_in_bytes == rhs.m_size_in_bytes &&
           m_start_lsn == rhs.m_start_lsn && m_end_lsn == rhs.m_end_lsn;
  }

  /** Validates that lsn fields seem correct (m_start_lsn, m_end_lsn) */
  void lsn_validate() const {
    ut_a(m_start_lsn == 0 || LOG_START_LSN <= m_start_lsn);
    ut_a(m_start_lsn < m_end_lsn);
    ut_a(m_start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
    ut_a(m_end_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  }

  /** Checks if a given lsn belongs to [m_start_lsn, m_end_lsn).
  In other words, checks that the given lsn belongs to this file.
  @param[in]  lsn   lsn to test against lsn range
  @return true iff lsn is in the file */
  bool contains(lsn_t lsn) const {
    return m_start_lsn <= lsn && lsn < m_end_lsn;
  }

  /** Provides offset for the given LSN (from the beginning of the log file).
  @param[in]  lsn   lsn to locate (must exist in the file)
  @return offset from the beginning of the file for the given lsn */
  os_offset_t offset(lsn_t lsn) const {
    lsn_validate();
    ut_a(contains(lsn) || lsn == m_end_lsn);
    return offset(lsn, m_start_lsn);
  }

  /** Provides offset for the given LSN and log file with the given start_lsn
  (offset from the beginning of the log file).
  @param[in]  lsn              lsn to locate (must be >= file_start_lsn)
  @param[in]  file_start_lsn   start lsn of the log file
  @return offset from the beginning of the file for the given lsn */
  static os_offset_t offset(lsn_t lsn, lsn_t file_start_lsn) {
    return LOG_FILE_HDR_SIZE + (lsn - file_start_lsn);
  }

  /** Computes id of the next log file. Does not check if such file exists.
  @return id of the next log file */
  Log_file_id next_id() const { return next_id(m_id, 1); }

  /** Opens this file and provides handle that allows to read from this file
  and / or write to this file (depends on the requested access mode).
  @param[in]  access_mode      requested access mode (reads and/or writes)
  @return handle to the opened file or empty handle with error information */
  Log_file_handle open(Log_file_access_mode access_mode) const;

  /** Opens a given redo log file and provides handle that allows to read from
  that file and / or write to that file (depends on the requested access mode).
  @param[in]  files_ctx            context within which file exists
  @param[in]  file_id              id of the redo log file to open
  @param[in]  access_mode          requested access mode (reads and/or writes)
  @param[in]  encryption_metadata  encryption metadata
  @param[in]  file_type            type of underlying file on disk to open
  @return handle to the opened file or empty handle with error information */
  static Log_file_handle open(const Log_files_context &files_ctx,
                              Log_file_id file_id,
                              Log_file_access_mode access_mode,
                              Encryption_metadata &encryption_metadata,
                              Log_file_type file_type = Log_file_type::NORMAL);

  /** Computes id + inc, asserting it does not overflow the maximum value.
  @param[in]  id    base id of file, to which inc is added
  @param[in]  inc   number of log files ahead (1 = directly next one)
  @return id + inc */
  static Log_file_id next_id(Log_file_id id, size_t inc = 1) {
    constexpr Log_file_id MAX_FILE_ID = std::numeric_limits<Log_file_id>::max();
    ut_a(0 < inc);
    ut_a(inc <= MAX_FILE_ID);
    ut_a(id <= MAX_FILE_ID - inc);
    return id + inc;
  }
};

struct alignas(ut::INNODB_CACHE_LINE_SIZE) log_t;

/** Runtime statistics related to redo log files management. These stats are
not persisted to disk. */
struct Log_files_stats {
  /** Last time stats were updated (last successful call to @see update()). */
  Log_clock_point m_last_update_time{};

  /** LSN difference by which result of log_files_oldest_needed_lsn() advanced
  during last second. This is basically average consumption speed.
  Updated by successful calls to @see update(). */
  lsn_t m_lsn_consumption_per_1s{0};

  /** LSN difference by which result of log_files_newest_needed_lsn() advanced
  during last second. This is basically average production speed.
  Updated by successful calls to @see update(). */
  lsn_t m_lsn_production_per_1s{0};

  /** Oldest LSN returned by log_files_oldest_needed_lsn() during last
  successful call to @see update(). */
  lsn_t m_oldest_lsn_on_update{0};

  /** Newest LSN returned by log_files_newest_needed_lsn() during last
  successful call to @see update(). */
  lsn_t m_newest_lsn_on_update{0};

  /** Tries to update stats. Fails and skips updating if less than 1s elapsed
  since last successful update, else: updates the stats and succeeds.
  @param[in]  log   redo log */
  void update(const log_t &log);
};

#endif /* !log0types_h */
