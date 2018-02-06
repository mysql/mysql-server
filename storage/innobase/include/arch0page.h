/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/arch0page.h
 Innodb interface for modified page archive

 *******************************************************/

#ifndef ARCH_PAGE_INCLUDE
#define ARCH_PAGE_INCLUDE

#include "arch0arch.h"
#include "buf0buf.h"

/** Callback for retrieving archived page IDs
@param[in]	ctx		context passed by caller
@param[in]	buff		buffer with page IDs
@param[in]	num_pages	number of page IDs in buffer
@return error code */
using Page_Arch_Cbk = dberr_t(void *ctx, byte *buff, uint num_pages);

/** Dirty page archiver client context */
class Page_Arch_Client_Ctx {
 public:
  /** Constructor: Initialize elements */
  Page_Arch_Client_Ctx() : m_state(ARCH_CLIENT_STATE_INIT) {}

  /** Start dirty page tracking and archiving */
  dberr_t start();

  /** Stop dirty page tracking and archiving */
  dberr_t stop();

  /** Get archived page Ids
  @param[in]	cbk_func	called repeatedly with page ID buffer
  @param[in]	cbk_ctx		callback function context
  @param[in]	buff		buffer to fill page IDs
  @param[in]	buf_len		buffer length in bytes
  @return error code */
  dberr_t get_pages(Page_Arch_Cbk *cbk_func, void *cbk_ctx, byte *buff,
                    uint buf_len);

  /** Release archived data so that system can purge it */
  void release();

 private:
  /** Get page IDs from archived file
  @param[in]	read_pos	position to read from
  @param[in]	read_len	length of data to read
  @param[in]	read_buff	buffer to read page IDs
  @return error code */
  dberr_t get_from_file(Arch_Page_Pos *read_pos, uint read_len,
                        byte *read_buff);

 private:
  /** Page archiver client state */
  Arch_Client_State m_state;

  /** Archive group the client is attached to */
  Arch_Group *m_group;

  /** Start LSN for archived data */
  lsn_t m_start_lsn;

  /** Stop LSN for archived data */
  lsn_t m_stop_lsn;

  /** Start position in archived file group */
  Arch_Page_Pos m_start_pos;

  /** Stop position in archived file group */
  Arch_Page_Pos m_stop_pos;
};

#endif /* ARCH_PAGE_INCLUDE */
