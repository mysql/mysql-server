/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "dynamic_loader_service_notification_imp.h"
#include "mysql_current_thread_reader_imp.h"

#include "sql/reference_caching_setup.h"
#include "sql/sql_class.h"

DEFINE_BOOL_METHOD(Dynamic_loader_services_loaded_notification_imp::notify,
                   (const char **services, unsigned int count)) {
  try {
    if (g_event_channels) {
      for (unsigned int index = 0; index < count; ++index) {
        (void)g_event_channels->service_notification(services[index], true);
      }
    }
    return false;
  } catch (...) {
    return true;
  }
}

DEFINE_BOOL_METHOD(Dynamic_loader_services_unload_notification_imp::notify,
                   (const char **services [[maybe_unused]],
                    unsigned int count)) {
  try {
    bool no_op = true;
    if (g_event_channels) {
      for (unsigned int index = 0; index < count; ++index) {
        no_op &= g_event_channels->service_notification(services[index], false);
      }
    }
    if (!no_op) {
      THD *thd = current_thd;
      if (thd) thd->refresh_reference_caches();
    }
    return false;
  } catch (...) {
    return true;
  }
}
