/**
  @file sql/item_vector_func.cc

  VECTOR_SEARCH SQL Function Implementation.
  Supports optional column parameter for multi-index per table.

  Signatures:
    VECTOR_SEARCH(query, table, k)                -- legacy (3 args)
    VECTOR_SEARCH(query, table, k, ef)            -- legacy with ef (4 args, arg[2] is int)
    VECTOR_SEARCH(query, table, column, k)        -- multi-index (4 args, arg[2] is string)
    VECTOR_SEARCH(query, table, column, k, ef)    -- multi-index with ef (5 args)
*/

#include "sql/item_strfunc.h"
#include "sql/mysqld.h"
#include "sql/error_handler.h"
#include "mysqld_error.h"
#include "storage/innobase/include/vec0hnsw_registry.h"

#include <sstream>

bool Item_func_vector_search::resolve_type(THD *thd) {
  // arg[0]: query vector
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VECTOR)) return true;
  // arg[1]: table name (string)
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;

  // For args 2+, types depend on whether column is provided.
  // We handle disambiguation at runtime via result_type() check.
  // Set remaining args as LONG by default (works for prepared stmts).
  if (arg_count == 3) {
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_LONG)) return true;
  } else if (arg_count == 4) {
    // Could be (query, table, column, k) or (query, table, k, ef)
    // Don't force type on arg[2] - let runtime disambiguate
    if (param_type_is_default(thd, 3, 4, MYSQL_TYPE_LONG)) return true;
  } else if (arg_count == 5) {
    // (query, table, column, k, ef)
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_VARCHAR)) return true;
    if (param_type_is_default(thd, 3, 4, MYSQL_TYPE_LONG)) return true;
    if (param_type_is_default(thd, 4, 5, MYSQL_TYPE_LONG)) return true;
  }

  set_data_type_string(65535U);
  set_nullable(true);
  return false;
}

String *Item_func_vector_search::val_str(String *str) {
  assert(fixed);

  // Get query vector
  String *query_str = args[0]->val_str(str);
  if (!query_str) {
    null_value = true;
    return nullptr;
  }

  // Get table name
  String table_name_buf;
  String *table_name_str = args[1]->val_str(&table_name_buf);
  if (!table_name_str) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    null_value = true;
    return nullptr;
  }
  std::string table_name(table_name_str->c_ptr_safe());

  // Parse remaining args based on count and types
  std::string column_name;
  longlong k;
  size_t ef;

  if (arg_count == 3) {
    // (query, table, k)
    k = args[2]->val_int();
    ef = static_cast<size_t>(k * 2);
  } else if (arg_count == 4) {
    // Disambiguate: is arg[2] a column name (string) or k (int)?
    if (args[2]->result_type() == STRING_RESULT) {
      // (query, table, column, k)
      String col_buf;
      String *col_str = args[2]->val_str(&col_buf);
      if (col_str) column_name = std::string(col_str->c_ptr_safe());
      k = args[3]->val_int();
      ef = static_cast<size_t>(k * 2);
    } else {
      // (query, table, k, ef) -- legacy
      k = args[2]->val_int();
      ef = static_cast<size_t>(args[3]->val_int());
    }
  } else {
    // 5 args: (query, table, column, k, ef)
    String col_buf;
    String *col_str = args[2]->val_str(&col_buf);
    if (col_str) column_name = std::string(col_str->c_ptr_safe());
    k = args[3]->val_int();
    ef = static_cast<size_t>(args[4]->val_int());
  }

  if (k <= 0 || k > 10000) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    null_value = true;
    return nullptr;
  }

  // Lookup index from registry
  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  auto* index = registry.get_index(table_name, column_name);

  if (!index) {
    result_buffer.set_ascii(
      "[{\"error\": \"No HNSW index found for table/column\"}]", 51);
    null_value = false;
    return &result_buffer;
  }

  // Extract query vector data
  const float* query_ptr = reinterpret_cast<const float*>(query_str->ptr());
  size_t query_dims = query_str->length() / sizeof(float);
  std::vector<float> query_vec(query_ptr, query_ptr + query_dims);

  // Perform search
  auto results = index->search(query_vec, static_cast<uint32_t>(k),
                                static_cast<uint32_t>(ef));

  // Build JSON result
  std::ostringstream json;
  json << "[";
  bool first = true;
  for (const auto& result : results) {
    if (!first) json << ",";
    first = false;
    json << "{\"id\":" << result.id
         << ",\"distance\":" << result.distance << "}";
  }
  json << "]";

  std::string json_str = json.str();
  result_buffer.copy(json_str.c_str(), json_str.length(), &my_charset_utf8mb4_bin);

  null_value = false;
  return &result_buffer;
}
