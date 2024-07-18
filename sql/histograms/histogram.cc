/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file sql/histograms/histogram.cc
  Histogram base class (implementation).
*/

#include "sql/histograms/histogram.h"  // Histogram, Histogram_comparator

#include <sys/types.h>
#include <algorithm>
#include <map>
#include <memory>  // std::unique_ptr
#include <new>
#include <random>
#include <string>
#include <vector>

#include "base64.h"       // base64_*
#include "decimal.h"      // *2decimal
#include "field_types.h"  // enum_field_types
#include "lex_string.h"
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"  // my_micro_time, get_charset
#include "my_systime.h"
#include "my_time.h"
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "scope_guard.h"          // create_scope_guard
#include "sql-common/json_dom.h"  // Json_*
#include "sql-common/my_decimal.h"
#include "sql/auth/auth_common.h"
#include "sql/create_field.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/dd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"
#include "sql/dd/types/column_statistics.h"
#include "sql/dd/types/table.h"  // dd::Table
#include "sql/debug_sync.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/handler.h"
#include "sql/histograms/equi_height.h"       // Equi_height<T>
#include "sql/histograms/singleton.h"         // Singleton<T>
#include "sql/histograms/table_histograms.h"  // Table_histograms
#include "sql/histograms/value_map.h"         // Value_map
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_json_func.h"  // parse_json
#include "sql/key.h"
#include "sql/mdl.h"             // MDL_request
#include "sql/mem_root_array.h"  // Mem_root_array
#include "sql/mysqld.h"          // read_only
#include "sql/psi_memory_key.h"  // key_memory_histograms
#include "sql/sql_base.h"        // open_and_lock_tables, close_thread_tables
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"  // make_lex_string_root
#include "sql/sql_const.h"
#include "sql/sql_error.h"  // Diagnostics_area
#include "sql/sql_lex.h"    // lex_start
#include "sql/sql_time.h"   // str_to_time
#include "sql/strfunc.h"    // find_type2, find_set
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"  // trans_commit_stmt, trans_rollback_stmt
#include "sql/tztime.h"       // my_tz_UTC
#include "sql_string.h"       // String
#include "template_utils.h"

struct TYPELIB;

namespace histograms {

// Same as MAX_NUMBER_OF_HISTOGRAM_BUCKETS defined in sql_yacc.yy
static constexpr int MAX_NUMBER_OF_HISTOGRAM_BUCKETS = 1024;

/*
  This type represents a instrumented map of value maps, indexed by field
  number.
  TODO: convert to simple datatype since dynamic containers should not be
  used for fixed collection.
*/
using value_map_collection = std::map<
    uint16, std::unique_ptr<histograms::Value_map_base>, std::less<uint16>,
    Histogram_key_allocator<
        std::pair<const uint16, std::unique_ptr<histograms::Value_map_base>>>>;

static std::map<const Value_map_type, const std::string> value_map_type_to_str =
    {{Value_map_type::DATETIME, "datetime"}, {Value_map_type::DATE, "date"},
     {Value_map_type::TIME, "time"},         {Value_map_type::INT, "int"},
     {Value_map_type::UINT, "uint"},         {Value_map_type::DOUBLE, "double"},
     {Value_map_type::DECIMAL, "decimal"},   {Value_map_type::STRING, "string"},
     {Value_map_type::ENUM, "enum"},         {Value_map_type::SET, "set"}};

void *Histogram_psi_key_alloc::operator()(size_t s) const {
  return my_malloc(key_memory_histograms, s, MYF(MY_WME | ME_FATALERROR));
}

/**
  Convert from enum_field_types to Value_map_type.

  @param field_type the field type
  @param is_unsigned whether the field type is unsigned or not. This is only
                     considered if the field type is LONGLONG

  @return A Value_map_type. May be INVALID if the Value_map does not support
          the field type.
*/
static Value_map_type field_type_to_value_map_type(
    const enum_field_types field_type, const bool is_unsigned) {
  switch (field_type) {
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      return Value_map_type::DECIMAL;
    case MYSQL_TYPE_BOOL:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_BIT:
      return Value_map_type::INT;
    case MYSQL_TYPE_ENUM:
      return Value_map_type::ENUM;
    case MYSQL_TYPE_SET:
      return Value_map_type::SET;
    case MYSQL_TYPE_LONGLONG:
      return is_unsigned ? Value_map_type::UINT : Value_map_type::INT;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      return Value_map_type::DOUBLE;
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      return Value_map_type::TIME;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      return Value_map_type::DATE;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DATETIME2:
      return Value_map_type::DATETIME;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
      return Value_map_type::STRING;
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_VECTOR:
    case MYSQL_TYPE_INVALID:
    default:
      return Value_map_type::INVALID;
  }

  // All cases should be handled, so this should not be hit.
  /* purecov: begin inspected */
  assert(false);
  return Value_map_type::INVALID;
  /* purecov: end */
}

/**
  Get the Value_map_type from a Field object.

  This effectively looks at the real_type() of a Field, and converts this to
  a Value_map_type

  @param field The field to convert from

  @return A Value_map_type. May be INVALID if the Value_map does not support
          the field type.
*/
static Value_map_type field_type_to_value_map_type(const Field *field) {
  bool is_unsigned = false;
  if (field->real_type() == MYSQL_TYPE_LONGLONG) {
    /*
      For most integer types, the Value_map_type will be INT (int64). This type
      will not cover the entire value range for the SQL data type UNSIGNED
      BIGINT, so we need to distinguish between SIGNED BIGINT and UNSIGNED
      BIGINT so that we can switch the Value_map_type to UINT (uint64).
    */
    is_unsigned = field->is_unsigned();
  }

  return field_type_to_value_map_type(field->real_type(), is_unsigned);
}

void Error_context::report_global(Message err_code) {
  assert(err_code == Message::JSON_NUM_BUCKETS_MORE_THAN_SPECIFIED ||
         err_code == Message::JSON_IMPOSSIBLE_EMPTY_EQUI_HEIGHT ||
         err_code == Message::JSON_INVALID_NULL_VALUES_FRACTION ||
         err_code == Message::JSON_INVALID_TOTAL_FREQUENCY);
  if (m_results == nullptr) return;
  Json_path path(1);
  String str;
  path.to_string(&str);
  m_results->emplace(to_string(str), err_code);
}

void Error_context::report_missing_attribute(const std::string &name) {
  // In histogram json, attributes are always top-level.
  if (m_results == nullptr) return;
  Json_path path(1);
  Json_path_leg leg(name);
  path.append(leg);

  String str;
  path.to_string(&str);
  m_results->emplace(to_string(str), Message::JSON_MISSING_ATTRIBUTE);
}

void Error_context::report_node(const Json_dom *dom, Message err_code) {
  assert(!(err_code == Message::JSON_INVALID_NULL_VALUES_FRACTION ||
           err_code == Message::JSON_INVALID_TOTAL_FREQUENCY ||
           err_code == Message::JSON_NUM_BUCKETS_MORE_THAN_SPECIFIED ||
           err_code == Message::JSON_IMPOSSIBLE_EMPTY_EQUI_HEIGHT ||
           err_code == Message::JSON_MISSING_ATTRIBUTE));
  if (m_results == nullptr) return;
  String str;
  dom->get_location().to_string(&str);
  m_results->emplace(to_string(str), err_code);
}

/**
  Helper function for check_value().

  It uses Field::store() on the actual Field that the histogram belongs to in
  order to test if the value is in the field definition domain.
*/
static type_conversion_status check_value_aux(Field *field, const double *nr) {
  return field->store(*nr);
}

static type_conversion_status check_value_aux(Field *field, const String *str) {
  return field->store(str->ptr(), str->length(), str->charset());
}

static type_conversion_status check_value_aux(Field *field,
                                              const longlong *nr) {
  return field->store(*nr, false);
}

static type_conversion_status check_value_aux(Field *field,
                                              const ulonglong *nr) {
  return field->store(*nr, true);
}

// Field::store_time() should be updated to use a const pointer. We assume that
// the input value is not modified.
static type_conversion_status check_value_aux(Field *field, MYSQL_TIME *ltime) {
  return field->store_time(ltime);
}

static type_conversion_status check_value_aux(Field *field,
                                              const my_decimal *mdec) {
  return field->store_decimal(mdec);
}

template <typename T>
bool Error_context::check_value(T *v) {
  if (m_thd == nullptr || m_field == nullptr) return false;
  return (check_value_aux(m_field, v) != type_conversion_status::TYPE_OK) ||
         m_thd->is_error();
}

// Explicit template instantiations.
template bool Error_context::check_value(double *);
template bool Error_context::check_value(String *);
template bool Error_context::check_value(longlong *);
template bool Error_context::check_value(ulonglong *);
template bool Error_context::check_value(MYSQL_TIME *);
template bool Error_context::check_value(my_decimal *);

/**
  Lock a column statistic MDL key for writing (exclusive lock).

  @param thd thread handle
  @param mdl_key the MDL key to lock

  @return true on error, false on success
*/
static bool lock_for_write(THD *thd, const MDL_key &mdl_key) {
  DBUG_EXECUTE_IF("histogram_fail_during_lock_for_write", { return true; });

  MDL_request mdl_request;
  MDL_REQUEST_INIT_BY_KEY(&mdl_request, &mdl_key, MDL_EXCLUSIVE,
                          MDL_TRANSACTION);

  // If locking fails, an error has already been flagged.
  return thd->mdl_context.acquire_lock(&mdl_request,
                                       thd->variables.lock_wait_timeout);
}

Histogram::Histogram(MEM_ROOT *mem_root, const std::string &db_name,
                     const std::string &tbl_name, const std::string &col_name,
                     enum_histogram_type type, Value_map_type data_type,
                     bool *error)
    : m_null_values_fraction(INVALID_NULL_VALUES_FRACTION),
      m_charset(nullptr),
      m_num_buckets_specified(0),
      m_mem_root(mem_root),
      m_hist_type(type),
      m_data_type(data_type),
      m_auto_update(false) {
  if (lex_string_strmake(m_mem_root, &m_database_name, db_name.c_str(),
                         db_name.length()) ||
      lex_string_strmake(m_mem_root, &m_table_name, tbl_name.c_str(),
                         tbl_name.length()) ||
      lex_string_strmake(m_mem_root, &m_column_name, col_name.c_str(),
                         col_name.length())) {
    *error = true;
  }
}

Histogram::Histogram(MEM_ROOT *mem_root, const Histogram &other, bool *error)
    : m_sampling_rate(other.m_sampling_rate),
      m_null_values_fraction(other.m_null_values_fraction),
      m_charset(other.m_charset),
      m_num_buckets_specified(other.m_num_buckets_specified),
      m_mem_root(mem_root),
      m_hist_type(other.m_hist_type),
      m_data_type(other.m_data_type),
      m_auto_update(other.m_auto_update) {
  if (lex_string_strmake(m_mem_root, &m_database_name,
                         other.m_database_name.str,
                         other.m_database_name.length) ||
      lex_string_strmake(m_mem_root, &m_table_name, other.m_table_name.str,
                         other.m_table_name.length) ||
      lex_string_strmake(m_mem_root, &m_column_name, other.m_column_name.str,
                         other.m_column_name.length)) {
    *error = true;
  }
}

bool Histogram::histogram_to_json(Json_object *json_object) const {
  // Get the current time in GMT timezone with microsecond accuracy.
  my_timeval time_value;
  my_micro_time_to_timeval(my_micro_time(), &time_value);

  MYSQL_TIME current_time;
  my_tz_UTC->gmt_sec_to_TIME(&current_time, time_value);

  // last-updated
  const Json_datetime last_updated(current_time, MYSQL_TYPE_DATETIME);
  if (json_object->add_clone(last_updated_str(), &last_updated))
    return true; /* purecov: inspected */

  // histogram-type
  const Json_string histogram_type(histogram_type_to_str());
  if (json_object->add_clone(histogram_type_str(), &histogram_type))
    return true; /* purecov: inspected */

  // Sampling rate
  assert(get_sampling_rate() >= 0.0);
  assert(get_sampling_rate() <= 1.0);
  const Json_double sampling_rate(get_sampling_rate());
  if (json_object->add_clone(sampling_rate_str(), &sampling_rate))
    return true; /* purecov: inspected */

  // The number of buckets specified in the ANALYZE TABLE command
  const Json_int num_buckets_specified(get_num_buckets_specified());
  if (json_object->add_clone(numer_of_buckets_specified_str(),
                             &num_buckets_specified))
    return true; /* purecov: inspected */

  // Fraction of NULL values.
  assert(get_null_values_fraction() >= 0.0);
  assert(get_null_values_fraction() <= 1.0);
  const Json_double null_values(get_null_values_fraction());
  if (json_object->add_clone(null_values_str(), &null_values))
    return true; /* purecov: inspected */

  // charset-id
  const Json_uint charset_id(get_character_set()->number);
  if (json_object->add_clone(collation_id_str(), &charset_id))
    return true; /* purecov: inspected */

  // auto-update
  const Json_boolean auto_update(get_auto_update());
  if (json_object->add_clone(auto_update_str(), &auto_update)) return true;

  return false;
}

double Histogram::get_null_values_fraction() const {
  if (m_null_values_fraction != INVALID_NULL_VALUES_FRACTION) {
    assert(m_null_values_fraction >= 0.0);
    assert(m_null_values_fraction <= 1.0);
  }

  return m_null_values_fraction;
}

template <class T>
Histogram *build_histogram(MEM_ROOT *mem_root, const Value_map<T> &value_map,
                           size_t num_buckets, const std::string &db_name,
                           const std::string &tbl_name,
                           const std::string &col_name) {
  Histogram *histogram = nullptr;

  /*
    If the number of buckets specified is greater or equal to the number
    of distinct values, we create a Singleton histogram. Otherwise we create
    an equi-height histogram.
  */
  if (num_buckets >= value_map.size()) {
    Singleton<T> *singleton = Singleton<T>::create(
        mem_root, db_name, tbl_name, col_name, value_map.get_data_type());

    if (singleton == nullptr) return nullptr;

    if (singleton->build_histogram(value_map, num_buckets))
      return nullptr; /* purecov: inspected */

    histogram = singleton;
  } else {
    Equi_height<T> *equi_height = Equi_height<T>::create(
        mem_root, db_name, tbl_name, col_name, value_map.get_data_type());

    if (equi_height == nullptr) return nullptr;

    if (equi_height->build_histogram(value_map, num_buckets))
      return nullptr; /* purecov: inspected */

    histogram = equi_height;
  }

  // We should not have a nullptr at this point.
  assert(histogram != nullptr);

  // Verify that the original number of buckets specified is set.
  assert(histogram->get_num_buckets_specified() == num_buckets);

  // Verify that we haven't created more buckets than requested.
  assert(histogram->get_num_buckets() <= num_buckets);

  // Ensure that the character set is set.
  assert(histogram->get_character_set() != nullptr);

  // Check that the fraction of NULL values has been set properly.
  assert(histogram->get_null_values_fraction() >= 0.0);
  assert(histogram->get_null_values_fraction() <= 1.0);

  return histogram;
}

Histogram *Histogram::json_to_histogram(MEM_ROOT *mem_root,
                                        const std::string &schema_name,
                                        const std::string &table_name,
                                        const std::string &column_name,
                                        const Json_object &json_object,
                                        Error_context *context) {
  // Histogram type (equi-height or singleton).
  const Json_dom *histogram_type_dom =
      json_object.get(Histogram::histogram_type_str());
  if (histogram_type_dom == nullptr) {
    context->report_missing_attribute(Histogram::histogram_type_str());
    return nullptr;
  }
  if (histogram_type_dom->json_type() != enum_json_type::J_STRING) {
    context->report_node(histogram_type_dom,
                         Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return nullptr;
  }

  // Histogram data type
  const Json_dom *data_type_dom = json_object.get(Histogram::data_type_str());
  if (data_type_dom == nullptr) {
    context->report_missing_attribute(Histogram::data_type_str());
    return nullptr;
  }
  if (data_type_dom->json_type() != enum_json_type::J_STRING) {
    context->report_node(data_type_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return nullptr;
  }

  const Json_string *histogram_type =
      down_cast<const Json_string *>(histogram_type_dom);

  const Json_string *data_type = down_cast<const Json_string *>(data_type_dom);
  Field *field = context->field();
  // compare field data type with histogram data type if context has field info
  if (field) {
    const Value_map_type value_map_type =
        histograms::field_type_to_value_map_type(field);
    std::string field_data_type = value_map_type_to_str[value_map_type];
    if (field_data_type.compare(data_type->value()) != 0) {
      context->report_node(data_type_dom, Message::JSON_WRONG_DATA_TYPE);
      return nullptr;
    }
  }

  Histogram *histogram = nullptr;
  if (histogram_type->value() == Histogram::equi_height_str()) {
    // Equi-height histogram
    if (data_type->value() == "double") {
      histogram =
          Equi_height<double>::create(mem_root, schema_name, table_name,
                                      column_name, Value_map_type::DOUBLE);
    } else if (data_type->value() == "int") {
      histogram = Equi_height<longlong>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::INT);
    } else if (data_type->value() == "enum") {
      histogram = Equi_height<longlong>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::ENUM);
    } else if (data_type->value() == "set") {
      histogram = Equi_height<longlong>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::SET);
    } else if (data_type->value() == "uint") {
      histogram = Equi_height<ulonglong>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::UINT);
    } else if (data_type->value() == "string") {
      histogram =
          Equi_height<String>::create(mem_root, schema_name, table_name,
                                      column_name, Value_map_type::STRING);
    } else if (data_type->value() == "date") {
      histogram = Equi_height<MYSQL_TIME>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::DATE);
    } else if (data_type->value() == "time") {
      histogram = Equi_height<MYSQL_TIME>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::TIME);
    } else if (data_type->value() == "datetime") {
      histogram = Equi_height<MYSQL_TIME>::create(mem_root, schema_name,
                                                  table_name, column_name,
                                                  Value_map_type::DATETIME);
    } else if (data_type->value() == "decimal") {
      histogram =
          Equi_height<my_decimal>::create(mem_root, schema_name, table_name,
                                          column_name, Value_map_type::DECIMAL);
    } else {
      context->report_node(data_type_dom, Message::JSON_UNSUPPORTED_DATA_TYPE);
      return nullptr;
    }
  } else if (histogram_type->value() == Histogram::singleton_str()) {
    // Singleton histogram
    if (data_type->value() == "double") {
      histogram =
          Singleton<double>::create(mem_root, schema_name, table_name,
                                    column_name, Value_map_type::DOUBLE);
    } else if (data_type->value() == "int") {
      histogram = Singleton<longlong>::create(mem_root, schema_name, table_name,
                                              column_name, Value_map_type::INT);
    } else if (data_type->value() == "enum") {
      histogram = Singleton<longlong>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::ENUM);
    } else if (data_type->value() == "set") {
      histogram = Singleton<longlong>::create(mem_root, schema_name, table_name,
                                              column_name, Value_map_type::SET);
    } else if (data_type->value() == "uint") {
      histogram = Singleton<ulonglong>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::UINT);
    } else if (data_type->value() == "string") {
      histogram =
          Singleton<String>::create(mem_root, schema_name, table_name,
                                    column_name, Value_map_type::STRING);
    } else if (data_type->value() == "datetime") {
      histogram =
          Singleton<MYSQL_TIME>::create(mem_root, schema_name, table_name,
                                        column_name, Value_map_type::DATETIME);
    } else if (data_type->value() == "date") {
      histogram = Singleton<MYSQL_TIME>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::DATE);
    } else if (data_type->value() == "time") {
      histogram = Singleton<MYSQL_TIME>::create(
          mem_root, schema_name, table_name, column_name, Value_map_type::TIME);
    } else if (data_type->value() == "decimal") {
      histogram =
          Singleton<my_decimal>::create(mem_root, schema_name, table_name,
                                        column_name, Value_map_type::DECIMAL);
    } else {
      context->report_node(data_type_dom, Message::JSON_UNSUPPORTED_DATA_TYPE);
      return nullptr;
    }
  } else {
    // Unsupported histogram type.
    context->report_node(histogram_type_dom,
                         Message::JSON_UNSUPPORTED_HISTOGRAM_TYPE);
    return nullptr;
  }

  if (histogram != nullptr &&
      histogram->json_to_histogram(json_object, context))
    return nullptr;

  // Global post-check
  if (histogram->get_num_buckets_specified() < histogram->get_num_buckets()) {
    context->report_global(Message::JSON_NUM_BUCKETS_MORE_THAN_SPECIFIED);
    return nullptr;
  }
  return histogram;
}

/*
  All subclasses should also call this function in order to populate fields that
  are shared among all histogram types (character set, null values fraction).
*/
bool Histogram::json_to_histogram(const Json_object &json_object,
                                  Error_context *context) {
  // The sampling rate that was used to create the histogram.
  const Json_dom *sampling_rate_dom = json_object.get(sampling_rate_str());
  if (sampling_rate_dom == nullptr) {
    context->report_missing_attribute(Histogram::sampling_rate_str());
    return true;
  }
  if (sampling_rate_dom->json_type() != enum_json_type::J_DOUBLE) {
    context->report_node(sampling_rate_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }
  const Json_double *sampling_rate =
      down_cast<const Json_double *>(sampling_rate_dom);
  m_sampling_rate = sampling_rate->value();

  // The number of buckets originally specified by the user.
  const Json_dom *num_buckets_specified_dom =
      json_object.get(numer_of_buckets_specified_str());
  if (num_buckets_specified_dom == nullptr) {
    context->report_missing_attribute(
        Histogram::numer_of_buckets_specified_str());
    return true;
  }
  if (num_buckets_specified_dom->json_type() != enum_json_type::J_INT) {
    context->report_node(num_buckets_specified_dom,
                         Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }
  const Json_int *num_buckets_specified =
      down_cast<const Json_int *>(num_buckets_specified_dom);
  m_num_buckets_specified = num_buckets_specified->value();

  // Fraction of SQL null-values in the original data set.
  const Json_dom *null_values_dom = json_object.get(null_values_str());
  if (null_values_dom == nullptr) {
    context->report_missing_attribute(Histogram::null_values_str());
    return true;
  }
  if (null_values_dom->json_type() != enum_json_type::J_DOUBLE) {
    context->report_node(null_values_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }
  const Json_double *null_values =
      down_cast<const Json_double *>(null_values_dom);
  m_null_values_fraction = null_values->value();

  // Character set ID
  const Json_dom *charset_id_dom = json_object.get(collation_id_str());
  if (charset_id_dom == nullptr) {
    context->report_missing_attribute(Histogram::collation_id_str());
    return true;
  }

  /*
   In the JSON object of the histogram, charset_id is defined as an unsigned
   integer, but it may become a signed integer when re-parsed into a JSON
   object.
  */
  if (charset_id_dom->json_type() == enum_json_type::J_UINT) {
    const Json_uint *charset_id = down_cast<const Json_uint *>(charset_id_dom);
    m_charset = get_charset(static_cast<uint>(charset_id->value()), MYF(0));
  } else if (!context->binary() &&
             charset_id_dom->json_type() == enum_json_type::J_INT) {
    const Json_int *charset_id = down_cast<const Json_int *>(charset_id_dom);
    m_charset = get_charset(static_cast<uint>(charset_id->value()), MYF(0));
  } else {
    context->report_node(charset_id_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  // Auto-update property.
  const Json_dom *auto_update_dom = json_object.get(auto_update_str());
  if (auto_update_dom == nullptr) {
    context->report_missing_attribute(Histogram::auto_update_str());
    return true;
  }
  if (auto_update_dom->json_type() != enum_json_type::J_BOOLEAN) {
    context->report_node(auto_update_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }
  const Json_boolean *auto_update =
      down_cast<const Json_boolean *>(auto_update_dom);
  m_auto_update = auto_update->value();

  // Common attributes post-check
  {
    if (sampling_rate->value() < 0.0 || sampling_rate->value() > 1.0) {
      context->report_node(sampling_rate_dom,
                           Message::JSON_INVALID_SAMPLING_RATE);
      return true;
    }
    if (num_buckets_specified->value() < 1 ||
        num_buckets_specified->value() > MAX_NUMBER_OF_HISTOGRAM_BUCKETS) {
      context->report_node(num_buckets_specified_dom,
                           Message::JSON_INVALID_NUM_BUCKETS_SPECIFIED);
      return true;
    }
    if (null_values->value() < 0.0 || null_values->value() > 1.0) {
      context->report_node(null_values_dom, Message::JSON_INVALID_FREQUENCY);
      return true;
    }
    if (m_charset == nullptr) {
      context->report_node(charset_id_dom, Message::JSON_UNSUPPORTED_CHARSET);
      return true;
    }
  }

  return false;
}

bool Histogram::histogram_data_type_to_json(Json_object *json_object) const {
  std::string foo = value_map_type_to_str[get_data_type()];
  const Json_string json_value(foo);
  return json_object->add_clone(data_type_str(), &json_value);
}

template <>
bool Histogram::extract_json_dom_value(const Json_dom *json_dom, double *out,
                                       Error_context *context) {
  if (json_dom->json_type() != enum_json_type::J_DOUBLE) {
    context->report_node(json_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  *out = down_cast<const Json_double *>(json_dom)->value();

  return false;
}

template <>
bool Histogram::extract_json_dom_value(const Json_dom *json_dom, String *out,
                                       Error_context *context) {
  assert(get_character_set() != nullptr);

  char *value_dup_data = nullptr;
  size_t value_dup_length = 0;

  if (json_dom->json_type() == enum_json_type::J_OPAQUE) {
    assert(context->binary());
    const Json_opaque *json_opaque = down_cast<const Json_opaque *>(json_dom);

    String value(json_opaque->value(), json_opaque->size(),
                 get_character_set());
    value_dup_length = value.length();

    /*
      Make a copy of the data, since the JSON opaque will free it before we need
      it.
    */
    value_dup_data = value.dup(get_mem_root());
    if (value_dup_data == nullptr) {
      return true;  // OOM
    }
  } else if (!context->binary() &&
             json_dom->json_type() == enum_json_type::J_STRING) {
    /*
      When a histogram is converted to binary json by histogram_to_json()
      to be persisted, a String-typed value is converted to Json_opaque with
      field type enum_field_types::MYSQL_TYPE_STRING.

      The opaque data is base64-encoded by Json_wrapper::to_string() before it
      goes to the outside as standard json. Json_wrapper::to_string() returns
      an encoded string in the format 'base64:typeN:encoded_data'.

      So when the outside data comes back and is processed by parse_json(), it
      is J_STRING and needs to be decoded here.
    */
    const Json_string *json_string = down_cast<const Json_string *>(json_dom);
    const std::string &encoded_str = json_string->value();

    std::string prefix_builder("base64:type");
    prefix_builder.append(
        std::to_string(static_cast<int>(enum_field_types::MYSQL_TYPE_STRING)));
    prefix_builder.append(":");
    const char *prefix = prefix_builder.c_str();
    int prefix_len = prefix_builder.length();

    size_t pos = encoded_str.find(prefix, 0, prefix_len);
    if (pos == encoded_str.npos) {
      context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
      return true;
    }

    const char *encoded_data = encoded_str.c_str() + prefix_len;
    const size_t encoded_data_len = encoded_str.size() - prefix_len;
    if (encoded_data_len > base64_decode_max_arg_length()) {
      context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
      return true;
    }

    const size_t needed =
        static_cast<size_t>(base64_needed_decoded_length(encoded_data_len));
    String base64_buffer;
    if (base64_buffer.reserve(needed)) {
      return true;
    }

    const char *end_ptr = nullptr;
    const int64 decoded_str_len = base64_decode(encoded_data, encoded_data_len,
                                                &base64_buffer[0], &end_ptr, 0);
    if (decoded_str_len < 0 ||
        (end_ptr &&
         (static_cast<size_t>(end_ptr - encoded_data) != encoded_data_len))) {
      context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
      return true;
    }
    base64_buffer.length(decoded_str_len);

    value_dup_data = base64_buffer.dup(get_mem_root());
    if (value_dup_data == nullptr) {
      return true;  // OOM
    }
    value_dup_length = decoded_str_len;
  } else {
    context->report_node(json_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  out->set(value_dup_data, value_dup_length, get_character_set());
  return false;
}

template <>
bool Histogram::extract_json_dom_value(const Json_dom *json_dom, ulonglong *out,
                                       Error_context *context) {
  if (json_dom->json_type() == enum_json_type::J_UINT)
    *out = down_cast<const Json_uint *>(json_dom)->value();
  else if (!context->binary() &&
           json_dom->json_type() == enum_json_type::J_INT) {
    longlong val = down_cast<const Json_int *>(json_dom)->value();
    if (val < 0) {
      context->report_node(json_dom, Message::JSON_VALUE_OUT_OF_RANGE);
      return true;
    }
    *out = static_cast<ulonglong>(val);
  } else {
    context->report_node(json_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  return false;
}

template <>
bool Histogram::extract_json_dom_value(const Json_dom *json_dom, longlong *out,
                                       Error_context *context) {
  if (json_dom->json_type() != enum_json_type::J_INT) {
    if (json_dom->json_type() == enum_json_type::J_UINT)
      context->report_node(json_dom, Message::JSON_VALUE_OUT_OF_RANGE);
    else
      context->report_node(json_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);

    return true;
  }

  *out = down_cast<const Json_int *>(json_dom)->value();
  return false;
}

template <>
bool Histogram::extract_json_dom_value(const Json_dom *json_dom,
                                       MYSQL_TIME *out,
                                       Error_context *context) {
  if (json_dom->json_type() == enum_json_type::J_DATE ||
      json_dom->json_type() == enum_json_type::J_TIME ||
      json_dom->json_type() == enum_json_type::J_DATETIME ||
      json_dom->json_type() == enum_json_type::J_TIMESTAMP) {
    assert(context->binary());
    *out = *down_cast<const Json_datetime *>(json_dom)->value();
  } else if (!context->binary() &&
             json_dom->json_type() == enum_json_type::J_STRING) {
    const Json_string *json_string = down_cast<const Json_string *>(json_dom);
    String str{json_string->value().c_str(), json_string->value().size(),
               &my_charset_utf8mb4_bin};
    MYSQL_TIME_STATUS status;

    if (get_data_type() == Value_map_type::TIME) {
      if (str_to_time(&str, out, 0, &status) || status.warnings) {
        context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
        return true;
      }
      out->time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_TIME;
    } else if (get_data_type() == Value_map_type::DATE) {
      if (str_to_datetime(&str, out, 0, &status) || status.warnings) {
        context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
        return true;
      }
      out->time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATE;
    } else if (get_data_type() == Value_map_type::DATETIME) {
      if (str_to_datetime(&str, out, 0, &status) || status.warnings) {
        context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
        return true;
      }
      out->time_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME;
    } else {
      return true;
    }
  } else {
    context->report_node(json_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  return false;
}

template <>
bool Histogram::extract_json_dom_value(const Json_dom *json_dom,
                                       my_decimal *out,
                                       Error_context *context) {
  if (json_dom->json_type() == enum_json_type::J_DECIMAL)
    *out = *down_cast<const Json_decimal *>(json_dom)->value();
  else if (!context->binary()) {
    if (json_dom->json_type() == enum_json_type::J_INT) {
      if (longlong2decimal(down_cast<const Json_int *>(json_dom)->value(),
                           out)) {
        context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
        return true;
      }
    } else if (json_dom->json_type() == enum_json_type::J_UINT) {
      if (ulonglong2decimal(down_cast<const Json_uint *>(json_dom)->value(),
                            out)) {
        context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
        return true;
      }
    } else if (json_dom->json_type() == enum_json_type::J_DOUBLE) {
      if (double2decimal(down_cast<const Json_double *>(json_dom)->value(),
                         out)) {
        context->report_node(json_dom, Message::JSON_VALUE_FORMAT_ERROR);
        return true;
      }
    } else {
      context->report_node(json_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
      return true;
    }
  } else {
    return true;
  }

  return false;
}

/**
  Check if a field is covered by a single-part unique index (primary key or
  unique index). Indexes that are marked as invisible are ignored.

  @param thd The current session.
  @param field The field to check.

  @return true if the field is covered by a single-part unique index. False
          otherwise.
*/
static bool covered_by_single_part_index(const THD *thd, const Field *field) {
  Key_map possible_keys;
  possible_keys.merge(field->table->s->usable_indexes(thd));
  possible_keys.intersect(field->key_start);
  assert(field->table->s->keys <= possible_keys.length());
  for (uint i = 0; i < field->table->s->keys; ++i) {
    if (possible_keys.is_set(i) &&
        field->table->s->key_info[i].user_defined_key_parts == 1 &&
        (field->table->s->key_info[i].flags & HA_NOSAME)) {
      return true;
    }
  }

  return false;
}

/**
  Prepare one Value_map for each field we are creating histogram statistics for.
  We will also estimate how many bytes one row will consume. For example, if we
  are creating histogram statistics for two INTEGER columns, we estimate that
  one row will consume (sizeof(longlong) * 2) bytes (16 bytes).

  @param settings            A collection of histogram settings with all the
  fields we are creating histogram statistics for.
  @param[out] value_maps     A map where the Value_maps will be initialized.
  @param[out] row_size_bytes An estimation of how many bytes one row will
                             consume.

  @return true on error, false otherwise.
*/
static bool prepare_value_maps(const Mem_root_array<HistogramSetting> &settings,
                               value_map_collection &value_maps,
                               size_t *row_size_bytes) {
  *row_size_bytes = 0;
  for (const HistogramSetting &setting : settings) {
    const Field *field = setting.field;
    histograms::Value_map_base *value_map = nullptr;

    const Value_map_type value_map_type =
        histograms::field_type_to_value_map_type(field);

    switch (value_map_type) {
      case histograms::Value_map_type::STRING: {
        size_t max_field_length =
            std::min(static_cast<size_t>(field->field_length),
                     histograms::HISTOGRAM_MAX_COMPARE_LENGTH);
        *row_size_bytes += max_field_length * field->charset()->mbmaxlen;
        value_map =
            new histograms::Value_map<String>(field->charset(), value_map_type);
        break;
      }
      case histograms::Value_map_type::DOUBLE: {
        value_map =
            new histograms::Value_map<double>(field->charset(), value_map_type);
        break;
      }
      case histograms::Value_map_type::INT:
      case histograms::Value_map_type::ENUM:
      case histograms::Value_map_type::SET: {
        value_map = new histograms::Value_map<longlong>(field->charset(),
                                                        value_map_type);
        break;
      }
      case histograms::Value_map_type::UINT: {
        value_map = new histograms::Value_map<ulonglong>(field->charset(),
                                                         value_map_type);
        break;
      }
      case histograms::Value_map_type::DATETIME:
      case histograms::Value_map_type::DATE:
      case histograms::Value_map_type::TIME: {
        value_map = new histograms::Value_map<MYSQL_TIME>(field->charset(),
                                                          value_map_type);
        break;
      }
      case histograms::Value_map_type::DECIMAL: {
        value_map = new histograms::Value_map<my_decimal>(field->charset(),
                                                          value_map_type);
        break;
      }
      case histograms::Value_map_type::INVALID: {
        assert(false); /* purecov: deadcode */
        return true;
      }
    }

    // Overhead for each element
    *row_size_bytes += value_map->element_overhead();

    value_maps.emplace(field->field_index(),
                       std::unique_ptr<histograms::Value_map_base>(value_map));
  }

  return false;
}

/**
  Read data from a table into the provided Value_maps. We will read data using
  sampling with the provided sampling percentage.

  @param settings           A collection of histogram settings that contain the
                            fields we are reading data from.
  @param sample_percentage  The sampling percentage we will use for sampling.
                            Must be between 0.0 and 100.0.
  @param table              The table we are reading the data from.
  @param value_maps         The Value_maps we are reading data into.

  @return true on error, false otherwise.
*/
static bool fill_value_maps(const Mem_root_array<HistogramSetting> &settings,
                            double sample_percentage, const TABLE *table,
                            value_map_collection &value_maps) {
  assert(sample_percentage > 0.0);
  assert(sample_percentage <= 100.0);
  assert(settings.size() == value_maps.size());

  // We use uint16_t to get a type with wrap-around that fits in a regular int.
  static std::atomic<uint16_t> global_histogram_sampling_seed(0);
  int sampling_seed = static_cast<int>(global_histogram_sampling_seed++);

  DBUG_EXECUTE_IF("histogram_force_sampling", {
    sampling_seed = 1;
    sample_percentage = 50.0;
  });

  void *scan_ctx = nullptr;

  for (auto &value_map : value_maps)
    value_map.second->set_sampling_rate(sample_percentage / 100.0);

  /* This is not a tablesample request. */
  bool tablesample = false;

  if (table->file->ha_sample_init(scan_ctx, sample_percentage, sampling_seed,
                                  enum_sampling_method::SYSTEM, tablesample)) {
    return true;
  }

  auto handler_guard = create_scope_guard([table, scan_ctx]() {
    table->file->ha_sample_end(scan_ctx); /* purecov: deadcode */
  });

  // Read the data from each column into its own Value_map.
  int res = table->file->ha_sample_next(scan_ctx, table->record[0]);

  while (res == 0) {
    for (const HistogramSetting &setting : settings) {
      const Field *field = setting.field;

      histograms::Value_map_base *value_map =
          value_maps.at(field->field_index()).get();

      switch (histograms::field_type_to_value_map_type(field)) {
        case histograms::Value_map_type::STRING: {
          StringBuffer<MAX_FIELD_WIDTH> str_buf(field->charset());
          field->val_str(&str_buf);

          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(static_cast<String>(str_buf), 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::DOUBLE: {
          double value = field->val_real();
          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::INT:
        case histograms::Value_map_type::ENUM:
        case histograms::Value_map_type::SET: {
          longlong value = field->val_int();
          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::UINT: {
          ulonglong value = static_cast<ulonglong>(field->val_int());
          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::DATE: {
          MYSQL_TIME time_value;
          TIME_from_longlong_date_packed(&time_value,
                                         field->val_date_temporal());
          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(time_value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::TIME: {
          MYSQL_TIME time_value;
          TIME_from_longlong_time_packed(&time_value,
                                         field->val_time_temporal());
          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(time_value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::DATETIME: {
          MYSQL_TIME time_value;
          TIME_from_longlong_datetime_packed(&time_value,
                                             field->val_date_temporal());
          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(time_value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::DECIMAL: {
          my_decimal buffer;
          my_decimal *value;
          value = field->val_decimal(&buffer);

          if (field->is_null())
            value_map->add_null_values(1);
          else if (value_map->add_values(*value, 1))
            return true; /* purecov: deadcode */
          break;
        }
        case histograms::Value_map_type::INVALID: {
          assert(false); /* purecov: deadcode */
          break;
        }
      }
    }

    res = table->file->ha_sample_next(scan_ctx, table->record[0]);

    DBUG_EXECUTE_IF(
        "sample_read_sample_half", static uint count = 1;
        if (count == std::max(1ULL, table->file->stats.records) / 2) {
          res = HA_ERR_END_OF_FILE;
          break;
        } ++count;);
  }

  if (res != HA_ERR_END_OF_FILE) return true; /* purecov: deadcode */

  // Close the handler
  handler_guard.release();
  if (table->file->ha_sample_end(scan_ctx)) {
    assert(false); /* purecov: deadcode */
    return true;
  }

  return false;
}

static bool is_using_data(const HistogramSetting &setting) {
  return setting.data.str != nullptr;
}

/**
  Resolve histogram fields on the supplied collection of histogram update
  settings. Modifies the collection of settings in-place to only keep those that
  can be resolved to fields that exist in the table with a data type that is
  supported by histograms. Also updates the read set for the TABLE to reflect
  what columns to read when sampling data to update the histograms.

  @param thd              Thread handle.
  @param table            Opened table.
  @param[in,out] settings Dynamic array of settings for histograms to update.
  @param results          A container for diagnostics information to the user.
*/
static void resolve_histogram_fields(THD *thd, TABLE *table,
                                     Mem_root_array<HistogramSetting> *settings,
                                     results_map &results) {
  bitmap_clear_all(table->write_set);
  bitmap_clear_all(table->read_set);

  // We iterate through the settings, swap settings that cannot be resolved to
  // the back, and resize the vector to keep only the resolved settings.
  size_t i = 0;
  size_t j = settings->size();
  while (i < j) {
    Field *field = find_field_in_table_sef(table, (*settings)[i].column_name);
    if (field == nullptr) {
      // Field not found in table
      results.emplace((*settings)[i].column_name, Message::FIELD_NOT_FOUND);
      std::swap((*settings)[i], (*settings)[--j]);
      continue;
    }

    if (histograms::field_type_to_value_map_type(field) ==
        histograms::Value_map_type::INVALID) {
      // Unsupported data type
      results.emplace((*settings)[i].column_name,
                      Message::UNSUPPORTED_DATA_TYPE);
      std::swap((*settings)[i], (*settings)[--j]);
      continue;
    }

    // Check if this field is covered by a single-part unique index. If it is,
    // we don't want to create histogram statistics for it.
    if (covered_by_single_part_index(thd, field)) {
      results.emplace((*settings)[i].column_name,
                      Message::COVERED_BY_SINGLE_PART_UNIQUE_INDEX);
      std::swap((*settings)[i], (*settings)[--j]);
      continue;
    }

    // The setting was successfully resolved.
    (*settings)[i].field = field;

    // Update read_set (and write_set in the case of generated columns) for the
    // subsequent sampling of data from the table.
    bitmap_set_bit(table->read_set, field->field_index());
    if (field->is_gcol()) {
      bitmap_set_bit(table->write_set, field->field_index());
      /*
        The base columns needs to be in the write set in case of nested
        generated columns:

        CREATE TABLE t1 (
          col1 INT,
          col2 INT AS (col1 + 1) VIRTUAL,
          col3 INT AS (col2 + 1) VIRTUAL);

        If we are reading data from "col3", we also need to update the data in
        "col2" in order for the generated value to be correct.
      */
      bitmap_union(table->write_set, &field->gcol_info->base_columns_map);
      bitmap_union(table->read_set, &field->gcol_info->base_columns_map);
    }
    ++i;
  }
  settings->resize(j);

  // We should only have a single column for UPDATE HISTOGRAM USING DATA.
  if (std::any_of(settings->begin(), settings->end(), is_using_data)) {
    if (settings->size() > 1) {
      results.emplace("", Message::MULTIPLE_COLUMNS_SPECIFIED);
      settings->clear();
    }
  }
}

/**
  Builds a histogram from a user-supplied JSON string and persists it to the
  dictionary.

  Errors from this function are reported both through calls to my_error() and by
  placing messages in the passed-along results map. These errors and messages
  are sent to the client as a result set, see send_histogram_results() in
  sql_admin.cc.

  The results map is a sink for histogram-specific messages and errors for which
  we typically do not have a my_error() error code. In the context of this
  function and its call to Histogram::json_to_histogram() any potential error
  messages in the results map will primarily relate to JSON formatting errors.

  Calls to my_error() is generally used to report more serious errors, but there
  is no absolute rule for which type of error (e.g. fatal or non-fatal to the
  execution of this function) that goes to which error sink (diagnostics area or
  the results map).

  We guarantee that if the function returns true there will be at least one
  error (ER_UNABLE_TO_BUILD_HISTOGRAM) placed in the diagnostics area.

  @param thd              Thread handle.
  @param table            Opened table.
  @param setting          Settings for the histogram to build.
  @param results          A container for diagnostics information (error and
                          completion messages) to the user.

  @return True on error, false on success.
*/
static bool update_histogram_using_data(THD *thd, Table_ref *table,
                                        const HistogramSetting &setting,
                                        results_map &results) {
  Field *field = setting.field;
  LEX_STRING data = setting.data;
  TABLE *tbl = table->table;

  auto error_guard = create_scope_guard([&]() {
    my_error(ER_UNABLE_TO_BUILD_HISTOGRAM, MYF(0), field->field_name, table->db,
             table->table_name);
  });

  // The column needs to be in the write set because Field::store() is used on
  // the histogram Field to test value domain.
  bitmap_set_bit(tbl->write_set, field->field_index());

  // Parse the literal for a standard JSON object.
  String parse_input{data.str, static_cast<uint32>(data.length),
                     &my_charset_utf8mb4_bin};
  Json_dom_ptr dom;
  JsonParseDefaultErrorHandler parse_handler("UPDATE HISTOGRAM", 0);
  if (parse_json(parse_input, &dom, true, parse_handler,
                 JsonDepthErrorHandler)) {
    results.emplace("", Message::JSON_FORMAT_ERROR);
    return true;
  }
  if (dom->json_type() != enum_json_type::J_OBJECT) {
    results.emplace("", Message::JSON_NOT_AN_OBJECT);
    return true;
  }

  // Convert JSON to histogram.
  MEM_ROOT local_mem_root;
  std::string column_name(field->field_name);
  Error_context context(thd, field, &results);
  histograms::Histogram *histogram = Histogram::json_to_histogram(
      &local_mem_root, std::string(table->db, table->db_length),
      std::string(table->table_name, table->table_name_length), column_name,
      *down_cast<Json_object *>(dom.get()), &context);

  // Store it to persistent storage.
  if (histogram == nullptr || histogram->store_histogram(thd)) return true;
  results.emplace(column_name, Message::HISTOGRAM_CREATED);
  error_guard.release();
  return false;
}

bool update_histograms(THD *thd, Table_ref *table,
                       Mem_root_array<HistogramSetting> *settings,
                       results_map &results) {
  // At this point we should have metadata locks on the table and histograms,
  // and the table should be opened.
  assert(thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::TABLE, table->db, table->table_name, MDL_SHARED_READ));
  assert(table->table != nullptr);
  TABLE *tbl = table->table;

  resolve_histogram_fields(thd, tbl, settings, results);
  if (settings->empty()) return false;

  // UPDATE HISTOGRAM ... USING DATA.
  if (std::any_of(settings->begin(), settings->end(), is_using_data)) {
    assert(settings->size() == 1);  // Checked in resolve_histogram_fields()
    return update_histogram_using_data(thd, table, settings->at(0), results);
  }

  // Sample data, build, and store new histograms.

  // Prepare one Value_map for each field we are creating histogram statistics
  // for. Also, estimate how many bytes one row will consume so that we can
  // estimate how many rows we can fit into memory permitted by
  // histogram_generation_max_mem_size.
  size_t row_size_bytes = 0;
  value_map_collection value_maps;
  if (prepare_value_maps(*settings, value_maps, &row_size_bytes)) return true;

  // Calculate how many rows we can fit into memory permitted by
  // histogram_generation_max_mem_size.
  double rows_in_memory = thd->variables.histogram_generation_max_mem_size /
                          static_cast<double>(row_size_bytes);

  // Ensure that we estimate at least one row in the table, so we avoid
  // division by zero error.
  // NOTE: We ignore errors from "fetch_number_of_rows()" on purpose, since we
  // don't consider it fatal not having the correct row estimate.
  table->fetch_number_of_rows();
  ha_rows rows_in_table = std::max(1ULL, tbl->file->stats.records);
  const double sample_percentage =
      std::min(100.0 * (rows_in_memory / rows_in_table), 100.0);

  // Read data from the table into the Value_maps we have prepared.
  if (fill_value_maps(*settings, sample_percentage, tbl, value_maps))
    return true;

  // Build a histogram for each Value_map, and store it in the data dictionary.
  for (const HistogramSetting &setting : *settings) {
    MEM_ROOT local_mem_root(key_memory_histograms, 256);
    histograms::Histogram *histogram =
        value_maps.at(setting.field->field_index())
            ->build_histogram(
                &local_mem_root, setting.num_buckets,
                std::string(table->db, table->db_length),
                std::string(table->table_name, table->table_name_length),
                std::string(setting.field->field_name));

    if (histogram == nullptr) {
      my_error(ER_UNABLE_TO_BUILD_HISTOGRAM, MYF(0), setting.field->field_name,
               table->db, table->table_name);
      return true;
    }

    histogram->set_auto_update(setting.auto_update);
    if (histogram->store_histogram(thd)) {
      return true;  // Errors have already been reported.
    }
    results.emplace(std::string(setting.field->field_name),
                    Message::HISTOGRAM_CREATED);
  }

  DBUG_EXECUTE_IF("update_histograms_failure", {
    my_error(ER_UNABLE_TO_BUILD_HISTOGRAM, MYF(0), "field", "schema", "table");
    return true;
  });
  return false;
}

bool update_share_histograms(THD *thd, Table_ref *table) {
  assert(thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::TABLE, table->db, table->table_name, MDL_SHARED_READ));
  assert(table->table != nullptr);

  TABLE_SHARE *share = table->table->s;
  Table_histograms *table_histograms =
      Table_histograms::create(key_memory_table_share);
  if (table_histograms == nullptr) return true;
  auto table_histograms_guard =
      create_scope_guard([table_histograms]() { table_histograms->destroy(); });

  // Retrieve histograms from the data dictionary and add them to the
  // set of table_histograms that is to be inserted into the TABLE_SHARE.
  for (size_t i = 0; i < share->fields; ++i) {
    const Field *field = share->field[i];
    if (field->is_hidden_by_system()) continue;

    const histograms::Histogram *histogram = nullptr;
    if (histograms::find_histogram(thd, table->db, table->table_name,
                                   field->field_name, &histogram)) {
      return true;
    }

    if (histogram != nullptr &&
        table_histograms->insert_histogram(field->field_index(), histogram)) {
      return true;
    }
  }

  mysql_mutex_lock(&LOCK_open);
  const bool error = share->m_histograms->insert(table_histograms);
  mysql_mutex_unlock(&LOCK_open);
  if (!error) {
    // If the insertion succeeded ownership responsibility was passed on, so we
    // can disable the scope guard that would free the Table_histograms object.
    table_histograms_guard.release();
  }
  return error;
}

/**
  Acquire exclusive metadata locks on histograms for all columns. Does not check
  whether a histogram exists or not, but simply acquires metadata locks on
  histograms for all columns that are not hidden by the system.

  @param thd Thread object for the statement.
  @param table Opened table.

  @returns True if error, false if success.
*/
static bool lock_table_histograms(THD *thd, TABLE *table) {
  MDL_request_list mdl_requests;
  for (size_t i = 0; i < table->s->fields; ++i) {
    const Field *field = table->s->field[i];
    if (field->is_hidden_by_system()) continue;
    MDL_key mdl_key;
    dd::Column_statistics::create_mdl_key(table->s->db.str,
                                          table->s->table_name.str,
                                          field->field_name, &mdl_key);
    MDL_request *request = new (thd->mem_root) MDL_request;
    if (request == nullptr) return true;  // OOM.
    MDL_REQUEST_INIT_BY_KEY(request, &mdl_key, MDL_EXCLUSIVE, MDL_STATEMENT);
    mdl_requests.push_front(request);
  }

  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout)) {
    return true;
  }
  return false;
}

/**
  A collection of checks to determine whether the session context and table
  properties support histogram updates.

  @param thd Thread handle.
  @param table Table_ref with an open table attached.

  @return True if histogram updates are supported, false otherwise.
*/
static bool supports_histogram_updates(THD *thd, Table_ref *table) {
  TABLE *tbl = table->table;

  // Read-only mode.
  if (read_only || thd->tx_read_only) {
    return false;
  }

  // Temporary table.
  if (tbl->s->tmp_table != NO_TMP_TABLE) {
    return false;
  }

  // View.
  if (table->is_view()) {
    return false;
  }

  // Encrypted table.
  if (tbl->s->encrypt_type.length > 0 &&
      my_strcasecmp(system_charset_info, "n", tbl->s->encrypt_type.str) != 0) {
    return false;
  }
  return true;
}

/**
  Collects the settings for automatically updated histograms on a table.

  @param thd Thread handle.
  @param table Table_ref with an open table attached.
  @param[in,out] settings The vector of settings to be populated.

  @return True if an error occured when retrieving the settings, false
  otherwise.
*/
static bool retrieve_auto_update_histogram_settings(
    THD *thd, Table_ref *table, Mem_root_array<HistogramSetting> *settings) {
  assert(settings->empty());
  TABLE *tbl = table->table;
  for (uint i = 0; i < tbl->s->fields; ++i) {
    const Field *field = tbl->s->field[i];
    if (field->is_hidden_by_system()) continue;

    const Histogram *histogram = nullptr;
    if (find_histogram(thd, table->db, table->table_name, field->field_name,
                       &histogram)) {
      return true;
    }

    if (histogram != nullptr && histogram->get_auto_update()) {
      HistogramSetting setting;
      setting.auto_update = true;
      setting.column_name = field->field_name;
      setting.num_buckets = histogram->get_num_buckets_specified();
      if (settings->push_back(setting)) return true;  // OOM.
    }
  }
  return false;
}

bool auto_update_table_histograms(THD *thd, Table_ref *table) {
  assert(table->table != nullptr);
  if (!supports_histogram_updates(thd, table)) return false;
  if (lock_table_histograms(thd, table->table)) return true;

  Mem_root_array<HistogramSetting> settings(thd->mem_root);
  if (retrieve_auto_update_histogram_settings(thd, table, &settings))
    return true;
  if (settings.empty()) return false;  // No histograms to update.

  results_map results;  // Not used here.
  return update_histograms(thd, table, &settings, results) ||
         update_share_histograms(thd, table);
}

static loglevel log_level(const Sql_condition *condition) {
  switch (condition->severity()) {
    case Sql_condition::SL_ERROR:
      return ERROR_LEVEL;
    case Sql_condition::SL_WARNING:
      return WARNING_LEVEL;
    case Sql_condition::SL_NOTE:
      return INFORMATION_LEVEL;
    default:
      assert(false);
      return ERROR_LEVEL;
  }
}

/**
  Writes messages from the diagnostics area to the error log. Used by the
  background histogram update operation to report diagnostics to the user
  through the error log.

  @note Follows the same approach to reporting as used by the event scheduler.
  See event_scheduler.cc:print_warnings.

  @param thd Thread handle.
  @param db_name The database of the target table for the histogram update.
  @param table_name The target table for the histogram update.
*/
static void write_diagnostics_area_to_error_log(THD *thd, std::string db_name,
                                                std::string table_name) {
  if (thd->get_stmt_da()->cond_count() == 0) return;

  Diagnostics_area::Sql_condition_iterator it =
      thd->get_stmt_da()->sql_conditions();
  const Sql_condition *condition = nullptr;
  while ((condition = it++)) {
    std::string message = "Background histogram update on " + db_name + "." +
                          table_name + ": " +
                          std::string(condition->message_text(),
                                      condition->message_octet_length());
    LogErr(log_level(condition), ER_BACKGROUND_HISTOGRAM_UPDATE,
           static_cast<int>(message.length()), message.c_str());
  }
}

/**
  Prepare the session context for histogram updates.

  @param thd Thread handle.
*/
static void prepare_session_context(THD *thd) {
  thd->reset_for_next_command();
  lex_start(thd);
}

/**
  Clean up the session context following histogram updates.

  @param thd Thread handle.
*/
static void cleanup_session_context(THD *thd) {
  thd->lex->destroy();
  thd->end_statement();  // Calls lex_end().
  thd->cleanup_after_query();

  constexpr size_t kTHDMemRootMaxSizeBytes = 1'000'000;
  if (thd->mem_root->allocated_size() < kTHDMemRootMaxSizeBytes) {
    thd->mem_root->ClearForReuse();
  } else {
    thd->mem_root->Clear();
  }
}

/**
  Custom error handling for histogram updates from the background thread.
  Downgrades all errors to warnings. This is done for two reasons:

  1) Because errors during background histogram updates are mostly ignorable,
     i.e., it is not critical that the user does something if histogram
     statistics fail to be updated.

  2) We wish to throttle error log entries from background histogram updates,
     and this is currently only supported for a priority level of warnings.
*/
class Background_error_handler : public Internal_error_handler {
 public:
  bool handle_condition(THD *, uint, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *) override {
    if (*level == Sql_condition::SL_ERROR) {
      *level = Sql_condition::SL_WARNING;
    }
    return false;
  }
};

/**
  Determine whether a column is hidden from the user.
  Should be equivalent to field::is_hidden_by_system().

  @param col Column definition.

  @return True if the column is hidden from the user, false otherwise.
*/
static bool is_hidden_by_system(const dd::Column *col) {
  return (col->hidden() == dd::Column::enum_hidden_type::HT_HIDDEN_SE ||
          col->hidden() == dd::Column::enum_hidden_type::HT_HIDDEN_SQL);
}

bool auto_update_table_histograms_from_background_thread(
    THD *thd, const std::string &db_name, const std::string &table_name) {
  prepare_session_context(thd);

  // We use a short MDL timeout to avoid blocking the background statistics
  // thread.
  const Timeout_type mdl_timeout_seconds = 5;

  Disable_binlog_guard binlog_guard(thd);
  Disable_autocommit_guard autocommit_guard(thd);

  auto session_guard = create_scope_guard([&]() {
    close_thread_tables(thd);
    thd->mdl_context.release_transactional_locks();
    write_diagnostics_area_to_error_log(thd, db_name, table_name);
    cleanup_session_context(thd);
  });

  // It is crucial that we release objects from the dictionary cache _before_
  // releasing metadata locks on the same objects. Therefore we construct the
  // Auto_releaser _after_ the session_guard scope guard that releases metadata
  // locks, since objects are destructed in the reverse order of construction.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  auto rollback_guard = create_scope_guard([&]() {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
  });

  Background_error_handler error_handler;
  thd->push_internal_handler(&error_handler);
  auto error_handler_guard =
      create_scope_guard([&]() { thd->pop_internal_handler(); });

  // Simulate multiple errors to test error log throttling. We should only see
  // the first error.
  DBUG_EXECUTE_IF("background_histogram_update_errors", {
    my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
    my_error(ER_UNABLE_TO_BUILD_HISTOGRAM, MYF(0), "field", "schema", "table");
    my_error(ER_UNABLE_TO_UPDATE_COLUMN_STATISTICS, MYF(0), "field", "schema",
             "table");
    my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
    my_error(ER_UNABLE_TO_BUILD_HISTOGRAM, MYF(0), "field", "schema", "table");
    my_error(ER_UNABLE_TO_UPDATE_COLUMN_STATISTICS, MYF(0), "field", "schema",
             "table");
    return true;
  });

  // Lock the table metadata so we can check whether the table has any
  // automatically updated histograms. We get the column names from the table
  // definition in order to avoid opening the table until we know that it will
  // be necessary in order to update histograms. Opening the table causes plan
  // changes in an number of MTR tests (likely due to how statistics are
  // loaded), so it is simplest to avoid opening tables for tests that are
  // already stable.
  MDL_request_list requests;
  MDL_request schema_request;
  MDL_request table_request;
  MDL_REQUEST_INIT(&schema_request, MDL_key::SCHEMA, db_name.c_str(), "",
                   MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION);
  MDL_REQUEST_INIT(&table_request, MDL_key::TABLE, db_name.c_str(),
                   table_name.c_str(), MDL_SHARED_READ, MDL_TRANSACTION);
  requests.push_front(&schema_request);
  requests.push_front(&table_request);
  if (thd->mdl_context.acquire_locks(&requests, mdl_timeout_seconds))
    return true;

  // According to dd::table_exists(), the table exists if it can be acquired.
  const dd::Table *table_def = nullptr;
  if (thd->dd_client()->acquire(db_name.c_str(), table_name.c_str(),
                                &table_def))
    return true;
  if (table_def == nullptr) return false;

  // X-lock histograms on all columns.
  MDL_request_list histogram_requests;
  for (const auto &col : table_def->columns()) {
    if (is_hidden_by_system(col)) continue;

    MDL_key mdl_key;
    dd::Column_statistics::create_mdl_key(db_name.c_str(), table_name.c_str(),
                                          col->name().c_str(), &mdl_key);
    MDL_request *request = new (thd->mem_root) MDL_request;
    if (request == nullptr) return true;  // OOM.
    MDL_REQUEST_INIT_BY_KEY(request, &mdl_key, MDL_EXCLUSIVE, MDL_TRANSACTION);
    histogram_requests.push_front(request);
  }

  if (thd->mdl_context.acquire_locks(&histogram_requests, mdl_timeout_seconds))
    return true;

  // Get histogram settings.
  Mem_root_array<HistogramSetting> settings(thd->mem_root);
  for (const auto &col : table_def->columns()) {
    if (is_hidden_by_system(col)) continue;

    const Histogram *histogram = nullptr;
    if (find_histogram(thd, db_name, table_name, col->name().c_str(),
                       &histogram)) {
      return true;
    }

    if (histogram != nullptr && histogram->get_auto_update()) {
      HistogramSetting setting;
      setting.auto_update = true;
      setting.column_name = col->name().c_str();
      setting.num_buckets = histogram->get_num_buckets_specified();
      settings.push_back(setting);
    }
  }

  if (settings.empty()) return false;  // No histograms to update.

  Table_ref table(db_name.c_str(), table_name.c_str(), thr_lock_type::TL_UNLOCK,
                  enum_mdl_type::MDL_SHARED_READ);
  if (open_and_lock_tables(thd, &table, MYSQL_OPEN_HAS_MDL_LOCK)) return true;
  error_handler_guard.release();
  thd->pop_internal_handler();

  if (!supports_histogram_updates(thd, &table)) return false;

  results_map results;  // Not used here.
  if (update_histograms(thd, &table, &settings, results)) return true;

  if (histograms::update_share_histograms(thd, &table) ||
      trans_commit_stmt(thd) || trans_commit(thd)) {
    // Something went wrong when trying to update the table share with the new
    // histograms or when committing the modifications to the histograms to the
    // dictionary. We rollback any modifications to the histograms and request
    // that the share is re-initialized to ensure that the histograms on the
    // share accurately reflect the dictionary.
    //
    // Note that flushing the table share has the potential to disrupt the
    // server as new queries must wait for all existing queries to terminate
    // (and all TABLE objects to be released) before a new TABLE_SHARE can be
    // constructed and new queries can proceed.
    tdc_remove_table(thd, TDC_RT_REMOVE_UNUSED, table.db, table.table_name,
                     false);
    return true;
  }
  rollback_guard.release();

  // The update succeeded and has been committed. Mark cached TABLE objects for
  // re-opening to ensure that they release their (stale) snapshot of the
  // histograms and subsequent queries use an updated snapshot.
  tdc_remove_table(thd, TDC_RT_MARK_FOR_REOPEN, table.db, table.table_name,
                   false);
  return false;
}

bool drop_all_histograms(THD *thd, Table_ref &table,
                         const dd::Table &table_definition,
                         results_map &results) {
  columns_set columns;
  for (const auto &col : table_definition.columns())
    columns.emplace(col->name().c_str());

  return drop_histograms(thd, table, columns, results);
}

bool drop_histograms(THD *thd, Table_ref &table, const columns_set &columns,
                     results_map &results) {
  dd::cache::Dictionary_client *client = thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser auto_releaser(client);

  for (const std::string &column_name : columns) {
    dd::String_type dd_name = dd::Column_statistics::create_name(
        {table.db, table.db_length},
        {table.table_name, table.table_name_length}, column_name.c_str());

    // Do we have an existing histogram for this column?
    const dd::Column_statistics *column_statistics = nullptr;
    if (client->acquire(dd_name, &column_statistics)) {
      // error is already reported.
      return true; /* purecov: deadcode */
    }

    if (column_statistics == nullptr) {
      results.emplace(column_name, Message::NO_HISTOGRAM_FOUND);
      continue;
    }

    if (client->drop(column_statistics)) {
      /* purecov: begin inspected */
      my_error(ER_UNABLE_TO_DROP_COLUMN_STATISTICS, MYF(0), column_name.c_str(),
               table.db, table.table_name);
      return true;
      /* purecov: end */
    }

    results.emplace(column_name, Message::HISTOGRAM_DELETED);
  }

  return false;
}

bool Histogram::store_histogram(THD *thd) const {
  dd::cache::Dictionary_client *client = thd->dd_client();

  DEBUG_SYNC(thd, "store_histogram_after_write_lock");

  dd::String_type dd_name = dd::Column_statistics::create_name(
      get_database_name().str, get_table_name().str, get_column_name().str);

  // Do we have an existing histogram for this column?
  dd::Column_statistics *column_stats = nullptr;
  if (client->acquire_for_modification(dd_name, &column_stats)) {
    // Error has already been reported
    return true; /* purecov: deadcode */
  }

  if (column_stats != nullptr) {
    // Update the existing object.
    column_stats->set_histogram(this);
    if (client->update(column_stats)) {
      /* purecov: begin inspected */
      my_error(ER_UNABLE_TO_UPDATE_COLUMN_STATISTICS, MYF(0),
               get_column_name().str, get_database_name().str,
               get_table_name().str);
      return true;
      /* purecov: end */
    }
  } else {
    // Create a new object
    std::unique_ptr<dd::Column_statistics> column_statistics(
        dd::create_object<dd::Column_statistics>());

    column_statistics.get()->set_schema_name(get_database_name().str);
    column_statistics.get()->set_table_name(get_table_name().str);
    column_statistics.get()->set_column_name(get_column_name().str);
    column_statistics.get()->set_name(dd_name);
    column_statistics.get()->set_histogram(this);

    if (client->store(column_statistics.get())) {
      /* purecov: begin inspected */
      my_error(ER_UNABLE_TO_STORE_COLUMN_STATISTICS, MYF(0),
               get_column_name().str, get_database_name().str,
               get_table_name().str);
      return true;
      /* purecov: end */
    }
  }

  return false;
}

/**
  Rename a single histogram from a old schema/table name to a new schema/table
  name. It is used for instance by RENAME TABLE, where the contents of the
  histograms doesn't change.

  @param thd             Thread handler.
  @param old_schema_name The old schema name.
  @param old_table_name  The old table name.
  @param new_schema_name The new schema name.
  @param new_table_name  The new table name.
  @param column_name     The column name.
  @param results         A map where the result of the operation is stored.

  @return false on success, true on error.
*/
static bool rename_histogram(THD *thd, const char *old_schema_name,
                             const char *old_table_name,
                             const char *new_schema_name,
                             const char *new_table_name,
                             const char *column_name, results_map &results) {
  dd::cache::Dictionary_client *client = thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser auto_releaser(client);

  // First find the histogram with the old name.
  MDL_key mdl_key;
  dd::Column_statistics::create_mdl_key(old_schema_name, old_table_name,
                                        column_name, &mdl_key);

  if (lock_for_write(thd, mdl_key)) {
    // Error has already been reported
    return true; /* purecov: deadcode */
  }

  dd::String_type dd_name = dd::Column_statistics::create_name(
      old_schema_name, old_table_name, column_name);

  dd::Column_statistics *column_statistics = nullptr;
  if (client->acquire_for_modification(dd_name, &column_statistics)) {
    // Error has already been reported
    return true; /* purecov: deadcode */
  }

  if (column_statistics == nullptr) {
    results.emplace(column_name, Message::NO_HISTOGRAM_FOUND);
    return false;
  }

  dd::Column_statistics::create_mdl_key(new_schema_name, new_table_name,
                                        column_name, &mdl_key);

  if (lock_for_write(thd, mdl_key)) {
    // Error has already been reported
    return true; /* purecov: deadcode */
  }

  column_statistics->set_schema_name(new_schema_name);
  column_statistics->set_table_name(new_table_name);
  column_statistics->set_column_name(column_name);
  column_statistics->set_name(column_statistics->create_name());
  if (client->update(column_statistics)) {
    /* purecov: begin inspected */
    my_error(ER_UNABLE_TO_UPDATE_COLUMN_STATISTICS, MYF(0), column_name,
             old_schema_name, old_table_name);
    return true;
    /* purecov: end */
  }

  results.emplace(column_name, Message::HISTOGRAM_DELETED);
  return false;
}

bool rename_histograms(THD *thd, const char *old_schema_name,
                       const char *old_table_name, const char *new_schema_name,
                       const char *new_table_name, results_map &results) {
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLE, old_schema_name,
                   old_table_name, MDL_SHARED_READ_ONLY, MDL_TRANSACTION);

  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout)) {
    // error has already been reported
    return true; /* purecov: deadcode */
  }

  /*
    We have to look up the new table since it already will be renamed at this
    point.
  */
  const dd::Table *table_def = nullptr;
  if (thd->dd_client()->acquire(new_schema_name, new_table_name, &table_def)) {
    // error has already been reported
    return false; /* purecov: deadcode */
  }

  if (table_def == nullptr) {
    assert(false); /* purecov: deadcode */
    return false;
  }

  for (const auto &col : table_def->columns()) {
    if (rename_histogram(thd, old_schema_name, old_table_name, new_schema_name,
                         new_table_name, col->name().c_str(), results))
      return true; /* purecov: deadcode */
  }

  return false;
}

bool find_histogram(THD *thd, const std::string &schema_name,
                    const std::string &table_name,
                    const std::string &column_name,
                    const Histogram **histogram) {
  assert(*histogram == nullptr);

  // We do not support histograms on the system schema.
  if (schema_name == "mysql") return false;

  dd::String_type dd_name = dd::Column_statistics::create_name(
      schema_name.c_str(), table_name.c_str(), column_name.c_str());

  const dd::Column_statistics *column_statistics = nullptr;
  dd::cache::Dictionary_client *client = thd->dd_client();
  if (client->acquire<dd::Column_statistics>(dd_name, &column_statistics))
    return true; /* purecov: deadcode */

  if (column_statistics == nullptr) return false;

  *histogram = column_statistics->histogram();
  return false;
}

template <class T>
double Histogram::get_less_than_selectivity_dispatcher(const T &value) const {
  switch (get_histogram_type()) {
    case enum_histogram_type::SINGLETON: {
      const Singleton<T> *singleton = down_cast<const Singleton<T> *>(this);
      return singleton->get_less_than_selectivity(value);
    }
    case enum_histogram_type::EQUI_HEIGHT: {
      const Equi_height<T> *equi_height =
          down_cast<const Equi_height<T> *>(this);
      return equi_height->get_less_than_selectivity(value);
    }
  }
  /* purecov: begin deadcode */
  assert(false);
  return 0.0;
  /* purecov: end deadcode */
}

template <class T>
double Histogram::get_greater_than_selectivity_dispatcher(
    const T &value) const {
  switch (get_histogram_type()) {
    case enum_histogram_type::SINGLETON: {
      const Singleton<T> *singleton = down_cast<const Singleton<T> *>(this);
      return singleton->get_greater_than_selectivity(value);
    }
    case enum_histogram_type::EQUI_HEIGHT: {
      const Equi_height<T> *equi_height =
          down_cast<const Equi_height<T> *>(this);
      return equi_height->get_greater_than_selectivity(value);
    }
  }
  /* purecov: begin deadcode */
  assert(false);
  return 0.0;
  /* purecov: end deadcode */
}

template <class T>
double Histogram::get_equal_to_selectivity_dispatcher(const T &value) const {
  switch (get_histogram_type()) {
    case enum_histogram_type::SINGLETON: {
      const Singleton<T> *singleton = down_cast<const Singleton<T> *>(this);
      return singleton->get_equal_to_selectivity(value);
    }
    case enum_histogram_type::EQUI_HEIGHT: {
      const Equi_height<T> *equi_height =
          down_cast<const Equi_height<T> *>(this);
      return equi_height->get_equal_to_selectivity(value);
    }
  }
  /* purecov: begin deadcode */
  assert(false);
  return 0.0;
  /* purecov: end deadcode */
}

static bool get_temporal(Item *item, Value_map_type preferred_type,
                         MYSQL_TIME *time_value) {
  if (item->is_temporal_with_date_and_time()) {
    TIME_from_longlong_datetime_packed(time_value, item->val_date_temporal());
  } else if (item->is_temporal_with_date()) {
    TIME_from_longlong_date_packed(time_value, item->val_date_temporal());
  } else if (item->is_temporal_with_time()) {
    TIME_from_longlong_time_packed(time_value, item->val_time_temporal());
  } else {
    switch (preferred_type) {
      case Value_map_type::DATE:
      case Value_map_type::DATETIME:
        if (item->get_date_from_non_temporal(time_value, 0)) return true;
        break;
      case Value_map_type::TIME:
        if (item->get_time_from_non_temporal(time_value)) return true;
        break;
      default:
        /* purecov: begin deadcode */
        assert(0);
        break;
        /* purecov: end deadcode */
    }
  }

  return false;
}

template <class T>
double Histogram::apply_operator(const enum_operator op, const T &value) const {
  switch (op) {
    case enum_operator::LESS_THAN:
      return get_less_than_selectivity_dispatcher(value);
    case enum_operator::GREATER_THAN:
      return get_greater_than_selectivity_dispatcher(value);
    case enum_operator::EQUALS_TO:
      return get_equal_to_selectivity_dispatcher(value);
    default:
      /* purecov: begin deadcode */
      assert(false);
      return 1.0;
      /* purecov: end deadcode */
  }
}

bool Histogram::get_selectivity_dispatcher(Item *item, const enum_operator op,
                                           const TYPELIB *typelib,
                                           double *selectivity) const {
  switch (this->get_data_type()) {
    case Value_map_type::INVALID: {
      /* purecov: begin deadcode */
      assert(false);
      return true;
      /* purecov: end deadcode */
    }
    case Value_map_type::STRING: {
      // Is the character set the same? If not, we cannot use the histogram
      if (item->collation.collation->number != get_character_set()->number)
        return true;

      StringBuffer<MAX_FIELD_WIDTH> str_buf(item->collation.collation);
      const String *str = item->val_str(&str_buf);
      if (item->is_null()) return true;

      *selectivity =
          apply_operator(op, str->substr(0, HISTOGRAM_MAX_COMPARE_LENGTH));
      return false;
    }
    case Value_map_type::INT: {
      const longlong value = item->val_int();
      if (item->is_null()) return true;

      *selectivity = apply_operator(op, value);
      return false;
    }
    case Value_map_type::ENUM: {
      assert(typelib != nullptr);

      longlong value;
      if (item->data_type() == MYSQL_TYPE_VARCHAR) {
        StringBuffer<MAX_FIELD_WIDTH> str_buf(item->collation.collation);
        const String *str = item->val_str(&str_buf);
        if (item->is_null()) return true;

        // Remove any trailing whitespace
        size_t length = str->charset()->cset->lengthsp(
            str->charset(), str->ptr(), str->length());
        value = find_type2(typelib, str->ptr(), length, str->charset());
      } else {
        value = item->val_int();
        if (item->is_null()) return true;
      }

      if (op == enum_operator::EQUALS_TO) {
        *selectivity = get_equal_to_selectivity_dispatcher(value);
        return false;
      }

      return true; /* purecov: deadcode */
    }
    case Value_map_type::SET: {
      assert(typelib != nullptr);

      longlong value;
      if (item->data_type() == MYSQL_TYPE_VARCHAR) {
        StringBuffer<MAX_FIELD_WIDTH> str_buf(item->collation.collation);
        const String *str = item->val_str(&str_buf);
        if (item->is_null()) return true;

        bool got_warning;
        const char *not_used;
        uint not_used2;
        ulonglong tmp_value =
            find_set(typelib, str->ptr(), str->length(), str->charset(),
                     &not_used, &not_used2, &got_warning);

        value = static_cast<ulonglong>(tmp_value);
      } else {
        value = item->val_int();
        if (item->is_null()) return true;
      }

      if (op == enum_operator::EQUALS_TO) {
        *selectivity = get_equal_to_selectivity_dispatcher(value);
        return false;
      }

      return true; /* purecov: deadcode */
    }
    case Value_map_type::UINT: {
      const ulonglong value = static_cast<ulonglong>(item->val_int());
      if (item->is_null()) return true;

      *selectivity = apply_operator(op, value);
      return false;
    }
    case Value_map_type::DOUBLE: {
      const double value = item->val_real();
      if (item->is_null()) return true;

      *selectivity = apply_operator(op, value);
      return false;
    }
    case Value_map_type::DECIMAL: {
      my_decimal buffer;
      const my_decimal *value = item->val_decimal(&buffer);
      if (item->is_null()) return true;

      *selectivity = apply_operator(op, *value);
      return false;
    }
    case Value_map_type::DATE:
    case Value_map_type::TIME:
    case Value_map_type::DATETIME: {
      MYSQL_TIME temporal_value;
      if (get_temporal(item, get_data_type(), &temporal_value) ||
          item->is_null())
        return true;

      *selectivity = apply_operator(op, temporal_value);
      return false;
    }
  }

  /* purecov: begin deadcode */
  assert(false);
  return true;
  /* purecov: end deadcode */
}

bool Histogram::get_selectivity(Item **items, size_t item_count,
                                enum_operator op, double *selectivity) const {
  if (get_raw_selectivity(items, item_count, op, selectivity)) return true;

  /*
    We return a selectivity of at least 0.001 in order to avoid returning very
    low estimates in the following cases:

    1) We miss a value or underestimate its frequency during sampling. With our
       current histogram format this causes "holes" between buckets where we
       estimate a selectivity of zero.

    2) We miss a range of values. With our format we are particularly vulnerable
       around the min and max of the distribution as the sampled min is likely
       greater than the true min and the sampled max likely smaller than the
       true max.

    3) Within-bucket heuristics produce very low estimates. This can for example
       happen for range-queries within a bucket. Another example is if we have
       many infrequent values and one highly frequent value in a bucket.

    4) The histogram has gone stale. While the usual assumption is that the
       value distribution remains nearly constant this assumption fails in some
       common use cases. Consider for example a date column where the current
       date is inserted.

    The reason for the choice of 0.001 for the lower bound is that we typically
    sample fewer than 1000 pages with the default settings. With a sample of
    1000 pages the probablity of missing a value or range of values with a
    selectivity of 0.001 is around 1/e (~0.368) as the size of the table goes to
    infinity in the worst case when the values of interest are concentrated on
    few pages.

    The cost of using a minimum selectivity of 0.001 is that we may sometimes
    over-estimate the selectivity. For very large tables 0.1% of the rows is
    still a lot in absolute terms -- 1000 rows for a table with 1 million rows,
    and 1 million rows for a table with 1 billion rows.

    We could improve this estimate by considering the actual number of pages
    sampled when the histogram was constructed.
  */
  const double minimum_selectivity = 0.001;
  *selectivity = std::max(*selectivity, minimum_selectivity);
  return false;
}

bool Histogram::get_raw_selectivity(Item **items, size_t item_count,
                                    enum_operator op,
                                    double *selectivity) const {
  // Do some sanity checking first
  switch (op) {
    case enum_operator::EQUALS_TO:
    case enum_operator::GREATER_THAN:
    case enum_operator::LESS_THAN:
    case enum_operator::LESS_THAN_OR_EQUAL:
    case enum_operator::GREATER_THAN_OR_EQUAL:
    case enum_operator::NOT_EQUALS_TO:
      assert(item_count == 2);
      /*
        Verify that one side of the predicate is a column/field, and that the
        other side is a constant value (except for EQUALS_TO and NOT_EQUALS_TO).

        Make sure that we have the field item as the left side argument of
        the predicate internally.
      */
      if (items[0]->type() != Item::FIELD_ITEM &&
          items[1]->type() == Item::FIELD_ITEM) {
        // Flip the operators as well as the operator itself.
        switch (op) {
          case enum_operator::GREATER_THAN:
            op = enum_operator::LESS_THAN;
            break;
          case enum_operator::LESS_THAN:
            op = enum_operator::GREATER_THAN;
            break;
          case enum_operator::LESS_THAN_OR_EQUAL:
            op = enum_operator::GREATER_THAN_OR_EQUAL;
            break;
          case enum_operator::GREATER_THAN_OR_EQUAL:
            op = enum_operator::LESS_THAN_OR_EQUAL;
            break;
          default:
            break;
        }
        Item *items_flipped[2];
        items_flipped[0] = items[1];
        items_flipped[1] = items[0];
        return get_selectivity(items_flipped, item_count, op, selectivity);
      } else if (items[0]->type() != Item::FIELD_ITEM ||
                 (!items[1]->const_item() && op != enum_operator::EQUALS_TO &&
                  op != enum_operator::NOT_EQUALS_TO)) {
        return true;
      }
      break;
    case enum_operator::BETWEEN:
    case enum_operator::NOT_BETWEEN:
      assert(item_count == 3);

      if (items[0]->type() != Item::FIELD_ITEM || !items[1]->const_item() ||
          !items[2]->const_item()) {
        return true;
      }
      break;
    case enum_operator::IN_LIST:
    case enum_operator::NOT_IN_LIST:
      assert(item_count >= 2);

      if (items[0]->type() != Item::FIELD_ITEM)
        return true; /* purecov: deadcode */

      // This will only work if all items are const_items
      for (size_t i = 1; i < item_count; ++i) {
        if (!items[i]->const_item()) return true;
      }
      break;
    case enum_operator::IS_NULL:
    case enum_operator::IS_NOT_NULL:
      assert(item_count == 1);
      if (items[0]->type() != Item::FIELD_ITEM) return true;
  }

  assert(items[0]->type() == Item::FIELD_ITEM);

  const TYPELIB *typelib = nullptr;
  const Item_field *item_field = down_cast<const Item_field *>(items[0]);
  if (item_field->field->real_type() == MYSQL_TYPE_ENUM ||
      item_field->field->real_type() == MYSQL_TYPE_SET) {
    const Field_enum *field_enum =
        down_cast<const Field_enum *>(item_field->field);
    typelib = field_enum->typelib;
  }

  switch (op) {
    case enum_operator::LESS_THAN:
    case enum_operator::GREATER_THAN: {
      return get_selectivity_dispatcher(items[1], op, typelib, selectivity);
    }
    case enum_operator::EQUALS_TO:
      if (items[1]->const_item()) {
        return get_selectivity_dispatcher(items[1], op, typelib, selectivity);
      } else if (empty(*this)) {
        return true;
      } else {
        // We do not know the value of items[1], but we assume that it
        // is uniformely distributed over the distinct values of
        // items[0] (i.e. the field for which '*this' is the
        // histogram).
        *selectivity =
            get_num_distinct_values() == 0
                ? 0.0
                : get_non_null_values_fraction() / get_num_distinct_values();

        return false;
      }
    case enum_operator::LESS_THAN_OR_EQUAL: {
      double greater_than_selectivity;
      if (get_selectivity_dispatcher(items[1], enum_operator::GREATER_THAN,
                                     typelib, &greater_than_selectivity))
        return true;

      *selectivity = std::max(
          get_non_null_values_fraction() - greater_than_selectivity, 0.0);
      return false;
    }
    case enum_operator::GREATER_THAN_OR_EQUAL: {
      double less_than_selectivity;
      if (get_selectivity_dispatcher(items[1], enum_operator::LESS_THAN,
                                     typelib, &less_than_selectivity))
        return true;

      *selectivity =
          std::max(get_non_null_values_fraction() - less_than_selectivity, 0.0);
      return false;
    }
    case enum_operator::NOT_EQUALS_TO:
      if (items[1]->const_item()) {
        double equals_to_selectivity;
        if (get_selectivity_dispatcher(items[1], enum_operator::EQUALS_TO,
                                       typelib, &equals_to_selectivity))
          return true;

        *selectivity = std::max(
            get_non_null_values_fraction() - equals_to_selectivity, 0.0);
        return false;
      } else if (empty(*this)) {
        return true;
      } else {
        const size_t distinct_values = get_num_distinct_values();
        if (distinct_values == 0) {
          *selectivity = 0.0;  // Field is NULL for all rows.
        } else if (distinct_values == 1) {
          // Special case of all rows having the same value for this field.
          // Setting the selectivity to 0.0 would be an error, as we may
          // test against a value different from that single distinct value.
          *selectivity = Item_func_ne::kMinSelectivityForUnknownValue;
        } else {
          // We do not know the value of items[1], but we assume that it
          // is uniformely distributed over the distinct values of
          // items[0] (i.e. the field for which '*this' is the
          // histogram).
          *selectivity = get_non_null_values_fraction() *
                         (distinct_values - 1.0) / distinct_values;
        }
        return false;
      }

    case enum_operator::BETWEEN: {
      double less_than_selectivity;
      double greater_than_selectivity;
      if (get_selectivity_dispatcher(items[1], enum_operator::LESS_THAN,
                                     typelib, &less_than_selectivity) ||
          get_selectivity_dispatcher(items[2], enum_operator::GREATER_THAN,
                                     typelib, &greater_than_selectivity))
        return true;

      *selectivity = this->get_non_null_values_fraction() -
                     (less_than_selectivity + greater_than_selectivity);

      /*
        Make sure that we don't return a value less than 0.0. This might happen
        with a query like:
          EXPLAIN SELECT a FROM t1 WHERE t1.a BETWEEN 3 AND 0;
      */
      *selectivity = std::max(0.0, *selectivity);
      return false;
    }
    case enum_operator::NOT_BETWEEN: {
      double less_than_selectivity;
      double greater_than_selectivity;
      if (get_selectivity_dispatcher(items[1], enum_operator::LESS_THAN,
                                     typelib, &less_than_selectivity) ||
          get_selectivity_dispatcher(items[2], enum_operator::GREATER_THAN,
                                     typelib, &greater_than_selectivity))
        return true;

      /*
        Make sure that we don't return a value greater than 1.0. This might
        happen with a query like:
          EXPLAIN SELECT a FROM t1 WHERE t1.a NOT BETWEEN 3 AND 0;
      */
      *selectivity = std::min(less_than_selectivity + greater_than_selectivity,
                              get_non_null_values_fraction());
      return false;
    }
    /*
      TODO(Tobias Christiani): Improve IN selectivity estimates by ensuring that
      selectivity estimates from within each bucket do not exceed the bucket
      frequency. This can be done without allocating additional memory if we
      sort the list of items and "merge" them with the histogram buckets.
    */
    case enum_operator::IN_LIST: {
      *selectivity = 0.0;
      for (size_t i = 1; i < item_count; ++i) {
        double equals_to_selectivity;
        if (get_selectivity_dispatcher(items[i], enum_operator::EQUALS_TO,
                                       typelib, &equals_to_selectivity))
          return true;

        *selectivity += equals_to_selectivity;

        if (*selectivity >= get_non_null_values_fraction()) break;
      }

      /*
        Long in-lists may easily exceed a selectivity of
        get_non_null_values_fraction() in certain cases.
      */
      *selectivity = std::min(*selectivity, get_non_null_values_fraction());
      return false;
    }
    case enum_operator::NOT_IN_LIST: {
      *selectivity = this->get_non_null_values_fraction();
      for (size_t i = 1; i < item_count; ++i) {
        double equals_to_selectivity;
        if (get_selectivity_dispatcher(items[i], enum_operator::EQUALS_TO,
                                       typelib, &equals_to_selectivity)) {
          if (items[i]->null_value) {
            // WHERE col1 NOT IN (..., NULL, ...) will return zero rows.
            *selectivity = 0.0;
            return false;
          }

          return true; /* purecov: deadcode */
        }

        *selectivity -= equals_to_selectivity;
        if (*selectivity <= 0.0) break;
      }

      /*
        Long in-lists may easily estimate a selectivity less than 0.0 in certain
        cases.
      */
      *selectivity = std::max(*selectivity, 0.0);
      return false;
    }
    case enum_operator::IS_NULL:
      *selectivity = this->get_null_values_fraction();
      return false;
    case enum_operator::IS_NOT_NULL:
      *selectivity = 1.0 - this->get_null_values_fraction();
      return false;
  }

  /* purecov: begin deadcode */
  assert(false);
  return true;
  /* purecov: end deadcode */
}

// Explicit template instantiations.
template Histogram *build_histogram(MEM_ROOT *, const Value_map<double> &,
                                    size_t, const std::string &,
                                    const std::string &, const std::string &);

template Histogram *build_histogram(MEM_ROOT *, const Value_map<String> &,
                                    size_t, const std::string &,
                                    const std::string &, const std::string &);

template Histogram *build_histogram(MEM_ROOT *, const Value_map<ulonglong> &,
                                    size_t, const std::string &,
                                    const std::string &, const std::string &);

template Histogram *build_histogram(MEM_ROOT *, const Value_map<longlong> &,
                                    size_t, const std::string &,
                                    const std::string &, const std::string &);

template Histogram *build_histogram(MEM_ROOT *, const Value_map<MYSQL_TIME> &,
                                    size_t, const std::string &,
                                    const std::string &, const std::string &);

template Histogram *build_histogram(MEM_ROOT *, const Value_map<my_decimal> &,
                                    size_t, const std::string &,
                                    const std::string &, const std::string &);

}  // namespace histograms
