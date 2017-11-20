/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file clone/clone0apply.cc
Innodb apply snapshot data

*******************************************************/

#include "handler.h"
#include "clone0clone.h"
#include "log0log.h"

/** Add file metadata entry at destination
@param[in,out]	file_desc	if there, set to current descriptor
@param[in]	data_dir	destination data directory
@return error code */
dberr_t
Clone_Snapshot::add_file_from_desc(
	Clone_File_Meta*&	file_desc,
	const char*	data_dir)
{
	uint		idx;
	idx = file_desc->m_file_index;

	ut_ad(m_snapshot_handle_type == CLONE_HDL_APPLY);

	ut_ad(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY
	      || m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);

	Clone_File_Vec&	file_vector =
		(m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY)
		? m_data_file_vector : m_redo_file_vector;

	/* File metadata is already there, possibly sent by another task. */
	if (file_vector[idx] != nullptr) {

		file_desc = file_vector[idx];
		return(DB_DUPLICATE_KEY);
	}

	/* Build complete path for the new file to be added. */
	Clone_File_Meta*  file_meta;
	char*		ptr;

	ulint		alloc_size;
	ulint		dir_len;
	ulint		name_len;

	dir_len = strlen(data_dir);

	name_len = (file_desc->m_file_name == nullptr)
		   ? MAX_LOG_FILE_NAME
		   : file_desc->m_file_name_len;

	alloc_size = dir_len + 1 + name_len;
	alloc_size += sizeof(Clone_File_Meta);

	ptr = static_cast<char*>(mem_heap_alloc(m_snapshot_heap, alloc_size));

	if (ptr == nullptr) {

		my_error(ER_OUTOFMEMORY, MYF(0), alloc_size);
		return(DB_OUT_OF_MEMORY);
	}

	file_meta = reinterpret_cast<Clone_File_Meta*>(ptr);
	*file_meta = *file_desc;

	ptr += sizeof(Clone_File_Meta);
	name_len = 0;

	strcpy(ptr, data_dir);

	file_meta->m_file_name = static_cast<const char*>(ptr);

	/* Add path separator at the end of data directory if not there. */
	if (ptr[dir_len - 1] != OS_PATH_SEPARATOR) {

		ptr[dir_len] = OS_PATH_SEPARATOR;
		ptr++;
		name_len++;
	}
	ptr += dir_len;
	name_len += dir_len;

	const char*	name_ptr;
	char		name_buf[MAX_LOG_FILE_NAME];

	if (m_snapshot_state == CLONE_SNAPSHOT_FILE_COPY) {

		name_ptr = file_desc->m_file_name;

		/* For absolute path, we must ensure that the file is not
		present. This would always fail for local clone. */
		if (is_absolute_path(file_desc->m_file_name)) {

			os_file_type_t	type;
			bool		exists = false;

			os_file_status(file_desc->m_file_name, &exists, &type);

			if (exists) {

				my_error(ER_FILE_EXISTS_ERROR, MYF(0),
					 file_desc->m_file_name);
				return(DB_TABLESPACE_EXISTS);
			}

		} else if (name_ptr[0] == '.' && name_ptr[1] == OS_PATH_SEPARATOR) {

			name_ptr += 2;
		}
	} else {

		ut_ad(m_snapshot_state == CLONE_SNAPSHOT_REDO_COPY);

		/* This is redo file. Use standard name. */
		snprintf(name_buf, MAX_LOG_FILE_NAME, "%s%u",
			 ib_logfile_basename, idx);

		name_ptr = const_cast<const char*>(name_buf);
	}

	strcpy(ptr, name_ptr);

	name_len += strlen(name_ptr);
	name_len++;

	file_meta->m_file_name_len = name_len;

	file_vector[idx] = file_meta;

	file_desc = file_meta;

	return(DB_SUCCESS);
}

/** Create apply task based on task metadata in callback
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::apply_task_metadata(
	Ha_clone_cbk*	callback)
{
	byte*			serial_desc;
	Clone_Desc_Task_Meta	task_desc;

	ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

	serial_desc = callback->get_data_desc(nullptr);

	task_desc.deserialize(serial_desc);

	m_clone_task_manager.set_task(&task_desc.m_task_meta);

	return(DB_SUCCESS);
}

/** Move to next state based on state metadata and set state information
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::apply_state_metadata(
	Ha_clone_cbk*	callback)
{
	dberr_t		err;
	byte*		serial_desc;
	uint		task_idx;
	Clone_Task*	task;

	Clone_Desc_State	state_desc;
	Clone_Snapshot*		snapshot;

	ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

	serial_desc = callback->get_data_desc(nullptr);
	state_desc.deserialize(serial_desc);

	snapshot = m_clone_task_manager.get_snapshot();

	/* Get task by index in descriptor. */
	task_idx = state_desc.m_task_index;
	task = m_clone_task_manager.get_task_by_index(task_idx);

	/* Close file in current state. */
	err = close_file(task);

	if (err != DB_SUCCESS) {

		return(err);
	}

	err = move_to_next_state(task, state_desc.m_state);

	if (err != DB_SUCCESS) {

		return(err);
	}

	snapshot->set_state_info(&state_desc);

	return(DB_SUCCESS);
}

/** Create file metadata based on callback
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::apply_file_metadata(
	Ha_clone_cbk*	callback)
{
	dberr_t				err = DB_SUCCESS;
	byte*				serial_desc;
	Clone_Desc_File_MetaData	file_desc;
	Clone_File_Meta*		file_meta;

	ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

	serial_desc = callback->get_data_desc(nullptr);

	file_desc.deserialize(serial_desc);
	file_meta = &file_desc.m_file_meta;

	Clone_Snapshot*	snapshot;
	snapshot = m_clone_task_manager.get_snapshot();

	ut_ad(snapshot->get_state() == file_desc.m_state);

	/* Add file metadata entry based on the descriptor. */
	err = snapshot->add_file_from_desc(file_meta, m_clone_dir);

	if (err != DB_SUCCESS && err != DB_DUPLICATE_KEY) {

		return(err);
	}

	if (file_desc.m_state == CLONE_SNAPSHOT_FILE_COPY) {

		return(DB_SUCCESS);
	}

	ut_ad(file_desc.m_state == CLONE_SNAPSHOT_REDO_COPY);

	/* open and reserve the redo file size */
	err = open_file(nullptr, file_meta, OS_CLONE_LOG_FILE, true, true);

	/* For redo copy, add entry for the second file. */
	if (err == DB_SUCCESS && file_meta->m_file_index == 0) {

		file_meta = &file_desc.m_file_meta;
		file_meta->m_file_index++;

		err = snapshot->add_file_from_desc(file_meta, m_clone_dir);

		if (err == DB_SUCCESS) {
			err = open_file(nullptr, file_meta, OS_CLONE_LOG_FILE,
					true, true);
		} else if (err == DB_DUPLICATE_KEY) {

			err = DB_SUCCESS;
		}
	}

	return(err);
}

/** Receive data from callback and apply
@param[in]	task		task that is receiving the information
@param[in]	offset		file offset for applying data
@param[in]	size		data length in bytes
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::receive_data(
	Clone_Task*	task,
	ib_uint64_t	offset,
	uint		size,
	Ha_clone_cbk*	callback)
{
	dberr_t			err;
	Clone_Snapshot*		snapshot;
	Clone_File_Meta*	file_meta;

	ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

	snapshot = m_clone_task_manager.get_snapshot();

	file_meta = snapshot->get_file_by_index(task->m_current_file_index);

	/* Open destination file for first block. */
	if (task->m_current_file_des.m_file == OS_FILE_CLOSED) {


		ut_ad(file_meta != nullptr);

		err = open_file(task, file_meta, OS_CLONE_LOG_FILE,
				   true, false);

		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	/* Copy data to current destination file using callback. */
	os_file_t	file_hdl;
	bool		success;
	char		errbuf[MYSYS_STRERROR_SIZE];

	file_hdl = task->m_current_file_des.m_file;
	success = os_file_seek(nullptr, file_hdl, offset);
	if (!success) {

		my_error(ER_ERROR_ON_READ, MYF(0), file_meta->m_file_name, errno,
			 my_strerror(errbuf, sizeof(errbuf), errno));
		return(DB_ERROR);
	}

	callback->set_dest_name(file_meta->m_file_name);

	err = file_callback(callback, task, size
#ifdef UNIV_PFS_IO
			    , __FILE__, __LINE__
#endif  /* UNIV_PFS_IO */
			    );

	return(err);
}

/** Apply data received via callback
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::apply_data(
	Ha_clone_cbk*	callback)
{
	dberr_t		err = DB_SUCCESS;
	byte*		serial_desc;
	Clone_Desc_Data	data_desc;

	ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

	/* Extract the data descriptor. */
	serial_desc = callback->get_data_desc(nullptr);

	data_desc.deserialize(serial_desc);

	/* Identify the task for the current block of data. */
	Clone_Task_Meta*	task_meta;
	task_meta = &data_desc.m_task_meta;

	Clone_Task*	task;
	task = m_clone_task_manager.get_task_by_index(task_meta->m_task_index);

	/* The data is from a different file. Close the current one. */
	if (task->m_current_file_index != data_desc.m_file_index) {

		err = close_file(task);
		if (err != DB_SUCCESS) {

			return(err);
		}
		task->m_current_file_index = data_desc.m_file_index;
	}

	/* Receive data from callback and apply. */
	err = receive_data(task, data_desc.m_file_offset, data_desc.m_data_len,
			   callback);

	task->m_task_meta = *task_meta;

	return(err);
}

/** Apply snapshot data received via callback
@param[in]	callback	user callback interface
@return error code */
dberr_t
Clone_Handle::apply(
	Ha_clone_cbk*	callback)
{
	dberr_t		err = DB_SUCCESS;
	Clone_Desc_Header	header;
	byte*		clone_desc;

	clone_desc = callback->get_data_desc(nullptr);
	ut_ad(clone_desc != nullptr);

	ut_ad(m_clone_handle_type == CLONE_HDL_APPLY);

	header.deserialize(clone_desc);

	/* Check the descriptor type in header and apply */
	switch(header.m_type) {

	case CLONE_DESC_TASK_METADATA:
		err = apply_task_metadata(callback);
		break;

	case CLONE_DESC_STATE:
		err = apply_state_metadata(callback);
		break;

	case CLONE_DESC_FILE_METADATA:
		err = apply_file_metadata(callback);
		break;

	case CLONE_DESC_DATA:
		err = apply_data(callback);
		break;

	default:
		ut_ad(false);
		break;
	}

	return(err);
}
