/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <sstream>

#include "observer_server_actions.h"
#include "observer_server_state.h"
#include "observer_trans.h"
#include "plugin.h"
#include "plugin_log.h"
#include "pipeline_stats.h"

using std::string;

/* Plugin generic fields */

static MYSQL_PLUGIN plugin_info_ptr= NULL;
unsigned int plugin_version= 0;

//The plugin running flag and lock
static mysql_mutex_t plugin_running_mutex;
static bool group_replication_running;
bool wait_on_engine_initialization= false;
bool server_shutdown_status= false;
bool plugin_is_auto_starting= false;
static bool plugin_is_waiting_to_set_server_read_mode= false;
static bool plugin_is_being_uninstalled= false;

/* Plugin modules */
//The plugin applier
Applier_module *applier_module= NULL;
//The plugin recovery module
Recovery_module *recovery_module= NULL;
//The plugin group communication module
Gcs_operations *gcs_module= NULL;
//The channel observation module
Channel_observation_manager *channel_observation_manager= NULL;
//The Single primary channel observation module
Asynchronous_channels_state_observer
    *asynchronous_channels_state_observer= NULL;
//Lock to check if the plugin is running or not.
Checkable_rwlock *plugin_stop_lock;
//Class to coordinate access to the plugin stop lock
Shared_writelock *shared_plugin_stop_lock;
//Initialization thread for server starts
Delayed_initialization_thread *delayed_initialization_thread= NULL;
//The transaction handler for network partitions
Group_partition_handling *group_partition_handler= NULL;
//The handler for transaction killing when an error or partition happens
Blocked_transaction_handler *blocked_transaction_handler= NULL;

/* Group communication options */
char *local_address_var= NULL;
char *group_seeds_var= NULL;
char *force_members_var= NULL;
bool force_members_running= false;
static mysql_mutex_t force_members_running_mutex;
my_bool bootstrap_group_var= false;
ulong poll_spin_loops_var= 0;
ulong ssl_mode_var= 0;

const char* ssl_mode_values[]= {
  "DISABLED",
  "REQUIRED",
  "VERIFY_CA",
  "VERIFY_IDENTITY",
  (char*)0
};

static const char *bool_type_allowed_values[]= {
  "OFF",
  "ON",
  (const char*)0
};

static TYPELIB plugin_bool_typelib= {
  sizeof(bool_type_allowed_values) / sizeof(*bool_type_allowed_values) - 1, // names count
  "",                       // type name
  bool_type_allowed_values, // value names
  NULL                      // count
};

#define IP_WHITELIST_STR_BUFFER_LENGTH 1024
char *ip_whitelist_var= NULL;
const char *IP_WHITELIST_DEFAULT= "AUTOMATIC";

//The plugin auto increment handler
Plugin_group_replication_auto_increment *auto_increment_handler= NULL;
Plugin_gcs_events_handler* events_handler= NULL;
Plugin_gcs_view_modification_notifier* view_change_notifier= NULL;

/* Group management information */
Group_member_info_manager_interface *group_member_mgr= NULL;
Group_member_info* local_member_info= NULL;

/*Compatibility management*/
Compatibility_module* compatibility_mgr= NULL;

/* Plugin group related options */
const char *group_replication_plugin_name= "group_replication";
char *group_name_var= NULL;
my_bool start_group_replication_at_boot_var= true;
rpl_sidno group_sidno;
my_bool single_primary_mode_var= FALSE;
my_bool enforce_update_everywhere_checks_var= TRUE;

/* Applier module related */
bool known_server_reset;

//Recovery ssl options

// Option map entries that map the different SSL options to integer
static const int RECOVERY_SSL_CA_OPT= 1;
static const int RECOVERY_SSL_CAPATH_OPT= 2;
static const int RECOVERY_SSL_CERT_OPT= 3;
static const int RECOVERY_SSL_CIPHER_OPT= 4;
static const int RECOVERY_SSL_KEY_OPT= 5;
static const int RECOVERY_SSL_CRL_OPT= 6;
static const int RECOVERY_SSL_CRLPATH_OPT= 7;
//The option map <SSL var_name, SSL var code>
std::map<const char*, int> recovery_ssl_opt_map;

// SSL options
my_bool recovery_use_ssl_var= false;
char* recovery_ssl_ca_var= NULL;
char* recovery_ssl_capath_var= NULL;
char* recovery_ssl_cert_var= NULL;
char* recovery_ssl_cipher_var= NULL;
char* recovery_ssl_key_var= NULL;
char* recovery_ssl_crl_var= NULL;
char* recovery_ssl_crlpath_var= NULL;
my_bool recovery_ssl_verify_server_cert_var= false;
ulong  recovery_completion_policy_var;

ulong recovery_retry_count_var= 0;
ulong recovery_reconnect_interval_var= 0;

/* Write set extraction algorithm*/
int write_set_extraction_algorithm= HASH_ALGORITHM_OFF;

/* Lower case table name */
uint gr_lower_case_table_names= 0;

/* Generic components variables */
ulong components_stop_timeout_var= LONG_TIMEOUT;

/* The timeout before going to error when majority becomes unreachable */
ulong timeout_on_unreachable_var= 0;

/*
 Exit state action that is executed when a server involuntarily leaves the
 group.
*/
ulong exit_state_action_var;

/**
  The default value for auto_increment_increment is choosen taking into
  account the maximum usable values for each possible auto_increment_increment
  and what is a normal group expected size.
*/
#define DEFAULT_AUTO_INCREMENT_INCREMENT 7
#define MIN_AUTO_INCREMENT_INCREMENT 1
#define MAX_AUTO_INCREMENT_INCREMENT 65535
ulong auto_increment_increment_var= DEFAULT_AUTO_INCREMENT_INCREMENT;

/* compression options */
#define DEFAULT_COMPRESSION_THRESHOLD 1000000
#define MAX_COMPRESSION_THRESHOLD UINT_MAX32
#define MIN_COMPRESSION_THRESHOLD 0
ulong compression_threshold_var= DEFAULT_COMPRESSION_THRESHOLD;

/* GTID assignment block size options */
#define DEFAULT_GTID_ASSIGNMENT_BLOCK_SIZE 1000000
#define MIN_GTID_ASSIGNMENT_BLOCK_SIZE 1
#define MAX_GTID_ASSIGNMENT_BLOCK_SIZE MAX_GNO
ulonglong gtid_assignment_block_size_var= DEFAULT_GTID_ASSIGNMENT_BLOCK_SIZE;

/* Flow control options */
ulong flow_control_mode_var= FCM_QUOTA;
#define DEFAULT_FLOW_CONTROL_THRESHOLD 25000
#define MAX_FLOW_CONTROL_THRESHOLD INT_MAX32
#define MIN_FLOW_CONTROL_THRESHOLD 0
int flow_control_certifier_threshold_var= DEFAULT_FLOW_CONTROL_THRESHOLD;
int flow_control_applier_threshold_var= DEFAULT_FLOW_CONTROL_THRESHOLD;

/* Transaction size limits */
#define DEFAULT_TRANSACTION_SIZE_LIMIT 0
#define MAX_TRANSACTION_SIZE_LIMIT 2147483647
#define MIN_TRANSACTION_SIZE_LIMIT 0
ulong transaction_size_limit_var= DEFAULT_TRANSACTION_SIZE_LIMIT;

/* Member Weight limits */
#define DEFAULT_MEMBER_WEIGHT 50
#define MAX_MEMBER_WEIGHT 100
#define MIN_MEMBER_WEIGHT 0
uint member_weight_var= DEFAULT_MEMBER_WEIGHT;

/* Downgrade options */
char allow_local_lower_version_join_var= 0;

/* Allow errand transactions */
char allow_local_disjoint_gtids_join_var= 0;

/* Certification latch */
Wait_ticket<my_thread_id> *certification_latch;

/*
  Internal auxiliary functions signatures.
*/
static int check_group_name_string(const char *str,
                                   bool is_var_update= false);

static int check_recovery_ssl_string(const char *str, const char *var_name,
                                     bool is_var_update= false);

static int check_if_server_properly_configured();

static bool init_group_sidno();

static void initialize_ssl_option_map();

int configure_group_communication(st_server_ssl_variables *ssl_variables);
int configure_group_member_manager(char *hostname, char *uuid,
                                   uint port, unsigned int server_version);
bool check_async_channel_running_on_secondary();
int configure_compatibility_manager();
int initialize_recovery_module();
int configure_and_start_applier_module();
void initialize_asynchronous_channels_observer();
void initialize_group_partition_handler();
int start_group_communication();
void declare_plugin_running();
int leave_group();
int terminate_plugin_modules(bool flag_stop_async_channel= false);
int terminate_applier_module();
int terminate_recovery_module();
void terminate_asynchronous_channels_observer();

/*
  Auxiliary public functions.
*/
void *get_plugin_pointer()
{
  return plugin_info_ptr;
}

mysql_mutex_t* get_plugin_running_lock()
{
  return &plugin_running_mutex;
}

bool plugin_is_group_replication_running()
{
  return group_replication_running;
}

int plugin_group_replication_set_retrieved_certification_info(void* info)
{
  return recovery_module->set_retrieved_cert_info(info);
}

int log_message(enum plugin_log_level level, const char *format, ...)
{
  va_list args;
  char buff[1024];

  va_start(args, format);
  my_vsnprintf(buff, sizeof(buff), format, args);
  va_end(args);
  return my_plugin_log_message(&plugin_info_ptr, level, buff);
}

static void option_deprecation_warning(MYSQL_THD thd, const char* name)
{
  if (thd != NULL)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
                        ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
                        name);
  }
  log_message(MY_WARNING_LEVEL,
              ER_DEFAULT(ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
              name);
}

/*
  Plugin interface.
*/
struct st_mysql_group_replication group_replication_descriptor=
{
  MYSQL_GROUP_REPLICATION_INTERFACE_VERSION,
  plugin_group_replication_start,
  plugin_group_replication_stop,
  plugin_is_group_replication_running,
  plugin_group_replication_set_retrieved_certification_info,
  plugin_get_connection_status,
  plugin_get_group_members,
  plugin_get_group_member_stats,
  plugin_get_group_members_number,
};

bool
plugin_get_connection_status(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks)
{
  char* channel_name= applier_module_channel_name;

  return get_connection_status(callbacks, group_name_var, channel_name,
                               plugin_is_group_replication_running());
}

bool
plugin_get_group_members(
    uint index, const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks)
{
  char* channel_name= applier_module_channel_name;

  return get_group_members_info(index, callbacks, group_member_mgr,
                                group_name_var, channel_name);
}

uint plugin_get_group_members_number()
{
  return group_member_mgr == NULL? 1 :
                                    (uint)group_member_mgr
                                                      ->get_number_of_members();
}

bool
plugin_get_group_member_stats(
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks)
{
  char* channel_name= applier_module_channel_name;

  return get_group_member_stats(callbacks, group_member_mgr, applier_module,
                                gcs_module, group_name_var, channel_name);
}

int plugin_group_replication_start()
{
  DBUG_ENTER("plugin_group_replication_start");

  Mutex_autolock auto_lock_mutex(&plugin_running_mutex);

  DBUG_EXECUTE_IF("group_replication_wait_on_start",
                 {
                   const char act[]= "now signal signal.start_waiting wait_for signal.start_continue";
                   DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                 });

  if (plugin_is_group_replication_running())
    DBUG_RETURN(GROUP_REPLICATION_ALREADY_RUNNING);
  if (check_if_server_properly_configured())
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  if (check_group_name_string(group_name_var))
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  if (check_recovery_ssl_string(recovery_ssl_ca_var, "ssl_ca") ||
      check_recovery_ssl_string(recovery_ssl_capath_var, "ssl_capath") ||
      check_recovery_ssl_string(recovery_ssl_cert_var, "ssl_cert_pointer") ||
      check_recovery_ssl_string(recovery_ssl_cipher_var,
                                "ssl_cipher_pointer") ||
      check_recovery_ssl_string(recovery_ssl_key_var, "ssl_key_pointer") ||
      check_recovery_ssl_string(recovery_ssl_crl_var, "ssl_crl_pointer") ||
      check_recovery_ssl_string(recovery_ssl_crlpath_var,
                                "ssl_crlpath_pointer"))
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  if (!start_group_replication_at_boot_var &&
      !server_engine_initialized())
  {
    log_message(MY_ERROR_LEVEL,
                "Unable to start Group Replication. Replication applier "
                "infrastructure is not initialized since the server was "
                "started with server_id=0. Please, restart the server "
                "with server_id larger than 0.");
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  }
  if (force_members_var != NULL &&
      strlen(force_members_var) > 0)
  {
    log_message(MY_ERROR_LEVEL,
                "group_replication_force_members must be empty "
                "on group start. Current value: '%s'",
                force_members_var);
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  }
  if (init_group_sidno())
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR); /* purecov: inspected */

  if (allow_local_disjoint_gtids_join_var)
  {
    option_deprecation_warning(current_thd,
                               "group_replication_allow_local_disjoint_gtids_join");
  }

  /*
    Instantiate certification latch.
  */
  certification_latch= new Wait_ticket<my_thread_id>();

  // GR delayed initialization.
  if (!server_engine_initialized())
  {
    wait_on_engine_initialization= true;
    plugin_is_auto_starting= false;

    delayed_initialization_thread= new Delayed_initialization_thread();
    if (delayed_initialization_thread->launch_initialization_thread())
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "It was not possible to guarantee the initialization of plugin"
                    " structures on server start");
      delete delayed_initialization_thread;
      delayed_initialization_thread= NULL;
      DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
      /* purecov: end */
    }

    DBUG_RETURN(0); //leave the decision for later
  }

  DBUG_RETURN(initialize_plugin_and_join(PSESSION_DEDICATED_THREAD,
                                         NULL));
}

int initialize_plugin_and_join(enum_plugin_con_isolation sql_api_isolation,
                               Delayed_initialization_thread *delayed_init_thd)
{
  DBUG_ENTER("initialize_plugin_and_join");

  int error= 0;

  //Avoid unnecessary operations
  bool enabled_super_read_only= false;
  bool read_only_mode= false, super_read_only_mode=false;

  st_server_ssl_variables server_ssl_variables=
    {false,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

  char *hostname, *uuid;
  uint port;
  unsigned int server_version;

  Sql_service_command_interface *sql_command_interface=
    new Sql_service_command_interface();

  // GCS interface.
  if ((error= gcs_module->initialize()))
    goto err; /* purecov: inspected */

  // Setup SQL service interface.
  if (sql_command_interface->
    establish_session_connection(sql_api_isolation, plugin_info_ptr))
  {
    error =1; /* purecov: inspected */
    goto err; /* purecov: inspected */
  }

  if (sql_command_interface->set_interface_user(GROUPREPL_USER))
  {
    error =1; /* purecov: inspected */
    goto err; /* purecov: inspected */
  }

  get_read_mode_state(sql_command_interface, &read_only_mode,
                      &super_read_only_mode);

  /*
   At this point in the code, set the super_read_only mode here on the
   server to protect recovery and version module of the Group Replication.
   This can only be done on START command though, on installs there are
   deadlock issues.
  */
  if (!plugin_is_auto_starting &&
      enable_super_read_only_mode(sql_command_interface))
  {
    /* purecov: begin inspected */
    error =1;
    log_message(MY_ERROR_LEVEL,
                "Could not enable the server read only mode and guarantee a "
                  "safe recovery execution");
    goto err;
    /* purecov: end */
  }
  enabled_super_read_only= true;
  if (delayed_init_thd)
    delayed_init_thd->signal_read_mode_ready();

  get_server_parameters(&hostname, &port, &uuid, &server_version,
                        &server_ssl_variables);

  // Setup GCS.
  if ((error= configure_group_communication(&server_ssl_variables)))
  {
    log_message(MY_ERROR_LEVEL,
                "Error on group communication engine initialization");
    goto err;
  }

  // Setup Group Member Manager.
  if ((error= configure_group_member_manager(hostname, uuid, port,
                                             server_version)))
    goto err; /* purecov: inspected */

  if (check_async_channel_running_on_secondary())
  {
    error= 1;
    log_message(MY_ERROR_LEVEL, "Can't start group replication on secondary"
      " member with single primary-mode while"
      " asynchronous replication channels are"
      " running.");
    goto err; /* purecov: inspected */
  }

  configure_compatibility_manager();
  DBUG_EXECUTE_IF("group_replication_compatibility_rule_error_major",
                  {
                    Member_version other_version= plugin_version + (0x010000);
                    Member_version current_version= plugin_version;
                    compatibility_mgr->add_incompatibility(current_version,
                                                           other_version);
                  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_rule_error_minor",
                  {
                    Member_version other_version= plugin_version;
                    Member_version current_version= plugin_version + (0x000100);
                    compatibility_mgr->add_incompatibility(current_version,
                                                           other_version);
                  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_minor_version",
                  {
                    Member_version higher_version= plugin_version + (0x000100);
                    compatibility_mgr->set_local_version(higher_version);
                  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_major_version",
                  {
                    Member_version higher_version= plugin_version + (0x010000);
                    compatibility_mgr->set_local_version(higher_version);
                  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_restore_version",
                  {
                    Member_version current_version= plugin_version;
                    compatibility_mgr->set_local_version(current_version);
                  };);

  // need to be initialized before applier, is called on kill_pending_transactions
  blocked_transaction_handler= new Blocked_transaction_handler();

  if ((error= initialize_recovery_module()))
    goto err; /* purecov: inspected */

  //we can only start the applier if the log has been initialized
  if (configure_and_start_applier_module())
  {
    error= GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR;
    goto err;
  }

  initialize_asynchronous_channels_observer();
  initialize_group_partition_handler();

  DBUG_EXECUTE_IF("group_replication_before_joining_the_group",
                  {
                    const char act[]= "now wait_for signal.continue_group_join";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  });

  if ((error= start_group_communication()))
  {
    log_message(MY_ERROR_LEVEL,
                "Error on group communication engine start");
    goto err;
  }

  if (view_change_notifier->wait_for_view_modification())
  {
    if (!view_change_notifier->is_cancelled())
    {
      //Only log a error when a view modification was not cancelled.
      log_message(MY_ERROR_LEVEL,
                  "Timeout on wait for view after joining group");
    }
    error= view_change_notifier->get_error();
    goto err;
  }
  group_replication_running= true;
  log_primary_member_details();

err:
  if (error)
  {
    //Unblock the possible stuck delayed thread
    if (delayed_init_thd)
      delayed_init_thd->signal_read_mode_ready();
    leave_group();
    terminate_plugin_modules();

    if (!server_shutdown_status && server_engine_initialized()
        && enabled_super_read_only)
    {
      set_read_mode_state(sql_command_interface, read_only_mode,
                          super_read_only_mode);
    }

    if (certification_latch != NULL)
    {
      delete certification_latch; /* purecov: inspected */
      certification_latch= NULL;  /* purecov: inspected */
    }
  }

  delete sql_command_interface;
  plugin_is_auto_starting= false;

  DBUG_RETURN(error);
}

int configure_group_member_manager(char *hostname, char *uuid,
                                   uint port, unsigned int server_version)
{
  DBUG_ENTER("configure_group_member_manager");

  /*
    Ensure that group communication interfaces are initialized
    and ready to use, since plugin can leave the group on errors
    but continue to be active.
  */
  std::string gcs_local_member_identifier;
  if (gcs_module->get_local_member_identifier(gcs_local_member_identifier))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL, "Error calling group communication interfaces");
    DBUG_RETURN(GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR);
    /* purecov: end */
  }

  if (!strcmp(uuid, group_name_var))
  {
    log_message(MY_ERROR_LEVEL,
                "Member server_uuid is incompatible with the group. "
                "Server_uuid %s matches group_name %s.", uuid, group_name_var);
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  }
  //Configure Group Member Manager
  plugin_version= server_version;

  uint32 local_version= plugin_version;
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_patch_version",
                  {
                    local_version= plugin_version + (0x000001);
                  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_minor_version",
                  {
                    local_version= plugin_version + (0x000100);
                  };);
  DBUG_EXECUTE_IF("group_replication_compatibility_higher_major_version",
                  {
                    local_version= plugin_version + (0x010000);
                  };);
  Member_version local_member_plugin_version(local_version);

  DBUG_EXECUTE_IF("group_replication_force_member_uuid",
                  {
                    uuid= const_cast<char*>("cccccccc-cccc-cccc-cccc-cccccccccccc");
                  };);
  delete local_member_info;
  local_member_info= new Group_member_info(hostname,
                                           port,
                                           uuid,
                                           write_set_extraction_algorithm,
                                           gcs_local_member_identifier,
                                           Group_member_info::MEMBER_OFFLINE,
                                           local_member_plugin_version,
                                           gtid_assignment_block_size_var,
                                           Group_member_info::MEMBER_ROLE_SECONDARY,
                                           single_primary_mode_var,
                                           enforce_update_everywhere_checks_var,
                                           member_weight_var,
                                           gr_lower_case_table_names);

  //Create the membership info visible for the group
  delete group_member_mgr;
  group_member_mgr= new Group_member_info_manager(local_member_info);

  log_message(MY_INFORMATION_LEVEL,
              "Member configuration: "
              "member_id: %lu; "
              "member_uuid: \"%s\"; "
              "single-primary mode: \"%s\"; "
              "group_replication_auto_increment_increment: %lu; ",
              get_server_id(),
              (local_member_info != NULL) ? local_member_info->get_uuid().c_str() : "NULL",
              single_primary_mode_var ? "true" : "false",
              auto_increment_increment_var);

  DBUG_RETURN(0);
}

void init_compatibility_manager()
{
  if (compatibility_mgr != NULL)
  {
    delete compatibility_mgr; /* purecov: inspected */
  }

  compatibility_mgr= new Compatibility_module();
}


int configure_compatibility_manager()
{
  Member_version local_member_version(plugin_version);
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

  return 0;
}

int leave_group()
{
  if (gcs_module->belongs_to_group())
  {
    view_change_notifier->start_view_modification();

    Gcs_operations::enum_leave_state state= gcs_module->leave();

    std::stringstream ss;
    plugin_log_level log_severity= MY_WARNING_LEVEL;
    switch (state)
    {
      case Gcs_operations::ERROR_WHEN_LEAVING:
        /* purecov: begin inspected */
        ss << "Unable to confirm whether the server has left the group or not. "
              "Check performance_schema.replication_group_members to check group membership information.";
        log_severity= MY_ERROR_LEVEL;
        break;
        /* purecov: end */
      case Gcs_operations::ALREADY_LEAVING:
        ss << "Skipping leave operation: concurrent attempt to leave the group is on-going.";
        break;
      case Gcs_operations::ALREADY_LEFT:
        /* purecov: begin inspected */
        ss << "Skipping leave operation: member already left the group.";
        break;
        /* purecov: end */
      case Gcs_operations::NOW_LEAVING:
        goto bypass_message;
    }
    log_message(log_severity, ss.str().c_str());
bypass_message:
    //Wait anyway
    log_message(MY_INFORMATION_LEVEL, "Going to wait for view modification");
    if (view_change_notifier->wait_for_view_modification())
    {
      log_message(MY_WARNING_LEVEL,
                  "On shutdown there was a timeout receiving a view change. "
                  "This can lead to a possible inconsistent state. "
                  "Check the log for more details");
    }
  }
  else
  {
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
    log_message(MY_INFORMATION_LEVEL,
                "Requesting to leave the group despite of not "
                "being a member");
    gcs_module->leave();
  }

  // Finalize GCS.
  gcs_module->finalize();

  if (auto_increment_handler != NULL)
  {
    auto_increment_handler->reset_auto_increment_variables();
  }

  // Destroy handlers and notifiers
  delete events_handler;
  events_handler= NULL;

  return 0;
}

int plugin_group_replication_stop()
{
  DBUG_ENTER("plugin_group_replication_stop");

  Mutex_autolock auto_lock_mutex(&plugin_running_mutex);

  DBUG_EXECUTE_IF("group_replication_wait_on_stop",
                 {
                   const char act[]= "now signal signal.stop_waiting wait_for signal.stop_continue";
                   DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
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
  if (delayed_initialization_thread != NULL)
  {
    wait_on_engine_initialization= false;
    delayed_initialization_thread->signal_thread_ready();
    delayed_initialization_thread->wait_for_thread_end();
    delete delayed_initialization_thread;
    delayed_initialization_thread= NULL;
  }

  shared_plugin_stop_lock->grab_write_lock();
  if (!plugin_is_group_replication_running())
  {
    shared_plugin_stop_lock->release_write_lock();
    DBUG_RETURN(0);
  }
  log_message(MY_INFORMATION_LEVEL,
              "Plugin 'group_replication' is stopping.");

  plugin_is_waiting_to_set_server_read_mode= true;

  // wait for all transactions waiting for certification
  bool timeout=
      certification_latch->block_until_empty(TRANSACTION_KILL_TIMEOUT);
  if (timeout)
  {
    //if they are blocked, kill them
    blocked_transaction_handler->unblock_waiting_transactions();
  }

  /* first leave all joined groups (currently one) */
  leave_group();

  int error= terminate_plugin_modules(true);

  group_replication_running= false;
  shared_plugin_stop_lock->release_write_lock();
  log_message(MY_INFORMATION_LEVEL,
              "Plugin 'group_replication' has been stopped.");

  // Enable super_read_only.
  if (!server_shutdown_status &&
      !plugin_is_being_uninstalled &&
      server_engine_initialized())
  {
    if (enable_server_read_mode(PSESSION_DEDICATED_THREAD))
    {
      log_message(MY_ERROR_LEVEL,
                  "On plugin shutdown it was not possible to enable the "
                  "server read only mode. Local transactions will be accepted "
                  "and committed."); /* purecov: inspected */
    }
    plugin_is_waiting_to_set_server_read_mode= false;
  }

  DBUG_RETURN(error);
}

int terminate_plugin_modules(bool flag_stop_async_channel)
{

  if(terminate_recovery_module())
  {
    //Do not throw an error since recovery is not vital, but warn either way
    log_message(MY_WARNING_LEVEL,
                "On shutdown there was a timeout on the Group Replication "
                "recovery module termination. Check the log for more details"); /* purecov: inspected */
  }

  DBUG_EXECUTE_IF("group_replication_after_recovery_module_terminated",
                 {
                   const char act[]= "now wait_for signal.termination_continue";
                   DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                 });

  /*
    The applier is only shutdown after the communication layer to avoid
    messages being delivered in the current view, but not applied
  */
  int error= 0;
  if((error= terminate_applier_module()))
  {
    log_message(MY_ERROR_LEVEL,
                "On shutdown there was a timeout on the Group Replication"
                " applier termination.");
  }

  terminate_asynchronous_channels_observer();

  if (flag_stop_async_channel)
  {
    int channel_err= channel_stop_all(CHANNEL_APPLIER_THREAD|CHANNEL_RECEIVER_THREAD,
                            components_stop_timeout_var);
    if (channel_err)
    {
      log_message(MY_ERROR_LEVEL,
                  "Error stopping all replication channels while server was"
                  " leaving the group. Please check the error log for "
                  "additional details. Got error: %d", channel_err);
      if (!error)
      {
        error= GROUP_REPLICATION_CONFIGURATION_ERROR;
      }
    }
  }

  delete group_partition_handler;
  group_partition_handler= NULL;

  delete blocked_transaction_handler;
  blocked_transaction_handler= NULL;

  /*
    Destroy certification latch.
  */
  if (certification_latch != NULL)
  {
    delete certification_latch;
    certification_latch= NULL;
  }

  /*
    Clear server sessions opened caches on transactions observer.
  */
  observer_trans_clear_io_cache_unused_list();

  if (group_member_mgr != NULL && local_member_info != NULL)
  {
    group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                           Group_member_info::MEMBER_OFFLINE);
  }

  return error;
}

int plugin_group_replication_init(MYSQL_PLUGIN plugin_info)
{
  // Register all PSI keys at the time plugin init
#ifdef HAVE_PSI_INTERFACE
  register_all_group_replication_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

  mysql_mutex_init(key_GR_LOCK_plugin_running, &plugin_running_mutex,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_force_members_running,
                   &force_members_running_mutex,
                   MY_MUTEX_INIT_FAST);

  plugin_stop_lock= new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
                                         key_GR_RWLOCK_plugin_stop
#endif /* HAVE_PSI_INTERFACE */
                                        );

  shared_plugin_stop_lock= new Shared_writelock(plugin_stop_lock);

  //Initialize transactions observer structures
  observer_trans_initialize();

  plugin_info_ptr= plugin_info;

  if (group_replication_init())
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Failure during Group Replication handler initialization");
    return 1;
    /* purecov: end */
  }

  if(register_server_state_observer(&server_state_observer,
                                    (void *)plugin_info_ptr))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Failure when registering the server state observers");
    return 1;
    /* purecov: end */
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Failure when registering the transactions state observers");
    return 1;
    /* purecov: end */
  }

  if (register_binlog_transmit_observer(&binlog_transmit_observer,
                                        (void *)plugin_info_ptr))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Failure when registering the binlog state observers");
    return 1;
    /* purecov: end */
  }

  //Initialize the recovery SSL option map
  initialize_ssl_option_map();

  //Initialize channel observation and auto increment handlers before start
  auto_increment_handler= new Plugin_group_replication_auto_increment();
  channel_observation_manager= new Channel_observation_manager(plugin_info);
  view_change_notifier= new Plugin_gcs_view_modification_notifier();
  gcs_module= new Gcs_operations();

  //Initialize the compatibility module before starting
  init_compatibility_manager();

  plugin_is_auto_starting= start_group_replication_at_boot_var;
  if (start_group_replication_at_boot_var && plugin_group_replication_start())
  {
    log_message(MY_ERROR_LEVEL,
                "Unable to start Group Replication on boot");
  }

  return 0;
}

int plugin_group_replication_deinit(void *p)
{
  // If plugin was not initialized, there is nothing to do here.
  if (plugin_info_ptr == NULL)
    return 0;

  plugin_is_being_uninstalled= true;
  int observer_unregister_error= 0;

  if (plugin_group_replication_stop())
    log_message(MY_ERROR_LEVEL,
                "Failure when stopping Group Replication on plugin uninstall");

  if (group_member_mgr != NULL)
  {
    delete group_member_mgr;
    group_member_mgr= NULL;
  }

  if (local_member_info != NULL)
  {
    delete local_member_info;
    local_member_info= NULL;
  }

  if (compatibility_mgr != NULL)
  {
    delete compatibility_mgr;
    compatibility_mgr= NULL;
  }

  if (unregister_server_state_observer(&server_state_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when unregistering the server state observers");
    observer_unregister_error++;
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when unregistering the transactions state observers");
    observer_unregister_error++;
  }

  if (unregister_binlog_transmit_observer(&binlog_transmit_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when unregistering the binlog state observers");
    observer_unregister_error++;
  }

  if (observer_unregister_error == 0)
    log_message(MY_INFORMATION_LEVEL,
                "All Group Replication server observers"
                " have been successfully unregistered");

  if (channel_observation_manager != NULL)
  {
    delete channel_observation_manager;
    channel_observation_manager= NULL;
  }

  delete gcs_module;
  gcs_module= NULL;

  delete view_change_notifier;
  view_change_notifier= NULL;

  if(auto_increment_handler != NULL)
  {
    delete auto_increment_handler;
    auto_increment_handler= NULL;
  }

  mysql_mutex_destroy(&plugin_running_mutex);
  mysql_mutex_destroy(&force_members_running_mutex);

  delete shared_plugin_stop_lock;
  shared_plugin_stop_lock= NULL;
  delete plugin_stop_lock;
  plugin_stop_lock= NULL;

  //Terminate transactions observer structures
  observer_trans_terminate();

  plugin_info_ptr= NULL;

  return observer_unregister_error;
}

static bool init_group_sidno()
{
  DBUG_ENTER("init_group_sidno");
  rpl_sid group_sid;

  if (group_sid.parse(group_name_var) != RETURN_STATUS_OK)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL, "Unable to parse the group name.");
    DBUG_RETURN(true);
    /* purecov: end */
  }

  group_sidno = get_sidno_from_global_sid_map(group_sid);
  if (group_sidno <= 0)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL, "Unable to generate the sidno for the group.");
    DBUG_RETURN(true);
    /* purecov: end */
  }

  DBUG_RETURN(false);
}

void declare_plugin_running()
{
  group_replication_running= true;
}

int configure_and_start_applier_module()
{
  DBUG_ENTER("configure_and_start_applier_module");

  int error= 0;

  //The applier did not stop properly or suffered a configuration error
  if (applier_module != NULL)
  {
    if ((error= applier_module->is_running())) //it is still running?
    {
      log_message(MY_ERROR_LEVEL,
                  "Cannot start the Group Replication applier as a previous "
                  "shutdown is still running: "
                  "The thread will stop once its task is complete.");
      DBUG_RETURN(error);
    }
    else
    {
      //clean a possible existent pipeline
      applier_module->terminate_applier_pipeline();
      //delete it and create from scratch
      delete applier_module;
    }
  }

  applier_module= new Applier_module();

  recovery_module->set_applier_module(applier_module);

  //For now, only defined pipelines are accepted.
  error=
    applier_module->setup_applier_module(STANDARD_GROUP_REPLICATION_PIPELINE,
                                         known_server_reset,
                                         components_stop_timeout_var,
                                         group_sidno,
                                         gtid_assignment_block_size_var,
                                         shared_plugin_stop_lock);
  if (error)
  {
    //Delete the possible existing pipeline
    applier_module->terminate_applier_pipeline();
    delete applier_module;
    applier_module= NULL;
    DBUG_RETURN(error);
  }

  known_server_reset= false;

  if ((error= applier_module->initialize_applier_thread()))
  {
    log_message(MY_ERROR_LEVEL,
                "Unable to initialize the Group Replication applier module.");
    //terminate the applier_thread if running
    if (!applier_module->terminate_applier_thread())
    {
      delete applier_module;
      applier_module= NULL;
    }
  }
  else
    log_message(MY_INFORMATION_LEVEL,
                "Group Replication applier module successfully initialized!");

  DBUG_RETURN(error);
}

void initialize_group_partition_handler()
{
  group_partition_handler=
      new Group_partition_handling(shared_plugin_stop_lock,
                                   timeout_on_unreachable_var);
}

int terminate_applier_module()
{

  int error= 0;
  if (applier_module != NULL)
  {
    if (!applier_module->terminate_applier_thread()) //all goes fine
    {
      delete applier_module;
      applier_module= NULL;
    }
    else
    {
      error= GROUP_REPLICATION_APPLIER_STOP_TIMEOUT;
    }
  }
  return error;
}

int configure_group_communication(st_server_ssl_variables *ssl_variables)
{
  DBUG_ENTER("configure_group_communication");

  // GCS interface parameters.
  Gcs_interface_parameters gcs_module_parameters;
  gcs_module_parameters.add_parameter("group_name",
                                      std::string(group_name_var));
  if (local_address_var != NULL)
    gcs_module_parameters.add_parameter("local_node",
                                        std::string(local_address_var));
  if (group_seeds_var != NULL)
    gcs_module_parameters.add_parameter("peer_nodes",
                                        std::string(group_seeds_var));
  const std::string bootstrap_group_string=
      bootstrap_group_var ? "true" : "false";
  gcs_module_parameters.add_parameter("bootstrap_group", bootstrap_group_string);
  std::stringstream poll_spin_loops_stream_buffer;
  poll_spin_loops_stream_buffer << poll_spin_loops_var;
  gcs_module_parameters.add_parameter("poll_spin_loops",
                                      poll_spin_loops_stream_buffer.str());

  // Compression parameter
  if (compression_threshold_var > 0)
  {
    std::stringstream ss;
    ss << compression_threshold_var;
    gcs_module_parameters.add_parameter("compression", std::string("on"));
    gcs_module_parameters.add_parameter("compression_threshold", ss.str());
  }
  else
  {
    gcs_module_parameters.add_parameter("compression", std::string("off")); /* purecov: inspected */
  }

  // SSL parameters.
  std::string ssl_mode(ssl_mode_values[ssl_mode_var]);
  if (ssl_mode_var > 0)
  {
    std::string ssl_key(ssl_variables->ssl_key ? ssl_variables->ssl_key : "");
    std::string ssl_cert(ssl_variables->ssl_cert ? ssl_variables->ssl_cert : "");
    std::string ssl_ca(ssl_variables->ssl_ca ? ssl_variables->ssl_ca : "");
    std::string ssl_capath(ssl_variables->ssl_capath ? ssl_variables->ssl_capath : "");
    std::string ssl_cipher(ssl_variables->ssl_cipher ? ssl_variables->ssl_cipher : "");
    std::string ssl_crl(ssl_variables->ssl_crl ? ssl_variables->ssl_crl : "");
    std::string ssl_crlpath(ssl_variables->ssl_crlpath ? ssl_variables->ssl_crlpath : "");
    std::string tls_version(ssl_variables->tls_version? ssl_variables->tls_version : "");

    // SSL support on server.
    if (ssl_variables->have_ssl_opt)
    {
      gcs_module_parameters.add_parameter("ssl_mode", ssl_mode);
      gcs_module_parameters.add_parameter("server_key_file", ssl_key);
      gcs_module_parameters.add_parameter("server_cert_file", ssl_cert);
      gcs_module_parameters.add_parameter("client_key_file", ssl_key);
      gcs_module_parameters.add_parameter("client_cert_file", ssl_cert);
      gcs_module_parameters.add_parameter("ca_file", ssl_ca);
      if (!ssl_capath.empty())
        gcs_module_parameters.add_parameter("ca_path", ssl_capath); /* purecov: inspected */
      gcs_module_parameters.add_parameter("cipher", ssl_cipher);
      gcs_module_parameters.add_parameter("tls_version", tls_version);

#if !defined(HAVE_YASSL)
      // YaSSL does not support CRL.
      if (!ssl_crl.empty())
        gcs_module_parameters.add_parameter("crl_file", ssl_crl); /* purecov: inspected */
      if (!ssl_crlpath.empty())
        gcs_module_parameters.add_parameter("crl_path", ssl_crlpath); /* purecov: inspected */
#endif

      log_message(MY_INFORMATION_LEVEL,
                  "Group communication SSL configuration: "
                  "group_replication_ssl_mode: \"%s\"; "
                  "server_key_file: \"%s\"; "
                  "server_cert_file: \"%s\"; "
                  "client_key_file: \"%s\"; "
                  "client_cert_file: \"%s\"; "
                  "ca_file: \"%s\"; "
                  "ca_path: \"%s\"; "
                  "cipher: \"%s\"; "
                  "tls_version: \"%s\"; "
                  "crl_file: \"%s\"; "
                  "crl_path: \"%s\"",
                  ssl_mode.c_str(), ssl_key.c_str(), ssl_cert.c_str(),
                  ssl_key.c_str(), ssl_cert.c_str(), ssl_ca.c_str(),
                  ssl_capath.c_str(), ssl_cipher.c_str(), tls_version.c_str(),
                  ssl_crl.c_str(), ssl_crlpath.c_str());
    }
    // No SSL support on server.
    else
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "MySQL server does not have SSL support and "
                  "group_replication_ssl_mode is \"%s\", START "
                  "GROUP_REPLICATION will abort", ssl_mode.c_str());
      DBUG_RETURN(GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR);
      /* purecov: end */
    }
  }
  // GCS SSL disabled.
  else
  {
    gcs_module_parameters.add_parameter("ssl_mode", ssl_mode);
    log_message(MY_INFORMATION_LEVEL,
                "Group communication SSL configuration: "
                "group_replication_ssl_mode: \"%s\"", ssl_mode.c_str());
  }

  if (ip_whitelist_var != NULL)
  {
    std::string v(ip_whitelist_var);
    v.erase(std::remove(v.begin(), v.end(), ' '), v.end());
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    // if the user specified a list other than automatic
    // then we need to pass it to the GCS, otherwise we
    // do nothing and let GCS scan for the proper IPs
    if (v.find("automatic") == std::string::npos)
    {
      gcs_module_parameters.add_parameter("ip_whitelist",
                                          std::string(ip_whitelist_var));
    }
  }

  // Configure GCS.
  if (gcs_module->configure(gcs_module_parameters))
  {
    log_message(MY_ERROR_LEVEL,
                "Unable to initialize the group communication engine");
    DBUG_RETURN(GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR);
  }
  log_message(MY_INFORMATION_LEVEL,
              "Initialized group communication with configuration: "
              "group_replication_group_name: \"%s\"; "
              "group_replication_local_address: \"%s\"; "
              "group_replication_group_seeds: \"%s\"; "
              "group_replication_bootstrap_group: %s; "
              "group_replication_poll_spin_loops: %lu; "
              "group_replication_compression_threshold: %lu; "
              "group_replication_ip_whitelist: \"%s\"",
              group_name_var, local_address_var, group_seeds_var,
              bootstrap_group_var ? "true" : "false",
              poll_spin_loops_var, compression_threshold_var,
              ip_whitelist_var);

  DBUG_RETURN(0);
}

int start_group_communication()
{
  DBUG_ENTER("start_group_communication");

  if (auto_increment_handler != NULL)
  {
    auto_increment_handler->
      set_auto_increment_variables(auto_increment_increment_var,
                                   get_server_id());
  }

  events_handler= new Plugin_gcs_events_handler(applier_module,
                                                recovery_module,
                                                view_change_notifier,
                                                compatibility_mgr,
                                                components_stop_timeout_var);

  view_change_notifier->start_view_modification();

  if (gcs_module->join(*events_handler, *events_handler))
    DBUG_RETURN(GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR);

  DBUG_RETURN(0);
}

bool check_async_channel_running_on_secondary()
{
  /* To stop group replication to start on secondary member with single primary-
     mode, when any async channels are running, we verify whether member is not
    bootstrapping. As only when the member is bootstrapping, it can be the
    primary leader on a single primary member context.
  */
  if (single_primary_mode_var && !bootstrap_group_var)
  {
    if (is_any_slave_channel_running(
        CHANNEL_RECEIVER_THREAD | CHANNEL_APPLIER_THREAD))
    {
      return true;
    }
  }

  return false;
}

void initialize_asynchronous_channels_observer()
{
  if (single_primary_mode_var)
  {
    asynchronous_channels_state_observer=
        new Asynchronous_channels_state_observer();
    channel_observation_manager
        ->register_channel_observer(asynchronous_channels_state_observer);
  }
}

void terminate_asynchronous_channels_observer()
{
  if (asynchronous_channels_state_observer != NULL)
  {
    channel_observation_manager
      ->unregister_channel_observer(asynchronous_channels_state_observer);
    delete asynchronous_channels_state_observer;
    asynchronous_channels_state_observer= NULL;
  }
}

int initialize_recovery_module()
{
  recovery_module = new Recovery_module(applier_module,
                                        channel_observation_manager,
                                        components_stop_timeout_var);

  recovery_module->set_recovery_ssl_options(recovery_use_ssl_var,
                                            recovery_ssl_ca_var,
                                            recovery_ssl_capath_var,
                                            recovery_ssl_cert_var,
                                            recovery_ssl_cipher_var,
                                            recovery_ssl_key_var,
                                            recovery_ssl_crl_var,
                                            recovery_ssl_crlpath_var,
                                            recovery_ssl_verify_server_cert_var);
  recovery_module->
      set_recovery_completion_policy(
          (enum_recovery_completion_policies) recovery_completion_policy_var);
  recovery_module->set_recovery_donor_retry_count(recovery_retry_count_var);
  recovery_module->
      set_recovery_donor_reconnect_interval(recovery_reconnect_interval_var);

  return 0;
}

int terminate_recovery_module()
{
  int error= 0;
  if(recovery_module != NULL)
  {
    error = recovery_module->stop_recovery();
    delete recovery_module;
    recovery_module= NULL;
  }
  return error;
}

bool server_engine_initialized()
{
  //check if empty channel exists, i.e, the slave structures are initialized
  return channel_is_active("", CHANNEL_NO_THD);
}

void register_server_reset_master(){
  known_server_reset= true;
}

bool get_allow_local_lower_version_join()
{
  DBUG_ENTER("get_allow_local_lower_version_join");
  DBUG_RETURN(allow_local_lower_version_join_var);
}

bool get_allow_local_disjoint_gtids_join()
{
  DBUG_ENTER("get_allow_local_disjoint_gtids_join");
  DBUG_RETURN(allow_local_disjoint_gtids_join_var);
}

ulong get_transaction_size_limit()
{
  DBUG_ENTER("get_transaction_size_limit");
  DBUG_RETURN(transaction_size_limit_var);
}

bool is_plugin_waiting_to_set_server_read_mode()
{
  DBUG_ENTER("is_plugin_waiting_to_set_server_read_mode");
  DBUG_RETURN(plugin_is_waiting_to_set_server_read_mode);
}

/*
  This method is used to accomplish the startup validations of the plugin
  regarding system configuration.

  It currently verifies:
  - Binlog enabled
  - Binlog checksum mode
  - Binlog format
  - Gtid mode
  - LOG_SLAVE_UPDATES
  - Single primary mode configuration

  @return If the operation succeed or failed
    @retval 0 in case of success
    @retval 1 in case of failure
 */
static int check_if_server_properly_configured()
{
  DBUG_ENTER("check_if_server_properly_configured");

  //Struct that holds startup and runtime requirements
  Trans_context_info startup_pre_reqs;

  get_server_startup_prerequirements(startup_pre_reqs, !plugin_is_auto_starting);

  if(!startup_pre_reqs.binlog_enabled)
  {
    log_message(MY_ERROR_LEVEL, "Binlog must be enabled for Group Replication");
    DBUG_RETURN(1);
  }

  if(startup_pre_reqs.binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF)
  {
    log_message(MY_ERROR_LEVEL, "binlog_checksum should be NONE for Group Replication");
    DBUG_RETURN(1);
  }

  if(startup_pre_reqs.binlog_format != BINLOG_FORMAT_ROW)
  {
    log_message(MY_ERROR_LEVEL, "Binlog format should be ROW for Group Replication");
    DBUG_RETURN(1);
  }

  if(startup_pre_reqs.gtid_mode != GTID_MODE_ON)
  {
    log_message(MY_ERROR_LEVEL, "Gtid mode should be ON for Group Replication");
    DBUG_RETURN(1);
  }

  if(startup_pre_reqs.log_slave_updates != true)
  {
    log_message(MY_ERROR_LEVEL,
                "LOG_SLAVE_UPDATES should be ON for Group Replication");
    DBUG_RETURN(1);
  }

  if(startup_pre_reqs.transaction_write_set_extraction ==
     HASH_ALGORITHM_OFF)
  {
    log_message(MY_ERROR_LEVEL,
                "Extraction of transaction write sets requires an hash algorithm "
                "configuration. Please, double check that the parameter "
                "transaction-write-set-extraction is set to a valid algorithm.");
    DBUG_RETURN(1);
  }
  else
  {
    write_set_extraction_algorithm=
       startup_pre_reqs.transaction_write_set_extraction;
  }

  if (startup_pre_reqs.mi_repository_type != 1) //INFO_REPOSITORY_TABLE
  {
    log_message(MY_ERROR_LEVEL, "Master info repository must be set to TABLE.");
    DBUG_RETURN(1);
  }

  if (startup_pre_reqs.rli_repository_type != 1) //INFO_REPOSITORY_TABLE
  {
    log_message(MY_ERROR_LEVEL, "Relay log info repository must be set to TABLE");
    DBUG_RETURN(1);
  }

  if (startup_pre_reqs.parallel_applier_workers > 0)
  {
    if (startup_pre_reqs.parallel_applier_type != CHANNEL_MTS_PARALLEL_TYPE_LOGICAL_CLOCK)
    {
      log_message(MY_ERROR_LEVEL,
                  "In order to use parallel applier on Group Replication, parameter "
                  "slave-parallel-type must be set to 'LOGICAL_CLOCK'.");
      DBUG_RETURN(1);
    }

    if (!startup_pre_reqs.parallel_applier_preserve_commit_order)
    {
      log_message(MY_WARNING_LEVEL,
                  "Group Replication requires slave-preserve-commit-order "
                  "to be set to ON when using more than 1 applier threads.");
      DBUG_RETURN(1);
    }
  }

  if (single_primary_mode_var && enforce_update_everywhere_checks_var)
  {
    log_message(MY_ERROR_LEVEL,
                "Is is not allowed to run single primary mode with "
                "'enforce_update_everywhere_checks' enabled.");
    DBUG_RETURN(1);
  }

  gr_lower_case_table_names= startup_pre_reqs.lower_case_table_names;
  DBUG_ASSERT (gr_lower_case_table_names <= 2);
#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("group_replication_skip_encode_lower_case_table_names",
                {
                  gr_lower_case_table_names = SKIP_ENCODING_LOWER_CASE_TABLE_NAMES;
                });
#endif


  DBUG_RETURN(0);
}

static int check_group_name_string(const char *str, bool is_var_update)
{
  DBUG_ENTER("check_group_name_string");

  if (!str)
  {
    if(!is_var_update)
      log_message(MY_ERROR_LEVEL, "The group name option is mandatory");
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group name option is mandatory",
                 MYF(0)); /* purecov: inspected */
    DBUG_RETURN(1);
  }

  if (strlen(str) > UUID_LENGTH)
  {
    if(!is_var_update)
      log_message(MY_ERROR_LEVEL, "The group name '%s' is not a valid UUID, its"
                  " length is too big", str);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                 "The group name is not a valid UUID, its length is too big",
                 MYF(0));
    DBUG_RETURN(1);
  }

  if (!Uuid::is_valid(str))
  {
    if(!is_var_update)
      log_message(MY_ERROR_LEVEL, "The group name '%s' is not a valid UUID", str); /* purecov: inspected */
    else
      my_message(ER_WRONG_VALUE_FOR_VAR, "The group name is not a valid UUID",
                 MYF(0));
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

static int check_group_name(MYSQL_THD thd, SYS_VAR *var, void* save,
                            struct st_mysql_value *value)
{
  DBUG_ENTER("check_group_name");

  char buff[NAME_CHAR_LEN];
  const char *str;

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The group name cannot be changed when Group Replication is running",
               MYF(0));
    DBUG_RETURN(1);
  }

  (*(const char **) save)= NULL;

  int length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= thd->strmake(str, length);
  else
    DBUG_RETURN(1); /* purecov: inspected */

  if (check_group_name_string(str, true))
    DBUG_RETURN(1);

  *(const char**)save= str;

  DBUG_RETURN(0);
}

//Recovery module's module variable update/validate methods

static void update_recovery_retry_count(MYSQL_THD thd, SYS_VAR *var,
                                        void *var_ptr, const void *save)
{
  DBUG_ENTER("update_recovery_retry_count");

  (*(ulong*) var_ptr)= (*(ulong*) save);
  ulong in_val= *static_cast<const ulong*>(save);

  if (recovery_module != NULL)
  {
    recovery_module->set_recovery_donor_retry_count(in_val);
  }

  DBUG_VOID_RETURN;
}

static void update_recovery_reconnect_interval(MYSQL_THD thd, SYS_VAR *var,
                                               void *var_ptr, const void *save)
{
  DBUG_ENTER("update_recovery_reconnect_interval");

  (*(ulong*) var_ptr)= (*(ulong*) save);
  ulong in_val= *static_cast<const ulong*>(save);

  if (recovery_module != NULL)
  {
    recovery_module->
        set_recovery_donor_reconnect_interval(in_val);
  }

  DBUG_VOID_RETURN;
}

//Recovery SSL options

static void
update_ssl_use(MYSQL_THD thd, SYS_VAR *var,
               void *var_ptr, const void *save)
{
  DBUG_ENTER("update_ssl_use");

  bool use_ssl_val= *((my_bool *) save);
  (*(my_bool *) var_ptr)= (*(my_bool *) save);

  if (recovery_module != NULL)
  {
      recovery_module->set_recovery_use_ssl(use_ssl_val);
  }

  DBUG_VOID_RETURN;
}

static int check_recovery_ssl_string(const char *str, const char *var_name,
                                     bool is_var_update)
{
  DBUG_ENTER("check_recovery_ssl_string");

  if (strlen(str) > FN_REFLEN)
  {
    if(!is_var_update)
      log_message(MY_ERROR_LEVEL,
                  "The given value for recovery ssl option '%s' is invalid"
                  " as its length is beyond the limit", var_name);
    else
      my_message(ER_WRONG_VALUE_FOR_VAR,
                  "The given value for recovery ssl option is invalid"
                  " as its length is beyond the limit",
                 MYF(0));
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

static int check_recovery_ssl_option(MYSQL_THD thd, SYS_VAR *var, void* save,
                                     struct st_mysql_value *value)
{
  DBUG_ENTER("check_recovery_ssl_option");

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str= NULL;

  (*(const char **) save)= NULL;

  int length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= thd->strmake(str, length);
  else
    DBUG_RETURN(1); /* purecov: inspected */

  if (str != NULL && check_recovery_ssl_string(str, var->name, true))
  {
    DBUG_RETURN(1);
  }

  *(const char**)save= str;

  DBUG_RETURN(0);
}

static void update_recovery_ssl_option(MYSQL_THD thd, SYS_VAR *var,
                                       void *var_ptr, const void *save)
{
  DBUG_ENTER("update_recovery_ssl_option");


  const char *new_option_val= *(const char**)save;
  (*(const char **) var_ptr)= (*(const char **) save);

  //According to the var name, get the operation code and act accordingly
  switch(recovery_ssl_opt_map[var->name])
  {
    case RECOVERY_SSL_CA_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_ca(new_option_val);
      break;
    case RECOVERY_SSL_CAPATH_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_capath(new_option_val);
      break;
    case RECOVERY_SSL_CERT_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_cert(new_option_val);
      break;
    case RECOVERY_SSL_CIPHER_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_cipher(new_option_val);
      break;
    case RECOVERY_SSL_KEY_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_key(new_option_val);
      break;
    case RECOVERY_SSL_CRL_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_crl(new_option_val);
      break;
    case RECOVERY_SSL_CRLPATH_OPT:
      if (recovery_module != NULL)
        recovery_module->set_recovery_ssl_crlpath(new_option_val);
      break;
    default:
      DBUG_ASSERT(0); /* purecov: inspected */
  }

  DBUG_VOID_RETURN;
}

static void
update_ssl_server_cert_verification(MYSQL_THD thd, SYS_VAR *var,
                                    void *var_ptr, const void *save)
{
  DBUG_ENTER("update_ssl_server_cert_verification");

  bool ssl_verify_server_cert= *((my_bool *) save);
  (*(my_bool *) var_ptr)= (*(my_bool *) save);

  if (recovery_module != NULL)
  {
    recovery_module->
        set_recovery_ssl_verify_server_cert(ssl_verify_server_cert);
  }

  DBUG_VOID_RETURN;
}

// Recovery threshold update method

static void
update_recovery_completion_policy(MYSQL_THD thd, SYS_VAR *var,
                                  void *var_ptr, const void *save)
{
  DBUG_ENTER("update_recovery_completion_policy");

  ulong in_val= *static_cast<const ulong*>(save);
  (*(ulong*) var_ptr)= (*(ulong*) save);

  if (recovery_module != NULL)
  {
    recovery_module->
        set_recovery_completion_policy(
            (enum_recovery_completion_policies)in_val);
  }

  DBUG_VOID_RETURN;
}

//Component timeout update method

static void update_component_timeout(MYSQL_THD thd, SYS_VAR *var,
                                     void *var_ptr, const void *save)
{
  DBUG_ENTER("update_component_timeout");

  ulong in_val= *static_cast<const ulong*>(save);
  (*(ulong*) var_ptr)= (*(ulong*) save);

  if (applier_module != NULL)
  {
    applier_module->set_stop_wait_timeout(in_val);
  }
  if (recovery_module != NULL)
  {
    recovery_module->set_stop_wait_timeout(in_val);
  }
  if (events_handler != NULL)
  {
    events_handler->set_stop_wait_timeout(in_val);
  }

  DBUG_VOID_RETURN;
}

static int check_auto_increment_increment(MYSQL_THD thd, SYS_VAR *var,
                                          void* save,
                                          struct st_mysql_value *value)
{
  DBUG_ENTER("check_auto_increment_increment");

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The group auto_increment_increment cannot be changed"
               " when Group Replication is running",
               MYF(0));
    DBUG_RETURN(1);
  }

  if (in_val > MAX_AUTO_INCREMENT_INCREMENT ||
      in_val < MIN_AUTO_INCREMENT_INCREMENT)
  {
    std::stringstream ss;
    ss << "The value " << in_val << " is not within the range of "
          "accepted values for the option "
          "group_replication_auto_increment_increment. The value "
          "must be between " << MIN_AUTO_INCREMENT_INCREMENT <<
          " and " << MAX_AUTO_INCREMENT_INCREMENT << " inclusive.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    DBUG_RETURN(1);
  }

  *(longlong*)save= in_val;
  DBUG_RETURN(0);
}

//Communication layer options.

static int check_ip_whitelist_preconditions(MYSQL_THD thd, SYS_VAR *var,
                                            void *save,
                                            struct st_mysql_value *value)
{
  DBUG_ENTER("check_ip_whitelist_preconditions");

  char buff[IP_WHITELIST_STR_BUFFER_LENGTH];
  const char *str;
  int length= sizeof(buff);

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The IP whitelist cannot be set while Group Replication "
               "is running", MYF(0));
    DBUG_RETURN(1);
  }

  (*(const char **) save)= NULL;

  if ((str= value->val_str(value, buff, &length)))
    str= thd->strmake(str, length);
  else // NULL value is not allowed
    DBUG_RETURN(1); /* purecov: inspected */

  // remove trailing whitespaces
  std::string v(str);
  v.erase(std::remove(v.begin(), v.end(), ' '), v.end());
  std::transform(v.begin(), v.end(), v.begin(), ::tolower);
  if (v.find("automatic") != std::string::npos && v.size() != 9)
  {
    my_message(ER_GROUP_REPLICATION_CONFIGURATION,
               "The IP whitelist is invalid. Make sure that AUTOMATIC "
               "when specifying \"AUTOMATIC\" the list contains no "
               "other values.", MYF(0));
    DBUG_RETURN(1);
  }

  *(const char**)save= str;

  DBUG_RETURN(0);
}

static int check_compression_threshold(MYSQL_THD thd, SYS_VAR *var,
                                       void* save,
                                       struct st_mysql_value *value)
{
  DBUG_ENTER("check_compression_threshold");

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The compression threshold cannot be set while "
               "Group Replication is running",
               MYF(0));
    DBUG_RETURN(1);
  }

  if (in_val > MAX_COMPRESSION_THRESHOLD || in_val < 0)
  {
    std::stringstream ss;
    ss << "The value " << in_val << " is not within the range of "
      "accepted values for the option compression_threshold!";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    DBUG_RETURN(1);
  }

  *(longlong*)save= in_val;

  DBUG_RETURN(0);
}

static int check_force_members(MYSQL_THD thd, SYS_VAR *var,
                               void* save,
                               struct st_mysql_value *value)
{
  DBUG_ENTER("check_force_members");
  int error= 0;
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str= NULL;
  (*(const char **) save)= NULL;
  int length= 0;

  // Only one set force_members can run at a time.
  mysql_mutex_lock(&force_members_running_mutex);
  if (force_members_running)
  {
    log_message(MY_ERROR_LEVEL,
                "There is one group_replication_force_members "
                "operation already ongoing");
    mysql_mutex_unlock(&force_members_running_mutex);
    DBUG_RETURN(1);
  }
  force_members_running= true;
  mysql_mutex_unlock(&force_members_running_mutex);

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("group_replication_wait_on_check_force_members",
                  {
                    const char act[]= "now wait_for waiting";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });
#endif

  // String validations.
  length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= thd->strmake(str, length);
  else
  {
    error= 1; /* purecov: inspected */
    goto end; /* purecov: inspected */
  }

  // If option value is empty string, just update its value.
  if (length == 0)
    goto update_value;

  // if group replication isn't running and majority is reachable you can't
  // update force_members
  if (!plugin_is_group_replication_running() ||
      !group_member_mgr->is_majority_unreachable())
  {
    log_message(MY_ERROR_LEVEL,
                "group_replication_force_members can only be updated"
                " when Group Replication is running and a majority of the"
                " members are unreachable");
    error= 1;
    goto end;
  }

  if ((error= gcs_module->force_members(str)))
    goto end;

update_value:
  *(const char**)save= str;

end:
  mysql_mutex_lock(&force_members_running_mutex);
  force_members_running= false;
  mysql_mutex_unlock(&force_members_running_mutex);

  DBUG_RETURN(error);
}

static int check_gtid_assignment_block_size(MYSQL_THD thd, SYS_VAR *var,
                                            void* save,
                                            struct st_mysql_value *value)
{
  DBUG_ENTER("check_gtid_assignment_block_size");

  longlong in_val;
  value->val_int(value, &in_val);

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "The GTID assignment block size cannot be set while "
               "Group Replication is running", MYF(0));
    DBUG_RETURN(1);
  }

  if (in_val > MAX_GTID_ASSIGNMENT_BLOCK_SIZE ||
      in_val < MIN_GTID_ASSIGNMENT_BLOCK_SIZE)
  {
    std::stringstream ss;
    ss << "The value " << in_val << " is not within the range of "
          "accepted values for the option gtid_assignment_block_size. "
          "The value must be between " << MIN_GTID_ASSIGNMENT_BLOCK_SIZE <<
          " and " << MAX_GTID_ASSIGNMENT_BLOCK_SIZE << " inclusive.";
    my_message(ER_WRONG_VALUE_FOR_VAR, ss.str().c_str(), MYF(0));
    DBUG_RETURN(1);
  }

  *(longlong*)save= in_val;

  DBUG_RETURN(0);
}

static bool
get_bool_value_using_type_lib(struct st_mysql_value *value,
                              my_bool &resulting_value)
{
  DBUG_ENTER("get_bool_value_using_type_lib");
  longlong value_to_check;

  if (MYSQL_VALUE_TYPE_STRING == value->value_type(value))
  {
    const unsigned int flags = 0;

    char text_buffer[10] = { 0 };
    int  text_buffer_size = sizeof(text_buffer);
    const char *text_value = value->val_str(value,text_buffer, &text_buffer_size);

    if (NULL == text_value)
      DBUG_RETURN(false);

    // Return index inside bool_type_allowed_values array
    // (first element start with index 1)
    value_to_check = find_type(text_value, &plugin_bool_typelib, flags);

    if (0 == value_to_check)
    {
      DBUG_RETURN(false);
    }

    // Move the index value to 0,1 values (OFF, ON)
    --value_to_check;
  }
  else
  {
    // Do implicit conversion to int
    value->val_int(value, &value_to_check);
  }

  resulting_value = value_to_check > 0 ? TRUE : FALSE;

  DBUG_RETURN(true);
}

static int
check_single_primary_mode(MYSQL_THD thd, SYS_VAR *var,
                          void* save,
                          struct st_mysql_value *value)
{
  DBUG_ENTER("check_single_primary_mode");
  my_bool single_primary_mode_val;

  if (!get_bool_value_using_type_lib(value, single_primary_mode_val))
    DBUG_RETURN(1);

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "Cannot change into or from single primary mode while "
               "Group Replication is running.", MYF(0));
    DBUG_RETURN(1);
  }

  if (single_primary_mode_val && enforce_update_everywhere_checks_var)
  {
    my_message(ER_WRONG_VALUE_FOR_VAR,
               "Cannot turn ON single_primary_mode while "
               "enforce_update_everywhere_checks is enabled.",
               MYF(0));
    DBUG_RETURN(1);
  }

  *(my_bool *)save = single_primary_mode_val;

  DBUG_RETURN(0);
}

static int
check_enforce_update_everywhere_checks(MYSQL_THD thd, SYS_VAR *var,
                                       void* save,
                                       struct st_mysql_value *value)
{
  DBUG_ENTER("check_enforce_update_everywhere_checks");
  my_bool enforce_update_everywhere_checks_val;

  if (!get_bool_value_using_type_lib(value, enforce_update_everywhere_checks_val))
    DBUG_RETURN(1);

  if (plugin_is_group_replication_running())
  {
    my_message(ER_GROUP_REPLICATION_RUNNING,
               "Cannot turn ON/OFF update everywhere checks mode while "
               "Group Replication is running.", MYF(0));
    DBUG_RETURN(1);
  }

  if (single_primary_mode_var && enforce_update_everywhere_checks_val)
  {
    my_message(ER_WRONG_VALUE_FOR_VAR,
               "Cannot enable enforce_update_everywhere_checks while "
               "single_primary_mode is enabled.",
               MYF(0));
    DBUG_RETURN(1);
  }

  *(my_bool *)save = enforce_update_everywhere_checks_val;

  DBUG_RETURN(0);
}

static void
update_allow_local_disjoint_gtids_join(MYSQL_THD thd, SYS_VAR *var,
                                       void *var_ptr, const void *save)
{
  DBUG_ENTER("update_allow_local_disjoint_gtids_join");

  (*(my_bool *) var_ptr)= (*(my_bool *) save);

  option_deprecation_warning(thd,
                             "group_replication_allow_local_disjoint_gtids_join");

  DBUG_VOID_RETURN;
}

static void update_unreachable_timeout(MYSQL_THD thd, SYS_VAR *var,
                                       void *var_ptr, const void *save)
{
  DBUG_ENTER("update_unreachable_timeout");

  ulong in_val= *static_cast<const ulong*>(save);
  (*(ulong*) var_ptr)= (*(ulong*) save);

  if (group_partition_handler != NULL)
  {
    group_partition_handler->update_timeout_on_unreachable(in_val);
  }

  DBUG_VOID_RETURN;
}

static void
update_member_weight(MYSQL_THD, SYS_VAR*,
                     void *var_ptr, const void *save)
{
  DBUG_ENTER("update_member_weight");

  (*(uint*) var_ptr)= (*(uint*) save);
  uint in_val= *static_cast<const uint*>(save);

  if (local_member_info != NULL)
  {
    local_member_info->set_member_weight(in_val);
  }

  DBUG_VOID_RETURN;
}

//Base plugin variables

static MYSQL_SYSVAR_STR(
  group_name,                                 /* name */
  group_name_var,                             /* var */
  /* optional var | malloc string | no set default */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_NODEFAULT,
  "The group name",
  check_group_name,                           /* check func*/
  NULL,                                       /* update func*/
  NULL);                                      /* default*/

static MYSQL_SYSVAR_BOOL(
  start_on_boot,                              /* name */
  start_group_replication_at_boot_var,        /* var */
  PLUGIN_VAR_OPCMDARG,                        /* optional var */
  "Whether the server should start Group Replication or not during bootstrap.",
  NULL,                                       /* check func*/
  NULL,                                       /* update func*/
  1);                                         /* default*/

//GCS module variables

static MYSQL_SYSVAR_STR(
  local_address,                              /* name */
  local_address_var,                          /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,  /* optional var | malloc string*/
  "The local address, i.e., host:port.",
  NULL,                                       /* check func*/
  NULL,                                       /* update func*/
  "");                                        /* default*/

static MYSQL_SYSVAR_STR(
  group_seeds,                                /* name */
  group_seeds_var,                            /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,  /* optional var | malloc string*/
  "The list of group seeds, comma separated. E.g., host1:port1,host2:port2.",
  NULL,                                       /* check func*/
  NULL,                                       /* update func*/
  "");                                        /* default*/

static MYSQL_SYSVAR_STR(
  force_members,                              /* name */
  force_members_var,                          /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,  /* optional var | malloc string*/
  "The list of members, comma separated. E.g., host1:port1,host2:port2. "
  "This option is used to force a new group membership, on which the excluded "
  "members will not receive a new view and will be blocked. The DBA will need "
  "to kill the excluded servers.",
  check_force_members,                        /* check func*/
  NULL,                                       /* update func*/
  "");                                        /* default*/

static MYSQL_SYSVAR_BOOL(
  bootstrap_group,                            /* name */
  bootstrap_group_var,                        /* var */
  PLUGIN_VAR_OPCMDARG,                        /* optional var */
  "Specify if this member will bootstrap the group.",
  NULL,                                       /* check func. */
  NULL,                                       /* update func*/
  0                                           /* default */
);

static MYSQL_SYSVAR_ULONG(
  poll_spin_loops,                            /* name */
  poll_spin_loops_var,                        /* var */
  PLUGIN_VAR_OPCMDARG,                        /* optional var */
  "The number of times a thread waits for a communication engine "
  "mutex to be freed before the thread is suspended.",
  NULL,                                       /* check func. */
  NULL,                                       /* update func. */
  0,                                          /* default */
  0,                                          /* min */
  ~0UL,                                       /* max */
  0                                           /* block */
);

//Recovery module variables

static MYSQL_SYSVAR_ULONG(
  recovery_retry_count,              /* name */
  recovery_retry_count_var,          /* var */
  PLUGIN_VAR_OPCMDARG,               /* optional var */
  "The number of times that the joiner tries to connect to the available donors before giving up.",
  NULL,                              /* check func. */
  update_recovery_retry_count,       /* update func. */
  10,                                /* default */
  0,                                 /* min */
  LONG_TIMEOUT,                      /* max */
  0                                  /* block */
);

static MYSQL_SYSVAR_ULONG(
  recovery_reconnect_interval,        /* name */
  recovery_reconnect_interval_var,    /* var */
  PLUGIN_VAR_OPCMDARG,                /* optional var */
  "The sleep time between reconnection attempts when no donor was found in the group",
  NULL,                               /* check func. */
  update_recovery_reconnect_interval, /* update func. */
  60,                                 /* default */
  0,                                  /* min */
  LONG_TIMEOUT,                       /* max */
  0                                   /* block */
);

//SSL options for recovery

static MYSQL_SYSVAR_BOOL(
    recovery_use_ssl,              /* name */
    recovery_use_ssl_var,          /* var */
    PLUGIN_VAR_OPCMDARG,           /* optional var */
    "Whether SSL use should be obligatory during Group Replication recovery process.",
    NULL,                          /* check func*/
    update_ssl_use,                /* update func*/
    0);                            /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_ca,                 /* name */
    recovery_ssl_ca_var,             /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "The path to a file that contains a list of trusted SSL certificate authorities.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_capath,             /* name */
    recovery_ssl_capath_var,         /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "The path to a directory that contains trusted SSL certificate authority certificates.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_cert,               /* name */
    recovery_ssl_cert_var,           /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "The name of the SSL certificate file to use for establishing a secure connection.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_cipher,             /* name */
    recovery_ssl_cipher_var,         /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "A list of permissible ciphers to use for SSL encryption.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_key,                /* name */
    recovery_ssl_key_var,            /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "The name of the SSL key file to use for establishing a secure connection.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_crl,                /* name */
    recovery_ssl_crl_var,            /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "The path to a file containing certificate revocation lists.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_STR(
    recovery_ssl_crlpath,            /* name */
    recovery_ssl_crlpath_var,        /* var */
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC, /* optional var | malloc string*/
    "The path to a directory that contains files containing certificate revocation lists.",
    check_recovery_ssl_option,       /* check func*/
    update_recovery_ssl_option,      /* update func*/
    "");                             /* default*/

static MYSQL_SYSVAR_BOOL(
    recovery_ssl_verify_server_cert,        /* name */
    recovery_ssl_verify_server_cert_var,    /* var */
    PLUGIN_VAR_OPCMDARG,                    /* optional var */
    "Make recovery check the server's Common Name value in the donor sent certificate.",
    NULL,                                   /* check func*/
    update_ssl_server_cert_verification,    /* update func*/
    0);                                     /* default*/

/** Initialize the ssl option map with variable names*/
static void initialize_ssl_option_map()
{
  recovery_ssl_opt_map.clear();
  st_mysql_sys_var* ssl_ca_var= MYSQL_SYSVAR(recovery_ssl_ca);
  recovery_ssl_opt_map[ssl_ca_var->name]= RECOVERY_SSL_CA_OPT;
  st_mysql_sys_var* ssl_capath_var= MYSQL_SYSVAR(recovery_ssl_capath);
  recovery_ssl_opt_map[ssl_capath_var->name]= RECOVERY_SSL_CAPATH_OPT;
  st_mysql_sys_var* ssl_cert_var= MYSQL_SYSVAR(recovery_ssl_cert);
  recovery_ssl_opt_map[ssl_cert_var->name]= RECOVERY_SSL_CERT_OPT;
  st_mysql_sys_var* ssl_cipher_var= MYSQL_SYSVAR(recovery_ssl_cipher);
  recovery_ssl_opt_map[ssl_cipher_var->name]= RECOVERY_SSL_CIPHER_OPT;
  st_mysql_sys_var* ssl_key_var= MYSQL_SYSVAR(recovery_ssl_key);
  recovery_ssl_opt_map[ssl_key_var->name]= RECOVERY_SSL_KEY_OPT;
  st_mysql_sys_var* ssl_crl_var=MYSQL_SYSVAR(recovery_ssl_crl);
  recovery_ssl_opt_map[ssl_crl_var->name]= RECOVERY_SSL_CRL_OPT;
  st_mysql_sys_var* ssl_crlpath_var=MYSQL_SYSVAR(recovery_ssl_crlpath);
  recovery_ssl_opt_map[ssl_crlpath_var->name]= RECOVERY_SSL_CRLPATH_OPT;
}

// Recovery threshold options

const char* recovery_policies[]= { "TRANSACTIONS_CERTIFIED",
                                   "TRANSACTIONS_APPLIED",
                                   (char *)0};

TYPELIB recovery_policies_typelib_t= {
  array_elements(recovery_policies) - 1,
  "recovery_policies_typelib_t",
  recovery_policies,
  NULL
};

static MYSQL_SYSVAR_ENUM(
   recovery_complete_at,                                 /* name */
   recovery_completion_policy_var,                       /* var */
   PLUGIN_VAR_OPCMDARG,                                  /* optional var */
   "Recovery policies when handling cached transactions after state transfer."
   "possible values are TRANSACTIONS_CERTIFIED or TRANSACTION_APPLIED", /* values */
   NULL,                                                 /* check func. */
   update_recovery_completion_policy,                    /* update func. */
   RECOVERY_POLICY_WAIT_EXECUTED,                        /* default */
   &recovery_policies_typelib_t);                        /* type lib */

//Generic timeout setting

static MYSQL_SYSVAR_ULONG(
  components_stop_timeout,                         /* name */
  components_stop_timeout_var,                     /* var */
  PLUGIN_VAR_OPCMDARG,                             /* optional var */
  "Timeout in seconds that the plugin waits for each of the components when shutting down.",
  NULL,                                            /* check func. */
  update_component_timeout,                        /* update func. */
  LONG_TIMEOUT,                                    /* default */
  2,                                               /* min */
  LONG_TIMEOUT,                                    /* max */
  0                                                /* block */
);

//Allow member downgrade

static MYSQL_SYSVAR_BOOL(
  allow_local_lower_version_join,        /* name */
  allow_local_lower_version_join_var,    /* var */
  PLUGIN_VAR_OPCMDARG,                   /* optional var */
  "Allow this server to join the group even if it has a lower plugin version than the group",
  NULL,                                  /* check func. */
  NULL,                                  /* update func*/
  0                                      /* default */
);

static MYSQL_SYSVAR_BOOL(
  allow_local_disjoint_gtids_join,       /* name */
  allow_local_disjoint_gtids_join_var,   /* var */
  PLUGIN_VAR_OPCMDARG,                   /* optional var */
  "Allow this server to join the group even if it has transactions not present in the group",
  NULL,                                  /* check func. */
  update_allow_local_disjoint_gtids_join,/* update func*/
  0                                      /* default */
);

static MYSQL_SYSVAR_ULONG(
  auto_increment_increment,          /* name */
  auto_increment_increment_var,      /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT, /* optional var | no set default */
  "The group replication auto_increment_increment determines interval between successive column values",
  check_auto_increment_increment,    /* check func. */
  NULL,                              /* update by update_func_long func. */
  DEFAULT_AUTO_INCREMENT_INCREMENT,  /* default */
  MIN_AUTO_INCREMENT_INCREMENT,      /* min */
  MAX_AUTO_INCREMENT_INCREMENT,      /* max */
  0                                  /* block */
);

static MYSQL_SYSVAR_ULONG(
  compression_threshold,             /* name */
  compression_threshold_var,         /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT, /* optional var | no set default */
  "The value in bytes above which (lz4) compression is "
  "enforced. When set to zero, deactivates compression. "
  "Default: 1000000.",
  check_compression_threshold,       /* check func. */
  NULL,                              /* update func. */
  DEFAULT_COMPRESSION_THRESHOLD,     /* default */
  MIN_COMPRESSION_THRESHOLD,         /* min */
  MAX_COMPRESSION_THRESHOLD,         /* max */
  0                                  /* block */
);

static MYSQL_SYSVAR_ULONGLONG(
  gtid_assignment_block_size,        /* name */
  gtid_assignment_block_size_var,    /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT, /* optional var | no set default */
  "The number of consecutive GTIDs that are reserved to each "
  "member. Each member will consume its blocks and reserve "
  "more when needed. Default: 1000000.",
  check_gtid_assignment_block_size,  /* check func. */
  NULL,                              /* update func. */
  DEFAULT_GTID_ASSIGNMENT_BLOCK_SIZE,/* default */
  MIN_GTID_ASSIGNMENT_BLOCK_SIZE,    /* min */
  MAX_GTID_ASSIGNMENT_BLOCK_SIZE,    /* max */
  0                                  /* block */
);

TYPELIB ssl_mode_values_typelib_t= {
  array_elements(ssl_mode_values) - 1,
  "ssl_mode_values_typelib_t",
  ssl_mode_values,
  NULL
};

static MYSQL_SYSVAR_ENUM(
  ssl_mode,                          /* name */
  ssl_mode_var,                      /* var */
  PLUGIN_VAR_OPCMDARG,               /* optional var */
  "Specifies the security state of the connection between Group "
  "Replication members. Default: DISABLED",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0,                                 /* default */
  &ssl_mode_values_typelib_t         /* type lib */
);

static MYSQL_SYSVAR_STR(
  ip_whitelist,                             /* name */
  ip_whitelist_var,                         /* var */
  /* optional var | malloc string | no set default */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_NODEFAULT,
  "This option can be used to specify which members "
  "are allowed to connect to this member. The input "
  "takes the form of a comma separated list of IPv4 "
  "addresses or subnet CIDR notation. For example: "
  "192.168.1.0/24,10.0.0.1. In addition, the user can "
  "also set as input the value 'AUTOMATIC', in which case "
  "active interfaces on the host will be scanned and "
  "those with addresses on private subnetworks will be "
  "automatically added to the IP whitelist. The address "
  "127.0.0.1 is always added if not specified explicitly "
  "in the whitelist. Default: 'AUTOMATIC'.",
  check_ip_whitelist_preconditions,           /* check func*/
  NULL,                                       /* update func*/
  IP_WHITELIST_DEFAULT);                      /* default*/

static MYSQL_SYSVAR_BOOL(
  single_primary_mode,                        /* name */
  single_primary_mode_var,                    /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT, /* optional var | no set default */
  "Instructs the group to automatically pick a single server to be "
  "the one that handles read/write workload. This server is the "
  "PRIMARY all others are SECONDARIES. Default: TRUE.",
  check_single_primary_mode,                  /* check func*/
  NULL,                                       /* update func*/
  TRUE);                                      /* default*/

static MYSQL_SYSVAR_BOOL(
  enforce_update_everywhere_checks,           /* name */
  enforce_update_everywhere_checks_var,       /* var */
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NODEFAULT, /* optional var | no set default */
  "Enable/Disable strict consistency checks for multi-master "
  "update everywhere. Default: FALSE.",
  check_enforce_update_everywhere_checks,     /* check func*/
  NULL,                                       /* update func*/
  FALSE);                                     /* default*/

const char* flow_control_mode_values[]= {
  "DISABLED",
  "QUOTA",
  (const char*)0
};

TYPELIB flow_control_mode_typelib_t= {
  array_elements(flow_control_mode_values) - 1,
  "flow_control_mode_typelib_t",
  flow_control_mode_values,
  NULL
};

static MYSQL_SYSVAR_ENUM(
  flow_control_mode,                 /* name */
  flow_control_mode_var,             /* var */
  PLUGIN_VAR_OPCMDARG,               /* optional var */
  "Specifies the mode used on flow control. "
  "Default: QUOTA",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  FCM_QUOTA,                         /* default */
  &flow_control_mode_typelib_t       /* type lib */
);

static MYSQL_SYSVAR_INT(
  flow_control_certifier_threshold,     /* name */
  flow_control_certifier_threshold_var, /* var */
  PLUGIN_VAR_OPCMDARG,                  /* optional var */
  "Specifies the number of waiting transactions that will trigger "
  "flow control. Default: 25000",
  NULL,                                 /* check func. */
  NULL,                                 /* update func. */
  DEFAULT_FLOW_CONTROL_THRESHOLD,       /* default */
  MIN_FLOW_CONTROL_THRESHOLD,           /* min */
  MAX_FLOW_CONTROL_THRESHOLD,           /* max */
  0                                     /* block */
);

static MYSQL_SYSVAR_INT(
  flow_control_applier_threshold,      /* name */
  flow_control_applier_threshold_var,  /* var */
  PLUGIN_VAR_OPCMDARG,                 /* optional var */
  "Specifies the number of waiting transactions that will trigger "
  "flow control. Default: 25000",
  NULL,                                /* check func. */
  NULL,                                /* update func. */
  DEFAULT_FLOW_CONTROL_THRESHOLD,      /* default */
  MIN_FLOW_CONTROL_THRESHOLD,          /* min */
  MAX_FLOW_CONTROL_THRESHOLD,          /* max */
  0                                    /* block */
);

static MYSQL_SYSVAR_ULONG(
  transaction_size_limit,              /* name */
  transaction_size_limit_var,          /* var */
  PLUGIN_VAR_OPCMDARG,                 /* optional var */
  "Specifies the limit of transaction size that can be transferred over network.",
  NULL,                                /* check func. */
  NULL,                                /* update func. */
  DEFAULT_TRANSACTION_SIZE_LIMIT,      /* default */
  MIN_TRANSACTION_SIZE_LIMIT,          /* min */
  MAX_TRANSACTION_SIZE_LIMIT,          /* max */
  0                                    /* block */
);

static MYSQL_SYSVAR_ULONG(
  unreachable_majority_timeout,                    /* name */
  timeout_on_unreachable_var,                      /* var */
  PLUGIN_VAR_OPCMDARG,                             /* optional var */
  "The number of seconds before going into error when a majority of members is unreachable."
  "If 0 there is no action taken.",
  NULL,                                            /* check func. */
  update_unreachable_timeout,                      /* update func. */
  0,                                               /* default */
  0,                                               /* min */
  LONG_TIMEOUT,                                    /* max */
  0                                                /* block */
);

static MYSQL_SYSVAR_UINT(
  member_weight,                       /* name */
  member_weight_var,                   /* var */
  PLUGIN_VAR_OPCMDARG,                 /* optional var */
  "Member weight will determine the member role in the group on"
  " future primary elections",
  NULL,                                /* check func. */
  update_member_weight,                /* update func. */
  DEFAULT_MEMBER_WEIGHT,               /* default */
  MIN_MEMBER_WEIGHT,                   /* min */
  MAX_MEMBER_WEIGHT,                   /* max */
  0                                    /* block */
);

const char *exit_state_actions[]= {"READ_ONLY", "ABORT_SERVER", (char *)0};
TYPELIB exit_state_actions_typelib_t= {array_elements(exit_state_actions) - 1,
                                       "exit_state_actions_typelib_t",
                                       exit_state_actions, NULL};
static MYSQL_SYSVAR_ENUM(exit_state_action,     /* name */
                         exit_state_action_var, /* var */
                         PLUGIN_VAR_OPCMDARG,   /* optional var */
                         "The action that is taken when the server "
                         "leaves the group. "
                         "Possible values are READ_ONLY or "
                         "ABORT_SERVER.",                /* values */
                         NULL,                           /* check func. */
                         NULL,                           /* update func. */
                         EXIT_STATE_ACTION_READ_ONLY,    /* default */
                         &exit_state_actions_typelib_t); /* type lib */

static SYS_VAR* group_replication_system_vars[]= {
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
  MYSQL_SYSVAR(components_stop_timeout),
  MYSQL_SYSVAR(allow_local_lower_version_join),
  MYSQL_SYSVAR(allow_local_disjoint_gtids_join),
  MYSQL_SYSVAR(auto_increment_increment),
  MYSQL_SYSVAR(compression_threshold),
  MYSQL_SYSVAR(gtid_assignment_block_size),
  MYSQL_SYSVAR(ssl_mode),
  MYSQL_SYSVAR(ip_whitelist),
  MYSQL_SYSVAR(single_primary_mode),
  MYSQL_SYSVAR(enforce_update_everywhere_checks),
  MYSQL_SYSVAR(flow_control_mode),
  MYSQL_SYSVAR(flow_control_certifier_threshold),
  MYSQL_SYSVAR(flow_control_applier_threshold),
  MYSQL_SYSVAR(transaction_size_limit),
  MYSQL_SYSVAR(unreachable_majority_timeout),
  MYSQL_SYSVAR(member_weight),
  MYSQL_SYSVAR(exit_state_action),
  NULL,
};


static int
show_primary_member(MYSQL_THD thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_CHAR;
  var->value= NULL;

  if (group_member_mgr && single_primary_mode_var &&
      plugin_is_group_replication_running())
  {
    string primary_member_uuid;
    group_member_mgr->get_primary_member_uuid(primary_member_uuid);

    strncpy(buff, primary_member_uuid.c_str(), SHOW_VAR_FUNC_BUFF_SIZE);
    buff[SHOW_VAR_FUNC_BUFF_SIZE - 1] = 0;

    var->value= buff;
  }

  return 0;
}

static SHOW_VAR group_replication_status_vars[]=
{
  {"group_replication_primary_member",
   (char*) &show_primary_member,
   SHOW_FUNC, SHOW_SCOPE_GLOBAL},
  {NULL, NULL, SHOW_LONG, SHOW_SCOPE_GLOBAL},
};


mysql_declare_plugin(group_replication_plugin)
{
  MYSQL_GROUP_REPLICATION_PLUGIN,
  &group_replication_descriptor,
  group_replication_plugin_name,
  "ORACLE",
  "Group Replication (1.0.0)",      /* Plugin name with full version*/
  PLUGIN_LICENSE_GPL,
  plugin_group_replication_init,    /* Plugin Init */
  plugin_group_replication_deinit,  /* Plugin Deinit */
  0x0100,                           /* Plugin Version: major.minor */
  group_replication_status_vars,    /* status variables */
  group_replication_system_vars,    /* system variables */
  NULL,                             /* config options */
  0,                                /* flags */
}
mysql_declare_plugin_end;
