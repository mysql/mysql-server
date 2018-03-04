/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/dynamic_loader.h>
#include <mysql/components/services/dynamic_loader_scheme_file.h>
#include <string.h>
#include <map>
#include <string>
#include <unordered_set>

#include "dynamic_loader_scheme_file.h"
#include "my_psi_config.h"
#include "rwlock_scoped_lock.h"
#include "scope_guard.h"
#include "server_component.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

typedef std::string my_string;

static PSI_rwlock_key key_rwlock_LOCK_dynamic_loader_scheme_file;

/**
  Initializes file:// scheme for dynamic loader for usage. Initializes
  RW lock, all other structures should be empty. Shouldn't be called multiple
  times.
*/
void mysql_dynamic_loader_scheme_file_imp::init()
{
  mysql_rwlock_init(key_rwlock_LOCK_dynamic_loader_scheme_file,
    &mysql_dynamic_loader_scheme_file_imp::LOCK_dynamic_loader_scheme_file);
}
/**
  De-initializes RW lock, all other structures doesn't require any action.
*/
void mysql_dynamic_loader_scheme_file_imp::deinit()
{
  mysql_rwlock_destroy(
    &mysql_dynamic_loader_scheme_file_imp::LOCK_dynamic_loader_scheme_file);
}

/**
  Loads components that are located in executable file specified by URN.
  We assume that URN starts with file://, but accept any. Will not success
  when called multiple times on the same file.

  @param urn URN to file to load components from.
  @param [out] out_data Pointer to pointer to MySQL component data structures
    to set result components data retrieved from specified file.
  @return Status of performed operation
  @retval false success
  @retval true Failure, may be caused when name does not contain ://, cannot
    be located, is not proper executable file or does not contain proper
    initialization function.
*/
DEFINE_BOOL_METHOD(mysql_dynamic_loader_scheme_file_imp::load,
  (const char *urn, mysql_component_t** out_data))
{
  try
  {
    if (urn == NULL)
    {
      return true;
    }

    my_string urn_string= urn;

    /* Check if library is not already loaded, by comparing URNs. */
    rwlock_scoped_lock lock(
      &mysql_dynamic_loader_scheme_file_imp::LOCK_dynamic_loader_scheme_file,
      true, __FILE__, __LINE__);

    if (object_files_list.find(urn_string) != object_files_list.end())
    {
      return true;
    }

    /* Omit scheme prefix to get filename. */
    const char* file= strstr(urn, "://");
    if (file == NULL)
    {
      return true;
    }
    /* Offset by "://" */
    file+= 3;
    my_string file_name =
      my_string(file);
#ifdef _WIN32
    file_name+= ".dll";
#else
    file_name+= ".so";
#endif

    /* Open library. */
    void* handle= dlopen(file_name.c_str(), RTLD_NOW);
    if (handle == NULL)
    {
      return true;
    }
    auto guard_library= create_scope_guard([&handle]()
    {
      /* In case we need to rollback we close the opened library. */
      dlclose(handle);
    });

    /* Look for "list_components" function. */
    list_components_func list_func=
      reinterpret_cast<list_components_func>(
        dlsym(handle, "list_components"));
    if (list_func == NULL)
    {
      return true;
    }

    /* Check if library is not already loaded, by comparing "list_components"
      function address. */
    if (library_entry_set.insert(list_func).second == false)
    {
      return true;
    }

    auto guard_library_set= create_scope_guard([&list_func]()
    {
      /* In case we need to rollback we remove library handle from set. */
      library_entry_set.erase(list_func);
    });

    /* Get components data from library. */
    *out_data= list_func();

    /* Add library and it's handle to list of loaded libraries. */

    if (object_files_list.emplace(urn_string, handle).second == false)
    {
      return true;
    }

    guard_library.commit();
    guard_library_set.commit();

    return false;
  }
  catch (...)
  {
  }
  return true;
}

/**
  Unloads file that was previously loaded. The URN string must be exactly
  the same as one used during call to load. Although you can call load() on
  specified URN multiple times, subsequent calls unload() will always fail,
  and all components from specified file will be invalid after first call to
  unload().

  @param urn URN to file to unload all components from.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(mysql_dynamic_loader_scheme_file_imp::unload,
  (const char *urn))
{
  try
  {
    /* Find library matching URN specified. */
    rwlock_scoped_lock lock(
      &mysql_dynamic_loader_scheme_file_imp::LOCK_dynamic_loader_scheme_file,
      true, __FILE__, __LINE__);

    my_registry::iterator it= object_files_list.find(my_string(urn));
    if (it == object_files_list.end())
    {
      return true;
    }

    /* Delete entry from library entry points list. */
    list_components_func list_func=
      reinterpret_cast<list_components_func>(
        dlsym(it->second, "list_components"));
    library_entry_set.erase(list_func);

    /* Close library and delete entry from libraries list. */
    dlclose(it->second);
    object_files_list.erase(it);
    return false;
  }
  catch (...)
  {
  }
  return true;
}

/* static members for mysql_dynamic_loader_scheme_file_imp */
mysql_dynamic_loader_scheme_file_imp::my_registry
  mysql_dynamic_loader_scheme_file_imp::object_files_list;
std::unordered_set<mysql_dynamic_loader_scheme_file_imp::list_components_func>
  mysql_dynamic_loader_scheme_file_imp::library_entry_set;
mysql_rwlock_t
  mysql_dynamic_loader_scheme_file_imp::LOCK_dynamic_loader_scheme_file;

/* Following code initialize and deinitialize service implementations by
  managing RW locks and their PSI augmentation. */

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_info all_dynamic_loader_scheme_file_rwlocks[]=
{
  { &key_rwlock_LOCK_dynamic_loader_scheme_file,
    "LOCK_dynamic_loader_scheme_file", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}
};


static void init_dynamic_loader_scheme_file_psi_keys(void)
{
  const char *category= "components";
  int count;

  count=
    static_cast<int>(array_elements(all_dynamic_loader_scheme_file_rwlocks));
  mysql_rwlock_register(
    category, all_dynamic_loader_scheme_file_rwlocks, count);
}
#endif

void dynamic_loader_scheme_file_init()
{
#ifdef HAVE_PSI_INTERFACE
  init_dynamic_loader_scheme_file_psi_keys();
#endif
  mysql_dynamic_loader_scheme_file_imp::init();
}


void dynamic_loader_scheme_file_deinit()
{
  mysql_dynamic_loader_scheme_file_imp::deinit();
}
