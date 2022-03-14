#include "sql/a_star.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <utility>  // std::forward

#include "decimal.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_compare.h"
#include "my_dbug.h"
#include "my_double2ulonglong.h"
#include "my_sys.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/aggregate_check.h"  // Distinct_check
#include "sql/create_field.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/dd/cache/dictionary_client.h"
#include "sql/derror.h"  // ER_THD
#include "sql/field.h"
#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometry_extraction.h"
#include "sql/gis/relops.h"
#include "sql/handler.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_json_func.h"
#include "sql/item_subselect.h"
#include "sql/json_dom.h"
#include "sql/key_spec.h"
#include "sql/mysqld.h"
#include "sql/parse_tree_helpers.h"    // PT_item_list
#include "sql/parse_tree_node_base.h"  // Parse_context
#include "sql/parse_tree_nodes.h"      // PT_order_list
#include "sql/parser_yystype.h"
#include "sql/sql_array.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"  // handle_std_exception
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_resolver.h"  // setup_order
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/srs_fetcher.h"    // Srs_fetcher
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"  // Temp_table_param
#include "sql/uniques.h"           // Unique
#include "sql/window.h"

Item_sum_shortest_dir_path::Item_sum_shortest_dir_path(
    THD *thd, Item_sum *item, unique_ptr_destroy_only<Json_wrapper> wrapper,
    unique_ptr_destroy_only<Json_object> object)
    : Item_sum_json(std::move(wrapper), thd, item),
      m_json_object(std::move(object)) {}

Item_sum_shortest_dir_path::Item_sum_shortest_dir_path(
    const POS &pos, PT_item_list *args, PT_window *w,
    unique_ptr_destroy_only<Json_wrapper> wrapper,
    unique_ptr_destroy_only<Json_object> object)
    : Item_sum_json(std::move(wrapper), pos, args, w),
      m_json_object(std::move(object)) {}

void Item_sum_shortest_dir_path::clear() {
  null_value = true;
  m_json_object->clear();

  // Set the object to the m_wrapper, but let a_star_ting keep the
  // ownership.
  *m_wrapper = Json_wrapper(m_json_object.get(), true);
  m_key_map.clear();
}

bool Item_sum_shortest_dir_path::add() {
  assert(fixed);
  assert(arg_count == 6);

  const THD *thd = base_query_block->parent_lex->thd;
  /*
     Checking if an error happened inside one of the functions that have no
     way of returning an error status. (reset_field(), update_field() or
     clear())
   */
  if (thd->is_error()) return error_json();

  try {
    // key
    Item *key_item = args[0];
    const char *safep;   // contents of key_item, possibly converted
    size_t safe_length;  // length of safep

    if (get_json_object_member_name(thd, key_item, &m_tmp_key_value,
                                    &m_conversion_buffer, &safep, &safe_length))
      return error_json();

    std::string key(safep, safe_length);
    if (m_is_window_function) {
      /*
        When a row is leaving a frame, we have two options:
        1. If rows are ordered according to the "key", then remove
        the key/value pair from Json_object if this row is the
        last row in peerset for that key.
        2. If unordered, reduce the count in the key map for this key.
        If the count is 0, remove the key/value pair from the Json_object.
      */
      if (m_window->do_inverse()) {
        auto object = down_cast<Json_object *>(m_wrapper->to_dom(thd));
        if (m_optimize)  // Option 1
        {
          if (m_window->is_last_row_in_peerset_within_frame())
            object->remove(key);
        } else  // Option 2
        {
          auto it = m_key_map.find(key);
          if (it != m_key_map.end()) {
            int count = it->second - 1;
            if (count > 0) {
              it->second = count;
            } else {
              m_key_map.erase(it);
              object->remove(key);
            }
          }
        }
        object->cardinality() == 0 ? null_value = true : null_value = false;
        return false;
      }
    }
    // value
    Json_wrapper value_wrapper;
    if (get_atom_null_as_null(args, 1, func_name(), &m_value,
                              &m_conversion_buffer, &value_wrapper))
      return error_json();

    /*
      The m_wrapper always points to m_json_object or the result of
      deserializing the result_field in reset/update_field.
    */
    Json_object *object = down_cast<Json_object *>(m_wrapper->to_dom(thd));
    if (object->add_alias(key, value_wrapper.to_dom(thd)))
      return error_json(); /* purecov: inspected */
    /*
      If rows in the window are not ordered based on "key", add this key
      to the key map.
    */
    if (m_is_window_function && !m_optimize) {
      int count = 1;
      auto it = m_key_map.find(key);
      if (it != m_key_map.end()) {
        count = count + it->second;
        it->second = count;
      } else
        m_key_map.emplace(std::make_pair(key, count));
    }

    null_value = false;
    // object will take ownership of the value
    value_wrapper.set_alias();
  } catch (...) {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  return false;
}

Item *Item_sum_shortest_dir_path::copy_or_same(THD *thd) {
  if (m_is_window_function) return this;

  auto wrapper = make_unique_destroy_only<Json_wrapper>(thd->mem_root);
  if (wrapper == nullptr) return nullptr;

  unique_ptr_destroy_only<Json_object> object{::new (thd->mem_root)
                                                  Json_object};
  if (object == nullptr) return nullptr;

  return new (thd->mem_root)
      Item_sum_shortest_dir_path(thd, this, std::move(wrapper), std::move(object));
}

bool Item_sum_shortest_dir_path::check_wf_semantics1(THD *thd, Query_block *select,
                                        Window_evaluation_requirements *reqs) {
  return Item_sum::check_wf_semantics1(thd, select, reqs);
}

