/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file clone/clone0clone.cc
Innodb Clone System

*******************************************************/

#include "clone0clone.h"

/** Global Clone System */
Clone_Sys*	clone_sys = nullptr;

/** Clone System state */
Clone_Sys_State	Clone_Sys::s_clone_sys_state = CLONE_SYS_INACTIVE;

/** Number of active abort requests */
uint		Clone_Sys::s_clone_abort_count = 0;

/** Construct clone system */
Clone_Sys::Clone_Sys()
	:
	m_clone_arr(),
	m_num_clones(),
	m_num_apply_clones(),
	m_snapshot_arr(),
	m_num_snapshots(),
	m_num_apply_snapshots(),
	m_clone_id_generator()
{
	mutex_create(LATCH_ID_CLONE_SYS, &m_clone_sys_mutex);
}

/** Destructor: Call during system shutdown */
Clone_Sys::~Clone_Sys()
{
	mutex_free(&m_clone_sys_mutex);

#ifdef UNIV_DEBUG
	/* Verify that no active clone is present */
	int	idx;
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

/** Find if a clone is already running for the reference locator
@param[in]	ref_loc		reference locator
@param[in]	hdl_type	clone type
@return clone handle if found, NULL otherwise */
Clone_Handle*
Clone_Sys::find_clone(
	byte*			ref_loc,
	Clone_Handle_Type	hdl_type)
{
	int	idx;
	bool	match_found;

	Clone_Desc_Locator	loc_desc;
	Clone_Desc_Locator	ref_desc;
	Clone_Handle*		clone_hdl;

	mutex_own(&m_clone_sys_mutex);

	if (ref_loc == nullptr) {

		return(nullptr);
	}

	ref_desc.deserialize(ref_loc);

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

		return(clone_hdl);
	}

	return(nullptr);
}

/** Create and add a new clone handle to clone system
@param[in]	loc		locator
@param[in]	hdl_type	handle type
@return clone handle if successful, NULL otherwise */
Clone_Handle*
Clone_Sys::add_clone(
	byte*			loc,
	Clone_Handle_Type	hdl_type)
{
	Clone_Handle*	clone_hdl;
	uint		version;
	uint		idx;
	uint		free_idx = CLONE_ARR_SIZE;

	mutex_own(&m_clone_sys_mutex);

	ut_ad(m_num_clones <= MAX_CLONES);
	ut_ad(m_num_apply_clones <= MAX_CLONES);

	version = choose_desc_version(loc);

	/* Find a free index to allocate new clone. */
	for (idx = 0; idx < CLONE_ARR_SIZE; idx++) {

		clone_hdl = m_clone_arr[idx];

		if (clone_hdl == NULL) {

			if (free_idx == CLONE_ARR_SIZE) {

				free_idx = idx;
			}
			continue;
		}

		/* For apply, We can have entries in INIT state. */
		if (clone_hdl->match_hdl_type(hdl_type)
		    && clone_hdl->get_version() == version
		    && clone_hdl->is_init()) {

			ut_ad(!clone_hdl->is_copy_clone());
			return(clone_hdl);
		}
	}

	if (free_idx == CLONE_ARR_SIZE
	    || (hdl_type == CLONE_HDL_COPY && m_num_clones == MAX_CLONES)
	    || (hdl_type == CLONE_HDL_APPLY
		&& m_num_apply_clones == MAX_CLONES)) {

		my_error(ER_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_CLONES);
		return(nullptr);
	}

	/* Create a new clone. */
	clone_hdl = UT_NEW(Clone_Handle(hdl_type, version, free_idx),
			   mem_key_clone);

	if (clone_hdl == nullptr) {

		my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Clone_Handle));
		return(nullptr);
	}

	m_clone_arr[free_idx] = clone_hdl;

	if (hdl_type == CLONE_HDL_COPY) {
		++m_num_clones;
	} else {
		ut_ad(hdl_type == CLONE_HDL_APPLY);
		++m_num_apply_clones;
	}

	ut_ad(clone_hdl != nullptr);

	return(clone_hdl);
}

/** drop a clone handle from clone system
@param[in]	clone_handle	Clone handle */
void
Clone_Sys::drop_clone(
	Clone_Handle*	clone_handle)
{
	uint		index;

	mutex_own(&m_clone_sys_mutex);

	index = clone_handle->get_index();

	ut_ad(m_clone_arr[index] == clone_handle);

	m_clone_arr[index] = nullptr;

	if (clone_handle->is_copy_clone()) {

		ut_ad(m_num_clones > 0);
		--m_num_clones;

	} else {

		ut_ad(m_num_apply_clones > 0);
		--m_num_apply_clones;
	}

	UT_DELETE(clone_handle);
}

/** Get the clone handle from locator by index
@param[in]	loc	locator
@return clone handle */
Clone_Handle*
Clone_Sys::get_clone_by_index(
	byte*	loc)
{
	Clone_Desc_Locator	loc_desc;
	Clone_Handle*		clone_hdl;

	loc_desc.deserialize(loc);

#ifdef UNIV_DEBUG
	Clone_Desc_Header*	header = &loc_desc.m_header;
	ut_ad(header->m_type == CLONE_DESC_LOCATOR);
#endif
	clone_hdl = m_clone_arr[loc_desc.m_clone_index];

	ut_ad(clone_hdl != nullptr);

	return(clone_hdl);
}

/** Get or create a snapshot for clone and attach
@param[in]	hdl_type	handle type
@param[in]	clone_type	clone type
@param[in]	snapshot_id	snapshot identifier
@return clone snapshot, if successful */
Clone_Snapshot*
Clone_Sys::attach_snapshot(
	Clone_Handle_Type	hdl_type,
	Ha_clone_type		clone_type,
	ib_uint64_t		snapshot_id)
{
	Clone_Snapshot*	snapshot;
	uint		idx;
	uint		free_idx = SNAPSHOT_ARR_SIZE;

	mutex_own(&m_clone_sys_mutex);

	/* Try to attach to an existing snapshot. */
	for (idx = 0; idx < SNAPSHOT_ARR_SIZE; idx++) {

		snapshot = m_snapshot_arr[idx];

		if (snapshot != nullptr) {

			if (snapshot->attach(hdl_type)) {

				return(snapshot);
			}
		} else if (free_idx == SNAPSHOT_ARR_SIZE) {

			free_idx = idx;
		}
	}

	if (free_idx == SNAPSHOT_ARR_SIZE
	    || (hdl_type == CLONE_HDL_COPY && m_num_snapshots == MAX_SNAPSHOTS)
	    || (hdl_type == CLONE_HDL_APPLY
		&& m_num_apply_snapshots == MAX_SNAPSHOTS)) {

		my_error(ER_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_SNAPSHOTS);
		return(nullptr);
	}

	/* Create a new snapshot. */
	snapshot = UT_NEW(Clone_Snapshot(hdl_type, clone_type, free_idx,
					snapshot_id), mem_key_clone);

	if (snapshot == nullptr) {

		my_error(ER_OUTOFMEMORY, MYF(0), sizeof(Clone_Snapshot));
		return(nullptr);
	}

	m_snapshot_arr[free_idx] = snapshot;

	if (hdl_type == CLONE_HDL_COPY) {
		++m_num_snapshots;
	} else {
		ut_ad(hdl_type == CLONE_HDL_APPLY);
		++m_num_apply_snapshots;
	}

	snapshot->attach(hdl_type);

	return(snapshot);
}

/** Detach clone handle from snapshot
@param[in]	snapshot	snapshot
@param[in]	hdl_type	handle type */
void
Clone_Sys::detach_snapshot(
	Clone_Snapshot*	snapshot,
	Clone_Handle_Type	hdl_type)
{
	uint	num_clones;

	mutex_own(&m_clone_sys_mutex);
	num_clones = snapshot->detach();

	if (num_clones != 0) {

		return;
	}

	/* All clones are detached. Drop the snapshot. */
	uint	index;

	index = snapshot->get_index();
	ut_ad(m_snapshot_arr[index] == snapshot);

	UT_DELETE(snapshot);

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

/** Wait for all clone operation to exit. Caller must set
the global state to abort. */
void
Clone_Sys::wait_clone_exit()
{
	Clone_Handle*	clone_hdl;
	bool		active_clone;
	int		loop_count;
	int		idx;

	ut_ad(mutex_own(&m_clone_sys_mutex));
	ut_ad(s_clone_sys_state == CLONE_SYS_ABORT);

	loop_count = 0;

	do {
		mutex_exit(&m_clone_sys_mutex);

		/* wait for 100ms */
		os_thread_sleep(100000);

		mutex_enter(&m_clone_sys_mutex);

		active_clone = false;

		for (idx = 0; idx < CLONE_ARR_SIZE; idx++) {

			clone_hdl = m_clone_arr[idx];

			if (clone_hdl != nullptr) {

				active_clone = true;
				break;
			}
		}

		loop_count++;

		if (loop_count % 600 == 0) {

			ib::info() << "DDL waiting for CLONE to abort : "
				   << (loop_count / 600) << " minutes";

			if (loop_count > 600 * 5) {

				ib::warn() << "Active CLONE didn't abort"
				" in 5 minutes; Continuing DDL.";
				break;
			}
		}

	} while (active_clone);
}

/** Mark clone state to abort if no active clone. If force is set,
abort all active clones and set state to abort.
@param[in]	force	force active clones to abort
@return true if global state is set to abort successfully */
bool
Clone_Sys::mark_abort(
	bool	force)
{
	ut_ad(mutex_own(&m_clone_sys_mutex));

	Clone_Handle*	clone_hdl;
	int		idx;
	bool		active_clone = false;

	/* Check for active clone operations. */
	for (idx = 0; idx < CLONE_ARR_SIZE; idx++) {

		clone_hdl = m_clone_arr[idx];

		if (clone_hdl != nullptr) {

			active_clone = true;
			break;
		}
	}

	/* If active clone is running and force is not set then
	return without setting abort state. */
	if (active_clone && !force) {

		return(false);
	}

	++s_clone_abort_count;

	if (s_clone_sys_state != CLONE_SYS_ABORT) {

		ut_ad(s_clone_abort_count == 1);
		s_clone_sys_state = CLONE_SYS_ABORT;

		DEBUG_SYNC_C("clone_marked_abort");
	}

	if (active_clone) {

		ut_ad(force);
		wait_clone_exit();
	}

	return(true);
}

/** Mark clone state to active if no other abort request */
void
Clone_Sys::mark_active()
{
	ut_ad(mutex_own(&m_clone_sys_mutex));

	ut_ad(s_clone_abort_count > 0);
	--s_clone_abort_count;

	if (s_clone_abort_count == 0) {

		s_clone_sys_state = CLONE_SYS_ACTIVE;
	}
}

/** Get next unique ID
@return unique ID */
ib_uint64_t
Clone_Sys::get_next_id()
{
	ut_ad(mutex_own(&m_clone_sys_mutex));

	return(++m_clone_id_generator);
}

/** Initialize task manager for clone handle
@param[in]	snapshot	snapshot */
void
Clone_Task_Manager::init(
	Clone_Snapshot*	snapshot)
{
	uint	idx;

	m_clone_snapshot = snapshot;

	m_current_state = snapshot->get_state();

	m_total_chunks = 0;
	m_next_chunk = 0;

	/* Initialize all tasks in inactive state. */
	for (idx = 0; idx < MAX_CLONE_TASKS; idx++) {

		Clone_Task*	task;

		task = m_clone_tasks + idx;
		task->m_task_state = CLONE_TASK_INACTIVE;

		task->m_serial_desc = nullptr;
		task->m_alloc_len = 0;

		task->m_current_file_des.m_file = OS_FILE_CLOSED;
		task->m_current_file_index = 0;
		task->m_file_cache = true;

		task->m_current_buffer = nullptr;
		task->m_buffer_alloc_len = 0;
	}

	m_num_tasks = 0;
	m_num_tasks_current = 0;

	m_next_state = CLONE_SNAPSHOT_NONE;
	m_num_tasks_next = 0;
}

/** Set task to active state
@param[in]	task_meta	task details */
void
Clone_Task_Manager::set_task(
	Clone_Task_Meta* task_meta)
{
	uint		idx;
	Clone_Task*	task;

	/* Get task index */
	idx = task_meta->m_task_index;

	task = m_clone_tasks + idx;

	ut_ad(task->m_task_state == CLONE_TASK_INACTIVE);
	task->m_task_state = CLONE_TASK_ACTIVE;

	task->m_task_meta = *task_meta;
}

/** Get free task from task manager and initialize
@param[out]	task	initialized clone task
@return error code */
dberr_t
Clone_Task_Manager::get_task(
	Clone_Task*&	task)
{
	uint		idx;

	mutex_enter(&m_state_mutex);

	/* Find inactive task in the array. */
	for (idx = 0; idx < MAX_CLONE_TASKS; idx++) {

		Clone_Task_Meta*	task_meta;

		task = m_clone_tasks + idx;
		task_meta = &task->m_task_meta;

		if (task->m_task_state == CLONE_TASK_INACTIVE) {

			task->m_task_state = CLONE_TASK_ACTIVE;

			task_meta->m_task_index = idx;
			task_meta->m_chunk_num = 0;
			task_meta->m_block_num = 0;

			break;
		}
		task = nullptr;
	}

	mutex_exit(&m_state_mutex);

	ut_ad(task != nullptr);

	/* Allocate task descriptor. */
	mem_heap_t*	heap;
	uint		alloc_len;

	heap = get_heap();

	/* Maximum variable length of descriptor. */
	alloc_len = m_clone_snapshot->get_max_file_name_length();

	/* Check with maximum path name length. */
	if (alloc_len < FN_REFLEN_SE) {

		alloc_len = FN_REFLEN_SE;
	}

	/* Maximum fixed length of descriptor */
	alloc_len += CLONE_DESC_MAX_BASE_LEN;

	/* Add some buffer. */
	alloc_len += CLONE_DESC_MAX_BASE_LEN;

	task->m_alloc_len = alloc_len;

	task->m_buffer_alloc_len = m_clone_snapshot->get_dyn_buffer_length();

	alloc_len += task->m_buffer_alloc_len;

	task->m_serial_desc = static_cast<byte*>(
		mem_heap_alloc(heap, alloc_len));

	if (task->m_serial_desc == nullptr) {

		my_error(ER_OUTOFMEMORY, MYF(0), alloc_len);
		return(DB_OUT_OF_MEMORY);
	}

	if (task->m_buffer_alloc_len > 0) {

		task->m_current_buffer = task->m_serial_desc + task->m_alloc_len;
	}

	return(DB_SUCCESS);
}

/** Add a task to clone handle
@return error code */
dberr_t
Clone_Task_Manager::add_task()
{
	ut_ad(mutex_own(clone_sys->get_mutex()));

	if (m_num_tasks == MAX_CLONE_TASKS) {

		my_error(ER_TOO_MANY_CONCURRENT_CLONES, MYF(0), MAX_CLONE_TASKS);
		return(DB_ERROR);
	}

	++m_num_tasks;

	return(DB_SUCCESS);
}

/** Drop task from clone handle
@return number of tasks left */
uint
Clone_Task_Manager::drop_task()
{
	ut_ad(mutex_own(clone_sys->get_mutex()));
	ut_ad(m_num_tasks > 0);

	--m_num_tasks;

	return(m_num_tasks);
}

/** Reserve next chunk from task manager. Called by individual tasks.
@return reserved chunk number. '0' indicates no more chunk */
uint
Clone_Task_Manager::reserve_next_chunk()
{
	uint	ret_chunk;

	mutex_enter(&m_state_mutex);

	ut_ad(m_next_chunk <= m_total_chunks);

	if (m_total_chunks == m_next_chunk) {

		/* No more chunks left for current state. */
		ret_chunk = 0;
	} else {

		ret_chunk = ++m_next_chunk;
	}

	mutex_exit(&m_state_mutex);

	return(ret_chunk);
}

/** Initialize task manager for current state */
void
Clone_Task_Manager::init_state()
{
	mutex_own(&m_state_mutex);

	m_next_chunk = 0;
	m_total_chunks = m_clone_snapshot->get_num_chunks();
}

/** Move to next snapshot state. Each task must call this after
no more chunk is left in current state. The state can be changed
only after all tasks have finished transferring the reserved chunks.
@param[in]	task		clone task
@param[in]	new_state	next state to move to
@param[out]	num_wait	unfinished tasks in current state
@return error code */
dberr_t
Clone_Task_Manager::change_state(
	Clone_Task*	task,
	Snapshot_State	new_state,
	uint&		num_wait)
{
	mutex_enter(&m_state_mutex);

	/* First requesting task needs to initiate the state transition. */
	if (!in_transit_state()) {

                m_num_tasks_current = m_num_tasks;

                m_next_state = new_state;
                m_num_tasks_next = 0;
	}

	/* Move the current task over to the next state */
	--m_num_tasks_current;
	++m_num_tasks_next;

	num_wait = m_num_tasks_current;

	/* Need to wait for num_wait tasks to move over to next state. */
	if (num_wait > 0) {

		mutex_exit(&m_state_mutex);
		return(DB_SUCCESS);
	}

	/* Last task requesting the state change. All other tasks have
	already moved over to next state and waiting for the transition
	to complete. Now it is safe to do the snapshot state transition. */
	dberr_t	err;
	uint	num_pending = 0;
	uint	loop_index = 0;

	err = m_clone_snapshot->change_state(m_next_state, task->m_current_buffer,
		task->m_buffer_alloc_len, num_pending);

	if (err != DB_SUCCESS) {

		mutex_exit(&m_state_mutex);
		return(err);
	}

	/* Need to wait for other concurrent clone attached to current snapshot. */
	while (num_pending > 0) {

		/* Sleep for 100ms */
		os_thread_sleep(SNAPSHOT_STATE_CHANGE_SLEEP);
		num_pending = m_clone_snapshot->check_state(m_next_state);

		loop_index++;

		/* Wait too long - 10 minutes */
		if (loop_index >= 600 * 10) {

			mutex_exit(&m_state_mutex);
			my_error(ER_INTERNAL_ERROR, MYF(0),
				 "Innodb Snapshot state change wait too long");
			return(DB_ERROR);
		}
	}

	m_current_state = m_next_state;
	m_next_state = CLONE_SNAPSHOT_NONE;

	m_num_tasks_current = 0;
	m_num_tasks_next = 0;

	/* Initialize next state after transition. */
	init_state();

	mutex_exit(&m_state_mutex);

	return(DB_SUCCESS);
}

/** Check if state transition is over and all tasks have moved to next state
@param[in]	new_state	next state to move to
@return number of tasks yet to move over to next state */
uint
Clone_Task_Manager::check_state(
	Snapshot_State	new_state)
{
	uint	num_wait;

	mutex_enter(&m_state_mutex);

	num_wait = 0;
	if (in_transit_state() && new_state == m_next_state) {

		num_wait = m_num_tasks_current;
	}

	mutex_exit(&m_state_mutex);

	return(num_wait);
}

/** Construct clone handle
@param[in]	handle_type	clone handle type
@param[in]	clone_version	clone version
@param[in]	clone_index	index in clone array */
Clone_Handle::Clone_Handle(
	Clone_Handle_Type	handle_type,
	uint			clone_version,
	uint			clone_index)
	:
	m_clone_handle_type(handle_type),
	m_clone_handle_state(CLONE_STATE_INIT),
	m_clone_locator(),
	m_locator_length(),
	m_clone_desc_version(clone_version),
	m_clone_arr_index(clone_index),
	m_clone_id(),
	m_clone_dir(),
	m_clone_task_manager()
{
	mutex_create(LATCH_ID_CLONE_TASK, m_clone_task_manager.get_mutex());
}

/** Destructor: Detach from snapshot */
Clone_Handle::~Clone_Handle()
{
	mutex_free(m_clone_task_manager.get_mutex());

	clone_sys->detach_snapshot(m_clone_task_manager.get_snapshot(),
				   m_clone_handle_type);
}

/** Initialize clone handle
@param[in]	ref_loc		reference locator
@param[in]	type		clone type
@param[in]	data_dir	data directory for apply
@return error code */
dberr_t
Clone_Handle::init(
	byte*		ref_loc,
	Ha_clone_type	type,
	const char*	data_dir)
{
	ib_uint64_t	snapshot_id;
	Clone_Snapshot*	snapshot;

	m_clone_dir = data_dir;

	/* Generate unique clone identifiers for copy clone handle. */
	if (is_copy_clone()) {

		m_clone_id = clone_sys->get_next_id();
		snapshot_id = clone_sys->get_next_id();

	} else {

		/* Return keeping the clone in INIT state. The locator
		would only have the version information. */
		if (ref_loc == nullptr) {

			return(DB_SUCCESS);
		}

		/* Set clone identifiers from reference locator for apply clone
		handle. The reference locator is from copy clone handle. */
		Clone_Desc_Locator	loc_desc;

		loc_desc.deserialize(ref_loc);

		m_clone_id = loc_desc.m_clone_id;
		snapshot_id = loc_desc.m_snapshot_id;

		ut_ad(m_clone_id != CLONE_LOC_INVALID_ID);
		ut_ad(snapshot_id != CLONE_LOC_INVALID_ID);
	}

	/* Create and attach to snapshot. */
	snapshot = clone_sys->attach_snapshot(m_clone_handle_type, type, snapshot_id);

	if (snapshot == nullptr) {

		return(DB_ERROR);
	}

	/* Initialize clone task manager. */
	m_clone_task_manager.init(snapshot);

	m_clone_handle_state = CLONE_STATE_ACTIVE;

	return(DB_SUCCESS);
}

/** Get locator for the clone handle.
@param[out]	loc_len	serialized locator length
@return serialized clone locator */
byte*
Clone_Handle::get_locator(
	uint& loc_len)
{
	mem_heap_t*		heap;
	Clone_Desc_Locator	loc_desc;

	heap = m_clone_task_manager.get_heap();

	build_descriptor(&loc_desc);

	loc_desc.serialize(m_clone_locator, m_locator_length, heap);

	loc_len = m_locator_length;

	return(m_clone_locator);
}

/** Build locator descriptor for the clone handle
@param[out]	loc_desc	locator descriptor */
void
Clone_Handle::build_descriptor(
	Clone_Desc_Locator*	loc_desc)
{
	Clone_Snapshot*	snapshot;
	ib_uint64_t	snapshot_id = CLONE_LOC_INVALID_ID;

	snapshot = m_clone_task_manager.get_snapshot();

	if (snapshot) {

		snapshot_id = snapshot->get_id();
	}

	loc_desc->init(m_clone_id, snapshot_id,
		       m_clone_desc_version, m_clone_arr_index);
}

/** Move to next state
@param[in]	task		clone task
@param[in]	next_state	next state to move to
@return error code */
dberr_t
Clone_Handle::move_to_next_state(
	Clone_Task*	task,
	Snapshot_State	next_state)
{
	dberr_t		err;
	uint		num_wait = 0;
	uint		loop_index = 0;

	Clone_Snapshot*	snapshot;

	snapshot = m_clone_task_manager.get_snapshot();

	if (is_copy_clone()) {

		/* Use input state only for apply. */
		next_state = snapshot->get_next_state();
	}

	/* Move to new state */
	err = m_clone_task_manager.change_state(task, next_state, num_wait);

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Need to wait for all other tasks to move over, if any. */
	while (num_wait > 0) {

		/* Sleep for 100ms */
		os_thread_sleep(SNAPSHOT_STATE_CHANGE_SLEEP);

		num_wait = m_clone_task_manager.check_state(next_state);

		loop_index++;

		/* Wait too long - 10 minutes */
		if (loop_index >= 600 * 10) {

			my_error(ER_INTERNAL_ERROR, MYF(0),
				 "Innodb Clone state change wait too long");
			return(DB_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/** Open file for the task
@param[in]	task		clone task
@param[in]	file_meta	file information
@param[in]	file_type	file type (data, log etc.)
@param[in]	create_file	create if not present
@param[in]	set_and_close	set size and close
@return error code */
dberr_t
Clone_Handle::open_file(
	Clone_Task*	task,
	Clone_File_Meta*	file_meta,
	ulint		file_type,
	bool		create_file,
	bool		set_and_close)
{
	pfs_os_file_t		handle;
	os_file_type_t		type;
	os_file_create_t	option;

	bool	success;
	bool	status;
	bool	exists;
	bool	read_only;

	/* Check if file exists */
	status = os_file_status(file_meta->m_file_name, &exists, &type);
	if (!status) {

		return(DB_SUCCESS);
	}

	if (create_file) {

		option = exists ? OS_FILE_OPEN : OS_FILE_CREATE_PATH;
		read_only = false;
	} else {

		ut_ad(exists);
		option = OS_FILE_OPEN;
		read_only = true;
	}

	handle = os_file_create(innodb_clone_file_key, file_meta->m_file_name,
				option, OS_FILE_NORMAL, file_type, read_only,
				&success);

	if (success && set_and_close) {

		ut_ad(create_file);

		success = os_file_set_size(file_meta->m_file_name,
			handle, 0, file_meta->m_file_size, false, false);

		os_file_close(handle);

		if (success) {

			return(DB_SUCCESS);
		}
	}

	if (!success) {

		int	err;
		char	errbuf[MYSYS_STRERROR_SIZE];

		err = (option == OS_FILE_OPEN)
			? ER_CANT_OPEN_FILE
			: ER_CANT_CREATE_FILE;

		my_error(err, MYF(0), file_meta->m_file_name, errno,
			 my_strerror(errbuf, sizeof(errbuf), errno));

		return(DB_CANNOT_OPEN_FILE);
	}

	/* Set file descriptor in task. */
	task->m_current_file_des = handle;

	ut_ad(handle.m_file != OS_FILE_CLOSED);

	task->m_file_cache = true;

	/* Set cache to false if direct IO(O_DIRECT) is used. */
	if (file_type == OS_CLONE_DATA_FILE && srv_is_direct_io()) {

		task->m_file_cache = false;
	}

	task->m_current_file_index = file_meta->m_file_index;

	return(DB_SUCCESS);
}

/** Close file for the task
@param[in]	task	clone task
@return error code */
dberr_t
Clone_Handle::close_file(
	Clone_Task*	task)
{
	bool	success = true;

	/* Close file, if opened. */
	if (task->m_current_file_des.m_file != OS_FILE_CLOSED) {

		success = os_file_close(task->m_current_file_des);
	}

	task->m_current_file_des.m_file = OS_FILE_CLOSED;
	task->m_current_file_index = 0;
	task->m_file_cache = true;

	if (!success) {

		my_error(ER_INTERNAL_ERROR, MYF(0),
			"Innodb error while closing file");
		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/** Callback providing the file reference and data length to copy
@param[in]	cbk	callback function
@param[in]	task	clone task
@param[in]	len	data length
@param[in]	name	file name where func invoked
@param[in]      line	line where the func invoked
@return error code */
dberr_t
Clone_Handle::file_callback(
	Ha_clone_cbk*	cbk,
	Clone_Task*	task,
	uint		len
#ifdef UNIV_PFS_IO
	,
	const char*	name,
	uint		line
#endif  /* UNIV_PFS_IO */
	)
{
	int		err;
	Ha_clone_file	file;

	/* Platform specific code to set file handle */
#ifdef _WIN32
	file.type = Ha_clone_file::FILE_HANDLE;
	file.file_handle = static_cast<void*>(task->m_current_file_des.m_file);
#else
	file.type = Ha_clone_file::FILE_DESC;
	file.file_desc = task->m_current_file_des.m_file;
#endif /* _WIN32 */

	/* Register for PFS IO */
#ifdef UNIV_PFS_IO
	PSI_file_locker_state	state;
	struct PSI_file_locker*	locker;
	enum PSI_file_operation psi_op;

	locker = nullptr;
	psi_op = is_copy_clone() ? PSI_FILE_READ : PSI_FILE_WRITE;

	register_pfs_file_io_begin(&state, locker, task->m_current_file_des,
				   len, psi_op, name, line);
#endif  /* UNIV_PFS_IO */

	/* Call appropriate callback to transfer data. */
	if (is_copy_clone()) {

		err = cbk->file_cbk(file, len);
	} else {

		err = cbk->apply_file_cbk(file);
	}

#ifdef UNIV_PFS_IO
	register_pfs_file_io_end(locker, len);
#endif  /* UNIV_PFS_IO */

	return(err == 0 ? DB_SUCCESS : DB_ERROR);
}
