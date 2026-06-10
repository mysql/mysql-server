/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>  // pid_t
#include <unistd.h>
#endif

#include <opentelemetry/sdk/version/version.h>
#include <opentelemetry/semconv/incubating/service_attributes.h>
#include <opentelemetry/version.h>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_query_attributes.h>
#include <mysql/components/services/pfs_notification.h>

#include "my_hostname.h"
#include "tm_global.h"
#include "tm_log.h"
#include "tm_required_services.h"
#include "tm_setup_otel.h"
#include "tm_system_variables.h"

namespace telemetry {

static const std::string service_name("telemetry_resource_provider");

/* Optional dependency. */
REQUIRES_SERVICE_PLACEHOLDER_AS(telemetry_resource_provider,
                                resource_provider_srv);

static void setup_default_resource(
    opentelemetry::sdk::resource::Resource &resource) {
  char host_name[HOSTNAME_LENGTH + 1];
  ulong port_number = 0;

  gethostname(host_name, sizeof(host_name));
  host_name[sizeof(host_name) - 1] = '\0';

  /*
    TODO: Better service identification

    What we should do is read mysqld_port in mysqld.cc,
    which requires a system_variable read service,
    so that "hostname:port_number" is a stable service id,
    identical after a restart.

    Instead, use PID as service identifier,
    so the service id is "hostname:pid",
    unique but not stable after a restart.
  */
  port_number = static_cast<ulong>(getpid());

  std::string service_instance_id;
  service_instance_id.append(host_name);
  service_instance_id.append(":");
  service_instance_id.append(std::to_string(port_number));

  opentelemetry::sdk::resource::ResourceAttributes attributes;

  attributes[opentelemetry::semconv::service::kServiceInstanceId] =
      service_instance_id;

  resource = setup_resource(attributes, sv_otel_resource_attributes);
}

static void detect_resource(opentelemetry::sdk::resource::Resource &resource,
                            bool &must_wait) {
  std::string full_service_name(service_name);
  full_service_name.append(".");
  full_service_name.append(sv_resource_provider);
  my_h_service h_srv = nullptr;
  opentelemetry::sdk::resource::ResourceAttributes attributes;
  int rc;

  log_info("%s: Acquiring service <%s> ...", component_name,
           full_service_name.c_str());

  rc = reg_srv->acquire(full_service_name.c_str(), &h_srv);

  if (rc != 0) {
    log_warning("%s: Failed to acquire service <%s>, will wait.",
                component_name, full_service_name.c_str());

    must_wait = true;
    return;
  }

  if (h_srv != nullptr) {
    log_info("%s: Invoking service <%s> ...", component_name,
             full_service_name.c_str());

    telemetry_resource_t *component_resource;
    telemetry_resource_iterator_t *iter;

    resource_provider_srv =
        reinterpret_cast<SERVICE_TYPE(telemetry_resource_provider) *>(h_srv);
    component_resource = resource_provider_srv->create();

    if (component_resource != nullptr) {
      iter = resource_provider_srv->iterator_create(component_resource);

      if (iter != nullptr) {
        const char *attribute_key = nullptr;
        const char *attribute_value = nullptr;
        bool key_rc;
        bool value_rc;
        bool has_next;

        do {
          key_rc = resource_provider_srv->iterator_get_key_name(iter,
                                                                &attribute_key);
          value_rc = resource_provider_srv->iterator_get_key_value(
              iter, &attribute_value);

          if (!key_rc && !value_rc) {
            log_info("%s: Adding resource attribute <%s>", component_name,
                     attribute_key);

            attributes[attribute_key] = attribute_value;
          }

          has_next = resource_provider_srv->iterator_next(iter);
        } while (has_next);

        resource_provider_srv->iterator_destroy(iter);
      }

      resource_provider_srv->destroy(component_resource);
    }

    reg_srv->release(h_srv);
    resource_provider_srv = nullptr;

    log_info("%s: Released service <%s>", component_name,
             full_service_name.c_str());
  }

  must_wait = false;
  resource = setup_resource(attributes, sv_otel_resource_attributes);
  return;
}

void setup_resource(opentelemetry::sdk::resource::Resource &resource,
                    bool &must_wait) {
  if (strlen(sv_resource_provider) != 0) {
    detect_resource(resource, must_wait);
  } else {
    must_wait = false;
    setup_default_resource(resource);
  }
}

}  // namespace telemetry
