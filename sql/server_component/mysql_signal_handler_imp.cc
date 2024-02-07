/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mysql_signal_handler_imp.h"

#include <signal.h>
#include "mysql/components/minimal_chassis.h"  // mysql_components_handle_std_exception
#include "mysql/components/service_implementation.h"
#include "sql/signal_handler.h"

constexpr auto MYSQL_SUCCESS = false;
constexpr auto MYSQL_FAILURE = true;

/**
  Implement the registration of the callback for a specific signal.

  @note Registration fails if there is already a registered callback for the
  same signal.

  @param signal_no signal to register for
  @param callback the callback to call
  @retval false success
  @retval true failure

  @sa mysql_service_host_application_signal_t
*/
DEFINE_BOOL_METHOD(my_signal_handler_imp::add,
                   (int signal_no, my_signal_handler_callback_t callback)) {
  bool retval = MYSQL_SUCCESS;
  try {
    switch (signal_no) {
      case SIGSEGV:
        if (g_fatal_callback.load() != nullptr) return MYSQL_FAILURE;
        g_fatal_callback = callback;
        break;
      default:
        retval = MYSQL_FAILURE;
        break;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return retval;
}

/**
  Implement the de-registration of the callback for a specific signal

  @note If the callback is not registered before, un-registration fails.

  @param signal_no signal to register for
  @param callback the callback to call
  @retval false success
  @retval true failure

  @sa mysql_service_host_application_signal_t
*/
DEFINE_BOOL_METHOD(my_signal_handler_imp::remove,
                   (int signal_no, my_signal_handler_callback_t callback)) {
  bool retval = MYSQL_SUCCESS;
  try {
    switch (signal_no) {
      case SIGSEGV:
        if (g_fatal_callback.load() == callback)
          g_fatal_callback = nullptr;
        else
          return MYSQL_FAILURE;
        break;
      default:
        retval = MYSQL_FAILURE;
        break;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return retval;
}
