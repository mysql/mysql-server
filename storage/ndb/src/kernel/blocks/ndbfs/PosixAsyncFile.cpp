/* 
   Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.

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
*/

#include <ndb_global.h>
#include "my_sys.h"
#include "my_thread.h"

#ifdef HAVE_XFS_XFS_H
#include <xfs/xfs.h>
#endif

#include "Ndbfs.hpp"
#include "AsyncFile.hpp"
#include "PosixAsyncFile.hpp"
#include "my_thread_local.h"

#include <ErrorHandlingMacros.hpp>
#include <kernel_types.h>
#include <ndbd_malloc.hpp>
#include <NdbThread.h>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>

#include <NdbTick.h>

// For readv and writev
#include <sys/uio.h>
#include <dirent.h>

#include <EventLogger.hpp>

#define JAM_FILE_ID 384

extern EventLogger* g_eventLogger;


PosixAsyncFile::PosixAsyncFile(SimulatedBlock& fs) :
  AsyncFile(fs),
  theFd(-1),
  use_gz(0)
{
  memset(&nzf,0,sizeof(nzf));
  init_mutex();
}

int PosixAsyncFile::init()
{
  /*
    Preallocate read and write buffers for ndbzio to workaround
    default behaviour of alloc/free at open/close
  */
  const size_t read_size = ndbz_bufsize_read();
  const size_t write_size = ndbz_bufsize_write();

  nzfBufferUnaligned= ndbd_malloc(read_size + write_size +
                                  NDB_O_DIRECT_WRITE_ALIGNMENT-1);
  nzf.inbuf= (Byte*)(((UintPtr)nzfBufferUnaligned
                      + NDB_O_DIRECT_WRITE_ALIGNMENT - 1) &
                     ~(UintPtr)(NDB_O_DIRECT_WRITE_ALIGNMENT - 1));
  nzf.outbuf= nzf.inbuf + read_size;

  /* Preallocate inflate/deflate buffers for ndbzio */
  nz_mempool.size = nz_mempool.mfree =
    ndbz_inflate_mem_size() + ndbz_deflate_mem_size();

  ndbout_c("NDBFS/AsyncFile: Allocating %u for In/Deflate buffer",
           (unsigned int)nz_mempool.size);
  nz_mempool.mem = (char*) ndbd_malloc(nz_mempool.size);

  nzf.stream.opaque= &nz_mempool;

  m_filetype = 0;

  return 0;
}

void PosixAsyncFile::set_or_check_filetype(bool set)
{
  struct stat sb;
  if (fstat(theFd, &sb) == -1)
  {
    g_eventLogger->error("fd=%d: fstat errno=%d",
                          theFd, errno);
    abort();
  }
  int ft = sb.st_mode >> 12; // posix
  if (set)
    m_filetype = ft;
  else if (m_filetype != ft)
  {
    g_eventLogger->error("fd=%d: type old=%d new=%d",
                          theFd, m_filetype, ft);
    abort();
  }
}

#ifdef O_DIRECT
static char g_odirect_readbuf[2*GLOBAL_PAGE_SIZE -1];
#endif

int PosixAsyncFile::check_odirect_write(Uint32 flags, int& new_flags, int mode)
{
  assert(new_flags & (O_CREAT | O_TRUNC));
#ifdef O_DIRECT
  int ret;
  char * bufptr = (char*)((UintPtr(g_odirect_readbuf)+(GLOBAL_PAGE_SIZE - 1)) & ~(GLOBAL_PAGE_SIZE - 1));
  while (((ret = ::write(theFd, bufptr, GLOBAL_PAGE_SIZE)) == -1) &&
         (errno == EINTR)) {};
  if (ret == -1)
  {
    new_flags &= ~O_DIRECT;
    ndbout_c("%s Failed to write using O_DIRECT, disabling",
             theFileName.c_str());
  }

  close(theFd);
  /**
   * We need to (O_TRUNC) truncate the file since we've written a page to it...
   */
  theFd = ::open(theFileName.c_str(), new_flags | O_TRUNC, mode);
  if (theFd == -1)
    return errno;
#endif

  return 0;
}

int PosixAsyncFile::check_odirect_read(Uint32 flags, int &new_flags, int mode)
{
#ifdef O_DIRECT
  int ret;
  char * bufptr = (char*)((UintPtr(g_odirect_readbuf)+(GLOBAL_PAGE_SIZE - 1)) & ~(GLOBAL_PAGE_SIZE - 1));
  while (((ret = ::read(theFd, bufptr, GLOBAL_PAGE_SIZE)) == -1) &&
         (errno == EINTR)) {};
  if (ret == -1)
  {
    ndbout_c("%s Failed to read using O_DIRECT, disabling",
             theFileName.c_str());
    goto reopen;
  }

  if(lseek(theFd, 0, SEEK_SET) != 0)
  {
    return errno;
  }

  if ((flags & FsOpenReq::OM_CHECK_SIZE) == 0)
  {
    struct stat buf;
    if ((fstat(theFd, &buf) == -1))
    {
      return errno;
    }
    else if ((buf.st_size % GLOBAL_PAGE_SIZE) != 0)
    {
      ndbout_c("%s filesize not a multiple of %d, disabling O_DIRECT",
               theFileName.c_str(), GLOBAL_PAGE_SIZE);
      goto reopen;
    }
  }

  return 0;

reopen:
  close(theFd);
  new_flags &= ~O_DIRECT;
  theFd = ::open(theFileName.c_str(), new_flags, mode);
  if (theFd == -1)
    return errno;
#endif
  return 0;
}

void PosixAsyncFile::openReq(Request *request)
{
  m_auto_sync_freq = 0;
  m_write_wo_sync = 0;
  m_open_flags = request->par.open.flags;
  m_use_o_direct_sync_flag = false;
  bool failed_to_set_o_direct = false;
  (void)failed_to_set_o_direct; //Silence compiler warning

  // for open.flags, see signal FSOPENREQ
  Uint32 flags = request->par.open.flags;
  int new_flags = 0;

  // Convert file open flags from Solaris to Linux
  if (flags & FsOpenReq::OM_CREATE)
  {
    new_flags |= O_CREAT;
  }

  if (flags & FsOpenReq::OM_TRUNCATE){
      new_flags |= O_TRUNC;
  }

  if (flags & FsOpenReq::OM_APPEND){
    new_flags |= O_APPEND;
  }

#ifdef O_DIRECT
  if (flags & FsOpenReq::OM_DIRECT_SYNC)
  {
    require(flags & FsOpenReq::OM_DIRECT);
  }
  if (flags & FsOpenReq::OM_DIRECT)
  {
    new_flags |= O_DIRECT;
  }
#endif

  m_always_sync = false;

  if ((flags & FsOpenReq::OM_SYNC) && ! (flags & FsOpenReq::OM_INIT))
  {
#ifdef O_SYNC
    new_flags |= O_SYNC;
#else
    m_always_sync = true;
#endif
  }

  const char * rw = "";
  switch(flags & 0x3){
  case FsOpenReq::OM_READONLY:
    rw = "r";
    new_flags |= O_RDONLY;
    break;
  case FsOpenReq::OM_WRITEONLY:
    rw = "w";
    new_flags |= O_WRONLY;
    break;
  case FsOpenReq::OM_READWRITE:
    rw = "rw";
    new_flags |= O_RDWR;
    break;
  default:
    request->error = 1000;
    break;
    return;
  }
  if (flags & FsOpenReq::OM_GZ)
  {
    use_gz = 1;
  }
  else
  {
    use_gz = 0;
  }

  // allow for user to choose any permissionsa with umask
  const int mode = S_IRUSR | S_IWUSR |
	           S_IRGRP | S_IWGRP |
		   S_IROTH | S_IWOTH;
  if (flags & FsOpenReq::OM_CREATE_IF_NONE)
  {
    Uint32 tmp_flags = new_flags;
#ifdef O_DIRECT
    tmp_flags &= ~O_DIRECT;
#endif
    if ((theFd = ::open(theFileName.c_str(), tmp_flags, mode)) != -1)
    {
      close(theFd);
      theFd = -1;
      request->error = FsRef::fsErrFileExists;
      return;
    }
    new_flags |= O_CREAT;
    flags |= FsOpenReq::OM_CREATE;
  }

#ifdef O_DIRECT
no_odirect:
#endif
  theFd = ::open(theFileName.c_str(), new_flags, mode);
  if (-1 == theFd)
  {
    PRINT_ERRORANDFLAGS(new_flags);
    if ((errno == ENOENT) && (new_flags & O_CREAT))
    {
      createDirectories();
      theFd = ::open(theFileName.c_str(), new_flags, mode);
      if (-1 == theFd)
      {
#ifdef O_DIRECT
	if (new_flags & O_DIRECT)
	{
	  new_flags &= ~O_DIRECT;
          failed_to_set_o_direct = true;
	  goto no_odirect;
	}
#endif
        PRINT_ERRORANDFLAGS(new_flags);
        request->error = errno;
	return;
      }
    }
#ifdef O_DIRECT
    else if (new_flags & O_DIRECT)
    {
      new_flags &= ~O_DIRECT;
      failed_to_set_o_direct = true;
      goto no_odirect;
    }
#endif
    else
    {
      request->error = errno;
      return;
    }
  }

  if (flags & FsOpenReq::OM_CHECK_SIZE)
  {
    struct stat buf;
    if ((fstat(theFd, &buf) == -1))
    {
      request->error = errno;
    }
    else if((Uint64)buf.st_size != request->par.open.file_size)
    {
      request->error = FsRef::fsErrInvalidFileSize;
    }
    if (request->error)
    {
      close(theFd);
      return;
    }
  }

  if (flags & FsOpenReq::OM_INIT)
  {
    off_t off = 0;
    const off_t sz = request->par.open.file_size;
    SignalT<25> tmp;
    Signal * signal = new (&tmp) Signal(0);
    bzero(signal, sizeof(tmp));
    FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();

    Uint32 index = 0;
    Uint32 block = refToMain(request->theUserReference);
    Uint32 instance = refToInstance(request->theUserReference);

#ifdef HAVE_XFS_XFS_H
    if(platform_test_xfs_fd(theFd))
    {
      ndbout_c("Using xfsctl(XFS_IOC_RESVSP64) to allocate disk space");
      xfs_flock64_t fl;
      fl.l_whence= 0;
      fl.l_start= 0;
      fl.l_len= (off64_t)sz;
      if(xfsctl(NULL, theFd, XFS_IOC_RESVSP64, &fl) < 0)
        ndbout_c("failed to optimally allocate disk space");
    }
#endif
#ifdef HAVE_POSIX_FALLOCATE
    posix_fallocate(theFd, 0, sz);
#endif

#ifdef VM_TRACE
#define TRACE_INIT
#endif

#ifdef TRACE_INIT
    Uint32 write_cnt = 0;
    const NDB_TICKS start = NdbTick_getCurrentTicks();
#endif
    while(off < sz)
    {
      off_t size = 0;
      Uint32 cnt = 0;
      while (cnt < m_page_cnt && (off + size) < sz)
      {
        req->filePointer = 0;          // DATA 0
        req->userPointer = request->theUserPointer;          // DATA 2
        req->numberOfPages = 1;        // DATA 5
        req->varIndex = index++;
        req->data.pageData[0] = m_page_ptr.i + cnt;

        m_fs.EXECUTE_DIRECT_MT(block, GSN_FSWRITEREQ, signal,
                               FsReadWriteReq::FixedLength + 1,
                               instance);
        
        cnt++;
        size += request->par.open.page_size;
      }
#ifdef O_DIRECT
  retry:
#endif
      off_t save_size = size;
      char* buf = (char*)m_page_ptr.p;
      while(size > 0)
      {
#ifdef TRACE_INIT
        write_cnt++;
#endif
        int n;
	if(use_gz)
          n= ndbzwrite(&nzf, buf, size);
        else
          n= write(theFd, buf, size);
	if(n == -1 && errno == EINTR)
	{
	  continue;
	}
	if(n == -1 || n == 0)
	{
          ndbout_c("ndbzwrite|write returned %d: errno: %d my_errno: %d",n,errno,my_errno());
	  break;
	}
	size -= n;
	buf += n;
      }
      if(size != 0)
      {
	int err = errno;
#ifdef O_DIRECT
	if ((new_flags & O_DIRECT) && off == 0)
	{
	  ndbout_c("error on first write(%d), disable O_DIRECT", err);
	  new_flags &= ~O_DIRECT;
          failed_to_set_o_direct = true;
	  close(theFd);
	  theFd = ::open(theFileName.c_str(), new_flags, mode);
	  if (theFd != -1)
	    goto retry;
	}
#endif
	close(theFd);
	theFd = -1;
	unlink(theFileName.c_str());
	request->error = err;
	return;
      }
      off += save_size;
    }
    ::fsync(theFd);
#ifdef TRACE_INIT
    const NDB_TICKS stop = NdbTick_getCurrentTicks();
    Uint64 diff = NdbTick_Elapsed(start, stop).milliSec();
    if (diff == 0)
      diff = 1;
    ndbout_c("wrote %umb in %u writes %us -> %ukb/write %umb/s",
             Uint32(sz /1024/1024),
             write_cnt,
             Uint32(diff / 1000),
             Uint32(sz / 1024 / write_cnt),
             Uint32(sz / diff));
#endif

    if(lseek(theFd, 0, SEEK_SET) != 0)
    {
      request->error = errno;
      close(theFd);
    }
  }
  else if (flags & FsOpenReq::OM_DIRECT)
  {
#ifdef O_DIRECT
    if (flags & (FsOpenReq::OM_TRUNCATE | FsOpenReq::OM_CREATE))
    {
      request->error = check_odirect_write(flags, new_flags, mode);
    }
    else
    {
      request->error = check_odirect_read(flags, new_flags, mode);
    }

    if (request->error)
    {
      close(theFd);
      return;
    }
#endif
  }

#ifdef VM_TRACE
  if (flags & FsOpenReq::OM_DIRECT)
  {
#ifdef O_DIRECT
    ndbout_c("%s %s O_DIRECT: %d",
             theFileName.c_str(), rw,
             !!(new_flags & O_DIRECT));
#else
    ndbout_c("%s %s O_DIRECT: 0",
             theFileName.c_str(), rw);
#endif
  }
#endif
  
  if ((flags & FsOpenReq::OM_SYNC) && (flags & FsOpenReq::OM_INIT))
  {
#ifdef O_SYNC
    /**
     * reopen file with O_SYNC
     */
    close(theFd);
    new_flags &= ~(O_CREAT | O_TRUNC);
    new_flags |= O_SYNC;
    theFd = ::open(theFileName.c_str(), new_flags, mode);
    if (theFd == -1)
    {
      request->error = errno;
    }
#else
    m_always_sync = true;
#endif
  }

#if ! defined(O_DIRECT) && defined HAVE_DIRECTIO && defined(DIRECTIO_ON)
  if (flags & FsOpenReq::OM_DIRECT)
  {
    if (directio(theFd, DIRECTIO_ON) == -1)
    {
      failed_to_set_o_direct = true;
      ndbout_c("%s Failed to set DIRECTIO_ON errno: %u",
               theFileName.c_str(), errno);
    }
#ifdef VM_TRACE
    else
    {
      ndbout_c("%s DIRECTIO_ON", theFileName.c_str());
    }
#endif
  }
#endif
  
  if(use_gz)
  {
    int err;
    if((err= ndbzdopen(&nzf, theFd, new_flags)) < 1)
    {
      ndbout_c("Stewart's brain broke: %d %d %s",
               err, my_errno(), theFileName.c_str());
      abort();
    }
  }

  set_or_check_filetype(true);

  if (flags & FsOpenReq::OM_READ_SIZE)
  {
    struct stat buf;
    if ((fstat(theFd, &buf) == -1))
    {
      request->error = errno;
      close(theFd);
    }
    Uint64 size = (Uint64)buf.st_size;
    request->m_file_size_hi = Uint32(size >> 32);
    request->m_file_size_lo = Uint32(size & 0xFFFFFFFF);
  }
  else
  {
    request->m_file_size_hi = Uint32(~0);
    request->m_file_size_lo = Uint32(~0);
  }

  if (flags & FsOpenReq::OM_AUTOSYNC)
  {
#ifdef O_DIRECT
    if (!failed_to_set_o_direct ||
        !(flags & FsOpenReq::OM_DIRECT_SYNC))
    {
      m_auto_sync_freq = request->par.open.auto_sync_size;
    }
#else
    m_auto_sync_freq = request->par.open.auto_sync_size;
#endif
  }

#ifdef O_DIRECT
  if (!failed_to_set_o_direct &&
      flags & FsOpenReq::OM_DIRECT_SYNC)
  {
    /**
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
     * succesfully applied on the file. This flag will not
     * change anything on block code. The blocks are still
     * expected to issue sync flags at the same places as
     * before, but if this flag is supported, the fsync
     * call will be skipped.
     */
    m_use_o_direct_sync_flag = true;
  }
#endif

  request->m_fileinfo = get_fileinfo();
}

int PosixAsyncFile::readBuffer(Request *req, char *buf,
                               size_t size, off_t offset)
{
  int return_value;
  req->par.readWrite.pages[0].size = 0;
  off_t seek_val;
#if ! defined(HAVE_PREAD)
  FileGuard guard(this);
  if(!use_gz)
  {
    while((seek_val= lseek(theFd, offset, SEEK_SET)) == (off_t)-1
          && errno == EINTR) {};
    if(seek_val == (off_t)-1)
    {
      return errno;
    }
  }
#endif
  if(use_gz)
  {
    while((seek_val= ndbzseek(&nzf, offset, SEEK_SET)) == (off_t)-1
          && errno == EINTR) {};
    if(seek_val == (off_t)-1)
    {
      return errno;
    }
  }

  int error = 0;

  while (size > 0) {
    size_t bytes_read = 0;

#if  ! defined(HAVE_PREAD)
    if(use_gz)
      return_value = ndbzread(&nzf, buf, size, &error);
    else
      return_value = ::read(theFd, buf, size);
#else // UNIX
    if(!use_gz)
      return_value = ::pread(theFd, buf, size, offset);
    else
      return_value = ndbzread(&nzf, buf, size, &error);
#endif
    if (return_value == -1 && errno == EINTR) {
      DEBUG(ndbout_c("EINTR in read"));
      continue;
    } else if (!use_gz) {
      if (return_value == -1)
        return errno;
    }
    else if (return_value < 1 && nzf.z_eof!=1)
    {
      if(my_errno()==0 && errno==0 && error==0 && nzf.z_err==Z_STREAM_END)
        break;
      DEBUG(ndbout_c("ERROR DURING %sRead: %d off: %d from %s",(use_gz)?"gz":"",size,offset,theFileName.c_str()));
      ndbout_c("ERROR IN PosixAsyncFile::readBuffer %d %d %d %d",
               my_errno(), errno, nzf.z_err, error);
      if(use_gz)
        return my_errno();
      return errno;
    }
    bytes_read = return_value;
    req->par.readWrite.pages[0].size += bytes_read;
    if(bytes_read == 0){
      if(req->action == Request::readPartial)
      {
	return 0;
      }
      DEBUG(ndbout_c("Read underflow %d %d\n %x\n%d %d",
		     size, offset, buf, bytes_read, return_value));
      return ERR_ReadUnderflow;
    }

    if(bytes_read != size){
      DEBUG(ndbout_c("Warning partial read %d != %d on %s",
		     bytes_read, size, theFileName.c_str()));
    }

    buf += bytes_read;
    size -= bytes_read;
    offset += bytes_read;
  }
  return 0;
}

void PosixAsyncFile::readvReq(Request *request)
{
#if ! defined(HAVE_PREAD)
  readReq(request);
  return;
#else
  int return_value;
  int length = 0;
  struct iovec iov[20]; // the parameter in the signal restricts this to 20 deep
  for(int i=0; i < request->par.readWrite.numberOfPages ; i++) {
    iov[i].iov_base= request->par.readWrite.pages[i].buf;
    iov[i].iov_len= request->par.readWrite.pages[i].size;
    length = length + iov[i].iov_len;
  }
  lseek( theFd, request->par.readWrite.pages[0].offset, SEEK_SET );
  return_value = ::readv(theFd, iov, request->par.readWrite.numberOfPages);
  if (return_value == -1) {
    request->error = errno;
    return;
  } else if (return_value != length) {
    request->error = 1011;
    return;
  }
#endif
}

int PosixAsyncFile::writeBuffer(const char *buf, size_t size, off_t offset)
{
  size_t chunk_size = 256*1024;
  size_t bytes_to_write = chunk_size;
  int return_value;

  m_write_wo_sync += size;

#if ! defined(HAVE_PWRITE)
  FileGuard guard(this);
  off_t seek_val;
  while((seek_val= lseek(theFd, offset, SEEK_SET)) == (off_t)-1
	&& errno == EINTR) {};
  if(seek_val == (off_t)-1)
  {
    return errno;
  }
#endif

  while (size > 0) {
    if (size < bytes_to_write){
      // We are at the last chunk
      bytes_to_write = size;
    }
    size_t bytes_written = 0;

#if ! defined(HAVE_PWRITE)
    if(use_gz)
      return_value= ndbzwrite(&nzf, buf, bytes_to_write);
    else
      return_value = ::write(theFd, buf, bytes_to_write);
#else // UNIX
    if(use_gz)
      return_value= ndbzwrite(&nzf, buf, bytes_to_write);
    else
      return_value = ::pwrite(theFd, buf, bytes_to_write, offset);
#endif
    if (return_value == -1 && errno == EINTR) {
      bytes_written = 0;
      DEBUG(ndbout_c("EINTR in write"));
    } else if (return_value == -1 || return_value < 1){
      ndbout_c("ERROR IN PosixAsyncFile::writeBuffer %d %d %d",
               my_errno(), errno, nzf.z_err);
      if(use_gz)
        return my_errno();
      return errno;
    } else {
      bytes_written = return_value;

      if(bytes_written == 0){
        DEBUG(ndbout_c("no bytes written"));
	abort();
      }

      if(bytes_written != bytes_to_write){
	DEBUG(ndbout_c("Warning partial write %d != %d",
		 bytes_written, bytes_to_write));
      }
    }

    buf += bytes_written;
    size -= bytes_written;
    offset += bytes_written;
  }
  return 0;
}

void PosixAsyncFile::closeReq(Request *request)
{
  set_or_check_filetype(false);
  if (m_open_flags & (
      FsOpenReq::OM_WRITEONLY |
      FsOpenReq::OM_READWRITE |
      FsOpenReq::OM_APPEND )) {
    syncReq(request);
  }
  int r;
  if(use_gz)
    r= ndbzclose(&nzf);
  else
    r= ::close(theFd);
  use_gz= 0;
  Byte *a,*b;
  a= nzf.inbuf;
  b= nzf.outbuf;
  memset(&nzf,0,sizeof(nzf));
  nzf.inbuf= a;
  nzf.outbuf= b;
  nzf.stream.opaque = (void*)&nz_mempool;

  if (-1 == r) {
#ifndef DBUG_OFF
    if (theFd == -1) {
      DEBUG(ndbout_c("close on fd = -1"));
      abort();
    }
#endif
    request->error = errno;
  }
  theFd = -1;
}

bool PosixAsyncFile::isOpen(){
  return (theFd != -1);
}


void PosixAsyncFile::syncReq(Request *request)
{
  if (m_write_wo_sync == 0 ||
      m_use_o_direct_sync_flag)
  {
    /**
     * Nothing has been written since last time we sync:ed, so
     * thus no need of calling fsync.
     *
     * If we use ODirect with sync flag, we trust the OS and the
     * filesystem to have performed the sync every time we return
     * from a write. So no need to call fsync in this case.
     * We only use this feature on files that have been completely
     * written at open of the file AND files that do not change in
     * size after that.
     */
    return;
  }
  if (-1 == ::fsync(theFd)){
    request->error = errno;
    return;
  }
  m_write_wo_sync = 0;
}

void PosixAsyncFile::appendReq(Request *request)
{
  set_or_check_filetype(false);
  const char * buf = request->par.append.buf;
  Uint32 size = request->par.append.size;

  m_write_wo_sync += size;

  /* ODirectSyncFlag cannot be used with append on files */
  require(!m_use_o_direct_sync_flag);
  while(size > 0){
    int n;
    if(use_gz)
      n= ndbzwrite(&nzf,buf,size);
    else
      n= write(theFd, buf, size);
    if(n == -1 && errno == EINTR){
      continue;
    }
    if(n == -1){
      if(use_gz)
        request->error = my_errno();
      else
        request->error = errno;
      return;
    }
    if(n == 0){
      DEBUG(ndbout_c("append with n=0"));
      abort();
    }
    size -= n;
    buf += n;
  }

  if ((m_auto_sync_freq && m_write_wo_sync > m_auto_sync_freq) ||
      m_always_sync)
  {
    syncReq(request);
  }
}

void PosixAsyncFile::removeReq(Request *request)
{
  if (-1 == ::remove(theFileName.c_str())) {
    request->error = errno;

  }
}

void
PosixAsyncFile::rmrfReq(Request *request, const char * src, bool removePath)
{
  if(!request->par.rmrf.directory)
  {
    // Remove file
    if(unlink(src) != 0 && errno != ENOENT)
      request->error = errno;
    return;
  }

  char path[PATH_MAX];
  strcpy(path, src);
  strcat(path, "/");

  DIR* dirp;
  struct dirent * dp;
loop:
  dirp = opendir(path);
  if(dirp == 0)
  {
    if(errno != ENOENT)
      request->error = errno;
    return;
  }

  while ((dp = readdir(dirp)) != NULL)
  {
    if ((strcmp(".", dp->d_name) != 0) && (strcmp("..", dp->d_name) != 0)) 
    {
      int len = strlen(path);
      strcat(path, dp->d_name);
      if (remove(path) == 0)
      {
        path[len] = 0;
        continue;
      }
      
      closedir(dirp);
      strcat(path, "/");
      goto loop;
    }
  }
  closedir(dirp);
  path[strlen(path)-1] = 0; // remove /
  if (strcmp(src, path) != 0)
  {
    char * t = strrchr(path, '/');
    t[1] = 0;
    goto loop;
  }

  if(removePath && rmdir(src) != 0)
  {
    request->error = errno;
  }
}

PosixAsyncFile::~PosixAsyncFile()
{
  /* Free the read and write buffer memory used by ndbzio */
  if (nzfBufferUnaligned)
    ndbd_free(nzfBufferUnaligned,
              ndbz_bufsize_read() +
              ndbz_bufsize_write() +
              NDB_O_DIRECT_WRITE_ALIGNMENT-1);
  nzfBufferUnaligned = NULL;

  /* Free the inflate/deflate buffers for ndbzio */
  if(nz_mempool.mem)
    ndbd_free(nz_mempool.mem, nz_mempool.size);
  nz_mempool.mem = NULL;

  destroy_mutex();
}

void PosixAsyncFile::createDirectories()
{
  char* tmp;
  const char * name = theFileName.c_str();
  const char * base = theFileName.get_base_name();
  while((tmp = (char *)strstr(base, DIR_SEPARATOR)))
  {
    char t = tmp[0];
    tmp[0] = 0;
    mkdir(name, S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP);
    tmp[0] = t;
    base = tmp + sizeof(DIR_SEPARATOR);
  }
}
