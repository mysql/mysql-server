/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file arch/arch0page.cc
 Innodb implementation for page archive

 *******************************************************/

#include "arch0page.h"
#include "srv0start.h"

/** Memory block size */
uint ARCH_PAGE_BLK_SIZE = UNIV_PAGE_SIZE;

/** Number of memory blocks */
uint ARCH_PAGE_NUM_BLKS = 32;

/** Archived file header size. No file header for this version. */
const uint ARCH_PAGE_FILE_HDR_SIZE = 0;

/** Archived file format version */
const uint ARCH_PAGE_FILE_VERSION = 1;

/** @name Page Archive block header elements //@{ */

/** Block Header: Version is in first 4 bytes */
const uint ARCH_PAGE_BLK_HEADER_VERSION = 0;

/** Block Header: Checksum is in next 4 bytes */
const uint ARCH_PAGE_BLK_HEADER_CHECKSUM = 4;

/** Block Header: Data length is in next 4 bytes.
Keep next 4 bytes free for future. */
const uint ARCH_PAGE_BLK_HEADER_DATA_LEN = 8;

/** Block Header: Start LSN is in next 8 bytes */
const uint ARCH_PAGE_BLK_HEADER_START_LSN = 16;
/** Block Header: Start LSN is in next 8 bytes */
const uint ARCH_PAGE_BLK_HEADER_NUMBER = 24;

/** Block Header: Total length.
Keep header length in multiple of #ARCH_BLK_PAGE_ID_SIZE */
const uint ARCH_PAGE_BLK_HEADER_LENGTH = 32;

/** //@} */

/** Serialized page ID: tablespace ID in First 4 bytes */
const uint ARCH_BLK_SPCE_ID_OFFSET = 0;

/** Serialized page ID: Page number in next 4 bytes */
const uint ARCH_BLK_PAGE_NO_OFFSET = 4;

/** Serialized page ID: Total length */
const uint ARCH_BLK_PAGE_ID_SIZE = 8;

/** Threshold for page archive reset. Attach to current, if the number of
tracked pages is less than 128 */
const uint ARCH_PAGE_RESET_THRESHOLD = ARCH_BLK_PAGE_ID_SIZE * 128;

/** Archived page file default size in number of blocks. */
const uint ARCH_PAGE_FILE_CAPACITY = 8 * 1024;

/** Start dirty page tracking and archiving */
dberr_t Page_Arch_Client_Ctx::start() {
  dberr_t err;

  err = arch_page_sys->start(&m_group, &m_start_lsn, &m_start_pos, false);

  if (err != DB_SUCCESS) {
    return (err);
  }

  m_state = ARCH_CLIENT_STATE_STARTED;

  ib::info(ER_IB_MSG_20) << "Clone Start PAGE ARCH : start LSN : "
                         << m_start_lsn << ", checkpoint LSN : "
                         << log_sys->last_checkpoint_lsn.load();

  return (DB_SUCCESS);
}

/** Stop dirty page tracking and archiving */
dberr_t Page_Arch_Client_Ctx::stop() {
  dberr_t err;

  err = arch_page_sys->stop(m_group, &m_stop_lsn, &m_stop_pos);

  if (err != DB_SUCCESS) {
    return (err);
  }

  m_state = ARCH_CLIENT_STATE_STOPPED;

  ib::info(ER_IB_MSG_21) << "Clone Stop  PAGE ARCH : end   LSN : " << m_stop_lsn
                         << ", checkpoint LSN : "
                         << log_sys->last_checkpoint_lsn.load();

  return (DB_SUCCESS);
}

/** Get page IDs from archived file
@param[in]	read_pos	position to read from
@param[in]	read_len	length of data to read
@param[in]	read_buff	buffer to read page IDs
@return error code */
dberr_t Page_Arch_Client_Ctx::get_from_file(Arch_Page_Pos *read_pos,
                                            uint read_len, byte *read_buff) {
  dberr_t err;
  char errbuf[MYSYS_STRERROR_SIZE];

  uint file_index;
  char file_name[MAX_ARCH_PAGE_FILE_NAME_LEN];

  /* Build file name */
  file_index =
      static_cast<uint>(read_pos->m_block_num / ARCH_PAGE_FILE_CAPACITY);

  m_group->get_file_name(file_index, file_name, MAX_ARCH_PAGE_FILE_NAME_LEN);

  /* Find offset to read from. */
  pfs_os_file_t file;
  os_offset_t offset;
  bool success;

  offset = read_pos->m_block_num % ARCH_PAGE_FILE_CAPACITY;
  offset *= ARCH_PAGE_BLK_SIZE;
  offset += read_pos->m_offset;

  /* Open file in read only mode. */
  file = os_file_create(innodb_arch_file_key, file_name, OS_FILE_OPEN,
                        OS_FILE_NORMAL, OS_CLONE_LOG_FILE, true, &success);

  if (!success) {
    my_error(ER_CANT_OPEN_FILE, MYF(0), file_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (DB_CANNOT_OPEN_FILE);
  }

  /* Read from file to the user buffer. */
  IORequest request(IORequest::READ);

  request.disable_compression();
  request.clear_encrypted();

  err = os_file_read(request, file, read_buff, offset, read_len);

  os_file_close(file);

  if (err != DB_SUCCESS) {
    my_error(ER_ERROR_ON_READ, MYF(0), file_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
  }

  return (err);
}

/** Get archived page Ids.
Attempt to read blocks directly from in memory buffer. If overwritten,
copy from archived files.
@param[in]	cbk_func	called repeatedly with page ID buffer
@param[in]	cbk_ctx		callback function context
@param[in]	buff		buffer to fill page IDs
@param[in]	buf_len		buffer length in bytes
@return error code */
dberr_t Page_Arch_Client_Ctx::get_pages(Page_Arch_Cbk *cbk_func, void *cbk_ctx,
                                        byte *buff, uint buf_len) {
  dberr_t err = DB_SUCCESS;
  bool success;
  uint num_pages;
  uint read_len;
  Arch_Page_Pos cur_pos;

  ut_ad(m_state == ARCH_CLIENT_STATE_STOPPED);

  cur_pos = m_start_pos;

  while (true) {
    ut_ad(cur_pos.m_block_num <= m_stop_pos.m_block_num);

    /* Check if last block */
    if (cur_pos.m_block_num >= m_stop_pos.m_block_num) {
      if (cur_pos.m_offset > m_stop_pos.m_offset) {
        ut_ad(false);
        my_error(ER_INTERNAL_ERROR, MYF(0), "Wrong Archiver page offset");

        err = DB_ERROR;
        break;
      }

      read_len = m_stop_pos.m_offset - cur_pos.m_offset;

      if (read_len == 0) {
        break;
      }

    } else {
      if (cur_pos.m_offset > ARCH_PAGE_BLK_SIZE) {
        ut_ad(false);

        my_error(ER_INTERNAL_ERROR, MYF(0), "Wrong Archiver page offset");

        err = DB_ERROR;
        break;
      }

      read_len = ARCH_PAGE_BLK_SIZE - cur_pos.m_offset;

      /* Move to next block. */
      if (read_len == 0) {
        cur_pos.set_next();
        continue;
      }
    }

    if (read_len > buf_len) {
      read_len = buf_len;
    }

    /* Attempt to read from in memory buffer. */
    success = arch_page_sys->get_pages(m_group, &cur_pos, read_len, buff);

    if (!success) {
      /* The buffer is overwritten. Read from file. */
      err = get_from_file(&cur_pos, read_len, buff);

      if (err != DB_SUCCESS) {
        return (err);
      }
    }

    cur_pos.m_offset += read_len;
    num_pages = read_len / ARCH_BLK_PAGE_ID_SIZE;

    err = cbk_func(cbk_ctx, buff, num_pages);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }
  return (err);
}

/** Release archived data so that system can purge it */
void Page_Arch_Client_Ctx::release() {
  if (m_state == ARCH_CLIENT_STATE_INIT) {
    return;
  }

  if (m_state == ARCH_CLIENT_STATE_STARTED) {
    stop();
  }

  ut_ad(m_state == ARCH_CLIENT_STATE_STOPPED);

  arch_page_sys->release(m_group, false);
  m_state = ARCH_CLIENT_STATE_INIT;
}

/** Wait till the block is flushed and is ready for write
@return true, if the block is flushed */
bool Arch_Block::wait_flush() {
  uint count = 0;

  while (m_state == ARCH_BLOCK_READY_TO_FLUSH) {
    /* Need to wait for flush. We don't expect it
    to happen normally. With no duplicate page ID
    dirty page growth should be very slow. */

    arch_page_sys->arch_oper_mutex_exit();
    os_event_set(archiver_thread_event);

    /* Sleep for 100ms */
    os_thread_sleep(100000);

    ++count;

    if (count % 50 == 0) {
      ib::warn(ER_IB_MSG_22) << "Page Tracking Write: Waiting"
                                " for archiver to flush blocks.";

      if (count > 600) {
        /* Wait too long - 1 minutes */
        return (false);
      }
    }

    if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
      return (false);
    }

    arch_page_sys->arch_oper_mutex_enter();
  }

  return (true);
}

/** Set the block ready to begin writing page ID
@param[in]	pos		position to initiate block number
@param[in]	start_lsn	start LSN for group last reset */
void Arch_Block::begin_write(Arch_Page_Pos *pos, lsn_t start_lsn) {
  m_state = ARCH_BLOCK_ACTIVE;
  m_number = pos->m_block_num;

  m_oldest_lsn = LSN_MAX;
  m_start_lsn = start_lsn;

  m_data_len = 0;
}

/** End writing to a block.
Change state to #ARCH_BLOCK_READY_TO_FLUSH */
void Arch_Block::end_write() { m_state = ARCH_BLOCK_READY_TO_FLUSH; }

/** Add page ID to current block
@param[in]	page	page from buffer pool
@param[in]	pos	Archiver current position
@return true, if successful
        false, if no more space in current block */
bool Arch_Block::add_page(buf_page_t *page, Arch_Page_Pos *pos) {
  space_id_t space_id;
  page_no_t page_num;
  byte *data_ptr;

  ut_ad(pos->m_offset <= ARCH_PAGE_BLK_SIZE);

  if ((pos->m_offset + ARCH_BLK_PAGE_ID_SIZE) > ARCH_PAGE_BLK_SIZE) {
    ut_ad(pos->m_offset == ARCH_PAGE_BLK_SIZE);
    return (false);
  }

  data_ptr = m_data + pos->m_offset;

  /* Write serialized page ID: tablespace ID and offset */
  space_id = page->id.space();
  page_num = page->id.page_no();

  mach_write_to_4(data_ptr + ARCH_BLK_SPCE_ID_OFFSET, space_id);
  mach_write_to_4(data_ptr + ARCH_BLK_PAGE_NO_OFFSET, page_num);

  /* Update position. */
  pos->m_offset += ARCH_BLK_PAGE_ID_SIZE;
  m_data_len += ARCH_BLK_PAGE_ID_SIZE;

  /* Update oldest LSN from page. */
  if (m_oldest_lsn > page->oldest_modification) {
    m_oldest_lsn = page->oldest_modification;
  }

  return (true);
}

/** Copy page Ids from in block at read position
@param[in]	read_pos	current read position
@param[in]	read_len	length of data to copy
@param[out]	read_buff	buffer to copy page IDs.
                                Caller must allocate the buffer.
@return true, if successful
        false, if block is already overwritten */
bool Arch_Block::copy_pages(Arch_Page_Pos *read_pos, uint read_len,
                            byte *read_buff) {
  ut_ad(m_state != ARCH_BLOCK_INIT);

  if (m_number != read_pos->m_block_num) {
    /* The block is already overwritten. */
    return (false);
  }

  byte *src;

  ut_ad(m_data_len + ARCH_PAGE_BLK_HEADER_LENGTH >=
        read_pos->m_offset + read_len);

  src = m_data + read_pos->m_offset;

  memcpy(read_buff, src, read_len);

  return (true);
}

/** Flush this block to the file group
@param[in]	file_group	current archive group
@param[in]	type		flush type
@return error code. */
dberr_t Arch_Block::flush(Arch_Group *file_group, Arch_Blk_Flush_Type type) {
  dberr_t err;
  uint32_t checksum;

  /* Support partial page flush for durable tracking */
  ut_a(type == ARCH_FLUSH_NORMAL);

  ut_ad(m_state == ARCH_BLOCK_READY_TO_FLUSH);

  /* Update block header. */
  mach_write_to_4(m_data + ARCH_PAGE_BLK_HEADER_VERSION,
                  ARCH_PAGE_FILE_VERSION);
  mach_write_to_4(m_data + ARCH_PAGE_BLK_HEADER_DATA_LEN, m_data_len);
  mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_START_LSN, m_start_lsn);
  mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_NUMBER, m_number);

  checksum = ut_crc32(m_data + ARCH_PAGE_BLK_HEADER_LENGTH,
                      m_size - ARCH_PAGE_BLK_HEADER_LENGTH);

  mach_write_to_4(m_data + ARCH_PAGE_BLK_HEADER_CHECKSUM, checksum);

  /* Write block to file. */
  err = file_group->write_to_file(nullptr, m_data, m_size);

  return (err);
}

/** Initialize a position */
void Arch_Page_Pos::init() {
  m_block_num = 0;
  m_offset = ARCH_PAGE_BLK_HEADER_LENGTH;
}

/** Position in the beginning of next block */
void Arch_Page_Pos::set_next() {
  m_block_num++;
  m_offset = ARCH_PAGE_BLK_HEADER_LENGTH;
}

/** Allocate buffer and initialize blocks
@return true, if successful */
bool ArchPageData::init() {
  uint alloc_size;
  uint index;
  byte *mem_ptr;

  ut_ad(m_buffer == nullptr);

  m_block_size = ARCH_PAGE_BLK_SIZE;
  m_num_blocks = ARCH_PAGE_NUM_BLKS;

  /* block size and number must be in power of 2 */
  ut_ad(ut_is_2pow(m_block_size));
  ut_ad(ut_is_2pow(m_num_blocks));

  alloc_size = m_block_size * m_num_blocks;
  alloc_size += m_block_size;

  /* Allocate buffer for memory blocks. */
  m_buffer = static_cast<byte *>(ut_zalloc(alloc_size, mem_key_archive));

  if (m_buffer == nullptr) {
    return (false);
  }

  mem_ptr = static_cast<byte *>(ut_align(m_buffer, m_block_size));

  Arch_Block *cur_blk;

  /* Create memory blocks. */
  for (index = 0; index < m_num_blocks; index++) {
    cur_blk = UT_NEW(Arch_Block(mem_ptr, m_block_size), mem_key_archive);

    if (cur_blk == nullptr) {
      return (false);
    }

    m_blocks.push_back(cur_blk);
    mem_ptr += m_block_size;
  }

  return (true);
}

/** Delete blocks and buffer */
void ArchPageData::clean() {
  for (auto &block : m_blocks) {
    UT_DELETE(block);
  }

  if (m_buffer != nullptr) {
    ut_free(m_buffer);
  }
}

/** Get the block for a position
@param[in]	pos	position in page archive sys
@return page archive in memory block */
Arch_Block *ArchPageData::get_block(Arch_Page_Pos *pos) {
  uint index;

  /* index = block_num % m_num_blocks */
  ut_ad(ut_is_2pow(m_num_blocks));
  index = pos->m_block_num & (m_num_blocks - 1);

  return (m_blocks[index]);
}

/** Check and add page ID to archived data.
Check for duplicate page.
@param[in]	bpage		page to track
@param[in]	track_lsn	LSN when tracking started
@param[in]	frame_lsn	current LSN of the page
@param[in]	force		if true, add page ID without check */
void Arch_Page_Sys::track_page(buf_page_t *bpage, lsn_t track_lsn,
                               lsn_t frame_lsn, bool force) {
  Arch_Block *cur_blk;
  uint count = 0;

  if (!force) {
    /* If the frame LSN is bigger than track LSN, it
    is already added to tracking list. */
    if (frame_lsn > track_lsn) {
      return;
    }
  }

  /* We need to track this page. */
  arch_oper_mutex_enter();

  while (true) {
    if (m_state != ARCH_STATE_ACTIVE) {
      break;
    }

    /* Can possibly loop only two times. */
    if (count >= 2) {
      if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
        return;
      }

      ut_ad(false);

      ib::warn(ER_IB_MSG_23) << "Fail to add page for tracking."
                             << " Space ID: " << bpage->id.space()
                             << " Page NO: " << bpage->id.page_no();
      return;
    }

    cur_blk = m_data.get_block(&m_write_pos);

    if (cur_blk->get_state() == ARCH_BLOCK_ACTIVE) {
      if (cur_blk->add_page(bpage, &m_write_pos)) {
        /* page added successfully. */
        break;
      }

      /* Current block is full. Move to next block. */
      cur_blk->end_write();
      m_write_pos.set_next();

      os_event_set(archiver_thread_event);

      ++count;
      continue;

    } else if (cur_blk->get_state() == ARCH_BLOCK_INIT ||
               cur_blk->get_state() == ARCH_BLOCK_FLUSHED) {
      cur_blk->begin_write(&m_write_pos, m_last_lsn);
      cur_blk->add_page(bpage, &m_write_pos);

      /* page added successfully. */
      break;

    } else {
      bool success;

      ut_a(cur_blk->get_state() == ARCH_BLOCK_READY_TO_FLUSH);
      /* Might release operation mutex temporarily. Need to
      loop again verifying the state. */
      success = cur_blk->wait_flush();
      count = success ? 0 : 2;

      continue;
    }
  }

  arch_oper_mutex_exit();
}

/** Get page IDs from a specific position.
Caller must ensure that read_len doesn't exceed the block.
@param[in]	group		archive group
@param[in]	read_pos	position in archived data
@param[in]	read_len	amount of data to read
@param[out]	read_buff	buffer to return the page IDs.
                                Caller must allocate the buffer. */
bool Arch_Page_Sys::get_pages(Arch_Group *group, Arch_Page_Pos *read_pos,
                              uint read_len, byte *read_buff) {
  Arch_Block *read_blk;
  bool success;

  arch_oper_mutex_enter();

  if (group != m_current_group) {
    arch_oper_mutex_exit();
    return (false);
  }

  /* Get the block to read from. */
  read_blk = m_data.get_block(read_pos);

  /* Read from the block. */
  success = read_blk->copy_pages(read_pos, read_len, read_buff);

  arch_oper_mutex_exit();

  return (success);
}

/** Wait for archive system to come out of #ARCH_STATE_PREPARE_IDLE.
If the system is preparing to idle, #start needs to wait
for it to come to idle state.
@return true, if successful
        false, if needs to abort */
bool Arch_Page_Sys::wait_idle() {
  uint count = 0;

  while (m_state == ARCH_STATE_PREPARE_IDLE) {
    arch_mutex_exit();

    os_event_set(archiver_thread_event);

    /* Sleep for 100ms. */
    os_thread_sleep(100000);

    ++count;

    if (count % 50 == 0) {
      ib::info(ER_IB_MSG_24) << "Page Tracking IDLE: Waiting for"
                                " archiver to flush last blocks.";

      if (count > 600) {
        /* Wait too long - 1 minute */
        ib::error(ER_IB_MSG_25) << "Page Tracking wait too long";
        return (false);
      }
    }

    if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
      return (false);
    }

    arch_mutex_enter();
  }

  return (true);
}

/** Check if the gap from last reset is short.
If not many page IDs are added till last reset, we avoid
taking a new reset point
@return true, if the gap is small. */
bool Arch_Page_Sys::is_gap_small() {
  ib_uint64_t next_block_num;

  next_block_num = m_last_pos.m_block_num + 1;

  if (next_block_num == m_write_pos.m_block_num &&
      m_write_pos.m_offset < ARCH_PAGE_RESET_THRESHOLD) {
    return true;
  }

  return (false);
}

/** Track pages for which IO is already started. */
void Arch_Page_Sys::track_initial_pages() {
  uint index;
  buf_pool_t *buf_pool;

  for (index = 0; index < srv_buf_pool_instances; ++index) {
    buf_pool = buf_pool_from_array(index);

    mutex_enter(&buf_pool->flush_state_mutex);

    /* Page tracking must already be active. */
    ut_ad(buf_pool->track_page_lsn != LSN_MAX);

    buf_flush_list_mutex_enter(buf_pool);

    buf_page_t *bpage;
    uint page_count;
    uint skip_count;

    bpage = UT_LIST_GET_LAST(buf_pool->flush_list);
    page_count = 0;
    skip_count = 0;

    /* Add all pages for which IO is already started. */
    while (bpage != NULL) {
      if (fsp_is_system_temporary(bpage->id.space())) {
        bpage = UT_LIST_GET_PREV(list, bpage);
        continue;
      }

      /* There cannot be any more IO fixed pages. */

      /* Check if we could finish traversing flush list
      earlier. Order of pages in flush list became relaxed,
      but the distortion is limited by the flush_order_lag.

      You can think about this in following way: pages
      start to travel to flush list when they have the
      oldest_modification field assigned. They start in
      proper order, but they can be delayed when traveling
      and they can finish their travel in different order.

      However page is disallowed to finish its travel,
      if there is other page, which started much much
      earlier its travel and still haven't finished.
      The "much much" part is defined by the maximum
      allowed lag - log_buffer_flush_order_lag(). */
      if (bpage->oldest_modification >
          buf_pool->max_lsn_io + log_buffer_flush_order_lag(*log_sys)) {
        /* All pages with oldest_modification
        smaller than bpage->oldest_modification
        minus the flush_order_lag have already
        been traversed. So there is no page which:
                - we haven't traversed
                - and has oldest_modification
                  smaller than buf_pool->max_lsn_io. */
        break;
      }

      if (buf_page_get_io_fix_unlocked(bpage) == BUF_IO_WRITE) {
        /* IO has already started. Must add the page */
        track_page(bpage, LSN_MAX, LSN_MAX, true);
        ++page_count;
      } else {
        ++skip_count;
      }

      bpage = UT_LIST_GET_PREV(list, bpage);
    }

    buf_flush_list_mutex_exit(buf_pool);
    mutex_exit(&buf_pool->flush_state_mutex);
  }
}

/** Enable tracking pages in all buffer pools.
@param[in]	tracking_lsn	track pages from this LSN */
void Arch_Page_Sys::set_tracking_buf_pool(lsn_t tracking_lsn) {
  uint index;
  buf_pool_t *buf_pool;

  for (index = 0; index < srv_buf_pool_instances; ++index) {
    buf_pool = buf_pool_from_array(index);

    mutex_enter(&buf_pool->flush_state_mutex);

    ut_ad(buf_pool->track_page_lsn == LSN_MAX ||
          buf_pool->track_page_lsn <= tracking_lsn);

    buf_pool->track_page_lsn = tracking_lsn;

    mutex_exit(&buf_pool->flush_state_mutex);
  }
}

/** Start dirty page ID archiving.
If archiving is already in progress, the client
is attached to current group.
@param[out]	group		page archive group
@param[out]	start_lsn	start lsn for client
@param[out]	start_pos	Start position in archived data
@param[in]	is_durable	if client needs durable archiving
@return error code */
dberr_t Arch_Page_Sys::start(Arch_Group **group, lsn_t *start_lsn,
                             Arch_Page_Pos *start_pos, bool is_durable) {
  dberr_t err = DB_SUCCESS;
  bool start_archiver = true;
  bool attach_to_current = false;

  lsn_t log_sys_lsn = LSN_MAX;

  /* Check if archiver task needs to be started. */
  if (arch_log_sys) {
    arch_log_sys->arch_mutex_enter();
    arch_mutex_enter();

    start_archiver = is_init() && arch_log_sys->is_init();
    arch_log_sys->arch_mutex_exit();
  } else {
    arch_mutex_enter();
    start_archiver = is_init();
  }

  /* Wait for idle state, if preparing to idle. */
  if (!wait_idle()) {
    if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
    } else {
      my_error(ER_INTERNAL_ERROR, MYF(0), "Page Archiver wait too long");
    }

    return (DB_ERROR);
  }

  switch (m_state) {
    case ARCH_STATE_ABORT:
      arch_mutex_exit();
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      return (DB_INTERRUPTED);

    case ARCH_STATE_IDLE:
    case ARCH_STATE_INIT:
      ut_ad(m_current_group == nullptr);
      /* Fall through */

    case ARCH_STATE_ACTIVE:

      if (m_current_group != nullptr) {
        /* If gap is small, just attach to current group */
        attach_to_current = is_gap_small();
      }

      if (!attach_to_current) {
        log_buffer_x_lock_enter(*log_sys);

        log_sys_lsn = log_get_lsn(*log_sys);

        /* Enable/Reset buffer pool page tracking. */
        set_tracking_buf_pool(log_sys_lsn);

        /* Take operation mutex before releasing log_sys to
        ensure that all pages modified after log_sys_lsn are
        tracked. */
        arch_oper_mutex_enter();

        log_buffer_x_lock_exit(*log_sys);
      }
      break;

    case ARCH_STATE_PREPARE_IDLE:
    default:
      ut_ad(false);
  }

  if (is_init() && !m_data.init()) {
    ut_ad(!attach_to_current);
    arch_oper_mutex_exit();
    arch_mutex_exit();

    my_error(ER_OUTOFMEMORY, MYF(0), ARCH_PAGE_BLK_SIZE);
    return (DB_OUT_OF_MEMORY);
  }

  /* Start archiver background task. */
  if (start_archiver && !start_archiver_background()) {
    ut_ad(!attach_to_current);
    arch_oper_mutex_exit();
    arch_mutex_exit();

    ib::error(ER_IB_MSG_26) << "Could not start"
                            << " Archiver background task";
    return (DB_ERROR);
  }

  /* Create a new archive group. */
  if (m_current_group == nullptr) {
    ut_ad(!attach_to_current);

    m_last_pos.init();
    m_flush_pos.init();
    m_write_pos.init();

    m_last_lsn = log_sys_lsn;

    m_current_group =
        UT_NEW(Arch_Group(log_sys_lsn, ARCH_PAGE_FILE_HDR_SIZE, &m_mutex),
               mem_key_archive);

    if (m_current_group == nullptr) {
      arch_oper_mutex_exit();
      arch_mutex_exit();

      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Arch_Group));
      return (DB_OUT_OF_MEMORY);
    }

    /* Initialize archiver file context. */
    err = m_current_group->init_file_ctx(
        ARCH_DIR, ARCH_PAGE_DIR, ARCH_PAGE_FILE, 0,
        static_cast<ib_uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_PAGE_FILE_CAPACITY);

    if (err != DB_SUCCESS) {
      arch_oper_mutex_exit();
      arch_mutex_exit();

      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Arch_File_Ctx));
      return (err);
    }

    m_group_list.push_back(m_current_group);

  } else if (!attach_to_current) {
    Arch_Block *cur_blk;

    /* For reset, move to next data block. */
    cur_blk = m_data.get_block(&m_write_pos);
    cur_blk->end_write();

    m_write_pos.set_next();
    os_event_set(archiver_thread_event);

    m_last_lsn = log_sys_lsn;
    m_last_pos = m_write_pos;
  }

  if (!attach_to_current) {
    m_state = ARCH_STATE_ACTIVE;
    arch_oper_mutex_exit();

    /* Add pages to tracking for which IO has already started. */
    track_initial_pages();
  }

  /* Attach to the group. */
  m_current_group->attach(m_last_lsn, m_last_pos.m_block_num, is_durable);

  *group = m_current_group;

  *start_pos = m_last_pos;
  *start_lsn = m_last_lsn;

  arch_mutex_exit();

  /* Make sure all written pages are synced to disk. */
  log_request_checkpoint(*log_sys, false);

  return (DB_SUCCESS);
}

/** Stop dirty page ID archiving.
If other clients are there, the client is detached from
the current group.
@param[out]	group		page archive group
@param[out]	stop_lsn	stop lsn for client
@param[out]	stop_pos	stop position in archived data
@return error code */
dberr_t Arch_Page_Sys::stop(Arch_Group *group, lsn_t *stop_lsn,
                            Arch_Page_Pos *stop_pos) {
  dberr_t err = DB_SUCCESS;
  Arch_Block *cur_blk;
  uint count = 0;

  arch_mutex_enter();

  log_buffer_x_lock_enter(*log_sys);

  *stop_lsn = log_get_lsn(*log_sys);

  count = group->detach(*stop_lsn);

  /* If no other active client, let the system get into
  idle state. */
  if (count == 0 && m_state != ARCH_STATE_ABORT) {
    ut_ad(m_state == ARCH_STATE_ACTIVE);

    set_tracking_buf_pool(LSN_MAX);

    arch_oper_mutex_enter();

    log_buffer_x_lock_exit(*log_sys);

    m_state = ARCH_STATE_PREPARE_IDLE;

    *stop_pos = m_write_pos;

    cur_blk = m_data.get_block(&m_write_pos);

    /* If any page ID is written to current page, let it flush. */
    if (m_write_pos.m_offset > ARCH_PAGE_BLK_HEADER_LENGTH) {
      cur_blk->end_write();
      m_write_pos.set_next();
    }

    os_event_set(archiver_thread_event);
  } else {
    log_buffer_x_lock_exit(*log_sys);

    arch_oper_mutex_enter();

    *stop_pos = m_write_pos;
  }

  if (m_state == ARCH_STATE_ABORT) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    err = DB_INTERRUPTED;
  }

  arch_oper_mutex_exit();
  arch_mutex_exit();

  return (err);
}

/** Release the current group from client.
@param[in]	group		group the client is attached to
@param[in]	is_durable	if client needs durable archiving */
void Arch_Page_Sys::release(Arch_Group *group, bool is_durable) {
  uint ref_count;

  arch_mutex_enter();

  ref_count = group->release(is_durable);

  if (ref_count != 0 || group->is_active()) {
    arch_mutex_exit();
    return;
  }

  ut_ad(group != m_current_group);

  m_group_list.remove(group);

  UT_DELETE(group);

  arch_mutex_exit();
}

/** Archive dirty page IDs in current group.
This interface is for archiver background task to flush page archive
data to disk by calling it repeatedly over time.
@param[out]	wait	true, if no more data to archive
@return true, if archiving is aborted */
bool Arch_Page_Sys::archive(bool *wait) {
  bool set_idle;
  bool is_abort;

  Arch_Page_Pos cur_pos;
  Arch_Page_Pos end_pos;

  Arch_Block *cur_blk;
  dberr_t err;

  is_abort = (srv_shutdown_state == SRV_SHUTDOWN_LAST_PHASE ||
              srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS);

  arch_oper_mutex_enter();

  /* Check if archiving state is inactive. */
  if (m_state == ARCH_STATE_IDLE || m_state == ARCH_STATE_INIT) {
    *wait = true;

    if (is_abort) {
      m_state = ARCH_STATE_ABORT;
      arch_oper_mutex_exit();

      return (true);
    }

    arch_oper_mutex_exit();

    return (false);
  }

  ut_ad(m_state == ARCH_STATE_ACTIVE || m_state == ARCH_STATE_PREPARE_IDLE);

  set_idle = (m_state == ARCH_STATE_PREPARE_IDLE);

  cur_pos = m_flush_pos;
  end_pos = m_write_pos;

  arch_oper_mutex_exit();

  ut_ad(cur_pos.m_block_num <= end_pos.m_block_num);

  /* Caller needs to wait/sleep, if nothing to flush. */
  *wait = (cur_pos.m_block_num == end_pos.m_block_num);

  /* Write all blocks that are ready for flushing. */
  while (cur_pos.m_block_num < end_pos.m_block_num) {
    cur_blk = m_data.get_block(&cur_pos);

    err = cur_blk->flush(m_current_group, ARCH_FLUSH_NORMAL);

    if (err != DB_SUCCESS) {
      is_abort = true;
      break;
    }

    cur_pos.set_next();

    arch_oper_mutex_enter();

    cur_blk->set_flushed();
    m_flush_pos.set_next();

    arch_oper_mutex_exit();
  }

  /* Move to idle state or abort, if needed. */
  if (set_idle || is_abort) {
    uint ref_count;

    arch_mutex_enter();

    ref_count = m_current_group->disable(LSN_MAX, m_flush_pos.m_block_num);

    /* Cleanup group, if no reference. */
    if (ref_count == 0) {
      m_group_list.remove(m_current_group);

      UT_DELETE(m_current_group);
    }

    m_current_group = nullptr;

    m_state = is_abort ? ARCH_STATE_ABORT : ARCH_STATE_IDLE;

    arch_mutex_exit();
  }

  return (is_abort);
}
