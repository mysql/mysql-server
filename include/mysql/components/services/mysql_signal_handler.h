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

#ifndef MY_SIGNAL_HANDLER_H
#define MY_SIGNAL_HANDLER_H

#include <mysql/components/service.h>
#if defined(_WIN32)
struct siginfo_t;
#else
#include <signal.h>
#endif

// Note: siginfo_t is a complex structure. It should not be passed through the
// component API boundary. This is an exception just for this service.
using my_signal_handler_callback_t = void (*)(int, siginfo_t *, void *);

/**
  @ingroup group_components_services_inventory

  A service to register/deregister a signal handler function callback

  The server component signal handler will call this callback
  if a signal occurs.
  The callback needs to be signal reentrant safe code.
  Otherwise no guaranees on what happens.

   Usage example:
    auto register_signal_handler_callback(my_signal_handler_callback_t callback)
      -> bool {
        SERVICE_PLACEHOLDER(my_signal_handler)->add(SIGSEGV, callback) == 0;
    }
    auto unregister_signal_handler_callback(my_signal_handler_callback_t
  callback)
      -> bool {
          return SERVICE_PLACEHOLDER(my_signal_handler)->remove(SIGSEGV,
  callback) == 0;
    }

    auto handle_segfault_signal(int signum) {
      // Code to handle segfault
      // ...
    }

    If this is used inside another component, at component init():
    register_signal_handler_callback(&handle_segfault_signal)

    At component deinit():
    unregister_signal_handler_callback(&handle_segfault_signal)

  @sa my_signal_handler_imp
*/
BEGIN_SERVICE_DEFINITION(my_signal_handler)
/**
  Register a callback that will be called when mysql server component handles a
  fatal signal.

  @note: Each registered callback needs to be unregistered by calling remove.

  @param signal_no The signal number to listen to
  @param callback Signal handling callback
  @return Status of performed operation
  @retval false registered successfully
  @retval true failure to register

  @sa my_host_application_signal
*/
DECLARE_BOOL_METHOD(add,
                    (int signal_no, my_signal_handler_callback_t callback));

/**
  Unregister a callback that was registered.

  @param signal_no The signal number to listen to
  @param callback Signal handling callback
  @return Status of performed operation
  @retval false unregistered successfully
  @retval true failure to unregister

  @sa my_host_application_signal
*/
DECLARE_BOOL_METHOD(remove,
                    (int signal_no, my_signal_handler_callback_t callback));

END_SERVICE_DEFINITION(my_signal_handler)

#endif /* MY_SIGNAL_HANDLER_H */
