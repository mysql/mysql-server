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

#include "event_tracking_example_service.h"
#include "event_tracking_producer.h"
#include "event_tracking_registry.h"

#include <cstring>
#include <iostream>
#include <map>
#include <string>

#include "mysql/components/my_service.h"

#define CSTRING_WITH_LENGTH(x) \
  { x, x ? strlen(x) : 0 }
#define EVENT_NAME(x) #x

extern REQUIRES_SERVICE_PLACEHOLDER(registry);

namespace event_tracking_producer {

const char *component_name = "event_tracking_producer_b";
Event_producer *g_event_producer = nullptr;

void print_info(const std::string &event) {
  std::cout << "-------------------------------------------------------------"
            << std::endl;
  std::cout << "Component: " << event_tracking_producer::component_name
            << ". Event : " << event << "." << std::endl;
}

void print_result(const std::string &event, bool result) {
  std::string retval = result ? "Error." : "Success.";
  std::cout << "Component: " << event_tracking_producer::component_name
            << ". Event: " << event << ". Consumer returned: " << retval
            << std::endl;
  std::cout << "-------------------------------------------------------------"
            << std::endl;
}

bool Event_producer::generate_events() {
  auto generate_event = [](auto &service, auto data,
                           const char *event_name) -> bool {
    bool result = true;
    print_info(event_name);
    result = service->notify(data);
    print_result(event_name, result);
    return result;
  };

  /* Generate EXAMPLE events */
  mysql_event_tracking_example_data example_data;
  example_data.id = 1;
  example_data.name = "Example event";

  /*
    Ideally we should have used reference caching service and do following:
    1. Define a reference caching cache channel for event_tracking_example
    2. Create a reference caching cache using the channel created in step 1
    3. Iterate over reference caching cache and broadcast the event

    The advantage we have here is - if a new consumer component is installed,
    reference caching component would have invalidated the channel created
    above. This would have forced cache to reaquire all reference next time
    it is used and producer would have automatically broadcasted service to
    the new component.

    Problem is: reference cache component uses psi_rwlock_v2 that's
    implemented by server component. Hence we cannot use it here.
  */
  std::map<std::string, int> service_names;
  {
    my_service<SERVICE_TYPE(registry_query)> query("registry_query",
                                                   mysql_service_registry);
    if (query.is_valid()) {
      my_h_service_iterator iter;
      std::string service_name{"event_tracking_example"};
      if (!query->create(service_name.c_str(), &iter)) {
        while (!query->is_valid(iter)) {
          const char *implementation_name;

          // can't get the name
          if (query->get(iter, &implementation_name)) return true;

          if (strncmp(implementation_name, service_name.c_str(),
                      service_name.length())) {
            break;
          }

          service_names[implementation_name] = 1;

          if (query->next(iter)) break;
        }
        query->release(iter);
      }
    }
  }

  for (auto element : service_names) {
    my_service<SERVICE_TYPE(event_tracking_example)> example_service(
        element.first.c_str(), mysql_service_registry);

    if (example_service.is_valid()) {
      example_data.event_subclass = EVENT_TRACKING_EXAMPLE_FIRST;
      if (generate_event(example_service, &example_data,
                         EVENT_NAME(EVENT_TRACKING_EXAMPLE_FIRST)))
        return true;

      example_data.event_subclass = EVENT_TRACKING_EXAMPLE_SECOND;
      if (generate_event(example_service, &example_data,
                         EVENT_NAME(EVENT_TRACKING_EXAMPLE_SECOND)))
        return true;

      example_data.event_subclass = EVENT_TRACKING_EXAMPLE_THIRD;
      if (generate_event(example_service, &example_data,
                         EVENT_NAME(EVENT_TRACKING_EXAMPLE_THIRD)))
        return true;
    }
  }
  return false;
}

static mysql_service_status_t init() {
  g_event_producer = new (std::nothrow) Event_producer();
  if (!g_event_producer || g_event_producer->generate_events()) return true;
  return false;
}

static mysql_service_status_t deinit() {
  if (g_event_producer) delete g_event_producer;
  g_event_producer = nullptr;
  return false;
}
}  // namespace event_tracking_producer

BEGIN_COMPONENT_PROVIDES(event_tracking_producer_b)
/* Nothing */
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(event_tracking_producer_b)
/* Nothing */
END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(event_tracking_producer_b)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(event_tracking_producer_b,
                  event_tracking_producer::component_name)
event_tracking_producer::init,
    event_tracking_producer::deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(event_tracking_producer_b)
    END_DECLARE_LIBRARY_COMPONENTS
