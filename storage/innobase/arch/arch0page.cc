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

/** @file arch/arch0page.cc
 Innodb implementation for page archive

 *******************************************************/

#include "arch0page.h"
#include "arch0recv.h"
#include "clone0clone.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "srv0start.h"

#ifdef UNIV_DEBUG
/** Archived page file default size in number of blocks. */
uint ARCH_PAGE_FILE_CAPACITY =
    (ARCH_PAGE_BLK_SIZE - ARCH_PAGE_BLK_HEADER_LENGTH) / ARCH_BLK_PAGE_ID_SIZE;

/** Archived page data file size (without header) in number of blocks. */
uint ARCH_PAGE_FILE_DATA_CAPACITY =
    ARCH_PAGE_FILE_CAPACITY - ARCH_PAGE_FILE_NUM_RESET_PAGE;
#endif

/** Event to signal the page archiver thread. */
os_event_t page_archiver_thread_event;

/** Archiver background thread */
void page_archiver_thread() {
  bool page_wait = false;

  Arch_Group::init_dblwr_file_ctx(
      ARCH_DBLWR_DIR, ARCH_DBLWR_FILE, ARCH_DBLWR_NUM_FILES,
      static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_DBLWR_FILE_CAPACITY);

  while (true) {
    /* Archive in memory data blocks to disk. */
    auto page_abort = arch_page_sys->archive(&page_wait);

    if (page_abort) {
      ib::info(ER_IB_MSG_14) << "Exiting Page Archiver";
      break;
    }

    if (page_wait) {
      /* Nothing to archive. Wait until next trigger. */
      os_event_wait(page_archiver_thread_event);
      os_event_reset(page_archiver_thread_event);
    }
  }
}

void Arch_Reset_File::init() {
  m_file_index = 0;
  m_lsn = LSN_MAX;
  m_start_point.clear();
}

Arch_File_Ctx Arch_Group::s_dblwr_file_ctx;

Arch_Group::~Arch_Group() {
  ut_ad(!m_is_active);

  m_file_ctx.close();

  if (m_active_file.m_file != OS_FILE_CLOSED) {
    os_file_close(m_active_file);
  }

  if (m_durable_file.m_file != OS_FILE_CLOSED) {
    os_file_close(m_durable_file);
  }

  if (m_active_file_name != nullptr) {
    ut::free(m_active_file_name);
  }

  if (m_durable_file_name != nullptr) {
    ut::free(m_durable_file_name);
  }

  if (!is_durable()) {
    m_file_ctx.delete_files(m_begin_lsn);
  }
}

dberr_t Arch_Group::write_to_doublewrite_file(Arch_File_Ctx *from_file,
                                              byte *from_buffer,
                                              uint write_size,
                                              Arch_Page_Dblwr_Offset offset) {
  dberr_t err = DB_SUCCESS;

  ut_ad(!s_dblwr_file_ctx.is_closed());

  switch (offset) {
    case ARCH_PAGE_DBLWR_RESET_PAGE:
      DBUG_EXECUTE_IF("crash_before_reset_block_dblwr_flush", DBUG_SUICIDE(););
      break;

    case ARCH_PAGE_DBLWR_PARTIAL_FLUSH_PAGE:
      DBUG_EXECUTE_IF("crash_before_partial_block_dblwr_flush",
                      DBUG_SUICIDE(););
      break;

    case ARCH_PAGE_DBLWR_FULL_FLUSH_PAGE:
      DBUG_EXECUTE_IF("crash_before_full_block_dblwr_flush", DBUG_SUICIDE(););
      break;
  }

  err = s_dblwr_file_ctx.write(from_file, from_buffer,
                               offset * ARCH_PAGE_BLK_SIZE, write_size);

  if (err != DB_SUCCESS) {
    return (err);
  }

  s_dblwr_file_ctx.flush();

  return (err);
}

dberr_t Arch_Group::init_dblwr_file_ctx(const char *path, const char *base_file,
                                        uint num_files, uint64_t file_size) {
  auto err = s_dblwr_file_ctx.init(ARCH_DIR, path, base_file, num_files);

  if (err != DB_SUCCESS) {
    ut_ad(s_dblwr_file_ctx.get_phy_size() == file_size);
    return (err);
  }

  err = s_dblwr_file_ctx.open(false, LSN_MAX, 0, 0, 0);

  if (err != DB_SUCCESS) {
    return (err);
  }

  return s_dblwr_file_ctx.resize_and_overwrite_with_zeros(file_size);
}

dberr_t Arch_Group::build_active_file_name() {
  char dir_name[MAX_ARCH_DIR_NAME_LEN];
  auto length = MAX_ARCH_DIR_NAME_LEN + 1 + MAX_ARCH_PAGE_FILE_NAME_LEN + 1;

  if (m_active_file_name != nullptr) {
    return (DB_SUCCESS);
  }

  m_active_file_name = static_cast<char *>(
      ut::malloc_withkey(ut::make_psi_memory_key(mem_key_archive), length));

  if (m_active_file_name == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  get_dir_name(dir_name, MAX_ARCH_DIR_NAME_LEN);

  snprintf(m_active_file_name, length, "%s%c%s", dir_name, OS_PATH_SEPARATOR,
           ARCH_PAGE_GROUP_ACTIVE_FILE_NAME);

  return (DB_SUCCESS);
}

dberr_t Arch_Group::build_durable_file_name() {
  char dir_name[MAX_ARCH_DIR_NAME_LEN];
  auto length = MAX_ARCH_DIR_NAME_LEN + 1 + MAX_ARCH_PAGE_FILE_NAME_LEN + 1;

  if (m_durable_file_name != nullptr) {
    return (DB_SUCCESS);
  }

  m_durable_file_name = static_cast<char *>(
      ut::malloc_withkey(ut::make_psi_memory_key(mem_key_archive), length));

  if (m_durable_file_name == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  get_dir_name(dir_name, MAX_ARCH_DIR_NAME_LEN);

  snprintf(m_durable_file_name, length, "%s%c%s", dir_name, OS_PATH_SEPARATOR,
           ARCH_PAGE_GROUP_DURABLE_FILE_NAME);

  return (DB_SUCCESS);
}

int Arch_Group::mark_active() {
  dberr_t db_err = build_active_file_name();

  if (db_err != DB_SUCCESS) {
    return (ER_OUTOFMEMORY);
  }

  os_file_create_t option;
  os_file_type_t type;

  bool success;
  bool exists;

  success = os_file_status(m_active_file_name, &exists, &type);

  if (!success) {
    return (ER_CANT_OPEN_FILE);
  }

  ut_ad(!exists);
  option = OS_FILE_CREATE_PATH;

  ut_ad(m_active_file.m_file == OS_FILE_CLOSED);

  m_active_file =
      os_file_create(innodb_arch_file_key, m_active_file_name, option,
                     OS_FILE_NORMAL, OS_CLONE_LOG_FILE, false, &success);

  int err = (success ? 0 : ER_CANT_OPEN_FILE);

  return (err);
}

int Arch_Group::mark_durable() {
  dberr_t db_err = build_durable_file_name();

  if (db_err != DB_SUCCESS) {
    return (ER_OUTOFMEMORY);
  }

  os_file_create_t option;
  os_file_type_t type;

  bool success;
  bool exists;

  success = os_file_status(m_durable_file_name, &exists, &type);

  if (exists) {
    return (0);
  }

  if (!success) {
    return (ER_CANT_OPEN_FILE);
  }

  option = OS_FILE_CREATE_PATH;

  ut_ad(m_durable_file.m_file == OS_FILE_CLOSED);

  m_durable_file =
      os_file_create(innodb_arch_file_key, m_durable_file_name, option,
                     OS_FILE_NORMAL, OS_CLONE_LOG_FILE, false, &success);

  int err = (success ? 0 : ER_CANT_OPEN_FILE);

  return (err);
}

int Arch_Group::mark_inactive() {
  os_file_type_t type;

  bool success;
  bool exists;

  dberr_t db_err;

  db_err = build_active_file_name();

  if (db_err != DB_SUCCESS) {
    return (ER_OUTOFMEMORY);
  }

  success = os_file_status(m_active_file_name, &exists, &type);

  if (!success) {
    return (ER_CANT_OPEN_FILE);
  }

  if (!exists) {
    return (0);
  }

  if (m_active_file.m_file != OS_FILE_CLOSED) {
    os_file_close(m_active_file);
    m_active_file.m_file = OS_FILE_CLOSED;
  }

  success = os_file_delete(innodb_arch_file_key, m_active_file_name);

  int err = (success ? 0 : ER_CANT_OPEN_FILE);

  return (err);
}

dberr_t Arch_Group::write_file_header(byte *from_buffer, uint length) {
  dberr_t err;

  ut_ad(!m_file_ctx.is_closed());

  /* Write to the doublewrite buffer before writing to the actual file */
  Arch_Group::write_to_doublewrite_file(nullptr, from_buffer, length,
                                        ARCH_PAGE_DBLWR_RESET_PAGE);

  DBUG_EXECUTE_IF("crash_after_reset_block_dblwr_flush", DBUG_SUICIDE(););

  err = m_file_ctx.write(nullptr, from_buffer, 0, length);

  if (err == DB_SUCCESS) {
    /* Flush the file to make sure the changes are made persistent as there
    would be no way to recover the data otherwise in case of a crash. */
    m_file_ctx.flush();
  }

  return (err);
}

dberr_t Arch_Group::open_file(Arch_Page_Pos write_pos, bool create_new) {
  dberr_t err;

  ut_d(auto count = get_file_count());
  ut_ad(count > 0);

  ut_a(m_file_size == ARCH_PAGE_BLK_SIZE * ARCH_PAGE_FILE_CAPACITY);

  if (!create_new) {
    auto block_num = write_pos.m_block_num;
    uint file_index = Arch_Block::get_file_index(block_num, ARCH_DATA_BLOCK);
    uint offset = Arch_Block::get_file_offset(block_num, ARCH_DATA_BLOCK);

    ut_ad(file_index + 1 == count);

    err = m_file_ctx.open(false, m_begin_lsn, file_index, offset, m_file_size);
  } else {
    err = m_file_ctx.open_new(m_begin_lsn, m_file_size, m_header_len);
  }

  return err;
}

void Arch_File_Ctx::update_stop_point(uint file_index, lsn_t stop_lsn) {
  auto last_point_index = m_stop_points.size() - 1;

  if (!m_stop_points.size() || last_point_index != file_index) {
    m_stop_points.push_back(stop_lsn);
  } else {
    m_stop_points[last_point_index] = stop_lsn;
  }
}

void Arch_File_Ctx::save_reset_point_in_mem(lsn_t lsn, Arch_Page_Pos pos) {
  uint current_file_index =
      Arch_Block::get_file_index(pos.m_block_num, ARCH_DATA_BLOCK);

  Arch_Point reset_point;
  reset_point.lsn = lsn;
  reset_point.pos = pos;

  Arch_Reset_File reset_file;

  if (m_reset.size()) {
    reset_file = m_reset.back();

    if (reset_file.m_file_index == current_file_index) {
      reset_file.m_start_point.push_back(reset_point);
      m_reset[m_reset.size() - 1] = reset_file;
      return;
    }
  }

  /* Reset info maintained in a new file. */
  reset_file.init();
  reset_file.m_file_index = current_file_index;
  reset_file.m_lsn = lsn;
  reset_file.m_start_point.push_back(reset_point);
  m_reset.push_back(reset_file);
}

bool Arch_File_Ctx::find_reset_point(lsn_t check_lsn, Arch_Point &reset_point) {
  if (!m_reset.size()) {
    return (false);
  }

  Arch_Reset_File file_reset_compare;
  file_reset_compare.m_lsn = check_lsn;

  /* Finds the file which has the element that is >= to check_lsn */
  auto reset_it = std::lower_bound(
      m_reset.begin(), m_reset.end(), file_reset_compare,
      [](const Arch_Reset_File &lhs, const Arch_Reset_File &rhs) {
        return (lhs.m_lsn < rhs.m_lsn);
      });

  if (reset_it != m_reset.end() && reset_it->m_lsn == check_lsn) {
    reset_point = reset_it->m_start_point.front();
    return (true);
  }

  if (reset_it == m_reset.begin()) {
    return (false);
  }

  /* The element that is less than check_lsn, which we're interested in,
  will be in the previous position. */
  --reset_it;
  ut_ad(reset_it->m_lsn < check_lsn);

  auto reset_file = *reset_it;
  auto reset_start_point = reset_file.m_start_point;

  Arch_Point reset_point_compare;
  reset_point_compare.lsn = check_lsn;

  /* Find the first start point whose lsn is >= to check_lsn. */
  auto reset_point_it = std::lower_bound(
      reset_start_point.begin(), reset_start_point.end(), reset_point_compare,
      [](const Arch_Point &lhs, const Arch_Point &rhs) {
        return (lhs.lsn < rhs.lsn);
      });

  if (reset_point_it == reset_start_point.end() ||
      reset_point_it->lsn != check_lsn) {
    ut_ad(reset_point_it != reset_start_point.begin());
    --reset_point_it;
  }

  reset_point = *reset_point_it;

  return (true);
}

dberr_t Arch_File_Ctx::write(Arch_File_Ctx *from_file, byte *from_buffer,
                             uint offset, uint size) {
  dberr_t err;

  ut_ad(offset + size <= m_size);
  ut_ad(!is_closed());

  if (from_buffer == nullptr) {
    ut_ad(offset + size <= from_file->get_size());
    ut_ad(!from_file->is_closed());

    err = os_file_copy(from_file->m_file, offset, m_file, offset, size);
  } else {
    IORequest request(IORequest::WRITE);
    request.disable_compression();
    request.clear_encrypted();

    err = os_file_write(request, "Page Track File", m_file, from_buffer, offset,
                        size);
  }

  return (err);
}

bool Arch_File_Ctx::find_stop_point(Arch_Group *group, lsn_t check_lsn,
                                    Arch_Point &stop_point,
                                    Arch_Page_Pos last_pos) {
  stop_point.lsn = LSN_MAX;
  stop_point.pos.init();

  arch_page_sys->arch_oper_mutex_enter();

  if (!m_stop_points.size()) {
    arch_page_sys->arch_oper_mutex_exit();
    return (false);
  }

  ut_ad(m_stop_points.back() <= arch_page_sys->get_latest_stop_lsn());

  /* 1. Find the file where the block we need to stop at is present */

  uint file_index = 0;

  for (uint i = 0; i < m_stop_points.size(); ++i) {
    file_index = i;

    if (m_stop_points[i] >= check_lsn) {
      break;
    }
  }

  ut_ad((m_stop_points[file_index] >= check_lsn &&
         (file_index == 0 || m_stop_points[file_index - 1] < check_lsn)));

  arch_page_sys->arch_oper_mutex_exit();

  /* 2. Find the block in the file where to stop. */

  byte header_buf[ARCH_PAGE_BLK_HEADER_LENGTH];

  Arch_Page_Pos left_pos;
  left_pos.m_block_num = ARCH_PAGE_FILE_DATA_CAPACITY * file_index;

  Arch_Page_Pos right_pos;

  if (file_index < m_stop_points.size() - 1) {
    right_pos.m_block_num =
        left_pos.m_block_num + ARCH_PAGE_FILE_DATA_CAPACITY - 1;
  } else {
    right_pos.m_block_num = last_pos.m_block_num;
  }

  lsn_t block_stop_lsn;
  int err;

  while (left_pos.m_block_num <= right_pos.m_block_num) {
    Arch_Page_Pos middle_pos;
    middle_pos.init();
    middle_pos.m_offset = 0;

    middle_pos.m_block_num = left_pos.m_block_num +
                             (right_pos.m_block_num - left_pos.m_block_num) / 2;

    /* Read the block header for data length and stop lsn info. */
    err = group->read_data(middle_pos, header_buf, ARCH_PAGE_BLK_HEADER_LENGTH);

    if (err != 0) {
      return (false);
    }

    block_stop_lsn = Arch_Block::get_stop_lsn(header_buf);
    auto data_len = Arch_Block::get_data_len(header_buf);

    middle_pos.m_offset = data_len + ARCH_PAGE_BLK_HEADER_LENGTH;

    if (block_stop_lsn >= check_lsn) {
      stop_point.lsn = block_stop_lsn;
      stop_point.pos = middle_pos;
    }

    if (left_pos.m_block_num == right_pos.m_block_num ||
        block_stop_lsn == check_lsn) {
      break;
    }

    if (block_stop_lsn > check_lsn) {
      right_pos.m_block_num = middle_pos.m_block_num - 1;
    } else {
      left_pos.m_block_num = middle_pos.m_block_num + 1;
    }
  }

  ut_ad(stop_point.lsn != LSN_MAX);

  return (true);
}

#ifdef UNIV_DEBUG

bool Arch_File_Ctx::validate_stop_point_in_file(Arch_Group *group,
                                                pfs_os_file_t file,
                                                uint file_index) {
  lsn_t stop_lsn = LSN_MAX;
  bool last_file = file_index + 1 == m_count;

  if (last_file && group->is_active() && group->get_end_lsn() == LSN_MAX) {
    /* Just return true if this is the case as the block might not have been
    flushed to disk yet */
    return (true);
  }

  if (file_index >= m_stop_points.size()) {
    ut_error;
  }

  /* Read from file to the user buffer. */
  IORequest request(IORequest::READ);
  request.disable_compression();
  request.clear_encrypted();

  uint64_t offset;

  if (!last_file) {
    offset = ARCH_PAGE_FILE_DATA_CAPACITY * ARCH_PAGE_BLK_SIZE;
  } else {
    offset = Arch_Block::get_file_offset(group->get_stop_pos().m_block_num,
                                         ARCH_DATA_BLOCK);
  }

  byte buf[ARCH_PAGE_BLK_SIZE];

  /* Read the entire reset block. */
  dberr_t err =
      os_file_read(request, m_path_name, file, buf, offset, ARCH_PAGE_BLK_SIZE);

  if (err != DB_SUCCESS) {
    return (false);
  }

  stop_lsn = Arch_Block::get_stop_lsn(buf);

  if (stop_lsn != m_stop_points[file_index]) {
    ut_error;
  }

  DBUG_PRINT("page_archiver", ("File stop point: %" PRIu64 "", stop_lsn));

  return (true);
}

bool Arch_File_Ctx::validate_reset_block_in_file(pfs_os_file_t file,
                                                 uint file_index,
                                                 uint &reset_count) {
  /* Read from file to the user buffer. */
  IORequest request(IORequest::READ);
  request.disable_compression();
  request.clear_encrypted();

  byte buf[ARCH_PAGE_BLK_SIZE];

  /* Read the entire reset block. */
  dberr_t err =
      os_file_read(request, m_path_name, file, buf, 0, ARCH_PAGE_BLK_SIZE);

  if (err != DB_SUCCESS) {
    return (false);
  }

  auto data_length = Arch_Block::get_data_len(buf);

  if (data_length == 0) {
    /* No reset, move to the next file. */
    return (true);
  }

  ut_ad(data_length >= ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE +
                           ARCH_PAGE_FILE_HEADER_RESET_POS_SIZE);

  Arch_Reset_File reset_file;

  if (!m_reset.size() || reset_count >= m_reset.size()) {
    ut_error;
  }

  reset_file = m_reset.at(reset_count);

  if (reset_file.m_file_index != file_index) {
    ut_error;
  }

  byte *block_data = buf + ARCH_PAGE_BLK_HEADER_LENGTH;

  lsn_t file_reset_lsn = mach_read_from_8(block_data);
  uint length = ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE;

  if (reset_file.m_lsn != file_reset_lsn) {
    ut_error;
  }

  DBUG_PRINT("page_archiver", ("File lsn : %" PRIu64 "", file_reset_lsn));

  uint index = 0;
  Arch_Point start_point;

  while (length < data_length) {
    if (index >= reset_file.m_start_point.size()) {
      ut_error;
    }

    start_point = reset_file.m_start_point.at(index);

    uint64_t block_num = mach_read_from_2(block_data + length);
    length += ARCH_PAGE_FILE_HEADER_RESET_BLOCK_NUM_SIZE;

    uint64_t block_offset = mach_read_from_2(block_data + length);
    length += ARCH_PAGE_FILE_HEADER_RESET_BLOCK_OFFSET_SIZE;

    if (block_num != start_point.pos.m_block_num ||
        block_offset != start_point.pos.m_offset) {
      ut_error;
    }

    DBUG_PRINT("page_archiver",
               ("Reset point %u : %" PRIu64 ", %" PRIu64 ", %" PRIu64 "", index,
                start_point.lsn, block_num, block_offset));

    ++index;
  }

  ut_ad(length == data_length);

  if (reset_file.m_start_point.size() != index) {
    ut_error;
  }

  ++reset_count;

  return (true);
}

bool Arch_Group::validate_info_in_files() {
  uint reset_count = 0;
  uint file_count = m_file_ctx.get_count();
  bool success = true;

  DBUG_PRINT("page_archiver", ("RESET PAGE"));

  for (uint file_index = 0; file_index < file_count; ++file_index) {
    bool last_file = file_index + 1 == file_count;

    if (last_file && m_file_ctx.get_phy_size() == 0) {
      success = false;
      break;
    }

    success = m_file_ctx.validate(this, file_index, m_begin_lsn, reset_count);

    if (!success) {
      break;
    }
  }

  DBUG_PRINT("page_archiver", ("\n"));

  return (success);
}

bool Arch_File_Ctx::validate(Arch_Group *group, uint file_index,
                             lsn_t start_lsn, uint &reset_count) {
  char file_name[MAX_ARCH_PAGE_FILE_NAME_LEN];

  build_name(file_index, start_lsn, file_name, MAX_ARCH_PAGE_FILE_NAME_LEN);

  if (!os_file_exists(file_name)) {
    /* Could be the case if files are purged. */
    return (true);
  }

  bool success;
  pfs_os_file_t file;

  file = os_file_create(innodb_arch_file_key, file_name, OS_FILE_OPEN,
                        OS_FILE_NORMAL, OS_CLONE_LOG_FILE, true, &success);

  if (!success) {
    return (false);
  }

  DBUG_PRINT("page_archiver", ("File : %u", file_index));

  success = validate_reset_block_in_file(file, file_index, reset_count);

  ut_ad(success);
  if (!success) {
    if (file.m_file != OS_FILE_CLOSED) {
      os_file_close(file);
    }
    return (false);
  }

  success = validate_stop_point_in_file(group, file, file_index);

  if (file.m_file != OS_FILE_CLOSED) {
    os_file_close(file);
  }

  if (!success ||
      (file_index + 1 == m_count && reset_count != m_reset.size())) {
    ut_error;
  }

  return (true);
}

#endif /* UNIV_DEBUG */

lsn_t Arch_File_Ctx::purge(lsn_t begin_lsn, lsn_t end_lsn, lsn_t purge_lsn) {
  Arch_Point reset_point;

  /* Find reset lsn which is <= purge_lsn. */
  auto success = find_reset_point(purge_lsn, reset_point);

  if (!success || reset_point.lsn == begin_lsn) {
    ib::info(ER_IB_MSG_PAGE_ARCH_NO_RESET_POINTS);
    return (LSN_MAX);
  }

  ut_ad(begin_lsn < reset_point.lsn && reset_point.lsn <= end_lsn);

  Arch_Reset_File file_reset_compare;
  file_reset_compare.m_lsn = reset_point.lsn;

  /* Finds the file which has the element that is >= to reset_point.lsn. */
  auto reset_file_it = std::lower_bound(
      m_reset.begin(), m_reset.end(), file_reset_compare,
      [](const Arch_Reset_File &lhs, const Arch_Reset_File &rhs) {
        return (lhs.m_lsn < rhs.m_lsn);
      });

  /* The element that is less than check_lsn, which we're interested in,
  will be in the previous position. */
  if (reset_file_it != m_reset.begin() &&
      (reset_file_it == m_reset.end() || reset_file_it->m_lsn != purge_lsn)) {
    --reset_file_it;
  }

  if (reset_file_it == m_reset.begin()) {
    return (LSN_MAX);
  }

  lsn_t purged_lsn = reset_file_it->m_lsn;

  for (auto it = m_reset.begin(); it != reset_file_it;) {
    bool success = delete_file(it->m_file_index, begin_lsn);

    if (success) {
      /** Removes the deleted file from reset info, thereby incrementing the
       iterator. */
      it = m_reset.erase(it);
    } else {
      purged_lsn = it->m_lsn;
      reset_file_it = it;
      ut_d(ut_error);
      ut_o(break);
    }
  }

  /** Only files which have a reset would be purged in the above loop. We want
  to purge all the files preceding reset_file_it regardless of whether it has
  a reset or not. */
  for (uint file_index = 0; file_index < reset_file_it->m_file_index;
       ++file_index) {
    delete_file(file_index, begin_lsn);
  }

  return (purged_lsn);
}

uint Arch_Group::purge(lsn_t purge_lsn, lsn_t &group_purged_lsn) {
  ut_ad(mutex_own(m_arch_mutex));

  if (m_begin_lsn > purge_lsn) {
    group_purged_lsn = LSN_MAX;
    return (0);
  }

  /** For a group (active or non-active) if there are any non-durable clients
  attached then we don't purge the group at all. */
  if (m_ref_count > 0) {
    group_purged_lsn = LSN_MAX;
    return (ER_PAGE_TRACKING_CANNOT_PURGE);
  }

  if (!m_is_active && m_end_lsn <= purge_lsn) {
    m_file_ctx.delete_files(m_begin_lsn);
    group_purged_lsn = m_end_lsn;
    return (0);
  }

  lsn_t purged_lsn = m_file_ctx.purge(m_begin_lsn, m_end_lsn, purge_lsn);

  group_purged_lsn = purged_lsn;

  return (0);
}

#ifdef UNIV_DEBUG
void Page_Arch_Client_Ctx::print() {
  DBUG_PRINT("page_archiver", ("CLIENT INFO"));
  DBUG_PRINT("page_archiver", ("Transient Client - %u", !m_is_durable));
  DBUG_PRINT("page_archiver", ("Start LSN - %" PRIu64 "", m_start_lsn));
  DBUG_PRINT("page_archiver", ("Stop LSN - %" PRIu64 "", m_stop_lsn));
  DBUG_PRINT("page_archiver",
             ("Last Reset LSN - %" PRIu64 "", m_last_reset_lsn));
  DBUG_PRINT("page_archiver", ("Start pos - %" PRIu64 ", %u",
                               m_start_pos.m_block_num, m_start_pos.m_offset));
  DBUG_PRINT("page_archiver", ("Stop pos - %" PRIu64 ", %u\n",
                               m_stop_pos.m_block_num, m_stop_pos.m_offset));
}
#endif

int Page_Arch_Client_Ctx::start(bool recovery, uint64_t *start_id) {
  bool reset = false;
  int err = 0;

  arch_client_mutex_enter();

  switch (m_state) {
    case ARCH_CLIENT_STATE_STOPPED:
      if (!m_is_durable) {
        arch_client_mutex_exit();
        return (ER_PAGE_TRACKING_NOT_STARTED);
      }
      DBUG_PRINT("page_archiver", ("Archiver in progress"));
      DBUG_PRINT("page_archiver", ("[->] Starting page archiving."));
      break;

    case ARCH_CLIENT_STATE_INIT:
      DBUG_PRINT("page_archiver", ("Archiver in progress"));
      DBUG_PRINT("page_archiver", ("[->] Starting page archiving."));
      break;

    case ARCH_CLIENT_STATE_STARTED:
      DBUG_PRINT("page_archiver", ("[->] Resetting page archiving."));
      ut_ad(m_group != nullptr);
      reset = true;
      break;

    default:
      ut_d(ut_error);
  }

  /* Start archiving. */
  err = arch_page_sys->start(&m_group, &m_last_reset_lsn, &m_start_pos,
                             m_is_durable, reset, recovery);

  if (err != 0) {
    arch_client_mutex_exit();
    return (err);
  }

  if (!reset) {
    m_start_lsn = m_last_reset_lsn;
  }

  if (start_id != nullptr) {
    *start_id = m_last_reset_lsn;
  }

  if (!is_active()) {
    m_state = ARCH_CLIENT_STATE_STARTED;
  }

  arch_client_mutex_exit();

  if (!m_is_durable) {
    /* Update DD table buffer to get rid of recovery dependency for auto INC */
    dict_persist_to_dd_table_buffer();

    /* Make sure all written pages are synced to disk. */
    fil_flush_file_spaces();

    ib::info(ER_IB_MSG_20) << "Clone Start PAGE ARCH : start LSN : "
                           << m_start_lsn << ", checkpoint LSN : "
                           << log_get_checkpoint_lsn(*log_sys);
  }

  return (err);
}

int Page_Arch_Client_Ctx::init_during_recovery(Arch_Group *group,
                                               lsn_t last_lsn) {
  /* Initialise the sys client */
  m_state = ARCH_CLIENT_STATE_STARTED;
  m_group = group;
  m_start_lsn = group->get_begin_lsn();
  m_last_reset_lsn = last_lsn;
  m_start_pos.init();

  /* Start page archiving. */
  int error = start(true, nullptr);

  ut_d(print());

  return (error);
}

int Page_Arch_Client_Ctx::stop(lsn_t *stop_id) {
  arch_client_mutex_enter();

  if (!is_active()) {
    arch_client_mutex_exit();
    ib::error(ER_PAGE_TRACKING_NOT_STARTED);
    return (ER_PAGE_TRACKING_NOT_STARTED);
  }

  ut_ad(m_group != nullptr);

  /* Stop archiving. */
  auto err =
      arch_page_sys->stop(m_group, &m_stop_lsn, &m_stop_pos, m_is_durable);

  ut_d(print());

  /* We stop the client even in cases of an error. */
  m_state = ARCH_CLIENT_STATE_STOPPED;

  if (stop_id != nullptr) {
    *stop_id = m_stop_lsn;
  }

  arch_client_mutex_exit();

  ib::info(ER_IB_MSG_21) << "Clone Stop  PAGE ARCH : end   LSN : " << m_stop_lsn
                         << ", log sys LSN : " << log_get_lsn(*log_sys);

  return (err);
}

int Page_Arch_Client_Ctx::get_pages(Page_Arch_Cbk *cbk_func, void *cbk_ctx,
                                    byte *buff, uint buf_len) {
  int err = 0;
  uint num_pages;
  uint read_len;

  arch_client_mutex_enter();

  ut_ad(m_state == ARCH_CLIENT_STATE_STOPPED);

  auto cur_pos = m_start_pos;

  while (true) {
    ut_ad(cur_pos.m_block_num <= m_stop_pos.m_block_num);

    /* Check if last block */
    if (cur_pos.m_block_num >= m_stop_pos.m_block_num) {
      if (cur_pos.m_offset > m_stop_pos.m_offset) {
        my_error(ER_INTERNAL_ERROR, MYF(0), "Wrong Archiver page offset");
        err = ER_INTERNAL_ERROR;
        ut_d(ut_error);
        ut_o(break);
      }

      read_len = m_stop_pos.m_offset - cur_pos.m_offset;

      if (read_len == 0) {
        break;
      }

    } else {
      if (cur_pos.m_offset > ARCH_PAGE_BLK_SIZE) {
        my_error(ER_INTERNAL_ERROR, MYF(0), "Wrong Archiver page offset");
        err = ER_INTERNAL_ERROR;
        ut_d(ut_error);
        ut_o(break);
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

    err = m_group->read_data(cur_pos, buff, read_len);

    if (err != 0) {
      break;
    }

    cur_pos.m_offset += read_len;
    num_pages = read_len / ARCH_BLK_PAGE_ID_SIZE;

    err = cbk_func(cbk_ctx, buff, num_pages);

    if (err != 0) {
      break;
    }
  }

  arch_client_mutex_exit();

  return (err);
}

void Page_Arch_Client_Ctx::release() {
  arch_client_mutex_enter();

  switch (m_state) {
    case ARCH_CLIENT_STATE_INIT:
      arch_client_mutex_exit();
      return;

    case ARCH_CLIENT_STATE_STARTED:
      arch_client_mutex_exit();
      stop(nullptr);
      break;

    case ARCH_CLIENT_STATE_STOPPED:
      break;

    default:
      ut_d(ut_error);
  }

  ut_ad(m_group != nullptr);

  arch_page_sys->release(m_group, m_is_durable, m_start_pos);

  m_state = ARCH_CLIENT_STATE_INIT;
  m_group = nullptr;
  m_start_lsn = LSN_MAX;
  m_stop_lsn = LSN_MAX;
  m_last_reset_lsn = LSN_MAX;
  m_start_pos.init();
  m_stop_pos.init();

  arch_client_mutex_exit();
}

bool wait_flush_archiver(Page_Wait_Flush_Archiver_Cbk cbk_func) {
  ut_ad(mutex_own(arch_page_sys->get_oper_mutex()));

  while (cbk_func()) {
    /* Need to wait for flush. We don't expect it
    to happen normally. With no duplicate page ID
    dirty page growth should be very slow. */
    os_event_set(page_archiver_thread_event);

    bool is_timeout = false;
    int alert_count = 0;

    auto err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(arch_page_sys->get_oper_mutex()));
          result = cbk_func();

          int err2 = 0;
          if (srv_shutdown_state.load() == SRV_SHUTDOWN_LAST_PHASE ||
              srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS ||
              arch_page_sys->is_abort()) {
            err2 = ER_QUERY_INTERRUPTED;

          } else if (result) {
            os_event_set(page_archiver_thread_event);
            if (alert && ++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_MSG_22) << "Clone Page Tracking: waiting "
                                        "for block to flush";
            }
          }
          return (err2);
        },
        arch_page_sys->get_oper_mutex(), is_timeout);

    if (err != 0) {
      return (false);

    } else if (is_timeout) {
      ib::warn(ER_IB_MSG_22) << "Clone Page Tracking: wait for block flush "
                                "timed out";
      ut_d(ut_error);
      ut_o(return false);
    }
  }
  return (true);
}

uint Arch_Block::get_file_index(uint64_t block_num, Arch_Blk_Type type) {
  size_t file_index = std::numeric_limits<size_t>::max();

  switch (type) {
    case ARCH_RESET_BLOCK:
      file_index = block_num;
      break;

    case ARCH_DATA_BLOCK:
      file_index = block_num / ARCH_PAGE_FILE_DATA_CAPACITY;
      break;

    default:
      ut_d(ut_error);
  }

  return file_index;
}

Arch_Blk_Type Arch_Block::get_type(byte *block) {
  return static_cast<Arch_Blk_Type>(
      mach_read_from_1(block + ARCH_PAGE_BLK_HEADER_TYPE_OFFSET));
}

uint Arch_Block::get_data_len(byte *block) {
  return (mach_read_from_2(block + ARCH_PAGE_BLK_HEADER_DATA_LEN_OFFSET));
}

lsn_t Arch_Block::get_stop_lsn(byte *block) {
  return (mach_read_from_8(block + ARCH_PAGE_BLK_HEADER_STOP_LSN_OFFSET));
}

uint64_t Arch_Block::get_block_number(byte *block) {
  return (mach_read_from_8(block + ARCH_PAGE_BLK_HEADER_NUMBER_OFFSET));
}

lsn_t Arch_Block::get_reset_lsn(byte *block) {
  return (mach_read_from_8(block + ARCH_PAGE_BLK_HEADER_RESET_LSN_OFFSET));
}

uint32_t Arch_Block::get_checksum(byte *block) {
  return (mach_read_from_4(block + ARCH_PAGE_BLK_HEADER_CHECKSUM_OFFSET));
}

uint64_t Arch_Block::get_file_offset(uint64_t block_num, Arch_Blk_Type type) {
  uint64_t offset = 0;

  switch (type) {
    case ARCH_RESET_BLOCK:
      offset = 0;
      break;

    case ARCH_DATA_BLOCK:
      offset = block_num % ARCH_PAGE_FILE_DATA_CAPACITY;
      offset += ARCH_PAGE_FILE_NUM_RESET_PAGE;
      offset *= ARCH_PAGE_BLK_SIZE;
      break;

    default:
      ut_d(ut_error);
  }

  return offset;
}

bool Arch_Block::is_zeroes(const byte *block) {
  for (ulint i = 0; i < ARCH_PAGE_BLK_SIZE; i++) {
    if (block[i] != 0) {
      return (false);
    }
  }
  return (true);
}

bool Arch_Block::validate(byte *block) {
  auto data_length = Arch_Block::get_data_len(block);
  auto block_checksum = Arch_Block::get_checksum(block);
  auto checksum = ut_crc32(block + ARCH_PAGE_BLK_HEADER_LENGTH, data_length);

  if (checksum != block_checksum) {
    ib::warn(ER_IB_ERR_PAGE_ARCH_INVALID_DOUBLE_WRITE_BUF)
        << Arch_Block::get_block_number(block);
    ut_d(ut_error);
    ut_o(return (false));
  } else if (Arch_Block::is_zeroes(block)) {
    return (false);
  }

  return (true);
}

void Arch_Block::update_block_header(lsn_t stop_lsn, lsn_t reset_lsn) {
  mach_write_to_2(m_data + ARCH_PAGE_BLK_HEADER_DATA_LEN_OFFSET, m_data_len);

  if (stop_lsn != LSN_MAX) {
    m_stop_lsn = stop_lsn;
    mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_STOP_LSN_OFFSET, m_stop_lsn);
  }

  if (reset_lsn != LSN_MAX) {
    m_reset_lsn = reset_lsn;
    mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_RESET_LSN_OFFSET,
                    m_reset_lsn);
  }
}

/** Set the block ready to begin writing page ID
@param[in]      pos     position to initiate block number */
void Arch_Block::begin_write(Arch_Page_Pos pos) {
  m_data_len = 0;

  m_state = ARCH_BLOCK_ACTIVE;

  m_number =
      (m_type == ARCH_DATA_BLOCK
           ? pos.m_block_num
           : Arch_Block::get_file_index(pos.m_block_num, ARCH_DATA_BLOCK));

  m_oldest_lsn = LSN_MAX;
  m_reset_lsn = LSN_MAX;

  if (m_type == ARCH_DATA_BLOCK) {
    arch_page_sys->update_stop_info(this);
  }
}

/** End writing to a block.
Change state to #ARCH_BLOCK_READY_TO_FLUSH */
void Arch_Block::end_write() { m_state = ARCH_BLOCK_READY_TO_FLUSH; }

/** Add page ID to current block
@param[in]      page    page from buffer pool
@param[in]      pos     Archiver current position
@return true, if successful
        false, if no more space in current block */
bool Arch_Block::add_page(buf_page_t *page, Arch_Page_Pos *pos) {
  space_id_t space_id;
  page_no_t page_num;
  byte *data_ptr;

  ut_ad(pos->m_offset <= ARCH_PAGE_BLK_SIZE);
  ut_ad(m_type == ARCH_DATA_BLOCK);
  ut_ad(pos->m_offset == m_data_len + ARCH_PAGE_BLK_HEADER_LENGTH);

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
  if (arch_page_sys->get_latest_stop_lsn() > m_oldest_lsn ||
      m_oldest_lsn > page->get_oldest_lsn()) {
    m_oldest_lsn = page->get_oldest_lsn();
  }

  return (true);
}

bool Arch_Block::get_data(Arch_Page_Pos *read_pos, uint read_len,
                          byte *read_buff) {
  ut_ad(read_pos->m_offset + read_len <= m_size);

  if (m_state == ARCH_BLOCK_INIT || m_number != read_pos->m_block_num) {
    /* The block is already overwritten. */
    return (false);
  }

  byte *src = m_data + read_pos->m_offset;

  memcpy(read_buff, src, read_len);

  return (true);
}

bool Arch_Block::set_data(uint read_len, byte *read_buff, uint read_offset) {
  ut_ad(m_state != ARCH_BLOCK_INIT);
  ut_ad(read_offset + read_len <= m_size);

  byte *dest = m_data + read_offset;

  memcpy(dest, read_buff, read_len);

  set_reset_lsn(Arch_Block::get_reset_lsn(m_data));

  return true;
}

/** Flush this block to the file group.
@param[in]      file_group      current archive group
@param[in]      type            flush type
@return error code. */
dberr_t Arch_Block::flush(Arch_Group *file_group, Arch_Blk_Flush_Type type) {
  dberr_t err = DB_SUCCESS;
  uint32_t checksum;

  checksum = ut_crc32(m_data + ARCH_PAGE_BLK_HEADER_LENGTH, m_data_len);

  /* Update block's header. */
  mach_write_to_1(m_data + ARCH_PAGE_BLK_HEADER_VERSION_OFFSET,
                  ARCH_PAGE_FILE_VERSION);
  mach_write_to_1(m_data + ARCH_PAGE_BLK_HEADER_TYPE_OFFSET, m_type);
  mach_write_to_2(m_data + ARCH_PAGE_BLK_HEADER_DATA_LEN_OFFSET, m_data_len);
  mach_write_to_4(m_data + ARCH_PAGE_BLK_HEADER_CHECKSUM_OFFSET, checksum);
  mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_STOP_LSN_OFFSET, m_stop_lsn);
  mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_RESET_LSN_OFFSET, m_reset_lsn);
  mach_write_to_8(m_data + ARCH_PAGE_BLK_HEADER_NUMBER_OFFSET, m_number);

  switch (m_type) {
    case ARCH_RESET_BLOCK:
      err = file_group->write_file_header(m_data, m_size);
      break;

    case ARCH_DATA_BLOCK: {
      bool is_partial_flush = (type == ARCH_FLUSH_PARTIAL);

      /* Callback responsible for setting up file's header starting at offset 0.
      This header is left empty within this flush operation. */
      auto get_empty_file_header_cbk = [](uint64_t, byte *) {
        return DB_SUCCESS;
      };

      /* We allow partial flush to happen even if there were no pages added
      since the last partial flush as the block's header might contain some
      useful info required during recovery. */
      err = file_group->write_to_file(nullptr, m_data, m_size, is_partial_flush,
                                      true, get_empty_file_header_cbk);
      break;
    }

    default:
      ut_d(ut_error);
  }

  return (err);
}

void Arch_Block::add_reset(lsn_t reset_lsn, Arch_Page_Pos reset_pos) {
  ut_ad(m_type == ARCH_RESET_BLOCK);
  ut_ad(m_data_len <= ARCH_PAGE_BLK_SIZE);
  ut_ad(m_data_len + ARCH_PAGE_FILE_HEADER_RESET_POS_SIZE <=
        ARCH_PAGE_BLK_SIZE);

  byte *buf = m_data + ARCH_PAGE_BLK_HEADER_LENGTH;

  if (m_data_len == 0) {
    /* Write file lsn. */

    mach_write_to_8(buf, reset_lsn);
    m_data_len += ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE;
  }

  ut_ad(m_data_len >= ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE);

  mach_write_to_2(buf + m_data_len, reset_pos.m_block_num);
  m_data_len += ARCH_PAGE_FILE_HEADER_RESET_BLOCK_NUM_SIZE;

  mach_write_to_2(buf + m_data_len, reset_pos.m_offset);
  m_data_len += ARCH_PAGE_FILE_HEADER_RESET_BLOCK_OFFSET_SIZE;
}

void Arch_Block::copy_data(const Arch_Block *block) {
  m_data_len = block->m_data_len;
  m_size = block->m_size;
  m_state = block->m_state;
  m_number = block->m_number;
  m_type = block->m_type;
  m_stop_lsn = block->m_stop_lsn;
  m_reset_lsn = block->m_reset_lsn;
  m_oldest_lsn = block->m_oldest_lsn;
  set_data(m_size, block->m_data, 0);
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
  m_num_data_blocks = ARCH_PAGE_NUM_BLKS;

  /* block size and number must be in power of 2 */
  ut_ad(ut_is_2pow(m_block_size));
  ut_ad(ut_is_2pow(m_num_data_blocks));

  alloc_size = m_block_size * m_num_data_blocks;

  /* For reset block. */
  alloc_size += m_block_size;

  /* For partial flush block. */
  alloc_size += m_block_size;

  /* Allocate buffer for memory blocks. */
  m_buffer = static_cast<byte *>(ut::aligned_zalloc_withkey(
      ut::make_psi_memory_key(mem_key_archive), alloc_size, m_block_size));

  if (m_buffer == nullptr) {
    return (false);
  }
  mem_ptr = m_buffer;

  Arch_Block *cur_blk;

  /* Create memory blocks. */
  for (index = 0; index < m_num_data_blocks; index++) {
    cur_blk =
        ut::new_withkey<Arch_Block>(ut::make_psi_memory_key(mem_key_archive),
                                    mem_ptr, m_block_size, ARCH_DATA_BLOCK);

    if (cur_blk == nullptr) {
      return (false);
    }

    m_data_blocks.push_back(cur_blk);
    mem_ptr += m_block_size;
  }

  m_reset_block =
      ut::new_withkey<Arch_Block>(ut::make_psi_memory_key(mem_key_archive),
                                  mem_ptr, m_block_size, ARCH_RESET_BLOCK);

  if (m_reset_block == nullptr) {
    return (false);
  }

  mem_ptr += m_block_size;

  m_partial_flush_block =
      ut::new_withkey<Arch_Block>(ut::make_psi_memory_key(mem_key_archive),
                                  mem_ptr, m_block_size, ARCH_DATA_BLOCK);

  if (m_partial_flush_block == nullptr) {
    return (false);
  }

  return (true);
}

/** Delete blocks and buffer */
void ArchPageData::clean() {
  for (auto &block : m_data_blocks) {
    ut::delete_(block);
    block = nullptr;
  }

  if (m_reset_block != nullptr) {
    ut::delete_(m_reset_block);
    m_reset_block = nullptr;
  }

  if (m_partial_flush_block != nullptr) {
    ut::delete_(m_partial_flush_block);
    m_partial_flush_block = nullptr;
  }

  ut::aligned_free(m_buffer);
}

/** Get the block for a position
@param[in]      pos     position in page archive sys
@param[in]      type    block type
@return page archive in memory block */
Arch_Block *ArchPageData::get_block(Arch_Page_Pos *pos, Arch_Blk_Type type) {
  switch (type) {
    case ARCH_DATA_BLOCK: {
      /* index = block_num % m_num_blocks */
      ut_ad(ut_is_2pow(m_num_data_blocks));

      uint index = pos->m_block_num & (m_num_data_blocks - 1);
      return (m_data_blocks[index]);
    }

    case ARCH_RESET_BLOCK:
      return (m_reset_block);

    default:
      ut_d(ut_error);
  }

  ut_o(return (nullptr));
}

Arch_Page_Sys::Arch_Page_Sys() {
  mutex_create(LATCH_ID_PAGE_ARCH, &m_mutex);
  mutex_create(LATCH_ID_PAGE_ARCH_OPER, &m_oper_mutex);

  m_ctx = ut::new_withkey<Page_Arch_Client_Ctx>(
      ut::make_psi_memory_key(mem_key_archive), true);

  DBUG_EXECUTE_IF("page_archiver_simulate_more_archived_files",
                  ARCH_PAGE_FILE_CAPACITY = 8;
                  ARCH_PAGE_FILE_DATA_CAPACITY =
                      ARCH_PAGE_FILE_CAPACITY - ARCH_PAGE_FILE_NUM_RESET_PAGE;);
}

Arch_Page_Sys::~Arch_Page_Sys() {
  ut_ad(m_state == ARCH_STATE_INIT || m_state == ARCH_STATE_ABORT ||
        m_state == ARCH_STATE_READ_ONLY);
  ut_ad(m_current_group == nullptr);

  for (auto group : m_group_list) {
    ut::delete_(group);
  }

  Arch_Group::shutdown();

  m_data.clean();

  ut::delete_(m_ctx);
  mutex_free(&m_mutex);
  mutex_free(&m_oper_mutex);
}

void Arch_Page_Sys::post_recovery_init() {
  if (!is_active()) {
    return;
  }

  arch_oper_mutex_enter();
  m_latest_stop_lsn = log_get_checkpoint_lsn(*log_sys);
  auto cur_block = m_data.get_block(&m_write_pos, ARCH_DATA_BLOCK);
  update_stop_info(cur_block);
  arch_oper_mutex_exit();
}

void Arch_Page_Sys::flush_at_checkpoint(lsn_t checkpoint_lsn) {
  arch_oper_mutex_enter();

  if (!is_active()) {
    arch_oper_mutex_exit();
    return;
  }

  lsn_t end_lsn = m_current_group->get_end_lsn();

  if (m_write_pos.m_offset == ARCH_PAGE_BLK_HEADER_LENGTH) {
    arch_oper_mutex_exit();
    return;
  }

  Arch_Page_Pos request_flush_pos;

  if (end_lsn == LSN_MAX) {
    Arch_Block *cur_block = m_data.get_block(&m_write_pos, ARCH_DATA_BLOCK);

    ut_ad(cur_block->get_state() == ARCH_BLOCK_ACTIVE);

    m_latest_stop_lsn = checkpoint_lsn;
    update_stop_info(cur_block);

    if (cur_block->get_oldest_lsn() != LSN_MAX &&
        cur_block->get_oldest_lsn() <= checkpoint_lsn) {
      /* If the oldest modified page in the block added since the last
      checkpoint was modified before the checkpoint_lsn then the block needs to
      be flushed*/

      request_flush_pos = m_write_pos;
    } else {
      /* Wait for blocks that are not active to be flushed. */

      if (m_write_pos.m_block_num == 0) {
        arch_oper_mutex_exit();
        return;
      }

      request_flush_pos.init();
      request_flush_pos.m_block_num = m_write_pos.m_block_num - 1;
    }

    if (request_flush_pos < m_flush_pos) {
      arch_oper_mutex_exit();
      return;
    }

    if (m_request_flush_pos < request_flush_pos) {
      m_request_flush_pos = request_flush_pos;
    }

  } else {
    request_flush_pos = m_current_group->get_stop_pos();
    m_request_flush_pos = request_flush_pos;
  }

  if (request_flush_pos.m_block_num == m_write_pos.m_block_num) {
    MONITOR_INC(MONITOR_PAGE_TRACK_CHECKPOINT_PARTIAL_FLUSH_REQUEST);
  }

  /* We need to ensure that blocks are flushed until request_flush_pos */
  auto cbk = [&] { return (request_flush_pos < m_flush_pos ? false : true); };

  if (!wait_flush_archiver(cbk)) {
    ib::warn(ER_IB_WRN_PAGE_ARCH_FLUSH_DATA);
  }

  arch_oper_mutex_exit();
}

/** Check and add page ID to archived data.
Check for duplicate page.
@param[in]      bpage           page to track
@param[in]      track_lsn       LSN when tracking started
@param[in]      frame_lsn       current LSN of the page
@param[in]      force           if true, add page ID without check */
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
      if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
        arch_oper_mutex_exit();
        return;
      }

      ib::warn(ER_IB_MSG_23) << "Fail to add page for tracking."
                             << " Space ID: " << bpage->id.space();

      m_state = ARCH_STATE_ABORT;
      arch_oper_mutex_exit();
      ut_d(ut_error);
      ut_o(return );
    }

    cur_blk = m_data.get_block(&m_write_pos, ARCH_DATA_BLOCK);

    if (cur_blk->get_state() == ARCH_BLOCK_ACTIVE) {
      if (cur_blk->add_page(bpage, &m_write_pos)) {
        /* page added successfully. */
        break;
      }

      /* Current block is full. Move to next block. */
      cur_blk->end_write();

      m_write_pos.set_next();

      /* Writing to a new file so move to the next reset block. */
      if (m_write_pos.m_block_num % ARCH_PAGE_FILE_DATA_CAPACITY == 0) {
        Arch_Block *reset_block =
            m_data.get_block(&m_reset_pos, ARCH_RESET_BLOCK);
        reset_block->end_write();

        m_reset_pos.set_next();
      }

      os_event_set(page_archiver_thread_event);

      ++count;
      continue;

    } else if (cur_blk->get_state() == ARCH_BLOCK_INIT ||
               cur_blk->get_state() == ARCH_BLOCK_FLUSHED) {
      ut_ad(m_write_pos.m_offset == ARCH_PAGE_BLK_HEADER_LENGTH);

      cur_blk->begin_write(m_write_pos);

      if (!cur_blk->add_page(bpage, &m_write_pos)) {
        /* Should always succeed. */
        ut_d(ut_error);
      }

      /* page added successfully. */
      break;

    } else {
      bool success;

      ut_a(cur_blk->get_state() == ARCH_BLOCK_READY_TO_FLUSH);

      auto cbk = std::bind(&Arch_Block::is_flushable, *cur_blk);

      /* Might release operation mutex temporarily. Need to
      loop again verifying the state. */
      success = wait_flush_archiver(cbk);
      count = success ? 0 : 2;

      continue;
    }
  }
  arch_oper_mutex_exit();
}

/** Get page IDs from a specific position.
Caller must ensure that read_len doesn't exceed the block.
@param[in]      group           group whose pages we're interested in
@param[in]      read_pos        position in archived data
@param[in]      read_len        amount of data to read
@param[out]     read_buff       buffer to return the page IDs.
@note Caller must allocate the buffer.
@return true if we could successfully read the block. */
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
  read_blk = m_data.get_block(read_pos, ARCH_DATA_BLOCK);

  read_blk->update_block_header(LSN_MAX, LSN_MAX);

  /* Read from the block. */
  success = read_blk->get_data(read_pos, read_len, read_buff);

  arch_oper_mutex_exit();

  return (success);
}

int Arch_Page_Sys::get_pages(MYSQL_THD thd, Page_Track_Callback cbk_func,
                             void *cbk_ctx, lsn_t &start_id, lsn_t &stop_id,
                             byte *buf, uint buf_len) {
  DBUG_PRINT("page_archiver", ("Fetch pages"));

  arch_mutex_enter();

  if (m_state == ARCH_STATE_READ_ONLY) {
    arch_mutex_exit();
    return (0);
  }

  /** 1. Get appropriate LSN range. */
  Arch_Group *group = nullptr;

  int error = fetch_group_within_lsn_range(start_id, stop_id, &group);

  DBUG_PRINT("page_archiver", ("Start id: %" PRIu64 ", stop id: %" PRIu64 "",
                               start_id, stop_id));

  if (error != 0) {
    arch_mutex_exit();
    return (error);
  }

  ut_ad(group != nullptr);

  /** 2. Get block position from where to start. */

  Arch_Page_Pos start_pos;
  Arch_Point reset_point;

  auto success = group->find_reset_point(start_id, reset_point);
  start_pos = reset_point.pos;
  start_id = reset_point.lsn;

  if (!success) {
    arch_mutex_exit();
    DBUG_PRINT("page_archiver",
               ("Can't fetch pages - No matching reset point."));
    return (ER_PAGE_TRACKING_RANGE_NOT_TRACKED);
  }

  /* 3. Fetch tracked pages. */

  DBUG_PRINT("page_archiver",
             ("Trying to get pages between %" PRIu64 " to %" PRIu64 "",
              start_id, stop_id));

  byte header_buf[ARCH_PAGE_BLK_HEADER_LENGTH];

  int err = 0;
  auto cur_pos = start_pos;
  Arch_Page_Pos temp_pos;
  uint num_pages;
  bool new_block = true;
  bool last_block = false;
  lsn_t block_stop_lsn = LSN_MAX;
  uint read_len = 0;
  uint bytes_left = 0;

  arch_oper_mutex_enter();

  auto end_lsn = group->get_end_lsn();

  Arch_Page_Pos last_pos =
      (end_lsn == LSN_MAX) ? m_write_pos : group->get_stop_pos();
  arch_oper_mutex_exit();

  while (true) {
    if (new_block) {
      temp_pos.m_block_num = cur_pos.m_block_num;
      temp_pos.m_offset = 0;

      /* Read the block header for data length and stop lsn info. */
      err = group->read_data(temp_pos, header_buf, ARCH_PAGE_BLK_HEADER_LENGTH);

      if (err != 0) {
        break;
      }

      block_stop_lsn = Arch_Block::get_stop_lsn(header_buf);
      auto data_len = Arch_Block::get_data_len(header_buf);
      bytes_left = data_len + ARCH_PAGE_BLK_HEADER_LENGTH;

      ut_ad(bytes_left <= ARCH_PAGE_BLK_SIZE);
      ut_ad(block_stop_lsn != LSN_MAX);

      bytes_left -= cur_pos.m_offset;

      if (data_len == 0 || cur_pos.m_block_num == last_pos.m_block_num ||
          block_stop_lsn > stop_id) {
        ut_ad(block_stop_lsn >= stop_id);
        stop_id = block_stop_lsn;
        last_block = true;
      }

      DBUG_PRINT("page_archiver",
                 ("%" PRIu64 " -> length : %u, stop lsn : %" PRIu64
                  ", last block : %u",
                  cur_pos.m_block_num, data_len, block_stop_lsn, last_block));
    }

    ut_ad(cur_pos.m_offset <= ARCH_PAGE_BLK_SIZE);

    /* Read how much ever is left to be read in the block. */
    read_len = bytes_left;

    if (last_block && read_len == 0) {
      /* There is nothing to read. */
      break;
    }

    if (read_len > buf_len) {
      read_len = buf_len;
    }

    /* Read the block for list of pages */
    err = group->read_data(cur_pos, buf, read_len);

    if (err != 0) {
      break;
    }

    cur_pos.m_offset += read_len;
    bytes_left -= read_len;
    num_pages = read_len / ARCH_BLK_PAGE_ID_SIZE;

    err = cbk_func(thd, buf, buf_len, num_pages, cbk_ctx);

    if (err != 0) {
      break;
    }

    if (bytes_left == 0) {
      /* We have read all the pages in the block. */

      if (last_block) {
        break;
      } else {
        new_block = true;
        bytes_left = 0;
        read_len = 0;
        cur_pos.set_next();
        continue;
      }
    } else {
      /* We still have some bytes to read from the current block. */
      new_block = false;
    }
  }

  arch_mutex_exit();

  return (0);
}

bool Arch_Page_Sys::get_num_pages(Arch_Page_Pos start_pos,
                                  Arch_Page_Pos stop_pos, uint64_t &num_pages) {
  if (start_pos.m_block_num > stop_pos.m_block_num ||
      ((start_pos.m_block_num == stop_pos.m_block_num) &&
       (start_pos.m_offset >= stop_pos.m_offset))) {
    return (false);
  }

  uint length = 0;

  if (start_pos.m_block_num != stop_pos.m_block_num) {
    length = ARCH_PAGE_BLK_SIZE - start_pos.m_offset;
    length += stop_pos.m_offset - ARCH_PAGE_BLK_HEADER_LENGTH;

    uint64_t num_blocks;
    num_blocks = stop_pos.m_block_num - start_pos.m_block_num - 1;
    length += num_blocks * (ARCH_PAGE_BLK_SIZE - ARCH_PAGE_BLK_HEADER_LENGTH);

  } else {
    length = stop_pos.m_offset - start_pos.m_offset;
  }

  num_pages = length / ARCH_BLK_PAGE_ID_SIZE;

  return (true);
}

int Arch_Page_Sys::get_num_pages(lsn_t &start_id, lsn_t &stop_id,
                                 uint64_t *num_pages) {
  DBUG_PRINT("page_archiver", ("Fetch num pages"));

  arch_mutex_enter();

  /** 1. Get appropriate LSN range. */
  Arch_Group *group = nullptr;

  int error = fetch_group_within_lsn_range(start_id, stop_id, &group);

#ifdef UNIV_DEBUG
  arch_oper_mutex_enter();

  DBUG_PRINT("page_archiver", ("Start id: %" PRIu64 ", stop id: %" PRIu64 "",
                               start_id, stop_id));
  if (is_active()) {
    DBUG_PRINT("page_archiver",
               ("Write_pos : %" PRIu64 ", %u", m_write_pos.m_block_num,
                m_write_pos.m_offset));
  }
  DBUG_PRINT("page_archiver",
             ("Latest stop lsn : %" PRIu64 "", m_latest_stop_lsn));

  arch_oper_mutex_exit();
#endif

  if (error != 0) {
    arch_mutex_exit();
    return (error);
  }

  ut_ad(group != nullptr);

  /** 2. Get block position from where to start. */

  Arch_Point start_point;
  bool success;

  success = group->find_reset_point(start_id, start_point);

  if (!success) {
    DBUG_PRINT("page_archiver",
               ("Can't fetch pages - No matching reset point."));
    arch_mutex_exit();
    return (ER_PAGE_TRACKING_RANGE_NOT_TRACKED);
  }

  DBUG_PRINT(
      "page_archiver",
      ("Start point - lsn : %" PRIu64 " \tpos : %" PRIu64 ", %u",
       start_point.lsn, start_point.pos.m_block_num, start_point.pos.m_offset));

  Arch_Page_Pos start_pos = start_point.pos;
  start_id = start_point.lsn;

  /** 3. Get block position where to stop */

  Arch_Point stop_point;

  success = group->find_stop_point(stop_id, stop_point, m_write_pos);
  ut_ad(success);

  DBUG_PRINT(
      "page_archiver",
      ("Stop point - lsn : %" PRIu64 " \tpos : %" PRIu64 ", %u", stop_point.lsn,
       stop_point.pos.m_block_num, stop_point.pos.m_offset));

  arch_mutex_exit();

  Arch_Page_Pos stop_pos = stop_point.pos;
  stop_id = stop_point.lsn;

  /** 4. Fetch number of pages tracked. */

  ut_ad(start_point.lsn <= stop_point.lsn);
  ut_ad(start_point.pos.m_block_num <= stop_point.pos.m_block_num);

  success = get_num_pages(start_pos, stop_pos, *num_pages);

  if (!success) {
    num_pages = nullptr;
  }

  DBUG_PRINT("page_archiver",
             ("Number of pages tracked : %" PRIu64 "", *num_pages));

  return (0);
}

/** Wait for archive system to come out of #ARCH_STATE_PREPARE_IDLE.
If the system is preparing to idle, #start needs to wait
for it to come to idle state.
@return true, if successful
        false, if needs to abort */
bool Arch_Page_Sys::wait_idle() {
  ut_ad(mutex_own(&m_mutex));

  if (m_state == ARCH_STATE_PREPARE_IDLE) {
    os_event_set(page_archiver_thread_event);
    bool is_timeout = false;
    int alert_count = 0;

    auto err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_mutex));
          result = (m_state == ARCH_STATE_PREPARE_IDLE);

          if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
            return (ER_QUERY_INTERRUPTED);
          }

          if (result) {
            os_event_set(page_archiver_thread_event);

            /* Print messages every 1 minute - default is 5 seconds. */
            if (alert && ++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_MSG_24) << "Page Tracking start: waiting for "
                                        "idle state.";
            }
          }
          return (0);
        },
        &m_mutex, is_timeout);

    if (err == 0 && is_timeout) {
      err = ER_INTERNAL_ERROR;
      ib::info(ER_IB_MSG_25) << "Page Tracking start: wait for idle state "
                                "timed out";
      ut_d(ut_error);
    }

    if (err != 0) {
      return (false);
    }
  }
  return (true);
}

/** Check if the gap from last reset is short.
If not many page IDs are added till last reset, we avoid taking a new reset
point
@return true, if the gap is small. */
bool Arch_Page_Sys::is_gap_small() {
  ut_ad(m_last_pos.m_block_num <= m_write_pos.m_block_num);

  if (m_last_pos.m_block_num == m_write_pos.m_block_num) {
    return (true);
  }

  auto next_block_num = m_last_pos.m_block_num + 1;
  auto length = ARCH_PAGE_BLK_SIZE - m_last_pos.m_offset;

  if (next_block_num != m_write_pos.m_block_num) {
    return (false);
  }

  length += m_write_pos.m_offset - ARCH_PAGE_BLK_HEADER_LENGTH;

  /* Pages added since last reset. */
  auto num_pages = length / ARCH_BLK_PAGE_ID_SIZE;

  return (num_pages < ARCH_PAGE_RESET_THRESHOLD);
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

    bpage = buf_pool->oldest_hp.get();
    if (bpage != nullptr) {
      ut_ad(bpage->in_flush_list);
    } else {
      bpage = UT_LIST_GET_LAST(buf_pool->flush_list);
    }

    /* Add all pages for which IO is already started. */
    while (bpage != nullptr) {
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
      if (bpage->get_oldest_lsn() >
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

      /** We read the io_fix flag without holding buf_page_get_mutex(bpage), but
      we hold flush_state_mutex which is also taken when transitioning:
      - from BUF_IO_NONE to BUF_IO_WRITE in buf_flush_page()
      - from BUF_IO_WRITE to BUF_IO_NONE in buf_flush_write_complete()
      which are the only transitions to and from BUF_IO_WRITE state that we care
      about. */
      if (bpage->is_io_fix_write()) {
        /* IO has already started. Must add the page */
        track_page(bpage, LSN_MAX, LSN_MAX, true);
      }

      bpage = UT_LIST_GET_PREV(list, bpage);
    }

    buf_flush_list_mutex_exit(buf_pool);
    mutex_exit(&buf_pool->flush_state_mutex);
  }
}

/** Enable tracking pages in all buffer pools.
@param[in]      tracking_lsn    track pages from this LSN */
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

int Arch_Page_Sys::recovery_load_and_start(const Arch_Recv_Group_Info &info) {
  /* Initialise the page archiver with the info parsed from the files. */

  m_current_group = info.m_group;

  m_write_pos = info.m_write_pos;
  m_reset_pos = info.m_reset_pos;
  m_flush_pos = m_write_pos;

  Arch_Reset_File last_reset_file = info.m_last_reset_file;
  ut_ad(last_reset_file.m_start_point.size() > 0);
  Arch_Point reset_point = last_reset_file.m_start_point.back();

  m_last_pos = reset_point.pos;
  m_last_lsn = reset_point.lsn;
  m_last_reset_file_index = last_reset_file.m_file_index;

  ut_ad(m_last_lsn != LSN_MAX);

  auto err = m_ctx->init_during_recovery(m_current_group, m_last_lsn);

  if (err != 0) {
    return err;
  }

  if (info.m_new_empty_file) {
    m_flush_pos.set_next();
    m_write_pos.set_next();
    m_reset_pos.set_next();
    m_last_reset_file_index = m_reset_pos.m_block_num;
  }

  /* Reload both reset block and write block active at the time of a crash. */

  auto cur_blk = m_data.get_block(&m_write_pos, ARCH_DATA_BLOCK);
  auto reset_block = m_data.get_block(&m_reset_pos, ARCH_RESET_BLOCK);

  arch_mutex_enter();
  arch_oper_mutex_enter();

  cur_blk->begin_write(m_write_pos);
  reset_block->begin_write(m_write_pos);

  if (!info.m_new_empty_file) {
    cur_blk->set_data_len(m_write_pos.m_offset - ARCH_PAGE_BLK_HEADER_LENGTH);
    cur_blk->set_data(ARCH_PAGE_BLK_SIZE, info.m_last_data_block, 0);

    reset_block->set_data_len(m_reset_pos.m_offset -
                              ARCH_PAGE_BLK_HEADER_LENGTH);
    reset_block->set_data(ARCH_PAGE_BLK_SIZE, info.m_last_reset_block, 0);
  }

  ut_d(print());

  arch_oper_mutex_exit();
  arch_mutex_exit();

  return err;
}

int Arch_Page_Sys::start(Arch_Group **group, lsn_t *start_lsn,
                         Arch_Page_Pos *start_pos, bool is_durable,
                         bool restart, bool recovery) {
  /* Check if archiver task needs to be started. */
  arch_mutex_enter();

  if (m_state == ARCH_STATE_READ_ONLY) {
    arch_mutex_exit();
    return (0);
  }

  bool start_archiver = true;
  bool attach_to_current = false;
  bool acquired_oper_mutex = false;

  lsn_t log_sys_lsn = LSN_MAX;

  start_archiver = is_init();

  /* Wait for idle state, if preparing to idle. */
  if (!wait_idle()) {
    int err = 0;

    if (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP) {
      err = ER_QUERY_INTERRUPTED;
      my_error(err, MYF(0));
    } else {
      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Page Archiver wait too long");
    }

    arch_mutex_exit();
    return (err);
  }

  switch (m_state) {
    case ARCH_STATE_ABORT:
      arch_mutex_exit();
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      return (ER_QUERY_INTERRUPTED);

    case ARCH_STATE_INIT:
    case ARCH_STATE_IDLE:
      [[fallthrough]];

    case ARCH_STATE_ACTIVE:

      if (m_current_group != nullptr) {
        /* If gap is small, just attach to current group */
        attach_to_current = (recovery ? false : is_gap_small());

        if (attach_to_current) {
          DBUG_PRINT("page_archiver",
                     ("Gap is small - last pos : %" PRIu64
                      " %u, write_pos : %" PRIu64 " %u",
                      m_last_pos.m_block_num, m_last_pos.m_offset,
                      m_write_pos.m_block_num, m_write_pos.m_offset));
        }
      }

      if (!attach_to_current) {
        log_buffer_x_lock_enter(*log_sys);

        if (!recovery) {
          MONITOR_INC(MONITOR_PAGE_TRACK_RESETS);
        }

        log_sys_lsn = (recovery ? m_last_lsn : log_get_lsn(*log_sys));

        /* Enable/Reset buffer pool page tracking. */
        set_tracking_buf_pool(log_sys_lsn);

        /* Take operation mutex before releasing log_sys to
        ensure that all pages modified after log_sys_lsn are
        tracked. */
        arch_oper_mutex_enter();
        acquired_oper_mutex = true;

        log_buffer_x_lock_exit(*log_sys);
      } else {
        arch_oper_mutex_enter();
        acquired_oper_mutex = true;
      }
      break;

    case ARCH_STATE_PREPARE_IDLE:
    default:
      ut_d(ut_error);
  }

  if (is_init() && !m_data.init()) {
    ut_ad(!attach_to_current);
    acquired_oper_mutex = false;
    arch_oper_mutex_exit();
    arch_mutex_exit();

    my_error(ER_OUTOFMEMORY, MYF(0), ARCH_PAGE_BLK_SIZE);
    return (ER_OUTOFMEMORY);
  }

  /* Start archiver background task. */
  if (start_archiver) {
    ut_ad(!attach_to_current);

    auto err = start_page_archiver_background();

    if (err != 0) {
      acquired_oper_mutex = false;
      arch_oper_mutex_exit();
      arch_mutex_exit();

      ib::error(ER_IB_MSG_26) << "Could not start "
                              << "Archiver background task";
      return (err);
    }
  }

  /* Create a new archive group. */
  if (m_current_group == nullptr) {
    ut_ad(!attach_to_current);

    m_last_pos.init();
    m_flush_pos.init();
    m_write_pos.init();
    m_reset_pos.init();
    m_request_flush_pos.init();
    m_request_blk_num_with_lsn = std::numeric_limits<uint64_t>::max();
    m_flush_blk_num_with_lsn = std::numeric_limits<uint64_t>::max();

    m_last_lsn = log_sys_lsn;
    m_last_reset_file_index = 0;

    m_current_group = ut::new_withkey<Arch_Group>(
        ut::make_psi_memory_key(mem_key_archive), log_sys_lsn,
        ARCH_PAGE_FILE_HDR_SIZE, &m_mutex);

    if (m_current_group == nullptr) {
      acquired_oper_mutex = false;
      arch_oper_mutex_exit();
      arch_mutex_exit();

      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Arch_Group));
      return (ER_OUTOFMEMORY);
    }

    const uint64_t new_file_size =
        static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_PAGE_FILE_CAPACITY;

    /* Initialize archiver file context. */
    auto db_err = m_current_group->init_file_ctx(
        ARCH_DIR, ARCH_PAGE_DIR, ARCH_PAGE_FILE, 0, new_file_size, 0);

    if (db_err != DB_SUCCESS) {
      arch_oper_mutex_exit();
      arch_mutex_exit();

      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Arch_File_Ctx));
      return (ER_OUTOFMEMORY);
    }

    m_group_list.push_back(m_current_group);

    Arch_Block *reset_block = m_data.get_block(&m_reset_pos, ARCH_RESET_BLOCK);
    reset_block->begin_write(m_write_pos);

    DBUG_PRINT("page_archiver", ("Creating a new archived group."));

  } else if (!attach_to_current && !recovery) {
    /* It's a reset. */
    m_last_lsn = log_sys_lsn;
    m_last_pos = m_write_pos;

    DBUG_PRINT("page_archiver", ("It's a reset."));
  }

  m_state = ARCH_STATE_ACTIVE;
  *start_lsn = m_last_lsn;

  bool wait_for_block_flush = false;

  if (!recovery) {
    if (!attach_to_current) {
      wait_for_block_flush = save_reset_point(is_durable);

    } else if (is_durable && !m_current_group->is_durable()) {
      /* In case this is the first durable archiving of the group and if the
      gap is small for a reset then set the below variable and wait for the
      reset info to be flushed before we return to the caller. */

      wait_for_block_flush = true;
      m_request_blk_num_with_lsn = m_last_pos.m_block_num;
    }
  }

  acquired_oper_mutex = false;
  arch_oper_mutex_exit();

  ut_ad(m_last_lsn != LSN_MAX);
  ut_ad(m_current_group != nullptr);

  if (!restart) {
    /* Add pages to tracking for which IO has already started. */
    track_initial_pages();

    *group = m_current_group;

    *start_pos = m_last_pos;

    arch_oper_mutex_enter();
    acquired_oper_mutex = true;

    /* Attach to the group. */
    m_current_group->attach(is_durable);

  } else if (recovery) {
    arch_oper_mutex_enter();
    acquired_oper_mutex = true;

    /* Attach to the group. */
    m_current_group->attach(is_durable);
  }

  ut_ad(*group == m_current_group);

  if (acquired_oper_mutex) {
    arch_oper_mutex_exit();
  }

  arch_mutex_exit();

  if (wait_for_block_flush) {
    bool success = wait_for_reset_info_flush(m_request_blk_num_with_lsn);

    if (!success) {
      ib::warn(ER_IB_WRN_PAGE_ARCH_FLUSH_DATA);
    }

    ut_ad(m_current_group->get_file_count());
  }

  if (!recovery) {
    if (is_durable && !restart) {
      m_current_group->mark_active();
      m_current_group->mark_durable();
    }

    /* Request checkpoint */
    log_request_checkpoint(*log_sys, true);
  }

  return (0);
}

int Arch_Page_Sys::stop(Arch_Group *group, lsn_t *stop_lsn,
                        Arch_Page_Pos *stop_pos, bool is_durable) {
  Arch_Block *cur_blk;

  arch_mutex_enter();

  if (m_state == ARCH_STATE_READ_ONLY) {
    arch_mutex_exit();
    return (0);
  }

  ut_ad(group == m_current_group);
  ut_ad(m_state == ARCH_STATE_ACTIVE);

  arch_oper_mutex_enter();

  *stop_lsn = m_latest_stop_lsn;
  cur_blk = m_data.get_block(&m_write_pos, ARCH_DATA_BLOCK);
  update_stop_info(cur_blk);

  auto count = group->detach(*stop_lsn, &m_write_pos);
  arch_oper_mutex_exit();

  int err = 0;
  bool wait_for_block_flush = false;

  /* If no other active client, let the system get into idle state. */
  if (count == 0 && m_state != ARCH_STATE_ABORT) {
    set_tracking_buf_pool(LSN_MAX);

    arch_oper_mutex_enter();

    m_state = ARCH_STATE_PREPARE_IDLE;
    *stop_pos = m_write_pos;

    cur_blk->end_write();
    m_request_flush_pos = m_write_pos;
    m_write_pos.set_next();

    os_event_set(page_archiver_thread_event);

    wait_for_block_flush = m_current_group->is_durable() ? true : false;

  } else {
    if (m_state != ARCH_STATE_ABORT && is_durable &&
        !m_current_group->is_durable_client_active()) {
      /* In case the non-durable clients are still active but there are no
      active durable clients we need to mark the group inactive for recovery
      to know that no durable clients were active. */
      err = m_current_group->mark_inactive();
    }

    arch_oper_mutex_enter();

    *stop_pos = m_write_pos;
  }

  if (m_state == ARCH_STATE_ABORT) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    err = ER_QUERY_INTERRUPTED;
  }

  arch_oper_mutex_exit();
  arch_mutex_exit();

  if (wait_for_block_flush) {
    /* Wait for flush archiver to flush the blocks. */
    auto cbk = [&] {
      return (m_flush_pos.m_block_num > m_request_flush_pos.m_block_num ? false
                                                                        : true);
    };

    arch_oper_mutex_enter();

    if (!wait_flush_archiver(cbk)) {
      ib::warn(ER_IB_WRN_PAGE_ARCH_FLUSH_DATA);
    }

    arch_oper_mutex_exit();

    ut_ad(group->validate_info_in_files());
  }

  return (err);
}

void Arch_Page_Sys::release(Arch_Group *group, bool is_durable,
                            Arch_Page_Pos start_pos [[maybe_unused]]) {
  arch_mutex_enter();
  arch_oper_mutex_enter();

  group->release(is_durable);

  arch_oper_mutex_exit();

  if (group->is_active()) {
    arch_mutex_exit();
    return;
  }

  ut_ad(group != m_current_group);

  if (!group->is_referenced()) {
    m_group_list.remove(group);
    ut::delete_(group);
  }

  arch_mutex_exit();
}

dberr_t Arch_Page_Sys::flush_inactive_blocks(Arch_Page_Pos &cur_pos,
                                             Arch_Page_Pos end_pos) {
  dberr_t err = DB_SUCCESS;
  Arch_Block *cur_blk;

  /* Write all blocks that are ready for flushing. */
  while (cur_pos.m_block_num < end_pos.m_block_num) {
    cur_blk = m_data.get_block(&cur_pos, ARCH_DATA_BLOCK);

    err = cur_blk->flush(m_current_group, ARCH_FLUSH_NORMAL);

    if (err != DB_SUCCESS) {
      break;
    }

    MONITOR_INC(MONITOR_PAGE_TRACK_FULL_BLOCK_WRITES);

    arch_oper_mutex_enter();

    m_flush_blk_num_with_lsn = cur_pos.m_block_num;
    cur_pos.set_next();
    cur_blk->set_flushed();
    m_flush_pos.set_next();

    arch_oper_mutex_exit();
  }

  return (err);
}

dberr_t Arch_Page_Sys::flush_active_block(Arch_Page_Pos cur_pos,
                                          bool partial_reset_block_flush) {
  Arch_Block *cur_blk;
  cur_blk = m_data.get_block(&cur_pos, ARCH_DATA_BLOCK);

  arch_oper_mutex_enter();

  if (!cur_blk->is_active()) {
    arch_oper_mutex_exit();
    return (DB_SUCCESS);
  }

  /* Copy block data so that we can release the arch_oper_mutex soon. */
  Arch_Block *flush_blk = m_data.get_partial_flush_block();
  flush_blk->copy_data(cur_blk);

  arch_oper_mutex_exit();

  dberr_t err = flush_blk->flush(m_current_group, ARCH_FLUSH_PARTIAL);

  if (err != DB_SUCCESS) {
    return (err);
  }

  MONITOR_INC(MONITOR_PAGE_TRACK_PARTIAL_BLOCK_WRITES);

  if (partial_reset_block_flush) {
    arch_oper_mutex_enter();

    Arch_Block *reset_block = m_data.get_block(&m_reset_pos, ARCH_RESET_BLOCK);

    arch_oper_mutex_exit();

    err = reset_block->flush(m_current_group, ARCH_FLUSH_NORMAL);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  arch_oper_mutex_enter();

  m_flush_pos.m_offset =
      flush_blk->get_data_len() + ARCH_PAGE_BLK_HEADER_LENGTH;

  arch_oper_mutex_exit();

  return (err);
}

dberr_t Arch_Page_Sys::flush_blocks(bool *wait) {
  arch_oper_mutex_enter();

  auto request_flush_pos = m_request_flush_pos;
  auto cur_pos = m_flush_pos;
  auto end_pos = m_write_pos;
  auto request_blk_num_with_lsn = m_request_blk_num_with_lsn;
  auto flush_blk_num_with_lsn = m_flush_blk_num_with_lsn;

  arch_oper_mutex_exit();

  uint64_t ARCH_UNKNOWN_BLOCK = std::numeric_limits<uint64_t>::max();

  ut_ad(cur_pos.m_block_num <= end_pos.m_block_num);

  /* Caller needs to wait/sleep, if nothing to flush. */
  *wait = (cur_pos.m_block_num == end_pos.m_block_num);

  dberr_t err;

  err = flush_inactive_blocks(cur_pos, end_pos);

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (cur_pos.m_block_num == end_pos.m_block_num) {
    /* Partial Flush */

    bool data_block_flush =
        request_flush_pos.m_block_num == cur_pos.m_block_num &&
        request_flush_pos.m_offset > cur_pos.m_offset;
    bool reset_block_flush =
        request_blk_num_with_lsn != ARCH_UNKNOWN_BLOCK &&
        (flush_blk_num_with_lsn == ARCH_UNKNOWN_BLOCK ||
         request_blk_num_with_lsn > flush_blk_num_with_lsn);

    /* We do partial flush only if we're explicitly requested to flush. */
    if (data_block_flush || reset_block_flush) {
      err = flush_active_block(cur_pos, reset_block_flush);

      if (err != DB_SUCCESS) {
        return (err);
      }
    }

    arch_oper_mutex_enter();

    if (request_blk_num_with_lsn != ARCH_UNKNOWN_BLOCK &&
        (flush_blk_num_with_lsn == ARCH_UNKNOWN_BLOCK ||
         request_blk_num_with_lsn > flush_blk_num_with_lsn)) {
      m_flush_blk_num_with_lsn = request_blk_num_with_lsn;
    }

    arch_oper_mutex_exit();
  }

  return (err);
}

bool Arch_Page_Sys::archive(bool *wait) {
  dberr_t db_err;

  auto is_abort = (srv_shutdown_state.load() == SRV_SHUTDOWN_LAST_PHASE ||
                   srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS ||
                   m_state == ARCH_STATE_ABORT);

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

  /* ARCH_STATE_ABORT is set for flush timeout which is asserted in debug. */
  ut_ad(m_state == ARCH_STATE_ACTIVE || m_state == ARCH_STATE_PREPARE_IDLE);

  auto set_idle = (m_state == ARCH_STATE_PREPARE_IDLE);
  arch_oper_mutex_exit();

  db_err = flush_blocks(wait);

  if (db_err != DB_SUCCESS) {
    is_abort = true;
  }

  /* Move to idle state or abort, if needed. */
  if (set_idle || is_abort) {
    arch_mutex_enter();
    arch_oper_mutex_enter();

    m_current_group->disable(LSN_MAX);
    m_current_group->close_file_ctxs();

    int err = 0;

    if (!is_abort && m_current_group->is_durable()) {
      err = m_current_group->mark_inactive();

      Arch_Group::init_dblwr_file_ctx(
          ARCH_DBLWR_DIR, ARCH_DBLWR_FILE, ARCH_DBLWR_NUM_FILES,
          static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_DBLWR_FILE_CAPACITY);

      ut_ad(m_current_group->validate_info_in_files());
    }

    if (err != 0) {
      is_abort = true;
    }

    /* Cleanup group, if no reference. */
    if (!m_current_group->is_referenced()) {
      m_group_list.remove(m_current_group);
      ut::delete_(m_current_group);
    }

    m_current_group = nullptr;
    m_state = is_abort ? ARCH_STATE_ABORT : ARCH_STATE_IDLE;

    arch_oper_mutex_exit();
    arch_mutex_exit();
  }

  return (is_abort);
}

int Arch_Group::read_from_file(Arch_Page_Pos *read_pos, uint read_len,
                               byte *read_buff) {
  char errbuf[MYSYS_STRERROR_SIZE];
  char file_name[MAX_ARCH_PAGE_FILE_NAME_LEN];

  /* Build file name */
  auto file_index = static_cast<uint>(
      Arch_Block::get_file_index(read_pos->m_block_num, ARCH_DATA_BLOCK));

  get_file_name(file_index, file_name, MAX_ARCH_PAGE_FILE_NAME_LEN);

  /* Find offset to read from. */
  os_offset_t offset =
      Arch_Block::get_file_offset(read_pos->m_block_num, ARCH_DATA_BLOCK);
  offset += read_pos->m_offset;

  bool success;

  /* Open file in read only mode. */
  pfs_os_file_t file =
      os_file_create(innodb_arch_file_key, file_name, OS_FILE_OPEN,
                     OS_FILE_NORMAL, OS_CLONE_LOG_FILE, true, &success);

  if (!success) {
    my_error(ER_CANT_OPEN_FILE, MYF(0), file_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (ER_CANT_OPEN_FILE);
  }

  /* Read from file to the user buffer. */
  IORequest request(IORequest::READ);

  request.disable_compression();
  request.clear_encrypted();

  auto db_err =
      os_file_read(request, file_name, file, read_buff, offset, read_len);

  os_file_close(file);

  if (db_err != DB_SUCCESS) {
    my_error(ER_ERROR_ON_READ, MYF(0), file_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    return (ER_ERROR_ON_READ);
  }

  return (0);
}

int Arch_Group::read_data(Arch_Page_Pos cur_pos, byte *buff, uint buff_len) {
  int err = 0;

  /* Attempt to read from in memory buffer. */
  auto success = arch_page_sys->get_pages(this, &cur_pos, buff_len, buff);

  if (!success) {
    /* The buffer is overwritten. Read from file. */
    err = read_from_file(&cur_pos, buff_len, buff);
  }

  return (err);
}

bool Arch_Page_Sys::save_reset_point(bool is_durable) {
  /* 1. Add the reset info to the reset block */

  uint current_file_index =
      Arch_Block::get_file_index(m_last_pos.m_block_num, ARCH_DATA_BLOCK);

  auto reset_block = m_data.get_block(&m_reset_pos, ARCH_RESET_BLOCK);

  /* If the reset info should belong to a new file then re-intialize the
  block as the block from now on will contain reset information belonging
  to the new file */
  if (m_last_reset_file_index != current_file_index) {
    ut_ad(current_file_index > m_last_reset_file_index);
    reset_block->begin_write(m_last_pos);
  }

  m_last_reset_file_index = current_file_index;

  reset_block->add_reset(m_last_lsn, m_last_pos);

  m_current_group->save_reset_point_in_mem(m_last_lsn, m_last_pos);

  auto cur_block = m_data.get_block(&m_last_pos, ARCH_DATA_BLOCK);

  if (cur_block->get_state() == ARCH_BLOCK_INIT ||
      cur_block->get_state() == ARCH_BLOCK_FLUSHED) {
    cur_block->begin_write(m_last_pos);
  }

  m_latest_stop_lsn = log_get_checkpoint_lsn(*log_sys);
  update_stop_info(cur_block);

  /* 2. Add the reset lsn to the current write_pos block header and request the
  flush archiver to flush the data block and reset block */

  cur_block->update_block_header(LSN_MAX, m_last_lsn);

  ut_d(auto ARCH_UNKNOWN_BLOCK = std::numeric_limits<uint64_t>::max());

  /* Reset LSN for a block can be updated only once. */
  ut_ad(m_flush_blk_num_with_lsn == ARCH_UNKNOWN_BLOCK ||
        m_flush_blk_num_with_lsn < cur_block->get_number());
  ut_ad(m_request_blk_num_with_lsn == ARCH_UNKNOWN_BLOCK ||
        m_request_blk_num_with_lsn < cur_block->get_number());

  uint64_t request_blk_num_with_lsn = cur_block->get_number();

  m_request_blk_num_with_lsn = request_blk_num_with_lsn;

  DBUG_PRINT("page_archiver",
             ("Saved reset point at %u - %" PRIu64 ", %" PRIu64 ", %u\n",
              m_last_reset_file_index, m_last_lsn, m_last_pos.m_block_num,
              m_last_pos.m_offset));

  return is_durable;
}

bool Arch_Page_Sys::wait_for_reset_info_flush(uint64_t request_blk) {
  auto ARCH_UNKNOWN_BLOCK = std::numeric_limits<uint64_t>::max();

  auto cbk = [&] {
    if (m_flush_blk_num_with_lsn == ARCH_UNKNOWN_BLOCK ||
        request_blk > m_flush_blk_num_with_lsn) {
      return (true);
    }

    return (false);
  };

  arch_oper_mutex_enter();

  bool success = wait_flush_archiver(cbk);

  arch_oper_mutex_exit();

  return (success);
}

int Arch_Page_Sys::fetch_group_within_lsn_range(lsn_t &start_id, lsn_t &stop_id,
                                                Arch_Group **group) {
  ut_ad(mutex_own(&m_mutex));

  if (start_id != 0 && stop_id != 0 && start_id >= stop_id) {
    return (ER_PAGE_TRACKING_RANGE_NOT_TRACKED);
  }

  arch_oper_mutex_enter();
  auto latest_stop_lsn = m_latest_stop_lsn;
  arch_oper_mutex_exit();

  ut_ad(latest_stop_lsn != LSN_MAX);

  if (start_id == 0 || stop_id == 0) {
    if (m_current_group == nullptr || !m_current_group->is_active()) {
      return (ER_PAGE_TRACKING_RANGE_NOT_TRACKED);
    }

    *group = m_current_group;

    ut_ad(m_last_lsn != LSN_MAX);

    start_id = (start_id == 0) ? m_last_lsn : start_id;
    stop_id = (stop_id == 0) ? latest_stop_lsn : stop_id;
  }

  if (start_id >= stop_id || start_id == LSN_MAX || stop_id == LSN_MAX) {
    return (ER_PAGE_TRACKING_RANGE_NOT_TRACKED);
  }

  if (*group == nullptr) {
    for (auto it : m_group_list) {
      *group = it;

      if (start_id < (*group)->get_begin_lsn() ||
          (!(*group)->is_active() && stop_id > (*group)->get_end_lsn()) ||
          ((*group)->is_active() && stop_id > latest_stop_lsn)) {
        *group = nullptr;
        continue;
      }

      break;
    }
  }

  if (*group == nullptr) {
    return (ER_PAGE_TRACKING_RANGE_NOT_TRACKED);
  }

  return (0);
}

uint Arch_Page_Sys::purge(lsn_t *purge_lsn) {
  lsn_t purged_lsn = LSN_MAX;
  uint err = 0;

  if (*purge_lsn == 0) {
    *purge_lsn = log_get_checkpoint_lsn(*log_sys);
  }

  DBUG_PRINT("page_archiver", ("Purging of files - %" PRIu64 "", *purge_lsn));

  arch_mutex_enter();

  for (auto it = m_group_list.begin(); it != m_group_list.end();) {
    lsn_t group_purged_lsn = LSN_MAX;
    auto group = *it;

    DBUG_PRINT("page_archiver",
               ("End lsn - %" PRIu64 "", group->get_end_lsn()));

    err = group->purge(*purge_lsn, group_purged_lsn);

    if (group_purged_lsn == LSN_MAX) {
      break;
    }

    DBUG_PRINT("page_archiver",
               ("Group purged lsn - %" PRIu64 "", group_purged_lsn));

    if (purged_lsn == LSN_MAX || group_purged_lsn > purged_lsn) {
      purged_lsn = group_purged_lsn;
    }

    if (!group->is_active() && group->get_end_lsn() <= group_purged_lsn) {
      it = m_group_list.erase(it);
      ut::delete_(group);

      DBUG_PRINT("page_archiver", ("Purged entire group."));

      continue;
    }

    ++it;
  }

  DBUG_PRINT("page_archiver",
             ("Purged archived file until : %" PRIu64 "", purged_lsn));

  *purge_lsn = purged_lsn;

  if (purged_lsn == LSN_MAX) {
    arch_mutex_exit();
    return (err);
  }

  m_latest_purged_lsn = purged_lsn;

  arch_mutex_exit();

  return (err);
}

void Arch_Page_Sys::update_stop_info(Arch_Block *cur_blk) {
  ut_ad(mutex_own(&m_oper_mutex));

  if (cur_blk != nullptr) {
    cur_blk->update_block_header(m_latest_stop_lsn, LSN_MAX);
  }

  if (m_current_group != nullptr) {
    m_current_group->update_stop_point(m_write_pos, m_latest_stop_lsn);
  }
}

#ifdef UNIV_DEBUG
void Arch_Page_Sys::print() {
  DBUG_PRINT("page_archiver", ("State : %u", m_state));
  DBUG_PRINT("page_archiver", ("Last pos : %" PRIu64 ", %u",
                               m_last_pos.m_block_num, m_last_pos.m_offset));
  DBUG_PRINT("page_archiver", ("Last lsn : %" PRIu64 "", m_last_lsn));
  DBUG_PRINT("page_archiver",
             ("Latest stop lsn : %" PRIu64 "", m_latest_stop_lsn));
  DBUG_PRINT("page_archiver", ("Flush pos : %" PRIu64 ", %u",
                               m_flush_pos.m_block_num, m_flush_pos.m_offset));
  DBUG_PRINT("page_archiver", ("Write pos : %" PRIu64 ", %u",
                               m_write_pos.m_block_num, m_write_pos.m_offset));
  DBUG_PRINT("page_archiver", ("Reset pos : %" PRIu64 ", %u",
                               m_reset_pos.m_block_num, m_reset_pos.m_offset));
  DBUG_PRINT("page_archiver",
             ("Last reset file index : %u", m_last_reset_file_index));

  Arch_Block *reset_block = m_data.get_block(&m_reset_pos, ARCH_RESET_BLOCK);
  Arch_Block *data_block = m_data.get_block(&m_write_pos, ARCH_DATA_BLOCK);

  DBUG_PRINT("page_archiver", ("Latest reset block data length: %u",
                               reset_block->get_data_len()));
  DBUG_PRINT("page_archiver",
             ("Latest data block data length: %u", data_block->get_data_len()));
}
#endif
