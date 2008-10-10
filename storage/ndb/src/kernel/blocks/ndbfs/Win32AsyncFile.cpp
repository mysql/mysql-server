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

#include "Win32AsyncFile.hpp"

#include <ErrorHandlingMacros.hpp>
#include <kernel_types.h>
#include <ndbd_malloc.hpp>
#include <NdbThread.h>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>

Win32AsyncFile::Win32AsyncFile(SimulatedBlock& fs) :
  AsyncFile(fs),hFile(INVALID_HANDLE_VALUE)
{
}

Win32AsyncFile::~Win32AsyncFile()
{

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
  if ((flags & FsOpenReq::OM_CREATE) && (flags & FsOpenReq::OM_TRUNCATE)){
    dwCreationDisposition = CREATE_ALWAYS;
  } else if (flags & FsOpenReq::OM_TRUNCATE){
    dwCreationDisposition = TRUNCATE_EXISTING;
  } else if (flags & (FsOpenReq::OM_CREATE|FsOpenReq::OM_CREATE_IF_NONE)){
    dwCreationDisposition = CREATE_NEW;
  } else {
    dwCreationDisposition = OPEN_EXISTING;
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

  if(INVALID_HANDLE_VALUE == hFile) {
    request->error = GetLastError();
    if(((ERROR_PATH_NOT_FOUND == request->error) || (ERROR_INVALID_NAME == request->error))
		&& (flags & (FsOpenReq::OM_CREATE|FsOpenReq::OM_CREATE_IF_NONE))) {
      createDirectories();
      hFile = CreateFile(theFileName.c_str(), dwDesiredAccess, dwShareMode,
                         0, dwCreationDisposition, dwFlagsAndAttributes, 0);

      if(INVALID_HANDLE_VALUE == hFile)
        request->error = GetLastError();
      else
        request->error = 0;
    }
  }
  else {
    request->error = 0;
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
        return;
      }
      DWORD dwWritten;
      BOOL bWrite= WriteFile(hFile, buf, sizeof(buf), &dwWritten, 0);
      if(!bWrite || dwWritten!=sizeof(buf))
      {
        request->error= GetLastError();
      }
      off.QuadPart+=sizeof(buf);
    }
    off.QuadPart= 0;
    BOOL r= SetFilePointerEx(hFile, off, NULL, FILE_BEGIN);
    if(r==0)
    {
      request->error= GetLastError();
      return;
    }
  }

  return;
}

int
Win32AsyncFile::readBuffer(Request* req, char * buf, size_t size, off_t offset){
  req->par.readWrite.pages[0].size = 0;

  DWORD dwSFP = SetFilePointer(hFile, offset, 0, FILE_BEGIN);
  if(dwSFP != offset) {
    return GetLastError();
  }

  while (size > 0) {
    size_t bytes_read = 0;

    DWORD dwBytesRead;
    BOOL bRead = ReadFile(hFile,
                          buf,
                          size,
                          &dwBytesRead,
                          0);
    if(!bRead){
      return GetLastError();
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
    offset += bytes_read;
  }
  return 0;
}

int
Win32AsyncFile::writeBuffer(const char * buf, size_t size, off_t offset,
		       size_t chunk_size)
{
  size_t bytes_to_write = chunk_size;

  m_write_wo_sync += size;

  DWORD dwSFP = SetFilePointer(hFile, offset, 0, FILE_BEGIN);
  if(dwSFP != offset) {
    return GetLastError();
  }

  while (size > 0) {
    if (size < bytes_to_write){
      // We are at the last chunk
      bytes_to_write = size;
    }
    size_t bytes_written = 0;

    DWORD dwWritten;
    BOOL bWrite = WriteFile(hFile, buf, bytes_to_write, &dwWritten, 0);
    if(!bWrite) {
      return GetLastError();
    }
    bytes_written = dwWritten;
    if (bytes_written != bytes_to_write) {
      DEBUG(ndbout_c("Warning partial write %d != %d", bytes_written, bytes_to_write));
    }

    buf += bytes_written;
    size -= bytes_written;
    offset += bytes_written;
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
  if(m_auto_sync_freq && m_write_wo_sync == 0){
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
  Uint32 size = request->par.append.size;

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

  if(m_auto_sync_freq && m_write_wo_sync > m_auto_sync_freq){
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
Win32AsyncFile::rmrfReq(Request * request, char * path, bool removePath){
  Uint32 path_len = strlen(path);
  Uint32 path_max_copy = PATH_MAX - path_len;
  char* path_add = &path[path_len];

  if(!request->par.rmrf.directory){
    // Remove file
    if(!DeleteFile(path)){
      DWORD dwError = GetLastError();
      if(dwError!=ERROR_FILE_NOT_FOUND)
	request->error = dwError;
    }
    return;
  }

  strcat(path, "\\*");
  WIN32_FIND_DATA ffd;
  HANDLE hFindFile = FindFirstFile(path, &ffd);
  path[path_len] = 0;
  if(INVALID_HANDLE_VALUE==hFindFile){
    DWORD dwError = GetLastError();
    if(dwError!=ERROR_PATH_NOT_FOUND)
      request->error = dwError;
    return;
  }

  do {
    if(0!=strcmp(".", ffd.cFileName) && 0!=strcmp("..", ffd.cFileName)){
      strcat(path, "\\");
      strcat(path, ffd.cFileName);
      if(DeleteFile(path)) {
        path[path_len] = 0;
	continue;
      }//if

      rmrfReq(request, path, true);
      path[path_len] = 0;
      if(request->error != 0){
	FindClose(hFindFile);
	return;
      }
    }
  } while(FindNextFile(hFindFile, &ffd));

  FindClose(hFindFile);

  if(removePath && !RemoveDirectory(path))
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
