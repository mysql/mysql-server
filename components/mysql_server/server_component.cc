/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <mysql/components/my_service.h>
#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/components/services/mysql_mutex_service.h>
#include <mysql/components/services/mysql_rwlock_service.h>
#include <stddef.h>
#include <new>
#include <stdexcept>  // std::exception subclasses

#include "component_status_var_service.h"
#include "component_sys_var_service.h"
#include "dynamic_loader.h"
#include "dynamic_loader_path_filter.h"
#include "dynamic_loader_scheme_file.h"
#include "host_application_signal_imp.h"
#include "log_builtins_filter_imp.h"
#include "log_builtins_imp.h"
#include "my_inttypes.h"
#include "my_sys.h"  // my_error
#include "mysql_backup_lock.h"
#include "mysql_clone_protocol.h"
#include "mysql_ongoing_transaction_query.h"
#include "mysql_string_service.h"
#include "mysqld_error.h"
#include "persistent_dynamic_loader.h"
#include "registry.h"
#include "security_context_imp.h"
#include "server_component.h"
#include "sql/auth/dynamic_privileges_impl.h"
#include "sql/log.h"
#include "sql/udf_registration_imp.h"
#include "system_variable_source_imp.h"

// Must come after sql/log.h.
#include "mysql/components/services/log_builtins.h"

/* Implementation located in the mysql_server component. */
extern SERVICE_TYPE(mysql_cond_v1)
    SERVICE_IMPLEMENTATION(mysql_server, mysql_cond_v1);
extern SERVICE_TYPE(mysql_mutex_v1)
    SERVICE_IMPLEMENTATION(mysql_server, mysql_mutex_v1);
extern SERVICE_TYPE(mysql_rwlock_v1)
    SERVICE_IMPLEMENTATION(mysql_server, mysql_rwlock_v1);

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry)
mysql_registry_imp::acquire, mysql_registry_imp::acquire_related,
    mysql_registry_imp::release END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_registration)
mysql_registry_imp::register_service, mysql_registry_imp::unregister,
    mysql_registry_imp::set_default END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_query)
mysql_registry_imp::iterator_create, mysql_registry_imp::iterator_get,
    mysql_registry_imp::iterator_next, mysql_registry_imp::iterator_is_valid,
    mysql_registry_imp::iterator_release, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_metadata_enumerate)
mysql_registry_imp::metadata_iterator_create,
    mysql_registry_imp::metadata_iterator_get,
    mysql_registry_imp::metadata_iterator_next,
    mysql_registry_imp::metadata_iterator_is_valid,
    mysql_registry_imp::metadata_iterator_release, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, registry_metadata_query)
mysql_registry_imp::metadata_get_value, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader)
mysql_dynamic_loader_imp::load,
    mysql_dynamic_loader_imp::unload END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_query)
mysql_dynamic_loader_imp::iterator_create,
    mysql_dynamic_loader_imp::iterator_get,
    mysql_dynamic_loader_imp::iterator_next,
    mysql_dynamic_loader_imp::iterator_is_valid,
    mysql_dynamic_loader_imp::iterator_release END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_metadata_enumerate)
mysql_dynamic_loader_imp::metadata_iterator_create,
    mysql_dynamic_loader_imp::metadata_iterator_get,
    mysql_dynamic_loader_imp::metadata_iterator_next,
    mysql_dynamic_loader_imp::metadata_iterator_is_valid,
    mysql_dynamic_loader_imp::metadata_iterator_release
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_metadata_query)
mysql_dynamic_loader_imp::metadata_get_value, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server_path_filter,
                             dynamic_loader_scheme_file)
mysql_dynamic_loader_scheme_file_path_filter_imp::load,
    mysql_dynamic_loader_scheme_file_path_filter_imp::unload
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_loader_scheme_file)
mysql_dynamic_loader_scheme_file_imp::load,
    mysql_dynamic_loader_scheme_file_imp::unload END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, persistent_dynamic_loader)
mysql_persistent_dynamic_loader_imp::load,
    mysql_persistent_dynamic_loader_imp::unload END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, dynamic_privilege_register)
dynamic_privilege_services_impl::register_privilege,
    dynamic_privilege_services_impl::unregister_privilege
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, global_grants_check)
dynamic_privilege_services_impl::has_global_grant END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_factory)
mysql_string_imp::create,
    mysql_string_imp::destroy END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_case)
mysql_string_imp::tolower,
    mysql_string_imp::toupper END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_converter)
mysql_string_imp::convert_from_buffer,
    mysql_string_imp::convert_to_buffer END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_character_access)
mysql_string_imp::get_char,
    mysql_string_imp::get_char_length END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_byte_access)
mysql_string_imp::get_byte,
    mysql_string_imp::get_byte_length END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_iterator)
mysql_string_imp::iterator_create, mysql_string_imp::iterator_get_next,
    mysql_string_imp::iterator_destroy END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_string_ctype)
mysql_string_imp::is_upper, mysql_string_imp::is_lower,
    mysql_string_imp::is_digit END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, log_builtins)
log_builtins_imp::wellknown_by_type, log_builtins_imp::wellknown_by_name,
    log_builtins_imp::wellknown_get_type, log_builtins_imp::wellknown_get_name,

    log_builtins_imp::item_inconsistent, log_builtins_imp::item_generic_type,
    log_builtins_imp::item_string_class, log_builtins_imp::item_numeric_class,

    log_builtins_imp::item_set_int, log_builtins_imp::item_set_float,
    log_builtins_imp::item_set_lexstring, log_builtins_imp::item_set_cstring,

    log_builtins_imp::item_set_with_key, log_builtins_imp::item_set,

    log_builtins_imp::line_item_set_with_key, log_builtins_imp::line_item_set,

    log_builtins_imp::line_init, log_builtins_imp::line_exit,
    log_builtins_imp::line_item_count,

    log_builtins_imp::line_item_types_seen,

    log_builtins_imp::line_item_iter_acquire,
    log_builtins_imp::line_item_iter_release,
    log_builtins_imp::line_item_iter_first,
    log_builtins_imp::line_item_iter_next,
    log_builtins_imp::line_item_iter_current,

    log_builtins_imp::line_submit,

    log_builtins_imp::message,

    log_builtins_imp::sanitize,

    log_builtins_imp::errmsg_by_errcode, log_builtins_imp::errcode_by_errsymbol,

    log_builtins_imp::label_from_prio,

    log_builtins_imp::open_errstream, log_builtins_imp::write_errstream,
    log_builtins_imp::dedicated_errstream,
    log_builtins_imp::close_errstream END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, log_builtins_filter)
log_builtins_filter_imp::filter_ruleset_new,
    log_builtins_filter_imp::filter_ruleset_lock,
    log_builtins_filter_imp::filter_ruleset_unlock,
    log_builtins_filter_imp::filter_ruleset_drop,
    log_builtins_filter_imp::filter_ruleset_free,
    log_builtins_filter_imp::filter_ruleset_move,
    log_builtins_filter_imp::filter_rule_init,
    log_builtins_filter_imp::filter_run END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, log_builtins_filter_debug)
log_builtins_filter_debug_imp::filter_debug_ruleset_get
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, log_builtins_string)
log_builtins_string_imp::malloc, log_builtins_string_imp::strndup,
    log_builtins_string_imp::free,

    log_builtins_string_imp::length, log_builtins_string_imp::find_first,
    log_builtins_string_imp::find_last,

    log_builtins_string_imp::compare,

    log_builtins_string_imp::substitutev,
    log_builtins_string_imp::substitute END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, log_builtins_tmp)
log_builtins_tmp_imp::notify_client END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, log_builtins_syseventlog)
log_builtins_syseventlog_imp::open, log_builtins_syseventlog_imp::write,
    log_builtins_syseventlog_imp::close END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, udf_registration)
mysql_udf_registration_imp::udf_register,
    mysql_udf_registration_imp::udf_unregister END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, udf_registration_aggregate)
mysql_udf_registration_imp::udf_register_aggregate,
    mysql_udf_registration_imp::udf_unregister END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, component_sys_variable_register)
mysql_component_sys_variable_imp::register_variable,
    mysql_component_sys_variable_imp::get_variable END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, component_sys_variable_unregister)
mysql_component_sys_variable_imp::unregister_variable,
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, status_variable_registration)
mysql_status_variable_registration_imp::register_variable,
    mysql_status_variable_registration_imp::unregister_variable
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, system_variable_source)
mysql_system_variable_source_imp::get END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_backup_lock)
mysql_acquire_backup_lock,
    mysql_release_backup_lock END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, clone_protocol)
mysql_clone_start_statement, mysql_clone_finish_statement, mysql_clone_connect,
    mysql_clone_send_command, mysql_clone_get_response, mysql_clone_kill,
    mysql_clone_disconnect, mysql_clone_get_command, mysql_clone_send_response,
    mysql_clone_send_error END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_thd_security_context)
mysql_security_context_imp::get,
    mysql_security_context_imp::set END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_security_context_factory)
mysql_security_context_imp::create, mysql_security_context_imp::destroy,
    mysql_security_context_imp::copy END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server,
                             mysql_account_database_security_context_lookup)
mysql_security_context_imp::lookup END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_security_context_options)
mysql_security_context_imp::get,
    mysql_security_context_imp::set END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_ongoing_transactions_query)
mysql_ongoing_transactions_query_imp::get_ongoing_server_transactions
END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, host_application_signal)
mysql_component_host_application_signal_imp::signal
END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(mysql_server)
PROVIDES_SERVICE(mysql_server, registry),
    PROVIDES_SERVICE(mysql_server, registry_registration),
    PROVIDES_SERVICE(mysql_server, registry_query),
    PROVIDES_SERVICE(mysql_server, registry_metadata_enumerate),
    PROVIDES_SERVICE(mysql_server, registry_metadata_query),
    PROVIDES_SERVICE(mysql_server, dynamic_loader),
    PROVIDES_SERVICE(mysql_server_path_filter, dynamic_loader_scheme_file),
    PROVIDES_SERVICE(mysql_server, persistent_dynamic_loader),
    PROVIDES_SERVICE(mysql_server, dynamic_loader_query),
    PROVIDES_SERVICE(mysql_server, dynamic_loader_metadata_enumerate),
    PROVIDES_SERVICE(mysql_server, dynamic_loader_metadata_query),
    PROVIDES_SERVICE(mysql_server, dynamic_loader_scheme_file),
    PROVIDES_SERVICE(mysql_server, dynamic_privilege_register),
    PROVIDES_SERVICE(mysql_server, global_grants_check),
    PROVIDES_SERVICE(mysql_server, mysql_string_factory),
    PROVIDES_SERVICE(mysql_server, mysql_string_case),
    PROVIDES_SERVICE(mysql_server, mysql_string_converter),
    PROVIDES_SERVICE(mysql_server, mysql_string_character_access),
    PROVIDES_SERVICE(mysql_server, mysql_string_byte_access),
    PROVIDES_SERVICE(mysql_server, mysql_string_iterator),
    PROVIDES_SERVICE(mysql_server, mysql_string_ctype),
    PROVIDES_SERVICE(mysql_server, log_builtins),
    PROVIDES_SERVICE(mysql_server, log_builtins_filter),
    PROVIDES_SERVICE(mysql_server, log_builtins_filter_debug),
    PROVIDES_SERVICE(mysql_server, log_builtins_string),
    PROVIDES_SERVICE(mysql_server, log_builtins_tmp),
    PROVIDES_SERVICE(mysql_server, log_builtins_syseventlog),
    PROVIDES_SERVICE(mysql_server, udf_registration),
    PROVIDES_SERVICE(mysql_server, udf_registration_aggregate),
    PROVIDES_SERVICE(mysql_server, component_sys_variable_register),
    PROVIDES_SERVICE(mysql_server, component_sys_variable_unregister),
    PROVIDES_SERVICE(mysql_server, mysql_cond_v1),
    PROVIDES_SERVICE(mysql_server, mysql_mutex_v1),
    PROVIDES_SERVICE(mysql_server, mysql_rwlock_v1),
    PROVIDES_SERVICE(mysql_server, status_variable_registration),
    PROVIDES_SERVICE(mysql_server, system_variable_source),
    PROVIDES_SERVICE(mysql_server, mysql_backup_lock),
    PROVIDES_SERVICE(mysql_server, clone_protocol),
    PROVIDES_SERVICE(mysql_server, mysql_thd_security_context),
    PROVIDES_SERVICE(mysql_server, mysql_security_context_factory),
    PROVIDES_SERVICE(mysql_server,
                     mysql_account_database_security_context_lookup),
    PROVIDES_SERVICE(mysql_server, mysql_security_context_options),
    PROVIDES_SERVICE(mysql_server, mysql_ongoing_transactions_query),
    PROVIDES_SERVICE(mysql_server, host_application_signal),
    END_COMPONENT_PROVIDES();

static BEGIN_COMPONENT_REQUIRES(mysql_server) END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(mysql_server)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(mysql_server, "mysql:core")
/* There are no initialization/deinitialization functions, they will not be
  called as this component is not a regular one. */
NULL, NULL END_DECLARE_COMPONENT();

/**
  Bootstraps service registry and dynamic loader and make ready all basic
  server services.

  @param [out] registry A service handle to registry service. May be NULL.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_services_bootstrap(SERVICE_TYPE(registry) * *registry) {
  /* Create the registry service suite internal structure mysql_registry. */
  registry_init();

  /* Seed the registry through registering the registry implementation into it,
    as well as other main bootstrap dynamic loader service implementations. */
  for (int inx = 0;
       mysql_component_mysql_server.provides[inx].implementation != NULL;
       ++inx) {
    if (imp_mysql_server_registry_registration.register_service(
            mysql_component_mysql_server.provides[inx].name,
            reinterpret_cast<my_h_service>(
                mysql_component_mysql_server.provides[inx].implementation))) {
      return true;
    }
  }

  if (registry != NULL) {
    my_h_service registry_handle;
    if (imp_mysql_server_registry.acquire("registry.mysql_server",
                                          &registry_handle)) {
      return true;
    }
    *registry = reinterpret_cast<SERVICE_TYPE(registry) *>(registry_handle);
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
  Shutdowns dynamic loader.
*/
void shutdown_dynamic_loader() {
  /* Dynamic loader deinitialization still needs all scheme service
    implementations to be functional. */
  dynamic_loader_deinit();
  dynamic_loader_scheme_file_deinit();
}

/**
  Shutdowns service registry making sure all basic services are unregistered.
  Will fail if any service implementation is in use.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_services_shutdown() {
  for (int inx = 0;
       mysql_component_mysql_server.provides[inx].implementation != NULL;
       ++inx) {
    if (imp_mysql_server_registry_registration.unregister(
            mysql_component_mysql_server.provides[inx].name)) {
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
void mysql_components_handle_std_exception(const char *funcname) {
  try {
    throw;
  } catch (const std::bad_alloc &e) {
    my_error(ER_STD_BAD_ALLOC_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::domain_error &e) {
    my_error(ER_STD_DOMAIN_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::length_error &e) {
    my_error(ER_STD_LENGTH_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::invalid_argument &e) {
    my_error(ER_STD_INVALID_ARGUMENT, MYF(0), e.what(), funcname);
  } catch (const std::out_of_range &e) {
    my_error(ER_STD_OUT_OF_RANGE_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::overflow_error &e) {
    my_error(ER_STD_OVERFLOW_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::range_error &e) {
    my_error(ER_STD_RANGE_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::underflow_error &e) {
    my_error(ER_STD_UNDERFLOW_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::logic_error &e) {
    my_error(ER_STD_LOGIC_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::runtime_error &e) {
    my_error(ER_STD_RUNTIME_ERROR, MYF(0), e.what(), funcname);
  } catch (const std::exception &e) {
    my_error(ER_STD_UNKNOWN_EXCEPTION, MYF(0), e.what(), funcname);
  } catch (...) {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
  }
}
