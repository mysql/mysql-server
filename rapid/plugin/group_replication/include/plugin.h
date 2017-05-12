/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "channel_observation_manager.h"
#include "ps_information.h"

#include <mysql/gcs/gcs_interface.h>
#include "gcs_event_handlers.h"
#include "gcs_view_modification_notifier.h"
#include "compatibility_module.h"
#include "auto_increment.h"
#include "read_mode_handler.h"
#include "delayed_plugin_initialization.h"
#include "gcs_operations.h"

#include "plugin_constants.h"
#include "plugin_server_include.h"
#include <mysql/plugin.h>
#include <mysql/plugin_group_replication.h>

//Definition of system var structures

//Definition of system vars structure for access their information in the plugin
struct st_mysql_sys_var
{
  MYSQL_PLUGIN_VAR_HEADER;
};
typedef st_mysql_sys_var SYS_VAR;

//Plugin variables
extern const char *group_replication_plugin_name;
extern char *group_name_var;
extern rpl_sidno group_sidno;
extern bool wait_on_engine_initialization;
extern bool delay_gr_user_creation;
extern bool server_shutdown_status;
extern const char *available_bindings_names[];
//Flag to register server rest master command invocations
extern bool known_server_reset;
//Certification latch
extern Wait_ticket<my_thread_id> *certification_latch;

//The modules
extern Gcs_operations *gcs_module;
extern Applier_module *applier_module;
extern Recovery_module *recovery_module;
extern Group_member_info_manager_interface *group_member_mgr;
extern Channel_observation_manager *channel_observation_manager;
//Lock for the applier and recovery module to prevent the race between STOP
//Group replication and ongoing transactions.
extern Shared_writelock *shared_plugin_stop_lock;
extern Read_mode_handler *read_mode_handler;
extern Delayed_initialization_thread *delayed_initialization_thread;

//Auxiliary Functionality
extern Plugin_gcs_events_handler* events_handler;
extern Plugin_gcs_view_modification_notifier* view_change_notifier;
extern Group_member_info* local_member_info;
extern Compatibility_module* compatibility_mgr;

//Plugin global methods
bool server_engine_initialized();
void *get_plugin_pointer();
int configure_and_start_applier_module();
int configure_group_member_manager();
int configure_compatibility_manager();
int terminate_applier_module();
int initialize_recovery_module();
int terminate_recovery_module();
int configure_group_communication(Sql_service_interface *sql_interface);
int start_group_communication();
void declare_plugin_running();
void register_server_reset_master();
int leave_group();
int terminate_plugin_modules();
bool get_allow_local_lower_version_join();
bool get_allow_local_disjoint_gtids_join();

//Plugin public methods
int plugin_group_replication_init(MYSQL_PLUGIN plugin_info);
int plugin_group_replication_deinit(void *p);
int plugin_group_replication_start();
int plugin_group_replication_stop();
bool plugin_is_group_replication_running();
bool plugin_get_connection_status(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS& callbacks);
bool plugin_get_group_members(
    uint index, const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS& callbacks);
bool plugin_get_group_member_stats(
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS& callbacks);
uint plugin_get_group_members_number();
/**
  Method to set retrieved certification info from a recovery channel extracted
  from a given View_change event

  @note a copy of the certification info is made here.

  @param info   the given view_change_event.
*/
int plugin_group_replication_set_retrieved_certification_info(void* info);

#endif /* PLUGIN_INCLUDE */
