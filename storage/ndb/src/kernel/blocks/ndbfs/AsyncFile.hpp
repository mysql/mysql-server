/*
   Copyright (c) 2003, 2020, Oracle and/or its affiliates.

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

#include "kernel/signaldata/BackupSignalData.hpp"
#include "portlib/ndb_file.h"
#include "util/ndbzio.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_zlib.h"
#include "util/ndbxfrm_buffer.h"

#include <kernel_types.h>
#include "AsyncIoThread.hpp"
#include "Filename.hpp"

#define JAM_FILE_ID 391

#ifndef _WIN32
static inline int get_last_os_error()
{
  return errno;
}

static inline void set_last_os_error(int err)
{
  errno = err;
}

#else
static inline int get_last_os_error()
{
  return GetLastError();
}

static inline void set_last_os_error(int err)
{
  SetLastError(err);
}

#endif

class AsyncFile
{
  friend class Ndbfs;
  friend class AsyncIoThread;

public:
  AsyncFile(Ndbfs& fs);
  virtual ~AsyncFile();

  int init();
  bool isOpen() const;

  Filename theFileName;
  EncryptionPasswordData m_password;
  Request *m_current_request, *m_last_request;

  void set_buffer(Uint32 rg, Ptr<GlobalPage> ptr, Uint32 cnt);
  bool has_buffer() const;
  void clear_buffer(Uint32 &rg, Ptr<GlobalPage> & ptr, Uint32 & cnt);

  AsyncIoThread* getThread() const { return m_thread;}
  bool thread_bound() const
  {
    return m_thread_bound;
  }
  void set_thread_bound(bool value)
  {
    m_thread_bound = value;
  }

private:

  /**
   * Implementers of AsyncFile interface
   * should implement the following
   */

  /**
   * openReq() - open a file.
   */
  void openReq(Request *request);

  /**
   * readBuffer - read into buffer
   */
  int readBuffer(Request*, char * buf, size_t size, off_t offset);

  /**
   * writeBuffer() - write into file
   */
  int writeBuffer(const char * buf, size_t size, off_t offset);

  void closeReq(Request *request);
  void syncReq(Request *request);
  virtual void removeReq(Request *request)=0;
  void appendReq(Request *request);
  virtual void rmrfReq(Request *request, const char * path, bool removePath)=0;
  virtual void createDirectories()=0;

protected:
  /**
   * These calls readBuffer and writeBuffer respectively, implement them instead.
   */
  void readReq(Request *request);
  void writeReq(Request *request);

  int ndbxfrm_append(Request* request, ndbxfrm_input_iterator* in);
private:
  void attach(AsyncIoThread* thr);
  void detach(AsyncIoThread* thr);
  bool check_odirect_request(const char* buf, size_t sz, off_t offset);

  AsyncIoThread* m_thread; // For bound files
  // Whether this file is one that will be/is bound to a thread
  bool m_thread_bound;

protected:
  using byte = unsigned char;
  ndb_file m_file;

  Uint32 m_open_flags;

  int use_gz;
  ndbzio_stream nzf;
  struct ndbz_alloc_rec nz_mempool;
  void* nzfBufferUnaligned;

  ndb_zlib zlib;
  int use_enc;
  ndb_openssl_evp openssl_evp;
  ndb_openssl_evp::operation openssl_evp_op;

  enum { FF_UNKNOWN, FF_RAW, FF_AZ31, FF_NDBXFRM1 } m_file_format;
  unsigned long m_crc32;
  unsigned long m_data_size;
  ndbxfrm_buffer m_compress_buffer;
  ndbxfrm_buffer m_encrypt_buffer;

  /**
   * file buffers
   */
  Uint32 m_resource_group;
  Uint32 m_page_cnt;
  Ptr<GlobalPage> m_page_ptr;

  char* theWriteBuffer;
  Uint32 theWriteBufferSize;

public:
  Ndbfs& m_fs;
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

inline
bool
AsyncFile::isOpen() const
{
  return m_file.is_open();
}

#undef JAM_FILE_ID

#endif
