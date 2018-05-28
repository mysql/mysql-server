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

#ifndef GCS_XCOM_INTERFACE_INCLUDED
#define GCS_XCOM_INTERFACE_INCLUDED

/*
  This file is the main entry point for the gcs_interface implementation for
  the XCom Group communication library.
*/

#include <cstdlib>
#include <ctime>
#include <map>
#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_psi.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_cond.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_mutex.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_thread.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_control_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_management.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"

class Gcs_suspicions_manager;

/**
  Struct that holds instances of this binding interface implementations.
*/
typedef struct xcom_group_interfaces {
  Gcs_control_interface *control_interface;
  Gcs_communication_interface *communication_interface;
  Gcs_statistics_interface *statistics_interface;
  Gcs_group_management_interface *management_interface;

  /*
    Additional storage of group interface auxiliary structures for later
    deletion.
  */
  Gcs_xcom_view_change_control_interface *vce;
  Gcs_xcom_state_exchange_interface *se;

} gcs_xcom_group_interfaces;

/**
  Implementation of the Gcs_interface for the XCom binding.
*/
class Gcs_xcom_interface : public Gcs_interface {
 private:
  // XCom single instance
  static Gcs_interface *interface_reference_singleton;

  /**
    XCom binding private constructor.
  */
  explicit Gcs_xcom_interface();

 public:
  /**
    Since one wants that a single instance exists, the interface implementation
    shall be retrieved via a Singleton pattern.

    This is the public method that allows the retrieving of the single
    instance.

    @return a reference to the Singleton instance
  */

  static Gcs_interface *get_interface();

  /**
    Public method that finalizes and cleans the singleton.
  */

  static void cleanup();

  /**
    Public method that cleans thread-local resources related to communication.
    Required when SSL is provided by OpenSSL.
  */

  static void cleanup_thread_ssl_resources();

  virtual ~Gcs_xcom_interface();

  /**
    This block implements the virtual methods defined in Gcs_interface.
  */

  enum_gcs_error initialize(const Gcs_interface_parameters &interface_params);

  enum_gcs_error configure(const Gcs_interface_parameters &interface_params);

  bool is_initialized();

  enum_gcs_error finalize();

  Gcs_control_interface *get_control_session(
      const Gcs_group_identifier &group_identifier);

  Gcs_communication_interface *get_communication_session(
      const Gcs_group_identifier &group_identifier);

  Gcs_statistics_interface *get_statistics(
      const Gcs_group_identifier &group_identifier);

  Gcs_group_management_interface *get_management_session(
      const Gcs_group_identifier &group_identifier);

  enum_gcs_error configure_message_stages(const Gcs_group_identifier &gid);

  enum_gcs_error configure_suspicions_mgr(Gcs_interface_parameters &p,
                                          Gcs_suspicions_manager *mgr);

  enum_gcs_error set_logger(Logger_interface *logger);

  void set_xcom_group_information(const std::string &group_id);

  Gcs_group_identifier *get_xcom_group_information(const u_long group_id);

  Gcs_xcom_node_address *get_node_address();

  /**
   This member function shall return the set of parameters that configure
   the interface at the time its initialization was done. The parameters
   returned already contain default values set as well as values that may
   have been fixed.

   @return The parameters configured at the time the interface was initialized.
   */
  const Gcs_interface_parameters &get_initialization_parameters() {
    return m_initialization_parameters;
  }

  /**
    Must return the white list.

    @return the list of whitelisted IP addresses and subnet masks.
   */
  const Gcs_ip_whitelist &get_ip_whitelist();

  /*
     Notify all controllers that XCOM's thread has finished.
  */

  void process_xcom_exit();

  /**
    Contains all the code needed to stop the xcom daemon if it was not
    already stopped as it should have been done.
  */

  void finalize_xcom();

  /**
    Used to initialize SSL assuming that the necessary parameters have already
    been read.
  */
  void initialize_ssl();

 private:
  /**
    Method to initialize the logging and debugging systems. If something
    bad happens, an error is returned.


    @param[in] debug_file File where the debug information on GCS will
                          be stored to
    @param[in] debug_path Default path where the debug information on GCS
                          will be stored to
  */

  enum_gcs_error initialize_logging(const std::string *debug_file,
                                    const std::string *debug_path);

  /**
    Method to finalize the logging and debugging systems. If something
    bad happens, an error is returned.
  */

  enum_gcs_error finalize_logging();

  /**
    Internal helper method that retrieves all group interfaces for a certain
    group.

    @note Since the group interfaces work as a singleton, meaning that a group
    has a single set of interfaces built, this method will also implement the
    behavior to build and initialize the interfaces implementation.

    @param[in] group_identifier the group in which one wants to instantiate the
           interface implementation

    @return a reference to a struct gcs_xcom_group_interfaces
  */

  gcs_xcom_group_interfaces *get_group_interfaces(
      const Gcs_group_identifier &group_identifier);

  /**
    Contains all the code needed to initialize a connection to the xcom
    daemon.

    @return true in case of error
  */

  bool initialize_xcom(const Gcs_interface_parameters &interface_params);

  /**
    Internal helper method to delete all previously created group interfaces.
  */

  void clean_group_interfaces();

  /**
    Internal helper method to delete all previously created group references.
  */

  void clean_group_references();

  /**
    Helper used to parse the peer_nodes parameter and to initialize XCom peer
    nodes.

    @param[in] peer_nodes received parameter with the addresses of all peer
      nodes
  */

  void initialize_peer_nodes(const std::string *peer_nodes);

  /**
    Helper used to delete the existing XCom peer nodes in m_xcom_peers and to
    clear that vector.
  */

  void clear_peer_nodes();

  // Holder to the created group interfaces, in which the key is the group
  std::map<std::string, gcs_xcom_group_interfaces *> m_group_interfaces;

  std::map<u_long, Gcs_group_identifier *> m_xcom_configured_groups;

  /*
    The address associated with the current node.
  */
  Gcs_xcom_node_address *m_node_address;

  /*
    The addresses associated with current node's peers.
  */
  std::vector<Gcs_xcom_node_address *> m_xcom_peers;

  // States if this interface is initialized
  bool m_is_initialized;

  bool m_boot;

  My_xp_socket_util *m_socket_util;

  /**
    The C++ interface to setup and configure xcom properties
    from GCS. Under the hood, this changes the C structure that
    holds the configuration for XCom.

    (As XCom moves into C++, we can replace XCom's internal
    structure with a similar object and remove it from this
    place.)
   */
  Gcs_xcom_app_cfg m_gcs_xcom_app_cfg;

  /**
   The initialization parameters provided through the initialize member
   function.
   */
  Gcs_interface_parameters m_initialization_parameters;

  // Store pointer to default sink
  Gcs_async_buffer *m_default_sink;

  // Store pointer to default logger
  Logger_interface *m_default_logger;

  // Store pointer to default debugger
  Gcs_default_debugger *m_default_debugger;

  /**
   The IP whitelist.
   */
  Gcs_ip_whitelist m_ip_whitelist;

  /**
    Indicates whether SSL has been initialized and if that initialization was
    successfull.
  */
  int m_ssl_init_state;

  /// protects the m_ssl_init_state thread shared variable
  My_xp_cond_impl m_wait_for_ssl_init_cond;
  My_xp_mutex_impl m_wait_for_ssl_init_mutex;

 private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_interface(Gcs_xcom_interface const &);
  Gcs_xcom_interface &operator=(Gcs_xcom_interface const &);
};

#ifdef __cplusplus
extern "C" {
#endif
int cb_xcom_match_port(xcom_port if_port);
#ifdef __cplusplus
}
#endif

#endif /* GCS_XCOM_INTERFACE_INCLUDED */
