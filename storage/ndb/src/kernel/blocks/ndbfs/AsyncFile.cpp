/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include "AsyncFile.hpp"
#include <ErrorHandlingMacros.hpp>
#include <kernel_types.h>
#include <ndbd_malloc.hpp>
#include <NdbThread.h>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <Configuration.hpp>

#define JAM_FILE_ID 387


AsyncFile::AsyncFile(SimulatedBlock& fs) :
  theFileName(),
  m_fs(fs)
{
  m_thread = 0;

  m_resource_group = RNIL;
  m_page_cnt = 0;
  m_page_ptr.setNull();
  theWriteBuffer = 0;
  theWriteBufferSize = 0;
}

void
AsyncFile::attach(AsyncIoThread* thr)
{
#if 0
  ndbout_c("%p:%s attach to %p (m_thread: %p)", this, theFileName.c_str(), thr,
             m_thread);
#endif
  assert(m_thread == 0);
  m_thread = thr;
}

void
AsyncFile::detach(AsyncIoThread* thr)
{
#if 0
  ndbout_c("%p:%s detach from %p", this, theFileName.c_str(), thr);
#endif
  assert(m_thread == thr);
  m_thread = 0;
}

void
AsyncFile::readReq( Request * request)
{
  for(int i = 0; i < request->par.readWrite.numberOfPages ; i++)
  {
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
AsyncFile::writeReq(Request * request)
{
  const Uint32 cnt = request->par.readWrite.numberOfPages;
  if (theWriteBuffer == 0 || cnt == 1)
  {
    for (Uint32 i = 0; i<cnt; i++)
    {
      int err = writeBuffer(request->par.readWrite.pages[i].buf,
                            request->par.readWrite.pages[i].size,
                            request->par.readWrite.pages[i].offset);
      if (err)
      {
        request->error = err;
        return;
      }
    }
    goto done;
  }

  {
    int page_num = 0;
    bool write_not_complete = true;

    while(write_not_complete) {
      size_t totsize = 0;
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
            off_t tmp=(off_t)(page_offset+request->par.readWrite.pages[i].size);
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
          page_offset += (off_t)request->par.readWrite.pages[i].size;
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
  }
done:
  if((m_auto_sync_freq && m_write_wo_sync > m_auto_sync_freq) ||
     m_always_sync)
  {
    syncReq(request);
  }
}

void
AsyncFile::writevReq(Request * request)
{
  writeReq(request);
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
