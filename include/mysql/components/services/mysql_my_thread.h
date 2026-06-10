/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVICE_MY_THREAD_H
#define MYSQL_SERVICE_MY_THREAD_H

#include <mysql/components/service.h>

/**
  @ingroup group_components_services_inventory

  A service allowing allocation thread specific memory for a calling thread.

  Specific memory is used by mysys and dbug purposes. The example below shows
  how to use the service by the std::thread that uses the
  mysql_service_mysql_debug_keyword_service service. keyword);

  @code

  static void *a_thread(void *arg) {
    // Allocate thread memory
    mysql_service_mysql_my_thread->attach();

    // Thread custom code begins here
    if
  (mysql_service_mysql_debug_keyword_service->lookup_debug_keyword("my_keyword"))
      printf("my_keyword found!");

    // Deallocate thread memory on thread exit.
    mysql_service_mysql_my_thread->detach();

    return nullptr;
  }

  std::thread thread(a_thread);

  thread.join();

  @endcode
*/
BEGIN_SERVICE_DEFINITION(mysql_my_thread)

/*
  Allocate thread specific memory for the thread, used by mysys and dbug.

  @return Zero value on success.
*/
DECLARE_BOOL_METHOD(attach, ());

/*
  Deallocate thread specific memory allocated with an attach() method.

  @return Zero value on success.
*/
DECLARE_BOOL_METHOD(detach, ());

/*
  Check, whether the attach() method was successfully called.

  @return attach() result.
*/
DECLARE_BOOL_METHOD(is_attached, ());

END_SERVICE_DEFINITION(mysql_my_thread)

#endif /* MYSQL_SERVICE_MY_THREAD_H */
