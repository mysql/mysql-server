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

#include "db0err.h"
#include "handler.h"
#include "univ.i"
#include "ut0mutex.h"

#include "clone0desc.h"
#include "clone0snapshot.h"

/** Clone system state */
enum Clone_Sys_State {
  CLONE_SYS_INACTIVE = 1,
  CLONE_SYS_ACTIVE,
  CLONE_SYS_ABORT
};

/** Clone Handle State */
enum Clone_Handle_State {
  CLONE_STATE_INIT = 1,
  CLONE_STATE_ACTIVE,
  CLONE_STATE_IDLE,
};

/** Clone task state */
enum Clone_Task_State {
  CLONE_TASK_INACTIVE = 1,
  CLONE_TASK_ACTIVE,
  CLONE_TASK_WAITING
};

/** Maximum number of concurrent snapshots */
const int MAX_SNAPSHOTS = 1;

/** Maximum number of concurrent clones */
const int MAX_CLONES = 1;

/** Clone system array size */
const int CLONE_ARR_SIZE = 2 * MAX_CLONES;

/** Snapshot system array size */
const int SNAPSHOT_ARR_SIZE = 2 * MAX_SNAPSHOTS;

/** Maximum number of concurrent tasks for each clone */
const int MAX_CLONE_TASKS = 1;

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

  /** Allocated buffer */
  byte *m_current_buffer;

  /** Allocated buffer length */
  uint m_buffer_alloc_len;
};

/** Task manager for manging the tasks for a clone operation */
class Clone_Task_Manager {
 public:
  /** Initialize task manager for clone handle
  @param[in]	snapshot	snapshot */
  void init(Clone_Snapshot *snapshot);

  /** Get task state mutex
  @return state mutex */
  ib_mutex_t *get_mutex() { return (&m_state_mutex); }

  /** Set task to active state
  @param[in]	task_meta	task details */
  void set_task(Clone_Task_Meta *task_meta);

  /** Get free task from task manager and initialize
  @param[out]	task	initialized clone task
  @return error code */
  dberr_t get_task(Clone_Task *&task);

  /** Add a task to clone handle
  @return error code */
  dberr_t add_task();

  /** Drop task from clone handle
  @return number of tasks left */
  uint drop_task();

  /** Get task by index
  @param[in]	index	task index
  @return task */
  Clone_Task *get_task_by_index(uint index) { return (m_clone_tasks + index); }

  /** Reserve next chunk from task manager. Called by individual tasks.
  @return reserved chunk number. '0' indicates no more chunk */
  uint reserve_next_chunk();

  /** Initialize task manager for current state */
  void init_state();

  /** Get current clone state
  @return clone state */
  Snapshot_State get_state() { return (m_current_state); }

  /** Check if in state transition
  @return true if state transition is in progress */
  bool in_transit_state() { return (m_next_state != CLONE_SNAPSHOT_NONE); }

  /** Get attached snapshot
  @return snapshot */
  Clone_Snapshot *get_snapshot() { return (m_clone_snapshot); }

  /** Get heap from snapshot
  @return memory heap from snapshot */
  mem_heap_t *get_heap() { return (m_clone_snapshot->get_heap()); }

  /** Move to next snapshot state. Each task must call this after
  no more chunk is left in current state. The state can be changed
  only after all tasks have finished transferring the reserved chunks.
  @param[in]	task		clone task
  @param[in]	new_state	next state to move to
  @param[out]	num_wait	unfinished tasks in current state
  @return error code */
  dberr_t change_state(Clone_Task *task, Snapshot_State new_state,
                       uint &num_wait);

  /** Check if state transition is over and all tasks have moved to next state
  @param[in]	new_state	next state to move to
  @return number of tasks yet to move over to next state */
  uint check_state(Snapshot_State new_state);

 private:
  /** Mutex synchronizing access by concurrent tasks */
  ib_mutex_t m_state_mutex;

  /** Chunks for current state */
  uint m_total_chunks;

  /** Next chunk to copy */
  uint m_next_chunk;

  /** Clone task array */
  Clone_Task m_clone_tasks[MAX_CLONE_TASKS];

  /** Current number of tasks */
  uint m_num_tasks;

  /** Current state for clone */
  Snapshot_State m_current_state;

  /** Number of tasks in current state */
  uint m_num_tasks_current;

  /** Next state: used during state transfer */
  Snapshot_State m_next_state;

  /** Number of tasks moved over to next state */
  uint m_num_tasks_next;

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
  @param[in]	type		clone type
  @param[in]	data_dir	data directory for apply
  @return error code */
  dberr_t init(byte *ref_loc, Ha_clone_type type, const char *data_dir);

  /** Get locator for the clone handle.
  @param[out]	loc_len	serialized locator length
  @return serialized clone locator */
  byte *get_locator(uint &loc_len);

  /** Build locator descriptor for the clone handle
  @param[out]	loc_desc	locator descriptor */
  void build_descriptor(Clone_Desc_Locator *loc_desc);

  /** Add a task to clone handle
  @return error code */
  dberr_t add_task() { return (m_clone_task_manager.add_task()); }

  /** Drop task from clone handle
  @return number of tasks left */
  uint drop_task() {
    auto index = m_clone_task_manager.drop_task();
    auto task = m_clone_task_manager.get_task_by_index(index);

    /* Close the current file for task. */
    if (task->m_task_state == CLONE_TASK_ACTIVE) {
      close_file(task);
      task->m_task_state = CLONE_TASK_INACTIVE;
    }

    return (index);
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

  /** Check if clone state is active
  @return true if in active state */
  bool is_active() { return (m_clone_handle_state == CLONE_STATE_ACTIVE); }

  /** Check if clone is initialized
  @return true if in initial state */
  bool is_init() { return (m_clone_handle_state == CLONE_STATE_INIT); }

  /** Transfer snapshot data via callback
  @param[in]	callback	user callback interface
  @return error code */
  dberr_t copy(Ha_clone_cbk *callback);

  /** Apply snapshot data received via callback
  @param[in]	callback	user callback interface
  @return error code */
  dberr_t apply(Ha_clone_cbk *callback);

 private:
  /** Open file for the task
  @param[in]	task		clone task
  @param[in]	file_meta	file information
  @param[in]	file_type	file type (data, log etc.)
  @param[in]	create_file	create if not present
  @param[in]	set_and_close	set size and close
  @return error code */
  dberr_t open_file(Clone_Task *task, Clone_File_Meta *file_meta,
                    ulint file_type, bool create_file, bool set_and_close);

  /** Close file for the task
  @param[in]	task	clone task
  @return error code */
  dberr_t close_file(Clone_Task *task);

  /** Callback providing the file reference and data length to copy
  @param[in]	cbk	callback function
  @param[in]	task	clone task
  @param[in]	len	data length
  @param[in]	name	file name where func invoked
  @param[in]      line	line where the func invoked
  @return error code */
  dberr_t file_callback(Ha_clone_cbk *cbk, Clone_Task *task, uint len
#ifdef UNIV_PFS_IO
                        ,
                        const char *name, uint line
#endif /* UNIV_PFS_IO */
  );

  /** Move to next state
  @param[in]	task		clone task
  @param[in]	next_state	next state to move to
  @return error code */
  dberr_t move_to_next_state(Clone_Task *task, Snapshot_State next_state);

  /** Send current state information via callback
  @param[in]	task		task that is sending the information
  @param[in]	callback	callback interface
  @return error code */
  dberr_t send_state_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Send current task information via callback
  @param[in]	task		task that is sending the information
  @param[in]	callback	callback interface
  @return error code */
  dberr_t send_task_metadata(Clone_Task *task, Ha_clone_cbk *callback);

  /** Send current file information via callback
  @param[in]	task		task that is sending the information
  @param[in]	file_meta	file meta information
  @param[in]	callback	callback interface
  @return error code */
  dberr_t send_file_metadata(Clone_Task *task, Clone_File_Meta *file_meta,
                             Ha_clone_cbk *callback);

  /** Send cloned data via callback
  @param[in]	task		task that is sending the information
  @param[in]	file_meta	file information
  @param[in]	offset		file offset
  @param[in]	buffer		data buffer or NULL if send from file
  @param[in]	size		data buffer size
  @param[in]	callback	callback interface
  @return error code */
  dberr_t send_data(Clone_Task *task, Clone_File_Meta *file_meta,
                    ib_uint64_t offset, byte *buffer, uint size,
                    Ha_clone_cbk *callback);

  /** Process a data chunk and send data blocks via callback
  @param[in]	task		task that is sending the information
  @param[in]	chunk_num	chunk number to process
  @param[in]	callback	callback interface
  @return error code */
  dberr_t process_chunk(Clone_Task *task, uint chunk_num,
                        Ha_clone_cbk *callback);

  /** Create apply task based on task metadata in callback
  @param[in]	callback	callback interface
  @return error code */
  dberr_t apply_task_metadata(Ha_clone_cbk *callback);

  /** Move to next state based on state metadata and set
  state information
  @param[in]	callback	callback interface
  @return error code */
  dberr_t apply_state_metadata(Ha_clone_cbk *callback);

  /** Create file metadata based on callback
  @param[in]	callback	callback interface
  @return error code */
  dberr_t apply_file_metadata(Ha_clone_cbk *callback);

  /** Apply data received via callback
  @param[in]	callback	callback interface
  @return error code */
  dberr_t apply_data(Ha_clone_cbk *callback);

  /** Receive data from callback and apply
  @param[in]	task		task that is receiving the information
  @param[in]	offset		file offset for applying data
  @param[in]	file_size	updated file size
  @param[in]	size		data length in bytes
  @param[in]	callback	callback interface
  @return error code */
  dberr_t receive_data(Clone_Task *task, uint64_t offset, uint64_t file_size,
                       uint32_t size, Ha_clone_cbk *callback);

 private:
  /** Clone handle type: Copy, Apply */
  Clone_Handle_Type m_clone_handle_type;

  /** Clone handle state */
  Clone_Handle_State m_clone_handle_state;

  /** Serialized locator */
  byte *m_clone_locator;

  /** Locator length in bytes */
  uint m_locator_length;

  /** Clone descriptor version in use */
  uint m_clone_desc_version;

  /** Index in global array */
  uint m_clone_arr_index;

  /** Unique clone identifier */
  ib_uint64_t m_clone_id;

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
  @return clone handle if successful, NULL otherwise */
  Clone_Handle *add_clone(byte *loc, Clone_Handle_Type hdl_type);

  /** drop a clone handle from clone system
  @param[in]	clone_handle	Clone handle */
  void drop_clone(Clone_Handle *clone_handle);

  /** Find if a clone is already running for the reference locator
  @param[in]	ref_loc		reference locator
  @param[in]	hdl_type	clone type
  @return clone handle if found, NULL otherwise */
  Clone_Handle *find_clone(byte *ref_loc, Clone_Handle_Type hdl_type);

  /** Get the clone handle from locator by index
  @param[in]	loc	locator
  @return clone handle */
  Clone_Handle *get_clone_by_index(byte *loc);

  /** Get or create a snapshot for clone and attach
  @param[in]	hdl_type	handle type
  @param[in]	clone_type	clone type
  @param[in]	snapshot_id	snapshot identifier
  @return clone snapshot, if successful */
  Clone_Snapshot *attach_snapshot(Clone_Handle_Type hdl_type,
                                  Ha_clone_type clone_type,
                                  ib_uint64_t snapshot_id);

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

 private:
  /** Wait for all clone operation to exit. Caller must set
  the global state to abort. */
  void wait_clone_exit();

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
