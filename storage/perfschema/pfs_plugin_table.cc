/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_plugin_table.cc
  The performance schema implementation of plugin table.
*/

#include "storage/perfschema/pfs_plugin_table.h"

#include <mysql/components/services/pfs_plugin_table_service.h>
#include <list>
#include <string>

#include "sql/field.h"
#include "sql/pfs_priv_util.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/table_helper.h"
#include "storage/perfschema/table_plugin_table.h"

/* clang-format off */
/**
  @page PAGE_PFS_TABLE_PLUGIN_SERVICE Plugin table service
  Performance Schema plugin table service is a mechanism which provides
  plugins/components a way to expose their own tables in Performance Schema.


  @subpage SERVICE_INTRODUCTION

  @subpage SERVICE_INTERFACE

  @subpage EXAMPLE_PLUGIN_COMPONENT


  @page SERVICE_INTRODUCTION Service Introduction

  This service is named as <i>pfs_plugin_table</i> and it exposes two major
  methods which are\n
  - @c add_tables    : plugin/component to add tables in performance schema
  - @c delete_tables : plugin/component to delete tables from performance schema

  There are three major actors in its implementation:
  - PFS_engine_table_proxy \n
      This structure is collection of function pointers which are to be
      implemented by plugins/components who wish to expose table in
      performance_schema.
  - PFS_engine_table_share_proxy \n
      This structure is to keep the required information which would be used
      to register a table in performance_schema. Table share in pfs would be
      initialized using instance of PFS_engine_table_share_proxy and it would
      be passed by plugin while registering table.
  - Table_Handle \n
      An instance of this structure will be created when a table is opened
      in performance_schema. This will be used to keep current status of
      table during operation being performed on table.

  @section Block Diagram
  Following diagram shows the block diagram of PFS services functionality, to
  add a plugin table in performance schema, exposed via mysql-server component.

@startuml

  actor client as "Plugin/component"
  participant registry_service as "MySQL server registry service"
  box "Performance Schema Storage Engine" #LightBlue
  participant pfs_service as "pfs_plugin_table Service\n(mysql-server component)"
  participant pfs as "PFS Implementation"
  endbox

  == Initialization ==
  client -> client :
  note right: Initialize PFS_engine_table_share_proxy \n with required information.

  == Acquire pfs service ==
  client -> registry_service : Get registry service handle
  client -> registry_service : Acquire pfs service handle

  == pfs service call to add a table ==
  client -> pfs_service : PFS Service Call \n[add_tables()]
  pfs_service -> pfs    : PFS Implementation Call\n[pfs_add_tables()]

  pfs -> pfs_service    : Return result
  pfs_service -> client : Return result

  == Operations on table ==

  == pfs service call to delete a table ==
  client -> pfs_service : PFS Service Call \n[delete_tables()]
  pfs_service -> pfs    : PFS Implementation Call\n[pfs_delete_tables()]

  pfs -> pfs_service    : Return result
  pfs_service -> client : Return result

  == Release pfs service ==
  client -> registry_service : Release pfs service handle
  client -> registry_service : Release registry service handle

  == Cleanup ==
  client -> client :
  note right: Clean-up PFS_engine_table_share_proxy.

  @enduml

  @page SERVICE_INTERFACE Service Interface

  This interface is provided to plugins/components, using which they can expose
  their tables in performance schema. This interface consists of mainly two
  structures
  - <b>PFS_engine_table_proxy</b> \n
      This structure is collection of function pointers which are to be
      implemented by plugins/components who wish to expose table in
      performance_schema.\n\n
      Control flow will be redirected to these functions' implementations when
      query reaches to Performance Schema storage engine to perform some
      operation on the table added by plugin/component.

  - <b>PFS_engine_table_share_proxy</b> \n
      An instance of this structure is to keep the required information which
      would be used to register a table in performance_schema. Table share, for
      the table added by plugin/component, in performance schema would be
      initialized using this instance
      and it would be provided by plugin/component
      while registering table.

  Plugin/component is responsible to maintain buffers which would be used to
  store table data. During insert/fetch/delete etc, these buffers will be used.

  @section Flow Diagram
  Following flow diagram shows the execution of a select query executed on
  a table which is added in performance schema by plugin/component.

@startuml

  actor client as "Client"
  box "MySQL Server" #LightBlue
  participant server as "MySQL server"
  participant pfs as "Performance schema\n storage engine"
  participant pfs_table as "Performance schema\n plugin table"
  endbox
  participant plugin as "Plugin/Component code"

  == query start ==
  client -> server :
  note right: Query to plugin table \n in performance schema

  == Opening table ==
  server -> pfs : ha_perfschema::open()
  activate pfs_table
  server -> pfs : ha_perfschema::rnd_init()
  pfs -> pfs_table : table_plugin_table::create()
  pfs_table -> plugin : open_table()
  plugin -> plugin :
  note right: Create an instance of \n PSI_table_handle

  pfs_table <-- plugin : return PSI_table_handle
  pfs <-- pfs_table : return table_plugin_table instance

  pfs -> pfs_table : rnd_init()
  pfs_table -> plugin : rnd_init (PSI_table_handle*)

  pfs_table <-- plugin: return
  pfs <-- pfs_table : return
  server <-- pfs : return

  == For each row ==
  server -> pfs : ha_perfschema::rnd_next()
  pfs -> pfs_table : rnd_next()
  pfs_table -> plugin : rnd_next(PSI_table_handle*)
  plugin -> plugin :
  note right: read row from \n stats buffer to table_handle
  pfs_table <-- plugin : return
  pfs <-- pfs_table : return

  pfs -> pfs_table : read_row_values()
  pfs_table -> plugin : read_row_values()
  plugin -> plugin :
  note right: read a column value \n from table_handle to field

  pfs_table <-- plugin : return
  pfs_table <-- pfs_table :
  note right: loop till all \n fields are read

  pfs <-- pfs_table : result set row
  server <-- pfs : result set row
  client <-- server : result set row

  == Closing table ==
  server -> pfs : ha_perfschema::rnd_end()
  pfs -> pfs_table : rnd_end()
  pfs_table -> plugin : rnd_end(PSI_table_handle);

  server -> pfs : ha_perfschema::close()
  pfs -> pfs_table : delete table instance
  pfs_table -> plugin : close_table()
  plugin -> plugin :
  note right: delete PSI_table_handle instance
  deactivate pfs_table

  pfs_table <-- plugin : return
  pfs <-- pfs_table : return
  server <-- pfs : return

  == query end ==
  client <-- server :
  note right: query end

  @enduml
*/
/* clang-format on */

struct PSI_POS;
struct PSI_RECORD;

bool plugin_table_service_initialized = false;

/**
 * Traverse all fields one by one and pass the values to be inserted to
 * plugin's/component's write_row() implementation.
 *
 * @param pfs_table PFS table handle.
 * @param table	    Server table in which data is to be inserted.
 * @param buf       Buffer (not used)
 * @param fields    Array of fields in the table
 */
static int write_row(PFS_engine_table *pfs_table, TABLE *table,
                     unsigned char *buf [[maybe_unused]], Field **fields) {
  int result = 0;
  Field *f;
  table_plugin_table *temp = (table_plugin_table *)pfs_table;

  for (; (f = *fields); fields++) {
    if (bitmap_is_set(table->write_set, f->field_index())) {
      result = temp->m_st_table->write_column_value(
          temp->plugin_table_handle, (PSI_field *)f, f->field_index());
      if (result) {
        return result;
      }
    }
  }

  /* After all the columns values are written, write the row */
  return temp->m_st_table->write_row_values(temp->plugin_table_handle);
}

/**
 * Initialize table share when table is being added.
 *
 * @param share        table share to be initialized
 * @param proxy_share  Proxy shared passed from plugin/component
 *
 * return 0 for success
 */
static int initialize_table_share(PFS_engine_table_share *share,
                                  PFS_engine_table_share_proxy *proxy_share) {
  /* Set acl */
  switch (proxy_share->m_acl) {
    case READONLY:
      share->m_acl = &pfs_readonly_acl;
      break;
    case TRUNCATABLE:
      share->m_acl = &pfs_truncatable_acl;
      break;
    case UPDATABLE:
      share->m_acl = &pfs_updatable_acl;
      break;
    case EDITABLE:
      share->m_acl = &pfs_editable_acl;
      break;
    default: /* Unknown ACL */
      share->m_acl = &pfs_unknown_acl;
      break;
  }

  /* Open table pointer to open a table with share provided */
  share->m_open_table = table_plugin_table::create;

  share->m_write_row = write_row;
  share->m_delete_all_rows = proxy_share->delete_all_rows;
  share->m_get_row_count = proxy_share->get_row_count;

  share->m_ref_length = proxy_share->m_ref_length;

  share->m_thr_lock_ptr = new THR_LOCK();

  share->m_table_def =
      new Plugin_table("performance_schema", proxy_share->m_table_name,
                       proxy_share->m_table_definition,
                       "ENGINE = 'PERFORMANCE_SCHEMA'", nullptr);

  share->m_perpetual = false;

  /* List of call back function pointers pointing to the interface functions
   * implemented by plugin/component.
   */
  memcpy(&share->m_st_table, &proxy_share->m_proxy_engine_table,
         sizeof(PFS_engine_table_proxy));

  /*Initialize table share locks */
  thr_lock_init(share->m_thr_lock_ptr);

  share->m_ref_count = 0;

  return 0;
}

/**
 * Destroy a table share
 *
 * @param share share to be destroyed.
 */
static void destroy_table_share(PFS_engine_table_share *share) {
  assert(share);

  thr_lock_delete(share->m_thr_lock_ptr);
  delete share->m_table_def;
  delete share->m_thr_lock_ptr;
  delete share;
}

/**
 * Add plugin/component tables in performance schema.
 *
 * @param st_share_list	   List of PFS_engine_table_share_proxy instances
 *                         initialized/populated by plugin/component.
 * @param share_list_count Number of shares in the list
 *
 * @return 0 for success.
 */
static int pfs_add_tables_v1(PFS_engine_table_share_proxy **st_share_list,
                             uint share_list_count) {
  PFS_engine_table_share_proxy *temp_st_share;

  /* A list of table shares 'to be added' */
  std::list<PFS_engine_table_share *> share_list;

  /* ============== CRITICAL SECTION 1 (begin) ================= */
  pfs_external_table_shares.lock_share_list();

  /* Check if any of the table already exist in PFS or there are duplicate
   * table names in share list.
   * This would cause traversing share list once more but its beneficial in case
   * when duplicate name was found at the end of the list when all earlier
   * shares are initialized.
   */
  for (uint i = 0; i < share_list_count; i++) {
    temp_st_share = st_share_list[i];
    assert(temp_st_share && temp_st_share->m_table_name &&
           temp_st_share->m_table_name_length > 0);

    /* If table already exists either in
     * - Native PFS tables list
     * - Other (non native) PFS tables list (including purgatory)
     */
    if (PFS_engine_table::find_engine_table_share(
            temp_st_share->m_table_name) ||
        pfs_external_table_shares.find_share(temp_st_share->m_table_name,
                                             true)) {
      pfs_external_table_shares.unlock_share_list();
      return ER_TABLE_EXISTS_ERROR;
    }
  }

  /* Now traverse the share list again to initialize and add table shares to
     PFS shares list.
   */
  for (uint i = 0; i < share_list_count; i++) {
    temp_st_share = st_share_list[i];

    /* Create a new instance of table_share */
    PFS_engine_table_share *temp_share = new PFS_engine_table_share();

    /* Initialize table share for this new table */
    if (initialize_table_share(temp_share, temp_st_share)) {
      /* Delete all the initialized table shares till now */
      for (auto share : share_list) {
        pfs_external_table_shares.remove_share(share);
        destroy_table_share(share);
      }
      pfs_external_table_shares.unlock_share_list();
      return 1;
    } else {
      /* Add share to PFS shares list */
      pfs_external_table_shares.add_share(temp_share);
      share_list.push_back(temp_share);
    }
  }

  /* Unlock mutex on PFS share list now because while creating table (by DD API)
     same mutex will be locked again while searching a share in PFS share list
  */
  pfs_external_table_shares.unlock_share_list();
  /* ============== CRITICAL SECTION 1 (end) ================= */

  /* At this point, all the shares have been added to PFS share list. Now
     traverse the share list again and create tables using DD API
  */
  for (uint i = 0; i < share_list_count; i++) {
    temp_st_share = st_share_list[i];

    Plugin_table t(PERFORMANCE_SCHEMA_str.str, temp_st_share->m_table_name,
                   temp_st_share->m_table_definition,
                   "engine = 'performance_schema'", nullptr);
    if (create_native_table_for_pfs(&t)) {
      /* ============== CRITICAL SECTION 2 (begin) ================= */
      pfs_external_table_shares.lock_share_list();
      /* Delete all the initialized table share till now */
      for (auto share : share_list) {
        pfs_external_table_shares.remove_share(share);
        destroy_table_share(share);
      }
      pfs_external_table_shares.unlock_share_list();
      /* ============== CRITICAL SECTION 2 (end) ================= */
      return 1;
    }
  }

  return 0;
}

/**
 * Delete plugin/component tables form performance schema.
 *
 * @param st_share_list	    List of PFS_engine_table_share_proxy instances
 *                          initialized/populated by plugin/component.
 * @param share_list_count  Number of shares in the list
 *
 * @return 0 for success
 */
static int pfs_delete_tables_v1(PFS_engine_table_share_proxy **st_share_list,
                                uint share_list_count) {
  /* A list of shares 'to be removed' */
  std::list<PFS_engine_table_share *> share_list;

  /* ============== CRITICAL SECTION 1 (begin) ================= */
  pfs_external_table_shares.lock_share_list();

  /* Check if any of the table, in the list, doesn't exist. */
  for (uint i = 0; i < share_list_count; i++) {
    PFS_engine_table_share_proxy *temp_st_share = st_share_list[i];
    assert(temp_st_share && temp_st_share->m_table_name &&
           temp_st_share->m_table_name_length > 0);

    /* Search table share for the table in other list (including purgatory) */
    PFS_engine_table_share *temp_share =
        pfs_external_table_shares.find_share(temp_st_share->m_table_name, true);

    if (temp_share != nullptr) {
      /* If share found in global list,
       *  - Move it to purgatory.
       *  - Add it to the list of shares to be removed.
       */
      temp_share->m_in_purgatory = true;
      share_list.push_back(temp_share);
    }
  }

  pfs_external_table_shares.unlock_share_list();
  /* ============== CRITICAL SECTION 1 ( end ) ================= */

  /* At this point, all 'to be removed' shares are moved to purgatory. So no
   * new thread would be able to search them in global shares list, therefore
   * no new query could be run on these tables.
   *
   * Now traverse share_list and drop tables using DD API.
   */
  for (auto share : share_list) {
    if (drop_native_table_for_pfs(PERFORMANCE_SCHEMA_str.str,
                                  share->m_table_def->get_name())) {
      return 1;
    }
  }

  /* ============== CRITICAL SECTION 2 (begin) ================= */
  pfs_external_table_shares.lock_share_list();

  /* At this point tables have been dropped. So we can remove shares from
   * PFS shares list.
   */
  for (auto share : share_list) {
    pfs_external_table_shares.remove_share(share);
    destroy_table_share(share);
  }

  pfs_external_table_shares.unlock_share_list();
  /* ============== CRITICAL SECTION 2 ( end ) ================= */

  return 0;
}

/* Helper functions to store/fetch value into/from a field */

/**************************************
 * Type TINYINT                       *
 **************************************/
void set_field_tinyint_v1(PSI_field *f, PSI_tinyint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_tiny(f_ptr, value.val);
  }
}

void set_field_utinyint_v1(PSI_field *f, PSI_utinyint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_utiny(f_ptr, value.val);
  }
}

void get_field_tinyint_v1(PSI_field *f, PSI_tinyint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_tiny(f_ptr);
  value->is_null = false;
}

void get_field_utinyint_v1(PSI_field *f, PSI_utinyint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_utiny(f_ptr);
  value->is_null = false;
}

void read_key_tinyint_v1(PSI_key_reader *reader, PSI_plugin_key_tinyint *key,
                         int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  char temp_value{0};
  key->m_find_flags =
      pfs_reader->read_int8(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

void read_key_utinyint_v1(PSI_key_reader *reader, PSI_plugin_key_utinyint *key,
                          int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  unsigned char temp_value{0};
  key->m_find_flags =
      pfs_reader->read_uint8(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

bool match_key_tinyint_v1(bool record_null, long record_value,
                          PSI_plugin_key_tinyint *key) {
  return PFS_key_long::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

bool match_key_utinyint_v1(bool record_null, unsigned long record_value,
                           PSI_plugin_key_utinyint *key) {
  return PFS_key_ulong::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

/**************************************
 * Type SMALLINT                      *
 **************************************/
void set_field_smallint_v1(PSI_field *f, PSI_smallint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_short(f_ptr, value.val);
  }
}

void set_field_usmallint_v1(PSI_field *f, PSI_usmallint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_ushort(f_ptr, value.val);
  }
}

void get_field_smallint_v1(PSI_field *f, PSI_smallint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_short(f_ptr);
  value->is_null = false;
}

void get_field_usmallint_v1(PSI_field *f, PSI_usmallint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_ushort(f_ptr);
  value->is_null = false;
}

void read_key_smallint_v1(PSI_key_reader *reader, PSI_plugin_key_smallint *key,
                          int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  short temp_value{0};
  key->m_find_flags =
      pfs_reader->read_int16(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

void read_key_usmallint_v1(PSI_key_reader *reader,
                           PSI_plugin_key_usmallint *key, int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  unsigned short temp_value{0};
  key->m_find_flags =
      pfs_reader->read_uint16(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

bool match_key_smallint_v1(bool record_null, long record_value,
                           PSI_plugin_key_smallint *key) {
  return PFS_key_long::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

bool match_key_usmallint_v1(bool record_null, unsigned long record_value,
                            PSI_plugin_key_usmallint *key) {
  return PFS_key_ulong::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

/**************************************
 * Type MEDIUMINT                     *
 **************************************/
void set_field_mediumint_v1(PSI_field *f, PSI_mediumint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_medium(f_ptr, value.val);
  }
}

void set_field_umediumint_v1(PSI_field *f, PSI_umediumint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_umedium(f_ptr, value.val);
  }
}

void get_field_mediumint_v1(PSI_field *f, PSI_mediumint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_medium(f_ptr);
  value->is_null = false;
}

void get_field_umediumint_v1(PSI_field *f, PSI_umediumint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_umedium(f_ptr);
  value->is_null = false;
}

void read_key_mediumint_v1(PSI_key_reader *reader,
                           PSI_plugin_key_mediumint *key, int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  long temp_value{0};
  key->m_find_flags =
      pfs_reader->read_int24(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

void read_key_umediumint_v1(PSI_key_reader *reader,
                            PSI_plugin_key_umediumint *key, int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  unsigned long temp_value{0};
  key->m_find_flags =
      pfs_reader->read_uint24(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

bool match_key_mediumint_v1(bool record_null, long record_value,
                            PSI_plugin_key_mediumint *key) {
  return PFS_key_long::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

bool match_key_umediumint_v1(bool record_null, unsigned long record_value,
                             PSI_plugin_key_umediumint *key) {
  return PFS_key_ulong::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

/**************************************
 * Type INTEGER (INT)                 *
 **************************************/
void set_field_integer_v1(PSI_field *f, PSI_int value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_long(f_ptr, value.val);
  }
}

void set_field_uinteger_v1(PSI_field *f, PSI_uint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_ulong(f_ptr, value.val);
  }
}

void get_field_integer_v1(PSI_field *f, PSI_int *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_long(f_ptr);
  value->is_null = false;
}

void get_field_uinteger_v1(PSI_field *f, PSI_int *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_ulong(f_ptr);
  value->is_null = false;
}

void read_key_integer_v1(PSI_key_reader *reader, PSI_plugin_key_integer *key,
                         int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  long temp_value{0};
  key->m_find_flags =
      pfs_reader->read_long(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

void read_key_uinteger_v1(PSI_key_reader *reader, PSI_plugin_key_uinteger *key,
                          int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  unsigned long temp_value{0};
  key->m_find_flags =
      pfs_reader->read_ulong(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

bool match_key_integer_v1(bool record_null, long record_value,
                          PSI_plugin_key_integer *key) {
  return PFS_key_long::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

bool match_key_uinteger_v1(bool record_null, unsigned long record_value,
                           PSI_plugin_key_uinteger *key) {
  return PFS_key_ulong::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

/**************************************
 * Type BIGINT                        *
 **************************************/
void set_field_bigint_v1(PSI_field *f, PSI_bigint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_longlong(f_ptr, value.val);
  }
}

void set_field_ubigint_v1(PSI_field *f, PSI_ubigint value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_ulonglong(f_ptr, value.val);
  }
}

void get_field_bigint_v1(PSI_field *f, PSI_bigint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_longlong(f_ptr);
  value->is_null = false;
}

void get_field_ubigint_v1(PSI_field *f, PSI_ubigint *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_ulonglong(f_ptr);
  value->is_null = false;
}

void read_key_bigint_v1(PSI_key_reader *reader, PSI_plugin_key_bigint *key,
                        int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  long long temp_value{0};
  key->m_find_flags =
      pfs_reader->read_longlong(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

void read_key_ubigint_v1(PSI_key_reader *reader, PSI_plugin_key_ubigint *key,
                         int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  unsigned long long temp_value{0};
  key->m_find_flags =
      pfs_reader->read_ulonglong(e_find_flag, key->m_is_null, &temp_value);
  key->m_value = temp_value;
}

bool match_key_bigint_v1(bool record_null, long long record_value,
                         PSI_plugin_key_bigint *key) {
  return PFS_key_longlong::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

bool match_key_ubigint_v1(bool record_null, unsigned long long record_value,
                          PSI_plugin_key_ubigint *key) {
  return PFS_key_ulonglong::stateless_match(
      record_null, record_value, key->m_is_null, key->m_value,
      (enum ha_rkey_function)key->m_find_flags);
}

/**************************************
 * Type DECIMAL                       *
 **************************************/
void set_field_decimal_v1(PSI_field *f, PSI_double value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_decimal(f_ptr, value.val);
  }
}

void get_field_decimal_v1(PSI_field *f, PSI_double *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_decimal(f_ptr);
  value->is_null = false;
}

/**************************************
 * Type FLOAT                         *
 **************************************/
void set_field_float_v1(PSI_field *f, PSI_double value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_float(f_ptr, value.val);
  }
}

void get_field_float_v1(PSI_field *f, PSI_double *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_float(f_ptr);
  value->is_null = false;
}

/**************************************
 * Type DOUBLE                        *
 **************************************/
void set_field_double_v1(PSI_field *f, PSI_double value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_double(f_ptr, value.val);
  }
}

void get_field_double_v1(PSI_field *f, PSI_double *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_double(f_ptr);
  value->is_null = false;
}

/**************************************
 * Type CHAR                          *
 **************************************/
void set_field_char_utf8mb4_v1(PSI_field *f, const char *value, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_char_utf8mb4(f_ptr, value, len);
  } else {
    f_ptr->set_null();
  }
}

void get_field_char_utf8mb4_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_char_utf8mb4(f_ptr, val, len);
}

/**************************************
 * Type VARCAHAR                      *
 **************************************/
void set_field_varchar_utf8mb4_len_v1(PSI_field *f, const char *str, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_varchar_utf8mb4(f_ptr, str, len);
  } else {
    f_ptr->set_null();
  }
}

void set_field_varchar_utf8mb4_v1(PSI_field *f, const char *str) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (str != nullptr) {
    set_field_varchar_utf8mb4(f_ptr, str);
  } else {
    f_ptr->set_null();
  }
}

void get_field_varchar_utf8mb4_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_varchar_utf8mb4(f_ptr, val, len);
}

/**************************************
 * Type BLOB/TEXT                     *
 **************************************/
void set_field_blob_v1(PSI_field *f, const char *val, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_blob(f_ptr, val, len);
  } else {
    f_ptr->set_null();
  }
}

void get_field_blob_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_blob(f_ptr, val, len);
}

/**************************************
 * Type ENUM                          *
 **************************************/
void set_field_enum_v1(PSI_field *f, PSI_enum value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_enum(f_ptr, value.val);
  }
}

void get_field_enum_v1(PSI_field *f, PSI_enum *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_enum(f_ptr);
  value->is_null = false;
}

/**************************************
 * Type DATE                          *
 **************************************/
void set_field_date_v1(PSI_field *f, const char *value, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_date(f_ptr, value, len);
  } else {
    f_ptr->set_null();
  }
}

void get_field_date_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_date(f_ptr, val, len);
}

/**************************************
 * Type TIME                          *
 **************************************/
void set_field_time_v1(PSI_field *f, const char *value, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_time(f_ptr, value, len);
  } else {
    f_ptr->set_null();
  }
}

void get_field_time_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_time(f_ptr, val, len);
}

/**************************************
 * Type DATETIME                      *
 **************************************/
void set_field_datetime_v1(PSI_field *f, const char *value, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_datetime(f_ptr, value, len);
  } else {
    f_ptr->set_null();
  }
}

void get_field_datetime_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_datetime(f_ptr, val, len);
}

/**************************************
 * Type TIMESTAMP                     *
 **************************************/
void set_field_timestamp_v1(PSI_field *f, const char *value, uint len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (len > 0) {
    set_field_timestamp(f_ptr, value, len);
  } else {
    f_ptr->set_null();
  }
}

void set_field_timestamp2_v1(PSI_field *f, ulonglong value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value > 0) {
    set_field_timestamp(f_ptr, value);
  } else {
    f_ptr->set_null();
  }
}

void get_field_timestamp_v1(PSI_field *f, char *val, uint *len) {
  Field *f_ptr = reinterpret_cast<Field *>(f);

  /* If NULL is provided */
  if (f_ptr->is_null()) {
    *len = 0;
    val[0] = '\0';
    return;
  }

  val = get_field_timestamp(f_ptr, val, len);
}

/**************************************
 * Type YEAR                          *
 **************************************/
void set_field_year_v1(PSI_field *f, PSI_year value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (value.is_null) {
    f_ptr->set_null();
  } else {
    set_field_year(f_ptr, value.val);
  }
}

void get_field_year_v1(PSI_field *f, PSI_year *value) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  if (f_ptr->is_null()) {
    value->is_null = true;
    return;
  }

  value->val = get_field_year(f_ptr);
  value->is_null = false;
}

/**************************************
 * NULL                               *
 **************************************/
void set_field_null_v1(PSI_field *f) {
  Field *f_ptr = reinterpret_cast<Field *>(f);
  f_ptr->set_null();
}

unsigned int get_parts_found_v1(PSI_key_reader *reader) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  return pfs_reader->m_parts_found;
}

void read_key_string_v1(PSI_key_reader *reader, PSI_plugin_key_string *key,
                        int find_flag) {
  PFS_key_reader *pfs_reader = (PFS_key_reader *)reader;
  enum ha_rkey_function e_find_flag = (enum ha_rkey_function)find_flag;

  key->m_find_flags = PFS_key_pstring::stateless_read(
      *pfs_reader, e_find_flag, key->m_is_null, key->m_value_buffer,
      &key->m_value_buffer_length, key->m_value_buffer_capacity);
}

bool match_key_string_v1(bool record_null, const char *record_string_value,
                         unsigned int record_string_length,
                         PSI_plugin_key_string *key) {
  return PFS_key_pstring::stateless_match(
      record_null, record_string_value, record_string_length,
      key->m_value_buffer, key->m_value_buffer_length, key->m_is_null,
      (enum ha_rkey_function)key->m_find_flags);
}

void init_pfs_plugin_table() {
  assert(!plugin_table_service_initialized);

  /* Asserts that ERRORS defined in pfs_plugin_table_service.h are in
     accordance with ERRORS defined in my_base.h
  */
  static_assert(
      (PFS_HA_ERR_WRONG_COMMAND == HA_ERR_WRONG_COMMAND) &&
          (PFS_HA_ERR_RECORD_DELETED == HA_ERR_RECORD_DELETED) &&
          (PFS_HA_ERR_END_OF_FILE == HA_ERR_END_OF_FILE) &&
          (PFS_HA_ERR_NO_REFERENCED_ROW == HA_ERR_NO_REFERENCED_ROW) &&
          (PFS_HA_ERR_FOUND_DUPP_KEY == HA_ERR_FOUND_DUPP_KEY) &&
          (PFS_HA_ERR_RECORD_FILE_FULL == HA_ERR_RECORD_FILE_FULL),
      "");

  pfs_external_table_shares.init_mutex();
  plugin_table_service_initialized = true;
  return;
}

void cleanup_pfs_plugin_table() {
  if (plugin_table_service_initialized) {
    pfs_external_table_shares.destroy_mutex();
    plugin_table_service_initialized = false;
  }
}

/* clang-format off */

/* Initialization of service methods to actual PFS implementation */
BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_table_v1)
    pfs_add_tables_v1, pfs_delete_tables_v1, get_parts_found_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_tiny_v1)
    set_field_tinyint_v1,  set_field_utinyint_v1, get_field_tinyint_v1,
    get_field_utinyint_v1, read_key_tinyint_v1,   read_key_utinyint_v1,
    match_key_tinyint_v1,  match_key_utinyint_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_small_v1)
    set_field_smallint_v1,  set_field_usmallint_v1, get_field_smallint_v1,
    get_field_usmallint_v1, read_key_smallint_v1,   read_key_usmallint_v1,
    match_key_smallint_v1,  match_key_usmallint_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_medium_v1)
    set_field_mediumint_v1,  set_field_umediumint_v1, get_field_mediumint_v1,
    get_field_umediumint_v1, read_key_mediumint_v1,   read_key_umediumint_v1,
    match_key_mediumint_v1,  match_key_umediumint_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_integer_v1)
    set_field_integer_v1,  set_field_uinteger_v1, get_field_integer_v1,
    get_field_uinteger_v1, read_key_integer_v1,   read_key_uinteger_v1,
    match_key_integer_v1,  match_key_uinteger_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_bigint_v1)
    set_field_bigint_v1,  set_field_ubigint_v1, get_field_bigint_v1,
    get_field_ubigint_v1, read_key_bigint_v1,   read_key_ubigint_v1,
    match_key_bigint_v1,  match_key_ubigint_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_decimal_v1)
    set_field_decimal_v1, get_field_decimal_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_float_v1)
    set_field_float_v1, get_field_float_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_double_v1)
    set_field_double_v1, get_field_double_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_string_v2)
    set_field_char_utf8mb4_v1,
    get_field_char_utf8mb4_v1,
    read_key_string_v1,
    match_key_string_v1,
    get_field_varchar_utf8mb4_v1,
    set_field_varchar_utf8mb4_v1,
    set_field_varchar_utf8mb4_len_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_blob_v1)
    set_field_blob_v1, get_field_blob_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_enum_v1)
    set_field_enum_v1, get_field_enum_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_date_v1)
    set_field_date_v1, get_field_date_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_time_v1)
    set_field_time_v1, get_field_time_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_datetime_v1)
    set_field_datetime_v1, get_field_datetime_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_timestamp_v1)
    set_field_timestamp_v1, get_field_timestamp_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_timestamp_v2)
    set_field_timestamp_v1, set_field_timestamp2_v1, get_field_timestamp_v1
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_year_v1)
    set_field_year_v1, get_field_year_v1
END_SERVICE_IMPLEMENTATION();

/* clang-format on */
