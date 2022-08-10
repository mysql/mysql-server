/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

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

/** @file rem/rec.h
 Record manager

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#ifndef rem_rec_h
#define rem_rec_h

#include "dict0boot.h"
#include "dict0dict.h"

/* Compact flag ORed to the extra size returned by rec_get_offsets() */
constexpr uint32_t REC_OFFS_COMPACT = 1U << 31;
/* SQL NULL flag in offsets returned by rec_get_offsets() */
constexpr uint32_t REC_OFFS_SQL_NULL = 1U << 31;
/* External flag in offsets returned by rec_get_offsets() */
constexpr uint32_t REC_OFFS_EXTERNAL = 1 << 30;
/* Default value flag in offsets returned by rec_get_offsets() */
constexpr uint32_t REC_OFFS_DEFAULT = 1 << 29;
/* An INSTANT DROP flag in offsets returned by rec_get_offsets() */
constexpr uint32_t REC_OFFS_DROP = 1 << 28;
/* Mask for offsets returned by rec_get_offsets() */
constexpr uint32_t REC_OFFS_MASK = REC_OFFS_DROP - 1;

/* The offset of heap_no in a compact record */
constexpr uint32_t REC_NEW_HEAP_NO = 4;
/* The shift of heap_no in a compact record.
The status is stored in the low-order bits. */
constexpr uint32_t REC_HEAP_NO_SHIFT = 3;

/* We list the byte offsets from the origin of the record, the mask,
and the shift needed to obtain each bit-field of the record. */

constexpr uint32_t REC_NEXT = 2;
constexpr uint32_t REC_NEXT_MASK = 0xFFFFUL;
constexpr uint32_t REC_NEXT_SHIFT = 0;

constexpr uint32_t REC_OLD_SHORT = 3; /* This is single byte bit-field */
constexpr uint32_t REC_OLD_SHORT_MASK = 0x1UL;
constexpr uint32_t REC_OLD_SHORT_SHIFT = 0;

constexpr uint32_t REC_OLD_N_FIELDS = 4;
constexpr uint32_t REC_OLD_N_FIELDS_MASK = 0x7FEUL;
constexpr uint32_t REC_OLD_N_FIELDS_SHIFT = 1;

constexpr uint32_t REC_NEW_STATUS = 3; /* This is single byte bit-field */
constexpr uint32_t REC_NEW_STATUS_MASK = 0x7UL;
constexpr uint32_t REC_NEW_STATUS_SHIFT = 0;

constexpr uint32_t REC_OLD_HEAP_NO = 5;
constexpr uint32_t REC_HEAP_NO_MASK = 0xFFF8UL;
#if 0 /* defined in rem0rec.h for use of page0zip.cc */
#define REC_NEW_HEAP_NO 4
#define REC_HEAP_NO_SHIFT 3
#endif

constexpr uint32_t REC_OLD_N_OWNED = 6; /* This is single byte bit-field */
constexpr uint32_t REC_NEW_N_OWNED = 5; /* This is single byte bit-field */
constexpr uint32_t REC_N_OWNED_MASK = 0xFUL;
constexpr uint32_t REC_N_OWNED_SHIFT = 0;

constexpr uint32_t REC_OLD_INFO_BITS = 6; /* This is single byte bit-field */
constexpr uint32_t REC_NEW_INFO_BITS = 5; /* This is single byte bit-field */
constexpr uint32_t REC_TMP_INFO_BITS = 1; /* This is single byte bit-field */
constexpr uint32_t REC_INFO_BITS_MASK = 0xF0UL;
constexpr uint32_t REC_INFO_BITS_SHIFT = 0;

static_assert((REC_OLD_SHORT_MASK << (8 * (REC_OLD_SHORT - 3)) ^
               REC_OLD_N_FIELDS_MASK << (8 * (REC_OLD_N_FIELDS - 4)) ^
               REC_HEAP_NO_MASK << (8 * (REC_OLD_HEAP_NO - 4)) ^
               REC_N_OWNED_MASK << (8 * (REC_OLD_N_OWNED - 3)) ^
               REC_INFO_BITS_MASK << (8 * (REC_OLD_INFO_BITS - 3)) ^
               0xFFFFFFFFUL) == 0,
              "sum of old-style masks != 0xFFFFFFFFUL");
static_assert((REC_NEW_STATUS_MASK << (8 * (REC_NEW_STATUS - 3)) ^
               REC_HEAP_NO_MASK << (8 * (REC_NEW_HEAP_NO - 4)) ^
               REC_N_OWNED_MASK << (8 * (REC_NEW_N_OWNED - 3)) ^
               REC_INFO_BITS_MASK << (8 * (REC_NEW_INFO_BITS - 3)) ^
               0xFFFFFFUL) == 0,
              "sum of new-style masks != 0xFFFFFFUL");

/* Info bit denoting the predefined minimum record: this bit is set
if and only if the record is the first user record on a non-leaf
B-tree page that is the leftmost page on its level
(PAGE_LEVEL is nonzero and FIL_PAGE_PREV is FIL_NULL). */
constexpr uint32_t REC_INFO_MIN_REC_FLAG = 0x10UL;
/** The deleted flag in info bits; when bit is set to 1, it means the record has
 been delete marked */
constexpr uint32_t REC_INFO_DELETED_FLAG = 0x20UL;
/* Use this bit to indicate record has version */
constexpr uint32_t REC_INFO_VERSION_FLAG = 0x40UL;
/** The instant ADD COLUMN flag. When it is set to 1, it means this record
was inserted/updated after an instant ADD COLUMN. */
constexpr uint32_t REC_INFO_INSTANT_FLAG = 0x80UL;

/* Number of extra bytes in an old-style record,
in addition to the data and the offsets */
constexpr uint32_t REC_N_OLD_EXTRA_BYTES = 6;
/* Number of extra bytes in a new-style record,
in addition to the data and the offsets */
constexpr int32_t REC_N_NEW_EXTRA_BYTES = 5;
/* Number of extra bytes in a new-style temporary record,
in addition to the data and the offsets.
This is used only after instant ADD COLUMN. */
constexpr uint32_t REC_N_TMP_EXTRA_BYTES = 1;

/* Record status values */
constexpr uint32_t REC_STATUS_ORDINARY = 0;
constexpr uint32_t REC_STATUS_NODE_PTR = 1;
constexpr uint32_t REC_STATUS_INFIMUM = 2;
constexpr uint32_t REC_STATUS_SUPREMUM = 3;

/* The following four constants are needed in page0zip.cc in order to
efficiently compress and decompress pages. */

/* Length of a B-tree node pointer, in bytes */
constexpr uint32_t REC_NODE_PTR_SIZE = 4;

/** SQL null flag in a 1-byte offset of ROW_FORMAT=REDUNDANT records */
constexpr uint32_t REC_1BYTE_SQL_NULL_MASK = 0x80UL;
/** SQL null flag in a 2-byte offset of ROW_FORMAT=REDUNDANT records */
constexpr uint32_t REC_2BYTE_SQL_NULL_MASK = 0x8000UL;

/** In a 2-byte offset of ROW_FORMAT=REDUNDANT records, the second most
significant bit denotes that the tail of a field is stored off-page. */
constexpr uint32_t REC_2BYTE_EXTERN_MASK = 0x4000UL;

#ifdef UNIV_DEBUG
/* Length of the rec_get_offsets() header */
constexpr uint32_t REC_OFFS_HEADER_SIZE = 4;
#else  /* UNIV_DEBUG */
/* Length of the rec_get_offsets() header */
constexpr uint32_t REC_OFFS_HEADER_SIZE = 2;
#endif /* UNIV_DEBUG */

/* Number of elements that should be initially allocated for the
offsets[] array, first passed to rec_get_offsets() */
constexpr uint32_t REC_OFFS_NORMAL_SIZE = 100;
constexpr uint32_t REC_OFFS_SMALL_SIZE = 10;

/* Get the base address of offsets.  The extra_size is stored at
this position, and following positions hold the end offsets of
the fields. */

static inline const ulint *rec_offs_base(const ulint *offsets) {
  return offsets + REC_OFFS_HEADER_SIZE;
}

static inline ulint *rec_offs_base(ulint *offsets) {
  return offsets + REC_OFFS_HEADER_SIZE;
}

/** Number of fields flag which means it occupies two bytes */
static const uint8_t REC_N_FIELDS_TWO_BYTES_FLAG = 0x80;

/** Max number of fields which can be stored in one byte */
static const uint8_t REC_N_FIELDS_ONE_BYTE_MAX = 0x7F;

/** The following function determines the offsets to each field
 in the record. It can reuse a previously allocated array.
 Note that after instant ADD COLUMN, if this is a record
 from clustered index, fields in the record may be less than
 the fields defined in the clustered index. So the offsets
 size is allocated according to the clustered index fields.
 @param[in] rec physical record
 @param[in] index record descriptor
 @param[in,out] offsets array consisting of offsets[0] allocated elements, or an
 array from rec_get_offsets(), or NULL
 @param[in] n_fields maximum number of initialized fields (ULINT_UNDEFINED is
 all fields)
 @param[in] location location where called
 @param[in,out] heap memory heap
 @return the new offsets */
[[nodiscard]] ulint *rec_get_offsets(const rec_t *rec,
                                     const dict_index_t *index, ulint *offsets,
                                     ulint n_fields, ut::Location location,
                                     mem_heap_t **heap);

/** The following function determines the offsets to each field
 in the record.  It can reuse a previously allocated array. */
void rec_get_offsets_reverse(
    const byte *extra,         /*!< in: the extra bytes of a
                               compact record in reverse order,
                               excluding the fixed-size
                               REC_N_NEW_EXTRA_BYTES */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint node_ptr,            /*!< in: nonzero=node pointer,
                              0=leaf node */
    ulint *offsets);           /*!< in/out: array consisting of
                              offsets[0] allocated elements */

/** Gets a bit field from within 1 byte. */
static inline ulint rec_get_bit_field_1(
    const rec_t *rec, /*!< in: pointer to record origin */
    ulint offs,       /*!< in: offset from the origin down */
    ulint mask,       /*!< in: mask used to filter bits */
    ulint shift)      /*!< in: shift right applied after masking */
{
  ut_ad(rec);

  return ((mach_read_from_1(rec - offs) & mask) >> shift);
}

/** Gets a bit field from within 2 bytes. */
static inline uint16_t rec_get_bit_field_2(
    const rec_t *rec, /*!< in: pointer to record origin */
    ulint offs,       /*!< in: offset from the origin down */
    ulint mask,       /*!< in: mask used to filter bits */
    ulint shift)      /*!< in: shift right applied after masking */
{
  ut_ad(rec);

  return ((mach_read_from_2(rec - offs) & mask) >> shift);
}

/** The following function retrieves the status bits of a new-style record.
 @return status bits */
[[nodiscard]] static inline ulint rec_get_status(
    const rec_t *rec) /*!< in: physical record */
{
  ulint ret;

  ut_ad(rec);

  ret = rec_get_bit_field_1(rec, REC_NEW_STATUS, REC_NEW_STATUS_MASK,
                            REC_NEW_STATUS_SHIFT);
  ut_ad((ret & ~REC_NEW_STATUS_MASK) == 0);

  return (ret);
}

#ifdef UNIV_DEBUG
/** Check if the info bits are valid.
@param[in]      bits    info bits to check
@return true if valid */
inline bool rec_info_bits_valid(ulint bits) {
  return (0 == (bits & ~(REC_INFO_DELETED_FLAG | REC_INFO_MIN_REC_FLAG |
                         REC_INFO_INSTANT_FLAG | REC_INFO_VERSION_FLAG)));
}
#endif /* UNIV_DEBUG */

/** The following function is used to retrieve the info bits of a record.
@param[in]      rec     physical record
@param[in]      comp    nonzero=compact page format
@return info bits */
static inline ulint rec_get_info_bits(const rec_t *rec, ulint comp) {
  const ulint val =
      rec_get_bit_field_1(rec, comp ? REC_NEW_INFO_BITS : REC_OLD_INFO_BITS,
                          REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
  ut_ad(rec_info_bits_valid(val));
  return (val);
}

/** The following function is used to retrieve the info bits of a temporary
record.
@param[in]      rec     physical record
@return info bits */
static inline ulint rec_get_info_bits_temp(const rec_t *rec) {
  const ulint val = rec_get_bit_field_1(
      rec, REC_TMP_INFO_BITS, REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
  ut_ad(rec_info_bits_valid(val));
  return (val);
}

/** The following function is used to get the number of fields
 in an old-style record, which is stored in the rec
 @return number of data fields */
[[nodiscard]] static inline uint16_t rec_get_n_fields_old_raw(
    const rec_t *rec) /*!< in: physical record */
{
  uint16_t n_fields;

  ut_ad(rec);

  n_fields = rec_get_bit_field_2(rec, REC_OLD_N_FIELDS, REC_OLD_N_FIELDS_MASK,
                                 REC_OLD_N_FIELDS_SHIFT);

  ut_ad(n_fields <= REC_MAX_N_FIELDS);
  ut_ad(n_fields > 0);

  return (n_fields);
}

/** The following function is used to get the number of fields
in an old-style record. Have to consider the case that after
instant ADD COLUMN, this record may have less fields than
current index.
@param[in]      rec     physical record
@param[in]      index   index where the record resides
@return number of data fields */
[[nodiscard]] static inline uint16_t rec_get_n_fields_old(
    const rec_t *rec, const dict_index_t *index) {
  uint16_t n = rec_get_n_fields_old_raw(rec);

  if (index->has_instant_cols_or_row_versions()) {
    if (strcmp(index->name, RECOVERY_INDEX_TABLE_NAME) == 0) {
      /* In recovery, index is not completely built. Skip validation. */
      if (n > 1) /* For infimum/supremum, n is 1 */ {
        n = static_cast<uint16_t>(dict_index_get_n_fields(index));
      }
      return n;
    }

    uint16_t n_uniq = dict_index_get_n_unique_in_tree_nonleaf(index);

    ut_ad(index->is_clustered());
    ut_ad(n <= dict_index_get_n_fields(index));
    ut_ad(n_uniq > 0);
    /* Only when it's infimum or supremum, n is 1.
    If n is exact n_uniq, this should be a record copied with prefix during
    search.
    And if it's node pointer, n is n_uniq + 1, which should be always less
    than the number of fields in any leaf page, even if the record in
    leaf page is before instant ADD COLUMN. This is because any record in
    leaf page must have at least n_uniq + 2 (system columns) fields */
    ut_ad(n == 1 || n >= n_uniq);
    ut_ad(static_cast<uint16_t>(dict_index_get_n_fields(index)) > n_uniq + 1);
    if (n > n_uniq + 1) {
      /* This is leaf node. If table has INSTANT columns, then it is possible
      that record might not have all the fields in index. So get it now from
      index. */
#ifdef UNIV_DEBUG
      if (index->has_instant_cols() && !index->has_row_versions()) {
        ut_ad(dict_index_get_n_fields(index) >= n);
        ulint rec_diff = dict_index_get_n_fields(index) - n;
        ulint col_diff = index->table->n_cols - index->table->n_instant_cols;
        ut_ad(rec_diff <= col_diff);
      }

      if (n != dict_index_get_n_fields(index)) {
        ut_ad(index->has_instant_cols_or_row_versions());
      }
#endif /* UNIV_DEBUG */
      n = static_cast<uint16_t>(dict_index_get_n_fields(index));
    }
  }

  return (n);
}

/** The following function is used to get the number of fields
 in a record. If it's REDUNDANT record, the returned number
 would be a logic one which considers the fact that after
 instant ADD COLUMN, some records may have less fields than
 index.
 @return number of data fields */
static inline ulint rec_get_n_fields(
    const rec_t *rec,          /*!< in: physical record */
    const dict_index_t *index) /*!< in: record descriptor */
{
  ut_ad(rec);
  ut_ad(index);

  if (!dict_table_is_comp(index->table)) {
    return (rec_get_n_fields_old(rec, index));
  }

  switch (rec_get_status(rec)) {
    case REC_STATUS_ORDINARY:
      return (dict_index_get_n_fields(index));
    case REC_STATUS_NODE_PTR:
      return (dict_index_get_n_unique_in_tree(index) + 1);
    case REC_STATUS_INFIMUM:
    case REC_STATUS_SUPREMUM:
      return (1);
    default:
      ut_error;
  }
}

/** Confirms the n_fields of the entry is sane with comparing the other
record in the same page specified
@param[in]      index   index
@param[in]      rec     record of the same page
@param[in]      entry   index entry
@return true if n_fields is sane */
static inline bool rec_n_fields_is_sane(dict_index_t *index, const rec_t *rec,
                                        const dtuple_t *entry) {
  return (rec_get_n_fields(rec, index) == dtuple_get_n_fields(entry)
          /* a record for older SYS_INDEXES table
          (missing merge_threshold column) is acceptable. */
          || (index->table->id == DICT_INDEXES_ID &&
              rec_get_n_fields(rec, index) == dtuple_get_n_fields(entry) - 1));
}

/** The following function returns the number of allocated elements
 for an array of offsets.
 @return number of elements */
[[nodiscard]] static inline ulint rec_offs_get_n_alloc(
    const ulint *offsets) /*!< in: array for rec_get_offsets() */
{
  ulint n_alloc;
  ut_ad(offsets);
  n_alloc = offsets[0];
  ut_ad(n_alloc > REC_OFFS_HEADER_SIZE);
  UNIV_MEM_ASSERT_W(offsets, n_alloc * sizeof *offsets);
  return (n_alloc);
}

/** The following function sets the number of allocated elements
 for an array of offsets. */
static inline void rec_offs_set_n_alloc(
    ulint *offsets, /*!< out: array for rec_get_offsets(),
                    must be allocated */
    ulint n_alloc)  /*!< in: number of elements */
{
  ut_ad(offsets);
  ut_ad(n_alloc > REC_OFFS_HEADER_SIZE);
  UNIV_MEM_ASSERT_AND_ALLOC(offsets, n_alloc * sizeof *offsets);
  offsets[0] = n_alloc;
}

/** The following function sets the number of fields in offsets. */
static inline void rec_offs_set_n_fields(
    ulint *offsets, /*!< in/out: array returned by
                    rec_get_offsets() */
    ulint n_fields) /*!< in: number of fields */
{
  ut_ad(offsets);
  ut_ad(n_fields > 0);
  ut_ad(n_fields <= REC_MAX_N_FIELDS);
  ut_ad(n_fields + REC_OFFS_HEADER_SIZE <= rec_offs_get_n_alloc(offsets));
  offsets[1] = n_fields;
}

/** The following function returns the number of fields in a record.
 @return number of fields */
[[nodiscard]] static inline ulint rec_offs_n_fields(
    const ulint *offsets) /*!< in: array returned by rec_get_offsets() */
{
  ulint n_fields;
  ut_ad(offsets);
  n_fields = offsets[1];
  ut_ad(n_fields > 0);
  ut_ad(n_fields <= REC_MAX_N_FIELDS);
  ut_ad(n_fields + REC_OFFS_HEADER_SIZE <= rec_offs_get_n_alloc(offsets));
  return (n_fields);
}

/** Determine the offset of a specified field in the record, when this
field is a field added after an instant ADD COLUMN
@param[in]      index   Clustered index where the record resides
@param[in]      n       Nth field to get offset
@param[in]      offs    Last offset before current field
@return The offset of the specified field */
static inline uint64_t rec_get_instant_offset(const dict_index_t *index,
                                              ulint n, uint64_t offs) {
  ut_ad(index->has_instant_cols_or_row_versions());

  ulint length;
  index->get_nth_default(n, &length);

  if (length == UNIV_SQL_NULL) {
    return (offs | REC_OFFS_SQL_NULL);
  } else {
    return (offs | REC_OFFS_DEFAULT);
  }
}

/** The following function determines the offsets to each field in the record.
The offsets are written to a previously allocated array of ulint, where
rec_offs_n_fields(offsets) has been initialized to the number of fields in the
record. The rest of the array will be initialized by this function.
- rec_offs_base(offsets)[0] will be set to the extra size
  (if REC_OFFS_COMPACT is set, the record is in the new format;
   if REC_OFFS_EXTERNAL is set, the record contains externally stored columns).
- rec_offs_base(offsets)[1..n_fields] will be set to offsets past the end of
  fields 0..n_fields, or to the beginning of fields 1..n_fields+1.
- If the high-order bit of the offset at [i+1] is set (REC_OFFS_SQL_NULL),
  the field i is NULL.
- If the second high-order bit of the offset at [i+1] is set
(REC_OFFS_EXTERNAL), the field i is being stored externally.
@param[in]      rec     physical record
@param[in]      index   record descriptor
@param[in,out]  offsets array of offsets */
void rec_init_offsets(const rec_t *rec, const dict_index_t *index,
                      ulint *offsets);

#ifdef UNIV_DEBUG
/** Validates offsets returned by rec_get_offsets().
 @return true if valid */
[[nodiscard]] static inline bool rec_offs_validate(
    const rec_t *rec,          /*!< in: record or NULL */
    const dict_index_t *index, /*!< in: record descriptor or NULL */
    const ulint *offsets)      /*!< in: array returned by
                               rec_get_offsets() */
{
  ulint i = rec_offs_n_fields(offsets);
  ulint last = ULINT_MAX;
  ulint comp = *rec_offs_base(offsets) & REC_OFFS_COMPACT;

  if (rec) {
    /* The offsets array might be:
    - specific to the `rec`, in which case its address is stored in offsets[2],
    - cached and shared by many records, in which case we've passed rec=nullptr
      when preparing the offsets array.
    We use caching only for the ROW_FORMAT=COMPACT format. */
    ut_ad((ulint)rec == offsets[2] || ((ulint) nullptr == offsets[2] &&
                                       offsets == index->rec_cache.offsets));
    if (!comp && index != nullptr) {
      ut_a(rec_get_n_fields_old(rec, index) >= i);
    }
  }

  if (index) {
    ulint max_n_fields;
    ut_ad((ulint)index == offsets[3]);
    ulint n_fields = dict_index_get_n_fields(index);
    ulint n_unique_in_tree = dict_index_get_n_unique_in_tree(index) + 1;
    max_n_fields = std::max(n_fields, n_unique_in_tree);
    if (!comp && rec != nullptr && rec_get_n_fields_old_raw(rec) < i) {
      ut_a(index->has_instant_cols_or_row_versions());
    }

    if (comp && rec) {
      switch (rec_get_status(rec)) {
        case REC_STATUS_ORDINARY:
          break;
        case REC_STATUS_NODE_PTR:
          max_n_fields = dict_index_get_n_unique_in_tree(index) + 1;
          break;
        case REC_STATUS_INFIMUM:
        case REC_STATUS_SUPREMUM:
          max_n_fields = 1;
          break;
        default:
          ut_error;
      }
    }
    /* index->n_def == 0 for dummy indexes if !comp */
    ut_a(!comp || index->n_def);
    ut_a(!index->n_def || i <= max_n_fields);
  }
  while (i--) {
    ulint curr = rec_offs_base(offsets)[1 + i] & REC_OFFS_MASK;
    ut_a(curr <= last);
    last = curr;
  }
  return true;
}

/** Updates debug data in offsets, in order to avoid bogus
 rec_offs_validate() failures. */
static inline void rec_offs_make_valid(
    const rec_t *rec,          /*!< in: record */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint *offsets)            /*!< in: array returned by
                               rec_get_offsets() */
{
  /* offsets are either intended for this particular rec, or to be cached */
  ut_ad(rec || offsets == index->rec_cache.offsets);
  ut_ad(index);
  ut_ad(offsets);
  ut_ad(rec == nullptr ||
        rec_get_n_fields(rec, index) >= rec_offs_n_fields(offsets));
  offsets[2] = (ulint)rec;
  offsets[3] = (ulint)index;
}

/** Check if the given two record offsets are identical.
@param[in]  offsets1  field offsets of a record
@param[in]  offsets2  field offsets of a record
@return true if they are identical, false otherwise. */
bool rec_offs_cmp(ulint *offsets1, ulint *offsets2);

/** Print the record offsets.
@param[in]    out         the output stream to which offsets are printed.
@param[in]    offsets     the field offsets of the record.
@return the output stream. */
std::ostream &rec_offs_print(std::ostream &out, const ulint *offsets);
#else
#define rec_offs_make_valid(rec, index, offsets) ((void)0)
#endif /* UNIV_DEBUG */

/** The following function tells if a new-style record is instant record.
@param[in]      rec     new-style record
@return true if it is instant affected */
static inline bool rec_get_instant_flag_new(const rec_t *rec) {
  ulint info = rec_get_info_bits(rec, true);
  return ((info & REC_INFO_INSTANT_FLAG) != 0);
}

/** The following function tells if a new-style temp record is instant record.
@param[in]      rec     new-style temp record
@return true if it is instant affected */
static inline bool rec_get_instant_flag_new_temp(const rec_t *rec) {
  ulint info = rec_get_info_bits_temp(rec);
  return ((info & REC_INFO_INSTANT_FLAG) != 0);
}

/** A simplified variant rec_init_offsets(rec, index, offsets) for the case in
which the rec contains only fixed-length columns and non-NULL values in them,
thus we can compute the offsets without looking at the rec. Such offsets can be
cached and reused for many recs which don't contain NULLs.
@see rec_init_offsets for more details
@param[in]     index The index for which we want to cache the fixed offsets
@param[in,out] offsets The already allocated array to store the offsets.
                       It should already have been initialized with
                       rec_offs_set_n_alloc() and rec_offs_set_n_fields() before
                       the call.
                       This function will fill all the other elements. */
inline void rec_init_fixed_offsets(const dict_index_t *index, ulint *offsets) {
  rec_offs_make_valid(nullptr, index, offsets);
  rec_offs_base(offsets)[0] =
      (REC_N_NEW_EXTRA_BYTES +
       UT_BITS_IN_BYTES(index->get_instant_nullable())) |
      REC_OFFS_COMPACT;
  const auto n_fields = rec_offs_n_fields(offsets);
  auto field_end = rec_offs_base(offsets) + 1;
  for (size_t i = 0; i < n_fields; i++) {
    field_end[i] = (i ? field_end[i - 1] : 0) + index->get_field(i)->fixed_len;
  }
}

/** The following function tells if a new-style record is versioned.
@param[in]      rec     new-style (COMPACT/DYNAMIC) record
@return true if it is versioned */
static inline bool rec_new_is_versioned(const rec_t *rec) {
  ulint info = rec_get_info_bits(rec, true);
  return ((info & REC_INFO_VERSION_FLAG) != 0);
}

/** The following function tells if an old-style record is versioned.
@param[in]      rec     old-style (REDUNDANT) record
@return true if it's versioned */
static inline bool rec_old_is_versioned(const rec_t *rec) {
  ulint info = rec_get_info_bits(rec, false);
  return ((info & REC_INFO_VERSION_FLAG) != 0);
}

/** The following function tells if a temporary record is versioned.
@param[in]      rec     new-style temporary record
@return true if it's instant affected */
static inline bool rec_new_temp_is_versioned(const rec_t *rec) {
  ulint info = rec_get_info_bits_temp(rec);
  return ((info & REC_INFO_VERSION_FLAG) != 0);
}

/** Get the number of fields for one new style leaf page record.
This is only needed for table after instant ADD COLUMN.
@param[in]      rec             leaf page record
@param[in]      extra_bytes     extra bytes of this record
@param[in,out]  length          length of number of fields
@return number of fields */
static inline uint32_t rec_get_n_fields_instant(const rec_t *rec,
                                                const ulint extra_bytes,
                                                uint16_t *length) {
  uint16_t n_fields;
  const byte *ptr;

  ptr = rec - (extra_bytes + 1);

  if ((*ptr & REC_N_FIELDS_TWO_BYTES_FLAG) == 0) {
    *length = 1;
    return (*ptr);
  }

  *length = 2;
  n_fields = ((*ptr-- & REC_N_FIELDS_ONE_BYTE_MAX) << 8);
  n_fields |= *ptr;
  ut_ad(n_fields < REC_MAX_N_FIELDS);
  ut_ad(n_fields != 0);

  return (n_fields);
}

/** Determines the information about null bytes and variable length bytes
for a new-style temporary record
@param[in]      rec             physical record
@param[in]      index           index where the record resides
@param[out]     nulls           the start of null bytes
@param[out]     lens            the start of variable length bytes
@param[out]     n_null          number of null fields
@return the row version of record */
static inline uint16_t rec_init_null_and_len_temp(const rec_t *rec,
                                                  const dict_index_t *index,
                                                  const byte **nulls,
                                                  const byte **lens,
                                                  uint16_t *n_null) {
  /* Following is the format for TEMP record.
  +----+----+-------------------+--------------------+
  | OP | ES |<-- Extra info --> | F1 | F2 | ...  | Fn|
  +----+----+-------------------+--------------------+
                  |
                  v
  +--------------------+-------+---------+-----------+
  | LN | ... | L2 | L1 | nulls | version | info-bits |
  +--------------------+-------+---------+-----------+
   <------ LENS ------>

   where
    info-bits => present iff instant bit or version bit is set.
    version   => version number iff version bit is set.
  */

  uint16_t non_default_fields =
      static_cast<uint16_t>(dict_index_get_n_fields(index));

  *nulls = rec - 1;

  if (!index->has_instant_cols_or_row_versions()) {
    *n_null = index->n_nullable;
    *lens = *nulls - UT_BITS_IN_BYTES(*n_null);
    return (non_default_fields);
  }

  uint8_t row_version = 0;
  uint16_t ret = 0;

  /* info bits are copied for record with versions or instant columns.
  nulls must be before that. */
  *nulls -= REC_N_TMP_EXTRA_BYTES;

  if (rec_new_temp_is_versioned(rec)) {
    ut_ad(index->table->has_row_versions());

    /* Read the version information */
    row_version = (uint8_t)(**nulls);
    /* For temp records, we write 0 as well for row_verison */
    ut_ad(row_version <= MAX_ROW_VERSION);

    /* Skip the version info */
    *nulls -= 1;

    *n_null = index->get_nullable_in_version(row_version);
    ret = (uint16_t)row_version;
  } else if (rec_get_instant_flag_new_temp(rec)) {
    /* Row inserted after first instant ADD COLUMN V1 */
    ut_ad(index->table->has_instant_cols());
    uint16_t length;
    non_default_fields =
        rec_get_n_fields_instant(rec, REC_N_TMP_EXTRA_BYTES, &length);
    ut_ad(length == 1 || length == 2);

    *nulls -= length;
    *n_null = index->get_n_nullable_before(non_default_fields);
    ret = non_default_fields;
  } else if (index->table->has_instant_cols()) {
    /* Row inserted before first INSTANT ADD COLUMN in V1 */
    *n_null = index->get_instant_nullable();
    non_default_fields = index->get_instant_fields();
    ret = non_default_fields;
  } else {
    /* Row inserted before first INSTANT ADD/DROP in V2 */
    ut_ad(row_version == 0);
    *n_null = index->get_nullable_in_version(row_version);
    ret = (uint16_t)row_version;
  }

  *lens = *nulls - UT_BITS_IN_BYTES(*n_null);

  return (ret);
}

/** Determines the information about null bytes and variable length bytes
for a new style record
@param[in]      rec             physical record
@param[in]      index           index where the record resides
@param[out]     nulls           the start of null bytes
@param[out]     lens            the start of variable length bytes
@param[out]     n_null          number of null fields
@return number of fields with no default or the row version of record */
static inline uint16_t rec_init_null_and_len_comp(const rec_t *rec,
                                                  const dict_index_t *index,
                                                  const byte **nulls,
                                                  const byte **lens,
                                                  uint16_t *n_null) {
  uint16_t non_default_fields =
      static_cast<uint16_t>(dict_index_get_n_fields(index));

  /* Position nulls */
  *nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);

  if (!index->has_instant_cols_or_row_versions()) {
    ut_ad(!rec_get_instant_flag_new(rec));
    ut_ad(!rec_new_is_versioned(rec));

    *n_null = index->n_nullable;

    /* Position lens */
    *lens = *nulls - UT_BITS_IN_BYTES(*n_null);
    return (non_default_fields);
  }

  /* For INSTANT ADD/DROP, we may have following 5 types of rec for table :
  +----------------------------------------------------------------------------+
  |              SCENARIO                         |        STATE               |
  |----------------------------------+------------+---------+------------------|
  |            Row property          | V < 8.0.28 | Bit set | Stored on row    |
  +----------------------------------+------------+---------+------------------+
  | INSERTED before 1st INSTANT ADD  |     Y      | NONE    | N/A              |
  |----------------------------------+------------+---------+------------------|
  | INSERTED after 1st INSTANT ADD   |     Y      | INSTANT | # of fields      |
  |----------------------------------+------------+---------+------------------|
  | INSERTED before INSTANT ADD/DROP |     Y      | VERSION | Version = 0      |
  |----------------------------------+------------+---------+------------------|
  | INSERTED before INSTANT ADD/DROP |     N      | NONE    | N/A              |
  |----------------------------------+------------+---------+------------------|
  | INSERTED after INSTANT ADD/DROP  |     Y/N    | VERSION | row version      |
  +----------------------------------------------------------------------------+
  */

  /* Only one of the two bits could be set */
  ut_ad(!rec_new_is_versioned(rec) || !rec_get_instant_flag_new(rec));

  uint8_t row_version = 0;
  uint16_t ret = 0;

  if (rec_new_is_versioned(rec)) {
    ut_ad(!rec_get_instant_flag_new(rec));

    /* Read the row version from record */
    row_version = (uint8_t)(**nulls);

#ifdef UNIV_DEBUG
    if (strcmp(index->name, RECOVERY_INDEX_TABLE_NAME) != 0) {
      if (row_version > 0) {
        /* Row inserted after first instant ADD/DROP COLUMN V2 */
        ut_ad(index->table->has_row_versions());
      } else {
        /* Upgraded table. Row inserted before V2 INSTANT ADD/DROP */
        ut_ad(index->table->is_upgraded_instant());
      }
    }
#endif

    /* Reposition nulls */
    *nulls -= 1;
    *n_null = index->get_nullable_in_version(row_version);
    ret = (uint16_t)row_version;
  } else if (rec_get_instant_flag_new(rec)) {
    ut_ad(!rec_new_is_versioned(rec));
    ut_ad(index->table->has_instant_cols());

    /* Row inserted after first instant ADD COLUMN V1 */
    uint16_t length;
    non_default_fields =
        rec_get_n_fields_instant(rec, REC_N_NEW_EXTRA_BYTES, &length);
    ut_ad(length == 1 || length == 2);

    /* Reposition nulls */
    *nulls -= length;
    *n_null = index->calculate_n_instant_nullable(non_default_fields);
    ret = non_default_fields;
  } else if (index->table->has_instant_cols()) {
    /* Row inserted before first INSTANT ADD COLUMN in V1 */
    *n_null = index->get_instant_nullable();
    non_default_fields = index->get_instant_fields();
    ret = non_default_fields;
  } else { /* Not an INSTANT upgraded table */
    /* Row inserted before first INSTANT ADD/DROP in V2 */
    ut_ad(row_version == 0);
    *n_null = index->get_nullable_in_version(row_version);
    ret = (uint16_t)row_version;
  }

  /* Position lens */
  *lens = *nulls - UT_BITS_IN_BYTES(*n_null);

  /* Return record version or non_default_field stored */
  return (ret);
}

/** Determine the offset to each field in a leaf-page record in
ROW_FORMAT=COMPACT.  This is a special case of rec_init_offsets() and
rec_get_offsets().
@param[in]      rec     physical record in ROW_FORMAT=COMPACT
@param[in]      temp    whether to use the format for temporary files in index
                        creation
@param[in]      index   record descriptor
@param[in,out]  offsets array of offsets */
void rec_init_offsets_comp_ordinary(const rec_t *rec, bool temp,
                                    const dict_index_t *index, ulint *offsets);

/** The following function is used to test whether the data offsets in the
 record are stored in one-byte or two-byte format.
 @return true if 1-byte form */
[[nodiscard]] static inline bool rec_get_1byte_offs_flag(
    const rec_t *rec) /*!< in: physical record */
{
  return (rec_get_bit_field_1(rec, REC_OLD_SHORT, REC_OLD_SHORT_MASK,
                              REC_OLD_SHORT_SHIFT));
}

#endif
