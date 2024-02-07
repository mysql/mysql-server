/*****************************************************************************

Copyright (c) 2017, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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

/** Archived page header file size (RESET Page) in number of blocks. */
constexpr uint ARCH_PAGE_FILE_NUM_RESET_PAGE = 1;

/** Archived file header size. No file header for this version. */
constexpr uint ARCH_PAGE_FILE_HDR_SIZE =
    ARCH_PAGE_FILE_NUM_RESET_PAGE * ARCH_PAGE_BLK_SIZE;

/** @name Page Archive doublewrite buffer file name prefix and constant length
parameters. @{ */

/** Archive doublewrite buffer directory prefix */
constexpr char ARCH_DBLWR_DIR[] = "ib_dblwr";

/** Archive doublewrite buffer file prefix */
constexpr char ARCH_DBLWR_FILE[] = "dblwr_";

/** File name for the active file which indicates whether a group is active or
not. */
constexpr char ARCH_PAGE_GROUP_ACTIVE_FILE_NAME[] = "active";

/** Archive doublewrite buffer number of files */
constexpr uint ARCH_DBLWR_NUM_FILES = 1;

/** Archive doublewrite buffer file capacity in no. of blocks */
constexpr uint ARCH_DBLWR_FILE_CAPACITY = 3;

/** @} */

/** @name Archive block header elements
@{ */

/** Block Header: Version is in first 1 byte. */
constexpr uint ARCH_PAGE_BLK_HEADER_VERSION_OFFSET = 0;

/** Block Header: Block Type is in next 1 byte. */
constexpr uint ARCH_PAGE_BLK_HEADER_TYPE_OFFSET = 1;

/** Block Header: Checksum is in next 4 bytes. */
constexpr uint ARCH_PAGE_BLK_HEADER_CHECKSUM_OFFSET = 2;

/** Block Header: Data length is in next 2 bytes. */
constexpr uint ARCH_PAGE_BLK_HEADER_DATA_LEN_OFFSET = 6;

/** Block Header: Stop LSN is in next 8 bytes */
constexpr uint ARCH_PAGE_BLK_HEADER_STOP_LSN_OFFSET = 8;

/** Block Header: Reset LSN is in next 8 bytes */
constexpr uint ARCH_PAGE_BLK_HEADER_RESET_LSN_OFFSET = 16;

/** Block Header: Block number is in next 8 bytes */
constexpr uint ARCH_PAGE_BLK_HEADER_NUMBER_OFFSET = 24;

/** Block Header: Total length.
Keep header length in multiple of #ARCH_BLK_PAGE_ID_SIZE */
constexpr uint ARCH_PAGE_BLK_HEADER_LENGTH = 32;

/** @} */

/** @name Page Archive reset block elements size.
@{ */

/** Serialized Reset ID: Reset LSN total size */
constexpr uint ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE = 8;

/** Serialized Reset ID: Reset block number size */
constexpr uint ARCH_PAGE_FILE_HEADER_RESET_BLOCK_NUM_SIZE = 2;

/** Serialized Reset ID: Reset block offset size */
constexpr uint ARCH_PAGE_FILE_HEADER_RESET_BLOCK_OFFSET_SIZE = 2;

/** Serialized Reset ID: Reset position total size */
constexpr uint ARCH_PAGE_FILE_HEADER_RESET_POS_SIZE =
    ARCH_PAGE_FILE_HEADER_RESET_BLOCK_NUM_SIZE +
    ARCH_PAGE_FILE_HEADER_RESET_BLOCK_OFFSET_SIZE;

/** @} */

/** @name Page Archive data block elements
@{ */

/** Serialized page ID: tablespace ID in First 4 bytes */
constexpr uint ARCH_BLK_SPCE_ID_OFFSET = 0;

/** Serialized page ID: Page number in next 4 bytes */
constexpr uint ARCH_BLK_PAGE_NO_OFFSET = 4;

/** Serialized page ID: Total length */
constexpr uint ARCH_BLK_PAGE_ID_SIZE = 8;

/** @} */

/** Number of memory blocks */
constexpr uint ARCH_PAGE_NUM_BLKS = 32;

/** Archived file format version */
constexpr uint ARCH_PAGE_FILE_VERSION = 1;

#ifdef UNIV_DEBUG
/** Archived page file default size in number of blocks. */
extern uint ARCH_PAGE_FILE_CAPACITY;

/** Archived page data file size (without header) in number of blocks. */
extern uint ARCH_PAGE_FILE_DATA_CAPACITY;
#else
/** Archived page file default size in number of blocks. */
constexpr uint ARCH_PAGE_FILE_CAPACITY =
    (ARCH_PAGE_BLK_SIZE - ARCH_PAGE_BLK_HEADER_LENGTH) / ARCH_BLK_PAGE_ID_SIZE;

/** Archived page data file size (without header) in number of blocks. */
constexpr uint ARCH_PAGE_FILE_DATA_CAPACITY =
    ARCH_PAGE_FILE_CAPACITY - ARCH_PAGE_FILE_NUM_RESET_PAGE;
#endif

/** Threshold for page archive reset. Attach to current reset if the number of
tracked pages between the reset request and the current reset is less than this
threshold as we allow only one reset per data block. */
constexpr uint ARCH_PAGE_RESET_THRESHOLD =
    (ARCH_PAGE_BLK_SIZE - ARCH_PAGE_BLK_HEADER_LENGTH) / ARCH_BLK_PAGE_ID_SIZE;

/** Callback for retrieving archived page IDs
@param[in]      ctx             context passed by caller
@param[in]      buff            buffer with page IDs
@param[in]      num_pages       number of page IDs in buffer
@return error code */
using Page_Arch_Cbk = int(void *ctx, byte *buff, uint num_pages);

/** Callback function to check if we need to wait for flush archiver to flush
more blocks */
using Page_Wait_Flush_Archiver_Cbk = std::function<bool(void)>;

/** Dirty page archiver client context */
class Page_Arch_Client_Ctx {
 public:
  /** Constructor: Initialize elements
  @param[in]    is_durable      true if the client requires durability, else
  false */
  Page_Arch_Client_Ctx(bool is_durable) : m_is_durable(is_durable) {
    m_start_pos.init();
    m_stop_pos.init();
    mutex_create(LATCH_ID_PAGE_ARCH_CLIENT, &m_mutex);
  }

  /** Destructor. */
  ~Page_Arch_Client_Ctx() { mutex_free(&m_mutex); }

  /** Start dirty page tracking and archiving
  @param[in]    recovery        true if the tracking is being started as part of
  recovery process
  @param[out]   start_id    fill the start lsn
  @return error code. */
  int start(bool recovery, uint64_t *start_id);

  /** Stop dirty page tracking and archiving
  @param[out]   stop_id fill the stop lsn
  @return error code. */
  int stop(uint64_t *stop_id);

  /** Release archived data so that system can purge it */
  void release();

  /** Initialize context during recovery.
  @param[in]    group           Group which needs to be attached to the client
  @param[in]    last_lsn        last reset lsn
  @return error code. */
  int init_during_recovery(Arch_Group *group, lsn_t last_lsn);

  /** Check if this client context is active.
  @return true if active, else false */
  bool is_active() const { return (m_state == ARCH_CLIENT_STATE_STARTED); }

  /** Get archived page Ids.
  Attempt to read blocks directly from in memory buffer. If overwritten,
  copy from archived files.
  @param[in]    cbk_func        called repeatedly with page ID buffer
  @param[in]    cbk_ctx         callback function context
  @param[in,out]        buff            buffer to fill page IDs
  @param[in]    buf_len         buffer length in bytes
  @return error code */
  int get_pages(Page_Arch_Cbk *cbk_func, void *cbk_ctx, byte *buff,
                uint buf_len);

#ifdef UNIV_DEBUG
  /** Print information related to the archiver client for debugging purposes.
   */
  void print();
#endif

  /** Disable copy construction */
  Page_Arch_Client_Ctx(Page_Arch_Client_Ctx const &) = delete;

  /** Disable assignment */
  Page_Arch_Client_Ctx &operator=(Page_Arch_Client_Ctx const &) = delete;

 private:
  /** Acquire client archiver mutex.
  It synchronizes members on concurrent start and stop operations. */
  void arch_client_mutex_enter() { mutex_enter(&m_mutex); }

  /** Release client archiver mutex */
  void arch_client_mutex_exit() { mutex_exit(&m_mutex); }

 private:
  /** Page archiver client state */
  Arch_Client_State m_state{ARCH_CLIENT_STATE_INIT};

  /** Archive group the client is attached to */
  Arch_Group *m_group{nullptr};

  /** True if the client requires durablity */
  bool m_is_durable;

  /** Start LSN for archived data */
  lsn_t m_start_lsn{LSN_MAX};

  /** Stop LSN for archived data */
  lsn_t m_stop_lsn{LSN_MAX};

  /** Reset LSN at the time of last reset. */
  lsn_t m_last_reset_lsn{LSN_MAX};

  /** Start position for client in archived file group */
  Arch_Page_Pos m_start_pos;

  /** Stop position for client in archived file group */
  Arch_Page_Pos m_stop_pos;

  /** Mutex protecting concurrent operation on data */
  ib_mutex_t m_mutex;
};

#endif /* ARCH_PAGE_INCLUDE */
