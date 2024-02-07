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

#include "ndb_config.h"  // HAVE_POSIX_FALLOCATE, HAVE_XFS_XFS_H
#include "util/require.h"

#include "kernel/signaldata/FsOpenReq.hpp"
#include "portlib/ndb_file.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_XFS_XFS_H
#include <xfs/xfs.h>
#endif

#ifndef require
template <const char *cond_str, const char *file, const char *func, int line>
static inline void require_fn(bool cond) {
  if (cond) return;
  g_eventLogger->info("YYY: FATAL ERROR: %s: %s: %d: REQUIRE FAILED: %s", file,
                      func, line, cond_str);
  std::abort();
}
#define require(cc) require_fn<#cc, __FILE__, __func__, __LINE__>((cc))
#endif

bool ndb_file::is_regular_file() const {
  struct stat st;
  if (fstat(m_handle, &st) == 0) {
    return ((st.st_mode & S_IFMT) == S_IFREG);
  }
  return false;
}

bool ndb_file::check_is_regular_file() const {
#if defined(VM_TRACE) || !defined(NDEBUG) || defined(ERROR_INSERT)
  if (!is_open()) return true;
  struct stat sb;
  if (fstat(m_handle, &sb) == -1) return true;
  if ((sb.st_mode & S_IFMT) == S_IFREG) return true;
  fprintf(
      stderr,
      "FATAL ERROR: %s: %u: Handle is not a regular file: fd=%d file type=%o\n",
      __func__, __LINE__, m_handle, sb.st_mode & S_IFMT);
  return false;
#else
  return true;
#endif
}

int ndb_file::write_forward(const void *buf, ndb_file::size_t count) {
  require(check_is_regular_file());
  require(check_block_size_and_alignment(buf, count, get_pos()));
  int ret;
  do {
    ret = ::write(m_handle, buf, count);
  } while (ret == -1 && errno == EINTR);
  if (ret >= 0) {
    assert(ndb_file::size_t(ret) == count);
    if (do_sync_after_write(ret) == -1) return -1;
  }
  return ret;
}

int ndb_file::write_pos(const void *buf, ndb_file::size_t count,
                        ndb_off_t offset) {
  require(check_is_regular_file());
  require(check_block_size_and_alignment(buf, count, offset));
  int ret;
  do {
    ret = ::pwrite(m_handle, buf, count, offset);
  } while (ret == -1 && errno == EINTR);
  if (ret >= 0) {
    assert(ndb_file::size_t(ret) == count);
    if (do_sync_after_write(ret) == -1) return -1;
  }
  return ret;
}

int ndb_file::read_forward(void *buf, ndb_file::size_t count) const {
  require(check_is_regular_file());
  require(check_block_size_and_alignment(buf, count, 1));
  int ret;
  do {
    ret = ::read(m_handle, buf, count);
  } while (ret == -1 && errno == EINTR);
  return ret;
}
int ndb_file::read_backward(void *buf, ndb_file::size_t count) const {
  require(check_is_regular_file());
  require(check_block_size_and_alignment(buf, count, 1));
  // Current pos must be within file.
  // Current pos - count must be within file.
  // Seek -count, read should read all.
  // if partial read - fatal error!
  errno = 0;
  const off_t off_count = (off_t)count;
  if (off_count < 0 || std::uintmax_t{count} != std::uintmax_t(off_count)) {
    errno = EOVERFLOW;
    return -1;
  }
  ndb_off_t offset = ::lseek(m_handle, -off_count, SEEK_CUR);
  if (offset < 0) {
    if (errno != 0) return -1;
    std::abort();
  }
  ssize_t ret;
  do {
    ret = ::read(m_handle, buf, count);
  } while (ret == -1 && errno == EINTR);
  if (ret >= 0 && ret != off_count) {
    return -1;
  }
  offset = ::lseek(m_handle, -off_count, SEEK_CUR);
  if (offset < 0) {
    if (errno != 0) return -1;
    std::abort();
  }
  return ret;
}
int ndb_file::read_pos(void *buf, ndb_file::size_t count,
                       ndb_off_t offset) const {
  require(check_is_regular_file());
  require(check_block_size_and_alignment(buf, count, offset));
  int ret;
  do {
    ret = ::pread(m_handle, buf, count, offset);
  } while (ret == -1 && errno == EINTR);
  return ret;
}

ndb_off_t ndb_file::get_pos() const { return ::lseek(m_handle, 0, SEEK_CUR); }

int ndb_file::set_pos(ndb_off_t pos) const {
  require(check_block_size_and_alignment(nullptr, 0, pos));
  ndb_off_t ret = ::lseek(m_handle, pos, SEEK_SET);
  if (ret == -1) return -1;
  require(ret == pos);
  return 0;
}

ndb_off_t ndb_file::get_size() const {
  struct stat st;
  int ret = ::fstat(m_handle, &st);
  if (ret == -1) return ret;
  return st.st_size;
}

int ndb_file::extend(ndb_off_t end, extend_flags flags) const {
  require(check_block_size_and_alignment(nullptr, end, end));
  require((flags == NO_FILL) || (flags == ZERO_FILL));
  const ndb_off_t size = get_size();
  if (size == -1) {
    return -1;
  }
  if (size > end) {
    // For shrinking use truncate instead.
    errno = EINVAL;
    return -1;
  }
  /*
   * ftruncate() zero fill for "free" even if flags is NO_FILL.
   * Zero fill is typically lazy, previously untouched blocks will be zero
   * filled on first access transparently.
   */
  if (::ftruncate(m_handle, end) == -1) {
    return -1;
  }
  return 0;
}

int ndb_file::truncate(ndb_off_t end) const {
  require(check_block_size_and_alignment(nullptr, end, end));
  ndb_off_t size = get_size();
  if (size == -1) {
    return -1;
  }
  if (size < end) {
    // For extending file use extend instead.
    errno = EINVAL;
    return -1;
  }
  if (::ftruncate(m_handle, end) == -1) {
    return -1;
  }
  return 0;
}

int ndb_file::allocate() const {
  ndb_off_t size = get_size();
  if (size == -1) {
    return -1;
  }
#ifdef HAVE_XFS_XFS_H
  if (::platform_test_xfs_fd(m_handle)) {
    std::printf("Using xfsctl(XFS_IOC_RESVSP64) to allocate disk space");
    xfs_flock64_t fl;
    fl.l_whence = 0;
    fl.l_start = 0;
    fl.l_len = (ndb_off_t)size;
    if (::xfsctl(NULL, m_handle, XFS_IOC_RESVSP64, &fl) < 0) {
      std::printf("failed to optimally allocate disk space");
      return -1;
    }
    return 0;
  }
#endif
#ifdef HAVE_POSIX_FALLOCATE
  return ::posix_fallocate(m_handle, 0, size);
#else
  errno = ENOSPC;
  return -1;
#endif
}

int ndb_file::do_sync() const {
  int r;
  do {
    r = ::fsync(m_handle);
  } while (r == -1 && errno == EINTR);
  return r;
}

/*
 * On Linux open(O_CREAT | O_DIRECT) can create a file and leave it behind even
 * if call fail due to O_DIRECT not supported on file system.
 *
 * It is chosen to separate create() and open() instead, create() fails if
 * there is already a file.
 */
int ndb_file::create(const char name[]) {
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int fd = ::open(name, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, mode);
  if (fd == -1) {
    return -1;
  }
  ::close(fd);
  return 0;
}

int ndb_file::remove(const char name[]) { return ::unlink(name); }

int ndb_file::open(const char name[], unsigned flags) {
  require(!is_open());

  init();

  const unsigned bad_flags =
      flags & ~(FsOpenReq::OM_APPEND | FsOpenReq::OM_READ_WRITE_MASK);

  if (bad_flags != 0) abort();

  m_open_flags = 0;
  m_write_need_sync = false;
  m_os_syncs_each_write = false;

  if (flags & FsOpenReq::OM_APPEND) m_open_flags |= O_APPEND;
  switch (flags & FsOpenReq::OM_READ_WRITE_MASK) {
    case FsOpenReq::OM_READONLY:
      m_open_flags |= O_RDONLY;
      break;
    case FsOpenReq::OM_WRITEONLY:
      m_open_flags |= O_WRONLY;
      break;
    case FsOpenReq::OM_READWRITE:
      m_open_flags |= O_RDWR;
      break;
    default:
      errno = EINVAL;
      return -1;
  }

  m_handle = ::open(name, m_open_flags, 0);
  if (m_handle == -1) {
    return -1;
  }

  return 0;
}

int ndb_file::close() {
  int ret = ::close(m_handle);
  m_handle = -1;
  return ret;
}

void ndb_file::invalidate() { m_handle = -1; }

bool ndb_file::have_direct_io_support() const {
#if defined(O_DIRECT) || (defined(HAVE_DIRECTIO) && defined(DIRECTIO_ON))
  return true;
#else
  return false;
#endif
}

bool ndb_file::avoid_direct_io_on_append() const {
#if (defined(HAVE_DIRECTIO) && defined(DIRECTIO_ON))
  return true;
#else
  return false;
#endif
}

int ndb_file::set_direct_io(bool assume_implicit_datasync) {
#if defined(O_DIRECT)
  int flags = 0;
  int ret = ::fcntl(m_handle, F_GETFL);
  if (ret != -1) {
    flags = ret;
    ret = ::fcntl(m_handle, F_SETFL, flags | O_DIRECT);
  }
#elif defined(HAVE_DIRECTIO) && defined(DIRECTIO_ON)
  int ret = ::directio(m_handle, DIRECTIO_ON);
#else
  int ret = -1;
#endif
  if (ret == -1) {
    return -1;
  }

  ret = detect_direct_io_block_size_and_alignment();
  if ((ret == -1) ||
#ifdef BUG32198728
      /*
       * Disabled check, see Bug#32198728.
       *
       * The i/o block size from fstat() gives a "preferred" block size.
       * Reading or writing smaller blocks may be suboptimal and may cause
       * notable worse performance.
       * Still the i/o should not fail due to too small block size is used.
       * For NFS mounted devices block size 1MiB have been seen in test there
       * this check made that visible by stopping node.
       *
       * Until the check is configurable in some way we disable it such that it
       * does not cause node failure in production.
       * The check was added recently in 8.0.22.
       */
      (m_block_size < m_direct_io_block_size) ||
      (m_block_size % m_direct_io_block_size != 0) ||
#endif
      (m_block_alignment < m_direct_io_block_alignment) ||
      (m_block_alignment % m_direct_io_block_alignment != 0)) {
#if defined(O_DIRECT)
    do {
      ret = ::fcntl(m_handle, F_SETFL, flags);
    } while (ret == -1 && errno == EINTR);
#elif defined(HAVE_DIRECTIO) && defined(DIRECTIO_ON)
    do {
      ret = ::directio(m_handle, DIRECTIO_OFF);
    } while (ret == -1 && errno == EINTR);
#else
    abort();
#endif
    // If O_DIRECT could not be reverted fail process.
    require(ret == 0);
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    // If we manage to set direct io it should not fail due to bad alignment
    abort();
#endif
    return -1;
  }

  /*
   * The user have set ODirectSyncFlag in the configuration.
   * We allow this to be used for files that are fixed in
   * size after receiving FSOPENCONF. This is true for
   * REDO log files, it is also true for tablespaces and
   * UNDO log files. There is however a flag for REDO log
   * files to set it to InitFragmentLogFiles=sparse, in this
   * case the file isn't fully allocated and thus file system
   * metadata have to be written as part of normal writes.
   *
   * At least XFS does not write metadata even when O_DIRECT
   * is set. Since XFS is our recommended file system we do
   * not support setting ODirectSyncFlag AND
   * InitFragmentLogFiles=sparse. If so we will ignore the
   * ODirectSyncFlag with a warning written to the node log.
   *
   * See e.g. BUG#45892 for a discussion about the same
   * flag in InnoDB (O_DIRECT_NO_FSYNC).
   *
   * We will only ever set this flag if O_DIRECT is
   * successfully applied on the file. This flag will not
   * change anything on block code. The blocks are still
   * expected to issue sync flags at the same places as
   * before, but if this flag is supported, the fsync
   * call will be skipped.
   */

  m_os_syncs_each_write |= assume_implicit_datasync;
  return 0;
}

alignas(2 * NDB_O_DIRECT_WRITE_ALIGNMENT) static char detect_directio_buffer
    [2 * NDB_O_DIRECT_WRITE_ALIGNMENT];

int ndb_file::detect_direct_io_block_size_and_alignment() {
  char *end = detect_directio_buffer + sizeof(detect_directio_buffer);
  int ret = -1;

  struct stat sb;
  if (::fstat(m_handle, &sb) == -1) {
    return -1;
  }
  const int block_size = sb.st_blksize;

  constexpr int align = NDB_O_DIRECT_WRITE_ALIGNMENT;

  if ((block_size % NDB_O_DIRECT_WRITE_ALIGNMENT) != 0) {
    // block size must be a multiple of alignment.
    return -1;
  }

  /*
   * Verify that NDB_O_DIRECT_WRITE_ALIGNMENT is a valid alignment both for
   * memory buffer and file offset.
   * And also a valid block size.
   */
  ret = ::pread(m_handle, end - align, align, NDB_O_DIRECT_WRITE_ALIGNMENT);
  if (ret == -1 && errno == EBADF) {
    // TODO YYY: assume EBADF means file is not open for read, for debugging
    // assume direct io is ok
    m_direct_io_block_size = block_size;
    m_direct_io_block_alignment = align;
    return 0;
  }
  if (ret == -1) {
    return -1;
  }

  m_direct_io_block_size = block_size;
  m_direct_io_block_alignment = align;

  return 0;
}

int ndb_file::reopen_with_sync(const char name[]) {
  if (m_os_syncs_each_write) {
    /*
     * If already synced on write by for example implicit by direct I/O mode no
     * further action needed.
     */
    return 0;
  }

#ifdef O_SYNC
  int flags = ::fcntl(m_handle, F_GETFL);
  if (flags != -1) {
    int new_flags = flags | O_SYNC | O_CLOEXEC;
    int fd = ::open(name, new_flags, 0);
    if (fd != -1) {
      ::close(m_handle);
      m_handle = fd;
      m_os_syncs_each_write = true;
      return 0;
    }
  }
#endif

  // If turning on O_SYNC failed fall back on explicit fsync
  m_write_need_sync = true;

  return 0;
}
