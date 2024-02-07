/*****************************************************************************
Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

/** @file include/rem0wrec.h
 Record manager wrapper implementation

 Created 13/08/2021 Mayank Prasad
******************************************************************************/

#include "rem0wrec.h"
#include "rem0lrec.h"

#ifndef UNIV_NO_ERR_MSGS
/** Dumps metadata of table.
@param[in]      table   InnoDB table object*/
static void dump_metadata_dict_table(const dict_table_t *table) {
  ib::info(ER_IB_DICT_LOG_TABLE_INFO) << "Table Id : " << table->id;
  ib::info(ER_IB_DICT_LOG_TABLE_INFO) << "Table Name : " << table->name.m_name;
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Has instant cols : " << table->has_instant_cols();
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Has instant row versions : " << table->has_row_versions();
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Current row version : " << table->current_row_version;
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Table initial column count : " << table->initial_col_count;
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Table current column count : " << table->current_col_count;
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Table total column count : " << table->total_col_count;
  ib::info(ER_IB_DICT_LOG_TABLE_INFO) << "Number of columns added instantly : "
                                      << table->get_n_instant_add_cols();
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Number of columns dropped instantly : "
      << table->get_n_instant_drop_cols();
  ib::info(ER_IB_DICT_LOG_TABLE_INFO)
      << "Table uses COMPACT page format : " << dict_table_is_comp(table);
}
#endif /* !UNIV_NO_ERR_MSGS */

/** Validates offset and field number.
@param[in]      index   record descriptor
@param[in]      offsets array returned by rec_get_offsets()
@param[in]      n       nth field
@param[in]      L       Line number of calling satement*/
static void validate_rec_offset(const dict_index_t *index, const ulint *offsets,
                                ulint n, ut::Location L) {
  ut_ad(rec_offs_validate(nullptr, nullptr, offsets));
  if (n >= rec_offs_n_fields(offsets)) {
#ifndef UNIV_NO_ERR_MSGS
    dump_metadata_dict_table(index->table);
    auto num_fields = static_cast<size_t>(rec_offs_n_fields(offsets));
    ib::fatal(L, ER_IB_DICT_INVALID_COLUMN_POSITION, ulonglong{n}, num_fields);
#endif /* !UNIV_NO_ERR_MSGS */
  }
}

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

  validate_rec_offset(index, offsets, n, UT_LOCATION_HERE);
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

  validate_rec_offset(index, offsets, n, UT_LOCATION_HERE);
  return (rec_offs_nth_sql_null_low(offsets, n));
}

ulint rec_offs_nth_default(const dict_index_t *index, const ulint *offsets,
                           ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  validate_rec_offset(index, offsets, n, UT_LOCATION_HERE);
  return (rec_offs_nth_default_low(offsets, n));
}

ulint rec_offs_nth_size(const dict_index_t *index, const ulint *offsets,
                        ulint n) {
  if (index && index->has_row_versions()) {
    n = index->get_field_off_pos(n);
  }

  validate_rec_offset(index, offsets, n, UT_LOCATION_HERE);
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
