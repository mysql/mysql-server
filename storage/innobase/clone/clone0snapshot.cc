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
      m_page_ctx(false),
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
      m_num_redo_chunks(),
      m_enable_pfs(false) {
  mutex_create(LATCH_ID_CLONE_SNAPSHOT, &m_snapshot_mutex);

  m_snapshot_heap = mem_heap_create(SNAPSHOT_MEM_INITIAL_SIZE);

  m_chunk_size_pow2 = SNAPSHOT_DEF_CHUNK_SIZE_POW2;
  m_block_size_pow2 = SNAPSHOT_DEF_BLOCK_SIZE_POW2;
}

Clone_Snapshot::~Clone_Snapshot() {
  m_redo_ctx.release();

  if (m_page_ctx.is_active()) {
    m_page_ctx.stop(nullptr);
  }
  m_page_ctx.release();

  mem_heap_free(m_snapshot_heap);

  mutex_free(&m_snapshot_mutex);
}

void Clone_Snapshot::get_state_info(bool do_estimate,
                                    Clone_Desc_State *state_desc) {
  state_desc->m_state = m_snapshot_state;
  state_desc->m_num_chunks = m_num_current_chunks;

  state_desc->m_is_start = true;
  state_desc->m_is_ack = false;
  state_desc->m_estimate = 0;

  if (do_estimate) {
    state_desc->m_estimate = m_monitor.get_estimate();
  }

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_FILE_COPY:
      state_desc->m_num_files = m_num_data_files;
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      state_desc->m_num_files = m_num_pages;
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      state_desc->m_num_files = m_num_redo_files;

      /* Minimum of two redo files need to be created. */
      if (state_desc->m_num_files < 2) {
        state_desc->m_num_files = 2;
      }
      break;

    case CLONE_SNAPSHOT_DONE:
      /* fall thorugh */

    case CLONE_SNAPSHOT_INIT:
      state_desc->m_num_files = 0;
      break;

    default:
      ut_ad(false);
  }
}

void Clone_Snapshot::set_state_info(Clone_Desc_State *state_desc) {
  ut_ad(mutex_own(&m_snapshot_mutex));
  ut_ad(state_desc->m_state == m_snapshot_state);

  m_num_current_chunks = state_desc->m_num_chunks;

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    m_num_data_files = state_desc->m_num_files;
    m_num_data_chunks = state_desc->m_num_chunks;
    m_data_file_vector.resize(m_num_data_files, nullptr);

    m_monitor.init_state(srv_stage_clone_file_copy.m_key, m_enable_pfs);
    m_monitor.add_estimate(state_desc->m_estimate);
    m_monitor.change_phase();

  } else if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    m_num_pages = state_desc->m_num_files;

    m_monitor.init_state(srv_stage_clone_page_copy.m_key, m_enable_pfs);
    m_monitor.add_estimate(state_desc->m_estimate);
    m_monitor.change_phase();

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    m_num_redo_files = state_desc->m_num_files;
    m_num_redo_chunks = state_desc->m_num_chunks;
    m_redo_file_vector.resize(m_num_redo_files, nullptr);

    m_monitor.init_state(srv_stage_clone_redo_copy.m_key, m_enable_pfs);
    m_monitor.add_estimate(state_desc->m_estimate);
    m_monitor.change_phase();

  } else if (m_snapshot_state == CLONE_SNAPSHOT_DONE) {
    ut_ad(m_num_current_chunks == 0);
    m_monitor.init_state(PSI_NOT_INSTRUMENTED, m_enable_pfs);

  } else {
    ut_ad(false);
  }
}

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

bool Clone_Snapshot::attach(Clone_Handle_Type hdl_type, bool pfs_monitor) {
  bool ret = false;
  mutex_enter(&m_snapshot_mutex);

  if (m_num_clones == 0) {
    m_enable_pfs = pfs_monitor;
  }

  if (m_allow_new_clone && hdl_type == m_snapshot_handle_type &&
      m_num_clones < MAX_CLONES_PER_SNAPSHOT) {
    ++m_num_clones;

    if (in_transit_state()) {
      ++m_num_clones_current;
    }

    ret = true;
  }

  mutex_exit(&m_snapshot_mutex);
  return (ret);
}

uint Clone_Snapshot::detach() {
  uint num_clones_left;

  mutex_enter(&m_snapshot_mutex);

  ut_ad(m_num_clones > 0);

  if (in_transit_state()) {
    --m_num_clones_current;
  }

  num_clones_left = --m_num_clones;

  mutex_exit(&m_snapshot_mutex);

  return (num_clones_left);
}

int Clone_Snapshot::change_state(Clone_Desc_State *state_desc,
                                 Snapshot_State new_state, byte *temp_buffer,
                                 uint temp_buffer_len, uint &pending_clones) {
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
    return (0);
  }

  /* Last clone requesting the state change. All other clones have
  already moved over to next state and waiting for the transition
  to complete. Now it is safe to do the snapshot state transition. */
  m_snapshot_state = m_snapshot_next_state;

  m_snapshot_next_state = CLONE_SNAPSHOT_NONE;

  m_num_clones_current = 0;
  m_num_clones_next = 0;

  /* Initialize the new state. */
  auto err = init_state(state_desc, temp_buffer, temp_buffer_len);

  mutex_exit(&m_snapshot_mutex);

  return (err);
}

uint Clone_Snapshot::check_state(Snapshot_State new_state, bool exit_on_wait) {
  uint pending_clones;

  mutex_enter(&m_snapshot_mutex);

  pending_clones = 0;
  if (in_transit_state() && new_state == m_snapshot_next_state) {
    pending_clones = m_num_clones_current;
  }

  if (pending_clones != 0 && exit_on_wait) {
    ++m_num_clones_current;
    --m_num_clones_next;
  }

  mutex_exit(&m_snapshot_mutex);

  return (pending_clones);
}

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

int Clone_Snapshot::iterate_files(File_Cbk_Func &&func) {
  if (m_snapshot_state != CLONE_SNAPSHOT_FILE_COPY &&
      m_snapshot_state != CLONE_SNAPSHOT_REDO_COPY) {
    return (0);
  }

  auto &file_vector = (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY)
                          ? m_data_file_vector
                          : m_redo_file_vector;

  for (auto file_meta : file_vector) {
    auto err = func(file_meta);
    if (err != 0) {
      return (err);
    }
  }

  return (0);
}

int Clone_Snapshot::get_next_block(uint chunk_num, uint &block_num,
                                   Clone_File_Meta *file_meta,
                                   ib_uint64_t &data_offset, byte *&data_buf,
                                   uint &data_size) {
  uint64_t start_offset = 0;
  uint start_index;
  Clone_File_Meta *current_file;

  /* File index for last chunk. This index value is always increasing
  for a task. We skip all previous index while searching for new file. */
  start_index = file_meta->m_file_index;

  if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    /* Copy the page from buffer pool. */
    auto err = get_next_page(chunk_num, block_num, file_meta, data_offset,
                             data_buf, data_size);
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
        return (0);
      }

      ++block_num;

      current_file = m_redo_file_vector.front();
      *file_meta = *current_file;

      data_offset = 0;

      data_buf = m_redo_header;
      ut_ad(data_buf != nullptr);

      data_size = m_redo_header_size;

      return (0);

    } else if (chunk_num == m_num_current_chunks) {
      /* Last chunk is the redo trailer. */

      if (block_num != 0 || m_redo_trailer_size == 0) {
        block_num = 0;
        return (0);
      }

      ++block_num;

      current_file = m_redo_file_vector.back();
      *file_meta = *current_file;

      data_offset = m_redo_trailer_offset;

      data_buf = m_redo_trailer;
      ut_ad(data_buf != nullptr);

      data_size = m_redo_trailer_size;

      return (0);
    }

    /* This is not header or trailer chunk. Need to get redo
    data from archived file. */
    current_file =
        get_file(m_redo_file_vector, m_num_redo_files, chunk_num, start_index);

    if (current_file->m_begin_chunk == 1) {
      /* Set start offset for the first file. */
      start_offset = m_redo_start_offset;
    }

    /* Dummy redo file entry. Need to send metadata. */
    if (current_file->m_file_size == 0) {
      if (block_num != 0) {
        block_num = 0;
        return (0);
      }
      ++block_num;

      *file_meta = *current_file;
      data_buf = nullptr;
      data_size = 0;
      data_offset = 0;

      return (0);
    }
  }

  /* We have identified the file to transfer data at this point.
  Get the data offset for next block to transfer. */
  uint num_blocks;

  data_buf = nullptr;

  uint64_t file_chnuk_num = chunk_num - current_file->m_begin_chunk;

  /* Offset in pages for current chunk. */
  uint64_t chunk_offset = file_chnuk_num << m_chunk_size_pow2;

  /* Find number of blocks in current chunk. */
  if (chunk_num == current_file->m_end_chunk) {
    /* If it is last chunk, we need to adjust the size. */
    ib_uint64_t size_in_pages;
    uint aligned_sz;

    ut_ad(current_file->m_file_size >= start_offset);
    size_in_pages = ut_uint64_align_up(current_file->m_file_size - start_offset,
                                       UNIV_PAGE_SIZE);
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
    return (0);
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
  data_offset += start_offset;

  ut_ad(data_offset < current_file->m_file_size);

  /* Adjust length for last block in last chunk. */
  if (chunk_num == current_file->m_end_chunk && block_num == num_blocks) {
    ut_ad((data_offset + data_size) >= current_file->m_file_size);
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

  return (0);
}

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

int Clone_Snapshot::init_state(Clone_Desc_State *state_desc, byte *temp_buffer,
                               uint temp_buffer_len) {
  int err = 0;
  m_num_current_chunks = 0;

  if (!is_copy()) {
    err = init_apply_state(state_desc);
    return (err);
  }

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_NONE:
    case CLONE_SNAPSHOT_INIT:
      ut_ad(false);

      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Innodb Clone Snapshot Invalid state");
      break;

    case CLONE_SNAPSHOT_FILE_COPY:
      ib::info(ER_IB_MSG_155) << "Clone State BEGIN FILE COPY";

      m_monitor.init_state(srv_stage_clone_file_copy.m_key, m_enable_pfs);
      err = init_file_copy();
      m_monitor.change_phase();
      DEBUG_SYNC_C("clone_start_page_archiving");
      DBUG_EXECUTE_IF("clone_crash_during_page_archiving", DBUG_SUICIDE(););
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      ib::info(ER_IB_MSG_155) << "Clone State BEGIN PAGE COPY";

      m_monitor.init_state(srv_stage_clone_page_copy.m_key, m_enable_pfs);
      err = init_page_copy(temp_buffer, temp_buffer_len);
      m_monitor.change_phase();
      DEBUG_SYNC_C("clone_start_redo_archiving");
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      ib::info(ER_IB_MSG_155) << "Clone State BEGIN REDO COPY";

      m_monitor.init_state(srv_stage_clone_redo_copy.m_key, m_enable_pfs);
      err = init_redo_copy();
      m_monitor.change_phase();
      break;

    case CLONE_SNAPSHOT_DONE:
      ib::info(ER_IB_MSG_155) << "Clone State DONE ";

      m_monitor.init_state(PSI_NOT_INSTRUMENTED, m_enable_pfs);
      m_redo_ctx.release();
      break;
  }
  return (err);
}

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

int Clone_Snapshot::get_next_page(uint chunk_num, uint &block_num,
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
    return (0);
  }

  /* For "page copy", each block is a page. */
  page_index = chunk_size() * (chunk_num - 1);
  page_index += block_num;

  /* For last chunk, actual number of blocks could be less
  than chunk_size. */
  if (page_index >= m_page_vector.size()) {
    ut_ad(page_index == m_page_vector.size());
    block_num = 0;
    return (0);
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

int Clone_Snapshot::get_page_for_write(const page_id_t &page_id,
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
      buf_page_get_gen(page_id, page_size, RW_SX_LATCH, nullptr,
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

  auto cur_lsn = log_get_lsn(*log_sys);
  const auto frame_lsn =
      static_cast<lsn_t>(mach_read_from_8(page_data + FIL_PAGE_LSN));

  buf_page_mutex_enter(block);
  ut_ad(!fsp_is_checksum_disabled(bpage->id.space()));
  /* Get oldest and newest page modification LSN for dirty page. */
  auto oldest_lsn = bpage->oldest_modification;
  auto newest_lsn = bpage->newest_modification;
  buf_page_mutex_exit(block);

  /* If the page is not dirty but frame LSN is zero, it could be half
  initialized page left from incomplete operation. Assign valid LSN and checksum
  before copy. */
  if (frame_lsn == 0 && oldest_lsn == 0) {
    oldest_lsn = cur_lsn;
    newest_lsn = cur_lsn;
  }

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

  int err = 0;

  if (reporter.is_corrupted() || page_lsn > cur_lsn ||
      (page_checksum != 0 && page_lsn == 0)) {
    ut_ad(false);
    my_error(ER_INTERNAL_ERROR, MYF(0), "Innodb Clone Corrupt Page");
    err = ER_INTERNAL_ERROR;
  }

  fil_io_set_encryption(request, page_id, space);

  /* Encrypt page if TDE is enabled. */
  if (err == 0 && request.is_encrypted()) {
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
