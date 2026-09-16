/*****************************************************************************

Copyright (c) 1995, 2026, Oracle and/or its affiliates.

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

/** @file include/fil0fil.h
 The low-level file system

 Created 10/25/1995 Heikki Tuuri
 *******************************************************/

#ifndef fil0fil_h
#define fil0fil_h

#include "univ.i"

#include "dict0types.h"
#include "fil0tablespace_node_handle_interface.h"
#include "fil0types.h"
#include "mem0mem.h" /* mem_strdup */
#include "page0size.h"
#ifndef UNIV_HOTBACKUP
#include "ibuf0types.h"
#endif /* !UNIV_HOTBACKUP */
#include "srv0srv.h"
#include "srv0start.h"
#include "ut0expected.h"
#include "ut0new.h"

#include "mysql/strings/m_ctype.h"
#include "sql/dd/object_id.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <list>
#include <optional>
#include <span>
#include <unordered_set>
#include <variant>
#include <vector>

extern ulong srv_fast_shutdown;

/** Maximum number of tablespaces to be scanned by a thread while scanning
for available tablespaces during server startup. This is a hard maximum.
If the number of files to be scanned is more than
FIL_SCAN_MAX_TABLESPACES_PER_THREAD,
then additional threads will be spawned to scan the additional files in
parallel. */
constexpr size_t FIL_SCAN_MAX_TABLESPACES_PER_THREAD = 8000;

/** Maximum number of threads that will be used for scanning the tablespace
files. This can be further adjusted depending on the number of available
cores. */
constexpr size_t FIL_SCAN_MAX_THREADS = 16;

/** Number of threads per core. */
constexpr size_t FIL_SCAN_THREADS_PER_CORE = 2;

/** Calculate the number of threads that can be spawned to scan the given
number of files taking into the consideration, number of cores available
on the machine.
@param[in]      num_files       Number of files to be scanned
@return number of threads to be spawned for scanning the files */
size_t fil_get_scan_threads(size_t num_files);

/** This tablespace name is used internally during file discovery to open a
general tablespace before the data dictionary is recovered and available. */
static constexpr char general_space_name[] = "innodb_general";

/** This tablespace name is used as the prefix for implicit undo tablespaces
and during file discovery to open an undo tablespace before the DD is
recovered and available. */
static constexpr char undo_space_name[] = "innodb_undo";

extern volatile bool recv_recovery_on;

/** Initial size of an UNDO tablespace when it is created new
or truncated under low load.
page size | FSP_EXTENT_SIZE  | Initial Size | Pages
----------+------------------+--------------+-------
    4 KB  | 256 pages = 1 MB |   16 MB      | 4096
    8 KB  | 128 pages = 1 MB |   16 MB      | 2048
   16 KB  |  64 pages = 1 MB |   16 MB      | 1024
   32 KB  |  64 pages = 2 MB |   16 MB      | 512
   64 KB  |  64 pages = 4 MB |   16 MB      | 256  */
constexpr uint32_t UNDO_INITIAL_SIZE = 16 * 1024 * 1024;
#define UNDO_INITIAL_SIZE_IN_PAGES \
  os_offset_t { UNDO_INITIAL_SIZE / srv_page_size }

#ifdef UNIV_HOTBACKUP
#include <unordered_set>
using Dir_set = std::unordered_set<std::string>;
extern Dir_set rem_gen_ts_dirs;
extern bool replay_in_datadir;
#endif /* UNIV_HOTBACKUP */

// Forward declaration
struct trx_t;
class page_id_t;

using Filenames = std::vector<std::string, ut::allocator<std::string>>;
using Space_ids = std::vector<space_id_t, ut::allocator<space_id_t>>;

/** Value of fil_space_t::magic_n */
constexpr size_t FIL_SPACE_MAGIC_N = 89472;

/** Value of fil_node_t::magic_n */
constexpr size_t FIL_NODE_MAGIC_N = 89389;

/** Real paths of dirs directly under the MySQL datadir (symlinks resolved) */
class Dirs_in_datadir {
 public:
  /** Build the set of dirs directly under the MySQL datadir.
  @return set of real paths of dirs directly under MySQL datadir */
  [[nodiscard]] static Dirs_in_datadir make();

  /** Check whether a dir path matches with one of the dirs directly under the
  MySQL datadir.
  @param[in]  discovered_dir  directory path to check
  @return true if discovered_dir resolves to a known schema directory */
  [[nodiscard]] bool contains(const std::string &discovered_dir) const;

 private:
  using Dirs = std::unordered_set<std::string>;

  /** Normalized paths of all dirs directly under the MySQL datadir. */
  Dirs m_dirs;
};

/** Result of comparing a path. */
enum class Fil_state {
  /** The path matches what was found during the scan. */
  MATCHES,

  /** No MLOG_FILE_DELETE record and the file could not be found. */
  MISSING,

  /** Space ID matches but the paths don't match. */
  MOVED,

  /** Space ID and paths match but dd_table data dir flag is false despite the
  file being outside default data dir */
  MOVED_PREV,

  /** In case of error during comparison. */
  COMPARE_ERROR
};

class fil_space_t;

/** Node of a tablespace encapsulating handle required for any IO operations on
this node. */
class fil_node_t {
 public:
  fil_node_t(fil_space_t *space, const char *name, uint32_t node_order,
             bool is_raw, page_no_t size_in_pages, page_no_t max_pages,
             uint32_t block_size)
      : space(space),
        name(mem_strdup(name)),
        m_order(node_order),
        is_raw_disk(is_raw),
        flush_size(size_in_pages),
        init_size(size_in_pages),
        m_max_size_in_pages(max_pages),
        m_block_size(block_size),
        m_cached_size_in_pages(size_in_pages) {}

  fil_node_t(fil_node_t &&other)
      : space(other.space),
        name(other.name),
        m_order(other.m_order),
        is_raw_disk(other.is_raw_disk),
        flush_size(other.flush_size),
        init_size(other.init_size),
        m_max_size_in_pages(other.m_max_size_in_pages),
        n_pending_ios(other.n_pending_ios),
        m_is_being_flushed(other.m_is_being_flushed),
        is_being_extended(other.is_being_extended),
        magic_n(other.magic_n),
        m_handle(std::move(other.m_handle)),
        m_is_open(other.m_is_open),
        m_modification_counter(other.m_modification_counter),
        m_flush_counter(other.m_flush_counter),
        m_n_threads_waiting_for_flush(other.m_n_threads_waiting_for_flush),
        m_block_size(other.m_block_size),
        m_punch_hole(other.m_punch_hole),
        m_cached_size_in_pages(other.m_cached_size_in_pages) {
    ut_a(!m_is_open);
    ut_a(!m_handle);
    ut_a(!other.LRU.prev);
    ut_a(!other.LRU.next);
    other.name = nullptr;
  }

  ~fil_node_t() {
    if (name) {
      ut::free(name);
    }
  }
  using modification_counter_t = int64_t;

  /** Returns true if the node can be closed. */
  [[nodiscard]] bool can_be_closed() const;

  /** Returns true if the node is fully flushed, that is it is flushed up to the
  modification_counter. */
  [[nodiscard]] bool is_flushed() const;

  /** Sets node to flushed state without actually flushing it. */
  void pretend_is_flushed();

  /** Increment modification counter for this node. */
  void increment_modification_counter();

  /** Determine if flush is needed for the node.
  @return true if flush is needed, false otherwise */
  [[nodiscard]] bool is_flush_needed() const;

  /** Waits for any pending flushes.
  @param[in]  shard  Reference to Fil_shard that will be used to release and
                     re-acquire mutex while we are waiting for the pending
                     flushes. */
  void wait_for_all_flushes_to_finish(class Fil_shard &shard);

  /** Assures the node will be flushed at least to m_modification_counter.
  @param[in]  shard  Reference to Fil_shard that will be used to release and
                     re-acquire mutex while we are waiting for the pending
                     flushes. */
  void flush(class Fil_shard &shard);

  [[nodiscard]] bool needs_flushes_for_durability() const {
    return m_is_open && m_handle->needs_flushes_for_durability();
  }

  /** Posts a synchronous READ or WRITE IO operation on the node.
  It is blocking call and returns the status code of the IO operation.
  @param[in]      type        IO request type, compression and encryption
                              information.
  @param[in,out]  buf         A buffer where to store read data or from where
                              to write. Data from this buffer might be first
                              transformed before writing it. It must be aligned
                              in memory to OS block size (UNIV_SECTOR_SIZE).
                              Additionally, for reads, it must be aligned to
                              physical page size, too. The memory pointed is
                              managed by the caller and must remain valid until
                              the call returns.
  @param[in]      buffer_len  Size of the buffer. It must not cross the node
                              boundary; It always has to be a multiply of OS
                              block size (UNIV_SECTOR_SIZE). In case of write of
                              an already compressed data, it is a length of the
                              compressed data buffer. Otherwise it should be
                              physical page size. Actual number of bytes
                              written can be smaller if the tablespace has
                              compression enabled and data was not compressed
                              already.
  @param[in]      page_no     Page number where to read from or write into.
  @return DB_SUCCESS on successful IO completion, otherwise error code */
  [[nodiscard]] dberr_t post_io_sync(IORequest &type, byte *buf,
                                     size_t buffer_len,
                                     page_no_t page_no) const;

#ifdef UNIV_LINUX
  /** Posts a synchronous vectored WRITE IO operation on the node.
  It is blocking call and returns the status code of the IO operation. This call
  does write operation on group of pages which are not supposed to be encrypted
  or compressed.
  @param[in]  buffers            Buffers where pages to be written to storage
                                 are present. Each buffer should be of physical
                                 page size of tablespace. Each buffer must be
                                 aligned in memory to OS block size
                                 (UNIV_SECTOR_SIZE). These buffers are meant to
                                 be written unencrypted and uncompressed. The
                                 memory pointed is managed by the caller and
                                 must remain valid until the call returns.
  @param[in]  first_page_number  Offset page in the file storage where writing
                                 starts */
  [[nodiscard]] dberr_t write_pages(std::span<const byte *> buffers,
                                    page_no_t first_page_number);
#endif /* UNIV_LINUX */

#ifndef UNIV_HOTBACKUP
  /** Posts an asynchronous READ or WRITE IO operation on the node.
  The operation may return error code immediately or DB_SUCCESS when the IO was
  submitted for asynchronous completion. After the asynchronous IO is complete,
  the `os_aio_handler` will return a reference to the current node, @p
  callback value supplied, the @p type and status code for the
  result of the IO operation.
  @param[in]      type        IO request type, compression and encryption
                              information.
  @param[in,out]  buf         A buffer where to store read data or from where
                              to write. Data from this buffer might be first
                              transformed before writing it. It must be aligned
                              in memory to OS block size (UNIV_SECTOR_SIZE).
                              Additionally, for reads, it must be aligned to
                              physical page size, too. The memory pointed is
                              managed by the caller and must remain valid until
                              the @p callback begins execution.
  @param[in]      buffer_len  Size of the buffer. It must not cross the node
                              boundary; It always has to be a multiply of OS
                              block size (UNIV_SECTOR_SIZE). In case of write of
                              an already compressed data, it is a length of the
                              compressed data buffer. Otherwise it should be
                              physical page size. Actual number of bytes
                              written can be smaller if the tablespace has
                              compression enabled and data was not compressed
                              already.
  @param[in]      page_no     Page number where to read from or write into.
  @param[in]      callback    A callback to be called exactly once when the
                              result of this IO operation is known. It may be a
                              success if the read or write succeeded or a subset
                              of `dberr_t` errors if the write or read could not
                              be executed or if it failed. It can be called
                              synchronously in this thread before returning from
                              this method, or can be executed asynchronously
                              from another thread, when @p sync is false, before
                              or after this call returns.
  @return DB_SUCCESS if IO was successfully posted, error code otherwise */
  [[nodiscard]] dberr_t post_io_async(
      IORequest &type, byte *buf, size_t buffer_len, page_no_t page_no,
      std::function<void(dberr_t)> callback) const;
#endif /* !UNIV_HOTBACKUP */

  /** Returns true iff the node is currently opened and allows IO operations. */
  [[nodiscard]] bool is_open() const { return m_is_open; }

  /** Must the node be opened only for read only mode? Files can be opened for
  read and write, unless the srv_read_only_mode is used. With srv_read_only_mode
  all files are read-only, but the temporary tables, as they are not altering
  the physical database data, they do not generate UNDO etc, so can be used even
  in the read-only mode.*/
  [[nodiscard]] bool is_read_only() const;

  /** Opens the node for incoming IO operations. The node will be opened for
  read-only if `is_read_only()` returns true, and for read-write otherwise.
  @return true if node is successfully opened */
  [[nodiscard]] bool open();

  /** Closes the node, the IO operations will not be permitted. It is up to the
  user to provide synchronization with open(), IO calls and wait for pending IO
  operations to finish first before calling the `close`. */
  void close();

  /** Truncates the node. Sets the truncated node to have size of @p size bytes,
  that are all zeroed.
  @param[in] new_size_in_pages Size in pages to set the node size to after the
  truncation. */
  [[nodiscard]] bool truncate(page_no_t new_size_in_pages);

  /** Makes the @p number_of_pages pages, starting with page number @p
  first_page contain all zeros. This can be used to efficiently extend the node
  size. It may be implemented more efficiently than actually writing buffers
  with zeros.
  @param[in] first_page ID of first page to be overwritten with zeros.
  @param[in] number_of_pages Number of pages to overwrite with zeros. */
  [[nodiscard]] dberr_t fill_range_with_zeros(page_no_t first_page,
                                              page_no_t number_of_pages);

  using List_node = UT_LIST_NODE_T(fil_node_t);

  /* Get the block_size for the storage.
  @return block_size of the storage. */
  [[nodiscard]] size_t get_block_size() const { return m_block_size; }

  /** Check if punch_hole is supported by storage.
  @return true if punch hole is supported, false otherwise. */
  [[nodiscard]] bool get_punch_hole() const { return m_punch_hole; }
  void set_punch_hole(bool value) { m_punch_hole = value; }

  /** tablespace containing this node */
  fil_space_t *const space;

  /** file name; protected by Fil_shard::m_mutex and log_sys->mutex. */
  char *name;

  /** Number of the file in the tablespace's nodes list */
  const size_t m_order;

  /** whether the node actually is a raw device or disk partition */
  const bool is_raw_disk;

  /** @see m_cached_size_in_pages */
  [[nodiscard]] page_no_t get_cached_size_in_pages() const {
    return m_cached_size_in_pages;
  }

  /** In addition to updating  @see m_cached_size_in_pages it also updates
  the size of the fil_space_t it belongs to */
  void set_cached_node_size(size_t new_pages_count_to_cache);

  /** Size of the node when last flushed, used to force the flush when node
  grows to keep the filesystem metadata synced when using O_DIRECT_NO_FSYNC */
  page_no_t flush_size;

  /** initial size of the node in database pages;
  FIL_IBD_FILE_INITIAL_SIZE by default */
  page_no_t init_size;

  /** maximum size of the node in database pages */
  const page_no_t m_max_size_in_pages;

  /** count of pending I/O's; is_open must be true if nonzero */
  size_t n_pending_ios{};

  /** Set to true when the node is being flushed. */
  bool m_is_being_flushed{};

  /** Set to true when a node is being extended. */
  bool is_being_extended{};

  /** link to the fil_system->LRU list (keeping track of open files) */
  List_node LRU{};

  /** FIL_NODE_MAGIC_N */
  const size_t magic_n{FIL_NODE_MAGIC_N};

 private:
  /** Determine if flush is needed.
  @param[in]  flush_upto  expectation up to which flush is to be done
  @return true if flush is not done up to flush_upto, false otherwise */
  [[nodiscard]] bool is_flush_needed(modification_counter_t flush_upto) const;

  /** Waits for the current flush (if any) to finish, unless the file is already
  flushed up to flush_upto
  @param[in]  shard       Reference to Fil_shard that will be used to release
                          and re-acquire mutex while we are waiting for the
                          current flush to finish.
  @param[in]  flush_upto  Wait until current flush finished or until
                          m_flush_counter reached flush_upto */
  void wait_for_current_flush_to_finish(class Fil_shard &shard,
                                        modification_counter_t flush_upto);

  /** Map error code returned from read/write APIs to dberr_t code.
  @param[in]  err  error code returned from read/write APIs
  @return corresponding dberr_t error code */
  [[nodiscard]] static dberr_t map_status_io_to_db_err(
      ib::fil::Tablespace_node_handle_interface::Status_IO err);

  /** file handle (not null iff is_open) */
  ut::unique_ptr<ib::fil::Tablespace_node_handle_interface> m_handle{};

  /** event that groups and serializes calls to fsync */
  Os_event_t m_flush_finished_event{};

  /** whether this node is open. */
  bool m_is_open{false};

  /** number of writes to the node since the system was started */
  modification_counter_t m_modification_counter{};

  /** the m_modification_counter of the latest flush to disk */
  modification_counter_t m_flush_counter{};

  /** number of threads waiting for chance to flush */
  size_t m_n_threads_waiting_for_flush{};

  /** block size to use for punching holes */
  const size_t m_block_size;

  /** whether the file system of this node supports PUNCH HOLE */
  bool m_punch_hole{false};

  /** Size of the node in physical pages, excluding any partial last page.
  The field is initialized with the fresh result of
  Tablespace_nodes_interface::get_node_info(), and updated by InnoDB whenever
  it is performing Fil_shard::space_truncate() or Fil_shard::space_extend(),
  to match the desired size.
  The size of the physical page is validated to flags in the tablespace header,
  i.e. it is impossible to continue if size of the page declared in the header
  doesn't match the expectation.
  However, the number of pages declared in the header, and cached in the
  m_fsp_cache.m_size_in_header, doesn't have to match m_cached_size_in_pages.
  In other words:
  1. m_fsp_cache.m_size_in_header matches what is in the header.
  2. m_cached_size_in_pages matches what Tablespace_nodes_interface last said,
  about the real length of the tablespace (until we extend the space, in which
  case we increase m_cached_size_in_pages to match the new size and an answer
  Tablespace_nodes_interface would give if we asked it). */
  page_no_t m_cached_size_in_pages;
};

/** Tablespace or log data space */
class fil_space_t {
 public:
  using List_node = UT_LIST_NODE_T(fil_space_t);
  using Files = std::vector<fil_node_t, ut::allocator<fil_node_t>>;

  fil_space_t();
  ~fil_space_t();

  /** @return true if the instance is queued for deletion. Guarantees the space
  is not deleted as long as the fil_shard mutex is not released. */
  [[nodiscard]] bool is_deleted() const;

  /** @return true if the instance was not queued for deletion. It does not
  guarantee it is not queued for deletion at the moment. */
  [[nodiscard]] bool was_not_deleted() const;

  /** Marks the space object for deletion. It will bump the space object version
  and cause all pages in buffer pool that reference to the current space
  object version to be stale and be freed on first encounter. */
  void set_deleted();

#ifndef UNIV_HOTBACKUP
  /** Returns current version of the space object. It is being bumped when the
   space is truncated or deleted. Guarantees the version returned is up to date
   as long as fil_shard mutex is not released.*/
  [[nodiscard]] uint32_t get_current_version() const;

  /** Returns current version of the space object. It is being bumped when the
   space is truncated or deleted. It does not guarantee the version is current
   one.*/
  [[nodiscard]] uint32_t get_recent_version() const;

  /** Bumps the space object version and cause all pages in buffer pool that
  reference the current space object version to be stale and be freed on
  first encounter. */
  void bump_version();

  /** @return true if this space does not have any more references. Guarantees
  the result only if true was returned. */
  [[nodiscard]] bool has_no_references() const;

  /** @return Current number of references to the space. This method
  should be called only while shutting down the server. Only when there is no
  background nor user session activity the returned value will be valid. */
  [[nodiscard]] size_t get_reference_count() const;

  /** Increment the page reference count. */
  void inc_ref() noexcept {
    /* We assume space is protected from being removed either through
    n_pending_ops or m_n_ref_count already bumped, OR MDL latch is
    protecting it, OR it is a private space. Bumping the n_pending_ops
    can be done only under fil shard mutex and stopping new bumps is also
    done under this mutex */
    const auto o = m_n_ref_count.fetch_add(1);
    ut_a(o != std::numeric_limits<size_t>::max());
  }

  /** Decrement the page reference count. */
  void dec_ref() noexcept {
    const auto o = m_n_ref_count.fetch_sub(1);
    ut_a(o >= 1);
  }
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
  /** Print the extent descriptor pages of this tablespace into
  the given output stream.
  @param[in]    out     the output stream.
  @return       the output stream. */
  std::ostream &print_xdes_pages(std::ostream &out) const;

  /** Print the extent descriptor pages of this tablespace into
  the given file.
  @param[in]    filename        the output file name. */
  void print_xdes_pages(const char *filename) const;
#endif /* UNIV_DEBUG */

  /** Get the file node corresponding to the given page number of the
  tablespace. Note that the size of nodes in tablespaces is rounded down to the
  extend size.
  @param[in,out]  page_no   Caller passes the page number within a tablespace.
                            After return, it contains the page number within
                            the returned file node. For tablespaces containing
                            only one file, the given page_no will not change.
  @return The file node object if the file is found with the page_no, or it must
  be opened to check if it contains page_no. `nullptr` if the file is not found
  or does not contain the page. */
  [[nodiscard]] fil_node_t *get_node_for_page_no(page_no_t &page_no) noexcept;

  /** @overload fil_node_t *get_node_for_page_no(page_no_t &page_no) noexcept*/
  [[nodiscard]] const fil_node_t *get_node_for_page_no(
      page_no_t &page_no) const noexcept;

#ifndef UNIV_HOTBACKUP
  /** Checks if the space has encryption flag enabled in the tablespace flags or
  @p validate is true. If not, it just returns `DB_SUCCESS`. Otherwise opens the
  first node, reads the first page out of it, runs validation of data stored
  inside the tablespace header against the data used to construct this object,
  which should come from the DD, checks if the stored space version is correct.
  It checks if re-encryption is needed if the default master key ID was used to
  encrypt it.
  @param[in] validate  Should the validation of the space ID and flags be
                       performed? This is forced if the tablespace flags have
                       the encryption flag even if false is passed.
  @param[in] old_space If specified, the space version checks will not be
                       performed. To be used for files from the MySQL Server 5.7
                       or older, during upgrade process only.
  @param[out] needs_reencryption Is set to true if the tablespace re-encryption
                       is required to be performed after this call ends and the
                       space is added to the `fil` tablespaces mappings and
                       `fil_encryption_reencrypt` requires the space to be
                       present in these mappings.
  @return DB_SUCCESS or error */
  [[nodiscard]] dberr_t validate_to_dd(bool validate, bool old_space,
                                       bool &needs_reencryption);
#endif

  /** Assures the FSP header of the tablespace is correct and fills the
  FSP-related data cache. If the tablespace was already validated, does nothing.
  The first node in the tablespace must be already opened and prevented from
  being closed - it will be used to read the first page directly from the
  storage of the first node. The file can be prevented from being closed by
  calling `prepare_file_for_io()` on it or by having the shard's mutex. The page
  will be read from the first node directly.
  @return DB_SUCCESS or error */
  [[nodiscard]] dberr_t validate_first_page();

  /** Sets the validation flag to true. It should be called from places where we
  are certain the space header is valid, like in case we just filled it in or
  have just verified it is valid. */
  void declare_first_page_valid() { m_validated = true; }

  /** Returns true iff the tablespace was already validated by checking the
  content of the first page. */
  [[nodiscard]] bool is_validated() const { return m_validated; }

  using Observer = Flush_observer;
  using Flush_observers = std::vector<Observer *, ut::allocator<Observer *>>;

  /** When the tablespace was extended last. */
  ib::Timer m_last_extended{};

  /** Extend undo tablespaces by so many pages. */
  page_no_t m_undo_extend{};

  /** When an undo tablespace has been initialized with required header pages,
  that size is recorded here.  Auto-truncation happens when the file size
  becomes bigger than both this and srv_max_undo_log_size. */
  page_no_t m_undo_initial{};

  /** Tablespace name */
  char *name{};

  /** Tablespace ID */
  space_id_t id;

 private:
#ifndef UNIV_HOTBACKUP
  /** All pages in the buffer pool that reference this fil_space_t instance with
  version before this version can be lazily freed or reused as free pages.
  They should be rejected if there is an attempt to write them to disk.

  Writes to m_version are guarded by the exclusive MDL/table lock latches
  acquired by the caller, as stated in docs. Note that the Fil_shard mutex seems
  to be latched in 2 of 3 usages only, so is not really an alternative.

  Existence of the space object during reads is assured during these operations:
  1. when read by the buf_page_init_low on page read/creation - the caller must
  have acquired shared MDL/table lock latches.
  2. when read on buf_page_t::is_stale() on page access for a query or for purge
  operation. The caller must have acquired shared MDL/table lock latches.
  3. when read on buf_page_t::is_stale() on page access from LRU list, flush
  list or whatever else. Here, the fact that the page has latched the space
  using the reference counting system is what guards the space existence.

  When reading the value for the page being created with buf_page_init_low we
  have the MDL latches on table that is in tablespace or the tablespace alone,
  so we won't be able to bump m_version until they are released, so we will
  read the current value of the version. When reading the value for the page
  validation with buf_page_t::is_stale(), we will either:
  a) have the MDL latches required in at least S mode in case we need to be
  certain if the page is stale, to use it in a query or in purge operation, or
  b) in case we don't not have the MDL latches, we may read an outdated value.
  This happens for pages that are seen during for example LRU or flush page
  scans. These pages are not needed for the query itself. The read is to decide
  if the page can be safely discarded. Reading incorrect value can lead to no
  action being executed. Reading incorrect value can't lead to page being
  incorrectly evicted.
  */
  std::atomic<uint32_t> m_version{};

  /** Number of buf_page_t entries that point to this instance.

  This field is guarded by the Fil_shard mutex and the "reference
  count system". The reference count system here is allowing to take a "latch"
  on the space by incrementing the reference count, and release it by
  decrementing it.

  The increments are possible from two places:
  1. buf_page_init_low is covered by the existing MDL/table lock latches only
  and the fact that the space it is using is a current version of the space
  (the version itself is also guarded by these MDL/table lock latches). It
  implicitly acquires the "reference count system" latch after this operation.
  2. buf_page_t::buf_page_t(const buf_page_t&) copy constructor - increases the
  value, but it assumes the page being copied from has "reference count system"
  latch so the reference count is greater than 0 during this constructor call.

  For decrementing the reference count is itself a latch allowing for the safe
  decrement.

  The value is checked for being 0 in Fil_shard::checkpoint under the Fil_shard
  mutex, and only if the space is deleted.
  Observing m_n_ref_count==0 might trigger freeing the object. No other thread
  can be during the process of incrementing m_n_ref_count from 0 to 1 in
  parallel to this check. This is impossible for following reasons. Recall the
  only two places where we do increments listed above:
  1. If the space is deleted, then MDL/table lock latches guarantee there are
  no users that would be able to see it as the current version of space and thus
  will not attempt to increase the reference value from 0.
  2. The buf_page_t copy constructor can increase it, but it assumes the page
  being copied from has "reference count system" latch so the reference count is
  greater than 0 during this constructor call.

  There is also an opposite race possible: while we check for ref count being
  zero, another thread may be decrementing it in parallel, and we might miss
  that if we check too soon. This is benign, as it will result in us not
  reclaiming the memory we could (but not have to) free, and will return to the
  check on next checkpoint.
  */
  std::atomic_size_t m_n_ref_count{};
#endif /* !UNIV_HOTBACKUP */

  /** true if the tablespace is marked for deletion. */
  std::atomic_bool m_deleted{};

  /** true if bulk operation is in progress. */
  std::atomic_bool m_is_bulk{false};

  /** true if bulk operation is in progress. */
  std::atomic<uint64_t> m_bulk_extend_size;

 public:
  /** Begin bulk operation on the space.
  @param[in] extend_size space extension size for bulk operation */
  void begin_bulk_operation(uint64_t extend_size) {
    m_is_bulk.store(true);
    m_bulk_extend_size.store(extend_size);
  }

  /** End bulk operation on the space. */
  void end_bulk_operation() { m_is_bulk.store(false); }

  /** @return true, if bulk operation in progress. */
  [[nodiscard]] bool is_bulk_operation_in_progress() const {
    return m_is_bulk.load();
  }

  /** @return automatic extension size for space. */
  [[nodiscard]] uint64_t get_auto_extend_size() const;

  /** @return the limit on this space size (usually PAGE_MAX_NO) */
  [[nodiscard]] page_no_t get_max_size_in_pages() const;

  /** @return true if added space should be initialized while extending space.
   */
  [[nodiscard]] bool initialize_while_extending() const;

  /** true if we want to rename the .ibd file of tablespace and
  want to temporarily prevent other threads from opening the file that is being
  renamed.  */
  bool prevent_file_open{};

  /** Throttles writing to log a message about long waiting for file to perform
  rename. */
  ib::Throttler m_prevent_file_open_wait_message_throttler;

  /** We set this true when we start deleting a single-table
  tablespace.  When this is set following new ops are not allowed:
  * read IO request
  * ibuf merge
  * file flush
  Note that we can still possibly have new write operations because we
  don't check this flag when doing flush batches. */
  bool stop_new_ops{};

#ifdef UNIV_DEBUG
  /** Reference count for operations who want to skip redo log in
  the file space in order to make fsp_space_modify_check pass. */
  ulint redo_skipped_count{};
#endif /* UNIV_DEBUG */

  /** Purpose */
  fil_type_t purpose;

  /** Files attached to this tablespace. Note: Only the system tablespace
  can have multiple files, this is a legacy issue. */
  Files files{};

  /** Tablespace file size in pages; 0 if not known yet */
  page_no_t m_size_in_pages{};

  /** Returns true if the FSP-related cache is already filled in and valid. */
  [[nodiscard]] bool is_fsp_cache_valid() const;

  /** Ensures the FSP-related cache is filled in and valid upon this call
  return. */
  void ensure_fsp_cache_valid();

  [[nodiscard]] page_no_t get_cached_fsp_size_in_header() const;

  void set_cached_fsp_size_in_header(page_no_t new_size);

  /** Length of the FSP_FREE list */
  [[nodiscard]] uint32_t get_cached_fsp_free_len() const;

  void set_cached_fsp_free_len(uint32_t new_free_len);

  void inc_cached_fsp_free_len(uint32_t delta);

  void dec_cached_fsp_free_len();

  /** Contents of FSP_FREE_LIMIT */
  [[nodiscard]] page_no_t get_cached_fsp_free_limit() const;

  void set_cached_fsp_free_limit(page_no_t new_free_limit);

  /** Fills the cache of the FSP-related data that is stored in the first page
  of the tablespace header.
  @param[in] first_page Pointer to buffer with the content of the first page.
  The buffer must be at least the length of the smallest page possible. */
  void fill_fsp_cache(const byte *first_page);

 private:
  /** Reads the specified page directly from the disk, without using `fil_io` or
  Buffer Pool caching. Requires the page to be located in the first file and
  requires this file to be already opened and prevented from being closed. The
  file can be prevented from being closed by calling `prepare_file_for_io()` on
  it or by having the shard's mutex.
  The returned buffer's length is `page_size_t{flags}.physical()`.
  @param[in] page_no Page number to read from the disk. */
  [[nodiscard]] ut::Expected<ut::unique_ptr_aligned<byte[]>> read_page_direct(
      page_no_t page_no) const;

  /* Specifies once the tablespace was validated to have a correct header in the
  first page. */
  bool m_validated{};

  /** Structure to hold cached data for the FSP subsystem. */
  struct fsp_cache {
    /** FSP_SIZE in the tablespace header; */
    page_no_t m_size_in_header{};

    /** Length of the FSP_FREE list */
    uint32_t m_free_len{};

    /** Contents of FSP_FREE_LIMIT */
    page_no_t m_free_limit{};

    /* Specifies if the cache was filled with valid data already. It will not be
    filled during recovery until the tablespace pages are all recovered. */
    bool m_cache_valid{};
  } m_fsp_cache;

 public:
  /** Autoextend size */
  uint64_t autoextend_size_in_bytes{};

  /** Tablespace flags; see fsp_flags_is_valid() and
  page_size_t(ulint) (constructor).
  This is protected by space->latch and tablespace MDL */
  uint32_t flags{};

  /** Number of reserved free extents for ongoing operations like
  B-tree page split */
  uint32_t n_reserved_extents{};

  /** This is positive when flushing the tablespace to disk;
  dropping of the tablespace is forbidden if this is positive */
  uint32_t n_pending_flushes{};

  /** This is positive when we have pending operations against this
  tablespace. The pending operations can be ibuf merges or lock
  validation code trying to read a block.  Dropping of the tablespace
  is forbidden if this is positive.  Protected by Fil_shard::m_mutex. */
  uint32_t n_pending_ops{};

#ifndef UNIV_HOTBACKUP
  /** Latch protecting the file space storage allocation */
  rw_lock_t latch;
#endif /* !UNIV_HOTBACKUP */

  /** List of spaces with at least one unflushed file we have
  written to */
  List_node unflushed_spaces{};

  /** true if this space is currently in unflushed_spaces */
  bool is_in_unflushed_spaces{};

  /** Compression algorithm */
  Compression::Type compression_type{};

  /** Encryption metadata */
  Encryption_metadata m_encryption_metadata{};

  /** Encryption is in progress */
  Encryption::Progress encryption_op_in_progress{Encryption::Progress::NONE};

  /** Flush lsn of header page. It is used only during recovery */
  lsn_t m_header_page_flush_lsn{};

  /** FIL_SPACE_MAGIC_N */
  ulint magic_n;

  /** System tablespace */
  static fil_space_t *s_sys_space;

  /** Check if the tablespace is compressed.
  @return true if compressed, false otherwise. */
  [[nodiscard]] bool is_compressed() const noexcept {
    return compression_type != Compression::NONE;
  }

  /** Check if the tablespace is encrypted.
  @return true if encrypted, false otherwise. */
  [[nodiscard]] bool is_encrypted() const noexcept {
    return FSP_FLAGS_GET_ENCRYPTION(flags);
  }

  /** Check if the encryption details, like the encryption key, type and
  other details, that are needed to carry out encryption are available.
  @return true if encryption can be done, false otherwise. */
  [[nodiscard]] bool can_encrypt() const noexcept {
    return m_encryption_metadata.m_type != Encryption::Type::NONE;
  }
};

/** Common InnoDB file extensions */
enum ib_file_suffix {
  NO_EXT = 0,
  IBD = 1,
  CFG = 2,
  CFP = 3,
  IBT = 4,
  IBU = 5,
  DWR = 6,
  BWR = 7
};

extern const char *dot_ext[];

#define DOT_IBD dot_ext[IBD]
#define DOT_CFG dot_ext[CFG]
#define DOT_CFP dot_ext[CFP]
#define DOT_IBT dot_ext[IBT]
#define DOT_IBU dot_ext[IBU]
#define DOT_DWR dot_ext[DWR]

#ifdef _WIN32
/* Initialization of m_abs_path() produces warning C4351:
"new behavior: elements of array '...' will be default initialized."
See https://msdn.microsoft.com/en-us/library/1ywe7hcy.aspx */
#pragma warning(disable : 4351)
#endif /* _WIN32 */

/** Wrapper for a path to a directory that may or may not exist. */
class Fil_path {
 public:
  /** schema '/' table separator */
  static constexpr auto DB_SEPARATOR = '/';

  /** OS specific path separator. */
  static constexpr auto OS_SEPARATOR = OS_PATH_SEPARATOR;

  /** Directory separators that are supported. */
  static constexpr auto SEPARATOR = "\\/";
#ifdef _WIN32
  static constexpr auto DOT_SLASH = ".\\";
  static constexpr auto DOT_DOT_SLASH = "..\\";
  static constexpr auto SLASH_DOT_DOT_SLASH = "\\..\\";
#else
  static constexpr auto DOT_SLASH = "./";
  static constexpr auto DOT_DOT_SLASH = "../";
  static constexpr auto SLASH_DOT_DOT_SLASH = "/../";
#endif /* _WIN32 */

  /** Various types of file paths. */
  enum path_type { absolute, relative, file_name_only, invalid };

  /** Default constructor. Defaults to MySQL_datadir_path.  */
  Fil_path();

  /** Constructor
  @param[in]  path            Path, not necessarily NUL terminated
  @param[in]  len             Length of path
  @param[in]  normalize_path  If false, it's the callers responsibility to
                              ensure that the path is normalized. */
  explicit Fil_path(const char *path, size_t len, bool normalize_path = false);

  /** Constructor
  @param[in]  path            Path, not necessarily NUL terminated
  @param[in]  normalize_path  If false, it's the callers responsibility to
                              ensure that the path is normalized. */
  explicit Fil_path(const char *path, bool normalize_path = false);

  /** Constructor
  @param[in]  path            pathname (may also include the file basename)
  @param[in]  normalize_path  If false, it's the callers responsibility to
                              ensure that the path is normalized. */
  explicit Fil_path(const std::string &path, bool normalize_path = false);

  /** Implicit type conversion
  @return pointer to m_path.c_str() */
  [[nodiscard]] operator const char *() const { return m_path.c_str(); }

  /** Explicit type conversion
  @return pointer to m_path.c_str() */
  [[nodiscard]] const char *operator()() const { return m_path.c_str(); }

  /** @return the value of m_path */
  [[nodiscard]] const std::string &path() const { return (m_path); }

  /** @return the length of m_path */
  [[nodiscard]] size_t len() const { return (m_path.length()); }

  /** Return the absolute path by value. If m_abs_path is null, calculate
  it and return it by value without trying to reset this const object.
  m_abs_path can be empty if the path did not exist when this object
  was constructed.
  @return the absolute path by value. */
  [[nodiscard]] const std::string abs_path() const {
    if (m_abs_path.empty()) {
      return (get_real_path(m_path));
    }

    return (m_abs_path);
  }

  /** @return the length of m_abs_path */
  [[nodiscard]] size_t abs_len() const { return (m_abs_path.length()); }

  /** Determine if this path is equal to the other path.
  @param[in]  other             path to compare to
  @return true if the paths are the same */
  bool operator==(const Fil_path &other) const { return (is_same_as(other)); }

  /** Check if m_path is the same as this other path.
  @param[in]  other  directory path to compare to
  @return true if m_path is the same as path */
  [[nodiscard]] bool is_same_as(const Fil_path &other) const;

  /** Check if this path is the same as the other path.
  @param[in]  other  directory path to compare to
  @return true if this path is the same as the other path */
  [[nodiscard]] bool is_same_as(const std::string &other) const;

  /** Get the absolute directory of this path */
  [[nodiscard]] Fil_path get_abs_directory() const;

  /** Check if the directory to path is same as directory as the other path.
  @param[in]  other  directory path to compare to
  @return true if this path directory is the same as the other path directory */
  [[nodiscard]] bool is_dir_same_as(const Fil_path &other) const;

  /** Check if two path strings are equal. Put them into Fil_path objects
  so that they can be compared correctly.
  @param[in]  first   first path to check
  @param[in]  second  socond path to check
  @return true if these two paths are the same */
  [[nodiscard]] static bool is_same_as(const std::string &first,
                                       const std::string &second) {
    if (first.empty() || second.empty()) {
      return (false);
    }

    Fil_path first_path(first);
    std::string first_abs = first_path.abs_path();
    trim_separator(first_abs);

    Fil_path second_path(second);
    std::string second_abs = second_path.abs_path();
    trim_separator(second_abs);

    return (first_abs == second_abs);
  }

  /** Splits the path into directory and file name parts.
  @param[in]  path  path to split
  @return [directory, file] for the path */
  [[nodiscard]] static std::pair<std::string, std::string> split(
      const std::string &path);

  /** Check if m_path is the parent of the other path.
  @param[in]  other  path to compare to
  @return true if m_path is an ancestor of name */
  [[nodiscard]] bool is_ancestor(const Fil_path &other) const;

  /** Check if this Fil_path is an ancestor of the other path.
  @param[in]  other  path to compare to
  @return true if this Fil_path is an ancestor of the other path */
  [[nodiscard]] bool is_ancestor(const std::string &other) const;

  /** Check if the first path is an ancestor of the second.
  Do not assume that these paths have been converted to real paths
  and are ready to compare. If the two paths are the same
  we will return false.
  @param[in]  first   Parent path to check
  @param[in]  second  Descendent path to check
  @return true if the first path is an ancestor of the second */
  [[nodiscard]] static bool is_ancestor(const std::string &first,
                                        const std::string &second) {
    if (first.empty() || second.empty()) {
      return (false);
    }

    Fil_path ancestor(first);
    Fil_path descendant(second);

    return (ancestor.is_ancestor(descendant));
  }

  /** @return true if m_path exists and is a directory. */
  [[nodiscard]] bool is_directory_and_exists() const;

  /** This validation is only for ':'.
  @return true if the path is valid. */
  [[nodiscard]] bool is_valid() const;

  /** Determine if m_path contains a circular section like "/anydir/../"
  Fil_path::normalize() must be run before this.
  @return true if a circular section if found, false if not */
  [[nodiscard]] bool is_circular() const;

  /** Determine if the file or directory is considered HIDDEN.
  Most file systems identify the HIDDEN attribute by a '.' preceding the
  basename.  On Windows, a HIDDEN path is identified by a file attribute.
  We will use the preceding '.' to indicate a HIDDEN attribute on ALL
  file systems so that InnoDB tablespaces and their directory structure
  remain portable.
  @param[in]  path  The full or relative path of a file or directory.
  @return true if the directory or path is HIDDEN. */
  static bool is_hidden(std::string path);

#ifdef _WIN32
  /** Use the WIN32_FIND_DATA struncture to determine if the file or
  directory is HIDDEN.  Consider a SYSTEM attribute also as an indicator
  that it is HIDDEN to InnoDB.
  @param[in]  dirent  A directory entry obtained from a call to FindFirstFile()
  or FindNextFile()
  @return true if the directory or path is HIDDEN. */
  static bool is_hidden(WIN32_FIND_DATA &dirent);
#endif /* WIN32 */

  /** Remove quotes e.g., 'a;b' or "a;b" -> a;b.
  This will only remove the quotes if they are matching on the whole string.
  This will not work if each delimited string is quoted since this is called
  before the string is parsed.
  @return pathspec with the quotes stripped */
  static std::string remove_quotes(const char *pathspec) {
    std::string path(pathspec);

    ut_ad(!path.empty());

    if (path.size() >= 2 && ((path.front() == '\'' && path.back() == '\'') ||
                             (path.front() == '"' && path.back() == '"'))) {
      path.erase(0, 1);
      path.erase(path.size() - 1);
    }

    return (path);
  }

  /** Determine if a path is a relative path or not.
  @param[in]  path  OS directory or file path to evaluate
  @retval true if the path is relative
  @retval false if the path is absolute or file_name_only */
  [[nodiscard]] static bool is_relative_path(const std::string &path) {
    return (type_of_path(path) == relative);
  }

  /** @return true if the path is an absolute path. */
  [[nodiscard]] bool is_absolute_path() const {
    return (type_of_path(m_path) == absolute);
  }

  /** Determine if a path is an absolute path or not.
  @param[in]  path  OS directory or file path to evaluate
  @retval true if the path is absolute
  @retval false if the path is relative or file_name_only */
  [[nodiscard]] static bool is_absolute_path(const std::string &path) {
    return (type_of_path(path) == absolute);
  }

  /** Determine what type of path is provided.
  @param[in]  path  OS directory or file path to evaluate
  @return the type of filepath; 'absolute', 'relative',
  'file_name_only', or 'invalid' if the path is empty. */
  [[nodiscard]] static path_type type_of_path(const std::string &path) {
    if (path.empty()) {
      return (invalid);
    }

    /* The most likely type is a file name only with no separators. */
    auto first_separator = path.find_first_of(SEPARATOR);
    if (first_separator == std::string::npos) {
      return (file_name_only);
    }

    /* Any string that starts with an OS_SEPARATOR is
    an absolute path. This includes any OS and even
    paths like "\\Host\share" on Windows. */
    if (first_separator == 0) {
      return (absolute);
    }

#ifdef _WIN32
    /* Windows may have an absolute path like 'A:\' */
    if (path.length() >= 3 && isalpha(path.at(0)) && path.at(1) == ':' &&
        (path.at(2) == '\\' || path.at(2) == '/')) {
      return (absolute);
    }
#endif /* _WIN32 */

    /* Since it contains separators and is not an absolute path,
    it must be a relative path. */
    return (relative);
  }

  /* Check if the path is prefixed with pattern.
  @return true if prefix matches */
  [[nodiscard]] static bool has_prefix(const std::string &path,
                                       const std::string prefix) {
    return (path.size() >= prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), path.begin()));
  }

  /** Normalize a directory path for the current OS:
  On Windows, we convert '/' to '\', else we convert '\' to '/'.
  @param[in,out]  path  Directory and file path */
  static void normalize(std::string &path) {
    for (auto &c : path) {
      if (c == OS_PATH_SEPARATOR_ALT) {
        c = OS_SEPARATOR;
      }
    }
  }

  /** Normalize a directory path for the current OS:
  On Windows, we convert '/' to '\', else we convert '\' to '/'.
  @param[in,out]  path  A NUL terminated path */
  static void normalize(char *path) {
    for (auto ptr = path; *ptr; ++ptr) {
      if (*ptr == OS_PATH_SEPARATOR_ALT) {
        *ptr = OS_SEPARATOR;
      }
    }
  }

  /** Convert a path string to lower case using the CHARSET my_charset_filename.
  @param[in,out]  path  Directory and file path */
  static void to_lower(std::string &path) {
    for (auto &c : path) {
      c = my_tolower(&my_charset_filename, c);
    }
  }

  /** @return true if the path exists and is a file . */
  [[nodiscard]] static os_file_type_t get_file_type(const std::string &path);

  /** Get the real path for a directory or a file name. This path can be
  used to compare with another absolute path. It will be converted to
  lower case on case insensitive file systems and if it is a directory,
  it will end with a directory separator. The call to my_realpath() may
  fail on non-Windows platforms if the path does not exist. If so, the
  parameter 'force' determines what to return.
  @param[in]  path   directory or filename to convert to a real path
  @param[in]  force  if true and my_realpath() fails, use the path provided.
                     if false and my_realpath() fails, return a null string.
  @return  the absolute path prepared for making comparisons with other real
           paths. */
  [[nodiscard]] static std::string get_real_path(const std::string &path,
                                                 bool force = true);

  /** Get the basename of the file path. This is the file name without any
  directory separators. In other words, the file name after the last separator.
  @param[in]  filepath  The name of a file, optionally with a path. */
  [[nodiscard]] static std::string get_basename(const std::string &filepath);

  /** Separate the portion of a directory path that exists and the portion that
  does not exist.
  @param[in]      path   Path to evaluate
  @param[in,out]  ghost  The portion of the path that does not exist.
  @return the existing portion of a path. */
  [[nodiscard]] static std::string get_existing_path(const std::string &path,
                                                     std::string &ghost);

  /** Check if the name is an undo tablespace name.
  @param[in]    name            Tablespace name
  @return true if it is an undo tablespace name */
  [[nodiscard]] static bool is_undo_tablespace_name(const std::string &name);

  /** Check if the file has the specified suffix
  @param[in]    sfx             suffix to look for
  @param[in]    path            Filename to check
  @return true if it has the ".ibd" suffix. */
  static bool has_suffix(ib_file_suffix sfx, const std::string &path) {
    const auto suffix = dot_ext[sfx];
    size_t len = strlen(suffix);

    return (path.size() >= len &&
            path.compare(path.size() - len, len, suffix) == 0);
  }

  /** Check if the file has the specified suffix and truncate
  @param[in]            sfx     suffix to look for
  @param[in,out]        path    Filename to check
  @return true if the suffix is found and truncated. */
  static bool truncate_suffix(ib_file_suffix sfx, std::string &path) {
    const auto suffix = dot_ext[sfx];
    size_t len = strlen(suffix);

    if (path.size() < len ||
        path.compare(path.size() - len, len, suffix) != 0) {
      return (false);
    }

    path.resize(path.size() - len);
    return (true);
  }

  /** Check if a character is a path separator ('\' or '/')
  @param[in]  c  Character to check
  @return true if it is a separator */
  static bool is_separator(char c) { return (c == '\\' || c == '/'); }

  /** If the last character of a directory path is a separator ('\' or '/')
  trim it off the string.
  @param[in]  path  file system path */
  static void trim_separator(std::string &path) {
    if (!path.empty() && is_separator(path.back())) {
      path.resize(path.size() - 1);
    }
  }

  /** If the last character of a directory path is NOT a separator,
  append a separator to the path.
  NOTE: We leave it up to the caller to assure that the path is a directory
  and not a file since if that directory does not yet exist, this function
  cannot tell the difference.
  @param[in]  path  file system path */
  static void append_separator(std::string &path) {
    if (!path.empty() && !is_separator(path.back())) {
      path.push_back(OS_SEPARATOR);
    }
  }

  /** Allocate and build a file name from a path, a table or
  tablespace name and a suffix.
  @param[in]    path_in         nullptr or the directory path or
                                  the full path and filename
  @param[in]    name_in         nullptr if path is full, or
                                  Table/Tablespace name
  @param[in]    ext             the file extension to use
  @param[in]      trim            whether last name on the path should
                                  be trimmed
  @return own: file name; must be freed by ut::free() */
  [[nodiscard]] static char *make(const std::string &path_in,
                                  const std::string &name_in,
                                  ib_file_suffix ext, bool trim = false);

  /** Allocate and build a CFG file name from a path.
  @param[in]    path_in         Full path to the filename
  @return own: file name; must be freed by ut::free() */
  [[nodiscard]] static char *make_cfg(const std::string &path_in) {
    return (make(path_in, "", CFG));
  }

  /** Allocate and build a CFP file name from a path.
  @param[in]    path_in         Full path to the filename
  @return own: file name; must be freed by ut::free() */
  [[nodiscard]] static char *make_cfp(const std::string &path_in) {
    return (make(path_in, "", CFP));
  }

  /** Allocate and build a file name from a path, a table or
  tablespace name and a suffix.
  @param[in]    path_in         nullptr or the directory path or
                                  the full path and filename
  @param[in]    name_in         nullptr if path is full, or
                                  Table/Tablespace name
  @return own: file name; must be freed by ut::free() */
  [[nodiscard]] static char *make_ibd(const std::string &path_in,
                                      const std::string &name_in) {
    return (make(path_in, name_in, IBD));
  }

  /** Allocate and build a file name from a path, a table or
  tablespace name and a suffix.
  @param[in]    name_in         Table/Tablespace name
  @return own: file name; must be freed by ut::free() */
  [[nodiscard]] static char *make_ibd_from_table_name(
      const std::string &name_in) {
    return (make("", name_in, IBD));
  }

  /** Create an IBD path name after replacing the basename in an old path
  with a new basename.  The old_path is a full path name including the
  extension.  The tablename is in the normal form "schema/tablename".
  @param[in]    path_in         Pathname
  @param[in]    name_in         Contains new base name
  @param[in]    extn            File extension
  @return new full pathname */
  [[nodiscard]] static std::string make_new_path(const std::string &path_in,
                                                 const std::string &name_in,
                                                 ib_file_suffix extn);

  /** Parse file-per-table file name and build Innodb dictionary table name.
  @param[in]    file_path       File name with complete path
  @param[in]    extn            File extension
  @param[out]   dict_name       Innodb dictionary table name
  @return true, if successful. */
  static bool parse_file_path(const std::string &file_path, ib_file_suffix extn,
                              std::string &dict_name);

  /** This function reduces a null-terminated full remote path name
  into the path that is sent by MySQL for DATA DIRECTORY clause.
  It replaces the 'databasename/tablename.ibd' found at the end of the
  path with just 'tablename'.

  Since the result is always smaller than the path sent in, no new
  memory is allocated. The caller should allocate memory for the path
  sent in. This function manipulates that path in place. If the path
  format is not as expected, set data_dir_path to "" and return.

  The result is used to inform a SHOW CREATE TABLE command.
  @param[in,out]        data_dir_path   Full path/data_dir_path */
  static void make_data_dir_path(char *data_dir_path);

#ifndef UNIV_HOTBACKUP
  /** Check if the filepath provided is in a valid placement.
  This routine is run during file discovery at startup.
  1) File-per-table must be in a dir named for the schema.
  2) File-per-table must not be in the datadir.
  3) General tablespace must not be under the datadir.
  @param[in]    space_name  tablespace name
  @param[in]    space_id    tablespace ID
  @param[in]    fsp_flags   tablespace flags
  @param[in]    path        scanned realpath to an existing file to validate
  @retval true if the filepath is a valid datafile location */
  static bool is_valid_location(const char *space_name, space_id_t space_id,
                                uint32_t fsp_flags, const std::string &path);

  /** Check if the implicit filepath is immediately within a dir named for
  the schema.
  @param[in]    space_name  tablespace name
  @param[in]    path        scanned realpath to an existing file to validate
  @retval true if the filepath is valid */
  static bool is_valid_location_within_db(const char *space_name,
                                          const std::string &path);

  /** Convert filename to the file system charset format.
  @param[in,out]        name            Filename to convert */
  static void convert_to_filename_charset(std::string &name);

  /** Convert to lower case using the file system charset.
  @param[in,out]        path            Filepath to convert */
  static void convert_to_lower_case(std::string &path);

#endif /* !UNIV_HOTBACKUP */

 protected:
  /** Path to a file or directory. */
  std::string m_path;

  /** A full absolute path to the same file. */
  std::string m_abs_path;
};

/** The MySQL server --datadir value */
extern Fil_path MySQL_datadir_path;

/** The MySQL server --innodb-undo-directory value */
extern Fil_path MySQL_undo_path;

/** The undo path is different from any other known directory. */
extern bool MySQL_undo_path_is_unique;

/** Initial size of a single-table tablespace in pages */
constexpr size_t FIL_IBD_FILE_INITIAL_SIZE = 7;
constexpr size_t FIL_IBT_FILE_INITIAL_SIZE = 5;

/** An empty tablespace (CREATE TABLESPACE) has minimum
of 4 pages and an empty CREATE TABLE (file_per_table) has 6 pages.
Minimum of these two is 4 */
constexpr size_t FIL_IBD_FILE_INITIAL_SIZE_5_7 = 4;

/** 'null' (undefined) page offset in the context of file spaces */
constexpr page_no_t FIL_NULL = std::numeric_limits<page_no_t>::max();

/* Space address data type; this is intended to be used when
addresses accurate to a byte are stored in file pages. If the page part
of the address is FIL_NULL, the address is considered undefined. */

/** 'type' definition in C: an address stored in a file page is a
string of bytes */
using fil_faddr_t = byte;

/** File space address */
struct fil_addr_t {
  /* Default constructor */
  fil_addr_t() : page(FIL_NULL), boffset(0) {}

  /** Constructor
  @param[in]    p       Logical page number
  @param[in]    boff    Offset within the page */
  fil_addr_t(page_no_t p, uint32_t boff) : page(p), boffset(boff) {}

  /** Compare to instances
  @param[in]    rhs     Instance to compare with
  @return true if the page number and page offset are equal */
  bool is_equal(const fil_addr_t &rhs) const {
    return (page == rhs.page && boffset == rhs.boffset);
  }

  /** Check if the file address is null.
  @return true if null */
  bool is_null() const { return (page == FIL_NULL && boffset == 0); }

  /** Print a string representation.
  @param[in,out]        out             Stream to write to */
  std::ostream &print(std::ostream &out) const {
    out << "[fil_addr_t: page=" << page << ", boffset=" << boffset << "]";

    return (out);
  }

  /** Page number within a space */
  page_no_t page;

  /** Byte offset within the page */
  uint32_t boffset;
};

/* For printing fil_addr_t to a stream.
@param[in,out]  out             Stream to write to
@param[in]      obj             fil_addr_t instance to write */
inline std::ostream &operator<<(std::ostream &out, const fil_addr_t &obj) {
  return (obj.print(out));
}

/** The null file address */
extern fil_addr_t fil_addr_null;

using page_type_t = uint16_t;

/** File page types (values of FIL_PAGE_TYPE) @{ */
/** B-tree node */
constexpr page_type_t FIL_PAGE_INDEX = 17855;

/** R-tree node */
constexpr page_type_t FIL_PAGE_RTREE = 17854;

/** Tablespace SDI Index page */
constexpr page_type_t FIL_PAGE_SDI = 17853;

/** This page type is unused. */
constexpr page_type_t FIL_PAGE_TYPE_UNUSED = 1;

/** Undo log page */
constexpr page_type_t FIL_PAGE_UNDO_LOG = 2;

/** Index node */
constexpr page_type_t FIL_PAGE_INODE = 3;

/** Insert buffer free list */
constexpr page_type_t FIL_PAGE_IBUF_FREE_LIST = 4;

/* File page types introduced in MySQL/InnoDB 5.1.7 */
/** Freshly allocated page */
constexpr page_type_t FIL_PAGE_TYPE_ALLOCATED = 0;

/** Insert buffer bitmap */
constexpr page_type_t FIL_PAGE_IBUF_BITMAP = 5;

/** System page */
constexpr page_type_t FIL_PAGE_TYPE_SYS = 6;

/** Transaction system data */
constexpr page_type_t FIL_PAGE_TYPE_TRX_SYS = 7;

/** File space header */
constexpr page_type_t FIL_PAGE_TYPE_FSP_HDR = 8;

/** Extent descriptor page */
constexpr page_type_t FIL_PAGE_TYPE_XDES = 9;

/** Uncompressed BLOB page */
constexpr page_type_t FIL_PAGE_TYPE_BLOB = 10;

/** First compressed BLOB page */
constexpr page_type_t FIL_PAGE_TYPE_ZBLOB = 11;

/** Subsequent compressed BLOB page */
constexpr page_type_t FIL_PAGE_TYPE_ZBLOB2 = 12;

/** In old tablespaces, garbage in FIL_PAGE_TYPE is replaced with
this value when flushing pages. */
constexpr page_type_t FIL_PAGE_TYPE_UNKNOWN = 13;

/** Compressed page */
constexpr page_type_t FIL_PAGE_COMPRESSED = 14;

/** Encrypted page */
constexpr page_type_t FIL_PAGE_ENCRYPTED = 15;

/** Compressed and Encrypted page */
constexpr page_type_t FIL_PAGE_COMPRESSED_AND_ENCRYPTED = 16;

/** Encrypted R-tree page */
constexpr page_type_t FIL_PAGE_ENCRYPTED_RTREE = 17;

/** Uncompressed SDI BLOB page */
constexpr page_type_t FIL_PAGE_SDI_BLOB = 18;

/** Compressed SDI BLOB page */
constexpr page_type_t FIL_PAGE_SDI_ZBLOB = 19;

/** Legacy doublewrite buffer page. */
constexpr page_type_t FIL_PAGE_TYPE_LEGACY_DBLWR = 20;

/** Rollback Segment Array page */
constexpr page_type_t FIL_PAGE_TYPE_RSEG_ARRAY = 21;

/** Index pages of uncompressed LOB */
constexpr page_type_t FIL_PAGE_TYPE_LOB_INDEX = 22;

/** Data pages of uncompressed LOB */
constexpr page_type_t FIL_PAGE_TYPE_LOB_DATA = 23;

/** The first page of an uncompressed LOB */
constexpr page_type_t FIL_PAGE_TYPE_LOB_FIRST = 24;

/** The first page of a compressed LOB */
constexpr page_type_t FIL_PAGE_TYPE_ZLOB_FIRST = 25;

/** Data pages of compressed LOB */
constexpr page_type_t FIL_PAGE_TYPE_ZLOB_DATA = 26;

/** Index pages of compressed LOB. This page contains an array of
z_index_entry_t objects.*/
constexpr page_type_t FIL_PAGE_TYPE_ZLOB_INDEX = 27;

/** Fragment pages of compressed LOB. */
constexpr page_type_t FIL_PAGE_TYPE_ZLOB_FRAG = 28;

/** Index pages of fragment pages (compressed LOB). */
constexpr page_type_t FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY = 29;

/** Note the highest valid non-index page_type_t. */
constexpr page_type_t FIL_PAGE_TYPE_LAST = FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY;

/** Check whether the page type is index (Btree or Rtree or SDI) type */
inline bool fil_page_type_is_index(page_type_t page_type) {
  return page_type == FIL_PAGE_INDEX || page_type == FIL_PAGE_SDI ||
         page_type == FIL_PAGE_RTREE;
}

/** Get the file page type.
@param[in]    page    File page
@return page type */
inline page_type_t fil_page_get_type(const byte *page) {
  return (static_cast<page_type_t>(mach_read_from_2(page + FIL_PAGE_TYPE)));
}

/** Check whether the page is index page (either regular Btree index or Rtree
index.
@param[in]  page  page frame whose page type is to be checked. */
inline bool fil_page_index_page_check(const byte *page) {
  const page_type_t type = fil_page_get_type(page);
  const bool is_idx = fil_page_type_is_index(type);
  return is_idx;
}

/** @} */

/** Number of pending tablespace flushes */
extern std::atomic<std::uint64_t> fil_n_pending_tablespace_flushes;

/** Number of files currently open */
extern std::atomic_size_t fil_n_files_open;

/** Look up a tablespace.
The caller should hold an InnoDB table lock or a MDL that prevents
the tablespace from being dropped during the operation,
or the caller should be in single-threaded crash recovery mode
(no user connections that could drop tablespaces).
If this is not the case, fil_space_acquire() and fil_space_release()
should be used instead.
@param[in]      space_id        Tablespace ID
@return tablespace, or nullptr if not found */
[[nodiscard]] fil_space_t *fil_space_get(space_id_t space_id);

#ifndef UNIV_HOTBACKUP
/** Returns the latch of a file space.
@param[in]      space_id        Tablespace ID
@return latch protecting storage allocation */
[[nodiscard]] rw_lock_t *fil_space_get_latch(space_id_t space_id);

#ifdef UNIV_DEBUG
/** Gets the type of a file space.
@param[in]      space_id        Tablespace ID
@return file type */
[[nodiscard]] fil_type_t fil_space_get_type(space_id_t space_id);
#endif /* UNIV_DEBUG */

/** Note that a tablespace has been imported.
It is initially marked as FIL_TYPE_IMPORT so that no logging is
done during the import process when the space ID is stamped to each page.
Now we change it to FIL_TYPE_TABLESPACE to start redo and undo logging.
NOTE: temporary tablespaces are never imported.
@param[in]      space_id        Tablespace ID */
void fil_space_set_imported(space_id_t space_id);
#endif /* !UNIV_HOTBACKUP */

/** Attach a node to a tablespace.
@param[in]      name            file name (file must be closed)
@param[in,out]  space           space where to attach to. Must not be null.
@param[in]      is_raw          true if a raw device or a raw disk partition
@param[in]      max_pages       maximum number of pages in file */
void fil_node_create(const char *name, fil_space_t *space, bool is_raw,
                     page_no_t max_pages);

/** Create a space memory object and put it to the fil_system hash table.
The tablespace name is independent from the tablespace file-name.
Error messages are issued to the server log.
@param[in]      name            Tablespace name
@param[in]      space_id        Tablespace ID
@param[in]      flags           Tablespace flags
@param[in]      purpose         Tablespace purpose
@return pointer to created tablespace, to be filled in with fil_node_create()
@retval nullptr on failure (such as when the same tablespace exists) */
[[nodiscard]] fil_space_t *fil_space_create(const char *name,
                                            space_id_t space_id, uint32_t flags,
                                            fil_type_t purpose);

/** Assigns a new space id for a new single-table tablespace. This works
simply by incrementing the global counter. If 4 billion id's is not enough,
we may need to recycle id's.
@param[out]     space_id                Set this to the new tablespace ID
@return true if assigned, false if not */
[[nodiscard]] bool fil_assign_new_space_id(space_id_t *space_id);

/** Returns the path from the first fil_node_t found with this space ID.
The caller is responsible for freeing the memory allocated here for the
value returned.
@param[in]      space_id        Tablespace ID
@return own: A copy of fil_node_t::path, nullptr if space ID is zero
        or not found. */
[[nodiscard]] char *fil_space_get_first_path(space_id_t space_id);

/** Returns the size of the space in pages. If the tablespace is not present
in the `fil` subsystem mappings, a value of 0 is returned.
@param[in]      space_id        Tablespace ID
@return space size, 0 if space not found */
[[nodiscard]] page_no_t fil_space_get_size(space_id_t space_id);

/** Returns the size of an undo space just after it was initialized.
@param[in]      space_id        Tablespace ID
@return initial space size, 0 if space not found */
[[nodiscard]] page_no_t fil_space_get_undo_initial_size(space_id_t space_id);

/** This is called for an undo tablespace after it has been initialized
or opened.  It sets the minimum size in pages at which it should be truncated
and the number of pages that it should be extended. An undo tablespace is
extended by larger amounts than normal tablespaces. It starts at 16Mb and
is increased during aggressive growth and decreased when the growth is slower.
@param[in]  space_id     Tablespace ID
@param[in]  use_current  If true, use the current size in pages as the initial
                         size. If false, use UNDO_INITIAL_SIZE_IN_PAGES. */
void fil_space_set_undo_size(space_id_t space_id, bool use_current);

/** Returns the flags of the space. The tablespace must be cached
in the memory cache.
@param[in]      space_id        Tablespace ID for which to get the flags
@return flags, ULINT_UNDEFINED if space not found */
[[nodiscard]] uint32_t fil_space_get_flags(space_id_t space_id);

/** Sets the flags of the tablespace. The tablespace must be locked
in MDL_EXCLUSIVE MODE.
@param[in]      space           tablespace in-memory struct
@param[in]      flags           tablespace flags */
void fil_space_set_flags(fil_space_t *space, uint32_t flags);

/** Looks up the tablespace, acquires the space, opens each node of a
tablespace if not already open, validates the first page if it was not
validated yet and fills the FSP-related cache if it was not filled yet. A
`fil_space_release()` needs to be called on the returned space.
@param[in]      space_id        Tablespace ID
@return The opened and acquired space object or an error if tablespace is
missing, being deleted, its nodes can't be opened or validation resulted in
errors. */
[[nodiscard]] ut::Expected<fil_space_t *> fil_space_open(space_id_t space_id);

/** Close each file of a tablespace if open.
@param[in]      space_id        Tablespace ID */
void fil_space_close(space_id_t space_id);

/** Returns the page size of the space and whether it is compressed or not.
The tablespace must be cached in the memory cache.
@param[in]      space_id        Tablespace ID
@param[out]     found           true if tablespace was found
@return page size */
[[nodiscard]] const page_size_t fil_space_get_page_size(space_id_t space_id,
                                                        bool *found);

/** Initializes the tablespace memory cache.
@param[in]      max_n_open      Maximum number of open files */
void fil_init(ulint max_n_open);

/** Changes the maximum opened files limit.
@param[in, out] new_max_open_files New value for the open files limit. If the
limit cannot be changed, the value is changed to a minimum value recommended.
@return true if the new limit was set. */
bool fil_open_files_limit_update(size_t &new_max_open_files);

/** Initializes the tablespace memory cache. */
void fil_close();

/** Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
void fil_close_all_files();

/** Iterate over the files in all the tablespaces. */
class Fil_iterator {
 public:
  using Function = std::function<dberr_t(fil_node_t *)>;

  /** For each data file.
  @param[in]    f               Callback */
  template <typename F>
  static dberr_t for_each_file(F &&f) {
    return iterate([=](fil_node_t *file) { return (f(file)); });
  }

  /** Iterate through all persistent tablespace files (FIL_TYPE_TABLESPACE)
  returning the nodes via callback function f.
  @param[in]    f               Callback
  @return any error returned by the callback function. */
  static dberr_t iterate(Function &&f);
};

/** Sets the max tablespace id counter if the given number is bigger than the
previous value.
@param[in]      max_id          Maximum known tablespace ID */
void fil_set_max_space_id_if_bigger(space_id_t max_id);

#ifndef UNIV_HOTBACKUP

/** Write the flushed LSN to the page header of the first page in the
system tablespace. Assumes the Buffer Pool is empty, or at least does not
contain the first page of the System Tablespace.
@param[in]      lsn             Flushed LSN
@return DB_SUCCESS or error number */
[[nodiscard]] dberr_t fil_write_flushed_lsn(lsn_t lsn);

#else /* !UNIV_HOTBACKUP */
/** Frees a space object from the tablespace memory cache.
Closes a tablespaces' files but does not delete them.
There must not be any pending i/o's or flushes on the files.
@param[in]      space_id        Tablespace ID
@return true if success */
bool meb_fil_space_free(space_id_t space_id);

/** Extends all tablespaces to the size stored in the space header. During the
mysqlbackup --apply-log phase we extended the spaces on-demand so that log
records could be applied, but that may have left spaces still too small
compared to the size stored in the space header. */
void meb_extend_tablespaces_to_stored_len();

/** Process a file name passed as an input
@param[in]      name            absolute path of tablespace file
@param[in]      space_id        the tablespace ID */
void meb_fil_name_process(const char *name, space_id_t space_id);

#endif /* !UNIV_HOTBACKUP */

/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]      space_id        Tablespace ID
@return the tablespace, or nullptr if missing or being deleted */
[[nodiscard]] fil_space_t *fil_space_acquire(space_id_t space_id);

/** Acquire a tablespace that may not exist.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]      space_id        Tablespace ID
@return the tablespace, or nullptr if missing or being deleted */
[[nodiscard]] fil_space_t *fil_space_acquire_silent(space_id_t space_id);

/** Release a tablespace acquired with fil_space_acquire().
@param[in,out]  space   Tablespace to release  */
void fil_space_release(fil_space_t *space);

/** Fetch the file name opened for an undo space number from the file map.
@param[in]   space_num  Undo tablespace Number
@param[out]  space_id   Undo tablespace ID
@param[out]  name       the scanned filename
@return true if the space_num was found. The name is set to an
empty string if the space_num is not found. */
bool fil_system_get_file_by_space_num(space_id_t space_num,
                                      space_id_t &space_id, std::string &name);

/** Basically recreates the tablespace`s only node and set it to the needed
size in place. The content of the prefix up to new size is overwritten with
zeros.
@param[in]      space_id        Tablespace ID to truncate
@param[in]      size_in_pages   Truncate size.
@return true if truncate was successful. */
[[nodiscard]] bool fil_truncate_tablespace(space_id_t space_id,
                                           page_no_t size_in_pages);

/** Closes a single-table tablespace. The tablespace must be cached in the
memory cache. Free all pages used by the tablespace.
@param[in]      space_id        Tablespace ID
@return DB_SUCCESS or error */
[[nodiscard]] dberr_t fil_close_tablespace(space_id_t space_id);

/** Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but

 1. We do not drop the table from the data dictionary;

 2. We remove all insert buffer entries for the tablespace immediately;
    in DROP TABLE they are only removed gradually in the background;

 3. When the user does IMPORT TABLESPACE, the tablespace will have the
    same id as it originally had.

 4. Free all the pages in use by the tablespace if rename=true.
@param[in]      space_id        Tablespace ID
@return DB_SUCCESS or error */
[[nodiscard]] dberr_t fil_discard_tablespace(space_id_t space_id);

/** Test if a tablespace file can be renamed to a new filepath by checking
if that the old filepath exists and the new filepath does not exist.
Any unexpected errors are reported as DB_ERROR.
@param[in]      space_id        Tablespace ID
@param[in]      old_path        Old filepath
@param[in]      new_path        New filepath
@param[in]      is_discarded    Whether the tablespace is discarded
@return innodb error code */
[[nodiscard]] dberr_t fil_rename_tablespace_check(space_id_t space_id,
                                                  const char *old_path,
                                                  const char *new_path,
                                                  bool is_discarded);

/** Rename a single-table tablespace.
The tablespace must exist in the memory cache.
@param[in]      space_id        Tablespace ID
@param[in]      old_path        Old file name
@param[in]      new_name        New tablespace name in the schema/name format
@param[in]      new_path_in     New file name, or nullptr if it is located in
                                the normal data directory
@return InnoDB error code */
[[nodiscard]] dberr_t fil_rename_tablespace(space_id_t space_id,
                                            const char *old_path,
                                            const char *new_name,
                                            const char *new_path_in);

/** Create an IBD tablespace file.
@param[in]      space_id        Tablespace ID
@param[in]      name            Tablespace name in dbname/tablename format.
                                For general tablespaces, the 'dbname/' part
                                may be missing.
@param[in]      path            Path and filename of the datafile to create.
@param[in]      flags           Tablespace flags
@param[in]      size            Initial size of the tablespace file in pages,
                                must be >= FIL_IBD_FILE_INITIAL_SIZE
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_ibd_create(space_id_t space_id, const char *name,
                                     const char *path, uint32_t flags,
                                     page_no_t size);

/** Create a session temporary tablespace (IBT) file.
@param[in]      space_id        Tablespace ID
@param[in]      name            Tablespace name
@param[in]      path            Path and filename of the datafile to create.
@param[in]      flags           Tablespace flags
@param[in]      size            Initial size of the tablespace file in pages,
                                must be >= FIL_IBT_FILE_INITIAL_SIZE
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_ibt_create(space_id_t space_id, const char *name,
                                     const char *path, uint32_t flags,
                                     page_no_t size);

/** Create an UNDO tablespace file.
@param[in]      space_id        Tablespace ID
@param[in]      name            Tablespace name in dbname/tablename format.
@param[in]      path            Path and filename of the datafile to create.
@param[in]      flags           Tablespace flags
@param[in]      size            Initial size of the tablespace file in pages,
                                must be >= FIL_IBD_FILE_INITIAL_SIZE
@param[in]      is_explicit     Enabled if it is an explicit UNDO tablespace
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_undo_create(space_id_t space_id, const char *name,
                                      const char *path, uint32_t flags,
                                      page_no_t size, bool is_explicit);
/** Deletes an IBD  or IBU tablespace.
The tablespace must be cached in the memory cache. This will delete the
datafile, fil_space_t & fil_node_t entries from the file_system_t cache. The
pages of this space that are cached in the BufferPool will remain untouched,
will be discarded lazily when accessed or encountered in lists like flush list
or LRU, as they will be visible as being stale.
@param[in]      space_id        Tablespace ID
@return DB_SUCCESS, DB_TABLESPCE_NOT_FOUND or DB_IO_ERROR */
[[nodiscard]] dberr_t fil_delete_tablespace(space_id_t space_id);

/** Open a single-table tablespace and optionally do some validation such
as checking that the space id is correct. If the file is already open,
the validation will be done before reporting success.
If not successful, print an error message to the error log.
This function is used to open a tablespace when we start up mysqld,
and also in IMPORT TABLESPACE.
NOTE that we assume this operation is used either at the database startup
or under the protection of the dictionary mutex, so that two users cannot
race here.

The fil_node_t::handle will not be left open.

@param[in]      validate        whether we should validate the tablespace
                                (read the first page of the file and
                                check that the space id in it matches id)
@param[in]      purpose         FIL_TYPE_TABLESPACE or FIL_TYPE_TEMPORARY
@param[in]      space_id        Tablespace ID
@param[in]      flags           tablespace flags
@param[in]      space_name      tablespace name of the datafile
                                If file-per-table, it is the table name in the
                                databasename/tablename format
@param[in]      path_in         expected filepath, usually read from dictionary
@param[in]      strict          whether to report error when open ibd failed
@param[in]      old_space       whether it is a 5.7 tablespace opening
                                by upgrade
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_ibd_open(bool validate, fil_type_t purpose,
                                   space_id_t space_id, uint32_t flags,
                                   const char *space_name, const char *path_in,
                                   bool strict, bool old_space);

/** Returns true if a matching tablespace exists in the InnoDB tablespace
memory cache.
@param[in]      space_id        Tablespace ID
@param[in]      name            Tablespace name used in space_create().
@param[in]      print_err       Print detailed error information to the
                                error log if a matching tablespace is
                                not found from memory.
@param[in]      adjust_space    Whether to adjust space id on mismatch
@return true if a matching tablespace exists in the memory cache */
[[nodiscard]] bool fil_space_exists_in_mem(space_id_t space_id,
                                           const char *name, bool print_err,
                                           bool adjust_space);

/** Extends all tablespaces to the size stored in the space header. During the
mysqlbackup --apply-log phase we extended the spaces on-demand so that log
records could be appllied, but that may have left spaces still too small
compared to the size stored in the space header. */
void fil_extend_tablespaces_to_stored_len();

/** Try to extend a tablespace if it is smaller than the specified size.
@param[in,out]  space           Tablespace ID
@param[in]      size            desired size in pages
@return whether the tablespace is at least as big as requested */
[[nodiscard]] bool fil_space_extend(fil_space_t *space, page_no_t size);

/** Tries to reserve free extents in a file space.
@param[in]      space_id        Tablespace ID
@param[in]      n_free_now      Number of free extents now
@param[in]      n_to_reserve    How many one wants to reserve
@return true if succeed */
[[nodiscard]] bool fil_space_reserve_free_extents(space_id_t space_id,
                                                  ulint n_free_now,
                                                  ulint n_to_reserve);

/** Releases free extents in a file space.
@param[in]      space_id        Tablespace ID
@param[in]      n_reserved      How many were reserved */
void fil_space_release_free_extents(space_id_t space_id, ulint n_reserved);

/** Gets the number of reserved extents. If the database is silent, this
number should be zero.
@param[in]      space_id        Tablespace ID
@return the number of reserved extents */
[[nodiscard]] ulint fil_space_get_n_reserved_extents(space_id_t space_id);

/** Read or write data for a single page from a file.
@param[in]      type            IO type
@param[in]      sync            If true then do synchronous IO
@param[in]      page_id         Page id to read or write.
@param[in]      page_size       Structure with information on logical and
                                physical page size in the affected tablespace.
@param[in]      len             Size of the @p buf supplied. It always has to be
  a multiply of OS block size (UNIV_SECTOR_SIZE). In case of write of an already
  compressed data, it is a length of the compressed data buffer. Otherwise it
  should be `page_size.physical()`. Actual number of bytes written can be
  smaller if the tablespace has compression enabled and data was not compressed
  already.
@param[in,out]  buf             A buffer where to store read data or from where
  to write. Data from this buffer might be copied and the copy transformed
  before writing it. It must be aligned in memory to OS block size
  (UNIV_SECTOR_SIZE). Additionally, for reads, it must be aligned to physical
  page size, too. The memory pointed is managed by the caller and must remain
  valid until the call returns if @p sync is true, or until the @p
  postprocess_result callback begins execution.
@param[in]      bpage           Pointer to the page descriptor in the buffer
  pool. This can be nullptr only if we assure the tablespace can't be deleted in
  meantime. If this is nullptr, the page with this page_id can't be in the
  buffer pool, as it risks race condition with a Buffer Pool-induced page flush.
@param[in]      evict_after_write
                                Evicts the page after successful write. It is
                                ignored if @p bpage is nullptr or if it is not a
                                write request.
@param[in]      pre_io_complete_callback
                                A callback to be called exactly once when the
                                result of this IO operation is known. It may be
                                a success if the read or write succeeded or a
                                subset of `dberr_t` errors if the write or read
                                could not be executed or if it failed. It can be
                                called synchronously in this thread before
                                returning from this method, or can be executed
                                asynchronously from another thread, when @p sync
                                is false, before or after this call returns. The
                                `buf_page_io_complete()` is called after this
                                callback returns DB_SUCCESS and @p bpage is not
                                null.
@return error code
@retval DB_SUCCESS on success
@retval DB_TABLESPACE_DELETED if the tablespace does not exist
Note: this is not an exhaustive list of errors returned.*/
[[nodiscard]] dberr_t fil_io(
    IORequest::Type type, bool sync, const page_id_t &page_id,
    const page_size_t &page_size, ulint len, byte *buf, buf_page_t *bpage,
    bool evict_after_write,
    std::function<void(dberr_t err)> pre_io_complete_callback = [](dberr_t) {});

/** Waits for an AIO operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.cc for more info). The thread specifies which
segment it wants to wait for.
@param[in]      segment         The number of the segment in the AIO array
                                to wait for */
void fil_aio_wait(ulint segment);

/** Flushes to disk possible writes cached by the OS. If the space does
not exist or is being dropped, does not do anything.
@param[in]      space_id        Tablespace ID */
void fil_flush(space_id_t space_id);

/** Flush to disk the writes in file spaces possibly cached by the OS
(note: spaces of type FIL_TYPE_TEMPORARY are skipped) */
void fil_flush_file_spaces();

#ifdef UNIV_DEBUG
/** Checks the consistency of the tablespace cache.
@return true if ok */
bool fil_validate();
#endif /* UNIV_DEBUG */

/** Returns true if file address is undefined.
@param[in]      addr            File address to check
@return true if undefined */
[[nodiscard]] bool fil_addr_is_null(const fil_addr_t &addr);

/** Get the predecessor of a file page.
@param[in]      page            File page
@return FIL_PAGE_PREV */
[[nodiscard]] page_no_t fil_page_get_prev(const byte *page);

/** Get the successor of a file page.
@param[in]      page            File page
@return FIL_PAGE_NEXT */
[[nodiscard]] page_no_t fil_page_get_next(const byte *page);

/** Sets the file page type.
@param[in,out]  page            File page
@param[in]      type            File page type to set */
void fil_page_set_type(byte *page, ulint type);

/** Reset the page type.
Data files created before MySQL 5.1 may contain garbage in FIL_PAGE_TYPE.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in]      page_id         Page number
@param[in,out]  page            Page with invalid FIL_PAGE_TYPE
@param[in]      type            Expected page type
@param[in,out]  mtr             Mini-transaction */
void fil_page_reset_type(const page_id_t &page_id, byte *page, ulint type,
                         mtr_t *mtr);

/** Check (and if needed, reset) the page type.
Data files created before MySQL 5.1 may contain
garbage in the FIL_PAGE_TYPE field.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in]      page_id         Page number
@param[in,out]  page            Page with possibly invalid FIL_PAGE_TYPE
@param[in]      type            Expected page type
@param[in,out]  mtr             Mini-transaction */
inline void fil_page_check_type(const page_id_t &page_id, byte *page,
                                ulint type, mtr_t *mtr) {
  ulint page_type = fil_page_get_type(page);

  if (page_type != type) {
    fil_page_reset_type(page_id, page, type, mtr);
  }
}

/** Check (and if needed, reset) the page type.
Data files created before MySQL 5.1 may contain
garbage in the FIL_PAGE_TYPE field.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in,out]  block           Block with possibly invalid FIL_PAGE_TYPE
@param[in]      type            Expected page type
@param[in,out]  mtr             Mini-transaction */
#define fil_block_check_type(block, type, mtr) \
  fil_page_check_type(block->page.id, block->frame, type, mtr)

#ifdef UNIV_DEBUG
/** Increase redo skipped count for a tablespace.
@param[in]      space_id        Tablespace ID */
void fil_space_inc_redo_skipped_count(space_id_t space_id);

/** Decrease redo skipped count for a tablespace.
@param[in]      space_id        Tablespace ID */
void fil_space_dec_redo_skipped_count(space_id_t space_id);

/** Check whether a single-table tablespace is redo skipped.
@param[in]      space_id        Tablespace ID
@return true if redo skipped */
[[nodiscard]] bool fil_space_is_redo_skipped(space_id_t space_id);
#endif /* UNIV_DEBUG */

/** Callback functor. */
struct PageCallback {
  /** Default constructor */
  PageCallback() : m_page_size(0, 0, false), m_filepath() UNIV_NOTHROW {}

  virtual ~PageCallback() UNIV_NOTHROW = default;

  /** Called for page 0 in the tablespace file at the start.
  @param file_size size of the file in bytes
  @param block contents of the first page in the tablespace file
  @retval DB_SUCCESS or error code. */
  [[nodiscard]] virtual dberr_t init(os_offset_t file_size,
                                     const buf_block_t *block) UNIV_NOTHROW = 0;

  /** Called for every page in the tablespace. If the page was not
  updated then its state must be set to BUF_PAGE_NOT_USED. For
  compressed tables the page descriptor memory will be at offset:
  block->frame + UNIV_PAGE_SIZE;
  @param offset physical offset within the file
  @param block block read from file, note it is not from the buffer pool
  @retval DB_SUCCESS or error code. */
  [[nodiscard]] virtual dberr_t operator()(os_offset_t offset,
                                           buf_block_t *block) UNIV_NOTHROW = 0;

  /** Set the name of the physical file and the file handle that is used
  to open it for the file that is being iterated over.
  @param filename then physical name of the tablespace file.
  @param file OS file handle */
  void set_file(const char *filename, pfs_os_file_t file) UNIV_NOTHROW {
    m_file = file;
    m_filepath = filename;
  }

  /** @return the space id of the tablespace */
  [[nodiscard]] virtual space_id_t get_space_id() const UNIV_NOTHROW = 0;

  /**
  @retval the space flags of the tablespace being iterated over */
  [[nodiscard]] virtual ulint get_space_flags() const UNIV_NOTHROW = 0;

  /** Set the tablespace table size.
  @param[in] page a page belonging to the tablespace */
  void set_page_size(const buf_frame_t *page) UNIV_NOTHROW;

  /** The compressed page size
  @return the compressed page size */
  [[nodiscard]] const page_size_t &get_page_size() const {
    return (m_page_size);
  }

  /** The tablespace page size. */
  page_size_t m_page_size;

  /** File handle to the tablespace */
  pfs_os_file_t m_file;

  /** Physical file path. */
  const char *m_filepath;

  // Disable copying
  PageCallback(PageCallback &&) = delete;
  PageCallback(const PageCallback &) = delete;
  PageCallback &operator=(const PageCallback &) = delete;
};

/** Iterate over all the pages in the tablespace.
@param[in]  encryption_metadata the encryption metadata to use for reading
@param[in]  table the table definition in the server
@param[in]  n_io_buffers number of blocks to read and write together
@param[in]  compression_type compression type if compression is enabled,
else Compression::Type::NONE
@param[in,out]  callback functor that will do the page updates
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_tablespace_iterate(
    const Encryption_metadata &encryption_metadata, dict_table_t *table,
    ulint n_io_buffers, Compression::Type compression_type,
    PageCallback &callback);

/** Looks for a pre-existing fil_space_t with the given tablespace ID
and, if found, returns the name and filepath in newly allocated buffers
that the caller must free.
@param[in]      space_id        The tablespace ID to search for.
@param[out]     name            Name of the tablespace found.
@param[out]     filepath        The filepath of the first datafile for the
tablespace.
@return true if tablespace is found, false if not. */
[[nodiscard]] bool fil_space_read_name_and_filepath(space_id_t space_id,
                                                    std::string &name,
                                                    std::string &filepath);

/** Convert a file path to a tablespace name, based on space ID and its flags.
@param[in]      file_path  Full path to the tablespace file.
@param[in]      space_id   space_id emitted by the redo log
@param[in]      flags      FSP flags of the tablespace. */
[[nodiscard]] std::string fil_path_to_space_name(const std::string &file_path,
                                                 space_id_t space_id,
                                                 uint32_t flags);

/** Convert a file name to a tablespace name. Strip the file name
prefix and suffix, leaving only databasename/tablename.
@param[in]      filename        directory/databasename/tablename.ibd
@return database/tablename string */
[[nodiscard]] std::string fil_path_to_space_name(std::string filename);

/** Returns the space ID based on the tablespace name.
The tablespace must be found in the tablespace memory cache.
This call is made from external to this module, so the mutex is not owned.
@param[in]      name            Tablespace name
@return space ID if tablespace found, SPACE_UNKNOWN if space not. */
[[nodiscard]] space_id_t fil_space_get_id_by_name(const char *name);

/** Check if swapping two .ibd files can be done without failure
@param[in]      old_table       old table
@param[in]      new_table       new table
@param[in]      tmp_name        temporary table name
@return innodb error code */
[[nodiscard]] dberr_t fil_rename_precheck(const dict_table_t *old_table,
                                          const dict_table_t *new_table,
                                          const char *tmp_name);

/** Set the compression type for the tablespace
@param[in]      space_id        Space ID of the tablespace
@param[in]      algorithm       Text representation of the algorithm
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_set_compression(space_id_t space_id,
                                          const char *algorithm);

/** Get the compression algorithm for a tablespace.
@param[in]      space_id        Space ID to check
@return the compression algorithm */
[[nodiscard]] Compression::Type fil_get_compression(space_id_t space_id);

/** Set encryption information for IORequest.
@param[in,out]  req_type        IO request
@param[in]      page_id         page id
@param[in]      space           table space */
void fil_io_set_encryption(IORequest &req_type, const page_id_t &page_id,
                           fil_space_t *space);

/** Set the encryption type for the tablespace
@param[in] space_id             Space ID of tablespace for which to set
@param[in] algorithm            Encryption algorithm
@param[in] key                  Encryption key
@param[in] iv                   Encryption iv
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_set_encryption(space_id_t space_id,
                                         Encryption::Type algorithm, byte *key,
                                         byte *iv);

/** Set the autoextend_size attribute for the tablespace
@param[in] space_id             Space ID of tablespace for which to set
@param[in] autoextend_size      Value of autoextend_size attribute
@return DB_SUCCESS or error code */
dberr_t fil_set_autoextend_size(space_id_t space_id, uint64_t autoextend_size);

/** Reset the encryption type for the tablespace
@param[in] space_id             Space ID of tablespace for which to set
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t fil_reset_encryption(space_id_t space_id);

/** Rotate the tablespace keys by new master key.
@return the number of tablespaces that failed to rotate. */
[[nodiscard]] size_t fil_encryption_rotate();

/** Re-encrypt the tablespace keys by current master key. */
void fil_encryption_reencrypt(const std::vector<space_id_t> &sid_vector);

#ifdef UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH
void test_make_filepath();
#endif /* UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH */

/** @return the system tablespace instance */
inline fil_space_t *fil_space_get_sys_space() {
  return fil_space_t::s_sys_space;
}

/** Redo a tablespace create.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@param[in]      parse_only      Don't apply, parse only
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_create(const byte *ptr,
                                                     const byte *end,
                                                     space_id_t space_id,
                                                     bool parse_only);

/** TODO: This is a wrapper for fil_tablespace_redo_create to be called
during recovery, it uses tablespace_scanning to resolve tablespace file
path. The reason it exists is because we want to call
fil_tablespace_redo_create from read replica code, and tablespace_scanning
is not available in this case. OTOH, we have a special tablespace interface
implementation for read replica, and it needs to be investigated further,
how to hide these details behind the tablespace interface.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_create_wrapper(
    const byte *ptr, const byte *end, space_id_t space_id);

/** Redo a tablespace delete.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@param[in]      parse_only      Don't apply, parse only
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_delete(const byte *ptr,
                                                     const byte *end,
                                                     space_id_t space_id,
                                                     bool parse_only);

/** TODO: This is a wrapper for fil_tablespace_redo_delete to be called
during recovery, it uses tablespace_scanning to resolve tablespace file
path. The reason it exists is because we want to call
fil_tablespace_redo_delete from read replica code, and tablespace_scanning
is not available in this case. OTOH, we have a special tablespace interface
implementation for read replica, and it needs to be investigated further,
how to hide these details behind the tablespace interface.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_delete_wrapper(
    const byte *ptr, const byte *end, space_id_t space_id);

/** Redo a tablespace rename.
This function doesn't do anything, simply parses the redo log record.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@param[in]      parse_only      Don't apply, parse only
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_rename(const byte *ptr,
                                                     const byte *end,
                                                     space_id_t space_id,
                                                     bool parse_only);

/** Redo a tablespace extend. if parse_only is false, function expects
tablespace to be present in fil cache.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@param[in]      parse_only      Don't apply the log if true
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_extend(const byte *ptr,
                                                     const byte *end,
                                                     space_id_t space_id,
                                                     bool parse_only);

/** TODO: This is a wrapper for fil_tablespace_redo_extend to be called
during recovery, it makes sure tablespace is loaded in fil cache. The
reason it exists is because we want to call fil_tablespace_redo_extend
from read replica code, and loading tablespace for recovery does not make
any sense in this case. OTOH, we have a special tablespace interface
implementation for read replica, and it needs to be investigated further,
how to hide these details behind the tablespace interface.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        Tablespace Id
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
[[nodiscard]] const byte *fil_tablespace_redo_extend_wrapper(
    const byte *ptr, const byte *end, space_id_t space_id);

/** Parse and process an encryption redo record.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      space_id        the tablespace ID
@param[in]      lsn             lsn for REDO record
@return true to proceed for applying the log record,
otherwise skip applying it the log record. Empty optional
indicates some error in parsing the log record */
[[nodiscard]] std::optional<bool> fil_tablespace_redo_encryption(
    const byte *ptr, const byte *end, space_id_t space_id, lsn_t lsn);

/** Lookup the tablespace ID.
@param[in]      space_id                Tablespace ID to lookup
@return true if the space ID is known. */
[[nodiscard]] bool fil_tablespace_lookup_for_recovery(space_id_t space_id);

/** Add tablespace to the set of tablespaces to be updated in DD.
@param[in]      dd_object_id    Server DD tablespace ID
@param[in]      space_id        Innodb tablespace ID
@param[in]      space_name      New tablespace name
@param[in]      old_path        Old Path in the data dictionary
@param[in]      new_path        New path to be update in dictionary
@param[in]      dd_flag_missing This tablespace is outside default data
                                directory, yet it is missing
                                DD_TABLE_DATA_DIRECTORY flag. This could
                                happen in versions earlier than
                                8.0.38/8.4.1/9.0.0 */
void fil_add_moved_space(dd::Object_id dd_object_id, space_id_t space_id,
                         const char *space_name, const std::string &old_path,
                         const std::string &new_path, bool dd_flag_missing);

/** Lookups the tablespace ID in the scanned tablespaces and return its path to
the file. Compares the absolute base dir (absolute path without filename) of
found scanned path and one supplied as the path from DD for equality. For
example it will return path is matching for `./test/a.ibd` and `./test/b.ibd`.
@param[in]  space_id                tablespace ID to lookup
@param[in]  space_name              tablespace name
@param[in]  fsp_flags               tablespace flags
@param[in]  dirs_in_datadir         full paths for dirs directly under datadir
@param[in]  old_path                the path found in dd:Tablespace_files
@param[out] new_path                the scanned path for this space_id
@return status of the match. */
[[nodiscard]] Fil_state fil_tablespace_dir_equals(
    const space_id_t space_id, const char *space_name, const ulint fsp_flags,
    const Dirs_in_datadir &dirs_in_datadir, const std::string old_path,
    std::string &new_path);

/** Open the tablespace and also get the tablespace filenames, space_id must
already be known.
@param[in]  space_id  Tablespace ID to lookup
@return DB_SUCCESS if open was successful */
[[nodiscard]] dberr_t fil_tablespace_open_for_recovery(space_id_t space_id);

/** Replay a file rename operation for ddl replay.
@param[in]      space_id        Space ID
@param[in]      old_name        old file name
@param[in]      new_name        new file name
@return whether the operation was successfully applied
(the name did not exist, or new_name did not exist and
name was successfully renamed to new_name)  */
[[nodiscard]] bool fil_op_replay_rename_for_ddl(space_id_t space_id,
                                                const char *old_name,
                                                const char *new_name);

/** Free the Tablespace_files instance.
@param[in]      read_only_mode  true if InnoDB is started in read only mode.
@return DB_SUCCESS if all OK */
[[nodiscard]] dberr_t fil_open_for_business(bool read_only_mode);

/** Rename a tablespace.  Use the space_id to find the shard.
@param[in]      space_id        tablespace ID
@param[in]      old_name        old tablespace name
@param[in]      new_name        new tablespace name
@return DB_SUCCESS on success */
[[nodiscard]] dberr_t fil_rename_tablespace_by_id(space_id_t space_id,
                                                  const char *old_name,
                                                  const char *new_name);

/** Write initial pages for a new tablespace file created. Caller of this
function should call fsync() to make the effect of this function durable.
@param[in]      file            open file handle
@param[in]      path            path and filename of the datafile
@param[in]      encrypt_info    encryption key information
@param[in]      space_id        tablespace ID
@param[in,out]  space_flags     tablespace flags
@return DB_SUCCESS on success */
[[nodiscard]] dberr_t fil_write_initial_pages(pfs_os_file_t file,
                                              const char *path,
                                              const byte *encrypt_info,
                                              space_id_t space_id,
                                              uint32_t space_flags);

/** Update the tablespace name. In case, the new name
and old name are same, no update done.
@param[in,out]  space           tablespace object on which name
                                will be updated
@param[in]      name            new name for tablespace */
void fil_space_update_name(fil_space_t *space, const char *name);

/** Adjust file name for import for partition files in different letter case.
@param[in]      table   Innodb dict table
@param[in]      path    file path to open
@param[in]      extn    file extension */
void fil_adjust_name_import(dict_table_t *table, const char *path,
                            ib_file_suffix extn);

#ifndef UNIV_HOTBACKUP

/** Write a log record about an operation on a tablespace file.
@param[in]      type            MLOG_FILE_OPEN or MLOG_FILE_DELETE
                                or MLOG_FILE_CREATE or MLOG_FILE_RENAME
@param[in]      space_id        Tablespace identifier
@param[in]      path            File path
@param[in]      new_path        If type is MLOG_FILE_RENAME, the new name
@param[in]      flags           If type is MLOG_FILE_CREATE, the space flags
@param[in,out]  mtr             Mini-transaction */
void fil_op_write_log(mlog_id_t type, space_id_t space_id, const char *path,
                      const char *new_path, uint32_t flags, mtr_t *mtr);

/** Allows fil system to do periodical cleanup. */
void fil_purge();

/** Count how many truncated undo space IDs are still tracked in
the buffer pool and the file_system cache.
@param[in]  undo_num  undo tablespace number.
@return number of undo tablespaces that are still in memory. */
size_t fil_count_undo_deleted(space_id_t undo_num);

#endif /* !UNIV_HOTBACKUP */

/** Get the page type as a string.
@param[in]  type  page type to be converted to string.
@return the page type as a string. */
[[nodiscard]] const char *fil_get_page_type_str(page_type_t type) noexcept;

/** Check if the given page type is valid.
@param[in]  type  the page type to be checked for validity.
@return true if it is valid page type, false otherwise. */
[[nodiscard]] bool fil_is_page_type_valid(page_type_t type) noexcept;

dberr_t fil_prepare_file_for_io(space_id_t space_id, page_no_t &page_no,
                                fil_node_t **node_out);
void fil_complete_write(space_id_t space_id, fil_node_t *node);

#endif /* fil0fil_h */
