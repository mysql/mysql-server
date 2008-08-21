/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <my_sys.h>
#include <my_pthread.h>

#include "AsyncFile.hpp"
#include <ErrorHandlingMacros.hpp>
#include <kernel_types.h>
#include <ndbd_malloc.hpp>
#include <NdbThread.h>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <Configuration.hpp>

const char *actionName[] = {
  "open",
  "close",
  "closeRemove",
  "read",
  "readv",
  "write",
  "writev",
  "writeSync",
  "writevSync",
  "sync",
  "end" };

static int numAsyncFiles = 0;

extern "C" void * runAsyncFile(void* arg)
{
  ((AsyncFile*)arg)->run();
  return (NULL);
}


AsyncFile::AsyncFile(SimulatedBlock& fs) :
  theFileName(),
  theReportTo(0),
  theMemoryChannelPtr(NULL),
  m_fs(fs)
{
  m_page_ptr.setNull();
  m_current_request= m_last_request= 0;
  m_auto_sync_freq = 0;
}

void
AsyncFile::doStart()
{
  // Stacksize for filesystem threads
  // An 8k stack should be enough
  const NDB_THREAD_STACKSIZE stackSize = 8192;

  char buf[16];
  struct ThreadContainer container;
  numAsyncFiles++;
  BaseString::snprintf(buf, sizeof(buf), "AsyncFile%d", numAsyncFiles);

  theStartMutexPtr = NdbMutex_Create();
  theStartConditionPtr = NdbCondition_Create();
  NdbMutex_Lock(theStartMutexPtr);
  theStartFlag = false;
  container.conf = globalEmulatorData.theConfiguration;
  container.type = NdbfsThread;
  theThreadPtr = NdbThread_CreateWithFunc(runAsyncFile,
                                  (void**)this,
                                  stackSize,
                                  (char*)&buf,
                                  NDB_THREAD_PRIO_MEAN,
                                  ndb_thread_add_thread_id,
                                  &container,
                                  sizeof(container),
                                  ndb_thread_remove_thread_id,
                                  &container,
                                  sizeof(container));
  if (theThreadPtr == 0)
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, "","Could not allocate file system thread");

  NdbCondition_Wait(theStartConditionPtr,
                    theStartMutexPtr);
  NdbMutex_Unlock(theStartMutexPtr);
  NdbMutex_Destroy(theStartMutexPtr);
  NdbCondition_Destroy(theStartConditionPtr);
}

void AsyncFile::shutdown()
{
  void *status;
  Request request;
  request.action = Request::end;
  this->theMemoryChannelPtr->writeChannel( &request );
  NdbThread_WaitFor(theThreadPtr, &status);
  NdbThread_Destroy(&theThreadPtr);
  delete theMemoryChannelPtr;
}

void
AsyncFile::reportTo( MemoryChannel<Request> *reportTo )
{
  theReportTo = reportTo;
}

void AsyncFile::execute(Request* request)
{
  theMemoryChannelPtr->writeChannel( request );
}

int AsyncFile::init()
{
  // Create write buffer for bigger writes
  theWriteBufferSize = WRITEBUFFERSIZE;
  theWriteBuffer = (char *) ndbd_malloc(theWriteBufferSize);

  return 0;
}

void
AsyncFile::run()
{
  Request *request;

  // Create theMemoryChannel in the thread that will wait for it
  NdbMutex_Lock(theStartMutexPtr);
  theMemoryChannelPtr = new MemoryChannel<Request>();
  theStartFlag = true;

  int r= this->init();

  NdbMutex_Unlock(theStartMutexPtr);
  NdbCondition_Signal(theStartConditionPtr);

  if(r!=0)
  {
    DEBUG(ndbout_c("AsyncFile::init() failed"));
    return;
  }

  while (1) {
    request = theMemoryChannelPtr->readChannel();
    if (!request) {
      DEBUG(ndbout_c("Nothing read from Memory Channel in AsyncFile"));
      endReq();
      return;
    }//if
    m_current_request= request;
    switch (request->action) {
    case Request:: open:
      openReq(request);
      break;
    case Request:: close:
      closeReq(request);
      break;
    case Request:: closeRemove:
      closeReq(request);
      removeReq(request);
      break;
    case Request:: readPartial:
    case Request:: read:
      readReq(request);
      break;
    case Request:: readv:
      readvReq(request);
      break;
    case Request:: write:
      writeReq(request);
      break;
    case Request:: writev:
      writevReq(request);
      break;
    case Request:: writeSync:
      writeReq(request);
      syncReq(request);
      break;
    case Request:: writevSync:
      writevReq(request);
      syncReq(request);
      break;
    case Request:: sync:
      syncReq(request);
      break;
    case Request:: append:
      appendReq(request);
      break;
    case Request:: append_synch:
      appendReq(request);
      syncReq(request);
      break;
    case Request::rmrf:
      rmrfReq(request, (char*)theFileName.c_str(), request->par.rmrf.own_directory);
      break;
    case Request:: end:
      if (isOpen())
        closeReq(request);
      endReq();
      return;
    default:
      DEBUG(ndbout_c("Invalid Request"));
      abort();
      break;
    }//switch
    m_last_request= request;
    m_current_request= 0;

    // No need to signal as ndbfs only uses tryRead
    theReportTo->writeChannelNoSignal(request);
  }//while
}//AsyncFile::run()


void
AsyncFile::readReq( Request * request)
{
  for(int i = 0; i < request->par.readWrite.numberOfPages ; i++) {
    off_t offset = request->par.readWrite.pages[i].offset;
    size_t size  = request->par.readWrite.pages[i].size;
    char * buf   = request->par.readWrite.pages[i].buf;

    int err = readBuffer(request, buf, size, offset);
    if(err != 0){
      request->error = err;
      return;
    }
  }
}

void
AsyncFile::readvReq( Request * request)
{
  readReq(request);
  return;
}

void
AsyncFile::writeReq( Request * request)
{
  int page_num = 0;
  bool write_not_complete = true;

  while(write_not_complete) {
    int totsize = 0;
    off_t offset = request->par.readWrite.pages[page_num].offset;
    char* bufptr = theWriteBuffer;

    write_not_complete = false;
    if (request->par.readWrite.numberOfPages > 1) {
      off_t page_offset = offset;

      // Multiple page write, copy to buffer for one write
      for(int i=page_num; i < request->par.readWrite.numberOfPages; i++) {
        memcpy(bufptr,
               request->par.readWrite.pages[i].buf,
               request->par.readWrite.pages[i].size);
        bufptr += request->par.readWrite.pages[i].size;
        totsize += request->par.readWrite.pages[i].size;
        if (((i + 1) < request->par.readWrite.numberOfPages)) {
          // There are more pages to write
          // Check that offsets are consequtive
	  off_t tmp = page_offset + request->par.readWrite.pages[i].size;
          if (tmp != request->par.readWrite.pages[i+1].offset) {
            // Next page is not aligned with previous, not allowed
            DEBUG(ndbout_c("Page offsets are not aligned"));
            request->error = EINVAL;
            return;
          }
          if ((unsigned)(totsize + request->par.readWrite.pages[i+1].size) > (unsigned)theWriteBufferSize) {
            // We are not finished and the buffer is full
            write_not_complete = true;
            // Start again with next page
            page_num = i + 1;
            break;
          }
        }
        page_offset += request->par.readWrite.pages[i].size;
      }
      bufptr = theWriteBuffer;
    } else {
      // One page write, write page directly
      bufptr = request->par.readWrite.pages[0].buf;
      totsize = request->par.readWrite.pages[0].size;
    }
    int err = writeBuffer(bufptr, totsize, offset);
    if(err != 0){
      request->error = err;
      return;
    }
  } // while(write_not_complete)

  if(m_auto_sync_freq && m_write_wo_sync > m_auto_sync_freq){
    syncReq(request);
  }
}

void
AsyncFile::writevReq( Request * request)
{
  // WriteFileGather on WIN32?
  writeReq(request);
}

void AsyncFile::endReq()
{
  if (theWriteBuffer)
    ndbd_free(theWriteBuffer, theWriteBufferSize);
}

#ifdef DEBUG_ASYNCFILE
void printErrorAndFlags(Uint32 used_flags) {
  char buf[255];
  sprintf(buf, "PEAF: errno=%d \"", errno);

  strcat(buf, strerror(errno));

  strcat(buf, "\" ");
  strcat(buf, " flags: ");
  switch(used_flags & 3){
  case O_RDONLY:
    strcat(buf, "O_RDONLY, ");
    break;
  case O_WRONLY:
    strcat(buf, "O_WRONLY, ");
    break;
  case O_RDWR:
    strcat(buf, "O_RDWR, ");
    break;
  default:
    strcat(buf, "Unknown!!, ");
  }

  if((used_flags & O_APPEND)==O_APPEND)
    strcat(buf, "O_APPEND, ");
  if((used_flags & O_CREAT)==O_CREAT)
    strcat(buf, "O_CREAT, ");
  if((used_flags & O_EXCL)==O_EXCL)
    strcat(buf, "O_EXCL, ");
  if((used_flags & O_NOCTTY) == O_NOCTTY)
    strcat(buf, "O_NOCTTY, ");
  if((used_flags & O_NONBLOCK)==O_NONBLOCK)
    strcat(buf, "O_NONBLOCK, ");
  if((used_flags & O_TRUNC)==O_TRUNC)
    strcat(buf, "O_TRUNC, ");
#ifdef O_DSYNC /* At least Darwin 7.9 doesn't have it */
  if((used_flags & O_DSYNC)==O_DSYNC)
    strcat(buf, "O_DSYNC, ");
#endif
  if((used_flags & O_NDELAY)==O_NDELAY)
    strcat(buf, "O_NDELAY, ");
#ifdef O_RSYNC /* At least Darwin 7.9 doesn't have it */
  if((used_flags & O_RSYNC)==O_RSYNC)
    strcat(buf, "O_RSYNC, ");
#endif
#ifdef O_SYNC
  if((used_flags & O_SYNC)==O_SYNC)
    strcat(buf, "O_SYNC, ");
#endif
  DEBUG(ndbout_c(buf));

}
#endif

NdbOut&
operator<<(NdbOut& out, const Request& req)
{
  out << "[ Request: file: " << hex << req.file
      << " userRef: " << hex << req.theUserReference
      << " userData: " << dec << req.theUserPointer
      << " theFilePointer: " << req.theFilePointer
      << " action: ";
  switch(req.action){
  case Request::open:
    out << "open";
    break;
  case Request::close:
    out << "close";
    break;
  case Request::closeRemove:
    out << "closeRemove";
    break;
  case Request::read:   // Allways leave readv directly after
    out << "read";
    break;
  case Request::readv:
    out << "readv";
    break;
  case Request::write:// Allways leave writev directly after
    out << "write";
    break;
  case Request::writev:
    out << "writev";
    break;
  case Request::writeSync:// Allways leave writevSync directly after
    out << "writeSync";
    break;
    // writeSync because SimblockAsyncFileSystem depends on it
  case Request::writevSync:
    out << "writevSync";
    break;
  case Request::sync:
    out << "sync";
    break;
  case Request::end:
    out << "end";
    break;
  case Request::append:
    out << "append";
    break;
  case Request::rmrf:
    out << "rmrf";
    break;
  default:
    out << (Uint32)req.action;
    break;
  }
  out << " ]";
  return out;
}
