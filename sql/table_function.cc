/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/table_function.h"

#include <string.h>
#include <algorithm>
#include <memory>
#include <new>

#include "binary_log_types.h"
#include "m_string.h"
#include "my_sys.h"
#include "mysql/psi/psi_base.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_json_func.h"
#include "sql/json_dom.h"
#include "sql/json_path.h"
#include "sql/my_decimal.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_exception_handler.h"
#include "sql/sql_list.h"
#include "sql/sql_show.h"
#include "sql/sql_table.h"      // create_typelib
#include "sql/sql_tmp_table.h"  // create_tmp_table_from_fields
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"
#include "template_utils.h"

/******************************************************************************
  Implementation of Table_function
******************************************************************************/

bool Table_function::create_result_table(ulonglong options,
                                         const char *table_alias) {
  DBUG_ASSERT(table == nullptr);

  table = create_tmp_table_from_fields(thd, *get_field_list(), false, options,
                                       table_alias);
  return table == nullptr;
}

bool Table_function::write_row() {
  int error;

  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (!table->file->is_ignorable_error(error) &&
        create_ondisk_from_heap(thd, table, nullptr, nullptr, error, true,
                                nullptr))
      return true;  // Not a table_is_full error
  }
  return false;
}

void Table_function::empty_table() {
  DBUG_ASSERT(table->is_created());
  table->file->ha_delete_all_rows();
}

bool Table_function::init_args() {
  if (inited) return false;
  if (do_init_args()) return true;
  table->pos_in_table_list->dep_tables |= used_tables();
  inited = true;
  return false;
}

/******************************************************************************
  Implementation of JSON_TABLE function
******************************************************************************/
Table_function_json::Table_function_json(THD *thd_arg, const char *alias,
                                         Item *a, List<Json_table_column> *cols)
    : Table_function(thd_arg),
      m_columns(cols),
      m_all_columns(thd->mem_root),
      m_table_alias(alias),
      is_source_parsed(false),
      source(a) {}

List<Create_field> *Table_function_json::get_field_list() {
  // It's safe as Json_table_column is derived from Create_field
  return reinterpret_cast<List<Create_field> *>(&m_vt_list);
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
  Save JSON to a JSON_TABLE's column

  Value is saved in type-aware manner. Into a JSON-typed column any JSON
  data could be saved. Into an SQL scalar field only a scalar could be
  saved. If data being saved isn't scalar or can't be coerced to the target
  type, an error is returned.

  @param  thd   thread handler
  @param  field Column's field to save data to
  @param  col   Column to save data from
  @param  w     JSON data to save
  @param  warn  level of warning for truncation handling

  @returns
    false ok
    true  coercion error occur
*/

static bool save_json_to_column(THD *thd, Field *field, Json_table_column *col,
                                Json_wrapper *w, enum_check_fields warn) {
  bool err = false;
  if (field->type() == MYSQL_TYPE_JSON) {
    Field_json *fld = down_cast<Field_json *>(field);
    return (fld->store_json(w) != TYPE_OK);
  }

  const enum_coercion_error cr_error =
      (warn == CHECK_FIELD_ERROR_FOR_NULL) ? CE_ERROR : CE_WARNING;
  if (w->type() == enum_json_type::J_ARRAY ||
      w->type() == enum_json_type::J_OBJECT) {
    if (col->m_on_error == enum_jtc_on::JTO_ERROR)
      my_error(ER_WRONG_JSON_TABLE_VALUE, MYF(0), col->field_name);
    return true;
  }
  thd->check_for_truncated_fields = warn;
  switch (field->result_type()) {
    case INT_RESULT: {
      longlong value = w->coerce_int(col->field_name, &err, cr_error);

      // If the Json_wrapper holds a numeric value, grab the signedness from it.
      // If not, grab the signedness from the column where we are storing the
      // value.
      bool value_unsigned;
      if (w->type() == enum_json_type::J_INT) {
        value_unsigned = false;
      } else if (w->type() == enum_json_type::J_UINT) {
        value_unsigned = true;
      } else {
        value_unsigned = col->is_unsigned;
      }

      if (!err &&
          (field->store(value, value_unsigned) >= TYPE_WARN_OUT_OF_RANGE))
        err = true;
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
      if (field->is_temporal_with_date()) {
        switch (w->type()) {
          case enum_json_type::J_DATE:
          case enum_json_type::J_DATETIME:
          case enum_json_type::J_TIMESTAMP:
            date_time_handled = true;
            err = w->coerce_date(&ltime, "JSON_TABLE", cr_error);
            break;
          default:
            break;
        }
      } else if (real_type_to_type(field->type()) == MYSQL_TYPE_TIME &&
                 w->type() == enum_json_type::J_TIME) {
        date_time_handled = true;
        err = w->coerce_time(&ltime, "JSON_TABLE", cr_error);
      }
      if (date_time_handled) {
        err = err || field->store_time(&ltime);
        break;
      }
      String str;
      if (can_store_json_value_unencoded(field, w)) {
        str.set(w->get_data(), w->get_data_length(), field->charset());
      } else {
        err = w->to_string(&str, false, "JSON_TABLE");
      }

      if (!err && (field->store(str.ptr(), str.length(), str.charset()) >=
                   TYPE_WARN_OUT_OF_RANGE))
        err = true;
      break;
    }
    case REAL_RESULT: {
      double value = w->coerce_real(col->field_name, &err, cr_error);
      if (!err && (field->store(value) >= TYPE_WARN_OUT_OF_RANGE)) err = true;
      break;
    }
    case DECIMAL_RESULT: {
      my_decimal value;
      w->coerce_decimal(&value, col->field_name, &err, cr_error);
      if (!err && (field->store_decimal(&value) >= TYPE_WARN_OUT_OF_RANGE))
        err = true;
      break;
    }
    case ROW_RESULT:
    default:
      // Shouldn't happen
      DBUG_ASSERT(0);
  }
  if (err && cr_error == CE_ERROR)
    my_error(ER_JT_VALUE_OUT_OF_RANGE, MYF(0), col->field_name);

  return err;
}

/**
  Initialize columns and lists for json table

  @details This function does several things:
  1) sets up list of fields (vt_list) for result table creation
  2) fills array of all columns (m_all_columns) for execution
  3) for each column that has default ON EMPTY or ON ERROR clauses, checks
    the value to be proper json and initializes column appropriately
  4) for each column that involves path, the path is checked to be correct.
  The function goes recursively, starting from the top NESTED PATH clause
  and going in the depth-first way, traverses the tree of columns.

  @param thd       thread handler
  @param nest_idx  index of parent's element in the nesting data array
  @param parent    Parent of the NESTED PATH clause being initialized

  @returns
    false  ok
    true   an error occurred
*/

bool Table_function_json::init_json_table_col_lists(THD *thd, uint *nest_idx,
                                                    Json_table_column *parent) {
  List_iterator<Json_table_column> li(*parent->m_nested_columns);
  Json_table_column *col;
  const uint current_nest_idx = *nest_idx;
  // Used to set fast track between sibling NESTED PATH nodes
  Json_table_column *nested = nullptr;
  /*
    This need to be set up once per statement, as it doesn't change between
    EXECUTE calls.
  */
  Prepared_stmt_arena_holder ps_arena_holder(thd);

  while ((col = li++)) {
    String path;
    col->is_unsigned = (col->flags & UNSIGNED_FLAG);
    col->m_jds_elt = &m_jds[current_nest_idx];
    if (col->m_jtc_type != enum_jt_column::JTC_NESTED_PATH) {
      col->m_field_idx = m_vt_list.elements;
      m_vt_list.push_back(col);
      col->create_length_to_internal_length();
      if (check_column_name(col->field_name)) {
        my_error(ER_WRONG_COLUMN_NAME, MYF(0), col->field_name);
        return true;
      }
      if ((col->sql_type == MYSQL_TYPE_ENUM ||
           col->sql_type == MYSQL_TYPE_SET) &&
          !col->interval)
        col->interval = create_typelib(thd->mem_root, col);
    }
    m_all_columns.push_back(col);

    switch (col->m_jtc_type) {
      case enum_jt_column::JTC_ORDINALITY: {
        // No special handling is needed
        break;
      }
      case enum_jt_column::JTC_PATH: {
        path.set(col->m_path_str.str, col->m_path_str.length,
                 thd->variables.character_set_client);
        if (parse_path(&path, false, &col->m_path_json)) return true;
        if (col->m_on_empty == enum_jtc_on::JTO_DEFAULT) {
          String src(col->m_default_empty_str.str,
                     col->m_default_empty_str.length,
                     thd->variables.character_set_client);
          Json_dom_ptr dom;  //@< we'll receive a DOM here
          bool parse_error;
          if (parse_json(src, 0, "JSON_TABLE", &dom, true, &parse_error) ||
              (col->sql_type != MYSQL_TYPE_JSON && !dom->is_scalar())) {
            my_error(ER_INVALID_DEFAULT, MYF(0), col->field_name);
            return true;
          }
          col->m_default_empty_json = Json_wrapper(std::move(dom));
        }
        if (col->m_on_error == enum_jtc_on::JTO_DEFAULT) {
          String src(col->m_default_error_str.str,
                     col->m_default_error_str.length,
                     thd->variables.character_set_client);
          Json_dom_ptr dom;  //@< we'll receive a DOM here
          bool parse_error;
          if (parse_json(src, 0, "JSON_TABLE", &dom, true, &parse_error) ||
              (col->sql_type != MYSQL_TYPE_JSON && !dom->is_scalar())) {
            my_error(ER_INVALID_DEFAULT, MYF(0), col->field_name);
            return true;
          }
          col->m_default_error_json = Json_wrapper(std::move(dom));
        }
        break;
      }
      case enum_jt_column::JTC_EXISTS: {
        path.set(col->m_path_str.str, col->m_path_str.length,
                 thd->variables.character_set_client);
        if (parse_path(&path, false, &col->m_path_json)) return true;
        break;
      }
      case enum_jt_column::JTC_NESTED_PATH: {
        (*nest_idx)++;
        if (*nest_idx >= MAX_NESTED_PATH) {
          my_error(ER_JT_MAX_NESTED_PATH, MYF(0), MAX_NESTED_PATH,
                   m_table_alias);
          return true;
        }
        col->m_child_jds_elt = &m_jds[*nest_idx];

        path.set(col->m_path_str.str, col->m_path_str.length,
                 thd->variables.character_set_client);
        if (nested) {
          nested->m_next_nested = col;
          col->m_prev_nested = nested;
        }
        nested = col;

        if (parse_path(&path, false, &col->m_path_json) ||
            init_json_table_col_lists(thd, nest_idx, col))
          return true;
        break;
      }
      default:
        DBUG_ASSERT(0);
    }
  }
  return false;
}

/**
  Check whether given default values can be saved to fields

  @returns
    true    a conversion error occurred
    false   defaults can be saved or aren't specified
*/

bool Table_function_json::do_init_args() {
  Item *dummy = source;
  if (source->fix_fields(thd, &dummy)) return true;

  DBUG_ASSERT(source->data_type() != MYSQL_TYPE_VAR_STRING);
  if (source->has_aggregation() || source->has_subquery() || source != dummy) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "JSON_TABLE");
    return true;
  }
  try {
    /*
      Check whether given JSON source is a const and it's valid, see also
      Table_function_json::fill_result_table().
    */
    if (source->const_item()) {
      String buf;
      Item *args[] = {source};
      if (get_json_wrapper(args, 0, &buf, func_name(), &m_jds[0].jdata))
        return true;  // Error is already thrown
      is_source_parsed = true;
    }
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return true;
    /* purecov: end */
  }

  Json_table_column *col;
  for (uint i = 0; i < m_all_columns.size(); i++) {
    col = m_all_columns[i];
    if (col->m_jtc_type != enum_jt_column::JTC_PATH) continue;
    DBUG_ASSERT(col->m_field_idx >= 0);
    if (col->m_on_empty == enum_jtc_on::JTO_DEFAULT) {
      if (save_json_to_column(thd, get_field(col->m_field_idx), col,
                              &col->m_default_empty_json, CHECK_FIELD_WARN)) {
        my_error(ER_INVALID_DEFAULT, MYF(0), col->field_name);
        return true;
      }
    }
    if (col->m_on_error == enum_jtc_on::JTO_DEFAULT) {
      if (save_json_to_column(thd, get_field(col->m_field_idx), col,
                              &col->m_default_error_json, CHECK_FIELD_WARN)) {
        my_error(ER_INVALID_DEFAULT, MYF(0), col->field_name);
        return true;
      }
    }
  }
  return false;
}

bool Table_function_json::init() {
  Json_table_column top({nullptr, 0}, m_columns);
  if (m_vt_list.elements == 0) {
    uint nest_idx = 0;
    if (init_json_table_col_lists(thd, &nest_idx, &top)) return true;
    List_iterator<Json_table_column> li(m_vt_list);

    /*
      Check for duplicate names.
      Two iterators over vt_list are used. First is used to get a field,
      second - to compare the field with fields in the rest of the list.
      For each iteration of the first list, we skip fields prior to the
      first iterator's field.
    */
    Json_table_column *first;
    while ((first = li++)) {
      Json_table_column *col;
      List_iterator<Json_table_column> li2(m_vt_list);
      // Compare 'first' with all columns prior to it
      while ((col = li2++) && col != first) {
        if (!strncmp(first->field_name, col->field_name, NAME_CHAR_LEN)) {
          my_error(ER_DUP_FIELDNAME, MYF(0), first->field_name);
          return true;
        }
      }
    }
  }
  return false;
}

/**
  A helper function which sets all columns under given NESTED PATH column
  to nullptr. Used to evaluate sibling NESTED PATHS.

  @param       root  root NESTED PATH column
  @param [out] last  last column which belongs to the given NESTED PATH
*/

void Table_function_json::set_subtree_to_null(Json_table_column *root,
                                              Json_table_column **last) {
  List_iterator<Json_table_column> li(*root->m_nested_columns);
  Json_table_column *col;
  while ((col = li++)) {
    *last = col;
    switch (col->m_jtc_type) {
      case enum_jt_column::JTC_NESTED_PATH:
        set_subtree_to_null(col, last);
        break;
      default:
        get_field(col->m_field_idx)->set_null();
        break;
    }
  }
}

/**
  Fill a json table column

  @details Fills a column with data, according to specification in
  JSON_TABLE. This function handles all kinds of columns:
  Ordinality)  just saves the counter into the column's field
  Path)        extracts value, saves it to the column's field and handles
               ON ERROR/ON EMPTY clauses
  Exists)      checks the path existence and saves either 1 or 0 into result
               field
  Nested path) matches the path expression against data source. If there're
               matches, this function sets NESTED PATH's iterator over those
               matches and resets ordinality counter.

  @param[in]   fld   Column's field to save data to
  @param[out]  skip  true <=> it's a NESTED PATH node and its path
                     expression didn't return any matches or a
                     previous sibling NESTED PATH clause still producing
                     records, thus all columns of this NESTED PATH node
                     should be skipped

  @returns
    false column is filled
    true  an error occurred, execution should be stopped
*/

bool Json_table_column::fill_column(Field *fld, jt_skip_reason *skip) {
  *skip = JTS_NONE;

  if (m_jtc_type != enum_jt_column::JTC_NESTED_PATH) {
    fld->set_notnull();
    DBUG_ASSERT(m_field_idx == fld->field_index);
  }

  switch (m_jtc_type) {
    case enum_jt_column::JTC_ORDINALITY: {
      if (fld->store(m_jds_elt->m_rowid, true)) return true;
      break;
    }
    case enum_jt_column::JTC_PATH: {
      THD *thd = fld->table->in_use;
      // Vector of matches
      Json_wrapper_vector data_v(key_memory_JSON);
      m_jds_elt->jdata.seek(m_path_json, m_path_json.leg_count(), &data_v, true,
                            false);
      if (data_v.size() > 0) {
        Json_wrapper buf;
        bool is_error = false;
        enum_check_fields warn;
        // Always issue at least a warning on truncation
        if (m_on_error == enum_jtc_on::JTO_ERROR) {
          // Issue an error when data is truncated on saving into field
          warn = CHECK_FIELD_ERROR_FOR_NULL;
        } else
          warn = CHECK_FIELD_WARN;
        if (data_v.size() > 1) {
          // Make result array
          if (fld->type() == MYSQL_TYPE_JSON) {
            Json_array *a = new (std::nothrow) Json_array();
            if (!a) return true;
            for (Json_wrapper &w : data_v) {
              if (a->append_alias(w.clone_dom(thd))) {
                delete a; /* purecov: inspected */
                return true;
              }
            }
            buf = Json_wrapper(a);
          } else {
            is_error = true;
            // Thrown an error when save_json_to_column() isn't called
            if (m_on_error == enum_jtc_on::JTO_ERROR)
              my_error(ER_WRONG_JSON_TABLE_VALUE, MYF(0), field_name);
          }
        } else
          buf = std::move(data_v[0]);
        is_error = is_error || save_json_to_column(thd, fld, this, &buf, warn);
        if (is_error) switch (m_on_error) {
            case enum_jtc_on::JTO_ERROR: {
              return true;
              break;
            }
            case enum_jtc_on::JTO_DEFAULT: {
              save_json_to_column(thd, fld, this, &m_default_error_json,
                                  CHECK_FIELD_IGNORE);
              break;
            }
            case enum_jtc_on::JTO_NULL:
            default: {
              fld->set_null();
              break;
            }
          }
      } else {
        switch (m_on_empty) {
          case enum_jtc_on::JTO_ERROR: {
            my_error(ER_MISSING_JSON_TABLE_VALUE, MYF(0), field_name);
            return true;
          }
          case enum_jtc_on::JTO_DEFAULT: {
            save_json_to_column(thd, fld, this, &m_default_empty_json,
                                CHECK_FIELD_IGNORE);
            break;
          }
          case enum_jtc_on::JTO_NULL:
          default: {
            fld->set_null();
            break;
          }
        }
      }
      break;
    }
    case enum_jt_column::JTC_EXISTS: {
      // Vector of matches
      Json_wrapper_vector data_v(key_memory_JSON);
      m_jds_elt->jdata.seek(m_path_json, m_path_json.leg_count(), &data_v, true,
                            true);
      if (data_v.size() >= 1)
        fld->store(1, true);
      else
        fld->store(0, true);
      break;
    }
    case enum_jt_column::JTC_NESTED_PATH: {
      // If this node sends data, advance ts iterator
      if (m_child_jds_elt->producing_records) {
        ++m_child_jds_elt->it;
        m_child_jds_elt->m_rowid++;

        if ((m_child_jds_elt->it != m_child_jds_elt->v.end()))
          m_child_jds_elt->jdata = std::move(*m_child_jds_elt->it);
        else {
          m_child_jds_elt->producing_records = false;
          *skip = JTS_EOD;
        }
        return false;
      }
      // Run only one sibling nested path at a time
      for (Json_table_column *tc = m_prev_nested; tc; tc = tc->m_prev_nested) {
        DBUG_ASSERT(tc->m_jtc_type == enum_jt_column::JTC_NESTED_PATH);
        if (tc->m_child_jds_elt->producing_records) {
          *skip = JTS_SIBLING;
          return false;
        }
      }
      m_child_jds_elt->v.clear();
      if (m_jds_elt->jdata.seek(m_path_json, m_path_json.leg_count(),
                                &m_child_jds_elt->v, true, false))
        return true;
      if (m_child_jds_elt->v.size() == 0) {
        *skip = JTS_EOD;
        return false;
      }
      m_child_jds_elt->it = m_child_jds_elt->v.begin();
      m_child_jds_elt->producing_records = true;
      m_child_jds_elt->m_rowid = 1;
      m_child_jds_elt->jdata = std::move(*m_child_jds_elt->it);
      break;
    }
    default: {
      DBUG_ASSERT(0);
      break;
    }
  }
  return false;
}

void Json_table_column::cleanup() {
  // Restore original length as it was adjusted according to charset
  length = char_length;
  // Reset paths and wrappers to free allocated memory.
  m_path_json = Json_path();
  if (m_on_empty == enum_jtc_on::JTO_DEFAULT)
    m_default_empty_json = Json_wrapper();
  if (m_on_error == enum_jtc_on::JTO_DEFAULT)
    m_default_error_json = Json_wrapper();
}

/**
  Fill json table

  @details This function goes along the flattened list of columns and
  updates them by calling fill_column(). As it goes, it pushes all nested
  path nodes to 'nested' list, using it as a stack. After writing a row, it
  checks whether there's more data in the right-most nested path (top in the
  stack). If there is, it advances path's iterator, if no - pops the path
  from stack and goes to the next nested path (i.e more to left). When stack
  is empty, then the loop is over and all data (if any) was stored in the table,
  and function exits. Otherwise, the list of columns is positioned to the top
  nested path in the stack and incremented to the column after the nested
  path, then the loop of updating columns is executed again. So, whole
  execution could look as follows:

      columns (                      <-- npr
        cr1,
        cr2,
        nested path .. columns (     <-- np1
          c11,
          nested path .. columns (   <-- np2
            c21
          )
        )
      )

      iteration | columns updated in the loop
      1           npr cr1 cr2 np1 c11 np2 c21
      2                                   c21
      3                                   c21
      4                           c11 np2 c21
      5                                   c21
      6                           c11 np2 c21
      7                                   c21
      8           npr cr1 cr2 np1 c11 np2 c21
      9                                   c21
     10                           c11 np2 c21
  Note that result table's row isn't automatically reset and if a column
  isn't updated, its data is written multiple times. E.g. cr1 in the
  example above is updated 2 times, but is written 10 times. This allows to
  save cycles on updating fields that for sure haven't been changed.

  When there's sibling nested paths, i.e two or more nested paths in the
  same columns clause, then they're processed one at a time. Started with
  first, and the rest are set to null with help f set_subtree_to_null().
  When the first sibling nested path runs out of rows, it's set to null and
  processing moves on to the next one.

  @returns
    false table filled
    true  error occured
*/

bool Table_function_json::fill_json_table() {
  // 'Stack' of nested NESTED PATH clauses
  Prealloced_array<uint, MAX_NESTED_PATH> nested(PSI_NOT_INSTRUMENTED);
  // The column being processed
  uint col_idx = 0;
  jt_skip_reason skip_subtree;
  const enum_check_fields check_save = thd->check_for_truncated_fields;

  do {
    skip_subtree = JTS_NONE;
    /*
      When a NESTED PATH runs out of matches, we set it to null, and
      continue filling the row, so next sibling NESTED PATH could start
      sending rows. But if there's no such NESTED PATH, then this row must be
      skipped as it's not a result of a match.
    */
    bool skip_row = true;
    for (; col_idx < m_all_columns.size(); col_idx++) {
      /*
        When NESTED PATH doesn't have a match for any reason, set its
        columns to nullptr.
      */
      Json_table_column *col = m_all_columns[col_idx];
      if (col->fill_column(
              (col->m_field_idx >= 0 ? get_field(col->m_field_idx) : nullptr),
              &skip_subtree))
        return true;
      if (skip_subtree) {
        set_subtree_to_null(col, &col);
        // Position iterator to the last element of subtree
        while (m_all_columns[col_idx] != col) col_idx++;
      } else if (col->m_jtc_type == enum_jt_column::JTC_NESTED_PATH) {
        nested.push_back(col_idx);
        // Found a NESTED PATH which produced a record
        skip_row = false;
      }
    }
    if (!skip_row) write_row();
    // Find next nested path and advance its iterator.
    if (nested.size() > 0) {
      uint j = nested.back();
      nested.pop_back();
      Json_table_column *col = m_all_columns[j];

      /*
        When there're sibling NESTED PATHs and the first one is producing
        records, second one will skip_subtree and we need to reset it here,
        as it's not relevant.
      */
      if (col->m_child_jds_elt->producing_records) skip_subtree = JTS_NONE;
      col_idx = j;
    }
  } while (nested.size() != 0 || skip_subtree != JTS_EOD);

  thd->check_for_truncated_fields = check_save;
  return false;
}

bool Table_function_json::fill_result_table() {
  String buf;
  // reset table
  empty_table();

  try {
    Item *args[] = {source};
    /*
      There are 3 possible cases of data source expression const-ness:

      1. Always const, e.g. a plain string, source will be parsed once at
         Table_function_json::init()
      2. Non-const during init(), but become const after it, e.g a field from a
         const table: source will be parsed here ONCE
      3. Non-const, e.g. a table field: source will be parsed here EVERY TIME
         fill_result_table() is called
    */
    if (((!source->const_item() || !is_source_parsed) &&
         get_json_wrapper(args, 0, &buf, func_name(), &m_jds[0].jdata)) ||
        args[0]->null_value)
      // No need to set null_value as it's not used by table functions
      return 0;
    is_source_parsed = true;
    return fill_json_table();
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return true;
    /* purecov: end */
  }
  return 0;
}

static bool print_on_empty_error(String *str, enum_jtc_on jto,
                                 LEX_STRING *default_str) {
  switch (jto) {
    case enum_jtc_on::JTO_ERROR:
      return str->append(STRING_WITH_LEN(" error on "));
    case enum_jtc_on::JTO_NULL:
      return str->append(STRING_WITH_LEN(" null on "));
    case enum_jtc_on::JTO_DEFAULT: {
      return (str->append(STRING_WITH_LEN(" default '")) ||
              str->append(default_str) || str->append(STRING_WITH_LEN("' on")));
      break;
    }
    default:
      DBUG_ASSERT(0);
  };
  return false;
}

bool Table_function_json::print_nested_path(Json_table_column *col, String *str,
                                            enum_query_type query_type) {
  if (str->append('\'') || str->append(col->m_path_str) ||
      str->append(STRING_WITH_LEN("' columns (")))
    return true;
  Json_table_column *jtc;
  List_iterator<Json_table_column> li(*col->m_nested_columns);
  bool first = true;
  while ((jtc = li++)) {
    if (!first) {
      if (str->append(STRING_WITH_LEN(", "))) return true;
    } else
      first = false;

    switch (jtc->m_jtc_type) {
      case enum_jt_column::JTC_ORDINALITY: {
        if (str->append(jtc->field_name, strlen(jtc->field_name)) ||
            str->append(STRING_WITH_LEN(" for ordinality")))
          return true;
        break;
      }
      case enum_jt_column::JTC_EXISTS:
      case enum_jt_column::JTC_PATH: {
        String type(15);
        if (str->append(jtc->field_name, strlen(jtc->field_name)) ||
            str->append(' '))
          return true;
        uint16 data = 0;
        Field *fld = get_field(jtc->m_field_idx);
        /*
          save_field_metadata + show_sql_type is broken for types below.
          I.e. the former produces data which the latter doesn't print
          properly.
        */
        switch (jtc->sql_type) {
          case MYSQL_TYPE_STRING:
            // Encode field's length so show_sql_type() prints it correctly
            data = (((fld->field_length & 0x300) ^ 0x300) << 4) +
                   (fld->field_length & 0xff);
            break;
          case MYSQL_TYPE_VAR_STRING:
          case MYSQL_TYPE_VARCHAR:
            data = fld->field_length;
            break;
          default:
            fld->save_field_metadata((uchar *)&data);
        }
        show_sql_type(jtc->sql_type, data, &type, fld->charset());
        str->append(type);
        if (jtc->m_jtc_type == enum_jt_column::JTC_EXISTS) {
          if (str->append(STRING_WITH_LEN(" exists"))) return true;
        }
        if (str->append(STRING_WITH_LEN(" path '")) ||
            str->append(jtc->m_path_str) || str->append('\''))
          return true;
        if (jtc->m_jtc_type == enum_jt_column::JTC_EXISTS) break;
        if (jtc->m_on_empty != enum_jtc_on::JTO_IMPLICIT) {
          print_on_empty_error(str, jtc->m_on_empty, &jtc->m_default_empty_str);
          if (str->append(STRING_WITH_LEN(" empty"))) return true;
        }
        if (jtc->m_on_error != enum_jtc_on::JTO_IMPLICIT) {
          if (print_on_empty_error(str, jtc->m_on_error,
                                   &jtc->m_default_error_str) ||
              str->append(STRING_WITH_LEN(" error")))
            return true;
        }
        break;
      }
      case enum_jt_column::JTC_NESTED_PATH: {
        if (str->append(STRING_WITH_LEN("nested path ")) ||
            print_nested_path(jtc, str, query_type))
          return true;
        break;
      }
    };
  }
  return str->append(')');
}

bool Table_function_json::print(String *str, enum_query_type query_type) {
  if (str->append(STRING_WITH_LEN("json_table("))) return true;
  source->print(str, query_type);
  return (thd->is_error() || str->append(STRING_WITH_LEN(", ")) ||
          print_nested_path(m_columns->head(), str, query_type) ||
          str->append(')'));
}

table_map Table_function_json::used_tables() { return source->used_tables(); };

void Table_function_json::do_cleanup() {
  source->cleanup();
  for (uint i = 0; i < MAX_NESTED_PATH; i++) m_jds[i].cleanup();
  for (uint i = 0; i < m_all_columns.size(); i++) m_all_columns[i]->cleanup();
  m_all_columns.clear();
  m_vt_list.empty();
}

void JT_data_source::cleanup() {
  jdata = Json_wrapper();
  v.clear();
  v.shrink_to_fit();
  producing_records = false;
}
