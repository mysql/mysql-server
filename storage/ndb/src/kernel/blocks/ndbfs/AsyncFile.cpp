/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include "util/require.h"

#include <math.h>  // sqrt
#include <algorithm>
#include <memory>
#include "AsyncFile.hpp"
#include "Filename.hpp"
#include "Ndbfs.hpp"
#include "my_thread_local.h"  // my_errno
#include "portlib/NdbTick.h"
#include "util/cstrbuf.h"
#include "util/ndb_az31.h"
#include "util/ndb_math.h"
#include "util/ndb_ndbxfrm1.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_rand.h"
#include "util/ndb_zlib.h"

#include <NdbThread.h>
#include <kernel_types.h>
#include <Configuration.hpp>
#include <ErrorHandlingMacros.hpp>
#include <ndbd_malloc.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>

#define JAM_FILE_ID 387

AsyncFile::AsyncFile(Ndbfs &fs)
    : theFileName(), m_thread_bound(false), m_fs(fs) {
  m_thread = 0;

  m_resource_group = RNIL;
  m_page_cnt = 0;
  m_page_ptr.setNull();
  theWriteBuffer = 0;
  theWriteBufferSize = 0;
}

AsyncFile::~AsyncFile() {}

int AsyncFile::init() { return 0; }

void AsyncFile::attach(AsyncIoThread *thr) {
#if 0
  g_eventLogger->info("%p:%s attach to %p (m_thread: %p)", this, theFileName.c_str(), thr,
             m_thread);
#endif
  assert(m_thread_bound);
  assert(m_thread == 0);
  m_thread = thr;
}

void AsyncFile::detach(AsyncIoThread *thr) {
#if 0
  g_eventLogger->info("%p:%s detach from %p", this, theFileName.c_str(), thr);
#endif
  assert(m_thread_bound);
  assert(m_thread == thr);
  m_thread = 0;
}

void AsyncFile::openReq(Request *request) {
  require(!m_file.is_open());
  // For open.flags, see signal FSOPENREQ
  m_open_flags = request->par.open.flags;
  Uint32 flags = m_open_flags;

  if ((flags & FsOpenReq::OM_READ_WRITE_MASK) == FsOpenReq::OM_WRITEONLY &&
      !(flags & FsOpenReq::OM_CREATE)) {
    /* If file is write only one can not read and detect fileformat!
     * Change to allow both read and write.
     * This mode is used by dbdict for schema log file, and by restore for lcp
     * ctl file.
     */
    flags = (flags & ~FsOpenReq::OM_READ_WRITE_MASK) | FsOpenReq::OM_READWRITE;
  }

  const Uint32 page_size = request->par.open.page_size;
  const Uint64 data_size = request->par.open.file_size;

  const bool is_data_size_estimated = (flags & FsOpenReq::OM_SIZE_ESTIMATED);

  // Validate some flag combination.

  // Not both OM_INIT and OM_GZ
  const bool file_init =
      (flags & FsOpenReq::OM_INIT) || (flags & FsOpenReq::OM_SPARSE_INIT);
  require(!file_init || !(flags & FsOpenReq::OM_GZ));

  // Set flags for compression (OM_GZ) and encryption (OM_ENCRYPT_CBC/XTS)
  const bool use_gz = (flags & FsOpenReq::OM_GZ);
  const bool use_enc = (flags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK);
  Uint32 enc_cipher;
  switch (flags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK) {
    case 0:
      require(!use_enc);
      enc_cipher = 0;
      break;
    case FsOpenReq::OM_ENCRYPT_CBC:
      require(use_enc);
      enc_cipher = ndb_ndbxfrm1::cipher_cbc;
      break;
    case FsOpenReq::OM_ENCRYPT_XTS:
      require(use_enc);
      enc_cipher = ndb_ndbxfrm1::cipher_xts;
      break;
    default:
      std::terminate();
  }

  // OM_DIRECT_SYNC is not valid without OM_DIRECT
  require(!(flags & FsOpenReq::OM_DIRECT_SYNC) ||
          (flags & FsOpenReq::OM_DIRECT));

  // Create file
  bool created = false;
  if (flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE)) {
    if (m_file.create(theFileName.c_str()) == -1) {
      int error = get_last_os_error();
      int ndbfs_error = Ndbfs::translateErrno(error);
      if (ndbfs_error == FsRef::fsErrFileDoesNotExist) {
        // Assume directories are missing, create directories and try again.
        createDirectories();
        if (m_file.create(theFileName.c_str()) == -1) {
          error = get_last_os_error();
          ndbfs_error = Ndbfs::translateErrno(error);
        } else {
          created = true;
        }
      }
      if (!created &&
          ((flags & FsOpenReq::OM_CREATE_IF_NONE) ||
           Ndbfs::translateErrno(error) != FsRef::fsErrFileExists)) {
        NDBFS_SET_REQUEST_ERROR(request, error);
        return;
      }
    } else {
      created = true;
    }
  }
  // Open file (OM_READ_WRITE_MASK, OM_APPEND)
  constexpr Uint32 open_flags =
      FsOpenReq::OM_READ_WRITE_MASK | FsOpenReq::OM_APPEND;
  if (m_file.open(theFileName.c_str(), flags & open_flags) == -1) {
    // Common expected error for NDBCNTR sysfile, DBDIH sysfile, LCP ctl
    NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
    goto remove_if_created;
  }

  // Truncate if OM_TRUNCATE
  if (!created && (flags & FsOpenReq::OM_TRUNCATE)) {
    if (m_file.truncate(0) == -1) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      m_file.close();
      goto remove_if_created;
    }
  }

  // Treat open zero sized file as creation of file if creation flags passed
  // (including case when file was truncated).
  if (!created && m_file.get_size() == 0) {
    if (flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE)) {
      require(!(flags & FsOpenReq::OM_CREATE_IF_NONE));
      created = true;
    } else {
#if defined(VM_TRACE) || !defined(NDEBUG)
      /*
       * LCP/0/T13F7.ctl has been seen with zero size, open flags OM_READWRITE |
       * OM_APPEND Likely a partial read or failed read will be caught by
       * application level, and file ignored. Are there ever files that can be
       * empty in ndb_x_fs? Else we could treat zero file as no file, must then
       * remove I guess to not trick create_if_none?
       *
       * D1/NDBCNTR/P0.sysfile: ABORT: open empty not fake created page_size 0
       * flags 0x00000000  : OM_READONLY?
       */
      if (strstr(theFileName.c_str(), "LCP") &&
          strstr(theFileName.c_str(), ".ctl"))
        ;  // TODO maybe not safe on all os file system, upper/lowercase?
      else if (strstr(theFileName.c_str(), "NDBCNTR") &&
               strstr(theFileName.c_str(), ".sysfile"))
        ;  // OM_READONLY?
      else if (strstr(theFileName.c_str(), "DBDIH") &&
               strstr(theFileName.c_str(), ".FragList")) {
        ;  // OM_READWRITE existing: D1/DBDIH/S17.FragList - disk full?
        // Maybe should fail open-request? Or wait for underflow on later read?
      } else {
        abort();  // TODO: relax since could be caused by previous disk full?
      }
#endif
    }
  }

  // append only allowed if file is created
  require(created || !(FsOpenReq::OM_APPEND & flags));

  //
  {
    const int pwd_len = use_enc ? m_key_material.length : 0;
    ndb_openssl_evp::byte *pwd =
        use_enc ? reinterpret_cast<ndb_openssl_evp::byte *>(m_key_material.data)
                : nullptr;
    int rc;
    if (created) {
      size_t key_data_unit_size;
      size_t file_block_size;
      if (page_size == 0 || use_gz) {
        size_t xts_data_unit_size = GLOBAL_PAGE_SIZE;
        const bool use_cbc = (enc_cipher == ndb_ndbxfrm1::cipher_cbc);
        const bool use_xts = (enc_cipher == ndb_ndbxfrm1::cipher_xts);
        key_data_unit_size = ((use_enc && use_xts) ? xts_data_unit_size : 0);
        /*
         * For compressed files we use 512 byte file block size to be
         * compatible with old compressed files (AZ31 format).
         * Also when using CBC-mode we use 512 byte file block size to be
         * compatible with old encrypted backup files.
         */
        file_block_size = ((use_enc && use_xts) ? xts_data_unit_size
                           : (use_gz || (use_enc && use_cbc)) ? 512
                                                              : 0);
      } else {
        key_data_unit_size = page_size;
        file_block_size = page_size;
      }
      if ((m_open_flags & FsOpenReq::OM_APPEND) && !is_data_size_estimated)
        require(!ndbxfrm_file::is_definite_size(data_size));

      if (is_data_size_estimated) require(FsOpenReq::OM_APPEND & flags);

      int kdf_iter_count = 0;  // Use AESKW (assume OM_ENCRYPT_KEY)
      if ((m_open_flags & FsOpenReq::OM_ENCRYPT_KEY_MATERIAL_MASK) ==
          FsOpenReq::OM_ENCRYPT_PASSWORD) {
        kdf_iter_count = -1;  // Use PBKDF2 let ndb_ndbxfrm decide iter count
      }
      rc = m_xfile.create(m_file, use_gz, pwd, pwd_len, kdf_iter_count,
                          enc_cipher, -1, key_data_unit_size, file_block_size,
                          data_size, is_data_size_estimated);
      if (rc < 0) NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
    } else {
      rc = m_xfile.open(m_file, pwd, pwd_len);
      if (rc < 0) NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
    }
    if (rc < 0) {
      m_file.close();
      goto remove_if_created;
    }
    if (ndbxfrm_file::is_definite_size(data_size) && !is_data_size_estimated &&
        size_t(m_xfile.get_data_size()) != data_size) {
      NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrInvalidFileSize);
      m_file.close();
      goto remove_if_created;
    }
  }

  // Verify file size (OM_CHECK_SIZE)
  if (flags & FsOpenReq::OM_CHECK_SIZE) {
    ndb_off_t file_data_size = m_xfile.get_size();
    if (file_data_size == -1) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
    } else if ((Uint64)file_data_size != request->par.open.file_size) {
      NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrInvalidFileSize);
    }
    if (request->error.code != 0) {
      m_file.close();
      goto remove_if_created;
    }
  }

  // Turn on direct io (OM_DIRECT, OM_DIRECT_SYNC)
  if (flags & FsOpenReq::OM_DIRECT) {
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
    if (m_file.have_direct_io_support() &&
        !m_file.avoid_direct_io_on_append()) {
      const bool direct_sync = flags & FsOpenReq::OM_DIRECT_SYNC;
      const int ret = m_file.set_direct_io(direct_sync);
      log_set_odirect_result(ret);
    }
  }

  /*
   * Initialise file sparsely if OM_SPARSE_INIT.
   * Set size and make sure unwritten block are read as zero.
   */

  if (flags & FsOpenReq::OM_SPARSE_INIT) {
    {
      ndb_off_t file_data_size = m_xfile.get_size();
      ndb_off_t data_size = request->par.open.file_size;
      require(file_data_size == data_size);  // Currently do not support neither
                                             // gz or enc on redo-log file
    }
    if (m_file.sync() == -1) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      m_file.close();
      goto remove_if_created;
    }
  }

  // Initialise file if OM_INIT

  if (flags & FsOpenReq::OM_INIT) {
    ndb_off_t file_data_size = m_xfile.get_size();
    ndb_off_t data_size = request->par.open.file_size;
    require(file_data_size == data_size);

    m_file.set_autosync(16 * 1024 * 1024);

    // Reserve disk blocks for whole file
    if (m_file.allocate() == -1) {
      // If fail, ignore, will try to write file anyway.
    }

    // Initialise blocks
    ndb_off_t off = 0;
    FsReadWriteReq req[1];

    Uint32 index = 0;

#ifdef VM_TRACE
#define TRACE_INIT
#endif

#ifdef TRACE_INIT
    Uint32 write_cnt = 0;
    const NDB_TICKS start = NdbTick_getCurrentTicks();
#endif
    ndb_openssl_evp::operation *openssl_evp_op = nullptr;
    /*
     * Block code will initialize one page at a time for a given position in
     * the file.
     * The block code will be called for a range of pages and when written to
     * file in big chunks.
     * For transformed files, we always pass the last page in range to block
     * code to initialize, then we transform it and write it to right position
     * in page range, and then write them to file.
     */
    Uint32 page_cnt =
        (!m_xfile.is_transformed()) ? m_page_cnt : (m_page_cnt - 1);
    require(page_cnt > 0);
    while (off < file_data_size) {
      ndb_off_t size = 0;
      Uint32 cnt = 0;
      while (cnt < page_cnt && (off + size) < file_data_size) {
        req->filePointer = 0;                        // DATA 0
        req->userPointer = request->theUserPointer;  // DATA 2
        req->numberOfPages = 1;                      // DATA 5
        req->varIndex = index++;
        req->operationFlag = 0;
        FsReadWriteReq::setFormatFlag(req->operationFlag,
                                      FsReadWriteReq::fsFormatSharedPage);
        if (!m_xfile.is_transformed())
          req->data.sharedPage.pageNumber = m_page_ptr.i + cnt;
        else
          req->data.sharedPage.pageNumber = m_page_ptr.i + page_cnt;

        m_fs.callFSWRITEREQ(request->theUserReference, req);

        if (m_xfile.is_transformed()) {
          const GlobalPage *src = m_page_ptr.p + page_cnt;
          GlobalPage *dst = m_page_ptr.p + cnt;
          ndbxfrm_input_iterator in = {(const byte *)src,
                                       (const byte *)(src + 1), false};
          ndbxfrm_output_iterator out = {(byte *)dst, (byte *)(dst + 1), false};
          if (m_xfile.transform_pages(openssl_evp_op,
                                      (index - 1) * GLOBAL_PAGE_SIZE, &out,
                                      &in) == -1) {
            fflush(stderr);
            abort();
          }
        }

        cnt++;
        size += request->par.open.page_size;
      }
      ndb_off_t save_size = size;
      byte *buf = (byte *)m_page_ptr.p;
      while (size > 0) {
#ifdef TRACE_INIT
        write_cnt++;
#endif
        int n;
        ndbxfrm_input_iterator in = {buf, buf + size, false};
        int rc = m_xfile.write_transformed_pages(off, &in);
        if (rc == -1)
          n = -1;
        else
          n = in.cbegin() - buf;
        if (n == -1 || n == 0) {
          g_eventLogger->info("write returned %d: errno: %d my_errno: %d", n,
                              get_last_os_error(), my_errno());
          break;
        }
        size -= n;
        buf += n;
      }
      if (size != 0) {
        NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
        m_file.close();
        goto remove_if_created;
      }
      require(save_size > 0);
      off += save_size;
    }
    if (m_file.sync() == -1) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      m_file.close();
      goto remove_if_created;
    }
#ifdef TRACE_INIT
    const NDB_TICKS stop = NdbTick_getCurrentTicks();
    Uint64 diff = NdbTick_Elapsed(start, stop).milliSec();
    if (diff == 0) diff = 1;
    g_eventLogger->info("wrote %umb in %u writes %us -> %ukb/write %umb/s",
                        Uint32(file_data_size / (1024 * 1024)), write_cnt,
                        Uint32(diff / 1000),
                        Uint32(file_data_size / 1024 / write_cnt),
                        Uint32(file_data_size / diff));
#endif

    if (m_file.set_pos(0) == -1) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      m_file.close();
      goto remove_if_created;
    }

    m_file.set_autosync(0);
  }

  // Turn on direct io (OM_DIRECT, OM_DIRECT_SYNC) after init
  if (flags & FsOpenReq::OM_DIRECT) {
    if (m_file.have_direct_io_support() && m_file.avoid_direct_io_on_append()) {
      const bool direct_sync = flags & FsOpenReq::OM_DIRECT_SYNC;
      const int ret = m_file.set_direct_io(direct_sync);
      log_set_odirect_result(ret);
    }
  }

  // Turn on synchronous mode (OM_SYNC)
  if (flags & FsOpenReq::OM_SYNC) {
    if (m_file.reopen_with_sync(theFileName.c_str()) == -1) {
      /*
       * reopen_with_sync should always succeed, if file can not be open in
       * sync mode, explicit call to fsync/FlushFiles will be done on every
       * write.
       */
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      m_file.close();
      goto remove_if_created;
    }
  }

  // Read file size
  if (flags & FsOpenReq::OM_READ_SIZE) {
    ndb_off_t file_data_size = m_xfile.get_size();
    if (file_data_size == -1) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      m_file.close();
      goto remove_if_created;
    }
    request->m_file_size_hi = Uint32(file_data_size >> 32);
    request->m_file_size_lo = Uint32(file_data_size & 0xFFFFFFFF);
  } else {
    request->m_file_size_hi = Uint32(~0);
    request->m_file_size_lo = Uint32(~0);
  }

  // Turn on compression (OM_GZ) and encryption (OM_ENCRYPT)
  if (use_gz || use_enc) {
    int ndbz_flags = 0;
    if (flags & (FsOpenReq::OM_CREATE | FsOpenReq::OM_CREATE_IF_NONE)) {
      ndbz_flags |= O_CREAT;
    }
    if (flags & FsOpenReq::OM_TRUNCATE) {
      ndbz_flags |= O_TRUNC;
    }
    if (flags & FsOpenReq::OM_APPEND) {
      ndbz_flags |= O_APPEND;
    }
    switch (flags & FsOpenReq::OM_READ_WRITE_MASK) {
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
        NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrInvalidParameters);
        m_file.close();
        goto remove_if_created;
    }
    if (flags & FsOpenReq::OM_APPEND ||
        ((flags & FsOpenReq::OM_READ_WRITE_MASK) ==
         FsOpenReq::OM_WRITEONLY)) {  // WRITE compressed (BACKUP, LCP)
    } else if ((flags & FsOpenReq::OM_READ_WRITE_MASK) ==
               FsOpenReq::OM_READONLY) {  // READ compressed (LCP)
    } else {
      // Compression and encryption only for appendable files
      require(!use_gz);
    }
  }

  // Turn on autosync mode (OM_AUTOSYNC auto_sync_size)
  if (flags & FsOpenReq::OM_AUTOSYNC) {
    m_file.set_autosync(request->par.open.auto_sync_size);
  }

  /*
   * If OM_READ_FORWARD it is expected that application layer read the file
   * from start to end without gaps.
   * That allows buffering between read calls which in turn allows file to be
   * compressed or efficiently decrypted if CBC-mode encrypted.
   */
  if (m_open_flags & FsOpenReq::OM_READ_FORWARD) {
    m_next_read_pos = 0;
  } else {
    m_next_read_pos = UINT64_MAX;
  }

  require(request->error.code == 0);
  return;

remove_if_created:
  //  require(!created);
#if TEST_UNRELIABLE_DISTRIBUTED_FILESYSTEM
  // Sometimes inject double file delete
  if (created && check_inject_and_log_extra_remove(theFileName.c_str()))
    m_file.remove(theFileName.c_str());
#endif
  if (created && m_file.remove(theFileName.c_str()) == -1) {
#if UNRELIABLE_DISTRIBUTED_FILESYSTEM
    if (check_and_log_if_remove_failure_ok(theFileName.c_str())) return;
#endif
    g_eventLogger->info(
        "Could not remove '%s' (err %u) after open failure (err %u).",
        theFileName.c_str(), get_last_os_error(), request->error.code);
  }
}

void AsyncFile::closeReq(Request *request) {
  // If closeRemove no final write or sync is needed!
  bool abort = (request->action & Request::closeRemove);
  if (m_open_flags & (FsOpenReq::OM_WRITEONLY | FsOpenReq::OM_READWRITE |
                      FsOpenReq::OM_APPEND)) {
    if (!abort) syncReq(request);
  }
  int r = 0;
#ifndef NDEBUG
  if (!m_file.is_open()) {
    DEBUG(g_eventLogger->info("close on already closed file"));
    ::abort();
  }
#endif
  if (m_xfile.is_open()) {
    int r = m_xfile.close(abort);
    if (r != 0) {
      NDBFS_SET_REQUEST_ERROR(request,
                              FsRef::fsErrUnknown);  // TODO better error
    }
  }
  if (m_file.is_open()) {
    if (!abort) m_file.sync();
    r = m_file.close();
  }
  if (-1 == r) {
    NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
  }
}

void AsyncFile::readReq(Request *request) {
  const bool read_forward = (m_open_flags & FsOpenReq::OM_READ_FORWARD);
  if (!read_forward) {  // Read random page
    require(m_xfile.get_random_access_block_size() > 0);
    ndb_openssl_evp::operation *openssl_evp_op = nullptr;
    if (!thread_bound() && m_xfile.is_encrypted()) {
      openssl_evp_op = &request->thread->m_openssl_evp_op;
    }
    /*
     * current_data_offset is the offset relative plain data.
     * current_file_offset is the offset relative the corresponding transformed
     * data on file.
     * Note, current_file_offset will not include NDBXFRM1 or AZ31 header, that
     * is, current_data_offset zero always corresponds to
     * current_file_offset zero.
     */
    ndb_off_t current_data_offset = request->par.readWrite.pages[0].offset;
    /*
     * Assumes size-preserving transform is used, currently either raw or
     * encrypted.
     */
    ndb_off_t current_file_offset = current_data_offset;
    for (int i = 0; i < request->par.readWrite.numberOfPages; i++) {
      if (current_data_offset != request->par.readWrite.pages[i].offset) {
        g_eventLogger->info(
            "%s: All parts of read do not form a consecutive "
            "read from file.",
            theFileName.c_str());
        NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
        return;
      }
      Uint64 size = request->par.readWrite.pages[i].size;
      byte *buf = reinterpret_cast<byte *>(request->par.readWrite.pages[i].buf);

      request->par.readWrite.pages[i].size = 0;

      ndbxfrm_output_iterator out = {buf, buf + size, false};
      if (m_xfile.read_transformed_pages(current_file_offset, &out) == -1) {
        NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
        return;
      }
      size_t bytes_read = out.begin() - buf;
      if (bytes_read != size) {
        if (request->action != Request::readPartial) {
          NDBFS_SET_REQUEST_ERROR(request, ERR_ReadUnderflow);
          return;
        }
      }
      current_file_offset += bytes_read;

      if (!m_xfile.is_transformed()) {
        current_data_offset += bytes_read;
      } else {
        /*
         * If transformed content, read transformed data from return buffer and
         * untransform into local buffer, then copy back to return buffer.
         * This way adds data copies that could be avoided but is an easy way
         * to be able to always read all at once instead of issuing several
         * system calls to read smaller chunks at a time.
         */
        const bool zeros_are_sparse =
            (m_open_flags & FsOpenReq::OM_ZEROS_ARE_SPARSE);
        ndbxfrm_input_iterator in = {buf, buf + bytes_read, false};
        while (!in.empty()) {
          if (!m_xfile.is_compressed())  // sparse_file)
          {
            // Only REDO log files can be sparse and they uses 32KB pages
            require(bytes_read % GLOBAL_PAGE_SIZE == 0);
            const byte *p = in.cbegin();
            const byte *end = in.cend();
            require((end - p) % GLOBAL_PAGE_SIZE == 0);
            while (p != end && *p == 0) p++;
            // Only skip whole pages with zeros
            size_t sz =
                ((p - in.cbegin()) / GLOBAL_PAGE_SIZE) * GLOBAL_PAGE_SIZE;
            if (sz > 0) {
              if (m_xfile.is_encrypted()) require(zeros_are_sparse);
              // Keep zeros as is without untransform.
              in.advance(sz);
              current_data_offset += sz;
              if (in.empty()) break;
            }
          }
          byte buffer[GLOBAL_PAGE_SIZE];
          ndbxfrm_output_iterator out = {buffer, buffer + GLOBAL_PAGE_SIZE,
                                         false};
          const byte *in_cbegin = in.cbegin();
          if (m_xfile.untransform_pages(openssl_evp_op, current_data_offset,
                                        &out, &in) == -1) {
            g_eventLogger->info(
                "%s: Transformation of reads from file buffer "
                "failed.",
                theFileName.c_str());
            NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
          }
          size_t bytes = in.cbegin() - in_cbegin;
          current_data_offset += bytes;
          byte *dst = buf + (in_cbegin - buf);
          memcpy(dst, buffer, bytes);
        }
        require(in.empty());
      }
      require(current_data_offset == current_file_offset);

      request->par.readWrite.pages[i].size += bytes_read;

      if (bytes_read != size) {  // eof
        return;
      }
    }
    return;
  }

  // Stream read forward.
  require(thread_bound());
  // Only one page supported.
  require(request->par.readWrite.numberOfPages == 1);
  {
    ndb_off_t offset = request->par.readWrite.pages[0].offset;
    size_t size = request->par.readWrite.pages[0].size;
    char *buf = request->par.readWrite.pages[0].buf;

    size_t bytes_read = 0;
    if (offset != (ndb_off_t)m_next_read_pos &&
        offset < m_xfile.get_data_size()) {
      // read out of sync
      request->par.readWrite.pages[0].size = 0;
      NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
      return;
    }
    if (m_xfile.get_data_pos() < offset) {
      // Likely a speculative read request beyond end when restoring LCP data
      require(m_xfile.get_data_pos() == m_xfile.get_data_size());
    } else {
      require(m_xfile.get_data_pos() == offset);
      int return_value = 0;
      byte *byte_buf = reinterpret_cast<byte *>(buf);
      ndbxfrm_output_iterator out = {byte_buf, byte_buf + size, false};
      return_value = m_xfile.read_forward(&out);
      if (return_value >= 0) bytes_read = out.begin() - byte_buf;
      if (return_value == -1) {
        NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
        return;
      }
    }
    request->par.readWrite.pages[0].size = bytes_read;
    if (bytes_read == 0) {
      if (request->action == Request::readPartial) {
        return;
      }
      DEBUG(g_eventLogger->info("Read underflow %d %d %x %d", size, offset, buf,
                                bytes_read));
      NDBFS_SET_REQUEST_ERROR(request, ERR_ReadUnderflow);
      return;
    }
    m_next_read_pos += request->par.readWrite.pages[0].size;
    require((ndb_off_t)m_next_read_pos <= m_xfile.get_data_size());
    if (bytes_read != size) {
      DEBUG(g_eventLogger->info("Warning partial read %d != %d on %s",
                                bytes_read, size, theFileName.c_str()));
      if (request->action == Request::readPartial) {
        return;
      }
      NDBFS_SET_REQUEST_ERROR(request, ERR_ReadUnderflow);
      return;
    }
  }
}

void AsyncFile::writeReq(Request *request) {
  /*
   * Always postitioned writes of blocks that can be transformed independent
   * of other blocks.
   */
  require(m_xfile.get_random_access_block_size() > 0);
  require(!m_xfile.is_compressed());

  const Uint32 cnt = request->par.readWrite.numberOfPages;
  if (!m_xfile.is_transformed() && (cnt == 1 || theWriteBuffer == nullptr)) {
    /*
     * Fast path for raw files written page by page directly from data buffers
     * in request.
     */
    for (Uint32 i = 0; i < cnt; i++) {
      Uint64 offset = request->par.readWrite.pages[i].offset;
      Uint32 size = request->par.readWrite.pages[i].size;
      const byte *buf = (const byte *)request->par.readWrite.pages[i].buf;
      ndbxfrm_input_iterator in = {buf, buf + size, false};
      int rc = m_xfile.write_transformed_pages(offset, &in);
      if (rc == -1) {
        NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
        return;
      }
      if (!in.empty()) {
        NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
        return;
      }
    }
    return;
  }

  /*
   * For raw data this path is used for copying all data from request into
   * contiguous memory to reduce number of system calls for write.
   *
   * For transformed data one always need to transform data first before write.
   */
  byte unaligned_buffer[GLOBAL_PAGE_SIZE + NDB_O_DIRECT_WRITE_ALIGNMENT];
  byte *file_buffer;
  size_t file_buffer_size;
  if (theWriteBuffer != nullptr) {
    // Use pre allocated big write buffer
    require(thread_bound());
    file_buffer = reinterpret_cast<byte *>(theWriteBuffer);
    file_buffer_size = theWriteBufferSize;
  } else {
    // Use a single page buffer for transform
    file_buffer = unaligned_buffer;
    size_t file_buffer_space = sizeof(unaligned_buffer);
    file_buffer_size = GLOBAL_PAGE_SIZE;
    void *voidp = file_buffer;
    require(std::align(NDB_O_DIRECT_WRITE_ALIGNMENT, file_buffer_size, voidp,
                       file_buffer_space) != nullptr);
    file_buffer = reinterpret_cast<byte *>(voidp);
  }

  ndb_openssl_evp::operation *openssl_evp_op = nullptr;
  if (m_xfile.is_encrypted() && !thread_bound()) {
    /*
     * For files that can use multiple threads for concurrent reads and writes
     * one can not reuse the encryption context from file object but need to
     * reuse the encryption context from thread.
     */
    openssl_evp_op = &request->thread->m_openssl_evp_op;
  }
  const bool zeros_are_sparse =
      m_xfile.is_encrypted() && (m_open_flags & FsOpenReq::OM_ZEROS_ARE_SPARSE);

  ndbxfrm_output_iterator file_out = {file_buffer,
                                      file_buffer + file_buffer_size, false};
  /*
   * current_data_offset is the offset relative plain data.
   * current_file_offset is the offset relative the corresponding transformed
   * data on file.
   * Note, current_file_offset will not include NDBXFRM1 or AZ31 header, that
   * is, current_data_offset zero always corresponds to
   * current_file_offset zero.
   */
  ndb_off_t current_data_offset = request->par.readWrite.pages[0].offset;
  /*
   * Assumes size-preserving transform is used, currently either raw or
   * encrypted.
   */
  ndb_off_t current_file_offset = current_data_offset;
  for (Uint32 i = 0; i < cnt; i++) {
    if (current_data_offset != request->par.readWrite.pages[i].offset) {
      g_eventLogger->info(
          "%s: All parts of write do not form a consecutive "
          "write to file.",
          theFileName.c_str());
      NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
      return;
    }
    Uint32 size = request->par.readWrite.pages[i].size;
    const byte *raw = (const byte *)request->par.readWrite.pages[i].buf;
    ndbxfrm_input_iterator raw_in = {raw, raw + size, false};

    do {
      byte *file_out_begin = file_out.begin();
      const byte *raw_in_begin = raw_in.cbegin();
      if (m_xfile.transform_pages(openssl_evp_op, current_data_offset,
                                  &file_out, &raw_in) == -1) {
        g_eventLogger->info(
            "%s: Transformation of writes to file buffer "
            "failed.",
            theFileName.c_str());
        NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
      }
      if (zeros_are_sparse) {
        const byte *p = file_out_begin;
        const byte *end = file_out.begin();
        require((end - p) % GLOBAL_PAGE_SIZE == 0);
        while (p != end) {
          const byte *q = p;
          while (q != end && *q == 0) q++;
          /*
           * If encryption produced a full page of zeros crash since reader can
           * not distinguish between sparse page and encrypted page that
           * happened to result in an all zeros page (should be a quite rare
           * event).
           */
          require((q - p) < GLOBAL_PAGE_SIZE);
          // start at next page boundary
          p += GLOBAL_PAGE_SIZE;
        }
      }

      current_data_offset += (raw_in.cbegin() - raw_in_begin);

      if (file_out.empty()) {
        ndbxfrm_input_iterator in = {file_buffer, file_out.begin(), false};
        const byte *in_cbegin = in.cbegin();
        m_xfile.write_transformed_pages(current_file_offset, &in);
        current_file_offset += (in.cbegin() - in_cbegin);
        if (!in.empty()) {
          NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
          return;
        }
        file_out = {file_buffer, file_buffer + file_buffer_size, false};
      }
    } while (!raw_in.empty());
  }

  if (file_out.begin() != file_buffer) {
    ndbxfrm_input_iterator in = {file_buffer, file_out.begin(), false};
    const byte *in_cbegin = in.cbegin();
    m_xfile.write_transformed_pages(current_file_offset, &in);
    current_file_offset += (in.cbegin() - in_cbegin);
    if (!in.empty()) {
      NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
      return;
    }
  }
  require(current_file_offset == current_data_offset);
}

void AsyncFile::syncReq(Request *request) {
  if (m_file.sync()) {
    NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
    return;
  }
}

bool AsyncFile::check_odirect_request(const char *buf, size_t sz,
                                      ndb_off_t offset) {
  if (m_open_flags & FsOpenReq::OM_DIRECT) {
    if ((sz % NDB_O_DIRECT_WRITE_ALIGNMENT) ||
        (((UintPtr)buf) % NDB_O_DIRECT_WRITE_ALIGNMENT) ||
        (offset % NDB_O_DIRECT_WRITE_ALIGNMENT)) {
      g_eventLogger->info(
          "Error r/w of size %llu using buf %p to offset %llu in "
          "file %s not O_DIRECT aligned",
          (long long unsigned)sz, buf, (long long unsigned)offset,
          theFileName.c_str());
      return false;
    }
  }
  return true;
}

void AsyncFile::appendReq(Request *request) {
  require(thread_bound());
  const byte *buf = reinterpret_cast<const byte *>(request->par.append.buf);
  Uint32 size = request->par.append.size;

  if (!check_odirect_request(request->par.append.buf, size, 0)) {
    NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrInvalidParameters);
  }

  ndbxfrm_input_iterator in(buf, buf + size, false);

  const byte *in_begin = in.cbegin();
  int r = m_xfile.write_forward(&in);
  if (r == -1) {
    NDBFS_SET_REQUEST_ERROR(request, get_last_os_error());
    if (request->error.code == 0) {
      NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
    }
    return;
  }
  if (!in.empty()) {
    NDBFS_SET_REQUEST_ERROR(request, FsRef::fsErrUnknown);
    return;
  }
  int n = in.cbegin() - in_begin;
  size -= n;
  buf += n;
  require(request->error.code == 0);
}

#ifdef DEBUG_ASYNCFILE
void printErrorAndFlags(Uint32 used_flags) {
  char buf[255];
  sprintf(buf, "PEAF: errno=%d \"", errno);

  strcat(buf, strerror(errno));

  strcat(buf, "\" ");
  strcat(buf, " flags: ");
  switch (used_flags & 3) {
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

  if ((used_flags & O_APPEND) == O_APPEND) strcat(buf, "O_APPEND, ");
  if ((used_flags & O_CREAT) == O_CREAT) strcat(buf, "O_CREAT, ");
  if ((used_flags & O_EXCL) == O_EXCL) strcat(buf, "O_EXCL, ");
  if ((used_flags & O_NOCTTY) == O_NOCTTY) strcat(buf, "O_NOCTTY, ");
  if ((used_flags & O_NONBLOCK) == O_NONBLOCK) strcat(buf, "O_NONBLOCK, ");
  if ((used_flags & O_TRUNC) == O_TRUNC) strcat(buf, "O_TRUNC, ");
#ifdef O_DSYNC /* At least Darwin 7.9 doesn't have it */
  if ((used_flags & O_DSYNC) == O_DSYNC) strcat(buf, "O_DSYNC, ");
#endif
  if ((used_flags & O_NDELAY) == O_NDELAY) strcat(buf, "O_NDELAY, ");
#ifdef O_RSYNC /* At least Darwin 7.9 doesn't have it */
  if ((used_flags & O_RSYNC) == O_RSYNC) strcat(buf, "O_RSYNC, ");
#endif
#ifdef O_SYNC
  if ((used_flags & O_SYNC) == O_SYNC) strcat(buf, "O_SYNC, ");
#endif
  DEBUG(g_eventLogger->info(buf));
}
#endif

#if UNRELIABLE_DISTRIBUTED_FILESYSTEM
bool AsyncFile::check_and_log_if_remove_failure_ok(const char *pathname) {
  int error = get_last_os_error();
  int ndbfs_error = Ndbfs::translateErrno(error);
  if (ndbfs_error != FsRef::fsErrFileDoesNotExist) return false;
  g_eventLogger->info(
      "Ignoring unexpected error: Path %s did not exist when removing. "
      "Unreliable filesystem?",
      pathname);
  set_last_os_error(0);
  return true;
}
#endif

#if TEST_UNRELIABLE_DISTRIBUTED_FILESYSTEM
bool AsyncFile::check_inject_and_log_extra_remove(const char *pathname) {
  // Remove file in 1% of cases
  if (ndb_rand() % 100 >= 1) return false;
  /*
   * The actual injection of an extra remove should be done by caller when
   * this function returns true.
   */
  g_eventLogger->info(
      "Injected error: expect 'Ignoring unexpected error' for path %s to "
      "follow. Removed file twice to emulate an unreliable filesystem.",
      pathname);
  return true;
}
#endif

void AsyncFile::log_set_odirect_result(int result) {
  const char *filename = theFileName.c_str();
  const bool success = (result == 0);
  const bool odirect_failure = (result == -1 && errno == EINVAL);
  const char *param = nullptr;
  BaseString baseparam;
  if (theFileName.is_under_base_path()) {
    // For files under base path, suppress repeated warnings
    const Uint32 bp_spec = theFileName.get_base_path_spec();

    // Update statistics
    if (success)
      odirect_set_log_bp[bp_spec].successes.fetch_add(1);
    else
      odirect_set_log_bp[bp_spec].failures.fetch_add(1);

    const NDB_TICKS now = NdbTick_getCurrentTicks();
    NDB_TICKS last = odirect_set_log_bp[bp_spec].last_warning.load();
    if (NdbTick_IsValid(last)) {
      const NdbDuration elapsed = NdbTick_Elapsed(last, now);
      if (elapsed.seconds() < odirect_set_log_suppress_period_s) {
        // Not yet time to report statistics
        return;
      }
    }
    if (!odirect_set_log_bp[bp_spec].last_warning.compare_exchange_strong(
            last, now)) {
      // Another thread came in between and will report
      return;
    }

    /*
     * Now it will be unlikely for another thread to come in between since
     * suppress_period_s is much bigger than milliseconds which should is
     * much more that should be needed to read and clear statistics below.
     */
    const Uint32 failures = odirect_set_log_bp[bp_spec].failures.exchange(0);
    const Uint32 successes = odirect_set_log_bp[bp_spec].successes.exchange(0);

    if (failures == 0) {
      // If no failures, skip report
      return;
    }

    g_eventLogger->warning(
        "Setting ODirect have failed for %u files and succeeded for %u files "
        "under %s (%s) since last warning.",
        failures, successes, m_fs.get_base_path(bp_spec).c_str(),
        m_fs.get_base_path_param_name(bp_spec).c_str());

    baseparam = m_fs.get_base_path_param_name(bp_spec);
    param = baseparam.c_str();
    assert(param != nullptr);
  } else {
    /*
     * Do not report statistics or single file success for files outside base
     * paths. That can be tablespace or logfile group files.
     * But do report any single file failure for those. Adding tablespace and
     * logfile group files do not happen very often.
     */
    if (success) return;
  }

  // Report single file failure or success for setting ODirect

  if (odirect_failure)  // Failed set ODirect
    g_eventLogger->warning(
        "Failed to set ODirect for file %s %s%s (errno: %u, block size %zu, "
        "alignment %zu, direct io %d, avoid on append %d, io block size %zu, "
        "alignment %zu).",
        filename, (param ? "under " : ""), (param ? param : ""),
        get_last_os_error(), m_file.get_block_size(),
        m_file.get_block_alignment(), m_file.have_direct_io_support(),
        m_file.avoid_direct_io_on_append(), m_file.get_direct_io_block_size(),
        m_file.get_direct_io_block_alignment());
  else if (success)  // Succeeded to set ODirect
    g_eventLogger->info("Succeeded to set ODirect for file %s %s%s.", filename,
                        (param ? "under " : ""), (param ? param : ""));
  else  // Failed checking ODirect
    g_eventLogger->warning(
        "Failed to probe ODirect for file %s %s%s (errno: %u, block size %zu, "
        "alignment %zu, direct io %d, avoid on append %d, io block size %zu, "
        "alignment %zu).",
        filename, (param ? "under " : ""), (param ? param : ""),
        get_last_os_error(), m_file.get_block_size(),
        m_file.get_block_alignment(), m_file.have_direct_io_support(),
        m_file.avoid_direct_io_on_append(), m_file.get_direct_io_block_size(),
        m_file.get_direct_io_block_alignment());
}

void AsyncFile::log_set_odirect_result(const char *param, const char *filename,
                                       int result) {
  const bool success = (result == 0);
  const bool odirect_failure = (result == -1 && errno == EINVAL);
  if (odirect_failure)  // Failed set ODirect
    g_eventLogger->warning(
        "Failed to set ODirect for file %s %s%s (errno: %u).", filename,
        (param ? "under " : ""), (param ? param : ""), get_last_os_error());
  else if (success)  // Succeeded to set ODirect
    g_eventLogger->info("Succeeded to set ODirect for file %s %s%s.", filename,
                        (param ? "under " : ""), (param ? param : ""));
  else  // Failed checking ODirect
    g_eventLogger->warning(
        "Failed to probe ODirect for file %s %s%s (errno: %u).", filename,
        (param ? "under " : ""), (param ? param : ""), get_last_os_error());
}

AsyncFile::odirect_set_log_state
    AsyncFile::odirect_set_log_bp[FsOpenReq::BP_MAX];

int AsyncFile::probe_directory_direct_io(const char param[],
                                         const char name[]) {
  int ret = -1;  // Could not check ODirect
  ndb_file file;
  file.create(name);  // Ignore failure, allow leftover file to be reused.
  if (file.open(name, FsOpenReq::OM_READWRITE) == 0) {
    file.set_block_size_and_alignment(NDB_O_DIRECT_WRITE_BLOCKSIZE,
                                      NDB_O_DIRECT_WRITE_ALIGNMENT);
    /*
     * direct_sync parameter in set_direct_io call is not relevant when
     * probing, uses false.
     */
    ret = file.set_direct_io(false);
    file.close();
    file.remove(name);
  }
  log_set_odirect_result(param, name, ret);
  return ret;
}

NdbOut &operator<<(NdbOut &out, const Request &req) {
  out << "[ Request: file: " << hex << req.file << " userRef: " << hex
      << req.theUserReference << " userData: " << dec << req.theUserPointer
      << " theFilePointer: " << req.theFilePointer << " action: ";
  out << Request::actionName(req.action);
  out << " ]";
  return out;
}
