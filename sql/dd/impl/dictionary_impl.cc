/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "auth_common.h"                   // acl_init
#include "bootstrap.h"                     // bootstrap::bootstrap_functor
#include "opt_costconstantcache.h"         // init_optimizer_cost_module
#include "sql_class.h"                     // THD

#include "dd/iterator.h"                   // dd::Iterator
#include "dd/cache/dictionary_client.h"    // dd::Dictionary_client
#include "dd/impl/bootstrapper.h"          // dd::Bootstrapper
#include "dd/impl/system_registry.h"       // dd::System_tables
#include "dd/impl/tables/version.h"        // get_actual_dd_version()

///////////////////////////////////////////////////////////////////////////

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Implementation details.
///////////////////////////////////////////////////////////////////////////

Dictionary_impl *Dictionary_impl::s_instance= NULL;

Object_id Dictionary_impl::DEFAULT_CATALOG_ID= 1;
const std::string Dictionary_impl::DEFAULT_CATALOG_NAME("def");

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::init(bool install)
{
  DBUG_ASSERT(!Dictionary_impl::s_instance);

  if (Dictionary_impl::s_instance)
    return false; /* purecov: inspected */

  std::unique_ptr<Dictionary_impl> d(new Dictionary_impl());

  Dictionary_impl::s_instance= d.release();

  // TODO: We need to do basic ACL initialization to get a working LOCK_grant.
  // We should instead rethink the order stuff happens during server start.
#ifndef EMBEDDED_LIBRARY
  acl_init(true);
#endif

  /*
    Initialize the cost model, but delete it after the dd is initialized.
    This is because the cost model is needed for the dd initialization, but
    it must be re-initialized later after the plugins have been initialized.
  */
  init_optimizer_cost_module(false);

  /* Install or start the dictionary depending on bootstrapping option */
  bool result= false;
  if (install)
    result= ::bootstrap::run_bootstrap_thread(NULL, &bootstrap::initialize,
                                              SYSTEM_THREAD_DD_INITIALIZE);
  else
    result= ::bootstrap::run_bootstrap_thread(NULL, &bootstrap::restart,
                                              SYSTEM_THREAD_DD_RESTART);

  /* Now that the dd is initialized, delete the cost model. */
  delete_optimizer_cost_module();

  // TODO: See above.
#ifndef EMBEDDED_LIBRARY
  acl_free(true);
#endif
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
{ return dd::tables::Version::get_target_dd_version(); }

///////////////////////////////////////////////////////////////////////////

uint Dictionary_impl::get_actual_dd_version(THD *thd)
{ return dd::tables::Version::instance().get_actual_dd_version(thd); }

///////////////////////////////////////////////////////////////////////////

const Object_table *Dictionary_impl::get_dd_table(
  const std::string &schema_name,
  const std::string &table_name) const
{
  if (!is_dd_schema_name(schema_name))
    return NULL;

  return System_tables::instance()->find(schema_name, table_name);
}

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::is_system_view_name(const std::string &schema_name,
                                          const std::string &table_name) const
{
  if (schema_name.compare(INFORMATION_SCHEMA_NAME.str) != 0)
    return false;

  return (System_views::instance()->find(schema_name, table_name) != NULL);
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
