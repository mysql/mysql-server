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

#ifndef GCS_COROSYNC_H
#define GCS_COROSYNC_H

#include <set>
#include <map>
#include <vector>
#include <utility>
#include <stdio.h>
#include <corosync/cpg.h>
#include <corosync/corotypes.h>
#include "gcs_stats.h"
#include "gcs_member_info.h"

using std::map;
using std::make_pair;
using std::vector;

namespace GCS
{

/* Corosync specific Glosed Process Group identifier */
typedef pair<ulong,ulong> Process_id;
typedef set<Process_id> Process_id_set;

/* Corosync's version of Protocol_member_id */
class Corosync_member_id : public Protocol_member_id
{
private:
  Process_id p_id;
public:
  Corosync_member_id(Process_id arg) : p_id(arg) {};
  uchar* describe(uchar *buf, size_t len)
  {
    snprintf((char*) buf, len, "(%lu,%lu)", p_id.first, p_id.second);
    return buf;
  }
};

class Protocol_corosync : public Protocol
{
private:

  cpg_handle_t handle;
  pthread_t dispatcher_thd;
  Protocol_corosync(const Protocol_corosync&);
  /* Local CPG member identifier in the Corosync format */
  Process_id local_process_id;

public:

  /* View installation related: maximum view id out of State messages */
  ulonglong max_view_id;
  /* max view_id source process id */
  Process_id max_view_id_p_id;
  /*
    This slot holds a part of the content of Member_state.
    It member is updated to receive in it the current quorate view members
    at @c reset_view_and_compute_leaving().
    The set remains actual until a new Primary component is finally installed,
    or the local member leaves.
  */
  Member_set last_prim_comp_members; /* the last Primary Component membership */
  ulonglong last_view_id;        /* view_id corresponding the last membership */
  Stats& group_stats;
  pthread_mutex_t vc_mutex;
  pthread_cond_t vc_cond;
  /*
    A flag that is set by receiver thread when CPG view-change is registered.
    It stays up until a View is installed.
    The flag blocks ::broadcast() service unless its done internally
    by the binding.
  */
  bool is_locked;
  bool is_leaving;
  /* Ring id value that is last received via Totem ring change handler */
  Corosync_ring_id last_seen_conf_id;
  /*
    Collection of State Message contents to facilitate view installation.
  */
  map<Process_id, Member_state*> member_states;
  /*
    Set of id:s in GCS native format as reported by View-change handler.
  */
  Process_id_set ms_total, ms_left, ms_joined;
  /*
    The vector holds the current View::members "reflection" by Corosync protocol.
    It changes synchronously with View::members.
  */
  vector<Corosync_member_id*> member_id_total;
  /*
    Two following slot serves in State Messages Exchange,
    to terminate it in particular. The termination condition is
    emptiness of the vector.
    It's implemented as Process_id-keyed map.
    Its value part is updated per CPG view-change perhaps being
    deferred till Totem ring change event, and the whole map
    gets reset at View install().
  */
  map<Process_id, uint> awaited_vector;
  /*
    The flag is set up when Totem-ring-change bound view-change is
    detected. It affect time when @c awaited_vector is updated.
  */
  bool pending_awaited_vector;
  Protocol_corosync(Stats& collector);
  Protocol_type get_type() { return PROTO_COROSYNC; };
  ~Protocol_corosync();
  bool open_session(Event_handlers* handlers);
  bool broadcast(Message& msg);
  bool join(const std::string& group_name, enum_member_role role= MEMBER_ACTOR);
  bool leave(const string& group_name);

  bool close_session();
  /*
    The currently implemented Protocol session to joined groups
    association is 1 to 1. Hence, the argument -aware and -less
    versions are equivalent.
  */
  View& get_view() { return group_view; }
  View& get_view(const string& group_name) { return get_view(); }
  /*
    The method is a part of Primary Component computation to be
    reacted on the Corosync CPG view-change. It clears marks of former
    Primary Component, to memorize its latest membership and to update
    objects relevant to View installation to the values supplied by
    the view-change event.

    Protocol_corosync::is_leaving is computed basing on the received event
    content.

    @param total  new total set of members in cpg_address format
    @param ts    its size
    @param left  the left set of members in cpg_address format
    @param ls    its size
    @param joined  the joined set of members in cpg_address format
    @param js    its size
  */
  void reset_view_and_compute_leaving(const cpg_address* total, size_t ts,
                                      const cpg_address* left, size_t ls,
                                      const cpg_address* joined, size_t js);
  /*
    The method is called at CPG view-change handling to see off the local member.
    The leaving Member does not need to send or see any State message.
    It reports View-change with empty new membership and the full left set,
    itself including.
  */
  void do_leave_local_member();

  /*
    The method finalizes the local member initialization after its
    joining attempt is rewarded with CPG view-change.
  */
  void do_complete_local_member_init();

  /*
    Following a notification on CPG membership or Totem ring change
    the method initiates group members state exchange to build a new View.
  */
  void start_states_exchange();

  /*
    Following a notification on CPG membership or Totem ring change
    the method adjusts the awaited vector of State message.
    The rules below aim to tackle a "concurrent" View-change notification
    that could arrive in the middle of an ongoing exchange:

    \forall member of total set
       awaited_vector[member]++
    \forall member of left set
       awaited_vector[left] = 0

    The first rule reads that any member of the new being installed
    membership must broadcast a State message and the message will be
    expected everywhere.
    The second rules says that when a member is found in the left set
    its expectations in awaited_vector[left-member] are gone.
    It can't broadcast anything incl State message after it has left.
    If it had to sent anything it must 've done that and the message
    would 've been already delivered.
  */
  void update_awaited_vector(bool reset_arg);

  /*
    The method processes a State message and when it's found to be the
    last expected to finalize States exchange and install the new
    configuration with computed quorate.
  */
  void do_process_state_message(Message *ptr_msg, Process_id p_id);
  /*
    The method is invoked by CPG deliver callback when
    conditions to deliver regular messages apply. That means
    the view is installed and it must be quorate.
  */
  void do_message_delivery(Message *ptr_msg);

  /*
    The method computes quorate property of the new configuration,
    and installs a new view to return its quorate property.
    State exchange objects resetting is done.

    @return true when quorate property is true, false otherwise.
  */
  bool install_view();

  /*
    The method computes quorate property of a new configuration
    as specified by the first argument set.
    The 2nd argument carries maximum view membership as reported
    through State messages.
  */
  bool compute_quorate(Member_set& mtr_set_arg, Member_set& max_pc);
  /*
    Protocol finalization in case the receiver thread
    exits voluntarily (no close_session() was called).
  */
  void end_of_dispatcher();

  ulonglong get_max_view_id() { return max_view_id; }
  ulonglong set_max_view_id(ulonglong val) { return max_view_id= val; }
  Process_id get_local_process_id() { return local_process_id; }
  void set_local_process_id(Process_id val) { local_process_id= val; }
  cpg_handle_t get_handle() { return handle; }
  void log_corosync_view_change(const struct cpg_address *total,
                                size_t total_entries,
                                const struct cpg_address *left,
                                size_t left_entries,
                                const struct cpg_address *joined,
                                size_t joined_entries);
};

} // namespace

#endif
