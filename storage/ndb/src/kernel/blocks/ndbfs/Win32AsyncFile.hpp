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

#ifndef Win32AsyncFile_H
#define Win32AsyncFile_H

/**
 * Win32 Implementation of AsyncFile interface
 */

#include <kernel_types.h>
#include "MemoryChannel.hpp"
#include "Filename.hpp"

#include <azlib.h>

const int ERR_ReadUnderflow = 1000;

const int WRITECHUNK = 262144;

class AsyncFile;

class Win32AsyncFile : public AsyncFile
{
  friend class Ndbfs;
public:
  Win32AsyncFile(SimulatedBlock& fs);
  ~Win32AsyncFile();

  void reportTo( MemoryChannel<Request> *reportTo );

  void execute( Request* request );

  void doStart();
  // its a thread so its always running
  void run();

  bool isOpen();

  Filename theFileName;
  Request *m_current_request, *m_last_request;
private:

  void openReq(Request *request);
  void readReq(Request *request);
  void readvReq(Request *request);
  void writeReq(Request *request);
  void writevReq(Request *request);

  void closeReq(Request *request);
  void syncReq(Request *request);
  void removeReq(Request *request);
  void appendReq(Request *request);
  void rmrfReq(Request *request, char * path, bool removePath);

  int readBuffer(Request*, char * buf, size_t size, off_t offset);
  int writeBuffer(const char * buf, size_t size, off_t offset,
		  size_t chunk_size = WRITECHUNK);

  int extendfile(Request* request);
  void createDirectories();

  HANDLE hFile;

  Uint32 m_open_flags; // OM_ flags from request to open file

  size_t m_write_wo_sync;  // Writes wo/ sync
  size_t m_auto_sync_freq; // Auto sync freq in bytes
};

#endif
