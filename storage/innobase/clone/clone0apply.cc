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

/** @file clone/clone0apply.cc
 Innodb apply snapshot data

 *******************************************************/

#include "clone0clone.h"
#include "dict0dict.h"
#include "handler.h"
#include "log0log.h"

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

int Clone_Snapshot::create_desc(const char *data_dir,
                                Clone_File_Meta *&file_desc) {
  /* Build complete path for the new file to be added. */
  auto dir_len = static_cast<ulint>(strlen(data_dir));

  auto name_len = static_cast<ulint>((file_desc->m_file_name == nullptr)
                                         ? MAX_LOG_FILE_NAME
                                         : file_desc->m_file_name_len);

  auto alloc_size = static_cast<ulint>(dir_len + 1 + name_len);
  alloc_size += sizeof(Clone_File_Meta);

  auto ptr = static_cast<char *>(mem_heap_alloc(m_snapshot_heap, alloc_size));

  if (ptr == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(0), alloc_size);
    return (ER_OUTOFMEMORY);
  }

  auto file_meta = reinterpret_cast<Clone_File_Meta *>(ptr);
  ptr += sizeof(Clone_File_Meta);

  *file_meta = *file_desc;

  file_meta->m_file_name = static_cast<const char *>(ptr);
  name_len = 0;

  strcpy(ptr, data_dir);

  /* Add path separator at the end of data directory if not there. */
  if (ptr[dir_len - 1] != OS_PATH_SEPARATOR) {
    ptr[dir_len] = OS_PATH_SEPARATOR;
    ptr++;
    name_len++;
  }
  ptr += dir_len;
  name_len += dir_len;

  std::string name;
  char name_buf[MAX_LOG_FILE_NAME];
  bool absolute_path = false;

  /* Construct correct file path */
  if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {
    name.assign(file_desc->m_file_name);
    absolute_path = Fil_path::is_absolute_path(name);

    if (absolute_path) {
      /* Set current pointer back as we don't want to append data directory
      for external files with absolute path. */
      ptr = const_cast<char *>(file_meta->m_file_name);
      name_len = 0;
    } else {
      /* For relative path remove "./" if there. */
      if (Fil_path::has_prefix(name, Fil_path::DOT_SLASH)) {
        name.erase(0, 2);
      }
    }
  } else {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);
    /* This is redo file. Use standard name. */
    snprintf(name_buf, MAX_LOG_FILE_NAME, "%s%u", ib_logfile_basename,
             file_desc->m_file_index);
    name.assign(name_buf);
  }

  strcpy(ptr, name.c_str());
  name_len += name.length();
  ++name_len;

  file_meta->m_file_name_len = name_len;
  file_desc = file_meta;

  /* For absolute path, we must ensure that the file is not
  present. This would always fail for local clone. */
  if (absolute_path) {
    ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY);

    auto is_hard_path = test_if_hard_path(file_desc->m_file_name);
    /* Check if the absolute path is not in right format */
    if (is_hard_path == 0) {
      my_error(ER_WRONG_VALUE, MYF(0), "file path", name.c_str());
      return (ER_WRONG_VALUE);
    }

    auto type = Fil_path::get_file_type(name);
    /* The file should not already exist */
    if (type == OS_FILE_TYPE_MISSING) {
      return (0);
    }
    if (type == OS_FILE_TYPE_FILE) {
      my_error(ER_FILE_EXISTS_ERROR, MYF(0), name.c_str());
      return (ER_FILE_EXISTS_ERROR);
    }
    /* Either the stat() call failed or the name is a
    directory/block device, or permission error etc. */
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_ERROR_ON_WRITE, MYF(0), name.c_str(), errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    return (ER_ERROR_ON_WRITE);
  }
  return (0);
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
    int err = ER_CLONE_PROTOCOL_ERROR;
    my_error(err, MYF(0), "Wrong Clone RPC: Invalid Task Descriptor");
    return (err);
  }
  task->m_task_meta = task_desc.m_task_meta;
  return (0);
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
    err = ER_CLONE_PROTOCOL_ERROR;
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

    err = move_to_next_state(task, nullptr, &state_desc);

#ifdef UNIV_DEBUG
    /* Network failure after moving to new state */
    err = m_clone_task_manager.debug_restart(task, err, 0);
#endif /* UNIV_DEBUG */

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
      ib::info(ER_IB_MSG_151)
          << "Clone Apply Master ACK finshed state: " << state_desc.m_state;
    }
  }

#ifdef UNIV_DEBUG
  /* Network failure after sending ACK */
  err = m_clone_task_manager.debug_restart(task, err, 4);
#endif /* UNIV_DEBUG */

  return (err);
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
    int err = ER_CLONE_PROTOCOL_ERROR;
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

    if (err == 0 && !desc_exists) {
      err = open_file(nullptr, file_meta, OS_CLONE_LOG_FILE, true, true);
      snapshot->add_file_from_desc(file_meta);
    }
  }

  mutex_exit(m_clone_task_manager.get_mutex());
  return (err);
}

int Clone_Handle::receive_data(Clone_Task *task, uint64_t offset,
                               uint64_t file_size, uint32_t size,
                               Ha_clone_cbk *callback) {
  ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

  auto snapshot = m_clone_task_manager.get_snapshot();

  auto file_meta = snapshot->get_file_by_index(task->m_current_file_index);

  /* Check and update file size for space header page */
  if (snapshot->get_state() == CLONE_SNAPSHOT_PAGE_COPY && offset == 0 &&
      file_meta->m_file_size < file_size) {
    snapshot->update_file_size(task->m_current_file_index, file_size);
  }

  auto file_type = OS_CLONE_DATA_FILE;
  bool is_log_file = (snapshot->get_state() == CLONE_SNAPSHOT_REDO_COPY);

  if (is_log_file || file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {
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

  auto err = file_callback(callback, task, size
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
    int err = ER_CLONE_PROTOCOL_ERROR;
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
    err = ER_CLONE_PROTOCOL_ERROR;
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
      ib::info(ER_IB_MSG_151) << "Clone Apply State FILE COPY: ";
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      ib::info(ER_IB_MSG_152) << "Clone Apply State PAGE COPY: ";
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      ib::info(ER_IB_MSG_152) << "Clone Apply State REDO COPY: ";
      break;

    case CLONE_SNAPSHOT_DONE:
      /* Extend and flush data files. */
      ib::info(ER_IB_MSG_153) << "Clone Apply State FLUSH DATA: ";
      err = extend_and_flush_files(false);
      if (err != 0) {
        ib::info(ER_IB_MSG_153)
            << "Clone Apply FLUSH DATA failed code: " << err;
        break;
      }
      /* Flush redo files. */
      ib::info(ER_IB_MSG_153) << "Clone Apply State FLUSH REDO: ";
      err = extend_and_flush_files(true);
      if (err != 0) {
        ib::info(ER_IB_MSG_153)
            << "Clone Apply FLUSH REDO failed code: " << err;
        break;
      }
      ib::info(ER_IB_MSG_154) << "Clone Apply State DONE";
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
