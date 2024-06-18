/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

// JSON function items.

#include "sql/item_json_func.h"

#include <assert.h>
#include <algorithm>  // std::fill
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "decimal.h"
#include "field_types.h"  // enum_field_types
#include "lex_string.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "prealloced_array.h"  // Prealloced_array
#include "scope_guard.h"
#include "sql-common/json_diff.h"
#include "sql-common/json_dom.h"
#include "sql-common/json_path.h"
#include "sql-common/json_schema.h"
#include "sql-common/json_syntax_check.h"
#include "sql-common/my_decimal.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/debug_sync.h"
#include "sql/error_handler.h"
#include "sql/field.h"
#include "sql/field_common_properties.h"
#include "sql/item_cmpfunc.h"  // Item_func_like
#include "sql/item_create.h"
#include "sql/parser_yystype.h"
#include "sql/psi_memory_key.h"  // key_memory_JSON
#include "sql/sql_class.h"       // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"  // handle_std_exception
#include "sql/sql_time.h"               // field_type_to_timestamp_type
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_function.h"
#include "sql/thd_raii.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"  // stringcmp
#include "string_with_len.h"
#include "template_utils.h"  // down_cast

class PT_item_list;

/** Helper routines */

bool ensure_utf8mb4(const String &val, String *buf, const char **resptr,
                    size_t *reslength, bool require_string) {
  const CHARSET_INFO *cs = val.charset();

  if (cs == &my_charset_bin) {
    if (require_string)
      my_error(ER_INVALID_JSON_CHARSET, MYF(0), my_charset_bin.csname);
    return true;
  }

  const char *s = val.ptr();
  size_t ss = val.length();

  if (my_charset_same(cs, &my_charset_utf8mb4_bin) ||
      my_charset_same(cs, &my_charset_utf8mb3_bin) ||
      !std::strcmp(cs->csname, "ascii")) {
    /*
      Character data is directly converted to JSON if the character
      set is utf8mb4 or a subset.
    */
  } else {  // If not, we convert, possibly with loss (best effort).
    uint dummy_errors;
    if (buf->copy(val.ptr(), val.length(), val.charset(),
                  &my_charset_utf8mb4_bin, &dummy_errors)) {
      return true; /* purecov: inspected */
    }
    assert(buf->charset() == &my_charset_utf8mb4_bin);
    s = buf->ptr();
    ss = buf->length();
  }

  *resptr = s;
  *reslength = ss;
  return false;
}

/**
  Parse a JSON dom out of an argument to a JSON function.

    @param[in]  res          Pointer to string value of arg.
    @param[in,out] dom       If non-null, we want any text parsed DOM
                             returned at the location pointed to
    @param[in]  require_str_or_json If true, generate an error if other types
                                    used as input
    @param[in] error_handler Pointer to a function that should handle
                             reporting of parsing error.
    @param[in] depth_handler Pointer to a function that should handle error
                             occurred when depth is exceeded.

    @returns false if the arg parsed as valid JSON, true otherwise
  */
bool parse_json(const String &res, Json_dom_ptr *dom, bool require_str_or_json,
                const JsonParseErrorHandler &error_handler,
                const JsonErrorHandler &depth_handler) {
  char buff[MAX_FIELD_WIDTH];
  String utf8_res(buff, sizeof(buff), &my_charset_utf8mb4_bin);

  const char *safep;   // contents of res, possibly converted
  size_t safe_length;  // length of safep

  if (ensure_utf8mb4(res, &utf8_res, &safep, &safe_length,
                     require_str_or_json)) {
    return true;
  }

  if (!dom) {
    assert(!require_str_or_json);
    return !is_valid_json_syntax(safep, safe_length, nullptr, nullptr,
                                 depth_handler);
  }

  *dom = Json_dom::parse(safep, safe_length, error_handler, depth_handler);

  return *dom == nullptr;
}

enum_json_diff_status apply_json_diffs(Field_json *field,
                                       const Json_diff_vector *diffs) {
  DBUG_TRACE;
  // Cannot apply a diff to NULL.
  if (field->is_null()) return enum_json_diff_status::REJECTED;

  DBUG_EXECUTE_IF("simulate_oom_in_apply_json_diffs", {
    DBUG_SET("+d,simulate_out_of_memory");
    DBUG_SET("-d,simulate_oom_in_apply_json_diffs");
  });

  Json_wrapper doc;
  if (field->val_json(&doc))
    return enum_json_diff_status::ERROR; /* purecov: inspected */

  doc.dbug_print("apply_json_diffs: before-doc", JsonDepthErrorHandler);

  // Should we collect logical diffs while applying them?
  const bool collect_logical_diffs =
      field->table->is_logical_diff_enabled(field);

  // Should we try to perform the update in place using binary diffs?
  bool binary_inplace_update = field->table->is_binary_diff_enabled(field);

  StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer;

  for (const Json_diff &diff : *diffs) {
    Json_wrapper val = diff.value();

    auto &path = diff.path();

    if (path.leg_count() == 0) {
      /*
        Cannot replace the root (then a full update will be used
        instead of creating a diff), or insert the root, or remove the
        root, so reject this diff.
      */
      return enum_json_diff_status::REJECTED;
    }

    if (collect_logical_diffs)
      field->table->add_logical_diff(field, path, diff.operation(), &val);

    if (binary_inplace_update) {
      if (diff.operation() == enum_json_diff_operation::REPLACE) {
        bool partially_updated = false;
        bool replaced_path = false;
        if (doc.attempt_binary_update(field, path, &val, false, &buffer,
                                      &partially_updated, &replaced_path))
          return enum_json_diff_status::ERROR; /* purecov: inspected */

        if (partially_updated) {
          if (!replaced_path) return enum_json_diff_status::REJECTED;
          DBUG_EXECUTE_IF("rpl_row_jsondiff_binarydiff", {
            const char act[] =
                "now SIGNAL signal.rpl_row_jsondiff_binarydiff_created";
            assert(opt_debug_sync_timeout > 0);
            assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
          };);
          continue;
        }
      } else if (diff.operation() == enum_json_diff_operation::REMOVE) {
        Json_wrapper_vector hits(key_memory_JSON);
        bool found_path = false;
        if (doc.binary_remove(field, path, &buffer, &found_path))
          return enum_json_diff_status::ERROR; /* purecov: inspected */
        if (!found_path) return enum_json_diff_status::REJECTED;
        continue;
      }

      // Couldn't update in place, so try full update.
      binary_inplace_update = false;
      field->table->disable_binary_diffs_for_current_row(field);
    }

    Json_dom *dom = doc.to_dom();
    if (doc.to_dom() == nullptr)
      return enum_json_diff_status::ERROR; /* purecov: inspected */

    enum_json_diff_status res = apply_json_diff(diff, dom);

    // If the diff was not applied successfully exit with the error status,
    // otherwise continue to the next diff
    if (res == enum_json_diff_status::ERROR ||
        res == enum_json_diff_status::REJECTED) {
      return res;
    } else {
      continue;
    }
  }

  if (field->store_json(&doc) != TYPE_OK)
    return enum_json_diff_status::ERROR; /* purecov: inspected */

  return enum_json_diff_status::SUCCESS;
}

/**
  Get correct blob type of given Field.
  A helper function for get_normalized_field_type().

  @param arg  the field to get blob type of

  @returns
    correct blob type
*/

static enum_field_types get_real_blob_type(const Field *arg) {
  assert(arg);
  return blob_type_from_pack_length(arg->pack_length() -
                                    portable_sizeof_char_ptr);
}

/**
  Get correct blob type of given Item.
  A helper function for get_normalized_field_type().

  @param arg  the item to get blob type of

  @returns
    correct blob type
*/

static enum_field_types get_real_blob_type(const Item *arg) {
  assert(arg);
  /*
    TINYTEXT, TEXT, MEDIUMTEXT, and LONGTEXT have type
    MYSQL_TYPE_BLOB. We want to treat them like strings. We check
    the collation to see if the blob is really a string.
  */
  if (arg->collation.collation != &my_charset_bin) return MYSQL_TYPE_VARCHAR;

  if (arg->type() == Item::FIELD_ITEM)
    return get_real_blob_type((down_cast<const Item_field *>(arg))->field);

  return arg->data_type();
}

/**
  Get the field type of an item. This function returns the same value
  as arg->data_type() in most cases, but in some cases it may return
  another field type in order to ensure that the item gets handled the
  same way as items of a different type.
*/
static enum_field_types get_normalized_field_type(const Item *arg) {
  const enum_field_types ft = arg->data_type();
  switch (ft) {
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
      return get_real_blob_type(arg);

    default:
      break;
  }
  return ft;
}

bool get_json_object_member_name(const THD *thd, Item *arg_item, String *value,
                                 String *utf8_res, const char **safep,
                                 size_t *safe_length) {
  String *const res = arg_item->val_str(value);
  if (thd->is_error()) return true;

  if (arg_item->null_value) {
    my_error(ER_JSON_DOCUMENT_NULL_KEY, MYF(0));
    return true;
  }

  if (ensure_utf8mb4(*res, utf8_res, safep, safe_length, true)) {
    return true;
  }

  return false;
}

/**
  A helper method that checks whether or not the given argument can be converted
  to JSON. The function only checks the type of the given item, and doesn't do
  any parsing or further checking of the item.

  @param item The item to be checked

  @retval true The item is possibly convertible to JSON
  @retval false The item is not convertible to JSON
*/
static bool is_convertible_to_json(const Item *item) {
  const enum_field_types field_type = get_normalized_field_type(item);
  switch (field_type) {
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_JSON:
      return true;
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
      if (item->type() == Item::FIELD_ITEM) {
        const Item_field *fi = down_cast<const Item_field *>(item);
        const Field *field = fi->field;
        if (field->is_flag_set(ENUM_FLAG) || field->is_flag_set(SET_FLAG)) {
          return false;
        }
      }
      return true;
    default:
      return false;
  }
}

/**
  Checks if an Item is of a type that is convertible to JSON. An error is raised
  if it is not convertible.
*/
static bool check_convertible_to_json(const Item *item, int argument_number,
                                      const char *function_name) {
  if (!is_convertible_to_json(item)) {
    my_error(ER_INVALID_TYPE_FOR_JSON, MYF(0), argument_number, function_name);
    return true;
  }
  return false;
}

/**
  Helper method for Item_func_json_* methods. Check if a JSON item or
  JSON text is valid and, for the latter, optionally construct a DOM
  tree (i.e. only if valid).

  @param[in]     args       Item_func::args alias
  @param[in]     arg_idx    Index (0-based) of argument into the args array
  @param[out]    value      Alias for @code Item_func_json_*::m_value @endcode
  @param[in]     func_name  Name of the user-invoked JSON_ function
  @param[in,out] dom        If non-null, we want any text parsed DOM
                            returned at the location pointed to
  @param[in]     require_str_or_json
                            If true, generate an error if other types used
                            as input
  @param[out]    valid      true if a valid JSON value was found (or NULL),
                            else false

  @returns true iff syntax error *and* dom != null, else false
*/
static bool json_is_valid(Item **args, uint arg_idx, String *value,
                          const char *func_name, Json_dom_ptr *dom,
                          bool require_str_or_json, bool *valid) {
  Item *const arg_item = args[arg_idx];

  const enum_field_types field_type = get_normalized_field_type(arg_item);

  if (!is_convertible_to_json(arg_item)) {
    if (require_str_or_json) {
      *valid = false;
      my_error(ER_INVALID_TYPE_FOR_JSON, MYF(0), arg_idx + 1, func_name);
      return true;
    }

    *valid = false;
    return false;
  } else if (field_type == MYSQL_TYPE_NULL) {
    if (arg_item->update_null_value()) return true;
    assert(arg_item->null_value);
    *valid = true;
    return false;
  } else if (field_type == MYSQL_TYPE_JSON) {
    Json_wrapper w;
    // Also sets the null_value flag
    *valid = !arg_item->val_json(&w);
    return !*valid;
  } else {
    String *const res = arg_item->val_str(value);
    if (current_thd->is_error()) return true;

    if (arg_item->null_value) {
      *valid = true;
      return false;
    }

    bool parse_error = false;
    const bool failure = parse_json(
        *res, dom, require_str_or_json,
        [&parse_error, arg_idx, func_name](const char *parse_err,
                                           size_t err_offset) {
          my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), arg_idx + 1,
                   func_name, parse_err, err_offset, "");
          parse_error = true;
        },
        JsonDepthErrorHandler);
    *valid = !failure;
    return parse_error;
  }
}

bool parse_path(const String &path_value, bool forbid_wildcards,
                Json_path *json_path) {
  const char *path_chars = path_value.ptr();
  size_t path_length = path_value.length();
  StringBuffer<STRING_BUFFER_USUAL_SIZE> res(&my_charset_utf8mb4_bin);

  if (ensure_utf8mb4(path_value, &res, &path_chars, &path_length, true)) {
    return true;
  }

  // OK, we have a string encoded in utf-8. Does it parse?
  size_t bad_idx = 0;
  if (parse_path(path_length, path_chars, json_path, &bad_idx)) {
    /*
      Issue an error message. The last argument is no longer used, but kept to
      avoid changing error message format.
    */
    my_error(ER_INVALID_JSON_PATH, MYF(0), bad_idx, "");
    return true;
  }

  if (forbid_wildcards && json_path->can_match_many()) {
    my_error(ER_INVALID_JSON_PATH_WILDCARD, MYF(0));
    return true;
  }

  return false;
}

/**
  Parse a oneOrAll argument.

  @param[in]  candidate   The string to compare to "one" or "all"
  @param[in]  func_name   The name of the calling function

  @returns ooa_one, ooa_all, or ooa_error, based on the match
*/
static enum_one_or_all_type parse_one_or_all(const String *candidate,
                                             const char *func_name) {
  /*
    First convert the candidate to utf8mb4.

    A buffer of four bytes is enough to hold the candidate in the common
    case ("one" or "all" + terminating NUL character).

    We can ignore conversion errors here. If a conversion error should
    happen, the converted string will contain a question mark, and we will
    correctly raise an error later because no string with a question mark
    will match "one" or "all".
  */
  StringBuffer<4> utf8str;
  uint errors;
  if (utf8str.copy(candidate->ptr(), candidate->length(), candidate->charset(),
                   &my_charset_utf8mb4_bin, &errors))
    return ooa_error; /* purecov: inspected */

  const char *str = utf8str.c_ptr_safe();
  if (!my_strcasecmp(&my_charset_utf8mb4_general_ci, str, "all"))
    return ooa_all;

  if (!my_strcasecmp(&my_charset_utf8mb4_general_ci, str, "one"))
    return ooa_one;

  my_error(ER_JSON_BAD_ONE_OR_ALL_ARG, MYF(0), func_name);
  return ooa_error;
}

/**
  Parse and cache a (possibly constant) oneOrAll argument.

  @param[in]  thd           THD handle.
  @param[in]  arg           The oneOrAll arg passed to the JSON function.
  @param[in]  cached_ooa    Previous result of parsing this arg.
  @param[in]  func_name     The name of the calling JSON function.

  @returns ooa_one, ooa_all, ooa_null or ooa_error, based on the match
*/
static enum_one_or_all_type parse_and_cache_ooa(
    const THD *thd, Item *arg, enum_one_or_all_type *cached_ooa,
    const char *func_name) {
  if (arg->const_for_execution()) {
    if (*cached_ooa != ooa_uninitialized) {
      return *cached_ooa;
    }
  }

  StringBuffer<16> buffer;  // larger than common case: three characters + '\0'
  String *const one_or_all = arg->val_str(&buffer);
  if (thd->is_error()) {
    *cached_ooa = ooa_error;
  } else if (arg->null_value) {
    *cached_ooa = ooa_null;
  } else {
    *cached_ooa = parse_one_or_all(one_or_all, func_name);
  }

  return *cached_ooa;
}

/** Json_path_cache */

Json_path_cache::Json_path_cache(THD *thd, uint size)
    : m_paths(key_memory_JSON), m_arg_idx_to_vector_idx(thd->mem_root, size) {
  reset_cache();
}

Json_path_cache::~Json_path_cache() = default;

bool Json_path_cache::parse_and_cache_path(const THD *thd, Item **args,
                                           uint arg_idx,
                                           bool forbid_wildcards) {
  Item *arg = args[arg_idx];

  const bool is_constant = arg->const_for_execution();
  Path_cell &cell = m_arg_idx_to_vector_idx[arg_idx];

  if (is_constant && cell.m_status != enum_path_status::UNINITIALIZED) {
    // nothing to do if it has already been parsed
    assert(cell.m_status == enum_path_status::OK_NOT_NULL ||
           cell.m_status == enum_path_status::OK_NULL);
    return false;
  }

  if (cell.m_status == enum_path_status::UNINITIALIZED) {
    cell.m_index = m_paths.size();
    if (m_paths.emplace_back(key_memory_JSON))
      return true; /* purecov: inspected */
  } else {
    // re-parsing a non-constant path for the next row
    m_paths[cell.m_index].clear();
  }

  const String *path_value = arg->val_str(&m_path_value);
  if (thd->is_error()) return true;
  if (arg->null_value) {
    cell.m_status = enum_path_status::OK_NULL;
    return false;
  }

  if (parse_path(*path_value, forbid_wildcards, &m_paths[cell.m_index]))
    return true;

  cell.m_status = enum_path_status::OK_NOT_NULL;
  return false;
}

const Json_path *Json_path_cache::get_path(uint arg_idx) const {
  const Path_cell &cell = m_arg_idx_to_vector_idx[arg_idx];

  if (cell.m_status != enum_path_status::OK_NOT_NULL) {
    return nullptr;
  }

  return &m_paths[cell.m_index];
}

void Json_path_cache::reset_cache() {
  std::fill(m_arg_idx_to_vector_idx.begin(), m_arg_idx_to_vector_idx.end(),
            Path_cell());

  m_paths.clear();
}

/** JSON_*() support methods */

void Item_json_func::cleanup() {
  Item_func::cleanup();

  m_path_cache.reset_cache();
}

longlong Item_func_json_valid::val_int() {
  assert(fixed);
  try {
    bool ok;
    if (json_is_valid(args, 0, &m_value, func_name(), nullptr, false, &ok)) {
      return error_int();
    }

    null_value = args[0]->null_value;

    if (null_value || !ok) return 0;

    return 1;
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }
}

static bool evaluate_constant_json_schema(
    THD *thd, Item *json_schema, Json_schema_validator *cached_schema_validator,
    Item **ref) {
  assert(is_convertible_to_json(json_schema));
  const char *func_name = down_cast<const Item_func *>(*ref)->func_name();
  if (json_schema->const_item()) {
    String schema_buffer;
    String *schema_string = json_schema->val_str(&schema_buffer);
    if (thd->is_error()) return true;
    if (json_schema->null_value) {
      *ref = new (thd->mem_root) Item_null((*ref)->item_name);
      if (*ref == nullptr) return true;
    } else {
      const JsonSchemaDefaultErrorHandler error_handler(func_name);
      if (cached_schema_validator->initialize(
              thd->mem_root, schema_string->ptr(), schema_string->length(),
              error_handler, JsonDepthErrorHandler)) {
        return true;
      }
    }
  }
  return false;
}

bool Item_func_json_schema_valid::fix_fields(THD *thd, Item **ref) {
  if (Item_bool_func::fix_fields(thd, ref)) return true;

  // Both arguments must have types that are convertible to JSON.
  for (uint i = 0; i < arg_count; ++i)
    if (check_convertible_to_json(args[i], i + 1, func_name())) return true;

  return evaluate_constant_json_schema(thd, args[0], &m_cached_schema_validator,
                                       ref);
}

void Item_func_json_schema_valid::cleanup() { Item_bool_func::cleanup(); }

Item_func_json_schema_valid::Item_func_json_schema_valid(const POS &pos,
                                                         Item *a, Item *b)
    : Item_bool_func(pos, a, b) {}

Item_func_json_schema_valid::~Item_func_json_schema_valid() = default;

static bool do_json_schema_validation(
    const THD *thd, Item *json_schema, Item *json_document,
    const char *func_name, const Json_schema_validator &cached_schema_validator,
    bool *null_value, bool *validation_result,
    Json_schema_validation_report *validation_report) {
  assert(is_convertible_to_json(json_document));

  String document_buffer;
  String *document_string = json_document->val_str(&document_buffer);
  if (thd->is_error()) return true;
  if (json_document->null_value) {
    *null_value = true;
    return false;
  }

  if (cached_schema_validator.is_initialized()) {
    assert(json_schema->const_item());
    const JsonSchemaDefaultErrorHandler error_handler(func_name);
    if (cached_schema_validator.is_valid(
            document_string->ptr(), document_string->length(), error_handler,
            JsonDepthErrorHandler, validation_result, validation_report)) {
      return true;
    }
  } else {
    // Fields that are a part of constant tables (i.e. primary key lookup) are
    // not reported as constant items during fix fields. So while we won't set
    // up the cached schema validator during fix_fields, the item will appear as
    // const here, and thus failing the assertion if we don't take constant
    // tables into account.
    assert(!json_schema->const_item() ||
           (json_schema->real_item()->type() == Item::FIELD_ITEM &&
            down_cast<const Item_field *>(json_schema->real_item())
                ->m_table_ref->table->const_table));

    assert(is_convertible_to_json(json_schema));

    String schema_buffer;
    String *schema_string = json_schema->val_str(&schema_buffer);
    if (thd->is_error()) return true;
    if (json_schema->null_value) {
      *null_value = true;
      return false;
    }

    const JsonSchemaDefaultErrorHandler error_handler(func_name);
    if (is_valid_json_schema(document_string->ptr(), document_string->length(),
                             schema_string->ptr(), schema_string->length(),
                             error_handler, JsonDepthErrorHandler,
                             validation_result, validation_report)) {
      return true;
    }
  }

  *null_value = false;
  return false;
}

bool Item_func_json_schema_valid::val_bool() {
  assert(fixed);
  bool validation_result = false;

  if (m_in_check_constraint_exec_ctx) {
    Json_schema_validation_report validation_report;
    if (do_json_schema_validation(current_thd, args[0], args[1], func_name(),
                                  m_cached_schema_validator, &null_value,
                                  &validation_result, &validation_report)) {
      return error_bool();
    }

    if (!null_value && !validation_result) {
      my_error(ER_JSON_SCHEMA_VALIDATION_ERROR_WITH_DETAILED_REPORT, MYF(0),
               validation_report.human_readable_reason().c_str());
    }
  } else {
    if (do_json_schema_validation(current_thd, args[0], args[1], func_name(),
                                  m_cached_schema_validator, &null_value,
                                  &validation_result, nullptr)) {
      return error_bool();
    }
  }

  assert(is_nullable() || !null_value);
  return validation_result;
}

bool Item_func_json_schema_validation_report::fix_fields(THD *thd, Item **ref) {
  if (Item_json_func::fix_fields(thd, ref)) return true;

  // Both arguments must have types that are convertible to JSON.
  for (uint i = 0; i < arg_count; ++i)
    if (check_convertible_to_json(args[i], i + 1, func_name())) return true;

  return evaluate_constant_json_schema(thd, args[0], &m_cached_schema_validator,
                                       ref);
}

void Item_func_json_schema_validation_report::cleanup() {
  Item_json_func::cleanup();
}

Item_func_json_schema_validation_report::
    Item_func_json_schema_validation_report(THD *thd, const POS &pos,
                                            PT_item_list *a)
    : Item_json_func(thd, pos, a) {}

Item_func_json_schema_validation_report::
    ~Item_func_json_schema_validation_report() = default;

bool Item_func_json_schema_validation_report::val_json(Json_wrapper *wr) {
  assert(fixed);
  bool validation_result = false;
  Json_schema_validation_report validation_report;
  if (do_json_schema_validation(current_thd, args[0], args[1], func_name(),
                                m_cached_schema_validator, &null_value,
                                &validation_result, &validation_report)) {
    return error_json();
  }

  assert(is_nullable() || !null_value);
  std::unique_ptr<Json_object> result(new (std::nothrow) Json_object());
  if (result == nullptr) return error_json();  // OOM

  Json_boolean *json_validation_result =
      new (std::nothrow) Json_boolean(validation_result);
  if (result->add_alias("valid", json_validation_result)) return error_json();

  if (!validation_result) {
    Json_string *json_human_readable_reason = new (std::nothrow)
        Json_string(validation_report.human_readable_reason());
    if (result->add_alias("reason", json_human_readable_reason))
      return error_json();  // OOM

    Json_string *json_schema_location =
        new (std::nothrow) Json_string(validation_report.schema_location());
    if (result->add_alias("schema-location", json_schema_location))
      return error_json();  // OOM

    Json_string *json_schema_failed_keyword = new (std::nothrow)
        Json_string(validation_report.schema_failed_keyword());
    if (result->add_alias("schema-failed-keyword", json_schema_failed_keyword))
      return error_json();  // OOM

    Json_string *json_document_location =
        new (std::nothrow) Json_string(validation_report.document_location());
    if (result->add_alias("document-location", json_document_location))
      return error_json();  // OOM
  }

  *wr = Json_wrapper(std::move(result));
  return false;
}

typedef Prealloced_array<size_t, 16> Sorted_index_array;

void Item_func_json_contains::cleanup() {
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
}

longlong Item_func_json_contains::val_int() {
  assert(fixed);
  THD *const thd = current_thd;

  try {
    Json_wrapper doc_wrapper;

    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &doc_wrapper) ||
        args[0]->null_value) {
      return error_int();
    }

    Json_wrapper containee_wr;

    // arg 1 is the possible containee
    if (get_json_wrapper(args, 1, &m_doc_value, func_name(), &containee_wr) ||
        args[1]->null_value) {
      return error_int();
    }

    if (arg_count == 3) {
      // path is specified
      if (m_path_cache.parse_and_cache_path(thd, args, 2, true))
        return error_int();
      const Json_path *path = m_path_cache.get_path(2);
      if (path == nullptr) {
        return error_int();
      }

      Json_wrapper_vector v(key_memory_JSON);
      if (doc_wrapper.seek(*path, path->leg_count(), &v, true, false))
        return error_int(); /* purecov: inspected */

      if (v.size() == 0) {
        return error_int();
      }

      bool ret;
      if (json_wrapper_contains(v[0], containee_wr, &ret))
        return error_int(); /* purecov: inspected */
      null_value = false;
      return ret;
    } else {
      bool ret;
      if (json_wrapper_contains(doc_wrapper, containee_wr, &ret))
        return error_int(); /* purecov: inspected */
      null_value = false;
      return ret;
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }
}

void Item_func_json_contains_path::cleanup() {
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
  m_cached_ooa = ooa_uninitialized;
}

longlong Item_func_json_contains_path::val_int() {
  assert(fixed);
  longlong result = 0;
  null_value = false;

  Json_wrapper wrapper;
  Json_wrapper_vector hits(key_memory_JSON);

  try {
    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value) {
      return error_int();
    }

    // arg 1 is the oneOrAll flag
    bool require_all;
    const THD *const thd = current_thd;
    switch (parse_and_cache_ooa(thd, args[1], &m_cached_ooa, func_name())) {
      case ooa_all: {
        require_all = true;
        break;
      }
      case ooa_one: {
        require_all = false;
        break;
      }
      case ooa_null: {
        null_value = true;
        return 0;
      }
      default: {
        return error_int();
      }
    }

    // the remaining args are paths
    for (uint32 i = 2; i < arg_count; ++i) {
      if (m_path_cache.parse_and_cache_path(thd, args, i, false))
        return error_int();
      const Json_path *path = m_path_cache.get_path(i);
      if (path == nullptr) {
        return error_int();
      }

      hits.clear();
      if (wrapper.seek(*path, path->leg_count(), &hits, true, true))
        return error_int(); /* purecov: inspected */
      if (hits.size() > 0) {
        result = 1;
        if (!require_all) {
          break;
        }
      } else {
        if (require_all) {
          result = 0;
          break;
        }
      }
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  return result;
}

bool json_value(Item *arg, Json_wrapper *result, bool *has_value) {
  if (arg->data_type() == MYSQL_TYPE_NULL) {
    if (arg->update_null_value()) return true;
    assert(arg->null_value);
    *has_value = true;
    return false;
  }

  // Give up if this is not a JSON value (including typed arrays)
  if (arg->data_type() != MYSQL_TYPE_JSON && !arg->returns_array()) {
    *has_value = false;
    return false;
  }

  *has_value = true;

  const bool error = arg->val_json(result);
  return error;
}

bool get_json_wrapper(Item **args, uint arg_idx, String *str,
                      const char *func_name, Json_wrapper *wrapper) {
  Item *const arg = args[arg_idx];

  bool has_value;
  if (json_value(arg, wrapper, &has_value)) {
    return true;
  }
  // If the value was handled, return with success
  if (has_value) {
    return false;
  }
  /*
    Otherwise, it's a non-JSON type, so we need to see if we can
    convert it to JSON.
  */

  /* Is this a JSON text? */
  Json_dom_ptr dom;  //@< we'll receive a DOM here from a successful text parse

  bool valid;
  if (json_is_valid(args, arg_idx, str, func_name, &dom, true, &valid))
    return true;

  if (!valid) {
    my_error(ER_INVALID_TYPE_FOR_JSON, MYF(0), arg_idx + 1, func_name);
    return true;
  }

  if (arg->null_value) {
    return false;
  }

  assert(dom);

  *wrapper = Json_wrapper(std::move(dom));
  return false;
}

bool Item_func_json_type::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_JSON)) return true;
  if (reject_vector_args()) return true;
  set_nullable(true);
  m_value.set_charset(&my_charset_utf8mb4_bin);
  set_data_type_string(kMaxJsonTypeNameLength + 1, &my_charset_utf8mb4_bin);
  return false;
}

String *Item_func_json_type::val_str(String *) {
  assert(fixed);

  try {
    Json_wrapper wr;
    if (get_json_wrapper(args, 0, &m_value, func_name(), &wr) ||
        args[0]->null_value) {
      null_value = true;
      return nullptr;
    }

    m_value.length(0);
    if (m_value.append(json_type_name(wr)))
      return error_str(); /* purecov: inspected */

  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_str();
    /* purecov: end */
  }

  null_value = false;
  return &m_value;
}

static String *val_string_from_json(Item_func *item, String *buffer) {
  Json_wrapper wr;
  if (item->val_json(&wr)) return item->error_str();
  if (item->null_value) return nullptr;

  buffer->length(0);
  if (wr.to_string(buffer, true, item->func_name(), JsonDepthErrorHandler))
    return item->error_str();

  item->null_value = false;
  return buffer;
}

String *Item_json_func::val_str(String *) {
  assert(fixed);
  return val_string_from_json(this, &m_string_buffer);
}

static bool get_date_from_json(Item_func *item, MYSQL_TIME *ltime,
                               my_time_flags_t) {
  Json_wrapper wr;
  if (item->val_json(&wr)) return true;
  if (item->null_value) return true;
  return wr.coerce_date(JsonCoercionWarnHandler{item->func_name()},
                        JsonCoercionDeprecatedDefaultHandler{}, ltime,
                        DatetimeConversionFlags(current_thd));
}

bool Item_json_func::get_date(MYSQL_TIME *ltime, my_time_flags_t flags) {
  return get_date_from_json(this, ltime, flags);
}

static bool get_time_from_json(Item_func *item, MYSQL_TIME *ltime) {
  Json_wrapper wr;
  if (item->val_json(&wr)) return true;
  if (item->null_value) return true;
  return wr.coerce_time(JsonCoercionWarnHandler{item->func_name()},
                        JsonCoercionDeprecatedDefaultHandler{}, ltime);
}

bool Item_json_func::get_time(MYSQL_TIME *ltime) {
  return get_time_from_json(this, ltime);
}

longlong val_int_from_json(Item_func *item) {
  Json_wrapper wr;
  if (item->val_json(&wr)) return 0;
  if (item->null_value) return 0;
  return wr.coerce_int(JsonCoercionWarnHandler{item->func_name()});
}

longlong Item_json_func::val_int() { return val_int_from_json(this); }

static double val_real_from_json(Item_func *item) {
  Json_wrapper wr;
  if (item->val_json(&wr)) return 0.0;
  if (item->null_value) return 0.0;
  return wr.coerce_real(JsonCoercionWarnHandler{item->func_name()});
}

double Item_json_func::val_real() { return val_real_from_json(this); }

static my_decimal *val_decimal_from_json(Item_func *item,
                                         my_decimal *decimal_value) {
  Json_wrapper wr;
  if (item->val_json(&wr)) {
    return item->error_decimal(decimal_value);
  }
  if (item->null_value) return nullptr;
  return wr.coerce_decimal(JsonCoercionWarnHandler{item->func_name()},
                           decimal_value);
}

my_decimal *Item_json_func::val_decimal(my_decimal *decimal_value) {
  return val_decimal_from_json(this, decimal_value);
}

/**
  Create a new Json_scalar object, either in memory owned by a
  Json_scalar_holder object or on the heap.

  @param[in,out] scalar  the Json_scalar_holder in which to create the new
                         Json_scalar, or `nullptr` if it should be created
                         on the heap
  @param[in,out] dom     pointer to the Json_scalar if it's created on the heap
  @param[in]     args    the arguments to pass to T's constructor

  @tparam T     the type of object to create; a subclass of Json_scalar
  @tparam Args  type of the arguments to pass to T's constructor

  @retval  false  if successful
  @retval  true   if memory could not be allocated
*/
template <typename T, typename... Args>
static bool create_scalar(Json_scalar_holder *scalar, Json_dom_ptr *dom,
                          Args &&...args) {
  if (scalar != nullptr) {
    scalar->emplace<T>(std::forward<Args>(args)...);
    return false;
  }

  *dom = create_dom_ptr<T>(std::forward<Args>(args)...);
  return dom == nullptr;
}

/**
  Get a JSON value from an SQL scalar value.

  @param[in]     arg         the function argument
  @param[in]     calling_function the name of the calling function
  @param[in,out] value       a scratch area
  @param[in,out] tmp         temporary scratch space for converting strings to
                             the correct charset; only used if accept_string is
                             true and conversion is needed
  @param[out]    wr          the retrieved JSON value
  @param[in,out] scalar      pointer to pre-allocated memory that can be
                             borrowed by the result wrapper to hold the scalar
                             result. If the pointer is NULL, memory will be
                             allocated on the heap.
  @param[in]     scalar_string
                             if true, interpret SQL strings as scalar JSON
                             strings.
                             if false, interpret SQL strings as JSON objects.
                             If conversion fails, return the string as a
                             scalar JSON string instead.

  @return false if we could get a value or NULL, otherwise true
*/
bool sql_scalar_to_json(Item *arg, const char *calling_function, String *value,
                        String *tmp, Json_wrapper *wr,
                        Json_scalar_holder *scalar, bool scalar_string) {
  enum_field_types field_type = get_normalized_field_type(arg);
  /*
    Most items and fields have same actual and resolved types, however e.g
    a dynamic parameter will usually have a different type (integer, string...)
  */
  if (field_type == MYSQL_TYPE_JSON) {
    field_type = arg->actual_data_type();
  }
  Json_dom_ptr dom;

  switch (field_type) {
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_YEAR: {
      longlong i = arg->val_int();
      if (current_thd->is_error()) return true;

      if (arg->null_value) return false;

      if (arg->unsigned_flag) {
        if (create_scalar<Json_uint>(scalar, &dom, i))
          return true; /* purecov: inspected */
      } else {
        if (create_scalar<Json_int>(scalar, &dom, i))
          return true; /* purecov: inspected */
      }

      break;
    }
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIME: {
      const longlong dt = arg->val_temporal_by_field_type();
      if (current_thd->is_error()) return true;
      if (arg->null_value) return false;

      MYSQL_TIME t;
      TIME_from_longlong_datetime_packed(&t, dt);
      t.time_type = field_type_to_timestamp_type(field_type);
      if (create_scalar<Json_datetime>(scalar, &dom, t, field_type))
        return true; /* purecov: inspected */

      break;
    }
    case MYSQL_TYPE_NEWDECIMAL: {
      my_decimal m;
      my_decimal *r = arg->val_decimal(&m);
      if (current_thd->is_error()) return true;
      if (arg->null_value) return false;
      assert(r != nullptr);

      if (create_scalar<Json_decimal>(scalar, &dom, *r))
        return true; /* purecov: inspected */

      break;
    }
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_FLOAT: {
      double d = arg->val_real();
      if (current_thd->is_error()) return true;

      if (arg->null_value) return false;

      if (create_scalar<Json_double>(scalar, &dom, d))
        return true; /* purecov: inspected */

      break;
    }
    case MYSQL_TYPE_GEOMETRY: {
      uint32 geometry_srid;
      String *swkb = arg->val_str(tmp);
      if (current_thd->is_error()) return true;
      if (arg->null_value) return false;
      const bool retval =
          geometry_to_json(wr, swkb, calling_function, INT_MAX32, false, false,
                           false, &geometry_srid);

      /**
        Scalar processing is irrelevant. Geometry types are converted
        to JSON objects.
      */
      return retval;
    }
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB: {
      String *oo = arg->val_str(value);
      if (current_thd->is_error()) return true;

      if (arg->null_value) return false;

      if (create_scalar<Json_opaque>(scalar, &dom, field_type, oo->ptr(),
                                     oo->length()))
        return true; /* purecov: inspected */

      break;
    }
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_STRING: {
      /*
        Wrong charset or Json syntax error (the latter: only if !accept_string,
        in which case a binary character set is our only hope for success).
      */
      String *res = arg->val_str(value);
      if (current_thd->is_error()) return true;

      if (arg->null_value) return false;
      const CHARSET_INFO *cs = res->charset();

      if (cs == &my_charset_bin || cs->mbminlen > 1) {
        /*
         When charset is always multi-byte, store string as OPAQUE value to
         preserve binary encoding. This case is used my multi-valued index,
         when it's created over char field with such charset. SE (InnoDB)
         expect correct binary encoding of such strings. This is similar to
         preserving precision in decimal values for multi-valued index.
         To keep such converted strings apart from other values, they are
         encoded as having MYSQL_TYPE_VAR_STRING which currently isn't used
         in server.
        */
        if (cs->mbminlen > 1) field_type = MYSQL_TYPE_VAR_STRING;
        // BINARY or similar
        if (create_scalar<Json_opaque>(scalar, &dom, field_type, res->ptr(),
                                       res->length()))
          return true; /* purecov: inspected */

        break;
      } else {
        const char *s = res->ptr();
        size_t ss = res->length();

        if (ensure_utf8mb4(*res, tmp, &s, &ss, true)) {
          return true;
        }

        if (scalar_string) {
          if (create_scalar<Json_string>(scalar, &dom, s, ss))
            return true; /* purecov: inspected */
        } else {
          JsonParseDefaultErrorHandler parse_handler(calling_function, 0);
          dom = Json_dom::parse(s, ss, parse_handler, JsonDepthErrorHandler);
          if (dom == nullptr) return true;
        }
      }
      break;
    }
    case MYSQL_TYPE_DECIMAL:  // pre 5.0
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "old decimal type");
      return true;

    case MYSQL_TYPE_NULL:
      // May occur for a parameter that is NULL
      assert(arg->null_value);
      return false;

    case MYSQL_TYPE_JSON:
      assert(false); /* purecov: inspected */

      // fall-through
    default:
      my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
      return true;
  }

  // Exactly one of scalar and dom should be used.
  assert((scalar == nullptr) != (dom == nullptr));
  assert(scalar == nullptr || scalar->get() != nullptr);

  if (scalar != nullptr) {
    /*
      The DOM object lives in memory owned by the caller. Tell the
      wrapper that it's not the owner.
    */
    *wr = Json_wrapper(scalar->get(), true);
    return false;
  }

  *wr = Json_wrapper(std::move(dom));
  return false;
}

// see the contract for this function in item_json_func.h
bool get_json_atom_wrapper(Item **args, uint arg_idx,
                           const char *calling_function, String *value,
                           String *tmp, Json_wrapper *wr,
                           Json_scalar_holder *scalar, bool accept_string) {
  bool result = false;

  Item *const arg = args[arg_idx];

  // First, try to handle simple cases: NULL value and JSON type
  bool has_value = false;
  if (json_value(arg, wr, &has_value)) {
    return true;
  }
  if (has_value) {
    return false;
  }

  try {
    // boolean operators should produce boolean values
    if (arg->is_bool_func()) {
      const bool boolean_value = arg->val_int() != 0;
      if (current_thd->is_error()) return true;
      Json_dom_ptr boolean_dom;
      if (create_scalar<Json_boolean>(scalar, &boolean_dom, boolean_value))
        return true; /* purecov: inspected */

      if (scalar != nullptr) {
        /*
          The DOM object lives in memory owned by the caller. Tell the
          wrapper that it's not the owner.
        */
        *wr = Json_wrapper(scalar->get());
        wr->set_alias();
        return false;
      }

      *wr = Json_wrapper(std::move(boolean_dom));
      return false;
    }

    /*
      Allow other types as first-class or opaque JSON values.
      But how to determine what the type is? We do a best effort...
    */
    result = sql_scalar_to_json(arg, calling_function, value, tmp, wr, scalar,
                                accept_string);

  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(calling_function);
    return true;
    /* purecov: end */
  }

  return result;
}

bool get_atom_null_as_null(Item **args, uint arg_idx,
                           const char *calling_function, String *value,
                           String *tmp, Json_wrapper *wr) {
  if (get_json_atom_wrapper(args, arg_idx, calling_function, value, tmp, wr,
                            nullptr, true))
    return true;

  if (args[arg_idx]->null_value) {
    *wr = Json_wrapper(new (std::nothrow) Json_null());
  }

  return false;
}

bool Item_typecast_json::val_json(Json_wrapper *wr) {
  assert(fixed);

  Json_dom_ptr dom;  //@< if non-null we want a DOM from parse

  if (args[0]->data_type() == MYSQL_TYPE_NULL) {
    null_value = true;
    return false;
  }

  if (args[0]->data_type() == MYSQL_TYPE_JSON) {
    bool has_value;
    if (json_value(args[0], wr, &has_value)) return error_json();

    assert(has_value);
    null_value = args[0]->null_value;
    return false;
  }

  bool valid;
  if (json_is_valid(args, 0, &m_value, func_name(), &dom, false, &valid))
    return error_json();

  if (valid) {
    if (args[0]->null_value) {
      null_value = true;
      return false;
    }
    // We were able to parse a JSON value from a string.
    assert(dom);
    // Pass on the DOM wrapped
    *wr = Json_wrapper(std::move(dom));
    null_value = false;
    return false;
  }

  // Not a non-binary string, nor a JSON value, wrap the rest

  if (get_json_atom_wrapper(args, 0, func_name(), &m_value,
                            &m_conversion_buffer, wr, nullptr, true))
    return error_json();

  null_value = args[0]->null_value;

  return false;
}

void Item_typecast_json::print(const THD *thd, String *str,
                               enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  str->append(')');
}

longlong Item_func_json_length::val_int() {
  assert(fixed);
  longlong result = 0;

  Json_wrapper wrapper;

  try {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value) {
      return error_int();
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  result = wrapper.length();

  null_value = false;
  return result;
}

longlong Item_func_json_depth::val_int() {
  assert(fixed);
  Json_wrapper wrapper;

  try {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper))
      return error_int();
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  null_value = args[0]->null_value;
  if (null_value) return 0;

  const Json_dom *dom = wrapper.to_dom();
  return dom == nullptr ? error_int() : dom->depth();
}

bool Item_func_json_keys::val_json(Json_wrapper *wr) {
  assert(fixed);

  Json_wrapper wrapper;

  try {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper))
      return error_json();
    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    if (arg_count > 1) {
      if (m_path_cache.parse_and_cache_path(current_thd, args, 1, true))
        return error_json();
      const Json_path *path = m_path_cache.get_path(1);
      if (path == nullptr) {
        null_value = true;
        return false;
      }

      Json_wrapper_vector hits(key_memory_JSON);
      if (wrapper.seek(*path, path->leg_count(), &hits, false, true))
        return error_json(); /* purecov: inspected */

      if (hits.size() != 1) {
        null_value = true;
        return false;
      }

      wrapper = std::move(hits[0]);
    }

    if (wrapper.type() != enum_json_type::J_OBJECT) {
      null_value = true;
      return false;
    }

    // We have located a JSON object value, now collect its keys
    // and return them as a JSON array.
    Json_array_ptr res(new (std::nothrow) Json_array());
    if (res == nullptr) return error_json(); /* purecov: inspected */
    for (const auto &i : Json_object_wrapper(wrapper)) {
      if (res->append_alias(new (std::nothrow) Json_string(i.first)))
        return error_json(); /* purecov: inspected */
    }
    *wr = Json_wrapper(std::move(res));
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

bool Item_func_json_extract::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    Json_wrapper w;

    // multiple paths means multiple possible matches
    bool could_return_multiple_matches = (arg_count > 2);

    // collect results here
    Json_wrapper_vector v(key_memory_JSON);

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &w))
      return error_json();

    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    const THD *const thd = current_thd;

    for (uint32 i = 1; i < arg_count; ++i) {
      if (m_path_cache.parse_and_cache_path(thd, args, i, false))
        return error_json();
      const Json_path *path = m_path_cache.get_path(i);
      if (path == nullptr) {
        null_value = true;
        return false;
      }

      could_return_multiple_matches |= path->can_match_many();

      if (w.seek(*path, path->leg_count(), &v, true, false))
        return error_json(); /* purecov: inspected */
    }

    if (v.size() == 0) {
      null_value = true;
      return false;
    }

    if (could_return_multiple_matches) {
      Json_array_ptr a(new (std::nothrow) Json_array());
      if (a == nullptr) return error_json(); /* purecov: inspected */
      for (Json_wrapper &ww : v) {
        if (a->append_clone(ww.to_dom()))
          return error_json(); /* purecov: inspected */
      }
      *wr = Json_wrapper(std::move(a));
    } else  // one path, no ellipsis or wildcard
    {
      // there should only be one match
      assert(v.size() == 1);
      *wr = std::move(v[0]);
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

bool Item_func_json_extract::eq(const Item *item) const {
  if (this == item) return true;
  if (item->type() != FUNC_ITEM) return false;
  const auto item_func = down_cast<const Item_func *>(item);
  if (arg_count != item_func->arg_count ||
      strcmp(func_name(), item_func->func_name()) != 0)
    return false;

  /*
    JSON_EXTRACT doesn't care about the collation of its arguments. String
    literal arguments are considered equal if they have the same character
    set and binary contents, even if their collations differ.
  */
  const auto item_json = down_cast<const Item_func_json_extract *>(item);

  for (uint i = 0; i < arg_count; i++) {
    const Item *a = args[i]->unwrap_for_eq();
    const Item *b = item_json->args[i]->unwrap_for_eq();
    if (a->type() == STRING_ITEM && b->type() == STRING_ITEM &&
        a->const_item() && b->const_item()) {
      if (!my_charset_same(a->collation.collation, b->collation.collation))
        return false;
      String str1, str2;
      if (stringcmp(const_cast<Item *>(a)->val_str(&str1),
                    const_cast<Item *>(b)->val_str(&str2))) {
        return false;
      }
    } else {
      if (!a->eq(b)) return false;
    }
  }
  return true;
}

bool Item_func_modify_json_in_path::resolve_type(THD *thd) {
  if (Item_json_func::resolve_type(thd)) return true;
  if (reject_vector_args()) return true;
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_JSON)) return true;
  if (param_type_is_default(thd, 1, -1, 2, MYSQL_TYPE_VARCHAR)) return true;
  if (param_type_is_default(thd, 2, -1, 2, MYSQL_TYPE_JSON)) return true;
  for (uint i = 2; i < arg_count; i += 2) {
    args[i]->mark_json_as_scalar();
  }
  not_null_tables_cache = calculate_not_null_tables();
  return false;
}

void Item_func_modify_json_in_path::update_used_tables() {
  Item_json_func::update_used_tables();
  not_null_tables_cache = calculate_not_null_tables();
}

table_map Item_func_modify_json_in_path::calculate_not_null_tables() const {
  // If the first argument (the JSON document) is NULL, the function returns
  // NULL.
  table_map tables = args[0]->not_null_tables();
  // If any of the JSON path arguments is NULL, the function returns NULL.
  for (uint i = 1; i < arg_count; i += 2) {
    tables |= args[i]->not_null_tables();
  }
  return tables;
}

#ifndef NDEBUG
/**
  Is this a path that could possibly return the root node of a JSON document?

  A path that returns the root node must be on one of the following forms:
  - the root ('$'), or
  - a sequence of array cells at index 0 or `last` that any non-array element
    at the top level could have been autowrapped to, i.e. '$[0]' or
    '$[0][0]...[0]'.

  @see Json_path_leg::is_autowrap

  @param begin  the beginning of the path
  @param end    the end of the path (exclusive)
  @return true if the path may match the root, false otherwise
*/
static bool possible_root_path(const Json_path_iterator &begin,
                               const Json_path_iterator &end) {
  auto is_autowrap = [](const Json_path_leg *leg) {
    return leg->is_autowrap();
  };
  return std::all_of(begin, end, is_autowrap);
}
#endif  // NDEBUG

bool Item_func_json_array_append::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();
    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    const THD *thd = current_thd;
    for (uint32 i = 1; i < arg_count; i += 2) {
      // Need a DOM to be able to manipulate arrays
      Json_dom *doc = docw.to_dom();
      if (!doc) return error_json(); /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(thd, args, i, true))
        return error_json();
      const Json_path *path = m_path_cache.get_path(i);
      if (path == nullptr) {
        null_value = true;
        return false;
      }

      Json_dom_vector hits(key_memory_JSON);
      if (doc->seek(*path, path->leg_count(), &hits, true, true))
        return error_json(); /* purecov: inspected */

      if (hits.empty()) continue;

      // Paths with wildcards and ranges are rejected, so expect one hit.
      assert(hits.size() == 1);
      Json_dom *hit = hits[0];

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer, &valuew))
        return error_json();

      Json_dom_ptr val_dom(valuew.to_dom());
      valuew.set_alias();  // we have taken over the DOM

      if (hit->json_type() == enum_json_type::J_ARRAY) {
        Json_array *arr = down_cast<Json_array *>(hit);
        if (arr->append_alias(std::move(val_dom)))
          return error_json(); /* purecov: inspected */
      } else {
        Json_array_ptr arr(new (std::nothrow) Json_array());
        if (arr == nullptr || arr->append_clone(hit) ||
            arr->append_alias(std::move(val_dom))) {
          return error_json(); /* purecov: inspected */
        }
        /*
          This value will replace the old document we found using path, since
          we did an auto-wrap. If this is root, this is trivial, but if it's
          inside an array or object, we need to find the parent DOM to be
          able to replace it in situ.
        */
        Json_container *parent = hit->parent();
        if (parent == nullptr)  // root
        {
          assert(possible_root_path(path->begin(), path->end()));
          docw = Json_wrapper(std::move(arr));
        } else {
          parent->replace_dom_in_container(hit, std::move(arr));
        }
      }
    }

    // docw still owns the augmented doc, so hand it over to result
    *wr = std::move(docw);
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

bool Item_func_json_insert::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    const THD *thd = current_thd;
    for (uint32 i = 1; i < arg_count; i += 2) {
      // Need a DOM to be able to manipulate arrays and objects
      Json_dom *doc = docw.to_dom();
      if (!doc) return error_json(); /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(thd, args, i, true))
        return error_json();
      const Json_path *path = m_path_cache.get_path(i);
      if (path == nullptr) {
        null_value = true;
        return false;
      }

      // Cannot insert the root element.
      if (path->leg_count() == 0) continue;

      Json_dom_vector hits(key_memory_JSON);
      if (doc->seek(*path, path->leg_count(), &hits, true, true))
        return error_json(); /* purecov: inspected */

      // If it already exists, there is nothing to do.
      if (!hits.empty()) continue;

      /*
        Need to look one step up the path: if we are specifying an array slot
        we need to find the array. If we are specifying an object element, we
        need to find the object. In both cases so we can insert into them.

        Seek again without considering the last path leg.
      */
      const Json_path_leg *leg = path->last_leg();
      if (doc->seek(*path, path->leg_count() - 1, &hits, true, true))
        return error_json(); /* purecov: inspected */

      if (hits.empty()) {
        // no unique object found at parent position, so bail out
        continue;
      }

      // We found *something* at that parent path

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer, &valuew)) {
        return error_json();
      }

      // Paths with wildcards and ranges are rejected, so expect one hit.
      assert(hits.size() == 1);
      Json_dom *hit = hits[0];

      // What did we specify in the path, object or array?
      if (leg->get_type() == jpl_array_cell) {
        // We specified an array, what did we find at that position?
        if (hit->json_type() == enum_json_type::J_ARRAY) {
          // We found an array, so either prepend or append.
          Json_array *arr = down_cast<Json_array *>(hit);
          size_t pos = leg->first_array_index(arr->size()).position();
          if (arr->insert_alias(pos, valuew.clone_dom()))
            return error_json(); /* purecov: inspected */
        } else if (!leg->is_autowrap()) {
          /*
            Found a scalar or object and we didn't specify position 0 or last:
            auto-wrap it and either prepend or append.
          */
          size_t pos = leg->first_array_index(1).position();
          Json_array_ptr newarr(new (std::nothrow) Json_array());
          if (newarr == nullptr ||
              newarr->append_clone(hit) /* auto-wrap this */ ||
              newarr->insert_alias(pos, valuew.clone_dom())) {
            return error_json(); /* purecov: inspected */
          }

          /*
            Now we need this value to replace the old document we found using
            path. If this is root, this is trivial, but if it's inside an
            array or object, we need to find the parent DOM to be able to
            replace it in situ.
          */
          Json_container *parent = hit->parent();
          if (parent == nullptr)  // root
          {
            assert(possible_root_path(path->begin(), path->end() - 1));
            docw = Json_wrapper(std::move(newarr));
          } else {
            parent->replace_dom_in_container(hit, std::move(newarr));
          }
        }
      } else if (leg->get_type() == jpl_member &&
                 hit->json_type() == enum_json_type::J_OBJECT) {
        Json_object *o = down_cast<Json_object *>(hit);
        if (o->add_clone(leg->get_member_name(), valuew.to_dom()))
          return error_json(); /* purecov: inspected */
      }
    }  // end of loop through paths
    // docw still owns the augmented doc, so hand it over to result
    *wr = std::move(docw);

  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

bool Item_func_json_array_insert::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    const THD *thd = current_thd;
    for (uint32 i = 1; i < arg_count; i += 2) {
      // Need a DOM to be able to manipulate arrays and objects
      Json_dom *doc = docw.to_dom();
      if (!doc) return error_json(); /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(thd, args, i, true))
        return error_json();
      const Json_path *path = m_path_cache.get_path(i);
      if (path == nullptr) {
        null_value = true;
        return false;
      }

      // the path must end in a cell identifier
      if (path->leg_count() == 0 ||
          path->last_leg()->get_type() != jpl_array_cell) {
        my_error(ER_INVALID_JSON_PATH_ARRAY_CELL, MYF(0));
        return error_json();
      }

      /*
        Need to look one step up the path: we need to find the array.

        Seek without the last path leg.
      */
      Json_dom_vector hits(key_memory_JSON);
      const Json_path_leg *leg = path->last_leg();
      if (doc->seek(*path, path->leg_count() - 1, &hits, false, true))
        return error_json(); /* purecov: inspected */

      if (hits.empty()) {
        // no unique object found at parent position, so bail out
        continue;
      }

      // We found *something* at that parent path

      // Paths with wildcards and ranges are rejected, so expect one hit.
      assert(hits.size() == 1);
      Json_dom *hit = hits[0];

      // NOP if parent is not an array
      if (hit->json_type() != enum_json_type::J_ARRAY) continue;

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer, &valuew)) {
        return error_json();
      }

      // Insert the value at that location.
      Json_array *arr = down_cast<Json_array *>(hit);
      assert(leg->get_type() == jpl_array_cell);
      size_t pos = leg->first_array_index(arr->size()).position();
      if (arr->insert_alias(pos, valuew.clone_dom()))
        return error_json(); /* purecov: inspected */

    }  // end of loop through paths
    // docw still owns the augmented doc, so hand it over to result
    *wr = std::move(docw);

  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

void Item_json_func::mark_for_partial_update(const Field_json *field) {
  assert(supports_partial_update(field));
  m_partial_update_column = field;

  // Peel off the Item_view_ref if we are updating an updatable view.
  Item *arg0 = args[0]->real_item();

  // supports_partial_update() returns true only when args[0] is an Item_field
  // or a subclass of Item_json_func (or a view ref wrapping any of those), so
  // we can safely assume it has one of those types when we get here.
  if (arg0->type() == FIELD_ITEM) {
    assert(down_cast<Item_field *>(arg0)->field == field);
  } else {
    down_cast<Item_json_func *>(arg0)->mark_for_partial_update(field);
  }
}

bool Item_json_func::supports_partial_update(const Field_json *field) const {
  if (!can_use_in_partial_update()) return false;

  /*
    This JSON_SET, JSON_REPLACE or JSON_REMOVE expression might be used for
    partial update if the first argument is a JSON column which is the same as
    the target column of the update operation, or if the first argument is
    another JSON_SET, JSON_REPLACE or JSON_REMOVE expression which has the
    target column as its first argument.
  */

  Item *arg0 = args[0];
  if (arg0->type() == Item::REF_ITEM &&
      down_cast<Item_ref *>(arg0)->ref_type() == Item_ref::VIEW_REF) {
    // If the target table is an updatable view, look at the column in the base
    // table.
    arg0 = arg0->real_item();
  }

  if (arg0->type() == FIELD_ITEM)
    return down_cast<const Item_field *>(arg0)->field == field;

  return arg0->supports_partial_update(field);
}

static void disable_logical_diffs(const Field_json *field) {
  field->table->disable_logical_diffs_for_current_row(field);
}

static void disable_binary_diffs(const Field_json *field) {
  field->table->disable_binary_diffs_for_current_row(field);
}

/**
  Common implementation for JSON_SET and JSON_REPLACE
*/
bool Item_func_json_set_replace::val_json(Json_wrapper *wr) {
  const THD *thd = current_thd;

  // Should we collect binary or logical diffs? We'll see later...
  bool binary_diffs = false;
  bool logical_diffs = false;

  try {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    /*
      Check if this function is called from an UPDATE statement in a way
      that could make partial update possible. For example:
      UPDATE t SET json_col = JSON_REPLACE(json_col, '$.pet', 'rabbit')

      If partial update was disabled for this column while evaluating the
      first argument, don't attempt to perform partial update here.
    */
    TABLE *table = nullptr;
    if (m_partial_update_column != nullptr) {
      table = m_partial_update_column->table;
      binary_diffs = table->is_binary_diff_enabled(m_partial_update_column);
      logical_diffs = table->is_logical_diff_enabled(m_partial_update_column);
    }

    if (args[0]->null_value) goto return_null;

    String *partial_update_buffer = nullptr;
    if (binary_diffs) {
      partial_update_buffer = table->get_partial_update_buffer();

      // Reset the buffer in the innermost call.
      if (args[0]->real_item()->type() == FIELD_ITEM)
        partial_update_buffer->length(0);
    }

    for (uint32 i = 1; i < arg_count; i += 2) {
      if (m_path_cache.parse_and_cache_path(thd, args, i, true))
        return error_json();
      const Json_path *current_path = m_path_cache.get_path(i);
      if (current_path == nullptr) goto return_null;

      // Clone the path, stripping off redundant auto-wrapping.
      if (clone_without_autowrapping(current_path, &m_path, &docw,
                                     key_memory_JSON)) {
        return error_json();
      }

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer, &valuew))
        return error_json();

      if (binary_diffs) {
        bool partially_updated = false;
        bool replaced_path = false;
        if (docw.attempt_binary_update(m_partial_update_column, m_path, &valuew,
                                       !m_json_set, partial_update_buffer,
                                       &partially_updated, &replaced_path))
          return error_json(); /* purecov: inspected */

        if (partially_updated) {
          if (logical_diffs && replaced_path)
            table->add_logical_diff(m_partial_update_column, m_path,
                                    enum_json_diff_operation::REPLACE, &valuew);
          /*
            Partial update of the binary value was successful, and docw has
            been updated accordingly. Go on to updating the next path.
          */
          continue;
        }

        binary_diffs = false;
        disable_binary_diffs(m_partial_update_column);
      }

      // Need a DOM to be able to manipulate arrays and objects
      Json_dom *doc = docw.to_dom();
      if (!doc) return error_json(); /* purecov: inspected */

      Json_dom_vector hits(key_memory_JSON);
      if (doc->seek(m_path, m_path.leg_count(), &hits, false, true))
        return error_json(); /* purecov: inspected */

      if (hits.empty()) {
        // Replace semantics, so skip if the path is not present.
        if (!m_json_set) continue;

        /*
          Need to look one step up the path: if we are specifying an array slot
          we need to find the array. If we are specifying an object element, we
          need to find the object. In both cases so we can insert into them.

          Remove the first path leg and search again.
        */
        const Json_path_leg *leg = m_path.last_leg();
        if (doc->seek(m_path, m_path.leg_count() - 1, &hits, false, true))
          return error_json(); /* purecov: inspected */

        if (hits.empty()) {
          // no unique object found at parent position, so bail out
          continue;
        }

        // We don't allow wildcards in the path, so there can only be one hit.
        assert(hits.size() == 1);
        Json_dom *hit = hits[0];

        // We now have either an array or an object in the parent's path
        if (leg->get_type() == jpl_array_cell) {
          if (hit->json_type() == enum_json_type::J_ARRAY) {
            /*
              The array element was not found, so either prepend or
              append the new value.
            */
            Json_array *arr = down_cast<Json_array *>(hit);
            size_t pos = leg->first_array_index(arr->size()).position();
            if (arr->insert_alias(pos, valuew.clone_dom()))
              return error_json(); /* purecov: inspected */

            if (logical_diffs)
              table->add_logical_diff(m_partial_update_column, m_path,
                                      enum_json_diff_operation::INSERT,
                                      &valuew);
          } else {
            /*
              Found a scalar or object, auto-wrap it and make it the first
              element in a new array, unless the new value specifies position
              0, in which case the old value should get replaced. Don't expect
              array position to be 0 here, though, as such legs should have
              been removed by the call to clone_without_autowrapping() above.
            */
            assert(!leg->is_autowrap());
            Json_array_ptr newarr = create_dom_ptr<Json_array>();
            size_t pos = leg->first_array_index(1).position();
            if (newarr == nullptr || newarr->append_clone(hit) ||
                newarr->insert_alias(pos, valuew.clone_dom())) {
              return error_json(); /* purecov: inspected */
            }

            /*
              Now we need this value to replace the old document we found
              using path. If this is root, this is trivial, but if it's
              inside an array or object, we need to find the parent DOM to be
              able to replace it in situ.
            */
            Json_container *parent = hit->parent();
            if (parent == nullptr)  // root
            {
              docw = Json_wrapper(std::move(newarr));

              // No point in partial update when we replace the entire document.
              if (logical_diffs) {
                disable_logical_diffs(m_partial_update_column);
                logical_diffs = false;
              }
            } else {
              if (logical_diffs) {
                Json_wrapper array_wrapper(newarr.get());
                array_wrapper.set_alias();
                table->add_logical_diff(
                    m_partial_update_column, hit->get_location(),
                    enum_json_diff_operation::REPLACE, &array_wrapper);
              }
              parent->replace_dom_in_container(hit, std::move(newarr));
            }
          }
        } else if (leg->get_type() == jpl_member &&
                   hit->json_type() == enum_json_type::J_OBJECT) {
          Json_object *o = down_cast<Json_object *>(hit);
          if (o->add_clone(leg->get_member_name(), valuew.to_dom()))
            return error_json(); /* purecov: inspected */

          if (logical_diffs)
            table->add_logical_diff(m_partial_update_column, m_path,
                                    enum_json_diff_operation::INSERT, &valuew);
        }
      } else {
        // We found one value, so replace semantics.
        assert(hits.size() == 1);
        Json_dom *child = hits[0];
        Json_container *parent = child->parent();
        if (parent == nullptr) {
          Json_dom_ptr dom = valuew.clone_dom();
          if (dom == nullptr) return error_json(); /* purecov: inspected */
          docw = Json_wrapper(std::move(dom));

          // No point in partial update when we replace the entire document.
          if (logical_diffs) {
            disable_logical_diffs(m_partial_update_column);
            logical_diffs = false;
          }
        } else {
          Json_dom_ptr dom = valuew.clone_dom();
          if (!dom) return error_json(); /* purecov: inspected */
          parent->replace_dom_in_container(child, std::move(dom));

          if (logical_diffs)
            table->add_logical_diff(m_partial_update_column, m_path,
                                    enum_json_diff_operation::REPLACE, &valuew);
        }
      }  // if: found 1 else more values
    }    // do: functions argument list run-though

    // docw still owns the augmented doc, so hand it over to result
    *wr = std::move(docw);
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;

return_null:
  /*
    When we return NULL, there is no point in doing partial update, as the
    entire document changes anyway. Disable binary and logical diffs.
  */
  if (binary_diffs) disable_binary_diffs(m_partial_update_column);
  if (logical_diffs) disable_logical_diffs(m_partial_update_column);
  null_value = true;
  return false;
}

bool Item_func_json_array::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    Json_array *arr = new (std::nothrow) Json_array();
    if (!arr) return error_json(); /* purecov: inspected */
    const Json_wrapper docw(arr);

    for (uint32 i = 0; i < arg_count; ++i) {
      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i, func_name(), &m_value,
                                &m_conversion_buffer, &valuew)) {
        return error_json();
      }

      Json_dom_ptr val_dom(valuew.to_dom());
      valuew.set_alias();  // release the DOM

      if (arr->append_alias(std::move(val_dom)))
        return error_json(); /* purecov: inspected */
    }

    // docw still owns the augmented doc, so hand it over to result
    *wr = std::move(docw);
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

bool Item_func_json_row_object::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    Json_object *object = new (std::nothrow) Json_object();
    if (!object) return error_json(); /* purecov: inspected */
    Json_wrapper docw(object);

    const THD *thd = current_thd;
    for (uint32 i = 0; i < arg_count; ++i) {
      /*
        arguments come in pairs. we have already verified that there
        are an even number of args.
      */
      const uint32 key_idx = i++;
      const uint32 value_idx = i;

      // key
      Item *key_item = args[key_idx];
      char buff[MAX_FIELD_WIDTH];
      String utf8_res(buff, sizeof(buff), &my_charset_utf8mb4_bin);
      const char *safep;   // contents of key_item, possibly converted
      size_t safe_length;  // length of safep

      if (get_json_object_member_name(thd, key_item, &tmp_key_value, &utf8_res,
                                      &safep, &safe_length))
        return error_json();

      const std::string key(safep, safe_length);

      // value
      Json_wrapper valuew;
      if (get_atom_null_as_null(args, value_idx, func_name(), &m_value,
                                &m_conversion_buffer, &valuew)) {
        return error_json();
      }

      Json_dom_ptr val_dom(valuew.to_dom());
      valuew.set_alias();  // we have taken over the DOM

      if (object->add_alias(key, std::move(val_dom)))
        return error_json(); /* purecov: inspected */
    }

    // docw still owns the augmented doc, so hand it over to result
    *wr = std::move(docw);
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value = false;
  return false;
}

bool Item_func_json_search::fix_fields(THD *thd, Item **items) {
  if (Item_json_func::fix_fields(thd, items)) return true;

  // Fabricate a LIKE node

  m_source_string_item = new Item_string(&my_charset_utf8mb4_bin);
  if (m_source_string_item == nullptr) {
    return true; /* purecov: inspected */
  }

  Item *like_string_item = args[2];

  // Get the escape character, if any
  if (arg_count > 3) {
    Item *escape_item = args[3];
    /*
      For a standalone LIKE expression,
      the escape clause only has to be constant during execution.
      However, we require a stronger condition: it must be constant.
      That means that we can evaluate the escape clause at resolution time
      and copy the results from the JSON_SEARCH() args into the arguments
      for the LIKE node which we're fabricating.
    */
    if (!escape_item->const_item()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }
    m_like_node =
        new Item_func_like(m_source_string_item, like_string_item, escape_item);
  } else {
    m_like_node = new Item_func_like(m_source_string_item, like_string_item);
  }

  if (m_like_node == nullptr) return true; /* purecov: inspected */

  Item *like = m_like_node;
  if (m_like_node->fix_fields(thd, &like)) return true;

  // Don't expect the LIKE node to be replaced during resolving.
  assert(like == m_like_node);

  // resolving the LIKE node may overwrite its arguments
  Item **resolved_like_args = m_like_node->arguments();
  m_source_string_item = down_cast<Item_string *>(resolved_like_args[0]);

  return false;
}

void Item_func_json_search::cleanup() {
  Item_json_func::cleanup();

  m_cached_ooa = ooa_uninitialized;
}

typedef Prealloced_array<std::string, 16> String_set;

/**
   Recursive function to find the string values, nested inside
   a json document, which satisfy the LIKE condition. As matches
   are found, their path locations are added to an evolving
   vector of matches.

   @param[in] wrapper A subdocument of the original document.
   @param[in] path The path location of the subdocument
   @param[in,out] matches The evolving vector of matches.
   @param[in,out] duplicates Sorted set of paths found already, which is used
                             to avoid inserting duplicates into @a matches.
   @param[in] one_match If true, then terminate search after first match.
   @param[in] like_node The LIKE node that's evaluated on the string values.
   @param[in] source_string The input string item of the LIKE node.
   @retval false on success
   @retval true on failure
*/
static bool find_matches(const Json_wrapper &wrapper, String *path,
                         Json_dom_vector *matches, String_set *duplicates,
                         bool one_match, Item *like_node,
                         Item_string *source_string) {
  switch (wrapper.type()) {
    case enum_json_type::J_STRING: {
      if (one_match && !matches->empty()) {
        return false;
      }

      // Evaluate the LIKE node on the JSON string.
      const char *data = wrapper.get_data();
      const uint len = static_cast<uint>(wrapper.get_data_length());
      source_string->set_str_with_copy(data, len, &my_charset_utf8mb4_bin);
      if (like_node->val_int()) {
        // Got a match with the LIKE node. Save the path of the JSON string.
        std::pair<String_set::iterator, bool> res =
            duplicates->insert_unique(std::string(path->ptr(), path->length()));

        if (res.second) {
          Json_string *jstr = new (std::nothrow) Json_string(*res.first);
          if (!jstr || matches->push_back(jstr))
            return true; /* purecov: inspected */
        }
      }
      break;
    }

    case enum_json_type::J_OBJECT: {
      const size_t path_length = path->length();
      for (const auto &jwot : Json_object_wrapper(wrapper)) {
        // recurse with the member added to the path
        if (Json_path_leg(jwot.first).to_string(path) ||
            find_matches(jwot.second, path, matches, duplicates, one_match,
                         like_node, source_string))
          return true;              /* purecov: inspected */
        path->length(path_length);  // restore the path

        if (one_match && !matches->empty()) {
          return false;
        }
      }
      break;
    }

    case enum_json_type::J_ARRAY: {
      const size_t path_length = path->length();
      for (size_t idx = 0; idx < wrapper.length(); idx++) {
        // recurse with the array index added to the path
        if (Json_path_leg(idx).to_string(path) ||
            find_matches(wrapper[idx], path, matches, duplicates, one_match,
                         like_node, source_string))
          return true;              /* purecov: inspected */
        path->length(path_length);  // restore the path

        if (one_match && !matches->empty()) {
          return false;
        }
      }
      break;
    }

    default: {
      break;
    }
  }  // end switch on wrapper type

  return false;
}

bool Item_func_json_search::val_json(Json_wrapper *wr) {
  assert(fixed);

  Json_dom_vector matches(key_memory_JSON);

  try {
    /*
      The "duplicates" set is used by find_matches() to track which
      paths it has added to "matches", so that it doesn't return the
      same path multiple times if JSON_SEARCH is called with wildcard
      paths or multiple path arguments.
    */
    String_set duplicates(key_memory_JSON);
    Json_wrapper docw;

    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    // arg 1 is the oneOrAll arg
    THD *const thd = current_thd;
    bool one_match;
    switch (parse_and_cache_ooa(thd, args[1], &m_cached_ooa, func_name())) {
      case ooa_all: {
        one_match = false;
        break;
      }
      case ooa_one: {
        one_match = true;
        break;
      }
      case ooa_null: {
        null_value = true;
        return false;
      }
      default: {
        return error_json();
      }
    }

    // arg 2 is the search string

    // arg 3 is the optional escape character

    // the remaining arguments are path expressions
    StringBuffer<STRING_BUFFER_USUAL_SIZE> path_str;
    if (arg_count < 5)  // no user-supplied path expressions
    {
      path_str.append('$');
      if (find_matches(docw, &path_str, &matches, &duplicates, one_match,
                       m_like_node, m_source_string_item))
        return error_json(); /* purecov: inspected */
    } else                   // user-supplied path expressions
    {
      Json_wrapper_vector hits(key_memory_JSON);

      // validate the user-supplied path expressions
      for (uint32 i = 4; i < arg_count; ++i) {
        if (m_path_cache.parse_and_cache_path(thd, args, i, false))
          return error_json();
        if (m_path_cache.get_path(i) == nullptr) {
          null_value = true;
          return false;
        }
      }

      // find the matches for each of the user-supplied path expressions
      for (uint32 i = 4; i < arg_count; ++i) {
        if (one_match && (matches.size() > 0)) {
          break;
        }

        const Json_path *path = m_path_cache.get_path(i);

        /*
          If there are wildcards in the path, then we need to
          compute the full path to the subdocument. We can only
          do this on doms.
        */
        if (path->can_match_many()) {
          Json_dom *dom = docw.to_dom();
          if (!dom) return error_json(); /* purecov: inspected */
          Json_dom_vector dom_hits(key_memory_JSON);

          if (dom->seek(*path, path->leg_count(), &dom_hits, false, false))
            return error_json(); /* purecov: inspected */

          for (Json_dom *subdocument : dom_hits) {
            if (one_match && (matches.size() > 0)) {
              break;
            }

            path_str.length(0);
            if (subdocument->get_location().to_string(&path_str))
              return error_json(); /* purecov: inspected */

            Json_wrapper subdocument_wrapper(subdocument);
            subdocument_wrapper.set_alias();

            if (find_matches(subdocument_wrapper, &path_str, &matches,
                             &duplicates, one_match, m_like_node,
                             m_source_string_item))
              return error_json(); /* purecov: inspected */
          }                        // end of loop through hits
        } else                     // no wildcards in the path
        {
          if (one_match && (matches.size() > 0)) break;

          hits.clear();
          if (docw.seek(*path, path->leg_count(), &hits, false, false))
            return error_json(); /* purecov: inspected */

          if (hits.empty()) continue;

          assert(hits.size() == 1);  // no wildcards

          path_str.length(0);
          if (path->to_string(&path_str))
            return error_json(); /* purecov: inspected */

          if (find_matches(hits[0], &path_str, &matches, &duplicates, one_match,
                           m_like_node, m_source_string_item))
            return error_json(); /* purecov: inspected */

        }  // end if the user-supplied path expression has wildcards
      }    // end of loop through user-supplied path expressions
    }      // end if there are user-supplied path expressions

  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  if (matches.size() == 0) {
    null_value = true;
    return false;
  } else if (matches.size() == 1) {
    *wr = Json_wrapper(matches[0]);
  } else {
    Json_array_ptr array(new (std::nothrow) Json_array());
    if (array == nullptr) return error_json(); /* purecov: inspected */
    for (auto match : matches) {
      if (array->append_alias(match))
        return error_json(); /* purecov: inspected */
    }

    *wr = Json_wrapper(std::move(array));
  }

  null_value = false;
  return false;
}

bool Item_func_json_remove::val_json(Json_wrapper *wr) {
  assert(fixed);

  Json_wrapper wrapper;
  const uint32 path_count = arg_count - 1;
  null_value = false;

  // Should we collect binary or logical diffs? We'll see later...
  bool binary_diffs = false;
  bool logical_diffs = false;
  TABLE *table = nullptr;

  try {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper))
      return error_json();

    /*
      Check if this function is called from an UPDATE statement in a way that
      could make partial update possible. For example:
      UPDATE t SET json_col = JSON_REMOVE(json_col, '$.name')

      If partial update was disabled for this column while evaluating the
      first argument, don't attempt to perform partial update here.
    */
    if (m_partial_update_column != nullptr) {
      table = m_partial_update_column->table;
      binary_diffs = table->is_binary_diff_enabled(m_partial_update_column);
      logical_diffs = table->is_logical_diff_enabled(m_partial_update_column);
    }

    if (args[0]->null_value) {
      if (binary_diffs) disable_binary_diffs(m_partial_update_column);
      if (logical_diffs) disable_logical_diffs(m_partial_update_column);
      null_value = true;
      return false;
    }

    const THD *const thd = current_thd;
    for (uint path_idx = 0; path_idx < path_count; ++path_idx) {
      if (m_path_cache.parse_and_cache_path(thd, args, path_idx + 1, true))
        return error_json();
      if (m_path_cache.get_path(path_idx + 1) == nullptr) {
        if (binary_diffs) disable_binary_diffs(m_partial_update_column);
        if (logical_diffs) disable_logical_diffs(m_partial_update_column);
        null_value = true;
        return false;
      }
    }

  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  for (uint path_idx = 0; path_idx < path_count; ++path_idx) {
    const Json_path *path = m_path_cache.get_path(path_idx + 1);
    if (path->leg_count() == 0) {
      my_error(ER_JSON_VACUOUS_PATH, MYF(0));
      return error_json();
    }
  }

  // good document, good paths. do some work

  Json_dom *dom = nullptr;
  String *partial_update_buffer = nullptr;
  if (binary_diffs) {
    assert(!wrapper.is_dom());
    partial_update_buffer = table->get_partial_update_buffer();
    // Reset the buffer in the innermost call.
    if (args[0]->real_item()->type() == FIELD_ITEM)
      partial_update_buffer->length(0);
  } else {
    // If we cannot do binary update, let's work on the DOM instead.
    dom = wrapper.to_dom();
    if (dom == nullptr) return error_json(); /* purecov: inspected */
  }

  // remove elements identified by the paths, one after the other
  Json_dom_vector hits(key_memory_JSON);
  Json_path_clone path(key_memory_JSON);
  for (uint path_idx = 0; path_idx < path_count; ++path_idx) {
    if (clone_without_autowrapping(m_path_cache.get_path(path_idx + 1), &path,
                                   &wrapper, key_memory_JSON))
      return error_json(); /* purecov: inspected */

    // Cannot remove the root of the document.
    if (path.leg_count() == 0) continue;

    if (binary_diffs) {
      bool found_path = false;
      if (wrapper.binary_remove(m_partial_update_column, path,
                                partial_update_buffer, &found_path))
        return error_json(); /* purecov: inspected */
      if (!found_path) continue;
    } else {
      const Json_path_leg *last_leg = path.last_leg();
      hits.clear();
      if (dom->seek(path, path.leg_count() - 1, &hits, false, true))
        return error_json();       /* purecov: inspected */
      if (hits.empty()) continue;  // nothing to do

      assert(hits.size() == 1);
      Json_dom *parent = hits[0];
      if (parent->json_type() == enum_json_type::J_OBJECT) {
        auto object = down_cast<Json_object *>(parent);
        if (last_leg->get_type() != jpl_member ||
            !object->remove(last_leg->get_member_name()))
          continue;
      } else if (parent->json_type() == enum_json_type::J_ARRAY) {
        auto array = down_cast<Json_array *>(parent);
        if (last_leg->get_type() != jpl_array_cell) continue;
        Json_array_index idx = last_leg->first_array_index(array->size());
        if (!idx.within_bounds() || !array->remove(idx.position())) continue;
      } else {
        // Nothing to do. Only objects and arrays can contain values to remove.
        continue;
      }
    }

    if (logical_diffs)
      table->add_logical_diff(m_partial_update_column, path,
                              enum_json_diff_operation::REMOVE, nullptr);
  }  // end of loop through all paths

  // wrapper still owns the pruned doc, so hand it over to result
  *wr = std::move(wrapper);

  return false;
}

Item_func_json_merge::Item_func_json_merge(THD *thd, const POS &pos,
                                           PT_item_list *a)
    : Item_func_json_merge_preserve(thd, pos, a) {
  push_deprecated_warn(thd, "JSON_MERGE",
                       "JSON_MERGE_PRESERVE/JSON_MERGE_PATCH");
}

bool Item_func_json_merge_preserve::val_json(Json_wrapper *wr) {
  assert(fixed);

  Json_dom_ptr result_dom;

  try {
    for (uint idx = 0; idx < arg_count; idx++) {
      Json_wrapper next_wrapper;
      if (get_json_wrapper(args, idx, &m_value, func_name(), &next_wrapper))
        return error_json();

      if (args[idx]->null_value) {
        null_value = true;
        return false;
      }

      /*
        Grab the next DOM, release it from its wrapper, and merge it
        into the previous DOM.
      */
      Json_dom_ptr next_dom(next_wrapper.to_dom());
      next_wrapper.set_alias();
      if (next_dom == nullptr) return error_json(); /* purecov: inspected */

      if (idx == 0)
        result_dom = std::move(next_dom);
      else
        result_dom = merge_doms(std::move(result_dom), std::move(next_dom));

      if (result_dom == nullptr) return error_json(); /* purecov: inspected */
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  *wr = Json_wrapper(std::move(result_dom));
  null_value = false;
  return false;
}

String *Item_func_json_quote::val_str(String *str) {
  assert(fixed);

  String *res = args[0]->val_str(str);
  if (!res) {
    null_value = true;
    return nullptr;
  }

  try {
    const char *safep;
    size_t safep_size;

    switch (args[0]->data_type()) {
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_TINY_BLOB:
        break;
      default:
        my_error(ER_INCORRECT_TYPE, MYF(0), "1", func_name());
        return error_str();
    }

    if (ensure_utf8mb4(*res, &m_value, &safep, &safep_size, true)) {
      null_value = true;
      return nullptr;
    }

    /*
      One of the string buffers (str or m_value) is no longer in use
      and can be reused as the result buffer. Which of them it is,
      depends on whether or not ensure_utf8mb4() needed to do charset
      conversion. Make res point to the available buffer.
    */
    if (safep == m_value.ptr()) {
      /*
        ensure_utf8mb4() converted the input string to utf8mb4 by
        copying it into m_value. str is now available for reuse as
        result buffer.
      */
      res = str;
    } else {
      /*
        Conversion to utf8mb4 was not needed, so ensure_utf8mb4() did
        not touch the m_value buffer, and the input string still lives
        in res. Use m_value as result buffer.
      */
      assert(safep == res->ptr());
      res = &m_value;
    }

    res->length(0);
    res->set_charset(&my_charset_utf8mb4_bin);
    if (double_quote(safep, safep_size, res))
      return error_str(); /* purecov: inspected */
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_str();
    /* purecov: end */
  }

  null_value = false;
  return res;
}

String *Item_func_json_unquote::val_str(String *str) {
  assert(fixed);

  try {
    if (args[0]->data_type() == MYSQL_TYPE_JSON) {
      Json_wrapper wr;
      if (get_json_wrapper(args, 0, str, func_name(), &wr)) {
        return error_str();
      }

      if (args[0]->null_value) {
        null_value = true;
        return nullptr;
      }

      m_value.length(0);

      if (wr.to_string(&m_value, false, func_name(), JsonDepthErrorHandler)) {
        return error_str();
      }

      null_value = false;
      // String pointer may be null.
      if (m_value.is_empty()) return make_empty_result();

      return &m_value;
    }

    String *res = args[0]->val_str(str);

    if (!res) {
      null_value = true;
      return nullptr;
    }

    /*
      We only allow a string argument, so get rid of any other
      type arguments.
    */
    switch (args[0]->data_type()) {
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_TINY_BLOB:
        break;
      default:
        my_error(ER_INCORRECT_TYPE, MYF(0), "1", func_name());
        return error_str();
    }

    const char *utf8text;
    size_t utf8len;
    if (ensure_utf8mb4(*res, &m_conversion_buffer, &utf8text, &utf8len, true))
      return error_str();
    String *utf8str = (res->ptr() == utf8text) ? res : &m_conversion_buffer;
    assert(utf8text == utf8str->ptr());

    if (utf8len < 2 || utf8text[0] != '"' || utf8text[utf8len - 1] != '"') {
      null_value = false;
      // Return string unchanged, but convert to utf8mb4 if needed.
      if (res == utf8str) {
        return res;
      }
      if (str->copy(utf8text, utf8len, collation.collation))
        return error_str(); /* purecov: inspected */
      return str;
    }

    Json_dom_ptr dom;
    JsonParseDefaultErrorHandler parse_handler(func_name(), 0);
    if (parse_json(*utf8str, &dom, true, parse_handler,
                   JsonDepthErrorHandler)) {
      return error_str();
    }

    /*
      Extract the internal string representation as a MySQL string
    */
    assert(dom->json_type() == enum_json_type::J_STRING);
    Json_wrapper wr(std::move(dom));
    if (str->copy(wr.get_data(), wr.get_data_length(), collation.collation))
      return error_str(); /* purecov: inspected */
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_str();
    /* purecov: end */
  }

  null_value = false;
  return str;
}

String *Item_func_json_pretty::val_str(String *str) {
  assert(fixed);
  try {
    Json_wrapper wr;
    if (get_json_wrapper(args, 0, str, func_name(), &wr)) return error_str();

    null_value = args[0]->null_value;
    if (null_value) return nullptr;

    str->length(0);
    if (wr.to_pretty_string(str, func_name(), JsonDepthErrorHandler))
      return error_str(); /* purecov: inspected */

    return str;
  }
  /* purecov: begin inspected */
  catch (...) {
    handle_std_exception(func_name());
    return error_str();
  }
  /* purecov: end */
}

longlong Item_func_json_storage_size::val_int() {
  assert(fixed);

  /*
    If the input is a reference to a JSON column, return the actual storage
    size of the value in the table.
  */
  if (args[0]->type() == FIELD_ITEM &&
      args[0]->data_type() == MYSQL_TYPE_JSON) {
    null_value = args[0]->is_null();
    if (null_value) return 0;
    return down_cast<Item_field *>(args[0])->field->data_length();
  }

  /*
    Otherwise, return the size required to store the argument if it were
    serialized to the binary JSON format.
  */
  Json_wrapper wrapper;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer;
  try {
    if (get_json_wrapper(args, 0, &buffer, func_name(), &wrapper))
      return error_int();
  }
  /* purecov: begin inspected */
  catch (...) {
    handle_std_exception(func_name());
    return error_int();
  }
  /* purecov: end */

  null_value = args[0]->null_value;
  if (null_value) return 0;

  const THD *const thd = current_thd;
  if (wrapper.to_binary(JsonSerializationDefaultErrorHandler(thd), &buffer))
    return error_int(); /* purecov: inspected */

  if (buffer.length() > thd->variables.max_allowed_packet) {
    my_error(ER_WARN_ALLOWED_PACKET_OVERFLOWED, MYF(0),
             "json_binary::serialize", thd->variables.max_allowed_packet);
    return error_int();
  }
  return buffer.length();
}

longlong Item_func_json_storage_free::val_int() {
  assert(fixed);

  Json_wrapper wrapper;
  try {
    StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer;
    if (get_json_wrapper(args, 0, &buffer, func_name(), &wrapper))
      return error_int();
  }
  /* purecov: begin inspected */
  catch (...) {
    handle_std_exception(func_name());
    return error_int();
  }
  /* purecov: end */

  null_value = args[0]->null_value;
  if (null_value) return 0;

  size_t space;
  if (wrapper.get_free_space(JsonSerializationDefaultErrorHandler(current_thd),
                             &space)) {
    return error_int(); /* purecov: inspected */
  }

  return space;
}

bool Item_func_json_merge_patch::val_json(Json_wrapper *wr) {
  assert(fixed);

  try {
    if (get_json_wrapper(args, 0, &m_value, func_name(), wr))
      return error_json();

    null_value = args[0]->null_value;

    Json_wrapper patch_wr;
    for (uint i = 1; i < arg_count; ++i) {
      if (get_json_wrapper(args, i, &m_value, func_name(), &patch_wr))
        return error_json();

      if (args[i]->null_value) {
        /*
          The patch is unknown, so the result so far is unknown. We
          cannot return NULL immediately, since a later patch can give
          a known result. This is because the result of a merge
          operation is the patch itself if the patch is not an object,
          regardless of what the target document is.
        */
        null_value = true;
        continue;
      }

      /*
        If a patch is not an object, the result of the merge operation
        is the patch itself. So just set the result to this patch and
        go on to the next patch.
      */
      if (patch_wr.type() != enum_json_type::J_OBJECT) {
        *wr = std::move(patch_wr);
        null_value = false;
        continue;
      }

      /*
        The target document is unknown, and we cannot tell the result
        from the patch alone when the patch is an object, so go on to
        the next patch.
      */
      if (null_value) continue;

      /*
        Get the DOM representation of the target document. It should
        be an object, and we will use an empty object if it is not.
      */
      Json_object_ptr target_dom;
      if (wr->type() == enum_json_type::J_OBJECT) {
        target_dom.reset(down_cast<Json_object *>(wr->to_dom()));
        wr->set_alias();
      } else {
        target_dom = create_dom_ptr<Json_object>();
      }

      if (target_dom == nullptr) return error_json(); /* purecov: inspected */

      // Get the DOM representation of the patch object.
      Json_object_ptr patch_dom(down_cast<Json_object *>(patch_wr.to_dom()));
      patch_wr.set_alias();

      // Apply the patch on the target document.
      if (patch_dom == nullptr || target_dom->merge_patch(std::move(patch_dom)))
        return error_json(); /* purecov: inspected */

      // Move the result of the merge operation into the result wrapper.
      *wr = Json_wrapper(std::move(target_dom));
      null_value = false;
    }

    return false;
  }
  /* purecov: begin inspected */
  catch (...) {
    handle_std_exception(func_name());
    return error_json();
  }
  /* purecov: end */
}

/**
  Sets the data type of an Item_func_array_cast or Item_func_json_value based on
  the Cast_type.

  @param item       the Item whose data type to set
  @param cast_type  the type of cast
  @param length     the declared length of the target type
  @param decimals   the declared precision of the target type
  @param charset    the character set of the target type (nullptr if not
                    specified)
*/
static void set_data_type_from_cast_type(Item *item, Cast_target cast_type,
                                         unsigned length, unsigned decimals,
                                         const CHARSET_INFO *charset) {
  switch (cast_type) {
    case ITEM_CAST_SIGNED_INT:
      item->set_data_type_longlong();
      item->unsigned_flag = false;
      return;
    case ITEM_CAST_UNSIGNED_INT:
      item->set_data_type_longlong();
      item->unsigned_flag = true;
      return;
    case ITEM_CAST_DATE:
      item->set_data_type_date();
      return;
    case ITEM_CAST_YEAR:
      item->set_data_type_year();
      return;
    case ITEM_CAST_TIME:
      item->set_data_type_time(decimals);
      return;
    case ITEM_CAST_DATETIME:
      item->set_data_type_datetime(decimals);
      return;
    case ITEM_CAST_DECIMAL:
      item->set_data_type_decimal(
          std::min<unsigned>(length, DECIMAL_MAX_PRECISION), decimals);
      return;
    case ITEM_CAST_CHAR:
      // If no character set is specified, the JSON default character set is
      // used.
      if (charset == nullptr)
        item->set_data_type_string(length, &my_charset_utf8mb4_0900_bin);
      else
        item->set_data_type_string(length, charset);
      return;
    case ITEM_CAST_JSON:
      // JSON_VALUE(... RETURNING JSON) is supported, CAST(... AS JSON ARRAY) is
      // not supported.
      assert(!item->returns_array());
      item->set_data_type_json();
      return;
    case ITEM_CAST_DOUBLE:
      item->set_data_type_double();
      return;
    case ITEM_CAST_FLOAT:
      item->set_data_type_float();
      return;
    // JSON_VALUE(... RETURNING <geometry type>) is not supported
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return;
  }

  assert(false); /* purecov: deadcode */
}

Item_func_array_cast::Item_func_array_cast(const POS &pos, Item *a,
                                           Cast_target type, uint len_arg,
                                           uint dec_arg,
                                           const CHARSET_INFO *cs_arg)
    : Item_func(pos, a), cast_type(type) {
  set_data_type_from_cast_type(this, type, len_arg, dec_arg, cs_arg);
}

Item_func_array_cast::~Item_func_array_cast() = default;

bool Item_func_array_cast::val_json(Json_wrapper *wr) {
  try {
    String data_buf;
    if (get_json_wrapper(args, 0, &data_buf, func_name(), wr))
      return error_json();
    null_value = args[0]->null_value;
    return false;
    /* purecov: begin inspected */
  } catch (...) {
    handle_std_exception(func_name());
    return error_json();
  }
  /* purecov: end */
}

bool Item_func_array_cast::fix_fields(THD *thd, Item **ref) {
  // Prohibit use of CAST AS ARRAY outside of functional index expressions.
  if (!m_is_allowed) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "Use of CAST( .. AS .. ARRAY) outside of functional index in "
             "CREATE(non-SELECT)/ALTER TABLE or in general expressions");
    return true;
  }

  if (m_result_array == nullptr) {
    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    m_result_array.reset(::new (thd->mem_root) Json_array);
    if (m_result_array == nullptr) return true;
  }

  return Item_func::fix_fields(thd, ref);
}

/**
  Prints the target type of a cast operation (either CAST or JSON_VALUE).

  @param cast_type   the cast type
  @param item        the Item in which the cast operation is performed
  @param[out] str    the string to print to
*/
static void print_cast_type(Cast_target cast_type, const Item *item,
                            String *str) {
  const unsigned decimals = item->decimals;
  switch (cast_type) {
    case ITEM_CAST_SIGNED_INT:
      str->append(STRING_WITH_LEN("signed"));
      return;
    case ITEM_CAST_UNSIGNED_INT:
      str->append(STRING_WITH_LEN("unsigned"));
      return;
    case ITEM_CAST_DATE:
      str->append(STRING_WITH_LEN("date"));
      return;
    case ITEM_CAST_YEAR:
      str->append(STRING_WITH_LEN("year"));
      return;
    case ITEM_CAST_TIME:
      str->append(STRING_WITH_LEN("time"));
      if (decimals > 0) str->append_parenthesized(decimals);
      return;
    case ITEM_CAST_DATETIME:
      str->append(STRING_WITH_LEN("datetime"));
      if (decimals > 0) str->append_parenthesized(decimals);
      return;
    case ITEM_CAST_DECIMAL:
      // length and dec are already set
      str->append(STRING_WITH_LEN("decimal("));
      str->append_ulonglong(my_decimal_length_to_precision(
          item->max_length, decimals, item->unsigned_flag));
      str->append(STRING_WITH_LEN(", "));
      str->append_ulonglong(decimals);
      str->append(')');
      return;
    case ITEM_CAST_CHAR: {
      const CHARSET_INFO *const cs = item->collation.collation;
      if (cs == &my_charset_bin) {
        str->append(STRING_WITH_LEN("binary"));
        str->append_parenthesized(item->max_length);
      } else {
        str->append(STRING_WITH_LEN("char"));
        str->append_parenthesized(item->max_char_length());
        if (cs != &my_charset_utf8mb4_0900_bin) {
          str->append(STRING_WITH_LEN(" character set "));
          str->append(cs->csname);
        }
      }
      return;
    }
    case ITEM_CAST_JSON:
      str->append(STRING_WITH_LEN("json"));
      return;
    case ITEM_CAST_FLOAT:
      str->append(STRING_WITH_LEN("float"));
      return;
    case ITEM_CAST_DOUBLE:
      str->append(STRING_WITH_LEN("double"));
      return;
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      str->append(STRING_WITH_LEN("point"));
      return;
    case ITEM_CAST_LINESTRING:
      str->append(STRING_WITH_LEN("linestring"));
      return;
    case ITEM_CAST_POLYGON:
      str->append(STRING_WITH_LEN("polygon"));
      return;
    case ITEM_CAST_MULTIPOINT:
      str->append(STRING_WITH_LEN("multipoint"));
      return;
    case ITEM_CAST_MULTILINESTRING:
      str->append(STRING_WITH_LEN("multilinestring"));
      return;
    case ITEM_CAST_MULTIPOLYGON:
      str->append(STRING_WITH_LEN("multipolygon"));
      return;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      str->append(STRING_WITH_LEN("geometrycollection"));
      return;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
}

void Item_func_array_cast::print(const THD *thd, String *str,
                                 enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  print_cast_type(cast_type, this, str);
  str->append(STRING_WITH_LEN(" array)"));
}

void Item_func_array_cast::add_json_info(Json_object *obj) {
  String cast_type_str;
  print_cast_type(cast_type, this, &cast_type_str);
  obj->add_alias("cast_type", create_dom_ptr<Json_string>(
                                  cast_type_str.ptr(), cast_type_str.length()));
}

bool Item_func_array_cast::resolve_type(THD *) {
  if (reject_vector_args()) return true;
  set_nullable(true);
  return false;
}

static enum Item_result json_cast_result_type(Cast_target cast_type) {
  switch (cast_type) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_YEAR:
    case ITEM_CAST_UNSIGNED_INT:
      return INT_RESULT;
    case ITEM_CAST_DATE:
    case ITEM_CAST_TIME:
    case ITEM_CAST_DATETIME:
    case ITEM_CAST_CHAR:
    case ITEM_CAST_JSON:
      return STRING_RESULT;
    case ITEM_CAST_DECIMAL:
      return DECIMAL_RESULT;
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return REAL_RESULT;
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
    case ITEM_CAST_LINESTRING:
    case ITEM_CAST_POLYGON:
    case ITEM_CAST_MULTIPOINT:
    case ITEM_CAST_MULTILINESTRING:
    case ITEM_CAST_MULTIPOLYGON:
    case ITEM_CAST_GEOMETRYCOLLECTION:
      return INVALID_RESULT;
      /* purecov: end */
  }

  assert(false); /* purecov: deadcode */
  return INT_RESULT;
}

enum Item_result Item_func_array_cast::result_type() const {
  return json_cast_result_type(cast_type);
}

type_conversion_status Item_func_array_cast::save_in_field_inner(Field *field,
                                                                 bool) {
  // Array of any type is stored as JSON.
  Json_wrapper wr;
  if (val_json(&wr)) return TYPE_ERR_BAD_VALUE;

  if (null_value) return set_field_to_null(field);

  field->set_notnull();
  return down_cast<Field_typed_array *>(field)->store_array(
      &wr, m_result_array.get());
}

void Item_func_array_cast::cleanup() { Item_func::cleanup(); }

/// Converts the "data type" used by Item to a "real type" used by Field.
static enum_field_types data_type_to_real_type(enum_field_types data_type) {
  // Only temporal types have different "data type" and "real type".
  switch (data_type) {
    case MYSQL_TYPE_DATE:
      return MYSQL_TYPE_NEWDATE;
    case MYSQL_TYPE_TIME:
      return MYSQL_TYPE_TIME2;
    case MYSQL_TYPE_DATETIME:
      return MYSQL_TYPE_DATETIME2;
    default:
      return data_type;
  }
}

Field *Item_func_array_cast::tmp_table_field(TABLE *table) {
  auto array_field = new (*THR_MALLOC) Field_typed_array(
      data_type_to_real_type(data_type()), unsigned_flag, max_length, decimals,
      nullptr, nullptr, 0, 0, "", table->s, 4, collation.collation);
  if (array_field == nullptr) return nullptr;
  array_field->init(table);
  return array_field;
}

bool Field_typed_array::coerce_json_value(const Json_wrapper *wr, bool no_error,
                                          Json_wrapper *coerced) const {
  Json_wrapper saved;
  THD *thd = current_thd;
  // Save JSON value to the conversion field
  if (wr->type() == enum_json_type::J_NULL) {
    Json_dom_ptr elt;
    if (!coerced) return false;
    *coerced = Json_wrapper(create_dom_ptr<Json_null>());
    return false;
  }
  String value, tmp;
  /*
    If caller isn't interested in the result, then it's a check on whether
    the value is coercible at all. In such case don't throw an error, just
    return 'true' when value isn't coercible.
  */
  if (save_json_to_field(thd, m_conv_item->field, wr, no_error)) return true;
  // The calling_function arg below isn't needed as it's used only for
  // geometry and geometry arrays aren't supported
  if (sql_scalar_to_json(m_conv_item, "<typed array>", &value, &tmp, &saved,
                         nullptr, true))
    return true;
  if (!coerced) return false;
  *coerced = std::move(saved);
  return false;
}

longlong Item_func_json_overlaps::val_int() {
  int res = 0;
  null_value = false;
  try {
    String m_doc_value;
    Json_wrapper wr_a, wr_b;
    Json_wrapper *doc_a = &wr_a;
    Json_wrapper *doc_b = &wr_b;

    // arg 0 is the document 1
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), doc_a) ||
        args[0]->null_value) {
      return error_int();
    }

    // arg 1 is the document 2
    if (get_json_wrapper(args, 1, &m_doc_value, func_name(), doc_b) ||
        args[1]->null_value) {
      return error_int();
    }
    // Handle case when doc_a is non-array and doc_b is array
    if (doc_a->type() != enum_json_type::J_ARRAY &&
        doc_b->type() == enum_json_type::J_ARRAY)
      std::swap(doc_a, doc_b);

    // Search in longer array
    if (doc_a->type() == enum_json_type::J_ARRAY &&
        doc_b->type() == enum_json_type::J_ARRAY &&
        doc_b->length() > doc_a->length())
      std::swap(doc_a, doc_b);

    switch (doc_a->type()) {
      case enum_json_type::J_ARRAY: {
        uint b_length = doc_b->length();
        Json_array *arr = down_cast<Json_array *>(doc_a->to_dom());
        // Use array auto-wrap to address whole object/scalar
        if (doc_b->type() != enum_json_type::J_ARRAY) b_length = 1;
        // Sort array and use binary search to lookup values
        arr->sort();
        for (uint i = 0; i < b_length; i++) {
          res = arr->binary_search((*doc_b)[i].to_dom());
          if (res) break;
        }

        break;
      }
      case enum_json_type::J_OBJECT: {
        // Objects can't overlap with a scalar and object vs array is
        // handled above
        if (doc_b->type() != enum_json_type::J_OBJECT) return 0;
        for (const auto &i : Json_object_wrapper(*doc_a)) {
          Json_wrapper elt_b = doc_b->lookup(i.first);
          // Not found
          if (elt_b.type() == enum_json_type::J_ERROR) continue;
          if ((res = (!elt_b.compare(i.second)))) break;
        }
        break;
      }
      default:
        // When both args are scalars behave like =
        return !doc_a->compare(*doc_b);
    }
    /* purecov: begin inspected */
  } catch (...) {
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }
  return res;
}

/**
  Return field Item that can be used for index lookups.
  JSON_OVERLAPS can be optimized using index in following cases
    JSON_OVERLAPS([json expr], [const json array])
    JSON_OVERLAPS([const json array], [json expr])
  If there's a functional index matching [json expr], the latter will be
  substituted for the GC field of the index. This function returns such a field
  so that the optimizer can generate range access for the index over that field.

  @returns
    Item_field field that can be used to generate index access
    NULL       when no such field
*/

Item *Item_func_json_overlaps::key_item() const {
  for (uint i = 0; i < arg_count; i++)
    if (args[i]->type() == Item::FIELD_ITEM && args[i]->returns_array())
      return args[i];
  return nullptr;
}

longlong Item_func_member_of::val_int() {
  null_value = false;
  try {
    String m_doc_value;
    String conv_buf;
    Json_wrapper doc_a, doc_b;
    bool is_doc_b_sorted = false;

    // arg 0 is the value to lookup
    if (get_json_atom_wrapper(args, 0, func_name(), &m_doc_value, &conv_buf,
                              &doc_a, nullptr, true) ||
        args[0]->null_value) {
      return error_int();
    }

    // arg 1 is the array to look up value in
    if (get_json_wrapper(args, 1, &m_doc_value, func_name(), &doc_b) ||
        args[1]->null_value) {
      return error_int();
    }

    // If it's cached as JSON, pre-sort array (only) for faster lookups
    if (args[1]->type() == Item::CACHE_ITEM &&
        args[1]->data_type() == MYSQL_TYPE_JSON) {
      Item_cache_json *cache = down_cast<Item_cache_json *>(args[1]);
      if (!(is_doc_b_sorted = cache->is_sorted())) {
        cache->sort();
        if (cache->val_json(&doc_b)) return error_int();
        is_doc_b_sorted = true;
      }
    }

    null_value = false;
    if (doc_b.type() != enum_json_type::J_ARRAY)
      return (!doc_a.compare(doc_b));
    else if (is_doc_b_sorted) {
      Json_array *arr = down_cast<Json_array *>(doc_b.to_dom());
      return arr->binary_search(doc_a.to_dom());
    } else {
      for (uint i = 0; i < doc_b.length(); i++) {
        const Json_wrapper elt = doc_b[i];
        if (!doc_a.compare(elt)) return true;
      }
    }
    /* purecov: begin inspected */
  } catch (...) {
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }
  return false;
}

void Item_func_member_of::print(const THD *thd, String *str,
                                enum_query_type query_type) const {
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" member of ("));
  args[1]->print(thd, str, query_type);
  str->append(')');
}

/**
  Check if a JSON value is a JSON OPAQUE, and if it can be printed in the field
  as a non base64 value.

  This is currently used by JSON_TABLE to see if we can print the JSON value in
  a field without having to encode it in base64.

  @param field_to_store_in The field we want to store the JSON value in
  @param json_data The JSON value we want to store.

  @returns
    true The JSON value can be stored without encoding it in base64
    false The JSON value can not be stored without encoding it, or it is not a
          JSON OPAQUE value.
*/
static bool can_store_json_value_unencoded(const Field *field_to_store_in,
                                           const Json_wrapper *json_data) {
  return (field_to_store_in->type() == MYSQL_TYPE_VARCHAR ||
          field_to_store_in->type() == MYSQL_TYPE_BLOB ||
          field_to_store_in->type() == MYSQL_TYPE_STRING) &&
         json_data->type() == enum_json_type::J_OPAQUE &&
         (json_data->field_type() == MYSQL_TYPE_STRING ||
          json_data->field_type() == MYSQL_TYPE_VARCHAR);
}

/**
  Save JSON to a given field

  Value is saved in type-aware manner. Into a JSON-typed column any JSON
  data could be saved. Into an SQL scalar field only a scalar could be
  saved. If data being saved isn't scalar or can't be coerced to the target
  type, an error is returned.

  @param  thd        Thread handler
  @param  field      Field to save data to
  @param  w          JSON data to save
  @param  no_error   If true, don't raise an error when the value cannot be
                     converted to the target type

  @returns
    false ok
    true  coercion error occur
*/

bool save_json_to_field(THD *thd, Field *field, const Json_wrapper *w,
                        bool no_error) {
  field->set_notnull();

  if (field->type() == MYSQL_TYPE_JSON) {
    Field_json *fld = down_cast<Field_json *>(field);
    return (fld->store_json(w) != TYPE_OK);
  }

  JsonCoercionHandler error_handler;
  if (no_error) {
    error_handler = JsonCoercionWarnHandler{field->field_name};
  } else {
    error_handler = JsonCoercionErrorHandler{field->field_name};
  }
  if (w->type() == enum_json_type::J_ARRAY ||
      w->type() == enum_json_type::J_OBJECT) {
    if (!no_error)
      my_error(ER_WRONG_JSON_TABLE_VALUE, MYF(0), field->field_name);
    return true;
  }

  auto truncated_fields_guard =
      create_scope_guard([thd, saved = thd->check_for_truncated_fields]() {
        thd->check_for_truncated_fields = saved;
      });
  thd->check_for_truncated_fields =
      no_error ? CHECK_FIELD_IGNORE : CHECK_FIELD_ERROR_FOR_NULL;

  bool err = false;
  switch (field->result_type()) {
    case INT_RESULT: {
      const longlong value = w->coerce_int(error_handler, &err, nullptr);

      // If the Json_wrapper holds a numeric value, grab the signedness from it.
      // If not, grab the signedness from the column where we are storing the
      // value.
      bool value_unsigned;
      if (w->type() == enum_json_type::J_INT) {
        value_unsigned = false;
      } else if (w->type() == enum_json_type::J_UINT) {
        value_unsigned = true;
      } else {
        value_unsigned = field->is_unsigned();
      }

      if (!err)
        err = field->store(value, value_unsigned) >= TYPE_WARN_OUT_OF_RANGE;
      break;
    }
    case STRING_RESULT: {
      MYSQL_TIME ltime;
      bool date_time_handled = false;
      /*
        Here we explicitly check for DATE/TIME to reduce overhead by
        avoiding encoding data into string in JSON code and decoding it
        back from string in Field code.

        Ensure that date is saved to a date column, and time into time
        column. Don't mix.
      */
      if (is_temporal_type_with_date(field->type())) {
        switch (w->type()) {
          case enum_json_type::J_DATE:
          case enum_json_type::J_DATETIME:
          case enum_json_type::J_TIMESTAMP:
            date_time_handled = true;
            err = w->coerce_date(error_handler,
                                 JsonCoercionDeprecatedDefaultHandler{}, &ltime,
                                 DatetimeConversionFlags(current_thd));
            break;
          default:
            break;
        }
      } else if (field->type() == MYSQL_TYPE_TIME &&
                 w->type() == enum_json_type::J_TIME) {
        date_time_handled = true;
        err = w->coerce_time(error_handler,
                             JsonCoercionDeprecatedDefaultHandler{}, &ltime);
      }
      if (date_time_handled) {
        err = err || field->store_time(&ltime);
        break;
      }
      // Initialize with an explicit empty string pointer,
      // instead of the default nullptr.
      // The reason is that we pass str.ptr() to Field::store()
      // which may end up calling memmove() which may have
      // __attribute__((nonnull)) on its 'src' argument.
      String str{"", 0, /* charset= */ nullptr};

      if (can_store_json_value_unencoded(field, w)) {
        str.set(w->get_data(), w->get_data_length(), field->charset());
      } else {
        err = w->to_string(&str, false, "JSON_TABLE", JsonDepthErrorHandler);
      }

      if (!err && (field->store(str.ptr(), str.length(), str.charset()) >=
                   TYPE_WARN_OUT_OF_RANGE))
        err = true;
      break;
    }
    case REAL_RESULT: {
      const double value = w->coerce_real(error_handler, &err);
      if (!err && (field->store(value) >= TYPE_WARN_OUT_OF_RANGE)) err = true;
      break;
    }
    case DECIMAL_RESULT: {
      my_decimal value;
      w->coerce_decimal(error_handler, &value, &err);
      if (!err && (field->store_decimal(&value) >= TYPE_WARN_OUT_OF_RANGE))
        err = true;
      break;
    }
    case ROW_RESULT:
    default:
      // Shouldn't happen
      assert(0);
  }

  if (err && !no_error)
    my_error(ER_JT_VALUE_OUT_OF_RANGE, MYF(0), field->field_name);
  return err;
}

struct Item_func_json_value::Default_value {
  int64_t integer_default;
  const MYSQL_TIME *temporal_default;
  LEX_CSTRING string_default;
  const my_decimal *decimal_default;
  std::unique_ptr<Json_dom> json_default;
  double real_default;
};

Item_func_json_value::Item_func_json_value(
    const POS &pos, Item *arg, Item *path, const Cast_type &cast_type,
    unsigned length, unsigned precision, Json_on_response_type on_empty_type,
    Item *on_empty_default, Json_on_response_type on_error_type,
    Item *on_error_default)
    : Item_func(pos, arg, path, on_empty_default, on_error_default),
      m_path_json(key_memory_JSON),
      m_on_empty(on_empty_type),
      m_on_error(on_error_type),
      m_cast_target(cast_type.target) {
  set_data_type_from_cast_type(this, m_cast_target, length, precision,
                               cast_type.charset);
}

Item_func_json_value::~Item_func_json_value() = default;

enum Item_result Item_func_json_value::result_type() const {
  return json_cast_result_type(m_cast_target);
}

Json_on_response_type Item_func_json_value::on_empty_response_type() const {
  return m_on_empty;
}

Json_on_response_type Item_func_json_value::on_error_response_type() const {
  return m_on_error;
}

bool Item_func_json_value::resolve_type(THD *) {
  // The path must be a character literal, so it's never NULL.
  assert(!args[1]->is_nullable());
  // The DEFAULT values are character literals, so they are never NULL if they
  // are specified.
  assert(m_on_empty != Json_on_response_type::DEFAULT ||
         !args[2]->is_nullable());
  assert(m_on_error != Json_on_response_type::DEFAULT ||
         !args[3]->is_nullable());

  // JSON_VALUE can return NULL if its first argument is nullable, or if NULL
  // ON EMPTY or NULL ON ERROR is specified or implied, or if the extracted JSON
  // value is the JSON null literal.
  set_nullable(true);
  return false;
}

/**
  Checks if a decimal value is within the range of the data type of an Item. It
  is considered within range if it can be converted to the data type without
  losing any leading significant digits.
*/
static bool decimal_within_range(const Item *item, const my_decimal *decimal) {
  assert(item->data_type() == MYSQL_TYPE_NEWDECIMAL);
  return decimal_intg(decimal) <= item->decimal_int_part();
}

unique_ptr_destroy_only<Item_func_json_value::Default_value>
Item_func_json_value::create_json_value_default(THD *thd, Item *item) {
  MEM_ROOT *const mem_root = thd->mem_root;

  auto default_value = make_unique_destroy_only<Default_value>(mem_root);
  if (default_value == nullptr) return nullptr;

  // Evaluate the defaults under strict mode, so that an error is raised if the
  // default value cannot be converted to the target type without warnings.
  Strict_error_handler strict_handler{
      Strict_error_handler::ENABLE_SET_SELECT_STRICT_ERROR_HANDLER};
  auto strict_handler_guard =
      create_scope_guard([thd, saved_sql_mode = thd->variables.sql_mode]() {
        thd->pop_internal_handler();
        thd->variables.sql_mode = saved_sql_mode;
      });
  thd->push_internal_handler(&strict_handler);
  thd->variables.sql_mode |=
      MODE_STRICT_ALL_TABLES | MODE_NO_ZERO_DATE | MODE_NO_ZERO_IN_DATE;
  thd->variables.sql_mode &= ~MODE_INVALID_DATES;

  // Check that the default value is within the range of the return type.
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_UNSIGNED_INT: {
      StringBuffer<STRING_BUFFER_USUAL_SIZE> string_buffer;
      const String *string_value = item->val_str(&string_buffer);
      if (thd->is_error()) return nullptr;
      assert(string_value != nullptr);
      const CHARSET_INFO *const cs = string_value->charset();
      const char *const start = string_value->ptr();
      const char *const end_of_string = start + string_value->length();
      const char *end_of_number = end_of_string;
      int error = 0;
      const int64_t value =
          cs->cset->strtoll10(cs, start, &end_of_number, &error);
      if (end_of_number != end_of_string) {
        const ErrConvString err(string_value);
        my_error(ER_TRUNCATED_WRONG_VALUE, MYF(0),
                 unsigned_flag ? "INTEGER UNSIGNED" : "INTEGER SIGNED",
                 err.ptr());
        return nullptr;
      }
      if (error > 0 ||
          (!unsigned_flag && error == 0 &&
           static_cast<uint64_t>(value) > INT64_MAX) ||
          (unsigned_flag && error == -1)) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0),
                 unsigned_flag ? "UNSIGNED DEFAULT" : "SIGNED DEFAULT",
                 func_name());
        return nullptr;
      }
      default_value->integer_default = value;
      break;
    }
    case ITEM_CAST_DATE: {
      MYSQL_TIME *ltime = new (mem_root) MYSQL_TIME;
      if (ltime == nullptr) return nullptr;
      if (item->get_date(ltime, 0)) return nullptr;
      assert(!thd->is_error());
      default_value->temporal_default = ltime;
      break;
    }
    case ITEM_CAST_YEAR: {
      StringBuffer<STRING_BUFFER_USUAL_SIZE> string_buffer;
      const String *string_value = item->val_str(&string_buffer);
      if (thd->is_error()) return nullptr;
      assert(string_value != nullptr);
      const CHARSET_INFO *const cs = string_value->charset();
      const char *const start = string_value->ptr();
      const char *const end_of_string = start + string_value->length();
      const char *end_of_number = end_of_string;
      int error = 0;
      const int64_t value =
          cs->cset->strtoll10(cs, start, &end_of_number, &error);
      if (end_of_number != end_of_string) {
        const ErrConvString err(string_value);
        my_error(ER_TRUNCATED_WRONG_VALUE, MYF(0), "YEAR", err.ptr());
        return nullptr;
      }
      if (error != 0 || (value > 2155) || (value < 1901 && value != 0)) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "YEAR", func_name());
        return nullptr;
      }
      default_value->integer_default = value;
      break;
    }
    case ITEM_CAST_TIME: {
      MYSQL_TIME *ltime = new (mem_root) MYSQL_TIME;
      if (ltime == nullptr) return nullptr;
      if (item->get_time(ltime)) return nullptr;
      assert(!thd->is_error());
      if (actual_decimals(ltime) > decimals) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "TIME DEFAULT", func_name());
        return nullptr;
      }
      default_value->temporal_default = ltime;
      break;
    }
    case ITEM_CAST_DATETIME: {
      MYSQL_TIME *ltime = new (mem_root) MYSQL_TIME;
      if (ltime == nullptr) return nullptr;
      if (item->get_date(ltime, TIME_DATETIME_ONLY)) return nullptr;
      assert(!thd->is_error());
      if (actual_decimals(ltime) > decimals) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "TIME DEFAULT", func_name());
        return nullptr;
      }
      default_value->temporal_default = ltime;
      break;
    }
    case ITEM_CAST_CHAR: {
      StringBuffer<STRING_BUFFER_USUAL_SIZE> string_buffer;
      const String *string_value = item->val_str(&string_buffer);
      if (thd->is_error()) return nullptr;
      assert(string_value != nullptr);
      if (string_value->numchars() > max_char_length()) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "CHAR DEFAULT", func_name());
        return nullptr;
      }
      if (my_charset_same(collation.collation, string_value->charset())) {
        default_value->string_default = {string_value->dup(mem_root),
                                         string_value->length()};
        if (default_value->string_default.str == nullptr) return nullptr;
      } else {
        String converted_string;
        unsigned errors;
        if (converted_string.copy(string_value->ptr(), string_value->length(),
                                  string_value->charset(), collation.collation,
                                  &errors))
          return nullptr; /* purecov: inspected */
        if (errors > 0) {
          my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "CHAR DEFAULT", func_name());
          return nullptr;
        }
        default_value->string_default = {converted_string.dup(mem_root),
                                         converted_string.length()};
        if (default_value->string_default.str == nullptr) return nullptr;
      }
      break;
    }
    case ITEM_CAST_DECIMAL: {
      my_decimal *buffer = new (mem_root) my_decimal;
      if (buffer == nullptr) return nullptr;
      const my_decimal *value = item->val_decimal(buffer);
      if (thd->is_error()) return nullptr;
      if (!decimal_within_range(this, value) || value->frac > decimals) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "DECIMAL DEFAULT", func_name());
        return nullptr;
      }
      default_value->decimal_default = value;
      break;
    }
    case ITEM_CAST_JSON: {
      StringBuffer<STRING_BUFFER_USUAL_SIZE> string_buffer;
      const String *string_value = item->val_str(&string_buffer);
      if (thd->is_error()) return nullptr;
      assert(string_value != nullptr);
      JsonParseDefaultErrorHandler parse_handler(func_name(), 0);
      if (parse_json(*string_value, &default_value->json_default, true,
                     parse_handler, JsonDepthErrorHandler)) {
        my_error(ER_INVALID_DEFAULT, MYF(0), func_name());
        return nullptr;
      }
      break;
    }
    case ITEM_CAST_FLOAT: {
      const double value = item->val_real();
      if (thd->is_error()) return nullptr;
      if (value > std::numeric_limits<float>::max() ||
          value < std::numeric_limits<float>::lowest()) {
        my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "FLOAT DEFAULT", func_name());
        return nullptr;
      }
      // The value is within range of FLOAT. Finally, cast it to float to get
      // rid of any extra (double) precision that doesn't fit in a FLOAT.
      default_value->real_default = static_cast<float>(value);
      break;
    }
    case ITEM_CAST_DOUBLE: {
      const double value = item->val_real();
      if (thd->is_error()) return nullptr;
      default_value->real_default = value;
      break;
    }
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return nullptr;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return nullptr;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return nullptr;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return nullptr;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return nullptr;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return nullptr;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return nullptr;
      /* purecov: end */
  }

  return default_value;
}

bool Item_func_json_value::fix_fields(THD *thd, Item **ref) {
  if (Item_func::fix_fields(thd, ref)) return true;

  if (check_convertible_to_json(args[0], 1, func_name())) return true;

  assert(args[1]->basic_const_item());
  const String *path = args[1]->val_str(nullptr);
  assert(path != nullptr);
  if (parse_path(*path, false, &m_path_json)) return true;

  if (m_on_empty == Json_on_response_type::DEFAULT &&
      m_default_empty == nullptr) {
    assert(args[2]->basic_const_item());
    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    m_default_empty = create_json_value_default(thd, args[2]);
    if (m_default_empty == nullptr) return true;
  }

  if (m_on_error == Json_on_response_type::DEFAULT &&
      m_default_error == nullptr) {
    assert(args[3]->basic_const_item());
    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    m_default_error = create_json_value_default(thd, args[3]);
    if (m_default_error == nullptr) return true;
  }

  return false;
}

void Item_func_json_value::print(const THD *thd, String *str,
                                 enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("json_value("));
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(", "));
  args[1]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" returning "));
  print_cast_type(m_cast_target, this, str);
  // ON EMPTY
  print_on_empty_or_error(thd, str, query_type, /*on_empty=*/true, m_on_empty,
                          args[2]);
  // ON ERROR
  print_on_empty_or_error(thd, str, query_type, /*on_empty=*/false, m_on_error,
                          args[3]);
  str->append(')');
}

/**
  Checks if two Json_on_response_type values represent the same response.
  Implicit responses are equal to NULL ON EMPTY/ERROR.
*/
static bool same_response_type(Json_on_response_type type1,
                               Json_on_response_type type2) {
  return type1 == type2 || ((type1 == Json_on_response_type::IMPLICIT ||
                             type1 == Json_on_response_type::NULL_VALUE) &&
                            (type2 == Json_on_response_type::IMPLICIT ||
                             type2 == Json_on_response_type::NULL_VALUE));
}

bool Item_func_json_value::eq_specific(const Item *item) const {
  const auto other = down_cast<const Item_func_json_value *>(item);

  if (other->m_cast_target != m_cast_target) return false;
  if (other->max_length != max_length) return false;
  if (other->decimals != decimals) return false;

  if (!same_response_type(other->m_on_empty, m_on_empty)) return false;
  if (!same_response_type(other->m_on_error, m_on_error)) return false;

  return true;
}

/**
  Handles conversion errors for JSON_VALUE according to the ON ERROR clause.
  Called when the conversion of the extracted JSON value cannot be converted to
  the target type without truncation or data loss.

  If ERROR ON ERROR is specified, an error is raised, and true is returned.

  If NULL ON ERROR is specified (explicitly or implicitly), the item's
  null_value is set to true, and false is returned.

  If DEFAULT ... ON ERROR is specified, the item's null_value is set to false,
  and false is returned. It is up to the caller to return the correct default
  value.

  @param on_error     the type of response to give to the error
  @param type         the data type returned by the JSON_VALUE expression
  @param[in,out] item the Item representing the JSON_VALUE expression

  @retval true for ERROR ON ERROR (my_error() is called before returning)
  @retval false if DEFAULT .. ON ERROR or NULL ON ERROR was given
*/
static bool handle_json_value_conversion_error(Json_on_response_type on_error,
                                               const char *type,
                                               Item_func_json_value *item) {
  // Should have returned earlier if the value is NULL.
  assert(!item->null_value);

  switch (on_error) {
    case Json_on_response_type::ERROR: {
      my_error(ER_DATA_OUT_OF_RANGE, MYF(0), type, item->func_name());
      return true;
    }
    case Json_on_response_type::DEFAULT:
      item->null_value = false;
      break;
    case Json_on_response_type::NULL_VALUE:
    case Json_on_response_type::IMPLICIT:
      assert(item->is_nullable());
      item->null_value = true;
      break;
  }
  return false;
}

bool Item_func_json_value::extract_json_value(
    Json_wrapper *json, const Default_value **return_default) {
  *return_default = nullptr;

  try {
    Json_wrapper doc;

    assert(is_convertible_to_json(args[0]));  // Checked in fix_fields().
    if (args[0]->data_type() == MYSQL_TYPE_JSON) {
      if (args[0]->val_json(&doc)) return true;
      null_value = args[0]->null_value;
      if (null_value) {
        assert(is_nullable());
        return false;
      }
    } else {
      String buffer;
      const String *doc_string = args[0]->val_str(&buffer);
      null_value = args[0]->null_value;
      if (null_value) {
        assert(is_nullable());
        return false;
      }

      Json_dom_ptr dom;
      bool parse_error = false;
      {
        THD *thd = current_thd;
        // For all other modes than ERROR ON ERROR, downgrade parse errors to
        // warnings.
        Ignore_json_syntax_handler error_handler(
            thd, m_on_error != Json_on_response_type::ERROR);
        const char *json_func_name = func_name();
        if (parse_json(
                *doc_string, &dom, true,
                [json_func_name, &parse_error](const char *parse_err,
                                               size_t err_offset) {
                  my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), 1,
                           json_func_name, parse_err, err_offset, "");
                  parse_error = true;
                },
                JsonDepthErrorHandler) &&
            thd->is_error())
          return error_json();
      }

      // Invoke the ON ERROR clause if a parse error was raised.
      if (parse_error) {
        // ERROR ON ERROR will have returned above.
        assert(m_on_error != Json_on_response_type::ERROR);

        if (m_on_error == Json_on_response_type::DEFAULT) {
          *return_default = m_default_error.get();
          return false;
        } else {
          assert(m_on_error == Json_on_response_type::IMPLICIT ||
                 m_on_error == Json_on_response_type::NULL_VALUE);
          assert(is_nullable());
          null_value = true;
          return false;
        }
      }

      assert(dom != nullptr);
      doc = Json_wrapper(std::move(dom));
    }

    Json_wrapper_vector v(key_memory_JSON);
    if (doc.seek(m_path_json, m_path_json.leg_count(), &v, true, false))
      return error_json(); /* purecov: inspected */

    if (v.size() == 1) {
      *json = std::move(v[0]);
      if (json->type() == enum_json_type::J_NULL) {
        /*
          SQL:2016 : following the rule of JSON_VALUE we come to:
          9.36 Parsing JSON text GenRule 3-a-iii-3-A-III
          then to
          9.40 Casting an SQL/JSON sequence to an SQL type GenRule 4-b-ii,
          So, JSON null literal -> SQL/JSON null -> SQL NULL.
        */
        null_value = true;
      }
      return false;
    }

    // Invoke the ON EMPTY clause if no value was found.
    if (v.empty()) {
      switch (m_on_empty) {
        case Json_on_response_type::DEFAULT:
          *return_default = m_default_empty.get();
          return false;
        case Json_on_response_type::ERROR:
          my_error(ER_MISSING_JSON_VALUE, MYF(0), func_name());
          return error_json();
        case Json_on_response_type::IMPLICIT:
        case Json_on_response_type::NULL_VALUE:
          assert(is_nullable());
          null_value = true;
          return false;
      }
    }

    // Otherwise, we have multiple matches. Invoke the ON ERROR clause.
    assert(v.size() > 1);

    switch (m_on_error) {
      case Json_on_response_type::ERROR:
        my_error(ER_MULTIPLE_JSON_VALUES, MYF(0), func_name());
        return error_json();
      case Json_on_response_type::NULL_VALUE:
      case Json_on_response_type::IMPLICIT:
        assert(is_nullable());
        null_value = true;
        break;
      case Json_on_response_type::DEFAULT:
        *return_default = m_default_error.get();
        break;
    }

    return false;

    /* purecov: begin inspected */
  } catch (...) {
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }
}

bool Item_func_json_value::val_json(Json_wrapper *wr) {
  assert(fixed);
  assert(m_cast_target == ITEM_CAST_JSON);

  const Default_value *return_default = nullptr;
  if (extract_json_value(wr, &return_default)) return error_json();

  if (return_default != nullptr) {
    assert(!null_value);
    *wr = Json_wrapper(return_default->json_default.get(), true);
  }

  return false;
}

String *Item_func_json_value::val_str(String *buffer) {
  assert(fixed);
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_UNSIGNED_INT:
    case ITEM_CAST_YEAR:
      return val_string_from_int(buffer);
    case ITEM_CAST_DATE:
      return val_string_from_date(buffer);
    case ITEM_CAST_TIME:
      return val_string_from_time(buffer);
    case ITEM_CAST_DATETIME:
      return val_string_from_datetime(buffer);
    case ITEM_CAST_CHAR:
      return extract_string_value(buffer);
    case ITEM_CAST_DECIMAL:
      return val_string_from_decimal(buffer);
    case ITEM_CAST_JSON:
      return val_string_from_json(this, buffer);
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return val_string_from_real(buffer);
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return nullptr;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return nullptr;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return nullptr;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return nullptr;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return nullptr;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return nullptr;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return nullptr;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
  return nullptr;
}

double Item_func_json_value::val_real() {
  assert(fixed);
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_DATE:
    case ITEM_CAST_YEAR:
    case ITEM_CAST_TIME:
    case ITEM_CAST_DATETIME:
      return static_cast<double>(val_int());
    case ITEM_CAST_UNSIGNED_INT:
      return static_cast<double>(val_uint());
    case ITEM_CAST_CHAR:
      return val_real_from_string();
    case ITEM_CAST_DECIMAL:
      return val_real_from_decimal();
    case ITEM_CAST_JSON:
      return val_real_from_json(this);
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return extract_real_value();
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return 0.0;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return 0.0;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return 0.0;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return 0.0;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return 0.0;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return 0.0;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return 0.0;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
  return 0.0;
}

longlong Item_func_json_value::val_int() {
  assert(fixed);
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_UNSIGNED_INT:
      return extract_integer_value();
    case ITEM_CAST_YEAR:
      return extract_year_value();
    case ITEM_CAST_DATE:
      return val_int_from_date();
    case ITEM_CAST_TIME:
      return val_int_from_time();
    case ITEM_CAST_DATETIME:
      return val_int_from_datetime();
    case ITEM_CAST_CHAR:
      return val_int_from_string();
    case ITEM_CAST_DECIMAL:
      return val_int_from_decimal();
    case ITEM_CAST_JSON:
      return val_int_from_json(this);
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return val_int_from_real();
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return 0;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return 0;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return 0;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return 0;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return 0;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return 0;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return 0;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
  return 0;
}

my_decimal *Item_func_json_value::val_decimal(my_decimal *value) {
  assert(fixed);
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_UNSIGNED_INT:
    case ITEM_CAST_YEAR:
      return val_decimal_from_int(value);
    case ITEM_CAST_DATE:
    case ITEM_CAST_DATETIME:
      return val_decimal_from_date(value);
    case ITEM_CAST_TIME:
      return val_decimal_from_time(value);
    case ITEM_CAST_CHAR:
      return val_decimal_from_string(value);
    case ITEM_CAST_DECIMAL:
      return extract_decimal_value(value);
    case ITEM_CAST_JSON:
      return val_decimal_from_json(this, value);
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return val_decimal_from_real(value);
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return nullptr;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return nullptr;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return nullptr;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return nullptr;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return nullptr;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return nullptr;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return nullptr;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
  return nullptr;
}

bool Item_func_json_value::get_date(MYSQL_TIME *ltime, my_time_flags_t flags) {
  assert(fixed);
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_UNSIGNED_INT:
      return get_date_from_int(ltime, flags);
    case ITEM_CAST_DATE:
    case ITEM_CAST_YEAR:
      return extract_date_value(ltime);
    case ITEM_CAST_DATETIME:
      return extract_datetime_value(ltime);
    case ITEM_CAST_TIME:
      return get_date_from_time(ltime);
    case ITEM_CAST_CHAR:
      return get_date_from_string(ltime, flags);
    case ITEM_CAST_DECIMAL:
      return get_date_from_decimal(ltime, flags);
    case ITEM_CAST_JSON:
      return get_date_from_json(this, ltime, flags);
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return get_date_from_real(ltime, flags);
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return true;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return true;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return true;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return true;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return true;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return true;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return true;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
  return true;
}

bool Item_func_json_value::get_time(MYSQL_TIME *ltime) {
  assert(fixed);
  switch (m_cast_target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_YEAR:
    case ITEM_CAST_UNSIGNED_INT:
      return get_time_from_int(ltime);
    case ITEM_CAST_DATE:
      return get_time_from_date(ltime);
    case ITEM_CAST_TIME:
      return extract_time_value(ltime);
    case ITEM_CAST_DATETIME:
      return get_time_from_datetime(ltime);
    case ITEM_CAST_CHAR:
      return get_time_from_string(ltime);
    case ITEM_CAST_DECIMAL:
      return get_time_from_decimal(ltime);
    case ITEM_CAST_JSON:
      return get_time_from_json(this, ltime);
    case ITEM_CAST_FLOAT:
    case ITEM_CAST_DOUBLE:
      return get_time_from_real(ltime);
    /* purecov: begin inspected */
    case ITEM_CAST_POINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POINT");
      return true;
    case ITEM_CAST_LINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "LINESTRING");
      return true;
    case ITEM_CAST_POLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "POLYGON");
      return true;
    case ITEM_CAST_MULTIPOINT:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOINT");
      return true;
    case ITEM_CAST_MULTILINESTRING:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTILINESTRING");
      return true;
    case ITEM_CAST_MULTIPOLYGON:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON", "MULTIPOLYGON");
      return true;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      my_error(ER_INVALID_CAST_TO_GEOMETRY, MYF(0), "JSON",
               "GEOMETRYCOLLECTION");
      return true;
      /* purecov: end */
  }
  assert(false); /* purecov: deadcode */
  return true;
}

int64_t Item_func_json_value::extract_integer_value() {
  assert(m_cast_target == ITEM_CAST_SIGNED_INT ||
         m_cast_target == ITEM_CAST_UNSIGNED_INT);
  assert(unsigned_flag == (m_cast_target == ITEM_CAST_UNSIGNED_INT));

  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default)) return error_int();

  if (null_value) {
    assert(is_nullable());
    return 0;
  }

  if (return_default != nullptr) {
    assert(!null_value);
    return return_default->integer_default;
  }

  bool err = false;
  bool unsigned_val = false;
  const int64_t value =
      wr.coerce_int([](const char *, int) {}, &err, &unsigned_val);

  if (!err && (unsigned_flag == unsigned_val || value >= 0)) return value;

  if (handle_json_value_conversion_error(
          m_on_error, unsigned_flag ? "UNSIGNED" : "SIGNED", this))
    return error_int();

  if (null_value) return 0;

  return m_default_error->integer_default;
}

int64_t Item_func_json_value::extract_year_value() {
  assert(m_cast_target == ITEM_CAST_YEAR);

  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default)) return error_int();

  if (null_value) {
    assert(is_nullable());
    return 0;
  }

  if (return_default != nullptr) {
    assert(!null_value);
    return return_default->integer_default;
  }

  bool err = false;
  bool unsigned_val = false;
  const int64_t value =
      wr.coerce_int([](const char *, int) {}, &err, &unsigned_val);

  if (!err && ((value == 0) || (value > 1900 && value <= 2155))) return value;

  if (handle_json_value_conversion_error(m_on_error, "YEAR", this))
    return error_int();

  if (null_value) return 0;

  return m_default_error->integer_default;
}

bool Item_func_json_value::extract_date_value(MYSQL_TIME *ltime) {
  assert(m_cast_target == ITEM_CAST_DATE || m_cast_target == ITEM_CAST_YEAR);
  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default) || null_value) {
    set_zero_time(ltime, MYSQL_TIMESTAMP_DATE);
    return true;
  }

  if (return_default != nullptr) {
    *ltime = *return_default->temporal_default;
    return false;
  }
  if (!wr.coerce_date([](const char *, int) {},
                      JsonCoercionDeprecatedDefaultHandler{}, ltime,
                      DatetimeConversionFlags(current_thd)))
    return false;

  if (handle_json_value_conversion_error(m_on_error, "DATE", this) ||
      null_value) {
    set_zero_time(ltime, MYSQL_TIMESTAMP_DATE);
    return true;
  }

  *ltime = *m_default_error->temporal_default;
  return false;
}

bool Item_func_json_value::extract_time_value(MYSQL_TIME *ltime) {
  assert(m_cast_target == ITEM_CAST_TIME);
  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default) || null_value) {
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
    return true;
  }

  if (return_default != nullptr) {
    *ltime = *return_default->temporal_default;
    return false;
  }
  if (!wr.coerce_time([](const char *, int) {},
                      JsonCoercionDeprecatedDefaultHandler{}, ltime))
    return false;

  if (handle_json_value_conversion_error(m_on_error, "TIME", this) ||
      null_value) {
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
    return true;
  }

  *ltime = *m_default_error->temporal_default;
  return false;
}

bool Item_func_json_value::extract_datetime_value(MYSQL_TIME *ltime) {
  assert(m_cast_target == ITEM_CAST_DATETIME);
  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default) || null_value) {
    set_zero_time(ltime, MYSQL_TIMESTAMP_DATETIME);
    return true;
  }

  if (return_default != nullptr) {
    *ltime = *return_default->temporal_default;
    return false;
  }

  if (!wr.coerce_date(
          [](const char *, int) {}, JsonCoercionDeprecatedDefaultHandler{},
          ltime, TIME_DATETIME_ONLY | DatetimeConversionFlags(current_thd)))
    return false;

  if (handle_json_value_conversion_error(m_on_error, "DATETIME", this) ||
      null_value) {
    set_zero_time(ltime, MYSQL_TIMESTAMP_DATETIME);
    return true;
  }

  *ltime = *m_default_error->temporal_default;
  return false;
}

my_decimal *Item_func_json_value::extract_decimal_value(my_decimal *value) {
  assert(m_cast_target == ITEM_CAST_DECIMAL);
  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default) || null_value) {
    return error_decimal(value);
  }

  if (return_default != nullptr) {
    *value = *return_default->decimal_default;
    return value;
  }

  bool err = false;
  wr.coerce_decimal([](const char *, int) {}, value, &err);
  if (!err && decimal_within_range(this, value)) return value;

  if (handle_json_value_conversion_error(m_on_error, "DECIMAL", this) ||
      null_value) {
    return error_decimal(value);
  }

  *value = *m_default_error->decimal_default;
  return value;
}

String *Item_func_json_value::extract_string_value(String *buffer) {
  assert(m_cast_target == ITEM_CAST_CHAR);
  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default)) return error_str();
  if (null_value) return null_return_str();
  if (return_default != nullptr) {
    buffer->set(return_default->string_default.str,
                return_default->string_default.length, collation.collation);
    return buffer;
  }

  // Return the unquoted result
  buffer->length(0);
  if (wr.to_string(buffer, false, func_name(), JsonDepthErrorHandler))
    return error_str();

  if (buffer->is_empty()) return make_empty_result();

  unsigned conversion_errors = 0;
  if (!my_charset_same(collation.collation, buffer->charset())) {
    // The string should be returned in a different character set. Convert it.
    String converted_string;
    if (converted_string.copy(buffer->ptr(), buffer->length(),
                              buffer->charset(), collation.collation,
                              &conversion_errors))
      return error_str(); /* purecov: inspected */
    assert(converted_string.charset() == collation.collation);
    buffer->swap(converted_string);
  }

  // If the string fits in the return type, return it.
  if (conversion_errors == 0 && buffer->numchars() <= max_char_length())
    return buffer;

  // Otherwise, handle the error.
  if (handle_json_value_conversion_error(m_on_error, "STRING", this))
    return error_str();
  if (null_value) return null_return_str();
  buffer->set(m_default_error->string_default.str,
              m_default_error->string_default.length, collation.collation);
  return buffer;
}

double Item_func_json_value::extract_real_value() {
  assert(m_cast_target == ITEM_CAST_FLOAT || m_cast_target == ITEM_CAST_DOUBLE);
  Json_wrapper wr;
  const Default_value *return_default = nullptr;
  if (extract_json_value(&wr, &return_default)) return error_real();
  if (null_value) {
    assert(is_nullable());
    return 0.0;
  }

  if (return_default != nullptr) return return_default->real_default;

  bool err = false;
  const double value = wr.coerce_real([](const char *, int) {}, &err);
  if (!err) {
    if (data_type() == MYSQL_TYPE_FLOAT) {
      // Remove any extra (double) precision.
      return static_cast<float>(value);
    } else {
      return value;
    }
  }

  if (handle_json_value_conversion_error(
          m_on_error, data_type() == MYSQL_TYPE_DOUBLE ? "DOUBLE" : "FLOAT",
          this))
    return error_real();

  if (null_value) return 0.0;

  return m_default_error->real_default;
}
