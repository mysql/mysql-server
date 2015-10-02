#ifndef SQL_SORT_INCLUDED
#define SQL_SORT_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* uchar */
#include "my_base.h"                            /* ha_rows */
#include "sql_array.h"
#include "mysql_com.h"
#include "filesort_utils.h"
#include "sql_alloc.h"
#include <string.h>                             /* memset */
#include <vector>

class Field;
class Item;
struct TABLE;
class Filesort;

/* Defines used by filesort and uniques */

#define MERGEBUFF		7
#define MERGEBUFF2		15

/* Structs used when sorting */

struct st_sort_field {
  Field *field;				/* Field to sort */
  Item	*item;				/* Item if not sorting fields */
  uint	 length;			/* Length of sort field */
  uint   suffix_length;                 /* Length suffix (0-4) */
  Item_result result_type;		/* Type of item */
  enum_field_types field_type;          /* Field type of the field or item */
  bool reverse;				/* if descending sort */
  bool need_strxnfrm;			/* If we have to use strxnfrm() */
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

struct Merge_chunk_compare_context
{
  qsort_cmp2 key_compare;
  const void *key_compare_arg;
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
    m_buffer_end= end;
  }

  void init_current_key() { m_current_key= m_buffer_start; }
  uchar *current_key() { return m_current_key; }
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
  uchar   *m_current_key;  /// The current key for this chunk.
  my_off_t m_file_position;/// Current position in the file to be sorted.
  uchar   *m_buffer_start; /// Start of main-memory buffer for this chunk.
  uchar   *m_buffer_end;   /// End of main-memory buffer for this chunk.
  ha_rows  m_rowcount;     /// Number of unread rows in this chunk.
  ha_rows  m_mem_count;    /// Number of rows in the main-memory buffer.
  ha_rows  m_max_keys;     /// If we have fixed-size rows:
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
    DBUG_ASSERT(m_using_packed_addons ==
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
    DBUG_ASSERT(*resl <= res_length);
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

  uint get_sort_length() const
  { return filesort_buffer.get_sort_length(); }

  void set_sort_length(uint val)
  { filesort_buffer.set_sort_length(val); }
};

typedef Bounds_checked_array<uchar> Sort_buffer;

int merge_many_buff(Sort_param *param, Sort_buffer sort_buffer,
		    Merge_chunk_array chunk_array,
		    size_t *num_chunks, IO_CACHE *t_file);
uint read_to_buffer(IO_CACHE *fromfile, Merge_chunk *merge_chunk,
                    Sort_param *param);
int merge_buffers(Sort_param *param,IO_CACHE *from_file,
                  IO_CACHE *to_file, Sort_buffer sort_buffer,
                  Merge_chunk *lastbuff,
                  Merge_chunk_array chunk_array,
                  int flag);

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
