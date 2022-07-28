/*****************************************************************************

Copyright (c) 2017, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/arch0log.h
 Innodb interface for log archive

 *******************************************************/

#ifndef ARCH_LOG_INCLUDE
#define ARCH_LOG_INCLUDE

#include "arch0arch.h"

/** File Node Iterator callback
@param[in]      file_name       NULL terminated file name
@param[in]      file_size       size of file in bytes
@param[in]      read_offset     offset to start reading from
@param[in]      ctx             context passed by caller
@return error code */
using Log_Arch_Cbk = int(char *file_name, uint64_t file_size,
                         uint64_t read_offset, void *ctx);

/** Redo Log archiver client context */
class Log_Arch_Client_Ctx {
 public:
  /** Constructor: Initialize elementsf */
  Log_Arch_Client_Ctx()
      : m_state(ARCH_CLIENT_STATE_INIT),
        m_group(nullptr),
        m_begin_lsn(LSN_MAX),
        m_end_lsn(LSN_MAX) {}

  /** Get redo file size for archived log file
  @return size of file in bytes */
  os_offset_t get_archived_file_size() const;

  /** Get redo header and trailer size
  @param[out]   header_sz       redo header size
  @param[out]   trailer_sz      redo trailer size */
  void get_header_size(uint &header_sz, uint &trailer_sz) const;

  /** Start redo log archiving
  @param[out]  header     buffer for redo header (caller must allocate)
  @param[in]   len        buffer length
  @return error code */
  int start(byte *header, uint len);

  /** Stop redo log archiving. Exact trailer length is returned as out
  parameter which could be less than the redo block size.
  @param[out]   trailer redo trailer. Caller must allocate buffer.
  @param[in,out]        len     trailer length
  @param[out]   offset  trailer block offset
  @return error code */
  int stop(byte *trailer, uint32_t &len, uint64_t &offset);

  /** Get archived data file details
  @param[in]    cbk_func        callback called for each file
  @param[in]    ctx             callback function context
  @return error code */
  int get_files(Log_Arch_Cbk *cbk_func, void *ctx);

  /** Release archived data so that system can purge it */
  void release();

 private:
  /** Archiver client state */
  Arch_Client_State m_state;

  /** Archive group the client is attached to */
  Arch_Group *m_group;

  /** Start LSN for archived data */
  lsn_t m_begin_lsn;

  /** Stop LSN for archived data */
  lsn_t m_end_lsn;
};

#endif /* ARCH_LOG_INCLUDE */
