/**
  @file sql/item_hnsw_func.cc

  HNSW Index Management SQL Functions Implementation.
*/

#include "sql/item_strfunc.h"
#include "sql/mysqld.h"
#include "mysqld_error.h"
#include "storage/innobase/include/vec0hnsw_registry.h"

#include <sstream>

// ============================================================================
// HNSW_CREATE_INDEX Implementation
// ============================================================================

bool Item_func_hnsw_create_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONG)) return true;
  if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_LONG)) return true;
  if (param_type_is_default(thd, 3, 4, MYSQL_TYPE_LONG)) return true;
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_create_index::val_str(String *str) {
  assert(fixed);
  
  String table_buf;
  String *table_str = args[0]->val_str(&table_buf);
  if (!table_str) { null_value = true; return nullptr; }
  
  longlong dim = args[1]->val_int();
  longlong M = args[2]->val_int();
  longlong ef = args[3]->val_int();
  
  std::string table_name(table_str->c_ptr_safe());
  
  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  bool success = registry.register_index(table_name, 
                                          static_cast<size_t>(dim),
                                          static_cast<size_t>(M),
                                          static_cast<size_t>(ef));
  
  if (success) {
    result_buffer.set_ascii("OK: Index created", 17);
  } else {
    result_buffer.set_ascii("ERROR: Index already exists", 27);
  }
  
  null_value = false;
  return &result_buffer;
}

// ============================================================================
// HNSW_DROP_INDEX Implementation
// ============================================================================

bool Item_func_hnsw_drop_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_drop_index::val_str(String *str) {
  assert(fixed);
  
  String table_buf;
  String *table_str = args[0]->val_str(&table_buf);
  if (!table_str) { null_value = true; return nullptr; }
  
  std::string table_name(table_str->c_ptr_safe());
  
  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  bool success = registry.drop_index(table_name);
  
  if (success) {
    result_buffer.set_ascii("OK: Index dropped", 17);
  } else {
    result_buffer.set_ascii("ERROR: Index not found", 22);
  }
  
  null_value = false;
  return &result_buffer;
}

// ============================================================================
// HNSW_SAVE_INDEX Implementation
// ============================================================================

bool Item_func_hnsw_save_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_save_index::val_str(String *str) {
  assert(fixed);
  
  String table_buf, path_buf;
  String *table_str = args[0]->val_str(&table_buf);
  String *path_str = args[1]->val_str(&path_buf);
  if (!table_str || !path_str) { null_value = true; return nullptr; }
  
  std::string table_name(table_str->c_ptr_safe());
  std::string path(path_str->c_ptr_safe());
  
  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  auto* index = registry.get_index(table_name);
  
  if (!index) {
    result_buffer.set_ascii("ERROR: Index not found", 22);
  } else if (index->save_to_file(path.c_str())) {
    result_buffer.set_ascii("OK: Index saved", 15);
  } else {
    result_buffer.set_ascii("ERROR: Save failed", 18);
  }
  
  null_value = false;
  return &result_buffer;
}

// ============================================================================
// HNSW_LOAD_INDEX Implementation
// ============================================================================

bool Item_func_hnsw_load_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_load_index::val_str(String *str) {
  assert(fixed);
  
  String table_buf, path_buf;
  String *table_str = args[0]->val_str(&table_buf);
  String *path_str = args[1]->val_str(&path_buf);
  if (!table_str || !path_str) { null_value = true; return nullptr; }
  
  std::string table_name(table_str->c_ptr_safe());
  std::string path(path_str->c_ptr_safe());
  
  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  auto* index = registry.get_index(table_name);
  
  if (!index) {
    result_buffer.set_ascii("ERROR: Index not found (create first)", 38);
  } else if (index->load_from_file(path.c_str())) {
    result_buffer.set_ascii("OK: Index loaded", 16);
  } else {
    result_buffer.set_ascii("ERROR: Load failed", 18);
  }
  
  null_value = false;
  return &result_buffer;
}
