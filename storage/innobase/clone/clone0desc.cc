/*****************************************************************************

Copyright (c) 2019, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file clone/clone0desc.cc
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
static const uint CLONE_DESC_LEN_OFFSET = CLONE_DESC_VER_OFFSET + 4;

/** Header: Descriptor type is in next 4 bytes */
static const uint CLONE_DESC_TYPE_OFFSET = CLONE_DESC_LEN_OFFSET + 4;

/** Header: Fixed length. */
static const uint CLONE_DESC_HEADER_LEN = CLONE_DESC_TYPE_OFFSET + 4;

uint choose_desc_version(const byte *ref_loc) {
  if (ref_loc == nullptr) {
    return (CLONE_DESC_MAX_VERSION);
  }

  Clone_Desc_Header header;
  uint version;

  header.deserialize(ref_loc, CLONE_DESC_HEADER_LEN);
  version = header.m_version;

  /* Choose the minimum of remote locator version local
  supported version. */
  if (version > CLONE_DESC_MAX_VERSION) {
    version = CLONE_DESC_MAX_VERSION;
  }

  return (version);
}

void Clone_Desc_Header::serialize(byte *desc_hdr) {
  mach_write_to_4(desc_hdr + CLONE_DESC_VER_OFFSET, m_version);
  mach_write_to_4(desc_hdr + CLONE_DESC_LEN_OFFSET, m_length);
  mach_write_to_4(desc_hdr + CLONE_DESC_TYPE_OFFSET, m_type);
}

bool Clone_Desc_Header::deserialize(const byte *desc_hdr, uint desc_len) {
  if (desc_len < CLONE_DESC_HEADER_LEN) {
    return (false);
  }
  m_version = mach_read_from_4(desc_hdr + CLONE_DESC_VER_OFFSET);
  m_length = mach_read_from_4(desc_hdr + CLONE_DESC_LEN_OFFSET);

  uint int_type;
  int_type = mach_read_from_4(desc_hdr + CLONE_DESC_TYPE_OFFSET);
  ut_ad(int_type < CLONE_DESC_MAX);

  m_type = static_cast<Clone_Desc_Type>(int_type);
  return (true);
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
void Clone_Desc_Task_Meta::init_header(uint version) {
  m_header.m_version = version;

  m_header.m_length = CLONE_TASK_META_LEN;

  m_header.m_type = CLONE_DESC_TASK_METADATA;
}

void Clone_Desc_Task_Meta::serialize(byte *&desc_task, uint &len,
                                     mem_heap_t *heap) {
  if (desc_task == nullptr) {
    len = m_header.m_length;
    desc_task = static_cast<byte *>(mem_heap_alloc(heap, len));
  } else {
    ut_ad(len >= m_header.m_length);
    len = m_header.m_length;
  }

  m_header.serialize(desc_task);

  mach_write_to_4(desc_task + CLONE_TASK_INDEX_OFFSET,
                  m_task_meta.m_task_index);
  mach_write_to_4(desc_task + CLONE_TASK_CHUNK_OFFSET, m_task_meta.m_chunk_num);
  mach_write_to_4(desc_task + CLONE_TASK_BLOCK_OFFSET, m_task_meta.m_block_num);
}

bool Clone_Desc_Task_Meta::deserialize(const byte *desc_task, uint desc_len) {
  /* Deserialize the header and validate type and length. */
  if (desc_len < CLONE_TASK_META_LEN ||
      !m_header.deserialize(desc_task, desc_len) ||
      m_header.m_type != CLONE_DESC_TASK_METADATA) {
    return (false);
  }
  m_task_meta.m_task_index =
      mach_read_from_4(desc_task + CLONE_TASK_INDEX_OFFSET);
  m_task_meta.m_chunk_num =
      mach_read_from_4(desc_task + CLONE_TASK_CHUNK_OFFSET);
  m_task_meta.m_block_num =
      mach_read_from_4(desc_task + CLONE_TASK_BLOCK_OFFSET);
  return (true);
}

/** Locator: Clone identifier in 8 bytes */
static const uint CLONE_LOC_CID_OFFSET = CLONE_DESC_HEADER_LEN;

/** Locator: Snapshot identifier in 8 bytes */
static const uint CLONE_LOC_SID_OFFSET = CLONE_LOC_CID_OFFSET + 8;

/** Locator: Clone array index in 4 bytes */
static const uint CLONE_LOC_IDX_OFFSET = CLONE_LOC_SID_OFFSET + 8;

/** Locator: Clone Snapshot state in 1 byte */
static const uint CLONE_LOC_STATE_OFFSET = CLONE_LOC_IDX_OFFSET + 4;

/** Locator: Clone Snapshot sub-state in 1 byte */
static const uint CLONE_LOC_META_OFFSET = CLONE_LOC_STATE_OFFSET + 1;

/** Locator: Total length */
static const uint CLONE_DESC_LOC_BASE_LEN = CLONE_LOC_META_OFFSET + 1;

uint32_t *Chnunk_Bitmap::reset(uint32_t max_bits, mem_heap_t *heap) {
  m_bits = max_bits;

  if (max_bits <= capacity()) {
    if (m_bitmap != nullptr && size() > 0) {
      memset(m_bitmap, 0, size());
    }
    return (nullptr);
  }

  auto old_buf = m_bitmap;

  m_size = static_cast<size_t>(m_bits >> 3);

  ut_ad(m_size == m_bits / 8);

  if (max_bits > capacity()) {
    ++m_size;
  }

  ut_ad(m_bits <= capacity());

  m_bitmap = static_cast<uint32_t *>(
      mem_heap_zalloc(heap, static_cast<ulint>(size())));

  return (old_buf);
}

uint32_t Chnunk_Bitmap::get_min_unset_bit() {
  uint32_t mask = 0;
  uint32_t return_bit = 0;
  size_t index = 0;

  mask = ~mask;

  /* Find the first block with unset BIT */
  for (index = 0; index < m_size; ++index) {
    if ((m_bitmap[index] & mask) != mask || return_bit >= m_bits) {
      break;
    }

    return_bit += 32;
  }

  /* All BITs are set */
  if (index >= m_size || return_bit >= m_bits) {
    return (m_bits + 1);
  }

  auto val = m_bitmap[index];
  ut_ad((val & mask) != mask);

  index = 0;

  /* Find the unset BIT within block */
  do {
    mask = 1 << index;

    if ((val & mask) == 0) {
      break;
    }

  } while (++index < 32);

  ut_ad(index < 32);

  return_bit += static_cast<uint32_t>(index);

  /* Change from 0 to 1 based index */
  ++return_bit;

  ut_ad(return_bit <= m_bits + 1);

  return (return_bit);
}

uint32_t Chnunk_Bitmap::get_max_set_bit() {
  uint32_t return_bit = 0;
  size_t block_index = 0;
  size_t index = 0;

  /* Find the last block with set BIT */
  for (index = 0; index < m_size; ++index) {
    if (return_bit >= m_bits) {
      break;
    }

    if (m_bitmap[index] != 0) {
      block_index = index + 1;
    }

    return_bit += 32;
  }

  /* No BITs are set */
  if (block_index == 0) {
    return (0);
  }

  --block_index;
  return_bit = static_cast<uint32_t>(block_index * 32);

  auto val = m_bitmap[block_index];
  ut_ad(val != 0);

  uint32_t mask = 0;
  index = 0;

  /* Find the last BIT set within block */
  do {
    mask = 1 << index;

    if ((val & mask) != 0) {
      block_index = index;
    }

  } while (++index < 32);

  return_bit += static_cast<uint32_t>(block_index);

  /* Change from 0 to 1 based index */
  ++return_bit;

  ut_ad(return_bit <= m_bits);

  return (return_bit);
}

size_t Chnunk_Bitmap::get_serialized_length() {
  /* Length of chunk BITMAP data */
  size_t ret_size = 4;

  /* Add size for chunk bitmap data */
  ret_size += size();

  return (ret_size);
}

size_t Chunk_Info::get_serialized_length(uint32_t num_tasks) {
  /* Length of incomplete chunk data */
  size_t ret_size = 4;

  auto num_elements = m_incomplete_chunks.size();

  /* Have bigger allocated length if requested */
  if (num_tasks > num_elements) {
    num_elements = num_tasks;
  }

  /* Add size for incomplete chunks data. Serialized element
  has chunk and block number: 4 + 4 = 8 bytes */
  ret_size += (8 * num_elements);

  /* Add length of chunk bitmap data */
  ret_size += m_reserved_chunks.get_serialized_length();

  return (ret_size);
}

void Chnunk_Bitmap::serialize(byte *&desc_chunk, uint &len) {
  auto len_left = len;
  auto bitmap_size = static_cast<ulint>(m_size);

  mach_write_to_4(desc_chunk, bitmap_size);
  desc_chunk += 4;

  ut_ad(len_left >= 4);
  len_left -= 4;

  for (size_t index = 0; index < m_size; ++index) {
    auto val = static_cast<ulint>(m_bitmap[index]);

    mach_write_to_4(desc_chunk, val);
    desc_chunk += 4;

    ut_ad(len_left >= 4);
    len_left -= 4;
  }

  ut_ad(len > len_left);
  len -= len_left;
}

void Chunk_Info::serialize(byte *desc_chunk, uint &len) {
  auto len_left = len;
  auto chunk_map_size = static_cast<ulint>(m_incomplete_chunks.size());

  mach_write_to_4(desc_chunk, chunk_map_size);
  desc_chunk += 4;

  ut_ad(len_left >= 4);
  len_left -= 4;

  ulint index = 0;

  for (auto &key_value : m_incomplete_chunks) {
    ut_ad(index < chunk_map_size);

    mach_write_to_4(desc_chunk, key_value.first);
    desc_chunk += 4;

    ut_ad(len_left >= 4);
    len_left -= 4;

    mach_write_to_4(desc_chunk, key_value.second);
    desc_chunk += 4;

    ut_ad(len_left >= 4);
    len_left -= 4;

    ++index;
  }
  ut_ad(index == chunk_map_size);

  /* Actual length for serialized chunk map */
  ut_ad(len > len_left);
  len -= len_left;

  m_reserved_chunks.serialize(desc_chunk, len_left);

  /* Total serialized length */
  len += len_left;
}

void Chnunk_Bitmap::deserialize(const byte *desc_chunk, uint &len_left) {
  auto bitmap_size = mach_read_from_4(desc_chunk);
  desc_chunk += 4;

  if (len_left < 4) {
    ut_ad(false);
    return;
  }

  len_left -= 4;

  if (bitmap_size > m_size) {
    ut_ad(false);
    return;
  }

  for (ulint index = 0; index < bitmap_size; index++) {
    m_bitmap[index] = static_cast<uint32_t>(mach_read_from_4(desc_chunk));

    desc_chunk += 4;

    if (len_left < 4) {
      ut_ad(false);
      return;
    }

    len_left -= 4;
  }

  ut_ad(len_left == 0);
}

void Chunk_Info::deserialize(const byte *desc_chunk, uint &len_left) {
  auto chunk_map_size = mach_read_from_4(desc_chunk);

  desc_chunk += 4;

  if (len_left < 4) {
    ut_ad(false);
    return;
  }

  len_left -= 4;

  /* Each task can have one incomplete chunk at most */
  if (chunk_map_size > CLONE_MAX_TASKS) {
    ut_ad(false);
    return;
  }

  for (ulint index = 0; index < chunk_map_size; index++) {
    auto chunk_num = static_cast<uint32_t>(mach_read_from_4(desc_chunk));

    desc_chunk += 4;

    if (len_left < 4) {
      ut_ad(false);
      return;
    }
    len_left -= 4;

    auto block_num = static_cast<uint32_t>(mach_read_from_4(desc_chunk));
    desc_chunk += 4;

    if (len_left < 4) {
      ut_ad(false);
      return;
    }
    len_left -= 4;

    m_incomplete_chunks[chunk_num] = block_num;
  }

  m_reserved_chunks.deserialize(desc_chunk, len_left);

  ut_ad(len_left == 0);
}

void Clone_Desc_Locator::init(ib_uint64_t id, ib_uint64_t snap_id,
                              Snapshot_State state, uint version, uint index) {
  m_header.m_version = version;

  m_header.m_length = CLONE_DESC_LOC_BASE_LEN;

  m_header.m_type = CLONE_DESC_LOCATOR;

  m_clone_id = id;
  m_snapshot_id = snap_id;

  m_clone_index = index;
  m_state = state;
  m_metadata_transferred = false;
}

bool Clone_Desc_Locator::match(Clone_Desc_Locator *other_desc) {
#ifdef UNIV_DEBUG
  Clone_Desc_Header *other_header = &other_desc->m_header;
#endif /* UNIV_DEBUG */

  if (other_desc->m_clone_id == m_clone_id &&
      other_desc->m_snapshot_id == m_snapshot_id) {
    ut_ad(m_header.m_version == other_header->m_version);
    return (true);
  }

  return (false);
}

void Clone_Desc_Locator::serialize(byte *&desc_loc, uint &len,
                                   Chunk_Info *chunk_info, mem_heap_t *heap) {
  if (chunk_info != nullptr) {
    auto chunk_len = static_cast<uint>(chunk_info->get_serialized_length(0));

    m_header.m_length += chunk_len;
  }

  if (desc_loc == nullptr) {
    len = m_header.m_length;
    desc_loc = static_cast<byte *>(mem_heap_alloc(heap, len));
  } else {
    ut_ad(len >= m_header.m_length);
    len = m_header.m_length;
  }

  m_header.serialize(desc_loc);

  mach_write_to_8(desc_loc + CLONE_LOC_CID_OFFSET, m_clone_id);
  mach_write_to_8(desc_loc + CLONE_LOC_SID_OFFSET, m_snapshot_id);

  mach_write_to_4(desc_loc + CLONE_LOC_IDX_OFFSET, m_clone_index);

  mach_write_to_1(desc_loc + CLONE_LOC_STATE_OFFSET,
                  static_cast<ulint>(m_state));

  ulint sub_state = m_metadata_transferred ? 1 : 0;

  mach_write_to_1(desc_loc + CLONE_LOC_META_OFFSET, sub_state);

  if (chunk_info != nullptr) {
    ut_ad(len > CLONE_DESC_LOC_BASE_LEN);

    auto len_left = len - CLONE_DESC_LOC_BASE_LEN;

    chunk_info->serialize(desc_loc + CLONE_DESC_LOC_BASE_LEN, len_left);
  }
}

bool clone_validate_locator(const byte *desc_loc, uint desc_len) {
  Clone_Desc_Header header;

  if (!header.deserialize(desc_loc, desc_len)) {
    ut_ad(false);
    return (false);
  }
  if (desc_len < CLONE_DESC_LOC_BASE_LEN ||
      header.m_length < CLONE_DESC_LOC_BASE_LEN || header.m_length > desc_len ||
      header.m_type != CLONE_DESC_LOCATOR) {
    ut_ad(false);
    return (false);
  }
  return (true);
}

void Clone_Desc_Locator::deserialize(const byte *desc_loc, uint desc_len,
                                     Chunk_Info *chunk_info) {
  m_header.deserialize(desc_loc, CLONE_DESC_HEADER_LEN);

  ut_ad(m_header.m_type == CLONE_DESC_LOCATOR);

  if (m_header.m_length < CLONE_DESC_LOC_BASE_LEN ||
      m_header.m_length > desc_len) {
    ut_ad(false);
    return;
  }

  m_clone_id = mach_read_from_8(desc_loc + CLONE_LOC_CID_OFFSET);
  m_snapshot_id = mach_read_from_8(desc_loc + CLONE_LOC_SID_OFFSET);

  m_clone_index = mach_read_from_4(desc_loc + CLONE_LOC_IDX_OFFSET);

  m_state = static_cast<Snapshot_State>(
      mach_read_from_1(desc_loc + CLONE_LOC_STATE_OFFSET));

  auto sub_state = mach_read_from_1(desc_loc + CLONE_LOC_META_OFFSET);
  m_metadata_transferred = (sub_state == 0) ? false : true;

  ut_ad(m_header.m_length >= CLONE_DESC_LOC_BASE_LEN);

  auto len_left = m_header.m_length - CLONE_DESC_LOC_BASE_LEN;

  if (chunk_info != nullptr && len_left != 0) {
    chunk_info->deserialize(desc_loc + CLONE_DESC_LOC_BASE_LEN, len_left);
  }
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

void Clone_Desc_File_MetaData::init_header(uint version) {
  m_header.m_version = version;

  m_header.m_length = CLONE_FILE_BASE_LEN;
  m_header.m_length += static_cast<uint>(m_file_meta.m_file_name_len);

  m_header.m_type = CLONE_DESC_FILE_METADATA;
}

void Clone_Desc_File_MetaData::serialize(byte *&desc_file, uint &len,
                                         mem_heap_t *heap) {
  /* Allocate descriptor if needed. */
  if (desc_file == nullptr) {
    len = m_header.m_length;
    ut_ad(len == CLONE_FILE_FNAME_OFFSET + m_file_meta.m_file_name_len);

    desc_file = static_cast<byte *>(mem_heap_alloc(heap, len));
  } else {
    ut_ad(len >= m_header.m_length);
    len = m_header.m_length;
  }

  m_header.serialize(desc_file);

  mach_write_to_4(desc_file + CLONE_FILE_STATE_OFFSET, m_state);

  mach_write_to_8(desc_file + CLONE_FILE_SIZE_OFFSET, m_file_meta.m_file_size);
  mach_write_to_4(desc_file + CLONE_FILE_SPACE_ID_OFFSET,
                  m_file_meta.m_space_id);
  mach_write_to_4(desc_file + CLONE_FILE_IDX_OFFSET, m_file_meta.m_file_index);

  mach_write_to_4(desc_file + CLONE_FILE_BCHUNK_OFFSET,
                  m_file_meta.m_begin_chunk);
  mach_write_to_4(desc_file + CLONE_FILE_ECHUNK_OFFSET,
                  m_file_meta.m_end_chunk);

  mach_write_to_4(desc_file + CLONE_FILE_FNAMEL_OFFSET,
                  m_file_meta.m_file_name_len);

  /* Copy variable length file name. */
  if (m_file_meta.m_file_name_len != 0) {
    memcpy(static_cast<void *>(desc_file + CLONE_FILE_FNAME_OFFSET),
           static_cast<const void *>(m_file_meta.m_file_name),
           m_file_meta.m_file_name_len);
  }
}

bool Clone_Desc_File_MetaData::deserialize(const byte *desc_file,
                                           uint desc_len) {
  /* Deserialize the header and validate type and length. */
  if (desc_len < CLONE_FILE_BASE_LEN ||
      !m_header.deserialize(desc_file, desc_len) ||
      m_header.m_type != CLONE_DESC_FILE_METADATA) {
    return (false);
  }
  desc_len -= CLONE_FILE_BASE_LEN;

  uint int_type;
  int_type = mach_read_from_4(desc_file + CLONE_FILE_STATE_OFFSET);

  m_state = static_cast<Snapshot_State>(int_type);

  m_file_meta.m_file_size =
      mach_read_from_8(desc_file + CLONE_FILE_SIZE_OFFSET);
  m_file_meta.m_space_id =
      mach_read_from_4(desc_file + CLONE_FILE_SPACE_ID_OFFSET);
  m_file_meta.m_file_index =
      mach_read_from_4(desc_file + CLONE_FILE_IDX_OFFSET);

  m_file_meta.m_begin_chunk =
      mach_read_from_4(desc_file + CLONE_FILE_BCHUNK_OFFSET);
  m_file_meta.m_end_chunk =
      mach_read_from_4(desc_file + CLONE_FILE_ECHUNK_OFFSET);

  m_file_meta.m_file_name_len =
      mach_read_from_4(desc_file + CLONE_FILE_FNAMEL_OFFSET);

  ut_ad(m_header.m_length ==
        CLONE_FILE_FNAME_OFFSET + m_file_meta.m_file_name_len);

  /* Check if we have enough length. */
  if (desc_len < m_file_meta.m_file_name_len) {
    return (false);
  }

  if (m_file_meta.m_file_name_len == 0) {
    m_file_meta.m_file_name = nullptr;
  } else {
    m_file_meta.m_file_name =
        reinterpret_cast<const char *>(desc_file + CLONE_FILE_FNAME_OFFSET);
    auto last_char = m_file_meta.m_file_name[m_file_meta.m_file_name_len - 1];

    /* File name must be NULL terminated. */
    if (last_char != '\0') {
      return (false);
    }
  }
  return (true);
}

/** Clone State: Snapshot state in 4 bytes */
static const uint CLONE_DESC_STATE_OFFSET = CLONE_DESC_HEADER_LEN;

/** Clone State: Task index in 4 bytes */
static const uint CLONE_DESC_TASK_OFFSET = CLONE_DESC_STATE_OFFSET + 4;

/** Clone State: Number of chunks in 4 bytes */
static const uint CLONE_DESC_STATE_NUM_CHUNKS = CLONE_DESC_TASK_OFFSET + 4;

/** Clone State: Number of files in 4 bytes */
static const uint CLONE_DESC_STATE_NUM_FILES = CLONE_DESC_STATE_NUM_CHUNKS + 4;

/** Clone State: Estimated nuber of bytes in 8 bytes */
static const uint CLONE_DESC_STATE_EST_BYTES = CLONE_DESC_STATE_NUM_FILES + 4;

/** Clone State: flags in 2 byte [max 16 flags] */
static const uint CLONE_DESC_STATE_FLAGS = CLONE_DESC_STATE_EST_BYTES + 8;

/** Clone State: Total length */
static const uint CLONE_DESC_STATE_LEN = CLONE_DESC_STATE_FLAGS + 2;

UNIV_INLINE bool DESC_CHECK_FLAG(ulint flag, ulint bit) {
  return (!!((flag & (1ULL << (bit - 1))) > 0));
}

UNIV_INLINE void DESC_SET_FLAG(ulint &flag, ulint bit) {
  flag |= static_cast<ulint>(1ULL << (bit - 1));
}

/** Clone State Flag: Start processing state */
static const uint CLONE_DESC_STATE_FLAG_START = 1;

/** Clone State Flag: Acknowledge processing state */
static const uint CLONE_DESC_STATE_FLAG_ACK = 2;

void Clone_Desc_State::init_header(uint version) {
  m_header.m_version = version;

  m_header.m_length = CLONE_DESC_STATE_LEN;

  m_header.m_type = CLONE_DESC_STATE;
}

void Clone_Desc_State::serialize(byte *&desc_state, uint &len,
                                 mem_heap_t *heap) {
  /* Allocate descriptor if needed. */
  if (desc_state == nullptr) {
    len = m_header.m_length;
    desc_state = static_cast<byte *>(mem_heap_alloc(heap, len));
  } else {
    ut_ad(len >= m_header.m_length);
    len = m_header.m_length;
  }

  m_header.serialize(desc_state);

  mach_write_to_4(desc_state + CLONE_DESC_STATE_OFFSET, m_state);
  mach_write_to_4(desc_state + CLONE_DESC_TASK_OFFSET, m_task_index);

  mach_write_to_4(desc_state + CLONE_DESC_STATE_NUM_CHUNKS, m_num_chunks);
  mach_write_to_4(desc_state + CLONE_DESC_STATE_NUM_FILES, m_num_files);
  mach_write_to_8(desc_state + CLONE_DESC_STATE_EST_BYTES, m_estimate);

  ulint state_flags = 0;

  if (m_is_start) {
    DESC_SET_FLAG(state_flags, CLONE_DESC_STATE_FLAG_START);
  }

  if (m_is_ack) {
    DESC_SET_FLAG(state_flags, CLONE_DESC_STATE_FLAG_ACK);
  }

  mach_write_to_2(desc_state + CLONE_DESC_STATE_FLAGS, state_flags);
}

bool Clone_Desc_State::deserialize(const byte *desc_state, uint desc_len) {
  /* Deserialize the header and validate type and length. */
  if (desc_len < CLONE_DESC_STATE_LEN ||
      !m_header.deserialize(desc_state, desc_len) ||
      m_header.m_type != CLONE_DESC_STATE) {
    return (false);
  }

  uint int_type;
  int_type = mach_read_from_4(desc_state + CLONE_DESC_STATE_OFFSET);

  m_state = static_cast<Snapshot_State>(int_type);

  m_task_index = mach_read_from_4(desc_state + CLONE_DESC_TASK_OFFSET);

  m_num_chunks = mach_read_from_4(desc_state + CLONE_DESC_STATE_NUM_CHUNKS);
  m_num_files = mach_read_from_4(desc_state + CLONE_DESC_STATE_NUM_FILES);
  m_estimate = mach_read_from_8(desc_state + CLONE_DESC_STATE_EST_BYTES);

  auto state_flags =
      static_cast<ulint>(mach_read_from_2(desc_state + CLONE_DESC_STATE_FLAGS));

  m_is_start = DESC_CHECK_FLAG(state_flags, CLONE_DESC_STATE_FLAG_START);

  m_is_ack = DESC_CHECK_FLAG(state_flags, CLONE_DESC_STATE_FLAG_ACK);

  return (true);
}

/** Clone Data: Snapshot state in 4 bytes */
static const uint CLONE_DATA_STATE_OFFSET = CLONE_DESC_HEADER_LEN;

/** Clone Data: Task index in 4 bytes */
static const uint CLONE_DATA_TASK_INDEX_OFFSET = CLONE_DATA_STATE_OFFSET + 4;

/** Clone Data: Current chunk number in 4 bytes */
static const uint CLONE_DATA_TASK_CHUNK_OFFSET =
    CLONE_DATA_TASK_INDEX_OFFSET + 4;

/** Clone Data: Current block number in 4 bytes */
static const uint CLONE_DATA_TASK_BLOCK_OFFSET =
    CLONE_DATA_TASK_CHUNK_OFFSET + 4;

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

void Clone_Desc_Data::init_header(uint version) {
  m_header.m_version = version;

  m_header.m_length = CLONE_DESC_DATA_LEN;

  m_header.m_type = CLONE_DESC_DATA;
}

void Clone_Desc_Data::serialize(byte *&desc_data, uint &len, mem_heap_t *heap) {
  /* Allocate descriptor if needed. */
  if (desc_data == nullptr) {
    len = m_header.m_length;
    desc_data = static_cast<byte *>(mem_heap_alloc(heap, len));
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

bool Clone_Desc_Data::deserialize(const byte *desc_data, uint desc_len) {
  /* Deserialize the header and validate type and length. */
  if (desc_len < CLONE_DESC_DATA_LEN ||
      !m_header.deserialize(desc_data, desc_len) ||
      m_header.m_type != CLONE_DESC_DATA) {
    return (false);
  }

  uint int_type;
  int_type = mach_read_from_4(desc_data + CLONE_DATA_STATE_OFFSET);

  m_state = static_cast<Snapshot_State>(int_type);

  m_task_meta.m_task_index =
      mach_read_from_4(desc_data + CLONE_DATA_TASK_INDEX_OFFSET);

  m_task_meta.m_chunk_num =
      mach_read_from_4(desc_data + CLONE_DATA_TASK_CHUNK_OFFSET);

  m_task_meta.m_block_num =
      mach_read_from_4(desc_data + CLONE_DATA_TASK_BLOCK_OFFSET);

  m_file_index = mach_read_from_4(desc_data + CLONE_DATA_FILE_IDX_OFFSET);
  m_data_len = mach_read_from_4(desc_data + CLONE_DATA_LEN_OFFSET);
  m_file_offset = mach_read_from_8(desc_data + CLONE_DATA_FOFF_OFFSET);
  m_file_size = mach_read_from_8(desc_data + CLONE_DATA_FILE_SIZE_OFFSET);

  return (true);
}
