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

/** @file clone/clone0snapshot.cc
 Innodb physical Snapshot

 *******************************************************/

#include "clone0snapshot.h"
#include "clone0clone.h"
#include "log0log.h" /* log_get_lsn */
#include "page0zip.h"
#include "sql/handler.h"

/** Snapshot heap initial size */
const uint SNAPSHOT_MEM_INITIAL_SIZE = 16 * 1024;

/** Number of clones that can attach to a snapshot. */
const uint MAX_CLONES_PER_SNAPSHOT = 1;

Clone_Snapshot::Clone_Snapshot(Clone_Handle_Type hdl_type,
                               Ha_clone_type clone_type, uint arr_idx,
                               uint64_t snap_id)
    : m_snapshot_handle_type(hdl_type),
      m_snapshot_type(clone_type),
      m_snapshot_id(snap_id),
      m_snapshot_arr_idx(arr_idx),
      m_num_blockers(),
      m_aborted(false),
      m_num_clones(),
      m_num_clones_transit(),
      m_snapshot_state(CLONE_SNAPSHOT_INIT),
      m_snapshot_next_state(CLONE_SNAPSHOT_NONE),
      m_num_current_chunks(),
      m_max_file_name_len(),
      m_num_data_chunks(),
      m_data_bytes_disk(),
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
      m_num_redo_chunks(),
      m_enable_pfs(false) {
  mutex_create(LATCH_ID_CLONE_SNAPSHOT, &m_snapshot_mutex);

  m_snapshot_heap =
      mem_heap_create(SNAPSHOT_MEM_INITIAL_SIZE, UT_LOCATION_HERE);

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

  if (do_estimate) {
    state_desc->m_estimate = m_monitor.get_estimate();
    state_desc->m_estimate_disk = m_data_bytes_disk;
  } else {
    state_desc->m_estimate = 0;
    state_desc->m_estimate_disk = 0;
  }

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_FILE_COPY:
      state_desc->m_num_files = num_data_files();
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      state_desc->m_num_files = m_num_pages;
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      state_desc->m_num_files = num_redo_files();
      break;

    case CLONE_SNAPSHOT_DONE:
    case CLONE_SNAPSHOT_INIT:
      state_desc->m_num_files = 0;
      break;

    default:
      state_desc->m_num_files = 0;
      ut_d(ut_error);
  }
}

void Clone_Snapshot::set_state_info(Clone_Desc_State *state_desc) {
  ut_ad(mutex_own(&m_snapshot_mutex));

  m_snapshot_state = state_desc->m_state;
  m_num_current_chunks = state_desc->m_num_chunks;

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    m_num_data_chunks = state_desc->m_num_chunks;
    m_data_bytes_disk = state_desc->m_estimate_disk;
    m_data_file_vector.resize(state_desc->m_num_files, nullptr);

    m_monitor.init_state(srv_stage_clone_file_copy.m_key, m_enable_pfs);
    m_monitor.add_estimate(state_desc->m_estimate);
    m_monitor.change_phase();

  } else if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    m_num_pages = state_desc->m_num_files;

    m_monitor.init_state(srv_stage_clone_page_copy.m_key, m_enable_pfs);
    m_monitor.add_estimate(state_desc->m_estimate);
    m_monitor.change_phase();

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    m_num_redo_chunks = state_desc->m_num_chunks;
    m_redo_file_vector.resize(state_desc->m_num_files, nullptr);

    m_monitor.init_state(srv_stage_clone_redo_copy.m_key, m_enable_pfs);
    m_monitor.add_estimate(state_desc->m_estimate);
    m_monitor.change_phase();

  } else if (m_snapshot_state == CLONE_SNAPSHOT_DONE) {
    ut_ad(m_num_current_chunks == 0);
    m_monitor.init_state(PSI_NOT_INSTRUMENTED, m_enable_pfs);

  } else {
    ut_d(ut_error);
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

  if (hdl_type == m_snapshot_handle_type &&
      m_num_clones < MAX_CLONES_PER_SNAPSHOT) {
    ++m_num_clones;
    m_enable_pfs = pfs_monitor;

    ut_ad(!in_transit_state());
    ret = true;
  }

  mutex_exit(&m_snapshot_mutex);
  return ret;
}

void Clone_Snapshot::detach() {
  mutex_enter(&m_snapshot_mutex);

  ut_ad(m_num_clones > 0);
  ut_ad(!in_transit_state());

  --m_num_clones;
  ut_ad(m_num_clones == 0);

  mutex_exit(&m_snapshot_mutex);
}

bool Clone_Snapshot::is_aborted() const {
  ut_ad(mutex_own(&m_snapshot_mutex));
  return m_aborted;
}

void Clone_Snapshot::set_abort() {
  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);
  m_aborted = true;
  ib::info(ER_IB_CLONE_OPERATION) << "Clone Snapshot aborted";
}

Clone_Snapshot::State_transit::State_transit(Clone_Snapshot *snapshot,
                                             Snapshot_State new_state)
    : m_snapshot(snapshot) {
  mutex_enter(&m_snapshot->m_snapshot_mutex);

  ut_ad(!m_snapshot->in_transit_wait());
  ut_ad(!m_snapshot->in_transit_state());

  m_snapshot->begin_transit_ddl_wait();
  ut_ad(m_snapshot->in_transit_wait());

  /* Wait for DDLs blocking clone state transition. */
  m_error = m_snapshot->wait(Wait_type::STATE_BLOCKER, nullptr, false, true);

  if (m_error != 0) {
    return; /* purecov: inspected */
  }

  m_snapshot->begin_transit(new_state);
  ut_ad(m_snapshot->in_transit_state());
}

Clone_Snapshot::State_transit::~State_transit() {
  if (m_error == 0) {
    m_snapshot->end_transit();
  }

  ut_ad(!m_snapshot->in_transit_state());
  ut_ad(!m_snapshot->in_transit_wait());

  mutex_exit(&m_snapshot->m_snapshot_mutex);
}

Clone_File_Meta *Clone_Snapshot::get_file_by_index(uint index) {
  auto file_ctx = get_file_ctx_by_index(index);

  if (file_ctx == nullptr) {
    return nullptr;
  }
  return file_ctx->get_file_meta();
}

Clone_file_ctx *Clone_Snapshot::get_file_ctx_by_index(uint index) {
  Clone_file_ctx *file_ctx = nullptr;

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY ||
      m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    auto num_data_files = m_data_file_vector.size();

    if (index < num_data_files) {
      file_ctx = m_data_file_vector[index];
    }

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    auto num_redo_files = m_redo_file_vector.size();

    if (index < num_redo_files) {
      file_ctx = m_redo_file_vector[index];
    }
  }

  return (file_ctx);
}

int Clone_Snapshot::iterate_files(File_Cbk_Func &&func) {
  int err = 0;

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_FILE_COPY:
      err = iterate_data_files(std::forward<File_Cbk_Func>(func));
      break;
    case CLONE_SNAPSHOT_REDO_COPY:
      err = iterate_redo_files(std::forward<File_Cbk_Func>(func));
      break;
    default:
      err = 0;
  }
  return err;
}

int Clone_Snapshot::iterate_data_files(File_Cbk_Func &&func) {
  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);

  for (auto file_ctx : m_data_file_vector) {
    auto err = func(file_ctx);
    if (err != 0) {
      return err; /* purecov: inspected */
    }
  }
  return 0;
}

int Clone_Snapshot::iterate_redo_files(File_Cbk_Func &&func) {
  for (auto file_ctx : m_redo_file_vector) {
    auto err = func(file_ctx);
    if (err != 0) {
      return err; /* purecov: inspected */
    }
  }
  return 0;
}

int Clone_Snapshot::get_next_block(uint chunk_num, uint &block_num,
                                   const Clone_file_ctx *&file_ctx,
                                   uint64_t &data_offset, byte *&data_buf,
                                   uint32_t &data_size, uint64_t &file_size) {
  uint64_t start_offset = 0;
  const auto file_meta = file_ctx->get_file_meta_read();
  file_size = 0;

  if (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    /* Copy the page from buffer pool. */
    auto err = get_next_page(chunk_num, block_num, file_ctx, data_offset,
                             data_buf, data_size, file_size);
    return (err);

  } else if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    /* For redo copy header and trailer are returned in buffer. */

    if (chunk_num == (m_num_current_chunks - 1)) {
      /* Last but one chunk is the redo header. */

      if (block_num != 0) {
        block_num = 0;
        return (0);
      }

      ++block_num;

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

      data_offset = m_redo_trailer_offset;

      data_buf = m_redo_trailer;
      ut_ad(data_buf != nullptr);

      data_size = m_redo_trailer_size;

      return (0);
    }

    /* This is not header or trailer chunk. Need to get redo
    data from archived file. */
    if (file_meta->m_begin_chunk == 1) {
      /* Set start offset for the first file. */
      start_offset = m_redo_start_offset;
    }

    /* Dummy redo file entry. Need to send metadata. */
    if (file_meta->m_file_size == 0) {
      if (block_num != 0) {
        block_num = 0;
        return (0);
      }
      ++block_num;

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

  uint64_t file_chnuk_num = chunk_num - file_meta->m_begin_chunk;

  /* Offset in pages for current chunk. */
  uint64_t chunk_offset = file_chnuk_num << m_chunk_size_pow2;

  /* Find number of blocks in current chunk. */
  if (chunk_num == file_meta->m_end_chunk) {
    /* If it is last chunk, we need to adjust the size. */
    uint64_t size_in_pages;
    uint aligned_sz;

    ut_ad(file_meta->m_file_size >= start_offset);
    size_in_pages = ut_uint64_align_up(file_meta->m_file_size - start_offset,
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
  uint64_t block_offset;

  block_offset = static_cast<uint64_t>(block_num);
  block_offset *= block_size();

  data_offset = chunk_offset + block_offset;
  data_size = block_size();

  ++block_num;

  /* Convert offset and length in bytes. */
  data_size *= UNIV_PAGE_SIZE;
  data_offset *= UNIV_PAGE_SIZE;
  data_offset += start_offset;

  ut_ad(data_offset < file_meta->m_file_size);

  /* Adjust length for last block in last chunk. */
  if (chunk_num == file_meta->m_end_chunk && block_num == num_blocks) {
    ut_ad((data_offset + data_size) >= file_meta->m_file_size);
    data_size = static_cast<uint>(file_meta->m_file_size - data_offset);
  }

#ifdef UNIV_DEBUG
  if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    /* Current file is the last redo file */
    auto redo_file_ctx = m_redo_file_vector.back();
    if (file_meta == redo_file_ctx->get_file_meta() &&
        m_redo_trailer_size != 0) {
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

uint32_t Clone_Snapshot::get_blocks_per_chunk() const {
  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);
  uint32_t num_blocks = 0;

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_PAGE_COPY:
      num_blocks = chunk_size();
      break;

    case CLONE_SNAPSHOT_FILE_COPY:
      [[fallthrough]];

    case CLONE_SNAPSHOT_REDO_COPY:
      num_blocks = blocks_per_chunk();
      break;

    default:
      /* purecov: begin deadcode */
      num_blocks = 0;
      break;
      /* purecov: end */
  }
  return num_blocks;
}

int Clone_Snapshot::change_state(Clone_Desc_State *state_desc,
                                 Snapshot_State new_state, byte *temp_buffer,
                                 uint temp_buffer_len, Clone_Alert_Func cbk) {
  ut_ad(m_snapshot_state != CLONE_SNAPSHOT_NONE);

  int err = 0;
  m_num_current_chunks = 0;

  if (!is_copy()) {
    err = init_apply_state(state_desc);
    return (err);
  }

  switch (new_state) {
    case CLONE_SNAPSHOT_NONE:
    case CLONE_SNAPSHOT_INIT:
      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Innodb Clone Snapshot Invalid state");
      ut_d(ut_error);
      ut_o(break);

    case CLONE_SNAPSHOT_FILE_COPY:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone State BEGIN FILE COPY";

      err = init_file_copy(new_state);

      DEBUG_SYNC_C("clone_start_page_archiving");
      DBUG_EXECUTE_IF("clone_crash_during_page_archiving", DBUG_SUICIDE(););
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone State BEGIN PAGE COPY";

      err = init_page_copy(new_state, temp_buffer, temp_buffer_len);

      DEBUG_SYNC_C("clone_start_redo_archiving");
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone State BEGIN REDO COPY";

      err = init_redo_copy(new_state, cbk);

      break;

    case CLONE_SNAPSHOT_DONE: {
      ib::info(ER_IB_CLONE_OPERATION) << "Clone State DONE ";

      State_transit transit_guard(this, new_state);
      m_monitor.init_state(PSI_NOT_INSTRUMENTED, m_enable_pfs);

      m_redo_ctx.release();

      err = transit_guard.get_error();
      break;
    }
  }
  return err;
}

Clone_file_ctx *Clone_Snapshot::get_file(Clone_File_Vec &file_vector,
                                         uint32_t chunk_num,
                                         uint32_t start_index) {
  Clone_file_ctx *current_file = nullptr;
  uint idx;

  auto num_files = file_vector.size();

  /* Scan through the file vector matching chunk number. */
  for (idx = start_index; idx < num_files; idx++) {
    current_file = file_vector[idx];
    auto file_meta = current_file->get_file_meta();

    ut_ad(chunk_num >= file_meta->m_begin_chunk);

    if (chunk_num <= file_meta->m_end_chunk) {
      break;
    }
  }

  return (current_file);
}

void Clone_Snapshot::skip_deleted_blocks(uint32_t chunk_num,
                                         uint32_t &block_num) {
  /* For file copy entire chunk can be ignored because chunk
  doesn't span across files. */
  if (m_snapshot_state != CLONE_SNAPSHOT_PAGE_COPY) {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY);
    block_num = 0;
    return;
  }

  const auto *cur_file_ctx = get_page_file_ctx(chunk_num, block_num);
  const auto *next_file_ctx = cur_file_ctx;

  ut_ad(cur_file_ctx->deleted());

  /* Skip over the deleted file pages of current file context. */
  while (next_file_ctx == cur_file_ctx) {
    ++block_num;
    next_file_ctx = get_page_file_ctx(chunk_num, block_num);

    /* End of current chunk. */
    if (next_file_ctx == nullptr || block_num >= chunk_size()) {
      block_num = 0;
      break;
    }
  }
}

int Clone_Snapshot::get_next_page(uint chunk_num, uint &block_num,
                                  const Clone_file_ctx *&file_ctx,
                                  uint64_t &data_offset, byte *&data_buf,
                                  uint32_t &data_size, uint64_t &file_size) {
  ut_ad(data_size >= UNIV_PAGE_SIZE);
  file_size = 0;

  ut_ad(file_ctx->is_pinned());
  ut_ad(block_num < chunk_size());

  /* For "page copy", each block is a page. */
  uint32_t page_index = chunk_size() * (chunk_num - 1);
  page_index += block_num;

  ut_a(page_index < m_page_vector.size());
  auto clone_page = m_page_vector[page_index];

  ++block_num;

  /* Get the data file for current page. */
  bool found;
  const page_size_t &page_size =
      fil_space_get_page_size(clone_page.m_space_id, &found);

  auto file_meta = file_ctx->get_file_meta_read();

  ut_ad(found);
  ut_ad(file_meta->m_space_id == clone_page.m_space_id);

  /* Data offset could be beyond 32 BIT integer. */
  data_offset = static_cast<uint64_t>(clone_page.m_page_no);
  data_offset *= page_size.physical();

  auto file_index = file_meta->m_file_index;

  /* Check if the page belongs to other nodes of the tablespace. */
  while (num_data_files() > file_index + 1) {
    const auto file_next = m_data_file_vector[file_index + 1];
    const auto file_meta_next = file_next->get_file_meta();

    /* Next node belongs to same tablespace and data offset
    exceeds current node size */
    if (file_meta_next->m_space_id == file_meta->m_space_id &&
        data_offset >= file_meta->m_file_size) {
      data_offset -= file_meta->m_file_size;
      file_meta = file_meta_next;
      file_index = file_meta->m_file_index;
      file_ctx = file_next;
    } else {
      break;
    }
  }

  /* Get page from buffer pool. */
  page_id_t page_id(clone_page.m_space_id, clone_page.m_page_no);

  auto err =
      get_page_for_write(page_id, page_size, file_ctx, data_buf, data_size);

  /* Update size from space header page. */
  if (clone_page.m_page_no == 0) {
    auto space_size = fsp_header_get_field(data_buf, FSP_SIZE);

    auto size_bytes = static_cast<uint64_t>(space_size);

    size_bytes *= page_size.physical();

    if (file_meta->m_file_size < size_bytes) {
      file_size = size_bytes;
    }
  }
  return (err);
}

bool Clone_Snapshot::encrypt_key_in_log_header(byte *log_header,
                                               uint32_t header_len) {
  size_t offset = LOG_ENCRYPTION + LOG_HEADER_ENCRYPTION_INFO_OFFSET;
  ut_a(offset + Encryption::INFO_SIZE <= header_len);

  auto encryption_info = log_header + offset;

  /* Get log Encryption Key and IV. */
  Encryption_metadata encryption_metadata;
  auto success = Encryption::decode_encryption_info(encryption_metadata,
                                                    encryption_info, false);

  if (success) {
    /* Encrypt with master key and fill encryption information. */
    success = Encryption::fill_encryption_info(encryption_metadata, true,
                                               encryption_info);
  }
  return (success);
}

bool Clone_Snapshot::encrypt_key_in_header(const page_size_t &page_size,
                                           byte *page_data) {
  auto offset = fsp_header_get_encryption_offset(page_size);
  ut_ad(offset != 0 && offset + Encryption::INFO_SIZE <= UNIV_PAGE_SIZE);

  auto encryption_info = page_data + offset;

  /* Get tablespace Encryption Key and IV. */
  Encryption_metadata encryption_metadata;
  auto success = Encryption::decode_encryption_info(encryption_metadata,
                                                    encryption_info, false);
  if (!success) {
    return (false);
  }

  /* Encrypt with master key and fill encryption information. */
  success = Encryption::fill_encryption_info(encryption_metadata, true,
                                             encryption_info);
  if (!success) {
    return (false);
  }

  const auto frame_lsn =
      static_cast<lsn_t>(mach_read_from_8(page_data + FIL_PAGE_LSN));

  /* Update page checksum */
  page_update_for_flush(page_size, frame_lsn, page_data);

  return (true);
}

void Clone_Snapshot::decrypt_key_in_header(const Clone_File_Meta *file_meta,
                                           const page_size_t &page_size,
                                           byte *&page_data) {
  byte encryption_info[Encryption::INFO_SIZE];

  /* Get tablespace encryption information. */
  Encryption::fill_encryption_info(file_meta->m_encryption_metadata, false,
                                   encryption_info);

  /* Set encryption information in page. */
  auto offset = fsp_header_get_encryption_offset(page_size);
  ut_ad(offset != 0 && offset < UNIV_PAGE_SIZE);
  memcpy(page_data + offset, encryption_info, sizeof(encryption_info));
}

void Clone_Snapshot::page_update_for_flush(const page_size_t &page_size,
                                           lsn_t page_lsn, byte *&page_data) {
  /* For compressed table, must copy the compressed page. */
  if (page_size.is_compressed()) {
    page_zip_des_t page_zip;

    auto data_size = page_size.physical();
    page_zip_set_size(&page_zip, data_size);
    page_zip.data = page_data;
    ut_d(page_zip.m_start = 0);
    page_zip.m_end = 0;
    page_zip.n_blobs = 0;
    page_zip.m_nonempty = false;

    buf_flush_init_for_writing(nullptr, page_data, &page_zip, page_lsn, false,
                               false);
  } else {
    buf_flush_init_for_writing(nullptr, page_data, nullptr, page_lsn, false,
                               false);
  }
}

/** Set Page encryption information for IORequest.
@param[in,out]  request         IO request
@param[in]      page_id         page id
@param[in]      file_ctx        clone file context */
static void set_page_encryption(IORequest &request, const page_id_t &page_id,
                                const Clone_file_ctx *file_ctx) {
  auto file_meta = file_ctx->get_file_meta_read();

  /* Page zero is never encrypted. Need to also check the FSP encryption
  flag in case decryption is in progress. */
  if (!file_meta->can_encrypt() ||
      !FSP_FLAGS_GET_ENCRYPTION(file_meta->m_fsp_flags) ||
      page_id.page_no() == 0) {
    request.clear_encrypted();
    return;
  }
  request.get_encryption_info().set(file_meta->m_encryption_metadata);
}

int Clone_Snapshot::get_page_for_write(const page_id_t &page_id,
                                       const page_size_t &page_size,
                                       const Clone_file_ctx *file_ctx,
                                       byte *&page_data, uint &data_size) {
  auto file_meta = file_ctx->get_file_meta_read();

  mtr_t mtr;
  mtr_start(&mtr);

  ut_ad(data_size >= 2 * page_size.physical());

  data_size = page_size.physical();

  /* Space header page is modified with SX latch while extending. Also,
  we would like to serialize with page flush to disk. */
  auto block =
      buf_page_get_gen(page_id, page_size, RW_SX_LATCH, nullptr,
                       Page_fetch::POSSIBLY_FREED, UT_LOCATION_HERE, &mtr);
  auto bpage = &block->page;

  buf_page_mutex_enter(block);
  ut_ad(!fsp_is_checksum_disabled(bpage->id.space()));
  /* Get oldest and newest page modification LSN for dirty page. */
  auto oldest_lsn = bpage->get_oldest_lsn();
  auto newest_lsn = bpage->get_newest_lsn();
  buf_page_mutex_exit(block);

  bool page_is_dirty = (oldest_lsn > 0);

  byte *src_data;

  if (bpage->zip.data != nullptr) {
    ut_ad(bpage->size.is_compressed());
    /* If the page is not dirty, then zip descriptor always has the latest
    flushed page copy with LSN and checksum set properly. If the page is
    dirty, the latest modified page is in uncompressed form for uncompressed
    page types. The LSN in such case is to be taken from block newest LSN and
    checksum needs to be recalculated. */
    if (page_is_dirty && page_is_uncompressed_type(block->frame)) {
      src_data = block->frame;
    } else {
      src_data = bpage->zip.data;
    }
  } else {
    ut_ad(!bpage->size.is_compressed());
    src_data = block->frame;
  }

  memcpy(page_data, src_data, data_size);

  auto cur_lsn = log_get_lsn(*log_sys);
  const auto frame_lsn =
      static_cast<lsn_t>(mach_read_from_8(page_data + FIL_PAGE_LSN));

  /* First page of a encrypted tablespace. */
  if (file_meta->can_encrypt() && page_id.page_no() == 0) {
    /* Update unencrypted tablespace key in page 0 to be send over
    SSL connection. */
    decrypt_key_in_header(file_meta, page_size, page_data);

    /* Force to recalculate the checksum if the page is not dirty. */
    if (!page_is_dirty) {
      page_is_dirty = true;
      newest_lsn = frame_lsn;
    }
  }

  /* If the page is not dirty but frame LSN is zero, it could be half
  initialized page left from incomplete operation. Assign valid LSN and checksum
  before copy. */
  if (frame_lsn == 0 && oldest_lsn == 0) {
    page_is_dirty = true;
    newest_lsn = cur_lsn;
  }

  /* If page is dirty, we need to set checksum and page LSN. */
  if (page_is_dirty) {
    ut_ad(newest_lsn > 0);
    page_update_for_flush(page_size, newest_lsn, page_data);
  }

  BlockReporter reporter(false, page_data, page_size, false);

  const auto page_lsn =
      static_cast<lsn_t>(mach_read_from_8(page_data + FIL_PAGE_LSN));

  const auto page_checksum = static_cast<uint32_t>(
      mach_read_from_4(page_data + FIL_PAGE_SPACE_OR_CHKSUM));

  int err = 0;

  if (reporter.is_corrupted() || page_lsn > cur_lsn ||
      (page_checksum != 0 && page_lsn == 0)) {
    my_error(ER_INTERNAL_ERROR, MYF(0), "Innodb Clone Corrupt Page");
    err = ER_INTERNAL_ERROR;
    ut_d(ut_error);
  }

  auto encrypted_data = page_data + data_size;
  /* Data length could be less for compressed page */
  auto data_len = data_size;

  /* Do transparent page compression if needed. */
  if (page_id.page_no() != 0 && file_meta->m_punch_hole &&
      file_meta->m_compress_type != Compression::NONE) {
    auto compressed_data = page_data + data_size;
    memset(compressed_data, 0, data_size);

    IORequest request(IORequest::WRITE);
    request.compression_algorithm(file_meta->m_compress_type);
    ulint compressed_len = 0;

    auto buf_ptr = os_file_compress_page(
        request.compression_algorithm(), file_meta->m_fsblk_size, page_data,
        data_size, compressed_data, &compressed_len);

    if (buf_ptr != page_data) {
      encrypted_data = page_data;
      page_data = compressed_data;
      data_len = static_cast<uint>(compressed_len);
    }
  }

  IORequest request(IORequest::WRITE);
  set_page_encryption(request, page_id, file_ctx);

  /* Encrypt page if TDE is enabled. */
  if (err == 0 && request.is_encrypted()) {
    Encryption encryption(request.encryption_algorithm());
    ulint encrypt_len = data_len;

    memset(encrypted_data, 0, data_size);
    auto ret_data = encryption.encrypt(request, page_data, data_len,
                                       encrypted_data, &encrypt_len);
    if (ret_data != page_data) {
      page_data = encrypted_data;
      data_len = static_cast<uint>(encrypt_len);
    }
  }

  mtr_commit(&mtr);
  return (err);
}

uint32_t Clone_Snapshot::get_max_blocks_pin() const {
  return (m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) ? S_MAX_PAGES_PIN
                                                        : S_MAX_BLOCKS_PIN;
}

Clone_file_ctx *Clone_Snapshot::get_file_ctx(uint32_t chunk_num,
                                             uint32_t block_num,
                                             uint32_t hint_index) {
  Clone_file_ctx *file = nullptr;

  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_FILE_COPY:
      file = get_data_file_ctx(chunk_num, hint_index);
      break;
    case CLONE_SNAPSHOT_PAGE_COPY:
      file = get_page_file_ctx(chunk_num, block_num);
      break;
    case CLONE_SNAPSHOT_REDO_COPY:
      file = get_redo_file_ctx(chunk_num, hint_index);
      break;
    default:
      ut_d(ut_error); /* purecov: deadcode */
  }
  return file;
}

Clone_file_ctx *Clone_Snapshot::get_data_file_ctx(uint32_t chunk_num,
                                                  uint32_t hint_index) {
  return get_file(m_data_file_vector, chunk_num, hint_index);
}

Clone_file_ctx *Clone_Snapshot::get_redo_file_ctx(uint32_t chunk_num,
                                                  uint32_t hint_index) {
  /* Last but one chunk is redo header */
  if (chunk_num == (m_num_current_chunks - 1)) {
    return m_redo_file_vector.front();
  }
  /* Last chunk is the redo trailer. */
  if (chunk_num == m_num_current_chunks) {
    return m_redo_file_vector.back();
  }
  return get_file(m_redo_file_vector, chunk_num, hint_index);
}

Clone_file_ctx *Clone_Snapshot::get_page_file_ctx(uint32_t chunk_num,
                                                  uint32_t block_num) {
  /* Check if block is beyond the current chunk. */
  if (block_num >= chunk_size()) {
    ut_ad(block_num == chunk_size());
    return nullptr;
  }

  auto page_index = chunk_size() * (chunk_num - 1);
  page_index += block_num;

  /* Check if all blocks are over. For last chunk, actual number of blocks
  could be less than chunk_size. */
  if (page_index >= m_page_vector.size()) {
    ut_ad(page_index == m_page_vector.size());
    return nullptr;
  }

  auto clone_page = m_page_vector[page_index];
  auto file_index = m_data_file_map[clone_page.m_space_id];
  if (file_index == 0) {
    /* purecov: begin deadcode */
    ut_d(ut_error);
    ut_o(return nullptr);
    /* purecov: end */
  }
  --file_index;

  auto page_file = get_file_ctx_by_index(file_index);

#ifdef UNIV_DEBUG
  auto file_meta = page_file->get_file_meta();
  ut_ad(file_meta->m_space_id == clone_page.m_space_id);
#endif  // UNIV_DEBUG

  return page_file;
}

void Clone_file_ctx::get_file_name(std::string &name) const {
  name.assign(m_meta.m_file_name);

  /* Add file name extension. */
  switch (m_extension) {
    case Extension::REPLACE:
      if (m_meta.m_space_id == dict_sys_t::s_log_space_id) {
        const auto [directory, file] = Fil_path::split(name);
        name = directory + CLONE_INNODB_REPLACED_FILE_EXTN + file;
      } else {
        name.append(CLONE_INNODB_REPLACED_FILE_EXTN);
      }
      break;

    case Extension::DDL:
      name.append(CLONE_INNODB_DDL_FILE_EXTN);
      break;

    case Extension::NONE:
    default:
      break;
  }
}

bool Clone_Snapshot::begin_ddl_state(Clone_notify::Type type, space_id_t space,
                                     bool no_wait, bool check_intr,
                                     int &error) {
  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);
  error = 0;
  bool blocked = false;

  for (;;) {
    ut_ad(mutex_own(&m_snapshot_mutex));
    auto state = get_state();

    switch (state) {
      case CLONE_SNAPSHOT_NONE:
        /* purecov: begin deadcode */
        /* Clone must have started at this point. */
        ut_d(ut_error);
        ut_o(break);
        /* purecov: end */

      case CLONE_SNAPSHOT_INIT:
        /* Fall through. */
      case CLONE_SNAPSHOT_FILE_COPY:
        /* Allow clone to enter next stage only after the DDL file operation
        is complete. */
        blocked = block_state_change(type, space, no_wait, check_intr, error);
        ut_ad(mutex_own(&m_snapshot_mutex));

        if (error != 0) {
          /* We should not have blocked in case of error but it is not fatal. */
          ut_ad(!blocked);
          break;
        }

        if (state != get_state()) {
          /* purecov: begin inspected */
          /* State is modified. Start again and recheck. This is safe
          as clone has to eventually exit from the above two states. */
          ut_ad(!blocked);
          continue;
          /* purecov: end */
        }

        ut_ad(blocked);

        if (state == CLONE_SNAPSHOT_FILE_COPY) {
          error = begin_ddl_file(type, space, no_wait, check_intr);
        }
        break;

      case CLONE_SNAPSHOT_PAGE_COPY:
        /* 1. Bulk operation currently need to wait if clone has entered page
              copy. This is because bulk changes don't generate any redo log.
           2. We don't let new encryption alter to begin during page copy state.
              We currently cannot handle encryption key in redo log which is
              encrypted by donor master key. */
        ut_ad(!blocked);
        if (type == Clone_notify::Type::SPACE_ALTER_INPLACE_BULK ||
            type == Clone_notify::Type::SPACE_ALTER_ENCRYPT_GENERAL ||
            type == Clone_notify::Type::SPACE_ALTER_ENCRYPT) {
          error =
              wait(Wait_type::STATE_END_PAGE_COPY, nullptr, false, check_intr);
          break;
        }
        /* Try to block state change. If state is already modified then nothing
        to do as the next states don't require blocking. */
        blocked = block_state_change(type, space, no_wait, check_intr, error);
        if (error != 0 || state != get_state()) {
          /* We should not have blocked in case of error but it is not fatal. */
          ut_ad(!blocked);
          break;
        }
        ut_ad(blocked);

        error = begin_ddl_file(type, space, no_wait, check_intr);
        break;
      case CLONE_SNAPSHOT_REDO_COPY:
        /* Snapshot end point is already taken. This changes are not part of
        snapshot. */
        break;
      case CLONE_SNAPSHOT_DONE:
        /* Clone has already finished. */
        break;
      default:
        /* purecov: begin deadcode */
        ut_d(ut_error);
        ut_o(break);
        /* purecov: end */
    }
    break;
  } /* purecov: inspected */

  /* Unblock clone, in case of error. */
  if (blocked && error != 0) {
    /* purecov: begin inspected */
    unblock_state_change();
    blocked = false;
    /* purecov: end */
  }
  return blocked;
}

void Clone_Snapshot::end_ddl_state(Clone_notify::Type type, space_id_t space) {
  /* Caller is responsible to call if we have blocked state change. */
  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);
  auto state = get_state();

  if (state == CLONE_SNAPSHOT_FILE_COPY || state == CLONE_SNAPSHOT_PAGE_COPY) {
    end_ddl_file(type, space);
  }
  unblock_state_change();
}

void Clone_Snapshot::get_wait_mesg(Wait_type wait_type, std::string &info,
                                   std::string &error) {
  switch (wait_type) {
    case Wait_type::STATE_TRANSIT_WAIT:
      break;
    case Wait_type::STATE_TRANSIT:
      info.assign("DDL waiting for clone state transition");
      error.assign("DDL wait for clone state transition timed out");
      break;
    case Wait_type::STATE_END_PAGE_COPY:
      info.assign("DDL waiting for Clone PAGE COPY to finish");
      error.assign("DDL wait for Clone PAGE COPY timed out");
      break;
    case Wait_type::STATE_BLOCKER:
      info.assign("Clone state transition waiting for DDL file operation");
      error.assign(
          "Clone state transition wait for DDL file operation timed out");
      break;
    case Wait_type::DATA_FILE_WAIT:
      info.assign("DDL waiting for clone threads to exit from previous wait");
      error.assign(
          "DDL wait for clone threads to exit from wait state timed out");
      break;
    case Wait_type::DATA_FILE_CLOSE:
      info.assign("DDL waiting for clone to close the open data file");
      error.assign("DDL wait for clone data file close timed out");
      break;
    case Wait_type::DDL_FILE_OPERATION:
      info.assign("Clone waiting for DDL file operation");
      error.assign("Clone wait for DDL file operation timed out");
      break;
    default:
      ut_d(ut_error); /* purecov: deadcode */
  }
}

const char *Clone_Snapshot::wait_string(Wait_type wait_type) const {
  const char *wait_info = nullptr;

  switch (wait_type) {
    /* DDL waiting for clone state transition */
    case Wait_type::STATE_TRANSIT_WAIT:
      [[fallthrough]];
    case Wait_type::STATE_TRANSIT:
      wait_info = "Waiting for clone state transition";
      break;

    /* DDL waiting till Clone PAGE COPY state is over. */
    case Wait_type::STATE_END_PAGE_COPY:
      wait_info = "Waiting for clone PAGE_COPY state";
      break;

    /*DDL waiting for clone file operation. */
    case Wait_type::DATA_FILE_WAIT:
      [[fallthrough]];
    case Wait_type::DATA_FILE_CLOSE:
      wait_info = "Waiting for clone to close files";
      break;

    /* Clone waiting for DDL. */
    case Wait_type::DDL_FILE_OPERATION:
      wait_info = "Waiting for ddl file operation";
      break;

    case Wait_type::STATE_BLOCKER:
      wait_info = "Waiting for ddl before state transition";
      [[fallthrough]];

    default:
      break;
  }

  return wait_info;
}

int Clone_Snapshot::wait(Wait_type wait_type, const Clone_file_ctx *ctx,
                         bool no_wait, bool check_intr) {
  ut_ad(mutex_own(&m_snapshot_mutex));

  std::string info_mesg;
  std::string error_mesg;

  get_wait_mesg(wait_type, info_mesg, error_mesg);

  auto wait_cond = [&](bool alert, bool &wait) {
    ut_ad(mutex_own(&m_snapshot_mutex));
    bool early_exit = false;

    switch (wait_type) {
      case Wait_type::STATE_TRANSIT_WAIT:
        wait = in_transit_wait();
        /* For state transition wait by DDL, exit on alert to avoid
        possible deadlock between DDLs. */
        early_exit = true;
        break;
      case Wait_type::STATE_TRANSIT:
        wait = in_transit_state();
        break;
      case Wait_type::STATE_END_PAGE_COPY:
        /* If clone has aborted, don't wait for state to end. */
        wait = !is_aborted() && (get_state() == CLONE_SNAPSHOT_PAGE_COPY);
        DBUG_EXECUTE_IF("clone_ddl_abort_wait_page_copy", {
          if (wait) {
            my_error(ER_INTERNAL_ERROR, MYF(0), "Simulated Clone DDL error");
            return ER_INTERNAL_ERROR;
          }
        });
        break;
      case Wait_type::STATE_BLOCKER:
        wait = (m_num_blockers > 0);
        break;
      case Wait_type::DATA_FILE_WAIT:
        wait = ctx->is_waiting();
        early_exit = true;
        break;
      case Wait_type::DATA_FILE_CLOSE:
        wait = ctx->is_pinned();
        break;
      case Wait_type::DDL_FILE_OPERATION:
        wait = blocks_clone(ctx);
        break;
      default:
        /* purecov: begin deadcode */
        wait = false;
        ut_d(ut_error);
        /* purecov: end */
    }

    if (wait) {
      if (no_wait || (alert && early_exit)) {
        return ER_QUERY_TIMEOUT; /* purecov: inspected */
      }

      if (alert) {
        ib::info(ER_IB_CLONE_TIMEOUT) << info_mesg; /* purecov: tested */
      }

      if (check_intr && thd_killed(nullptr)) {
        /* For early exit the caller would ignore error. */
        if (!early_exit) {
          my_error(ER_QUERY_INTERRUPTED, MYF(0));
        }
        return ER_QUERY_INTERRUPTED;
      }
    }
    return 0;
  };

  /* SET THD information string to display waiting state in PROCESS LIST. */
  Clone_Sys::Wait_stage wait_guard(wait_string(wait_type));

  bool is_timeout = false;
  int err = 0;

  /* Increase the defaults to wait more while waiting for page copy state. */
  if (wait_type == Wait_type::STATE_END_PAGE_COPY) {
    /* Generate alert message every 5 minutes. */
    Clone_Sec alert_interval(Clone_Min(5));
    /* Wait for 2 hours for clone to finish. */
    Clone_Sec time_out(Clone_Min(120));

    err = Clone_Sys::wait(CLONE_DEF_SLEEP, time_out, alert_interval, wait_cond,
                          &m_snapshot_mutex, is_timeout);
  } else {
    err = Clone_Sys::wait_default(wait_cond, &m_snapshot_mutex, is_timeout);
  }

  if (!err && is_timeout) {
    /* purecov: begin deadcode */
    err = ER_INTERNAL_ERROR;
    my_error(err, MYF(0), error_mesg.c_str());
    ut_d(ut_error);
    /* purecov: end */
  }
  return err;
}

bool Clone_Snapshot::block_state_change(Clone_notify::Type type,
                                        space_id_t space, bool no_wait,
                                        bool check_intr, int &error) {
  ut_ad(mutex_own(&m_snapshot_mutex));

  bool undo_ddl_ntfn = (type == Clone_notify::Type::SPACE_UNDO_DDL);
  bool undo_space = fsp_is_undo_tablespace(space);

  /* For undo DDL, there could be recursive notification for file create
  and drop which are !undo_ddl_ntfn. For such notifications we don't need
  to wait for clone as we must have already blocked it. */
  bool wait_clone = (!undo_space || undo_ddl_ntfn);

  /* If no wait option is used, override any waiting clone. Used for undo
  truncate background currently. We don't want to block purge threads. */
  if (no_wait) {
    wait_clone = false;
  }

  auto saved_state = get_state();

  /* Wait for the waiting clone. That is if clone is blocked by other DDL and
  waiting. This is an attempt to prevent starvation of clone by DDLs. We wait
  here for limited time to prevent possible deadlock between DDLs.
  e.g. DDL-2 <- DDL-1 (Critical section) <- Clone <- DDL-2. */
  if (wait_clone) {
    static_cast<void>(
        wait(Wait_type::STATE_TRANSIT_WAIT, nullptr, false, false));
    ut_ad(mutex_own(&m_snapshot_mutex));
    if (saved_state != get_state()) {
      /* State is modified. Return for possible recheck. */
      return false; /* purecov: inspected */
    }
  }

  /* Wait for state transition to get over. */
  error = wait(Wait_type::STATE_TRANSIT, nullptr, no_wait, check_intr);

  if (error != 0) {
    return false;
  }

  ut_ad(mutex_own(&m_snapshot_mutex));
  if (saved_state != get_state()) {
    /* State is modified. Return for possible recheck. */
    return false; /* purecov: inspected */
  }

  ut_ad(mutex_own(&m_snapshot_mutex));
  ++m_num_blockers;

  return true;
}

inline void Clone_Snapshot::unblock_state_change() {
  ut_ad(mutex_own(&m_snapshot_mutex));
  --m_num_blockers;
}

Clone_file_ctx::State Clone_Snapshot::get_target_file_state(
    Clone_notify::Type type, bool begin) {
  auto target_state = Clone_file_ctx::State::NONE;

  switch (type) {
    case Clone_notify::Type::SPACE_DROP:
      target_state = begin ? Clone_file_ctx::State::DROPPING
                           : Clone_file_ctx::State::DROPPED;
      break;
    case Clone_notify::Type::SPACE_RENAME:
      target_state = begin ? Clone_file_ctx::State::RENAMING
                           : Clone_file_ctx::State::RENAMED;
      break;
    default:
      target_state = Clone_file_ctx::State::NONE;
      break;
  }
  return target_state;
}

bool Clone_Snapshot::blocks_clone(const Clone_file_ctx *file_ctx) {
  bool block = false;
  auto clone_state = get_state();

  switch (clone_state) {
    case CLONE_SNAPSHOT_FILE_COPY:
      /* Block clone operation for both rename and delete operation
      as we directly access the file. */
      block = file_ctx->modifying();
      break;
    case CLONE_SNAPSHOT_PAGE_COPY:
      /* Block clone operation only if deleting. In page copy state we don't
      bother about space/file rename. If the page is not found in buffer pool,
      it would need to be read from underlying file but this IO needs to be
      synchronized with file operation irrespective of clone. */
      block = file_ctx->deleting();
      break;
    default:
      block = false;
      break;
  }
  return block;
}

int Clone_Snapshot::begin_ddl_file(Clone_notify::Type type, space_id_t space,
                                   bool no_wait, bool check_intr) {
  ut_ad(mutex_own(&m_snapshot_mutex));
  ut_ad(get_state() == CLONE_SNAPSHOT_FILE_COPY ||
        get_state() == CLONE_SNAPSHOT_PAGE_COPY);

  auto target_state = get_target_file_state(type, true);

  /* The type doesn't need any file operation. */
  if (target_state == Clone_file_ctx::State::NONE) {
    return 0;
  }
  auto count = m_data_file_map.count(space);

  /* The space is added concurrently and then modified again. */
  if (count == 0) {
    return 0;
  }
  /* If the space is already added for clone, we would have that in the map
  with a valid file index (starts from 1). */
  auto file_index = m_data_file_map[space];

  if (file_index == 0) {
    /* purecov: begin deadcode */
    ut_d(ut_error);
    ut_o(return 0);
    /* purecov: end */
  }
  --file_index;

  auto file_ctx = get_file_ctx_by_index(file_index);

  auto saved_state = file_ctx->m_state.load();

  ut_ad(saved_state != Clone_file_ctx::State::DROPPING);
  ut_ad(saved_state != Clone_file_ctx::State::RENAMING);
  ut_ad(saved_state != Clone_file_ctx::State::DROPPED);

  file_ctx->m_state.store(target_state);

  /* Wait for all data files to be closed by clone threads. */
  if (blocks_clone(file_ctx)) {
    auto err = wait(Wait_type::DATA_FILE_CLOSE, file_ctx, no_wait, check_intr);

    if (err != 0) {
      /* purecov: begin inspected */
      file_ctx->m_state.store(saved_state);
      return err;
      /* purecov: end */
    }
  }
  return 0;
}

void Clone_Snapshot::end_ddl_file(Clone_notify::Type type, space_id_t space) {
  ut_ad(mutex_own(&m_snapshot_mutex));
  ut_ad(get_state() == CLONE_SNAPSHOT_FILE_COPY ||
        get_state() == CLONE_SNAPSHOT_PAGE_COPY);

  auto target_state = get_target_file_state(type, false);

  /* The type doesn't need any file operation. */
  if (target_state == Clone_file_ctx::State::NONE) {
    return;
  }
  auto count = m_data_file_map.count(space);

  /* The space is added concurrently and then modified again. */
  if (count == 0) {
    return;
  }
  uint32_t file_index = m_data_file_map[space];

  if (file_index == 0) {
    /* purecov: begin deadcode */
    ut_d(ut_error);
    ut_o(return );
    /* purecov: end */
  }
  --file_index;

  auto file_ctx = get_file_ctx_by_index(file_index);
  auto file_meta = file_ctx->get_file_meta();

  file_ctx->set_ddl(get_next_state());

  if (type == Clone_notify::Type::SPACE_DROP) {
    file_meta->m_deleted = true;
    file_ctx->m_state.store(target_state);
    return;
  }

  bool blocking_clone = blocks_clone(file_ctx);

  /* We need file handling for drop and rename. */
  ut_ad(type == Clone_notify::Type::SPACE_RENAME);
  file_meta->m_renamed = true;
  file_ctx->m_state.store(target_state);

  if (blocking_clone) {
    auto fil_space = fil_space_get(space);

    ut_ad(fil_space->files.size() == 1);

    auto &file = fil_space->files.front();
    build_file_name(file_meta, file.name);

    /* Wait for any previously waiting clone threads to restart. This is to
    avoid starvation of clone by repeated renames. We ignore any error. Although
    not expected there is no functional impact of a timeout here. */
    static_cast<void>(wait(Wait_type::DATA_FILE_WAIT, file_ctx, false, false));
  }
}

bool Clone_Snapshot::update_deleted_state(Clone_file_ctx *file_ctx) {
  ut_ad(mutex_own(&m_snapshot_mutex));

  if (file_ctx->m_state == Clone_file_ctx::State::DROPPED_HANDLED) {
    return false;
  }

  ut_ad(file_ctx->m_state == Clone_file_ctx::State::DROPPED);
  /* The deleted file to be handled by current task. Set the
  state here so that other tasks can ignore the deleted file. */
  file_ctx->m_state = Clone_file_ctx::State::DROPPED_HANDLED;
  return true;
}

int Clone_Snapshot::pin_file(Clone_file_ctx *file_ctx, bool &handle_delete) {
  handle_delete = false;
  file_ctx->pin();

  /* Quick return without acquiring mutex if no DDL. */
  if (!blocks_clone(file_ctx)) {
    /* Check and update deleted state. */
    if (file_ctx->deleted()) {
      IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);
      handle_delete = update_deleted_state(file_ctx);
    }
    return 0;
  }
  file_ctx->unpin();

  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);

  if (!blocks_clone(file_ctx)) {
    /* purecov: begin inspected */
    file_ctx->pin();
    /* Check and update deleted state. */
    if (file_ctx->deleted()) {
      handle_delete = update_deleted_state(file_ctx);
    }
    return 0;
    /* purecov: end */
  }

  file_ctx->begin_wait();

  /* Wait for DDL file operation to complete. */
  auto err = wait(Wait_type::DDL_FILE_OPERATION, file_ctx, false, true);

  if (err == 0) {
    file_ctx->pin();
    /* Check and update deleted state. */
    if (file_ctx->deleted()) {
      handle_delete = update_deleted_state(file_ctx);
    }
  }

  file_ctx->end_wait();
  return err;
}
