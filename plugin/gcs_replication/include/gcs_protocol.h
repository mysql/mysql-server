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
#include <vector>
#include <algorithm>
#include <string.h>
#include "gcs_member_info.h"
#include "gcs_stdlib_and_types.h"

namespace GCS
{

using std::string;
using std::set;
using std::pair;

/**
   The Protocol_member_id class holds protocol specific identifier that
   the Member class instance may like have association with.
   Each interested protocol should define a derived class.
*/
class Protocol_member_id
{
private:
  Protocol_member_id(const Protocol_member_id&);
protected:
  Protocol_member_id() {};
public:
  virtual uchar* describe(uchar*, size_t)= 0;
};

class Member
{
public:

  Member(Client_info c_arg, Protocol_member_id *m_arg= NULL) :
    info(c_arg), m_id(m_arg) {}
  /* Member's info getters: */
  string& get_hostname()  { return info.get_hostname(); }
  uint get_port()         { return info.get_port(); }
  /*
    info.uuid is effectively (and by the def of uuid as well) the
    member identifier.
  */
  string& get_uuid()      { return info.get_uuid(); }
  uchar* describe_member_id(uchar *buf, size_t len)
  {
    return m_id->describe(buf, len);
  }
  Member_recovery_status get_recovery_status()
  {
    return info.get_recovery_status();
  }

private:
  /*
    Indentification of the member is up to the Protocol client
  */
  Client_info info;
  /*
    This slot is introduced to provide association with
    Protocol specific identifier.
  */
  Protocol_member_id *m_id;
};

inline bool operator < (const Member& lhs, const Member& rhs)
{
  const string a= static_cast<Member>(lhs).get_uuid();
  const string b= static_cast<Member>(rhs).get_uuid();
  return a.compare(b) < 0;
}

struct Member_cmp
{
  bool operator() (const Member& lhs, const Member& rhs) const
  {
    return lhs < rhs;
  }
};

typedef set<Member, Member_cmp> Member_set;

/**
   Member_set intersection function: c = a \cap b
   Here a,b and c are Member_set references.

   @return ret as a result of intersection of the 2nd and the 3rd parameters
*/
inline Member_set& mset_intersection(Member_set& c, Member_set& a, Member_set& b)
{
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                        std::inserter(c, c.begin()));
  return c;
}

/**
   Member_set relative complement function.
   c = a \ b, i.e c is relative complement of b in a.
   All a,b and c are Member_set.

   @return ret as a result of Set subtraction the 3rd from the 2nd parameter.
*/
inline Member_set& mset_diff(Member_set& c, Member_set& a, Member_set& b)
{
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                      std::inserter(c, c.begin()));
  return c;
}

class Group_members
{
public:
  //TODO: this class does not have a good reason to exist,
  //consider to dismantle it in WL#7331.
  //Methods invoked in WL#7331 shoul migrate either into View or Protocol.

  Group_members() {};
  ~Group_members() {};
  const string& get_group_name() { return group_name; }
  Member_set& get_members() { return members; }
  // Astha,Nuno-todo: wl7331 review
  Member get_member(ulong index)
  {
    Member_set::iterator it;
    for (ulong i=0; i < index; i++, it++)
    {};
    return *it;
  }

  /*
  class dummy_member_id : public Protocol_member_id
  {
  public:
  dummy_member_id() {}
  uchar* describe(uchar *buf, size_t len) { return buf; }
  };

  Member_set::iterator find_member(string& key_str)
  {
  Client_info key(key_str);
  dummy_member_id m_id;
  Member mbr(key, (Protocol_member_id*) &m_id);
  return members.find(mbr);
  };
  */
  const string& set_group_name(const string& arg)
  {
      return const_cast<string&>(group_name)= arg;
  }
  bool is_empty() { return members.size() == 0; }
  bool has_group_name() { return group_name.length() != 0; }

protected:

  const string group_name;
  /*
    fully indentified Members of the Group are interally (in set) sorted
    with uuid as the key.
  */
  Member_set members;
};

/**
   This class supersedes a bare Group_members with attributes that are
   meaningful only when concreate Protocol provides support for them.
   Instance of the class may be associated with @c Protocol session
   instance as many to one. Any view instance represents a certain
   group of processes/Members. One to one is that is currently implemented.
   The view instance is supposed to be updated at View-change event handling.
*/
class View : public Group_members
{
public:
  View() : m_installed(false), m_quorate(false), view_id(0) {};
  bool is_quorate() { return m_quorate; }
  bool is_installed() { return m_installed; }

  /*
    The method installs a new view. Whether that's the Primary Component
    depends on @c max_view_id_arg argument.
    The effective Primary Component installation increments the supplied
    max view-id to set up a new view id value.
    The ineffective means this members lost membership in the cluster.
    The whole group where such member belongs in now will have an "undefined"
    view_id value of zero.
    The left and joined set are updated.

    @param total            a copy of the new Member set
    @param max_view_id_arg  max heard view-id out of State messages
    @param quorate_arg      quorate value to be installed

    @return                 the value of quorate_arg
  */
  bool install(Member_set &total_arg, ulonglong max_view_id_arg,
               bool quorate_arg)
  {
    Member_set intr;

    /* Compute @c joined and left for reporting */
    mset_intersection(intr, members, total_arg);
    mset_diff(left, members, intr);
    mset_diff(joined, total_arg, intr);
    members.clear();
    members= total_arg;
    m_quorate= quorate_arg;

    assert(members.size() >= joined.size());
    assert(!m_quorate || view_id <= max_view_id_arg);

    view_id= m_quorate ? max_view_id_arg + m_quorate : 0;
    m_installed= true;

    return m_quorate;
  }
  ulonglong get_view_id() { return view_id; }

  /*
    Two sets to be reported to the Client at view-change event delivery.
    Notice that unlike the members (total) slot which is globally consistent
    the two represents *local* to a group member set differences.
    The left set is defined as entities from the last View::members
    that are not present in a being installed view.
    The join set is defined as entities that were not present in the last
    View::members.
  */
  Member_set left, joined;
  /**
    The method resets auxiliary objects relevant to the view
    installation time.
  */
  void reset()
  {
    left.clear();
    joined.clear();
    m_installed= false;
    m_quorate= false;
  }
  bool is_prim_component() { return m_installed && m_quorate; }

private:
  /*
    The flag designates State exchange is in progress.
  */
  bool m_installed;
  /*
    The flag designates a quorate view or the Primary Component view.
    It gets updated to a new computed value
    along with the members, left and joined at @c install().
  */
  bool m_quorate;
  /*
    Subsequent quorate view increments this counter.
  */
  ulonglong view_id;
};

typedef enum enum_protocol_type { PROTO_COROSYNC } Protocol_type;
typedef enum enum_member_role { MEMBER_ACTOR, MEMBER_OBSERVER } Member_role;

class Protocol;
class Message;
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
  /* Client info also contains the local Member uuid */
  Client_info local_client_info;

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
  virtual bool broadcast(Message& msg)= 0;

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
   * the Protocol session and a joined group.
   * The instance should be created not later than the first View change
   * event is handled. The instance's members attribute as well as others
   * must be updated at least by one of registered sessions' (*view_change) handler.
   *
   * @param group_name the name of the group which View is requested.
   * @return pointer to View object when the object exists, otherwise NULL.
   */
  virtual View& get_view(const string& group_name)= 0;

  /**
   * This member function stores the protocol client description
   * which identifies the local group member.
   */
  void set_client_info(Client_info&);

  /**
   * The method returns a reference to the Client instance.
   */
  Client_info& get_client_info() { return local_client_info; }

  /**
   * The method returns a reference to the Client instance's uuid.
   */
  string& get_client_uuid();
};

/* Protocol non-specific View-changes logger */
void log_view_change(ulonglong view_id, Member_set& total, Member_set& left,
                     Member_set& joined);

} // namespace

#endif
