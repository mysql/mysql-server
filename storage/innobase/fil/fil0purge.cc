/*****************************************************************************

Copyright (c) 1995, 2019, Alibaba and/or its affiliates. All Rights Reserved.

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

/** @file fil/fil0purge.cc
 Implementation of data file purge operation.

 Created  1/11/2019 Jianwei.zhao
 *******************************************************/

#include "fil0purge.h"
#include "fil0fil.h"
#include "os0file.h"
#include "row0mysql.h"
#include "srv0file.h"
#include "ut0mutex.h"

/* Global file purge system */
File_purge *file_purge_sys = nullptr;

/** Constructor */
File_purge::File_purge(ulint thread_id, time_t start_time)
    : m_thread_id(thread_id),
      m_start_time(start_time),
      m_id(0),
      m_dir(nullptr) {
  mutex_create(LATCH_ID_FILE_PURGE_LIST, &m_mutex);
  UT_LIST_INIT(m_list, &file_purge_node_t::LIST);
}

File_purge::~File_purge() {
  /**
    Clear the file nodes, those will be purged again when DDL log recovery after
    reboot
 */
  for (file_purge_node_t *node = UT_LIST_GET_FIRST(m_list); node != nullptr;
       node = UT_LIST_GET_FIRST(m_list)) {
    if (node->m_file_path) ut_free(node->m_file_path);
    UT_LIST_REMOVE(m_list, node);
    ut_free(node);
  }

  mutex_free(&m_mutex);
}

/* Get next unique id number */
ulint File_purge::next_id() {
  ulint number;
  number = m_id++;
  return number;
}
/**
  Add file into purge list

  @param[in]    id        Log DDL record id
  @param[int]   path      The temporary file name

  @retval       false     Success
  @retval       true      Failure
*/
bool File_purge::add_file(ulint id, const char *path) {
  bool success = false;

  pfs_os_file_t handle = os_file_create_simple_no_error_handling(
      innodb_data_file_key, path, OS_FILE_OPEN, OS_FILE_READ_WRITE, FALSE,
      &success);

  if (!success) {
    ib::error(ER_IB_MSG_392) << "Cannot open temp data file for read-write: '"
                             << path << "'"
                             << " when add file into purge list";

    /** Temp file maybe has been purged */
    ut_free(const_cast<char *>(path));
    log_ddl->remove_by_id(id);
    return true;
  }

  os_file_close(handle);

  file_purge_node_t *node = static_cast<file_purge_node_t *>(
      ut_malloc_nokey(sizeof(file_purge_node_t)));

  node->m_log_ddl_id = id;
  node->m_file_path = const_cast<char *>(path);

  mutex_enter(&m_mutex);
  UT_LIST_ADD_LAST(m_list, node);
  mutex_exit(&m_mutex);

  /* Wake up background thread */
  srv_wakeup_file_purge_thread();

  if (srv_print_data_file_purge_process)
    ib::info(ER_IB_MSG_FILE_PURGE) << "File purge add file : " << id << ";"
                                   << path;
  return false;
}

/**
  Purge the first file node by purge max size.

  @param[in]  size        Purge max size (bytes)
  @param[in]  force       Whether unlink file directly

  @retval     -1          Error
  @retval     0           Success & no file purge
  @retval     >0          How many purged operation
*/
int File_purge::purge_file(ulint size, bool force) {
  bool success = false;
  uint truncated = 0;
  file_purge_node_t *node = nullptr;
  pfs_os_file_t handle;

  mutex_enter(&m_mutex);
  node = UT_LIST_GET_FIRST(m_list);
  mutex_exit(&m_mutex);

  if (node) {
    handle = os_file_create_simple_no_error_handling(
        innodb_data_file_key, node->m_file_path, OS_FILE_OPEN,
        OS_FILE_READ_WRITE, FALSE, &success);
    if (!success) {
      ulint err = os_file_get_last_error(true);
      ib::error(ER_IB_MSG_392) << "Cannot open temp data file for read-write: '"
                               << node->m_file_path << "'"
                               << " when purge file from list";
      /* Maybe delete by DBA, so remove file directly */
      if (err == OS_FILE_NOT_FOUND)
        remove_file(node);

      return -1;
    }

    if (srv_print_data_file_purge_process)
      ib::info(ER_IB_MSG_FILE_PURGE) << "File purge purge file : "
                                     << node->m_file_path;

    os_offset_t file_size = os_file_get_size(handle);
    if (!force && file_size > size) {
      /* Truncate predefined size once */
      os_file_truncate(node->m_file_path, handle, size);
      os_file_close(handle);
      truncated++;
    } else {
      /* Direct delete the file when file_size < predefined size */
      os_file_close(handle);
      os_file_delete(innodb_data_file_key, node->m_file_path);
      remove_file(node);
      truncated++;
    }
  } else {
    return 0;
  }

  return truncated;
}

/**
  The data file list length

  @retval       >=0
*/
ulint File_purge::length() {
  ulint len;
  mutex_enter(&m_mutex);
  len = UT_LIST_GET_LEN(m_list);
  mutex_exit(&m_mutex);

  return len;
}
/**
  Purge all the data file cached in list.

  @param[in]    size      Purge max size (bytes)
  @param[in]    force     Purge little by little
                          or unlink directly.
*/
void File_purge::purge_all(ulint size, bool force) {
  while (length() > 0) {
    purge_file(size, force);
  }
}
/**
  Remove the file node from list

  @param[in]    node      File purge node pointer
*/
void File_purge::remove_file(file_purge_node_t *node) {
  ulint log_id = node->m_log_ddl_id;

  mutex_enter(&m_mutex);
  UT_LIST_REMOVE(m_list, node);
  mutex_exit(&m_mutex);

#ifdef UNIV_DEBUG
  bool exist;
  os_file_type_t type;
  os_file_status(node->m_file_path, &exist, &type);
  ut_ad(exist == false);
#endif

  if (srv_print_data_file_purge_process)
    ib::info(ER_IB_MSG_FILE_PURGE) << "File purge remove file : "
                                   << node->m_file_path;
  ut_free(node->m_file_path);
  ut_free(node);

  log_ddl->remove_by_id(log_id);
}
/**
  Generate a unique temporary file name.

  @param[in]    filepath      Original file name

  @retval       file name     Generated file name
*/
char *File_purge::generate_file(const char *filepath) {
  std::string new_path;

  new_path.assign(get_dir());

  std::string temp_filename;
  temp_filename.assign(PREFIX);
  temp_filename.append(std::to_string((ulong)m_start_time));
  temp_filename.append("_");
  temp_filename.append(std::to_string(next_id()));

  char *new_file = Fil_path::make(new_path, temp_filename, NO_EXT, false);

  if (srv_print_data_file_purge_process)
    ib::info(ER_IB_MSG_FILE_PURGE) << "File purge generate file : " << filepath
                                   << ";" << new_file;
  return new_file;
}

/**
  Drop a single-table tablespace and rename the data file as temporary file.
  This deletes the fil_space_t if found and rename the file on disk.

  @param[in]      space_id      Tablespace id
  @param[in]      filepath      File path of tablespace to delete

  @retval         error code */
dberr_t row_purge_single_table_tablespace(space_id_t space_id,
                                          const char *filepath) {
  dberr_t err = DB_SUCCESS;
  uint64_t log_id;

  char *new_filepath = file_purge_sys->generate_file(filepath);

  log_ddl->write_purge_file_log(&log_id, file_purge_sys->get_thread_id(),
                                new_filepath);

  if (srv_print_data_file_purge_process)
    ib::info(ER_IB_MSG_FILE_PURGE) << "File purge write log : " << log_id << ";"
                                   << new_filepath;

  if (!fil_space_exists_in_mem(space_id, nullptr, true, false, NULL, 0)) {
    if (fil_purge_file(filepath, new_filepath)) {
      ib::info(ER_IB_MSG_989) << "Purge data file " << filepath;
    }
  } else {
    err = fil_delete_tablespace(space_id, BUF_REMOVE_FLUSH_NO_WRITE,
                                new_filepath);
  }

  file_purge_sys->add_file(log_id, new_filepath);

  return err;
}

/**
  Rename the ibd data file and delete the releted files

  @param[in]      old_filepath  The original data file
  @param[in]      new_filepath  The new data file

  @retval         TRUE         Success
  @retval         FALSE        Failure
*/
bool fil_purge_file(const char *old_filepath, const char *new_filepath) {
  bool success = true;
  bool exist;
  os_file_type_t type;

  /* If old file has been renamed or dropped, just skip it */
  os_file_status(old_filepath, &exist, &type);

  if (exist)
    success = os_file_rename(innodb_data_file_key, old_filepath, new_filepath);

  char *cfg_filepath = Fil_path::make_cfg(old_filepath);

  if (cfg_filepath != nullptr) {
    os_file_delete_if_exists(innodb_data_file_key, cfg_filepath, nullptr);

    ut_free(cfg_filepath);
  }

  char *cfp_filepath = Fil_path::make_cfp(old_filepath);

  if (cfp_filepath != nullptr) {
    os_file_delete_if_exists(innodb_data_file_key, cfp_filepath, nullptr);

    ut_free(cfp_filepath);
  }

  return (success);
}

/**
  Drop or purge single table tablespace

  @param[in]    space_id      tablespace id
  @param[in]    filepath      tablespace data file path


  @retval     DB_SUCCESS or error
*/
dberr_t row_drop_or_purge_single_table_tablespace(space_id_t space_id,
                                                  const char *filepath) {
  if (srv_data_file_purge)
    return row_purge_single_table_tablespace(space_id, filepath);
  else
    return row_drop_tablespace(space_id, filepath);
}
