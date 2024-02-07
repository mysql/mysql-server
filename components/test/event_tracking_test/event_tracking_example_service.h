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

#ifndef COMPONENTS_TEST_EVENT_TRACKING_TEST_EVENT_TRACKING_EXAMPLE_SERVICE_H
#define COMPONENTS_TEST_EVENT_TRACKING_TEST_EVENT_TRACKING_EXAMPLE_SERVICE_H

#include "event_tracking_example_service_defs.h"
#include "mysql/components/service.h"

/**
  Example event tracking service
*/
BEGIN_SERVICE_DEFINITION(event_tracking_example)

/**
  Process example event

  @param [in] data Event data

  @returns Status of handling
    @retval false success
    @retval true  error
*/
DECLARE_BOOL_METHOD(notify, (const mysql_event_tracking_example_data *data));

END_SERVICE_DEFINITION(event_tracking_example)

#endif  // !COMPONENTS_TEST_EVENT_TRACKING_TEST_EVENT_TRACKING_EXAMPLE_SERVICE_H
