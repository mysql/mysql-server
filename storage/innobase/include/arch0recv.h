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

#ifndef ARCH_RECV_INCLUDE
#define ARCH_RECV_INCLUDE

#include "arch0arch.h"
#include "arch0page.h"

/** Information of a group required for parsing information from the archived
file. */
struct Arch_Recv_Group_Info {
  /** Number of archived files belonging to the group. */
  uint m_num_files{0};

  /** Group is active or not. */
  bool m_active{false};

  /** True if group is from durable archiving, false if left over from a crash
   * during clone operation. */
  bool m_durable{false};

  /** The file index which is part of the file name may not necessarily
  be 0 always. It's possible that purge might have purged files in the
  group leading to the file index of the first file in the group being
  greater than 0. So we need this info to know the index of the first
  file in the group. */
  uint m_file_start_index{std::numeric_limits<uint>::max()};
};

/** Mapping of group directory name to information related to the group. */
using Arch_Dir_Group_Info_Map = std::map<std::string, Arch_Recv_Group_Info>;

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
  Arch_Dblwr_Ctx() {}

  /** Destructor: Close open file and free resources */
  ~Arch_Dblwr_Ctx() {
    if (m_buf != nullptr) {
      UT_DELETE_ARRAY(m_buf);
    }

    m_file_ctx.close();
  }

  /** Initialize the doublewrite buffer.
  @return error code. */
  dberr_t init(const char *path, const char *base_file, uint num_files,
               ib_uint64_t file_size);

  /** Load doublewrite buffer from the file that archiver maintains to
  keep doublewrite buffer.
  @return error code */
  dberr_t read_blocks();

  /** Get doubewrite buffer blocks.
  @return doublewrite buffer blocks */
  Arch_Dblwr_Blocks get_blocks() { return (m_blocks); }

  /** Disable copy construction */
  Arch_Dblwr_Ctx(Arch_Dblwr_Ctx const &) = delete;

  /** Disable assignment */
  Arch_Dblwr_Ctx &operator=(Arch_Dblwr_Ctx const &) = delete;

 private:
  /** Buffer to hold the contents of the doublwrite buffer. */
  byte *m_buf{nullptr};

  /** Total file size of the file which holds the doublewrite buffer. */
  uint64_t m_file_size{};

  /** Doublewrite buffer file context. */
  Arch_File_Ctx m_file_ctx{};

  /** List of doublewrite buffer blocks. */
  Arch_Dblwr_Blocks m_blocks{};
};

/** Recovery system data structure for the archiver. */
class Arch_Page_Sys::Recv {
 public:
  /** Constructor: Initialize members */
  Recv(const char *dir_name) : m_arch_dir_name(dir_name) {}

  /** Destructor: Close open file and free resources */
  ~Recv() {}

  /** Initialise the archiver's recovery system.
  @return error code. */
  dberr_t init();

  /** Scan the archive directory and fetch all the group directories.
  @return true if the scan was successful. */
  bool scan_group();

#ifdef UNIV_DEBUG
  /** Print information related to the archiver recovery system added
  for debugging purposes. */
  void print();
#endif

  /** Parse for group information and fill the group.
  @param[in,out]	page_sys	global dirty page archive system
  @return error code. */
  dberr_t fill_info(Arch_Page_Sys *page_sys);

  /** Disable copy construction */
  Recv(Recv const &) = delete;

  /** Disable assignment */
  Recv &operator=(Recv const &) = delete;

 private:
  /** Read all the group directories and store information related to them
  required for parsing.
  @param[in]	file_path	file path information */
  void read_group_dirs(const std::string file_path);

  /** Read all the archived files belonging to a group and store information
  related to them required for parsing.
  @param[in]	dir_path	dir path information
  @param[in]	file_path	file path information */
  void read_group_files(const std::string dir_path,
                        const std::string file_path);

 private:
  /** Archive directory. */
  std::string m_arch_dir_name;

  /** Doublewrite buffer context. */
  Arch_Dblwr_Ctx m_dblwr_ctx{};

  /** Mapping of group directory names and group information related to
  the group. */
  Arch_Dir_Group_Info_Map m_dir_group_info_map{};
};

#endif /* ARCH_RECV_INCLUDE */
