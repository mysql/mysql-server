/*****************************************************************************


Copyright (c) 2018, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file include/arch0recv.h
 Interface for crash recovery for page archiver system.

 *******************************************************/

#include "arch0recv.h"

dberr_t Arch_Page_Sys::recover() {
  DBUG_PRINT("page_archiver", ("Crash Recovery"));

  Recovery arch_recv(this, ARCH_DIR);

  auto err = arch_recv.init_dblwr();

  if (err == DB_FILE_READ_BEYOND_SIZE) {
    ib::error(ER_IB_ERR_PAGE_ARCH_DBLWR_INIT_FAILED);
  }

  /* Scan for group directories and files */
  if (!arch_recv.scan_for_groups()) {
    DBUG_PRINT("page_archiver", ("No group information available"));
    return DB_SUCCESS;
  }

  err = arch_recv.recover();

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_ERR_PAGE_ARCH_RECOVERY_FAILED);
    return err;
  }

  err = arch_recv.load_archiver();

  if (err != DB_SUCCESS) {
    ib::error(ER_IB_ERR_PAGE_ARCH_RECOVERY_FAILED);
  }

  return err;
}

dberr_t Arch_Page_Sys::Recovery::init_dblwr() {
  auto err = m_dblwr_ctx.init(
      ARCH_DBLWR_DIR, ARCH_DBLWR_FILE, ARCH_DBLWR_NUM_FILES,
      static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_DBLWR_FILE_CAPACITY);

  if (err == DB_SUCCESS) {
    err = m_dblwr_ctx.read_file();
  }

  return err;
}

dberr_t Arch_Dblwr_Ctx::init(const char *dblwr_path,
                             const char *dblwr_base_file, uint dblwr_num_files,
                             uint64_t dblwr_file_size) {
  m_file_size = dblwr_file_size;

  m_buf = static_cast<byte *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, m_file_size));

  if (m_buf == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  auto err =
      m_file_ctx.init(ARCH_DIR, dblwr_path, dblwr_base_file, dblwr_num_files);

  return err;
}

dberr_t Arch_Dblwr_Ctx::read_file() {
  auto err = m_file_ctx.open(true, LSN_MAX, 0, 0, m_file_size);

  if (err != DB_SUCCESS) {
    return err;
  }

  if (m_file_ctx.get_phy_size() < m_file_size) {
    return DB_FILE_READ_BEYOND_SIZE;
  }

  ut_ad(m_buf != nullptr);

  /* Read the entire file. */
  err = m_file_ctx.read(m_buf, 0, static_cast<uint>(m_file_size));

  return err;
}

void Arch_Dblwr_Ctx::validate_and_fill_blocks(size_t num_files) {
  auto ARCH_UNKNOWN_BLOCK = std::numeric_limits<uint64_t>::max();
  uint64_t full_flush_blk_num = ARCH_UNKNOWN_BLOCK;

  for (uint dblwr_block_num = 0;
       dblwr_block_num < m_file_size / ARCH_PAGE_BLK_SIZE; ++dblwr_block_num) {
    auto dblwr_block_offset = m_buf + (dblwr_block_num * ARCH_PAGE_BLK_SIZE);
    auto block_num = Arch_Block::get_block_number(dblwr_block_offset);
    uint file_index = Arch_Block::get_file_index(
        block_num, Arch_Block::get_type(dblwr_block_offset));

    ut_ad(file_index < num_files);

    /* If the block does not belong to the last file then ignore. */
    if (file_index != num_files - 1) {
      continue;
    }

    if (!Arch_Block::validate(dblwr_block_offset)) {
      continue;
    }

    Arch_Dblwr_Block dblwr_block;

    switch (dblwr_block_num) {
      case ARCH_PAGE_DBLWR_RESET_PAGE:
        dblwr_block.m_block_type = ARCH_RESET_BLOCK;
        dblwr_block.m_flush_type = ARCH_FLUSH_NORMAL;
        break;

      case ARCH_PAGE_DBLWR_FULL_FLUSH_PAGE:
        full_flush_blk_num = block_num;

        dblwr_block.m_block_type = ARCH_DATA_BLOCK;
        dblwr_block.m_flush_type = ARCH_FLUSH_NORMAL;
        break;

      case ARCH_PAGE_DBLWR_PARTIAL_FLUSH_PAGE:
        /* It's possible that the partial flush block might have been fully
        flushed, in which case we need to skip this block. */
        if (full_flush_blk_num != ARCH_UNKNOWN_BLOCK &&
            full_flush_blk_num >= block_num) {
          continue;
        }

        dblwr_block.m_block_type = ARCH_DATA_BLOCK;
        dblwr_block.m_flush_type = ARCH_FLUSH_PARTIAL;
        break;

      default:
        ut_d(ut_error);
    }

    dblwr_block.m_block = dblwr_block_offset;
    dblwr_block.m_block_num = block_num;

    m_blocks.push_back(dblwr_block);
  }
}

#ifdef UNIV_DEBUG
void Arch_Page_Sys::Recovery::print() {
  for (auto group = m_dir_group_info_map.begin();
       group != m_dir_group_info_map.end(); ++group) {
    DBUG_PRINT("page_archiver", ("Group : %s\t%u", group->first.c_str(),
                                 group->second.m_active));
  }
}
#endif

void Arch_Page_Sys::Recovery::read_group_dirs(const std::string file_path) {
  if (file_path.find(ARCH_PAGE_DIR) == std::string::npos) {
    return;
  }

  try {
    size_t pos = file_path.find(ARCH_PAGE_DIR);

    lsn_t start_lsn = static_cast<lsn_t>(
        std::stoull(file_path.substr(pos + strlen(ARCH_PAGE_DIR))));

    auto &group_info = m_dir_group_info_map[file_path];

    group_info.m_start_lsn = start_lsn;

  } catch (const std::exception &) {
    ib::error(ER_IB_ERR_PAGE_ARCH_INVALID_FORMAT) << ARCH_PAGE_FILE;
    return;
  }
}

void Arch_Page_Sys::Recovery::read_group_files(const std::string dir_path,
                                               const std::string file_path) {
  if (file_path.find(ARCH_PAGE_FILE) == std::string::npos &&
      file_path.find(ARCH_PAGE_GROUP_ACTIVE_FILE_NAME) == std::string::npos &&
      file_path.find(ARCH_PAGE_GROUP_DURABLE_FILE_NAME) == std::string::npos) {
    return;
  }

  auto &info = m_dir_group_info_map[dir_path];

  if (file_path.find(ARCH_PAGE_GROUP_ACTIVE_FILE_NAME) != std::string::npos) {
    info.m_active = true;
    return;
  }

  if (file_path.find(ARCH_PAGE_GROUP_DURABLE_FILE_NAME) != std::string::npos) {
    info.m_durable = true;
    return;
  }

  info.m_num_files += 1;

  /* Fetch start index. */
  try {
    size_t found = file_path.find(ARCH_PAGE_FILE);

    size_t file_index = static_cast<uint>(
        std::stoi(file_path.substr(found + strlen(ARCH_PAGE_FILE))));

    if (info.m_file_start_index > file_index) {
      info.m_file_start_index = file_index;
    }
  } catch (const std::exception &) {
    ib::error(ER_IB_ERR_PAGE_ARCH_INVALID_FORMAT) << ARCH_PAGE_FILE;
    return;
  }
}

bool Arch_Page_Sys::Recovery::scan_for_groups() {
  os_file_type_t type;
  bool exists;

  os_file_status(m_arch_dir_name.c_str(), &exists, &type);

  if (!exists || type != OS_FILE_TYPE_DIR) {
    return false;
  }

  Dir_Walker::walk(m_arch_dir_name, false, [&](const std::string file_path) {
    read_group_dirs(file_path);
  });

  if (m_dir_group_info_map.size() == 0) {
    return false;
  }

  for (auto it = m_dir_group_info_map.begin(); it != m_dir_group_info_map.end();
       ++it) {
    Dir_Walker::walk(it->first, true, [&](const std::string file_path) {
      read_group_files(it->first, file_path);
    });
  }

  ut_d(print());

  return true;
}

dberr_t Arch_Group::Recovery::replace_pages_from_dblwr(
    Arch_Dblwr_Ctx *dblwr_ctx) {
  dberr_t err{DB_SUCCESS};

  uint num_files = m_group->get_file_count();

  ut_ad(num_files);

  dblwr_ctx->validate_and_fill_blocks(num_files);

  auto &file_ctx = m_group->m_file_ctx;

  err = file_ctx.open(false, m_group->m_begin_lsn, num_files - 1, 0,
                      m_group->get_file_size());

  if (err != DB_SUCCESS) {
    return err;
  }

  Arch_scope_guard file_ctx_guard([&file_ctx] { file_ctx.close(); });

  auto dblwr_blocks = dblwr_ctx->blocks();

  for (uint index = 0; index < dblwr_blocks.size(); ++index) {
    auto dblwr_block = dblwr_blocks[index];
    uint64_t offset = Arch_Block::get_file_offset(
        dblwr_block.m_block_num, Arch_Block::get_type(dblwr_block.m_block));

    if (file_ctx.get_phy_size() < offset) {
      break;
    }

    err = file_ctx.write(nullptr, dblwr_block.m_block,
                         static_cast<uint>(offset), ARCH_PAGE_BLK_SIZE);

    if (err != DB_SUCCESS) {
      break;
    }
  }

  return err;
}

dberr_t Arch_Group::Recovery::cleanup_if_required(Arch_Recv_Group_Info &info) {
  ut_ad(!info.m_durable || info.m_num_files > 0);

  auto &file_ctx = m_group->m_file_ctx;
  auto start_index = info.m_file_start_index;
  uint index = start_index + info.m_num_files - 1;

  ut_ad(file_ctx.is_closed());

  /* Open the last file in the group. */
  auto err = file_ctx.open(true, m_group->m_begin_lsn, index, 0,
                           m_group->get_file_size());

  if (err != DB_SUCCESS) {
    return err;
  }

  Arch_scope_guard file_ctx_guard([&file_ctx] { file_ctx.close(); });

  /* We check whether the archive file has anything else apart from the header
   * that was written to it during creation phase and treat it as an empty file
   * if it only has the header. */

  if (file_ctx.get_phy_size() > m_group->m_header_len && info.m_durable) {
    return DB_SUCCESS;
  }

  info.m_new_empty_file = true;

  /* No blocks have been flushed into the file so delete the file. */

  char file_path[MAX_ARCH_PAGE_FILE_NAME_LEN];
  char dir_path[MAX_ARCH_DIR_NAME_LEN];

  file_ctx.build_name(index, m_group->m_begin_lsn, file_path,
                      MAX_ARCH_PAGE_FILE_NAME_LEN);

  auto found = std::string(file_path).find(ARCH_PAGE_FILE);
  ut_ad(found != std::string::npos);
  auto file_name = std::string(file_path).substr(found);

  file_ctx.build_dir_name(m_group->m_begin_lsn, dir_path,
                          MAX_ARCH_DIR_NAME_LEN);

  file_ctx_guard.cleanup();

  arch_remove_file(dir_path, file_name.c_str());

  --info.m_num_files;

  /* If there are no archive files in the group or if it's not a durable group
  we might as well purge it. */
  if (info.m_num_files == 0 || !info.m_durable) {
    m_group->m_is_active = false;

    found = std::string(dir_path).find(ARCH_PAGE_DIR);
    ut_ad(found != std::string::npos);

    auto path = std::string(dir_path).substr(0, found - 1);
    auto dir_name = std::string(dir_path).substr(found);

    info.m_num_files = 0;
    arch_remove_dir(path.c_str(), dir_name.c_str());

    return err;
  }

  /* Need to reinitialize the file context as num_files has changed. */
  err =
      file_ctx.init(ARCH_DIR, ARCH_PAGE_DIR, ARCH_PAGE_FILE, info.m_num_files);

  return err;
}

dberr_t Arch_Page_Sys::Recovery::recover() {
  dberr_t err = DB_SUCCESS;
  uint num_active [[maybe_unused]] = 0;

  for (auto info = m_dir_group_info_map.begin();
       info != m_dir_group_info_map.end(); ++info) {
    auto &group_info = info->second;

    Arch_Group *group = ut::new_withkey<Arch_Group>(
        ut::make_psi_memory_key(mem_key_archive), group_info.m_start_lsn,
        ARCH_PAGE_FILE_HDR_SIZE, m_page_sys->get_mutex());

    if (group == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    err = group->recover(group_info, &m_dblwr_ctx);

    if (err != DB_SUCCESS) {
      ut::delete_(group);
      break;
    }

    if (group_info.m_num_files == 0) {
      ut::delete_(group);
      continue;
    }

    if (group_info.m_active) {
      ++num_active;
    }

    group_info.m_group = group;
  }

  /* There can be only one active group at a time. */
  ut_ad(num_active <= 1);

  return err;
}

dberr_t Arch_Page_Sys::Recovery::load_archiver() {
  dberr_t err = DB_SUCCESS;

  for (auto info_map = m_dir_group_info_map.begin();
       info_map != m_dir_group_info_map.end(); ++info_map) {
    auto &info = info_map->second;

    if (info.m_group == nullptr) {
      continue;
    }

    m_page_sys->m_group_list.push_back(info.m_group);

    if (!info.m_active) {
      continue;
    }

    /* Group was active at the time of shutdown/crash so start page archiving.
     */

    err = info.m_group->open_file(info.m_write_pos, info.m_new_empty_file);

    if (err != DB_SUCCESS) {
      break;
    }

    int error = m_page_sys->recovery_load_and_start(info);

    if (error) {
      err = DB_ERROR;
      break;
    }
  }

  return err;
}

dberr_t Arch_Group::recover(Arch_Recv_Group_Info &group_info,
                            Arch_Dblwr_Ctx *dblwr_ctx) {
  Recovery group_recv(this);

  const auto file_size =
      static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_PAGE_FILE_CAPACITY;

  auto err = init_file_ctx(ARCH_DIR, ARCH_PAGE_DIR, ARCH_PAGE_FILE,
                           group_info.m_num_files, file_size, 0);

  if (err != DB_SUCCESS) {
    return err;
  }

  if (group_info.m_active) {
    /* Since the group was active at the time of crash it's possible that the
    doublewrite buffer might have the latest data in case of a crash. */

    err = group_recv.replace_pages_from_dblwr(dblwr_ctx);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  err = group_recv.cleanup_if_required(group_info);

  if (err != DB_SUCCESS || group_info.m_num_files == 0) {
    return err;
  }

  err = group_recv.parse(group_info);

  if (err != DB_SUCCESS) {
    return err;
  }

  if (!group_info.m_active) {
    auto end_lsn = group_info.m_last_stop_lsn;
    ut_ad(end_lsn != LSN_MAX);

    m_stop_pos = group_info.m_write_pos;
    m_end_lsn = end_lsn;

    group_recv.attach();
    disable(end_lsn);
  }

#ifdef UNIV_DEBUG
  Arch_File_Ctx::Recovery file_ctx_recv(m_file_ctx);
  file_ctx_recv.reset_print(group_info.m_file_start_index);
#endif

  return err;
}

#ifdef UNIV_DEBUG
void Arch_File_Ctx::Recovery::reset_print(uint file_start_index) {
  Arch_Reset reset;
  Arch_Reset_File reset_file;
  Arch_Point start_point;

  DBUG_PRINT("page_archiver", ("No. of files : %u", m_file_ctx.m_count));

  if (m_file_ctx.m_reset.size() == 0) {
    DBUG_PRINT("page_archiver", ("No reset info available for this group."));
  }

  for (auto reset_file : m_file_ctx.m_reset) {
    DBUG_PRINT("page_archiver", ("File %u\tFile LSN : %" PRIu64 "",
                                 reset_file.m_file_index, reset_file.m_lsn));

    if (reset_file.m_start_point.size() == 0) {
      DBUG_PRINT("page_archiver", ("No reset info available for this file."));
    }

    for (uint i = 0; i < reset_file.m_start_point.size(); i++) {
      start_point = reset_file.m_start_point[i];
      DBUG_PRINT("page_archiver",
                 ("\tReset lsn : %" PRIu64 ", reset_pos : %" PRIu64 "\t %u",
                  start_point.lsn, start_point.pos.m_block_num,
                  start_point.pos.m_offset));
    }
  }

  DBUG_PRINT("page_archiver",
             ("Starting index of the file : %u", file_start_index));

  DBUG_PRINT("page_archiver", ("Latest stop points"));
  uint file_index = 0;
  for (auto stop_point : m_file_ctx.m_stop_points) {
    DBUG_PRINT("page_archiver",
               ("\tFile %u : %" PRIu64 "", file_index, stop_point));
    ++file_index;
  }
}
#endif

dberr_t Arch_Group::Recovery::parse(Arch_Recv_Group_Info &info) {
  dberr_t err = DB_SUCCESS;

  size_t num_files = m_group->get_file_count();

  if (num_files == 0) {
    DBUG_PRINT("page_archiver", ("No group information available"));
    return DB_SUCCESS;
  }

  uint start_index = info.m_file_start_index;
  size_t file_count = start_index + num_files;

  auto &file_ctx = m_group->m_file_ctx;

  for (uint file_index = start_index; file_index < file_count; ++file_index) {
    Arch_File_Ctx::Recovery file_ctx_recv(file_ctx);
    Arch_scope_guard file_ctx_guard([&file_ctx] { file_ctx.close(); });

    if (file_index == start_index) {
      err = file_ctx.open(true, m_group->m_begin_lsn, start_index, 0,
                          m_group->get_file_size());
    } else {
      err =
          file_ctx.open_next(m_group->m_begin_lsn, 0, m_group->get_file_size());
    }

    if (err != DB_SUCCESS) {
      break;
    }

    bool last_file = (file_index + 1 == file_count);

    err = file_ctx_recv.parse_reset_points(file_index, last_file, info);

    if (err != DB_SUCCESS) {
      break;
    }

    err = file_ctx_recv.parse_stop_points(last_file, info);

    if (err != DB_SUCCESS) {
      break;
    }
  }

  return err;
}

dberr_t Arch_File_Ctx::Recovery::parse_stop_points(bool last_file,
                                                   Arch_Recv_Group_Info &info) {
  ut_ad(!m_file_ctx.is_closed());

  uint64_t offset;
  byte buf[ARCH_PAGE_BLK_SIZE];

  auto phy_size = m_file_ctx.get_phy_size();

  if (last_file) {
    offset = phy_size - ARCH_PAGE_BLK_SIZE;
  } else {
    offset = ARCH_PAGE_FILE_DATA_CAPACITY * ARCH_PAGE_BLK_SIZE;
  }

  if (phy_size < offset + ARCH_PAGE_BLK_SIZE) {
    return DB_FILE_READ_BEYOND_SIZE;
  }

  auto err = m_file_ctx.read(buf, offset, ARCH_PAGE_BLK_SIZE);

  if (err != DB_SUCCESS) {
    return err;
  }

  auto stop_lsn = Arch_Block::get_stop_lsn(buf);
  m_file_ctx.m_stop_points.push_back(stop_lsn);

  if (last_file) {
    info.m_last_stop_lsn = stop_lsn;
    memcpy(info.m_last_data_block, buf, ARCH_PAGE_BLK_SIZE);
  }

  info.m_write_pos.init();
  info.m_write_pos.m_block_num = Arch_Block::get_block_number(buf);
  info.m_write_pos.m_offset =
      Arch_Block::get_data_len(buf) + ARCH_PAGE_BLK_HEADER_LENGTH;

  return err;
}

dberr_t Arch_File_Ctx::Recovery::parse_reset_points(
    uint file_index, bool last_file, Arch_Recv_Group_Info &info) {
  ut_ad(!m_file_ctx.is_closed());
  ut_ad(m_file_ctx.m_index == file_index);

  byte buf[ARCH_PAGE_BLK_SIZE];

  if (m_file_ctx.get_phy_size() < ARCH_PAGE_BLK_SIZE) {
    return DB_FILE_READ_BEYOND_SIZE;
  }

  /* Read reset block to fetch reset points. */
  auto err = m_file_ctx.read(buf, 0, ARCH_PAGE_BLK_SIZE);

  if (err != DB_SUCCESS) {
    return err;
  }

  auto block_num = Arch_Block::get_block_number(buf);
  auto data_len = Arch_Block::get_data_len(buf);

  if (file_index != block_num) {
    /* This means there was no reset for this file and hence the
    reset block was not flushed. */

    ut_ad(Arch_Block::is_zeroes(buf));
    info.m_reset_pos.init();
    info.m_reset_pos.m_block_num = file_index;
    return err;
  }

  /* Normal case. */
  info.m_reset_pos.m_block_num = block_num;
  info.m_reset_pos.m_offset = data_len + ARCH_PAGE_BLK_HEADER_LENGTH;

  if (last_file) {
    memcpy(info.m_last_reset_block, buf, ARCH_PAGE_BLK_SIZE);
  }

  Arch_Reset_File reset_file;
  reset_file.init();
  reset_file.m_file_index = file_index;

  if (data_len != 0) {
    uint length = 0;
    byte *buf1 = buf + ARCH_PAGE_BLK_HEADER_LENGTH;

    ut_ad(data_len >= ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE +
                          ARCH_PAGE_FILE_HEADER_RESET_POS_SIZE);

    reset_file.m_lsn = mach_read_from_8(buf1);
    length += ARCH_PAGE_FILE_HEADER_RESET_LSN_SIZE;

    Arch_Point start_point;
    Arch_Page_Pos pos;

    while (length != data_len) {
      ut_ad((data_len - length) % ARCH_PAGE_FILE_HEADER_RESET_POS_SIZE == 0);

      pos.m_block_num = mach_read_from_2(buf1 + length);
      length += ARCH_PAGE_FILE_HEADER_RESET_BLOCK_NUM_SIZE;

      pos.m_offset = mach_read_from_2(buf1 + length);
      length += ARCH_PAGE_FILE_HEADER_RESET_BLOCK_OFFSET_SIZE;

      start_point.lsn = m_file_ctx.fetch_reset_lsn(pos.m_block_num);
      start_point.pos = pos;

      reset_file.m_start_point.push_back(start_point);
    }

    m_file_ctx.m_reset.push_back(reset_file);
  }

  info.m_last_reset_file = reset_file;

  return err;
}

lsn_t Arch_File_Ctx::fetch_reset_lsn(uint64_t block_num) {
  ut_ad(!is_closed());
  ut_ad(Arch_Block::get_file_index(block_num, ARCH_DATA_BLOCK) == m_index);

  byte buf[ARCH_PAGE_BLK_SIZE];

  auto offset = Arch_Block::get_file_offset(block_num, ARCH_DATA_BLOCK);

  ut_ad(offset + ARCH_PAGE_BLK_SIZE <= get_phy_size());

  auto err = read(buf, offset, ARCH_PAGE_BLK_HEADER_LENGTH);

  if (err != DB_SUCCESS) {
    return (LSN_MAX);
  }

  auto lsn = Arch_Block::get_reset_lsn(buf);

  ut_ad(lsn != LSN_MAX);

  return (lsn);
}
