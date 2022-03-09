#include "sql/item_sum.h"

class a_star_ting final : public Item_sum_json {
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
  a_star_ting(THD *thd, Item_sum *item,
                       unique_ptr_destroy_only<Json_wrapper> wrapper,
                       unique_ptr_destroy_only<Json_object> object);
  a_star_ting(const POS &pos, Item *a, Item *b, PT_window *w,
                       unique_ptr_destroy_only<Json_wrapper> wrapper,
                       unique_ptr_destroy_only<Json_object> object);
  ~a_star_ting() override;
  const char *func_name() const override { return "json_objectagg"; }
  void clear() override;
  bool add() override;
  Item *copy_or_same(THD *thd) override;
  bool check_wf_semantics1(THD *thd, Query_block *select,
                           Window_evaluation_requirements *reqs) override;
};