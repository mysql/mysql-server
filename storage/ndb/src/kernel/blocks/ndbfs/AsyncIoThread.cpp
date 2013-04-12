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

#include <ndb_global.h>

#include "AsyncIoThread.hpp"
#include "AsyncFile.hpp"
#include <ErrorHandlingMacros.hpp>
#include <kernel_types.h>
#include <NdbThread.h>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/AllocMem.hpp>
#include "Ndbfs.hpp"
#include <NdbSleep.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

AsyncIoThread::AsyncIoThread(class Ndbfs& fs, bool bound)
  : m_fs(fs)
{
  m_current_file = 0;
  if (bound)
  {
    theMemoryChannelPtr = &m_fs.theToBoundThreads;
  }
  else
  {
    theMemoryChannelPtr = &m_fs.theToUnboundThreads;
  }
  theReportTo = &m_fs.theFromThreads;
}

static int numAsyncFiles = 0;

extern "C"
void *
runAsyncIoThread(void* arg)
{
  ((AsyncIoThread*)arg)->run();
  return (NULL);
}


struct NdbThread*
AsyncIoThread::doStart()
{
  // Stacksize for filesystem threads
#if !defined(DBUG_OFF) && defined (__hpux)
  // Empirical evidence indicates at least 32k
  const NDB_THREAD_STACKSIZE stackSize = 32768;
#else
  // Otherwise an 8k stack should be enough
  const NDB_THREAD_STACKSIZE stackSize = 8192;
#endif

  char buf[16];
  numAsyncFiles++;
  BaseString::snprintf(buf, sizeof(buf), "AsyncIoThread%d", numAsyncFiles);

  theStartMutexPtr = NdbMutex_Create();
  theStartConditionPtr = NdbCondition_Create();
  NdbMutex_Lock(theStartMutexPtr);
  theStartFlag = false;

  theThreadPtr = NdbThread_Create(runAsyncIoThread,
                                  (void**)this,
                                  stackSize,
                                  buf,
                                  NDB_THREAD_PRIO_MEAN);

  if (theThreadPtr == 0)
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC,
              "","Could not allocate file system thread");
  }

  do
  {
    NdbCondition_Wait(theStartConditionPtr,
                      theStartMutexPtr);
  }
  while (theStartFlag == false);

  NdbMutex_Unlock(theStartMutexPtr);
  NdbMutex_Destroy(theStartMutexPtr);
  NdbCondition_Destroy(theStartConditionPtr);

  return theThreadPtr;
}

void
AsyncIoThread::shutdown()
{
  void *status;
  Request request;
  request.action = Request::end;
  this->theMemoryChannelPtr->writeChannel( &request );
  NdbThread_WaitFor(theThreadPtr, &status);
  NdbThread_Destroy(&theThreadPtr);
}

void
AsyncIoThread::dispatch(Request *request)
{
  assert(m_current_file);
  assert(m_current_file->getThread() == this);
  assert(theMemoryChannelPtr == &theMemoryChannel);
  theMemoryChannelPtr->writeChannel(request);
}

void
AsyncIoThread::run()
{
  Request *request;

  // Create theMemoryChannel in the thread that will wait for it
  NdbMutex_Lock(theStartMutexPtr);
  theStartFlag = true;
  NdbMutex_Unlock(theStartMutexPtr);
  NdbCondition_Signal(theStartConditionPtr);

  while (1)
  {
    request = theMemoryChannelPtr->readChannel();
    if (!request || request->action == Request::end)
    {
      DEBUG(ndbout_c("Nothing read from Memory Channel in AsyncFile"));
      theStartFlag = false;
      return;
    }//if

    AsyncFile * file = request->file;
    m_current_request= request;
    switch (request->action) {
    case Request::open:
      file->openReq(request);
      if (request->error == 0 && request->m_do_bind)
        attach(file);
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
    case Request::readv:
      file->readvReq(request);
      break;
    case Request::write:
      file->writeReq(request);
      break;
    case Request::writev:
      file->writevReq(request);
      break;
    case Request::writeSync:
      file->writeReq(request);
      file->syncReq(request);
      break;
    case Request::writevSync:
      file->writevReq(request);
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
    case Request::allocmem:
    {
      allocMemReq(request);
      break;
    }
    case Request::buildindx:
      buildIndxReq(request);
      break;
    case Request::suspend:
      if (request->par.suspend.milliseconds)
      {
        g_eventLogger->debug("Suspend %s %u ms",
                             file->theFileName.c_str(),
                             request->par.suspend.milliseconds);
        NdbSleep_MilliSleep(request->par.suspend.milliseconds);
        continue;
      }
      else
      {
        g_eventLogger->debug("Suspend %s",
                             file->theFileName.c_str());
        theStartFlag = false;
        return;
      }
    default:
      DEBUG(ndbout_c("Invalid Request"));
      abort();
      break;
    }//switch
    m_last_request = request;
    m_current_request = 0;

    // No need to signal as ndbfs only uses tryRead
    theReportTo->writeChannelNoSignal(request);
    m_fs.wakeup();
  }
}

void
AsyncIoThread::allocMemReq(Request* request)
{
  Uint32 watchDog = 0;
  switch((request->par.alloc.requestInfo & 255)){
  case AllocMemReq::RT_MAP:{
    bool memlock = !!(request->par.alloc.requestInfo & AllocMemReq::RT_MEMLOCK);
    request->par.alloc.ctx->m_mm.map(&watchDog, memlock);
    request->par.alloc.bytes = 0;
    request->error = 0;
    break;
  }
  case AllocMemReq::RT_EXTEND:
    /**
     * Not implemented...
     */
    assert(false);
    request->par.alloc.bytes = 0;
    request->error = 1;
    break;
  }
}

void
AsyncIoThread::buildIndxReq(Request* request)
{
  mt_BuildIndxReq req;
  memcpy(&req, &request->par.build.m_req, sizeof(req));
  req.mem_buffer = request->file->m_page_ptr.p;
  req.buffer_size = request->file->m_page_cnt * sizeof(GlobalPage);
  request->error = (* req.func_ptr)(&req);
}

void
AsyncIoThread::attach(AsyncFile* file)
{
  assert(m_current_file == 0);
  assert(theMemoryChannelPtr == &m_fs.theToBoundThreads);
  m_current_file = file;
  theMemoryChannelPtr = &theMemoryChannel;
  file->attach(this);
  m_fs.cnt_active_bound(1);
}

void
AsyncIoThread::detach(AsyncFile* file)
{
  if (m_current_file == 0)
  {
    assert(file->getThread() == 0);
  }
  else
  {
    assert(m_current_file == file);
    assert(theMemoryChannelPtr = &theMemoryChannel);
    m_current_file = 0;
    theMemoryChannelPtr = &m_fs.theToBoundThreads;
    file->detach(this);
    m_fs.cnt_active_bound(-1);
  }
}
