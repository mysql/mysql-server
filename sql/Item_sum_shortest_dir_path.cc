#include "sql/Item_sum_shortest_dir_path.h"

// PUBLIC:

Item_sum_shortest_dir_path::Item_sum_shortest_dir_path(
    THD *thd, Item_sum *item, unique_ptr_destroy_only<Json_wrapper> wrapper)
    : Item_sum_json(std::move(wrapper), thd, item),
      m_edge_map(key_memory_Dijkstra), m_point_map(key_memory_Dijkstra),
      m_edge_ids(key_memory_Dijkstra) {}

Item_sum_shortest_dir_path::Item_sum_shortest_dir_path(
    const POS &pos, PT_item_list *args,
    unique_ptr_destroy_only<Json_wrapper> wrapper)
    : Item_sum_json(std::move(wrapper), pos, args, nullptr),
      m_edge_map(key_memory_Dijkstra), m_point_map(key_memory_Dijkstra),
      m_edge_ids(key_memory_Dijkstra) {}

bool Item_sum_shortest_dir_path::val_json(Json_wrapper *wr) {
  assert(!m_is_window_function);

  const THD *thd = base_query_block->parent_lex->thd;

  static std::function stop_dijkstra = [&thd]() -> bool {
    return thd->is_error() || thd->is_fatal_error() || thd->is_killed();
  };

  std::function heuristic = [](const int&) -> double {
    return 0.0;
  };
  
  if (!m_point_map.empty()){
    // no path can exist if end_point geom doesn't exist in non empty node set
    // begin_node geom is not needed
    if (m_point_map.find(m_end_node) == m_point_map.end()) {
      my_error(ER_NO_PATH_FOUND, MYF(0), func_name());
      return true;
    }
    const gis::Geometry *end_geom = m_point_map.at(m_end_node).get();
    heuristic = [this, end_geom](const int& node) -> double {
      static gis::Distance dst(NAN, NAN);
      return dst(end_geom, this->m_point_map.at(node).get());
    };
  }

  Json_array_ptr arr(new (std::nothrow) Json_array());
  if (arr == nullptr)
    return error_json();
  
  double cost;
  int popped_points;
  std::vector<Edge> path;
  try {
    // Dijkstra's externally allocated memory (my_malloc)
    std::deque<void*> allocated_memory;
    {
      Dijkstra dijkstra(&m_edge_map, heuristic, [&allocated_memory](const size_t n) -> void* {
        void* p = my_malloc(key_memory_Dijkstra, n, MYF(MY_WME | ME_FATALERROR));
        allocated_memory.push_front(p);
        return p;
      });
      path = dijkstra(m_begin_node, m_end_node, &cost, &popped_points, stop_dijkstra);
    }
    // deallocating Dijkstra's borrowed memory
    for (void* p : allocated_memory)
      my_free(p);
  } catch(...) { // handles std::bad_alloc and gis_exceptions from gis::Distance
    handle_std_exception(func_name());
    handle_gis_exception(func_name());
    return error_json();
  }

  if (path.empty()) {
    my_error(ER_NO_PATH_FOUND, MYF(0), func_name());
    return true;
  }
  
  if (stop_dijkstra())
    return error_json();
  // jsonifying path from dijkstra into arr
  for (const Edge edge : path) {
    Json_object_ptr json_edge(new (std::nothrow) Json_object());
    if (
      json_edge == nullptr ||
      json_edge->add_alias("id", jsonify_to_heap(edge.id)) ||
      json_edge->add_alias("cost", jsonify_to_heap(edge.cost)) ||
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
    obj->add_alias("cost", jsonify_to_heap(cost)) ||
    obj->add_alias("visited_nodes", jsonify_to_heap(popped_points)))
      return error_json();
  
  *wr = Json_wrapper(std::move(obj));
  return false;
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
  m_point_map.clear();
  m_edge_ids.clear();
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
  // * skips arg 4 (geom)
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
  // id error
  if (m_edge_ids.find(id) != m_edge_ids.end()){
    my_error(ER_DUPLICATE_EDGE_ID, MYF(0), func_name(), id);
    return true;
  }
  // cost error
  if (cost <= 0){
    my_error(ER_NEGATIVE_OR_ZERO_EDGE_COST, MYF(0), func_name(), cost, id);
    return true;
  }
  // from/to error
  if (from_id == to_id){
    my_error(ER_EDGE_LOOP, MYF(0), func_name(), id);
    return true;
  }
  m_edge_ids[id] = true;
  // begin/end error
  if (m_edge_map.empty()){
    m_begin_node = args[5]->val_int();
    m_end_node = args[6]->val_int();
    if (m_begin_node == m_end_node) {
      my_error(ER_START_AND_END_NODE_CONFLICT, MYF(0), func_name());
      return true;
    }
  }
  else if (m_begin_node != args[5]->val_int() || m_end_node != args[6]->val_int()){
    my_error(ER_START_AND_END_NODE_CONSTANT, MYF(0), func_name());
    return true;
  }
  
  // catch any leftover type errors
  // TODO evaluate necessity
  if (thd->is_error()) return true;
  for (int i = 0; i < 7; i++)
    if (i != 4 && args[i]->null_value)
      return true;

  // store edge
  try {
    Edge edge = Edge{ id, from_id, to_id, cost };
    m_edge_map.insert(std::pair(from_id, edge));
  } catch (...) { // handles std::bad_alloc
    handle_std_exception(func_name());
    return true;
  }
  return false;
}

Item *Item_sum_shortest_dir_path::copy_or_same(THD *) {
  assert(!m_is_window_function);
  return this;
}

bool Item_sum_shortest_dir_path::check_wf_semantics1(THD *thd, Query_block *select, Window_evaluation_requirements *reqs) {
  return Item_sum::check_wf_semantics1(thd, select, reqs);
}

// PRIVATE:

inline bool Item_sum_shortest_dir_path::add_geom(Item *arg, const int& node_id, THD *thd) {
  GeometryExtractionResult geomRes = ExtractGeometry(arg, thd, func_name());

  switch (geomRes.GetResultType()) {
    case ResultType::Error:
      return true;
    case ResultType::NullValue:
      // null geom after non null geom
      if (!m_point_map.empty()){
        my_error(ER_INCONSISTENT_GEOMETRY_NULLNESS, MYF(0), func_name());
        return true;
      }
      return false;
    default:
      // non null geom after null geom
      // * expects add_geom to be called before adding first edge to m_edge_map
      if (m_point_map.empty() && !m_edge_map.empty()) {
        my_error(ER_INCONSISTENT_GEOMETRY_NULLNESS, MYF(0), func_name());
        return true;
      }
  }

  gis::srid_t srid = geomRes.GetSrid();

  if (m_point_map.empty())
    m_srid = srid;
  else if (m_srid != srid){
    my_error(ER_GIS_DIFFERENT_SRIDS_AGGREGATION, MYF(0), func_name(), m_srid, srid);
    return true;
  }

  std::unique_ptr<gis::Geometry> geom = geomRes.GetValue();

  if (geom->type() != gis::Geometry_type::kPoint){
    my_error(ER_GIS_WRONG_GEOM_TYPE, MYF(0), func_name());
    return true;
  }

  // redefinition of already defined geom
  if (m_point_map.find(node_id) != m_point_map.end()){
    const gis::Point* _p = down_cast<const gis::Point*>(m_point_map.at(node_id).get());
    const gis::Point* p  = down_cast<const gis::Point*>(geom.get());
    static constexpr double tol = 0.001;
    if (std::abs(_p->x() - p->x()) > tol || std::abs(_p->y() - p->y()) > tol) {
      my_error(ER_GEOMETRY_REDEFINED, MYF(0), func_name(), node_id);
      return true;
    }
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
