/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/components/component_implementation.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/table_access_service.h>

#include "table_access_service_impl.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/mysqld.h"  // mysqld_server_started
#include "sql/mysqld_thd_manager.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/table.h"
#include "sql/transaction.h"
#include "thr_lock.h"

/* clang-format off */
/**
  @page PAGE_TABLE_ACCESS_SERVICE Table Access service

  The TABLE ACCESS service allows components to read and write
  to MySQL tables owned by the component.

  The table access service is meant to be used to operate on
  "well known" tables, with a known name and structure.
  It is not meant to access arbitrary tables in a generic way.

  The overall service consists of the following parts:
  - The table access factory (@ref s_mysql_table_access_factory_v1)
  - The table access (@ref s_mysql_table_access_v1)
  - The table access index (@ref s_mysql_table_access_index_v1)
  - The table access scan (@ref s_mysql_table_access_scan_v1)
  - The table access update (@ref s_mysql_table_access_update_v1)

  To manipulate table columns in a type safe manner,
  and to decouple data types from the general service,
  each data type supported is manipulated using the following parts:
  - The field access for integer types (@ref s_mysql_field_integer_access_v1)
  - The field access for varchar types (@ref s_mysql_field_varchar_access_v1)

  In addition, to manipulate fields in a generic way,
  each field can be read from and written to using 'any':
  - The field access for any types (@ref s_mysql_field_any_access_v1)

  All these parts are related, and share the @c Table_access type.

  To read from, or write to, a MySQL table from a component,
  the component code must, in that order:
  - create a table access object, using the factory,
  - populate the table access object, with all the tables to open,
  - call the begin operation,
  - check that the table DDL for each table corresponds to the component
    expectations.

  At this point, the tables are ready for use.
  Supported operations are:
  - full table scan,
  - index open,
  - index scans,
  - index fetch,
  - insert, update or delete on a row.

  For table access sessions that update data, the session can be committed.

  To complete the session, either commit or rollback,
  then destroy the session object.

  The table access service only supports basic DML operations (no DDL).

  Please note that, in all the code examples below:
  - the variables "*_srv" points to the proper service part,
    without details,
  - proper error handling is missing.

  This is deliberate, to have legible code examples.

  @section TA_FACTORY Factory

  The entry point to the table access service is the factory part
  (@ref s_mysql_table_access_factory_v1).

  Example below:

  @code
    MYSQL_THD thd;
    Table_access ta;
    current_thd_srv->get(&thd);
    ta = srv->create_table_access(thd, 3);
    // session using up to 3 tables here
    srv->destroy_table_access(ta);
  @endcode

  @section TA_ADD_TABLE Add tables

  Every table involved in the table access session must be added
  explicitly, using @ref s_mysql_table_access_v1

  Table can be read from or written to.

  Example below:

  @code
    size_t ticket_customer;
    size_t ticket_order;
    size_t ticket_order_item;

    // Add table shop.customer
    ticket_customer = srv->add_table(ta,
                                     "shop", 4,
                                     "customer", 8,
                                     TA_READ);

    // Add table shop.order
    ticket_order = srv->add_table(ta,
                                  "shop", 4,
                                  "order", 5,
                                  TA_WRITE);

    // Add table shop.order_line
    ticket_order_line = srv->add_table(ta,
                                       "shop", 4,
                                       "order_line", 10,
                                       TA_WRITE);
  @endcode

  The result of add_table is a ticket that will be used later to retrieve
  the table once opened. These tickets should be preserved in the calling code.

  @section TA_BEGIN Begin

  An important property of the table access service is that all tables
  used in a given table access session are opened together, and locked
  together.

  This part is critical for the mysqld server operation:
  - all the metadata locks involved must be acquired at the same time,
  - it prevents deadlocks between concurrent client sessions.

  The component code should be prepared to handle errors,
  as all tables might not exist in the database (install or upgrade in
  progress), or might exist but not be writable (GLOBAL READ LOCK in place
  in the current session, system in READ ONLY or SUPER READ ONLY state).

  Example below:

  @code
    int rc = srv->begin(ta);
    if (rc != 0) {
      // failed
    }
  @endcode

  @section TA_CHECK Check tables

  Once the open and lock operation succeeds,
  tables with the proper @em name are guaranteed to exist in the database.

  This says nothing about the actual table @em structure.

  Note that:
  - component @em code is compiled in a component @em binary, deployed in a
    @em library, delivered with a given life cycle (software install),
  - component @em data is stored in a MySQL @em table, stored ultimately
    somewhere on @em disk, which follows a different life cycle
    (database install, upgrade, backup, restore).

  The next step to perform table dml is to make sure that the table structure
  implemented in the component @em code actually matches the table structure
  found on @em disk.

  The way to achieve this is to declare, in the code, the expected table
  structure, and compare it to the table found after open.

  This check must be performed for every table involved.

  Note that it is not required to check every column in a table:
  only checking the columns that the code actually uses is sufficient.

  Example below:

  @code
  TA_table table_customer;
  TA_table table_order;
  TA_table table_order_line;

  // Find the opened tables
  table_customer = srv->get_table(ta, ticket_customer);
  table_order = srv->get_table(ta, ticket_order);
  table_order_line = srv->get_table(ta, ticket_order_line);

  // Verify the table shop.customer structure
  static const int COL_ID = 0;
  static const int COL_NAME = 1;
  static const int COL_ADDRESS = 2;
  static const TA_table_field_def columns_customer[] = {
    {COL_ID, "ID", 2, TA_TYPE_INTEGER, 0},
    {COL_NAME, "NAME", 4, TA_TYPE_VARCHAR, 64},
    {COL_ADDRESS, "ADDRESS", 7, TA_TYPE_VARCHAR, 255}};

  rc = srv->check_table_fields(ta, table_customer, columns_customer, 3);
  if (rc != 0) {
    // Failed: the table on disk is not what the code expects
  }

  // Verify the table shop.order structure
  static const TA_table_field_def columns_order[] = { ... };
  rc = srv->check_table_fields(ta, table_order, columns_order, ...);

  // Verify the table shop.order_line structure
  static const TA_table_field_def columns_order_line[] = { ... };
  rc = srv->check_table_fields(ta, table_order_line, columns_order_line, ...);
  @endcode

  @section TA_TABLE_SCAN Table scan

  Once a table structure has been checked, the code can now access
  specific columns with safety.

  Columns used when reading a row must be acquired based on the
  column ordinal position in the table, which is known by the calling code.

  The table scan itself consists of a loop, processing one row at a time.

  Example below:

  @code
  my_h_string name_value;
  my_h_string address_value;

  string_factory_srv->create(&name_value);;
  string_factory_srv->create(&address_value);;

  scan_srv->init(ta, table_customer);

  while (scan_srv->next(ta, table_customer) == 0) {
    // NAME column in table shop.customer is as index 1
    srv_varchar->get(ta, table_customer, COL_NAME, name_value);

    // ADDRESS column in table shop.customer is at index 2
    srv_varchar->get(ta, table_customer, COL_ADDRESS, address_value);

    // Use name_value and address_value
  }

  scan_srv->end(ta, table_customer);

  string_factory_srv->destroy(name_value);;
  string_factory_srv->destroy(address_value);;
  @endcode

  @section TA_INDEX_OPEN Index open

  To use an index, the caller must provide:
  - the index name, per the table DDL,
  - the list of columns part of the index definition, per the table DDL.

  This is required to ensure that what:
  - the index used in the code
  - the index defined in the actual table

  are actually the same.

  Example below:

  @code
  // For a table such as:
  //   CREATE TABLE person (NAME VARCHAR(50), SURNAME VARCHAR(50), ...);
  //   ALTER TABLE person ADD INDEX NAME_AND_SURNAME(NAME, SURNAME);
  // Index metadata:
  // - the key name is NAME_AND_SURNAME
  // - the columns in the key are:
  //   - "NAME" (in ascending order)
  //   - "SURNAME" (in ascending order)
  // - the number of columns in the key is 2

  const char *index_name = "NAME_AND_SURNAME";
  size_t int index_name_length = 16;
  const TA_index_field_def index_cols[] = {
    {"NAME", 4, true}
    {"SURNAME", 7, true}
  };
  const size_t index_numcol = 2;

  // Open the index.

  TA_key index_key = nullptr;
  if (index_srv->init(ta,
                      table_person,
                      index_name, index_name_length,
                      index_cols, index_numcol,
                      &index_key)) {
    // Opening index failed.
  }

  // Use the index.
  ...

  // Close the index.

  if (index_key != nullptr) {
    index_srv->end(access, table_person, index_key);
  }
  @endcode

  @section TA_INDEX_SCAN Index scan

  An index scan is similar to a full table scan,
  except that rows will be processed in index order.

  Example below:

  @code
  rc = index_srv->first(ta, table_person, index_key);

  while (rc == 0) {
    // Do something with the current row.

    // Find the next row.
    rc = index_srv->next(ta, table_person, index_key);
  }
  @endcode

  @section TA_INDEX_FETCH Index fetch

  An index fetch can be executed to find particular rows.

  The sequence consist of:
  - populating the search key
  - looking up the index for that key
  - optionally, when the key is not unique,
    loop for more records matching the search key.

  Example involving a simple fetch below:

  @code
  my_h_string name_value = ...;
  my_h_string surname_value = ...;
  CHARSET_INFO_h utf8 = charset_srv->get_utf8mb4();

  // name_value = "Doe"
  string_convert_srv->convert_from_buffer(name_value, "Doe", 3, utf8);
  // surname_value = "John"
  string_convert_srv->convert_from_buffer(surname_value, "John", 4, utf8);

  // Write NAME = "Doe" in the search key (at index 0)
  fa_varchar_srv->set(ta, table_person, COL_NAME, name_value));

  // Write SURNAME = "John" in the search key (at index 1)
  fa_varchar_srv->set(ta, table_person, COL_SURNAME, surname_value));

  // Fetch the index with a 2 key parts search key (NAME + SURNAME)
  rc = index_srv->read_map(ta, table_person, 2, index_key);

  if (rc == 0) {
    // Found John Doe.
  }
  @endcode

  Example involving a partial key fetch below:

  @code
  my_h_string name_value = ...;
  my_h_string surname_value = ...;
  CHARSET_INFO_h utf8 = charset_srv->get_utf8mb4();

  // name_value = "Smith"
  string_convert_srv->convert_from_buffer(name_value, "Smith", 5, utf8);

  // Write NAME = "Smith" in the search key
  fa_varchar_srv->set(ta, table_person, COL_NAME, &name_value));

  // Fetch the index with a 1 key parts search key (NAME only)
  rc = index_srv->read_map(ta, table_person, 1, index_key);

  while (rc == 0) {
    // Found someone named Smith, read the surname.
    fa_varchar_srv->get(ta, table_person, COL_SURNAME, surname_value));

    // Find next record
    rc = index_srv->next_same(ta, table_person, index_key);
  }
  @endcode

  @section TA_INSERT Write data

  To insert a record, the table must be added to the table access session in
  TA_WRITE mode.

  An insert consist of:
  - writing to each column in the current record
  - calling the insert service.

  Example below:

  @code
  my_h_string name_value = ...;
  my_h_string address_value = ...;

  srv_varchar->set(ta, table_customer, COL_NAME, name_value);
  srv_varchar->set(ta, table_customer, COL_ADDRESS, address_value);

  rc = update_srv->insert(ta, table_customer);
  if (rc == 0) {
    // Row inserted.
  }
  @endcode

  @section TA_UPDATE Update data

  To update a record, the table must be added to the table access session in
  TA_WRITE mode.

  The record to update must be located in a scan:
  - either in a init() / next() table scan loop
  - or in an init() / first() / next() index scan loop
  - or in an init() / read_map() / next_same() index fetch

  Then:
  - write columns to change in the current record
  - invoke the update service

  Example below:

  @code
  // Table cursor positioned on the row to update

  // Write to columns
  my_h_string address_value = ...;
  srv_varchar->set(ta, table_customer, COL_ADDRESS, address_value);

  // Update the current row
  rc = update_srv->update(ta, table_customer);
  if (rc == 0) {
    // Row Updated.
  }
  @endcode

  @section TA_DELETE Delete data

  Similar with an update, the table must be opened in TA_WRITE mode,
  and the table cursor positioned on the row to delete.

  Example below:

  @code
  // Table cursor positioned on the row to delete

  // Delete the current row
  rc = update_srv->delete_row(ta, table_customer);
  if (rc == 0) {
    // Row Deleted.
  }
  @endcode
*/
/* clang-format on */

static CHARSET_INFO *get_api_charset() { return &my_charset_utf8mb4_bin; }

static String *from_api(my_h_string api) {
  // FIXME: This breaks the mysql_string_service implementation open.
  // The problem is that Field::store() does not use my_h_string.
  return reinterpret_cast<String *>(api);
}

class TA_table_impl;

struct Table_state {
  char m_schema_name[NAME_LEN + 1];
  size_t m_schema_name_length;
  char m_table_name[NAME_LEN + 1];
  size_t m_table_name_length;
};

class Table_access_impl {
 public:
  static Table_access to_api(Table_access_impl *impl) {
    return reinterpret_cast<Table_access>(impl);
  }

  static Table_access_impl *from_api(Table_access api) {
    return reinterpret_cast<Table_access_impl *>(api);
  }

  Table_access_impl(THD *thd, size_t count);
  ~Table_access_impl();

  size_t add_table(const char *schema_name, size_t schema_name_length,
                   const char *table_name, size_t table_name_length,
                   enum thr_lock_type lock_type);
  int begin();
  int commit();
  int rollback();
  TABLE *get_table(size_t index);

 private:
  /** Array of size m_max_count. */
  Table_ref *m_table_array;
  /** Array of size m_max_count. */
  Table_state *m_table_state_array;
  size_t m_current_count;
  size_t m_max_count;
  bool m_write;
  bool m_in_tx;

  THD *m_parent_thd;
  THD *m_child_thd;
};

class TA_table_impl {
 public:
  static TA_table to_api(TABLE *impl) {
    return reinterpret_cast<TA_table>(impl);
  }

  static TABLE *from_api(TA_table api) {
    return reinterpret_cast<TABLE *>(api);
  }
};

class TA_key_impl {
 public:
  static TA_key to_api(TA_key_impl *impl) {
    return reinterpret_cast<TA_key>(impl);
  }

  static TA_key_impl *from_api(TA_key api) {
    return reinterpret_cast<TA_key_impl *>(api);
  }

  TA_key_impl();
  ~TA_key_impl() {}

  void key_copy(uchar *record, uint key_length);

 public:
  /* Metadata */
  size_t m_key_index;
  KEY *m_key_info;

  /* Data */
  uchar m_key[MAX_KEY_LENGTH];
  uint m_key_length;
};

TA_key_impl::TA_key_impl() {
  m_key_index = 0;
  m_key_info = nullptr;
  memset(m_key, 0, sizeof(m_key));
  m_key_length = 0;
}

void TA_key_impl::key_copy(uchar *record, uint key_length) {
  /* Actual key may be a prefix */
  assert(key_length <= m_key_info->key_length);
  m_key_length = key_length;
  ::key_copy(m_key, record, m_key_info, m_key_length);
}

Table_access_impl::Table_access_impl(THD *thd, size_t count)
    : m_current_count(0), m_max_count(count), m_write(false), m_in_tx(false) {
  m_parent_thd = thd;

  m_child_thd = new THD(true);

  if (m_parent_thd)
    m_child_thd->copy_table_access_properties(m_parent_thd);
  else {
    void *dummy_p = nullptr;
    m_child_thd->thread_stack = (char *)&dummy_p;
    m_child_thd->security_context()->assign_user(
        STRING_WITH_LEN("table_access"));
    m_child_thd->security_context()->skip_grants("", "");
    my_thread_init();
  }

  m_child_thd->real_id = my_thread_self();
  m_child_thd->set_new_thread_id();

  // FIXME: this breaks the calling code for the table_access duration.
  m_child_thd->store_globals();

  /*
    Because this code creates a child THD object for the same session,
    and runs with it, this child THD must be visible somehow,
    so that the DBA can have a chance to KILL it.
    Hence, we add the child THD in the global "session" list,
    so that SHOW PROCESS can actually see it.
  */
  Global_THD_manager::get_instance()->add_thd(m_child_thd);

  m_table_array = new Table_ref[m_max_count];
  m_table_state_array = new Table_state[m_max_count];
}

Table_access_impl::~Table_access_impl() {
  if (m_in_tx) {
    trans_rollback_stmt(m_child_thd);
  }

  close_thread_tables(m_child_thd);

  if (!mysqld_server_started) {
    /*
      After initialization of the server, InnoDB's data dictionary cache is
      reset. It requires all tables, including the cached ones, to be released.
    */
    close_cached_tables(m_child_thd, m_table_array, false, LONG_TIMEOUT);
  }

  m_child_thd->release_resources();
  m_child_thd->restore_globals();

  if (m_parent_thd) m_parent_thd->store_globals();

  Global_THD_manager::get_instance()->remove_thd(m_child_thd);

  delete m_child_thd;

  delete[] m_table_array;
  delete[] m_table_state_array;

  if (!m_parent_thd) my_thread_end();

  // FIXME : kill flag ?
  // FIXME : nested THD status variables ?
}

enum thr_lock_type convert_lock_type(TA_lock_type api_lock_type) {
  enum thr_lock_type result;
  switch (api_lock_type) {
    case TA_READ:
      result = TL_READ;
      break;
    case TA_WRITE:
      result = TL_WRITE;
      break;
    default:
      assert(false);
      result = TL_READ;
  }
  return result;
}

TA_field_type field_type_to_api(enum enum_field_types impl_field_type,
                                bool &has_length) {
  TA_field_type result = TA_TYPE_UNKNOWN;
  has_length = false;
  switch (impl_field_type) {
    case MYSQL_TYPE_VARCHAR:
      result = TA_TYPE_VARCHAR;
      has_length = true;
      break;
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_INT24:
      result = TA_TYPE_INTEGER;
      break;
    case MYSQL_TYPE_JSON:
      result = TA_TYPE_JSON;
    default:
      break;
  };
  return result;
}

size_t Table_access_impl::add_table(const char *schema_name,
                                    size_t schema_name_length,
                                    const char *table_name,
                                    size_t table_name_length,
                                    enum thr_lock_type lock_type) {
  if (m_current_count >= m_max_count) {
    return UINT16_MAX;
  }

  if (lock_type == TL_WRITE) {
    m_write = true;
  }

  Table_state *state = &m_table_state_array[m_current_count];

  uint errors;
  CHARSET_INFO *cs = get_api_charset();

  state->m_schema_name_length = my_convert(
      state->m_schema_name, sizeof(state->m_schema_name) - 1,
      system_charset_info, schema_name, schema_name_length, cs, &errors);
  state->m_schema_name[state->m_schema_name_length] = '\0';

  state->m_table_name_length = my_convert(
      state->m_table_name, sizeof(state->m_table_name) - 1, system_charset_info,
      table_name, table_name_length, cs, &errors);
  state->m_table_name[state->m_table_name_length] = '\0';

  Table_ref *current = &m_table_array[m_current_count];

  // FIXME : passing alias = table_name to force MDL key init.
  *current = Table_ref(state->m_schema_name, state->m_schema_name_length,
                       state->m_table_name, state->m_table_name_length,
                       state->m_table_name, lock_type);
  assert(current->mdl_request.key.length() != 0);

  current->next_local = nullptr;
  current->next_global = nullptr;
  current->open_type = OT_BASE_ONLY;  // TODO: VIEWS ?
  current->open_strategy = Table_ref::OPEN_IF_EXISTS;

  if (m_current_count > 0) {
    Table_ref *prev = &m_table_array[m_current_count - 1];
    prev->next_local = current;
    prev->next_global = current;
  }

  return m_current_count++;
}

int Table_access_impl::begin() {
  /*
    Read lock must be acquired during entire open_and_lock_tables function
    call, because shutdown process can make its internals unavailable in the
    middle of the call. If tables are acquired before the shutdown process, the
    shutdown process will not deallocate internals until tables are closed.
   */
  rwlock_scoped_lock rdlock(&LOCK_server_shutting_down, false, __FILE__,
                            __LINE__);

  if (server_shutting_down) return TA_ERROR_OPEN;

  if (m_write && m_parent_thd != nullptr) {
    if (m_parent_thd->global_read_lock.is_acquired()) {
      /*
        Avoid waiting in the child THD session
        for the global read lock held by the parent THD session,
        which is a self deadlock for this thread.
      */
      return TA_ERROR_GRL;
    }
    if (check_readonly(m_parent_thd, false)) {
      /*
        Honor READONLY and SUPER_READONLY.
      */
      return TA_ERROR_READONLY;
    }
  }

  if (open_and_lock_tables(m_child_thd, m_table_array,
                           MYSQL_LOCK_IGNORE_TIMEOUT)) {
    return TA_ERROR_OPEN;
  }

  assert(!m_in_tx);
  m_in_tx = true;
  return 0;
}

TABLE *Table_access_impl::get_table(size_t index) {
  if (index < m_current_count) {
    TABLE *table = m_table_array[index].table;
    if (table != nullptr) {
      table->use_all_columns();

      return table;
    }
  }

  return nullptr;
}

int Table_access_impl::commit() {
  assert(m_in_tx);
  m_in_tx = false;
  if (trans_commit_stmt(m_child_thd)) {
    return 1;
  }
  return 0;
}

int Table_access_impl::rollback() {
  assert(m_in_tx);
  m_in_tx = false;
  if (trans_rollback_stmt(m_child_thd)) {
    return 1;
  }
  return 0;
}

Table_access impl_create_table_access(THD *thd, size_t count) {
  if (count == 0) {
    return nullptr;
  }

  Table_access_impl *ta = new Table_access_impl(thd, count);
  Table_access result = Table_access_impl::to_api(ta);
  return result;
}

void impl_destroy_table_access(Table_access api_ta) {
  Table_access_impl *ta = Table_access_impl::from_api(api_ta);
  delete ta;
}

size_t impl_add_table(Table_access api_ta, const char *schema_name,
                      size_t schema_name_length, const char *table_name,
                      size_t table_name_length, TA_lock_type api_lock_type) {
  Table_access_impl *ta = Table_access_impl::from_api(api_ta);
  const enum thr_lock_type lock_type = convert_lock_type(api_lock_type);
  assert(ta != nullptr);
  const size_t result = ta->add_table(schema_name, schema_name_length,
                                      table_name, table_name_length, lock_type);
  return result;
}

int impl_begin(Table_access api_ta) {
  Table_access_impl *ta = Table_access_impl::from_api(api_ta);
  assert(ta != nullptr);
  const int result = ta->begin();
  return result;
}

int impl_commit(Table_access api_ta) {
  Table_access_impl *ta = Table_access_impl::from_api(api_ta);
  assert(ta != nullptr);
  const int result = ta->commit();
  return result;
}

int impl_rollback(Table_access api_ta) {
  Table_access_impl *ta = Table_access_impl::from_api(api_ta);
  assert(ta != nullptr);
  const int result = ta->rollback();
  return result;
}

TA_table impl_get_table(Table_access api_ta, size_t index) {
  Table_access_impl *ta = Table_access_impl::from_api(api_ta);
  assert(ta != nullptr);
  TABLE *table_impl = ta->get_table(index);
  TA_table result = TA_table_impl::to_api(table_impl);
  return result;
}

int impl_check_table_fields(Table_access /* api_ta */, TA_table api_table,
                            const TA_table_field_def *fields,
                            size_t fields_count) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  TABLE_SHARE *share = table->s;
  assert(share != nullptr);

  if (share->fields < fields_count) {
    return 1;
  }

  const TA_table_field_def *expected_field;
  Field *actual_field;
  size_t actual_field_name_length;
  size_t index;
  size_t expected_index;
  TA_field_type actual_type;
  bool actual_nullable;
  bool has_length;

  uint errors;
  CHARSET_INFO *cs = get_api_charset();
  char expected_field_name[NAME_LEN + 1];
  size_t expected_field_name_length;
  int cmp;

  for (index = 0, expected_field = fields; index < fields_count;
       index++, expected_field++) {
    expected_index = expected_field->m_index;

    /* Convert the expected field name from UTF8MB4 to system_charset_info. */
    expected_field_name_length =
        my_convert(expected_field_name, sizeof(expected_field_name) - 1,
                   system_charset_info, expected_field->m_name,
                   expected_field->m_name_length, cs, &errors);
    expected_field_name[expected_field_name_length] = '\0';

    if (errors) {
      /* User input not well formed UTF8MB4. */
      return 2;
    }

    if (expected_index > share->fields) {
      return 3;
    }

    actual_field = share->field[expected_index];
    actual_type = field_type_to_api(actual_field->type(), has_length);
    actual_nullable = actual_field->is_nullable();

    /* FIXME: Field::field_name has no associated length member */
    actual_field_name_length = strlen(actual_field->field_name);
    /* Compare the expected field name to the actual. */
    cmp = my_strnncoll(
        system_charset_info, reinterpret_cast<uchar *>(expected_field_name),
        expected_field_name_length,
        reinterpret_cast<const uchar *>(actual_field->field_name),
        actual_field_name_length);

    if ((cmp != 0) || (actual_type != expected_field->m_type) ||
        (actual_type == TA_TYPE_UNKNOWN)) {
      return 4;
    }

    if (actual_nullable != expected_field->m_nullable) {
      return 5;
    }

    /* For types that have a length. */
    if (has_length) {
      const size_t actual = actual_field->char_length();
      if (actual != expected_field->m_length) {
        return 6;
      }
    }
  }

  return 0;
}

int impl_index_init(Table_access /* api_ta */, TA_table api_table,
                    const char *index_name, size_t index_name_length,
                    const TA_index_field_def *fields, size_t fields_count,
                    TA_key *api_key) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  assert(index_name != nullptr);
  assert(index_name_length != 0);
  assert(fields != nullptr);
  assert(fields_count != 0);
  assert(api_key != nullptr);
  const TABLE_SHARE *share = table->s;
  assert(share != nullptr);
  size_t index;
  bool found = false;
  KEY *key_info = nullptr;

  uint errors;
  CHARSET_INFO *cs = get_api_charset();
  char expected_index_name[NAME_LEN + 1];
  size_t expected_index_name_length;
  size_t actual_index_name_length;
  int cmp;

  /* Convert the expected index name from UTF8MB4 to system_charset_info. */
  expected_index_name_length = my_convert(
      expected_index_name, sizeof(expected_index_name) - 1, system_charset_info,
      index_name, index_name_length, cs, &errors);
  expected_index_name[expected_index_name_length] = '\0';

  if (errors) {
    return HA_ERR_UNSUPPORTED;
  }

  *api_key = nullptr;

  for (index = 0; index < share->keys; index++) {
    key_info = &table->key_info[index];

    /* FIXME: KEY::name has no associated length member */
    actual_index_name_length = strlen(key_info->name);
    /* Compare the expected index name to the actual. */
    cmp = my_strnncoll(system_charset_info,
                       reinterpret_cast<uchar *>(expected_index_name),
                       expected_index_name_length,
                       reinterpret_cast<const uchar *>(key_info->name),
                       actual_index_name_length);

    if (cmp == 0) {
      found = true;
      break;
    }
  }

  if (!found) {
    return HA_ERR_TABLE_DEF_CHANGED;
  }

  if (key_info->actual_key_parts != fields_count) {
    return HA_ERR_TABLE_DEF_CHANGED;
  }

  char expected_field_name[NAME_LEN + 1];
  size_t expected_field_name_length;
  size_t actual_field_name_length;

  for (unsigned int i = 0; i < fields_count; i++) {
    KEY_PART_INFO *actual_part = &key_info->key_part[i];
    const TA_index_field_def *expected_part = &fields[i];

    /* Convert the expected field name from UTF8MB4 to system_charset_info. */
    expected_field_name_length =
        my_convert(expected_field_name, sizeof(expected_field_name) - 1,
                   system_charset_info, expected_part->m_name,
                   expected_part->m_name_length, cs, &errors);
    expected_field_name[expected_field_name_length] = '\0';

    if (errors) {
      return HA_ERR_UNSUPPORTED;
    }

    /* FIXME: Field::name has no associated length member */
    actual_field_name_length = strlen(actual_part->field->field_name);
    /* Compare the expected field name to the actual. */
    cmp = my_strnncoll(
        system_charset_info, reinterpret_cast<uchar *>(expected_field_name),
        expected_field_name_length,
        reinterpret_cast<const uchar *>(actual_part->field->field_name),
        actual_field_name_length);

    if (cmp != 0) {
      return HA_ERR_TABLE_DEF_CHANGED;
    }

#ifdef LATER
    if (actual_part->xxx != expected_part->m_ascending) {
      return HA_ERR_TABLE_DEF_CHANGED;
    }
#endif
  }

  if (!key_info->is_visible) {
    /*
      For invisible indexes, we can either:
      - fail now, before the index is actually removed
      - fail later, after the index is actually removed
      Fail fast, fail now.
      TODO: raise an error instead ?
    */
    return HA_ERR_TABLE_DEF_CHANGED;
  }

  // TODO: sort order ?
  const int result = table->file->ha_index_init(index, false);

  if (result == 0) {
    TA_key_impl *key = new TA_key_impl();
    key->m_key_index = index;
    key->m_key_info = key_info;

    *api_key = TA_key_impl::to_api(key);
  }

  return result;
}

int impl_index_read_map(Table_access /* api_ta */, TA_table api_table,
                        size_t num_parts, TA_key api_key) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  TA_key_impl *key = TA_key_impl::from_api(api_key);
  assert(key != nullptr);
  int result;

  assert(num_parts > 0);
  assert(num_parts <= key->m_key_info->actual_key_parts);

  /*
    NUM_PARTS | KEY_PART_MAP
    ------------------------
            1 | b1 = 1
            2 | b11 = 3
            3 | b111 = 7
            4 | b1111 = 15
    etc
  */
  const key_part_map map = make_prev_keypart_map(num_parts);

  const uint key_len = calculate_key_len(table, key->m_key_index, map);

  key->key_copy(table->record[0], key_len);

  result = table->file->ha_index_read_map(table->record[0], key->m_key, map,
                                          HA_READ_KEY_EXACT);

  if (result == 0) {
    if (table->record[1]) {
      store_record(table, record[1]);
    }
  }

  return result;
}

int impl_index_first(Table_access /* api_ta */, TA_table api_table,
                     TA_key /* api_key */) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  int result;

  result = table->file->ha_index_first(table->record[0]);

  if (result == 0) {
    if (table->record[1]) {
      store_record(table, record[1]);
    }
  }

  return result;
}

int impl_index_next(Table_access /* api_ta */, TA_table api_table,
                    TA_key /* api_key */) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  int result;

  result = table->file->ha_index_next(table->record[0]);

  if (result == 0) {
    if (table->record[1]) {
      store_record(table, record[1]);
    }
  }

  return result;
}

int impl_index_next_same(Table_access /* api_ta */, TA_table api_table,
                         TA_key api_key) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  TA_key_impl *key = TA_key_impl::from_api(api_key);
  assert(key != nullptr);
  int result;

  result = table->file->ha_index_next_same(table->record[0], key->m_key,
                                           key->m_key_length);

  if (result == 0) {
    if (table->record[1]) {
      store_record(table, record[1]);
    }
  }

  return result;
}

int impl_index_end(Table_access /* api_ta */, TA_table api_table,
                   TA_key api_key) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  TA_key_impl *key = TA_key_impl::from_api(api_key);
  assert(key != nullptr);
  const int result = table->file->ha_index_end();
  delete key;
  return result;
}

int impl_rnd_init(Table_access /* api_ta */, TA_table api_table) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  const int result = table->file->ha_rnd_init(true);

  if (result == 0) {
    if (table->record[1]) {
      store_record(table, record[1]);
    }
  }

  return result;
}

int impl_rnd_next(Table_access /* api_ta */, TA_table api_table) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  const int result = table->file->ha_rnd_next(table->record[0]);

  if (result == 0) {
    if (table->record[1]) {
      store_record(table, record[1]);
    }
  }

  return result;
}

int impl_rnd_end(Table_access /* api_ta */, TA_table api_table) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  const int result = table->file->ha_rnd_end();
  return result;
}

int impl_write_row(Table_access /* api_ta */, TA_table api_table) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  const int result = table->file->ha_write_row(table->record[0]);
  return result;
}

int impl_update_row(Table_access /* api_ta */, TA_table api_table) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  const int result =
      table->file->ha_update_row(table->record[1], table->record[0]);
  return result;
}

int impl_delete_row(Table_access /* api_ta */, TA_table api_table) {
  TABLE *table = TA_table_impl::from_api(api_table);
  assert(table != nullptr);
  const int result = table->file->ha_delete_row(table->record[0]);
  return result;
}

static Field *get_field(TABLE *table, size_t index) {
  assert(table != nullptr);
  assert(index < table->s->fields);
  Field *f = table->field[index];
  assert(f != nullptr);
  assert(f->table == table);
  return f;
}

void impl_set_field_null(Table_access /* api_ta */, TA_table api_table,
                         size_t index) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  assert(f->is_nullable());
  f->set_null();
}

bool impl_is_field_null(Table_access /* api_ta */, TA_table api_table,
                        size_t index) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  assert(f->is_nullable());
  return f->is_null();
}

int impl_set_field_integer_value(Table_access /* api_ta */, TA_table api_table,
                                 size_t index, const long long v) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  int result;

  type_conversion_status cs;
  f->set_notnull();
  cs = f->store(v, false);
  result = (cs == TYPE_OK) ? 0 : 1;

  return result;
}

int impl_get_field_integer_value(Table_access /* api_ta */, TA_table api_table,
                                 size_t index, long long *v) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);

  assert(!f->is_null());
  *v = f->val_int();

  return 0;
}

int impl_set_field_varchar_value(Table_access /* api_ta */, TA_table api_table,
                                 size_t index, my_h_string v) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  String *value = from_api(v);
  assert(value != nullptr);
  int result;

  type_conversion_status tcs;
  f->set_notnull();
  tcs = f->store(value->ptr(), value->length(), value->charset());
  result = (tcs == TYPE_OK) ? 0 : 1;

  return result;
}

int impl_get_field_varchar_value(Table_access /* api_ta */, TA_table api_table,
                                 size_t index, my_h_string v) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  assert(f != nullptr);
  String *value = from_api(v);
  assert(value != nullptr);

  assert(!f->is_null());
  String *s_ptr [[maybe_unused]];
  s_ptr = f->val_str(value, value);
  assert(s_ptr == value);

  return 0;
}

int impl_set_field_any_value(Table_access /* api_ta */, TA_table api_table,
                             size_t index, my_h_string v) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  String *value = from_api(v);
  assert(value != nullptr);
  int result;

  type_conversion_status tcs;
  f->set_notnull();
  tcs = f->store(value->ptr(), value->length(), value->charset());
  result = (tcs == TYPE_OK) ? 0 : 1;

  return result;
}

int impl_get_field_any_value(Table_access /* api_ta */, TA_table api_table,
                             size_t index, my_h_string v) {
  TABLE *table = TA_table_impl::from_api(api_table);
  Field *f = get_field(table, index);
  assert(f != nullptr);
  String *value = from_api(v);
  assert(value != nullptr);

  assert(!f->is_null());
  String *s_ptr [[maybe_unused]];
  s_ptr = f->val_str(value, value);
  assert(s_ptr == value);

  return 0;
}

SERVICE_TYPE(table_access_factory_v1)
SERVICE_IMPLEMENTATION(mysql_server, table_access_factory_v1) = {
    impl_create_table_access, impl_destroy_table_access};

SERVICE_TYPE(table_access_v1)
SERVICE_IMPLEMENTATION(mysql_server, table_access_v1) = {
    impl_add_table, impl_begin,     impl_commit,
    impl_rollback,  impl_get_table, impl_check_table_fields};

SERVICE_TYPE(table_access_index_v1)
SERVICE_IMPLEMENTATION(mysql_server, table_access_index_v1) = {
    impl_index_init, impl_index_read_map,  impl_index_first,
    impl_index_next, impl_index_next_same, impl_index_end};

SERVICE_TYPE(table_access_scan_v1)
SERVICE_IMPLEMENTATION(mysql_server, table_access_scan_v1) = {
    impl_rnd_init, impl_rnd_next, impl_rnd_end};

SERVICE_TYPE(table_access_update_v1)
SERVICE_IMPLEMENTATION(mysql_server, table_access_update_v1) = {
    impl_write_row, impl_update_row, impl_delete_row};

SERVICE_TYPE(field_access_nullability_v1)
SERVICE_IMPLEMENTATION(mysql_server, field_access_nullability_v1) = {
    impl_set_field_null, impl_is_field_null};

SERVICE_TYPE(field_integer_access_v1)
SERVICE_IMPLEMENTATION(mysql_server, field_integer_access_v1) = {
    impl_set_field_integer_value, impl_get_field_integer_value};

SERVICE_TYPE(field_varchar_access_v1)
SERVICE_IMPLEMENTATION(mysql_server, field_varchar_access_v1) = {
    impl_set_field_varchar_value, impl_get_field_varchar_value};

SERVICE_TYPE(field_any_access_v1)
SERVICE_IMPLEMENTATION(mysql_server, field_any_access_v1) = {
    impl_set_field_any_value, impl_get_field_any_value};
