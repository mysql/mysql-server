/*
   Copyright (c) 2003, 2021, Oracle and/or its affiliates.

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

#include "my_thread_local.h" // my_errno
#include "util/ndbzio.h"
#include "util/ndb_math.h"
#include "util/ndb_az31.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_zlib.h"

#include "AsyncFile.hpp"
#include "Ndbfs.hpp"

#include <ErrorHandlingMacros.hpp>
#include <kernel_types.h>
#include <ndbd_malloc.hpp>
#include <NdbThread.h>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <Configuration.hpp>

#define JAM_FILE_ID 387

//#define DUMMY_PASSWORD
//#define DEBUG_ODIRECT

AsyncFile::AsyncFile(Ndbfs& fs) :
  theFileName(),
  m_thread_bound(false),
  use_gz(0),
  use_enc(0),
  openssl_evp_op(&openssl_evp),
  m_file_format(FF_UNKNOWN),
  m_fs(fs)
{
  m_thread = 0;

  m_resource_group = RNIL;
  m_page_cnt = 0;
  m_page_ptr.setNull();
  theWriteBuffer = 0;
  theWriteBufferSize = 0;

  memset(&nzf,0,sizeof(nzf));
}

AsyncFile::~AsyncFile()
{
  /* Free the read and write buffer memory used by ndbzio */
  if (nzfBufferUnaligned)
    ndbd_free(nzfBufferUnaligned,
              ndbz_bufsize_read() +
              ndbz_bufsize_write() +
              NDB_O_DIRECT_WRITE_ALIGNMENT-1);
  nzfBufferUnaligned = NULL;

  /* Free the inflate/deflate buffers for ndbzio */
  if(nz_mempool.mem)
    ndbd_free(nz_mempool.mem, nz_mempool.size);
  nz_mempool.mem = NULL;
  // TODO Unset memory for zlib
}

int AsyncFile::init()
{
  /*
    Preallocate read and write buffers for ndbzio to workaround
    default behaviour of alloc/free at open/close
  */
  const size_t read_size = ndbz_bufsize_read();
  const size_t write_size = ndbz_bufsize_write();

  nzfBufferUnaligned= ndbd_malloc(read_size + write_size +
                                  NDB_O_DIRECT_WRITE_ALIGNMENT-1);
  nzf.inbuf= (Byte*)(((UintPtr)nzfBufferUnaligned
                      + NDB_O_DIRECT_WRITE_ALIGNMENT - 1) &
                     ~(UintPtr)(NDB_O_DIRECT_WRITE_ALIGNMENT - 1));
  nzf.outbuf= nzf.inbuf + read_size;

  /* Preallocate inflate/deflate buffers for ndbzio */
  const size_t inflate_size = ndbz_inflate_mem_size();
  if (inflate_size == SIZE_T_MAX)
    return -1;
  const size_t deflate_size = ndbz_deflate_mem_size();
  if (deflate_size == SIZE_T_MAX)
    return -1;
  nz_mempool.size = nz_mempool.mfree = inflate_size + deflate_size;
  if (nz_mempool.size < zlib.MEMORY_NEED)
  {
    nz_mempool.size = nz_mempool.mfree = zlib.MEMORY_NEED;
  }

  ndbout_c("NDBFS/AsyncFile: Allocating %u for In/Deflate buffer",
           (unsigned int)nz_mempool.size);
  nz_mempool.mem = (char*) ndbd_malloc(nz_mempool.size);

  nzf.stream.opaque= &nz_mempool;

  zlib.set_memory(nz_mempool.mem, nz_mempool.size);
  return 0;
}

void
AsyncFile::attach(AsyncIoThread* thr)
{
#if 0
  ndbout_c("%p:%s attach to %p (m_thread: %p)", this, theFileName.c_str(), thr,
             m_thread);
#endif
  assert(m_thread_bound);
  assert(m_thread == 0);
  m_thread = thr;
}

void
AsyncFile::detach(AsyncIoThread* thr)
{
#if 0
  ndbout_c("%p:%s detach from %p", this, theFileName.c_str(), thr);
#endif
  assert(m_thread_bound);
  assert(m_thread == thr);
  m_thread = 0;
}

void
AsyncFile::openReq(Request * request)
{
  m_compress_buffer.init();
  m_encrypt_buffer.init();
  openssl_evp.reset();

  // For open.flags, see signal FSOPENREQ
  m_open_flags = request->par.open.flags;
  Uint32 flags = m_open_flags;

  // Validate some flag combination.

  // Not both OM_INIT and OM_GZ
  require(!(flags & FsOpenReq::OM_INIT) ||
          !(flags & FsOpenReq::OM_GZ));

  // OM_DIRECT_SYNC is not valid without OM_DIRECT
  require(!(flags & FsOpenReq::OM_DIRECT_SYNC) ||
          (flags & FsOpenReq::OM_DIRECT));

  // Create file
  bool created = false;
  if (flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE))
  {
    if (m_file.create(theFileName.c_str()) == -1)
    {
      int error = get_last_os_error();
      int ndbfs_error = Ndbfs::translateErrno(error);
      if (ndbfs_error == FsRef::fsErrFileDoesNotExist)
      {
        // Assume directories are missing, create directories and try again.
        createDirectories();
        if (m_file.create(theFileName.c_str()) == -1)
        {
          error = get_last_os_error();
          ndbfs_error = Ndbfs::translateErrno(error);
        }
        else
        {
          created = true;
        }
      }
      if (!created &&
          ((flags & FsOpenReq::OM_CREATE_IF_NONE) ||
           Ndbfs::translateErrno(error) != FsRef::fsErrFileExists))
      {
        request->error = error;
        return;
      }
    }
    else
    {
      created = true;
    }
  }

  // Open file (OM_READ_WRITE_MASK, OM_APPEND)
  constexpr Uint32 open_flags =
      FsOpenReq::OM_READ_WRITE_MASK | FsOpenReq::OM_APPEND;
  if (m_file.open(theFileName.c_str(), flags & open_flags) == -1)
  {
    request->error = get_last_os_error();
    goto remove_if_created;
  }

  // Truncate if OM_TRUNCATE
  if (!created && (flags & FsOpenReq::OM_TRUNCATE))
  {
    if (m_file.truncate(0) == -1)
    {
      request->error = get_last_os_error();
      m_file.close();
      goto remove_if_created;
    }
  }

  // Verify file size (OM_CHECK_SIZE)
  if (flags & FsOpenReq::OM_CHECK_SIZE)
  {
    ndb_file::off_t file_size = m_file.get_size();
    if (file_size == -1)
    {
      request->error = get_last_os_error();
    }
    else if ((Uint64)file_size != request->par.open.file_size)
    {
      request->error = FsRef::fsErrInvalidFileSize;
    }
    if (request->error)
    {
      m_file.close();
require(request->error==0);
      goto remove_if_created;
    }
  }

  // Turn on direct io (OM_DIRECT, OM_DIRECT_SYNC)
  if (flags & FsOpenReq::OM_DIRECT)
  {
    /* TODO:
     * Size and alignment should be passed in request.
     * And also checked in ndb_file append/write/read/set_pos/truncate/extend.
     */
    m_file.set_block_size_and_alignment(NDB_O_DIRECT_WRITE_BLOCKSIZE,
                                        NDB_O_DIRECT_WRITE_ALIGNMENT);

    /*
     * Initializing file may write lots of pages sequentially.  Some
     * implementation of direct io should be avoided in that case and
     * direct io should be turned on after initialization.
     */
    if (m_file.have_direct_io_support() && !m_file.avoid_direct_io_on_append())
    {
      const bool direct_sync = flags & FsOpenReq::OM_DIRECT_SYNC;
      if (m_file.set_direct_io(direct_sync) == -1)
      {
        ndbout_c("%s Failed to set ODirect errno: %u",
                 theFileName.c_str(), get_last_os_error());
      }
#ifdef DEBUG_ODIRECT
      else
      {
        ndbout_c("%s ODirect is set.", theFileName.c_str());
      }
#endif
    }
  }

  // Initialise file if OM_INIT

  if (flags & FsOpenReq::OM_INIT)
  {
    require(m_file_format == FF_UNKNOWN);
    m_file_format = FF_RAW; // TODO also allow NDBXFRM1 for encrypted
    m_file.set_autosync(1024 * 1024);

    // Extend file size
    require(request->par.open.file_size <= ndb_file::OFF_T_MAX);
    const ndb_file::off_t file_size = (ndb_file::off_t)request->par.open.file_size;
    if (m_file.extend(file_size, ndb_file::NO_FILL) == -1)
    {
      request->error = get_last_os_error();
      m_file.close();
require(!"m_file.extend");
      goto remove_if_created;
    }

    // Reserve disk blocks for whole file
    if (m_file.allocate() == -1)
    {
      // If fail, ignore, will try to write file anyway.
    }

    // Initialise blocks
    ndb_file::off_t off = 0;
    FsReadWriteReq req[1];

    Uint32 index = 0;

#ifdef VM_TRACE
#define TRACE_INIT
#endif

#ifdef TRACE_INIT
    Uint32 write_cnt = 0;
    const NDB_TICKS start = NdbTick_getCurrentTicks();
#endif
    require(m_file.get_pos() == 0);
    while (off < file_size)
    {
      ndb_file::off_t size = 0;
      Uint32 cnt = 0;
      while (cnt < m_page_cnt && (off + size) < file_size)
      {
        req->filePointer = 0;          // DATA 0
        req->userPointer = request->theUserPointer;          // DATA 2
        req->numberOfPages = 1;        // DATA 5
        req->varIndex = index++;
        req->data.pageData[0] = m_page_ptr.i + cnt;

        m_fs.callFSWRITEREQ(request->theUserReference, req);

        cnt++;
        size += request->par.open.page_size;
      }
      ndb_file::off_t save_size = size;
      char* buf = (char*)m_page_ptr.p;
      while (size > 0)
      {
#ifdef TRACE_INIT
        write_cnt++;
#endif
        int n;
        n = m_file.write_forward(buf, size);
        if (n == -1 || n == 0)
        {
          ndbout_c("write returned %d: errno: %d my_errno: %d",
                   n, get_last_os_error(), my_errno());
          break;
        }
        size -= n;
        buf += n;
      }
      if (size != 0)
      {
        request->error = get_last_os_error();
        m_file.close();
require(size==0);
        goto remove_if_created;
        return;
      }
      off += save_size;
    }
    if (m_file.sync() == -1)
    {
        request->error = get_last_os_error();
        m_file.close();
require(!"m_file.sync() != -1");
        goto remove_if_created;
        return;
    }
#ifdef TRACE_INIT
    const NDB_TICKS stop = NdbTick_getCurrentTicks();
    Uint64 diff = NdbTick_Elapsed(start, stop).milliSec();
    if (diff == 0)
      diff = 1;
    ndbout_c("wrote %umb in %u writes %us -> %ukb/write %umb/s",
             Uint32(file_size / (1024 * 1024)),
             write_cnt,
             Uint32(diff / 1000),
             Uint32(file_size / 1024 / write_cnt),
             Uint32(file_size / diff));
#endif

    if (m_file.set_pos(0) == -1)
    {
      request->error = get_last_os_error();
      m_file.close();
      goto remove_if_created;
    }

    m_file.set_autosync(0);
  }

#if defined(_WIN32)
  /*
   * Make sure compression is ignored on Windows as before for LCP until
   * supported.
   */
  if ((flags & FsOpenReq::OM_GZ) &&
      theFileName.get_base_path_spec() == FsOpenReq::BP_FS)
  {
    flags &= ~FsOpenReq::OM_GZ;
  }
#endif 

  // Set flags for compression (OM_GZ) and encryption (OM_ENCRYPT)
  use_gz = (flags & FsOpenReq::OM_GZ);
  use_enc = (flags & FsOpenReq::OM_ENCRYPT);
#if defined(DUMMY_PASSWORD)
  use_enc = (theFileName.get_base_path_spec() == FsOpenReq::BP_BACKUP);
#endif

  // Turn on direct io (OM_DIRECT, OM_DIRECT_SYNC) after init
  if (flags & FsOpenReq::OM_DIRECT)
  {
    if (m_file.have_direct_io_support() && m_file.avoid_direct_io_on_append())
    {
      const bool direct_sync = flags & FsOpenReq::OM_DIRECT_SYNC;
      if (m_file.set_direct_io(direct_sync) == -1)
      {
        ndbout_c("%s Failed to set ODirect errno: %u",
                 theFileName.c_str(), get_last_os_error());
      }
#ifdef DEBUG_ODIRECT
      else
      {
        ndbout_c("%s ODirect is set.", theFileName.c_str());
      }
#endif
    }
  }

  // Turn on synchronous mode (OM_SYNC)
  if (flags & FsOpenReq::OM_SYNC)
  {
    if (m_file.reopen_with_sync(theFileName.c_str()) == -1)
    {
      /*
       * reopen_with_sync should always succeed, if file can not be open in
       * sync mode, explicit call to fsync/FlushFiles will be done on every
       * write.
       */
      request->error = get_last_os_error();
      m_file.close();
      goto remove_if_created;
    }
  }

  // Read file size
  if (flags & FsOpenReq::OM_READ_SIZE)
  {
    // TODO yyy Typically fixed size files, not gzipped. And not init:ed?
    require(m_file_format == FF_UNKNOWN);
    m_file_format = FF_RAW; // TODO allow NDBXFRM1 for encypted
    ndb_file::off_t file_size = m_file.get_size();
    if (file_size == -1)
    {
      request->error = get_last_os_error();
      m_file.close();
      goto remove_if_created;
    }
    request->m_file_size_hi = Uint32(file_size >> 32);
    request->m_file_size_lo = Uint32(file_size & 0xFFFFFFFF);
  }
  else
  {
    request->m_file_size_hi = Uint32(~0);
    request->m_file_size_lo = Uint32(~0);
  }

  // Turn on compression (OM_GZ) and encryption (OM_ENCRYPT)
  if (use_gz || use_enc)
  {
    require(m_file_format == FF_UNKNOWN);
    int err;
    int ndbz_flags = 0;
    if (flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE))
    {
      ndbz_flags |= O_CREAT;
    }
    if (flags & FsOpenReq::OM_TRUNCATE)
    {
      ndbz_flags |= O_TRUNC;
    }
    if (flags & FsOpenReq::OM_APPEND)
    {
      ndbz_flags |= O_APPEND;
    }
    switch (flags & FsOpenReq::OM_READ_WRITE_MASK)
    {
    case FsOpenReq::OM_READONLY:
      ndbz_flags |= O_RDONLY;
      break;
    case FsOpenReq::OM_WRITEONLY:
      ndbz_flags |= O_WRONLY;
      break;
    case FsOpenReq::OM_READWRITE:
      ndbz_flags |= O_RDWR;
      break;
    default:
      request->error = FsRef::fsErrInvalidParameters;
      m_file.close();
      goto remove_if_created;
    }
    m_crc32 = crc32(0L, nullptr, 0);
    m_data_size = 0;
    if (flags & FsOpenReq::OM_APPEND)
    { // WRITE compressed (BACKUP, LCP)
      m_file_format = (use_enc ? FF_NDBXFRM1 : FF_AZ31);
      int rv = 0;
      if (use_gz)
      {
        rv = zlib.deflate_init();
      }
      if (rv == -1)
      {
        request->error = FsRef::fsErrInvalidParameters; // TODO better error!
        m_file.close();
      }
    }
    else if ((flags & FsOpenReq::OM_READ_WRITE_MASK) == FsOpenReq::OM_READONLY)
    { // READ compressed (LCP)
#if !defined(_WIN32)
      require(!use_enc);
      m_file_format = FF_RAW;
      if ((err= ndbzdopen(&nzf, m_file.get_os_handle(), ndbz_flags)) < 1)
      {
        ndbout_c("Stewart's brain broke: %d %d %s",
                 err, my_errno(), theFileName.c_str());
        require(!"ndbzdopen");
      }
#else
      // Compressed LCP files are not yet supported on Windows.
      abort();
#endif
    }
    else
    {
      // Compression and encryption only for appendable files
      abort();
    }
  }

  // Turn on autosync mode (OM_AUTOSYNC auto_sync_size)
  if (flags & FsOpenReq::OM_AUTOSYNC)
  {
    m_file.set_autosync(request->par.open.auto_sync_size);
  }

  if (m_file_format == FF_UNKNOWN)
    m_file_format = FF_RAW;

  if (m_file_format == FF_AZ31)
  {
    require(!use_enc);
    require(use_gz);
    require(flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE));
    ndbxfrm_output_iterator out = m_compress_buffer.get_output_iterator();
    require(ndb_az31::write_header(&out) == 0);
    m_compress_buffer.update_write(out);
  }
  else if (m_file_format == FF_NDBXFRM1)
  {
    require(flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE));
    ndbxfrm_buffer* file_buffer = (use_enc ? &m_encrypt_buffer : &m_compress_buffer);
    ndbxfrm_output_iterator out = file_buffer->get_output_iterator();
    ndb_ndbxfrm1::header ndbxfrm1;
    ndbxfrm1.set_file_block_size(NDB_O_DIRECT_WRITE_ALIGNMENT);
    if (use_gz)
    {
      ndbxfrm1.set_compression_method(1 /* deflate */);
    }
    if (use_enc)
    {
      openssl_evp.set_aes_256_cbc(true, 0);
      ndbxfrm1.set_encryption_cipher(1 /* CBC-STREAM */);
      ndbxfrm1.set_encryption_padding(1 /* ON PKCS */);

      byte salt[ndb_openssl_evp::SALT_LEN];
      openssl_evp.generate_salt256(salt);
      ndbxfrm1.set_encryption_salts(salt, ndb_openssl_evp::SALT_LEN, 1);
      Uint32 kdf_iter_count = ndb_openssl_evp::DEFAULT_KDF_ITER_COUNT;
#if !defined(DUMMY_PASSWORD)
      int pwd_len = m_password.password_length;
      ndb_openssl_evp::byte* pwd =
        reinterpret_cast<ndb_openssl_evp::byte*>(m_password.encryption_password);
#else
      ndb_openssl_evp::byte pwd[6]="DUMMY";
      int pwd_len = 5;
#endif
      openssl_evp.derive_and_add_key_iv_pair(pwd, pwd_len, kdf_iter_count, salt);
      ndbxfrm1.set_encryption_kdf(1 /* pbkdf2_sha256 */);
      ndbxfrm1.set_encryption_kdf_iter_count(kdf_iter_count);
      int rv = openssl_evp_op.encrypt_init(0, 0);
      require(rv == 0);
    }
    require(ndbxfrm1.prepare_for_write() == 0);
    require(ndbxfrm1.get_size() <= out.size());
    require(ndbxfrm1.write_header(&out) == 0);
    file_buffer->update_write(out);
  }

  require(request->error == 0);
  return;

remove_if_created:
  m_file_format = FF_UNKNOWN;
  if (created && m_file.remove(theFileName.c_str()) == -1)
  {
    ndbout_c("Could not remove '%s' (err %u) after open failure (err %u).\n",
             theFileName.c_str(),
             get_last_os_error(),
             request->error);
  }
}

void
AsyncFile::closeReq(Request *request)
{
  // If closeRemove no final write or sync is needed!
  bool no_write = (request->action & Request::closeRemove);
  if (m_open_flags & (
      FsOpenReq::OM_WRITEONLY |
      FsOpenReq::OM_READWRITE |
      FsOpenReq::OM_APPEND )) {
    if (!no_write) syncReq(request);
  }
  int r = 0;
#ifndef NDEBUG
  if (!m_file.is_open())
  {
    DEBUG(ndbout_c("close on already closed file"));
    abort();
  }
#endif
  if(use_gz || use_enc)
  {
    if (m_open_flags & (
      FsOpenReq::OM_WRITEONLY |
      FsOpenReq::OM_READWRITE |
      FsOpenReq::OM_APPEND ))
    { // APPEND backup
      require(m_open_flags & FsOpenReq::OM_APPEND);
      if (!no_write)
      {
        require((m_file_format == FF_AZ31) ||
                (m_file_format == FF_NDBXFRM1));

        ndbxfrm_input_iterator in(nullptr, nullptr, true);

        /*
         * In some cases flushing zlib::deflate() did not flush out and even
         * returned without outputting anything, and needed a second call
         * requesting flush.
         */
        for (int Guard = 5; Guard > 0; Guard--)
        {
          int r = ndbxfrm_append(request, &in);
          if (r == -1)
          {
            request->error = get_last_os_error();
            if (request->error == 0)
            {
              request->error = FsRef::fsErrUnknown;
            }
          }
          if (r == 0) break;
        }
        if (r != 0 || !m_compress_buffer.last())
        {
          request->error = get_last_os_error();
          if (request->error == 0)
          {
            request->error = FsRef::fsErrUnknown;
          }
        }
      }
      if (use_gz)
      {
        int r = zlib.deflate_end();
        if (!no_write) require(r == 0);
      }
      if (use_enc)
      {
        int r = openssl_evp_op.encrypt_end();
        if (!no_write) require(r == 0);
      }
    }
    else
    { // READ lcp
      r= ndbzclose(&nzf);
      m_file.invalidate();
    }
  }
  if (m_open_flags & (FsOpenReq::OM_APPEND ) && !no_write)
  {
    if (m_file_format == FF_NDBXFRM1)
    {
      require(m_open_flags & FsOpenReq::OM_APPEND);
      require(m_file.is_open());

      if (use_enc)
      {
        ndbxfrm_input_iterator wr_in = m_compress_buffer.get_input_iterator();
        require(wr_in.empty());
        require(wr_in.last());
      }
      ndbxfrm_buffer* file_buffer = (use_enc ? &m_encrypt_buffer : &m_compress_buffer);
      file_buffer->clear_last();
      off_t file_pos = m_file.get_pos();
      {
        ndbxfrm_input_iterator wr_in = file_buffer->get_input_iterator();
        file_pos += wr_in.size();
      }
      ndb_ndbxfrm1::trailer ndbxfrm1;
      require(ndbxfrm1.set_data_size(m_data_size) == 0);
      require(ndbxfrm1.set_data_crc32(m_crc32) == 0);
      require(ndbxfrm1.set_file_pos(file_pos) == 0);
      require(ndbxfrm1.set_file_block_size(NDB_O_DIRECT_WRITE_ALIGNMENT) == 0);
      require(ndbxfrm1.prepare_for_write() == 0);
  
      ndbxfrm_output_iterator out = file_buffer->get_output_iterator();
      if (out.size() < ndbxfrm1.get_size())
      {
        file_buffer->rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
        out = file_buffer->get_output_iterator();
      }
      require(out.size() >= ndbxfrm1.get_size());
      require(ndbxfrm1.write_trailer(&out) == 0);
      file_buffer->update_write(out);

      ndbxfrm_input_iterator wr_in = file_buffer->get_input_iterator();
      size_t write_len = wr_in.size();
      require(write_len % NDB_O_DIRECT_WRITE_ALIGNMENT == 0);

      int n = m_file.append(wr_in.cbegin(), write_len);
      if (n > 0) wr_in.advance(n);
      if (n == -1 || size_t(n) != write_len)
      {
        request->error = get_last_os_error();
      }
      if (wr_in.empty()) wr_in.set_last();
      require(wr_in.last());
      file_buffer->update_read(wr_in);
      syncReq(request);
    }
    if (m_file_format == FF_AZ31)
    {
      require(m_open_flags & FsOpenReq::OM_APPEND);
      require(m_file.is_open());
      require(m_compress_buffer.last());
      m_compress_buffer.clear_last();
      ndbxfrm_output_iterator out = m_compress_buffer.get_output_iterator();
      ndb_az31 az31;
      const size_t trailer_size = az31.get_trailer_size();
      if (out.size() < trailer_size)
      {
        m_compress_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
        out = m_compress_buffer.get_output_iterator();
      }
      require(out.size() >= trailer_size);
      /*
       * Since m_compress_buffer size is multiple of 512, and buffer only
       * "wrap" when completely full we can use out.size() to determine amount
       * of padding needed
       */
      require(az31.set_data_size(m_data_size) == 0);
      require(az31.set_data_crc32(m_crc32) == 0);
      size_t pad_len =
          (out.size() - trailer_size) % NDB_O_DIRECT_WRITE_ALIGNMENT;
      require(az31.write_trailer(&out, pad_len) == 0);
      m_compress_buffer.update_write(out);

      ndbxfrm_input_iterator wr_in = m_compress_buffer.get_input_iterator();
      size_t write_len = wr_in.size();
      require(write_len % NDB_O_DIRECT_WRITE_ALIGNMENT == 0);

      int n = m_file.append(wr_in.cbegin(), write_len);
      if (n > 0) wr_in.advance(n);
      if (n == -1 || size_t(n) != write_len)
      {
        request->error = get_last_os_error();
      }
      if (wr_in.empty()) wr_in.set_last();
      require(wr_in.last());
      m_compress_buffer.update_read(wr_in);
      syncReq(request);
    }
  }
  if (m_file.is_open())
  {
    r= m_file.close();
  }
  m_file_format = FF_UNKNOWN;
  use_gz= 0;
  use_enc = 0;
  Byte *a,*b;
  a= nzf.inbuf;
  b= nzf.outbuf;
  memset(&nzf,0,sizeof(nzf));
  nzf.inbuf= a;
  nzf.outbuf= b;
  nzf.stream.opaque = (void*)&nz_mempool;

  if (-1 == r) {
    request->error = get_last_os_error();
  }
}

void
AsyncFile::readReq( Request * request)
{
  for(int i = 0; i < request->par.readWrite.numberOfPages ; i++)
  {
    off_t offset = request->par.readWrite.pages[i].offset;
    size_t size  = request->par.readWrite.pages[i].size;
    char * buf   = request->par.readWrite.pages[i].buf;

    int err = readBuffer(request, buf, size, offset);
    if(err != 0){
      request->error = err;
      return;
    }
  }
}

void
AsyncFile::writeReq(Request * request)
{
  const Uint32 cnt = request->par.readWrite.numberOfPages;
  if (theWriteBuffer == 0 || cnt == 1)
  {
    for (Uint32 i = 0; i<cnt; i++)
    {
      int err = writeBuffer(request->par.readWrite.pages[i].buf,
                            request->par.readWrite.pages[i].size,
                            request->par.readWrite.pages[i].offset);
      if (err)
      {
        request->error = err;
        return;
      }
    }
    if (m_file.sync_on_write() == -1)
    {
      request->error = get_last_os_error();
    }
    return;
  }

  {
    int page_num = 0;
    bool write_not_complete = true;

    while(write_not_complete) {
      size_t totsize = 0;
      off_t offset = request->par.readWrite.pages[page_num].offset;
      char* bufptr = theWriteBuffer;

      write_not_complete = false;
      if (request->par.readWrite.numberOfPages > 1) {
        off_t page_offset = offset;

        // Multiple page write, copy to buffer for one write
        for(int i=page_num; i < request->par.readWrite.numberOfPages; i++) {
          memcpy(bufptr,
                 request->par.readWrite.pages[i].buf,
                 request->par.readWrite.pages[i].size);
          bufptr += request->par.readWrite.pages[i].size;
          totsize += request->par.readWrite.pages[i].size;
          if (((i + 1) < request->par.readWrite.numberOfPages)) {
            // There are more pages to write
            // Check that offsets are consequtive
            off_t tmp=(off_t)(page_offset+request->par.readWrite.pages[i].size);
            if (tmp != request->par.readWrite.pages[i+1].offset) {
              // Next page is not aligned with previous, not allowed
              DEBUG(ndbout_c("Page offsets are not aligned"));
              request->error = EINVAL;
              return;
            }
            if ((unsigned)(totsize + request->par.readWrite.pages[i+1].size) > (unsigned)theWriteBufferSize) {
              // We are not finished and the buffer is full
              write_not_complete = true;
              // Start again with next page
              page_num = i + 1;
              break;
            }
          }
          page_offset += (off_t)request->par.readWrite.pages[i].size;
        }
        bufptr = theWriteBuffer;
      } else {
        // One page write, write page directly
        bufptr = request->par.readWrite.pages[0].buf;
        totsize = request->par.readWrite.pages[0].size;
      }
      int err = writeBuffer(bufptr, totsize, offset);
      if(err != 0){
        request->error = err;
        return;
      }
    } // while(write_not_complete)
  }
  if (m_file.sync_on_write() == -1)
  {
    request->error = get_last_os_error();
  }
}

void AsyncFile::syncReq(Request *request)
{
  if (m_file.sync())
  {
    request->error = get_last_os_error();
    return;
  }
}

bool AsyncFile::check_odirect_request(const char* buf,
                                           size_t sz,
                                           off_t offset)
{
  if (m_open_flags & FsOpenReq::OM_DIRECT)
  {
    if ((sz % NDB_O_DIRECT_WRITE_ALIGNMENT) ||
        (((UintPtr)buf) % NDB_O_DIRECT_WRITE_ALIGNMENT) ||
        (offset % NDB_O_DIRECT_WRITE_ALIGNMENT))
    {
      fprintf(stderr,
              "Error r/w of size %llu using buf %p to offset %llu in "
              "file %s not O_DIRECT aligned\n",
              (long long unsigned) sz,
              buf,
              (long long unsigned) offset,
              theFileName.c_str());
      return false;
    }
  }
  return true;
}

int AsyncFile::readBuffer(Request *req, char *buf,
                          size_t size, off_t offset)
{
  require(!use_enc);
  // TODO ensure OM_THREAD_POOL is respected!
  int return_value;
  req->par.readWrite.pages[0].size = 0;

  if (!check_odirect_request(buf, size, offset))
    return FsRef::fsErrInvalidParameters;

  if (use_gz)
  {
    /*
     * For compressed files one can only read forward from current position.
     */
    off_t curr = ndbzseek(&nzf, 0, SEEK_CUR);
    if (curr == -1)
    {
      /*
       * This should never happen, ndbzseek(0,SEEK_CUR) should always succeed.
       */
      return FsRef::fsErrUnknown;
    }
    if (offset < curr)
    {
      /*
       * Seek and read are not supported for compressed files.
       */
      return FsRef::fsErrInvalidParameters;
    }
    if (offset > curr)
    {
      /*
       * Seek and read are not supported for compressed files.
       * But handle speculative reads beyond end.
       */
      if (nzf.z_eof == 1 || nzf.z_err == Z_STREAM_END)
      {
        if (req->action == Request::readPartial)
        {
          return 0;
        }
        DEBUG(ndbout_c("Read underflow %d %d\n %x\n%d %d",
                        size, offset, buf, bytes_read, return_value));
        return ERR_ReadUnderflow;
      }
      return FsRef::fsErrInvalidParameters;
    }
  }

  int error = 0;

  while (size > 0)
  {
    size_t bytes_read = 0;

    if (!use_gz)
    {
      return_value = m_file.read_pos(buf, size, offset);
    }
    else
    {
      return_value = ndbzread(&nzf, buf, size, &error);
      if (return_value == 0)
      {
        if (nzf.z_eof != 1 && nzf.z_err != Z_STREAM_END)
        {
          ndbout_c("ERROR IN PosixAsyncFile::readBuffer %d %d %d",
                   my_errno(), nzf.z_err, error);
          require(my_errno() != 0);
          set_last_os_error(my_errno());
          return_value = -1;
        }
      }
    }
    if (return_value == -1)
    {
      return get_last_os_error();
    }
    bytes_read = return_value;
    req->par.readWrite.pages[0].size += bytes_read;
    if (bytes_read == 0)
    {
      if (req->action == Request::readPartial)
      {
	return 0;
      }
      DEBUG(ndbout_c("Read underflow %d %d\n %x\n%d %d",
		     size, offset, buf, bytes_read, return_value));
      return ERR_ReadUnderflow;
    }

    if (bytes_read != size)
    {
      DEBUG(ndbout_c("Warning partial read %d != %d on %s",
		     bytes_read, size, theFileName.c_str()));
    }

    buf += bytes_read;
    size -= bytes_read;
    offset += bytes_read;
  }
  return 0;
}

int AsyncFile::writeBuffer(const char *buf, size_t size, off_t offset)
{
  /*
   * Compression and encryption are only supported by appended files which uses appendReq().
   */
  require(!use_gz);
  require(!use_enc);

  size_t chunk_size = 256*1024;
  size_t bytes_to_write = chunk_size;
  int return_value;

  if (!check_odirect_request(buf, size, offset))
    return FsRef::fsErrInvalidParameters;

  while (size > 0)
  {
    if (size < bytes_to_write)
    {
      // We are at the last chunk
      bytes_to_write = size;
    }
    size_t bytes_written = 0;
    return_value = m_file.write_pos(buf, bytes_to_write, offset);

    if (return_value == -1)
    {
      ndbout_c("ERROR IN PosixAsyncFile::writeBuffer %d %d",
               get_last_os_error()/*m_file.get_error()*/, nzf.z_err);
      return get_last_os_error();
    }
    else
    {
      bytes_written = return_value;

      if (bytes_written == 0)
      {
        DEBUG(ndbout_c("no bytes written"));
	require(bytes_written > 0);
      }

      if (bytes_written != bytes_to_write)
      {
	DEBUG(ndbout_c("Warning partial write %d != %d",
		 bytes_written, bytes_to_write));
      }
    }

    buf += bytes_written;
    size -= bytes_written;
    offset += bytes_written;
  }
  return 0;
}

int AsyncFile::ndbxfrm_append(Request *request, ndbxfrm_input_iterator* in)
{
  bool transform_failed = false;
  const byte* in_cbegin = in->cbegin();
  ndbxfrm_buffer* file_bufp = nullptr;
  ndbxfrm_input_iterator file_in = *in;

  /*
   * If data is both compressed and encrypted, data will always first be
   * compressed than encrypted.
   */
  if (use_gz)
  {
    ndbxfrm_output_iterator out = m_compress_buffer.get_output_iterator();
    if (out.size() < NDB_O_DIRECT_WRITE_BLOCKSIZE)
    {
      m_compress_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
      out = m_compress_buffer.get_output_iterator();
    }
    int rv = zlib.deflate(&out, in);
    if (rv == -1)
    {
      transform_failed = true;
      request->error = get_last_os_error();
      return -1;
    }
    if (!in->last()) require(!out.last());
    m_compress_buffer.update_write(out);
    file_bufp = &m_compress_buffer;
    file_in = file_bufp->get_input_iterator();
  }
  else if (use_enc)
  {
    /*
     * Copy (uncompressed) app data into m_compress_buffer.
     *
     * This since later encryption step will use m_compress_buffer as input
     * buffer.
     */
    ndbxfrm_output_iterator out = m_compress_buffer.get_output_iterator();
    if (out.size() < NDB_O_DIRECT_WRITE_BLOCKSIZE)
    {
      m_compress_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
      out = m_compress_buffer.get_output_iterator();
    }
    size_t copy_len = std::min(in->size(), out.size());
    memcpy(out.begin(), in->cbegin(), copy_len);
    out.advance(copy_len);
    in->advance(copy_len);
    require(!out.last());
    if (in->last() && in->empty()) out.set_last();

    m_compress_buffer.update_write(out);
  }

  if (use_enc)
  { // encrypt data from m_compress_buffer into m_encrypt_buffer
    ndbxfrm_input_iterator c_in = m_compress_buffer.get_input_iterator();
    ndbxfrm_output_iterator out = m_encrypt_buffer.get_output_iterator();
    if (out.size() < NDB_O_DIRECT_WRITE_BLOCKSIZE)
    {
      m_encrypt_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
      out = m_encrypt_buffer.get_output_iterator();
    }
    int rv = openssl_evp_op.encrypt(&out, &c_in);
    if (rv == -1)
    {
      transform_failed = true;
      request->error = get_last_os_error();
      return -1;
    }
    m_compress_buffer.update_read(c_in);
    m_compress_buffer.rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
    m_encrypt_buffer.update_write(out);
    file_bufp = &m_encrypt_buffer;
    file_in = file_bufp->get_input_iterator();
  }

  if (!transform_failed)
  {
    size_t write_len = file_in.size();
    if (file_bufp != nullptr)
      write_len -= write_len % NDB_O_DIRECT_WRITE_BLOCKSIZE;
    int n;
    if (write_len > 0)
      n = m_file.append(file_in.cbegin(), write_len);
    else
      n = 0;
    if (n > 0) file_in.advance(n);
    // Fail if not all written and no buffer is used.
    if (n == -1 || (file_bufp == nullptr && !file_in.empty()))
    {
      request->error = get_last_os_error();
      return -1;
    }
    if (file_bufp != nullptr)
    {
      file_bufp->update_read(file_in);
      file_bufp->rebase(NDB_O_DIRECT_WRITE_BLOCKSIZE);
    }
    else
      in->advance(n);
  }
  else
  {
    request->error = get_last_os_error();
    return -1;
  }
#if 0
  if (n == 0)
  {
    DEBUG(ndbout_c("append with n=0"));
    require(n != 0);
  }
#endif

  int n = in->cbegin() - in_cbegin;
  if (n > 0) m_crc32 = crc32(m_crc32, in_cbegin, n);
  m_data_size += n;

  return file_in.last() ? 0 : 1;
}

void AsyncFile::appendReq(Request *request)
{
  const byte * buf = reinterpret_cast<const byte*>(request->par.append.buf);
  Uint32 size = request->par.append.size;

  if (!check_odirect_request(request->par.append.buf, size, 0))
    request->error = FsRef::fsErrInvalidParameters;

  int Guard = 80;
  while (size > 0)
  {
    require(Guard--);

    int n;
    ndbxfrm_input_iterator in(buf, buf + size, false);

    const byte* in_begin = in.cbegin();
    int r;
    if ((r = ndbxfrm_append(request, &in)) == -1)
    {
      request->error = get_last_os_error();
      if (request->error == 0)
      {
        request->error = FsRef::fsErrUnknown;
      }
      return;
    }
    if (r == 0)
    {
      require(in.empty());
    }
    n = in.cbegin() - in_begin;
    size -= n;
    buf += n;
  }

  if (m_file.sync_on_write() == -1)
  {
    request->error = get_last_os_error();
    if (request->error == 0)
    {
      request->error = FsRef::fsErrSync;
    }
    return;
  }
  require(request->error == 0);
}

#ifdef DEBUG_ASYNCFILE
void printErrorAndFlags(Uint32 used_flags) {
  char buf[255];
  sprintf(buf, "PEAF: errno=%d \"", errno);

  strcat(buf, strerror(errno));

  strcat(buf, "\" ");
  strcat(buf, " flags: ");
  switch(used_flags & 3){
  case O_RDONLY:
    strcat(buf, "O_RDONLY, ");
    break;
  case O_WRONLY:
    strcat(buf, "O_WRONLY, ");
    break;
  case O_RDWR:
    strcat(buf, "O_RDWR, ");
    break;
  default:
    strcat(buf, "Unknown!!, ");
  }

  if((used_flags & O_APPEND)==O_APPEND)
    strcat(buf, "O_APPEND, ");
  if((used_flags & O_CREAT)==O_CREAT)
    strcat(buf, "O_CREAT, ");
  if((used_flags & O_EXCL)==O_EXCL)
    strcat(buf, "O_EXCL, ");
  if((used_flags & O_NOCTTY) == O_NOCTTY)
    strcat(buf, "O_NOCTTY, ");
  if((used_flags & O_NONBLOCK)==O_NONBLOCK)
    strcat(buf, "O_NONBLOCK, ");
  if((used_flags & O_TRUNC)==O_TRUNC)
    strcat(buf, "O_TRUNC, ");
#ifdef O_DSYNC /* At least Darwin 7.9 doesn't have it */
  if((used_flags & O_DSYNC)==O_DSYNC)
    strcat(buf, "O_DSYNC, ");
#endif
  if((used_flags & O_NDELAY)==O_NDELAY)
    strcat(buf, "O_NDELAY, ");
#ifdef O_RSYNC /* At least Darwin 7.9 doesn't have it */
  if((used_flags & O_RSYNC)==O_RSYNC)
    strcat(buf, "O_RSYNC, ");
#endif
#ifdef O_SYNC
  if((used_flags & O_SYNC)==O_SYNC)
    strcat(buf, "O_SYNC, ");
#endif
  DEBUG(ndbout_c(buf));

}
#endif

NdbOut&
operator<<(NdbOut& out, const Request& req)
{
  out << "[ Request: file: " << hex << req.file
      << " userRef: " << hex << req.theUserReference
      << " userData: " << dec << req.theUserPointer
      << " theFilePointer: " << req.theFilePointer
      << " action: ";
  out << Request::actionName(req.action);
  out << " ]";
  return out;
}
