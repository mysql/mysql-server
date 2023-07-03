/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include "portlib/ndb_file.h"

#include "Windows.h"

#include "kernel/signaldata/FsOpenReq.hpp"

#include <cstring>
#include <cstdlib>

#ifndef require
template<const char* cond_str,const char* file,const char* func,int line>
static inline void require_fn(bool cond)
{
  if (cond)
    return;
  g_eventLogger->info("YYY: FATAL ERROR: %s: %s: %d: REQUIRE FAILED: %s", file,
                      func, line, cond_str);
  std::abort();
}
#define require(cc) require_fn<#cc,__FILE__,__func__,__LINE__>((cc))
#endif

const ndb_file::os_handle ndb_file::os_invalid_handle = INVALID_HANDLE_VALUE;

bool ndb_file::is_regular_file() const
{
  DWORD file_type = GetFileType(m_handle);
  return (file_type == FILE_TYPE_DISK);
}

int ndb_file::write_forward(const void* buf, ndb_file::size_t count)
{
  require(check_block_size_and_alignment(buf, count, get_pos()));
  const DWORD bytes_to_write = count;

  DWORD dwWritten;
  BOOL bWrite = WriteFile(m_handle, buf, bytes_to_write, &dwWritten, nullptr);
  if (!bWrite)
  {
    return -1;
  }
  assert(ndb_file::size_t(dwWritten) == count);
  if (do_sync_after_write(dwWritten) == -1) return -1;
  return dwWritten;
}

int ndb_file::write_pos(const void* buf, ndb_file::size_t count,
                        ndb_off_t offset)
{
  require(check_block_size_and_alignment(buf, count, offset));
  LARGE_INTEGER li;
  li.QuadPart = offset;

  OVERLAPPED ov;
  std::memset(&ov, 0, sizeof(ov));
  ov.Offset = li.LowPart;
  ov.OffsetHigh = li.HighPart;

  const DWORD bytes_to_write = count;

  DWORD dwWritten;
  BOOL bWrite = WriteFile(m_handle, buf, bytes_to_write, &dwWritten, &ov);
  if (!bWrite)
  {
    return -1;
  }
  assert(ndb_file::size_t(dwWritten) == count);
  if (do_sync_after_write(dwWritten) == -1) return -1;
  return dwWritten;
}

int ndb_file::read_forward(void* buf, ndb_file::size_t count) const
{
  require(check_block_size_and_alignment(buf, count, 1));
  const DWORD size = count;
  require(size > 0);
  DWORD dwBytesRead;
  BOOL bRead = ReadFile(m_handle,
                        buf,
                        size,
                        &dwBytesRead,
                        nullptr);
  if (!bRead)
  {
    int err = GetLastError();
    if (err == ERROR_HANDLE_EOF)
    {
      return 0;
    }
    return -1;
  }
  return dwBytesRead;
}

int ndb_file::read_backward(void* buf, ndb_file::size_t count) const
{
  require(check_block_size_and_alignment(buf, count, 0));
  // Current pos must be within file.
  // Current pos - count must be within file.
  // Seek -count, read should read all.
  // if partial read - fatal error!

  BOOL ret;
  LARGE_INTEGER off;
  off.QuadPart = -LONG(count);
  ret = SetFilePointerEx(m_handle, off, NULL, FILE_CURRENT);
  if (!ret)
  {
    return -1;
  }

  const DWORD size = count;
  require(size > 0);
  DWORD dwBytesRead;
  BOOL bRead = ReadFile(m_handle,
                        buf,
                        size,
                        &dwBytesRead,
                        nullptr);
  if (!bRead)
  {
    int err = GetLastError();
    if (err == ERROR_HANDLE_EOF)
    {
      return -1;
    }
    return -1;
  }
  if (dwBytesRead != size)
  {
    return -1;
  }

  ret = SetFilePointerEx(m_handle, off, NULL, FILE_CURRENT);
  if (!ret)
  {
    return -1;
  }

  return dwBytesRead;
}

int ndb_file::read_pos(void* buf, ndb_file::size_t count,
                       ndb_off_t offset) const
{
  require(check_block_size_and_alignment(buf, count, offset));
  LARGE_INTEGER li;
  li.QuadPart = offset;

  OVERLAPPED ov;
  std::memset(&ov, 0, sizeof(ov));
  ov.Offset = li.LowPart;
  ov.OffsetHigh = li.HighPart;

  const DWORD size = count;
  require(size > 0);
  DWORD dwBytesRead;
  BOOL bRead = ReadFile(m_handle,
                        buf,
                        size,
                        &dwBytesRead,
                        &ov);
  if (!bRead)
  {
    int err = GetLastError();
    if (err == ERROR_HANDLE_EOF)
    {
      return 0;
    }
    return -1;
  }
  return dwBytesRead;
}

ndb_off_t ndb_file::get_pos() const
{
  LARGE_INTEGER off;
  off.QuadPart = 0;
  BOOL ret = SetFilePointerEx(m_handle, off, &off, FILE_CURRENT);
  if (!ret)
  {
    return -1;
  }
  return off.QuadPart;
}

int ndb_file::set_pos(ndb_off_t pos) const
{
  require(check_block_size_and_alignment(nullptr, 0, pos));
  LARGE_INTEGER off;
  off.QuadPart = pos;
  BOOL ret = SetFilePointerEx(m_handle, off, NULL, FILE_BEGIN);
  if (!ret)
  {
    return -1;
  }
  return 0;
}

ndb_off_t ndb_file::get_size() const
{
  LARGE_INTEGER size;
  BOOL ret_code = GetFileSizeEx(m_handle, &size);
  if (!ret_code)
  {
    return -1;
  }
  return size.QuadPart;
}

int ndb_file::extend(ndb_off_t end, extend_flags flags) const
{
  require(check_block_size_and_alignment(nullptr, end, end));
  const ndb_off_t saved_file_pos = get_pos();
  if (saved_file_pos == -1)
  {
    return -1;
  }
  const ndb_off_t size = get_size();
  if (size == -1)
  {
    return -1;
  }
  if (size > end)
  {
    SetLastError(ERROR_INVALID_DATA);
    return -1;
  }
  if (set_pos(end) == -1)
  {
    return -1;
  }
  if (!SetEndOfFile(m_handle))
  {
    return -1;
  }
  if (flags & ZERO_FILL)
  {
    /*
     * Do not change FileValidData, which will imply zeros to be written when
     * a write occur beyond valid data-
     * Quite ok for a file where only new blocks are written at after other
     * written blocks, that is not creating "holes" in file.
     */
  }
  else
  {
    /*
     * Try to avoid zero fill.
     * Ignoring failure, which may cause holes in file to be zero filled on
     * write, but since files typically are initialized by appending or
     * writing in forward direction there should typically be no harm.
     */
    if (!SetFileValidData(m_handle, size))
    {
      SetLastError(0);
    }
  }
  if (set_pos(saved_file_pos) == -1)
  {
    return -1;
  }
  return 0;
}

int ndb_file::truncate(ndb_off_t end) const
{
  require(check_block_size_and_alignment(nullptr, end, end));
  const ndb_off_t size = get_size();
  if (size == -1)
  {
    return -1;
  }
  if (size < end)
  {
    // For extending file use extend instead.
    SetLastError(ERROR_INVALID_DATA);
    return -1;
  }
  if (set_pos(end) == -1)
  {
    return -1;
  }
  if (!SetEndOfFile(m_handle))
  {
    return -1;
  }
  return 0;
}

int ndb_file::allocate() const
{
  /* Nothing to do, blocks are already allocated for file */
  return 0;
}

int ndb_file::do_sync() const
{
  if (!FlushFileBuffers(m_handle))
  {
    return -1;
  }
  return 0;
}

int ndb_file::create(const char name[])
{
  DWORD dwCreationDisposition = CREATE_NEW;
  DWORD dwDesiredAccess = GENERIC_WRITE;
  DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

  HANDLE hFile = CreateFile(name, dwDesiredAccess, dwShareMode,
                            0, dwCreationDisposition, dwFlagsAndAttributes, 0);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    return -1;
  }
  if (!CloseHandle(hFile))
  {
    SetLastError(0);
  }
  return 0;
}

int ndb_file::remove(const char name[])
{
  return ::DeleteFile(name) ? 0 : -1;
}

int ndb_file::open(const char name[], unsigned flags)
{
  require(!is_open());

  init();

  const unsigned bad_flags = flags & ~(FsOpenReq::OM_APPEND |
      FsOpenReq::OM_READ_WRITE_MASK );

  if (bad_flags != 0) abort();

  m_open_flags = 0;
  m_write_need_sync = false;
  m_os_syncs_each_write = false;

  // for open.flags, see signal FSOPENREQ
  DWORD dwCreationDisposition;
  DWORD dwDesiredAccess = 0;
  DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

  if (flags & FsOpenReq::OM_TRUNCATE)
  {
    dwCreationDisposition = TRUNCATE_EXISTING;
  }
  else
  {
    dwCreationDisposition = OPEN_EXISTING;
  }

  // OM_APPEND not used.

  switch (flags & FsOpenReq::OM_READ_WRITE_MASK)
  {
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
    SetLastError(ERROR_INVALID_ACCESS);
    return -1;
  }

  m_handle = CreateFile(name, dwDesiredAccess, dwShareMode,
                     0, dwCreationDisposition, dwFlagsAndAttributes, 0);
  if (m_handle == INVALID_HANDLE_VALUE)
  {
    return -1;
  }

  return 0;
}

int ndb_file::close()
{
  BOOL ok = CloseHandle(m_handle);
  m_handle = INVALID_HANDLE_VALUE;
  return ok ? 0 : -1;
}

void ndb_file::invalidate()
{
  // Should never be called on Windows
  m_handle = INVALID_HANDLE_VALUE;
}

bool ndb_file::have_direct_io_support() const
{
  return false;
}

bool ndb_file::avoid_direct_io_on_append() const
{
  return false;
}

int ndb_file::set_direct_io(bool /* assume_implicit_datasync */)
{
  // Not implemented.
  return -1;
}

int ndb_file::reopen_with_sync(const char /* name */ [])
{
  if (m_os_syncs_each_write)
  {
    /*
     * If already synced on write by for example implicit by direct I/O mode no
     * further action needed.
     */
    return 0;
  }

  m_write_need_sync = true;

  return 0;
}
