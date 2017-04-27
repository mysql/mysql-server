/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/service_plugin_registry.h>
#include "server_component.h" // imp_mysql_server_registry

/**
   Returns a new reference to the "registry" service.

   Implementation of
   @ref plugin_registry_service_st::mysql_plugin_registry_acquire_func

   Uses the global registry instance to search for the default implementation
   of the "registry" service by calling
   @ref mysql_service_registry_t::acquire().

   The reference must be released through calling
   @ref mysql_plugin_registry_release() to avoid resource leaks.

   @return The newly acquired registry service pointer.
   @retval NULL error. @ref mysql_service_registry_t::acquire() failed.

   See also @ref mysql_plugin_registry_release(),
   @ref PAGE_COMPONENTS, @ref PAGE_COMPONENTS_REGISTRY,
   @ref mysql_service_registry_t::acquire()
*/
SERVICE_TYPE(registry) * mysql_plugin_registry_acquire()
{
  my_h_service registry_handle;
  if (imp_mysql_server_registry.acquire(
    "registry", &registry_handle))
  {
    return nullptr;
  }
  return reinterpret_cast<SERVICE_TYPE(registry) *>(registry_handle);
}


/**
  Releases a registry service reference

  Implementation of
  @ref plugin_registry_service_st::mysql_plugin_registry_release_func

  Uses the global registry instance to release a service reference to
  the "registry" service passed as a parameter by calling
  @ref mysql_service_registry_t::release()
  This is the reverse of mysql_plugin_registry_acquire().

  @param reg the registry service handle to release
  @return the result of mysql_service_registry_t::release()
  @retval 0 Success
  @retval non-zero Failure

  See also @ref mysql_plugin_registry_release(),
  @ref PAGE_COMPONENTS, @ref PAGE_COMPONENTS_REGISTRY,
  @ref mysql_service_registry_t::release()
*/
int mysql_plugin_registry_release(SERVICE_TYPE(registry) *reg)
{
  return imp_mysql_server_registry.release((my_h_service) reg);
}
