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

/*
   Methods of Gcs_protocol_corosync class and necessary auxiliary functions
*/

#include <string>
#include <utility>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>      // getpid
#include <gcs_protocol.h>
#include <gcs_corosync.h>
#include <gcs_message.h>
#include <corosync/corotypes.h>
#include <poll.h>
#include <string.h>
#include <gcs_protocol_factory.h>

using std::string;
using std::max;

namespace GCS
{

/*
  The initial "impossible" value for the Totem ring id that a being
  booted up Member should have. The value can't be from the real ring
  so it identifies the joining member.
*/
static const Corosync_ring_id zero_ring_id= {0, 0};
/*
  The initial "impossible" value of the process id of the joining member.
  It will change to an actual value once the member's join message has been
  delivered to the member itself.
*/
static const Process_id zero_process_id= Process_id(0, 0);
bool operator == (const Corosync_ring_id& lhs, const Corosync_ring_id& rhs)
{
  return lhs.nodeid == rhs.nodeid && lhs.seq == rhs.seq;
}


/*** Callbacks related ***/

/*
  The dispatcher thread exit is (may be) waited by initiator
  such as leave() to guarantee a clean GCS protocol shutdown.
  The pair of Mutex, cond-var faciliates the clean shutdown.
*/
static pthread_cond_t dispatcher_cond;
static pthread_mutex_t dispatcher_mutex;
static bool is_dispatcher_inited= false;

/*
  The Totem protocol callback notifies on network changes.
  Notice that the Totem Ring change may or may not be associated with
  (so preceded by) a Closed Process Group view_change notification.
  Association with a CPG notification would indicate either
  the new totem ring node brings in or the old one carried away
  some CPG member(s).
  The "blank" pure ring view change is about no CPG members on
  new merged-in or departed rings.
  Whether the Totem ring change is blank or not is determined by
  Protocol_corosync awaited messages vector.
  When the vector is not empty this Member transits to State Exchange.
  The blank Totem ring change does not affect the current View status,
  should it be the Primary Component view.
*/
static void totem_ring_change(cpg_handle_t handle,
                              struct cpg_ring_id ring_id,
                              uint32_t member_list_entries,
                              const uint32_t *member_list)
{
  Protocol_corosync *proto=
    static_cast<Protocol_corosync*>(Protocol_factory::get_instance());

  /*
    Corosync delivers the Totem event even to CPG left/leaving member,
    which is to ignore.
  */
  if (proto->is_leaving)
    return;

  /* proto->local_process_id may 've been already changed from its initial zero */
  bool is_joiner= proto->last_seen_conf_id == zero_ring_id;

  proto->last_seen_conf_id.nodeid= ring_id.nodeid;
  proto->last_seen_conf_id.seq=    ring_id.seq;
  if (proto->pending_awaited_vector)
  {
    assert(proto->is_locked);
    /*
      is_joiner true means the reason of this Totem ring change that
      the Joiner status. State messages vector is not going to be
      reset in that case.
    */
    proto->update_awaited_vector(!is_joiner);
    proto->start_states_exchange();
  }

  proto->get_client_info().
    logger_func(GCS_INFORMATION_LEVEL,
                "Totem new ring notification is received: "
                "id '%lu:%llu'; number of nodes %lu",
                ring_id.nodeid, ring_id.seq, member_list_entries);
}

/*
  The function diagnoses CPG View change to find out whether it is bound
  to the Totem ring update.

  @return true   when CPG View change is due to Totem,
          false  otherwise.
*/
static bool is_totem_ring_changed(const struct cpg_address *left,
                                  size_t left_entries,
                                  const struct cpg_address *joined,
                                  size_t joined_entries)
{
  size_t i;

  for (i= 0; i < left_entries; i++)
  {
    if (left[i].reason == CPG_REASON_NODEDOWN)
      return true;
  }

  for (i= 0; i < joined_entries; i++)
  {
    if (joined[i].reason == CPG_REASON_NODEUP)
      return true;
  }

  return false;
}

static const char* corosync_vc_reasons[]=
{
  "joined",
  "left",
  "node is down",
  "node is up",
  "process is down",
  "unspecified",
};

/*
  The function converts Corosync's CPG memer status into
  short text description for logger.
*/
static const char* get_corosync_vc_reasons(uint reason)
{
  const char *ret;

  switch (reason) {
  case CPG_REASON_JOIN :         /* cpg_join() ws called */
    ret= corosync_vc_reasons[0];
    break;
  case CPG_REASON_LEAVE :        /* cpg_leave() ws called */
    ret= corosync_vc_reasons[1];
    break;

  /* Member is partitioned out; Totem ring change notification is expected */
  case CPG_REASON_NODEDOWN :
    ret= corosync_vc_reasons[2];
    break;
  /* Member is merging back; Totem ring change notification is expected */
  case CPG_REASON_NODEUP :
    ret= corosync_vc_reasons[3];
    break;
  case CPG_REASON_PROCDOWN :      /* cpg_finalize() ws called */
    ret= corosync_vc_reasons[4];
    break;

  default:
    ret= corosync_vc_reasons[5];  /* irrelevant details */
  };

  return ret;
}

static const uint log_buf_size= 1024;
static void log_describe_processes(const struct cpg_address *list,
                                   size_t list_entries,
                                   char* buf)
{
  if (list_entries == 0)
    sprintf(buf, "*empty*");

  int remained= log_buf_size, it_done;
  for (size_t i= 0; i < list_entries && remained > 0;
       i++, remained -= it_done, buf += it_done)
  {
    const char* fmt= i == 0 ? "'%lu,%lu' reason %s" :  ", '%lu,%lu' %s";
    it_done= snprintf(buf, remained, fmt,
                  (ulong) list[i].nodeid, (ulong) list[i].pid,
                  get_corosync_vc_reasons(list[i].reason));
  }
}

void Protocol_corosync::log_corosync_view_change(const struct cpg_address *total,
                                     size_t total_entries,
                                     const struct cpg_address *left,
                                     size_t left_entries,
                                     const struct cpg_address *joined,
                                     size_t joined_entries)
{
  char buf_total[log_buf_size]= {'0'};
  char buf_left[log_buf_size]= {'0'};
  char buf_joined[log_buf_size]= {'0'};

  log_describe_processes(total, total_entries, buf_total);
  log_describe_processes(left, left_entries, buf_left);
  log_describe_processes(joined, joined_entries, buf_joined);
  get_client_info().logger_func(GCS_INFORMATION_LEVEL,
                                "Corosync reports configuration change: "
                                "number of members %lu, Total set: %s; "
                                "Left set: %s; "
                                "Joined set %s.",
                                total_entries, buf_total, buf_left, buf_joined);
}

/*
  The function implements Corosync callback on Closed Process Group
  membership change, or just View-Change, VC in short.
  The function initiates State Exchange algorithm that is distributed and
  is later (deliver() callback) finallized with the current membership
  quorate computation and possible installation of the Primary
  Component.
  There're two types of CPG membership change: one deals
  and another does not with the Totem Ring update. In the former case
  State Exchange is deferred till Totem Ring change notification, see
  totem_ring_change callback.
  Within the latter there's also the local Member leave special branch.
  At any rate the view object gets reset in those attributes that are
  responsible for new view installation.
  New values of member sets are memorized into the Protocol_corosync instances
  to be required at the end of State Exchange.
*/
void view_change(cpg_handle_t handle, const struct cpg_name *name,
                 const struct cpg_address *total, size_t total_entries,
                 const struct cpg_address *left, size_t left_entries,
                 const struct cpg_address *joined, size_t joined_entries)
{
  Protocol_corosync *proto=
    static_cast<Protocol_corosync*>(Protocol_factory::get_instance());

  proto->log_corosync_view_change(total, total_entries,
                                  left, left_entries, joined, joined_entries);
  /*
    The node and the process id are not available until the Member has joint.
    The fact of joining becomes known at time of the first View-change delivery.
    Corresponding slots of Protocol_corosync and View classes are finally initialized
    at this point.
  */
  if (proto->get_local_process_id() == zero_process_id)
    proto->do_complete_local_member_init();

  /* CPG protocol must not deliver any VC to gracefully left/leaving member */
  assert(!proto->is_leaving);

  /*
    Protocol_corosync::vc_mutex is grabbed to be released when
    this Member will finally install the quorate view.
    This lock is to not let the Client to broadcast meanwhile and can
    be active in the course of multiple consequent invokation of this
    function.
  */
  if (!proto->is_locked)
  {
    pthread_mutex_lock(&proto->vc_mutex);
    proto->is_locked= true;
  }

  proto->reset_view_and_compute_leaving(total, total_entries,
                                        left, left_entries,
                                        joined, joined_entries);
  /*
    When ring id has changed no more messages from the current (old) view
    including State messages will arrive.
    In such case the awaited state messages vector is marked for
    reconstruction in the Token ring change handler.
    State Exchange has to be started in there as well 'cos the State message
    needs Totem rind id to identify itself as relevant to being built new
    configuration.
    State exchange is deferred in the case of Joiner that is unware of its
    totem ring id.
  */
  if (is_totem_ring_changed(left, left_entries, joined, joined_entries) ||
      proto->last_seen_conf_id == zero_ring_id)
  {
    proto->pending_awaited_vector= true;
  }
  else
  {
    proto->update_awaited_vector(false);
  }
  /*
    The leaving local member won't take part in State Exchange
    neither it will deliver any regular messages.

    todo: (optimization) consider explict LEAVE message to avoid State
    Message Exchange.
  */
  if (proto->is_leaving)
  {
    proto->pending_awaited_vector= false;
    proto->do_leave_local_member();
    return;
  }
  /*
    When the current VC is not bound to Totem ring change
    the State Exchange is started right here.
  */
  if (!proto->pending_awaited_vector)
    proto->start_states_exchange();
}

/*
  The function normally delivers a message to the protocol Message delivery
  callback.
  Its second purpose is to finalize new group members state exchange.
*/
static void deliver(cpg_handle_t handle, const struct cpg_name *name,
                    uint32_t nodeid, uint32_t pid, void *data, size_t len)
{
  Message msg((uchar*) data, len);
  Protocol_corosync *proto=
    static_cast<Protocol_corosync*>(Protocol_factory::get_instance());
  View& view= proto->get_view(string(name->value));

  assert(!proto->is_leaving); // the same as in view_change()

  if (msg.get_type() == MSG_GCS_INTERNAL)
  {
    proto->do_process_state_message(&msg, Process_id(nodeid, pid));
    return;
  }
  else
  {
    /* The "normal" branch. */
    assert(get_payload_code(&msg) == PAYLOAD_TRANSACTION_EVENT);

    if (view.is_prim_component() && view.get_view_id() != 0 /* not Joiner */)
      proto->do_message_delivery(&msg);
    else
    {
      /*
        Something is wrong if a Regular message slips into
        not-installed yet or non-quorate group.
      */
      assert(1);
    }
  }
}

/*
  The receiver thread that executes @c Protocol::event_handlers vector.
  The thread is gone when cpg_dispatch() senses connection to the group(s)
  is(are) terminated.
*/
static void *run_dispatcher(void *args)
{
  int fd;
  int res;
  cpg_handle_t handle= *(static_cast<cpg_handle_t *>(args));
  cpg_fd_get (handle, &fd);
  struct pollfd pfd;
  Protocol_corosync *proto=
    static_cast<Protocol_corosync*>(Protocol_factory::get_instance());

  /* Prepare exit reporting mechanism to possible requestor */
  pthread_mutex_lock(&dispatcher_mutex);
  is_dispatcher_inited= true;
  pthread_mutex_unlock(&dispatcher_mutex);

  pfd.fd= fd;
  pfd.events= POLLIN;
  res= CS_OK;
  while (res == CS_OK)
  {
    if (poll(&pfd, 1, 1000) < 0)
    {
      proto->get_client_info().logger_func(GCS_ERROR_LEVEL,
                                           "CPG dispatcher polls negative");
      goto exit;
    }
    res= cpg_dispatch(handle, CS_DISPATCH_ALL);
  }

exit:
  proto->end_of_dispatcher();

  /* signal to possible waiter */
  pthread_mutex_lock(&dispatcher_mutex);
  is_dispatcher_inited= false;
  pthread_cond_broadcast(&dispatcher_cond);
  pthread_mutex_unlock(&dispatcher_mutex);

  return NULL;
}

/*** regular methods ***/

Protocol_corosync::Protocol_corosync(Stats& collector) :
  Protocol(), handle(0), dispatcher_thd(0),
  local_process_id(zero_process_id), max_view_id(0),
  max_view_id_p_id(zero_process_id),
  group_stats(collector),
  is_locked(false), is_leaving(false),
  pending_awaited_vector(false)
{
  pthread_mutex_init(&vc_mutex, NULL);
  pthread_cond_init(&vc_cond, 0);

  collector.set_view(&get_view());
  last_seen_conf_id.nodeid= 0;
  last_seen_conf_id.seq= 0;
};

Protocol_corosync::~Protocol_corosync()
{
  member_states.clear();
  member_id_total.clear();
  pthread_mutex_destroy(&vc_mutex);
  pthread_cond_destroy(&vc_cond);
}

bool Protocol_corosync::open_session(Event_handlers* handlers_arg)
{
  int res= CS_OK;
  cpg_model_v1_data_t model_data;

  model_data.cpg_deliver_fn=         deliver;
  model_data.cpg_confchg_fn=         view_change;
  model_data.cpg_totem_confchg_fn=   totem_ring_change;
  model_data.flags=                  CPG_MODEL_V1_DELIVER_INITIAL_TOTEM_CONF;

  /*
    Reset possible leftovers from a previous session.
    It's safe until the receiver thread is up.
  */
  is_leaving= false;
  pending_awaited_vector= false;
  awaited_vector.clear();
  last_prim_comp_members.clear();
  last_view_id= 0;
  get_view().get_members().clear();

  // needed by dispatcher exit notification
  pthread_cond_init(&dispatcher_cond, NULL);
  pthread_mutex_init(&dispatcher_mutex, NULL);

  handlers= handlers_arg;

  res= cpg_model_initialize (&this->handle, CPG_MODEL_V1,
                             (cpg_model_data_t *) &model_data, NULL);

  pthread_create(&this->dispatcher_thd, NULL, run_dispatcher, &this->handle);
  pthread_detach(this->dispatcher_thd);

  return res != CS_OK;
}

static cpg_guarantee_t get_guarantee(const Message& msg)
{
  return CPG_TYPE_AGREED;
}

bool Protocol_corosync::broadcast(Message& msg)
{
  struct iovec iov;
  Message_header hdr;
  View& view= get_view();

  iov.iov_base= (void*) const_cast<Message&>(msg).get_data();
  iov.iov_len= const_cast<Message&>(msg).get_size();

  // Todo: fill in micro time and version
  hdr.version= 0;
  hdr.micro_time= 0;

  /*
    The binding's internal messages can flow in freely.
    todo: lock-free version of the user broadcast() concurrent with VC.
  */
  if (msg.get_type() != MSG_GCS_INTERNAL)
  {
    assert(get_payload_code(&msg) == PAYLOAD_TRANSACTION_EVENT);

    pthread_mutex_lock(&vc_mutex);

    /*
      The transaction rolls back when the old quorate view
      changes to a new non-quorate.

      Todo: implement official service to wait for some configurable time
      and bail out with an error when the Prim comp service
      is not back after the timeout elapses.
    */
    while (!view.is_installed())
    {
      pthread_cond_wait(&vc_cond, &vc_mutex);
    }

    assert(!is_locked);

    if (!view.is_quorate())
    {
      pthread_mutex_unlock(&vc_mutex);
      return true;
    }
  }
  else
  {
    // this branch can't be concurrent always be executed by receiver thread
    assert(is_locked);
  }

  hdr.local_cnt= group_stats.get_total_messages_sent();
  msg.store_header(hdr);
  int res= cpg_mcast_joined(handle, get_guarantee(msg), &iov, 1);

  if (msg.get_type() != MSG_GCS_INTERNAL)
  {
    pthread_mutex_unlock(&vc_mutex);
    group_stats.update_per_message_sent((ulonglong) iov.iov_len);
  }
  else
  {
    // this branch can't be concurrent always be executed by receiver thread
    assert(is_locked);
  }

  return res != CS_OK;
};

/*
  The method joins the local member to a group specified by the 1st argument.
  View object gets updated with the name of being joined group.
*/
bool Protocol_corosync::join(const string& name_arg, enum_member_role role)
{
  cpg_name name;
  bool rc;

  name.length= name_arg.length();
  strncpy(name.value, name_arg.c_str(), CPG_MAX_NAME_LENGTH);
  rc= cpg_join(handle, &name) != CS_OK;
  if (!rc)
    get_view().set_group_name(name_arg);

  return rc;
};

bool Protocol_corosync::leave(const string& group_name)
{
  struct cpg_name name;
  name.length= group_name.length();
  strncpy(name.value, group_name.c_str(), CPG_MAX_NAME_LENGTH);

  return cpg_leave(handle, &name) != CS_OK;
};

bool Protocol_corosync::close_session()
{
  bool res= (cpg_finalize(this->handle) != CS_OK);

  pthread_mutex_lock(&dispatcher_mutex);
  while (is_dispatcher_inited)  // todo: make the loop breakable (shutdown)
    pthread_cond_wait(&dispatcher_cond, &dispatcher_mutex);

  return res;
};

/**
  The function fills in the supplied Process_id_set identifiers
  of members of corosync's closed process group.

  @param list [in]    a pointer to array of structs holding Corosync process ids
  @param num  [in]    the number of the array elements
  @param pset [out]   the destination Process_id set.
  @param comp_id [in] process id to search in the set.

  @return true   when @c comp_id parameter is found,
          false  otherwise.
*/
static bool fill_member_set(const struct cpg_address *list,
                            size_t num, Process_id_set& pset,
                            Process_id comp_id= zero_process_id)
{
  assert(pset.size() == 0);
  bool rc= false;

  for (size_t i= 0; i < num; i++)
  {
    Process_id cpg_m_id((ulong) list[i].nodeid, (ulong) list[i].pid);

    pset.insert(cpg_m_id);
    if (comp_id != zero_process_id && !rc)
      rc= cpg_m_id == comp_id;
  }

  assert(pset.size() == num);

  return rc;
}

void Protocol_corosync::
reset_view_and_compute_leaving(const cpg_address *total, size_t total_num,
                               const cpg_address *left, size_t left_num,
                               const cpg_address *joined, size_t joined_num)
{
  ms_total.clear();
  ms_left.clear();
  ms_joined.clear();

  if (get_view().is_quorate())
  {
    /*
      Properties of the last time seen Primary Component are preserved
      to take part in install_view() computation.
      Notice, view::reset() is done *after* the last* slots are filled.
    */
    last_prim_comp_members= get_view().get_members();
    last_view_id= get_view().get_view_id();
  }
  member_states.clear();
  max_view_id= 0;
  max_view_id_p_id= zero_process_id;

  fill_member_set(total, total_num, ms_total);
  fill_member_set(joined, joined_num, ms_joined);

  is_leaving= fill_member_set(left, left_num, ms_left, local_process_id);
  get_view().reset();
}

/*
  Quorate compuation is spit into th following disjoint cases:
  - the departure case (empty new total set);
  the outcome: false
  - the initial empty primary component case;
  the outcome: true (see todo).
  - the regular case, the local member was used to be in a prim component.
  the outcome: true when the new membership constists of a majority
  of the maximum primary component, otherwise false.

  @param  mtr_set  new membership set
  @param  max_pc   maximum view membership
  @return true     when quorate computed positively,
          false    otherwise.
*/
bool Protocol_corosync::compute_quorate(Member_set& mbr_set, Member_set& max_pc)
{
  bool rc;
  Member_set common_mbr;
  mset_intersection(common_mbr, mbr_set, max_pc);
  if (mbr_set.size() == 0)
    rc= false;
  else if (max_pc.size() == 0)
  {
    /*
       Todo: we may need a policy to be specified via an user option
       to disallow such never been to Prim Comp to form it.
    */
    get_client_info().
      logger_func(GCS_INFORMATION_LEVEL,
                  "Member '%s (%lu,%lu)' is installing the cluster "
                  "for the first time",
                  get_client_uuid().c_str(),
                  local_process_id.first, local_process_id.second);
    rc= true;
  }
  else if (common_mbr.size() * 2 <= max_pc.size())
    rc= false;
  else
    rc= true;

  return rc;
}

bool Protocol_corosync::install_view()
{
  Member_set mbr_set;               /* to be installed view set */
  Member_set max_prim_comp_members; /* set from the max view id State message */
  Process_id_set::iterator it_total;
  bool quorate;

  assert(ms_total.size() == member_states.size());

  /*
    The former configuration protocol member id objects get deleted.
    Notice in between of this point and view.install below,
    View::members lose association with the protocol Member_id.
  */
  member_id_total.clear();
  for (it_total= ms_total.begin(); it_total != ms_total.end(); ++it_total)
  {
    Process_id p_id= *it_total;

    assert(member_states.find(p_id) != member_states.end());

    Corosync_member_id *ptr_m_id= new Corosync_member_id(p_id);
    member_id_total.push_back(ptr_m_id);
    Client_info& ci= member_states[p_id]->client_info;

    Member mbr(ci, (Protocol_member_id*) ptr_m_id);

    mbr_set.insert(mbr);
    if (p_id == max_view_id_p_id)
    {
      /* Form the max view-id primary component membership (set) */
      set<string>& uuids= member_states[p_id]->member_uuids;
      set<string>::iterator it_uuid;

      for (it_uuid= uuids.begin(); it_uuid != uuids.end(); it_uuid++)
      {
        string name= *it_uuid;
        Client_info info(name);
        max_prim_comp_members.insert(Member(info));
      }
    }
  }

  assert(max_view_id_p_id != zero_process_id || max_view_id == 0);

  quorate= compute_quorate(mbr_set, max_prim_comp_members);
  get_view().install(mbr_set, max_view_id, quorate);

  assert(ms_total.size() == get_view().get_members().size());
  assert(ms_total.size() == member_id_total.size());
  assert(ms_total.size() == mbr_set.size());

  /* State Exchange reset/cleanup */
  assert(pending_awaited_vector == false);
  awaited_vector.clear();

  return quorate;
}

void Protocol_corosync::start_states_exchange()
{
  Member_state mstate(last_view_id, last_prim_comp_members,
                      last_seen_conf_id, get_client_info());
  Message msg(&mstate, MSG_GCS_INTERNAL);

  broadcast(msg);
}

void Protocol_corosync::update_awaited_vector(bool reset_arg)
{
  Process_id_set::iterator it;
  Process_id p_id;

  pending_awaited_vector= false;
  if (reset_arg)
    awaited_vector.clear();
  for (it= ms_total.begin(), p_id= *it; it != ms_total.end(); ++it, p_id= *it)
  {
    awaited_vector[p_id]++;
  }
  for (it= ms_left.begin(), p_id= *it; it != ms_left.end(); ++it, p_id= *it)
  {
    awaited_vector.erase(p_id);
  }

  assert(awaited_vector.size() >= ms_total.size());
}

void Protocol_corosync::do_leave_local_member()
{
  View& view= get_view();

  /* Deliver empty set to the Client */
  ms_total.clear();
  /* Not state message exchange with anybody */
  max_view_id= 0;
  max_view_id_p_id= zero_process_id;
  install_view();

  assert(!view.is_quorate());

  group_stats.update_per_view_change();
  /*
    Deliver the final View-change event to Client. The client must
    deduce itself that its local instance is shut down from the fact
    that the being departed local Member id is in view.left.
  */
  handlers->view_change(view, view.get_members(), view.left, view.joined,
                        false);
  /* Release VC-mutex allowing senders */
  assert(is_locked);

  is_locked= false;
  pthread_mutex_unlock(&vc_mutex);
  pthread_cond_broadcast(&vc_cond);

  get_client_info().
    logger_func(GCS_INFORMATION_LEVEL,
                "Member '%s (%lu,%lu)' is leaving the cluster",
                get_client_uuid().c_str(),
                local_process_id.first, local_process_id.second);

  return;
}

void Protocol_corosync::do_complete_local_member_init()
{
  uint32 local_nodeid;

  /* local id:s should be initialized before join message is sent */
  cpg_local_get(handle, &local_nodeid);
  local_process_id= Process_id(local_nodeid, getpid());
}

void Protocol_corosync::do_process_state_message(Message *ptr_msg,
                                                 Process_id p_id)
{
  size_t data_len= get_data_len(ptr_msg);
  const uchar* data= get_payload_data(ptr_msg);
  Member_state *ms_info= new Member_state(data, data_len);
  View& view= get_view();

  assert(get_payload_code(ptr_msg) == PAYLOAD_STATE_EXCHANGE);
  assert(!view.is_installed());

  if (!(ms_info->conf_id == last_seen_conf_id))
  {
    get_client_info().
      logger_func(GCS_INFORMATION_LEVEL,
                  "Incompatible state message has arrived from member "
                  "'%s (%lu,%lu)'; to be ignored at forming new membership",
                  ms_info->client_info.get_uuid().c_str(),
                  p_id.first, p_id.second);
    return;
  }

  if (ms_info->member_uuids.size() != 0)
  {
    /* This member was a in Primary component */
    if (max_view_id < ms_info->view_id)
    {
      /* and its view_id is higher than found so far so it's memorized. */
      max_view_id= ms_info->view_id;
      max_view_id_p_id= p_id;
    }
    else if (max_view_id == ms_info->view_id)
    {
#ifndef DBUG_OFF
      /*
        When a state message claims to be from a same view-id member
        its primary component members and the ordering must be of the
        same set.
      */
      assert(max_view_id_p_id != zero_process_id);

      set<string>& curr_uuids= ms_info->member_uuids;
      set<string>& max_uuids= member_states[max_view_id_p_id]->member_uuids;
      set<string>::iterator it_curr, it_max;

      for (it_curr= curr_uuids.begin(), it_max= max_uuids.begin();
           it_curr != curr_uuids.end(); ++it_curr, ++it_max)
      {
        assert((*it_curr).compare(*it_max) == 0);
      }
#endif
    }
  }
  member_states[p_id]= ms_info;
  /*
    The rule of updating the awaited_vector at receiving is simply to
    decrement the counter in the right index. When the value drops to
    zero the index is discared from the vector.

    Installation goes into terminal phase when all expected state
    messagages has arrived which is indicated by the emtpy vector.
  */
  if (--awaited_vector[p_id] == 0)
  {
    awaited_vector.erase(p_id);
  }
  if (awaited_vector.size() == 0)
  {
    assert(member_states.size() == ms_total.size());

    if (install_view())
    {
      if (last_view_id + 1 < view.get_view_id())
      {
        get_client_info().logger_func(GCS_INFORMATION_LEVEL,
                    "Member '%s (%lu,%lu)' joins from %llu view, "
                    "distributed recovery must follow",
                    get_client_uuid().c_str(),
                    p_id.first, p_id.second, last_view_id);

        //TODO: Pedro's wl#6837 followup starts here...
      }
      group_stats.update_per_view_change();
      /*
        Deliver View-change event to Client.
        Notice the last argument value as false normally indicates
        this just installed group is not the cluster.
      */
      handlers->view_change(view, view.get_members(), view.left, view.joined,
                            view.is_quorate());
      is_locked= false;
      pthread_mutex_unlock(&vc_mutex);
      pthread_cond_broadcast(&vc_cond);
    }
    else
    {
      get_client_info().
        logger_func(GCS_INFORMATION_LEVEL,
                    "Member '%s (%lu,%lu)' as a part of '%d'-member "
                    "configuration could not form the cluster",
                    get_client_uuid().c_str(),
                    p_id.first, p_id.second, ms_total.size());
    }
  }
}

void Protocol_corosync::do_message_delivery(Message *ptr_msg)
{
  handlers->message_delivery(ptr_msg, get_view());
  /* gcs statistic for the server */
  group_stats.update_per_message_delivery((ulonglong) ptr_msg->get_size());
}

void Protocol_corosync::end_of_dispatcher()
{
  View &view= get_view();
  if (is_locked)
  {
    assert(!view.is_prim_component());

    is_locked= false;
    pthread_mutex_unlock(&vc_mutex);
    pthread_cond_broadcast(&vc_cond);
  }
  get_client_info().logger_func(GCS_INFORMATION_LEVEL,
                            "Member '%s (%lu,%lu)' is shutting down",
                            get_client_uuid().c_str(),
                            local_process_id.first, local_process_id.second);
}

} // namespace

