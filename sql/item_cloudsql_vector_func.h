// Copyright 2026 Google LLC

#ifndef ITEM_CLOUDSQL_VECTOR_FUNC_INCLUDED
#define ITEM_CLOUDSQL_VECTOR_FUNC_INCLUDED

#include <sys/types.h>

#include <cstdint>
#include <vector>

#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"  // Item_result_field
#include "sql/item_func.h"
#include "sql/iterators/cloudsql_vector_iterators.h"
#include "sql/key_spec.h"
#include "sql/parse_location.h"  // POS
#include "sql/set_var.h"         // enum_var_type
#include "sql/sql_const.h"
#include "sql/sql_udf.h"  // udf_handler
#include "sql/table.h"
#include "storage/innobase/include/vector0types.h"

/** Maximum number of neighbors for ANN to prevent memory impact of storing too
 * many result PKs.
*/
#define MAX_NEIGHBORS_COUNT 10000

class Json_wrapper;
class PT_item_list;
class Protocol;
class Query_block;
class THD;
class sp_rcontext;
struct MY_BITMAP;
struct Parse_context;

template <class T>
class List;

bool is_ann_function(Item *item);
bool is_ann_function_on_table(Item *item, Table_ref *table_ref);
bool contains_ann_function(Item *item);
Field *get_embedding_field(Item *item);

class Item_func_cloudsql_vector_distance final : public Item_real_func {
 public:
  Item_func_cloudsql_vector_distance(const POS &pos, PT_item_list *ilist)
      : Item_real_func(pos, ilist) {}

  const char *func_name() const override { return "vector_distance"; }
  bool resolve_type(THD *thd) override {
    return Item_real_func::resolve_type(thd);
  }

  String *val_str(String *) override;
  double val_real() override;
};

class Item_func_cosine_distance final : public Item_real_func {
 public:
  Item_func_cosine_distance(const POS &pos, Item *a, Item *b)
      : Item_real_func(pos, a, b) {}

  const char *func_name() const override { return "cosine_distance"; }
  bool resolve_type(THD *thd) override {
    return Item_real_func::resolve_type(thd);
  }

  String *val_str(String *) override;
  double val_real() override;
};

class Item_func_l2_squared_distance final : public Item_real_func {
 public:
  Item_func_l2_squared_distance(const POS &pos, Item *a, Item *b)
      : Item_real_func(pos, a, b) {}

  const char *func_name() const override { return "l2_squared_distance"; }
  bool resolve_type(THD *thd) override {
    return Item_real_func::resolve_type(thd);
  }

  String *val_str(String *) override;
  double val_real() override;
};

class Item_func_dot_product final : public Item_real_func {
 public:
  Item_func_dot_product(const POS &pos, Item *a, Item *b)
      : Item_real_func(pos, a, b) {}

  const char *func_name() const override { return "dot_product"; }
  bool resolve_type(THD *thd) override {
    return Item_real_func::resolve_type(thd);
  }

  String *val_str(String *) override;
  double val_real() override;
};

class Item_func_approx_distance : public Item_real_func {
 public:
  Item_func_approx_distance(const POS &pos, PT_item_list *item_list)
      : Item_real_func(pos, item_list) {
    m_results = nullptr;
    query_status = nullptr;
  }

  ~Item_func_approx_distance() {
    m_results = nullptr;
    query_status = nullptr;
  }

  const char *func_name() const override { return "approx_distance"; }
  bool resolve_type(THD *thd) override {
    return Item_real_func::resolve_type(thd);
  }

  bool fix_fields(THD *thd, Item **ref) override;

  double val_real() override final;

  float *get_query_vector() { return m_query_vector; }

  uint get_query_vector_size() { return m_query_vector_size; }

  ib_vector::DistMeasure get_distance_measure() { return m_distance_measure; }

  VectorSearchOptions get_search_options() { return m_search_options; }

  VectorSearchResults *get_results() { return m_results; }

  VectorSearchQueryStatus *get_vector_query_status() { return query_status; }

  bool allocate_vector_results(THD *thd, TABLE *tblPtr, uint32_t num_rows) {
    m_results = new (thd->mem_root) VectorSearchResults(thd, tblPtr, num_rows);
    return true;
  }

  void push_ann_warning_and_inc_counter(THD *thd);

  enum Functype functype() const override {
    return CLOUDSQL_APPROX_DISTANCE_FUNC;
  }

  bool ann_failed{false};

 private:
  VectorSearchQueryStatus *query_status;
  float *m_query_vector;
  uint m_query_vector_size;
  ib_vector::DistMeasure m_distance_measure;
  VectorSearchOptions m_search_options{
      .num_leaves_to_search = -1,
      .num_neighbors = 10,
  };
  int m_counter{0};
  VectorSearchResults *m_results;
  bool knn_fallback_warning_pushed{false};
};

#endif  // ITEM_CLOUDSQL_VECTOR_FUNC_INCLUDED
