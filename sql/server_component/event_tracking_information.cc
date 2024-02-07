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

#include "event_tracking_information_imp.h"

#include "mysql/components/minimal_chassis.h"
#include "sql/current_thd.h"
#include "sql/sql_audit.h"
#include "sql/sql_class.h"

DEFINE_BOOL_METHOD(Event_tracking_authentication_information_imp::init,
                   (event_tracking_authentication_information_handle *
                    handle)) {
  try {
    THD *thd = current_thd;
    if (!thd || !handle) return true;

    Event_tracking_data data = thd->get_event_tracking_data();
    if (data.first != Event_tracking_class::AUTHENTICATION ||
        data.second == nullptr)
      return true;

    *handle =
        reinterpret_cast<event_tracking_authentication_information_handle>(
            data.second);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(Event_tracking_authentication_information_imp::deinit,
                   (event_tracking_authentication_information_handle handle
                    [[maybe_unused]])) {
  try {
    /**
      No need to free the handle because
      THD::event_notify will take care of it
    */
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(Event_tracking_authentication_information_imp::get,
                   (event_tracking_authentication_information_handle handle,
                    const char *name, void *value)) {
  try {
    if (!handle || !name) return true;
    auto data =
        reinterpret_cast<Event_tracking_authentication_information *>(handle);

    if (!strcmp(name, "new_user")) {
      if (!data->new_user_.length) return true;
      *((mysql_cstring_with_length *)value) = data->new_user_;
    } else if (!strcmp(name, "new_host")) {
      if (!data->new_host_.length) return true;
      *((mysql_cstring_with_length *)value) = data->new_host_;
    } else if (!strcmp(name, "is_role")) {
      *((bool *)value) = data->is_role_;
    } else if (!strcmp(name, "authentcation_method_count")) {
      if (!data->authentication_methods_.size()) return true;
      *((unsigned int *)value) = data->authentication_methods_.size();
    } else if (!strcmp(name, "authentication_method_info")) {
      *((event_tracking_authentication_method_handle *)value) =
          reinterpret_cast<event_tracking_authentication_method_handle>(handle);
    } else
      return true;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(Event_tracking_authentication_method_imp::get,
                   (event_tracking_authentication_method_handle handle,
                    unsigned int index, const char *name, void *value)) {
  try {
    if (!handle || !value) return true;
    auto data =
        reinterpret_cast<Event_tracking_authentication_information *>(handle);

    if (index >= data->authentication_methods_.size()) return true;

    mysql_cstring_with_length val;

    if (!strcmp(name, "name")) {
      val.str = data->authentication_methods_[index];
      val.length = data->authentication_methods_[index]
                       ? strlen(data->authentication_methods_[index])
                       : 0;
      *((mysql_cstring_with_length *)value) = val;
    } else
      return true;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(Event_tracking_general_information_imp::init,
                   (event_tracking_general_information_handle * handle)) {
  try {
    THD *thd = current_thd;
    if (!thd || !handle) return true;

    Event_tracking_data data = thd->get_event_tracking_data();
    if (data.first != Event_tracking_class::GENERAL || data.second == nullptr)
      return true;

    *handle = reinterpret_cast<event_tracking_general_information_handle>(
        data.second);
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(Event_tracking_general_information_imp::deinit,
                   (event_tracking_general_information_handle handle
                    [[maybe_unused]])) {
  try {
    /**
      No need to free the handle because
      THD::event_notify will take care of it
    */
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_BOOL_METHOD(Event_tracking_general_information_imp::get,
                   (event_tracking_general_information_handle handle,
                    const char *name, void *value)) {
  try {
    if (!handle || !name) return true;

    Event_tracking_general_information *data =
        reinterpret_cast<Event_tracking_general_information *>(handle);

    if (!strcmp(name, "rows")) {
      *((uint64_t *)value) = data->rows_;
    } else if (!strcmp(name, "time")) {
      *((uint64_t *)value) = data->time_;
    } else if (!strcmp(name, "external_user")) {
      *((mysql_cstring_with_length *)value) = data->external_user_;
    } else if (!strcmp(name, "command")) {
      *((mysql_cstring_with_length *)value) = data->command_;
    } else
      return true;

    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
