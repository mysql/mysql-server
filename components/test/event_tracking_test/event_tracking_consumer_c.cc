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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <cstring>
#include <iostream>
#include <string>

#include "event_tracking_example_consumer_helper.h"

#define EVENT_NAME(x) #x

namespace Event_tracking_consumer_c {
const char *component_name = "event_tracking_consumer_c";

static mysql_service_status_t init() { return false; }

static mysql_service_status_t deinit() { return false; }
}  // namespace Event_tracking_consumer_c

namespace Event_tracking_implementation {

void print_info(const std::string &event, const std::string &event_data) {
  std::cout << "Component: " << Event_tracking_consumer_c::component_name
            << ". Event : " << event << ". Data : " << event_data << "."
            << std::endl;
}

/* START: Service Implementation */
mysql_event_tracking_example_subclass_t
    Event_tracking_example_implementation::filtered_sub_events = 0;
bool Event_tracking_example_implementation::callback(
    const mysql_event_tracking_example_data *data) {
  std::string event_name{}, event_data{"["};
  event_data += "ID: ";
  event_data += std::to_string(data->id);
  event_data += ", Name: ";
  event_data += data->name;

  switch (data->event_subclass) {
    case EVENT_TRACKING_EXAMPLE_FIRST:
      event_name.assign(EVENT_NAME(EVENT_TRACKING_EXAMPLE_FIRST));
      break;
    case EVENT_TRACKING_EXAMPLE_SECOND:
      event_name.assign(EVENT_NAME(EVENT_TRACKING_EXAMPLE_SECOND));
      break;
    case EVENT_TRACKING_EXAMPLE_THIRD:
      event_name.assign(EVENT_NAME(EVENT_TRACKING_EXAMPLE_THIRD));
      break;
    default:
      return true;
  };

  event_data += " ]";
  print_info(event_name, event_data);
  return false;
}

/* END: Service Implementation */

}  // namespace Event_tracking_implementation

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
IMPLEMENTS_SERVICE_EVENT_TRACKING_EXAMPLE(event_tracking_consumer_c);

/** This component provides following services */
BEGIN_COMPONENT_PROVIDES(event_tracking_consumer_c)
PROVIDES_SERVICE_EVENT_TRACKING_EXAMPLE(event_tracking_consumer_c),
    END_COMPONENT_PROVIDES();

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(event_tracking_consumer_c)
/* Nothing */
END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(event_tracking_consumer_c)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("event_tracking_consumer_c", "1"), END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(event_tracking_consumer_c,
                  Event_tracking_consumer_c::component_name)
Event_tracking_consumer_c::init,
    Event_tracking_consumer_c::deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(event_tracking_consumer_c)
    END_DECLARE_LIBRARY_COMPONENTS
