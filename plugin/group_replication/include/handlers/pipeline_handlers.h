/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#ifndef PIPELINE_HANDLERS_INCLUDED
#define PIPELINE_HANDLERS_INCLUDED

#include <assert.h>
#include <mysql/group_replication_priv.h>

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/pipeline_interfaces.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"

/*
  @enum Event modifier
  Enumeration type for the different kinds of pipeline event modifiers.
*/
enum enum_event_modifier {
  TRANSACTION_BEGIN = 1,  ///< transaction start event
  TRANSACTION_END = 2,    ///< transaction end event
  UNMARKED_EVENT = 3,     ///< transaction regular event
  SINGLE_VIEW_EVENT = 4,  ///< the current Pipeline_event only contains
                          ///< a single view event injected from GCS
};

/**
  @enum enum_handler_role
  Enumeration type for the different roles of the used handlers.
*/
enum enum_handler_role {
  EVENT_CATALOGER = 0,
  APPLIER = 1,
  CERTIFIER = 2,
  QUEUER = 3,
  ROLE_NUMBER = 4  // The number of roles
};

/**
  @enum enum_group_replication_handler_actions

  Enumeration of all actions sent to the plugin handlers.
*/
typedef enum enum_group_replication_handler_actions {
  HANDLER_START_ACTION = 0,         // Action that signals the handlers to start
  HANDLER_STOP_ACTION = 1,          // Action that signals the handlers to stop
  HANDLER_APPLIER_CONF_ACTION = 2,  // Configuration for applier handlers
  HANDLER_CERT_CONF_ACTION = 3,     // Configuration for certification handlers
  HANDLER_CERT_INFO_ACTION = 4,     // Certification info for the certifier
  HANDLER_VIEW_CHANGE_ACTION = 5,   // Certification notification on view change
  HANDLER_GCS_INTERFACE_ACTION = 6,  // Action with GCS interfaces to be used.
  HANDLER_THD_ACTION = 7,    // Configuration action that carries a THD obj
  HANDLER_ACTION_NUMBER = 8  // The number of roles
} Plugin_handler_action;

/**
  @class Handler_start_action

  Action to signal the handler to start existing routines
*/
class Handler_start_action : public Pipeline_action {
 public:
  Handler_start_action() : Pipeline_action(HANDLER_START_ACTION) {}

  ~Handler_start_action() override = default;
};

/**
  @class Handler_stop_action

  Action to signal the handler to stop existing routines
*/
class Handler_stop_action : public Pipeline_action {
 public:
  Handler_stop_action() : Pipeline_action(HANDLER_STOP_ACTION) {}

  ~Handler_stop_action() override = default;
};

/**
  @class Handler_applier_configuration_action

  Action to configure existing applier handlers
*/
class Handler_applier_configuration_action : public Pipeline_action {
 public:
  /**
   Configuration for applier handlers

   @param applier_name              the applier's channel name
   @param reset_logs                if a reset was executed in the server
   @param plugin_shutdown_timeout   the plugin's timeout for component shutdown
   @param group_sidno               the group configured sidno
  */
  Handler_applier_configuration_action(char *applier_name, bool reset_logs,
                                       ulong plugin_shutdown_timeout,
                                       rpl_sidno group_sidno)
      : Pipeline_action(HANDLER_APPLIER_CONF_ACTION),
        applier_name(applier_name),
        reset_logs(reset_logs),
        applier_shutdown_timeout(plugin_shutdown_timeout),
        group_sidno(group_sidno),
        initialization_conf(true) {
    assert(applier_name != nullptr);
  }

  /**
   Configuration for applier handlers

   @param plugin_shutdown_timeout   the plugin's timeout for component shutdown
  */
  Handler_applier_configuration_action(ulong plugin_shutdown_timeout)
      : Pipeline_action(HANDLER_APPLIER_CONF_ACTION),
        applier_name(nullptr),
        reset_logs(false),
        applier_shutdown_timeout(plugin_shutdown_timeout),
        group_sidno(0),
        initialization_conf(false) {}

  ~Handler_applier_configuration_action() override = default;

  /**
    @return the applier's name
      @retval NULL    if not defined
      @retval !=NULL  if defined
   */
  char *get_applier_name() { return applier_name; }

  ulong get_applier_shutdown_timeout() { return applier_shutdown_timeout; }

  bool is_reset_logs_planned() { return reset_logs; }

  rpl_sidno get_sidno() { return group_sidno; }

  /**
   Informs if this is a action with configurations for initialization or just
   timeout configurations.

   @retval true    if initialization action
   @retval false   if timeout configuration action
  */
  bool is_initialization_conf() { return initialization_conf; }

 private:
  /*The applier's name, used for channel naming*/
  char *applier_name;
  /*If a reset was executed in the server */
  bool reset_logs;
  /*The plugin's timeout for component shutdown set in the applier*/
  ulong applier_shutdown_timeout;
  /*The configured group sidno*/
  rpl_sidno group_sidno;

  // Internal fields

  /*If this is a initialization packet or not*/
  bool initialization_conf;
};

/**
  @class Handler_certifier_configuration_action

  Action to configure existing certification handlers
*/
class Handler_certifier_configuration_action : public Pipeline_action {
 public:
  /**
   Configuration for certification handlers

   @param group_sidno                the group sidno
   @param gtid_assignment_block_size the group gtid assignment block size
  */
  Handler_certifier_configuration_action(rpl_sidno group_sidno,
                                         ulonglong gtid_assignment_block_size)
      : Pipeline_action(HANDLER_CERT_CONF_ACTION),
        group_sidno(group_sidno),
        gtid_assignment_block_size(gtid_assignment_block_size) {}

  rpl_sidno get_group_sidno() { return group_sidno; }

  ulonglong get_gtid_assignment_block_size() {
    return gtid_assignment_block_size;
  }

 private:
  rpl_sidno group_sidno;
  ulonglong gtid_assignment_block_size;
};

/**
  @class Handler_certifier_information_action

  Action that carries a certification info to be
  applied on certification handlers.
*/
class Handler_certifier_information_action : public Pipeline_action {
 public:
  /**
    Create an action to communicate certification info to a certifier

    @param cert_info   A certification info
  */
  Handler_certifier_information_action(
      std::map<std::string, std::string> *cert_info)
      : Pipeline_action(HANDLER_CERT_INFO_ACTION),
        certification_info(cert_info) {}

  std::map<std::string, std::string> *get_certification_info() {
    return certification_info;
  }

 private:
  std::map<std::string, std::string> *certification_info;
};

/**
  @class View_change_pipeline_action

  Action to signal any interested handler that a VC happened
*/
class View_change_pipeline_action : public Pipeline_action {
 public:
  /**
    Creates an action to inform handler of a View Change

    @param is_leaving informs if the member is leaving
  */
  View_change_pipeline_action(bool is_leaving)
      : Pipeline_action(HANDLER_VIEW_CHANGE_ACTION), leaving(is_leaving) {}

  bool is_leaving() { return leaving; }

 private:
  /*If this member is leaving the group on this view change*/
  bool leaving;
};

/**
  @class Handler_THD_setup_action

  Action that gives handlers access to the a THD object.
  This THD would usually belong to the caller avoiding the current_thd macro
*/
class Handler_THD_setup_action : public Pipeline_action {
 public:
  /**
    An action that a THD object

    @param given_thread  a pointer to a THD object
  */
  Handler_THD_setup_action(THD *given_thread)
      : Pipeline_action(HANDLER_THD_ACTION), shared_thd_object(given_thread) {}

  THD *get_THD_object() { return shared_thd_object; }

 private:
  THD *shared_thd_object;
};

#endif /* PIPELINE_HANDLERS_INCLUDED */
