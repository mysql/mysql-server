/* Copyright (c) 2014, 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/dictionary_impl.h"

#include <string.h>
#include <memory>

#include "auth_common.h"                   // acl_init
#include "binlog_event.h"
#include "bootstrap.h"                     // bootstrap::bootstrap_functor
#include "dd/cache/dictionary_client.h"    // dd::Dictionary_client
#include "dd/dd.h"                         // enum_dd_init_type
#include "dd/impl/bootstrapper.h"          // dd::Bootstrapper
#include "dd/impl/system_registry.h"       // dd::System_tables
#include "dd/impl/tables/dd_properties.h"  // get_actual_dd_version()
#include "dd/info_schema/metadata.h"       // dd::info_schema::store_dynamic...
#include "dd/upgrade/upgrade.h"            // dd::upgrade
#include "m_ctype.h"
#include "mdl.h"
#include "my_dbug.h"
#include "mysql/thread_type.h"
#include "opt_costconstantcache.h"         // init_optimizer_cost_module
#include "sql_class.h"                     // THD
#include "system_variables.h"

///////////////////////////////////////////////////////////////////////////

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Implementation details.
///////////////////////////////////////////////////////////////////////////

class Object_table;

Dictionary_impl *Dictionary_impl::s_instance= NULL;

Dictionary_impl *Dictionary_impl::instance()
{
  return s_instance;
}

Object_id Dictionary_impl::DEFAULT_CATALOG_ID= 1;
const String_type Dictionary_impl::DEFAULT_CATALOG_NAME("def");

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::init(enum_dd_init_type dd_init)
{
  if (dd_init == enum_dd_init_type::DD_INITIALIZE ||
      dd_init == enum_dd_init_type::DD_RESTART_OR_UPGRADE)
  {
    DBUG_ASSERT(!Dictionary_impl::s_instance);

    if (Dictionary_impl::s_instance)
      return false; /* purecov: inspected */

    std::unique_ptr<Dictionary_impl> d(new Dictionary_impl());

    Dictionary_impl::s_instance= d.release();
  }

  acl_init(true);

  /*
    Initialize the cost model, but delete it after the dd is initialized.
    This is because the cost model is needed for the dd initialization, but
    it must be re-initialized later after the plugins have been initialized.
    Upgrade process needs heap engine initialized, hence parameter 'true'
    is passed to the function.
  */
  init_optimizer_cost_module(true);

  /*
    Install or start or upgrade the dictionary
    depending on bootstrapping option.
  */

  bool result= false;

  // Creation of Data Dictionary through current server
  if (dd_init == enum_dd_init_type::DD_INITIALIZE)
    result= ::bootstrap::run_bootstrap_thread(NULL, &bootstrap::initialize,
                                              SYSTEM_THREAD_DD_INITIALIZE);

  // Creation of INFORMATION_SCHEMA system views.
  else if (dd_init == enum_dd_init_type::DD_INITIALIZE_SYSTEM_VIEWS)
    result= ::bootstrap::run_bootstrap_thread(NULL,
                                              &dd::info_schema::initialize,
                                              SYSTEM_THREAD_DD_INITIALIZE);

  /*
    Creation of Dictionary Tables in old Data Directory
    This function also takes care of normal server restart.
  */
  else if (dd_init == enum_dd_init_type::DD_RESTART_OR_UPGRADE)
    result= ::bootstrap::run_bootstrap_thread(NULL,
                         &upgrade::do_pre_checks_and_initialize_dd,
                         SYSTEM_THREAD_DD_INITIALIZE);

  // Populate metadata in DD tables from old data directory and do cleanup.
  else if (dd_init == enum_dd_init_type::DD_POPULATE_UPGRADE)
    result= ::bootstrap::run_bootstrap_thread(NULL,
                         &upgrade::fill_dd_and_finalize,
                         SYSTEM_THREAD_DD_INITIALIZE);


  // Delete DD tables and do cleanup in case of error in upgrade
  else if (dd_init == enum_dd_init_type::DD_DELETE)
    result= ::bootstrap::run_bootstrap_thread(NULL,
                         &upgrade::terminate,
                         SYSTEM_THREAD_DD_INITIALIZE);

  // Update server and plugin I_S table metadata into DD tables.
  else if (dd_init == enum_dd_init_type::DD_UPDATE_I_S_METADATA)
    result= ::bootstrap::run_bootstrap_thread(NULL,
                         &dd::info_schema::update_I_S_metadata,
                         SYSTEM_THREAD_DD_INITIALIZE);

  /* Now that the dd is initialized, delete the cost model. */
  delete_optimizer_cost_module();

  // TODO: See above.
  acl_free(true);
  return result;
}

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::shutdown()
{
  if (!Dictionary_impl::s_instance)
    return true;

  delete Dictionary_impl::s_instance;
  Dictionary_impl::s_instance= NULL;

  return false;
}

///////////////////////////////////////////////////////////////////////////
// Implementation details.
///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::get_target_dd_version()
{ return tables::DD_properties::get_target_dd_version(); }

///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::get_actual_dd_version(THD *thd)
{
  bool not_used;
  return tables::DD_properties::instance().get_actual_dd_version(thd,
                                                                 &not_used);
}

///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::get_actual_dd_version(THD *thd, bool *not_used)
{ return tables::DD_properties::instance().get_actual_dd_version(thd,
                                                                 not_used);
}

///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::get_target_I_S_version()
{ return tables::DD_properties::get_target_I_S_version(); }

///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::get_actual_I_S_version(THD *thd)
{ return tables::DD_properties::instance().get_actual_I_S_version(thd); }

///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::set_I_S_version(THD *thd, uint version)
{
  return const_cast<tables::DD_properties&>(
           tables::DD_properties::instance()).set_I_S_version(thd, version);
}

///////////////////////////////////////////////////////////////////////////

const Object_table *Dictionary_impl::get_dd_table(
  const String_type &schema_name,
  const String_type &table_name) const
{
  if (!is_dd_schema_name(schema_name))
    return NULL;

  return System_tables::instance()->find_table(schema_name, table_name);
}

///////////////////////////////////////////////////////////////////////////

int Dictionary_impl::table_type_error_code(
  const String_type &schema_name,
  const String_type &table_name) const
{
  const System_tables::Types *type= System_tables::instance()->
                                      find_type(schema_name, table_name);
  if (type != nullptr)
    return System_tables::type_name_error_code(*type);
  return ER_NO_SYSTEM_TABLE_ACCESS_FOR_TABLE;
}

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::is_dd_table_access_allowed(
  bool is_dd_internal_thread,
  bool is_ddl_statement,
  const char *schema_name,
  size_t schema_length,
  const char *table_name) const
{
  /*
    From WL#6391, we have the following matrix describing access:

    ---------+---------------------+
             | Dictionary internal |
    ---------+----------+----------+
             |   DDL    |   DML    |
    ---------+-----+----+-----+----+
             | IN  | EX | IN  | EX |
    ---------+-----+----+-----+----+
    Inert    |  X          X       |
    Core     |  X          X       |
    Second   |  X          X       |
    Support  |  X          X    X  |
    ---------+---------------------+

    For performance reasons, we first check the schema
    name to shortcut the evaluation. If the table is not in
    the 'mysql' schema, we don't need any further checks. Same for
    checking for internal threads - an internal thread has full
    access. We also allow access if the appropriate debug flag
    is set.
  */
   /* FIX_ME: NewDD: re-enable this check when mysql-trunk-meta-sync pushes
   to mysql-trunk to mysql-trunk-wl7743-wip-3 */
  if (schema_length != MYSQL_SCHEMA_NAME.length ||
      strncmp(schema_name, MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length) ||
      is_dd_internal_thread ||
      DBUG_EVALUATE_IF("skip_dd_table_access_check", true, false))
    return true;

  // Now we need to get the table type.
  const String_type schema_str(schema_name);
  const String_type table_str(table_name);
  const System_tables::Types *table_type= System_tables::instance()->
                               find_type(schema_str, table_str);

  // Access allowed for external DD tables and for DML on DDSE tables.
  return (table_type == nullptr ||
          (*table_type == System_tables::Types::SUPPORT && !is_ddl_statement));
}

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::is_system_view_name(const char *schema_name,
                                          const char *table_name,
                                          bool *hidden) const
{
  /*
    TODO One possible improvement here could be to try and use the variant
    of is_infoschema_db() that takes length as a parameter. Then, if the
    schema name length is different, this can quickly be used to conclude
    that this is indeed not a system view, without having to do a strcmp at
    all.
  */
  if (schema_name == nullptr ||
      table_name == nullptr ||
      is_infoschema_db(schema_name) == false)
    return false;

  // The System_views registry stores the view name in uppercase.
  // So convert the input to uppercase before search.
  char tab_name_buf[NAME_LEN + 1];
  my_stpcpy(tab_name_buf, table_name);
  my_caseup_str(system_charset_info, tab_name_buf);

  const system_views::System_view *s=
    System_views::instance()->find(INFORMATION_SCHEMA_NAME.str, tab_name_buf);

  if (s)
    *hidden= s->hidden();
  else
    *hidden = false;

  return s != nullptr;
}

///////////////////////////////////////////////////////////////////////////

/*
  Global interface methods at 'dd' namespace.
  Following are couple of API's that InnoDB needs to acquire MDL locks.
*/

static bool acquire_mdl(THD *thd,
                        MDL_key::enum_mdl_namespace lock_namespace,
                        const char *schema_name,
                        const char *table_name,
                        bool no_wait,
                        enum_mdl_type lock_type,
                        enum_mdl_duration lock_duration,
                        MDL_ticket **out_mdl_ticket)
{
  DBUG_ENTER("dd::acquire_mdl");

  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, lock_namespace, schema_name, table_name,
                   lock_type, lock_duration);

  if (no_wait)
  {
    if (thd->mdl_context.try_acquire_lock(&mdl_request))
      DBUG_RETURN(true);
  }
  else if (thd->mdl_context.acquire_lock(&mdl_request,
                                         thd->variables.lock_wait_timeout))
    DBUG_RETURN(true);

  if (out_mdl_ticket)
    *out_mdl_ticket= mdl_request.ticket;

  DBUG_RETURN(false);
}


bool acquire_shared_table_mdl(THD *thd,
                              const char *schema_name,
                              const char *table_name,
                              bool no_wait,
                              MDL_ticket **out_mdl_ticket)
{
  return acquire_mdl(thd, MDL_key::TABLE, schema_name, table_name, no_wait,
                     MDL_SHARED, MDL_EXPLICIT, out_mdl_ticket);
}

bool has_shared_table_mdl(THD *thd,
                          const char *schema_name,
                          const char *table_name)
{
  return thd->mdl_context.owns_equal_or_stronger_lock(
                          MDL_key::TABLE,
                          schema_name,
                          table_name,
                          MDL_SHARED);
}


bool has_exclusive_table_mdl(THD *thd,
                             const char *schema_name,
                             const char *table_name)
{
  return thd->mdl_context.owns_equal_or_stronger_lock(
                          MDL_key::TABLE,
                          schema_name,
                          table_name,
                          MDL_EXCLUSIVE);
}


bool acquire_exclusive_tablespace_mdl(THD *thd,
                                      const char *tablespace_name,
                                      bool no_wait)
{
  // When requesting a tablespace name lock, we leave the schema name empty.
  return acquire_mdl(thd, MDL_key::TABLESPACE, "", tablespace_name, no_wait,
                     MDL_EXCLUSIVE, MDL_TRANSACTION, NULL);
}


bool acquire_shared_tablespace_mdl(THD *thd,
                                   const char *tablespace_name,
                                   bool no_wait)
{
  // When requesting a tablespace name lock, we leave the schema name empty.
  return acquire_mdl(thd, MDL_key::TABLESPACE, "", tablespace_name, no_wait,
                     MDL_SHARED, MDL_TRANSACTION, NULL);
}


bool has_shared_tablespace_mdl(THD *thd,
                               const char *tablespace_name)
{
  // When checking a tablespace name lock, we leave the schema name empty.
  return thd->mdl_context.owns_equal_or_stronger_lock(
                          MDL_key::TABLESPACE,
                          "",
                          tablespace_name,
                          MDL_SHARED);
}


bool has_exclusive_tablespace_mdl(THD *thd,
                                  const char *tablespace_name)
{
  // When checking a tablespace name lock, we leave the schema name empty.
  return thd->mdl_context.owns_equal_or_stronger_lock(
                          MDL_key::TABLESPACE,
                          "",
                          tablespace_name,
                          MDL_EXCLUSIVE);
}

bool acquire_exclusive_table_mdl(THD *thd,
                                 const char *schema_name,
                                 const char *table_name,
                                 bool no_wait,
                                 MDL_ticket **out_mdl_ticket)
{
  return acquire_mdl(thd, MDL_key::TABLE, schema_name, table_name, no_wait,
                           MDL_EXCLUSIVE, MDL_TRANSACTION, out_mdl_ticket);
}

bool acquire_exclusive_schema_mdl(THD *thd,
                                 const char *schema_name,
                                 bool no_wait,
                                 MDL_ticket **out_mdl_ticket)
{
  return acquire_mdl(thd, MDL_key::SCHEMA, schema_name, "", no_wait,
                           MDL_EXCLUSIVE, MDL_EXPLICIT, out_mdl_ticket);
}

void release_mdl(THD *thd, MDL_ticket *mdl_ticket)
{
  DBUG_ENTER("dd::release_mdl");

  thd->mdl_context.release_lock(mdl_ticket);

  DBUG_VOID_RETURN;
}

/* purecov: begin deadcode */
cache::Dictionary_client *get_dd_client(THD *thd)
{ return thd->dd_client(); }
/* purecov: end */

}
