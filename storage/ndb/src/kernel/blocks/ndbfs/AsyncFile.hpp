/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef AsyncFile_H
#define AsyncFile_H

#include <atomic>

#include <kernel_types.h>
#include "AsyncIoThread.hpp"
#include "Filename.hpp"
#include "kernel/signaldata/FsOpenReq.hpp"
#include "portlib/NdbTick.h"
#include "portlib/ndb_file.h"
#include "util/ndbxfrm_file.h"

#define JAM_FILE_ID 391

/*
 * This define is used to mark up code that is added to workaround issues seen
 * with some distributed filesystem.
 *
 * For example that unlink(file) may succeed removing file but still return
 * error ENOENT.
 */
#define UNRELIABLE_DISTRIBUTED_FILESYSTEM 1

#if UNRELIABLE_DISTRIBUTED_FILESYSTEM
#if defined(VM_TRACE) || !defined(NDEBUG) || defined(ERROR_INSERT)
#define TEST_UNRELIABLE_DISTRIBUTED_FILESYSTEM 1
#endif
#endif
#ifndef TEST_UNRELIABLE_DISTRIBUTED_FILESYSTEM
#define TEST_UNRELIABLE_DISTRIBUTED_FILESYSTEM 0
#endif

#ifndef _WIN32
static inline int get_last_os_error() { return errno; }

static inline void set_last_os_error(int err) { errno = err; }

#else
static inline int get_last_os_error() { return GetLastError(); }

static inline void set_last_os_error(int err) { SetLastError(err); }

#endif

class AsyncFile {
 public:
  AsyncFile(Ndbfs &fs);
  virtual ~AsyncFile();

  int init();
  bool isOpen() const;

  Filename theFileName;
  EncryptionKeyMaterial m_key_material;

  void set_buffer(Uint32 rg, Ptr<GlobalPage> ptr, Uint32 cnt);
  bool has_buffer() const;
  void clear_buffer(Uint32 &rg, Ptr<GlobalPage> &ptr, Uint32 &cnt);
  bool get_buffer(Uint32 &rg, Ptr<GlobalPage> &ptr, Uint32 &cnt);

  AsyncIoThread *getThread() const { return m_thread; }
  bool thread_bound() const { return m_thread_bound; }
  void set_thread_bound(bool value) { m_thread_bound = value; }

  /**
   * openReq() - open a file.
   */
  void openReq(Request *request);

  void closeReq(Request *request);
  void syncReq(Request *request);
  void appendReq(Request *request);
  void readReq(Request *request);
  void writeReq(Request *request);

#if UNRELIABLE_DISTRIBUTED_FILESYSTEM
  bool check_and_log_if_remove_failure_ok(const char *pathname);
#endif
#if TEST_UNRELIABLE_DISTRIBUTED_FILESYSTEM
  bool check_inject_and_log_extra_remove(const char *pathname);
#endif

  /**
   * Implementers of AsyncFile interface
   * should implement the following
   */

  virtual void removeReq(Request *request) = 0;
  virtual void rmrfReq(Request *request, const char *path, bool removePath) = 0;
  virtual void createDirectories() = 0;

  void attach(AsyncIoThread *thr);
  void detach(AsyncIoThread *thr);

  static int probe_directory_direct_io(const char param[],
                                       const char dirname[]);

 private:
  int ndbxfrm_append(Request *request, ndbxfrm_input_iterator *in);

  bool check_odirect_request(const char *buf, size_t sz, ndb_off_t offset);
  void log_set_odirect_result(int result);
  static void log_set_odirect_result(const char *param, const char *filename,
                                     int result);

  Request *m_current_request, *m_last_request;

  AsyncIoThread *m_thread;  // For bound files
  // Whether this file is one that will be/is bound to a thread
  bool m_thread_bound;

  using byte = unsigned char;
  ndb_file m_file;

  Uint32 m_open_flags;

  Uint64 m_next_read_pos;  // if OM_READ_FORWARD else UINT64_MAX
  ndbxfrm_file m_xfile;

  /**
   * file buffers
   */
  Uint32 m_resource_group;
  Uint32 m_page_cnt;
  Ptr<GlobalPage> m_page_ptr;

  char *theWriteBuffer;
  Uint32 theWriteBufferSize;

  Ndbfs &m_fs;

  // ODirect log suppression state
  static constexpr Uint64 odirect_set_log_suppress_period_s =
      4 * 60 * 60;  // 4 hours in seconds
  struct odirect_set_log_state {
    std::atomic<NDB_TICKS> last_warning;
    std::atomic<Uint32> failures = 0;
    std::atomic<Uint32> successes = 0;
  };
  static odirect_set_log_state odirect_set_log_bp[FsOpenReq::BP_MAX];
};

inline void AsyncFile::set_buffer(Uint32 rg, Ptr<GlobalPage> ptr, Uint32 cnt) {
  assert(!has_buffer());
  m_resource_group = rg;
  m_page_ptr = ptr;
  m_page_cnt = cnt;
  theWriteBuffer = (char *)ptr.p;
  theWriteBufferSize = cnt * sizeof(GlobalPage);
}

inline bool AsyncFile::has_buffer() const { return m_page_cnt > 0; }

inline bool AsyncFile::get_buffer(Uint32 &rg, Ptr<GlobalPage> &ptr,
                                  Uint32 &cnt) {
  if (!has_buffer()) return false;
  rg = m_resource_group;
  ptr = m_page_ptr;
  cnt = m_page_cnt;
  return true;
}

inline void AsyncFile::clear_buffer(Uint32 &rg, Ptr<GlobalPage> &ptr,
                                    Uint32 &cnt) {
  bool has_buffer = get_buffer(rg, ptr, cnt);
  require(has_buffer);
  m_page_cnt = 0;
  m_page_ptr.setNull();
  theWriteBuffer = 0;
  theWriteBufferSize = 0;
}

inline bool AsyncFile::isOpen() const { return m_file.is_open(); }

#undef JAM_FILE_ID

#endif
