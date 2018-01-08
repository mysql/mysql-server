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
@file clone/clone0copy.cc
Innodb copy snapshot data

*******************************************************/

#include "handler.h"
#include "srv0start.h"
#include "clone0clone.h"
#include "fsp0sysspace.h"
#include "buf0dump.h"
#include "dict0dict.h"

/** Callback to add an archived redo file to current snapshot
@param[in]	file_name	file name
@param[in]	file_size	file size in bytes
@param[in]	file_offset	start offset in bytes
@param[in]	context		snapshot
@return	error code */
static dberr_t
add_redo_file_callback(
	char*		file_name,
	ib_uint64_t	file_size,
	ib_uint64_t	file_offset,
	void*		context)
{
	dberr_t	err;

	Clone_Snapshot*	snapshot;
	snapshot = static_cast<Clone_Snapshot*>(context);

	err = snapshot->add_redo_file(file_name, file_size, file_offset);

	return(err);
}

/** Callback to add tracked page IDs to current snapshot
@param[in]	context		snapshot
@param[in]	buff		buffer having page IDs
@param[in]	num_pages	number of tracked pages
@return	error code */
static dberr_t
add_page_callback(
	void*	context,
	byte*	buff,
	uint	num_pages)
{
	dberr_t		err;
	uint		index;
	Clone_Snapshot*	snapshot;

	ib_uint32_t	space_id;
	ib_uint32_t	page_num;

	snapshot = static_cast<Clone_Snapshot*>(context);

	/* Extract the page Ids from the buffer. */
	for (index = 0; index < num_pages; index++) {

		space_id = mach_read_from_4(buff);
		buff += 4;

		page_num = mach_read_from_4(buff);
		buff += 4;

		err = snapshot->add_page(space_id, page_num);

		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	return(DB_SUCCESS);
}

/** Add buffer pool dump file to the file list
@return error code */
dberr_t
Clone_Snapshot::add_buf_pool_file()
{
	dberr_t		err = DB_SUCCESS;

	os_file_type_t	type;
	os_file_size_t	file_size;

	ib_uint64_t	size_bytes;
	char		path[OS_FILE_MAX_PATH];
	bool		exists = false;

	/* Generate the file name. */
	buf_dump_generate_path(path, sizeof(path));

	os_file_status(path, &exists, &type);

	/* Add if the file is found. */
	if (exists) {

		file_size = os_file_get_size(path);
		size_bytes = file_size.m_total_size;

		/* Always the first file in list */
		ut_ad(m_num_data_files == 0);

		err = add_file(path, size_bytes,
			       dict_sys_t::s_invalid_space_id, true);
	}

	return(err);
}

/** Initialize snapshot state for file copy
@return error code */
dberr_t
Clone_Snapshot::init_file_copy()
{
	dberr_t	err = DB_SUCCESS;

	ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

	/* If not blocking clone, allocate redo header and trailer buffer. */
	if (m_snapshot_type != HA_CLONE_BLOCKING) {

		m_redo_ctx.get_header_size(m_redo_file_size, m_redo_header_size,
					   m_redo_trailer_size);

		m_redo_header = static_cast<byte*>(mem_heap_alloc(
			m_snapshot_heap,
			m_redo_header_size + m_redo_trailer_size));

		if (m_redo_header == nullptr) {

			my_error(ER_OUTOFMEMORY, MYF(0),
				 m_redo_header_size + m_redo_trailer_size);

			return(DB_OUT_OF_MEMORY);
		}

		m_redo_trailer = m_redo_header + m_redo_header_size;
	}

	if (m_snapshot_type == HA_CLONE_REDO) {

		/* Start Redo Archiving */
		err = m_redo_ctx.start(m_redo_header, m_redo_header_size);

	} else if (m_snapshot_type == HA_CLONE_HYBRID
		   || m_snapshot_type == HA_CLONE_PAGE) {

		/* Start modified Page ID Archiving */
		err = m_page_ctx.start();
	} else {

		ut_ad(m_snapshot_type == HA_CLONE_BLOCKING);
	}

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Add buffer pool dump file. Always the first one in the list. */
	err = add_buf_pool_file();

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Do not include redo files in file list. */
	bool	include_log = (m_snapshot_type == HA_CLONE_BLOCKING);

	/* Iterate all tablespace files and add persistent data files. */
	err = Fil_iterator::for_each_file(include_log, [&] (fil_node_t* file)
	{
		return(add_node(file));
	});

	if (err != DB_SUCCESS) {

		return(err);
	}

	ib::info() << "Clone State FILE COPY : " << m_num_current_chunks
		   << " chunks, " << " chunk size : "
		   << (chunk_size() * UNIV_PAGE_SIZE) / (1024 * 1024) << " M";

	return(err);
}

/** Initialize snapshot state for page copy
@param[in]	page_buffer	temporary buffer to copy page IDs
@param[in]	page_buffer_len	buffer length
@return error code */
dberr_t
Clone_Snapshot::init_page_copy(
	byte*	page_buffer,
	uint	page_buffer_len)
{
	dberr_t	err = DB_SUCCESS;

	ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

	if (m_snapshot_type == HA_CLONE_HYBRID) {

		/* Start Redo Archiving */
		err = m_redo_ctx.start(m_redo_header, m_redo_header_size);

	} else if (m_snapshot_type == HA_CLONE_PAGE) {

		/* Start COW for all modified pages - Not implemented. */
		ut_ad(false);
	} else {

		ut_ad(false);
	}

	if (err != DB_SUCCESS) {
		goto func_end;
	}

	/* Stop modified page archiving. */
	err = m_page_ctx.stop();

	if (err != DB_SUCCESS) {
		goto func_end;
	}

	/* Collect modified page Ids from Page Archiver. */
	void*	context;
	uint	aligned_size;

	context = static_cast<void*>(this);

	err = m_page_ctx.get_pages(add_page_callback, context, page_buffer,
				 page_buffer_len);

	m_page_vector.assign(m_page_set.begin(), m_page_set.end());

	aligned_size = ut_calc_align(m_num_pages, chunk_size());
	m_num_current_chunks = aligned_size >> m_chunk_size_pow2;

	ib::info() << "Clone State PAGE COPY : " << m_num_pages <<" pages, "
		   << m_num_duplicate_pages << " duplicate pages, "
		   << m_num_current_chunks
		   << " chunks, " << " chunk size : "
		   << (chunk_size() * UNIV_PAGE_SIZE) / (1024 * 1024) << " M";

func_end:
	m_page_ctx.release();

	return(err);
}

/** Initialize snapshot state for redo copy
@return error code */
dberr_t
Clone_Snapshot::init_redo_copy()
{
	dberr_t	err;

	ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);
	ut_ad(m_snapshot_type != HA_CLONE_BLOCKING);

	/* Stop redo archiving. */
	err = m_redo_ctx.stop(m_redo_trailer, m_redo_trailer_size,
			      m_redo_trailer_offset);

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Collect archived redo log files from Log Archiver. */
	void*	context;

	context = static_cast<void*>(this);

	err = m_redo_ctx.get_files(add_redo_file_callback, context);

	/* Add another chunk for the redo log header. */
	++m_num_redo_chunks;

#ifdef HAVE_PSI_STAGE_INTERFACE
	m_monitor.add_estimate(m_redo_header_size);
#endif

	/* Add another chunk for the redo log trailer. */
	++m_num_redo_chunks;
#ifdef HAVE_PSI_STAGE_INTERFACE
	m_monitor.add_estimate(m_redo_trailer_size);
#endif

	m_num_current_chunks = m_num_redo_chunks;

	ib::info() << "Clone State REDO COPY : " << m_num_current_chunks
		   << " chunks, " << " chunk size : "
		   << (chunk_size() * UNIV_PAGE_SIZE) / (1024 * 1024) << " M";

	return(err);
}

/** Build file metadata entry
@param[in]	file_name	name of the file
@param[in]	file_size	file size in bytes
@param[in]	file_offset	start offset
@param[in]	num_chunks	total number of chunks in the file
@param[in]	copy_file_name	copy the file name or use reference
@return file metadata entry */
Clone_File_Meta*
Clone_Snapshot::build_file(
	const char*	file_name,
	ib_uint64_t	file_size,
	ib_uint64_t	file_offset,
	uint&		num_chunks,
	bool		copy_file_name)
{
	Clone_File_Meta*	file_meta;

	ib_uint64_t	aligned_size;
	ib_uint64_t	size_in_pages;

	/* Allocate for file metadata from snapshot heap. */
	aligned_size = sizeof(Clone_File_Meta);

	if (file_name != nullptr && copy_file_name) {

		aligned_size += strlen(file_name) + 1;
	}

	file_meta = static_cast<Clone_File_Meta*>(mem_heap_alloc(
		m_snapshot_heap, aligned_size));

	if (file_meta == nullptr) {

		my_error(ER_OUTOFMEMORY, MYF(0),
			 static_cast<int>(aligned_size));
		return(file_meta);
	}

	/* For redo file with no data, add dummy entry. */
	if (file_name == nullptr) {

		num_chunks = 1;

		file_meta->m_file_name = nullptr;
		file_meta->m_file_name_len = 0;
		file_meta->m_file_size = 0;

		file_meta->m_begin_chunk = 1;
		file_meta->m_end_chunk = 1;

		return(file_meta);
	}

	file_meta->m_file_size = file_size;

	if (file_offset != 0) {

		/* reduce offset amount from total size */
		ut_ad(file_size >= file_offset);
		file_size -= file_offset;
	}

	/* Calculate and set chunk parameters. */
	size_in_pages = ut_uint64_align_up(file_size, UNIV_PAGE_SIZE);
	size_in_pages /= UNIV_PAGE_SIZE;

	aligned_size = ut_uint64_align_up(size_in_pages, chunk_size());

	num_chunks = static_cast<uint>(aligned_size >> m_chunk_size_pow2);

	file_meta->m_begin_chunk = m_num_current_chunks + 1;
	file_meta->m_end_chunk = m_num_current_chunks + num_chunks;

	file_meta->m_file_name_len = strlen(file_name) + 1;

	if (copy_file_name) {

		char*	tmp_name = reinterpret_cast<char*>(file_meta + 1);

		strcpy(tmp_name, file_name);
		file_meta->m_file_name = const_cast<const char*>(tmp_name);
	} else {

		/* We use the same pointer as the tablespace and files
		should not be dropped or changed during clone. */
		file_meta->m_file_name = file_name;
	}

	return(file_meta);
}

/** Check if the tablespace file is temporary file created by DDL
@param[in]	node	file node
@return true if created by DDL */
static
bool
is_ddl_temp_table(
	fil_node_t*	node)
{
	const char*	name_ptr;

	name_ptr = strrchr(node->name, OS_PATH_SEPARATOR);

	if (name_ptr == nullptr) {

		name_ptr = node->name;
	} else {

		name_ptr++;
	}

	/* Check if it is a temporary table created by DDL. This is work
	around to identify concurrent DDL till server provides MDL lock
	for blocking DDL. */
	if (strncmp(name_ptr, TEMP_FILE_PREFIX,
		    TEMP_FILE_PREFIX_LENGTH) == 0) {

		return(true);
	}

	return(false);
}

/** Add file to snapshot
@param[in]	name		file name
@param[in]	size_bytes	file size in bytes
@param[in]	space_id	tablespace id
@param[in]	copy_name	copy the file name or use reference
@return error code. */
dberr_t
Clone_Snapshot::add_file(
	const char*	name,
	ib_uint64_t	size_bytes,
	ulint		space_id,
	bool		copy_name)
{
	ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

	Clone_File_Meta*	file_meta;
	uint		num_chunks;

	/* Build file metadata entry and add to data file vector. */
	file_meta = build_file(name, size_bytes, 0, num_chunks, copy_name);

	if (file_meta == nullptr) {

		return(DB_OUT_OF_MEMORY);
	}

	file_meta->m_space_id = space_id;

	file_meta->m_file_index = m_num_data_files;

	m_data_file_vector.push_back(file_meta);

	++m_num_data_files;

	ut_ad(m_data_file_vector.size() == m_num_data_files);

	/* Update total number of chunks. */
	m_num_data_chunks += num_chunks;
	m_num_current_chunks = m_num_data_chunks;

	/* Update maximum file name length in snapshot. */
	if (file_meta->m_file_name_len > m_max_file_name_len) {

		m_max_file_name_len = file_meta->m_file_name_len;
	}

	return(DB_SUCCESS);
}

/** Extract file information from node and add to snapshot
@param[in]	node	file node
@return error code */
dberr_t
Clone_Snapshot::add_node(
	fil_node_t*	node)
{
	ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

	ib_uint64_t	size_bytes;
	fil_space_t*	space;
	dberr_t		err;

	space = node->space;

	/* Exit if concurrent DDL in progress. */
	if (is_ddl_temp_table(node)) {

		my_error(ER_DDL_IN_PROGRESS, MYF(0));
		return(DB_ERROR);
	}

	/* Currently don't support encrypted tablespace. */
	if (space->encryption_type != Encryption::NONE) {

		my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Clone Encrypted Tablespace");
		return(DB_ERROR);
	}

	/* Find out the file size from node. */
	page_size_t	page_sz(space->flags);

	/* For compressed pages the file size doesn't match
	physical page size multiplied by number of pages. It is
	because we use UNIV_PAGE_SIZE while creating the node
	and tablespace. */
	if (node->is_open && !page_sz.is_compressed()) {

		size_bytes = static_cast<ib_uint64_t>(node->size);
		size_bytes *= page_sz.physical();
	} else {

		os_file_size_t	file_size;

		file_size = os_file_get_size(node->name);
		size_bytes = file_size.m_total_size;
	}

#ifdef HAVE_PSI_STAGE_INTERFACE
	m_monitor.add_estimate(size_bytes);
#endif

	/* Add file to snapshot. */
	err = add_file(node->name, size_bytes, space->id, false);

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Add to hash map only for first node of the tablesapce. */
	if (m_data_file_map[space->id] == 0) {

		m_data_file_map[space->id] = m_num_data_files;
	}

	return(DB_SUCCESS);
}

/** Add page ID to to the set of pages in snapshot
@param[in]	space_id	page tablespace
@param[in]	page_num	page number within tablespace
@return error code */
dberr_t
Clone_Snapshot::add_page(
	ib_uint32_t	space_id,
	ib_uint32_t	page_num)
{
	Clone_Page	cur_page;

	cur_page.m_space_id = space_id;
	cur_page.m_page_no = page_num;

	auto result = m_page_set.insert(cur_page);

	if (result.second) {

		m_num_pages++;
#ifdef HAVE_PSI_STAGE_INTERFACE
		m_monitor.add_estimate(UNIV_PAGE_SIZE);
#endif
	} else {

		m_num_duplicate_pages++;
	}

	return(DB_SUCCESS);
}

/** Add redo file to snapshot
@param[in]	file_name	file name
@param[in]	file_size	file size in bytes
@param[in]	file_offset	start offset
@return error code. */
dberr_t
Clone_Snapshot::add_redo_file(
	char*		file_name,
	ib_uint64_t	file_size,
	ib_uint64_t	file_offset)
{
	ut_ad(m_snapshot_handle_type == CLONE_HDL_COPY);

	Clone_File_Meta*	file_meta;
	uint			num_chunks;

	file_offset = ut_uint64_align_down(file_offset, UNIV_PAGE_SIZE);

	/* Build redo file metadata and add to redo vector. */
	file_meta = build_file(file_name, file_size, file_offset, num_chunks, true);

#ifdef HAVE_PSI_STAGE_INTERFACE
	m_monitor.add_estimate(file_meta->m_file_size);
#endif

	if (file_meta == nullptr) {

		return(DB_OUT_OF_MEMORY);
	}

	/* Set the start offset for first redo file. This could happen
	if redo archiving was already in progress, possibly by another
	concurrent snapshot. */
	if (m_num_redo_files == 0) {

		m_redo_start_offset = file_offset;
	} else {

		ut_ad(file_offset == 0);
	}

	file_meta->m_space_id = dict_sys_t::s_log_space_first_id;

	file_meta->m_file_index = m_num_redo_files;

	m_redo_file_vector.push_back(file_meta);
	++m_num_redo_files;

	ut_ad(m_redo_file_vector.size() == m_num_redo_files);

	m_num_redo_chunks += num_chunks;
	m_num_current_chunks = m_num_redo_chunks;

	return(DB_SUCCESS);
}

/** Send current task information via callback
@param[in]	task		task that is sending the information
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::send_task_metadata(
	Clone_Task*	task,
	Ha_clone_cbk*	callback)
{
	int			err = 0;
	mem_heap_t*		heap;

	Clone_Desc_Task_Meta	task_desc;
	uint			desc_len;

	ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

	heap = m_clone_task_manager.get_heap();

	/* Build task descriptor with metadata */
	task_desc.init_header(get_version());
	task_desc.m_task_meta = task->m_task_meta;

	desc_len = task->m_alloc_len;
	task_desc.serialize(task->m_serial_desc, desc_len, heap);

	callback->set_data_desc(task->m_serial_desc, desc_len);
	callback->clear_flags();
	callback->set_ack();

	err = callback->buffer_cbk(nullptr, 0);

	return(err == 0 ? DB_SUCCESS : DB_ERROR);
}

/** Send current state information via callback
@param[in]	task		task that is sending the information
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::send_state_metadata(
	Clone_Task*	task,
	Ha_clone_cbk*	callback)
{
	int		err = 0;
	Clone_Desc_State	state_desc;
	Clone_Snapshot*	snapshot;

	ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

	state_desc.init_header(get_version());

	/* Build state descriptor from snapshot and task */
	snapshot = m_clone_task_manager.get_snapshot();
	snapshot->get_state_info(&state_desc);

	Clone_Task_Meta*	task_meta;
	task_meta = &task->m_task_meta;
	state_desc.m_task_index = task_meta->m_task_index;

	mem_heap_t*	heap;
	uint		desc_len;

	heap = m_clone_task_manager.get_heap();
	desc_len = task->m_alloc_len;
	state_desc.serialize(task->m_serial_desc, desc_len, heap);

	callback->set_data_desc(task->m_serial_desc, desc_len);
	callback->clear_flags();
	callback->set_ack();

	err = callback->buffer_cbk(nullptr, 0);

	return(err == 0 ? DB_SUCCESS : DB_ERROR);
}

/** Send current file information via callback
@param[in]	task		task that is sending the information
@param[in]	file_meta	file meta information
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::send_file_metadata(
	Clone_Task*		task,
	Clone_File_Meta*	file_meta,
	Ha_clone_cbk*		callback)
{
	int	err = 0;

	Clone_Desc_File_MetaData	file_desc;
	Clone_Snapshot*			snapshot;

	ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

	snapshot = m_clone_task_manager.get_snapshot();

	file_desc.m_file_meta = *file_meta;
	file_desc.m_state = snapshot->get_state();

	if (file_desc.m_state == CLONE_SNAPSHOT_REDO_COPY) {

		/* For Redo log always send the fixed redo file size. */
		file_desc.m_file_meta.m_file_size
			= snapshot->get_redo_file_size();

		file_desc.m_file_meta.m_file_name = nullptr;
		file_desc.m_file_meta.m_file_name_len = 0;

	} else if (file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {

		/* Server buffer dump file ib_buffer_pool. */
		ut_ad(file_desc.m_state == CLONE_SNAPSHOT_FILE_COPY);
		ut_ad(file_meta->m_file_index == 0);

		file_desc.m_file_meta.m_file_name =
			SRV_BUF_DUMP_FILENAME_DEFAULT;

		file_desc.m_file_meta.m_file_name_len
			= strlen(SRV_BUF_DUMP_FILENAME_DEFAULT) + 1;

	} else if (!fsp_is_ibd_tablespace(
			static_cast<space_id_t>(file_meta->m_space_id))
		   && Fil_path::is_absolute_path(file_meta->m_file_name)) {

		/* For system tablespace, remove absolute path. */
		ut_ad(file_desc.m_state == CLONE_SNAPSHOT_FILE_COPY);

		const char*	name_ptr;

		name_ptr = strrchr(file_meta->m_file_name, OS_PATH_SEPARATOR);
		name_ptr++;
		ut_a(name_ptr != nullptr);

		file_desc.m_file_meta.m_file_name = name_ptr;
		file_desc.m_file_meta.m_file_name_len = strlen(name_ptr) + 1;
	}

	file_desc.init_header(get_version());

	mem_heap_t*	heap;
	uint		desc_len;

	heap = m_clone_task_manager.get_heap();
	desc_len = task->m_alloc_len;
	file_desc.serialize(task->m_serial_desc, desc_len, heap);

	callback->set_data_desc(task->m_serial_desc, desc_len);
	callback->clear_flags();

	err = callback->buffer_cbk(nullptr, 0);

	return(err == 0 ? DB_SUCCESS : DB_ERROR);
}

/** Send cloned data via callback
@param[in]	task		task that is sending the information
@param[in]	file_meta	file information
@param[in]	offset		file offset
@param[in]	buffer		data buffer or NULL if send from file
@param[in]	size		data buffer size
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::send_data(
	Clone_Task*		task,
	Clone_File_Meta*	file_meta,
	ib_uint64_t		offset,
	byte*			buffer,
	uint			size,
	Ha_clone_cbk*		callback)
{
	dberr_t		err;
	Clone_Desc_Data	data_desc;
	Clone_Snapshot*	snapshot;

	ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

	snapshot = m_clone_task_manager.get_snapshot();

	/* Build data descriptor */
	data_desc.init_header(get_version());
	data_desc.m_state = snapshot->get_state();

	data_desc.m_task_meta = task->m_task_meta;

	data_desc.m_file_index = file_meta->m_file_index;
	data_desc.m_data_len = size;
	data_desc.m_file_offset = offset;
	data_desc.m_file_size = file_meta->m_file_size;

	/* Serialize data descriptor and set in callback */
	mem_heap_t*	heap;
	uint		desc_len;
	ulint		file_type;

	heap = snapshot->get_heap();
	desc_len = task->m_alloc_len;
	data_desc.serialize(task->m_serial_desc, desc_len, heap);

	callback->set_data_desc(task->m_serial_desc, desc_len);
	callback->clear_flags();

	if (data_desc.m_state == CLONE_SNAPSHOT_REDO_COPY
	    || file_meta->m_space_id == dict_sys_t::s_invalid_space_id) {

		file_type = OS_CLONE_LOG_FILE;
	} else {

		file_type = OS_CLONE_DATA_FILE;
	}

	if (buffer != nullptr) {

		/* Send data from buffer. */
		int	int_err;
		int_err = callback->buffer_cbk(buffer, size);

#ifdef HAVE_PSI_STAGE_INTERFACE
		/* Update PFS if success. */
		if (!int_err) {
			Clone_Monitor &monitor = snapshot->get_clone_monitor();
			monitor.update_work(size);
		}
#endif

		return(int_err == 0 ? DB_SUCCESS : DB_ERROR);

	} else {

		/* Send data from file. */
		if (task->m_current_file_des.m_file == OS_FILE_CLOSED) {

			err = open_file(task, file_meta, file_type,
					   false, false);

			if (err != DB_SUCCESS) {

				return(err);
			}
		}

		os_file_t	file_hdl;
		bool		success;
		char		errbuf[MYSYS_STRERROR_SIZE];

		file_hdl = task->m_current_file_des.m_file;
		success = os_file_seek(nullptr, file_hdl, offset);

		if (!success) {

			my_error(ER_ERROR_ON_READ, MYF(0),
				 file_meta->m_file_name, errno,
				 my_strerror(errbuf, sizeof(errbuf), errno));
			return(DB_ERROR);
		}

		if (task->m_file_cache) {

			callback->set_os_buffer_cache();
		}

		callback->set_source_name(file_meta->m_file_name);

		err = file_callback(callback, task, size
#ifdef UNIV_PFS_IO
				    , __FILE__,  __LINE__
#endif  /* UNIV_PFS_IO */
				    );

#ifdef HAVE_PSI_STAGE_INTERFACE
		/* Update PFS if success. */
		if (err == DB_SUCCESS) {
			Clone_Monitor &monitor = snapshot->get_clone_monitor();
			monitor.update_work(size);
		}
#endif

		return(err);
	}
}

/** Transfer snapshot data via callback
@param[in]	callback	user callback interface
@return error code */
dberr_t
Clone_Handle::copy(
	Ha_clone_cbk*	callback)
{
	dberr_t		err = DB_SUCCESS;
	Clone_Task*	task;
	uint		current_chunk;
	Clone_Snapshot*	snapshot;
	uint		max_chunks;
	uint		percent_done;
	ulint		disp_time;

	ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

	/* Get a free task from task manager. */
	err = m_clone_task_manager.get_task(task);

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Send the task metadata. */
	err = send_task_metadata(task, callback);

	if (err != DB_SUCCESS) {

		return(err);
	}

	/* Adjust block size based on client buffer size. */
	snapshot = m_clone_task_manager.get_snapshot();
	snapshot->update_block_size(callback->get_client_buffer_size());

	max_chunks = snapshot->get_num_chunks();

	/* Set time values for tracking stage progress. */
	percent_done = 0;
	disp_time = ut_time_ms();

	/* Loop and process data until snapshot is moved to DONE state. */
	while (m_clone_task_manager.get_state() != CLONE_SNAPSHOT_DONE) {

		/* Reserve next chunk for current state from snapshot. */
		current_chunk = m_clone_task_manager.reserve_next_chunk();

		if (current_chunk != 0) {

			/* Send blocks from the reserved chunk. */
			err = process_chunk(task, current_chunk, callback);

			/* Display stage progress based on % completion. */
			uint	current_percent;
			ulint	current_time;

			current_time = ut_time_ms();
			current_percent = (current_chunk * 100) / max_chunks;

			if (current_percent >= percent_done + 20
			    || (current_time - disp_time > 5000
				&& current_percent > percent_done)) {

				percent_done = current_percent;
				disp_time = current_time;

				ib::info() << "Stage progress: "
					   << percent_done << "% completed.";
			}

		} else {
			/* No more chunks in current state. Transit to next state. */

			/* Close the last open file before proceeding to next state */
			err = close_file(task);

			if (err != DB_SUCCESS) {

				break;
			}

			/* Next state is decided by snapshot for Copy. */
			err = move_to_next_state(task, CLONE_SNAPSHOT_NONE);

			if (err != DB_SUCCESS) {

				break;
			}

			max_chunks = snapshot->get_num_chunks();
			percent_done = 0;
			disp_time = ut_time_ms();

			/* Send state metadata before processing chunks. */
			err = send_state_metadata(task, callback);
		}

		if (err != DB_SUCCESS) {

			break;
		}
	}

	return(err);
}

#ifdef UNIV_DEBUG
/** Wait during clone operation
@param[in]	snapshot	task that is sending the information
@param[in]	chunk_num	chunk number to process */
static void
debug_wait(
	Clone_Snapshot*	snapshot,
	uint		chunk_num)
{
	Snapshot_State	state;
	uint		nchunks;

	state = snapshot->get_state();
	nchunks = snapshot->get_num_chunks();

	/* Stop somewhere in the middle of current stage */
	if (chunk_num != (nchunks / 2 + 1)) {

		return;
	}

	if (state == CLONE_SNAPSHOT_FILE_COPY) {
		DEBUG_SYNC_C("clone_file_copy");

	} else if (state == CLONE_SNAPSHOT_PAGE_COPY) {
		DEBUG_SYNC_C("clone_page_copy");

	} else if (state == CLONE_SNAPSHOT_REDO_COPY) {
		DEBUG_SYNC_C("clone_redo_copy");
	}
}
#endif /* UNIV_DEBUG */

/** Process a data chunk and send data blocks via callback
@param[in]	task		task that is sending the information
@param[in]	chunk_num	chunk number to process
@param[in]	callback	callback interface
@return error code */
dberr_t
Clone_Handle::process_chunk(
	Clone_Task*	task,
	uint		chunk_num,
	Ha_clone_cbk*	callback)
{
	dberr_t		err = DB_SUCCESS;
	uint		block_num = 0;

	byte*		data_buf;
	uint		data_size;
	ib_uint64_t	data_offset;

	Clone_Snapshot*	snapshot;
	Clone_File_Meta	file_meta;

	ut_ad(m_clone_handle_type == CLONE_HDL_COPY);

	file_meta.m_file_index = task->m_current_file_index;

	snapshot = m_clone_task_manager.get_snapshot();

#ifdef UNIV_DEBUG
	debug_wait(snapshot, chunk_num);
#endif /* UNIV_DEBUG */

	/* Loop over all the blocks of current chunk and send data. */
	while (err == DB_SUCCESS) {

		data_buf = task->m_current_buffer;
		data_size = task->m_buffer_alloc_len;

		/* Get next block from snapshot */
		err = snapshot->get_next_block(chunk_num, block_num,
			&file_meta, data_offset, data_buf, data_size);

		/* '0' block number indicates no more blocks. */
		if (err != DB_SUCCESS || block_num == 0) {

			break;
		}

		/* Need to exit if DDL has marked for abort. */
		if (Clone_Sys::s_clone_sys_state == CLONE_SYS_ABORT) {

			my_error(ER_DDL_IN_PROGRESS, MYF(0));

			err = DB_ERROR;
			break;
		}

		task->m_task_meta.m_block_num = block_num;
		task->m_task_meta.m_chunk_num = chunk_num;

		if (data_buf == nullptr
		    && (task->m_current_file_des.m_file == OS_FILE_CLOSED
			|| task->m_current_file_index
			   != file_meta.m_file_index)) {

			/* We are moving to next file. Close the current file and
			send metadata for the next file. */

			err = close_file(task);

			if (err != DB_SUCCESS) {

				break;
			}

			err = send_file_metadata(task, &file_meta, callback);

			if (err != DB_SUCCESS) {

				break;
			}
		}

		if (data_size == 0) {

			continue;
		}

		err = send_data(task, &file_meta, data_offset,
				data_buf, data_size, callback);
	}

	return(err);
}
