/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef AsyncFile_H
#define AsyncFile_H

#include <kernel_types.h>
#include "AsyncIoThread.hpp"
#include "Filename.hpp"

#define JAM_FILE_ID 391


class AsyncFile
{
  friend class Ndbfs;
  friend class AsyncIoThread;

public:
  AsyncFile(SimulatedBlock& fs);
  virtual ~AsyncFile() {};

  virtual int init() = 0;
  virtual bool isOpen() = 0;

  Filename theFileName;
  Request *m_current_request, *m_last_request;

  void set_buffer(Uint32 rg, Ptr<GlobalPage> ptr, Uint32 cnt);
  bool has_buffer() const;
  void clear_buffer(Uint32 &rg, Ptr<GlobalPage> & ptr, Uint32 & cnt);

  AsyncIoThread* getThread() const { return m_thread;}

  virtual Uint32 get_fileinfo() const { return 0; }
private:

  /**
   * Implementers of AsyncFile interface
   * should implement the following
   */

  /**
   * openReq() - open a file.
   */
  virtual void openReq(Request *request) = 0;

  /**
   * readBuffer - read into buffer
   */
  virtual int readBuffer(Request*, char * buf, size_t size, off_t offset)=0;

  /**
   * writeBuffer() - write into file
   */
  virtual int writeBuffer(const char * buf, size_t size, off_t offset)=0;

  virtual void closeReq(Request *request)=0;
  virtual void syncReq(Request *request)=0;
  virtual void removeReq(Request *request)=0;
  virtual void appendReq(Request *request)=0;
  virtual void rmrfReq(Request *request, const char * path, bool removePath)=0;
  virtual void createDirectories()=0;

  /**
   * Unlikely to need to implement these. readvReq for iovec
   */
protected:
  virtual void readReq(Request *request);
  virtual void readvReq(Request *request);

  /**
   * Unlikely to need to implement these, writeBuffer likely sufficient.
   * writevReq for iovec (not yet used)
   */
  virtual void writeReq(Request *request);
  virtual void writevReq(Request *request);

private:
  void attach(AsyncIoThread* thr);
  void detach(AsyncIoThread* thr);

  AsyncIoThread* m_thread; // For bound files

protected:
  size_t m_write_wo_sync;  // Writes wo/ sync
  size_t m_auto_sync_freq; // Auto sync freq in bytes
  bool m_always_sync; /* O_SYNC not supported, then use this flag */
  Uint32 m_open_flags;

  /**
   * file buffers
   */
  Uint32 m_resource_group;
  Uint32 m_page_cnt;
  Ptr<GlobalPage> m_page_ptr;

  char* theWriteBuffer;
  Uint32 theWriteBufferSize;

public:
  SimulatedBlock& m_fs;
};

inline
void
AsyncFile::set_buffer(Uint32 rg, Ptr<GlobalPage> ptr, Uint32 cnt)
{
  assert(!has_buffer());
  m_resource_group = rg;
  m_page_ptr = ptr;
  m_page_cnt = cnt;
  theWriteBuffer = (char*)ptr.p;
  theWriteBufferSize = cnt * sizeof(GlobalPage);
}

inline
bool
AsyncFile::has_buffer() const
{
  return m_page_cnt > 0;
}

inline
void
AsyncFile::clear_buffer(Uint32 & rg, Ptr<GlobalPage> & ptr, Uint32 & cnt)
{
  assert(has_buffer());
  rg = m_resource_group;
  ptr = m_page_ptr;
  cnt = m_page_cnt;
  m_resource_group = RNIL;
  m_page_cnt = 0;
  m_page_ptr.setNull();
  theWriteBuffer = 0;
  theWriteBufferSize = 0;
}


#undef JAM_FILE_ID

#endif
