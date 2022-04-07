#include "sql/Item_sum_shortest_dir_path.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <utility>  // std::forward
#include <functional>

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
    THD *thd, Item_sum *item, unique_ptr_destroy_only<Json_wrapper> wrapper)
    : Item_sum_json(std::move(wrapper), thd, item),
      m_edge_map(key_memory_Dijkstra), m_point_map(key_memory_Dijkstra) {}

Item_sum_shortest_dir_path::Item_sum_shortest_dir_path(
    const POS &pos, PT_item_list *args,
    unique_ptr_destroy_only<Json_wrapper> wrapper)
    : Item_sum_json(std::move(wrapper), pos, args, nullptr),
      m_edge_map(key_memory_Dijkstra), m_point_map(key_memory_Dijkstra) {}

bool Item_sum_shortest_dir_path::val_json(Json_wrapper *wr) {
  assert(!m_is_window_function);

  const THD *thd = base_query_block->parent_lex->thd;
  static std::function stop_dijkstra = [&thd]() -> bool {
    return thd->is_error() || thd->is_fatal_error() || thd->is_killed();
  };
  static std::function null_heuristic = [](const int&) -> double {
    return 0.0;
  };
  static std::function geom_heuristic = [](const int&) -> double {
    return 0.0; // TODO implement
  };
  std::function<double(const int&)>& heuristic = true ? null_heuristic : geom_heuristic;

  try {
    Json_array_ptr arr(new (std::nothrow) Json_array());
    if (arr == nullptr)
      return error_json();
    Dijkstra dijkstra(&m_edge_map, heuristic, key_memory_Dijkstra);
    double cost;
    std::vector<const Edge*> path = dijkstra(m_begin_node, m_end_node, cost, stop_dijkstra);
    if (stop_dijkstra())
      return error_json();
    // jsonifying path from dijkstra into arr
    for (const Edge* edge : path) {
      Json_object_ptr json_edge(new (std::nothrow) Json_object());
      if (
        json_edge == nullptr ||
        json_edge->add_alias("id", jsonify_to_heap(edge->id)) ||
        json_edge->add_alias("cost", jsonify_to_heap(edge->cost)) ||
        //json_edge->add_alias("from", jsonify_to_heap(edge->from)) ||
        //json_edge->add_alias("to", jsonify_to_heap(edge->to)) ||
        arr->append_alias(std::move(json_edge)))
          return error_json();
    }
    // inserting path and cost into obj
    Json_object_ptr obj = Json_object_ptr(new (std::nothrow) Json_object());
    if (
      obj == nullptr ||
      obj->add_alias("path", std::move(arr)) ||
      obj->add_alias("cost", jsonify_to_heap(cost)))
        return error_json();
    
    *wr = Json_wrapper(std::move(obj));
    return false;
  } catch(...) { // expects to catch std::bad_alloc
    handle_std_exception(func_name());
    return error_json();
  }
}
String *Item_sum_shortest_dir_path::val_str(String *str) {
  assert(!m_is_window_function);

  Json_wrapper wr;
  if (val_json(&wr))
    return error_str();

  str->length(0);
  if (wr.to_string(str, true, func_name()))
    return error_str();

  if(aggr) aggr->endup();

  return str;
}

void Item_sum_shortest_dir_path::clear() {
  null_value = true;

  m_edge_map.clear();
}

bool Item_sum_shortest_dir_path::add() {
  assert(fixed);
  assert(arg_count == 6);
  assert(!m_is_window_function);

  const THD *thd = base_query_block->parent_lex->thd;
  /*
     Checking if an error happened inside one of the functions that have no
     way of returning an error status. (reset_field(), update_field() or
     clear())
   */
  if (thd->is_error()) return error_json();

  int id, from_id, to_id;
  double cost;

  // verify arg 0, 1, 2
  for (size_t i = 0; i < 3; i++)
    if (verify_id_argument(args[i]))
      return true;
  // verify arg 3
  if (verify_cost_argument(args[3]))
    return true;
  // verify arg 4, 5
  // TODO only once per agg
  for (size_t i = 4; i < 6; i++)
    if (verify_const_id_argument(args[i]))
      return true;

  // get data
  id = args[0]->val_int();
  from_id = args[1]->val_int();
  to_id = args[2]->val_int();
  cost = args[3]->val_real();
  // TODO only once per agg
  m_begin_node = args[4]->val_int();
  m_end_node = args[5]->val_int();
  
  // catch any leftover type errors
  if (thd->is_error()) return true;
  for (int i = 0; i < 4; i++)
    if (args[i]->null_value)
      return true;

  // store edge
  Edge *edge = new (thd->mem_root, std::nothrow) Edge{ id, from_id, to_id, cost };
  if (edge == nullptr)
    return true;
  try {
    m_edge_map.insert(std::pair(from_id, edge));
  } catch (...) { // expects to catch std::bad_alloc
    handle_std_exception(func_name());
    return true;
  }
  return false;
}

Item *Item_sum_shortest_dir_path::copy_or_same(THD *thd) {
  assert(!m_is_window_function);
  auto wrapper = make_unique_destroy_only<Json_wrapper>(thd->mem_root);
  if (wrapper == nullptr) return nullptr;
  return new (thd->mem_root) Item_sum_shortest_dir_path(thd, this, std::move(wrapper));
}

bool Item_sum_shortest_dir_path::check_wf_semantics1(THD *thd, Query_block *select,
                                        Window_evaluation_requirements *reqs) {
  return Item_sum::check_wf_semantics1(thd, select, reqs);
}

inline bool Item_sum_shortest_dir_path::verify_const_id_argument(Item *item) {
    if (!item->const_item() || item->is_null() || item->result_type() != INT_RESULT) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return true;
    }
    return false;
}

inline bool Item_sum_shortest_dir_path::verify_id_argument(Item *item) {
    if (item->is_null() || item->result_type() != INT_RESULT) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return true;
    }
    return false;
}

inline bool Item_sum_shortest_dir_path::verify_cost_argument(Item *item) {
  if (item->is_null() || item->result_type() != REAL_RESULT) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return true;
    }
    return false;
}

inline Json_dom_ptr Item_sum_shortest_dir_path::jsonify_to_heap(const int& i) {
  return Json_dom_ptr(new (std::nothrow) Json_int(i));
}
inline Json_dom_ptr Item_sum_shortest_dir_path::jsonify_to_heap(const double& d) {
  return Json_dom_ptr(new (std::nothrow) Json_double(d));
}
