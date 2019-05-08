/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_INCLUDE
#define PLUGIN_INCLUDE

#include <mysql/plugin.h>
#include <mysql/plugin_group_replication.h>

#include "plugin/group_replication/include/applier.h"
#include "plugin/group_replication/include/asynchronous_channels_state_observer.h"
#include "plugin/group_replication/include/auto_increment.h"
#include "plugin/group_replication/include/compatibility_module.h"
#include "plugin/group_replication/include/delayed_plugin_initialization.h"
#include "plugin/group_replication/include/gcs_event_handlers.h"
#include "plugin/group_replication/include/gcs_operations.h"
#include "plugin/group_replication/include/gcs_view_modification_notifier.h"
#include "plugin/group_replication/include/group_actions/group_action_coordinator.h"
#include "plugin/group_replication/include/plugin_constants.h"
#include "plugin/group_replication/include/plugin_handlers/group_partition_handling.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_invocation_handler.h"
#include "plugin/group_replication/include/plugin_handlers/read_mode_handler.h"
#include "plugin/group_replication/include/plugin_observers/channel_observation_manager.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/include/plugin_observers/group_transaction_observation_manager.h"
#include "plugin/group_replication/include/plugin_server_include.h"
#include "plugin/group_replication/include/ps_information.h"
#include "plugin/group_replication/include/recovery.h"
#include "plugin/group_replication/include/services/registry.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_interface.h"

// Forward declarations
class Autorejoin_thread;
class Hold_transactions;
class Transaction_consistency_manager;

// Definition of system var structures

// Definition of system vars structure for access their information in the
// plugin
struct SYS_VAR {
  MYSQL_PLUGIN_VAR_HEADER;
};

// Plugin variables

/**
  Position of channel observation manager's in channel_observation_manager_list
*/
enum enum_channel_observation_manager_position {
  GROUP_CHANNEL_OBSERVATION_MANAGER_POS = 0,
  ASYNC_CHANNEL_OBSERVATION_MANAGER_POS,
  END_CHANNEL_OBSERVATION_MANAGER_POS
};

enum enum_exit_state_action {
  EXIT_STATE_ACTION_READ_ONLY = 0,
  EXIT_STATE_ACTION_ABORT_SERVER
};

/**
  This struct provides a namespace for the GR layer components.
*/
struct gr_modules {
  /**
    @enum gr_modules_enum
    @brief Represents the GR layer modules that can be initialized
    and/or terminated at will while the plugin is running.
    @see terminate_plugin_modules
  */
  enum gr_modules_enum {
    RECOVERY_MODULE = 0,
    GROUP_ACTION_COORDINATOR,
    PRIMARY_ELECTION_HANDLER,
    AUTO_INCREMENT_HANDLER,
    APPLIER_MODULE,
    ASYNC_REPL_CHANNELS,
    GROUP_PARTITION_HANDLER,
    AUTOREJOIN_THREAD,
    BLOCKED_TRANSACTION_HANDLER,
    CERTIFICATION_LATCH,
    GROUP_MEMBER_MANAGER,
    REGISTRY_MODULE,
    WAIT_ON_START,
    COMPATIBILITY_MANAGER,
    GCS_EVENTS_HANDLER,
    NUM_MODULES
  };
  using mask = std::bitset<NUM_MODULES>;
  static constexpr mask all_modules = (1 << NUM_MODULES) - 1;
};

extern const char *group_replication_plugin_name;
extern char *group_name_var;
extern rpl_sidno group_sidno;
extern bool wait_on_engine_initialization;
extern bool server_shutdown_status;
extern const char *available_bindings_names[];
extern char *communication_debug_options_var;
extern char *communication_debug_file_var;
// Flag to register if read mode is being set
extern bool plugin_is_setting_read_mode;
// Flag to register server rest master command invocations
extern bool known_server_reset;
// Latch used as the control point of the event driven
// management of the transactions.
extern Wait_ticket<my_thread_id> *transactions_latch;
extern ulong exit_state_action_var;
extern std::atomic<bool> plugin_is_stopping;

/**
  The plugin modules.

  @note Whenever you want to create a new plugin module, be sure to add it to
  the gr_modules enum (@sa gr_modules) and see if it's part of the rejoin
  process.
*/
extern Gcs_operations *gcs_module;
extern Applier_module *applier_module;
extern Recovery_module *recovery_module;
extern Registry_module_interface *registry_module;
extern Group_member_info_manager_interface *group_member_mgr;
extern Group_events_observation_manager *group_events_observation_manager;
extern Channel_observation_manager_list *channel_observation_manager_list;
extern Asynchronous_channels_state_observer
    *asynchronous_channels_state_observer;
extern Transaction_consistency_manager *transaction_consistency_manager;
// Lock for the applier and recovery module to prevent the race between STOP
// Group replication and ongoing transactions.
extern Group_transaction_observation_manager
    *group_transaction_observation_manager;
extern Shared_writelock *shared_plugin_stop_lock;
extern Delayed_initialization_thread *delayed_initialization_thread;
extern Group_action_coordinator *group_action_coordinator;
extern Primary_election_handler *primary_election_handler;
extern Hold_transactions *hold_transactions;
extern Autorejoin_thread *autorejoin_module;

// Auxiliary Functionality
extern Plugin_gcs_events_handler *events_handler;
extern Group_member_info *local_member_info;
extern Compatibility_module *compatibility_mgr;
extern Group_partition_handling *group_partition_handler;
extern Blocked_transaction_handler *blocked_transaction_handler;

// Plugin global methods
bool server_engine_initialized();
void *get_plugin_pointer();
mysql_mutex_t *get_plugin_running_lock();
Plugin_waitlock *get_plugin_online_lock();
int initialize_plugin_and_join(enum_plugin_con_isolation sql_api_isolation,
                               Delayed_initialization_thread *delayed_init_thd);
int initialize_plugin_modules(gr_modules::mask modules_to_init);
int terminate_plugin_modules(gr_modules::mask modules_to_terminate,
                             char **error_message = nullptr);
void register_server_reset_master();
bool get_allow_local_lower_version_join();
ulong get_transaction_size_limit();
bool is_plugin_waiting_to_set_server_read_mode();
bool check_async_channel_running_on_secondary();
void set_enforce_update_everywhere_checks(bool option);
void set_single_primary_mode_var(bool option);
void set_auto_increment_handler_values();
void reset_auto_increment_handler_values(bool force_reset = false);
SERVICE_TYPE(registry) * get_plugin_registry();
rpl_sidno get_group_sidno();
bool is_autorejoin_enabled();
uint get_number_of_autorejoin_tries();
ulonglong get_rejoin_timeout();

/**
  Encapsulates the logic necessary to attempt a rejoin, i.e. gracefully leave
  the group, terminate GCS infrastructure, terminate auto-rejoin relevant plugin
  modules, reinitialize auto-rejoin relevant plugin modules, reinitialize GCS
  infrastructure and attempt to join the group again.

  @returns a flag indicating success or failure.
  @retval true the rejoin failed.
  @retval false the rejoin succeeded.
*/
bool attempt_rejoin();

// Plugin public methods
int plugin_group_replication_init(MYSQL_PLUGIN plugin_info);
int plugin_group_replication_deinit(void *p);
int plugin_group_replication_start(char **error_message = NULL);
int plugin_group_replication_stop(char **error_message = NULL);
bool plugin_is_group_replication_running();
bool is_plugin_auto_starting_on_non_bootstrap_member();
bool is_plugin_configured_and_starting();
void initiate_wait_on_start_process();
void terminate_wait_on_start_process();
void set_wait_on_start_process(bool cond);
bool plugin_get_connection_status(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS &callbacks);
bool plugin_get_group_members(
    uint index, const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS &callbacks);
bool plugin_get_group_member_stats(
    uint index,
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS &callbacks);
uint plugin_get_group_members_number();

/**
  Method to set retrieved certification info from a recovery channel extracted
  from a given View_change event

  @note a copy of the certification info is made here.

  @param info   the given view_change_event.
*/
int plugin_group_replication_set_retrieved_certification_info(void *info);

#endif /* PLUGIN_INCLUDE */
