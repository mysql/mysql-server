/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
@file clone/src/clone_local.cc
Clone Plugin: Local clone implementation

*/

#include "plugin/clone/include/clone_local.h"
#include "plugin/clone/include/clone_os.h"

#include "sql/sql_thd_internal_api.h"

/* Namespace for all clone data types */
namespace myclone {

/** Start concurrent clone  operation.
@param[in]	share	shared client information
@param[in]	server	shared server handle
@param[in]	index	index of current thread */
static void clone_local(Client_Share *share, Server *server, uint32_t index) {
  THD *thd = nullptr;

  /* Create a session statement and set PFS keys */
  mysql_service_clone_protocol->mysql_clone_start_statement(
      thd, clone_local_thd_key, PSI_NOT_INSTRUMENTED);

  Local clone_inst(thd, server, share, index, false);

  /* Worker task has already reported the error. We ignore any error
  returned  here. */
  static_cast<void>(clone_inst.clone_exec());

  /* Drop the statement and session */
  mysql_service_clone_protocol->mysql_clone_finish_statement(thd);
}

Local::Local(THD *thd, Server *server, Client_Share *share, uint32_t index,
             bool is_master)
    : m_clone_server(server), m_clone_client(thd, share, index, is_master) {}

int Local::clone() {
  /* Begin PFS state if no concurrent clone in progress. */
  auto err = m_clone_client.pfs_begin_state();
  if (err != 0) {
    return (err);
  }

  /* Move to first stage. */
  m_clone_client.pfs_change_stage(0);

  /* Execute clone */
  err = clone_exec();

  /* End PFS table state. */
  const char *err_mesg = nullptr;
  uint32_t err_number = 0;
  auto thd = m_clone_client.get_thd();

  mysql_service_clone_protocol->mysql_clone_get_error(thd, &err_number,
                                                      &err_mesg);
  m_clone_client.pfs_end_state(err_number, err_mesg);
  return (err);
}

int Local::clone_exec() {
  auto thd = m_clone_client.get_thd();
  auto dir_name = m_clone_client.get_data_dir();
  auto is_master = m_clone_client.is_master();
  auto acquire_backup_lock = (is_master && clone_block_ddl);
  auto num_workers = m_clone_client.get_max_concurrency() - 1;

  auto &client_vector = m_clone_client.get_storage_vector();
  auto &client_tasks = m_clone_client.get_task_vector();
  auto &server_vector = m_clone_server->get_storage_vector();

  Task_Vector server_tasks;
  server_tasks.reserve(MAX_CLONE_STORAGE_ENGINE);

  /* Acquire DDL lock. Wait for 5 minutes by default. */
  if (acquire_backup_lock) {
    auto failed = mysql_service_mysql_backup_lock->acquire(
        thd, BACKUP_LOCK_SERVICE_DEFAULT, clone_ddl_timeout);

    if (failed) {
      return (ER_LOCK_WAIT_TIMEOUT);
    }
  }

  auto begin_mode = is_master ? HA_CLONE_MODE_START : HA_CLONE_MODE_ADD_TASK;

  /* Begin clone copy from source. */
  auto error = hton_clone_begin(thd, server_vector, server_tasks,
                                HA_CLONE_HYBRID, begin_mode);

  if (error != 0) {
    /* Release DDL lock */
    if (acquire_backup_lock) {
      mysql_service_mysql_backup_lock->release(thd);
    }
    return (error);
  }

  /* Spawn parallel threads for clone */
  if (is_master) {
    /* Copy Server locators to Client. */
    client_vector = server_vector;

    /* Begin clone apply to destination. */
    error = hton_clone_apply_begin(thd, dir_name, client_vector, client_tasks,
                                   begin_mode);

    if (error != 0) {
      hton_clone_end(thd, server_vector, server_tasks, error);

      /* Release DDL lock */
      if (acquire_backup_lock) {
        mysql_service_mysql_backup_lock->release(thd);
      }
      return (error);
    }

    /* Spawn concurrent client tasks if auto tuning is OFF. */
    if (!clone_autotune_concurrency) {
      /* Limit number of workers based on other configurations. */
      auto to_spawn = m_clone_client.limit_workers(num_workers);
      using namespace std::placeholders;
      auto func = std::bind(clone_local, _1, m_clone_server, _2);
      m_clone_client.spawn_workers(to_spawn, func);
    }

  } else {
    /* Begin clone apply to destination. For auxiliary threads,
    use server storage locator with current copy state.
    1. Auxiliary threads don't overwrite the locator in apply begin
    2. Auxiliary threads must wait for apply state to reach
    copy state */
    error = hton_clone_apply_begin(thd, dir_name, server_vector, client_tasks,
                                   begin_mode);
    if (error != 0) {
      hton_clone_end(thd, server_vector, server_tasks, error);
      return (error);
    }
  }

  Ha_clone_cbk *clone_callback = new Local_Callback(this);

  auto buffer_size = m_clone_client.limit_buffer(clone_buffer_size);
  clone_callback->set_client_buffer_size(buffer_size);

  /* Copy data from source and apply to destination. */
  error = hton_clone_copy(thd, server_vector, server_tasks, clone_callback);

  delete clone_callback;

  /* Wait for concurrent tasks to finish */
  m_clone_client.wait_for_workers();

  /* End clone apply to destination. */
  hton_clone_apply_end(thd, client_vector, client_tasks, error);

  /* End clone copy from source. */
  hton_clone_end(thd, server_vector, server_tasks, error);

  /* Release DDL lock */
  if (acquire_backup_lock) {
    mysql_service_mysql_backup_lock->release(thd);
  }
  return (error);
}

int Local_Callback::file_cbk(Ha_clone_file from_file, uint len) {
  assert(!m_apply_data);

  /* Set source file to external handle of "Clone Client". */
  auto ext_link = get_client_data_link();

  ext_link->set_file(from_file, len);

  auto error = apply_data();

  return (error);
}

int Local_Callback::buffer_cbk(uchar *from_buffer, uint buf_len) {
  int error = 0;

  if (m_apply_data) {
    /* Acknowledge data transfer while in apply phase */
    error = apply_ack();
    return (error);
  }

  /* Set source buffer to external handle of "Clone Client". */
  auto ext_link = get_client_data_link();

  ext_link->set_buffer(from_buffer, buf_len);

  error = apply_data();

  return (error);
}

int Local_Callback::apply_ack() {
  assert(m_apply_data);

  auto client = get_clone_client();

  uint64_t data_estimate = 0;
  /* Check and update PFS table while beginning state. */
  if (is_state_change(data_estimate)) {
    client->pfs_change_stage(data_estimate);
    return (0);
  }

  /* Update and reset statistics information at state end. */
  client->update_stat(true);

  uint loc_len = 0;

  auto hton = get_hton();

  auto server = get_clone_server();

  auto thd = server->get_thd();
  auto server_loc = server->get_locator(get_loc_index(), loc_len);

  /* Use master task ID = 0 */
  auto error = hton->clone_interface.clone_ack(hton, thd, server_loc, loc_len,
                                               0, 0, this);

  return (error);
}

int Local_Callback::apply_data() {
  uint loc_len = 0;

  auto client = get_clone_client();
  auto client_loc = client->get_locator(get_loc_index(), loc_len);

  auto hton = get_hton();
  auto thd = client->get_thd();

  /* Check and abort, if killed */
  if (thd_killed(thd)) {
    if (client->is_master()) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
    }

    return (ER_QUERY_INTERRUPTED);
  }

  auto &task_vector = client->get_task_vector();

  assert(get_loc_index() < task_vector.size());
  auto task_id = task_vector[get_loc_index()];

  /* Call storage engine to apply the data. */
  assert(!m_apply_data);
  m_apply_data = true;

  auto error = hton->clone_interface.clone_apply(hton, thd, client_loc, loc_len,
                                                 task_id, 0, this);

  m_apply_data = false;

  return (error);
}

int Local_Callback::apply_buffer_cbk(uchar *&to_buffer, uint &len) {
  Ha_clone_file dummy_file;
  dummy_file.type = Ha_clone_file::FILE_HANDLE;
  dummy_file.file_handle = nullptr;
  return (apply_cbk(dummy_file, false, to_buffer, len));
}

int Local_Callback::apply_file_cbk(Ha_clone_file to_file) {
  uchar *bufp = nullptr;
  uint buf_len = 0;
  return (apply_cbk(to_file, true, bufp, buf_len));
}

int Local_Callback::apply_cbk(Ha_clone_file to_file, bool apply_file,
                              uchar *&to_buffer, uint &to_len) {
  int error;

  assert(m_apply_data);

  auto client = get_clone_client();
  auto server = get_clone_server();
  auto &info = client->get_thread_info();

  /* Update statistics. */
  auto num_workers = client->update_stat(false);

  /* Spawn new concurrent client tasks, if needed. */
  using namespace std::placeholders;
  auto func = std::bind(clone_local, _1, server, _2);
  client->spawn_workers(num_workers, func);

  auto ext_link = get_client_data_link();

  auto dest_type = ext_link->get_type();

  if (dest_type == CLONE_HANDLE_BUFFER) {
    auto from_buf = ext_link->get_buffer();

    /* Assert alignment to CLONE_OS_ALIGN for O_DIRECT */
    assert(is_os_buffer_cache() ||
           from_buf->m_buffer == clone_os_align(from_buf->m_buffer));

    if (apply_file) {
      error = clone_os_copy_buf_to_file(from_buf->m_buffer, to_file,
                                        from_buf->m_length, get_dest_name());
    } else {
      error = 0;
      to_buffer = from_buf->m_buffer;
      to_len = static_cast<uint>(from_buf->m_length);
    }

    info.update(from_buf->m_length, 0);

  } else {
    assert(dest_type == CLONE_HANDLE_FILE);
    uchar *buf_ptr;
    uint buf_len;

    if (is_os_buffer_cache() && is_zero_copy() &&
        clone_os_supports_zero_copy()) {
      buf_ptr = nullptr;
      buf_len = 0;
    } else {
      /* For direct IO use client buffer. */
      buf_len = client->limit_buffer(clone_buffer_size);
      buf_ptr = client->get_aligned_buffer(buf_len);

      if (buf_ptr == nullptr) {
        return (ER_OUTOFMEMORY);
      }
    }

    auto from_file = ext_link->get_file();

    if (apply_file) {
      error = clone_os_copy_file_to_file(from_file->m_file_desc, to_file,
                                         from_file->m_length, buf_ptr, buf_len,
                                         get_source_name(), get_dest_name());
    } else {
      to_len = from_file->m_length;
      to_buffer = client->get_aligned_buffer(to_len);
      if (to_buffer == nullptr) {
        return (ER_OUTOFMEMORY); /* purecov: inspected */
      }

      error = clone_os_copy_file_to_buf(from_file->m_file_desc, to_buffer,
                                        to_len, get_source_name());
    }
    info.update(from_file->m_length, 0);
  }

  /* Check limits and throttle if needed. */
  client->check_and_throttle();

  return (error);
}
}  // namespace myclone
