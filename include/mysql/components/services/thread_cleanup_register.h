/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef THREAD_CLEANUP_REGISTER_H
#define THREAD_CLEANUP_REGISTER_H

#include <mysql/components/service.h>

/**
  @ingroup group_components_services_inventory

  Thread cleanup service allows a OS thread to free resources allocated
  for specific thread, during thread exit. Component can register for
  thread cleanup handler, for desired OS threads.

  At thread exit, the service 'thread_cleanup_handler' implemented
  by the component is invoked by MySQL server.

  @section Registering thread cleanup.

  The service can be instantiated using the registry service with the
  "thread_cleanup_register" name.

  @code
  SERVICE_TYPE(registry) *registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(thread_cleanup_register)>
                                svc("thread_cleanup_register", registry);
  if (svc.is_valid()) {
    // The service is ready to be used
  }
  @endcode

  The code below demonstrates way to register thread cleanup for a thread
  from a component. Note that we cannot deregister thread cleanup event. In
  case the component is uninstalled, then the thread cleanup is NO-OP.

  @code
  svc->register_cleanup("implementation_name");
  @endcode
*/
BEGIN_SERVICE_DEFINITION(thread_cleanup_register)

/**
  Enable thread cleanup for calling thread.

  @param [in] component_name  Must be same as the implementation name
                              used for @ref thread_cleanup_handler service.

  @return The function always fail and return TRUE value.
*/
DECLARE_BOOL_METHOD(register_cleanup, (char const *component_name));

END_SERVICE_DEFINITION(thread_cleanup_register)

#endif /* THREAD_CLEANUP_REGISTER_H */
