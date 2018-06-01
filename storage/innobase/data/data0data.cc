/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file data/data0data.cc
 SQL data field and tuple

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

#include <sys/types.h>
#include <new>

#include "data0data.h"
#include "ha_prototypes.h"

#ifndef UNIV_HOTBACKUP
#include "btr0cur.h"
#include "dict0dict.h"
#include "lob0lob.h"
#include "page0page.h"
#include "page0zip.h"
#include "rem0cmp.h"
#include "rem0rec.h"
#include "row0mysql.h"
#include "row0upd.h"

#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/** Dummy variable to catch access to uninitialized fields.  In the
debug version, dtuple_create() will make all fields of dtuple_t point
to data_error. */
byte data_error;
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP
/** Compare two data tuples.
@param[in] tuple1 first data tuple
@param[in] tuple2 second data tuple
@return whether tuple1 == tuple2 */
bool dtuple_coll_eq(const dtuple_t *tuple1, const dtuple_t *tuple2) {
  ulint n_fields;
  ulint i;
  int cmp;

  ut_ad(tuple1 != NULL);
  ut_ad(tuple2 != NULL);
  ut_ad(tuple1->magic_n == DATA_TUPLE_MAGIC_N);
  ut_ad(tuple2->magic_n == DATA_TUPLE_MAGIC_N);
  ut_ad(dtuple_check_typed(tuple1));
  ut_ad(dtuple_check_typed(tuple2));

  n_fields = dtuple_get_n_fields(tuple1);

  cmp = (int)n_fields - (int)dtuple_get_n_fields(tuple2);

  for (i = 0; cmp == 0 && i < n_fields; i++) {
    const dfield_t *field1 = dtuple_get_nth_field(tuple1, i);
    const dfield_t *field2 = dtuple_get_nth_field(tuple2, i);
    /* Equality comparison does not care about ASC/DESC. */
    cmp = cmp_dfield_dfield(field1, field2, true);
  }

  return (cmp == 0);
}

/** Sets number of fields used in a tuple. Normally this is set in
 dtuple_create, but if you want later to set it smaller, you can use this. */
void dtuple_set_n_fields(dtuple_t *tuple, /*!< in: tuple */
                         ulint n_fields)  /*!< in: number of fields */
{
  ut_ad(tuple);

  tuple->n_fields = n_fields;
  tuple->n_fields_cmp = n_fields;
}

/** Checks that a data field is typed.
 @return true if ok */
static ibool dfield_check_typed_no_assert(
    const dfield_t *field) /*!< in: data field */
{
  if (dfield_get_type(field)->mtype > DATA_MTYPE_CURRENT_MAX ||
      dfield_get_type(field)->mtype < DATA_MTYPE_CURRENT_MIN) {
    ib::error(ER_IB_MSG_156)
        << "Data field type " << dfield_get_type(field)->mtype << ", len "
        << dfield_get_len(field);

    return (FALSE);
  }

  return (TRUE);
}

/** Checks that a data tuple is typed.
 @return true if ok */
static ibool dtuple_check_typed_no_assert(
    const dtuple_t *tuple) /*!< in: tuple */
{
  const dfield_t *field;
  ulint i;

  if (dtuple_get_n_fields(tuple) > REC_MAX_N_FIELDS) {
    ib::error(ER_IB_MSG_157)
        << "Index entry has " << dtuple_get_n_fields(tuple) << " fields";
  dump:
    fputs("InnoDB: Tuple contents: ", stderr);
    dtuple_print(stderr, tuple);
    putc('\n', stderr);

    return (FALSE);
  }

  for (i = 0; i < dtuple_get_n_fields(tuple); i++) {
    field = dtuple_get_nth_field(tuple, i);

    if (!dfield_check_typed_no_assert(field)) {
      goto dump;
    }
  }

  return (TRUE);
}
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/** Checks that a data field is typed. Asserts an error if not.
 @return true if ok */
ibool dfield_check_typed(const dfield_t *field) /*!< in: data field */
{
  if (dfield_get_type(field)->mtype > DATA_MTYPE_CURRENT_MAX ||
      dfield_get_type(field)->mtype < DATA_MTYPE_CURRENT_MIN) {
    ib::fatal(ER_IB_MSG_158)
        << "Data field type " << dfield_get_type(field)->mtype << ", len "
        << dfield_get_len(field);
  }

  return (TRUE);
}

/** Checks that a data tuple is typed. Asserts an error if not.
 @return true if ok */
ibool dtuple_check_typed(const dtuple_t *tuple) /*!< in: tuple */
{
  const dfield_t *field;
  ulint i;

  for (i = 0; i < dtuple_get_n_fields(tuple); i++) {
    field = dtuple_get_nth_field(tuple, i);

    ut_a(dfield_check_typed(field));
  }

  return (TRUE);
}

/** Validates the consistency of a tuple which must be complete, i.e,
 all fields must have been set.
 @return true if ok */
ibool dtuple_validate(const dtuple_t *tuple) /*!< in: tuple */
{
  const dfield_t *field;
  ulint n_fields;
  ulint len;
  ulint i;

  ut_ad(tuple->magic_n == DATA_TUPLE_MAGIC_N);

  n_fields = dtuple_get_n_fields(tuple);

  /* We dereference all the data of each field to test
  for memory traps */

  for (i = 0; i < n_fields; i++) {
    field = dtuple_get_nth_field(tuple, i);
    len = dfield_get_len(field);

    if (!dfield_is_null(field)) {
      const byte *data;

      data = static_cast<const byte *>(dfield_get_data(field));
#ifndef UNIV_DEBUG_VALGRIND
      ulint j;

      for (j = 0; j < len; j++) {
        data++;
      }
#endif /* !UNIV_DEBUG_VALGRIND */

      UNIV_MEM_ASSERT_RW(data, len);
    }
  }

  ut_a(dtuple_check_typed(tuple));

  return (TRUE);
}
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP
/** Pretty prints a dfield value according to its data type. Also the hex string
 is printed if a string contains non-printable characters. */
void dfield_print_also_hex(const dfield_t *dfield) /*!< in: dfield */
{
  const byte *data;
  ulint len;
  ulint prtype;
  ulint i;
  ibool print_also_hex;

  len = dfield_get_len(dfield);
  data = static_cast<const byte *>(dfield_get_data(dfield));

  if (dfield_is_null(dfield)) {
    fputs("NULL", stderr);

    return;
  }

  prtype = dtype_get_prtype(dfield_get_type(dfield));

  switch (dtype_get_mtype(dfield_get_type(dfield))) {
    ib_id_t id;
    case DATA_INT:
      switch (len) {
        ulint val;
        case 1:
          val = mach_read_from_1(data);

          if (!(prtype & DATA_UNSIGNED)) {
            val &= ~0x80;
            fprintf(stderr, "%ld", (long)val);
          } else {
            fprintf(stderr, "%lu", (ulong)val);
          }
          break;

        case 2:
          val = mach_read_from_2(data);

          if (!(prtype & DATA_UNSIGNED)) {
            val &= ~0x8000;
            fprintf(stderr, "%ld", (long)val);
          } else {
            fprintf(stderr, "%lu", (ulong)val);
          }
          break;

        case 3:
          val = mach_read_from_3(data);

          if (!(prtype & DATA_UNSIGNED)) {
            val &= ~0x800000;
            fprintf(stderr, "%ld", (long)val);
          } else {
            fprintf(stderr, "%lu", (ulong)val);
          }
          break;

        case 4:
          val = mach_read_from_4(data);

          if (!(prtype & DATA_UNSIGNED)) {
            val &= ~0x80000000;
            fprintf(stderr, "%ld", (long)val);
          } else {
            fprintf(stderr, "%lu", (ulong)val);
          }
          break;

        case 6:
          id = mach_read_from_6(data);
          fprintf(stderr, IB_ID_FMT, id);
          break;

        case 7:
          id = mach_read_from_7(data);
          fprintf(stderr, IB_ID_FMT, id);
          break;
        case 8:
          id = mach_read_from_8(data);
          fprintf(stderr, IB_ID_FMT, id);
          break;
        default:
          goto print_hex;
      }
      break;

    case DATA_SYS:
      switch (prtype & DATA_SYS_PRTYPE_MASK) {
        case DATA_TRX_ID:
          id = mach_read_from_6(data);

          fprintf(stderr, "trx_id " TRX_ID_FMT, id);
          break;

        case DATA_ROLL_PTR:
          id = mach_read_from_7(data);

          fprintf(stderr, "roll_ptr " TRX_ID_FMT, id);
          break;

        case DATA_ROW_ID:
          id = mach_read_from_6(data);

          fprintf(stderr, "row_id " TRX_ID_FMT, id);
          break;

        default:
          goto print_hex;
      }
      break;

    case DATA_CHAR:
    case DATA_VARCHAR:
      print_also_hex = FALSE;

      for (i = 0; i < len; i++) {
        int c = *data++;

        if (!isprint(c)) {
          print_also_hex = TRUE;

          fprintf(stderr, "\\x%02x", (unsigned char)c);
        } else {
          putc(c, stderr);
        }
      }

      if (dfield_is_ext(dfield)) {
        fputs("(external)", stderr);
      }

      if (!print_also_hex) {
        break;
      }

      data = static_cast<byte *>(dfield_get_data(dfield));
      /* fall through */

    case DATA_BINARY:
    default:
    print_hex:
      fputs(" Hex: ", stderr);

      for (i = 0; i < len; i++) {
        fprintf(stderr, "%02lx", static_cast<ulong>(*data++));
      }

      if (dfield_is_ext(dfield)) {
        fputs("(external)", stderr);
      }
  }
}

/** Print a dfield value using ut_print_buf. */
static void dfield_print_raw(FILE *f,                /*!< in: output stream */
                             const dfield_t *dfield) /*!< in: dfield */
{
  ulint len = dfield_get_len(dfield);
  if (!dfield_is_null(dfield)) {
    ulint print_len = ut_min(len, static_cast<ulint>(1000));
    ut_print_buf(f, dfield_get_data(dfield), print_len);
    if (len != print_len) {
      fprintf(f, "(total %lu bytes%s)", (ulong)len,
              dfield_is_ext(dfield) ? ", external" : "");
    }
  } else {
    fputs(" SQL NULL", f);
  }
}

/** The following function prints the contents of a tuple. */
void dtuple_print(FILE *f,               /*!< in: output stream */
                  const dtuple_t *tuple) /*!< in: tuple */
{
  ulint n_fields;
  ulint i;

  n_fields = dtuple_get_n_fields(tuple);

  fprintf(f, "DATA TUPLE: %lu fields;\n", (ulong)n_fields);

  for (i = 0; i < n_fields; i++) {
    fprintf(f, " %lu:", (ulong)i);

    dfield_print_raw(f, dtuple_get_nth_field(tuple, i));

    putc(';', f);
    putc('\n', f);
  }

  ut_ad(dtuple_validate(tuple));
}

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	field	array of data fields
@param[in]	n	number of data fields */
void dfield_print(std::ostream &o, const dfield_t *field, ulint n) {
  for (ulint i = 0; i < n; i++, field++) {
    const void *data = dfield_get_data(field);
    const ulint len = dfield_get_len(field);

    if (i) {
      o << ',';
    }

    if (dfield_is_null(field)) {
      o << "NULL";
    } else if (dfield_is_ext(field)) {
      ulint local_len = len - BTR_EXTERN_FIELD_REF_SIZE;
      ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);

      o << '[' << local_len << '+' << BTR_EXTERN_FIELD_REF_SIZE << ']';
      ut_print_buf(o, data, local_len);
      ut_print_buf_hex(o, static_cast<const byte *>(data) + local_len,
                       BTR_EXTERN_FIELD_REF_SIZE);
    } else {
      o << '[' << len << ']';
      ut_print_buf(o, data, len);
    }
  }
}

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	tuple	data tuple */
void dtuple_print(std::ostream &o, const dtuple_t *tuple) {
  const ulint n = dtuple_get_n_fields(tuple);

  o << "TUPLE (info_bits=" << dtuple_get_info_bits(tuple) << ", " << n
    << " fields): {";

  dfield_print(o, tuple->fields, n);

  o << "}";
}

/** Moves parts of long fields in entry to the big record vector so that
 the size of tuple drops below the maximum record size allowed in the
 database. Moves data only from those fields which are not necessary
 to determine uniquely the insertion place of the tuple in the index.
 @return own: created big record vector, NULL if we are not able to
 shorten the entry enough, i.e., if there are too many fixed-length or
 short fields in entry or the index is clustered */
big_rec_t *dtuple_convert_big_rec(dict_index_t *index, /*!< in: index */
                                  upd_t *upd,      /*!< in/out: update vector */
                                  dtuple_t *entry, /*!< in/out: index entry */
                                  ulint *n_ext)    /*!< in/out: number of
                                                   externally stored columns */
{
  DBUG_ENTER("dtuple_convert_big_rec");

  mem_heap_t *heap;
  big_rec_t *vector;
  dfield_t *dfield;
  dict_field_t *ifield;
  ulint size;
  ulint n_fields;
  ulint local_len;
  ulint local_prefix_len;

  if (!index->is_clustered()) {
    DBUG_RETURN(NULL);
  }

  if (!dict_table_has_atomic_blobs(index->table)) {
    /* up to MySQL 5.1: store a 768-byte prefix locally */
    local_len = BTR_EXTERN_FIELD_REF_SIZE + DICT_ANTELOPE_MAX_INDEX_COL_LEN;
  } else {
    /* new-format table: do not store any BLOB prefix locally */
    local_len = BTR_EXTERN_FIELD_REF_SIZE;
  }

  ut_a(dtuple_check_typed_no_assert(entry));

  size = rec_get_converted_size(index, entry, *n_ext);

  if (UNIV_UNLIKELY(size > 1000000000)) {
    ib::warn(ER_IB_MSG_159) << "Tuple size is very big: " << size;
    fputs("InnoDB: Tuple contents: ", stderr);
    dtuple_print(stderr, entry);
    putc('\n', stderr);
  }

  heap = mem_heap_create(
      size + dtuple_get_n_fields(entry) * sizeof(big_rec_field_t) + 1000);

  vector = big_rec_t::alloc(heap, dtuple_get_n_fields(entry));

  /* Decide which fields to shorten: the algorithm is to look for
  a variable-length field that yields the biggest savings when
  stored externally */

  n_fields = 0;

  while (page_zip_rec_needs_ext(rec_get_converted_size(index, entry, *n_ext),
                                dict_table_is_comp(index->table),
                                dict_index_get_n_fields(index),
                                dict_table_page_size(index->table))) {
    ulint i;
    ulint longest = 0;
    ulint longest_i = ULINT_MAX;
    byte *data;
    upd_field_t *uf = nullptr;

    for (i = dict_index_get_n_unique_in_tree(index);
         i < dtuple_get_n_fields(entry); i++) {
      ulint savings;

      dfield = dtuple_get_nth_field(entry, i);
      ifield = index->get_field(i);

      /* Skip fixed-length, NULL, externally stored,
      or short columns */

      if (ifield->fixed_len || dfield_is_null(dfield) ||
          dfield_is_ext(dfield) || dfield_get_len(dfield) <= local_len ||
          dfield_get_len(dfield) <= BTR_EXTERN_LOCAL_STORED_MAX_SIZE) {
        goto skip_field;
      }

      savings = dfield_get_len(dfield) - local_len;

      /* Check that there would be savings */
      if (longest >= savings) {
        goto skip_field;
      }

      /* In DYNAMIC and COMPRESSED format, store
      locally any non-BLOB columns whose maximum
      length does not exceed 256 bytes.  This is
      because there is no room for the "external
      storage" flag when the maximum length is 255
      bytes or less. This restriction trivially
      holds in REDUNDANT and COMPACT format, because
      there we always store locally columns whose
      length is up to local_len == 788 bytes.
      @see rec_init_offsets_comp_ordinary */
      if (!DATA_BIG_COL(ifield->col)) {
        goto skip_field;
      }

      longest_i = i;
      longest = savings;

    skip_field:
      continue;
    }

    if (!longest) {
      /* Cannot shorten more */

      mem_heap_free(heap);

      DBUG_RETURN(NULL);
    }

    /* Move data from field longest_i to big rec vector.

    We store the first bytes locally to the record. Then
    we can calculate all ordering fields in all indexes
    from locally stored data. */

    dfield = dtuple_get_nth_field(entry, longest_i);

    ifield = index->get_field(longest_i);
    local_prefix_len = local_len - BTR_EXTERN_FIELD_REF_SIZE;

    big_rec_field_t big_rec(
        longest_i, dfield_get_len(dfield) - local_prefix_len,
        static_cast<char *>(dfield_get_data(dfield)) + local_prefix_len);

    /* Allocate the locally stored part of the column. */
    data = static_cast<byte *>(mem_heap_alloc(heap, local_len));

    /* Copy the local prefix (including LOB pointer). */
    memcpy(data, dfield_get_data(dfield), local_len);

    /* Clear the extern field reference (BLOB pointer). */
    memset(data + local_prefix_len, 0, BTR_EXTERN_FIELD_REF_SIZE);

    if (upd != nullptr && upd->is_modified(longest_i)) {
      /* When the externally stored LOB is going to be
      updated, the old LOB reference (BLOB pointer) can be
      used to access the old LOB object. So copy the LOB
      reference here. */
      uf = upd->get_field_by_field_no(longest_i, index);

      if (dfield_is_ext(&uf->old_val)) {
        byte *field_ref = static_cast<byte *>(dfield_get_data(&uf->old_val)) +
                          local_prefix_len;
        memcpy(data + local_prefix_len, field_ref, lob::ref_t::SIZE);
      }
    }

#if 0
		/* The following would fail the Valgrind checks in
		page_cur_insert_rec_low() and page_cur_insert_rec_zip().
		The BLOB pointers in the record will be initialized after
		the record and the BLOBs have been written. */
		UNIV_MEM_ALLOC(data + local_prefix_len,
			       BTR_EXTERN_FIELD_REF_SIZE);
#endif

    dfield_set_data(dfield, data, local_len);
    dfield_set_ext(dfield);

    n_fields++;
    (*n_ext)++;
    ut_ad(n_fields < dtuple_get_n_fields(entry));

    if (upd && !upd->is_modified(longest_i)) {
      DEBUG_SYNC_C("ib_mv_nonupdated_column_offpage");

      upd_field_t upd_field;
      upd_field.field_no = longest_i;
      upd_field.orig_len = 0;
      upd_field.exp = NULL;
      upd_field.old_v_val = NULL;
      upd_field.ext_in_old = dfield_is_ext(dfield);
      dfield_copy(&upd_field.new_val, dfield->clone(upd->heap));
      upd->append(upd_field);
      ut_ad(upd->is_modified(longest_i));

      ut_ad(upd_field.new_val.len >= BTR_EXTERN_FIELD_REF_SIZE);
      ut_ad(upd_field.new_val.len == local_len);
      ut_ad(upd_field.new_val.len == dfield_get_len(dfield));
    }

    if (upd == nullptr) {
      big_rec.ext_in_old = false;
    } else {
      upd_field_t *uf = upd->get_field_by_field_no(longest_i, index);
      ut_ad(uf != nullptr);
      big_rec.ext_in_old = uf->ext_in_old;
    }

    big_rec.ext_in_new = true;
    vector->append(big_rec);
  }

  ut_ad(n_fields == vector->n_fields);
  DBUG_RETURN(vector);
}

/** Puts back to entry the data stored in vector. Note that to ensure the
 fields in entry can accommodate the data, vector must have been created
 from entry with dtuple_convert_big_rec. */
void dtuple_convert_back_big_rec(
    dict_index_t *index MY_ATTRIBUTE((unused)), /*!< in: index */
    dtuple_t *entry,   /*!< in: entry whose data was put to vector */
    big_rec_t *vector) /*!< in, own: big rec vector; it is
                       freed in this function */
{
  big_rec_field_t *b = vector->fields;
  const big_rec_field_t *const end = b + vector->n_fields;

  for (; b < end; b++) {
    dfield_t *dfield;
    ulint local_len;

    dfield = dtuple_get_nth_field(entry, b->field_no);
    local_len = dfield_get_len(dfield);

    ut_ad(dfield_is_ext(dfield));
    ut_ad(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

    local_len -= BTR_EXTERN_FIELD_REF_SIZE;

    /* Only in REDUNDANT and COMPACT format, we store
    up to DICT_ANTELOPE_MAX_INDEX_COL_LEN (768) bytes
    locally */
    ut_ad(local_len <= DICT_ANTELOPE_MAX_INDEX_COL_LEN);

    dfield_set_data(dfield, (char *)b->data - local_len, b->len + local_len);
  }

  mem_heap_free(vector->heap);
}

/** Allocate a big_rec_t object in the given memory heap, and for storing
n_fld number of fields.
@param[in]	heap	memory heap in which this object is allocated
@param[in]	n_fld	maximum number of fields that can be stored in
                        this object

@return the allocated object */
big_rec_t *big_rec_t::alloc(mem_heap_t *heap, ulint n_fld) {
  big_rec_t *rec =
      static_cast<big_rec_t *>(mem_heap_alloc(heap, sizeof(big_rec_t)));

  new (rec) big_rec_t(n_fld);

  rec->heap = heap;
  rec->fields = static_cast<big_rec_field_t *>(
      mem_heap_alloc(heap, n_fld * sizeof(big_rec_field_t)));

  rec->n_fields = 0;
  return (rec);
}

/** Create a deep copy of this object
@param[in]	heap	the memory heap in which the clone will be
                        created.

@return	the cloned object. */
dfield_t *dfield_t::clone(mem_heap_t *heap) {
  const ulint size = len == UNIV_SQL_NULL ? 0 : len;
  dfield_t *obj =
      static_cast<dfield_t *>(mem_heap_alloc(heap, sizeof(dfield_t) + size));

  obj->ext = ext;
  obj->len = len;
  obj->type = type;
  obj->spatial_status = spatial_status;

  if (len != UNIV_SQL_NULL) {
    obj->data = obj + 1;
    memcpy(obj->data, data, len);
  } else {
    obj->data = 0;
  }

  return (obj);
}

byte *dfield_t::blobref() const {
  ut_ad(ext);

  return (static_cast<byte *>(data) + len - BTR_EXTERN_FIELD_REF_SIZE);
}

ulint dfield_t::lob_version() const {
  ut_ad(ext);
  byte *field_ref = blobref();

  lob::ref_t ref(field_ref);
  return (ref.version());
}

/** Adjust and(or) set virtual column value which is read from undo
or online DDL log
@param[in]	vcol		virtual column definition
@param[in]	comp		true if compact format
@param[in]	field		virtual column value
@param[in]	len		value length
@param[in,out]	heap		memory heap to keep value when necessary */
void dfield_t::adjust_v_data_mysql(const dict_v_col_t *vcol, bool comp,
                                   const byte *field, ulint len,
                                   mem_heap_t *heap) {
  ulint mtype;
  const byte *data = field;

  ut_ad(heap != nullptr);

  mtype = type.mtype;

  if (mtype != DATA_MYSQL) {
    dfield_set_data(this, field, len);
    return;
  }

  /* Adjust the value if the data type is DATA_MYSQL, either
  adding or striping trailing spaces when necessary. This may happen
  in the scenario where there is an ALTER TABLE changing table's
  row format from compact to non-compact or vice versa, and there
  is also concurrent INSERT to this table. The log for the data could
  be in different format from the final format, which should be adjusted.
  Refer to row_mysql_store_col_in_innobase_format() too. */
  if (comp && len == vcol->m_col.len && dtype_get_mbminlen(&type) == 1 &&
      dtype_get_mbmaxlen(&type) > 1) {
    /* A full length record, which is of multibyte
    charsets and recorded because old table is non-compact.
    However, in compact table, no trailing spaces. */
    ulint n_chars;

    ut_a(!(dtype_get_len(&type) % dtype_get_mbmaxlen(&type)));

    n_chars = dtype_get_len(&type) / dtype_get_mbmaxlen(&type);

    while (len > n_chars && data[len - 1] == 0x20) {
      --len;
    }
  } else if (!comp && len < vcol->m_col.len && dtype_get_mbminlen(&type) == 1) {
    /* A not full length record from compact table, so have to
    add trailing spaces. */
    byte *v_data =
        reinterpret_cast<byte *>(mem_heap_alloc(heap, vcol->m_col.len));

    memcpy(v_data, field, len);
    row_mysql_pad_col(1, v_data + len, vcol->m_col.len - len);

    data = v_data;
    len = vcol->m_col.len;
  }

  dfield_set_data(this, data, len);
}

/** Print the dfield_t object into the given output stream.
@param[in]	out	the output stream.
@return	the ouput stream. */
std::ostream &dfield_t::print(std::ostream &out) const {
  out << "[dfield_t: data=" << (void *)data << ", ext=" << ext << " ";

  if (dfield_is_ext(this)) {
    byte *tmp = static_cast<byte *>(data);
    lob::ref_t ref(tmp + len - lob::ref_t::SIZE);
    out << ref;
  }

  out << ", spatial_status=" << spatial_status << ", len=" << len << ", type="
      << "]";

  return (out);
}

#ifdef UNIV_DEBUG
/** Print the big_rec_field_t object into the given output stream.
@param[in]	out	the output stream.
@return	the ouput stream. */
std::ostream &big_rec_field_t::print(std::ostream &out) const {
  out << "[big_rec_field_t: field_no=" << field_no << ", len=" << len
      << ", data=" << PrintBuffer(data, len) << ", ext_in_old=" << ext_in_old
      << ", ext_in_new=" << ext_in_new << "]";
  return (out);
}

/** Print the current object into the given output stream.
@param[in]	out	the output stream.
@return	the ouput stream. */
std::ostream &big_rec_t::print(std::ostream &out) const {
  out << "[big_rec_t: capacity=" << capacity << ", n_fields=" << n_fields
      << " ";
  for (ulint i = 0; i < n_fields; ++i) {
    out << fields[i];
  }
  out << "]";
  return (out);
}
#endif /* UNIV_DEBUG */

/* Read the trx id from the tuple (DB_TRX_ID)
@return transaction id of the tuple. */
trx_id_t dtuple_t::get_trx_id() const {
  for (ulint i = 0; i < n_fields; ++i) {
    dfield_t &field = fields[i];

    uint32_t prtype = field.type.prtype & DATA_SYS_PRTYPE_MASK;

    if (field.type.mtype == DATA_SYS && prtype == DATA_TRX_ID) {
      return (mach_read_from_6((byte *)field.data));
    }
  }

  return (0);
}

/** Ignore trailing default fields if this is a tuple from instant index
@param[in]	index		clustered index object for this tuple */
void dtuple_t::ignore_trailing_default(const dict_index_t *index) {
  if (!index->has_instant_cols()) {
    return;
  }

  /* It's necessary to check all the fields that could be default.
  If it's from normal update, it should be OK to keep original
  default values in the physical record as is, however,
  if it's from rollback, it may rollback an update from default
  value to non-default. To make the rolled back record as is,
  it has to check all possible default values. */
  for (; n_fields > index->get_instant_fields(); --n_fields) {
    const dict_col_t *col = index->get_field(n_fields - 1)->col;
    const dfield_t *dfield = dtuple_get_nth_field(this, n_fields - 1);
    ulint len = dfield_get_len(dfield);

    ut_ad(col->instant_default != nullptr);

    if (len != col->instant_default->len ||
        (len != UNIV_SQL_NULL &&
         memcmp(dfield_get_data(dfield), col->instant_default->value, len) !=
             0)) {
      break;
    }
  }
}

#endif /* !UNIV_HOTBACKUP */
