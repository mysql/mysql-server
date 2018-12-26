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

/** @file include/clone0desc.h
 Innodb clone descriptors

 *******************************************************/

#ifndef CLONE_DESC_INCLUDE
#define CLONE_DESC_INCLUDE

#include "mem0mem.h"
#include "univ.i"

/** Invalid locator ID. */
const ib_uint64_t CLONE_LOC_INVALID_ID = 0;

/** Maximum base length for any serialized descriptor. This is only used for
optimal allocation and has no impact on version compatibility. */
const ib_uint64_t CLONE_DESC_MAX_BASE_LEN = 64;

/** Snapshot state transfer during clone.

Clone Type: HA_CLONE_BLOCKING
@startuml
  state CLONE_SNAPSHOT_INIT
  state CLONE_SNAPSHOT_FILE_COPY
  state CLONE_SNAPSHOT_DONE

  [*] -down-> CLONE_SNAPSHOT_INIT : Build snapshot
  CLONE_SNAPSHOT_INIT -right-> CLONE_SNAPSHOT_FILE_COPY
  CLONE_SNAPSHOT_FILE_COPY -right-> CLONE_SNAPSHOT_DONE
  CLONE_SNAPSHOT_DONE -down-> [*] : Destroy snapshot
@enduml

Clone Type: HA_CLONE_REDO
@startuml
  state CLONE_SNAPSHOT_REDO_COPY

  [*] -down-> CLONE_SNAPSHOT_INIT : Build snapshot
  CLONE_SNAPSHOT_INIT -right-> CLONE_SNAPSHOT_FILE_COPY : Start redo archiving
  CLONE_SNAPSHOT_FILE_COPY -right-> CLONE_SNAPSHOT_REDO_COPY
  CLONE_SNAPSHOT_REDO_COPY -right-> CLONE_SNAPSHOT_DONE
  CLONE_SNAPSHOT_DONE -down-> [*] : Destroy snapshot
@enduml

Clone Type: HA_CLONE_HYBRID
@startuml
  state CLONE_SNAPSHOT_PAGE_COPY

  [*] -down-> CLONE_SNAPSHOT_INIT : Build snapshot
  CLONE_SNAPSHOT_INIT -right-> CLONE_SNAPSHOT_FILE_COPY : Start page tracking
  CLONE_SNAPSHOT_FILE_COPY -right-> CLONE_SNAPSHOT_PAGE_COPY : Start redo \
  archiving
  CLONE_SNAPSHOT_PAGE_COPY -right-> CLONE_SNAPSHOT_REDO_COPY
  CLONE_SNAPSHOT_REDO_COPY -right> CLONE_SNAPSHOT_DONE
  CLONE_SNAPSHOT_DONE -down-> [*] : Destroy snapshot
@enduml

Clone Type: HA_CLONE_PAGE: Not implemented
*/
enum Snapshot_State {
  /** Invalid state */
  CLONE_SNAPSHOT_NONE = 0,

  /** Initialize state when snapshot object is created */
  CLONE_SNAPSHOT_INIT,

  /** Snapshot state while transferring files. */
  CLONE_SNAPSHOT_FILE_COPY,

  /** Snapshot state while transferring pages. */
  CLONE_SNAPSHOT_PAGE_COPY,

  /** Snapshot state while transferring redo. */
  CLONE_SNAPSHOT_REDO_COPY,

  /** Snapshot state at end after finishing transfer. */
  CLONE_SNAPSHOT_DONE
};

/** Choose lowest descriptor version between reference locator
and currently supported version.
@param[in]	ref_loc	reference locator
@return chosen version */
uint choose_desc_version(byte *ref_loc);

/** Clone descriptors contain meta information needed for applying cloned data.
These are PODs with interface to serialize and deserialize them. */
enum Clone_Desc_Type {
  /** Logical pointer to identify a clone operation */
  CLONE_DESC_LOCATOR = 1,

  /** Metadata for a Task/Thread for clone operation */
  CLONE_DESC_TASK_METADATA,

  /** Information for snapshot state */
  CLONE_DESC_STATE,

  /** Metadata for a database file */
  CLONE_DESC_FILE_METADATA,

  /** Information for a data block */
  CLONE_DESC_DATA,

  /** Must be the last member */
  CLONE_DESC_MAX
};

/** Header common to all descriptors. */
struct Clone_Desc_Header {
  /** Descriptor version */
  uint m_version;

  /** Serialized length of descriptor in bytes */
  uint m_length;

  /** Descriptor type */
  Clone_Desc_Type m_type;

  /** Serialize the descriptor header: Caller must allocate
  the serialized buffer.
  @param[out]	desc_hdr	serialized header */
  void serialize(byte *desc_hdr);

  /** Deserialize the descriptor header.
  @param[in]	desc_hdr	serialized header */
  void deserialize(byte *desc_hdr);
};

/** Task information in clone operation. */
struct Clone_Task_Meta {
  /** Index in task array. */
  uint m_task_index;

  /** Current chunk number reserved by the task. */
  uint m_chunk_num;

  /** Current block number that is already transferred. */
  uint m_block_num;
};

/** CLONE_DESC_LOCATOR: Descriptor for a task for clone operation.
A task is used by exactly one thread */
struct Clone_Desc_Locator {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Unique identifier for a clone operation. */
  ib_uint64_t m_clone_id;

  /** Unique identifier for a clone snapshot. */
  ib_uint64_t m_snapshot_id;

  /** Index in clone array for fast reference. */
  uint m_clone_index;

  /** Initialize clone locator.
  @param[in]	id	Clone identifier
  @param[in]	snap_id	Snapshot identifier
  @param[in]	version	Descriptor version
  @param[in]	index	clone index */
  void init(ib_uint64_t id, ib_uint64_t snap_id, uint version, uint index);

  /** Check if the passed locator matches the current one.
  @param[in]	other_desc	input locator descriptor
  @return true if matches */
  bool match(Clone_Desc_Locator *other_desc);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]	desc_loc	serialized descriptor
  @param[in,out]	len		length of serialized descriptor
  @param[in]	heap		heap for allocating memory */
  void serialize(byte *&desc_loc, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]	desc_loc	serialized locator */
  void deserialize(byte *desc_loc);
};

/** CLONE_DESC_TASK_METADATA: Descriptor for a task for clone operation.
A task is used by exactly one thread */
struct Clone_Desc_Task_Meta {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Task information */
  Clone_Task_Meta m_task_meta;

  /** Initialize header
  @param[in]	version	descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]	desc_task	serialized descriptor
  @param[in,out]	len		length of serialized descriptor
  @param[in]	heap		heap for allocating memory */
  void serialize(byte *&desc_task, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]	desc_task	serialized descriptor */
  void deserialize(byte *desc_task);
};

/** CLONE_DESC_STATE: Descriptor for current snapshot state */
struct Clone_Desc_State {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Current snapshot State */
  Snapshot_State m_state;

  /** Task identifier */
  uint m_task_index;

  /** Number of chunks in current state */
  uint m_num_chunks;

  /** Number of files in current state */
  uint m_num_files;

  /** Initialize header
  @param[in]	version	descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]	desc_state	serialized descriptor
  @param[in,out]	len		length of serialized descriptor
  @param[in]	heap		heap for allocating memory */
  void serialize(byte *&desc_state, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]	desc_state	serialized descriptor */
  void deserialize(byte *desc_state);
};

/** Clone file information */
struct Clone_File_Meta {
  /** File size in bytes */
  ib_uint64_t m_file_size;

  /** Tablespace ID for the file */
  ulint m_space_id;

  /** File index in clone data file vector */
  uint m_file_index;

  /** Chunk number for the first chunk in file */
  uint m_begin_chunk;

  /** Chunk number for the last chunk in file */
  uint m_end_chunk;

  /** File name length in bytes */
  size_t m_file_name_len;

  /** File name */
  const char *m_file_name;
};

/** CLONE_DESC_FILE_METADATA: Descriptor for file metadata */
struct Clone_Desc_File_MetaData {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Current snapshot State */
  Snapshot_State m_state;

  /** File metadata */
  Clone_File_Meta m_file_meta;

  /** Initialize header
  @param[in]	version	descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]	desc_file	serialized descriptor
  @param[in,out]	len		length of serialized descriptor
  @param[in]	heap		heap for allocating memory */
  void serialize(byte *&desc_file, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]	desc_file	serialized descriptor */
  void deserialize(byte *desc_file);
};

/** CLONE_DESC_DATA: Descriptor for data */
struct Clone_Desc_Data {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Current snapshot State */
  Snapshot_State m_state;

  /** Task information */
  Clone_Task_Meta m_task_meta;

  /** File identifier */
  uint32_t m_file_index;

  /** Data Length */
  uint32_t m_data_len;

  /** File offset for the data */
  uint64_t m_file_offset;

  /** Updated file size */
  uint64_t m_file_size;

  /** Initialize header
  @param[in]	version	descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]	desc_data	serialized descriptor
  @param[in,out]	len		length of serialized descriptor
  @param[in]	heap		heap for allocating memory */
  void serialize(byte *&desc_data, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]	desc_data	serialized descriptor */
  void deserialize(byte *desc_data);
};

#endif /* CLONE_DESC_INCLUDE */
