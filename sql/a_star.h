#ifndef ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED
#define ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED

#include "sql/item_sum.h"


struct Edge {
  int id, from, to;
  double cost;
};

class Dijkstra {
  int heu_coeff;
public:
  Dijkstra(const int& heu_coeff = 1): heu_coeff(heu_coeff) {}
  template<typename EdgeID_T>
  std::vector<Edge> operator()(const std::unordered_multimap<int, Edge>& edge_map, int start_id, int end_id){
    return {};
  }
};

class Item_sum_shortest_dir_path final : public Item_sum_json {
  std::unordered_multimap<int, Edge> edge_map;
  /// Accumulates the final value.
  unique_ptr_destroy_only<Json_object> m_json_object;
  /// Buffer used to get the value of the key.
  String m_tmp_key_value;
  /**
     Map of keys in Json_object and the count for each key
     within a window frame. It is used in handling rows
     leaving a window frame when rows are not sorted
     according to the key in Json_object.
   */
  std::map<std::string, int> m_key_map;
  /**
    If window provides ordering on the key in Json_object,
    a key_map is not needed to handle rows leaving a window
    frame. In this case, process_buffered_windowing_record()
    will set flags when a key/value pair can be removed from
    the Json_object.
  */
  bool m_optimize{false};

 public:
  Item_sum_shortest_dir_path(THD *thd, Item_sum *item,
                       unique_ptr_destroy_only<Json_wrapper> wrapper,
                       unique_ptr_destroy_only<Json_object> object);
  Item_sum_shortest_dir_path(const POS &pos, PT_item_list *args, PT_window *w,
                       unique_ptr_destroy_only<Json_wrapper> wrapper,
                       unique_ptr_destroy_only<Json_object> object);

  ~Item_sum_shortest_dir_path() override = default;

  enum Sumfunctype sum_func() const override { return SHORTEST_DIR_PATH_FUNC; }
  const char *func_name() const override { return "st_shortest_dir_path"; }
  
  void clear() override;
  bool add() override;
  Item *copy_or_same(THD *thd) override;
  bool check_wf_semantics1(THD *thd, Query_block *select,
                           Window_evaluation_requirements *reqs) override;

};

#endif /* ITEM_SUM_SHORTEST_DIR_PATH_INCLUDED */
