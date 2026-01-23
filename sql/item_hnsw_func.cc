/**
  @file sql/item_hnsw_func.cc

  HNSW Index Management SQL Functions Implementation.
  Supports optional column parameter for multi-index per table.
*/

#include "sql/item_strfunc.h"
#include "sql/mysqld.h"
#include "mysqld_error.h"
#include "storage/innobase/include/vec0hnsw_registry.h"

#include <sstream>

// ============================================================================
// HNSW_CREATE_INDEX Implementation
// 4 args: (table, dim, M, ef)           -- legacy single-index
// 5 args: (table, column, dim, M, ef)   -- multi-index
// ============================================================================

bool Item_func_hnsw_create_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (arg_count == 5) {
    // (table, column, dim, M, ef)
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_LONG)) return true;
    if (param_type_is_default(thd, 3, 4, MYSQL_TYPE_LONG)) return true;
    if (param_type_is_default(thd, 4, 5, MYSQL_TYPE_LONG)) return true;
  } else {
    // (table, dim, M, ef)
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONG)) return true;
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_LONG)) return true;
    if (param_type_is_default(thd, 3, 4, MYSQL_TYPE_LONG)) return true;
  }
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_create_index::val_str(String *str) {
  assert(fixed);

  String table_buf;
  String *table_str = args[0]->val_str(&table_buf);
  if (!table_str) { null_value = true; return nullptr; }

  std::string table_name(table_str->c_ptr_safe());
  std::string column_name;
  longlong dim, M, ef;

  if (arg_count == 5) {
    String col_buf;
    String *col_str = args[1]->val_str(&col_buf);
    if (!col_str) { null_value = true; return nullptr; }
    column_name = std::string(col_str->c_ptr_safe());
    dim = args[2]->val_int();
    M = args[3]->val_int();
    ef = args[4]->val_int();
  } else {
    dim = args[1]->val_int();
    M = args[2]->val_int();
    ef = args[3]->val_int();
  }

  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  bool success = registry.register_index(table_name, column_name,
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
// 1 arg: (table)           -- legacy
// 2 args: (table, column)  -- multi-index
// ============================================================================

bool Item_func_hnsw_drop_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (arg_count >= 2) {
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
  }
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
  std::string column_name;

  if (arg_count >= 2) {
    String col_buf;
    String *col_str = args[1]->val_str(&col_buf);
    if (col_str) column_name = std::string(col_str->c_ptr_safe());
  }

  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  bool success = registry.drop_index(table_name, column_name);

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
// 2 args: (table, path)           -- legacy
// 3 args: (table, column, path)   -- multi-index
// ============================================================================

bool Item_func_hnsw_save_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
  if (arg_count >= 3) {
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_VARCHAR)) return true;
  }
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_save_index::val_str(String *str) {
  assert(fixed);

  String table_buf;
  String *table_str = args[0]->val_str(&table_buf);
  if (!table_str) { null_value = true; return nullptr; }

  std::string table_name(table_str->c_ptr_safe());
  std::string column_name;
  std::string path;

  if (arg_count == 3) {
    String col_buf, path_buf;
    String *col_str = args[1]->val_str(&col_buf);
    String *path_str = args[2]->val_str(&path_buf);
    if (!col_str || !path_str) { null_value = true; return nullptr; }
    column_name = std::string(col_str->c_ptr_safe());
    path = std::string(path_str->c_ptr_safe());
  } else {
    String path_buf;
    String *path_str = args[1]->val_str(&path_buf);
    if (!path_str) { null_value = true; return nullptr; }
    path = std::string(path_str->c_ptr_safe());
  }

  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  auto* index = registry.get_index(table_name, column_name);

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
// 2 args: (table, path)           -- legacy
// 3 args: (table, column, path)   -- multi-index
// ============================================================================

bool Item_func_hnsw_load_index::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_VARCHAR)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_VARCHAR)) return true;
  if (arg_count >= 3) {
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_VARCHAR)) return true;
  }
  set_data_type_string(255U);
  set_nullable(true);
  return false;
}

String *Item_func_hnsw_load_index::val_str(String *str) {
  assert(fixed);

  String table_buf;
  String *table_str = args[0]->val_str(&table_buf);
  if (!table_str) { null_value = true; return nullptr; }

  std::string table_name(table_str->c_ptr_safe());
  std::string column_name;
  std::string path;

  if (arg_count == 3) {
    String col_buf, path_buf;
    String *col_str = args[1]->val_str(&col_buf);
    String *path_str = args[2]->val_str(&path_buf);
    if (!col_str || !path_str) { null_value = true; return nullptr; }
    column_name = std::string(col_str->c_ptr_safe());
    path = std::string(path_str->c_ptr_safe());
  } else {
    String path_buf;
    String *path_str = args[1]->val_str(&path_buf);
    if (!path_str) { null_value = true; return nullptr; }
    path = std::string(path_str->c_ptr_safe());
  }

  auto& registry = innodb_vector::HnswIndexRegistry::instance();
  auto* index = registry.get_index(table_name, column_name);

  if (!index) {
    result_buffer.set_ascii("ERROR: Index not found (create first)", 37);
  } else if (index->load_from_file(path.c_str())) {
    result_buffer.set_ascii("OK: Index loaded", 16);
  } else {
    result_buffer.set_ascii("ERROR: Load failed", 18);
  }

  null_value = false;
  return &result_buffer;
}
