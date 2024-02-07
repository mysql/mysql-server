/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/table_helper.cc
  Performance schema table helpers (implementation).
*/

#include "storage/perfschema/table_helper.h"

#include <assert.h>
#include <algorithm>

#include "my_config.h"

#include "my_compiler.h"

#include "m_string.h"
#include "my_macros.h"
#include "my_thread.h"
#include "sql-common/json_dom.h"
#include "sql/field.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_error.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_prepared_stmt.h"
#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_setup_object.h"
#include "storage/perfschema/pfs_user.h"
#include "storage/perfschema/pfs_variable.h"

struct CHARSET_INFO;

/* TINYINT TYPE */
void set_field_tiny(Field *f, long value) {
  assert(f->real_type() == MYSQL_TYPE_TINY);
  auto *f2 = (Field_tiny *)f;
  f2->store(value, false);
}

void set_field_utiny(Field *f, ulong value) {
  assert(f->real_type() == MYSQL_TYPE_TINY);
  auto *f2 = (Field_tiny *)f;
  f2->store(value, true);
}

long get_field_tiny(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_TINY);
  auto *f2 = (Field_tiny *)f;
  return f2->val_int();
}

ulong get_field_utiny(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_TINY);
  auto *f2 = (Field_tiny *)f;
  return f2->val_int();
}

/* SMALLINT TYPE */
void set_field_short(Field *f, long value) {
  assert(f->real_type() == MYSQL_TYPE_SHORT);
  auto *f2 = (Field_short *)f;
  f2->store(value, false);
}

void set_field_ushort(Field *f, ulong value) {
  assert(f->real_type() == MYSQL_TYPE_SHORT);
  auto *f2 = (Field_short *)f;
  f2->store(value, true);
}

long get_field_short(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_SHORT);
  auto *f2 = (Field_short *)f;
  return f2->val_int();
}

ulong get_field_ushort(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_SHORT);
  auto *f2 = (Field_short *)f;
  return f2->val_int();
}

/* MEDIUMINT TYPE */
void set_field_medium(Field *f, long value) {
  assert(f->real_type() == MYSQL_TYPE_INT24);
  auto *f2 = (Field_medium *)f;
  f2->store(value, false);
}

void set_field_umedium(Field *f, ulong value) {
  assert(f->real_type() == MYSQL_TYPE_INT24);
  auto *f2 = (Field_medium *)f;
  f2->store(value, true);
}

long get_field_medium(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_INT24);
  auto *f2 = (Field_medium *)f;
  return f2->val_int();
}

ulong get_field_umedium(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_INT24);
  auto *f2 = (Field_medium *)f;
  return f2->val_int();
}

/* INTEGER (INT) TYPE */
void set_field_long(Field *f, long value) {
  assert(f->real_type() == MYSQL_TYPE_LONG);
  auto *f2 = (Field_long *)f;
  f2->store(value, false);
}

void set_field_ulong(Field *f, ulong value) {
  assert(f->real_type() == MYSQL_TYPE_LONG);
  auto *f2 = (Field_long *)f;
  f2->store(value, true);
}

long get_field_long(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_LONG);
  auto *f2 = (Field_long *)f;
  return f2->val_int();
}

ulong get_field_ulong(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_LONG);
  auto *f2 = (Field_long *)f;
  return f2->val_int();
}

/* BIGINT TYPE */
void set_field_longlong(Field *f, longlong value) {
  assert(f->real_type() == MYSQL_TYPE_LONGLONG);
  auto *f2 = (Field_longlong *)f;
  f2->store(value, false);
}

void set_field_ulonglong(Field *f, ulonglong value) {
  assert(f->real_type() == MYSQL_TYPE_LONGLONG);
  auto *f2 = (Field_longlong *)f;
  f2->store(value, true);
}

longlong get_field_longlong(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_LONGLONG);
  auto *f2 = (Field_longlong *)f;
  return f2->val_int();
}

ulonglong get_field_ulonglong(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_LONGLONG);
  auto *f2 = (Field_longlong *)f;
  return f2->val_int();
}

/* DECIMAL TYPE */
void set_field_decimal(Field *f, double value) {
  assert(f->real_type() == MYSQL_TYPE_NEWDECIMAL);
  auto *f2 = (Field_new_decimal *)f;
  f2->store(value);
}

double get_field_decimal(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_NEWDECIMAL);
  auto *f2 = (Field_new_decimal *)f;
  return f2->val_real();
}

/* FLOAT TYPE */
void set_field_float(Field *f, double value) {
  assert(f->real_type() == MYSQL_TYPE_FLOAT);
  auto *f2 = (Field_float *)f;
  f2->store(value);
}

double get_field_float(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_FLOAT);
  auto *f2 = (Field_float *)f;
  return f2->val_real();
}

/* DOUBLE TYPE */
void set_field_double(Field *f, double value) {
  assert(f->real_type() == MYSQL_TYPE_DOUBLE);
  auto *f2 = (Field_double *)f;
  f2->store(value);
}

double get_field_double(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_DOUBLE);
  auto *f2 = (Field_double *)f;
  return f2->val_real();
}

/* CHAR TYPE */
void set_field_char_utf8mb4(Field *f, const char *str, uint len) {
  assert(f->real_type() == MYSQL_TYPE_STRING);
  auto *f2 = (Field_string *)f;
  f2->store(str, len, &my_charset_utf8mb4_bin);
}

String *get_field_char_utf8mb4(Field *f, String *val) {
  assert(f->real_type() == MYSQL_TYPE_STRING);
  auto *f2 = (Field_string *)f;
  val = f2->val_str(nullptr, val);
  return val;
}

char *get_field_char_utf8mb4(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_STRING);
  String temp;
  auto *f2 = (Field_string *)f;
  f2->val_str(nullptr, &temp);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

/* VARCHAR TYPE */
void set_field_varchar(Field *f, const CHARSET_INFO *cs, const char *str,
                       uint len) {
  assert(f->real_type() == MYSQL_TYPE_VARCHAR);
  auto *f2 = (Field_varstring *)f;
  f2->store(str, len, cs);
}

String *get_field_varchar_utf8mb4(Field *f, String *val) {
  assert(f->real_type() == MYSQL_TYPE_VARCHAR);
  auto *f2 = (Field_varstring *)f;
  val = f2->val_str(nullptr, val);
  return val;
}
char *get_field_varchar_utf8mb4(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_VARCHAR);
  String temp;
  auto *f2 = (Field_varstring *)f;
  f2->val_str(nullptr, &temp);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

void set_field_varchar_utf8mb4(Field *f, const char *str) {
  assert(f->real_type() == MYSQL_TYPE_VARCHAR);
  auto *f2 = (Field_varstring *)f;
  f2->store(str, strlen(str), &my_charset_utf8mb4_bin);
}

void set_field_varchar_utf8mb4(Field *f, const char *str, uint len) {
  assert(f->real_type() == MYSQL_TYPE_VARCHAR);
  auto *f2 = (Field_varstring *)f;
  f2->store(str, len, &my_charset_utf8mb4_bin);
}

/* BLOB TYPE */
void set_field_blob(Field *f, const char *val, size_t len) {
  assert(f->real_type() == MYSQL_TYPE_BLOB);
  auto *f2 = (Field_blob *)f;
  f2->store(val, len, &my_charset_utf8mb4_bin);
}

/* TEXT TYPE */
void set_field_text(Field *f, const char *val, size_t len,
                    const CHARSET_INFO *cs) {
  assert(f->real_type() == MYSQL_TYPE_BLOB);
  auto *f2 = (Field_blob *)f;
  f2->store(val, len, cs);
}

char *get_field_blob(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_BLOB);
  String temp;
  auto *f2 = (Field_blob *)f;
  f2->val_str(nullptr, &temp);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

/* ENUM TYPE */
void set_field_enum(Field *f, ulonglong value) {
  assert(f->real_type() == MYSQL_TYPE_ENUM);
  auto *f2 = (Field_enum *)f;
  f2->store_type(value);
}

ulonglong get_field_enum(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_ENUM);
  auto *f2 = (Field_enum *)f;
  return f2->val_int();
}

/* SET TYPE */
void set_field_set(Field *f, ulonglong value) {
  assert(f->real_type() == MYSQL_TYPE_SET);
  auto *f2 = (Field_set *)f;
  f2->store_type(value);
}

ulonglong get_field_set(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_SET);
  auto *f2 = (Field_set *)f;
  return f2->val_int();
}

/* DATE TYPE */
void set_field_date(Field *f, const char *value, uint len) {
  assert(f->real_type() == MYSQL_TYPE_NEWDATE);
  auto *f2 = (Field_newdate *)f;
  f2->store(value, len, system_charset_info);
}

char *get_field_date(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_NEWDATE);
  String temp;
  auto *f2 = (Field_newdate *)f;
  f2->val_str(&temp, nullptr);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

/* TIME TYPE */
void set_field_time(Field *f, const char *value, uint len) {
  assert(f->real_type() == MYSQL_TYPE_TIME2);
  auto *f2 = (Field_timef *)f;
  f2->store(value, len, system_charset_info);
}

char *get_field_time(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_TIME2);
  String temp;
  auto *f2 = (Field_timef *)f;
  f2->val_str(&temp, nullptr);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

/* DATETIME TYPE */
void set_field_datetime(Field *f, const char *value, uint len) {
  assert(f->real_type() == MYSQL_TYPE_DATETIME2);
  auto *f2 = (Field_datetimef *)f;
  f2->store(value, len, system_charset_info);
}

char *get_field_datetime(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_DATETIME2);
  String temp;
  auto *f2 = (Field_datetimef *)f;
  f2->val_str(&temp, nullptr);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

/* TIMESTAMP TYPE */
void set_field_timestamp(Field *f, const char *value, uint len) {
  assert(f->real_type() == MYSQL_TYPE_TIMESTAMP2);
  auto *f2 = (Field_timestampf *)f;
  f2->store(value, len, system_charset_info);
}

char *get_field_timestamp(Field *f, char *val, uint *len) {
  assert(f->real_type() == MYSQL_TYPE_TIMESTAMP2);
  String temp;
  auto *f2 = (Field_timestampf *)f;
  f2->val_str(&temp, nullptr);
  *len = temp.length();
  strncpy(val, temp.ptr(), *len);
  return val;
}

void set_field_timestamp(Field *f, ulonglong value) {
  const my_timeval tm = {static_cast<int64_t>(value / 1000000),
                         static_cast<int64_t>(value % 1000000)};
  assert(f->real_type() == MYSQL_TYPE_TIMESTAMP2);
  auto *f2 = (Field_timestampf *)f;
  f2->store_timestamp(&tm);
}

/* YEAR TYPE */
void set_field_year(Field *f, ulong value) {
  assert(f->real_type() == MYSQL_TYPE_YEAR);
  auto *f2 = (Field_year *)f;
  f2->store(value, true);
}

ulong get_field_year(Field *f) {
  assert(f->real_type() == MYSQL_TYPE_YEAR);
  auto *f2 = (Field_year *)f;
  return f2->val_int();
}

/* JSON TYPE */
void set_field_json(Field *f, const Json_wrapper *json) {
  assert(f->real_type() == MYSQL_TYPE_JSON);
  auto *f2 = (Field_json *)f;
  f2->store_json(json);
}

void set_nullable_field_schema_name(Field *f, const PFS_schema_name *schema) {
  assert(f->is_nullable());
  const size_t len = schema->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_varchar_utf8mb4(f, schema->ptr(), len);
  }
}

void set_field_schema_name(Field *f, const PFS_schema_name *schema) {
  assert(!f->is_nullable());
  set_field_varchar_utf8mb4(f, schema->ptr(), schema->length());
}

void set_nullable_field_schema_name(Field *f,
                                    const PFS_schema_name_view *schema) {
  assert(f->is_nullable());
  const size_t len = schema->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_varchar_utf8mb4(f, schema->ptr(), len);
  }
}

void set_field_schema_name(Field *f, const PFS_schema_name_view *schema) {
  assert(!f->is_nullable());
  set_field_varchar_utf8mb4(f, schema->ptr(), schema->length());
}

void set_nullable_field_object_name(Field *f, const PFS_object_name *object) {
  assert(f->is_nullable());
  const size_t len = object->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_varchar_utf8mb4(f, object->ptr(), len);
  }
}

void set_field_object_name(Field *f, const PFS_object_name *object) {
  assert(!f->is_nullable());
  set_field_varchar_utf8mb4(f, object->ptr(), object->length());
}

void set_nullable_field_object_name(Field *f,
                                    const PFS_object_name_view *object) {
  assert(f->is_nullable());
  const size_t len = object->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_varchar_utf8mb4(f, object->ptr(), len);
  }
}

void set_field_object_name(Field *f, const PFS_object_name_view *object) {
  assert(!f->is_nullable());
  set_field_varchar_utf8mb4(f, object->ptr(), object->length());
}

void set_nullable_field_routine_name(Field *f, const PFS_routine_name *object) {
  assert(f->is_nullable());
  const size_t len = object->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_varchar_utf8mb4(f, object->ptr(), len);
  }
}

void set_field_routine_name(Field *f, const PFS_routine_name *object) {
  assert(!f->is_nullable());
  set_field_varchar_utf8mb4(f, object->ptr(), object->length());
}

void set_field_host_name(Field *f, const PFS_host_name *host) {
  assert(!f->is_nullable());
  set_field_char_utf8mb4(f, host->ptr(), host->length());
}

void set_nullable_field_host_name(Field *f, const PFS_host_name *host) {
  assert(f->is_nullable());
  const size_t len = host->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_char_utf8mb4(f, host->ptr(), len);
  }
}

void set_field_user_name(Field *f, const PFS_user_name *user) {
  assert(!f->is_nullable());
  set_field_char_utf8mb4(f, user->ptr(), user->length());
}

void set_nullable_field_user_name(Field *f, const PFS_user_name *user) {
  assert(f->is_nullable());
  const size_t len = user->length();
  if (len == 0) {
    f->set_null();
  } else {
    set_field_char_utf8mb4(f, user->ptr(), len);
  }
}

void format_sqltext(const char *source_sqltext, size_t source_length,
                    const CHARSET_INFO *source_cs, bool truncated,
                    String &sqltext) {
  assert(source_cs != nullptr);

  sqltext.set_charset(source_cs);
  sqltext.length(0);

  if (source_length == 0) {
    return;
  }

  /* Adjust sqltext length to a valid number of bytes. */
  int cs_error = 0;
  const size_t sqltext_length = source_cs->cset->well_formed_len(
      source_cs, source_sqltext, source_sqltext + source_length, source_length,
      &cs_error);
  if (sqltext_length > 0) {
    /* Copy the source text into the target, convert charset if necessary. */
    sqltext.append(source_sqltext, sqltext_length, source_cs);

    /* Append "..." if the string is truncated or not well-formed. */
    if (truncated) {
      size_t chars = sqltext.numchars();
      if (chars > 3) {
        chars -= 3;
        const size_t bytes_offset = sqltext.charpos(chars, 0);
        sqltext.length(bytes_offset);
        sqltext.append("...", 3);
      }
    }
  }
}

/**
  Create a SOURCE column from source file and line.
*/
void make_source_column(const char *source_file, size_t source_line,
                        char row_buffer[], size_t row_buffer_size,
                        uint &row_length) {
  row_length = 0;

  /* Check that a source file reset is not in progress. */
  if (source_file == nullptr || pfs_unload_plugin_ref_count.load() > 0) {
    return;
  }

  /* Make a working copy. */
  char safe_source_file[COL_INFO_SIZE + 1]; /* 1024 + 1*/
  strncpy(safe_source_file, source_file, COL_INFO_SIZE);
  safe_source_file[sizeof(safe_source_file) - 1] = 0;

  try {
    /* Isolate the base file name and append the line number. */
    const char *base = base_name(safe_source_file);
    row_length =
        snprintf(row_buffer, row_buffer_size, "%s:%d", base, (int)source_line);
  } catch (...) {
  }
}

int PFS_host_row::make_row(PFS_host *pfs) {
  m_host_name = pfs->m_key.m_host_name;
  return 0;
}

void PFS_host_row::set_field(Field *f) { set_field_host_name(f, &m_host_name); }

void PFS_host_row::set_nullable_field(Field *f) {
  set_nullable_field_host_name(f, &m_host_name);
}

int PFS_user_row::make_row(PFS_user *pfs) {
  m_user_name = pfs->m_key.m_user_name;
  return 0;
}

void PFS_user_row::set_field(Field *f) { set_field_user_name(f, &m_user_name); }

void PFS_user_row::set_nullable_field(Field *f) {
  set_nullable_field_user_name(f, &m_user_name);
}

int PFS_account_row::make_row(PFS_account *pfs) {
  m_user_name = pfs->m_key.m_user_name;
  m_host_name = pfs->m_key.m_host_name;
  return 0;
}

void PFS_account_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* USER */
      set_field_user_name(f, &m_user_name);
      break;
    case 1: /* HOST */
      set_field_host_name(f, &m_host_name);
      break;
    default:
      assert(false);
      break;
  }
}

void PFS_account_row::set_nullable_field(uint index, Field *f) {
  switch (index) {
    case 0: /* USER */
      set_nullable_field_user_name(f, &m_user_name);
      break;
    case 1: /* HOST */
      set_nullable_field_host_name(f, &m_host_name);
      break;
    default:
      assert(false);
      break;
  }
}

int PFS_digest_row::make_row(PFS_statements_digest_stat *pfs) {
  m_schema_name = pfs->m_digest_key.m_schema_name;

  size_t safe_byte_count = pfs->m_digest_storage.m_byte_count;
  if (safe_byte_count > pfs_max_digest_length) {
    safe_byte_count = 0;
  }

  /*
    "0" value for byte_count indicates special entry i.e. aggregated
    stats at index 0 of statements_digest_stat_array. So do not calculate
    digest/digest_text as it should always be "NULL".
  */
  if (safe_byte_count > 0) {
    /*
      Calculate digest from HASH collected to be shown as
      DIGEST in this row.
    */
    DIGEST_HASH_TO_STRING(pfs->m_digest_storage.m_hash, m_digest);
    m_digest_length = DIGEST_HASH_TO_STRING_LENGTH;

    /*
      Calculate digest_text information from the token array collected
      to be shown as DIGEST_TEXT column.
    */
    compute_digest_text(&pfs->m_digest_storage, &m_digest_text);

    if (m_digest_text.length() == 0) {
      m_digest_length = 0;
    }
  } else {
    m_digest_length = 0;
  }

  return 0;
}

void PFS_digest_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* SCHEMA_NAME */
      if (m_schema_name.length() > 0) {
        set_field_varchar_utf8mb4(f, m_schema_name.ptr(),
                                  m_schema_name.length());
      } else {
        f->set_null();
      }
      break;
    case 1: /* DIGEST */
      if (m_digest_length > 0) {
        set_field_varchar_utf8mb4(f, m_digest, m_digest_length);
      } else {
        f->set_null();
      }
      break;
    case 2: /* DIGEST_TEXT */
      if (m_digest_text.length() > 0) {
        set_field_blob(f, m_digest_text.ptr(), (uint)m_digest_text.length());
      } else {
        f->set_null();
      }
      break;
    default:
      assert(false);
      break;
  }
}

int PFS_object_row::make_row(PFS_table_share *pfs) {
  m_object_type = pfs->get_object_type();
  m_schema_name = pfs->m_key.m_schema_name;
  m_object_name = pfs->m_key.m_table_name;

  return 0;
}

int PFS_object_row::make_row(PFS_program *pfs) {
  m_object_type = pfs->m_key.m_type;
  m_schema_name = pfs->m_key.m_schema_name;
  m_object_name = pfs->m_key.m_object_name;

  return 0;
}

int PFS_column_row::make_row(const MDL_key *mdl) {
  static_assert(MDL_key::NAMESPACE_END == 18,
                "Adjust performance schema when changing enum_mdl_namespace");

  bool with_schema = false;
  bool with_object = false;
  bool with_column = false;

  m_schema_name_length = 0;
  m_object_name_length = 0;
  m_column_name_length = 0;

  switch (mdl->mdl_namespace()) {
    case MDL_key::GLOBAL:
      m_object_type = OBJECT_TYPE_GLOBAL;
      break;
    case MDL_key::TABLESPACE:
      m_object_type = OBJECT_TYPE_TABLESPACE;
      with_object = true;
      break;
    case MDL_key::SCHEMA:
      m_object_type = OBJECT_TYPE_SCHEMA;
      with_schema = true;
      break;
    case MDL_key::TABLE:
      m_object_type = OBJECT_TYPE_TABLE;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::FUNCTION:
      m_object_type = OBJECT_TYPE_FUNCTION;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::PROCEDURE:
      m_object_type = OBJECT_TYPE_PROCEDURE;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::TRIGGER:
      m_object_type = OBJECT_TYPE_TRIGGER;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::EVENT:
      m_object_type = OBJECT_TYPE_EVENT;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::COMMIT:
      m_object_type = OBJECT_TYPE_COMMIT;
      break;
    case MDL_key::USER_LEVEL_LOCK:
      m_object_type = OBJECT_TYPE_USER_LEVEL_LOCK;
      with_object = true;
      break;
    case MDL_key::LOCKING_SERVICE:
      m_object_type = OBJECT_TYPE_LOCKING_SERVICE;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::SRID:
      m_object_type = OBJECT_TYPE_SRID;
      with_object = true;
      break;
    case MDL_key::ACL_CACHE:
      m_object_type = OBJECT_TYPE_ACL_CACHE;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::COLUMN_STATISTICS:
      m_object_type = OBJECT_TYPE_COLUMN_STATISTICS;
      with_schema = true;
      with_object = true;
      with_column = true;
      break;
    case MDL_key::BACKUP_LOCK:
      m_object_type = OBJECT_TYPE_BACKUP_LOCK;
      break;
    case MDL_key::RESOURCE_GROUPS:
      m_object_type = OBJECT_TYPE_RESOURCE_GROUPS;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::FOREIGN_KEY:
      m_object_type = OBJECT_TYPE_FOREIGN_KEY;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::CHECK_CONSTRAINT:
      m_object_type = OBJECT_TYPE_CHECK_CONSTRAINT;
      with_schema = true;
      with_object = true;
      break;
    case MDL_key::NAMESPACE_END:
    default:
      assert(false);
      m_object_type = NO_OBJECT_TYPE;
      break;
  }

  if (with_schema) {
    m_schema_name_length = mdl->db_name_length();
    if (m_schema_name_length > sizeof(m_schema_name)) {
      assert(false);
      return 1;
    }
    if (m_schema_name_length > 0) {
      memcpy(m_schema_name, mdl->db_name(), m_schema_name_length);
    }
  }

  if (with_object) {
    m_object_name_length = mdl->name_length();
    if (m_object_name_length > sizeof(m_object_name)) {
      assert(false);
      return 1;
    }
    if (m_object_name_length > 0) {
      memcpy(m_object_name, mdl->name(), m_object_name_length);
    }
  }

  if (with_column) {
    m_column_name_length = mdl->col_name_length();
    if (m_column_name_length > sizeof(m_column_name)) {
      assert(false);
      return 1;
    }
    if (m_column_name_length > 0) {
      memcpy(m_column_name, mdl->col_name(), m_column_name_length);
    }
  }

  return 0;
}

void PFS_object_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
      set_field_object_type(f, m_object_type);
      break;
    case 1: /* SCHEMA_NAME */
      set_field_schema_name(f, &m_schema_name);
      break;
    case 2: /* OBJECT_NAME */
      set_field_object_name(f, &m_object_name);
      break;
    default:
      assert(false);
  }
}

void PFS_object_row::set_nullable_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
      if (m_object_type != NO_OBJECT_TYPE) {
        set_field_object_type(f, m_object_type);
      } else {
        f->set_null();
      }
      break;
    case 1: /* SCHEMA_NAME */
      set_nullable_field_schema_name(f, &m_schema_name);
      break;
    case 2: /* OBJECT_NAME */
      set_nullable_field_object_name(f, &m_object_name);
      break;
    default:
      assert(false);
  }
}

void PFS_object_view_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
      set_field_object_type(f, m_object_type);
      break;
    case 1: /* SCHEMA_NAME */
      set_field_schema_name(f, &m_schema_name);
      break;
    case 2: /* OBJECT_NAME */
      set_field_object_name(f, &m_object_name);
      break;
    default:
      assert(false);
  }
}

void PFS_object_view_row::set_nullable_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
      if (m_object_type != NO_OBJECT_TYPE) {
        set_field_object_type(f, m_object_type);
      } else {
        f->set_null();
      }
      break;
    case 1: /* SCHEMA_NAME */
      set_nullable_field_schema_name(f, &m_schema_name);
      break;
    case 2: /* OBJECT_NAME */
      set_nullable_field_object_name(f, &m_object_name);
      break;
    default:
      assert(false);
  }
}

void PFS_column_row::set_nullable_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
      if (m_object_type != NO_OBJECT_TYPE) {
        set_field_object_type(f, m_object_type);
      } else {
        f->set_null();
      }
      break;
    case 1: /* SCHEMA_NAME */
      if (m_schema_name_length > 0) {
        set_field_varchar_utf8mb4(f, m_schema_name, m_schema_name_length);
      } else {
        f->set_null();
      }
      break;
    case 2: /* OBJECT_NAME */
      if (m_object_name_length > 0) {
        set_field_varchar_utf8mb4(f, m_object_name, m_object_name_length);
      } else {
        f->set_null();
      }
      break;
    case 3: /* COLUMN_NAME */
      if (m_column_name_length > 0) {
        set_field_varchar_utf8mb4(f, m_column_name, m_column_name_length);
      } else {
        f->set_null();
      }
      break;
    default:
      assert(false);
  }
}

int PFS_index_row::make_index_name(PFS_table_share_index *pfs_index,
                                   uint table_index) {
  if (pfs_index == nullptr) {
    if (table_index < MAX_INDEXES) {
      m_index_name_length = sprintf(m_index_name, "(index %d)", table_index);
    } else {
      m_index_name_length = 0;
    }
    return 0;
  }

  if (table_index < MAX_INDEXES) {
    m_index_name_length = pfs_index->m_key.m_name_length;
    if (m_index_name_length > sizeof(m_index_name)) {
      return 1;
    }

    memcpy(m_index_name, pfs_index->m_key.m_name, sizeof(m_index_name));
  } else {
    m_index_name_length = 0;
  }

  return 0;
}

int PFS_index_row::make_row(PFS_table_share *pfs,
                            PFS_table_share_index *pfs_index,
                            uint table_index) {
  if (m_object_row.make_row(pfs)) {
    return 1;
  }

  if (make_index_name(pfs_index, table_index)) {
    return 1;
  }

  return 0;
}

void PFS_index_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name_length > 0) {
        set_field_varchar_utf8mb4(f, m_index_name, m_index_name_length);
      } else {
        f->set_null();
      }
      break;
    default:
      assert(false);
  }
}

void PFS_index_row::set_nullable_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_nullable_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name_length > 0) {
        set_field_varchar_utf8mb4(f, m_index_name, m_index_name_length);
      } else {
        f->set_null();
      }
      break;
    default:
      assert(false);
  }
}

void PFS_index_view_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name.length() > 0) {
        set_field_varchar_utf8mb4(f, m_index_name.ptr(), m_index_name.length());
      } else {
        f->set_null();
      }
      break;
    default:
      assert(false);
  }
}

void PFS_index_view_row::set_nullable_field(uint index, Field *f) {
  switch (index) {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_nullable_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name.length() > 0) {
        set_field_varchar_utf8mb4(f, m_index_name.ptr(), m_index_name.length());
      } else {
        f->set_null();
      }
      break;
    default:
      assert(false);
  }
}

void PFS_statement_stat_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* COUNT_STAR */
    case 1: /* SUM_TIMER_WAIT */
    case 2: /* MIN_TIMER_WAIT */
    case 3: /* AVG_TIMER_WAIT */
    case 4: /* MAX_TIMER_WAIT */
      m_timer1_row.set_field(index, f);
      break;
    case 5: /* SUM_LOCK_TIME */
      set_field_ulonglong(f, m_lock_time);
      break;
    case 6: /* SUM_ERRORS */
      set_field_ulonglong(f, m_error_count);
      break;
    case 7: /* SUM_WARNINGS */
      set_field_ulonglong(f, m_warning_count);
      break;
    case 8: /* SUM_ROWS_AFFECTED */
      set_field_ulonglong(f, m_rows_affected);
      break;
    case 9: /* SUM_ROWS_SENT */
      set_field_ulonglong(f, m_rows_sent);
      break;
    case 10: /* SUM_ROWS_EXAMINED */
      set_field_ulonglong(f, m_rows_examined);
      break;
    case 11: /* SUM_CREATED_TMP_DISK_TABLES */
      set_field_ulonglong(f, m_created_tmp_disk_tables);
      break;
    case 12: /* SUM_CREATED_TMP_TABLES */
      set_field_ulonglong(f, m_created_tmp_tables);
      break;
    case 13: /* SUM_SELECT_FULL_JOIN */
      set_field_ulonglong(f, m_select_full_join);
      break;
    case 14: /* SUM_SELECT_FULL_RANGE_JOIN */
      set_field_ulonglong(f, m_select_full_range_join);
      break;
    case 15: /* SUM_SELECT_RANGE */
      set_field_ulonglong(f, m_select_range);
      break;
    case 16: /* SUM_SELECT_RANGE_CHECK */
      set_field_ulonglong(f, m_select_range_check);
      break;
    case 17: /* SUM_SELECT_SCAN */
      set_field_ulonglong(f, m_select_scan);
      break;
    case 18: /* SUM_SORT_MERGE_PASSES */
      set_field_ulonglong(f, m_sort_merge_passes);
      break;
    case 19: /* SUM_SORT_RANGE */
      set_field_ulonglong(f, m_sort_range);
      break;
    case 20: /* SUM_SORT_ROWS */
      set_field_ulonglong(f, m_sort_rows);
      break;
    case 21: /* SUM_SORT_SCAN */
      set_field_ulonglong(f, m_sort_scan);
      break;
    case 22: /* SUM_NO_INDEX_USED */
      set_field_ulonglong(f, m_no_index_used);
      break;
    case 23: /* SUM_NO_GOOD_INDEX_USED */
      set_field_ulonglong(f, m_no_good_index_used);
      break;
    case 24: /* SUM_CPU_TIME */
      set_field_ulonglong(f, m_cpu_time);
      break;
    case 25: /* MAX_CONTROLLED_MEMORY */
      set_field_ulonglong(f, m_max_controlled_memory);
      break;
    case 26: /* MAX_TOTAL_MEMORY */
      set_field_ulonglong(f, m_max_total_memory);
      break;
    case 27: /* COUNT_SECONDARY */
      set_field_ulonglong(f, m_count_secondary);
      break;
    default:
      assert(false);
      break;
  }
}

void PFS_transaction_stat_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* COUNT_STAR */
    case 1: /* SUM_TIMER_WAIT */
    case 2: /* MIN_TIMER_WAIT */
    case 3: /* AVG_TIMER_WAIT */
    case 4: /* MAX_TIMER_WAIT */
      m_timer1_row.set_field(index, f);
      break;
    case 5: /* COUNT_READ_WRITE */
    case 6: /* SUM_TIMER_READ_WRITE */
    case 7: /* MIN_TIMER_READ_WRITE */
    case 8: /* AVG_TIMER_READ_WRITE */
    case 9: /* MAX_TIMER_READ_WRITE */
      m_read_write_row.set_field(index - 5, f);
      break;
    case 10: /* COUNT_READ_ONLY */
    case 11: /* SUM_TIMER_READ_ONLY */
    case 12: /* MIN_TIMER_READ_ONLY */
    case 13: /* AVG_TIMER_READ_ONLY */
    case 14: /* MAX_TIMER_READ_ONLY */
      m_read_only_row.set_field(index - 10, f);
      break;
    default:
      assert(false);
      break;
  }
}

void PFS_error_stat_row::set_field(uint index, Field *f,
                                   server_error *temp_error) {
  switch (index) {
    case 0: /* ERROR NUMBER */
      if (temp_error) {
        set_field_long(f, temp_error->mysql_errno);
      } else /* NULL ROW */
      {
        f->set_null();
      }
      break;
    case 1: /* ERROR NAME */
      if (temp_error) {
        set_field_varchar_utf8mb4(f, temp_error->name,
                                  (uint)strlen(temp_error->name));
      } else /* NULL ROW */
      {
        f->set_null();
      }
      break;
    case 2: /* SQLSTATE */
      if (temp_error) {
        set_field_varchar_utf8mb4(f, temp_error->odbc_state,
                                  (uint)strlen(temp_error->odbc_state));
      } else /* NULL ROW */
      {
        f->set_null();
      }
      break;
    case 3: /* SUM_ERROR_RAISED */
      set_field_ulonglong(f, m_count);
      break;
    case 4: /* SUM_ERROR_HANDLED */
      set_field_ulonglong(f, m_handled_count);
      break;
    case 5: /* FIRST_SEEN */
      if (m_first_seen != 0) {
        set_field_timestamp(f, m_first_seen);
      } else {
        f->set_null();
      }
      break;
    case 6: /* LAST_SEEN */
      if (m_last_seen != 0) {
        set_field_timestamp(f, m_last_seen);
      } else {
        f->set_null();
      }
      break;
    default:
      /* It should never be reached */
      assert(false);
      break;
  }
}

void PFS_connection_stat_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* CURRENT_CONNECTIONS */
      set_field_ulonglong(f, m_stat.m_current_connections);
      break;
    case 1: /* TOTAL_CONNECTIONS */
      set_field_ulonglong(f, m_stat.m_total_connections);
      break;
    case 2: /* MAX_SESSION_CONTROLLED_MEMORY */
      set_field_ulonglong(f, m_stat.m_max_session_controlled_memory);
      break;
    case 3: /* MAX_SESSION_TOTAL_MEMORY */
      set_field_ulonglong(f, m_stat.m_max_session_total_memory);
      break;
    default:
      assert(false);
      break;
  }
}

void set_field_object_type(Field *f, enum_object_type object_type) {
  const char *name;
  size_t length;
  object_type_to_string(object_type, &name, &length);
  set_field_varchar_utf8mb4(f, name, (uint)length);
}

void set_field_lock_type(Field *f, PFS_TL_LOCK_TYPE lock_type) {
  switch (lock_type) {
    case PFS_TL_READ:
      set_field_varchar_utf8mb4(f, "READ", 4);
      break;
    case PFS_TL_READ_WITH_SHARED_LOCKS:
      set_field_varchar_utf8mb4(f, "READ WITH SHARED LOCKS", 22);
      break;
    case PFS_TL_READ_HIGH_PRIORITY:
      set_field_varchar_utf8mb4(f, "READ HIGH PRIORITY", 18);
      break;
    case PFS_TL_READ_NO_INSERT:
      set_field_varchar_utf8mb4(f, "READ NO INSERT", 14);
      break;
    case PFS_TL_WRITE_ALLOW_WRITE:
      set_field_varchar_utf8mb4(f, "WRITE ALLOW WRITE", 17);
      break;
    case PFS_TL_WRITE_CONCURRENT_INSERT:
      set_field_varchar_utf8mb4(f, "WRITE CONCURRENT INSERT", 23);
      break;
    case PFS_TL_WRITE_LOW_PRIORITY:
      set_field_varchar_utf8mb4(f, "WRITE LOW PRIORITY", 18);
      break;
    case PFS_TL_WRITE:
      set_field_varchar_utf8mb4(f, "WRITE", 5);
      break;
    case PFS_TL_READ_EXTERNAL:
      set_field_varchar_utf8mb4(f, "READ EXTERNAL", 13);
      break;
    case PFS_TL_WRITE_EXTERNAL:
      set_field_varchar_utf8mb4(f, "WRITE EXTERNAL", 14);
      break;
    case PFS_TL_NONE:
      f->set_null();
      break;
    default:
      assert(false);
  }
}

void set_field_mdl_type(Field *f, opaque_mdl_type mdl_type) {
  static_assert(MDL_TYPE_END == 11,
                "Adjust performance schema when changing enum_mdl_type");

  const auto e = (enum_mdl_type)mdl_type;
  switch (e) {
    case MDL_INTENTION_EXCLUSIVE:
      set_field_varchar_utf8mb4(f, "INTENTION_EXCLUSIVE", 19);
      break;
    case MDL_SHARED:
      set_field_varchar_utf8mb4(f, "SHARED", 6);
      break;
    case MDL_SHARED_HIGH_PRIO:
      set_field_varchar_utf8mb4(f, "SHARED_HIGH_PRIO", 16);
      break;
    case MDL_SHARED_READ:
      set_field_varchar_utf8mb4(f, "SHARED_READ", 11);
      break;
    case MDL_SHARED_WRITE:
      set_field_varchar_utf8mb4(f, "SHARED_WRITE", 12);
      break;
    case MDL_SHARED_WRITE_LOW_PRIO:
      set_field_varchar_utf8mb4(f, "SHARED_WRITE_LOW_PRIO", 21);
      break;
    case MDL_SHARED_UPGRADABLE:
      set_field_varchar_utf8mb4(f, "SHARED_UPGRADABLE", 17);
      break;
    case MDL_SHARED_READ_ONLY:
      set_field_varchar_utf8mb4(f, "SHARED_READ_ONLY", 16);
      break;
    case MDL_SHARED_NO_WRITE:
      set_field_varchar_utf8mb4(f, "SHARED_NO_WRITE", 15);
      break;
    case MDL_SHARED_NO_READ_WRITE:
      set_field_varchar_utf8mb4(f, "SHARED_NO_READ_WRITE", 20);
      break;
    case MDL_EXCLUSIVE:
      set_field_varchar_utf8mb4(f, "EXCLUSIVE", 9);
      break;
    case MDL_TYPE_END:
    default:
      assert(false);
  }
}

void set_field_mdl_duration(Field *f, opaque_mdl_duration mdl_duration) {
  static_assert(MDL_DURATION_END == 3,
                "Adjust performance schema when changing enum_mdl_duration");

  const auto e = (enum_mdl_duration)mdl_duration;
  switch (e) {
    case MDL_STATEMENT:
      set_field_varchar_utf8mb4(f, "STATEMENT", 9);
      break;
    case MDL_TRANSACTION:
      set_field_varchar_utf8mb4(f, "TRANSACTION", 11);
      break;
    case MDL_EXPLICIT:
      set_field_varchar_utf8mb4(f, "EXPLICIT", 8);
      break;
    case MDL_DURATION_END:
    default:
      assert(false);
  }
}

void set_field_mdl_status(Field *f, opaque_mdl_status mdl_status) {
  const auto e = static_cast<MDL_ticket::enum_psi_status>(mdl_status);
  switch (e) {
    case MDL_ticket::PENDING:
      set_field_varchar_utf8mb4(f, "PENDING", 7);
      break;
    case MDL_ticket::GRANTED:
      set_field_varchar_utf8mb4(f, "GRANTED", 7);
      break;
    case MDL_ticket::PRE_ACQUIRE_NOTIFY:
      set_field_varchar_utf8mb4(f, "PRE_ACQUIRE_NOTIFY", 18);
      break;
    case MDL_ticket::POST_RELEASE_NOTIFY:
      set_field_varchar_utf8mb4(f, "POST_RELEASE_NOTIFY", 19);
      break;
    default:
      assert(false);
  }
}

void PFS_memory_stat_row::set_field(uint index, Field *f) {
  ssize_t val;

  switch (index) {
    case 0: /* COUNT_ALLOC */
      set_field_ulonglong(f, m_stat.m_alloc_count);
      break;
    case 1: /* COUNT_FREE */
      set_field_ulonglong(f, m_stat.m_free_count);
      break;
    case 2: /* SUM_NUMBER_OF_BYTES_ALLOC */
      set_field_ulonglong(f, m_stat.m_alloc_size);
      break;
    case 3: /* SUM_NUMBER_OF_BYTES_FREE */
      set_field_ulonglong(f, m_stat.m_free_size);
      break;
    case 4: /* LOW_COUNT_USED */
      val = m_stat.m_low_count_used;
      set_field_longlong(f, val);
      break;
    case 5: /* CURRENT_COUNT_USED */
      val = m_stat.m_alloc_count - m_stat.m_free_count;
      set_field_longlong(f, val);
      break;
    case 6: /* HIGH_COUNT_USED */
      val = m_stat.m_high_count_used;
      set_field_longlong(f, val);
      break;
    case 7: /* LOW_NUMBER_OF_BYTES_USED */
      val = m_stat.m_low_size_used;
      set_field_longlong(f, val);
      break;
    case 8: /* CURRENT_NUMBER_OF_BYTES_USED */
      val = m_stat.m_alloc_size - m_stat.m_free_size;
      set_field_longlong(f, val);
      break;
    case 9: /* HIGH_NUMBER_OF_BYTES_USED */
      val = m_stat.m_high_size_used;
      set_field_longlong(f, val);
      break;
    default:
      assert(false);
      break;
  }
}

void PFS_session_all_memory_stat_row::set(
    const PFS_session_all_memory_stat *stat) {
  m_controlled_size = stat->m_controlled.get_session_size();
  m_max_controlled_size = stat->m_controlled.get_session_max();
  m_total_size = stat->m_total.get_session_size();
  m_max_total_size = stat->m_total.get_session_max();
}

void PFS_session_all_memory_stat_row::set_field(uint index, Field *f) {
  switch (index) {
    case 0: /* CONTROLLED_MEMORY */
      set_field_ulonglong(f, m_controlled_size);
      break;
    case 1: /* MAX_CONTROLLED_MEMORY */
      set_field_ulonglong(f, m_max_controlled_size);
      break;
    case 2: /* TOTAL_MEMORY */
      set_field_ulonglong(f, m_total_size);
      break;
    case 3: /* MAX_TOTAL_MEMORY */
      set_field_ulonglong(f, m_max_total_size);
      break;
    default:
      assert(false);
      break;
  }
}

void set_field_isolation_level(Field *f, enum_isolation_level iso_level) {
  switch (iso_level) {
    case TRANS_LEVEL_READ_UNCOMMITTED:
      set_field_varchar_utf8mb4(f, "READ UNCOMMITTED", 16);
      break;
    case TRANS_LEVEL_READ_COMMITTED:
      set_field_varchar_utf8mb4(f, "READ COMMITTED", 14);
      break;
    case TRANS_LEVEL_REPEATABLE_READ:
      set_field_varchar_utf8mb4(f, "REPEATABLE READ", 15);
      break;
    case TRANS_LEVEL_SERIALIZABLE:
      set_field_varchar_utf8mb4(f, "SERIALIZABLE", 12);
      break;
    default:
      assert(false);
  }
}

void set_field_xa_state(Field *f, enum_xa_transaction_state xa_state) {
  switch (xa_state) {
    case TRANS_STATE_XA_NOTR:
      set_field_varchar_utf8mb4(f, "NOTR", 4);
      break;
    case TRANS_STATE_XA_ACTIVE:
      set_field_varchar_utf8mb4(f, "ACTIVE", 6);
      break;
    case TRANS_STATE_XA_IDLE:
      set_field_varchar_utf8mb4(f, "IDLE", 4);
      break;
    case TRANS_STATE_XA_PREPARED:
      set_field_varchar_utf8mb4(f, "PREPARED", 8);
      break;
    case TRANS_STATE_XA_ROLLBACK_ONLY:
      set_field_varchar_utf8mb4(f, "ROLLBACK ONLY", 13);
      break;
    case TRANS_STATE_XA_COMMITTED:
      set_field_varchar_utf8mb4(f, "COMMITTED", 9);
      break;
    default:
      assert(false);
  }
}

int PFS_variable_name_row::make_row(const char *str, size_t length) {
  assert(length <= sizeof(m_str));
  assert(length <= NAME_CHAR_LEN);

  /* enforce max name length */
  m_length = std::min(length, size_t{NAME_CHAR_LEN});
  if (m_length > 0) {
    memcpy(m_str, str, length);
  }
  m_str[m_length] = '\0';

  return 0;
}

int PFS_variable_value_row::make_row(const Status_variable *var) {
  return make_row(var->m_charset, var->m_value_str, var->m_value_length);
}

int PFS_variable_value_row::make_row(const System_variable *var) {
  return make_row(var->m_charset, var->m_value_str, var->m_value_length);
}

int PFS_variable_value_row::make_row(const CHARSET_INFO *cs, const char *str,
                                     size_t length) {
  assert(cs != nullptr);
  assert(length <= sizeof(m_str));
  if (length > 0) {
    memcpy(m_str, str, length);
  }
  m_length = (uint)length;
  m_charset = cs;

  return 0;
}

void PFS_variable_value_row::set_field(Field *f) {
  set_field_varchar(f, m_charset, m_str, m_length);
}

void PFS_user_variable_value_row::clear() {
  my_free(m_value);
  m_value = nullptr;
  m_value_length = 0;
}

int PFS_user_variable_value_row::make_row(const char *val, size_t length) {
  if (length > 0) {
    m_value = (char *)my_malloc(PSI_NOT_INSTRUMENTED, length, MYF(0));
    m_value_length = length;
    memcpy(m_value, val, length);
  } else {
    m_value = nullptr;
    m_value_length = 0;
  }

  return 0;
}

/*
  Code is the same for all int types,
  expects the following parameters:
  bool record_null
  <T> record_value,
  bool m_is_null,
  <T> m_key_value,
  enum ha_rkey_function m_find_flag
*/
#define COMMON_STATELESS_MATCH               \
  int cmp = 0;                               \
  if (is_null) {                             \
    cmp = (record_null ? 0 : 1);             \
  } else {                                   \
    if (record_null) {                       \
      cmp = -1;                              \
    } else if (record_value < m_key_value) { \
      cmp = -1;                              \
    } else if (record_value > m_key_value) { \
      cmp = +1;                              \
    } else {                                 \
      cmp = 0;                               \
    }                                        \
  }                                          \
  switch (find_flag) {                       \
    case HA_READ_KEY_EXACT:                  \
      return (cmp == 0);                     \
    case HA_READ_KEY_OR_NEXT:                \
      return (cmp >= 0);                     \
    case HA_READ_KEY_OR_PREV:                \
      return (cmp <= 0);                     \
    case HA_READ_BEFORE_KEY:                 \
      return (cmp < 0);                      \
    case HA_READ_AFTER_KEY:                  \
      return (cmp > 0);                      \
    default:                                 \
      assert(false);                         \
      return false;                          \
  }

bool PFS_key_long::stateless_match(bool record_null, long record_value,
                                   bool is_null, long m_key_value,
                                   enum ha_rkey_function find_flag) {
  COMMON_STATELESS_MATCH
}

bool PFS_key_ulong::stateless_match(bool record_null, ulong record_value,
                                    bool is_null, ulong m_key_value,
                                    enum ha_rkey_function find_flag) {
  COMMON_STATELESS_MATCH
}

bool PFS_key_longlong::stateless_match(bool record_null, longlong record_value,
                                       bool is_null, longlong m_key_value,
                                       enum ha_rkey_function find_flag) {
  COMMON_STATELESS_MATCH
}

bool PFS_key_ulonglong::stateless_match(bool record_null,
                                        ulonglong record_value, bool is_null,
                                        ulonglong m_key_value,
                                        enum ha_rkey_function find_flag) {
  COMMON_STATELESS_MATCH
}

bool PFS_key_ulong::do_match(bool record_null, ulong record_value) {
  int cmp = 0;

  if (m_is_null) {
    cmp = (record_null ? 0 : 1);
  } else {
    if (record_null) {
      cmp = -1;
    } else if (record_value < m_key_value) {
      cmp = -1;
    } else if (record_value > m_key_value) {
      cmp = +1;
    } else {
      cmp = 0;
    }
  }

  switch (m_find_flag) {
    case HA_READ_KEY_EXACT:
      return (cmp == 0);
    case HA_READ_KEY_OR_NEXT:
      return (cmp >= 0);
    case HA_READ_KEY_OR_PREV:
      return (cmp <= 0);
    case HA_READ_BEFORE_KEY:
      return (cmp < 0);
    case HA_READ_AFTER_KEY:
      return (cmp > 0);
    default:
      assert(false);
      return false;
  }
}

bool PFS_key_ulonglong::do_match(bool record_null, ulonglong record_value) {
  int cmp = 0;

  if (m_is_null) {
    cmp = (record_null ? 0 : 1);
  } else {
    if (record_null) {
      cmp = -1;
    } else if (record_value < m_key_value) {
      cmp = -1;
    } else if (record_value > m_key_value) {
      cmp = +1;
    } else {
      cmp = 0;
    }
  }

  switch (m_find_flag) {
    case HA_READ_KEY_EXACT:
      return (cmp == 0);
    case HA_READ_KEY_OR_NEXT:
      return (cmp >= 0);
    case HA_READ_KEY_OR_PREV:
      return (cmp <= 0);
    case HA_READ_BEFORE_KEY:
      return (cmp < 0);
    case HA_READ_AFTER_KEY:
      return (cmp > 0);
    default:
      assert(false);
      return false;
  }
}

bool PFS_key_pstring::stateless_match(bool record_null,
                                      const char *record_string,
                                      size_t record_string_length,
                                      const char *m_key_value,
                                      size_t m_key_value_length, bool is_null,
                                      enum ha_rkey_function find_flag) {
  if (find_flag == HA_READ_KEY_EXACT) {
    if (is_null) {
      return record_null;
    }

    if (record_null) {
      return false;
    }

    if (m_key_value_length != record_string_length) {
      return false;
    }

    return (native_strncasecmp(record_string, m_key_value,
                               m_key_value_length) == 0);
  }

  int cmp = 0;

  if (is_null) {
    cmp = record_null ? 0 : 1;
  } else {
    if (record_null) {
      cmp = -1;
    } else {
      cmp = native_strncasecmp(record_string, m_key_value, m_key_value_length);
    }
  }

  switch (find_flag) {
    case HA_READ_KEY_OR_NEXT:
      return (cmp >= 0);
    case HA_READ_KEY_OR_PREV:
      return (cmp <= 0);
    case HA_READ_BEFORE_KEY:
      return (cmp < 0);
    case HA_READ_AFTER_KEY:
      return (cmp > 0);
    default:
      assert(false);
      return false;
  }
}

template <int SIZE>
bool PFS_key_string<SIZE>::do_match_prefix(bool record_null,
                                           const char *record_string,
                                           size_t record_string_length) {
  if (m_is_null) {
    return record_null;
  }

  if (record_null) {
    return false;
  }

  if (record_string_length > m_key_value_length) {
    return false;
  }

  return (native_strncasecmp(record_string, m_key_value,
                             record_string_length) == 0);
}

bool PFS_key_thread_id::match(ulonglong thread_id) {
  const bool record_null = (thread_id == 0);
  return do_match(record_null, thread_id);
}

bool PFS_key_thread_id::match(const PFS_thread *pfs) {
  const bool record_null = (pfs->m_thread_internal_id == 0);
  return do_match(record_null, pfs->m_thread_internal_id);
}

bool PFS_key_thread_id::match_owner(const PFS_table *pfs) {
  PFS_thread *thread = sanitize_thread(pfs->m_thread_owner);

  if (thread == nullptr) {
    return do_match(true, 0);
  }

  const bool record_null = (thread->m_thread_internal_id == 0);
  return do_match(record_null, thread->m_thread_internal_id);
}

bool PFS_key_thread_id::match_owner(const PFS_socket *pfs) {
  PFS_thread *thread = sanitize_thread(pfs->m_thread_owner);

  if (thread == nullptr) {
    return do_match(true, 0);
  }

  return do_match(false, thread->m_thread_internal_id);
}

bool PFS_key_thread_id::match_owner(const PFS_mutex *pfs) {
  PFS_thread *thread = sanitize_thread(pfs->m_owner);

  if (thread == nullptr) {
    return do_match(true, 0);
  }

  return do_match(false, thread->m_thread_internal_id);
}

bool PFS_key_thread_id::match_owner(const PFS_prepared_stmt *pfs) {
  const bool record_null = (pfs->m_owner_thread_id == 0);
  return do_match(record_null, pfs->m_owner_thread_id);
}

bool PFS_key_thread_id::match_owner(const PFS_metadata_lock *pfs) {
  const bool record_null = (pfs->m_owner_thread_id == 0);
  return do_match(record_null, pfs->m_owner_thread_id);
}

bool PFS_key_thread_id::match_writer(const PFS_rwlock *pfs) {
  PFS_thread *thread = sanitize_thread(pfs->m_writer);

  if (thread == nullptr) {
    return do_match(true, 0);
  }

  return do_match(false, thread->m_thread_internal_id);
}

bool PFS_key_event_id::match(ulonglong event_id) {
  const bool record_null = (event_id == 0);
  return do_match(record_null, event_id);
}

bool PFS_key_event_id::match(const PFS_events *pfs) {
  const bool record_null = (pfs->m_event_id == 0);
  return do_match(record_null, pfs->m_event_id);
}

bool PFS_key_event_id::match(const PFS_events_waits *pfs) {
  const bool record_null = (pfs->m_event_id == 0);
  return do_match(record_null, pfs->m_event_id);
}

bool PFS_key_event_id::match_owner(const PFS_table *pfs) {
  const bool record_null = (pfs->m_owner_event_id == 0);
  return do_match(record_null, pfs->m_owner_event_id);
}

bool PFS_key_event_id::match_owner(const PFS_prepared_stmt *pfs) {
  const bool record_null = (pfs->m_owner_event_id == 0);
  return do_match(record_null, pfs->m_owner_event_id);
}

bool PFS_key_event_id::match_owner(const PFS_metadata_lock *pfs) {
  const bool record_null = (pfs->m_owner_event_id == 0);
  return do_match(record_null, pfs->m_owner_event_id);
}

bool PFS_key_processlist_id::match(const PFS_thread *pfs) {
  const bool record_null = (pfs->m_processlist_id == 0);
  return do_match(record_null, pfs->m_processlist_id);
}

bool PFS_key_engine_transaction_id::match(ulonglong engine_transaction_id) {
  return do_match(false, engine_transaction_id);
}

bool PFS_key_thread_os_id::match(const PFS_thread *pfs) {
  const bool record_null = (pfs->m_thread_os_id == 0);
  return do_match(record_null, pfs->m_thread_os_id);
}

bool PFS_key_statement_id::match(const PFS_prepared_stmt *pfs) {
  const bool record_null = (pfs->m_stmt_id == 0);
  return do_match(record_null, pfs->m_stmt_id);
}

bool PFS_key_worker_id::match_not_null(ulonglong worker_id) {
  return do_match(false, worker_id);
}

bool PFS_key_socket_id::match(const PFS_socket *pfs) {
  const bool record_null = (pfs->m_fd == 0);
  return do_match(record_null, (int32)pfs->m_fd);
}

bool PFS_key_port::match(const PFS_socket *pfs) {
  bool record_null = (pfs->m_addr_len == 0);
  uint port = 0;
  char ip[INET6_ADDRSTRLEN + 1];
  uint ip_len = 0;
  if (!record_null) {
    ip_len = pfs_get_socket_address(ip, sizeof(ip), &port, &pfs->m_sock_addr,
                                    pfs->m_addr_len);
    record_null = (ip_len == 0);
  }
  return do_match(record_null, (int32)port);
}

bool PFS_key_port::match(uint port) {
  const bool record_null = (port == 0);
  return do_match(record_null, (int32)port);
}

bool PFS_key_error_number::match_error_index(uint error_index) {
  assert(error_index < PFS_MAX_GLOBAL_SERVER_ERRORS);

  server_error *temp_error;
  temp_error = &error_names_array[pfs_to_server_error_map[error_index]];

  const bool record_null = (temp_error->mysql_errno == 0);
  return do_match(record_null, (int32)temp_error->mysql_errno);
}

bool PFS_key_thread_name::match(const PFS_thread *pfs) {
  PFS_thread_class *klass = sanitize_thread_class(pfs->m_class);
  if (klass == nullptr) {
    return false;
  }

  return match(klass);
}

bool PFS_key_thread_name::match(const PFS_thread_class *klass) {
  return do_match(false, klass->m_name.str(), klass->m_name.length());
}

bool PFS_key_event_name::match(const PFS_instr_class *pfs) {
  return do_match(false, pfs->m_name.str(), pfs->m_name.length());
}

bool PFS_key_event_name::match(const PFS_mutex *pfs) {
  PFS_mutex_class *safe_class = sanitize_mutex_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }

  return do_match(false, safe_class->m_name.str(), safe_class->m_name.length());
}

bool PFS_key_event_name::match(const PFS_rwlock *pfs) {
  PFS_rwlock_class *safe_class = sanitize_rwlock_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }
  return do_match(false, safe_class->m_name.str(), safe_class->m_name.length());
}

bool PFS_key_event_name::match(const PFS_cond *pfs) {
  PFS_cond_class *safe_class = sanitize_cond_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }
  return do_match(false, safe_class->m_name.str(), safe_class->m_name.length());
}

bool PFS_key_event_name::match(const PFS_file *pfs) {
  PFS_file_class *safe_class = sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }
  return do_match(false, safe_class->m_name.str(), safe_class->m_name.length());
}

bool PFS_key_event_name::match(const PFS_socket *pfs) {
  PFS_socket_class *safe_class = sanitize_socket_class(pfs->m_class);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }
  return do_match(false, safe_class->m_name.str(), safe_class->m_name.length());
}

bool PFS_key_event_name::match_view(uint view) {
  switch (view) {
    case PFS_instrument_view_constants::VIEW_MUTEX:
      return do_match_prefix(false, mutex_instrument_prefix.str,
                             mutex_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_RWLOCK:
      bool match;
      match = do_match_prefix(false, prlock_instrument_prefix.str,
                              prlock_instrument_prefix.length);
      if (!match) {
        match = do_match_prefix(false, rwlock_instrument_prefix.str,
                                rwlock_instrument_prefix.length);
      }
      if (!match) {
        match = do_match_prefix(false, sxlock_instrument_prefix.str,
                                sxlock_instrument_prefix.length);
      }
      return match;

    case PFS_instrument_view_constants::VIEW_COND:
      return do_match_prefix(false, cond_instrument_prefix.str,
                             cond_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_FILE:
      return do_match_prefix(false, file_instrument_prefix.str,
                             file_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_TABLE:
      if (do_match_prefix(false, table_io_class_name.str,
                          table_io_class_name.length)) {
        return true;
      }
      return do_match_prefix(false, table_lock_class_name.str,
                             table_lock_class_name.length);

    case PFS_instrument_view_constants::VIEW_SOCKET:
      return do_match_prefix(false, socket_instrument_prefix.str,
                             socket_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_IDLE:
      return do_match_prefix(false, idle_class_name.str,
                             idle_class_name.length);

    case PFS_instrument_view_constants::VIEW_METADATA:
      return do_match_prefix(false, metadata_lock_class_name.str,
                             metadata_lock_class_name.length);

    case PFS_instrument_view_constants::VIEW_STAGE:
      return do_match_prefix(false, stage_instrument_prefix.str,
                             stage_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_STATEMENT:
      return do_match_prefix(false, statement_instrument_prefix.str,
                             statement_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_TRANSACTION:
      return do_match_prefix(false, transaction_instrument_prefix.str,
                             transaction_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_BUILTIN_MEMORY:
      return do_match_prefix(false, builtin_memory_instrument_prefix.str,
                             builtin_memory_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_MEMORY:
      return do_match_prefix(false, memory_instrument_prefix.str,
                             memory_instrument_prefix.length);

    case PFS_instrument_view_constants::VIEW_ERROR:
      return do_match_prefix(false, error_class_name.str,
                             error_class_name.length);

    default:
      return false;
  }
}

bool PFS_key_meter_name::match(PFS_meter_class *pfs) {
  PFS_meter_class *safe_class = sanitize_meter_class(pfs);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }
  return do_match(false, safe_class->m_meter, safe_class->m_meter_length);
}

bool PFS_key_metric_name::match(PFS_metric_class *pfs) {
  PFS_metric_class *safe_class = sanitize_metric_class(pfs);
  if (unlikely(safe_class == nullptr)) {
    return false;
  }
  return do_match(false, safe_class->m_metric, safe_class->m_metric_length);
}

bool PFS_key_user::match(const PFS_thread *pfs) {
  const bool record_null = (pfs->m_user_name.length() == 0);
  return do_match(record_null, pfs->m_user_name.ptr(),
                  pfs->m_user_name.length());
}

bool PFS_key_user::match(const PFS_user *pfs) {
  const bool record_null = (pfs->m_key.m_user_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_user_name.ptr(),
                  pfs->m_key.m_user_name.length());
}

bool PFS_key_user::match(const PFS_account *pfs) {
  const bool record_null = (pfs->m_key.m_user_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_user_name.ptr(),
                  pfs->m_key.m_user_name.length());
}

bool PFS_key_user::match(const PFS_setup_actor *pfs) {
  const bool record_null = (pfs->m_key.m_user_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_user_name.ptr(),
                  pfs->m_key.m_user_name.length());
}

bool PFS_key_host::match(const PFS_thread *pfs) {
  const bool record_null = (pfs->m_host_name.length() == 0);
  return do_match(record_null, pfs->m_host_name.ptr(),
                  pfs->m_host_name.length());
}

bool PFS_key_host::match(const PFS_host *pfs) {
  const bool record_null = (pfs->m_key.m_host_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_host_name.ptr(),
                  pfs->m_key.m_host_name.length());
}

bool PFS_key_host::match(const PFS_account *pfs) {
  const bool record_null = (pfs->m_key.m_host_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_host_name.ptr(),
                  pfs->m_key.m_host_name.length());
}

bool PFS_key_host::match(const PFS_setup_actor *pfs) {
  const bool record_null = (pfs->m_key.m_host_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_host_name.ptr(),
                  pfs->m_key.m_host_name.length());
}

bool PFS_key_host::match(const char *hostname, size_t hostname_length) {
  const bool record_null = (hostname_length == 0);
  return do_match(record_null, hostname, hostname_length);
}

bool PFS_key_role::match(const PFS_setup_actor *pfs) {
  const bool record_null = (pfs->m_key.m_role_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_role_name.ptr(),
                  pfs->m_key.m_role_name.length());
}

bool PFS_key_schema::match(const PFS_statements_digest_stat *pfs) {
  const bool record_null = (pfs->m_digest_key.m_schema_name.length() == 0);
  return do_match(record_null, pfs->m_digest_key.m_schema_name.ptr(),
                  pfs->m_digest_key.m_schema_name.length());
}

bool PFS_key_digest::match(PFS_statements_digest_stat *pfs) {
  const bool record_null = (pfs->m_digest_storage.is_empty());
  char hash_string[DIGEST_HASH_TO_STRING_LENGTH + 1];

  DIGEST_HASH_TO_STRING(pfs->m_digest_storage.m_hash, hash_string);

  return do_match(record_null, hash_string, DIGEST_HASH_TO_STRING_LENGTH);
}

bool PFS_key_bucket_number::match(ulong value) {
  return do_match(false, value);
}

bool PFS_key_name::match(const LEX_CSTRING *name) {
  const bool record_null = (name->length == 0);
  return do_match(record_null, name->str, name->length);
}

bool PFS_key_name::match(const char *name, size_t name_length) {
  const bool record_null = (name_length == 0);
  return do_match(record_null, name, name_length);
}

bool PFS_key_name::match_not_null(const LEX_STRING *name) {
  return do_match(false, name->str, name->length);
}

bool PFS_key_name::match_not_null(const char *name, size_t name_length) {
  return do_match(false, name, name_length);
}

bool PFS_key_group_name::match(const LEX_STRING *name) {
  const bool record_null = (name->length == 0);
  return do_match(record_null, name->str, name->length);
}

bool PFS_key_group_name::match(const char *name, size_t name_length) {
  const bool record_null = (name_length == 0);
  return do_match(record_null, name, name_length);
}

bool PFS_key_group_name::match(PFS_thread *pfs) {
  const bool record_null = (pfs->m_groupname_length == 0);
  return do_match(record_null, pfs->m_groupname, pfs->m_groupname_length);
}

bool PFS_key_variable_name::match(const System_variable *pfs) {
  return do_match(false, pfs->m_name, pfs->m_name_length);
}

bool PFS_key_variable_name::match(const Status_variable *pfs) {
  return do_match(false, pfs->m_name, pfs->m_name_length);
}

bool PFS_key_variable_name::match(const PFS_variable_name_row *pfs) {
  const bool record_null = (pfs->m_length == 0);
  return do_match(record_null, pfs->m_str, pfs->m_length);
}

bool PFS_key_engine_name::match(const char *engine_name, size_t length) {
  return do_match(false, engine_name, length);
}

bool PFS_key_engine_lock_id::match(const char *engine_lock_id, size_t length) {
  return do_match(false, engine_lock_id, length);
}

bool PFS_key_ip::match(const PFS_socket *pfs) {
  bool record_null = (pfs->m_addr_len == 0);
  uint port = 0;
  char ip[INET6_ADDRSTRLEN + 1];
  size_t ip_len = 0;
  if (!record_null) {
    ip_len = pfs_get_socket_address(ip, sizeof(ip), &port, &pfs->m_sock_addr,
                                    pfs->m_addr_len);
    record_null = (ip_len == 0);
  }
  return do_match(record_null, (const char *)ip, ip_len);
}

bool PFS_key_ip::match(const char *ip, size_t ip_length) {
  const bool record_null = (ip_length == 0);
  return do_match(record_null, ip, ip_length);
}

bool PFS_key_statement_name::match(const PFS_prepared_stmt *pfs) {
  return do_match(false, pfs->m_stmt_name, pfs->m_stmt_name_length);
}

bool PFS_key_file_name::match(const PFS_file *pfs) {
  return do_match(false, pfs->m_file_name.ptr(), pfs->m_file_name.length());
}

void PFS_key_object_type::read(PFS_key_reader &reader,
                               enum ha_rkey_function find_flag) {
  char object_type_string[255];  // FIXME
  uint object_type_string_length;

  m_find_flag = reader.read_varchar_utf8(
      find_flag, m_is_null, object_type_string, &object_type_string_length,
      sizeof(object_type_string));
  if (m_is_null) {
    m_object_type = NO_OBJECT_TYPE;
  } else {
    string_to_object_type(object_type_string, object_type_string_length,
                          &m_object_type);
  }
}

bool PFS_key_object_type::match(enum_object_type object_type) {
  const bool record_null = (object_type == NO_OBJECT_TYPE);
  return do_match(record_null, object_type);
}

bool PFS_key_object_type::match(const PFS_object_row *pfs) {
  const bool record_null = (pfs->m_object_type == NO_OBJECT_TYPE);
  return do_match(record_null, pfs->m_object_type);
}

bool PFS_key_object_type::match(const PFS_column_row *pfs) {
  const bool record_null = (pfs->m_object_type == NO_OBJECT_TYPE);
  return do_match(record_null, pfs->m_object_type);
}

bool PFS_key_object_type::match(const PFS_program *pfs) {
  const bool record_null = (pfs->m_key.m_type == NO_OBJECT_TYPE);
  return do_match(record_null, pfs->m_key.m_type);
}

bool PFS_key_object_type::do_match(bool record_null,
                                   enum_object_type record_value) {
  int cmp = 0;

  if (m_is_null) {
    cmp = (record_null ? 0 : 1);
  } else {
    if (record_null) {
      cmp = -1;
    } else if (record_value < m_object_type) {
      cmp = -1;
    } else if (record_value > m_object_type) {
      cmp = +1;
    } else {
      cmp = 0;
    }
  }

  switch (m_find_flag) {
    case HA_READ_KEY_EXACT:
      return (cmp == 0);
    case HA_READ_KEY_OR_NEXT:
      return (cmp >= 0);
    case HA_READ_KEY_OR_PREV:
      return (cmp <= 0);
    case HA_READ_BEFORE_KEY:
      return (cmp < 0);
    case HA_READ_AFTER_KEY:
      return (cmp > 0);
    default:
      assert(false);
      return false;
  }
}

void PFS_key_object_type_enum::read(PFS_key_reader &reader,
                                    enum ha_rkey_function find_flag) {
  uchar object_type = 0;

  m_find_flag = reader.read_uint8(find_flag, m_is_null, &object_type);

  if (m_is_null) {
    m_object_type = NO_OBJECT_TYPE;
  } else {
    m_object_type = static_cast<enum_object_type>(object_type);
  }
}

bool PFS_key_object_type_enum::match(enum_object_type object_type) {
  const bool record_null = (object_type == NO_OBJECT_TYPE);
  return do_match(record_null, object_type);
}

bool PFS_key_object_type_enum::match(const PFS_prepared_stmt *pfs) {
  const bool record_null = (pfs->m_owner_object_type == NO_OBJECT_TYPE);
  return do_match(record_null, pfs->m_owner_object_type);
}

bool PFS_key_object_type_enum::match(const PFS_object_row *pfs) {
  const bool record_null = (pfs->m_object_type == NO_OBJECT_TYPE);
  return do_match(record_null, pfs->m_object_type);
}

bool PFS_key_object_type_enum::match(const PFS_program *pfs) {
  const bool record_null = (pfs->m_key.m_type == NO_OBJECT_TYPE);
  return do_match(record_null, pfs->m_key.m_type);
}

bool PFS_key_object_type_enum::do_match(bool record_null,
                                        enum_object_type record_value) {
  int cmp = 0;

  if (m_is_null) {
    cmp = (record_null ? 0 : 1);
  } else {
    if (record_null) {
      cmp = -1;
    } else if (record_value < m_object_type) {
      cmp = -1;
    } else if (record_value > m_object_type) {
      cmp = +1;
    } else {
      cmp = 0;
    }
  }

  switch (m_find_flag) {
    case HA_READ_KEY_EXACT:
      return (cmp == 0);
    case HA_READ_KEY_OR_NEXT:
      return (cmp >= 0);
    case HA_READ_KEY_OR_PREV:
      return (cmp <= 0);
    case HA_READ_BEFORE_KEY:
      return (cmp < 0);
    case HA_READ_AFTER_KEY:
      return (cmp > 0);
    default:
      assert(false);
      return false;
  }
}

bool PFS_key_object_schema::match(const PFS_table_share *share) {
  return do_match(false, share->m_key.m_schema_name.ptr(),
                  share->m_key.m_schema_name.length());
}

bool PFS_key_object_schema::match(const PFS_program *pfs) {
  return do_match(false, pfs->m_key.m_schema_name.ptr(),
                  pfs->m_key.m_schema_name.length());
}

bool PFS_key_object_schema::match(const PFS_prepared_stmt *pfs) {
  return do_match(false, pfs->m_owner_object_schema.ptr(),
                  pfs->m_owner_object_schema.length());
}

bool PFS_key_object_schema::match(const PFS_object_row *pfs) {
  const bool record_null = (pfs->m_object_name.length() == 0);
  return do_match(record_null, pfs->m_schema_name.ptr(),
                  pfs->m_schema_name.length());
}

bool PFS_key_object_schema::match(const PFS_column_row *pfs) {
  const bool record_null = (pfs->m_schema_name_length == 0);
  return do_match(record_null, pfs->m_schema_name, pfs->m_schema_name_length);
}

bool PFS_key_object_schema::match(const PFS_setup_object *pfs) {
  const bool record_null = (pfs->m_key.m_schema_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_schema_name.ptr(),
                  pfs->m_key.m_schema_name.length());
}

bool PFS_key_object_schema::match(const char *schema_name,
                                  size_t schema_name_length) {
  const bool record_null = (schema_name_length == 0);
  return do_match(record_null, schema_name, schema_name_length);
}

bool PFS_key_object_name::match(const PFS_table_share *share) {
  return do_match(false, share->m_key.m_table_name.ptr(),
                  share->m_key.m_table_name.length());
}

bool PFS_key_object_name::match(const PFS_program *pfs) {
  return do_match(false, pfs->m_key.m_object_name.ptr(),
                  pfs->m_key.m_object_name.length());
}

bool PFS_key_object_name::match(const PFS_prepared_stmt *pfs) {
  return do_match(false, pfs->m_owner_object_name.ptr(),
                  pfs->m_owner_object_name.length());
}

bool PFS_key_object_name::match(const PFS_object_row *pfs) {
  const bool record_null = (pfs->m_object_name.length() == 0);
  return do_match(record_null, pfs->m_object_name.ptr(),
                  pfs->m_object_name.length());
}

bool PFS_key_object_name::match(const PFS_column_row *pfs) {
  const bool record_null = (pfs->m_object_name_length == 0);
  return do_match(record_null, pfs->m_object_name, pfs->m_object_name_length);
}

bool PFS_key_object_name::match(const PFS_index_row *pfs) {
  const bool record_null = (pfs->m_index_name_length == 0);
  return do_match(record_null, pfs->m_index_name, pfs->m_index_name_length);
}

bool PFS_key_object_name::match(const PFS_setup_object *pfs) {
  const bool record_null = (pfs->m_key.m_object_name.length() == 0);
  return do_match(record_null, pfs->m_key.m_object_name.ptr(),
                  pfs->m_key.m_object_name.length());
}

bool PFS_key_object_name::match(const char *object_name,
                                size_t object_name_length) {
  const bool record_null = (object_name_length == 0);
  return do_match(record_null, object_name, object_name_length);
}

bool PFS_key_column_name::match(const PFS_column_row *pfs) {
  const bool record_null = (pfs->m_column_name_length == 0);
  return do_match(record_null, pfs->m_column_name, pfs->m_column_name_length);
}

bool PFS_key_object_instance::match(const PFS_table *pfs) {
  return (m_identity == pfs->m_identity);  // FIXME ?
}

bool PFS_key_object_instance::match(const PFS_mutex *pfs) {
  return (m_identity == pfs->m_identity);
}

bool PFS_key_object_instance::match(const PFS_rwlock *pfs) {
  return (m_identity == pfs->m_identity);
}

bool PFS_key_object_instance::match(const PFS_cond *pfs) {
  return (m_identity == pfs->m_identity);
}

bool PFS_key_object_instance::match(const PFS_file *pfs) {
  return (m_identity == pfs->m_identity);
}

bool PFS_key_object_instance::match(const PFS_socket *pfs) {
  return (m_identity == pfs->m_identity);
}

bool PFS_key_object_instance::match(const PFS_prepared_stmt *pfs) {
  return (m_identity == pfs->m_identity);
}

bool PFS_key_object_instance::match(const PFS_metadata_lock *pfs) {
  return (m_identity == pfs->m_identity);
}
