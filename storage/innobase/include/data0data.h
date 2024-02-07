/*****************************************************************************

Copyright (c) 1994, 2024, Oracle and/or its affiliates.

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

/** @file include/data0data.h
 SQL data field and tuple

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

#ifndef data0data_h
#define data0data_h

#include "univ.i"

#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "mem0mem.h"
#include "trx0types.h"
#include "ut0bitset.h"

#include <ostream>

/** Storage for overflow data in a big record, that is, a clustered
index record which needs external storage of data fields */
struct big_rec_t;
struct upd_t;

#ifdef UNIV_DEBUG
/** Gets pointer to the type struct of SQL data field.
 @return pointer to the type struct */
[[nodiscard]] static inline dtype_t *dfield_get_type(
    const dfield_t *field); /*!< in: SQL data field */
/** Gets pointer to the data in a field.
 @return pointer to data */
[[nodiscard]] static inline void *dfield_get_data(
    const dfield_t *field); /*!< in: field */
#else                       /* UNIV_DEBUG */
#define dfield_get_type(field) (&(field)->type)
#define dfield_get_data(field) ((field)->data)
#endif /* UNIV_DEBUG */

/** Sets the type struct of SQL data field.
@param[in]      field   SQL data field
@param[in]      type    pointer to data type struct */
static inline void dfield_set_type(dfield_t *field, const dtype_t *type);

/** Gets length of field data.
 @return length of data; UNIV_SQL_NULL if SQL null data */
[[nodiscard]] static inline uint32_t dfield_get_len(
    const dfield_t *field); /*!< in: field */

/** Sets length in a field.
@param[in]      field   field
@param[in]      len     length or UNIV_SQL_NULL */
static inline void dfield_set_len(dfield_t *field, ulint len);

/** Determines if a field is SQL NULL
 @return nonzero if SQL null data */
[[nodiscard]] static inline ulint dfield_is_null(
    const dfield_t *field); /*!< in: field */
/** Determines if a field is externally stored
 @return nonzero if externally stored */
[[nodiscard]] static inline bool dfield_is_ext(
    const dfield_t *field); /*!< in: field */
/** Sets the "external storage" flag */
static inline void dfield_set_ext(dfield_t *field); /*!< in/out: field */

/** Gets spatial status for "external storage"
@param[in,out]  field           field */
static inline spatial_status_t dfield_get_spatial_status(const dfield_t *field);

/** Sets spatial status for "external storage"
@param[in,out]  field           field
@param[in]      spatial_status  spatial status */
static inline void dfield_set_spatial_status(dfield_t *field,
                                             spatial_status_t spatial_status);

/** Sets pointer to the data and length in a field.
@param[in]      field   field
@param[in]      data    data
@param[in]      len     length or UNIV_SQL_NULL */
static inline void dfield_set_data(dfield_t *field, const void *data,
                                   ulint len);

/** Sets pointer to the data and length in a field.
@param[in]      field   field
@param[in]      mbr     data */
static inline void dfield_write_mbr(dfield_t *field, const double *mbr);

/** Sets a data field to SQL NULL. */
static inline void dfield_set_null(dfield_t *field); /*!< in/out: field */

/** Writes an SQL null field full of zeros.
@param[in]      data    pointer to a buffer of size len
@param[in]      len     SQL null size in bytes */
static inline void data_write_sql_null(byte *data, ulint len);

/** Copies the data and len fields.
@param[out]     field1  field to copy to
@param[in]      field2  field to copy from */
static inline void dfield_copy_data(dfield_t *field1, const dfield_t *field2);

/** Copies a data field to another.
@param[out]     field1  field to copy to
@param[in]      field2  field to copy from */
static inline void dfield_copy(dfield_t *field1, const dfield_t *field2);

/** Copies the data pointed to by a data field.
@param[in,out]  field   data field
@param[in]      heap    memory heap where allocated */
static inline void dfield_dup(dfield_t *field, mem_heap_t *heap);

/** Copies the data pointed to by a data field.
This function works for multi-value fields only.
@param[in,out]  field   data field
@param[in]      heap    memory heap where allocated */
static inline void dfield_multi_value_dup(dfield_t *field, mem_heap_t *heap);

/** Determine if a field is of multi-value type
@param[in]      field   data field
@return true if multi-value type field, otherwise false */
static inline bool dfield_is_multi_value(const dfield_t *field);

/** Tests if two data fields are equal.
If len==0, tests the data length and content for equality.
If len>0, tests the first len bytes of the content for equality.
@param[in]      field1          first field to compare
@param[in]      field2          second field to compare
@param[in]      len             maximum prefix to compare,
                                or 0 to compare the whole field length.
                                This works only if !multi_val
@return true if both fields are NULL or if they are equal */
[[nodiscard]] inline bool dfield_datas_are_binary_equal(const dfield_t *field1,
                                                        const dfield_t *field2,
                                                        ulint len);
/** Tests if dfield data length and content is equal to the given.
@param[in] field Field
@param[in] len   Data length or UNIV_SQL_NULL
@param[in] data  Data
@return true if equal */
[[nodiscard]] static inline bool dfield_data_is_binary_equal(
    const dfield_t *field, ulint len, const byte *data);
/** Gets number of fields in a data tuple.
 @return number of fields */
[[nodiscard]] static inline ulint dtuple_get_n_fields(
    const dtuple_t *tuple); /*!< in: tuple */

/** Gets number of virtual fields in a data tuple.
@param[in]      tuple   dtuple to check
@return number of fields */
static inline ulint dtuple_get_n_v_fields(const dtuple_t *tuple);

#ifdef UNIV_DEBUG
/** Gets nth field of a tuple.
@param[in]      tuple   tuple
@param[in]      n       index of field
@return nth field */
static inline dfield_t *dtuple_get_nth_field(const dtuple_t *tuple, ulint n);

/** Gets nth virtual field of a tuple.
@param[in]      tuple   tuple
@param[in]      n       the nth field to get
@return nth field */
static inline dfield_t *dtuple_get_nth_v_field(const dtuple_t *tuple, ulint n);

#else /* UNIV_DEBUG */
#define dtuple_get_nth_field(tuple, n) ((tuple)->fields + (n))
#define dtuple_get_nth_v_field(tuple, n) \
  ((tuple)->fields + (tuple)->n_fields + (n))
#endif /* UNIV_DEBUG */
/** Gets info bits in a data tuple.
 @return info bits */
[[nodiscard]] static inline ulint dtuple_get_info_bits(
    const dtuple_t *tuple); /*!< in: tuple */

/** Sets info bits in a data tuple.
@param[in]      tuple           tuple
@param[in]      info_bits       info bits */
static inline void dtuple_set_info_bits(dtuple_t *tuple, ulint info_bits);

/** Gets number of fields used in record comparisons.
 @return number of fields used in comparisons in rem0cmp.* */
[[nodiscard]] static inline ulint dtuple_get_n_fields_cmp(
    const dtuple_t *tuple); /*!< in: tuple */

/** Gets number of fields used in record comparisons.
@param[in]      tuple           tuple
@param[in]      n_fields_cmp    number of fields used in comparisons in
                                rem0cmp */
static inline void dtuple_set_n_fields_cmp(dtuple_t *tuple, ulint n_fields_cmp);

/* Estimate the number of bytes that are going to be allocated when
creating a new dtuple_t object */
#define DTUPLE_EST_ALLOC(n_fields) \
  (sizeof(dtuple_t) + (n_fields) * sizeof(dfield_t))

/** Creates a data tuple from an already allocated chunk of memory.
 The size of the chunk must be at least DTUPLE_EST_ALLOC(n_fields).
 The default value for number of fields used in record comparisons
 for this tuple is n_fields.
 @param[in,out] buf             buffer to use
 @param[in]     buf_size        buffer size
 @param[in]     n_fields        number of field
 @param[in]     n_v_fields      number of fields on virtual columns
 @return created tuple (inside buf) */
[[nodiscard]] static inline dtuple_t *dtuple_create_from_mem(void *buf,
                                                             ulint buf_size,
                                                             ulint n_fields,
                                                             ulint n_v_fields);
/** Creates a data tuple to a memory heap. The default value for number
 of fields used in record comparisons for this tuple is n_fields.
 @return own: created tuple */
static inline dtuple_t *dtuple_create(
    mem_heap_t *heap, /*!< in: memory heap where the tuple
                      is created, DTUPLE_EST_ALLOC(n_fields)
                      bytes will be allocated from this heap */
    ulint n_fields)   /*!< in: number of fields */
    MY_ATTRIBUTE((malloc));

/** Initialize the virtual field data in a dtuple_t
@param[in,out]          vrow    dtuple contains the virtual fields */
static inline void dtuple_init_v_fld(const dtuple_t *vrow);

/** Duplicate the virtual field data in a dtuple_t
@param[in,out]          vrow    dtuple contains the virtual fields
@param[in]              heap    heap memory to use */
static inline void dtuple_dup_v_fld(const dtuple_t *vrow, mem_heap_t *heap);

/** Creates a data tuple with possible virtual columns to a memory heap.
@param[in]      heap            memory heap where the tuple is created
@param[in]      n_fields        number of fields
@param[in]      n_v_fields      number of fields on virtual col
@return own: created tuple */
static inline dtuple_t *dtuple_create_with_vcol(mem_heap_t *heap,
                                                ulint n_fields,
                                                ulint n_v_fields);
/** Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this.
@param[in] tuple                Tuple.
@param[in] n_fields             Number of fields. */
void dtuple_set_n_fields(dtuple_t *tuple, ulint n_fields);

/** Copies a data tuple's virtual fields to another. This is a shallow copy;
@param[in,out]  d_tuple         destination tuple
@param[in]      s_tuple         source tuple */
static inline void dtuple_copy_v_fields(dtuple_t *d_tuple,
                                        const dtuple_t *s_tuple);
/** Copies a data tuple to another.  This is a shallow copy; if a deep copy
 is desired, dfield_dup() will have to be invoked on each field.
 @return own: copy of tuple */
static inline dtuple_t *dtuple_copy(
    const dtuple_t *tuple, /*!< in: tuple to copy from */
    mem_heap_t *heap)      /*!< in: memory heap
                           where the tuple is created */
    MY_ATTRIBUTE((malloc));

/** The following function returns the sum of data lengths of a tuple. The space
occupied by the field structs or the tuple struct is not counted.
@param[in]      tuple   typed data tuple
@param[in]      comp    nonzero=ROW_FORMAT=COMPACT
@return sum of data lens */
static inline ulint dtuple_get_data_size(const dtuple_t *tuple, ulint comp);
/** Compare two data tuples.
@param[in] tuple1 first data tuple
@param[in] tuple2 second data tuple
@return whether tuple1==tuple2 */
[[nodiscard]] bool dtuple_coll_eq(const dtuple_t *tuple1,
                                  const dtuple_t *tuple2);

/** Compute a hash value of a prefix of an index record.
@param[in]      tuple           index record
@param[in]      n_fields        number of fields to include
@param[in]      n_bytes         number of bytes to hash in the last field
@param[in]      hash_value      hash value of the index identifier
@return the hashed value */
[[nodiscard]] static inline uint64_t dtuple_hash(const dtuple_t *tuple,
                                                 ulint n_fields, ulint n_bytes,
                                                 uint64_t hash_value);

/** Sets types of fields binary in a tuple.
@param[in]      tuple   data tuple
@param[in]      n       number of fields to set */
static inline void dtuple_set_types_binary(dtuple_t *tuple, ulint n);

/** Checks if a dtuple contains an SQL null value.
 @return true if some field is SQL null */
[[nodiscard]] static inline bool dtuple_contains_null(
    const dtuple_t *tuple); /*!< in: dtuple */
/** Checks that a data field is typed. Asserts an error if not.
 @return true if ok */
[[nodiscard]] bool dfield_check_typed(
    const dfield_t *field); /*!< in: data field */
/** Checks that a data tuple is typed. Asserts an error if not.
 @return true if ok */
[[nodiscard]] bool dtuple_check_typed(const dtuple_t *tuple); /*!< in: tuple */
#ifdef UNIV_DEBUG
/** Validates the consistency of a tuple which must be complete, i.e,
 all fields must have been set.
 @return true if ok */
[[nodiscard]] bool dtuple_validate(const dtuple_t *tuple); /*!< in: tuple */
#endif                                                     /* UNIV_DEBUG */
/** Pretty prints a dfield value according to its data type. Also the hex string
 is printed if a string contains non-printable characters. */
void dfield_print_also_hex(const dfield_t *dfield); /*!< in: dfield */

/** The following function prints the contents of a tuple.
@param[in,out] f                Output stream.
@param[in] tuple                Tuple to print. */
void dtuple_print(FILE *f, const dtuple_t *tuple);

/** Print the contents of a tuple.
@param[out]     o       output stream
@param[in]      field   array of data fields
@param[in]      n       number of data fields */
void dfield_print(std::ostream &o, const dfield_t *field, ulint n);

/** Print the contents of a tuple.
@param[out]     o       output stream
@param[in]      tuple   data tuple */
void dtuple_print(std::ostream &o, const dtuple_t *tuple);

/** Print the contents of a tuple.
@param[out]     o       output stream
@param[in]      tuple   data tuple */
inline std::ostream &operator<<(std::ostream &o, const dtuple_t &tuple) {
  dtuple_print(o, &tuple);
  return (o);
}

/** Moves parts of long fields in entry to the big record vector so that
the size of tuple drops below the maximum record size allowed in the
database. Moves data only from those fields which are not necessary
to determine uniquely the insertion place of the tuple in the index.
@param[in,out] index            Index that owns the record.
@param[in,out] upd              Update vector.
@param[in,out] entry            Index entry.
@return own: created big record vector, NULL if we are not able to
shorten the entry enough, i.e., if there are too many fixed-length or
short fields in entry or the index is clustered */
[[nodiscard]] big_rec_t *dtuple_convert_big_rec(dict_index_t *index, upd_t *upd,
                                                dtuple_t *entry)
    MY_ATTRIBUTE((malloc));

/** Puts back to entry the data stored in vector. Note that to ensure the
fields in entry can accommodate the data, vector must have been created
from entry with dtuple_convert_big_rec.
@param[in] entry                Entry whose data was put to vector.
@param[in] vector               Big rec vector; it is freed in this function*/
void dtuple_convert_back_big_rec(dtuple_t *entry, big_rec_t *vector);

/** Frees the memory in a big rec vector. */
static inline void dtuple_big_rec_free(
    big_rec_t *vector); /*!< in, own: big rec vector; it is
                freed in this function */

/*######################################################################*/

/** Structure to hold number of multiple values */
struct multi_value_data {
 public:
  /** points to different value */
  const void **datap;

  /** each individual value length */
  uint32_t *data_len;

  /** convert buffer if the data is an integer */
  uint64_t *conv_buf;

  /** number of values */
  uint32_t num_v;

  /** number of pointers allocated */
  uint32_t num_alc;

  /** Bitset to indicate which data should be handled for current data
  array. This is mainly used for UPDATE case. UPDATE may not need
  to delete all old values and insert all new values because there could
  be some same values in both old and new data array.
  If current data array is for INSERT and DELETE, this can(should) be
  nullptr since all values in current array should be handled in these
  two cases. */
  Bitset *bitset;

  /** Allocate specified number of elements for all arrays and initialize
  the structure accordingly
  @param[in]            num     number of elements to allocate
  @param[in]            bitset  true if memory for bitset should be
                                allocated too
  @param[in,out]        heap    memory heap */
  void alloc(uint32_t num, bool bitset, mem_heap_t *heap);

  /** Allocate the bitset for current data array
  @param[in,out]        heap    memory heap
  @param[in]            size    size of the bitset, if 0 or default,
                                the size would be num_v */
  void alloc_bitset(mem_heap_t *heap, uint32_t size = 0);

  /** Check if two multi_value_data are equal or not, regardless of bitset
  @param[in]    multi_value     Another multi-value data to be compared
  @return true if two data structures are equal, otherwise false */
  bool equal(const multi_value_data *multi_value) const {
    if (num_v != multi_value->num_v) {
      return (false);
    }

    for (uint32_t i = 0; i < num_v; ++i) {
      if (data_len[i] != multi_value->data_len[i] ||
          memcmp(datap[i], multi_value->datap[i], data_len[i]) != 0) {
        return (false);
      }
    }

    return (true);
  }

  /** Copy a multi_value_data structure
  @param[in]            multi_value     multi_value structure to copy from
  @param[in,out]        heap            memory heap */
  void copy(const multi_value_data *multi_value, mem_heap_t *heap) {
    if (num_alc < multi_value->num_v) {
      alloc(multi_value->num_v, (multi_value->bitset != nullptr), heap);
    }

    copy_low(multi_value, heap);
  }

  /** Compare and check if one value from dfield_t is in current data set.
  Any caller calls this function to check if one field from clustered index
  is equal to a record on multi-value index should understand that this
  function can only be used for equality comparison, rather than order
  comparison
  @param[in]    type    type of the data
  @param[in]    data    data value to compare
  @param[in]    len     length of data
  @return true if the value exists in current data set, otherwise false */
  bool has(const dtype_t *type, const byte *data, uint64_t len) const;

  /** Compare and check if one value from dfield_t is in current data set.
  Any caller calls this function to check if one field from clustered index
  is equal to a record on multi-value index should understand that this
  function can only be used for equality comparison, rather than order
  comparison
  @param[in]    mtype   mtype of data
  @param[in]    prtype  prtype of data
  @param[in]    data    data value to compare
  @param[in]    len     length of data
  @return true if the value exists in current data set, otherwise false */
  bool has(ulint mtype, ulint prtype, const byte *data, uint64_t len) const;

#ifdef UNIV_DEBUG
  /* Check if there is any duplicate data in this array.
  It is safe to assume all the data has been sorted.
  @return true if duplicate data found, otherwise false */
  bool duplicate() const {
    /* Since the array is guaranteed to be sorted, so it is fine to
    scan it sequentially and only compare current one with previous one
    if exists. */
    if (num_v > 1) {
      for (uint32_t i = 1; i < num_v; ++i) {
        if (data_len[i] == data_len[i - 1] &&
            memcmp(datap[i], datap[i - 1], data_len[i]) == 0) {
          return (true);
        }
      }
    }
    return (false);
  }
#endif /* UNIV_DEBUG */

 private:
  /** Copy a multi_value_data structure, current one should be bigger
  or equal to the one to be copied
  @param[in]            multi_value     multi_value structure to copy from
  @param[in,out]        heap            memory heap */
  void copy_low(const multi_value_data *multi_value, mem_heap_t *heap) {
    ut_ad(num_alc >= multi_value->num_v);
    for (uint32_t i = 0; i < multi_value->num_v; ++i) {
      datap[i] =
          mem_heap_dup(heap, multi_value->datap[i], multi_value->data_len[i]);
    }
    memcpy(data_len, multi_value->data_len,
           sizeof(*data_len) * multi_value->num_v);
    memcpy(conv_buf, multi_value->conv_buf,
           sizeof(*conv_buf) * multi_value->num_v);
    if (multi_value->bitset != nullptr) {
      ut_ad(bitset != nullptr);
      *bitset = *multi_value->bitset;
    }
    num_v = multi_value->num_v;
  }

 public:
  /** default number of multiple values */
  static constexpr uint32_t s_default_allocate_num = 24;
};

/** Class to log the multi-value data and read it from the log */
class Multi_value_logger {
 public:
  /** Constructor
  @param[in]    mv_data         multi-value data structure to log
  @param[in]    field_len       multi-value data field length */
  Multi_value_logger(const multi_value_data *mv_data, uint32_t field_len)
      : m_mv_data(mv_data), m_field_len(field_len) {}

  /** Get the log length for the multi-value data
  @param[in]    precise true if precise length is needed, false if rough
                estimation is OK
  @return the total log length for the multi-value data */
  uint32_t get_log_len(bool precise) const;

  /** Log the multi-value data to specified memory
  @param[in,out]        ptr     the memory to write
  @return next to the end of the multi-value data log */
  byte *log(byte **ptr);

  /** Read the log length for the multi-value data log starting from ptr
  @param[in]    ptr     log starting from here
  @return the length of this log */
  static uint32_t read_log_len(const byte *ptr);

  /** Read the multi-value data from the ptr
  @param[in]            ptr     log starting from here
  @param[in,out]        field   multi-value data field to store the array
  @param[in,out]        heap    memory heap
  @return next to the end of the multi-value data log */
  static const byte *read(const byte *ptr, dfield_t *field, mem_heap_t *heap);

  /** Estimate how many multi-value keys at most can be accommodated into the
  log of specified size.
  @param[in]    log_size        max log size
  @param[in]    key_length      max multi-value key length, charset considered
  @param[out]   num_keys        max possible number of multi-value keys
  @return the total size of the keys, let's assume all keys are concatenated
  one by one compactly */
  static uint32_t get_keys_capacity(uint32_t log_size, uint32_t key_length,
                                    uint32_t *num_keys);

  /** Determine if the log starting from ptr is for multi-value data
  @return true if it is for multi-value data, otherwise false */
  static bool is_multi_value_log(const byte *ptr) {
    return (*ptr == s_multi_value_virtual_col_length_marker);
  }

 private:
  /** Multi-value data */
  const multi_value_data *m_mv_data;

  /** Multi-value field length */
  uint32_t m_field_len;

  /** Length of log for NULL value or no indexed value cases */
  static constexpr uint32_t s_log_length_for_null_or_empty = 2;

  /** Multi-value virtual column length marker. With this length marker,
  a multi-value virtual column undo log can be identified. Meanwhile, this
  marker should/will not conflict with any normal compressed written length
  leading byte */
  static constexpr uint8_t s_multi_value_virtual_col_length_marker = 0xFF;

  /** Multi-value virtual column length, which indicates that there is
  no value on the multi-value index. It's mapped to UNIV_NO_INDEX_VALUE */
  static constexpr uint16_t s_multi_value_no_index_value = 0x0;

  /** Multi-value virtual column length, which indicates that the field
  is NULL. It's mapped to UNIV_SQL_NULL. Since any not NULL and not no value
  multi-value data must be longer than 1 byte, so this is safe for this
  special meaning */
  static constexpr uint16_t s_multi_value_null = 0x1;

  /** The compressed length for multi-value key length logging.
  This would not be longer than 2 bytes for now, while 2 bytes can
  actually support key length of 16384 bytes. And the actual key
  length would never be longer than this */
  static constexpr uint8_t s_max_compressed_mv_key_length_size = 2;
};

/** Structure for an SQL data field */
struct dfield_t {
  void *data; /*!< pointer to data */
  bool ext;   /*!< true=externally stored, false=local */
  unsigned spatial_status : 2;
  /*!< spatial status of externally stored field
  in undo log for purge */
  unsigned len; /*!< data length; UNIV_SQL_NULL if SQL null */
  dtype_t type; /*!< type of data */

  bool is_virtual() const { return (type.is_virtual()); }

  void reset() {
    data = nullptr;
    ext = false;
    spatial_status = SPATIAL_UNKNOWN, len = 0;
  }

  /** Create a deep copy of this object
  @param[in]    heap    the memory heap in which the clone will be
                          created.
  @return       the cloned object. */
  dfield_t *clone(mem_heap_t *heap);

  byte *blobref() const;

  /** Obtain the LOB version number, if this is an externally
  stored field. */
  uint32_t lob_version() const;

  dfield_t()
      : data(nullptr), ext(0), spatial_status(0), len(0), type({0, 0, 0, 0}) {}

  /** Print the dfield_t object into the given output stream.
  @param[in]    out     the output stream.
  @return       the output stream. */
  std::ostream &print(std::ostream &out) const;

  /** Adjust and(or) set virtual column value which is read from undo
  or online DDL log
  @param[in]    vcol    virtual column definition
  @param[in]    comp    true if compact format
  @param[in]    field   virtual column value
  @param[in]    len     value length
  @param[in,out]        heap    memory heap to keep value when necessary */
  void adjust_v_data_mysql(const dict_v_col_t *vcol, bool comp,
                           const byte *field, ulint len, mem_heap_t *heap);
};

/** Compare a multi-value clustered index field with a secondary index
field, to see if they are equal. If the clustered index field is the
array, then equal means it contains the secondary index field
@param[in]      clust_field     clustered index field
@param[in]      clust_len       clustered index field length
@param[in]      sec_field       secondary index field
@param[in]      sec_len         secondary index field length
@param[in]      col             the column tied to this field
@return true if they are equal, otherwise false */
bool is_multi_value_clust_and_sec_equal(const byte *clust_field,
                                        uint64_t clust_len,
                                        const byte *sec_field, uint64_t sec_len,
                                        const dict_col_t *col);

/** Overloading the global output operator to easily print the given dfield_t
object into the given output stream.
@param[in]      out     the output stream
@param[in]      obj     the given object to print.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const dfield_t &obj) {
  return (obj.print(out));
}

#ifdef UNIV_DEBUG
/** Value of dtuple_t::magic_n */
constexpr uint32_t DATA_TUPLE_MAGIC_N = 65478679;
#endif

/** Structure for an SQL data tuple of fields (logical record) */
struct dtuple_t {
  /** info bits of an index record: the default is 0; this field is used if an
  index record is built from a data tuple */
  uint16_t info_bits;

  /** Number of fields in dtuple */
  uint16_t n_fields;

  /** number of fields which should be used in comparison services of rem0cmp.*;
  the index search is performed by comparing only these fields, others are
  ignored; the default value in dtuple creation is the same value as n_fields */
  uint16_t n_fields_cmp;

  /** Fields. */
  dfield_t *fields;

  /** Number of virtual fields. */
  uint16_t n_v_fields;

  /** Fields on virtual column */
  dfield_t *v_fields;

  /** Data tuples can be linked into a list using this field */
  UT_LIST_NODE_T(dtuple_t) tuple_list;

#ifdef UNIV_DEBUG
  /** Memory heap where this tuple is allocated. */
  mem_heap_t *m_heap{};

  /** Value of dtuple_t::magic_n */
  static constexpr size_t MAGIC_N = 614679;

  /** Magic number, used in debug assertions */
  size_t magic_n{MAGIC_N};
#endif /* UNIV_DEBUG */

  /** Print the tuple to the output stream.
  @param[in,out] out            Stream to output to.
  @return stream */
  std::ostream &print(std::ostream &out) const {
    dtuple_print(out, this);
    return out;
  }

  /** Read the trx id from the tuple (DB_TRX_ID)
  @return transaction id of the tuple. */
  trx_id_t get_trx_id() const;

  /** Ignore at most n trailing default fields if this is a tuple
  from instant index
  @param[in]    index   clustered index object for this tuple */
  void ignore_trailing_default(const dict_index_t *index);

  /** Compare a data tuple to a physical record.
  @param[in]    rec             record
  @param[in]    index           index
  @param[in]    offsets         rec_get_offsets(rec)
  @param[in,out]        matched_fields  number of completely matched fields
  @return the comparison result of dtuple and rec
  @retval 0 if dtuple is equal to rec
  @retval negative if dtuple is less than rec
  @retval positive if dtuple is greater than rec */
  int compare(const rec_t *rec, const dict_index_t *index, const ulint *offsets,
              ulint *matched_fields) const;

  /** Compare a data tuple to a physical record.
  @param[in]    rec             record
  @param[in]    index           index
  @param[in]    offsets         rec_get_offsets(rec)
  @return the comparison result of dtuple and rec
  @retval 0 if dtuple is equal to rec
  @retval negative if dtuple is less than rec
  @retval positive if dtuple is greater than rec */
  inline int compare(const rec_t *rec, const dict_index_t *index,
                     const ulint *offsets) const {
    ulint matched_fields{};

    return (compare(rec, index, offsets, &matched_fields));
  }

  /** Get number of externally stored fields.
  @retval number of externally stored fields. */
  inline size_t get_n_ext() const {
    size_t n_ext = 0;
    for (uint32_t i = 0; i < n_fields; ++i) {
      if (dfield_is_ext(&fields[i])) {
        ++n_ext;
      }
    }
    return n_ext;
  }

  /** Set the flag REC_INFO_MIN_REC_FLAG in the info bits. */
  void set_min_rec_flag();

  /** Unset the flag REC_INFO_MIN_REC_FLAG in the info bits. */
  void unset_min_rec_flag();

  /** Does tuple has externally stored fields.
  @retval true if there is externally stored fields. */
  inline bool has_ext() const {
    for (uint32_t i = 0; i < n_fields; ++i) {
      if (dfield_is_ext(&fields[i])) {
        return true;
      }
    }
    return false;
  }

  dtuple_t *deep_copy(mem_heap_t *heap) const;
};

/** A slot for a field in a big rec vector */
struct big_rec_field_t {
  /** Constructor.
  @param[in]    field_no_       the field number
  @param[in]    len_            the data length
  @param[in]    data_           the data */
  big_rec_field_t(ulint field_no_, ulint len_, void *data_)
      : field_no(field_no_),
        len(len_),
        data(data_),
        ext_in_old(false),
        ext_in_new(false) {}

  byte *ptr() const { return (static_cast<byte *>(data)); }

  ulint field_no; /*!< field number in record */
  ulint len;      /*!< stored data length, in bytes */
  void *data;     /*!< stored data */

  /** If true, this field was stored externally in the old row.
  If false, this field was stored inline in the old row.*/
  bool ext_in_old;

  /** If true, this field is stored externally in the new row.
  If false, this field is stored inline in the new row.*/
  bool ext_in_new;

  /** Print the big_rec_field_t object into the given output stream.
  @param[in]    out     the output stream.
  @return       the output stream. */
  std::ostream &print(std::ostream &out) const;
};

/** Overloading the global output operator to easily print the given
big_rec_field_t object into the given output stream.
@param[in]      out     the output stream
@param[in]      obj     the given object to print.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const big_rec_field_t &obj) {
  return (obj.print(out));
}

/** Storage format for overflow data in a big record, that is, a
clustered index record which needs external storage of data fields */
struct big_rec_t {
  mem_heap_t *heap;        /*!< memory heap from which
                           allocated */
  const ulint capacity;    /*!< fields array size */
  ulint n_fields;          /*!< number of stored fields */
  big_rec_field_t *fields; /*!< stored fields */

  /** Constructor.
  @param[in]    max     the capacity of the array of fields. */
  explicit big_rec_t(const ulint max)
      : heap(nullptr), capacity(max), n_fields(0), fields(nullptr) {}

  /** Append one big_rec_field_t object to the end of array of fields */
  void append(const big_rec_field_t &field) {
    ut_ad(n_fields < capacity);
    fields[n_fields] = field;
    n_fields++;
  }

  /** Allocate a big_rec_t object in the given memory heap, and for
  storing n_fld number of fields.
  @param[in]    heap    memory heap in which this object is allocated
  @param[in]    n_fld   maximum number of fields that can be stored in
                  this object
  @return the allocated object */
  static big_rec_t *alloc(mem_heap_t *heap, ulint n_fld);

  /** Print the current object into the given output stream.
  @param[in]    out     the output stream.
  @return       the output stream. */
  std::ostream &print(std::ostream &out) const;
};

/** Overloading the global output operator to easily print the given
big_rec_t object into the given output stream.
@param[in]      out     the output stream
@param[in]      obj     the given object to print.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const big_rec_t &obj) {
  return (obj.print(out));
}

#include "data0data.ic"

#endif
