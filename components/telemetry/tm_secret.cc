/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/log_builtins.h>

#include "tm_global.h"
#include "tm_log.h"
#include "tm_required_services.h"
#include "tm_secret.h"
#include "tm_system_variables.h"

namespace telemetry {

static const std::string service_name("telemetry_secret_provider");

/* Optional dependency. */
REQUIRES_SERVICE_PLACEHOLDER_AS(telemetry_secret_provider, secret_provider_srv);

static int decode_one_secret(telemetry_secret_client_t *client,
                             const char *secret_name,
                             std::string &decoded_secret) {
  telemetry_secret_t *secret;
  const char *sensitive_secret = nullptr;
  int rc;
  bool failure;

  secret = secret_provider_srv->secret_open(client, secret_name);

  if (secret == nullptr) {
    log_error("%s: Can not open secret <%s>.", component_name, secret_name);
    return SECRET_OPEN_ERROR;
  }

  failure = secret_provider_srv->secret_read(secret, &sensitive_secret);

  if (!failure) {
    log_info("%s: Read secret <%s>", component_name, secret_name);
    decoded_secret = sensitive_secret;
    rc = 0;
  } else {
    log_error("%s: Can not read secret <%s>.", component_name, secret_name);
    rc = SECRET_READ_ERROR;
  }

  secret_provider_srv->secret_close(secret);

  return rc;
}

int decode_secrets(bool &must_wait) {
  sensitive_otel_exporter_otlp_traces_secret_headers = "";
  sensitive_otel_exporter_otlp_metrics_secret_headers = "";
  sensitive_otel_exporter_otlp_logs_secret_headers = "";

  must_wait = false;

  bool need_decoding = false;
  bool has_provider = false;

  if ((strlen(sv_otel_exporter_otlp_traces_secret_headers) != 0) ||
      (strlen(sv_otel_exporter_otlp_metrics_secret_headers) != 0) ||
      (strlen(sv_otel_exporter_otlp_logs_secret_headers) != 0)) {
    need_decoding = true;
  }

  if (!need_decoding) {
    // Nothing to do.
    return 0;
  }

  if (strlen(sv_secret_provider) != 0) {
    has_provider = true;
  }

  if (!has_provider) {
    // This really needs to be a warning and not an error,
    // otherwise it is possible with SET PERSIST to write
    // a broken configuration that prevents to load the telemetry component,
    // so that fixing the issue becomes impossible.
    log_warning("%s: Ignoring secrets, no secret_provider defined.",
                component_name);
    return 0;
  }

  std::string full_service_name(service_name);
  full_service_name.append(".");
  full_service_name.append(sv_secret_provider);
  my_h_service h_srv = nullptr;
  telemetry_secret_client_t *client = nullptr;
  int failure;
  int acquire_rc = 0;
  int traces_rc = 0;
  int metrics_rc = 0;
  int logs_rc = 0;

  log_info("%s: Acquiring service <%s> ...", component_name,
           full_service_name.c_str());

  failure = reg_srv->acquire(full_service_name.c_str(), &h_srv);

  if (failure != 0) {
    log_warning("%s: Failed to acquire service <%s>, will wait.",
                component_name, full_service_name.c_str());

    must_wait = true;
    return 0;
  }

  log_info("%s: Invoking service <%s> ...", component_name,
           full_service_name.c_str());

  secret_provider_srv =
      reinterpret_cast<SERVICE_TYPE(telemetry_secret_provider) *>(h_srv);

  client = secret_provider_srv->secret_init();

  if (client != nullptr) {
    if (strlen(sv_otel_exporter_otlp_traces_secret_headers) != 0) {
      traces_rc =
          decode_one_secret(client, sv_otel_exporter_otlp_traces_secret_headers,
                            sensitive_otel_exporter_otlp_traces_secret_headers);
    }

    if (strlen(sv_otel_exporter_otlp_metrics_secret_headers) != 0) {
      metrics_rc = decode_one_secret(
          client, sv_otel_exporter_otlp_metrics_secret_headers,
          sensitive_otel_exporter_otlp_metrics_secret_headers);
    }

    if (strlen(sv_otel_exporter_otlp_logs_secret_headers) != 0) {
      logs_rc =
          decode_one_secret(client, sv_otel_exporter_otlp_logs_secret_headers,
                            sensitive_otel_exporter_otlp_logs_secret_headers);
    }
  } else {
    log_error("%s: Failed to init service <%s>, not decoding secrets.",
              component_name, full_service_name.c_str());

    acquire_rc = SECRET_PROVIDER_INIT_ERROR;
  }

  secret_provider_srv->secret_cleanup(client);

  reg_srv->release(h_srv);
  secret_provider_srv = nullptr;

  log_info("%s: Released service <%s>", component_name,
           full_service_name.c_str());

  if (acquire_rc != 0) {
    return acquire_rc;
  }

  if (traces_rc != 0) {
    return traces_rc;
  }

  if (metrics_rc != 0) {
    return metrics_rc;
  }

  if (logs_rc != 0) {
    return logs_rc;
  }

  return 0;
}

}  // namespace telemetry
