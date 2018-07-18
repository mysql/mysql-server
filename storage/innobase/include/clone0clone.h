/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/clone0clone.h
 Innodb Clone System

 *******************************************************/

#ifndef CLONE_CLONE_INCLUDE
#define CLONE_CLONE_INCLUDE

#include <chrono>
#include "db0err.h"
#include "handler.h"
#include "mysql/plugin.h"  // thd_killed()
#include "univ.i"
#include "ut0mutex.h"

#include "clone0desc.h"
#include "clone0snapshot.h"

/** Clone in progress file name length. */
const size_t CLONE_FILE_LEN = 32;

/** Clone in progress file name. */
const char CLONE_IN_PROGRESS_FILE[] = "#clone_in_progress";

using Clone_Msec = std::chrono::milliseconds;
using Clone_Sec = std::chrono::seconds;
using Clone_Min = std::chrono::minutes;

/** Default sleep time while waiting: 100 ms */
const Clone_Msec CLONE_DEF_SLEEP{100};

/** Default alert interval in multiple of sleep time: 5 seconds */
const Clone_Sec CLONE_DEF_ALERT_INTERVAL{5};

/** Default timeout in multiple of sleep time: 30 minutes */
const Clone_Min CLONE_DEF_TIMEOUT{30};

/** Clone system state */
enum Clone_System_State {
  CLONE_SYS_INACTIVE,
  CLONE_SYS_ACTIVE,
  CLONE_SYS_ABORT
};

using Clone_Sys_State = std::atomic<Clone_System_State>;

/** Clone Handle State */
enum Clone_Handle_State {
  CLONE_STATE_INIT = 1,
  CLONE_STATE_ACTIVE,
  CLONE_STATE_IDLE,
  CLONE_STATE_ABORT
};

/** Clone task state */
enum Clone_Task_State { CLONE_TASK_INACTIVE = 1, CLONE_TASK_ACTIVE };

/** Maximum number of concurrent snapshots */
const int MAX_SNAPSHOTS = 1;

/** Maximum number of concurrent clones */
const int MAX_CLONES = 1;

/** Clone system array size */
const int CLONE_ARR_SIZE = 2 * MAX_CLONES;

/** Snapshot system array size */
const int SNAPSHOT_ARR_SIZE = 2 * MAX_SNAPSHOTS;

/** Task for clone operation. Multiple task can concurrently work
on a clone operation. */
struct Clone_Task {
  /** Task Meta data */
  Clone_Task_Meta m_task_meta;

  /** Task state */
  Clone_Task_State m_task_state;

  /** Serial descriptor byte string */
  byte *m_serial_desc;

  /** Serial descriptor allocated length */
  uint m_alloc_len;

  /** Current file descriptor */
  pfs_os_file_t m_current_file_des;

  /** Current file index */
  uint m_current_file_index;

  /** Data files are read using OS buffer cache */
  bool m_file_cache;

  /** If master task */
  bool m_is_master;

  /** If task has associated session */
  bool m_has_thd;

#ifdef UNIV_DEBUG
  /** Ignore debug sync point */
  bool m_ignore_sync;

  /** Counter to restart in different state */
  int m_debug_counter;
#endif /* UNIV_DEBUG */

  /** Allocated buffer */
  byte *m_current_buffer;

  /** Allocated buffer length */
  uint m_buffer_alloc_len;

  /** Data transferred for current chunk in bytes */
  uint32_t m_data_size;
};

class Clone_Handle;

/** Task manager for manging the tasks for a clone operation */
class Clone_Task_Manager {
 public:
  /** Initialize task manager for clone handle
  @param[in]	snapshot	snapshot */
  void init(Clone_Snapshot *snapshot);

  /** Get task state mutex
  @return state mutex */
  ib_mutex_t *get_mutex() { return (&m_state_mutex); }

  /** Handle any error raised by concurrent tasks.
  @param[in]	raise_error	raise error if true
  @return error code */
  int handle_error_other_task(bool raise_error);

  /** Set error number
  @param[in]	err		error number
  @param[in]	file_name	associated file name if any  */
  void set_error(int err, const char *file_name) {
    mutex_enter(&m_state_mutex);

    ib::info(ER_IB_MSG_151) << "Clone Set Error code: " << err
                            << " Saved Error code: " << m_saved_error;

    /* Override any network error as we should not be waiting for restart
    if other errors have occurred. */
    if (m_saved_error == 0 || is_network_error(m_saved_error)) {
      m_saved_error = err;

      if (file_name != nullptr) {
        ut_ad(m_err_file_name != nullptr);
        ut_ad(m_err_file_len != 0);

        strncpy(m_err_file_name, file_name, m_err_file_len);
      }
    }

    mutex_exit(&m_state_mutex);
  }

  /** Add a task to task manager
  @param[in]	thd	server THD object
  @param[in]	ref_loc	reference locator from remote
  @param[in]	loc_len	locator length in bytes
  @param[out]	task_id	task identifier
  @return error code */
  int add_task(THD *thd, const byte *ref_loc, uint loc_len, uint &task_id);

  /** Drop task from task manager
  @param[in]	thd		server THD object
  @param[in]	task_id		current task ID
  @param[out]	is_master	true, if master task
  @return true if needs to wait for re-start */
  bool drop_task(THD *thd, uint task_id, bool &is_master);

  /** Reset chunk information for task
  @param[in]	task	current task */
  void reset_chunk(Clone_Task *task) {
    ut_ad(mutex_own(&m_state_mutex));

    /* Reset current processing chunk */
    task->m_task_meta.m_chunk_num = 0;
    task->m_task_meta.m_block_num = 0;

    if (task->m_data_size > 0) {
      ut_ad(get_state() != CLONE_SNAPSHOT_NONE);
      ut_ad(get_state() != CLONE_SNAPSHOT_INIT);
      ut_ad(get_state() != CLONE_SNAPSHOT_DONE);

      auto &monitor = m_clone_snapshot->get_clone_monitor();

      monitor.update_work(task->m_data_size);
    }

    task->m_data_size = 0;
  }

  /** Get task by index
  @param[in]	index	task index
  @return task */
  Clone_Task *get_task_by_index(uint index) {
    auto task = (m_clone_tasks + index);
    ut_ad(task->m_task_state == CLONE_TASK_ACTIVE);

    return (task);
  }

  /** Reserve next chunk from task manager. Called by individual tasks.
  @param[in]	task		requesting task
  @param[out]	ret_chunk	reserved chunk number
  @param[out]	ret_block	start block number
                                  '0' if no more chunk.
  @return error code */
  int reserve_next_chunk(Clone_Task *task, uint32_t &ret_chunk,
                         uint32_t &ret_block);

  /** Set current chunk and block information
  @param[in,out]	task		requesting task
  @param[in]	new_meta	updated task metadata
  @return error code */
  int set_chunk(Clone_Task *task, Clone_Task_Meta *new_meta);

  /** Track any incomplete chunks handled by the task
  @param[in,out]	task	current task */
  void add_incomplete_chunk(Clone_Task *task);

  /** Initialize task manager for current state */
  void init_state();

  /** Reinitialize state using locator
  @param[in]	loc	locator from remote client
  @param[in]	loc_len	locator length in bytes */
  void reinit_copy_state(const byte *loc, uint loc_len);

  /** Reinitialize state using locator
  @param[in]	ref_loc		current locator
  @param[in]	ref_len		current locator length
  @param[out]	new_loc		new locator to be sent to remote server
  @param[out]	new_len		length of new locator
  @param[in,out]	alloc_len	allocated length for locator buffer */
  void reinit_apply_state(const byte *ref_loc, uint ref_len, byte *&new_loc,
                          uint &new_len, uint &alloc_len);

  /** Reset state transition information */
  void reset_transition() {
    m_num_tasks_transit = 0;
    m_num_tasks_finished = 0;
    m_next_state = CLONE_SNAPSHOT_NONE;
  }

  /** Reset error information */
  void reset_error() {
    m_saved_error = 0;
    strncpy(m_err_file_name, "Clone File", m_err_file_len);
  }

  /** Get current clone state
  @return clone state */
  Snapshot_State get_state() { return (m_current_state); }

  /** Check if in state transition
  @return true if state transition is in progress */
  bool in_transit_state() { return (m_next_state != CLONE_SNAPSHOT_NONE); }

  /** Get attached snapshot
  @return snapshot */
  Clone_Snapshot *get_snapshot() { return (m_clone_snapshot); }

  /** Move to next snapshot state. Each task must call this after
  no more chunk is left in current state. The state can be changed
  only after all tasks have finished transferring the reserved chunks.
  @param[in]	task		clone task
  @param[in]	state_desc	descriptor for next state
  @param[in]	new_state	next state to move to
  @param[out]	num_wait	unfinished tasks in current state
  @return error code */
  int change_state(Clone_Task *task, Clone_Desc_State *state_desc,
                   Snapshot_State new_state, uint &num_wait);

  /** Check if state transition is over and all tasks moved to next state
  @param[in]	task		requesting task
  @param[in]	new_state	next state to move to
  @param[in]	exit_on_wait	exit from transition if needs to wait
  @param[in]	in_err		input error if already occurred
  @param[out]	num_wait	number of tasks to move to next state
  @return error code */
  int check_state(Clone_Task *task, Snapshot_State new_state, bool exit_on_wait,
                  int in_err, uint32_t &num_wait);

  /** Check if needs to send state metadata once
  @param[in]	task	current task
  @return true if needs to send state metadata */
  bool is_restart_metadata(Clone_Task *task) {
    if (task->m_is_master && m_send_state_meta) {
      m_send_state_meta = false;
      return (true);
    }

    return (false);
  }

  /** @return true if file metadata is transferred */
  bool is_file_metadata_transferred() const {
    return (m_transferred_file_meta);
  }

  /** Set sub-state: all file metadata is transferred */
  void set_file_meta_transferred() { m_transferred_file_meta = true; }

  /** Mark state finished for current task
  @param[in]	task	current task
  @return error code */
  int finish_state(Clone_Task *task);

  /** Set acknowledged state
  @param[in]	state_desc	State descriptor */
  void ack_state(const Clone_Desc_State *state_desc);

  /** Wait for acknowledgement
  @param[in]	clone		parent clone handle
  @param[in]	task		current task
  @param[in]	callback	user callback interface
  @return error code */
  int wait_ack(Clone_Handle *clone, Clone_Task *task, Ha_clone_cbk *callback);

  /** Check if state ACK is needed
  @param[in]	state_desc	State descriptor
  @return true if need to wait for ACK from remote */
  bool check_ack(const Clone_Desc_State *state_desc) {
    bool ret = true;

    mutex_enter(&m_state_mutex);

    /* Check if state is already acknowledged */
    if (m_ack_state == state_desc->m_state) {
      ut_ad(m_restart_count > 0);
      ret = false;
      ++m_num_tasks_finished;
    }

    mutex_exit(&m_state_mutex);

    return (ret);
  }

  /** Check if clone is restarted after failure
  @return true if restarted */
  bool is_restarted() { return (m_restart_count > 0); }

  /** Allocate buffers for current task
  @param[in,out]	task	current task
  @return error code */
  int alloc_buffer(Clone_Task *task);

#ifdef UNIV_DEBUG
  /** Wait during clone operation
  @param[in]	chunk_num	chunk number to process
  @param[in]	task		current task */
  void debug_wait(uint chunk_num, Clone_Task *task);

  /** Force restart clone operation by raising network error
  @param[in]	task		current task
  @param[in]	in_err		any err that has occurred
  @param[in]	restart_count	restart counter
  @return error code */
  int debug_restart(Clone_Task *task, int in_err, int restart_count);
#endif /* UNIV_DEBUG */

 private:
  /** Check if we need to wait before adding current task
  @param[in]	ref_loc	reference locator from remote
  @param[in]	loc_len	reference locator length
  @return true, if needs to wait */
  bool wait_before_add(const byte *ref_loc, uint loc_len);

 private:
  /** Check if network error
  @param[in]	err	error code
  @return true if network error */
  bool is_network_error(int err) {
    if (err == ER_NET_ERROR_ON_WRITE || err == ER_NET_READ_ERROR ||
        err == ER_NET_WRITE_INTERRUPTED || err == ER_NET_READ_INTERRUPTED) {
      return (true);
    }
    return (false);
  }

  /** Reserve free task from task manager and initialize
  @param[in]	thd	server THD object
  @param[out]	task_id	initialized task ID */
  void reserve_task(THD *thd, uint &task_id);

  /** Check if we should process incomplete chunk next. Incomplete
  chunks could be there after a re-start from network failure. We always
  process the chunks in order and need to choose accordingly.
  @return if need to process incomplete chunk next. */
  inline bool process_inclomplete_chunk() {
    /* 1. Check if there is any incomplete chunk. */
    auto &chunks = m_chunk_info.m_incomplete_chunks;
    if (chunks.empty()) {
      return (false);
    }

    /* 2. Check if all complete chunks are processed. */
    auto min_complete_chunk = m_chunk_info.m_min_unres_chunk;
    if (min_complete_chunk > m_chunk_info.m_total_chunks) {
      return (true);
    }

    /* 3. Compare the minimum chunk number for complete and incomplete chunk */
    auto it = chunks.begin();
    auto min_incomplete_chunk = it->first;

    ut_ad(min_complete_chunk != min_incomplete_chunk);
    return (min_incomplete_chunk < min_complete_chunk);
  }

  /** Get next in complete chunk if any
  @param[out]	block_num	first block number in chunk
  @return incomplete chunk number */
  uint32_t get_next_incomplete_chunk(uint32 &block_num);

  /** Get next unreserved chunk
  @return chunk number */
  uint32_t get_next_chunk();

 private:
  /** Mutex synchronizing access by concurrent tasks */
  ib_mutex_t m_state_mutex;

  /** Finished and incomplete chunk information */
  Chunk_Info m_chunk_info;

  /** Clone task array */
  Clone_Task m_clone_tasks[CLONE_MAX_TASKS];

  /** Current number of tasks */
  uint m_num_tasks;

  /** Number of tasks finished current state */
  uint m_num_tasks_finished;

  /** Number of tasks in transit state */
  uint m_num_tasks_transit;

  /** Number of times clone is restarted */
  uint m_restart_count;

  /** Acknowledged state from client */
  Snapshot_State m_ack_state;

  /** Current state for clone */
  Snapshot_State m_current_state;

  /** Next state: used during state transfer */
  Snapshot_State m_next_state;

  /* Sub state: File metadata is transferred */
  bool m_transferred_file_meta;

  /** Send state metadata before starting: Used for restart */
  bool m_send_state_meta;

  /** Save any error raised by a task */
  int m_saved_error;

  /** File name related to the saved error */
  char *m_err_file_name;

  /** File name length */
  size_t m_err_file_len;

  /** Attached snapshot handle */
  Clone_Snapshot *m_clone_snapshot;
};

/** Clone Handle for copying or applying data */
class Clone_Handle {
 public:
  /** Construct clone handle
  @param[in]	handle_type	clone handle type
  @param[in]	clone_version	clone version
  @param[in]	clone_index	index in clone array */
  Clone_Handle(Clone_Handle_Type handle_type, uint clone_version,
               uint clone_index);

  /** Destructor: Detach from snapshot */
  ~Clone_Handle();

  /** Initialize clone handle
  @param[in]	ref_loc		reference locator
  @param[in]	ref_len		reference locator length
  @param[in]	type		clone type
  @param[in]	data_dir	data directory for apply
  @return error code */
  int init(const byte *ref_loc, uint ref_len, Ha_clone_type type,
           const char *data_dir);

  /** Attach to the clone handle */
  void attach() { ++m_ref_count; }

  /** Detach from the clone handle
  @return reference count */
  uint detach() {
    ut_a(m_ref_count > 0);
    --m_ref_count;

    return (m_ref_count);
  }

  /** Get locator for the clone handle.
  @param[out]	loc_len	serialized locator length
  @return serialized clone locator */
  byte *get_locator(uint &loc_len);

  /** @return clone data directory */
  const char *get_datadir() const { return (m_clone_dir); }

  /** Build locator descriptor for the clone handle
  @param[out]	loc_desc	locator descriptor */
  void build_descriptor(Clone_Desc_Locator *loc_desc);

  /** Add a task to clone handle
  @param[in]	thd	server THD object
  @param[in]	ref_loc	reference locator from remote
  @param[in]	ref_len	reference locator length
  @param[out]	task_id	task identifier
  @return error code */
  int add_task(THD *thd, const byte *ref_loc, uint ref_len, uint &task_id) {
    return (m_clone_task_manager.add_task(thd, ref_loc, ref_len, task_id));
  }

  /** Drop task from clone handle
  @param[in]	thd		server THD object
  @param[in]	task_id		current task ID
  @param[in]	in_err		input error
  @param[out]	is_master	true, if master task
  @return true if needs to wait for re-start */
  bool drop_task(THD *thd, uint task_id, int in_err, bool &is_master);

  /** Save current error number
  @param[in]	err	error number */
  void save_error(int err) {
    if (err != 0) {
      m_clone_task_manager.set_error(err, nullptr);
    }
  }

  /** Check for error from other tasks and DDL
  @param[in,out]	thd	session THD
  @return error code */
  int check_error(THD *thd) {
    bool has_thd = (thd != nullptr);
    auto err = m_clone_task_manager.handle_error_other_task(has_thd);
    /* Save any error reported */
    save_error(err);
    return (err);
  }

  /** @return true if any task is interrupted */
  bool is_interrupted() {
    auto err = m_clone_task_manager.handle_error_other_task(false);
    return (err == ER_QUERY_INTERRUPTED);
  }

  /** Get clone handle index in clone array
  @return array index */
  uint get_index() { return (m_clone_arr_index); }

  /** Get clone data descriptor version
  @return version */
  uint get_version() { return (m_clone_desc_version); }

  /** Check if it is copy clone
  @return true if copy clone handle */
  bool is_copy_clone() { return (m_clone_handle_type == CLONE_HDL_COPY); }

  /** Check if clone type matches
  @param[in]	other_handle_type	type to match with
  @return true if type matches with clone handle type */
  bool match_hdl_type(Clone_Handle_Type other_handle_type) {
    return (m_clone_handle_type == other_handle_type);
  }

  /** Set current clone state
  @param[in]	state	clone handle state */
  void set_state(Clone_Handle_State state) { m_clone_handle_state = state; }

  /** Check if clone state is active
  @return true if in active state */
  bool is_active() { return (m_clone_handle_state == CLONE_STATE_ACTIVE); }

  /** Check if clone is initialized
  @return true if in initial state */
  bool is_init() { return (m_clone_handle_state == CLONE_STATE_INIT); }

  /** Check if clone is idle waiting for restart
  @return true if clone is in idle state */
  bool is_idle() { return (m_clone_handle_state == CLONE_STATE_IDLE); }

  /** Check if clone is aborted
  @return true if clone is aborted */
  bool is_abort() { return (m_clone_handle_state == CLONE_STATE_ABORT); }

  /** Restart copy after a network failure
  @param[in]	thd	server THD object
  @param[in]	loc	locator wit copy state from remote client
  @param[in]	loc_len	locator length in bytes
  @return error code */
  int restart_copy(THD *thd, const byte *loc, uint loc_len);

  /** Build locator with current state and restart apply
  @param[in]	thd	server THD object
  @param[in,out]	loc	loctor with current state information
  @param[in,out]	loc_len	locator length in bytes
  @return error code */
  int restart_apply(THD *thd, const byte *&loc, uint &loc_len);

  /** Transfer snapshot data via callback
  @param[in]	thd		server THD object
  @param[in]	task_id		current task ID
  @param[in]	callback	user callback interface
  @return error code */
  int copy(THD *thd, uint task_id, Ha_clone_cbk *callback);

  /** Apply snapshot data received via callback
  @param[in]	thd		server THD
  @param[in]	task_id		current task ID
  @param[in]	callback	user callback interface
  @return error code */
  int apply(THD *thd, uint task_id, Ha_clone_cbk *callback);

  /** Send keep alive while during long wait
  @param[in]	task		task that is sending the information
  @param[in]	callback	callback interface
  @return error code */
  int send_keep_alive(Clone_Task *task, Ha_clone_cbk *callback);

 private:
  /** Delete clone in progress file. */
  void delete_clone_file();

  /** Create clone data directory.
  @return error code */
  int create_clone_directory();

  /** Display clone progress
  @param[in]	cur_chunk	current chunk number
  @param[in]	max_chunk	total number of chunks
  @param[in,out]	percent_done	percentage completed
  @param[in,out]	disp_time	last displayed time */
  void display_progress(uint32_t cur_chunk, uint32_t max_chunk,
                        uint32_t &percent_done, ulint &disp_time);

  /** Open file for the task
  @param[in]	task		clone task
  @param[in]	file_meta	file information
  @param[in]	file_type	file type (data, log etc.)
  @param[in]	create_file	create if not present
  @param[in]	set_and_close	set size and close
  @return error code */
  int open_file(Clone_Task *task, Clone_File_Meta *file_meta, ulint file_type,
                bool create_file, bool set_and_close);

  /** Close file for the task
  @param[in]	task	clone task
  @return error code */
  int close_file(Clone_Task *task);

  /** Callback providing the file reference and data length to copy
  @param[in]	cbk	callback interface
  @param[in]	task	clone task
  @param[in]	len	data length
  @param[in]	name	file name where func invoked
  @param[in]	line	line where the func invoked
  @return error code */
  int file_callback(Ha_clone_cbk *cbk, Clone_Task *task, uint len
#ifdef UNIV_PFS_IO
                    ,
                    const char *name, uint line
#endif /* UNIV_PFS_IO */
  );

  /** Move to next state
  @param[in]	task		clone task
  @param[in]	callback	callback interface
  @param[in]	state_desc	descriptor for next state to move to
  @return error code */
  int move_to_next_state(Clone_Task *task, Ha_clone_cbk *callback,
                         Clone_Desc_State *state_desc);

  /** Send current state information via callback
  @param[in]	task		task that is sending the information
  @param[in]	callback	callback interface
  @param[in]	is_start	if it is the start of current state
  @return error code */
  int send_state_metadata(Clone_Task *task, Ha_clone_cbk *callback,
                          bool is_start);

  /** Send current task information via callback
  @param[in]	task		task that is sending the information
  @param[in]	callback	callback interface
  @return error code */
  int send_task_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Send all file information via callback
  @param[in]	task		task that is sending the information
  @param[in]	callback	callback interface
  @return error code */
  int send_all_file_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Send current file information via callback
  @param[in]	task		task that is sending the information
  @param[in]	file_meta	file meta information
  @param[in]	callback	callback interface
  @return error code */
  int send_file_metadata(Clone_Task *task, Clone_File_Meta *file_meta,
                         Ha_clone_cbk *callback);

  /** Send cloned data via callback
  @param[in]	task		task that is sending the information
  @param[in]	file_meta	file information
  @param[in]	offset		file offset
  @param[in]	buffer		data buffer or NULL if send from file
  @param[in]	size		data buffer size
  @param[in]	callback	callback interface
  @return error code */
  int send_data(Clone_Task *task, Clone_File_Meta *file_meta,
                ib_uint64_t offset, byte *buffer, uint size,
                Ha_clone_cbk *callback);

  /** Process a data chunk and send data blocks via callback
  @param[in]	task		task that is sending the information
  @param[in]	chunk_num	chunk number to process
  @param[in]	block_num	start block number
  @param[in]	callback	callback interface
  @return error code */
  int process_chunk(Clone_Task *task, uint32_t chunk_num, uint32_t block_num,
                    Ha_clone_cbk *callback);

  /** Create apply task based on task metadata in callback
  @param[in]	task		current task
  @param[in]	callback	callback interface
  @return error code */
  int apply_task_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Move to next state based on state metadata and set
  state information
  @param[in]	task		current task
  @param[in,out]	callback	callback interface
  @param[in,out]	state_desc	clone state descriptor
  @return error code */
  int ack_state_metadata(Clone_Task *task, Ha_clone_cbk *callback,
                         Clone_Desc_State *state_desc);

  /** Move to next state based on state metadata and set
  state information
  @param[in]	task		current task
  @param[in]	callback	callback interface
  @return error code */
  int apply_state_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Create file metadata based on callback
  @param[in]	task		current task
  @param[in]	callback	callback interface
  @return error code */
  int apply_file_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Apply data received via callback
  @param[in]	task		current task
  @param[in]	callback	callback interface
  @return error code */
  int apply_data(Clone_Task *task, Ha_clone_cbk *callback);

  /** Receive data from callback and apply
  @param[in]	task		task that is receiving the information
  @param[in]	offset		file offset for applying data
  @param[in]	file_size	updated file size
  @param[in]	size		data length in bytes
  @param[in]	callback	callback interface
  @return error code */
  int receive_data(Clone_Task *task, uint64_t offset, uint64_t file_size,
                   uint32_t size, Ha_clone_cbk *callback);

 private:
  /** Clone handle type: Copy, Apply */
  Clone_Handle_Type m_clone_handle_type;

  /** Clone handle state */
  Clone_Handle_State m_clone_handle_state;

  /** Fixed locator for version negotiation. */
  byte m_version_locator[CLONE_DESC_MAX_BASE_LEN];

  /** Serialized locator */
  byte *m_clone_locator;

  /** Locator length in bytes */
  uint m_locator_length;

  /** Serialized Restart locator */
  byte *m_restart_loc;

  /** Restart locator length in bytes */
  uint m_restart_loc_len;

  /** Clone descriptor version in use */
  uint m_clone_desc_version;

  /** Index in global array */
  uint m_clone_arr_index;

  /** Unique clone identifier */
  ib_uint64_t m_clone_id;

  /** Reference count */
  uint m_ref_count;

  /** Allow restart of clone operation after network failure */
  bool m_allow_restart;

  /** Clone data directory */
  const char *m_clone_dir;

  /** Clone task manager */
  Clone_Task_Manager m_clone_task_manager;
};

/** Clone System */
class Clone_Sys {
 public:
  /** Construct clone system */
  Clone_Sys();

  /** Destructor: Call during system shutdown */
  ~Clone_Sys();

  /** Create and add a new clone handle to clone system
  @param[in]	loc		locator
  @param[in]	hdl_type	handle type
  @param[out]	clone_hdl	clone handle
  @return error code */
  int add_clone(const byte *loc, Clone_Handle_Type hdl_type,
                Clone_Handle *&clone_hdl);

  /** drop a clone handle from clone system
  @param[in]	clone_handle	Clone handle */
  void drop_clone(Clone_Handle *clone_handle);

  /** Find if a clone is already running for the reference locator
  @param[in]	ref_loc		reference locator
  @param[in]	loc_len		reference locator length
  @param[in]	hdl_type	clone type
  @return clone handle if found, NULL otherwise */
  Clone_Handle *find_clone(const byte *ref_loc, uint loc_len,
                           Clone_Handle_Type hdl_type);

  /** Get the clone handle from locator by index
  @param[in]	loc	locator
  @param[in]	loc_len	locator length in bytes
  @return clone handle */
  Clone_Handle *get_clone_by_index(const byte *loc, uint loc_len);

  /** Get or create a snapshot for clone and attach
  @param[in]	hdl_type	handle type
  @param[in]	clone_type	clone type
  @param[in]	snapshot_id	snapshot identifier
  @param[in]	is_pfs_monitor	true, if needs PFS monitoring
  @param[out]	snapshot	clone snapshot
  @return error code */
  int attach_snapshot(Clone_Handle_Type hdl_type, Ha_clone_type clone_type,
                      ib_uint64_t snapshot_id, bool is_pfs_monitor,
                      Clone_Snapshot *&snapshot);

  /** Detach clone handle from snapshot
  @param[in]	snapshot	snapshot
  @param[in]	hdl_type	handle type */
  void detach_snapshot(Clone_Snapshot *snapshot, Clone_Handle_Type hdl_type);

  /** Mark clone state to abort if no active clone. If force is set,
  abort all active clones and set state to abort.
  @param[in]	force	force active clones to abort
  @return true if global state is set to abort successfully */
  bool mark_abort(bool force);

  /** Mark clone state to active if no other abort request */
  void mark_active();

  /** Get next unique ID
  @return unique ID */
  ib_uint64_t get_next_id();

  /** Get clone sys mutex
  @return clone system mutex */
  ib_mutex_t *get_mutex() { return (&m_clone_sys_mutex); }

  /** Clone System state */
  static Clone_Sys_State s_clone_sys_state;

  /** Number of active abort requests */
  static uint s_clone_abort_count;

  /** Function to check wait condition
  @param[in]	is_alert	print alert message
  @param[out]	result		true, if condition is satisfied
  @return error code */
  using Wait_Cond_Cbk_Func = std::function<int(bool, bool &)>;

  /** Wait till the condition is satisfied or timeout.
  @param[in]	sleep_time	sleep time in milliseconds
  @param[in]	timeout		total time to wait in seconds
  @param[in]	alert_interval	alert interval in seconds
  @param[in]	func		callback function for condition check
  @param[in]	mutex		release during sleep and re-acquire
  @param[out]	is_timeout	true if timeout
  @return error code returned by callback function. */
  static int wait(Clone_Msec sleep_time, Clone_Sec timeout,
                  Clone_Sec alert_interval, Wait_Cond_Cbk_Func &&func,
                  ib_mutex_t *mutex, bool &is_timeout) {
    int err = 0;
    bool wait = true;
    is_timeout = false;

    int loop_count = 0;
    auto alert_count = static_cast<int>(alert_interval / sleep_time);
    auto total_count = static_cast<int>(timeout / sleep_time);

    while (!is_timeout && wait && err == 0) {
      ++loop_count;

      /* Release input mutex */
      if (mutex != nullptr) {
        ut_ad(mutex_own(mutex));
        mutex_exit(mutex);
      }

      std::this_thread::sleep_for(sleep_time);

      /* Acquire input mutex back */
      if (mutex != nullptr) {
        mutex_enter(mutex);
      }

      auto alert = (alert_count > 0) ? (loop_count % alert_count == 0) : true;

      err = func(alert, wait);

      is_timeout = (loop_count > total_count);
    }
    return (err);
  }

  /** Wait till the condition is satisfied or default timeout.
  @param[in]	func		callback function for condition check
  @param[in]	mutex		release during sleep and re-acquire
  @param[out]	is_timeout	true if timeout
  @return error code returned by callback function. */
  static int wait_default(Wait_Cond_Cbk_Func &&func, ib_mutex_t *mutex,
                          bool &is_timeout) {
    return (wait(CLONE_DEF_SLEEP, Clone_Sec(CLONE_DEF_TIMEOUT),
                 CLONE_DEF_ALERT_INTERVAL,
                 std::forward<Wait_Cond_Cbk_Func>(func), mutex, is_timeout));
  }

 private:
  /** Check if any active clone is running.
  @param[in]	print_alert	print alert message
  @return true, if concurrent clone in progress */
  bool check_active_clone(bool print_alert);

  /** Find free index to allocate new clone handle.
  @param[in]	hdl_type	clone handle type
  @param[out]	free_index	free index in array
  @return error code */
  int find_free_index(Clone_Handle_Type hdl_type, uint &free_index);

 private:
  /** Array of clone handles */
  Clone_Handle *m_clone_arr[CLONE_ARR_SIZE];

  /** Number of copy clones */
  uint m_num_clones;

  /** Number of apply clones */
  uint m_num_apply_clones;

  /** Array of clone snapshots */
  Clone_Snapshot *m_snapshot_arr[SNAPSHOT_ARR_SIZE];

  /** Number of copy snapshots */
  uint m_num_snapshots;

  /** Number of apply snapshots */
  uint m_num_apply_snapshots;

  /** Clone system mutex */
  ib_mutex_t m_clone_sys_mutex;

  /** Clone unique ID generator */
  ib_uint64_t m_clone_id_generator;
};

/** Clone system global */
extern Clone_Sys *clone_sys;

#endif /* CLONE_CLONE_INCLUDE */
