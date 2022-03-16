#include "sql/Item_sum_shortest_dir_path.h"

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
#include "sql/Dijkstras_functor.h"

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

bool Item_sum_shortest_dir_path::val_json(Json_wrapper *wr) {
  

  return Item_sum_json::val_json(wr);
}
String *Item_sum_shortest_dir_path::val_str(String *str) {
  const THD *thd = base_query_block->parent_lex->thd;
  Json_object *object = down_cast<Json_object *>(m_wrapper->to_dom(thd));

  Dijkstra dijkstra(m_edge_map);
  double cost;
  for (const Edge* edge : dijkstra(0, 2, cost)) {
    Json_double *num = new (std::nothrow) Json_double(edge->cost);
    if (object->add_alias(std::to_string(edge->id), (Json_dom*)num))
      return error_str();
  }

  str->length(0);
  if (m_wrapper->to_string(str, true, func_name())) return error_str();

  return str; //Item_sum_json::val_str(str);
}

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
  
  int id, from_id, to_id;
  double cost;
  id = args[0]->val_real();
  from_id = args[1]->val_real();
  to_id = args[2]->val_real();
  cost = args[3]->val_real();
  if (thd->is_error()) return true;
  for (int i = 0; i < 4; i++) if (args[i]->null_value) return true;

  m_edge_map.insert(std::pair<int, Edge*>(from_id, new Edge{id, from_id, to_id, cost}));


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

