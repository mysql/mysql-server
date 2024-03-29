/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/** @file clone/clone0clone.cc
 Innodb Clone System

 *******************************************************/

#include "clone0clone.h"
#include <string>
#ifdef UNIV_DEBUG
#include "current_thd.h" /* current_thd */
#include "debug_sync.h"  /* DBUG_SIGNAL_WAIT_FOR */
#endif                   /* UNIV_DEBUG */

/** Global Clone System */
Clone_Sys *clone_sys = nullptr;

/** Clone System state */
Clone_Sys_State Clone_Sys::s_clone_sys_state = {CLONE_SYS_INACTIVE};

/** Number of active abort requests */
uint Clone_Sys::s_clone_abort_count = 0;

/** Number of active wait requests */
uint Clone_Sys::s_clone_wait_count = 0;

Clone_Sys::Clone_Sys()
    : m_clone_arr(),
      m_num_clones(),
      m_num_apply_clones(),
      m_snapshot_arr(),
      m_num_snapshots(),
      m_num_apply_snapshots(),
      m_clone_id_generator() {
  mutex_create(LATCH_ID_CLONE_SYS, &m_clone_sys_mutex);
  m_space_initialized.store(false);
}

Clone_Sys::~Clone_Sys() {
  mutex_free(&m_clone_sys_mutex);

#ifdef UNIV_DEBUG
  /* Verify that no active clone is present */
  int idx;
  for (idx = 0; idx < CLONE_ARR_SIZE; idx++) {
    ut_ad(m_clone_arr[idx] == nullptr);
  }
  ut_ad(m_num_clones == 0);
  ut_ad(m_num_apply_clones == 0);

  for (idx = 0; idx < SNAPSHOT_ARR_SIZE; idx++) {
    ut_ad(m_snapshot_arr[idx] == nullptr);
  }
  ut_ad(m_num_snapshots == 0);
  ut_ad(m_num_apply_snapshots == 0);

#endif /* UNIV_DEBUG */
}

Clone_Handle *Clone_Sys::find_clone(const byte *ref_loc, uint loc_len,
                                    Clone_Handle_Type hdl_type) {
  int idx;
  bool match_found;

  Clone_Desc_Locator loc_desc;
  Clone_Desc_Locator ref_desc;
  Clone_Handle *clone_hdl;

  ut_ad(mutex_own(&m_clone_sys_mutex));

  if (ref_loc == nullptr) {
    return (nullptr);
  }

  ref_desc.deserialize(ref_loc, loc_len, nullptr);

  match_found = false;
  clone_hdl = nullptr;

  for (idx = 0; idx < CLONE_ARR_SIZE; idx++) {
    clone_hdl = m_clone_arr[idx];

    if (clone_hdl == nullptr || clone_hdl->is_init()) {
      continue;
    }

    if (clone_hdl->match_hdl_type(hdl_type)) {
      clone_hdl->build_descriptor(&loc_desc);

      if (loc_desc.match(&ref_desc)) {
        match_found = true;
        break;
      }
    }
  }

  if (match_found) {
    clone_hdl->attach();
    return (clone_hdl);
  }

  return (nullptr);
}

int Clone_Sys::find_free_index(Clone_Handle_Type hdl_type, uint &free_index) {
  free_index = CLONE_ARR_SIZE;

  uint target_index = CLONE_ARR_SIZE;
  Clone_Handle *target_clone = nullptr;

  for (uint idx = 0; idx < CLONE_ARR_SIZE; idx++) {
    auto clone_hdl = m_clone_arr[idx];

    if (clone_hdl == nullptr) {
      free_index = idx;
      break;
    }

    /* If existing clone has some error, it is on its way to exit. */
    auto err = clone_hdl->check_error(nullptr);
    if (hdl_type == CLONE_HDL_COPY && (clone_hdl->is_idle() || err != 0)) {
      target_clone = clone_hdl;
      target_index = idx;
    }
  }

  if (free_index == CLONE_ARR_SIZE ||
      (hdl_type == CLONE_HDL_COPY && m_num_clones == MAX_CLONES) ||
      (hdl_type == CLONE_HDL_APPLY && m_num_apply_clones == MAX_CLONES)) {
    if (target_clone == nullptr) {
      my_error(ER_CLONE_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_CLONES);
      return (ER_CLONE_TOO_MANY_CONCURRENT_CLONES);
    }
  } else {
    return (0);
  }

  /* We can abort idle clone and use the index. */
  ut_ad(target_clone != nullptr);
  ut_ad(mutex_own(&m_clone_sys_mutex));
  ut_ad(hdl_type == CLONE_HDL_COPY);

  target_clone->set_state(CLONE_STATE_ABORT);

  free_index = target_index;

  /* Sleep for 100 milliseconds. */
  Clone_Msec sleep_time(100);
  /* Generate alert message every second. */
  Clone_Sec alert_interval(1);
  /* Wait for 5 seconds for idle client to abort. */
  Clone_Sec time_out(5);

  bool is_timeout = false;
  auto err = Clone_Sys::wait(
      sleep_time, time_out, alert_interval,
      [&](bool alert, bool &result) {
        ut_ad(mutex_own(clone_sys->get_mutex()));
        auto current_clone = m_clone_arr[target_index];
        result = (current_clone != nullptr);

        if (thd_killed(nullptr)) {
          ib::info(ER_IB_CLONE_START_STOP)
              << "Clone Begin Master wait for abort interrupted";
          my_error(ER_QUERY_INTERRUPTED, MYF(0));
          return (ER_QUERY_INTERRUPTED);

        } else if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
          ib::info(ER_IB_CLONE_START_STOP)
              << "Clone Begin Master wait for abort interrupted by DDL";
          my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
          return (ER_CLONE_DDL_IN_PROGRESS);

        } else if (result) {
          if (!current_clone->is_abort()) {
            /* Another clone has taken over the free index. */
            ib::info(ER_IB_CLONE_START_STOP)
                << "Clone Begin Master wait for abort interrupted";
            my_error(ER_QUERY_INTERRUPTED, MYF(0));
            return ER_QUERY_INTERRUPTED;
          }
        }

        if (!result) {
          ib::info(ER_IB_CLONE_START_STOP) << "Clone Master aborted idle task";

        } else if (alert) {
          ib::info(ER_IB_CLONE_TIMEOUT)
              << "Clone Master waiting for idle task abort";
        }
        return (0);
      },
      clone_sys->get_mutex(), is_timeout);

  if (err == 0 && is_timeout) {
    ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Master wait for abort timed out";
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Innodb Clone Copy failed to abort idle clone [timeout]");
    err = ER_INTERNAL_ERROR;
  }
  return (err);
}

int Clone_Sys::add_clone(const byte *loc, Clone_Handle_Type hdl_type,
                         Clone_Handle *&clone_hdl) {
  ut_ad(mutex_own(&m_clone_sys_mutex));
  ut_ad(m_num_clones <= MAX_CLONES);
  ut_ad(m_num_apply_clones <= MAX_CLONES);

  auto version = choose_desc_version(loc);

  /* Find a free index to allocate new clone. */
  uint free_idx;
  auto err = find_free_index(hdl_type, free_idx);
  if (err != 0) {
    return (err);
  }

  /* Create a new clone. */
  clone_hdl = ut::new_withkey<Clone_Handle>(
      ut::make_psi_memory_key(mem_key_clone), hdl_type, version, free_idx);

  if (clone_hdl == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Clone_Handle));
    return (ER_OUTOFMEMORY);
  }

  m_clone_arr[free_idx] = clone_hdl;

  if (hdl_type == CLONE_HDL_COPY) {
    ++m_num_clones;
  } else {
    ut_ad(hdl_type == CLONE_HDL_APPLY);
    ++m_num_apply_clones;
  }

  clone_hdl->attach();

  return (0);
}

void Clone_Sys::drop_clone(Clone_Handle *clone_handle) {
  ut_ad(mutex_own(&m_clone_sys_mutex));

  if (clone_handle->detach() > 0) {
    return;
  }

  auto index = clone_handle->get_index();

  ut_ad(m_clone_arr[index] == clone_handle);

  m_clone_arr[index] = nullptr;

  if (clone_handle->is_copy_clone()) {
    ut_ad(m_num_clones > 0);
    --m_num_clones;

  } else {
    ut_ad(m_num_apply_clones > 0);
    --m_num_apply_clones;
  }

  ut::delete_(clone_handle);
}

Clone_Handle *Clone_Sys::get_clone_by_index(const byte *loc, uint loc_len) {
  Clone_Desc_Locator loc_desc;
  Clone_Handle *clone_hdl;

  loc_desc.deserialize(loc, loc_len, nullptr);

#ifdef UNIV_DEBUG
  Clone_Desc_Header *header = &loc_desc.m_header;
  ut_ad(header->m_type == CLONE_DESC_LOCATOR);
#endif
  clone_hdl = m_clone_arr[loc_desc.m_clone_index];

  ut_ad(clone_hdl != nullptr);

  return (clone_hdl);
}

int Clone_Sys::attach_snapshot(Clone_Handle_Type hdl_type,
                               Ha_clone_type clone_type, uint64_t snapshot_id,
                               bool is_pfs_monitor, Clone_Snapshot *&snapshot) {
  uint idx;
  uint free_idx = SNAPSHOT_ARR_SIZE;

  ut_ad(mutex_own(&m_clone_sys_mutex));

  /* Try to attach to an existing snapshot. */
  for (idx = 0; idx < SNAPSHOT_ARR_SIZE; idx++) {
    snapshot = m_snapshot_arr[idx];

    if (snapshot != nullptr) {
      if (snapshot->attach(hdl_type, is_pfs_monitor)) {
        return (0);
      }
    } else if (free_idx == SNAPSHOT_ARR_SIZE) {
      free_idx = idx;
    }
  }

  if (free_idx == SNAPSHOT_ARR_SIZE ||
      (hdl_type == CLONE_HDL_COPY && m_num_snapshots == MAX_SNAPSHOTS) ||
      (hdl_type == CLONE_HDL_APPLY && m_num_apply_snapshots == MAX_SNAPSHOTS)) {
    my_error(ER_CLONE_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_SNAPSHOTS);
    return (ER_CLONE_TOO_MANY_CONCURRENT_CLONES);
  }

  /* Create a new snapshot. */
  snapshot = ut::new_withkey<Clone_Snapshot>(
      ut::make_psi_memory_key(mem_key_clone), hdl_type, clone_type, free_idx,
      snapshot_id);

  if (snapshot == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Clone_Snapshot));
    return (ER_OUTOFMEMORY);
  }

  m_snapshot_arr[free_idx] = snapshot;

  if (hdl_type == CLONE_HDL_COPY) {
    ++m_num_snapshots;
  } else {
    ut_ad(hdl_type == CLONE_HDL_APPLY);
    ++m_num_apply_snapshots;
  }

  snapshot->attach(hdl_type, is_pfs_monitor);

  return (0);
}

void Clone_Sys::detach_snapshot(Clone_Snapshot *snapshot,
                                Clone_Handle_Type hdl_type) {
  ut_ad(mutex_own(&m_clone_sys_mutex));
  snapshot->detach();

  /* Drop the snapshot. */
  uint index;

  index = snapshot->get_index();
  ut_ad(m_snapshot_arr[index] == snapshot);

  ut::delete_(snapshot);

  m_snapshot_arr[index] = nullptr;

  if (hdl_type == CLONE_HDL_COPY) {
    ut_ad(m_num_snapshots > 0);
    --m_num_snapshots;

  } else {
    ut_ad(hdl_type == CLONE_HDL_APPLY);
    ut_ad(m_num_apply_snapshots > 0);
    --m_num_apply_snapshots;
  }
}

Clone_Sys::Acquire_clone::Acquire_clone() {
  std::tie(std::ignore, m_clone) = clone_sys->check_active_clone();

  if (m_clone != nullptr) {
    m_clone->attach();
  }
}

Clone_Sys::Acquire_clone::~Acquire_clone() {
  if (m_clone != nullptr) {
    clone_sys->drop_clone(m_clone);
  }
  m_clone = nullptr;
}

Clone_Snapshot *Clone_Sys::Acquire_clone::get_snapshot() {
  if (m_clone == nullptr) {
    return nullptr; /* purecov: inspected */
  }
  return m_clone->get_snapshot();
}

bool Clone_Sys::check_active_clone(bool print_alert) {
  bool active_clone = false;
  std::tie(active_clone, std::ignore) = check_active_clone();

  if (active_clone && print_alert) {
    /* purecov: begin inspected */
    ib::info(ER_IB_CLONE_TIMEOUT) << "DDL waiting for CLONE to abort";
    /* purecov: end */
  }
  return (active_clone);
}

std::tuple<bool, Clone_Handle *> Clone_Sys::check_active_clone() {
  ut_ad(mutex_own(&m_clone_sys_mutex));

  bool active_clone = false;
  Clone_Handle *active_handle = nullptr;

  /* Check for active clone operations. */
  for (int idx = 0; idx < CLONE_ARR_SIZE; idx++) {
    auto clone_hdl = m_clone_arr[idx];

    if (clone_hdl != nullptr && clone_hdl->is_copy_clone()) {
      active_clone = true;
      active_handle = clone_hdl;
      break;
    }
  }
  return std::make_tuple(active_clone, active_handle);
}

bool Clone_Sys::mark_abort(bool force) {
  ut_ad(mutex_own(&m_clone_sys_mutex));

  /* Check for active clone operations. Ignore clone, before initializing
  space. It is safe as clone would check for abort request afterwards. We
  require this check to prevent self deadlock when clone needs to create
  space objects while initializing.*/

  auto active_clone = is_space_initialized() && check_active_clone(false);

  /* If active clone is running and force is not set then
  return without setting abort state. */
  if (active_clone && !force) {
    return (false);
  }

  ++s_clone_abort_count;

  if (s_clone_sys_state != CLONE_SYS_ABORT) {
    ut_ad(s_clone_abort_count == 1);
    s_clone_sys_state = CLONE_SYS_ABORT;

    DEBUG_SYNC_C("clone_marked_abort");
  }

  if (active_clone) {
    ut_ad(force);

    /* Sleep for 1 second */
    Clone_Msec sleep_time(Clone_Sec(1));
    /* Generate alert message every minute. */
    Clone_Sec alert_time(Clone_Min(1));
    /* Timeout in 15 minutes - safeguard against hang, should not happen */
    Clone_Sec time_out(Clone_Min(15));

    bool is_timeout = false;

    wait(
        sleep_time, time_out, alert_time,
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_clone_sys_mutex));
          result = check_active_clone(alert);

          return (0);
        },
        &m_clone_sys_mutex, is_timeout);

    if (is_timeout) {
      ib::warn(ER_IB_CLONE_TIMEOUT) << "DDL wait for CLONE abort timed out"
                                       ", Continuing DDL.";
      ut_d(ut_error);
    }
  }
  return (true);
}

void Clone_Sys::mark_active() {
  ut_ad(mutex_own(&m_clone_sys_mutex));

  ut_ad(s_clone_abort_count > 0);
  --s_clone_abort_count;

  if (s_clone_abort_count == 0) {
    s_clone_sys_state = CLONE_SYS_ACTIVE;
  }
}

void Clone_Sys::mark_wait() {
  ut_ad(mutex_own(&m_clone_sys_mutex));
  /* Let any new clone operation wait till mark_free is called. */
  ++s_clone_wait_count;
}

void Clone_Sys::mark_free() {
  ut_ad(mutex_own(&m_clone_sys_mutex));
  ut_ad(s_clone_wait_count > 0);
  --s_clone_wait_count;
}

#ifdef UNIV_DEBUG
void Clone_Sys::debug_wait_clone_begin() {
  mutex_exit(&m_clone_sys_mutex);
  DEBUG_SYNC_C("clone_begin_wait_ddl");
  mutex_enter(&m_clone_sys_mutex);
}
#endif /* UNIV_DEBUG */

int Clone_Sys::wait_for_free(THD *thd) {
  ut_ad(mutex_own(&m_clone_sys_mutex));

  if (s_clone_wait_count == 0) {
    return (0);
  }

  auto wait_condition = [&](bool alert, bool &result) {
    ut_ad(mutex_own(&m_clone_sys_mutex));
    result = (s_clone_wait_count > 0);
    if (alert) {
      /* purecov: begin inspected */
      ib::info(ER_IB_CLONE_OPERATION)
          << "CLONE BEGIN waiting for DDL in critical section";
      /* purecov: end */
    }

    ut_d(debug_wait_clone_begin());

    if (thd_killed(thd)) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      return (ER_QUERY_INTERRUPTED);
    }

    if (s_clone_sys_state == CLONE_SYS_ABORT) {
      /* purecov: begin inspected */
      my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
      return ER_CLONE_DDL_IN_PROGRESS;
      /* purecov: end */
    }
    return (0);
  };

  /* Sleep for 100 milliseconds */
  Clone_Msec sleep_time(100);
  /* Generate alert message 5 second. */
  Clone_Sec alert_time(5);
  /* Timeout in 5 minutes - safeguard against hang, should not happen */
  Clone_Sec time_out(Clone_Min(5));

  bool is_timeout = false;
  auto err = wait(sleep_time, time_out, alert_time, wait_condition,
                  &m_clone_sys_mutex, is_timeout);

  if (err != 0) {
    return (err);
  }

  if (is_timeout) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Clone BEGIN timeout waiting for DDL in critical section");
    ut_d(ut_error);
    ut_o(return (ER_INTERNAL_ERROR));
  }

  return (0);
}

bool Clone_Sys::begin_ddl_state(Clone_notify::Type type, space_id_t space,
                                bool no_wait, bool check_intr,
                                uint32_t &blocked_state, int &error) {
  ut_ad(mutex_own(get_mutex()));
  Acquire_clone clone_handle;

  auto snapshot = clone_handle.get_snapshot();
  ut_ad(snapshot != nullptr);
  blocked_state = CLONE_SNAPSHOT_NONE;

  if (snapshot == nullptr) {
    return false; /* purecov: inspected */
  }

  DBUG_EXECUTE_IF("clone_ddl_error_abort", {
    error = ER_INTERNAL_ERROR;
    my_error(error, MYF(0), "Simulated Clone DDL error");
    return false;
  });

  /* Safe to release mutex after pinning the clone handle. */
  mutex_exit(get_mutex());
  bool blocked =
      snapshot->begin_ddl_state(type, space, no_wait, check_intr, error);
  mutex_enter(get_mutex());

  blocked_state = blocked ? snapshot->get_state() : CLONE_SNAPSHOT_NONE;

  return blocked;
}

void Clone_Sys::end_ddl_state(Clone_notify::Type type, space_id_t space,
                              uint32_t blocked_state) {
  ut_ad(mutex_own(get_mutex()));
  Acquire_clone clone_handle;

  auto snapshot = clone_handle.get_snapshot();

  /* Clone might have exited with error. */
  if (snapshot == nullptr) {
    return; /* purecov: inspected */
  }

  if (blocked_state != snapshot->get_state()) {
    /* purecov: begin deadcode */
    ib::error(ER_IB_CLONE_INTERNAL);
    ut_d(ut_error);
    /* purecov: end */
  }

  /* Safe to release mutex after pinning the clone handle. */
  mutex_exit(get_mutex());
  snapshot->end_ddl_state(type, space);
  mutex_enter(get_mutex());
}

uint64_t Clone_Sys::get_next_id() {
  ut_ad(mutex_own(&m_clone_sys_mutex));

  return (++m_clone_id_generator);
}

#ifdef UNIV_DEBUG
bool Clone_Task_Manager::debug_sync_check(uint32_t chunk_num,
                                          Clone_Task *task) {
  auto nchunks = m_clone_snapshot->get_num_chunks();

  /* Stop somewhere in the middle of current stage */
  if (!task->m_is_master || task->m_ignore_sync ||
      (chunk_num != 0 && chunk_num < (nchunks / 2 + 1))) {
    return false;
  }

  /* Ignore sync request for all future requests. */
  task->m_ignore_sync = true;
  return true;
}

void Clone_Task_Manager::debug_wait_ddl_meta() {
  auto state = m_clone_snapshot->get_state();

  /* We send DDL metadata of previous state. */
  if (state == CLONE_SNAPSHOT_PAGE_COPY) {
    DEBUG_SYNC_C("clone_before_file_ddl_meta");

  } else if (state == CLONE_SNAPSHOT_REDO_COPY) {
    DEBUG_SYNC_C("clone_before_page_ddl_meta");
  }
}

Clone_Task *Clone_Task_Manager::find_master_task() {
  Clone_Task *task = nullptr;

  for (uint32_t index = 0; index < m_num_tasks; ++index) {
    task = &m_clone_tasks[index];
    if (task->m_is_master) {
      break;
    }
  }
  return task;
}

void Clone_Handle::close_master_file() {
  auto task = m_clone_task_manager.find_master_task();
  close_and_unpin_file(task);
}

void Clone_Sys::close_donor_master_file() {
  IB_mutex_guard sys_mutex(get_mutex(), UT_LOCATION_HERE);

  Clone_Handle *clone_donor = nullptr;
  std::tie(std::ignore, clone_donor) = clone_sys->check_active_clone();

  clone_donor->close_master_file();
}

void Clone_Task_Manager::debug_wait(uint chunk_num, Clone_Task *task) {
  auto state = m_clone_snapshot->get_state();

  if (!debug_sync_check(chunk_num, task)) {
    return;
  }

  /* We are releasing the donor PIN early in debug mode to allow concurrent DDL
  after blocking here. The test need to ensure that it is local clone so that
  donor master task context can be found. This is in recipient path. */
  DBUG_EXECUTE_IF("local_release_clone_file_pin", {
    clone_sys->close_donor_master_file();
    ib::info(ER_IB_CLONE_OPERATION) << "Clone debug close donor master file";
  });

  if (state == CLONE_SNAPSHOT_FILE_COPY) {
    DBUG_SIGNAL_WAIT_FOR(current_thd, "gr_clone_wait", "gr_clone_paused",
                         "gr_clone_continue");

    DEBUG_SYNC_C("clone_file_copy");

  } else if (state == CLONE_SNAPSHOT_PAGE_COPY) {
    DEBUG_SYNC_C("clone_page_copy");

  } else if (state == CLONE_SNAPSHOT_REDO_COPY) {
    DEBUG_SYNC_C("clone_redo_copy");
  }
}

int Clone_Task_Manager::debug_restart(Clone_Task *task, int in_err,
                                      int restart_count) {
  auto err = in_err;

  if (err != 0 || restart_count < task->m_debug_counter || !task->m_is_master) {
    return (err);
  }

  /* Restart somewhere in the middle of all chunks */
  if (restart_count == 1) {
    auto nchunks = m_clone_snapshot->get_num_chunks();
    auto cur_chunk = task->m_task_meta.m_chunk_num;

    if (cur_chunk != 0 && cur_chunk < (nchunks / 2 + 1)) {
      return (err);
    }
  }

  DBUG_EXECUTE_IF("clone_restart_apply", err = ER_NET_READ_ERROR;);

  if (err != 0) {
    my_error(err, MYF(0));
  }

  /* Allow restart from next point */
  task->m_debug_counter = restart_count + 1;

  return (err);
}
#endif /* UNIV_DEBUG */

void Clone_Task_Manager::init(Clone_Snapshot *snapshot) {
  uint idx;

  m_clone_snapshot = snapshot;

  m_current_state = snapshot->get_state();

  /* ACK state is the previous state of current state */
  if (m_current_state == CLONE_SNAPSHOT_INIT) {
    m_ack_state = CLONE_SNAPSHOT_NONE;
  } else {
    /* If clone is attaching to active snapshot with
    other concurrent clone */
    ut_ad(m_current_state == CLONE_SNAPSHOT_FILE_COPY);
    m_ack_state = CLONE_SNAPSHOT_INIT;
  }

  m_chunk_info.m_total_chunks = 0;

  m_chunk_info.m_min_unres_chunk = 1;
  m_chunk_info.m_max_res_chunk = 0;

  /* Initialize all tasks in inactive state. */
  for (idx = 0; idx < CLONE_MAX_TASKS; idx++) {
    Clone_Task *task;

    task = m_clone_tasks + idx;
    task->m_task_state = CLONE_TASK_INACTIVE;

    task->m_serial_desc = nullptr;
    task->m_alloc_len = 0;

    task->m_current_file_des.m_file = OS_FILE_CLOSED;
    task->m_pinned_file = false;
    task->m_current_file_index = 0;
    task->m_file_cache = true;

    task->m_current_buffer = nullptr;
    task->m_buffer_alloc_len = 0;
    task->m_is_master = false;
    task->m_has_thd = false;
    task->m_data_size = 0;
    ut_d(task->m_ignore_sync = false);
    ut_d(task->m_debug_counter = 2);
  }

  m_num_tasks = 0;
  m_num_tasks_finished = 0;
  m_num_tasks_transit = 0;
  m_restart_count = 0;

  m_next_state = CLONE_SNAPSHOT_NONE;
  m_send_state_meta = false;
  m_transferred_file_meta = false;
  m_saved_error = 0;

  /* Initialize error file name */
  m_err_file_name.assign("Clone File");
}

void Clone_Task_Manager::reserve_task(THD *thd, uint &task_id) {
  ut_ad(mutex_own(&m_state_mutex));

  Clone_Task *task = nullptr;

  task_id = 0;

  /* Find inactive task in the array. */
  for (; task_id < CLONE_MAX_TASKS; task_id++) {
    task = m_clone_tasks + task_id;
    auto task_meta = &task->m_task_meta;

    if (task->m_task_state == CLONE_TASK_INACTIVE) {
      task->m_task_state = CLONE_TASK_ACTIVE;

      task_meta->m_task_index = task_id;
      task_meta->m_chunk_num = 0;
      task_meta->m_block_num = 0;

      /* Set first task as master task */
      if (task_id == 0) {
        ut_ad(thd != nullptr);
        task->m_is_master = true;
      }

      /* Whether the task has an associated user session */
      task->m_has_thd = (thd != nullptr);

      break;
    }

    task = nullptr;
  }

  ut_ad(task != nullptr);
}

int Clone_Task_Manager::alloc_buffer(Clone_Task *task) {
  if (task->m_alloc_len != 0) {
    /* Task buffers are already allocated in case
    clone operation is restarted. */

    ut_ad(task->m_buffer_alloc_len != 0);
    ut_ad(task->m_serial_desc != nullptr);
    ut_ad(task->m_current_buffer != nullptr);

    return (0);
  }

  /* Allocate task descriptor. */
  auto heap = m_clone_snapshot->lock_heap();

  /* Maximum variable length of descriptor. */
  auto alloc_len =
      static_cast<uint>(m_clone_snapshot->get_max_file_name_length());

  /* Check with maximum path name length. */
  if (alloc_len < FN_REFLEN_SE) {
    alloc_len = FN_REFLEN_SE;
  }

  /* Maximum fixed length of descriptor */
  alloc_len += CLONE_DESC_MAX_BASE_LEN;

  /* Add some buffer. */
  alloc_len += CLONE_DESC_MAX_BASE_LEN;

  ut_ad(task->m_alloc_len == 0);
  ut_ad(task->m_buffer_alloc_len == 0);

  task->m_alloc_len = alloc_len;
  task->m_buffer_alloc_len = m_clone_snapshot->get_dyn_buffer_length();

  alloc_len += task->m_buffer_alloc_len;

  alloc_len += CLONE_ALIGN_DIRECT_IO;

  ut_ad(task->m_serial_desc == nullptr);

  task->m_serial_desc = static_cast<byte *>(mem_heap_zalloc(heap, alloc_len));

  m_clone_snapshot->release_heap(heap);

  if (task->m_serial_desc == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(0), alloc_len);
    return (ER_OUTOFMEMORY);
  }

  if (task->m_buffer_alloc_len > 0) {
    task->m_current_buffer = static_cast<byte *>(ut_align(
        task->m_serial_desc + task->m_alloc_len, CLONE_ALIGN_DIRECT_IO));
  }

  return (0);
}

int Clone_Task_Manager::handle_error_other_task(bool set_error) {
  char errbuf[MYSYS_STRERROR_SIZE];

  if (set_error && m_saved_error != 0) {
    ib::info(ER_IB_CLONE_OPERATION)
        << "Clone error from other task code: " << m_saved_error;
  }

  if (!set_error) {
    return (m_saved_error);
  }

  /* Handle shutdown and KILL */
  if (thd_killed(nullptr)) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    return (ER_QUERY_INTERRUPTED);
  }

  /* Check if DDL has marked for abort. Ignore for client apply. */
  if ((m_clone_snapshot == nullptr || m_clone_snapshot->is_copy()) &&
      Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
    my_error(ER_CLONE_DDL_IN_PROGRESS, MYF(0));
    return (ER_CLONE_DDL_IN_PROGRESS);
  }

  switch (m_saved_error) {
    case ER_CLONE_DDL_IN_PROGRESS:
    case ER_QUERY_INTERRUPTED:
      my_error(m_saved_error, MYF(0));
      break;

    /* Network errors */
    case ER_NET_PACKET_TOO_LARGE:
    case ER_NET_PACKETS_OUT_OF_ORDER:
    case ER_NET_UNCOMPRESS_ERROR:
    case ER_NET_READ_ERROR:
    case ER_NET_READ_INTERRUPTED:
    case ER_NET_ERROR_ON_WRITE:
    case ER_NET_WRITE_INTERRUPTED:
    case ER_NET_WAIT_ERROR:
      my_error(m_saved_error, MYF(0));
      break;

    /* IO Errors */
    case ER_CANT_OPEN_FILE:
    case ER_CANT_CREATE_FILE:
    case ER_ERROR_ON_READ:
    case ER_ERROR_ON_WRITE:
      /* purecov: begin inspected */
      my_error(m_saved_error, MYF(0), m_err_file_name.c_str(), errno,
               my_strerror(errbuf, sizeof(errbuf), errno));
      break;
      /* purecov: end */

    case ER_FILE_EXISTS_ERROR:
      my_error(m_saved_error, MYF(0), m_err_file_name.c_str());
      break;

    case ER_WRONG_VALUE:
      my_error(m_saved_error, MYF(0), "file path", m_err_file_name.c_str());
      break;

    case ER_CLONE_DONOR:
      /* Will get the error message from remote */
      break;

    case 0:
      break;

    default:
      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Innodb Clone error in concurrent task");
  }

  return (m_saved_error);
}

bool Clone_Task_Manager::wait_before_add(const byte *ref_loc, uint loc_len) {
  ut_ad(mutex_own(&m_state_mutex));

  /* 1. Don't wait if master task. */
  if (m_num_tasks == 0) {
    return (false);
  }

  /* 2. Wait for state transition to get over */
  if (in_transit_state()) {
    return (true);
  }

  /* 3. For copy state(donor), wait for the state to reach file copy. */
  ut_ad(m_current_state != CLONE_SNAPSHOT_NONE);
  if (ref_loc == nullptr) {
    return (m_current_state == CLONE_SNAPSHOT_INIT);
  }

  Clone_Desc_Locator ref_desc;
  ref_desc.deserialize(ref_loc, loc_len, nullptr);

  ut_ad(m_current_state <= ref_desc.m_state);

  /* 4. For apply state (recipient), wait for apply state to reach
  the copy state in reference locator. */
  if (m_current_state != ref_desc.m_state) {
    return (true);
  }

  /* 4A. For file copy state, wait for all metadata to be transferred. */
  if (m_current_state == CLONE_SNAPSHOT_FILE_COPY &&
      !is_file_metadata_transferred()) {
    return (true);
  }
  return (false);
}

int Clone_Task_Manager::add_task(THD *thd, const byte *ref_loc, uint loc_len,
                                 uint &task_id) {
  mutex_enter(&m_state_mutex);

  /* Check for error from other tasks */
  bool raise_error = (thd != nullptr);

  auto err = handle_error_other_task(raise_error);

  if (err != 0) {
    mutex_exit(&m_state_mutex);
    return (err);
  }

  if (wait_before_add(ref_loc, loc_len)) {
    bool is_timeout = false;
    int alert_count = 0;
    err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_state_mutex));
          result = wait_before_add(ref_loc, loc_len);

          /* Check for error from other tasks */
          err = handle_error_other_task(raise_error);

          if (err == 0 && result && alert) {
            /* Print messages every 1 minute - default is 5 seconds. */
            if (++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Add task waiting "
                                               "for state change";
            }
          }
          return (err);
        },
        &m_state_mutex, is_timeout);

    if (err != 0) {
      mutex_exit(&m_state_mutex);
      return (err);

    } else if (is_timeout) {
      ut_d(ut_error);
#ifndef UNIV_DEBUG
      mutex_exit(&m_state_mutex);

      ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Add task timed out";

      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Clone Add task failed: "
               "Wait too long for state transition");
      return (ER_INTERNAL_ERROR);
#endif
    }
  }

  /* We wait for state transition before adding new task. */
  ut_ad(!in_transit_state());

  if (m_num_tasks == CLONE_MAX_TASKS) {
    err = ER_CLONE_TOO_MANY_CONCURRENT_CLONES;
    my_error(err, MYF(0), CLONE_MAX_TASKS);

    mutex_exit(&m_state_mutex);
    return (err);
  }

  reserve_task(thd, task_id);
  ut_ad(task_id <= m_num_tasks);

  ++m_num_tasks;

  mutex_exit(&m_state_mutex);
  return (0);
}

bool Clone_Task_Manager::drop_task(THD *thd, uint task_id, bool &is_master) {
  mutex_enter(&m_state_mutex);

  if (in_transit_state()) {
    ut_ad(m_num_tasks_transit > 0);
    --m_num_tasks_transit;
  }

  ut_ad(m_num_tasks > 0);
  --m_num_tasks;

  auto task = get_task_by_index(task_id);

  add_incomplete_chunk(task);

  reset_chunk(task);

  ut_ad(task->m_task_state == CLONE_TASK_ACTIVE);
  task->m_task_state = CLONE_TASK_INACTIVE;

  is_master = task->m_is_master;

  if (!is_master) {
    mutex_exit(&m_state_mutex);
    return (false);
  }

  /* Master needs to wait for other tasks to get dropped */
  if (m_num_tasks > 0) {
    bool is_timeout = false;
    int alert_count = 0;
    auto err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_state_mutex));
          result = (m_num_tasks > 0);

          if (thd_killed(thd)) {
            return (ER_QUERY_INTERRUPTED);

          } else if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {
            return (ER_CLONE_DDL_IN_PROGRESS);
          }
          if (alert && result) {
            /* Print messages every 1 minute - default is 5 seconds. */
            if (++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Master drop task waiting "
                                               "for other tasks";
            }
          }
          return (0);
        },
        &m_state_mutex, is_timeout);

    if (err != 0) {
      mutex_exit(&m_state_mutex);
      return (false);

    } else if (is_timeout) {
      ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Master drop task timed out";

      mutex_exit(&m_state_mutex);
      ut_d(ut_error);
      ut_o(return (false));
    }
  }

  mutex_exit(&m_state_mutex);

  /* Restart after network error */
  auto current_err = handle_error_other_task(false);
  if (is_network_error(current_err)) {
    return (true);
  }
  return (false);
}

uint32_t Clone_Task_Manager::get_next_chunk() {
  auto &max_chunk = m_chunk_info.m_max_res_chunk;
  auto &min_chunk = m_chunk_info.m_min_unres_chunk;

  ut_ad(max_chunk <= m_chunk_info.m_total_chunks);

  if (min_chunk > m_chunk_info.m_total_chunks) {
    /* No more chunks left for current state. */
    return (0);
  }

  /* Return the minimum unreserved chunk */
  auto ret_chunk = min_chunk;

  /* Mark the chunk reserved. The chunk must be unreserved. */
  ut_ad(!m_chunk_info.m_reserved_chunks[min_chunk]);
  m_chunk_info.m_reserved_chunks[min_chunk] = true;

  /* Increase max reserved chunk if needed */
  if (max_chunk < min_chunk) {
    max_chunk = min_chunk;
  }

  ut_ad(max_chunk == m_chunk_info.m_reserved_chunks.get_max_set_bit());

  /* Set the next unreserved chunk */
  while (m_chunk_info.m_reserved_chunks[min_chunk]) {
    ++min_chunk;

    /* Exit if all chunks are over */
    if (min_chunk > max_chunk || min_chunk > m_chunk_info.m_total_chunks) {
      ut_ad(min_chunk > m_chunk_info.m_total_chunks ||
            !m_chunk_info.m_reserved_chunks[min_chunk]);

      break;
    }
  }

  return (ret_chunk);
}

uint32_t Clone_Task_Manager::get_next_incomplete_chunk(uint32_t &block_num) {
  block_num = 0;

  auto &chunks = m_chunk_info.m_incomplete_chunks;

  if (chunks.empty()) {
    return (0);
  }

  auto it = chunks.begin();

  auto chunk_num = it->first;

  block_num = it->second;

  chunks.erase(it);

  return (chunk_num);
}

int Clone_Task_Manager::reserve_next_chunk(Clone_Task *task,
                                           uint32_t &ret_chunk,
                                           uint32_t &ret_block) {
  mutex_enter(&m_state_mutex);
  ret_chunk = 0;

  /* Check for error from other tasks */
  auto err = handle_error_other_task(task->m_has_thd);
  if (err != 0) {
    mutex_exit(&m_state_mutex);
    return (err);
  }

  if (process_inclomplete_chunk()) {
    /* Get next incomplete chunk. */
    ret_chunk = get_next_incomplete_chunk(ret_block);
    ut_ad(ret_chunk != 0);

  } else {
    /* Get next unreserved chunk. */
    ret_block = 0;
    ret_chunk = get_next_chunk();
  }

  reset_chunk(task);
  mutex_exit(&m_state_mutex);
  return (0);
}

int Clone_Task_Manager::set_chunk(Clone_Task *task, Clone_Task_Meta *new_meta) {
  auto cur_meta = &task->m_task_meta;
  int err = 0;

  ut_ad(cur_meta->m_task_index == new_meta->m_task_index);
  cur_meta->m_task_index = new_meta->m_task_index;

  /* Check if this is a new chunk */
  if (cur_meta->m_chunk_num != new_meta->m_chunk_num) {
    mutex_enter(&m_state_mutex);

    /* Mark the current chunk reserved */
    m_chunk_info.m_reserved_chunks[new_meta->m_chunk_num] = true;

    /* Check and remove the chunk from incomplete chunk list. */
    auto &chunks = m_chunk_info.m_incomplete_chunks;

    auto key_value = chunks.find(new_meta->m_chunk_num);

    if (key_value != chunks.end()) {
      ut_ad(key_value->second < new_meta->m_block_num);
      chunks.erase(key_value);
    }

    reset_chunk(task);

    /* Check for error from other tasks */
    err = handle_error_other_task(task->m_has_thd);

    mutex_exit(&m_state_mutex);

    cur_meta->m_chunk_num = new_meta->m_chunk_num;

#ifdef UNIV_DEBUG
    /* Network failure in the middle of a state */
    err = debug_restart(task, err, 1);

    /* Wait in the middle of state */
    debug_wait(cur_meta->m_chunk_num, task);
#endif /* UNIV_DEBUG */
  }

  cur_meta->m_block_num = new_meta->m_block_num;

  return (err);
}

void Clone_Task_Manager::add_incomplete_chunk(Clone_Task *task) {
  /* Track incomplete chunks during apply */
  if (m_clone_snapshot->is_copy()) {
    return;
  }

  auto &task_meta = task->m_task_meta;

  /* The task doesn't have any incomplete chunks */
  if (task_meta.m_chunk_num == 0) {
    return;
  }

  auto &chunks = m_chunk_info.m_incomplete_chunks;

  chunks[task_meta.m_chunk_num] = task_meta.m_block_num;

  ib::info(ER_IB_CLONE_RESTART)
      << "Clone Apply add incomplete Chunk = " << task_meta.m_chunk_num
      << " Block = " << task_meta.m_block_num
      << " Task = " << task_meta.m_task_index;
}

/** Print completed chunk information
@param[in]      chunk_info      chunk information */
static void print_chunk_info(Chunk_Info *chunk_info) {
  for (auto &chunk : chunk_info->m_incomplete_chunks) {
    ib::info(ER_IB_CLONE_RESTART)
        << "Incomplete: Chunk = " << chunk.first << " Block = " << chunk.second;
  }

  auto min = chunk_info->m_reserved_chunks.get_min_unset_bit();
  auto max = chunk_info->m_reserved_chunks.get_max_set_bit();

  auto size = chunk_info->m_reserved_chunks.size_bits();

  ib::info(ER_IB_CLONE_RESTART)
      << "Number of Chunks: " << size << " Min = " << min << " Max = " << max;

  ut_ad(min != max);

  if (max > min) {
    ib::info(ER_IB_CLONE_RESTART)
        << "Reserved Chunk Information : " << min << " - " << max
        << " Chunks: " << max - min + 1;

    for (uint32_t index = min; index <= max;) {
      uint32_t ind = 0;

      const int STR_SIZE = 64;
      char str[STR_SIZE + 1];

      while (index <= max && ind < STR_SIZE) {
        str[ind] = chunk_info->m_reserved_chunks[index] ? '1' : '0';
        ++index;
        ++ind;
      }

      ut_ad(ind <= STR_SIZE);
      str[ind] = '\0';

      ib::info(ER_IB_CLONE_RESTART) << str;
    }
  }
}

void Clone_Task_Manager::reinit_apply_state(const byte *ref_loc, uint ref_len,
                                            byte *&new_loc, uint &new_len,
                                            uint &alloc_len) {
  ut_ad(m_current_state != CLONE_SNAPSHOT_NONE);
  ut_ad(!m_clone_snapshot->is_copy());

  /* Only master task should be present */
  ut_ad(m_num_tasks == 1);

  /* Reset State transition information */
  reset_transition();

  /* Reset Error information */
  reset_error();

  /* Check if current state is finished and acknowledged */
  ut_ad(m_ack_state <= m_current_state);

  if (m_ack_state == m_current_state) {
    ++m_num_tasks_finished;
  }

  ++m_restart_count;

  switch (m_current_state) {
    case CLONE_SNAPSHOT_INIT:
      ib::info(ER_IB_CLONE_RESTART) << "Clone Apply Restarting State: INIT";
      break;

    case CLONE_SNAPSHOT_FILE_COPY:
      ib::info(ER_IB_CLONE_OPERATION)
          << "Clone Apply Restarting State: FILE COPY";
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      ib::info(ER_IB_CLONE_OPERATION)
          << "Clone Apply Restarting State: PAGE COPY";
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      ib::info(ER_IB_CLONE_OPERATION)
          << "Clone Apply Restarting State: REDO COPY";
      break;

    case CLONE_SNAPSHOT_DONE:
      ib::info(ER_IB_CLONE_OPERATION) << "Clone Apply Restarting State: DONE";
      break;

    case CLONE_SNAPSHOT_NONE:
    default:
      ut_d(ut_error);
  }

  if (m_current_state == CLONE_SNAPSHOT_INIT ||
      m_current_state == CLONE_SNAPSHOT_DONE ||
      m_current_state == CLONE_SNAPSHOT_NONE) {
    new_loc = nullptr;
    new_len = 0;
    return;
  }

  /* Add incomplete chunks from master task */
  auto task = get_task_by_index(0);

  add_incomplete_chunk(task);

  /* Reset task information */
  mutex_enter(&m_state_mutex);
  reset_chunk(task);
  mutex_exit(&m_state_mutex);

  /* Allocate for locator if required */
  Clone_Desc_Locator temp_locator;

  temp_locator.deserialize(ref_loc, ref_len, nullptr);

  /* Update current state information */
  temp_locator.m_state = m_current_state;

  /* Update sub-state information */
  temp_locator.m_metadata_transferred = m_transferred_file_meta;

  auto len = temp_locator.m_header.m_length;
  len += static_cast<uint>(m_chunk_info.get_serialized_length(0));

  if (len > alloc_len) {
    /* Allocate for more for possible reuse */
    len = CLONE_DESC_MAX_BASE_LEN;
    ut_ad(len >= temp_locator.m_header.m_length);

    len += static_cast<uint>(m_chunk_info.get_serialized_length(
        static_cast<uint32_t>(CLONE_MAX_TASKS)));

    auto heap = m_clone_snapshot->lock_heap();

    new_loc = static_cast<byte *>(mem_heap_zalloc(heap, len));
    alloc_len = len;

    m_clone_snapshot->release_heap(heap);
  }

  new_len = alloc_len;

  temp_locator.serialize(new_loc, new_len, &m_chunk_info, nullptr);

  print_chunk_info(&m_chunk_info);
}

void Clone_Task_Manager::reinit_copy_state(const byte *loc, uint loc_len) {
  ut_ad(m_clone_snapshot->is_copy());
  ut_ad(m_num_tasks == 0);

  mutex_enter(&m_state_mutex);

  /* Reset State transition information */
  reset_transition();

  /* Reset Error information */
  reset_error();

  ++m_restart_count;

  switch (m_current_state) {
    case CLONE_SNAPSHOT_INIT:
      ib::info(ER_IB_CLONE_RESTART) << "Clone Restarting State: INIT";
      break;

    case CLONE_SNAPSHOT_FILE_COPY:
      ib::info(ER_IB_CLONE_RESTART) << "Clone Restarting State: FILE COPY";
      break;

    case CLONE_SNAPSHOT_PAGE_COPY:
      ib::info(ER_IB_CLONE_RESTART) << "Clone Restarting State: PAGE COPY";
      break;

    case CLONE_SNAPSHOT_REDO_COPY:
      ib::info(ER_IB_CLONE_RESTART) << "Clone Restarting State: REDO COPY";
      break;

    case CLONE_SNAPSHOT_DONE:
      ib::info(ER_IB_CLONE_RESTART) << "Clone Restarting State: DONE";
      break;

    case CLONE_SNAPSHOT_NONE:
    default:
      ut_d(ut_error);
  }

  if (m_current_state == CLONE_SNAPSHOT_NONE) {
    mutex_exit(&m_state_mutex);
    ut_d(ut_error);
    ut_o(return );
  }

  /* Reset to beginning of current state */
  init_state();

  /* Compare local and remote state */
  Clone_Desc_Locator temp_locator;

  temp_locator.deserialize(loc, loc_len, nullptr);

  /* If Local state is ahead, we must have finished the
  previous state confirmed by ACK. It is enough to
  start from current state. */
  if (temp_locator.m_state != m_current_state) {
#ifdef UNIV_DEBUG
    /* Current state could be just one state ahead */
    if (temp_locator.m_state == CLONE_SNAPSHOT_INIT) {
      ut_ad(m_current_state == CLONE_SNAPSHOT_FILE_COPY);

    } else if (temp_locator.m_state == CLONE_SNAPSHOT_FILE_COPY) {
      ut_ad(m_current_state == CLONE_SNAPSHOT_PAGE_COPY);

    } else if (temp_locator.m_state == CLONE_SNAPSHOT_PAGE_COPY) {
      ut_ad(m_current_state == CLONE_SNAPSHOT_REDO_COPY);

    } else if (temp_locator.m_state == CLONE_SNAPSHOT_REDO_COPY) {
      ut_ad(m_current_state == CLONE_SNAPSHOT_DONE);

    } else {
      ut_d(ut_error);
    }
#endif /* UNIV_DEBUG */

    /* Apply state is behind. Need to send state metadata */
    m_send_state_meta = true;

    mutex_exit(&m_state_mutex);
    return;
  }

  m_send_state_meta = false;
  m_transferred_file_meta = temp_locator.m_metadata_transferred;

  /* Set progress information for current state */
  temp_locator.deserialize(loc, loc_len, &m_chunk_info);

  m_chunk_info.init_chunk_nums();

  mutex_exit(&m_state_mutex);

  print_chunk_info(&m_chunk_info);
}

void Clone_Task_Manager::init_state() {
  ut_ad(mutex_own(&m_state_mutex));

  auto num_chunks = m_clone_snapshot->get_num_chunks();

  auto heap = m_clone_snapshot->lock_heap();

  m_chunk_info.m_reserved_chunks.reset(num_chunks, heap);

  m_clone_snapshot->release_heap(heap);

  m_chunk_info.m_incomplete_chunks.clear();

  m_chunk_info.m_min_unres_chunk = 1;
  ut_ad(m_chunk_info.m_reserved_chunks.get_min_unset_bit() == 1);

  m_chunk_info.m_max_res_chunk = 0;
  ut_ad(m_chunk_info.m_reserved_chunks.get_max_set_bit() == 0);

  m_chunk_info.m_total_chunks = num_chunks;
}

void Clone_Task_Manager::ack_state(const Clone_Desc_State *state_desc) {
  mutex_enter(&m_state_mutex);

  m_ack_state = state_desc->m_state;
  ut_ad(m_current_state == m_ack_state);
  ib::info(ER_IB_CLONE_OPERATION)
      << "Clone set state change ACK: " << m_ack_state;

  mutex_exit(&m_state_mutex);
}

int Clone_Task_Manager::wait_ack(Clone_Handle *clone, Clone_Task *task,
                                 Ha_clone_cbk *callback) {
  mutex_enter(&m_state_mutex);

  ++m_num_tasks_finished;

  /* All chunks are finished */
  reset_chunk(task);

  if (!task->m_is_master) {
    mutex_exit(&m_state_mutex);
    return (0);
  }

  int err = 0;

  if (m_current_state != m_ack_state) {
    bool is_timeout = false;
    int alert_count = 0;
    err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_state_mutex));
          result = (m_current_state != m_ack_state);

          /* Check for error from other tasks */
          err = handle_error_other_task(task->m_has_thd);

          if (err == 0 && result && alert) {
            /* Print messages every 1 minute - default is 5 seconds. */
            if (++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Master waiting "
                                               "for state change ACK ";
            }
            err = clone->send_keep_alive(task, callback);
          }
          return (err);
        },
        &m_state_mutex, is_timeout);

    /* Wait too long */
    if (err == 0 && is_timeout) {
      ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Master wait for state change ACK"
                                       " timed out";

      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Innodb clone state ack wait too long");

      err = ER_INTERNAL_ERROR;
      ut_d(ut_error);
    }
  }
  mutex_exit(&m_state_mutex);

  if (err == 0) {
    ib::info(ER_IB_CLONE_OPERATION) << "Clone Master received state change ACK";
  }

  return (err);
}

int Clone_Task_Manager::finish_state(Clone_Task *task) {
  mutex_enter(&m_state_mutex);

  if (task->m_is_master) {
    /* Check if ACK was sent before restart */
    if (m_ack_state != m_current_state) {
      ut_ad(m_ack_state < m_current_state);
      ++m_num_tasks_finished;
    } else {
      ut_ad(m_restart_count > 0);
    }
    m_ack_state = m_current_state;

  } else {
    ++m_num_tasks_finished;
  }

  /* All chunks are finished */
  reset_chunk(task);

  /* Check for error from other tasks */
  auto err = handle_error_other_task(task->m_has_thd);

  if (!task->m_is_master || err != 0) {
    mutex_exit(&m_state_mutex);
    return (err);
  }

  ut_ad(task->m_is_master);

#ifdef UNIV_DEBUG
  /* Wait before ending state, if needed */
  if (!task->m_ignore_sync) {
    mutex_exit(&m_state_mutex);
    debug_wait(0, task);
    mutex_enter(&m_state_mutex);
  }
#endif /* UNIV_DEBUG */

  if (m_num_tasks_finished < m_num_tasks) {
    bool is_timeout = false;
    int alert_count = 0;
    err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          ut_ad(mutex_own(&m_state_mutex));
          result = (m_num_tasks_finished < m_num_tasks);

          /* Check for error from other tasks */
          err = handle_error_other_task(task->m_has_thd);

          if (err == 0 && result && alert) {
            /* Print messages every 1 minute - default is 5 seconds. */
            if (++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_CLONE_TIMEOUT)
                  << "Clone Apply Master waiting for "
                     "workers before sending ACK."
                  << " Total = " << m_num_tasks
                  << " Finished = " << m_num_tasks_finished;
            }
          }
          return (err);
        },
        &m_state_mutex, is_timeout);

    if (err == 0 && is_timeout) {
      ib::info(ER_IB_CLONE_TIMEOUT) << "Clone Apply Master wait timed out";

      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Clone Apply Master wait timed out before sending ACK");

      err = ER_INTERNAL_ERROR;
      ut_d(ut_error);
    }
  }

  mutex_exit(&m_state_mutex);
  return (err);
}

int Clone_Task_Manager::change_state(Clone_Task *task,
                                     Clone_Desc_State *state_desc,
                                     Snapshot_State new_state,
                                     Clone_Alert_Func cbk, uint &num_wait) {
  mutex_enter(&m_state_mutex);

  num_wait = 0;

  /* Check for error from other tasks */
  auto err = handle_error_other_task(task->m_has_thd);

  if (err != 0) {
    mutex_exit(&m_state_mutex);
    return (err);
  }

  /* First requesting task needs to initiate the state transition. */
  if (!in_transit_state()) {
    m_num_tasks_transit = m_num_tasks;
    m_next_state = new_state;
  }

  /* Master needs to wait for all other tasks. */
  if (task->m_is_master && m_num_tasks_transit > 1) {
    num_wait = m_num_tasks_transit;

    mutex_exit(&m_state_mutex);
    return (0);
  }

  /* Need to wait for transition to next state */
  if (!task->m_is_master) {
    /* Move the current task over to the next state */
    ut_ad(m_num_tasks_transit > 0);
    --m_num_tasks_transit;

    num_wait = m_num_tasks_transit;
    ut_ad(num_wait > 0);

    mutex_exit(&m_state_mutex);
    return (0);
  }

  /* Last task requesting the state change. All other tasks have
  already moved over to next state and waiting for the transition
  to complete. Now it is safe to do the snapshot state transition. */

  ut_ad(task->m_is_master);
  mutex_exit(&m_state_mutex);

  if (m_clone_snapshot->is_copy()) {
    ib::info(ER_IB_CLONE_OPERATION)
        << "Clone State Change : Number of tasks = " << m_num_tasks;
  } else {
    ib::info(ER_IB_CLONE_OPERATION)
        << "Clone Apply State Change : Number of tasks = " << m_num_tasks;
  }

  err = m_clone_snapshot->change_state(state_desc, m_next_state,
                                       task->m_current_buffer,
                                       task->m_buffer_alloc_len, cbk);

  if (err != 0) {
    return (err);
  }

  mutex_enter(&m_state_mutex);

  /* Check for error from other tasks. Must finish the state transition
  even in case of an error. */
  err = handle_error_other_task(task->m_has_thd);

  m_current_state = m_next_state;
  m_next_state = CLONE_SNAPSHOT_NONE;

  --m_num_tasks_transit;
  /* In case of error, the other tasks might have exited. */
  ut_ad(m_num_tasks_transit == 0 || err != 0);
  m_num_tasks_transit = 0;

  /* For restart, m_num_tasks_finished may not be up to date */
  ut_ad(m_num_tasks_finished == m_num_tasks || err != 0);
  m_num_tasks_finished = 0;

  ut_d(task->m_ignore_sync = false);
  ut_d(task->m_debug_counter = 0);

  /* Initialize next state after transition. */
  init_state();

  mutex_exit(&m_state_mutex);

  return (err);
}

int Clone_Task_Manager::check_state(Clone_Task *task, Snapshot_State new_state,
                                    bool exit_on_wait, int in_err,
                                    uint32_t &num_wait) {
  mutex_enter(&m_state_mutex);

  num_wait = 0;

  if (in_err != 0) {
    /* Save error for other tasks */
    if (m_saved_error == 0) {
      m_saved_error = in_err;
    }
    /* Mark transit incomplete */
    if (in_transit_state()) {
      ++m_num_tasks_transit;
    }
    mutex_exit(&m_state_mutex);
    return (in_err);
  }

  /* Check for error from other tasks */
  auto err = handle_error_other_task(task->m_has_thd);

  if (err != 0) {
    mutex_exit(&m_state_mutex);
    return (err);
  }

  /* Check if current transition is still in progress. */
  if (in_transit_state() && new_state == m_next_state) {
    num_wait = m_num_tasks_transit;

    ut_ad(num_wait > 0);

    if (exit_on_wait) {
      /* Mark error for other tasks */
      m_saved_error = ER_INTERNAL_ERROR;
      /* Mark transit incomplete */
      ++m_num_tasks_transit;
    }
  }

  mutex_exit(&m_state_mutex);

  return (0);
}

Clone_Handle::Clone_Handle(Clone_Handle_Type handle_type, uint clone_version,
                           uint clone_index)
    : m_clone_handle_type(handle_type),
      m_clone_handle_state(CLONE_STATE_INIT),
      m_clone_locator(),
      m_locator_length(),
      m_restart_loc(),
      m_restart_loc_len(),
      m_clone_desc_version(clone_version),
      m_clone_arr_index(clone_index),
      m_clone_id(),
      m_ref_count(),
      m_allow_restart(false),
      m_abort_ddl(false),
      m_clone_dir(),
      m_clone_task_manager() {
  mutex_create(LATCH_ID_CLONE_TASK, m_clone_task_manager.get_mutex());

  Clone_Desc_Locator loc_desc;
  loc_desc.init(0, 0, CLONE_SNAPSHOT_NONE, clone_version, clone_index);

  auto loc = &m_version_locator[0];
  uint len = CLONE_DESC_MAX_BASE_LEN;

  memset(loc, 0, CLONE_DESC_MAX_BASE_LEN);

  loc_desc.serialize(loc, len, nullptr, nullptr);

  ut_ad(len <= CLONE_DESC_MAX_BASE_LEN);
}

Clone_Handle::~Clone_Handle() {
  mutex_free(m_clone_task_manager.get_mutex());

  if (!is_init()) {
    clone_sys->detach_snapshot(m_clone_task_manager.get_snapshot(),
                               m_clone_handle_type);
  }
  ut_ad(m_ref_count == 0);
}

int Clone_Handle::create_clone_directory() {
  ut_ad(!is_copy_clone());
  dberr_t db_err = DB_SUCCESS;
  std::string file_name;

  if (!replace_datadir()) {
    /* Create data directory, if we not replacing the current one. */
    db_err = os_file_create_subdirs_if_needed(m_clone_dir);
    if (db_err == DB_SUCCESS) {
      auto status = os_file_create_directory(m_clone_dir, false);
      /* Create mysql schema directory. */
      file_name.assign(m_clone_dir);
      file_name.append(OS_PATH_SEPARATOR_STR);
      if (status) {
        file_name.append("mysql");
        status = os_file_create_directory(file_name.c_str(), true);
      }
      if (!status) {
        db_err = DB_ERROR;
      }
    }
    file_name.assign(m_clone_dir);
    file_name.append(OS_PATH_SEPARATOR_STR);
  }

  /* Create clone status directory. */
  if (db_err == DB_SUCCESS) {
    file_name.append(CLONE_FILES_DIR);
    auto status = os_file_create_directory(file_name.c_str(), false);
    if (!status) {
      db_err = DB_ERROR;
    }
  }
  /* Check and report error. */
  if (db_err != DB_SUCCESS) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(ER_CANT_CREATE_DB, MYF(0), m_clone_dir, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (ER_CANT_CREATE_DB);
  }
  return (0);
}

int Clone_Handle::init(const byte *ref_loc, uint ref_len, Ha_clone_type type,
                       const char *data_dir) {
  uint64_t snapshot_id;
  Clone_Snapshot *snapshot;

  m_clone_dir = data_dir;

  bool enable_monitor = true;

  /* Generate unique clone identifiers for copy clone handle. */
  if (is_copy_clone()) {
    m_clone_id = clone_sys->get_next_id();
    snapshot_id = clone_sys->get_next_id();

    /* For local clone, monitor while applying data. */
    if (ref_loc == nullptr) {
      enable_monitor = false;
    }

  } else {
    /* We don't provision instance on which active clone is running. */
    if (replace_datadir() && clone_sys->check_active_clone(false)) {
      my_error(ER_CLONE_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_CLONES);
      return (ER_CLONE_TOO_MANY_CONCURRENT_CLONES);
    }
    /* Return keeping the clone in INIT state. The locator
    would only have the version information. */
    if (ref_loc == nullptr) {
      return (0);
    }

    auto err = create_clone_directory();
    if (err != 0) {
      return (err);
    }

    /* Set clone identifiers from reference locator for apply clone
    handle. The reference locator is from copy clone handle. */
    Clone_Desc_Locator loc_desc;

    loc_desc.deserialize(ref_loc, ref_len, nullptr);

    m_clone_id = loc_desc.m_clone_id;
    snapshot_id = loc_desc.m_snapshot_id;

    ut_ad(m_clone_id != CLONE_LOC_INVALID_ID);
    ut_ad(snapshot_id != CLONE_LOC_INVALID_ID);
  }

  /* Create and attach to snapshot. */
  auto err = clone_sys->attach_snapshot(m_clone_handle_type, type, snapshot_id,
                                        enable_monitor, snapshot);

  if (err != 0) {
    return (err);
  }

  /* Initialize clone task manager. */
  m_clone_task_manager.init(snapshot);

  m_clone_handle_state = CLONE_STATE_ACTIVE;

  return (0);
}

byte *Clone_Handle::get_locator(uint &loc_len) {
  Clone_Desc_Locator loc_desc;

  /* Return version locator during initialization. */
  if (is_init()) {
    loc_len = CLONE_DESC_MAX_BASE_LEN;
    return (&m_version_locator[0]);
  }

  auto snapshot = m_clone_task_manager.get_snapshot();

  auto heap = snapshot->lock_heap();

  build_descriptor(&loc_desc);

  loc_desc.serialize(m_clone_locator, m_locator_length, nullptr, heap);

  loc_len = m_locator_length;

  snapshot->release_heap(heap);

  return (m_clone_locator);
}

void Clone_Handle::build_descriptor(Clone_Desc_Locator *loc_desc) {
  Clone_Snapshot *snapshot;
  uint64_t snapshot_id = CLONE_LOC_INVALID_ID;
  Snapshot_State state = CLONE_SNAPSHOT_NONE;

  snapshot = m_clone_task_manager.get_snapshot();

  if (snapshot) {
    state = snapshot->get_state();
    snapshot_id = snapshot->get_id();
  }

  loc_desc->init(m_clone_id, snapshot_id, state, m_clone_desc_version,
                 m_clone_arr_index);
}

bool Clone_Handle::drop_task(THD *thd, uint task_id, bool &is_master) {
  /* No task is added in INIT state. The drop task is still called and
  should be ignored. */
  if (is_init()) {
    /* Only relevant for apply clone master */
    ut_ad(!is_copy_clone());
    ut_ad(task_id == 0);
    is_master = true;
    return (false);
  }
  /* Cannot be in IDLE state as master waits for tasks to drop before idling */
  ut_ad(!is_idle());

  /* Close and reset file related information */
  auto task = m_clone_task_manager.get_task_by_index(task_id);

  close_file(task);

  ut_ad(mutex_own(clone_sys->get_mutex()));
  mutex_exit(clone_sys->get_mutex());

  auto wait_restart = m_clone_task_manager.drop_task(thd, task_id, is_master);
  mutex_enter(clone_sys->get_mutex());

  /* Need to wait for restart, if network error */
  if (is_copy_clone() && m_allow_restart && wait_restart) {
    ut_ad(is_master);
    return (true);
  }

  return (false);
}

int Clone_Handle::move_to_next_state(Clone_Task *task, Ha_clone_cbk *callback,
                                     Clone_Desc_State *state_desc) {
  auto snapshot = m_clone_task_manager.get_snapshot();
  /* Use input state only for apply. */
  auto next_state =
      is_copy_clone() ? snapshot->get_next_state() : state_desc->m_state;

  Clone_Alert_Func alert_callback;

  if (is_copy_clone()) {
    /* Send Keep alive to recipient during long wait. */
    alert_callback = [&]() {
      auto err = send_keep_alive(task, callback);
      return (err);
    };
  }

  /* Move to new state */
  uint num_wait = 0;
  auto err = m_clone_task_manager.change_state(task, state_desc, next_state,
                                               alert_callback, num_wait);

  /* Need to wait for all other tasks to move over, if any. */
  if (num_wait > 0) {
    bool is_timeout = false;
    int alert_count = 0;
    err = Clone_Sys::wait_default(
        [&](bool alert, bool &result) {
          /* For multi threaded clone, master task does the state change. */
          if (task->m_is_master) {
            err = m_clone_task_manager.change_state(
                task, state_desc, next_state, alert_callback, num_wait);
          } else {
            err = m_clone_task_manager.check_state(task, next_state, false, 0,
                                                   num_wait);
          }
          result = (num_wait > 0);

          if (err == 0 && result && alert) {
            /* Print messages every 1 minute - default is 5 seconds. */
            if (++alert_count == 12) {
              alert_count = 0;
              ib::info(ER_IB_CLONE_TIMEOUT) << "Clone: master state change "
                                               "waiting for workers";
            }
            if (is_copy_clone()) {
              err = send_keep_alive(task, callback);
            }
          }
          return (err);
        },
        nullptr, is_timeout);

    if (err == 0 && !is_timeout) {
      return (0);
    }

    if (!task->m_is_master) {
      /* Exit from state transition */
      err = m_clone_task_manager.check_state(task, next_state, is_timeout, err,
                                             num_wait);
      if (err != 0 || num_wait == 0) {
        return (err);
      }
    }

    if (err == 0 && is_timeout) {
      ib::info(ER_IB_CLONE_TIMEOUT) << "Clone: state change: "
                                       "wait for other tasks timed out";

      my_error(ER_INTERNAL_ERROR, MYF(0),
               "Clone: state change wait for other tasks timed out: "
               "Wait too long for state transition");
      ut_d(ut_error);
      ut_o(return (ER_INTERNAL_ERROR));
    }
  }
  return (err);
}

void Clone_Handle::set_abort() {
  set_state(CLONE_STATE_ABORT);

  Clone_Snapshot *snapshot = m_clone_task_manager.get_snapshot();

  /* Clone is set to abort state and snapshot can never be reused. It is
  safe to mark the snapshot aborted to let any waiting DDL exit. There
  could be other tasks on their way to exit and we should not change
  the snapshot state yet. */
  if (snapshot != nullptr) {
    snapshot->set_abort();
  }
}

int Clone_Handle::open_file(Clone_Task *task, const Clone_file_ctx *file_ctx,
                            ulint file_type, bool create_file,
                            File_init_cbk &init_cbk) {
  os_file_type_t type;
  bool exists;
  std::string file_name;

  file_ctx->get_file_name(file_name);

  /* Check if file exists */
  auto status = os_file_status(file_name.c_str(), &exists, &type);

  if (!status) {
    return (0);
  }

  ulint option;
  bool read_only;

  if (create_file) {
    option = exists ? OS_FILE_OPEN : OS_FILE_CREATE_PATH;
    read_only = false;
  } else {
    ut_ad(exists);
    option = OS_FILE_OPEN;
    read_only = true;
  }

  option |= OS_FILE_ON_ERROR_NO_EXIT;
  bool success = false;

  auto handle = os_file_create(innodb_clone_file_key, file_name.c_str(), option,
                               OS_FILE_NORMAL, file_type, read_only, &success);

  int err = 0;

  if (!success) {
    /* purecov: begin inspected */
    err = (option == OS_FILE_OPEN) ? ER_CANT_OPEN_FILE : ER_CANT_CREATE_FILE;
    /* purecov: end */

  } else if (create_file && init_cbk) {
    auto db_err = init_cbk(handle);

    if (db_err != DB_SUCCESS) {
      /* purecov: begin inspected */
      os_file_close(handle);
      err = ER_ERROR_ON_WRITE;
      /* purecov: end */
    }
  }

  if (err != 0) {
    /* purecov: begin inspected */
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error(err, MYF(0), file_name.c_str(), errno,
             my_strerror(errbuf, sizeof(errbuf), errno));
    return err;
    /* purecov: end */
  }

  if (task == nullptr) {
    ut_ad(create_file);
    os_file_close(handle);
    return 0;
  }

  /* Set file descriptor in task. */
  close_file(task);
  task->m_current_file_des = handle;

  ut_ad(handle.m_file != OS_FILE_CLOSED);

  task->m_file_cache = true;

  /* Set cache to false if direct IO(O_DIRECT) is used. */
  if (file_type == OS_CLONE_DATA_FILE) {
    task->m_file_cache = !srv_is_direct_io();

    DBUG_EXECUTE_IF("clone_no_zero_copy", task->m_file_cache = false;);
  }

  auto file_meta = file_ctx->get_file_meta_read();

  /* If the task has pinned file, the index should be set. */
  ut_ad(!task->m_pinned_file ||
        task->m_current_file_index == file_meta->m_file_index);

  task->m_current_file_index = file_meta->m_file_index;

  return 0;
}

int Clone_Handle::close_file(Clone_Task *task) {
  bool success = true;

  /* Close file, if opened. */
  if (task->m_current_file_des.m_file != OS_FILE_CLOSED) {
    success = os_file_close(task->m_current_file_des);
  }

  task->m_current_file_des.m_file = OS_FILE_CLOSED;
  task->m_file_cache = true;

  if (!success) {
    my_error(ER_INTERNAL_ERROR, MYF(0), "Innodb error while closing file");
    return (ER_INTERNAL_ERROR);
  }

  return (0);
}

int Clone_Handle::file_callback(Ha_clone_cbk *cbk, Clone_Task *task, uint len,
                                bool buf_cbk, uint64_t offset
#ifdef UNIV_PFS_IO
                                ,
                                ut::Location location
#endif /* UNIV_PFS_IO */
) {
  int err;
  Ha_clone_file file;

  /* Platform specific code to set file handle */
#ifdef _WIN32
  file.type = Ha_clone_file::FILE_HANDLE;
  file.file_handle = static_cast<void *>(task->m_current_file_des.m_file);
#else
  file.type = Ha_clone_file::FILE_DESC;
  file.file_desc = task->m_current_file_des.m_file;
#endif /* _WIN32 */

  /* Register for PFS IO */
#ifdef UNIV_PFS_IO
  PSI_file_locker_state state;
  struct PSI_file_locker *locker;
  enum PSI_file_operation psi_op;

  locker = nullptr;
  psi_op = is_copy_clone() ? PSI_FILE_READ : PSI_FILE_WRITE;

  register_pfs_file_io_begin(&state, locker, task->m_current_file_des, len,
                             psi_op, location);
#endif /* UNIV_PFS_IO */

  /* Call appropriate callback to transfer data. */
  if (is_copy_clone()) {
    /* Send data from file. */
    err = cbk->file_cbk(file, len);

  } else if (buf_cbk) {
    unsigned char *data_buf = nullptr;
    uint32_t data_len = 0;
    /* Get data buffer */
    err = cbk->apply_buffer_cbk(data_buf, data_len);
    if (err == 0) {
      /* Modify and write data buffer to file. */
      err = modify_and_write(task, offset, data_buf, data_len);
    }
  } else {
    /* Write directly to file. */
    err = cbk->apply_file_cbk(file);
  }

#ifdef UNIV_PFS_IO
  register_pfs_file_io_end(locker, len);
#endif /* UNIV_PFS_IO */

  return (err);
}
