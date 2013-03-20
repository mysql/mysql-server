/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef AsyncIoThread_H
#define AsyncIoThread_H

#include <kernel_types.h>
#include "MemoryChannel.hpp"
#include <signaldata/BuildIndxImpl.hpp>

// Use this define if you want printouts from AsyncFile class
//#define DEBUG_ASYNCFILE

#ifdef DEBUG_ASYNCFILE
#include <NdbOut.hpp>
#define DEBUG(x) x
#define PRINT_ERRORANDFLAGS(f) printErrorAndFlags(f)
void printErrorAndFlags(Uint32 used_flags);
#else
#define DEBUG(x)
#define PRINT_ERRORANDFLAGS(f)
#endif

const int ERR_ReadUnderflow = 1000;

class AsyncFile;
struct Block_context;

class Request
{
public:
  Request() {}

  void atGet() { m_do_bind = false; }

  enum Action {
    open,
    close,
    closeRemove,
    read,   // Allways leave readv directly after
            // read because SimblockAsyncFileSystem depends on it
    readv,
    write,// Allways leave writev directly after
	        // write because SimblockAsyncFileSystem depends on it
    writev,
    writeSync,// Allways leave writevSync directly after
    // writeSync because SimblockAsyncFileSystem depends on it
    writevSync,
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
  union {
    struct {
      Uint32 flags;
      Uint32 page_size;
      Uint64 file_size;
      Uint32 auto_sync_size;
    } open;
    struct {
      int numberOfPages;
      struct{
	char *buf;
	size_t size;
	off_t offset;
      } pages[32];
    } readWrite;
    struct {
      const char * buf;
      size_t size;
    } append;
    struct {
      bool directory;
      bool own_directory;
    } rmrf;
    struct {
      Block_context* ctx;
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
  int error;

  void set(BlockReference userReference,
	   Uint32 userPointer,
	   Uint16 filePointer);
  BlockReference theUserReference;
  Uint32 theUserPointer;
  Uint16 theFilePointer;
   // Information for open, needed if the first open action fails.
  AsyncFile* file;
  Uint32 theTrace;
  bool m_do_bind;

  MemoryChannel<Request>::ListMember m_mem_channel;
};

NdbOut& operator <<(NdbOut&, const Request&);

inline
void
Request::set(BlockReference userReference,
	     Uint32 userPointer, Uint16 filePointer)
{
  theUserReference= userReference;
  theUserPointer= userPointer;
  theFilePointer= filePointer;
}

class AsyncIoThread
{
  friend class Ndbfs;
  friend class AsyncFile;
public:
  AsyncIoThread(class Ndbfs&, bool bound);
  virtual ~AsyncIoThread() {};

  struct NdbThread* doStart();
  void shutdown();

  // its a thread so its always running
  void run();

  /**
   * Add a request to a thread,
   *   should only be used with bound threads
   */
  void dispatch(Request*);

  AsyncFile * m_current_file;
  Request *m_current_request, *m_last_request;

private:
  Ndbfs & m_fs;

  MemoryChannel<Request> *theReportTo;
  MemoryChannel<Request> *theMemoryChannelPtr;
  MemoryChannel<Request> theMemoryChannel; // If file-bound

  bool   theStartFlag;
  struct NdbThread* theThreadPtr;
  NdbMutex* theStartMutexPtr;
  NdbCondition* theStartConditionPtr;

  /**
   * Alloc mem in FS thread
   */
  void allocMemReq(Request*);

  /**
   * Build ordered index in multi-threaded fashion
   */
  void buildIndxReq(Request*);

  void attach(AsyncFile*);
  void detach(AsyncFile*);
};

#endif
