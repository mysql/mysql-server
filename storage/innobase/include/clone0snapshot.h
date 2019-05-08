/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/clone0snapshot.h
 Database Physical Snapshot

 *******************************************************/

#ifndef CLONE_SNAPSHOT_INCLUDE
#define CLONE_SNAPSHOT_INCLUDE

#include "univ.i"

#include "arch0log.h"
#include "arch0page.h"
#include "clone0desc.h"
#include "clone0monitor.h"
#include "fil0fil.h"
#include "sql/handler.h"

#include <map>
#include <vector>

/** Vector type for storing clone files */
using Clone_File_Vec = std::vector<Clone_File_Meta *>;

/** Map type for mapping space ID to clone file index */
using Clone_File_Map = std::map<space_id_t, uint>;

/** Page identified by space and page number */
struct Clone_Page {
  /** Tablespace ID */
  ib_uint32_t m_space_id;

  /** Page number within tablespace */
  ib_uint32_t m_page_no;
};

/** Comparator for storing sorted page ID. */
struct Less_Clone_Page {
  /** Less than operator for page ID.
  @param[in]	page1	first page
  @param[in]	page2	second page
  @return true, if page1 is less than page2 */
  inline bool operator()(const Clone_Page &page1,
                         const Clone_Page &page2) const {
    if (page1.m_space_id < page2.m_space_id) {
      return (true);
    }

    if (page1.m_space_id == page2.m_space_id &&
        page1.m_page_no < page2.m_page_no) {
      return (true);
    }
    return (false);
  }
};

/** Vector type for storing clone page IDs */
using Clone_Page_Vec = std::vector<Clone_Page>;

/** Set for storing unique page IDs. */
using Clone_Page_Set = std::set<Clone_Page, Less_Clone_Page>;

/** Clone handle type */
enum Clone_Handle_Type {
  /** Clone Handle for COPY */
  CLONE_HDL_COPY = 1,

  /** Clone Handle for APPLY */
  CLONE_HDL_APPLY
};

/** Default chunk size in power of 2 in unit of pages.
Chunks are reserved by each thread for multi-threaded clone. For 16k page
size, chunk size is 64M. */
const uint SNAPSHOT_DEF_CHUNK_SIZE_POW2 = 12;

/** Default block size in power of 2 in unit of pages.
Data transfer callback is invoked once for each block. This is also
the maximum size of data that would be re-send if clone is stopped
and resumed. For 16k page size, block size is 1M. */
const uint SNAPSHOT_DEF_BLOCK_SIZE_POW2 = 6;

/** Maximum block size in power of 2 in unit of pages.
For 16k page size, maximum block size is 64M. */
const uint SNAPSHOT_MAX_BLOCK_SIZE_POW2 = 12;

/** Sleep time in microseconds while waiting for other clone/task */
const uint SNAPSHOT_STATE_CHANGE_SLEEP = 100 * 1000;

/** Dynamic database snapshot: Holds metadata and handle to data */
class Clone_Snapshot {
 public:
  /** Construct snapshot
  @param[in]	hdl_type	copy, apply
  @param[in]	clone_type	clone type
  @param[in]	arr_idx		index in global array
  @param[in]	snap_id		unique snapshot ID */
  Clone_Snapshot(Clone_Handle_Type hdl_type, Ha_clone_type clone_type,
                 uint arr_idx, ib_uint64_t snap_id);

  /** Release contexts and free heap */
  ~Clone_Snapshot();

  /** Get unique snapshot identifier
  @return snapshot ID */
  ib_uint64_t get_id() { return (m_snapshot_id); }

  /** Get snapshot index in global array
  @return array index */
  uint get_index() { return (m_snapshot_arr_idx); }

  /** Get performance schema accounting object used to monitor stage
  progress.
  @return PFS stage object */
  Clone_Monitor &get_clone_monitor() { return (m_monitor); }

  /** Get snapshot heap used for allocation during clone.
  @return heap */
  mem_heap_t *lock_heap() {
    mutex_enter(&m_snapshot_mutex);
    return (m_snapshot_heap);
  }

  /* Release snapshot heap */
  void release_heap(mem_heap_t *&heap) {
    heap = nullptr;
    mutex_exit(&m_snapshot_mutex);
  }

  /** Get snapshot state
  @return state */
  Snapshot_State get_state() { return (m_snapshot_state); }

  /** Get the redo file size for the snapshot
  @return redo file size */
  ib_uint64_t get_redo_file_size() { return (m_redo_file_size); }

  /** Get total number of chunks for current state
  @return number of data chunks */
  uint get_num_chunks() { return (m_num_current_chunks); }

  /** Get maximum file length seen till now
  @return file name length */
  size_t get_max_file_name_length() { return (m_max_file_name_len); }

  /** Get maximum buffer size required for clone
  @return maximum dynamic buffer */
  uint get_dyn_buffer_length() {
    uint ret_len = 0;

    if (is_copy() && m_snapshot_type != HA_CLONE_BLOCKING) {
      ret_len = static_cast<uint>(2 * UNIV_PAGE_SIZE);
    }

    return (ret_len);
  }

  using File_Cbk_Func = std::function<int(Clone_File_Meta *)>;

  /** Iterate through all files in current state
  @param[in]	func	callback function
  @return error code */
  int iterate_files(File_Cbk_Func &&func);

  /** Fill state descriptor from snapshot
  @param[in]	do_estimate	estimate data bytes to transfer
  @param[out]	state_desc	snapshot state descriptor */
  void get_state_info(bool do_estimate, Clone_Desc_State *state_desc);

  /** Set state information during apply
  @param[in]	state_desc	snapshot state descriptor */
  void set_state_info(Clone_Desc_State *state_desc);

  /** Get next state based on snapshot type
  @return next state */
  Snapshot_State get_next_state();

  /** Try to attach to snapshot
  @param[in]	hdl_type	copy, apply
  @param[in]	pfs_monitor	enable PFS monitoring
  @return true if successfully attached */
  bool attach(Clone_Handle_Type hdl_type, bool pfs_monitor);

  /** Detach from snapshot
  @return number of clones attached */
  uint detach();

  /** Start transition to new state
  @param[in]	state_desc	descriptor for next state
  @param[in]	new_state	state to move for apply
  @param[in]	temp_buffer	buffer used for collecting page IDs
  @param[in]	temp_buffer_len	buffer length
  @param[out]	pending_clones	clones yet to transit to next state
  @return error code */
  int change_state(Clone_Desc_State *state_desc, Snapshot_State new_state,
                   byte *temp_buffer, uint temp_buffer_len,
                   uint &pending_clones);

  /** Check if transition is complete
  @param[in]	new_state	new state after transition
  @param[in]	exit_on_wait	exit from transition if needs to wait
  @return number of clones yet to transit to next state */
  uint check_state(Snapshot_State new_state, bool exit_on_wait);

  /* Don't allow to attach new clone - Not supported
  void stop_attach_new_clone()
  {
          m_allow_new_clone = false;
  }
  */

  /** Add file metadata entry at destination
  @param[in,out]	file_desc	if there, set to current descriptor
  @param[in]	data_dir	destination data directory
  @param[in]	desc_create	create if doesn't exist
  @param[out]	desc_exists	descriptor already exists
  @return error code */
  int get_file_from_desc(Clone_File_Meta *&file_desc, const char *data_dir,
                         bool desc_create, bool &desc_exists);

  /** Add file descriptor to file list
  @param[in,out]	file_desc	current file descriptor
  @return true, if it is the last file. */
  bool add_file_from_desc(Clone_File_Meta *&file_desc);

  /** Extract file information from node and add to snapshot
  @param[in]	node	file node
  @return error code */
  dberr_t add_node(fil_node_t *node);

  /** Add page ID to to the set of pages in snapshot
  @param[in]	space_id	page tablespace
  @param[in]	page_num	page number within tablespace
  @return error code */
  int add_page(ib_uint32_t space_id, ib_uint32_t page_num);

  /** Add redo file to snapshot
  @param[in]	file_name	file name
  @param[in]	file_size	file size in bytes
  @param[in]	file_offset	start offset
  @return error code. */
  int add_redo_file(char *file_name, ib_uint64_t file_size,
                    ib_uint64_t file_offset);

  /** Get file metadata by index for current state
  @param[in]	index	file index
  @return file metadata entry */
  Clone_File_Meta *get_file_by_index(uint index);

  /** Get next block of data to transfer
  @param[in]	chunk_num	current chunk
  @param[in,out]	block_num	current/next block
  @param[in,out]	file_meta	current/next block file metadata
  @param[out]	data_offset	block offset in file
  @param[out]	data_buf	data buffer or NULL if transfer from file
  @param[out]	data_size	size of data in bytes
  @return error code */
  int get_next_block(uint chunk_num, uint &block_num,
                     Clone_File_Meta *file_meta, ib_uint64_t &data_offset,
                     byte *&data_buf, uint &data_size);

  /** Update snapshot block size based on caller's buffer size
  @param[in]	buff_size	buffer size for clone transfer */
  void update_block_size(uint buff_size);

  /** Check if copy snapshot
  @return true if snapshot is for copy */
  bool is_copy() const { return (m_snapshot_handle_type == CLONE_HDL_COPY); }

  /** Update file size when file is extended during page copy
  @param[in]	file_index	current file index
  @param[in]	file_size	new file size */
  void update_file_size(uint32_t file_index, uint64_t file_size);

 private:
  /** Check if state transition is in progress
  @return true during state transition */
  bool in_transit_state() {
    mutex_own(&m_snapshot_mutex);
    return (m_snapshot_next_state != CLONE_SNAPSHOT_NONE);
  }

  /** Initialize current state
  @param[in]	state_desc	descriptor for the state
  @param[in]	temp_buffer	buffer used during page copy initialize
  @param[in]	temp_buffer_len	buffer length
  @return error code */
  int init_state(Clone_Desc_State *state_desc, byte *temp_buffer,
                 uint temp_buffer_len);

  /** Initialize snapshot state for file copy
  @return error code */
  int init_file_copy();

  /** Initialize snapshot state for page copy
  @param[in]	page_buffer	temporary buffer to copy page IDs
  @param[in]	page_buffer_len	buffer length
  @return error code */
  int init_page_copy(byte *page_buffer, uint page_buffer_len);

  /** Initialize snapshot state for redo copy
  @return error code */
  int init_redo_copy();

  /** Initialize state while applying cloned data
  @param[in]	state_desc	snapshot state descriptor
  @return error code */
  int init_apply_state(Clone_Desc_State *state_desc);

  /** Extend and flush files after copying data
  @param[in]	is_redo	if true flush redo, otherwise data
  @return error code */
  int extend_and_flush_files(bool is_redo);

  /** Create file descriptor and add to current file list
  @param[in]	data_dir	destination data directory
  @param[in,out]	file_desc	file descriptor
  @return error code */
  int create_desc(const char *data_dir, Clone_File_Meta *&file_desc);

  /** Get file metadata for current chunk
  @param[in]	file_vector	clone file vector
  @param[in]	num_files	total number of files
  @param[in]	chunk_num	current chunk number
  @param[in]	start_index	index for starting the search
  @return file metadata */
  Clone_File_Meta *get_file(Clone_File_Vec &file_vector, uint num_files,
                            uint chunk_num, uint start_index);

  /** Get next page from buffer pool
  @param[in]	chunk_num	current chunk
  @param[in,out]	block_num	current, next block
  @param[in]	file_meta	file metadata for page
  @param[out]	data_offset	offset in file
  @param[out]	data_buf	page data
  @param[out]	data_size	page data size
  @return error code */
  int get_next_page(uint chunk_num, uint &block_num, Clone_File_Meta *file_meta,
                    ib_uint64_t &data_offset, byte *&data_buf, uint &data_size);

  /** Get page from buffer pool and make ready for write
  @param[in]	page_id		page ID chunk
  @param[in]	page_size	page size descriptor
  @param[out]	page_data	data page
  @param[out]	data_size	page size in bytes
  @return error code */
  int get_page_for_write(const page_id_t &page_id, const page_size_t &page_size,
                         byte *&page_data, uint &data_size);

  /** Build file metadata entry
  @param[in]	file_name	name of the file
  @param[in]	file_size	file size in bytes
  @param[in]	file_offset	start offset
  @param[in]	num_chunks	total number of chunks in the file
  @param[in]	copy_file_name	copy the file name or use reference
  @return file metadata entry */
  Clone_File_Meta *build_file(const char *file_name, ib_uint64_t file_size,
                              ib_uint64_t file_offset, uint &num_chunks,
                              bool copy_file_name);

  /** Add buffer pool dump file to the file list
  @return error code */
  int add_buf_pool_file();

  /** Add file to snapshot
  @param[in]	name		file name
  @param[in]	size_bytes	file size in bytes
  @param[in]	space_id	tablespace id
  @param[in]	copy_name	copy the file name or use reference
  @return error code. */
  int add_file(const char *name, ib_uint64_t size_bytes, ulint space_id,
               bool copy_name);

  /** Get chunk size
  @return chunk size in pages */
  uint chunk_size() {
    uint size;

    size = static_cast<uint>(ut_2_exp(m_chunk_size_pow2));
    return (size);
  }

  /** Get block size
  @return block size in pages */
  uint block_size() {
    uint size;

    ut_a(m_block_size_pow2 <= SNAPSHOT_MAX_BLOCK_SIZE_POW2);
    size = static_cast<uint>(ut_2_exp(m_block_size_pow2));

    return (size);
  }

  /** Get number of blocks per chunk
  @return blocks per chunk */
  uint blocks_per_chunk() {
    ut_a(m_block_size_pow2 <= m_chunk_size_pow2);
    return (1 << (m_chunk_size_pow2 - m_block_size_pow2));
  }

 private:
  /** @name Snapshot type and ID */

  /** Snapshot handle type */
  Clone_Handle_Type m_snapshot_handle_type;

  /** Clone type */
  Ha_clone_type m_snapshot_type;

  /** Unique snapshot ID */
  ib_uint64_t m_snapshot_id;

  /** Index in global snapshot array */
  uint m_snapshot_arr_idx;

  /** @name Snapshot State  */

  /** Mutex to handle access by concurrent clones */
  ib_mutex_t m_snapshot_mutex;

  /** Allow new clones to get attached to this snapshot */
  bool m_allow_new_clone;

  /** Number of clones attached to this snapshot */
  uint m_num_clones;

  /** Number of clones in current state */
  uint m_num_clones_current;

  /** Number of clones moved over to next state */
  uint m_num_clones_next;

  /** Current state */
  Snapshot_State m_snapshot_state;

  /** Next state to move to. Set only during state transfer. */
  Snapshot_State m_snapshot_next_state;

  /** @name Snapshot data block */

  /** Memory allocation heap */
  mem_heap_t *m_snapshot_heap;

  /** Chunk size in power of 2 */
  uint m_chunk_size_pow2;

  /** Block size in power of 2 */
  uint m_block_size_pow2;

  /** Number of chunks in current state */
  uint m_num_current_chunks;

  /** Maximum file name length observed till now. */
  size_t m_max_file_name_len;

  /** @name Snapshot file data */

  /** All data files for transfer */
  Clone_File_Vec m_data_file_vector;

  /** Map space ID to file vector index */
  Clone_File_Map m_data_file_map;

  /** Number of data files to transfer */
  uint m_num_data_files;

  /** Total number of data chunks */
  uint m_num_data_chunks;

  /** @name Snapshot page data */

  /** Page archiver client */
  Page_Arch_Client_Ctx m_page_ctx;

  /** Set of unique page IDs */
  Clone_Page_Set m_page_set;

  /** Sorted page IDs to transfer */
  Clone_Page_Vec m_page_vector;

  /** Number of pages to transfer */
  uint m_num_pages;

  /** Number of duplicate pages found */
  uint m_num_duplicate_pages;

  /** @name Snapshot redo data */

  /** redo log archiver client */
  Log_Arch_Client_Ctx m_redo_ctx;

  /** All archived redo files to transfer */
  Clone_File_Vec m_redo_file_vector;

  /** Start offset in first redo file */
  ib_uint64_t m_redo_start_offset;

  /** Redo header block */
  byte *m_redo_header;

  /** Redo header size */
  uint m_redo_header_size;

  /** Redo trailer block */
  byte *m_redo_trailer;

  /** Redo trailer size */
  uint m_redo_trailer_size;

  /** Redo trailer block offset */
  ib_uint64_t m_redo_trailer_offset;

  /** Archived redo file size */
  ib_uint64_t m_redo_file_size;

  /** Number of archived redo files to transfer */
  uint m_num_redo_files;

  /** Total number of redo data chunks */
  uint m_num_redo_chunks;

  /** Enable PFS monitoring */
  bool m_enable_pfs;

  /** Performance Schema accounting object to monitor stage progess */
  Clone_Monitor m_monitor;
};

#endif /* CLONE_SNAPSHOT_INCLUDE */
