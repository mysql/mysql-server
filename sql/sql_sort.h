#ifndef SQL_SORT_INCLUDED
#define SQL_SORT_INCLUDED

/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_global.h"

#include "my_base.h"          // ha_rows
#include "my_byteorder.h"     // uint2korr
#include "my_sys.h"           // qsort2_cmp
#include "mysql_com.h"        // Item_result
#include "binary_log_types.h" // enum_field_types
#include "filesort_utils.h"   // Filesort_buffer
#include "sql_array.h"        // Bounds_checked_arary
#include "thr_malloc.h"       // sql_alloc

class Field;
class Item;
struct TABLE;
class Filesort;

/* Defines used by filesort and uniques */

constexpr size_t MERGEBUFF= 7;
constexpr size_t MERGEBUFF2= 15;
// Number of bytes used to store varlen key's length
constexpr size_t VARLEN_PREFIX= 4;

/* Structs used when sorting */

/// Struct that holds information about a sort field.
struct st_sort_field {
  Field *field;                  ///< Field to sort
  Item  *item;                   ///< Item if not sorting fields
  uint  length;                  ///< Length of sort field
  uint  suffix_length;           ///< Length suffix (0-4)
  Item_result result_type;       ///< Type of item
  enum_field_types field_type;   ///< Field type of the field or item
  bool reverse;                  ///< if descending sort
  bool need_strxnfrm;            ///< If we have to use strxnfrm()
  bool is_varlen;                ///< If key part has variable length
  bool maybe_null;               ///< If key part is nullable
};


/**
  The structure Sort_addon_field describes the layout
  for field values appended to sorted values in records to be sorted
  in the sort buffer.

  Null bit maps for the appended values is placed before the values 
  themselves. Offsets are from the last sorted field.

  The structure is used to store values of the additional fields 
  in the sort buffer. It is used also when these values are read
  from a temporary file/buffer in 'Filesort_info::unpack_addon_fields'.
*/

struct Sort_addon_field {/* Sort addon packed field */
  Field *field;          /* Original field */
  uint   offset;         /* Offset from the last sorted field */
  uint   null_offset;    /* Offset to to null bit from the last sorted field */
  uint   max_length;     /* Maximum length in the sort buffer */
  uint8  null_bit;       /* Null bit mask for the field */
};

/**
  Descriptor for a merge chunk to be sort-merged.
  A merge chunk is a sequence of pre-sorted records, written to a
  temporary file. A Merge_chunk instance describes where this chunk is stored
  in the file, and where it is located when it is in memory.

  It is a POD because
   - we read/write them from/to files.

  We have accessors (getters/setters) for all struct members.
 */
struct Merge_chunk
{
public:
  Merge_chunk()
    : m_current_key(NULL),
      m_file_position(0),
      m_buffer_start(NULL),
      m_buffer_end(NULL),
      m_rowcount(0),
      m_mem_count(0),
      m_max_keys(0)
  {}

  my_off_t file_position() const { return m_file_position; }
  void set_file_position(my_off_t val) { m_file_position= val; }
  void advance_file_position(my_off_t val) { m_file_position+= val; }

  uchar *buffer_start() { return m_buffer_start; }
  const uchar *buffer_end() const { return m_buffer_end; }

  void set_buffer(uchar *start, uchar *end)
  {
    m_buffer_start= start;
    m_buffer_end= end;
  }
  void set_buffer_start(uchar *start)
  {
    m_buffer_start= start;
  }
  void set_buffer_end(uchar *end)
  {
    DBUG_ASSERT(m_buffer_end == NULL || end <= m_buffer_end);
    DBUG_ASSERT(m_buffer_start != nullptr);
    m_buffer_end= end;
  }

  void init_current_key() { m_current_key= m_buffer_start; }
  uchar *current_key() const { return m_current_key; }
  void advance_current_key(uint val) { m_current_key+= val; }

  void decrement_rowcount(ha_rows val) { m_rowcount-= val; }
  void set_rowcount(ha_rows val)       { m_rowcount= val; }
  ha_rows rowcount() const             { return m_rowcount; }

  ha_rows mem_count() const { return m_mem_count; }
  void set_mem_count(ha_rows val) { m_mem_count= val; }
  ha_rows decrement_mem_count() { return --m_mem_count; }

  ha_rows max_keys() const { return m_max_keys; }
  void set_max_keys(ha_rows val) { m_max_keys= val; }

  size_t  buffer_size() const { return m_buffer_end - m_buffer_start; }

  /**
    Tries to merge *this with *mc, returns true if successful.
    The assumption is that *this is no longer in use,
    and the space it has been allocated can be handed over to a
    buffer which is adjacent to it.
   */
  bool merge_freed_buff(Merge_chunk *mc) const
  {
    if (mc->m_buffer_end == m_buffer_start)
    {
      mc->m_buffer_end= m_buffer_end;
      mc->m_max_keys+= m_max_keys;
      return true;
    }
    else if (mc->m_buffer_start == m_buffer_end)
    {
      mc->m_buffer_start= m_buffer_start;
      mc->m_max_keys+= m_max_keys;
      return true;
    }
    return false;
  }

private:
  uchar   *m_current_key;  ///< The current key for this chunk.
  my_off_t m_file_position;///< Current position in the file to be sorted.
  uchar   *m_buffer_start; ///< Start of main-memory buffer for this chunk.
  uchar   *m_buffer_end;   ///< End of main-memory buffer for this chunk.
  ha_rows  m_rowcount;     ///< Number of unread rows in this chunk.
  ha_rows  m_mem_count;    ///< Number of rows in the main-memory buffer.
  ha_rows  m_max_keys;     ///< If we have fixed-size rows:
                           ///    max number of rows in buffer.
};

typedef Bounds_checked_array<Sort_addon_field> Addon_fields_array;
typedef Bounds_checked_array<Merge_chunk>      Merge_chunk_array;

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
    DBUG_ASSERT(!arr.is_null());
  }

  Sort_addon_field *begin() { return m_field_descriptors.begin(); }
  Sort_addon_field *end()   { return m_field_descriptors.end(); }
  size_t num_field_descriptors() const { return m_field_descriptors.size(); }

  /// rr_unpack_from_tempfile needs an extra buffer when unpacking.
  uchar *allocate_addon_buf(uint sz)
  {
    if (m_addon_buf != NULL)
    {
      DBUG_ASSERT(m_addon_buf_length == sz);
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
  There are several record formats for sorting:
@verbatim
    |<key a><key b>...    | <rowid> |
    / m_fixed_sort_length / ref_len /
@endverbatim

  or with "addon fields"
@verbatim
    |<key a><key b>...    |<null bits>|<field a><field b>...|
    / m_fixed_sort_length /         addon_length            /
@endverbatim

  The packed format for "addon fields"
@verbatim
    |<key a><key b>...    |<length>|<null bits>|<field a><field b>...|
    / m_fixed_sort_length /         addon_length                     /
@endverbatim

  All the formats above have fixed-size keys, with appropriate padding.
  Fixed-size keys can be compared/sorted using memcmp().

  The packed (variable length) format for keys:
@verbatim
    |<keylen>|<varkey a><key b>...<hash>|<rowid>  or <addons>     |
    / 4 bytes/   keylen bytes           / ref_len or addon_length /
@endverbatim

  This format is currently only used if we are sorting JSON data.
  Variable-size keys must be compared piece-by-piece, using type information
  about each individual key part, @see cmp_varlen_keys.

  All the record formats consist of a (possibly composite) key,
  followed by a (possibly composite) payload.
  The key is used for sorting data. Once sorting is done, the payload is
  stored in some buffer, and read by some rr_from or rr_unpack routine.

  For fixed-size keys, with @<rowid@> payload, the @<rowid@> is also
  considered to be part of the key.

<dl>
<dt>@<key@>
          <dd>  Fields are fixed-size, specially encoded with
                Field::make_sort_key() so we can do byte-by-byte compare.
<dt>@<length@>
          <dd>  Contains the *actual* packed length (after packing) of
                everything after the sort keys.
                The size of the length field is 2 bytes,
                which should cover most use cases: addon data <= 65535 bytes.
                This is the same as max record size in MySQL.
<dt>@<null bits@>
          <dd>  One bit for each nullable field, indicating whether the field
                is null or not. May have size zero if no fields are nullable.
<dt>@<field xx@>
          <dd>  Are stored with field->pack(), and retrieved with
                field->unpack().
                Addon fields within a record are stored consecutively, with no
                "holes" or padding. They will have zero size for NULL values.
<dt>@<keylen@>
          <dd>  Contains the *actual* packed length of all the keys.
                We may have an arbitrary mix of fixed and variable-sized keys.
<dt>@<hash@>
          <dd>  Optional 8 byte hash, used for GROUPing of JSON values.
<dt>@<varkey@>
          <dd>  Used for JSON values, the format is:
</dl>
@verbatim
                |<null value>|<key length>|<JSON sort key>    |
                / 1 byte     /   4 bytes  / key length bytes  /
@endverbatim
 */
class Sort_param {
  uint m_fixed_rec_length;    ///< Maximum length of a record, see above.
  uint m_fixed_sort_length;   ///< Maximum number of bytes used for sorting.
public:
  uint ref_length;            // Length of record ref.
  uint addon_length;          // Length of added packed fields.
  uint fixed_res_length;      // Length of records in final sorted file/buffer.
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
  bool not_killable;
  bool using_pq;
  char* tmp_buffer;

  Sort_param()
  {
    memset(this, 0, sizeof(*this));
  }
  /**
    Initialize this struct for filesort() usage.
    @see description of record layout above
    @param [in,out] file_sort sorting information which may be re-used on
                              subsequent invocations of filesort()
    @param sf_array  initialization value for local_sortorder
    @param sortlen   length of sorted columns
    @param table     table to be sorted
    @param max_length_for_sort_data from thd->variables
    @param maxrows   HA_POS_ERROR or possible LIMIT value
    @param sort_positions see documentation for the filesort() function
  */
  void init_for_filesort(Filesort *file_sort,
                         Bounds_checked_array<st_sort_field> sf_array,
                         uint sortlen, TABLE *table,
                         ulong max_length_for_sort_data,
                         ha_rows maxrows, bool sort_positions);

  /// Enables the packing of addons if possible.
  void try_to_pack_addons(ulong max_length_for_sort_data);

  /// Are we packing the "addon fields"?
  bool using_packed_addons() const
  {
    DBUG_ASSERT(m_using_packed_addons ==
                (addon_fields != NULL && addon_fields->using_packed_addons()));
    return m_using_packed_addons;
  }

  /// Are we using varlen JSON key fields?
  bool using_varlen_keys() const
  {
    return m_num_varlen_keys > 0;
  }

  /// Are we using "addon fields"?
  bool using_addon_fields() const
  {
    return addon_fields != NULL;
  }

  /**
    Stores key fields in *to.
    Then appends either *ref_pos (the @<rowid@>) or the "addon fields".
    @param  to      out Where to store the result
    @param  ref_pos in  Where to find the @<rowid@>
    @returns Number of bytes stored.
   */
  uint make_sortkey(uchar *to, const uchar *ref_pos);

  /// Stores the length of a variable-sized key.
  static void store_varlen_key_length(uchar *p, uint sz);

  /// Skips the key part, and returns address of payload.
  uchar *get_start_of_payload(uchar *p) const;

  /**
    Skips the key part, and returns address of payload.
    For rr_unpack_from_buffer, which does not have access to Sort_param.
   */
  static uchar *get_start_of_payload(uint val, bool is_varlen, uchar *p);

  /// @returns The number of bytes used for sorting of fixed-size keys.
  uint max_compare_length() const
  {
    return m_fixed_sort_length;
  }

  void set_max_compare_length(uint len)
  {
    m_fixed_sort_length= len;
  }

  /// @returns The actual size of a record (key + addons)
  size_t get_record_length(uchar *p) const;

  /// @returns The maximum size of a record (key + addons)
  uint max_record_length() const
  {
    return m_fixed_rec_length;
  }

  void set_max_record_length(uint len)
  {
    m_fixed_rec_length= len;
  }

  /**
    Getter for record length and result length.
    @param record_start Pointer to record.
    @param [out] recl   Store record length here.
    @param [out] resl   Store result length here.
   */
  void get_rec_and_res_len(uchar *record_start, uint *recl, uint *resl);

  static const uint size_of_varlength_field= 4;

private:
  /// Counts number of JSON keys
  int count_varlen_keys() const;

  uint m_packable_length; ///< total length of fields which have a packable type
  bool m_using_packed_addons; ///< caches the value of using_packed_addons()
  int  m_num_varlen_keys;     ///< number of varlen keys

public:
  // Not copyable.
  Sort_param(const Sort_param&)= delete;
  Sort_param &operator=(const Sort_param&)= delete;
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
  bool      m_using_varlen_keys;
  uint      m_sort_length;

  ha_rows   found_records;        ///< How many records in sort.

  // Note that we use the default copy CTOR / assignment operator in filesort().
  Filesort_info(const Filesort_info&)= default;
  Filesort_info &operator=(const Filesort_info&)= default;

  Filesort_info()
    : sorted_result_in_fsbuf(false),
      sorted_result(NULL), sorted_result_end(NULL),
      m_using_varlen_keys(false), m_sort_length(0)
  {};

  bool has_filesort_result_in_memory() const
  {
    return sorted_result || sorted_result_in_fsbuf;
  }

  bool has_filesort_result() const
  {
    return has_filesort_result_in_memory() ||
      (io_cache && my_b_inited(io_cache));
  }

  /** Sort filesort_buffer */
  void sort_buffer(Sort_param *param, uint count)
  { filesort_buffer.sort_buffer(param, count); }

  /**
    Copies (unpacks) values appended to sorted fields from a buffer back to
    their regular positions specified by the Field::ptr pointers.
    @param buff            Buffer which to unpack the value from
  */
  template<bool Packed_addon_fields>
    inline void unpack_addon_fields(uchar *buff);

  /**
    Reads 'count' number of chunk descriptors into the merge_chunks array.
    In case of error, the merge_chunks array will be empty.
    @param chunk_file File containing the descriptors.
    @param count      Number of chunks to read.
  */
  void read_chunk_descriptors(IO_CACHE* chunk_file, uint count);

  /// Are we using "addon fields"?
  bool using_addon_fields() const
  {
    return addon_fields != NULL;
  }

  /// Are we using "packed addon fields"?
  bool using_packed_addons() const
  {
    return addon_fields != NULL && addon_fields->using_packed_addons();
  }

  /**
    Accessors for filesort_buffer (@see Filesort_buffer for documentation).
  */
  size_t space_used_for_data() const
  { return filesort_buffer.space_used_for_data(); }

  bool isfull() const
  { return filesort_buffer.isfull(); }

  void init_next_record_pointer()
  { filesort_buffer.init_next_record_pointer(); }

  uchar *get_next_record_pointer()
  { return filesort_buffer.get_next_record_pointer(); }

  void adjust_next_record_pointer(uint32 val)
  { filesort_buffer.adjust_next_record_pointer(val); }

  uchar* get_sorted_record(uint idx)
  { return filesort_buffer.get_sorted_record(idx); }

  uchar **get_sort_keys()
  { return filesort_buffer.get_sort_keys(); }

  Bounds_checked_array<uchar> get_raw_buf()
  { return filesort_buffer.get_raw_buf(); }

  uchar *alloc_sort_buffer(uint num_records, uint record_length)
  { return filesort_buffer.alloc_sort_buffer(num_records, record_length); }

  void free_sort_buffer()
  { filesort_buffer.free_sort_buffer(); }

  void init_record_pointers()
  { filesort_buffer.init_record_pointers(); }

  size_t sort_buffer_size() const
  { return filesort_buffer.sort_buffer_size(); }

  uchar *get_start_of_payload(uchar *p)
  {
    return
      Sort_param::get_start_of_payload(m_sort_length, m_using_varlen_keys, p);
  }

  void set_sort_length(uint val, bool is_varlen)
  {
    m_sort_length= val;
    m_using_varlen_keys= is_varlen;
  }
};

typedef Bounds_checked_array<uchar> Sort_buffer;


/**
  Put all room used by freed buffer to use in adjacent buffer.

  Note, that we can't simply distribute memory evenly between all buffers,
  because new areas must not overlap with old ones.
*/
template<typename Heap_type>
void reuse_freed_buff(Merge_chunk *old_top, Heap_type *heap)
{
  typename Heap_type::iterator it= heap->begin();
  typename Heap_type::iterator end= heap->end();
  for (; it != end; ++it)
  {
    if (old_top->merge_freed_buff(*it))
      return;
  }
  DBUG_ASSERT(0);
}

#endif /* SQL_SORT_INCLUDED */
