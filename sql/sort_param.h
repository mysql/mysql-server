/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SORT_PARAM_INCLUDED
#define SORT_PARAM_INCLUDED

#include "binary_log_types.h"  // enum_field_types
#include "my_base.h"           // ha_rows
#include "my_byteorder.h"      // uint2korr
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"          // mysql_com.h needs my_socket
#include "mysql_com.h"      // Item_result
#include "sql/sql_array.h"  // Bounds_checked_array
#include "sql/sql_const.h"
#include "sql/sql_sort.h"  // Filesort_info
#include "sql/thr_malloc.h"
#include "sql_string.h"

class Field;
class Filesort;
class Item;
struct TABLE;

enum class Addon_fields_status {
  unknown_status,
  using_heap_table,
  fulltext_searched,
  keep_rowid,
  row_not_packable,
  row_contains_blob,
  max_length_for_sort_data,
  row_too_large,
  skip_heuristic,
  using_priority_queue
};

inline const char *addon_fields_text(Addon_fields_status afs) {
  switch (afs) {
    default:
      return "unknown";
    case Addon_fields_status::using_heap_table:
      return "using_heap_table";
    case Addon_fields_status::fulltext_searched:
      return "fulltext_searched";
    case Addon_fields_status::keep_rowid:
      return "keep_rowid";
    case Addon_fields_status::row_not_packable:
      return "row_not_packable";
    case Addon_fields_status::row_contains_blob:
      return "row_contains_blob";
    case Addon_fields_status::max_length_for_sort_data:
      return "max_length_for_sort_data";
    case Addon_fields_status::row_too_large:
      return "row_too_large";
    case Addon_fields_status::skip_heuristic:
      return "skip_heuristic";
    case Addon_fields_status::using_priority_queue:
      return "using_priority_queue";
  }
}

/* Structs used when sorting */

/// Struct that holds information about a sort field.
struct st_sort_field {
  Field *field;  ///< Field to sort
  Item *item;    ///< Item if not sorting fields

  /// Length of sort field. Beware, can be 0xFFFFFFFFu (infinite)!
  uint length;

  uint suffix_length;           ///< Length suffix (0-4)
  Item_result result_type;      ///< Type of item (not used for fields)
  enum_field_types field_type;  ///< Field type of the field or item
  bool reverse;                 ///< if descending sort
  bool is_varlen;               ///< If key part has variable length
  bool maybe_null;              ///< If key part is nullable
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

struct Sort_addon_field { /* Sort addon packed field */
  Field *field;           /* Original field */
  uint offset;            /* Offset from the last sorted field */
  uint null_offset;       /* Offset to to null bit from the last sorted field */
  uint max_length;        /* Maximum length in the sort buffer */
  uint8 null_bit;         /* Null bit mask for the field */
};

typedef Bounds_checked_array<Sort_addon_field> Addon_fields_array;

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
        m_using_packed_addons(false) {
    DBUG_ASSERT(!arr.is_null());
  }

  Sort_addon_field *begin() { return m_field_descriptors.begin(); }
  Sort_addon_field *end() { return m_field_descriptors.end(); }
  size_t num_field_descriptors() const { return m_field_descriptors.size(); }

  /// SortFileIterator needs an extra buffer when unpacking.
  uchar *allocate_addon_buf(uint sz) {
    if (m_addon_buf != NULL) {
      DBUG_ASSERT(m_addon_buf_length == sz);
      return m_addon_buf;
    }
    m_addon_buf = static_cast<uchar *>(sql_alloc(sz));
    if (m_addon_buf) m_addon_buf_length = sz;
    return m_addon_buf;
  }

  uchar *get_addon_buf() { return m_addon_buf; }
  uint get_addon_buf_length() const { return m_addon_buf_length; }

  void set_using_packed_addons(bool val) { m_using_packed_addons = val; }

  bool using_packed_addons() const { return m_using_packed_addons; }

  static bool can_pack_addon_fields(uint record_length) {
    return (record_length <= (0xFFFF));
  }

  /**
    @returns Total number of bytes used for packed addon fields.
    the size of the length field + size of null bits + sum of field sizes.
   */
  static uint read_addon_length(uchar *p) {
    return size_of_length_field + uint2korr(p);
  }

  /**
    Stores the number of bytes used for packed addon fields.
   */
  static void store_addon_length(uchar *p, uint sz) {
    // We actually store the length of everything *after* the length field.
    int2store(p, sz - size_of_length_field);
  }

  static const uint size_of_length_field = 2;

 private:
  Addon_fields_array m_field_descriptors;

  uchar *m_addon_buf;          ///< Buffer for unpacking addon fields.
  uint m_addon_buf_length;     ///< Length of the buffer.
  bool m_using_packed_addons;  ///< Are we packing the addon fields?
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
  stored in some buffer, and read by some RowIterator.

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
          <dd>  Used for JSON and variable-length string values, the format is:
</dl>
@verbatim
                |<null value>|<key length>|<sort key>        |
                / 1 byte     /   4 bytes  / key length bytes /
@endverbatim
<dl>
<dt>@<null value@>
          <dd>  0x00 for NULL. 0xff for NULL under DESC sort. 0x01 for NOT NULL.
<dt>@<key length@>
          <dd>  The length of the sort key, *including* the four bytes for the
                key length. Does not exist if the field is NULL.
</dl>
 */
class Sort_param {
  uint m_fixed_rec_length;   ///< Maximum length of a record, see above.
  uint m_fixed_sort_length;  ///< Maximum number of bytes used for sorting.
 public:
  uint ref_length;            // Length of record ref.
  uint m_addon_length;        // Length of added packed fields.
  uint fixed_res_length;      // Length of records in final sorted file/buffer.
  uint max_rows_per_buffer;   // Max (unpacked) rows / buffer.
  ha_rows max_rows;           // Select limit, or HA_POS_ERROR if unlimited.
  ha_rows num_examined_rows;  // Number of examined rows.
  TABLE *sort_form;           // For quicker make_sortkey.
  bool use_hash;              // Whether to use hash to distinguish cut JSON
  bool m_force_stable_sort;   // Keep relative order of equal elements

  /**
    ORDER BY list with some precalculated info for filesort.
    Array is created and owned by a Filesort instance.
   */
  Bounds_checked_array<st_sort_field> local_sortorder;

  Addon_fields *addon_fields;  ///< Descriptors for addon fields.
  bool not_killable;
  bool using_pq;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> tmp_buffer;

  Sort_param() { memset(this, 0, sizeof(*this)); }
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
                         ulong max_length_for_sort_data, ha_rows maxrows,
                         bool sort_positions);

  /// Enables the packing of addons if possible.
  void try_to_pack_addons(ulong max_length_for_sort_data);

  /// Are we packing the "addon fields"?
  bool using_packed_addons() const {
    DBUG_ASSERT(m_using_packed_addons ==
                (addon_fields != NULL && addon_fields->using_packed_addons()));
    return m_using_packed_addons;
  }

  /// Are we using varlen key fields?
  bool using_varlen_keys() const { return m_num_varlen_keys > 0; }

  /// Are we using any JSON key fields?
  bool using_json_keys() const { return m_num_json_keys > 0; }

  /// Are we using "addon fields"?
  bool using_addon_fields() const { return addon_fields != NULL; }

  /**
    Stores key fields in *dst.
    Then appends either *ref_pos (the @<rowid@>) or the "addon fields".
    @param  dst     out Where to store the result
    @param  ref_pos in  Where to find the @<rowid@>
    @returns Number of bytes stored, or UINT_MAX if the result could not
      provably fit within the destination buffer.
   */
  uint make_sortkey(Bounds_checked_array<uchar> dst, const uchar *ref_pos);

  // Adapter for Bounded_queue.
  uint make_sortkey(uchar *dst, size_t dst_len, const uchar *ref_pos) {
    return make_sortkey(Bounds_checked_array<uchar>(dst, dst_len), ref_pos);
  }

  /// Stores the length of a variable-sized key.
  static void store_varlen_key_length(uchar *p, uint sz) { int4store(p, sz); }

  /// Skips the key part, and returns address of payload.
  uchar *get_start_of_payload(uchar *p) const {
    size_t offset = using_varlen_keys() ? uint4korr(p) : max_compare_length();
    if (!using_addon_fields() && !using_varlen_keys())
      offset -= ref_length;  // The reference is also part of the sort key.
    return p + offset;
  }

  /**
    Skips the key part, and returns address of payload.
    For SortBufferIterator, which does not have access to Sort_param.
   */
  static uchar *get_start_of_payload(uint default_val, bool is_varlen,
                                     uchar *p) {
    size_t offset = is_varlen ? uint4korr(p) : default_val;
    return p + offset;
  }

  /// @returns The number of bytes used for sorting of fixed-size keys.
  uint max_compare_length() const { return m_fixed_sort_length; }

  void set_max_compare_length(uint len) { m_fixed_sort_length = len; }

  /// @returns The actual size of a record (key + addons)
  size_t get_record_length(uchar *p) const;

  /// @returns The maximum size of a record (key + addons)
  uint max_record_length() const { return m_fixed_rec_length; }

  void set_max_record_length(uint len) { m_fixed_rec_length = len; }

  /**
    Getter for record length and result length.
    @param record_start Pointer to record.
    @param [out] recl   Store record length here.
    @param [out] resl   Store result length here.
   */
  void get_rec_and_res_len(uchar *record_start, uint *recl, uint *resl);

  enum enum_sort_algorithm {
    FILESORT_ALG_NONE,
    FILESORT_ALG_STD_SORT,
    FILESORT_ALG_STD_STABLE
  };
  enum_sort_algorithm m_sort_algorithm;

  Addon_fields_status m_addon_fields_status;

  static const uint size_of_varlength_field = 4;

 private:
  /// Counts number of varlen keys
  int count_varlen_keys() const;
  /// Counts number of JSON keys
  int count_json_keys() const;

  uint
      m_packable_length;  ///< total length of fields which have a packable type
  bool m_using_packed_addons;  ///< caches the value of using_packed_addons()
  int m_num_varlen_keys;       ///< number of varlen keys
  int m_num_json_keys;         ///< number of JSON keys

 public:
  // Not copyable.
  Sort_param(const Sort_param &) = delete;
  Sort_param &operator=(const Sort_param &) = delete;
};

inline uchar *get_start_of_payload(const Filesort_info *fsi, uchar *p) {
  return Sort_param::get_start_of_payload(fsi->sort_length(),
                                          fsi->using_varlen_keys(), p);
}

/// Are we using "packed addon fields"?
inline bool using_packed_addons(const Filesort_info *fsi) {
  return fsi->addon_fields != nullptr &&
         fsi->addon_fields->using_packed_addons();
}

#endif  // SORT_PARAM_INCLUDED
