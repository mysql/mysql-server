/* 
   Copyright (c) 2007, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PosixAsyncFile_H
#define PosixAsyncFile_H

/**
 * POSIX implementation of AsyncFile
 *
 * Also does direct IO, preallocation.
 */

#include <ndbzio.h>

#define JAM_FILE_ID 397


/**
 * PREAD/PWRITE is needed to use file != thread
 *   therefor it's defined/checked here
 */
#ifdef HAVE_BROKEN_PREAD
#undef HAVE_PWRITE
#undef HAVE_PREAD
#elif defined (HAVE_PREAD)
#define HAVE_PWRITE
#endif

class PosixAsyncFile : public AsyncFile
{
  friend class Ndbfs;
public:
  PosixAsyncFile(SimulatedBlock& fs);
  virtual ~PosixAsyncFile();

  virtual int init();
  virtual bool isOpen();

  virtual void openReq(Request *request);
  virtual void readvReq(Request *request);

  virtual void closeReq(Request *request);
  virtual void syncReq(Request *request);
  virtual void removeReq(Request *request);
  virtual void appendReq(Request *request);
  virtual void rmrfReq(Request *request, const char * path, bool removePath);

  virtual int readBuffer(Request*, char * buf, size_t size, off_t offset);
  virtual int writeBuffer(const char * buf, size_t size, off_t offset);

  virtual void createDirectories();

  virtual Uint32 get_fileinfo() const {
    Uint32 ft = (Uint32)m_filetype;
    Uint32 fd = (Uint32)theFd;
    return (ft << 16) | (fd & 0xFFFF);
  }

private:
  int theFd;
  int m_filetype;
  void set_or_check_filetype(bool set);

  int use_gz;
  ndbzio_stream nzf;
  struct ndbz_alloc_rec nz_mempool;
  void* nzfBufferUnaligned;

  int check_odirect_read(Uint32 flags, int&new_flags, int mode);
  int check_odirect_write(Uint32 flags, int&new_flags, int mode);

#ifndef HAVE_PREAD
  struct FileGuard;
  friend struct FileGuard;
  NdbMutex * m_mutex;
  void init_mutex() { m_mutex = NdbMutex_Create();}
  void destroy_mutex() { NdbMutex_Destroy(m_mutex);}

  /**
   * If dont HAVE_PREAD and using file != thread
   */
  struct FileGuard
  {
    PosixAsyncFile* m_file;
    FileGuard (PosixAsyncFile* file) : m_file(file) {
      if (m_file->getThread() == 0)
      {
        NdbMutex_Lock(m_file->m_mutex);
      }
    }
    ~FileGuard() {
      if (m_file->getThread() == 0)
      {
        NdbMutex_Unlock(m_file->m_mutex);
      }
    }
  };
#else
  void init_mutex() {}
  void destroy_mutex() {}
  struct FileGuard
  {
    FileGuard (PosixAsyncFile* file){}
    ~FileGuard () {}
  };
#endif
};


#undef JAM_FILE_ID

#endif
