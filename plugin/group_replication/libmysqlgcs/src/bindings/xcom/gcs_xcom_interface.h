/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_manager.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_statistics_storage_impl.h"

/**
 * Keep track of the most recent XCom configuration the node will deliver
 * upwards.
 */
class Gcs_xcom_config {
 public:
  Gcs_xcom_config();
  /**
   * Resets the object to its initial state.
   */
  void reset();
  /**
   * Updates this configuration's information.
   *
   * @param config_id the synod of the configuration
   * @param xcom_nodes the XCom configuration's membership
   * @param event_horizon the XCom configuration's event horizon
   */
  void update(synode_no config_id, Gcs_xcom_nodes const &xcom_nodes,
              xcom_event_horizon event_horizon);
  /**
   * Checks whether this configuration pertains to a received XCom view, i.e.
   * the configuration object is not in its initial state.
   *
   * @returns true if the configuration pertains to a received XCom view, false
   * otherwise
   */
  bool has_view() const;
  /**
   * Checks whether this configuration's synod matches the the given synod.
   *
   * @param config_id the synod to compare against
   * @returns true if the synods are the same, false otherwise
   */
  bool same_view(synode_no config_id) const;
  /**
   * Checks whether this configuration's membership matches the given
   * membership.
   *
   * @param xcom_nodes the membership to compare against
   * @returns true if the memberships are the same, false otherwise
   */
  bool same_xcom_nodes(Gcs_xcom_nodes const &xcom_nodes) const;
  /**
   * Checks whether this configuration's event horizon matches the given event
   * horizon.
   *
   * @param event_horizon the event horizon to compare against
   * @returns true if the event horizons are the same, false otherwise
   */
  bool same_event_horizon(xcom_event_horizon const &event_horizon) const;
  /**
   * Checks whether this configuration's membership matches the given
   * membership.
   *
   * @param xcom_nodes the membership to compare against
   * @returns true if the memberships are the same, false otherwise
   */
  bool same_xcom_nodes_v3(Gcs_xcom_nodes const &xcom_nodes) const;
  /*
   * This class will have a singleton object, so we delete the {copy,move}
   * {constructor,assignment}. This way the compiler slaps us on the wrist if we
   * attempt to copy or move the singleton.
   */
  Gcs_xcom_config(Gcs_xcom_config const &) = delete;
  Gcs_xcom_config(Gcs_xcom_config &&) = delete;
  Gcs_xcom_config &operator=(Gcs_xcom_config const &) = delete;
  Gcs_xcom_config &operator=(Gcs_xcom_config &&) = delete;

 private:
  synode_no config_id_;
  Gcs_xcom_nodes xcom_nodes_;
  xcom_event_horizon event_horizon_;
};

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

  ~Gcs_xcom_interface() override;

  /**
    This block implements the virtual methods defined in Gcs_interface.
  */

  enum_gcs_error initialize(
      const Gcs_interface_parameters &interface_params) override;

  enum_gcs_error configure(
      const Gcs_interface_parameters &interface_params) override;

  bool is_initialized() override;

  enum_gcs_error finalize() override;

  Gcs_control_interface *get_control_session(
      const Gcs_group_identifier &group_identifier) override;

  Gcs_communication_interface *get_communication_session(
      const Gcs_group_identifier &group_identifier) override;

  Gcs_statistics_interface *get_statistics(
      const Gcs_group_identifier &group_identifier) override;

  Gcs_group_management_interface *get_management_session(
      const Gcs_group_identifier &group_identifier) override;

  enum_gcs_error configure_message_stages(const Gcs_group_identifier &gid);

  enum_gcs_error configure_suspicions_mgr(Gcs_interface_parameters &p,
                                          Gcs_suspicions_manager *mgr);

  enum_gcs_error set_logger(Logger_interface *logger) override;

  void set_xcom_group_information(const std::string &group_id);

  Gcs_group_identifier *get_xcom_group_information(const u_long group_id);

  Gcs_xcom_node_address *get_node_address();

  void set_node_address(std::string const &address);

  /**
   * @see Gcs_interface#setup_runtime_resources
   */
  enum_gcs_error setup_runtime_resources(
      Gcs_interface_runtime_requirements &reqs) override;

  /**
   * @see Gcs_interface#cleanup_runtime_resources
   */
  enum_gcs_error cleanup_runtime_resources(
      Gcs_interface_runtime_requirements &reqs) override;

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
    Must return the allowlist.

    @return the list of allowlisted IP addresses and subnet masks.
   */
  Gcs_ip_allowlist &get_ip_allowlist();

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
    Makes GCS leave the group when an error has caused XCom to terminate
    unexpectedly.
   */
  void make_gcs_leave_group_on_error();

  /**
    Used to initialize SSL assuming that the necessary parameters have already
    been read.
  */
  void initialize_ssl();

  /**
   Used to initialize the unique identifier of the XCom instance.

   @param node_information Information about the XCom node
   @param xcom_proxy XCom proxy
   @retval true if there was an error initialising the XCom identity
   @retval false if operation was successful
   */
  bool set_xcom_identity(Gcs_xcom_node_information const &node_information,
                         Gcs_xcom_proxy &xcom_proxy);

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

  /**
   * @brief Announces that a finalize was called to all group instances that
   *        use a Gcs_xcom_view_change_control_interface. The purpose of this
   *        is to end any ongoing tasks, like pending joins.
   */
  void announce_finalize_to_view_control();

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
   The IP allowlist.
   */
  Gcs_ip_allowlist m_ip_allowlist;

  /**
    Indicates whether SSL has been initialized and if that initialization was
    successful.
  */
  int m_ssl_init_state;

  /// protects the m_ssl_init_state thread shared variable
  My_xp_cond_impl m_wait_for_ssl_init_cond;
  My_xp_mutex_impl m_wait_for_ssl_init_mutex;

  /**
   Network namespace service provider
   */
  Network_namespace_manager *m_netns_manager;

  /**
   Interface for statistic storage
   */
  Gcs_xcom_statistics_manager_interface *m_stats_mgr;

  /**
   Interface for XCom statistic storage
   */
  Gcs_xcom_statistics_storage_impl *m_xcom_stats_storage;

 private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_interface(Gcs_xcom_interface const &);
  Gcs_xcom_interface &operator=(Gcs_xcom_interface const &);
};

int cb_xcom_match_port(xcom_port if_port);

#endif /* GCS_XCOM_INTERFACE_INCLUDED */
