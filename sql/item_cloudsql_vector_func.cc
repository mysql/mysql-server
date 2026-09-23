// Copyright 2026 Google LLC

#include "sql/item_cloudsql_vector_func.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "include/field_types.h"
#include "include/my_base.h"
#include "include/my_dbug.h"
#include "include/my_inttypes.h"
#include "include/my_sqlcommand.h"
#include "include/my_sys.h"
#include "include/mysql_com.h"
#include "include/mysqld_error.h"
#include "sql/dd/types/column.h"
#include "sql/derror.h"
#include "sql/enum_query_type.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/iterators/cloudsql_vector_iterators.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/options_parser.h"
#include "sql/parse_location.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"
#include "sql/vector_opts.h"
#include "storage/innobase/include/vector0cfg.h"
#include "storage/innobase/include/vector0types.h"

bool is_ann_function_on_table(Item *item, Table_ref *table_ref) {
  if (!(item->type() == Item::FUNC_ITEM)) return false;

  if (!is_function_of_type(item, Item_func::CLOUDSQL_APPROX_DISTANCE_FUNC))
    return false;

  Item_func *item_func = (Item_func *)item;
  Item *search_item = item_func->get_arg(0);
  // Column names and references from within views are allowed.
  if (search_item->type() != Item::FIELD_ITEM &&
      !(search_item->type() == Item::REF_ITEM &&
        ((Item_ref *)search_item)->ref_item()->type() == Item::FIELD_ITEM))
    return false;
  Table_ref *searchItemTableRef = search_item->type() == Item::FIELD_ITEM ?
      ((Item_field *)search_item)->m_table_ref :
      ((Item_ref *)search_item)->ref_item()->type() == Item::FIELD_ITEM ?
      ((Item_field *)((Item_ref *)search_item)->ref_item())->m_table_ref :
      nullptr;
  if (searchItemTableRef != table_ref)
    return false;
  return true;
}
Field *get_embedding_field(Item *search_item) {
  // Column names and references from within views are allowed.
  if (search_item->type() != Item::FIELD_ITEM &&
      !(search_item->type() == Item::REF_ITEM &&
        ((Item_ref *)search_item)->ref_item()->type() == Item::FIELD_ITEM))
    return nullptr;
  Field *searchVectorField = nullptr;
  Item_field *searchVectorFieldItem = nullptr;
  Item_ref *searchVectorFieldRef = nullptr;
  // Fetch the field from the item.
  // 1. If the item is a Item_field, fetch its field object.
  // 2. If the item is a Item_ref, fetch the referenced item's field object.
  if (search_item->type() == Item::FIELD_ITEM) {
    searchVectorFieldItem = (Item_field *)search_item;
    searchVectorField = searchVectorFieldItem->field;
  } else if (search_item->type() == Item::REF_ITEM &&
      ((Item_ref *)search_item)->ref_item()->type() == Item::FIELD_ITEM) {
    searchVectorFieldRef = (Item_ref *)search_item;
    searchVectorField =
        ((Item_field *)(searchVectorFieldRef->ref_item()))->field;
  }
  return searchVectorField;
}

void Item_func_approx_distance::push_ann_warning_and_inc_counter(THD *thd) {
  if (knn_fallback_warning_pushed) return;
  switch (*query_status) {
    case INDEX_NOT_FOUND:
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_ANN_FALLBACK_TO_BRUTE_FORCE,
          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
          "No Vector index with the specified distance measure found");
      csql_vector_knn_fallback_missing_index++;
      break;
    case INDEX_NOT_INLINED:
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ANN_FALLBACK_TO_BRUTE_FORCE,
                          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
                          "Unable to inline Vector index for ANN search");
      csql_vector_knn_fallback_index_inlining_failures++;
      break;
    case INDEX_UNUSABLE:
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_ANN_FALLBACK_TO_BRUTE_FORCE,
          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
          "Vector index is corrupt or invisible to the current transaction.");
      csql_vector_knn_fallback_unusable_index++;
      break;
    case INLINED_INDEX_RESULTS_NOT_POPULATED:
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ANN_FALLBACK_TO_BRUTE_FORCE,
                          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
                          "Unable to read vector index for ANN search");
      csql_vector_knn_fallback_index_inlining_failures++;
      break;
    case LIMIT_NOT_FOUND:
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_ANN_FALLBACK_TO_BRUTE_FORCE,
                          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
                          "Limit is required for ANN search");
      csql_vector_knn_fallback_missing_limit++;
      break;
    case LIMIT_TOO_LARGE:
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_ANN_FALLBACK_TO_BRUTE_FORCE,
          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
          std::format(
              "Limit specified for ANN search larger than the maximum ({})",
              MAX_NEIGHBORS_COUNT)
              .c_str());
      csql_vector_knn_fallback_limit_too_large++;
      break;
    case ANN_MORE_EXPENSIVE_THAN_KNN:
      // Do not warn in this case as it is a cost model optimization to do KNN.
      csql_vector_knn_fallback_knn_less_expensive++;
      break;
    case MULTIPLE_ANN_ONE_TABLE:
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_ANN_FALLBACK_TO_BRUTE_FORCE,
          ER_DEFAULT(ER_ANN_FALLBACK_TO_BRUTE_FORCE),
          "Vector index already searched for a different ANN expression in the same query");
      csql_vector_knn_fallback_multiple_ann_one_table++;
      break;
    default:
      break;
  }
  knn_fallback_warning_pushed = true;
}

double calculate_l2_squared_distance(char *ptr1, char *ptr2, int dims) {
  double dist = 0.0;
  /* squared L2 = (A - B)^ 2 */
  for (int i = 0; i < dims; i++) {
    float f1 = 0.00, f2 = 0.0;
    memcpy(&f1, ptr1, sizeof(float));
    memcpy(&f2, ptr2, sizeof(float));

    float t = f1 - f2;
    dist += (t * t);

    ptr1 += sizeof(float);
    ptr2 += sizeof(float);
  }
  return dist;
}

double calculate_dot_product_distance(char *ptr1, char *ptr2, int dims) {
  double dist = 0.0;
  /* dot product = A * B */
  for (int i = 0; i < dims; i++) {
    float f1 = 0.00, f2 = 0.0;
    memcpy(&f1, ptr1, sizeof(float));
    memcpy(&f2, ptr2, sizeof(float));
    dist += f1 * f2;
    ptr1 += sizeof(float);
    ptr2 += sizeof(float);
  }
  /* We return negative inner product as the distance to make actual
   * select statement order by asc clause behaves the same way for the
   * other distances.
   */
  return dist * -1;
}

double calculate_cosine_distance(char *ptr1, char *ptr2, int dims) {
  double length1 = 0.0;
  double length2 = 0.0;
  double dotvalue = 0.0;
  for (int i = 0; i < dims; i++) {
    float f1 = 0.00, f2 = 0.0;
    memcpy(&f1, ptr1, sizeof(float));
    memcpy(&f2, ptr2, sizeof(float));

    dotvalue += f1 * f2;
    length1 += f1 * f1;
    length2 += f2 * f2;
    ptr1 += sizeof(float);
    ptr2 += sizeof(float);
  }
  /* cosine distance = 1 - ( (A*B) / (||A|| * ||B||) )*/
  return 1 - (dotvalue / (sqrt(length1) * sqrt(length2)));
}

std::pair<bool, double> calculate_vector_distance(
    Item *a, Item *b, ib_vector::DistMeasure distance_measure) {
  double dist = 0.0;
  String vector_str1;
  String *temp1 = a->val_str(&vector_str1);
  String vector_str2;
  String *temp2 = b->val_str(&vector_str2);
  if (temp1 == nullptr || temp2 == nullptr) {
    /* no error*/
    return {true, 0};
  }
  if (temp1->length() == 0 || temp2->length() == 0) {
    my_error(ER_INVALID_VECTOR_INPUT, MYF(0));
    return {true, 0};
  }
  if (temp1->length() != temp2->length()) {
    my_error(ER_VECTOR_DISTANCE_CALCULATION_ERROR, MYF(0),
             "input vector dimensions must be same");
    return {true, 0};
  }
  char *ptr1 = temp1->c_ptr_safe();
  if (temp1->length() % Field_vector::precision != 0 ||
      temp1->length() >
          Field_vector::dimension_bytes(Field_vector::max_dimensions)) {
    my_error(
        ER_VECTOR_DATA_WRONG_LENGTH, MYF(0),
        Field_vector::dimension_bytes(Field_vector::max_dimensions));
    return {true, 0};
  }
  int dims = temp1->length() / Field_vector::precision;
  char *ptr2 = temp2->c_ptr_safe();
  switch (distance_measure) {
    case ib_vector::DistMeasure::L2_SQUARED:
      dist = calculate_l2_squared_distance(ptr1, ptr2, dims);
      break;
    case ib_vector::DistMeasure::COSINE:
      dist = calculate_cosine_distance(ptr1, ptr2, dims);
      break;
    case ib_vector::DistMeasure::DOT_PRODUCT:
      dist = calculate_dot_product_distance(ptr1, ptr2, dims);
      break;
    default:
      /* not possible to reach here due to parse_vector_distance_options */
      break;
  }
  return {false, dist};
}

double Item_func_cloudsql_vector_distance::val_real() {
  DBUG_TRACE;
  null_value = false;
  double dist = 0.0;
  ib_vector::DistMeasure distance_measure;
  if (arg_count == 3) {
    String optionStr;
    String *option_ptr = args[2]->val_str(&optionStr);
    if (option_ptr == nullptr) {
      distance_measure = ib_vector::DistMeasure::L2_SQUARED;
    } else {
      ib_vector::VectorCfg cfg;
      if (parse_vector_distance_options(option_ptr->c_ptr_safe(), cfg,
                                        "vector_distance")) {
        null_value = true;
        return 0;
      }
      distance_measure = cfg[ib_vector::Options::DIST_MEASURE];
    }
  } else {
    distance_measure = ib_vector::DistMeasure::L2_SQUARED;
  }
  auto result = calculate_vector_distance(args[0], args[1], distance_measure);
  if (result.first) {
    null_value = true;
  }
  dist = result.second;
  return dist;
}
double Item_func_cosine_distance::val_real() {
  DBUG_TRACE;
  null_value = false;
  double dist = 0.0;
  auto result = calculate_vector_distance(args[0], args[1],
                                          ib_vector::DistMeasure::COSINE);
  if (result.first) {
    null_value = true;
  }
  dist = result.second;
  return dist;
}
double Item_func_l2_squared_distance::val_real() {
  DBUG_TRACE;
  null_value = false;
  double dist = 0.0;
  auto result = calculate_vector_distance(args[0], args[1],
                                          ib_vector::DistMeasure::L2_SQUARED);
  if (result.first) {
    null_value = true;
  }
  dist = result.second;
  return dist;
}
double Item_func_dot_product::val_real() {
  DBUG_TRACE;
  null_value = false;
  double dist = 0.0;
  auto result = calculate_vector_distance(args[0], args[1],
                                          ib_vector::DistMeasure::DOT_PRODUCT);
  if (result.first) {
    null_value = true;
  }
  dist = result.second;
  return dist;
}

String *Item_func_cloudsql_vector_distance::val_str(String *str) {
  double d = val_real();
  if (null_value) return nullptr;
  char buff[64];
  sprintf(buff, "%20.7lf", d);

  str->copy(buff, strlen(buff), system_charset_info);
  return str;
}
String *Item_func_cosine_distance::val_str(String *str) {
  double d = val_real();
  if (null_value) return nullptr;
  char buff[64];
  sprintf(buff, "%20.7lf", d);

  str->copy(buff, strlen(buff), system_charset_info);
  return str;
}
String *Item_func_dot_product::val_str(String *str) {
  double d = val_real();
  if (null_value) return nullptr;
  char buff[64];
  sprintf(buff, "%20.7lf", d);

  str->copy(buff, strlen(buff), system_charset_info);
  return str;
}
String *Item_func_l2_squared_distance::val_str(String *str) {
  double d = val_real();
  if (null_value) return nullptr;
  char buff[64];
  sprintf(buff, "%20.7lf", d);

  str->copy(buff, strlen(buff), system_charset_info);
  return str;
}

KEY *get_table_vector_index(Table_ref *table_ref) {
  KEY *key = table_ref->table->key_info;
  KEY *key_end = key + table_ref->table->s->keys;
  for (; key < key_end; key++) {
    if (key->flags & HA_VECTOR) return key;
  }
  return nullptr;
}

distance_measure get_vector_index_distance_measure(Table_ref *table_ref)  {
  KEY *key = get_table_vector_index(table_ref);
  if (key == nullptr || key->distance_measure == DISTANCE_MEASURE_UNSPECIFIED) {
    assert(false);
  }
  return key->distance_measure;
}

bool Item_func_approx_distance::fix_fields(THD *thd, Item **ref) {
  if (Item_real_func::fix_fields(thd, ref)) {
    return true;
  }

  query_status = new (thd->mem_root) VectorSearchQueryStatus(INDEX_NOT_INLINED);

  if (!opt_cloudsql_vector) {
    my_error(ER_VECTOR_FEATURE_CANNOT_BE_USED, MYF(0));
    return true;
  }
  // APPROX_DISTANCE cannot be used in the HAVING clause.
  if (thd->lex->current_query_block()->having_fix_field) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE cannot be used in the HAVING clause");
    return true;
  }

  // SERIALIZABLE transaction isolation level is not supported because it can
  // convert SELECTs into SELECT .. FOR SHAREs which require table locking.
  if (thd->tx_isolation == ISO_SERIALIZABLE ||
      (thd->variables.session_track_transaction_info <= TX_TRACK_NONE &&
       global_system_variables.transaction_isolation == ISO_SERIALIZABLE)) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE functions are not supported with SERIALIZABLE "
             "transaction isolation level");
    return true;
  }

  if (thd->lex->sql_command != SQLCOM_SELECT) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "Non SELECT statement Unimplemented");
    return true;
  }

  /* Fail if first argument is not a column.
     Example: select * from tab order by APPROX_*('abc', query); */
  Item *searchVectorItem = (Item *)args[0];
  if (searchVectorItem->type() != Item::FIELD_ITEM &&
      !(searchVectorItem->type() == Item::REF_ITEM &&
      ((Item_ref *)searchVectorItem)->ref_item()->type() == Item::FIELD_ITEM)) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE cannot be used with non-column");
    return true;
  }

  /* Fail if second argument is not a constant or a string_to_vector function or
    a User variable.
    Example: select * from tab order by APPROX_*('abc', select vector ..); */
  Item *queryVectorItem = (Item *)args[1];
  if (!(queryVectorItem->const_item() ||
        (queryVectorItem->type() == Item::FUNC_ITEM &&
         (!strcmp(((Item_func *)args[1])->func_name(), "string_to_vector") ||
          !strcmp(((Item_func *)args[1])->func_name(), "get_user_var") ||
          !strcmp(((Item_func *)args[1])->func_name(), "to_vector"))))) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE cannot be used with non-constants");
    return true;
  }

  /* Get the db name, table name and table share from the column
     Example: select * from tab order by APPROX_*(int-column, query); */
  Field *searchVectorField = get_embedding_field(searchVectorItem);
  if (searchVectorField == nullptr) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE cannot be used with non-column");
    return true;
  }
  if (searchVectorItem->data_type() != MYSQL_TYPE_VECTOR) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE cannot be used with non-vector column");
    return true;
  }

  const char *dbName = nullptr;
  const char *tblName = nullptr;
  TABLE *tblPtr = nullptr;
  TABLE_SHARE *tblShare = nullptr;
  Table_ref *tblRef = searchVectorItem->type() == Item::FIELD_ITEM ?
      ((Item_field *)searchVectorItem)->m_table_ref :
      ((Item_ref *)searchVectorItem)->ref_item()->type() == Item::FIELD_ITEM ?
      ((Item_field *)((Item_ref *)searchVectorItem)->ref_item())->m_table_ref :
      nullptr;
  // Get the db name and table name from the table reference.
  if (tblRef) {
    dbName = tblRef->db;
    tblName = tblRef->table_name;
  }

  // Get the table pointer and table share from the column.
  if (searchVectorField && searchVectorField->table) {
    tblPtr = searchVectorField->table;
    tblShare = searchVectorField->table->s;
  }
  if (!tblPtr || !tblShare || !dbName || !tblName) {
    sql_print_error(
        "Unable to fetch db/table name and/or table share for APPROX_DISTANCE");
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "Unable to fetch db/table name for APPROX_DISTANCE");
    return true;
  }

  /* Fail if table does not have a primary key */
  if (!tblPtr->key_info) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "APPROX_DISTANCE not supported on tables without primary key");
    return true;
  }

  String vector_str;
  String *temp = args[1]->val_str(&vector_str);
  if (!temp) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "Query vector cannot be NULL in APPROX_DISTANCE");
    return true;
  }
  char *vectorptr = temp->ptr();
  auto length = temp->length();

  m_query_vector = nullptr;
  if (length % 4 != 0) {
    std::string err_msg =
        "Wrong query vector length: " + std::to_string(length);
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             err_msg.c_str());
    return true;
  }
  // Allocate a float array for the query vector.
  m_query_vector_size = length / 4;
  m_query_vector = new (thd->mem_root) float[m_query_vector_size];

  // Investigate if this is needed post-GA b/381546059
  std::memcpy(m_query_vector, vectorptr, length);

  String search_options;
  String *search_options_ptr = args[2]->val_str(&search_options);
  if (!search_options_ptr) {
    my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
             "Search options cannot be NULL in APPROX_DISTANCE");
    return true;
  }

  const char *func_name = "APPROX_DISTANCE";
  std::map<std::string, std::string> opts_map;
  if (get_options_map(search_options_ptr->c_ptr_safe(), opts_map, func_name)) {
    return true;
  }

  /* Validate if we only have the supported options as key. */
  if (validate_options(opts_map, {"num_leaves_to_search", "distance_measure"},
                       func_name)) {
    return true;
  }

  if (opts_map.find("distance_measure") != opts_map.end()) {
    auto it = opts_map.find("distance_measure");
    if (it != opts_map.end()) {
      auto [key, val] = *it;
      if (get_distance_measure_from_opts(opts_map, func_name,
                                         m_distance_measure)) {
        my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
                 "Invalid distance_measure option for APPROX_DISTANCE");
        return true;
      }
    }
  } else {
    my_error(
        ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
        "distance_measure is a required parameter for APPROX_DISTANCE search");
    return true;
  }

  if (opts_map.find("num_leaves_to_search") != opts_map.end()) {
    auto it = opts_map.find("num_leaves_to_search");
    if (it != opts_map.end()) {
      auto [key, val] = *it;
      if (parse_integer_opts(key, val, m_search_options.num_leaves_to_search,
                             "APPROX_DISTANCE")) {
        return true;
      }
    }
  } else {
    m_search_options.num_leaves_to_search = -1;
  }

  Query_block *current_query_block = thd->lex->current_query_block();
  bool has_limit =
      current_query_block->master_query_expression()->has_any_limit();

  if (!has_limit) {
    *query_status = LIMIT_NOT_FOUND;
  }

  if (!get_table_vector_index(tblRef) ||
      convert_distance_measure(m_distance_measure) !=
          get_vector_index_distance_measure(tblRef)) {
    *query_status = INDEX_NOT_FOUND;
  }

  if (*query_status != INDEX_NOT_INLINED) {
    Item *replacement_knn_func = nullptr;
    switch (m_distance_measure) {
      case ib_vector::DistMeasure::COSINE:
        replacement_knn_func = new (thd->mem_root)
            Item_func_cosine_distance(POS(), args[0], args[1]);
        break;
      case ib_vector::DistMeasure::L2_SQUARED:
        replacement_knn_func = new (thd->mem_root)
            Item_func_l2_squared_distance(POS(), args[0], args[1]);
        break;
      case ib_vector::DistMeasure::DOT_PRODUCT:
        replacement_knn_func =
            new (thd->mem_root) Item_func_dot_product(POS(), args[0], args[1]);
        break;
      default:
        break;
    }

    // Sanity check, but this should be unreachable.
    if (replacement_knn_func == nullptr) {
      my_error(ER_UNABLE_TO_EXECUTE_ANN, MYF(0),
               "Invalid distance_measure option for APPROX_DISTANCE");
      return true;
    }
    // Parse_context pc(thd, thd->lex->current_query_block());
    // replacement_predicate->itemize(&pc, &replacement_predicate);
    replacement_knn_func->item_name = item_name;
    replacement_knn_func->fix_fields(thd, &replacement_knn_func);
    *ref = replacement_knn_func;
    this->push_ann_warning_and_inc_counter(thd);
    return false;
  }

  m_results = nullptr;
  return false;
}

double Item_func_approx_distance::val_real() {
  DBUG_TRACE;
  null_value = false;
  if (query_status == nullptr || m_results == nullptr ||
      *query_status != RESULTS_POPULATED) {
    if (!knn_fallback_warning_pushed) {
      if (query_status) {
        this->push_ann_warning_and_inc_counter(current_thd);
      }
      knn_fallback_warning_pushed = true;
    }
  } else {
    if (args[0]->is_null()) {
      null_value = true;
      return 0.0;
    }

    // Compare the current row's primary key with the ANN result's primary key.
    const TABLE *table = m_results->table();
    const KEY &key = table->key_info[table->s->primary_key];
    for (uint32_t i = 0; i < m_results->count(); ++i) {
      const uchar *ann_rec = m_results->records() + i * m_results->record_size();
      bool match = true;
      for (auto kp = key.key_part, end = kp + key.user_defined_key_parts; kp < end; ++kp) {
        const Field *f = table->field[kp->fieldnr - 1];
        // Access PK from table->record[0] buffer.
        ptrdiff_t offset = f->field_ptr() - table->record[0];
        if (f->cmp(ann_rec + offset) != 0) {
          match = false;
          break;
        }
      }
      // Return the approx distance matching the row's primary key.
      if (match) {
       return m_results->distances()[i];
      }
    }
  }
  /** Calculate the exact distance for fallback KNN search and additional
   * results from iterative filtering.
   */
  double dist = 0.0;

  auto result = calculate_vector_distance(args[0], args[1], m_distance_measure);
  if (result.first) {
    null_value = true;
  }
  dist = result.second;
  return dist;
}
