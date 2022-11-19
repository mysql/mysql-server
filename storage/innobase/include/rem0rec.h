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

/** @file include/rem0rec.h
 Record manager

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

#ifndef rem0rec_h
#define rem0rec_h

#include <ostream>
#include <sstream>

#include "univ.i"

#include "data0data.h"
#include "mtr0types.h"
#include "page0types.h"
#include "rem/rec.h"
#include "rem0types.h"
#include "trx0types.h"
#include "ut0class_life_cycle.h"

#include "rem0wrec.h"

/** The following function is used to get the pointer of the next chained record
on the same page.
@param[in] rec  Physical record.
@param[in] comp Nonzero=compact page format.
@return pointer to the next chained record, or nullptr if none */
[[nodiscard]] static inline const rec_t *rec_get_next_ptr_const(
    const rec_t *rec, ulint comp);
/** The following function is used to get the pointer of the next chained record
on the same page.
@param[in] rec  Physical record.
@param[in] comp Nonzero=compact page format.
@return pointer to the next chained record, or nullptr if none */
[[nodiscard]] static inline rec_t *rec_get_next_ptr(rec_t *rec, ulint comp);
/** The following function is used to get the offset of the
next chained record on the same page.
@param[in] rec  Physical record.
@param[in] comp Nonzero=compact page format.
@return the page offset of the next chained record, or 0 if none */
[[nodiscard]] static inline ulint rec_get_next_offs(const rec_t *rec,
                                                    ulint comp);

/** The following function is used to set the next record offset field of an
old-style record.
@param[in]      rec     old-style physical record
@param[in]      next    offset of the next record */
static inline void rec_set_next_offs_old(rec_t *rec, ulint next);

/** The following function is used to set the next record offset field of a
new-style record. */
static inline void rec_set_next_offs_new(rec_t *rec, ulint next);

/** The following function is used to get the number of records owned by the
 previous directory record.
 @return number of owned records */
[[nodiscard]] static inline ulint rec_get_n_owned_old(
    const rec_t *rec); /*!< in: old-style physical record */

/** The following function is used to set the number of owned records.
@param[in]      rec             old-style physical record
@param[in]      n_owned         the number of owned */
static inline void rec_set_n_owned_old(rec_t *rec, ulint n_owned);

/** The following function is used to get the number of records owned by the
 previous directory record.
 @return number of owned records */
[[nodiscard]] static inline ulint rec_get_n_owned_new(
    const rec_t *rec); /*!< in: new-style physical record */

/** The following function is used to set the number of owned records.
@param[in,out]  rec             new-style physical record
@param[in,out]  page_zip        compressed page, or NULL
@param[in]      n_owned         the number of owned */
static inline void rec_set_n_owned_new(rec_t *rec, page_zip_des_t *page_zip,
                                       ulint n_owned);

/** The following function is used to set the info bits of a record.
@param[in]      rec     old-style physical record
@param[in]      bits    info bits */
static inline void rec_set_info_bits_old(rec_t *rec, ulint bits);

/** The following function is used to set the info bits of a record.
@param[in,out]  rec     new-style physical record
@param[in]      bits    info bits */
static inline void rec_set_info_bits_new(rec_t *rec, ulint bits);

/** The following function is used to set the status bits of a new-style record.
@param[in,out]  rec     physical record
@param[in]      bits    info bits */
static inline void rec_set_status(rec_t *rec, ulint bits);

/** The following function is used to retrieve the info and status
bits of a record.  (Only compact records have status bits.)
@param[in] rec  Physical record.
@param[in] comp Nonzero=compact page format.
@return info bits */
[[nodiscard]] static inline ulint rec_get_info_and_status_bits(const rec_t *rec,
                                                               bool comp);

/** The following function is used to set the info and status bits of a record.
(Only compact records have status bits.)
@param[in,out]  rec     compact physical record
@param[in]      bits    info bits */
static inline void rec_set_info_and_status_bits(rec_t *rec, ulint bits);

/** The following function tells if record is delete marked.
@param[in] rec Physical record.
@param[in] comp Nonzero=compact page format.
@return nonzero if delete marked */
[[nodiscard]] static inline bool rec_get_deleted_flag(const rec_t *rec,
                                                      bool comp);

/** The following function is used to set the deleted bit.
@param[in]      rec             old-style physical record
@param[in]      flag            true if delete marked */
static inline void rec_set_deleted_flag_old(rec_t *rec, bool flag);

/** The following function is used to set the deleted bit.
@param[in,out]  rec             new-style physical record
@param[in,out]  page_zip        compressed page, or NULL
@param[in]      flag            true if delete marked */
static inline void rec_set_deleted_flag_new(rec_t *rec,
                                            page_zip_des_t *page_zip,
                                            bool flag);

/** The following function is used to set the instant bit.
@param[in,out]  rec     new-style physical record */
static inline void rec_new_set_instant(rec_t *rec);

/** The following function is used to set the row version bit.
@param[in,out]  rec     new-style (COMPACT/DYNAMIC) physical record */
static inline void rec_new_set_versioned(rec_t *rec);

/** The following function is used to reset the instant bit and the row version
bit.
@param[in,out]  rec     new-style (COMPACT/DYNAMIC) physical record */
static inline void rec_new_reset_instant_version(rec_t *rec);

/** The following function is used to set the instant bit.
@param[in,out]  rec     old-style (REDUNDANT) physical record
@param[in]      flag    set the bit to this flag */
static inline void rec_old_set_versioned(rec_t *rec, bool flag);

/** The following function tells if a new-style record is a node pointer.
 @return true if node pointer */
[[nodiscard]] static inline bool rec_get_node_ptr_flag(
    const rec_t *rec); /*!< in: physical record */

/** The following function is used to get the order number of an old-style
record in the heap of the index page.
@param[in]      rec     physical record
@return heap order number */
[[nodiscard]] static inline ulint rec_get_heap_no_old(const rec_t *rec);

/** The following function is used to set the heap number field in an old-style
record.
@param[in]      rec     physical record
@param[in]      heap_no the heap number */
static inline void rec_set_heap_no_old(rec_t *rec, ulint heap_no);

/** The following function is used to get the order number of a new-style
record in the heap of the index page.
@param[in]      rec     physical record
@return heap order number */
[[nodiscard]] static inline ulint rec_get_heap_no_new(const rec_t *rec);

/** The following function is used to set the heap number field in a new-style
record.
@param[in,out]  rec     physical record
@param[in]      heap_no the heap number */
static inline void rec_set_heap_no_new(rec_t *rec, ulint heap_no);

/** The following function is used to set the 1-byte offsets flag.
@param[in]      rec     physical record
@param[in]      flag    true if 1byte form */
static inline void rec_set_1byte_offs_flag(rec_t *rec, bool flag);

/** Determine how many of the first n columns in a compact
physical record are stored externally.
@param[in]      rec     compact physical record
@param[in]      index   record descriptor
@param[in]      n       number of columns to scan
@return number of externally stored columns */
[[nodiscard]] ulint rec_get_n_extern_new(const rec_t *rec,
                                         const dict_index_t *index, ulint n);

/** Gets the value of the specified field in the record in old style.
This is only used for record from instant index, which is clustered
index and has some instantly added columns.
@param[in]      rec     physical record
@param[in]      n       index of the field
@param[in]      index   clustered index where the record resides
@param[out]     len     length of the field, UNIV_SQL if SQL null
@return value of the field, could be either pointer to rec or default value */
static inline const byte *rec_get_nth_field_old_instant(
    const rec_t *rec, uint16_t n, const dict_index_t *index, ulint *len);

/** Gets the value of the specified field in the record.
This is only used when there is possibility that the record comes from the
clustered index, which has some instantly added columns.
@param[in]      rec     physical record
@param[in]      offsets array returned by rec_get_offsets()
@param[in]      n       index of the field
@param[in]      index   clustered index where the record resides, or nullptr
                        if the record doesn't have instantly added columns
                        for sure
@param[out]     len     length of the field, UNIV_SQL_NULL if SQL null
@return value of the field, could be either pointer to rec or default value */
static inline const byte *rec_get_nth_field_instant(const rec_t *rec,
                                                    const ulint *offsets,
                                                    ulint n,
                                                    const dict_index_t *index,
                                                    ulint *len);

/** Determine if the field is not NULL and not having default value
after instant ADD COLUMN
@param[in]      len             length of a field
@return true if not NULL and not having default value */
static inline bool rec_field_not_null_not_add_col_def(ulint len);

/** Determine if the offsets are for a record in the new compact format.
@param[in]      offsets         array returned by rec_get_offsets()
@return nonzero if compact format */
[[nodiscard]] static inline bool rec_offs_comp(const ulint *offsets);

/** Determine if the offsets are for a record containing externally stored
columns.
@param[in]      offsets         array returned by rec_get_offsets()
@return nonzero if externally stored */
[[nodiscard]] static inline bool rec_offs_any_extern(const ulint *offsets);

/** Determine if the offsets are for a record containing null BLOB pointers.
@param[in]      index           record descriptor
@param[in]      rec             record
@param[in]      offsets         array returned by rec_get_offsets()
@return first field containing a null BLOB pointer, or NULL if none found */
[[nodiscard]] static inline const byte *rec_offs_any_null_extern(
    const dict_index_t *index, const rec_t *rec, const ulint *offsets);

/** Returns the number of extern bits set in a record.
@param[in]      index           record descriptor
@param[in]      offsets         array returned by rec_get_offsets()
@return number of externally stored fields */
[[nodiscard]] static inline ulint rec_offs_n_extern(const dict_index_t *index,
                                                    const ulint *offsets);

#define rec_offs_init(offsets) \
  rec_offs_set_n_alloc(offsets, (sizeof offsets) / sizeof *offsets)

/**
A helper RAII wrapper for otherwise difficult to use sequence of:

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  rec_offs_init(offsets_);
  mem_heap_t *heap = nullptr;

  const ulint *offsets =
      rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap);

  DO_SOMETHING(offsets);

  if (heap != nullptr) {
    mem_heap_free(heap);
  }

With this helper you can simply do:

  DO_SOMETHING(Rec_offsets().compute(rec,index));

And if you need to reuse the memory allocated offsets several times you can:
  Rec_offsets offsets;
  for(rec: recs) DO_SOMTHING(offsets.compute(rec,index))
*/
class Rec_offsets : private ut::Non_copyable {
 public:
  /** Prepares offsets to initially point to the fixed-size buffer, and marks
  the memory as allocated, but uninitialized. You first need to call compute()
  to use it */
  Rec_offsets() { rec_offs_init(m_preallocated_buffer); }

  /** Computes offsets for given record. Returned array is owned by this
  instance. You can use its value as long as this object does not go out of
  scope (which can free the buffer), and you don't call compute() again (which
  can overwrite the offsets).
  @param[in]  rec      The record for which you want to compute the offsets
  @param[in]  index    The index which contains the record
  @param[in]  n_fields Number of columns to scan
  @return A pointer to offsets array owned by this instance. Valid till next
  call to compute() or end of this instance lifetime.
  */
  const ulint *compute(const rec_t *rec, const dict_index_t *index,
                       const ulint n_fields = ULINT_UNDEFINED) {
    m_offsets = rec_get_offsets(rec, index, m_offsets, n_fields,
                                UT_LOCATION_HERE, &m_heap);
    return m_offsets;
  }
  /** Deallocated dynamically allocated memory, if any. */
  ~Rec_offsets() {
    if (m_heap) {
      mem_heap_free(m_heap);
      m_heap = nullptr;
    }
  }

 private:
  /** Pointer to heap used by rec_get_offsets(). Initially nullptr. If row is
  really big, rec_get_offsets() may need to allocate new buffer for offsets.
  At, first, when heap is null, rec_get_offsets() will create new heap, and pass
  it back via reference. On subsequent calls, we will pass this heap, so it
  is reused if needed. Therefore all allocated buffers are in this heap, if it
  is not nullptr */
  mem_heap_t *m_heap{nullptr};

  /** Buffer with size large enough to handle common cases without having to use
  heap. This is the initial value of m_offsets.*/
  ulint m_preallocated_buffer[REC_OFFS_NORMAL_SIZE];

  /* Initially points to m_preallocated_buffer (which is uninitialized memory).
  After each call to compute() contains the pointer to the most recently
  computed offsets.
  We pass it back to rec_get_offsets() on subsequent calls to compute() to reuse
  the same memory if possible. */
  ulint *m_offsets{m_preallocated_buffer};
};

/** The following function returns the data size of a physical
record, that is the sum of field lengths. SQL null fields
are counted as length 0 fields. The value returned by the function
is the distance from record origin to record end in bytes.
@param[in]      offsets array returned by rec_get_offsets()
@return size */
[[nodiscard]] static inline ulint rec_offs_data_size(const ulint *offsets);

/** Returns the total size of record minus data size of record.
The value returned by the function is the distance from record
start to record origin in bytes.
@param[in]      offsets array returned by rec_get_offsets()
@return size */
[[nodiscard]] static inline ulint rec_offs_extra_size(const ulint *offsets);

/** Returns the total size of a physical record.
@param[in]      offsets array returned by rec_get_offsets()
@return size */
[[nodiscard]] static inline ulint rec_offs_size(const ulint *offsets);

#ifdef UNIV_DEBUG
/** Returns a pointer to the start of the record.
@param[in]      rec     pointer to record
@param[in]      offsets array returned by rec_get_offsets()
@return pointer to start */
[[nodiscard]] static inline byte *rec_get_start(const rec_t *rec,
                                                const ulint *offsets);

/** Returns a pointer to the end of the record.
@param[in]      rec     pointer to record
@param[in]      offsets array returned by rec_get_offsets()
@return pointer to end */
[[nodiscard]] static inline byte *rec_get_end(const rec_t *rec,
                                              const ulint *offsets);
#else /* UNIV_DEBUG */
#define rec_get_start(rec, offsets) ((rec)-rec_offs_extra_size(offsets))
#define rec_get_end(rec, offsets) ((rec) + rec_offs_data_size(offsets))
#endif /* UNIV_DEBUG */

/** Copy a physical record to a buffer.
@param[in]      buf     buffer
@param[in]      rec     physical record
@param[in]      offsets array returned by rec_get_offsets()
@return pointer to the origin of the copy */
static inline rec_t *rec_copy(void *buf, const rec_t *rec,
                              const ulint *offsets);

#ifndef UNIV_HOTBACKUP
/** Determines the size of a data tuple prefix in a temporary file.
@param[in]      index           record descriptor
@param[in]      fields          array of data fields
@param[in]      n_fields        number of data fields
@param[in]      v_entry         dtuple contains virtual column data
@param[out]     extra           extra size
@param[in]      rec_version     row version of record
@return total size */
[[nodiscard]] ulint rec_get_serialize_size(const dict_index_t *index,
                                           const dfield_t *fields,
                                           ulint n_fields,
                                           const dtuple_t *v_entry,
                                           ulint *extra, uint8_t rec_version);

/** Determine the offset to each field in temporary file.
@param[in]      rec     temporary file record
@param[in]      index   record descriptor
@param[in,out]  offsets array of offsets
 @see rec_serialize_dtuple() */
void rec_deserialize_init_offsets(const rec_t *rec, const dict_index_t *index,
                                  ulint *offsets);

/** Builds a temporary file record out of a data tuple.
@param[out]     rec             record
@param[in]      index           record descriptor
@param[in]      fields          array of data fields
@param[in]      n_fields        number of fields
@param[in]      v_entry         dtuple contains virtual column data
@param[in]      rec_version     rec version
@see rec_deserialize_init_offsets() */
void rec_serialize_dtuple(rec_t *rec, const dict_index_t *index,
                          const dfield_t *fields, ulint n_fields,
                          const dtuple_t *v_entry, uint8_t rec_version);

/** Copies the first n fields of a physical record to a new physical record in
a buffer.
@param[in]      rec             physical record
@param[in]      index           record descriptor
@param[in]      n_fields        number of fields to copy
@param[in,out]  buf             memory buffer for the copied prefix, or NULL
@param[in,out]  buf_size        buffer size
@return own: copied record */
rec_t *rec_copy_prefix_to_buf(const rec_t *rec, const dict_index_t *index,
                              ulint n_fields, byte **buf, size_t *buf_size);

/** Compute a hash value of a prefix of a leaf page record.
@param[in]      rec             leaf page record
@param[in]      offsets         rec_get_offsets(rec)
@param[in]      n_fields        number of complete fields to hash
@param[in]      n_bytes         number of bytes to hash in the last field
@param[in]      hashed_value    hash value of the index identifier
@param[in]      index           index where the record resides
@return the hashed value */
[[nodiscard]] static inline uint64_t rec_hash(const rec_t *rec,
                                              const ulint *offsets,
                                              ulint n_fields, ulint n_bytes,
                                              uint64_t hashed_value,
                                              const dict_index_t *index);
#endif /* !UNIV_HOTBACKUP */

/** Builds a physical record out of a data tuple and stores it into the given
buffer.
@param[in]      buf     start address of the physical record
@param[in]      index   record descriptor
@param[in]      dtuple  data tuple
@return pointer to the origin of physical record */
[[nodiscard]] rec_t *rec_convert_dtuple_to_rec(byte *buf,
                                               const dict_index_t *index,
                                               const dtuple_t *dtuple);

/** Returns the extra size of an old-style physical record if we know its
 data size and number of fields.
 @param[in] data_size   data size
 @param[in] n_fields    number of fields
 @param[in] has_ext     true if tuple has ext fields
 @return extra size */
static inline ulint rec_get_converted_extra_size(ulint data_size,
                                                 ulint n_fields, bool has_ext);

/** Determines the size of a data tuple prefix in ROW_FORMAT=COMPACT.
@param[in]      index           record descriptor
@param[in]      fields          array of data fields
@param[in]      n_fields        number of data fields
@param[out]     extra           extra size
@return total size */
[[nodiscard]] ulint rec_get_converted_size_comp_prefix(
    const dict_index_t *index, const dfield_t *fields, ulint n_fields,
    ulint *extra);

/** Determines the size of a data tuple in ROW_FORMAT=COMPACT.
@param[in]      index           record descriptor; dict_table_is_comp() is
                                assumed to hold, even if it does not
@param[in]      status          status bits of the record
@param[in]      fields          array of data fields
@param[in]      n_fields        number of data fields
@param[out]     extra           extra size
@return total size */
ulint rec_get_converted_size_comp(const dict_index_t *index, ulint status,
                                  const dfield_t *fields, ulint n_fields,
                                  ulint *extra);

/** The following function returns the size of a data tuple when converted to
a physical record.
@param[in] index        record descriptor
@param[in] dtuple       data tuple
@return size */
[[nodiscard]] static inline ulint rec_get_converted_size(
    const dict_index_t *index, const dtuple_t *dtuple);

#ifndef UNIV_HOTBACKUP
/** Copies the first n fields of a physical record to a data tuple.
The fields are copied to the memory heap.
@param[out]     tuple           data tuple
@param[in]      rec             physical record
@param[in]      index           record descriptor
@param[in]      n_fields        number of fields to copy
@param[in]      heap            memory heap */
void rec_copy_prefix_to_dtuple(dtuple_t *tuple, const rec_t *rec,
                               const dict_index_t *index, ulint n_fields,
                               mem_heap_t *heap);
#endif /* !UNIV_HOTBACKUP */

/** Get the length of the number of fields for any new style record.
@param[in]      n_fields        number of fields in the record
@return length of specified number of fields */
static inline uint8_t rec_get_n_fields_length(ulint n_fields);

/** Set the row version on one new style leaf page record.
This is only needed for table after instant ADD/DROP COLUMN.
@param[in,out]  rec             leaf page record
@param[in]      version         row version */
static inline void rec_set_instant_row_version_new(rec_t *rec, uint8_t version);

/** Set the row version on one old style leaf page record.
This is only needed for table after instant ADD/DROP COLUMN.
@param[in,out]  rec             leaf page record
@param[in]      version         row version */
static inline void rec_set_instant_row_version_old(rec_t *rec, uint8_t version);

/** Validates the consistency of a physical record.
@param[in]      rec     physical record
@param[in]      offsets array returned by rec_get_offsets()
@return true if ok */
bool rec_validate(const rec_t *rec, const ulint *offsets);

/** Prints an old-style physical record.
@param[in]      file    File where to print
@param[in]      rec     Physical record */
void rec_print_old(FILE *file, const rec_t *rec);

#ifndef UNIV_HOTBACKUP

/** Prints a spatial index record.
@param[in]      file    File where to print
@param[in]      rec     Physical record
@param[in]      offsets Array returned by rec_get_offsets() */
void rec_print_mbr_rec(FILE *file, const rec_t *rec, const ulint *offsets);

/** Prints a physical record.
@param[in]      file    file where to print
@param[in]      rec     physical record
@param[in]      offsets array returned by rec_get_offsets() */
void rec_print_new(FILE *file, const rec_t *rec, const ulint *offsets);

/** Prints a physical record.
@param[in]      file    File where to print
@param[in]      rec     Physical record
@param[in]      index   Record descriptor */
void rec_print(FILE *file, const rec_t *rec, const dict_index_t *index);

/** Pretty-print a record.
@param[in,out]  o       output stream
@param[in]      rec     physical record
@param[in]      info    rec_get_info_bits(rec)
@param[in]      offsets rec_get_offsets(rec) */
void rec_print(std::ostream &o, const rec_t *rec, ulint info,
               const ulint *offsets);

/** Wrapper for pretty-printing a record */
struct rec_index_print {
  /** Constructor */
  rec_index_print(const rec_t *rec, const dict_index_t *index)
      : m_rec(rec), m_index(index) {}

  /** Record */
  const rec_t *m_rec;
  /** Index */
  const dict_index_t *m_index;
};

/** Display a record.
@param[in,out]  o       output stream
@param[in]      r       record to display
@return the output stream */
std::ostream &operator<<(std::ostream &o, const rec_index_print &r);

/** Wrapper for pretty-printing a record */
struct rec_offsets_print {
  /** Constructor */
  rec_offsets_print(const rec_t *rec, const ulint *offsets)
      : m_rec(rec), m_offsets(offsets) {}

  /** Record */
  const rec_t *m_rec;
  /** Offsets to each field */
  const ulint *m_offsets;
};

/** Display a record.
@param[in,out]  o       output stream
@param[in]      r       record to display
@return the output stream */
std::ostream &operator<<(std::ostream &o, const rec_offsets_print &r);

#ifdef UNIV_DEBUG
/** Pretty-printer of records and tuples */
class rec_printer : public std::ostringstream {
 public:
  /** Construct a pretty-printed record.
  @param rec    record with header
  @param offsets        rec_get_offsets(rec, ...) */
  rec_printer(const rec_t *rec, const ulint *offsets) : std::ostringstream() {
    rec_print(*this, rec, rec_get_info_bits(rec, rec_offs_comp(offsets)),
              offsets);
  }

  /** Construct a pretty-printed record.
  @param rec record, possibly lacking header
  @param info rec_get_info_bits(rec)
  @param offsets rec_get_offsets(rec, ...) */
  rec_printer(const rec_t *rec, ulint info, const ulint *offsets)
      : std::ostringstream() {
    rec_print(*this, rec, info, offsets);
  }

  /** Construct a pretty-printed tuple.
  @param tuple  data tuple */
  rec_printer(const dtuple_t *tuple) : std::ostringstream() {
    dtuple_print(*this, tuple);
  }

  /** Construct a pretty-printed tuple.
  @param field  array of data tuple fields
  @param n      number of fields */
  rec_printer(const dfield_t *field, ulint n) : std::ostringstream() {
    dfield_print(*this, field, n);
  }

  /** Destructor */
  virtual ~rec_printer() = default;

 private:
  /** Copy constructor */
  rec_printer(const rec_printer &other);
  /** Assignment operator */
  rec_printer &operator=(const rec_printer &other);
};
#endif /* UNIV_DEBUG */

/** Reads the DB_TRX_ID of a clustered index record.
@param[in]      rec     record
@param[in]      index   clustered index
@return the value of DB_TRX_ID */
[[nodiscard]] trx_id_t rec_get_trx_id(const rec_t *rec,
                                      const dict_index_t *index);
#endif /* UNIV_HOTBACKUP */

/* Maximum lengths for the data in a physical record if the offsets
are given in one byte (resp. two byte) format. */
constexpr ulint REC_1BYTE_OFFS_LIMIT = 0x7FUL;
constexpr ulint REC_2BYTE_OFFS_LIMIT = 0x7FFFUL;

/* The data size of record must be smaller than this because we reserve
two upmost bits in a two byte offset for special purposes */
constexpr ulint REC_MAX_DATA_SIZE = 16384;

/** For a given clustered index, version is to be stored on physical record.
@param[in]  index           clustered index
@param[in]  n_tuple_fields  number of fields in tuple
@return true, if version is to be stored */
bool is_store_version(const dict_index_t *index, size_t n_tuple_fields);

/* A temp record, generated for a REDUNDANT row record, will have info bits
iff table has INSTANT ADD columns. And if record has row version, then it will
also be stored on temp record header. Following function finds the number of
more bytes needed in record header to store this info.
@param[in]  index record descriptor
@param[in]  valid_version true if record has version
@return number of bytes NULL pointer should be adjusted. */
size_t get_extra_bytes_for_temp_redundant(const dict_index_t *index,
                                          bool valid_version);
#include "rem0rec.ic"

#endif
