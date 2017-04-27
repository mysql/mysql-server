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

#include <my_sys.h>                         // my_error
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <stddef.h>
#include <new>
#include <stdexcept>                        // std::exception subclasses

#include "dynamic_loader.h"
#include "dynamic_loader_path_filter.h"
#include "dynamic_loader_scheme_file.h"
#include "my_inttypes.h"
#include "mysqld_error.h"
#include "persistent_dynamic_loader.h"
#include "mysql_string_service.h"
#include "registry.h"
#include "server_component.h"
#include "auth/dynamic_privileges_impl.h"

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry)
  mysql_registry_imp::acquire,
  mysql_registry_imp::acquire_related,
  mysql_registry_imp::release
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_registration)
  mysql_registry_imp::register_service,
  mysql_registry_imp::unregister,
  mysql_registry_imp::set_default
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_query)
  mysql_registry_imp::iterator_create,
  mysql_registry_imp::iterator_get,
  mysql_registry_imp::iterator_next,
  mysql_registry_imp::iterator_is_valid,
  mysql_registry_imp::iterator_release,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_metadata_enumerate)
  mysql_registry_imp::metadata_iterator_create,
  mysql_registry_imp::metadata_iterator_get,
  mysql_registry_imp::metadata_iterator_next,
  mysql_registry_imp::metadata_iterator_is_valid,
  mysql_registry_imp::metadata_iterator_release,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_metadata_query)
  mysql_registry_imp::metadata_get_value,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader)
  mysql_dynamic_loader_imp::load,
  mysql_dynamic_loader_imp::unload
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_query)
  mysql_dynamic_loader_imp::iterator_create,
  mysql_dynamic_loader_imp::iterator_get,
  mysql_dynamic_loader_imp::iterator_next,
  mysql_dynamic_loader_imp::iterator_is_valid,
  mysql_dynamic_loader_imp::iterator_release
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_metadata_enumerate)
  mysql_dynamic_loader_imp::metadata_iterator_create,
  mysql_dynamic_loader_imp::metadata_iterator_get,
  mysql_dynamic_loader_imp::metadata_iterator_next,
  mysql_dynamic_loader_imp::metadata_iterator_is_valid,
  mysql_dynamic_loader_imp::metadata_iterator_release
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_metadata_query)
  mysql_dynamic_loader_imp::metadata_get_value,
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(
  mysql_server_path_filter, dynamic_loader_scheme_file)
  mysql_dynamic_loader_scheme_file_path_filter_imp::load,
  mysql_dynamic_loader_scheme_file_path_filter_imp::unload
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_scheme_file)
  mysql_dynamic_loader_scheme_file_imp::load,
  mysql_dynamic_loader_scheme_file_imp::unload
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, persistent_dynamic_loader)
  mysql_persistent_dynamic_loader_imp::load,
  mysql_persistent_dynamic_loader_imp::unload
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_privilege_register)
  dynamic_privilege_services_impl::register_privilege,
  dynamic_privilege_services_impl::unregister_privilege
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, global_grants_check)
  dynamic_privilege_services_impl::has_global_grant
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_factory)
  mysql_string_imp::create,
  mysql_string_imp::destroy
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_case)
  mysql_string_imp::tolower,
  mysql_string_imp::toupper
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_converter)
  mysql_string_imp::convert_from_buffer,
  mysql_string_imp::convert_to_buffer
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_character_access)
  mysql_string_imp::get_char,
  mysql_string_imp::get_char_length
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_byte_access)
  mysql_string_imp::get_byte,
  mysql_string_imp::get_byte_length
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_iterator)
  mysql_string_imp::iterator_create,
  mysql_string_imp::iterator_get_next,
  mysql_string_imp::iterator_destroy
END_SERVICE_IMPLEMENTATION()

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_ctype)
  mysql_string_imp::is_upper,
  mysql_string_imp::is_lower,
  mysql_string_imp::is_digit
END_SERVICE_IMPLEMENTATION()

BEGIN_COMPONENT_PROVIDES(mysql_server)
  PROVIDES_SERVICE(mysql_server, registry)
  PROVIDES_SERVICE(mysql_server, registry_registration)
  PROVIDES_SERVICE(mysql_server, registry_query)
  PROVIDES_SERVICE(mysql_server, registry_metadata_enumerate)
  PROVIDES_SERVICE(mysql_server, registry_metadata_query)
  PROVIDES_SERVICE(mysql_server, dynamic_loader)
  PROVIDES_SERVICE(mysql_server_path_filter, dynamic_loader_scheme_file)
  PROVIDES_SERVICE(mysql_server, persistent_dynamic_loader)
  PROVIDES_SERVICE(mysql_server, dynamic_loader_query)
  PROVIDES_SERVICE(mysql_server, dynamic_loader_metadata_enumerate)
  PROVIDES_SERVICE(mysql_server, dynamic_loader_metadata_query)
  PROVIDES_SERVICE(mysql_server, dynamic_loader_scheme_file)
  PROVIDES_SERVICE(mysql_server, dynamic_privilege_register)
  PROVIDES_SERVICE(mysql_server, global_grants_check)
  PROVIDES_SERVICE(mysql_server, mysql_string_factory)
  PROVIDES_SERVICE(mysql_server, mysql_string_case)
  PROVIDES_SERVICE(mysql_server, mysql_string_converter)
  PROVIDES_SERVICE(mysql_server, mysql_string_character_access)
  PROVIDES_SERVICE(mysql_server, mysql_string_byte_access)
  PROVIDES_SERVICE(mysql_server, mysql_string_iterator)
  PROVIDES_SERVICE(mysql_server, mysql_string_ctype)
END_COMPONENT_PROVIDES()

static BEGIN_COMPONENT_REQUIRES(mysql_server)
END_COMPONENT_REQUIRES()

BEGIN_COMPONENT_METADATA(mysql_server)
  METADATA("mysql.author", "Oracle Corporation")
  METADATA("mysql.license", "GPL")
END_COMPONENT_METADATA()

DECLARE_COMPONENT(mysql_server, "mysql:core")
  /* There are no initialization/deinitialization functions, they will not be
    called as this component is not a regular one. */
  NULL,
  NULL
END_DECLARE_COMPONENT()

/**
  Bootstraps service registry and dynamic loader and make ready all basic
  server services.

  @param [out] registry A service handle to registry service. May be NULL.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_services_bootstrap(SERVICE_TYPE(registry)** registry)
{
  /* Create the registry service suite internal structure mysql_registry. */
  registry_init();

  /* Seed the registry through registering the registry implementation into it,
    as well as other main bootstrap dynamic loader service implementations. */
  for (int inx= 0;
    mysql_component_mysql_server.provides[inx].implementation != NULL;
    ++inx)
  {
    if (imp_mysql_server_registry_registration.register_service(
      mysql_component_mysql_server.provides[inx].name,
      reinterpret_cast<my_h_service>(
        mysql_component_mysql_server.provides[inx].implementation)))
    {
      return true;
    }
  }

  if (registry != NULL)
  {
    my_h_service registry_handle;
    if (imp_mysql_server_registry.acquire(
      "registry.mysql_server", &registry_handle))
    {
      return true;
    }
    *registry= reinterpret_cast<SERVICE_TYPE(registry)*>(registry_handle);
  }

  dynamic_loader_init();
  dynamic_loader_scheme_file_init();

  my_service<SERVICE_TYPE(registry_registration)> registrator(
    "registry_registration", &imp_mysql_server_registry);

  // Sets default file scheme loader for MySQL server.
  registrator->set_default(
    "dynamic_loader_scheme_file.mysql_server_path_filter");

  return false;
}

/**
  Shutdowns service registry and dynamic loader making sure all basic services
  are unregistered. Will fail if any service implementation is in use.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_services_shutdown()
{
  /* Dynamic loader deinitialization still needs all scheme service
    implementations to be functional. */
  dynamic_loader_deinit();
  dynamic_loader_scheme_file_deinit();

  for (int inx= 0;
    mysql_component_mysql_server.provides[inx].implementation != NULL;
    ++inx)
  {
    if (imp_mysql_server_registry_registration.unregister(
      mysql_component_mysql_server.provides[inx].name))
    {
      return true;
    }
  }
  registry_deinit();
  return false;
}

/**
  Checks if last thrown exception is any kind of standard exceptions, i.e. the
  exceptions inheriting from std::exception. If so, reports an error message
  that states exception type and message. On any other thrown value it just
  reports general error.
*/
void mysql_components_handle_std_exception(const char *funcname)
{
  try
  {
    throw;
  }
  catch (const std::bad_alloc &e)
  {
    my_error(ER_STD_BAD_ALLOC_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::domain_error &e)
  {
    my_error(ER_STD_DOMAIN_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::length_error &e)
  {
    my_error(ER_STD_LENGTH_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::invalid_argument &e)
  {
    my_error(ER_STD_INVALID_ARGUMENT, MYF(0), e.what(), funcname);
  }
  catch (const std::out_of_range &e)
  {
    my_error(ER_STD_OUT_OF_RANGE_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::overflow_error &e)
  {
    my_error(ER_STD_OVERFLOW_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::range_error &e)
  {
    my_error(ER_STD_RANGE_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::underflow_error &e)
  {
    my_error(ER_STD_UNDERFLOW_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::logic_error &e)
  {
    my_error(ER_STD_LOGIC_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::runtime_error &e)
  {
    my_error(ER_STD_RUNTIME_ERROR, MYF(0), e.what(), funcname);
  }
  catch (const std::exception &e)
  {
    my_error(ER_STD_UNKNOWN_EXCEPTION, MYF(0), e.what(), funcname);
  }
  catch (...)
  {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
  }
}
