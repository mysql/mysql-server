#ifndef ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED
#define ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED

#include <functional> // std::function

#include "sql/item_sum.h"                 // Item_sum_json
#include "sql/gis/geometries.h"           // gis::Point
#include "sql/gis/geometry_extraction.h"  // ExtractGeometry
#include "sql/sql_exception_handler.h"    // handle_std_exception
#include "sql/gis/distance_functor.h"     // Distance
#include "sql/Dijkstras_functor.h"        // Dijkstra
#include "sql/json_dom.h"                 // Json_dom
#include "include/map_helpers.h"          // Malloc_unordered_map
#include "sql/psi_memory_key.h"           // PSI_memory_key

class Item_sum_shortest_dir_path final : public Item_sum_json {
  int m_begin_node, m_end_node;
  // * accumulated edges from ::add. map key = node id of edge origin (i.e. Edge.from)
  malloc_unordered_multimap<int, const Edge*> m_edge_map;
  // * accumulated points from ::add. map key = node id
  malloc_unordered_map<int, std::unique_ptr<gis::Geometry>> m_point_map;
  // map of all used edge ids (used to check for id overlap)
  malloc_unordered_map<int, bool> m_edge_ids;
  // coord type of points in m_point_map
  gis::srid_t m_srid;
 public:
 /**
  * @brief Construct a new Item_sum_shortest_dir_path object
  * 
  * @param thd 
  * @param item 
  * @param wrapper
  */
  Item_sum_shortest_dir_path(THD *thd, Item_sum *item,
                       unique_ptr_destroy_only<Json_wrapper> wrapper);
  /**
   * @brief Construct a new Item_sum_shortest_dir_path object
   * 
   * @param pos 
   * @param args 
   * @param w
   * @param wrapper
   * ! wrapper not needed
   * TODO inherit from Item_sum?
   */
  Item_sum_shortest_dir_path(const POS &pos, PT_item_list *args,
                       unique_ptr_destroy_only<Json_wrapper> wrapper);

  ~Item_sum_shortest_dir_path() override = default;

  enum Sumfunctype sum_func() const override { return SHORTEST_DIR_PATH_FUNC; }
  const char *func_name() const override { return "st_shortest_dir_path"; }
  
  bool val_json(Json_wrapper *wr) override;
  String *val_str(String *str) override;

  void clear() override;
  bool fix_fields(THD *thd, Item **pItem) override;
  bool add() override;
  Item *copy_or_same(THD *thd) override;
  bool check_wf_semantics1(THD *thd, Query_block *select,
                           Window_evaluation_requirements *reqs) override;

 private:
  /**
   * @brief inserts geometry from arg into m_point_map
   * 
   * @param arg with geom
   * @param node_id id of node associated with arg
   * @param thd 
   * @return true if invalid or not geom
   * @return false if NULL or valid geom
   */
  inline bool add_geom(Item *arg, const int& node_id, THD *thd);
  /**
   * @brief verifies that item is a valid const id
   * 
   * @param item 
   * @return true if valid
   * @return false if invalid
   */
  inline bool verify_const_id_argument(Item *item);
  /**
   * @brief verifies that item is a valid id
   * 
   * @param item 
   * @return true if valid
   * @return false if invalid
   */
  inline bool verify_id_argument(Item *item);
  /**
   * @brief verifies that item is a valid dijkstra weight (cost)
   * 
   * @param item 
   * @return true if valid
   * @return false if invalid
   */
  inline bool verify_cost_argument(Item *item);
  /**
   * @brief allocates Json_int on heap with given value
   * 
   * @param i value
   * @return Json_dom_ptr to Json_int
   */
  inline Json_dom_ptr jsonify_to_heap(const int& i);
  /**
   * @brief allocates Json_double on heap with given value
   * 
   * @param d value
   * @return Json_dom_ptr to Json_double
   */
  inline Json_dom_ptr jsonify_to_heap(const double& d);
};

#endif /* ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED */
