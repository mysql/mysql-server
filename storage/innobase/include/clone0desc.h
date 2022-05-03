/*****************************************************************************

Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
#include "os0file.h"
#include "univ.i"

/** Invalid locator ID. */
const uint64_t CLONE_LOC_INVALID_ID = 0;

/** Maximum base length for any serialized descriptor. This is only used for
optimal allocation and has no impact on version compatibility. */
const uint32_t CLONE_DESC_MAX_BASE_LEN =
    64 + Encryption::KEY_LEN + Encryption::KEY_LEN;
/** Align by 4K for O_DIRECT */
const uint32_t CLONE_ALIGN_DIRECT_IO = 4 * 1024;

/** Maximum number of concurrent tasks for each clone */
const int CLONE_MAX_TASKS = 128;

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
enum Snapshot_State : uint32_t {
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

/** Total number of data transfer stages in clone. */
const size_t CLONE_MAX_TRANSFER_STAGES = 3;

/** Choose lowest descriptor version between reference locator
and currently supported version.
@param[in]      ref_loc reference locator
@return chosen version */
uint choose_desc_version(const byte *ref_loc);

/** Check if clone locator is valid
@param[in]      desc_loc        serialized descriptor
@param[in]      desc_len        descriptor length
@return true, if valid locator */
bool clone_validate_locator(const byte *desc_loc, uint desc_len);

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
  @param[out]   desc_hdr        serialized header */
  void serialize(byte *desc_hdr);

  /** Deserialize the descriptor header.
  @param[in]    desc_hdr        serialized header
  @param[in]    desc_len        descriptor length
  @return true, if successful. */
  bool deserialize(const byte *desc_hdr, uint desc_len);
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

/** Map for current block number for unfinished chunks. Used during
restart from incomplete clone operation. */
using Chunk_Map = std::map<uint32_t, uint32_t>;

/** Bitmap for completed chunks in current state */
class Chnunk_Bitmap {
 public:
  /** Construct bitmap */
  Chnunk_Bitmap() : m_bitmap(), m_size(), m_bits() {}

  /** Bitmap array index operator implementation */
  class Bitmap_Operator_Impl {
   public:
    /** Construct bitmap operator
    @param[in]  bitmap  reference to bitmap buffer
    @param[in]  index   array operation index */
    Bitmap_Operator_Impl(uint32_t *&bitmap, uint32_t index)

        : m_bitmap_ref(bitmap) {
      /* BYTE position */
      auto byte_index = index >> 3;
      ut_ad(byte_index == index / 8);

      /* MAP array position */
      m_map_index = byte_index >> 2;
      ut_ad(m_map_index == byte_index / 4);

      /* BIT position */
      auto bit_pos = index & 31;
      ut_ad(bit_pos == index % 32);

      m_bit_mask = 1 << bit_pos;
    }

    /** Check value at specified index in BITMAP
    @return true if the BIT is set */
    operator bool() const {
      auto &val = m_bitmap_ref[m_map_index];

      if ((val & m_bit_mask) == 0) {
        return (false);
      }

      return (true);
    }

    /** Set BIT at specific index
    @param[in]  bit     bit value to set */
    void operator=(bool bit) {
      auto &val = m_bitmap_ref[m_map_index];

      if (bit) {
        val |= m_bit_mask;
      } else {
        val &= ~m_bit_mask;
      }
    }

   private:
    /** Reference to BITMAP array */
    uint32_t *&m_bitmap_ref;

    /** Current array position */
    uint32_t m_map_index;

    /** Mask with current BIT set */
    uint32_t m_bit_mask;
  };

  /** Array index operator
  @param[in]    index   bitmap array index
  @return       operator implementation object */
  Bitmap_Operator_Impl operator[](uint32_t index) {
    /* Convert to zero based index */
    --index;

    ut_a(index < m_bits);
    return (Bitmap_Operator_Impl(m_bitmap, index));
  }

  /** Reset bitmap with new size
  @param[in]    max_bits        number of BITs to hold
  @param[in]    heap            heap for allocating memory
  @return       old buffer pointer */
  uint32_t *reset(uint32_t max_bits, mem_heap_t *heap);

  /** Get minimum BIT position that is not set
  @return BIT position */
  uint32_t get_min_unset_bit();

  /** Get maximum BIT position that is not set
  @return BIT position */
  uint32_t get_max_set_bit();

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_chunk      serialized chunk info
  @param[in,out]        len             length of serialized descriptor */
  void serialize(byte *&desc_chunk, uint &len);

  /** Deserialize the descriptor.
  @param[in]    desc_chunk      serialized chunk info
  @param[in,out]        len_left        length left in bytes */
  void deserialize(const byte *desc_chunk, uint &len_left);

  /** Get the length of serialized data
  @return length serialized chunk info */
  size_t get_serialized_length();

  /** Maximum bit capacity
  @return maximum number of BITs it can hold */
  size_t capacity() const { return (8 * size()); }

  /** Size of bitmap in bytes
  @return BITMAP buffer size */
  size_t size() const { return (m_size * 4); }

  /** Size of bitmap in bits
  @return number of BITs stored */
  uint32_t size_bits() const { return (m_bits); }

 private:
  /** BITMAP buffer */
  uint32_t *m_bitmap;

  /** BITMAP buffer size: Number of 4 byte blocks */
  size_t m_size;

  /** Total number of BITs in the MAP */
  uint32_t m_bits;
};

/** Incomplete Chunk information */
struct Chunk_Info {
  /** Information about chunks completed */
  Chnunk_Bitmap m_reserved_chunks;

  /** Information about unfinished chunks */
  Chunk_Map m_incomplete_chunks;

  /** Chunks for current state */
  uint32_t m_total_chunks;

  /** Minimum chunk number that is not reserved yet */
  uint32_t m_min_unres_chunk;

  /** Maximum chunk number that is already reserved */
  uint32_t m_max_res_chunk;

  /** Initialize Chunk number ranges */
  void init_chunk_nums() {
    m_min_unres_chunk = m_reserved_chunks.get_min_unset_bit();
    ut_ad(m_min_unres_chunk <= m_total_chunks + 1);

    m_max_res_chunk = m_reserved_chunks.get_max_set_bit();
    ut_ad(m_max_res_chunk <= m_total_chunks);
  }

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_chunk      serialized chunk info
  @param[in,out]        len             length of serialized descriptor */
  void serialize(byte *desc_chunk, uint &len);

  /** Deserialize the descriptor.
  @param[in]    desc_chunk      serialized chunk info
  @param[in,out]        len_left        length left in bytes */
  void deserialize(const byte *desc_chunk, uint &len_left);

  /** Get the length of serialized data
  @param[in]    num_tasks       number of tasks to include
  @return length serialized chunk info */
  size_t get_serialized_length(uint32_t num_tasks);
};

/** CLONE_DESC_LOCATOR: Descriptor for a task for clone operation.
A task is used by exactly one thread */
struct Clone_Desc_Locator {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Unique identifier for a clone operation. */
  uint64_t m_clone_id;

  /** Unique identifier for a clone snapshot. */
  uint64_t m_snapshot_id;

  /** Index in clone array for fast reference. */
  uint32_t m_clone_index;

  /** Current snapshot State */
  Snapshot_State m_state;

  /** Sub-state information: metadata transferred */
  bool m_metadata_transferred;

  /** Initialize clone locator.
  @param[in]    id      Clone identifier
  @param[in]    snap_id Snapshot identifier
  @param[in]    state   snapshot state
  @param[in]    version Descriptor version
  @param[in]    index   clone index */
  void init(uint64_t id, uint64_t snap_id, Snapshot_State state, uint version,
            uint index);

  /** Check if the passed locator matches the current one.
  @param[in]    other_desc      input locator descriptor
  @return true if matches */
  bool match(Clone_Desc_Locator *other_desc);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_loc        serialized descriptor
  @param[in,out]        len             length of serialized descriptor
  @param[in]    chunk_info      chunk information to serialize
  @param[in]    heap            heap for allocating memory */
  void serialize(byte *&desc_loc, uint &len, Chunk_Info *chunk_info,
                 mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]    desc_loc        serialized locator
  @param[in]    desc_len        locator length
  @param[in,out]        chunk_info      chunk information */
  void deserialize(const byte *desc_loc, uint desc_len, Chunk_Info *chunk_info);
};

/** CLONE_DESC_TASK_METADATA: Descriptor for a task for clone operation.
A task is used by exactly one thread */
struct Clone_Desc_Task_Meta {
  /** Descriptor header */
  Clone_Desc_Header m_header;

  /** Task information */
  Clone_Task_Meta m_task_meta;

  /** Initialize header
  @param[in]    version descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_task       serialized descriptor
  @param[in,out]        len             length of serialized descriptor
  @param[in]    heap            heap for allocating memory */
  void serialize(byte *&desc_task, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]    desc_task       serialized descriptor
  @param[in]    desc_len        descriptor length
  @return true, if successful. */
  bool deserialize(const byte *desc_task, uint desc_len);
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

  /** Number of estimated bytes to transfer */
  uint64_t m_estimate;

  /** Number of estimated bytes on disk */
  uint64_t m_estimate_disk;

  /** If start processing state */
  bool m_is_start;

  /** State transfer Acknowledgement */
  bool m_is_ack;

  /** Initialize header
  @param[in]    version descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_state      serialized descriptor
  @param[in,out]        len             length of serialized descriptor
  @param[in]    heap            heap for allocating memory */
  void serialize(byte *&desc_state, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]    desc_state      serialized descriptor
  @param[in]    desc_len        descriptor length
  @return true, if successful. */
  bool deserialize(const byte *desc_state, uint desc_len);
};

/** Clone file information */
struct Clone_File_Meta {
  /** Set file as deleted chunk.
  @param[in]    chunk   chunk number that is found deleted. */
  inline void set_deleted_chunk(uint32_t chunk) {
    m_begin_chunk = chunk;
    m_end_chunk = 0;
    m_deleted = true;
  }

  /** @return true, iff file is deleted. */
  bool is_deleted() const { return m_deleted; }

  /** @return true, iff file is deleted. */
  bool is_renamed() const { return m_renamed; }

  /** @return true, iff file is encrypted. */
  bool can_encrypt() const { return m_encryption_metadata.can_encrypt(); }

  /** Reset DDL state of file metadata. */
  void reset_ddl() {
    m_renamed = false;
    m_deleted = false;
  }

  /* Initialize parameters. */
  void init();

  /** File size in bytes */
  uint64_t m_file_size;

  /** File allocation size on disk for sparse files. */
  uint64_t m_alloc_size;

  /** Tablespace FSP flags */
  uint32_t m_fsp_flags;

  /** File compression type */
  Compression::Type m_compress_type;

  /** If transparent compression is needed. It is derived information
  and is not transferred. */
  bool m_punch_hole;

  /* Set file metadata as deleted. */
  bool m_deleted;

  /* Set file metadata as renamed. */
  bool m_renamed;

  /* Contains encryption key to be transferred. */
  bool m_transfer_encryption_key;

  /** File system block size. */
  size_t m_fsblk_size;

  /** Tablespace ID for the file */
  space_id_t m_space_id;

  /** File index in clone data file vector */
  uint m_file_index;

  /** Chunk number for the first chunk in file */
  uint m_begin_chunk;

  /** Chunk number for the last chunk in file */
  uint m_end_chunk;

  /** File name length in bytes */
  size_t m_file_name_len;

  /** Allocation length of name buffer. */
  size_t m_file_name_alloc_len;

  /** File name */
  const char *m_file_name;

  /** Encryption metadata. */
  Encryption_metadata m_encryption_metadata;
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
  @param[in]    version descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_file       serialized descriptor
  @param[in,out]        len             length of serialized descriptor
  @param[in]    heap            heap for allocating memory */
  void serialize(byte *&desc_file, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]    desc_file       serialized descriptor
  @param[in]    desc_len        descriptor length
  @return true, if successful. */
  bool deserialize(const byte *desc_file, uint desc_len);
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
  @param[in]    version descriptor version */
  void init_header(uint version);

  /** Serialize the descriptor. Caller should pass
  the length if allocated.
  @param[out]   desc_data       serialized descriptor
  @param[in,out]        len             length of serialized descriptor
  @param[in]    heap            heap for allocating memory */
  void serialize(byte *&desc_data, uint &len, mem_heap_t *heap);

  /** Deserialize the descriptor.
  @param[in]    desc_data       serialized descriptor
  @param[in]    desc_len        descriptor length
  @return true, if successful. */
  bool deserialize(const byte *desc_data, uint desc_len);
};

#endif /* CLONE_DESC_INCLUDE */
