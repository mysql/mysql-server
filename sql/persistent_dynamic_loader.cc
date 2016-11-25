/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include <scope_guard.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../components/mysql_server/dynamic_loader.h"
#include "../components/mysql_server/persistent_dynamic_loader.h"
#include "../components/mysql_server/server_component.h"
#include "auth_common.h" // commit_and_close_mysql_tables
#include "derror.h"
#include "field.h"
#include "handler.h"
#include "key.h"
#include "log.h" // error_log_print
#include "m_string.h"
#include "mdl.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_global.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "mysql/components/service_implementation.h"
#include "mysqld_error.h"
#include "records.h"
#include "sql_base.h"
#include "sql_class.h"
#include "sql_const.h"
#include "sql_error.h"
#include "sql_string.h"
#include "table.h"
#include "thr_lock.h"
#include "mysqld.h"

typedef std::string my_string;

enum enum_component_table_field
{
  CT_FIELD_COMPONENT_ID = 0,
  CT_FIELD_GROUP_ID,
  CT_FIELD_COMPONENT_URN,
  CT_FIELD_COUNT /* a cool trick to count the number of fields :) */
};

static const TABLE_FIELD_TYPE component_table_fields[CT_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("component_id") },
    { C_STRING_WITH_LEN("int(10)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("component_group_id") },
    { C_STRING_WITH_LEN("int(10)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("component_urn") },
    { C_STRING_WITH_LEN("text") },
    { C_STRING_WITH_LEN("utf8") }
  }
};

static const TABLE_FIELD_DEF
  component_table_def= {CT_FIELD_COUNT, component_table_fields};

class Component_db_intact : public Table_check_intact
{
protected:
  void report_error(uint, const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)))
  {
    va_list args;
    va_start(args, fmt);
    error_log_print(ERROR_LEVEL, fmt, args);
    va_end(args);
  }
};

/** In case of an error, a message is printed to the error log. */
static Component_db_intact table_intact;

/**
  Closes tables that is being used by current thread processing.

  @param thd Thread context
  @param mdl_savepoint Save-point to revert to after closing tables.
  @return Status of performed operation
  @retval true failure
  @retval false success
*/
static bool close_tables(THD* thd, MDL_savepoint& mdl_savepoint)
{
  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  return false;
}

/**
  Open mysql.component table for read or write.

  Note that if the table can't be locked successfully this operation will
  close it. Therefore it provides guarantee that it either opens and locks
  table or fails without leaving any tables open.

  @param thd Thread context
  @param lock_type How to lock the table
  @param [out] table Pointer to table structure to store the open table into.
  @param [out] mdl_savepoint A pointer to MDL save-point to store current
    status of locks. In case of NULL, the save-point will not be acquired.
  @param acl_to_check acl type to check
  @return Status of performed operation
  @retval true open and lock failed - an error message is pushed into the
    stack.
  @retval false success
*/
static bool open_component_table(
  THD *thd, enum thr_lock_type lock_type, TABLE **table,
  MDL_savepoint* mdl_savepoint, ulong acl_to_check)
{
  TABLE_LIST tables;
  if (mdl_savepoint != NULL)
  {
    /* Take a save-point to release only the locks gathered during current
    execution flow, for example these taken on mysql.component table, at the
    end, when closing component table. This will keep intact then any other
    locks and the global locks by the caller before this call. */
    *mdl_savepoint= thd->mdl_context.mdl_savepoint();
  }

  tables.init_one_table("mysql", 5, "component", 9, "component", lock_type);

#ifndef EMBEDDED_LIBRARY
  if (mysql_persistent_dynamic_loader_imp::initialized() && !opt_noacl &&
      check_one_table_access(thd, acl_to_check, &tables))
    return true;
#endif

  if (open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    return true;
  }

  *table= tables.table;
  tables.table->use_all_columns();

  if (table_intact.check(thd, *table, &component_table_def))
  {
    if (mdl_savepoint != NULL)
    {
      close_tables(thd, *mdl_savepoint);
    }
    else
    {
      /* In case we don't have save-point, we just close transaction and tables.
      */
      commit_and_close_mysql_tables(thd);
    }
    return true;
  }

  return false;
}

/**
  Initializes persistence store, loads all groups of components registered in
  component table. Shouldn't be called multiple times. We assume the order
  specified by group ID is correct one. This should be assured by dynamic
  loader as long as it will not allow to unload the component that has
  dependency on, in case there would be a possibility to switch that
  dependency to other component that is not to be unloaded. If this is
  assured, then it will not be possible for components with lower group IDs
  to have a dependency on component with higher group ID, even after state is
  restored in this initialization method.

  @param thdp Current thread execution context
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_persistent_dynamic_loader_imp::init(void* thdp)
{
  try
  {
    THD* thd= reinterpret_cast<THD*>(thdp);
    if (mysql_persistent_dynamic_loader_imp::initialized())
    {
      return true;
    }

    TABLE* component_table;
    READ_RECORD read_record_info;
    int res;

    mysql_persistent_dynamic_loader_imp::group_id= 0;

    /* Open component table and scan read all records. */
    if (open_component_table(thd, TL_READ, &component_table, NULL, SELECT_ACL))
    {
      push_warning(thd, Sql_condition::SL_WARNING,
        ER_COMPONENT_TABLE_INCORRECT,
        ER_THD(thd, ER_COMPONENT_TABLE_INCORRECT));
      mysql_persistent_dynamic_loader_imp::is_initialized= true;
      return false;
    }

    auto guard= create_scope_guard([&thd]()
    {
      commit_and_close_mysql_tables(thd);
    });

    if (init_read_record(
      &read_record_info, thd, component_table, NULL, 1, 1, FALSE))
    {
      push_warning(thd, Sql_condition::SL_WARNING,
        ER_COMPONENT_TABLE_INCORRECT,
        ER_THD(thd, ER_COMPONENT_TABLE_INCORRECT));
      return false;
    }

    if (component_table->s->fields < CT_FIELD_COUNT)
    {
      push_warning(thd, Sql_condition::SL_WARNING,
        ER_COMPONENT_TABLE_INCORRECT,
        ER_THD(thd, ER_COMPONENT_TABLE_INCORRECT));
      return false;
    }

    /* All read records will be aggregated in groups by group ID. */
    std::map<uint64, std::vector<std::string> > component_groups;

    for (;;)
    {
      res= read_record_info.read_record(&read_record_info);
      if (res != 0)
      {
        break;
      }

      uint64 component_id=
        component_table->field[CT_FIELD_COMPONENT_ID]->val_int();
      uint64 component_group_id=
        component_table->field[CT_FIELD_GROUP_ID]->val_int();
      String component_urn_str;
      component_table->field[CT_FIELD_COMPONENT_URN]->val_str(
        &component_urn_str);

      std::string component_urn(
        component_urn_str.ptr(), component_urn_str.length());

      mysql_persistent_dynamic_loader_imp::group_id= std::max(
        mysql_persistent_dynamic_loader_imp::group_id.load(),
        component_group_id);

      component_groups[component_group_id].push_back(component_urn);
      mysql_persistent_dynamic_loader_imp::component_id_by_urn.emplace(
        component_urn, component_id);
    }

    end_read_record(&read_record_info);

    /* res is guaranteed to be != 0, -1 means end of records encountered, which
      is interpreted as a success. */
    DBUG_ASSERT(res != 0);
    if (res != -1)
    {
      return true;
    }

    for (auto it= component_groups.begin(); it != component_groups.end();
      ++it)
    {
      std::vector<const char*> urns;
      for (auto group_it= it->second.begin(); group_it != it->second.end();
      ++group_it)
      {
        urns.push_back(group_it->c_str());
      }
      /* We continue process despite of any errors. */
      mysql_dynamic_loader_imp::load(urns.data(), urns.size());
    }

    mysql_persistent_dynamic_loader_imp::is_initialized= true;

    return false;
  }
  catch (...)
  {
    mysql_components_handle_std_exception(__func__);
  }

  return true;
}
/**
  De-initializes persistence loader.
*/
void mysql_persistent_dynamic_loader_imp::deinit()
{
  mysql_persistent_dynamic_loader_imp::is_initialized= false;
  mysql_persistent_dynamic_loader_imp::component_id_by_urn.clear();
}
/**
 Initialisation status of persistence loader.

 @return Status of performed operation
 @retval true initialization is done
 @retval false initialization is not done
*/
bool mysql_persistent_dynamic_loader_imp::initialized()
{
  return mysql_persistent_dynamic_loader_imp::is_initialized;
}

/**
  Loads specified group of components by URN, initializes them and
  registers all service implementations present in these components.
  Assures all dependencies will be met after loading specified components.
  The dependencies may be circular, in such case it's necessary to specify
  all components on cycle to load in one batch. From URNs specified the
  scheme part of URN (part before "://") is extracted and used to acquire
  service implementation of scheme component loader service for specified
  scheme. If the loading process successes then a group of Components by their
  URN is added to the component table.

  @param thd_ptr Current thread execution context.
  @param urns List of URNs of Components to load.
  @param component_count Number of Components on list to load.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(mysql_persistent_dynamic_loader_imp::load,
  (void* thd_ptr, const char *urns[], int component_count))
{
  try
  {
    THD* thd= (THD*)thd_ptr;

    if (!mysql_persistent_dynamic_loader_imp::initialized())
    {
      my_error(ER_COMPONENT_TABLE_INCORRECT, MYF(0));
      return true;
    }

    TABLE* component_table;
    MDL_savepoint mdl_savepoint;
    if (open_component_table(thd, TL_WRITE, &component_table,
                             &mdl_savepoint, INSERT_ACL))
    {
      my_error(ER_COMPONENT_TABLE_INCORRECT, MYF(0));
      return true;
    }

    if (mysql_dynamic_loader_imp::load(urns, component_count))
    {
      return true;
    }

    /* Unload components if anything goes wrong with handling changes. */
    auto guard= create_scope_guard([&thd, &urns, &component_count]()
    {
      mysql_dynamic_loader_imp::unload(urns, component_count);
    });

    auto guard_close_tables= create_scope_guard(
      [&thd, &component_table, &mdl_savepoint]
    {
      close_tables(thd, mdl_savepoint);
    });

    uint64 group_id= ++mysql_persistent_dynamic_loader_imp::group_id;

    /* Insert all component URNs into component table into one group. */
    CHARSET_INFO *system_charset= system_charset_info;
    for (int i= 0; i < component_count; ++i)
    {
      /* Get default values for fields. */
      restore_record(component_table, s->default_values);

      component_table->next_number_field=
        component_table->found_next_number_field;

      component_table->field[CT_FIELD_GROUP_ID]->store(group_id, true);
      component_table->field[CT_FIELD_COMPONENT_URN]->store(urns[i],
        strlen(urns[i]), system_charset);

      int res=
        component_table->file->ha_write_row(component_table->record[0]);
      if (res != 0)
      {
        my_error(ER_COMPONENT_MANIPULATE_ROW_FAILED, MYF(0), urns[i], res);
        return true;
      }

      /* Use last insert auto-increment column value and store it by the URN. */
      mysql_persistent_dynamic_loader_imp::component_id_by_urn.emplace(
        urns[i], component_table->file->insert_id_for_cur_row);

      component_table->file->ha_release_auto_increment();
    }

    guard.commit();

    return false;
  }
  catch (...)
  {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
  Unloads specified group of Components by URN, deinitializes them and
  unregisters all service implementations present in these components.
  Assumes, although does not check it, all dependencies of not unloaded
  components will still be met after unloading specified components.
  The dependencies may be circular, in such case it's necessary to specify
  all components on cycle to unload in one batch. From URNs specified the
  scheme part of URN (part before "://") is extracted and used to acquire
  service implementation of scheme component loader service for specified
  scheme. URN specified should be identical to ones specified in load()
  method, i.e. all letters must have the same case. If the unloading process
  successes then a group of Components by their URN is added to the component
  table.

  @param thd_ptr Current thread execution context.
  @param urns List of URNs of components to unload.
  @param component_count Number of components on list to unload.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(mysql_persistent_dynamic_loader_imp::unload,
  (void* thd_ptr, const char *urns[], int component_count))
{
  try
  {
    THD* thd= (THD*)thd_ptr;

    if (!mysql_persistent_dynamic_loader_imp::initialized())
    {
      my_error(ER_COMPONENT_TABLE_INCORRECT, MYF(0));
      return true;
    }

    int res;

    TABLE* component_table;
    MDL_savepoint mdl_savepoint;
    if (open_component_table(thd, TL_WRITE, &component_table,
                             &mdl_savepoint, DELETE_ACL))
    {
      my_error(ER_COMPONENT_TABLE_INCORRECT, MYF(0));
      return true;
    }

    bool result=
      mysql_dynamic_loader_imp::unload(urns, component_count);
    if (result)
    {
      /* No need to specify error, underlying service implementation would add
        one. */
      return result;
    }

    auto guard_close_tables= create_scope_guard(
    [&thd, &component_table, &mdl_savepoint]()
    {
      close_tables(thd, mdl_savepoint);
    });

    DBUG_ASSERT(component_table->key_info != NULL);

    for (int i= 0; i < component_count; ++i)
    {
      /* Find component ID used in component table using memory mapping. */
      auto it= mysql_persistent_dynamic_loader_imp::component_id_by_urn.find(
        urns[i]);
      if (it ==
        mysql_persistent_dynamic_loader_imp::component_id_by_urn.end())
      {
        /* Component was loaded with persistence bypassed? If we continue, the
          state will be still consistent. */
        push_warning_printf(thd, Sql_condition::SL_WARNING,
          ER_WARN_UNLOAD_THE_NOT_PERSISTED,
          ER_THD(thd, ER_WARN_UNLOAD_THE_NOT_PERSISTED),
          urns[i]);
        continue;
      }
      component_table->field[CT_FIELD_COMPONENT_ID]->store(it->second, true);

      /* Place PK index on specified record and delete it. */
      uchar key[MAX_KEY_LENGTH];
      key_copy(key, component_table->record[0], component_table->key_info,
        component_table->key_info->key_length);

      res= component_table->file->ha_index_read_idx_map(
        component_table->record[0], 0, key, HA_WHOLE_KEY, HA_READ_KEY_EXACT);
      if (res != 0)
      {
        my_error(ER_COMPONENT_MANIPULATE_ROW_FAILED, MYF(0), urns[i], res);
        return true;
      }

      res= component_table->file->ha_delete_row(component_table->record[0]);
      if (res != 0)
      {
        my_error(ER_COMPONENT_MANIPULATE_ROW_FAILED, MYF(0), urns[i], res);
        return true;
      }

      mysql_persistent_dynamic_loader_imp::component_id_by_urn.erase(it);
    }

    return false;
  }
  catch (...)
  {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

std::atomic<uint64> mysql_persistent_dynamic_loader_imp::group_id;
std::map<my_string, uint64>
  mysql_persistent_dynamic_loader_imp::component_id_by_urn;
bool mysql_persistent_dynamic_loader_imp::is_initialized;

/**
  Initializes persistence store, loads all groups of components registered in
    component table.

  @param thd Current thread execution context
  @return Status of performed operation
  @retval false success
  @retval true failure
  */
bool persistent_dynamic_loader_init(void* thd)
{
  return mysql_persistent_dynamic_loader_imp::init(reinterpret_cast<THD*>(thd));
}

void persistent_dynamic_loader_deinit()
{
  mysql_persistent_dynamic_loader_imp::deinit();
}
