/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_corosync_interface.h"

pthread_cond_t dispatcher_cond;
pthread_mutex_t dispatcher_mutex;
bool is_dispatcher_inited;
bool terminate_dispatcher;

Gcs_interface*
Gcs_corosync_interface::interface_reference_singleton= NULL;

Gcs_interface*
Gcs_corosync_interface::get_interface()
{
  if(interface_reference_singleton == NULL)
  {
    interface_reference_singleton= new Gcs_corosync_interface();
  }

  return interface_reference_singleton;
}

void
Gcs_corosync_interface::cleanup()
{
  if(interface_reference_singleton != NULL)
  {
    delete interface_reference_singleton;
  }
}

Gcs_corosync_interface::Gcs_corosync_interface()
{
  //Initialize random seed
  srand(time(0));

  initialized= false;
}

Gcs_corosync_interface::~Gcs_corosync_interface()
{
  clean_group_interfaces();
}

bool
Gcs_corosync_interface::initialize()
{
  if(!initialized)
  {
    pthread_mutex_init(&dispatcher_mutex, NULL);
    pthread_cond_init(&dispatcher_cond, NULL);
    is_dispatcher_inited= false;
    terminate_dispatcher= false;

    initialized= !(initialize_corosync());
  }

  this->clean_group_interfaces();

  return !initialized;
}

bool
Gcs_corosync_interface::finalize()
{
  terminate_dispatcher= true;
  cpg_finalize(this->handle);

  pthread_mutex_lock(&dispatcher_mutex);
  while (is_dispatcher_inited)
  {
    pthread_cond_wait(&dispatcher_cond, &dispatcher_mutex);
  }
  pthread_mutex_unlock(&dispatcher_mutex);

  this->handle= 0;

  initialized= false;

  pthread_mutex_destroy(&dispatcher_mutex);
  pthread_cond_destroy(&dispatcher_cond);

  return initialized;
}

bool
Gcs_corosync_interface::is_initialized()
{
  return initialized;
}


Gcs_control_interface*
Gcs_corosync_interface::get_control_session(Gcs_group_identifier group_identifier)
{
  gcs_corosync_group_interfaces* group_if=
                                get_group_interfaces(group_identifier);

  return group_if->control_interface;
}

Gcs_communication_interface*
Gcs_corosync_interface::get_communication_session(Gcs_group_identifier group_identifier)
{
  gcs_corosync_group_interfaces* group_if=
                                        get_group_interfaces(group_identifier);

  return group_if->communication_interface;
}

Gcs_statistics_interface*
Gcs_corosync_interface::get_statistics(Gcs_group_identifier group_identifier)
{
  gcs_corosync_group_interfaces* group_if=
                                get_group_interfaces(group_identifier);

  return group_if->statistics_interface;
}

gcs_corosync_group_interfaces*
Gcs_corosync_interface::get_group_interfaces(Gcs_group_identifier group_identifier)
{
  //Try and retrieve already instantiated group interfaces for a certain group
  map<string, gcs_corosync_group_interfaces*>::iterator registered_group;
  registered_group= group_interfaces.find(group_identifier.get_group_id());

  gcs_corosync_group_interfaces* group_interface= NULL;
  if(registered_group == group_interfaces.end())
  {
    /*
     If the group interfaces do not exist, create and add them to
     the dictionary.
     */

    group_interface= new gcs_corosync_group_interfaces();
    group_interfaces[group_identifier.get_group_id()]= group_interface;

    Gcs_corosync_statistics *stats= new Gcs_corosync_statistics();

    group_interface->statistics_interface= stats;

    Gcs_corosync_view_change_control_interface *vce=
                              new Gcs_corosync_view_change_control();

    Gcs_corosync_communication_proxy* comm_proxy=
                                new Gcs_corosync_communication_proxy_impl();
    group_interface->communication_interface=
                 new Gcs_corosync_communication(handle,
                                                stats,
                                                comm_proxy,
                                                vce);
    Gcs_corosync_control_proxy* control_proxy=
                                new Gcs_corosync_control_proxy_impl();

    Gcs_corosync_state_exchange_interface *se
                            = new Gcs_corosync_state_exchange
                                     (group_interface->communication_interface);
    group_interface->control_interface
                      = new Gcs_corosync_control(handle,
                                                 group_identifier,
                                                 control_proxy,
                                                 se,
                                                 vce);

    //Store the created objects for later deletion
    group_interface->comm_proxy= comm_proxy;
    group_interface->vce= vce;
    group_interface->control_proxy= control_proxy;
    group_interface->se= se;
  }
  else
  {
    group_interface= registered_group->second;
  }

  return group_interface;
}

void
Gcs_corosync_interface::clean_group_interfaces()
{
  map<string, gcs_corosync_group_interfaces*>::iterator group_if;
  for(group_if= group_interfaces.begin();
      group_if != group_interfaces.end();
      group_if++)
  {
    delete (*group_if).second->comm_proxy;
    delete (*group_if).second->vce;
    delete (*group_if).second->control_proxy;
    delete (*group_if).second->se;

    delete (*group_if).second->communication_interface;
    delete (*group_if).second->control_interface;
    delete (*group_if).second->statistics_interface;

    delete (*group_if).second;
  }

  group_interfaces.clear();
}

bool
Gcs_corosync_interface::initialize_corosync()
{
  /*
   Corosync initialization is comprised of:
   - Corosync connection initialization
   - Registration of a thread to dispatch the callbacks
  */
  int res= CS_OK;

  cpg_callbacks_t callbacks;
  callbacks.cpg_confchg_fn= (cpg_confchg_fn_t) view_change;
  callbacks.cpg_deliver_fn= (cpg_deliver_fn_t) deliver;

  res= cpg_initialize (&handle, &callbacks);

  pthread_create(&this->dispatcher_thd, NULL, run_dispatcher, &this->handle);
  pthread_detach(this->dispatcher_thd);

  return res != CS_OK;
}


void
view_change(cpg_handle_t handle, const struct cpg_name *name,
            const struct cpg_address *total, size_t total_entries,
            const struct cpg_address *left, size_t left_entries,
            const struct cpg_address *joined, size_t joined_entries)
{
  /*
   The main algorithm in the view_change implementation is the following
   - According with the cpg_name received determine from which group the message
    is.
   - Retrieve the correspondent Control interface and delegate unto it the
    processing of the view change event
   */
  Gcs_interface* intf= Gcs_corosync_interface::get_interface();

  Gcs_group_identifier group_id(string(name->value));

  Gcs_control_interface*
             control_if= intf->get_control_session(group_id);

  //If the control interface is catastrophically unretrievable, do nothing
  if(control_if == NULL)
  {
    return;
  }

  Gcs_corosync_control* coro_control_if
                    = static_cast<Gcs_corosync_control*>(control_if);

  coro_control_if->view_changed(name,
                                total, total_entries,
                                left, left_entries,
                                joined, joined_entries);
}

void
deliver(cpg_handle_t handle, const struct cpg_name *name,
        uint32_t nodeid, uint32_t pid, void *data, size_t len)
{
  /*
   The main algorithm  of the deliver callback is the following:
   - Decode the received data into a Gcs_message object
   - Retrieve and Delegate to the Control Interface to check if it a Binding
     internal control message
   - Retrieve and Delegate to the Communication interface the message to be
     delivered
   */

  //Build a gcs_message from the arriving data...
  Gcs_member_identifier* origin=
                     Gcs_corosync_utils::build_corosync_member_id(nodeid, pid);
  Gcs_group_identifier destination(string(name->value));

  Gcs_message message(*origin,
                      destination,
                      (gcs_message_delivery_guarantee)0);

  delete origin;

  message.decode((uchar*)data, len);

  Gcs_interface* intf= Gcs_corosync_interface::get_interface();

  Gcs_control_interface*
             control_if= intf->get_control_session(destination);

  //If the control interface is catastrophically unretrievable, do nothing
  if(control_if == NULL)
  {
    return;
  }

  /*
   Test if this is a Control message, meaning that is a message sent internally
   by the binding implementation itself, such as State Exchange messages.

   If so, then break the execution here, since this message shall not be
   delivered to any registered listeners.
   */
  Gcs_corosync_control* coro_control_if
                    = static_cast<Gcs_corosync_control*>(control_if);
  if(coro_control_if->process_possible_control_message(&message))
  {
    return;
  }

  Gcs_communication_interface* comm_if=
                            intf->get_communication_session(destination);

  //If the communication interface is catastrophically unretrievable, do nothing
  if(comm_if == NULL)
  {
    return;
  }

  Gcs_corosync_communication_interface* coro_comm_if
                  = static_cast<Gcs_corosync_communication_interface*>(comm_if);

  coro_comm_if->deliver_message(name, nodeid, pid, data, len);
}

void
*run_dispatcher(void *args)
{
  int fd;
  int res;
  cpg_handle_t handle= *(static_cast<cpg_handle_t *>(args));
  cpg_fd_get (handle, &fd);
  struct pollfd pfd;

  pthread_mutex_lock(&dispatcher_mutex);
  is_dispatcher_inited= true;
  pthread_cond_broadcast(&dispatcher_cond);
  pthread_mutex_unlock(&dispatcher_mutex);

  pfd.fd= fd;
  pfd.events= POLLIN;
  res= CS_OK;
  while (!terminate_dispatcher && res == CS_OK)
  {
    if (poll(&pfd, 1, 1000) < 0)
    {
      break;
    }

    res= cpg_dispatch(handle, CS_DISPATCH_ALL);
  }

  pthread_mutex_lock(&dispatcher_mutex);
  is_dispatcher_inited= false;
  pthread_cond_broadcast(&dispatcher_cond);
  pthread_mutex_unlock(&dispatcher_mutex);

  return NULL;
}


