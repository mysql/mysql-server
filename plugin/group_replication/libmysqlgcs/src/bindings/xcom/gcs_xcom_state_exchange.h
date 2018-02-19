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

#ifndef GCS_XCOM_STATE_EXCHANGE_INCLUDED
#define GCS_XCOM_STATE_EXCHANGE_INCLUDED

#include <stdio.h>
#include <sys/types.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_view_identifier.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#define WIRE_XCOM_VARIABLE_VIEW_ID_SIZE 8
#define WIRE_XCOM_VIEW_ID_SIZE 4
#define WIRE_XCOM_GROUP_ID_SIZE 4
#define WIRE_XCOM_MSG_ID_SIZE 8
#define WIRE_XCOM_NODE_ID_SIZE 4

/**
  @class Xcom_member_state

  Class that conveys the state to be exchanged between members, which is not
  provided by XCom.
*/
class Xcom_member_state {
 public:
  /**
    Xcom_member_state constructor.

    @param[in] view_id the view identifier from the node
    @param[in] configuration_id Configuration identifier in use when the state
                                exchange message was created
    @param[in] data the generic data to be exchanged
    @param[in] data_size data's size
  */

  explicit Xcom_member_state(const Gcs_xcom_view_identifier &view_id,
                             synode_no configuration_id, const uchar *data,
                             uint64_t data_size);

  /**
    Xcom_member_state constructor.

    @param[in] data the generic data to be exchanged
    @param[in] data_size data's size
  */

  explicit Xcom_member_state(const uchar *data, uint64_t data_size);

  /**
    Member state destructor.
  */

  ~Xcom_member_state();

  /**
    Encodes the Member State's header to be sent through the newtwork.

    @param[out] buffer where the header will be stored.
    @param[in,out] buffer_len pointer to the variable that will hold the
                   header's size and has the buffer's len as input.

    @return True if there is no space to store the header.
            Otherwise, false.
  */
  bool encode_header(uchar *buffer, uint64_t *buffer_len);

  /**
    Encodes Member State to be sent through the network.

    @param[out] buffer where the data will be stored.
    @param[out] buffer_len pointer to the variable that will hold the
                data's size.

    @return True if for any reason the data could not be exchanged.
            Otherwise, false.
  */

  bool encode(uchar *buffer, uint64_t *buffer_len);

  /**
    @return the size of the encoded data when put on the wire.
  */

  uint64_t get_encode_size() const;

  /**
    @return the size of the encoded payload when put on the wire.
  */

  uint64_t get_encode_payload_size() const;

  /**
    @return the size of the encoded header when put on the wire.
  */

  static uint64_t get_encode_header_size();

  /**
    @return the view identifier
  */

  Gcs_xcom_view_identifier *get_view_id() { return m_view_id; }

  /**
    @return the configuration identifier
  */

  synode_no get_configuration_id() const { return m_configuration_id; }

  /**
    @return the generic exchangeable data
  */

  const uchar *get_data() const { return m_data; }

  /**
    @return the size of the generic exchangeable data
  */

  uint64_t get_data_size() const { return m_data_size; }

 private:
  /*
    View identifier installed by the current member if there
    is any.
  */
  Gcs_xcom_view_identifier *m_view_id;

  /*
    Configuration identifier in use when the state exchange message
    was created.
  */
  synode_no m_configuration_id;

  /*
    Data to be disseminated by the state exchange phase.
  */
  uchar *m_data;

  /*
    Data's size disseminated by the state exchange phase.
  */
  uint64_t m_data_size;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Xcom_member_state(Xcom_member_state const &);
  Xcom_member_state &operator=(Xcom_member_state const &);
};

/**
  @interface gcs_xcom_state_exchange_interface

  Interface that defines the operations that state exchange will provide.
  In what follows, we describe how the state exchange algorithm works and
  the view change process where it is inserted and which is an essential
  part of our system.

  The view change process is comprised of two major parts:
    - Adding or removing a node from the system, accomplished in the
      XCom/Paxos layer: "The SMART Way to Migrate Replicated Stateful
      Services"

    - A state exchange phase in which all members distribute data among
     themselves.

  Whenever a node wants to add or remove itself from the group, or after
  a failure when a healthy member expels the faulty node from the group, a
  reconfiguration request is sent in the form of an add_node or remove_node
  message.

  After the success of the request, XCOM sends a global view message
  that contains information on all nodes tagging them as alive or
  faulty to all non-faulty members. MySQL GCS looks at this information
  and computes who has joined and who has left the group. The computation
  is trivially simple and compares the set of nodes received in the
  current view with the set of nodes in the previous view:

    . left nodes = (alive_members in old_set) - (alive_members in new_set)

    . joined nodes = (alive_members in new_set) - (alive_members in old_set)

  However, the new view is only delivered to an upper layer after all
  members exchange what we call a state message. While the view is being
  processed and the state exchange is ongoing, all incoming data messages
  are not delivered to the application and are put into a buffer. So after
  getting state messages from all members, the view change is delivered
  to the upper layer along with the content of the state messages and any
  buffered message is delivered afterwards.

  Why blocking the delivery of data messages and why these state
  messages?

  Recall that all messages are atomically delivered and we can guarantee
  that all nodes will have the same state which encompasses messages
  (e.g. transactions) in queues and in the storage (e.g. binary log)
  because all new data messages are buffered while the state messages
  are being exchanged.

  Blocking the delivery of new data messages give us a synchronization
  point.

  But if all nodes have the same state why gathering a state message
  from all members?

  The power of choice. Let us use MySQL Group Replication as a concrete
  example of an upper layer to understand why. This is done because having
  information on all members allow the new node to choose a member that is
  not lagging behind (i.e. has a small queue) as a donor in a recovery
  phase. Besides, the state message also carries information on IPs and
  Ports used to access the MySQL Instances. This information is necessary
  to start the recovery which will be asynchronously started and will dump
  the missing data from a donor.

  Note that the content of the state message is opaque to the MySQL GCS
  layer which only provides a synchronization point.
*/
class Gcs_xcom_state_exchange_interface {
 public:
  virtual ~Gcs_xcom_state_exchange_interface(){};

  /**
    Accomplishes all necessary initialization steps.
  */

  virtual void init() = 0;

  /**
    If messages were buffered during its processing, they are discarded
    and internal structures needed are cleaned up.
  */

  virtual void reset() = 0;

  /**
    Has the same behavior as the reset but additionaly flushes buffered
    messages.
  */

  virtual void reset_with_flush() = 0;

  /**
    If messages were buffered during its processing, they are delivered
    to upper layers and internal structures needed are cleaned up.
  */

  virtual void end() = 0;

  /**
    Signals the module to start a State Exchange.

    @param[in] configuration_id Configuration identifier in use when the state
                                exchange phase started
    @param[in] total          xcom total members in the new view
    @param[in] left           xcom members that left in the new view
    @param[in] joined         xcom members that joined in the new view
    @param[in] exchangeable_data generic exchanged data
    @param[in] current_view   the currently installed view
    @param[in] group          group name
    @param[in] local_info     the local GCS member identifier

    @return true if the member is leaving
  */

  virtual bool state_exchange(
      synode_no configuration_id, std::vector<Gcs_member_identifier *> &total,
      std::vector<Gcs_member_identifier *> &left,
      std::vector<Gcs_member_identifier *> &joined,
      std::vector<Gcs_message_data *> &exchangeable_data,
      Gcs_view *current_view, std::string *group,
      const Gcs_member_identifier &local_info) = 0;

  /**
    Processes a member state message on an ongoing State Exchange round.

    @param[in] ms_info received Member State
    @param[in] p_id the node that the Member State pertains

    @return true if State Exchanged is to be finished and the view can be
                 installed
  */

  virtual bool process_member_state(Xcom_member_state *ms_info,
                                    const Gcs_member_identifier &p_id) = 0;

  /**
    Retrieves the new view identifier after a State Exchange.

    @return the new view identifier
  */

  virtual Gcs_xcom_view_identifier *get_new_view_id() = 0;

  /**
    @return the members that joined in this State Exchange round
  */

  virtual std::set<Gcs_member_identifier *> *get_joined() = 0;

  /**
    @return the members that left in this State Exchange round
  */

  virtual std::set<Gcs_member_identifier *> *get_left() = 0;

  /**
    @return All the members in this State Exchange round
  */

  virtual std::set<Gcs_member_identifier *> *get_total() = 0;

  /**
    @return the group in which this State Exchange is occurring
  */

  virtual std::string *get_group() = 0;

  /**
    @return the saved states
  */

  virtual std::map<Gcs_member_identifier, Xcom_member_state *>
      *get_member_states() = 0;
};

/**
  Implementation of the gcs_xcom_state_exchange_interface.
*/
class Gcs_xcom_state_exchange : public Gcs_xcom_state_exchange_interface {
 public:
  /**
    State Exchange constructor.

    @param[in] comm Communication interface reference to allow broadcasting of
                    member states
  */

  explicit Gcs_xcom_state_exchange(Gcs_communication_interface *comm);

  virtual ~Gcs_xcom_state_exchange();

  // Implementation of gcs_xcom_state_exchange_interface
  void init();

  void reset();

  void reset_with_flush();

  void end();

  bool state_exchange(synode_no configuration_id,
                      std::vector<Gcs_member_identifier *> &total,
                      std::vector<Gcs_member_identifier *> &left,
                      std::vector<Gcs_member_identifier *> &joined,
                      std::vector<Gcs_message_data *> &exchangeable_data,
                      Gcs_view *current_view, std::string *group,
                      const Gcs_member_identifier &local_info);

  bool process_member_state(Xcom_member_state *ms_info,
                            const Gcs_member_identifier &p_id);

  Gcs_xcom_view_identifier *get_new_view_id();

  std::set<Gcs_member_identifier *> *get_joined() { return &m_ms_joined; }

  std::set<Gcs_member_identifier *> *get_left() { return &m_ms_left; }

  std::set<Gcs_member_identifier *> *get_total() { return &m_ms_total; }

  std::map<Gcs_member_identifier, Xcom_member_state *> *get_member_states() {
    return &m_member_states;
  }

  std::string *get_group() { return m_group_name; }

 private:
  /**
    Computes if the local member is leaving.

    @return true in case of the local member is leaving
  */

  bool is_leaving();

  /**
    Computes if the local member is joining.

    @return true in case of the local member is joining
  */

  bool is_joining();

  /**
    Broadcasts the local state to all nodes in the Cluster.

    @param[in] proposed_view proposed view to broadcast
    @param[in] exchangeable_data List with exchangeable messages
  */

  enum_gcs_error broadcast_state(
      const Gcs_xcom_view_identifier &proposed_view,
      std::vector<Gcs_message_data *> &exchangeable_data);

  /**
    Updates the structure that waits for State Exchanges.
  */

  void update_awaited_vector();

  /**
    Converts xcom data to a set of internal representation.

    @param[in] in xcom list
    @param[in] pset Set where the converted member ids will be written
  */

  void fill_member_set(std::vector<Gcs_member_identifier *> &in,
                       std::set<Gcs_member_identifier *> &pset);

  Gcs_communication_interface *m_broadcaster;

  std::map<Gcs_member_identifier, uint> m_awaited_vector;

  /* Set of ids in GCS native format as reported by View-change handler. */
  std::set<Gcs_member_identifier *> m_ms_total, m_ms_left, m_ms_joined;

  /* Collection of State Message contents to facilitate view installation. */
  std::map<Gcs_member_identifier, Xcom_member_state *> m_member_states;

  // Group name to exchange state
  std::string *m_group_name;

  // Local GCS member identification
  Gcs_member_identifier m_local_information;

  /* Configuration identifier in use when the state exchange phase started */
  synode_no m_configuration_id;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_state_exchange(Gcs_xcom_state_exchange const &);
  Gcs_xcom_state_exchange &operator=(Gcs_xcom_state_exchange const &);
};

/*
  @interface gcs_xcom_view_change_control_interface

  This interface will serve as a synchronization point to all those that are
  interested in maintaining view safety. This will guarantee that no actions are
  accomplished while a view change procedure is ongoing.

  The promoters of view change will indicate via start_view_exchange() and
  end_view_exchange() the boundaries of the process. Those that want to wait
  for the end, will synchronize on wait_for_view_change_end().
*/
class Gcs_xcom_view_change_control_interface {
 public:
  virtual ~Gcs_xcom_view_change_control_interface() {}

  virtual void start_view_exchange() = 0;
  virtual void end_view_exchange() = 0;
  virtual void wait_for_view_change_end() = 0;
  virtual bool is_view_changing() = 0;

  // Leave related information
  virtual bool start_leave() = 0;
  virtual void end_leave() = 0;
  virtual bool is_leaving() = 0;

  // Join related information
  virtual bool start_join() = 0;
  virtual void end_join() = 0;
  virtual bool is_joining() = 0;

  // Keep track of delivered views
  virtual void set_current_view(Gcs_view *current_view) = 0;
  virtual Gcs_view *get_current_view() = 0;
  virtual bool belongs_to_group() = 0;
  virtual void set_belongs_to_group(bool belong) = 0;
  virtual void set_unsafe_current_view(Gcs_view *current_view) = 0;
  virtual Gcs_view *get_unsafe_current_view() = 0;
};

/*
  @class gcs_xcom_view_change_control

  Implementation of gcs_xcom_view_change_control_interface.
*/
class Gcs_xcom_view_change_control
    : public Gcs_xcom_view_change_control_interface {
 public:
  explicit Gcs_xcom_view_change_control();
  virtual ~Gcs_xcom_view_change_control();

  void start_view_exchange();
  void end_view_exchange();
  void wait_for_view_change_end();
  bool is_view_changing();

  bool start_leave();
  void end_leave();
  bool is_leaving();

  bool start_join();
  void end_join();
  bool is_joining();

  void set_current_view(Gcs_view *current_view);
  Gcs_view *get_current_view();
  bool belongs_to_group();
  void set_belongs_to_group(bool belong);
  void set_unsafe_current_view(Gcs_view *current_view);
  Gcs_view *get_unsafe_current_view();

 private:
  bool m_view_changing;
  bool m_leaving;
  bool m_joining;

  My_xp_cond_impl m_wait_for_view_cond;
  My_xp_mutex_impl m_wait_for_view_mutex;
  My_xp_mutex_impl m_joining_leaving_mutex;

  /*
    Reference to the currently installed view.
  */
  Gcs_view *m_current_view;

  /*
    Protect access to the current view so that it can be
    copied and returned.
  */
  My_xp_mutex_impl m_current_view_mutex;

  /*
    Whether the current node belongs to a group or not.
  */
  bool m_belongs_to_group;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_view_change_control(Gcs_xcom_view_change_control const &);
  Gcs_xcom_view_change_control &operator=(Gcs_xcom_view_change_control const &);
};
#endif /* GCS_XCOM_STATE_EXCHANGE_INCLUDED */
