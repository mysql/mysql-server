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

#include "thread_cleanup_register_imp.h"

#include <mysql/components/my_service.h>
#include <mysql/components/services/thread_cleanup_handler.h>
#include <cassert>
#include <list>
#include <string>
#include "sql/mysqld.h" /* srv_registry */

class Thread_cleanup {
  class ThreadExitHandler {
   public:
    std::list<std::string> requested_component_names;

    ~ThreadExitHandler() {
      // This assert would fail, only if external language SP is executed as
      // part of thread executing mysqld_main. This should not be allowed.
      assert(srv_registry);
      if (!srv_registry) {
        return;
      }

      // Invoke exit_handler for all the components, that requested
      // callback.
      for (auto component_name : requested_component_names) {
        my_service<SERVICE_TYPE(thread_cleanup_handler)> service(
            ("thread_cleanup_handler." + component_name).c_str(), srv_registry);

        // Ignore handler invocation if the component service is not installed
        if (!service) {
          service->exit_handler();
        }
      }
    }
  };

 public:
  void setup_thread_exit_handler(std::string component_name) {
    thread_local ThreadExitHandler teh;
    teh.requested_component_names.push_back(component_name);
  }
};

DEFINE_BOOL_METHOD(thread_cleanup_register_imp::register_cleanup,
                   (char const *component_name)) {
  Thread_cleanup thread_cleanup_context;
  thread_cleanup_context.setup_thread_exit_handler(component_name);

  return false;
}
