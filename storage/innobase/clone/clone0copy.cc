/*****************************************************************************

Copyright (c) 2017, 2020, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file clone/clone0copy.cc
 Innodb copy snapshot data

 *******************************************************/

#include "buf0dump.h"
#include "clone0clone.h"
#include "dict0dict.h"
#include "fsp0sysspace.h"
#include "sql/binlog.h"
#include "sql/clone_handler.h"
#include "sql/handler.h"
#include "sql/mysqld.h"
#include "srv0start.h"

/** Callback to add an archived redo file to current snapshot
@param[in]	file_name	file name
@param[in]	file_size	file size in bytes
@param[in]	file_offset	start offset in bytes
@param[in]	context		snapshot
@return	error code */
static int add_redo_file_callback(char *file_name, ib_uint64_t file_size,
                                  ib_uint64_t file_offset, void *context) {
  auto snapshot = static_cast<Clone_Snapshot *>(context);

  auto err = snapshot->add_redo_file(file_name, file_size, file_offset);

  return (err);
}

/** Callback to add tracked page IDs to current snapshot
@param[in]	context		snapshot
@param[in]	buff		buffer having page IDs
@param[in]	num_pages	number of tracked pages
@return	error code */
static int add_page_callback(void *context, byte *buff, uint num_pages) {
  uint index;
  Clone_Snapshot *snapshot;

  space_id_t space_id;
  ib_uint32_t page_num;

  snapshot = static_cast<Clone_Snapshot *>(context);

  /* Extract the page Ids from the buffer. */
  for (index = 0; index < num_pages; index++) {
    space_id = mach_read_from_4(buff);
    buff += 4;

    page_num = mach_read_from_4(buff);
    buff += 4;

    auto err = snapshot->add_page(space_id, page_num);

    if (err != 0) {
      return (err);
    }
  }

  return (0);
}

int Clone_Snapshot::add_buf_pool_file() {
  char path[OS_FILE_MAX_PATH];
  /* Generate the file name. */
  buf_dump_generate_path(path, sizeof(path));

  /* Add if the file is found. */
  int err = 0;

  if (os_file_exists(path)) {
    auto file_size = os_file_get_size(path);
    auto size_bytes = file_size.m_total_size;

    /* Check for error */
    if (size_bytes == static_cast<os_offset_t>(~0)) {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(ER_CANT_OPEN_FILE, MYF(0), path, errno,
               my_strerror(errbuf, sizeof(errbuf), errno));
      return (ER_CANT_OPEN_FILE);
    }

    /* Always the first file in list */
    ut_ad(m_num_data_files == 0);

    m_data_bytes_disk += size_bytes;
    m_monitor.add_estimate(size_bytes);

    err = add_file(path, size_bytes, size_bytes, nullptr, true);
  }

  return (err);
}

int Clone_Snapshot::init_file_copy() {
  int err = 0;

  ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

  /* If not blocking clone, allocate redo header and trailer buffer. */
  if (m_snapshot_type != HA_CLONE_BLOCKING) {
    m_redo_ctx.get_header_size(m_redo_file_size, m_redo_header_size,
                               m_redo_trailer_size);

    m_redo_header = static_cast<byte *>(mem_heap_zalloc(
        m_snapshot_heap,
        m_redo_header_size + m_redo_trailer_size + UNIV_SECTOR_SIZE));

    if (m_redo_header == nullptr) {
      my_error(ER_OUTOFMEMORY, MYF(0),
               m_redo_header_size + m_redo_trailer_size);

      return (ER_OUTOFMEMORY);
    }

    m_redo_header =
        static_cast<byte *>(ut_align(m_redo_header, UNIV_SECTOR_SIZE));

    m_redo_trailer = m_redo_header + m_redo_header_size;
  }

  if (m_snapshot_type == HA_CLONE_REDO) {
    /* Start Redo Archiving */
    err = m_redo_ctx.start(m_redo_header, m_redo_header_size);

  } else if (m_snapshot_type == HA_CLONE_HYBRID ||
             m_snapshot_type == HA_CLONE_PAGE) {
    /* Start modified Page ID Archiving */
    err = m_page_ctx.start(false, nullptr);
  } else {
    ut_ad(m_snapshot_type == HA_CLONE_BLOCKING);
  }

  if (err != 0) {
    return (err);
  }

  /* Initialize estimation about on disk bytes. */
  init_disk_estimate();

  /* Add buffer pool dump file. Always the first one in the list. */
  err = add_buf_pool_file();

  if (err != 0) {
    return (err);
  }

  /* Do not include redo files in file list. */
  bool include_log = (m_snapshot_type == HA_CLONE_BLOCKING);

  /* Iterate all tablespace files and add persistent data files. */
  auto error = Fil_iterator::for_each_file(
      include_log, [&](fil_node_t *file) { return (add_node(file)); });

  if (error != DB_SUCCESS) {
    return (ER_INTERNAL_ERROR);
  }

  ib::info(ER_IB_CLONE_OPERATION)
      << "Clone State FILE COPY : " << m_num_current_chunks << " chunks, "
      << " chunk size : " << (chunk_size() * UNIV_PAGE_SIZE) / (1024 * 1024)
      << " M";

  return (0);
}

int Clone_Snapshot::init_page_copy(byte *page_buffer, uint page_buffer_len) {
  int err = 0;

  ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

  if (m_snapshot_type == HA_CLONE_HYBRID) {
    /* Start Redo Archiving */
    err = m_redo_ctx.start(m_redo_header, m_redo_header_size);

  } else if (m_snapshot_type == HA_CLONE_PAGE) {
    /* Start COW for all modified pages - Not implemented. */
    ut_ad(false);
  } else {
    ut_ad(false);
  }

  if (err != 0) {
    m_page_ctx.release();
    return (err);
  }

  /* Stop modified page archiving. */
  err = m_page_ctx.stop(nullptr);

  DEBUG_SYNC_C("clone_stop_page_archiving_without_releasing");

  if (err != 0) {
    m_page_ctx.release();
    return (err);
  }

  /* Collect modified page Ids from Page Archiver. */
  void *context;
  uint aligned_size;

  context = static_cast<void *>(this);

  /* Check pages added for encryption. */
  auto num_pages_encryption = m_page_set.size();

  if (num_pages_encryption > 0) {
    m_monitor.add_estimate(num_pages_encryption * UNIV_PAGE_SIZE);
  }

  err = m_page_ctx.get_pages(add_page_callback, context, page_buffer,
                             page_buffer_len);

  m_page_vector.assign(m_page_set.begin(), m_page_set.end());

  aligned_size = ut_calc_align(m_num_pages, chunk_size());
  m_num_current_chunks = aligned_size >> m_chunk_size_pow2;

  ib::info(ER_IB_CLONE_OPERATION)
      << "Clone State PAGE COPY : " << m_num_pages << " pages, "
      << m_num_duplicate_pages << " duplicate pages, " << m_num_current_chunks
      << " chunks, "
      << " chunk size : " << (chunk_size() * UNIV_PAGE_SIZE) / (1024 * 1024)
      << " M";
  m_page_ctx.release();
  return (err);
}

int Clone_Snapshot::synchronize_binlog_gtid(Clone_Alert_Func cbk) {
  /* Get a list of binlog prepared transactions and wait for them to commit
  or rollback. This is to ensure that any possible unordered transactions
  are completed. */
  auto error = wait_for_binlog_prepared_trx();

  if (error != 0) {
    return (error);
  }

  /* Persist non-innodb GTIDs */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();
  gtid_persistor.wait_flush(true, false, cbk);

  error = update_binlog_position();
  return (error);
}

int Clone_Snapshot::update_binlog_position() {
  /* Since the caller ensures all future commits are in order of binary log and
  innodb updates trx sys page for all transactions by default, any single
  transaction commit here would ensure that the binary log position is
  synchronized. However, currently we don't create any special table for clone
  and we cannot execute transaction/dml here. A possible simplification for
  future.

  Ideally the only case we need to update innodb trx sys page is when no
  transaction commit has happened yet after forced ordering is imposed. We
  end up updating the page in more cases but is harmless. We follow the steps
  below.

  1. Note the last updated Innodb binary log position - P1

  2. Note the current log position from binary log - P2
     All transactions up to this point are already prepared and may or may not
     be committed.

  3. Note the Innodb binary log position again - P3
     if P1 != P3 then exit as there is already some new transaction committed.
     if P1 == P3 then update the trx sys log position with P2
  *Check and update in [3] are atomic for trx sys page.

  4. Wait for all binary log prepared transaction to complete. We have
  updated the trx sys page out of order but it is sufficient to ensure that
  all transaction up to the updated binary log position are committed. */

  /* 1. Read binary log position from innodb. */
  LOG_INFO log_info1;
  char file_name[TRX_SYS_MYSQL_LOG_NAME_LEN + 1];
  uint64_t file_pos;
  trx_sys_read_binlog_position(&file_name[0], file_pos);

  /* 2. Get current binary log position. */
  LOG_INFO log_info;
  mysql_bin_log.get_current_log(&log_info);

  /* 3. Check and write binary log position in Innodb. */
  bool written = trx_sys_write_binlog_position(
      &file_name[0], file_pos, &log_info.log_file_name[0], log_info.pos);

  /* 4. If we had to write current binary log position, should wait for all
  prepared transactions to finish to make sure that all transactions up to
  the binary log position is committed. */
  if (written) {
    auto err = wait_for_binlog_prepared_trx();
    return (err);
  }
  return (0);
}

int Clone_Snapshot::wait_trx_end(THD *thd, trx_id_t trx_id) {
  auto trx = trx_rw_is_active(trx_id, nullptr, false);
  if (trx == nullptr) {
    return (0);
  }

  auto wait_cond = [&](bool alert, bool &result) {
    /* Check if transaction is still active. */
    auto trx = trx_rw_is_active(trx_id, nullptr, false);
    if (trx == nullptr) {
      result = false;
      return (0);
    }

    result = true;
    if (thd_killed(thd)) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      return (ER_QUERY_INTERRUPTED);
    }

    if (alert) {
      ib::warn(ER_IB_CLONE_TIMEOUT)
          << "Waiting for prepared transaction to exit";
    }
    return (0);
  };

  bool is_timeout = false;

  /* Sleep for 10 millisecond */
  Clone_Msec sleep_time(10);
  /* Generate alert message every 5 second. */
  Clone_Sec alert_interval(5);
  /* Wait for 5 minutes. */
  Clone_Sec time_out(Clone_Min(5));

  auto err = Clone_Sys::wait(sleep_time, time_out, alert_interval, wait_cond,
                             nullptr, is_timeout);

  if (err == 0 && is_timeout) {
    ib::info(ER_IB_CLONE_TIMEOUT)
        << "Clone wait for prepared transaction timed out";
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Innodb Clone wait for prepared transaction timed out.");
    err = ER_INTERNAL_ERROR;
  }
  return (err);
}

/* To get current session thread default THD */
THD *thd_get_current_thd();

int Clone_Snapshot::wait_for_binlog_prepared_trx() {
  /* Return if binary log is not enabled. */
  if (!opt_bin_log) {
    return (0);
  }
  auto thd = thd_get_current_thd();
  /* Get all binlog prepared transactions. */
  std::vector<trx_id_t> trx_ids;
  trx_sys_get_binlog_prepared(trx_ids);

  /* Now wait for the transactions to finish. */
  for (auto trx_id : trx_ids) {
    auto err = wait_trx_end(thd, trx_id);
    if (err != 0) {
      return (err);
    }
  }
  return (0);
}

int Clone_Snapshot::init_redo_copy(Clone_Alert_Func cbk) {
  ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);
  ut_ad(m_snapshot_type != HA_CLONE_BLOCKING);

  /* Block external XA operations. XA prepare commit and rollback operations
  are first logged to binlog and added to global gtid_executed before doing
  operation in SE. Without blocking, we might persist such GTIDs from global
  gtid_executed before the operations are persisted in Innodb. */
  int binlog_error = 0;
  auto thd = thd_get_current_thd();
  Clone_handler::XA_Block xa_block_guard(thd);

  if (xa_block_guard.failed()) {
    if (thd_killed(thd)) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      binlog_error = ER_QUERY_INTERRUPTED;
    } else {
      ut_ad(false);
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Clone wait for XA operation timed out.");
      binlog_error = ER_INTERNAL_ERROR;
    }
  }

  /* Before stopping redo log archiving synchronize with binlog and GTID. At
  this point a transaction can commit only in the order they are written to
  binary log. We have ensure this by forcing ordered commit and waiting for
  all unordered transactions to finish. */
  if (binlog_error == 0) {
    binlog_error = synchronize_binlog_gtid(cbk);
  }

  /* Stop redo archiving even on error. */
  auto redo_error = m_redo_ctx.stop(m_redo_trailer, m_redo_trailer_size,
                                    m_redo_trailer_offset);

  if (binlog_error != 0) {
    return (binlog_error);
  }

  if (redo_error != 0) {
    return (redo_error);
  }

  /* Collect archived redo log files from Log Archiver. */
  auto context = static_cast<void *>(this);

  redo_error = m_redo_ctx.get_files(add_redo_file_callback, context);

  /* Add another chunk for the redo log header. */
  ++m_num_redo_chunks;

  m_monitor.add_estimate(m_redo_header_size);

  /* Add another chunk for the redo log trailer. */
  ++m_num_redo_chunks;

  if (m_redo_trailer_size != 0) {
    m_monitor.add_estimate(m_redo_trailer_size);
  }

  m_num_current_chunks = m_num_redo_chunks;

  ib::info(ER_IB_CLONE_OPERATION)
      << "Clone State REDO COPY : " << m_num_current_chunks << " chunks, "
      << " chunk size : " << (chunk_size() * UNIV_PAGE_SIZE) / (1024 * 1024)
      << " M";

  return (redo_error);
}

Clone_File_Meta *Clone_Snapshot::build_file(const char *file_name,
                                            uint64_t file_size,
                                            uint64_t file_offset,
                                            uint &num_chunks,
                                            bool copy_file_name) {
  Clone_File_Meta *file_meta;

  ib_uint64_t aligned_size;
  ib_uint64_t size_in_pages;

  /* Allocate for file metadata from snapshot heap. */
  aligned_size = sizeof(Clone_File_Meta);

  if (file_name != nullptr && copy_file_name) {
    aligned_size += strlen(file_name) + 1;
  }

  file_meta = static_cast<Clone_File_Meta *>(
      mem_heap_alloc(m_snapshot_heap, aligned_size));

  if (file_meta == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(0), static_cast<int>(aligned_size));
    return (file_meta);
  }

  /* For redo file with no data, add dummy entry. */
  if (file_name == nullptr) {
    num_chunks = 1;

    file_meta->m_file_name = nullptr;
    file_meta->m_file_name_len = 0;
    file_meta->m_file_size = 0;

    file_meta->m_begin_chunk = 1;
    file_meta->m_end_chunk = 1;

    return (file_meta);
  }

  file_meta->m_file_size = file_size;

  /* reduce offset amount from total size */
  ut_ad(file_size >= file_offset);
  file_size -= file_offset;

  /* Calculate and set chunk parameters. */
  size_in_pages = ut_uint64_align_up(file_size, UNIV_PAGE_SIZE);
  size_in_pages /= UNIV_PAGE_SIZE;

  aligned_size = ut_uint64_align_up(size_in_pages, chunk_size());

  num_chunks = static_cast<uint>(aligned_size >> m_chunk_size_pow2);

  file_meta->m_begin_chunk = m_num_current_chunks + 1;
  file_meta->m_end_chunk = m_num_current_chunks + num_chunks;

  file_meta->m_file_name_len = strlen(file_name) + 1;

  if (copy_file_name) {
    char *tmp_name = reinterpret_cast<char *>(file_meta + 1);

    strcpy(tmp_name, file_name);
    file_meta->m_file_name = const_cast<const char *>(tmp_name);
  } else {
    /* We use the same pointer as the tablespace and files
    should not be dropped or changed during clone. */
    file_meta->m_file_name = file_name;
  }

  return (file_meta);
}

/** Check if the tablespace file is temporary file created by DDL
@param[in]	node	file node
@return true if created by DDL */
static bool is_ddl_temp_table(fil_node_t *node) {
  const char *name_ptr;

  name_ptr = strrchr(node->name, OS_PATH_SEPARATOR);

  if (name_ptr == nullptr) {
    name_ptr = node->name;
  } else {
    name_ptr++;
  }

  /* Check if it is a temporary table created by DDL. This is work
  around to identify concurrent DDL till server provides MDL lock
  for blocking DDL. */
  if (strncmp(name_ptr, TEMP_FILE_PREFIX, TEMP_FILE_PREFIX_LENGTH) == 0) {
    return (true);
  }

  return (false);
}

int Clone_Snapshot::add_file(const char *name, uint64_t size_bytes,
                             uint64_t alloc_bytes, fil_node_t *node,
                             bool copy_name) {
  ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

  uint num_chunks;

  /* Build file metadata entry and add to data file vector. */
  auto file_meta = build_file(name, size_bytes, 0, num_chunks, copy_name);

  if (file_meta == nullptr) {
    return (ER_OUTOFMEMORY);
  }

  file_meta->m_alloc_size = alloc_bytes;

  /* Set space ID, compression and encryption attribute */
  if (node == nullptr) {
    file_meta->m_space_id = dict_sys_t::s_invalid_space_id;
    file_meta->m_compress_type = Compression::NONE;
    file_meta->m_encrypt_type = Encryption::NONE;
    file_meta->m_fsp_flags = UINT32_UNDEFINED;
    file_meta->m_punch_hole = false;
    file_meta->m_fsblk_size = 0;

  } else {
    auto space = node->space;
    file_meta->m_space_id = space->id;
    file_meta->m_compress_type = space->compression_type;
    file_meta->m_encrypt_type = space->encryption_type;
    file_meta->m_fsp_flags = static_cast<uint32_t>(space->flags);
    file_meta->m_punch_hole = node->punch_hole;
    file_meta->m_fsblk_size = node->block_size;

    /* If file node supports punch hole then check if we need it. */
    if (file_meta->m_punch_hole) {
      page_size_t page_size(space->flags);
      /* Transparent compression is skipped if table compression is enabled. */
      if (page_size.is_compressed() ||
          space->compression_type == Compression::NONE) {
        file_meta->m_punch_hole = false;
      }
    }
  }

  file_meta->m_file_index = m_num_data_files;

  m_data_file_vector.push_back(file_meta);

  ++m_num_data_files;

  ut_ad(m_data_file_vector.size() == m_num_data_files);

  /* Update total number of chunks. */
  m_num_data_chunks += num_chunks;
  m_num_current_chunks = m_num_data_chunks;

  /* Update maximum file name length in snapshot. */
  if (file_meta->m_file_name_len > m_max_file_name_len) {
    m_max_file_name_len = static_cast<uint32_t>(file_meta->m_file_name_len);
  }

  return (0);
}

dberr_t Clone_Snapshot::add_node(fil_node_t *node) {
  ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

  /* Exit if concurrent DDL in progress. */
  if (is_ddl_temp_table(node)) {
    my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
    return (DB_ERROR);
  }

  auto space = node->space;

  /* Skip deleted tablespaces. Their pages may still be in
  the buffer pool. */
  if (space->is_deleted()) {
    return (DB_SUCCESS);
  }

  if (space->encryption_type != Encryption::NONE) {
    /* Add page 0 always for encrypted tablespace. */
    Clone_Page page_zero;
    page_zero.m_space_id = space->id;
    page_zero.m_page_no = 0;
    m_page_set.insert(page_zero);
    ++m_num_pages;
  }

  /* Find out the file size from node. */
  page_size_t page_sz(space->flags);

  /* For compressed pages the file size doesn't match
  physical page size multiplied by number of pages. It is
  because we use UNIV_PAGE_SIZE while creating the node
  and tablespace. */
  auto file_size = os_file_get_size(node->name);
  auto size_bytes = file_size.m_total_size;
  auto alloc_size = file_size.m_alloc_size;

  /* Check for error */
  if (size_bytes == static_cast<os_offset_t>(~0)) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_CANT_OPEN_FILE, MYF(0), node->name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    return (DB_ERROR);
  }

  /* Update estimation */
  m_data_bytes_disk += alloc_size;
  m_monitor.add_estimate(size_bytes);

  /* Add file to snapshot. */
  auto err = add_file(node->name, size_bytes, alloc_size, node, false);

  if (err != 0) {
    return (DB_ERROR);
  }

  /* Add to hash map only for first node of the tablespace. */
  if (m_data_file_map[space->id] == 0) {
    m_data_file_map[space->id] = m_num_data_files;
  }

  return (DB_SUCCESS);
}

int Clone_Snapshot::add_page(space_id_t space_id, ib_uint32_t page_num) {
  /* Skip pages belonging to tablespace not included for clone. This could
  be some left over pages from drop or truncate in buffer pool which
  would eventually get removed. Or it may be a page for an undo tablespace
  that was deleted with BUF_REMOVE_NONE. */
  auto count = m_data_file_map.count(space_id);
  if (count == 0) {
    return (0);
  }

  Clone_Page cur_page;
  cur_page.m_space_id = space_id;
  cur_page.m_page_no = page_num;

  auto result = m_page_set.insert(cur_page);

  if (result.second) {
    m_num_pages++;
    m_monitor.add_estimate(UNIV_PAGE_SIZE);
  } else {
    m_num_duplicate_pages++;
  }

  return (0);
}

int Clone_Snapshot::add_redo_file(char *file_name, uint64_t file_size,
                                  uint64_t file_offset) {
  ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

  Clone_File_Meta *file_meta;
  uint num_chunks;

  /* Build redo file metadata and add to redo vector. */
  file_meta = build_file(file_name, file_size, file_offset, num_chunks, true);

  m_monitor.add_estimate(file_meta->m_file_size - file_offset);

  if (file_meta == nullptr) {
    return (ER_OUTOFMEMORY);
  }

  /* Set the start offset for first redo file. This could happen
  if redo archiving was already in progress, possibly by another
  concurrent snapshot. */
  if (m_num_redo_files == 0) {
    m_redo_start_offset = file_offset;
  } else {
    ut_ad(file_offset == 0);
  }

  file_meta->m_alloc_size = 0;

  file_meta->m_space_id = dict_sys_t::s_log_space_first_id;
  file_meta->m_compress_type = Compression::NONE;

  /* Fill encryption type from redo tablespace. */
  auto redo_space = fil_space_get(dict_sys_t::s_log_space_first_id);
  file_meta->m_encrypt_type = redo_space->encryption_type;

  file_meta->m_fsp_flags = UINT32_UNDEFINED;
  file_meta->m_punch_hole = false;
  file_meta->m_fsblk_size = 0;

  file_meta->m_file_index = m_num_redo_files;

  m_redo_file_vector.push_back(file_meta);
  ++m_num_redo_files;

  ut_ad(m_redo_file_vector.size() == m_num_redo_files);

  m_num_redo_chunks += num_chunks;
  m_num_current_chunks = m_num_redo_chunks;

  /* In rare case of small redo file, large concurrent DMLs and
  slow data transfer. Currently we support maximum 1k redo files. */
  if (m_num_redo_files > SRV_N_LOG_FILES_CLONE_MAX) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "More than 1000 archived redo files. Please retry clone.");
    return (ER_INTERNAL_ERROR);
  }

  return (0);
}

int Clone_Handle::send_task_metadata(Clone_Task *task, Ha_clone_cbk *callback) {
  Clone_Desc_Task_Meta task_desc;

  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  /* Build task descriptor with metadata */
  task_desc.init_header(get_version());
  task_desc.m_task_meta = task->m_task_meta;

  auto desc_len = task->m_alloc_len;
  task_desc.serialize(task->m_serial_desc, desc_len, nullptr);

  callback->set_data_desc(task->m_serial_desc, desc_len);
  callback->clear_flags();
  callback->set_ack();

  auto err = callback->buffer_cbk(nullptr, 0);

  return (err);
}

int Clone_Handle::send_keep_alive(Clone_Task *task, Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  Clone_Desc_State state_desc;
  state_desc.init_header(get_version());

  /* Build state descriptor from snapshot and task */
  auto snapshot = m_clone_task_manager.get_snapshot();
  snapshot->get_state_info(false, &state_desc);

  state_desc.m_is_ack = true;

  auto task_meta = &task->m_task_meta;
  state_desc.m_task_index = task_meta->m_task_index;

  auto desc_len = task->m_alloc_len;
  state_desc.serialize(task->m_serial_desc, desc_len, nullptr);

  callback->set_data_desc(task->m_serial_desc, desc_len);
  callback->clear_flags();

  auto err = callback->buffer_cbk(nullptr, 0);

  return (err);
}

int Clone_Handle::send_state_metadata(Clone_Task *task, Ha_clone_cbk *callback,
                                      bool is_start) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  Clone_Desc_State state_desc;
  state_desc.init_header(get_version());

  /* Build state descriptor from snapshot and task */
  auto snapshot = m_clone_task_manager.get_snapshot();

  /* Master needs to send estimation while beginning state */
  auto get_estimate = (task->m_is_master && is_start);

  snapshot->get_state_info(get_estimate, &state_desc);

  /* Indicate if it is the end of state */
  state_desc.m_is_start = is_start;

  /* Check if remote has already acknowledged state transfer */
  if (!is_start && task->m_is_master &&
      !m_clone_task_manager.check_ack(&state_desc)) {
    ut_ad(task->m_is_master);
    ut_ad(m_clone_task_manager.is_restarted());

    ib::info(ER_IB_CLONE_RESTART)
        << "CLONE COPY: Skip ACK after restart for state "
        << state_desc.m_state;
    return (0);
  }

  auto task_meta = &task->m_task_meta;
  state_desc.m_task_index = task_meta->m_task_index;

  auto desc_len = task->m_alloc_len;
  state_desc.serialize(task->m_serial_desc, desc_len, nullptr);

  callback->set_data_desc(task->m_serial_desc, desc_len);
  callback->clear_flags();
  callback->set_ack();

  auto err = callback->buffer_cbk(nullptr, 0);

  if (err != 0) {
    return (err);
  }

  if (is_start) {
    /* Send all file metadata while starting state */
    err = send_all_file_metadata(task, callback);

  } else {
    /* Wait for ACK while finishing state */
    err = m_clone_task_manager.wait_ack(this, task, callback);
  }

  return (err);
}

int Clone_Handle::send_all_file_metadata(Clone_Task *task,
                                         Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  if (!task->m_is_master) {
    return (0);
  }

  auto snapshot = m_clone_task_manager.get_snapshot();

  /* Send all file metadata for data/redo files */
  auto err = snapshot->iterate_files([&](Clone_File_Meta *file_meta) {
    return (send_file_metadata(task, file_meta, callback));
  });

  return (err);
}

int Clone_Handle::send_file_metadata(Clone_Task *task,
                                     Clone_File_Meta *file_meta,
                                     Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  auto snapshot = m_clone_task_manager.get_snapshot();

  Clone_Desc_File_MetaData file_desc;

  file_desc.m_file_meta = *file_meta;
  file_desc.m_state = snapshot->get_state();

  if (file_desc.m_state == CLONE_SNAPSHOT_REDO_COPY) {
    /* For Redo log always send the fixed redo file size. */
    file_desc.m_file_meta.m_file_size = snapshot->get_redo_file_size();

    file_desc.m_file_meta.m_file_name = nullptr;
    file_desc.m_file_meta.m_file_name_len = 0;

  } else if (file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {
    /* Server buffer dump file ib_buffer_pool. */
    ut_ad(file_desc.m_state == CLONE_SNAPSHOT_FILE_COPY);
    ut_ad(file_meta->m_file_index == 0);

    file_desc.m_file_meta.m_file_name = SRV_BUF_DUMP_FILENAME_DEFAULT;

    file_desc.m_file_meta.m_file_name_len =
        static_cast<uint32_t>(strlen(SRV_BUF_DUMP_FILENAME_DEFAULT)) + 1;

  } else if (!fsp_is_ibd_tablespace(
                 static_cast<space_id_t>(file_meta->m_space_id))) {
    /* For system tablespace, remove path. */
    ut_ad(file_desc.m_state == CLONE_SNAPSHOT_FILE_COPY);

    auto name_ptr = strrchr(file_meta->m_file_name, OS_PATH_SEPARATOR);

    if (name_ptr != nullptr) {
      name_ptr++;

      file_desc.m_file_meta.m_file_name = name_ptr;
      file_desc.m_file_meta.m_file_name_len =
          static_cast<uint32_t>(strlen(name_ptr)) + 1;
    }
  }

  file_desc.init_header(get_version());

  auto desc_len = task->m_alloc_len;
  file_desc.serialize(task->m_serial_desc, desc_len, nullptr);

  callback->set_data_desc(task->m_serial_desc, desc_len);
  callback->clear_flags();

  /* Check for secure transfer for encrypted table. */
  if (file_meta->m_encrypt_type != Encryption::NONE || srv_undo_log_encrypt ||
      srv_redo_log_encrypt) {
    callback->set_secure();
  }

  auto err = callback->buffer_cbk(nullptr, 0);

  return (err);
}

int Clone_Handle::send_data(Clone_Task *task, Clone_File_Meta *file_meta,
                            uint64_t offset, byte *buffer, uint size,
                            Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  auto snapshot = m_clone_task_manager.get_snapshot();

  /* Build data descriptor */
  Clone_Desc_Data data_desc;
  data_desc.init_header(get_version());
  data_desc.m_state = snapshot->get_state();

  data_desc.m_task_meta = task->m_task_meta;

  data_desc.m_file_index = file_meta->m_file_index;
  data_desc.m_data_len = size;
  data_desc.m_file_offset = offset;
  data_desc.m_file_size = file_meta->m_file_size;

  /* Serialize data descriptor and set in callback */
  auto desc_len = task->m_alloc_len;
  data_desc.serialize(task->m_serial_desc, desc_len, nullptr);

  callback->set_data_desc(task->m_serial_desc, desc_len);
  callback->clear_flags();

  auto file_type = OS_CLONE_DATA_FILE;
  bool is_log_file = (data_desc.m_state == CLONE_SNAPSHOT_REDO_COPY);

  if (is_log_file || file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {
    file_type = OS_CLONE_LOG_FILE;
  }

  int err = 0;

  if (buffer != nullptr) {
    /* Send data from buffer. */
    err = callback->buffer_cbk(buffer, size);

  } else {
    /* Send data from file. */
    if (task->m_current_file_des.m_file == OS_FILE_CLOSED) {
      err = open_file(task, file_meta, file_type, false, false);

      if (err != 0) {
        return (err);
      }
    }

    ut_ad(task->m_current_file_index == file_meta->m_file_index);

    os_file_t file_hdl;
    char errbuf[MYSYS_STRERROR_SIZE];

    file_hdl = task->m_current_file_des.m_file;
    auto success = os_file_seek(nullptr, file_hdl, offset);

    if (!success) {
      my_error(ER_ERROR_ON_READ, MYF(0), file_meta->m_file_name, errno,
               my_strerror(errbuf, sizeof(errbuf), errno));
      return (ER_ERROR_ON_READ);
    }

    if (task->m_file_cache) {
      callback->set_os_buffer_cache();
      /* For data file recommend zero copy for cached IO. */
      if (!is_log_file) {
        callback->set_zero_copy();
      }
    }

    callback->set_source_name(file_meta->m_file_name);

    err = file_callback(callback, task, size, false, offset
#ifdef UNIV_PFS_IO
                        ,
                        __FILE__, __LINE__
#endif /* UNIV_PFS_IO */
    );
  }

  task->m_data_size += size;

  return (err);
}

void Clone_Handle::display_progress(uint32_t cur_chunk, uint32_t max_chunk,
                                    uint32_t &percent_done,
                                    ib_time_monotonic_ms_t &disp_time) {
  auto current_time = ut_time_monotonic_ms();
  auto current_percent = (cur_chunk * 100) / max_chunk;

  if (current_percent >= percent_done + 20 ||
      (current_time - disp_time > 5000 && current_percent > percent_done)) {
    percent_done = current_percent;
    disp_time = current_time;

    ib::info(ER_IB_CLONE_OPERATION)
        << "Stage progress: " << percent_done << "% completed.";
  }
}

int Clone_Handle::copy(THD *thd, uint task_id, Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  /* Get task from task manager. */
  auto task = m_clone_task_manager.get_task_by_index(task_id);

  auto err = m_clone_task_manager.alloc_buffer(task);
  if (err != 0) {
    return (err);
  }

  /* Allow restart only after copy is started. */
  m_allow_restart = true;

  /* Send the task metadata. */
  err = send_task_metadata(task, callback);

  if (err != 0) {
    return (err);
  }

  auto send_matadata = m_clone_task_manager.is_restart_metadata(task);

  /* Send state metadata to remote during restart */
  if (send_matadata) {
    ut_ad(task->m_is_master);
    ut_ad(m_clone_task_manager.is_restarted());

    err = send_state_metadata(task, callback, true);

    /* Send all file metadata during restart */
  } else if (task->m_is_master &&
             m_clone_task_manager.get_state() == CLONE_SNAPSHOT_FILE_COPY &&
             !m_clone_task_manager.is_file_metadata_transferred()) {
    ut_ad(m_clone_task_manager.is_restarted());
    err = send_all_file_metadata(task, callback);
  }

  if (err != 0) {
    return (err);
  }
  /* Adjust block size based on client buffer size. */
  auto snapshot = m_clone_task_manager.get_snapshot();
  snapshot->update_block_size(callback->get_client_buffer_size());

  auto max_chunks = snapshot->get_num_chunks();

  /* Set time values for tracking stage progress. */

  auto disp_time = ut_time_monotonic_ms();

  /* Loop and process data until snapshot is moved to DONE state. */
  uint32_t percent_done = 0;

  while (m_clone_task_manager.get_state() != CLONE_SNAPSHOT_DONE) {
    /* Reserve next chunk for current state from snapshot. */
    uint32_t current_chunk = 0;
    uint32_t current_block = 0;

    err = m_clone_task_manager.reserve_next_chunk(task, current_chunk,
                                                  current_block);

    if (err != 0) {
      break;
    }

    if (current_chunk != 0) {
      /* Send blocks from the reserved chunk. */
      err = process_chunk(task, current_chunk, current_block, callback);

      /* Display stage progress based on % completion. */
      if (task->m_is_master) {
        display_progress(current_chunk, max_chunks, percent_done, disp_time);
      }

    } else {
      /* No more chunks in current state. Transit to next state. */

      /* Close the last open file before proceeding to next state */
      err = close_file(task);

      if (err != 0) {
        break;
      }

      /* Inform that the data transfer for current state
      is over before moving to next state. The remote
      needs to send back state transfer ACK for the state
      transfer to complete. */
      err = send_state_metadata(task, callback, false);

      if (err != 0) {
        break;
      }

      /* Next state is decided by snapshot for Copy. */
      err = move_to_next_state(task, callback, nullptr);

      ut_d(task->m_ignore_sync = false);

      if (err != 0) {
        break;
      }

      max_chunks = snapshot->get_num_chunks();
      percent_done = 0;
      disp_time = ut_time_monotonic_ms();

      /* Send state metadata before processing chunks. */
      err = send_state_metadata(task, callback, true);
    }

    if (err != 0) {
      break;
    }
  }

  /* Close the last open file after error. */
  if (err != 0) {
    close_file(task);
  }

  return (err);
}

int Clone_Handle::process_chunk(Clone_Task *task, uint32_t chunk_num,
                                uint32_t block_num, Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

  Clone_File_Meta file_meta;

  auto &task_meta = task->m_task_meta;

  file_meta.m_file_index = 0;

  /* If chunks are in increasing order, optimize file
  search by index */
  if (task_meta.m_chunk_num <= chunk_num) {
    file_meta.m_file_index = task->m_current_file_index;
  }

  auto snapshot = m_clone_task_manager.get_snapshot();
  auto state = m_clone_task_manager.get_state();

  /* Loop over all the blocks of current chunk and send data. */
  int err = 0;

  while (err == 0) {
    auto data_buf = task->m_current_buffer;
    auto data_size = task->m_buffer_alloc_len;

    /* Get next block from snapshot */
    ib_uint64_t data_offset;

    err = snapshot->get_next_block(chunk_num, block_num, &file_meta,
                                   data_offset, data_buf, data_size);

    /* '0' block number indicates no more blocks. */
    if (err != 0 || block_num == 0) {
      break;
    }

    /* Check for error from other tasks and DDL */
    err = m_clone_task_manager.handle_error_other_task(task->m_has_thd);

    if (err != 0) {
      break;
    }

    task->m_task_meta.m_block_num = block_num;
    task->m_task_meta.m_chunk_num = chunk_num;

    if (state != CLONE_SNAPSHOT_PAGE_COPY &&
        (task->m_current_file_des.m_file == OS_FILE_CLOSED ||
         task->m_current_file_index != file_meta.m_file_index)) {
      /* We are moving to next file. Close the current file and
      send metadata for the next file. */

      err = close_file(task);

      if (err != 0) {
        break;
      }

      /* During redo copy, worker could be ahead of master and needs to
      send the metadata */
      if (state == CLONE_SNAPSHOT_REDO_COPY) {
        err = send_file_metadata(task, &file_meta, callback);
      }

      if (err != 0) {
        break;
      }
    }

    if (data_size == 0) {
      continue;
    }

    err =
        send_data(task, &file_meta, data_offset, data_buf, data_size, callback);
  }

  /* Save current error and file name. */
  if (err != 0) {
    m_clone_task_manager.set_error(err, file_meta.m_file_name);
  }

  return (err);
}

int Clone_Handle::restart_copy(THD *thd, const byte *loc, uint loc_len) {
  ut_ad(mutex_own(clone_sys->get_mutex()));

  if (is_abort()) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Innodb Clone Restart failed, existing clone aborted");
    return (ER_INTERNAL_ERROR);
  }

  /* Wait for the Idle state */
  if (!is_idle()) {
    /* Sleep for 1 second */
    Clone_Msec sleep_time(Clone_Sec(1));
    /* Generate alert message every 5 seconds. */
    Clone_Sec alert_time(5);
    /* Wait for 30 seconds for server to reach idle state. */
    Clone_Sec time_out(30);

    bool is_timeout = false;
    auto err = Clone_Sys::wait(
        sleep_time, time_out, alert_time,
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(clone_sys->get_mutex()));
          result = !is_idle();

          if (thd_killed(thd)) {
            my_error(ER_QUERY_INTERRUPTED, MYF(0));
            return (ER_QUERY_INTERRUPTED);

          } else if (is_abort()) {
            my_error(ER_INTERNAL_ERROR, MYF(0),
                     "Innodb Clone Restart failed, existing clone aborted");
            return (ER_INTERNAL_ERROR);

          } else if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
            my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
            return (ER_CLONE_DDL_IN_PROGRESS);
          }

          if (result && alert) {
            ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Master Restart "
                                             "wait for idle state";
          }
          return (0);
        },
        clone_sys->get_mutex(), is_timeout);

    if (err != 0) {
      return (err);

    } else if (is_timeout) {
      ib::info(ER_IB_CLONE_TIMEOUT)
          << "Clone Master restart wait for idle timed out";

      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Clone restart wait for idle state timed out");
      return (ER_INTERNAL_ERROR);
    }
  }

  ut_ad(is_idle());
  m_clone_task_manager.reinit_copy_state(loc, loc_len);

  set_state(CLONE_STATE_ACTIVE);

  return (0);
}
