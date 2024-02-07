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

#ifndef COMPONENTS_TEST_EVENT_TRACKING_TEST_EXAMPLE_CONSUMER_HELPER_H
#define COMPONENTS_TEST_EVENT_TRACKING_TEST_EXAMPLE_CONSUMER_HELPER_H

#include "event_tracking_example_service.h"
#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"

#define PROVIDES_SERVICE_EVENT_TRACKING_EXAMPLE(component) \
  PROVIDES_SERVICE(component, event_tracking_example)

#define IMPLEMENTS_SERVICE_EVENT_TRACKING_EXAMPLE(component)                   \
  BEGIN_SERVICE_IMPLEMENTATION(component, event_tracking_example)              \
  Event_tracking_implementation::Event_tracking_example_implementation::notify \
  END_SERVICE_IMPLEMENTATION()

namespace Event_tracking_implementation {
/** Implementation helper class for example events. */
class Event_tracking_example_implementation {
 public:
  /** Sub-events to be filtered/ignored - To be defined by the component */
  static mysql_event_tracking_example_subclass_t filtered_sub_events;

  /** Callback function - To be implemented by component to handle an event */
  static bool callback(const mysql_event_tracking_example_data *data);

  /**
    event_tracking_example service implementation

    @param [in] data  Data related to example event

    @returns Status of operation
      @retval false Success
      @retval true  Failure
  */
  static DEFINE_BOOL_METHOD(notify,
                            (const mysql_event_tracking_example_data *data)) {
    try {
      if (!data) return true;
      if (filtered_sub_events & data->event_subclass) return false;
      return callback(data);
    } catch (...) {
      return true;
    }
  }
};
}  // namespace Event_tracking_implementation

#endif  // !COMPONENTS_TEST_EVENT_TRACKING_TEST_EXAMPLE_CONSUMER_HELPER_H
