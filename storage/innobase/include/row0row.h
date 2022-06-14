/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file include/row0row.h
 General row routines

 Created 4/20/1996 Heikki Tuuri
 *******************************************************/

#ifndef row0row_h
#define row0row_h

#include "btr0types.h"
#include "data0data.h"
#include "dict0types.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "univ.i"

/** Gets the offset of the DB_TRX_ID field, in bytes relative to the origin of
a clustered index record.
@param[in] index Clustered index.
@param[in] offsets rec_get_offsets(rec, index).
@return offset of DATA_TRX_ID */
[[nodiscard]] static inline ulint row_get_trx_id_offset(
    const dict_index_t *index, const ulint *offsets);
/** Reads the trx id field from a clustered index record.
@param[in] rec Record.
@param[in] index Clustered index.
@param[in] offsets rec_get_offsets(rec, index).
@return value of the field */
[[nodiscard]] static inline trx_id_t row_get_rec_trx_id(
    const rec_t *rec, const dict_index_t *index, const ulint *offsets);
/** Reads the roll pointer field from a clustered index record.
@param[in] rec Record.
@param[in] index Clustered index.
@param[in] offsets rec_get_offsets(rec, index).
@return value of the field */
[[nodiscard]] static inline roll_ptr_t row_get_rec_roll_ptr(
    const rec_t *rec, const dict_index_t *index, const ulint *offsets);

/* Flags for row build type. */
/** build index row */
constexpr uint32_t ROW_BUILD_NORMAL = 0;
/** build row for purge. */
constexpr uint32_t ROW_BUILD_FOR_PURGE = 1;
/** build row for undo. */
constexpr uint32_t ROW_BUILD_FOR_UNDO = 2;
/** build row for insert. */
constexpr uint32_t ROW_BUILD_FOR_INSERT = 3;
/** When an insert or purge to a table is performed, this function builds
the entry to be inserted into or purged from an index on the table.
@param[in] row   Row which should be inserted or purged.
@param[in] ext   Externally stored column prefixes, or nullptr.
@param[in] index Index on the table.
@param[in] heap  Memory heap from which the memory for the index entry is
allocated.
@param[in] flag  ROW_BUILD_NORMAL, ROW_BUILD_FOR_PURGE or ROW_BUILD_FOR_UNDO.
@return index entry which should be inserted or purged
@retval NULL if the externally stored columns in the clustered index record
are unavailable and ext != nullptr, or row is missing some needed columns. */
[[nodiscard]] dtuple_t *row_build_index_entry_low(const dtuple_t *row,
                                                  const row_ext_t *ext,
                                                  const dict_index_t *index,
                                                  mem_heap_t *heap, ulint flag);
/** When an insert or purge to a table is performed, this function builds
the entry to be inserted into or purged from an index on the table.
@return index entry which should be inserted or purged, or NULL if the
externally stored columns in the clustered index record are
unavailable and ext != nullptr
@param[in] row   Row which should be inserted or purged.
@param[in] ext   Externally stored column prefixes, or nullptr.
@param[in] index Index on the table.
@param[in] heap  Memory heap from which the memory for the index entry is
allocated. */
[[nodiscard]] static inline dtuple_t *row_build_index_entry(
    const dtuple_t *row, const row_ext_t *ext, const dict_index_t *index,
    mem_heap_t *heap);
/** An inverse function to row_build_index_entry. Builds a row from a
record in a clustered index.
@param[in] type      ROW_COPY_POINTERS or ROW_COPY_DATA; the latter copies also
the data fields to heap while the first only places pointers to data fields on
the index page, and thus is more efficient.
@param[in] index     Clustered index.
@param[in] rec       Record in the clustered index; NOTE: in the case
ROW_COPY_POINTERS the data fields in the row will point directly into this
record, therefore, the buffer page of this record must be at least s-latched and
the latch held as long as the row dtuple is used!
@param[in] offsets rec_get_offsets(rec,index) or nullptr, in which case this
function will invoke rec_get_offsets().
@param[in] col_table Table, to check which externally stored columns occur in
the ordering columns of an index, or nullptr if index->table should be consulted
instead; the user columns in this table should be the same columns as in
index->table.
@param[in] add_cols  Default values of added columns, or nullptr.
@param[in] col_map   Mapping of old column numbers to new ones, or nullptr.
@param[out] ext      cache of externally stored column prefixes, or nullptr.
@param[in] heap      Memory heap from which the memory needed is allocated.
@return own: row built; see the NOTE below! */
dtuple_t *row_build(ulint type, const dict_index_t *index, const rec_t *rec,
                    const ulint *offsets, const dict_table_t *col_table,
                    const dtuple_t *add_cols, const ulint *col_map,
                    row_ext_t **ext, mem_heap_t *heap);

/** An inverse function to row_build_index_entry. Builds a row from a
record in a clustered index, with possible indexing on ongoing
addition of new virtual columns.
@param[in]      type            ROW_COPY_POINTERS or ROW_COPY_DATA;
@param[in]      index           clustered index
@param[in]      rec             record in the clustered index
@param[in]      offsets         rec_get_offsets(rec,index) or NULL
@param[in]      col_table       table, to check which
                                externally stored columns
                                occur in the ordering columns
                                of an index, or NULL if
                                index->table should be
                                consulted instead
@param[in]      add_cols        default values of added columns, or NULL
@param[in]      add_v           new virtual columns added
                                along with new indexes
@param[in]      col_map         mapping of old column
                                numbers to new ones, or NULL
@param[in]      ext             cache of externally stored column
                                prefixes, or NULL
@param[in]      heap            memory heap from which
                                the memory needed is allocated
@return own: row built */
dtuple_t *row_build_w_add_vcol(ulint type, const dict_index_t *index,
                               const rec_t *rec, const ulint *offsets,
                               const dict_table_t *col_table,
                               const dtuple_t *add_cols,
                               const dict_add_v_col_t *add_v,
                               const ulint *col_map, row_ext_t **ext,
                               mem_heap_t *heap);

/** Converts an index record to a typed data tuple.
 @return index entry built; does not set info_bits, and the data fields
 in the entry will point directly to rec */
[[nodiscard]] dtuple_t *row_rec_to_index_entry_low(
    const rec_t *rec,          /*!< in: record in the index */
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec, index) */
    mem_heap_t *heap);         /*!< in: memory heap from which
                              the memory needed is allocated */
/** Converts an index record to a typed data tuple. NOTE that externally
 stored (often big) fields are NOT copied to heap.
 @return own: index entry built */
[[nodiscard]] dtuple_t *row_rec_to_index_entry(
    const rec_t *rec,          /*!< in: record in the index */
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets,      /*!< in/out: rec_get_offsets(rec) */
    mem_heap_t *heap);         /*!< in: memory heap from which
                              the memory needed is allocated */
/** Builds from a secondary index record a row reference with which we can
 search the clustered index record.
 @return own: row reference built; see the NOTE below! */
[[nodiscard]] dtuple_t *row_build_row_ref(
    ulint type,                /*!< in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
                               the former copies also the data fields to
                               heap, whereas the latter only places pointers
                               to data fields on the index page */
    const dict_index_t *index, /*!< in: secondary index */
    const rec_t *rec,          /*!< in: record in the index;
                               NOTE: in the case ROW_COPY_POINTERS
                               the data fields in the row will point
                               directly into this record, therefore,
                               the buffer page of this record must be
                               at least s-latched and the latch held
                               as long as the row reference is used! */
    mem_heap_t *heap);         /*!< in: memory heap from which the memory
                              needed is allocated */

/** Builds from a secondary index record a row reference with which we can
search the clustered index record.
@param[in,out] ref Row reference built; see the note below!
@param[in,out] rec Record in the index; note: the data fields in ref will point
directly into this record, therefore, the buffer page of this record must be at
least s-latched and the latch held as long as the row reference is used!
@param[in] index Secondary index
@param[in] offsets Rec_get_offsets(rec, index) or null */
void row_build_row_ref_in_tuple(dtuple_t *ref, const rec_t *rec,
                                const dict_index_t *index, ulint *offsets);

/** Builds from a secondary index record a row reference with which we can
search the clustered index record.
@param[in,out]  ref     typed data tuple where the reference is built
@param[in]      map     array of field numbers in rec telling how ref should
                        be built from the fields of rec
@param[in]      rec     record in the index; must be preserved while ref is
                        used, as we do not copy field values to heap
@param[in]      offsets array returned by rec_get_offsets() */
static inline void row_build_row_ref_fast(dtuple_t *ref, const ulint *map,
                                          const rec_t *rec,
                                          const ulint *offsets);

/** Searches the clustered index record for a row, if we have the row
 reference.
 @return true if found */
[[nodiscard]] bool row_search_on_row_ref(
    btr_pcur_t *pcur,    /*!< out: persistent cursor, which
            must be closed by the caller */
    ulint mode,          /*!< in: BTR_MODIFY_LEAF, ... */
    dict_table_t *table, /*!< in: table */
    const dtuple_t *ref, /*!< in: row reference */
    mtr_t *mtr);         /*!< in/out: mtr */
/** Fetches the clustered index record for a secondary index record. The latches
 on the secondary index record are preserved.
 @return record or NULL, if no record found */
[[nodiscard]] rec_t *row_get_clust_rec(
    ulint mode,                 /*!< in: BTR_MODIFY_LEAF, ... */
    const rec_t *rec,           /*!< in: record in a secondary index */
    const dict_index_t *index,  /*!< in: secondary index */
    dict_index_t **clust_index, /*!< out: clustered index */
    mtr_t *mtr);                /*!< in: mtr */

/** Parse the integer data from specified data, which could be
DATA_INT, DATA_FLOAT or DATA_DOUBLE. If the value is less than 0
and the type is not unsigned then we reset the value to 0
@param[in]      data            data to read
@param[in]      len             length of data
@param[in]      mtype           mtype of data
@param[in]      unsigned_type   if the data is unsigned
@return the integer value from the data */
inline uint64_t row_parse_int(const byte *data, ulint len, ulint mtype,
                              bool unsigned_type);

/** Parse the integer data from specified field, which could be
DATA_INT, DATA_FLOAT or DATA_DOUBLE. We could return 0 if
1) the value is less than 0 and the type is not unsigned
or 2) the field is null.
@param[in]      field           field to read the int value
@return the integer value read from the field, 0 for negative signed
int or NULL field */
uint64_t row_parse_int_from_field(const dfield_t *field);

/** Read the autoinc counter from the clustered index row.
@param[in]      row     row to read the autoinc counter
@param[in]      n       autoinc counter is in the nth field
@return the autoinc counter read */
uint64_t row_get_autoinc_counter(const dtuple_t *row, ulint n);

/** Result of row_search_index_entry */
enum row_search_result {
  ROW_FOUND = 0,      /*!< the record was found */
  ROW_NOT_FOUND,      /*!< record not found */
  ROW_BUFFERED,       /*!< one of BTR_INSERT, BTR_DELETE, or
                      BTR_DELETE_MARK was specified, the
                      secondary index leaf page was not in
                      the buffer pool, and the operation was
                      enqueued in the insert/delete buffer */
  ROW_NOT_DELETED_REF /*!< BTR_DELETE was specified, and
                      row_purge_poss_sec() failed */
};

/** Searches an index record.
 @return whether the record was found or buffered */
[[nodiscard]] enum row_search_result row_search_index_entry(
    dict_index_t *index,   /*!< in: index */
    const dtuple_t *entry, /*!< in: index entry */
    ulint mode,            /*!< in: BTR_MODIFY_LEAF, ... */
    btr_pcur_t *pcur,      /*!< in/out: persistent cursor, which must
                           be closed by the caller */
    mtr_t *mtr);           /*!< in: mtr */

constexpr uint32_t ROW_COPY_DATA = 1;
constexpr uint32_t ROW_COPY_POINTERS = 2;

/* The allowed latching order of index records is the following:
(1) a secondary index record ->
(2) the clustered index record ->
(3) rollback segment data for the clustered index record. */

/** Formats the raw data in "data" (in InnoDB on-disk format) using
 "dict_field" and writes the result to "buf".
 Not more than "buf_size" bytes are written to "buf".
 The result is always NUL-terminated (provided buf_size is positive) and the
 number of bytes that were written to "buf" is returned (including the
 terminating NUL).
 @return number of bytes that were written */
[[nodiscard]] ulint row_raw_format(
    const char *data,               /*!< in: raw data */
    ulint data_len,                 /*!< in: raw data length
                                    in bytes */
    const dict_field_t *dict_field, /*!< in: index field */
    char *buf,                      /*!< out: output buffer */
    ulint buf_size);                /*!< in: output buffer size
                                   in bytes */

/** Class to build a series of entries based on one multi-value field.
It assumes that there is only one multi-value field on multi-value index. */
class Multi_value_entry_builder {
 public:
  /** Constructor */
  Multi_value_entry_builder(dict_index_t *index, dtuple_t *entry, bool selected)
      : m_index(index),
        m_selected(selected),
        m_entry(entry),
        m_pos(0),
        m_mv_data(nullptr),
        m_mv_field_no(0) {}

  virtual ~Multi_value_entry_builder() = default;

  /** Get the first index entry. If the multi-value field on the index
  is null, then it's the entry including the null field, otherwise,
  it should be the entry with  multi-value data at the 'pos' position.
  @param[in]    pos     position of the multi-value array, default value
                        will always start from 0
  @return the first index entry to handle, the one including null
  multi-value field, or the  multi-value data at the 'pos' position */
  dtuple_t *begin(uint32_t pos = 0) {
    if (!prepare_multi_value_field()) {
      return (nullptr);
    }

    prepare_entry_if_necessary();
    ut_ad(m_entry != nullptr);

    m_pos = pos;
    return (m_mv_data == nullptr ? m_entry : next());
  }

  /** Get next index entry based on next multi-value data.
  If the previous value is null, then always no next.
  @return next index entry, or nullptr if no more multi-value data */
  dtuple_t *next() {
    if (m_mv_data == nullptr || m_pos >= m_mv_data->num_v) {
      return (nullptr);
    }

    ut_ad(m_entry != nullptr);
    dfield_t *field = dtuple_get_nth_field(m_entry, m_mv_field_no);
    ut_ad(dfield_is_multi_value(field));

    if (m_selected && (skip() == m_mv_data->num_v)) {
      return (nullptr);
    }

    const auto len = m_mv_data->data_len[m_pos];
    dfield_set_data(field, m_mv_data->datap[m_pos], len);

    ++m_pos;
    return (m_entry);
  }

  /** Get the position of last generated multi-value data
  @return the position */
  uint32_t last_multi_value_position() const {
    return (m_pos > 0 ? m_pos - 1 : 0);
  }

 protected:
  /** Find the multi-value field from the passed in entry or row.
  m_mv_field_no should be set once the multi-value field found.
  @return the multi-value field pointer, or nullptr if not found */
  virtual dfield_t *find_multi_value_field() = 0;

  /** Prepare the corresponding multi-value field from the row.
  This function will set the m_mv_data if the proper field found.
  @return true if the multi-value field with data on index found,
  otherwise, false */
  virtual bool prepare_multi_value_field() {
    dfield_t *field = find_multi_value_field();

    if (field == nullptr || field->len == UNIV_NO_INDEX_VALUE) {
      return (false);
    }

    ut_ad(m_mv_field_no > 0);
    ut_ad(dfield_is_multi_value(field));

    --m_mv_field_no;

    if (!dfield_is_null(field)) {
      m_mv_data = static_cast<multi_value_data *>(field->data);
    }

    return (true);
  }

  /** Prepare the entry when the entry is not passed in */
  virtual void prepare_entry_if_necessary() { return; }

  /** Skip the not selected values and stop m_pos at the next selected one
  @return the next valid value position, or size of m_mv_data to indicate
  there is no more valid value */
  virtual uint32_t skip() {
    ut_ad(m_mv_data != nullptr);
    ut_ad(m_selected);
    return (m_mv_data->num_v);
  }

 protected:
  /** Based on which index to build the entry */
  dict_index_t *m_index;

  /** True if only the selected(bitmap set) multi-value data would be
  used to build the entries, otherwise false. */
  const bool m_selected;

  /** Entry built for the index */
  dtuple_t *m_entry;

  /** Multi-value data position */
  uint32_t m_pos;

  /** Multi-value data */
  const multi_value_data *m_mv_data;

  /** Field number of multi-value data on the index */
  uint32_t m_mv_field_no;
};

/** The subclass of the multi-value entry builder, for non-INSERT cases,
With this class, there should be no need to build separate entries for
different values in the same multi-value field. */
class Multi_value_entry_builder_normal : public Multi_value_entry_builder {
 public:
  /** Constructor
  @param[in]            row             based on which complete row to build
                                        the index row
  @param[in]            ext             externally stored column prefixes of
                                        the row
  @param[in,out]        index           multi-value index
  @param[in,out]        heap            memory heap
  @param[in]            check           true if type can be checked, otherwise
                                        skip checking
  @param[in]            selected        true if only the selected(bitmap set)
                                        multi-value data would be used to build
                                        the entries, otherwise false. */
  Multi_value_entry_builder_normal(const dtuple_t *row, const row_ext_t *ext,
                                   dict_index_t *index, mem_heap_t *heap,
                                   bool check, bool selected)
      : Multi_value_entry_builder(index, nullptr, selected),
        m_row(row),
        m_ext(ext),
        m_heap(heap),
        m_check(check) {}

 private:
  /** Find the multi-value field from the passed in entry or row.
  m_mv_field_no should be set once the multi-value field found.
  @return the multi-value field pointer, or nullptr if not found */
  dfield_t *find_multi_value_field() override;

  /** Prepare the entry when the entry is not passed in */
  void prepare_entry_if_necessary() override {
    if (m_check) {
      m_entry = row_build_index_entry(m_row, m_ext, m_index, m_heap);
    } else {
      /* If not check, then it's basically coming from purge. And actually,
      for multi-value index, this flag really doesn't matter. */
      m_entry = row_build_index_entry_low(m_row, m_ext, m_index, m_heap,
                                          ROW_BUILD_FOR_PURGE);
    }
  }

  /** Skip the not selected values and stop m_pos at the next selected one
  @return the next valid value position, or size of m_mv_data to indicate
  there is no more valid value */
  uint32_t skip() override {
    ut_ad(m_selected);

    if (m_mv_data->bitset == nullptr) {
      return (m_pos);
    }

    while (m_pos < m_mv_data->num_v && !m_mv_data->bitset->test(m_pos)) {
      ++m_pos;
    }

    return (m_pos);
  }

 private:
  /** Based on which complete row to build the index row */
  const dtuple_t *m_row;

  /** Externally stored column prefixes, or nullptr */
  const row_ext_t *m_ext;

  /** Memory heap */
  mem_heap_t *m_heap;

  /** True if dfield type should be checked, otherwise false */
  const bool m_check;
};

/** The subclass of the multi-value row builder, for INSERT cases.
It simply replace the pointers to the multi-value field data for
each different value */
class Multi_value_entry_builder_insert : public Multi_value_entry_builder {
 public:
  /** Constructor
  @param[in,out]        index   multi-value index
  @param[in]            entry   entry to insert based on the index */
  Multi_value_entry_builder_insert(dict_index_t *index, dtuple_t *entry)
      : Multi_value_entry_builder(index, entry, false) {}

 private:
  /** Find the multi-value field from the passed entry in or row.
  m_mv_field_no should be set once the multi-value field found.
  @return the multi-value field pointer, or nullptr if not found */
  dfield_t *find_multi_value_field() override {
    uint16_t i = 0;
    dfield_t *field = nullptr;

    ut_ad(m_entry != nullptr);

    m_mv_field_no = 0;
    for (; i < m_entry->n_fields; ++i) {
      field = &m_entry->fields[i];
      if (!dfield_is_multi_value(field)) {
        continue;
      }

      m_mv_field_no = i + 1;
      break;
    }

    return (i == m_entry->n_fields ? nullptr : field);
  }
};

#include "row0row.ic"

#endif
