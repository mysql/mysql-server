/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/table_access_service.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/udf_registration.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <thread>

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader, current_thd_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_udf_metadata, udf_metadata_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_charset, charset_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_charset_converter,
                                string_converter_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_factory_v1, ta_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_v1, ta_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_index_v1, ta_index_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_scan_v1, ta_scan_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(table_access_update_v1, ta_update_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(field_access_nullability_v1, fa_null_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(field_integer_access_v1, fa_integer_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(field_varchar_access_v1, fa_varchar_srv);

/*
  commit_action:
  0 = none
  1 = commit
  2 = rollback
*/
const char *common_insert_customer(char * /* out */, size_t num_tables,
                                   TA_lock_type lock_type, size_t ticket_fuzz,
                                   int commit_action) {
  const int ID_COL = 0;
  const int NAME_COL = 1;
  const int ADDRESS_COL = 2;
  static const TA_table_field_def columns[] = {
      {ID_COL, "ID", 2, TA_TYPE_INTEGER, false, 0},
      {NAME_COL, "NAME", 4, TA_TYPE_VARCHAR, false, 50},
      {ADDRESS_COL, "ADDRESS", 7, TA_TYPE_VARCHAR, true, 255}};

  const char *result;
  Table_access access = nullptr;
  TA_table table;
  long long id_value;
  my_h_string name_value = nullptr;
  size_t ticket;
  int rc;

  CHARSET_INFO_h utf8mb4_h = charset_srv->get_utf8mb4();
  MYSQL_THD thd;
  current_thd_srv->get(&thd);

  string_factory_srv->create(&name_value);

  access = ta_factory_srv->create(thd, num_tables);
  if (access == nullptr) {
    result = "create() failed";
    goto cleanup;
  }

  ticket = ta_srv->add(access, "shop", 4, "customer", 8, lock_type);

  rc = ta_srv->begin(access);
  if (rc != 0) {
    result = "begin() failed";
    goto cleanup;
  }

  table = ta_srv->get(access, ticket + ticket_fuzz);
  if (table == nullptr) {
    result = "get() failed";
    goto cleanup;
  }

  rc = ta_srv->check(access, table, columns, 3);
  if (rc != 0) {
    result = "check() failed";
    goto cleanup;
  }

  id_value = 1;

  if (fa_integer_srv->set(access, table, ID_COL, id_value)) {
    result = "set(id) failed";
    goto cleanup;
  }

  string_converter_srv->convert_from_buffer(name_value, "John Doe", 8,
                                            utf8mb4_h);

  if (fa_varchar_srv->set(access, table, NAME_COL, name_value)) {
    result = "set(name) failed";
    goto cleanup;
  }

  fa_null_srv->set(access, table, ADDRESS_COL);

  rc = ta_update_srv->insert(access, table);
  if (rc != 0) {
    result = "insert() failed";
    goto cleanup;
  }

  if (commit_action == 1) {
    if (ta_srv->commit(access)) {
      result = "commit() failed";
      goto cleanup;
    }
  } else if (commit_action == 2) {
    if (ta_srv->rollback(access)) {
      result = "rollback() failed";
      goto cleanup;
    }
  } else {
    result = "OK, but forgot to commit";
    goto cleanup;
  }

  result = "OK";

cleanup:
  if (name_value != nullptr) {
    string_factory_srv->destroy(name_value);
  }

  if (access != nullptr) {
    ta_factory_srv->destroy(access);
  }

  return result;
}

const char *test_insert_customer(char *out) {
  // Nominal path
  return common_insert_customer(out, 1, TA_WRITE, 0, 1);
}

const char *test_insert_customer_1(char *out) {
  return common_insert_customer(out, 0, TA_READ, 0, 1);
}

const char *test_insert_customer_2(char *out) {
  return common_insert_customer(out, 5, TA_READ, 99, 1);
}

const char *test_insert_customer_3(char *out) {
  return common_insert_customer(out, 1, TA_WRITE, 99, 1);
}

const char *test_insert_customer_4(char *out) {
  return common_insert_customer(out, 1, TA_WRITE, 0, 0);
}

const char *test_insert_customer_5(char *out) {
  return common_insert_customer(out, 1, TA_WRITE, 0, 2);
}

const char *common_fetch_order(char *out, int order_num) {
  /* TABLE shop.order metadata */

  static const size_t ORDER_ORDER_ID = 1;
  static const size_t ORDER_ORDER_COMMENT = 2;
  static const TA_table_field_def columns_order[] = {
      /* Ignoring CUSTOMER_ID. */
      {ORDER_ORDER_ID, "ORDER_ID", 8, TA_TYPE_INTEGER, false, 0},
      {ORDER_ORDER_COMMENT, "ORDER_COMMENT", 13, TA_TYPE_VARCHAR, true, 50}
      /* Ignoring DATE_CREATED. */
  };

  static const char *pk_order_name = "PRIMARY";
  static size_t pk_order_name_length = 7;
  static const TA_index_field_def pk_order_cols[] = {{"ORDER_ID", 8, false}};
  static const size_t pk_order_numcol = 1;

  /* TABLE shop.order_line metadata */

  static const size_t ORDER_LINE_ORDER_ID = 0;
  static const size_t ORDER_LINE_LINE_NUM = 1;
  static const size_t ORDER_LINE_QTY = 4;
  static const TA_table_field_def columns_order_line[] = {
      {ORDER_LINE_ORDER_ID, "ORDER_ID", 8, TA_TYPE_INTEGER, false, 0},
      {ORDER_LINE_LINE_NUM, "LINE_NUM", 8, TA_TYPE_INTEGER, false, 0},
      /* Ignoring ITEM_ID. */
      /* Ignoring UNIT_PRICE. */
      {ORDER_LINE_QTY, "QTY", 3, TA_TYPE_INTEGER, false, 0}};

  static const char *pk_order_line_name = "PRIMARY";
  static size_t pk_order_line_name_length = 7;
  static const TA_index_field_def pk_order_line_cols[] = {
      {"ORDER_ID", 8, false},
      {"LINE_NUM", 8, false},
  };
  static const size_t pk_order_line_numcol = 2;

  const char *result;
  Table_access access = nullptr;
  TA_table table_order;
  bool order_comment_null;
  my_h_string order_comment_value = nullptr;
  TA_table table_order_line;
  long long order_line_qty_value;
  TA_key order_pk = nullptr;
  TA_key order_line_pk = nullptr;
  long long total_qty;
  char buff_order_comment[50 + 1];
  size_t ticket_order;
  size_t ticket_order_line;
  int rc;

  CHARSET_INFO_h utf8mb4_h = charset_srv->get_utf8mb4();
  MYSQL_THD thd;
  current_thd_srv->get(&thd);

  string_factory_srv->create(&order_comment_value);

  access = ta_factory_srv->create(thd, 2);
  if (access == nullptr) {
    result = "create() failed";
    goto cleanup;
  }

  ticket_order = ta_srv->add(access, "shop", 4, "order", 5, TA_READ);
  ticket_order_line = ta_srv->add(access, "shop", 4, "order_line", 10, TA_READ);

  rc = ta_srv->begin(access);
  if (rc != 0) {
    result = "begin() failed";
    goto cleanup;
  }

  table_order = ta_srv->get(access, ticket_order);
  if (table_order == nullptr) {
    result = "get(order) failed";
    goto cleanup;
  }

  rc = ta_srv->check(access, table_order, columns_order, 2);
  if (rc != 0) {
    result = "check(order) failed";
    goto cleanup;
  }

  table_order_line = ta_srv->get(access, ticket_order_line);
  if (table_order_line == nullptr) {
    result = "get(order_line) failed";
    goto cleanup;
  }

  rc = ta_srv->check(access, table_order_line, columns_order_line, 3);
  if (rc != 0) {
    result = "check(order_line) failed";
    goto cleanup;
  }

  if (ta_index_srv->init(access, table_order, pk_order_name,
                         pk_order_name_length, pk_order_cols, pk_order_numcol,
                         &order_pk)) {
    result = "init(order::pk) failed";
    goto cleanup;
  }

  if (fa_integer_srv->set(access, table_order, ORDER_ORDER_ID, order_num)) {
    result = "set(order::id) failed";
    goto cleanup_index;
  }

  rc = ta_index_srv->read_map(access, table_order, 1, order_pk);

  if (rc != 0) {
    result = "No such order";
    goto cleanup_index;
  }

  order_comment_null =
      fa_null_srv->get(access, table_order, ORDER_ORDER_COMMENT);

  if (order_comment_null) {
    buff_order_comment[0] = '\0';
  } else {
    if (fa_varchar_srv->get(access, table_order, ORDER_ORDER_COMMENT,
                            order_comment_value)) {
      result = "get(order::comment) failed";
      goto cleanup_index;
    }

    string_converter_srv->convert_to_buffer(
        order_comment_value, buff_order_comment, sizeof(buff_order_comment),
        utf8mb4_h);
  }

  if (ta_index_srv->end(access, table_order, order_pk)) {
    result = "end(order::pk) failed";
    goto cleanup;
  }
  order_pk = nullptr;

  // Now looking at order_line table, to count line items.

  if (ta_index_srv->init(access, table_order_line, pk_order_line_name,
                         pk_order_line_name_length, pk_order_line_cols,
                         pk_order_line_numcol, &order_line_pk)) {
    result = "init(order_line::pk) failed";
    goto cleanup;
  }

  if (fa_integer_srv->set(access, table_order_line, ORDER_LINE_ORDER_ID,
                          order_num)) {
    result = "set(order_line::id) failed";
    goto cleanup;
  }

  total_qty = 0;

  rc = ta_index_srv->read_map(access, table_order_line, 1, order_line_pk);

  if (rc != 0) {
    sprintf(out, "found: (%s), no order line", buff_order_comment);
    result = out;
    goto cleanup_index;
  }

  do {
    if (fa_integer_srv->get(access, table_order_line, ORDER_LINE_QTY,
                            &order_line_qty_value)) {
      result = "get(order_line::qty) failed";
      goto cleanup_index;
    }

    total_qty += order_line_qty_value;

    rc = ta_index_srv->next_same(access, table_order_line, order_line_pk);
  } while (rc == 0);

  if (ta_index_srv->end(access, table_order_line, order_line_pk)) {
    result = "end(order::pk) failed";
    goto cleanup;
  }
  order_line_pk = nullptr;

  sprintf(out, "found: (%s), total qty: %lld", buff_order_comment, total_qty);

  result = out;

cleanup_index:
  if (order_line_pk != nullptr) {
    ta_index_srv->end(access, table_order_line, order_line_pk);
  }

  if (order_pk != nullptr) {
    ta_index_srv->end(access, table_order, order_pk);
  }

cleanup:
  if (order_comment_value != nullptr) {
    string_factory_srv->destroy(order_comment_value);
  }

  if (access != nullptr) {
    ta_factory_srv->destroy(access);
  }
  return result;
}

const char *test_fetch_order(char *out) {
  return common_fetch_order(out, 1001);
}

const char *common_index(char *out, bool scan, int min_capacity,
                         int building_id, int floor_num, int alley_num,
                         int shelve_num) {
  /* TABLE shop.warehouse metadata */

  static const size_t BUILDING_ID = 0;
  static const size_t FLOOR_NUMBER = 1;
  static const size_t ALLEY_NUMBER = 2;
  static const size_t SHELVE_NUMBER = 3;
  static const size_t CAPACITY = 4;
  static const TA_table_field_def columns_warehouse[] = {
      {BUILDING_ID, "BUILDING_ID", 11, TA_TYPE_INTEGER, false, 0},
      {FLOOR_NUMBER, "FLOOR_NUMBER", 12, TA_TYPE_INTEGER, false, 0},
      {ALLEY_NUMBER, "ALLEY_NUMBER", 12, TA_TYPE_INTEGER, false, 0},
      {SHELVE_NUMBER, "SHELVE_NUMBER", 13, TA_TYPE_INTEGER, false, 0},
      {CAPACITY, "CAPACITY", 8, TA_TYPE_INTEGER, false, 0},
  };
  static const size_t num_columns_warehouse = 5;

  static const char *key_shelves_name = "SHELVES";
  static size_t key_shelves_name_length = 7;
  static const TA_index_field_def key_shelves_cols[] = {
      {"BUILDING_ID", 11, true},
      {"FLOOR_NUMBER", 12, true},
      {"ALLEY_NUMBER", 12, true},
      {"SHELVE_NUMBER", 13, true}};
  static const size_t key_shelves_numcol = 4;

  const char *result = nullptr;
  Table_access access;
  size_t ticket_warehouse;
  int rc;
  TA_table table_warehouse;
  long long building_id_value;
  long long floor_num_value;
  long long alley_num_value;
  long long shelve_num_value;
  long long capacity_value;

  TA_key shelves_key = nullptr;
  bool found = false;
  char where[80];

  MYSQL_THD thd;
  current_thd_srv->get(&thd);

  access = ta_factory_srv->create(thd, 1);
  if (access == nullptr) {
    return "create() failed";
  }

  ticket_warehouse = ta_srv->add(access, "shop", 4, "warehouse", 9, TA_READ);

  rc = ta_srv->begin(access);
  if (rc != 0) {
    result = "begin() failed";
    goto cleanup;
  }

  table_warehouse = ta_srv->get(access, ticket_warehouse);
  if (table_warehouse == nullptr) {
    result = "get(warehouse) failed";
    goto cleanup;
  }

  rc = ta_srv->check(access, table_warehouse, columns_warehouse,
                     num_columns_warehouse);
  if (rc != 0) {
    result = "check(warehouse) failed";
    goto cleanup;
  }

  if (ta_index_srv->init(access, table_warehouse, key_shelves_name,
                         key_shelves_name_length, key_shelves_cols,
                         key_shelves_numcol, &shelves_key)) {
    result = "init(shelves) failed";
    goto cleanup;
  }

  found = false;
  sprintf(where, "anywhere");

  if (scan) {
    rc = ta_index_srv->first(access, table_warehouse, shelves_key);
  } else {
    int num_key_parts = 0;

    if (building_id > 0) {
      fa_integer_srv->set(access, table_warehouse, BUILDING_ID, building_id);
      num_key_parts++;
      sprintf(where, "B:%d", building_id);
    }

    if (floor_num > 0) {
      fa_integer_srv->set(access, table_warehouse, FLOOR_NUMBER, floor_num);
      num_key_parts++;
      sprintf(where, "B:%d F:%d", building_id, floor_num);
    }

    if (alley_num > 0) {
      fa_integer_srv->set(access, table_warehouse, ALLEY_NUMBER, alley_num);
      num_key_parts++;
      sprintf(where, "B:%d F:%d A:%d", building_id, floor_num, alley_num);
    }

    if (shelve_num > 0) {
      fa_integer_srv->set(access, table_warehouse, SHELVE_NUMBER, shelve_num);
      num_key_parts++;
      sprintf(where, "B:%d F:%d A:%d S:%d", building_id, floor_num, alley_num,
              shelve_num);
    }

    rc = ta_index_srv->read_map(access, table_warehouse, num_key_parts,
                                shelves_key);
  }

  while (rc == 0) {
    fa_integer_srv->get(access, table_warehouse, CAPACITY, &capacity_value);
    if (capacity_value >= min_capacity) {
      // Found.

      fa_integer_srv->get(access, table_warehouse, BUILDING_ID,
                          &building_id_value);
      fa_integer_srv->get(access, table_warehouse, FLOOR_NUMBER,
                          &floor_num_value);
      fa_integer_srv->get(access, table_warehouse, ALLEY_NUMBER,
                          &alley_num_value);
      fa_integer_srv->get(access, table_warehouse, SHELVE_NUMBER,
                          &shelve_num_value);

      sprintf(
          out,
          "Found capacity (%lld) for min (%d) at B:%lld F:%lld A:%lld S:%lld",
          capacity_value, min_capacity, building_id_value, floor_num_value,
          alley_num_value, shelve_num_value);
      result = out;
      found = true;
      break;
    }
    if (scan) {
      rc = ta_index_srv->next(access, table_warehouse, shelves_key);
    } else {
      rc = ta_index_srv->next_same(access, table_warehouse, shelves_key);
    }
  }

  if (!found) {
    sprintf(out, "No shelve with min capacity (%d) in %s", min_capacity, where);
    result = out;
  }

  if (shelves_key != nullptr) {
    ta_index_srv->end(access, table_warehouse, shelves_key);
  }

cleanup:
  ta_factory_srv->destroy(access);
  return result;
}

const char *test_index_scan(char *out) {
  return common_index(out, true, 100, 0, 0, 0, 0);
}

const char *test_index_fetch_b(char *out) {
  return common_index(out, false, 100, 1005, 0, 0, 0);
}

const char *test_index_fetch_bf(char *out) {
  return common_index(out, false, 100, 1005, 5, 0, 0);
}

const char *test_index_fetch_bfa(char *out) {
  return common_index(out, false, 100, 1005, 5, 5, 0);
}

const char *test_index_fetch_bfas(char *out) {
  return common_index(out, false, 100, 1005, 5, 5, 5);
}

const char *test_math_insert(char * /* out */, bool is_utf8mb4) {
  static const char *schema_name =
      "\xE2" /* for each */
      "\x88"
      "\x80"
      "p"    /* p */
      "\xE2" /* element */
      "\x88"
      "\x8A"
      "\xE2" /* P */
      "\x84"
      "\x99";
  static const size_t schema_name_length = 10;

  static const char *table_name_utf8mb3 =
      "\xE2" /* there exists */
      "\x88"
      "\x83"
      "s"
      "\xE2" /* element */
      "\x88"
      "\x8A"
      "\xE2" /* Q */
      "\x84"
      "\x9A";
  static const size_t table_name_utf8mb3_length = 10;

  static const char *table_name_utf8mb4 =
      "\xE2" /* there exists */
      "\x88"
      "\x83"
      "s"
      "\xE2" /* element */
      "\x88"
      "\x8A"
      "\xF0" /* S */
      "\x9D"
      "\x95"
      "\x8A";
  static const size_t table_name_utf8mb4_length = 11;

  static const char *column_name =
      "s(p)"
      "\xE2" /* equivalent */
      "\x89"
      "\x8E"
      "\xE2" /* truth */
      "\x8A"
      "\xA4";
  static const size_t column_name_length = 10;

  static const TA_table_field_def columns[] = {
      {0, column_name, column_name_length, TA_TYPE_VARCHAR, true, 255}};

  const char *result;
  Table_access access = nullptr;
  TA_table table;
  my_h_string row_value = nullptr;
  char value_buffer[255];
  char *ptr;
  size_t value_length;
  size_t ticket;
  int rc;

  const char *table_name;
  size_t table_name_length;

  CHARSET_INFO_h utf8mb4_h = charset_srv->get_utf8mb4();
  MYSQL_THD thd;
  current_thd_srv->get(&thd);

  string_factory_srv->create(&row_value);

  access = ta_factory_srv->create(thd, 1);
  if (access == nullptr) {
    result = "create() failed";
    goto cleanup;
  }

  if (is_utf8mb4) {
    table_name = table_name_utf8mb4;
    table_name_length = table_name_utf8mb4_length;
  } else {
    table_name = table_name_utf8mb3;
    table_name_length = table_name_utf8mb3_length;
  }

  ticket = ta_srv->add(access, schema_name, schema_name_length, table_name,
                       table_name_length, TA_WRITE);

  rc = ta_srv->begin(access);
  if (rc != 0) {
    result = "begin() failed";
    goto cleanup;
  }

  table = ta_srv->get(access, ticket);
  if (table == nullptr) {
    result = "get() failed";
    goto cleanup;
  }

  rc = ta_srv->check(access, table, columns, 1);
  if (rc != 0) {
    result = "check() failed";
    goto cleanup;
  }

  ptr = &value_buffer[0];
  memcpy(ptr, schema_name, schema_name_length);
  ptr += schema_name_length;
  memcpy(ptr, " ", 1);
  ptr += 1;
  memcpy(ptr, table_name, table_name_length);
  ptr += table_name_length;
  memcpy(ptr, " ", 1);
  ptr += 1;
  memcpy(ptr, column_name, column_name_length);
  ptr += column_name_length;

  value_length = ptr - &value_buffer[0];

  memcpy(ptr, "TRAILING GARBAGE\0", 17);
  ptr += column_name_length;

  string_converter_srv->convert_from_buffer(row_value, value_buffer,
                                            value_length, utf8mb4_h);

  if (fa_varchar_srv->set(access, table, 0, row_value)) {
    result = "set() failed";
    goto cleanup;
  }

  rc = ta_update_srv->insert(access, table);
  if (rc != 0) {
    result = "insert() failed";
    goto cleanup;
  }

  if (ta_srv->commit(access)) {
    result = "commit() failed";
    goto cleanup;
  }

  result = "OK";

cleanup:
  if (row_value != nullptr) {
    string_factory_srv->destroy(row_value);
  }

  if (access != nullptr) {
    ta_factory_srv->destroy(access);
  }

  return result;
}

const char *test_math_insert_utf8mb3(char *out) {
  return test_math_insert(out, false);
}

const char *test_math_insert_utf8mb4(char *out) {
  return test_math_insert(out, true);
}

typedef const char *(*test_driver_fn)(char *output_message_buffer);

struct test_driver_t {
  const char *m_name;
  test_driver_fn m_driver;
};

static test_driver_t driver[] = {
    {"INSERT-CUSTOMER", test_insert_customer},
    {"INSERT-CUSTOMER-STRESS-1", test_insert_customer_1},
    {"INSERT-CUSTOMER-STRESS-2", test_insert_customer_2},
    {"INSERT-CUSTOMER-STRESS-3", test_insert_customer_3},
    {"INSERT-CUSTOMER-NO-COMMIT", test_insert_customer_4},
    {"INSERT-CUSTOMER-ROLLBACK", test_insert_customer_5},
    {"FETCH-ORDER", test_fetch_order},
    {"INDEX-SCAN", test_index_scan},
    {"INDEX-FETCH-B", test_index_fetch_b},
    {"INDEX-FETCH-BF", test_index_fetch_bf},
    {"INDEX-FETCH-BFA", test_index_fetch_bfa},
    {"INDEX-FETCH-BFAS", test_index_fetch_bfas},
    {"MATH-INSERT-UTF8MB3", test_math_insert_utf8mb3},
    {"MATH-INSERT-UTF8MB4", test_math_insert_utf8mb4},
    {nullptr, nullptr},
};

static const char *const udf_name = "test_table_access_driver";
static const size_t udf_result_size = 80;

static bool udf_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  initid->maybe_null = true;
  initid->max_length = udf_result_size;

  if (args->arg_count != 1) {
    sprintf(message, "%s() requires 1 argument", udf_name);
    return true;
  }

  args->arg_type[0] = STRING_RESULT;

  const char *attr_name = "charset";
  const char *attr_value = "utf8mb4";
  char *attr_value_2 = const_cast<char *>(attr_value);
  if (udf_metadata_srv->result_set(initid, attr_name, attr_value_2)) {
    return true;
  }

  return false;
}

static void udf_deinit(UDF_INIT *) {}

static char *test_table_access_driver(UDF_INIT *, UDF_ARGS *args, char *result,
                                      unsigned long *length,
                                      unsigned char *is_null,
                                      unsigned char *error) {
  const char *p1 = args->args[0];
  size_t len_p1 = args->lengths[0];

  test_driver_t *entry;
  char output_message[255];

  for (entry = &driver[0]; entry->m_name != nullptr; entry++) {
    if (strlen(entry->m_name) == len_p1) {
      if (strncmp(entry->m_name, p1, len_p1) == 0) {
        const char *fn_result = (*entry->m_driver)(output_message);
        if (fn_result != nullptr) {
          size_t len = strlen(fn_result);
          len = std::min(len, udf_result_size);
          memcpy(result, fn_result, len);
          *length = len;
          *is_null = 0;
          *error = 0;
        } else {
          *is_null = 1;
          *error = 0;
        }
        return result;
      }
    }
  }

  *error = 1;
  return nullptr;
}

#define CONST_STR_AND_LEN(x) x, sizeof(x) - 1

/**
 @param [out] status: true for failure, false otherwise
 */
static void thd_function(bool *ret) {
  TA_table tb = nullptr;
  Table_access ta = nullptr;
  size_t ticket = 0;
  bool txn_started = false;
  *ret = true;

  ta = ta_factory_srv->create(nullptr, 1);
  if (!ta) goto cleanup;
  ticket = ta_srv->add(ta, CONST_STR_AND_LEN("mysql"), CONST_STR_AND_LEN("db"),
                       TA_READ);
  if (ta_srv->begin(ta)) goto cleanup;
  txn_started = true;
  tb = ta_srv->get(ta, ticket);
  if (!tb) goto cleanup;

  *ret = false;
cleanup:
  if (txn_started) ta_srv->rollback(ta);
  if (ta) ta_factory_srv->destroy(ta);
}

static bool test_native_thread() {
  bool retval = true;
  std::thread thd(thd_function, &retval);
  thd.join();
  return retval;
}

mysql_service_status_t test_table_access_init() {
  if (udf_srv->udf_register(udf_name, Item_result::STRING_RESULT,
                            (Udf_func_any)test_table_access_driver, udf_init,
                            udf_deinit)) {
    return 1;
  }

  /*
    Make sure the table access service can be used from
    a component init function as well.
    Ignore errors when the table is not present.
  */
  (void)test_math_insert_utf8mb3(nullptr);
  (void)test_math_insert_utf8mb4(nullptr);
  if (test_native_thread()) return 1;

  return 0;
}

mysql_service_status_t test_table_access_deinit() {
  int was_present = 0;

  if (udf_srv->udf_unregister(udf_name, &was_present)) {
    return 1;
  }

  return 0;
}

BEGIN_COMPONENT_PROVIDES(test_table_access)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_table_access)
REQUIRES_SERVICE_AS(mysql_current_thread_reader, current_thd_srv),
    REQUIRES_SERVICE_AS(udf_registration, udf_srv),
    REQUIRES_SERVICE_AS(mysql_udf_metadata, udf_metadata_srv),
    REQUIRES_SERVICE_AS(mysql_charset, charset_srv),
    REQUIRES_SERVICE_AS(mysql_string_factory, string_factory_srv),
    REQUIRES_SERVICE_AS(mysql_string_charset_converter, string_converter_srv),
    REQUIRES_SERVICE_AS(table_access_factory_v1, ta_factory_srv),
    REQUIRES_SERVICE_AS(table_access_v1, ta_srv),
    REQUIRES_SERVICE_AS(table_access_index_v1, ta_index_srv),
    REQUIRES_SERVICE_AS(table_access_scan_v1, ta_scan_srv),
    REQUIRES_SERVICE_AS(table_access_update_v1, ta_update_srv),
    REQUIRES_SERVICE_AS(field_access_nullability_v1, fa_null_srv),
    REQUIRES_SERVICE_AS(field_integer_access_v1, fa_integer_srv),
    REQUIRES_SERVICE_AS(field_varchar_access_v1, fa_varchar_srv),
    END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_table_access)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_table_access", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_table_access, "mysql:test_table_access")
test_table_access_init, test_table_access_deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_table_access)
    END_DECLARE_LIBRARY_COMPONENTS
