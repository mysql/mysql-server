/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#include <cassert>
#include <sstream>

#include <mysql/components/services/log_builtins.h>
#include <mysql/service_rpl_transaction_write_set.h>
#include "mutex_lock.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "plugin/group_replication/include/autorejoin.h"
#include "plugin/group_replication/include/consistency_manager.h"
#include "plugin/group_replication/include/gcs_mysql_network_provider.h"
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/observer_server_actions.h"
#include "plugin/group_replication/include/observer_server_state.h"
#include "plugin/group_replication/include/observer_trans.h"
#include "plugin/group_replication/include/perfschema/pfs.h"
#include "plugin/group_replication/include/pipeline_stats.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/consensus_leaders_handler.h"
#include "plugin/group_replication/include/plugin_handlers/member_actions_handler.h"
#include "plugin/group_replication/include/plugin_variables.h"
#include "plugin/group_replication/include/plugin_variables/recovery_endpoints.h"
#include "plugin/group_replication/include/services/message_service/message_service.h"
#include "plugin/group_replication/include/services/status_service/status_service.h"
#include "plugin/group_replication/include/sql_service/sql_service_interface.h"
#include "plugin/group_replication/include/thread/mysql_thread.h"
#include "plugin/group_replication/include/udf/udf_registration.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

#ifndef NDEBUG
#include "plugin/group_replication/include/services/notification/impl/gms_listener_test.h"
#endif

using std::string;

/*
  Variables that are only accessible inside plugin.cc.
*/
static struct plugin_local_variables lv;

/*
  Plugin options variables that are only accessible inside plugin.cc.
*/
static struct plugin_options_variables ov;

/*
  Log service log_bi and log_bs are extern variables.
*/
SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

/*
  Plugin modules.

  plugin.cc class pointers that are accessible on all plugin files,
  that is, are declared as extern on plugin.h.
  These pointers will be initialized on plugin_group_replication_init()
  or plugin_group_replication_start().
*/
/** The plugin applier */
Applier_module *applier_module = nullptr;
/** The plugin recovery module */
Recovery_module *recovery_module = nullptr;
/** The plugin group communication module */
Gcs_operations *gcs_module = nullptr;
/** The registry module */
Registry_module_interface *registry_module = nullptr;
/** The observation module for group events */
Group_events_observation_manager *group_events_observation_manager = nullptr;
/** The channel observation modules */
Channel_observation_manager_list *channel_observation_manager_list = nullptr;
/** The Single primary channel observation module */
Asynchronous_channels_state_observer *asynchronous_channels_state_observer =
    nullptr;
/** The transaction observation module */
Group_transaction_observation_manager *group_transaction_observation_manager =
    nullptr;
/** Transactions latch */
Wait_ticket<my_thread_id> *transactions_latch = nullptr;
/** The plugin transaction consistency manager */
Transaction_consistency_manager *transaction_consistency_manager = nullptr;
/** Class to coordinate access to the plugin stop lock */
Shared_writelock *shared_plugin_stop_lock = nullptr;
/** Initialization thread for server starts */
Delayed_initialization_thread *delayed_initialization_thread = nullptr;
/** The transaction handler for network partitions */
Group_partition_handling *group_partition_handler = nullptr;
/** The handler for transaction killing when an error or partition happens */
Blocked_transaction_handler *blocked_transaction_handler = nullptr;
/** The coordinator for group actions */
Group_action_coordinator *group_action_coordinator = nullptr;
/** The primary election handler */
Primary_election_handler *primary_election_handler = nullptr;
/** The thread that handles the auto-rejoin process */
Autorejoin_thread *autorejoin_module = nullptr;
/** The handler to invoke clone */
Remote_clone_handler *remote_clone_handler = nullptr;
/** The thread that handles the message service process */
Message_service_handler *message_service_handler = nullptr;
/** Handle validation of advertised recovery endpoints */
Advertised_recovery_endpoints *advertised_recovery_endpoints = nullptr;
Member_actions_handler *member_actions_handler = nullptr;
/** Handle tasks on mysql_thread */
Mysql_thread *mysql_thread_handler = nullptr;
/**
  Dedicated mysql_thread to enable `read_only` and `super_read_only`
  since these are blocking operations.
  If we did use `mysql_thread_handler` that would block all other
  other operations until read modes operations complete.
*/
Mysql_thread *mysql_thread_handler_read_only_mode = nullptr;
/** Module with the acquired server services on plugin install */
Server_services_references *server_services_references_module = nullptr;

Plugin_gcs_events_handler *events_handler = nullptr;
Plugin_gcs_view_modification_notifier *view_change_notifier = nullptr;

/* Group management information */
Group_member_info_manager_interface *group_member_mgr = nullptr;
Group_member_info *local_member_info = nullptr;

/*Compatibility management*/
Compatibility_module *compatibility_mgr = nullptr;

/* Runtime error service */
SERVICE_TYPE_NO_CONST(mysql_runtime_error) *mysql_runtime_error_service =
    nullptr;

Consensus_leaders_handler *consensus_leaders_handler = nullptr;

/* Performance schema module */
static gr::perfschema::Perfschema_module *perfschema_module = nullptr;
static bool initialize_perfschema_module();
static void finalize_perfschema_module();

/*
  Internal auxiliary functions signatures.
*/
static bool check_uuid_against_rpl_channel_settings(const char *str);

static int check_group_name_string(const char *str, bool is_var_update = false);

static int check_recovery_ssl_string(const char *str, const char *var_name,
                                     bool is_var_update = false);

static int check_if_server_properly_configured();

static bool init_group_sidno();

static void initialize_ssl_option_map();

static bool initialize_registry_module();

static bool finalize_registry_module();

static int check_flow_control_min_quota_long(longlong value,
                                             bool is_var_update = false);

static int check_flow_control_min_recovery_quota_long(
    longlong value, bool is_var_update = false);

static int check_flow_control_max_quota_long(longlong value,
                                             bool is_var_update = false);

static int check_view_change_uuid_string(const char *str,
                                         bool is_var_update = false);

int configure_group_communication();
int build_gcs_parameters(Gcs_interface_parameters &params);
int configure_group_member_manager();
bool check_async_channel_running_on_secondary();
int configure_compatibility_manager();
int initialize_recovery_module();
int configure_and_start_applier_module();
void initialize_asynchronous_channels_observer();
void initialize_group_partition_handler();
int start_group_communication();
int leave_group_and_terminate_plugin_modules(
    gr_modules::mask modules_to_terminate, char **error_message);
int leave_group();
int terminate_applier_module();
int terminate_recovery_module();
void terminate_asynchronous_channels_observer();
void set_auto_increment_handler_values();

static void option_deprecation_warning(MYSQL_THD thd, const char *old_name,
                                       const char *new_name) {
  push_deprecated_warn(thd, old_name, new_name);
}

static void check_deprecated_variables() {
  MYSQL_THD thd = lv.plugin_is_auto_starting_on_install ? nullptr : current_thd;
  if (ov.ip_whitelist_var != nullptr &&
      strcmp(ov.ip_whitelist_var, "AUTOMATIC")) {
    option_deprecation_warning(thd, "group_replication_ip_whitelist",
                               "group_replication_ip_allowlist");
  }
}

static const char *get_ip_allowlist() {
  std::string whitelist(ov.ip_whitelist_var);
  std::string allowlist(ov.ip_allowlist_var);
  std::transform(whitelist.begin(), whitelist.end(), whitelist.begin(),
                 ::tolower);
  std::transform(allowlist.begin(), allowlist.end(), allowlist.begin(),
                 ::tolower);

  return allowlist.compare("automatic")
             ? ov.ip_allowlist_var  // ip_allowlist_var is set
             : whitelist.compare("automatic")
                   ? ov.ip_whitelist_var   // ip_whitelist_var is set
                   : ov.ip_allowlist_var;  // both are not set
}
/*
  Auxiliary public functions.
*/
void *get_plugin_pointer() { return lv.plugin_info_ptr; }

Checkable_rwlock *get_plugin_running_lock() { return lv.plugin_running_lock; }

mysql_mutex_t *get_plugin_applier_module_initialize_terminate_lock() {
  return &lv.plugin_applier_module_initialize_terminate_mutex;
}

bool plugin_is_group_replication_running() {
  return lv.group_replication_running;
}

bool plugin_is_group_replication_cloning() {
  return lv.group_replication_cloning;
}

bool is_plugin_auto_starting_on_non_bootstrap_member() {
  return !ov.bootstrap_group_var && lv.plugin_is_auto_starting_on_boot;
}

bool is_plugin_configured_and_starting() {
  return lv.group_member_mgr_configured;
}

int plugin_group_replication_set_retrieved_certification_info(void *info) {
  return recovery_module->set_retrieved_cert_info(info);
}

rpl_sidno get_group_sidno() {
  assert(lv.group_sidno > 0);
  return lv.group_sidno;
}

rpl_sidno get_view_change_sidno() {
  assert(lv.view_change_sidno > 0);
  return lv.view_change_sidno;
}

bool get_plugin_is_stopping() { return lv.plugin_is_stopping; }

bool get_wait_on_engine_initialization() {
  return lv.wait_on_engine_initialization;
}

void enable_server_shutdown_status() { lv.server_shutdown_status = true; }

bool get_server_shutdown_status() { return lv.server_shutdown_status; }

void set_plugin_is_setting_read_mode(bool value) {
  lv.plugin_is_setting_read_mode = value;
}

bool get_plugin_is_setting_read_mode() {
  return lv.plugin_is_setting_read_mode;
}

const char *get_group_name_var() { return ov.group_name_var; }

const char *get_view_change_uuid_var() { return ov.view_change_uuid_var; }

ulong get_exit_state_action_var() { return ov.exit_state_action_var; }

ulong get_flow_control_mode_var() { return ov.flow_control_mode_var; }

long get_flow_control_certifier_threshold_var() {
  return ov.flow_control_certifier_threshold_var;
}

long get_flow_control_applier_threshold_var() {
  return ov.flow_control_applier_threshold_var;
}

long get_flow_control_min_quota_var() { return ov.flow_control_min_quota_var; }

long get_flow_control_min_recovery_quota_var() {
  return ov.flow_control_min_recovery_quota_var;
}

long get_flow_control_max_quota_var() { return ov.flow_control_max_quota_var; }

int get_flow_control_member_quota_percent_var() {
  return ov.flow_control_member_quota_percent_var;
}

int get_flow_control_period_var() { return ov.flow_control_period_var; }

int get_flow_control_hold_percent_var() {
  return ov.flow_control_hold_percent_var;
}

int get_flow_control_release_percent_var() {
  return ov.flow_control_release_percent_var;
}

ulong get_components_stop_timeout_var() {
  return ov.components_stop_timeout_var;
}

ulong get_communication_stack_var() { return ov.communication_stack_var; }

bool is_autorejoin_enabled() { return ov.autorejoin_tries_var > 0U; }

uint get_number_of_autorejoin_tries() { return ov.autorejoin_tries_var; }

ulonglong get_rejoin_timeout() { return lv.rejoin_timeout; }

bool get_allow_single_leader() {
  if (lv.allow_single_leader_latch.first)
    return lv.allow_single_leader_latch.second;
  else
    return ov.allow_single_leader_var;
}

/**
 * @brief Callback implementation of
 * handle_group_replication_incoming_connection. This is the entry point for
 * new MySQL connections that are directed to GCS
 *
 * @param thd     THD object of the connection
 * @param fd      File descriptor of the connections
 * @param ssl_ctx SSL data of the connection
 */
void handle_group_replication_incoming_connection(THD *thd, int fd,
                                                  SSL *ssl_ctx) {
  auto *new_connection = new Network_connection(fd, ssl_ctx);
  new_connection->has_error = false;

  Gcs_mysql_network_provider *mysql_provider =
      gcs_module->get_mysql_network_provider();

  if (mysql_provider) {
    mysql_provider->set_new_connection(thd, new_connection);
  }
}

/**
  Set condition to block or unblock the calling threads

  @param[in] cond  if the threads should be blocked or not
*/
void set_wait_on_start_process(bool cond) {
  lv.online_wait_mutex->set_wait_lock(cond);
}

/**
  Blocks the calling thread
*/
enum_wait_on_start_process_result initiate_wait_on_start_process() {
  // block the thread
  lv.online_wait_mutex->start_waitlock();

#ifndef NDEBUG
  DBUG_EXECUTE_IF("group_replication_wait_thread_for_server_online", {
    const char act[] =
        "now wait_for signal.continue_applier_thread NO_CLEAR_EVENT";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
#endif
  return lv.wait_on_start_process;
}

/**
  Release all the blocked threads
*/
void terminate_wait_on_start_process(enum_wait_on_start_process_result abort) {
  lv.plugin_is_auto_starting_on_boot = false;
  lv.wait_on_start_process = abort;

  // unblocked waiting threads
  lv.online_wait_mutex->end_wait_lock();
}

static void finalize_perfschema_module() {
  if (nullptr != perfschema_module) {
    perfschema_module->finalize();
    delete perfschema_module;
    perfschema_module = nullptr;
  }
}

static bool initialize_perfschema_module() {
  if (nullptr != perfschema_module) {
    return true; /* purecov: inspected */
  }

  perfschema_module = new gr::perfschema::Perfschema_module{};
  if (nullptr == perfschema_module) {
    return true; /* purecov: inspected */
  }

  if (perfschema_module->initialize()) {
    /* purecov: begin inspected */
    finalize_perfschema_module();
    return true;
    /* purecov: end */
  }

  return false;
}

static bool initialize_registry_module() {
  return (!(registry_module = new Registry_module()) ||
          registry_module->initialize());
}

static bool finalize_registry_module() {
  int res = false;
  if (registry_module) {
    res = registry_module->finalize();
    delete registry_module;
    registry_module = nullptr;
  }
  return res;
}

/*
  Plugin interface.
*/
struct st_mysql_group_replication group_replication_descriptor = {
    MYSQL_GROUP_REPLICATION_INTERFACE_VERSION,
    plugin_group_replication_start,
    plugin_group_replication_stop,
    plugin_is_group_replication_running,
    plugin_is_group_replication_cloning,
    plugin_group_replication_set_retrieved_certification_info,
    plugin_get_connection_status,
    plugin_get_group_members,
    plugin_get_group_member_stats,
    plugin_get_group_members_number,
};

bool plugin_get_connection_status(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS &callbacks) {
  char *channel_name = applier_module_channel_name;

  return get_connection_status(callbacks, ov.group_name_var, channel_name,
                               plugin_is_group_replication_running());
}

bool plugin_get_group_members(
    uint index, const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS &callbacks) {
  char *channel_name = applier_module_channel_name;

  return get_group_members_info(index, callbacks, channel_name);
}

/*
  If the local member is already OFFLINE but still has the previous
  membership because is waiting for the leave view, do not report
  the other members.
*/
uint plugin_get_group_members_number() {
  bool unitialized_or_offline = group_member_mgr == nullptr ||
                                local_member_info == nullptr ||
                                local_member_info->get_recovery_status() ==
                                    Group_member_info::MEMBER_OFFLINE;

  return unitialized_or_offline
             ? 1
             : (uint)group_member_mgr->get_number_of_members();
}

bool plugin_get_group_member_stats(
    uint index,
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS &callbacks) {
  char *channel_name = applier_module_channel_name;

  return get_group_member_stats(index, callbacks, channel_name);
}

int plugin_group_replication_start(char **error_message) {
  DBUG_TRACE;

  if (lv.plugin_is_being_uninstalled) {
    std::string err_msg("Group Replication plugin is being uninstalled.");
    *error_message =
        (char *)my_malloc(PSI_NOT_INSTRUMENTED, err_msg.length() + 1, MYF(0));
    strcpy(*error_message, err_msg.c_str());
    return GROUP_REPLICATION_COMMAND_FAILURE;
  }

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::WRITE_LOCK);
  int error = 0;

  std::string debug_options;

  DBUG_EXECUTE_IF("group_replication_wait_on_start", {
    const char act[] =
        "now signal signal.start_waiting wait_for signal.start_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  if (plugin_is_group_replication_running()) {
    error = GROUP_REPLICATION_ALREADY_RUNNING;
    goto err;
  }

  if (check_if_server_properly_configured()) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (check_group_name_string(ov.group_name_var)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (check_view_change_uuid_string(ov.view_change_uuid_var)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (check_recovery_ssl_string(ov.recovery_ssl_ca_var, "ssl_ca") ||
      check_recovery_ssl_string(ov.recovery_ssl_capath_var, "ssl_capath") ||
      check_recovery_ssl_string(ov.recovery_ssl_cert_var, "ssl_cert_pointer") ||
      check_recovery_ssl_string(ov.recovery_ssl_cipher_var,
                                "ssl_cipher_pointer") ||
      check_recovery_ssl_string(ov.recovery_ssl_key_var, "ssl_key_pointer") ||
      check_recovery_ssl_string(ov.recovery_ssl_crl_var, "ssl_crl_pointer") ||
      check_recovery_ssl_string(ov.recovery_ssl_crlpath_var,
                                "ssl_crlpath_pointer") ||
      check_recovery_ssl_string(ov.recovery_public_key_path_var,
                                "public_key_path") ||
      check_recovery_ssl_string(ov.recovery_tls_version_var, "tls_version") ||
      check_recovery_ssl_string(ov.recovery_tls_ciphersuites_var,
                                "tls_ciphersuites")) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (!ov.start_group_replication_at_boot_var && !server_engine_initialized()) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_START_WITH_INVALID_SERVER_ID);
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (ov.force_members_var != nullptr && strlen(ov.force_members_var) > 0) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FORCE_MEMBERS_MUST_BE_EMPTY,
                 ov.force_members_var);
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (check_flow_control_min_quota_long(ov.flow_control_min_quota_var)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (check_flow_control_min_recovery_quota_long(
          ov.flow_control_min_recovery_quota_var)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (check_flow_control_max_quota_long(ov.flow_control_max_quota_var)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (init_group_sidno()) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR; /* purecov: inspected */
    goto err;
  }

  if (advertised_recovery_endpoints->check(
          ov.advertise_recovery_endpoints_var,
          !server_engine_initialized()
              ? Advertised_recovery_endpoints::enum_log_context::ON_BOOT
              : Advertised_recovery_endpoints::enum_log_context::ON_START)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_IS_STARTING);

  DBUG_EXECUTE_IF("register_gms_listener_example",
                  { register_listener_service_gr_example(); });

  /*
    The debug options is also set/verified here because if it was set during
    the server start, it was not set/verified due to the plugin life-cycle.
    For that reason, we have to call set_debug_options here as well to set/
    validate the information in the communication_debug_options_var. Note,
    however, that the option variable is not automatically set to a valid
    value if the validation fails.
  */
  debug_options.assign(ov.communication_debug_options_var);
  if (gcs_module->set_debug_options(debug_options)) {
    error = GROUP_REPLICATION_CONFIGURATION_ERROR; /* purecov: inspected */
    goto err;
  }

  check_deprecated_variables();

  assert(transactions_latch->empty());

  // Reset the single-leader latch flag
  lv.allow_single_leader_latch.first = false;

  // Reset the coordinator in case there was a previous stop.
  group_action_coordinator->reset_coordinator_process();

  // GR delayed initialization.
  if (!server_engine_initialized()) {
    lv.wait_on_engine_initialization = true;
    lv.plugin_is_auto_starting_on_install = false;

    delayed_initialization_thread = new Delayed_initialization_thread();
    if (delayed_initialization_thread->launch_initialization_thread()) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_PLUGIN_STRUCT_INIT_NOT_POSSIBLE_ON_SERVER_START);
      delete delayed_initialization_thread;
      delayed_initialization_thread = nullptr;
      error = GROUP_REPLICATION_CONFIGURATION_ERROR;
      goto err;
      /* purecov: end */
    }

    goto err;  // leave the decision for later
  }

  error = initialize_plugin_and_join(PSESSION_DEDICATED_THREAD, nullptr);
  if (!error) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_HAS_STARTED);
  }
  return error;

err:

  if (error) {
    // end wait for thread waiting for server to start
    terminate_wait_on_start_process();
  } else {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_HAS_STARTED);
  }

  return error;
}

int initialize_plugin_and_join(
    enum_plugin_con_isolation sql_api_isolation,
    Delayed_initialization_thread *delayed_init_thd) {
  DBUG_TRACE;
  lv.plugin_running_lock->assert_some_wrlock();

  int error = 0;

  // Avoid unnecessary operations
  bool enabled_super_read_only = false;
  bool read_only_mode = false, super_read_only_mode = false;
  bool write_set_limits_set = false;

  /*
    Despite START GROUP_REPLICATION do not depend on the SQL API,
    other operations, like distributed recovery through clone, do
    depend, as such we do validate early that SQL API is operational.
  */
  Sql_service_command_interface sql_command_interface;
  if (sql_command_interface.establish_session_connection(
          sql_api_isolation, GROUPREPL_USER, lv.plugin_info_ptr)) {
    error = 1;
    goto err;
  }

  /**
    We redo the check for the group name here when starting on boot as only
    now the information about channels and the
    assign_gtids_to_anonymous_transactions is available.
  */
  if (lv.plugin_is_auto_starting_on_boot) {
    if (check_uuid_against_rpl_channel_settings(ov.group_name_var)) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_GRP_NAME_IS_SAME_AS_ANONYMOUS_TO_GTID_UUID,
                   ov.group_name_var);
      error = GROUP_REPLICATION_CONFIGURATION_ERROR;
      goto err;
    }

    if (check_uuid_against_rpl_channel_settings(ov.view_change_uuid_var)) {
      LogPluginErr(
          ERROR_LEVEL,
          ER_GRP_RPL_VIEW_CHANGE_UUID_IS_SAME_AS_ANONYMOUS_TO_GTID_UUID,
          ov.group_name_var);
      error = GROUP_REPLICATION_CONFIGURATION_ERROR;
      goto err;
    }
  }

  // GCS interface.
  if ((error = gcs_module->initialize())) goto err; /* purecov: inspected */

  get_read_mode_state(&read_only_mode, &super_read_only_mode);

  /*
   At this point in the code, set the super_read_only mode here on the
   server to protect recovery and version module of the Group Replication.
   This can only be done on START command though, on installs there are
   deadlock issues.
  */
  if (!lv.plugin_is_auto_starting_on_install) {
    if (enable_server_read_mode()) {
      /* purecov: begin inspected */
      error = 1;
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_ENABLE_SUPER_READ_ONLY_MODE);
      goto err;
      /* purecov: end */
    }
  } else {
    /*
      This flag is used to prevent that a GCS thread that's setting the read
      mode and a simultaneous uninstall command block.

      If the plugin is installed with autostart, the following actions occur:
      1) The install invokes start.
      2) Start cannot set the read mode because it is inside the install
      (server MDL locks issue).
      3) Start delays the read mode setting to the view installation.
      4) The view is installed, so the start terminates and the install
      returns.
      5) Then, some user requests the plugin to uninstall.
      6) The uninstall command will take a MDL lock.
      7) This causes the GCS thread that was setting the read mode to block.
      8) Ultimately, the uninstall command blocks because GCS is not able to
      set the read mode.
    */
    lv.plugin_is_setting_read_mode = true;
  }
  enabled_super_read_only = true;
  if (delayed_init_thd) delayed_init_thd->signal_read_mode_ready();

  require_full_write_set(true);
  set_write_set_memory_size_limit(get_transaction_size_limit());
  write_set_limits_set = true;

  // Setup GCS.
  if ((error = configure_group_communication())) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_INIT_COMMUNICATION_ENGINE);
    goto err;
  }

  if ((error = initialize_plugin_modules(gr_modules::all_modules))) goto err;

  DBUG_EXECUTE_IF("group_replication_before_joining_the_group", {
    const char act[] =
        "now signal signal.group_join_waiting "
        "wait_for signal.continue_group_join";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  if ((error = start_group_communication())) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_START_COMMUNICATION_ENGINE);
    goto err;
  }

  if (view_change_notifier->wait_for_view_modification()) {
    if (!view_change_notifier->is_cancelled()) {
      // Only log a error when a view modification was not cancelled.
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TIMEOUT_ON_VIEW_AFTER_JOINING_GRP);
    }
    error = view_change_notifier->get_error();
    gcs_module->remove_view_notifer(view_change_notifier);
    goto err;
  }
  gcs_module->remove_view_notifer(view_change_notifier);

  transaction_consistency_manager->register_transaction_observer();
  transaction_consistency_manager->plugin_started();

  if (register_gr_message_service_send()) {
    /* purecov: begin inspected */
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
    /* purecov: end */
  }

  if (member_actions_handler->acquire_send_service()) {
    /* purecov: begin inspected */
    error = GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
    /* purecov: end */
  }

  lv.group_replication_running = true;
  lv.plugin_is_stopping = false;
  log_primary_member_details();

err:

  if (error) {
    lv.plugin_is_setting_read_mode = false;
    lv.group_member_mgr_configured = false;

    // Unblock the possible stuck delayed thread
    if (delayed_init_thd) delayed_init_thd->signal_read_mode_ready();

    DBUG_EXECUTE_IF("group_replication_wait_before_leave_on_error", {
      const char act[] =
          "now signal signal.wait_leave_process "
          "wait_for signal.continue_leave_process";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    member_actions_handler->release_send_service();
    unregister_gr_message_service_send();

    auto modules_to_terminate = gr_modules::all_modules;
    modules_to_terminate.reset(gr_modules::ASYNC_REPL_CHANNELS);
    modules_to_terminate.reset(gr_modules::BINLOG_DUMP_THREAD_KILL);
    leave_group_and_terminate_plugin_modules(modules_to_terminate, nullptr);

    if (write_set_limits_set) {
      // Remove server constraints on write set collection
      update_write_set_memory_size_limit(0);
      require_full_write_set(false);
    }

    if (!lv.server_shutdown_status && server_engine_initialized() &&
        enabled_super_read_only) {
      set_read_mode_state(read_only_mode, super_read_only_mode);
    }

    assert(transactions_latch->empty());
    // Inform the transaction observer that we won't apply any further backlog
    // (because we are erroring out).
    if (primary_election_handler) {
      primary_election_handler->notify_election_end();
      delete primary_election_handler;
      primary_election_handler = nullptr;
    }
  }

  lv.plugin_is_auto_starting_on_install = false;

  return error;
}

int configure_group_member_manager() {
  DBUG_TRACE;
  char *hostname = nullptr;
  char *uuid = nullptr;
  uint port = 0U;
  uint server_version = 0U;
  uint admin_port = 0U;

  get_server_parameters(&hostname, &port, &uuid, &server_version, &admin_port);

  /*
    Ensure that group communication interfaces are initialized
    and ready to use, since plugin can leave the group on errors
    but continue to be active.
  */
  std::string gcs_local_member_identifier;
  if (gcs_module->get_local_member_identifier(gcs_local_member_identifier)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_CALL_GRP_COMMUNICATION_INTERFACE);
    return GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR;
    /* purecov: end */
  }

  if (!strcmp(uuid, ov.group_name_var)) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MEMBER_SERVER_UUID_IS_INCOMPATIBLE_WITH_GRP, uuid,
                 ov.group_name_var);
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }

  if (!strcmp(uuid, ov.view_change_uuid_var)) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_GRP_VIEW_CHANGE_UUID_IS_INCOMPATIBLE_WITH_SERVER_UUID,
        ov.view_change_uuid_var, uuid);
    return GROUP_REPLICATION_CONFIGURATION_ERROR;
  }
  // Configure Group Member Manager
  lv.plugin_version = server_version;

  uint32 local_version = lv.plugin_version;
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_major_version",
                  { local_version = lv.plugin_version + (0x010000); };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_minor_version",
                  { local_version = lv.plugin_version + (0x000100); };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_patch_version",
                  { local_version = lv.plugin_version + (0x000001); };);
  DBUG_EXECUTE_IF("group_replication_compatibility_lower_major_version",
                  { local_version = lv.plugin_version - (0x010000); };);
  DBUG_EXECUTE_IF("group_replication_compatibility_lower_minor_version",
                  { local_version = lv.plugin_version - (0x000100); };);
  DBUG_EXECUTE_IF("group_replication_compatibility_lower_patch_version",
                  { local_version = lv.plugin_version - (0x000001); };);
  DBUG_EXECUTE_IF("group_replication_compatibility_restore_version",
                  { local_version = lv.plugin_version; };);
  DBUG_EXECUTE_IF("group_replication_legacy_election_version",
                  { local_version = 0x080012; };);
  DBUG_EXECUTE_IF("group_replication_legacy_election_version2",
                  { local_version = 0x080015; };);
  DBUG_EXECUTE_IF("group_replication_version_8_0_28",
                  { local_version = 0x080028; };);
  Member_version local_member_plugin_version(local_version);
  DBUG_EXECUTE_IF("group_replication_force_member_uuid", {
    uuid = const_cast<char *>("cccccccc-cccc-cccc-cccc-cccccccccccc");
  };);

  // Initialize or update local_member_info.
  if (local_member_info != nullptr) {
    local_member_info->update(
        hostname, port, uuid, lv.write_set_extraction_algorithm,
        gcs_local_member_identifier, Group_member_info::MEMBER_OFFLINE,
        local_member_plugin_version, ov.gtid_assignment_block_size_var,
        Group_member_info::MEMBER_ROLE_SECONDARY, ov.single_primary_mode_var,
        ov.enforce_update_everywhere_checks_var, ov.member_weight_var,
        lv.gr_lower_case_table_names, lv.gr_default_table_encryption,
        ov.advertise_recovery_endpoints_var, ov.view_change_uuid_var,
        get_allow_single_leader());
  } else {
    local_member_info = new Group_member_info(
        hostname, port, uuid, lv.write_set_extraction_algorithm,
        gcs_local_member_identifier, Group_member_info::MEMBER_OFFLINE,
        local_member_plugin_version, ov.gtid_assignment_block_size_var,
        Group_member_info::MEMBER_ROLE_SECONDARY, ov.single_primary_mode_var,
        ov.enforce_update_everywhere_checks_var, ov.member_weight_var,
        lv.gr_lower_case_table_names, lv.gr_default_table_encryption,
        ov.advertise_recovery_endpoints_var, ov.view_change_uuid_var,
        get_allow_single_leader());
  }

#ifndef NDEBUG
  DBUG_EXECUTE_IF("group_replication_skip_encode_default_table_encryption", {
    local_member_info->skip_encode_default_table_encryption = true;
  });

  DBUG_EXECUTE_IF("group_replication_skip_encode_view_change_uuid", {
    local_member_info->m_skip_encode_view_change_uuid = true;
  });
#endif

  // Update membership info of member itself
  if (group_member_mgr != nullptr) group_member_mgr->update(local_member_info);
  // Create the membership info visible for the group
  else
    group_member_mgr = new Group_member_info_manager(local_member_info);

  lv.group_member_mgr_configured = true;

  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_MEMBER_CONF_INFO, get_server_id(),
               local_member_info->get_uuid().c_str(),
               ov.single_primary_mode_var ? "true" : "false",
               ov.auto_increment_increment_var, ov.view_change_uuid_var);

  return 0;
}

void init_compatibility_manager() {
  if (compatibility_mgr != nullptr) {
    delete compatibility_mgr; /* purecov: inspected */
  }

  compatibility_mgr = new Compatibility_module();
}

int configure_compatibility_manager() {
  Member_version local_member_version(lv.plugin_version);
  compatibility_mgr->set_local_version(local_member_version);

  /*
   If needed.. configure here static rules of incompatibility.

   Example:
     Member_version local_member_version(plugin_version);
     Member_version remote_member_version(0x080001);
     compatibility_mgr->add_incompatibility(local_member_version,
                                            remote_member_version);

     Member_version local_member_version(plugin_version);
     Member_version remote_member_min_version(0x080000);
     Member_version remote_member_max_version(0x080005);
     compatibility_mgr->add_incompatibility(local_member_version,
                                            remote_member_min_version,
                                            remote_member_max_version);
   */
  DBUG_EXECUTE_IF("group_replication_compatibility_rule_error_higher", {
    // Mark this member as being another version
    Member_version other_version = lv.plugin_version + (0x010000);
    Member_version local_member_version(lv.plugin_version);
    compatibility_mgr->add_incompatibility(local_member_version, other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_rule_error_lower", {
    // Mark this member as being another version
    Member_version other_version = lv.plugin_version;
    Member_version local_member_version = lv.plugin_version + (0x000001);
    compatibility_mgr->add_incompatibility(local_member_version, other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_major_version", {
    Member_version other_version = lv.plugin_version + (0x010000);
    compatibility_mgr->set_local_version(other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_minor_version", {
    Member_version other_version = lv.plugin_version + (0x000100);
    compatibility_mgr->set_local_version(other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_patch_version", {
    Member_version other_version = lv.plugin_version + (0x000001);
    compatibility_mgr->set_local_version(other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_lower_major_version", {
    Member_version other_version = lv.plugin_version - (0x010000);
    compatibility_mgr->set_local_version(other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_lower_minor_version", {
    Member_version other_version = lv.plugin_version - (0x000100);
    compatibility_mgr->set_local_version(other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_lower_patch_version", {
    Member_version other_version = lv.plugin_version - (0x000001);
    compatibility_mgr->set_local_version(other_version);
  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_restore_version", {
    Member_version current_version = lv.plugin_version;
    compatibility_mgr->set_local_version(current_version);
  };);
  DBUG_EXECUTE_IF("group_replication_legacy_election_version", {
    Member_version higher_version(0x080012);
    compatibility_mgr->set_local_version(higher_version);
  };);
  DBUG_EXECUTE_IF("group_replication_legacy_election_version2", {
    Member_version higher_version(0x080015);
    compatibility_mgr->set_local_version(higher_version);
  };);

  return 0;
}

int leave_group_and_terminate_plugin_modules(
    gr_modules::mask modules_to_terminate, char **error_message) {
  /*
    We acquire the plugin_modules_termination_mutex lock at this point in
    time, before attempting to leave the group or terminate the plugin
    modules.

    This warrants a more in-depth explanation of the life-cycle of
    plugin_modules_termination_mutex.

    By calling terminate_plugin_modules() with gr_modules::all_modules, you
    will terminate all plugin modules. One of those modules happens to be the
    Autorejoin_thread. The termination of the Autorejoin_thread plugin module
    will first terminate the thread.
    The auto-rejoin process could already be undergoing. During the
    auto-rejoin process, in attempt_rejoin(), there will be a call to
    terminate_plugin_modules() as well (albeit with a different bitmask).
    Since we can't terminate the thread during attempt_rejoin(), only after
    it, we could have concurrent calls to terminate_plugin_modules(). Thus the
    need for a critical section around it.

    Also note that we are locking even before we call leave_group(). This is
    just to make sure that we don't start an auto-rejoin just before we
    call leave_group() (as you can see, we only call it if there isn't an
    auto-rejoin undergoing).
  */
  int error = 0;
  mysql_mutex_lock(&lv.plugin_modules_termination_mutex);

  if (!autorejoin_module->is_autorejoin_ongoing()) leave_group();

  error = terminate_plugin_modules(modules_to_terminate, error_message);
  mysql_mutex_unlock(&lv.plugin_modules_termination_mutex);

  return error;
}

int plugin_group_replication_leave_group() { return leave_group(); }

int leave_group() {
  if (gcs_module->belongs_to_group()) {
    view_change_notifier->start_view_modification();

    Gcs_operations::enum_leave_state state =
        gcs_module->leave(view_change_notifier);

    longlong log_severity = WARNING_LEVEL;
    longlong errcode = 0;
    switch (state) {
      case Gcs_operations::ERROR_WHEN_LEAVING:
        /* purecov: begin inspected */
        errcode = ER_GRP_RPL_FAILED_TO_CONFIRM_IF_SERVER_LEFT_GRP;
        log_severity = ERROR_LEVEL;
        break;
      /* purecov: end */
      case Gcs_operations::ALREADY_LEAVING:
        errcode = ER_GRP_RPL_SERVER_IS_ALREADY_LEAVING;
        break;
      case Gcs_operations::ALREADY_LEFT:
        /* purecov: begin inspected */
        errcode = ER_GRP_RPL_SERVER_ALREADY_LEFT;
        break;
      /* purecov: end */
      case Gcs_operations::NOW_LEAVING:
        break;
    }
    if (errcode) LogPluginErr(log_severity, errcode);

    if (!errcode || ER_GRP_RPL_SERVER_IS_ALREADY_LEAVING == errcode) {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_WAITING_FOR_VIEW_UPDATE);
      if (view_change_notifier->wait_for_view_modification()) {
        LogPluginErr(WARNING_LEVEL,
                     ER_GRP_RPL_TIMEOUT_RECEIVING_VIEW_CHANGE_ON_SHUTDOWN);
      }
    }
    gcs_module->remove_view_notifer(view_change_notifier);
  } else {
    /*
      Even when we do not belong to the group we invoke leave()
      to prevent the following situation:
       1) Server joins group;
       2) Server leaves group before receiving the view on which
          it joined the group.
      If we do not leave preemptively, the server will only leave
      the group when the communication layer failure detector
      detects that it left.

      If we leave the group due to a server shutdown, we will end up in this
      code branch. Since we already left the group, we should not execute this
      leave() instruction. Hence, we check if we are currently in a server
      shutdown process.
    */
    if (!get_server_shutdown_status()) {
      LogPluginErr(INFORMATION_LEVEL,
                   ER_GRP_RPL_REQUESTING_NON_MEMBER_SERVER_TO_LEAVE);
      gcs_module->leave(nullptr);
    }
  }

  // Finalize GCS.
  gcs_module->finalize();

  // Destroy handlers and notifiers
  delete events_handler;
  events_handler = nullptr;

  return 0;
}

int plugin_group_replication_stop(char **error_message) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::WRITE_LOCK);
  DBUG_EXECUTE_IF("gr_plugin_gr_stop_after_holding_plugin_running_lock", {
    const char act[] =
        "now signal "
        "signal.reached_plugin_gr_stop_after_holding_plugin_running_lock "
        "wait_for "
        "signal.resume_plugin_gr_stop_after_holding_plugin_running_lock";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  /*
    We delete the delayed initialization object here because:

    1) It is invoked even if the plugin is stopped as failed starts may still
    leave the class instantiated. This way, either the stop command or the
    deinit process that calls this method will always clean this class

    2) Its use is on before_handle_connection, meaning no stop command can be
    made before that. This makes this delete safe under the plugin running
    mutex.
  */
  if (delayed_initialization_thread != nullptr) {
    lv.wait_on_engine_initialization = false;
    delayed_initialization_thread->signal_thread_ready();
    delayed_initialization_thread->wait_for_thread_end();
    delete delayed_initialization_thread;
    delayed_initialization_thread = nullptr;
  }

  if (!plugin_is_group_replication_running()) {
    return 0;
  }

  lv.plugin_is_stopping = true;

  shared_plugin_stop_lock->grab_write_lock();
  LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_IS_STOPPING);

  lv.plugin_is_waiting_to_set_server_read_mode = true;

  transaction_consistency_manager->plugin_is_stopping();

  DBUG_EXECUTE_IF("group_replication_hold_stop_before_leave_the_group", {
    const char act[] =
        "now signal signal.stopping_before_leave_the_group "
        "wait_for signal.resume_stop_before_leave_the_group";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  // wait for all transactions waiting for certification
  bool timeout =
      transactions_latch->block_until_empty(TRANSACTION_KILL_TIMEOUT);
  if (timeout) {
    // if they are blocked, kill them
    blocked_transaction_handler->unblock_waiting_transactions();
  }

  lv.recovery_timeout_issue_on_stop = false;
  int error = leave_group_and_terminate_plugin_modules(gr_modules::all_modules,
                                                       error_message);

  /*
    We do terminate these services after call `terminate_plugin_modules()`,
    despite that is after leaving the group, to ensure that these services
    are terminated after cancelled auto-rejoin attempts.
  */
  member_actions_handler->release_send_service();
  unregister_gr_message_service_send();

  /* Delete of credentials is safe now from recovery thread. */
  Replication_thread_api::delete_credential("group_replication_recovery");

  lv.group_replication_running = false;
  lv.group_member_mgr_configured = false;

  DBUG_EXECUTE_IF("register_gms_listener_example",
                  { unregister_listener_service_gr_example(); });

  shared_plugin_stop_lock->release_write_lock();

  // Enable super_read_only.
  if (!lv.server_shutdown_status && !lv.plugin_is_being_uninstalled &&
      server_engine_initialized()) {
    if (enable_server_read_mode()) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_ENABLE_READ_ONLY_MODE_ON_SHUTDOWN);
      /* purecov: end */
    }
    lv.plugin_is_waiting_to_set_server_read_mode = false;
  }

  // Remove server constraints on write set collection
  update_write_set_memory_size_limit(0);
  require_full_write_set(false);

  // plugin is stopping, resume hold connections
  if (primary_election_handler) {
    primary_election_handler->notify_election_end();
    delete primary_election_handler;
    primary_election_handler = nullptr;
  }

  /*
    Clear transaction consistency manager, waiting transactions
    were already killed above under the protection of
    shared_plugin_stop_lock.
  */
  transaction_consistency_manager->unregister_transaction_observer();
  transaction_consistency_manager->clear();

  if (!error && lv.recovery_timeout_issue_on_stop)
    error = GROUP_REPLICATION_STOP_WITH_RECOVERY_TIMEOUT;

  LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_IS_STOPPED);
  return error;
}

int initialize_plugin_modules(gr_modules::mask modules_to_init) {
  DBUG_TRACE;
  int ret = 0;

  DBUG_EXECUTE_IF("group_replication_rejoin_short_retry",
                  { lv.rejoin_timeout = 1ULL; };);
  DBUG_EXECUTE_IF("group_replication_rejoin_long_retry",
                  { lv.rejoin_timeout = 60ULL; };);

  /*
    Registry module.
  */
  if (modules_to_init[gr_modules::REGISTRY_MODULE]) {
    if ((ret = initialize_registry_module())) return ret;
  }

  /*
    Group Member Manager module.
  */
  if (modules_to_init[gr_modules::GROUP_MEMBER_MANAGER]) {
    if ((ret = configure_group_member_manager())) return ret;
  }

  /*
    Asynchronous Replication Channels.
  */
  if (modules_to_init[gr_modules::ASYNC_REPL_CHANNELS]) {
    lv.wait_on_start_process = WAIT_ON_START_PROCESS_SUCCESS;

    if (check_async_channel_running_on_secondary()) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_START_ON_SECONDARY_WITH_ASYNC_CHANNELS);
      return 1;
    }

    reload_failover_channels_status();
  }

  /*
    Blocked Transaction Handler module.
  */
  if (modules_to_init[gr_modules::BLOCKED_TRANSACTION_HANDLER]) {
    // need to be initialized before applier, is called on
    // kill_pending_transactions
    blocked_transaction_handler = new Blocked_transaction_handler();
  }

  /*
    Remote Cloning Handler module.
  */
  if (modules_to_init[gr_modules::REMOTE_CLONE_HANDLER]) {
    remote_clone_handler = new Remote_clone_handler(
        ov.clone_threshold_var, ov.components_stop_timeout_var);
  }

  /*
    Recovery module.
  */
  if (modules_to_init[gr_modules::RECOVERY_MODULE]) {
    if ((ret = initialize_recovery_module())) return ret;
  }

  /*
    Applier module.
  */
  if (modules_to_init[gr_modules::APPLIER_MODULE]) {
    // we can only start the applier if the log has been initialized
    if (configure_and_start_applier_module())
      return GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR;
  }

  /*
    Group Partition Handler module.
  */
  if (modules_to_init[gr_modules::GROUP_PARTITION_HANDLER]) {
    initialize_group_partition_handler();
  }

  /*
    Auto Increment Handler module.
  */
  if (modules_to_init[gr_modules::AUTO_INCREMENT_HANDLER]) {
    set_auto_increment_handler_values();
  }

  /*
    Primary Election Handler module.
  */
  if (modules_to_init[gr_modules::PRIMARY_ELECTION_HANDLER]) {
    primary_election_handler =
        new Primary_election_handler(ov.components_stop_timeout_var);
  }

  /*
    The Compatibility Manager module.
  */
  if (modules_to_init[gr_modules::COMPATIBILITY_MANAGER]) {
    configure_compatibility_manager();
  }

  /*
    The Auto-rejoin thread.
  */
  if (modules_to_init[gr_modules::AUTOREJOIN_THREAD]) {
    autorejoin_module->init();
  }

  /*
    The Group coordinator module.
  */
  if (modules_to_init[gr_modules::GROUP_ACTION_COORDINATOR]) {
    group_action_coordinator->reset_coordinator_process();
  }

  /*
    The Service message handler.
  */
  if (modules_to_init[gr_modules::MESSAGE_SERVICE_HANDLER]) {
    message_service_handler = new Message_service_handler();
    if (message_service_handler->initialize()) {
      return GROUP_REPLICATION_SERVICE_MESSAGE_INIT_FAILURE; /* purecov:
                                                                inspected */
    }
  }

  /*
    Member actions handler.
  */
  if (modules_to_init[gr_modules::MEMBER_ACTIONS_HANDLER]) {
    if (member_actions_handler->init()) {
      return GROUP_REPLICATION_CONFIGURATION_ERROR;
    }
  }

  /*
    The GCS events handler module.
  */
  if (modules_to_init[gr_modules::GCS_EVENTS_HANDLER]) {
    events_handler = new Plugin_gcs_events_handler(
        applier_module, recovery_module, compatibility_mgr,
        ov.components_stop_timeout_var);
  }

  return ret;
}

int terminate_plugin_modules(gr_modules::mask modules_to_terminate,
                             char **error_message, bool rejoin) {
  /*
    Wait On Start module.
  */
  if (modules_to_terminate[gr_modules::WAIT_ON_START])
    terminate_wait_on_start_process();

  /*
    Autorejoin Thread module.
  */
  if (modules_to_terminate[gr_modules::AUTOREJOIN_THREAD]) {
    autorejoin_module->abort_rejoin();

    /*
      We could be in a situation where the auto-rejoin process terminated
      successfully even after we terminated the auto-rejoin thread. If this
      happens we should leave the group forcefully.

      If we don't leave then we rely on the other members to create a
      suspicion of us and eventually (after the expel timeout ellapses) expel
      us from the group.

      There is no need for such overhead, so we gracefully leave the group.
    */
    if (gcs_module->belongs_to_group()) {
      view_change_notifier->start_view_modification();
      auto state = gcs_module->leave(view_change_notifier);
      if (state != Gcs_operations::ERROR_WHEN_LEAVING &&
          state != Gcs_operations::ALREADY_LEFT)
        view_change_notifier->wait_for_view_modification();
      gcs_module->remove_view_notifer(view_change_notifier);
    }

    // Also, we must terminate the GCS infrastructure completely.
    if (gcs_module->is_initialized()) gcs_module->finalize();
  }

  /*
    Recovery module.
  */
  if (modules_to_terminate[gr_modules::RECOVERY_MODULE]) {
    if (terminate_recovery_module()) {
      lv.recovery_timeout_issue_on_stop = true;
      LogPluginErr(
          WARNING_LEVEL,
          ER_GRP_RPL_RECOVERY_MODULE_TERMINATION_TIMED_OUT_ON_SHUTDOWN);
    }
  }

  DBUG_EXECUTE_IF("group_replication_after_recovery_module_terminated", {
    const char act[] = "now wait_for signal.termination_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  /*
    Remote Cloning Handler module.
  */
  if (modules_to_terminate[gr_modules::REMOTE_CLONE_HANDLER]) {
    if (remote_clone_handler != nullptr) {
      remote_clone_handler->terminate_clone_process(rejoin);
      delete remote_clone_handler;
      remote_clone_handler = nullptr;
    }
  }

  /*
    Group Action Coordinator module.
  */
  if (modules_to_terminate[gr_modules::GROUP_ACTION_COORDINATOR]) {
    group_action_coordinator->stop_coordinator_process(true, true);
  }

  while (!UDF_counter::is_zero()) {
    /* Give 50 ms to udf terminate*/
    my_sleep(50000);
  }

  /*
    Primary Election Handler module.
  */
  if (modules_to_terminate[gr_modules::PRIMARY_ELECTION_HANDLER]) {
    if (primary_election_handler != nullptr) {
      primary_election_handler->terminate_election_process();
    }
  }

  /*
    Auto Increment Handler module.
  */
  if (modules_to_terminate[gr_modules::AUTO_INCREMENT_HANDLER])
    reset_auto_increment_handler_values();

  /*
    Member actions handler.
  */
  if (modules_to_terminate[gr_modules::MEMBER_ACTIONS_HANDLER]) {
    member_actions_handler->deinit();
  }

  /*
    The service message handler.
  */
  if (modules_to_terminate[gr_modules::MESSAGE_SERVICE_HANDLER]) {
    if (message_service_handler) {
      message_service_handler->terminate();
      delete message_service_handler;
      message_service_handler = nullptr;
    }
  }

  /*
    The applier is only shutdown after the communication layer to avoid
    messages being delivered in the current view, but not applied
  */
  int error = 0;
  if (modules_to_terminate[gr_modules::APPLIER_MODULE]) {
    if ((error = terminate_applier_module())) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_APPLIER_TERMINATION_TIMED_OUT_ON_SHUTDOWN);
    }
  }

  /*
    Asynchronous Replication Channels.
  */
  if (modules_to_terminate[gr_modules::ASYNC_REPL_CHANNELS]) {
    std::string stop_error_message;
    int channel_err =
        channel_stop_all(CHANNEL_APPLIER_THREAD | CHANNEL_RECEIVER_THREAD,
                         ov.components_stop_timeout_var, &stop_error_message);
    if (channel_err) {
      std::stringstream err_tmp_ss;
      if (stop_error_message.empty()) {
        err_tmp_ss << "Error stopping all replication channels while"
                   << " server was leaving the group. Got error: "
                   << channel_err
                   << ". Please check the  error log for more details.";
      } else {
        err_tmp_ss << "Error stopping all replication channels while"
                   << " server was leaving the group. ";
        err_tmp_ss << stop_error_message;

        LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_MODULE_TERMINATE_ERROR,
                     stop_error_message.c_str());
      }

      std::string err_tmp_msg = err_tmp_ss.str();
      if (err_tmp_msg.length() + 1 < MYSQL_ERRMSG_SIZE) {
        *error_message = (char *)my_malloc(PSI_NOT_INSTRUMENTED,
                                           err_tmp_msg.length() + 1, MYF(0));
        strcpy(*error_message, err_tmp_msg.c_str());
      }

      if (!error) error = GROUP_REPLICATION_COMMAND_FAILURE;
    }
  }
  if (modules_to_terminate[gr_modules::BINLOG_DUMP_THREAD_KILL])
    Replication_thread_api::rpl_binlog_dump_thread_kill();

  /*
    Group Partition Handler module.
  */
  if (modules_to_terminate[gr_modules::GROUP_PARTITION_HANDLER]) {
    if (group_partition_handler != nullptr)
      group_partition_handler->terminate_partition_handler_thread();
    delete group_partition_handler;
    group_partition_handler = nullptr;
  }

  /*
    Blocked Transaction Handler module.
  */
  if (modules_to_terminate[gr_modules::BLOCKED_TRANSACTION_HANDLER]) {
    delete blocked_transaction_handler;
    blocked_transaction_handler = nullptr;
  }

#if !defined(NDEBUG)
  if (modules_to_terminate[gr_modules::CERTIFICATION_LATCH])
    assert(transactions_latch->empty());
#endif

  /*
    Group member manager module.
  */
  if (modules_to_terminate[gr_modules::GROUP_MEMBER_MANAGER]) {
    if (group_member_mgr != nullptr && local_member_info != nullptr) {
      Notification_context ctx;
      group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                             Group_member_info::MEMBER_OFFLINE,
                                             ctx);
      notify_and_reset_ctx(ctx);
    }
  }

  /*
    Registry module.
  */
  if (modules_to_terminate[gr_modules::REGISTRY_MODULE]) {
    if (finalize_registry_module()) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_SHUTDOWN_REGISTRY_MODULE);
      if (!error) error = 1;
      /* purecov: end */
    }
  }

  /*
    The GCS events handler module.
  */
  if (modules_to_terminate[gr_modules::GCS_EVENTS_HANDLER]) {
    if (events_handler) {
      delete events_handler;
      events_handler = nullptr;
    }
  }

  return error;
}

bool attempt_rejoin() {
  DBUG_TRACE;
  bool ret = true;
  Gcs_operations::enum_leave_state state = Gcs_operations::ERROR_WHEN_LEAVING;
  int error = 0;
  enum enum_gcs_error join_state = GCS_OK;
  Gcs_interface_parameters gcs_params;

  gr_modules::mask modules_mask;
  modules_mask.set(gr_modules::BLOCKED_TRANSACTION_HANDLER, true);
  modules_mask.set(gr_modules::GROUP_PARTITION_HANDLER, true);
  modules_mask.set(gr_modules::APPLIER_MODULE, true);
  modules_mask.set(gr_modules::ASYNC_REPL_CHANNELS, true);
  modules_mask.set(gr_modules::GROUP_ACTION_COORDINATOR, true);
  modules_mask.set(gr_modules::GCS_EVENTS_HANDLER, true);
  modules_mask.set(gr_modules::REMOTE_CLONE_HANDLER, true);
  modules_mask.set(gr_modules::MEMBER_ACTIONS_HANDLER, true);
  modules_mask.set(gr_modules::MESSAGE_SERVICE_HANDLER, true);
  modules_mask.set(gr_modules::BINLOG_DUMP_THREAD_KILL, true);
  modules_mask.set(gr_modules::RECOVERY_MODULE, true);

  /*
    Before leaving the group we need to terminate services that
    do depend on GCS.
  */
  member_actions_handler->release_send_service();
  unregister_gr_message_service_send();

  /*
    The first step is to issue a GCS leave() operation. This is done because
    the join() operation will assume that the GCS layer is not initiated and
    will try to reinitialize everything. Thus, we will simply teardown and
    setup both the GCS layer and the group membership dependent components on
    the GR side between each retry.
  */
  Plugin_gcs_view_modification_notifier vc_notifier;
  vc_notifier.start_view_modification();

  state = gcs_module->leave(&vc_notifier);
  if (state == Gcs_operations::ERROR_WHEN_LEAVING)
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_CONFIRM_IF_SERVER_LEFT_GRP);
  if ((state == Gcs_operations::NOW_LEAVING ||
       state == Gcs_operations::ALREADY_LEAVING) &&
      vc_notifier.wait_for_view_modification())
    LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_TIMEOUT_RECEIVED_VC_LEAVE_ON_REJOIN);

  gcs_module->remove_view_notifer(&vc_notifier);
  gcs_module->finalize();
  group_member_mgr->update(local_member_info);

  /*
    Then we terminate the GR layer components.

    We verify if the plugin_modules_termination_mutex is held. If it is, it
    means that there is an ongoing STOP GROUP_REPLICATION command and that
    command will abort the auto-rejoin thread. So, in that scenario, we
    shouldn't terminate the plugin modules since they are already being
    terminated in the STOP GROUP_REPLICATION command handling thread and we
    leave gracefully.
  */
  error = mysql_mutex_trylock(&lv.plugin_modules_termination_mutex);
  if (!error) {
    error = terminate_plugin_modules(modules_mask, nullptr, true);
    mysql_mutex_unlock(&lv.plugin_modules_termination_mutex);
    if (error) goto end;
  } else {
    goto end;
  }

  /*
    The next step is to prepare the new member for the join.
  */
  if (gcs_module->initialize()) goto end;

  /*
    If the member was the boot node, we rejoin without bootstrapping, because
    the join operation will try to boot the group if the joining member is
    the boot node.
  */
  if (build_gcs_parameters(gcs_params)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_INIT_COMMUNICATION_ENGINE);
    goto end;
  }

  gcs_params.add_parameter("bootstrap_group", "false");
  if (gcs_module->configure(gcs_params) != GCS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_INIT_COMMUNICATION_ENGINE);
    goto end;
  }

  /*
    We try to reinitialize everything again, so that the GCS infrastructure is
    at the same state as before the join() in the START GROUP_REPLICATION
    command.
  */
  if (initialize_plugin_modules(modules_mask)) goto end;

  /*
    Finally we attempt the join itself.
  */
  DBUG_EXECUTE_IF("group_replication_fail_rejoin", goto end;);
  view_change_notifier->start_view_modification();
  join_state =
      gcs_module->join(*events_handler, *events_handler, view_change_notifier);
  if (join_state == GCS_OK) {
    if (view_change_notifier->wait_for_view_modification()) {
      if (view_change_notifier->is_cancelled()) {
        /*
          Member may have become incompatible with the group while was
          disconnected, for instance, if the group mode did change.
        */
        Notification_context ctx;
        group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                               Group_member_info::MEMBER_ERROR,
                                               ctx);
        notify_and_reset_ctx(ctx);

        view_change_notifier->start_view_modification();
        Gcs_operations::enum_leave_state state =
            gcs_module->leave(view_change_notifier);
        if (state != Gcs_operations::ERROR_WHEN_LEAVING &&
            state != Gcs_operations::ALREADY_LEFT) {
          view_change_notifier->wait_for_view_modification();
        }
      } else {
        // Only log a error when a view modification was not cancelled.
        LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_TIMEOUT_RECEIVED_VC_ON_REJOIN);
        DBUG_EXECUTE_IF(
            "group_replication_autorejoin_allow_join_to_change_state", {
              const char act[] =
                  "now wait_for "
                  "signal.group_replication_autorejoin_allow_join_to_change_"
                  "state_resume";
              assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
            });
      }
    } else {
      /*
        Restart services that do depend on GCS.
      */
      if (register_gr_message_service_send() ||
          member_actions_handler->acquire_send_service()) {
        /* purecov: begin inspected */
        member_actions_handler->release_send_service();
        unregister_gr_message_service_send();

        Notification_context ctx;
        group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                               Group_member_info::MEMBER_ERROR,
                                               ctx);
        notify_and_reset_ctx(ctx);

        view_change_notifier->start_view_modification();
        Gcs_operations::enum_leave_state state =
            gcs_module->leave(view_change_notifier);
        if (state != Gcs_operations::ERROR_WHEN_LEAVING &&
            state != Gcs_operations::ALREADY_LEFT) {
          view_change_notifier->wait_for_view_modification();
          /* purecov: end */
        }
      } else {
        ret = false;
      }
    }
  }

end:
  if (ret) {
    /*
      Even when we do not belong to the group we invoke leave()
      to prevent the following situation:
      1) Server joins group;
      2) Server leaves group before receiving the view on which
      it joined the group.
      If we do not leave preemptively, the server will only leave
      the group when the communication layer failure detector
      detects that it left.
    */
    gcs_module->leave(nullptr);
    gcs_module->finalize();
    Notification_context ctx;
    group_member_mgr->update_member_status(
        local_member_info->get_uuid(), Group_member_info::MEMBER_ERROR, ctx);
    notify_and_reset_ctx(ctx);
  }

  gcs_module->remove_view_notifer(view_change_notifier);
  return ret;
}

void mysql_thread_handler_finalize() {
  if (nullptr != mysql_thread_handler_read_only_mode) {
    mysql_thread_handler_read_only_mode->terminate();
    delete mysql_thread_handler_read_only_mode;
    mysql_thread_handler_read_only_mode = nullptr;
  }

  if (nullptr != mysql_thread_handler) {
    mysql_thread_handler->terminate();
    delete mysql_thread_handler;
    mysql_thread_handler = nullptr;
  }
}

bool mysql_thread_handler_initialize() {
  mysql_thread_handler = new Mysql_thread(
      key_GR_THD_mysql_thread_handler, key_GR_LOCK_mysql_thread_handler_run,
      key_GR_COND_mysql_thread_handler_run,
      key_GR_LOCK_mysql_thread_handler_dispatcher_run,
      key_GR_COND_mysql_thread_handler_dispatcher_run);
  bool error = mysql_thread_handler->initialize();

  mysql_thread_handler_read_only_mode = new Mysql_thread(
      key_GR_THD_mysql_thread_handler_read_only_mode,
      key_GR_LOCK_mysql_thread_handler_read_only_mode_run,
      key_GR_COND_mysql_thread_handler_read_only_mode_run,
      key_GR_LOCK_mysql_thread_handler_read_only_mode_dispatcher_run,
      key_GR_COND_mysql_thread_handler_read_only_mode_dispatcher_run);
  error |= mysql_thread_handler_read_only_mode->initialize();

  if (error) {
    LogPluginErr(
        ERROR_LEVEL, ER_GRP_RPL_ERROR_MSG,
        "Failed to initialize Group Replication mysql thread handlers.");
    mysql_thread_handler_finalize();
  }

  return error;
}

void server_services_references_finalize() {
  if (nullptr != server_services_references_module) {
    server_services_references_module->finalize();
    delete server_services_references_module;
    server_services_references_module = nullptr;
  }
}

bool server_services_references_initialize() {
  server_services_references_module = new Server_services_references();
  bool error = server_services_references_module->initialize();
  if (error) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_MSG,
                 "Failed to acquire the required server services.");
    server_services_references_finalize();
  }

  return error;
}

int plugin_group_replication_init(MYSQL_PLUGIN plugin_info) {
  // Initialize plugin local variables.
  lv.init();

  // Initialize error logging service.
  log_bi = nullptr;
  log_bs = nullptr;
  if (init_logging_service_for_plugin(&lv.reg_srv, &log_bi, &log_bs)) return 1;

  if (Charset_service::init(lv.reg_srv)) return 1;

  // Initialize runtime error service.
  my_h_service h_mysql_runtime_error_service = nullptr;
  if (lv.reg_srv->acquire("mysql_runtime_error",
                          &h_mysql_runtime_error_service))
    return 1; /* purecov: inspected */
  mysql_runtime_error_service =
      reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_runtime_error) *>(
          h_mysql_runtime_error_service);

  /*
    Acquire required server services once at plugin install.
  */
  if (server_services_references_initialize()) {
    return 1;
  }

// Register all PSI keys at the time plugin init
#ifdef HAVE_PSI_INTERFACE
  register_all_group_replication_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(key_GR_LOCK_force_members_running,
                   &lv.force_members_running_mutex, MY_MUTEX_INIT_FAST);

  lv.online_wait_mutex =
      new Plugin_waitlock(&lv.plugin_online_mutex, &lv.plugin_online_condition,
#ifdef HAVE_PSI_INTERFACE
                          key_GR_LOCK_plugin_online, key_GR_COND_plugin_online
#else
                          0, 0
#endif /* HAVE_PSI_INTERFACE */
      );

  lv.plugin_running_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_plugin_running
#endif /* HAVE_PSI_INTERFACE */
  );
  lv.plugin_stop_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_plugin_stop
#endif /* HAVE_PSI_INTERFACE */
  );

  shared_plugin_stop_lock = new Shared_writelock(lv.plugin_stop_lock);
  transactions_latch = new Wait_ticket<my_thread_id>();
  transaction_consistency_manager = new Transaction_consistency_manager();
  advertised_recovery_endpoints = new Advertised_recovery_endpoints();

  lv.plugin_info_ptr = plugin_info;

  mysql_mutex_init(key_GR_LOCK_plugin_modules_termination,
                   &lv.plugin_modules_termination_mutex, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_plugin_applier_module_initialize_terminate,
                   &lv.plugin_applier_module_initialize_terminate_mutex,
                   MY_MUTEX_INIT_FAST);

  // Initialize performance_schema tables
  if (initialize_perfschema_module()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_MSG,
                 "Failed to initialize Performance Schema tables.");
    return 1;
    /* purecov: end */
  }

  if (group_replication_init()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_INIT_HANDLER);
    return 1;
    /* purecov: end */
  }

  if (register_server_state_observer(&server_state_observer,
                                     (void *)lv.plugin_info_ptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_REGISTER_SERVER_STATE_OBSERVER);
    return 1;
    /* purecov: end */
  }

  group_transaction_observation_manager =
      new Group_transaction_observation_manager();
  if (register_trans_observer(&trans_observer, (void *)lv.plugin_info_ptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_REGISTER_TRANS_STATE_OBSERVER);
    return 1;
    /* purecov: end */
  }

  if (register_binlog_transmit_observer(&binlog_transmit_observer,
                                        (void *)lv.plugin_info_ptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_REGISTER_BINLOG_STATE_OBSERVER);
    return 1;
    /* purecov: end */
  }

  group_events_observation_manager = new Group_events_observation_manager();
  group_action_coordinator =
      new Group_action_coordinator(ov.components_stop_timeout_var);
  group_action_coordinator->register_coordinator_observers();
  member_actions_handler = new Member_actions_handler();
  consensus_leaders_handler =
      new Consensus_leaders_handler{*group_events_observation_manager};

  /*
    We do query super_read_only value while Group Replication is stopped,
    on example is the `group_replication_enable_member_action` UDF, as such,
    we need to initialize the mysql_thread used on `Get_system_variable`
    on plugin install.
  */
  if (mysql_thread_handler_initialize()) {
    return 1;
  }

  bool const error = register_udfs();
  if (error) return 1;

  if (sql_service_interface_init()) return 1;

  if (gr::status_service::register_gr_status_service()) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_MSG,
                 "Failed to initialize Group Replication status (role and "
                 "mode) service.");
    return 1;
  }

  // Initialize the recovery SSL option map
  initialize_ssl_option_map();

  // Initialize channel observation and auto increment handlers before start
  lv.auto_increment_handler = new Plugin_group_replication_auto_increment();
  channel_observation_manager_list = new Channel_observation_manager_list(
      plugin_info, END_CHANNEL_OBSERVATION_MANAGER_POS);

  view_change_notifier = new Plugin_gcs_view_modification_notifier();
  gcs_module = new Gcs_operations();

  initialize_asynchronous_channels_observer();

  // Initialize the compatibility module before starting
  init_compatibility_manager();

  /*
    Initialize the auto-rejoin thread.
    This will only initialize the thread, not start it.
  */
  autorejoin_module = new Autorejoin_thread();

  lv.plugin_is_auto_starting_on_install =
      ov.start_group_replication_at_boot_var;
  lv.plugin_is_auto_starting_on_boot = ov.start_group_replication_at_boot_var;

  /*
    if the member is auto starting make asynchronous slave threads
    to wait till member comes ONLINE
  */
  set_wait_on_start_process(ov.start_group_replication_at_boot_var);

  // Set the atomic var to the value of the base plugin variable
  ov.transaction_size_limit_var = ov.transaction_size_limit_base_var;

  if (ov.start_group_replication_at_boot_var &&
      plugin_group_replication_start()) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_START_ON_BOOT);
  }

  return 0;
}

int plugin_group_replication_deinit(void *p) {
  // If plugin was not initialized, there is nothing to do here.
  if (lv.plugin_info_ptr == nullptr) return 0;

  lv.plugin_is_being_uninstalled = true;
  lv.plugin_is_stopping = true;
  int observer_unregister_error = 0;

  /*
    We make this call early because perfschema tables may rely on
    `group_member_mgr`.
  */
  finalize_perfschema_module();

  gr::status_service::unregister_gr_status_service();

  if (plugin_group_replication_stop())
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_STOP_ON_PLUGIN_UNINSTALL);

  if (group_member_mgr != nullptr) {
    delete group_member_mgr;
    group_member_mgr = nullptr;
  }

  if (local_member_info != nullptr) {
    delete local_member_info;
    local_member_info = nullptr;
  }

  if (compatibility_mgr != nullptr) {
    delete compatibility_mgr;
    compatibility_mgr = nullptr;
  }

  if (autorejoin_module != nullptr) {
    delete autorejoin_module;
    autorejoin_module = nullptr;
  }

  if (consensus_leaders_handler) {
    delete consensus_leaders_handler;
    consensus_leaders_handler = nullptr;
  }

  if (group_action_coordinator) {
    group_action_coordinator->stop_coordinator_process(true, true);
    group_action_coordinator->unregister_coordinator_observers();
    delete group_action_coordinator;
    group_action_coordinator = nullptr;
  }

  if (group_events_observation_manager != nullptr) {
    delete group_events_observation_manager;
    group_events_observation_manager = nullptr;
  }

  terminate_asynchronous_channels_observer();

  if (unregister_server_state_observer(&server_state_observer, p)) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_UNREGISTER_SERVER_STATE_OBSERVER);
    observer_unregister_error++;
  }

  if (unregister_trans_observer(&trans_observer, p)) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_UNREGISTER_TRANS_STATE_OBSERVER);
    observer_unregister_error++;
  }

  if (unregister_binlog_transmit_observer(&binlog_transmit_observer, p)) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_UNREGISTER_BINLOG_STATE_OBSERVER);
    observer_unregister_error++;
  }

  if (observer_unregister_error == 0)
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_ALL_OBSERVERS_UNREGISTERED);

  if (channel_observation_manager_list != nullptr) {
    delete channel_observation_manager_list;
    channel_observation_manager_list = nullptr;
  }

  // Deleted after un-registration
  if (group_transaction_observation_manager != nullptr) {
    delete group_transaction_observation_manager;
    group_transaction_observation_manager = nullptr;
  }

  delete gcs_module;
  gcs_module = nullptr;
  delete view_change_notifier;
  view_change_notifier = nullptr;

  if (lv.auto_increment_handler != nullptr) {
    delete lv.auto_increment_handler;
    lv.auto_increment_handler = nullptr;
  }

  unregister_udfs();
  sql_service_interface_deinit();
  mysql_thread_handler_finalize();

  delete member_actions_handler;
  member_actions_handler = nullptr;

  if (advertised_recovery_endpoints) delete advertised_recovery_endpoints;
  delete transaction_consistency_manager;
  transaction_consistency_manager = nullptr;
  delete transactions_latch;
  transactions_latch = nullptr;

  mysql_mutex_destroy(&lv.force_members_running_mutex);
  mysql_mutex_destroy(&lv.plugin_applier_module_initialize_terminate_mutex);
  mysql_mutex_destroy(&lv.plugin_modules_termination_mutex);

  delete shared_plugin_stop_lock;
  shared_plugin_stop_lock = nullptr;
  delete lv.plugin_stop_lock;
  lv.plugin_stop_lock = nullptr;
  delete lv.plugin_running_lock;
  lv.plugin_running_lock = nullptr;

  delete lv.online_wait_mutex;
  lv.online_wait_mutex = nullptr;

  lv.plugin_info_ptr = nullptr;

  server_services_references_finalize();

  // Deinitialize runtime error service.
  my_h_service h_mysql_runtime_error_service =
      reinterpret_cast<my_h_service>(mysql_runtime_error_service);
  lv.reg_srv->release(h_mysql_runtime_error_service);
  mysql_runtime_error_service = nullptr;

  Charset_service::deinit(lv.reg_srv);

  deinit_logging_service_for_plugin(&lv.reg_srv, &log_bi, &log_bs);

  return observer_unregister_error;
}

static int plugin_group_replication_check_uninstall(void *) {
  DBUG_TRACE;

  /*
    Uninstall fails
    1. Plugin is setting the read mode so uninstall would deadlock
    2. Plugin in on a network partition
  */
  if (lv.plugin_is_setting_read_mode ||
      (plugin_is_group_replication_running() &&
       group_member_mgr->is_majority_unreachable())) {
    my_error(ER_PLUGIN_CANNOT_BE_UNINSTALLED, MYF(0), "group_replication",
             "Plugin is busy, it cannot be uninstalled. To"
             " force a stop run STOP GROUP_REPLICATION and then UNINSTALL"
             " PLUGIN group_replication.");
    return 1;
  }

  finalize_perfschema_module();

  return 0;
}

static bool init_group_sidno() {
  DBUG_TRACE;
  rpl_sid group_sid;

  if (group_sid.parse(ov.group_name_var, strlen(ov.group_name_var)) !=
      RETURN_STATUS_OK) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_PARSE_THE_GRP_NAME);
    return true;
    /* purecov: end */
  }

  lv.group_sidno = get_sidno_from_global_sid_map(group_sid);
  if (lv.group_sidno <= 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_GENERATE_SIDNO_FOR_GRP);
    return true;
    /* purecov: end */
  }

  if (strcmp(ov.view_change_uuid_var, "AUTOMATIC")) {
    rpl_sid view_change_sid;

    if (view_change_sid.parse(ov.view_change_uuid_var,
                              strlen(ov.view_change_uuid_var)) !=
        RETURN_STATUS_OK) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_PARSE_THE_VIEW_CHANGE_UUID);
      return true;
      /* purecov: end */
    }

    lv.view_change_sidno = get_sidno_from_global_sid_map(view_change_sid);
    if (lv.view_change_sidno <= 0) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_GENERATE_SIDNO_FOR_VIEW_CHANGE_UUID);
      return true;
      /* purecov: end */
    }
  }

  return false;
}

void declare_plugin_cloning(bool is_running) {
  lv.group_replication_cloning = is_running;
}

int configure_and_start_applier_module() {
  DBUG_TRACE;
  MUTEX_LOCK(lock, &lv.plugin_applier_module_initialize_terminate_mutex);

  int error = 0;

  Replication_thread_api applier_channel(applier_module_channel_name);
  applier_channel.set_stop_wait_timeout(1);
  if (applier_channel.is_applier_thread_running() &&
      applier_channel.stop_threads(false, true)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_APPLIER_CHANNEL_STILL_RUNNING);
    return 1;
  }

  // The applier did not stop properly or suffered a configuration error
  if (applier_module != nullptr) {
    if ((error = applier_module->is_running()))  // it is still running?
    {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_APPLIER_NOT_STARTED_DUE_TO_RUNNING_PREV_SHUTDOWN);
      return error;
    } else {
      // clean a possible existent pipeline
      applier_module->terminate_applier_pipeline();
      // delete it and create from scratch
      delete applier_module;
    }
  }

  applier_module = new Applier_module();

  recovery_module->set_applier_module(applier_module);

  // For now, only defined pipelines are accepted.
  error = applier_module->setup_applier_module(
      STANDARD_GROUP_REPLICATION_PIPELINE, lv.known_server_reset,
      ov.components_stop_timeout_var, lv.group_sidno,
      ov.gtid_assignment_block_size_var);
  if (error) {
    // Delete the possible existing pipeline
    applier_module->terminate_applier_pipeline();
    delete applier_module;
    applier_module = nullptr;
    return error;
  }

  lv.known_server_reset = false;

  if ((error = applier_module->initialize_applier_thread())) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_INIT_APPLIER_MODULE);
    // terminate the applier_thread if running
    if (!applier_module->terminate_applier_thread()) {
      delete applier_module;
      applier_module = nullptr;
    }
  } else
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_APPLIER_INITIALIZED);

  return error;
}

void initialize_group_partition_handler() {
  group_partition_handler =
      new Group_partition_handling(ov.timeout_on_unreachable_var);
}

void set_auto_increment_handler_values() {
  lv.auto_increment_handler->set_auto_increment_variables(
      ov.auto_increment_increment_var, get_server_id());
}

void reset_auto_increment_handler_values(bool force_reset) {
  lv.auto_increment_handler->reset_auto_increment_variables(force_reset);
}

int terminate_applier_module() {
  DBUG_TRACE;
  MUTEX_LOCK(lock, &lv.plugin_applier_module_initialize_terminate_mutex);

  int error = 0;
  if (applier_module != nullptr) {
    if (!applier_module->terminate_applier_thread())  // all goes fine
    {
      delete applier_module;
      applier_module = nullptr;
    } else {
      error = GROUP_REPLICATION_APPLIER_STOP_TIMEOUT;
    }
  }
  /*
    Since GR is being stopped we can disable the guarantee that the
    binlog commit order will follow the order instructed by GR.
  */
  Commit_stage_manager::disable_manual_session_tickets();
  return error;
}

int build_gcs_parameters(Gcs_interface_parameters &gcs_module_parameters) {
  DBUG_TRACE;
  int result = 0;
  st_server_ssl_variables sv;

  sv.init();
  if (TLS_SOURCE_MYSQL_ADMIN == ov.tls_source_var)
    get_server_admin_ssl_parameters(&sv);
  else
    get_server_main_ssl_parameters(&sv);

  gcs_module_parameters.add_parameter("group_name",
                                      std::string(ov.group_name_var));
  if (ov.local_address_var != nullptr)
    gcs_module_parameters.add_parameter("local_node",
                                        std::string(ov.local_address_var));
  if (ov.group_seeds_var != nullptr)
    gcs_module_parameters.add_parameter("peer_nodes",
                                        std::string(ov.group_seeds_var));
  const std::string bootstrap_group_string =
      ov.bootstrap_group_var ? "true" : "false";
  gcs_module_parameters.add_parameter("bootstrap_group",
                                      bootstrap_group_string);
  std::stringstream poll_spin_loops_stream_buffer;
  poll_spin_loops_stream_buffer << ov.poll_spin_loops_var;
  gcs_module_parameters.add_parameter("poll_spin_loops",
                                      poll_spin_loops_stream_buffer.str());
  std::stringstream member_expel_timeout_stream_buffer;
  member_expel_timeout_stream_buffer << ov.member_expel_timeout_var;
  gcs_module_parameters.add_parameter("member_expel_timeout",
                                      member_expel_timeout_stream_buffer.str());
  gcs_module_parameters.add_parameter(
      "xcom_cache_size", std::to_string(ov.message_cache_size_var));

  gcs_module_parameters.add_parameter(
      "communication_stack", std::to_string(ov.communication_stack_var));

  /*
   We will add GCS-level join retries for those scenarios where a node
   crashes and comes back immediately, but it still has a reencarnation
   in the system ready to be expel.

   The chosen values relate with START GROUP_REPLICATION timeout which is
   60 seconds.

   This will cover most cases. If a user changes the parameter
   member_expel_timeout this mechanism for sure will not have the same
   effect.
  */
  // Enable only if autorejoin is not running.
  if (!autorejoin_module->is_autorejoin_ongoing()) {
    gcs_module_parameters.add_parameter("join_attempts", "10");
    gcs_module_parameters.add_parameter("join_sleep_time", "5");
  }

  // Compression parameter
  if (ov.compression_threshold_var > 0) {
    std::stringstream ss;
    ss << ov.compression_threshold_var;
    gcs_module_parameters.add_parameter("compression", std::string("on"));
    gcs_module_parameters.add_parameter("compression_threshold", ss.str());
  } else {
    gcs_module_parameters.add_parameter(
        "compression", std::string("off")); /* purecov: inspected */
  }

  // Fragmentation parameter
  if (ov.communication_max_message_size_var > 0) {
    std::stringstream ss;
    ss << ov.communication_max_message_size_var;
    gcs_module_parameters.add_parameter("fragmentation", std::string("on"));
    gcs_module_parameters.add_parameter("fragmentation_threshold", ss.str());
  } else {
    gcs_module_parameters.add_parameter("fragmentation", std::string("off"));
  }

  // SSL parameters.
  std::string ssl_mode(ov.ssl_mode_values[ov.ssl_mode_var]);
  if (ov.ssl_mode_var > 0) {
    enum_transport_protocol xcom_comm_protocol =
        static_cast<enum_transport_protocol>(ov.communication_stack_var);

    std::string ssl_key("");
    std::string ssl_cert("");
    std::string ssl_ca("");
    std::string ssl_capath("");
    std::string ssl_cipher("");
    std::string ssl_crl("");
    std::string ssl_crlpath("");
    std::string tls_version("");
    std::string ssl_fips_mode("");
    if (xcom_comm_protocol == XCOM_PROTOCOL) {
      ssl_key.append(sv.ssl_key ? sv.ssl_key : "");
      ssl_cert.append(sv.ssl_cert ? sv.ssl_cert : "");
      ssl_ca.append(sv.ssl_ca ? sv.ssl_ca : "");
      ssl_capath.append(sv.ssl_capath ? sv.ssl_capath : "");
      ssl_cipher.append(sv.ssl_cipher ? sv.ssl_cipher : "");
      ssl_crl.append(sv.ssl_crl ? sv.ssl_crl : "");
      ssl_crlpath.append(sv.ssl_crlpath ? sv.ssl_crlpath : "");
      tls_version.append(sv.tls_version ? sv.tls_version : "");
      ssl_fips_mode.append(ov.ssl_fips_mode_values[sv.ssl_fips_mode]);
    } else if (xcom_comm_protocol == MYSQL_PROTOCOL) {
      ssl_key.append(ov.recovery_ssl_key_var ? ov.recovery_ssl_key_var : "");
      ssl_cert.append(ov.recovery_ssl_cert_var ? ov.recovery_ssl_cert_var : "");
      ssl_ca.append(ov.recovery_ssl_ca_var ? ov.recovery_ssl_ca_var : "");
      ssl_capath.append(ov.recovery_ssl_capath_var ? ov.recovery_ssl_capath_var
                                                   : "");
      ssl_cipher.append(ov.recovery_ssl_cipher_var ? ov.recovery_ssl_cipher_var
                                                   : "");
      ssl_crl.append(ov.recovery_ssl_crl_var ? ov.recovery_ssl_crl_var : "");
      ssl_crlpath.append(
          ov.recovery_ssl_crlpath_var ? ov.recovery_ssl_crlpath_var : "");
      tls_version.append(
          ov.recovery_tls_version_var ? ov.recovery_tls_version_var : "");
    }

    // SSL support on server.
    gcs_module_parameters.add_parameter("ssl_mode",
                                        ov.ssl_mode_values[ov.ssl_mode_var]);

    gcs_module_parameters.add_parameter("server_key_file", ssl_key);
    gcs_module_parameters.add_parameter("server_cert_file", ssl_cert);
    gcs_module_parameters.add_parameter("client_key_file", ssl_key);
    gcs_module_parameters.add_parameter("client_cert_file", ssl_cert);
    gcs_module_parameters.add_parameter("ca_file", ssl_ca);
    if (!ssl_capath.empty())
      gcs_module_parameters.add_parameter("ca_path",
                                          ssl_capath); /* purecov: inspected */
    gcs_module_parameters.add_parameter("cipher", ssl_cipher);
    gcs_module_parameters.add_parameter("tls_version", tls_version);

    bool is_ciphersuites_null =
        xcom_comm_protocol == XCOM_PROTOCOL
            ? sv.tls_ciphersuites == nullptr
            : ov.recovery_tls_ciphersuites_var == nullptr;
    if (!is_ciphersuites_null) {
      /* Not specifying the ciphersuites means "use the OpenSSL default."
         Specifying an empty string means "disallow all ciphersuites." */
      gcs_module_parameters.add_parameter(
          "tls_ciphersuites", xcom_comm_protocol == XCOM_PROTOCOL
                                  ? sv.tls_ciphersuites
                                  : ov.recovery_tls_ciphersuites_var);
    }

    if (!ssl_crl.empty())
      gcs_module_parameters.add_parameter("crl_file",
                                          ssl_crl); /* purecov: inspected */
    if (!ssl_crlpath.empty())
      gcs_module_parameters.add_parameter("crl_path",
                                          ssl_crlpath); /* purecov: inspected */
    if (!ssl_fips_mode.empty())
      gcs_module_parameters.add_parameter(
          "ssl_fips_mode", ssl_fips_mode); /* purecov: inspected */

    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_COMMUNICATION_SSL_CONF_INFO,
                 ssl_mode.c_str(), ssl_key.c_str(), ssl_cert.c_str(),
                 ssl_key.c_str(), ssl_cert.c_str(), ssl_ca.c_str(),
                 ssl_capath.c_str(), ssl_cipher.c_str(), tls_version.c_str(),
                 sv.tls_ciphersuites ? sv.tls_ciphersuites : "NOT_SET",
                 ssl_crl.c_str(), ssl_crlpath.c_str(), ssl_fips_mode.c_str());
  }
  // GCS SSL disabled.
  else {
    gcs_module_parameters.add_parameter("ssl_mode", ssl_mode);

    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_SSL_DISABLED, ssl_mode.c_str());
  }

  if (ov.ip_whitelist_var != nullptr || ov.ip_allowlist_var != nullptr) {
    std::string v(get_ip_allowlist());
    v.erase(std::remove(v.begin(), v.end(), ' '), v.end());
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    // if the user specified a list other than automatic
    // then we need to pass it to the GCS, otherwise we
    // do nothing and let GCS scan for the proper IPs
    if (v.find("automatic") == std::string::npos) {
      gcs_module_parameters.add_parameter("ip_allowlist",
                                          std::string(get_ip_allowlist()));
    }
  }

  /*
    Define the file where GCS debug messages will be sent to.
  */
  gcs_module_parameters.add_parameter("communication_debug_file",
                                      GCS_DEBUG_TRACE_FILE);

  /*
    By default debug files will be created in a path relative to
    the data directory.
  */
  gcs_module_parameters.add_parameter("communication_debug_path",
                                      mysql_real_data_home);

  sv.deinit();
  return result;
}

int configure_group_communication() {
  DBUG_TRACE;

  // GCS interface parameters.
  Gcs_interface_parameters gcs_module_parameters;
  int err = 0;
  if ((err = build_gcs_parameters(gcs_module_parameters))) goto end;

  // Configure GCS.
  if (gcs_module->configure(gcs_module_parameters)) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_INIT_COMMUNICATION_ENGINE);
    return GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR;
  }

  LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_GRP_COMMUNICATION_INIT_WITH_CONF,
               ov.group_name_var, ov.local_address_var, ov.group_seeds_var,
               ov.bootstrap_group_var ? "true" : "false",
               ov.poll_spin_loops_var, ov.compression_threshold_var,
               get_ip_allowlist(), ov.communication_debug_options_var,
               ov.member_expel_timeout_var,
               ov.communication_max_message_size_var, ov.message_cache_size_var,
               ov.communication_stack_var);

end:
  return err;
}

int start_group_communication() {
  DBUG_TRACE;

  view_change_notifier->start_view_modification();

  if (gcs_module->join(*events_handler, *events_handler, view_change_notifier))
    return GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR;

  return 0;
}

bool check_async_channel_running_on_secondary() {
  /* To stop group replication to start on secondary member with single
    primary- mode, when any async channels are running, we verify whether
    member is not bootstrapping. As only when the member is bootstrapping, it
    can be the primary leader on a single primary member context.
  */
  if (ov.single_primary_mode_var && !ov.bootstrap_group_var &&
      !lv.plugin_is_auto_starting_on_boot) {
    if (is_any_slave_channel_running(CHANNEL_RECEIVER_THREAD |
                                     CHANNEL_APPLIER_THREAD)) {
      return true;
    }
  }

  return false;
}

void initialize_asynchronous_channels_observer() {
  asynchronous_channels_state_observer =
      new Asynchronous_channels_state_observer();
  channel_observation_manager_list
      ->get_channel_observation_manager(ASYNC_CHANNEL_OBSERVATION_MANAGER_POS)
      ->register_channel_observer(asynchronous_channels_state_observer);
}

void terminate_asynchronous_channels_observer() {
  if (asynchronous_channels_state_observer != nullptr) {
    channel_observation_manager_list
        ->get_channel_observation_manager(ASYNC_CHANNEL_OBSERVATION_MANAGER_POS)
        ->unregister_channel_observer(asynchronous_channels_state_observer);
    delete asynchronous_channels_state_observer;
    asynchronous_channels_state_observer = nullptr;
  }
}

int initialize_recovery_module() {
  recovery_module = new Recovery_module(
      applier_module,
      channel_observation_manager_list->get_channel_observation_manager(
          GROUP_CHANNEL_OBSERVATION_MANAGER_POS));

  recovery_module->set_recovery_ssl_options(
      ov.recovery_use_ssl_var, ov.recovery_ssl_ca_var,
      ov.recovery_ssl_capath_var, ov.recovery_ssl_cert_var,
      ov.recovery_ssl_cipher_var, ov.recovery_ssl_key_var,
      ov.recovery_ssl_crl_var, ov.recovery_ssl_crlpath_var,
      ov.recovery_ssl_verify_server_cert_var, ov.recovery_tls_version_var,
      ov.recovery_tls_ciphersuites_var);
  recovery_module->set_recovery_completion_policy(
      (enum_recovery_completion_policies)ov.recovery_completion_policy_var);
  recovery_module->set_recovery_donor_retry_count(ov.recovery_retry_count_var);
  recovery_module->set_recovery_donor_reconnect_interval(
      ov.recovery_reconnect_interval_var);

  recovery_module->set_recovery_public_key_path(
      ov.recovery_public_key_path_var);
  recovery_module->set_recovery_get_public_key(ov.recovery_get_public_key_var);
  recovery_module->set_recovery_compression_algorithm(
      ov.recovery_compression_algorithm_var);
  recovery_module->set_recovery_zstd_compression_level(
      ov.recovery_zstd_compression_level_var);

  // on case of the threads is locked and not able to terminate, change
  // timeout to fail faster
  recovery_module->set_stop_wait_timeout(1);
  int error = recovery_module->check_recovery_thread_status();
  recovery_module->set_stop_wait_timeout(get_components_stop_timeout_var());

  return error;
}

int terminate_recovery_module() {
  int error = 0;
  if (recovery_module != nullptr) {
    error = recovery_module->stop_recovery();
    delete recovery_module;
    recovery_module = nullptr;
  }
  return error;
}

bool server_engine_initialized() {
  // check if empty channel exists, i.e, the slave structures are initialized
  return channel_is_active("", CHANNEL_NO_THD);
}

void register_server_reset_master() { lv.known_server_reset = true; }

bool get_allow_local_lower_version_join() {
  DBUG_TRACE;
  return ov.allow_local_lower_version_join_var;
}

ulong get_transaction_size_limit() {
  DBUG_TRACE;
  return ov.transaction_size_limit_var;
}

bool is_plugin_waiting_to_set_server_read_mode() {
  DBUG_TRACE;
  return lv.plugin_is_waiting_to_set_server_read_mode;
}

void set_enforce_update_everywhere_checks(bool option) {
  ov.enforce_update_everywhere_checks_var = option;
}

void set_single_primary_mode_var(bool option) {
  ov.single_primary_mode_var = option;
}

SERVICE_TYPE(registry) * get_plugin_registry() { return lv.reg_srv; }

/*
  This method is used to accomplish the startup validations of the plugin
  regarding system configuration.

  It currently verifies:
  - Binlog enabled
  - Binlog format
  - Gtid mode
  - LOG_REPLICA_UPDATES
  - Single primary mode configuration

  @return If the operation succeed or failed
    @retval 0 in case of success
    @retval 1 in case of failure
 */
static int check_if_server_properly_configured() {
  DBUG_TRACE;

  // Struct that holds startup and runtime requirements
  Trans_context_info startup_pre_reqs;

  get_server_startup_prerequirements(startup_pre_reqs);

  if (!startup_pre_reqs.binlog_enabled) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_BINLOG_DISABLED);
    return 1;
  }

  if (startup_pre_reqs.binlog_format != BINLOG_FORMAT_ROW) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INVALID_BINLOG_FORMAT);
    return 1;
  }

  if (startup_pre_reqs.gtid_mode != Gtid_mode::ON) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GTID_MODE_OFF);
    return 1;
  }

  if (startup_pre_reqs.log_replica_updates != true) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_LOG_REPLICA_UPDATES_NOT_SET);
    return 1;
  }

  if (startup_pre_reqs.transaction_write_set_extraction == HASH_ALGORITHM_OFF) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_INVALID_TRANS_WRITE_SET_EXTRACTION_VALUE);
    return 1;
  } else {
    lv.write_set_extraction_algorithm =
        startup_pre_reqs.transaction_write_set_extraction;
  }

  if (startup_pre_reqs.mi_repository_type != 1)  // INFO_REPOSITORY_TABLE
  {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_CONNECTION_METADATA_REPO_MUST_BE_TABLE);
    return 1;
  }

  if (startup_pre_reqs.rli_repository_type != 1)  // INFO_REPOSITORY_TABLE
  {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_APPLIER_METADATA_REPO_MUST_BE_TABLE);
    return 1;
  }

  if (startup_pre_reqs.parallel_applier_workers > 0) {
    if (startup_pre_reqs.parallel_applier_type !=
        CHANNEL_MTS_PARALLEL_TYPE_LOGICAL_CLOCK) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_INCORRECT_TYPE_SET_FOR_PARALLEL_APPLIER);
      return 1;
    }

    if (!startup_pre_reqs.parallel_applier_preserve_commit_order) {
      LogPluginErr(WARNING_LEVEL,
                   ER_GRP_RPL_REPLICA_PRESERVE_COMMIT_ORDER_NOT_SET);
      return 1;
    }
  }

  if (ov.single_primary_mode_var && ov.enforce_update_everywhere_checks_var) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_SINGLE_PRIM_MODE_NOT_ALLOWED_WITH_UPDATE_EVERYWHERE);
    return 1;
  }

  lv.gr_lower_case_table_names = startup_pre_reqs.lower_case_table_names;
  assert(lv.gr_lower_case_table_names <= 2);
#ifndef NDEBUG
  DBUG_EXECUTE_IF("group_replication_skip_encode_lower_case_table_names", {
    lv.gr_lower_case_table_names = SKIP_ENCODING_LOWER_CASE_TABLE_NAMES;
  });
#endif

  lv.gr_default_table_encryption = startup_pre_reqs.default_table_encryption;

  return 0;
}

/*
  Check if the parameter Checkable_rwlock::Guard was able to acquire
  a read lock on `plugin_running_lock`.
  This function must only be used by check and update options
  functions.

  If it succeeded to acquire the read lock, `true` is returned.
  The RAII Checkable_rwlock::Guard object will keep the acquired
  lock until it leaves scope, either by success or by any error that
  might occur.

  If a read lock was not acquired, `false` is returned.
  That means that there is already a write lock acquired by someone
  else. The error ER_UNABLE_TO_SET_OPTION is thrown to the client
  session.
*/
static bool plugin_running_lock_is_rdlocked(
    Checkable_rwlock::Guard const &guard) {
  if (guard.is_rdlocked()) {
    return true;
  }

  my_message(ER_UNABLE_TO_SET_OPTION,
             "This option cannot be set while START or STOP "
             "GROUP_REPLICATION is ongoing.",
             MYF(0));
  return false;
}

static bool check_uuid_against_rpl_channel_settings(const char *str) {
  DBUG_TRACE;
  Replication_thread_api replication_api_lookup;
  if (replication_api_lookup
          .is_any_channel_using_uuid_for_assign_gtids_to_anonymous_transaction(
              str)) {
    return true;
  }
  return false;
}

static int check_group_name_string(const char *str, bool is_var_update) {
  DBUG_TRACE;

  if (!str) {
    if (!is_var_update)
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_NAME_OPTION_MANDATORY);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group_replication_group_name "
                 "option is mandatory",
                 MYF(0)); /* purecov: inspected */
    return 1;
  }

  size_t length = strlen(str);
  if (length > UUID_LENGTH) {
    if (!is_var_update)
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_NAME_IS_TOO_LONG, str);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group_replication_group_name is not a valid UUID, its "
                 "length is too big",
                 MYF(0));
    return 1;
  }

  if (!binary_log::Uuid::is_valid(str, length)) {
    if (!is_var_update) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GRP_NAME_IS_NOT_VALID_UUID, str);
    } else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group_replication_group_name is "
                 "not a valid UUID",
                 MYF(0));
    return 1;
  }
  if (check_uuid_against_rpl_channel_settings(str)) {
    if (!is_var_update) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_GRP_NAME_IS_SAME_AS_ANONYMOUS_TO_GTID_UUID, str);
    } else {
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group_replication_group_name is already used for "
                 "ASSIGN_GTIDS_TO_ANOYMOUS_TRANSACTIONS in a server channel",
                 MYF(0));
    }
    return 1;
  }

  if (strcmp(str, ov.view_change_uuid_var) == 0) {
    if (!is_var_update) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GROUP_NAME_SAME_AS_VIEW_CHANGE_UUID,
                   str);
    } else {
      mysql_error_service_emit_printf(
          mysql_runtime_error_service,
          ER_WRONG_VALUE_FOR_VAR_PLUS_ACTIONABLE_PART, 0,
          "group_replication_group_name", str,
          "The value is the same as group_replication_view_change_uuid. "
          "Please "
          "change group_replication_view_change_uuid to AUTOMATIC");
    }
    return 1;
  }
  return 0;
}

static int check_group_name(MYSQL_THD thd, SYS_VAR *, void *save,
                            struct st_mysql_value *value) {
  DBUG_TRACE;

  char buff[NAME_CHAR_LEN];
  const char *str;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (plugin_is_group_replication_running()) {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The group_replication_group_name cannot be changed when Group "
               "Replication is running",
               MYF(0));
    return 1;
  }

  (*(const char **)save) = nullptr;

  int length = sizeof(buff);
  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  else {
    return 1; /* purecov: inspected */
  }

  if (check_group_name_string(str, true)) {
    return 1;
  }

  *(const char **)save = str;

  return 0;
}

/*
 Flow control variable update/validate methods
*/

static int check_flow_control_min_quota_long(longlong value,
                                             bool is_var_update) {
  DBUG_TRACE;

  if (value > ov.flow_control_max_quota_var &&
      ov.flow_control_max_quota_var > 0) {
    if (!is_var_update)
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FLOW_CTRL_MIN_QUOTA_GREATER_THAN_MAX_QUOTA);
    else
      my_message(
          ER_WRONG_VALUE_FOR_VAR,
          "group_replication_flow_control_min_quota cannot be larger than "
          "group_replication_flow_control_max_quota",
          MYF(0));
    return 1;
  }

  return 0;
}

static int check_flow_control_min_recovery_quota_long(longlong value,
                                                      bool is_var_update) {
  DBUG_TRACE;

  if (value > ov.flow_control_max_quota_var &&
      ov.flow_control_max_quota_var > 0) {
    if (!is_var_update)
      LogPluginErr(
          ERROR_LEVEL,
          ER_GRP_RPL_FLOW_CTRL_MIN_RECOVERY_QUOTA_GREATER_THAN_MAX_QUOTA);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "group_replication_flow_control_min_recovery_quota cannot be "
                 "larger than group_replication_flow_control_max_quota",
                 MYF(0));
    return 1;
  }

  return 0;
}

static int check_flow_control_max_quota_long(longlong value,
                                             bool is_var_update) {
  DBUG_TRACE;

  if (value > 0 && ((value < ov.flow_control_min_quota_var &&
                     ov.flow_control_min_quota_var != 0) ||
                    (value < ov.flow_control_min_recovery_quota_var &&
                     ov.flow_control_min_recovery_quota_var != 0))) {
    if (!is_var_update)
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FLOW_CTRL_MAX_QUOTA_SMALLER_THAN_MIN_QUOTAS);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "group_replication_flow_control_max_quota cannot be smaller "
                 "than group_replication_flow_control_min_quota or "
                 "group_replication_flow_control_min_recovery_quota",
                 MYF(0));

    return 1;
  }

  return 0;
}

static int check_flow_control_min_quota(MYSQL_THD, SYS_VAR *, void *save,
                                        struct st_mysql_value *value) {
  DBUG_TRACE;

  longlong in_val;
  value->val_int(value, &in_val);

  if (check_flow_control_min_quota_long(in_val, true)) return 1;

  *(longlong *)save = (in_val < 0) ? 0
                                   : (in_val < MAX_FLOW_CONTROL_THRESHOLD)
                                         ? in_val
                                         : MAX_FLOW_CONTROL_THRESHOLD;

  return 0;
}

static int check_flow_control_min_recovery_quota(MYSQL_THD, SYS_VAR *,
                                                 void *save,
                                                 struct st_mysql_value *value) {
  DBUG_TRACE;

  longlong in_val;
  value->val_int(value, &in_val);

  if (check_flow_control_min_recovery_quota_long(in_val, true)) return 1;

  *(longlong *)save = (in_val < 0) ? 0
                                   : (in_val < MAX_FLOW_CONTROL_THRESHOLD)
                                         ? in_val
                                         : MAX_FLOW_CONTROL_THRESHOLD;
  return 0;
}

static int check_flow_control_max_quota(MYSQL_THD, SYS_VAR *, void *save,
                                        struct st_mysql_value *value) {
  DBUG_TRACE;

  longlong in_val;
  value->val_int(value, &in_val);

  if (check_flow_control_max_quota_long(in_val, true)) return 1;

  *(longlong *)save = (in_val < 0) ? 0
                                   : (in_val < MAX_FLOW_CONTROL_THRESHOLD)
                                         ? in_val
                                         : MAX_FLOW_CONTROL_THRESHOLD;

  return 0;
}

/*
 Recovery module's module variable update/validate methods
*/

static int check_sysvar_ulong_timeout(MYSQL_THD, SYS_VAR *var, void *save,
                                      struct st_mysql_value *value) {
  DBUG_TRACE;
  longlong minimum = 0;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (!strcmp("group_replication_components_stop_timeout", var->name))
    minimum = 2;

  longlong in_val;
  value->val_int(value, &in_val);

  *(longlong *)save = (in_val < minimum)
                          ? minimum
                          : (static_cast<ulonglong>(in_val) < LONG_TIMEOUT)
                                ? in_val
                                : LONG_TIMEOUT;

  return 0;
}

static void update_recovery_retry_count(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                        const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_donor_retry_count(in_val);
  }
}

static void update_recovery_reconnect_interval(MYSQL_THD, SYS_VAR *,
                                               void *var_ptr,
                                               const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_donor_reconnect_interval(in_val);
  }
}

// Recovery SSL options

static void update_ssl_use(MYSQL_THD, SYS_VAR *, void *var_ptr,
                           const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  bool use_ssl_val = *static_cast<const bool *>(save);
  *static_cast<bool *>(var_ptr) = use_ssl_val;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_use_ssl(use_ssl_val);
  }
}

static int check_recovery_ssl_string(const char *str, const char *var_name,
                                     bool is_var_update) {
  DBUG_TRACE;

  if (str != nullptr && strlen(str) > FN_REFLEN) {
    if (!is_var_update)
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INVALID_SSL_RECOVERY_STRING,
                   var_name);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The given value for recovery ssl option is invalid"
                 " as its length is beyond the limit",
                 MYF(0));
    return 1;
  }

  return 0;
}

static int check_recovery_ssl_option(MYSQL_THD thd, SYS_VAR *var, void *save,
                                     struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str = nullptr;

  (*(const char **)save) = nullptr;

  int length = sizeof(buff);
  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  /* group_replication_tls_ciphersuites option can be set to NULL */
  else if (strcmp(var->name, "group_replication_recovery_tls_ciphersuites")) {
    return 1; /* purecov: inspected */
  }

  if (str != nullptr && check_recovery_ssl_string(str, var->name, true)) {
    return 1;
  }

  *(const char **)save = str;

  return 0;
}

static void update_recovery_ssl_option(MYSQL_THD, SYS_VAR *var, void *var_ptr,
                                       const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  const char *new_option_val = *static_cast<char *const *>(save);
  *static_cast<const char **>(var_ptr) = new_option_val;

  // According to the var name, get the operation code and act accordingly
  switch (ov.recovery_ssl_opt_map[var->name]) {
    case ov.RECOVERY_SSL_CA_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_ca(new_option_val);
      break;
    case ov.RECOVERY_SSL_CAPATH_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_capath(new_option_val);
      break;
    case ov.RECOVERY_SSL_CERT_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_cert(new_option_val);
      break;
    case ov.RECOVERY_SSL_CIPHER_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_cipher(new_option_val);
      break;
    case ov.RECOVERY_SSL_KEY_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_key(new_option_val);
      break;
    case ov.RECOVERY_SSL_CRL_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_crl(new_option_val);
      break;
    case ov.RECOVERY_SSL_CRLPATH_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_ssl_crlpath(new_option_val);
      break;
    case ov.RECOVERY_SSL_PUBLIC_KEY_PATH_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_public_key_path(new_option_val);
      break;
    case ov.RECOVERY_TLS_VERSION_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_tls_version(new_option_val);
      break;
    case ov.RECOVERY_TLS_CIPHERSUITES_OPT:
      if (recovery_module != nullptr)
        recovery_module->set_recovery_tls_ciphersuites(new_option_val);
      break;
    default:
      assert(0); /* purecov: inspected */
  }
}

static void update_recovery_get_public_key(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                           const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  bool get_public_key = *static_cast<const bool *>(save);
  *static_cast<bool *>(var_ptr) = get_public_key;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_get_public_key(get_public_key);
  }
}

static void update_ssl_server_cert_verification(MYSQL_THD, SYS_VAR *,
                                                void *var_ptr,
                                                const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  bool ssl_verify_server_cert = *static_cast<const bool *>(save);
  *static_cast<bool *>(var_ptr) = ssl_verify_server_cert;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_ssl_verify_server_cert(
        ssl_verify_server_cert);
  }
}

// Recovery threshold update method
static int check_recovery_completion_policy(MYSQL_THD, SYS_VAR *, void *save,
                                            struct st_mysql_value *value) {
  DBUG_TRACE;

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  TYPELIB *typelib = &ov.recovery_policies_typelib_t;
  long long tmp;
  long result;
  int length;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING) {
    length = sizeof(buff);
    if (!(str = value->val_str(value, buff, &length))) goto err;
    if ((result = (long)find_type(str, typelib, 0) - 1) < 0) goto err;
  } else {
    if (value->val_int(value, &tmp)) goto err;
    if (tmp < 0 || tmp >= static_cast<long long>(typelib->count)) goto err;
    result = (long)tmp;
  }
  *(long *)save = result;

  return 0;

err:
  return 1;
}

static void update_recovery_completion_policy(MYSQL_THD, SYS_VAR *,
                                              void *var_ptr, const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_completion_policy(
        (enum_recovery_completion_policies)in_val);
  }
}

// Component timeout update method

static void update_component_timeout(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                     const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;

  if (applier_module != nullptr) {
    applier_module->set_stop_wait_timeout(in_val);
  }
  if (recovery_module != nullptr) {
    recovery_module->set_stop_wait_timeout(in_val);
  }
  if (events_handler != nullptr) {
    events_handler->set_stop_wait_timeout(in_val);
  }
  if (group_action_coordinator != nullptr) {
    group_action_coordinator->set_stop_wait_timeout(in_val);
  }
  if (primary_election_handler != nullptr) {
    primary_election_handler->set_stop_wait_timeout(in_val);
  }
}

static int check_auto_increment_increment(MYSQL_THD, SYS_VAR *, void *save,
                                          struct st_mysql_value *value) {
  DBUG_TRACE;

  longlong in_val;
  value->val_int(value, &in_val);

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (plugin_is_group_replication_running()) {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The group group_replication_auto_increment_increment cannot be"
               " changed when Group Replication is running",
               MYF(0));
    return 1;
  }

  if (in_val > MAX_AUTO_INCREMENT_INCREMENT ||
      in_val < MIN_AUTO_INCREMENT_INCREMENT) {
    std::stringstream ss;
    ss << "The value " << in_val
       << " is not within the range of "
          "accepted values for the option "
          "group_replication_auto_increment_increment. The value "
          "must be between "
       << MIN_AUTO_INCREMENT_INCREMENT << " and "
       << MAX_AUTO_INCREMENT_INCREMENT << " inclusive.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }

  *(longlong *)save = in_val;
  return 0;
}

// Communication layer options.
static int check_ip_allowlist_preconditions(MYSQL_THD thd, SYS_VAR *var,
                                            void *save,
                                            struct st_mysql_value *value) {
  DBUG_TRACE;
  char buff[IP_ALLOWLIST_STR_BUFFER_LENGTH];
  const char *str;
  int length = sizeof(buff);
  if (!strcmp(var->name, "group_replication_ip_whitelist")) {
    option_deprecation_warning(thd, "group_replication_ip_whitelist",
                               "group_replication_ip_allowlist");
  }

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  (*(const char **)save) = nullptr;

  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  else  // NULL value is not allowed
  {
    return 1; /* purecov: inspected */
  }

  std::stringstream ss;
  ss << "The " << var->name << " is invalid. Make sure that when ";
  ss << "specifying \"AUTOMATIC\" the list contains no other values.";

  // remove trailing whitespaces
  std::string v(str);
  v.erase(std::remove(v.begin(), v.end(), ' '), v.end());
  std::transform(v.begin(), v.end(), v.begin(), ::tolower);
  if (v.find("automatic") != std::string::npos && v.size() != 9) {
    my_message(ER_GROUP_REPLICATION_CONFIGURATION, ss.str().c_str(), MYF(0));
    return 1;
  }

  if (plugin_is_group_replication_running()) {
    Gcs_interface_parameters gcs_module_parameters;
    gcs_module_parameters.add_parameter("group_name",
                                        std::string(ov.group_name_var));
    gcs_module_parameters.add_parameter("ip_allowlist", v.c_str());
    gcs_module_parameters.add_parameter("reconfigure_ip_allowlist", "true");

    if (gcs_module->reconfigure(gcs_module_parameters)) {
      my_message(ER_GROUP_REPLICATION_CONFIGURATION, ss.str().c_str(), MYF(0));
      return 1;
    }
  }

  *(const char **)save = str;

  return 0;
}

static int check_compression_threshold(MYSQL_THD, SYS_VAR *, void *save,
                                       struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running()) {
    my_message(
        ER_GROUP_REPLICATION_RUNNING,
        "The group_replication_compression_threshold cannot be set while "
        "Group Replication is running",
        MYF(0));
    return 1;
  }

  if (in_val > MAX_COMPRESSION_THRESHOLD || in_val < 0) {
    std::stringstream ss;
    ss << "The value " << in_val
       << " is not within the range of "
          "accepted values for the option "
          "group_replication_compression_threshold!";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }

  *(longlong *)save = in_val;

  return 0;
}

static int check_communication_max_message_size(MYSQL_THD, SYS_VAR *,
                                                void *save,
                                                struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running()) {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The group_replication_communication_max_message_size option "
               "cannot be "
               "set while Group Replication is running",
               MYF(0));
    return 1;
  }

  if (in_val > static_cast<longlong>(MAX_COMMUNICATION_MAX_MESSAGE_SIZE) ||
      in_val < MIN_COMMUNICATION_MAX_MESSAGE_SIZE) {
    std::stringstream ss;
    ss << "The value " << in_val
       << " is not within the range of accepted values for the "
          "group_replication_communication_max_message_size option. Use 0 to "
          "disable message fragmentation, or specify a value up to "
       << MAX_COMMUNICATION_MAX_MESSAGE_SIZE << ".";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }

  *(longlong *)save = in_val;

  return 0;
}

static int check_force_members(MYSQL_THD thd, SYS_VAR *, void *save,
                               struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  int error = 0;
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str = nullptr;
  (*(const char **)save) = nullptr;
  int length = 0;
  Gcs_operations::enum_force_members_state force_members_error =
      Gcs_operations::FORCE_MEMBERS_OK;

  // Only one set force_members can run at a time.
  mysql_mutex_lock(&lv.force_members_running_mutex);
  if (lv.force_members_running) {
    my_error(ER_GROUP_REPLICATION_FORCE_MEMBERS_COMMAND_FAILURE, MYF(0),
             "value",
             "There is one group_replication_force_members operation "
             "already ongoing.");
    mysql_mutex_unlock(&lv.force_members_running_mutex);
    return 1;
  }
  lv.force_members_running = true;
  mysql_mutex_unlock(&lv.force_members_running_mutex);

#ifndef NDEBUG
  DBUG_EXECUTE_IF("group_replication_wait_on_check_force_members", {
    const char act[] = "now wait_for waiting";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
#endif

  // String validations.
  length = sizeof(buff);
  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  else {
    error = 1; /* purecov: inspected */
    goto end;  /* purecov: inspected */
  }

  // If option value is empty string, just update its value.
  if (length == 0) goto update_value;

  // if group replication isn't running and majority is reachable you can't
  // update force_members
  if (!plugin_is_group_replication_running()) {
    force_members_error = Gcs_operations::FORCE_MEMBERS_ER_MEMBER_NOT_ONLINE;
  } else if (!group_member_mgr->is_majority_unreachable()) {
    force_members_error =
        Gcs_operations::FORCE_MEMBERS_ER_NOT_ONLINE_AND_MAJORITY_UNREACHABLE;
  } else {
    force_members_error = gcs_module->force_members(str);
  }

  if (force_members_error != Gcs_operations::FORCE_MEMBERS_OK) {
    std::stringstream ss;
    switch (force_members_error) {
      case Gcs_operations::FORCE_MEMBERS_ER_NOT_ONLINE_AND_MAJORITY_UNREACHABLE:
        ss << "The group_replication_force_members can only be updated when "
           << "Group Replication is running and majority of the members are "
           << "unreachable.";
        break;
      case Gcs_operations::FORCE_MEMBERS_ER_MEMBER_NOT_ONLINE:
        ss << "Member is not ONLINE, it is not possible to force a new "
           << "group membership.";
        break;
      case Gcs_operations::FORCE_MEMBERS_ER_MEMBERS_WHEN_LEAVING:
        ss << "A request to force a new group membership was issued "
           << "while the member is leaving the group.";
        break;
      case Gcs_operations::FORCE_MEMBERS_ER_TIMEOUT_ON_WAIT_FOR_VIEW:
        ss << "Timeout on wait for view after setting "
           << "group_replication_force_members.";
        break;
      case Gcs_operations::FORCE_MEMBERS_ER_VALUE_SET_ERROR:
        ss << "Error setting group_replication_force_members value '" << str
           << "'. Please check error log for additional details.";
        break;
      case Gcs_operations::FORCE_MEMBERS_ER_INTERNAL_ERROR:
      default:
        ss << "Please check error log for additional details.";
        break;
    }

    my_error(ER_GROUP_REPLICATION_FORCE_MEMBERS_COMMAND_FAILURE, MYF(0), str,
             ss.str().c_str());
    error = 1;
    goto end;
  }

update_value:
  *(const char **)save = str;

end:
  mysql_mutex_lock(&lv.force_members_running_mutex);
  lv.force_members_running = false;
  mysql_mutex_unlock(&lv.force_members_running_mutex);

  return error;
}

static int check_gtid_assignment_block_size(MYSQL_THD, SYS_VAR *, void *save,
                                            struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running()) {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The group_replication_gtid_assignment_block size cannot be "
               "set while "
               "Group Replication is running",
               MYF(0));
    return 1;
  }

  if (in_val > MAX_GTID_ASSIGNMENT_BLOCK_SIZE ||
      in_val < MIN_GTID_ASSIGNMENT_BLOCK_SIZE) {
    std::stringstream ss;
    ss << "The value " << in_val
       << " is not within the range of accepted values for the option "
          "group_replication_gtid_assignment_block_size.The value must be "
          "between "
       << MIN_GTID_ASSIGNMENT_BLOCK_SIZE << " and "
       << MAX_GTID_ASSIGNMENT_BLOCK_SIZE << " inclusive.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }

  *(longlong *)save = in_val;

  return 0;
}

static bool get_bool_value_using_type_lib(struct st_mysql_value *value,
                                          bool &resulting_value) {
  DBUG_TRACE;
  longlong value_to_check;

  if (MYSQL_VALUE_TYPE_STRING == value->value_type(value)) {
    const unsigned int flags = 0;

    char text_buffer[10] = {0};
    int text_buffer_size = sizeof(text_buffer);
    const char *text_value =
        value->val_str(value, text_buffer, &text_buffer_size);

    if (nullptr == text_value) return false;

    // Return index inside bool_type_allowed_values array
    // (first element start with index 1)
    value_to_check = find_type(text_value, &ov.plugin_bool_typelib_t, flags);

    if (0 == value_to_check) {
      return false;
    }

    // Move the index value to 0,1 values (OFF, ON)
    --value_to_check;
  } else {
    // Do implicit conversion to int
    value->val_int(value, &value_to_check);
  }

  resulting_value = value_to_check > 0 ? true : false;

  return true;
}

static int check_sysvar_bool(MYSQL_THD, SYS_VAR *, void *save,
                             struct st_mysql_value *value) {
  DBUG_TRACE;
  bool in_val;

  if (!get_bool_value_using_type_lib(value, in_val)) return 1;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  *(bool *)save = in_val;

  return 0;
}

static int check_single_primary_mode(MYSQL_THD, SYS_VAR *, void *save,
                                     struct st_mysql_value *value) {
  DBUG_TRACE;
  bool single_primary_mode_val;

  if (!get_bool_value_using_type_lib(value, single_primary_mode_val)) return 1;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (plugin_is_group_replication_running()) {
    my_message(
        ER_GROUP_REPLICATION_RUNNING,
        "Cannot modify group replication mode by changing "
        "group_replication_single_primary_mode system variable. Please use "
        "the "
        "group_replication_switch_to_single_primary_mode([member_uuid]) OR "
        "group_replication_switch_to_multi_primary_mode() UDF.",
        MYF(0));
    return 1;
  }

  if (single_primary_mode_val && ov.enforce_update_everywhere_checks_var) {
    my_message(ER_WRONG_VALUE_FOR_VAR,
               "Cannot turn ON group_replication_single_primary_mode while "
               "group_replication_enforce_update_everywhere_checks is "
               "enabled.",
               MYF(0));
    return 1;
  }

  *(bool *)save = single_primary_mode_val;

  return 0;
}

static int check_enforce_update_everywhere_checks(
    MYSQL_THD, SYS_VAR *, void *save, struct st_mysql_value *value) {
  DBUG_TRACE;
  bool enforce_update_everywhere_checks_val;

  if (!get_bool_value_using_type_lib(value,
                                     enforce_update_everywhere_checks_val))
    return 1;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (plugin_is_group_replication_running()) {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "Cannot turn ON/OFF "
               "group_replication_enforce_update_everywhere_checks mode while "
               "Group Replication is running.",
               MYF(0));
    return 1;
  }

  if (ov.single_primary_mode_var && enforce_update_everywhere_checks_val) {
    my_message(ER_WRONG_VALUE_FOR_VAR,
               "Cannot enable "
               "group_replication_enforce_update_everywhere_checks while "
               "group_replication_single_primary_mode is enabled.",
               MYF(0));
    return 1;
  }

  *(bool *)save = enforce_update_everywhere_checks_val;

  return 0;
}

static int check_communication_debug_options(MYSQL_THD thd, SYS_VAR *,
                                             void *save,
                                             struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str = nullptr;
  int length = sizeof(buff);

  (*(const char **)save) = nullptr;
  if ((str = value->val_str(value, buff, &length)) == nullptr)
    return 1; /* purecov: inspected */

  std::string debug_options(str);
  if (gcs_module->set_debug_options(debug_options)) return 1;
  (*(const char **)save) =
      thd->strmake(debug_options.c_str(), debug_options.length());

  return 0;
}

static void update_unreachable_timeout(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                       const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;

  if (group_partition_handler != nullptr) {
    group_partition_handler->update_timeout_on_unreachable(in_val);
  }
}

static int check_member_weight(MYSQL_THD, SYS_VAR *, void *save,
                               struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running()) {
    std::pair<std::string, std::string> action_initiator_and_description;
    if (group_action_coordinator->is_group_action_running(
            action_initiator_and_description)) {
      std::string message(
          "The member weight for primary elections cannot be changed while "
          "group configuration operation '");
      message.append(action_initiator_and_description.second);
      message.append("' is running initiated by '");
      message.append(action_initiator_and_description.first);
      message.append("'.");
      my_message(ER_WRONG_VALUE_FOR_VAR, message.c_str(), MYF(0));
      return 1;
    }
  }

  *(uint *)save =
      (in_val < MIN_MEMBER_WEIGHT)
          ? MIN_MEMBER_WEIGHT
          : (in_val < MAX_MEMBER_WEIGHT) ? in_val : MAX_MEMBER_WEIGHT;

  return 0;
}

static void update_member_weight(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                 const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  uint in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;

  if (local_member_info != nullptr) {
    local_member_info->set_member_weight(in_val);
  }
}

static int check_member_expel_timeout(MYSQL_THD, SYS_VAR *, void *save,
                                      struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val;
  value->val_int(value, &in_val);

  if ((in_val < MIN_MEMBER_EXPEL_TIMEOUT) ||
      (in_val > MAX_MEMBER_EXPEL_TIMEOUT)) {
    return 1;
  }

  *(longlong *)save = in_val;

  return 0;
}

static void update_member_expel_timeout(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                        const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;
  Gcs_interface_parameters gcs_module_parameters;

  if (ov.group_name_var == nullptr) {
    return;
  }

  gcs_module_parameters.add_parameter("group_name",
                                      std::string(ov.group_name_var));

  std::stringstream member_expel_timeout_stream_buffer;
  member_expel_timeout_stream_buffer << in_val;
  gcs_module_parameters.add_parameter("member_expel_timeout",
                                      member_expel_timeout_stream_buffer.str());
  gcs_module_parameters.add_parameter("reconfigure_ip_allowlist", "false");

  if (gcs_module != nullptr) {
    gcs_module->reconfigure(gcs_module_parameters);
  }
}

static int check_autorejoin_tries(MYSQL_THD, SYS_VAR *, void *save,
                                  struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val = 0;
  value->val_int(value, &in_val);

  if (autorejoin_module->is_autorejoin_ongoing()) {
    my_message(ER_DA_GRP_RPL_STARTED_AUTO_REJOIN,
               "Cannot update the number of auto-rejoin retry attempts when "
               "an auto-rejoin process is already running.",
               MYF(0));
    return 1;
  }

  if (in_val < 0 || in_val > lv.MAX_AUTOREJOIN_TRIES) {
    return 1;
  }

  *(uint *)save = in_val;

  return 0;
}

static void update_autorejoin_tries(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                    const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  uint in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;

  if (autorejoin_module->is_autorejoin_ongoing()) {
    my_message(ER_DA_GRP_RPL_STARTED_AUTO_REJOIN,
               "Cannot update the number of auto-rejoin retry attempts when "
               "an auto-rejoin process is already running.",
               MYF(0));
  } else {
    ov.autorejoin_tries_var = in_val;
  }
}

static int check_message_cache_size(MYSQL_THD, SYS_VAR *var, void *save,
                                    struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong orig;
  ulonglong in_val;
  bool is_negative = false;

  value->val_int(value, &orig);
  in_val = orig;

  /* Check if value is negative */
  if (!value->is_unsigned(value) && orig < 0) {
    is_negative = true;
  }

  if (is_negative || in_val > MAX_MESSAGE_CACHE_SIZE ||
      in_val < MIN_MESSAGE_CACHE_SIZE) {
    std::stringstream ss;
    ss << "The value "
       << (is_negative ? std::to_string(orig) : std::to_string(in_val))
       << " is not within the range of accepted values for the option "
       << var->name << ". The value must be between " << MIN_MESSAGE_CACHE_SIZE
       << " and " << MAX_MESSAGE_CACHE_SIZE << " inclusive.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }

  *(ulong *)save = (ulong)in_val;

  return 0;
}

static void update_message_cache_size(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                      const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;

  if (gcs_module != nullptr) {
    gcs_module->set_xcom_cache_size(in_val);
  }
}

static int check_recovery_compression_algorithm(MYSQL_THD thd, SYS_VAR *var,
                                                void *save,
                                                struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str = nullptr;

  *static_cast<const char **>(save) = nullptr;

  int length = sizeof(buff);
  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  else {
    return 1;
  }
  if (str) {
    if (strcmp(str, COMPRESSION_ALGORITHM_ZLIB) &&
        strcmp(str, COMPRESSION_ALGORITHM_ZSTD) &&
        strcmp(str, COMPRESSION_ALGORITHM_UNCOMPRESSED)) {
      std::stringstream ss;
      ss << "The value '" << str << "' is invalid for " << var->name
         << " option.";
      my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
      return 1;
    }
  }
  *static_cast<const char **>(save) = str;

  return 0;
}

static void update_recovery_compression_algorithm(MYSQL_THD, SYS_VAR *,
                                                  void *var_ptr,
                                                  const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  const char *in_val = *static_cast<char *const *>(save);
  *static_cast<const char **>(var_ptr) = in_val;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_compression_algorithm(in_val);
  }
}

static int check_recovery_zstd_compression_level(MYSQL_THD, SYS_VAR *var,
                                                 void *save,
                                                 struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong in_val;
  value->val_int(value, &in_val);
  if (in_val < 1 || in_val > 22) {
    std::stringstream ss;
    ss << "The value '" << in_val << "' is invalid for " << var->name
       << " option.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }
  *static_cast<uint *>(save) = in_val;
  return 0;
}

static void update_recovery_zstd_compression_level(MYSQL_THD, SYS_VAR *,
                                                   void *var_ptr,
                                                   const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  uint in_val = *static_cast<const uint *>(save);
  *static_cast<uint *>(var_ptr) = in_val;

  if (recovery_module != nullptr) {
    recovery_module->set_recovery_zstd_compression_level(in_val);
  }
}

// Clone var related methods

static int check_clone_threshold(MYSQL_THD, SYS_VAR *var, void *save,
                                 struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  longlong orig = 0;
  ulonglong in_val = 0;
  bool is_negative = false;

  value->val_int(value, &orig);
  in_val = orig;

  /* Check if value is negative */
  if (!value->is_unsigned(value) && orig < 0) {
    is_negative = true;
  }

  if (is_negative || in_val > GNO_END || in_val < 1) {
    std::stringstream ss;
    ss << "The value "
       << (is_negative ? std::to_string(orig) : std::to_string(in_val))
       << " is not within the range of accepted values for the option "
       << var->name << ". The value must be between 1 and " << GNO_END
       << " inclusive.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    return 1;
  }

  *static_cast<ulonglong *>(save) = in_val;

  return 0;
}

static void update_clone_threshold(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                   const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  ulonglong in_val = *static_cast<const ulonglong *>(save);
  *static_cast<ulonglong *>(var_ptr) = in_val;

  if (remote_clone_handler != nullptr) {
    remote_clone_handler->set_clone_threshold((longlong)in_val);
  }
}

static void update_transaction_size_limit(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                          const void *save) {
  DBUG_TRACE;

  ulong in_val = *static_cast<const ulong *>(save);
  *static_cast<ulong *>(var_ptr) = in_val;
  ov.transaction_size_limit_var = in_val;

  if (plugin_is_group_replication_running()) {
    update_write_set_memory_size_limit(ov.transaction_size_limit_var);
  }
}

static void update_allow_single_leader(MYSQL_THD, SYS_VAR *, void *var_ptr,
                                       const void *save) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return;

  if (plugin_is_group_replication_running()) {
    lv.allow_single_leader_latch.first = true;
  } else {
    lv.allow_single_leader_latch.first = false;
  }

  lv.allow_single_leader_latch.second = ov.allow_single_leader_var;
  bool allow_single_leader = *static_cast<const bool *>(save);
  *static_cast<bool *>(var_ptr) = allow_single_leader;
  ov.allow_single_leader_var = allow_single_leader;
}

// Base plugin variables

static MYSQL_SYSVAR_STR(group_name,        /* name */
                        ov.group_name_var, /* var */
                        /* optional var | malloc string | no set default */
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
                            PLUGIN_VAR_NODEFAULT |
                            PLUGIN_VAR_PERSIST_AS_READ_ONLY,
                        "The group name", check_group_name, /* check func*/
                        nullptr,                            /* update func*/
                        nullptr);                           /* default*/

static MYSQL_SYSVAR_BOOL(start_on_boot,                          /* name */
                         ov.start_group_replication_at_boot_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Whether the server should start Group Replication or "
                         "not during bootstrap.",
                         nullptr, /* check func*/
                         nullptr, /* update func*/
                         1);      /* default*/

// GCS module variables

static MYSQL_SYSVAR_STR(
    local_address,        /* name */
    ov.local_address_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The local address, i.e., host:port.", nullptr, /* check func*/
    nullptr,                                        /* update func*/
    "");                                            /* default*/

static MYSQL_SYSVAR_STR(
    group_seeds,        /* name */
    ov.group_seeds_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The list of group seeds, comma separated. E.g., "
    "host1:port1,host2:port2.",
    nullptr, /* check func*/
    nullptr, /* update func*/
    "");     /* default*/

static MYSQL_SYSVAR_STR(
    force_members,        /* name */
    ov.force_members_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_NOPERSIST, /* optional var | malloc string*/
    "The list of members, comma separated. E.g., host1:port1,host2:port2. "
    "This option is used to force a new group membership, on which the "
    "excluded "
    "members will not receive a new view and will be blocked. The DBA will "
    "need "
    "to kill the excluded servers.",
    check_force_members, /* check func*/
    nullptr,             /* update func*/
    "");                 /* default*/

static MYSQL_SYSVAR_BOOL(bootstrap_group,        /* name */
                         ov.bootstrap_group_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Specify if this member will bootstrap the group.",
                         nullptr, /* check func. */
                         nullptr, /* update func*/
                         0        /* default */
);

static MYSQL_SYSVAR_ULONG(
    poll_spin_loops,                                       /* name */
    ov.poll_spin_loops_var,                                /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The number of times a thread waits for a communication engine "
    "mutex to be freed before the thread is suspended.",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0,       /* default */
    0,       /* min */
    ~0UL,    /* max */
    0        /* block */
);

static MYSQL_SYSVAR_ULONG(
    member_expel_timeout,                                  /* name */
    ov.member_expel_timeout_var,                           /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The period of time, in seconds, that a member waits before "
    "expelling any member suspected of failing from the group.",
    check_member_expel_timeout,   /* check func. */
    update_member_expel_timeout,  /* update func. */
    DEFAULT_MEMBER_EXPEL_TIMEOUT, /* default */
    MIN_MEMBER_EXPEL_TIMEOUT,     /* min */
    MAX_MEMBER_EXPEL_TIMEOUT,     /* max */
    0                             /* block */
);

static MYSQL_SYSVAR_ULONG(
    message_cache_size,                                    /* name */
    ov.message_cache_size_var,                             /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The maximum size (in bytes) of Group Replication's internal message "
    "cache (the XCom cache).",
    check_message_cache_size,   /* check func. */
    update_message_cache_size,  /* update func. */
    DEFAULT_MESSAGE_CACHE_SIZE, /* default */
    MIN_MESSAGE_CACHE_SIZE,     /* min */
    MAX_MESSAGE_CACHE_SIZE,     /* max */
    0                           /* block */
);

// Recovery module variables

static MYSQL_SYSVAR_ULONG(
    recovery_retry_count,                                  /* name */
    ov.recovery_retry_count_var,                           /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The number of times that the joiner tries to connect to the available "
    "donors before giving up.",
    check_sysvar_ulong_timeout,  /* check func. */
    update_recovery_retry_count, /* update func. */
    10,                          /* default */
    0,                           /* min */
    LONG_TIMEOUT,                /* max */
    0                            /* block */
);

static MYSQL_SYSVAR_ULONG(
    recovery_reconnect_interval,                           /* name */
    ov.recovery_reconnect_interval_var,                    /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The sleep time between reconnection attempts when no donor was found in "
    "the group",
    check_sysvar_ulong_timeout,         /* check func. */
    update_recovery_reconnect_interval, /* update func. */
    60,                                 /* default */
    0,                                  /* min */
    LONG_TIMEOUT,                       /* max */
    0                                   /* block */
);

// SSL options for recovery

static MYSQL_SYSVAR_BOOL(recovery_use_ssl,        /* name */
                         ov.recovery_use_ssl_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Whether SSL use should be obligatory during Group "
                         "Replication recovery process.",
                         check_sysvar_bool, /* check func*/
                         update_ssl_use,    /* update func*/
                         0);                /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_ca,        /* name */
    ov.recovery_ssl_ca_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The path to a file that contains a list of trusted SSL certificate "
    "authorities.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_capath,        /* name */
    ov.recovery_ssl_capath_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The path to a directory that contains trusted SSL certificate authority "
    "certificates.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_cert,        /* name */
    ov.recovery_ssl_cert_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The name of the SSL certificate file to use for establishing a secure "
    "connection.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_cipher,        /* name */
    ov.recovery_ssl_cipher_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "A list of permissible ciphers to use for SSL encryption.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_key,        /* name */
    ov.recovery_ssl_key_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The name of the SSL key file to use for establishing a secure "
    "connection.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_crl,        /* name */
    ov.recovery_ssl_crl_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The path to a file containing certificate revocation lists.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_crlpath,        /* name */
    ov.recovery_ssl_crlpath_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The path to a directory that contains files containing certificate "
    "revocation lists.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_BOOL(recovery_ssl_verify_server_cert,        /* name */
                         ov.recovery_ssl_verify_server_cert_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Make recovery check the server's Common Name value "
                         "in the donor sent certificate.",
                         check_sysvar_bool,                   /* check func*/
                         update_ssl_server_cert_verification, /* update func*/
                         0);                                  /* default*/

static MYSQL_SYSVAR_STR(
    recovery_tls_version,        /* name */
    ov.recovery_tls_version_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "A list of permissible versions to use for TLS encryption.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "TLSv1.2,TLSv1.3");         /* default*/

static MYSQL_SYSVAR_STR(
    recovery_tls_ciphersuites,        /* name */
    ov.recovery_tls_ciphersuites_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "A list of permissible ciphersuites to use for TLS 1.3 encryption.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    nullptr);                   /* default*/

// Public key path information

static MYSQL_SYSVAR_STR(
    recovery_public_key_path,        /* name */
    ov.recovery_public_key_path_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string*/
    "The path to a file containing donor's public key information.",
    check_recovery_ssl_option,  /* check func*/
    update_recovery_ssl_option, /* update func*/
    "");                        /* default*/

static MYSQL_SYSVAR_BOOL(recovery_get_public_key,        /* name */
                         ov.recovery_get_public_key_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Make recovery fetch the donor's public key "
                         "information during authentication.",
                         check_sysvar_bool,              /* check func*/
                         update_recovery_get_public_key, /* update func*/
                         0);                             /* default*/

/** Initialize the ssl option map with variable names*/
static void initialize_ssl_option_map() {
  ov.recovery_ssl_opt_map.clear();
  SYS_VAR *ssl_ca_var = MYSQL_SYSVAR(recovery_ssl_ca);
  ov.recovery_ssl_opt_map[ssl_ca_var->name] = ov.RECOVERY_SSL_CA_OPT;
  SYS_VAR *ssl_capath_var = MYSQL_SYSVAR(recovery_ssl_capath);
  ov.recovery_ssl_opt_map[ssl_capath_var->name] = ov.RECOVERY_SSL_CAPATH_OPT;
  SYS_VAR *ssl_cert_var = MYSQL_SYSVAR(recovery_ssl_cert);
  ov.recovery_ssl_opt_map[ssl_cert_var->name] = ov.RECOVERY_SSL_CERT_OPT;
  SYS_VAR *ssl_cipher_var = MYSQL_SYSVAR(recovery_ssl_cipher);
  ov.recovery_ssl_opt_map[ssl_cipher_var->name] = ov.RECOVERY_SSL_CIPHER_OPT;
  SYS_VAR *ssl_key_var = MYSQL_SYSVAR(recovery_ssl_key);
  ov.recovery_ssl_opt_map[ssl_key_var->name] = ov.RECOVERY_SSL_KEY_OPT;
  SYS_VAR *ssl_crl_var = MYSQL_SYSVAR(recovery_ssl_crl);
  ov.recovery_ssl_opt_map[ssl_crl_var->name] = ov.RECOVERY_SSL_CRL_OPT;
  SYS_VAR *ssl_crlpath_var = MYSQL_SYSVAR(recovery_ssl_crlpath);
  ov.recovery_ssl_opt_map[ssl_crlpath_var->name] = ov.RECOVERY_SSL_CRLPATH_OPT;
  SYS_VAR *public_key_path_var = MYSQL_SYSVAR(recovery_public_key_path);
  ov.recovery_ssl_opt_map[public_key_path_var->name] =
      ov.RECOVERY_SSL_PUBLIC_KEY_PATH_OPT;
  SYS_VAR *tls_version_var = MYSQL_SYSVAR(recovery_tls_version);
  ov.recovery_ssl_opt_map[tls_version_var->name] = ov.RECOVERY_TLS_VERSION_OPT;
  SYS_VAR *tls_ciphersuites_var = MYSQL_SYSVAR(recovery_tls_ciphersuites);
  ov.recovery_ssl_opt_map[tls_ciphersuites_var->name] =
      ov.RECOVERY_TLS_CIPHERSUITES_OPT;
}

// Recovery threshold options

static MYSQL_SYSVAR_ENUM(recovery_complete_at,              /* name */
                         ov.recovery_completion_policy_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Recovery policies when handling cached transactions "
                         "after state transfer."
                         "possible values are TRANSACTIONS_CERTIFIED or "
                         "TRANSACTION_APPLIED",             /* values */
                         check_recovery_completion_policy,  /* check func. */
                         update_recovery_completion_policy, /* update func. */
                         RECOVERY_POLICY_WAIT_EXECUTED,     /* default */
                         &ov.recovery_policies_typelib_t);  /* type lib */

// Generic timeout setting

static MYSQL_SYSVAR_ULONG(
    components_stop_timeout,                               /* name */
    ov.components_stop_timeout_var,                        /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Timeout in seconds that the plugin waits for each of the components "
    "when "
    "shutting down.",
    check_sysvar_ulong_timeout, /* check func. */
    update_component_timeout,   /* update func. */
    300,                        /* default */
    2,                          /* min */
    LONG_TIMEOUT,               /* max */
    0                           /* block */
);

// Allow member downgrade

static MYSQL_SYSVAR_BOOL(allow_local_lower_version_join,        /* name */
                         ov.allow_local_lower_version_join_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Allow this server to join the group even if it has a "
                         "lower plugin version than the group",
                         nullptr, /* check func. */
                         nullptr, /* update func*/
                         0        /* default */
);

static MYSQL_SYSVAR_ULONG(
    auto_increment_increment,        /* name */
    ov.auto_increment_increment_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | no set default */
    "The group replication group_replication_auto_increment_increment "
    "determines interval between successive column values",
    check_auto_increment_increment,   /* check func. */
    nullptr,                          /* update by update_func_long func. */
    DEFAULT_AUTO_INCREMENT_INCREMENT, /* default */
    MIN_AUTO_INCREMENT_INCREMENT,     /* min */
    MAX_AUTO_INCREMENT_INCREMENT,     /* max */
    0                                 /* block */
);

static MYSQL_SYSVAR_ULONG(
    compression_threshold,        /* name */
    ov.compression_threshold_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | no set default */
    "The value in bytes above which (lz4) compression is "
    "enforced. When set to zero, deactivates compression. "
    "Default: 1000000.",
    check_compression_threshold,   /* check func. */
    nullptr,                       /* update func. */
    DEFAULT_COMPRESSION_THRESHOLD, /* default */
    MIN_COMPRESSION_THRESHOLD,     /* min */
    MAX_COMPRESSION_THRESHOLD,     /* max */
    0                              /* block */
);

static MYSQL_SYSVAR_ULONG(
    communication_max_message_size,                        /* name */
    ov.communication_max_message_size_var,                 /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The maximum message size in bytes after which a message is fragmented.",
    check_communication_max_message_size,   /* check func. */
    nullptr,                                /* update func. */
    DEFAULT_COMMUNICATION_MAX_MESSAGE_SIZE, /* default */
    MIN_COMMUNICATION_MAX_MESSAGE_SIZE,     /* min */
    MAX_COMMUNICATION_MAX_MESSAGE_SIZE,     /* max */
    0                                       /* block */
);

static MYSQL_SYSVAR_ULONGLONG(
    gtid_assignment_block_size,        /* name */
    ov.gtid_assignment_block_size_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | no set default */
    "The number of consecutive GTIDs that are reserved to each "
    "member. Each member will consume its blocks and reserve "
    "more when needed. Default: 1000000.",
    check_gtid_assignment_block_size,   /* check func. */
    nullptr,                            /* update func. */
    DEFAULT_GTID_ASSIGNMENT_BLOCK_SIZE, /* default */
    MIN_GTID_ASSIGNMENT_BLOCK_SIZE,     /* min */
    MAX_GTID_ASSIGNMENT_BLOCK_SIZE,     /* max */
    0                                   /* block */
);

static MYSQL_SYSVAR_ENUM(
    ssl_mode,                                              /* name */
    ov.ssl_mode_var,                                       /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the security state of the connection between Group "
    "Replication members. Default: DISABLED",
    nullptr,                      /* check func. */
    nullptr,                      /* update func. */
    0,                            /* default */
    &ov.ssl_mode_values_typelib_t /* type lib */
);

static MYSQL_SYSVAR_STR(
    ip_whitelist,        /* name */
    ov.ip_whitelist_var, /* var */
    /* optional var | malloc string | no set default */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY,
    "This option can be used to specify which members "
    "are allowed to connect to this member. The input "
    "takes the form of a comma separated list of IPv4 "
    "addresses or subnet CIDR notation. For example: "
    "192.168.1.0/24,10.0.0.1. In addition, the user can "
    "also set as input the value 'AUTOMATIC', in which case "
    "active interfaces on the host will be scanned and "
    "those with addresses on private subnetworks will be "
    "automatically added to the IP allowlist. The address "
    "127.0.0.1 is always added if not specified explicitly "
    "in the allowlist. Default: 'AUTOMATIC'.",
    check_ip_allowlist_preconditions, /* check func*/
    nullptr,                          /* update func*/
    "AUTOMATIC");                     /* default*/

static MYSQL_SYSVAR_STR(
    ip_allowlist,        /* name */
    ov.ip_allowlist_var, /* var */
    /* optional var | malloc string | no set default */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY,
    "This option can be used to specify which members "
    "are allowed to connect to this member. The input "
    "takes the form of a comma separated list of IPv4 "
    "addresses or subnet CIDR notation. For example: "
    "192.168.1.0/24,10.0.0.1. In addition, the user can "
    "also set as input the value 'AUTOMATIC', in which case "
    "active interfaces on the host will be scanned and "
    "those with addresses on private subnetworks will be "
    "automatically added to the IP allowlist. The address "
    "127.0.0.1 is always added if not specified explicitly "
    "in the allowlist. Default: 'AUTOMATIC'.",
    check_ip_allowlist_preconditions, /* check func*/
    nullptr,                          /* update func*/
    "AUTOMATIC");                     /* default*/

static MYSQL_SYSVAR_BOOL(
    single_primary_mode,        /* name */
    ov.single_primary_mode_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | no set default */
    "Instructs the group to automatically pick a single server to be "
    "the one that handles read/write workload. This server is the "
    "PRIMARY all others are SECONDARIES. Default: TRUE.",
    check_single_primary_mode, /* check func*/
    nullptr,                   /* update func*/
    true);                     /* default*/

static MYSQL_SYSVAR_BOOL(
    enforce_update_everywhere_checks,        /* name */
    ov.enforce_update_everywhere_checks_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | no set default */
    "Enable/Disable strict consistency checks for multi-primary "
    "update everywhere. Default: FALSE.",
    check_enforce_update_everywhere_checks, /* check func*/
    nullptr,                                /* update func*/
    false);                                 /* default*/

static MYSQL_SYSVAR_ENUM(flow_control_mode,        /* name */
                         ov.flow_control_mode_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Specifies the mode used on flow control. "
                         "Default: QUOTA",
                         nullptr,                        /* check func. */
                         nullptr,                        /* update func. */
                         FCM_QUOTA,                      /* default */
                         &ov.flow_control_mode_typelib_t /* type lib */
);

static MYSQL_SYSVAR_LONG(
    flow_control_certifier_threshold,                      /* name */
    ov.flow_control_certifier_threshold_var,               /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the number of waiting transactions that will trigger "
    "flow control. Default: 25000",
    nullptr,                        /* check func. */
    nullptr,                        /* update func. */
    DEFAULT_FLOW_CONTROL_THRESHOLD, /* default */
    MIN_FLOW_CONTROL_THRESHOLD,     /* min */
    MAX_FLOW_CONTROL_THRESHOLD,     /* max */
    0                               /* block */
);

static MYSQL_SYSVAR_LONG(
    flow_control_applier_threshold,                        /* name */
    ov.flow_control_applier_threshold_var,                 /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the number of waiting transactions that will trigger "
    "flow control. Default: 25000",
    nullptr,                        /* check func. */
    nullptr,                        /* update func. */
    DEFAULT_FLOW_CONTROL_THRESHOLD, /* default */
    MIN_FLOW_CONTROL_THRESHOLD,     /* min */
    MAX_FLOW_CONTROL_THRESHOLD,     /* max */
    0                               /* block */
);

static MYSQL_SYSVAR_ULONG(
    transaction_size_limit,                                /* name */
    ov.transaction_size_limit_base_var,                    /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the limit of transaction size that can be transferred over "
    "network.",
    nullptr,                        /* check func. */
    update_transaction_size_limit,  /* update func. */
    DEFAULT_TRANSACTION_SIZE_LIMIT, /* default */
    MIN_TRANSACTION_SIZE_LIMIT,     /* min */
    MAX_TRANSACTION_SIZE_LIMIT,     /* max */
    0                               /* block */
);

static MYSQL_SYSVAR_STR(
    communication_debug_options,        /* name */
    ov.communication_debug_options_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string */
    "The set of debug options, comma separated. E.g., DEBUG_BASIC, "
    "DEBUG_ALL.",
    check_communication_debug_options, /* check func */
    nullptr,                           /* update func */
    "GCS_DEBUG_NONE"                   /* default */
);

static MYSQL_SYSVAR_ENUM(exit_state_action,        /* name */
                         ov.exit_state_action_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "The action that is taken when the server "
                         "leaves the group unexpectedly. "
                         "Possible values are READ_ONLY, "
                         "ABORT_SERVER and OFFLINE_MODE.",  /* values */
                         nullptr,                           /* check func. */
                         nullptr,                           /* update func. */
                         EXIT_STATE_ACTION_READ_ONLY,       /* default */
                         &ov.exit_state_actions_typelib_t); /* type lib */

static MYSQL_SYSVAR_UINT(autorejoin_tries,        /* name */
                         ov.autorejoin_tries_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "The number of retries to attempt in the auto-rejoin "
                         "procedure.",
                         check_autorejoin_tries,  /* check func */
                         update_autorejoin_tries, /* update func */
                         3U,                      /* default */
                         0U,                      /* min */
                         lv.MAX_AUTOREJOIN_TRIES, /* max */
                         0);                      /* block */

static MYSQL_SYSVAR_ULONG(
    unreachable_majority_timeout,                          /* name */
    ov.timeout_on_unreachable_var,                         /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The number of seconds before going into error when a majority of "
    "members "
    "is unreachable."
    "If 0 there is no action taken.",
    check_sysvar_ulong_timeout, /* check func. */
    update_unreachable_timeout, /* update func. */
    0,                          /* default */
    0,                          /* min */
    LONG_TIMEOUT,               /* max */
    0                           /* block */
);

static MYSQL_SYSVAR_UINT(
    member_weight,                                         /* name */
    ov.member_weight_var,                                  /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Member weight will determine the member role in the group on"
    " future primary elections",
    check_member_weight,   /* check func. */
    update_member_weight,  /* update func. */
    DEFAULT_MEMBER_WEIGHT, /* default */
    MIN_MEMBER_WEIGHT,     /* min */
    MAX_MEMBER_WEIGHT,     /* max */
    0                      /* block */
);

static MYSQL_SYSVAR_LONG(
    flow_control_min_quota,                                /* name */
    ov.flow_control_min_quota_var,                         /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the minimum flow-control quota that can be assigned to a node."
    "Default: 0 (5% of thresholds)",
    check_flow_control_min_quota, /* check func. */
    nullptr,                      /* update func. */
    MIN_FLOW_CONTROL_THRESHOLD,   /* default */
    MIN_FLOW_CONTROL_THRESHOLD,   /* min */
    MAX_FLOW_CONTROL_THRESHOLD,   /* max */
    0                             /* block */
);

static MYSQL_SYSVAR_LONG(
    flow_control_min_recovery_quota,                       /* name */
    ov.flow_control_min_recovery_quota_var,                /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the minimum flow-control quota that can be assigned to a node,"
    "if flow control was needed due to a recovering node. Default: 0 "
    "(disabled)",
    check_flow_control_min_recovery_quota, /* check func. */
    nullptr,                               /* update func. */
    MIN_FLOW_CONTROL_THRESHOLD,            /* default */
    MIN_FLOW_CONTROL_THRESHOLD,            /* min */
    MAX_FLOW_CONTROL_THRESHOLD,            /* max */
    0                                      /* block */
);

static MYSQL_SYSVAR_LONG(flow_control_max_quota,        /* name */
                         ov.flow_control_max_quota_var, /* var */
                         PLUGIN_VAR_OPCMDARG |
                             PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
                         "Specifies the maximum cluster commit rate allowed "
                         "when flow-control is active."
                         "Default: 0 (disabled)",
                         check_flow_control_max_quota, /* check func. */
                         nullptr,                      /* update func. */
                         MIN_FLOW_CONTROL_THRESHOLD,   /* default */
                         MIN_FLOW_CONTROL_THRESHOLD,   /* min */
                         MAX_FLOW_CONTROL_THRESHOLD,   /* max */
                         0                             /* block */
);

static MYSQL_SYSVAR_INT(
    flow_control_member_quota_percent,                     /* name */
    ov.flow_control_member_quota_percent_var,              /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the proportion of the quota that is assigned to this member."
    "Default: 0% (disabled)",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0,       /* default */
    0,       /* min */
    100,     /* max */
    0        /* block */
);

static MYSQL_SYSVAR_INT(
    flow_control_period,                                   /* name */
    ov.flow_control_period_var,                            /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies how many seconds to wait between flow-control iterations."
    "Default: 1",
    nullptr, /* check func. */
    nullptr, /* update func. */
    1,       /* default */
    1,       /* min */
    60,      /* max */
    0        /* block */
);

static MYSQL_SYSVAR_INT(
    flow_control_hold_percent,                             /* name */
    ov.flow_control_hold_percent_var,                      /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the percentage of the quota that is reserved for catch-up."
    "Default: 10%, 0 disables",
    nullptr, /* check func. */
    nullptr, /* update func. */
    10,      /* default */
    0,       /* min */
    100,     /* max */
    0        /* block */
);

static MYSQL_SYSVAR_INT(
    flow_control_release_percent,                          /* name */
    ov.flow_control_release_percent_var,                   /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Specifies the percentage of the quota the can increase per iteration"
    "when flow-control is released. Default: 50%, 0 disables",
    nullptr, /* check func. */
    nullptr, /* update func. */
    50,      /* default */
    0,       /* min */
    1000,    /* max */
    0        /* block */
);

static MYSQL_SYSVAR_ULONGLONG(
    clone_threshold,                                       /* name */
    ov.clone_threshold_var,                                /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The number of missing transactions in a joining member needed to "
    "execute "
    "the clone procedure.",
    check_clone_threshold,  /* check func. */
    update_clone_threshold, /* update func. */
    GNO_END,                /* default */
    1,                      /* min */
    GNO_END,                /* max */
    0                       /* block */
);

static MYSQL_SYSVAR_STR(
    recovery_compression_algorithms,       /* name */
    ov.recovery_compression_algorithm_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string */
    "Recovery channel compression algorithm.",
    check_recovery_compression_algorithm,  /* check func */
    update_recovery_compression_algorithm, /* update func */
    COMPRESSION_ALGORITHM_UNCOMPRESSED     /* default */
);

static MYSQL_SYSVAR_UINT(
    recovery_zstd_compression_level,                       /* name */
    ov.recovery_zstd_compression_level_var,                /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Recovery channel compression level.",
    check_recovery_zstd_compression_level,  /* check func */
    update_recovery_zstd_compression_level, /* update func */
    3U,                                     /* default */
    1U,                                     /* min */
    22U,                                    /* max */
    0                                       /* block */
);

static MYSQL_SYSVAR_BOOL(
    paxos_single_leader,                                   /* name */
    ov.allow_single_leader_var,                            /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Enables or disables the usage of a single consensus leader when running "
    "the group in Single-Primary mode.",
    check_sysvar_bool,          /* check func. */
    update_allow_single_leader, /* update func*/
    0                           /* default */
);

static int check_advertise_recovery_endpoints(MYSQL_THD thd, SYS_VAR *,
                                              void *save,
                                              struct st_mysql_value *value) {
  DBUG_TRACE;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str = nullptr;

  *static_cast<const char **>(save) = nullptr;

  int length = sizeof(buff);
  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  else {
    return 1; /* purecov: inspected */
  }

  if (str) {
    if (advertised_recovery_endpoints->check(
            str, Advertised_recovery_endpoints::enum_log_context::ON_SET)) {
      return 1;
    }
  }
  if (local_member_info != nullptr) {
    local_member_info->set_recovery_endpoints(str);
  }
  *static_cast<const char **>(save) = str;

  return 0;
}

static MYSQL_SYSVAR_STR(
    advertise_recovery_endpoints,        /* name */
    ov.advertise_recovery_endpoints_var, /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
        PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var | malloc string */
    "Recovery list of endpoints for joiner connection",
    check_advertise_recovery_endpoints, /* check func */
    nullptr,                            /* update func */
    "DEFAULT"                           /* default */
);

static MYSQL_SYSVAR_ENUM(
    tls_source,                                            /* name */
    ov.tls_source_var,                                     /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "The source of TLS configuration of the connection between "
    "Group Replication members. Possible values are MYSQL_MAIN "
    "and MYSQL_ADMIN. Default: MYSQL_MAIN", /* values */
    nullptr,                                /* check func. */
    nullptr,                                /* update func. */
    0,                                      /* default */
    &ov.tls_source_values_typelib_t         /* type lib */
);

static int check_view_change_uuid_string(const char *str, bool is_var_update) {
  DBUG_TRACE;

  if (strcmp(str, "AUTOMATIC") == 0) return 0;

  size_t length = strlen(str);
  if (!binary_log::Uuid::is_valid(str, length)) {
    if (!is_var_update) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_VIEW_CHANGE_UUID_INVALID, str);
    } else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group_replication_view_change_uuid is "
                 "not a valid UUID",
                 MYF(0));
    return 1;
  }

  if (ov.group_name_var != nullptr && strcmp(str, ov.group_name_var) == 0) {
    if (!is_var_update) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_VIEW_CHANGE_UUID_SAME_AS_GROUP_NAME,
                   str);
    } else {
      mysql_error_service_emit_printf(
          mysql_runtime_error_service,
          ER_WRONG_VALUE_FOR_VAR_PLUS_ACTIONABLE_PART, 0,
          "group_replication_view_change_uuid", str,
          "If you want to use the UUID of 'group_replication_group_name' for "
          "the UUID of View_change_log_events, please set "
          "'group_replication_view_change_uuid' to AUTOMATIC.");
    }
    return 1;
  }

  if (check_uuid_against_rpl_channel_settings(str)) {
    if (!is_var_update) {
      LogPluginErr(
          ERROR_LEVEL,
          ER_GRP_RPL_VIEW_CHANGE_UUID_IS_SAME_AS_ANONYMOUS_TO_GTID_UUID, str);
    } else {
      mysql_error_service_emit_printf(
          mysql_runtime_error_service,
          ER_WRONG_VALUE_FOR_VAR_PLUS_ACTIONABLE_PART, 0,
          "group_replication_view_change_uuid", str,
          "The value is already used for "
          "ASSIGN_GTIDS_TO_ANOYMOUS_TRANSACTIONS "
          "in a server channel");
    }
    return 1;
  }
  return 0;
}

static int check_view_change_uuid(MYSQL_THD thd, SYS_VAR *, void *save,
                                  struct st_mysql_value *value) {
  DBUG_TRACE;

  char buff[NAME_CHAR_LEN];
  const char *str;

  Checkable_rwlock::Guard g(*lv.plugin_running_lock,
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!plugin_running_lock_is_rdlocked(g)) return 1;

  if (plugin_is_group_replication_running()) {
    my_message(
        ER_GROUP_REPLICATION_RUNNING,
        "The group_replication_view_change_uuid cannot be changed when Group "
        "Replication is running",
        MYF(0));
    return 1;
  }

  (*(const char **)save) = nullptr;

  int length = sizeof(buff);
  if ((str = value->val_str(value, buff, &length)))
    str = thd->strmake(str, length);
  else {
    return 1; /* purecov: inspected */
  }

  if (check_view_change_uuid_string(str, true)) {
    return 1;
  }

  *(const char **)save = str;

  if (local_member_info != nullptr) {
    local_member_info->set_view_change_uuid(str);
  }

  return 0;
}

static MYSQL_SYSVAR_STR(
    view_change_uuid,        /* name */
    ov.view_change_uuid_var, /* var */
    /* optional var | malloc string */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_PERSIST_AS_READ_ONLY,
    "The UUID used to identify view changes in the group, "
    "but also used in the associated GTID events whenever a "
    "View Change is logged.",
    check_view_change_uuid, /* check func*/
    nullptr,                /* update func*/
    "AUTOMATIC");           /* default*/

static MYSQL_SYSVAR_ENUM(
    communication_stack,                                   /* name */
    ov.communication_stack_var,                            /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY, /* optional var */
    "Selects the group replication protocol stack to "
    "use : Legacy XCom or MySQL.This option only takes effect after a group "
    "replication restart. Default: XCom",
    nullptr,                                 /* check func. */
    nullptr,                                 /* update func. */
    XCOM_PROTOCOL,                           /* default */
    &ov.communication_stack_values_typelib_t /* type lib */
);

static SYS_VAR *group_replication_system_vars[] = {
    MYSQL_SYSVAR(group_name),
    MYSQL_SYSVAR(start_on_boot),
    MYSQL_SYSVAR(local_address),
    MYSQL_SYSVAR(group_seeds),
    MYSQL_SYSVAR(force_members),
    MYSQL_SYSVAR(bootstrap_group),
    MYSQL_SYSVAR(poll_spin_loops),
    MYSQL_SYSVAR(recovery_retry_count),
    MYSQL_SYSVAR(recovery_use_ssl),
    MYSQL_SYSVAR(recovery_ssl_ca),
    MYSQL_SYSVAR(recovery_ssl_capath),
    MYSQL_SYSVAR(recovery_ssl_cert),
    MYSQL_SYSVAR(recovery_ssl_cipher),
    MYSQL_SYSVAR(recovery_ssl_key),
    MYSQL_SYSVAR(recovery_ssl_crl),
    MYSQL_SYSVAR(recovery_ssl_crlpath),
    MYSQL_SYSVAR(recovery_ssl_verify_server_cert),
    MYSQL_SYSVAR(recovery_complete_at),
    MYSQL_SYSVAR(recovery_reconnect_interval),
    MYSQL_SYSVAR(recovery_public_key_path),
    MYSQL_SYSVAR(recovery_get_public_key),
    MYSQL_SYSVAR(recovery_compression_algorithms),
    MYSQL_SYSVAR(recovery_zstd_compression_level),
    MYSQL_SYSVAR(components_stop_timeout),
    MYSQL_SYSVAR(allow_local_lower_version_join),
    MYSQL_SYSVAR(auto_increment_increment),
    MYSQL_SYSVAR(compression_threshold),
    MYSQL_SYSVAR(communication_max_message_size),
    MYSQL_SYSVAR(gtid_assignment_block_size),
    MYSQL_SYSVAR(ssl_mode),
    MYSQL_SYSVAR(ip_whitelist),
    MYSQL_SYSVAR(ip_allowlist),
    MYSQL_SYSVAR(single_primary_mode),
    MYSQL_SYSVAR(enforce_update_everywhere_checks),
    MYSQL_SYSVAR(flow_control_mode),
    MYSQL_SYSVAR(flow_control_certifier_threshold),
    MYSQL_SYSVAR(flow_control_applier_threshold),
    MYSQL_SYSVAR(transaction_size_limit),
    MYSQL_SYSVAR(communication_debug_options),
    MYSQL_SYSVAR(exit_state_action),
    MYSQL_SYSVAR(autorejoin_tries),
    MYSQL_SYSVAR(unreachable_majority_timeout),
    MYSQL_SYSVAR(member_weight),
    MYSQL_SYSVAR(flow_control_min_quota),
    MYSQL_SYSVAR(flow_control_min_recovery_quota),
    MYSQL_SYSVAR(flow_control_max_quota),
    MYSQL_SYSVAR(flow_control_member_quota_percent),
    MYSQL_SYSVAR(flow_control_period),
    MYSQL_SYSVAR(flow_control_hold_percent),
    MYSQL_SYSVAR(flow_control_release_percent),
    MYSQL_SYSVAR(member_expel_timeout),
    MYSQL_SYSVAR(message_cache_size),
    MYSQL_SYSVAR(clone_threshold),
    MYSQL_SYSVAR(recovery_tls_version),
    MYSQL_SYSVAR(recovery_tls_ciphersuites),
    MYSQL_SYSVAR(advertise_recovery_endpoints),
    MYSQL_SYSVAR(tls_source),
    MYSQL_SYSVAR(view_change_uuid),
    MYSQL_SYSVAR(communication_stack),
    MYSQL_SYSVAR(paxos_single_leader),
    nullptr,
};

static int show_primary_member(MYSQL_THD, SHOW_VAR *var, char *buff) {
  var->type = SHOW_CHAR;
  var->value = nullptr;

  if (group_member_mgr && ov.single_primary_mode_var &&
      plugin_is_group_replication_running()) {
    string primary_member_uuid;
    group_member_mgr->get_primary_member_uuid(primary_member_uuid);

    strncpy(buff, primary_member_uuid.c_str(), SHOW_VAR_FUNC_BUFF_SIZE);
    buff[SHOW_VAR_FUNC_BUFF_SIZE - 1] = 0;

    var->value = buff;
  }

  return 0;
}

static SHOW_VAR group_replication_status_vars[] = {
    {"group_replication_primary_member", (char *)&show_primary_member,
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_LONG, SHOW_SCOPE_GLOBAL},
};

mysql_declare_plugin(group_replication_plugin){
    MYSQL_GROUP_REPLICATION_PLUGIN,
    &group_replication_descriptor,
    "group_replication",
    PLUGIN_AUTHOR_ORACLE,
    "Group Replication (1.1.0)", /* Plugin name with full version*/
    PLUGIN_LICENSE_GPL,
    plugin_group_replication_init,            /* Plugin Init */
    plugin_group_replication_check_uninstall, /* Plugin Check uninstall */
    plugin_group_replication_deinit,          /* Plugin Deinit */
    0x0101,                                   /* Plugin Version: major.minor */
    group_replication_status_vars,            /* status variables */
    group_replication_system_vars,            /* system variables */
    nullptr,                                  /* config options */
    0,                                        /* flags */
} mysql_declare_plugin_end;
