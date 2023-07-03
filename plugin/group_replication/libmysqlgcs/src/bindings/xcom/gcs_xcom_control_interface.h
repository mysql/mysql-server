/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_CONTROL_INTERFACE_INCLUDED
#define GCS_XCOM_CONTROL_INTERFACE_INCLUDED

#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_psi.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_thread.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_expels_in_progress.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_management.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_notification.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/include/network_management_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

class Gcs_xcom_control;

/**
  @class Gcs_suspicions_manager

  This class stores all node suspicions, as well as the timeout and period
  parameters used by the thread that processes the suspicions.
  Suspicions are added and removed upon reception of a new global view.
*/
class Gcs_suspicions_manager {
 public:
  /**
    Constructor for Gcs_suspicions_manager, which sets m_proxy with the
    received pointer parameter.
    @param[in] proxy Pointer to Gcs_xcom_proxy
    @param[in] ctrl Pointer to Gcs_xcom_control
  */

  explicit Gcs_suspicions_manager(Gcs_xcom_proxy *proxy,
                                  Gcs_xcom_control *ctrl);

  /**
    Destructor for Gcs_suspicions_manager.
  */

  ~Gcs_suspicions_manager();

  /**
    Invoked by Gcs_xcom_control::xcom_receive_global_view, it invokes the
    remove_suspicions method for the alive_nodes and left_nodes parameters,
    if they're not empty, neither m_suspicions. It also invokes the
    add_suspicions method if the non_member_suspect_nodes and
    member_suspect_nodes parameter aren't empty.

    @param[in] config_id Configuration ID of the subsequent node information
    @param[in] xcom_nodes List of all nodes (i.e. alive or dead) with low level
                          information such as timestamp, unique identifier, etc
    @param[in] alive_nodes List of the nodes that currently belong to the group
    @param[in] left_nodes List of the nodes that have left the group
    @param[in] non_member_suspect_nodes List of joining nodes to add to
                                        m_suspicions
    @param[in] member_suspect_nodes List of previously active nodes to add to
                                    m_suspicions
    @param[in] is_killer_node Indicates if node should remove suspect members
                              from the group
    @param[in] max_synode XCom max synode
  */

  void process_view(
      synode_no const config_id, Gcs_xcom_nodes *xcom_nodes,
      std::vector<Gcs_member_identifier *> alive_nodes,
      std::vector<Gcs_member_identifier *> left_nodes,
      std::vector<Gcs_member_identifier *> member_suspect_nodes,
      std::vector<Gcs_member_identifier *> non_member_suspect_nodes,
      bool is_killer_node, synode_no max_synode);

  /**
    Invoked periodically by the suspicions processing thread, it picks a
    timestamp and verifies which suspect nodes should be removed as they
    have timed out.
  */

  void process_suspicions();

  /**
    Clear all suspicions. Invoked when node is leaving the group.
  */

  void clear_suspicions();

  /**
    Invoked periodically by the suspicions processing thread, it picks a
    timestamp and verifies which suspect nodes should be removed as they
    have timed out.

    @param[in] lock Whether lock should be acquired or not
  */

  void run_process_suspicions(bool lock);

  /**
    Retrieves current list of suspicions.
  */

  const Gcs_xcom_nodes &get_suspicions() const;

  /**
    Retrieves suspicion thread period in seconds.
  */

  unsigned int get_suspicions_processing_period();

  /**
    Sets the period or sleep time, between iterations, for the suspicion
    thread.
    @param[in] sec Suspicion thread period
  */

  void set_suspicions_processing_period(unsigned int sec);

  /**
    Retrieves non-member expel timeout in 100s of nanoseconds.
    @return Non-member expel timeout
  */

  uint64_t get_non_member_expel_timeout();

  /**
    Sets the time interval to wait before removing non-member nodes marked to
    be expelled from the cluster.
    @param[in] sec Suspicions timeout in seconds
  */

  void set_non_member_expel_timeout_seconds(unsigned long sec);

  /**
    Retrieves member expel timeout in 100s of nanoseconds.
    @return Member expel timeout
  */

  uint64_t get_member_expel_timeout();

  /**
    Sets the time interval to wait before removing member nodes marked to be
    expelled from the cluster.
    @param[in] sec Expel suspicions timeout in seconds
  */

  void set_member_expel_timeout_seconds(unsigned long sec);

  /**
    Sets the hash for the current group identifier.
    @param[in] gid_h Group ID hash
  */

  void set_groupid_hash(unsigned int gid_h);

  /**
    Sets the information for this node
    @param[in] node_info Information on this node
  */

  void set_my_info(Gcs_xcom_node_information *node_info);

  /**
    Auxiliary method to wake the suspicions processing thread and set if it
    should terminate or not.
    @param[in] terminate Signals if thread should terminate
  */

  void wake_suspicions_processing_thread(bool terminate);

  /**
    Auxiliary method to inform the suspicions manager that this node is in
    a group with the majority of the configured nodes.
    @param[in] majority Signals if the group has the majority of the nodes
                        alive
  */

  void inform_on_majority(bool majority);

  /**
    Auxiliary method to retrieve if the suspicions manager has the majority
    enabled.
    @return majority
  */
  bool has_majority();

  /*
    Updates the synode_no of the last message removed from the XCom cache.

    @param[in] last_removed The synode_no of the last message removed from the
                            cache.
  */
  void update_last_removed(synode_no last_removed);

 private:
  /**
    Invoked by Gcs_suspicions_manager::process_view, it verifies if any
    of the nodes in the received list was a suspect and removes it from
    m_suspicions.
    @param[in] nodes List of nodes to remove from m_suspicions
  */

  void remove_suspicions(std::vector<Gcs_member_identifier *> nodes);

  /**
    Invoked by Gcs_suspicions_manager::process_view, it adds suspicions
    for the nodes received as argument if they aren't already suspects.

    @param[in] xcom_nodes List of all nodes (i.e. alive or dead) with low level
                          information such as timestamp, unique identifier, etc
    @param[in] non_member_suspect_nodes List of joining nodes to add to
                                        m_suspicions
    @param[in] member_suspect_nodes List of previously active nodes to add to
                                    m_suspicions
    @param[in] max_synode XCom max synode
    @return Indicates if new suspicions were added
  */

  bool add_suspicions(
      Gcs_xcom_nodes *xcom_nodes,
      std::vector<Gcs_member_identifier *> non_member_suspect_nodes,
      std::vector<Gcs_member_identifier *> member_suspect_nodes,
      synode_no max_synode);

  /*
    XCom proxy pointer
  */
  Gcs_xcom_proxy *m_proxy;

  /*
    XCom control interface pointer
  */
  Gcs_xcom_control *m_control_if;

  /*
    Suspicions processing thread period in seconds
  */
  unsigned int m_suspicions_processing_period;

  /*
    Non-member expel timeout stored in 100s of nanoseconds
  */
  uint64_t m_non_member_expel_timeout;

  /*
    Member expel timeout stored in 100s of nanoseconds
  */
  uint64_t m_member_expel_timeout;

  /*
   Group ID hash
  */
  unsigned int m_gid_hash;

  /*
    List of suspicions
  */
  Gcs_xcom_nodes m_suspicions;

  /*
    Mutex to control access to m_suspicions
  */
  My_xp_mutex_impl m_suspicions_mutex;

  /*
    Condition used to wake up suspicions thread
  */
  My_xp_cond_impl m_suspicions_cond;

  /*
    Mutex to control access to suspicions parameters
  */
  My_xp_mutex_impl m_suspicions_parameters_mutex;

  /*
    Signals if node should remove suspect nodes from group.
  */
  bool m_is_killer_node;

  /*
    Pointer to this node's information
  */
  Gcs_xcom_node_information *m_my_info;

  /*
    Signals if group has a majority of alive nodes.
  */
  bool m_has_majority;

  /*
    The synode_no of the last message removed from the XCom cache.
    The suspicions manager will use this to verify if a suspected node has
    gone too far behind the group to be recoverable; when that happens, it
    will print a warning message.
  */
  synode_no m_cache_last_removed;

  /*
    The set of expels we have issued but that have not yet taken effect.
  */
  Gcs_xcom_expels_in_progress m_expels_in_progress;

  /*
    The XCom configuration/membership ID of the last view we processed.
  */
  synode_no m_config_id;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_suspicions_manager(Gcs_suspicions_manager const &);
  Gcs_suspicions_manager &operator=(Gcs_suspicions_manager const &);
};

/**
  @class Gcs_xcom_control_interface

  This class implements the generic Gcs_control_interface. It relates with:
  - Gcs_xcom_interface, since the view_changed registered callback will
     delegate its calls to an instance of this class.
  - Gcs_xcom_control_proxy in order to isolate xcom calls from their
     actual implementation, to allow unit testing
  - Gcs_xcom_view_change_control_interface that implements a structure
     to allow View Safety. This ensures that, while the view installation
     procedure is not finished, all applications are not allowed to execute
     operations based upon a possible inconsistent state.
*/
class Gcs_xcom_control : public Gcs_control_interface {
 public:
  static constexpr int CONNECTION_ATTEMPTS = 10;

  /**
    Gcs_xcom_control_interface constructor.

    @param[in] xcom_node_address Information about the node's address
    @param[in] xcom_peers Information about the nodes that it
                          should get in touch to enter a group

    @param[in] group_identifier Group identifier object
    @param[in] xcom_proxy Proxy implementation reference
    @param[in] xcom_group_management Group management reference
    @param[in] gcs_engine MySQL GCS engine
    @param[in] state_exchange Reference to the State Exchange algorithm
    implementation
    @param[in] view_control View change control interface reference
    @param[in] boot Whether the node will be used to bootstrap the group
    @param[in] socket_util Reference to a socket utility
    @param[in] comms_operation_interface an unique_ptr to a
                                          Network_provider_operations_interface
  */

  explicit Gcs_xcom_control(
      Gcs_xcom_node_address *xcom_node_address,
      std::vector<Gcs_xcom_node_address *> &xcom_peers,
      Gcs_group_identifier group_identifier, Gcs_xcom_proxy *xcom_proxy,
      Gcs_xcom_group_management *xcom_group_management,
      Gcs_xcom_engine *gcs_engine,
      Gcs_xcom_state_exchange_interface *state_exchange,
      Gcs_xcom_view_change_control_interface *view_control, bool boot,
      My_xp_socket_util *socket_util,
      std::unique_ptr<Network_provider_operations_interface>
          comms_operation_interface);

  ~Gcs_xcom_control() override;

  // Gcs_control_interface implementation
  enum_gcs_error join() override;

  enum_gcs_error do_join(const bool retry = true);

  /*
    Responsible for doing the heavy lifting related to the join
    operation.
  */
  enum_gcs_error retry_do_join();

  enum_gcs_error leave() override;

  /*
    Responsible for doing the heavy lifting related to the leave operation.
    Triggers and oversees the termination of XCom, then calls 'do_leave_gcs'.
  */
  enum_gcs_error do_leave();

  /**
    Sends a leave view message to informat that XCOM has already exited or
    is about to do so.
  */
  void do_leave_view();

  /**
    Request other members to remove node from the group.
  */
  void do_remove_node_from_group();

  bool belongs_to_group() override;

  Gcs_view *get_current_view() override;

  const Gcs_member_identifier get_local_member_identifier() const override;

  int add_event_listener(
      const Gcs_control_event_listener &event_listener) override;

  void remove_event_listener(int event_listener_handle) override;

  /**
    The purpose of this method is to be called when in Gcs_xcom_interface
    callback method of View Changing is invoked.

    This allows, in terms of software architecture, to concentrate all the
    view change logic and processing in a single place. The view_change
    callback that is registered in Gcs_xcom_interface should be a simple
    pass-through.

    @param[in] config_id The configuration ID that this view pertains to
    @param[in] message_id the message that conveys the View Change
    @param[in] xcom_nodes Set of nodes that participated in the consensus
                            to deliver the message
    @param[in] do_not_deliver_to_client  Whether to filter this view from being
                                         delivered to the client
    @param[in] max_synode XCom max synode
  */

  bool xcom_receive_global_view(synode_no const config_id, synode_no message_id,
                                Gcs_xcom_nodes *xcom_nodes,
                                bool do_not_deliver_to_client,
                                synode_no max_synode);

  /*
    This method is called in order to give a hint on what the node thinks
    about other nodes.

    The view is ignored if 1) it has no nodes, 2) the local node does not
    have a view installed or 3) the local node is not present in its current
    view (i.e., it has been expelled).

    @param[in] config_id The configuration ID that this view pertains to
    @param[in] xcom_nodes Set of nodes that participated in the consensus
                          to deliver the message
    @param[in] max_synode XCom max synode

    @return   True if the view was processed;
              False otherwise.
  */
  bool xcom_receive_local_view(synode_no const config_id,
                               Gcs_xcom_nodes *xcom_nodes,
                               synode_no max_synode);

  /*
    This method is called in order to inform that the node has left the
    group or is about to do so.
  */
  bool xcom_receive_leave();

  /**
    Process a message from the control interface and if necessary delegate it
    to the state exchange.

    @param[in] msg message
    @param[in] maximum_supported_protocol_version maximum supported protocol
    version
    @param[in] used_protocol_version protocol version in use by control message,
                                i.e. state exchange message
  */

  void process_control_message(
      Gcs_message *msg, Gcs_protocol_version maximum_supported_protocol_version,
      Gcs_protocol_version used_protocol_version);

  std::map<int, const Gcs_control_event_listener &> *get_event_listeners();

  /**
    Return the address associated with the current node.
  */
  Gcs_xcom_node_address *get_node_address();

  /**
    @returns the information about the local membership of this node.
   */
  Gcs_xcom_node_information const &get_node_information() const;

  /**
    Return a pointer to the proxy object used to access XCOM.
  */
  Gcs_xcom_proxy *get_xcom_proxy();

  Gcs_suspicions_manager *get_suspicions_manager();

  // For testing purposes
  void set_boot_node(bool boot);

  /**
    Set the address associated with the current node.
  */
  void set_node_address(Gcs_xcom_node_address *node_address);

  /**
    Inserts in m_initial_peers copies of the Gcs_xcom_node_address
    objects whose addresses are in the xcom_peers vector.

    @param[in] xcom_peers vector with peers' information
  */

  void set_peer_nodes(std::vector<Gcs_xcom_node_address *> &xcom_peers);

  /**
    Deletes all the Gcs_xcom_node_address objects pointed by the
    elements of the m_initial_peers vector, clearing it at the end.
  */

  void clear_peer_nodes();

  /**
    Return a pointer to a socket utility.

    @return a pointer to a socket utility
  */

  My_xp_socket_util *get_socket_util();

  /**
    This member function can be used to wait until xcom thread exits.
  */
  void wait_for_xcom_thread();

  /**
    Whether XCOM's Thread is running or not.
  */
  bool is_xcom_running();

  /*
    Configure how many times the node will try to join a group.

    @param[in] join_attempts number of attempts to join
    @param[in] join_sleep_time time between attempts to join
  */
  void set_join_behavior(unsigned int join_attempts,
                         unsigned int join_sleep_time);

  /**
    Sets a new value for the maximum size of the XCom cache.

    @param[in] size the new maximum size of the XCom cache
    @retval - GCS_OK if request was successfully scheduled in XCom,
              GCS_NOK otherwise.
  */
  enum_gcs_error set_xcom_cache_size(uint64_t size) override;

  /**
    Notify that the current member has left the group and whether it left
    gracefully or not.

    @param[in] error_code that identifies whether there was any error
               when the view was received.
  */
  void install_leave_view(Gcs_view::Gcs_view_error_code error_code);

 private:
  void init_me();

  /*
    Utility methods to build lists from the data that arrives with a view.
   */
  void build_total_members(
      Gcs_xcom_nodes *xcom_nodes,
      std::vector<Gcs_member_identifier *> &alive_members,
      std::vector<Gcs_member_identifier *> &failed_members);

  void build_left_members(
      std::vector<Gcs_member_identifier *> &left_members,
      std::vector<Gcs_member_identifier *> &alive_members,
      std::vector<Gcs_member_identifier *> &failed_members,
      const std::vector<Gcs_member_identifier> *current_members);

  void build_joined_members(
      std::vector<Gcs_member_identifier *> &joined_members,
      std::vector<Gcs_member_identifier *> &alive_members,
      const std::vector<Gcs_member_identifier> *current_members);

  void build_member_suspect_nodes(
      std::vector<Gcs_member_identifier *> &member_suspect_nodes,
      std::vector<Gcs_member_identifier *> &failed_members,
      const std::vector<Gcs_member_identifier> *current_members);

  void build_non_member_suspect_nodes(
      std::vector<Gcs_member_identifier *> &non_member_suspect_nodes,
      std::vector<Gcs_member_identifier *> &failed_members,
      const std::vector<Gcs_member_identifier> *current_members);

  /**
     Decides if this node shall be the one to kill failed nodes. The algorithm
     is: i am the highest node id alive.

     @param alive_members Set of alive members.

     @return true if i am the node responsible to call remove_node to expel
                  another member
   */
  bool is_killer_node(
      const std::vector<Gcs_member_identifier *> &alive_members) const;

  /**
    Copies from a set to a vector of Gcs_member_identifier.

    @param[in] origin original set
    @param[in] to_fill destination vector
  */

  void build_member_list(std::set<Gcs_member_identifier *> *origin,
                         std::vector<Gcs_member_identifier> *to_fill);

  /**
    Makes all the necessary arrangements to install a new view in the binding
    and in all registered client applications.

    @param[in] new_view_id new view identifier
    @param[in] group_id group id
    @param[in] states collection of states to set in the new view
    @param[in] total all the members
    @param[in] left members that left the last view
    @param[in] join members that joined from the last view
    @param[in] error_code Error code to set in the new view
  */
  void install_view(
      Gcs_xcom_view_identifier *new_view_id,
      const Gcs_group_identifier &group_id,
      std::map<Gcs_member_identifier, Xcom_member_state *> *states,
      std::set<Gcs_member_identifier *> *total,
      std::set<Gcs_member_identifier *> *left,
      std::set<Gcs_member_identifier *> *join,
      Gcs_view::Gcs_view_error_code error_code = Gcs_view::OK);

  /**
    Check whether the current member is in the received vector of members.

    @param[in] members list of members
  */
  bool is_this_node_in(std::vector<Gcs_member_identifier *> *members);

  /**
    Cycle through peers_list and try to open a connection to the peer, if it
    isn't the node itself.

    @param[in] peers_list list of the peers

    @return connection descriptor to a peer
  */
  connection_descriptor *get_connection_to_node(
      std::vector<Gcs_xcom_node_address *> *peers_list);

  /**
    Attempts to send an add_node request to some initial peer from @c
    m_initial_peers.
    Performs up to @c s_connection_attempts attempts.

    @param my_addresses The addresses of this node, used to filter our own
    address from the initial peers.
    @returns true if the add_node request was successfully sent, false
    otherwise.
  */
  bool send_add_node_request(std::map<std::string, int> const &my_addresses);

  /**
    Attempts to send an add_node request to some initial peer from @c
    m_initial_peers.

    @param my_addresses The addresses of this node, used to filter our own
    address from the initial peers.
    @returns true if the add_node request was successfully sent, false
    otherwise.
  */
  bool try_send_add_node_request_to_seeds(
      std::map<std::string, int> const &my_addresses);

  /**
    Connects to the given peer's XCom.

    @param peer Peer to connect to.
    @param my_addresses The addresses of this node, used to filter our own
    address from the initial peers.
    @retval {true, connection_descriptor*} If we connected successfully.
    @retval {false, _} If we could not connect.
  */
  std::pair<bool, connection_descriptor *> connect_to_peer(
      Gcs_xcom_node_address &peer,
      std::map<std::string, int> const &my_addresses);

  /**
   * Expel the given members from XCom.
   *
   * @param incompatible_members the members to expel
   */
  void expel_incompatible_members(
      std::vector<Gcs_xcom_node_information> const &incompatible_members);

  // The group that this interface pertains
  Gcs_group_identifier *m_gid;
  unsigned int m_gid_hash;

  // Reference to the proxy between xcom and this implementation
  Gcs_xcom_proxy *m_xcom_proxy;

  // Reference to the group management object.
  Gcs_xcom_group_management *m_xcom_group_management;

  // Map holding all the registered control event listeners
  std::map<int, const Gcs_control_event_listener &> event_listeners;

  // Information about the local membership of this node
  Gcs_xcom_node_information *m_local_node_info;

  // Reference to the local physical node information
  Gcs_xcom_node_address *m_local_node_address;

  // A reference of the State Exchange algorithm implementation
  Gcs_xcom_state_exchange_interface *m_state_exchange;

  // XCom main loop
  My_xp_thread_impl m_xcom_thread;

  // Socket utility.
  My_xp_socket_util *m_socket_util;

  /*
    Number of attempts to join a group before giving up and reporting
    an error.
  */
  unsigned int m_join_attempts;

  /*
    Number of time in seconds to wait between attempts to join a group.
  */
  unsigned int m_join_sleep_time;

  // Suspect nodes holding object
  Gcs_suspicions_manager *m_suspicions_manager;

  // Suspicions processing task
  My_xp_thread_impl m_suspicions_processing_thread;

  // Proxy to GCS Sock Probe
  Gcs_sock_probe_interface *m_sock_probe_interface;

  std::unique_ptr<Network_provider_operations_interface>
      m_comms_operation_interface;

 protected:
  /*
    Whether the XCOM was left running or not meaning that the join
    operation was successfully executed. Note, however, that this
    does not mean that any view was delivered yet.

    This flag is only updated by the MySQL GCS engine when the join
    and leave are processed.
  */
  bool m_xcom_running;

  /*
    Whether it was requested to make the node leave the group or not.
  */
  bool m_leave_view_requested;

  /*
    Whether a view saying that the node has voluntarily left the group
    was delivered or not.
  */
  bool m_leave_view_delivered;

  /* Whether this site boots the group or not. */
  bool m_boot;

  // Reference to the remote node to whom I shall connect
  std::vector<Gcs_xcom_node_address *> m_initial_peers;

  // Reference to the mechanism that ensures view safety
  Gcs_xcom_view_change_control_interface *m_view_control;

  // Reference to the MySQL GCS Engine
  Gcs_xcom_engine *m_gcs_engine;

 private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_control(const Gcs_xcom_control &);
  Gcs_xcom_control &operator=(const Gcs_xcom_control &);
};
#endif /* GCS_XCOM_CONTROL_INTERFACE_INCLUDED */
