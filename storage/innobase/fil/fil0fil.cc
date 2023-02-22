/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file fil/fil0fil.cc
The tablespace memory cache */

#include <fil0fil.h>
#include "my_config.h"

#include "detail/fil/open_files_limit.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include "arch0page.h"
#include "btr0btr.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "dict0boot.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "fsp0file.h"
#include "fsp0fsp.h"
#include "fsp0space.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "hash0hash.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "log0recv.h"
#include "log0write.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "mtr0log.h"
#include "my_dbug.h"
#include "ut0new.h"

#include "clone0api.h"
#include "os0file.h"
#include "page0zip.h"
#include "sql/mysqld.h"  // lower_case_file_system
#include "srv0srv.h"
#include "srv0start.h"

#ifndef UNIV_HOTBACKUP
#include "buf0lru.h"
#include "ibuf0ibuf.h"
#include "os0event.h"
#include "row0mysql.h"
#include "sql_backup_lock.h"
#include "sql_class.h"
#include "sync0sync.h"
#include "trx0purge.h"
#else /* !UNIV_HOTBACKUP */
#include <cstring>
#include "srv0srv.h"
#endif /* !UNIV_HOTBACKUP */

#include "os0thread-create.h"

#include "current_thd.h"
#include "ha_prototypes.h"

#include <array>
#include <fstream>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>

using Dirs = std::vector<std::string>;
using Space_id_set = std::set<space_id_t>;

dberr_t dict_stats_rename_table(const char *old_name, const char *new_name,
                                char *errstr, size_t errstr_sz);

/** Used for collecting the data in boot_tablespaces() */
namespace dd_fil {

enum {
  /** DD Object ID */
  OBJECT_ID,

  /** InnoDB tablspace ID */
  SPACE_ID,

  /** DD/InnoDB tablespace name */
  SPACE_NAME,

  /** Path in DD tablespace */
  OLD_PATH,

  /** Path where it was found during the scan. */
  NEW_PATH
};

using Moved = std::tuple<dd::Object_id, space_id_t, std::string, std::string,
                         std::string>;

using Tablespaces = std::vector<Moved>;
}  // namespace dd_fil

size_t fil_get_scan_threads(size_t num_files) {
  /* Number of additional threads required to scan all the files.
  n_threads == 0 means that the main thread itself will do all the
  work instead of spawning any additional threads. */
  size_t n_threads = num_files / FIL_SCAN_MAX_TABLESPACES_PER_THREAD;

  /* Return if no additional threads are needed. */
  if (n_threads == 0) {
    return 0;
  }

  /* Number of concurrent threads supported by the host machine. */
  size_t max_threads =
      FIL_SCAN_THREADS_PER_CORE * std::thread::hardware_concurrency();

  /* If the number of concurrent threads supported by the host
  machine could not be calculated, assume the supported threads
  to be FIL_SCAN_MAX_THREADS. */
  max_threads = max_threads == 0 ? FIL_SCAN_MAX_THREADS : max_threads;

  /* Restrict the number of threads to the lower of number of threads
  supported by the host machine or FIL_SCAN_MAX_THREADS. */
  if (n_threads > max_threads) {
    n_threads = max_threads;
  }

  if (n_threads > FIL_SCAN_MAX_THREADS) {
    n_threads = FIL_SCAN_MAX_THREADS;
  }

  return n_threads;
}

/* uint16_t is the index into Tablespace_dirs::m_dirs */
using Scanned_files = std::vector<std::pair<uint16_t, std::string>>;

#ifdef UNIV_PFS_IO
mysql_pfs_key_t innodb_tablespace_open_file_key;
#endif /* UNIV_PFS_IO */

/** System tablespace. */
fil_space_t *fil_space_t::s_sys_space;

#ifdef UNIV_HOTBACKUP
/** Directories in which remote general tablespaces have been found in the
target directory during apply log operation */
Dir_set rem_gen_ts_dirs;

/** true in case the apply-log operation is being performed
in the data directory */
bool replay_in_datadir = false;

/* Re-define mutex macros to use the Mutex class defined by the MEB
source. MEB calls the routines in "fil0fil.cc" in parallel and,
therefore, the mutex protecting the critical sections of the tablespace
memory cache must be included also in the MEB compilation of this
module. */
#undef mutex_create
#undef mutex_free
#undef mutex_enter
#undef mutex_exit
#undef mutex_own
#undef mutex_validate

#define mutex_create(I, M) new (M) meb::Mutex()
#define mutex_free(M) delete (M)
#define mutex_enter(M) (M)->lock()
#define mutex_exit(M) (M)->unlock()
#define mutex_own(M) 1
#define mutex_validate(M) 1

/** Process a MLOG_FILE_CREATE redo record.
@param[in]      page_id         Page id of the redo log record
@param[in]      flags           Tablespace flags
@param[in]      name            Tablespace filename */
static void meb_tablespace_redo_create(const page_id_t &page_id, uint32_t flags,
                                       const char *name);

/** Process a MLOG_FILE_RENAME redo record.
@param[in]      page_id         Page id of the redo log record
@param[in]      from_name       Tablespace from filename
@param[in]      to_name         Tablespace to filename */
static void meb_tablespace_redo_rename(const page_id_t &page_id,
                                       const char *from_name,
                                       const char *to_name);

/** Process a MLOG_FILE_DELETE redo record.
@param[in]      page_id         Page id of the redo log record
@param[in]      name            Tablespace filename */
static void meb_tablespace_redo_delete(const page_id_t &page_id,
                                       const char *name);

#endif /* UNIV_HOTBACKUP */

/*
                IMPLEMENTATION OF THE TABLESPACE MEMORY CACHE
                =============================================

The tablespace cache is responsible for providing fast read/write access to
tablespaces. File creation and deletion is done in other modules which know
more of the logic of the operation, however.

Only the system  tablespace consists of a list  of files. The size of these
files does not have to be divisible by the database block size, because
we may just leave the last incomplete block unused. When a new file is
appended to the tablespace, the maximum size of the file is also specified.
At the moment, we think that it is best to extend the file to its maximum
size already at the creation of the file, because then we can avoid dynamically
extending the file when more space is needed for the tablespace.

Non system tablespaces contain only a single file.

A block's position in the tablespace is specified with a 32-bit unsigned
integer. The files in the list are thought to be catenated, and the block
corresponding to an address n is the nth block in the catenated file (where
the first block is named the 0th block, and the incomplete block fragments
at the end of files are not taken into account). A tablespace can be extended
by appending a new file at the end of the list.

Our tablespace concept is similar to the one of Oracle.

To have fast access to a tablespace file, we put the data structures to
a hash table. Each tablespace file is given an unique 32-bit identifier,
its tablespace ID.

Some operating systems do not support many open files at the same time, or have
a limit set for user or process. Therefore, we put the open files that can be
easily closed in an LRU-list. If we need to open another file, we may close the
file at the end of the LRU-list. When an I/O-operation is pending on a file, the
file cannot be closed - we take the file nodes with pending I/O-operations out
of the LRU-list and keep a count of pending operations for each such file node.
When an operation completes, we decrement the count and return the file to the
LRU-list if the count drops to zero.

The data structure (Fil_shard) that keeps track of the tablespace ID to
fil_space_t* mapping are hashed on the tablespace ID. The tablespace name to
fil_space_t* mapping is stored in the same shard. A shard tracks the flushing
and open state of a file. When we run out open file handles, we use a ticketing
system to serialize the file open, see Fil_shard::reserve_open_slot() and
Fil_shard::release_open_slot().

When updating the global/shared data in Fil_system acquire the mutexes of
all shards in ascending order. The shard mutex covers the fil_space_t data
members as noted in the fil_space_t and fil_node_t definition. */

/** Reference to the server data directory. */
Fil_path MySQL_datadir_path;

/** Reference to the server undo directory. */
Fil_path MySQL_undo_path;

/** The undo path is different from any other known directory. */
bool MySQL_undo_path_is_unique;

/** Common InnoDB file extensions */
const char *dot_ext[] = {"",     ".ibd", ".cfg",   ".cfp",
                         ".ibt", ".ibu", ".dblwr", ".bdblwr"};

/** Number of pending tablespace flushes */
std::atomic<std::uint64_t> fil_n_pending_tablespace_flushes = 0;

/** Number of files currently open */
std::atomic_size_t fil_n_files_open{0};

enum fil_load_status {
  /** The tablespace file(s) were found and valid. */
  FIL_LOAD_OK,

  /** The name no longer matches space_id */
  FIL_LOAD_ID_CHANGED,

  /** The file(s) were not found */
  FIL_LOAD_NOT_FOUND,

  /** The file(s) were not valid */
  FIL_LOAD_INVALID,

  /** The tablespace file ID in the first page doesn't match
  expected value. */
  FIL_LOAD_MISMATCH,

  /** Doublewrite buffer corruption */
  FIL_LOAD_DBWLR_CORRUPTION
};

/** File operations for tablespace */
enum fil_operation_t {

  /** delete a single-table tablespace */
  FIL_OPERATION_DELETE,

  /** close a single-table tablespace */
  FIL_OPERATION_CLOSE
};

/** The null file address */
fil_addr_t fil_addr_null = {FIL_NULL, 0};

/** Maximum number of pages to read to determine the space ID. */
static const size_t MAX_PAGES_TO_READ = 1;

#ifndef UNIV_HOTBACKUP
/** Maximum number of shards supported. */
static const size_t MAX_SHARDS = 68;

/** Number of undo shards to reserve. */
static const size_t UNDO_SHARDS = 4;

/** The UNDO logs have their own shards (4). */
static const size_t UNDO_SHARDS_START = MAX_SHARDS - UNDO_SHARDS;
#else  /* !UNIV_HOTBACKUP */

/** Maximum number of shards supported. */
static const size_t MAX_SHARDS = 1;

/** The UNDO logs have their own shards (4). */
static const size_t UNDO_SHARDS_START = 0;
#endif /* !UNIV_HOTBACKUP */

/** We want to store the line number from where it was called. */
#define mutex_acquire() acquire(__LINE__)

/** Hash a NUL terminated 'string' */
struct Char_Ptr_Hash {
  /** Hashing function
  @param[in]    ptr             NUL terminated string to hash
  @return the hash */
  size_t operator()(const char *ptr) const {
    return static_cast<size_t>(ut::hash_string(ptr));
  }
};

/** Compare two 'strings' */
struct Char_Ptr_Compare {
  /** Compare two NUL terminated strings
  @param[in]    lhs             Left hand side
  @param[in]    rhs             Right hand side
  @return true if the contents match */
  bool operator()(const char *lhs, const char *rhs) const {
    return (strcmp(lhs, rhs) == 0);
  }
};

/** Tablespace files discovered during startup. */
class Tablespace_files {
 public:
  using Names = std::vector<std::string, ut::allocator<std::string>>;
  using Paths = std::unordered_map<space_id_t, Names>;
  using Undo_num2id = std::unordered_map<space_id_t, space_id_t>;

  /** Default constructor
  @param[in]    dir             Directory that the files are under */
  explicit Tablespace_files(const std::string &dir);

  /** Add a space ID to filename mapping.
  @param[in]    space_id        Tablespace ID
  @param[in]    name            File name.
  @return number of files that map to the space ID */
  [[nodiscard]] size_t add(space_id_t space_id, const std::string &name);

  /** Get the file names that map to a space ID
  @param[in]    space_id        Tablespace ID
  @return the filenames that map to space id */
  [[nodiscard]] Names *find_by_id(space_id_t space_id) {
    ut_ad(space_id != TRX_SYS_SPACE);

    if (undo::is_reserved(space_id)) {
      auto it = m_undo_paths.find(space_id);

      if (it != m_undo_paths.end()) {
        return &it->second;
      }

    } else {
      auto it = m_ibd_paths.find(space_id);

      if (it != m_ibd_paths.end()) {
        return &it->second;
      }
    }

    return nullptr;
  }

  /** Get the file name that maps to an undo space number
  @param[in]   space_num  undo tablespace number
  @param[out]  space_id   undo tablespace ID
  @return the file name that maps to the space number */
  [[nodiscard]] Names *find_by_num(space_id_t space_num, space_id_t &space_id) {
    ut_ad(space_num > 0 && space_num <= FSP_MAX_UNDO_TABLESPACES);
    auto it_nums = m_undo_nums.find(space_num);
    if (it_nums == m_undo_nums.end()) {
      return (nullptr);
    }
    space_id = it_nums->second;

    auto it = m_undo_paths.find(space_id);
    ut_ad(it != m_undo_paths.end());

    return (&it->second);
  }

  /** Remove the entry for the space ID.
  @param[in]    space_id        Tablespace ID mapping to remove
  @return true if erase successful */
  [[nodiscard]] bool erase_path(space_id_t space_id) {
    ut_ad(space_id != TRX_SYS_SPACE);

    if (undo::is_reserved(space_id)) {
      auto n_erased = m_undo_nums.erase(undo::id2num(space_id));
      ut_ad(n_erased == 1);

      n_erased = m_undo_paths.erase(space_id);

      return (n_erased == 1);
    } else {
      auto n_erased = m_ibd_paths.erase(space_id);

      return (n_erased == 1);
    }
  }

  /** Clear all the tablespace data. */
  void clear() {
    m_ibd_paths.clear();
    m_undo_paths.clear();
    m_undo_nums.clear();
  }

  /** @return m_dir */
  const Fil_path &root() const { return m_dir; }

  /** @return the directory path specified by the user. */
  const std::string &path() const { return m_dir.path(); }

 private:
  /* Note:  The file names in m_ibd_paths and m_undo_paths are relative
  to m_real_path. */

  /** Mapping from tablespace ID to data filenames */
  Paths m_ibd_paths;

  /** Mapping from tablespace ID to Undo files */
  Paths m_undo_paths;

  /** Mapping from undo space number to space ID */
  Undo_num2id m_undo_nums;

  /** Top level directory where the above files were found. */
  Fil_path m_dir;
};

/** Directories scanned during startup and the files discovered. */
class Tablespace_dirs {
 public:
  using Result = std::pair<std::string, Tablespace_files::Names *>;

  /** Constructor */
  Tablespace_dirs() : m_dirs(), m_checked() {}

  /** Normalize and save a directory to scan for IBD and IBU datafiles
  before recovery.
  @param[in]  directory    directory to scan for ibd and ibu files
  @param[in]  is_undo_dir  true for an undo directory */
  void set_scan_dir(const std::string &directory, bool is_undo_dir = false);

  /** Normalize and save a list of directories to scan for IBD and IBU
  datafiles before recovery.
  @param[in]  directories  Directories to scan for ibd and ibu files */
  void set_scan_dirs(const std::string &directories);

  /** Discover tablespaces by reading the header from .ibd files.
  @return DB_SUCCESS if all goes well */
  [[nodiscard]] dberr_t scan();

  /** Clear all the tablespace file data but leave the list of
  scanned directories in place. */
  void clear() {
    for (auto &dir : m_dirs) {
      dir.clear();
    }

    m_checked = 0;
  }

  /** Erase a space ID to filename mapping.
  @param[in]    space_id        Tablespace ID to erase
  @return true if successful */
  [[nodiscard]] bool erase_path(space_id_t space_id) {
    for (auto &dir : m_dirs) {
      if (dir.erase_path(space_id)) {
        return true;
      }
    }

    return false;
  }

  /* Find the first matching space ID -> name mapping.
  @param[in]    space_id        Tablespace ID
  @return directory searched and pointer to names that map to the
          tablespace ID */
  [[nodiscard]] Result find_by_id(space_id_t space_id) {
    for (auto &dir : m_dirs) {
      const auto names = dir.find_by_id(space_id);

      if (names != nullptr) {
        return (Result{dir.path(), names});
      }
    }

    return (Result{"", nullptr});
  }

  /* Find the matching space number ->space ID -> name mapping.
  @param[in]   space_num  undo tablespace number
  @param[out]  space_id   undo tablespace ID
  @return directory searched and pointer to name that maps to the
          tablespace number */
  [[nodiscard]] Result find_by_num(space_id_t space_num, space_id_t &space_id) {
    for (auto &dir : m_dirs) {
      const auto names = dir.find_by_num(space_num, space_id);

      if (names != nullptr) {
        return Result{dir.path(), names};
      }
    }

    return Result{"", nullptr};
  }

  /** Determine if this Fil_path contains the path provided.
  @param[in]  path  file or directory path to compare.
  @return true if this Fil_path contains path */
  [[nodiscard]] bool contains(const std::string &path) const {
    const Fil_path descendant{path};

    for (const auto &dir : m_dirs) {
      if (dir.root().is_same_as(descendant) ||
          dir.root().is_ancestor(descendant)) {
        return true;
      }
    }
    return false;
  }

  /** Get the list of directories that InnoDB knows about.
  @return the list of directories 'dir1;dir2;....;dirN' */
  std::string get_dirs() const {
    std::string dirs;

    ut_ad(!m_dirs.empty());

    for (const auto &dir : m_dirs) {
      dirs.append(dir.root());
      dirs.push_back(FIL_PATH_SEPARATOR);
    }

    dirs.pop_back();

    ut_ad(!dirs.empty());

    return dirs;
  }

 private:
  /** Print the duplicate filenames for a tablespace ID to the log
  @param[in]    duplicates      Duplicate tablespace IDs*/
  void print_duplicates(const Space_id_set &duplicates);

  /** first=dir path from the user, second=files found under first. */
  using Scanned = std::vector<Tablespace_files>;

  /** Report a warning that a path is being ignored and include the reason. */
  void warn_ignore(std::string path_in, const char *reason);

  /** Add a single path specification to this list of tablespace directories.
  Convert it to an absolute path. Check if the path is valid.  Ignore
  unreadable, duplicate or invalid directories.
  @param[in]  str  Path specification to tokenize
  @param[in]  is_undo_dir  true for an undo directory */
  void add_path(const std::string &str, bool is_undo_dir = false);

  /** Add a delimited list of path specifications to this list of tablespace
  directories. Convert relative paths to absolute paths. Check if the paths
  are valid.  Ignore unreadable, duplicate or invalid directories.
  @param[in]    str             Path specification to tokenize
  @param[in]    delimiters      Delimiters */
  void add_paths(const std::string &str, const std::string &delimiters);

  using Const_iter = Scanned_files::const_iterator;

  /** Check for duplicate tablespace IDs.
  @param[in]    start           Start of slice
  @param[in]    end             End of slice
  @param[in]    thread_id       Thread ID
  @param[in,out]        mutex           Mutex protecting the global state
  @param[in,out]        unique          To check for duplicates
  @param[in,out]        duplicates      Duplicate space IDs found */
  void duplicate_check(const Const_iter &start, const Const_iter &end,
                       size_t thread_id, std::mutex *mutex,
                       Space_id_set *unique, Space_id_set *duplicates);

 private:
  /** Directories scanned and the files discovered under them. */
  Scanned m_dirs;

  /** Number of files checked. */
  std::atomic_size_t m_checked;
};

/** Determine if space flushing should be disabled, for example when user has
explicitly disabled fsync(). */
static inline bool fil_disable_space_flushing(const fil_space_t *space) {
#ifndef _WIN32
  if (space->purpose == FIL_TYPE_TABLESPACE &&
      srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC) {
    return true;
  }
#endif /* !_WIN32 */
  if (space->purpose == FIL_TYPE_TEMPORARY) {
    return true;
  }
  return false;
}

class Fil_shard {
  using File_list = UT_LIST_BASE_NODE_T(fil_node_t, LRU);
  using Space_list = UT_LIST_BASE_NODE_T(fil_space_t, unflushed_spaces);
  using Spaces = std::unordered_map<space_id_t, fil_space_t *>;

  using Names = std::unordered_map<const char *, fil_space_t *, Char_Ptr_Hash,
                                   Char_Ptr_Compare>;

 public:
  /** Constructor
  @param[in]    shard_id        Shard ID  */
  explicit Fil_shard(size_t shard_id);

  /** Destructor */
  ~Fil_shard() {
    mutex_destroy(&m_mutex);
    ut_a(UT_LIST_GET_LEN(m_LRU) == 0);
    ut_a(UT_LIST_GET_LEN(m_unflushed_spaces) == 0);
  }

  /** @return the shard ID */
  size_t id() const { return m_id; }

  /** Acquire the mutex.
  @param[in]    line    Line number from where it was called */
  void acquire(int line) const {
#ifndef UNIV_HOTBACKUP
    m_mutex.enter(srv_n_spin_wait_rounds, srv_spin_wait_delay, __FILE__, line);
#else
    mutex_enter(&m_mutex);
#endif /* !UNIV_HOTBACKUP */
  }

  /** Release the mutex. */
  void mutex_release() const { mutex_exit(&m_mutex); }

#ifdef UNIV_DEBUG
  /** @return true if the mutex is owned. */
  bool mutex_owned() const { return mutex_own(&m_mutex); }
#endif /* UNIV_DEBUG */

  /** Acquire a tablespace to prevent it from being dropped concurrently.
  The thread must call Fil_shard::fil_space_release() when the operation
  is done.
  @param[in]  space  tablespace  to acquire
  @return true if not space->stop_new_ops */
  bool space_acquire(fil_space_t *space);

  /** Release a tablespace acquired with Fil_shard::space_acquire().
  @param[in,out]  space  tablespace to release */
  void space_release(fil_space_t *space);

  /** Fetch the fil_space_t instance that maps to space_id. Does not look
  through system reserved spaces.
  @param[in]    space_id        Tablespace ID to lookup
  @return tablespace instance or nullptr if not found. */
  [[nodiscard]] fil_space_t *get_space_by_id_from_map(
      space_id_t space_id) const {
    ut_ad(mutex_owned());

    auto it = m_spaces.find(space_id);

    if (it == m_spaces.end()) {
      return nullptr;
    }

    ut_ad(it->second->magic_n == FIL_SPACE_MAGIC_N);
    ut_ad(fsp_is_system_temporary(space_id) || it->second->files.size() == 1);

    return it->second;
  }

  /** Fetch the fil_space_t instance that maps to space_id.
  @param[in]    space_id        Tablespace ID to lookup
  @return tablespace instance or nullptr if not found. */
  fil_space_t *get_space_by_id(space_id_t space_id) const;

  /** Fetch the fil_space_t instance that maps to the name.
  @param[in]    name            Tablespace name to lookup
  @return tablespace instance or nullptr if not found. */
  [[nodiscard]] fil_space_t *get_space_by_name(const char *name) const {
    ut_ad(mutex_owned());

    auto it = m_names.find(name);

    if (it == m_names.end()) {
      return nullptr;
    }

    ut_ad(it->second->magic_n == FIL_SPACE_MAGIC_N);

    return it->second;
  }

  /** Tries to close a file in the shard LRU list.
  The caller must hold the Fil_shard::m_mutex.
  @return true if success, false if should retry later */
  [[nodiscard]] bool close_files_in_LRU();

  /** Remove the file node from the LRU list.
  @param[in,out]        file            File for the tablespace */
  void remove_from_LRU(fil_node_t *file);

  /** Add the file node to the LRU list if required.
  @param[in,out]        file            File for the tablespace */
  void add_to_lru_if_needed(fil_node_t *file);

  /** Open all the system files.
  @param[in]    max_n_open      Maximum number of open files allowed
  @param[in,out]        n_open          Current number of open files */
  void open_system_tablespaces(size_t max_n_open, size_t *n_open);

  /** Close a tablespace file.
  @param[in,out]        file            Tablespace file to close */
  void close_file(fil_node_t *file);

  /** Close a tablespace file based on tablespace ID.
  @param[in]    space_id        Tablespace ID
  @return false if space_id was not found. */
  bool close_file(space_id_t space_id);

  /** Prepare to free a file object from a tablespace
  memory cache.
  @param[in,out]        file    Tablespace file
  @param[in]    space   tablespace */
  void file_close_to_free(fil_node_t *file, fil_space_t *space);

  /** Close all open files. */
  void close_all_files();

#ifndef UNIV_HOTBACKUP
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  /** Check that each fil_space_t::m_n_ref_count in this shard matches the
  number of pages counted in the buffer pool.
  @param[in]  buffer_pool_references  Map of spaces instances to the count
  of their pages in the buffer pool. */
  void validate_space_reference_count(Space_References &buffer_pool_references);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#endif /* !UNIV_HOTBACKUP */

  /** Determine if the tablespace needs encryption rotation.
  @param[in]  space  tablespace to rotate
  @return true if the tablespace needs to be rotated, false if not. */
  bool needs_encryption_rotate(fil_space_t *space);

  /** Rotate the tablespace keys by new master key.
  @param[in,out]  rotate_count  A cumulative count of all tablespaces rotated
  in the Fil_system.
  @return the number of tablespaces that failed to rotate. */
  [[nodiscard]] size_t encryption_rotate(size_t *rotate_count);

  /** Detach a space object from the tablespace memory cache and
  closes the tablespace files but does not delete them.
  There must not be any pending I/O's or flushes on the files.
  @param[in,out]        space           tablespace */
  void space_detach(fil_space_t *space);

  /** Remove the fil_space_t instance from the maps used to search for it.
  @param[in]    space_id        Tablespace ID to remove from maps. */
  void space_remove_from_lookup_maps(space_id_t space_id) {
    ut_ad(mutex_owned());

    auto it = m_spaces.find(space_id);

    if (it != m_spaces.end()) {
      m_names.erase(it->second->name);
      m_spaces.erase(it);
    }
  }

#ifndef UNIV_HOTBACKUP
  /** Move the space to the deleted list and remove from the default
  lookup set.
  @param[in, out] space         Space instance to delete. */
  void space_prepare_for_delete(fil_space_t *space) noexcept {
    mutex_acquire();

    space->set_deleted();

    /* Remove access to the fil_space_t instance. */
    space_remove_from_lookup_maps(space->id);

    m_deleted_spaces.push_back({space->id, space});

    space_detach(space);
    ut_a(space->files.size() == 1);
    ut_a(space->files.front().n_pending_ios == 0);

    mutex_release();
  }

  /** Purge entries from m_deleted_spaces that are no longer referenced by a
  buffer pool page. This is no longer required to be done during checkpoint -
  this is done here for historical reasons - it has to be done periodically
  somewhere. */
  void purge() {
    /* Avoid cleaning up old undo files while this is on. */
    DBUG_EXECUTE_IF("ib_undo_trunc_checkpoint_off", return;);

    mutex_acquire();
    for (auto it = m_deleted_spaces.begin(); it != m_deleted_spaces.end();) {
      auto space = it->second;

      if (space->has_no_references()) {
        ut_a(space->files.size() == 1);
        ut_a(space->files.front().n_pending_ios == 0);

        space_free_low(space);

        it = m_deleted_spaces.erase(it);
      } else {
        ++it;
      }
    }

    mutex_release();
  }

  /** Count how many truncated undo space IDs are still tracked in
  the buffer pool and the file_system cache.
  @param[in]  undo_num  undo tablespace number.
  @return number of undo tablespaces that are still in memory. */
  size_t count_undo_deleted(space_id_t undo_num) noexcept {
    size_t count = 0;

    mutex_acquire();

    for (auto deleted : m_deleted_spaces) {
      if (undo::id2num(deleted.first) == undo_num) {
        count++;
      }
    }

    mutex_release();

    return count;
  }

  /** Check if a particular space_id for a page in the buffer pool has
  been deleted recently.  Its space_id will be found in m_deleted_spaces
  until Fil:shard::checkpoint removes the fil_space_t from Fil_system.
  @param[in]  space_id  Tablespace ID to check.
  @return true if this space_id is in the list of recently deleted spaces. */
  bool is_deleted(space_id_t space_id) {
    bool found = false;

    mutex_acquire();

    for (auto deleted : m_deleted_spaces) {
      if (deleted.first == space_id) {
        found = true;
        break;
      }
    }

    mutex_release();

    return found;
  }

#endif /* !UNIV_HOTBACKUP */

  /** Frees a space object from the tablespace memory cache.
  Closes a tablespaces' files but does not delete them.
  There must not be any pending I/O's or flushes on the files.
  @param[in]    space_id        Tablespace ID
  @return fil_space_t instance on success or nullptr */
  [[nodiscard]] fil_space_t *space_free(space_id_t space_id);

  /** Map the space ID and name to the tablespace instance.
  @param[in]    space           Tablespace instance */
  void space_add(fil_space_t *space);

  /** Prepare to free a file. Remove from the unflushed list
  if there are no pending flushes.
  @param[in,out]        file            File instance to free */
  void prepare_to_free_file(fil_node_t *file);

  /** If the tablespace is on the unflushed list and there
  are no pending flushes then remove from the unflushed list.
  @param[in,out]        space           Tablespace to remove*/
  void remove_from_unflushed_list(fil_space_t *space);

  /** Updates the data structures when an I/O operation
  finishes. Updates the pending I/O's field in the file
  appropriately.
  @param[in]    file            Tablespace file
  @param[in]    type            Marks the file as modified type == WRITE */
  void complete_io(fil_node_t *file, const IORequest &type);

  /** Prepares a file for I/O. Opens the file if it is closed. Updates the
  pending I/O's field in the file and the system appropriately. Takes the file
  off the LRU list if it is in the LRU list.
  @param[in]    file            Tablespace file for IO
  @return false if the file can't be opened, otherwise true */
  [[nodiscard]] bool prepare_file_for_io(fil_node_t *file);

  /** Remap the tablespace to the new name.
  @param[in]    space           Tablespace instance, with old name.
  @param[in]    new_name        New tablespace name */
  void update_space_name_map(fil_space_t *space, const char *new_name);

  /** Flush to disk the writes in file spaces possibly cached by the OS
  (note: spaces of type FIL_TYPE_TEMPORARY are skipped) */
  void flush_file_spaces();

  /** Try to extend a tablespace if it is smaller than the specified size.
  @param[in,out]        space           tablespace
  @param[in]    size            desired size in pages
  @return whether the tablespace is at least as big as requested */
  [[nodiscard]] bool space_extend(fil_space_t *space, page_no_t size);

  /** Flushes to disk possible writes cached by the OS. If the space does
  not exist or is being dropped, does not do anything.
  @param[in]    space_id        file space ID (id of tablespace of the database)
*/
  void space_flush(space_id_t space_id);

  /** Open a file of a tablespace.
  The caller must own the shard mutex.
  @param[in,out]        file            Tablespace file
  @return false if the file can't be opened, otherwise true */
  [[nodiscard]] bool open_file(fil_node_t *file);

  /** Checks if all the file nodes in a space are flushed. The caller must hold
  the fil_system mutex.
  @param[in]    space           Tablespace to check
  @return true if all are flushed */
  [[nodiscard]] bool space_is_flushed(const fil_space_t *space);

  /** Open each file of a tablespace if not already open.
  @param[in]    space_id        tablespace identifier
  @retval       true    if all file nodes were opened
  @retval       false   on failure */
  [[nodiscard]] bool space_open(space_id_t space_id);

  /** Opens the files associated with a tablespace and returns a
  pointer to the fil_space_t that is in the memory cache associated
  with a space id.
  @param[in]    space_id        Get the tablespace instance or this ID
  @return file_space_t pointer, nullptr if space not found */
  [[nodiscard]] fil_space_t *space_load(space_id_t space_id);

  /** Wait for pending operations on a tablespace to stop.
  @param[in]   space_id  Tablespace ID
  @param[out]  space     tablespace instance in memory
  @param[out]  path      tablespace path
  @return DB_SUCCESS or DB_TABLESPACE_NOT_FOUND. */
  [[nodiscard]] dberr_t wait_for_pending_operations(space_id_t space_id,
                                                    fil_space_t *&space,
                                                    char **path) const;

  /** Rename a single-table tablespace.
  The tablespace must exist in the memory cache.
  @param[in]    space_id        Tablespace ID
  @param[in]    old_path        Old file name
  @param[in]    new_name        New tablespace  name in the schema/space
  @param[in]    new_path_in     New file name, or nullptr if it
                                  is located in the normal data directory
  @return InnoDB error code */
  [[nodiscard]] dberr_t space_rename(space_id_t space_id, const char *old_path,
                                     const char *new_name,
                                     const char *new_path_in);

  /** Deletes an IBD or IBU tablespace.
  The tablespace must be cached in the memory cache. This will delete the
  datafile, fil_space_t & fil_node_t entries from the file_system_t cache.
  @param[in]    space_id        Tablespace ID
  @param[in]    buf_remove      Specify the action to take on the pages
                                  for this table in the buffer pool.
  @return DB_SUCCESS, DB_TABLESPCE_NOT_FOUND or DB_IO_ERROR */
  [[nodiscard]] dberr_t space_delete(space_id_t space_id,
                                     buf_remove_t buf_remove);

  /** Truncate the tablespace to needed size.
  @param[in]    space_id        Tablespace ID to truncate
  @param[in]    size_in_pages   Truncate size.
  @return true if truncate was successful. */
  [[nodiscard]] bool space_truncate(space_id_t space_id,
                                    page_no_t size_in_pages);

  /** Create a space memory object and put it to the fil_system hash table.
  The tablespace name is independent from the tablespace file-name.
  Error messages are issued to the server log.
  @param[in]    name            Tablespace name
  @param[in]    space_id        Tablespace identifier
  @param[in]    flags           Tablespace flags
  @param[in]    purpose         Tablespace purpose
  @return pointer to created tablespace, to be filled in with fil_node_create()
  @retval nullptr on failure (such as when the same tablespace exists) */
  [[nodiscard]] fil_space_t *space_create(const char *name, space_id_t space_id,
                                          uint32_t flags, fil_type_t purpose);

  /** Adjust temporary auto-generated names created during
  file discovery with correct tablespace names from the DD.
  @param[in,out]        space           Tablespace
  @param[in]    dd_space_name   Tablespace name from the DD
  @return true if the tablespace is a general or undo tablespace. */
  bool adjust_space_name(fil_space_t *space, const char *dd_space_name);

  /** Returns true if a matching tablespace exists in the InnoDB
  tablespace memory cache.
  @param[in]    space_id        Tablespace ID
  @param[in]    name            Tablespace name used in fil_space_create().
  @param[in]    print_err       Print detailed error information to the
                                error log if a matching tablespace is
                                not found from memory.
  @param[in]    adjust_space    Whether to adjust space id on mismatch
  @return true if a matching tablespace exists in the memory cache */
  [[nodiscard]] bool space_check_exists(space_id_t space_id, const char *name,
                                        bool print_err, bool adjust_space);

  /** Read or write data. This operation could be asynchronous (aio).
  @param[in]    type            IO context
  @param[in]    sync            whether synchronous aio is desired
  @param[in]    page_id         page id
  @param[in]    page_size       page size
  @param[in]    byte_offset     remainder of offset in bytes; in AIO this must
  be divisible by the OS block size
  @param[in]    len             how many bytes to read or write; this
  must not cross a file boundary; in AIO this must be a block size multiple
  @param[in,out]        buf             buffer where to store read data or from
  where to write; in AIO this must be appropriately aligned
  @param[in]    message         message for AIO handler if !sync, else ignored
  @return error code
  @retval DB_SUCCESS on success
  @retval DB_TABLESPACE_DELETED if the tablespace does not exist */
  [[nodiscard]] dberr_t do_io(const IORequest &type, bool sync,
                              const page_id_t &page_id,
                              const page_size_t &page_size, ulint byte_offset,
                              ulint len, void *buf, void *message);

  /** Iterate through all persistent tablespace files (FIL_TYPE_TABLESPACE)
  returning the nodes via callback function f.
  @param[in]    f               Callback
  @return any error returned by the callback function. */
  [[nodiscard]] dberr_t iterate(Fil_iterator::Function &f);

  /** Open an ibd tablespace and add it to the InnoDB data structures.
  This is similar to fil_ibd_open() except that it is used while
  processing the redo and DDL log, so the data dictionary is not
  available and very little validation is done. The tablespace name
  is extracted from the dbname/tablename.ibd portion of the filename,
  which assumes that the file is a file-per-table tablespace. Any name
  will do for now. General tablespace names will be read from the
  dictionary after it has been recovered. The tablespace flags are read
  at this time from the first page of the file in validate_for_recovery().
  @param[in]    space_id        tablespace ID
  @param[in]    path            path/to/databasename/tablename.ibd
  @param[out]   space           the tablespace, or nullptr on error
  @return status of the operation */
  [[nodiscard]] fil_load_status ibd_open_for_recovery(space_id_t space_id,
                                                      const std::string &path,
                                                      fil_space_t *&space);

  /** Attach a file to a tablespace
  @param[in]    name            file name of a file that is not open
  @param[in]    size            file size in entire database blocks
  @param[in,out]        space           tablespace from fil_space_create()
  @param[in]    is_raw          whether this is a raw device or partition
  @param[in]    punch_hole      true if supported for this file
  @param[in]    atomic_write    true if the file has atomic write enabled
  @param[in]    max_pages       maximum number of pages in file
  @return pointer to the file name
  @retval nullptr if error */
  [[nodiscard]] fil_node_t *create_node(const char *name, page_no_t size,
                                        fil_space_t *space, bool is_raw,
                                        bool punch_hole, bool atomic_write,
                                        page_no_t max_pages = PAGE_NO_MAX);

#ifdef UNIV_DEBUG
  /** Validate a shard. */
  void validate() const;
#endif /* UNIV_DEBUG */

#ifdef UNIV_HOTBACKUP
  /** Extends all tablespaces to the size stored in the space header.
  During the mysqlbackup --apply-log phase we extended the spaces
  on-demand so that log records could be applied, but that may have
  left spaces still too small compared to the size stored in the space
  header. */
  void meb_extend_tablespaces_to_stored_len();
#endif /* UNIV_HOTBACKUP */

  /** Free a tablespace object on which fil_space_detach() was invoked.
  There must not be any pending I/O's or flushes on the files.
  @param[in,out]        space           tablespace */
  static void space_free_low(fil_space_t *&space);

 private:
  /** We keep system tablespace files always open; this is important
  in preventing deadlocks in this module, as a page read completion
  often performs another read from the insert buffer. The insert buffer
  is in tablespace TRX_SYS_SPACE, and we cannot end up waiting in this
  function.
  @param[in]  space_id  Tablespace ID to look up
  @return tablespace instance */
  [[nodiscard]] fil_space_t *get_reserved_space(space_id_t space_id);

  /** Prepare for truncating a single-table tablespace.
  1) Wait for pending operations on the tablespace to stop;
  2) Remove all insert buffer entries for the tablespace;
  @param[in]   space_id  Tablespace ID
  @param[out]  space     Instance that maps to the space ID.
  @return DB_SUCCESS or error */
  [[nodiscard]] dberr_t space_prepare_for_truncate(space_id_t space_id,
                                                   fil_space_t *&space);

  /** Note that a write IO has completed.
  @param[in,out]  file  File on which a write was completed */
  void write_completed(fil_node_t *file);

  /** If the tablespace is not on the unflushed list, add it.
  @param[in,out]        space           Tablespace to add */
  void add_to_unflushed_list(fil_space_t *space);

  /** Check for pending operations.
  @param[in]    space   tablespace
  @param[in]    count   number of attempts so far
  @return 0 if no pending operations else count + 1. */
  [[nodiscard]] ulint space_check_pending_operations(fil_space_t *space,
                                                     ulint count) const;

  /** Check for pending IO.
  @param[in]    space           Tablespace to check
  @param[in]    file            File in space list
  @param[in]    count           number of attempts so far
  @return 0 if no pending else count + 1. */
  [[nodiscard]] ulint check_pending_io(const fil_space_t *space,
                                       const fil_node_t &file,
                                       ulint count) const;

  /** First we open the file in the normal mode, no async I/O here, for
  simplicity. Then do some checks, and close the file again.  NOTE that we
  could not use the simple file read function os_file_read() in Windows
  to read from a file opened for async I/O!
  @param[in,out]        file            Get the size of this file
  @param[in]    read_only_mode  true if read only mode set
  @return DB_SUCCESS or error */
  [[nodiscard]] dberr_t get_file_size(fil_node_t *file, bool read_only_mode);

  /** Get the AIO mode.
  @param[in]    req_type        IO request type
  @param[in]    sync            true if Synchronous IO
  return the AIO mode */
  [[nodiscard]] static AIO_mode get_AIO_mode(const IORequest &req_type,
                                             bool sync);

  /** Get the file name for IO and the local offset within that file.
  @param[in,out]  space     Tablespace for IO
  @param[in,out]  page_no   The relative page number in the file
  @param[out]     file      File node if DB_SUCCESS, NULL if not
  @retval DB_SUCCESS if the file is found with the page_no
  @retval DB_ERROR if the file is not found or does not contain the page.
                   in this case file == nullptr */
  [[nodiscard]] static dberr_t get_file_for_io(fil_space_t *space,
                                               page_no_t *page_no,
                                               fil_node_t *&file);

 private:
  /** Fil_shard ID */
  const size_t m_id;

  /** Tablespace instances hashed on the space id */
  Spaces m_spaces;

  /** Tablespace instances hashed on the space name */
  Names m_names;

#ifndef UNIV_HOTBACKUP
  using Pair = std::pair<space_id_t, fil_space_t *>;
  using Deleted_spaces = std::vector<Pair, ut::allocator<Pair>>;

  /** Deleted tablespaces. All pages for these tablespaces in the buffer pool
  will be passively deleted. They need not be written. Once the reference count
  is zero, this fil_space_t can be deleted from m_deleted_spaces and removed
  from memory. All reads and writes must be done under the shard mutex. */
  Deleted_spaces m_deleted_spaces;
#endif /* !UNIV_HOTBACKUP */

  /** Base node for the LRU list of the most recently used open
  files with no pending I/O's; if we start an I/O on the file,
  we first remove it from this list, and return it to the start
  of the list when the I/O ends; the system tablespace file is
  not put to this list: it is opened after the startup, and kept
  open until shutdown */

  File_list m_LRU;

  /** Base node for the list of those tablespaces whose files contain unflushed
  writes; those spaces have at least one file where modification_counter >
  flush_counter */
  Space_list m_unflushed_spaces;

  /** When we write to a file we increment this by one */
  int64_t m_modification_counter;

  /** Mutex protecting this shard. */
#ifndef UNIV_HOTBACKUP
  mutable ib_mutex_t m_mutex;
#else
  mutable meb::Mutex m_mutex;
#endif /* !UNIV_HOTBACKUP */

  // Disable copying
  Fil_shard(Fil_shard &&) = delete;
  Fil_shard(const Fil_shard &) = delete;
  Fil_shard &operator=(Fil_shard &&) = delete;
  Fil_shard &operator=(const Fil_shard &) = delete;

  friend class Fil_system;
};

/** The tablespace memory cache */
class Fil_system {
 public:
  using Fil_shards = std::vector<Fil_shard *>;

  /** Constructor.
  @param[in]    n_shards        Number of shards to create
  @param[in]    max_open        Maximum number of open files */
  Fil_system(size_t n_shards, size_t max_open);

  /** Destructor */
  ~Fil_system();

  /** Acquire a tablespace when it could be dropped concurrently.
  Used by background threads that do not necessarily hold proper locks
  for concurrency control.
  @param[in]  space_id  Tablespace ID
  @param[in]  silent    Whether to silently ignore missing tablespaces
  @return the tablespace, or nullptr if missing or being deleted */
  fil_space_t *space_acquire(space_id_t space_id, bool silent);

  /** Fetch the file names opened for a space_id during recovery.
  @param[in]    space_id        Tablespace ID to lookup
  @return pair of top level directory scanned and names that map
          to space_id or nullptr if not found. */
  [[nodiscard]] Tablespace_dirs::Result get_scanned_filename_by_space_id(
      space_id_t space_id) {
    return m_dirs.find_by_id(space_id);
  }

  /** Fetch the file name opened for an undo space number.
  @param[in]   space_num  undo tablespace numb er to lookup
  @param[out]  space_id   Tablespace ID found
  @return pair of top level directory scanned and name that maps
          to the space_num or nullptr if not found. */
  [[nodiscard]] Tablespace_dirs::Result get_scanned_filename_by_space_num(
      space_id_t space_num, space_id_t &space_id) {
    return (m_dirs.find_by_num(space_num, space_id));
  }

  /** Fetch the file name opened for a space_id from the file map.
  @param[in]   space_id  tablespace ID
  @param[out]  name      the scanned filename
  @return true if the space_id is found. The name is set to an
  empty string if the space_id is not found. */
  [[nodiscard]] bool get_file_by_space_id(space_id_t space_id,
                                          std::string &name) {
    auto result = get_scanned_filename_by_space_id(space_id);

    if (result.second != nullptr) {
      /* Duplicates should have been sorted out by now. */
      ut_a(result.second->size() == 1);
      name = result.first + result.second->front();
      return true;
    }

    name = "";
    return false;
  }

  /** Fetch the file name opened for an undo space number.
  @param[in]   space_num  undo tablespace number
  @param[out]  space_id   tablespace ID
  @param[out]  name       the scanned filename
  @return true if the space_id is found. The name is set to an
  empty string if the space_id is not found. */
  [[nodiscard]] bool get_file_by_space_num(space_id_t space_num,
                                           space_id_t &space_id,
                                           std::string &name) {
    auto result = get_scanned_filename_by_space_num(space_num, space_id);

    if (result.second != nullptr) {
      /* Duplicates should have been sorted out by now. */
      ut_a(result.second->size() == 1);
      name = result.first + result.second->front();
      return true;
    }

    name = "";
    return false;
  }

  /** Erase a tablespace ID and its mapping from the scanned files.
  @param[in]    space_id        Tablespace ID to erase
  @return true if successful */
  [[nodiscard]] bool erase_path(space_id_t space_id) {
    return m_dirs.erase_path(space_id);
  }

  /** Add file to old file list. The list is used during 5.7 upgrade failure
  to revert back the modified file names. We modify partitioned file names
  to lower case.
  @param[in]    file_path       old file name with path */
  void add_old_file(const std::string &file_path) {
    m_old_paths.push_back(file_path);
  }

  /** Rename partition files during upgrade.
  @param[in]    revert  if true, revert to old names */
  void rename_partition_files(bool revert);

  /** Clear all accumulated old files. */
  void clear_old_files() { m_old_paths.clear(); }

  /** Get the top level directory where this filename was found.
  @param[in]    path            Path to look for.
  @return the top level directory under which this file was found. */
  [[nodiscard]] const std::string &get_root(const std::string &path) const;

  /** Update the DD if any files were moved to a new location.
  Free the Tablespace_files instance.
  @param[in]    read_only_mode  true if InnoDB is started in
                                  read only mode.
  @return DB_SUCCESS if all OK */
  [[nodiscard]] dberr_t prepare_open_for_business(bool read_only_mode);

  /** Flush to disk the writes in file spaces possibly cached by the OS
  (note: spaces of type FIL_TYPE_TEMPORARY are skipped) */
  void flush_file_spaces();

#ifndef UNIV_HOTBACKUP
  /** Clean up the shards. */
  void purge() {
    for (auto shard : m_shards) {
      shard->purge();
    }
  }

  /** Count how many truncated undo space IDs are still tracked in
  the buffer pool and the file_system cache.
  @param[in]  undo_num  undo tablespace number.
  @return number of undo tablespaces that are still in memory. */
  size_t count_undo_deleted(space_id_t undo_num) {
    size_t count = 0;

    for (auto shard : m_shards) {
      count += shard->count_undo_deleted(undo_num);
    }

    return count;
  }

  /** Check if a particular undo space_id for a page in the buffer pool has
  been deleted recently.
  Its space_id will be found in Fil_shard::m_deleted_spaces until
  Fil:shard::checkpoint removes the fil_space_t from Fil_system.
  @param[in]  space_id  Tablespace ID to check.
  @return true if this space_id is in the list of recently deleted spaces. */
  bool is_deleted(space_id_t space_id) noexcept {
    auto shard = shard_by_id(space_id);

    return shard->is_deleted(space_id);
  }
#endif /* !UNIV_HOTBACKUP */

  /** Fetch the fil_space_t instance that maps to the name.
  @param[in]    name            Tablespace name to lookup
  @return tablespace instance or nullptr if not found. */
  [[nodiscard]] fil_space_t *get_space_by_name(const char *name) {
    for (auto shard : m_shards) {
      shard->mutex_acquire();

      auto space = shard->get_space_by_name(name);

      shard->mutex_release();

      if (space != nullptr) {
        return space;
      }
    }

    return nullptr;
  }

  /** Check a space ID against the maximum known tablespace ID.
  @param[in]    space_id        Tablespace ID to check
  @return true if it is > than maximum known tablespace ID. */
  [[nodiscard]] bool is_greater_than_max_id(space_id_t space_id) const {
    ut_ad(mutex_owned_all());

    return space_id > m_max_assigned_id;
  }

  /** Update the maximum known tablespace ID.
  @param[in]    space           Tablespace instance */
  void set_maximum_space_id(const fil_space_t *space) {
    ut_ad(mutex_owned_all());

    if (!m_space_id_reuse_warned) {
      m_space_id_reuse_warned = true;

      ib::warn(ER_IB_MSG_266) << "Allocated tablespace ID " << space->id
                              << " for " << space->name << ", old maximum"
                              << " was " << m_max_assigned_id;
    }

    m_max_assigned_id = space->id;
  }

  /** Update the maximum known space ID if it's smaller than max_id.
  @param[in]    space_id                Value to set if it's greater */
  void update_maximum_space_id(space_id_t space_id) {
    mutex_acquire_all();

    if (is_greater_than_max_id(space_id)) {
      m_max_assigned_id = space_id;
    }

    mutex_release_all();
  }

  /** Assigns a new space id for a new single-table tablespace. This
  works simply by incrementing the global counter. If 4 billion ids
  is not enough, we may need to recycle ids.
  @param[out]   space_id        Set this to the new tablespace ID
  @return true if assigned, false if not */
  [[nodiscard]] bool assign_new_space_id(space_id_t *space_id);

  /** Allows other threads to advance work while we wait for I/Os to complete.
   */
  void wait_while_ios_in_progress() const {
#ifndef UNIV_HOTBACKUP
    /* Wake the I/O-handler threads to make sure pending I/Os are
    performed. */
    os_aio_simulated_wake_handler_threads();
#endif /* !UNIV_HOTBACKUP */
    /* Give CPU to other threads that keep files opened. */
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  /** Tries to close a file in all the LRU lists.
  The caller must hold the mutex.
  @return true if success, false if should retry later */
  [[nodiscard]] bool close_file_in_all_LRU();

  /** Opens all system tablespace data files in all shards. */
  void open_all_system_tablespaces();

  /** Close all open files. */
  void close_all_files();

  /** Returns maximum number of allowed non-LRU files opened for a specified
  open files limit. */
  static size_t get_limit_for_non_lru_files(size_t open_files_limit);

  /** Returns minimum open files limit to be set to allow the specified number
  of non-LRU files opened. This is inverse function for the
  get_limit_for_non_lru_files. */
  size_t get_minimum_limit_for_open_files(
      size_t n_files_not_belonging_in_lru) const;

  /** Changes the maximum opened files limit.
  @param[in,out] new_max_open_files New value for the open files limit. If the
  limit cannot be changed, the value is changed to a minimum value recommended.
  If there are any concurrent calls to set_open_files_limit in progress, setting
  the limit will fail and the new_max_open_files will be set to 0.
  @return true if the new limit was set. */
  bool set_open_files_limit(size_t &new_max_open_files);

  /** Returns maximum number of allowed opened files. */
  size_t get_open_files_limit() const { return m_open_files_limit.get_limit(); }

  /** Iterate through all persistent tablespace files
  (FIL_TYPE_TABLESPACE) returning the nodes via callback function cbk.
  @param[in]    f               Callback
  @return any error returned by the callback function. */
  [[nodiscard]] dberr_t iterate(Fil_iterator::Function &f);

  /** Rotate the tablespace keys by new master key.
  @return the number of tablespaces that failed to rotate. */
  [[nodiscard]] size_t encryption_rotate();

  /** Reencrypt tablespace keys by current master key. */
  void encryption_reencrypt(std::vector<space_id_t> &sid_vector);

  /** Detach a space object from the tablespace memory cache.
  Closes the tablespace files but does not delete them.
  There must not be any pending I/O's or flushes on the files.
  @param[in,out]        space           tablespace */
  void space_detach(fil_space_t *space);

  /** @return the maximum assigned ID so far */
  space_id_t get_max_space_id() const { return m_max_assigned_id; }

  /** Lookup the tablespace ID.
  @param[in]    space_id        Tablespace ID to lookup
  @return true if the space ID is known. */
  [[nodiscard]] bool lookup_for_recovery(space_id_t space_id);

  /** Open a tablespace that has a redo log record to apply.
  @param[in]  space_id    Tablespace ID
  @return DB_SUCCESS if the open was successful */
  [[nodiscard]] dberr_t open_for_recovery(space_id_t space_id);

  /** This function should be called after recovery has completed.
  Check for tablespace files for which we did not see any
  MLOG_FILE_DELETE or MLOG_FILE_RENAME record. These could not
  be recovered.
  @return true if there were some filenames missing for which we had to
          ignore redo log records during the apply phase */
  [[nodiscard]] bool check_missing_tablespaces();

  /** Note that a file has been relocated.
  @param[in]    object_id       Server DD tablespace ID
  @param[in]    space_id        InnoDB tablespace ID
  @param[in]    space_name      Tablespace name
  @param[in]    old_path        Path to the old location
  @param[in]    new_path        Path scanned from disk */
  void moved(dd::Object_id object_id, space_id_t space_id,
             const char *space_name, const std::string &old_path,
             const std::string &new_path) {
    auto tuple =
        std::make_tuple(object_id, space_id, space_name, old_path, new_path);

    m_moved.push_back(tuple);
  }

  /** Check if a path is known to InnoDB.
  @param[in]    path            Path to check
  @return true if path is known to InnoDB */
  bool check_path(const std::string &path) const {
    return m_dirs.contains(path);
  }

  /** Get the list of directories that InnoDB knows about.
  @return the list of directories 'dir1;dir2;....;dirN' */
  std::string get_dirs() const { return m_dirs.get_dirs(); }

  /** Determines if a file belongs to the least-recently-used list.
  @param[in]    space           Tablespace to check
  @return true if the file belongs to fil_system->m_LRU mutex. */
  [[nodiscard]] static bool space_belongs_in_LRU(const fil_space_t *space);

  /** Normalize and save a directory to scan for IBD and IBU datafiles
  before recovery.
  @param[in]  directory    Directory to scan
  @param[in]  is_undo_dir  true for an undo directory */
  void set_scan_dir(const std::string &directory, bool is_undo_dir) {
    m_dirs.set_scan_dir(directory, is_undo_dir);
  }

  /** Normalize and save a list of directories to scan for IBD and IBU
  datafiles before recovery.
  @param[in]  directories  Directories to scan */
  void set_scan_dirs(const std::string &directories) {
    m_dirs.set_scan_dirs(directories);
  }

  /** Scan the directories to build the tablespace ID to file name
  mapping table. */
  dberr_t scan() { return m_dirs.scan(); }

  /** Get the tablespace ID from an .ibd and/or an undo tablespace. If the ID is
  0 on the first page then try finding the ID with Datafile::find_space_id().
  @param[in]    filename        File name to check
  @return s_invalid_space_id if not found, otherwise the space ID */
  [[nodiscard]] static space_id_t get_tablespace_id(
      const std::string &filename);

  /** Fil_shard by space ID.
  @param[in]    space_id        Tablespace ID
  @return reference to the shard */
  [[nodiscard]] Fil_shard *shard_by_id(space_id_t space_id) const {
#ifndef UNIV_HOTBACKUP
    if (fsp_is_undo_tablespace(space_id)) {
      const size_t limit = space_id % UNDO_SHARDS;

      return m_shards[UNDO_SHARDS_START + limit];
    }

    ut_ad(m_shards.size() == MAX_SHARDS);

    return m_shards[space_id % UNDO_SHARDS_START];
#else  /* !UNIV_HOTBACKUP */
    ut_ad(m_shards.size() == 1);

    return m_shards[0];
#endif /* !UNIV_HOTBACKUP */
  }

  /** Acquire all the mutexes. */
  void mutex_acquire_all() const {
#ifdef UNIV_HOTBACKUP
    ut_ad(m_shards.size() == 1);
#endif /* UNIV_HOTBACKUP */

    for (auto shard : m_shards) {
      shard->mutex_acquire();
    }
  }

  /** Release all the mutexes. */
  void mutex_release_all() const {
#ifdef UNIV_HOTBACKUP
    ut_ad(m_shards.size() == 1);
#endif /* UNIV_HOTBACKUP */

    for (auto shard : m_shards) {
      shard->mutex_release();
    }
  }

#ifdef UNIV_DEBUG

  /** Checks the consistency of the tablespace cache.
  @return true if ok */
  [[nodiscard]] bool validate() const;

  /** Check if all mutexes are owned
  @return true if all owned. */
  [[nodiscard]] bool mutex_owned_all() const {
#ifdef UNIV_HOTBACKUP
    ut_ad(m_shards.size() == 1);
#endif /* UNIV_HOTBACKUP */

    for (const auto shard : m_shards) {
      ut_ad(shard->mutex_owned());
    }

    return true;
  }

#endif /* UNIV_DEBUG */

  /** Rename a tablespace.  Use the space_id to find the shard.
  @param[in]    space_id        tablespace ID
  @param[in]    old_name        old tablespace name
  @param[in]    new_name        new tablespace name
  @return DB_SUCCESS on success */
  [[nodiscard]] dberr_t rename_tablespace_name(space_id_t space_id,
                                               const char *old_name,
                                               const char *new_name);

  /** Free the data structures required for recovery. */
  void free_scanned_files() { m_dirs.clear(); }

#ifdef UNIV_HOTBACKUP
  /** Extends all tablespaces to the size stored in the space header.
  During the mysqlbackup --apply-log phase we extended the spaces
  on-demand so that log records could be applied, but that may have
  left spaces still too small compared to the size stored in the space
  header. */
  void meb_extend_tablespaces_to_stored_len() {
    ut_ad(m_shards.size() == 1);

    /* We use a single shard for MEB. */
    auto shard = shard_by_id(SPACE_UNKNOWN);

    shard->mutex_acquire();

    shard->meb_extend_tablespaces_to_stored_len();

    shard->mutex_release();
  }

  /** Process a file name passed as an input
  Wrapper around meb_name_process()
  @param[in,out]        name            absolute path of tablespace file
  @param[in]    space_id        The tablespace ID
  @param[in]    deleted         true if MLOG_FILE_DELETE */
  void meb_name_process(char *name, space_id_t space_id, bool deleted);

#endif /* UNIV_HOTBACKUP */

 private:
  /** Open an ibd tablespace and add it to the InnoDB data structures.
  This is similar to fil_ibd_open() except that it is used while
  processing the redo log, so the data dictionary is not available
  and very little validation is done. The tablespace name is extracted
  from the dbname/tablename.ibd portion of the filename, which assumes
  that the file is a file-per-table tablespace.  Any name will do for
  now.  General tablespace names will be read from the dictionary after
  it has been recovered.  The tablespace flags are read at this time
  from the first page of the file in validate_for_recovery().
  @param[in]    space_id        tablespace ID
  @param[in]    path            path/to/databasename/tablename.ibd
  @param[out]   space           the tablespace, or nullptr on error
  @return status of the operation */
  [[nodiscard]] fil_load_status ibd_open_for_recovery(space_id_t space_id,
                                                      const std::string &path,
                                                      fil_space_t *&space);

 private:
  /** Fil_shards managed */
  Fil_shards m_shards;

  fil::detail::Open_files_limit m_open_files_limit;

  /** Maximum space id in the existing tables, or assigned during
  the time mysqld has been up; at an InnoDB startup we scan the
  data dictionary and set here the maximum of the space id's of
  the tables there */
  space_id_t m_max_assigned_id;

  /** true if fil_space_create() has issued a warning about
  potential space_id reuse */
  bool m_space_id_reuse_warned;

  /** List of tablespaces that have been relocated. We need to
  update the DD when it is safe to do so. */
  dd_fil::Tablespaces m_moved;

  /** Tablespace directories scanned at startup */
  Tablespace_dirs m_dirs;

  /** Old file paths during 5.7 upgrade. */
  std::vector<std::string> m_old_paths;

  /** Next index (modulo number of shards) to try to close a file from the LRU
  list to distribute closures evenly between the shards. */
  std::atomic_size_t m_next_shard_to_close_from_LRU{};

  /** Current number of files that are not belonging in LRU. This includes redo
  and temporary tablespaces, but not files that were temporarily removed from
  the LRU for I/O. */
  std::atomic_size_t m_n_files_not_belonging_in_lru{};

  /** Throttles messages about high files not belonging in LRU count, the
  warning ER_IB_WARN_MANY_NON_LRU_FILES_OPENED. */
  ib::Throttler m_MANY_NON_LRU_FILES_OPENED_throttler{};
  /** Throttles messages about long waiting for opened files limit, the warning
  ER_IB_MSG_TRYING_TO_OPEN_FILE_FOR_LONG_TIME. */
  ib::Throttler m_TRYING_TO_OPEN_FILE_FOR_LONG_TIME_throttler{};
  /** Throttles messages about accessing space that was already removed, the
  warning ACCESSING_NONEXISTINC_SPACE. */
  ib::Throttler m_ACCESSING_NONEXISTINC_SPACE_throttler{};

  // Disable copying
  Fil_system(Fil_system &&) = delete;
  Fil_system(const Fil_system &) = delete;
  Fil_system &operator=(const Fil_system &) = delete;

  friend class Fil_shard;
};

/** The tablespace memory cache. This variable is nullptr before the module is
initialized. */
static Fil_system *fil_system = nullptr;

#ifdef UNIV_HOTBACKUP
static ulint srv_data_read;
static ulint srv_data_written;
#endif /* UNIV_HOTBACKUP */

static bool is_fast_shutdown() {
#ifndef UNIV_HOTBACKUP
  return srv_shutdown_state >= SRV_SHUTDOWN_LAST_PHASE &&
         srv_fast_shutdown >= 2;
#else
  return false;
#endif
}

bool fil_node_t::can_be_closed() const {
  ut_ad(is_open);
  /* We need to wait for the pending extension and I/Os to finish. */
  if (n_pending_ios != 0) {
    return false;
  }
  if (n_pending_flushes != 0) {
    return false;
  }
  if (is_being_extended) {
    return false;
  }
#ifndef UNIV_HOTBACKUP
  /* The file must be flushed, unless we are in very fast shutdown process. */
  if (is_fast_shutdown()) {
    return true;
  }
#endif
  return is_flushed();
}

/** Replay a file rename operation if possible.
@param[in]      page_id         Space ID and first page number in the file
@param[in]      old_name        old file name
@param[in]      new_name        new file name
@return whether the operation was successfully applied (the name did not exist,
or new_name did not exist and name was successfully renamed to new_name) */
[[nodiscard]] static bool fil_op_replay_rename(const page_id_t &page_id,
                                               const std::string &old_name,
                                               const std::string &new_name);

#ifndef UNIV_HOTBACKUP
/** Rename partition file.
@param[in]      old_path        old file path
@param[in]      extn            file extension suffix
@param[in]      revert          if true, rename from new to old file
@param[in]      import          if called during import */
static void fil_rename_partition_file(const std::string &old_path,
                                      ib_file_suffix extn, bool revert,
                                      bool import);
#endif /* !UNIV_HOTBACKUP */

/** Get modified name for partition file. During upgrade we change all
partition files to have lower case separator and partition name.
@param[in]      old_path        old file name and path
@param[in]      extn            file extension suffix
@param[out]     new_path        modified new name for partitioned file
@return true, iff name needs modification. */
static bool fil_get_partition_file(const std::string &old_path,
                                   ib_file_suffix extn, std::string &new_path);

#ifdef UNIV_DEBUG
/** Try fil_validate() every this many times */
static const size_t FIL_VALIDATE_SKIP = 17;
/** Checks the consistency of the tablespace cache some of the time.
@return true if ok or the check was skipped */
static bool fil_validate_skip() {
/** The fil_validate() call skip counter. Use a signed type
because of the race condition below. */
#ifdef UNIV_HOTBACKUP
  static meb::Mutex meb_mutex;

  meb_mutex.lock();
#endif /* UNIV_HOTBACKUP */
  static int fil_validate_count = FIL_VALIDATE_SKIP;

  /* There is a race condition below, but it does not matter,
  because this call is only for heuristic purposes. We want to
  reduce the call frequency of the costly fil_validate() check
  in debug builds. */
  --fil_validate_count;

  if (fil_validate_count > 0) {
#ifdef UNIV_HOTBACKUP
    meb_mutex.unlock();
#endif /* UNIV_HOTBACKUP */
    return true;
  }

  fil_validate_count = FIL_VALIDATE_SKIP;
#ifdef UNIV_HOTBACKUP
  meb_mutex.unlock();
#endif /* UNIV_HOTBACKUP */

  return fil_validate();
}

/** Validate a shard */
void Fil_shard::validate() const {
  mutex_acquire();

  for (auto elem : m_spaces) {
    page_no_t size = 0;
    auto space = elem.second;

    for (const auto &file : space->files) {
      ut_a(file.is_open || !file.n_pending_ios);
      size += file.size;
    }

    ut_a(space->size == size);
  }

  UT_LIST_CHECK(m_LRU);

  for (auto file : m_LRU) {
    ut_a(file->is_open);
    ut_a(file->n_pending_ios == 0);
    ut_a(fil_system->space_belongs_in_LRU(file->space));
  }

  mutex_release();
}

/** Checks the consistency of the tablespace cache.
@return true if ok */
bool Fil_system::validate() const {
  for (const auto shard : m_shards) {
    shard->validate();
  }

  return true;
}
/** Checks the consistency of the tablespace cache.
@return true if ok */
bool fil_validate() { return fil_system->validate(); }
#endif /* UNIV_DEBUG */

/** Constructor.
@param[in]      n_shards        Number of shards to create
@param[in]      max_open        Maximum number of open files */
Fil_system::Fil_system(size_t n_shards, size_t max_open)
    : m_shards(),
      m_open_files_limit(max_open),
      m_max_assigned_id(),
      m_space_id_reuse_warned() {
  for (size_t i = 0; i < n_shards; ++i) {
    auto shard = ut::new_withkey<Fil_shard>(UT_NEW_THIS_FILE_PSI_KEY, i);

    m_shards.push_back(shard);
  }
}

/** Destructor */
Fil_system::~Fil_system() {
  for (auto shard : m_shards) {
    ut::delete_(shard);
  }

  m_shards.clear();
}

/** Determines if a file belongs to the least-recently-used list.
@param[in]      space           Tablespace to check
@return true if the file belongs to fil_system->m_LRU mutex. */
bool Fil_system::space_belongs_in_LRU(const fil_space_t *space) {
  switch (space->purpose) {
    case FIL_TYPE_TABLESPACE:
      return !fsp_is_system_tablespace(space->id) &&
             !fsp_is_undo_tablespace(space->id);

    case FIL_TYPE_TEMPORARY:
    case FIL_TYPE_IMPORT:
      return true;
  }

  ut_d(ut_error);
  ut_o(return false);
}

/** Constructor
@param[in]      shard_id        Shard ID  */
Fil_shard::Fil_shard(size_t shard_id)
    : m_id(shard_id),
      m_spaces(),
      m_names(),
      m_LRU(),
      m_unflushed_spaces(),
      m_modification_counter() {
  mutex_create(LATCH_ID_FIL_SHARD, &m_mutex);
}

/** Map the space ID and name to the tablespace instance.
@param[in]      space           Tablespace instance */
void Fil_shard::space_add(fil_space_t *space) {
  ut_ad(mutex_owned());

  {
    auto it = m_spaces.insert(Spaces::value_type(space->id, space));

    ut_a(it.second);
  }

  {
    auto name = space->name;

    auto it = m_names.insert(Names::value_type(name, space));

    ut_a(it.second);
  }
}

void Fil_shard::add_to_lru_if_needed(fil_node_t *file) {
  ut_ad(mutex_owned());

  if (Fil_system::space_belongs_in_LRU(file->space)) {
    UT_LIST_ADD_FIRST(m_LRU, file);
  }
}

/** Remove the file node from the LRU list.
@param[in,out]  file            File for the tablespace */
void Fil_shard::remove_from_LRU(fil_node_t *file) {
  ut_ad(mutex_owned());

  if (Fil_system::space_belongs_in_LRU(file->space)) {
    /* The file is in the LRU list, remove it */
    ut_ad(ut_list_exists(m_LRU, file));
    UT_LIST_REMOVE(m_LRU, file);
  }
}

/** Close a tablespace file based on tablespace ID.
@param[in]      space_id        Tablespace ID
@return false if space_id was not found. */
bool Fil_shard::close_file(space_id_t space_id) {
  mutex_acquire();

  auto space = get_space_by_id(space_id);

  if (space == nullptr) {
    mutex_release();

    return false;
  }

  for (auto &file : space->files) {
    while (file.is_open && !file.can_be_closed()) {
      mutex_release();

      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      mutex_acquire();
    }

    if (file.is_open) {
      close_file(&file);
    }
  }

  mutex_release();

  return true;
}

/** Remap the tablespace to the new name.
@param[in]      space           Tablespace instance, with old name.
@param[in]      new_name        New tablespace name */
void Fil_shard::update_space_name_map(fil_space_t *space,
                                      const char *new_name) {
  ut_ad(mutex_owned());

  ut_ad(m_spaces.find(space->id) != m_spaces.end());

  m_names.erase(space->name);

  auto it = m_names.insert(Names::value_type(new_name, space));

  ut_a(it.second);
}

/** Check if the basename of a filepath is an undo tablespace name
@param[in]      name    Tablespace name
@return true if it is an undo tablespace name */
bool Fil_path::is_undo_tablespace_name(const std::string &name) {
  if (name.empty()) {
    return false;
  }

  std::string basename = Fil_path::get_basename(name);

  const auto end = basename.end();

  /* 5 is the minimum length for an explicit undo space name.
  It must be at least this long; "_.ibu". */
  if (basename.length() <= strlen(DOT_IBU)) {
    return false;
  }

  /* Implicit undo names can come in two formats: undo_000 and undo000.
  Check for both. */
  size_t u = (*(end - 4) == '_') ? 1 : 0;

  if (basename.length() == sizeof("undo000") - 1 + u &&
      *(end - 7 - u) == 'u' && /* 'u' */
      *(end - 6 - u) == 'n' && /* 'n' */
      *(end - 5 - u) == 'd' && /* 'd' */
      *(end - 4 - u) == 'o' && /* 'o' */
      isdigit(*(end - 3)) &&   /* 'n' */
      isdigit(*(end - 2)) &&   /* 'n' */
      isdigit(*(end - 1))) {   /* 'n' */
    return true;
  }

  if (basename.substr(basename.length() - 4, 4) == DOT_IBU) {
    return true;
  }

  return false;
}

/** Add a space ID to filename mapping.
@param[in]      space_id        Tablespace ID
@param[in]      name            File name.
@return number of files that map to the space ID */
size_t Tablespace_files::add(space_id_t space_id, const std::string &name) {
  ut_a(space_id != TRX_SYS_SPACE);

  Names *names;

  if (undo::is_reserved(space_id)) {
    ut_ad(!Fil_path::has_suffix(IBD, name.c_str()));

    /* Use m_undo_nums to allow a reserved undo space ID
    to be found quickly. */
    space_id_t space_num = undo::id2num(space_id);
    m_undo_nums[space_num] = space_id;

    names = &m_undo_paths[space_id];

  } else {
    ut_ad(!Fil_path::has_suffix(IBU, name.c_str()));

    if (0 == strncmp(name.c_str(), "undo_", 5)) {
      ib::warn(ER_IB_MSG_267) << "Tablespace '" << name << "' naming"
                              << " format is like an undo tablespace"
                              << " but its ID " << space_id << " is not"
                              << " in the undo tablespace range";
    }

    names = &m_ibd_paths[space_id];
  }

  names->push_back(name);

  return names->size();
}

/** Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space.
@param[in]      page_id         page id
@param[in]      page_size       page size
@param[in]      byte_offset     remainder of offset in bytes; in aio this
must be divisible by the OS block size
@param[in]      len             how many bytes to read; this must not cross a
file boundary; in aio this must be a block size multiple
@param[in,out]  buf             buffer where to store data read; in aio this
must be appropriately aligned
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
static dberr_t fil_read(const page_id_t &page_id, const page_size_t &page_size,
                        ulint byte_offset, ulint len, void *buf) {
  return fil_io(IORequestRead, true, page_id, page_size, byte_offset, len, buf,
                nullptr);
}

/** Writes data to a space from a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space.
@param[in]      page_id         page id
@param[in]      page_size       page size
@param[in]      byte_offset     remainder of offset in bytes; in aio this
must be divisible by the OS block size
@param[in]      len             how many bytes to write; this must not cross
a file boundary; in aio this must be a block size multiple
@param[in]      buf             buffer from which to write; in aio this must
be appropriately aligned
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
        I/O on a tablespace which does not exist */
static dberr_t fil_write(const page_id_t &page_id, const page_size_t &page_size,
                         ulint byte_offset, ulint len, void *buf) {
  ut_ad(!srv_read_only_mode);

  return fil_io(IORequestWrite, true, page_id, page_size, byte_offset, len, buf,
                nullptr);
}

/** Look up a tablespace. The caller should hold an InnoDB table lock or
a MDL that prevents the tablespace from being dropped during the operation,
or the caller should be in single-threaded crash recovery mode (no user
connections that could drop tablespaces). If this is not the case,
fil_space_acquire() and fil_space_release() should be used instead.
@param[in]      space_id        Tablespace ID
@return tablespace, or nullptr if not found */
fil_space_t *fil_space_get(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  shard->mutex_release();

  return space;
}

#ifndef UNIV_HOTBACKUP

/** Returns the latch of a file space.
@param[in]      space_id        Tablespace ID
@return latch protecting storage allocation */
rw_lock_t *fil_space_get_latch(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  shard->mutex_release();

  return &space->latch;
}

#ifdef UNIV_DEBUG

/** Gets the type of a file space.
@param[in]      space_id        Tablespace ID
@return file type */
fil_type_t fil_space_get_type(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  auto space = shard->get_space_by_id(space_id);

  shard->mutex_release();

  return space->purpose;
}

#endif /* UNIV_DEBUG */

void fil_space_set_imported(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  ut_ad(space->purpose == FIL_TYPE_IMPORT);
  space->purpose = FIL_TYPE_TABLESPACE;

  shard->mutex_release();
}
#endif /* !UNIV_HOTBACKUP */

/** Checks if all the file nodes in a space are flushed. The caller must hold
the fil_system mutex.
@param[in]      space           Tablespace to check
@return true if all are flushed */
bool Fil_shard::space_is_flushed(const fil_space_t *space) {
  ut_ad(mutex_owned());

  for (const auto &file : space->files) {
    if (!file.is_flushed()) {
      ut_ad(!fil_disable_space_flushing(space));
      return false;
    }
  }

  return true;
}

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)

#include <sys/ioctl.h>

/** FusionIO atomic write control info */
#define DFS_IOCTL_ATOMIC_WRITE_SET _IOW(0x95, 2, uint)

/** Try and enable FusionIO atomic writes.
@param[in] file         OS file handle
@return true if successful */
bool fil_fusionio_enable_atomic_write(pfs_os_file_t file) {
  if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
    uint atomic = 1;

    ut_a(file.m_file != -1);

    if (ioctl(file.m_file, DFS_IOCTL_ATOMIC_WRITE_SET, &atomic) != -1) {
      return true;
    }
  }

  return false;
}
#endif /* !NO_FALLOCATE && UNIV_LINUX */

/** Attach a file to a tablespace
@param[in]      name            file name of a file that is not open
@param[in]      size            file size in entire database blocks
@param[in,out]  space           tablespace from fil_space_create()
@param[in]      is_raw          whether this is a raw device or partition
@param[in]      punch_hole      true if supported for this file
@param[in]      atomic_write    true if the file has atomic write enabled
@param[in]      max_pages       maximum number of pages in file
@return pointer to the file name
@retval nullptr if error */
fil_node_t *Fil_shard::create_node(const char *name, page_no_t size,
                                   fil_space_t *space, bool is_raw,
                                   bool punch_hole, bool atomic_write,
                                   page_no_t max_pages) {
  ut_ad(name != nullptr);
  ut_ad(fil_system != nullptr);

  if (space == nullptr) {
    return nullptr;
  }

  fil_node_t file{};

  file.name = mem_strdup(name);

  ut_a(!is_raw || srv_start_raw_disk_in_use);

  file.sync_event = os_event_create();

  file.is_raw_disk = is_raw;

  file.size = size;

  file.flush_size = size;

  file.magic_n = FIL_NODE_MAGIC_N;

  file.init_size = size;

  file.max_size = max_pages;

  file.space = space;

  os_file_stat_t stat_info = os_file_stat_t();

#ifdef UNIV_DEBUG
  dberr_t err =
#endif /* UNIV_DEBUG */

      os_file_get_status(
          file.name, &stat_info, false,
          fsp_is_system_temporary(space->id) ? true : srv_read_only_mode);

  ut_ad(err == DB_SUCCESS);

  file.block_size = stat_info.block_size;

  /* In this debugging mode, we can overcome the limitation of some
  OSes like Windows that support Punch Hole but have a hole size
  effectively too large.  By setting the block size to be half the
  page size, we can bypass one of the checks that would normally
  turn Page Compression off.  This execution mode allows compression
  to be tested even when full punch hole support is not available. */
  DBUG_EXECUTE_IF(
      "ignore_punch_hole",
      file.block_size = std::min(static_cast<ulint>(stat_info.block_size),
                                 UNIV_PAGE_SIZE / 2););

  if (!IORequest::is_punch_hole_supported() || !punch_hole ||
      file.block_size >= srv_page_size) {
    fil_no_punch_hole(&file);
  } else {
    file.punch_hole = punch_hole;
  }

  file.atomic_write = atomic_write;

  mutex_acquire();

  space->size += size;

  space->files.push_back(file);

  mutex_release();

  ut_a(space->id == TRX_SYS_SPACE || space->purpose == FIL_TYPE_TEMPORARY ||
       space->files.size() == 1);

  return &space->files.front();
}

/** Attach a file to a tablespace. File must be closed.
@param[in]      name            file name (file must be closed)
@param[in]      size            file size in database blocks, rounded
                                downwards to an integer
@param[in,out]  space           space where to append
@param[in]      is_raw          true if a raw device or a raw disk partition
@param[in]      atomic_write    true if the file has atomic write enabled
@param[in]      max_pages       maximum number of pages in file
@return pointer to the file name
@retval nullptr if error */
char *fil_node_create(const char *name, page_no_t size, fil_space_t *space,
                      bool is_raw, bool atomic_write, page_no_t max_pages) {
  auto shard = fil_system->shard_by_id(space->id);

  fil_node_t *file;

  file = shard->create_node(name, size, space, is_raw,
                            IORequest::is_punch_hole_supported(), atomic_write,
                            max_pages);

  return file == nullptr ? nullptr : file->name;
}

dberr_t Fil_shard::get_file_size(fil_node_t *file, bool read_only_mode) {
  bool success;
  fil_space_t *space = file->space;
  ut_ad(mutex_owned());

  do {
    ut_a(!file->is_open);

    file->handle = os_file_create_simple_no_error_handling(
        innodb_data_file_key, file->name, OS_FILE_OPEN, OS_FILE_READ_ONLY,
        read_only_mode, &success);

    if (!success) {
      /* The following call prints an error message */
      os_file_get_last_error(true);

      ib::warn(ER_IB_MSG_268) << "Cannot open '" << file->name
                              << "'."
                                 " Have you deleted .ibd files under a"
                                 " running mysqld server?";

      return DB_ERROR;
    }

  } while (!success);

  os_offset_t size_bytes = os_file_get_size(file->handle);

  ut_a(size_bytes != (os_offset_t)-1);

#ifdef UNIV_HOTBACKUP
  if (space->id == TRX_SYS_SPACE) {
    file->size = (ulint)(size_bytes / UNIV_PAGE_SIZE);
    space->size += file->size;
    os_file_close(file->handle);
    return DB_SUCCESS;
  }
#endif /* UNIV_HOTBACKUP */

  /* Align memory for file I/O if we might have O_DIRECT set */
  const ulint buf_size =
      recv_recovery_is_on() ? (UNIV_PAGE_SIZE * 2) : UNIV_PAGE_SIZE;
  auto page = static_cast<byte *>(ut::aligned_alloc(buf_size, UNIV_PAGE_SIZE));

  ut_ad(page == page_align(page));

  /* Read the first page of the tablespace */

  IORequest request(IORequest::READ);

  dberr_t err = os_file_read_first_page(request, file->name, file->handle, page,
                                        buf_size);

  ut_a(err == DB_SUCCESS);

  os_file_close(file->handle);

  uint32_t flags = fsp_header_get_flags(page);
  space_id_t space_id = fsp_header_get_space_id(page);

  /* To determine if tablespace is from 5.7 or not, we
  rely on SDI flag. For IBDs from 5.7, which are opened
  during import or during upgrade, their initial size
  is lesser than the initial size in 8.0 */
  bool has_sdi = FSP_FLAGS_HAS_SDI(flags);

  uint8_t expected_size =
      has_sdi ? FIL_IBD_FILE_INITIAL_SIZE : FIL_IBD_FILE_INITIAL_SIZE_5_7;

  const page_size_t page_size(flags);

  ulint min_size = expected_size * page_size.physical();

  if (size_bytes < min_size) {
    if (has_sdi) {
      /** Add some tolerance when the tablespace is upgraded. If an empty
      general tablespace is created in 5.7, and then upgraded to 8.0, then
      its size changes from FIL_IBD_FILE_INITIAL_SIZE_5_7 pages to
      FIL_IBD_FILE_INITIAL_SIZE-1. */

      ut_ad(expected_size == FIL_IBD_FILE_INITIAL_SIZE);
      ulint upgrade_size = (expected_size - 1) * page_size.physical();

      if (size_bytes < upgrade_size) {
        ib::error(ER_IB_MSG_269)
            << "The size of tablespace file " << file->name << " is only "
            << size_bytes << ", should be at least " << upgrade_size << "!";

        ut_error;
      }

    } else {
      ib::error(ER_IB_MSG_269)
          << "The size of tablespace file " << file->name << " is only "
          << size_bytes << ", should be at least " << min_size << "!";

      ut_error;
    }
  }

  if (space_id != space->id) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_270)
        << "Tablespace id is " << space->id
        << " in the data dictionary but in file " << file->name << " it is "
        << space_id << "!";
  }

  /* We need to adjust for compressed pages. */
  const page_size_t space_page_size(space->flags);

  if (!page_size.equals_to(space_page_size)) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_271)
        << "Tablespace file " << file->name << " has page size " << page_size
        << " (flags=" << ib::hex(flags) << ") but the data dictionary expects"
        << " page size " << space_page_size
        << " (flags=" << ib::hex(space->flags) << ")!";
  }

  /* Make the SDI flag in space->flags reflect the SDI flag in the header
  page.  Maybe this space was discarded before a reboot and then replaced
  with a 5.7 space that needs to be imported and upgraded. */
  fsp_flags_unset_sdi(space->flags);
  space->flags |= flags & FSP_FLAGS_MASK_SDI;

  /* Data dictionary and tablespace are flushed at different points in
  time. If a crash happens in between, they can have different
  encryption flags as long as the redo log is not replayed. To avoid a
  recovery error due to differing encryption flags, ensure that the
  fil_space_t instance has the same setting as the header page. First
  clear the encryption flag, then set it from the flags found in the
  file. */
  if (recv_recovery_is_on()) {
    fsp_flags_unset_encryption(space->flags);
    space->flags |= flags & FSP_FLAGS_MASK_ENCRYPTION;
  }

  /* Make sure the space_flags are the same as the header page flags. */
  if (space->flags != flags) {
    ib::error(ER_IB_MSG_272, ulong{space->flags}, file->name, ulonglong{flags});
    ut_error;
  }

  {
    page_no_t size = fsp_header_get_field(page, FSP_SIZE);

    page_no_t free_limit;

    free_limit = fsp_header_get_field(page, FSP_FREE_LIMIT);

    ulint free_len;

    free_len = flst_get_len(FSP_HEADER_OFFSET + FSP_FREE + page);

    ut_ad(space->free_limit == 0 || space->free_limit == free_limit);

    ut_ad(space->free_len == 0 || space->free_len == free_len);

    space->size_in_header = size;
    space->free_limit = free_limit;

    ut_a(free_len < std::numeric_limits<uint32_t>::max());

    space->free_len = (uint32_t)free_len;

    /* TODO: Get consistent flag from recovered DD. For that, DD should be
    recovered already. */
    /* Set estimated value for space->compression_type
    during recovery process. */

    if (recv_recovery_is_on() &&
        (Compression::is_compressed_page(page + page_size.physical()) ||
         Compression::is_compressed_encrypted_page(page +
                                                   page_size.physical()))) {
      ut_ad(buf_size >= (UNIV_PAGE_SIZE * 2));
      Compression::meta_t header;
      Compression::deserialize_header(page + page_size.physical(), &header);
      space->compression_type = header.m_algorithm;
    }
  }

  ut::aligned_free(page);

  /* For encrypted tablespace, we need to check the
  encryption key and iv(initial vector) is read. */
  if (FSP_FLAGS_GET_ENCRYPTION(space->flags) && !recv_recovery_is_on() &&
      space->m_encryption_metadata.m_type != Encryption::AES) {
    ib::error(ER_IB_MSG_273, file->name);

    return DB_ERROR;
  }

  if (file->size == 0) {
    ulint extent_size;

    extent_size = page_size.physical() * FSP_EXTENT_SIZE;

#ifndef UNIV_HOTBACKUP
    /* Truncate the size to a multiple of extent size. */
    if (size_bytes >= extent_size) {
      size_bytes = ut_2pow_round(size_bytes, extent_size);
    }
#else /* !UNIV_HOTBACKUP */

    /* After apply-incremental, tablespaces are not
    extended to a whole megabyte. Do not cut off
    valid data. */

#endif /* !UNIV_HOTBACKUP */

    file->size = static_cast<page_no_t>(size_bytes / page_size.physical());

    space->size += file->size;
  }

  return DB_SUCCESS;
}

size_t Fil_system::get_limit_for_non_lru_files(size_t open_files_limit) {
  /* Leave at least 10% of the limit for the LRU files, to not make system too
  inefficient in an edge case there is a lot of non-LRU files causing LRU
  ones to be constantly closed and opened. The absolute minimum would be a
  single slot for LRU files, but it may be very inefficient. Let's make two the
  hard limit. */
  const size_t minimum_limit_left_for_lru_files =
      std::max(2LL, std::llround(0.1 * open_files_limit));
  return open_files_limit - minimum_limit_left_for_lru_files;
}

size_t Fil_system::get_minimum_limit_for_open_files(
    size_t n_files_not_belonging_in_lru) const {
  size_t result = 0;
  /* Start with the most significant bit and iterate till last one. */
  for (size_t current_bit = ~(std::numeric_limits<size_t>::max() >> 1);
       current_bit; current_bit >>= 1) {
    if (get_limit_for_non_lru_files(result + current_bit - 1) <
        n_files_not_belonging_in_lru) {
      result += current_bit;
    }
  }

  return result;
}

bool Fil_shard::open_file(fil_node_t *file) {
  bool success;
  fil_space_t *space = file->space;

  ut_ad(mutex_owned());

  /* This method is not straightforward. The description is included in comments
  to different parts of this function. They are best read one after another
  in order. */

  ut_a(!file->is_open);
  ut_a(file->n_pending_ios == 0);

  const auto start_time = std::chrono::steady_clock::now();

  /* This method first assures we can open a file. This comes down to assuring a
  correct state under locks is present and that opening of this file will not
  exceed any limits. Currently we have two limits for open files:
  - maximum number of opened files - fil_system->m_open_files_limit,
  - maximum number of opened files that can't be closed on request, i.e. are not
  part of open files LRU list.

  We can ignore the latter iff the file will be part of the LRU.
  For each limit we need to comply with, we need to bump the current number of
  files within the limit. If the bump succeeds (results in a current number not
  larger than the limit value), we have a right to open a file.

  When all rights are acquired the `Fil_shard::open_file` opens the file and
  returns true. The file opened will naturally count against the limits. After
  this happens, the current values of files for the limits are decreased only in
  `Fil_shard::close_file`. */

  /* We remember if we have already acquired right to open the file against the
  total open files limit. */
  bool have_right_for_open = false;
  /* As well as the right to open the file against limit for files that do not
  take part in LRU algorithm. If the file takes part in LRU algorithm, this
  is never true. */
  bool have_right_for_open_non_lru = false;

  /* To acquire a right against a limit, we use this helper function. It
  atomically tries to bump the value of supplied reference to a current value
  as long as it is below the limit set. Returns true if the right to open is
  acquired. */
  const auto acquire_right = [](std::atomic<size_t> &counter,
                                size_t limit) -> bool {
    auto current_count = counter.load();
    while (limit > current_count) {
      if (counter.compare_exchange_weak(current_count, current_count + 1)) {
        return true;
      }
    }
    return false;
  };

  /* At any point we can decide to release any rights that we have acquired so
  far. This will make us unable to open the file now. The
  `Fil_shard::open_file()` when returning `false` must assure no rights are
  left unreleased - this helper function helps to assure that. */
  const auto release_rights = [&]() {
    if (have_right_for_open) {
      fil_n_files_open.fetch_sub(1);
      have_right_for_open = false;
    }
    if (have_right_for_open_non_lru) {
      ut_ad(fil_system->m_n_files_not_belonging_in_lru.load() > 0);
      fil_system->m_n_files_not_belonging_in_lru.fetch_sub(1);
      have_right_for_open_non_lru = false;
    }
  };

  /* Helper function: In case of repeated errors, we will delay printing of
  any messages to log by PRINT_INTERVAL_SECS after the method processing
  starts and then print one message per PRINT_INTERVAL_SECS. */
  const auto should_print_message =
      [&start_time](ib::Throttler &throttler) -> bool {
    const auto current_time = std::chrono::steady_clock::now();
    if (current_time - start_time >= PRINT_INTERVAL) {
      return throttler.apply();
    }
    return false;
  };
  /* If this is `false`, the file to open will count against the limit for
  opened files not taking part in the LRU algorithm. We will need to acquire
  a right to open it. */
  const bool belongs_to_lru = Fil_system::space_belongs_in_LRU(file->space);

  /* We remember the current limit for opened files. If it changes while we are
  acquiring the rights, we must ensure we have not caused it to be bumped higher
  than the new limit. This double checking works together with double checking
  in the `Fil_system::set_open_files_limit` to ensure no race conditions are
  possible to leave number of opened files over the limit even in an event of
  changing the limit in parallel.

  The non-LRU files limit can only change when the main limit for open files is
  changed, so we monitor only the main one. */
  auto last_open_file_limit = fil_system->get_open_files_limit();

  /* This is the main loop. It tries to assure all conditions required to open
  the file or causes `open_file` to exit if the file is already opened in
  different thread.

  At this point, start of each loop and upon exit of the loop (either with
  `break` or `return`) the shard's mutex is owned by this thread. However, it
  may be released and re-acquired in meantime, for a while, inside this loop. If
  we decide to release the mutex, we must execute the loop from begin after
  re-acquiring it.

  The following is the list of conditions we must fulfill to allow file to be
  opened:
  1. At any point, if the file becomes open, we just return success. Must be
  done under the mutex.

  2. At any point, if the space becomes deleted, we just return failure. Must be
  done under the mutex.

  3. If the file is locked with `space->prevent_file_open`, then release any
  rights against the limits acquired so far, and wait till the "lock" is
  released. The check must be done under the mutex. But the waiting must be
  executed only when we don't own the mutex - it is required by other thread to
  release the "lock".

  4. Reserve a right within the non-LRU opened files limit. This is conditional
  - required only if the file is supposed to not be placed in the opened files
  LRU. This is lock-free.

  5. Reserve a right within the fil_system->get_open_files_limit(),
  unconditionally.

  6. Check if the limit value for the opened files have not changed. If it did,
  we just release all rights and retry from the beginning.

  7. Only then we can proceed with actually opening the file. */
  for (;;) {
    /* 1. If the file becomes open, we just return success. We own the mutex,
    may have some rights already acquired. */
    ut_ad(mutex_owned());
    if (file->is_open) {
      release_rights();
      return true;
    }

    /* 2. If the space becomes deleted, we just return failure. We own the
    mutex and may have some rights already acquired.*/
    if (space->is_deleted()) {
      release_rights();
      return false;
    }

    /* 3. If the file is locked with `space->prevent_file_open`. */
    if (space->prevent_file_open) {
      /* Someone wants to rename the file. We can't have it opened now.
      Give CPU to other thread that renames the file.  Release any
      rights and the mutex before we go to sleep - we will not need it and
      someone else will be able to get these or use the mutex to change the
      `space->prevent_file_open`. */
      mutex_release();
      release_rights();

      if (should_print_message(
              space->m_prevent_file_open_wait_message_throttler)) {
        ib::warn(ER_IB_MSG_278, space->name,
                 (long long)std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now() - start_time)
                     .count());
      }

#ifndef UNIV_HOTBACKUP
      /* Wake the I/O handler threads to make sure pending I/O's are performed
       */
      os_aio_simulated_wake_handler_threads();

#endif /* UNIV_HOTBACKUP */

      std::this_thread::sleep_for(std::chrono::milliseconds(1));

      mutex_acquire();
      continue;
    }
    /* 4. We try to acquire required right for non-LRU file, if it is not taking
    part in the LRU algorithm. */
    if (!(belongs_to_lru || have_right_for_open_non_lru)) {
      have_right_for_open_non_lru =
          acquire_right(fil_system->m_n_files_not_belonging_in_lru,
                        Fil_system::get_limit_for_non_lru_files(
                            fil_system->get_open_files_limit()));
      if (!have_right_for_open_non_lru) {
        mutex_release();
        if (should_print_message(
                fil_system->m_MANY_NON_LRU_FILES_OPENED_throttler)) {
          ib::warn(ER_IB_WARN_MANY_NON_LRU_FILES_OPENED,
                   fil_system->m_n_files_not_belonging_in_lru.load(),
                   fil_system->get_open_files_limit());
        }
        /* Give CPU to other threads that keep files opened. */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        mutex_acquire();
        continue;
      }
    }

    /* 5. We try to acquire required right to open file. */
    if (!have_right_for_open) {
      have_right_for_open =
          acquire_right(fil_n_files_open, fil_system->get_open_files_limit());
      if (!have_right_for_open) {
        mutex_release();

        if (should_print_message(
                fil_system->m_TRYING_TO_OPEN_FILE_FOR_LONG_TIME_throttler)) {
          ib::warn warning(
              ER_IB_MSG_TRYING_TO_OPEN_FILE_FOR_LONG_TIME,
              static_cast<long long>(
                  std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::steady_clock::now() - start_time)
                      .count()),
              fil_system->get_open_files_limit());
        }

        /* Flush tablespaces so that we can close modified files in the LRU
        list. */
        fil_system->flush_file_spaces();

        if (!fil_system->close_file_in_all_LRU()) {
          fil_system->wait_while_ios_in_progress();
        }
        mutex_acquire();
        continue;
      }
    }

    /* 6. Re-check the open files limit value. This is working in tandem with
    double checking the limits in the `set_open_files_limit()`. Either this
    thread or one executing `set_open_files_limit()` will spot the limit is
    exceeded and rollback to a correct state: either restore limit or release
    rights. */
    if (last_open_file_limit != fil_system->get_open_files_limit()) {
      release_rights();
      last_open_file_limit = fil_system->get_open_files_limit();
      continue;
    }
    /* 7. If we have all required rights, and checked under the mutex the
    file is not open and can be opened, proceed to opening the file. The file
    must be opened before we release the mutex again. */
    break;
  }

  /* We have fulfilled all requirements to actually open the file. */
  ut_ad(mutex_owned());
  ut_ad(!file->is_open);
  ut_ad(!space->prevent_file_open);
  ut_ad(belongs_to_lru || have_right_for_open_non_lru);
  ut_ad(have_right_for_open);

  bool read_only_mode;

  read_only_mode = !fsp_is_system_temporary(space->id) && srv_read_only_mode;

  if (file->size == 0 ||
      (space->size_in_header == 0 && space->purpose == FIL_TYPE_TABLESPACE &&
       file == &space->files.front()
#ifndef UNIV_HOTBACKUP
       && undo::is_active(space->id, false) &&
       srv_startup_is_before_trx_rollback_phase
#endif /* !UNIV_HOTBACKUP */
       )) {
    /* We don't know the file size yet. */
    dberr_t err = get_file_size(file, read_only_mode);

    if (err != DB_SUCCESS) {
      /* Release the rights acquired as we failed to open it in the end.
       */
      release_rights();
      return false;
    }
  }

  /* Open the file for reading and writing, in Windows normally in the
  unbuffered async I/O mode, though global variables may make os_file_create()
  to fall back to the normal file I/O mode. */

  if (file->is_raw_disk) {
    file->handle =
        os_file_create(innodb_data_file_key, file->name, OS_FILE_OPEN_RAW,
                       OS_FILE_AIO, OS_DATA_FILE, read_only_mode, &success);
  } else {
    file->handle =
        os_file_create(innodb_data_file_key, file->name, OS_FILE_OPEN,
                       OS_FILE_AIO, OS_DATA_FILE, read_only_mode, &success);
  }

  if (success) {
    add_to_lru_if_needed(file);
    /* The file is ready for IO. */
    file->is_open = true;
  } else {
    /* Release the rights acquired as we failed to open it in the end. */
    release_rights();
  }

  /* We exit with the mutex acquired. The file is assured to remain open only as
  long as the mutex is held. Calls, like to `prepare_file_for_io()` are
  required to continue to use the file with the mutex released. */
  return success;
}

void Fil_shard::close_file(fil_node_t *file) {
  ut_ad(mutex_owned());

  ut_a(file->can_be_closed());

  bool ret = os_file_close(file->handle);

  ut_a(ret);

  file->handle.m_file = (os_file_t)-1;

  file->is_open = false;

  auto old_files_open_count = fil_n_files_open.fetch_sub(1);
  ut_a(old_files_open_count > 0);

  if (!Fil_system::space_belongs_in_LRU(file->space)) {
    ut_ad(fil_system->m_n_files_not_belonging_in_lru.load() > 0);

    fil_system->m_n_files_not_belonging_in_lru.fetch_sub(1);
  }

  remove_from_LRU(file);
}

bool Fil_shard::close_files_in_LRU() {
  ut_ad(mutex_owned());

  for (auto file = UT_LIST_GET_LAST(m_LRU); file != nullptr;
       file = UT_LIST_GET_PREV(LRU, file)) {
    if (file->can_be_closed()) {
      close_file(file);

      return true;
    }
  }

  return false;
}

bool Fil_system::close_file_in_all_LRU() {
  const auto n_shards = m_shards.size();
  const auto index = m_next_shard_to_close_from_LRU++;
  for (size_t i = 0; i < n_shards; ++i) {
    auto shard = m_shards[(index + i) % n_shards];
    shard->mutex_acquire();

    bool success = shard->close_files_in_LRU();

    shard->mutex_release();

    if (success) {
      return true;
    }
  }

  return false;
}

fil_space_t *Fil_shard::get_space_by_id(space_id_t space_id) const {
  ut_ad(mutex_owned());

  if (space_id == TRX_SYS_SPACE) {
    return fil_space_t::s_sys_space;
  }

  return get_space_by_id_from_map(space_id);
}

/** Prepare to free a file. Remove from the unflushed list if there
are no pending flushes.
@param[in,out]  file            File instance to free */
void Fil_shard::prepare_to_free_file(fil_node_t *file) {
  ut_ad(mutex_owned());

  fil_space_t *space = file->space;

  if (space->is_in_unflushed_spaces && space_is_flushed(space)) {
    space->is_in_unflushed_spaces = false;

    UT_LIST_REMOVE(m_unflushed_spaces, space);
  }
}

/** Prepare to free a file object from a tablespace memory cache.
@param[in,out]  file    Tablespace file
@param[in]      space   tablespace */
void Fil_shard::file_close_to_free(fil_node_t *file, fil_space_t *space) {
  ut_ad(mutex_owned());
  ut_a(file->magic_n == FIL_NODE_MAGIC_N);
  ut_a(file->n_pending_ios == 0);
  ut_a(!file->is_being_extended);
  ut_a(file->space == space);

  if (file->is_open) {
    /* We fool the assertion in Fil_system::close_file() to think
    there are no unflushed modifications in the file */

    file->set_flushed();

    os_event_set(file->sync_event);

    if (fil_disable_space_flushing(space)) {
      ut_ad(!space->is_in_unflushed_spaces);
      ut_ad(space_is_flushed(space));

    } else {
      prepare_to_free_file(file);
    }

    close_file(file);
  }
}

void Fil_shard::space_detach(fil_space_t *space) {
  ut_ad(mutex_owned());

  m_names.erase(space->name);

  if (space->is_in_unflushed_spaces) {
    ut_ad(!fil_disable_space_flushing(space));

    space->is_in_unflushed_spaces = false;

    UT_LIST_REMOVE(m_unflushed_spaces, space);
  }

  ut_a(space->magic_n == FIL_SPACE_MAGIC_N);
  ut_a(space->n_pending_flushes == 0);

  for (auto &file : space->files) {
    file_close_to_free(&file, space);
  }
}

/** Free a tablespace object on which fil_space_detach() was invoked.
There must not be any pending I/O's or flushes on the files.
@param[in,out]  space           tablespace */
void Fil_shard::space_free_low(fil_space_t *&space) {
#ifndef UNIV_HOTBACKUP
  {
    /* Temporary and undo tablespaces IDs are assigned from a large but
    fixed size pool of reserved IDs. Therefore we must ensure that a
    fil_space_t instance can't be dropped until all the pages that point
    to it are also purged from the buffer pool. */

    ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_LAST_PHASE ||
         space->has_no_references());
  }
#endif /* !UNIV_HOTBACKUP */

  for (auto &file : space->files) {
    ut_d(space->size -= file.size);

    os_event_destroy(file.sync_event);

    ut::free(file.name);
  }

  call_destructor(&space->files);

  ut_ad(space->size == 0);

  rw_lock_free(&space->latch);
  ut::free(space->name);
  ut::free(space);

  space = nullptr;
}

/** Frees a space object from the tablespace memory cache.
Closes a tablespaces' files but does not delete them.
There must not be any pending I/O's or flushes on the files.
@param[in]      space_id        Tablespace ID
@return fil_space_t instance on success or nullptr */
fil_space_t *Fil_shard::space_free(space_id_t space_id) {
  mutex_acquire();

  fil_space_t *space = get_space_by_id(space_id);

  if (space != nullptr) {
    space_detach(space);

    space_remove_from_lookup_maps(space_id);
  }

  mutex_release();

  return space;
}

/** Frees a space object from the tablespace memory cache.
Closes a tablespaces' files but does not delete them.
There must not be any pending i/o's or flushes on the files.
@param[in]      space_id        Tablespace ID
@param[in]      x_latched       Whether the caller holds X-mode space->latch
@return true if success */
static bool fil_space_free(space_id_t space_id, bool x_latched) {
  ut_ad(space_id != TRX_SYS_SPACE);

  auto shard = fil_system->shard_by_id(space_id);
  auto space = shard->space_free(space_id);

  if (space == nullptr) {
    return false;
  }

  if (x_latched) {
    rw_lock_x_unlock(&space->latch);
  }

  Fil_shard::space_free_low(space);
  ut_a(space == nullptr);

  return true;
}

#ifdef UNIV_HOTBACKUP
/** Frees a space object from the tablespace memory cache.
Closes a tablespaces' files but does not delete them.
There must not be any pending i/o's or flushes on the files.
@param[in]      space_id        Tablespace ID
@return true if success */
bool meb_fil_space_free(space_id_t space_id) {
  return fil_space_free(space_id, false);
}
#endif /* UNIV_HOTBACKUP */

/** Create a space memory object and put it to the fil_system hash table.
The tablespace name is independent from the tablespace file-name.
Error messages are issued to the server log.
@param[in]      name            Tablespace name
@param[in]      space_id        Tablespace identifier
@param[in]      flags           Tablespace flags
@param[in]      purpose         Tablespace purpose
@return pointer to created tablespace, to be filled in with fil_node_create()
@retval nullptr on failure (such as when the same tablespace exists) */
fil_space_t *Fil_shard::space_create(const char *name, space_id_t space_id,
                                     uint32_t flags, fil_type_t purpose) {
  ut_ad(mutex_owned());

  /* Look for a matching tablespace. */
  fil_space_t *space = get_space_by_name(name);

  if (space == nullptr) {
    space = get_space_by_id(space_id);
  }

  if (space != nullptr) {
    std::ostringstream oss;

    for (size_t i = 0; i < space->files.size(); ++i) {
      oss << "'" << space->files[i].name << "'";

      if (i < space->files.size() - 1) {
        oss << ", ";
      }
    }

    ib::info(ER_IB_MSG_281)
        << "Trying to add tablespace '" << name << "'"
        << " with id " << space_id << " to the tablespace"
        << " memory cache, but tablespace"
        << " '" << space->name << "'"
        << " already exists in the cache with space ID " << space->id
        << ". It maps to the following file(s): " << oss.str();

    return nullptr;
  }

  space = static_cast<fil_space_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*space)));
  /* This could be just a placement new constructor call if, only if it compiles
  OK on SunPro. */
  space->initialize();

  space->id = space_id;
  space->name = mem_strdup(name);

#ifndef UNIV_HOTBACKUP
  if (fil_system->is_greater_than_max_id(space_id) && !recv_recovery_on &&
      !dict_sys_t::is_reserved(space_id) &&
      !fsp_is_system_temporary(space_id)) {
    fil_system->set_maximum_space_id(space);
  }
#endif /* !UNIV_HOTBACKUP */

  space->purpose = purpose;

  ut_a(flags < std::numeric_limits<uint32_t>::max());
  space->flags = (uint32_t)flags;

  space->magic_n = FIL_SPACE_MAGIC_N;

  space->m_encryption_metadata.m_type = Encryption::NONE;
  space->encryption_op_in_progress = Encryption::Progress::NONE;

  rw_lock_create(fil_space_latch_key, &space->latch, LATCH_ID_FIL_SPACE);

#ifndef UNIV_HOTBACKUP
  if (space->purpose == FIL_TYPE_TEMPORARY) {
    ut_d(space->latch.set_temp_fsp());
  }
#endif /* !UNIV_HOTBACKUP */

  space_add(space);

  return space;
}

/** Create a space memory object and put it to the fil_system hash table.
The tablespace name is independent from the tablespace file-name.
Error messages are issued to the server log.
@param[in]      name            Tablespace name
@param[in]      space_id        Tablespace ID
@param[in]      flags           Tablespace flags
@param[in]      purpose         Tablespace purpose
@return pointer to created tablespace, to be filled in with fil_node_create()
@retval nullptr on failure (such as when the same tablespace exists) */
fil_space_t *fil_space_create(const char *name, space_id_t space_id,
                              uint32_t flags, fil_type_t purpose) {
  ut_ad(fsp_flags_is_valid(flags));
  ut_ad(srv_page_size == UNIV_PAGE_SIZE_ORIG || flags != 0);

  DBUG_EXECUTE_IF("fil_space_create_failure", return nullptr;);

  fil_system->mutex_acquire_all();

  auto shard = fil_system->shard_by_id(space_id);

  auto space = shard->space_create(name, space_id, flags, purpose);

  if (space == nullptr) {
    /* Duplicate error. */
    fil_system->mutex_release_all();
    return nullptr;
  }

  /* Cache the system tablespaces, avoid looking them up during IO. */

  if (space->id == TRX_SYS_SPACE) {
    ut_a(fil_space_t::s_sys_space == nullptr ||
         fil_space_t::s_sys_space == space);

    fil_space_t::s_sys_space = space;
  }

  fil_system->mutex_release_all();

  return space;
}

/** Assigns a new space id for a new single-table tablespace. This
works simply by incrementing the global counter. If 4 billion ids
is not enough, we may need to recycle ids.
@param[out]     space_id        Set this to the new tablespace ID
@return true if assigned, false if not */
bool Fil_system::assign_new_space_id(space_id_t *space_id) {
  mutex_acquire_all();

  space_id_t id = *space_id;

  if (id < m_max_assigned_id) {
    id = m_max_assigned_id;
  }

  ++id;

  space_id_t reserved_space_id = dict_sys_t::s_reserved_space_id;

  if (id > (reserved_space_id / 2) && (id % 1000000UL == 0)) {
    ib::warn(ER_IB_MSG_282)
        << "You are running out of new single-table"
           " tablespace id's. Current counter is "
        << id << " and it must not exceed " << reserved_space_id
        << "! To reset the counter to zero you have to dump"
           " all your tables and recreate the whole InnoDB"
           " installation.";
  }

  bool success = !dict_sys_t::is_reserved(id);

  if (success) {
    *space_id = m_max_assigned_id = id;

  } else {
    ib::warn(ER_IB_MSG_283) << "You have run out of single-table tablespace"
                               " id's! Current counter is "
                            << id
                            << ". To reset the counter to zero"
                               " you have to dump all your tables and"
                               " recreate the whole InnoDB installation.";

    *space_id = SPACE_UNKNOWN;
  }

  mutex_release_all();

  return success;
}

/** Assigns a new space id for a new single-table tablespace. This works
simply by incrementing the global counter. If 4 billion id's is not enough,
we may need to recycle id's.
@param[out]     space_id                Set this to the new tablespace ID
@return true if assigned, false if not */
bool fil_assign_new_space_id(space_id_t *space_id) {
  return fil_system->assign_new_space_id(space_id);
}

/** Open the files associated with a tablespace, make sure the size of
the tablespace is read from the header page, and return a pointer to the
fil_space_t that is in the memory cache associated with the given space id.
@param[in]      space_id        Get the tablespace instance or this ID
@return file_space_t pointer, nullptr if space not found */
fil_space_t *Fil_shard::space_load(space_id_t space_id) {
  ut_ad(mutex_owned());

  fil_space_t *space = get_space_by_id(space_id);

  if (space == nullptr || space->size != 0) {
    return space;
  }

  switch (space->purpose) {
    case FIL_TYPE_IMPORT:
    case FIL_TYPE_TEMPORARY:
    case FIL_TYPE_TABLESPACE:

      ut_a(space_id != TRX_SYS_SPACE);

      space = get_space_by_id(space_id);

      if (space == nullptr) {
        return nullptr;
      }

      ut_a(1 == space->files.size());

      {
        auto file = &space->files.front();

        /* It must be a single-table tablespace and
        we have not opened the file yet; the following
        calls will open it and update the size fields */
        if (!prepare_file_for_io(file)) {
          /* The single-table tablespace can't be opened,
          because the ibd file is missing. */

          return nullptr;
        }

        complete_io(file, IORequestRead);
      }
  }

  return space;
}

/** Returns the path from the first fil_node_t found with this space ID.
The caller is responsible for freeing the memory allocated here for the
value returned.
@param[in]      space_id        Tablespace ID
@return own: A copy of fil_node_t::path, nullptr if space ID is zero
or not found. */
char *fil_space_get_first_path(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->space_load(space_id);

  char *path;

  if (space != nullptr) {
    path = mem_strdup(space->files.front().name);
  } else {
    path = nullptr;
  }

  shard->mutex_release();

  return path;
}

/** Returns the size of the space in pages. The tablespace must be cached
in the memory cache.
@param[in]      space_id        Tablespace ID
@return space size, 0 if space not found */
page_no_t fil_space_get_size(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->space_load(space_id);

  page_no_t size = space ? space->size : 0;

  shard->mutex_release();

  return size;
}

page_no_t fil_space_get_undo_initial_size(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->space_load(space_id);

  page_no_t size = space ? space->m_undo_initial : 0;

  shard->mutex_release();

  return size;
}

void fil_space_set_undo_size(space_id_t space_id, bool use_current) {
  ut_ad(fsp_is_undo_tablespace(space_id));

  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->space_load(space_id);

  if (space != nullptr) {
    space->m_undo_initial =
        (use_current ? space->size : UNDO_INITIAL_SIZE_IN_PAGES);
    space->m_undo_extend = UNDO_INITIAL_SIZE_IN_PAGES;
  }

  shard->mutex_release();
}

/** Returns the flags of the space. The tablespace must be cached
in the memory cache.
@param[in]      space_id        Tablespace ID for which to get the flags
@return flags, ULINT_UNDEFINED if space not found */
uint32_t fil_space_get_flags(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->space_load(space_id);

  uint32_t flags;

  flags = (space != nullptr) ? space->flags : UINT32_UNDEFINED;

  shard->mutex_release();

  return flags;
}

/** Open each file of a tablespace if not already open.
@param[in]      space_id        tablespace identifier
@retval true    if all file nodes were opened
@retval false   on failure */
bool Fil_shard::space_open(space_id_t space_id) {
  ut_ad(mutex_owned());

  fil_space_t *space = get_space_by_id(space_id);

  for (auto &file : space->files) {
    if (!file.is_open && !open_file(&file)) {
      return false;
    }
  }

  return true;
}

/** Open each file of a tablespace if not already open.
@param[in]      space_id        Tablespace ID
@retval true    if all file nodes were opened
@retval false   on failure */
bool fil_space_open(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  bool success = shard->space_open(space_id);

  shard->mutex_release();

  return success;
}

/** Close each file of a tablespace if open.
@param[in]      space_id        Tablespace ID */
void fil_space_close(space_id_t space_id) {
  if (fil_system == nullptr) {
    return;
  }

  auto shard = fil_system->shard_by_id(space_id);

  shard->close_file(space_id);
}

/** Returns the page size of the space and whether it is compressed or not.
The tablespace must be cached in the memory cache.
@param[in]      space_id        Tablespace ID
@param[out]     found           true if tablespace was found
@return page size */
const page_size_t fil_space_get_page_size(space_id_t space_id, bool *found) {
  const uint32_t flags = fil_space_get_flags(space_id);

  if (flags == UINT32_UNDEFINED) {
    *found = false;
    return univ_page_size;
  }

  *found = true;

  return page_size_t(flags);
}

/** Initializes the tablespace memory cache.
@param[in]      max_n_open      Maximum number of open files */
void fil_init(ulint max_n_open) {
  static_assert((1 << UNIV_PAGE_SIZE_SHIFT_MAX) == UNIV_PAGE_SIZE_MAX,
                "(1 << UNIV_PAGE_SIZE_SHIFT_MAX) != UNIV_PAGE_SIZE_MAX");

  static_assert((1 << UNIV_PAGE_SIZE_SHIFT_MIN) == UNIV_PAGE_SIZE_MIN,
                "(1 << UNIV_PAGE_SIZE_SHIFT_MIN) != UNIV_PAGE_SIZE_MIN");

  ut_a(fil_system == nullptr);

  ut_a(max_n_open > 0);

  fil_system = ut::new_withkey<Fil_system>(UT_NEW_THIS_FILE_PSI_KEY, MAX_SHARDS,
                                           max_n_open);
}

bool fil_open_files_limit_update(size_t &new_max_open_files) {
  return fil_system->set_open_files_limit(new_max_open_files);
}

bool Fil_system::set_open_files_limit(size_t &new_max_open_files) {
  const auto start_time = std::chrono::steady_clock::now();
  {
    const auto current_minimum_limit_for_open_files =
        get_minimum_limit_for_open_files(m_n_files_not_belonging_in_lru);

    if (new_max_open_files < current_minimum_limit_for_open_files) {
      /* Use the same value that was used for the check, to not mislead user
      with other value than was used in calculations. */
      new_max_open_files = current_minimum_limit_for_open_files;
      return false;
    }
  }
  /* We impose our new limit to not allow new file openings to cross new limits
  (including non-LRU limit). */
  if (!m_open_files_limit.set_desired_limit(new_max_open_files)) {
    new_max_open_files = 0;
    return false;
  }

  /* We read the m_n_files_not_belonging_in_lru again after the
  m_open_files_limit write is issued. */
  const auto current_minimum_limit_for_open_files =
      get_minimum_limit_for_open_files(m_n_files_not_belonging_in_lru);

  if (new_max_open_files < current_minimum_limit_for_open_files) {
    /* The limit was already exceeded while we were setting the
    m_open_files_limit.get_limit(). We rollback from the limit change. There is
    a counterpart check in the `Fil_shard::open_file` to rollback the limit
    reservation if this case is encountered there. */
    m_open_files_limit.revert_desired_limit();

    /* Use the same value that was used for the check, to not mislead user with
    other value than was used in calculations. */
    new_max_open_files = current_minimum_limit_for_open_files;
    return false;
  }

  const auto set_new_limit_timeout = std::chrono::seconds(5);

  for (;;) {
    auto current_n_files_open = fil_n_files_open.load();
    if ((size_t)new_max_open_files >= current_n_files_open) {
      break;
    }
    if (std::chrono::steady_clock::now() - start_time > set_new_limit_timeout) {
      /* Timeout, let's rollback the limit change and recommend a new limit. */
      m_open_files_limit.revert_desired_limit();

      /* Use the same value that was used for the check, to not mislead user
      with other value than was used in calculations. */
      new_max_open_files = current_n_files_open;
      return false;
    }
    fil_system->flush_file_spaces();

    if (fil_system->close_file_in_all_LRU()) {
      /* We closed some file, loop again to re-evaluate situation. */
      continue;
    }
    wait_while_ios_in_progress();
  }

#ifndef UNIV_HOTBACKUP
  /* Set the new limit in system variable. */
  innobase_set_open_files_limit(new_max_open_files);
  m_open_files_limit.commit_desired_limit();
#endif

  return true;
}

/** Open all the system files.
@param[in]      max_n_open      Maximum number of open files allowed
@param[in,out]  n_open          Current number of open files */
void Fil_shard::open_system_tablespaces(size_t max_n_open, size_t *n_open) {
  mutex_acquire();

  for (auto elem : m_spaces) {
    auto space = elem.second;

    if (Fil_system::space_belongs_in_LRU(space)) {
      continue;
    }

    for (auto &file : space->files) {
      if (!file.is_open) {
        if (!open_file(&file)) {
          /* This func is called during server's startup. If some file of log
          or system tablespace is missing, the server can't start
          successfully. So we should assert for it. */
          ut_error;
        }

        ++*n_open;
      }

      if (max_n_open < 10 + *n_open) {
        ib::warn(ER_IB_MSG_284, *n_open, max_n_open);
      }
    }
  }

  mutex_release();
}

/** Opens all system tablespace data files in all shards. */
void Fil_system::open_all_system_tablespaces() {
  size_t n_open = 0;

  for (auto shard : m_shards) {
    shard->open_system_tablespaces(get_open_files_limit(), &n_open);
  }
}

/** Opens all system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup
after the space objects for the log and the system tablespace have
been created. The purpose of this operation is to make sure we never
run out of file descriptors if we need to read from the insert buffer
or to write to the log. */
void fil_open_system_tablespace_files() {
  fil_system->open_all_system_tablespaces();
}

#ifndef UNIV_HOTBACKUP
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
static void fil_validate_space_reference_count(
    fil_space_t *space, Space_References &buffer_pool_references) {
  const auto space_reference_count = space->get_reference_count();

  if (space_reference_count != buffer_pool_references[space]) {
    ib::error() << "Space id=" << space->id << " reference count is "
                << space_reference_count
                << ", while references count found in buffer pool is "
                << buffer_pool_references[space] << ". fast_shutdown is "
                << srv_fast_shutdown;
  }
}

void Fil_shard::validate_space_reference_count(
    Space_References &buffer_pool_references) {
  ut_ad(!mutex_owned());

  mutex_acquire();

  for (auto &e : m_spaces) {
    fil_validate_space_reference_count(e.second, buffer_pool_references);
  }

  for (auto &e : m_deleted_spaces) {
    fil_validate_space_reference_count(e.second, buffer_pool_references);
  }

  mutex_release();
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#endif /* !UNIV_HOTBACKUP */

void Fil_shard::close_all_files() {
  ut_ad(mutex_owned());

  /* Iterates over a specified container of pair */
  auto iterate_all_spaces_files = [this](auto &spaces, auto preprocess_space,
                                         auto postprocess_space) {
    for (auto &e : spaces) {
      auto &space = e.second;
      if (space == nullptr) {
        continue;
      }

      preprocess_space(space);

      for (auto &file : space->files) {
        if (file.is_open && !file.can_be_closed()) {
          mutex_release();
          std::this_thread::sleep_for(std::chrono::milliseconds{1});
          mutex_acquire();
          /* Files or spaces could have changed when we did not hold the
          mutex, restart the loop. */
          return false;
        }
        if (file.is_open) {
          close_file(&file);
        }
      }

      postprocess_space(space);

      space_free_low(space);

      ut_a(space == nullptr);
    }
    return true;
  };

  for (;;) {
    if (!iterate_all_spaces_files(
            m_spaces,
            [](auto space) {
              ut_a(space->id == TRX_SYS_SPACE ||
                   space->purpose == FIL_TYPE_TEMPORARY ||
                   space->files.size() == 1);
            },
            [this](auto space) { space_detach(space); })) {
      continue;
    }

    m_spaces.clear();

#ifndef UNIV_HOTBACKUP
    if (!iterate_all_spaces_files(
            m_deleted_spaces,
            [](auto space) {
              ut_a(space->id != TRX_SYS_SPACE &&
                   space->id != dict_sys_t::s_dict_space_id);

              ut_a(space->files.size() <= 1);
            },
            [](auto) {})) {
      continue;
    }

    m_deleted_spaces.clear();
#endif /* !UNIV_HOTBACKUP */
    break;
  }
}

/** Close all open files. */
void Fil_system::close_all_files() {
#ifndef UNIV_HOTBACKUP
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  bool should_validate_space_reference_count = srv_fast_shutdown == 0;
  DBUG_EXECUTE_IF("buf_disable_space_reference_count_check",
                  should_validate_space_reference_count = false;);

  if (should_validate_space_reference_count) {
    auto buffer_pool_references = buf_LRU_count_space_references();
    for (auto shard : m_shards) {
      shard->validate_space_reference_count(buffer_pool_references);
    }
  }
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#endif /* !UNIV_HOTBACKUP */

  for (auto shard : m_shards) {
    shard->mutex_acquire();

    shard->close_all_files();

    shard->mutex_release();
  }

#ifndef UNIV_HOTBACKUP
  /* Revert to old names if downgrading after upgrade failure. */
  if (srv_downgrade_partition_files) {
    rename_partition_files(true);
  }

  clear_old_files();
#endif /* !UNIV_HOTBACKUP */
}

/** Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
void fil_close_all_files() { fil_system->close_all_files(); }

/** Iterate through all persistent tablespace files (FIL_TYPE_TABLESPACE)
returning the nodes via callback function cbk.
@param[in]      f               Callback
@return any error returned by the callback function. */
dberr_t Fil_shard::iterate(Fil_iterator::Function &f) {
  mutex_acquire();

  for (auto &elem : m_spaces) {
    auto space = elem.second;

    if (space->purpose != FIL_TYPE_TABLESPACE) {
      continue;
    }

    for (auto &file : space->files) {
      /* Note: The callback can release the mutex. */

      dberr_t err = f(&file);

      if (err != DB_SUCCESS) {
        mutex_release();

        return err;
      }
    }
  }

  mutex_release();

  return DB_SUCCESS;
}

dberr_t Fil_system::iterate(Fil_iterator::Function &f) {
  for (auto shard : m_shards) {
    dberr_t err = shard->iterate(f);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  return DB_SUCCESS;
}

dberr_t Fil_iterator::iterate(Function &&f) { return fil_system->iterate(f); }

/** Sets the max tablespace id counter if the given number is bigger than the
previous value.
@param[in]      max_id          Maximum known tablespace ID */
void fil_set_max_space_id_if_bigger(space_id_t max_id) {
  if (dict_sys_t::is_reserved(max_id)) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_285, ulong{max_id});
  }

  fil_system->update_maximum_space_id(max_id);
}

/** Write the flushed LSN to the page header of the first page in the
system tablespace.
@param[in]      lsn             Flushed LSN
@return DB_SUCCESS or error number */
dberr_t fil_write_flushed_lsn(lsn_t lsn) {
  dberr_t err;

  auto buf =
      static_cast<byte *>(ut::aligned_alloc(UNIV_PAGE_SIZE, UNIV_PAGE_SIZE));

  const page_id_t page_id(TRX_SYS_SPACE, 0);

  err = fil_read(page_id, univ_page_size, 0, univ_page_size.physical(), buf);

  if (err == DB_SUCCESS) {
    mach_write_to_8(buf + FIL_PAGE_FILE_FLUSH_LSN, lsn);

    err = fil_write(page_id, univ_page_size, 0, univ_page_size.physical(), buf);

    fil_system->flush_file_spaces();
  }

  ut::aligned_free(buf);

  return err;
}

/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]      space_id        Tablespace ID
@param[in]      silent          Whether to silently ignore missing tablespaces
@return the tablespace, or nullptr if missing or being deleted */
fil_space_t *Fil_system::space_acquire(space_id_t space_id, bool silent) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  if (space == nullptr) {
    if (!silent && m_ACCESSING_NONEXISTINC_SPACE_throttler.apply()) {
      ib::warn(ER_IB_WARN_ACCESSING_NONEXISTINC_SPACE, ulong{space_id});
    }
  } else if (!shard->space_acquire(space)) {
    space = nullptr;
  }

  shard->mutex_release();

  return space;
}

inline bool Fil_shard::space_acquire(fil_space_t *space) {
  ut_ad(mutex_owned());
  ut_ad(space != nullptr);

  if (space->stop_new_ops) {
    return false;
  }

  ++space->n_pending_ops;

  return true;
}

/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]      space_id        Tablespace ID
@return the tablespace, or nullptr if missing or being deleted */
fil_space_t *fil_space_acquire(space_id_t space_id) {
  return fil_system->space_acquire(space_id, false);
}

/** Acquire a tablespace that may not exist.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]      space_id        Tablespace ID
@return the tablespace, or nullptr if missing or being deleted */
fil_space_t *fil_space_acquire_silent(space_id_t space_id) {
  return fil_system->space_acquire(space_id, true);
}

/** Release a tablespace acquired with fil_space_acquire().
@param[in,out]  space   Tablespace to release  */
void fil_space_release(fil_space_t *space) {
  auto shard = fil_system->shard_by_id(space->id);

  shard->mutex_acquire();
  shard->space_release(space);
  shard->mutex_release();
}

/** Release a tablespace acquired with Fil_shard::space_acquire().
@param[in,out]  space  tablespace to release */
void Fil_shard::space_release(fil_space_t *space) {
  ut_ad(space->magic_n == FIL_SPACE_MAGIC_N);
  ut_ad(space->n_pending_ops > 0);

  --space->n_pending_ops;
}

/** Check for pending operations.
@param[in]      space   tablespace
@param[in]      count   number of attempts so far
@return 0 if no pending operations else count + 1. */
ulint Fil_shard::space_check_pending_operations(fil_space_t *space,
                                                ulint count) const {
  ut_ad(mutex_owned());

  if (space != nullptr && space->n_pending_ops > 0) {
    if (count > 5000) {
      ib::warn(ER_IB_MSG_287, space->name, ulong{space->n_pending_ops});
    }

    return count + 1;
  }

  return 0;
}

/** Check for pending IO.
@param[in]      space           Tablespace to check
@param[in]      file            File in space list
@param[in]      count           number of attempts so far
@return 0 if no pending else count + 1. */
ulint Fil_shard::check_pending_io(const fil_space_t *space,
                                  const fil_node_t &file, ulint count) const {
  ut_ad(mutex_owned());
  ut_a(space->n_pending_ops == 0);

  ut_a(space->id == TRX_SYS_SPACE || space->purpose == FIL_TYPE_TEMPORARY ||
       space->files.size() == 1);

  if (space->n_pending_flushes > 0 || file.n_pending_ios > 0) {
    if (count > 1000) {
      ib::warn(ER_IB_MSG_288, space->name, ulong{space->n_pending_flushes},
               size_t{file.n_pending_ios});
    }

    return count + 1;
  }

  return 0;
}

dberr_t Fil_shard::wait_for_pending_operations(space_id_t space_id,
                                               fil_space_t *&space,
                                               char **path) const {
  ut_ad(!fsp_is_system_tablespace(space_id));
  ut_ad(!fsp_is_global_temporary(space_id));

  space = nullptr;

  mutex_acquire();

  fil_space_t *sp = get_space_by_id(space_id);

  if (sp != nullptr) {
    sp->stop_new_ops = true;
  }

  mutex_release();

  /* Check for pending operations. */

  ulint count = 0;

  do {
    mutex_acquire();

    sp = get_space_by_id(space_id);

    count = space_check_pending_operations(sp, count);

    mutex_release();

    if (count > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

  } while (count > 0);

  /* Check for pending IO. */

  *path = nullptr;

  do {
    mutex_acquire();

    sp = get_space_by_id(space_id);

    if (sp == nullptr) {
      mutex_release();

      return DB_TABLESPACE_NOT_FOUND;
    }

    ut_a(sp->files.size() == 1);
    const fil_node_t &file = sp->files.front();

    count = check_pending_io(sp, file, count);

    if (count == 0) {
      *path = mem_strdup(file.name);
    }

    mutex_release();

    if (count > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

  } while (count > 0);

  ut_ad(sp != nullptr);

  space = sp;

  return DB_SUCCESS;
}

std::string Fil_path::get_existing_path(const std::string &path,
                                        std::string &ghost) {
  std::string existing_path{path};

  /* This is only called for non-existing paths. */
  while (!os_file_exists(existing_path.c_str())) {
    /* Some part of this path does not exist.
    If the last char is a separator, strip it off. */
    trim_separator(existing_path);

    auto sep = existing_path.find_last_of(SEPARATOR);
    if (sep == std::string::npos) {
      /* If no separator is found, it must be relative to the current dir. */
      if (existing_path == ".") {
        /* This probably cannot happen, but break here to ensure that the
        loop always has a way out. */
        break;
      }
      ghost.assign(path);
      existing_path.assign(".");
      existing_path.push_back(OS_SEPARATOR);
    } else {
      ghost.assign(path.substr(sep + 1, path.length()));
      existing_path.resize(sep + 1);
    }
  }

  return existing_path;
}

std::string Fil_path::get_real_path(const std::string &path, bool force) {
  bool path_exists;
  os_file_type_t path_type;
  char abspath[OS_FILE_MAX_PATH];
  std::string in_path{path};
  std::string real_path;

  if (path.empty()) {
    return std::string("");
  }

  /* We do not need a separator at the end in order to determine what
  kind of object it is.  So take it off. If it is there and the last
  part is actually a file, the correct real path will be returned. */
  if (in_path.length() > 1 && is_separator(in_path.back())) {
    trim_separator(in_path);
  }

  /* Before we make an absolute path, check if this path exists,
  and if so, what type it is. */
  os_file_status(in_path.c_str(), &path_exists, &path_type);

  int ret = my_realpath(abspath, in_path.c_str(), MYF(0));

  if (ret == 0) {
    real_path.assign(abspath);
  } else {
    /* This often happens on non-Windows platforms when the path does not
    fully exist yet. */

    if (path_exists) {
      /* my_realpath() failed for some reason other than the path does not
      exist. */
      if (force) {
        /* Use the given path and make it comparable. */
        real_path.assign(in_path);
      } else {
        /* Return null and make a note of it.  Another attempt will be made
        later when Fil_path::get_real_path() is called with force=true. */
        ib::info(ER_IB_MSG_289) << "my_realpath('" << path
                                << "') failed for path type " << path_type;
        return (std::string(""));
      }
    } else {
      /* The path does not exist.  Try my_realpath() again with the
      existing portion of the path. */
      std::string ghost;
      std::string dir = get_existing_path(in_path, ghost);

      ret = my_realpath(abspath, dir.c_str(), MYF(0));
      ut_ad(ret == 0);

      /* Concatenate the absolute path with the non-existing sub-path.
      NOTE: If this path existed, my_realpath() would put a separator
      at the end if it is a directory.  But since the ghost portion
      does not yet exist, we don't know if it is a dir or a file, so
      we cannot attach a trailing separator for a directory.  So we
      trim them off in Fil_path::is_same_as() and is_ancestor(). */
      real_path.assign(abspath);
      append_separator(real_path);
      real_path.append(ghost);
    }
  }

  if (lower_case_file_system) {
    Fil_path::to_lower(real_path);
  }

  /* Try to consistently end a directory name with a separator.
  On Windows, my_realpath() usually puts a separator at the end
  of a directory path (it does not do that for the path ".").
  On non-Windows it never does.
  So if the separator is missing, decide whether to append it. */
  ut_ad(!real_path.empty());
  if (!is_separator(real_path.back())) {
    bool add_sep = true;
    switch (path_type) {
      case OS_FILE_TYPE_DIR:
      case OS_FILE_TYPE_BLOCK:
        break;
      case OS_FILE_TYPE_FILE:
      case OS_FILE_TYPE_LINK:
        add_sep = false;
        break;
      case OS_FILE_TYPE_FAILED:
      case OS_FILE_TYPE_MISSING:
      case OS_FILE_TYPE_NAME_TOO_LONG:
      case OS_FILE_PERMISSION_ERROR:
      case OS_FILE_TYPE_UNKNOWN:
        /* This filepath is missing or cannot be identified for some other
        reason. If it ends in a three letter extension, assume it is a file
        name and do not add the trailing separator. Otherwise, assume it is
        intended to be a directory.*/
        size_t s = real_path.size();
        if (s > 4 && real_path[s - 4] == '.' && real_path[s - 3] != '.' &&
            real_path[s - 2] != '.' && real_path[s - 1] != '.' &&
            !is_separator(real_path[s - 3]) &&
            !is_separator(real_path[s - 2])) {
          add_sep = false;
        }
    }

    if (add_sep) {
      append_separator(real_path);
    }
  }

  return real_path;
}

std::string Fil_path::get_basename(const std::string &filepath) {
  auto sep = filepath.find_last_of(SEPARATOR);

  return (sep == std::string::npos)
             ? filepath
             : filepath.substr(sep + 1, filepath.length() - sep);
}

/** Constructor
@param[in]      dir             Directory that the files are under */
Tablespace_files::Tablespace_files(const std::string &dir)
    : m_ibd_paths(), m_undo_paths(), m_dir(dir) {
  ut_ad(Fil_path::is_separator(dir.back()));
}

/** Closes a single-table tablespace. The tablespace must be cached in the
memory cache. Free all pages used by the tablespace.
@param[in]      space_id        Tablespace ID
@return DB_SUCCESS or error */
dberr_t fil_close_tablespace(space_id_t space_id) {
  ut_ad(!fsp_is_undo_tablespace(space_id));
  ut_ad(!fsp_is_system_or_temp_tablespace(space_id));

  auto shard = fil_system->shard_by_id(space_id);

  char *path{};
  fil_space_t *space{};

  auto err = shard->wait_for_pending_operations(space_id, space, &path);

  if (err != DB_SUCCESS) {
    return err;
  }

  ut_a(path != nullptr);

#ifndef UNIV_HOTBACKUP
  shard->space_prepare_for_delete(space);
#else
  rw_lock_x_lock(&space->latch, UT_LOCATION_HERE);

  /* If the free is successful, the X lock will be released before
  the space memory data structure is freed. */

  if (!fil_space_free(space_id, true)) {
    rw_lock_x_unlock(&space->latch);
    err = DB_TABLESPACE_NOT_FOUND;
  } else {
    err = DB_SUCCESS;
  }
#endif /* !UNIV_HOTBACKUP */

  /* Delete any generated files, otherwise if we drop the database the
  remove directory will fail. */

  auto cfg_name = Fil_path::make_cfg(path);

  if (cfg_name != nullptr) {
    os_file_delete_if_exists(innodb_data_file_key, cfg_name, nullptr);

    ut::free(cfg_name);
  }

  auto cfp_name = Fil_path::make_cfp(path);

  if (cfp_name != nullptr) {
    os_file_delete_if_exists(innodb_data_file_key, cfp_name, nullptr);

    ut::free(cfp_name);
  }

  ut::free(path);

  return err;
}

#ifndef UNIV_HOTBACKUP
/** Write a log record about an operation on a tablespace file.
@param[in]      type            MLOG_FILE_OPEN or MLOG_FILE_DELETE
                                or MLOG_FILE_CREATE or MLOG_FILE_RENAME
@param[in]      space_id        Tablespace identifier
@param[in]      path            File path
@param[in]      new_path        If type is MLOG_FILE_RENAME, the new name
@param[in]      flags           If type is MLOG_FILE_CREATE, the space flags
@param[in,out]  mtr             Mini-transaction */
static void fil_op_write_log(mlog_id_t type, space_id_t space_id,
                             const char *path, const char *new_path,
                             uint32_t flags, mtr_t *mtr) {
  ut_ad(space_id != TRX_SYS_SPACE);

  byte *log_ptr = nullptr;

  if (!mlog_open(mtr, 11 + 4 + 2 + 1, log_ptr)) {
    /* Logging in mtr is switched off during crash recovery:
    in that case mlog_open returns nullptr */
    return;
  }

  log_ptr = mlog_write_initial_log_record_low(type, space_id, 0, log_ptr, mtr);

  if (type == MLOG_FILE_CREATE) {
    mach_write_to_4(log_ptr, flags);
    log_ptr += 4;
  }

  /* Let us store the strings as null-terminated for easier readability
  and handling */

  ulint len = strlen(path) + 1;

  mach_write_to_2(log_ptr, len);
  log_ptr += 2;

  mlog_close(mtr, log_ptr);

  mlog_catenate_string(mtr, reinterpret_cast<const byte *>(path), len);

  switch (type) {
    case MLOG_FILE_RENAME:

      ut_ad(strchr(new_path, Fil_path::OS_SEPARATOR) != nullptr);

      len = strlen(new_path) + 1;

      ut_a(mlog_open(mtr, 2 + len, log_ptr));

      mach_write_to_2(log_ptr, len);

      log_ptr += 2;

      mlog_close(mtr, log_ptr);

      mlog_catenate_string(mtr, reinterpret_cast<const byte *>(new_path), len);
      break;
    case MLOG_FILE_DELETE:
    case MLOG_FILE_CREATE:
      break;
    default:
      ut_d(ut_error);
  }
}

bool fil_system_get_file_by_space_id(space_id_t space_id, std::string &name) {
  ut_a(dict_sys_t::is_reserved(space_id) || srv_is_upgrade_mode);

  return fil_system->get_file_by_space_id(space_id, name);
}

bool fil_system_get_file_by_space_num(space_id_t space_num,
                                      space_id_t &space_id, std::string &name) {
  return fil_system->get_file_by_space_num(space_num, space_id, name);
}

#endif /* !UNIV_HOTBACKUP */

dberr_t Fil_shard::space_delete(space_id_t space_id, buf_remove_t buf_remove) {
  char *path = nullptr;
  fil_space_t *space = nullptr;

  ut_ad(!fsp_is_system_tablespace(space_id));
  ut_ad(!fsp_is_global_temporary(space_id));

  dberr_t err = wait_for_pending_operations(space_id, space, &path);

  if (err != DB_SUCCESS) {
    ut_a(err == DB_TABLESPACE_NOT_FOUND);
    return err;
  }

  ut_a(path != nullptr);
  ut_a(space != nullptr);

#ifndef UNIV_HOTBACKUP
  /* IMPORTANT: Because we have set space::stop_new_ops there
  can't be any new ibuf merges, reads or flushes. We are here
  because file::n_pending was zero above. However, it is still
  possible to have pending read and write requests:

  A read request can happen because the reader thread has
  gone through the ::stop_new_ops check in buf_page_init_for_read()
  before the flag was set and has not yet incremented ::n_pending
  when we checked it above.

  A write request can be issued any time because we don't check
  the ::stop_new_ops flag when queueing a block for write.

  We deal with pending write requests in the following function
  where we'd minimally evict all dirty pages belonging to this
  space from the flush_list. Note that if a block is IO-fixed
  we'll wait for IO to complete.

  For buf_remove == BUF_REMOVE_NONE we mark the fil_space_t instance
  as deleted by bumping up the file_space_t::m_version. All pages
  that are less than this version number will be discarded. We wait
  for any pending IO to complete after that.

  To deal with potential read requests, we will check the
  ::stop_new_ops flag in fil_io(). */

  if (buf_remove != BUF_REMOVE_NONE) {
    buf_LRU_flush_or_remove_pages(space_id, buf_remove, nullptr);
  }

  /* Ensure that we write redo log for the operation also within the Clone
  notifier block. This is needed because we don't have any mechanism today
  to avoid checkpoint crossing the redo log before the actual operation
  is complete. Make sure we are not holding shard mutex. */
  ut_ad(!mutex_owned());
  Clone_notify notifier(Clone_notify::Type::SPACE_DROP, space_id, false);

  if (notifier.failed()) {
    /* Currently post DDL operations are never rolled back. */
    /* purecov: begin deadcode */
    ut::free(path);
    ut_d(ut_error);
    ut_o(return DB_ERROR);
    /* purecov: end */
  }
#endif /* !UNIV_HOTBACKUP */

  /* If it is a delete then also delete any generated files, otherwise
  when we drop the database the remove directory will fail. */
  if (space->purpose != FIL_TYPE_TEMPORARY) {
#ifdef UNIV_HOTBACKUP
    /* When replaying the operation in MySQL Enterprise
    Backup, we do not try to write any log record. */
#else  /* UNIV_HOTBACKUP */
    /* Before deleting the file, write a log record about it, so that
    InnoDB crash recovery will expect the file to be gone. */
    mtr_t mtr;

    mtr.start();

    fil_op_write_log(MLOG_FILE_DELETE, space_id, path, nullptr, 0, &mtr);

    mtr.commit();

    /* Even if we got killed shortly after deleting the
    tablespace file, the record must have already been
    written to the redo log. */
    log_write_up_to(*log_sys, mtr.commit_lsn(), true);

    DBUG_EXECUTE_IF("space_delete_crash", DBUG_SUICIDE(););
#endif /* UNIV_HOTBACKUP */

    char *cfg_name = Fil_path::make_cfg(path);

    if (cfg_name != nullptr) {
      os_file_delete_if_exists(innodb_data_file_key, cfg_name, nullptr);

      ut::free(cfg_name);
    }

    char *cfp_name = Fil_path::make_cfp(path);

    if (cfp_name != nullptr) {
      os_file_delete_if_exists(innodb_data_file_key, cfp_name, nullptr);

      ut::free(cfp_name);
    }
  }

  mutex_acquire();

  /* Double check the sanity of pending ops after reacquiring
  the fil_system::mutex. */
  if (const fil_space_t *s = get_space_by_id(space_id)) {
    ut_a(s == space);

    space->set_deleted();

#ifndef UNIV_HOTBACKUP
    ut_a(space->files.size() == 1);
    auto &file = space->files.front();

    /* Wait for any pending writes. */
    while (file.n_pending_ios > 0 || file.n_pending_flushes > 0 ||
           file.is_being_extended) {
      /* Release and reacquire the mutex because we want the IO to complete. */
      mutex_release();

      std::this_thread::yield();

      mutex_acquire();
    }

    m_deleted_spaces.push_back({space->id, space});
#endif /* !UNIV_HOTBACKUP */

    space_detach(space);

    ut_a(space->files.size() == 1);
    ut_a(space->files.front().n_pending_ios == 0);
    space_remove_from_lookup_maps(space_id);

    mutex_release();

#ifdef UNIV_HOTBACKUP
    /* For usage inside MEB we don't support lazy stale page eviction, we just
     do what fil_shard::purge() does directly here. */
    space_free_low(space);
#endif /* UNIV_HOTBACKUP */

    if (!os_file_delete(innodb_data_file_key, path) &&
        !os_file_delete_if_exists(innodb_data_file_key, path, nullptr)) {
      /* Note: This is because we have removed the
      tablespace instance from the cache. */

      err = DB_IO_ERROR;
    }
  } else {
    mutex_release();

    err = DB_TABLESPACE_NOT_FOUND;
  }

  ut::free(path);
  return err;
}

dberr_t fil_delete_tablespace(space_id_t space_id, buf_remove_t buf_remove) {
  auto shard = fil_system->shard_by_id(space_id);

  return shard->space_delete(space_id, buf_remove);
}

dberr_t Fil_shard::space_prepare_for_truncate(space_id_t space_id,
                                              fil_space_t *&space) {
  ut_ad(space_id != TRX_SYS_SPACE);
  ut_ad(!fsp_is_system_tablespace(space_id));
  ut_ad(!fsp_is_global_temporary(space_id));

  char *path{};
  auto err = wait_for_pending_operations(space_id, space, &path);

  ut::free(path);

  return err;
}

bool Fil_shard::space_truncate(space_id_t space_id, page_no_t size_in_pages) {
#ifndef UNIV_HOTBACKUP
  fil_space_t *space{};

  /* Step-1: Prepare tablespace for truncate. This involves
  stopping all the new operations + IO on that tablespace. Any future attempts
  to flush will be ignored and pages discarded. */
  if (space_prepare_for_truncate(space_id, space) != DB_SUCCESS) {
    return false;
  }

  mutex_acquire();

  /* Step-2: Mark the tablespace pages in the buffer pool as stale by bumping
   the version number of the space. Those stale pages will be ignored and freed
   lazily later. This includes AHI, for which entries will be removed on
   buf_page_free_stale*() -> buf_LRU_free_page ->
   btr_search_drop_page_hash_index() */
  space->bump_version();

  /* Step-3: Truncate the tablespace and accordingly update
  the fil_space_t handler that is used to access this tablespace. */
  ut_a(space->files.size() == 1);

  auto &file = space->files.front();

  if (!file.is_open) {
    if (!open_file(&file)) {
      mutex_release();
      return false;
    }
  }

  space->size = file.size = size_in_pages;

  bool success = os_file_truncate(file.name, file.handle, 0);

  if (success) {
    os_offset_t size = size_in_pages * UNIV_PAGE_SIZE;

    success = os_file_set_size(file.name, file.handle, 0, size, true);

    if (success) {
      space->stop_new_ops = false;
    }
  }

  mutex_release();

  return success;
#else
  /* Truncating a tablespace is not supported for MEB. */
  ut_error;
#endif
}

/** Truncate the tablespace to needed size.
@param[in]      space_id        Tablespace ID to truncate
@param[in]      size_in_pages   Truncate size.
@return true if truncate was successful. */
bool fil_truncate_tablespace(space_id_t space_id, page_no_t size_in_pages) {
  auto shard = fil_system->shard_by_id(space_id);

  return shard->space_truncate(space_id, size_in_pages);
}

#ifdef UNIV_DEBUG
/** Increase redo skipped count for a tablespace.
@param[in]      space_id        Tablespace ID */
void fil_space_inc_redo_skipped_count(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  ut_a(space != nullptr);

  ++space->redo_skipped_count;

  shard->mutex_release();
}

/** Decrease redo skipped count for a tablespace.
@param[in]      space_id        Tablespace ID */
void fil_space_dec_redo_skipped_count(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  ut_a(space != nullptr);
  ut_a(space->redo_skipped_count > 0);

  --space->redo_skipped_count;

  shard->mutex_release();
}

/** Check whether a single-table tablespace is redo skipped.
@param[in]      space_id        Tablespace ID
@return true if redo skipped */
bool fil_space_is_redo_skipped(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  ut_a(space != nullptr);

  bool is_redo_skipped = space->redo_skipped_count > 0;

  shard->mutex_release();

  return is_redo_skipped;
}
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP

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
dberr_t fil_discard_tablespace(space_id_t space_id) {
  dberr_t err;

  err = fil_delete_tablespace(space_id, BUF_REMOVE_NONE);

  switch (err) {
    case DB_SUCCESS:
      break;

    case DB_IO_ERROR:

      ib::warn(ER_IB_MSG_291, ulong{space_id}, ut_strerr(err));
      break;

    case DB_TABLESPACE_NOT_FOUND:

      ib::warn(ER_IB_MSG_292, ulong{space_id}, ut_strerr(err));
      break;

    default:
      ut_error;
  }

  return err;
}

/** Write redo log for renaming a file.
@param[in]      space_id        Tablespace id
@param[in]      old_name        Tablespace file name
@param[in]      new_name        Tablespace file name after renaming
@param[in,out]  mtr             Mini-transaction */
static void fil_name_write_rename(space_id_t space_id, const char *old_name,
                                  const char *new_name, mtr_t *mtr) {
  ut_ad(!fsp_is_system_or_temp_tablespace(space_id));
  ut_ad(!fsp_is_undo_tablespace(space_id));

  /* Note: A checkpoint can take place here. */

  DBUG_EXECUTE_IF("ib_crash_rename_log_1", DBUG_SUICIDE(););

  static const auto type = MLOG_FILE_RENAME;

  fil_op_write_log(type, space_id, old_name, new_name, 0, mtr);

  DBUG_EXECUTE_IF("ib_crash_rename_log_2", DBUG_SUICIDE(););

  /* Note: A checkpoint can take place here too before we
  have physically renamed the file. */
}

#ifdef UNIV_LINUX
/* Write a redo log record for adding pages to a tablespace
@param[in]      space_id        Space ID
@param[in]      offset          Offset from where the file
                                is extended
@param[in]      size            Number of bytes by which the file
                                is extended starting from the offset
@param[in,out]  mtr             Mini-transaction */
static void fil_op_write_space_extend(space_id_t space_id, os_offset_t offset,
                                      os_offset_t size, mtr_t *mtr) {
  ut_ad(space_id != TRX_SYS_SPACE);

  byte *log_ptr;

  if (!mlog_open(mtr, 7 + 8 + 8, log_ptr)) {
    /* Logging in mtr is switched off during crash recovery:
    in that case mlog_open returns nullptr */
    return;
  }

#ifdef UNIV_DEBUG
  byte *start_log = log_ptr;
#endif /*  UNIV_DEBUG */

  log_ptr = mlog_write_initial_log_record_low(MLOG_FILE_EXTEND, space_id, 0,
                                              log_ptr, mtr);

  ut_ad(size > 0);

  /* Write the starting offset in the file */
  mach_write_to_8(log_ptr, offset);
  log_ptr += 8;

  /* Write the size by which file needs to be extended from
  the given offset */
  mach_write_to_8(log_ptr, size);
  log_ptr += 8;

#ifdef UNIV_DEBUG
  ut_ad(log_ptr <= start_log + 23);
#endif /*  UNIV_DEBUG */

  mlog_close(mtr, log_ptr);
}
#endif
#endif /* !UNIV_HOTBACKUP */

/** Allocate and build a file name from a path, a table or tablespace name
and a suffix.
@param[in]      path_in         nullptr or the directory path or the full path
                                and filename
@param[in]      name_in         nullptr if path is full, or Table/Tablespace
                                name
@param[in]      ext             the file extension to use
@param[in]      trim            whether last name on the path should be trimmed
@return own: file name; must be freed by ut::free() */
char *Fil_path::make(const std::string &path_in, const std::string &name_in,
                     ib_file_suffix ext, bool trim) {
  /* The path should be a directory and should not contain the
  basename of the file. If the path is empty, we will use  the
  default path, */

  ut_ad(!path_in.empty() || !name_in.empty());

  std::string path;

  if (path_in.empty()) {
    if (is_absolute_path(name_in)) {
      path = "";
    } else {
      path.assign(MySQL_datadir_path);
    }
  } else {
    path.assign(path_in);
  }

  std::string name;

  if (!name_in.empty()) {
    name.assign(name_in);
  }

  /* Do not prepend the datadir path (which must be DOT_SLASH)
  if the name is an absolute path or a relative path like
  DOT_SLASH or DOT_DOT_SLASH.  */
  if (is_absolute_path(name) || has_prefix(name, DOT_SLASH) ||
      has_prefix(name, DOT_DOT_SLASH)) {
    path.clear();
  }

  std::string filepath;

  if (!path.empty()) {
    filepath.assign(path);
  }

  if (trim) {
    /* Find the offset of the last DIR separator and set it to
    null in order to strip off the old basename from this path. */
    auto pos = filepath.find_last_of(SEPARATOR);

    if (pos != std::string::npos) {
      filepath.resize(pos);
    }
  }

  if (!name.empty()) {
    append_separator(filepath);

    filepath.append(name);
  }

  /* Make sure that the specified suffix is at the end. */
  if (ext != NO_EXT) {
    const auto suffix = dot_ext[ext];
    size_t len = strlen(suffix);

    /* This assumes that the suffix starts with '.'.  If the
    first char of the suffix is found in the filepath at the
    same length as the suffix from the end, then we will assume
    that there is a previous suffix that needs to be replaced. */

    ut_ad(*suffix == '.');

    if (filepath.length() > len && *(filepath.end() - len) == *suffix) {
      filepath.replace(filepath.end() - len, filepath.end(), suffix);
    } else {
      filepath.append(suffix);
    }
  }

  normalize(filepath);

  return mem_strdup(filepath.c_str());
}

bool Fil_path::parse_file_path(const std::string &file_path,
                               ib_file_suffix extn, std::string &dict_name) {
  dict_name.assign(file_path);
  if (!Fil_path::truncate_suffix(extn, dict_name)) {
    dict_name.clear();
    return false;
  }

  /* Extract table name */
  auto table_pos = dict_name.find_last_of(SEPARATOR);
  if (table_pos == std::string::npos) {
    dict_name.clear();
    return false;
  }
  std::string table_name = dict_name.substr(table_pos + 1);
  dict_name.resize(table_pos);

  /* Extract schema name */
  auto schema_pos = dict_name.find_last_of(SEPARATOR);
  if (schema_pos == std::string::npos) {
    dict_name.clear();
    return false;
  }
  std::string schema_name = dict_name.substr(schema_pos + 1);

  /* Build dictionary table name schema/table form. */
  dict_name.assign(schema_name);
  dict_name.push_back(DB_SEPARATOR);
  dict_name.append(table_name);
  return true;
}

std::string Fil_path::make_new_path(const std::string &path_in,
                                    const std::string &name_in,
                                    ib_file_suffix extn) {
  ut_a(Fil_path::has_suffix(extn, path_in));
  ut_a(!Fil_path::has_suffix(extn, name_in));

  std::string path(path_in);

  auto pos = path.find_last_of(SEPARATOR);

  ut_a(pos != std::string::npos);

  path.resize(pos);

  pos = path.find_last_of(SEPARATOR);

  ut_a(pos != std::string::npos);

  path.resize(pos + 1);

  path.append(name_in + dot_ext[extn]);

  normalize(path);

  return path;
}

/** This function reduces a null-terminated full remote path name
into the path that is sent by MySQL for DATA DIRECTORY clause.
It replaces the 'databasename/tablename.ibd' found at the end of the
path with just 'tablename'.

Since the result is always smaller than the path sent in, no new
memory is allocated. The caller should allocate memory for the path
sent in. This function manipulates that path in place. If the path
format is not as expected, set data_dir_path to "" and return.

The result is used to inform a SHOW CREATE TABLE command.
@param[in,out]  data_dir_path   Full path/data_dir_path */
void Fil_path::make_data_dir_path(char *data_dir_path) {
  /* Replace the period before the extension with a null byte. */
  ut_ad(has_suffix(IBD, data_dir_path));
  char *dot = strrchr((char *)data_dir_path, '.');
  *dot = '\0';

  /* The tablename starts after the last slash. */
  char *base_slash = strrchr((char *)data_dir_path, OS_PATH_SEPARATOR);
  ut_ad(base_slash != nullptr);

  *base_slash = '\0';

  std::string base_name{base_slash + 1};

  /* The database name starts after the next to last slash. */
  char *db_slash = strrchr((char *)data_dir_path, OS_SEPARATOR);
  ut_ad(db_slash != nullptr);
  char *db_name = db_slash + 1;

  /* Overwrite the db_name with the base_name. */
  memmove(db_name, base_name.c_str(), base_name.length());
  db_name[base_name.length()] = '\0';
}

/** Test if a tablespace file can be renamed to a new filepath by checking
if that the old filepath exists and the new filepath does not exist.
@param[in]      space_id        Tablespace ID
@param[in]      old_path        Old filepath
@param[in]      new_path        New filepath
@param[in]      is_discarded    Whether the tablespace is discarded
@return innodb error code */
dberr_t fil_rename_tablespace_check(space_id_t space_id, const char *old_path,
                                    const char *new_path, bool is_discarded) {
  bool exists;
  os_file_type_t ftype;

  if (!is_discarded && os_file_status(old_path, &exists, &ftype) && !exists) {
    ib::error(ER_IB_MSG_293, old_path, new_path, ulong{space_id});
    return DB_TABLESPACE_NOT_FOUND;
  }

  if (!os_file_status(new_path, &exists, &ftype) || exists) {
    ib::error(ER_IB_MSG_294, old_path, new_path, ulong{space_id});
    return DB_TABLESPACE_EXISTS;
  }

  return DB_SUCCESS;
}

/** Rename a single-table tablespace.
The tablespace must exist in the memory cache.
@param[in]      space_id        Tablespace ID
@param[in]      old_path        Old file name
@param[in]      new_name        New tablespace  name in the schema/space
@param[in]      new_path_in     New file name, or nullptr if it is located
                                in the normal data directory
@return InnoDB error code */
dberr_t Fil_shard::space_rename(space_id_t space_id, const char *old_path,
                                const char *new_name, const char *new_path_in) {
  fil_space_t *space;
  ulint count = 0;
  fil_node_t *file = nullptr;
  bool write_ddl_log = true;
  auto start_time = std::chrono::steady_clock::now();

#ifdef UNIV_DEBUG
  static uint32_t crash_injection_rename_tablespace_counter = 1;
#endif /* UNIV_DEBUG */

  ut_a(space_id != TRX_SYS_SPACE);
  ut_ad(strchr(new_name, '/') != nullptr);

  for (;;) {
    bool retry = false;
    bool flush = false;

    ++count;

    if (!(count % 1000)) {
      ib::warn(ER_IB_MSG_295, old_path, ulong{space_id}, ulonglong{count});
    }

    /* The name map and space ID map are in the same shard. */
    mutex_acquire();

    space = get_space_by_id(space_id);

    DBUG_EXECUTE_IF("fil_rename_tablespace_failure_1", space = nullptr;);

    if (space == nullptr) {
      ib::error(ER_IB_MSG_296, ulong{space_id}, old_path);

      mutex_release();

      return DB_ERROR;

    } else if (space->prevent_file_open) {
      /* Some other thread has stopped the IO. We need to
       wait for the other thread to complete its operation. */
      mutex_release();

      if (std::chrono::steady_clock::now() - start_time >= PRINT_INTERVAL) {
        ib::warn(ER_IB_MSG_297);

        start_time = std::chrono::steady_clock::now();
      }

      std::this_thread::sleep_for(std::chrono::seconds(1));

      continue;

    } else if (count > 25000) {
      mutex_release();

      return DB_ERROR;

    } else if (space != get_space_by_name(space->name)) {
      ib::error(ER_IB_MSG_298, space->name);

      mutex_release();

      return DB_ERROR;

    } else {
      auto new_space = get_space_by_name(new_name);

      if (new_space != nullptr) {
        if (new_space == space) {
          mutex_release();

          return DB_SUCCESS;
        }

        ut_a(new_space->id == space->id);
      }
    }

    ut_a(space->files.size() == 1);

#ifndef UNIV_HOTBACKUP
    /* Don't write DDL log during recovery when log_ddl is
    not initialized. */

    if (write_ddl_log && log_ddl != nullptr) {
      /* Write ddl log when space->prevent_file_open is true
      can cause deadlock:
      a. buffer flush thread waits for rename thread to set
         prevent_file_open to false;
      b. rename thread waits for buffer flush thread to flush
         a page and release page lock. The page is ready for
         flush in double write buffer. */

      ut_ad(!space->prevent_file_open);

      file = &space->files.front();

      char *new_file_name = new_path_in == nullptr
                                ? Fil_path::make_ibd_from_table_name(new_name)
                                : mem_strdup(new_path_in);

      char *old_file_name = file->name;

      ut_ad(strchr(old_file_name, OS_PATH_SEPARATOR) != nullptr);

      ut_ad(strchr(new_file_name, OS_PATH_SEPARATOR) != nullptr);

      mutex_release();

      /* Rename ddl log is for rollback, so we exchange
      old file name with new file name. */
      dberr_t err = log_ddl->write_rename_space_log(space_id, new_file_name,
                                                    old_file_name);
      ut::free(new_file_name);
      if (err != DB_SUCCESS) {
        return err;
      }

      write_ddl_log = false;
      continue;
    }
#endif /* !UNIV_HOTBACKUP */

    /* We temporarily close the .ibd file because we do
    not trust that operating systems can rename an open
    file. For the closing we have to wait until there
    are no pending I/O's or flushes on the file. */

    space->prevent_file_open = true;

    file = &space->files.front();

    if (file->n_pending_ios > 0 || file->n_pending_flushes > 0 ||
        file->is_being_extended) {
      /* There are pending I/O's or flushes or the
      file is currently being extended, sleep for
      a while and retry */

      retry = true;

      space->prevent_file_open = false;

    } else if (!file->is_flushed()) {
      /* Flush the space */

      retry = flush = true;

      space->prevent_file_open = false;

    } else if (file->is_open) {
      close_file(file);
    }

    if (!retry) {
      ut_ad(space->prevent_file_open);
    }

    mutex_release();

    if (!retry) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (flush) {
      mutex_acquire();

      space_flush(space->id);

      mutex_release();
    }
  }

#ifndef UNIV_HOTBACKUP
  /* Make sure we re not holding shard mutex. */
  ut_ad(!mutex_owned());
  Clone_notify notifier(Clone_notify::Type::SPACE_RENAME, space_id, false);

  if (notifier.failed()) {
    mutex_acquire();
    space->prevent_file_open = false;
    mutex_release();

    return DB_ERROR;
  }
#endif /* !UNIV_HOTBACKUP */

  char *new_file_name;

  if (new_path_in == nullptr) {
    new_file_name = Fil_path::make_ibd_from_table_name(new_name);
  } else {
    new_file_name = mem_strdup(new_path_in);
  }

  char *old_file_name = file->name;
  char *old_space_name = space->name;
  char *new_space_name = mem_strdup(new_name);

#ifndef UNIV_HOTBACKUP
  if (!recv_recovery_on) {
    mtr_t mtr;

    mtr.start();

    fil_name_write_rename(space_id, old_file_name, new_file_name, &mtr);

    mtr.commit();
  }
#endif /* !UNIV_HOTBACKUP */

  ut_ad(strchr(old_file_name, OS_PATH_SEPARATOR) != nullptr);
  ut_ad(strchr(new_file_name, OS_PATH_SEPARATOR) != nullptr);

  mutex_acquire();

  ut_ad(space->prevent_file_open);

  /* We already checked these. */
  ut_ad(space == get_space_by_name(old_space_name));
  ut_ad(get_space_by_name(new_space_name) == nullptr);

  bool success;

  DBUG_EXECUTE_IF("fil_rename_tablespace_failure_2", goto skip_rename;);

  DBUG_INJECT_CRASH("ddl_crash_before_rename_tablespace",
                    crash_injection_rename_tablespace_counter++);

  file = &space->files.front();
  ut_ad(!file->is_open);

  success = os_file_rename(innodb_data_file_key, old_file_name, new_file_name);

  DBUG_EXECUTE_IF("fil_rename_tablespace_failure_2", skip_rename
                  : success = false;);

  DBUG_INJECT_CRASH("ddl_crash_after_rename_tablespace",
                    crash_injection_rename_tablespace_counter++);

  if (success) {
    file->name = new_file_name;

    update_space_name_map(space, new_space_name);

    space->name = new_space_name;

  } else {
    /* Because nothing was renamed, we must free the new
    names, not the old ones. */
    old_file_name = new_file_name;
    old_space_name = new_space_name;
  }

  ut_ad(space->prevent_file_open);
  space->prevent_file_open = false;

  mutex_release();

  ut::free(old_file_name);
  ut::free(old_space_name);

  return success ? DB_SUCCESS : DB_ERROR;
}

/** Rename a single-table tablespace.
The tablespace must exist in the memory cache.
@param[in]      space_id        Tablespace ID
@param[in]      old_path        Old file name
@param[in]      new_name        New tablespace name in the schema/name format
@param[in]      new_path_in     New file name, or nullptr if it is located in
the normal data directory
@return InnoDB error code */
dberr_t fil_rename_tablespace(space_id_t space_id, const char *old_path,
                              const char *new_name, const char *new_path_in) {
  auto shard = fil_system->shard_by_id(space_id);

  dberr_t err = shard->space_rename(space_id, old_path, new_name, new_path_in);

  return err;
}

/** Rename a tablespace.  Use the space_id to find the shard.
@param[in]      space_id        tablespace ID
@param[in]      old_name        old tablespace name
@param[in]      new_name        new tablespace name
@return DB_SUCCESS on success */
dberr_t Fil_system::rename_tablespace_name(space_id_t space_id,
                                           const char *old_name,
                                           const char *new_name) {
  auto old_shard = fil_system->shard_by_id(space_id);

  old_shard->mutex_acquire();

  auto old_space = old_shard->get_space_by_id(space_id);

  if (old_space == nullptr) {
    old_shard->mutex_release();

    ib::error(ER_IB_MSG_299, old_name);

    return DB_TABLESPACE_NOT_FOUND;
  }

  ut_ad(old_space == old_shard->get_space_by_name(old_name));
  old_shard->mutex_release();

  Fil_shard *new_shard{};
  fil_space_t *new_space{};

  mutex_acquire_all();

  for (auto shard : m_shards) {
    new_space = shard->get_space_by_name(new_name);

    if (new_space != nullptr) {
      new_shard = shard;
      break;
    }
  }

  if (new_space != nullptr) {
    mutex_release_all();

    if (new_space->id != old_space->id) {
      ib::error(ER_IB_MSG_300, new_name);

      return DB_TABLESPACE_EXISTS;
    } else {
      ut_a(new_shard == old_shard);
    }

    return DB_SUCCESS;
  }

  auto new_space_name = mem_strdup(new_name);
  auto old_space_name = old_space->name;

  old_shard->update_space_name_map(old_space, new_space_name);

  old_space->name = new_space_name;

  mutex_release_all();

  ut::free(old_space_name);

  return DB_SUCCESS;
}

dberr_t fil_rename_tablespace_by_id(space_id_t space_id, const char *old_name,
                                    const char *new_name) {
  return fil_system->rename_tablespace_name(space_id, old_name, new_name);
}

dberr_t fil_write_initial_pages(pfs_os_file_t file, const char *path,
                                fil_type_t type [[maybe_unused]],
                                page_no_t size, const byte *encrypt_info,
                                space_id_t space_id, uint32_t &space_flags,
                                bool &atomic_write, bool &punch_hole) {
  bool success = false;
  atomic_write = false;
  punch_hole = false;

  const page_size_t page_size(space_flags);
  const auto sz = ulonglong{size * page_size.physical()};

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
  {
    int ret = 0;
#ifdef UNIV_DEBUG
    DBUG_EXECUTE_IF("fil_create_temp_tablespace_fail_fallocate", ret = -1;);
    if (ret == 0)
#endif /* UNIV_DEBUG */
    {
      ret = posix_fallocate(file.m_file, 0, sz);
    }

    if (ret == 0) {
      success = true;
      if (type == FIL_TYPE_TEMPORARY ||
          fil_fusionio_enable_atomic_write(file)) {
        atomic_write = true;
      }
    } else {
      /* If posix_fallocate() fails for any reason, issue only a warning
      and then fall back to os_file_set_size() */
      ib::warn(ER_IB_MSG_303, path, sz, ret, strerror(errno));
    }
  }
#endif /* !NO_FALLOCATE && UNIV_LINUX */

  if (!success || (tbsp_extend_and_initialize && !atomic_write)) {
    success = os_file_set_size(path, file, 0, sz, true);

    if (success) {
      /* explicit initialization is needed as same as fil_space_extend(),
      instead of punch_hole. */
      dberr_t err =
          os_file_write_zeros(file, path, page_size.physical(), 0, sz);
      if (err != DB_SUCCESS) {
        ib::warn(ER_IB_MSG_320) << "Error while writing " << sz << " zeroes to "
                                << path << " starting at offset " << 0;
      }
    }
  }

  if (!success) {
    return DB_OUT_OF_DISK_SPACE;
  }

  /* Note: We are actually punching a hole, previous contents will
  be lost after this call, if it succeeds. In this case the file
  should be full of NULs. */

  punch_hole = os_is_sparse_file_supported(file);

  /* Should not make large punch hole as initialization of large file,
  for crash-recovery safeness around disk-full. */

  /* We have to write the space id to the file immediately and flush the
  file to disk. This is because in crash recovery we must be aware what
  tablespaces exist and what are their space id's, so that we can apply
  the log records to the right file. It may take quite a while until
  buffer pool flush algorithms write anything to the file and flush it to
  disk. If we would not write here anything, the file would be filled
  with zeros from the call of os_file_set_size(), until a buffer pool
  flush would write to it. */

  /* Align the memory for file i/o if we might have O_DIRECT set */
  auto page = static_cast<byte *>(
      ut::aligned_zalloc(2 * page_size.logical(), page_size.logical()));

  /* Add the UNIV_PAGE_SIZE to the table flags and write them to the
  tablespace header. */
  space_flags = fsp_flags_set_page_size(space_flags, page_size);
  fsp_header_init_fields(page, space_id, space_flags);
  mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

  mach_write_to_4(page + FIL_PAGE_SRV_VERSION, DD_SPACE_CURRENT_SRV_VERSION);
  mach_write_to_4(page + FIL_PAGE_SPACE_VERSION,
                  DD_SPACE_CURRENT_SPACE_VERSION);

  if (encrypt_info != nullptr) {
    auto key_offset = fsp_header_get_encryption_offset(page_size);
    memcpy(page + key_offset, encrypt_info, Encryption::INFO_SIZE);
  }

  IORequest request(IORequest::WRITE);
  dberr_t err = DB_SUCCESS;

  if (!page_size.is_compressed()) {
    buf_flush_init_for_writing(nullptr, page, nullptr, 0,
                               fsp_is_checksum_disabled(space_id),
                               true /* skip_lsn_check */);

    err = os_file_write(request, path, file, page, 0, page_size.physical());

    ut_ad(err != DB_IO_NO_PUNCH_HOLE);

  } else {
    page_zip_des_t page_zip;

    page_zip_set_size(&page_zip, page_size.physical());
    page_zip.data = page + page_size.logical();
    ut_d(page_zip.m_start = 0);
    page_zip.m_end = 0;
    page_zip.n_blobs = 0;
    page_zip.m_nonempty = false;

    buf_flush_init_for_writing(nullptr, page, &page_zip, 0,
                               fsp_is_checksum_disabled(space_id),
                               true /* skip_lsn_check */);

    err = os_file_write(request, path, file, page_zip.data, 0,
                        page_size.physical());

    ut_a(err != DB_IO_NO_PUNCH_HOLE);

    punch_hole = false;
  }

  ut::aligned_free(page);
  return err;
}

/** Create a tablespace (an IBD or IBT) file
@param[in]      space_id        Tablespace ID
@param[in]      name            Tablespace name in dbname/tablename format.
                                For general tablespaces, the 'dbname/' part
                                may be missing.
@param[in]      path            Path and filename of the datafile to create.
@param[in]      flags           Tablespace flags
@param[in]      size            Initial size of the tablespace file in pages,
                                must be >= FIL_IBD_FILE_INITIAL_SIZE
@param[in]      type            FIL_TYPE_TABLESPACE or FIL_TYPE_TEMPORARY
@return DB_SUCCESS or error code */
static dberr_t fil_create_tablespace(space_id_t space_id, const char *name,
                                     const char *path, uint32_t flags,
                                     page_no_t size, fil_type_t type) {
  ut_ad(!fsp_is_system_tablespace(space_id));
  ut_ad(!fsp_is_global_temporary(space_id));
  ut_a(fsp_flags_is_valid(flags));
  ut_a(type == FIL_TYPE_TEMPORARY || type == FIL_TYPE_TABLESPACE);

  bool has_shared_space = FSP_FLAGS_GET_SHARED(flags);
  /* Create the subdirectories in the path, if they are
  not there already. */
  if (!has_shared_space) {
    auto err = os_file_create_subdirs_if_needed(path);

    if (err != DB_SUCCESS) {
      return err; /* purecov: inspected */
    }
  }

  bool success = false;

  auto file = os_file_create(
      type == FIL_TYPE_TEMPORARY ? innodb_temp_file_key : innodb_data_file_key,
      path, OS_FILE_CREATE | OS_FILE_ON_ERROR_NO_EXIT, OS_FILE_NORMAL,
      OS_DATA_FILE, srv_read_only_mode && (type != FIL_TYPE_TEMPORARY),
      &success);

  if (!success) {
    /* purecov: begin inspected */
    /* The following call will print an error message */
    ulint error = os_file_get_last_error(true);

    ib::error(ER_IB_MSG_301, path);

    switch (error) {
      case OS_FILE_ALREADY_EXISTS:
#ifndef UNIV_HOTBACKUP
        ib::error(ER_IB_MSG_UNEXPECTED_FILE_EXISTS, path, path);
        return DB_TABLESPACE_EXISTS;
#else  /* !UNIV_HOTBACKUP */
        return DB_SUCCESS; /* Already existing file not an error here. */
#endif /* !UNIV_HOTBACKUP */

      case OS_FILE_NAME_TOO_LONG:
        ib::error(ER_IB_MSG_TOO_LONG_PATH, path);
        return DB_TOO_LONG_PATH;

      case OS_FILE_DISK_FULL:
        return DB_OUT_OF_DISK_SPACE;

      default:
        return DB_ERROR;
    }
    /* purecov: end */
  }

  bool atomic_write = false;
  bool punch_hole = false;

  auto err = fil_write_initial_pages(file, path, type, size, nullptr, space_id,
                                     flags, atomic_write, punch_hole);
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_304, path);

    os_file_close(file);
    os_file_delete(innodb_data_file_key, path);

    return err;
  }

  success = os_file_flush(file);

  if (!success) {
    ib::error(ER_IB_MSG_305, path);

    os_file_close(file);
    os_file_delete(innodb_data_file_key, path);
    return DB_ERROR;
  }

#ifndef UNIV_HOTBACKUP
  /* Notifier block covers space creation and initialization. */
  Clone_notify notifier(Clone_notify::Type::SPACE_CREATE, space_id, false);

  if (notifier.failed()) {
    os_file_close(file);
    return DB_ERROR;
  }
#endif /* !UNIV_HOTBACKUP */

  auto space = fil_space_create(name, space_id, flags, type);

  if (space == nullptr) {
    os_file_close(file);
    os_file_delete(innodb_data_file_key, path);
    return DB_ERROR;
  }

  DEBUG_SYNC_C("fil_ibd_created_space");

  auto shard = fil_system->shard_by_id(space_id);

  fil_node_t *file_node =
      shard->create_node(path, size, space, false, punch_hole, atomic_write);

  err = (file_node == nullptr) ? DB_ERROR : DB_SUCCESS;

#ifndef UNIV_HOTBACKUP
  /* Temporary tablespace creation need not be redo logged */
  if (err == DB_SUCCESS && type != FIL_TYPE_TEMPORARY) {
    ut_a(space->files.size() == 1);
    const auto &file = space->files.front();

    mtr_t mtr;

    mtr_start(&mtr);

    fil_op_write_log(MLOG_FILE_CREATE, space_id, file.name, nullptr,
                     space->flags, &mtr);

    mtr_commit(&mtr);

    DBUG_EXECUTE_IF("fil_ibd_create_log", log_make_latest_checkpoint(););
  }

#endif /* !UNIV_HOTBACKUP */

  /* For encryption tablespace, initial encryption information. */
  if (space != nullptr && FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
    err = fil_set_encryption(space->id, Encryption::AES, nullptr, nullptr);

    ut_ad(err == DB_SUCCESS);
  }

  space->encryption_op_in_progress = Encryption::Progress::NONE;

  os_file_close(file);
  if (err != DB_SUCCESS) {
    os_file_delete(innodb_data_file_key, path);
  }

  return err;
}

dberr_t fil_ibd_create(space_id_t space_id, const char *name, const char *path,
                       uint32_t flags, page_no_t size) {
  ut_a(size >= FIL_IBD_FILE_INITIAL_SIZE);
  ut_ad(!srv_read_only_mode);
  return fil_create_tablespace(space_id, name, path, flags, size,
                               FIL_TYPE_TABLESPACE);
}

dberr_t fil_ibt_create(space_id_t space_id, const char *name, const char *path,
                       uint32_t flags, page_no_t size) {
  ut_a(size >= FIL_IBT_FILE_INITIAL_SIZE);
  return fil_create_tablespace(space_id, name, path, flags, size,
                               FIL_TYPE_TEMPORARY);
}

#ifndef UNIV_HOTBACKUP
dberr_t fil_ibd_open(bool validate, fil_type_t purpose, space_id_t space_id,
                     uint32_t flags, const char *space_name,
                     const char *path_in, bool strict, bool old_space) {
  Datafile df;
  bool is_encrypted = FSP_FLAGS_GET_ENCRYPTION(flags);
  bool for_import = (purpose == FIL_TYPE_IMPORT);

  if (!fsp_flags_is_valid(flags)) {
    return DB_CORRUPTION;
  }

  /* Check if the file is already open. The space can be loaded
  via fil_space_get_first_path() on startup. This is a problem
  for partitioning code. It's a convoluted call graph via the DD.
  On Windows this can lead to a sharing violation when we attempt
  to open it again. */

  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  auto space = shard->get_space_by_id(space_id);

  shard->mutex_release();

  df.init(space_name, flags);

  if (path_in == nullptr) {
    df.make_filepath(nullptr, space_name, IBD);
  } else {
    df.set_filepath(path_in);
  }

  /* Attempt to open the tablespace. */
  if (df.open_read_only(strict) == DB_SUCCESS) {
    ut_ad(df.is_open());
  } else {
    ut_ad(!df.is_open());
    return DB_CANNOT_OPEN_FILE;
  }

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
  const bool atomic_write =
      !dblwr::is_enabled() && fil_fusionio_enable_atomic_write(df.handle());
#else
  const bool atomic_write = false;
#endif /* !NO_FALLOCATE && UNIV_LINUX */

  dberr_t err;

  if ((validate || is_encrypted) &&
      (err = df.validate_to_dd(space_id, flags, for_import)) != DB_SUCCESS) {
    /* We don't reply the rename via the redo log anymore.
    Therefore we can get a space ID mismatch when validating
    the files during bootstrap. */

    if (!is_encrypted && err != DB_WRONG_FILE_NAME) {
      /* The following call prints an error message.
      For encrypted tablespace we skip print, since it should
      be keyring plugin issues. */

      os_file_get_last_error(true);

      ib::error(ER_IB_MSG_306, space_name, TROUBLESHOOT_DATADICT_MSG);
    }

    return err;
  }

  if (validate && !old_space && !for_import) {
    if (df.server_version() > DD_SPACE_CURRENT_SRV_VERSION) {
      ib::error(ER_IB_MSG_1272, ulong{DD_SPACE_CURRENT_SRV_VERSION},
                ulonglong{df.server_version()});
      /* Server version is less than the tablespace server version.
      We don't support downgrade for 8.0 server, so report error */
      return DB_SERVER_VERSION_LOW;
    }
    ut_ad(df.space_version() == DD_SPACE_CURRENT_SPACE_VERSION);
  }

  /* We are done validating. If the tablespace is already open,
  return success. */
  if (space != nullptr) {
    return DB_SUCCESS;
  }

  /* We pass UNINITIALIZED flags while we try to open DD tablespace. In that
  case, set the flags now based on what is read from disk.*/
  if (FSP_FLAGS_ARE_NOT_SET(flags) && fsp_is_dd_tablespace(space_id)) {
    flags = df.flags();
    is_encrypted = FSP_FLAGS_GET_ENCRYPTION(flags);
  }

  space = fil_space_create(space_name, space_id, flags, purpose);

  if (space == nullptr) {
    return DB_ERROR;
  }

  /* We do not measure the size of the file, that is why
  we pass the 0 below */

  const fil_node_t *file =
      shard->create_node(df.filepath(), 0, space, false,
                         IORequest::is_punch_hole_supported(), atomic_write);

  if (file == nullptr) {
    return DB_ERROR;
  }

  /* Set encryption operation in progress */
  space->encryption_op_in_progress = df.m_encryption_op_in_progress;

  /* It's possible during Encryption processing, space flag for encryption
  has been updated in ibd file but server crashed before DD flags are
  updated. Thus, consider ibd setting for encryption. */
  if (FSP_FLAGS_GET_ENCRYPTION(df.flags())) {
    fsp_flags_set_encryption(space->flags);
  } else {
    fsp_flags_unset_encryption(space->flags);
  }

  /* For encryption tablespace, initialize encryption information.*/
  if ((is_encrypted || FSP_FLAGS_GET_ENCRYPTION(space->flags)) && !for_import) {
    dberr_t err;
    byte *iv = df.m_encryption_iv;
    byte *key = df.m_encryption_key;

    err = fil_set_encryption(space->id, Encryption::AES, key, iv);

    if (err != DB_SUCCESS) {
      return DB_ERROR;
    }

    /* If tablespace is encrypted with default master key and server has already
    started, rotate it now. */
    if (df.m_encryption_master_key_id == Encryption::DEFAULT_MASTER_KEY_ID &&
        /* There is no dependency on master thread but we are trying to check if
        server is in initial phase or not. */
        srv_master_thread_is_active()) {
      /* Reencrypt tablespace key */
      std::vector<space_id_t> sid;
      sid.push_back(space->id);
      fil_encryption_reencrypt(sid);
    }
  }

  return DB_SUCCESS;
}

#else  /* !UNIV_HOTBACKUP */

/** Allocates a file name for an old version of a single-table tablespace.
The string must be freed by caller with ut::free()!
@param[in]      name            Original file name
@return own: file name */
static char *meb_make_ibbackup_old_name(const char *name) {
  char *path;
  ulint len = strlen(name);
  static const char suffix[] = "_ibbackup_old_vers_";

  path = static_cast<char *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, len + 15 + sizeof(suffix)));

  memcpy(path, name, len);
  memcpy(path + len, suffix, sizeof(suffix) - 1);

  meb_sprintf_timestamp_without_extra_chars(path + len + sizeof(suffix) - 1);

  return path;
}
#endif /* UNIV_HOTBACKUP */

/** Looks for a pre-existing fil_space_t with the given tablespace ID
and, if found, returns the name and filepath in newly allocated buffers
that the caller must free.
@param[in]      space_id        The tablespace ID to search for.
@param[out]     name            Name of the tablespace found.
@param[out]     filepath        The filepath of the first datafile for the
tablespace.
@return true if tablespace is found, false if not. */
bool fil_space_read_name_and_filepath(space_id_t space_id, char **name,
                                      char **filepath) {
  bool success = false;

  *name = nullptr;
  *filepath = nullptr;

  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  if (space != nullptr) {
    *name = mem_strdup(space->name);

    ut_a(space->files.size() == 1);
    *filepath = mem_strdup(space->files.front().name);

    success = true;
  }

  shard->mutex_release();

  return success;
}

/** Convert a file name to a tablespace name. Strip the file name
prefix and suffix, leaving only databasename/tablename.
@param[in]      filename        directory/databasename/tablename.ibd
@return database/tablename string, to be freed with ut::free() */
char *fil_path_to_space_name(const char *filename) {
  std::string path{filename};
  auto pos = path.find_last_of(Fil_path::SEPARATOR);

  ut_a(pos != std::string::npos && !Fil_path::is_separator(path.back()));

  std::string db_name = path.substr(0, pos);
  std::string space_name = path.substr(pos + 1, path.length());

  /* If it is a path such as a/b/c.ibd, ignore everything before 'b'. */
  pos = db_name.find_last_of(Fil_path::SEPARATOR);

  if (pos != std::string::npos) {
    db_name = db_name.substr(pos + 1);
  }

  char *name;

  if (Fil_path::has_suffix(IBD, space_name)) {
    /* fil_space_t::name always uses '/' . */

    path = db_name;
    path.push_back('/');

    /* Strip the ".ibd" suffix. */
    path.append(space_name.substr(0, space_name.length() - 4));

    name = mem_strdupl(path.c_str(), path.length());

  } else {
    /* Must have an "undo" prefix. */
    ut_ad(space_name.find("undo") == 0);

    name = mem_strdupl(space_name.c_str(), space_name.length());
  }

  return name;
}

/** Open an ibd tablespace and add it to the InnoDB data structures.
This is similar to fil_ibd_open() except that it is used while processing
the redo and DDL log, so the data dictionary is not available and very little
validation is done. The tablespace name is extracted from the
dbname/tablename.ibd portion of the filename, which assumes that the file
is a file-per-table tablespace.  Any name will do for now.  General
tablespace names will be read from the dictionary after it has been
recovered.  The tablespace flags are read at this time from the first page
of the file in validate_for_recovery().
@param[in]      space_id        tablespace ID
@param[in]      path            path/to/databasename/tablename.ibd
@param[out]     space           the tablespace, or nullptr on error
@return status of the operation */
fil_load_status Fil_shard::ibd_open_for_recovery(space_id_t space_id,
                                                 const std::string &path,
                                                 fil_space_t *&space) {
  /* If the a space is already in the file system cache with this
  space ID, then there is nothing to do. */

  mutex_acquire();

  space = get_space_by_id(space_id);

  mutex_release();

  const char *filename = path.c_str();

  if (space != nullptr) {
    ut_a(space->files.size() == 1);

    const auto &file = space->files.front();

    /* Compare the real paths. */
    if (Fil_path::is_same_as(filename, file.name)) {
      return FIL_LOAD_OK;
    }

#ifdef UNIV_HOTBACKUP
    ib::trace_2() << "Ignoring data file '" << filename << "' with space ID "
                  << space->id << ". Another data file called '" << file.name
                  << "' exists with the same space ID";
#else  /* UNIV_HOTBACKUP */
    ib::info(ER_IB_MSG_307, filename, ulong{space->id}, file.name);
#endif /* UNIV_HOTBACKUP */

    space = nullptr;

    return FIL_LOAD_ID_CHANGED;
  }

  Datafile df;

  df.set_filepath(filename);

  if (df.open_read_only(false) != DB_SUCCESS) {
    return FIL_LOAD_NOT_FOUND;
  }

  ut_ad(df.is_open());

  /* Get and test the file size. */
  os_offset_t size = os_file_get_size(df.handle());

  /* Read and validate the first page of the tablespace. Assign a tablespace
  name based on the tablespace type. This will close the file, but will leave
  the flags and names to be queried. */
  dberr_t err = df.validate_for_recovery(space_id);

  ut_a(err == DB_SUCCESS || err == DB_INVALID_ENCRYPTION_META ||
       err == DB_CORRUPTION);

  if (err == DB_CORRUPTION) {
    return FIL_LOAD_DBWLR_CORRUPTION;
  }

  if (err == DB_INVALID_ENCRYPTION_META) {
    bool success = fil_system->erase_path(space_id);
    ut_a(success);
    return FIL_LOAD_NOT_FOUND;
  }

  ut_a(df.space_id() == space_id);

  /* Every .ibd file is created >= 4 pages in size.
  Smaller files cannot be OK. */
  os_offset_t minimum_size;

  /* Every .ibd file is created >= FIL_IBD_FILE_INITIAL_SIZE
  pages in size. Smaller files cannot be OK. */
  {
    const page_size_t page_size(df.flags());

    minimum_size = FIL_IBD_FILE_INITIAL_SIZE * page_size.physical();
  }

  if (size == static_cast<os_offset_t>(-1)) {
    /* The following call prints an error message */
    os_file_get_last_error(true);

    ib::error(ER_IB_MSG_308) << "Could not measure the size of"
                                " single-table tablespace file '"
                             << df.filepath() << "'";

  } else if (size < minimum_size) {
#ifndef UNIV_HOTBACKUP
    ib::error(ER_IB_MSG_309)
        << "The size of tablespace file '" << df.filepath() << "' is only "
        << size << ", should be at least " << minimum_size << "!";
#else
    /* In MEB, we work around this error. */
    df.set_space_id(SPACE_UNKNOWN);
    df.set_flags(0);
#endif /* !UNIV_HOTBACKUP */
  }

  ut_ad(space == nullptr);

#ifdef UNIV_HOTBACKUP
  if (df.space_id() == SPACE_UNKNOWN || df.space_id() == 0) {
    char *new_path;

    ib::info(ER_IB_MSG_310)
        << "Renaming tablespace file '" << df.filepath() << "' with space ID "
        << df.space_id() << " to " << df.name()
        << "_ibbackup_old_vers_<timestamp>"
           " because its size "
        << df.size()
        << " is too small"
           " (< 4 pages 16 kB each), or the space id in the"
           " file header is not sensible. This can happen in"
           " an mysqlbackup run, and is not dangerous.";
    df.close();

    new_path = meb_make_ibbackup_old_name(df.filepath());

    bool success =
        os_file_rename(innodb_data_file_key, df.filepath(), new_path);

    ut_a(success);

    ut::free(new_path);

    return FIL_LOAD_ID_CHANGED;
  }

  /* A backup may contain the same space several times, if the space got
  renamed at a sensitive time. Since it is enough to have one version of
  the space, we rename the file if a space with the same space id
  already exists in the tablespace memory cache. We rather rename the
  file than delete it, because if there is a bug, we do not want to
  destroy valuable data. */

  mutex_acquire();

  space = get_space_by_id(space_id);

  mutex_release();

  if (space != nullptr) {
    ib::info(ER_IB_MSG_311)
        << "Renaming data file '" << df.filepath() << "' with space ID "
        << space_id << " to " << df.name()
        << "_ibbackup_old_vers_<timestamp> because space " << space->name
        << " with the same id was scanned"
           " earlier. This can happen if you have renamed tables"
           " during an mysqlbackup run.";

    df.close();

    char *new_path = meb_make_ibbackup_old_name(df.filepath());

    bool success =
        os_file_rename(innodb_data_file_key, df.filepath(), new_path);

    ut_a(success);

    ut::free(new_path);
    return FIL_LOAD_OK;
  }
#endif /* UNIV_HOTBACKUP */
  std::string tablespace_name(df.name());

  /* During the apply-log operation, MEB already has translated the
  file name, so file name to space name conversion is not required. */
#ifndef UNIV_HOTBACKUP
  dict_name::convert_to_space(tablespace_name);
#endif /* !UNIV_HOTBACKUP */

  fil_system->mutex_acquire_all();

  space = space_create(tablespace_name.c_str(), space_id, df.flags(),
                       FIL_TYPE_TABLESPACE);

  fil_system->mutex_release_all();

  if (space == nullptr) {
    return FIL_LOAD_INVALID;
  }

  ut_ad(space->id == df.space_id());
  ut_ad(space->id == space_id);

  /* We do not use the size information we have about the file, because
  the rounding formula for extents and pages is somewhat complex; we
  let create_node() do that task. */

  const fil_node_t *file;

  file = create_node(df.filepath(), 0, space, false, true, false);

  ut_a(file != nullptr);

  /* For encryption tablespace, initial encryption information. */
  if (FSP_FLAGS_GET_ENCRYPTION(space->flags) &&
      df.m_encryption_key != nullptr) {
    dberr_t err = fil_set_encryption(space->id, Encryption::AES,
                                     df.m_encryption_key, df.m_encryption_iv);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_312, space->name);
    }
  }

  /* Set encryption operation in progress */
  space->encryption_op_in_progress = df.m_encryption_op_in_progress;
  space->m_header_page_flush_lsn = df.get_flush_lsn();

  return FIL_LOAD_OK;
}

/** Open an ibd tablespace and add it to the InnoDB data structures.
This is similar to fil_ibd_open() except that it is used while processing
the redo log, so the data dictionary is not available and very little
validation is done. The tablespace name is extracted from the
dbname/tablename.ibd portion of the filename, which assumes that the file
is a file-per-table tablespace.  Any name will do for now.  General
tablespace names will be read from the dictionary after it has been
recovered.  The tablespace flags are read at this time from the first page
of the file in validate_for_recovery().
@param[in]      space_id        tablespace ID
@param[in]      path            path/to/databasename/tablename.ibd
@param[out]     space           the tablespace, or nullptr on error
@return status of the operation */
fil_load_status Fil_system::ibd_open_for_recovery(space_id_t space_id,
                                                  const std::string &path,
                                                  fil_space_t *&space) {
  /* System tablespace open should never come here. It should be
  opened explicitly using the config path. */
  ut_a(space_id != TRX_SYS_SPACE);

#ifndef UNIV_HOTBACKUP
  /* Do not attempt to open or load for recovery any undo tablespace that
  is currently being truncated. */
  if (fsp_is_undo_tablespace(space_id) &&
      undo::is_active_truncate_log_present(undo::id2num(space_id))) {
    return FIL_LOAD_NOT_FOUND;
  }
#endif /* !UNIV_HOTBACKUP */

  auto shard = shard_by_id(space_id);

  return shard->ibd_open_for_recovery(space_id, path, space);
}

#ifndef UNIV_HOTBACKUP

/** Report that a tablespace for a table was not found.
@param[in]      name            Table name
@param[in]      space_id        Table's space ID */
static void fil_report_missing_tablespace(const char *name,
                                          space_id_t space_id) {
  ib::error(ER_IB_MSG_313)
      << "Table " << name << " in the InnoDB data dictionary has tablespace id "
      << space_id << ", but a tablespace with that id or name does not exist."
      << " Have you deleted or moved .ibd files?";
}

bool Fil_shard::adjust_space_name(fil_space_t *space,
                                  const char *dd_space_name) {
  if (!strcmp(space->name, dd_space_name)) {
    return true;
  }

  bool replace_general =
      FSP_FLAGS_GET_SHARED(space->flags) &&
      0 == strncmp(space->name, general_space_name, strlen(general_space_name));
  bool replace_undo =
      fsp_is_undo_tablespace(space->id) &&
      0 == strncmp(space->name, undo_space_name, strlen(undo_space_name));

  /* Update the auto-generated fil_space_t::name */
  if (replace_general || replace_undo) {
    char *old_space_name = space->name;
    char *new_space_name = mem_strdup(dd_space_name);

    update_space_name_map(space, new_space_name);

    space->name = new_space_name;

    ut::free(old_space_name);
  }

  /* Update the undo::Tablespace::name. Since the fil_shard mutex is held by
  the caller, it would be a sync order violation to get undo::spaces->s_lock.
  It is OK to skip this s_lock since this occurs during boot_tablespaces()
  which is still single threaded. */
  if (replace_undo) {
    space_id_t space_num = undo::id2num(space->id);
    undo::Tablespace *undo_space = undo::spaces->find(space_num);
    undo_space->set_space_name(dd_space_name);
  }

  return (replace_general || replace_undo);
}

bool Fil_shard::space_check_exists(space_id_t space_id, const char *name,
                                   bool print_err, bool adjust_space) {
  fil_space_t *fnamespace = nullptr;

  mutex_acquire();

  /* Look if there is a space with the same id */
  fil_space_t *space = get_space_by_id(space_id);

  /* name is nullptr when replaying a DELETE ddl log. */
  if (name == nullptr) {
    mutex_release();
    return (space != nullptr);
  }

  if (space != nullptr) {
    /* No need to check a general tablespace name if the DD
    is not yet available. */
    if (!srv_sys_tablespaces_open && FSP_FLAGS_GET_SHARED(space->flags)) {
      mutex_release();
      return true;
    }

    /* Sometimes the name has been auto-generated when the
    datafile is discovered and needs to be adjusted to that
    of the DD. This happens for general and undo tablespaces. */
    if (srv_sys_tablespaces_open && adjust_space &&
        adjust_space_name(space, name)) {
      mutex_release();
      return true;
    }

    /* If this space has the expected name, use it. */
    fnamespace = get_space_by_name(name);

    if (space == fnamespace) {
      /* Found */
      mutex_release();
      return true;
    }
  }

  /* Info from "fnamespace" comes from the ibd file itself, it can
  be different from data obtained from System tables since file
  operations are not transactional. If adjust_space is set, and the
  mismatching space are between a user table and its temp table, we
  shall adjust the ibd file name according to system table info */
  if (adjust_space && space != nullptr &&
      row_is_mysql_tmp_table_name(space->name) &&
      !row_is_mysql_tmp_table_name(name)) {
    /* Atomic DDL's "ddl_log" will adjust the tablespace name. */
    mutex_release();

    return true;

  } else if (!print_err) {
    ;

  } else if (space == nullptr) {
    if (fnamespace == nullptr) {
      if (print_err) {
        fil_report_missing_tablespace(name, space_id);
      }

    } else {
      ib::error(ER_IB_MSG_314)
          << "Table " << name << " in InnoDB data dictionary has tablespace id "
          << space_id << ", but a tablespace with that id does not exist."
          << " But there is a tablespace of name " << fnamespace->name
          << " and id " << fnamespace->id
          << ". Have you deleted or moved .ibd files?";
    }

    ib::warn(ER_IB_MSG_315) << TROUBLESHOOT_DATADICT_MSG;

  } else if (0 != strcmp(space->name, name)) {
    ib::error(ER_IB_MSG_316)
        << "Table " << name << " in InnoDB data dictionary"
        << " has tablespace id " << space_id
        << ", but the tablespace with that id has name " << space->name
        << ". Have you deleted or moved .ibd files?";

    if (fnamespace != nullptr) {
      ib::error(ER_IB_MSG_317)
          << "There is a tablespace with the name " << fnamespace->name
          << ", but its id is " << fnamespace->id << ".";
    }

    ib::warn(ER_IB_MSG_318) << TROUBLESHOOT_DATADICT_MSG;
  }

  mutex_release();

  return false;
}

bool fil_space_exists_in_mem(space_id_t space_id, const char *name,
                             bool print_err, bool adjust_space) {
  auto shard = fil_system->shard_by_id(space_id);

  return shard->space_check_exists(space_id, name, print_err, adjust_space);
}
#endif /* !UNIV_HOTBACKUP */

/** Returns the space ID based on the tablespace name.
The tablespace must be found in the tablespace memory cache.
This call is made from external to this module, so the mutex is not owned.
@param[in]      name            Tablespace name
@return space ID if tablespace found, SPACE_UNKNOWN if space not. */
space_id_t fil_space_get_id_by_name(const char *name) {
  auto space = fil_system->get_space_by_name(name);

  return (space == nullptr) ? SPACE_UNKNOWN : space->id;
}

/** Fill the pages with NULs
@param[in] file         Tablespace file
@param[in] page_size    physical page size
@param[in] start        Offset from the start of the file in bytes
@param[in] len          Length in bytes
@return DB_SUCCESS or error code */
static dberr_t fil_write_zeros(const fil_node_t *file, ulint page_size,
                               os_offset_t start, os_offset_t len) {
  ut_a(len > 0);

  /* Extend at most 1M at a time */
  os_offset_t n_bytes = std::min(static_cast<os_offset_t>(1024 * 1024), len);

  byte *buf = reinterpret_cast<byte *>(ut::aligned_zalloc(n_bytes, page_size));

  os_offset_t offset = start;
  dberr_t err = DB_SUCCESS;
  const os_offset_t end = start + len;
  IORequest request(IORequest::WRITE);

  while (offset < end) {
    err =
        os_file_write(request, file->name, file->handle, buf, offset, n_bytes);

    if (err != DB_SUCCESS) {
      break;
    }

    offset += n_bytes;

    n_bytes = std::min(n_bytes, end - offset);

    DBUG_EXECUTE_IF("ib_crash_during_tablespace_extension", DBUG_SUICIDE(););
  }

  ut::aligned_free(buf);

  return err;
}

/** Try to extend a tablespace if it is smaller than the specified size.
@param[in,out]  space           tablespace
@param[in]      size            desired size in pages
@return whether the tablespace is at least as big as requested */
bool Fil_shard::space_extend(fil_space_t *space, page_no_t size) {
  /* In read-only mode we allow write to shared temporary tablespace
  as intrinsic table created by Optimizer reside in this tablespace. */
  ut_ad(!srv_read_only_mode || fsp_is_system_temporary(space->id));

#ifndef UNIV_HOTBACKUP
  DBUG_EXECUTE_IF("fil_space_print_xdes_pages",
                  space->print_xdes_pages("xdes_pages.log"););
#endif /* !UNIV_HOTBACKUP */

  fil_node_t *file;
  bool success = true;

#ifdef UNIV_HOTBACKUP
  page_no_t prev_size = 0;
#endif /* UNIV_HOTBACKUP */

  for (;;) {
    mutex_acquire();
    space = get_space_by_id(space->id);

    /* Note:If the file is being opened for the first time then
    we don't have the file physical size. There is no guarantee
    that the file has been opened at this stage. */

    if (size < space->size) {
      /* Space already big enough */
      mutex_release();

      return true;
    }

    file = &space->files.back();

    if (!file->is_being_extended) {
      /* Mark this file as undergoing extension. This flag
      is used to synchronize threads to execute space extension in order. */

      file->is_being_extended = true;

      break;
    }

    /* Another thread is currently using the file. Wait
    for it to finish.  It'd have been better to use an event
    driven mechanism but the entire module is peppered with
    polling code. */

    mutex_release();

    if (!tbsp_extend_and_initialize) {
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  if (!prepare_file_for_io(file)) {
    /* The tablespace data file, such as .ibd file, is missing */
    ut_a(file->is_being_extended);
    file->is_being_extended = false;

    mutex_release();

    return false;
  }

  ut_a(file->is_open);

  const page_size_t page_size(space->flags);
  const size_t phy_page_size = page_size.physical();

#ifdef UNIV_HOTBACKUP
  prev_size = space->size;

  ib::trace_1() << "Extending space id : " << space->id
                << ", space name : " << space->name
                << ", space size : " << space->size
                << " pages, page size : " << phy_page_size
                << ", to size : " << size;
#endif /* UNIV_HOTBACKUP */

  if (size <= space->size) {
    ut_a(file->is_being_extended);
    file->is_being_extended = false;

    complete_io(file, IORequestRead);

    mutex_release();

    return true;
  }

  /* At this point it is safe to release the shard mutex. No
  other thread can rename, delete or close the file because
  we have set the file->in_use flag. */

  mutex_release();

  page_no_t pages_added;
  os_offset_t node_start = os_file_get_size(file->handle);

  ut_a(node_start != (os_offset_t)-1);

  /* File first page number */
  page_no_t node_first_page = space->size - file->size;

  /* Number of physical pages in the file */
  page_no_t n_node_physical_pages =
      static_cast<page_no_t>(node_start / phy_page_size);

  /* Number of pages to extend in the file */
  page_no_t n_node_extend;

  n_node_extend = size - (node_first_page + file->size);

  /* If we already have enough physical pages to satisfy the
  extend request on the file then ignore it */
  if (file->size + n_node_extend > n_node_physical_pages) {
    DBUG_EXECUTE_IF("ib_crash_during_tablespace_extension", DBUG_SUICIDE(););

    os_offset_t len;
    dberr_t err = DB_SUCCESS;

    len = ((file->size + n_node_extend) * phy_page_size) - node_start;

    ut_ad(len > 0);

#if !defined(UNIV_HOTBACKUP) && defined(UNIV_LINUX)
    /* Do not write redo log record for temporary tablespace
    and the system tablespace as they don't need to be recreated.
    Temporary tablespaces are reinitialized during startup and
    hence need not be recovered during recovery. The system
    tablespace is neither recreated nor resized and hence we do
    not need to redo log any operations on it. */
    if (!recv_recovery_is_on() && space->purpose != FIL_TYPE_TEMPORARY &&
        space->id != TRX_SYS_SPACE) {
      /* Write the redo log record for extending the space */
      mtr_t mtr;
      mtr_start(&mtr);

      ut_ad(node_start > 0);
      ut_ad(len > 0);

      /* The posix_fallocate() reserves the desired space and updates
      the file metadata in the filesystem. On successful execution, any
      subsequent attempt to access this newly allocated space will see
      it as initialized because of the updated filesystem metadata.

      However, the posix_fallocate() call used to allocate space seems
      to have an atomicity issue where it can fail after reserving the
      space, but before updating the file metadata in the filesystem.

      Offset is required to be written in the redo log record to find
      out the position in the file where it was extended during space
      extend operation. The offset value written in the redo log can
      be directly used to find the starting point for initializing
      the file during recovery. In case posix_fallocate() crashes as
      described above, it will be difficult to find out the old size
      of the file and hence it will be difficult to find out the exact
      region which needs to be initialized by writing 0's. */

      fil_op_write_space_extend(space->id, node_start, len, &mtr);

      mtr_commit(&mtr);

      DBUG_INJECT_CRASH_WITH_LOG_FLUSH("ib_crash_after_writing_redo_extend", 1);

      /* NOTE: Though against for the Write-Ahead-Log principal,
      log_write_up_to() is not needed here, because no file shrinks and
      duplicate extending is allowed. And log_write_up_to() here helps
      nothing for fallocate() inconsistency. */
    }
#endif /* !UNIV_HOTBACKUP && UNIV_LINUX */

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
    /* This is required by FusionIO HW/Firmware */

    int ret = posix_fallocate(file->handle.m_file, node_start, len);

    DBUG_EXECUTE_IF("ib_posix_fallocate_fail_eintr", ret = EINTR;);

    DBUG_EXECUTE_IF("ib_posix_fallocate_fail_einval", ret = EINVAL;);

    if (ret != 0) {
      /* We already pass the valid offset and len in, if EINVAL
      is returned, it could only mean that the file system doesn't
      support fallocate(), currently one known case is ext3 with O_DIRECT.

      Also because above call could be interrupted, in this case,
      simply go to plan B by writing zeroes.

      Both error messages for above two scenarios are skipped in case
      of flooding error messages, because they can be ignored by users. */
      if (ret != EINTR && ret != EINVAL) {
        ib::error(ER_IB_MSG_319)
            << "posix_fallocate(): Failed to preallocate"
               " data for file "
            << file->name << ", desired size " << len
            << " bytes."
               " Operating system error number "
            << ret
            << ". Check"
               " that the disk is not full or a disk quota"
               " exceeded. Make sure the file system supports"
               " this function. Refer to your operating system"
               " documentation for operating system error code"
               " information.";
      }

      err = DB_IO_ERROR;
    }
#endif /* NO_FALLOCATE || !UNIV_LINUX */

    if ((tbsp_extend_and_initialize && !file->atomic_write) ||
        err == DB_IO_ERROR) {
      err = fil_write_zeros(file, phy_page_size, node_start, len);

      if (err != DB_SUCCESS) {
        ib::warn(ER_IB_MSG_320)
            << "Error while writing " << len << " zeroes to " << file->name
            << " starting at offset " << node_start;
      }
    }

    /* Check how many pages actually added */
    os_offset_t end = os_file_get_size(file->handle);
    ut_a(end != static_cast<os_offset_t>(-1) && end >= node_start);

    success = (end == node_start + len);
    os_has_said_disk_full = !success;

    pages_added = static_cast<page_no_t>(end / phy_page_size);

    ut_a(pages_added >= file->size);
    pages_added -= file->size;

  } else {
    success = true;
    pages_added = n_node_extend;
    os_has_said_disk_full = false;
  }

  mutex_acquire();

  file->size += pages_added;
  space->size += pages_added;

  ut_a(file->is_being_extended);
  file->is_being_extended = false;

  complete_io(file, IORequestWrite);

#ifndef UNIV_HOTBACKUP
  /* Keep the last data file size info up to date, rounded to
  full megabytes */
  page_no_t pages_per_mb =
      static_cast<page_no_t>((1024 * 1024) / phy_page_size);

  page_no_t size_in_pages = ((file->size / pages_per_mb) * pages_per_mb);

  if (space->id == TRX_SYS_SPACE) {
    srv_sys_space.set_last_file_size(size_in_pages);
  } else if (fsp_is_system_temporary(space->id)) {
    srv_tmp_space.set_last_file_size(size_in_pages);
  }
#else  /* !UNIV_HOTBACKUP */
  ib::trace_2() << "Extended space : " << space->name << " from " << prev_size
                << " pages to " << space->size << " pages "
                << ", desired space size : " << size << " pages";
#endif /* !UNIV_HOTBACKUP */

  space_flush(space->id);

  mutex_release();

  DBUG_EXECUTE_IF("fil_crash_after_extend", DBUG_SUICIDE(););
  return success;
}

/** Try to extend a tablespace if it is smaller than the specified size.
@param[in,out]  space           Tablespace ID
@param[in]      size            desired size in pages
@return whether the tablespace is at least as big as requested */
bool fil_space_extend(fil_space_t *space, page_no_t size) {
  auto shard = fil_system->shard_by_id(space->id);

  return shard->space_extend(space, size);
}

#ifdef UNIV_HOTBACKUP
/** Extends all tablespaces to the size stored in the space header. During the
mysqlbackup --apply-log phase we extended the spaces on-demand so that log
records could be applied, but that may have left spaces still too small
compared to the size stored in the space header. */
void Fil_shard::meb_extend_tablespaces_to_stored_len() {
  ut_ad(mutex_owned());

  byte *buf = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, UNIV_PAGE_SIZE));

  ut_a(buf != nullptr);

  for (auto &elem : m_spaces) {
    auto space = elem.second;

    ut_a(space->purpose == FIL_TYPE_TABLESPACE);

    /* No need to protect with a mutex, because this is
    a single-threaded operation */

    mutex_release();

    dberr_t error;

    const page_size_t page_size(space->flags);

    error = fil_read(page_id_t(space->id, 0), page_size, 0,
                     page_size.physical(), buf);

    ut_a(error == DB_SUCCESS);

    ulint size_in_header;

    size_in_header = fsp_header_get_field(buf, FSP_SIZE);

    bool success;

    success = space_extend(space, size_in_header);

    if (!success) {
      ib::error(ER_IB_MSG_321)
          << "Could not extend the tablespace of " << space->name
          << " to the size stored in"
             " header, "
          << size_in_header
          << " pages;"
             " size after extension "
          << 0
          << " pages. Check that you have free disk"
             " space and retry!";

      ut_a(success);
    }

    mutex_acquire();
  }

  ut::free(buf);
}

/** Extends all tablespaces to the size stored in the space header. During the
mysqlbackup --apply-log phase we extended the spaces on-demand so that log
records could be applied, but that may have left spaces still too small
compared to the size stored in the space header. */
void meb_extend_tablespaces_to_stored_len() {
  fil_system->meb_extend_tablespaces_to_stored_len();
}

bool meb_is_redo_log_only_restore = false;

/** Determine if file is intermediate / temporary. These files are
created during reorganize partition, rename tables, add / drop columns etc.
@param[in]      filepath        absolute / relative or simply file name
@retvalue       true            if it is intermediate file
@retvalue       false           if it is normal file */
bool meb_is_intermediate_file(const std::string &filepath) {
  std::string file_name = filepath;

  {
    /** If its redo only restore, apply log needs to got through the
        intermediate steps to apply a ddl.
        Some of these operation might result in intermediate files.
    */
    if (meb_is_redo_log_only_restore) return false;
    /* extract file name from relative or absolute file name */
    auto pos = file_name.rfind(OS_PATH_SEPARATOR);

    if (pos != std::string::npos) {
      ++pos;
      file_name = file_name.substr(pos);
    }
  }

  transform(file_name.begin(), file_name.end(), file_name.begin(), ::tolower);

  if (file_name[0] != '#') {
    auto pos = file_name.rfind("#tmp#.ibd");
    if (pos != std::string::npos) {
      return true;
    } else {
      return false; /* normal file name */
    }
  }

  static std::vector<std::string> prefixes = {"#sql-", "#sql2-", "#tmp#",
                                              "#ren#"};

  /* search for the unsupported patterns */
  for (const auto &prefix : prefixes) {
    if (Fil_path::has_prefix(file_name, prefix)) {
      return true;
    }
  }

  return false;
}

/** Return the space ID based of the remote general tablespace name.
This is a wrapper over fil_space_get_id_by_name() method. it means,
the tablespace must be found in the tablespace memory cache.
This method extracts the tablespace name from input parameters and checks if
it has been loaded in memory cache through either any of the remote general
tablespaces directories identified at the time memory cache created.
@param[in, out] tablespace      Tablespace name
@return space ID if tablespace found, SPACE_UNKNOWN if not found. */
space_id_t meb_fil_space_get_rem_gen_ts_id_by_name(std::string &tablespace) {
  space_id_t space_id = SPACE_UNKNOWN;

  for (auto newpath : rem_gen_ts_dirs) {
    auto pos = tablespace.rfind(OS_PATH_SEPARATOR);

    if (pos == std::string::npos) {
      break;
    }

    newpath += tablespace.substr(pos);

    space_id = fil_space_get_id_by_name(newpath.c_str());

    if (space_id != SPACE_UNKNOWN) {
      tablespace = newpath;
      break;
    }
  }

  return space_id;
}

/** Tablespace item during recovery */
struct MEB_file_name {
  /** Constructor */
  MEB_file_name(std::string name, bool deleted)
      : m_name(name), m_space(), m_deleted(deleted) {}

  /** Tablespace file name (MLOG_FILE_NAME) */
  std::string m_name;

  /** Tablespace object (NULL if not valid or not found) */
  fil_space_t *m_space;

  /** Whether the tablespace has been deleted */
  bool m_deleted;
};

/** Map of dirty tablespaces during recovery */
using MEB_recv_spaces =
    std::map<space_id_t, MEB_file_name, std::less<space_id_t>,
             ut::allocator<std::pair<const space_id_t, MEB_file_name>>>;

static MEB_recv_spaces recv_spaces;

/** Checks if MEB has loaded this space for reovery.
@param[in]      space_id        Tablespace ID
@return true if the space_id is loaded */
bool meb_is_space_loaded(const space_id_t space_id) {
  return (recv_spaces.find(space_id) != recv_spaces.end());
}

/** Set the keys for an encrypted tablespace.
@param[in]      space           Tablespace for which to set the key */
static void meb_set_encryption_key(const fil_space_t *space) {
  ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

  for (auto &key : *recv_sys->keys) {
    if (key.space_id != space->id) {
      continue;
    }

    dberr_t err;

    err = fil_set_encryption(space->id, Encryption::AES, key.ptr, key.iv);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_322) << "Can't set encryption information"
                               << " for tablespace" << space->name << "!";
    }

    ut::free(key.iv);
    ut::free(key.ptr);

    key.iv = nullptr;
    key.ptr = nullptr;
    key.space_id = 0;
  }
}

/** Process a file name passed as an input
Wrapper around meb_name_process()
@param[in,out]  name            absolute path of tablespace file
@param[in]      space_id        The tablespace ID
@param[in]      deleted         true if MLOG_FILE_DELETE */
void Fil_system::meb_name_process(char *name, space_id_t space_id,
                                  bool deleted) {
  ut_ad(space_id != TRX_SYS_SPACE);

  /* We will also insert space=nullptr into the map, so that
  further checks can ensure that a MLOG_FILE_NAME record was
  scanned before applying any page records for the space_id. */

  Fil_path::normalize(name);

  size_t len = std::strlen(name);

  MEB_file_name fname(std::string(name, len - 1), deleted);

  auto p = recv_spaces.insert(std::make_pair(space_id, fname));

  ut_ad(p.first->first == space_id);

  MEB_file_name &f = p.first->second;

  if (deleted) {
    /* Got MLOG_FILE_DELETE */

    if (!p.second && !f.m_deleted) {
      f.m_deleted = true;

      if (f.m_space != nullptr) {
        f.m_space = nullptr;
      }
    }

    ut_ad(f.m_space == nullptr);

  } else if (p.second || f.m_name != fname.m_name) {
    fil_space_t *space;

    /* Check if the tablespace file exists and contains
    the space_id. If not, ignore the file after displaying
    a note. Abort if there are multiple files with the
    same space_id. */

    switch (ibd_open_for_recovery(space_id, name, space)) {
      case FIL_LOAD_OK:
        ut_ad(space != nullptr);

        /* For encrypted tablespace, set key and iv. */
        if (FSP_FLAGS_GET_ENCRYPTION(space->flags) &&
            recv_sys->keys != nullptr) {
          meb_set_encryption_key(space);
        }

        if (f.m_space == nullptr || f.m_space == space) {
          f.m_name = fname.m_name;
          f.m_space = space;
          f.m_deleted = false;

        } else {
          ib::error(ER_IB_MSG_323)
              << "Tablespace " << space_id << " has been found in two places: '"
              << f.m_name << "' and '" << name
              << "'."
                 " You must delete one of them.";

          recv_sys->found_corrupt_fs = true;
        }
        break;

      case FIL_LOAD_ID_CHANGED:
        ut_ad(space == nullptr);

        ib::trace_1() << "Ignoring file " << name << " for space-id mismatch "
                      << space_id;
        break;

      case FIL_LOAD_NOT_FOUND:
        /* No matching tablespace was found; maybe it
        was renamed, and we will find a subsequent
        MLOG_FILE_* record. */
        ut_ad(space == nullptr);
        break;

      case FIL_LOAD_INVALID:
        ut_ad(space == nullptr);

        ib::warn(ER_IB_MSG_324) << "Invalid tablespace " << name;
        break;

      case FIL_LOAD_MISMATCH:
        ut_ad(space == nullptr);
        break;
      case FIL_LOAD_DBWLR_CORRUPTION:
        ut_ad(space == nullptr);
        break;
    }
  }
}

/** Process a file name passed as an input
Wrapper around meb_name_process()
@param[in]      name            absolute path of tablespace file
@param[in]      space_id        the tablespace ID */
void meb_fil_name_process(const char *name, space_id_t space_id) {
  char *file_name = static_cast<char *>(mem_strdup(name));

  fil_system->meb_name_process(file_name, space_id, false);

  ut::free(file_name);
}

/** Test, if a file path name contains a back-link ("../").
We assume a path to a file. So we don't check for a trailing "/..".
@param[in]      path            path to check
@return whether the path contains a back-link.
 */
static bool meb_has_back_link(const std::string &path) {
#ifdef _WIN32
  static const std::string DOT_DOT_SLASH = "..\\";
  static const std::string SLASH_DOT_DOT_SLASH = "\\..\\";
#else
  static const std::string DOT_DOT_SLASH = "../";
  static const std::string SLASH_DOT_DOT_SLASH = "/../";
#endif /* _WIN32 */
  return ((0 == path.compare(0, 3, DOT_DOT_SLASH)) ||
          (std::string::npos != path.find(SLASH_DOT_DOT_SLASH)));
}

/** Parse a file name retrieved from a MLOG_FILE_* record,
and return the absolute file path corresponds to backup dir
as well as in the form of database/tablespace
@param[in]      name            path emitted by the redo log
@param[in]      flags           flags emitted by the redo log
@param[in]      space_id        space_id emitted by the redo log
@param[out]     absolute_path   absolute path of tablespace
corresponds to target dir
@param[out]     tablespace_name name in the form of database/table */
static void meb_make_abs_file_path(const std::string &name, uint32_t flags,
                                   space_id_t space_id,
                                   std::string &absolute_path,
                                   std::string &tablespace_name) {
  Datafile df;
  std::string file_name = name;

  /* If the tablespace path name is absolute or has back-links ("../"),
  we assume, that it is located outside of datadir. */
  if (Fil_path::is_absolute_path(file_name.c_str()) ||
      (meb_has_back_link(file_name) && !replay_in_datadir)) {
    if (replay_in_datadir) {
      /* This is an apply-log in the restored datadir. Take the path as is. */
      df.set_filepath(file_name.c_str());
    } else {
      /* This is an apply-log in backup_dir/datadir. Get the file inside. */
      auto pos = file_name.rfind(OS_PATH_SEPARATOR);

      /* if it is file per tablespace, then include the schema
      directory as well */
      if (fsp_is_file_per_table(space_id, flags) && pos != std::string::npos) {
        pos = file_name.rfind(OS_PATH_SEPARATOR, pos - 1);
      }

      if (pos == std::string::npos) {
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_325)
            << "Could not extract the tablespace"
            << " file name from the in the path : " << name;
      }

      ++pos;

      file_name = file_name.substr(pos);

      df.make_filepath(MySQL_datadir_path, file_name.c_str(), IBD);
    }

  } else {
    /* This is an apply-log with a relative path, either in the restored
    datadir, or in backup_dir/datadir. If in the restored datadir, the
    path might start with "../" to reach outside of datadir. */
    auto pos = file_name.find(OS_PATH_SEPARATOR);

    /* Remove the cur dir from the path as this will cause the
    path name mismatch when we try to find out the space_id based
    on tablespace name */

    if (file_name.substr(0, pos) == ".") {
      ++pos;
      file_name = file_name.substr(pos);
    }

    /* make_filepath() does not prepend the directory, if the file name
    starts with "../". Prepend it unconditionally here. */
    file_name.insert(0, 1, OS_PATH_SEPARATOR);
    file_name.insert(0, MySQL_datadir_path);

    df.make_filepath(nullptr, file_name.c_str(), IBD);
  }

  df.set_flags(flags);
  df.set_space_id(space_id);
  df.set_name(nullptr);

  absolute_path = df.filepath();

  tablespace_name = df.name();
}

/** Process a MLOG_FILE_CREATE redo record.
@param[in]      page_id         Page id of the redo log record
@param[in]      flags           Tablespace flags
@param[in]      name            Tablespace filename */
static void meb_tablespace_redo_create(const page_id_t &page_id, uint32_t flags,
                                       const char *name) {
  std::string abs_file_path;
  std::string tablespace_name;

  meb_make_abs_file_path(name, flags, page_id.space(), abs_file_path,
                         tablespace_name);

  if (meb_is_intermediate_file(abs_file_path.c_str()) ||
      fil_space_get(page_id.space()) ||
      fil_space_get_id_by_name(tablespace_name.c_str()) != SPACE_UNKNOWN ||
      meb_fil_space_get_rem_gen_ts_id_by_name(tablespace_name) !=
          SPACE_UNKNOWN) {
    /* Don't create table while :-
    1. scanning the redo logs during backup
    2. apply-log on a partial backup
    3. if it is intermediate file
    4. tablespace is already loaded in memory
    5. tablespace is a remote general tablespace which is
       already loaded for recovery/apply-log from different
       directory path */

    ib::trace_1() << "Ignoring the log record. No need to "
                  << "create the tablespace : " << abs_file_path;
  } else {
    auto it = recv_spaces.find(page_id.space());

    if (it == recv_spaces.end() || it->second.m_name != abs_file_path) {
      ib::trace_1() << "Creating the tablespace : " << abs_file_path
                    << ", space_id : " << page_id.space();

      dberr_t ret = fil_ibd_create(page_id.space(), tablespace_name.c_str(),
                                   abs_file_path.c_str(), flags,
                                   FIL_IBD_FILE_INITIAL_SIZE);

      if (ret != DB_SUCCESS) {
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_326)
            << "Could not create the tablespace : " << abs_file_path
            << " with space Id : " << page_id.space();
      }
    }
  }
}

/** Process a MLOG_FILE_RENAME redo record.
@param[in]      page_id         Page id of the redo log record
@param[in]      from_name       Tablespace from filename
@param[in]      to_name         Tablespace to filename */
static void meb_tablespace_redo_rename(const page_id_t &page_id,
                                       const char *from_name,
                                       const char *to_name) {
  std::string abs_to_path;
  std::string abs_from_path;
  std::string tablespace_name;

  meb_make_abs_file_path(from_name, 0, page_id.space(), abs_from_path,
                         tablespace_name);

  meb_make_abs_file_path(to_name, 0, page_id.space(), abs_to_path,
                         tablespace_name);

  char *new_name = nullptr;

  if (meb_is_intermediate_file(from_name) ||
      meb_is_intermediate_file(to_name) ||
      fil_space_get_id_by_name(tablespace_name.c_str()) != SPACE_UNKNOWN ||
      meb_fil_space_get_rem_gen_ts_id_by_name(tablespace_name) !=
          SPACE_UNKNOWN ||
      fil_space_get(page_id.space()) == nullptr) {
    /* Don't rename table while :
    1. Scanning the redo logs during backup
    2. Apply-log on a partial backup
    3. Either of old or new tables are intermediate table
    4. The new name is already loaded for recovery/apply-log
    5. The new name is a remote general tablespace which is
       already loaded for recovery/apply-log from different
       directory path
    6. Tablespace is not yet loaded in memory.
    This will prevent unintended renames during recovery. */

    ib::trace_1() << "Ignoring the log record. "
                  << "No need to rename tablespace";

    return;

  } else {
    ib::trace_1() << "Renaming space id : " << page_id.space()
                  << ", old tablespace name : " << from_name
                  << " to new tablespace name : " << to_name;

    new_name = static_cast<char *>(mem_strdup(abs_to_path.c_str()));
  }

  meb_fil_name_process(from_name, page_id.space());
  meb_fil_name_process(new_name, page_id.space());

  if (!fil_op_replay_rename(page_id, abs_from_path.c_str(),
                            abs_to_path.c_str())) {
    recv_sys->found_corrupt_fs = true;
  }

  meb_fil_name_process(to_name, page_id.space());

  ut::free(new_name);
}

/** Process a MLOG_FILE_DELETE redo record.
@param[in]      page_id         Page id of the redo log record
@param[in]      name            Tablespace filename */
static void meb_tablespace_redo_delete(const page_id_t &page_id,
                                       const char *name) {
  std::string abs_file_path;
  std::string tablespace_name;

  meb_make_abs_file_path(name, 0, page_id.space(), abs_file_path,
                         tablespace_name);

  char *file_name = static_cast<char *>(mem_strdup(name));

  fil_system->meb_name_process(file_name, page_id.space(), true);

  if (fil_space_get(page_id.space())) {
    ib::trace_1() << "Deleting the tablespace : " << abs_file_path
                  << ", space_id : " << page_id.space();
    dberr_t err =
        fil_delete_tablespace(page_id.space(), BUF_REMOVE_FLUSH_NO_WRITE);

    ut_a(err == DB_SUCCESS);
  }

  ut::free(file_name);
}

#endif /* UNIV_HOTBACKUP */

/*========== RESERVE FREE EXTENTS (for a B-tree split, for example) ===*/

/** Tries to reserve free extents in a file space.
@param[in]      space_id        Tablespace ID
@param[in]      n_free_now      Number of free extents now
@param[in]      n_to_reserve    How many one wants to reserve
@return true if succeed */
bool fil_space_reserve_free_extents(space_id_t space_id, ulint n_free_now,
                                    ulint n_to_reserve) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  bool success;

  if (space->n_reserved_extents + n_to_reserve > n_free_now) {
    success = false;
  } else {
    ut_a(n_to_reserve < std::numeric_limits<uint32_t>::max());
    space->n_reserved_extents += (uint32_t)n_to_reserve;
    success = true;
  }

  shard->mutex_release();

  return success;
}

/** Releases free extents in a file space.
@param[in]      space_id        Tablespace ID
@param[in]      n_reserved      How many were reserved */
void fil_space_release_free_extents(space_id_t space_id, ulint n_reserved) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  ut_a(n_reserved < std::numeric_limits<uint32_t>::max());
  ut_a(space->n_reserved_extents >= n_reserved);

  space->n_reserved_extents -= (uint32_t)n_reserved;

  shard->mutex_release();
}

/** Gets the number of reserved extents. If the database is silent, this number
should be zero.
@param[in]      space_id        Tablespace ID
@return the number of reserved extents */
ulint fil_space_get_n_reserved_extents(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  ulint n = space->n_reserved_extents;

  shard->mutex_release();

  return n;
}

/*============================ FILE I/O ================================*/

bool Fil_shard::prepare_file_for_io(fil_node_t *file) {
  ut_ad(mutex_owned());

  fil_space_t *space = file->space;

  if (space->is_deleted()) {
    return false;
  }

  if (!file->is_open) {
    ut_a(file->n_pending_ios == 0);

    if (!open_file(file)) {
      return false;
    }
  }
  if (file->n_pending_ios == 0) {
    remove_from_LRU(file);
  }

  ++file->n_pending_ios;

  /* The file can't be in the LRU list. */
  ut_ad(!ut_list_exists(m_LRU, file));

  return true;
}

/** If the tablespace is not on the unflushed list, add it.
@param[in,out]  space           Tablespace to add */
void Fil_shard::add_to_unflushed_list(fil_space_t *space) {
  ut_ad(mutex_owned());
  ut_a(space->purpose != FIL_TYPE_TEMPORARY);

  if (!space->is_in_unflushed_spaces) {
    space->is_in_unflushed_spaces = true;

    UT_LIST_ADD_FIRST(m_unflushed_spaces, space);
  }
}

/** Note that a write IO has completed.
@param[in,out]  file            File on which a write was completed */
void Fil_shard::write_completed(fil_node_t *file) {
  ut_ad(mutex_owned());

  ++m_modification_counter;

  file->modification_counter = m_modification_counter;

  if (fil_disable_space_flushing(file->space)) {
    /* We don't need to keep track of not flushed changes as either:
    - user has explicitly disabled buffering,
    - or it is FIL_TYPE_TEMPORARY space and we don't ever flush these. */
    ut_ad(!file->space->is_in_unflushed_spaces);

    file->set_flushed();

  } else {
    add_to_unflushed_list(file->space);
  }
}

/** Updates the data structures when an I/O operation finishes. Updates the
pending I/O's field in the file appropriately.
@param[in]      file            Tablespace file
@param[in]      type            Marks the file as modified type == WRITE */
void Fil_shard::complete_io(fil_node_t *file, const IORequest &type) {
  ut_ad(mutex_owned());

  ut_a(file->n_pending_ios > 0);

  --file->n_pending_ios;

  ut_ad(type.validate());

  if (type.is_write()) {
    ut_ad(!srv_read_only_mode || fsp_is_system_temporary(file->space->id));

    write_completed(file);
  }

  if (file->n_pending_ios == 0) {
    /* The file must be put back to the LRU list */
    add_to_lru_if_needed(file);
  }
}

/** Report information about an invalid page access.
@param[in]      block_offset    Block offset
@param[in]      space_id        Tablespace ID
@param[in]      space_name      Tablespace name
@param[in]      byte_offset     Byte offset
@param[in]      len             I/O length
@param[in]      is_read         I/O type
@param[in]      line            Line called from */
static void fil_report_invalid_page_access_low(page_no_t block_offset,
                                               space_id_t space_id,
                                               const char *space_name,
                                               ulint byte_offset, ulint len,
                                               bool is_read, int line) {
  ib::error(ER_IB_MSG_328)
      << "Trying to access page number " << block_offset
      << " in"
         " space "
      << space_id << ", space name " << space_name
      << ","
         " which is outside the tablespace bounds. Byte offset "
      << byte_offset << ", len " << len << ", i/o type "
      << (is_read ? "read" : "write")
      << ". If you get this error at mysqld startup, please check"
         " that your my.cnf matches the ibdata files that you have in"
         " the MySQL server.";

  ib::error(ER_IB_MSG_329) << "Server exits"
#ifdef UNIV_DEBUG
                           << " at "
                           << "fil0fil.cc"
                           << "[" << line << "]"
#endif /* UNIV_DEBUG */
                           << ".";

  ut_error;
}

#define fil_report_invalid_page_access(b, s, n, o, l, t) \
  fil_report_invalid_page_access_low((b), (s), (n), (o), (l), (t), __LINE__)

/** Set encryption information for IORequest.
@param[in,out]  req_type        IO request
@param[in]      page_id         page id
@param[in]      space           table space */
void fil_io_set_encryption(IORequest &req_type, const page_id_t &page_id,
                           fil_space_t *space) {
  ut_a(!req_type.is_log());
  /* Don't encrypt page 0 of all tablespaces except redo log
  tablespace, all pages from the system tablespace. */
  if ((space->encryption_op_in_progress == Encryption::Progress::DECRYPTION &&
       req_type.is_write()) ||
      !space->can_encrypt() || page_id.page_no() == 0) {
    req_type.clear_encrypted();
    return;
  }

  /* For writing undo log, if encryption for undo log is disabled,
  skip set encryption. */
  if (fsp_is_undo_tablespace(space->id) && !srv_undo_log_encrypt &&
      req_type.is_write()) {
    req_type.clear_encrypted();
    return;
  }

  if (req_type.get_encrypted_block() != nullptr) {
    /* Already encrypted. */
    req_type.clear_encrypted();
    return;
  }
  req_type.get_encryption_info().set(space->m_encryption_metadata);
  ut_ad(space->m_encryption_metadata.m_type == Encryption::AES);
}

AIO_mode Fil_shard::get_AIO_mode(const IORequest &, bool sync) {
#ifndef UNIV_HOTBACKUP
  if (sync) {
    return AIO_mode::SYNC;

  } else {
    return AIO_mode::NORMAL;
  }
#else  /* !UNIV_HOTBACKUP */
  ut_a(sync);
  return AIO_mode::SYNC;
#endif /* !UNIV_HOTBACKUP */
}

dberr_t Fil_shard::get_file_for_io(fil_space_t *space, page_no_t *page_no,
                                   fil_node_t *&file) {
  file = space->get_file_node(page_no);
  return (file == nullptr) ? DB_ERROR : DB_SUCCESS;
}

dberr_t Fil_shard::do_io(const IORequest &type, bool sync,
                         const page_id_t &page_id, const page_size_t &page_size,
                         ulint byte_offset, ulint len, void *buf,
                         void *message) {
  IORequest req_type(type);

  ut_ad(req_type.validate());
  ut_a(!req_type.is_log());

  ut_ad(len > 0);
  ut_ad(byte_offset < UNIV_PAGE_SIZE);
  ut_ad(!page_size.is_compressed() || byte_offset == 0);
  ut_ad(UNIV_PAGE_SIZE == (ulong)(1 << UNIV_PAGE_SIZE_SHIFT));

  ut_ad(fil_validate_skip());

#ifndef UNIV_HOTBACKUP
  /* ibuf bitmap pages must be read in the sync AIO mode: */
  ut_ad(recv_no_ibuf_operations || req_type.is_write() ||
        !ibuf_bitmap_page(page_id, page_size) || sync);

  auto aio_mode = get_AIO_mode(req_type, sync);

  if (req_type.is_read()) {
    ut_ad(type.get_original_size() == 0);
    srv_stats.data_read.add(len);

    if (aio_mode == AIO_mode::NORMAL && !recv_no_ibuf_operations &&
        ibuf_page(page_id, page_size, UT_LOCATION_HERE, nullptr)) {
      /* Reduce probability of deadlock bugs
      in connection with ibuf: do not let the
      ibuf I/O handler sleep */

      req_type.clear_do_not_wake();

      aio_mode = AIO_mode::IBUF;
    }

#ifdef UNIV_DEBUG
    mutex_acquire();
    /* Should never attempt to read from a deleted tablespace, unless we
    are also importing the tablespace. By the time we get here in the final
    phase of import the state has changed. Therefore we check if there is
    an active fil_space_t instance with the same ID. */
    for (auto pair : m_deleted_spaces) {
      if (pair.first == page_id.space()) {
        auto space = get_space_by_id(page_id.space());
        if (space != nullptr) {
          ut_a(pair.second != space);
        }
      }
    }
    mutex_release();
#endif /* UNIV_DEBUG && !UNIV_HOTBACKUP */

  } else if (req_type.is_write()) {
    ut_ad(!srv_read_only_mode || fsp_is_system_temporary(page_id.space()));

    srv_stats.data_written.add(len);
  }
#else  /* !UNIV_HOTBACKUP */
  ut_a(sync);
  auto aio_mode = AIO_mode::SYNC;
#endif /* !UNIV_HOTBACKUP */

  /* Reserve the mutex and make sure that we can open at
  least one file while holding it, if the file is not already open */

  auto bpage = static_cast<buf_page_t *>(message);

  mutex_acquire();
  auto space = get_space_by_id(page_id.space());

  /* If we are deleting a tablespace we don't allow async read
  operations on that. However, we do allow write operations and
  sync read operations. */
  if (space == nullptr ||
      (req_type.is_read() && !sync && space->stop_new_ops)) {
#ifndef UNIV_HOTBACKUP
    const auto is_page_stale = bpage != nullptr && bpage->is_stale();
#endif /* !UNIV_HOTBACKUP */

    mutex_release();

    if (space == nullptr) {
#ifndef UNIV_HOTBACKUP
      if (req_type.is_write() && is_page_stale) {
        ut_a(bpage->get_space()->id == page_id.space());
        return DB_PAGE_IS_STALE;
      }
#endif /* !UNIV_HOTBACKUP */

      if (!req_type.ignore_missing()) {
#ifndef UNIV_HOTBACKUP
        /* Don't have any record of this tablespace. print a warning. */
        if (!Fil_shard::is_deleted(page_id.space())) {
#endif /* !UNIV_HOTBACKUP */
          if (space == nullptr) {
            ib::error(ER_IB_MSG_330)
                << "Trying to do I/O on a tablespace"
                << " which does not exist. I/O type: "
                << (req_type.is_read() ? "read" : "write")
                << ", page: " << page_id << ", I/O length: " << len << " bytes";
          } else {
            ib::error(ER_IB_MSG_331)
                << "Trying to do async read on a tablespace which is being"
                << " deleted. Tablespace name: \"" << space->name << "\","
                << " page: " << page_id << ", read length: " << len << " bytes";
          }
#ifndef UNIV_HOTBACKUP
        }
#endif /* !UNIV_HOTBACKUP */
      }
    }

    return DB_TABLESPACE_DELETED;
  }

#ifndef UNIV_HOTBACKUP
  if (bpage != nullptr) {
    ut_a(bpage->get_space()->id == page_id.space());

    if (req_type.is_write() && bpage->is_stale()) {
      mutex_release();
      return DB_PAGE_IS_STALE;
    }
    ut_a(bpage->get_space() == space);
  }
#endif /* !UNIV_HOTBACKUP */

  fil_node_t *file;
  auto page_no = page_id.page_no();
  auto err = get_file_for_io(space, &page_no, file);

  if (file == nullptr) {
    ut_ad(err == DB_ERROR);

    if (req_type.ignore_missing()) {
      mutex_release();

      return DB_ERROR;
    }

#ifndef UNIV_HOTBACKUP
    if (req_type.is_write() && bpage != nullptr && bpage->is_stale()) {
      ut_a(bpage->get_space()->id == page_id.space());

      mutex_release();
      return DB_PAGE_IS_STALE;
    }
#endif /* !UNIV_HOTBACKUP */

    /* This is a hard error. */
    fil_report_invalid_page_access(page_id.page_no(), page_id.space(),
                                   space->name, byte_offset, len,
                                   req_type.is_read());
  }

  if (!prepare_file_for_io(file)) {
#ifndef UNIV_HOTBACKUP
    if (space->is_deleted()) {
      mutex_release();

      if (!sync) {
        ut_d(bpage->take_io_responsibility());
        buf_page_io_complete(bpage, false);
      }

      return DB_TABLESPACE_DELETED;
    }
#endif /* !UNIV_HOTBACKUP */

    if (fsp_is_ibd_tablespace(space->id)) {
      mutex_release();

      if (!req_type.ignore_missing()) {
        ib::error(ER_IB_MSG_332)
            << "Trying to do I/O to a tablespace"
               " which exists without an .ibd data"
            << " file. I/O type: " << (req_type.is_read() ? "read" : "write")
            << ", page: " << page_id_t(page_id.space(), page_no)
            << ", I/O length: " << len << " bytes";
      }

      return DB_TABLESPACE_DELETED;
    }

    /* Could not open a file to perform IO and this is not a IBD file,
    which could have become deleted meanwhile. This is a fatal error.
    Note: any log information should be emitted inside prepare_file_for_io()
    called few lines earlier. That's because the specific reason for this
    problem is known only inside there. */
    ut_error;
  }

  /* Check that at least the start offset is within the bounds of a
  single-table tablespace, including rollback tablespaces. */
  if (file->size <= page_no && space->id != TRX_SYS_SPACE) {
#ifndef UNIV_HOTBACKUP
    if (req_type.is_write() && bpage != nullptr && bpage->is_stale()) {
      ut_a(bpage->get_space()->id == page_id.space());
      return DB_PAGE_IS_STALE;
    }
#endif /* !UNIV_HOTBACKUP */

    if (req_type.ignore_missing()) {
      /* If we can tolerate the non-existent pages, we
      should return with DB_ERROR and let caller decide
      what to do. */

      complete_io(file, req_type);

      mutex_release();

      return DB_ERROR;
    }

    /* This is a hard error. */
    fil_report_invalid_page_access(page_id.page_no(), page_id.space(),
                                   space->name, byte_offset, len,
                                   req_type.is_read());
  }

  mutex_release();

  DEBUG_SYNC_C("innodb_fil_do_io_prepared_io_with_no_mutex");

  ut_a(page_size.is_compressed() ||
       page_size.physical() == page_size.logical());

  auto offset = (os_offset_t)page_no * page_size.physical();

  offset += byte_offset;

  ut_a(file->size - page_no >=
       (byte_offset +
        std::max(static_cast<uint32_t>(len), type.get_original_size()) +
        (page_size.physical() - 1)) /
           page_size.physical());

  ut_a(len % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(byte_offset % OS_FILE_LOG_BLOCK_SIZE == 0);

  /* Don't compress the log, page 0 of all tablespaces, tables compressed with
   the old compression scheme and all pages from the system tablespace. */
  if (req_type.is_write() && !page_size.is_compressed() &&
      page_id.page_no() > 0 && IORequest::is_punch_hole_supported() &&
      file->punch_hole) {
    req_type.set_punch_hole();

    req_type.compression_algorithm(space->compression_type);

  } else {
    req_type.clear_compressed();
  }

  /* Set encryption information. */
  fil_io_set_encryption(req_type, page_id, space);

  req_type.block_size(file->block_size);

#ifdef UNIV_HOTBACKUP
  /* In mysqlbackup do normal I/O, not AIO */
  if (req_type.is_read()) {
    err = os_file_read(req_type, file->name, file->handle, buf, offset, len);

  } else {
    ut_ad(!srv_read_only_mode || fsp_is_system_temporary(page_id.space()));

    err = os_file_write(req_type, file->name, file->handle, buf, offset, len);
  }
#else /* UNIV_HOTBACKUP */
  /* Queue the aio request */
  err = os_aio(
      req_type, aio_mode, file->name, file->handle, buf, offset, len,
      fsp_is_system_temporary(page_id.space()) ? false : srv_read_only_mode,
      file, message);

#endif /* UNIV_HOTBACKUP */

  if (err == DB_IO_NO_PUNCH_HOLE) {
    err = DB_SUCCESS;

    if (file->punch_hole) {
      ib::warn(ER_IB_MSG_333) << "Punch hole failed for '" << file->name << "'";
    }

    fil_no_punch_hole(file);
  }

  /* We an try to recover the page from the double write buffer if
  the decompression fails or the page is corrupt. */

  ut_a(req_type.is_dblwr() || err == DB_SUCCESS);

  if (sync) {
    /* The i/o operation is already completed when we return from
    os_aio: */

    mutex_acquire();

    complete_io(file, req_type);

    mutex_release();

    ut_ad(fil_validate_skip());
  }

  return err;
}

#ifndef UNIV_HOTBACKUP
/** Waits for an AIO operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.cc for more info). The thread specifies which
segment it wants to wait for.
@param[in]      segment         The number of the segment in the AIO array
                                to wait for */
void fil_aio_wait(ulint segment) {
  void *m2;
  fil_node_t *m1;
  IORequest type;

  ut_ad(fil_validate_skip());

  auto err = os_aio_handler(segment, &m1, &m2, &type);
  ut_a(err == DB_SUCCESS);

  auto file = reinterpret_cast<fil_node_t *>(m1);

  if (file == nullptr) {
    ut_ad(srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS);
    return;
  }

  ut_a(!type.is_dblwr());

  srv_set_io_thread_op_info(segment, "complete io for file");

  auto shard = fil_system->shard_by_id(file->space->id);

  shard->mutex_acquire();

  shard->complete_io(file, type);

  shard->mutex_release();

  ut_ad(fil_validate_skip());

  /* Do the i/o handling */
  /* IMPORTANT: since i/o handling for reads will read also the insert
  buffer in tablespace 0, you have to be very careful not to introduce
  deadlocks in the i/o system. We keep tablespace 0 data files always
  open, and use a special i/o thread to serve insert buffer requests. */

  switch (file->space->purpose) {
    case FIL_TYPE_IMPORT:
    case FIL_TYPE_TEMPORARY:
    case FIL_TYPE_TABLESPACE:
      srv_set_io_thread_op_info(segment, "complete io for buf page");

      /* async single page writes from the dblwr buffer don't have
      access to the page */
      if (m2 != nullptr) {
        auto bpage = static_cast<buf_page_t *>(m2);
        ut_d(bpage->take_io_responsibility());
        buf_page_io_complete(bpage, false);
      }
      return;
  }

  ut_d(ut_error);
}
#endif /* !UNIV_HOTBACKUP */

dberr_t fil_io(const IORequest &type, bool sync, const page_id_t &page_id,
               const page_size_t &page_size, ulint byte_offset, ulint len,
               void *buf, void *message) {
  auto shard = fil_system->shard_by_id(page_id.space());
#ifdef UNIV_DEBUG
  if (!sync) {
    /* In case of async io we transfer the io responsibility to the thread which
    will perform the io completion routine. */
    static_cast<buf_page_t *>(message)->release_io_responsibility();
  }
#endif

  auto const err = shard->do_io(type, sync, page_id, page_size, byte_offset,
                                len, buf, message);
#ifdef UNIV_DEBUG
  /* If the error prevented async io, then we haven't actually transferred the
  io responsibility at all, so we revert the debug io responsibility info. */
  auto bpage = static_cast<buf_page_t *>(message);

  /* When space is deleted, we could have marked the io complete. */
  if (err != DB_SUCCESS && !sync && bpage->was_io_fixed()) {
    bpage->take_io_responsibility();
  }
#endif
  return err;
}

/** If the tablespace is on the unflushed list and there are no pending
flushes then remove from the unflushed list.
@param[in,out]  space           Tablespace to remove */
void Fil_shard::remove_from_unflushed_list(fil_space_t *space) {
  ut_ad(mutex_owned());

  if (space->is_in_unflushed_spaces && space_is_flushed(space)) {
    space->is_in_unflushed_spaces = false;

    UT_LIST_REMOVE(m_unflushed_spaces, space);
  }
}

void Fil_shard::space_flush(space_id_t space_id) {
  ut_ad(mutex_owned());

  fil_space_t *space = get_space_by_id(space_id);

  if (space == nullptr || space->purpose == FIL_TYPE_TEMPORARY ||
      space->stop_new_ops) {
    return;
  }

  const bool disable_flush = fil_disable_space_flushing(space);

  if (disable_flush) {
    /* No need to flush. User has explicitly disabled
    buffering. However, flush should be called if the file
    size changes to keep OЅ metadata in sync. */
    ut_ad(!space->is_in_unflushed_spaces);
    ut_ad(space_is_flushed(space));

    /* Flush only if the file size changes */
    bool no_flush = true;
    for (const auto &file : space->files) {
#ifdef UNIV_DEBUG
      ut_ad(file.is_flushed());
#endif /* UNIV_DEBUG */
      if (file.flush_size != file.size) {
        /* Found at least one file whose size has changed */
        no_flush = false;
        break;
      }
    }

    if (no_flush) {
      /* Nothing to flush. Just return */
      return;
    }
  }

  /* Prevent dropping of the space while we are flushing */
  ++space->n_pending_flushes;

  for (auto &file : space->files) {
    int64_t old_mod_counter = file.modification_counter;

    if (!file.is_open) {
      continue;
    }

    /* Skip flushing if the file size has not changed since
    last flush was done and the flush mode is O_DIRECT_NO_FSYNC */
    if (disable_flush && (file.flush_size == file.size)) {
      ut_ad(old_mod_counter <= file.flush_counter);
      continue;
    }

    /* If we are here and the flush mode is O_DIRECT_NO_FSYNC, then
    it means that the file size has changed and hence, it should be
    flushed, irrespective of the mod_counter and flush counter values,
    which are always same in case of O_DIRECT_NO_FSYNC to avoid flush
    on every write operation.
    For other flush modes, if the flush_counter is same or ahead of
    the mod_counter, skip the flush. */
    if (!disable_flush && (old_mod_counter <= file.flush_counter)) {
      continue;
    }

    switch (space->purpose) {
      case FIL_TYPE_TEMPORARY:
        ut_error;  // we already checked for this

      case FIL_TYPE_TABLESPACE:
      case FIL_TYPE_IMPORT:
        fil_n_pending_tablespace_flushes.fetch_add(1);
        break;
    }

    bool skip_flush = is_fast_shutdown();
#ifdef _WIN32
    if (file.is_raw_disk) {
      skip_flush |= true;
    }
#endif /* _WIN32 */

    while (file.n_pending_flushes > 0 && !skip_flush) {
      /* We want to avoid calling os_file_flush() on
      the file twice at the same time, because we do
      not know what bugs OS's may contain in file
      I/O */

      int64_t sig_count = os_event_reset(file.sync_event);

      mutex_release();

      os_event_wait_low(file.sync_event, sig_count);

      mutex_acquire();

      if (file.flush_counter >= old_mod_counter) {
        skip_flush |= true;
      }
      skip_flush |= is_fast_shutdown();
    }

    if (!skip_flush) {
      ut_a(file.is_open);
      ++file.n_pending_flushes;

      mutex_release();

      os_file_flush(file.handle);

      file.flush_size = file.size;

      mutex_acquire();

      os_event_set(file.sync_event);

      --file.n_pending_flushes;
    }

    if (file.flush_counter < old_mod_counter) {
      file.flush_counter = old_mod_counter;

      remove_from_unflushed_list(space);
    }

    switch (space->purpose) {
      case FIL_TYPE_TEMPORARY:
        ut_error;  // we already checked for this

      case FIL_TYPE_TABLESPACE:
      case FIL_TYPE_IMPORT:
        fil_n_pending_tablespace_flushes.fetch_sub(1);
        continue;
    }

    ut_d(ut_error);
  }

  --space->n_pending_flushes;
}

void fil_flush(space_id_t space_id) {
  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  /* Note: Will release and reacquire the Fil_shard::mutex. */
  shard->space_flush(space_id);

  shard->mutex_release();
}

void Fil_shard::flush_file_spaces() {
  Space_ids space_ids;

  mutex_acquire();

  for (auto space : m_unflushed_spaces) {
    if ((to_int(space->purpose) & FIL_TYPE_TABLESPACE) &&
        !space->stop_new_ops) {
      space_ids.push_back(space->id);
    }
  }

  mutex_release();

  /* Flush the spaces.  It will not hurt to call fil_flush() on
  a non-existing space id. */
  for (auto space_id : space_ids) {
    mutex_acquire();

    space_flush(space_id);

    mutex_release();
  }
}

void Fil_system::flush_file_spaces() {
  for (auto shard : m_shards) {
    shard->flush_file_spaces();
  }
}

void fil_flush_file_spaces() { fil_system->flush_file_spaces(); }

/** Returns true if file address is undefined.
@param[in]      addr            File address to check
@return true if undefined */
bool fil_addr_is_null(const fil_addr_t &addr) {
  return (addr.page == FIL_NULL);
}

/** Get the predecessor of a file page.
@param[in]      page            File page
@return FIL_PAGE_PREV */
page_no_t fil_page_get_prev(const byte *page) {
  return mach_read_from_4(page + FIL_PAGE_PREV);
}

/** Get the successor of a file page.
@param[in]      page            File page
@return FIL_PAGE_NEXT */
page_no_t fil_page_get_next(const byte *page) {
  return mach_read_from_4(page + FIL_PAGE_NEXT);
}

/** Sets the file page type.
@param[in,out]  page            File page
@param[in]      type            File page type to set */
void fil_page_set_type(byte *page, ulint type) {
  mach_write_to_2(page + FIL_PAGE_TYPE, type);
}

/** Reset the page type.
Data files created before MySQL 5.1 may contain garbage in FIL_PAGE_TYPE.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in]      page_id Page number
@param[in,out]  page    Page with invalid FIL_PAGE_TYPE
@param[in]      type    Expected page type
@param[in,out]  mtr     Mini-transaction */
void fil_page_reset_type(const page_id_t &page_id, byte *page, ulint type,
                         mtr_t *mtr) {
  ib::info(ER_IB_MSG_334) << "Resetting invalid page " << page_id << " type "
                          << fil_page_get_type(page) << " to " << type << ".";
  mlog_write_ulint(page + FIL_PAGE_TYPE, type, MLOG_2BYTES, mtr);
}

/** Closes the tablespace memory cache. */
void fil_close() {
  if (fil_system == nullptr) {
    return;
  }

  ut::delete_(fil_system);

  fil_system = nullptr;
}

#ifndef UNIV_HOTBACKUP
/** Initializes the buffer control block used by fil_tablespace_iterate.
@param[in]      block           Pointer to the control block
@param[in]      frame           Pointer to buffer frame */
static void fil_buf_block_init(buf_block_t *block, byte *frame) {
  UNIV_MEM_DESC(frame, UNIV_PAGE_SIZE);

  block->frame = frame;

  block->page.init_io_fix();
  /* There are assertions that check for this. */
  block->page.buf_fix_count.store(1);
  block->page.state = BUF_BLOCK_READY_FOR_USE;

  page_zip_des_init(&block->page.zip);
}

struct Fil_page_iterator {
  /** File handle */
  pfs_os_file_t m_file;

  /** File path name */
  const char *m_filepath;

  /** From where to start */
  os_offset_t m_start;

  /** Where to stop */
  os_offset_t m_end;

  /* File size in bytes */
  os_offset_t m_file_size;

  /** Page size */
  size_t m_page_size;

  /** Number of pages to use for I/O */
  size_t m_n_io_buffers;

  /** Buffer to use for IO */
  byte *m_io_buffer;

  /** Encryption metadata */
  const Encryption_metadata &m_encryption_metadata;

  /** FS Block Size */
  size_t block_size;

  /** Compression algorithm to be used if the table needs to be compressed. */
  Compression::Type m_compression_type{};
};

/** TODO: This can be made parallel trivially by chunking up the file
and creating a callback per thread. Main benefit will be to use multiple
CPUs for checksums and compressed tables. We have to do compressed tables
block by block right now. Secondly we need to decompress/compress and copy
too much of data. These are CPU intensive.

Iterate over all the pages in the tablespace.
@param[in]      iter            Tablespace iterator
@param[in,out]  block           Block to use for IO
@param[in]      callback        Callback to inspect and update page contents
@retval DB_SUCCESS or error code */
static dberr_t fil_iterate(const Fil_page_iterator &iter, buf_block_t *block,
                           PageCallback &callback) {
  os_offset_t offset;
  size_t n_bytes;
  page_no_t page_no = 0;
  space_id_t space_id = callback.get_space_id();

  n_bytes = iter.m_n_io_buffers * iter.m_page_size;

  ut_ad(!srv_read_only_mode);

  /* For old style compressed tables we do a lot of useless copying
  for non-index pages. Unfortunately, it is required by
  buf_zip_decompress() */

  ulint read_type = IORequest::READ;
  ulint write_type = IORequest::WRITE;

  for (offset = iter.m_start; offset < iter.m_end; offset += n_bytes) {
    byte *io_buffer = iter.m_io_buffer;

    block->frame = io_buffer;

    if (callback.get_page_size().is_compressed()) {
      page_zip_des_init(&block->page.zip);
      page_zip_set_size(&block->page.zip, iter.m_page_size);

      block->page.size.copy_from(
          page_size_t(static_cast<uint32_t>(iter.m_page_size),
                      static_cast<uint32_t>(univ_page_size.logical()), true));

      block->page.zip.data = block->frame + UNIV_PAGE_SIZE;
      ut_d(block->page.zip.m_external = true);
      ut_ad(iter.m_page_size == callback.get_page_size().physical());

      /* Zip IO is done in the compressed page buffer. */
      io_buffer = block->page.zip.data;
    } else {
      io_buffer = iter.m_io_buffer;
    }

    /* We have to read the exact number of bytes. Otherwise the
    InnoDB IO functions croak on failed reads. */

    n_bytes = static_cast<ulint>(
        std::min(static_cast<os_offset_t>(n_bytes), iter.m_end - offset));

    ut_ad(n_bytes > 0);
    ut_ad(!(n_bytes % iter.m_page_size));

    dberr_t err;
    IORequest read_request(read_type);
    read_request.block_size(iter.block_size);

    /* For encrypted table, set encryption information. */
    if (iter.m_encryption_metadata.can_encrypt() && offset != 0) {
      read_request.get_encryption_info().set(iter.m_encryption_metadata);
    }

    err = os_file_read(read_request, iter.m_filepath, iter.m_file, io_buffer,
                       offset, (ulint)n_bytes);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_335) << "os_file_read() failed";

      return err;
    }

    size_t n_pages_read;
    bool updated = false;
    os_offset_t page_off = offset;

    n_pages_read = (ulint)n_bytes / iter.m_page_size;

    for (size_t i = 0; i < n_pages_read; ++i) {
      buf_block_set_file_page(block, page_id_t(space_id, page_no++));

      /* We are going to modify the page. Add to page tracking system. */
      arch_page_sys->track_page(&block->page, LSN_MAX, LSN_MAX, true);

      if ((err = callback(page_off, block)) != DB_SUCCESS) {
        return err;

      } else if (!updated) {
        updated = buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE;
      }

      buf_block_set_state(block, BUF_BLOCK_NOT_USED);
      buf_block_set_state(block, BUF_BLOCK_READY_FOR_USE);

      page_off += iter.m_page_size;
      block->frame += iter.m_page_size;
    }

    IORequest write_request(write_type);
    write_request.block_size(iter.block_size);

    /* For encrypted table, set encryption information. */
    if (iter.m_encryption_metadata.can_encrypt() && offset != 0) {
      write_request.get_encryption_info().set(iter.m_encryption_metadata);
    }

    /* For compressed table, set compressed information.
    @note os_file_compress_page() function expects that the page size is a
    multiple of OS punch hole size so we make sure it's true before turning
    on compression. */
    if (iter.m_compression_type != Compression::Type::NONE &&
        IORequest::is_punch_hole_supported() &&
        !(srv_page_size % iter.block_size)) {
      write_request.compression_algorithm(iter.m_compression_type);

      /* In the case of import since we're doing compression for the first time
      we would like to ignore any optimisations to not do punch hole. So force
      the punch hole. */
      write_request.disable_punch_hole_optimisation();
    }

    /* A page was updated in the set, write back to disk. */

    if (updated && (err = os_file_write(write_request, iter.m_filepath,
                                        iter.m_file, io_buffer, offset,
                                        (ulint)n_bytes)) != DB_SUCCESS) {
      /* This is not a hard error */
      if (err == DB_IO_NO_PUNCH_HOLE) {
        err = DB_SUCCESS;
        write_type &= ~IORequest::PUNCH_HOLE;

      } else {
        ib::error(ER_IB_MSG_336) << "os_file_write() failed";

        return err;
      }
    }
  }

  return DB_SUCCESS;
}

void fil_adjust_name_import(dict_table_t *table [[maybe_unused]],
                            const char *path,
                            ib_file_suffix extn [[maybe_unused]]) {
  /* Try to open with current name first. */
  if (os_file_exists(path)) {
    return;
  }

  /* On failure we need to check if file exists in different letter case
  for partitioned table. */

  /* Safe check. Never needed on Windows. */
#ifndef _WIN32
  /* Needed only for case sensitive file system. */
  if (lower_case_file_system) {
    return;
  }

  /* Only needed for partition file. */
  if (!dict_name::is_partition(table->name.m_name)) {
    return;
  }

  /* Get Import directory path. */
  std::string import_dir(path);
  Fil_path::normalize(import_dir);

  auto pos = import_dir.find_last_of(Fil_path::SEPARATOR);
  if (pos == std::string::npos) {
    import_dir.assign(Fil_path::DOT_SLASH);

  } else {
    import_dir.resize(pos + 1);
    ut_ad(Fil_path::is_separator(import_dir.back()));
  }

  /* Walk through all files under the directory and match the import file
  after adjusting case. This is a safe check to allow files exported from
  earlier versions where the case for partition name and separator could
  be different. */
  bool found_path = false;
  std::string saved_path;

  Dir_Walker::walk(import_dir, false, [&](const std::string &file_path) {
    /* Skip entry if already found. */
    if (found_path) {
      return;
    }
    /* Check only for partition files. */
    if (!dict_name::is_partition(file_path)) {
      return;
    }

    /* Extract table name from path. */
    std::string table_name;
    if (!Fil_path::parse_file_path(file_path, extn, table_name)) {
      /* Not a valid file-per-table path */
      return;
    }

    /* Check if the file name would match after correcting the case. */
    dict_name::rebuild(table_name);
    if (table_name.compare(table->name.m_name) != 0) {
      return;
    }

    saved_path.assign(file_path);
    found_path = true;
  });

  /* Check and rename the import file name. */
  if (found_path) {
    fil_rename_partition_file(saved_path, extn, false, true);
  }
#endif /* !WIN32 */

  return;
}

dberr_t fil_tablespace_iterate(const Encryption_metadata &encryption_metadata,
                               dict_table_t *table, ulint n_io_buffers,
                               Compression::Type compression_type,
                               PageCallback &callback) {
  dberr_t err;
  pfs_os_file_t file;
  char *filepath;
  bool success;

  ut_a(n_io_buffers > 0);
  ut_ad(!srv_read_only_mode);

  DBUG_EXECUTE_IF("ib_import_trigger_corruption_1", return DB_CORRUPTION;);

  /* Make sure the data_dir_path is set. */
  dd_get_and_save_data_dir_path<dd::Table>(table, nullptr, false);

  std::string path = dict_table_get_datadir(table);

  filepath = Fil_path::make(path, table->name.m_name, IBD, true);

  if (filepath == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  /* Adjust filename for partition file if in different letter case. */
  fil_adjust_name_import(table, filepath, IBD);

  file = os_file_create_simple_no_error_handling(
      innodb_data_file_key, filepath, OS_FILE_OPEN, OS_FILE_READ_WRITE,
      srv_read_only_mode, &success);

  DBUG_EXECUTE_IF("fil_tablespace_iterate_failure", {
    static bool once;

    if (!once || ut::random_from_interval(0, 10) == 5) {
      once = true;
      success = false;
      os_file_close(file);
    }
  });

  if (!success) {
    /* The following call prints an error message */
    os_file_get_last_error(true);

    ib::error(ER_IB_MSG_337) << "Trying to import a tablespace, but could not"
                                " open the tablespace file "
                             << filepath;

    ut::free(filepath);

    return DB_TABLESPACE_NOT_FOUND;

  } else {
    err = DB_SUCCESS;
  }

  /* Set File System Block Size */
  size_t block_size;
  {
    os_file_stat_t stat_info;

    ut_d(dberr_t err =) os_file_get_status(filepath, &stat_info, false, false);
    ut_ad(err == DB_SUCCESS);

    block_size = stat_info.block_size;
  }

  callback.set_file(filepath, file);

  os_offset_t file_size = os_file_get_size(file);
  ut_a(file_size != (os_offset_t)-1);

  /* The block we will use for every physical page */
  buf_block_t *block;

  block = reinterpret_cast<buf_block_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*block)));

  mutex_create(LATCH_ID_BUF_BLOCK_MUTEX, &block->mutex);

  /* Allocate a page to read in the tablespace header, so that we
  can determine the page size and zip size (if it is compressed).
  We allocate an extra page in case it is a compressed table. One
  page is to ensure alignment. */

  byte *page = static_cast<byte *>(
      ut::aligned_alloc(2 * UNIV_PAGE_SIZE, UNIV_PAGE_SIZE));

  fil_buf_block_init(block, page);

  /* Read the first page and determine the page and zip size. */

  IORequest request(IORequest::READ);

  err = os_file_read_first_page(request, path.c_str(), file, page,
                                UNIV_PAGE_SIZE);

  if (err != DB_SUCCESS) {
    err = DB_IO_ERROR;

  } else if ((err = callback.init(file_size, block)) == DB_SUCCESS) {
    Fil_page_iterator iter{
        /* .m_file = */ file,
        /* .m_filepath = */ filepath,
        /* .m_start = */ 0,
        /* .m_end = */ file_size,
        /* .m_file_size = */ file_size,
        /* .m_page_size = */ callback.get_page_size().physical(),
        /* .m_n_io_buffers = */ n_io_buffers,
        /* .m_io_buffer = */ nullptr,
        /* .m_encryption_metadata = */ encryption_metadata,
        /* .block_size = */ block_size,
        /* .m_compression_type = */ compression_type,
    };
    /* Check encryption is matched or not. */
    ulint space_flags = callback.get_space_flags();

    if (FSP_FLAGS_GET_ENCRYPTION(space_flags)) {
      if (!dd_is_table_in_encrypted_tablespace(table)) {
        ib::error(ER_IB_MSG_338) << "Table is not in an encrypted tablespace,"
                                    " but the data file intended for import"
                                    " is an encrypted tablespace";

        err = DB_IO_NO_ENCRYPT_TABLESPACE;
      } else {
        /* encryption_key must have been populated while reading CFP file. */
        ut_ad(encryption_metadata.can_encrypt());

        if (!encryption_metadata.can_encrypt()) {
          err = DB_ERROR;
        }
      }
    }

    if (err == DB_SUCCESS) {
      /* Compressed pages can't be optimised for block IO
      for now.  We do the IMPORT page by page. */

      if (callback.get_page_size().is_compressed()) {
        iter.m_n_io_buffers = 1;
        ut_a(iter.m_page_size == callback.get_page_size().physical());
      }

      /** Add an extra page for compressed page scratch
      area. */
      iter.m_io_buffer = static_cast<byte *>(ut::aligned_alloc(
          (1 + iter.m_n_io_buffers) * UNIV_PAGE_SIZE, UNIV_PAGE_SIZE));

      err = fil_iterate(iter, block, callback);

      ut::aligned_free(iter.m_io_buffer);
    }
  }

  if (err == DB_SUCCESS) {
    ib::info(ER_IB_MSG_339) << "Sync to disk";

    if (!os_file_flush(file)) {
      ib::info(ER_IB_MSG_340) << "os_file_flush() failed!";
      err = DB_IO_ERROR;
    } else {
      ib::info(ER_IB_MSG_341) << "Sync to disk - done!";
    }
  }

  os_file_close(file);

  ut::aligned_free(page);
  ut::free(filepath);

  mutex_free(&block->mutex);

  ut::free(block);

  return err;
}
#endif /* !UNIV_HOTBACKUP */

/** Set the tablespace table size.
@param[in]      page    a page belonging to the tablespace */
void PageCallback::set_page_size(const buf_frame_t *page) UNIV_NOTHROW {
  m_page_size.copy_from(fsp_header_get_page_size(page));
}

/** Delete the tablespace file and any related files like .cfg.
This should not be called for temporary tables.
@param[in]      path            File path of the IBD tablespace
@return true on success */
bool fil_delete_file(const char *path) {
  bool success = true;

  /* Force a delete of any stale .ibd files that are lying around. */
  success = os_file_delete_if_exists(innodb_data_file_key, path, nullptr);

  char *cfg_filepath = Fil_path::make_cfg(path);

  if (cfg_filepath != nullptr) {
    os_file_delete_if_exists(innodb_data_file_key, cfg_filepath, nullptr);

    ut::free(cfg_filepath);
  }

  char *cfp_filepath = Fil_path::make_cfp(path);

  if (cfp_filepath != nullptr) {
    os_file_delete_if_exists(innodb_data_file_key, cfp_filepath, nullptr);

    ut::free(cfp_filepath);
  }

  return success;
}

#ifndef UNIV_HOTBACKUP
dberr_t fil_rename_precheck(const dict_table_t *old_table,
                            const dict_table_t *new_table,
                            const char *tmp_name) {
  bool old_is_file_per_table = dict_table_is_file_per_table(old_table);
  bool new_is_file_per_table = dict_table_is_file_per_table(new_table);

  /* If neither table is file-per-table, there will be no renaming of files. */
  if (!old_is_file_per_table && !new_is_file_per_table) {
    return DB_SUCCESS;
  }

  auto fetch_path = [](std::string &path, const dict_table_t *source_table,
                       bool fpt) -> dberr_t {
    char *path_ptr{};

    /* It is possible that the file could be present in a directory outside of
    the data directory (possible because of innodb_directories option), so
    fetch the path accordingly.

    We are only interested in fetching the right path for file-per-table
    tablespaces as during file_rename_tablespace_check the source table should
    always exist and we do the rename for only source tables which are
    file-per-table tablspaces. */
    if (fpt && !dict_table_is_discarded(source_table)) {
      path_ptr = fil_space_get_first_path(source_table->space);

      if (path_ptr == nullptr) {
        return DB_TABLESPACE_NOT_FOUND;
      }
    } else {
      auto dir = dict_table_get_datadir(source_table);

      path_ptr =
          Fil_path::make(dir, source_table->name.m_name, IBD, !dir.empty());

      if (path_ptr == nullptr) {
        return DB_OUT_OF_MEMORY;
      }
    }

    path.assign(path_ptr);
    ut::free(path_ptr);

    return DB_SUCCESS;
  };

  std::string old_path;

  auto err = fetch_path(old_path, old_table, old_is_file_per_table);

  if (err != DB_SUCCESS) {
    return err;
  }

  if (old_is_file_per_table) {
    std::string tmp_path =
        Fil_path::make_new_path(old_path.c_str(), tmp_name, IBD);

    /* Temp filepath must not exist. */
    err = fil_rename_tablespace_check(old_table->space, old_path.c_str(),
                                      tmp_path.c_str(),
                                      dict_table_is_discarded(old_table));

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  if (new_is_file_per_table) {
    std::string new_path;

    err = fetch_path(new_path, new_table, new_is_file_per_table);

    if (err != DB_SUCCESS) {
      return err;
    }

    /* Destination filepath must not exist unless this ALTER TABLE starts and
    ends with a file_per-table tablespace. */
    if (!old_is_file_per_table) {
      err = fil_rename_tablespace_check(new_table->space, new_path.c_str(),
                                        old_path.c_str(),
                                        dict_table_is_discarded(new_table));
    }
  }

  return err;
}
#endif /* !UNIV_HOTBACKUP */

/** Note that the file system where the file resides doesn't support PUNCH HOLE.
Called from AIO handlers when IO returns DB_IO_NO_PUNCH_HOLE
@param[in,out]  file            file to set */
void fil_no_punch_hole(fil_node_t *file) { file->punch_hole = false; }

dberr_t fil_set_compression(space_id_t space_id, const char *algorithm) {
  dberr_t err;
  Compression compression;

  if (algorithm == nullptr || strlen(algorithm) == 0) {
#ifndef UNIV_DEBUG
    compression.m_type = Compression::NONE;
#else /* UNIV_DEBUG */
    /* This is a Debug tool for setting compression on all
    compressible tables not otherwise specified. */
    switch (srv_debug_compress) {
      case Compression::LZ4:
      case Compression::ZLIB:
      case Compression::NONE:

        compression.m_type = static_cast<Compression::Type>(srv_debug_compress);
        break;

      default:
        compression.m_type = Compression::NONE;
    }

#endif /* UNIV_DEBUG */

    err = DB_SUCCESS;

  } else {
    err = Compression::check(algorithm, &compression);
  }

  fil_space_t *space = fil_space_get(space_id);

  if (space == nullptr) {
    return DB_NOT_FOUND;
  }

  const page_size_t page_size(space->flags);

  if (!fsp_is_file_per_table(space_id, space->flags) ||
      fsp_is_system_temporary(space_id) || page_size.is_compressed()) {
    return DB_IO_NO_PUNCH_HOLE_TABLESPACE;
  }

  space->compression_type = compression.m_type;

  if (space->compression_type != Compression::NONE) {
    if (!space->files.front().punch_hole) {
      return DB_IO_NO_PUNCH_HOLE_FS;
    }
  }

  return err;
}

/** Get the compression algorithm for a tablespace.
@param[in]      space_id        Space ID to check
@return the compression algorithm */
Compression::Type fil_get_compression(space_id_t space_id) {
  fil_space_t *space = fil_space_get(space_id);

  return space == nullptr ? Compression::NONE : space->compression_type;
}

/** Set the autoextend_size attribute for the tablespace
@param[in] space_id             Space ID of tablespace for which to set
@param[in] autoextend_size      Value of autoextend_size attribute
@return DB_SUCCESS or error code */
dberr_t fil_set_autoextend_size(space_id_t space_id, uint64_t autoextend_size) {
  ut_ad(space_id != TRX_SYS_SPACE);

  fil_space_t *space = fil_space_acquire(space_id);

  if (space == nullptr) {
    return DB_NOT_FOUND;
  }

  rw_lock_x_lock(&space->latch, UT_LOCATION_HERE);

  space->autoextend_size_in_bytes = autoextend_size;

  rw_lock_x_unlock(&space->latch);

  fil_space_release(space);

  return DB_SUCCESS;
}

/** Set the encryption type for the tablespace
@param[in] space_id             Space ID of tablespace for which to set
@param[in] algorithm            Encryption algorithm
@param[in] key                  Encryption key
@param[in] iv                   Encryption iv
@return DB_SUCCESS or error code */
dberr_t fil_set_encryption(space_id_t space_id, Encryption::Type algorithm,
                           byte *key, byte *iv) {
  ut_ad(space_id != TRX_SYS_SPACE);

  if (fsp_is_system_or_temp_tablespace(space_id)) {
    return DB_IO_NO_ENCRYPT_TABLESPACE;
  }

  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  if (space == nullptr) {
    shard->mutex_release();
    return DB_NOT_FOUND;
  }

  Encryption::set_or_generate(algorithm, key, iv, space->m_encryption_metadata);

  shard->mutex_release();

  return DB_SUCCESS;
}

/** Reset the encryption type for the tablespace
@param[in] space_id             Space ID of tablespace for which to set
@return DB_SUCCESS or error code */
dberr_t fil_reset_encryption(space_id_t space_id) {
  ut_ad(space_id != TRX_SYS_SPACE);

  if (fsp_is_system_or_temp_tablespace(space_id)) {
    return DB_IO_NO_ENCRYPT_TABLESPACE;
  }

  auto shard = fil_system->shard_by_id(space_id);

  shard->mutex_acquire();

  fil_space_t *space = shard->get_space_by_id(space_id);

  if (space == nullptr) {
    shard->mutex_release();
    return DB_NOT_FOUND;
  }

  space->m_encryption_metadata = {};

  shard->mutex_release();

  return DB_SUCCESS;
}

#ifndef UNIV_HOTBACKUP
bool Fil_shard::needs_encryption_rotate(fil_space_t *space) {
  /* We only rotate if encryption is already set. */
  if (!space->can_encrypt()) {
    return false;
  }

  /* Deleted spaces do not need rotation.  Their pages are being
  deleted from the buffer pool. */
  if (space->is_deleted()) {
    return false;
  }

  /* Skip unencypted tablespaces. */
  if (fsp_is_system_or_temp_tablespace(space->id)) {
    return false;
  }

  DBUG_EXECUTE_IF(
      "ib_encryption_rotate_skip",
      ib::info(ER_IB_MSG_INJECT_FAILURE, "ib_encryption_rotate_skip");
      return false;);

  return true;
}

size_t Fil_shard::encryption_rotate(size_t *rotate_count) {
  /* If there are no tablespaces to rotate, return true. */
  size_t fail_count = 0;
  byte encrypt_info[Encryption::INFO_SIZE];
  using Spaces_to_rotate = std::vector<fil_space_t *>;
  Spaces_to_rotate spaces2rotate;

  /* Use the shard mutex to collect a list of the spaces to rotate. */
  mutex_acquire();

  for (auto &elem : m_spaces) {
    auto space = elem.second;

    if (!needs_encryption_rotate(space)) {
      continue;
    }

    spaces2rotate.push_back(space);
  }

  mutex_release();

  /* We can now be assured that each fil_space_t collected above will not be
  deleted below (outside the shard mutex protection) because:
  1. The caller, Rotate_innodb_master_key::execute(), holds an exclusive
     backup lock which blocks any other concurrent DDL or MDL on this space.
  2. Only a thread with an MDL on the space name can mark a fil_space_t as
     deleted or actually delete it. This includes background threads like
     the purge thread doing undo truncation as well as any client DDL.
  3. We assured above using the shard mutex that the space is not deleted. */

  for (auto &space : spaces2rotate) {
    /* Rotate this encrypted tablespace. */
    mtr_t mtr;
    mtr_start(&mtr);
    memset(encrypt_info, 0, Encryption::INFO_SIZE);
    bool rotate_ok = fsp_header_rotate_encryption(space, encrypt_info, &mtr);
    ut_ad(rotate_ok);
    mtr_commit(&mtr);

    if (rotate_ok) {
      ++(*rotate_count);
    } else {
      ++fail_count;
    }
  }

  /* This crash forces encryption rotate to complete at startup. */
  DBUG_EXECUTE_IF(
      "ib_encryption_rotate_crash",
      ib::info(ER_IB_MSG_INJECT_FAILURE, "ib_encryption_rotate_crash");
      DBUG_SUICIDE(););

  return fail_count;
}

size_t Fil_system::encryption_rotate() {
  size_t fail_count = 0;
  size_t rotate_count = 0;

  for (auto shard : m_shards) {
    fail_count += shard->encryption_rotate(&rotate_count);
  }

  if (rotate_count > 0) {
    ib::info(ER_IB_MSG_MASTER_KEY_ROTATED, static_cast<int>(rotate_count));
  }

  return fail_count;
}

void Fil_system::encryption_reencrypt(
    std::vector<space_id_t> &space_id_vector) {
  /* If there are no tablespaces to reencrypt, return true. */
  if (space_id_vector.empty()) {
    return;
  }

  size_t fail_count = 0;
  byte encrypt_info[Encryption::INFO_SIZE];

  /* This operation is done either post recovery or when the first time
  tablespace is loaded. */

  for (auto &space_id : space_id_vector) {
    fil_space_t *space = fil_space_get(space_id);
    ut_ad(space != nullptr);
    ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

    /* Rotate this encrypted tablespace. */
    mtr_t mtr;
    mtr_start(&mtr);
    memset(encrypt_info, 0, Encryption::INFO_SIZE);
    bool rotate_ok = fsp_header_rotate_encryption(space, encrypt_info, &mtr);
    ut_ad(rotate_ok);
    mtr_commit(&mtr);

    if (rotate_ok) {
      if (fsp_is_ibd_tablespace(space_id)) {
        if (fsp_is_file_per_table(space_id, space->flags)) {
          ib::info(ER_IB_MSG_REENCRYPTED_TABLESPACE_KEY, space->name);
        } else {
          ib::info(ER_IB_MSG_REENCRYPTED_GENERAL_TABLESPACE_KEY, space->name);
        }
      }
    } else {
      ++fail_count;
    }
  }

  /* The operation should finish successfully for all tablespaces */
  ut_a(fail_count == 0);
}

size_t fil_encryption_rotate() { return (fil_system->encryption_rotate()); }

void fil_encryption_reencrypt(std::vector<space_id_t> &sid_vector) {
  fil_system->encryption_reencrypt(sid_vector);
}
#endif /* !UNIV_HOTBACKUP */

/** Constructor
@param[in]  path            pathname (may also include the file basename)
@param[in]  normalize_path  If false, it's the callers responsibility to
                            ensure that the path is normalized. */
Fil_path::Fil_path(const std::string &path, bool normalize_path)
    : m_path(path) {
  if (normalize_path) {
    normalize(m_path);
  }

  m_abs_path = get_real_path(m_path, false);
}

/** Constructor
@param[in]  path            Path, not necessarily NUL terminated
@param[in]  normalize_path  If false, it's the callers responsibility to
                            ensure that the path is normalized. */
Fil_path::Fil_path(const char *path, bool normalize_path) : m_path(path) {
  if (normalize_path) {
    normalize(m_path);
  }

  m_abs_path = get_real_path(m_path, false);
}

/** Constructor
@param[in]  path            Path, not necessarily NUL terminated
@param[in]  len             Length of path
@param[in]  normalize_path  If false, it's the callers responsibility to
                            ensure that the path is normalized. */
Fil_path::Fil_path(const char *path, size_t len, bool normalize_path)
    : m_path(path, len) {
  if (normalize_path) {
    normalize(m_path);
  }

  m_abs_path = get_real_path(m_path, false);
}

/** Default constructor. */
Fil_path::Fil_path() : m_path(), m_abs_path() { /* No op */
}

bool Fil_path::is_same_as(const Fil_path &other) const {
  if (path().empty() || other.path().empty()) {
    return false;
  }

  std::string first = abs_path();
  trim_separator(first);

  std::string second = other.abs_path();
  trim_separator(second);

  return (first == second);
}

bool Fil_path::is_same_as(const std::string &other) const {
  if (path().empty() || other.empty()) {
    return false;
  }

  Fil_path other_path(other);

  return is_same_as(other_path);
}

std::pair<std::string, std::string> Fil_path::split(const std::string &path) {
  const auto n = path.rfind(OS_PATH_SEPARATOR);
  ut_ad(n != std::string::npos);
  return {path.substr(0, n), path.substr(n)};
}

bool Fil_path::is_ancestor(const Fil_path &other) const {
  if (path().empty() || other.path().empty()) {
    return false;
  }

  std::string ancestor = abs_path();
  std::string descendant = other.abs_path();

  /* We do not know if the descendant is a dir or a file.
  But the ancestor in this routine is always a directory.
  If it does not yet exist, it may not have a trailing separator.
  If there is no trailing separator, add it. */
  append_separator(ancestor);

  if (descendant.length() <= ancestor.length()) {
    return false;
  }

  return std::equal(ancestor.begin(), ancestor.end(), descendant.begin());
}

bool Fil_path::is_ancestor(const std::string &other) const {
  if (path().empty() || other.empty()) {
    return false;
  }

  Fil_path descendant(other);

  return is_ancestor(descendant);
}

bool Fil_path::is_hidden(std::string path) {
  std::string basename(path);
  while (!basename.empty()) {
    char c = basename.back();
    if (!(Fil_path::is_separator(c) || c == '*')) {
      break;
    }
    basename.resize(basename.size() - 1);
  }
  auto sep = basename.find_last_of(SEPARATOR);

  return (sep != std::string::npos && basename[sep + 1] == '.');
}

#ifdef _WIN32
bool Fil_path::is_hidden(WIN32_FIND_DATA &dirent) {
  if (dirent.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ||
      dirent.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
    return true;
  }

  return false;
}
#endif /* WIN32 */

/** @return true if the path exists and is a file . */
os_file_type_t Fil_path::get_file_type(const std::string &path) {
  os_file_type_t type;

  os_file_status(path.c_str(), nullptr, &type);

  return type;
}

/** Return a string to display the file type of a path.
@param[in]  path  path name
@return true if the path exists and is a file . */
const char *Fil_path::get_file_type_string(const std::string &path) {
  return get_file_type_string(Fil_path::get_file_type(path));
}

/** Return a string to display the file type of a path.
@param[in]  type  OS file type
@return true if the path exists and is a file . */
const char *Fil_path::get_file_type_string(os_file_type_t type) {
  switch (type) {
    case OS_FILE_TYPE_FILE:
      return "file";
    case OS_FILE_TYPE_LINK:
      return "symbolic link";
    case OS_FILE_TYPE_DIR:
      return "directory";
    case OS_FILE_TYPE_BLOCK:
      return "block device";
    case OS_FILE_TYPE_NAME_TOO_LONG:
      return "name too long";
    case OS_FILE_PERMISSION_ERROR:
      return "permission error";
    case OS_FILE_TYPE_MISSING:
      return "missing";
    case OS_FILE_TYPE_UNKNOWN:
    case OS_FILE_TYPE_FAILED:
      break;
  }
  return "unknown";
}

/** @return true if the path exists and is a file . */
bool Fil_path::is_file_and_exists() const {
  return (get_file_type(abs_path()) == OS_FILE_TYPE_FILE);
}

/** @return true if the path exists and is a directory. */
bool Fil_path::is_directory_and_exists() const {
  return (get_file_type(abs_path()) == OS_FILE_TYPE_DIR);
}

/** This validation is only for ':'.
@return true if the path is valid. */
bool Fil_path::is_valid() const {
  auto count = std::count(m_path.begin(), m_path.end(), ':');

  if (count == 0) {
    return true;
  }

#ifdef _WIN32
  /* Do not allow names like "C:name.ibd" because it
  specifies the "C:" drive but allows a relative location.
  It should be like "c:\". If a single colon is used it
  must be the second byte and the third byte must be a
  separator. */

  /* 8 == strlen("c:\a,ibd") */
  if (count == 1 && m_path.length() >= 8 && isalpha(m_path.at(0)) &&
      m_path.at(1) == ':' && (m_path.at(2) == '\\' || m_path.at(2) == '/')) {
    return true;
  }
#endif /* _WIN32 */

  return false;
}

bool Fil_path::is_circular() const {
  size_t first;

  /* Find the first named directory.  It is OK for a path to
  start with "../../../dir". */
  for (first = 0; m_path[first] == OS_SEPARATOR || m_path[first] == '.';
       ++first)
    ;

  size_t back_up = m_path.find(SLASH_DOT_DOT_SLASH, first);
  if (back_up == std::string::npos) {
    return false;
  }

#ifndef _WIN32
  /* If the path contains a symlink before the /../ and the platform
  is not Windows, then '/../' does not go bback through the symlink,
  so it is not circular.  It refers to the parent of the symlinked
  location and we must allow it. On Windows, it backs up to the directory
  where the symlink starts, which is a circular reference. */
  std::string up_path = m_path.substr(0, back_up);
  if (my_is_symlink(up_path.c_str(), nullptr)) {
    return false;
  }
#endif /* _WIN32 */

  return true;
}

/** Sets the flags of the tablespace. The tablespace must be locked
in MDL_EXCLUSIVE MODE.
@param[in]      space   tablespace in-memory struct
@param[in]      flags   tablespace flags */
void fil_space_set_flags(fil_space_t *space, uint32_t flags) {
  ut_ad(fsp_flags_is_valid(flags));

  rw_lock_x_lock(&space->latch, UT_LOCATION_HERE);

  ut_a(flags < std::numeric_limits<uint32_t>::max());
  space->flags = (uint32_t)flags;

  rw_lock_x_unlock(&space->latch);
}

/* Unit Tests */
#ifdef UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH
#define MF Fil_path::make
#define DISPLAY ib::info(ER_IB_MSG_342) << path
void test_make_filepath() {
  char *path;
  const char *long_path =
      "this/is/a/very/long/path/including/a/very/"
      "looooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooong"
      "/folder/name";
  path = MF("/this/is/a/path/with/a/filename", nullptr, IBD, false);
  DISPLAY;
  path = MF("/this/is/a/path/with/a/filename", nullptr, ISL, false);
  DISPLAY;
  path = MF("/this/is/a/path/with/a/filename", nullptr, CFG, false);
  DISPLAY;
  path = MF("/this/is/a/path/with/a/filename", nullptr, CFP, false);
  DISPLAY;
  path = MF("/this/is/a/path/with/a/filename.ibd", nullptr, IBD, false);
  DISPLAY;
  path = MF("/this/is/a/path/with/a/filename.ibd", nullptr, IBD, false);
  DISPLAY;
  path = MF("/this/is/a/path/with/a/filename.dat", nullptr, IBD, false);
  DISPLAY;
  path = MF(nullptr, "tablespacename", NO_EXT, false);
  DISPLAY;
  path = MF(nullptr, "tablespacename", IBD, false);
  DISPLAY;
  path = MF(nullptr, "dbname/tablespacename", NO_EXT, false);
  DISPLAY;
  path = MF(nullptr, "dbname/tablespacename", IBD, false);
  DISPLAY;
  path = MF(nullptr, "dbname/tablespacename", ISL, false);
  DISPLAY;
  path = MF(nullptr, "dbname/tablespacename", CFG, false);
  DISPLAY;
  path = MF(nullptr, "dbname/tablespacename", CFP, false);
  DISPLAY;
  path = MF(nullptr, "dbname\\tablespacename", NO_EXT, false);
  DISPLAY;
  path = MF(nullptr, "dbname\\tablespacename", IBD, false);
  DISPLAY;
  path = MF("/this/is/a/path", "dbname/tablespacename", IBD, false);
  DISPLAY;
  path = MF("/this/is/a/path", "dbname/tablespacename", IBD, true);
  DISPLAY;
  path = MF("./this/is/a/path", "dbname/tablespacename.ibd", IBD, true);
  DISPLAY;
  path = MF("this\\is\\a\\path", "dbname/tablespacename", IBD, true);
  DISPLAY;
  path = MF("/this/is/a/path", "dbname\\tablespacename", IBD, true);
  DISPLAY;
  path = MF(long_path, nullptr, IBD, false);
  DISPLAY;
  path = MF(long_path, "tablespacename", IBD, false);
  DISPLAY;
  path = MF(long_path, "tablespacename", IBD, true);
  DISPLAY;
}
#endif /* UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH */

/** Release the reserved free extents.
@param[in]      n_reserved      number of reserved extents */
void fil_space_t::release_free_extents(ulint n_reserved) {
#ifndef UNIV_HOTBACKUP
  ut_ad(rw_lock_own(&latch, RW_LOCK_X));
#endif /* !UNIV_HOTBACKUP */

  ut_a(n_reserved < std::numeric_limits<uint32_t>::max());
  ut_a(n_reserved_extents >= n_reserved);

  n_reserved_extents -= (uint32_t)n_reserved;
}

#ifndef UNIV_HOTBACKUP

#ifdef UNIV_DEBUG

/** Print the extent descriptor pages of this tablespace into
the given file.
@param[in]      filename        the output file name. */
void fil_space_t::print_xdes_pages(const char *filename) const {
  std::ofstream out(filename);
  print_xdes_pages(out);
}

/** Print the extent descriptor pages of this tablespace into
the given output stream.
@param[in]      out     the output stream.
@return the output stream. */
std::ostream &fil_space_t::print_xdes_pages(std::ostream &out) const {
  mtr_t mtr;
  const page_size_t page_size(flags);

  mtr_start(&mtr);

  for (page_no_t i = 0; i < 100; ++i) {
    page_no_t xdes_page_no = i * UNIV_PAGE_SIZE;

    if (xdes_page_no >= size) {
      break;
    }

    buf_block_t *xdes_block =
        buf_page_get(page_id_t(id, xdes_page_no), page_size, RW_S_LATCH,
                     UT_LOCATION_HERE, &mtr);

    page_t *page = buf_block_get_frame(xdes_block);

    ulint page_type = fil_page_get_type(page);

    switch (page_type) {
      case FIL_PAGE_TYPE_ALLOCATED:

        ut_ad(xdes_page_no >= free_limit);

        mtr_commit(&mtr);
        return out;

      case FIL_PAGE_TYPE_FSP_HDR:
      case FIL_PAGE_TYPE_XDES:
        break;
      default:
        ut_error;
    }

    xdes_page_print(out, page, xdes_page_no, &mtr);
  }

  mtr_commit(&mtr);
  return out;
}
#endif /* UNIV_DEBUG */

/** Initialize the table space encryption
@param[in,out]  space           Tablespace instance */
static void fil_tablespace_encryption_init(const fil_space_t *space) {
  for (auto &key : *recv_sys->keys) {
    if (key.space_id != space->id) {
      continue;
    }

    dberr_t err = DB_SUCCESS;

    ut_ad(!fsp_is_system_tablespace(space->id));

    if (fsp_is_file_per_table(space->id, space->flags)) {
      /* For file-per-table tablespace, which is not INPLACE algorithm, copy
      what is found on REDO Log. */
      err = fil_set_encryption(space->id, Encryption::AES, key.ptr, key.iv);
    } else {
      /* Here we try to populate space tablespace_key which is read during
      REDO scan.

      Consider following scenario:
      1. Alter tablespce .. encrypt=y (KEY1)
      2. Alter tablespce .. encrypt=n
      3. Alter tablespce .. encrypt=y (KEY2)

      Lets say there is a crash after (3) is finished successfully. Let's say
      we scanned till REDO of (1) but couldn't reach to REDO of (3).

      During recovery:
      ----------------
      Case 1:
      - Before crash, pages of tablespace were encrypted with KEY2 and flushed.
      - In recovery, on REDO we've got tablespace key as KEY1.
      - Note, during tablespce load, KEY2 would have been found on page 0 and
        thus loaded already in file_space_t.
      - If we overwrite this space key (KEY2) with the one we got from REDO log
        scan (KEY1), then when we try to read a page from Disk, we will try to
        decrypt it using KEY1 whereas page was encrypted with KEY2. ERROR.
      - So don't overwrite keys on tablespace in this scenario.

      Case 2:
      - Before crash, if tablespace pages were not flushed.
      - On disk, there may be
        - No Key (after decrypt page 0 was flushed)
        - KEY1   (after decrypt, page 0 wasn't flushed)
        - KEY2.  (After 3 starts, page 0 was flushed)
        Thus tablespace would have been loaded accordingly.

      This function is called only during recovery when a tablespce is loaded.
      So we can see the LSN for REDO Entry (recv_sys->keys) and compare it with
      the LSN of page 0 and take decision of updating encryption accordingly. */

      if (space->m_encryption_metadata.m_key_len == 0 ||
          key.lsn > space->m_header_page_flush_lsn) {
        /* Key on tablesapce isn't present or old. Update it. */
        err = fil_set_encryption(space->id, Encryption::AES, key.ptr, key.iv);
      } else {
        /* Key on tablespace is new. Skip updating. */
      }
    }

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_343) << "Can't set encryption information"
                               << " for tablespace" << space->name << "!";
    }

    ut::free(key.iv);
    ut::free(key.ptr);

    key.iv = nullptr;
    key.ptr = nullptr;

    key.space_id = std::numeric_limits<space_id_t>::max();
  }
}

/** Modify table name in Innodb persistent stat tables, if needed. Required
when partitioned table file names from old versions are modified to change
the letter case.
@param[in]      old_path        path to old file
@param[in]      new_path        path to new file */
static void fil_adjust_partition_stat(const std::string &old_path,
                                      const std::string &new_path) {
  char errstr[FN_REFLEN];
  std::string path;

  /* Skip if not IBD file extension. */
  if (!Fil_path::has_suffix(IBD, old_path) ||
      !Fil_path::has_suffix(IBD, new_path)) {
    return;
  }

  /* Check if partitioned table. */
  if (!dict_name::is_partition(old_path) ||
      !dict_name::is_partition(new_path)) {
    return;
  }

  std::string old_name;
  path.assign(old_path);
  if (!Fil_path::parse_file_path(path, IBD, old_name)) {
    return;
  }
  ut_ad(!old_name.empty());

  std::string new_name;
  path.assign(new_path);
  if (!Fil_path::parse_file_path(path, IBD, new_name)) {
    return;
  }
  ut_ad(!new_name.empty());

  /* Required for case insensitive file system where file path letter case
  doesn't matter. We need to keep the name in stat table consistent. */
  dict_name::rebuild(new_name);

  if (old_name.compare(new_name) != 0) {
    dict_stats_rename_table(old_name.c_str(), new_name.c_str(), errstr,
                            sizeof(errstr));
  }
}

/** Update the DD if any files were moved to a new location.
Free the Tablespace_files instance.
@param[in]      read_only_mode  true if InnoDB is started in read only mode.
@return DB_SUCCESS if all OK */
dberr_t Fil_system::prepare_open_for_business(bool read_only_mode) {
  if (read_only_mode && !m_moved.empty()) {
    ib::error(ER_IB_MSG_344)
        << m_moved.size() << " files have been relocated"
        << " and the server has been started in read"
        << " only mode. Cannot update the data dictionary.";

    return DB_READ_ONLY;
  }

  trx_t *trx = check_trx_exists(current_thd);

  TrxInInnoDB trx_in_innodb(trx);

  /* The transaction should not be active yet, start it */

  trx->isolation_level = trx_t::READ_UNCOMMITTED;

  trx_start_if_not_started_xa(trx, false, UT_LOCATION_HERE);

  size_t count = 0;
  size_t failed = 0;
  size_t batch_size = 0;
  bool print_msg = false;
  auto start_time = std::chrono::steady_clock::now();

  /* If some file paths have changed then update the DD */
  for (auto &tablespace : m_moved) {
    dberr_t err;

    auto old_path = std::get<dd_fil::OLD_PATH>(tablespace);

    auto space_name = std::get<dd_fil::SPACE_NAME>(tablespace);

    auto new_path = std::get<dd_fil::NEW_PATH>(tablespace);
    auto object_id = std::get<dd_fil::OBJECT_ID>(tablespace);

    /* We already have the space name in system cs. */
    err = dd_tablespace_rename(object_id, true, space_name.c_str(),
                               new_path.c_str());

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_345) << "Unable to update tablespace ID"
                               << " " << object_id << " "
                               << " '" << old_path << "' to"
                               << " '" << new_path << "'";

      ++failed;
    }

    /* Update persistent stat table if table name is modified. */
    fil_adjust_partition_stat(old_path, new_path);

    ++count;

    if (std::chrono::steady_clock::now() - start_time >= PRINT_INTERVAL) {
      ib::info(ER_IB_MSG_346) << "Processed " << count << "/" << m_moved.size()
                              << " tablespace paths. Failures " << failed;

      start_time = std::chrono::steady_clock::now();
      print_msg = true;
    }

    ++batch_size;

    if (batch_size > 10000) {
      innobase_commit_low(trx);

      ib::info(ER_IB_MSG_347) << "Committed : " << batch_size;

      batch_size = 0;

      trx_start_if_not_started_xa(trx, false, UT_LOCATION_HERE);
    }
  }

  if (batch_size > 0) {
    ib::info(ER_IB_MSG_348) << "Committed : " << batch_size;
  }

  innobase_commit_low(trx);

  if (print_msg) {
    ib::info(ER_IB_MSG_349) << "Updated " << count << " tablespace paths"
                            << ", failures " << failed;
  }

  return failed == 0 ? DB_SUCCESS : DB_ERROR;
}

/** Free the Tablespace_files instance.
@param[in]      read_only_mode  true if InnoDB is started in read only mode.
@return DB_SUCCESS if all OK */
dberr_t fil_open_for_business(bool read_only_mode) {
  return fil_system->prepare_open_for_business(read_only_mode);
}

/** Replay a file rename operation for ddl replay.
@param[in]      page_id         Space ID and first page number in the file
@param[in]      old_name        old file name
@param[in]      new_name        new file name
@return whether the operation was successfully applied (the name did not
exist, or new_name did not exist and name was successfully renamed to
new_name)  */
bool fil_op_replay_rename_for_ddl(const page_id_t &page_id,
                                  const char *old_name, const char *new_name) {
  space_id_t space_id = page_id.space();
  fil_space_t *space = fil_space_get(space_id);

  if (space == nullptr && !fil_system->open_for_recovery(space_id)) {
    ib::info(ER_IB_MSG_350)
        << "Can not find space with space ID " << space_id
        << " when replaying the DDL log "
        << "rename from '" << old_name << "' to '" << new_name << "'";

    return true;
  }

  return fil_op_replay_rename(page_id, old_name, new_name);
}

/** Lookup the tablespace ID for recovery and DDL log apply.
@param[in]      space_id                Tablespace ID to lookup
@return true if the space ID is known. */
bool Fil_system::lookup_for_recovery(space_id_t space_id) {
  ut_ad(recv_recovery_is_on() || Log_DDL::is_in_recovery());

  /* Single threaded code, no need to acquire mutex. */
  const auto result = get_scanned_filename_by_space_id(space_id);

  if (recv_recovery_is_on()) {
    const auto &end = recv_sys->deleted.end();
    const auto &it = recv_sys->deleted.find(space_id);

    if (result.second == nullptr) {
      /* If it wasn't deleted after finding it on disk then
      we tag it as missing. */

      if (it == end) {
        recv_sys->missing_ids.insert(space_id);
      }

      return false;
    }

    /* Check that it wasn't deleted. */

    return (it == end);
  }

  return (result.second != nullptr);
}

/** Lookup the tablespace ID.
@param[in]      space_id                Tablespace ID to lookup
@return true if the space ID is known. */
bool fil_tablespace_lookup_for_recovery(space_id_t space_id) {
  return fil_system->lookup_for_recovery(space_id);
}

dberr_t Fil_system::open_for_recovery(space_id_t space_id) {
  ut_ad(recv_recovery_is_on() || Log_DDL::is_in_recovery());

  if (!lookup_for_recovery(space_id)) {
    return DB_FAIL;
  }

  const auto result = get_scanned_filename_by_space_id(space_id);

  /* Duplicates should have been sorted out before start of recovery. */
  ut_a(result.second->size() == 1);

  const auto &filename = result.second->front();
  const std::string path = result.first + filename;

  fil_space_t *space;

  auto status = ibd_open_for_recovery(space_id, path, space);

  if (status == FIL_LOAD_DBWLR_CORRUPTION) {
    return DB_CORRUPTION;
  }

  dberr_t err = DB_SUCCESS;

  if (status == FIL_LOAD_OK) {
    /* In the case of undo tablespace, even if the encryption flag is not
    enabled in space->flags, the encryption keys needs to be restored from
    recv_sys->keys to the corresponding fil_space_t object. */
    const bool is_undo = fsp_is_undo_tablespace(space_id);

    if ((FSP_FLAGS_GET_ENCRYPTION(space->flags) || is_undo ||
         space->encryption_op_in_progress ==
             Encryption::Progress::ENCRYPTION) &&
        recv_sys->keys != nullptr) {
      fil_tablespace_encryption_init(space);
    }

    if (!recv_sys->dblwr->empty()) {
      err = recv_sys->dblwr->recover(space);

    } else {
      ib::info(ER_IB_MSG_DBLWR_1317) << "DBLWR recovery skipped for "
                                     << space->name << " ID: " << space->id;
    }

    return err;
  }

  return DB_FAIL;
}

dberr_t fil_tablespace_open_for_recovery(space_id_t space_id) {
  return fil_system->open_for_recovery(space_id);
}

Fil_state fil_tablespace_path_equals(space_id_t space_id,
                                     const char *space_name, ulint fsp_flags,
                                     std::string old_path,
                                     std::string *new_path) {
  ut_ad((fsp_is_ibd_tablespace(space_id) &&
         Fil_path::has_suffix(IBD, old_path)) ||
        fsp_is_undo_tablespace(space_id));

  /* Watch out for implicit undo tablespaces that are created during startup.
  They will not be in the list of scanned files.  But the DD might need to be
  updated if the undo directory is different now from when the database was
  initialized.  The DD will be updated if we put it in fil_system->moved. */
  if (fsp_is_undo_tablespace(space_id)) {
    undo::spaces->s_lock();
    space_id_t space_num = undo::id2num(space_id);
    undo::Tablespace *undo_space = undo::spaces->find(space_num);

    if (undo_space != nullptr && undo_space->is_new()) {
      *new_path = undo_space->file_name();
      Fil_state state = ((old_path.compare(*new_path) == 0) ? Fil_state::MATCHES
                                                            : Fil_state::MOVED);
      undo::spaces->s_unlock();
      return state;
    }
    undo::spaces->s_unlock();
  }

  /* Single threaded code, no need to acquire mutex. */
  const auto &end = recv_sys->deleted.end();
  const auto &it = recv_sys->deleted.find(space_id);
  const auto result = fil_system->get_scanned_filename_by_space_id(space_id);

  if (result.second == nullptr) {
    /* The file was not scanned but the DD has the tablespace. Either;
    1. This file is missing
    2. The file could not be opened because of encryption or something else,
    3. The path is not included in --innodb-directories.
    We need to check if the DD path is valid before we tag the file
    as missing. */

    if (Fil_path::get_file_type(old_path) == OS_FILE_TYPE_FILE) {
      /* This file from the DD exists where the DD thinks it is. It will be
      opened later.  Make some noise if the location is unknown. */
      if (!fil_path_is_known(old_path)) {
        ib::warn(ER_IB_MSG_UNPROTECTED_LOCATION_ALLOWED, old_path.c_str(),
                 space_name);
      }
      return Fil_state::MATCHES;
    }

    /* If it wasn't deleted during redo apply, we tag it as missing. */

    if (it == end && recv_recovery_is_on()) {
      recv_sys->missing_ids.insert(space_id);
    }

    return Fil_state::MISSING;
  }

  /* Check if it was deleted according to the redo log. */
  if (it != end) {
    return Fil_state::DELETED;
  }

  /* A file with this space_id was found during scanning.
  Validate its location and check if it was moved from where
  the DD thinks it is.

  Don't compare the full filename, there can be a mismatch if
  there was a DDL in progress and we will end up renaming the path
  in the DD dictionary. Such renames should be handled by the
  atomic DDL "ddl_log". */

  std::string old_dir{old_path};

  /* Ignore the filename component of the old path. */
  auto pos = old_dir.find_last_of(Fil_path::SEPARATOR);
  if (pos == std::string::npos) {
    old_dir = MySQL_datadir_path;
  } else {
    old_dir.resize(pos + 1);
    ut_ad(Fil_path::is_separator(old_dir.back()));
  }
  old_dir = Fil_path::get_real_path(old_dir);

  /* Build the new path from the scan path and the found path. */
  std::string new_dir{result.first};

  ut_ad(Fil_path::is_separator(new_dir.back()));

  new_dir.append(result.second->front());

  new_dir = Fil_path::get_real_path(new_dir);

  /* Do not use a datafile that is in the wrong place. */
  if (!Fil_path::is_valid_location(space_name, space_id, fsp_flags, new_dir)) {
    return Fil_state::MISSING;
  }

  /* Ignore the filename component of the new path. */
  pos = new_dir.find_last_of(Fil_path::SEPARATOR);

  ut_ad(pos != std::string::npos);

  new_dir.resize(pos + 1);

  if (old_dir.compare(new_dir) != 0) {
    *new_path = result.first + result.second->front();
    return Fil_state::MOVED;
  }

  *new_path = old_path;
  return Fil_state::MATCHES;
}

void fil_add_moved_space(dd::Object_id dd_object_id, space_id_t space_id,
                         const char *space_name, const std::string &old_path,
                         const std::string &new_path) {
  /* Keep space_name in system cs. We handle it while modifying DD. */
  fil_system->moved(dd_object_id, space_id, space_name, old_path, new_path);
}

bool fil_update_partition_name(space_id_t space_id, uint32_t fsp_flags,
                               bool update_space, std::string &space_name,
                               std::string &dd_path) {
#ifdef _WIN32
  /* Safe check. Never needed on Windows for path. */
  if (!update_space) {
    return false;
  }
#endif /* WIN32 */

  /* Never needed in case insensitive file system for path. */
  if (!update_space && lower_case_file_system) {
    return false;
  }

  /* Only needed for file per table. */
  if (update_space && !fsp_is_file_per_table(space_id, fsp_flags)) {
    return false;
  }

  /* Extract dictionary name schema_name/table_name from dd path. */
  std::string table_name;

  if (!Fil_path::parse_file_path(dd_path, IBD, table_name)) {
    /* Not a valid file-per-table IBD path */
    return false;
  }
  ut_ad(!table_name.empty());

  /* Only needed for partition file. */
  if (!dict_name::is_partition(table_name)) {
    return false;
  }

  /* Rebuild dictionary name to convert partition names to lower case. */
  dict_name::rebuild(table_name);

  if (update_space) {
    /* Rebuild space name if required. */
    dict_name::rebuild_space(table_name, space_name);
  }

  /* No need to update file name for lower case file system. */
  if (lower_case_file_system) {
    return false;
  }

  /* Rebuild path and compare. */
  std::string table_path = Fil_path::make_new_path(dd_path, table_name, IBD);
  ut_ad(!table_path.empty());

  if (dd_path.compare(table_path) != 0) {
    /* Validate that the file exists. */
    if (os_file_exists(table_path.c_str())) {
      dd_path.assign(table_path);
      return true;

    } else {
      ib::warn(ER_IB_WARN_OPEN_PARTITION_FILE, table_path.c_str());
    }
  }

  return false;
}

#endif /* !UNIV_HOTBACKUP */

/** This function should be called after recovery has completed.
Check for tablespace files for which we did not see any MLOG_FILE_DELETE
or MLOG_FILE_RENAME record. These could not be recovered.
@return true if there were some filenames missing for which we had to
        ignore redo log records during the apply phase */
bool Fil_system::check_missing_tablespaces() {
  bool missing = false;
  const auto end = recv_sys->deleted.end();

  /* Called in single threaded mode, no need to acquire the mutex. */

  recv_sys->dblwr->check_missing_tablespaces();

  for (auto space_id : recv_sys->missing_ids) {
    if (recv_sys->deleted.find(space_id) != end) {
      continue;
    }

    const auto result = get_scanned_filename_by_space_id(space_id);

    if (result.second == nullptr) {
      if (fsp_is_undo_tablespace(space_id)) {
        /* This could happen if an undo truncate is in progress because
        undo tablespace construction is not redo logged.  The DD is updated
        at the end and may be out of sync. */
        continue;
      }

      ib::error(ER_IB_MSG_354) << "Could not find any file associated with"
                               << " the tablespace ID: " << space_id;
      missing = true;

    } else {
      ut_a(!result.second->empty());
    }
  }

  return missing;
}

/** This function should be called after recovery has completed.
Check for tablespace files for which we did not see any MLOG_FILE_DELETE
or MLOG_FILE_RENAME record. These could not be recovered
@return true if there were some filenames missing for which we had to
        ignore redo log records during the apply phase */
bool fil_check_missing_tablespaces() {
  return fil_system->check_missing_tablespaces();
}

/** Redo a tablespace create.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      page_id         Tablespace Id and first page in file
@param[in]      parsed_bytes    Number of bytes parsed so far
@param[in]      parse_only      Don't apply, parse only
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
byte *fil_tablespace_redo_create(byte *ptr, const byte *end,
                                 const page_id_t &page_id, ulint parsed_bytes,
                                 bool parse_only) {
  ut_a(page_id.page_no() == 0);

  /* We never recreate the system tablespace. */
  ut_a(page_id.space() != TRX_SYS_SPACE);

  ut_a(parsed_bytes != ULINT_UNDEFINED);

  /* Where 6 = flags (uint32_t) + name len (uint16_t). */
  if (end <= ptr + 6) {
    return nullptr;
  }

#ifdef UNIV_HOTBACKUP
  uint32_t flags = mach_read_from_4(ptr);
#else
  /* Skip the flags, not used here. */
#endif /* UNIV_HOTBACKUP */

  ptr += 4;

  ulint len = mach_read_from_2(ptr);

  ptr += 2;

  /* Do we have the full/valid file name. */
  if (end < ptr + len || len < 5) {
    if (len < 5) {
      char name[6];

      snprintf(name, sizeof(name), "%.*s", (int)len, ptr);

      ib::error(ER_IB_MSG_355) << "MLOG_FILE_CREATE : Invalid file name."
                               << " Length (" << len << ") must be >= 5"
                               << " and end in '.ibd'. File name in the"
                               << " redo log is '" << name << "'";

      recv_sys->found_corrupt_log = true;
    }

    return nullptr;
  }

  char *name = reinterpret_cast<char *>(ptr);

  Fil_path::normalize(name);

  ptr += len;

  if (!(Fil_path::has_suffix(IBD, name) ||
        fsp_is_undo_tablespace(page_id.space()))) {
    recv_sys->found_corrupt_log = true;

    return nullptr;
  }

  if (parse_only) {
    return ptr;
  }
#ifdef UNIV_HOTBACKUP

  meb_tablespace_redo_create(page_id, flags, name);

#else  /* !UNIV_HOTBACKUP */

  const auto result =
      fil_system->get_scanned_filename_by_space_id(page_id.space());

  if (result.second == nullptr) {
    /* No file maps to this tablespace ID. It's possible that
    the file was deleted later or is missing. */

    return ptr;
  }

  /* Update filename with correct partition case, if needed. */
  std::string name_str(name);
  std::string space_name;
  fil_update_partition_name(page_id.space(), 0, false, space_name, name_str);

  auto abs_name = Fil_path::get_real_path(name_str);

  /* Duplicates should have been sorted out before we get here. */
  ut_a(result.second->size() == 1);

  /* It's possible that the tablespace file was renamed later. */
  if (result.second->front().compare(abs_name) == 0) {
    dberr_t success = fil_tablespace_open_for_recovery(page_id.space());

    if (success != DB_SUCCESS) {
      ib::info(ER_IB_MSG_356) << "Create '" << abs_name << "' failed!";
    }
  }
#endif /* UNIV_HOTBACKUP */

  return ptr;
}

byte *fil_tablespace_redo_rename(byte *ptr, const byte *end,
                                 const page_id_t &page_id, ulint parsed_bytes,
                                 bool parse_only [[maybe_unused]]) {
  ut_a(page_id.page_no() == 0);

  /* We never recreate the system tablespace. */
  ut_a(page_id.space() != TRX_SYS_SPACE);

  ut_a(parsed_bytes != ULINT_UNDEFINED);

  /* Where 2 = from name len (uint16_t). */
  if (end <= ptr + 2) {
    return nullptr;
  }

  /* Read and check the RENAME FROM_NAME. */
  ulint from_len = mach_read_from_2(ptr);
  ptr += 2;
  char *from_name = reinterpret_cast<char *>(ptr);

  /* Check if the 'from' file name is valid. */
  if (end < ptr + from_len) {
    return nullptr;
  }

  std::string whats_wrong;
  constexpr char more_than_five[] = "The length must be >= 5.";
  constexpr char end_with_ibd[] = "The file suffix must be '.ibd'.";
  if (from_len < 5) {
    recv_sys->found_corrupt_log = true;
    whats_wrong.assign(more_than_five);
  } else {
    std::string name{from_name};

    if (!Fil_path::has_suffix(IBD, name)) {
      recv_sys->found_corrupt_log = true;
      whats_wrong.assign(end_with_ibd);
    }
  }

  if (recv_sys->found_corrupt_log) {
    ib::info(ER_IB_MSG_357) << "MLOG_FILE_RENAME: Invalid {from} file name: '"
                            << from_name << "'. " << whats_wrong;

    return nullptr;
  }

  ptr += from_len;
  Fil_path::normalize(from_name);

  /* Read and check the RENAME TO_NAME. */
  ulint to_len = mach_read_from_2(ptr);
  ptr += 2;
  char *to_name = reinterpret_cast<char *>(ptr);

  /* Check if the 'to' file name is valid. */
  if (end < ptr + to_len) {
    return nullptr;
  }

  if (to_len < 5) {
    recv_sys->found_corrupt_log = true;
    whats_wrong.assign(more_than_five);
  } else {
    std::string name{to_name};

    if (!Fil_path::has_suffix(IBD, name)) {
      recv_sys->found_corrupt_log = true;
      whats_wrong.assign(end_with_ibd);
    }
  }

  if (recv_sys->found_corrupt_log) {
    ib::info(ER_IB_MSG_357) << "MLOG_FILE_RENAME: Invalid {to} file name: '"
                            << to_name << "'. " << whats_wrong;

    return nullptr;
  }

  ptr += to_len;
  Fil_path::normalize(to_name);

#ifdef UNIV_HOTBACKUP

  if (!parse_only) {
    meb_tablespace_redo_rename(page_id, from_name, to_name);
  }

#else /* !UNIV_HOTBACKUP */

  /* Update filename with correct partition case, if needed. */
  std::string to_name_str(to_name);
  std::string space_name;
  fil_update_partition_name(page_id.space(), 0, false, space_name, to_name_str);

  if (from_len == to_len && strncmp(to_name, from_name, to_len) == 0) {
    ib::error(ER_IB_MSG_360)
        << "MLOG_FILE_RENAME: The from and to name are the"
        << " same: '" << from_name << "', '" << to_name << "'";

    recv_sys->found_corrupt_log = true;

    return nullptr;
  }

#endif /* UNIV_HOTBACKUP */

  return ptr;
}

byte *fil_tablespace_redo_extend(byte *ptr, const byte *end,
                                 const page_id_t &page_id, ulint parsed_bytes,
                                 bool parse_only) {
  ut_a(page_id.page_no() == 0);

  /* We never recreate the system tablespace. */
  ut_a(page_id.space() != TRX_SYS_SPACE);

  ut_a(parsed_bytes != ULINT_UNDEFINED);

  /* Check for valid offset and size values */
  if (end < ptr + 16) {
    return nullptr;
  }

  /* Offset within the file to start writing zeros */
  os_offset_t offset = mach_read_from_8(ptr);
  ptr += 8;

  /* Size of the space which needs to be initialized by
  writing zeros */
  os_offset_t size = mach_read_from_8(ptr);
  ptr += 8;

  if (size == 0) {
    ib::error(ER_IB_MSG_INCORRECT_SIZE)
        << "MLOG_FILE_EXTEND: Incorrect value for size encountered."
        << "Redo log corruption found.";
    recv_sys->found_corrupt_log = true;
    return nullptr;
  }

  if (parse_only) {
    return ptr;
  }

#ifndef UNIV_HOTBACKUP
  const auto result =
      fil_system->get_scanned_filename_by_space_id(page_id.space());

  if (result.second == nullptr) {
    /* No files found for this tablespace ID. It's possible that the
    files were deleted later. */
    return ptr;
  }

  dberr_t err = fil_tablespace_open_for_recovery(page_id.space());

  if (err != DB_SUCCESS) {
    /* fil_tablespace_open_for_recovery may fail if the tablespace being
    opened is an undo tablespace which is also marked for truncation.
    In such a case, skip processing this redo log further and goto the
    next record without doing anything more here. */
    if (fsp_is_undo_tablespace(page_id.space()) &&
        undo::is_active_truncate_log_present(undo::id2num(page_id.space()))) {
      return ptr;
    }
    return nullptr;
  }

  /* Open the space */
  bool success = fil_space_open(page_id.space());

  if (!success) {
    return nullptr;
  }

  fil_space_t *space = fil_space_get(page_id.space());

  ut_a(space != nullptr);
  ut_a(!space->files.empty());

  /* Space extension operations on temporary tablespaces
  are not redo logged as they are always recreated on
  server startup. */
  ut_a(space->purpose != FIL_TYPE_TEMPORARY);

  fil_node_t *file = &space->files.back();

  ut_a(file != nullptr);

  page_size_t page_size(space->flags);

  size_t phy_page_size = page_size.physical();

  /* No one else should be extending this file. */
  ut_a(!file->is_being_extended);

  ut_a(offset > 0);
  os_offset_t initial_fsize = os_file_get_size(file->handle);
  ut_a(offset <= initial_fsize);
  /* file->size unit is FSP_EXTENT_SIZE.
  Disk-full might cause partial FSP_EXTENT_SIZE extension. */
  ut_a(initial_fsize / (phy_page_size * FSP_EXTENT_SIZE) ==
       file->size / FSP_EXTENT_SIZE);

  /* Because punch_hole flush might recover disk-full situation.
  We might be able to extend from the partial extension at the
  previous disk-full. So, offset might not be at boundary.
  But target is aligned to the page boundary */
  ut_a(((offset + size) % phy_page_size) == 0);

  /* If the physical size of the file is greater than or equal to the
  expected size (offset + size), it means that posix_fallocate was
  successfully executed.
  However, if the redo log record requests an expected size (offset + size)
  which is more than the physical size of the file, it means that
  posix_fallocate() either allocated partially (in case of emulated
  posix_fallocate) or did not allocate at all. In case posix_fallocate()
  fails, the server calls fil_write_zeros to extend the space, which
  can also fail after allocating space partially because of reasons like
  lack of disk space. The real indicator of how much file was actually
  allocated is the physical file size itself.
  Write out the 0's in the extended space only if the physical size of
  the file is less than the expected size (offset + size). */

  /* If the file is already equal or larger than the expected size,
  nothing more to do here. */
  if ((offset + size) <= initial_fsize) {
    return ptr;
  }

#if defined(UNIV_DEBUG)
  /* Validate that there are no pages in the buffer pool. */
  buf_must_be_all_freed();
#endif /* UNIV_DEBUG */

  /* Adjust the actual allocation size to take care of the allocation
  problems described above.
  Find out the size by which the file should be extended to have
  a file of expected size while ensuring that the already allocated
  pages are not overwritten with zeros. */
  os_offset_t new_ext_size = size - (initial_fsize - offset);

  /* Initialize the region starting from current end of file with zeros. */
  err = fil_write_zeros(file, phy_page_size, initial_fsize, new_ext_size);

  if (err != DB_SUCCESS) {
    /* Error writing zeros to the file. */
    ib::warn(ER_IB_MSG_320) << "Error while writing " << size << " zeroes to "
                            << file->name << " starting at offset " << offset;
    /* Should return normally. If "return nullptr", it means "broken log"
    and will skip to apply the all of following logs. */
  }

  /* Get the final size of the file and adjust file->size accordingly. */
  os_offset_t end_fsize = os_file_get_size(file->handle);

  file->size = end_fsize / phy_page_size;
  space->size = file->size;

  fil_flush(space->id);

  fil_space_close(space->id);
#endif /* !UNIV_HOTBACKUP */

  return ptr;
}

/** Redo a tablespace delete.
@param[in]      ptr             redo log record
@param[in]      end             end of the redo log buffer
@param[in]      page_id         Tablespace Id and first page in file
@param[in]      parsed_bytes    Number of bytes parsed so far
@param[in]      parse_only      Don't apply, parse only
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
byte *fil_tablespace_redo_delete(byte *ptr, const byte *end,
                                 const page_id_t &page_id, ulint parsed_bytes,
                                 bool parse_only) {
  ut_a(page_id.page_no() == 0);

  /* We never recreate the system tablespace. */
  ut_a(page_id.space() != TRX_SYS_SPACE);

  ut_a(parsed_bytes != ULINT_UNDEFINED);

  /* Where 2 =  len (uint16_t). */
  if (end <= ptr + 2) {
    return nullptr;
  }

  ulint len = mach_read_from_2(ptr);

  ptr += 2;

  /* Do we have the full/valid file name. */
  if (end < ptr + len || len < 5) {
    if (len < 5) {
      char name[6];

      snprintf(name, sizeof(name), "%.*s", (int)len, ptr);

      ib::error(ER_IB_MSG_362) << "MLOG_FILE_DELETE : Invalid file name."
                               << " Length (" << len << ") must be >= 5"
                               << " and end in '.ibd'. File name in the"
                               << " redo log is '" << name << "'";
    }

    return nullptr;
  }

  char *name = reinterpret_cast<char *>(ptr);

  Fil_path::normalize(name);

  ptr += len;

  if (!(Fil_path::has_suffix(IBD, name) ||
        fsp_is_undo_tablespace(page_id.space()))) {
    recv_sys->found_corrupt_log = true;

    return nullptr;
  }

  if (parse_only) {
    return ptr;
  }
#ifdef UNIV_HOTBACKUP

  meb_tablespace_redo_delete(page_id, name);

#else  /* !UNIV_HOTBACKUP */

  const auto result =
      fil_system->get_scanned_filename_by_space_id(page_id.space());

  recv_sys->deleted.insert(page_id.space());
  recv_sys->missing_ids.erase(page_id.space());

  if (result.second == nullptr) {
    /* No files map to this tablespace ID. The drop must
    have succeeded. */

    return ptr;
  }

  /* Space_id_set should have been sorted out before we get here. */

  ut_a(result.second->size() == 1);

  /* Update filename with correct partition case, if needed. */
  std::string name_str(name);
  std::string space_name;
  fil_update_partition_name(page_id.space(), 0, false, space_name, name_str);

  fil_space_free(page_id.space(), false);

  bool success = fil_system->erase_path(page_id.space());
  ut_a(success);
#endif /* UNIV_HOTBACKUP */

  return ptr;
}

byte *fil_tablespace_redo_encryption(byte *ptr, const byte *end,
                                     space_id_t space_id, lsn_t lsn) {
  fil_space_t *space = fil_space_get(space_id);

  /* An undo space might be open but not have the ENCRYPTION bit set
  in its header if the current value of innodb_undo_log_encrypt=OFF
  and a crash occurred between flushing this redo record and the header
  page of the undo space.  So if the flag is missing, ignore the header
  page. */
  if (fsp_is_undo_tablespace(space_id) && space != nullptr &&
      !FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
    space = nullptr;
  }

  ulint offset = mach_read_from_2(ptr);
  ptr += 2;

  const ulint len = mach_read_from_2(ptr);
  ptr += 2;

  if (end < ptr + len) {
    return (nullptr);
  }

  if (offset >= UNIV_PAGE_SIZE || len + offset > UNIV_PAGE_SIZE ||
      len != Encryption::INFO_SIZE) {
    recv_sys->found_corrupt_log = true;
    return (nullptr);
  }

  byte *encryption_ptr = ptr;
  ptr += len;

  /* If space is already loaded and have header_page_flushed_lsn greater than
  this REDO entry LSN, then skip it coz header has latest information. */
  if (space != nullptr && space->m_header_page_flush_lsn > lsn) {
    return (ptr);
  }

  /* If encryption info is 0 filled, then this is erasing encryption info
  during unencryption operation. Skip decrypting it. */
  {
    byte buf[Encryption::INFO_SIZE] = {0};

    if (memcmp(encryption_ptr + 4, buf, Encryption::INFO_SIZE - 4) == 0) {
      /* NOTE: We don't need to reset encryption info of space here because it
      might be needed. It will be reset when this REDO record is applied. */
      return (ptr);
    }
  }

  byte iv[Encryption::KEY_LEN] = {0};
  byte key[Encryption::KEY_LEN] = {0};

  Encryption_key e_key{key, iv};
  if (!Encryption::decode_encryption_info(space_id, e_key, encryption_ptr,
                                          true)) {
    recv_sys->found_corrupt_log = true;

    ib::warn(ER_IB_MSG_364)
        << "Encryption information"
        << " in the redo log of space " << space_id << " is invalid";

    return (nullptr);
  }

  ut_ad(len == Encryption::INFO_SIZE);

  if (space != nullptr) {
    Encryption::set_or_generate(Encryption::AES, key, iv,
                                space->m_encryption_metadata);
    fsp_flags_set_encryption(space->flags);
    return ptr;
  }

  /* Space is not loaded yet. Remember this key in recv_sys and use it later
  to pupulate space encryption info once it is loaded. */
  DBUG_EXECUTE_IF("dont_update_key_found_during_REDO_scan", return ptr;);

  if (recv_sys->keys == nullptr) {
    recv_sys->keys =
        ut::new_withkey<recv_sys_t::Encryption_Keys>(UT_NEW_THIS_FILE_PSI_KEY);
  }

  /* Search if key entry already exists for this tablespace, update it. */
  for (auto &recv_key : *recv_sys->keys) {
    if (recv_key.space_id == space_id) {
      memcpy(recv_key.iv, iv, Encryption::KEY_LEN);
      memcpy(recv_key.ptr, key, Encryption::KEY_LEN);
      recv_key.lsn = lsn;
      return ptr;
    }
  }

  /* No existing entry found, create new one and insert it. */
  recv_sys_t::Encryption_Key new_key;
  new_key.iv = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, Encryption::KEY_LEN));
  memcpy(new_key.iv, iv, Encryption::KEY_LEN);
  new_key.ptr = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, Encryption::KEY_LEN));
  memcpy(new_key.ptr, key, Encryption::KEY_LEN);
  new_key.space_id = space_id;
  new_key.lsn = lsn;
  recv_sys->keys->push_back(new_key);

  return ptr;
}

void Tablespace_dirs::warn_ignore(std::string ignore_path, const char *reason) {
  ib::warn(ER_IB_MSG_IGNORE_SCAN_PATH, ignore_path.c_str(), reason);
}

void Tablespace_dirs::add_path(const std::string &path_in, bool is_undo_dir) {
  /* Ignore an invalid path. */
  if (path_in == "") {
    return;
  }
  if (path_in == "/") {
    warn_ignore(path_in,
                "the root directory '/' is not allowed to be scanned.");
    return;
  }
  if (std::string::npos != path_in.find('*')) {
    warn_ignore(path_in, "it contains '*'.");
    return;
  }

  /* Assume this path is a directory and put a trailing slash on it. */
  std::string dir_in(path_in);
  Fil_path::append_separator(dir_in);

  Fil_path found_path(dir_in, true);

  /* Exclude this path if it is a duplicate of a path already stored or
  if a previously stored path is an ancestor.  Remove any previously stored
  path that is a descendant of this path. */
  for (auto it = m_dirs.cbegin(); it != m_dirs.cend(); /* No op */) {
    if (it->root().is_same_as(found_path)) {
      /* The exact same path is obviously ignored, so there is no need to
      log a warning. */
      return;
    }

    /* Check if dir_abs_path is an ancestor of this path */
    if (it->root().is_ancestor(found_path)) {
      /* Descendant directories will be scanned recursively, so don't
      add it to the scan list.  Log a warning unless this descendant
      is the undo directory since it must be supplied even if it is
      a descendant of another data location. */
      if (!is_undo_dir) {
        std::string reason = "it is a sub-directory of '";
        reason += it->root().abs_path();
        warn_ignore(path_in, reason.c_str());
      }
      return;
    }

    if (found_path.is_ancestor(it->root())) {
      /* This path is an ancestor of an existing dir in fil_system::m_dirs.
      The settings have overlapping locations.  Put a note about it to
      the error log. The undo_dir is added last, so if it is an ancestor,
      the descendant was listed as a datafile directory. So always issue
      this message*/
      std::string reason = "it is a sub-directory of '";
      reason += found_path;
      warn_ignore(it->root().path(), reason.c_str());

      /* It might also be an ancestor to another dir as well, so keep looking.
      We must delete this descendant because we know that this ancestor path
      will be inserted and all its descendants will be scanned. */
      it = m_dirs.erase(it);
    } else {
      it++;
    }
  }

  m_dirs.push_back(Tablespace_files{found_path.path()});
  return;
}

void Tablespace_dirs::add_paths(const std::string &str,
                                const std::string &delimiters) {
  std::string::size_type start = 0;
  std::string::size_type end = 0;

  /* Scan until 'start' reaches the end of the string (npos) */
  for (;;) {
    start = str.find_first_not_of(delimiters, end);
    if (std::string::npos == start) {
      break;
    }

    end = str.find_first_of(delimiters, start);

    const auto path = str.substr(start, end - start);

    add_path(path);
  }
}

/** Check whether we can rename the file
@param[in]      space           Tablespace for which to rename
@param[in]      name            Source file name
@param[in]      df              Target file that exists on disk
@return DB_SUCCESS if all OK */
static dberr_t fil_rename_validate(fil_space_t *space, const std::string &name,
                                   Datafile &&df) {
  dberr_t err = df.validate_for_recovery(space->id);
  /* The validate_for_recovery will set space_id, but will close the file. It is
  safe to access filepath and space_id. */

  if (err == DB_TABLESPACE_NOT_FOUND) {
    /* Tablespace header doesn't contain the expected
    tablespace ID. This is can happen during truncate. */

    return err;

  } else if (err != DB_SUCCESS) {
    ib::warn(ER_IB_MSG_367) << "Failed to read the first page of the"
                            << " file '" << df.filepath() << "'."
                            << " You will need to verify and move the"
                            << " file out of the way retry recovery.";

    return err;
  }

  auto file = &space->files.front();

  if (strcmp(df.filepath(), file->name) == 0) {
    /* Check if already points to the correct file.
    Must have the same space ID */

    ib::info(ER_IB_MSG_368) << "Tablespace ID already maps to: '"
                            << df.filepath() << "', rename ignored.";

    ut_a(df.space_id() == space->id);

    return DB_SUCCESS;

  } else if (df.space_id() != space->id) {
    /* Target file exists on disk but has a different
    tablespace ID. The user should manually delete it. */

    ib::error(ER_IB_MSG_369)
        << "Cannot rename '" << name << "' to '" << df.filepath() << "'. File '"
        << df.filepath() << "' tablespace ID " << df.space_id()
        << " doesn't match the expected tablespace"
        << " ID " << space->id << ". You will need to verify and move '"
        << df.filepath() << "' manually and retry recovery!";

    return DB_ERROR;
  }

  /* Target file exists on disk and has the same ID. */

  ib::error(ER_IB_MSG_370)
      << "Cannot rename '" << name << "' to '" << df.filepath()
      << "'. The File '" << df.filepath() << " already exists on"
      << " disk. You will need to verify and move either file"
      << " manually and retry recovery!";

  return DB_ERROR;
}

/** Replay a file rename operation if possible.
@param[in]      page_id         Space ID and first page number in the file
@param[in]      old_name        old file name
@param[in]      new_name        new file name
@return whether the operation was successfully applied (the name did not exist,
or new_name did not exist and name was successfully renamed to new_name) */
static bool fil_op_replay_rename(const page_id_t &page_id,
                                 const std::string &old_name,
                                 const std::string &new_name) {
  ut_ad(page_id.page_no() == 0);
  ut_ad(old_name.compare(new_name) != 0);
  ut_ad(Fil_path::has_suffix(IBD, new_name));
  ut_ad(page_id.space() != TRX_SYS_SPACE);

  /* In order to replay the rename, the following must hold:
  1. The new name is not already used.
  2. A tablespace exists with the old name.
  3. The space ID for that tablespace matches this log entry.
  This will prevent unintended renames during recovery. */

  space_id_t space_id = page_id.space();
  fil_space_t *space = fil_space_get(space_id);

  if (space == nullptr) {
    return true;
  }

  std::string name{new_name};
  {
    Datafile df;

    df.set_filepath(name.c_str());

    if (df.open_read_only(false) == DB_SUCCESS) {
      dberr_t err = fil_rename_validate(space, old_name, std::move(df));

      if (err == DB_TABLESPACE_NOT_FOUND) {
        /* This can happen during truncate. */
        ib::info(ER_IB_MSG_371) << "Tablespace ID mismatch in '" << name << "'";
      }
      return (err == DB_SUCCESS);
    }
  }

  auto path_sep_pos = name.find_last_of(Fil_path::SEPARATOR);

  ut_a(path_sep_pos != std::string::npos);

  /* Create the database directory for the new name, if
  it does not exist yet */

  name.resize(path_sep_pos);

  bool success = os_file_create_directory(name.c_str(), false);
  ut_a(success);

  auto datadir_pos = name.find_last_of(Fil_path::SEPARATOR);

  ut_ad(datadir_pos != std::string::npos);

  name.erase(0, datadir_pos + 1);

  ut_ad(!Fil_path::is_separator(name.back()));

  /* schema/table separator is always a '/'. */
  name.push_back('/');

  /* Strip the '.ibd' suffix. */
  name.append(new_name.begin() + path_sep_pos + 1, new_name.end() - 4);

  ut_ad(!Fil_path::has_suffix(IBD, name));

  const auto ptr = name.c_str();

  dberr_t err =
      fil_rename_tablespace(space_id, old_name.c_str(), ptr, new_name.c_str());

  /* Stop recovery if this does not succeed. */
  ut_a(err == DB_SUCCESS);

  return true;
}

/** Get the tablespace ID from an .ibd and/or an undo tablespace. If the ID is 0
on the first page then try finding the ID with Datafile::find_space_id().
@param[in]      filename        File name to check
@return s_invalid_space_id if not found, otherwise the space ID */
space_id_t Fil_system::get_tablespace_id(const std::string &filename) {
  FILE *fp = fopen(filename.c_str(), "rb");

  if (fp == nullptr) {
    ib::warn(ER_IB_MSG_372) << "Unable to open '" << filename << "'";
    return dict_sys_t::s_invalid_space_id;
  }

  std::vector<space_id_t> space_ids;
  auto page_size = srv_page_size;

  space_ids.reserve(MAX_PAGES_TO_READ);

  const auto n_bytes = page_size * MAX_PAGES_TO_READ;

  std::unique_ptr<byte[]> buf(new byte[n_bytes]);

  if (!buf) {
    return dict_sys_t::s_invalid_space_id;
  }

  auto pages_read = fread(buf.get(), page_size, MAX_PAGES_TO_READ, fp);

  DBUG_EXECUTE_IF("invalid_header", pages_read = 0;);

  /* Find the space id from the pages read if enough pages could be read.
  Fall back to the more heavier method of finding the space id from
  Datafile::find_space_id() if pages cannot be read properly. */
  if (pages_read >= MAX_PAGES_TO_READ) {
    auto bytes_read = pages_read * page_size;

#ifdef POSIX_FADV_DONTNEED
    posix_fadvise(fileno(fp), 0, bytes_read, POSIX_FADV_DONTNEED);
#endif /* POSIX_FADV_DONTNEED */

    for (page_no_t i = 0; i < MAX_PAGES_TO_READ; ++i) {
      const auto off = i * page_size + FIL_PAGE_SPACE_ID;

      if (off == FIL_PAGE_SPACE_ID) {
        /* Find out the page size of the tablespace from the first page.
        In case of compressed pages, the subsequent pages can be of different
        sizes. If MAX_PAGES_TO_READ is changed to a different value, then the
        page size of subsequent pages is needed to find out the offset for
        space ID. */

        auto space_flags_offset = FSP_HEADER_OFFSET + FSP_SPACE_FLAGS;

        ut_a(space_flags_offset + 4 < n_bytes);

        const auto flags = mach_read_from_4(buf.get() + space_flags_offset);

        page_size_t space_page_size(flags);

        page_size = space_page_size.physical();
      }

      space_ids.push_back(mach_read_from_4(buf.get() + off));

      if ((i + 1) * page_size >= bytes_read) {
        break;
      }
    }
  }

  fclose(fp);

  space_id_t space_id;

  if (!space_ids.empty()) {
    space_id = space_ids.front();

    for (auto id : space_ids) {
      if (id == 0 || space_id != id) {
        space_id = UINT32_UNDEFINED;

        break;
      }
    }
  } else {
    space_id = UINT32_UNDEFINED;
  }

  /* Try the more heavy duty method, as a last resort. */
  if (space_id == UINT32_UNDEFINED) {
    /* If the first page cannot be read properly, then for compressed
    tablespaces we don't know where the page boundary starts because
    we don't know the page size. */

    Datafile file;

    file.set_filepath(filename.c_str());

    dberr_t err = file.open_read_only(false);

    ut_a(file.is_open());
    ut_a(err == DB_SUCCESS);

    /* Use the heavier Datafile::find_space_id() method to
    find the space id. */
    err = file.find_space_id();

    if (err == DB_SUCCESS) {
      space_id = file.space_id();
    }

    file.close();
  }

  return space_id;
}

void Fil_system::rename_partition_files(bool revert) {
#ifndef UNIV_HOTBACKUP
  /* If revert, then we are downgrading after upgrade failure from 5.7 */
  ut_ad(!revert || srv_downgrade_partition_files);

  if (m_old_paths.empty()) {
    return;
  }

  ut_ad(!lower_case_file_system);

  for (auto &old_path : m_old_paths) {
    ut_ad(Fil_path::has_suffix(IBD, old_path));
    ut_ad(dict_name::is_partition(old_path));

    fil_rename_partition_file(old_path, IBD, revert, false);
  }
#endif /* !UNIV_HOTBACKUP */
}

void Tablespace_dirs::duplicate_check(const Const_iter &start,
                                      const Const_iter &end, size_t thread_id,
                                      std::mutex *mutex, Space_id_set *unique,
                                      Space_id_set *duplicates) {
  size_t count = 0;
  bool printed_msg = false;
  auto start_time = std::chrono::steady_clock::now();

  for (auto it = start; it != end; ++it, ++m_checked) {
    const std::string filename = it->second;
    auto &files = m_dirs[it->first];
    const std::string phy_filename = files.path() + filename;

    space_id_t space_id;

    space_id = Fil_system::get_tablespace_id(phy_filename);

    if (space_id != 0 && space_id != dict_sys_t::s_invalid_space_id) {
      std::lock_guard<std::mutex> guard(*mutex);

      auto ret = unique->insert(space_id);

      size_t n_files;

      n_files = files.add(space_id, filename);

      if (n_files > 1 || !ret.second) {
        duplicates->insert(space_id);
      }

    } else if (space_id != 0 &&
               Fil_path::is_undo_tablespace_name(phy_filename)) {
      ib::info(ER_IB_MSG_373) << "Can't determine the undo file tablespace"
                              << " ID for '" << phy_filename << "', could be"
                              << " an undo truncate in progress";

    } else {
      ib::info(ER_IB_MSG_374) << "Ignoring '" << phy_filename << "' invalid"
                              << " tablespace ID in the header";
    }

    ++count;

    if (std::chrono::steady_clock::now() - start_time >= PRINT_INTERVAL) {
      ib::info(ER_IB_MSG_375) << "Thread# " << thread_id << " - Checked "
                              << count << "/" << (end - start) << " files";

      start_time = std::chrono::steady_clock::now();

      printed_msg = true;
    }
  }

  if (printed_msg) {
    ib::info(ER_IB_MSG_376) << "Checked " << count << " files";
  }
}

/** Print the duplicate filenames for a tablespce ID to the log
@param[in]      duplicates      Duplicate tablespace IDs*/
void Tablespace_dirs::print_duplicates(const Space_id_set &duplicates) {
  /* Print the duplicate names to the error log. */
  for (auto space_id : duplicates) {
    Dirs files;

    for (auto &dir : m_dirs) {
      const auto names = dir.find_by_id(space_id);

      if (names == nullptr) {
        continue;
      }

      files.insert(files.end(), names->begin(), names->end());
    }

    /* Fixes the order in the mtr tests. */
    std::sort(files.begin(), files.end());

    ut_a(files.size() > 1);

    std::ostringstream oss;

    oss << "Tablespace ID: " << space_id << " = [";

    for (size_t i = 0; i < files.size(); ++i) {
      oss << "'" << files[i] << "'";

      if (i < files.size() - 1) {
        oss << ", ";
      }
    }

    oss << "]" << std::endl;

    ib::error(ER_IB_MSG_377) << oss.str();
  }
}

static bool fil_get_partition_file(const std::string &old_path [[maybe_unused]],
                                   ib_file_suffix extn [[maybe_unused]],
                                   std::string &new_path [[maybe_unused]]) {
  /* Safe check. Never needed on Windows. */
#ifdef _WIN32
  return false;
#else /* WIN32 */

#ifndef UNIV_HOTBACKUP
  /* Needed only for case sensitive file system. */
  if (lower_case_file_system) {
    return false;
  }

  /* Skip if not right file extension. */
  if (!Fil_path::has_suffix(extn, old_path)) {
    return false;
  }

  /* Check if partitioned table. */
  if (!dict_name::is_partition(old_path)) {
    return false;
  }

  std::string table_name;
  /* Get Innodb dictionary name from file path. */
  if (!Fil_path::parse_file_path(old_path, extn, table_name)) {
    ut_d(ut_error);
    ut_o(return false);
  }
  ut_ad(!table_name.empty());

  /* Rebuild partition table name with lower case. */
  std::string save_name(table_name);
  dict_name::rebuild(table_name);

  if (save_name.compare(table_name) == 0) {
    return false;
  }

  /* Build new partition file name. */
  new_path = Fil_path::make_new_path(old_path, table_name, extn);
  ut_ad(!new_path.empty());
#endif /* !UNIV_HOTBACKUP */

  return true;
#endif /* WIN32 */
}

#ifndef UNIV_HOTBACKUP
static void fil_rename_partition_file(const std::string &old_path,
                                      ib_file_suffix extn, bool revert,
                                      bool import) {
  std::string new_path;

  if (!fil_get_partition_file(old_path, extn, new_path)) {
    ut_d(ut_error);
    ut_o(return );
  }

  ut_ad(!new_path.empty());

  bool old_exists = os_file_exists(old_path.c_str());
  bool new_exists = os_file_exists(new_path.c_str());

  static bool print_upgrade = true;
  static bool print_downgrade = true;
  bool ret = false;

  if (revert) {
    /* Check if rename is required. */
    if (!new_exists || old_exists) {
      return;
    }
    ret = os_file_rename(innodb_data_file_key, new_path.c_str(),
                         old_path.c_str());
    ut_ad(ret);

    if (ret && print_downgrade) {
      ib::info(ER_IB_MSG_DOWNGRADE_PARTITION_FILE, new_path.c_str(),
               old_path.c_str());
      print_downgrade = false;
    }
    return;
  }

  /* Check if rename is required. */
  if (new_exists || !old_exists) {
    return;
  }

  ret =
      os_file_rename(innodb_data_file_key, old_path.c_str(), new_path.c_str());

  if (!ret) {
    /* File rename failed. */
    ut_d(ut_error);
    ut_o(return );
  }

  if (import) {
    ib::info(ER_IB_MSG_UPGRADE_PARTITION_FILE_IMPORT, old_path.c_str(),
             new_path.c_str());
    return;
  }

  if (print_upgrade) {
    ib::info(ER_IB_MSG_UPGRADE_PARTITION_FILE, old_path.c_str(),
             new_path.c_str());
    print_upgrade = false;
  }
}
#endif /* !UNIV_HOTBACKUP */

void Tablespace_dirs::set_scan_dir(const std::string &in_directory,
                                   bool is_undo_dir) {
  std::string directory(in_directory);

  Fil_path::normalize(directory);

  add_path(directory, is_undo_dir);
}

void Tablespace_dirs::set_scan_dirs(const std::string &in_directories) {
  std::string directories(in_directories);

  Fil_path::normalize(directories);

  std::string separators;

  separators.push_back(FIL_PATH_SEPARATOR);

  add_paths(directories, separators);
}

/** Discover tablespaces by reading the header from .ibd files.
@return DB_SUCCESS if all goes well */
dberr_t Tablespace_dirs::scan() {
  Scanned_files ibd_files;
  Scanned_files undo_files;
  uint16_t count = 0;
  bool print_msg = false;
  auto start_time = std::chrono::steady_clock::now();

  /* Should be trivial to parallelize the scan and ID check. */
  for (const auto &dir : m_dirs) {
    const auto real_path_dir = dir.root().abs_path();

    ut_a(Fil_path::is_separator(dir.path().back()));

    ib::info(ER_IB_MSG_379) << "Scanning '" << dir.path() << "'";

    /* Walk the sub-tree of dir. */

    Dir_Walker::walk(real_path_dir, true, [&](const std::string &path) {
      /* If it is a file and the suffix matches ".ibd"
      or the undo file name format then store it for
      determining the space ID. */

      ut_a(path.length() > real_path_dir.length());
      ut_a(Fil_path::get_file_type(path) != OS_FILE_TYPE_DIR);

      /* Check if need to alter partition file names to lower case. */
      std::string new_path;

      if (fil_get_partition_file(path, IBD, new_path)) {
        /* Note all old file names to be renamed. */
        ut_ad(!new_path.empty());
        fil_system->add_old_file(path);

      } else {
        new_path.assign(path);
      }

      /* Make the filename relative to the directory that was scanned. */
      std::string file = new_path.substr(real_path_dir.length());

      if (file.size() <= 4) {
        return;
      }

      using Value = Scanned_files::value_type;

      if (Fil_path::has_suffix(IBD, file.c_str())) {
        ibd_files.push_back(Value{count, file});

      } else if (Fil_path::is_undo_tablespace_name(file)) {
        undo_files.push_back(Value{count, file});
      }

      if (std::chrono::steady_clock::now() - start_time >= PRINT_INTERVAL) {
        ib::info(ER_IB_MSG_380)
            << "Files found so far: " << ibd_files.size() << " data files"
            << " and " << undo_files.size() << " undo files";

        start_time = std::chrono::steady_clock::now();
        print_msg = true;
      }
    });

    ++count;
  }

  /* Rename all old partition files. */
  fil_system->rename_partition_files(false);

  if (print_msg) {
    ib::info(ER_IB_MSG_381) << "Found " << ibd_files.size() << " '.ibd' and "
                            << undo_files.size() << " undo files";
  }

  Space_id_set unique;
  Space_id_set duplicates;

  /* Get the number of additional threads needed to scan the files. */
  size_t n_threads = fil_get_scan_threads(ibd_files.size());

  if (n_threads > 0) {
    ib::info(ER_IB_MSG_382)
        << "Using " << (n_threads + 1) << " threads to"
        << " scan " << ibd_files.size() << " tablespace files";
  }

  std::mutex m;

  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;
  using std::placeholders::_4;
  using std::placeholders::_5;
  using std::placeholders::_6;

  std::function<void(const Const_iter &, const Const_iter &, size_t,
                     std::mutex *, Space_id_set *, Space_id_set *)>
      check = std::bind(&Tablespace_dirs::duplicate_check, this, _1, _2, _3, _4,
                        _5, _6);

  par_for(PFS_NOT_INSTRUMENTED, ibd_files, n_threads, check, &m, &unique,
          &duplicates);

  duplicate_check(undo_files.begin(), undo_files.end(), n_threads, &m, &unique,
                  &duplicates);

  ut_a(m_checked == ibd_files.size() + undo_files.size());

  ib::info(ER_IB_MSG_383) << "Completed space ID check of " << m_checked.load()
                          << " files.";

  dberr_t err;

  if (!duplicates.empty()) {
    ib::error(ER_IB_MSG_384)
        << "Multiple files found for the same tablespace ID:";

    print_duplicates(duplicates);

    err = DB_FAIL;
  } else {
    err = DB_SUCCESS;
  }

  return err;
}

void fil_set_scan_dir(const std::string &directory, bool is_undo_dir) {
  fil_system->set_scan_dir(directory, is_undo_dir);
}

void fil_set_scan_dirs(const std::string &directories) {
  fil_system->set_scan_dirs(directories);
}

/** Discover tablespaces by reading the header from .ibd files.
@return DB_SUCCESS if all goes well */
dberr_t fil_scan_for_tablespaces() { return fil_system->scan(); }

/** Check if a path is known to InnoDB meaning that it is in or under
one of the four path settings scanned at startup for file discovery.
@param[in]  path    Path to check
@return true if path is known to InnoDB */
bool fil_path_is_known(const std::string &path) {
  return fil_system->check_path(path);
}

/** Get the list of directories that datafiles can reside in.
@return the list of directories 'dir1;dir2;....;dirN' */
std::string fil_get_dirs() { return fil_system->get_dirs(); }

/** Free the data structures required for recovery. */
void fil_free_scanned_files() { fil_system->free_scanned_files(); }

/** Update the tablespace name. In case, the new name
and old name are same, no update done.
@param[in,out]  space           tablespace object on which name
                                will be updated
@param[in]      name            new name for tablespace */
void fil_space_update_name(fil_space_t *space, const char *name) {
  if (space == nullptr || name == nullptr || space->name == nullptr ||
      strcmp(space->name, name) == 0) {
    return;
  }

  dberr_t err = fil_rename_tablespace_by_id(space->id, space->name, name);

  if (err != DB_SUCCESS) {
    ib::warn(ER_IB_MSG_387) << "Tablespace rename '" << space->name << "' to"
                            << " '" << name << "' failed!";
  }
}

#ifndef UNIV_HOTBACKUP
bool Fil_path::is_valid_location(const char *space_name, space_id_t space_id,
                                 uint32_t fsp_flags, const std::string &path) {
  ut_ad(!path.empty());
  ut_ad(space_name != nullptr);

  /* All files sent to this routine have been found by scanning known
  locations. */
  ib_file_suffix type = (fsp_is_undo_tablespace(space_id) ? IBU : IBD);

  if (type == IBD) {
    size_t dirname_len = dirname_length(path.c_str());
    Fil_path dirpath(path.c_str(), dirname_len, true);

    bool is_shared = fsp_is_shared_tablespace(fsp_flags);
    bool under_datadir = MySQL_datadir_path.is_ancestor(dirpath);

    if (is_shared) {
      if (under_datadir) {
        ib::error(ER_IB_MSG_GENERAL_TABLESPACE_UNDER_DATADIR, path.c_str());
        return false;
      }
    } else {
      /* file-per-table */
      bool in_datadir =
          (under_datadir ? false : MySQL_datadir_path.is_same_as(dirpath));

      if (in_datadir) {
        ib::error(ER_IB_MSG_IMPLICIT_TABLESPACE_IN_DATADIR, path.c_str());
        return false;
      }

      /* Make sure that the last directory of an implicit tablespace is a
      filesystem charset version of the schema name. */
      if (!is_valid_location_within_db(space_name, path)) {
        ib::error(ER_IB_MSG_INVALID_LOCATION_WRONG_DB, path.c_str(),
                  space_name);
        return false;
      }
    }
  }

  return true;
}

bool Fil_path::is_valid_location_within_db(const char *space_name,
                                           const std::string &path) {
  /* Strip off the basename to reduce the path to a directory. */
  std::string dirpath{path};
  auto pos = dirpath.find_last_of(SEPARATOR);
  dirpath.resize(pos);

  /* Only implicit tablespaces are sent to this routine.
  They are always prefixed by `schema/`. */
  ut_ad(pos != std::string::npos);

  /* Get the subdir that the file is in. */
  pos = dirpath.find_last_of(SEPARATOR);
  std::string db_dir = (pos == std::string::npos)
                           ? dirpath
                           : dirpath.substr(pos + 1, dirpath.length());

  /* Convert to lowercase if necessary. */
  if (innobase_get_lower_case_table_names() == 2) {
    Fil_path::convert_to_lower_case(db_dir);
  }

  /* Make sure the db_dir matches the schema name.
  db_dir is in filesystem charset and space_name is usually in the
  system charset.

  The problem here is that the system charset version of a schema or
  table name may contain a '/' and the tablespace name we were sent
  is a combination of the two with '/' as a delimiter.
  For example `my/schema` + `my/table` == `my/schema/my/table`

  Search the space_name string backwards until we find the db name that
  matches the schema name from the path. */

  std::string name(space_name);
  pos = name.find_last_of(SEPARATOR);
  while (pos < std::string::npos) {
    name.resize(pos);
    std::string temp = name;
    if (temp == db_dir) {
      return true;
    }

    /* Convert to filename charset and compare again. */
    Fil_path::convert_to_filename_charset(temp);
    if (temp == db_dir) {
      return true;
    }

    /* Still no match, iterate through the next SEPARATOR. */
    pos = name.find_last_of(SEPARATOR);

    /* If end of string is hit, there is no match. */
    if (pos == std::string::npos) {
      return false;
    }
  }

  return true;
}

/** Convert filename to the file system charset format.
@param[in,out]  name            Filename to convert */
void Fil_path::convert_to_filename_charset(std::string &name) {
  uint errors = 0;
  char old_name[MAX_TABLE_NAME_LEN + 20];
  char filename[MAX_TABLE_NAME_LEN + 20];

  strncpy(filename, name.c_str(), sizeof(filename) - 1);
  strncpy(old_name, filename, sizeof(old_name));

  innobase_convert_to_filename_charset(filename, old_name, MAX_TABLE_NAME_LEN);

  if (errors == 0) {
    name.assign(filename);
  }
}

/** Convert to lower case using the file system charset.
@param[in,out]  path            Filepath to convert */
void Fil_path::convert_to_lower_case(std::string &path) {
  char lc_path[MAX_TABLE_NAME_LEN + 20];

  ut_ad(path.length() < sizeof(lc_path) - 1);

  strncpy(lc_path, path.c_str(), sizeof(lc_path) - 1);

  innobase_casedn_path(lc_path);

  path.assign(lc_path);
}

void fil_purge() { fil_system->purge(); }

size_t fil_count_undo_deleted(space_id_t undo_num) {
  return fil_system->count_undo_deleted(undo_num);
}

#endif /* !UNIV_HOTBACKUP */

#define PAGE_TYPE(x) \
  case x:            \
    return #x;

const char *fil_get_page_type_str(page_type_t type) noexcept {
  switch (type) {
    PAGE_TYPE(FIL_PAGE_INDEX);
    PAGE_TYPE(FIL_PAGE_RTREE);
    PAGE_TYPE(FIL_PAGE_SDI);
    PAGE_TYPE(FIL_PAGE_UNDO_LOG);
    PAGE_TYPE(FIL_PAGE_INODE);
    PAGE_TYPE(FIL_PAGE_IBUF_FREE_LIST);
    PAGE_TYPE(FIL_PAGE_TYPE_ALLOCATED);
    PAGE_TYPE(FIL_PAGE_IBUF_BITMAP);
    PAGE_TYPE(FIL_PAGE_TYPE_SYS);
    PAGE_TYPE(FIL_PAGE_TYPE_TRX_SYS);
    PAGE_TYPE(FIL_PAGE_TYPE_FSP_HDR);
    PAGE_TYPE(FIL_PAGE_TYPE_XDES);
    PAGE_TYPE(FIL_PAGE_TYPE_BLOB);
    PAGE_TYPE(FIL_PAGE_TYPE_ZBLOB);
    PAGE_TYPE(FIL_PAGE_TYPE_ZBLOB2);
    PAGE_TYPE(FIL_PAGE_TYPE_UNKNOWN);
    PAGE_TYPE(FIL_PAGE_COMPRESSED);
    PAGE_TYPE(FIL_PAGE_ENCRYPTED);
    PAGE_TYPE(FIL_PAGE_COMPRESSED_AND_ENCRYPTED);
    PAGE_TYPE(FIL_PAGE_ENCRYPTED_RTREE);
    PAGE_TYPE(FIL_PAGE_SDI_BLOB);
    PAGE_TYPE(FIL_PAGE_SDI_ZBLOB);
    PAGE_TYPE(FIL_PAGE_TYPE_LOB_INDEX);
    PAGE_TYPE(FIL_PAGE_TYPE_LOB_DATA);
    PAGE_TYPE(FIL_PAGE_TYPE_LOB_FIRST);
    PAGE_TYPE(FIL_PAGE_TYPE_ZLOB_FIRST);
    PAGE_TYPE(FIL_PAGE_TYPE_ZLOB_DATA);
    PAGE_TYPE(FIL_PAGE_TYPE_ZLOB_INDEX);
    PAGE_TYPE(FIL_PAGE_TYPE_ZLOB_FRAG);
    PAGE_TYPE(FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY);
    PAGE_TYPE(FIL_PAGE_TYPE_RSEG_ARRAY);
    PAGE_TYPE(FIL_PAGE_TYPE_LEGACY_DBLWR);
  }
  ut_d(ut_error);
  ut_o(return "UNKNOWN");
}

bool fil_is_page_type_valid(page_type_t type) noexcept {
  if (fil_page_type_is_index(type)) {
    return true;
  }

  if (type <= FIL_PAGE_TYPE_LAST && type != FIL_PAGE_TYPE_UNUSED) {
    return true;
  }

  ut_d(ut_error);
  ut_o(return false);
}

std::ostream &Fil_page_header::print(std::ostream &out) const noexcept {
  /* Print the header information in the order it is stored. */
  out << "[Fil_page_header: FIL_PAGE_OFFSET=" << get_page_no()
      << ", FIL_PAGE_TYPE=" << get_page_type()
      << ", FIL_PAGE_SPACE_ID=" << get_space_id() << "]";
  return out;
}

space_id_t Fil_page_header::get_space_id() const noexcept {
  return mach_read_from_4(m_frame + FIL_PAGE_SPACE_ID);
}

page_no_t Fil_page_header::get_page_no() const noexcept {
  return mach_read_from_4(m_frame + FIL_PAGE_OFFSET);
}

uint16_t Fil_page_header::get_page_type() const noexcept {
  return mach_read_from_2(m_frame + FIL_PAGE_TYPE);
}

fil_node_t *fil_space_t::get_file_node(page_no_t *page_no) noexcept {
  if (files.size() > 1) {
    ut_a(id == TRX_SYS_SPACE || purpose == FIL_TYPE_TEMPORARY);

    for (auto &f : files) {
      if (f.size > *page_no) {
        return &f;
      }
      *page_no -= f.size;
    }

  } else if (!files.empty()) {
    fil_node_t &f = files.front();

    if ((fsp_is_ibd_tablespace(id) && f.size == 0) || f.size > *page_no) {
      /* We do not know the size of a single-table tablespace
      before we open the file */
      return &f;
    }
    /* The page is outside the current bounds of the file. We should not assert
    here as we could be loading pages in buffer pool from dump file having pages
    from dropped tablespaces. Specifically, for undo tablespace it is possible
    to re-use the dropped space ID and the page could be out of bound. We need
    to ignore such cases. */
  }

  return nullptr;
}

bool fil_space_t::is_deleted() const {
  ut_ad(fil_system->shard_by_id(id)->mutex_owned());
  return m_deleted;
}

bool fil_space_t::was_not_deleted() const {
  /* This is not a critical assertion - if you have this mutex, then possibly
  you want to call !is_deleted(). */
  ut_ad(!fil_system->shard_by_id(id)->mutex_owned());
  return !m_deleted;
}

#ifndef UNIV_HOTBACKUP
uint32_t fil_space_t::get_current_version() const {
  ut_ad(fil_system->shard_by_id(id)->mutex_owned());
  return m_version;
}
uint32_t fil_space_t::get_recent_version() const {
  /* This is not a critical assertion - if you have this mutex, then possibly
  you want to call get_current_version(). */
  ut_ad(!fil_system->shard_by_id(id)->mutex_owned());
  return m_version;
}
bool fil_space_t::has_no_references() const {
  /* To assure the ref count can't be increased, we must either operate on
  detached space that is ready to be removed, or have the fil shard latched. */
#ifdef UNIV_DEBUG
  if (!fil_system->shard_by_id(id)->mutex_owned()) {
    ut_a(fil_space_get(id) != this);
  }
#endif
  return m_n_ref_count.load() == 0;
}
size_t fil_space_t::get_reference_count() const {
  /* This should be only called on server shutdown. */
  ut_ad(fil_system->shard_by_id(id)->mutex_owned());
  return m_n_ref_count.load();
}

#endif /* !UNIV_HOTBACKUP */

void fil_space_t::set_deleted() {
  ut_ad(fil_system->shard_by_id(id)->mutex_owned());
  ut_a(files.size() == 1);
  ut_a(n_pending_ops == 0);

#ifndef UNIV_HOTBACKUP
  bump_version();

  m_deleted = true;
#endif /* !UNIV_HOTBACKUP */
}

#ifndef UNIV_HOTBACKUP

void fil_space_t::bump_version() {
  ut_ad(fil_system->shard_by_id(id)->mutex_owned());
  ut_a(files.size() == 1);
  ut_a(n_pending_ops == 0);

  /* Bump the version. This will make all pages in buffer pool that reference
  the current space version to be stale and freed on first encounter. */
  ut_a(stop_new_ops);
  ut_a(!m_deleted);

  ++m_version;
}
#endif /* !UNIV_HOTBACKUP */
