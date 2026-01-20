#include "sql/item_strfunc.h"
#include "sql/mysqld.h"
#include "sql/error_handler.h"
#include "mysqld_error.h"
#include "storage/innobase/include/vec0hnsw_registry.h"

#include <sstream>

// Implementation of Item_func_vector_search

bool Item_func_vector_search::resolve_type(THD *thd) {
  // First arg: query vector
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VECTOR)) return true;
  // Second arg: table name (string)
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
  // Third arg: k (integer)
  if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_LONG)) return true;
  // Optional fourth arg: ef (integer)
  if (arg_count >= 4) {
    if (param_type_is_default(thd, 3, 4, MYSQL_TYPE_LONG)) return true;
  }
  
  set_data_type_string(65535U);  // Return JSON string
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
  
  // Get k
  longlong k = args[2]->val_int();
  if (k <= 0 || k > 10000) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    null_value = true;
    return nullptr;
  }
  
  // Get ef (optional, default to k * 2)
  size_t ef = (arg_count >= 4) ? static_cast<size_t>(args[3]->val_int()) 
                               : static_cast<size_t>(k * 2);
  
  // Lookup index from registry
  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  auto* index = registry.get_index(table_name);
  
  if (!index) {
    // No index found - return empty result with error note
    result_buffer.set_ascii(
      "[{\"error\": \"No HNSW index found for table\"}]", 46);
    null_value = false;
    return &result_buffer;
  }
  
  // Extract query vector data
  const float* query_ptr = reinterpret_cast<const float*>(query_str->ptr());
  size_t query_dims = query_str->length() / sizeof(float);
  std::vector<float> query_vec(query_ptr, query_ptr + query_dims);
  
  // Perform search
  auto results = index->search(query_vec, static_cast<uint32_t>(k), static_cast<uint32_t>(ef));
  
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
