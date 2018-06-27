/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>

#include "my_compiler.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_lz4.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_ssl_transport.h"

using std::map;
using std::string;
using std::vector;

Gcs_interface *Gcs_xcom_interface::interface_reference_singleton = NULL;

/*
  Keep track of the last configuration installed by the node.
*/
synode_no last_config_id;

int xcom_local_port = 0;

/*
  Interface to access XCOM.
*/
static Gcs_xcom_proxy *xcom_proxy;

/*
  Engine to process XCOM's notifications.
*/
static Gcs_xcom_engine *gcs_engine;

void cb_xcom_receive_data(synode_no message_id, node_set nodes, u_int size,
                          char *data);
void do_cb_xcom_receive_data(synode_no message_id, Gcs_xcom_nodes *xcom_nodes,
                             u_int size, char *data);
void cb_xcom_receive_local_view(synode_no message_id, node_set nodes);
void do_cb_xcom_receive_local_view(synode_no message_id,
                                   Gcs_xcom_nodes *xcom_nodes);
void cb_xcom_receive_global_view(synode_no config_id, synode_no message_id,
                                 node_set nodes);
void do_cb_xcom_receive_global_view(synode_no config_id, synode_no message_id,
                                    Gcs_xcom_nodes *xcom_nodes);
void cb_xcom_comms(int status);
void cb_xcom_ready(int status);
void cb_xcom_exit(int status);
void cb_xcom_fatal_error(int status);

synode_no cb_xcom_get_app_snap(blob *gcs_snap);
int cb_xcom_get_should_exit();
void cb_xcom_handle_app_snap(blob *gcs_snap);
int cb_xcom_socket_accept(int fd, site_def const *xcom_config);

// XCom logging callback
void cb_xcom_logger(const int64_t level, const char *message);
void cb_xcom_debugger(const char *format, ...)
    MY_ATTRIBUTE((format(printf, 1, 2)));
int cb_xcom_debugger_check(const int64_t options);

Gcs_interface *Gcs_xcom_interface::get_interface() {
  if (interface_reference_singleton == NULL) {
#ifdef SAFE_MUTEX
    /*
      Must invoke this function in order for safe mutexes to be used when GCS is
      used without the server, like in unit or JET tests.
    */
    safe_mutex_global_init();
#endif

    interface_reference_singleton = new Gcs_xcom_interface();
  }

  return interface_reference_singleton;
}

void Gcs_xcom_interface::cleanup() {
  if (interface_reference_singleton != NULL &&
      !interface_reference_singleton->is_initialized()) {
    delete interface_reference_singleton;
    interface_reference_singleton = NULL;
  }

  cleanup_thread_ssl_resources();
}

void Gcs_xcom_interface::cleanup_thread_ssl_resources() {
  ::xcom_cleanup_ssl();
}

Gcs_xcom_interface::Gcs_xcom_interface()
    : m_group_interfaces(),
      m_xcom_configured_groups(),
      m_node_address(NULL),
      m_xcom_peers(),
      m_is_initialized(false),
      m_boot(false),
      m_socket_util(NULL),
      m_gcs_xcom_app_cfg(),
      m_initialization_parameters(),
      m_default_sink(NULL),
      m_default_logger(NULL),
      m_default_debugger(NULL),
      m_ip_whitelist(),
      m_ssl_init_state(-1),
      m_wait_for_ssl_init_cond(),
      m_wait_for_ssl_init_mutex() {
  // Initialize random seed
  srand(static_cast<unsigned int>(time(0)));

  My_xp_util::init_time();
}

Gcs_xcom_interface::~Gcs_xcom_interface() {}

enum_gcs_error Gcs_xcom_interface::initialize_logging(
    const std::string *debug_file, const std::string *debug_path) {
  assert(m_default_sink == NULL);

#ifndef XCOM_STANDALONE
  if (debug_file != NULL && debug_path != NULL)
    m_default_sink =
        new Gcs_async_buffer(new Gcs_file_sink(*debug_file, *debug_path));
  else
#endif /* XCOM_STANDALONE */
       /* purecov: begin inspected */
    m_default_sink = new Gcs_async_buffer(new Gcs_output_sink());
  /* purecov: end */

  if (m_default_sink->initialize()) /* purecov: begin inspected */
    return GCS_NOK;
  /* purecov: end */

  if (Gcs_debug_manager::get_debugger() == NULL) {
    m_default_debugger = new Gcs_default_debugger(m_default_sink);
    if (Gcs_debug_manager::initialize(m_default_debugger))
      /* purecov: begin inspected */
      return GCS_NOK;
    /* purecov: end */
    MYSQL_GCS_LOG_INFO("Debug messages will be sent to: "
                           << m_default_sink->get_information(););
  }

  if (Gcs_log_manager::get_logger() == NULL) {
    /* purecov: begin tested */
    m_default_logger = new Gcs_default_logger(m_default_sink);
    if (Gcs_log_manager::initialize(m_default_logger)) return GCS_NOK;
    MYSQL_GCS_LOG_INFO(
        "Log messages will be sent to: " << m_default_sink->get_information(););
    /* purecov: end */
  }

  /*
    Set the xcom logging callback.
  */
  ::set_xcom_logger(cb_xcom_logger);
  ::set_xcom_debugger(cb_xcom_debugger);
  ::set_xcom_debugger_check(cb_xcom_debugger_check);

  return GCS_OK;
}

enum_gcs_error Gcs_xcom_interface::finalize_logging() {
  Gcs_log_manager::finalize();

  if (m_default_logger != NULL) {
    /* purecov: begin inspected */
    m_default_logger->finalize();
    delete m_default_logger;
    m_default_logger = NULL;
    /* purecov: end */
  }

  Gcs_debug_manager::finalize();

  if (m_default_debugger != NULL) {
    m_default_debugger->finalize();
    delete m_default_debugger;
    m_default_debugger = NULL;
  }

  if (m_default_sink != NULL) {
    m_default_sink->finalize();
    delete m_default_sink;
    m_default_sink = NULL;
  }

  return GCS_OK;
}

enum_gcs_error Gcs_xcom_interface::initialize(
    const Gcs_interface_parameters &interface_params) {
  const std::string *ip_whitelist_str = NULL;
  Gcs_interface_parameters validated_params;

  if (is_initialized()) return GCS_OK;

  register_gcs_thread_psi_keys();
  register_gcs_mutex_cond_psi_keys();
  register_xcom_memory_psi_keys();

  last_config_id.group_id = 0;

  m_wait_for_ssl_init_mutex.init(
      key_GCS_MUTEX_Gcs_xcom_interface_m_wait_for_ssl_init_mutex, NULL);
  m_wait_for_ssl_init_cond.init(
      key_GCS_COND_Gcs_xcom_interface_m_wait_for_ssl_init_cond);

  /*
    Initialize logging sub-systems.
  */
  if (initialize_logging(
          interface_params.get_parameter("communication_debug_file"),
          interface_params.get_parameter("communication_debug_path")))
    /* purecov: begin deadcode */
    goto err;
  /* purecov: end */

  /*
    Copy the parameters to an internal structure.
  */
  validated_params.add_parameters_from(interface_params);

  /*
   Initialize the network structures in XCom before validating the parameters
   since we are going to need network infrastructure for validating checking
   host names.
  */
  Gcs_xcom_utils::init_net();

  // validate whitelist
  ip_whitelist_str = validated_params.get_parameter("ip_whitelist");

  if (ip_whitelist_str && !m_ip_whitelist.is_valid(*ip_whitelist_str)) goto err;

  /*
    ---------------------------------------------------------------
     Fix the input parameters - replaces aliases, sets defaults, etc.
    ---------------------------------------------------------------
  */
  fix_parameters_syntax(validated_params);

  /*
    ---------------------------------------------------------------
    Perform syntax checks
    ---------------------------------------------------------------
  */
  if (!is_parameters_syntax_correct(validated_params)) goto err;

  /*
    ---------------------------------------------------------------
    Perform semantic checks
    ---------------------------------------------------------------
  */
  // Mandatory at this point
  if (!validated_params.get_parameter("group_name") ||
      !validated_params.get_parameter("peer_nodes") ||
      !validated_params.get_parameter("local_node") ||
      !validated_params.get_parameter("bootstrap_group")) {
    MYSQL_GCS_LOG_ERROR("The group_name, peer_nodes, local_node or"
                        << " bootstrap_group parameters were not specified.")
    goto err;
  }

  /*
    ---------------------------------------------------------------
    Proceed with initialization
    ---------------------------------------------------------------
  */

  // initialize xcom's data structures to pass configuration
  // from the application
  m_gcs_xcom_app_cfg.init();
  this->clean_group_interfaces();
  m_socket_util = new My_xp_socket_util_impl();

  m_is_initialized = !(initialize_xcom(validated_params));

  if (!m_is_initialized) {
    MYSQL_GCS_LOG_ERROR("Error initializing the group communication engine.")
    goto err;
  }

  m_initialization_parameters.add_parameters_from(validated_params);

  return GCS_OK;

err:
  /*
    Deinitialize network structures
  */
  m_gcs_xcom_app_cfg.deinit();
  Gcs_xcom_utils::deinit_net();
  delete m_socket_util;
  m_socket_util = NULL;
  /*
   Clear logging here. This should be done in case of failure
   on initialize.
  */
  finalize_logging();

  m_wait_for_ssl_init_mutex.destroy();
  m_wait_for_ssl_init_cond.destroy();

  return GCS_NOK;
}

enum_gcs_error Gcs_xcom_interface::configure(
    const Gcs_interface_parameters &interface_params) {
  bool reconfigured = false;
  enum_gcs_error error = GCS_OK;
  Gcs_interface_parameters validated_params;
  map<std::string, gcs_xcom_group_interfaces *>::const_iterator
      registered_group;
  Gcs_xcom_control *xcom_control = NULL;

  // Error! Interface still not initialized or finalize has already been invoked
  if (!is_initialized()) return GCS_NOK;

  // fill in the copy of the parameters injected
  validated_params.add_parameters_from(interface_params);

  /*
    ---------------------------------------------------------------
    Fix the input parameters - replaces aliases, deprecations, etc.
    ---------------------------------------------------------------
  */
  fix_parameters_syntax(validated_params);

  /*
    ---------------------------------------------------------------
    Perform syntax checks
    ---------------------------------------------------------------
   */
  if (!is_parameters_syntax_correct(validated_params)) return GCS_NOK;

  // validate whitelist
  const std::string *ip_whitelist_str =
      validated_params.get_parameter("ip_whitelist");

  if (!ip_whitelist_str || !m_ip_whitelist.is_valid(*ip_whitelist_str)) {
    MYSQL_GCS_LOG_ERROR("The ip_whitelist parameter is not valid")
    return GCS_NOK;
  }

  /*
    ---------------------------------------------------------------
    Perform semantic checks
    ---------------------------------------------------------------
   */
  const std::string *group_name_str =
      validated_params.get_parameter("group_name");
  const std::string *local_node_str =
      validated_params.get_parameter("local_node");
  const std::string *peer_nodes_str =
      validated_params.get_parameter("peer_nodes");
  const std::string *bootstrap_group_str =
      validated_params.get_parameter("bootstrap_group");
  const std::string *poll_spin_loops_str =
      validated_params.get_parameter("poll_spin_loops");
  const std::string *join_attempts_str =
      validated_params.get_parameter("join_attempts");
  const std::string *join_sleep_time_str =
      validated_params.get_parameter("join_sleep_time");

  // Mandatory
  if (group_name_str == NULL) {
    MYSQL_GCS_LOG_ERROR("The group_name parameter was not specified.")
    return GCS_NOK;
  }

  /*
    If all of these parameters are NULL, return immediately.
    Otherwise, clean group interfaces.
  */
  if ((local_node_str == NULL) && (peer_nodes_str == NULL) &&
      (bootstrap_group_str == NULL)) {
    MYSQL_GCS_LOG_ERROR("The local_node, peer_nodes and bootstrap_group"
                        << " parameters were not specified.")
    return GCS_NOK;
  }

  // Try and retrieve already instantiated group interfaces for a certain group
  registered_group = m_group_interfaces.find(*group_name_str);

  // Error! Group interface does not exist
  if (registered_group == m_group_interfaces.end()) {
    MYSQL_GCS_LOG_ERROR("Group interface does not exist for group "
                        << group_name_str->c_str())
    error = GCS_NOK;
    goto end;
  }

  {
    Gcs_group_identifier group_id(*group_name_str);
    xcom_control = (Gcs_xcom_control *)get_control_session(group_id);
    if (((bootstrap_group_str != NULL) || (local_node_str != NULL)) &&
        xcom_control->belongs_to_group()) {
      /* Node still in the group */
      MYSQL_GCS_LOG_ERROR("Member is still in the group while trying to"
                          << " configure it.")
      error = GCS_NOK;
      goto end;
    }
  }

  /*
    ---------------------------------------------------------------
    Perform reconfiguration, since we are all good to go
    ---------------------------------------------------------------
   */
  if (bootstrap_group_str != NULL) {
    // Changing bootstrap_group
    bool received_boot_param = bootstrap_group_str->compare("on") == 0 ||
                               bootstrap_group_str->compare("true") == 0;

    m_boot = received_boot_param;
    xcom_control->set_boot_node(m_boot);

    reconfigured |= true;
  }

  if (local_node_str != NULL) {
    /* purecov: begin tested */
    // Changing local_node
    delete m_node_address;
    m_node_address = new Gcs_xcom_node_address(local_node_str->c_str());
    xcom_local_port = m_node_address->get_member_port();
    xcom_control->set_node_address(m_node_address);

    reconfigured |= true;
    /* purecov: end */
  }

  if (peer_nodes_str != NULL) {
    // Changing peer_nodes
    // Clear current nodes
    clear_peer_nodes();
    // Initialize
    initialize_peer_nodes(peer_nodes_str);

    xcom_control->set_peer_nodes(m_xcom_peers);

    reconfigured |= true;
  }

  if (poll_spin_loops_str != NULL && poll_spin_loops_str->size() > 0) {
    m_gcs_xcom_app_cfg.set_poll_spin_loops(
        (unsigned int)atoi(poll_spin_loops_str->c_str()));

    reconfigured |= true;
  }

  xcom_control->set_join_behavior(
      static_cast<unsigned int>(atoi(join_attempts_str->c_str())),
      static_cast<unsigned int>(atoi(join_sleep_time_str->c_str())));

  // Set suspicion configuration parameters
  configure_suspicions_mgr(validated_params,
                           xcom_control->get_suspicions_manager());

end:
  if (error == GCS_NOK || !reconfigured) {
    MYSQL_GCS_LOG_ERROR("Error while configuring the member.")
    return GCS_NOK;
  } else
    return GCS_OK;
}

void cleanup_xcom() {
  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  intf->finalize_xcom();
  xcom_proxy->xcom_destroy_ssl();
  xcom_proxy->xcom_set_ssl_mode(0 /* SSL_DISABLED */);
  xcom_proxy->xcom_set_ssl_fips_mode(0 /* SSL_FIPS_MODE_OFF */);
}

void Gcs_xcom_interface::finalize_xcom() {
  Gcs_group_identifier *group_identifier = NULL;
  map<u_long, Gcs_group_identifier *>::iterator xcom_configured_groups_it;
  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());

  for (xcom_configured_groups_it = m_xcom_configured_groups.begin();
       xcom_configured_groups_it != m_xcom_configured_groups.end();
       xcom_configured_groups_it++) {
    group_identifier = (*xcom_configured_groups_it).second;
    Gcs_xcom_control *control_if = static_cast<Gcs_xcom_control *>(
        intf->get_control_session(*group_identifier));
    if (control_if->is_xcom_running()) {
      MYSQL_GCS_LOG_DEBUG(
          "There is a request to finalize the member but apparently "
          "it is running. Calling leave now to stop it first.")
      control_if->do_leave();
    }
  }
}

void Gcs_xcom_interface::finalize_gcs_on_error() {
  Gcs_group_identifier *group_identifier = NULL;
  map<u_long, Gcs_group_identifier *>::iterator xcom_configured_groups_it;
  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());

  for (xcom_configured_groups_it = m_xcom_configured_groups.begin();
       xcom_configured_groups_it != m_xcom_configured_groups.end();
       xcom_configured_groups_it++) {
    group_identifier = (*xcom_configured_groups_it).second;
    Gcs_xcom_control *control_if = static_cast<Gcs_xcom_control *>(
        intf->get_control_session(*group_identifier));
    control_if->do_leave_gcs(Gcs_view::MEMBER_EXPELLED);
  }
}

enum_gcs_error Gcs_xcom_interface::finalize() {
  if (!is_initialized()) return GCS_NOK;

  // Finalize and delete the engine
  gcs_engine->finalize(cleanup_xcom);
  delete gcs_engine;
  gcs_engine = NULL;

  m_is_initialized = false;

  // Delete references...
  delete m_node_address;
  m_node_address = NULL;

  clean_group_references();

  clean_group_interfaces();

  clear_peer_nodes();

  // Delete the proxy
  delete xcom_proxy;
  xcom_proxy = NULL;

  delete m_socket_util;
  m_socket_util = NULL;

  Gcs_xcom_utils::deinit_net();

  // de-initialize data structures to pass configs to xcom
  m_gcs_xcom_app_cfg.deinit();

  // clear the cached parameters
  m_initialization_parameters.clear();

  // deinitialize logging
  finalize_logging();

  m_wait_for_ssl_init_mutex.destroy();
  m_wait_for_ssl_init_cond.destroy();

  return GCS_OK;
}

bool Gcs_xcom_interface::is_initialized() { return m_is_initialized; }

Gcs_control_interface *Gcs_xcom_interface::get_control_session(
    const Gcs_group_identifier &group_identifier) {
  gcs_xcom_group_interfaces *group_if = get_group_interfaces(group_identifier);

  return group_if == NULL ? NULL : group_if->control_interface;
}

Gcs_communication_interface *Gcs_xcom_interface::get_communication_session(
    const Gcs_group_identifier &group_identifier) {
  gcs_xcom_group_interfaces *group_if = get_group_interfaces(group_identifier);

  return group_if == NULL ? NULL : group_if->communication_interface;
}

/* purecov: begin deadcode */
Gcs_statistics_interface *Gcs_xcom_interface::get_statistics(
    const Gcs_group_identifier &group_identifier) {
  gcs_xcom_group_interfaces *group_if = get_group_interfaces(group_identifier);

  return group_if == NULL ? NULL : group_if->statistics_interface;
}
/* purecov: end */

Gcs_group_management_interface *Gcs_xcom_interface::get_management_session(
    const Gcs_group_identifier &group_identifier) {
  gcs_xcom_group_interfaces *group_if = get_group_interfaces(group_identifier);

  return group_if == NULL ? NULL : group_if->management_interface;
}

gcs_xcom_group_interfaces *Gcs_xcom_interface::get_group_interfaces(
    const Gcs_group_identifier &group_identifier) {
  if (!is_initialized()) return NULL;

  // Try and retrieve already instantiated group interfaces for a certain group
  map<std::string, gcs_xcom_group_interfaces *>::const_iterator
      registered_group;
  registered_group = m_group_interfaces.find(group_identifier.get_group_id());

  gcs_xcom_group_interfaces *group_interface = NULL;
  if (registered_group == m_group_interfaces.end()) {
    /*
      Retrieve some initialization parameters.
    */
    const std::string *join_attempts_str =
        m_initialization_parameters.get_parameter("join_attempts");
    const std::string *join_sleep_time_str =
        m_initialization_parameters.get_parameter("join_sleep_time");

    /*
      If the group interfaces do not exist, create and add them to
      the dictionary.
    */
    group_interface = new gcs_xcom_group_interfaces();
    m_group_interfaces[group_identifier.get_group_id()] = group_interface;

    Gcs_xcom_statistics *stats = new Gcs_xcom_statistics();

    group_interface->statistics_interface = stats;

    Gcs_xcom_view_change_control_interface *vce =
        new Gcs_xcom_view_change_control();

    group_interface->communication_interface =
        new Gcs_xcom_communication(stats, xcom_proxy, vce);

    Gcs_xcom_state_exchange_interface *se =
        new Gcs_xcom_state_exchange(group_interface->communication_interface);

    Gcs_xcom_group_management *xcom_group_management =
        new Gcs_xcom_group_management(xcom_proxy, group_identifier);
    group_interface->management_interface = xcom_group_management;

    Gcs_xcom_control *xcom_control = new Gcs_xcom_control(
        m_node_address, m_xcom_peers, group_identifier, xcom_proxy,
        xcom_group_management, gcs_engine, se, vce, m_boot, m_socket_util);
    group_interface->control_interface = xcom_control;

    xcom_control->set_join_behavior(
        static_cast<unsigned int>(atoi(join_attempts_str->c_str())),
        static_cast<unsigned int>(atoi(join_sleep_time_str->c_str())));

    // Set suspicion configuration parameters
    configure_suspicions_mgr(
        m_initialization_parameters,
        static_cast<Gcs_xcom_control *>(group_interface->control_interface)
            ->get_suspicions_manager());

    // Store the created objects for later deletion
    group_interface->vce = vce;
    group_interface->se = se;

    configure_message_stages(group_identifier);
  } else {
    group_interface = registered_group->second;
  }

  return group_interface;
}

enum_gcs_error Gcs_xcom_interface::set_logger(Logger_interface *logger) {
  return Gcs_log_manager::initialize(logger);
}

void Gcs_xcom_interface::clean_group_interfaces() {
  map<string, gcs_xcom_group_interfaces *>::iterator group_if;
  for (group_if = m_group_interfaces.begin();
       group_if != m_group_interfaces.end(); group_if++) {
    delete (*group_if).second->vce;
    delete (*group_if).second->se;

    delete (*group_if).second->communication_interface;
    delete (*group_if).second->control_interface;
    delete (*group_if).second->statistics_interface;
    delete (*group_if).second->management_interface;

    delete (*group_if).second;
  }

  m_group_interfaces.clear();
}

void Gcs_xcom_interface::clean_group_references() {
  map<u_long, Gcs_group_identifier *>::iterator xcom_configured_groups_it;
  for (xcom_configured_groups_it = m_xcom_configured_groups.begin();
       xcom_configured_groups_it != m_xcom_configured_groups.end();
       xcom_configured_groups_it++) {
    delete (*xcom_configured_groups_it).second;
  }

  m_xcom_configured_groups.clear();
}

void start_ssl() {
  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  intf->initialize_ssl();
}

void Gcs_xcom_interface::initialize_ssl() {
  m_wait_for_ssl_init_mutex.lock();
  m_ssl_init_state = xcom_proxy->xcom_init_ssl();
  m_wait_for_ssl_init_cond.broadcast();
  m_wait_for_ssl_init_mutex.unlock();
}

bool Gcs_xcom_interface::initialize_xcom(
    const Gcs_interface_parameters &interface_params) {
  /*
    Whether the proxy should be created or not.
  */
  bool create_proxy = (xcom_proxy == NULL ? true : false);

  /*
    Since initializing XCom is actually joining the group itself, one shall
    delegate the xcom init to the join() method in the control interface.

    Here we will just load the group configuration present in an external file
    or passed though properties.
  */
  if (is_initialized()) return false;

  /**
   These parameters must have been validated syntatically on the caller.
   @c Gcs_xcom_interface::initialize.
   */

  const std::string *group_name = interface_params.get_parameter("group_name");
  const std::string *peers = interface_params.get_parameter("peer_nodes");
  const std::string *local_node_str =
      interface_params.get_parameter("local_node");
  const std::string *bootstrap_group_str =
      interface_params.get_parameter("bootstrap_group");
  const std::string *poll_spin_loops_str =
      interface_params.get_parameter("poll_spin_loops");
  const std::string *ip_whitelist_str =
      interface_params.get_parameter("ip_whitelist");

  set_xcom_group_information(*group_name);

  initialize_peer_nodes(peers);

  MYSQL_GCS_LOG_DEBUG("Configured total number of peers: %llu",
                      static_cast<long long unsigned>(m_xcom_peers.size()))

  m_node_address = new Gcs_xcom_node_address(local_node_str->c_str());
  xcom_local_port = m_node_address->get_member_port();

  MYSQL_GCS_LOG_DEBUG("Configured Local member: %s", local_node_str->c_str())

  m_boot = bootstrap_group_str->compare("on") == 0 ||
           bootstrap_group_str->compare("true") == 0;

  MYSQL_GCS_LOG_DEBUG("Configured Bootstrap: %s", bootstrap_group_str->c_str())

  // configure poll spin loops
  if (poll_spin_loops_str != NULL) {
    m_gcs_xcom_app_cfg.set_poll_spin_loops(
        (unsigned int)atoi(poll_spin_loops_str->c_str()));
  }

  // configure whitelist
  if (ip_whitelist_str) m_ip_whitelist.configure(*ip_whitelist_str);

  // Register XCom callbacks
  ::set_xcom_data_receiver(cb_xcom_receive_data);
  ::set_xcom_local_view_receiver(cb_xcom_receive_local_view);
  ::set_xcom_global_view_receiver(cb_xcom_receive_global_view);
  ::set_port_matcher(cb_xcom_match_port);
  ::set_app_snap_handler(cb_xcom_handle_app_snap);
  ::set_should_exit_getter(cb_xcom_get_should_exit);
  ::set_app_snap_getter(cb_xcom_get_app_snap);
  ::set_xcom_run_cb(cb_xcom_ready);
  ::set_xcom_comms_cb(cb_xcom_comms);
  ::set_xcom_exit_cb(cb_xcom_exit);
  ::set_xcom_fatal_error_cb(cb_xcom_fatal_error);
  ::set_xcom_socket_accept_cb(cb_xcom_socket_accept);

  const std::string *wait_time_str =
      interface_params.get_parameter("wait_time");

  MYSQL_GCS_LOG_DEBUG("Configured waiting time(s): %s", wait_time_str->c_str())

  int wait_time = atoi(wait_time_str->c_str());
  assert(wait_time > 0);

  // Setup the proxy
  if (create_proxy)
    xcom_proxy = new Gcs_xcom_proxy_impl(static_cast<unsigned int>(wait_time));

  // Setup the processing engine
  gcs_engine = new Gcs_xcom_engine();
  gcs_engine->initialize(NULL);

  const std::string *ssl_mode_str = interface_params.get_parameter("ssl_mode");
  const std::string *ssl_fips_mode_str =
      interface_params.get_parameter("ssl_fips_mode");
  if (ssl_mode_str) {
    int ssl_mode_int = xcom_proxy->xcom_get_ssl_mode(ssl_mode_str->c_str());
    if (ssl_mode_int == -1) /* INVALID_SSL_MODE */
    {
      MYSQL_GCS_LOG_ERROR(
          "Requested invalid SSL mode: " << ssl_mode_str->c_str());

      goto error;
    }
    xcom_proxy->xcom_set_ssl_mode(ssl_mode_int);
  }

  if (ssl_fips_mode_str) {
    int ssl_fips_mode_int =
        xcom_proxy->xcom_get_ssl_fips_mode(ssl_fips_mode_str->c_str());
    if (ssl_fips_mode_int == -1) /* INVALID_SSL_FIPS_MODE */
    {
      MYSQL_GCS_LOG_ERROR(
          "Requested invalid SSL FIPS mode: " << ssl_fips_mode_str->c_str());

      goto error;
    }
    xcom_proxy->xcom_set_ssl_fips_mode(ssl_fips_mode_int);
  }

  if (xcom_proxy->xcom_use_ssl()) {
    const std::string *server_key_file =
        interface_params.get_parameter("server_key_file");
    const std::string *server_cert_file =
        interface_params.get_parameter("server_cert_file");
    const std::string *client_key_file =
        interface_params.get_parameter("client_key_file");
    const std::string *client_cert_file =
        interface_params.get_parameter("client_cert_file");
    const std::string *ca_file = interface_params.get_parameter("ca_file");
    const std::string *ca_path = interface_params.get_parameter("ca_path");
    const std::string *crl_file = interface_params.get_parameter("crl_file");
    const std::string *crl_path = interface_params.get_parameter("crl_path");
    const std::string *cipher = interface_params.get_parameter("cipher");
    const std::string *tls_version =
        interface_params.get_parameter("tls_version");

    xcom_proxy->xcom_set_ssl_parameters(
        server_key_file ? server_key_file->c_str() : NULL,
        server_cert_file ? server_cert_file->c_str() : NULL,
        client_key_file ? client_key_file->c_str() : NULL,
        client_cert_file ? client_cert_file->c_str() : NULL,
        ca_file ? ca_file->c_str() : NULL, ca_path ? ca_path->c_str() : NULL,
        crl_file ? crl_file->c_str() : NULL,
        crl_path ? crl_path->c_str() : NULL, cipher ? cipher->c_str() : NULL,
        tls_version ? tls_version->c_str() : NULL);

    m_wait_for_ssl_init_mutex.lock();
    gcs_engine->push(new Initialize_notification(start_ssl));
    while (m_ssl_init_state < 0) {
      m_wait_for_ssl_init_cond.wait(
          m_wait_for_ssl_init_mutex.get_native_mutex());
    }
    m_wait_for_ssl_init_mutex.unlock();

    if (!m_ssl_init_state) {
      MYSQL_GCS_LOG_ERROR("Error starting SSL in the group communication"
                          << " engine.")
      m_ssl_init_state = -1;
      goto error;
    }
    m_ssl_init_state = -1;
  } else {
    MYSQL_GCS_LOG_INFO("SSL was not enabled");
  }

  return false;

error:
  /* purecov: begin deadcode */
  assert(xcom_proxy != NULL);
  assert(gcs_engine != NULL);
  xcom_proxy->xcom_set_ssl_mode(0);      /* SSL_DISABLED */
  xcom_proxy->xcom_set_ssl_fips_mode(0); /* SSL_FIPS_MODE_OFF */

  delete m_node_address;
  m_node_address = NULL;

  clear_peer_nodes();

  clean_group_references();

  /*
    If this method created the proxy object it should also
    delete it.
  */
  if (create_proxy) {
    delete xcom_proxy;
    xcom_proxy = NULL;
  }

  /*
    Finalize and delete the processing engine.
  */
  gcs_engine->finalize(NULL);
  delete gcs_engine;
  gcs_engine = NULL;

  return true;
  /* purecov: end */
}

void Gcs_xcom_interface::initialize_peer_nodes(const std::string *peer_nodes) {
  MYSQL_GCS_LOG_DEBUG("Initializing peers")
  std::vector<std::string> processed_peers, invalid_processed_peers;
  Gcs_xcom_utils::process_peer_nodes(peer_nodes, processed_peers);
  Gcs_xcom_utils::validate_peer_nodes(processed_peers, invalid_processed_peers);

  std::vector<std::string>::iterator processed_peers_it;
  for (processed_peers_it = processed_peers.begin();
       processed_peers_it != processed_peers.end(); ++processed_peers_it) {
    m_xcom_peers.push_back(new Gcs_xcom_node_address(*processed_peers_it));

    MYSQL_GCS_LOG_TRACE("::initialize_peer_nodes():: Configured Peer Nodes: %s",
                        (*processed_peers_it).c_str())
  }
}

void Gcs_xcom_interface::clear_peer_nodes() {
  std::vector<Gcs_xcom_node_address *>::iterator it;
  for (it = m_xcom_peers.begin(); it != m_xcom_peers.end(); ++it) delete (*it);

  m_xcom_peers.clear();
}

void Gcs_xcom_interface::set_xcom_group_information(
    const std::string &group_id) {
  Gcs_group_identifier *old_s = NULL;
  Gcs_group_identifier *new_s = new Gcs_group_identifier(group_id);
  u_long xcom_group_id = Gcs_xcom_utils::build_xcom_group_id(*new_s);

  MYSQL_GCS_LOG_TRACE(
      "::set_xcom_group_information():: Configuring XCom "
      "group: XCom Group Id=%lu Name=%s",
      xcom_group_id, group_id.c_str())

  if ((old_s = get_xcom_group_information(xcom_group_id)) != NULL) {
    assert(*new_s == *old_s);
    delete new_s;
  } else {
    m_xcom_configured_groups[xcom_group_id] = new_s;
  }
}

Gcs_group_identifier *Gcs_xcom_interface::get_xcom_group_information(
    const u_long xcom_group_id) {
  Gcs_group_identifier *retval = NULL;

  map<u_long, Gcs_group_identifier *>::iterator xcom_configured_groups_finder;

  xcom_configured_groups_finder = m_xcom_configured_groups.find(xcom_group_id);
  if (xcom_configured_groups_finder != m_xcom_configured_groups.end()) {
    retval = xcom_configured_groups_finder->second;
  }

  MYSQL_GCS_LOG_TRACE(
      "::get_xcom_group_information():: Configuring XCom "
      "group: XCom Group Id=%lu Name=%s",
      xcom_group_id, (retval ? retval->get_group_id().c_str() : "NULL"))

  return retval;
}

/* purecov: begin deadcode */
Gcs_xcom_node_address *Gcs_xcom_interface::get_node_address() {
  return m_node_address;
}
/* purecov: end*/

enum_gcs_error Gcs_xcom_interface::configure_message_stages(
    const Gcs_group_identifier &gid) {
  /*
   Define local variables.
   */
  bool error = false;
  Gcs_xcom_communication *comm_if =
      static_cast<Gcs_xcom_communication *>(get_communication_session(gid));
  Gcs_message_pipeline &pipeline = comm_if->get_msg_pipeline();
  bool enabled = false;
  unsigned long long threshold = Gcs_message_stage_lz4::DEFAULT_THRESHOLD;

  /*
   Extract configuration options from the initialization parameters.
   */
  const std::string *sptr =
      m_initialization_parameters.get_parameter("compression");
  if (sptr->compare("on") == 0) {
    threshold = static_cast<unsigned long long>(atoll(
        m_initialization_parameters.get_parameter("compression_threshold")
            ->c_str()));
    MYSQL_GCS_LOG_TRACE(
        "::configure_msg_stages():: Set compression threshold to %llu",
        threshold)
    enabled = true;
  }

  /*
   Create and configure all the stages.
   */
  // clang-format off
  pipeline.cleanup();
  pipeline.register_stage<Gcs_message_stage_lz4>(enabled, threshold);
  error = pipeline.register_pipeline({
    {
      Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION, {
          Gcs_message_stage::stage_code::ST_LZ4
      }
    }
  });
  // clang-format on

  return (error ? GCS_NOK : GCS_OK);
}

enum_gcs_error Gcs_xcom_interface::configure_suspicions_mgr(
    Gcs_interface_parameters &p, Gcs_suspicions_manager *mgr) {
  const std::string *suspicions_timeout_ptr =
      p.get_parameter("suspicions_timeout");
  if (suspicions_timeout_ptr != NULL) {
    mgr->set_timeout_seconds(
        static_cast<unsigned long>(atoi(suspicions_timeout_ptr->c_str())));
    MYSQL_GCS_LOG_TRACE(
        "::configure_suspicions_mgr():: Set suspicions timeout to %s seconds",
        suspicions_timeout_ptr->c_str())
  }

  const std::string *suspicions_processing_period_ptr =
      p.get_parameter("suspicions_processing_period");
  if (suspicions_processing_period_ptr != NULL) {
    mgr->set_period(static_cast<unsigned int>(
        atoi(suspicions_processing_period_ptr->c_str())));
    MYSQL_GCS_LOG_TRACE(
        "::configure_suspicions_mgr():: Set suspicions processing period to %s "
        "seconds",
        suspicions_processing_period_ptr->c_str());
  }
  return GCS_OK;
}

const Gcs_ip_whitelist &Gcs_xcom_interface::get_ip_whitelist() {
  return m_ip_whitelist;
}

void cb_xcom_receive_data(synode_no message_id, node_set nodes, u_int size,
                          char *data) {
  const site_def *site = find_site_def(message_id);

  if (site->nodeno == VOID_NODE_NO) {
    free_node_set(&nodes);
    free(data);
    return;
  }

  Gcs_xcom_nodes *xcom_nodes = new Gcs_xcom_nodes(site, nodes);
  assert(xcom_nodes->is_valid());
  free_node_set(&nodes);

  Gcs_xcom_notification *notification = new Data_notification(
      do_cb_xcom_receive_data, message_id, xcom_nodes, size, data);
  bool scheduled = gcs_engine->push(notification);
  if (!scheduled) {
    MYSQL_GCS_LOG_DEBUG(
        "Tried to enqueue a message but the member is about to stop.")
    free(data);
    delete xcom_nodes;
    delete notification;
  } else {
    MYSQL_GCS_LOG_TRACE("Scheduled message notification: %p", notification)
  }
}

void do_cb_xcom_receive_data(synode_no message_id, Gcs_xcom_nodes *xcom_nodes,
                             u_int size, char *data) {
  if (size == 0) {
    MYSQL_GCS_LOG_ERROR("Rejecting this received message because it has"
                        << " size zero.")
    delete xcom_nodes;
    return;
  }

  Gcs_internal_message_header hd;
  Gcs_communication_interface *comm_if = NULL;
  Gcs_packet p(reinterpret_cast<unsigned char *>(data), size);

  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());

  Gcs_group_identifier *destination =
      intf->get_xcom_group_information(message_id.group_id);

  if (destination == NULL) {
    // It means that the group is not configured at all...
    MYSQL_GCS_LOG_WARN("Rejecting this message. Group still not configured.")
    p.free_buffer();
    delete xcom_nodes;
    return;
  }

  Gcs_xcom_control *xcom_control_if =
      static_cast<Gcs_xcom_control *>(intf->get_control_session(*destination));

  /*
    The request has been queued but the XCOM's thread has been shut
    down so the message will be simply discarded.
  */
  if (!xcom_control_if->is_xcom_running()) {
    MYSQL_GCS_LOG_DEBUG(
        "Rejecting this message. The group communication engine has already "
        "stopped.")
    p.free_buffer();
    delete xcom_nodes;
    return;
  }

  /*
    This information is used to synchronize the reception of data and global
    view messages. Note that a node can get messages without receiving any
    global view message and we have to discard them at this level. This may
    happen because global view messages are delivered periodically after
    communication channels have been established.

    When a global view message is received and can be successfuly processed,
    the node can start receiving data messages. Note though that it does not
    mean that the application has received a view change notification and
    and can start receiving data messages. Recall that the state exchange
    must be executed before the application can install a view and only
    after that it can start receiving data messages.

    It is important to clean up the last_config_id when the node leaves or
    joins the cluster otherwise, it may receive messages before a global
    view message is delivered.
  */
  if (last_config_id.group_id == 0) {
    MYSQL_GCS_LOG_DEBUG(
        "Rejecting this message. The member is not in a view yet.")
    p.free_buffer();
    delete xcom_nodes;
    return;
  }

  MYSQL_GCS_LOG_TRACE(
      "xcom_receive_data_internal:: xcom_receive_data My node_id is %d "
      "message_id.group= %u message_id.msgno= %llu message_id.node= %d",
      xcom_nodes->get_node_no(), message_id.group_id,
      static_cast<long long unsigned>(message_id.msgno), message_id.node)

  comm_if = intf->get_communication_session(*destination);
  Gcs_message_pipeline &pipeline =
      static_cast<Gcs_xcom_communication *>(comm_if)->get_msg_pipeline();

  if (hd.decode(p.get_buffer())) {
    p.free_buffer();
    delete xcom_nodes;
    return;
  }

  if (pipeline.incoming(p)) {
    MYSQL_GCS_LOG_ERROR(
        "Rejecting message since it wasn't processed correctly in the "
        "pipeline.")
    p.free_buffer();
    delete xcom_nodes;
    return;
  }

  // Build a gcs_message from the arriving data...
  MYSQL_GCS_TRACE_EXECUTE(
      if (hd.get_cargo_type() ==
          Gcs_internal_message_header::cargo_type::CT_INTERNAL_STATE_EXCHANGE) {
        MYSQL_GCS_LOG_TRACE(
            "Reading message that carries exchangeable data: (header, "
            "payload)= %llu",
            p.get_payload_length());
      });
  Gcs_message_data *message_data = new Gcs_message_data(p.get_payload_length());
  if (message_data->decode(p.get_payload(), p.get_payload_length())) {
    p.free_buffer();
    delete xcom_nodes;
    delete message_data;
    MYSQL_GCS_LOG_WARN("Discarding message. Unable to decode it.");
    return;
  }
  p.free_buffer();

  const Gcs_xcom_node_information *node = xcom_nodes->get_node(message_id.node);
  Gcs_member_identifier origin(node->get_member_id());
  Gcs_message *message = new Gcs_message(origin, *destination, message_data);

  /*
    Test if this is a Control message, meaning that is a message sent
    internally by the binding implementation itself, such as State Exchange
    messages.

    If so, then break the execution here, since this message shall not be
    delivered to any registered listeners.
  */
  if (hd.get_cargo_type() ==
      Gcs_internal_message_header::cargo_type::CT_INTERNAL_STATE_EXCHANGE) {
    xcom_control_if->process_control_message(message, hd.get_version());
    delete xcom_nodes;
    return;
  }

  Gcs_xcom_communication_interface *xcom_comm_if =
      static_cast<Gcs_xcom_communication_interface *>(comm_if);

  xcom_comm_if->xcom_receive_data(message);

  delete xcom_nodes;
}

void cb_xcom_receive_global_view(synode_no config_id, synode_no message_id,
                                 node_set nodes) {
  const site_def *site = find_site_def(message_id);

  if (site->nodeno == VOID_NODE_NO) {
    free_node_set(&nodes);
    return;
  }

  Gcs_xcom_nodes *xcom_nodes = new Gcs_xcom_nodes(site, nodes);
  assert(xcom_nodes->is_valid());
  free_node_set(&nodes);

  Gcs_xcom_notification *notification = new Global_view_notification(
      do_cb_xcom_receive_global_view, config_id, message_id, xcom_nodes);
  bool scheduled = gcs_engine->push(notification);
  if (!scheduled) {
    MYSQL_GCS_LOG_DEBUG(
        "Tried to enqueue a global view but the member is about to stop.")
    delete xcom_nodes;
    delete notification;
  } else {
    MYSQL_GCS_LOG_TRACE("Scheduled global view notification: %p", notification)
  }
}

static bool same_members(Gcs_xcom_control &xcom_control,
                         Gcs_xcom_nodes const *const xcom_nodes) {
  bool result = false;
  Gcs_view const *const current_view = xcom_control.get_current_view();
  if (current_view != nullptr &&
      current_view->get_members().size() == xcom_nodes->get_size()) {
    result = true;
    for (auto const &node : xcom_nodes->get_nodes()) {
      result = result &&
               current_view->has_member(node.get_member_id().get_member_id());
    }
  }
  delete current_view;
  return result;
}

void do_cb_xcom_receive_global_view(synode_no config_id, synode_no message_id,
                                    Gcs_xcom_nodes *xcom_nodes) {
  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());

  Gcs_group_identifier *destination =
      intf->get_xcom_group_information(message_id.group_id);

  if (destination == NULL) {
    // It means that the group is not configured at all...
    MYSQL_GCS_LOG_WARN("Rejecting this view. Group still not configured.")
    delete xcom_nodes;
    return;
  }

  Gcs_xcom_control *xcom_control_if =
      static_cast<Gcs_xcom_control *>(intf->get_control_session(*destination));

  /*
    The request has been queued but the XCOM's thread has been shut
    down so the view will be simply discarded.
  */
  if (!xcom_control_if->is_xcom_running()) {
    MYSQL_GCS_LOG_DEBUG(
        "Rejecting this view. The group communication engine has "
        "already stopped.")
    delete xcom_nodes;
    return;
  }

  MYSQL_GCS_TRACE_EXECUTE(
      unsigned int node_no = xcom_nodes->get_node_no();
      const std::vector<Gcs_xcom_node_information> &nodes =
          xcom_nodes->get_nodes();
      std::vector<Gcs_xcom_node_information>::const_iterator nodes_it;
      MYSQL_GCS_LOG_TRACE(
          "Received global view: My node_id is %d "
          "config_id.group= %u config_id.msgno= %llu config_id.node=%d "
          "message_id.group= %u message_id.msgno= %llu message_id.node= %d",
          node_no, config_id.group_id,
          static_cast<long long unsigned>(config_id.msgno), config_id.node,
          message_id.group_id,
          static_cast<long long unsigned>(message_id.msgno), message_id.node)

          MYSQL_GCS_LOG_TRACE("Received global view: node set:")

              for (nodes_it = nodes.begin(); nodes_it != nodes.end();
                   ++nodes_it){MYSQL_GCS_LOG_TRACE(
                  "My node_id is %d peer_ %d address: %s flag: %s", node_no,
                  (*nodes_it).get_node_no(),
                  (*nodes_it).get_member_id().get_member_id().c_str(),
                  ((*nodes_it).is_alive() ? "Active" : "Failed"))})

  /*
    If the global view message is not rejected, this means that the node
    can receive data messages and this fact is registered by recording
    the configuration.

    This means that the state exchange phase has started as well and the
    node should process messages accordingly.

    A global view message may be rejected if the installed configuration
    is the same or there are new members marked as faulty. The controller
    must be called because it is responsible for removing from the system
    those members that are faulty.

    same_view = true prevents
    control_if->xcom_receive_global_view(message_id, xcom_nodes, same_view)
    from firing a new view upwards.
    We want to filter out a view if:

    a) we have already processed it, or
    b) it is due to an event horizon reconfiguration.

    We identify situation (a) by comparing the incoming view's config synod
    against the config synod of the last delivered view.
    We conclude we are in situation (b) if the incoming view's membership is
    the same as the last delivered view.
  */
  // Situation (a).
  bool const already_processed =
      (last_config_id.group_id != 0 && synode_eq(last_config_id, config_id));
  // Situation (b).
  bool const same_nodes = same_members(*xcom_control_if, xcom_nodes);

  bool const same_view = already_processed || same_nodes;

  MYSQL_GCS_TRACE_EXECUTE(if (same_view) {
    if (already_processed) {
      MYSQL_GCS_LOG_TRACE(
          "Received a global view we already processed: { group=%" PRIu32
          " msgno=%" PRIu64 " node=%" PRIu32 " }",
          config_id.group_id, config_id.msgno, config_id.node);
    } else {
      MYSQL_GCS_LOG_TRACE(
          "Received a global view due to an event horizon reconfiguration. The "
          "members are the same");
    }
  });

  if (!(xcom_control_if->xcom_receive_global_view(message_id, xcom_nodes,
                                                  same_view))) {
    // Copy node set and config id if the view is not rejected...
    last_config_id.group_id = config_id.group_id;
    last_config_id.msgno = config_id.msgno;
    last_config_id.node = config_id.node;
  } else {
    MYSQL_GCS_LOG_TRACE("View rejected by handler. My node_id is %d",
                        message_id.node)
  }

  delete xcom_nodes;
}

int cb_xcom_match_port(xcom_port if_port) { return xcom_local_port == if_port; }

void cb_xcom_receive_local_view(synode_no message_id, node_set nodes) {
  const site_def *site = find_site_def(message_id);

  if (site->nodeno == VOID_NODE_NO) {
    free_node_set(&nodes);
    return;
  }

  Gcs_xcom_nodes *xcom_nodes = new Gcs_xcom_nodes(site, nodes);
  assert(xcom_nodes->is_valid());
  free_node_set(&nodes);

  Gcs_xcom_notification *notification = new Local_view_notification(
      do_cb_xcom_receive_local_view, message_id, xcom_nodes);
  bool scheduled = gcs_engine->push(notification);
  if (!scheduled) {
    MYSQL_GCS_LOG_DEBUG(
        "Tried to enqueue a local view but the member is about to stop.")
    delete xcom_nodes;
    delete notification;
  } else {
    MYSQL_GCS_LOG_TRACE("Scheduled local view notification: %p", notification)
  }
}

void do_cb_xcom_receive_local_view(synode_no message_id,
                                   Gcs_xcom_nodes *xcom_nodes) {
  Gcs_xcom_interface *gcs = NULL;
  Gcs_control_interface *ctrl = NULL;
  Gcs_xcom_control *xcom_ctrl = NULL;
  Gcs_group_identifier *destination = NULL;

  if (!(gcs = static_cast<Gcs_xcom_interface *>(
            Gcs_xcom_interface::get_interface())))
    goto end;  // ignore this local view

  if (!(destination = gcs->get_xcom_group_information(message_id.group_id))) {
    MYSQL_GCS_LOG_WARN("Rejecting this view. Group still not configured.")
    goto end;  // ignore this local view
  }

  if (!(ctrl = gcs->get_control_session(*destination)))
    goto end;  // ignore this local view

  xcom_ctrl = static_cast<Gcs_xcom_control *>(ctrl);
  if (!xcom_ctrl->is_xcom_running()) {
    MYSQL_GCS_LOG_DEBUG(
        "Rejecting this view. The group communnication engine has "
        "already stopped.")
    goto end;  // ignore this local view
  }

  xcom_ctrl->xcom_receive_local_view(xcom_nodes);

end:
  delete xcom_nodes;
}

void cb_xcom_handle_app_snap(blob *gcs_snap MY_ATTRIBUTE((unused))) {}

synode_no cb_xcom_get_app_snap(blob *gcs_snap MY_ATTRIBUTE((unused))) {
  return null_synode;
}

int cb_xcom_get_should_exit() {
  if (xcom_proxy)
    return (int)xcom_proxy->get_should_exit();
  else
    return 0;
}

void cb_xcom_ready(int status MY_ATTRIBUTE((unused))) {
  if (xcom_proxy) xcom_proxy->xcom_signal_ready();
}

void cb_xcom_comms(int status) {
  if (xcom_proxy) xcom_proxy->xcom_signal_comms_status_changed(status);
}

void cb_xcom_exit(int status MY_ATTRIBUTE((unused))) {
  last_config_id.group_id = 0;
  if (xcom_proxy) xcom_proxy->xcom_signal_exit();
}

/**
  Callback function used by XCom to signal that a 'die_op' has been received,
  causing XCom to terminate.
 */
void cb_xcom_fatal_error(int status MY_ATTRIBUTE((unused))) {
  MYSQL_GCS_LOG_FATAL(
      "The node has missed messages that can no longer be "
      "recovered from the other nodes' caches. GCS will now terminate.");
  Gcs_xcom_interface *iface =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());
  iface->finalize_gcs_on_error();
}

/**
  Callback function used by XCOM to write information, warning and error
  messages in coordination with the GCS layer.
*/

void cb_xcom_logger(const int64_t level, const char *message) {
  std::stringstream log;

  log << GCS_PREFIX << message;

  Gcs_log_manager::get_logger()->log_event(static_cast<gcs_log_level_t>(level),
                                           log.str().c_str());
}

/**
  Callback function used by XCOM to write debug messages in coordination with
  the GCS layer.

  The variadic signature allows XCOM to call this function without forcing
  any preliminary processing on the message, meaning that the cost is minimal
  and there is no need to use an intermediate buffer. The GCS layer will then
  be responsible for writing the message content directly to the final buffer.
*/

void cb_xcom_debugger(const char *format, ...) {
  va_list args;
  va_start(args, format);
  Gcs_default_debugger *debugger = Gcs_debug_manager::get_debugger();
  debugger->log_event(format, args);
  va_end(args);
}

/**
  Callback function used by XCOM to check whether a debug module is enabled
  or not.
*/

int cb_xcom_debugger_check(const int64_t options) {
  return Gcs_debug_manager::test_debug_options(options);
}

/**
  Callback function used by XCOM to check whether an incoming connection
  should be accepted or not.
*/

int cb_xcom_socket_accept(int fd, site_def const *xcom_config) {
  Gcs_xcom_interface *intf =
      static_cast<Gcs_xcom_interface *>(Gcs_xcom_interface::get_interface());

  const Gcs_ip_whitelist &wl = intf->get_ip_whitelist();

  return wl.shall_block(fd, xcom_config) ? 0 : 1;
}
