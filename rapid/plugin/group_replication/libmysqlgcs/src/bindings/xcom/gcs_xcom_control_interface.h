/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_XCOM_CONTROL_INTERFACE_INCLUDED
#define GCS_XCOM_CONTROL_INTERFACE_INCLUDED

#include <cstring>
#include <map>
#include <set>
#include <cstdlib>

#include "mysql/gcs/xplatform/my_xp_thread.h"
#include "mysql/gcs/xplatform/my_xp_util.h"
#include "mysql/gcs/gcs_control_interface.h"
#include "mysql/gcs/gcs_message.h"
#include "mysql/gcs/gcs_types.h"
#include "mysql/gcs/gcs_view.h"

#include "gcs_xcom_utils.h"
#include "gcs_xcom_state_exchange.h"
#include "gcs_xcom_group_member_information.h"
#include "gcs_xcom_interface.h"
#include "gcs_xcom_notification.h"

#include "simset.h"
#include "xcom_vp.h"
#include "xcom_common.h"
#include "node_list.h"
#include "node_set.h"
#include "task.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "site_def.h"
#include "xcom_transport.h"
#include "xcom_base.h"

typedef struct {
  std::vector<Gcs_member_identifier *> *nodes;
  Gcs_xcom_proxy *proxy;
  unsigned int group_id_hash;
} nodes_to_kill;


/**
  @class Gcs_node_suspicion

  This class stores the id of the suspect node, as well as the timestamp
  of the suspicion.
*/
class Gcs_node_suspicion
{
public:
  /**
    Constructor for Gcs_node_suspicion, which sets both the identifier
    of the suspect node and the timestamp of the suspicion creation
    according to received parameters.
    @param[in] id Member identifier of the suspect node
    @param[in] ts Timestamp of the creation of the suspicion
  */
  explicit Gcs_node_suspicion(Gcs_member_identifier id, uint64_t ts);


  ~Gcs_node_suspicion();


  /**
    Compares the object's timestamp with the received one, in order
    to check if the suspicion has timed out and the suspect node
    must be removed.
    @param[in] ts Provided timestamp
    @param[in] timeout Time interval for the suspicion to timeout
  */

  bool has_timed_out(uint64_t ts, uint64_t timeout);


  /**
    Compares m_suspect with the received Gcs_member_identifier object
    to determine if both refer to the same suspect node.
    @param[in] id Provided member identifier
  */

  bool equals(Gcs_member_identifier id);


  /**
    Retrieves the identifier of the suspect node.
  */

  Gcs_member_identifier get_member_id();

private:

  /**
    Stores the identifier of the suspect node.
  */
  Gcs_member_identifier m_suspect_id;

  /**
    Stores the timestamp of the creation of the suspicion.
  */
  uint64_t m_timestamp;
};


/**
  @class Gcs_suspicions_manager

  This class stores all node suspicions, as well as the timeout and period
  parameters used by the thread that processes the suspicions.
  Suspicions are added and removed upon reception of a new global view.
*/
class Gcs_suspicions_manager
{
public:
  /**
    Constructor for Gcs_suspicions_manager, which sets m_proxy with the
    received pointer parameter.
    @param[in] proxy Pointer to Gcs_xcom_proxy
  */

  explicit Gcs_suspicions_manager(Gcs_xcom_proxy *proxy);


  /**
    Destructor for Gcs_suspicions_manager.
  */

  ~Gcs_suspicions_manager();


  /**
    Invoked by Gcs_xcom_control::xcom_receive_global_view, it invokes the
    remove_suspicions method for the alive_nodes and expel_nodes parameters,
    if they're not empty, neither m_suspicions. It also invokes the
    add_suspicions method if the suspect_nodes parameter isn't empty.

    @param[in] alive_nodes List of the nodes that currently belong to the group
    @param[in] expel_nodes List of nodes to expel from the group
    @param[in] suspect_nodes List of nodes to suspect
  */

  void process_view(std::vector<Gcs_member_identifier *> alive_nodes,
                    std::vector<Gcs_member_identifier *> expel_nodes,
                    std::vector<Gcs_member_identifier *> suspect_nodes);


  /**
    Invoked periodically by the suspicions processing thread, it picks a
    timestamp and verifies which suspect nodes should be removed as they
    have timed out.
  */

  void process_suspicions();


  /**
    Retrieves current list of suspicions.
  */

  std::vector<Gcs_node_suspicion> get_suspicions();


  /**
    Retrieves suspicion thread period in seconds.
  */

  unsigned int get_period();


  /**
    Sets the period or sleep time, between iterations, for the suspicion
    thread.
    @param[in] sec Suspicion thread period
  */

  void set_period(unsigned int sec);


  /**
    Retrieves suspicion timeout in 100s of nanoseconds.
  */

  uint64_t get_timeout();


  /**
    Sets the time interval to wait before removing the suspect nodes
    from the cluster.
    @param[in] sec Suspicions timeout in seconds
  */

  void set_timeout_seconds(unsigned long sec);


  /**
    Sets the hash for the current group identifier.
    @param[in] gid_h Group ID hash
  */

  void set_groupid_hash(unsigned int gid_h);


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
    @param[in] suspect_nodes List of nodes to add to m_suspicions
  */

  void add_suspicions(std::vector<Gcs_member_identifier *> suspect_nodes);


  /**
    XCom proxy pointer
  */
  Gcs_xcom_proxy *m_proxy;

  /**
    Suspicions processing thread period in seconds
  */
  unsigned int m_period;

  /**
    Suspicion timeout stored in 100s of nanoseconds
  */
  uint64_t m_timeout;

  /*
   Group ID hash
  */
  unsigned int m_gid_hash;

  /*
    List of suspicions
  */
  std::vector<Gcs_node_suspicion> m_suspicions;

  // Mutex to control access to m_suspicions
  My_xp_mutex_impl m_suspicions_mutex;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_suspicions_manager(Gcs_suspicions_manager const&);
  Gcs_suspicions_manager& operator=(Gcs_suspicions_manager const&);
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
class Gcs_xcom_control: public Gcs_control_interface
{
public:
  /**
    Gcs_xcom_control_interface constructor.

    @param[in] group_member_information Information about this node in XCom
                                        format
    @param[in] xcom_peers Information about the nodes that it
                                       should get in touch to enter a group

    @param[in] group_identifier Group identifier object
    @param[in] xcom_proxy Proxy implementation reference
    @param[in] gcs_engine MySQL GCS engine
    @param[in] state_exchange Reference to the State Exchange algorithm implementation
    @param[in] view_control View change control interface reference
    @param[in] boot Whether the node will be used to bootstrap the group
    @param[in] socket_util Reference to a socket utility

  */

  explicit Gcs_xcom_control(
    Gcs_xcom_group_member_information *group_member_information,
    std::vector<Gcs_xcom_group_member_information *> &xcom_peers,
    Gcs_group_identifier group_identifier,
    Gcs_xcom_proxy *xcom_proxy,
    Gcs_xcom_engine *gcs_engine,
    Gcs_xcom_state_exchange_interface *state_exchange,
    Gcs_xcom_view_change_control_interface *view_control,
    bool boot,
    My_xp_socket_util *socket_util);

  virtual ~Gcs_xcom_control();

  // Gcs_control_interface implementation
  enum_gcs_error join();

  enum_gcs_error do_join(const bool retry=true);

  /*
    Responsible for doing the heavy lifting related to the join
    operation.
  */
  enum_gcs_error retry_do_join();

  enum_gcs_error leave();

  /*
    Responsible for doing the heavy lifting related to the leave
    operation.
  */
  enum_gcs_error do_leave();

  bool belongs_to_group();

  Gcs_view *get_current_view();

  const Gcs_member_identifier get_local_member_identifier() const;

  int add_event_listener(const Gcs_control_event_listener &event_listener);

  void remove_event_listener(int event_listener_handle);

  /**
    The purpose of this method is to be called when in Gcs_xcom_interface
    callback method of View Changing is invoked.

    This allows, in terms of software architecture, to concentrate all the
    view change logic and processing in a single place. The view_change
    callback that is registered in Gcs_xcom_interface should be a simple
    pass-through.

    @param[in] message_id the message that conveys the View Change
    @param[in] xcom_nodes Set of nodes that participated in the consensus
                          to deliver the message
    @param[in] same_view  Whether this global view was already delivered.
  */

  bool xcom_receive_global_view(synode_no message_id, Gcs_xcom_nodes *xcom_nodes,
                                bool same_view);

  /*
    This method is called in order to give a hint on what the node thinks
    about other nodes.

    @param[in] xcom_nodes Set of nodes that participated in the consensus
                          to deliver the message
  */
  bool xcom_receive_local_view(Gcs_xcom_nodes *xcom_nodes);


  /**
    Process a message from the control interface and if necessary delegate it
    to the state exchange.

    @param[in] msg message
  */

  void process_control_message(Gcs_message *msg);


  std::map<int, const Gcs_control_event_listener &> *get_event_listeners();


  Gcs_xcom_group_member_information *get_local_member_info();


  Gcs_xcom_proxy *get_xcom_proxy();


  Gcs_suspicions_manager *get_suspicions_manager();


  // For testing purposes
  void set_boot_node(bool boot);


  void set_local_node_info(Gcs_xcom_group_member_information *group_member_information);


  /**
    Inserts in m_initial_peers copies of the Gcs_xcom_group_member_information
    objects whose addresses are in the xcom_peers vector.

    @param[in] xcom_peers vector with peers' information
  */

  void set_peer_nodes(std::vector<Gcs_xcom_group_member_information *> &xcom_peers);


  /**
    Deletes all the Gcs_xcom_group_member_information objects pointed by the
    elements of the m_initial_peers vector, clearing it at the end.
  */

  void clear_peer_nodes();


  /**
    Return a pointer to a socket utility.

    @return a pointer to a socket utility
  */

  My_xp_socket_util* get_socket_util();


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

private:
  void init_me();

  /*
    Utility methods to build lists from the data that arrives with a view.
   */
  void build_total_members(Gcs_xcom_nodes *xcom_nodes,
                           std::vector<Gcs_member_identifier *> &alive_members,
                           std::vector<Gcs_member_identifier *> &failed_members);

  void
  build_left_members(std::vector<Gcs_member_identifier *> &left_members,
                     std::vector<Gcs_member_identifier *> &alive_members,
                     std::vector<Gcs_member_identifier *> &failed_members,
                     const std::vector<Gcs_member_identifier> *current_members);

  void
  build_joined_members(std::vector<Gcs_member_identifier *> &joined_members,
                       std::vector<Gcs_member_identifier *> &alive_members,
                       const std::vector<Gcs_member_identifier> *current_members);

  void
  build_expel_members(std::vector<Gcs_member_identifier *> &expel_members,
                      std::vector<Gcs_member_identifier *> &failed_members,
                      const std::vector<Gcs_member_identifier> *current_members);

  void
  build_suspect_members(std::vector<Gcs_member_identifier *> &suspect_members,
                        std::vector<Gcs_member_identifier *> &failed_members,
                        const std::vector<Gcs_member_identifier> *current_members);


  /**
     Decides if this node shall be the one to kill failed nodes. The algorithm
     is: i am the highest node id alive.

     @param alive_members Set of alive members.

     @return true if i am the node responsible to call remove_node to expel
                  another member
   */
  bool is_killer_node(std::vector<Gcs_member_identifier *> &alive_members);

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
  void install_view(Gcs_xcom_view_identifier *new_view_id,
                    const Gcs_group_identifier &group_id,
                    std::map<Gcs_member_identifier, Xcom_member_state *> *states,
                    std::set<Gcs_member_identifier *> *total,
                    std::set<Gcs_member_identifier *> *left,
                    std::set<Gcs_member_identifier *> *join,
		    Gcs_view::Gcs_view_error_code error_code=Gcs_view::OK);

  /**
    Check whether the current member is in the vector of failed members
    and in this case is considered faulty.

    @param[in] failed_members failed members
  */
  bool is_considered_faulty(
    std::vector<Gcs_member_identifier *> *failed_members);

  /**
    Notify that the current member has left the group and whether it left
    gracefully or not.

    @param[in] error_code that identifies whether there was any error
               when the view was received.
  */
  void install_leave_view(Gcs_view::Gcs_view_error_code error_code);

  // The group that this interface pertains
  Gcs_group_identifier *m_gid;
  unsigned int          m_gid_hash;

  // Reference to the proxy between xcom and this implementation
  Gcs_xcom_proxy *m_xcom_proxy;

  // Map holding all the registered control event listeners
  std::map<int, const Gcs_control_event_listener &> event_listeners;

  // Information about the local membership of this node
  Gcs_member_identifier *m_local_member_id;

  // A reference of the State Exchange algorithm implementation
  Gcs_xcom_state_exchange_interface *m_state_exchange;

  // Reference to the local physical node information
  Gcs_xcom_group_member_information *m_local_node_info;

  // XCom main loop
  My_xp_thread_impl m_xcom_thread;

  /*
     Structure that contains the identification of this node
     from XCOM's perspective
  */
  node_list m_node_list_me;

  My_xp_socket_util* m_socket_util;

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

protected:
  /*
    Whether the XCOM was left running or not meaning that the join
    operation was successfuly executed. Note, however, that this
    does not mean that any view was delivered yet.

    This flag is only updated by the MySQL GCS engine when the join
    and leave are processed.
  */
  bool m_xcom_running;

  /* Whether this site boots the group or not. */
  bool m_boot;

  // Reference to the remote node to whom I shall connect
  std::vector<Gcs_xcom_group_member_information *> m_initial_peers;

  // Reference to the mechanism that ensures view safety
  Gcs_xcom_view_change_control_interface *m_view_control;

  // Reference to the MySQL GCS Engine
  Gcs_xcom_engine *m_gcs_engine;

private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_control(const Gcs_xcom_control&);
  Gcs_xcom_control& operator=(const Gcs_xcom_control&);
};
#endif /* GCS_XCOM_CONTROL_INTERFACE_INCLUDED */
