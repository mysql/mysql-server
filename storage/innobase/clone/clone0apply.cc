/*****************************************************************************

Copyright (c) 2017, 2021, Oracle and/or its affiliates.

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

/** @file clone/clone0apply.cc
 Innodb apply snapshot data

 *******************************************************/

#include <fstream>
#include <sstream>

#include "buf0dump.h"
#include "clone0api.h"
#include "clone0clone.h"
#include "dict0dict.h"
#include "log0log.h"
#include "sql/handler.h"

int Clone_Snapshot::get_file_from_desc(Clone_File_Meta *&file_desc,
                                       const char *data_dir, bool desc_create,
                                       bool &desc_exists) {
  int err = 0;

  mutex_enter(&m_snapshot_mutex);

  auto idx = file_desc->m_file_index;

  ut_ad(m_snapshot_handle_type == CLONE_HDL_APPLY);

  ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY ||
        m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);

  Clone_File_Vec &file_vector = (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY)
                                    ? m_data_file_vector
                                    : m_redo_file_vector;

  desc_exists = false;

  /* File metadata is already there, possibly sent by another task. */
  if (file_vector[idx] != nullptr) {
    file_desc = file_vector[idx];
    desc_exists = true;

  } else if (desc_create) {
    /* Create the descriptor. */
    err = create_desc(data_dir, file_desc);
  }

  mutex_exit(&m_snapshot_mutex);

  return (err);
}

int Clone_Snapshot::update_file_name(const char *data_dir,
                                     Clone_File_Meta *file_desc, char *path,
                                     size_t path_len) {
  auto space_id = file_desc->m_space_id;

  if (data_dir != nullptr || m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    return (0);
  }

  /* Update buffer pool dump file path for provisioning. */
  if (file_desc->m_space_id == dict_sys_t::s_invalid_space_id) {
    ut_ad(0 == strcmp(file_desc->m_file_name, SRV_BUF_DUMP_FILENAME_DEFAULT));
    buf_dump_generate_path(path, path_len);
    file_desc->m_file_name = path;
    file_desc->m_file_name_len = strlen(path) + 1;
    return (0);
  }

  /* Change name to system configured file when replacing current directory. */
  if (!fsp_is_system_tablespace(space_id)) {
    return (0);
  }

  /* Find out the node index of the file within system tablespace. */
  auto loop_index = file_desc->m_file_index;
  decltype(loop_index) node_index = 0;

  while (loop_index > 0) {
    --loop_index;
    auto cur_desc = m_data_file_vector[loop_index];
    /* Loop through all files of current tablespace. */
    if (cur_desc->m_space_id != space_id) {
      break;
    }
    ++node_index;
  }

  auto last_file_index =
      static_cast<decltype(node_index)>(srv_sys_space.m_files.size() - 1);

  /* Check if the file is beyond maximum configured files. */
  if (node_index > last_file_index) {
    std::ostringstream err_strm;
    err_strm << "innodb_data_file_path: Recipient file count: "
             << last_file_index + 1 << " is less than Donor file count.";

    std::string err_str(err_strm.str());

    my_error(ER_CLONE_SYS_CONFIG, MYF(0), err_str.c_str());

    return (ER_CLONE_SYS_CONFIG);
  }

  auto &file = srv_sys_space.m_files[node_index];
  page_size_t page_sz(srv_sys_space.flags());

  auto size_bytes = static_cast<uint64_t>(file.size());
  size_bytes *= page_sz.physical();

  /* Check if the file size matches with configured files. */
  if (file_desc->m_file_size != size_bytes) {
    /* For last file it could mismatch if auto extend is specified. */
    if (node_index != last_file_index ||
        !srv_sys_space.can_auto_extend_last_file()) {
      std::ostringstream err_strm;

      err_strm << "innodb_data_file_path: Recipient value for " << node_index
               << "th file size: " << size_bytes
               << " doesn't match Donor file size: " << file_desc->m_file_size;

      std::string err_str(err_strm.str());

      my_error(ER_CLONE_SYS_CONFIG, MYF(0), err_str.c_str());

      return (ER_CLONE_SYS_CONFIG);
    }
  }
  /* Change filename to currently configured name. */
  file_desc->m_file_name = file.filepath();
  file_desc->m_file_name_len = strlen(file_desc->m_file_name) + 1;

  return (0);
}

size_t Clone_Snapshot::compute_path_length(const char *data_dir,
                                           const Clone_File_Meta *file_desc) {
  bool is_absolute_path = false;
  auto alloc_len = sizeof(Clone_File_Meta);

  alloc_len += sizeof(CLONE_INNODB_REPLACED_FILE_EXTN);

  if (file_desc->m_file_name == nullptr) {
    alloc_len += MAX_LOG_FILE_NAME;
  } else {
    alloc_len += file_desc->m_file_name_len;
    std::string name;
    name.assign(file_desc->m_file_name, file_desc->m_file_name_len);
    is_absolute_path = Fil_path::is_absolute_path(name);
  }

  /* For absolute path, name length is the total length. */
  if (is_absolute_path) {
    return (alloc_len);
  }

  /* Add data directory length for relative path. */
  if (data_dir != nullptr) {
    alloc_len += strlen(data_dir);
    ++alloc_len;
    return (alloc_len);
  }

  /* While replacing current data directory, calculate length
  based on current system configuration. */

  /* Use server redo file location */
  if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    alloc_len += strlen(srv_log_group_home_dir);
    ++alloc_len;
    return (alloc_len);
  }

  ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY);

  /* Use server undo file location */
  if (fsp_is_undo_tablespace(file_desc->m_space_id)) {
    alloc_len += strlen(srv_undo_dir);
    ++alloc_len;
  }
  return (alloc_len);
}

int Clone_Snapshot::handle_existing_file(bool replace,
                                         Clone_File_Meta *file_desc) {
  /* For undo tablespace, check for duplicate file name. Currently it
  is possible to create multiple undo tablespaces of same name under
  different directory. This should not be recommended and in future
  we aim to disallow specifying file name for tablespaces and generate
  it internally based on space ID. Till that time, Clone needs to identify
  and disallow undo tablespaces of same name as Clone creates all undo
  tablespaces under innodb_undo_directory configuration in recipient. */
  if (fsp_is_undo_tablespace(file_desc->m_space_id)) {
    std::string clone_file(file_desc->m_file_name);
    clone_file.append(CLONE_INNODB_REPLACED_FILE_EXTN);

    for (auto undo_index : m_undo_file_indexes) {
      auto undo_meta = m_data_file_vector[undo_index];
      if (undo_meta == nullptr) {
        continue;
      }

      /* undo_meta: already added undo file with or without #clone extension.
      The #clone extension is present when recipient also has the same file.
      file_desc: current undo file name without #clone extension.
      clone_file: current undo file name with #clone extension.
      Since the existing undo file may or may not have the #clone extension
      we need to match both. */
      if (0 == strcmp(undo_meta->m_file_name, file_desc->m_file_name) ||
          0 == strcmp(undo_meta->m_file_name, clone_file.c_str())) {
        std::ostringstream err_strm;
        err_strm << "Found multiple undo files with same name: "
                 << file_desc->m_file_name;
        std::string err_str(err_strm.str());
        my_error(ER_CLONE_SYS_CONFIG, MYF(0), err_str.c_str());
        return (ER_CLONE_SYS_CONFIG);
      }
    }
    m_undo_file_indexes.push_back(file_desc->m_file_index);
    ut_ad(m_undo_file_indexes.size() <= FSP_MAX_UNDO_TABLESPACES);
  }

  std::string file_name;
  file_name.assign(file_desc->m_file_name);

  auto type = Fil_path::get_file_type(file_name);

  /* Nothing to do if file doesn't exist */
  if (type == OS_FILE_TYPE_MISSING) {
    int err = 0;
    if (replace) {
      /* Add file to new file list to enable rollback. */
      err = clone_add_to_list_file(CLONE_INNODB_NEW_FILES,
                                   file_desc->m_file_name);
    }
    return (err);
  }

  if (type != OS_FILE_TYPE_FILE) {
    /* Either the stat() call failed or the name is a
    directory/block device, or permission error etc. */
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(0), file_name.c_str(), errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    return (ER_ERROR_ON_WRITE);
  }

  ut_a(type == OS_FILE_TYPE_FILE);

  /* For cloning to different data directory, we must ensure that the
  file is not present. This would always fail for local clone. */
  if (!replace) {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), file_name.c_str());
    return (ER_FILE_EXISTS_ERROR);
  }

  /* Save original data file name. */
  std::string data_file(file_desc->m_file_name);

  /* For clone to current data directory, we need to clone system files
  to a file with different name and then move back during restart. */
  auto file_extn_loc = const_cast<char *>(file_desc->m_file_name);
  file_extn_loc += file_desc->m_file_name_len;

  /* Overwrite null terminator. */
  --file_extn_loc;
  strcpy(file_extn_loc, CLONE_INNODB_REPLACED_FILE_EXTN);

  file_desc->m_file_name_len += sizeof(CLONE_INNODB_REPLACED_FILE_EXTN);

  /* Check that file with clone extension is not present */
  file_name.assign(file_desc->m_file_name);
  type = Fil_path::get_file_type(file_name);

  if (type != OS_FILE_TYPE_MISSING) {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), file_name.c_str());
    return (ER_FILE_EXISTS_ERROR);
  }

  /* Add file name to files to be replaced before recovery. */
  auto err =
      clone_add_to_list_file(CLONE_INNODB_REPLACED_FILES, data_file.c_str());
  return (err);
}

int Clone_Snapshot::build_file_path(const char *data_dir, ulint alloc_size,
                                    Clone_File_Meta *&file_desc) {
  /* Check if data directory is being replaced. */
  bool replace_dir = (data_dir == nullptr);

  /* Allocate for file path string. */
  auto path = static_cast<char *>(mem_heap_alloc(m_snapshot_heap, alloc_size));

  if (path == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(0), alloc_size);
    return (ER_OUTOFMEMORY);
  }

  /* Copy file metadata */
  auto file_meta = reinterpret_cast<Clone_File_Meta *>(path);
  path += sizeof(Clone_File_Meta);
  *file_meta = *file_desc;

  bool is_absolute_path = false;
  std::string file_name;

  /* Check if absolute or relative path. */
  if (file_desc->m_file_name != nullptr) {
    file_name.assign(file_desc->m_file_name, file_desc->m_file_name_len);
    is_absolute_path = Fil_path::is_absolute_path(file_name);
  }

  file_meta->m_file_name = static_cast<const char *>(path);

  /* Copy path and file name together for absolute path. */
  if (is_absolute_path) {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY);
    auto is_hard_path = test_if_hard_path(file_desc->m_file_name);

    /* Check if the absolute path is not in right format */
    if (is_hard_path == 0) {
      my_error(ER_WRONG_VALUE, MYF(0), "file path", file_desc->m_file_name);
      return (ER_WRONG_VALUE);
    }

    strcpy(path, file_desc->m_file_name);

    auto err = handle_existing_file(replace_dir, file_meta);

    file_desc = file_meta;

    return (err);
  }

  const char *file_path = data_dir;

  /* Use configured path when cloning into current data directory. */
  if (file_path == nullptr) {
    /* Get file path from redo configuration. */
    if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
      file_path = srv_log_group_home_dir;

      /* Get file path from undo configuration. */
    } else if (fsp_is_undo_tablespace(file_desc->m_space_id)) {
      file_path = srv_undo_dir;
    }
  }

  /* Copy file path. */
  if (file_path != nullptr) {
    auto path_len = strlen(file_path);
    strcpy(path, file_path);

    /* Add path separator at the end of file path, if not there. */
    if (file_path[path_len - 1] != OS_PATH_SEPARATOR) {
      path[path_len] = OS_PATH_SEPARATOR;
      ++path;
    }
    path += path_len;
  }

  /* Copy file name */
  if (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY) {
    /* This is redo file. Use standard name. */
    snprintf(path, MAX_LOG_FILE_NAME, "%s%u", ib_logfile_basename,
             file_desc->m_file_index);
  } else {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY);
    ut_ad(file_desc->m_file_name != nullptr);
    /* For relative path remove "./" if there. */
    if (Fil_path::has_prefix(file_name, Fil_path::DOT_SLASH)) {
      file_name.erase(0, 2);
    }

    /* Copy adjusted file name */
    strcpy(path, file_name.c_str());
  }

  file_meta->m_file_name_len = strlen(file_meta->m_file_name) + 1;

  /* Check and handle when file is already present in recipient. */
  auto err = handle_existing_file(replace_dir, file_meta);
  file_desc = file_meta;

  return (err);
}

int Clone_Snapshot::create_desc(const char *data_dir,
                                Clone_File_Meta *&file_desc) {
  /* Update file name from configuration for system space */
  char path[OS_FILE_MAX_PATH];
  auto err = update_file_name(data_dir, file_desc, &path[0], sizeof(path));

  if (err != 0) {
    return (err);
  }

  /* Find out length of complete path string for file */
  auto alloc_size =
      static_cast<ulint>(compute_path_length(data_dir, file_desc));

  /* Build complete path for the new file to be added. */
  err = build_file_path(data_dir, alloc_size, file_desc);

  return (err);
}

bool Clone_Snapshot::add_file_from_desc(Clone_File_Meta *&file_desc) {
  mutex_enter(&m_snapshot_mutex);

  ut_ad(m_snapshot_handle_type == CLONE_HDL_APPLY);

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    m_data_file_vector[file_desc->m_file_index] = file_desc;
  } else {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);
    m_redo_file_vector[file_desc->m_file_index] = file_desc;
  }

  mutex_exit(&m_snapshot_mutex);

  /** Check if it the last file */
  if (file_desc->m_file_index == m_num_data_files - 1) {
    return true;
  }

  return (false);
}

int Clone_Handle::apply_task_metadata(Clone_Task *task,
                                      Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);
  uint desc_len = 0;
  auto serial_desc = callback->get_data_desc(&desc_len);

  Clone_Desc_Task_Meta task_desc;
  auto success = task_desc.deserialize(serial_desc, desc_len);

  if (!success) {
    ut_ad(false);
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Task Descriptor");
    return (err);
  }
  task->m_task_meta = task_desc.m_task_meta;
  return (0);
}

int Clone_Handle::check_space() {
  /* Do space check only during file copy. */
  if (m_clone_task_manager.get_state() != CLONE_SNAPSHOT_FILE_COPY) {
    return (0);
  }
  uint64_t free_space;
  auto MySQL_datadir_abs_path = MySQL_datadir_path.abs_path();
  auto data_dir =
      (replace_datadir() ? MySQL_datadir_abs_path.c_str() : get_datadir());

  auto db_err = os_get_free_space(data_dir, free_space);
  /* We skip space check if the OS interface returns error. */
  if (db_err != DB_SUCCESS) {
    ib::warn(ER_IB_CLONE_VALIDATE)
        << "Clone could not validate available free space";
    return (0);
  }

  auto snapshot = m_clone_task_manager.get_snapshot();
  auto bytes_disk = snapshot->get_disk_estimate();

  std::string avaiable_space;
  std::string clone_space;
  ut_format_byte_value(bytes_disk, clone_space);
  ut_format_byte_value(free_space, avaiable_space);

  int err = 0;
  if (bytes_disk > free_space) {
    err = ER_CLONE_DISK_SPACE;
    my_error(err, MYF(0), clone_space.c_str(), avaiable_space.c_str());
  }

  ib::info(ER_IB_CLONE_VALIDATE)
      << "Clone estimated size: " << clone_space.c_str()
      << " Available space: " << avaiable_space.c_str();
  return (err);
}

int Clone_Handle::apply_state_metadata(Clone_Task *task,
                                       Ha_clone_cbk *callback) {
  int err = 0;
  uint desc_len = 0;
  auto serial_desc = callback->get_data_desc(&desc_len);

  Clone_Desc_State state_desc;
  auto success = state_desc.deserialize(serial_desc, desc_len);

  if (!success) {
    ut_ad(false);
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid State Descriptor");
    return (err);
  }
  if (m_clone_handle_type == CLONE_HDL_COPY) {
    ut_ad(state_desc.m_is_ack);
    m_clone_task_manager.ack_state(&state_desc);
    return (0);
  }

  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  /* ACK descriptor is sent for keeping the connection alive. */
  if (state_desc.m_is_ack) {
    return (0);
  }

  /* Reset current chunk information */
  auto &task_meta = task->m_task_meta;
  task_meta.m_chunk_num = 0;
  task_meta.m_block_num = 0;

  /* Move to the new state */
  if (state_desc.m_is_start) {
#ifdef UNIV_DEBUG
    /* Network failure before moving to new state */
    err = m_clone_task_manager.debug_restart(task, err, 5);
#endif /* UNIV_DEBUG */

    /** Notify state change via callback. */
    notify_state_change(task, callback, &state_desc);

    err = move_to_next_state(task, nullptr, &state_desc);

#ifdef UNIV_DEBUG
    /* Network failure after moving to new state */
    err = m_clone_task_manager.debug_restart(task, err, 0);
#endif /* UNIV_DEBUG */

    /* Check if enough space available on disk */
    if (err == 0) {
      err = check_space();
    }

    return (err);
  }

  /* It is the end of current state. Close active file. */
  err = close_file(task);

#ifdef UNIV_DEBUG
  /* Network failure before finishing state */
  err = m_clone_task_manager.debug_restart(task, err, 2);
#endif /* UNIV_DEBUG */

  if (err != 0) {
    return (err);
  }

  ut_ad(state_desc.m_state == m_clone_task_manager.get_state());

  /* Mark current state finished for the task */
  err = m_clone_task_manager.finish_state(task);

#ifdef UNIV_DEBUG
  /* Network failure before sending ACK */
  err = m_clone_task_manager.debug_restart(task, err, 3);
#endif /* UNIV_DEBUG */

  /* Send acknowledgement back to remote server */
  if (err == 0 && task->m_is_master) {
    err = ack_state_metadata(task, callback, &state_desc);

    if (err != 0) {
      ib::info(ER_IB_CLONE_OPERATION)
          << "Clone Apply Master ACK finshed state: " << state_desc.m_state;
    }
  }

#ifdef UNIV_DEBUG
  /* Network failure after sending ACK */
  err = m_clone_task_manager.debug_restart(task, err, 4);
#endif /* UNIV_DEBUG */

  return (err);
}

void Clone_Handle::notify_state_change(Clone_Task *task, Ha_clone_cbk *callback,
                                       Clone_Desc_State *state_desc) {
  if (!task->m_is_master) {
    return;
  }
  callback->mark_state_change(state_desc->m_estimate);
  callback->buffer_cbk(nullptr, 0);
  callback->clear_flags();
}

int Clone_Handle::ack_state_metadata(Clone_Task *task, Ha_clone_cbk *callback,
                                     Clone_Desc_State *state_desc) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  state_desc->m_is_ack = true;

  byte desc_buf[CLONE_DESC_MAX_BASE_LEN];

  auto serial_desc = &desc_buf[0];
  uint desc_len = CLONE_DESC_MAX_BASE_LEN;

  state_desc->serialize(serial_desc, desc_len, nullptr);

  callback->set_data_desc(serial_desc, desc_len);
  callback->clear_flags();

  auto err = callback->buffer_cbk(nullptr, 0);

  return (err);
}

int Clone_Handle::apply_file_metadata(Clone_Task *task,
                                      Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  uint desc_len = 0;
  auto serial_desc = callback->get_data_desc(&desc_len);

  Clone_Desc_File_MetaData file_desc;
  auto success = file_desc.deserialize(serial_desc, desc_len);

  if (!success) {
    ut_ad(false);
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid File Descriptor");
    return (err);
  }
  auto file_meta = &file_desc.m_file_meta;
  auto snapshot = m_clone_task_manager.get_snapshot();

  ut_ad(snapshot->get_state() == file_desc.m_state);

  bool desc_exists;

  /* Check file metadata entry based on the descriptor. */
  auto err =
      snapshot->get_file_from_desc(file_meta, m_clone_dir, false, desc_exists);
  if (err != 0 || desc_exists) {
    return (err);
  }

  mutex_enter(m_clone_task_manager.get_mutex());

  /* Create file metadata entry based on the descriptor. */
  err = snapshot->get_file_from_desc(file_meta, m_clone_dir, true, desc_exists);
  file_meta->m_punch_hole = false;

  if (err != 0 || desc_exists) {
    mutex_exit(m_clone_task_manager.get_mutex());

    /* Save error with file name. */
    if (err != 0) {
      m_clone_task_manager.set_error(err, file_meta->m_file_name);
    }
    return (err);
  }

  if (file_desc.m_state == CLONE_SNAPSHOT_FILE_COPY) {
    auto file_type = OS_CLONE_DATA_FILE;

    if (file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {
      file_type = OS_CLONE_LOG_FILE;
    }

    /* Create the file */
    err = open_file(nullptr, file_meta, file_type, true, false);

    /* If last file is received, set all file metadata transferred */
    if (snapshot->add_file_from_desc(file_meta)) {
      m_clone_task_manager.set_file_meta_transferred();
    }

    mutex_exit(m_clone_task_manager.get_mutex());

    if (err != 0) {
      return (err);
    }

    /* Check and set punch hole for compressed page table. */
    if (file_type == OS_CLONE_DATA_FILE &&
        file_meta->m_compress_type != Compression::NONE) {
      page_size_t page_size(file_meta->m_fsp_flags);

      /* Disable punch hole if donor compression is not effective. */
      if (page_size.is_compressed() ||
          file_meta->m_fsblk_size * 2 > srv_page_size) {
        file_meta->m_punch_hole = false;
        return (0);
      }

      os_file_stat_t stat_info;
      os_file_get_status(file_meta->m_file_name, &stat_info, false, false);

      /* Check and disable punch hole if recipient cannot support it. */
      if (!IORequest::is_punch_hole_supported() ||
          stat_info.block_size * 2 > srv_page_size) {
        file_meta->m_punch_hole = false;
      } else {
        file_meta->m_punch_hole = true;
      }

      /* Currently the format for compressed and encrypted page is
      dependent on file system block size. */
      if (file_meta->m_encrypt_type != Encryption::NONE &&
          file_meta->m_fsblk_size != stat_info.block_size) {
        auto donor_str = std::to_string(file_meta->m_fsblk_size);
        auto recipient_str = std::to_string(stat_info.block_size);

        my_error(ER_CLONE_CONFIG, MYF(0), "FS Block Size", donor_str.c_str(),
                 recipient_str.c_str());
        err = ER_CLONE_CONFIG;
      }
    }
    return (err);
  }

  ut_ad(file_desc.m_state == CLONE_SNAPSHOT_REDO_COPY);

  /* open and reserve the redo file size */
  err = open_file(nullptr, file_meta, OS_CLONE_LOG_FILE, true, true);

  snapshot->add_file_from_desc(file_meta);

  /* For redo copy, check and add entry for the second file. */
  if (err == 0 && file_meta->m_file_index == 0) {
    file_meta = &file_desc.m_file_meta;
    file_meta->m_file_index++;

    err =
        snapshot->get_file_from_desc(file_meta, m_clone_dir, true, desc_exists);

    file_meta->m_punch_hole = false;

    if (err == 0 && !desc_exists) {
      err = open_file(nullptr, file_meta, OS_CLONE_LOG_FILE, true, true);
      snapshot->add_file_from_desc(file_meta);
    }
  }

  mutex_exit(m_clone_task_manager.get_mutex());
  return (err);
}

dberr_t Clone_Handle::punch_holes(os_file_t file, const byte *buffer,
                                  uint32_t len, uint64_t start_off,
                                  uint32_t page_len, uint32_t block_size) {
  dberr_t err = DB_SUCCESS;

  /* Loop through all pages in current data block and punch hole. */
  while (len >= page_len) {
    /* Validate compressed page type */
    auto page_type = mach_read_from_2(buffer + FIL_PAGE_TYPE);
    if (page_type == FIL_PAGE_COMPRESSED ||
        page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
      auto comp_len = mach_read_from_2(buffer + FIL_PAGE_COMPRESS_SIZE_V1);
      comp_len += FIL_PAGE_DATA;

      /* Align compressed length */
      comp_len = ut_calc_align(comp_len, block_size);

      auto offset = static_cast<ulint>(start_off + comp_len);
      auto hole_size = static_cast<ulint>(page_len - comp_len);

      err = os_file_punch_hole(file, offset, hole_size);
      if (err != DB_SUCCESS) {
        break;
      }
    }
    start_off += page_len;
    buffer += page_len;
    len -= page_len;
  }
  /* Must have consumed all data. */
  ut_ad(err != DB_SUCCESS || len == 0);
  return (err);
}

int Clone_Handle::modify_and_write(const Clone_Task *task, uint64_t offset,
                                   unsigned char *buffer, uint32_t buf_len) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  auto snapshot = m_clone_task_manager.get_snapshot();
  auto file_meta = snapshot->get_file_by_index(task->m_current_file_index);

  bool encryption = (file_meta->m_encrypt_type != Encryption::NONE);

  if (encryption) {
    bool success = true;

    bool is_page_copy = (snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY);
    bool key_page = (is_page_copy && offset == 0);

    bool is_log_file = (snapshot->get_state() == CLONE_SNAPSHOT_REDO_COPY);
    bool key_log = (is_log_file && file_meta->m_file_index == 0 && offset == 0);

    if (key_page) {
      /* Encrypt tablespace key with master key for encrypted tablespace. */
      page_size_t page_size(file_meta->m_fsp_flags);
      success = snapshot->encrypt_key_in_header(page_size, buffer);

    } else if (key_log) {
      /* Encrypt redo log key with master key */
      success = snapshot->encrypt_key_in_log_header(buffer, buf_len);
    }
    if (!success) {
      ut_ad(false);
      int err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Innodb Clone Apply Failed to Encrypt Key");
      return (err);
    }
  }

  /* No more compression/encryption is needed. */
  IORequest request(IORequest::WRITE);
  request.disable_compression();
  request.clear_encrypted();

  /* Write buffer to file. */
  errno = 0;
  auto db_err =
      os_file_write(request, "Clone data file", task->m_current_file_des,
                    reinterpret_cast<char *>(buffer), offset, buf_len);
  if (db_err != DB_SUCCESS) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(0), file_meta->m_file_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (ER_ERROR_ON_WRITE);
  }

  /* Attempt to punch holes if page compression is enabled. */
  if (file_meta->m_punch_hole) {
    page_size_t page_size(file_meta->m_fsp_flags);

    ut_ad(file_meta->m_compress_type != Compression::NONE ||
          file_meta->m_file_size > file_meta->m_alloc_size);
    ut_ad(IORequest::is_punch_hole_supported());
    ut_ad(!page_size.is_compressed());

    auto page_length = page_size.physical();
    auto start_offset = offset;

    ut_a(buf_len >= page_length);
    /* Skip first page */
    if (start_offset == 0) {
      start_offset += page_length;
      buffer += page_length;
      buf_len -= page_length;
    }
    auto db_err = punch_holes(task->m_current_file_des.m_file, buffer, buf_len,
                              start_offset, page_length,
                              static_cast<uint32_t>(file_meta->m_fsblk_size));
    if (db_err != DB_SUCCESS) {
      ut_ad(db_err == DB_IO_NO_PUNCH_HOLE);
      ib::info(ER_IB_CLONE_PUNCH_HOLE)
          << "Innodb Clone Apply failed to punch hole: "
          << file_meta->m_file_name;
      file_meta->m_punch_hole = false;
    }
  }
  return (0);
}

int Clone_Handle::receive_data(Clone_Task *task, uint64_t offset,
                               uint64_t file_size, uint32_t size,
                               Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  auto snapshot = m_clone_task_manager.get_snapshot();

  auto file_meta = snapshot->get_file_by_index(task->m_current_file_index);

  bool is_page_copy = (snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY);
  bool is_log_file = (snapshot->get_state() == CLONE_SNAPSHOT_REDO_COPY);

  /* During page and redo copy, we encrypt the key in header page. */
  bool key_page = (is_page_copy && offset == 0);
  bool key_log = (is_log_file && file_meta->m_file_index == 0 && offset == 0);

  if (key_page) {
    /* Check and update file size for space header page */
    if (file_meta->m_file_size < file_size) {
      snapshot->update_file_size(task->m_current_file_index, file_size);
    }
  }

  auto file_type = OS_CLONE_DATA_FILE;

  if (is_log_file || is_page_copy ||
      file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {
    file_type = OS_CLONE_LOG_FILE;
  }

  /* Open destination file for first block. */
  if (task->m_current_file_des.m_file == OS_FILE_CLOSED) {
    ut_ad(file_meta != nullptr);

    auto err = open_file(task, file_meta, file_type, true, false);

    if (err != 0) {
      /* Save error with file name. */
      m_clone_task_manager.set_error(err, file_meta->m_file_name);
      return (err);
    }
  }

  ut_ad(task->m_current_file_index == file_meta->m_file_index);

  /* Copy data to current destination file using callback. */
  char errbuf[MYSYS_STRERROR_SIZE];

  auto file_hdl = task->m_current_file_des.m_file;
  auto success = os_file_seek(nullptr, file_hdl, offset);

  if (!success) {
    my_error(ER_ERROR_ON_READ, MYF(0), file_meta->m_file_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    /* Save error with file name. */
    m_clone_task_manager.set_error(ER_ERROR_ON_READ, file_meta->m_file_name);
    return (ER_ERROR_ON_READ);
  }

  if (task->m_file_cache) {
    callback->set_os_buffer_cache();
    /* For data file recommend zero copy for cached IO. */
    if (!is_log_file) {
      callback->set_zero_copy();
    }
  }

  callback->set_dest_name(file_meta->m_file_name);

  bool modify_buffer = false;

  /* In case of page compression we need to punch hole. */
  if (file_meta->m_punch_hole) {
    ut_ad(!is_log_file);
    modify_buffer = true;
  }

  /* We need to encrypt the tablespace key by master key. */
  if (file_meta->m_encrypt_type != Encryption::NONE && (key_page || key_log)) {
    modify_buffer = true;
  }
  auto err = file_callback(callback, task, size, modify_buffer, offset
#ifdef UNIV_PFS_IO
                           ,
                           __FILE__, __LINE__
#endif /* UNIV_PFS_IO */
  );

  task->m_data_size += size;

  if (err != 0) {
    /* Save error with file name. */
    m_clone_task_manager.set_error(err, file_meta->m_file_name);
  }
  return (err);
}

int Clone_Handle::apply_data(Clone_Task *task, Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  /* Extract the data descriptor. */
  uint desc_len = 0;
  auto serial_desc = callback->get_data_desc(&desc_len);

  Clone_Desc_Data data_desc;
  auto success = data_desc.deserialize(serial_desc, desc_len);

  if (!success) {
    ut_ad(false);
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Data Descriptor");
    return (err);
  }
  /* Identify the task for the current block of data. */
  int err = 0;
  auto task_meta = &data_desc.m_task_meta;

  /* The data is from a different file. Close the current one. */
  if (task->m_current_file_index != data_desc.m_file_index) {
    err = close_file(task);
    if (err != 0) {
      return (err);
    }
    task->m_current_file_index = data_desc.m_file_index;
  }

  /* Receive data from callback and apply. */
  err = receive_data(task, data_desc.m_file_offset, data_desc.m_file_size,
                     data_desc.m_data_len, callback);

  /* Close file in case of error. */
  if (err != 0) {
    close_file(task);
  } else {
    err = m_clone_task_manager.set_chunk(task, task_meta);
  }

  return (err);
}

int Clone_Handle::apply(THD *thd, uint task_id, Ha_clone_cbk *callback) {
  int err = 0;
  uint desc_len = 0;

  auto clone_desc = callback->get_data_desc(&desc_len);
  ut_ad(clone_desc != nullptr);

  Clone_Desc_Header header;
  auto success = header.deserialize(clone_desc, desc_len);

  if (!success) {
    ut_ad(false);
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Descriptor Header");
    return (err);
  }

  /* Check the descriptor type in header and apply */
  auto task = m_clone_task_manager.get_task_by_index(task_id);

  switch (header.m_type) {
    case CLONE_DESC_TASK_METADATA:
      err = apply_task_metadata(task, callback);
      break;

    case CLONE_DESC_STATE:
      err = apply_state_metadata(task, callback);
      break;

    case CLONE_DESC_FILE_METADATA:
      err = apply_file_metadata(task, callback);
      break;

    case CLONE_DESC_DATA:
      err = apply_data(task, callback);
      break;

    default:
      ut_ad(false);
      break;
  }

  if (err != 0) {
    close_file(task);
  }

  return (err);
}

int Clone_Handle::restart_apply(THD *thd, const byte *&loc, uint &loc_len) {
  auto init_loc = m_restart_loc;
  auto init_len = m_restart_loc_len;
  auto alloc_len = m_restart_loc_len;

  /* Get latest locator */
  loc = get_locator(loc_len);

  m_clone_task_manager.reinit_apply_state(loc, loc_len, init_loc, init_len,
                                          alloc_len);

  /* Return the original locator if no state information */
  if (init_loc == nullptr) {
    return (0);
  }

  loc = init_loc;
  loc_len = init_len;

  /* Reset restart loc buffer if newly allocated */
  if (alloc_len > m_restart_loc_len) {
    m_restart_loc = init_loc;
    m_restart_loc_len = alloc_len;
  }

  ut_ad(loc == m_restart_loc);

  auto master_task = m_clone_task_manager.get_task_by_index(0);

  auto err = close_file(master_task);

  return (err);
}

void Clone_Snapshot::update_file_size(uint32_t file_index, uint64_t file_size) {
  /* Update file size when file is extended during page copy */
  ut_ad(m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY);

  auto cur_file = get_file_by_index(file_index);

  while (file_size > cur_file->m_file_size) {
    ++file_index;

    if (file_index >= m_num_data_files) {
      /* Update file size for the last file. */
      cur_file->m_file_size = file_size;
      break;
    }

    auto next_file = get_file_by_index(file_index);

    if (next_file->m_space_id != cur_file->m_space_id) {
      /* Update file size for the last file. */
      cur_file->m_file_size = file_size;
      break;
    }

    /* Only system tablespace can have multiple nodes. */
    ut_ad(cur_file->m_space_id == 0);

    file_size -= cur_file->m_file_size;
    cur_file = next_file;
  }
}

int Clone_Snapshot::init_apply_state(Clone_Desc_State *state_desc) {
  set_state_info(state_desc);

  int err = 0;
  switch (m_snapshot_state) {
    case CLONE_SNAPSHOT_FILE_COPY:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply State FILE COPY: ";
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply State PAGE COPY: ";
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply State REDO COPY: ";
      break;

    case CLONE_SNAPSHOT_DONE:
      /* Extend and flush data files. */
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply State FLUSH DATA: ";
      err = extend_and_flush_files(false);
      if (err != 0) {
        ib::info(ER_IB_CLONE_OPERATION)
            << "Clone Apply FLUSH DATA failed code: " << err;
        break;
      }
      /* Flush redo files. */
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply State FLUSH REDO: ";
      err = extend_and_flush_files(true);
      if (err != 0) {
        ib::info(ER_IB_CLONE_OPERATION)
            << "Clone Apply FLUSH REDO failed code: " << err;
        break;
      }
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply State DONE";
      break;

    case CLONE_SNAPSHOT_NONE:
    case CLONE_SNAPSHOT_INIT:
    default:
      ut_ad(false);
      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Innodb Clone Snapshot Invalid state");
      break;
  }
  return (err);
}

int Clone_Snapshot::extend_and_flush_files(bool flush_redo) {
  auto &file_vector = (flush_redo) ? m_redo_file_vector : m_data_file_vector;

  for (auto file_meta : file_vector) {
    char errbuf[MYSYS_STRERROR_SIZE];
    bool success = true;

    auto file =
        os_file_create(innodb_clone_file_key, file_meta->m_file_name,
                       OS_FILE_OPEN | OS_FILE_ON_ERROR_NO_EXIT, OS_FILE_NORMAL,
                       OS_CLONE_DATA_FILE, false, &success);

    if (!success) {
      my_error(ER_CANT_OPEN_FILE, MYF(0), file_meta->m_file_name, errno,
               my_strerror(errbuf, sizeof(errbuf), errno));

      return (ER_CANT_OPEN_FILE);
    }

    auto file_size = os_file_get_size(file);

    if (file_size < file_meta->m_file_size) {
      success = os_file_set_size(file_meta->m_file_name, file, file_size,
                                 file_meta->m_file_size, false, true);
    } else {
      success = os_file_flush(file);
    }

    os_file_close(file);

    if (!success) {
      my_error(ER_ERROR_ON_WRITE, MYF(0), file_meta->m_file_name, errno,
               my_strerror(errbuf, sizeof(errbuf), errno));

      return (ER_ERROR_ON_WRITE);
    }
  }
  return (0);
}
