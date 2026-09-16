/*****************************************************************************

Copyright (c) 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/clone_descriptor_format.h
 Shared serialized clone descriptor layout constants.

 *******************************************************/

#ifndef CLONE_DESCRIPTOR_FORMAT_INCLUDED
#define CLONE_DESCRIPTOR_FORMAT_INCLUDED

#include <cstdint>

namespace clone_desc_format {

/** Highest serialized descriptor version supported by this format. */
inline constexpr std::uint32_t CLONE_DESC_MAX_VERSION = 100;

/** Descriptor version field offset in the serialized header. */
inline constexpr std::uint32_t CLONE_DESC_VER_OFFSET = 0;
/** Descriptor length field offset in the serialized header. */
inline constexpr std::uint32_t CLONE_DESC_LEN_OFFSET =
    CLONE_DESC_VER_OFFSET + 4;
/** Descriptor type field offset in the serialized header. */
inline constexpr std::uint32_t CLONE_DESC_TYPE_OFFSET =
    CLONE_DESC_LEN_OFFSET + 4;
/** Serialized descriptor header length in bytes. */
inline constexpr std::uint32_t CLONE_DESC_HEADER_LEN =
    CLONE_DESC_TYPE_OFFSET + 4;

/** Serialized type value for file metadata descriptors. */
inline constexpr std::uint32_t CLONE_DESC_FILE_METADATA_TYPE = 4;

/** Task metadata task index field offset. */
inline constexpr std::uint32_t CLONE_TASK_INDEX_OFFSET = CLONE_DESC_HEADER_LEN;
/** Task metadata chunk number field offset. */
inline constexpr std::uint32_t CLONE_TASK_CHUNK_OFFSET =
    CLONE_TASK_INDEX_OFFSET + 4;
/** Task metadata block number field offset. */
inline constexpr std::uint32_t CLONE_TASK_BLOCK_OFFSET =
    CLONE_TASK_CHUNK_OFFSET + 4;
/** Serialized task metadata descriptor length in bytes. */
inline constexpr std::uint32_t CLONE_TASK_META_LEN =
    CLONE_TASK_BLOCK_OFFSET + 4;

/** Locator descriptor clone id field offset. */
inline constexpr std::uint32_t CLONE_LOC_CID_OFFSET = CLONE_DESC_HEADER_LEN;
/** Locator descriptor snapshot id field offset. */
inline constexpr std::uint32_t CLONE_LOC_SID_OFFSET = CLONE_LOC_CID_OFFSET + 8;
/** Locator descriptor clone array index field offset. */
inline constexpr std::uint32_t CLONE_LOC_IDX_OFFSET = CLONE_LOC_SID_OFFSET + 8;
/** Locator descriptor snapshot state field offset. */
inline constexpr std::uint32_t CLONE_LOC_STATE_OFFSET =
    CLONE_LOC_IDX_OFFSET + 4;
/** Locator descriptor snapshot sub-state field offset. */
inline constexpr std::uint32_t CLONE_LOC_META_OFFSET =
    CLONE_LOC_STATE_OFFSET + 1;
/** Serialized locator descriptor base length in bytes. */
inline constexpr std::uint32_t CLONE_DESC_LOC_BASE_LEN =
    CLONE_LOC_META_OFFSET + 1;

/** File metadata state field offset. */
inline constexpr std::uint32_t CLONE_FILE_STATE_OFFSET = CLONE_DESC_HEADER_LEN;
/** File metadata file size field offset. */
inline constexpr std::uint32_t CLONE_FILE_SIZE_OFFSET =
    CLONE_FILE_STATE_OFFSET + 4;
/** File metadata allocated size field offset. */
inline constexpr std::uint32_t CLONE_FILE_ALLOC_SIZE_OFFSET =
    CLONE_FILE_SIZE_OFFSET + 8;
/** File metadata FSP flags field offset. */
inline constexpr std::uint32_t CLONE_FILE_FSP_OFFSET =
    CLONE_FILE_ALLOC_SIZE_OFFSET + 8;
/** File metadata filesystem block size field offset. */
inline constexpr std::uint32_t CLONE_FILE_FSBLK_OFFSET =
    CLONE_FILE_FSP_OFFSET + 4;
/** File metadata flags field offset. */
inline constexpr std::uint32_t CLONE_FILE_FLAGS_OFFSET =
    CLONE_FILE_FSBLK_OFFSET + 4;
/** File metadata zlib compression flag bit. */
inline constexpr std::uint32_t CLONE_DESC_FILE_FLAG_ZLIB = 1;
/** File metadata LZ4 compression flag bit. */
inline constexpr std::uint32_t CLONE_DESC_FILE_FLAG_LZ4 = 2;
/** File metadata encryption flag bit. */
inline constexpr std::uint32_t CLONE_DESC_FILE_FLAG_AES = 3;
/** File metadata renamed flag bit. */
inline constexpr std::uint32_t CLONE_DESC_FILE_FLAG_RENAMED = 4;
/** File metadata deleted flag bit. */
inline constexpr std::uint32_t CLONE_DESC_FILE_FLAG_DELETED = 5;
/** File metadata key-presence flag bit. */
inline constexpr std::uint32_t CLONE_DESC_FILE_HAS_KEY = 6;
/** File metadata tablespace id field offset. */
inline constexpr std::uint32_t CLONE_FILE_SPACE_ID_OFFSET =
    CLONE_FILE_FLAGS_OFFSET + 2;
/** File metadata file index field offset. */
inline constexpr std::uint32_t CLONE_FILE_IDX_OFFSET =
    CLONE_FILE_SPACE_ID_OFFSET + 4;
/** File metadata begin chunk field offset. */
inline constexpr std::uint32_t CLONE_FILE_BCHUNK_OFFSET =
    CLONE_FILE_IDX_OFFSET + 4;
/** File metadata end chunk field offset. */
inline constexpr std::uint32_t CLONE_FILE_ECHUNK_OFFSET =
    CLONE_FILE_BCHUNK_OFFSET + 4;
/** File metadata file name length field offset. */
inline constexpr std::uint32_t CLONE_FILE_FNAMEL_OFFSET =
    CLONE_FILE_ECHUNK_OFFSET + 4;
/** File metadata file name field offset. */
inline constexpr std::uint32_t CLONE_FILE_FNAME_OFFSET =
    CLONE_FILE_FNAMEL_OFFSET + 4;
/** Serialized file metadata descriptor base length in bytes. */
inline constexpr std::uint32_t CLONE_FILE_BASE_LEN = CLONE_FILE_FNAME_OFFSET;

/** State descriptor clone state field offset. */
inline constexpr std::uint32_t CLONE_DESC_STATE_OFFSET = CLONE_DESC_HEADER_LEN;
/** State descriptor task index field offset. */
inline constexpr std::uint32_t CLONE_DESC_TASK_OFFSET =
    CLONE_DESC_STATE_OFFSET + 4;
/** State descriptor chunk count field offset. */
inline constexpr std::uint32_t CLONE_DESC_STATE_NUM_CHUNKS =
    CLONE_DESC_TASK_OFFSET + 4;
/** State descriptor file count field offset. */
inline constexpr std::uint32_t CLONE_DESC_STATE_NUM_FILES =
    CLONE_DESC_STATE_NUM_CHUNKS + 4;
/** State descriptor estimated byte count field offset. */
inline constexpr std::uint32_t CLONE_DESC_STATE_EST_BYTES =
    CLONE_DESC_STATE_NUM_FILES + 4;
/** State descriptor estimated disk usage field offset. */
inline constexpr std::uint32_t CLONE_DESC_STATE_EST_DISK =
    CLONE_DESC_STATE_EST_BYTES + 8;
/** State descriptor flags field offset. */
inline constexpr std::uint32_t CLONE_DESC_STATE_FLAGS =
    CLONE_DESC_STATE_EST_DISK + 8;
/** Serialized state descriptor length in bytes. */
inline constexpr std::uint32_t CLONE_DESC_STATE_LEN =
    CLONE_DESC_STATE_FLAGS + 2;
/** State descriptor start flag bit. */
inline constexpr std::uint32_t CLONE_DESC_STATE_FLAG_START = 1;
/** State descriptor acknowledgement flag bit. */
inline constexpr std::uint32_t CLONE_DESC_STATE_FLAG_ACK = 2;

/** Data descriptor state field offset. */
inline constexpr std::uint32_t CLONE_DATA_STATE_OFFSET = CLONE_DESC_HEADER_LEN;
/** Data descriptor task index field offset. */
inline constexpr std::uint32_t CLONE_DATA_TASK_INDEX_OFFSET =
    CLONE_DATA_STATE_OFFSET + 4;
/** Data descriptor chunk number field offset. */
inline constexpr std::uint32_t CLONE_DATA_TASK_CHUNK_OFFSET =
    CLONE_DATA_TASK_INDEX_OFFSET + 4;
/** Data descriptor block number field offset. */
inline constexpr std::uint32_t CLONE_DATA_TASK_BLOCK_OFFSET =
    CLONE_DATA_TASK_CHUNK_OFFSET + 4;
/** Data descriptor file index field offset. */
inline constexpr std::uint32_t CLONE_DATA_FILE_IDX_OFFSET =
    CLONE_DATA_TASK_BLOCK_OFFSET + 4;
/** Data descriptor data length field offset. */
inline constexpr std::uint32_t CLONE_DATA_LEN_OFFSET =
    CLONE_DATA_FILE_IDX_OFFSET + 4;
/** Data descriptor file offset field offset. */
inline constexpr std::uint32_t CLONE_DATA_FOFF_OFFSET =
    CLONE_DATA_LEN_OFFSET + 4;
/** Data descriptor file size field offset. */
inline constexpr std::uint32_t CLONE_DATA_FILE_SIZE_OFFSET =
    CLONE_DATA_FOFF_OFFSET + 8;
/** Serialized data descriptor length in bytes. */
inline constexpr std::uint32_t CLONE_DESC_DATA_LEN =
    CLONE_DATA_FILE_SIZE_OFFSET + 8;

}  // namespace clone_desc_format

#endif  // CLONE_DESCRIPTOR_FORMAT_INCLUDED
