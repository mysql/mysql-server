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
@file clone/clone0desc.cc
Innodb clone descriptors

*******************************************************/

#include "clone0desc.h"
#include "mach0data.h"

/** Maximum supported descriptor version. The version represents the current
set of descriptors and its elements. */
static const uint CLONE_DESC_MAX_VERSION = 100;

/** Header: Version is in first 4 bytes */
static const uint CLONE_DESC_VER_OFFSET = 0;

/** Header: Total length is stored in next 4 bytes */
static const uint CLONE_DESC_LEN_OFFSET =  CLONE_DESC_VER_OFFSET + 4;

/** Header: Descriptor type is in next 4 bytes */
static const uint CLONE_DESC_TYPE_OFFSET = CLONE_DESC_LEN_OFFSET + 4;

/** Header: Fixed length. Keep 4 bytes extra for any addition in future. */
static const uint CLONE_DESC_HEADER_LEN = CLONE_DESC_TYPE_OFFSET + 4 + 4;

/** Choose lowest descriptor version between reference locator
and currently supported version.
@param[in]	ref_loc	reference locator
@return chosen version */
uint
choose_desc_version(
	byte*	ref_loc)
{
	if (ref_loc == nullptr) {

		return(CLONE_DESC_MAX_VERSION);
	}

	Clone_Desc_Header header;
	uint		version;

	header.deserialize(ref_loc);
	version = header.m_version;

	/* Choose the minimum of remote locator version local
	supported version. */
	if (version > CLONE_DESC_MAX_VERSION) {

		version = CLONE_DESC_MAX_VERSION;
	}

	return(version);
}

/** Serialize the descriptor header: Caller must allocate
the serialized buffer.
@param[out]	desc_hdr	serialized header */
void
Clone_Desc_Header::serialize(
	byte*	desc_hdr)
{
	mach_write_to_4(desc_hdr + CLONE_DESC_VER_OFFSET, m_version);
	mach_write_to_4(desc_hdr + CLONE_DESC_LEN_OFFSET, m_length);
	mach_write_to_4(desc_hdr + CLONE_DESC_TYPE_OFFSET, m_type);
}

/** Deserialize the descriptor header.
@param[in]	desc_hdr	serialized header */
void
Clone_Desc_Header::deserialize(
	byte*	desc_hdr)
{
	m_version = mach_read_from_4(desc_hdr + CLONE_DESC_VER_OFFSET);
	m_length = mach_read_from_4(desc_hdr + CLONE_DESC_LEN_OFFSET);

	uint	int_type;
	int_type = mach_read_from_4(desc_hdr + CLONE_DESC_TYPE_OFFSET);
	ut_ad(int_type < CLONE_DESC_MAX);

	m_type = static_cast<Clone_Desc_Type>(int_type);
}

/** Locator: Clone identifier in 8 bytes */
static const uint CLONE_LOC_CID_OFFSET = CLONE_DESC_HEADER_LEN;

/** Locator: Snapshot identifier in 8 bytes */
static const uint CLONE_LOC_SID_OFFSET = CLONE_LOC_CID_OFFSET + 8;

/** Locator: Clone array index in 4 bytes */
static const uint CLONE_LOC_IDX_OFFSET = CLONE_LOC_SID_OFFSET + 8;

/** Locator: Total length */
static const uint CLONE_DESC_LOC_LEN = CLONE_LOC_IDX_OFFSET + 4;

/** Initialize clone locator.
@param[in]	id	Clone identifier
@param[in]	snap_id	Snapshot identifier
@param[in]	version	Descriptor version
@param[in]	index	clone index */
void
Clone_Desc_Locator::init(
	ib_uint64_t	id,
	ib_uint64_t	snap_id,
	uint		version,
	uint		index)
{
	m_header.m_version = version;

	m_header.m_length = CLONE_DESC_LOC_LEN;
	m_header.m_type = CLONE_DESC_LOCATOR;

	m_clone_id = id;
	m_snapshot_id = snap_id;

	m_clone_index = index;
}

/** Check if the passed locator matches the current one.
@param[in]	other_desc	input locator descriptor
@return true if matches */
bool
Clone_Desc_Locator::match(
	Clone_Desc_Locator*	other_desc)
{
#ifdef UNIV_DEBUG
	Clone_Desc_Header*	other_header = &other_desc->m_header;
#endif /* UNIV_DEBUG */

	if (other_desc->m_clone_id == m_clone_id
	    && other_desc->m_snapshot_id == m_snapshot_id) {

		ut_ad(m_header.m_version == other_header->m_version);
		return true;
	}

	return false;
}

/** Serialize the descriptor. Caller should pass
the length if allocated.
@param[out]	desc_loc	serialized descriptor
@param[in,out]	len		length of serialized descriptor
@param[in]	heap		heap for allocating memory */
void
Clone_Desc_Locator::serialize(
	byte*&		desc_loc,
	uint&		len,
	mem_heap_t*	heap)
{
	if (desc_loc == nullptr) {

		len = m_header.m_length;
		desc_loc = static_cast<byte*>(mem_heap_alloc(heap, len));
	} else {

		ut_ad(len >= m_header.m_length);
		len = m_header.m_length;
	}

	m_header.serialize(desc_loc);

	mach_write_to_8(desc_loc + CLONE_LOC_CID_OFFSET, m_clone_id);
	mach_write_to_8(desc_loc + CLONE_LOC_SID_OFFSET, m_snapshot_id);

	mach_write_to_4(desc_loc + CLONE_LOC_IDX_OFFSET, m_clone_index);
}

/** Deserialize the descriptor.
@param[in]	desc_loc	serialized locator */
void
Clone_Desc_Locator::deserialize(
	byte*	desc_loc)
{
	m_header.deserialize(desc_loc);

	ut_ad(m_header.m_type == CLONE_DESC_LOCATOR);

	m_clone_id = mach_read_from_8(desc_loc + CLONE_LOC_CID_OFFSET);
	m_snapshot_id = mach_read_from_8(desc_loc + CLONE_LOC_SID_OFFSET);

	m_clone_index = mach_read_from_4(desc_loc + CLONE_LOC_IDX_OFFSET);
}

/** Task: Clone task index in 4 bytes */
static const uint CLONE_TASK_INDEX_OFFSET = CLONE_DESC_HEADER_LEN;

/** Task: Task chunk number in 4 bytes */
static const uint CLONE_TASK_CHUNK_OFFSET = CLONE_TASK_INDEX_OFFSET + 4;

/** Task: Task block number in 4 bytes */
static const uint CLONE_TASK_BLOCK_OFFSET = CLONE_TASK_CHUNK_OFFSET + 4;

/** Task: Total length */
static const uint CLONE_TASK_META_LEN = CLONE_TASK_BLOCK_OFFSET + 4;

/** Initialize header
@param[in]	version	descriptor version */
void
Clone_Desc_Task_Meta::init_header(
	uint		version)
{
	m_header.m_version = version;

	m_header.m_length = CLONE_TASK_META_LEN;

	m_header.m_type = CLONE_DESC_TASK_METADATA;
}

/** Serialize the descriptor. Caller should pass
the length if allocated.
	@param[out]	desc_task	serialized descriptor
	@param[in,out]	len		length of serialized descriptor
	@param[in]	heap		heap for allocating memory */
void
Clone_Desc_Task_Meta::serialize(
	byte*&		desc_task,
	uint&		len,
	mem_heap_t*	heap)
{
	if (desc_task == nullptr) {

		len = m_header.m_length;
		desc_task = static_cast<byte*>(mem_heap_alloc(heap, len));
	} else {

		ut_ad(len >= m_header.m_length);
		len = m_header.m_length;
	}

	m_header.serialize(desc_task);

	mach_write_to_4(desc_task + CLONE_TASK_INDEX_OFFSET,
			m_task_meta.m_task_index);
	mach_write_to_4(desc_task + CLONE_TASK_CHUNK_OFFSET,
			m_task_meta.m_chunk_num);
	mach_write_to_4(desc_task + CLONE_TASK_BLOCK_OFFSET,
			m_task_meta.m_block_num);
}

/** Deserialize the descriptor.
@param[in]	desc_task	serialized descriptor */
void
Clone_Desc_Task_Meta::deserialize(
	byte*		desc_task)
{
	m_header.deserialize(desc_task);

	ut_ad(m_header.m_type == CLONE_DESC_TASK_METADATA);

	m_task_meta.m_task_index = mach_read_from_4(
		desc_task + CLONE_TASK_INDEX_OFFSET);
	m_task_meta.m_chunk_num = mach_read_from_4(
		desc_task + CLONE_TASK_CHUNK_OFFSET);
	m_task_meta.m_block_num = mach_read_from_4(
		desc_task + CLONE_TASK_BLOCK_OFFSET);
}

/** File Metadata: Snapshot state in 4 bytes */
static const uint CLONE_FILE_STATE_OFFSET = CLONE_DESC_HEADER_LEN;

/** File Metadata: File size in 8 bytes */
static const uint CLONE_FILE_SIZE_OFFSET = CLONE_FILE_STATE_OFFSET + 4;

/** File Metadata: Tablespace ID in 4 bytes */
static const uint CLONE_FILE_SPACE_ID_OFFSET = CLONE_FILE_SIZE_OFFSET + 8;

/** File Metadata: File index in 4 bytes */
static const uint CLONE_FILE_IDX_OFFSET = CLONE_FILE_SPACE_ID_OFFSET + 4;

/** File Metadata: First chunk number in 4 bytes */
static const uint CLONE_FILE_BCHUNK_OFFSET = CLONE_FILE_IDX_OFFSET + 4;

/** File Metadata: Last chunk number in 4 bytes */
static const uint CLONE_FILE_ECHUNK_OFFSET = CLONE_FILE_BCHUNK_OFFSET + 4;

/** File Metadata: File name length in 4 bytes */
static const uint CLONE_FILE_FNAMEL_OFFSET = CLONE_FILE_ECHUNK_OFFSET + 4;

/** File Metadata: File name */
static const uint CLONE_FILE_FNAME_OFFSET = CLONE_FILE_FNAMEL_OFFSET + 4;

/** File Metadata: Length excluding the file name */
static const uint CLONE_FILE_BASE_LEN = CLONE_FILE_FNAME_OFFSET;

/** Initialize header
@param[in]	version	descriptor version */
void
Clone_Desc_File_MetaData::init_header(
	uint		version)
{
	m_header.m_version = version;

	m_header.m_length = CLONE_FILE_BASE_LEN;
	m_header.m_length  += static_cast<uint>(m_file_meta.m_file_name_len);

	m_header.m_type = CLONE_DESC_FILE_METADATA;
}

/** Serialize the descriptor. Caller should pass
the length if allocated.
@param[out]	desc_file	serialized descriptor
@param[in,out]	len		length of serialized descriptor
@param[in]	heap		heap for allocating memory */
void
Clone_Desc_File_MetaData::serialize(
	byte*&		desc_file,
	uint&		len,
	mem_heap_t*	heap)
{
	/* Allocate descriptor if needed. */
	if (desc_file == nullptr) {

		len = m_header.m_length;
		ut_ad(len == CLONE_FILE_FNAME_OFFSET
			     + m_file_meta.m_file_name_len);

		desc_file = static_cast<byte*>(mem_heap_alloc(heap, len));
	} else {

		ut_ad(len >= m_header.m_length);
		len = m_header.m_length;
	}

	m_header.serialize(desc_file);

	mach_write_to_4(desc_file + CLONE_FILE_STATE_OFFSET, m_state);

	mach_write_to_8(desc_file + CLONE_FILE_SIZE_OFFSET,
			m_file_meta.m_file_size);
	mach_write_to_4(desc_file + CLONE_FILE_SPACE_ID_OFFSET,
			m_file_meta.m_space_id);
	mach_write_to_4(desc_file + CLONE_FILE_IDX_OFFSET,
			m_file_meta.m_file_index);

	mach_write_to_4(desc_file + CLONE_FILE_BCHUNK_OFFSET,
			m_file_meta.m_begin_chunk);
	mach_write_to_4(desc_file + CLONE_FILE_ECHUNK_OFFSET,
			m_file_meta.m_end_chunk);

	mach_write_to_4(desc_file + CLONE_FILE_FNAMEL_OFFSET,
			m_file_meta.m_file_name_len);

	/* Copy variable length file name. */
	if (m_file_meta.m_file_name_len != 0) {
		memcpy(static_cast<void*>(desc_file + CLONE_FILE_FNAME_OFFSET),
		       static_cast<const void*>(m_file_meta.m_file_name),
		       m_file_meta.m_file_name_len);
	}
}

/** Deserialize the descriptor.
@param[in]	desc_file	serialized descriptor */
void
Clone_Desc_File_MetaData::deserialize(
	byte*		desc_file)
{
	m_header.deserialize(desc_file);
	ut_ad(m_header.m_type == CLONE_DESC_FILE_METADATA);

	uint	int_type;
	int_type = mach_read_from_4(desc_file + CLONE_FILE_STATE_OFFSET);

	m_state = static_cast<Snapshot_State>(int_type);

	m_file_meta.m_file_size = mach_read_from_8(
		desc_file + CLONE_FILE_SIZE_OFFSET);
	m_file_meta.m_space_id = mach_read_from_4(
		desc_file + CLONE_FILE_SPACE_ID_OFFSET);
	m_file_meta.m_file_index = mach_read_from_4(
		desc_file + CLONE_FILE_IDX_OFFSET);

	m_file_meta.m_begin_chunk = mach_read_from_4(
		desc_file + CLONE_FILE_BCHUNK_OFFSET);
	m_file_meta.m_end_chunk = mach_read_from_4(
		desc_file + CLONE_FILE_ECHUNK_OFFSET);

	m_file_meta.m_file_name_len = mach_read_from_4(
		desc_file + CLONE_FILE_FNAMEL_OFFSET);

	ut_ad(m_header.m_length
		== CLONE_FILE_FNAME_OFFSET + m_file_meta.m_file_name_len);

	if (m_file_meta.m_file_name_len == 0) {

		m_file_meta.m_file_name = nullptr;
	} else {

		m_file_meta.m_file_name = reinterpret_cast<const char*>(
			desc_file + CLONE_FILE_FNAME_OFFSET);
	}
}

/** Clone State: Snapshot state in 4 bytes */
static const uint CLONE_DESC_STATE_OFFSET = CLONE_DESC_HEADER_LEN;

/** Clone State: Task index in 4 bytes */
static const uint CLONE_DESC_TASK_OFFSET = CLONE_DESC_STATE_OFFSET + 4;

/** Clone State: Number of chunks in 4 bytes */
static const uint CLONE_DESC_STATE_NUM_CHUNKS = CLONE_DESC_TASK_OFFSET + 4;

/** Clone State: Number of files in 4 bytes */
static const uint CLONE_DESC_STATE_NUM_FILES = CLONE_DESC_STATE_NUM_CHUNKS + 4;

/** Clone State: Total length */
static const uint CLONE_DESC_STATE_LEN = CLONE_DESC_STATE_NUM_FILES + 4;

/** Initialize header
@param[in]	version	descriptor version */
void
Clone_Desc_State::init_header(
	uint		version)
{
	m_header.m_version = version;

	m_header.m_length = CLONE_DESC_STATE_LEN;

	m_header.m_type = CLONE_DESC_STATE;
}

/** Serialize the descriptor. Caller should pass
the length if allocated.
@param[out]	desc_state	serialized descriptor
@param[in,out]	len		length of serialized descriptor
@param[in]	heap		heap for allocating memory */
void
Clone_Desc_State::serialize(
	byte*&		desc_state,
	uint&		len,
	mem_heap_t*	heap)
{
	/* Allocate descriptor if needed. */
	if (desc_state == nullptr) {

		len = m_header.m_length;
		desc_state = static_cast<byte*>(mem_heap_alloc(heap, len));
	} else {

		ut_ad(len >= m_header.m_length);
		len = m_header.m_length;
	}

	m_header.serialize(desc_state);

	mach_write_to_4(desc_state + CLONE_DESC_STATE_OFFSET, m_state);
	mach_write_to_4(desc_state + CLONE_DESC_TASK_OFFSET, m_task_index);

	mach_write_to_4(desc_state + CLONE_DESC_STATE_NUM_CHUNKS, m_num_chunks);
	mach_write_to_4(desc_state + CLONE_DESC_STATE_NUM_FILES, m_num_files);
}

/** Deserialize the descriptor.
@param[in]	desc_state	serialized descriptor */
void
Clone_Desc_State::deserialize(
	byte*		desc_state)
{
	m_header.deserialize(desc_state);
	ut_ad(m_header.m_type == CLONE_DESC_STATE);

	uint	int_type;
	int_type = mach_read_from_4(desc_state + CLONE_DESC_STATE_OFFSET);

	m_state = static_cast<Snapshot_State>(int_type);

	m_task_index = mach_read_from_4(desc_state + CLONE_DESC_TASK_OFFSET);

	m_num_chunks = mach_read_from_4(desc_state + CLONE_DESC_STATE_NUM_CHUNKS);
	m_num_files = mach_read_from_4(desc_state + CLONE_DESC_STATE_NUM_FILES);
}

/** Clone Data: Snapshot state in 4 bytes */
static const uint CLONE_DATA_STATE_OFFSET = CLONE_DESC_HEADER_LEN;

/** Clone Data: Task index in 4 bytes */
static const uint CLONE_DATA_TASK_INDEX_OFFSET = CLONE_DATA_STATE_OFFSET + 4;

/** Clone Data: Current chunk number in 4 bytes */
static const uint CLONE_DATA_TASK_CHUNK_OFFSET = CLONE_DATA_TASK_INDEX_OFFSET + 4;

/** Clone Data: Current block number in 4 bytes */
static const uint CLONE_DATA_TASK_BLOCK_OFFSET = CLONE_DATA_TASK_CHUNK_OFFSET + 4;

/** Clone Data: Data file index in 4 bytes */
static const uint CLONE_DATA_FILE_IDX_OFFSET = CLONE_DATA_TASK_BLOCK_OFFSET + 4;

/** Clone Data: Data length in 4 bytes */
static const uint CLONE_DATA_LEN_OFFSET = CLONE_DATA_FILE_IDX_OFFSET + 4;

/** Clone Data: Data file offset in 8 bytes */
static const uint CLONE_DATA_FOFF_OFFSET = CLONE_DATA_LEN_OFFSET + 4;

/** Clone Data: Updated file size in 8 bytes */
static const uint CLONE_DATA_FILE_SIZE_OFFSET = CLONE_DATA_FOFF_OFFSET + 8;

/** Clone Data: Total length */
static const uint CLONE_DESC_DATA_LEN = CLONE_DATA_FILE_SIZE_OFFSET + 8;

/** Initialize header
@param[in]	version	descriptor version */
void
Clone_Desc_Data::init_header(
	uint		version)
{
	m_header.m_version = version;

	m_header.m_length = CLONE_DESC_DATA_LEN;

	m_header.m_type = CLONE_DESC_DATA;
}

/** Serialize the descriptor. Caller should pass
the length if allocated.
@param[out]	desc_data	serialized descriptor
@param[in,out]	len		length of serialized descriptor
@param[in]	heap		heap for allocating memory */
void
Clone_Desc_Data::serialize(
	byte*&		desc_data,
	uint&		len,
	mem_heap_t*	heap)
{
	/* Allocate descriptor if needed. */
	if (desc_data == nullptr) {

		len = m_header.m_length;
		desc_data = static_cast<byte*>(mem_heap_alloc(heap, len));
	} else {

		ut_ad(len >= m_header.m_length);
		len = m_header.m_length;
	}

	m_header.serialize(desc_data);

	mach_write_to_4(desc_data + CLONE_DATA_STATE_OFFSET, m_state);

	mach_write_to_4(desc_data + CLONE_DATA_TASK_INDEX_OFFSET,
			m_task_meta.m_task_index);
	mach_write_to_4(desc_data + CLONE_DATA_TASK_CHUNK_OFFSET,
			m_task_meta.m_chunk_num);
	mach_write_to_4(desc_data + CLONE_DATA_TASK_BLOCK_OFFSET,
			m_task_meta.m_block_num);

	mach_write_to_4(desc_data + CLONE_DATA_FILE_IDX_OFFSET, m_file_index);
	mach_write_to_4(desc_data + CLONE_DATA_LEN_OFFSET, m_data_len);
	mach_write_to_8(desc_data + CLONE_DATA_FOFF_OFFSET, m_file_offset);
	mach_write_to_8(desc_data + CLONE_DATA_FILE_SIZE_OFFSET, m_file_size);
}

/** Deserialize the descriptor.
@param[in]	desc_data	serialized descriptor */
void
Clone_Desc_Data::deserialize(
	byte*		desc_data)
{
	m_header.deserialize(desc_data);
	ut_ad(m_header.m_type == CLONE_DESC_DATA);

	uint	int_type;
	int_type = mach_read_from_4(desc_data + CLONE_DATA_STATE_OFFSET);

	m_state = static_cast<Snapshot_State>(int_type);

	m_task_meta.m_task_index = mach_read_from_4(
		desc_data + CLONE_DATA_TASK_INDEX_OFFSET);

	m_task_meta.m_chunk_num = mach_read_from_4(
		desc_data + CLONE_DATA_TASK_CHUNK_OFFSET);

	m_task_meta.m_block_num = mach_read_from_4(
		desc_data + CLONE_DATA_TASK_BLOCK_OFFSET);

	m_file_index = mach_read_from_4(desc_data + CLONE_DATA_FILE_IDX_OFFSET);
	m_data_len = mach_read_from_4(desc_data + CLONE_DATA_LEN_OFFSET);
	m_file_offset = mach_read_from_8(desc_data + CLONE_DATA_FOFF_OFFSET);
	m_file_size = mach_read_from_8(desc_data + CLONE_DATA_FILE_SIZE_OFFSET);
}
