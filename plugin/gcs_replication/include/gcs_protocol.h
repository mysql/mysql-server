/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_PROTOCOL_H
#define GCS_PROTOCOL_H

#include <set>
#include <string>
#include <my_global.h>
#include <string.h>

namespace GCS
{

using std::string;
using std::set;
using std::pair;

/*
  Member (of a group) is defined as a process running on some node,
  so it has two attributes the node id and a private id of a process
  on that node.
*/
typedef pair<ulong,ulong> Member;
typedef set<Member> Member_set;

class Group_members
{
public:

  Group_members() {};
  ~Group_members() {};
  const string& get_group_name() { return group_name; }
  Member_set& get_members() { return members; }
  const string& set_group_name(string& arg)
    {
      return const_cast<string&>(group_name)= arg;
    }
  bool is_empty() { return members.size() == 0; }
  bool has_group_name() { return group_name.length() != 0; }

protected:

  const string group_name;      // unique or human-readable name of the group
  Member_set members;
  void reset_members(Member_set& arg)
    {
      members.clear();
      for (Member_set::iterator it= arg.begin(); it != arg.end(); ++it)
        members.insert(*it);
    };
};

/**
   This class supersedes a bare Group_members with attributes that are meaningful
   only when concreate Protocol provides support for them.
   Instance of the class is associated with @c Protocol session instance as many to
   one. Any view instance represents a certain  group of processes/Members.
   The view instance is supposed to be updated at View-change event handling.
*/
class View : public Group_members
{
public:
  View() : is_quorate(false), local_node_id(0) {};
  ~View() {};
  bool get_quorate() { return is_quorate; }
  void update(Member_set &ms, bool verdict)
    {
      reset_members(ms);
      is_quorate= verdict;
    }
  void set_local_node_id(ulonglong arg) { local_node_id= arg; };
  ulonglong get_local_node_id() { return local_node_id; };
private:

  bool is_quorate;         // flag to designate the quorate view
  ulonglong local_node_id; // Note, a member of the group is a pair!
};

typedef enum enum_protocol_type { PROTO_COROSYNC } Protocol_type;
typedef enum enum_message_type { MSG_REGULAR } Msg_type;
typedef enum enum_msg_qos { MSGQOS_BEST_EFFORT,
                            MSGQOS_RELIABLE, MSGQOS_UNIFORM } Msg_qos;
typedef enum enum_msg_ordering { MSGORD_UNORDERED, MSGORD_TOTAL_ORDER } Msg_ordering;
typedef enum enum_member_role { MEMBER_ACTOR, MEMBER_OBSERVER } Member_role;

// Todo: version handshake at Group joining
class Message {

public:

  /* No-data copy constructor e.g to broadcast */
  Message(Msg_type type_arg, Msg_qos qos_arg, Msg_ordering ord_arg,
          const void* data_arg, size_t len_arg) :
    len(len_arg), data((void*) data_arg)
    {
      type= type_arg; qos= qos_arg; ord= ord_arg;
      msg_id.view_no= 0;
      msg_id.seq_no= 0;
    }

  /*
     The delivered (dispatched) Message constructor.
     TODO: consider memory root of similar mechanism to hold private
     string data.
     Memory must be provided by the consumer.
  */
  Message(const void* data_arg, size_t len_arg) : len(len_arg)
    {
      str_data= string((const char*) data_arg, len);
      data= (void*) str_data.data();
    };

  size_t get_length() { return len; }
  void* get_data() { return data; }

private:

  size_t len;
  void *data;
  Msg_type type;
  Member sender_id;
  struct st_msg_global_id
  {
    ulonglong view_no; // the deliery view id
    ulonglong seq_no;  // sequence number in the view
  } msg_id;
  Msg_qos qos;
  Msg_ordering ord;

  string str_data;
};

class Protocol;

/**
   The GCS event handler vector is associated with a Protocol instance (Session).
   The handlers are meaningful after the session of Protocol is created and
   has joined a group.
*/
typedef struct st_event_handler
{
  /**
   *  View Change handler to be invoked when Configuration change event
   *  is registered by low-level GCS backbone.
   *  Component service to cope with network partitioning (split
   *  brain).
   *
   * @param view     reference to View object as context
   * @param total    reference to the new member set
   * @param left     reference to a set of nodes that left
   * @param joined   reference to a set of new arrived nodes
   * @param quorate  is false if the member is not part of a quorate partition
   *                 or true otherwise. The parameter is critical for providing
   *                 Primary Component Service.
   */
  void (*view_change)(View& view,
                      Member_set& total, Member_set& left, Member_set& joined,
                      bool quorate);

  /**
   * @param msg  The message to be delivered
   * @param view pointer to View object as context
   *
   * @note Memory pointered by the parameter is release by libary and calls back.
   */
  void (*message_delivery)(Message* msg, const View& ptr_v);
} Event_handlers;

/**
 * We use the group communication `protocol' as an alias of GCS.
 * This class aggregates interfaces belonging to few GCS categories.
 * Those include Data and Control interfaces as well as Session manangement.
 * From the pure interfaces perspective there's no limit on the number
 * of the class instances nor groups an instance is allowed to join.
 */
class Protocol
{
  /*
    The class is defined as abstract. Instantiation is left to a derived class.
  */
private:

  Protocol(const Protocol&);

protected:

  /**
     Only one instance of View is allowed currently.
     Lift this limitation at later time, if ever.
  */
  View group_view;

  Protocol() : handlers(NULL) {};
  virtual ~Protocol() {};

public:

  /**
     The session properties include customizable vector of handlers which
     gets determined through @c init_session.
  */
  Event_handlers *handlers;

  /**
     This member describes the protocol capabilities. A member
     presents it at group joining and may adopt an older value
     that is compatible with the rest of the group and adapt to it.
     The member plays significant role in upgrading and mixed version
     members in the cluster use cases.
  */
  ulonglong version;

  /**
   * The method conducts session initialization to assign a vector
   * of @c Event_handlers to @c this->handlers.
   *
   * @note this member function MUST be called prior to the
   * @c Protocol::join member function.
   *
   * @param handlers Vector of reactions of the protocol event.
   * @return false if the setting succeeded, true otherwise.
   */
  virtual bool open_session(Event_handlers* handlers_arg)= 0;

  /**
   * Undoes initialization performed by @c open_session.
   * @note This member function MUST be called after leaving all groups.
   *
   * @return true if the shutdown operation did not succeed,
   *              false otherwise.
   */
  virtual bool close_session()= 0;

  /**
     @return enum value of the selected protocol
  */
  virtual Protocol_type get_type()= 0;

  /**
   * This member function SHALL broadcast a message to the group.
   * The guarantees of the broadcast delivery SHALL be those as specified
   * on the @c Message::qos and @c Message::order fields.
   *
   * Note that this member function passes the Message instance by
   * reference, thence it retains ownership of the object. It also states
   * that it is constant, so no changes SHALL occur in the implementor of
   * this interface.
   *
   * @param Message the message to be broadcast.
   * @return false if the sending succeeded, true otherwise.
   */
  virtual bool broadcast(const Message& msg)= 0;

  /**
   * This member function SHALL join this member to the specified group.
   * If the member is already joined, then this function is ineffective
   * and SHALL return true.
   *
   * When a server joins a group, it can define its role. It can be an
   * observer or an active participant. If the underlaying group communication
   * toolkit does not support OBSERVER roles and join is issued with
   * OBSERVER as role, then this function shall result in an error,
   * and the join operation will fail.
   *
   * If the underlaying group communication system does not support
   * multiple groups, this function shall return an error and the join
   * operation is ineffective.
   *
   * @param group_name The name of the group to join.
   * @param role The type of role that this member will fulfill in the
   *             group. It is either ACTOR or OBSERVER.
   * @return false if the join operation was successful, true otherwise.
   */
  virtual bool join(const string& group_name,
                    enum_member_role role= MEMBER_ACTOR)= 0;

  /**
   * This member function SHALL remove this member from the currently
   * joined group. If no group is joined, then this function is
   * ineffective and SHALL return true.
   *
   * If the member had joined with role as OBSERVER, then the fact that
   * this member leaves the group SHALL not trigger a view change.
   *
   * @param group_name the name of the group to depart from.
   * @return false if the leave operation succeed, false otherwise.
   */
  virtual bool leave(const string& group_name)= 0;

  /**
   * This member function obtains a pointer to the current view of the group.
   *
   * Todo: this is a proposal. Accept|refine|remove.
   *
   * The returned instance of View is accossiated with
   * the Protocol session and a joined group. The view instance gets gone
   * when the group is left or the Protocol object is deleted.
   * The instance should be created not later than the first View change
   * event is handled. The instance's members attribute as well as others
   * must be updated at least by one of registered sessions' (*view_change) handler.
   *
   * @param group_name the name of the group which View is requested.
   * @return pointer to View object when the object exists, otherwise NULL.
   */
  virtual View& get_view(const string& group_name)= 0;

  /**
     The method is to help unit-testing.
  */
  virtual void test_me()= 0;

};

/**
   Some of Protocols may require limiting parameters for objects
   that their bindings temporarily construct.
*/
const uint max_members_in_corosync_group= 64;

} // namespace

#endif
