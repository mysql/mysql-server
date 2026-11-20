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

#include "ddl_convertor.h"
#include <algorithm>
#include <iomanip>
#include "mysql/strings/m_ctype.h"  // my_strcasecmp
#include "sql/create_field.h"       // Create_field
#include "sql/dd/types/table.h"     // dd::Table
#include "sql/duckdb/duckdb_charset_collation.h"
#include "sql/duckdb/duckdb_config.h"  // use_double_for_decimal
#include "sql/duckdb/duckdb_sql_quote.h"
#include "sql/duckdb/duckdb_table.h"
#include "sql/item.h"  // Item
#include "sql/mysqld.h"
#include "sql/sql_gipk.h"
#include "sql/sql_table.h"  // primary_key_name

/** Check if the type of column is changed.
@param[in]  new_field   new created field from Alter_info
@param[in]  field       old field
@return true if type changed */
inline static bool is_type_changed(const Create_field *new_field,
                                   const Field *field) {
  return field->is_equal(new_field) != IS_EQUAL_YES;
}

/** Check if the nullable of column is changed.
@param[in]  new_field   new created field from Alter_info
@param[in]  field       old field
@return true if nullable changed */
inline static bool is_nullable_change(const Create_field *new_field,
                                      const Field *field) {
  return ((new_field->flags & NOT_NULL_FLAG) != 0) ^
         field->is_flag_set(NOT_NULL_FLAG);
}

inline static bool is_numeric_field_type(enum_field_types t) {
  switch (t) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_YEAR:
      return true;
    default:
      return false;
  }
}

/** Check if the name of column is changed.
  @param[in]  new_field   new created field from Alter_info
  @param[in]  field       old field
  @return true if name changed */
inline static bool is_name_changed(const Create_field *new_field,
                                   const Field *field) {
  return my_strcasecmp(system_charset_info, field->field_name,
                       new_field->field_name);
}

/** Find the associated field in the new table.
  @param[in]  new_field   new created field from Alter_info
  @param[in]  new_table   new table
  @return the associated new field in new table */
inline static Field *find_field(const Create_field *new_field,
                                const TABLE *new_table) {
  Field **first_field = new_table->field;
  Field **ptr, *cur_field;
  for (ptr = first_field; (cur_field = *ptr); ptr++) {
    if (my_strcasecmp(system_charset_info, cur_field->field_name,
                      new_field->field_name) == 0)
      break;
  }
  return cur_field;
}

/** Check if the key is primary key.
@param[in]  new_key   key
@return true if primary key, false otherwise */
inline static bool is_primary_key(const KEY *key) {
  return ((key->flags & HA_NOSAME) != 0) &&
         (my_strcasecmp(system_charset_info, key->name, primary_key_name) == 0);
}

inline static bool contains_space(const char *str) {
  std::string s(str);
  return s.find(' ') != std::string::npos;
}

/** Get expression for duckdb.
  @param[in]  thd       thread handler
  @param[in]  expr      the expression
  @return expression */
inline static std::string get_expr_for_duckdb(THD *thd, Value_generator *expr) {
  char buffer[128];
  String s(buffer, sizeof(buffer), system_charset_info);
  expr->print_expr(thd, &s, true);
  std::string expr_str(s.ptr(), s.length());
  expr_str = "(" + expr_str + ")";
  return expr_str;
}

/** Get default value for the specified field.
  @param[in]  field     field
  @param[in]  dd_table   dd table object
  @return default value string */
inline static std::string get_default_value(Field *field,
                                            const dd::Table *dd_table) {
  std::string default_value;
  const dd::Column *col_obj = dd_table->get_column(field->field_name);
  assert(col_obj != nullptr);
  if (!col_obj->has_no_default()) {
    if (!col_obj->default_option().empty() &&
        col_obj->default_option().compare(0, 17, "CURRENT_TIMESTAMP") == 0) {
      default_value = col_obj->default_option().c_str();
    } else if (field->m_default_val_expr != nullptr) {
      default_value =
          get_expr_for_duckdb(current_thd, field->m_default_val_expr);
    } else if (!col_obj->is_default_value_null()) {
      std::string duckdb_type =
          FieldConvertor(field, dd_table).convert_type(field);
      if (duckdb_type == "BLOB" || duckdb_type == "BITSTRING") {
        /*
          For BINARY/VARBINARY/BIT types, default_value_utf8() is unreliable
          because it is generated using strlen() which truncates at \0 bytes.
          Read from the raw binary default_value() instead.

          For VARBINARY (MYSQL_TYPE_VARCHAR without charset), default_value()
          includes a length prefix (1 or 2 bytes depending on max field_length)
          in MySQL's internal record format. We must skip this prefix and only
          convert the actual data portion.
        */
        dd::String_type bin_val = col_obj->default_value();
        const char *data = bin_val.c_str();
        size_t data_len = bin_val.length();

        if (field->type() == MYSQL_TYPE_VARCHAR) {
          auto *varfield = down_cast<const Field_varstring *>(field);
          uint32 length_bytes = varfield->get_length_bytes();
          /* Read the actual data length from the prefix. */
          uint32 actual_len =
              length_bytes == 1 ? static_cast<uint8>(data[0]) : uint2korr(data);
          data += length_bytes;
          data_len = actual_len;
        }

        default_value = toHex(data, data_len);
      } else {
        const dd::String_type &default_value_utf8 =
            col_obj->default_value_utf8();
        default_value = myduck::quote_duckdb_string(
            default_value_utf8.data(), default_value_utf8.length());
      }
    }
  }
  return default_value;
}

bool FieldConvertor::check() {
  /* No support for INVISIBLE. */
  if (m_field->is_hidden()) {
    return myduck::report_duckdb_table_struct_error(
        "invisible column is not supported");
  }

  /* No support for non-utf8 charset. */
  if (m_field->has_charset()) {
    const CHARSET_INFO *cs = m_field->charset();
    if (strcmp(cs->csname, "utf8") && strcmp(cs->csname, "utf8mb3") &&
        strcmp(cs->csname, "utf8mb4") && strcmp(cs->csname, "ascii") &&
        strcmp(cs->csname, "latin1")) {
      return myduck::report_duckdb_table_struct_error(
          "DuckDB only supports utf8, utf8mb4, ascii, and latin1 character "
          "sets");
    }
  }

  if (!duckdb_convert_tables_with_generated_columns && m_field->is_gcol()) {
    return myduck::report_duckdb_table_struct_error(
        "DuckDB is not supported for generated columns when "
        "duckdb_convert_tables_with_generated_columns is OFF");
  }

  return false;
}

std::string FieldConvertor::translate() {
  Field *field = m_field;
  if (field->is_hidden_by_system()) return "";
  std::ostringstream result;

  result << myduck::quote_duckdb_ident(field->field_name);
  result << " ";

  result << convert_type(m_field);

  if (field->is_gcol()) {
    /*
      DuckDB only supports virtual generated columns. So we handle stored
      generated columns as virtual one.
    */
    std::string gcol_expr = get_expr_for_duckdb(current_thd, field->gcol_info);
    result << " GENERATED ALWAYS AS " + gcol_expr << " VIRTUAL";
  }
  /* Constraints on generated columns are not supported yet. */
  if (field->is_flag_set(NOT_NULL_FLAG) && !field->is_gcol())
    result << " NOT NULL";

  /* Get default value from DD. */
  std::string default_value = get_default_value(field, m_dd_table);
  if (!default_value.empty()) result << " DEFAULT " << default_value;

  return result.str();
}

std::string FieldConvertor::convert_type(const Field *field) {
  // refer: 1. static constexpr const builtin_type_array BUILTIN_TYPES
  //        2. LogicalType MySQLUtils::TypeToLogicalType
  //        3. void show_sql_type
  //        *  duckdb-mysql MySQLColumnsToSQL
  std::string ret;

  enum_field_types field_type = field->real_type();
  bool is_unsigned = field->is_unsigned();
  // Use has_charset instead of BINARY_FLAG.
  bool has_charset = field->has_charset();

  switch (field_type) {
    case MYSQL_TYPE_TINY:  // Field_tiny
      if (is_unsigned) {
        ret = "utinyint";
      } else {
        ret = "tinyint";
      }
      break;

    case MYSQL_TYPE_SHORT:
      if (is_unsigned) {
        ret = "usmallint";
      } else {
        ret = "smallint";
      }
      break;

    case MYSQL_TYPE_INT24:  // "mediumint"
    case MYSQL_TYPE_LONG:   //  "int"
      if (is_unsigned) {
        ret = "uinteger";
      } else {
        ret = "integer";
      }
      break;

    case MYSQL_TYPE_LONGLONG:  // "bigint"
      if (is_unsigned) {
        ret = "ubigint";
      } else {
        ret = "bigint";
      }
      break;

    case MYSQL_TYPE_FLOAT:
      ret = "float";
      break;
    case MYSQL_TYPE_DOUBLE:
      ret = "double";
      break;
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL: {
      // TODO: need to deal
      const Field_new_decimal *decimal_field =
          static_cast<const Field_new_decimal *>(field);
      uint precision = decimal_field->precision;
      uint dec = decimal_field->dec;
      if (precision <= 38) {
        ret = "decimal(" + std::to_string(precision) + "," +
              std::to_string(dec) + ")";
      } else if (myduck::use_double_for_decimal) {
        ret = "double";
      } else {
        /* MySQL's dec is not bigger than 30. */
        assert(dec <= 30);
        ret = "decimal(38," + std::to_string(dec) + ")";
      }
      break;
    }
    case MYSQL_TYPE_TIMESTAMP2:
      ret = "timestamptz";
      break;
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATE:
      ret = "date";
      break;
    case MYSQL_TYPE_TIME2:
      ret = "time";
      break;
    case MYSQL_TYPE_DATETIME2:
      ret = "datetime";
      break;
    case MYSQL_TYPE_YEAR:
      ret = "integer";
      break;
    case MYSQL_TYPE_BIT:
      ret = "bitstring";
      break;
    case MYSQL_TYPE_GEOMETRY:
      ret = "blob";
      break;
    case MYSQL_TYPE_NULL:
      // type_data.type_name = "null";
      // TODO: ?
      break;
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
      ret = "varchar";
      break;
    case MYSQL_TYPE_JSON:
      ret = "json";
      break;
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
      ret = has_charset ? "varchar" : "blob";
      break;
    default:
      ret = "__unknown_type";
      break;
  }

  /* Collations are not supported on generated columns. */
  if (ret == "varchar" && field->has_charset() && !field->is_gcol()) {
    std::string warn_msg;
    std::string co = myduck::get_duckdb_collation(field->charset(), warn_msg);
    ret.append(" COLLATE ").append(co);
    assert(warn_msg.empty());
  }

  std::transform(ret.begin(), ret.end(), ret.begin(), ::toupper);

  return ret;
}

static constexpr char CREATE_TABLE_STR[] = "CREATE TABLE ";
static constexpr char IF_NOT_EXISTS_STR[] = "IF NOT EXISTS ";
static constexpr char ALTER_TABLE_OP_STR[] = "ALTER TABLE ";
static constexpr char RENAME_TABLE_OP_STR[] = " RENAME TO ";
static constexpr char ALTER_COLUMN_OP_STR[] = " ALTER COLUMN ";
static constexpr char ADD_COLUMN_OP_STR[] = " ADD COLUMN ";
static constexpr char DROP_COLUMN_OP_STR[] = " DROP COLUMN ";
static constexpr char RENAME_COLUMN_OP_STR[] = " RENAME COLUMN ";
static constexpr char DEFINE_DEFAULT_STR[] = " DEFAULT ";
static constexpr char SET_DATA_TYPE_STR[] = " SET DATA TYPE ";
static constexpr char SET_DEFAULT_STR[] = " SET DEFAULT ";
static constexpr char DROP_DEFAULT_STR[] = " DROP DEFAULT";
static constexpr char SET_NOT_NULL_STR[] = " SET NOT NULL";
static constexpr char DROP_NOT_NULL_STR[] = " DROP NOT NULL";

/** Append 'ALTER TABLE ...' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name */
inline static void append_stmt_alter_table(std::ostringstream &output,
                                           const std::string &schema_name,
                                           const std::string &table_name) {
  output << "USE " << myduck::quote_duckdb_ident(schema_name) << ";";
  output << ALTER_TABLE_OP_STR << myduck::quote_duckdb_ident(table_name);
}

/** Append 'ADD COLUMN ... TYPE ... DEFAULT ...' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name
@param[in]       column_type  column type
@param[in]       has_default  whether has default value
@param[in]       default_value  default value */
inline static void append_stmt_column_add(std::ostringstream &output,
                                          const std::string &schema_name,
                                          const std::string &table_name,
                                          const std::string &column_name,
                                          const std::string &column_type,
                                          bool has_default,
                                          const std::string &default_value) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty() &&
         !column_type.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << ADD_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name) << " "
         << column_type;
  if (has_default) {
    output << DEFINE_DEFAULT_STR << default_value << "";
  }

  output << ";";
}

/** Append 'DROP COLUMN ...' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name */
inline static void append_stmt_column_drop(std::ostringstream &output,
                                           const std::string &schema_name,
                                           const std::string &table_name,
                                           const std::string &column_name) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << DROP_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name)
         << ";";
}

/** Append 'ALTER TABLE ... ALTER COLUMN ... SET DATA TYPE ...' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name
@param[in]       column_type  column type */
inline static void append_stmt_column_change_type(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &column_name,
    const std::string &column_type) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty() &&
         !column_type.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << ALTER_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name)
         << SET_DATA_TYPE_STR << column_type << ";";
}

/** Append 'ALTER TABLE ... RENAME COLUMN ... TO ...' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name
@param[in]       column_type  column type
@param[in]       has_default  whether has default value
@param[in]       default_value  default value */
inline static void append_stmt_column_rename(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &old_column_name,
    const std::string &new_column_name) {
  assert(!schema_name.empty() && !table_name.empty() &&
         !old_column_name.empty() && !new_column_name.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << RENAME_COLUMN_OP_STR << myduck::quote_duckdb_ident(old_column_name)
         << " TO " << myduck::quote_duckdb_ident(new_column_name) << ";";
}

/** Append 'ALTER TABLE ... ALTER COLUMN ... SET DEFAULT ...' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name
@param[in]       default_value  default value */
inline static void append_stmt_column_set_default(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &column_name,
    const std::string &default_value) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty() &&
         !default_value.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << ALTER_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name)
         << SET_DEFAULT_STR << default_value << ";";
}

/** Append 'ALTER TABLE ... ALTER COLUMN ... DROP DEFAULT' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name */
inline static void append_stmt_column_drop_default(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &column_name) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << ALTER_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name)
         << DROP_DEFAULT_STR << ";";
}

/** Append 'ALTER TABLE ... ALTER COLUMN ... SET NOT NULL' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name */
inline static void append_stmt_column_set_not_null(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &column_name) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << ALTER_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name)
         << SET_NOT_NULL_STR << ";";
}

/** Append 'ALTER TABLE ... ALTER COLUMN ... DROP NOT NULL' to statement.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name */
inline static void append_stmt_column_drop_not_null(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &column_name) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty());
  append_stmt_alter_table(output, schema_name, table_name);
  output << ALTER_COLUMN_OP_STR << myduck::quote_duckdb_ident(column_name)
         << DROP_NOT_NULL_STR << ";";
}

/** Append 'UPDATE t SET c = v WHERE c IS NULL;' to statement. Used to
back-fill NULL rows before SET NOT NULL on MODIFY/CHANGE COLUMN; DuckDB's
SET DEFAULT does not touch existing rows.
@param[in, out]  output       statement stream
@param[in]       schema_name  schema name
@param[in]       table_name   table name
@param[in]       column_name  column name
@param[in]       fill_value   value to write in place of NULL */
inline static void append_stmt_update_null_with_default(
    std::ostringstream &output, const std::string &schema_name,
    const std::string &table_name, const std::string &column_name,
    const std::string &fill_value) {
  assert(!schema_name.empty() && !table_name.empty() && !column_name.empty() &&
         !fill_value.empty());
  output << "USE " << myduck::quote_duckdb_ident(schema_name) << ";";
  output << "UPDATE " << myduck::quote_duckdb_ident(table_name) << " SET "
         << myduck::quote_duckdb_ident(column_name) << " = " << fill_value
         << " WHERE " << myduck::quote_duckdb_ident(column_name) << " IS NULL;";
}

/** Append 'ALTER TABLE ... RENAME TO ...' to statement.
@param[in, out]  output       statement stream
@param[in]       old_schema_name  old schema name
@param[in]       old_table_name   old table name
@param[in]       new_schema_name  new schema name
@param[in]       new_table_name   new table name */
inline static void append_stmt_table_rename(std::ostringstream &output,
                                            const std::string &old_schema_name,
                                            const std::string &old_table_name,
                                            const std::string &new_schema_name
                                            [[maybe_unused]],
                                            const std::string &new_table_name) {
  assert(!old_schema_name.empty() && !old_table_name.empty() &&
         !new_schema_name.empty() && !new_table_name.empty());
  assert(old_schema_name == new_schema_name);
  append_stmt_alter_table(output, old_schema_name, old_table_name);
  output << RENAME_TABLE_OP_STR << myduck::quote_duckdb_ident(new_table_name)
         << ";";
}

bool CreateTableConvertor::check() {
  /* Check columns. */
  Field **first_field = m_table->field;
  Field **ptr, *field;

  for (ptr = first_field; (field = *ptr); ptr++) {
    if (FieldConvertor(field, m_dd_table).check()) {
      return true;
    }
  }

  /* Check PK. */
  TABLE_SHARE *share = m_table->s;
  KEY *key_info = m_table->key_info;

  /* If duckdb_require_primary_key is OFF, table can be created without PK. */
  if (share->keys == 0) {
    return false;
  }

  /* By now, we have one and only one primary key. */
  assert(share->keys == 1 && is_primary_key(key_info));

  /*
    There are currently no indexes in DuckDB, we don't care whether they are
    prefix/partial indexes or not.
  */

  // TODO: field number limit, reserv name,

  return false;
}

std::string CreateTableConvertor::translate() {
  std::ostringstream result;
  assert((m_create_info->options & HA_LEX_CREATE_TMP_TABLE) == 0);

  result << "USE " << myduck::quote_duckdb_ident(m_schema_name) << ";";

  result << CREATE_TABLE_STR;
  if (m_create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS) {
    result << IF_NOT_EXISTS_STR;
  }
  result << myduck::quote_duckdb_ident(m_table_name);
  result << " (";

  append_column_definition(result);
  result << ");";

  return result.str();
}

void CreateTableConvertor::append_column_definition(
    std::ostringstream &output) {
  Field *field = nullptr;
  Field **first_field = m_table->field;
  for (Field **ptr = first_field; (field = *ptr); ptr++) {
    if (ptr != first_field) {
      output << ",";
    }
    output << FieldConvertor(field, m_dd_table).translate();
  }
}

bool RenameTableConvertor::check() {
  if (m_new_schema_name != m_schema_name) {
    return myduck::report_duckdb_table_struct_error(
        "DuckDB does not support rename between different schema");
  }

  /*
    There are currently no indexes or constraints in DuckDB, and we no longer
    need to check for dependencies between indexes and constraints.
  */

  return false;
}

std::string RenameTableConvertor::translate() {
  std::ostringstream result;
  append_stmt_table_rename(result, m_schema_name, m_table_name,
                           m_new_schema_name, m_new_table_name);
  return result.str();
}

/** Get MySQL's implicit default value for a field, translated to DuckDB syntax.
  MySQL fills existing rows with the type's implicit default when adding a
  NOT NULL column without an explicit DEFAULT. DuckDB has no such concept,
  so we must supply it explicitly.
  @param[in]  field         the field
  @param[in]  is_add_column true for ADD COLUMN, false for MODIFY/CHANGE.
              ENUM differs: ADD fills the first enum value, MODIFY fills ''
              (enum index 0 sentinel) to match InnoDB behaviour.
  @return default value string suitable for DuckDB SQL */
inline static std::string get_implicit_default_for_duckdb(
    const Field *field, bool is_add_column = true) {
  enum_field_types field_type = field->real_type();

  switch (field_type) {
    /* Numeric types: MySQL implicit default is 0. */
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_BIT:
      return "0";

    /*
      TIME's MySQL implicit default '00:00:00' is a legal DuckDB value,
      so it goes through the normal path.
    */
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      return "'00:00:00'";

    /*
      DATE / DATETIME / TIMESTAMP: zero-date '0000-00-00[ 00:00:00]'.
      Adding DATE / DATETIME NOT NULL without DEFAULT uses COPY so that
      the SQL layer checks sql_mode and fills the implicit date values.
    */
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      return "'0000-00-00'";

    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      return "'0000-00-00 00:00:00'";

    /* JSON null value. */
    case MYSQL_TYPE_JSON:
      return "'null'";

    case MYSQL_TYPE_SET:
      return "''";

    case MYSQL_TYPE_ENUM: {
      if (!is_add_column) return "''";
      auto *f = down_cast<const Field_enum *>(field);
      if (f->typelib && f->typelib->count > 0) {
        return myduck::quote_duckdb_string(f->typelib->type_names[0],
                                           f->typelib->type_lengths[0]);
      }
      return "''";
    }

    /*
      BINARY(N): fixed-length binary, InnoDB fills N zero bytes.
      CHAR(N): fixed-length text, InnoDB fills empty string.
    */
    case MYSQL_TYPE_STRING:
      if (!field->has_charset()) {
        uint32 len = field->field_length;
        std::string hex = "'";
        for (uint32 i = 0; i < len; i++) hex += "\\x00";
        hex += "'::BLOB";
        return hex;
      }
      return "''";

    /*
      Variable-length string/BLOB types: text columns get empty string,
      binary columns (no charset) need the DuckDB ''::BLOB cast.
    */
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
      return field->has_charset() ? "''" : "''::BLOB";

    /* GEOMETRY has not been supported yet. */
    case MYSQL_TYPE_GEOMETRY:
      assert(false);

    default:
      return "NULL";
  }
}

void AddColumnConvertor::prepare_columns() {
  List_iterator<Create_field> new_field_it(m_alter_info->create_list);
  Create_field *new_field;

  while ((new_field = new_field_it++)) {
    if (new_field->field != nullptr) {
      continue;
    }

    Field *field = find_field(new_field, m_new_table);

    m_columns_to_add.emplace_back(new_field, field);

    if ((new_field->flags & NOT_NULL_FLAG) != 0) {
      m_columns_to_set_not_null.emplace_back(new_field, field);
    }
  }
}

bool AddColumnConvertor::check() {
  for (auto &pair : m_columns_to_add) {
    Field *field = pair.second;

    if (field->is_gcol()) {
      return myduck::report_duckdb_table_struct_error(
          "Adding generated columns after table creation is not supported");
    }

    assert(!field->is_flag_set(AUTO_INCREMENT_FLAG));

    if (FieldConvertor(field, m_new_dd_table).check()) {
      return true;
    }
  }

  return false;
}

std::string AddColumnConvertor::translate() {
  std::ostringstream result;

  for (auto &pair : m_columns_to_add) {
    Create_field *new_field = pair.first;
    Field *field = pair.second;
    assert(field != nullptr);

    std::string type = FieldConvertor::convert_type(field);

    bool has_default = (new_field->constant_default != nullptr) ||
                       (new_field->auto_flags & Field::DEFAULT_NOW) ||
                       (new_field->m_default_val_expr != nullptr);

    /* Set default value. */
    std::string default_value = get_default_value(field, m_new_dd_table);
    default_value = default_value.empty() ? "NULL" : default_value;

    /*
      MySQL fills existing rows with the type's implicit default (0,
      '', etc.) when adding a NOT NULL column without DEFAULT. DuckDB
      fills with NULL, so SET NOT NULL would fail. Inject the default
      to back-fill existing rows, then drop it below so the resulting
      column metadata matches InnoDB (no DEFAULT). Future INSERTs are
      handled by MySQL Server's strict / non-strict implicit-default
      logic before reaching DuckDB, so the column itself does not need
      a DEFAULT to stay aligned with InnoDB semantics.
    */
    bool injected_default = false;
    if (!has_default && (new_field->flags & NOT_NULL_FLAG) != 0) {
      default_value = get_implicit_default_for_duckdb(field);
      injected_default = true;
    }

    append_stmt_column_add(result, m_schema_name, m_table_name,
                           new_field->field_name, type,
                           has_default || injected_default, default_value);

    if (injected_default) {
      append_stmt_column_drop_default(result, m_schema_name, m_table_name,
                                      new_field->field_name);
    }
  }

  for (auto &pair : m_columns_to_set_not_null) {
    Create_field *new_field = pair.first;
    assert((new_field->flags & NOT_NULL_FLAG) != 0);
    append_stmt_column_set_not_null(result, m_schema_name, m_table_name,
                                    new_field->field_name);
  }

  return result.str();
}

void DropColumnConvertor::prepare_columns() {
  Field **first_field = m_old_table->field;
  Field **ptr, *field;

  for (ptr = first_field; (field = *ptr); ptr++) {
    if (!field->is_flag_set(FIELD_IS_DROPPED)) {
      continue;
    }

    m_columns_to_drop.emplace_back(nullptr, field);
  }

  /*
    Drop generated columns before the physical columns they depend on,
    otherwise DuckDB rejects the DROP because the expression still
    references the base column.
  */
  std::stable_sort(m_columns_to_drop.begin(), m_columns_to_drop.end(),
                   [](const Column &a, const Column &b) {
                     return a.second->is_gcol() && !b.second->is_gcol();
                   });
}

bool DropColumnConvertor::check() {
  /*
    There are currently no indexes or constraints in DuckDB, and we no longer
    need to check for dependencies between indexes and constraints.
  */

  return false;
}

std::string DropColumnConvertor::translate() {
  std::ostringstream result;

  for (auto &pair : m_columns_to_drop) {
    Field *field = pair.second;
    append_stmt_column_drop(result, m_schema_name, m_table_name,
                            field->field_name);
  }

  return result.str();
}

void ChangeColumnDefaultConvertor::prepare_columns() {
  assert(m_alter_info != nullptr);

  List_iterator<Create_field> new_field_it(m_alter_info->create_list);
  Create_field *new_field;

  while ((new_field = new_field_it++)) {
    Field *cur_field = find_field(new_field, m_new_table);

    bool set_default = (new_field->constant_default != nullptr) ||
                       (new_field->auto_flags & Field::DEFAULT_NOW) ||
                       (new_field->m_default_val_expr != nullptr);
    bool drop_default = ((new_field->flags & NO_DEFAULT_VALUE_FLAG) != 0);

    if (drop_default) {
      m_columns_to_drop_default.emplace_back(new_field, cur_field);
    }

    if (set_default) {
      m_columns_to_set_default.emplace_back(new_field, cur_field);
    }
  }
}

std::string ChangeColumnDefaultConvertor::translate() {
  std::ostringstream result;

  /* Drop default value. */
  for (auto &pair : m_columns_to_drop_default) {
    Create_field *new_field = pair.first;
    assert((new_field->flags & NO_DEFAULT_VALUE_FLAG) != 0);
    append_stmt_column_drop_default(result, m_schema_name, m_table_name,
                                    new_field->field_name);
  }

  /* Set default value. */
  for (auto &pair : m_columns_to_set_default) {
    Create_field *new_field = pair.first;
    Field *field = pair.second;

    std::string default_value = get_default_value(field, m_new_dd_table);
    default_value = default_value.empty() ? "NULL" : default_value;

    append_stmt_column_set_default(result, m_schema_name, m_table_name,
                                   new_field->field_name, default_value);
  }

  return result.str();
}

void ChangeColumnConvertor::prepare_columns() {
  List_iterator<Create_field> new_field_it(m_alter_info->create_list);
  Create_field *new_field;
  Field *field;

  while ((new_field = new_field_it++)) {
    if (new_field->change == nullptr) {
      continue;
    }
    field = new_field->field;
    Field *cur_field = find_field(new_field, m_new_table);

    bool type_changed = is_type_changed(new_field, field);
    bool nullable_changed = is_nullable_change(new_field, field);
    bool name_changed = is_name_changed(new_field, field);

    /* Change type. */
    if (type_changed) {
      m_columns_to_change_type.emplace_back(new_field, cur_field);
    }

    /* Change nullable. */
    if (nullable_changed) {
      if ((new_field->flags & NOT_NULL_FLAG) != 0) {
        m_columns_to_set_not_null.emplace_back(new_field, cur_field);
      } else {
        m_columns_to_drop_not_null.emplace_back(new_field, cur_field);
      }
    }

    /* Change name. */
    if (name_changed) {
      assert(field->is_flag_set(FIELD_IS_RENAMED));
      m_columns_to_rename.emplace_back(new_field, cur_field);
    }

    /* All columns will be saved here. */
    m_columns.emplace_back(new_field, cur_field);
  }
}

bool ChangeColumnConvertor::check() {
  for (auto &pair : m_columns) {
    Field *new_field = pair.second;

    if (new_field->is_gcol()) {
      return myduck::report_duckdb_table_struct_error(
          "Modifying generated columns is not supported");
    }

    if (FieldConvertor(new_field, m_new_dd_table).check()) {
      return true;
    }
  }

  for ([[maybe_unused]] auto &pair : m_columns_to_change_type) {
    assert(pair.second != nullptr);
    assert(pair.second->part_of_prefixkey.bits_set() == 0);
    /*
      There are currently no indexes or constraints in DuckDB, and we no longer
      need to check for dependencies between indexes and constraints.
    */
  }

  /* Rename check key words */

  return false;
}

std::string ChangeColumnConvertor::translate() {
  std::ostringstream result;

  /* Rename column.*/
  for (auto &pair : m_columns_to_rename) {
    Create_field *new_field = pair.first;
    Field *old_field = new_field->field;
    assert(old_field->is_flag_set(FIELD_IS_RENAMED));
    append_stmt_column_rename(result, m_schema_name, m_table_name,
                              old_field->field_name, new_field->field_name);
  }

  /* Change type. */
  for (auto &pair : m_columns_to_change_type) {
    Field *field = pair.second;
    std::string new_type = FieldConvertor::convert_type(field);
    append_stmt_column_change_type(result, m_schema_name, m_table_name,
                                   field->field_name, new_type);
    /* DuckDB support using clause when alter column type but MySQL
    does not support it, so ignore it now. */
  }

  /*
    Numeric → ENUM data mapping. After SET DATA TYPE VARCHAR, integer
    values become string representations ('1','2',...). InnoDB maps by
    ENUM index (1→first name, 2→second name), so we emit an UPDATE
    with CASE to replicate that mapping. Out-of-range indices map to
    '' (ENUM index 0 sentinel), matching InnoDB.
  */
  for (auto &pair : m_columns_to_change_type) {
    Create_field *new_field = pair.first;
    Field *new_field_obj = pair.second;
    Field *old_field = new_field->field;

    if (new_field_obj->real_type() == MYSQL_TYPE_ENUM &&
        is_numeric_field_type(old_field->real_type())) {
      auto *ef = down_cast<const Field_enum *>(new_field_obj);
      if (ef->typelib && ef->typelib->count > 0) {
        std::string q_column =
            myduck::quote_duckdb_ident(new_field->field_name);
        std::ostringstream case_expr;
        case_expr << "CASE " << q_column;
        for (uint i = 0; i < ef->typelib->count; i++) {
          case_expr << " WHEN '" << (i + 1) << "' THEN "
                    << myduck::quote_duckdb_string(
                           ef->typelib->type_names[i],
                           ef->typelib->type_lengths[i]);
        }
        case_expr << " ELSE '' END";
        result << "USE " << myduck::quote_duckdb_ident(m_schema_name) << ";";
        result << "UPDATE " << myduck::quote_duckdb_ident(m_table_name)
               << " SET " << q_column << " = " << case_expr.str() << " WHERE "
               << q_column << " IS NOT NULL;";
      }
    }
  }

  /* Change default value. All columns should be processed. */
  for (auto &pair : m_columns) {
    Create_field *new_field = pair.first;
    Field *field = pair.second;

    /* Skip default value set/drop for generated column. */
    if (new_field->field && new_field->field->is_gcol()) {
      continue;
    }

    bool drop_default = ((new_field->flags & NO_DEFAULT_VALUE_FLAG) != 0);

    /* Drop default value. */
    if (drop_default) {
      append_stmt_column_drop_default(result, m_schema_name, m_table_name,
                                      new_field->field_name);
    }

    /* Set default value. */
    std::string default_value = get_default_value(field, m_new_dd_table);
    default_value = default_value.empty() ? "NULL" : default_value;

    append_stmt_column_set_default(result, m_schema_name, m_table_name,
                                   new_field->field_name, default_value);
  }

  for (auto &pair : m_columns_to_drop_not_null) {
    Create_field *new_field = pair.first;
    append_stmt_column_drop_not_null(result, m_schema_name, m_table_name,
                                     new_field->field_name);
  }

  for (auto &pair : m_columns_to_set_not_null) {
    Create_field *new_field = pair.first;
    Field *field = pair.second;
    assert((new_field->flags & NOT_NULL_FLAG) != 0);

    /*
      MySQL non-strict mode fills existing NULL rows with the type's
      implicit default before SET NOT NULL. DuckDB's SET DEFAULT
      doesn't touch existing rows, so emit an UPDATE first.
      Strict mode rejects in InnoDB (ER_INVALID_USE_OF_NULL); skip
      the UPDATE so DuckDB also rejects (Constraint Error at SET
      NOT NULL), keeping the two engines aligned. Also skip when
      the helper returns "NULL" (unsupported type): falls back to
      the pre-fix Constraint Error.
    */
    std::string fill_value = get_implicit_default_for_duckdb(field, false);
    if (!current_thd->is_strict_mode() && fill_value != "NULL") {
      append_stmt_update_null_with_default(result, m_schema_name, m_table_name,
                                           new_field->field_name, fill_value);
    }
    append_stmt_column_set_not_null(result, m_schema_name, m_table_name,
                                    new_field->field_name);
  }

  return result.str();
}

std::string ChangeColumnForPrimaryKeyConvertor::translate() {
  std::ostringstream result;
  for (auto field : m_columns_to_set_not_null) {
    assert(field->is_flag_set(PRI_KEY_FLAG));
    assert(field->is_flag_set(NOT_NULL_FLAG));
    append_stmt_column_set_not_null(result, m_schema_name, m_table_name,
                                    field->field_name);
  }

  return result.str();
}

void ChangeColumnForPrimaryKeyConvertor::prepare_columns() {
  Field **first_field = m_new_table->field;
  Field **ptr, *cur_field;
  for (ptr = first_field; (cur_field = *ptr); ptr++) {
    if (!cur_field->is_flag_set(PRI_KEY_FLAG)) {
      continue;
    }
    if (!cur_field->is_flag_set(NOT_NULL_FLAG)) {
      continue;
    }

    m_columns_to_set_not_null.push_back(cur_field);
  }
}

bool DropPartitionConvertor::check() {
  assert(m_part_info != nullptr);
  assert(m_part_info->part_type != partition_type::HASH);
  return myduck::generate_delete_from_partition(m_part_info, false, m_query);
}

std::string DropPartitionConvertor::translate() { return m_query; }

std::string toHex(const char *data, size_t length) {
  std::stringstream ss;
  ss << "x'";
  ss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < length; ++i) {
    ss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(data[i]));
  }
  ss << "'::BLOB";
  return ss.str();
}
