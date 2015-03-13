/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <pthread.h>
#include "observer_server_actions.h"
#include "observer_server_state.h"
#include "observer_trans.h"
#include "gcs_binding_factory.h"
#include "gcs_corosync_control_interface.h"
#include "plugin.h"
#include "plugin_log.h"

#define VIEW_MODIFICATION_TIMEOUT 10

using std::string;

/* Plugin generic fields */

static MYSQL_PLUGIN plugin_info_ptr;

//The plugin running flag and lock
static pthread_mutex_t plugin_running_mutex;
static bool group_replication_running;
bool wait_on_engine_initialization= false;

/* Plugin modules */
//The plugin applier
Applier_module *applier_module= NULL;
//The plugin recovery module
Recovery_module *recovery_module= NULL;
//The plugin group communication module
Gcs_interface *gcs_module= NULL;

/* Group communication options */
ulong gcs_protocol_opt;
const char *available_bindings_names[]= {"COROSYNC", (char *)0};
TYPELIB gcs_protocol_typelib=
{ array_elements(available_bindings_names) - 1, "", available_bindings_names, NULL };

Plugin_gcs_events_handler* events_handler= NULL;
Plugin_gcs_view_modification_notifier* view_change_notifier= NULL;

int gcs_communication_event_handle= 0;
int gcs_control_event_handler= 0;
int gcs_control_exchanged_data_handle= 0;

/* Group management information */
Group_member_info_manager_interface *group_member_mgr= NULL;
Group_member_info* local_member_info= NULL;

/* Plugin group related options */
const char *group_replication_plugin_name= "group_replication";
char group_name[UUID_LENGTH+1];
char *group_name_pointer= NULL;
char start_group_replication_at_boot;
rpl_sidno group_sidno;

/* Applier module related */
ulong handler_pipeline_type;
bool known_server_reset;

/* Recovery module related */
char recovery_user[USERNAME_LENGTH + 1];
char *recovery_user_pointer= NULL;
/**
  Dummy variable associated to the recovery password sysvar making it never
  accessible.
  The real value resides in the below field
*/
char *dummy_recovery_password= NULL;
//Invisible. After Recovery consumes it will be nullified
char recovery_password[MAX_PASSWORD_LENGTH + 1];
ulong recovery_retry_count= 0;

/* Generic components variables */
ulong components_stop_timeout= LONG_TIMEOUT;

/* Certification latch */
Wait_ticket<my_thread_id> *certification_latch;

/*
  Internal auxiliary functions signatures.
*/
static int check_group_name_string(const char *str);

static int check_if_server_properly_configured();

static bool init_group_sidno();

static bool server_engine_initialized();

/*
  Auxiliary public functions.
*/
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
plugin_get_connection_status(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info)
{
  char* channel_name= applier_module_channel_name;

  return get_connection_status(info, gcs_module, group_name_pointer,
                               channel_name,
                               plugin_is_group_replication_running());
}

bool
plugin_get_group_members(uint index, GROUP_REPLICATION_GROUP_MEMBERS_INFO *info)
{
  char* channel_name= applier_module_channel_name;

  return get_group_members_info(index, info,group_member_mgr, gcs_module,
                                group_name_pointer, channel_name);
}

uint plugin_get_group_members_number()
{
  return group_member_mgr == NULL? 1 :
                                    (uint)group_member_mgr
                                                      ->get_number_of_members();
}

bool
plugin_get_group_member_stats(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO *info)
{
  char* channel_name= applier_module_channel_name;

  return get_group_member_stats(info, group_member_mgr, applier_module,
                                gcs_module, group_name_pointer, channel_name);
}

int plugin_group_replication_start()
{
  Mutex_autolock auto_lock_mutex(&plugin_running_mutex);

  DBUG_ENTER("plugin_group_replication_start");
  int error= 0;

  if (plugin_is_group_replication_running())
    DBUG_RETURN(GROUP_REPLICATION_ALREADY_RUNNING);
  if (check_if_server_properly_configured())
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  if (check_group_name_string(group_name_pointer))
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);
  if (init_group_sidno())
    DBUG_RETURN(GROUP_REPLICATION_CONFIGURATION_ERROR);

  /*
    Instantiate certification latch.
  */
  certification_latch= new Wait_ticket<my_thread_id>();

  if(gcs_module->initialize())
  {
    error= GROUP_REPLICATION_CONFIGURATION_ERROR;
    goto err;
  }

  if (server_engine_initialized())
  {
    configure_group_member_manager();

    initialize_recovery_module();

    //we can only start the applier if the log has been initialized
    if (configure_and_start_applier_module())
    {
      error= GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR;
      goto err;
    }
  }
  else
  {
    wait_on_engine_initialization= true;
    DBUG_RETURN(0); //leave the decision for later
  }

  if ((error= configure_and_start_group_communication()))
  {
    //terminate the before created pipeline
    log_message(MY_ERROR_LEVEL,
                "Error on group communication initialization methods, "
                "killing the Group Replication applier");
    applier_module->terminate_applier_thread();
    goto err;
  }

  if(!(view_change_notifier
           ->wait_for_view_modification(VIEW_MODIFICATION_TIMEOUT)))
  {
    group_replication_running= true;
  }
  else
  {
    error= 1;
    goto err;
  }

err:
  if (error && certification_latch != NULL)
  {
    delete certification_latch;
    certification_latch= NULL;
  }

  DBUG_RETURN(error); //All is OK
}

int configure_group_member_manager()
{
  //Retrieve local GCS information
  string group_id_str(group_name_pointer);
  Gcs_group_identifier group_id(group_id_str);
  Gcs_control_interface* gcs_ctrl= gcs_module->get_control_session(group_id);

  //Configure Group Member Manager
  char *hostname, *uuid;
  uint port;
  get_server_host_port_uuid(&hostname, &port, &uuid);

  if(local_member_info != NULL)
  {
    delete local_member_info;
  }

  local_member_info= new Group_member_info(hostname,
                                           port,
                                           uuid,
                                           gcs_ctrl->get_local_information(),
                                           Group_member_info::MEMBER_OFFLINE);

  //Create the membership info visible for the group
  if(group_member_mgr != NULL)
  {
    delete group_member_mgr;
  }

  group_member_mgr= new Group_member_info_manager(local_member_info);

  return 0;
}

int plugin_group_replication_stop()
{
  Mutex_autolock auto_lock_mutex(&plugin_running_mutex);
  DBUG_ENTER("plugin_group_replication_stop");

  if (!plugin_is_group_replication_running())
    DBUG_RETURN(0);

  /* first leave all joined groups (currently one) */
  string group_name(group_name_pointer);
  Gcs_group_identifier group_id(group_name);

  Gcs_control_interface *ctrl_if= gcs_module->get_control_session(group_id);
  Gcs_communication_interface *comm_if=
      gcs_module->get_communication_session(group_id);

  view_change_notifier->start_view_modification();

  if(ctrl_if->belongs_to_group())
  {
    if(ctrl_if->leave())
    {
      log_message(MY_WARNING_LEVEL,"Error leaving the group");
    }

    log_message(MY_INFORMATION_LEVEL, "going to wait for view modification");
    if(view_change_notifier
                        ->wait_for_view_modification(VIEW_MODIFICATION_TIMEOUT))
    {
      log_message(MY_WARNING_LEVEL,
                  "On shutdown there was a timeout receiving a view change."
                  "This can lead to a possible inconsistent state."
                  "Check the log for more details");
    }
  }

  //Unregister callbacks and destroy notifiers
  ctrl_if->remove_data_exchange_event_listener(gcs_control_event_handler);
  ctrl_if->remove_data_exchange_event_listener(gcs_control_exchanged_data_handle);
  comm_if->remove_event_listener(gcs_communication_event_handle);

  gcs_control_event_handler= 0;
  gcs_control_exchanged_data_handle= 0;
  gcs_communication_event_handle= 0;

  delete events_handler;
  delete view_change_notifier;

  if(terminate_recovery_module())
  {
    //Do not trow an error since recovery is not vital, but warn either way
    log_message(MY_WARNING_LEVEL,
                "On shutdown there was a timeout on the Group Replication "
                "recovery module termination. Check the log for more details");
  }

  /*
    The applier is only shutdown after the communication layer to avoid
    messages being delivered in the current view, but not applied
  */
  int error= 0;
  if((error= terminate_applier_module()))
    log_message(MY_ERROR_LEVEL,
                "On shutdown there was a timeout on the Group Replication"
                " applier termination.");

  /*
    Even if the applier did not terminate, let group_replication_running be
    false as he can shutdown in the meanwhile.
  */

  gcs_module->finalize();

  /*
    Destroy certification latch.
  */
  if (certification_latch != NULL)
  {
    delete certification_latch;
    certification_latch= NULL;
  }

  group_replication_running= false;
  DBUG_RETURN(error);
}

int plugin_group_replication_init(MYSQL_PLUGIN plugin_info)
{
  pthread_mutex_init(&plugin_running_mutex, NULL);
  plugin_info_ptr= plugin_info;
  if (group_replication_init(group_replication_plugin_name))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure on Group Replication handler initialization");
    return 1;
  }

  if(register_server_state_observer(&server_state_observer,
                                    (void *)plugin_info_ptr))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when registering the server state observers");
    return 1;
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when registering the transactions state observers");
    return 1;
  }

  if (register_binlog_transmit_observer(&binlog_transmit_observer,
                                        (void *)plugin_info_ptr))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when registering the binlog state observers");
    return 1;
  }

  if ((gcs_module=
         Gcs_binding_factory::get_gcs_implementation
                              ((plugin_gcs_bindings)gcs_protocol_opt)) == NULL)
  {
    log_message(MY_ERROR_LEVEL,
                "Failure in group communication protocol initialization");
    return 1;
  };

  if (start_group_replication_at_boot && group_replication_start())
    return 1;

  return 0;
}

int plugin_group_replication_deinit(void *p)
{
  if (group_replication_cleanup())
    return 1;

  Gcs_binding_factory::cleanup_gcs_implementation
                                       ((plugin_gcs_bindings)gcs_protocol_opt);

  if(group_member_mgr != NULL)
  {
    delete group_member_mgr;
    group_member_mgr= NULL;
  }

  if(local_member_info != NULL)
  {
    delete local_member_info;
    local_member_info= NULL;
  }

  if (unregister_server_state_observer(&server_state_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when unregistering the server state observers");
    return 1;
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when unregistering the transactions state observers");
    return 1;
  }

  if (unregister_binlog_transmit_observer(&binlog_transmit_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure when unregistering the binlog state observers");
    return 1;
  }

  log_message(MY_INFORMATION_LEVEL,
              "All Group Replication server observers"
              " have been successfully unregistered");

  pthread_mutex_destroy(&plugin_running_mutex);

  return 0;
}

static void update_boot(MYSQL_THD thd, SYS_VAR *var, void *ptr, const void *val)
{
  DBUG_ENTER("update_boot");

  *(char *)ptr= *(char *)val;
  start_group_replication_at_boot= *((char *) ptr);

  DBUG_VOID_RETURN;
}

static bool init_group_sidno()
{
  DBUG_ENTER("init_group_sidno");
  rpl_sid group_sid;

  if (group_sid.parse(group_name_pointer) != RETURN_STATUS_OK)
    DBUG_RETURN(true);

  group_sidno = get_sidno_from_global_sid_map(group_sid);
  if (group_sidno <= 0)
    DBUG_RETURN(true);

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
    applier_module->setup_applier_module((Handler_pipeline_type)handler_pipeline_type,
                                         known_server_reset,
                                         components_stop_timeout,
                                         group_sidno);
  if (error)
  {
    //Delete the possible existing pipeline
    applier_module->terminate_applier_pipeline();
    DBUG_RETURN(error);
  }

  known_server_reset= false;

  if ((error= applier_module->initialize_applier_thread()))
  {
    log_message(MY_ERROR_LEVEL,
                "Unable to initialize the Group Replication applier module.");
    //clean a possible existent pipeline
    applier_module->terminate_applier_pipeline();
    delete applier_module;
    applier_module= NULL;
  }
  else
    log_message(MY_INFORMATION_LEVEL,
                "Group Replication applier module successfully initialized!");

  DBUG_RETURN(error);
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

int configure_and_start_group_communication()
{
  //Create data to be exchanged here...
  string group_id_str(group_name_pointer);
  Gcs_group_identifier group_id(group_id_str);
  Gcs_control_interface* gcs_ctrl= gcs_module->get_control_session(group_id);

  gcs_ctrl->set_exchangeable_data(group_member_mgr->get_exchangeable_format());

  view_change_notifier= new Plugin_gcs_view_modification_notifier();
  events_handler= new Plugin_gcs_events_handler(applier_module,
                                                recovery_module,
                                                group_member_mgr,
                                                local_member_info,
                                                view_change_notifier);

  view_change_notifier->start_view_modification();

  gcs_control_event_handler= gcs_ctrl->add_event_listener(events_handler);
  gcs_control_exchanged_data_handle=
      gcs_ctrl->add_data_exchange_event_listener(events_handler);

  //Set interfaces for Certifier
  Gcs_communication_interface *comm_if=
      gcs_module->get_communication_session(group_id);

  gcs_communication_event_handle= comm_if->add_event_listener(events_handler);

  //Transmit the interfaces to the interested handlers.
  Handler_gcs_interfaces_action *interf_action=
    new Handler_gcs_interfaces_action(local_member_info, comm_if, gcs_ctrl);
  applier_module->handle_pipeline_action(interf_action);
  delete interf_action;

  if (gcs_ctrl->join())
  {
    return GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR;
  }

  return 0;
}

int initialize_recovery_module()
{
  string group_id_str(group_name_pointer);
  Gcs_group_identifier group_id(group_id_str);

  Gcs_communication_interface *comm_if=
      gcs_module->get_communication_session(group_id);
  Gcs_control_interface *ctrl_if=
      gcs_module->get_control_session(group_id);

  recovery_module = new Recovery_module(applier_module, comm_if,
                                        ctrl_if, local_member_info,
                                        group_member_mgr,
                                        components_stop_timeout);

  recovery_module->set_recovery_donor_connection_user(recovery_user_pointer);
  recovery_module->set_recovery_donor_connection_password(&recovery_password[0]);

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

static bool server_engine_initialized(){
  return is_server_engine_ready();
}

void register_server_reset_master(){
  known_server_reset= true;
}

/*
  This method is used to accomplish the startup validations of the plugin
  regarding system configuration.

  It currently verifies:
  - Binlog enabled
  - Binlog checksum mode
  - Binlog format
  - Gtid mode

  @return If the operation succeed or failed
    @retval 0 in case of success
    @retval 1 in case of failure
 */
static int check_if_server_properly_configured()
{
  DBUG_ENTER("check_if_server_properly_configured");

  //Struct that holds startup and runtime requirements
  Trans_context_info startup_pre_reqs;

  get_server_startup_prerequirements(startup_pre_reqs, true);

  if(!startup_pre_reqs.binlog_enabled)
  {
    log_message(MY_ERROR_LEVEL, "Binlog must be enabled for Group Replication");
    DBUG_RETURN(1);
  }

  if(startup_pre_reqs.binlog_checksum_options != binary_log::BINLOG_CHECKSUM_ALG_OFF)
  {
    log_message(MY_ERROR_LEVEL, "Binlog checksum should be OFF for Group Replication");
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

  if(startup_pre_reqs.transaction_write_set_extraction !=
     HASH_ALGORITHM_MURMUR32)
  {
    log_message(MY_ERROR_LEVEL,
                "Extraction of a transaction write set requires MURMUR32 hash "
                "configuration. Please, double check that the parameter "
                "transaction-write-set-extraction is set accordingly.");
    DBUG_RETURN(1);
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

  DBUG_RETURN(0);
}

static int check_group_name_string(const char *str)
{
  DBUG_ENTER("check_group_name_string");

  if (!str)
  {
    log_message(MY_ERROR_LEVEL, "The group name option is mandatory");
    DBUG_RETURN(1);
  }
  if (!Uuid::is_valid(str))
  {
    log_message(MY_ERROR_LEVEL, "The group name '%s' is not a valid UUID", str);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

static int check_group_name(MYSQL_THD thd, SYS_VAR *var, void* prt,
                            struct st_mysql_value *value)
{
  DBUG_ENTER("check_group_name");

  char buff[NAME_CHAR_LEN];
  const char *str;

  if (plugin_is_group_replication_running())
  {
    log_message(MY_ERROR_LEVEL,
                "The group name cannot be changed when Group Replication is running");
    DBUG_RETURN(1);
  }

  int length= sizeof(buff);
  str= value->val_str(value, buff, &length);
  if (check_group_name_string(str))
    DBUG_RETURN(1);

  *(const char**)prt= str;
  DBUG_RETURN(0);
}

static void update_group_name(MYSQL_THD thd, SYS_VAR *var, void *ptr, const
                              void *val)
{
  DBUG_ENTER("update_group_name");

  const char *newGroup= *(const char**)val;
  strncpy(group_name, newGroup, UUID_LENGTH);
  group_name_pointer= &group_name[0];

  DBUG_VOID_RETURN;
}

//Recovery module's module variable update/validate methods

static int check_recovery_con_user(MYSQL_THD thd, SYS_VAR *var, void* ptr,
                                   struct st_mysql_value *value)
{
  DBUG_ENTER("check_recovery_con_user");

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;

  int length= sizeof(buff);
  str= value->val_str(value, buff, &length);

  if(strlen(str) > USERNAME_LENGTH)
  {
    log_message(MY_ERROR_LEVEL,
                "The given user name for recovery donor connection is to big");
    DBUG_RETURN(1);
  }

  *(const char**)ptr= str;
  DBUG_RETURN(0);
}


static void update_recovery_con_user(MYSQL_THD thd, SYS_VAR *var, void *ptr,
                                     const void *value)
{
  DBUG_ENTER("update_recovery_con_user");

  const char *new_user= *(const char**)value;
  strncpy(recovery_user, new_user, strlen(new_user)+1);
  recovery_user_pointer= &recovery_user[0];
  if (recovery_module != NULL)
  {
    recovery_module->set_recovery_donor_connection_user(new_user);
  }
  DBUG_VOID_RETURN;
}

static int check_recovery_con_password(MYSQL_THD thd, SYS_VAR *var, void* ptr,
                                       struct st_mysql_value *value)
{
  DBUG_ENTER("check_recovery_con_password");

  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;

  int length= sizeof(buff);
  str= value->val_str(value, buff, &length);

  if(strlen(str) > MAX_PASSWORD_LENGTH)
  {
    log_message(MY_ERROR_LEVEL,
                "The given password for recovery donor connection is to big");
    DBUG_RETURN(1);
  }

  strncpy(recovery_password, str, strlen(str)+1);
  if (recovery_module != NULL)
  {
    recovery_module->set_recovery_donor_connection_password(str);
  }

  DBUG_RETURN(0);
}

static void update_recovery_con_password(MYSQL_THD thd, SYS_VAR *var, void *ptr,
                                         const void *value)
{
  DBUG_ENTER("update_recovery_con_password");
  DBUG_VOID_RETURN;
}

static void update_recovery_retry_count(MYSQL_THD thd, SYS_VAR *var, void *ptr,
                                        const void *value)
{
  DBUG_ENTER("update_recovery_retry_count");

  ulong in_val= *static_cast<const ulong*>(value);
  recovery_retry_count= in_val;

  if (recovery_module != NULL)
  {
    recovery_module->set_recovery_donor_retry_count(recovery_retry_count);
  }

  DBUG_VOID_RETURN;
}

//Component timeout update method

static void update_component_timeout(MYSQL_THD thd, SYS_VAR *var, void *ptr,
                                     const void *value)
{
  DBUG_ENTER("update_component_timeout");

  ulong in_val= *static_cast<const ulong*>(value);
  components_stop_timeout= in_val;

  if (applier_module != NULL)
  {
    applier_module->set_stop_wait_timeout(components_stop_timeout);
  }
  if (recovery_module != NULL)
  {
    recovery_module->set_stop_wait_timeout(components_stop_timeout);
  }

  DBUG_VOID_RETURN;
}

//Base plugin variables

static MYSQL_SYSVAR_BOOL(start_on_boot, start_group_replication_at_boot,
  PLUGIN_VAR_OPCMDARG,
  "Whether the server should start Group Replication or not during bootstrap.",
  NULL,
  update_boot,
  0);

static MYSQL_SYSVAR_STR(group_name, group_name_pointer,
  PLUGIN_VAR_OPCMDARG,
  "The group name",
  check_group_name,
  update_group_name,
  NULL);

static const char* pipeline_names[]= { "STANDARD", NullS};

static TYPELIB pipeline_name_typelib_t= {
         array_elements(pipeline_names) - 1,
         "pipeline_name_typelib_t",
         pipeline_names,
         NULL
 };

//Applier module variables

static MYSQL_SYSVAR_ENUM(pipeline_type_var, handler_pipeline_type,
   PLUGIN_VAR_OPCMDARG,
   "pipeline types"
   "possible values are STANDARD",
   NULL, NULL, STANDARD_GROUP_REPLICATION_PIPELINE, &pipeline_name_typelib_t);

//GCS module variables

static MYSQL_SYSVAR_ENUM(gcs_protocol, gcs_protocol_opt,
  PLUGIN_VAR_OPCMDARG,
  "The name of group communication protocol to use.",
  NULL,
  NULL,
  COROSYNC,
  &gcs_protocol_typelib);

//Recovery module variables

static MYSQL_SYSVAR_STR(
  recovery_user,                              /* name */
  recovery_user_pointer,                      /* var */
  PLUGIN_VAR_OPCMDARG,                        /* optional var */
  "The user name of the account that recovery uses for the donor connection",
  check_recovery_con_user,                                       /* check func*/
  update_recovery_con_user,                   /* update func*/
  "root");                                    /* default*/

static MYSQL_SYSVAR_STR(
  recovery_password,                          /* name */
  dummy_recovery_password,                    /* var */
  PLUGIN_VAR_OPCMDARG,                        /* optional var */
  "The password of the account that recovery uses for the donor connection",
  check_recovery_con_password,                /* check func*/
  update_recovery_con_password,               /* update func*/
  "");                                        /* default*/

static MYSQL_SYSVAR_ULONG(
  recovery_retry_count,              /* name */
  recovery_retry_count,              /* var */
  PLUGIN_VAR_OPCMDARG,               /* optional var */
  "The number of times that the joiner tries to connect to the donor before giving up.",
  NULL,                              /* check func. */
  update_recovery_retry_count,       /* update func. */
  0,                                 /* default */
  0,                                 /* min */
  LONG_TIMEOUT,                      /* max */
  0                                  /* block */
);

//Generic timeout setting

static MYSQL_SYSVAR_ULONG(
  components_stop_timeout,                         /* name */
  components_stop_timeout,       /* var */
  PLUGIN_VAR_OPCMDARG,                             /* optional var */
  "Timeout in seconds that the plugin waits for each of the components when shutting down.",
        NULL,                                      /* check func. */
  update_component_timeout,                        /* update func. */
  LONG_TIMEOUT,                                    /* default */
  2,                                               /* min */
  LONG_TIMEOUT,                                    /* max */
  0                                                /* block */
);

static SYS_VAR* group_replication_system_vars[]= {
  MYSQL_SYSVAR(group_name),
  MYSQL_SYSVAR(start_on_boot),
  MYSQL_SYSVAR(pipeline_type_var),
  MYSQL_SYSVAR(gcs_protocol),
  MYSQL_SYSVAR(recovery_user),
  MYSQL_SYSVAR(recovery_password),
  MYSQL_SYSVAR(recovery_retry_count),
  MYSQL_SYSVAR(components_stop_timeout),
  NULL,
};

mysql_declare_plugin(group_replication_plugin)
{
  MYSQL_GROUP_REPLICATION_PLUGIN,
  &group_replication_descriptor,
  group_replication_plugin_name,
  "ORACLE",
  "Group Replication",
  PLUGIN_LICENSE_GPL,
  plugin_group_replication_init,    /* Plugin Init */
  plugin_group_replication_deinit,  /* Plugin Deinit */
  0x0030,                           /* 0.3.0 Plugin version*/
  NULL,                             /* status variables */
  group_replication_system_vars,    /* system variables */
  NULL,                             /* config options */
  0,                                /* flags */
}
mysql_declare_plugin_end;
