#ifndef SQL_SORT_INCLUDED
#define SQL_SORT_INCLUDED

<<<<<<< HEAD
/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include "map_helpers.h"
#include "my_base.h"  // ha_rows

#include "my_sys.h"
#include "sql/filesort_utils.h"  // Filesort_buffer
#include "sql/mem_root_array.h"

class Addon_fields;
struct TABLE;

/* Defines used by filesort and uniques */

constexpr size_t MERGEBUFF = 7;
constexpr size_t MERGEBUFF2 = 15;
// Number of bytes used to store varlen key's length
constexpr size_t VARLEN_PREFIX = 4;

/**
  Descriptor for a merge chunk to be sort-merged.
  A merge chunk is a sequence of pre-sorted records, written to a
  temporary file. A Merge_chunk instance describes where this chunk is stored
  in the file, and where it is located when it is in memory.

  It is a POD because we read/write them from/to files (but note,
  only m_file_position and m_rowcount are actually used in that
  situation).

  We have accessors (getters/setters) for all struct members.
 */
struct Merge_chunk {
 public:
  my_off_t file_position() const { return m_file_position; }
  void set_file_position(my_off_t val) { m_file_position = val; }
  void advance_file_position(my_off_t val) { m_file_position += val; }

  uchar *buffer_start() { return m_buffer_start; }
  const uchar *buffer_end() const { return m_buffer_end; }
  const uchar *valid_buffer_end() const { return m_valid_buffer_end; }

  void set_buffer(uchar *start, uchar *end) {
    m_buffer_start = start;
    m_buffer_end = end;
  }
<<<<<<< HEAD
  void set_buffer_start(uchar *start) { m_buffer_start = start; }
  void set_buffer_end(uchar *end) {
    assert(m_buffer_end == nullptr || end <= m_buffer_end);
    m_buffer_end = end;
=======
  void set_buffer_start(uchar *start)
  {
    m_buffer_start= start;
  }
  void set_buffer_end(uchar *end)
  {
    assert(m_buffer_end == NULL || end <= m_buffer_end);
    m_buffer_end= end;
>>>>>>> upstream/cluster-7.6
  }
  void set_valid_buffer_end(uchar *end) {
    assert(end <= m_buffer_end);
    m_valid_buffer_end = end;
  }

  void init_current_key() { m_current_key = m_buffer_start; }
  uchar *current_key() const { return m_current_key; }
  void advance_current_key(uint val) { m_current_key += val; }

  void decrement_rowcount(ha_rows val) { m_rowcount -= val; }
  void set_rowcount(ha_rows val) { m_rowcount = val; }
  ha_rows rowcount() const { return m_rowcount; }

  ha_rows mem_count() const { return m_mem_count; }
  void set_mem_count(ha_rows val) { m_mem_count = val; }
  ha_rows decrement_mem_count() { return --m_mem_count; }

  ha_rows max_keys() const { return m_max_keys; }
  void set_max_keys(ha_rows val) { m_max_keys = val; }

  size_t buffer_size() const { return m_buffer_end - m_buffer_start; }

  /**
    Tries to merge *this with *mc, returns true if successful.
    The assumption is that *this is no longer in use,
    and the space it has been allocated can be handed over to a
    buffer which is adjacent to it.
   */
  bool merge_freed_buff(Merge_chunk *mc) const {
    if (mc->m_buffer_end == m_buffer_start) {
      mc->m_buffer_end = m_buffer_end;
      mc->m_max_keys += m_max_keys;
      return true;
    } else if (mc->m_buffer_start == m_buffer_end) {
      mc->m_buffer_start = m_buffer_start;
      mc->m_max_keys += m_max_keys;
      return true;
    }
    return false;
  }

 private:
  /// The current key for this chunk.
  uchar *m_current_key = nullptr;

  /// Current position in the file to be sorted.
  my_off_t m_file_position = 0;

  /// Start of main-memory buffer for this chunk.
  uchar *m_buffer_start = nullptr;

  /// End of main-memory buffer for this chunk.
  uchar *m_buffer_end = nullptr;

  /// End of actual, valid data for this chunk.
  uchar *m_valid_buffer_end;

  /// Number of unread rows in this chunk.
  ha_rows m_rowcount = 0;

  /// Number of rows in the main-memory buffer.
  ha_rows m_mem_count = 0;

  /// If we have fixed-size rows: max number of rows in buffer.
  ha_rows m_max_keys = 0;
};

typedef Bounds_checked_array<Merge_chunk> Merge_chunk_array;

<<<<<<< HEAD
/*
  The result of Unique or filesort; can either be stored on disk
  (in which case io_cache points to the file) or in memory in one
  of two ways. See sorted_result_in_fsbuf.

  Note if sort_result points into memory, it does _not_ own the sort buffer;
  Filesort_info does.

  TODO: Clean up so that Filesort / Filesort_info / Filesort_buffer /
  Sort_result have less confusing overlap.
*/
class Sort_result {
 public:
  Sort_result() : sorted_result_in_fsbuf(false), sorted_result_end(nullptr) {}

  bool has_result_in_memory() const {
=======
/**
  This class wraps information about usage of addon fields.
  An Addon_fields object is used both during packing of data in the filesort
  buffer, and later during unpacking in 'Filesort_info::unpack_addon_fields'.
  
  @see documentation for the Sort_addon_field struct.
  @see documentation for get_addon_fields()
 */
class Addon_fields {
public:
  Addon_fields(Addon_fields_array arr)
    : m_field_descriptors(arr),
      m_addon_buf(NULL),
      m_addon_buf_length(0),
      m_using_packed_addons(false)
  {
    assert(!arr.is_null());
  }

  Sort_addon_field *begin() { return m_field_descriptors.begin(); }
  Sort_addon_field *end()   { return m_field_descriptors.end(); }
  size_t num_field_descriptors() const { return m_field_descriptors.size(); }

  /// rr_unpack_from_tempfile needs an extra buffer when unpacking.
  uchar *allocate_addon_buf(uint sz)
  {
    if (m_addon_buf != NULL)
    {
      assert(m_addon_buf_length == sz);
      return m_addon_buf;
    }
    m_addon_buf= static_cast<uchar*>(sql_alloc(sz));
    if (m_addon_buf)
      m_addon_buf_length= sz;
    return m_addon_buf;
  }

  uchar *get_addon_buf()              { return m_addon_buf; }
  uint   get_addon_buf_length() const { return m_addon_buf_length; }

  void set_using_packed_addons(bool val)
  {
    m_using_packed_addons= val;
  }

  bool using_packed_addons() const
  {
    return m_using_packed_addons;
  }

  static bool can_pack_addon_fields(uint record_length)
  {
    return (record_length <= (0xFFFF));
  }

  /**
    @returns Total number of bytes used for packed addon fields.
    the size of the length field + size of null bits + sum of field sizes.
   */
  static uint read_addon_length(uchar *p)
  {
    return size_of_length_field + uint2korr(p);
  }

  /**
    Stores the number of bytes used for packed addon fields.
   */
  static void store_addon_length(uchar *p, uint sz)
  {
    // We actually store the length of everything *after* the length field.
    int2store(p, sz - size_of_length_field);
  }

  static const uint size_of_length_field= 2;

private:
  Addon_fields_array m_field_descriptors;

  uchar    *m_addon_buf;            ///< Buffer for unpacking addon fields.
  uint      m_addon_buf_length;     ///< Length of the buffer.
  bool      m_using_packed_addons;  ///< Are we packing the addon fields?
};

/**
  There are two record formats for sorting:
    |<key a><key b>...|<rowid>|
    /  sort_length    / ref_l /

  or with "addon fields"
    |<key a><key b>...|<null bits>|<field a><field b>...|
    /  sort_length    /         addon_length            /

  The packed format for "addon fields"
    |<key a><key b>...|<length>|<null bits>|<field a><field b>...|
    /  sort_length    /         addon_length                     /

  <key>       Fields are fixed-size, specially encoded with
              Field::make_sort_key() so we can do byte-by-byte compare.
  <length>    Contains the *actual* packed length (after packing) of
              everything after the sort keys.
              The size of the length field is 2 bytes,
              which should cover most use cases: addon data <= 65535 bytes.
              This is the same as max record size in MySQL.
  <null bits> One bit for each nullable field, indicating whether the field
              is null or not. May have size zero if no fields are nullable.
  <field xx>  Are stored with field->pack(), and retrieved with field->unpack().
              Addon fields within a record are stored consecutively, with no
              "holes" or padding. They will have zero size for NULL values.

 */
class Sort_param {
public:
  uint rec_length;            // Length of sorted records.
  uint sort_length;           // Length of sorted columns.
  uint ref_length;            // Length of record ref.
  uint addon_length;          // Length of added packed fields.
  uint res_length;            // Length of records in final sorted file/buffer.
  uint max_keys_per_buffer;   // Max keys / buffer.
  ha_rows max_rows;           // Select limit, or HA_POS_ERROR if unlimited.
  ha_rows examined_rows;      // Number of examined rows.
  TABLE *sort_form;           // For quicker make_sortkey.
  bool use_hash;              // Whether to use hash to distinguish cut JSON

  /**
    ORDER BY list with some precalculated info for filesort.
    Array is created and owned by a Filesort instance.
   */
  Bounds_checked_array<st_sort_field> local_sortorder;

  Addon_fields *addon_fields; ///< Descriptors for addon fields.
  uchar *unique_buff;
  bool not_killable;
  bool using_pq;
  char* tmp_buffer;

  // The fields below are used only by Unique class.
  Merge_chunk_compare_context cmp_context;
  typedef int (*chunk_compare_fun)(Merge_chunk_compare_context* ctx,
                                   uchar* arg1, uchar* arg2);
  chunk_compare_fun compare;

  Sort_param()
  {
    memset(this, 0, sizeof(*this));
  }
  /**
    Initialize this struct for filesort() usage.
    @see description of record layout above.
    @param [in,out] file_sort Sorting information which may be re-used on
                              subsequent invocations of filesort().
    @param sortlen   Length of sorted columns.
    @param table     Table to be sorted.
    @param max_length_for_sort_data From thd->variables.
    @param maxrows   HA_POS_ERROR or possible LIMIT value.
    @param sort_positions @see documentation for the filesort() function.
  */
  void init_for_filesort(Filesort *file_sort,
                         uint sortlen, TABLE *table,
                         ulong max_length_for_sort_data,
                         ha_rows maxrows, bool sort_positions);

  /// Enables the packing of addons if possible.
  void try_to_pack_addons(ulong max_length_for_sort_data);

  /// Are we packing the "addon fields"?
  bool using_packed_addons() const
  {
    assert(m_using_packed_addons ==
           (addon_fields != NULL && addon_fields->using_packed_addons()));
    return m_using_packed_addons;
  }

  /// Are we using "addon fields"?
  bool using_addon_fields() const
  {
    return addon_fields != NULL;
  }

  /**
    Stores key fields in *to.
    Then appends either *ref_pos (the <rowid>) or the "addon fields".
    @param  to      out Where to store the result
    @param  ref_pos in  Where to find the <rowid>
    @returns Number of bytes stored.
   */
  uint make_sortkey(uchar *to, const uchar *ref_pos);

  /// @returns The number of bytes used for sorting.
  size_t compare_length() const {
    return sort_length;
  }

  /**
    Getter for record length and result length.
    @param record_start Pointer to record.
    @param [out] recl   Store record length here.
    @param [out] resl   Store result length here.
   */
  void get_rec_and_res_len(uchar *record_start, uint *recl, uint *resl)
  {
    if (!using_packed_addons())
    {
      *recl= rec_length;
      *resl= res_length;
      return;
    }
    uchar *plen= record_start + sort_length;
    *resl= Addon_fields::read_addon_length(plen);
    assert(*resl <= res_length);
    const uchar *record_end= plen + *resl;
    *recl= static_cast<uint>(record_end - record_start);
  }

private:
  uint m_packable_length;     ///< total length of fields which have a packable type
  bool m_using_packed_addons; ///< caches the value of using_packed_addons()

  // Not copyable.
  Sort_param(const Sort_param&);
  Sort_param &operator=(const Sort_param&);
};


/**
  A class wrapping misc buffers used for sorting.
  It is part of 'struct TABLE' which is still initialized using memset(),
  so do not add any virtual functions to this class.
 */
class Filesort_info
{
  /// Buffer for sorting keys.
  Filesort_buffer filesort_buffer;

public:
  IO_CACHE *io_cache;             ///< If sorted through filesort
  Merge_chunk_array merge_chunks; ///< Array of chunk descriptors

  Addon_fields *addon_fields;     ///< Addon field descriptors.

  /**
    If the entire result of filesort fits in memory, we skip the merge phase.
    We may leave the result in filesort_buffer
    (indicated by sorted_result_in_fsbuf), or we may strip away
    the sort keys, and copy the sorted result into a new buffer.
    This new buffer is [sorted_result ... sorted_result_end]
    @see save_index()
   */
  bool      sorted_result_in_fsbuf;
  uchar     *sorted_result;
  uchar     *sorted_result_end;

  ha_rows   found_records;        ///< How many records in sort.

  // Note that we use the default copy CTOR / assignment operator in filesort().
  Filesort_info()
    : sorted_result_in_fsbuf(false),
      sorted_result(NULL), sorted_result_end(NULL)
  {};

  bool has_filesort_result_in_memory() const
  {
>>>>>>> upstream/cluster-7.6
    return sorted_result || sorted_result_in_fsbuf;
  }

  bool has_result() const {
    return has_result_in_memory() || (io_cache && my_b_inited(io_cache));
  }

  IO_CACHE *io_cache{nullptr};

  /**
    If the entire result fits in memory, we skip the merge phase.
    We may leave the result in the parent Filesort_info's filesort_buffer
    (indicated by sorted_result_in_fsbuf), or we may strip away
    the sort keys, and copy the sorted result into a new buffer.
    Unique always uses the latter.
    This new buffer is [sorted_result ... sorted_result_end]
    @see save_index()
   */
  bool sorted_result_in_fsbuf{false};
  unique_ptr_my_free<uchar> sorted_result{nullptr};
  uchar *sorted_result_end{nullptr};

  ha_rows found_records{0};  ///< How many records in sort.
};

/**
  A class wrapping misc buffers used for sorting.
 */
class Filesort_info {
  /// Buffer for sorting keys.
  Filesort_buffer filesort_buffer;

 public:
  Merge_chunk_array merge_chunks;  ///< Array of chunk descriptors

  Addon_fields *addon_fields{nullptr};  ///< Addon field descriptors.

  bool m_using_varlen_keys{false};
  uint m_sort_length{0};

  Filesort_info(const Filesort_info &) = delete;
  Filesort_info &operator=(const Filesort_info &) = delete;

  Filesort_info() : m_using_varlen_keys(false), m_sort_length(0) {}

  /** Sort filesort_buffer
    @return Number of records, after any deduplication
   */
  size_t sort_buffer(Sort_param *param, size_t num_input_rows,
                     size_t max_output_rows) {
    return filesort_buffer.sort_buffer(param, num_input_rows, max_output_rows);
  }

  /**
    Copies (unpacks) values appended to sorted fields from a buffer back to
    their regular positions specified by the Field::ptr pointers.
    @param tables  Tables in the join; for NULL row flags.
    @param buff    Buffer which to unpack the value from.
  */
  template <bool Packed_addon_fields>
  inline void unpack_addon_fields(const Mem_root_array<TABLE *> &tables,
                                  uchar *buff);

  /**
    Reads 'count' number of chunk descriptors into the merge_chunks array.
    In case of error, the merge_chunks array will be empty.
    @param chunk_file File containing the descriptors.
    @param count      Number of chunks to read.
  */
  void read_chunk_descriptors(IO_CACHE *chunk_file, uint count);

  /// Are we using "addon fields"?
  bool using_addon_fields() const { return addon_fields != nullptr; }

  void reset() { filesort_buffer.reset(); }

  void clear_peak_memory_used() { filesort_buffer.clear_peak_memory_used(); }

  Bounds_checked_array<uchar> get_next_record_pointer(size_t min_size) {
    return filesort_buffer.get_next_record_pointer(min_size);
  }

  void commit_used_memory(size_t num_bytes) {
    filesort_buffer.commit_used_memory(num_bytes);
  }

  uchar *get_sorted_record(uint idx) {
    return filesort_buffer.get_sorted_record(idx);
  }

  uchar **get_sort_keys() { return filesort_buffer.get_sort_keys(); }

  Bounds_checked_array<uchar> get_contiguous_buffer() {
    return filesort_buffer.get_contiguous_buffer();
  }

  void set_max_size(size_t max_size, size_t record_length) {
    filesort_buffer.set_max_size(max_size, record_length);
  }

  void free_sort_buffer() { filesort_buffer.free_sort_buffer(); }

  bool preallocate_records(size_t num_records) {
    return filesort_buffer.preallocate_records(num_records);
  }

  size_t peak_memory_used() const { return filesort_buffer.peak_memory_used(); }

  size_t max_size_in_bytes() const {
    return filesort_buffer.max_size_in_bytes();
  }

  uint sort_length() const { return m_sort_length; }
  bool using_varlen_keys() const { return m_using_varlen_keys; }

  void set_sort_length(uint val, bool is_varlen) {
    m_sort_length = val;
    m_using_varlen_keys = is_varlen;
  }
};

typedef Bounds_checked_array<uchar> Sort_buffer;

/**
  Put all room used by freed buffer to use in adjacent buffer.

  Note, that we can't simply distribute memory evenly between all buffers,
  because new areas must not overlap with old ones.
*/
template <typename Heap_type>
void reuse_freed_buff(Merge_chunk *old_top, Heap_type *heap) {
  typename Heap_type::iterator it = heap->begin();
  typename Heap_type::iterator end = heap->end();
  for (; it != end; ++it) {
    if (old_top->merge_freed_buff(*it)) return;
  }
  assert(0);
}

#endif /* SQL_SORT_INCLUDED */
