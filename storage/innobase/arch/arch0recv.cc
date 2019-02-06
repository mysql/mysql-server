/*****************************************************************************


Copyright (c) 2018, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

  Recv arch_recv(ARCH_DIR);
  dberr_t err;

  err = arch_recv.init();

  if (!arch_recv.scan_group()) {
    DBUG_PRINT("page_archiver", ("No group information available"));
    return (DB_SUCCESS);
  }

  err = arch_recv.fill_info(this);

  if (err != DB_SUCCESS) {
    ib::error() << "Page archiver system's recovery failed";
    return (DB_OUT_OF_MEMORY);
  }

  return (err);
}

dberr_t Arch_Page_Sys::Recv::init() {
  dberr_t err;

  err = m_dblwr_ctx.init(
      ARCH_DBLWR_DIR, ARCH_DBLWR_FILE, ARCH_DBLWR_NUM_FILES,
      static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_DBLWR_FILE_CAPACITY);

  if (err != DB_SUCCESS) {
    return (err);
  }

  err = m_dblwr_ctx.read_blocks();

  return (err);
}

dberr_t Arch_Dblwr_Ctx::init(const char *dblwr_path,
                             const char *dblwr_base_file, uint dblwr_num_files,
                             uint64_t dblwr_file_size) {
  m_file_size = dblwr_file_size;

  m_buf = static_cast<byte *>(UT_NEW_ARRAY_NOKEY(byte, m_file_size));

  if (m_buf == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  memset(m_buf, 0, m_file_size);

  auto err = m_file_ctx.init(dblwr_path, nullptr, dblwr_base_file,
                             dblwr_num_files, m_file_size);

  return (err);
}

dberr_t Arch_Dblwr_Ctx::read_blocks() {
  ut_ad(m_buf != nullptr);

  auto err = m_file_ctx.open(true, LSN_MAX, 0, 0);

  if (err != DB_SUCCESS) {
    return (err);
  }

  ut_ad(m_file_ctx.get_phy_size() == m_file_size);

  /* Read the entire file. */
  err = m_file_ctx.read(m_buf, 0, m_file_size);

  if (err != DB_SUCCESS) {
    return (err);
  }

  Arch_Dblwr_Block dblwr_block;

  for (uint block_num = 0; block_num < m_file_size / ARCH_PAGE_BLK_SIZE;
       ++block_num) {
    auto block = m_buf + (block_num * ARCH_PAGE_BLK_SIZE);

    if (!Arch_Block::validate(block)) {
      continue;
    }

    if (block_num == ARCH_PAGE_DBLWR_RESET_PAGE) {
      dblwr_block.m_block_type = ARCH_RESET_BLOCK;
      dblwr_block.m_flush_type = ARCH_FLUSH_NORMAL;
    } else {
      dblwr_block.m_block_type = ARCH_DATA_BLOCK;
      dblwr_block.m_flush_type = (block_num == ARCH_PAGE_DBLWR_FULL_FLUSH_PAGE)
                                     ? ARCH_FLUSH_NORMAL
                                     : ARCH_FLUSH_PARTIAL;
    }

    dblwr_block.m_block_num = Arch_Block::get_block_number(block);
    dblwr_block.m_block = block;
    m_blocks.push_back(dblwr_block);
  }

  m_file_ctx.close();

  return (err);
}

#ifdef UNIV_DEBUG
void Arch_Page_Sys::Recv::print() {
  for (auto group : m_dir_group_info_map) {
    DBUG_PRINT("page_archiver",
               ("Group : %s\t%u", group.first.c_str(), group.second.m_active));
  }
}
#endif

void Arch_Page_Sys::Recv::read_group_dirs(const std::string file_path) {
  if (file_path.find(ARCH_PAGE_DIR) == std::string::npos) {
    return;
  }

  Arch_Recv_Group_Info info;
  m_dir_group_info_map.insert(
      std::pair<std::string, Arch_Recv_Group_Info>(file_path, info));
}

void Arch_Page_Sys::Recv::read_group_files(const std::string dir_path,
                                           const std::string file_path) {
  if (file_path.find(ARCH_PAGE_FILE) == std::string::npos &&
      file_path.find(ARCH_PAGE_GROUP_ACTIVE_FILE_NAME) == std::string::npos &&
      file_path.find(ARCH_PAGE_GROUP_DURABLE_FILE_NAME) == std::string::npos) {
    return;
  }

  Arch_Recv_Group_Info &info = m_dir_group_info_map[dir_path];

  if (file_path.find(ARCH_PAGE_GROUP_ACTIVE_FILE_NAME) != std::string::npos) {
    info.m_active = true;
    return;
  }

  if (file_path.find(ARCH_PAGE_GROUP_DURABLE_FILE_NAME) != std::string::npos) {
    info.m_durable = true;
    return;
  }

  info.m_num_files += 1;

  auto found = file_path.find(ARCH_PAGE_FILE);
  ut_ad(found != std::string::npos);
  uint file_index = 0;

  /* Fetch start index. */
  try {
    file_index = static_cast<uint>(
        std::stoi(file_path.substr(found + strlen(ARCH_PAGE_FILE))));
  } catch (const std::exception &) {
    ut_ad(0);
    ib::error() << "Invalid archived file name format. The archived file"
                << " is supposed to have the format " << ARCH_PAGE_FILE
                << "+ [0-9]*.";
    return;
  }

  if (info.m_file_start_index > file_index) {
    info.m_file_start_index = file_index;
  }
}

bool Arch_Page_Sys::Recv::scan_group() {
  os_file_type_t type;
  bool exists;
  bool success;

  success = os_file_status(m_arch_dir_name.c_str(), &exists, &type);

  if (!success || !exists || type != OS_FILE_TYPE_DIR) {
    return (false);
  }

  Dir_Walker::walk(m_arch_dir_name, false, [&](const std::string file_path) {
    read_group_dirs(file_path);
  });

  if (m_dir_group_info_map.size() == 0) {
    return (false);
  }

  for (auto it : m_dir_group_info_map) {
    Dir_Walker::walk(it.first, true, [&](const std::string file_path) {
      read_group_files(it.first, file_path);
    });
  }

  ut_d(print());

  return (true);
}

dberr_t Arch_Group::recovery_replace_pages_from_dblwr(
    Arch_Dblwr_Ctx *dblwr_ctx) {
  auto ARCH_UNKNOWN_BLOCK = std::numeric_limits<uint>::max();
  uint full_flush_blk_num = ARCH_UNKNOWN_BLOCK;
  auto dblwr_blocks = dblwr_ctx->get_blocks();
  size_t num_files = get_file_count();

  ut_ad(num_files > 0);

  for (uint index = 0; index < dblwr_blocks.size(); ++index) {
    auto dblwr_block = dblwr_blocks[index];

    switch (dblwr_block.m_block_type) {
      case ARCH_RESET_BLOCK:

        ut_ad(dblwr_block.m_block_num < num_files);
        /* If the block does not belong to the last file then ignore. */
        if (dblwr_block.m_block_num != num_files - 1) {
          continue;
        } else {
          break;
        }

      case ARCH_DATA_BLOCK: {
        uint file_index = Arch_Block::get_file_index(dblwr_block.m_block_num);
        ut_ad(file_index < num_files);

        /* If the block does not belong to the last file then ignore. */
        if (file_index < num_files - 1) {
          continue;
        }

        if (dblwr_block.m_flush_type == ARCH_FLUSH_NORMAL) {
          full_flush_blk_num = dblwr_block.m_block_num;
        } else {
          /* It's possible that the partial flush block might have been fully
          flushed, in which case we need to skip this block. */
          if (full_flush_blk_num != ARCH_UNKNOWN_BLOCK &&
              full_flush_blk_num >= dblwr_block.m_block_num) {
            continue;
          }
        }
      } break;

      default:
        ut_ad(false);
    }

    uint64_t offset = Arch_Block::get_file_offset(dblwr_block.m_block_num,
                                                  dblwr_block.m_block_type);

    ut_ad(m_file_ctx.is_closed());

    dberr_t err;

    err = m_file_ctx.open(false, m_begin_lsn, num_files - 1, 0);

    if (err != DB_SUCCESS) {
      return (err);
    }

    err = m_file_ctx.write(nullptr, dblwr_block.m_block, offset,
                           ARCH_PAGE_BLK_SIZE);

    if (err != DB_SUCCESS) {
      return (err);
    }

    m_file_ctx.close();
  }

  return (DB_SUCCESS);
}

dberr_t Arch_Group::recovery_cleanup_if_required(uint &num_files,
                                                 uint start_index, bool durable,
                                                 bool &empty_file) {
  dberr_t err;

  ut_ad(!durable || num_files > 0);
  ut_ad(m_file_ctx.is_closed());

  uint index = start_index + num_files - 1;

  /* Open the last file in the group. */
  err = m_file_ctx.open(true, m_begin_lsn, index, 0);

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (m_file_ctx.get_phy_size() != 0 && durable) {
    m_file_ctx.close();
    return (DB_SUCCESS);
  }

  empty_file = true;

  /* No blocks have been flushed into the file so delete the file. */

  char file_path[MAX_ARCH_PAGE_FILE_NAME_LEN];
  char dir_path[MAX_ARCH_DIR_NAME_LEN];

  m_file_ctx.build_name(index, m_begin_lsn, file_path,
                        MAX_ARCH_PAGE_FILE_NAME_LEN);

  auto found = std::string(file_path).find(ARCH_PAGE_FILE);
  ut_ad(found != std::string::npos);
  auto file_name = std::string(file_path).substr(found);

  m_file_ctx.build_dir_name(m_begin_lsn, dir_path, MAX_ARCH_DIR_NAME_LEN);

  m_file_ctx.close();

  arch_remove_file(dir_path, file_name.c_str());

  --num_files;

  /* If there are no archive files in the group we might as well
  purge it. */
  if (num_files == 0 || !durable) {
    m_is_active = false;

    found = std::string(dir_path).find(ARCH_PAGE_DIR);
    ut_ad(found != std::string::npos);

    auto path = std::string(dir_path).substr(0, found - 1);
    auto dir_name = std::string(dir_path).substr(found);

    num_files = 0;
    arch_remove_dir(path.c_str(), dir_name.c_str());
  }

  /* Need to reinitialize the file context as num_files has changed. */
  err = m_file_ctx.init(
      ARCH_DIR, ARCH_PAGE_DIR, ARCH_PAGE_FILE, num_files,
      static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_PAGE_FILE_CAPACITY);

  return (err);
}

dberr_t Arch_Page_Sys::Recv::fill_info(Arch_Page_Sys *page_sys) {
  uint num_active = 0;
  dberr_t err = DB_SUCCESS;
  bool new_empty_file = false;

  for (auto info = m_dir_group_info_map.begin();
       info != m_dir_group_info_map.end(); ++info) {
    auto group_info = info->second;
    auto dir_name = info->first;

    auto pos = dir_name.find(ARCH_PAGE_DIR);
    lsn_t start_lsn = static_cast<lsn_t>(
        std::stoull(dir_name.substr(pos + strlen(ARCH_PAGE_DIR))));

    Arch_Group *group = UT_NEW(
        Arch_Group(start_lsn, ARCH_PAGE_FILE_HDR_SIZE, page_sys->get_mutex()),
        mem_key_archive);

    Arch_Page_Pos write_pos;
    Arch_Page_Pos reset_pos;

    err = group->recover(&group_info, new_empty_file, &m_dblwr_ctx, write_pos,
                         reset_pos);

    if (err != DB_SUCCESS) {
      UT_DELETE(group);
      return (err);
    }

    if (group_info.m_num_files == 0) {
      UT_DELETE(group);
      continue;
    }

    page_sys->m_group_list.push_back(group);

    if (group_info.m_active) {
      /* Group was active at the time of shutdown/crash, so
      we need to start page archiving */

      page_sys->m_write_pos = write_pos;
      page_sys->m_reset_pos = reset_pos;

      ++num_active;
      int error = page_sys->start_during_recovery(group, new_empty_file);

      if (error != 0) {
        return (DB_ERROR);
      }
    }
  }

  /* There can be only one active group at a time. */
  ut_ad(num_active <= 1);

  return (DB_SUCCESS);
}

dberr_t Arch_Group::recover(Arch_Recv_Group_Info *group_info,
                            bool &new_empty_file, Arch_Dblwr_Ctx *dblwr_ctx,
                            Arch_Page_Pos &write_pos,
                            Arch_Page_Pos &reset_pos) {
  dberr_t err;

  err = init_file_ctx(
      ARCH_DIR, ARCH_PAGE_DIR, ARCH_PAGE_FILE, group_info->m_num_files,
      static_cast<uint64_t>(ARCH_PAGE_BLK_SIZE) * ARCH_PAGE_FILE_CAPACITY);

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (group_info->m_active) {
    /* Since the group was active at the time of crash it's possible that the
    doublewrite buffer might have the latest data in case of a crash. */

    err = recovery_replace_pages_from_dblwr(dblwr_ctx);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  err = recovery_cleanup_if_required(group_info->m_num_files,
                                     group_info->m_file_start_index,
                                     group_info->m_durable, new_empty_file);

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (group_info->m_num_files == 0) {
    return (err);
  }

  err = recovery_parse(write_pos, reset_pos, group_info->m_file_start_index);

  if (err != DB_SUCCESS) {
    return (err);
  }

  if (!group_info->m_active) {
    /* Group was inactive at the time of shutdown/crash, so
    we just add the group to the group list that the
    archiver maintains. */

    attach_during_recovery();
    m_stop_pos = write_pos;

    auto end_lsn = m_file_ctx.get_last_stop_point();
    ut_ad(end_lsn != LSN_MAX);

    disable(end_lsn);
  } else {
    err = open_file_during_recovery(write_pos, new_empty_file);
  }

  ut_d(m_file_ctx.recovery_reset_print(group_info->m_file_start_index));

  return (err);
}

#ifdef UNIV_DEBUG
void Arch_File_Ctx::recovery_reset_print(uint file_start_index) {
  Arch_Reset reset;
  Arch_Reset_File reset_file;
  Arch_Point start_point;

  DBUG_PRINT("page_archiver", ("No. of files : %u", m_count));

  if (m_reset.size() == 0) {
    DBUG_PRINT("page_archiver", ("No reset info available for this group."));
  }

  for (auto reset_file : m_reset) {
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
  for (auto stop_point : m_stop_points) {
    DBUG_PRINT("page_archiver",
               ("\tFile %u : %" PRIu64 "", file_index, stop_point));
    ++file_index;
  }
}
#endif

dberr_t Arch_Group::recovery_parse(Arch_Page_Pos &write_pos,
                                   Arch_Page_Pos &reset_pos,
                                   size_t start_index) {
  size_t num_files = get_file_count();
  dberr_t err = DB_SUCCESS;

  if (num_files == 0) {
    DBUG_PRINT("page_archiver", ("No group information available"));
    return (DB_SUCCESS);
  }

  ut_ad(m_file_ctx.is_closed());

  uint file_count = start_index + num_files;

  for (auto file_index = start_index; file_index < file_count; ++file_index) {
    if (file_index == start_index) {
      err = m_file_ctx.open(true, m_begin_lsn, start_index, 0);
    } else {
      err = m_file_ctx.open_next(m_begin_lsn, 0);
    }

    if (err != DB_SUCCESS) {
      break;
    }

    err = m_file_ctx.fetch_reset_points(file_index, reset_pos);

    if (err != DB_SUCCESS) {
      break;
    }

    bool last_file = (file_index + 1 == file_count);
    err = m_file_ctx.fetch_stop_points(last_file, write_pos);

    if (err != DB_SUCCESS) {
      break;
    }

    m_file_ctx.close();
  }

  m_file_ctx.close();

  return (err);
}

dberr_t Arch_File_Ctx::fetch_stop_points(bool last_file,
                                         Arch_Page_Pos &write_pos) {
  ut_ad(!is_closed());

  uint64_t offset;
  byte buf[ARCH_PAGE_BLK_SIZE];

  if (last_file) {
    offset = get_phy_size() - ARCH_PAGE_BLK_SIZE;
  } else {
    offset = ARCH_PAGE_FILE_DATA_CAPACITY * ARCH_PAGE_BLK_SIZE;
  }

  auto err = read(buf, offset, ARCH_PAGE_BLK_SIZE);

  if (err != DB_SUCCESS) {
    return (err);
  }

  auto stop_lsn = Arch_Block::get_stop_lsn(buf);
  m_stop_points.push_back(stop_lsn);

  write_pos.init();
  write_pos.m_block_num = Arch_Block::get_block_number(buf);
  write_pos.m_offset =
      Arch_Block::get_data_len(buf) + ARCH_PAGE_BLK_HEADER_LENGTH;

  return (err);
}

dberr_t Arch_File_Ctx::fetch_reset_points(uint file_index,
                                          Arch_Page_Pos &reset_pos) {
  ut_ad(!is_closed());
  ut_ad(m_index == file_index);

  byte buf[ARCH_PAGE_BLK_SIZE];

  /* Read reset block to fetch reset points. */
  auto err = read(buf, 0, ARCH_PAGE_BLK_SIZE);

  if (err != DB_SUCCESS) {
    return (err);
  }

  auto block_num = Arch_Block::get_block_number(buf);
  auto data_len = Arch_Block::get_data_len(buf);

  if (file_index != block_num) {
    /* This means there was no reset for this file and hence the
    reset block was not flushed. */

    ut_ad(Arch_Block::is_zeroes(buf));
    reset_pos.init();
    reset_pos.m_block_num = file_index;
    return (err);
  }

  /* Normal case. */
  reset_pos.m_block_num = block_num;
  reset_pos.m_offset = data_len + ARCH_PAGE_BLK_HEADER_LENGTH;

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

      start_point.lsn = fetch_reset_lsn(pos.m_block_num);
      start_point.pos = pos;

      reset_file.m_start_point.push_back(start_point);
    }

    m_reset.push_back(reset_file);
  }

  return (err);
}

lsn_t Arch_File_Ctx::fetch_reset_lsn(uint64_t block_num) {
  ut_ad(!is_closed());
  ut_ad(Arch_Block::get_file_index(block_num) == m_index);

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

dberr_t Arch_Group::recovery_read_latest_blocks(byte *buf, uint64_t offset,
                                                Arch_Blk_Type type) {
  dberr_t err = DB_SUCCESS;

  ut_ad(!m_file_ctx.is_closed());

  switch (type) {
    case ARCH_RESET_BLOCK: {
      ut_d(uint64_t file_size = m_file_ctx.get_phy_size());
      ut_ad((file_size > ARCH_PAGE_FILE_NUM_RESET_PAGE * ARCH_PAGE_BLK_SIZE) &&
            (file_size % ARCH_PAGE_BLK_SIZE == 0));

      err = m_file_ctx.read(buf, 0, ARCH_PAGE_BLK_SIZE);
    } break;

    case ARCH_DATA_BLOCK:
      err = m_file_ctx.read(buf, offset, ARCH_PAGE_BLK_SIZE);
      break;
  }

  return (err);
}
