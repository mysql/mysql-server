/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef PLUGIN_VARIABLES_INCLUDE
#define PLUGIN_VARIABLES_INCLUDE

#include <atomic>
#include <map>

/*
  Variables that have file context on plugin.cc

  All variables declared on this structure must be initialized
  on init() function.
*/
struct plugin_local_variables {
  MYSQL_PLUGIN plugin_info_ptr;
  unsigned int plugin_version;
  rpl_sidno group_sidno;
  rpl_sidno view_change_sidno;

  mysql_mutex_t force_members_running_mutex;
  mysql_mutex_t plugin_online_mutex;
  mysql_mutex_t plugin_modules_termination_mutex;
  mysql_mutex_t plugin_applier_module_initialize_terminate_mutex;
  mysql_cond_t plugin_online_condition;
  Plugin_waitlock *online_wait_mutex;
  Checkable_rwlock *plugin_running_lock;
  Checkable_rwlock *plugin_stop_lock;
  std::atomic<bool> plugin_is_stopping;
  std::atomic<bool> group_replication_running;
  std::atomic<bool> group_replication_cloning;

  bool force_members_running;
  uint gr_lower_case_table_names;
  bool gr_default_table_encryption;
  bool known_server_reset;
  bool group_member_mgr_configured;
  bool plugin_is_auto_starting_on_boot;
  bool plugin_is_auto_starting_on_install;
  bool plugin_is_being_uninstalled;
  bool plugin_is_setting_read_mode;
  bool plugin_is_waiting_to_set_server_read_mode;
  bool server_shutdown_status;
  bool wait_on_engine_initialization;
  int write_set_extraction_algorithm;
  enum_wait_on_start_process_result wait_on_start_process;
  bool recovery_timeout_issue_on_stop;
  // The first argument indicates whether or not to use the value stored in this
  // pair's second argument for the group_replication_paxos_single_leader sysvar
  // or the actual value that's stored on the sysvar
  std::pair<bool, bool> allow_single_leader_latch{false, true};

  // (60min / 5min) * 24 * 7, i.e. a week.
  const uint MAX_AUTOREJOIN_TRIES = 2016;
  ulonglong rejoin_timeout;

  Plugin_group_replication_auto_increment *auto_increment_handler;
  SERVICE_TYPE(registry) * reg_srv;

  /*
    Initialize all variables, except mutexes.
  */
  void init() {
    plugin_info_ptr = nullptr;
    plugin_version = 0;
    group_sidno = 0;
    view_change_sidno = 0;

    online_wait_mutex = nullptr;
    plugin_running_lock = nullptr;
    plugin_stop_lock = nullptr;
    plugin_is_stopping = false;
    group_replication_running = false;
    group_replication_cloning = false;

    force_members_running = false;
    gr_lower_case_table_names = 0;
    gr_default_table_encryption = false;
    known_server_reset = false;
    group_member_mgr_configured = false;
    plugin_is_auto_starting_on_boot = false;
    plugin_is_auto_starting_on_install = false;
    plugin_is_being_uninstalled = false;
    plugin_is_setting_read_mode = false;
    plugin_is_waiting_to_set_server_read_mode = false;
    server_shutdown_status = false;
    wait_on_engine_initialization = false;
    write_set_extraction_algorithm = HASH_ALGORITHM_OFF;
    wait_on_start_process = WAIT_ON_START_PROCESS_SUCCESS;
    allow_single_leader_latch.first = false;
    recovery_timeout_issue_on_stop = false;
    // the default is 5 minutes (300 secs).
    rejoin_timeout = 300ULL;

    auto_increment_handler = nullptr;
    reg_srv = nullptr;
  }
};

/*
  Options variables that have file context on plugin.cc

  All *_var variables declared on this structure, are initialized
  on plugin install when server creates the options.

  Variables are listed on the same order as plugin.cc
*/
struct plugin_options_variables {
  const char *ssl_fips_mode_values[4] = {"OFF", "ON", "STRICT",
                                         (const char *)nullptr};

  const char *bool_type_allowed_values[3] = {"OFF", "ON",
                                             (const char *)nullptr};
  TYPELIB plugin_bool_typelib_t = {2, "bool_type_typelib_t",
                                   bool_type_allowed_values, nullptr};

  char *group_name_var;
  bool start_group_replication_at_boot_var;
  char *local_address_var;
  char *group_seeds_var;
  char *force_members_var;
  bool bootstrap_group_var;
  ulong poll_spin_loops_var;

#define DEFAULT_MEMBER_EXPEL_TIMEOUT 5
#define MAX_MEMBER_EXPEL_TIMEOUT 3600
#define MIN_MEMBER_EXPEL_TIMEOUT 0
  ulong member_expel_timeout_var;

  // Option map entries that map the different SSL options to integer
  static const int RECOVERY_SSL_CA_OPT = 1;
  static const int RECOVERY_SSL_CAPATH_OPT = 2;
  static const int RECOVERY_SSL_CERT_OPT = 3;
  static const int RECOVERY_SSL_CIPHER_OPT = 4;
  static const int RECOVERY_SSL_KEY_OPT = 5;
  static const int RECOVERY_SSL_CRL_OPT = 6;
  static const int RECOVERY_SSL_CRLPATH_OPT = 7;
  static const int RECOVERY_SSL_PUBLIC_KEY_PATH_OPT = 8;
  static const int RECOVERY_TLS_VERSION_OPT = 9;
  static const int RECOVERY_TLS_CIPHERSUITES_OPT = 10;
  // The option map <SSL var_name, SSL var code>
  std::map<const char *, int> recovery_ssl_opt_map;

  ulong recovery_retry_count_var;
  ulong recovery_reconnect_interval_var;
  bool recovery_use_ssl_var;
  char *recovery_ssl_ca_var;
  char *recovery_ssl_capath_var;
  char *recovery_ssl_cert_var;
  char *recovery_ssl_cipher_var;
  char *recovery_ssl_key_var;
  char *recovery_ssl_crl_var;
  char *recovery_ssl_crlpath_var;
  bool recovery_ssl_verify_server_cert_var;
  char *recovery_public_key_path_var;
  bool recovery_get_public_key_var;
  char *recovery_compression_algorithm_var;
  uint recovery_zstd_compression_level_var;
  char *recovery_tls_version_var;
  char *recovery_tls_ciphersuites_var;

  const char *recovery_policies[3] = {"TRANSACTIONS_CERTIFIED",
                                      "TRANSACTIONS_APPLIED", (char *)nullptr};
  TYPELIB recovery_policies_typelib_t = {2, "recovery_policies_typelib_t",
                                         recovery_policies, nullptr};
  ulong recovery_completion_policy_var;

  ulong components_stop_timeout_var;

  bool allow_local_lower_version_join_var;

  /*
    The default value for auto_increment_increment is chosen taking into
    account the maximum usable values for each possible auto_increment_increment
    and what is a normal group expected size.
  */
#define DEFAULT_AUTO_INCREMENT_INCREMENT 7
#define MIN_AUTO_INCREMENT_INCREMENT 1
#define MAX_AUTO_INCREMENT_INCREMENT 65535
  ulong auto_increment_increment_var;

#define DEFAULT_COMPRESSION_THRESHOLD 1000000
#define MAX_COMPRESSION_THRESHOLD UINT_MAX32
#define MIN_COMPRESSION_THRESHOLD 0
  ulong compression_threshold_var;

#define DEFAULT_GTID_ASSIGNMENT_BLOCK_SIZE 1000000
#define MIN_GTID_ASSIGNMENT_BLOCK_SIZE 1
#define MAX_GTID_ASSIGNMENT_BLOCK_SIZE GNO_END
  ulonglong gtid_assignment_block_size_var;

  const char *ssl_mode_values[5] = {"DISABLED", "REQUIRED", "VERIFY_CA",
                                    "VERIFY_IDENTITY", (char *)nullptr};
  TYPELIB ssl_mode_values_typelib_t = {4, "ssl_mode_values_typelib_t",
                                       ssl_mode_values, nullptr};
  ulong ssl_mode_var;

#define IP_ALLOWLIST_STR_BUFFER_LENGTH 1024
  char *ip_whitelist_var;
  char *ip_allowlist_var;

#define DEFAULT_COMMUNICATION_MAX_MESSAGE_SIZE 10485760
#define MAX_COMMUNICATION_MAX_MESSAGE_SIZE get_max_replica_max_allowed_packet()
#define MIN_COMMUNICATION_MAX_MESSAGE_SIZE 0
  ulong communication_max_message_size_var;

#define DEFAULT_MESSAGE_CACHE_SIZE 1073741824
#define MIN_MESSAGE_CACHE_SIZE 134217728
#define MAX_MESSAGE_CACHE_SIZE ULONG_MAX
  ulong message_cache_size_var;

  bool single_primary_mode_var;
  bool enforce_update_everywhere_checks_var;

  const char *flow_control_mode_values[3] = {"DISABLED", "QUOTA",
                                             (const char *)nullptr};
  TYPELIB flow_control_mode_typelib_t = {2, "flow_control_mode_typelib_t",
                                         flow_control_mode_values, nullptr};
  ulong flow_control_mode_var;
#define DEFAULT_FLOW_CONTROL_THRESHOLD 25000
#define MAX_FLOW_CONTROL_THRESHOLD INT_MAX32
#define MIN_FLOW_CONTROL_THRESHOLD 0
  long flow_control_certifier_threshold_var;
  long flow_control_applier_threshold_var;

#define DEFAULT_TRANSACTION_SIZE_LIMIT 150000000
#define MAX_TRANSACTION_SIZE_LIMIT 2147483647
#define MIN_TRANSACTION_SIZE_LIMIT 0
  /** Base variable that feeds the value to an atomic variable */
  ulong transaction_size_limit_base_var;
  std::atomic<ulong> transaction_size_limit_var;

  char *communication_debug_options_var;

  const char *exit_state_actions[4] = {"READ_ONLY", "ABORT_SERVER",
                                       "OFFLINE_MODE", (char *)nullptr};
  TYPELIB exit_state_actions_typelib_t = {3, "exit_state_actions_typelib_t",
                                          exit_state_actions, nullptr};
  ulong exit_state_action_var;

  uint autorejoin_tries_var;

  ulong timeout_on_unreachable_var;

#define DEFAULT_MEMBER_WEIGHT 50
#define MAX_MEMBER_WEIGHT 100
#define MIN_MEMBER_WEIGHT 0
  uint member_weight_var;

  long flow_control_min_quota_var;
  long flow_control_min_recovery_quota_var;
  long flow_control_max_quota_var;
  int flow_control_member_quota_percent_var;
  int flow_control_period_var;
  int flow_control_hold_percent_var;
  int flow_control_release_percent_var;

  ulonglong clone_threshold_var;

  char *advertise_recovery_endpoints_var;

  const char *tls_source_values[3] = {"MYSQL_MAIN", "MYSQL_ADMIN",
                                      (char *)nullptr};
  TYPELIB tls_source_values_typelib_t = {2, "tls_source_typelib_t",
                                         tls_source_values, nullptr};
  ulong tls_source_var;

  char *view_change_uuid_var;

  const char *communication_stack_source_values[3] = {"XCOM", "MYSQL",
                                                      (char *)nullptr};
  TYPELIB communication_stack_values_typelib_t = {
      2, "communication_stack_typelib_t", communication_stack_source_values,
      nullptr};
  ulong communication_stack_var;

  bool allow_single_leader_var{false};
};

#endif /* PLUGIN_VARIABLES_INCLUDE */
