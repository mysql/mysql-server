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

/** @file clone/clone0apply.cc
 Innodb apply snapshot data

 *******************************************************/

#include <fstream>
#include <sstream>

#include "buf0dump.h"
#include "clone0api.h"
#include "clone0clone.h"
#include "dict0dict.h"
#include "log0files_io.h"
#include "sql/handler.h"

int Clone_Snapshot::get_file_from_desc(const Clone_File_Meta *file_meta,
                                       const char *data_dir, bool desc_create,
                                       bool &desc_exists,
                                       Clone_file_ctx *&file_ctx) {
  int err = 0;

  mutex_enter(&m_snapshot_mutex);

  auto idx = file_meta->m_file_index;

  ut_ad(m_snapshot_handle_type == CLONE_HDL_APPLY);

  ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY ||
        m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY ||
        m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);

  desc_exists = false;

  /* File metadata is already there, possibly sent by another task. */
  file_ctx = get_file_ctx_by_index(idx);

  if (file_ctx != nullptr) {
    desc_exists = true;

  } else if (desc_create) {
    /* Create the descriptor. */
    err = create_desc(data_dir, file_meta, false, file_ctx);
  }

  mutex_exit(&m_snapshot_mutex);

  return (err);
}

int Clone_Snapshot::rename_desc(const Clone_File_Meta *file_meta,
                                const char *data_dir,
                                Clone_file_ctx *&file_ctx) {
  /* Create new file context with new name. */
  auto err = create_desc(data_dir, file_meta, true, file_ctx);

  if (err != 0) {
    return err; /* purecov: inspected */
  }

  file_ctx->m_state.store(Clone_file_ctx::State::RENAMED);

  /* Overwrite with the renamed file context. */
  add_file_from_desc(file_ctx, false);

  return 0;
}

int Clone_Snapshot::fix_ddl_extension(const char *data_dir,
                                      Clone_file_ctx *file_ctx) {
  ut_ad(file_ctx->m_extension == Clone_file_ctx::Extension::DDL);

  /* If data directory is being replaced. */
  bool replace_dir = (data_dir == nullptr);

  auto file_meta = file_ctx->get_file_meta();
  bool is_undo_file = fsp_is_undo_tablespace(file_meta->m_space_id);
  bool is_redo_file = file_meta->m_space_id == dict_sys_t::s_log_space_id;

  auto extn = Clone_file_ctx::Extension::NONE;
  const std::string file_path(file_meta->m_file_name);

  /* Check if file is already present and extension is needed. */
  auto err = handle_existing_file(replace_dir, is_undo_file, is_redo_file,
                                  file_meta->m_file_index, file_path, extn);
  if (err == 0) {
    file_ctx->m_extension = extn;
  }

  return err;
}

int Clone_Snapshot::update_sys_file_name(bool replace,
                                         const Clone_File_Meta *file_meta,
                                         std::string &file_name) {
  /* Currently needed only while replacing data directory. */
  if (!replace) {
    return (0);
  }
  auto space_id = file_meta->m_space_id;

  /* Update buffer pool dump file path for provisioning. */
  if (space_id == dict_sys_t::s_invalid_space_id) {
    ut_ad(0 == strcmp(file_name.c_str(), SRV_BUF_DUMP_FILENAME_DEFAULT));

    char path[OS_FILE_MAX_PATH];
    buf_dump_generate_path(path, sizeof(path));

    file_name.assign(path);
    return (0);
  }

  /* Change name to system configured file when replacing current directory. */
  if (!fsp_is_system_tablespace(space_id)) {
    return (0);
  }

  /* Find out the node index of the file within system tablespace. */
  auto loop_index = file_meta->m_file_index;

  if (loop_index >= num_data_files()) {
    /* purecov: begin deadcode */
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid File Index");
    ut_d(ut_error);
    ut_o(return err);
    /* purecov: end */
  }

  decltype(loop_index) node_index = 0;

  while (loop_index > 0) {
    --loop_index;
    auto file_ctx = get_file_ctx_by_index(loop_index);
    auto cur_desc = file_ctx->get_file_meta();
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
  if (file_meta->m_file_size != size_bytes) {
    /* For last file it could mismatch if auto extend is specified. */
    if (node_index != last_file_index ||
        !srv_sys_space.can_auto_extend_last_file()) {
      /* purecov: begin tested */
      std::ostringstream err_strm;

      err_strm << "innodb_data_file_path: Recipient value for " << node_index
               << "th file size: " << size_bytes
               << " doesn't match Donor file size: " << file_meta->m_file_size;

      std::string err_str(err_strm.str());

      my_error(ER_CLONE_SYS_CONFIG, MYF(0), err_str.c_str());

      return (ER_CLONE_SYS_CONFIG);
      /* purecov: end */
    }
  }

  /* Change filename to currently configured name. */
  file_name.assign(file.filepath());
  return (0);
}

int Clone_Snapshot::handle_existing_file(bool replace, bool undo_file,
                                         bool redo_file,
                                         uint32_t data_file_index,
                                         const std::string &data_file,
                                         Clone_file_ctx::Extension &extn) {
  extn = Clone_file_ctx::Extension::NONE;
  /* For undo tablespace, check for duplicate file name. Currently it
  is possible to create multiple undo tablespaces of same name under
  different directory. This should not be recommended and in future
  we aim to disallow specifying file name for tablespaces and generate
  it internally based on space ID. Till that time, Clone needs to identify
  and disallow undo tablespaces of same name as Clone creates all undo
  tablespaces under innodb_undo_directory configuration in recipient. */
  if (undo_file) {
    for (auto undo_index : m_undo_file_indexes) {
      auto undo_file_ctx = get_file_ctx_by_index(undo_index);
      if (undo_file_ctx == nullptr || undo_file_ctx->deleted()) {
        continue;
      }
      auto undo_meta = undo_file_ctx->get_file_meta();

      if (0 == strcmp(undo_meta->m_file_name, data_file.c_str())) {
        /* purecov: begin tested */
        std::ostringstream err_strm;
        err_strm << "Found multiple undo files with same name: " << data_file;
        std::string err_str(err_strm.str());
        my_error(ER_CLONE_SYS_CONFIG, MYF(0), err_str.c_str());
        return (ER_CLONE_SYS_CONFIG);
        /* purecov: end */
      }
    }
    m_undo_file_indexes.push_back(data_file_index);
    /* With concurrent DDL support there could be deleted undo file
    indexes here. At the end of every stage, new undo files could be
    added limited by FSP_MAX_UNDO_TABLESPACES. */
    ut_ad(m_undo_file_indexes.size() <=
          CLONE_MAX_TRANSFER_STAGES * FSP_MAX_UNDO_TABLESPACES);
  }

  auto type = Fil_path::get_file_type(data_file);
  int err = 0;

  /* Consider redo files as existing always if we are cloning to
  the same directory on which we are working. */
  if (redo_file && replace && type == OS_FILE_TYPE_MISSING) {
    type = OS_FILE_TYPE_FILE;
  }

  /* Nothing to do if file doesn't exist. */
  if (type == OS_FILE_TYPE_MISSING) {
    if (replace) {
      /* Add file to new file list to enable rollback. */
      err = clone_add_to_list_file(CLONE_INNODB_NEW_FILES, data_file.c_str());
    }
    extn = Clone_file_ctx::Extension::NONE;
    return err;
  }

  if (type != OS_FILE_TYPE_FILE) {
    /* purecov: begin inspected */
    /* Either the stat() call failed or the name is a
    directory/block device, or permission error etc. */
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(0), data_file.c_str(), errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    return ER_ERROR_ON_WRITE;
    /* purecov: end */
  }

  /* For cloning to different data directory, we must ensure that the
  file is not present. This would always fail for local clone. */
  if (!replace) {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), data_file.c_str());
    return ER_FILE_EXISTS_ERROR;
  }

  std::string replace_path, clone_file;

  if (redo_file) {
    const auto [directory, file] = Fil_path::split(data_file);
    replace_path = directory;
    clone_file = directory + CLONE_INNODB_REPLACED_FILE_EXTN + file;
  } else {
    replace_path = data_file;
    clone_file = data_file + CLONE_INNODB_REPLACED_FILE_EXTN;
  }

  /* Check that file with clone extension is not present */
  type = Fil_path::get_file_type(clone_file);

  if (type != OS_FILE_TYPE_MISSING) {
    /* purecov: begin inspected */
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), clone_file.c_str());
    return ER_FILE_EXISTS_ERROR;
    /* purecov: end */
  }

  extn = Clone_file_ctx::Extension::REPLACE;

  /* Add file name to files to be replaced before recovery. */
  err =
      clone_add_to_list_file(CLONE_INNODB_REPLACED_FILES, replace_path.c_str());

  return err;
}

int Clone_Snapshot::build_file_path(const char *data_dir,
                                    const Clone_File_Meta *file_meta,
                                    std::string &built_path) {
  std::string source;

  bool redo_file = (m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);
  bool absolute_path = false;

  if (!redo_file) {
    source.assign(file_meta->m_file_name);

    bool replace = (data_dir == nullptr);
    auto err = update_sys_file_name(replace, file_meta, source);

    if (err != 0) {
      return err; /* purecov: inspected */
    }
    absolute_path = Fil_path::is_absolute_path(source);
  }

  /* For absolute path, copy the name and return. */
  if (absolute_path) {
    auto is_hard_path = test_if_hard_path(source.c_str());

    /* Check if the absolute path is not in right format */
    if (is_hard_path == 0) {
      /* purecov: begin inspected */
      my_error(ER_WRONG_VALUE, MYF(0), "file path", source.c_str());
      return (ER_WRONG_VALUE);
      /* purecov: end */
    }

    built_path.assign(source);
    return 0;
  }

  bool undo_file = fsp_is_undo_tablespace(file_meta->m_space_id);

  /* Append appropriate data directory path. */

  /* Use configured path when cloning into current data directory. */
  if (data_dir == nullptr) {
    /* Get file path from redo configuration. */
    if (redo_file) {
      /* Path returned by log_directory_path() will have the
      #innodb_redo directory at the end. */
      built_path = log_directory_path(log_sys->m_files_ctx);
    } else if (undo_file) {
      /* Get file path from undo configuration. */
      built_path = std::string{srv_undo_dir};
    } else {
      built_path = std::string{};
    }
  } else {
    built_path = std::string{data_dir};
    /* Add #innodb_redo directory to the path if this is redo file. */
    if (redo_file) {
      /* Add path separator at the end of file path, if not there. */
      Fil_path::append_separator(built_path);
      built_path += LOG_DIRECTORY_NAME;
    }
  }

  /* Add path separator if required. */
  Fil_path::append_separator(built_path);

  /* Add file name. For redo file use standard name. */
  if (redo_file) {
    /* This is redo file. Use standard name. */
    built_path += log_file_name(log_sys->m_files_ctx,
                                Log_file_id{file_meta->m_file_index});
    return 0;
  }

  ut_ad(!source.empty());

  if (Fil_path::has_prefix(source, Fil_path::DOT_SLASH)) {
    source.erase(0, 2);
  }

  built_path.append(source);
  return 0;
}

int Clone_Snapshot::build_file_ctx(Clone_file_ctx::Extension extn,
                                   const Clone_File_Meta *file_meta,
                                   const std::string &file_path,
                                   Clone_file_ctx *&file_ctx) {
  size_t alloc_size = sizeof(Clone_file_ctx) + file_path.length() + 1;

  /* Allocate for file path string. */
  auto path = static_cast<char *>(mem_heap_alloc(m_snapshot_heap, alloc_size));

  if (path == nullptr) {
    /* purecov: begin inspected */
    my_error(ER_OUTOFMEMORY, MYF(0), alloc_size);
    return (ER_OUTOFMEMORY);
    /* purecov: end */
  }

  /* Copy file metadata */
  file_ctx = reinterpret_cast<Clone_file_ctx *>(path);
  file_ctx->init(extn);
  path += sizeof(Clone_file_ctx);

  strcpy(path, file_path.c_str());

  auto ctx_file_meta = file_ctx->get_file_meta();
  *ctx_file_meta = *file_meta;

  ctx_file_meta->m_file_name = static_cast<const char *>(path);

  ctx_file_meta->m_file_name_len = file_path.length() + 1;

  ctx_file_meta->m_file_name_alloc_len = ctx_file_meta->m_file_name_len;

  return 0;
}

/** Add directory path to file
@param[in]      dir     directory
@param[in]      file    file name
@param[out]     path    file along with path. */
static void add_directory_path(const char *dir, const char *file,
                               std::string &path) {
  path.clear();
  /* Append directory */
  if (dir != nullptr) {
    path.assign(dir);
    if (path.back() != OS_PATH_SEPARATOR) {
      path.append(OS_PATH_SEPARATOR_STR); /* purecov: inspected */
    }
  }
  /* Append file */
  if (file != nullptr) {
    path.append(file);
  }
}

int Clone_Snapshot::create_desc(const char *data_dir,
                                const Clone_File_Meta *file_meta, bool is_ddl,
                                Clone_file_ctx *&file_ctx) {
  /* Update file path from configuration. */
  std::string file_path;

  auto err = build_file_path(data_dir, file_meta, file_path);

  if (err != 0) {
    return (err);
  }

  auto extn = Clone_file_ctx::Extension::NONE;

  if (is_ddl) {
    extn = Clone_file_ctx::Extension::DDL;

    std::string ddl_list_file;
    add_directory_path(data_dir, CLONE_INNODB_DDL_FILES, ddl_list_file);

    err = clone_add_to_list_file(ddl_list_file.c_str(), file_path.c_str());

  } else {
    /* If data directory is being replaced. */
    bool replace_dir = (data_dir == nullptr);
    bool is_undo_file = fsp_is_undo_tablespace(file_meta->m_space_id);
    bool is_redo_file = file_meta->m_space_id == dict_sys_t::s_log_space_id;

    /* Check if file is already present in recipient. */
    err = handle_existing_file(replace_dir, is_undo_file, is_redo_file,
                               file_meta->m_file_index, file_path, extn);
  }

  if (err == 0) {
    /* Build complete path for the new file to be added. */
    err = build_file_ctx(extn, file_meta, file_path, file_ctx);
  }
  return (err);
}

bool Clone_Snapshot::add_file_from_desc(Clone_file_ctx *&file_ctx,
                                        bool ddl_create) {
  mutex_enter(&m_snapshot_mutex);

  ut_ad(m_snapshot_handle_type == CLONE_HDL_APPLY);
  auto file_meta = file_ctx->get_file_meta();

  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY ||
      m_snapshot_state == CLONE_SNAPSHOT_PAGE_COPY) {
    if (ddl_create) {
      ut_a(file_meta->m_file_index == num_data_files());
      /* Add data file at the end and extend length. */
      m_data_file_vector.push_back(file_ctx);
    } else {
      m_data_file_vector[file_meta->m_file_index] = file_ctx;
    }
  } else {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);
    m_redo_file_vector[file_meta->m_file_index] = file_ctx;
  }

  mutex_exit(&m_snapshot_mutex);

  /** Check if it the last file */
  if (file_meta->m_file_index == num_data_files() - 1) {
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
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Task Descriptor");
    ut_d(ut_error);
    ut_o(return (err));
  }
  task->m_task_meta = task_desc.m_task_meta;
  return (0);
}

int Clone_Handle::check_space(const Clone_Task *task) {
  /* Do space check only during file copy. */
  auto current_state = m_clone_task_manager.get_state();
  if (!task->m_is_master || current_state != CLONE_SNAPSHOT_FILE_COPY) {
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
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid State Descriptor");
    ut_d(ut_error);
    ut_o(return (err));
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
    if (err != 0) {
      return err;
    }
#endif /* UNIV_DEBUG */

    /** Notify state change via callback. */
    notify_state_change(task, callback, &state_desc);

    err = fix_all_renamed(task);

    if (err == 0) {
      err = move_to_next_state(task, nullptr, &state_desc);
    }

#ifdef UNIV_DEBUG
    /* Network failure after moving to new state */
    err = m_clone_task_manager.debug_restart(task, err, 0);
#endif /* UNIV_DEBUG */

    /* Check if enough space available on disk */
    if (err == 0) {
      err = check_space(task);
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
    if (state_desc.m_state == CLONE_SNAPSHOT_FILE_COPY) {
      DEBUG_SYNC_C("clone_file_copy_end_before_ack");
    }
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

int Clone_Handle::ack_state_metadata(Clone_Task *, Ha_clone_cbk *callback,
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

int Clone_Handle::apply_file_delete(Clone_Task *task, Clone_file_ctx *file_ctx,
                                    const Clone_File_Meta *new_meta) {
  auto err = close_file(task);
  if (err != 0) {
    return err; /* purecov: inspected */
  }

  auto file_meta = file_ctx->get_file_meta();

  if (task->m_current_file_index != file_meta->m_file_index) {
    task->m_current_file_index = file_meta->m_file_index;
  }

  auto snapshot = m_clone_task_manager.get_snapshot();

  auto begin_chunk = file_meta->m_begin_chunk;
  auto end_chunk = file_meta->m_end_chunk;
  auto block_num = snapshot->get_blocks_per_chunk();
  auto data_size = snapshot->get_chunk_size();

  /* For page copy, we reset one page of the current chunk passed. Chunks
  in file_meta corresponds to chunk in file copy. */
  if (snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY) {
    begin_chunk = new_meta->m_begin_chunk;
    end_chunk = begin_chunk;
    block_num = 0;
    data_size = UNIV_PAGE_SIZE;
  }

  Clone_Task_Meta new_task_meta = task->m_task_meta;

  /* Consume all chunks of deleted file. */
  for (auto cur_chunk = begin_chunk; cur_chunk <= end_chunk; ++cur_chunk) {
    /* Set current chunk details. */
    new_task_meta.m_chunk_num = cur_chunk;
    new_task_meta.m_block_num = block_num;

    if (m_clone_task_manager.is_chunk_reserved(cur_chunk)) {
      continue;
    }

    m_clone_task_manager.set_chunk(task, &new_task_meta);

    /* Set data size for progress estimation. */
    task->m_data_size = data_size;
  }

  if (!file_ctx->deleted()) {
    file_ctx->m_state.store(Clone_file_ctx::State::DROPPED);
  }

  std::string old_file;
  file_ctx->get_file_name(old_file);

  std::string mesg("FILE : ");
  mesg.append(old_file);
  mesg.append(" Space ID: ");
  mesg.append(std::to_string(file_meta->m_space_id));
  mesg.append(" Chunks : ");
  mesg.append(std::to_string(begin_chunk));
  mesg.append(" - ");
  mesg.append(std::to_string(end_chunk));

  ib::info(ER_IB_MSG_CLONE_DDL_INVALIDATE) << mesg;
  return 0;
}

int Clone_Handle::apply_ddl(const Clone_File_Meta *new_meta,
                            Clone_file_ctx *file_ctx) {
  auto snapshot = m_clone_task_manager.get_snapshot();
  ut_ad(snapshot->get_state() == CLONE_SNAPSHOT_FILE_COPY ||
        snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY);

  std::string old_file;
  file_ctx->get_file_name(old_file);

  std::string mesg("DELETE FILE : ");

  if (new_meta->is_deleted()) {
    /* Check if we have already deleted the file context. This is possible
    in case of a network error and restart where donor could send the delete
    request again. */
    if (file_ctx->m_state.load() == Clone_file_ctx::State::DROPPED_HANDLED) {
      mesg.append(" IGNORE : ");

    } else {
      /* File needs to be deleted. */
      if (!os_file_delete(innodb_clone_file_key, old_file.c_str())) {
        /* purecov: begin inspected */
        mesg.append("Innodb Clone Apply Failed to delete file: ");
        mesg.append(old_file);
        my_error(ER_INTERNAL_ERROR, MYF(0), mesg.c_str());
        return ER_INTERNAL_ERROR;
        /* purecov: end */
      }
      file_ctx->m_state.store(Clone_file_ctx::State::DROPPED_HANDLED);
    }
    mesg.append(old_file);
    mesg.append(" Space ID: ");
    mesg.append(std::to_string(new_meta->m_space_id));

    ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << mesg;
    return 0;
  }

  auto old_meta = file_ctx->get_file_meta();

  /* Check if file needs to be renamed. */
  if (!new_meta->is_renamed()) {
    std::string update_mesg;
    /* Set new encryption and compression type. */
    if (old_meta->m_encryption_metadata.m_type !=
        new_meta->m_encryption_metadata.m_type) {
      old_meta->m_encryption_metadata.m_type =
          new_meta->m_encryption_metadata.m_type;
      if (!new_meta->can_encrypt()) {
        update_mesg.assign("UNENCRYPTED ");
      } else {
        update_mesg.assign("ENCRYPTED ");
      }
    }

    if (old_meta->m_compress_type != new_meta->m_compress_type) {
      old_meta->m_compress_type = new_meta->m_compress_type;
      if (new_meta->m_compress_type == Compression::NONE) {
        update_mesg.assign("UNCOMPRESSED ");
      } else {
        update_mesg.assign("COMPRESSED ");
      }
    }

    auto err = set_compression(file_ctx);

    std::string mesg("SET FILE ");
    mesg.append(update_mesg);
    mesg.append(": ");
    mesg.append(old_file);
    mesg.append(" Space ID: ");
    mesg.append(std::to_string(new_meta->m_space_id));

    ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << mesg;
    return err;
  }

  Clone_file_ctx *new_ctx = nullptr;

  /* Rename file context. */
  auto err = snapshot->rename_desc(new_meta, m_clone_dir, new_ctx);

  if (err != 0) {
    return err; /* purecov: inspected */
  }

  std::string new_file;
  new_ctx->get_file_name(new_file);

  /* Preserve the old file size which could have been extended while applying
  page 0 changes and set it to new descriptor. */
  auto file_meta = new_ctx->get_file_meta();
  auto file_size = file_meta->m_file_size;

  if (file_size < old_meta->m_file_size) {
    file_size = old_meta->m_file_size;
  }
  file_meta->m_file_size = file_size;

  /* Do the actual rename. At this point we rename the files with temp DDL
  extension. After all rename and delete requests are received we rename
  the files again removing the ddl extension. This is required as file rename
  requests are not in the real order and there could be conflicts. */
  ut_ad(new_ctx->m_extension == Clone_file_ctx::Extension::DDL);

  std::string rename_mesg("RENAME FILE WITH EXTN: ");

  if (old_file.compare(new_file) == 0) {
    rename_mesg.append(" IGNORE : ");

  } else {
    bool success =
        os_file_rename(OS_CLONE_DATA_FILE, old_file.c_str(), new_file.c_str());

    if (!success) {
      /* purecov: begin inspected */
      char errbuf[MYSYS_STRERROR_SIZE];
      err = ER_ERROR_ON_RENAME;

      my_error(ER_ERROR_ON_RENAME, MYF(0), old_file.c_str(), new_file.c_str(),
               errno, my_strerror(errbuf, sizeof(errbuf), errno));
      /* purecov: end */
    }
  }

  rename_mesg.append(old_file);
  rename_mesg.append(" to ");
  rename_mesg.append(new_file);
  rename_mesg.append(" Space ID: ");
  rename_mesg.append(std::to_string(new_meta->m_space_id));

  ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << rename_mesg;

  if (err == 0) {
    err = set_compression(new_ctx);
  }
  return err;
}

int Clone_Handle::fix_all_renamed(const Clone_Task *task) {
  /* Do space check only during file copy and page copy. */
  auto current_state = m_clone_task_manager.get_state();

  bool fix_needed = current_state == CLONE_SNAPSHOT_FILE_COPY ||
                    current_state == CLONE_SNAPSHOT_PAGE_COPY;

  if (!task->m_is_master || !fix_needed) {
    return 0;
  }

  auto snapshot = m_clone_task_manager.get_snapshot();

  ut_ad(snapshot->get_state() == CLONE_SNAPSHOT_FILE_COPY ||
        snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY);

  auto fix_func = [&](Clone_file_ctx *file_ctx) {
    /* Need to handle files with DDL extension. */
    if (file_ctx->deleted() ||
        file_ctx->m_extension != Clone_file_ctx::Extension::DDL) {
      return 0;
    }
    /* Save old file name */
    std::string old_file;
    file_ctx->get_file_name(old_file);

    auto err = snapshot->fix_ddl_extension(m_clone_dir, file_ctx);
    if (err != 0) {
      return err; /* purecov: inspected */
    }
    /* Get new file name. */
    std::string new_file;
    file_ctx->get_file_name(new_file);

    /* Rename file */
    bool success =
        os_file_rename(OS_CLONE_DATA_FILE, old_file.c_str(), new_file.c_str());
    if (!success) {
      /* purecov: begin inspected */
      char errbuf[MYSYS_STRERROR_SIZE];
      err = ER_ERROR_ON_RENAME;

      my_error(ER_ERROR_ON_RENAME, MYF(0), old_file.c_str(), new_file.c_str(),
               errno, my_strerror(errbuf, sizeof(errbuf), errno));
      /* purecov: end */
    }

    std::string mesg("RENAMED FILE REMOVED EXTN : ");
    mesg.append(old_file);
    mesg.append(" to ");
    mesg.append(new_file);
    mesg.append(" Space ID: ");
    auto file_meta = file_ctx->get_file_meta_read();
    mesg.append(std::to_string(file_meta->m_space_id));

    ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << mesg;
    return err;
  };

  auto err = snapshot->iterate_data_files(fix_func);

  /* Delete ddl list file. */
  if (err == 0) {
    std::string ddl_list_file;
    add_directory_path(m_clone_dir, CLONE_INNODB_DDL_FILES, ddl_list_file);

    clone_remove_list_file(ddl_list_file.c_str());
  }

  return err;
}

/* Check and set punch hole for compressed page table. */
int Clone_Handle::set_compression(Clone_file_ctx *file_ctx) {
  auto file_meta = file_ctx->get_file_meta();

  if (file_meta->m_compress_type == Compression::NONE || file_ctx->deleted()) {
    return 0;
  }

  /* Disable punch hole if donor compression is not effective. */
  page_size_t page_size(file_meta->m_fsp_flags);

  if (page_size.is_compressed() ||
      file_meta->m_fsblk_size * 2 > srv_page_size) {
    /* purecov: begin inspected */
    file_meta->m_punch_hole = false;
    return 0;
    /* purecov: end */
  }

  os_file_stat_t stat_info;
  std::string file_name;
  file_ctx->get_file_name(file_name);

  os_file_get_status(file_name.c_str(), &stat_info, false, false);

  /* Check and disable punch hole if recipient cannot support it. */
  if (!IORequest::is_punch_hole_supported() ||
      stat_info.block_size * 2 > srv_page_size) {
    file_meta->m_punch_hole = false; /* purecov: inspected */
  } else {
    file_meta->m_punch_hole = true;
  }

  /* Old format for compressed and encrypted page is
  dependent on file system block size. */
  if (file_meta->can_encrypt() &&
      file_meta->m_fsblk_size != stat_info.block_size) {
    /* purecov: begin tested */
    auto donor_str = std::to_string(file_meta->m_fsblk_size);
    auto recipient_str = std::to_string(stat_info.block_size);

    my_error(ER_CLONE_CONFIG, MYF(0), "FS Block Size", donor_str.c_str(),
             recipient_str.c_str());
    return ER_CLONE_CONFIG;
    /* purecov: end */
  }

  return 0;
}

int Clone_Handle::file_create_init(const Clone_file_ctx *file_ctx,
                                   ulint file_type, bool init) {
  /* Create the file and path. */
  File_init_cbk init_cbk = [&](pfs_os_file_t file) {
    if (!init) {
      return DB_SUCCESS;
    }

    bool punch_hole = false;

    std::string file_name;
    file_ctx->get_file_name(file_name);

    const auto file_meta = file_ctx->get_file_meta_read();
    auto flags = file_meta->m_fsp_flags;

    bool is_undo_file = fsp_is_undo_tablespace(file_meta->m_space_id);

    page_no_t size_in_pages =
        is_undo_file ? UNDO_INITIAL_SIZE_IN_PAGES : FIL_IBD_FILE_INITIAL_SIZE;

    byte encryption_info[Encryption::INFO_SIZE];
    byte *encryption_ptr = nullptr;

    dberr_t db_err = DB_SUCCESS;
    std::string mesg("CREATE NEW FILE : ");

    if (file_meta->m_transfer_encryption_key) {
      encryption_ptr = &encryption_info[0];
      mesg.append(" WRITE KEY: ");

      bool success = Encryption::fill_encryption_info(
          file_meta->m_encryption_metadata, true, encryption_info);

      if (!success) {
        db_err = DB_ERROR; /* purecov: inspected */
      }
    }

    if (db_err == DB_SUCCESS) {
      db_err = fil_write_initial_pages(
          file, file_name.c_str(), FIL_TYPE_TABLESPACE, size_in_pages,
          encryption_ptr, file_meta->m_space_id, flags, punch_hole);
    }

    mesg.append(file_name);
    mesg.append(" Space ID: ");
    mesg.append(std::to_string(file_meta->m_space_id));

    if (db_err != DB_SUCCESS) {
      mesg.append(" FAILED"); /* purecov: inspected */
    }

    ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << mesg;

    return db_err;
  };

  auto err = open_file(nullptr, file_ctx, file_type, true, init_cbk);

  return err;
}

int Clone_Handle::apply_file_metadata(Clone_Task *task,
                                      Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  uint desc_len = 0;
  auto serial_desc = callback->get_data_desc(&desc_len);

  Clone_Desc_File_MetaData file_desc;
  auto success = file_desc.deserialize(serial_desc, desc_len);

  if (!success) {
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid File Descriptor");
    ut_d(ut_error);
    ut_o(return (err));
  }
  const auto file_desc_meta = &file_desc.m_file_meta;
  auto snapshot = m_clone_task_manager.get_snapshot();

  /* At end of current state DDL file alterations are communicated. */
  bool ddl_desc = (file_desc.m_state == snapshot->get_next_state());

  ut_ad(ddl_desc || snapshot->get_state() == file_desc.m_state);

  bool file_deleted = file_desc_meta->is_deleted();

  bool desc_exists = false;
  Clone_file_ctx *file_ctx = nullptr;

  /* Check file metadata entry based on the descriptor. */
  auto err = snapshot->get_file_from_desc(file_desc_meta, m_clone_dir, false,
                                          desc_exists, file_ctx);
  if (err != 0) {
    return (err);
  }

  if (desc_exists) {
    if (ddl_desc) {
      err = apply_ddl(file_desc_meta, file_ctx);

    } else if (file_deleted) {
      /* File delete notification sent immediately for chunk adjustment. */
      err = apply_file_delete(task, file_ctx, file_desc_meta);
    }
    return err;
  }

  mutex_enter(m_clone_task_manager.get_mutex());

  /* Create file metadata entry based on the descriptor. */
  err = snapshot->get_file_from_desc(file_desc_meta, m_clone_dir, true,
                                     desc_exists, file_ctx);
  if (err != 0 || desc_exists) {
    mutex_exit(m_clone_task_manager.get_mutex());

    /* Save error with file name. */
    if (err != 0) {
      m_clone_task_manager.set_error(err, file_desc_meta->m_file_name);
    }
    return (err);
  }

  auto file_meta = file_ctx->get_file_meta();
  file_meta->m_punch_hole = false;

  bool is_file_copy = snapshot->get_state() == CLONE_SNAPSHOT_FILE_COPY;
  bool is_page_copy = snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY;

  if (is_file_copy || is_page_copy) {
    ut_ad(is_file_copy || ddl_desc);

    auto file_type = OS_CLONE_DATA_FILE;

    if (file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {
      file_type = OS_CLONE_LOG_FILE;
    }

    if (file_deleted) {
      /* Mark the newly created descriptor deleted. */
      file_ctx->m_state.store(Clone_file_ctx::State::DROPPED_HANDLED);

      std::string file_name;
      file_ctx->get_file_name(file_name);

      std::string mesg("ADD DELETED FILE : ");
      mesg.append(file_name);
      mesg.append(" Space ID: ");
      mesg.append(std::to_string(file_meta->m_space_id));
      ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << mesg;

    } else {
      /* Create the file and write initial pages if created by DDL. */
      err = file_create_init(file_ctx, file_type, ddl_desc);
    }

    /* If last file is received, set all file metadata transferred */
    if (snapshot->add_file_from_desc(file_ctx, ddl_desc)) {
      m_clone_task_manager.set_file_meta_transferred();
    }

    mutex_exit(m_clone_task_manager.get_mutex());

    if (err == 0 && file_type == OS_CLONE_DATA_FILE) {
      err = set_compression(file_ctx);
    }
    return err;
  }

  ut_ad(snapshot->get_state() == CLONE_SNAPSHOT_REDO_COPY);
  ut_ad(file_desc.m_state == CLONE_SNAPSHOT_REDO_COPY);
  ut_ad(!ddl_desc);

  /* open and reserve the redo file size */
  File_init_cbk empty_cbk;

  err = open_file(nullptr, file_ctx, OS_CLONE_LOG_FILE, true, empty_cbk);

  snapshot->add_file_from_desc(file_ctx, false);

  mutex_exit(m_clone_task_manager.get_mutex());
  return (err);
}

bool Clone_Handle::read_compressed_len(unsigned char *buffer, uint32_t len,
                                       uint32_t block_size,
                                       uint32_t &compressed_len) {
  ut_a(len >= 2);

  /* Validate compressed page type */
  auto page_type = mach_read_from_2(buffer + FIL_PAGE_TYPE);

  if (page_type == FIL_PAGE_COMPRESSED ||
      page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
    compressed_len = mach_read_from_2(buffer + FIL_PAGE_COMPRESS_SIZE_V1);
    compressed_len += FIL_PAGE_DATA;

    /* Align compressed length */
    compressed_len = ut_calc_align(compressed_len, block_size);
    return true;
  }

  return false;
}

int Clone_Handle::sparse_file_write(Clone_File_Meta *file_meta,
                                    unsigned char *buffer, uint32_t len,
                                    pfs_os_file_t file, uint64_t start_off) {
  dberr_t err = DB_SUCCESS;
  page_size_t page_size(file_meta->m_fsp_flags);
  auto page_len = page_size.physical();

  IORequest request(IORequest::WRITE);
  request.disable_compression();
  request.clear_encrypted();

  /* Loop through all pages in current data block */
  while (len >= page_len) {
    uint32_t comp_len;
    bool is_compressed = read_compressed_len(
        buffer, len, static_cast<uint32_t>(file_meta->m_fsblk_size), comp_len);

    auto write_len = is_compressed ? comp_len : page_len;

    /* Punch hole if needed */
    bool first_page = (start_off == 0);

    /* In rare case during file copy the page could be a torn page
    and the size may not be correct. In such case the page is going to
    be replaced later during page copy.*/
    if (first_page || write_len > page_len) {
      write_len = page_len;
    }

    /* Write Data Page */
    errno = 0;
    err = os_file_write(request, "Clone data file", file,
                        reinterpret_cast<char *>(buffer), start_off,
                        (start_off == 0) ? page_len : write_len);
    if (err != DB_SUCCESS) {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(ER_ERROR_ON_WRITE, MYF(0), file_meta->m_file_name, errno,
               my_strerror(errbuf, sizeof(errbuf), errno));

      return (ER_ERROR_ON_WRITE);
    }

    os_offset_t offset = start_off + write_len;
    os_offset_t hole_size = page_len - write_len;

    if (file_meta->m_punch_hole && hole_size > 0) {
      err = os_file_punch_hole(file.m_file, offset, hole_size);
      if (err != DB_SUCCESS) {
        /* Disable for whole file */
        file_meta->m_punch_hole = false;
        ut_ad(err == DB_IO_NO_PUNCH_HOLE);
        ib::info(ER_IB_CLONE_PUNCH_HOLE)
            << "Innodb Clone Apply failed to punch hole: "
            << file_meta->m_file_name;
      }
    }

    start_off += page_len;
    buffer += page_len;
    len -= page_len;
  }

  /* Must have consumed all data. */
  ut_ad(err != DB_SUCCESS || len == 0);
  return 0;
}

int Clone_Handle::modify_and_write(const Clone_Task *task, uint64_t offset,
                                   unsigned char *buffer, uint32_t buf_len) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  auto snapshot = m_clone_task_manager.get_snapshot();
  auto file_meta = snapshot->get_file_by_index(task->m_current_file_index);

  if (file_meta->can_encrypt()) {
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
      int err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Innodb Clone Apply Failed to Encrypt Key");
      ut_d(ut_error);
      ut_o(return (err));
    }
  }

  if (file_meta->m_punch_hole) {
    auto err = sparse_file_write(file_meta, buffer, buf_len,
                                 task->m_current_file_des, offset);
    return err;
  }

  /* No more compression/encryption is needed. */
  IORequest request(IORequest::WRITE);
  request.disable_compression();
  request.clear_encrypted();

  /* For redo/undo log files and uncompressed tables ,directly write to file */
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
  return 0;
}

int Clone_Handle::receive_data(Clone_Task *task, uint64_t offset,
                               uint64_t file_size, uint32_t size,
                               Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  auto snapshot = m_clone_task_manager.get_snapshot();

  auto file_ctx = snapshot->get_file_ctx_by_index(task->m_current_file_index);
  auto file_meta = file_ctx->get_file_meta();

  std::string file_name;
  file_ctx->get_file_name(file_name);

  /* If the file is deleted, then fetch the data and ignore. */
  if (file_ctx->deleted()) {
    unsigned char *data_buf = nullptr;
    uint32_t data_len = 0;
    callback->apply_buffer_cbk(data_buf, data_len);

    std::string mesg("IGNORE DATA for DELETED FILE: ");
    mesg.append(file_name);
    mesg.append(" Space ID: ");
    mesg.append(std::to_string(file_meta->m_space_id));

    ib::info(ER_IB_MSG_CLONE_DDL_APPLY) << mesg;
    return 0;
  }

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
      file_meta->m_space_id == dict_sys_t::s_invalid_space_id ||
      file_meta->m_punch_hole) {
    file_type = OS_CLONE_LOG_FILE;
  }

  /* Open destination file for first block. */
  if (task->m_current_file_des.m_file == OS_FILE_CLOSED) {
    ut_ad(file_meta != nullptr);

    File_init_cbk empty_cbk;
    auto err = open_file(task, file_ctx, file_type, true, empty_cbk);

    if (err != 0) {
      /* Save error with file name. */
      /* purecov: begin inspected */
      m_clone_task_manager.set_error(err, file_name.c_str());
      return (err);
      /* purecov: end */
    }
  }

  ut_ad(task->m_current_file_index == file_meta->m_file_index);

  /* Copy data to current destination file using callback. */
  char errbuf[MYSYS_STRERROR_SIZE];

  auto file_hdl = task->m_current_file_des.m_file;
  auto success = os_file_seek(nullptr, file_hdl, offset);

  if (!success) {
    /* purecov: begin inspected */
    my_error(ER_ERROR_ON_READ, MYF(0), file_name.c_str(), errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    /* Save error with file name. */
    m_clone_task_manager.set_error(ER_ERROR_ON_READ, file_name.c_str());
    return (ER_ERROR_ON_READ);
    /* purecov: end */
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
  if (file_meta->can_encrypt() && (key_page || key_log)) {
    modify_buffer = true;
  }
  auto err = file_callback(callback, task, size, modify_buffer, offset
#ifdef UNIV_PFS_IO
                           ,
                           UT_LOCATION_HERE
#endif /* UNIV_PFS_IO */
  );

  task->m_data_size += size;

  if (err != 0) {
    /* Save error with file name. */
    /* purecov: begin inspected */
    m_clone_task_manager.set_error(err, file_name.c_str());
    /* purecov: end */
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
    int err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Data Descriptor");
    ut_d(ut_error);
    ut_o(return (err));
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

int Clone_Handle::apply(THD *, uint task_id, Ha_clone_cbk *callback) {
  int err = 0;
  uint desc_len = 0;

  auto clone_desc = callback->get_data_desc(&desc_len);
  ut_ad(clone_desc != nullptr);

  Clone_Desc_Header header;
  auto success = header.deserialize(clone_desc, desc_len);

  if (!success) {
    err = ER_CLONE_PROTOCOL;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Descriptor Header");
    ut_d(ut_error);
    ut_o(return (err));
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
      ut_d(ut_error);
      ut_o(break);
  }

  if (err != 0) {
    close_file(task);
  }

  return (err);
}

int Clone_Handle::restart_apply(THD *, const byte *&loc, uint &loc_len) {
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

    if (file_index >= num_data_files()) {
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
  IB_mutex_guard guard(&m_snapshot_mutex, UT_LOCATION_HERE);

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
      err = ER_INTERNAL_ERROR;
      my_error(err, MYF(0), "Innodb Clone Snapshot Invalid state");
      ut_d(ut_error);
      ut_o(break);
  }
  return (err);
}

int Clone_Snapshot::extend_and_flush_files(bool flush_redo) {
  auto &file_vector = (flush_redo) ? m_redo_file_vector : m_data_file_vector;

  for (auto file_ctx : file_vector) {
    if (file_ctx->deleted()) {
      ut_ad(file_ctx->m_state.load() == Clone_file_ctx::State::DROPPED_HANDLED);
      continue;
    }
    char errbuf[MYSYS_STRERROR_SIZE];
    bool success = true;
    auto file_meta = file_ctx->get_file_meta();

    std::string file_name;
    file_ctx->get_file_name(file_name);

    auto file = os_file_create(
        innodb_clone_file_key, file_name.c_str(),
        OS_FILE_OPEN | OS_FILE_ON_ERROR_NO_EXIT,
        flush_redo ? OS_CLONE_LOG_FILE : OS_CLONE_DATA_FILE, false, &success);

    if (!success) {
      /* purecov: begin inspected */
      my_error(ER_CANT_OPEN_FILE, MYF(0), file_name.c_str(), errno,
               my_strerror(errbuf, sizeof(errbuf), errno));

      return (ER_CANT_OPEN_FILE);
      /* purecov: end */
    }

    auto file_size = os_file_get_size(file);

    size_t aligned_size = 0;
    /* If file size is not aligned to extent size, recovery handling has
    some issues. This work around eliminates dependency with that. */
    if (file_meta->m_fsp_flags != UINT32_UNDEFINED) {
      page_size_t page_size(file_meta->m_fsp_flags);
      auto extent_size = page_size.physical() * FSP_EXTENT_SIZE;
      /* Skip extending files smaller than one extent. */
      if (file_size > extent_size) {
        aligned_size = ut_uint64_align_up(file_size, extent_size);
      }
    }

    if (file_size < file_meta->m_file_size) {
      success = os_file_set_size(file_name.c_str(), file, file_size,
                                 file_meta->m_file_size, true);
    } else if (file_size < aligned_size) {
      success = os_file_set_size(file_name.c_str(), file, file_size,
                                 aligned_size, true);
    } else {
      success = os_file_flush(file);
    }

    os_file_close(file);

    if (!success) {
      /* purecov: begin inspected */
      my_error(ER_ERROR_ON_WRITE, MYF(0), file_name.c_str(), errno,
               my_strerror(errbuf, sizeof(errbuf), errno));

      return (ER_ERROR_ON_WRITE);
      /* purecov: end */
    }
  }
  return (0);
}
