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
#include <opentelemetry/version.h>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/dynamic_loader_service_notification.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_query_attributes.h>
#include <mysql/components/services/pfs_notification.h>

#include "my_hostname.h"
#include "option_usage.h"
#include "tm_control.h"
#include "tm_global.h"
#include "tm_log.h"
#include "tm_notification.h"
#include "tm_ns.h"
#include "tm_psi.h"
#include "tm_required_services.h"
#include "tm_resource.h"
#include "tm_secret.h"
#include "tm_setup_otel.h"
#include "tm_slot.h"
#include "tm_status_variables.h"
#include "tm_system_variables.h"

namespace telemetry {

REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                statvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                sysvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                sysvar_unregister_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_string_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_notification_v3, notification_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader, current_thd_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_connection_attributes_iterator,
                                con_attr_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_store, thd_store_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_traces_v1,
                                telemetry_traces_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_metrics_v1,
                                telemetry_metrics_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_logs,
                                telemetry_logs_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attributes_iterator, qa_iter_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attribute_string, qa_string_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_get_data_in_charset,
                                string_get_data_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter, string_converter_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_cond_v1, cond_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_mutex_v1, mutex_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(psi_metric_v1, metric_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(psi_stage_v1, stage_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(psi_thread_v7, thread_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(psi_idle_v1, idle_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration, reg_reg);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry, reg_srv);

const char *component_name = "telemetry";

constexpr int FLUSH_TIMEOUT_USEC = 10000;
constexpr int CLOSE_TIMEOUT_USEC = 10000;

opentelemetry::sdk::resource::Resource g_resource =
    opentelemetry::sdk::resource::Resource::GetEmpty();

mysql_service_status_t telemetry_init_install();
void telemetry_init_detect_resource(bool &must_wait);
mysql_service_status_t telemetry_init_decode_secret(bool &must_wait);
mysql_service_status_t telemetry_init_configure();
void telemetry_init_noop_configure();

void telemetry_deinit_install();

static DEFINE_BOOL_METHOD(services_loaded_notification,
                          (const char ** /* services */,
                           unsigned int /* count */)) {
  int rc;
  bool must_wait = false;
  int run_level = g_run_level.load();

  if ((run_level == RUN_LEVEL_READY) || (run_level == RUN_LEVEL_FAILED)) {
    // Do not spam logs
    return 0;
  }

  log_info("%s: A component was loaded, resuming initialization.",
           component_name);

  if (g_run_level.load() == RUN_LEVEL_DETECT_RESOURCE) {
    telemetry_init_detect_resource(must_wait);
  }

  if (g_run_level.load() == RUN_LEVEL_DECODE_SECRET) {
    rc = telemetry_init_decode_secret(must_wait);
    if (rc != 0) {
      log_error("%s: Failed to decode secrets.", component_name);
    }
  }

  if (g_run_level.load() == RUN_LEVEL_CONFIGURE) {
    rc = telemetry_init_configure();
    if (rc != 0) {
      log_error("%s: Failed to configure.", component_name);
    }
  }

  return 0;
}

// clang-format off
BEGIN_SERVICE_IMPLEMENTATION(component_telemetry,
                             dynamic_loader_services_loaded_notification)
  services_loaded_notification
END_SERVICE_IMPLEMENTATION();
// clang-format on

static bool is_services_loaded_notification_registered = false;

constexpr const char *NOTIFICATION_SERVICE_NAME =
    "dynamic_loader_services_loaded_notification.component_telemetry";

int register_services_loaded_notification() {
  const void *service_implementation = &SERVICE_IMPLEMENTATION(
      component_telemetry, dynamic_loader_services_loaded_notification);
  auto service = reinterpret_cast<my_h_service>(
      const_cast<void *>(service_implementation));

  if (reg_reg->register_service(NOTIFICATION_SERVICE_NAME, service) != 0) {
    log_error("Failed to register notification service.");
    return 1;
  }

  is_services_loaded_notification_registered = true;
  return 0;
}

void unregister_services_loaded_notification() {
  if (is_services_loaded_notification_registered) {
    reg_reg->unregister(NOTIFICATION_SERVICE_NAME);
  }

  is_services_loaded_notification_registered = false;
}

mysql_service_status_t telemetry_init() {
  int rc;
  bool must_wait = false;

  g_run_level.store(RUN_LEVEL_BOOT);

  Log::init(log_srv, log_string_srv);

  log_info("%s: Starting ...", component_name);
  log_info("%s: Using opentelemetry-cpp API version %s", component_name,
           OPENTELEMETRY_VERSION);
  log_info("%s: Using opentelemetry-cpp SDK version %s", component_name,
           opentelemetry::sdk::version::full_version);

  log_info("%s: RUN_LEVEL = INSTALL", component_name);
  g_run_level.store(RUN_LEVEL_INSTALL);

  rc = telemetry_init_install();
  if (rc != 0) {
    goto telemetry_init_install_failed;
  }

  telemetry_init_detect_resource(must_wait);

  if (must_wait) {
    rc = register_services_loaded_notification();
    if (rc != 0) {
      log_error("%s: Failed to get service loaded notifications.",
                component_name);
      return 1;
    }
    log_info("%s: Waiting for a resource detection provider.", component_name);
    return 0;
  }

  rc = telemetry_init_decode_secret(must_wait);
  if (rc != 0) {
    /*
     * IMPORTANT:
     *
     * There is a chicken-and-eggs problem with components and system variables:
     * - to define a telemetry.xxx system variable,
     *   the telemetry component must be loaded,
     *   because the component itself provides the system variable metadata,
     * - to load a component,
     *   the content of PERSIST_ONLY system variables must be correct,
     *   otherwise initialization fails,
     * - to modify a broken system variable in PERSIST_ONLY,
     *   the component must be loaded.
     *
     * If initialization of the telemetry component fails,
     * for example due to a failure to read secrets from a secret provider,
     * the telemetry component is not functional.
     * In this case, we still allow INSTALL COMPONENT telemetry to proceed,
     * and set the component in noop mode.
     *
     * This allows to modify the PERSIST_ONLY configuration to correct it.
     */
    telemetry_init_noop_configure();
    log_error("%s: Failed to configure.", component_name);
    return 0;
  }

  if (must_wait) {
    rc = register_services_loaded_notification();
    if (rc != 0) {
      log_error("%s: Failed to get service loaded notifications.",
                component_name);
      return 1;
    }
    log_info("%s: Waiting for a secret decoder provider.", component_name);
    return 0;
  }

  rc = telemetry_init_configure();
  if (rc != 0) {
    goto telemetry_init_configure_failed;
  }

  log_info("%s: Started.", component_name);
  return 0;

telemetry_init_configure_failed:

  telemetry_deinit_install();

telemetry_init_install_failed:
  log_error("%s: Failed to start.", component_name);
  return 1;
}

mysql_service_status_t telemetry_init_install() {
  int rc;
  bool brc;

  g_shutting_down = false;
  g_sessions_closed = 0LL;
  g_tracer_provider = nullptr;
  g_tracer = nullptr;

  log_info("%s: Register performance schema ...", component_name);

  register_performance_schema();

  init_mutexes();

  log_info("%s: Register status variables ...", component_name);

  register_status_variables();

  log_info("%s: Register system variables ...", component_name);

  rc = register_system_variables();

  if (rc != 0) {
    goto telemetry_system_variables_failed;
  }

  setup_internal_logger();

  log_info("%s: Setup network namespaces ...", component_name);

  rc = setup_network_namespaces();

  if (rc != 0) {
    goto telemetry_network_namespaces_failed;
  }

  log_info("%s: Init option_usage ...", component_name);

  // We really need to do this while in the main
  // component INSTALL, and not later in a component load notification,
  // because of deadlocks in the component registry.
  brc = otel_component_option_usage_init();

  if (brc) {
    goto telemetry_option_usage_failed;
  }

  log_info("%s: RUN_LEVEL = DETECT_RESOURCE", component_name);
  g_run_level.store(RUN_LEVEL_DETECT_RESOURCE);
  return 0;

telemetry_option_usage_failed:
  otel_component_option_usage_deinit();

telemetry_network_namespaces_failed:
  cleanup_network_namespaces();

telemetry_system_variables_failed:
  unregister_system_variables();

  unregister_status_variables();

  cleanup_mutexes();

  return 1;
}

void telemetry_deinit_install() {
  otel_component_option_usage_deinit();
  cleanup_network_namespaces();
  unregister_system_variables();
  unregister_status_variables();
  cleanup_mutexes();
}

void telemetry_init_detect_resource(bool &must_wait) {
  log_info("%s: Setup resource ...", component_name);

  setup_resource(g_resource, must_wait);

  if (!must_wait) {
    log_info("%s: RUN_LEVEL = DECODE_SECRET", component_name);
    g_run_level.store(RUN_LEVEL_DECODE_SECRET);
  }
}

mysql_service_status_t telemetry_init_decode_secret(bool &must_wait) {
  log_info("%s: Reading secrets ...", component_name);
  int rc;

  rc = decode_secrets(must_wait);
  if (rc != 0) {
    log_info("%s: RUN_LEVEL = FAILED", component_name);
    g_run_level.store(RUN_LEVEL_FAILED);
    return 1;
  }

  if (!must_wait) {
    log_info("%s: RUN_LEVEL = CONFIGURE", component_name);
    g_run_level.store(RUN_LEVEL_CONFIGURE);
  }

  return 0;
}

mysql_service_status_t telemetry_init_configure() {
  int rc;

  log_info("%s: Setup tracer provider ...", component_name);

  g_tracer_provider = setup_otel_tracer_provider(g_resource);

  log_info("%s: Setup tracer ...", component_name);

  g_tracer = setup_otel_tracer(g_tracer_provider);

  if (sv_metrics_enabled) {
    log_info("%s: Setup meter provider ...", component_name);

#ifdef SINGLE_METER_PROVIDER
    g_meter_provider = setup_otel_meter_provider(g_resource);
#else
    setup_otel_meter_providers(g_resource);
#endif

    log_info("%s: Setup meters notification ...", component_name);

    setup_otel_meters_notification();

    log_info("%s: Setup meters ...", component_name);

    setup_otel_meters();
  } else {
    log_info("%s: Metrics are disabled, nothing to setup.", component_name);
  }

  log_info("%s: Setup logger provider ...", component_name);

  g_logger_provider = setup_otel_logger_provider(g_resource);

  log_info("%s: Setup logger ...", component_name);

  g_logger = setup_otel_logger(g_logger_provider);

  log_info("%s: Register telemetry slot ...", component_name);

  rc = register_telemetry_slot();

  if (rc != 0) {
    goto telemetry_slot_failed;
  }

  log_info("%s: Register notification ...", component_name);

  rc = register_notification_callback();

  if (rc != 0) {
    goto notification_callback_failed;
  }

  log_info("%s: Register telemetry ...", component_name);

  rc = register_telemetry_callback();

  if (rc != 0) {
    goto telemetry_callback_failed;
  }

  log_info("%s: Register telemetry logger ...", component_name);

  rc = register_telemetry_logger();

  if (rc != 0) {
    goto telemetry_logger_failed;
  }

  log_info("%s: Send control span ...", component_name);

  emit_control_span(sv_trace_enabled, sv_metrics_enabled, sv_log_enabled,
                    k_reason_startup);

  log_info("%s: Send control log ...", component_name);

  emit_control_log(sv_trace_enabled, sv_metrics_enabled, sv_log_enabled,
                   k_reason_startup);

  log_info("%s: RUN_LEVEL = READY", component_name);
  g_run_level.store(RUN_LEVEL_READY);
  return 0;

telemetry_logger_failed:
  unregister_telemetry_logger();

telemetry_callback_failed:
  unregister_telemetry_callback();

notification_callback_failed:
  unregister_notification_callback();

telemetry_slot_failed:
  unregister_telemetry_slot();

  g_tracer = nullptr;
  g_tracer_provider = nullptr;

  g_logger = nullptr;
  g_logger_provider = nullptr;

  log_error("%s: Failed to configure.", component_name);
  log_info("%s: RUN_LEVEL = FAILED", component_name);
  g_run_level.store(RUN_LEVEL_FAILED);
  return 1;
}

void telemetry_init_noop_configure() {
  g_tracer_provider = nullptr;
  g_tracer = nullptr;

  sv_metrics_enabled = false;

  g_logger_provider = nullptr;
  g_logger = nullptr;
}

mysql_service_status_t telemetry_deinit() {
  log_info("%s: Stopping ...", component_name);

  unregister_services_loaded_notification();

  // Done before emit_control_span() and emit_control_log(),
  // so that UNLOAD COMPONENT does not count as actual usage.
  otel_component_option_usage_deinit();

  emit_control_span(false, false, false, k_reason_shutdown);
  emit_control_log(false, false, false, k_reason_shutdown);
  g_shutting_down.store(true);

  log_info("%s: Unregister telemetry logger ...", component_name);

  unregister_telemetry_logger();

  log_info("%s: Unregister telemetry ...", component_name);

  unregister_telemetry_callback();

  log_info("%s: Aborting current session ...", component_name);

  abort_current_session();

  log_info("%s: Waiting for sessions ...", component_name);

  wait_for_sessions();

  log_info("%s: Unregister notification ...", component_name);

  unregister_notification_callback();

  log_info("%s: Unregister telemetry slot ...", component_name);

  unregister_telemetry_slot();

  if (g_tracer_provider != nullptr) {
    std::chrono::microseconds const flush_timeout(FLUSH_TIMEOUT_USEC);
    std::chrono::microseconds const close_timeout(CLOSE_TIMEOUT_USEC);
    log_info("%s: Flush tracer provider ...", component_name);
    g_tracer_provider->ForceFlush(flush_timeout);
    log_info("%s: Shutdown tracer provider ...", component_name);
    g_tracer_provider->Shutdown(close_timeout);
  }

  g_tracer = nullptr;
  g_tracer_provider = nullptr;

  if (sv_metrics_enabled) {
    log_info("%s: Cleanup meters notification ...", component_name);

    cleanup_otel_meters_notification();

    log_info("%s: Cleanup meters ...", component_name);

    cleanup_otel_meters();

#ifdef SINGLE_METER_PROVIDER
    g_meter_provider = nullptr;
#else
    cleanup_otel_meter_providers();
#endif
  } else {
    log_info("%s: Metrics are disabled, nothing to cleanup.", component_name);
  }

  if (g_logger_provider != nullptr) {
    std::chrono::microseconds const flush_timeout(FLUSH_TIMEOUT_USEC);
    std::chrono::microseconds const close_timeout(CLOSE_TIMEOUT_USEC);
    log_info("%s: Flush logger provider ...", component_name);
    g_logger_provider->ForceFlush(flush_timeout);
    log_info("%s: Shutdown logger provider ...", component_name);
    g_logger_provider->Shutdown(close_timeout);
  }

  g_logger = nullptr;
  g_logger_provider = nullptr;

  log_info("%s: Unregister system variables ...", component_name);

  unregister_system_variables();

  log_info("%s: Unregister status variables ...", component_name);

  unregister_status_variables();

  log_info("%s: Cleanup network namespaces ...", component_name);

  cleanup_network_namespaces();

  cleanup_mutexes();

  log_info("%s: Stopped.", component_name);
  return 0;
}

BEGIN_COMPONENT_PROVIDES(component_telemetry)
// optional: dynamic_loader_services_loaded_notification
END_COMPONENT_PROVIDES();

// clang-format off
BEGIN_COMPONENT_REQUIRES(component_telemetry)
  REQUIRES_SERVICE_AS(status_variable_registration, statvar_register_srv),
  REQUIRES_SERVICE_AS(component_sys_variable_register, sysvar_register_srv),
  REQUIRES_SERVICE_AS(component_sys_variable_unregister, sysvar_unregister_srv),
  REQUIRES_SERVICE_AS(log_builtins, log_srv),
  REQUIRES_SERVICE_AS(log_builtins_string, log_string_srv),
  REQUIRES_SERVICE_AS(pfs_notification_v3, notification_srv),
  REQUIRES_SERVICE_AS(mysql_current_thread_reader, current_thd_srv),
  REQUIRES_SERVICE_AS(mysql_connection_attributes_iterator, con_attr_srv),
  REQUIRES_SERVICE_AS(mysql_thd_store, thd_store_srv),
  REQUIRES_SERVICE_AS(mysql_server_telemetry_traces_v1, telemetry_traces_srv),
  REQUIRES_SERVICE_AS(mysql_server_telemetry_metrics_v1, telemetry_metrics_srv),
  REQUIRES_SERVICE_AS(mysql_server_telemetry_logs, telemetry_logs_srv),
  REQUIRES_SERVICE_AS(mysql_query_attributes_iterator, qa_iter_srv),
  REQUIRES_SERVICE_AS(mysql_query_attribute_string, qa_string_srv),
  REQUIRES_SERVICE_AS(mysql_string_get_data_in_charset, string_get_data_srv),
  REQUIRES_SERVICE_AS(mysql_string_factory, string_factory_srv),
  REQUIRES_SERVICE_AS(mysql_string_converter, string_converter_srv),
  REQUIRES_SERVICE_AS(mysql_cond_v1, cond_srv),
  REQUIRES_SERVICE_AS(mysql_mutex_v1, mutex_srv),
  REQUIRES_SERVICE_AS(psi_metric_v1, metric_srv),
  REQUIRES_SERVICE_AS(psi_stage_v1, stage_srv),
  REQUIRES_SERVICE_AS(psi_thread_v7, thread_srv),
  REQUIRES_SERVICE_AS(psi_idle_v1, idle_srv),
  REQUIRES_SERVICE_AS(registry_registration, reg_reg),
  REQUIRES_SERVICE_AS(registry, reg_srv),
END_COMPONENT_REQUIRES();
// clang-format on

// clang-format off
BEGIN_COMPONENT_METADATA(component_telemetry)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("mysql.license", "GPL"),
  METADATA("component_telemetry", "1"),
END_COMPONENT_METADATA();
// clang-format on

// clang-format off
DECLARE_COMPONENT(component_telemetry,
                  "mysql::component_telemetry")
  telemetry_init,
  telemetry_deinit
END_DECLARE_COMPONENT();
// clang-format on

// clang-format off
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(component_telemetry)
END_DECLARE_LIBRARY_COMPONENTS
// clang-format on

}  // namespace telemetry
