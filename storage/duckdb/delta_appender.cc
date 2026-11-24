/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

#include "delta_appender.h"
#include <mysql/components/services/log_builtins.h>
#include "ddl_convertor.h"
#include "duckdb/common/error_data.hpp"
#include "duckdb/common/hugeint.hpp"
#include "duckdb/common/types/decimal.hpp"
#include "duckdb/main/physical_appender.hpp"
#include "sql-common/my_decimal.h"
#include "sql/duckdb/duckdb_charset_collation.h"
#include "sql/duckdb/duckdb_config.h"  // use_double_for_decimal
#include "sql/duckdb/duckdb_context.h"
#include "sql/duckdb/duckdb_manager.h"
#include "sql/duckdb/duckdb_sql_quote.h"
#include "sql/duckdb/duckdb_timezone.h"  // days_at_timestart
#include "sql/field.h"                   // field
#include "sql/sql_class.h"
#include "sql/tztime.h"  // my_tz_UTC

ulonglong batch_max_row_count = 0;

DeltaAppender *DeltaAppenders::get_appender(THD *thd, std::string &db,
                                            std::string &tb, bool insert_only,
                                            bool idempotent_flag,
                                            TABLE *table) {
  auto key = std::make_pair(db, tb);
  auto it = m_append_infos.find(key);

  if (it != m_append_infos.end()) {
    return it->second.get();
  } else {
    auto appender = std::make_unique<DeltaAppender>(thd, db, tb, !insert_only,
                                                    idempotent_flag);

    try {
      if (appender->Initialize(table)) {
        appender.reset();
        return nullptr;
      }
    } catch (std::exception &ex) {
      duckdb::ErrorData error(ex);
      LogErr(INFORMATION_LEVEL, ER_DUCKDB, error.RawMessage().c_str());
      my_error(ER_DUCKDB_APPENDER_ERROR, MYF(0), error.RawMessage().c_str());
      return nullptr;
    }

    m_append_infos[key] = std::move(appender);

    return m_append_infos[key].get();
  }
}

void DeltaAppenders::delete_appender(std::string &db, std::string &tb) {
  auto key = std::make_pair(db, tb);
  auto it = m_append_infos.find(key);

  if (it != m_append_infos.end()) {
    m_append_infos.erase(it);
  }
}

bool DeltaAppenders::flush_all(std::string &error_msg) {
  try {
    for (auto &pair : m_append_infos) {
      auto appender = pair.second.get();
      if (appender->flush()) return true;
    }
  } catch (std::exception &ex) {
    duckdb::ErrorData error(ex);
    error_msg = error.RawMessage();
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, error_msg.c_str());
    return true;
  }
  // remove all
  m_append_infos.clear();
  return false;
}

void DeltaAppenders::reset_all() { m_append_infos.clear(); }

bool DeltaAppenders::rollback_trx(ulonglong trx_no) {
  DBUG_EXECUTE_IF("simulate_duckdb_rollback_partial_trx_failed", return true;);

  try {
    for (auto &pair : m_append_infos) {
      auto appender = pair.second.get();
      if (appender->rollback(trx_no)) return true;
    }
  } catch (std::exception &ex) {
    duckdb::ErrorData error(ex);
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, error.RawMessage().c_str());
    return true;
  }

  return false;
}

int DeltaAppender::append_row_insert(TABLE *table, ulonglong trx_no,
                                     const MY_BITMAP *blob_type_map) {
  ++m_row_count;
  m_has_insert = true;

  try {
    m_appender->BeginRow();

    for (uint i = 0; i < table->s->fields; i++) {
      Field *field = table->field[i];
      if (!m_use_tmp_table && field->is_gcol()) {
        continue;
      }
      int ret = append_mysql_field(field, blob_type_map);
      if (ret) return HA_DUCKDB_APPEND_ERROR;
    }

    if (m_use_tmp_table) {
      m_appender->Append<int64_t>(0);            // append delete_flag with 0
      m_appender->Append<int64_t>(m_row_count);  // append row_no
      m_appender->Append<int64_t>(trx_no);       // append trx_no
    }

    m_appender->EndRow();
  } catch (std::exception &ex) {
    duckdb::ErrorData error(ex);
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, error.RawMessage().c_str());
    my_error(ER_DUCKDB_APPENDER_ERROR, MYF(0), error.RawMessage().c_str());
    return HA_DUCKDB_APPEND_ERROR;
  }

  if (m_use_tmp_table && batch_max_row_count &&
      m_row_count >= batch_max_row_count) {
    return flush_partial_batch();
  }

  return 0;
}

int DeltaAppender::append_row_update(TABLE *table, ulonglong trx_no,
                                     const uchar *old_row) {
  m_has_update = true;

  return (append_row_delete(table, trx_no, old_row) ||
          append_row_insert(table, trx_no, nullptr))
             ? HA_DUCKDB_APPEND_ERROR
             : 0;
}

int DeltaAppender::append_row_delete(TABLE *table, ulonglong trx_no,
                                     const uchar *old_row) {
  ++m_row_count;
  m_has_delete = true;

  try {
    m_appender->BeginRow();

    for (uint i = 0; i < table->s->fields; i++) {
      Field *field = table->field[i];
      if (!m_use_tmp_table && field->is_gcol()) {
        continue;
      }

      if (bitmap_is_set(&m_pk_bitmap, field->field_index())) {
        int ret = 0;

        if (!old_row) {
          ret = append_mysql_field(field);
        } else {
          uchar *saved_ptr = field->field_ptr();
          field->set_field_ptr(
              const_cast<uchar *>(old_row + field->offset(table->record[0])));
          ret = append_mysql_field(field);
          field->set_field_ptr(saved_ptr);
        }

        if (ret) return HA_DUCKDB_APPEND_ERROR;
      } else {
        m_appender->Append(duckdb::Value(duckdb::LogicalType::SQLNULL));
      }
    }

    if (m_use_tmp_table) {
      m_appender->Append<int64_t>(1);            // append delete_flag with 1
      m_appender->Append<int64_t>(m_row_count);  // append row_no
      m_appender->Append<int64_t>(trx_no);       // append trx_no
    }

    m_appender->EndRow();
  } catch (std::exception &ex) {
    duckdb::ErrorData error(ex);
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, error.RawMessage().c_str());
    my_error(ER_DUCKDB_APPENDER_ERROR, MYF(0), error.RawMessage().c_str());
    return HA_DUCKDB_APPEND_ERROR;
  }

  if (m_use_tmp_table && batch_max_row_count &&
      m_row_count >= batch_max_row_count) {
    return flush_partial_batch();
  }

  return 0;
}

bool DeltaAppender::Initialize(TABLE *table) {
  auto &con = m_thd->get_duckdb_context()->get_connection();
  if (m_use_tmp_table) {
    m_tmp_table_name = buf_table_name(m_schema_name, m_table_name);

    const std::string q_tmp = myduck::quote_duckdb_ident(m_tmp_table_name);

    std::stringstream ss;
    ss << "USE " << myduck::quote_duckdb_ident(m_schema_name) << ";";
    ss << "CREATE TEMPORARY TABLE IF NOT EXISTS main." << q_tmp << " AS FROM "
       << myduck::quote_duckdb_ident(m_table_name) << " LIMIT 0;";

    ss << "ALTER TABLE main." << q_tmp
       << " ADD COLUMN `#alibaba_rds_delete_flag` BOOL;";
    ss << "ALTER TABLE main." << q_tmp
       << " ADD COLUMN `#alibaba_rds_row_no` INT;";
    ss << "ALTER TABLE main." << q_tmp
       << " ADD COLUMN `#alibaba_rds_trx_no` INT;";

    auto ret = myduck::duckdb_query(m_thd, ss.str());
    if (ret->HasError()) {
      return true;
    }
    std::string schema_name("main");
    m_appender = std::make_unique<duckdb::PhysicalAppender>(
        con, schema_name, m_tmp_table_name, duckdb::AppenderType::PHYSICAL,
        myduck::appender_allocator_flush_threshold != 0
            ? myduck::appender_allocator_flush_threshold
            : duckdb::DConstants::INVALID_INDEX);

    // get pklist and pk_bitmap
    KEY *key_info = table->key_info;
    if (!key_info) return true;
    bitmap_init(&m_pk_bitmap, nullptr, table->s->fields);
    KEY_PART_INFO *key_part = key_info->key_part;
    for (uint i = 0; i < key_info->user_defined_key_parts; i++, key_part++) {
      if (i) m_pk_list += ", ";
      m_pk_list += myduck::quote_duckdb_ident(key_part->field->field_name);
      bitmap_set_bit(&m_pk_bitmap, key_part->field->field_index());
    }

    // get column list
    bool is_first_field = true;
    for (uint i = 0; i < table->s->fields; i++) {
      if (table->field[i]->is_gcol()) {
        m_has_gcols = true;
        continue;
      }
      if (!is_first_field) m_col_list += ", ";
      m_col_list += myduck::quote_duckdb_ident(table->field[i]->field_name);
      is_first_field = false;
    }
  } else {
    m_appender = std::make_unique<duckdb::PhysicalAppender>(
        con, m_schema_name, m_table_name, duckdb::AppenderType::PHYSICAL,
        myduck::appender_allocator_flush_threshold != 0
            ? myduck::appender_allocator_flush_threshold
            : duckdb::DConstants::INVALID_INDEX);
  }

  return false;
}

static void appendSelectQuery(std::stringstream &ss,
                              const std::string &select_list,
                              const std::string &pk_list,
                              const std::string &table_name, int delete_flag) {
  // STRUCT_PACK is required (not ROW): in DuckDB v1.5.2 UNNEST on a struct
  // resolves children by name, and ROW(...) produces an unnamed struct which
  // triggers "struct_extract with a string key cannot be used on an unnamed
  // struct". STRUCT_PACK uses the column name as the field name, so the
  // resulting struct's children are named after the source columns.
  ss << "SELECT UNNEST(r) FROM (SELECT LAST(STRUCT_PACK(" << select_list
     << ") ORDER BY `#alibaba_rds_row_no`) AS r, "
        "LAST(`#alibaba_rds_delete_flag` ORDER BY `#alibaba_rds_row_no`) AS "
        "`#alibaba_rds_delete_flag` FROM main."
     << myduck::quote_duckdb_ident(table_name) << " GROUP BY " << pk_list
     << ")";
  if (!delete_flag) ss << " WHERE `#alibaba_rds_delete_flag` = " << delete_flag;
}

void DeltaAppender::generateQuery(std::stringstream &ss, bool delete_flag) {
  ss.str("");
  ss << "USE " << myduck::quote_duckdb_ident(m_schema_name) << ";";
  if (!delete_flag) {
    ss << "INSERT INTO " << myduck::quote_duckdb_ident(m_table_name) << " ";
    if (m_has_gcols) {
      ss << "(" << m_col_list << ") ";
    }
    appendSelectQuery(ss, m_col_list, m_pk_list, m_tmp_table_name, delete_flag);
    ss << ";";
  } else {
    ss << "DELETE FROM " << myduck::quote_duckdb_ident(m_table_name)
       << " WHERE (" << m_pk_list << ") IN (";
    appendSelectQuery(ss, m_pk_list, m_pk_list, m_tmp_table_name, delete_flag);
    ss << ");";
  }
}

bool DeltaAppender::flush() {
  m_appender->Flush();

  if (m_use_tmp_table) {
    std::stringstream ss;

    /* Delete */
    if (m_has_delete || m_idempotent_flag) {
      generateQuery(ss, true);
      auto ret = myduck::duckdb_query(m_thd, ss.str());
      if (ret->HasError()) {
        return true;
      }
    }

    /* Insert */
    if (m_has_insert) {
      generateQuery(ss, false);
      auto ret = myduck::duckdb_query(m_thd, ss.str());
      if (ret->HasError()) {
        return true;
      }
    }
  }

  return false;
}

int DeltaAppender::flush_partial_batch() {
  if (flush()) {
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, "flush_partial_batch flush failed");
    my_error(ER_DUCKDB_APPENDER_ERROR, MYF(0),
             "flush_partial_batch flush failed");
    return HA_DUCKDB_APPEND_ERROR;
  }

  std::stringstream ss;
  ss << "TRUNCATE main." << myduck::quote_duckdb_ident(m_tmp_table_name) << ";";
  if (myduck::duckdb_query(m_thd, ss.str())->HasError()) {
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, "flush_partial_batch truncate failed");
    my_error(ER_DUCKDB_APPENDER_ERROR, MYF(0),
             "flush_partial_batch truncate failed");
    return HA_DUCKDB_APPEND_ERROR;
  }

  m_row_count = 0;
  m_has_insert = false;
  m_has_delete = false;
  m_has_update = false;

  return 0;
}

bool DeltaAppender::rollback(ulonglong trx_no) {
  if (m_use_tmp_table) {
    /* Flush to tmp table, then we can rollback the TRX's modified rows */
    m_appender->Flush();
    std::stringstream ss;
    ss << "DELETE FROM main." << myduck::quote_duckdb_ident(m_tmp_table_name)
       << " WHERE `#alibaba_rds_trx_no` = " << trx_no;

    auto ret = myduck::duckdb_query(m_thd, ss.str());
    if (ret->HasError()) {
      return true;
    }
  }

  return false;
}

void DeltaAppender::cleanup() {
  if (m_use_tmp_table) {
    bitmap_free(&m_pk_bitmap);
    std::stringstream ss;
    ss << "DROP TABLE IF EXISTS main."
       << myduck::quote_duckdb_ident(m_tmp_table_name) << ";";
    myduck::duckdb_query(m_thd, ss.str());
  }
}

#define DIG_PER_DEC1 9
#define DIG_BASE 1000000000
static const decimal_digit_t powers10[DIG_PER_DEC1 + 1] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

template <typename T>
static T get_duckdb_decimal(const my_decimal &from, int fixed_decimal) {
  T res{0};
  decimal_digit_t *buf = from.buf;
  int intg = from.intg, frac = from.frac, fill = fixed_decimal - frac;
  bool sign = from.sign();

  auto update_res = [&](longlong digit) {
    if (sign) {
      res -= digit;
    } else {
      res += digit;
    }
  };

  for (; intg > 0; intg -= DIG_PER_DEC1) {
    res *= DIG_BASE;
    update_res(*buf++);
  }

  for (; frac >= DIG_PER_DEC1; frac -= DIG_PER_DEC1) {
    res *= DIG_BASE;
    update_res(*buf++);
  }

  if (frac > 0) {
    res *= powers10[frac];
    update_res(decimal_div_by_pow10(*buf++, DIG_PER_DEC1 - frac));
  }

  if (fill > 0) {
    res *= powers10[fill];
  }

  return res;
}
int DeltaAppender::append_mysql_field(const Field *field,
                                      const MY_BITMAP *blob_type_map) {
  auto appender = m_appender.get();

  if (field->is_real_null()) {  // NULL
    appender->Append(duckdb::Value(duckdb::LogicalType::SQLNULL));
    return 0;
  }

  enum_field_types type = field->real_type();  // type from mysql
  // duckdb::LogicalType duck_type = duck_types[col_index];

  switch (type) {
    case MYSQL_TYPE_TINY: {
      longlong value = static_cast<const Field_tiny *>(field)->val_int();
      appender->Append<int64_t>(value);
      break;
    }

    case MYSQL_TYPE_SHORT: {
      longlong value = static_cast<const Field_short *>(field)->val_int();
      appender->Append<int64_t>(value);
      break;
    }

    case MYSQL_TYPE_INT24: {
      longlong value = static_cast<const Field_medium *>(field)->val_int();
      appender->Append<int64_t>(value);
      break;
    }

    case MYSQL_TYPE_LONG: {
      longlong value = static_cast<const Field_long *>(field)->val_int();
      appender->Append<int64_t>(value);
      break;
    }

    case MYSQL_TYPE_LONGLONG: {
      longlong value = static_cast<const Field_longlong *>(field)->val_int();
      if (field->is_unsigned()) {
        appender->Append<uint64_t>(value);
      } else {
        appender->Append<int64_t>(value);
      }
      break;
    }

    case MYSQL_TYPE_FLOAT: {
      double value = static_cast<const Field_float *>(field)->val_real();
      // write_batch_double(col_index, &value);
      appender->Append<double>(value);
      break;
    }
    case MYSQL_TYPE_DOUBLE: {
      double value = static_cast<const Field_double *>(field)->val_real();
      // write_batch_double(col_index, &value);
      appender->Append<double>(value);
      break;
    }

    case MYSQL_TYPE_NEWDECIMAL: {
      my_decimal value;
      const Field_new_decimal *decimal_field =
          static_cast<const Field_new_decimal *>(field);
      uint precision = decimal_field->precision;
      uint8 dec = decimal_field->dec;
      if (precision <= 38) {
        decimal_field->val_decimal(&value);
        if (value.intg + value.frac > (int)precision || value.frac > (int)dec) {
          LogErr(INFORMATION_LEVEL, ER_DUCKDB, "Append DECIMAL field failed!");
          my_error(ER_DUCKDB_APPENDER_ERROR, MYF(0),
                   "Append DECIMAL field failed!");
          return HA_DUCKDB_APPEND_ERROR;
        }

        if (precision <= duckdb::Decimal::MAX_WIDTH_INT16) {
          appender->Append<int16_t>(get_duckdb_decimal<int16_t>(value, dec));
        } else if (precision <= duckdb::Decimal::MAX_WIDTH_INT32) {
          appender->Append<int32_t>(get_duckdb_decimal<int32_t>(value, dec));
        } else if (precision <= duckdb::Decimal::MAX_WIDTH_INT64) {
          appender->Append<int64_t>(get_duckdb_decimal<int64_t>(value, dec));
        } else {
          appender->Append<duckdb::hugeint_t>(
              get_duckdb_decimal<duckdb::hugeint_t>(value, dec));
        }
      } else if (myduck::use_double_for_decimal) {
        double value = decimal_field->val_real();
        appender->Append<double>(value);
      } else {
        // Append decimal as decimal(38, dec)
        decimal_field->val_decimal(&value);
        auto real_intg = decimal_actual_intg((decimal_t *)&value);
        assert(real_intg <= value.intg);
        if (real_intg + dec > 38) {
          throw duckdb::OutOfRangeException("decimal value is out of range");
        } else {
          appender->Append<duckdb::hugeint_t>(
              get_duckdb_decimal<duckdb::hugeint_t>(value, dec));
        }
      }
      break;
    }

    case MYSQL_TYPE_NEWDATE: {
      /* The date '2020-01-01' will be converted into int '20200101',
      which is same storage method as InnoDB. */
      Datetime_val tm;
      field->val_datetime(&tm, 0);
      long date =
          calc_daynr(tm.year, tm.month, tm.day) - myduck::days_at_timestart;

      appender->Append<duckdb::date_t>(static_cast<duckdb::date_t>(date));
      break;
    }

    case MYSQL_TYPE_DATETIME2: {
      Datetime_val tm;
      field->val_datetime(&tm, 0);
      longlong ts_us;
      if (tm.month == 0) {
        /*
          Zero date (e.g. 0000-00-00 00:00:00) or invalid date with month=0.
          Bypass TIME_to_gmt_sec() which asserts month > 0.
          Use calc_daynr() to compute timestamp directly, consistent with
          MYSQL_TYPE_NEWDATE handling.
        */
        long days =
            calc_daynr(tm.year, tm.month, tm.day) - myduck::days_at_timestart;
        ts_us = static_cast<longlong>(days) * 86400LL * 1000000LL +
                tm.hour * 3600LL * 1000000LL + tm.minute * 60LL * 1000000LL +
                tm.second * 1000000LL + tm.second_part;
      } else {
        bool not_used;
        longlong sec = my_tz_UTC->TIME_to_gmt_sec(&tm, &not_used);
        ts_us = sec * 1000000LL + tm.second_part;
      }
      appender->Append<duckdb::timestamp_t>(
          static_cast<duckdb::timestamp_t>(ts_us));
      break;
    }

    case MYSQL_TYPE_YEAR: {
      longlong value = static_cast<const Field_year *>(field)->val_int();
      appender->Append<int64_t>(value);
      break;
    }

    case MYSQL_TYPE_TIME2: {
      Time_val tm;
      field->val_time(&tm);
      int64_t micros =
          (tm.hour() * 3600LL + tm.minute() * 60LL + tm.second()) * 1000000LL +
          tm.microsecond();
      if (tm.is_negative()) micros = -micros;
      appender->Append<duckdb::dtime_t>(duckdb::dtime_t(micros));
      break;
    }

    case MYSQL_TYPE_TIMESTAMP2: {
      my_timeval tm;
      static_cast<const Field_timestamp *>(field)->get_timestamp(&tm, nullptr);
      appender->Append<duckdb::timestamp_t>(
          duckdb::timestamp_t{tm.m_tv_sec * 1000000 + tm.m_tv_usec});
      break;
    }
    case MYSQL_TYPE_JSON: {
      char buf[128];
      String tmp(buf, sizeof(buf), &my_charset_bin);
      field->val_str(&tmp, &tmp);
      appender->Append<duckdb::string_t>(
          duckdb::string_t(tmp.ptr(), tmp.length()));
      break;
    }
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB: {
      char buf[128];
      String tmp(buf, sizeof(buf), &my_charset_bin);
      field->val_str(&tmp, &tmp);

      bool is_blob = false;
      if (blob_type_map != nullptr) {
        is_blob = bitmap_is_set(blob_type_map, field->field_index());
      } else {
        is_blob = (FieldConvertor::convert_type(field) == "BLOB" ||
                   FieldConvertor::convert_type(field) == "BITSTRING");
      }

      if (is_blob) {
        assert(!field->has_charset());
        auto value = duckdb::Value::BLOB((duckdb::const_data_ptr_t)tmp.ptr(),
                                         tmp.length());
        appender->Append(value);
      } else {
        assert(field->has_charset());
        /* DuckDB store latin1 column's data as utf8mb4. */
        if (strcmp(field->charset()->csname, "latin1") == 0) {
          const CHARSET_INFO *utf8mb4_cs =
              myduck::get_utf8mb4_charset_for_latin1(field->charset());
          char utf8mb4_buf[512];
          String utf8mb4_str(utf8mb4_buf, sizeof(utf8mb4_buf), utf8mb4_cs);
          uint conversion_errors = 0;

          if (utf8mb4_str.copy(tmp.ptr(), tmp.length(), field->charset(),
                               &my_charset_utf8mb4_0900_bin,
                               &conversion_errors)) {
            /* The conversion has no reason to fail */
            assert(false);
            appender->Append<duckdb::string_t>(
                duckdb::string_t(tmp.ptr(), tmp.length()));
          } else {
            appender->Append<duckdb::string_t>(
                duckdb::string_t(utf8mb4_str.ptr(), utf8mb4_str.length()));
          }
        } else {
          appender->Append<duckdb::string_t>(
              duckdb::string_t(tmp.ptr(), tmp.length()));
        }
      }
      break;
    }
    default:
      return HA_DUCKDB_UNSUPPORTED_DATA_TYPE;
  }
  return 0;
}
