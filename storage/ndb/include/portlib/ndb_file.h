/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_FILE_H
#define NDB_FILE_H

#include <stdio.h>  // ONLY TEMPORARY FOR DEBUG!

#include <atomic>
#include <climits>
#include <cstdint>
#include <cstdio>   // fprintf
#include <cstdlib>  // abort
#include "ndb_types.h"

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include "Windows.h"
#endif

/**
 * ndb_file - portable file abstraction for use in NDBFS and NDB BACKUP
 *
 * Note this is not an abstraction for generic file like access.
 *
 * It is assumed that a regular file is accessed by one single process at a
 * time using blocking I/O.
 *
 * For example using this class for named pipes, sockets, or other non regular
 * file like objects may have surprising results.  And the best approach is
 * probably to have specific classes for these usages.
 *
 * Functions used during creation and initialization of a file are broken up
 * in smaller parts even if underlying OS typically can combine several of
 * them in one system call.  This simplifies detection of exactly what failed
 * and simplifies the recovering of failure.  And the number of system calls
 * are typically not important in this case.
 *
 * For read and write operations implicit calls to sync may occur, and since
 * sync failures is not in general guaranteed to be able to retry and ensuring
 * old writes will eventually be synced these failures should be treated as
 * fatal failures for the file and file content should be regarded as
 * inconsistent.
 *
 *
 * A typical life time for a file
 *
 * Initialization phase
 * --------------------
 *
 * create(name) - Note, only creates an empty file, leavs no handle open.
 * open(name, read-write-append flags)
 *
 * extend() or truncate() - Set the initial file size.
 * allocate() - May be used to reserve disk blocks for entire file.
 *
 * set_block_size_and_alignment(size, align)
 * - tells class what size and alignment caller will use on memory blocks to
 *   read and write functions, alignment also restricts the file positions for
 *   read and write.
 *
 * Initialize file by calling append and write functions.
 *
 * set_direct_io()
 * - if by passing OS caching is considered an optimization.
 *   This will also check that block size and alignment requirements for
 *   direct io is satisfied by the block size and alignment set by
 *   application.
 *
 * reopen_with_sync(name)
 * - Turning on sync mode after initialization at least on Linux requires
 *   reopening the file.
 *
 * set_autosync()
 * - for files not opened in sync mode, make sure to flush out outstanding
 *   writes automatically, not for consistency but to not build up large use
 *   of file buffers while nothing is written to disk.
 *
 * Note, currently one can not open a file in sync mode. A valid encrypted file
 * need both a header and a trailer, keeping a valid trailer updated in append
 * mode will be tricky, it should be possible to do but Ndb does not need that
 * functionality now. For fixed sized files one typically have an
 * initialization phase that does not gain from having each write synced, and
 * using reopen_with_sync() after initialization is good enough for Ndb.
 *
 * Usage phase
 * -----------
 *
 * Calls to append(), write_forward(), write_pos(), read_forward(),
 * read_backward(), read_pos().
 *
 * write_pos() and read_pos() may be called from different threads in parallel.
 *
 * Close and cleanup phase
 * -----------------------
 *
 * sync()
 *
 * close()
 *
 * remove(name)
 *
 */

class ndb_file {
 public:
  using byte = uint8_t;
  using size_t = uint64_t;
#ifndef _WIN32
  /*
   * On POSIX like system some system functions like lseek is sometimes called
   * with ndb_off_t put takes and returns off_t, make sure off_t is of same
   * size.
   *
   * On Windows off_t is 32-bit and 64-bit alternatives to lseek will be used.
   */
  static_assert(sizeof(ndb_off_t) == sizeof(::off_t));
#endif

#ifndef _WIN32
  using os_handle = int;
  static constexpr os_handle os_invalid_handle = -1;
#else
  // At least Visual Studio silently put static constexpr HANDLE to zero,
  // casting to intptr_t works ...
  using os_handle = HANDLE;
  static const os_handle os_invalid_handle;
#endif

  enum extend_flags { NO_FILL, ZERO_FILL };

  ndb_file();
  ~ndb_file();
  // Do not allow copying, that will cause problem with double close.
  ndb_file(const ndb_file &) = delete;
  ndb_file &operator=(const ndb_file &) = delete;

  static int create(const char name[]);
  static int remove(const char name[]);

  // Valid flags are FsOpenReq::OM_READONLY,OM_READWRITE,OM_WRITEONLY,OM_APPEND
  int open(const char name[], unsigned flags);
  int reopen_with_sync(const char name[]);
  int close();

  /*
   * extend and truncate may change file pointer.
   * extend may partially succeed.
   */
  int extend(ndb_off_t end, extend_flags flags) const;
  int truncate(ndb_off_t end) const;

  /*
   * Reserve disk blocks for entire file.
   */
  int allocate() const;

  int set_block_size_and_alignment(size_t size, size_t alignment);
  bool have_direct_io_support() const;
  /*
   * On Solaris directio should not be used during for example initialization
   * of files there one writes a lot of pages in sequence.
   */
  bool avoid_direct_io_on_append() const;
  int set_direct_io(bool assume_implicit_datasync);
  int set_autosync(size_t size);

  /*
   * Does a file synchronization if there have been writes since last sync.
   * That is, changes only in read access times may not be synced.
   * TODO: ensure effects of extend(), truncate(), and, allocate() are synced.
   */
  int sync();

  bool is_open() const;

  ndb_off_t get_pos() const;
  int set_pos(ndb_off_t pos) const;
  ndb_off_t get_size() const;

  size_t get_block_size() const;
  size_t get_block_alignment() const;

  size_t get_direct_io_block_alignment() const;
  size_t get_direct_io_block_size() const;

  // Functions needed as long as ndbzdopen is used.
  os_handle get_os_handle() const;
  void invalidate();

  // stream interface

  /*
   * For blocking disk file I/O all operations should either fail or read or
   * write the full count.
   * Only at end of file read may return a partial count.
   * And if write operation fail due to sync failure the file state is
   * unspecified, some part of buffer may have been written.
   *
   * On Posix operations are retried internally if EINTR is encountered.
   */
  int append(const void *buf, size_t count);
  int write_forward(const void *buf, size_t count);
  int write_pos(const void *buf, size_t count, ndb_off_t offset);
  int read_forward(void *buf, size_t count) const;
  int read_backward(void *buf, size_t count) const;
  int read_pos(void *buf, size_t count, ndb_off_t offset) const;

 private:
  void init();  // reset all data members
  int do_sync() const;
  int detect_direct_io_block_size_and_alignment();
  bool check_block_size_and_alignment(const void *buf, size_t count,
                                      ndb_off_t offset) const;
  bool check_is_regular_file() const;
  bool is_regular_file() const;
  int do_sync_after_write(size_t written_bytes);

  os_handle m_handle;
  int m_open_flags;
  bool m_write_need_sync;
  bool m_os_syncs_each_write;
  size_t m_block_size;
  size_t m_block_alignment;
  size_t m_direct_io_block_size;
  size_t m_direct_io_block_alignment;
  size_t m_autosync_period;
  std::atomic<size_t> m_write_byte_count;  // writes since last sync
};

inline bool ndb_file::is_open() const { return m_handle != os_invalid_handle; }

inline ndb_file::os_handle ndb_file::get_os_handle() const { return m_handle; }

inline ndb_file::size_t ndb_file::get_direct_io_block_alignment() const {
  return m_direct_io_block_alignment;
}

inline ndb_file::size_t ndb_file::get_direct_io_block_size() const {
  return m_direct_io_block_size;
}

inline int ndb_file::set_block_size_and_alignment(size_t size, size_t align) {
  if (align == 0 || size == 0 || size % align != 0) {
    // size must be a multiple of alignment.
    return -1;
  }

  m_block_size = size;
  m_block_alignment = align;
  return 0;
}

inline ndb_file::size_t ndb_file::get_block_size() const {
  return m_block_size;
}

inline ndb_file::size_t ndb_file::get_block_alignment() const {
  return m_block_alignment;
}

inline bool ndb_file::check_block_size_and_alignment(const void *buf,
                                                     size_t count,
                                                     ndb_off_t offset) const {
  if (m_block_size == 0) return true;

  uintptr_t size_mask = -1 + (uintptr_t)m_block_size;
  uintptr_t align_mask = -1 + (uintptr_t)m_block_alignment;

  if (((uintptr_t)buf & align_mask) || (offset & size_mask) ||
      (count & align_mask)) {
    return false;
  }

  return true;
}

#endif
