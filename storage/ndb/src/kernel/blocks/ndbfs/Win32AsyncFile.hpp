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
#include "AsyncFile.hpp"
#include "MemoryChannel.hpp"
#include "Filename.hpp"

class Win32AsyncFile : public AsyncFile
{
  friend class Ndbfs;
public:
  Win32AsyncFile(SimulatedBlock& fs);
  ~Win32AsyncFile();

  void reportTo( MemoryChannel<Request> *reportTo );

  void execute( Request* request );

  bool isOpen();

private:

  void openReq(Request *request);
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
};

#endif
