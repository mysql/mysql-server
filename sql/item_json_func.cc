/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* JSON Function items used by mysql */

#include "sql/item_json_func.h"

#include <string.h>

#include <algorithm>  // std::fill
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "binary_log_types.h"
#include "m_string.h"
#include "my_compare.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "prealloced_array.h"  // Prealloced_array
#include "sql/current_thd.h"   // current_thd
#include "sql/field.h"
#include "sql/item_cmpfunc.h"  // Item_func_like
#include "sql/item_subselect.h"
#include "sql/json_diff.h"
#include "sql/json_dom.h"
#include "sql/json_path.h"
#include "sql/my_decimal.h"
#include "sql/psi_memory_key.h"  // key_memory_JSON
#include "sql/sql_class.h"       // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"  // handle_std_exception
#include "sql/sql_time.h"               // field_type_to_timestamp_type
#include "sql/table.h"
#include "template_utils.h"  // down_cast

class PT_item_list;

/** Helper routines */

// see the contract for this function in item_json_func.h
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
      my_charset_same(cs, &my_charset_utf8_bin) ||
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
    buf->set_charset(&my_charset_utf8mb4_bin);
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
  @param[in]  arg_idx      0-based index of corresponding JSON function argument
  @param[in]  func_name    Name of the user-invoked JSON_ function
  @param[in,out] dom       If non-null, we want any text parsed DOM
                           returned at the location pointed to
  @param[in]  require_str_or_json
                           If true, generate an error if other types used
                           as input
  @param[out] parse_error  set to true if the parser was run and found an error
                           else false

  @returns false if the arg parsed as valid JSON, true otherwise
*/
bool parse_json(const String &res, uint arg_idx, const char *func_name,
                Json_dom_ptr *dom, bool require_str_or_json,
                bool *parse_error) {
  char buff[MAX_FIELD_WIDTH];
  String utf8_res(buff, sizeof(buff), &my_charset_utf8mb4_bin);

  const char *safep;   // contents of res, possibly converted
  size_t safe_length;  // length of safep

  *parse_error = false;

  if (ensure_utf8mb4(res, &utf8_res, &safep, &safe_length,
                     require_str_or_json)) {
    return true;
  }

  if (!dom) {
    DBUG_ASSERT(!require_str_or_json);
    return !is_valid_json_syntax(safep, safe_length);
  }

  const char *parse_err;
  size_t err_offset;
  *dom = Json_dom::parse(safep, safe_length, &parse_err, &err_offset);

  if (*dom == NULL && parse_err != NULL) {
    /*
      Report syntax error. The last argument is no longer used, but kept to
      avoid changing error message format.
    */
    my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), arg_idx + 1, func_name,
             parse_err, err_offset, "");
    *parse_error = true;
  }
  return *dom == NULL;
}

/**
  Get the field type of an item. This function returns the same value
  as arg->data_type() in most cases, but in some cases it may return
  another field type in order to ensure that the item gets handled the
  same way as items of a different type.
*/
static enum_field_types get_normalized_field_type(Item *arg) {
  enum_field_types ft = arg->data_type();
  switch (ft) {
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
      /*
        TINYTEXT, TEXT, MEDIUMTEXT, and LONGTEXT have type
        MYSQL_TYPE_BLOB. We want to treat them like strings. We check
        the collation to see if the blob is really a string.
      */
      if (arg->collation.collation != &my_charset_bin) return MYSQL_TYPE_STRING;
      break;
    case MYSQL_TYPE_VARCHAR:
      /*
        If arg represents a parameter to a prepared statement, its field
        type will be MYSQL_TYPE_VARCHAR instead of the actual type of
        the parameter. The item type will have the info, so adjust
        field_type to match.
      */
      switch (arg->type()) {
        case Item::NULL_ITEM:
          return MYSQL_TYPE_NULL;
        case Item::INT_ITEM:
          return MYSQL_TYPE_LONGLONG;
        case Item::REAL_ITEM:
          return MYSQL_TYPE_DOUBLE;
        case Item::DECIMAL_ITEM:
          return MYSQL_TYPE_NEWDECIMAL;
        default:
          break;
      }
    default:
      break;
  }
  return ft;
}

bool get_json_string(Item *arg_item, String *value, String *utf8_res,
                     const char **safep, size_t *safe_length) {
  String *const res = arg_item->val_str(value);

  if (!res) {
    return true;
  }

  if (ensure_utf8mb4(*res, utf8_res, safep, safe_length, true)) {
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

  switch (get_normalized_field_type(arg_item)) {
    case MYSQL_TYPE_NULL:
      if (arg_item->update_null_value()) return true;
      DBUG_ASSERT(arg_item->null_value);
      *valid = true;
      return false;
    case MYSQL_TYPE_JSON: {
      Json_wrapper w;
      // Also sets the null_value flag
      *valid = !arg_item->val_json(&w);
      return !*valid;
    }
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB: {
      String *const res = arg_item->val_str(value);
      if (arg_item->type() == Item::FIELD_ITEM) {
        Item_field *fi = down_cast<Item_field *>(arg_item);
        Field *field = fi->field;
        if (field->flags & (ENUM_FLAG | SET_FLAG)) {
          *valid = false;
          return false;
        }
      }

      if (arg_item->null_value) {
        *valid = true;
        return false;
      }

      bool parse_error = false;
      const bool failure = parse_json(*res, arg_idx, func_name, dom,
                                      require_str_or_json, &parse_error);
      *valid = !failure;
      return parse_error;
    }
    default:
      if (require_str_or_json) {
        *valid = false;
        my_error(ER_INVALID_TYPE_FOR_JSON, MYF(0), arg_idx + 1, func_name);
        return true;
      }

      *valid = false;
      return false;
  }
}

bool parse_path(String *path_value, bool forbid_wildcards,
                Json_path *json_path) {
  DBUG_ASSERT(path_value);

  const char *path_chars = path_value->ptr();
  size_t path_length = path_value->length();
  StringBuffer<STRING_BUFFER_USUAL_SIZE> res(&my_charset_utf8mb4_bin);

  if (ensure_utf8mb4(*path_value, &res, &path_chars, &path_length, true)) {
    return true;
  }

  // OK, we have a string encoded in utf-8. Does it parse?
  size_t bad_idx = 0;
  if (parse_path(false, path_length, path_chars, json_path, &bad_idx)) {
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

  @param[in]  arg           The oneOrAll arg passed to the JSON function.
  @param[in]  cached_ooa    Previous result of parsing this arg.
  @param[in]  func_name     The name of the calling JSON function.

  @returns ooa_one, ooa_all, ooa_null or ooa_error, based on the match
*/
static enum_one_or_all_type parse_and_cache_ooa(
    Item *arg, enum_one_or_all_type *cached_ooa, const char *func_name) {
  bool is_constant = arg->const_for_execution();

  if (is_constant) {
    if (*cached_ooa != ooa_uninitialized) {
      return *cached_ooa;
    }
  }

  StringBuffer<16> buffer;  // larger than common case: three characters + '\0'
  String *const one_or_all = arg->val_str(&buffer);
  if (!one_or_all || arg->null_value) {
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

Json_path_cache::~Json_path_cache() {}

bool Json_path_cache::parse_and_cache_path(Item **args, uint arg_idx,
                                           bool forbid_wildcards) {
  Item *arg = args[arg_idx];

  const bool is_constant = arg->const_for_execution();
  Path_cell &cell = m_arg_idx_to_vector_idx[arg_idx];

  if (is_constant && cell.m_status != enum_path_status::UNINITIALIZED) {
    // nothing to do if it has already been parsed
    return cell.m_status == enum_path_status::ERROR;
  }

  if (cell.m_status == enum_path_status::UNINITIALIZED) {
    cell.m_index = m_paths.size();
    if (m_paths.emplace_back()) return true; /* purecov: inspected */
  } else {
    // re-parsing a non-constant path for the next row
    m_paths[cell.m_index].clear();
  }

  String *path_value = arg->val_str(&m_path_value);
  bool null_value = (path_value == nullptr);
  if (!null_value &&
      parse_path(path_value, forbid_wildcards, &m_paths[cell.m_index])) {
    // oops, parsing failed
    cell.m_status = enum_path_status::ERROR;
    return true;
  }

  cell.m_status =
      null_value ? enum_path_status::OK_NULL : enum_path_status::OK_NOT_NULL;

  return false;
}

const Json_path *Json_path_cache::get_path(uint arg_idx) const {
  const Path_cell &cell = m_arg_idx_to_vector_idx[arg_idx];

  if (cell.m_status != enum_path_status::OK_NOT_NULL) {
    return NULL;
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

type_conversion_status Item_json_func::save_in_field_inner(
    Field *field, bool no_conversions) {
  return save_possibly_as_json(field, no_conversions);
}

longlong Item_func_json_valid::val_int() {
  DBUG_ASSERT(fixed == 1);
  try {
    bool ok;
    if (json_is_valid(args, 0, &m_value, func_name(), NULL, false, &ok)) {
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

typedef Prealloced_array<size_t, 16> Sorted_index_array;

/**
  Sort the elements of a JSON array and remove duplicates.

  @param[in]  orig  the original JSON array
  @param[out] v     vector that will be filled with the indexes of the array
                    elements in increasing order
  @return false on success, true on error
*/
static bool sort_array(const Json_wrapper &orig, Sorted_index_array *v) {
  if (v->reserve(orig.length())) return true; /* purecov: inspected */

  for (size_t i = 0; i < orig.length(); i++) v->push_back(i);

  // Sort the array...
  const auto less = [&orig](size_t idx1, size_t idx2) {
    return orig[idx1].compare(orig[idx2]) < 0;
  };
  std::sort(v->begin(), v->end(), less);

  // ... and remove duplicates.
  const auto equal = [&orig](size_t idx1, size_t idx2) {
    return orig[idx1].compare(orig[idx2]) == 0;
  };
  v->erase(std::unique(v->begin(), v->end(), equal), v->end());

  return false;
}

/**
  Check if one Json_wrapper contains all the elements of another
  Json_wrapper.

  @param[in]  thd           THD handle
  @param[in]  doc_wrapper   the containing document
  @param[in]  containee_wr  the possibly contained document
  @param[out] result        true if doc_wrapper contains containee_wr,
                            false otherwise
  @retval false on success
  @retval true on failure
*/
static bool contains_wr(const THD *thd, const Json_wrapper &doc_wrapper,
                        const Json_wrapper &containee_wr, bool *result) {
  if (doc_wrapper.type() == enum_json_type::J_OBJECT) {
    if (containee_wr.type() != enum_json_type::J_OBJECT ||
        containee_wr.length() > doc_wrapper.length()) {
      *result = false;
      return false;
    }

    for (auto c_oi = containee_wr.object_iterator(); !c_oi.empty();
         c_oi.next()) {
      auto c_elt = c_oi.elt();
      auto d_wr = doc_wrapper.lookup(c_elt.first);

      if (d_wr.type() == enum_json_type::J_ERROR) {
        // No match for this key. Give up.
        *result = false;
        return false;
      }

      // key is the same, now compare values
      if (contains_wr(thd, d_wr, c_elt.second, result))
        return true; /* purecov: inspected */

      if (!*result) {
        // Value didn't match, give up.
        return false;
      }
    }

    // All members in containee_wr found a match in doc_wrapper.
    *result = true;
    return false;
  }

  if (doc_wrapper.type() == enum_json_type::J_ARRAY) {
    const Json_wrapper *wr = &containee_wr;
    Json_wrapper a_wr;

    if (containee_wr.type() != enum_json_type::J_ARRAY) {
      // auto-wrap scalar or object in an array for uniform treatment later
      Json_wrapper scalar = containee_wr;
      Json_array_ptr array(new (std::nothrow) Json_array());
      if (array == nullptr || array->append_alias(scalar.clone_dom(thd)))
        return true; /* purecov: inspected */
      a_wr = Json_wrapper(std::move(array));
      wr = &a_wr;
    }

    // Indirection vectors containing the original indices
    Sorted_index_array d(key_memory_JSON);
    Sorted_index_array c(key_memory_JSON);

    // Sort both vectors, so we can compare efficiently
    if (sort_array(doc_wrapper, &d) || sort_array(*wr, &c))
      return true; /* purecov: inspected */

    size_t doc_i = 0;

    for (size_t c_i = 0; c_i < c.size(); c_i++) {
      Json_wrapper candidate = (*wr)[c[c_i]];
      if (candidate.type() == enum_json_type::J_ARRAY) {
        bool found = false;
        /*
          We do not increase doc_i here, use a tmp. We might need to check again
          against doc_i: this allows duplicates in the candidate.
        */
        for (size_t tmp = doc_i; tmp < d.size(); tmp++) {
          auto d_wr = doc_wrapper[d[tmp]];
          const auto dtype = d_wr.type();

          // Skip past all non-arrays.
          if (dtype < enum_json_type::J_ARRAY) {
            /*
              Remember the position so that we don't need to skip past
              these elements again for the next candidate.
            */
            doc_i = tmp;
            continue;
          }

          /*
            No more potential matches for this candidate if we've
            moved past all the arrays.
          */
          if (dtype > enum_json_type::J_ARRAY) break;

          if (contains_wr(thd, d_wr, candidate, result))
            return true; /* purecov: inspected */
          if (*result) {
            found = true;
            break;
          }
        }

        if (!found) {
          *result = false;
          return false;
        }
      } else {
        bool found = false;
        size_t tmp = doc_i;

        while (tmp < d.size()) {
          auto d_wr = doc_wrapper[d[tmp]];
          const auto dtype = d_wr.type();
          if (dtype == enum_json_type::J_ARRAY ||
              dtype == enum_json_type::J_OBJECT) {
            if (contains_wr(thd, d_wr, candidate, result))
              return true; /* purecov: inspected */
            if (*result) {
              found = true;
              break;
            }
          } else if (d_wr.compare(candidate) == 0) {
            found = true;
            break;
          }
          tmp++;
        }

        if (doc_i == d.size() || !found) {
          *result = false;
          return false;
        }
      }
    }

    *result = true;
    return false;
  }

  *result = (doc_wrapper.compare(containee_wr) == 0);
  return false;
}

void Item_func_json_contains::cleanup() {
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
}

longlong Item_func_json_contains::val_int() {
  DBUG_ASSERT(fixed == 1);

  try {
    Json_wrapper doc_wrapper;

    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &doc_wrapper) ||
        args[0]->null_value) {
      null_value = true;
      return 0;
    }

    Json_wrapper containee_wr;

    // arg 1 is the possible containee
    if (get_json_wrapper(args, 1, &m_doc_value, func_name(), &containee_wr) ||
        args[1]->null_value) {
      null_value = true;
      return 0;
    }

    if (arg_count == 3) {
      // path is specified
      if (m_path_cache.parse_and_cache_path(args, 2, true)) return error_int();
      const Json_path *path = m_path_cache.get_path(2);
      if (path == nullptr) {
        null_value = true;
        return 0;
      }

      Json_wrapper_vector v(key_memory_JSON);
      if (doc_wrapper.seek(*path, path->leg_count(), &v, true, false))
        return error_int(); /* purecov: inspected */

      if (v.size() == 0) {
        null_value = true;
        return 0;
      }

      bool ret;
      if (contains_wr(current_thd, v[0], containee_wr, &ret))
        return error_int(); /* purecov: inspected */
      null_value = false;
      return ret;
    } else {
      bool ret;
      if (contains_wr(current_thd, doc_wrapper, containee_wr, &ret))
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
  DBUG_ASSERT(fixed == 1);
  longlong result = 0;
  null_value = false;

  Json_wrapper wrapper;
  Json_wrapper_vector hits(key_memory_JSON);

  try {
    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value) {
      null_value = true;
      return 0;
    }

    // arg 1 is the oneOrAll flag
    bool require_all;
    switch (parse_and_cache_ooa(args[1], &m_cached_ooa, func_name())) {
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
      default: { return error_int(); }
    }

    // the remaining args are paths
    for (uint32 i = 2; i < arg_count; ++i) {
      if (m_path_cache.parse_and_cache_path(args, i, false)) return error_int();
      const Json_path *path = m_path_cache.get_path(i);
      if (path == nullptr) {
        null_value = true;
        return 0;
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

bool json_value(Item **args, uint arg_idx, Json_wrapper *result) {
  Item *arg = args[arg_idx];

  if (arg->data_type() == MYSQL_TYPE_NULL) {
    if (arg->update_null_value()) return true;
    DBUG_ASSERT(arg->null_value);
    return false;
  }

  if (arg->data_type() != MYSQL_TYPE_JSON) {
    // This is not a JSON value. Give up.
    return true;
  }

  return arg->val_json(result);
}

bool get_json_wrapper(Item **args, uint arg_idx, String *str,
                      const char *func_name, Json_wrapper *wrapper) {
  if (!json_value(args, arg_idx, wrapper)) {
    // Found a JSON value, return successfully.
    return false;
  }

  if (args[arg_idx]->data_type() == MYSQL_TYPE_JSON) {
    /*
      If the type of the argument is JSON and json_value() returned
      false, it means the argument didn't contain valid JSON data.
      Give up.
    */
    return true;
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

  if (args[arg_idx]->null_value) {
    return false;
  }

  DBUG_ASSERT(dom);

  *wrapper = Json_wrapper(std::move(dom));
  return false;
}

/**
  Extended type ids so that JSON_TYPE() can give useful type
  names to certain sub-types of J_OPAQUE.
*/
enum class enum_json_opaque_type {
  J_OPAQUE_BLOB = static_cast<int>(enum_json_type::J_ERROR) + 1,
  J_OPAQUE_BIT,
  J_OPAQUE_GEOMETRY
};

/**
  Maps the enumeration value of type enum_json_type into a string.
  For example:
  json_type_string_map[J_OBJECT] == "OBJECT"
*/
static constexpr const char *json_type_string_map[] = {
    "NULL",
    "DECIMAL",
    "INTEGER",
    "UNSIGNED INTEGER",
    "DOUBLE",
    "STRING",
    "OBJECT",
    "ARRAY",
    "BOOLEAN",
    "DATE",
    "TIME",
    "DATETIME",
    "TIMESTAMP",
    "OPAQUE",
    "ERROR",

    // OPAQUE types with special names
    "BLOB",
    "BIT",
    "GEOMETRY",
};

/// A constexpr version of std::strlen.
static constexpr uint32 strlen_const(const char *str) {
  return *str == '\0' ? 0 : 1 + strlen_const(str + 1);
}

/// std::max isn't constexpr until C++14, so we roll our own for now.
static constexpr uint32 max_const(uint32 a, uint32 b) { return a > b ? a : b; }

/// Find the length of the longest string in a range.
static constexpr uint32 longest_string(const char *const *begin,
                                       const char *const *end) {
  return begin == end
             ? 0
             : max_const(strlen_const(*begin), longest_string(begin + 1, end));
}

/**
   The maximum length of a string in json_type_string_map including
   a final zero char.
*/
static constexpr uint32 typelit_max_length =
    longest_string(
        json_type_string_map,
        json_type_string_map + array_elements(json_type_string_map)) +
    1;

bool Item_func_json_type::resolve_type(THD *) {
  maybe_null = true;
  m_value.set_charset(&my_charset_utf8mb4_bin);
  set_data_type_string(typelit_max_length, &my_charset_utf8mb4_bin);
  return false;
}

/**
   Compute an index into json_type_string_map
   to be applied to certain sub-types of J_OPAQUE.

   @param field_type The refined field type of the opaque value.

   @return an index into json_type_string_map
*/
static uint opaque_index(enum_field_types field_type) {
  switch (field_type) {
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      return static_cast<uint>(enum_json_opaque_type::J_OPAQUE_BLOB);

    case MYSQL_TYPE_BIT:
      return static_cast<uint>(enum_json_opaque_type::J_OPAQUE_BIT);

    case MYSQL_TYPE_GEOMETRY: {
      /**
        Should not get here. This path should be orphaned by the
        work done on implicit CASTing of geometry values to geojson
        objects. However, that work was done late in the project
        cycle for WL#7909. Do something sensible in case we missed
        something.

        FIXME.
      */
      /* purecov: begin deadcode */
      DBUG_ASSERT(false);
      return static_cast<uint>(enum_json_opaque_type::J_OPAQUE_GEOMETRY);
      /* purecov: end */
    }

    default:
      return static_cast<uint>(enum_json_type::J_OPAQUE);
  }
}

String *Item_func_json_type::val_str(String *) {
  DBUG_ASSERT(fixed == 1);

  try {
    Json_wrapper wr;
    if (get_json_wrapper(args, 0, &m_value, func_name(), &wr) ||
        args[0]->null_value) {
      null_value = true;
      return NULL;
    }

    const enum_json_type type = wr.type();
    uint typename_idx = static_cast<uint>(type);
    if (type == enum_json_type::J_OPAQUE) {
      typename_idx = opaque_index(wr.field_type());
    }

    m_value.length(0);
    if (m_value.append(json_type_string_map[typename_idx]))
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

String *Item_json_func::val_str(String *) {
  DBUG_ASSERT(fixed == 1);
  Json_wrapper wr;
  if (val_json(&wr)) return error_str();

  if (null_value) return NULL;

  m_string_buffer.length(0);

  if (wr.to_string(&m_string_buffer, true, func_name())) return error_str();

  null_value = false;
  return &m_string_buffer;
}

bool Item_json_func::get_date(MYSQL_TIME *ltime, my_time_flags_t) {
  Json_wrapper wr;
  if (val_json(&wr)) return true;

  if (null_value) return true;

  return wr.coerce_date(ltime, func_name());
}

bool Item_json_func::get_time(MYSQL_TIME *ltime) {
  Json_wrapper wr;
  if (val_json(&wr)) return true;

  if (null_value) return true;

  return wr.coerce_time(ltime, func_name());
}

longlong Item_json_func::val_int() {
  Json_wrapper wr;
  if (val_json(&wr)) return 0;

  if (null_value) return 0;

  return wr.coerce_int(func_name());
}

double Item_json_func::val_real() {
  Json_wrapper wr;
  if (val_json(&wr)) return 0.0;

  if (null_value) return 0.0;

  return wr.coerce_real(func_name());
}

my_decimal *Item_json_func::val_decimal(my_decimal *decimal_value) {
  Json_wrapper wr;
  if (val_json(&wr)) {
    my_decimal_set_zero(decimal_value);
    return decimal_value;
  }
  if (null_value) {
    my_decimal_set_zero(decimal_value);
    return decimal_value;
  }
  return wr.coerce_decimal(decimal_value, func_name());
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
                          Args &&... args) {
  if (scalar != nullptr) {
    scalar->emplace<T>(std::forward<Args>(args)...);
    return false;
  }

  *dom = create_dom_ptr<T>(std::forward<Args>(args)...);
  return dom == nullptr;
}

/**
  Get a JSON value from a function, field or subselect scalar.

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
  @param[in]     accept_string
                             if true, accept SQL strings as scalars
                             (false implies we need a valid
                              JSON parsable string)
  @return false if we could get a value or NULL, otherwise true
*/
static bool val_json_func_field_subselect(
    Item *arg, const char *calling_function, String *value, String *tmp,
    Json_wrapper *wr, Json_scalar_holder *scalar, bool accept_string) {
  enum_field_types field_type = get_normalized_field_type(arg);
  Json_dom_ptr dom;

  switch (field_type) {
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_LONGLONG: {
      longlong i = arg->val_int();

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
      longlong dt = arg->val_temporal_by_field_type();

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

      if (arg->null_value) return false;

      if (!r) {
        my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
        return true;
      }

      if (create_scalar<Json_decimal>(scalar, &dom, *r))
        return true; /* purecov: inspected */

      break;
    }
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_FLOAT: {
      double d = arg->val_real();

      if (arg->null_value) return false;

      if (create_scalar<Json_double>(scalar, &dom, d))
        return true; /* purecov: inspected */

      break;
    }
    case MYSQL_TYPE_GEOMETRY: {
      uint32 geometry_srid;
      bool retval = geometry_to_json(wr, arg, calling_function, INT_MAX32,
                                     false, false, false, &geometry_srid);

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
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_YEAR: {
      String *oo = arg->val_str(value);

      if (arg->null_value) return false;

      if (arg->type() == Item::FIELD_ITEM && field_type == MYSQL_TYPE_BLOB) {
        Item_field *it_f = down_cast<Item_field *>(arg);
        Field *f = it_f->field;
        Field_blob *fb = down_cast<Field_blob *>(f);
        switch (fb->pack_length() - portable_sizeof_char_ptr) {
          case 1:
            field_type = MYSQL_TYPE_TINY_BLOB;
            break;
          case 2:
            field_type = MYSQL_TYPE_BLOB;
            break;
          case 3:
            field_type = MYSQL_TYPE_MEDIUM_BLOB;
            break;
          case 4:
            field_type = MYSQL_TYPE_LONG_BLOB;
            break;
          default:
            DBUG_ASSERT(false);
        }
      }

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

      if (arg->null_value) return false;
      const CHARSET_INFO *cs = res->charset();

      if (cs == &my_charset_bin) {
        // BINARY or similar
        if (create_scalar<Json_opaque>(scalar, &dom, field_type, res->ptr(),
                                       res->length()))
          return true; /* purecov: inspected */

        break;
      } else if (accept_string) {
        const char *s = res->ptr();
        size_t ss = res->length();

        if (ensure_utf8mb4(*res, tmp, &s, &ss, true)) {
          return true;
        }

        if (create_scalar<Json_string>(scalar, &dom, s, ss))
          return true; /* purecov: inspected */

      } else {
        my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
        return true;
      }
      break;
    }
    case MYSQL_TYPE_DECIMAL:  // pre 5.0
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "old decimal type");
      return true;

    case MYSQL_TYPE_NULL:
      /*
        This shouldn't happen, since the only caller of this function
        returns earlier if it sees that the type is MYSQL_TYPE_NULL.
      */
      /* purecov: begin inspected */
      if (arg->update_null_value()) return true;
      DBUG_ASSERT(arg->null_value);
      return false;
      /* purecov: end */

    case MYSQL_TYPE_JSON:
      DBUG_ASSERT(false); /* purecov: inspected */

      // fall-through
    default:
      my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
      return true;
  }

  // Exactly one of scalar and dom should be used.
  DBUG_ASSERT((scalar == NULL) != (dom == NULL));
  DBUG_ASSERT(scalar == NULL || scalar->get() != NULL);

  if (scalar) {
    /*
      The DOM object lives in memory owned by the caller. Tell the
      wrapper that it's not the owner.
    */
    *wr = Json_wrapper(scalar->get());
    wr->set_alias();
    return false;
  }

  *wr = Json_wrapper(std::move(dom));
  return false;
}

/**
  Try to determine whether an argument has a boolean (as opposed
  to an int) type, and if so, return its boolean value.

  @param[in] arg The argument to inspect.
  @param[in,out] result Fill in the result if this is a boolean arg.

  @return True if the arg can be determined to have a boolean type.
*/
static bool extract_boolean(Item *arg, bool *result) {
  if (arg->is_bool_func()) {
    *result = arg->val_int();
    return true;
  }

  if (arg->type() == Item::SUBSELECT_ITEM) {
    // EXISTS, IN, ALL, ANY subqueries have boolean type
    Item_subselect *subs = down_cast<Item_subselect *>(arg);
    switch (subs->substype()) {
      case Item_subselect::EXISTS_SUBS:
      case Item_subselect::IN_SUBS:
      case Item_subselect::ALL_SUBS:
      case Item_subselect::ANY_SUBS:
        *result = arg->val_int();
        return true;
      default:
        break;
    }
  }

  if (arg->type() == Item::INT_ITEM) {
    const Name_string *const name = &arg->item_name;
    const bool is_literal_false = name->is_set() && name->eq("FALSE");
    const bool is_literal_true = name->is_set() && name->eq("TRUE");
    if (is_literal_false || is_literal_true) {
      *result = is_literal_true;
      return true;
    }
  }

  // doesn't fit any of the checks we perform
  return false;
}

// see the contract for this function in item_json_func.h
bool get_json_atom_wrapper(Item **args, uint arg_idx,
                           const char *calling_function, String *value,
                           String *tmp, Json_wrapper *wr,
                           Json_scalar_holder *scalar, bool accept_string) {
  bool result = false;

  Item *const arg = args[arg_idx];

  try {
    if (!json_value(args, arg_idx, wr)) {
      return false;
    }

    if (arg->data_type() == MYSQL_TYPE_JSON) {
      /*
        If the type of the argument is JSON and json_value() returned
        false, it means the argument didn't contain valid JSON data.
        Give up.
      */
      return true;
    }

    // boolean operators should produce boolean values
    bool boolean_value;
    if (extract_boolean(arg, &boolean_value)) {
      Json_dom_ptr boolean_dom;
      if (create_scalar<Json_boolean>(scalar, &boolean_dom, boolean_value))
        return true; /* purecov: inspected */

      if (scalar) {
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
    result = val_json_func_field_subselect(arg, calling_function, value, tmp,
                                           wr, scalar, accept_string);

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
                            NULL, true))
    return true;

  if (args[arg_idx]->null_value) {
    *wr = Json_wrapper(new (std::nothrow) Json_null());
  }

  return false;
}

bool Item_json_typecast::val_json(Json_wrapper *wr) {
  DBUG_ASSERT(fixed == 1);

  Json_dom_ptr dom;  //@< if non-null we want a DOM from parse

  if (args[0]->data_type() == MYSQL_TYPE_NULL) {
    null_value = true;
    return false;
  }

  if (args[0]->data_type() == MYSQL_TYPE_JSON) {
    if (json_value(args, 0, wr)) return error_json();

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
    DBUG_ASSERT(dom);
    // Pass on the DOM wrapped
    *wr = Json_wrapper(std::move(dom));
    null_value = false;
    return false;
  }

  // Not a non-binary string, nor a JSON value, wrap the rest

  if (get_json_atom_wrapper(args, 0, func_name(), &m_value,
                            &m_conversion_buffer, wr, NULL, true))
    return error_json();

  null_value = args[0]->null_value;
  return false;
}

void Item_json_typecast::print(String *str, enum_query_type query_type) {
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  str->append(')');
}

void Item_func_json_length::cleanup() {
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
}

longlong Item_func_json_length::val_int() {
  DBUG_ASSERT(fixed == 1);
  longlong result = 0;

  Json_wrapper wrapper;

  try {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value) {
      null_value = true;
      return 0;
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  if (arg_count > 1) {
    if (m_path_cache.parse_and_cache_path(args, 1, true)) return error_int();
    const Json_path *json_path = m_path_cache.get_path(1);
    if (json_path == nullptr) {
      null_value = true;
      return 0;
    }

    Json_wrapper_vector hits(key_memory_JSON);
    if (wrapper.seek(*json_path, json_path->leg_count(), &hits, true, true))
      return error_int(); /* purecov: inspected */

    if (hits.size() != 1) {
      // path does not exist. return null.
      null_value = true;
      return 0;
    }

    // there should only be one hit because wildcards were forbidden
    DBUG_ASSERT(hits.size() == 1);

    wrapper = std::move(hits[0]);
  }

  result = wrapper.length();

  null_value = false;
  return result;
}

longlong Item_func_json_depth::val_int() {
  DBUG_ASSERT(fixed);
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

  const Json_dom *dom = wrapper.to_dom(current_thd);
  return dom == nullptr ? error_int() : dom->depth();
}

bool Item_func_json_keys::val_json(Json_wrapper *wr) {
  DBUG_ASSERT(fixed == 1);

  Json_wrapper wrapper;

  try {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper))
      return error_json();
    if (args[0]->null_value) {
      null_value = true;
      return false;
    }

    if (arg_count > 1) {
      if (m_path_cache.parse_and_cache_path(args, 1, true)) return error_json();
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
    for (Json_wrapper_object_iterator i(wrapper.object_iterator()); !i.empty();
         i.next()) {
      if (res->append_alias(new (std::nothrow) Json_string(i.elt().first)))
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
  DBUG_ASSERT(fixed == 1);

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

    for (uint32 i = 1; i < arg_count; ++i) {
      if (m_path_cache.parse_and_cache_path(args, i, false))
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
      const THD *thd = current_thd;
      for (Json_wrapper &w : v) {
        if (a->append_clone(w.to_dom(thd)))
          return error_json(); /* purecov: inspected */
      }
      *wr = Json_wrapper(std::move(a));
    } else  // one path, no ellipsis or wildcard
    {
      // there should only be one match
      DBUG_ASSERT(v.size() == 1);
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

bool Item_func_json_extract::eq(const Item *item, bool binary_cmp) const {
  if (this == item) return true;
  if (item->type() != FUNC_ITEM) return false;
  const auto item_func = down_cast<const Item_func *>(item);
  if (arg_count != item_func->arg_count ||
      strcmp(func_name(), item_func->func_name()) != 0)
    return false;

  auto cmp = [binary_cmp](const Item *arg1, const Item *arg2) {
    /*
      JSON_EXTRACT doesn't care about the collation of its arguments. String
      literal arguments are considered equal if they have the same character
      set and binary contents, even if their collations differ.
    */
    const bool ignore_collation =
        binary_cmp ||
        (arg1->type() == STRING_ITEM &&
         my_charset_same(arg1->collation.collation, arg2->collation.collation));
    return arg1->eq(arg2, ignore_collation);
  };
  const auto item_json = down_cast<const Item_func_json_extract *>(item);
  return std::equal(args, args + arg_count, item_json->args, cmp);
}

#ifndef DBUG_OFF
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
#endif  // DBUG_OFF

bool Item_func_json_array_append::val_json(Json_wrapper *wr) {
  DBUG_ASSERT(fixed == 1);

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
      Json_dom *doc = docw.to_dom(thd);
      if (!doc) return error_json(); /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true)) return error_json();
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
      DBUG_ASSERT(hits.size() == 1);
      Json_dom *hit = hits[0];

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer, &valuew))
        return error_json();

      Json_dom_ptr val_dom(valuew.to_dom(thd));
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
        Json_dom *parent = hit->parent();
        if (parent == nullptr)  // root
        {
          DBUG_ASSERT(possible_root_path(path->begin(), path->end()));
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
  DBUG_ASSERT(fixed == 1);

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
      Json_dom *doc = docw.to_dom(thd);
      if (!doc) return error_json(); /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true)) return error_json();
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
      DBUG_ASSERT(hits.size() == 1);
      Json_dom *hit = hits[0];

      // What did we specify in the path, object or array?
      if (leg->get_type() == jpl_array_cell) {
        // We specified an array, what did we find at that position?
        if (hit->json_type() == enum_json_type::J_ARRAY) {
          // We found an array, so either prepend or append.
          Json_array *arr = down_cast<Json_array *>(hit);
          size_t pos = leg->first_array_index(arr->size()).position();
          if (arr->insert_alias(pos, valuew.clone_dom(thd)))
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
              newarr->insert_alias(pos, valuew.clone_dom(thd))) {
            return error_json(); /* purecov: inspected */
          }

          /*
            Now we need this value to replace the old document we found using
            path. If this is root, this is trivial, but if it's inside an
            array or object, we need to find the parent DOM to be able to
            replace it in situ.
          */
          Json_dom *parent = hit->parent();
          if (parent == nullptr)  // root
          {
            DBUG_ASSERT(possible_root_path(path->begin(), path->end() - 1));
            docw = Json_wrapper(std::move(newarr));
          } else {
            parent->replace_dom_in_container(hit, std::move(newarr));
          }
        }
      } else if (leg->get_type() == jpl_member &&
                 hit->json_type() == enum_json_type::J_OBJECT) {
        Json_object *o = down_cast<Json_object *>(hit);
        if (o->add_clone(leg->get_member_name(), valuew.to_dom(thd)))
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
  DBUG_ASSERT(fixed == 1);

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
      Json_dom *doc = docw.to_dom(thd);
      if (!doc) return error_json(); /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true)) return error_json();
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
      DBUG_ASSERT(hits.size() == 1);
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
      DBUG_ASSERT(leg->get_type() == jpl_array_cell);
      size_t pos = leg->first_array_index(arr->size()).position();
      if (arr->insert_alias(pos, valuew.clone_dom(thd)))
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

/**
  Clone a source path to a target path, stripping out legs which are made
  redundant by the auto-wrapping rule from the WL#7909 spec and further
  extended in the WL#9831 spec:

  "If an array cell path leg or an array range path leg is evaluated against a
  non-array value, the result of the evaluation is the same as if the non-array
  value had been wrapped in a single-element array."

  @see Json_path_leg::is_autowrap

  @param[in]      source_path The original path.
  @param[in,out]  target_path The clone to be filled in.
  @param[in]      doc The document to seek through.

  @returns True if an error occurred. False otherwise.
*/
static bool clone_without_autowrapping(const Json_path *source_path,
                                       Json_path_clone *target_path,
                                       Json_wrapper *doc) {
  Json_wrapper_vector hits(key_memory_JSON);

  target_path->clear();
  for (const Json_path_leg *path_leg : *source_path) {
    if (path_leg->is_autowrap()) {
      /*
         We have a partial path of the form

         pathExpression[0]

         So see if pathExpression identifies a non-array value.
      */
      hits.clear();
      if (doc->seek(*target_path, target_path->leg_count(), &hits, false, true))
        return true; /* purecov: inspected */

      if (!hits.empty() && hits[0].type() != enum_json_type::J_ARRAY) {
        /*
          pathExpression identifies a non-array value.
          We satisfy the conditions of the rule above.
          So we can throw away the [0] leg.
        */
        continue;
      }
    }
    // The rule above is NOT satisified. So add the leg.
    if (target_path->append(path_leg)) return true; /* purecov: inspected */
  }

  return false;
}

void Item_json_func::mark_for_partial_update(const Field_json *field) {
  DBUG_ASSERT(can_use_in_partial_update());
  m_partial_update_column = field;

  if (args[0]->type() == FIELD_ITEM) {
    DBUG_ASSERT(down_cast<Item_field *>(args[0])->field == field);
  } else {
    down_cast<Item_json_func *>(args[0])->mark_for_partial_update(field);
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
  if (args[0]->type() == FIELD_ITEM)
    return down_cast<Item_field *>(args[0])->field == field;

  return args[0]->supports_partial_update(field);
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
      if (args[0]->type() == FIELD_ITEM) partial_update_buffer->length(0);
    }

    for (uint32 i = 1; i < arg_count; i += 2) {
      if (m_path_cache.parse_and_cache_path(args, i, true)) return error_json();
      const Json_path *current_path = m_path_cache.get_path(i);
      if (current_path == nullptr) goto return_null;

      // Clone the path, stripping off redundant auto-wrapping.
      if (clone_without_autowrapping(current_path, &m_path, &docw)) {
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
      Json_dom *doc = docw.to_dom(thd);
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
        DBUG_ASSERT(hits.size() == 1);
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
            if (arr->insert_alias(pos, valuew.clone_dom(thd)))
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
            DBUG_ASSERT(!leg->is_autowrap());
            Json_array_ptr newarr = create_dom_ptr<Json_array>();
            size_t pos = leg->first_array_index(1).position();
            if (newarr == nullptr || newarr->append_clone(hit) ||
                newarr->insert_alias(pos, valuew.clone_dom(thd))) {
              return error_json(); /* purecov: inspected */
            }

            /*
              Now we need this value to replace the old document we found
              using path. If this is root, this is trivial, but if it's
              inside an array or object, we need to find the parent DOM to be
              able to replace it in situ.
            */
            Json_dom *parent = hit->parent();
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
          if (o->add_clone(leg->get_member_name(), valuew.to_dom(thd)))
            return error_json(); /* purecov: inspected */

          if (logical_diffs)
            table->add_logical_diff(m_partial_update_column, m_path,
                                    enum_json_diff_operation::INSERT, &valuew);
        }
      } else {
        // We found one value, so replace semantics.
        DBUG_ASSERT(hits.size() == 1);
        Json_dom *child = hits[0];
        Json_dom *parent = child->parent();
        if (!parent) {
          Json_dom_ptr dom = valuew.clone_dom(thd);
          if (dom == nullptr) return error_json(); /* purecov: inspected */
          docw = Json_wrapper(std::move(dom));

          // No point in partial update when we replace the entire document.
          if (logical_diffs) {
            disable_logical_diffs(m_partial_update_column);
            logical_diffs = false;
          }
        } else {
          Json_dom_ptr dom = valuew.clone_dom(thd);
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
  DBUG_ASSERT(fixed == 1);

  try {
    Json_array *arr = new (std::nothrow) Json_array();
    if (!arr) return error_json(); /* purecov: inspected */
    Json_wrapper docw(arr);

    const THD *thd = current_thd;
    for (uint32 i = 0; i < arg_count; ++i) {
      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i, func_name(), &m_value,
                                &m_conversion_buffer, &valuew)) {
        return error_json();
      }

      Json_dom_ptr val_dom(valuew.to_dom(thd));
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
  DBUG_ASSERT(fixed == 1);

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
      uint32 key_idx = i++;
      uint32 value_idx = i;

      // key
      Item *key_item = args[key_idx];
      char buff[MAX_FIELD_WIDTH];
      String utf8_res(buff, sizeof(buff), &my_charset_utf8mb4_bin);
      const char *safep;   // contents of key_item, possibly converted
      size_t safe_length;  // length of safep

      if (get_json_string(key_item, &tmp_key_value, &utf8_res, &safep,
                          &safe_length)) {
        my_error(ER_JSON_DOCUMENT_NULL_KEY, MYF(0));
        return error_json();
      }

      std::string key(safep, safe_length);

      // value
      Json_wrapper valuew;
      if (get_atom_null_as_null(args, value_idx, func_name(), &m_value,
                                &m_conversion_buffer, &valuew)) {
        return error_json();
      }

      Json_dom_ptr val_dom(valuew.to_dom(thd));
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
  Item_string *default_escape = new Item_string(&my_charset_utf8mb4_bin);
  if (m_source_string_item == NULL || default_escape == NULL)
    return true; /* purecov: inspected */

  Item *like_string_item = args[2];
  bool escape_initialized = false;

  // Get the escape character, if any
  if (arg_count > 3) {
    Item *orig_escape = args[3];

    /*
      Evaluate the escape clause. For a standalone LIKE expression,
      the escape clause only has to be constant during execution.
      However, we require a stronger condition: it must be constant.
      That means that we can evaluate the escape clause at resolution time
      and copy the results from the JSON_SEARCH() args into the arguments
      for the LIKE node which we're fabricating.
    */
    if (!orig_escape->const_item()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }

    StringBuffer<16> buffer;  // larger than common case: one character + '\0'
    String *escape_str = orig_escape->val_str(&buffer);
    if (thd->is_error()) return true;
    if (escape_str) {
      uint escape_length = static_cast<uint>(escape_str->length());
      default_escape->set_str_with_copy(escape_str->ptr(), escape_length);
      escape_initialized = true;
    }
  }

  if (!escape_initialized) {
    default_escape->set_str_with_copy("\\", 1);
  }

  m_like_node = new Item_func_like(m_source_string_item, like_string_item,
                                   default_escape, true);
  if (m_like_node == NULL) return true; /* purecov: inspected */

  Item *like_args[3];
  like_args[0] = m_source_string_item;
  like_args[1] = like_string_item;
  like_args[2] = default_escape;

  if (m_like_node->fix_fields(thd, like_args)) return true;

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
      uint len = static_cast<uint>(wrapper.get_data_length());
      source_string->set_str_with_copy(data, len);
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
      for (Json_wrapper_object_iterator jwot(wrapper.object_iterator());
           !jwot.empty(); jwot.next()) {
        std::pair<const std::string, Json_wrapper> pair = jwot.elt();
        // recurse with the member added to the path
        if (Json_path_leg(pair.first).to_string(path) ||
            find_matches(pair.second, path, matches, duplicates, one_match,
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

    default: { break; }
  }  // end switch on wrapper type

  return false;
}

bool Item_func_json_search::val_json(Json_wrapper *wr) {
  DBUG_ASSERT(fixed == 1);

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
    bool one_match;
    switch (parse_and_cache_ooa(args[1], &m_cached_ooa, func_name())) {
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
      default: { return error_json(); }
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
        if (m_path_cache.parse_and_cache_path(args, i, false))
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
          Json_dom *dom = docw.to_dom(current_thd);
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

          DBUG_ASSERT(hits.size() == 1);  // no wildcards

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
  DBUG_ASSERT(fixed == 1);

  Json_wrapper wrapper;
  uint32 path_count = arg_count - 1;
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

    for (uint path_idx = 0; path_idx < path_count; ++path_idx) {
      if (m_path_cache.parse_and_cache_path(args, path_idx + 1, true))
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
    DBUG_ASSERT(!wrapper.is_dom());
    partial_update_buffer = table->get_partial_update_buffer();
    // Reset the buffer in the innermost call.
    if (args[0]->type() == FIELD_ITEM) partial_update_buffer->length(0);
  } else {
    // If we cannot do binary update, let's work on the DOM instead.
    dom = wrapper.to_dom(current_thd);
    if (dom == nullptr) return error_json(); /* purecov: inspected */
  }

  // remove elements identified by the paths, one after the other
  Json_dom_vector hits(key_memory_JSON);
  Json_path_clone path;
  for (uint path_idx = 0; path_idx < path_count; ++path_idx) {
    if (clone_without_autowrapping(m_path_cache.get_path(path_idx + 1), &path,
                                   &wrapper))
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

      DBUG_ASSERT(hits.size() == 1);
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
  DBUG_ASSERT(fixed == 1);

  Json_dom_ptr result_dom;

  try {
    const THD *thd = current_thd;
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
      Json_dom_ptr next_dom(next_wrapper.to_dom(thd));
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
  DBUG_ASSERT(fixed == 1);

  String *res = args[0]->val_str(str);
  if (!res) {
    null_value = true;
    return NULL;
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
      return NULL;
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
      DBUG_ASSERT(safep == res->ptr());
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
  DBUG_ASSERT(fixed == 1);

  try {
    if (args[0]->data_type() == MYSQL_TYPE_JSON) {
      Json_wrapper wr;
      if (get_json_wrapper(args, 0, str, func_name(), &wr)) {
        return error_str();
      }

      if (args[0]->null_value) {
        null_value = true;
        return NULL;
      }

      m_value.length(0);

      if (wr.to_string(&m_value, false, func_name())) {
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
      return NULL;
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

    StringBuffer<STRING_BUFFER_USUAL_SIZE> buf;
    const char *utf8text;
    size_t utf8len;
    if (ensure_utf8mb4(*res, &buf, &utf8text, &utf8len, true))
      return error_str();
    String *utf8str = (res->ptr() == utf8text) ? res : &buf;
    DBUG_ASSERT(utf8text == utf8str->ptr());

    if (utf8len < 2 || utf8text[0] != '"' || utf8text[utf8len - 1] != '"') {
      null_value = false;
      // Return string unchanged, but convert to utf8mb4 if needed.
      if (res == utf8str) return res;
      if (str->copy(utf8text, utf8len, collation.collation))
        return error_str(); /* purecov: inspected */
      return str;
    }

    Json_dom_ptr dom;
    bool parse_error = false;
    if (parse_json(*utf8str, 0, func_name(), &dom, true, &parse_error)) {
      return error_str();
    }

    /*
      Extract the internal string representation as a MySQL string
    */
    DBUG_ASSERT(dom->json_type() == enum_json_type::J_STRING);
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
  DBUG_ASSERT(fixed);
  try {
    Json_wrapper wr;
    if (get_json_wrapper(args, 0, str, func_name(), &wr)) return error_str();

    null_value = args[0]->null_value;
    if (null_value) return nullptr;

    str->length(0);
    if (wr.to_pretty_string(str, func_name()))
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
  DBUG_ASSERT(fixed);

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

  if (wrapper.to_binary(current_thd, &buffer))
    return error_int(); /* purecov: inspected */
  return buffer.length();
}

longlong Item_func_json_storage_free::val_int() {
  DBUG_ASSERT(fixed);

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
  if (wrapper.get_free_space(&space))
    return error_int(); /* purecov: inspected */

  return space;
}

bool Item_func_json_merge_patch::val_json(Json_wrapper *wr) {
  DBUG_ASSERT(fixed);

  try {
    if (get_json_wrapper(args, 0, &m_value, func_name(), wr))
      return error_json();

    null_value = args[0]->null_value;

    Json_wrapper patch_wr;
    const THD *thd = current_thd;
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
        target_dom.reset(down_cast<Json_object *>(wr->to_dom(thd)));
        wr->set_alias();
      } else {
        target_dom = create_dom_ptr<Json_object>();
      }

      if (target_dom == nullptr) return error_json(); /* purecov: inspected */

      // Get the DOM representation of the patch object.
      Json_object_ptr patch_dom(down_cast<Json_object *>(patch_wr.to_dom(thd)));
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
