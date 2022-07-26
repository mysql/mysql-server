/*****************************************************************************
Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

/** @file include/rem0wrec.h
 Record manager wrapper implementation

 Created 13/08/2021 Mayank Prasad
******************************************************************************/

#include "rem0wrec.h"
#include "rem0lrec.h"

byte *rec_get_nth_field(const dict_index_t *index, const rec_t *rec,
                        const ulint *offsets, ulint n, ulint *len) {
  byte *field =
      const_cast<byte *>(rec) + rec_get_nth_field_offs(index, offsets, n, len);
  return (field);
}

const byte *rec_get_nth_field_old(const dict_index_t *index, const rec_t *rec,
                                  ulint n, ulint *len) {
  const byte *field = rec + rec_get_nth_field_offs_old(index, rec, n, len);
  return (field);
}

ulint rec_get_nth_field_size(const dict_index_t *index, const rec_t *rec,
                             ulint n) {
  if (index) {
    ut_ad(!dict_table_is_comp(index->table));
    if (index->has_row_versions()) {
      uint8_t version = UINT8_UNDEFINED;
      if (rec_old_is_versioned(rec)) {
        version = rec_get_instant_row_version_old(rec);
      }

      n = index->get_field_phy_pos(n, version);
    }
  }

  return rec_get_nth_field_size_low(rec, n);
}

ulint rec_get_nth_field_offs(const dict_index_t *index, const ulint *offsets,
                             ulint n, ulint *len) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  return rec_get_nth_field_offs_low(offsets, n, len);
}

ulint rec_get_nth_field_offs_old(const dict_index_t *index, const rec_t *rec,
                                 ulint n, ulint *len) {
  if (index) {
    ut_ad(!dict_table_is_comp(index->table));
    if (index->has_row_versions()) {
      uint8_t version = UINT8_UNDEFINED;
      if (rec_old_is_versioned(rec)) {
        version = rec_get_instant_row_version_old(rec);
      }

      n = index->get_field_phy_pos(n, version);
    }
  }

  return rec_get_nth_field_offs_old_low(rec, n, len);
}

ulint rec_offs_nth_extern(const dict_index_t *index, const ulint *offsets,
                          ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  return (rec_offs_nth_extern_low(offsets, n));
}

void rec_offs_make_nth_extern(dict_index_t *index, ulint *offsets, ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  rec_offs_make_nth_extern_low(offsets, n);
}

ulint rec_offs_nth_sql_null(const dict_index_t *index, const ulint *offsets,
                            ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  return (rec_offs_nth_sql_null_low(offsets, n));
}

ulint rec_offs_nth_default(const dict_index_t *index, const ulint *offsets,
                           ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  return (rec_offs_nth_default_low(offsets, n));
}

ulint rec_offs_nth_size(const dict_index_t *index, const ulint *offsets,
                        ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  return (rec_offs_nth_size_low(offsets, n));
}

void rec_set_nth_field(const dict_index_t *index, rec_t *rec,
                       const ulint *offsets, ulint n, const void *data,
                       ulint len) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  rec_set_nth_field_low(rec, offsets, n, data, len);
}

ulint rec_2_is_field_extern(const dict_index_t *index, const rec_t *rec,
                            ulint n) {
  if (index) {
    ut_ad(!dict_table_is_comp(index->table));
    if (index->has_row_versions()) {
      uint8_t version = UINT8_UNDEFINED;
      if (rec_old_is_versioned(rec)) {
        version = rec_get_instant_row_version_old(rec);
      }

      n = index->get_field_phy_pos(n, version);
    }
  }

  return (rec_2_get_field_end_info_low(rec, n) & REC_2BYTE_EXTERN_MASK);
}

ulint rec_get_data_size_old(const rec_t *rec) {
  ut_ad(rec);

  return (rec_get_field_start_offs_low(rec, rec_get_n_fields_old_raw(rec)));
}
