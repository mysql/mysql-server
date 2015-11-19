/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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
#include "dd/impl/object_table_registry.h" // dd::Object_table_registry
#include "dd/impl/system_view_name_registry.h"  // dd::System_view_name_registry

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

  /* Install or start the dictionary depending on bootstrapping option */
  bootstrap::bootstrap_functor boot_handler= &Bootstrapper::start;
  if (install)
    boot_handler= &Bootstrapper::install;

  /*
    Initialize the cost model, but delete it after the dd is initialized.
    This is because the cost model is needed for the dd initialization, but
    it must be re-initialized later after the plugins have been initialized.
  */
  init_optimizer_cost_module(false);

  bool result= bootstrap::run_bootstrap_thread(NULL, boot_handler);

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

const Object_table *Dictionary_impl::get_dd_table(
  const std::string &schema_name,
  const std::string &table_name) const
{
  if (!is_dd_schema_name(schema_name))
    return NULL;

  std::unique_ptr<Iterator<const Object_table> > it(
    Object_table_registry::instance()->types());

  for (const Object_table *t= it->next(); t != NULL; t= it->next())
    if (table_name == t->name())
      return t;

  return NULL;
}

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::is_system_view_name(const std::string &schema_name,
                                          const std::string &table_name) const
{
  if (schema_name.compare("information_schema") != 0)
    return false;

  std::unique_ptr<Iterator<const char> > it(
    System_view_name_registry::instance()->names());

  while (true)
  {
    const char *name= it->next();

    if (!name)
      break;

    if (table_name.compare(name) == 0)
      return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Dictionary_impl::load_and_cache_server_collation(THD *thd)
{
  // Build object and cache it.
  // This object will be released and destroyed when Dictionary object dies.
  const dd::Collation *obj= NULL;
  if (thd->dd_client()->acquire<dd::Collation>(default_charset_info->name, &obj))
    return true;

  DBUG_ASSERT(obj);
  thd->dd_client()->set_sticky(obj, true);

  m_server_collation= obj;

  return false;
}


///////////////////////////////////////////////////////////////////////////

/*
  Global interface methods at 'dd' namespace.
  Following are couple of API's that InnoDB needs to acquire MDL locks.
*/

static bool acquire_table_mdl(THD *thd,
                              const char *schema_name,
                              const char *table_name,
                              bool no_wait,
                              enum_mdl_type lock_type,
                              MDL_ticket **out_mdl_ticket)
{
  DBUG_ENTER("dd::acquire_table_mdl");

  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::TABLE, schema_name, table_name,
                   lock_type, MDL_EXPLICIT);

  if (no_wait)
  {
    if (thd->mdl_context.try_acquire_lock(&mdl_request))
      DBUG_RETURN(true);
  }
  else if (thd->mdl_context.acquire_lock(&mdl_request,
                                         thd->variables.lock_wait_timeout))
    DBUG_RETURN(true);


  *out_mdl_ticket= mdl_request.ticket;

  DBUG_RETURN(false);
}

bool acquire_shared_table_mdl(THD *thd,
                              const char *schema_name,
                              const char *table_name,
                              bool no_wait,
                              MDL_ticket **out_mdl_ticket)
{
  return acquire_table_mdl(thd, schema_name, table_name, no_wait,
                           MDL_SHARED, out_mdl_ticket);
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
