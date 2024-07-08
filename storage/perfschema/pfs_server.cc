/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_server.cc
  Private interface for the server (implementation).
*/

#include "storage/perfschema/pfs_server.h"

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "mysys_err.h"
#include "sql/mysqld.h"
#include "storage/perfschema/mysql_server_telemetry_logs_client_service_imp.h"
#include "storage/perfschema/mysql_server_telemetry_logs_service_imp.h"
#include "storage/perfschema/mysql_server_telemetry_metrics_service_imp.h"
#include "storage/perfschema/mysql_server_telemetry_traces_service_imp.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_defaults.h"
#include "storage/perfschema/pfs_digest.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_error.h"
#include "storage/perfschema/pfs_events_stages.h"
#include "storage/perfschema/pfs_events_statements.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_events_waits.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_metrics_service_imp.h"
#include "storage/perfschema/pfs_plugin_table.h"
#include "storage/perfschema/pfs_prepared_stmt.h"
#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_setup_object.h"
#include "storage/perfschema/pfs_timer.h"
#include "storage/perfschema/pfs_tls_channel.h"
#include "storage/perfschema/pfs_user.h"
#include "template_utils.h"

PFS_global_param pfs_param;

PFS_table_stat PFS_table_stat::g_reset_template;

static void cleanup_performance_schema();
void cleanup_instrument_config();
void cleanup_meter_config();
void cleanup_logger_config();

void pre_initialize_performance_schema() {
  record_main_thread_id();

  pfs_initialized = false;

  init_all_builtin_memory_class();

  PFS_table_stat::g_reset_template.reset();
  global_idle_stat.reset();
  global_table_io_stat.reset();
  global_table_lock_stat.reset();
  g_histogram_pico_timers.init();
  global_statements_histogram.reset();

  /*
    There is no automatic cleanup. Please either use:
    - my_thread_end()
    - or PSI_server->delete_current_thread()
    in the instrumented code, to explicitly cleanup the instrumentation.
  */
  THR_PFS = nullptr;
}

int initialize_performance_schema(
    PFS_global_param *param, PSI_thread_bootstrap **thread_bootstrap,
    PSI_mutex_bootstrap **mutex_bootstrap,
    PSI_rwlock_bootstrap **rwlock_bootstrap,
    PSI_cond_bootstrap **cond_bootstrap, PSI_file_bootstrap **file_bootstrap,
    PSI_socket_bootstrap **socket_bootstrap,
    PSI_table_bootstrap **table_bootstrap, PSI_mdl_bootstrap **mdl_bootstrap,
    PSI_idle_bootstrap **idle_bootstrap, PSI_stage_bootstrap **stage_bootstrap,
    PSI_statement_bootstrap **statement_bootstrap,
    PSI_transaction_bootstrap **transaction_bootstrap,
    PSI_memory_bootstrap **memory_bootstrap,
    PSI_error_bootstrap **error_bootstrap,
    PSI_data_lock_bootstrap **data_lock_bootstrap,
    PSI_system_bootstrap **system_bootstrap,
    PSI_tls_channel_bootstrap **tls_channel_bootstrap,
    PSI_metric_bootstrap **metric_bootstrap,
    PSI_logs_client_bootstrap **logs_client_bootstrap) {
  bool init_failed = false;

  *thread_bootstrap = nullptr;
  *mutex_bootstrap = nullptr;
  *rwlock_bootstrap = nullptr;
  *cond_bootstrap = nullptr;
  *file_bootstrap = nullptr;
  *socket_bootstrap = nullptr;
  *table_bootstrap = nullptr;
  *mdl_bootstrap = nullptr;
  *idle_bootstrap = nullptr;
  *stage_bootstrap = nullptr;
  *statement_bootstrap = nullptr;
  *transaction_bootstrap = nullptr;
  *memory_bootstrap = nullptr;
  *error_bootstrap = nullptr;
  *data_lock_bootstrap = nullptr;
  *system_bootstrap = nullptr;
  *tls_channel_bootstrap = nullptr;
  *metric_bootstrap = nullptr;
  *logs_client_bootstrap = nullptr;

  pfs_enabled = param->m_enabled;

  pfs_automated_sizing(param);
  init_timers();
  init_event_name_sizing(param);
  register_global_classes();

  if (init_sync_class(param->m_mutex_class_sizing, param->m_rwlock_class_sizing,
                      param->m_cond_class_sizing) ||
      init_thread_class(param->m_thread_class_sizing) ||
      init_table_share(param->m_table_share_sizing) ||
      init_table_share_lock_stat(param->m_table_lock_stat_sizing) ||
      init_table_share_index_stat(param->m_index_stat_sizing) ||
      init_file_class(param->m_file_class_sizing) ||
      init_stage_class(param->m_stage_class_sizing) ||
      init_statement_class(param->m_statement_class_sizing) ||
      init_socket_class(param->m_socket_class_sizing) ||
      init_memory_class(param->m_memory_class_sizing) ||
      init_instruments(param) ||
      init_events_waits_history_long(
          param->m_events_waits_history_long_sizing) ||
      init_events_stages_history_long(
          param->m_events_stages_history_long_sizing) ||
      init_events_statements_history_long(
          param->m_events_statements_history_long_sizing) ||
      init_events_transactions_history_long(
          param->m_events_transactions_history_long_sizing) ||
      init_file_hash(param) || init_table_share_hash(param) ||
      init_setup_actor(param) || init_setup_actor_hash(param) ||
      init_setup_object(param) || init_setup_object_hash(param) ||
      init_host(param) || init_host_hash(param) || init_user(param) ||
      init_user_hash(param) || init_account(param) ||
      init_account_hash(param) || init_digest(param) ||
      init_digest_hash(param) || init_program(param) ||
      init_program_hash(param) || init_prepared_stmt(param) ||
      init_meter_class(param->m_meter_class_sizing) ||
      init_metric_class(param->m_metric_class_sizing) ||
      init_logger_class(param->m_logger_class_sizing) || init_error(param)) {
    /*
      The performance schema initialization failed.
      Free the memory used, and disable the instrumentation.
    */
    cleanup_performance_schema();
    init_failed = true;
  }

  if (param->m_enabled && !init_failed) {
    /** Default values for SETUP_CONSUMERS */
    flag_events_stages_current =
        param->m_consumer_events_stages_current_enabled;
    flag_events_stages_history =
        param->m_consumer_events_stages_history_enabled;
    flag_events_stages_history_long =
        param->m_consumer_events_stages_history_long_enabled;
    flag_events_statements_cpu =
        param->m_consumer_events_statements_cpu_enabled;
    flag_events_statements_current =
        param->m_consumer_events_statements_current_enabled;
    flag_events_statements_history =
        param->m_consumer_events_statements_history_enabled;
    flag_events_statements_history_long =
        param->m_consumer_events_statements_history_long_enabled;
    flag_events_transactions_current =
        param->m_consumer_events_transactions_current_enabled;
    flag_events_transactions_history =
        param->m_consumer_events_transactions_history_enabled;
    flag_events_transactions_history_long =
        param->m_consumer_events_transactions_history_long_enabled;
    flag_events_waits_current = param->m_consumer_events_waits_current_enabled;
    flag_events_waits_history = param->m_consumer_events_waits_history_enabled;
    flag_events_waits_history_long =
        param->m_consumer_events_waits_history_long_enabled;
    flag_global_instrumentation =
        param->m_consumer_global_instrumentation_enabled;
    flag_thread_instrumentation =
        param->m_consumer_thread_instrumentation_enabled;
    flag_statements_digest = param->m_consumer_statement_digest_enabled;
  } else {
    flag_events_stages_current = false;
    flag_events_stages_history = false;
    flag_events_stages_history_long = false;
    flag_events_statements_cpu = false;
    flag_events_statements_current = false;
    flag_events_statements_history = false;
    flag_events_statements_history_long = false;
    flag_events_transactions_current = false;
    flag_events_transactions_history = false;
    flag_events_transactions_history_long = false;
    flag_events_waits_current = false;
    flag_events_waits_history = false;
    flag_events_waits_history_long = false;
    flag_global_instrumentation = false;
    flag_thread_instrumentation = false;
    flag_statements_digest = false;
  }

  if (!init_failed) {
    pfs_initialized = true;

    if (param->m_enabled) {
      install_default_setup(&pfs_thread_bootstrap);
      *thread_bootstrap = &pfs_thread_bootstrap;
      *mutex_bootstrap = &pfs_mutex_bootstrap;
      *rwlock_bootstrap = &pfs_rwlock_bootstrap;
      *cond_bootstrap = &pfs_cond_bootstrap;
      *file_bootstrap = &pfs_file_bootstrap;
      *socket_bootstrap = &pfs_socket_bootstrap;
      *table_bootstrap = &pfs_table_bootstrap;
      *mdl_bootstrap = &pfs_mdl_bootstrap;
      *idle_bootstrap = &pfs_idle_bootstrap;
      *stage_bootstrap = &pfs_stage_bootstrap;
      *statement_bootstrap = &pfs_statement_bootstrap;
      *transaction_bootstrap = &pfs_transaction_bootstrap;
      *memory_bootstrap = &pfs_memory_bootstrap;
      *error_bootstrap = &pfs_error_bootstrap;
      *data_lock_bootstrap = &pfs_data_lock_bootstrap;
      *system_bootstrap = &pfs_system_bootstrap;
      *tls_channel_bootstrap = &pfs_tls_channel_bootstrap;
      *metric_bootstrap = &pfs_metric_bootstrap;
      *logs_client_bootstrap = &pfs_logs_client_bootstrap;
    }
  }

  /*
    Initialize plugin table services.
    This must be done:
    - after the memory allocations for the mutex instrumentation,
      so that mutex LOCK_pfs_share_list gets instrumented
      (if the instrumentation is enabled),
    - in all cases, even when init_failed due to out of memory errors,
      as the plugin table service is independent of
      the main performance schema instrumentation.
  */
  init_pfs_plugin_table();

  /*
     Initialize TLS channel instrumentation data structures
    This must be done:
    - after the memory allocation for rwlock instrumentation,
      so that rwlock LOCK_pfs_tls_channel gets instrumented
      (if the instrumentation is enabled),
    - Even if the RWLOCK LOCK_pfs_tls_channels ends up not instrumented,
       it still needs to be initialized.
  */
  init_pfs_tls_channels_instrumentation();

  /*
     Initialize telemetry logs service.
    This must be done:
    - after the memory allocation for mutex instrumentation,
      so that mutex LOCK_pfs_logging_callback gets instrumented
      (if the instrumentation is enabled),
    - Even if the mutex LOCK_pfs_logging_callback ends up not instrumented,
       it still needs to be initialized.
  */
  initialize_mysql_server_telemetry_logs_service();

  /*
     Initialize telemetry logs client service.
  */
  initialize_mysql_server_telemetry_logs_client_service();

  /*
    Initialize telemetry metrics instrument service.
    This must be done:
    - after the memory allocation for rwlock instrumentation,
      so that rwlock LOCK_pfs_metrics gets instrumented
      (if the instrumentation is enabled),
    - Even if the rwlock LOCK_pfs_metrics ends up not instrumented,
       it still needs to be initialized.
  */
  initialize_mysql_server_metrics_instrument_service();

  /*
    Initialize telemetry metrics service.
  */
  initialize_mysql_server_telemetry_metrics_service();

  /*
    Initialize telemetry tracing service.
    This must be done:
    - after the memory allocation for mutex instrumentation,
      so that mutex LOCK_pfs_tracing_callback gets instrumented
      (if the instrumentation is enabled),
    - Even if the mutex LOCK_pfs_tracing_callback ends up not instrumented,
       it still needs to be initialized.
  */
  initialize_mysql_server_telemetry_traces_service();

  if (init_failed) {
    return 1;
  }

  return 0;
}

static void cleanup_performance_schema() {
  /*
    my.cnf options
  */

  cleanup_instrument_config();
  cleanup_meter_config();
  cleanup_logger_config();

  /*
    All the LF_HASH
  */

  cleanup_setup_actor_hash();
  cleanup_setup_object_hash();
  cleanup_account_hash();
  cleanup_host_hash();
  cleanup_user_hash();
  cleanup_program_hash();
  cleanup_table_share_hash();
  cleanup_file_hash();
  cleanup_digest_hash();

  /*
    Then the lookup tables
  */

  cleanup_setup_actor();
  cleanup_setup_object();

  /*
    Then the history tables
  */

  cleanup_events_waits_history_long();
  cleanup_events_stages_history_long();
  cleanup_events_statements_history_long();
  cleanup_events_transactions_history_long();

  /*
    Then the various aggregations
  */

  cleanup_digest();
  cleanup_account();
  cleanup_host();
  cleanup_user();

  /*
    Then the instrument classes.
    Once a class is cleaned up,
    find_XXX_class(key)
    will return PSI_NOT_INSTRUMENTED
  */
  cleanup_mysql_server_telemetry_metrics_service();
  cleanup_mysql_server_metrics_instrument_service();
  cleanup_mysql_server_telemetry_traces_service();
  cleanup_mysql_server_telemetry_logs_service();
  cleanup_mysql_server_telemetry_logs_client_service();
  cleanup_pfs_tls_channels_instrumentation();
  cleanup_pfs_plugin_table();
  cleanup_error();
  cleanup_program();
  cleanup_prepared_stmt();
  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_table_share();
  cleanup_table_share_lock_stat();
  cleanup_table_share_index_stat();
  cleanup_file_class();
  cleanup_stage_class();
  cleanup_statement_class();
  cleanup_socket_class();
  cleanup_memory_class();
  cleanup_meter_class();
  cleanup_metric_class();
  cleanup_logger_class();

  cleanup_instruments();
}

void shutdown_performance_schema() {
  pfs_initialized = false;

  /* disable everything, especially for this thread. */
  flag_events_stages_current = false;
  flag_events_stages_history = false;
  flag_events_stages_history_long = false;
  flag_events_statements_cpu = false;
  flag_events_statements_current = false;
  flag_events_statements_history = false;
  flag_events_statements_history_long = false;
  flag_events_transactions_current = false;
  flag_events_transactions_history = false;
  flag_events_transactions_history_long = false;
  flag_events_waits_current = false;
  flag_events_waits_history = false;
  flag_events_waits_history_long = false;
  flag_global_instrumentation = false;
  flag_thread_instrumentation = false;
  flag_statements_digest = false;

  global_table_io_class.m_enabled = false;
  global_table_lock_class.m_enabled = false;
  global_idle_class.m_enabled = false;
  global_metadata_class.m_enabled = false;
  global_error_class.m_enabled = false;
  global_transaction_class.m_enabled = false;

  cleanup_performance_schema();
}

/**
  Initialize the dynamic array used to hold PFS_INSTRUMENT configuration
  options.
*/
void init_pfs_instrument_array() {
  pfs_instr_config_array = new Pfs_instr_config_array(PSI_NOT_INSTRUMENTED);
}

/**
  Deallocate the PFS_INSTRUMENT array.
*/
void cleanup_instrument_config() {
  if (pfs_instr_config_array != nullptr) {
    my_free_container_pointers(*pfs_instr_config_array);
  }
  delete pfs_instr_config_array;
  pfs_instr_config_array = nullptr;
}

/**
  Process one performance_schema_instrument configuration string. Isolate the
  instrument name, evaluate the option value, and store them in a dynamic array.
  Return 'false' for success, 'true' for error.

  @param name    Instrument name
  @param value   Configuration option: 'on', 'off', etc.
  @return 0 for success, non zero for errors
*/

int add_pfs_instr_to_array(const char *name, const char *value) {
  const size_t name_length = strlen(name);
  const size_t value_length = strlen(value);

  /* Allocate structure plus string buffers plus null terminators */
  auto *e = (PFS_instr_config *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(PFS_instr_config) + name_length + 1 + value_length + 1,
      MYF(MY_WME));
  if (!e) {
    return 1;
  }

  /* Copy the instrument name */
  e->m_name = (char *)e + sizeof(PFS_instr_config);
  memcpy(e->m_name, name, name_length);
  e->m_name_length = (uint)name_length;
  e->m_name[name_length] = '\0';

  /* Set flags accordingly */
  if (!my_strcasecmp(&my_charset_latin1, value, "counted")) {
    e->m_enabled = true;
    e->m_timed = false;
  } else if (!my_strcasecmp(&my_charset_latin1, value, "true") ||
             !my_strcasecmp(&my_charset_latin1, value, "on") ||
             !my_strcasecmp(&my_charset_latin1, value, "1") ||
             !my_strcasecmp(&my_charset_latin1, value, "yes")) {
    e->m_enabled = true;
    e->m_timed = true;
  } else if (!my_strcasecmp(&my_charset_latin1, value, "false") ||
             !my_strcasecmp(&my_charset_latin1, value, "off") ||
             !my_strcasecmp(&my_charset_latin1, value, "0") ||
             !my_strcasecmp(&my_charset_latin1, value, "no")) {
    e->m_enabled = false;
    e->m_timed = false;
  } else {
    my_free(e);
    return 1;
  }

  /* Add to the array of default startup options */
  if (pfs_instr_config_array->push_back(e)) {
    my_free(e);
    return 1;
  }

  return 0;
}

/**
  Initialize the dynamic array used to hold PFS_METER configuration
  options.
*/
void init_pfs_meter_array() {
  pfs_meter_config_array = new Pfs_meter_config_array(PSI_NOT_INSTRUMENTED);
}

/**
  Deallocate the PFS_METER array.
*/
void cleanup_meter_config() {
  if (pfs_meter_config_array != nullptr) {
    my_free_container_pointers(*pfs_meter_config_array);
  }
  delete pfs_meter_config_array;
  pfs_meter_config_array = nullptr;
}

/**
  Initialize the dynamic array used to hold PFS_LOGGER configuration
  options.
*/
void init_pfs_logger_array() {
  pfs_logger_config_array = new Pfs_logger_config_array(PSI_NOT_INSTRUMENTED);
}

/**
  Deallocate the PFS_LOGGER array.
*/
void cleanup_logger_config() {
  if (pfs_logger_config_array != nullptr) {
    my_free_container_pointers(*pfs_logger_config_array);
  }
  delete pfs_logger_config_array;
  pfs_logger_config_array = nullptr;
}

/**
  Process one performance_schema_meter configuration string. Isolate the
  instrument name, evaluate the option values, and store them in a dynamic
  array. Return 'false' for success, 'true' for error.

  @param name    Instrument name
  @param value   Configuration option: 'enabled:ON,frequency:30', 'enabled:OFF',
  etc.
  @return 0 for success, non zero for errors
*/

int add_pfs_meter_to_array(const char *name, const char *value) {
  const size_t name_length = strlen(name);
  const size_t value_length = strlen(value);

  /* Allocate structure plus string buffers plus null terminators */
  auto *e = (PFS_meter_config *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(PFS_meter_config) + name_length + 1 + value_length + 1,
      MYF(MY_WME));
  if (!e) {
    return 1;
  }

  /* Copy the meter instrument name */
  e->m_name = (char *)e + sizeof(PFS_meter_config);

  memcpy(e->m_name, name, name_length);
  e->m_name_length = (uint)name_length;
  e->m_name[name_length] = '\0';

  // init state
  e->m_enabled_set = false;
  e->m_frequency_set = false;

  /*
   Value string must have a "<property1>:<value1>,<property2>:<value2>" form,
   for example "enabled:ON,frequency:30", split it into pieces.
  */
  bool more_properties = true;
  while (more_properties) {
    const char *val_delimiter = strchr(value, ':');
    if (val_delimiter == nullptr) {
      my_free(e);
      return 1;
    }
    const char *property_name = value;
    const size_t name_len = val_delimiter - value;

    const char *property_value = val_delimiter + 1;
    const char *value_end = strchr(property_value, ',');
    if (value_end == nullptr) {
      value_end = property_value + strlen(property_value);
      more_properties = false;
    }
    const size_t val_len = value_end - property_value;

    if (name_len == strlen("enabled") &&
        (!strncmp(property_name, "enabled", name_len) ||
         !strncmp(property_name, "ENABLED", name_len))) {
      // parse "enabled" property value
      if (!strncmp(property_value, "true", val_len) ||
          !strncmp(property_value, "TRUE", val_len) ||
          !strncmp(property_value, "1", val_len) ||
          !strncmp(property_value, "on", val_len) ||
          !strncmp(property_value, "ON", val_len) ||
          !strncmp(property_value, "yes", val_len) ||
          !strncmp(property_value, "YES", val_len)) {
        e->m_enabled = true;
        e->m_enabled_set = true;
      } else if (!strncmp(property_value, "false", val_len) ||
                 !strncmp(property_value, "FALSE", val_len) ||
                 !strncmp(property_value, "0", val_len) ||
                 !strncmp(property_value, "off", val_len) ||
                 !strncmp(property_value, "OFF", val_len) ||
                 !strncmp(property_value, "no", val_len) ||
                 !strncmp(property_value, "NO", val_len)) {
        e->m_enabled = false;
        e->m_enabled_set = true;
      } else {
        // unsupported value for "enabled" property
        my_free(e);
        return 1;
      }

    } else if (name_len == strlen("frequency") &&
               (!strncmp(property_name, "frequency", name_len) ||
                !strncmp(property_name, "FREQUENCY", name_len))) {
      // parse "frequency" property value
      char *end = nullptr;
      unsigned long val = std::strtoul(property_value, &end, 10);
      if (val > UINT_MAX || end == property_value) {
        // out of range or parse error
        my_free(e);
        return 1;
      }
      e->m_frequency = val;
      e->m_frequency_set = true;

    } else {
      // unknown property name
      my_free(e);
      return 1;
    }

    value = value_end + 1;
  }

  /* Add to the array of default startup options */
  if (pfs_meter_config_array->push_back(e)) {
    my_free(e);
    return 1;
  }

  return 0;
}

/**
  Process one performance_schema_logger configuration string. Isolate the
  instrument name, evaluate the option values, and store them in a dynamic
  array. Return 'false' for success, 'true' for error.

  @param name    Instrument name
  @param value   Configuration option: 'level:INFO', 'level:NONE', etc.
  @return 0 for success, non zero for errors
*/

int add_pfs_logger_to_array(const char *name, const char *value) {
  const size_t name_length = strlen(name);
  const size_t value_length = strlen(value);

  /* Allocate structure plus string buffers plus null terminators */
  auto *e = (PFS_logger_config *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(PFS_logger_config) + name_length + 1 + value_length + 1,
      MYF(MY_WME));
  if (!e) {
    return 1;
  }

  /* Copy the logger instrument name */
  e->m_name = (char *)e + sizeof(PFS_logger_config);
  memcpy(e->m_name, name, name_length);
  e->m_name_length = (uint)name_length;
  e->m_name[name_length] = '\0';

  /*
   Value string must have a "<property_name>:<property value>" form,
   for example "level:INFO", split it into pieces.
  */
  const char *val_delimiter = strchr(value, ':');
  if (val_delimiter == nullptr) {
    my_free(e);
    return 1;
  }
  const size_t val_len = val_delimiter - value;
  const char *property_name = value;
  const char *property_value = val_delimiter + 1;

  /* must match the only supported property - level */
  if (val_len != strlen("level") ||
      (strncmp(property_name, "level", val_len) &&
       strncmp(property_name, "LEVEL", val_len))) {
    my_free(e);
    return 1;
  }

  /* Set flags accordingly */
  if (!my_strcasecmp(&my_charset_latin1, property_value, "error")) {
    e->m_level = OTELLogLevel::TLOG_ERROR;
  } else if (!my_strcasecmp(&my_charset_latin1, property_value, "warn")) {
    e->m_level = OTELLogLevel::TLOG_WARN;
  } else if (!my_strcasecmp(&my_charset_latin1, property_value, "info")) {
    e->m_level = OTELLogLevel::TLOG_INFO;
  } else if (!my_strcasecmp(&my_charset_latin1, property_value, "debug")) {
    e->m_level = OTELLogLevel::TLOG_DEBUG;
  } else if (!my_strcasecmp(&my_charset_latin1, property_value, "none")) {
    e->m_level = OTELLogLevel::TLOG_NONE;
  } else {
    my_free(e);
    return 1;
  }

  /* Add to the array of default startup options */
  if (pfs_logger_config_array->push_back(e)) {
    my_free(e);
    return 1;
  }

  return 0;
}
