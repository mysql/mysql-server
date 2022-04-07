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

// PUBLIC:

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

    if (path.empty()) {
      my_error(ER_NO_PATH_FOUND, MYF(0), func_name());
      return true;
    }
    
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
  } catch(...) { // handles std::bad_alloc
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

bool Item_sum_shortest_dir_path::fix_fields(THD *thd, Item **pItem) {
  assert(!fixed);
  assert(!m_is_window_function);
  
  if (Item_sum_json::fix_fields(thd, pItem))
    return true;

  // verify arg 0, 1, 2
  for (size_t i = 0; i < 3; i++)
    if (verify_id_argument(args[i]))
      return true;
  // verify arg 3
  if (verify_cost_argument(args[3]))
    return true;
  // verify arg 4
  // TODO verify point
  // verify arg 5, 6
  for (size_t i = 5; i < 7; i++)
    if (verify_const_id_argument(args[i]))
      return true;

  return false;
}

bool Item_sum_shortest_dir_path::add() {
  assert(arg_count == 7);

  THD *thd = base_query_block->parent_lex->thd;
  if (thd->is_error())
    return error_json();

  int id, from_id, to_id;
  double cost;

  // get data
  id = args[0]->val_int();
  from_id = args[1]->val_int();
  to_id = args[2]->val_int();
  cost = args[3]->val_real();
  add_geom(args[4], to_id, thd);
  if (m_edge_map.empty()){
    m_begin_node = args[5]->val_int();
    m_end_node = args[6]->val_int();
  }
  else if (m_begin_node != args[5]->val_int() || m_end_node != args[6]->val_int()){
    // TODO my_error
    return true;
  }

  if (m_begin_node == m_end_node)
  {
    my_error(ER_START_AND_END_NODE_CONFLICT, MYF(0), func_name());
    return true;
  }
  
  // catch any leftover type errors
  // TODO evaluate necessity
  if (thd->is_error()) return true;
  for (int i = 0; i < 7; i++)
    if (i != 4 && args[i]->null_value)
      return true;

  // store edge
  Edge *edge = new (thd->mem_root, std::nothrow) Edge{ id, from_id, to_id, cost };
  if (edge == nullptr)
    return true;
  try {
    m_edge_map.insert(std::pair(from_id, edge));
  } catch (...) { // handles std::bad_alloc
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

// PRIVATE:

inline bool Item_sum_shortest_dir_path::add_geom(Item *arg, const int& node_id, THD *thd) {
  GeometryExtractionResult geomRes = ExtractGeometry(arg, thd, func_name());

  switch (geomRes.GetResultType()) {
    case ResultType::Error:
      return true;
    case ResultType::NullValue:
      if (!m_point_map.empty()){
        //TODO my_error(ER_ALL_OR_NONE_MUST_BE_NULL)
      }
      return false;
    default: break;
  }

  gis::srid_t srid = geomRes.GetSrid();

  if (m_point_map.empty())
    m_srid = srid;
  else if (m_srid != srid){
    my_error(ER_GIS_DIFFERENT_SRIDS_AGGREGATION, MYF(0), func_name(), m_srid, srid);
    return true;
  }

  std::unique_ptr<gis::Geometry> geom = geomRes.GetValue();

  if (geom.get()->type() != gis::Geometry_type::kPoint){
    // TODO make error my_error(ER_GIS_WRONG_GEOM_TYPE, MYF(0), func_name(), "Point");
    return true;
  }

  try {
    m_point_map.insert(std::pair(node_id, std::move(geom)));
  } catch (...) { // handles std::bad_alloc
    handle_std_exception(func_name());
    return true;
  }
  return false;
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
