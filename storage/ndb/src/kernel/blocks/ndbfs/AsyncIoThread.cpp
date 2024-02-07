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

#include <ndb_global.h>

#include <NdbSleep.h>
#include <NdbThread.h>
#include <kernel_types.h>
#include <ErrorHandlingMacros.hpp>
#include <signaldata/AllocMem.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>
#include "AsyncFile.hpp"
#include "AsyncIoThread.hpp"
#include "Ndbfs.hpp"
#include "util/require.h"

#include <EventLogger.hpp>

#define JAM_FILE_ID 388

AsyncIoThread::AsyncIoThread(class Ndbfs &fs, bool bound)
    : m_fs(fs), m_real_time(false) {
  m_current_file = 0;
  if (bound) {
    theMemoryChannelPtr = &m_fs.theToBoundThreads;
  } else {
    theMemoryChannelPtr = &m_fs.theToUnboundThreads;
  }
  theReportTo = &m_fs.theFromThreads;
}

static int numAsyncFiles = 0;

extern "C" void *runAsyncIoThread(void *arg) {
  ((AsyncIoThread *)arg)->run();
  return (NULL);
}

struct NdbThread *AsyncIoThread::doStart() {
  // Stacksize for filesystem threads
  const NDB_THREAD_STACKSIZE stackSize = 256 * 1024;

  char buf[16];
  numAsyncFiles++;
  BaseString::snprintf(buf, sizeof(buf), "AsyncIoThread%d", numAsyncFiles);

  theStartMutexPtr = NdbMutex_Create();
  theStartConditionPtr = NdbCondition_Create();
  NdbMutex_Lock(theStartMutexPtr);
  theStartFlag = false;

  theThreadPtr = NdbThread_Create(runAsyncIoThread, (void **)this, stackSize,
                                  buf, NDB_THREAD_PRIO_MEAN);

  if (theThreadPtr == 0) {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, "",
              "Could not allocate file system thread");
  }

  do {
    NdbCondition_Wait(theStartConditionPtr, theStartMutexPtr);
  } while (theStartFlag == false);

  NdbMutex_Unlock(theStartMutexPtr);
  NdbMutex_Destroy(theStartMutexPtr);
  NdbCondition_Destroy(theStartConditionPtr);

  return theThreadPtr;
}

void AsyncIoThread::shutdown() {
  void *status;
  Request request;
  request.action = Request::end;
  this->theMemoryChannelPtr->writeChannel(&request);
  NdbThread_WaitFor(theThreadPtr, &status);
  NdbThread_Destroy(&theThreadPtr);
}

void AsyncIoThread::dispatch(Request *request) {
  assert(m_current_file);
  require(m_current_file->thread_bound());
  assert(m_current_file->getThread() == this);
  assert(theMemoryChannelPtr == &theMemoryChannel);
  theMemoryChannelPtr->writeChannel(request);
}

void AsyncIoThread::run() {
  bool first_flag = true;
  Request *request;
  NDB_TICKS last_yield_ticks;

  // Create theMemoryChannel in the thread that will wait for it
  NdbMutex_Lock(theStartMutexPtr);
  theStartFlag = true;
  NdbCondition_Signal(theStartConditionPtr);
  NdbMutex_Unlock(theStartMutexPtr);

  EmulatedJamBuffer jamBuffer;
  jamBuffer.theEmulatedJamIndex = 0;
  // This key is needed by jamNoBlock().
  NDB_THREAD_TLS_JAM = &jamBuffer;

  while (1) {
    if (m_real_time) {
      /**
       * If we are running in real-time we'll simply insert a break every
       * so often to ensure that low-prio threads aren't blocked from the
       * CPU, this is especially important if we're using a compressed
       * file system where lots of CPU is used by this thread.
       */
      bool yield_flag = false;
      const NDB_TICKS current_ticks = NdbTick_getCurrentTicks();

      if (first_flag) {
        first_flag = false;
        yield_flag = true;
      } else {
        Uint64 micros_passed =
            NdbTick_Elapsed(last_yield_ticks, current_ticks).microSec();
        if (micros_passed > 10000) {
          yield_flag = true;
        }
      }
      if (yield_flag) {
        if (NdbThread_yield_rt(theThreadPtr, true)) {
          m_real_time = false;
        }
        last_yield_ticks = current_ticks;
      }
    }
    request = theMemoryChannelPtr->readChannel();
    if (!request || request->action == Request::end) {
      DEBUG(
          g_eventLogger->info("Nothing read from Memory Channel in AsyncFile"));
      theStartFlag = false;
      return;
    }  // if

    AsyncFile *file = request->file;
    /*
     * Associate request with thread to be able to reuse encryption context
     * m_openssl_evp_op.
     */
    request->thread = this;
    m_current_request = request;
    switch (request->action) {
      case Request::open:
        file->openReq(request);
        if (request->error.code == 0 && request->m_do_bind) attach(file);
        break;
      case Request::close:
        file->closeReq(request);
        detach(file);
        break;
      case Request::closeRemove:
        file->closeReq(request);
        file->removeReq(request);
        detach(file);
        break;
      case Request::readPartial:
      case Request::read:
        file->readReq(request);
        break;
      case Request::write:
        file->writeReq(request);
        break;
      case Request::writeSync:
        file->writeReq(request);
        file->syncReq(request);
        break;
      case Request::sync:
        file->syncReq(request);
        break;
      case Request::append:
        file->appendReq(request);
        break;
      case Request::append_synch:
        file->appendReq(request);
        file->syncReq(request);
        break;
      case Request::rmrf:
        file->rmrfReq(request, file->theFileName.c_str(),
                      request->par.rmrf.own_directory);
        break;
      case Request::end:
        theStartFlag = false;
        return;
      case Request::allocmem: {
        allocMemReq(request);
        break;
      }
      case Request::buildindx:
        buildIndxReq(request);
        break;
      case Request::suspend:
        if (request->par.suspend.milliseconds) {
          g_eventLogger->debug("Suspend %s %u ms", file->theFileName.c_str(),
                               request->par.suspend.milliseconds);
          NdbSleep_MilliSleep(request->par.suspend.milliseconds);
          continue;
        } else {
          g_eventLogger->debug("Suspend %s", file->theFileName.c_str());
          theStartFlag = false;
          return;
        }
      default:
        DEBUG(g_eventLogger->info("Invalid Request"));
        abort();
        break;
    }  // switch
    m_last_request = request;
    m_current_request = 0;

    // No need to signal as ndbfs only uses tryRead
    theReportTo->writeChannelNoSignal(request);
    m_fs.wakeup();
  }
}

void AsyncIoThread::allocMemReq(Request *request) {
  Uint32 watchDog = 0;
  switch ((request->par.alloc.requestInfo & 255)) {
    case AllocMemReq::RT_MAP: {
      bool memlock =
          !!(request->par.alloc.requestInfo & AllocMemReq::RT_MEMLOCK);
      request->par.alloc.ctx->m_mm.map(&watchDog, memlock);
      request->par.alloc.bytes = 0;
      NDBFS_SET_REQUEST_ERROR(request, 0);
      break;
    }
    case AllocMemReq::RT_EXTEND:
      /**
       * Not implemented...
       */
      assert(false);
      request->par.alloc.bytes = 0;
      NDBFS_SET_REQUEST_ERROR(request, 1);
      break;
  }
}

void AsyncIoThread::buildIndxReq(Request *request) {
  /**
   * Rebind thread config to allow different behaviour
   * during Index build.
   */
  THRConfigRebinder idxbuild_cpulock(&m_fs.m_ctx.m_config.m_thr_config,
                                     THRConfig::T_IXBLD, theThreadPtr);

  mt_BuildIndxReq req;
  memcpy(&req, &request->par.build.m_req, sizeof(req));
  Uint32 rg;
  Ptr<GlobalPage> ptr;
  Uint32 cnt;
  bool has_buffer = request->file->get_buffer(rg, ptr, cnt);
  require(has_buffer);
  req.mem_buffer = ptr.p;
  req.buffer_size = cnt * sizeof(GlobalPage);
  NDBFS_SET_REQUEST_ERROR(request, (*req.func_ptr)(&req));
}

void AsyncIoThread::attach(AsyncFile *file) {
  assert(m_current_file == 0);
  assert(theMemoryChannelPtr == &m_fs.theToBoundThreads);
  m_current_file = file;
  theMemoryChannelPtr = &theMemoryChannel;
  file->attach(this);
}

void AsyncIoThread::detach(AsyncFile *file) {
  if (m_current_file == 0) {
    assert(!file->thread_bound());
  } else {
    assert(m_current_file == file);
    assert(theMemoryChannelPtr = &theMemoryChannel);
    m_current_file = 0;
    theMemoryChannelPtr = &m_fs.theToBoundThreads;
    file->detach(this);
  }
}

const char *Request::actionName(Request::Action action) {
  switch (action) {
    case Request::open:
      return "open";
    case Request::close:
      return "close";
    case Request::closeRemove:
      return "closeRemove";
    case Request::read:
      return "read";
    case Request::write:
      return "write";
    case Request::writeSync:
      return "writeSync";
    case Request::sync:
      return "sync";
    case Request::end:
      return "end";
    case Request::append:
      return "append";
    case Request::append_synch:
      return "append_synch";
    case Request::rmrf:
      return "rmrf";
    case Request::readPartial:
      return "readPartial";
    case Request::allocmem:
      return "allocmem";
    case Request::buildindx:
      return "buildindx";
    case Request::suspend:
      return "suspend";
    default:
      return "Unknown action";
  }
}
