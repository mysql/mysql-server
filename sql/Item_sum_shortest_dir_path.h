#ifndef ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED
#define ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED

#include "sql/item_sum.h"
#include "sql/Dijkstras_functor.h"

class Item_sum_shortest_dir_path final : public Item_sum_json {
  int m_begin_node, m_end_node;
  // accumulated edges from ::add. map key = node id of edge origin (i.e. Edge.from)
  std::unordered_multimap<int, Edge*> m_edge_map;
 public:
 /**
  * @brief Construct a new Item_sum_shortest_dir_path object
  * 
  * @param thd 
  * @param item 
  * @param wrapper must contain Json_object
  */
  Item_sum_shortest_dir_path(THD *thd, Item_sum *item,
                       unique_ptr_destroy_only<Json_wrapper> wrapper);
  /**
   * @brief Construct a new Item_sum_shortest_dir_path object
   * 
   * @param pos 
   * @param args 
   * @param w 
   * @param wrapper must contain Json_object
   */
  Item_sum_shortest_dir_path(const POS &pos, PT_item_list *args, PT_window *w,
                       unique_ptr_destroy_only<Json_wrapper> wrapper);

  ~Item_sum_shortest_dir_path() override = default;

  enum Sumfunctype sum_func() const override { return SHORTEST_DIR_PATH_FUNC; }
  const char *func_name() const override { return "st_shortest_dir_path"; }
  
  bool val_json(Json_wrapper *wr) override;
  String *val_str(String *str) override;

  void clear() override;
  bool add() override;
  Item *copy_or_same(THD *thd) override;
  bool check_wf_semantics1(THD *thd, Query_block *select,
                           Window_evaluation_requirements *reqs) override;

 private:
  inline bool verify_const_id_argument(int i);
  inline bool verify_id_argument(int i);
  inline bool verify_cost_argument(int i);
  /**
   * @brief allocates Json_int on heap with given value
   * 
   * @param i value
   * @return Json_dom* ptr to Json_int
   */
  inline Json_dom *jsonify_to_heap(int i);
  /**
   * @brief allocates Json_double on heap with given value
   * 
   * @param d value
   * @return Json_dom* ptr to Json_double
   */
  inline Json_dom *jsonify_to_heap(double d);
};

#endif /* ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED */
