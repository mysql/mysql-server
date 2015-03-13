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

#ifndef PLUGIN_INCLUDE
#define PLUGIN_INCLUDE

#include "applier.h"
#include "recovery.h"
#include "ps_information.h"

#include "gcs_interface.h"
#include "gcs_event_handlers.h"

#include "plugin_server_include.h"
#include <mysql/plugin.h>
#include <mysql/plugin_group_replication.h>


/*
  Plugin errors
  TODO: If this goes into the server side, replace this with server errors.
*/
#define GROUP_REPLICATION_CONFIGURATION_ERROR 1
#define GROUP_REPLICATION_ALREADY_RUNNING 2
#define GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR 3
#define GROUP_REPLICATION_COMMUNICATION_LAYER_JOIN_ERROR 4
#define GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR 5
#define GROUP_REPLICATION_APPLIER_STOP_TIMEOUT 6


//Plugin variables
typedef st_mysql_sys_var SYS_VAR;
extern const char *group_replication_plugin_name;
extern char group_name[UUID_LENGTH+1];
extern char *group_name_pointer;
extern rpl_sidno group_sidno;
extern char start_group_replication_at_boot;
extern bool wait_on_engine_initialization;
extern const char *available_bindings_names[];
//Flag to register server rest master command invocations
extern bool known_server_reset;
//Certification latch
extern Wait_ticket<my_thread_id> *certification_latch;

//The modules
extern Gcs_interface *gcs_module;
extern Applier_module *applier_module;
extern Recovery_module *recovery_module;
extern Group_member_info_manager_interface *group_member_mgr;

//Auxiliary Functionality
extern Plugin_gcs_events_handler* events_handler;
extern Plugin_gcs_view_modification_notifier* view_change_notifier;
extern Group_member_info* local_member_info;

/*
  These variables are handles to the registered event handlers in each GCS
  interface. To better understand the handle mechanism, please refer to
  the event handler registration mechanism in any GCS interface
 */
extern int gcs_communication_event_handle;
extern int gcs_control_event_handler;
extern int gcs_control_exchanged_data_handle;

//Plugin global methods
int configure_and_start_applier_module();
int configure_group_member_manager();
int terminate_applier_module();
int initialize_recovery_module();
int terminate_recovery_module();
int configure_and_start_group_communication();
void declare_plugin_running();
void register_server_reset_master();

//Plugin public methods
int plugin_group_replication_init(MYSQL_PLUGIN plugin_info);
int plugin_group_replication_deinit(void *p);
int plugin_group_replication_start();
int plugin_group_replication_stop();
bool plugin_is_group_replication_running();
bool plugin_get_connection_status(GROUP_REPLICATION_CONNECTION_STATUS_INFO *info);
bool plugin_get_group_members(uint index, GROUP_REPLICATION_GROUP_MEMBERS_INFO *info);
bool plugin_get_group_member_stats(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO *info);
uint plugin_get_group_members_number();
/**
  Method to set retrieved certification info from a recovery channel extracted
  from a given View_change event

  @note a copy of the certification info is made here.

  @param info   the given view_change_event.
*/
int plugin_group_replication_set_retrieved_certification_info(void* info);

#endif /* PLUGIN_INCLUDE */
