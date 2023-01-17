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

#ifndef ARCH_RECV_INCLUDE
#define ARCH_RECV_INCLUDE

#include "arch0arch.h"
#include "arch0page.h"

/** Info related to each group parsed at different stages of page archive
recovery. */
class Arch_Recv_Group_Info {
 public:
  Arch_Recv_Group_Info() {
    m_reset_pos.init();
    m_write_pos.init();

    m_last_reset_block = static_cast<byte *>(ut::zalloc_withkey(
        ut::make_psi_memory_key(mem_key_archive), ARCH_PAGE_BLK_SIZE));
    m_last_data_block = static_cast<byte *>(ut::zalloc_withkey(
        ut::make_psi_memory_key(mem_key_archive), ARCH_PAGE_BLK_SIZE));
  }

  ~Arch_Recv_Group_Info() {
    ut::free(m_last_reset_block);
    ut::free(m_last_data_block);
  }

  /** Disable assignment. */
  Arch_Recv_Group_Info &operator=(const Arch_Recv_Group_Info &) = delete;

  /** Disable copy construction. */
  Arch_Recv_Group_Info(const Arch_Recv_Group_Info &) = delete;

  /** Group data. */
  Arch_Group *m_group{nullptr};

  /** Number of archived files belonging to the group. */
  uint m_num_files{0};

  /** Group is active or not. */
  bool m_active{false};

  /** True if group is from durable archiving, false if left over from a crash
  during clone operation. */
  bool m_durable{false};

  /** True if a new empty file was present in the group directory.
  This can happen in case of a crash while writing to a new file. */
  bool m_new_empty_file{false};

  /** The file index which is part of the file name may not necessarily
  be 0 always. It's possible that purge might have purged files in the
  group leading to the file index of the first file in the group being
  greater than 0. So we need this info to know the index of the first
  file in the group. */
  uint m_file_start_index{std::numeric_limits<uint>::max()};

  /** Last reset position of the group. */
  Arch_Page_Pos m_reset_pos;

  /** Last write position of the group. */
  Arch_Page_Pos m_write_pos;

  /** Reset block of the last reset file in a group. */
  byte *m_last_reset_block{nullptr};

  /** Data block of the last reset file in a group. */
  byte *m_last_data_block{nullptr};

  /** Reset file structure of the last reset file */
  Arch_Reset_File m_last_reset_file;

  /** Start LSN of the group. */
  lsn_t m_start_lsn{LSN_MAX};

  /** Last stop LSN of the group if active, else end LSN. */
  lsn_t m_last_stop_lsn{LSN_MAX};
};

/** Mapping of group directory name to information related to the recovery
group info. */
using Arch_Dir_Group_Info_Map =
    std::unordered_map<std::string, Arch_Recv_Group_Info>;

/** Doublewrite buffer block along with their info. */
struct Arch_Dblwr_Block {
  /** Type of block flushed into the doublewrite block */
  Arch_Blk_Type m_block_type;

  /** Flush type of the block flushed into the doublewrite buffer */
  Arch_Blk_Flush_Type m_flush_type;

  /** Block number of the block flushed into the doublewrite buffer */
  uint64_t m_block_num;

  /** Doublewrite buffer block */
  byte *m_block;
};

/** Vector of doublewrite buffer blocks and their info. */
using Arch_Dblwr_Blocks = std::vector<Arch_Dblwr_Block>;

/** Doublewrite buffer context. */
class Arch_Dblwr_Ctx {
 public:
  /** Constructor: Initialize members */
  Arch_Dblwr_Ctx() = default;

  ~Arch_Dblwr_Ctx() {
    ut::free(m_buf);
    m_file_ctx.close();
  }

  /** Initialize the doublewrite buffer.
  @param[in]  path      path to the file
  @param[in]  base_file file name prefix
  @param[in]  num_files initial number of files
  @param[in]  file_size file size in bytes
  @return error code. */
  dberr_t init(const char *path, const char *base_file, uint num_files,
               uint64_t file_size);

  /** Read the doublewrite buffer file.
  @return error code */
  dberr_t read_file();

  /** Validate the blocks contained in the m_buf buffer and load only the valid
  buffers into m_blocks.
  @param[in]  num_files number of files in the group information required to
  validate the blocks */
  void validate_and_fill_blocks(size_t num_files);

  /** Get doubewrite buffer blocks.
  @return doublewrite buffer blocks */
  Arch_Dblwr_Blocks blocks() { return m_blocks; }

  /** Disable copy construction. */
  Arch_Dblwr_Ctx(Arch_Dblwr_Ctx const &) = delete;

  /** Disable assignment. */
  Arch_Dblwr_Ctx &operator=(Arch_Dblwr_Ctx const &) = delete;

 private:
  /** Buffer to hold the contents of the doublwrite buffer. */
  byte *m_buf{nullptr};

  /** Total file size of the file which holds the doublewrite buffer. */
  uint64_t m_file_size{};

  /** Doublewrite buffer file context. */
  Arch_File_Ctx m_file_ctx;

  /** List of doublewrite buffer blocks. */
  Arch_Dblwr_Blocks m_blocks{};
};

/** Recovery system data structure for the archiver. */
class Arch_Page_Sys::Recovery {
 public:
  /** Constructor: Initialize members
  @param[in,out]  page_sys  global dirty page archive system
  @param[in]      dir_name  main archiver directory name */
  Recovery(Arch_Page_Sys *page_sys, const char *dir_name)
      : m_arch_dir_name(dir_name), m_page_sys(page_sys) {}

  /** Destructor: Close open file and free resources */
  ~Recovery() = default;

  /** Initialise the archiver's recovery system.
  @return error code. */
  dberr_t init_dblwr();

  /** Scan the archive directory and fetch all info related to group
  directories and its files.
  @return true if the scan was successful. */
  bool scan_for_groups();

#ifdef UNIV_DEBUG
  /** Print information related to the archiver recovery system added
  for debugging purposes. */
  void print();
#endif

  /** Parse for group information and fill the group.
  @return error code. */
  dberr_t recover();

  /** Load archiver with the related data and start tracking if required.
  @return error code. */
  dberr_t load_archiver();

  /** Disable copy construction */
  Recovery(Recovery const &) = delete;

  /** Disable assignment */
  Recovery &operator=(Recovery const &) = delete;

 private:
  /** Read all the group directories and store information related to them
  required for parsing.
  @param[in]    file_path       file path information */
  void read_group_dirs(const std::string file_path);

  /** Read all the archived files belonging to a group and store information
  related to them required for parsing.
  @param[in]    dir_path        dir path information
  @param[in]    file_path       file path information */
  void read_group_files(const std::string dir_path,
                        const std::string file_path);

 private:
  /** Archive directory. */
  std::string m_arch_dir_name;

  /** Global dirty page archive system */
  Arch_Page_Sys *m_page_sys;

  /** Doublewrite buffer context. */
  Arch_Dblwr_Ctx m_dblwr_ctx{};

  /** Mapping of group directory names and group information related to
  the group. */
  Arch_Dir_Group_Info_Map m_dir_group_info_map{};
};

/** Recovery system data structure for the archiver. */
class Arch_Group::Recovery {
 public:
  /** Constructor.
  @param[in]  group the parent class group object */
  Recovery(Arch_Group *group) {
    ut_ad(group != nullptr);
    m_group = group;
  }

  /** Destructor. */
  ~Recovery() {}

  /** Check and replace blocks in archived files belonging to a group
  from the doublewrite buffer if required.
  @param[in]      dblwr_ctx     Doublewrite context which has the doublewrite
  buffer blocks
  @return error code */
  dberr_t replace_pages_from_dblwr(Arch_Dblwr_Ctx *dblwr_ctx);

  /** Delete the last file if there are no blocks flushed to it.
  @param[in,out]  info  information related to group required for recovery
  @return error code. */
  dberr_t cleanup_if_required(Arch_Recv_Group_Info &info);

  /** Start parsing the archive file for archive group information.
  @param[in,out]  info  information related to group required for recovery
  @return error code */
  dberr_t parse(Arch_Recv_Group_Info &info);

  /** Attach system client to the archiver during recovery if any group was
  active at the time of crash. */
  void attach() { ++m_group->m_dur_ref_count; }

  /** Disable copy construction */
  Recovery(Recovery const &) = delete;

  /** Disable assignment */
  Recovery &operator=(Recovery const &) = delete;

 private:
  /** The parent class group object. */
  Arch_Group *m_group{nullptr};
};

/** Recovery system data structure for the archiver. */
class Arch_File_Ctx::Recovery {
 public:
  /** Constructor.
  @param[in]  file_ctx file context to be used by this recovery class */
  Recovery(Arch_File_Ctx &file_ctx) : m_file_ctx(file_ctx) {}

  /** Destructor. */
  ~Recovery() {}

#ifdef UNIV_DEBUG
  /** Print recovery related data.
  @param[in]    file_start_index        file index from where to begin */
  void reset_print(uint file_start_index);
#endif

  /** Fetch the reset points pertaining to a file.
  @param[in]      file_index  file index of the file from which reset points
  needs to be fetched
  @param[in]      last_file true if the file for which the stop point is
  being fetched for is the last file
  @param[in,out]  info  information related to group required for recovery
  @return error code. */
  dberr_t parse_reset_points(uint file_index, bool last_file,
                             Arch_Recv_Group_Info &info);

  /** Fetch the stop lsn pertaining to a file.
  @param[in]      last_file true if the file for which the stop point is
  being fetched for is the last file
  @param[in,out]  info  information related to group required for recovery
  @return error code. */
  dberr_t parse_stop_points(bool last_file, Arch_Recv_Group_Info &info);

  /** Disable copy construction */
  Recovery(Recovery const &) = delete;

  /** Disable assignment */
  Recovery &operator=(Recovery const &) = delete;

 private:
  /** File context. */
  Arch_File_Ctx &m_file_ctx;
};

#endif /* ARCH_RECV_INCLUDE */
