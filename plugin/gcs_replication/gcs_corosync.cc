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
#include <gcs_protocol.h>
#include <gcs_corosync.h>
#include <corosync/corotypes.h>
#include <poll.h>
#include <string.h>
#include <gcs_protocol_factory.h>

using std::string;

namespace GCS
{

/*** Callbacks related ***/

/*
  The dispatcher thread exit is (may be) waited by initiator
  such as leave() to guarantee a clean GCS protocol shutdown.
  The pair of Mutex, cond-var faciliates the clean shutdown.
*/
static pthread_cond_t dispatcher_cond;
static pthread_mutex_t dispatcher_mutex;
static bool is_dispatcher_inited= false;

static void fill_member_set(const struct cpg_address *list,
                            size_t num, Member_set& ms)
{
  if (num > 0)
  {
    for (size_t i= 0; i < num; i++)
    {
      Member m((ulong) list[i].nodeid, (ulong) list[i].pid);
      ms.insert(m);
    }
  }
}

/*
  The function converts implementation specific types to non-specific ones
  to pass them to a generic view change handler.

  Todo: augement the function to receive is_quorate along with Membership
        rather than compute.
*/
void view_change(cpg_handle_t handle, const struct cpg_name *name,
                 const struct cpg_address *totl, size_t totl_entries,
                 const struct cpg_address *left, size_t left_entries,
                 const struct cpg_address *join, size_t join_entries)
{
  Member_set ms_totl, ms_left, ms_join;
  Protocol_corosync *p=
    static_cast<Protocol_corosync*>(Protocol_factory::get_instance());
  View& view= p->get_view();

  fill_member_set(totl, totl_entries, ms_totl);
  fill_member_set(left, left_entries, ms_left);
  fill_member_set(join, join_entries, ms_join);
  if (!view.has_group_name())
  {
    uint32 local_nodeid;
    /* the very first View change initialized the group name */
    string gname= string(name->value);
    view.set_group_name(gname);
    cpg_local_get(handle, &local_nodeid);
    view.set_local_node_id((ulonglong) local_nodeid);
  }
  /*
    Todo: replace the local quorate comptutation with one of the protocol.
    Equal split like {1,2} -> {1},{2} is handled pessimistically with
    closing session on either side.
  */
  bool quorate= ms_totl.size() == ms_join.size() ||
    (ms_totl.size() - ms_join.size()) > ms_left.size();
  view.update(ms_totl, quorate);

  // stats
  p->group_stats.update_per_view_change(view);

  p->handlers->view_change(view, ms_totl, ms_left, ms_join, quorate);
}

static void deliver(cpg_handle_t handle, const struct cpg_name *name,
                    uint32_t nodeid, uint32_t pid, void *data, size_t len)
{
  Message *msg= new Message(data, len);
  Protocol_corosync *p= static_cast<Protocol_corosync*>(Protocol_factory::get_instance());

  p->handlers->message_delivery(msg, p->get_view(string(name->value)));

  /* gcs statistic */
  p->group_stats.update_per_message_delivery((ulonglong) len);
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

  /* Prepare exit reporting mechanism to possible requestor */
  pthread_mutex_lock(&dispatcher_mutex);
  is_dispatcher_inited= true;
  pthread_mutex_unlock(&dispatcher_mutex);

  pfd.fd= fd;
  pfd.events= POLLIN;
  res= CS_OK;
  while (res == CS_OK)
  {
    poll (&pfd, 1, 1000);
    res= cpg_dispatch(handle, CS_DISPATCH_ALL);
  }
  /* signal to possible waiter */
  pthread_mutex_lock(&dispatcher_mutex);
  is_dispatcher_inited= false;
  pthread_cond_broadcast(&dispatcher_cond);
  pthread_mutex_unlock(&dispatcher_mutex);

  return NULL;
}

/*** regular methods ***/

bool Protocol_corosync::open_session(Event_handlers* handlers_arg)
{
  int res= CS_OK;
  // install the callbacks
  cpg_callbacks_t callbacks;
  // needed by dispatcher exit notification
  pthread_cond_init(&dispatcher_cond, NULL);
  pthread_mutex_init(&dispatcher_mutex, NULL);

  callbacks.cpg_confchg_fn= (cpg_confchg_fn_t) view_change;
  callbacks.cpg_deliver_fn= (cpg_deliver_fn_t) deliver;

  handlers= handlers_arg;

  res= cpg_initialize (&this->handle, &callbacks);
  pthread_create(&this->dispatcher_thd, NULL, run_dispatcher, &this->handle);
  pthread_detach(this->dispatcher_thd);

  return res != CS_OK;
}

static cpg_guarantee_t get_guarantee(const Message& msg)
{
  return CPG_TYPE_AGREED;
}

bool Protocol_corosync::broadcast(const Message& msg)
{
  struct iovec iov;
  iov.iov_base= const_cast<Message&>(msg).get_data();
  iov.iov_len= const_cast<Message&>(msg).get_length();
  int res= cpg_mcast_joined(handle, get_guarantee(msg), &iov, 1);

  // stats
  Protocol_corosync *p= static_cast<Protocol_corosync*>(Protocol_factory::get_instance());
  p->group_stats.update_per_message_sent((ulonglong) iov.iov_len);

  return res != CS_OK;
};

bool Protocol_corosync::join(const string& name_arg, enum_member_role role)
{
  cpg_name name;
  name.length= name_arg.length();
  strncpy(name.value, name_arg.c_str(), CPG_MAX_NAME_LENGTH);
  return cpg_join(handle, &name) != CS_OK;
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


/***  Testing compartment ***/

static void *test_me_thread_func(void *args)
{
  int i= 0;
  int msg_num= 1000;
  const char format_str[]= "test message %d";
  char buf[sizeof(format_str) + 4*sizeof(msg_num)];
  Message *ptr_msg;
  Protocol *p= Protocol_factory::get_instance();

  while (i < msg_num)
  {
    sprintf(buf, format_str, i+1); // enumeration starts from 1
    ptr_msg= new Message(MSG_REGULAR, MSGQOS_UNIFORM, MSGORD_TOTAL_ORDER,
                         buf, strlen(buf));
    p->broadcast(*ptr_msg);
    i++;
  }
  return NULL;
}


void Protocol_corosync::test_me()
{
  pthread_t test_thd;

  pthread_create(&test_thd, NULL, test_me_thread_func, NULL);
  pthread_detach(this->dispatcher_thd);

  return;
};

} // namespace

