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
bool dtuple_coll_eq(const dtuple_t *tuple1, const dtuple_t *tuple2) {
  ulint n_fields;
  ulint i;
  int cmp;

  ut_ad(tuple1 != nullptr);
  ut_ad(tuple2 != nullptr);
  ut_ad(tuple1->magic_n == dtuple_t::MAGIC_N);
  ut_ad(tuple2->magic_n == dtuple_t::MAGIC_N);
  ut_ad(dtuple_check_typed(tuple1));
  ut_ad(dtuple_check_typed(tuple2));

  n_fields = dtuple_get_n_fields(tuple1);

  cmp = (int)n_fields - (int)dtuple_get_n_fields(tuple2);

  for (i = 0; cmp == 0 && i < n_fields; i++) {
    dfield_t *field1 = dtuple_get_nth_field(tuple1, i);
    const dfield_t *field2 = dtuple_get_nth_field(tuple2, i);

    ut_ad(dfield_get_len(field2) != UNIV_NO_INDEX_VALUE);
    ut_ad(!dfield_is_multi_value(field2) ||
          dfield_get_len(field2) != UNIV_MULTI_VALUE_ARRAY_MARKER);

    if (dfield_is_multi_value(field1)) {
      cmp = cmp_multi_value_dfield_dfield(field1, field2);
    } else {
      /* Equality comparison does not care about ASC/DESC. */
      cmp = cmp_dfield_dfield(field1, field2, true);
    }
  }

  return (cmp == 0);
}

void dtuple_set_n_fields(dtuple_t *tuple, ulint n_fields) {
  ut_ad(tuple);

  tuple->n_fields = n_fields;
  tuple->n_fields_cmp = n_fields;
}

/** Checks that a data field is typed.
@param[in] field                  Data field.
@return true if ok */
static bool dfield_check_typed_no_assert(const dfield_t *field) {
  if (dfield_get_type(field)->mtype > DATA_MTYPE_CURRENT_MAX ||
      dfield_get_type(field)->mtype < DATA_MTYPE_CURRENT_MIN) {
    ib::error(ER_IB_MSG_156)
        << "Data field type " << dfield_get_type(field)->mtype << ", len "
        << dfield_get_len(field);

    return (false);
  }

  return (true);
}

/** Checks that a data tuple is typed.
@param[in] tuple                Tuple to check.
@return true if ok */
static bool dtuple_check_typed_no_assert(const dtuple_t *tuple) {
  if (dtuple_get_n_fields(tuple) > REC_MAX_N_FIELDS) {
    ib::error(ER_IB_MSG_157)
        << "Index entry has " << dtuple_get_n_fields(tuple) << " fields";
  dump:
    fputs("InnoDB: Tuple contents: ", stderr);
    dtuple_print(stderr, tuple);
    putc('\n', stderr);

    return (false);
  }

  for (ulint i = 0; i < dtuple_get_n_fields(tuple); i++) {
    auto field = dtuple_get_nth_field(tuple, i);

    if (!dfield_check_typed_no_assert(field)) {
      goto dump;
    }
  }

  return (true);
}
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
bool dfield_check_typed(const dfield_t *field) {
  if (dfield_get_type(field)->mtype > DATA_MTYPE_CURRENT_MAX ||
      dfield_get_type(field)->mtype < DATA_MTYPE_CURRENT_MIN) {
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_158)
        << "Data field type " << dfield_get_type(field)->mtype << ", len "
        << dfield_get_len(field);
  }

  return (true);
}

bool dtuple_check_typed(const dtuple_t *tuple) {
  const dfield_t *field;
  ulint i;

  for (i = 0; i < dtuple_get_n_fields(tuple); i++) {
    field = dtuple_get_nth_field(tuple, i);

    ut_a(dfield_check_typed(field));
  }

  return (true);
}

bool dtuple_validate(const dtuple_t *tuple) {
  ut_ad(tuple->magic_n == dtuple_t::MAGIC_N);

  auto n_fields = dtuple_get_n_fields(tuple);

  /* We dereference all the data of each field to test
  for memory traps */

  for (ulint i = 0; i < n_fields; i++) {
    auto field = dtuple_get_nth_field(tuple, i);
    auto len = dfield_get_len(field);

    if (!dfield_is_null(field)) {
      const byte *data [[maybe_unused]];

      data = static_cast<const byte *>(dfield_get_data(field));
#ifndef UNIV_DEBUG_VALGRIND
      for (ulint j = 0; j < len; j++) {
        data++;
      }
#endif /* !UNIV_DEBUG_VALGRIND */

      UNIV_MEM_ASSERT_RW(data, len);
    }
  }

  ut_a(dtuple_check_typed(tuple));

  return (true);
}
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP
void dfield_print_also_hex(const dfield_t *dfield) {
  auto len = dfield_get_len(dfield);
  auto data = static_cast<const byte *>(dfield_get_data(dfield));

  if (dfield_is_null(dfield)) {
    fputs("NULL", stderr);

    return;
  }

  bool print_also_hex{};
  auto prtype = dtype_get_prtype(dfield_get_type(dfield));

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
      print_also_hex = false;

      for (ulint i = 0; i < len; i++) {
        int c = *data++;

        if (!isprint(c)) {
          print_also_hex = true;

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
      [[fallthrough]];

    case DATA_BINARY:
    default:
    print_hex:
      fputs(" Hex: ", stderr);

      for (ulint i = 0; i < len; i++) {
        fprintf(stderr, "%02lx", static_cast<ulong>(*data++));
      }

      if (dfield_is_ext(dfield)) {
        fputs("(external)", stderr);
      }
  }
}

/** Print a dfield value using ut_print_buf.
@param[in,out] f                Output stream.
@param[in]  dfield              Value to print. */
static void dfield_print_raw(FILE *f, const dfield_t *dfield) {
  ulint len = dfield_get_len(dfield);
  if (!dfield_is_null(dfield)) {
    ulint print_len = std::min(len, static_cast<ulint>(1000));
    ut_print_buf(f, dfield_get_data(dfield), print_len);
    if (len != print_len) {
      fprintf(f, "(total %lu bytes%s)", (ulong)len,
              dfield_is_ext(dfield) ? ", external" : "");
    }
  } else {
    fputs(" SQL NULL", f);
  }
}

void dtuple_print(FILE *f, const dtuple_t *tuple) {
  auto n_fields = dtuple_get_n_fields(tuple);

  fprintf(f, "DATA TUPLE: %lu fields;\n", (ulong)n_fields);

  for (ulint i = 0; i < n_fields; i++) {
    fprintf(f, " %lu:", (ulong)i);

    dfield_print_raw(f, dtuple_get_nth_field(tuple, i));

    putc(';', f);
    putc('\n', f);
  }

  ut_ad(dtuple_validate(tuple));
}

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

void dtuple_print(std::ostream &o, const dtuple_t *tuple) {
  const ulint n = dtuple_get_n_fields(tuple);

  o << "TUPLE (info_bits=" << dtuple_get_info_bits(tuple) << ", " << n
    << " n_cmp=" << tuple->n_fields_cmp << ", fields): {";

  dfield_print(o, tuple->fields, n);

  o << "}";
}

big_rec_t *dtuple_convert_big_rec(dict_index_t *index, upd_t *upd,
                                  dtuple_t *entry) {
  DBUG_TRACE;

  mem_heap_t *heap;
  big_rec_t *vector;
  dfield_t *dfield;
  dict_field_t *ifield;
  ulint size;
  ulint n_fields [[maybe_unused]];
  ulint local_len;
  ulint local_prefix_len;

  if (!index->is_clustered()) {
    return nullptr;
  }

  if (!dict_table_has_atomic_blobs(index->table)) {
    /* up to MySQL 5.1: store a 768-byte prefix locally */
    local_len = BTR_EXTERN_FIELD_REF_SIZE + DICT_ANTELOPE_MAX_INDEX_COL_LEN;
  } else {
    /* new-format table: do not store any BLOB prefix locally */
    local_len = BTR_EXTERN_FIELD_REF_SIZE;
  }

  ut_a(dtuple_check_typed_no_assert(entry));

  size = rec_get_converted_size(index, entry);

  if (size > 1000000000) {
    ib::warn(ER_IB_MSG_159) << "Tuple size is very big: " << size;
    fputs("InnoDB: Tuple contents: ", stderr);
    dtuple_print(stderr, entry);
    putc('\n', stderr);
  }

  heap = mem_heap_create(
      size + dtuple_get_n_fields(entry) * sizeof(big_rec_field_t) + 1000,
      UT_LOCATION_HERE);

  vector = big_rec_t::alloc(heap, dtuple_get_n_fields(entry));

  /* Decide which fields to shorten: the algorithm is to look for
  a variable-length field that yields the biggest savings when
  stored externally */

  n_fields = 0;

  while (page_zip_rec_needs_ext(
      rec_get_converted_size(index, entry), dict_table_is_comp(index->table),
      dict_index_get_n_fields(index), dict_table_page_size(index->table))) {
    byte *data;
    ulint longest = 0;
    ulint longest_i = ULINT_MAX;
    upd_field_t *uf = nullptr;

    for (ulint i = dict_index_get_n_unique_in_tree(index);
         i < dtuple_get_n_fields(entry); i++) {
      ulint savings;

      dfield = dtuple_get_nth_field(entry, i);
      ifield = index->get_field(i);

      ut_ad(dfield_get_len(dfield) != UNIV_SQL_INSTANT_DROP_COL);

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

      return nullptr;
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

    if (upd != nullptr &&
        ((uf = upd->get_field_by_field_no(longest_i, index)) != nullptr)) {
      /* When the externally stored LOB is going to be
      updated, the old LOB reference (BLOB pointer) can be
      used to access the old LOB object. So copy the LOB
      reference here. */

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
    ut_ad(n_fields < dtuple_get_n_fields(entry));

    if (upd && !upd->is_modified(longest_i)) {
      DEBUG_SYNC_C("ib_mv_nonupdated_column_offpage");

      upd_field_t upd_field;
      upd_field.field_no = longest_i;
      IF_DEBUG(upd_field.field_phy_pos =
                   index->get_field(longest_i)->col->get_col_phy_pos();)
      upd_field.orig_len = 0;
      upd_field.exp = nullptr;
      upd_field.old_v_val = nullptr;
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
  return vector;
}

void dtuple_convert_back_big_rec(dtuple_t *entry, big_rec_t *vector) {
  auto b = vector->fields;
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
    obj->data = nullptr;
  }

  return (obj);
}

byte *dfield_t::blobref() const {
  ut_ad(ext);

  return (static_cast<byte *>(data) + len - BTR_EXTERN_FIELD_REF_SIZE);
}

uint32_t dfield_t::lob_version() const {
  ut_ad(ext);
  byte *field_ref = blobref();

  lob::ref_t ref(field_ref);
  return (ref.version());
}

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

std::ostream &dfield_t::print(std::ostream &out) const {
  out << "[dfield_t: data=" << (void *)data << ", ext=" << ext << " ";

  if (dfield_is_ext(this)) {
    byte *tmp = static_cast<byte *>(data);
    lob::ref_t ref(tmp + len - lob::ref_t::SIZE);
    out << ref;
  }

  out << ", spatial_status=" << spatial_status << ", len=" << len
      << ", type=" << type << "]";

  return (out);
}

#ifdef UNIV_DEBUG
std::ostream &big_rec_field_t::print(std::ostream &out) const {
  out << "[big_rec_field_t: field_no=" << field_no << ", len=" << len
      << ", data=" << PrintBuffer(data, len) << ", ext_in_old=" << ext_in_old
      << ", ext_in_new=" << ext_in_new << "]";
  return (out);
}

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

void dtuple_t::ignore_trailing_default(const dict_index_t *index) {
  ut_a(index->has_instant_cols());
  ut_a(!index->has_row_versions());

  /* It's necessary to check all the fields that could be default.
  If it's from normal update, it should be OK to keep original
  default values in the physical record as is, however,
  if it's from rollback, it may rollback an update from default
  value to non-default. To make the rolled back record as is,
  it has to check all possible default values. */
  for (; n_fields > index->get_instant_fields(); --n_fields) {
    const dict_col_t *col = index->get_field(n_fields - 1)->col;

    /* We shall never come here if INSTANT ADD/DROP is done in a version. */
    ut_a(!col->is_instant_added());
    ut_a(!col->is_instant_dropped());

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

bool is_multi_value_clust_and_sec_equal(const byte *clust_field,
                                        uint64_t clust_len,
                                        const byte *sec_field, uint64_t sec_len,
                                        const dict_col_t *col) {
  if (clust_len == UNIV_SQL_NULL) {
    return (sec_len == UNIV_SQL_NULL);
  } else if (clust_len == UNIV_NO_INDEX_VALUE) {
    return (false);
  }

  ut_ad(clust_len == UNIV_MULTI_VALUE_ARRAY_MARKER);

  const multi_value_data *multi_value =
      reinterpret_cast<const multi_value_data *>(clust_field);
  ut_ad(multi_value != nullptr);
  return (multi_value->has(col->mtype, col->prtype, sec_field, sec_len));
}

bool multi_value_data::has(const dtype_t *type, const byte *data,
                           uint64_t len) const {
  return (has(type->mtype, type->prtype, data, len));
}

bool multi_value_data::has(ulint mtype, ulint prtype, const byte *data,
                           uint64_t len) const {
  for (uint32_t i = 0; i < num_v; ++i) {
    if (cmp_data_data(mtype, prtype, true,
                      reinterpret_cast<const byte *>(datap[i]), data_len[i],
                      data, len) == 0) {
      return (true);
    }
  }

  return (false);
}

#endif /* !UNIV_HOTBACKUP */

void multi_value_data::alloc(uint32_t num, bool alc_bitset, mem_heap_t *heap) {
  datap =
      static_cast<const void **>(mem_heap_zalloc(heap, num * sizeof(*datap)));
  data_len =
      static_cast<uint32_t *>(mem_heap_zalloc(heap, num * sizeof(*data_len)));
  conv_buf =
      static_cast<uint64_t *>(mem_heap_zalloc(heap, num * sizeof(*conv_buf)));

  num_alc = num;

  if (alc_bitset) {
    alloc_bitset(heap, num);
  } else {
    bitset = nullptr;
  }
}

void multi_value_data::alloc_bitset(mem_heap_t *heap, uint32_t size) {
  ut_ad(bitset == nullptr);

  bitset = static_cast<Bitset *>(mem_heap_zalloc(heap, sizeof(Bitset)));
  uint32_t alloc_size = (size == 0 ? num_v : size);
  byte *bitmap =
      static_cast<byte *>(mem_heap_zalloc(heap, UT_BITS_IN_BYTES(alloc_size)));
  bitset->init(bitmap, UT_BITS_IN_BYTES(alloc_size));
  bitset->set();
}

uint32_t Multi_value_logger::get_log_len(bool precise) const {
  /* Alwayas a multi-value data marker at the beginning */
  uint32_t total_len = 1;

  if (m_field_len == UNIV_SQL_NULL || m_field_len == UNIV_NO_INDEX_VALUE) {
    total_len += s_log_length_for_null_or_empty;
    return (total_len);
  }

  ut_ad(m_field_len == UNIV_MULTI_VALUE_ARRAY_MARKER);

  /* Keep two bytes for the total length of the log */
  total_len += 2;

  /* Remember the length of the multi-value array.
  Will write the data_len[i] in compressed format which at most
  costs 5 bytes */
  total_len += precise ? mach_get_compressed_size(m_mv_data->num_v)
                       : s_max_compressed_mv_key_length_size;

  /* Remember each data length and value */
  for (uint32_t i = 0; i < m_mv_data->num_v; ++i) {
    ut_ad(m_mv_data->data_len[i] != UNIV_SQL_NULL);
    total_len += m_mv_data->data_len[i];
    total_len += precise ? mach_get_compressed_size(m_mv_data->data_len[i])
                         : s_max_compressed_mv_key_length_size;
  }

  /* Remember the bitset of the multi-value data if exists*/
  if (m_mv_data->bitset != nullptr) {
    total_len += UT_BITS_IN_BYTES(m_mv_data->num_v);
  }

  return (total_len);
}

byte *Multi_value_logger::log(byte **ptr) {
  mach_write_to_1(*ptr, s_multi_value_virtual_col_length_marker);
  *ptr += 1;

  if (m_field_len == UNIV_SQL_NULL) {
    mach_write_to_2(*ptr, s_multi_value_null);
    *ptr += s_log_length_for_null_or_empty;
    return (*ptr);
  } else if (m_field_len == UNIV_NO_INDEX_VALUE) {
    mach_write_to_2(*ptr, s_multi_value_no_index_value);
    *ptr += s_log_length_for_null_or_empty;
    return (*ptr);
  }

  ut_ad(m_field_len == UNIV_MULTI_VALUE_ARRAY_MARKER);
  byte *old_ptr = *ptr;

  /* Store data in the same sequence as described in get_log_len() */
  *ptr += 2;
  *ptr += mach_write_compressed(*ptr, m_mv_data->num_v);
  for (uint32_t i = 0; i < m_mv_data->num_v; ++i) {
    ut_ad(m_mv_data->data_len[i] != UNIV_SQL_NULL);
    *ptr += mach_write_compressed(*ptr, m_mv_data->data_len[i]);
    ut_memcpy(*ptr, m_mv_data->datap[i], m_mv_data->data_len[i]);
    *ptr += m_mv_data->data_len[i];
  }

  if (m_mv_data->bitset != nullptr) {
    /* Always just write out the bitset of enough size for all data,
    rather than the size of bitset. */
    uint32_t bitset_len = UT_BITS_IN_BYTES(m_mv_data->num_v);
    ut_memcpy(*ptr, m_mv_data->bitset->bitset(), bitset_len);
    *ptr += bitset_len;
  }

  mach_write_to_2(old_ptr, *ptr - old_ptr);

  return (*ptr);
}

uint32_t Multi_value_logger::read_log_len(const byte *ptr) {
  ut_ad(is_multi_value_log(ptr));

  uint32_t total_len = mach_read_from_2(ptr + 1);

  if (total_len == s_multi_value_null ||
      total_len == s_multi_value_no_index_value) {
    return (1 + s_log_length_for_null_or_empty);
  }

  return (1 + total_len);
}

const byte *Multi_value_logger::read(const byte *ptr, dfield_t *field,
                                     mem_heap_t *heap) {
  ut_ad(is_multi_value_log(ptr));

  ++ptr;

  uint32_t total_len = mach_read_from_2(ptr);
  const byte *old_ptr = ptr;

  ptr += s_log_length_for_null_or_empty;

  if (total_len == s_multi_value_null) {
    dfield_set_null(field);
    return (ptr);
  } else if (total_len == s_multi_value_no_index_value) {
    dfield_set_data(field, nullptr, UNIV_NO_INDEX_VALUE);
    return (ptr);
  }

  uint32_t num = mach_read_next_compressed(&ptr);
  field->data = mem_heap_alloc(heap, sizeof(multi_value_data));
  field->len = UNIV_MULTI_VALUE_ARRAY_MARKER;

  multi_value_data *multi_val = static_cast<multi_value_data *>(field->data);

  multi_val->num_v = num;
  multi_val->alloc(num, false, heap);

  for (uint32_t i = 0; i < num; ++i) {
    uint32_t len = mach_read_next_compressed(&ptr);
    multi_val->datap[i] = ptr;
    multi_val->data_len[i] = len;

    ptr += len;
  }

  if (ptr < old_ptr + total_len) {
    multi_val->alloc_bitset(heap);
    multi_val->bitset->copy(ptr, UT_BITS_IN_BYTES(num));
    ptr += UT_BITS_IN_BYTES(num);
  }

  ut_ad(ptr == old_ptr + total_len);
  return (ptr);
}

uint32_t Multi_value_logger::get_keys_capacity(uint32_t log_size,
                                               uint32_t key_length,
                                               uint32_t *num_keys) {
  uint32_t keys_length = log_size;

  /* The calculation should be based on the ::log(), how it logs will
  affect how the capacities calculated. And to be safe, in this function,
  the length of each key would be assumed to be always the key_length
  passed in, regardless of how actual data will consume. */

  /* Exclude the bytes for multi-value marker, total log length and
  number of keys */
  keys_length -= (1 + 2 + 2);

  /* Ignore the bitset too, to make the estimation simple */

  *num_keys =
      keys_length /
      (key_length + Multi_value_logger::s_max_compressed_mv_key_length_size);

  /* Total key length should also exclude the bytes for length of each key */
  keys_length -=
      *num_keys * Multi_value_logger::s_max_compressed_mv_key_length_size;

  return (keys_length);
}
