/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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


/**
  @brief

  This defines the API used to call functions in logging components.
  When implementing such a service, refer to log_service_imp.h instead!

  A log service may take the shape of a writer for a specific log format
  (JSON, XML, traditional MySQL, etc.), it may implement a filter that
  removes or modifies log_items, etc.
*/

#ifndef LOG_SERVICE_H
#define LOG_SERVICE_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/log_shared.h>

BEGIN_SERVICE_DEFINITION(log_service)
  /**
    Have the service process one log line.

    @param   instance  State-pointer that was returned on open.
    @param   ll        The log_line collection of log_items.

    @retval  <0        an error occurred
    @retval  =0        no work was done
    @retval  >0        number of processed entities
  */
  DECLARE_METHOD(int, run,             (void *instance, log_line *ll));

  /**
    Flush any buffers.  This function will be called by the server
    on FLUSH ERROR LOGS.  The service may write its buffers, close
    and re-open any log files to work with log-rotation, etc.
    The flush function MUST NOT itself log anything!
    A service implementation may provide a nullptr if it does not
    wish to provide a flush function.

    @param   instance  State-pointer that was returned on open.
                       Value may be changed in flush.

    @retval  <0        an error occurred
    @retval  =0        no work was done
    @retval  >0        flush completed without incident
  */
  DECLARE_METHOD(int, flush,           (void **instance));

  /**
    Open a new instance.

    @param   ll        optional arguments
    @param   instance  If state is needed, the service may allocate and
                       initialize it and return a pointer to it here.
                       (This of course is particularly pertinent to
                       components that may be opened multiple times,
                       such as the JSON log writer.)
                       This state is for use of the log-service component
                       in question only and can take any layout suitable
                       to that component's need. The state is opaque to
                       the server/logging framework. It must be released
                       on close.

    @retval  <0        a new instance could not be created
    @retval  =0        success, returned hande is valid
  */
  DECLARE_METHOD(int, open,         (log_line *ll, void **instance));

  /**
    Close and release an instance. Flushes any buffers.

    @param   instance  State-pointer that was returned on open.
                       If memory was allocated for this state,
                       it should be released, and the pointer
                       set to nullptr.

    @retval  <0        an error occurred
    @retval  =0        success
  */
  DECLARE_METHOD(int, close,           (void **instance));

  /**
    Variable listener.  This is a temporary solution until we have
    per-component system variables.  "check" is called when the user
    uses SQL statements trying to assign a value to certain server
    system variables; the function can prevent assignment if e.g.
    the supplied value has the wrong format.

    If several listeners are registered, an error will be signaled
    to the user on the SQL level as soon as one service identifies
    a problem with the value.

    @param   ll  log-line having as first element a list-item
                 describing the variable (name, new value)

    @retval   0  for allow (including when we don't feel the event is for us),
    @retval  -1  for deny
  */
  DECLARE_METHOD(int, variable_check,  (log_line *ll));

  /**
    Variable listener.  This is a temporary solution until we have
    per-component system variables. "update" is called when the user
    uses SQL statements trying to assign a value to certain server
    system variables. If we got this far, we have already been called
    upon to "check" the new value, and have confirmed that it meets
    the requirements. "update" should now update the internal
    representation of the value. Since we have already checked the
    new value, failure should be a rare occurance (out of memory,
    the operating system did not let us open the new file name, etc.).

    If several listeners are registered, all will currently be called
    with the new value, even if one of them signals failure.

    @param  ll  log-line having as first element a list-item 
                describing the variable (name, new value)

    @retval  0  for success (including when we don't feel the event is for us),
    @retval !0  for failure
  */
  DECLARE_METHOD(int, variable_update, (log_line *ll));
END_SERVICE_DEFINITION(log_service)

#endif
