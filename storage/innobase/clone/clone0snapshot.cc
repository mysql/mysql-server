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

/** @file clone/clone0snapshot.cc
 Innodb physical Snaphot

 *******************************************************/

#include "clone0snapshot.h"
#include "handler.h"
#include "page0zip.h"

/** Snapshot heap initial size */
const uint SNAPSHOT_MEM_INITIAL_SIZE = 16 * 1024;

/** Number of clones that can attach to a snapshot. */
const uint MAX_CLONES_PER_SNAPSHOT = 1;

/** Construct snapshot
@param[in]	hdl_type	copy, apply
@param[in]	clone_type	clone type
@param[in]	arr_idx		index in global array
@param[in]	snap_id		unique snapshot ID */
Clone_Snapshot::Clone_Snapshot(Clone_Handle_Type hdl_type,
                               Ha_clone_type clone_type, uint arr_idx,
                               ib_uint64_t snap_id)
    : m_snapshot_handle_type(hdl_type),
      m_snapshot_type(clone_type),
      m_snapshot_id(snap_id),
      m_snapshot_arr_idx(arr_idx),
      m_allow_new_clone(true),
      m_num_clones(),
      m_num_clones_current(),
      m_num_clones_next(),
      m_snapshot_state(CLONE_SNAPSHOT_INIT),
      m_snapshot_next_state(CLONE_SNAPSHOT_NONE),
      m_num_current_chunks(),
      m_max_file_name_len(),
      m_num_data_files(),
      m_num_data_chunks(),
      m_page_ctx(),
      m_num_pages(),
      m_num_duplicate_pages(),
      m_redo_ctx(),
      m_redo_start_offset(),
      m_redo_header(),
      m_redo_header_size(),
      m_redo_trailer(),
      m_redo_trailer_size(),
      m_redo_trailer_offset(),
      m_redo_file_size(),
      m_num_redo_files(),
      m_num_redo_chunks() {
  mutex_create(LATCH_ID_CLONE_SNAPSHOT, &m_snapshot_mutex);

  m_snapshot_heap = mem_heap_create(SNAPSHOT_MEM_INITIAL_SIZE);

  m_chunk_size_pow2 = SNAPSHOT_DEF_CHUNK_SIZE_POW2;
  m_block_size_pow2 = SNAPSHOT_DEF_BLOCK_SIZE_POW2;
}

/** Release contexts and free heap */
Clone_Snapshot::~Clone_Snapshot() {
  m_redo_ctx.release();
  m_page_ctx.release();

  mem_heap_free(m_snapshot_heap);

  mutex_free(&m_snapshot_mutex);
}

/** Fill state descriptor from snapshot
@param[out]	state_desc	snapshot state descriptor */
void Clone_Snapshot::get_state_info(Clone_Desc_State *state_desc) {
  state_desc->m_state = m_snapshot_state;
  state_desc->m_num_chunks = m_num_current_chunks;

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    state_desc->m_num_files = m_num_data_files;

  } else if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    state_desc->m_num_files = m_num_pages;

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    state_desc->m_num_files = m_num_redo_files;
    /* Minimum of two redo files need to be created. */
    if (state_desc->m_num_files < 2) {
      state_desc->m_num_files = 2;
    }

  } else if (m_snapshot_state == CLONE_SNAPSHOT_DONE) {
    state_desc->m_num_files = 0;

  } else {
    ut_ad(false);
  }
}

/** Set state information during apply
@param[in]	state_desc	snapshot state descriptor */
void Clone_Snapshot::set_state_info(Clone_Desc_State *state_desc) {
  mutex_enter(&m_snapshot_mutex);

  ut_ad(state_desc->m_state == m_snapshot_state);

  m_num_current_chunks = state_desc->m_num_chunks;

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    m_num_data_files = state_desc->m_num_files;
    m_num_data_chunks = state_desc->m_num_chunks;

    m_data_file_vector.resize(m_num_data_files, nullptr);

  } else if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    m_num_pages = state_desc->m_num_files;

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    m_num_redo_files = state_desc->m_num_files;
    m_num_redo_chunks = state_desc->m_num_chunks;

    m_redo_file_vector.resize(m_num_redo_files, nullptr);

  } else if (m_snapshot_state == CLONE_SNAPSHOT_DONE) {
    ut_ad(m_num_current_chunks == 0);

  } else {
    ut_ad(false);
  }

  mutex_exit(&m_snapshot_mutex);
}

/** Get next state based on snapshot type
@return next state */
Snapshot_State Clone_Snapshot::get_next_state() {
  Snapshot_State next_state;

  ut_ad(m_snapshot_state != CLONE_SNAPSHOT_NONE);

  if (m_snapshot_state == CLONE_SNAPSHOT_INIT) {
    next_state = CLONE_SNAPSHOT_FILE_COPY;

  } else if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    if (m_snapshot_type == HA_CLONE_HYBRID ||
        m_snapshot_type == HA_CLONE_PAGE) {
      next_state = CLONE_SNAPSHOT_PAGE_COPY;

    } else if (m_snapshot_type == HA_CLONE_REDO) {
      next_state = CLONE_SNAPSHOT_REDO_COPY;

    } else {
      ut_ad(m_snapshot_type == HA_CLONE_BLOCKING);
      next_state = CLONE_SNAPSHOT_DONE;
    }

  } else if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    next_state = CLONE_SNAPSHOT_REDO_COPY;

  } else {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);
    next_state = CLONE_SNAPSHOT_DONE;
  }

  return (next_state);
}

/** Try to attach to snapshot
@param[in]	hdl_type	copy, apply
@return true if successfully attached */
bool Clone_Snapshot::attach(Clone_Handle_Type hdl_type) {
  if (m_allow_new_clone && hdl_type == m_snapshot_handle_type &&
      m_num_clones < MAX_CLONES_PER_SNAPSHOT) {
    mutex_enter(&m_snapshot_mutex);

    ++m_num_clones;
    if (in_transit_state()) {
      ++m_num_clones_current;
    }

    mutex_exit(&m_snapshot_mutex);

    return (true);
  }

  return (false);
}

/** Detach from snapshot
@return number of clones attached */
uint Clone_Snapshot::detach() {
  uint num_clones_left;

  mutex_enter(&m_snapshot_mutex);

  ut_ad(!in_transit_state());
  ut_ad(m_num_clones > 0);

  num_clones_left = --m_num_clones;

  mutex_exit(&m_snapshot_mutex);

  return (num_clones_left);
}

/** Start transition to new state
@param[in]	new_state	state to move for apply
@param[in]	temp_buffer	buffer used for collecting page IDs
@param[in]	temp_buffer_len	buffer length
@param[out]	pending_clones	clones yet to transit to next state
@return error code */
dberr_t Clone_Snapshot::change_state(Snapshot_State new_state,
                                     byte *temp_buffer, uint temp_buffer_len,
                                     uint &pending_clones) {
  ut_ad(m_snapshot_state != CLONE_SNAPSHOT_NONE);

  mutex_enter(&m_snapshot_mutex);

  if (m_snapshot_state != CLONE_SNAPSHOT_INIT) {
    m_allow_new_clone = false;
  }

  /* Initialize transition if not started yet by other clones. */
  if (!in_transit_state()) {
    m_num_clones_current = m_num_clones;

    m_snapshot_next_state = new_state;
    m_num_clones_next = 0;
  }

  /* Move clone over to next state */
  --m_num_clones_current;
  ++m_num_clones_next;

  pending_clones = m_num_clones_current;

  /* Need to wait for other clones to move over. */
  if (pending_clones > 0) {
    mutex_exit(&m_snapshot_mutex);
    return (DB_SUCCESS);
  }

  /* Last clone requesting the state change. All other clones have
  already moved over to next state and waiting for the transition
  to complete. Now it is safe to do the snapshot state transition. */
  m_snapshot_state = m_snapshot_next_state;

  m_snapshot_next_state = CLONE_SNAPSHOT_NONE;

  m_num_clones_current = 0;
  m_num_clones_next = 0;

  dberr_t err;

  /* Initialize the new state. */
  err = init_state(temp_buffer, temp_buffer_len);

  mutex_exit(&m_snapshot_mutex);

  return (err);
}

/** Check if transition is complete
@return number of clones yet to transit to next state */
uint Clone_Snapshot::check_state(Snapshot_State new_state) {
  uint pending_clones;

  mutex_enter(&m_snapshot_mutex);

  pending_clones = 0;
  if (in_transit_state() && new_state == m_snapshot_next_state) {
    pending_clones = m_num_clones_current;
  }

  mutex_exit(&m_snapshot_mutex);

  return (pending_clones);
}

/** Get file metadata by index for current state
@param[in]	index	file index
@return file metadata entry */
Clone_File_Meta *Clone_Snapshot::get_file_by_index(uint index) {
  Clone_File_Meta *file_meta;

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY ||
      m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    ut_ad(index < m_num_data_files);
    file_meta = m_data_file_vector[index];

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    ut_ad(index < m_num_redo_files);
    file_meta = m_redo_file_vector[index];

  } else {
    ut_ad(false);
    file_meta = nullptr;
  }

  return (file_meta);
}

/** Get next block of data to transfer
@param[in]	chunk_num	current chunk
@param[in,out]	block_num	current/next block
@param[in,out]	file_meta	current/next block file metadata
@param[out]	data_offset	block offset in file
@param[out]	data_buf	data buffer or NULL if transfer from file
@param[out]	data_size	size of data in bytes
@return error code */
dberr_t Clone_Snapshot::get_next_block(uint chunk_num, uint &block_num,
                                       Clone_File_Meta *file_meta,
                                       ib_uint64_t &data_offset,
                                       byte *&data_buf, uint &data_size) {
  dberr_t err = DB_SUCCESS;
  ib_uint64_t chunk_offset = 0;
  uint start_index;
  Clone_File_Meta *current_file;

  /* File index for last chunk. This index value is always increasing
  for a task. We skip all previous index while searching for new file. */
  start_index = file_meta->m_file_index;

  if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    /* Copy the page from buffer pool. */
    err = get_next_page(chunk_num, block_num, file_meta, data_offset, data_buf,
                        data_size);
    return (err);

  } else if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    /* Get file for the chunk. */
    current_file =
        get_file(m_data_file_vector, m_num_data_files, chunk_num, start_index);
  } else {
    /* For redo copy header and trailer are returned in buffer. */
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);

    if (chunk_num == (m_num_current_chunks - 1)) {
      /* Last but one chunk is the redo header. */

      if (block_num != 0) {
        block_num = 0;
        return (DB_SUCCESS);
      }

      ++block_num;

      current_file = m_redo_file_vector.front();
      *file_meta = *current_file;

      data_offset = 0;

      data_buf = m_redo_header;
      ut_ad(data_buf != nullptr);

      data_size = m_redo_header_size;

      return (err);

    } else if (chunk_num == m_num_current_chunks) {
      /* Last chunk is the redo trailer. */

      if (block_num != 0 || m_redo_trailer_size == 0) {
        block_num = 0;
        return (DB_SUCCESS);
      }

      ++block_num;

      current_file = m_redo_file_vector.back();
      *file_meta = *current_file;

      data_offset = m_redo_trailer_offset;

      data_buf = m_redo_trailer;
      ut_ad(data_buf != nullptr);

      data_size = m_redo_trailer_size;

      return (err);
    }

    /* This is not header or trailer chunk. Need to get redo
    data from archived file. */
    current_file =
        get_file(m_redo_file_vector, m_num_redo_files, chunk_num, start_index);

    if (current_file->m_begin_chunk == 1) {
      /* Set start offset for the first file. */
      chunk_offset = m_redo_start_offset / UNIV_PAGE_SIZE;
    }

    /* Dummy redo file entry. Need to send metadata. */
    if (current_file->m_file_size == 0) {
      if (block_num != 0) {
        block_num = 0;
        return (DB_SUCCESS);
      }
      ++block_num;

      *file_meta = *current_file;
      data_buf = nullptr;
      data_size = 0;
      data_offset = 0;

      return (DB_SUCCESS);
    }
  }

  /* We have identified the file to transfer data at this point.
  Get the data offset for next block to transfer. */
  uint num_blocks;
  ib_uint64_t file_chnuk_num;

  data_buf = nullptr;

  file_chnuk_num = chunk_num - current_file->m_begin_chunk;

  /* Offset in pages for current chunk. */
  chunk_offset += file_chnuk_num << m_chunk_size_pow2;

  /* Find number of blocks in current chunk. */
  if (chunk_num == current_file->m_end_chunk) {
    /* If it is last chunk, we need to adjust the size. */
    ib_uint64_t size_in_pages;
    uint aligned_sz;

    size_in_pages =
        ut_uint64_align_up(current_file->m_file_size, UNIV_PAGE_SIZE);
    size_in_pages /= UNIV_PAGE_SIZE;

    ut_ad(size_in_pages >= chunk_offset);
    size_in_pages -= chunk_offset;

    aligned_sz = static_cast<uint>(size_in_pages);
    ut_ad(aligned_sz == size_in_pages);

    aligned_sz = ut_calc_align(aligned_sz, block_size());

    num_blocks = aligned_sz >> m_block_size_pow2;
  } else {
    num_blocks = blocks_per_chunk();
  }

  /* Current block is the last one. No more blocks in current chunk. */
  if (block_num == num_blocks) {
    block_num = 0;
    return (DB_SUCCESS);
  }

  ut_ad(block_num < num_blocks);

  /* Calculate the offset of next block. */
  ib_uint64_t block_offset;

  block_offset = static_cast<ib_uint64_t>(block_num);
  block_offset *= block_size();

  data_offset = chunk_offset + block_offset;
  data_size = block_size();

  ++block_num;

  *file_meta = *current_file;

  /* Convert offset and length in bytes. */
  data_size *= UNIV_PAGE_SIZE;
  data_offset *= UNIV_PAGE_SIZE;

  /* Adjust length for last block in last chunk. */
  if (chunk_num == current_file->m_end_chunk && block_num == num_blocks) {
    ut_ad((data_offset + data_size) >= current_file->m_file_size);
    ut_ad(data_offset < current_file->m_file_size);

    data_size = static_cast<uint>(current_file->m_file_size - data_offset);
  }

#ifdef UNIV_DEBUG
  if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    /* Current file is the last redo file */
    if (current_file == m_redo_file_vector.back() && m_redo_trailer_size != 0) {
      /* Should not exceed/overwrite the trailer */
      ut_ad(data_offset + data_size <= m_redo_trailer_offset);
    }
  }
#endif /* UNIV_DEBUG */

  return (DB_SUCCESS);
}

/** Update snapshot block size based on caller's buffer size
@param[in]	buff_size	buffer size for clone transfer */
void Clone_Snapshot::update_block_size(uint buff_size) {
  mutex_enter(&m_snapshot_mutex);

  /* Transfer data block is used only for direct IO. */
  if (m_snapshot_state != CLONE_SNAPSHOT_INIT || !srv_is_direct_io()) {
    mutex_exit(&m_snapshot_mutex);
    return;
  }

  /* Try to set block size bigger than the transfer buffer. */
  while (buff_size > (block_size() * UNIV_PAGE_SIZE) &&
         m_block_size_pow2 < SNAPSHOT_MAX_BLOCK_SIZE_POW2) {
    ++m_block_size_pow2;
  }

  mutex_exit(&m_snapshot_mutex);
}

/** Initialize current state
@param[in]	temp_buffer	buffer used during page copy initialize
@param[in]	temp_buffer_len	buffer length
@return error code */
dberr_t Clone_Snapshot::init_state(byte *temp_buffer, uint temp_buffer_len) {
  dberr_t err = DB_SUCCESS;

  m_num_current_chunks = 0;

  if (!is_copy()) {
    err = extend_files();

    return (err);
  }

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_NONE:
    case CLONE_SNAPSHOT_INIT:
      ut_ad(false);
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Innodb Clone Snapshot Invalid state");
      err = DB_ERROR;
      break;

    case CLONE_SNAPSHOT_FILE_COPY:

#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.init_state(srv_stage_clone_file_copy.m_key);
#endif
      err = init_file_copy();
#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.change_phase();
#endif
      DEBUG_SYNC_C("page_archiving");
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:

#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.init_state(srv_stage_clone_page_copy.m_key);
#endif
      err = init_page_copy(temp_buffer, temp_buffer_len);
#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.change_phase();
#endif
      DEBUG_SYNC_C("redo_archiving");
      break;

    case CLONE_SNAPSHOT_REDO_COPY:

#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.init_state(srv_stage_clone_redo_copy.m_key);
#endif
      err = init_redo_copy();
#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.change_phase();
#endif
      break;

    case CLONE_SNAPSHOT_DONE:

#ifdef HAVE_PSI_STAGE_INTERFACE
      m_monitor.init_state(Clone_Monitor::s_invalid_key);
#endif
      m_redo_ctx.release();
      ib::info(ER_IB_MSG_155) << "Clone State DONE ";
      break;
  }

  return (err);
}

/** Get file metadata for current chunk
@param[in]	file_vector	clone file vector
@param[in]	num_files	total number of files
@param[in]	chunk_num	current chunk number
@param[in]	start_index	index for starting the search
@return file metadata */
Clone_File_Meta *Clone_Snapshot::get_file(Clone_File_Vec &file_vector,
                                          uint num_files, uint chunk_num,
                                          uint start_index) {
  Clone_File_Meta *current_file = nullptr;
  uint idx;

  ut_ad(file_vector.size() >= num_files);

  /* Scan through the file vector matching chunk number. */
  for (idx = start_index; idx < num_files; idx++) {
    current_file = file_vector[idx];

    ut_ad(chunk_num >= current_file->m_begin_chunk);

    if (chunk_num <= current_file->m_end_chunk) {
      break;
    }
  }

  ut_ad(idx < num_files);

  return (current_file);
}

/** Get next page from buffer pool
@param[in]	chunk_num	current chunk
@param[in,out]	block_num	current, next block
@param[in]	file_meta	file metadata for page
@param[out]	data_offset	offset in file
@param[out]	data_buf	page data
@param[out]	data_size	page data size
@return error code */
dberr_t Clone_Snapshot::get_next_page(uint chunk_num, uint &block_num,
                                      Clone_File_Meta *file_meta,
                                      ib_uint64_t &data_offset, byte *&data_buf,
                                      uint &data_size) {
  Clone_Page clone_page;
  Clone_File_Meta *page_file;

  uint page_index;
  uint file_index;

  ut_ad(data_size >= UNIV_PAGE_SIZE);

  if (block_num == chunk_size()) {
    block_num = 0;
    return (DB_SUCCESS);
  }

  /* For "page copy", each block is a page. */
  page_index = chunk_size() * (chunk_num - 1);
  page_index += block_num;

  /* For last chunk, actual number of blocks could be less
  than chunk_size. */
  if (page_index >= m_page_vector.size()) {
    ut_ad(page_index == m_page_vector.size());
    block_num = 0;
    return (DB_SUCCESS);
  }

  clone_page = m_page_vector[page_index];

  ++block_num;

  /* Get the data file for current page. */
  bool found;
  const page_size_t &page_size =
      fil_space_get_page_size(clone_page.m_space_id, &found);

  ut_ad(found);

  file_index = m_data_file_map[clone_page.m_space_id];

  ut_ad(file_index > 0);
  --file_index;

  page_file = m_data_file_vector[file_index];
  ut_ad(page_file->m_space_id == clone_page.m_space_id);

  /* Data offset could be beyond 32 BIT integer. */
  data_offset = static_cast<ib_uint64_t>(clone_page.m_page_no);
  data_offset *= page_size.physical();

  /* Check if the page belongs to other nodes of the tablespace. */
  while (m_num_data_files > file_index + 1) {
    Clone_File_Meta *page_file_next;

    page_file_next = m_data_file_vector[file_index + 1];

    /* Next node belongs to same tablespace and data offset
    exceeds current node size */
    if (page_file_next->m_space_id == clone_page.m_space_id &&
        data_offset >= page_file->m_file_size) {
      data_offset -= page_file->m_file_size;
      file_index++;
      page_file = m_data_file_vector[file_index];
    } else {
      break;
    }
  }

  *file_meta = *page_file;

  /* Get page from buffer pool. */
  page_id_t page_id(clone_page.m_space_id, clone_page.m_page_no);

  auto err = get_page_for_write(page_id, page_size, data_buf, data_size);

  /* Update size from space header page. */
  if (clone_page.m_page_no == 0) {
    auto space_size = fsp_header_get_field(data_buf, FSP_SIZE);

    auto size_bytes = static_cast<uint64_t>(space_size);

    size_bytes *= page_size.physical();

    if (file_meta->m_file_size < size_bytes) {
      file_meta->m_file_size = size_bytes;
    }
  }

  return (err);
}

/** Get page from buffer pool and make ready for write
@param[in]	page_id		page ID chunk
@param[in]	page_size	page size descriptor
@param[out]	page_data	data page
@param[out]	data_size	page size in bytes
@return error code */
dberr_t Clone_Snapshot::get_page_for_write(const page_id_t &page_id,
                                           const page_size_t &page_size,
                                           byte *&page_data, uint &data_size) {
  auto space = fil_space_get(page_id.space());
  IORequest request(IORequest::WRITE);

  mtr_t mtr;
  mtr_start(&mtr);

  ut_ad(data_size >= 2 * page_size.physical());

  data_size = page_size.physical();
  auto encrypted_data = page_data + data_size;

  /* Space header page is modified with SX latch while extending. Also,
  we would like to serialize with page flush to disk. */
  auto block =
      buf_page_get_gen(page_id, page_size, RW_S_LATCH, nullptr,
                       Page_fetch::POSSIBLY_FREED, __FILE__, __LINE__, &mtr);
  auto bpage = &block->page;

  byte *src_data;

  if (bpage->zip.data != nullptr) {
    ut_ad(bpage->size.is_compressed());
    src_data = bpage->zip.data;
  } else {
    ut_ad(!bpage->size.is_compressed());
    src_data = block->frame;
  }

  memcpy(page_data, src_data, data_size);

  buf_page_mutex_enter(block);
  ut_ad(!fsp_is_checksum_disabled(bpage->id.space()));
  /* Get oldest and newest page modification LSN for dirty page. */
  auto oldest_lsn = bpage->oldest_modification;
  auto newest_lsn = bpage->newest_modification;
  buf_page_mutex_exit(block);

  /* If page is dirty, we need to set checksum and page LSN. */
  if (oldest_lsn > 0) {
    ut_ad(newest_lsn > 0);
    /* For compressed table, must copy the compressed page. */
    if (page_size.is_compressed()) {
      page_zip_des_t page_zip;

      page_zip_set_size(&page_zip, data_size);
      page_zip.data = page_data;
#ifdef UNIV_DEBUG
      page_zip.m_start =
#endif /* UNIV_DEBUG */
          page_zip.m_end = page_zip.m_nonempty = page_zip.n_blobs = 0;

      buf_flush_init_for_writing(nullptr, block->frame, &page_zip, newest_lsn,
                                 false, false);
    } else {
      buf_flush_init_for_writing(nullptr, page_data, nullptr, newest_lsn, false,
                                 false);
    }
  }

  BlockReporter reporter(false, page_data, page_size, false);

  const auto page_lsn =
      static_cast<lsn_t>(mach_read_from_8(page_data + FIL_PAGE_LSN));

  const auto page_checksum = static_cast<uint32_t>(
      mach_read_from_4(page_data + FIL_PAGE_SPACE_OR_CHKSUM));

  auto cur_lsn = log_get_lsn(*log_sys);

  dberr_t err = DB_SUCCESS;

  if (reporter.is_corrupted() || page_lsn > cur_lsn ||
      (page_checksum != 0 && page_lsn == 0)) {
    ut_ad(false);
    my_error(ER_INTERNAL_ERROR, MYF(0), "Innodb Clone Corrupt Page");
    err = DB_ERROR;
  }

  fil_io_set_encryption(request, page_id, space);

  /* Encrypt page if TDE is enabled. */
  if (err == DB_SUCCESS && request.is_encrypted()) {
    Encryption encryption(request.encryption_algorithm());
    ulint data_len;
    byte *ret_data;

    data_len = data_size;

    ret_data = encryption.encrypt(request, page_data, data_size, encrypted_data,
                                  &data_len);
    if (ret_data != page_data) {
      page_data = encrypted_data;
      data_size = static_cast<uint>(data_len);
    }
  }

  /* NOTE: We don't do transparent compression (TDC) here as punch hole
  support may not be there on remote. Also, punching hole for every page
  in remote during clone could be expensive. */

  mtr_commit(&mtr);
  return (err);
}
