/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef AsyncIoThread_H
#define AsyncIoThread_H

#include <NdbTick.h>
#include <kernel_types.h>
#include <signaldata/BuildIndxImpl.hpp>
#include "MemoryChannel.hpp"
#include "util/ndb_openssl_evp.h"

// Use this define if you want printouts from AsyncFile class
// #define DEBUG_ASYNCFILE

#ifdef DEBUG_ASYNCFILE
#include <NdbOut.hpp>
#define DEBUG(x) x
#define PRINT_ERRORANDFLAGS(f) printErrorAndFlags(f)
void printErrorAndFlags(Uint32 used_flags);
#else
#define DEBUG(x)
#define PRINT_ERRORANDFLAGS(f)
#endif

#define JAM_FILE_ID 381

const int ERR_ReadUnderflow = 1000;

class AsyncFile;
class AsyncIoThread;
struct Block_context;

class Request {
 public:
  Request() {}

  void atGet() {
    m_do_bind = false;
    NdbTick_Invalidate(&m_startTime);
  }

  enum Action {
    open,
    close,
    closeRemove,
    read,
    write,
    writeSync,
    sync,
    end,
    append,
    append_synch,
    rmrf,
    readPartial,
    allocmem,
    buildindx,
    suspend
  };
  Action action;
  static const char *actionName(Action);
  union {
    struct {
      Uint32 flags;
      Uint32 page_size;
      Uint64 file_size;
      Uint32 auto_sync_size;
    } open;
    struct {
      int numberOfPages;
      struct {
        char *buf;
        size_t size;
        ndb_off_t offset;
      } pages[NDB_FS_RW_PAGES];
    } readWrite;
    struct {
      const char *buf;
      size_t size;
    } append;
    struct {
      bool directory;
      bool own_directory;
    } rmrf;
    struct {
      Block_context *ctx;
      Uint32 requestInfo;
      Uint64 bytes;
    } alloc;
    struct {
      struct mt_BuildIndxReq m_req;
    } build;
    struct {
      Uint32 milliseconds;
    } suspend;
  } par;
  struct {
    int code;
    int line;
    const char *file;
    const char *func;
  } error;
  void set_error(int code, int line, const char *file, const char *func) {
    error = {code, line, file, func};
  }
#define NDBFS_SET_REQUEST_ERROR(req, code) \
  ((req)->set_error((code), __LINE__, __FILE__, __func__))
  void set(BlockReference userReference, Uint32 userPointer,
           Uint16 filePointer);
  BlockReference theUserReference;
  Uint32 theUserPointer;
  Uint16 theFilePointer;
  // Information for open, needed if the first open action fails.
  AsyncFile *file;
  AsyncIoThread *thread;
  Uint32 theTrace;
  bool m_do_bind;

  MemoryChannel<Request>::ListMember m_mem_channel;

  // file info for debug
  Uint32 m_file_size_hi;
  Uint32 m_file_size_lo;

  /* More debugging info */
  NDB_TICKS m_startTime;

  /* Pool members */
  Request *listNext;
  Request *listPrev;
};

NdbOut &operator<<(NdbOut &, const Request &);

inline void Request::set(BlockReference userReference, Uint32 userPointer,
                         Uint16 filePointer) {
  theUserReference = userReference;
  theUserPointer = userPointer;
  theFilePointer = filePointer;
}

class AsyncIoThread {
  friend class Ndbfs;
  friend class AsyncFile;

 public:
  AsyncIoThread(class Ndbfs &, bool bound);
  virtual ~AsyncIoThread() {}

  struct NdbThread *doStart();
  void set_real_time(bool real_time) { m_real_time = real_time; }
  void shutdown();

  // its a thread so its always running
  void run();

  /**
   * Add a request to a thread,
   *   should only be used with bound threads
   */
  void dispatch(Request *);

  AsyncFile *m_current_file;
  Request *m_current_request, *m_last_request;

 private:
  Ndbfs &m_fs;

  MemoryChannel<Request> *theReportTo;
  MemoryChannel<Request> *theMemoryChannelPtr;
  MemoryChannel<Request> theMemoryChannel;  // If file-bound

  bool m_real_time;
  bool theStartFlag;
  struct NdbThread *theThreadPtr;
  NdbMutex *theStartMutexPtr;
  NdbCondition *theStartConditionPtr;

  /*
   * Keep an encryption context for reuse for thread unbound files since
   * recreating EVP_CIPHER_CTX is slow.
   */
  ndb_openssl_evp::operation m_openssl_evp_op;
  /**
   * Alloc mem in FS thread
   */
  void allocMemReq(Request *);

  /**
   * Build ordered index in multi-threaded fashion
   */
  void buildIndxReq(Request *);

  void attach(AsyncFile *);
  void detach(AsyncFile *);
};

#undef JAM_FILE_ID

#endif
