/* 
   Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef Win32AsyncFile_H
#define Win32AsyncFile_H

/**
 * Win32 Implementation of AsyncFile interface
 */

#include <kernel_types.h>
#include "AsyncFile.hpp"

class Win32AsyncFile : public AsyncFile
{
  friend class Ndbfs;
public:
  Win32AsyncFile(SimulatedBlock& fs);
  virtual ~Win32AsyncFile();

  virtual int init();
  virtual bool isOpen();
  virtual void openReq(Request *request);
  virtual void closeReq(Request *request);
  virtual void syncReq(Request *request);
  virtual void removeReq(Request *request);
  virtual void appendReq(Request *request);
  virtual void rmrfReq(Request *request, const char * path, bool removePath);

  virtual int readBuffer(Request*, char * buf, size_t size, off_t offset);
  virtual int writeBuffer(const char * buf, size_t size, off_t offset);

private:
  int extendfile(Request* request);
  void createDirectories();

  HANDLE hFile;
};

#endif
