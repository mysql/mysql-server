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

#ifndef PosixAsyncFile_H
#define PosixAsyncFile_H

/**
 * POSIX implementation of AsyncFile
 *
 * Also does direct IO, preallocation.
 */

#include <azlib.h>

class PosixAsyncFile : public AsyncFile
{
  friend class Ndbfs;
public:
  PosixAsyncFile(SimulatedBlock& fs);

  int init();

  bool isOpen();

  virtual void openReq(Request *request);
  virtual void readvReq(Request *request);

  virtual void closeReq(Request *request);
  virtual void syncReq(Request *request);
  virtual void removeReq(Request *request);
  virtual void appendReq(Request *request);
  virtual void rmrfReq(Request *request, char * path, bool removePath);
  void endReq();

  virtual int readBuffer(Request*, char * buf, size_t size, off_t offset);
  virtual int writeBuffer(const char * buf, size_t size, off_t offset,
		  size_t chunk_size = WRITECHUNK);

  virtual void createDirectories();

private:
  int theFd;

  Uint32 m_open_flags; // OM_ flags from request to open file

  int use_gz;
  azio_stream azf;
  struct az_alloc_rec az_mempool;

  void* theWriteBufferUnaligned;
  void* azfBufferUnaligned;

  size_t m_write_wo_sync;  // Writes wo/ sync
  size_t m_auto_sync_freq; // Auto sync freq in bytes

  int check_odirect_read(Uint32 flags, int&new_flags, int mode);
  int check_odirect_write(Uint32 flags, int&new_flags, int mode);
};

#endif
