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

#include "Win32AsyncFile.hpp"

#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>

#define JAM_FILE_ID 399


Win32AsyncFile::Win32AsyncFile(SimulatedBlock& fs) :
  AsyncFile(fs),hFile(INVALID_HANDLE_VALUE)
{
}

Win32AsyncFile::~Win32AsyncFile()
{

}

int
Win32AsyncFile::init()
{
  return 0;
}

void Win32AsyncFile::openReq(Request* request)
{
  m_auto_sync_freq = 0;
  m_write_wo_sync = 0;
  m_open_flags = request->par.open.flags;

  // for open.flags, see signal FSOPENREQ
  DWORD dwCreationDisposition;
  DWORD dwDesiredAccess = 0;
  DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  /**
   * FIXME:
   * Previously we had FILE_FLAG_NO_BUFFERING also set here.
   * This has similar alignment rules to O_DIRECT on 2.4 kernels.
   * which means we should obey the directio req as we can't do it
   * everywhere (this seemingly "worked" in 5.0 though), e.g. by default
   * LCP isn't aligned IO.
   */
  DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS;
  Uint32 flags = request->par.open.flags;

  // Convert file open flags from Solaris to Windows
  if ((flags & FsOpenReq::OM_CREATE) && (flags & FsOpenReq::OM_TRUNCATE))
  {
    dwCreationDisposition = CREATE_ALWAYS;
  }
  else if (flags & FsOpenReq::OM_TRUNCATE)
  {
    dwCreationDisposition = TRUNCATE_EXISTING;
  }
  else if (flags & (FsOpenReq::OM_CREATE_IF_NONE))
  {
    dwCreationDisposition = CREATE_NEW;
  }
  else if (flags & FsOpenReq::OM_CREATE)
  {
    dwCreationDisposition = OPEN_ALWAYS;
  }
  else
  {
    dwCreationDisposition = OPEN_EXISTING;
  }

  m_always_sync = false;
  if (flags & FsOpenReq::OM_SYNC)
  {
    m_always_sync = true;
  }

  switch(flags & 3){
  case FsOpenReq::OM_READONLY:
    dwDesiredAccess = GENERIC_READ;
    break;
  case FsOpenReq::OM_WRITEONLY:
    dwDesiredAccess = GENERIC_WRITE;
    break;
  case FsOpenReq::OM_READWRITE:
    dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
    break;
  default:
    request->error = 1000;
    break;
    return;
  }

  hFile = CreateFile(theFileName.c_str(), dwDesiredAccess, dwShareMode,
                     0, dwCreationDisposition, dwFlagsAndAttributes, 0);

  if(INVALID_HANDLE_VALUE == hFile)
  {
    request->error = GetLastError();

    if (ERROR_FILE_EXISTS == request->error)
    {
      if (!(flags & FsOpenReq::OM_CREATE_IF_NONE))
        abort();
      request->error = FsRef::fsErrFileExists;
      (void)CloseHandle(hFile);
      return;
    }

    if (((ERROR_PATH_NOT_FOUND == request->error) ||
        (ERROR_INVALID_NAME == request->error)) &&
        (flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE)))
    {
      createDirectories();
      hFile = CreateFile(theFileName.c_str(), dwDesiredAccess, dwShareMode,
                         0, dwCreationDisposition, dwFlagsAndAttributes, 0);

      if(INVALID_HANDLE_VALUE == hFile)
        request->error = GetLastError();
      else
        request->error = 0;
    }
  }
  else
  {
    request->error = 0;
  }

  if (flags & FsOpenReq::OM_CHECK_SIZE)
  {
    LARGE_INTEGER size;
    BOOL ret_code = GetFileSizeEx(hFile, &size);
    if (!ret_code)
    {
      request->error = GetLastError();
      (void)CloseHandle(hFile);
      return;
    }
    if (Uint64(size.QuadPart) != request->par.open.file_size)
    {
      request->error = FsRef::fsErrInvalidFileSize;
      (void)CloseHandle(hFile);
      return;
    }
  }

  if (flags & FsOpenReq::OM_INIT)
  {
    LARGE_INTEGER off;
    off.QuadPart= 0;
    LARGE_INTEGER sz;
    sz.QuadPart= request->par.open.file_size;
    char buf[4096];
    bzero(buf,sizeof(buf));
    while(off.QuadPart < sz.QuadPart)
    {
      BOOL r= SetFilePointerEx(hFile, off, NULL, FILE_BEGIN);
      if(r==0)
      {
        request->error= GetLastError();
        (void)CloseHandle(hFile);
        (void)DeleteFile(theFileName.c_str());
        return;
      }
      DWORD dwWritten;
      BOOL bWrite= WriteFile(hFile, buf, sizeof(buf), &dwWritten, 0);
      if(!bWrite || dwWritten!=sizeof(buf))
      {
        request->error= GetLastError();
        (void)CloseHandle(hFile);
        (void)DeleteFile(theFileName.c_str());
        return;
      }
      off.QuadPart+=sizeof(buf);
    }
    off.QuadPart= 0;
    BOOL r= SetFilePointerEx(hFile, off, NULL, FILE_BEGIN);
    if(r==0)
    {
      request->error= GetLastError();
      (void)CloseHandle(hFile);
      (void)DeleteFile(theFileName.c_str());
      return;
    }

    /* Write initial data */
    SignalT<25> tmp;
    Signal * signal = (Signal*)(&tmp);
    bzero(signal, sizeof(tmp));
    FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();
    Uint32 index = 0;
    Uint32 block = refToMain(request->theUserReference);
    Uint32 instance = refToInstance(request->theUserReference);

    off.QuadPart= 0;
    sz.QuadPart= request->par.open.file_size;
    while(off.QuadPart < sz.QuadPart)
    {
      req->filePointer = 0;          // DATA 0
      req->userPointer = request->theUserPointer;          // DATA 2
      req->numberOfPages = 1;        // DATA 5
      req->varIndex = index++;
      req->data.pageData[0] = m_page_ptr.i;

      m_fs.EXECUTE_DIRECT_MT(block, GSN_FSWRITEREQ, signal,
			     FsReadWriteReq::FixedLength + 1,
                             instance // wl4391_todo This EXECUTE_DIRECT is thread safe
                            );
      Uint32 size = request->par.open.page_size;
      char* buf = (char*)m_page_ptr.p;
      DWORD dwWritten;
      while(size > 0){
	BOOL bWrite= WriteFile(hFile, buf, size, &dwWritten, 0);
	if(!bWrite || dwWritten!=size)
	{
	  request->error= GetLastError();
          (void)CloseHandle(hFile);
          (void)DeleteFile(theFileName.c_str());
          return;
	}
	size -= dwWritten;
	buf += dwWritten;
      }
      if(size != 0)
      {
	int err = errno;
	request->error = err;
        (void)CloseHandle(hFile);
        (void)DeleteFile(theFileName.c_str());
	return;
      }
      off.QuadPart += request->par.open.page_size;
    }

    off.QuadPart= 0;
    r= SetFilePointerEx(hFile, off, NULL, FILE_BEGIN);
    if(r==0)
    {
      request->error= GetLastError();
      (void)CloseHandle(hFile);
      (void)DeleteFile(theFileName.c_str());
      return;
    }
  }
  if (flags & FsOpenReq::OM_READ_SIZE)
  {
    LARGE_INTEGER size;
    BOOL ret_code = GetFileSizeEx(hFile, &size);
    if (ret_code)
    {
      request->error = GetLastError();
      (void)CloseHandle(hFile);
      return;
    }
    request->m_file_size_hi = Uint32(Uint64(size.QuadPart) >> 32);
    request->m_file_size_lo = Uint32(Uint64(size.QuadPart) & 0xFFFFFFFF);
  }

  return;
}

int
Win32AsyncFile::readBuffer(Request* req, char * buf, size_t size, off_t offset)
{
  req->par.readWrite.pages[0].size = 0;

  while (size > 0) {
    size_t bytes_read = 0;

    OVERLAPPED ov;
    bzero(&ov, sizeof(ov));

    LARGE_INTEGER li;
    li.QuadPart = offset;
    ov.Offset = li.LowPart;
    ov.OffsetHigh = li.HighPart;
    
    DWORD dwBytesRead;
    BOOL bRead = ReadFile(hFile,
                          buf,
                          (DWORD)size,
                          &dwBytesRead,
                          &ov);
    if(!bRead){
      int err = GetLastError();
      if (err == ERROR_HANDLE_EOF && req->action == Request::readPartial)
      {
        return 0;
      }
      return err;
    }
    bytes_read = dwBytesRead;

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
      DEBUG(ndbout_c("Warning partial read %d != %d",
		     bytes_read, size));
    }

    buf += bytes_read;
    size -= bytes_read;
    offset += (off_t)bytes_read;
  }
  return 0;
}

int
Win32AsyncFile::writeBuffer(const char * buf, size_t size, off_t offset)
{
  size_t chunk_size = 256 * 1024;
  size_t bytes_to_write = chunk_size;

  m_write_wo_sync += size;

  while (size > 0) {
    OVERLAPPED ov;
    bzero(&ov, sizeof(ov));

    LARGE_INTEGER li;
    li.QuadPart = offset;
    ov.Offset = li.LowPart;
    ov.OffsetHigh = li.HighPart;
    
    if (size < bytes_to_write){
      // We are at the last chunk
      bytes_to_write = size;
    }
    size_t bytes_written = 0;

    DWORD dwWritten;
    BOOL bWrite = WriteFile(hFile, buf, (DWORD)bytes_to_write, &dwWritten, &ov);
    if(!bWrite) {
      return GetLastError();
    }
    bytes_written = dwWritten;
    if (bytes_written != bytes_to_write) {
      DEBUG(ndbout_c("Warning partial write %d != %d", bytes_written, bytes_to_write));
    }

    buf += bytes_written;
    size -= bytes_written;
    offset += (off_t)bytes_written;
  }
  return 0;
}

void
Win32AsyncFile::closeReq(Request * request)
{
  if (m_open_flags & (
      FsOpenReq::OM_WRITEONLY |
      FsOpenReq::OM_READWRITE |
      FsOpenReq::OM_APPEND )) {
    syncReq(request);
  }

  if(!CloseHandle(hFile)) {
    request->error = GetLastError();
  }
  hFile = INVALID_HANDLE_VALUE;
}

bool Win32AsyncFile::isOpen(){
  return (hFile != INVALID_HANDLE_VALUE);
}


void
Win32AsyncFile::syncReq(Request * request)
{
  if (m_write_wo_sync == 0)
  {
    /**
     * No need to call fsync when nothing to write.
     */
    return;
  }
  if(!FlushFileBuffers(hFile)) {
    request->error = GetLastError();
    return;
  }
  m_write_wo_sync = 0;
}

void
Win32AsyncFile::appendReq(Request * request){

  const char * buf = request->par.append.buf;
  Uint32 size = Uint32(request->par.append.size);

  m_write_wo_sync += size;

  DWORD dwWritten = 0;
  while(size > 0){
    if(!WriteFile(hFile, buf, size, &dwWritten, 0)){
      request->error = GetLastError();
      return ;
    }

    buf += dwWritten;
    size -= dwWritten;
  }

  if((m_auto_sync_freq && m_write_wo_sync > m_auto_sync_freq) ||
      m_always_sync)
  {
    syncReq(request);
  }
}

void
Win32AsyncFile::removeReq(Request * request)
{
  if(!DeleteFile(theFileName.c_str())) {
    request->error = GetLastError();
  }
}

void
Win32AsyncFile::rmrfReq(Request * request, const char * src, bool removePath){
  if (!request->par.rmrf.directory)
  {
    // Remove file
    if (!DeleteFile(src))
    {
      DWORD dwError = GetLastError();
      if (dwError != ERROR_FILE_NOT_FOUND)
	request->error = dwError;
    }
    return;
  }

  char path[PATH_MAX];
  strcpy(path, src);
  strcat(path, "\\*");

  WIN32_FIND_DATA ffd;
  HANDLE hFindFile;
loop:
  hFindFile = FindFirstFile(path, &ffd);
  if (INVALID_HANDLE_VALUE == hFindFile)
  {
    DWORD dwError = GetLastError();
    if (dwError != ERROR_PATH_NOT_FOUND)
      request->error = dwError;
    return;
  }
  path[strlen(path) - 1] = 0; // remove '*'

  do {
    if (0 != strcmp(".", ffd.cFileName) && 0 != strcmp("..", ffd.cFileName))
    {
      int len = (int)strlen(path);
      strcat(path, ffd.cFileName);
      if(DeleteFile(path) || RemoveDirectory(path)) 
      {
        path[len] = 0;
	continue;
      }//if

      FindClose(hFindFile);
      strcat(path, "\\*");
      goto loop;
    }
  } while(FindNextFile(hFindFile, &ffd));
  
  FindClose(hFindFile);
  path[strlen(path)-1] = 0; // remove '\'
  if (strcmp(src, path) != 0)
  {
    char * t = strrchr(path, '\\');
    t[1] = '*';
    t[2] = 0;
    goto loop;
  }

  if(removePath && !RemoveDirectory(src))
    request->error = GetLastError();
}

void Win32AsyncFile::createDirectories()
{
  char* tmp;
  const char * name = theFileName.c_str();
  const char * base = theFileName.get_base_name();
  while((tmp = (char *)strstr(base, DIR_SEPARATOR)))
  {
    char t = tmp[0];
    tmp[0] = 0;
    CreateDirectory(name, 0);
    tmp[0] = t;
    base = tmp + sizeof(DIR_SEPARATOR);
  }
}
