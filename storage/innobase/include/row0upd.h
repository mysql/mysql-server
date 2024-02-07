/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

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

/** @file include/row0upd.h
 Update of a row

 Created 12/27/1996 Heikki Tuuri
 *******************************************************/

#ifndef row0upd_h
#define row0upd_h

#include <stack>
#include "btr0types.h"
#include "data0data.h"
#include "dict0types.h"
#include "lob0lob.h"
#include "row0types.h"
#include "table.h"
#include "trx0types.h"
#include "univ.i"

#include "btr0pcur.h"
#ifndef UNIV_HOTBACKUP
#include "pars0types.h"
#include "que0types.h"
#endif /* !UNIV_HOTBACKUP */

/** Creates an update vector object.
@param[in]      n       number of fields
@param[in]      heap    heap from which memory allocated
@return own: update vector object */
static inline upd_t *upd_create(ulint n, mem_heap_t *heap);

/** Returns the number of fields in the update vector == number of columns
 to be updated by an update vector.
 @return number of fields */
static inline ulint upd_get_n_fields(
    const upd_t *update); /*!< in: update vector */

#ifdef UNIV_DEBUG
/** Returns the nth field of an update vector.
@param[in]      update  update vector
@param[in]      n       field position in update vector
@return update vector field */
static inline upd_field_t *upd_get_nth_field(const upd_t *update, ulint n);
#else
#define upd_get_nth_field(update, n) ((update)->fields + (n))
#endif
/** Sets an index field number to be updated by an update vector field.
@param[in]      upd_field       update vector field
@param[in]      field_no        field number in a clustered index
@param[in]      index           index */
static inline void upd_field_set_field_no(upd_field_t *upd_field,
                                          ulint field_no,
                                          const dict_index_t *index);

/** set field number to a update vector field, marks this field is updated
@param[in,out]  upd_field       update vector field
@param[in]      field_no        virtual column sequence num
@param[in]      index           index */
static inline void upd_field_set_v_field_no(upd_field_t *upd_field,
                                            ulint field_no,
                                            const dict_index_t *index);
/** Returns a field of an update vector by field_no.
@param[in] update     Update vector.
@param[in] no         Field no.
@param[in] is_virtual If it is a virtual column.
@return update vector field, or nullptr. */
[[nodiscard]] static inline const upd_field_t *upd_get_field_by_field_no(
    const upd_t *update, ulint no, bool is_virtual);
/** Writes into the redo log the values of trx id and roll ptr and enough info
to determine their positions within a clustered index record.
@param[in] index    Clustered index.
@param[in] trx_id   Transaction ID.
@param[in] roll_ptr Roll ptr of the undo log record.
@param[in] log_ptr  Pointer to a buffer of size > 20 opened in mlog.
@param[in] mtr      Mini-transaction.
@return new pointer to mlog */
byte *row_upd_write_sys_vals_to_log(dict_index_t *index, trx_id_t trx_id,
                                    roll_ptr_t roll_ptr, byte *log_ptr,
                                    mtr_t *mtr);

#ifndef UNIV_HOTBACKUP
/** Updates the trx id and roll ptr field in a clustered index record when a
row is updated or marked deleted.
@param[in,out]  rec             record
@param[in,out]  page_zip        compressed page whose uncompressed part will
                                be updated, or NULL
@param[in]      index           clustered index
@param[in]      offsets         rec_get_offsets(rec, index)
@param[in]      trx             transaction
@param[in]      roll_ptr        roll ptr of the undo log record, can be 0
                                during IMPORT */
static inline void row_upd_rec_sys_fields(rec_t *rec, page_zip_des_t *page_zip,
                                          const dict_index_t *index,
                                          const ulint *offsets,
                                          const trx_t *trx,
                                          roll_ptr_t roll_ptr);
#endif /* !UNIV_HOTBACKUP */

/** Sets the trx id or roll ptr field of a clustered index entry.
@param[in,out] entry Index entry, where the memory buffers for sys fields are
already allocated: the function just copies the new values to them
@param[in] index Clustered index
@param[in] type Data_trx_id or data_roll_ptr
@param[in] val Value to write */
void row_upd_index_entry_sys_field(dtuple_t *entry, dict_index_t *index,
                                   ulint type, uint64_t val);

/** Creates an update node for a query graph.
 @return own: update node */
upd_node_t *upd_node_create(
    mem_heap_t *heap); /*!< in: mem heap where created */

/** Writes to the redo log the new values of the fields occurring in the index.
@param[in]      index   index which to be updated
@param[in]      update  update vector
@param[in]      log_ptr pointer to mlog buffer: must contain at least
                        MLOG_BUF_MARGIN bytes of free space; the buffer
                        is closed within this function
@param[in]      mtr     mtr into whose log to write */
void row_upd_index_write_log(dict_index_t *index, const upd_t *update,
                             byte *log_ptr, mtr_t *mtr);

/** Returns true if row update changes size of some field in index or if some
 field to be updated is stored externally in rec or update.
 @return true if the update changes the size of some field in index or
 the field is external in rec or update */
bool row_upd_changes_field_size_or_external(
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec, index) */
    const upd_t *update);      /*!< in: update vector */
/** Returns true if row update contains disowned external fields.
 @return true if the update contains disowned external fields. */
[[nodiscard]] bool row_upd_changes_disowned_external(
    const upd_t *update); /*!< in: update vector */
/** Replaces the new column values stored in the update vector to the
 record given. No field size changes are allowed. This function is
 usually invoked on a clustered index. The only use case for a
 secondary index is row_ins_sec_index_entry_by_modify() or its
 counterpart in ibuf_insert_to_index_page(). */
void row_upd_rec_in_place(
    rec_t *rec,                /*!< in/out: record where replaced */
    const dict_index_t *index, /*!< in: the index the record belongs to */
    const ulint *offsets,      /*!< in: array returned by rec_get_offsets() */
    const upd_t *update,       /*!< in: update vector */
    page_zip_des_t *page_zip); /*!< in: compressed page with enough space
                             available, or NULL */
/** Builds an update vector from those fields which in a secondary index entry
 differ from a record that has the equal ordering fields. NOTE: we compare
 the fields as binary strings!
 @return own: update vector of differing fields */
[[nodiscard]] upd_t *row_upd_build_sec_rec_difference_binary(
    const rec_t *rec,      /*!< in: secondary index record */
    dict_index_t *index,   /*!< in: index */
    const ulint *offsets,  /*!< in: rec_get_offsets(rec, index) */
    const dtuple_t *entry, /*!< in: entry to insert */
    mem_heap_t *heap);     /*!< in: memory heap from which allocated */
/** Builds an update vector from those fields, excluding the roll ptr and
trx id fields, which in an index entry differ from a record that has
the equal ordering fields. NOTE: we compare the fields as binary strings!
@param[in]      index           clustered index
@param[in]      entry           clustered index entry to insert
@param[in]      rec             clustered index record
@param[in]      offsets         rec_get_offsets(rec,index), or NULL
@param[in]      no_sys          skip the system columns
                                DB_TRX_ID and DB_ROLL_PTR
@param[in]      trx             transaction (for diagnostics),
                                or NULL
@param[in]      heap            memory heap from which allocated
@param[in]      mysql_table     NULL, or mysql table object when
                                user thread invokes dml
@param[out]     error           error number in case of failure
@return own: update vector of differing fields, excluding roll ptr and
trx id */
[[nodiscard]] upd_t *row_upd_build_difference_binary(
    dict_index_t *index, const dtuple_t *entry, const rec_t *rec,
    const ulint *offsets, bool no_sys, trx_t *trx, mem_heap_t *heap,
    TABLE *mysql_table, dberr_t *error);

/** Replaces the new column values stored in the update vector to the index
 entry given.
@param[in,out] entry Index entry where replaced; the clustered index record must
be covered by a lock or a page latch to prevent deletion [rollback or purge]
@param[in] index Index; note that this may also be a non-clustered index
@param[in] update An update vector built for the index so that the field number
in an upd_field is the index position
@param[in] order_only If true, limit the replacement to ordering fields of
index; note that this does not work for non-clustered indexes.
@param[in] heap Memory heap for allocating and copying the new values */
void row_upd_index_replace_new_col_vals_index_pos(dtuple_t *entry,
                                                  const dict_index_t *index,
                                                  const upd_t *update,
                                                  bool order_only,
                                                  mem_heap_t *heap);

/** Replaces the new column values stored in the update vector to the index
 entry given.
@param[in,out] entry Index entry where replaced; the clustered index record must
be covered by a lock or a page latch to prevent deletion (rollback or purge)
@param[in] index Index; note that this may also be a non-clustered index
@param[in] update An update vector built for the clustered index so that the
field number in an upd_field is the clustered index position
@param[in] heap Memory heap for allocating and copying the new values */
void row_upd_index_replace_new_col_vals(dtuple_t *entry,
                                        const dict_index_t *index,
                                        const upd_t *update, mem_heap_t *heap);

/** Replaces the new column values stored in the update vector.
@param[in,out] row Row where replaced, indexed by col_no; the clustered index
record must be covered by a lock or a page latch to prevent deletion (rollback
or purge)
@param[in,out] ext Null, or externally stored column prefixes
@param[in] index Clustered index
@param[in] update An update vector built for the clustered index
@param[in] heap Memory heap */
void row_upd_replace(dtuple_t *row, row_ext_t **ext, const dict_index_t *index,
                     const upd_t *update, mem_heap_t *heap);

/** Replaces the virtual column values stored in a dtuple with that of
a update vector.
@param[in,out]  row     dtuple whose column to be updated
@param[in]      table   table
@param[in]      update  an update vector built for the clustered index
@param[in]      upd_new update to new or old value
@param[in,out]  undo_row undo row (if needs to be updated)
@param[in]      ptr     remaining part in update undo log */
void row_upd_replace_vcol(dtuple_t *row, const dict_table_t *table,
                          const upd_t *update, bool upd_new, dtuple_t *undo_row,
                          const byte *ptr);

/** Checks if an update vector changes an ordering field of an index record.
It will also help check if any non-multi-value field on the multi-value index
gets updated or not.

This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic.
NOTE: we compare the fields as binary strings!
@param[in]      index           index of the record
@param[in]      update          update vector for the row; NOTE: the
                                field numbers in this MUST be clustered index
                                positions!
@param[in]      thr             query thread, or NULL
@param[in]      row             old value of row, or NULL if the
                                row and the data values in update are not
                                known when this function is called, e.g., at
                                compile time
@param[in]      ext             NULL, or prefixes of the externally
                                stored columns in the old row
@param[in,out]  non_mv_upd      NULL, or not NULL pointer to get the
                                information about whether any non-multi-value
                                field on the multi-value index gets updated
@param[in]      flag            ROW_BUILD_NORMAL, ROW_BUILD_FOR_PURGE or
                                ROW_BUILD_FOR_UNDO
@return true if update vector changes an ordering field in the index record */
[[nodiscard]] bool row_upd_changes_ord_field_binary_func(
    dict_index_t *index, const upd_t *update,
    IF_DEBUG(const que_thr_t *thr, ) const dtuple_t *row, const row_ext_t *ext,
    bool *non_mv_upd, ulint flag);

static inline bool row_upd_changes_ord_field_binary(
    dict_index_t *index, const upd_t *update,
    const que_thr_t *thr [[maybe_unused]], const dtuple_t *row,
    const row_ext_t *ext, bool *non_mv_upd) {
  return row_upd_changes_ord_field_binary_func(
      index, update, IF_DEBUG(thr, ) row, ext, non_mv_upd, 0);
}

/** Checks if an FTS indexed column is affected by an UPDATE.
 @return offset within fts_t::indexes if FTS indexed column updated else
 ULINT_UNDEFINED */
ulint row_upd_changes_fts_column(
    dict_table_t *table,     /*!< in: table */
    upd_field_t *upd_field); /*!< in: field to check */
/** Checks if an FTS Doc ID column is affected by an UPDATE.
 @return whether Doc ID column is affected */
[[nodiscard]] bool row_upd_changes_doc_id(
    dict_table_t *table,     /*!< in: table */
    upd_field_t *upd_field); /*!< in: field to check */
/** Checks if an update vector changes an ordering field of an index record.
 This function is fast if the update vector is short or the number of ordering
 fields in the index is small. Otherwise, this can be quadratic.
 NOTE: we compare the fields as binary strings!
 @return true if update vector may change an ordering field in an index
 record */
bool row_upd_changes_some_index_ord_field_binary(
    const dict_table_t *table, /*!< in: table */
    const upd_t *update);      /*!< in: update vector for the row */

/** Stores to the heap the row on which the node->pcur is positioned.
@param[in]      node            row update node
@param[in]      thd             mysql thread handle
@param[in,out]  mysql_table     NULL, or mysql table object when
                                user thread invokes dml */
void row_upd_store_row(upd_node_t *node, THD *thd, TABLE *mysql_table);

/** Updates a row in a table. This is a high-level function used
 in SQL execution graphs.
 @return query thread to run next or NULL */
que_thr_t *row_upd_step(que_thr_t *thr); /*!< in: query thread */
/** Parses the log data of system field values.
 @return log data end or NULL */
byte *row_upd_parse_sys_vals(const byte *ptr,     /*!< in: buffer */
                             const byte *end_ptr, /*!< in: buffer end */
                             ulint *pos, /*!< out: TRX_ID position in record */
                             trx_id_t *trx_id,      /*!< out: trx id */
                             roll_ptr_t *roll_ptr); /*!< out: roll ptr */

/** Updates the trx id and roll ptr field in a clustered index record in
 database recovery.
@param[in,out] rec Record
@param[in,out] page_zip Compressed page, or null
@param[in] offsets Array returned by rec_get_offsets()
@param[in] pos Trx_id position in rec
@param[in] trx_id Transaction id
@param[in] roll_ptr Roll ptr of the undo log record */
void row_upd_rec_sys_fields_in_recovery(rec_t *rec, page_zip_des_t *page_zip,
                                        const ulint *offsets, ulint pos,
                                        trx_id_t trx_id, roll_ptr_t roll_ptr);

/** Parses the log data written by row_upd_index_write_log.
 @return log data end or NULL */
byte *row_upd_index_parse(const byte *ptr,     /*!< in: buffer */
                          const byte *end_ptr, /*!< in: buffer end */
                          mem_heap_t *heap,    /*!< in: memory heap where update
                                               vector is    built */
                          upd_t **update_out); /*!< out: update vector */

/** Get the new autoinc counter from the update vector when there is
an autoinc field defined in this table.
@param[in]      update                  update vector for the clustered index
@param[in]      autoinc_field_no        autoinc field's order in clustered index
@return the new counter if we find it in the update vector, otherwise 0.
We don't mind that the new counter happens to be 0, we just care about
non-zero counters. */
uint64_t row_upd_get_new_autoinc_counter(const upd_t *update,
                                         ulint autoinc_field_no);

/** This structure is used for undo logging of LOB index changes. */
struct lob_index_diff_t {
  trx_id_t m_modifier_trxid;
  undo_no_t m_modifier_undo_no;

  /** Print the current object into the given output stream.
  @param[in,out]        out     the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const {
    out << "[lob_index_diff_t: m_modifier_trxid=" << m_modifier_trxid
        << ", m_modifier_undo_no=" << m_modifier_undo_no << "]";
    return (out);
  }
};

using Lob_index_diff_vec =
    std::vector<lob_index_diff_t, mem_heap_allocator<lob_index_diff_t>>;

/** Overloading the global output operator to print lob_index_diff_t object.
@param[in,out]  out     the output stream.
@param[in]      obj     the object to be printed.
@return the output stream.*/
inline std::ostream &operator<<(std::ostream &out,
                                const lob_index_diff_t &obj) {
  return (obj.print(out));
}

/** The modification done to the LOB. */
struct Lob_diff {
  /** Constructor.
  @param[in]  mem_heap  the memory heap in which this object
                        has been created. */
  Lob_diff(mem_heap_t *mem_heap) : heap(mem_heap) {
    m_idx_diffs = static_cast<Lob_index_diff_vec *>(
        mem_heap_alloc(heap, sizeof(Lob_index_diff_vec)));
    new (m_idx_diffs)
        Lob_index_diff_vec(mem_heap_allocator<lob_index_diff_t>(heap));
  }

  /** Read the offset from the undo record.
  @param[in]   undo_ptr   pointer into the undo log record.
  @return pointer into the undo log record after offset. */
  const byte *read_offset(const byte *undo_ptr) {
    /* Read the offset. */
    m_offset = mach_read_next_compressed(&undo_ptr);
    return (undo_ptr);
  }

  /** Read the length from the undo record.
  @param[in]   undo_ptr   pointer into the undo log record.
  @return pointer into the undo log record after length information. */
  const byte *read_length(const byte *undo_ptr) {
    /* Read the length. */
    m_length = mach_read_next_compressed(&undo_ptr);
    ut_ad(m_length <= lob::ref_t::LOB_SMALL_CHANGE_THRESHOLD);

    return (undo_ptr);
  }

  void set_old_data(const byte *undo_ptr) { m_old_data = undo_ptr; }

  std::ostream &print(std::ostream &out) const {
    out << "[Lob_diff: offset=" << m_offset << ", length=" << m_length;
    if (m_old_data == nullptr) {
      out << ", m_old_data=nullptr";
    } else {
      out << ", m_old_data=" << PrintBuffer(m_old_data, m_length);
    }

    if (m_idx_diffs != nullptr) {
      for (auto iter = m_idx_diffs->begin(); iter != m_idx_diffs->end();
           ++iter) {
        out << *iter;
      }
    }

    out << "]";
    return (out);
  }

  /** The offset within LOB where partial update happened. */
  ulint m_offset = 0;

  /** The length of the modification. */
  ulint m_length = 0;

  /** Changes to the LOB data. */
  const byte *m_old_data = nullptr;

  /** Changes to the LOB index. */
  Lob_index_diff_vec *m_idx_diffs;

  /** Memory heap in which this object is allocated. */
  mem_heap_t *heap;
};

using Lob_diff_vector = std::vector<Lob_diff, mem_heap_allocator<Lob_diff>>;

inline std::ostream &operator<<(std::ostream &out, const Lob_diff &obj) {
  return (obj.print(out));
}

/* Update vector field */
struct upd_field_t {
  upd_field_t()
      : field_no(0),
        orig_len(0),
        exp(nullptr),
        old_v_val(nullptr),
        mysql_field(nullptr),
        ext_in_old(false),
        lob_diffs(nullptr),
        lob_first_page_no(FIL_NULL),
        lob_version(0),
        last_trx_id(0),
        last_undo_no(0),
        heap(nullptr) {}

  bool is_virtual() const { return (new_val.is_virtual()); }

  unsigned field_no : 16; /*!< field number in an index, usually
                          the clustered index, but in updating
                          a secondary index record in btr0cur.cc
                          this is the position in the secondary
                          index, also it could be the position
                          in virtual index for virtual column */
  IF_DEBUG(uint16_t field_phy_pos{UINT16_MAX};)
  unsigned orig_len : 16; /*!< original length of the locally
                          stored part of an externally stored
                          column, or 0 */
  que_node_t *exp;        /*!< expression for calculating a new
                          value: it refers to column values and
                          constants in the symbol table of the
                          query graph */
  dfield_t old_val;       /*!< old value for the column */
  dfield_t new_val;       /*!< new value for the column */
  dfield_t *old_v_val;    /*!< old value for the virtual column */

  Field *mysql_field; /*!< the mysql field object. */

  /** If true, the field was stored externally in the old row. */
  bool ext_in_old;

  void push_lob_diff(const Lob_diff &lob_diff) {
    if (lob_diffs == nullptr) {
      lob_diffs = static_cast<Lob_diff_vector *>(
          mem_heap_alloc(heap, sizeof(Lob_diff_vector)));
      new (lob_diffs) Lob_diff_vector(mem_heap_allocator<Lob_diff>(heap));
    }
    lob_diffs->push_back(lob_diff);
  }

  /** List of changes done to this updated field.  This is usually
  populated from the undo log. */
  Lob_diff_vector *lob_diffs;

  /** The LOB first page number.  This information is read from
  the undo log. */
  page_no_t lob_first_page_no;

  ulint lob_version;

  /** The last trx that modified the LOB. */
  trx_id_t last_trx_id;

  /** The last stmt within trx that modified the LOB. */
  undo_no_t last_undo_no;

  std::ostream &print(std::ostream &out) const;

  /** Empty the information collected on LOB diffs. */
  void reset() {
    if (lob_diffs != nullptr) {
      lob_diffs->clear();
    }
  }

  /** Memory heap in which this object is allocated. */
  mem_heap_t *heap;
};

inline std::ostream &operator<<(std::ostream &out, const upd_field_t &obj) {
  return (obj.print(out));
}

/* check whether an update field is on virtual column */
static inline bool upd_fld_is_virtual_col(const upd_field_t *upd_fld) {
  return (upd_fld->new_val.type.prtype & DATA_VIRTUAL) == DATA_VIRTUAL;
}

/* check whether an update field is on multi-value virtual column */
static inline bool upd_fld_is_multi_value_col(const upd_field_t *upd_fld) {
  return dfield_is_multi_value(&upd_fld->new_val);
}

/* set DATA_VIRTUAL bit on update field to show it is a virtual column */
static inline void upd_fld_set_virtual_col(upd_field_t *upd_fld) {
  upd_fld->new_val.type.prtype |= DATA_VIRTUAL;
}

/* Update vector structure */
struct upd_t {
  /** Heap from which memory allocated. This is not a new heap, rather
  will point to other heap. Therefore memory allocated from this heap
  is released when the pointed heap is freed or emptied. */
  mem_heap_t *heap;

  /** Heap from which memory is allocated if required only for current
  statement. This heap is emtied at the end of statement from inside
  ha_innobase::end_stmt(). */
  mem_heap_t *per_stmt_heap;

  /** New value of info bits to record; default is 0. */
  ulint info_bits;

  /** Pointer to old row, used for virtual column update now. */
  dtuple_t *old_vrow;

  /** The table object. */
  dict_table_t *table;

  /** The mysql table object. */
  TABLE *mysql_table;

  /** Number of update fields. */
  ulint n_fields;

  /** Array of update fields. */
  upd_field_t *fields;

  /** Append an update field to the end of array
  @param[in]    field   an update field */
  void append(const upd_field_t &field) { fields[n_fields++] = field; }

  /** Determine if the given field_no is modified.
  @return true if modified, false otherwise.  */
  bool is_modified(const ulint field_no) const {
    if (table == nullptr) {
      ut_d(ut_error);
      ut_o(return false);
    }

    return (get_field_by_field_no(field_no, table->first_index()) != nullptr);
  }

  /** Reset the update fields. */
  void reset() {
    for (ulint i = 0; i < n_fields; ++i) {
      fields[i].reset();
    }
  }

#ifdef UNIV_DEBUG
  bool validate() const {
    for (ulint i = 0; i < n_fields; ++i) {
      dfield_t *field = &fields[i].new_val;
      if (dfield_is_ext(field)) {
        ut_ad(dfield_get_len(field) >= BTR_EXTERN_FIELD_REF_SIZE);
      }
    }
    return (true);
  }
#endif  // UNIV_DEBUG

  /** Check if the given field number is partially updated.
  @param[in]    field_no        the field number.
  @return true if partially updated, false otherwise. */
  bool is_partially_updated(ulint field_no) const;

  upd_field_t *get_field_by_field_no(ulint field_no, dict_index_t *index) const;

  const Binary_diff_vector *get_binary_diff_by_field_no(ulint field_no) const;

  /** Calculate the total number of bytes modified in one BLOB.
  @param[in]    bdv     the binary diff vector containing all the
                          modifications to one BLOB.
  @return the total modified bytes. */
  static size_t get_total_modified_bytes(const Binary_diff_vector &bdv) {
    size_t total = 0;
    for (const Binary_diff &bdiff : bdv) {
      total += bdiff.length();
    }
    return (total);
  }

  /** Empty the per_stmt_heap. */
  void empty_per_stmt_heap() {
    if (per_stmt_heap != nullptr) {
      mem_heap_empty(per_stmt_heap);
    }
  }

  /** Free the per_stmt_heap. */
  void free_per_stmt_heap() {
    if (per_stmt_heap != nullptr) {
      mem_heap_free(per_stmt_heap);
      per_stmt_heap = nullptr;
    }
  }

  std::ostream &print(std::ostream &out) const;

  /** Print the partial update vector (puvect) of the given update
  field.
  @param[in,out]        out     the output stream
  @param[in]    uf      the updated field.
  @return the output stream. */
  std::ostream &print_puvect(std::ostream &out, upd_field_t *uf) const;
};

#ifdef UNIV_DEBUG
/** Print the given binary diff into the given output stream.
@param[in]      out     the output stream
@param[in]      bdiff   binary diff to be printed.
@param[in]      table   the table dictionary object.
@param[in]      field   mysql field object.
@param[in]  print_old prints old data of the updated field
@return the output stream */
std::ostream &print_binary_diff(std::ostream &out, const Binary_diff *bdiff,
                                const dict_table_t *table, const Field *field,
                                bool print_old);

std::ostream &print_binary_diff(std::ostream &out, const Binary_diff *bdiff);

inline std::ostream &operator<<(std::ostream &out, const upd_t &obj) {
  return (obj.print(out));
}

inline std::ostream &operator<<(std::ostream &out, const Binary_diff_vector &) {
  return (out);
}
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP
/* Update node structure which also implements the delete operation
of a row */

struct upd_node_t {
  que_common_t common; /*!< node type: QUE_NODE_UPDATE */
  bool is_delete;      /* true if delete, false if update */
  bool searched_update;
  /* true if searched update, false if
  positioned */
  bool in_mysql_interface;
  /* true if the update node was created
  for the MySQL interface */
  dict_foreign_t *foreign;  /* NULL or pointer to a foreign key
                            constraint if this update node is used in
                            doing an ON DELETE or ON UPDATE operation */
  upd_node_t *cascade_node; /* NULL or an update node template which
                       is used to implement ON DELETE/UPDATE CASCADE
                       or ... SET NULL for foreign keys */
  mem_heap_t *cascade_heap;
  /*!< NULL or a mem heap where cascade_upd_nodes
  are created.*/
  sel_node_t *select;  /*!< query graph subtree implementing a base
                       table cursor: the rows returned will be
                       updated */
  btr_pcur_t *pcur;    /*!< persistent cursor placed on the clustered
                       index record which should be updated or
                       deleted; the cursor is stored in the graph
                       of 'select' field above, except in the case
                       of the MySQL interface */
  dict_table_t *table; /*!< table where updated */
  upd_t *update;       /*!< update vector for the row */
  ulint update_n_fields;
  /* when this struct is used to implement
  a cascade operation for foreign keys, we store
  here the size of the buffer allocated for use
  as the update vector */
  sym_node_list_t columns; /* symbol table nodes for the columns
                           to retrieve from the table */
  bool has_clust_rec_x_lock;
  /* true if the select which retrieves the
  records to update already sets an x-lock on
  the clustered record; note that it must always
  set at least an s-lock */
  ulint cmpl_info; /* information extracted during query
                 compilation; speeds up execution:
                 UPD_NODE_NO_ORD_CHANGE and
                 UPD_NODE_NO_SIZE_CHANGE, ORed */
  /*----------------------*/
  /* Local storage for this graph node */
  ulint state;         /*!< node execution state */
  dict_index_t *index; /*!< NULL, or the next index whose record should
                       be updated */
  dtuple_t *row;       /*!< NULL, or a copy (also fields copied to
                       heap) of the row to update; this must be reset
                       to NULL after a successful update */
  row_ext_t *ext;      /*!< NULL, or prefixes of the externally
                       stored columns in the old row */
  dtuple_t *upd_row;   /* NULL, or a copy of the updated row */
  row_ext_t *upd_ext;  /* NULL, or prefixes of the externally
                       stored columns in upd_row */
  mem_heap_t *heap;    /*!< memory heap used as auxiliary storage;
                       this must be emptied after a successful
                       update */
  /*----------------------*/
  sym_node_t *table_sym; /* table node in symbol table */
  que_node_t *col_assign_list;
  /* column assignment list */

  /** When there is a lock wait error, this remembers current position of
  the multi-value field, before which the values have been deleted.
  This will be used for both DELETE and the delete phase of UPDATE. */
  uint32_t del_multi_val_pos;

  /** When there is a lock wait error, this remembers current position of
  the multi-value field, before which the values have been updated. */
  uint32_t upd_multi_val_pos;

  ulint magic_n;
};

constexpr uint32_t UPD_NODE_MAGIC_N = 1579975;

/* Node execution states */
/** execution came to the node from  a node above and if the field
has_clust_rec_x_lock is false, we should set an intention x-lock on the table
 */
constexpr uint32_t UPD_NODE_SET_IX_LOCK = 1;
/** clustered index record should be updated */
constexpr uint32_t UPD_NODE_UPDATE_CLUSTERED = 2;
/* clustered index record should be inserted, old record is already delete
 marked */
constexpr uint32_t UPD_NODE_INSERT_CLUSTERED = 3;
/** an ordering field of the clustered index record was changed, or this is  a
 delete operation: should update  all the secondary index records */
constexpr uint32_t UPD_NODE_UPDATE_ALL_SEC = 5;
/** secondary index entries should be looked at and updated if an ordering field
 changed */
constexpr uint32_t UPD_NODE_UPDATE_SOME_SEC = 6;

/* Compilation info flags: these must fit within 2 bits; see trx0rec.h */
/** no secondary index record will be changed in the update and no ordering
 field of the clustered index */
constexpr uint32_t UPD_NODE_NO_ORD_CHANGE = 1;
/** no record field size will be changed in the update */
constexpr uint32_t UPD_NODE_NO_SIZE_CHANGE = 2;
#endif /* !UNIV_HOTBACKUP */

#include "row0upd.ic"

#endif
